"""MT2009_PLUS_DB_EDITOR_V1: "Tabela doświadczenia" - how many experience
points (PD) each level needs.

Where it lives: the engine has the table compiled in (game/src/constants.cpp
exp_table_common, levels 0-250) AND - ENABLE_EXPTABLE_FROMDB in
game/src/config.cpp - every core reads "SELECT level, exp FROM exp_table" of
the COMMON database while it boots (config_init, __LoadExpTableFromDB) and
puts each row over the compiled value. So the override is
common.exp_table (level, exp; 120 rows in this world) - no engine patch and
no file is needed; a level without a row keeps the compiled value. The cores
read it once, at start: an edit is live after "Zastosuj" (restart).

exp_table[level] is what a character of that level needs to reach the next
one (CHARACTER::GetNextExp); the bar starts from 0 again at every level, so
the values are not cumulative. The engine keeps experience as a DWORD; the
compiled table's ceiling is 2 500 000 000 (levels 120-250), MAX_LEVEL is 120
in this server's CONFIG files.

Bots (overlays/playerbot): they level through the same PointChange(POINT_EXP)
and GetNextExp as players; their hunting missions pay a % of exp_table
(playerbot_missions.h) and so follow an edit; the new pets
(playerbot_newpet.h) use the compiled exp_table_common and do NOT.
Nothing in apply.sh writes common.exp_table.

Every save goes through common_items (history with undo, pending until
Zastosuj).
"""
from flask import flash, redirect, render_template, request, url_for

from dbeditor import common_items as common
from dbeditor import tables_common

TABLE = "common.exp_table"
MAX_LEVEL = 120          # MAX_LEVEL of the server's CONFIG files
EXP_MAX = 4000000000     # a DWORD with room for the experience gained over it
EXP_ENGINE_CEILING = 2500000000

# game/src/constants.cpp exp_table_common[0..120] (above 120 every level is 2 500 000 000).
BUILTIN = [
    0, 300, 800, 1500, 2500, 4300, 7200, 11000, 17000, 24000,
    33000, 43000, 58000, 76000, 100000, 130000, 169000, 219000, 283000, 365000,
    472000, 610000, 705000, 813000, 937000, 1077000, 1237000, 1418000, 1624000, 1857000,
    2122000, 2421000, 2761000, 3145000, 3580000, 4073000, 4632000, 5194000, 5717000, 6264000,
    6837000, 7600000, 8274000, 8990000, 9753000, 10560000, 11410000, 12320000, 13270000, 14280000,
    15340000, 16870000, 18960000, 19980000, 21420000, 22930000, 24530000, 26200000, 27960000, 29800000,
    32780000, 36060000, 39670000, 43640000, 48000000, 52800000, 58080000, 63890000, 70280000, 77310000,
    85040000, 93540000, 102900000, 113200000, 124500000, 137000000, 150700000, 165700000, 236990000, 260650000,
    286780000, 315380000, 346970000, 381680000, 419770000, 461760000, 508040000, 558740000, 614640000, 676130000,
    743730000, 1041222000, 1145344200, 1259878620, 1385866482, 1524453130, 1676898443, 1844588288, 2029047116,
    2050000000, 2150000000, 2210000000, 2250000000, 2280000000, 2310000000, 2330000000, 2350000000, 2370000000,
    2390000000, 2400000000, 2410000000, 2420000000, 2430000000, 2440000000, 2450000000, 2460000000, 2470000000,
    2480000000, 2490000000, 2490000000, 2500000000,
]
assert len(BUILTIN) == MAX_LEVEL + 1

SPEC = {"kind": "int", "min": 1, "max": EXP_MAX, "label": "PD potrzebne do następnego poziomu"}
# (label, from, to, factor) - the quick buttons; factor None = the built-in values.
PRESETS = [
    ("×0,8 PD dla poziomów 75–99", 75, 99, 0.8),
    ("×0,5 PD dla poziomów 1–30 (szybszy start)", 1, 30, 0.5),
    ("×1,25 PD dla poziomów 100–119", 100, 119, 1.25),
    ("Przywróć wbudowaną tabelę gry (1–120)", 1, MAX_LEVEL, None),
]


