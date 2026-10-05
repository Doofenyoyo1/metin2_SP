"""MT2009_PLUS_DB_EDITOR_V1: "Potwory i bossowie" - world.mob_proto for an
operator who is not a programmer.

The db core reads world.mob_proto once, while it boots (PROTO_FROM_DB, the
same as item_proto; player.mob_proto is only a view of it), so an edit made
here changes the database at once and the game after "Zastosuj" (a restart).
Every saved field goes through common_items.save_rows() into the editor's
one history (player.web_dbeditor_history, tbl 'world.mob_proto'), so the
history page undoes a single monster as well as a whole mass change, and
"Zastosuj" (clientdata.py) lists them as pending.

What the columns mean in this engine (common/tables.h TMobTable, battle.cpp,
char_state.cpp):
  level         BYTE (1-255)   rank  0 PAWN .. 4 BOSS, 5 KING (Metin stones are 5)
  max_hp        DWORD          damage_min/max  the base hit (x dam_multiply)
  def           WORD           exp, gold_min/max  for the kill (before the rates)
  ai_flag       SET - AGGR = attacks on sight within aggressive_sight
  attack_speed / move_speed  100 = normal       resist_*  -100..100 %

The client draws the monster's name and level from its own mob_proto, which
the editor does not rebuild: a new level shows in the game's fights at once
after the restart, but the level above a monster's head stays the client's.
"""
import re

from flask import abort, flash, redirect, render_template, request, url_for

from dbeditor import common_items as common

TABLE = "world.mob_proto"
LIMIT = 300
MASS_PREVIEW_ROWS = 200
MASS_MAX_ROWS = 3000

RANKS = {0: "Zwykły", 1: "Silniejszy", 2: "Rycerz", 3: "Elitarny", 4: "Boss", 5: "Król / Metin"}
TYPES = {0: "Potwór", 1: "NPC", 2: "Kamień Metin", 3: "Teleport", 4: "Drzwi", 5: "Budynek", 6: "Gracz",
         7: "Przemiana", 8: "Koń", 9: "Przejście", 10: "Obszar", 11: "Zwierzak"}
KINDS = {
    "walka": ("Potwory, bossowie i Metiny", "type IN (0,2)"),
    "potwory": ("Zwykłe potwory (bez bossów)", "type=0 AND `rank`<4"),
    "bossowie": ("Bossowie (ranga Boss i Król)", "type=0 AND `rank`>=4"),
    "metiny": ("Kamienie Metin", "type=2"),
    "wszystko": ("Wszystko (także NPC, konie, drzwi…)", None),
}
AI_FLAGS = ["AGGR", "NOMOVE", "COWARD", "NOATTSHINSU", "NOATTCHUNJO", "NOATTJINNO", "ATTMOB", "BERSERK",
            "STONESKIN", "GODSPEED", "DEATHBLOW", "REVIVE", "IGNORE_LAST_ATTACK", "NO_WANDER", "SEE_OTHER_NPC",
            "RETURN_LASTATTACK_POS", "LOOSE_AGGRO_ON_DISTANCE"]
AI_FLAG_LABELS = {
    "AGGR": "Agresywny – atakuje sam, gdy zobaczy gracza", "NOMOVE": "Stoi w miejscu (nie chodzi)",
    "COWARD": "Tchórz – ucieka przy niskim PŻ", "NOATTSHINSU": "Nie atakuje Shinsoo",
    "NOATTCHUNJO": "Nie atakuje Chunjo", "NOATTJINNO": "Nie atakuje Jinno", "ATTMOB": "Atakuje inne potwory",
    "BERSERK": "Szał (silniejszy przy niskim PŻ)", "STONESKIN": "Kamienna skóra", "GODSPEED": "Boska szybkość",
    "DEATHBLOW": "Śmiertelny cios", "REVIVE": "Odradza się po śmierci",
    "IGNORE_LAST_ATTACK": "Nie goni ostatniego napastnika", "NO_WANDER": "Nie wędruje",
    "SEE_OTHER_NPC": "Widzi inne NPC", "RETURN_LASTATTACK_POS": "Wraca na miejsce ataku",
    "LOOSE_AGGRO_ON_DISTANCE": "Odpuszcza pościg na dystans",
}
RESISTS = [("sword", "Miecze"), ("twohand", "Broń dwuręczna"), ("dagger", "Sztylety"), ("bell", "Dzwony"),
           ("fan", "Wachlarze"), ("bow", "Łuki"), ("fire", "Ogień"), ("elect", "Błyskawice"),
           ("magic", "Magia"), ("wind", "Wiatr"), ("poison", "Trucizna")]


