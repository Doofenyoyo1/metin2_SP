"""MT2009_PLUS_DB_EDITOR_V1: "Bonusy do zmiany i 6/7" - world.item_attr (the
five normal bonuses an item gets from "Dodaj bonus" / "Zmień bonus" and from
drops) and world.item_attr_rare (the 6th/7th bonus: Seon-Hae, 71051) for an
operator who is not a programmer.

How the engine reads them (db/src/ClientManagerBoot.cpp InitializeItemAttr-
Table / InitializeItemRareTable, once at boot):

  SELECT apply, apply+0, prob, lv1..lv5, weapon, body, wrist, foots, neck,
         head, shield, ear, costume_body, costume_hair, costume_weapon, pendant
  (glove is not read: ENABLE_GLOVE_SYSTEM is off)

  * apply is an ENUM of POINT_* names; apply+0 (its 1-based position) is the
    number the item keeps - in this engine the POINT_* number (common/length.h
    EPointTypes; 'POINT_ATTBONUS_SAVAGE' is the engine's 148,
    POINT_ATTBONUS_ORC_VALLEY). items.POINT_LABELS names them in Polish.
    Points above the ENUM (162+, e.g. POINT_FIRE_PCT) cannot be a row.
  * prob is a weight: a new bonus is drawn among the rows the item's kind
    allows, by weight (item_attribute.cpp PutAttributeWithLevel; the 6th/7th
    by PutRareAttributeWithLevel through AddRareAttribute2 - Seon-Hae and
    71051).
  * the kind columns are the HIGHEST LEVEL (0-5) the bonus may reach on that
    kind of equipment; 0 = it never rolls there. The level itself is drawn
    by the scroll/system, then capped to that column; the value is lvN.
  * a value is stored as a short on the item: 0..32767; a level whose value
    is 0 adds nothing (CItem::AddAttr).

Every save goes through common_items (history with undo, "pending" until
Zastosuj restarts the cores). apply.sh rewrites (see WARN_* below):
  * MT2009_PLUS_COSTUME_BONUS_V1 - item_attr's costume_body/hair/weapon
    are copied from body/head/weapon at every start WHILE all three are zero
    in the whole table;
  * MT2009_PLUS_RARE_TABLE_V2 - item_attr_rare is written once per world
    (marker rare_6_7_v2 in world.mt2009_plus_once); later edits stay.
"""
import re

from flask import abort, flash, redirect, render_template, request, url_for

from dbeditor import common_items as common
from dbeditor import tables_common
from dbeditor.items import POINT_LABELS, TECHNICAL_POINTS

POOLS = {
    "zwykle": {"table": "world.item_attr", "title": "Bonusy 1–5 (zwykłe)", "short": "bonus 1–5",
               "endpoint": "dbeditor.attr_edit_normal", "need": 5, "prob_max": 2147483647},
    "rzadkie": {"table": "world.item_attr_rare", "title": "Bonusy 6 i 7 (Seon-Hae, 71051)", "short": "bonus 6/7",
                "endpoint": "dbeditor.attr_edit_rare", "need": 2, "prob_max": 4294967295},
}
# (column, Polish name, icon). The order the pages show them in.
KINDS = [
    ("weapon", "Broń", "⚔️"), ("body", "Zbroja", "🛡️"), ("head", "Hełm", "⛑️"), ("shield", "Tarcza", "🔰"),
    ("wrist", "Bransoleta", "📿"), ("foots", "Buty", "👢"), ("neck", "Naszyjnik", "📿"), ("ear", "Kolczyki", "💎"),
    ("costume_body", "Kostium", "👘"), ("costume_hair", "Fryzura", "💇"), ("costume_weapon", "Kostium broni", "🗡️"),
    ("pendant", "Talizman", "🧿"),
]
KIND_NAMES = {col: name for col, name, _icon in KINDS}
KIND_WHERE = {"weapon": "broni", "body": "zbroi", "head": "hełmie", "shield": "tarczy", "wrist": "bransolecie",
              "foots": "butach", "neck": "naszyjniku", "ear": "kolczykach", "costume_body": "kostiumie",
              "costume_hair": "fryzurze", "costume_weapon": "kostiumie broni", "pendant": "talizmanie"}