def builtin(level):
    return BUILTIN[level] if 0 <= level < len(BUILTIN) else EXP_ENGINE_CEILING


def apply_preset(table, low, high, factor):
    """{level: new exp} for levels low..high: ×factor of the current value
    (rounded, at least 1, at most EXP_MAX) or the built-in value."""
    out = {}
    for level in range(max(1, low), min(MAX_LEVEL, high) + 1):
        if factor is None:
            value = builtin(level)
        else:
            value = int(round(table.get(level, builtin(level)) * factor))
        out[level] = min(EXP_MAX, max(1, value))
    return out


def chart(levels, width=960, height=300):
    """An SVG line chart (log scale) of the current and the built-in values:
    {"w", "h", "cur", "base", "ticks", "xticks", "points"}."""
    import math
    left, right, top, bottom = 64, 12, 12, 28
    values = [l["exp"] for l in levels] + [l["builtin"] for l in levels]
    low = max(1, min(v for v in values if v > 0)) if values else 1
    high = max(values) if values else 10
    lo, hi = math.floor(math.log10(low)), math.ceil(math.log10(max(high, 10)))
    if hi <= lo:
        hi = lo + 1
    span = len(levels) - 1 or 1

    def x(i):
        return left + (width - left - right) * i / span

    def y(v):
        return top + (height - top - bottom) * (1 - (math.log10(max(v, 1)) - lo) / (hi - lo))

    cur = " ".join(f"{x(i):.1f},{y(l['exp']):.1f}" for i, l in enumerate(levels))
    base = " ".join(f"{x(i):.1f},{y(l['builtin']):.1f}" for i, l in enumerate(levels))
    names = {0: "1", 3: "1 tys.", 6: "1 mln", 9: "1 mld"}
    ticks = [{"y": round(y(10 ** p), 1), "label": names.get(p, f"10^{p}")} for p in range(lo, hi + 1)]
    xticks = [{"x": round(x(i), 1), "label": str(l["level"])} for i, l in enumerate(levels)
              if l["level"] == 1 or l["level"] % 10 == 0]
    step = (width - left - right) / span
    points = [{"x": round(x(i) - step / 2, 1), "w": round(step, 1), "level": l["level"], "exp": l["exp"],
               "builtin": l["builtin"]} for i, l in enumerate(levels)]
    return {"w": width, "h": height, "cur": cur, "base": base, "ticks": ticks, "xticks": xticks, "points": points,
            "left": left, "right": width - right, "top": top, "bottom": height - bottom}