def _int(low, high, label):
    return {"kind": "int", "min": low, "max": high, "label": label}


SPECS = {
    "level": _int(1, 255, "Poziom"),
    "rank": _int(0, 5, "Ranga"),
    "max_hp": _int(1, 2000000000, "PŻ (punkty życia)"),
    "damage_min": _int(0, 65535, "Obrażenia – minimum"),
    "damage_max": _int(0, 65535, "Obrażenia – maksimum"),
    "def": _int(0, 65535, "Obrona"),
    "exp": _int(0, 2000000000, "Doświadczenie za zabicie"),
    "gold_min": _int(0, 2000000000, "Yang za zabicie – minimum"),
    "gold_max": _int(0, 2000000000, "Yang za zabicie – maksimum"),
    "ai_flag": {"kind": "set", "members": AI_FLAGS, "label": "Zachowanie (ai_flag)"},
    "aggressive_sight": _int(0, 10000, "Zasięg wzroku agresji"),
    "attack_speed": _int(10, 1000, "Szybkość ataku (100 = zwykła)"),
    "move_speed": _int(10, 1000, "Szybkość ruchu (100 = zwykła)"),
}
for _key, _label in RESISTS:
    SPECS[f"resist_{_key}"] = _int(-100, 100, f"Odporność: {_label} (%)")
EDIT_COLS = list(SPECS)

# MT2009_PLUS_DB_EDITOR_MOB_MELEE_V1: "Zwykłe potwory: atak z dystansu →
# wręcz" - the owner's one-click form of
#   UPDATE world.mob_proto SET battle_type = 0, attack_range = 175
#   WHERE battle_type IN (1, 2) AND `rank` IN (0, 1, 2, 3) AND `type` = 0;
# (ordinary archers and casters fight in melee, bosses, kings/Metins and NPCs
# untouched). Written through common.save_rows() as ONE batch, so every field
# is in the history, "Zastosuj" lists it and one click undoes the lot. The two
# columns stay off the monster's own form (EDIT_COLS); they are registered
# with the history only so it can write and undo them. common/length.h
# EBattleType: 0 MELEE, 1 RANGE, 2 MAGIC; char_state.cpp keeps a RANGE/MAGIC
# monster at attack_range*0.8 and shoots - a MELEE one walks up and hits.
# apply.sh never rewrites battle_type, and attack_range only for 9706 (rank
# 5, not matched); its mob_proto.mt2009plus.sql and the Ochao / pet clones
# are INSERT IGNORE (a row that is there is never changed), mod/30_costume_
# pack.sql (REPLACE) runs once per install and holds only NPCs (type 1).
BATTLE_TYPES = {0: "wręcz", 1: "dystans (łuk)", 2: "magia (dystans)", 3: "specjalny", 4: "siłacz", 5: "tank",
                6: "super siłacz", 7: "super tank"}
MELEE_SPECS = {
    "battle_type": {"kind": "int", "min": 0, "max": 7, "label": "Sposób walki (battle_type)"},
    "attack_range": {"kind": "int", "min": 0, "max": 65535, "label": "Zasięg ataku"},
}
MELEE_WHERE = "battle_type IN (1, 2) AND `rank` IN (0, 1, 2, 3) AND `type` = 0"
MELEE_VALUES = {"battle_type": 0, "attack_range": 175}
MELEE_NOTE = "Zwykłe potwory: atak z dystansu → wręcz"
# What the history may write for a monster: the form's fields and the two above.
ALL_SPECS = dict(SPECS, **MELEE_SPECS)
READ_ONLY = ["type", "battle_type", "st", "dx", "ht", "iq", "dam_multiply", "drop_item", "regen_cycle",
             "regen_percent", "aggressive_hp_pct", "attack_range", "summon", "resurrection_vnum"]