KIND_COLS = [col for col, _n, _i in KINDS]
COSTUME_COLS = ("costume_body", "costume_hair", "costume_weapon")
LEVELS = ("lv1", "lv2", "lv3", "lv4", "lv5")
VALUE_MAX = 32767  # TPlayerItemAttribute.sValue is a short

# The ENUM of world.item_attr.apply in this world (161 names, position = the
# POINT_* number); used when information_schema cannot be read.
POINT_NAMES = (
    "POINT_LEVEL POINT_VOICE POINT_EXP POINT_NEXT_EXP POINT_HP POINT_MAX_HP POINT_SP POINT_MAX_SP POINT_STAMINA "
    "POINT_MAX_STAMINA POINT_GOLD POINT_ST POINT_HT POINT_DX POINT_IQ POINT_DEF_GRADE POINT_ATT_SPEED POINT_ATT_GRADE "
    "POINT_MOV_SPEED POINT_CLIENT_DEF_GRADE POINT_CASTING_SPEED POINT_MAGIC_ATT_GRADE POINT_MAGIC_DEF_GRADE "
    "POINT_EMPIRE_POINT POINT_LEVEL_STEP POINT_STAT POINT_SUB_SKILL POINT_SKILL POINT_WEAPON_MIN POINT_WEAPON_MAX "
    "POINT_PLAYTIME POINT_HP_REGEN POINT_SP_REGEN POINT_BOW_DISTANCE POINT_HP_RECOVERY POINT_SP_RECOVERY "
    "POINT_POISON_PCT POINT_STUN_PCT POINT_SLOW_PCT POINT_CRITICAL_PCT POINT_PENETRATE_PCT POINT_CURSE_PCT "
    "POINT_ATTBONUS_HUMAN POINT_ATTBONUS_ANIMAL POINT_ATTBONUS_ORC POINT_ATTBONUS_MILGYO POINT_ATTBONUS_UNDEAD "
    "POINT_ATTBONUS_DEVIL POINT_ATTBONUS_INSECT POINT_ATTBONUS_FIRE POINT_ATTBONUS_ICE POINT_ATTBONUS_DESERT "
    "POINT_ATTBONUS_MONSTER POINT_ATTBONUS_WARRIOR POINT_ATTBONUS_ASSASSIN POINT_ATTBONUS_SURA POINT_ATTBONUS_SHAMAN "
    "POINT_ATTBONUS_TREE POINT_RESIST_WARRIOR POINT_RESIST_ASSASSIN POINT_RESIST_SURA POINT_RESIST_SHAMAN "
    "POINT_STEAL_HP POINT_STEAL_SP POINT_MANA_BURN_PCT POINT_DAMAGE_SP_RECOVER POINT_BLOCK POINT_DODGE "
    "POINT_RESIST_SWORD POINT_RESIST_TWOHAND POINT_RESIST_DAGGER POINT_RESIST_BELL POINT_RESIST_FAN POINT_RESIST_BOW "
    "POINT_RESIST_FIRE POINT_RESIST_ELEC POINT_RESIST_MAGIC POINT_RESIST_WIND POINT_REFLECT_MELEE POINT_REFLECT_ARROW "
    "POINT_POISON_REDUCE POINT_KILL_SP_RECOVER POINT_EXP_DOUBLE_BONUS POINT_GOLD_DOUBLE_BONUS POINT_ITEM_DROP_BONUS "
    "POINT_POTION_BONUS POINT_KILL_HP_RECOVERY POINT_IMMUNE_STUN POINT_IMMUNE_SLOW POINT_IMMUNE_FALL "
    "POINT_PARTY_ATTACKER_BONUS POINT_PARTY_TANKER_BONUS POINT_ATT_BONUS POINT_DEF_BONUS POINT_ATT_GRADE_BONUS "
    "POINT_DEF_GRADE_BONUS POINT_MAGIC_ATT_GRADE_BONUS POINT_MAGIC_DEF_GRADE_BONUS POINT_RESIST_NORMAL_DAMAGE "
    "POINT_HIT_HP_RECOVERY POINT_HIT_SP_RECOVERY POINT_MANASHIELD POINT_PARTY_BUFFER_BONUS "
    "POINT_PARTY_SKILL_MASTER_BONUS POINT_HP_RECOVER_CONTINUE POINT_SP_RECOVER_CONTINUE POINT_STEAL_GOLD "
    "POINT_POLYMORPH POINT_MOUNT POINT_PARTY_HASTE_BONUS POINT_PARTY_DEFENDER_BONUS POINT_STAT_RESET_COUNT "
    "POINT_HORSE_SKILL POINT_MALL_ATTBONUS POINT_MALL_DEFBONUS POINT_MALL_EXPBONUS POINT_MALL_ITEMBONUS "
    "POINT_MALL_GOLDBONUS POINT_MAX_HP_PCT POINT_MAX_SP_PCT POINT_SKILL_DAMAGE_BONUS POINT_NORMAL_HIT_DAMAGE_BONUS "
    "POINT_SKILL_DEFEND_BONUS POINT_NORMAL_HIT_DEFEND_BONUS POINT_PC_BANG_EXP_BONUS POINT_PC_BANG_DROP_BONUS "
    "POINT_RAMADAN_CANDY_BONUS_EXP POINT_ENERGY POINT_ENERGY_END_TIME POINT_COSTUME_ATTR_BONUS "
    "POINT_MAGIC_ATT_BONUS_PER POINT_MELEE_MAGIC_ATT_BONUS_PER POINT_RESIST_ICE POINT_RESIST_EARTH POINT_RESIST_DARK "
    "POINT_RESIST_CRITICAL POINT_RESIST_PENETRATE POINT_TERROR POINT_ST_REGEN POINT_DAGGER_ATT_GRADE_MONSTER "
    "POINT_ATT_GRADE_MONSTER POINT_RESIST_MONSTER_1000PCT POINT_ABSORB_DAMAGE POINT_ABSORB_DAMAGE_MONSTER "
    "POINT_IMMUNE_STUN_BREAK POINT_BREAK_TEMPLE_CURSE POINT_SKILL_DURATION POINT_ATTBONUS_SAVAGE POINT_ATTBONUS_STONE "
    "POINT_ATTBONUS_BOSS POINT_MAGIC_ATT_MONSTER POINT_BREAK_RESIST_SWORD POINT_BREAK_RESIST_TWOHAND "
    "POINT_BREAK_RESIST_DAGGER POINT_BREAK_RESIST_BELL POINT_BREAK_RESIST_FAN POINT_BREAK_RESIST_BOW "
    "POINT_COLLECT_CHANCE POINT_LEARN_CHANCE POINT_RESIST_HUMAN POINT_MAGIC_ATT"
).split()