def install(bp, ctx):
    import dbeditor
    common.init(ctx)
    common.register_table(TABLE, "level", {"exp": SPEC}, "Tabela doświadczenia", "dbeditor.exptable",
                          lambda col, value: tables_common.num_text(value) + " PD")
    common.install_history(bp, ctx)
    tables_common.register(bp)
    login_required = ctx["login_required"]

    def q(sql, params=()):
        return common.ctx()["rows"](sql, params)

    def load():
        """{level: exp} of the database rows."""
        return {int(r["level"]): int(r["exp"]) for r in q(f"SELECT `level`, `exp` FROM {TABLE}")}

    def pending_here():
        try:
            return [c for c in common.pending_changes() if c["tbl"] == TABLE]
        except Exception:
            return []

    def save(changes, note):
        """changes {level: exp}; levels without a row get one with the built-in
        value first (what the game already uses for them), so the history and
        its undo cover the real change."""
        table = load()
        for level in sorted(changes):
            if level not in table:
                q(f"INSERT INTO {TABLE} (`level`, `exp`) VALUES (%s, %s)", (level, builtin(level)))
        updates = [(level, {"exp": value}) for level, value in sorted(changes.items())]
        _batch, changed = common.save_rows(TABLE, updates, note=note, label_of=lambda level: f"Poziom {level}")
        return changed

    def report(changed):
        if changed:
            flash(f"Zapisano {len(changed)} poziom(ów). Gra wczyta nową tabelę po restarcie (Zastosuj).", "success")
            if any(int(new) > EXP_ENGINE_CEILING for _k, _c, _o, new in changed):
                flash("Niektóre poziomy wymagają więcej niż 2 500 000 000 PD – tyle wynosi największa wartość "
                      "wbudowana w grę; większe wartości nie były testowane.", "warning")
        else:
            flash("Nic się nie zmieniło.", "info")

    @bp.route("/exp", methods=["GET", "POST"])
    @login_required
    def exptable():
        if request.method == "POST":
            if not common.check_csrf():
                return redirect(url_for("dbeditor.exptable"))
            action = request.form.get("action", "save")
            try:
                table = load()
            except Exception as exc:
                flash(f"Nie udało się odczytać tabeli: {exc}", "error")
                return redirect(url_for("dbeditor.exptable"))
            if action == "save":
                changes, errors = {}, []
                for level in range(1, MAX_LEVEL + 1):
                    raw = request.form.get(f"l{level}")
                    if raw is None:
                        continue
                    value, error = common.validate(SPEC, raw, f"Poziom {level}")
                    if error:
                        errors.append(error)
                    elif value != table.get(level, builtin(level)):
                        changes[level] = value
                if errors:
                    for error in errors[:10]:
                        flash(error, "error")
                    return redirect(url_for("dbeditor.exptable"))
                note = request.form.get("note", "")[:200] or "Tabela doświadczenia: zmiana ręczna"
            else:
                try:
                    if action.startswith("preset"):
                        label, low, high, factor = PRESETS[int(action[6:])]
                    else:
                        low, high = int(request.form.get("from", "")), int(request.form.get("to", ""))
                        factor = float(request.form.get("factor", "").replace(",", ".").strip())
                        if not (1 <= low <= high <= MAX_LEVEL) or not 0.01 <= factor <= 100:
                            raise ValueError
                        label = f"×{factor:g} PD dla poziomów {low}–{high}".replace(".", ",")
                except (ValueError, IndexError):
                    flash(f"Podaj poziomy 1–{MAX_LEVEL} (od ≤ do) i mnożnik 0,01–100 (np. 0,8).", "error")
                    return redirect(url_for("dbeditor.exptable"))
                changes = {lv: v for lv, v in apply_preset(table, low, high, factor).items()
                           if v != table.get(lv, builtin(lv))}
                note = "Tabela doświadczenia: " + label
            try:
                changed = save(changes, note) if changes else []
            except Exception as exc:
                flash(f"Nie zapisano: {exc}", "error")
                return redirect(url_for("dbeditor.exptable"))
            report(changed)
            return redirect(url_for("dbeditor.exptable"))

        error = None
        try:
            table = load()
        except Exception as exc:
            table, error = {}, str(exc)
        try:
            highlight = int(request.args.get("vnum", "0"))
        except ValueError:
            highlight = 0
        pending = pending_here()
        pending_levels = {int(c["row_key"]) for c in pending if str(c["row_key"]).isdigit()}
        levels, total = [], 0
        for level in range(1, MAX_LEVEL + 1):
            exp = table.get(level, builtin(level))
            total += exp
            base = builtin(level)
            levels.append({"level": level, "exp": exp, "builtin": base, "in_db": level in table,
                           "diff": (100.0 * exp / base - 100) if base else 0.0, "total": total,
                           "pending": level in pending_levels})
        extra = sorted(lv for lv in table if lv > MAX_LEVEL)
        try:
            history = common.history_batches(8, TABLE)
        except Exception:
            history = []
        return render_template("dbeditor/exptable.html", levels=levels, chart=chart(levels), presets=PRESETS,
                               error=error, extra=extra, highlight=highlight, pending=pending, history=history,
                               max_level=MAX_LEVEL, ceiling=EXP_ENGINE_CEILING, dbe_csrf=common.csrf_token(),
                               **common.template_helpers())

    dbeditor.add_section("dbeditor.exptable", "📈", "Tabela doświadczenia",
                         "ile PD potrzeba na każdy poziom 1–120 – z wykresem i gotowymi zmianami (np. ×0,8 dla 75–99)")