# Mass changes: what can be changed, and which columns move together.
MASS_FIELDS = {
    "max_hp": ("PŻ", ["max_hp"]),
    "damage": ("Obrażenia (min. i maks.)", ["damage_min", "damage_max"]),
    "def": ("Obrona", ["def"]),
    "exp": ("Doświadczenie za zabicie", ["exp"]),
    "gold": ("Yang za zabicie (min. i maks.)", ["gold_min", "gold_max"]),
    "level": ("Poziom", ["level"]),
    "attack_speed": ("Szybkość ataku", ["attack_speed"]),
    "move_speed": ("Szybkość ruchu", ["move_speed"]),
    "aggressive_sight": ("Zasięg wzroku agresji", ["aggressive_sight"]),
}
MASS_OPS = {"mul": "pomnóż przez", "add": "dodaj (lub odejmij)", "set": "ustaw na"}

# What linux-port/docker/mariadb/playerbot/apply.sh writes into
# world.mob_proto at EVERY start of the server (the playerbot-migrate step,
# before the db core boots) - an edit of these fields lasts only until the
# next start. Only its unconditional UPDATEs and only the columns this page
# edits; test_dbeditor_mobs.py keeps the list in step with apply.sh.
BOOT_RULES = [
    ((6001, 6002, 6003, 6004, 6005, 6006, 6007, 6008, 6009, 6051, 6091, 6101, 6102, 6103, 6104, 6105, 6106,
      6107, 6108, 6109, 6151, 6191),
     ("level", "max_hp", "damage_min", "damage_max", "def", "exp"), "Ognista i Lodowa Kraina (Razador, Nemere)"),
    ((2493, 8057, 9677, 9678, 9696, 9701, 9702, 9710, 9711), ("level", "max_hp", "def", "exp"),
     "potwory i Metiny dodatków MT2009 Plus"),
    ((8031, 8032, 8033, 8034), ("level", "max_hp", "def"), "Metiny dodatków MT2009 Plus"),
    ((9601, 9602, 9603, 9604, 9605, 9606, 9607, 9671, 9672, 9673, 9674, 9675, 9676, 9679, 9680, 9681, 9682,
      9683, 9684, 9690, 9691, 9692, 9693, 9694, 9695, 9697, 9698, 9699, 9700, 9707, 9708, 9709, 9712, 9713,
      9714),
     ("rank", "level", "max_hp", "damage_min", "damage_max", "def", "exp", "gold_min", "gold_max", "ai_flag"),
     "potwory map Arezzo"),
    ((9611, 9612, 9613, 9614, 9615), ("max_hp", "damage_min", "damage_max", "exp", "gold_min", "gold_max"),
     "Lemury (Arezzo)"),
    ((9703,), ("rank", "level", "max_hp", "damage_min", "damage_max", "def", "exp", "gold_min", "gold_max",
               "ai_flag", "resist_poison"), "Loch Pająków (Arezzo)"),
    ((9704,), ("rank", "level", "max_hp", "def", "exp", "gold_min", "gold_max", "resist_fire", "resist_poison"),
     "Loch Pająków (Arezzo)"),
    ((9705,), ("rank", "level", "max_hp", "damage_min", "damage_max", "def", "exp", "gold_min", "gold_max",
               "resist_fire", "resist_poison"), "Loch Pająków (Arezzo)"),
    ((9706,), ("rank", "level", "max_hp", "damage_min", "damage_max", "def", "exp", "gold_min", "gold_max",
               "ai_flag", "attack_speed", "move_speed", "attack_range"), "Loch Pająków (Arezzo)"),
]
BOOT_WARNING = "Te pola skrypt startowy bazy ustawia na nowo przy KAŻDYM starcie serwera – Twoja zmiana zniknie po restarcie"