WARN_NORMAL = ("apply.sh (MT2009_PLUS_COSTUME_BONUS_V1) przy każdym starcie serwera sprawdza kolumny Kostium, "
               "Fryzura i Kostium broni: jeśli w CAŁEJ tabeli wszystkie są zerowe, kopiuje do nich Zbroję, Hełm i "
               "Broń. Dopóki choć jeden bonus ma kostium większy od zera, Twoje zmiany zostają.")
WARN_RARE = ("apply.sh (MT2009_PLUS_RARE_TABLE_V2) wpisuje tę tabelę tylko raz na świat (znacznik rare_6_7_v2 "
             "w world.mt2009_plus_once) – Twoje zmiany zostają po restartach. Tylko nowa baza bez tego znacznika "
             "dostanie z powrotem tabelę z 1 października.")
WARN_COSTUME_RESET = ("Uwaga: wszystkie kolumny kostiumów w tabeli są teraz zerowe – przy następnym starcie serwera "
                      "apply.sh skopiuje do nich wartości ze Zbroi, Hełmu i Broni.")


def _int(low, high, label):
    return {"kind": "int", "min": low, "max": high, "label": label}


def specs_for(pool):
    specs = {"prob": _int(0, POOLS[pool]["prob_max"], "Waga (szansa wylosowania)")}
    for n, col in enumerate(LEVELS, 1):
        specs[col] = _int(0, VALUE_MAX, f"Wartość na poziomie {n}")
    for col in KIND_COLS:
        specs[col] = _int(0, 5, f"{KIND_NAMES[col]} – najwyższy poziom")
    return specs


