"""MT2009_PLUS_DB_EDITOR_V1: "Łowienie ryb" - what a fishing rod brings up.

Where it lives: NOT in locale/poland/fishing.txt. This engine's fishing.cpp
(the fishing minigame) has no loader for fishing.txt at all - the file is a
leftover of the old engine (its "map grade" chances and fish lengths are
never read). A catch is decided by the quest library
  share/locale/poland/quest/libs/fishing/fishing_drop.lua
whose table fishing_drop_table holds six numbers per line:
  itemVnum ("farba" = a random hair dye 70202-70206), itemCount,
  reqRodLevel, maxRodLevel, minimumBonus, poolValue (a weight).
fishing.quest -> fishing_manage.predict_fish() draws one line among those
whose rod level range holds the rod's level ((vnum - 27400) / 10: Wędka+0 =
0 ... Wędka+9 = 9) and whose minimumBonus the angler's bonus reaches (rod
value0 + the bait's value0 + 10 with the premium Fish Mind), by weight.
Bots fish through the same CHARACTER::fishing() / quest path.

The quest libraries are read when the cores boot. The panel writes only the
lines (plain text) into the spool both containers mount:
    <spool>/fishing/fishing_drop.custom.txt
and the game's m2-fishing (bin/m2-fishing, run by m2-supervise before the
cores boot) rebuilds the live fishing_drop.lua from the image's copy with
those lines, after checking them (and every item against item_proto, every
rod level 0-9 having something to catch); a file that fails is not used and
<spool>/fishing/status says why. The image's file comes back as
fishing/fishing_drop.base.lua; until a game with m2-fishing has run, the
panel shows its snapshot (dbeditor/fishing_drop.snapshot.lua).

Backups, "pending" and the game's report use dropfiles.py's machinery: the
file is registered there as key "fishing", so /db/apply lists it.
"""
import re
from pathlib import Path

from flask import flash, g, redirect, render_template, request, url_for

from dbeditor import dropfiles as df
from dbeditor import tables_common

KEY = "fishing"
FOLDER = "fishing"
SNAPSHOT = Path(__file__).resolve().parent / "fishing_drop.snapshot.lua"   # the image's file, 4 Oct 2026
ROD_BASE = 27400
ROD_LEVELS = range(0, 10)        # Wędka+0 ... Wędka+9 (+9 does not refine further)
ROD_MAX = 19                     # the item table also has 27500-27590 (+10..+19)
DYE = "farba"
DYE_VNUMS = (70202, 70203, 70204, 70205, 70206)
# rod level -> its own bonus (item_proto value0), when the database is not around
ROD_BONUS_DEFAULT = {0: 0, 1: 10, 2: 20, 3: 30, 4: 40, 5: 50, 6: 60, 7: 70, 8: 80, 9: 100}
BAITS = ((0, "bez przynęty / Papka", 0), (27801, "Robak", 10), (27802, "Drobne Ryby", 20))
PREMIUM_BONUS = 10
LIMITS = {"count": (1, 200), "rod_min": (0, ROD_MAX), "rod_max": (0, ROD_MAX), "bonus": (0, 1000),
          "weight": (1, 1000000)}
LABELS = {"item": "Przedmiot", "count": "Ilość", "rod_min": "Od wędki +", "rod_max": "Do wędki +",
          "bonus": "Minimalny bonus", "weight": "Waga"}
TABLE_START = re.compile(r"^\s*local\s+fishing_drop_table\s*=\s*\{")


def register_file():
    """The fishing file in dropfiles' table: custom path, status, backups and
    the "pending" list of /db/apply come with it."""
    df.FILES.setdefault(KEY, (FOLDER, "fishing_drop.custom.txt", "status", "Łowienie ryb (fishing_drop.lua)"))
    df.BASES.setdefault(KEY, (FOLDER, "fishing_drop.base.lua"))