def boot_cols(vnum):
    """The edited columns apply.sh puts back for this monster at every start."""
    cols = set()
    for vnums, rule_cols, _note in BOOT_RULES:
        if int(vnum) in vnums:
            cols |= set(rule_cols)
    return cols


def boot_notes(vnum):
    return [(note, cols) for vnums, cols, note in BOOT_RULES if int(vnum) in vnums]


# ---- rows ------------------------------------------------------------------

ROW_COLS = ["vnum", "locale_name", "name"] + EDIT_COLS + READ_ONLY


def select_sql(where, order="level, vnum", limit=None):
    parts = []
    for col in ROW_COLS:
        if col in ("locale_name", "name"):
            parts.append(f"CAST(`{col}` AS BINARY) AS `{col}`")
        elif col == "ai_flag":
            parts.append("CAST(`ai_flag` AS CHAR) AS `ai_flag`")
        else:
            parts.append(f"`{col}`")
    sql = f"SELECT {', '.join(parts)} FROM {TABLE} WHERE {where} ORDER BY {order}"
    return sql + (f" LIMIT {int(limit)}" if limit else "")


def flags_of(value):
    return {p.strip().upper() for p in (common.text_of(value) or "").split(",") if p.strip()}


def kind_of(row):
    mob_type, rank = int(row.get("type") or 0), int(row.get("rank") or 0)
    if mob_type == 2:
        return "metin", "🗿", "Metin"
    if mob_type == 0 and rank >= 4:
        return "boss", "👑", "Boss"
    if mob_type == 0:
        return "mob", "👹", "Potwór"
    return "other", "🧍", TYPES.get(mob_type, f"Typ {mob_type}")


def decorate(row):
    row = dict(row)
    for col in ("locale_name", "name"):
        row[col] = common.text_of(row.get(col)) or ""
    row["display_name"] = row["locale_name"] or row["name"] or f"#{row['vnum']}"
    row["ai_flag"] = common.text_of(row.get("ai_flag")) or ""
    row["flags"] = flags_of(row["ai_flag"])
    row["aggressive"] = "AGGR" in row["flags"]
    row["kind"], row["kind_icon"], row["kind_name"] = kind_of(row)
    row["rank_name"] = RANKS.get(int(row.get("rank") or 0), str(row.get("rank")))
    row["type_name"] = TYPES.get(int(row.get("type") or 0), str(row.get("type")))
    return row


def history_formatter(col, value):
    if col == "rank":
        return f"{RANKS.get(int(value), value)} [{value}]"
    if col == "battle_type":  # MT2009_PLUS_DB_EDITOR_MOB_MELEE_V1
        return f"{BATTLE_TYPES.get(int(value), value)} [{value}]"
    if col == "ai_flag":
        return ", ".join(sorted(flags_of(value))) or "(brak flag)"
    return None


def search_where(args, map_vnums=None):
    """(where, params, filters) for the search / mass change form."""
    filters = {"q": (args.get("q") or "").strip(), "ranga": args.get("ranga", ""),
               "rodzaj": args.get("rodzaj") or "walka", "lvl_od": (args.get("lvl_od") or "").strip(),
               "lvl_do": (args.get("lvl_do") or "").strip(), "mapa": (args.get("mapa") or "").strip()}
    if filters["rodzaj"] not in KINDS:
        filters["rodzaj"] = "walka"
    where, params = [], []
    query = filters["q"]
    if query:
        if query.isdigit():
            where.append("(vnum=%s OR locale_name LIKE %s)")
            params += [int(query), f"%{query}%"]
        else:
            where.append("locale_name LIKE %s")
            params.append(f"%{query}%")
    if filters["ranga"].isdigit() and int(filters["ranga"]) in RANKS:
        where.append("`rank`=%s")
        params.append(int(filters["ranga"]))
    else:
        filters["ranga"] = ""
    for key, op in (("lvl_od", ">="), ("lvl_do", "<=")):
        if filters[key].isdigit():
            where.append(f"level{op}%s")
            params.append(int(filters[key]))
        else:
            filters[key] = ""
    if map_vnums is not None:
        vnums = sorted(map_vnums)[:5000] or [-1]
        where.append(f"vnum IN ({','.join(['%s'] * len(vnums))})")
        params += vnums
    kind_sql = KINDS[filters["rodzaj"]][1]
    searched = bool(where) or filters["rodzaj"] != "walka"
    if kind_sql:
        where.append(kind_sql)
    return (" AND ".join(where) or "1=1"), params, filters, searched


