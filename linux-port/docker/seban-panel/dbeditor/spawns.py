"""MT2009_PLUS_DB_EDITOR_V1: "Respawn bossów i metinów" - the timed bosses
(special_spawns.txt) and every map's boss.txt / stone.txt in a layman's
view: "Boss / mapa / co ile minut", edited, added, removed.

Files, backups and how the game takes them over: spawnfiles.py and the game
image's m2-spawns (before the cores boot). Every change waits for
"Zastosuj" (a restart) and shows up there as pending.

The panel's own respawn setting (the "Respawny" page, app.py /respawns:
the event flags fastBossSpawn / fastMobSpawn - "every N% of the time" - and
m2_boss_count / m2_mob_count - "N% as many") is not part of these files: the
core applies it while it runs, on top of whatever time a line has
(regen.cpp regen_event / regen_target_count). This editor changes the BASE
time in the file and shows next to it the time the game will really use with
that setting - it never writes the setting into the files, so it is never
applied twice, and changing it on "Respawny" keeps working for every line.
The timed bosses of special_spawns.txt are not touched by that setting at all.
"""
import hashlib
import re

from flask import abort, flash, g, redirect, render_template, request, url_for

from . import common_items as common
from . import spawnfiles as sf

NEW_ROWS = 3
NEW_PLACES = 4