# ----------------------------------------------------------------- parsing ---
def parse_lua(text):
    """fishing_drop.lua -> [entry] of its fishing_drop_table (lines commented
    out with -- are not in the table)."""
    entries, inside = [], False
    for raw in (text or "").replace("\r", "").split("\n"):
        if not inside:
            if TABLE_START.match(raw):
                inside = True
            continue
        line = raw.split("--", 1)[0].strip()
        if line.startswith("}"):
            break
        if not line:
            continue
        fields = [f.strip() for f in line.split(",") if f.strip()]
        if len(fields) != 6:
            continue
        item = fields[0].strip('"').strip("'")
        try:
            numbers = [int(f) for f in fields[1:]]
        except ValueError:
            continue
        comment = raw.split("--", 1)[1].strip() if "--" in raw else ""
        entries.append(entry(item, *numbers, comment=comment))
    return entries


def entry(item, count, rod_min, rod_max, bonus, weight, comment=""):
    return {"item": str(item).lower(), "count": int(count), "rod_min": int(rod_min), "rod_max": int(rod_max),
            "bonus": int(bonus), "weight": int(weight), "comment": comment}


def parse_custom(text):
    """The panel's file: one line per catch, tab separated, "#" comments."""
    entries = []
    for raw in (text or "").replace("\r", "").split("\n"):
        line, _sep, comment = raw.partition("#")
        fields = line.split()
        if len(fields) != 6:
            continue
        try:
            entries.append(entry(fields[0], *[int(f) for f in fields[1:]], comment=comment.strip()))
        except ValueError:
            continue
    return entries


def render_custom(entries, reason=""):
    from datetime import datetime
    lines = ["# MT2009_PLUS_DB_EDITOR_V1 - tabela lowienia ryb zapisana w panelu Seban",
             f"# zapis: {datetime.now():%Y-%m-%d %H:%M:%S} - {df.ascii_comment(reason)}",
             "# m2-fishing wstawia te linie do quest/libs/fishing/fishing_drop.lua (fishing_drop_table).",
             "# przedmiot\tilosc\twedka_od\twedka_do\tmin_bonus\twaga", ""]
    for e in entries:
        line = "\t".join(str(e[k]) for k in ("item", "count", "rod_min", "rod_max", "bonus", "weight"))
        comment = df.ascii_comment(e.get("comment", ""))
        lines.append(line + (f"\t# {comment}" if comment else ""))
    return ("\r\n".join(lines) + "\r\n").encode("ascii")


# ------------------------------------------------------------------- odds ---
def rod_level(vnum):
    return (int(vnum) - ROD_BASE) // 10


def chances(entries, level, bonus):
    """[(entry index, %)] of one rod level and bonus - the quest's own draw."""
    allowed = [(i, e["weight"]) for i, e in enumerate(entries)
               if e["rod_min"] <= level <= e["rod_max"] and bonus >= e["bonus"]]
    total = sum(w for _i, w in allowed)
    return {i: (100.0 * w / total if total else 0.0) for i, w in allowed}, total


def validate(entries, known_items, rod_bonus):
    """Problems the game would hit, in Polish ([] = fine). known_items: the
    VNUMs item_proto has (None = not checked)."""
    problems = []
    if not entries:
        problems.append("Tabela nie może być pusta – wędka nie miałaby czego złowić.")
    for n, e in enumerate(entries, 1):
        if e["item"] != DYE and not e["item"].isdigit():
            problems.append(f"Wiersz {n}: „{e['item']}” to nie numer przedmiotu (ani „{DYE}”).")
        elif e["item"].isdigit() and known_items is not None and int(e["item"]) not in known_items:
            problems.append(f"Wiersz {n}: przedmiotu {e['item']} nie ma w grze.")
        for key, (low, high) in LIMITS.items():
            if not low <= e[key] <= high:
                problems.append(f"Wiersz {n}: {LABELS[key]} – dozwolone {low}–{high}.")
        if e["rod_min"] > e["rod_max"]:
            problems.append(f"Wiersz {n}: „od wędki” jest większe niż „do wędki”.")
    if not problems:
        for level in ROD_LEVELS:
            if not chances(entries, level, rod_bonus.get(level, 0))[1]:
                problems.append(f"Wędka +{level} (bonus {rod_bonus.get(level, 0)}) bez przynęty nie złowiłaby nic – "
                                "gra pokazałaby błąd. Dodaj coś dla tej wędki z minimalnym bonusem "
                                f"≤ {rod_bonus.get(level, 0)}.")
    return problems