# ---- mass change -------------------------------------------------------------

def parse_mass(args):
    """(field, op, value, error) of the mass change form."""
    field = args.get("pole") or "max_hp"
    op = args.get("operacja") or "mul"
    raw = (args.get("wartosc") or "").strip().replace(",", ".").replace("×", "").replace("x", "")
    if field not in MASS_FIELDS:
        return field, op, None, "Wybierz, co zmienić."
    if op not in MASS_OPS:
        return field, op, None, "Wybierz rodzaj zmiany."
    if not raw:
        return field, op, None, None
    try:
        value = float(raw) if op == "mul" else int(raw)
    except ValueError:
        return field, op, None, f"„{args.get('wartosc')}” nie jest liczbą" + (" (np. 2 albo 1,5)." if op == "mul" else " całkowitą.")
    if op == "mul" and not 0.01 <= value <= 100:
        return field, op, None, "Mnożnik musi być od 0,01 do 100 (np. 2 = dwa razy więcej, 0,5 = o połowę mniej)."
    return field, op, value, None


def mass_value(op, value, current):
    if op == "mul":
        return int(round(int(current or 0) * value))
    if op == "add":
        return int(current or 0) + int(value)
    return int(value)


def mass_plan(rows, field, op, value):
    """[(row, {col: (old, new)}, [notes])] - the new values clamped to what
    the column (and the game) takes; min/max pairs kept in order."""
    cols = MASS_FIELDS[field][1]
    plan = []
    for row in rows:
        changes, notes = {}, []
        for col in cols:
            spec = SPECS[col]
            new = mass_value(op, value, row.get(col))
            if new < spec["min"] or new > spec["max"]:
                clamped = max(spec["min"], min(spec["max"], new))
                notes.append(f"{spec['label']}: {new} poza zakresem – ustawiono {clamped}")
                new = clamped
            changes[col] = (int(row.get(col) or 0), new)
        if len(cols) == 2 and changes[cols[0]][1] > changes[cols[1]][1]:
            low = changes[cols[1]][1]
            changes[cols[0]] = (changes[cols[0]][0], low)
            notes.append("minimum nie może przekroczyć maksimum – wyrównano")
        if any(old != new for old, new in changes.values()):
            plan.append((row, changes, notes))
    return plan


# ---- ranged -> melee (MT2009_PLUS_DB_EDITOR_MOB_MELEE_V1) -----------------------

def melee_plan(rows):
    """[(row, {col: (old, new)}, boot_lost)] for the rows MELEE_WHERE found:
    the fields that change, and the ones apply.sh would put back at the next
    start (none today - the warning keeps that visible if it ever changes)."""
    plan = []
    for row in rows:
        changes = {col: (int(row.get(col) or 0), new) for col, new in MELEE_VALUES.items()
                   if int(row.get(col) or 0) != new}
        if changes:
            plan.append((row, changes, sorted(set(changes) & boot_cols(row["vnum"]))))
    return plan


def melee_batches(rows_fn, limit=20):
    """The editor's earlier "ranged -> melee" saves, newest first, with the
    state of each (undone / waiting for a restart / in the game)."""
    common.ensure_table()
    return rows_fn(f"""SELECT batch, MIN(id) AS first_id, MIN(changed_at) AS changed_at, MIN(who) AS who,
            COUNT(*) AS fields, COUNT(DISTINCT row_key) AS row_count, SUM(reverted_in IS NULL) AS live,
            SUM(applied_at IS NULL) AS unapplied
        FROM {common.HISTORY_TABLE} WHERE tbl=%s AND note=%s GROUP BY batch ORDER BY first_id DESC
        LIMIT {int(limit)}""", (TABLE, MELEE_NOTE))