def bonus_name(point):
    name, unit = POINT_LABELS.get(int(point or 0), (f"Nieznany bonus #{point}", ""))
    return name, unit


def history_formatter(col, value):
    if col in KIND_NAMES:
        level = int(value)
        return "nie losuje się" if level == 0 else f"do poziomu {level}"
    return None


def pct(value, total):
    return 100.0 * value / total if total else 0.0


def fmt_pct(value):
    if value <= 0:
        return "0%"
    text = f"{value:.1f}" if value >= 1 else f"{value:.2f}"
    return text.rstrip("0").rstrip(".").replace(".", ",") + "%"


def decorate(row, names):
    """A table row as the pages use it."""
    apply = row["apply"].decode() if isinstance(row["apply"], bytes) else str(row["apply"])
    point = names.index(apply) + 1 if apply in names else 0
    name, unit = bonus_name(point)
    return {"apply": apply, "point": point, "name": name, "unit": unit, "prob": int(row["prob"] or 0),
            "lv": [int(row[c] or 0) for c in LEVELS], "kinds": {c: int(row.get(c) or 0) for c in KIND_COLS}}


def kind_view(bonuses, col, need):
    """What can roll on one kind of equipment: [{bonus, chance, level,
    max_value}] by chance, and the warnings for that kind."""
    allowed = [b for b in bonuses if b["kinds"][col] > 0]
    total = sum(b["prob"] for b in allowed)
    entries = []
    for b in allowed:
        level = min(5, b["kinds"][col])
        entries.append({"bonus": b, "chance": pct(b["prob"], total), "level": level,
                        "max_value": b["lv"][level - 1], "zero": b["lv"][level - 1] == 0})
    entries.sort(key=lambda e: (-e["chance"], e["bonus"]["name"]))
    warnings = []
    if not allowed:
        warnings.append("Na tym rodzaju nie wylosuje się żaden bonus.")
    elif total <= 0:
        warnings.append("Wszystkie bonusy mają wagę 0 – gra nie wylosuje żadnego.")
    elif len([b for b in allowed if b["prob"] > 0]) < need:
        warnings.append(f"Tylko {len([b for b in allowed if b['prob'] > 0])} bonusów z wagą > 0 – przedmiot nie "
                        f"dostanie {need} różnych bonusów (ostatnie się nie wylosują).")
    for e in entries:
        if e["zero"]:
            warnings.append(f"{e['bonus']['name']}: wartość na poziomie {e['level']} to 0 – taki bonus nic nie doda.")
    return {"col": col, "name": KIND_NAMES[col], "where": KIND_WHERE[col], "entries": entries, "total": total, "warnings": warnings}