def form_entries(form):
    """The rows of the edit form -> (entries, errors)."""
    entries, errors = [], []
    try:
        count = min(500, int(form.get("row_count", "0")))
    except ValueError:
        count = 0
    for i in range(count):
        p = f"r{i}_"
        item = (form.get(p + "item") or "").strip().lower()
        if form.get(p + "delete") or not item:
            continue
        values = {}
        for key in ("count", "rod_min", "rod_max", "bonus", "weight"):
            raw = (form.get(p + key) or "").strip()
            if not re.fullmatch(r"\d{1,10}", raw):
                errors.append(f"Wiersz {i + 1}: {LABELS[key]} – „{raw}” to nie liczba całkowita.")
                values[key] = 0
            else:
                values[key] = int(raw)
        entries.append(entry(item, comment=form.get(p + "comment", ""), **values))
    return entries, errors


# ------------------------------------------------------------------ pages ---
def install(bp, ctx):
    import dbeditor
    register_file()
    df.register(bp)
    tables_common.register(bp)
    rows, game_text, login_required = ctx["rows"], ctx["game_text"], ctx["login_required"]

    def base_entries(spool):
        text = df.read_text(df.base_path(spool, KEY))
        source = "game"
        if text is None:
            text, source = df.read_text(SNAPSHOT), "snapshot"
        return parse_lua(text or ""), source

    def current(spool):
        text = df.read_text(df.custom_path(spool, KEY))
        if text is not None and parse_custom(text):
            return parse_custom(text), "custom"
        entries, source = base_entries(spool)
        return entries, source

    def rod_info():
        """{level: {"vnum", "name", "bonus", "chance"}} from item_proto."""
        if "dbt_rods" in g:
            return g.dbt_rods
        info = {lv: {"vnum": ROD_BASE + 10 * lv, "name": f"Wędka+{lv}", "bonus": b, "chance": 20}
                for lv, b in ROD_BONUS_DEFAULT.items()}
        try:
            for r in rows("SELECT vnum, locale_name, value0, value5 FROM player.item_proto WHERE type=13 "
                          "AND vnum BETWEEN %s AND %s", (ROD_BASE, ROD_BASE + 90)):
                lv = rod_level(r["vnum"])
                if lv in info and int(r["vnum"]) % 10 == 0:
                    info[lv].update(name=game_text(r["locale_name"]) or info[lv]["name"],
                                    bonus=int(r.get("value0") or 0), chance=20 + int(r.get("value5") or 0))
        except Exception:
            pass
        g.dbt_rods = info
        return info

    def decorate(entries):
        vnums = set()
        for e in entries:
            if e["item"].isdigit():
                vnums.add(int(e["item"]))
        vnums.update(DYE_VNUMS[:1])
        try:
            names = df.item_names(rows, game_text, sorted(vnums))
        except Exception:
            names = {}
        out = []
        for e in entries:
            if e["item"] == DYE:
                label, icon, bad = "Farba do włosów (losowy kolor)", df.icon_url(DYE_VNUMS[0]), False
            elif e["item"].isdigit():
                known = names.get(int(e["item"]))
                label = known["name"] if known else "NIEZNANY PRZEDMIOT"
                icon, bad = df.icon_url(e["item"]), known is None
            else:
                label, icon, bad = "?", None, True
            out.append(dict(e, label=label, icon=icon, bad=bad))
        return out, names

    @bp.route("/fishing")
    @login_required
    def fishing():
        spool = df.spool_dir()
        entries, source = current(spool)
        rods = rod_info()
        try:
            extra = int(request.args.get("przyneta", "10"))
        except ValueError:
            extra = 10
        premium = request.args.get("premium") == "1"
        bonus_extra = extra + (PREMIUM_BONUS if premium else 0)
        lines, _names = decorate(entries)
        matrix, totals = {}, {}
        for lv in ROD_LEVELS:
            matrix[lv], totals[lv] = chances(entries, lv, rods[lv]["bonus"] + bonus_extra)
        problems = validate(entries, None, {lv: rods[lv]["bonus"] for lv in ROD_LEVELS})
        backups = df.list_backups(spool, [KEY], 20)
        return render_template("dbeditor/fishing.html", lines=lines, source=source, rods=rods, levels=list(ROD_LEVELS),
                               matrix=matrix, totals=totals, baits=BAITS, extra=extra, premium=premium,
                               problems=problems, status=df.live_state(spool, KEY), backups=backups,
                               pending=[1] if df.live_state(spool, KEY)["pending"] else [], rod_max=ROD_MAX,
                               dye=DYE, csrf=df.csrf_token())

    @bp.post("/fishing")
    @login_required
    def fishing_save():
        spool = df.spool_dir()
        if not df.csrf_ok(request.form):
            flash("Sesja formularza wygasła – odśwież stronę i spróbuj jeszcze raz.", "error")
            return redirect(url_for("dbeditor.fishing"))
        action = request.form.get("action", "save")
        try:
            if action == "reset":
                df.write_custom(spool, KEY, None, "przywrocono tabele z obrazu gry")
                flash("Łowienie wróci do tabeli z obrazu gry po restarcie (Zastosuj).", "success")
                return redirect(url_for("dbeditor.fishing"))
            if action == "restore":
                df.restore_backup(spool, KEY, request.form.get("name", ""))
                flash("Przywrócono kopię. Działa po restarcie (Zastosuj).", "success")
                return redirect(url_for("dbeditor.fishing"))
        except (OSError, ValueError) as exc:
            flash(f"Nie udało się: {exc}", "error")
            return redirect(url_for("dbeditor.fishing"))
        entries, errors = form_entries(request.form)
        rods = rod_info()
        vnums = sorted({int(e["item"]) for e in entries if e["item"].isdigit()})
        try:
            known = set(df.item_names(rows, game_text, vnums)) if vnums else set()
        except Exception:
            known = None
            flash("Nie udało się sprawdzić przedmiotów w bazie – sprawdzi je gra przy starcie.", "warning")
        problems = errors or validate(entries, known, {lv: rods[lv]["bonus"] for lv in ROD_LEVELS})
        if problems:
            for problem in problems[:12]:
                flash(problem, "error")
            flash("Nic nie zapisano – popraw tabelę.", "error")
            return redirect(url_for("dbeditor.fishing"))
        base, _source = base_entries(spool)
        key = lambda es: [(e["item"], e["count"], e["rod_min"], e["rod_max"], e["bonus"], e["weight"]) for e in es]
        try:
            if key(entries) == key(base):
                df.write_custom(spool, KEY, None, "tabela taka jak w obrazie gry")
            else:
                df.write_custom(spool, KEY, render_custom(entries, request.form.get("note", "") or "zmiana w panelu"),
                                request.form.get("note", "") or "zmiana tabeli lowienia")
        except OSError as exc:
            flash(f"Nie udało się zapisać pliku w wolumenie spool: {exc}", "error")
            return redirect(url_for("dbeditor.fishing"))
        flash(f"Zapisano tabelę łowienia ({len(entries)} pozycji). Gra wczyta ją po restarcie (Zastosuj).", "success")
        return redirect(url_for("dbeditor.fishing"))

    dbeditor.add_section("dbeditor.fishing", "🎣", "Łowienie ryb",
                         "co i jak często łowi każda wędka +0…+9 (z przynętą), ryby i skarby")