def mass_description(field, op, value):
    label = MASS_FIELDS[field][0]
    if op == "mul":
        return f"{label} ×{('%g' % value).replace('.', ',')}"
    if op == "add":
        return f"{label} {int(value):+d}"
    return f"{label} = {int(value)}"


# ---- the pages ---------------------------------------------------------------

def install(bp, ctx):
    import dbeditor
    from dbeditor import spawnfiles

    common.init(ctx)
    common.register_table(TABLE, "vnum", ALL_SPECS, "Potwór", "dbeditor.mob_edit", history_formatter)
    common.install_history(bp, ctx)
    login_required = ctx["login_required"]

    def rows(sql, params=()):
        # looked up per call, so the panel's helpers can be swapped (tests)
        return common.ctx()["rows"](sql, params)

    def page_context():
        try:
            pending = common.pending_context()
        except Exception:
            pending = {"pending_total": 0}
        try:
            pending["pending_spawns"] = len(spawnfiles.pending_changes(spawnfiles.spool_dir()))
        except Exception:
            pending["pending_spawns"] = 0
        return pending

    def spawn_index():
        try:
            return spawnfiles.SpawnIndex.load(spawnfiles.spool_dir())
        except Exception:
            return None

    def load(vnum):
        found = rows(select_sql("vnum=%s"), (vnum,))
        return decorate(found[0]) if found else None

    def map_choices(index):
        if not index:
            return []
        return [(idx, index.map_title(idx), len(vnums)) for idx, vnums in sorted(index.mobs_by_map().items())
                if vnums]

    def filtered_rows(args, limit, force=False):
        index = spawn_index()
        map_vnums = None
        mapa = (args.get("mapa") or "").strip()
        if mapa.isdigit() and index:
            map_vnums = index.mobs_by_map().get(int(mapa), set())
        where, params, filters, searched = search_where(args, map_vnums)
        if map_vnums is None:
            filters["mapa"] = ""
        else:
            searched = True
        searched = searched or force
        found = rows(select_sql(where, limit=limit + 1), params) if searched else []
        return [decorate(r) for r in found[:limit]], len(found) > limit, filters, searched, index

    @bp.route("/mobs")
    @login_required
    def mobs():
        results, more, filters, searched, index = filtered_rows(request.args, LIMIT)
        where_map = {}
        if index and results:
            for row in results:
                where_map[row["vnum"]] = index.maps_of_mob(row["vnum"])[:4]
        return render_template("dbeditor/mobs.html", results=results, more=more, limit=LIMIT, f=filters,
                               searched=searched, ranks=RANKS, kinds=KINDS, maps=map_choices(index),
                               has_index=bool(index), where_map=where_map,
                               map_title=index.map_title if index else str, **page_context())

    @bp.route("/mobs/<int:vnum>", methods=["GET", "POST"])
    @login_required
    def mob_edit(vnum):
        mob = load(vnum)
        if not mob:
            abort(404)
        raw = {}
        if request.method == "POST":
            if not common.check_csrf():
                return redirect(url_for("dbeditor.mob_edit", vnum=vnum))
            new, errors = {}, []
            for col in EDIT_COLS:
                if col == "ai_flag":
                    if "ai_flag_sent" not in request.form:
                        continue
                    value, error = common.validate(SPECS[col], request.form.getlist("ai_flag"), SPECS[col]["label"])
                elif col in request.form:
                    raw[col] = request.form.get(col, "")
                    value, error = common.validate(SPECS[col], raw[col], SPECS[col]["label"])
                else:
                    continue
                if error:
                    errors.append(error)
                else:
                    new[col] = value
            for low, high, label in (("damage_min", "damage_max", "Obrażenia"), ("gold_min", "gold_max", "Yang")):
                if low in new and high in new and new[low] > new[high]:
                    errors.append(f"{label}: minimum ({new[low]}) jest większe niż maksimum ({new[high]}).")
            if errors:
                for error in errors[:8]:
                    flash(error, "error")
                flash("Nic nie zapisano – popraw zaznaczone pola.", "error")
            else:
                try:
                    _batch, saved = common.save_rows(TABLE, [(vnum, new)], note="Edycja potwora",
                                                     label_of=lambda key: mob["display_name"])
                except Exception as exc:
                    flash(f"Nie udało się zapisać: {exc}", "error")
                    return redirect(url_for("dbeditor.mob_edit", vnum=vnum))
                if not saved:
                    flash("Brak zmian do zapisania.", "success")
                else:
                    lost = sorted({col for _k, col, *_r in saved if col in boot_cols(vnum)})
                    if lost:
                        flash(f"{BOOT_WARNING}: {', '.join(SPECS[c]['label'] for c in lost)}.", "warning")
                    flash(f"Zapisano {len(saved)} zmian(y). Zmiany czekają na zastosowanie (restart gry – „Zastosuj”).",
                          "success")
                return redirect(url_for("dbeditor.mob_edit", vnum=vnum))
        try:
            history = common.history_batches(10, TABLE, vnum)
        except Exception:
            history = []
        index = spawn_index()
        spawns = index.spawns_of_mob(vnum) if index else []
        return render_template("dbeditor/mobs_edit.html", mob=mob, raw=raw, specs=ALL_SPECS, ranks=RANKS,
                               resists=RESISTS, ai_flags=AI_FLAGS, ai_labels=AI_FLAG_LABELS, history=history,
                               boot_notes=boot_notes(vnum), boot_always=sorted(boot_cols(vnum)),
                               boot_warning=BOOT_WARNING, spawns=spawns[:30], has_index=bool(index),
                               map_title=index.map_title if index else str,
                               dbe_csrf=common.csrf_token(), **common.template_helpers(), **page_context())

    @bp.route("/mobs/masowo", methods=["GET", "POST"])
    @login_required
    def mobs_mass():
        source = request.form if request.method == "POST" else request.args
        field, op, value, error = parse_mass(source)
        results, more, filters, searched, index = filtered_rows(
            source, MASS_MAX_ROWS, force=request.method == "POST" or "podglad" in source)
        plan = []
        if searched and value is not None and not error and not more:
            plan = mass_plan(results, field, op, value)
        if request.method == "POST":
            back = url_for("dbeditor.mobs_mass", podglad=1,
                           **{k: v for k, v in source.items() if k not in ("dbe_csrf", "vnums", "podglad")})
            if not common.check_csrf():
                return redirect(back)
            if error or value is None:
                flash(error or "Podaj wartość zmiany.", "error")
                return redirect(back)
            if more:
                flash(f"Filtr obejmuje ponad {MASS_MAX_ROWS} potworów – zawęź go.", "error")
                return redirect(back)
            previewed = {int(v) for v in (request.form.get("vnums") or "").split(",") if v.strip().isdigit()}
            planned = {int(row["vnum"]) for row, _c, _n in plan}
            if not plan or previewed != planned:
                flash("Lista potworów zmieniła się od podglądu – sprawdź podgląd jeszcze raz i zatwierdź ponownie.", "error")
                return redirect(back)
            description = mass_description(field, op, value)
            names = {int(row["vnum"]): row["display_name"] for row, _c, _n in plan}
            updates = [(int(row["vnum"]), {col: new for col, (_old, new) in changes.items()}) for row, changes, _n in plan]
            try:
                batch, saved = common.save_rows(TABLE, updates, note=f"Masowo: {description}",
                                                label_of=lambda key: names.get(int(key), ""))
            except Exception as exc:
                flash(f"Nie udało się zapisać: {exc}", "error")
                return redirect(back)
            lost = sorted({int(k) for k, col, *_r in saved if col in boot_cols(k)})
            if lost:
                flash(f"{BOOT_WARNING} – dotyczy {len(lost)} potworów (np. {', '.join(str(v) for v in lost[:6])}).",
                      "warning")
            flash(f"Zmieniono {len({k for k, *_r in saved})} potworów ({description}). Cofniesz to jednym kliknięciem "
                  "w historii zmian. Zmiany czekają na zastosowanie (restart gry – „Zastosuj”).", "success")
            return redirect(url_for("dbeditor.proto_history", tbl=TABLE))
        boot_hits = sum(1 for row, changes, _n in plan if set(changes) & boot_cols(row["vnum"]))
        return render_template("dbeditor/mobs_mass.html", f=filters, field=field, op=op, value=value, error=error,
                               raw_value=source.get("wartosc", ""), searched=searched, more=more,
                               matched=len(results), plan=plan[:MASS_PREVIEW_ROWS], plan_total=len(plan),
                               plan_vnums=",".join(str(row["vnum"]) for row, _c, _n in plan),
                               fields=MASS_FIELDS, ops=MASS_OPS, ranks=RANKS, kinds=KINDS, maps=map_choices(index),
                               specs=SPECS, max_rows=MASS_MAX_ROWS, boot_hits=boot_hits,
                               description=mass_description(field, op, value) if value is not None else "",
                               dbe_csrf=common.csrf_token(), **page_context())

    @bp.route("/mobs/wrecz", methods=["GET", "POST"])
    @login_required
    def mobs_melee():
        """MT2009_PLUS_DB_EDITOR_MOB_MELEE_V1: preview, then one save (one
        history batch) of every ordinary ranged monster turned melee."""
        try:
            found = [decorate(r) for r in rows(select_sql(MELEE_WHERE, order="level, vnum"))]
            error = None
        except Exception as exc:  # pymysql errors: show, do not 500
            found, error = [], str(exc)
        plan = melee_plan(found)
        if request.method == "POST":
            back = url_for("dbeditor.mobs_melee")
            if not common.check_csrf():
                return redirect(back)
            previewed = {int(v) for v in (request.form.get("vnums") or "").split(",") if v.strip().isdigit()}
            planned = {int(row["vnum"]) for row, _c, _b in plan}
            if not plan:
                flash("Nie ma już zwykłych potworów walczących z dystansu – nic do zmiany.", "error")
                return redirect(back)
            if previewed != planned:
                flash("Lista potworów zmieniła się od podglądu – sprawdź podgląd jeszcze raz i zatwierdź ponownie.", "error")
                return redirect(back)
            names = {int(row["vnum"]): row["display_name"] for row, _c, _b in plan}
            updates = [(int(row["vnum"]), {col: new for col, (_old, new) in changes.items()}) for row, changes, _b in plan]
            try:
                _batch, saved = common.save_rows(TABLE, updates, note=MELEE_NOTE,
                                                 label_of=lambda key: names.get(int(key), ""))
            except Exception as exc:
                flash(f"Nie udało się zapisać: {exc}", "error")
                return redirect(back)
            lost = sorted({int(k) for k, col, *_r in saved if col in boot_cols(k)})
            if lost:
                flash(f"{BOOT_WARNING} – dotyczy {len(lost)} potworów (np. {', '.join(str(v) for v in lost[:6])}).",
                      "warning")
            flash(f"{len({k for k, *_r in saved})} potworów walczy teraz wręcz (zasięg ataku "
                  f"{MELEE_VALUES['attack_range']}). Cofniesz to jednym kliknięciem niżej albo w historii. "
                  "Gra wczyta zmianę dopiero po restarcie – kliknij „Zastosuj”.", "success")
            return redirect(back)
        try:
            done = melee_batches(rows)
        except Exception:
            done = []
        return render_template("dbeditor/mobs_melee.html", plan=plan, error=error, done=done,
                               plan_vnums=",".join(str(row["vnum"]) for row, _c, _b in plan),
                               boot_hits=sum(1 for _r, _c, lost in plan if lost), values=MELEE_VALUES,
                               battle_types=BATTLE_TYPES, where=MELEE_WHERE,
                               dbe_csrf=common.csrf_token(), **page_context())

    dbeditor.add_section("dbeditor.mobs", "👹", "Potwory i bossowie",
                         "PŻ, obrażenia, obrona, poziom, exp i yang, agresja, odporności – także masowo (np. ×2 PŻ bossów)")