def install(bp, ctx):
    import dbeditor
    common.init(ctx)
    for pool, meta in POOLS.items():
        common.register_table(meta["table"], "apply", specs_for(pool), meta["short"].capitalize(), meta["endpoint"],
                              history_formatter)
    common.install_history(bp, ctx)
    tables_common.register(bp)
    login_required = ctx["login_required"]

    def q(sql, params=()):
        return common.ctx()["rows"](sql, params)

    def enum_names(table):
        schema, name = table.split(".")
        try:
            found = q("SELECT COLUMN_TYPE FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=%s AND TABLE_NAME=%s "
                      "AND COLUMN_NAME='apply'", (schema, name))
            names = re.findall(r"'([^']+)'", str((found[0] if found else {}).get("COLUMN_TYPE") or ""))
        except Exception:
            names = []
        return names or list(POINT_NAMES)

    def load(pool):
        table = POOLS[pool]["table"]
        names = enum_names(table)
        cols = ", ".join(f"`{c}`" for c in ("apply", "prob") + LEVELS + tuple(KIND_COLS))
        found = [decorate(r, names) for r in q(f"SELECT {cols} FROM {table}")]
        found.sort(key=lambda b: (b["name"].lower(), b["point"]))
        return found, names

    def pool_or_404(pool):
        if pool not in POOLS:
            abort(404)
        return POOLS[pool]

    def pending_here():
        tables = {m["table"] for m in POOLS.values()}
        try:
            return [c for c in common.pending_changes() if c["tbl"] in tables]
        except Exception:
            return []

    def label_for(pool, names):
        return lambda apply: f"{bonus_name(names.index(apply) + 1 if apply in names else 0)[0]} ({POOLS[pool]['short']})"

    def costume_check(pool):
        if pool != "zwykle":
            return
        try:
            bonuses, _ = load(pool)
        except Exception:
            return
        if bonuses and not any(b["kinds"][c] for b in bonuses for c in COSTUME_COLS):
            flash(WARN_COSTUME_RESET, "warning")

    def save(pool, apply, values, note, names):
        meta = POOLS[pool]
        _batch, changed = common.save_rows(meta["table"], [(apply, values)], note=note,
                                           label_of=label_for(pool, names))
        return changed

    @bp.route("/attrs")
    @login_required
    def attrs():
        pool = request.args.get("pula", "zwykle")
        meta = pool_or_404(pool)
        error = None
        try:
            bonuses, names = load(pool)
        except Exception as exc:  # pymysql errors: show, do not 500
            bonuses, names, error = [], list(POINT_NAMES), str(exc)
        kinds = [dict(kind_view(bonuses, col, meta["need"]), icon=icon) for col, _n, icon in KINDS]
        present = {b["apply"] for b in bonuses}
        addable = []
        for number, name in enumerate(names, 1):
            if name not in present and number in POINT_LABELS and number not in TECHNICAL_POINTS:
                addable.append({"apply": name, "point": number, "name": bonus_name(number)[0]})
        addable.sort(key=lambda a: a["name"].lower())
        return render_template("dbeditor/attrs.html", pool=pool, meta=meta, pools=POOLS, bonuses=bonuses,
                               kinds=kinds, kind_list=KINDS, addable=addable, error=error, fmt_pct=fmt_pct,
                               warning=WARN_NORMAL if pool == "zwykle" else WARN_RARE, pending=pending_here(),
                               dbe_csrf=common.csrf_token())

    def edit_view(pool, vnum):
        meta = pool_or_404(pool)
        try:
            bonuses, names = load(pool)
        except Exception as exc:
            flash(f"Nie udało się odczytać tabeli: {exc}", "error")
            return redirect(url_for("dbeditor.attrs", pula=pool))
        bonus = next((b for b in bonuses if b["apply"] == vnum), None)
        if bonus is None:
            abort(404)
        specs = specs_for(pool)
        if request.method == "POST":
            if not common.check_csrf():
                return redirect(request.full_path)
            if request.form.get("action") == "clear":
                values = {c: 0 for c in KIND_COLS}
            else:
                values, errors = {}, []
                for col in ("prob",) + LEVELS + tuple(KIND_COLS):
                    if col not in request.form:
                        continue
                    value, error = common.validate(specs[col], request.form.get(col), specs[col]["label"])
                    if error:
                        errors.append(error)
                    else:
                        values[col] = value
                if errors:
                    for error in errors:
                        flash(error, "error")
                    return redirect(url_for(meta["endpoint"], vnum=vnum))
            try:
                changed = save(pool, vnum, values, request.form.get("note", "")[:200] or
                               f"{bonus['name']}: zmiana w edytorze bonusów", names)
            except Exception as exc:
                flash(f"Nie zapisano: {exc}", "error")
                return redirect(url_for(meta["endpoint"], vnum=vnum))
            if changed:
                flash(f"Zapisano {len(changed)} zmian(y). Czekają na zastosowanie (restart gry).", "success")
                costume_check(pool)
            else:
                flash("Nic się nie zmieniło.", "info")
            return redirect(url_for(meta["endpoint"], vnum=vnum))
        kinds = []
        for col, _name, icon in KINDS:
            view = kind_view(bonuses, col, meta["need"])
            entry = next((e for e in view["entries"] if e["bonus"]["apply"] == vnum), None)
            kinds.append({"col": col, "name": KIND_NAMES[col], "icon": icon, "level": bonus["kinds"][col],
                          "chance": entry["chance"] if entry else 0.0, "count": len(view["entries"])})
        try:
            history = common.history_batches(10, meta["table"], vnum)
        except Exception:
            history = []
        return render_template("dbeditor/attrs_edit.html", pool=pool, meta=meta, bonus=bonus, kinds=kinds,
                               fmt_pct=fmt_pct, history=history, warning=WARN_NORMAL if pool == "zwykle" else WARN_RARE,
                               value_max=VALUE_MAX, dbe_csrf=common.csrf_token(), **common.template_helpers())

    def edit_normal(vnum):
        return edit_view("zwykle", vnum)

    def edit_rare(vnum):
        return edit_view("rzadkie", vnum)

    bp.add_url_rule("/attrs/zwykle/<vnum>", "attr_edit_normal", login_required(edit_normal), methods=["GET", "POST"])
    bp.add_url_rule("/attrs/rzadkie/<vnum>", "attr_edit_rare", login_required(edit_rare), methods=["GET", "POST"])

    @bp.post("/attrs/rodzaj/<pool>")
    @login_required
    def attrs_kind(pool):
        """Add a bonus to one kind of equipment (with its highest level) or
        take it away (level 0)."""
        pool_or_404(pool)
        back = url_for("dbeditor.attrs", pula=pool) + "#k-" + request.form.get("kind", "")
        if not common.check_csrf():
            return redirect(back)
        kind = request.form.get("kind", "")
        apply = request.form.get("apply", "")
        if kind not in KIND_NAMES:
            flash("Nieznany rodzaj ekwipunku.", "error")
            return redirect(back)
        level, error = common.validate(specs_for(pool)[kind], request.form.get("level", "0"), "Poziom")
        if error:
            flash(error, "error")
            return redirect(back)
        try:
            bonuses, names = load(pool)
            bonus = next((b for b in bonuses if b["apply"] == apply), None)
            if bonus is None:
                flash("Tego bonusu nie ma w tabeli.", "error")
                return redirect(back)
            what = (f"{bonus['name']} na: {KIND_NAMES[kind]} (do poziomu {level})" if level
                    else f"{bonus['name']} usunięty z: {KIND_NAMES[kind]}")
            changed = save(pool, apply, {kind: level}, what, names)
        except Exception as exc:
            flash(f"Nie zapisano: {exc}", "error")
            return redirect(back)
        if changed:
            flash(what + ". Czeka na zastosowanie (restart gry).", "success")
            if level and bonus["prob"] <= 0:
                flash(f"{bonus['name']} ma wagę 0 – nie wylosuje się, dopóki nie ustawisz wagi.", "warning")
            costume_check(pool)
        else:
            flash("Nic się nie zmieniło.", "info")
        return redirect(back)

    @bp.post("/attrs/nowy/<pool>")
    @login_required
    def attrs_new(pool):
        """A bonus that is not in the table yet: an inert row (weight 0, no
        kind) - harmless for the game - then its edit page, where every value
        is saved with history."""
        meta = pool_or_404(pool)
        if not common.check_csrf():
            return redirect(url_for("dbeditor.attrs", pula=pool))
        apply = request.form.get("apply", "")
        try:
            bonuses, names = load(pool)
        except Exception as exc:
            flash(f"Nie udało się odczytać tabeli: {exc}", "error")
            return redirect(url_for("dbeditor.attrs", pula=pool))
        if apply not in names or (names.index(apply) + 1) in TECHNICAL_POINTS:
            flash("Tego bonusu nie można dodać.", "error")
            return redirect(url_for("dbeditor.attrs", pula=pool))
        if apply not in {b["apply"] for b in bonuses}:
            try:
                q(f"INSERT INTO {meta['table']} (`apply`, `prob`) VALUES (%s, 0)", (apply,))
            except Exception as exc:
                flash(f"Nie dodano: {exc}", "error")
                return redirect(url_for("dbeditor.attrs", pula=pool))
            flash("Dodano bonus do tabeli (na razie bez wagi i bez rodzajów – gra go nie losuje). "
                  "Ustaw wagę, wartości i rodzaje ekwipunku, potem Zapisz.", "success")
        return redirect(url_for(meta["endpoint"], vnum=apply))

    dbeditor.add_section("dbeditor.attrs", "🎲", "Bonusy do zmiany i 6/7",
                         "jakie bonusy losują się na broni, zbroi, biżuterii; wartości 1–5, szanse – i pula 6/7")