def install(bp, ctx):
    import dbeditor

    common.init(ctx)
    login_required = ctx["login_required"]

    def rows(sql, params=()):
        return common.ctx()["rows"](sql, params)

    def spool():
        return sf.spool_dir()

    # ------------------------------------------------------------ data ---
    def mobs():
        """{vnum: {name, rank, type, level}} of the whole mob_proto (once a request)."""
        if "dbe_spawn_mobs" not in g:
            table = {}
            try:
                for row in rows("SELECT vnum, CAST(`locale_name` AS BINARY) AS `locale_name`, `rank`, type, level "
                                "FROM world.mob_proto"):
                    table[int(row["vnum"])] = {"name": common.text_of(row.get("locale_name")) or f"#{row['vnum']}",
                                               "rank": int(row.get("rank") or 0), "type": int(row.get("type") or 0),
                                               "level": int(row.get("level") or 0)}
            except Exception:
                pass
            g.dbe_spawn_mobs = table
        return g.dbe_spawn_mobs

    def flags():
        if "dbe_spawn_flags" not in g:
            g.dbe_spawn_flags = sf.read_flags(rows)
        return g.dbe_spawn_flags

    def index_or_none():
        try:
            return sf.SpawnIndex.load(spool())
        except Exception:
            return None

    def mob_name(vnum):
        info = mobs().get(int(vnum))
        return info["name"] if info else f"nieznany potwór #{vnum}"

    def settings_summary():
        f = flags()
        boss, _s = sf.delay_percent(f, True, 0)
        mob, _s = sf.delay_percent(f, False, 0)
        per_map = sorted({int(m.group(2)) for k in f for m in [re.fullmatch(r"fast(Boss|Mob)Spawn(\d+)", k)]
                          if m and 0 < int(f[k] or 0) < 100})
        status = {}
        function = sf.dropfiles.panel_function("read_map_regen_status")
        if function is not None:
            try:
                status = function() or {}
            except Exception:
                status = {}
        return {"boss": boss, "mob": mob,
                "boss_count": sf.count_percent(f, True, 0, True), "mob_count": sf.count_percent(f, False, 0, True),
                "per_map": per_map, "map_regens": bool(status.get("values") or status.get("stones")),
                "map_regen_status": status}

    def describe(index, row, map_index):
        """A respawn line in plain words, with its time and count in the game."""
        members = index.members(row["type"], row["vnum"])
        table = mobs()
        known = [m for m in members if m in table]
        boss_or_stone = any(table[m]["rank"] >= 4 or sf.is_mini_boss(m) for m in known)
        all_fighters = bool(members) and len(known) == len(members) and all(table[m]["type"] in (0, 2) for m in known)
        if row["type"] in ("m", "s"):
            name = mob_name(row["vnum"])
        elif row["type"] == "g":
            group = index.groups.get(row["vnum"])
            if group:
                head = group["leader"] or (group["members"][0] if group["members"] else None)
                others = len(group["members"]) - 1
                name = (mob_name(head) if head else f"grupa {row['vnum']}") + (f" + {others} innych" if others > 0 else "")
            else:
                name = f"nieznana grupa #{row['vnum']}"
        else:
            groups = index.group_groups.get(row["vnum"], [])
            leaders = []
            for number in groups[:3]:
                group = index.groups.get(number) or {}
                head = group.get("leader") or (group.get("members") or [None])[0]
                if head:
                    leaders.append(mob_name(head))
            name = ("losowo: " + " / ".join(leaders) + (" …" if len(groups) > 3 else "")) if leaders else f"grupa grup #{row['vnum']}"
        if any(table.get(m, {}).get("type") == 2 for m in known):
            icon = "🗿"
        elif boss_or_stone:
            icon = "👑"
        else:
            icon = "👹"
        level = max((table[m]["level"] for m in known), default=0)
        percent, source = sf.delay_percent(flags(), boss_or_stone, map_index)
        count_pct = sf.count_percent(flags(), boss_or_stone, map_index, all_fighters)
        return {"name": name, "icon": icon, "level": level, "boss_or_stone": boss_or_stone,
                "low": row["low"], "high": row["high"], "once": row["low"] == 0,
                "eff_low": sf.effective_seconds(row["low"], percent), "eff_high": sf.effective_seconds(row["high"], percent),
                "percent": percent, "source": source, "count": row["count"],
                "eff_count": row["count"] * count_pct // 100, "count_pct": count_pct,
                "flag": sf.ROW_FLAGS.get(row.get("flag", ""), "")}

    def row_form(row):
        """What the edit form shows for a line (strings, as they come back)."""
        return {"typ": row["type"], "vnum": str(row["vnum"]), "x": str(row["x"]), "y": str(row["y"]),
                "rx": str(row["rx"]), "ry": str(row["ry"]), "od": sf.minutes_text(row["low"]),
                "do": sf.minutes_text(row["high"]), "ile": str(row["count"])}

    def special_view(index, group):
        members = index.special_members(group)
        table = mobs()
        low, high = group["time"] or (0, 0)
        if group["spawn_type"] == "group":
            info = index.groups.get(group["spawn_vnum"] or 0)
            head = (info or {}).get("leader") or ((info or {}).get("members") or [None])[0]
            name = (mob_name(head) if head else f"grupa {group['spawn_vnum']}") + (
                f" + {len(info['members']) - 1} innych" if info and len(info["members"]) > 1 else "")
        else:
            name = mob_name(group["spawn_vnum"] or 0)
        if group["time_type"] == "hour":
            when = f"codziennie o {clock(low)}" + (f"–{clock(high)}" if high != low else "")
        else:
            when = "co " + sf.range_text(low, high)
        initial = group.get("initial_delay")
        return {"group": group, "name": name, "when": when, "map": group["map_index"],
                "map_title": index.map_title(group["map_index"]) if group["map_index"] is not None else "?",
                "places": len(group["places"]), "level": max((table[m]["level"] for m in members if m in table), default=0),
                "initial": sf.range_text(*initial) if initial else "3–6 s",
                "origin": group.get("origin", "image")}

    def file_state(key):
        try:
            return sf.live_state(spool(), key)
        except Exception:
            return {"kind": "unknown", "pending": False, "text": ""}

    def page_context():
        try:
            pending = common.pending_context()
        except Exception:
            pending = {"pending_total": 0}
        try:
            pending["pending_spawns"] = len(sf.pending_changes(spool()))
        except Exception:
            pending["pending_spawns"] = 0
        return pending

    def missing_index():
        return render_template("dbeditor/spawns_missing.html", **page_context())

    # ------------------------------------------------------- overview ---
    @bp.route("/spawns")
    @login_required
    def spawns():
        index = index_or_none()
        if index is None:
            return missing_index()
        specials = sorted((special_view(index, grp) for grp in index.special),
                          key=lambda v: (v["map_title"], v["name"]))
        removed = [grp for grp in index.special_custom if grp["removed"]]
        maps = []
        for mapdir in index.map_dirs_with_files():
            map_index = index.dirs[mapdir]
            entry = {"dir": mapdir, "index": map_index, "title": index.map_title(map_index), "files": []}
            for kind, title in sf.MAP_FILES.items():
                lines, origin = index.map_lines(mapdir, kind)
                described = [describe(index, ln["row"], map_index) for ln in lines if ln["row"] and not ln["row"].get("bad")]
                if not described and origin == "image":
                    continue
                entry["files"].append({"kind": kind, "title": title, "rows": len(described), "origin": origin,
                                       "state": file_state(sf.key_of(mapdir, kind)),
                                       "low": min((d["low"] for d in described if d["low"]), default=0),
                                       "high": max((d["high"] for d in described), default=0),
                                       "eff_low": min((d["eff_low"] for d in described if d["eff_low"]), default=0),
                                       "eff_high": max((d["eff_high"] for d in described), default=0),
                                       "bosses": [d for d in described if d["icon"] == "👑"][:6]})
            if entry["files"]:
                maps.append(entry)
        maps.sort(key=lambda m: m["title"])
        return render_template("dbeditor/spawns.html", specials=specials, removed=removed, maps=maps,
                               settings=settings_summary(), special_state=file_state("special"),
                               duration=sf.duration_text, range_text=sf.range_text, mob_name=mob_name,
                               dbe_csrf=common.csrf_token(), **page_context())

    # ---------------------------------------------------- one map file ---
    def parse_rows_form(index, form, lines, size):
        """The submitted lines -> (new lines, errors). An untouched line keeps
        its exact text; a changed one is written in the file's notation."""
        errors, out = [], []
        table = mobs()
        width, height = size or (5000, 5000)

        def build(prefix, original, number):
            typ = (form.get(prefix + "typ") or "m").strip()
            if typ not in sf.ROW_TYPES:
                raise ValueError(f"wiersz {number}: nieznany rodzaj „{typ}”.")
            vnum = sf.parse_int(form.get(prefix + "vnum"), f"wiersz {number}: numer", 1, 4294967295)
            if typ in ("m", "s") and table and vnum not in table:
                raise ValueError(f"wiersz {number}: nie ma potwora {vnum} w mob_proto.")
            if typ == "g" and vnum not in index.groups:
                raise ValueError(f"wiersz {number}: nie ma grupy {vnum} w group.txt.")
            if typ == "r" and vnum not in index.group_groups:
                raise ValueError(f"wiersz {number}: nie ma grupy grup {vnum} w group_group.txt.")
            x = sf.parse_int(form.get(prefix + "x"), f"wiersz {number}: X", 0, width)
            y = sf.parse_int(form.get(prefix + "y"), f"wiersz {number}: Y", 0, height)
            rx = sf.parse_int(form.get(prefix + "rx") or "0", f"wiersz {number}: obszar X", 0, 2000)
            ry = sf.parse_int(form.get(prefix + "ry") or "0", f"wiersz {number}: obszar Y", 0, 2000)
            low = sf.parse_minutes(form.get(prefix + "od"), f"wiersz {number}: co ile – od")
            high_raw = (form.get(prefix + "do") or "").strip()
            high = sf.parse_minutes(high_raw, f"wiersz {number}: co ile – do") if high_raw else low
            if high < low:
                raise ValueError(f"wiersz {number}: „do” ({sf.minutes_text(high)} min) mniejsze niż „od”.")
            if low == 0 and high > 0:
                raise ValueError(f"wiersz {number}: czas 0 znaczy „tylko raz przy starcie” – wtedy „do” też 0.")
            count = sf.parse_int(form.get(prefix + "ile"), f"wiersz {number}: ile naraz", 1, 100)
            row = dict(original or {"z": 0, "dir": 0, "pct": 100, "flag": ""})
            row.update(type=typ, x=x, y=y, rx=rx, ry=ry, count=count, vnum=vnum, low=low, high=high,
                       time=sf.time_token(low, high))
            return row

        number = 0
        for position, line in enumerate(lines):
            row = line["row"]
            if not row or row.get("bad"):
                out.append(line)
                continue
            number += 1
            prefix = f"r{position}_"
            if form.get(prefix + "usun") == "1":
                continue
            shown = row_form(row)
            sent = {k: (form.get(prefix + k) or "").strip() for k in shown}
            if sent == shown:
                out.append(line)
                continue
            try:
                new = build(prefix, row, number)
            except ValueError as exc:
                errors.append(str(exc))
                out.append(line)
                continue
            out.append({"raw": sf.render_row(new), "row": new})
        for extra in range(int(form.get("nowe") or 0)):
            prefix = f"n{extra}_"
            if not (form.get(prefix + "vnum") or "").strip():
                continue
            number += 1
            try:
                new = build(prefix, None, number)
            except ValueError as exc:
                errors.append(str(exc))
                continue
            out.append({"raw": sf.render_row(new), "row": new})
        return out, errors

    @bp.route("/spawns/mapa/<mapdir>", methods=["GET", "POST"])
    @login_required
    def spawn_map(mapdir):
        index = index_or_none()
        if index is None:
            return missing_index()
        if mapdir not in index.dirs or mapdir not in index.map_dirs_with_files():
            abort(404)
        map_index = index.dirs[mapdir]
        size = index.map_size(mapdir)
        if request.method == "POST":
            kind = request.form.get("plik", "")
            back = url_for("dbeditor.spawn_map", mapdir=mapdir) + f"#plik-{kind}"
            if kind not in sf.MAP_FILES or not common.check_csrf():
                return redirect(back)
            lines, origin = index.map_lines(mapdir, kind)
            current = sf.render_regen(lines)
            if request.form.get("wersja") != hashlib.sha256(current).hexdigest()[:16]:
                flash("Plik zmienił się od otwarcia strony (ktoś inny zapisał zmiany) – sprawdź i zapisz jeszcze raz.", "error")
                return redirect(back)
            new_lines, errors = parse_rows_form(index, request.form, lines, size)
            problems = sf.check_regen_lines(new_lines)
            if errors or problems:
                for error in (errors + problems)[:8]:
                    flash(error, "error")
                flash("Nic nie zapisano – popraw zaznaczone wiersze.", "error")
                return redirect(back)
            base_lines, _o = index.map_lines(mapdir, kind, wanted=False)
            data = sf.render_regen(new_lines)
            if data == current:
                flash("Brak zmian do zapisania.", "success")
                return redirect(back)
            # Back to exactly the image's file = no custom file at all.
            same_as_image = data == sf.render_regen(base_lines)
            try:
                sf.write_custom(spool(), sf.key_of(mapdir, kind), None if same_as_image else data,
                                f"{index.map_title(map_index)}: {sf.MAP_FILES[kind].lower()}")
            except OSError as exc:
                flash(f"Nie udało się zapisać pliku: {exc}", "error")
                return redirect(back)
            flash(f"Zapisano: {sf.MAP_FILES[kind]} – {index.map_title(map_index)}. Zmiana czeka na zastosowanie "
                  "(restart gry – „Zastosuj”).", "success")
            return redirect(back)
        files = []
        for kind, title in sf.MAP_FILES.items():
            lines, origin = index.map_lines(mapdir, kind)
            if not (index.spool / "spawns" / "base" / "maps" / mapdir / f"{kind}.txt").exists():
                continue
            entries = []
            for position, line in enumerate(lines):
                row = line["row"]
                if row and not row.get("bad"):
                    entries.append({"pos": position, "row": row, "form": row_form(row),
                                    "info": describe(index, row, map_index)})
            files.append({"kind": kind, "title": title, "origin": origin, "entries": entries,
                          "other": sum(1 for ln in lines if ln["row"] and ln["row"].get("bad")),
                          "state": file_state(sf.key_of(mapdir, kind)),
                          "version": hashlib.sha256(sf.render_regen(lines)).hexdigest()[:16]})
        return render_template("dbeditor/spawns_map.html", mapdir=mapdir, map_index=map_index,
                               title=index.map_title(map_index), files=files, size=size,
                               picture=index.map_picture(map_index), settings=settings_summary(),
                               row_types=sf.ROW_TYPES, new_rows=NEW_ROWS, duration=sf.duration_text,
                               range_text=sf.range_text, dbe_csrf=common.csrf_token(), **page_context())

    @bp.post("/spawns/mapa/<mapdir>/<kind>/przywroc")
    @login_required
    def spawn_map_reset(mapdir, kind):
        back = url_for("dbeditor.spawn_map", mapdir=mapdir)
        if kind not in sf.MAP_FILES or not sf.DIR_RE.match(mapdir) or not common.check_csrf():
            return redirect(back)
        if not sf.map_custom_path(spool(), mapdir, kind).exists():
            flash("Ten plik jest już taki, jak w obrazie gry.", "success")
            return redirect(back)
        sf.write_custom(spool(), sf.key_of(mapdir, kind), None, f"{mapdir}: przywrócono plik z obrazu gry")
        flash("Przywrócono plik z obrazu gry (kopia Twojej wersji jest w „Kopie zapasowe”). "
              "Zmiana czeka na zastosowanie (restart gry).", "success")
        return redirect(back)

    # ------------------------------------------------- timed bosses ---
    def special_form_values(group):
        low, high = group.get("time") or (3600, 3600)
        initial = group.get("initial_delay")
        return {"nazwa": group.get("name", ""), "rodzaj": group.get("spawn_type") or "mob",
                "spawn_vnum": str(group.get("spawn_vnum") or ""), "mapa": str(group.get("map_index") or ""),
                "czas_typ": group.get("time_type") or "normal",
                "od": sf.minutes_text(low), "do": sf.minutes_text(high), "godz_od": clock(low), "godz_do": clock(high),
                "ile": str(group.get("count") or 1),
                "start_od": sf.minutes_text(initial[0]) if initial else "", "start_do": sf.minutes_text(initial[1]) if initial else "",
                "zapamietaj": "1" if group.get("save_kill") else "", "kanal": str(group.get("channel") or 0),
                "ogloszenie": str(group.get("notify_level") or 0), "flaga": group.get("event_flag") or ""}

    def parse_special_form(index, form, original, vnum):
        errors = []
        group = dict(original or {"keys": {}, "order": [], "places": []})
        group["vnum"] = vnum
        group["removed"] = False

        def take(fn):
            try:
                return fn()
            except ValueError as exc:
                errors.append(str(exc))
                return None

        name = (form.get("nazwa") or "").strip()
        if not sf.GROUP_NAME_RE.match(name):
            errors.append("Nazwa wpisu: tylko litery bez polskich znaków, cyfry i _ (bez spacji), np. Boss_Las.")
        group["name"] = name
        kind = form.get("rodzaj")
        if kind not in ("mob", "group"):
            errors.append("Wybierz: jeden potwór albo grupa.")
        group["spawn_type"] = kind
        spawn = take(lambda: sf.parse_int(form.get("spawn_vnum"), "Numer potwora / grupy", 1, 4294967295))
        if spawn is not None:
            if kind == "mob" and mobs() and spawn not in mobs():
                errors.append(f"Nie ma potwora {spawn} w mob_proto.")
            if kind == "group" and spawn not in index.groups:
                errors.append(f"Nie ma grupy {spawn} w group.txt.")
        group["spawn_vnum"] = spawn
        map_index = take(lambda: sf.parse_int(form.get("mapa"), "Mapa", 1, 100000))
        if map_index is not None and map_index not in index.maps:
            errors.append(f"Mapy {map_index} nie ma w map/index.")
        group["map_index"] = map_index
        time_type = form.get("czas_typ") if form.get("czas_typ") in ("normal", "hour") else "normal"
        group["time_type"] = time_type
        if time_type == "hour":
            low = take(lambda: parse_clock(form.get("godz_od"), "Godzina od"))
            high = take(lambda: parse_clock(form.get("godz_do") or form.get("godz_od"), "Godzina do"))
        else:
            low = take(lambda: sf.parse_minutes(form.get("od"), "Co ile – od", allow_zero=False))
            high = take(lambda: sf.parse_minutes(form.get("do") or form.get("od"), "Co ile – do", allow_zero=False))
        if low is not None and high is not None and high < low:
            errors.append("Czas „do” jest mniejszy niż „od”.")
        group["time"] = (low or 0, high or 0)
        start_low_raw, start_high_raw = (form.get("start_od") or "").strip(), (form.get("start_do") or "").strip()
        if start_low_raw or start_high_raw:
            s_low = take(lambda: sf.parse_minutes(start_low_raw or start_high_raw, "Pierwsze pojawienie – od"))
            s_high = take(lambda: sf.parse_minutes(start_high_raw or start_low_raw, "Pierwsze pojawienie – do"))
            if s_low is not None and s_high is not None and s_high < s_low:
                errors.append("Pierwsze pojawienie: „do” mniejsze niż „od”.")
            group["initial_delay"] = (s_low or 0, s_high or 0)
        else:
            group["initial_delay"] = None
        group["save_kill"] = 1 if form.get("zapamietaj") == "1" else 0
        group["channel"] = take(lambda: sf.parse_int(form.get("kanal") or "0", "Kanał", 0, 99)) or 0
        group["notify_level"] = take(lambda: sf.parse_int(form.get("ogloszenie") or "0", "Ogłoszenie", 0, 255)) or 0
        flag = (form.get("flaga") or "").strip()
        if flag and not sf.FLAG_RE.match(flag):
            errors.append("Flaga wydarzenia: tylko litery bez polskich znaków, cyfry i _.")
        group["event_flag"] = flag
        size = index.map_size(index.maps.get(map_index, "")) if map_index in index.maps else None
        width, height = size or (5000, 5000)
        places = []
        for i in range(int(form.get("miejsca") or 0)):
            prefix = f"p{i}_"
            if form.get(prefix + "usun") == "1" or not ((form.get(prefix + "x") or "").strip() or (form.get(prefix + "y") or "").strip()):
                continue
            x = take(lambda: sf.parse_int(form.get(prefix + "x"), f"Miejsce {len(places) + 1}: X", 0, width))
            y = take(lambda: sf.parse_int(form.get(prefix + "y"), f"Miejsce {len(places) + 1}: Y", 0, height))
            rot = take(lambda: sf.parse_int(form.get(prefix + "rot") or "0", f"Miejsce {len(places) + 1}: kierunek", 0, 8))
            if None not in (x, y, rot):
                places.append((x, y, rot))
        if not places:
            errors.append("Podaj co najmniej jedno miejsce (X, Y).")
        if len(places) > 255:
            errors.append("Najwyżej 255 miejsc.")
        group["places"] = places
        count = take(lambda: sf.parse_int(form.get("ile"), "Ile naraz", 1, 255))
        if count is not None and places and count > len(places):
            errors.append(f"Ile naraz ({count}) nie może być większe niż liczba miejsc ({len(places)}) – "
                          "gra by się przez to nie uruchomiła.")
        group["count"] = count or 1
        names = {grp["name"].lower() for grp in index.special if grp["vnum"] != vnum}
        if name and name.lower() in names:
            errors.append(f"Wpis o nazwie {name} już istnieje – wybierz inną nazwę.")
        return group, errors

    def save_special(index, change, reason):
        """change(custom groups by vnum) -> custom groups; written as the custom file."""
        custom = {grp["vnum"]: grp for grp in index.special_custom}
        change(custom)
        base = {grp["vnum"]: grp for grp in index.special_base}
        keep = []
        for vnum, grp in sorted(custom.items()):
            original = base.get(vnum)
            if original is not None and not grp["removed"] and \
                    sf.render_special_group(grp) == sf.render_special_group(dict(original, removed=False)):
                continue  # the same as the image's
            if original is None and grp["removed"]:
                continue
            keep.append(grp)
        sf.write_custom(spool(), "special", sf.render_special(keep) if keep else None, reason)

    def special_page(index, group, vnum, values, is_new):
        map_index = int(values["mapa"]) if values.get("mapa", "").isdigit() else None
        mapdir = index.maps.get(map_index) if map_index is not None else None
        maps = sorted(((i, index.map_title(i)) for i in index.maps), key=lambda m: m[1])
        places = list(group.get("places") or []) if group else []
        return render_template("dbeditor/spawns_special.html", group=group, vnum=vnum, v=values, is_new=is_new,
                               maps=maps, places=places, new_places=NEW_PLACES,
                               size=index.map_size(mapdir) if mapdir else None,
                               picture=index.map_picture(map_index) if map_index is not None else None,
                               view=special_view(index, group) if group and not is_new else None,
                               base=next((grp for grp in index.special_base if grp["vnum"] == vnum), None),
                               custom=next((grp for grp in index.special_custom if grp["vnum"] == vnum), None),
                               state=file_state("special"), dbe_csrf=common.csrf_token(), **page_context())

    @bp.route("/spawns/specjalne/<int:vnum>", methods=["GET", "POST"])
    @bp.route("/spawns/specjalne/nowy", methods=["GET", "POST"], defaults={"vnum": None})
    @login_required
    def spawn_special(vnum):
        index = index_or_none()
        if index is None:
            return missing_index()
        is_new = vnum is None
        if is_new:
            used = {grp["vnum"] for grp in index.special_base + index.special_custom if grp["vnum"] is not None}
            vnum = max([900] + list(used)) + 1
            original = None
        else:
            original = next((grp for grp in index.special if grp["vnum"] == vnum), None)
            if original is None:
                abort(404)
        if request.method == "POST":
            if not common.check_csrf():
                return redirect(request.path)
            group, errors = parse_special_form(index, request.form, original, vnum)
            if errors:
                for error in errors[:8]:
                    flash(error, "error")
                flash("Nic nie zapisano – popraw zaznaczone pola.", "error")
                values = {k: request.form.get(k, "") for k in special_form_values({})}
                return special_page(index, dict(group, places=group.get("places") or []), vnum, values, is_new)

            def change(custom):
                custom[vnum] = group
            try:
                save_special(index, change, f"{'nowy' if is_new else 'zmiana'}: {group['name']}")
            except OSError as exc:
                flash(f"Nie udało się zapisać pliku: {exc}", "error")
                return redirect(request.path)
            flash(f"Zapisano: {group['name']}. Zmiana czeka na zastosowanie (restart gry – „Zastosuj”).", "success")
            return redirect(url_for("dbeditor.spawn_special", vnum=vnum))
        values = special_form_values(original or {"map_index": None})
        return special_page(index, original or {"places": []}, vnum, values, is_new)

    @bp.post("/spawns/specjalne/<int:vnum>/usun")
    @login_required
    def spawn_special_delete(vnum):
        index = index_or_none()
        if index is None or not common.check_csrf():
            return redirect(url_for("dbeditor.spawns"))
        base = next((grp for grp in index.special_base if grp["vnum"] == vnum), None)

        def change(custom):
            if base is not None:
                custom[vnum] = dict(base, removed=True)
            else:
                custom.pop(vnum, None)
        save_special(index, change, f"usunięto wpis {vnum}")
        flash("Usunięto bossa czasowego. Zmiana czeka na zastosowanie (restart gry). Cofniesz to przyciskiem "
              "„Przywróć z obrazu gry” w liście usuniętych.", "success")
        return redirect(url_for("dbeditor.spawns"))

    @bp.post("/spawns/specjalne/<int:vnum>/przywroc")
    @login_required
    def spawn_special_reset(vnum):
        index = index_or_none()
        if index is None or not common.check_csrf():
            return redirect(url_for("dbeditor.spawns"))

        def change(custom):
            custom.pop(vnum, None)
        save_special(index, change, f"przywrócono wpis {vnum} z obrazu gry")
        flash("Przywrócono wpis z obrazu gry. Zmiana czeka na zastosowanie (restart gry).", "success")
        if any(grp["vnum"] == vnum for grp in index.special_base):
            return redirect(url_for("dbeditor.spawn_special", vnum=vnum))
        return redirect(url_for("dbeditor.spawns"))

    # ----------------------------------------------------------- backups ---
    @bp.route("/spawns/kopie", methods=["GET", "POST"])
    @login_required
    def spawn_backups():
        index = index_or_none()
        titles = index.dir_titles() if index else {}
        if request.method == "POST":
            if common.check_csrf():
                try:
                    key = sf.restore_backup(spool(), request.form.get("kopia", ""))
                    flash(f"Przywrócono kopię: {sf.key_title(key, titles)}. Zmiana czeka na zastosowanie (restart gry).",
                          "success")
                except (ValueError, OSError) as exc:
                    flash(str(exc), "error")
            return redirect(url_for("dbeditor.spawn_backups"))
        backups = sf.list_backups(spool())
        for backup in backups:
            backup["title"] = sf.key_title(backup["key"], titles)
        return render_template("dbeditor/spawns_backups.html", backups=backups, dbe_csrf=common.csrf_token(),
                               **page_context())

    dbeditor.add_section("dbeditor.spawns", "⏱️", "Respawn bossów i metinów",
                         "Boss / mapa / co ile minut – bossowie czasowi, bossowie i Metiny każdej mapy, miejsca i liczba")


def clock(seconds):
    seconds = int(seconds or 0) % 86400
    return f"{seconds // 3600:02d}:{seconds % 3600 // 60:02d}"


def parse_clock(raw, label):
    match = re.fullmatch(r"\s*(\d{1,2})[:.](\d{2})\s*", raw or "")
    if not match or int(match.group(1)) > 23 or int(match.group(2)) > 59:
        raise ValueError(f"{label}: podaj godzinę jak 18:30.")
    return int(match.group(1)) * 3600 + int(match.group(2)) * 60
