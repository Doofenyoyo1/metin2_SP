"""MT2009_PLUS_DB_EDITOR_V1: "Przedmioty" - world.item_proto for an operator.

The db core loads world.item_proto at boot (PROTO_FROM_DB = 1, game/Dockerfile;
player.item_proto is a view of it), so this edits world.item_proto and the
game sees the change after a restart. The bonus lines (applytype0-2) of this
engine hold POINT_* numbers (common/length.h EPointTypes - there is no
EApplyTypes in mt2009, see playerbot_engine_compat.h), which is the table
POINT_LABELS below names in Polish.

What the values mean (game/src/battle.cpp, char.cpp):
  weapon  value3/value4 attack min/max, value1/value2 magic attack min/max,
          value5 the refine's attack bonus (added to both, counted twice);
  armour  value1 defence, value5 the refine's defence bonus (counted twice) -
          for body, head, shield and shoes only;
  limittype0/1 + limitvalue0/1: 1 = the level needed to wear it.

MT2009_PLUS_ITEM_EXTRA_APPLY_V1: item_proto has room for three bonuses
(applytype0..2 - ITEM_APPLY_MAX_NUM of the engine and aApplies[3] of the
client exe, neither of which can grow). Lines beyond them are rows of
world.item_extra_apply (vnum, slot, apply_type - the same POINT_* number as
applytype -, apply_value), the section "Dodatkowe bonusy (ponad 3)" of an
item's page. The game core wears them like the proto's own after a restart
(playerbot_item_extra_apply.h, server-patches/enginefixes); the client shows
them from gamedata/item_extra_apply.txt of the dbdata zip (m2clientpack/
dbsource.py, uitooltip.py). Saved through common_items.write_rows - one
history row a line, with undo, listed by "Zastosuj" like any other change.
"""
import json
import re

from flask import abort, flash, redirect, render_template, request, url_for

from dbeditor import common_items as common
from dbeditor import protected

TABLE = "world.item_proto"

# POINT number -> (Polish name, unit). Gaps of the client's text are named
# after the engine's constant; "tech" marks the points no item should carry.
POINT_LABELS = {
    0: ("— brak bonusu —", ""), 1: ("Poziom", ""), 2: ("Głos", ""), 3: ("Doświadczenie", ""),
    4: ("Wymagane doświadczenie", ""), 5: ("PŻ", ""), 6: ("Maks. PŻ", ""), 7: ("PM", ""), 8: ("Maks. PM", ""),
    9: ("Wytrzymałość", ""), 10: ("Maks. wytrzymałość", ""), 11: ("Yang", ""), 12: ("Siła", ""),
    13: ("Witalność", ""), 14: ("Zręczność", ""), 15: ("Inteligencja", ""), 16: ("Obrona", ""),
    17: ("Szybkość ataku", "%"), 18: ("Wartość ataku (stała)", ""), 19: ("Szybkość ruchu", "%"),
    20: ("Obrona (klient)", ""), 21: ("Szybkość zaklęcia", "%"), 22: ("Wartość magicznego ataku", ""),
    23: ("Magiczna wartość obrony", ""), 24: ("Punkty królestwa", ""), 25: ("Krok poziomu", ""),
    26: ("Punkty statystyk", ""), 27: ("Punkty umiejętności pobocznych", ""), 28: ("Punkty umiejętności", ""),
    29: ("Atak broni min.", ""), 30: ("Atak broni maks.", ""), 31: ("Czas gry", ""),
    32: ("Regeneracja PŻ", "%"), 33: ("Regeneracja PM", "%"), 34: ("Zasięg łuku", "m"),
    35: ("Odzyskiwanie PŻ", ""), 36: ("Odzyskiwanie PM", ""), 37: ("Szansa na otrucie", "%"),
    38: ("Szansa na omdlenie", "%"), 39: ("Szansa na spowolnienie", "%"), 40: ("Szansa na cios krytyczny", "%"),
    41: ("Szansa na przeszywający", "%"), 42: ("Szansa na klątwę", "%"), 43: ("Silny przeciw ludziom", "%"),
    44: ("Silny przeciw zwierzętom", "%"), 45: ("Silny przeciw orkom", "%"), 46: ("Silny przeciw mistykom", "%"),
    47: ("Silny przeciw nieumarłym", "%"), 48: ("Silny przeciw diabłom", "%"), 49: ("Silny przeciw owadom", "%"),
    50: ("Silny przeciw ogniu", "%"), 51: ("Silny przeciw lodowi", "%"), 52: ("Silny przeciw pustyni", "%"),
    53: ("Silny przeciw potworom", "%"), 54: ("Silny przeciw wojownikom", "%"), 55: ("Silny przeciw ninja", "%"),
    56: ("Silny przeciw surom", "%"), 57: ("Silny przeciw szamanom", "%"), 58: ("Silny przeciw drzewom", "%"),
    59: ("Odporność na wojowników", "%"), 60: ("Odporność na ninja", "%"), 61: ("Odporność na sury", "%"),
    62: ("Odporność na szamanów", "%"), 63: ("Kradzież PŻ", "%"), 64: ("Kradzież PM", "%"),
    65: ("Szansa na kradzież PM", "%"), 66: ("Odzyskanie PM po obrażeniach", "%"), 67: ("Szansa na blok", "%"),
    68: ("Szansa na unik strzał", "%"), 69: ("Odporność na miecze", "%"), 70: ("Odporność na broń dwuręczną", "%"),
    71: ("Odporność na sztylety", "%"), 72: ("Odporność na dzwony", "%"), 73: ("Odporność na wachlarze", "%"),
    74: ("Odporność na strzały", "%"), 75: ("Odporność na ogień", "%"), 76: ("Odporność na błyskawice", "%"),
    77: ("Odporność na magię", "%"), 78: ("Odporność na wiatr", "%"), 79: ("Odbicie obrażeń fizycznych", "%"),
    80: ("Odbicie strzał", "%"), 81: ("Odporność na trucizny", "%"), 82: ("Odzyskanie PM po zabiciu", "%"),
    83: ("Bonus doświadczenia", "%"), 84: ("Bonus Yang", "%"), 85: ("Bonus dropu przedmiotów", "%"),
    86: ("Bonus mikstur", "%"), 87: ("Odzyskanie PŻ po zabiciu", "%"), 88: ("Odporność na omdlenie", ""),
    89: ("Odporność na spowolnienie", ""), 90: ("Odporność na przewrócenie", ""),
    91: ("Premia drużyny: atakujący", ""), 92: ("Premia drużyny: obrońca", ""), 93: ("Wartość ataku", "%"),
    94: ("Wartość obrony", "%"), 95: ("Wartość ataku", ""), 96: ("Wartość obrony", ""),
    97: ("Wartość magicznego ataku (premia)", ""), 98: ("Magiczna wartość obrony (premia)", ""),
    99: ("Odporność na zwykłe ataki", "%"), 100: ("Odzysk PŻ przy trafieniu", "%"), 101: ("Odzysk PM przy trafieniu", "%"),
    102: ("Tarcza many", ""), 103: ("Premia drużyny: wsparcie", ""), 104: ("Premia drużyny: mistrz umiejętności", ""),
    105: ("Ciągłe odzyskiwanie PŻ", ""), 106: ("Ciągłe odzyskiwanie PM", ""), 107: ("Kradzież Yang", "%"),
    108: ("Przemiana", ""), 109: ("Wierzchowiec", ""), 110: ("Premia drużyny: pośpiech", ""),
    111: ("Premia drużyny: obrona", ""), 112: ("Liczba resetów statystyk", ""), 113: ("Umiejętność konia", ""),
    114: ("Wartość ataku (sklep)", "%"), 115: ("Wartość obrony (sklep)", "%"), 116: ("Bonus doświadczenia (sklep)", "%"),
    117: ("Szansa na zdobycie przedmiotów (sklep)", "%"), 118: ("Szansa na zdobycie Yang (sklep)", "%"),
    119: ("Maks. PŻ", "%"), 120: ("Maks. PM", "%"), 121: ("Obrażenia umiejętności", "%"), 122: ("Średnie obrażenia", "%"),
    123: ("Odporność na obrażenia umiejętności", "%"), 124: ("Odporność na średnie obrażenia", "%"),
    125: ("Bonus doświadczenia (iCafe)", "%"), 126: ("Bonus dropu przedmiotów (iCafe)", "%"),
    127: ("Bonus doświadczenia (Ramadan)", "%"), 128: ("Energia", ""), 129: ("Koniec energii", ""),
    130: ("Bonus kostiumu", "%"), 131: ("Magiczny atak", "%"), 132: ("Magiczny/fizyczny atak", "%"),
    133: ("Odporność na lód", "%"), 134: ("Odporność na ziemię", "%"), 135: ("Odporność na mrok", "%"),
    136: ("Odporność na cios krytyczny", "%"), 137: ("Odporność na przeszywający", "%"), 138: ("Terror", "%"),
    139: ("Regeneracja wytrzymałości", "%"), 140: ("Atak sztyletem przeciw potworom", ""),
    141: ("Wartość ataku przeciw potworom", ""), 142: ("Odporność na potwory", "‰"), 143: ("Pochłanianie obrażeń", "%"),
    144: ("Pochłanianie obrażeń od potworów", "%"), 145: ("Przełamanie odporności na ogłuszenie", ""),
    146: ("Przełamanie klątwy świątyni", ""), 147: ("Czas trwania umiejętności", "%"),
    148: ("Silny przeciw potworom z Doliny Orków", "%"), 149: ("Silny przeciw Metinom", "%"),
    150: ("Silny przeciw bossom", "%"), 151: ("Magiczny atak przeciw potworom", "%"),
    152: ("Przełamanie odporności na miecz", "%"), 153: ("Przełamanie odporności na broń dwuręczną", "%"),
    154: ("Przełamanie odporności na sztylet", "%"), 155: ("Przełamanie odporności na dzwonek", "%"),
    156: ("Przełamanie odporności na wachlarz", "%"), 157: ("Przełamanie odporności na łuk", "%"),
    158: ("Szansa na zbieranie", "%"), 159: ("Szansa na naukę", "%"), 160: ("Odporność na ludzi", "%"),
    161: ("Magiczny atak", ""), 162: ("Szansa na podpalenie", "%"), 163: ("Zamiana obrażeń na PM", "%"),
    164: ("Szansa na rzadki łup", "%"), 165: ("Magiczna wartość ataku przeciw potworom", ""),
    166: ("Szansa na unieruchomienie", "%"), 167: ("Atak specjalny", ""), 168: ("Kara za śmierć", "%"),
    169: ("Atak broni min. (klient)", ""), 170: ("Atak broni maks. (klient)", ""),
    171: ("Magiczny atak broni min. (klient)", ""), 172: ("Magiczny atak broni maks. (klient)", ""),
    173: ("Atak min. (klient)", ""), 174: ("Atak maks. (klient)", ""), 175: ("Celność (klient)", ""),
    176: ("Unik (klient)", ""), 177: ("Absorpcja szarfy", "%"),
}
TECHNICAL_POINTS = {1, 2, 3, 4, 5, 7, 9, 11, 20, 24, 25, 26, 27, 29, 30, 31, 35, 36, 91, 92, 102, 103, 104, 105,
                    106, 108, 109, 110, 111, 112, 113, 125, 126, 127, 128, 129, 169, 170, 171, 172, 173, 174,
                    175, 176, 177}

ITEM_TYPES = {
    0: "Brak", 1: "Broń", 2: "Zbroja i biżuteria", 3: "Przedmiot użytkowy", 4: "Używany automatycznie",
    5: "Materiał", 6: "Specjalny", 7: "Narzędzie", 8: "Los", 9: "Yang", 10: "Kamień duchowy", 11: "Pojemnik",
    12: "Ryba", 13: "Wędka", 14: "Surowiec", 15: "Ognisko", 16: "Unikatowy", 17: "Księga umiejętności",
    18: "Przedmiot do zadań", 19: "Kula przemiany", 20: "Skrzynia skarbów", 21: "Klucz", 22: "Księga zapomnienia",
    23: "Szkatułka", 24: "Kilof", 25: "Fryzura", 26: "Totem", 27: "Mieszanka", 28: "Kostium", 29: "Kamień smoka",
    30: "Specjalny kamień smoka", 31: "Ekstrakt", 32: "Druga waluta", 33: "Pierścień", 34: "Pas",
    35: "Nóż zielarski", 36: "Mikstura", 37: "Zwierzak",
}
SUBTYPES = {
    1: {0: "Miecz", 1: "Sztylet", 2: "Łuk", 3: "Broń dwuręczna", 4: "Dzwon", 5: "Wachlarz", 6: "Strzała",
        7: "Włócznia jeździecka", 8: "Pazury", 9: "Kołczan"},
    2: {0: "Zbroja", 1: "Hełm", 2: "Tarcza", 3: "Bransoleta", 4: "Buty", 5: "Naszyjnik", 6: "Kolczyki",
        7: "Wisior", 8: "Rękawice"},
    28: {0: "Kostium", 1: "Fryzura", 2: "Wierzchowiec", 3: "Szarfa", 4: "Kostium broni"},
}
LIMIT_TYPES = {0: "brak", 1: "poziom postaci", 2: "siła", 3: "zręczność", 4: "inteligencja", 5: "witalność",
               6: "PC-bang (nieużywane)", 7: "czas rzeczywisty (sekundy)", 8: "czas od pierwszego użycia (s)",
               9: "czas noszenia (s)"}
LIMIT_LEVEL = 1


def _int(low, high, label):
    return {"kind": "int", "min": low, "max": high, "label": label}


TINY, UTINY, SMALL, USMALL = (-128, 127), (0, 255), (-32768, 32767), (0, 65535)
INT, UINT = (-2147483648, 2147483647), (0, 4294967295)
SPECS = {
    "locale_name": {"kind": "cp1250", "max": 32, "required": True, "label": "Nazwa w grze"},
    "type": _int(*TINY, "Typ (type)"), "subtype": _int(*TINY, "Podtyp (subtype)"),
    "stack": _int(*UINT, "Maks. w jednym stosie (stack)"), "weight": _int(*TINY, "Waga (weight)"),
    "size": _int(*TINY, "Wysokość w ekwipunku: 1–3 pola (size)"),
    "antiflag": _int(*INT, "Zakazy – antiflag (bity)"), "flag": _int(*INT, "Cechy – flag (bity)"),
    "wearflag": _int(*INT, "Miejsce noszenia – wearflag (bity)"),
    "gold": _int(*INT, "Cena w sklepie NPC (gold)"), "shop_buy_price": _int(*UINT, "Cena odkupu przez NPC (shop_buy_price)"),
    "refined_vnum": _int(*UINT, "Vnum po ulepszeniu (refined_vnum)"), "refine_set": _int(*USMALL, "Przepis ulepszenia (refine_set)"),
    "magic_pct": _int(*TINY, "Szansa na magiczny przedmiot (magic_pct)"), "specular": _int(*TINY, "Połysk (specular)"),
    "socket_pct": _int(*TINY, "Szansa na gniazda (socket_pct)"), "addon_type": _int(*SMALL, "Dodatkowe bonusy (addon_type)"),
    "limittype0": _int(*TINY, "Wymaganie 1 – rodzaj (limittype0)"), "limitvalue0": _int(*INT, "Wymaganie 1 – wartość (limitvalue0)"),
    "limittype1": _int(*TINY, "Wymaganie 2 – rodzaj (limittype1)"), "limitvalue1": _int(*INT, "Wymaganie 2 – wartość (limitvalue1)"),
}
for _i in range(3):
    SPECS[f"applytype{_i}"] = _int(*UTINY, f"Bonus {_i + 1} – rodzaj (applytype{_i})")
    SPECS[f"applyvalue{_i}"] = _int(*INT, f"Bonus {_i + 1} – wartość (applyvalue{_i})")
for _i in range(6):
    SPECS[f"value{_i}"] = _int(*INT, f"Wartość {_i} (value{_i})")
for _i in range(6):
    SPECS[f"socket{_i}"] = _int(*TINY, f"Gniazdo {_i} (socket{_i})")
ADVANCED_ORDER = ["type", "subtype", "limittype0", "limitvalue0", "limittype1", "limitvalue1",
                  "value0", "value1", "value2", "value3", "value4", "value5",
                  "gold", "shop_buy_price", "stack", "size", "weight", "antiflag", "flag", "wearflag",
                  "refined_vnum", "refine_set", "magic_pct", "socket_pct", "addon_type", "specular",
                  "socket0", "socket1", "socket2", "socket3", "socket4", "socket5"]
# Quantities: "change every item of the family by the same amount" applies to
# these; every other column (kinds, flags, links) is set to the same value.
DELTA_COLS = {"limitvalue0", "limitvalue1", "applyvalue0", "applyvalue1", "applyvalue2", "value0", "value1",
              "value2", "value3", "value4", "value5", "gold", "shop_buy_price"}
FAMILY_SUFFIX = re.compile(r"^(.*?)(\s*\+\d)$")

# MT2009_PLUS_ITEM_EXTRA_APPLY_V1: the extra bonus lines (see the docstring).
EXTRA_TABLE = "world.item_extra_apply"
EXTRA_ROW_COLS = ("vnum", "slot", "apply_type", "apply_value")
EXTRA_MAX = 30
EXTRA_SPECS = {"apply_type": {"kind": "int", "min": 1, "max": 255, "label": "Dodatkowy bonus – rodzaj"},
               "apply_value": _int(*INT, "Dodatkowy bonus – wartość")}
EXTRA_CREATE = (f"CREATE TABLE IF NOT EXISTS {EXTRA_TABLE} (vnum INT UNSIGNED NOT NULL, slot TINYINT UNSIGNED NOT NULL, "
                "apply_type TINYINT UNSIGNED NOT NULL DEFAULT 0, apply_value INT NOT NULL DEFAULT 0, "
                "PRIMARY KEY (vnum, slot)) ENGINE=InnoDB")
# Item types the game wears (CItem::ModifyPoints runs for what is equipped):
# weapon, armour and jewellery, unique (rings of old), costume, ring, belt.
WORN_TYPES = {1, 2, 16, 28, 33, 34}
COSTUME_MOUNT = (28, 2)
_EXTRA_STATE = {"ready": False}


def ensure_extra_table(rows):
    """The table apply.sh creates at every start - made here too, so a panel
    updated before the server's next start can already save lines."""
    if _EXTRA_STATE["ready"]:
        return
    rows(EXTRA_CREATE)
    _EXTRA_STATE["ready"] = True


def load_extras(rows, vnums):
    """{vnum: [{"slot", "apply_type", "apply_value"}]} of the given items,
    each list in slot order."""
    vnums = sorted({int(v) for v in vnums})
    out = {v: [] for v in vnums}
    if not vnums:
        return out
    marks = ",".join(["%s"] * len(vnums))
    for r in rows(f"SELECT vnum, slot, apply_type, apply_value FROM {EXTRA_TABLE} WHERE vnum IN ({marks}) "
                  "ORDER BY vnum, slot", vnums):
        out.setdefault(int(r["vnum"]), []).append({"slot": int(r["slot"]), "apply_type": int(r["apply_type"]),
                                                    "apply_value": int(r["apply_value"])})
    for lines in out.values():
        lines.sort(key=lambda line: line["slot"])
    return out


def extra_line_text(point, value):
    name, unit = POINT_LABELS.get(int(point or 0), (f"#{point}", ""))
    return f"{name} {int(value):+d}{unit}"


def extra_text(lines):
    return ", ".join(extra_line_text(l["apply_type"], l["apply_value"]) for l in lines if l["apply_type"])


def parse_extra_form(form):
    """(lines [(apply_type, apply_value)] in order, errors, submitted rows)
    of the "Dodatkowe bonusy" form: rows extra_type / extra_value, a row
    without a bonus kind is left out (that is how a line is removed)."""
    types, values = form.getlist("extra_type"), form.getlist("extra_value")
    lines, errors, raw = [], [], []
    for i in range(max(len(types), len(values))):
        type_text = (types[i] if i < len(types) else "").strip()
        value_text = (values[i] if i < len(values) else "").strip()
        raw.append({"apply_type": type_text, "apply_value": value_text})
        if type_text in ("", "0"):
            continue
        n = len(lines) + 1
        point, error = common.validate(EXTRA_SPECS["apply_type"], type_text, f"Dodatkowy bonus {n} – rodzaj")
        if error:
            errors.append(error)
            continue
        value, error = common.validate(EXTRA_SPECS["apply_value"], value_text or "0", f"Dodatkowy bonus {n} – wartość")
        if error:
            errors.append(error)
            continue
        if value == 0:
            errors.append(f"Dodatkowy bonus {n} ({point_label(point)}): podaj wartość różną od 0 "
                          "albo ustaw „— brak bonusu —”, aby go usunąć.")
            continue
        lines.append((point, value))
    if len(lines) > EXTRA_MAX:
        errors.append(f"Najwyżej {EXTRA_MAX} dodatkowych bonusów na przedmiot (podano {len(lines)}).")
    return lines, errors, raw


def extra_changes(vnum, current, lines):
    """(inserts, deletes) for write_rows that turn an item's current lines
    into `lines` (slot = position, from 1); an unchanged slot is left alone."""
    want = {i + 1: (int(t), int(v)) for i, (t, v) in enumerate(lines)}
    have = {l["slot"]: (l["apply_type"], l["apply_value"]) for l in current}
    inserts, deletes = [], []
    for slot in sorted(set(want) | set(have)):
        if want.get(slot) == have.get(slot):
            continue
        if slot in have:
            deletes.append({"vnum": int(vnum), "slot": slot})
        if slot in want:
            inserts.append({"vnum": int(vnum), "slot": slot, "apply_type": want[slot][0], "apply_value": want[slot][1]})
    return inserts, deletes


def extra_history_formatter(col, value):
    if col != common.ROW_COL:
        return None
    data = json.loads(value)
    return f"{data.get('slot', '?')}. {extra_line_text(data.get('apply_type'), data.get('apply_value', 0))} [{data.get('apply_type')}]"


def value_fields(item_type, subtype):
    """The value columns this kind of item uses, with what they mean."""
    if item_type == 1 and subtype == 6:
        return [("value3", "Dodatkowe obrażenia strzały", "dodawane do ataku łuku")]
    if item_type == 1 and subtype != 9:
        return [("value3", "Atak – minimum", "najmniejszy atak fizyczny broni"),
                ("value4", "Atak – maksimum", "największy atak fizyczny broni"),
                ("value1", "Atak magiczny – minimum", "dla sury i szamana"),
                ("value2", "Atak magiczny – maksimum", "dla sury i szamana"),
                ("value5", "Premia ataku z ulepszenia", "dodawana do ataku i ataku magicznego (w walce liczona ×2); rośnie z każdym +")]
    if item_type == 2 and subtype in (0, 1, 2, 4):
        return [("value1", "Obrona", "obrona tego przedmiotu"),
                ("value5", "Premia obrony z ulepszenia", "dodawana do obrony (liczona ×2); rośnie z każdym +")]
    return []


def level_slot(item):
    """Which limit slot holds the level requirement (or a free one)."""
    for slot in (0, 1):
        if int(item.get(f"limittype{slot}") or 0) == LIMIT_LEVEL:
            return slot
    for slot in (0, 1):
        if int(item.get(f"limittype{slot}") or 0) == 0:
            return slot
    return None


def friendly_columns(item):
    cols = {"locale_name"} | {f"applytype{i}" for i in range(3)} | {f"applyvalue{i}" for i in range(3)}
    slot = level_slot(item)
    if slot is not None:
        cols |= {f"limittype{slot}", f"limitvalue{slot}"}
    cols |= {c for c, _l, _h in value_fields(int(item.get("type") or 0), int(item.get("subtype") or 0))}
    return cols


def point_label(point):
    name, unit = POINT_LABELS.get(int(point or 0), (f"Nieznany bonus #{point}", ""))
    return name + (f" ({unit})" if unit else "")


def point_options():
    regular = sorted((p for p in POINT_LABELS if p and p not in TECHNICAL_POINTS), key=lambda p: point_label(p).lower())
    technical = sorted(TECHNICAL_POINTS, key=lambda p: point_label(p).lower())
    return [(p, point_label(p)) for p in regular], [(p, point_label(p)) for p in technical]


def bonus_text(item):
    parts = []
    for i in range(3):
        point = int(item.get(f"applytype{i}") or 0)
        if point:
            value = int(item.get(f"applyvalue{i}") or 0)
            name, unit = POINT_LABELS.get(point, (f"#{point}", ""))
            parts.append(f"{name} {value:+d}{unit}")
    return ", ".join(parts)


def type_text(item_type, subtype):
    base = ITEM_TYPES.get(int(item_type), f"Typ {item_type}")
    sub = SUBTYPES.get(int(item_type), {}).get(int(subtype))
    return f"{base} · {sub}" if sub else base


ROW_COLS = ["vnum"] + list(SPECS)


def select_sql(where, order="vnum", limit=None):
    cols = ", ".join("CAST(`locale_name` AS BINARY) AS `locale_name`" if c == "locale_name" else f"`{c}`" for c in ROW_COLS)
    sql = f"SELECT {cols} FROM {TABLE} WHERE {where} ORDER BY {order}"
    return sql + (f" LIMIT {int(limit)}" if limit else "")


def decorate(row):
    row = dict(row)
    row["locale_name"] = common.text_of(row.get("locale_name")) or ""
    row["type_text"] = type_text(row["type"], row["subtype"])
    row["bonus_text"] = bonus_text(row)
    slot = level_slot(row)
    row["level"] = int(row.get(f"limitvalue{slot}") or 0) if slot is not None and int(row.get(f"limittype{slot}") or 0) == LIMIT_LEVEL else 0
    return row


def family_of(item, rows):
    """The same item at +0..+9: vnums sharing all but the last digit, the
    same kind and a "+N" at the end of the name. [] when it has none."""
    if not FAMILY_SUFFIX.match(item["locale_name"] or ""):
        return []
    base = int(item["vnum"]) - int(item["vnum"]) % 10
    found = rows(select_sql("vnum BETWEEN %s AND %s AND type=%s AND subtype=%s"),
                 (base, base + 9, item["type"], item["subtype"]))
    members = [decorate(r) for r in found]
    members = [m for m in members if FAMILY_SUFFIX.match(m["locale_name"])]
    return members if len(members) > 1 else []


def family_value(col, edited_value, old_value, member, mode):
    """What a family member gets when the edited item's col went from
    old_value to edited_value."""
    if col == "locale_name":
        match = FAMILY_SUFFIX.match(edited_value)
        stem = match.group(1) if match else edited_value
        own = FAMILY_SUFFIX.match(member["locale_name"])
        return stem + (own.group(2) if own else "")
    if mode == "delta" and col in DELTA_COLS:
        return int(member.get(col) or 0) + int(edited_value) - int(old_value or 0)
    return edited_value


def parse_form(item, form):
    """(new values {col: value}, errors, submitted text by field) for the
    edit form: the friendly fields plus the advanced columns."""
    errors, new, raw = [], {}, {}
    friendly = friendly_columns(item)

    def take(col, text, label=None):
        raw[col] = text
        value, error = common.validate(SPECS[col], text, label or SPECS[col]["label"])
        if error:
            errors.append(error)
        else:
            new[col] = value

    if "locale_name" in form:
        take("locale_name", form.get("locale_name", ""))
    slot = level_slot(item)
    if "level" in form:
        raw["level"] = form.get("level", "")
        if slot is None:
            errors.append("Ten przedmiot ma zajęte oba wymagania – poziom zmień w części „Zaawansowane”.")
        else:
            value, error = common.validate({"kind": "int", "min": 0, "max": 255}, form.get("level", ""), "Wymagany poziom")
            if error:
                errors.append(error)
            else:
                new[f"limitvalue{slot}"] = value
                # A free slot becomes the level requirement only when a level is set.
                if value > 0 or int(item.get(f"limittype{slot}") or 0) == LIMIT_LEVEL:
                    new[f"limittype{slot}"] = LIMIT_LEVEL
    for i in range(3):
        if f"applytype{i}" in form:
            take(f"applytype{i}", form.get(f"applytype{i}", "0"), f"Bonus {i + 1} – rodzaj")
            take(f"applyvalue{i}", form.get(f"applyvalue{i}", "0") or "0", f"Bonus {i + 1} – wartość")
            if new.get(f"applytype{i}") == 0 and f"applyvalue{i}" in new:
                new[f"applyvalue{i}"] = 0
    for col, label, _help in value_fields(int(item.get("type") or 0), int(item.get("subtype") or 0)):
        if col in form:
            take(col, form.get(col, ""), label)
    for col in ADVANCED_ORDER:
        if col in friendly or f"adv_{col}" not in form:
            continue
        take(col, form.get(f"adv_{col}", ""))
    for low, high, label in (("value3", "value4", "Atak"), ("value1", "value2", "Atak magiczny")):
        if low in new and high in new and low in friendly and high in friendly and new[low] > new[high]:
            errors.append(f"{label}: minimum ({new[low]}) jest większe niż maksimum ({new[high]}).")
    return new, errors, raw


def history_formatter(col, value):
    if col.startswith("applytype"):
        return f"{point_label(int(value))} [{value}]"
    if col.startswith("limittype"):
        return f"{LIMIT_TYPES.get(int(value), value)} [{value}]"
    if col == "type":
        return f"{ITEM_TYPES.get(int(value), value)} [{value}]"
    return None


def install(bp, ctx):
    common.init(ctx)
    common.register_table(TABLE, "vnum", SPECS, "Przedmiot", "dbeditor.item_edit", history_formatter)
    # MT2009_PLUS_ITEM_EXTRA_APPLY_V1: whole rows (one a line), key vnum:slot
    common.register_table(EXTRA_TABLE, ("vnum", "slot"), EXTRA_SPECS, "Dodatkowy bonus", "dbeditor.item_edit",
                          extra_history_formatter, row_cols=EXTRA_ROW_COLS, row_label="dodatkowy bonus (ponad 3)")
    common.install_history(bp, ctx)
    login_required = ctx["login_required"]

    def rows(sql, params=()):
        # looked up per call, so the panel's helpers can be swapped (tests)
        return common.ctx()["rows"](sql, params)

    def page_context():
        try:
            return common.pending_context()
        except Exception:
            return {"pending_total": 0}

    def load(vnum):
        found = rows(select_sql("vnum=%s"), (vnum,))
        return decorate(found[0]) if found else None

    @bp.route("/items")
    @login_required
    def items():
        query = (request.args.get("q") or "").strip()
        type_raw = request.args.get("typ", "")
        item_type = int(type_raw) if type_raw.isdigit() else None
        sub_raw = request.args.get("podtyp", "")
        subtype = int(sub_raw) if sub_raw.isdigit() and item_type is not None else None
        only_top = request.args.get("rodziny") == "1"
        where, params = [], []
        if query:
            if query.isdigit():
                where.append("(vnum=%s OR locale_name LIKE %s)")
                params += [int(query), f"%{query}%"]
            else:
                where.append("locale_name LIKE %s")
                params.append(f"%{query}%")
        if item_type is not None:
            where.append("type=%s")
            params.append(item_type)
        if subtype is not None:
            where.append("subtype=%s")
            params.append(subtype)
        if only_top:
            where.append("(MOD(vnum,10)=0 OR locale_name NOT REGEXP '[+][0-9]$')")
        limit = 300
        results, more = [], False
        if where:
            found = rows(select_sql(" AND ".join(where), limit=limit + 1), params)
            more = len(found) > limit
            results = [decorate(r) for r in found[:limit]]
        types = rows(f"SELECT type, COUNT(*) AS count FROM {TABLE} GROUP BY type ORDER BY type")
        return render_template("dbeditor/items.html", results=results, more=more, limit=limit, query=query,
                               item_type=item_type, subtype=subtype, only_top=only_top, searched=bool(where),
                               types=[(int(t["type"]), ITEM_TYPES.get(int(t["type"]), f"Typ {t['type']}"), t["count"]) for t in types],
                               subtypes=SUBTYPES.get(item_type, {}) if item_type is not None else {},
                               **page_context())

    @bp.route("/items/<int:vnum>", methods=["GET", "POST"])
    @login_required
    def item_edit(vnum):
        item = load(vnum)
        if not item:
            abort(404)
        family = family_of(item, rows)
        raw = {}
        if request.method == "POST":
            if not common.check_csrf():
                return redirect(url_for("dbeditor.item_edit", vnum=vnum))
            new, errors, raw = parse_form(item, request.form)
            changed = {c: v for c, v in new.items() if str(v) != str(item.get(c))}
            updates, members_changed = [], 0
            if not errors and changed:
                updates.append((vnum, changed))
                if family and request.form.get("family") == "1":
                    mode = "delta" if request.form.get("family_mode") == "delta" else "same"
                    for member in family:
                        if int(member["vnum"]) == vnum:
                            continue
                        values = {}
                        for col, value in changed.items():
                            member_value = family_value(col, value, item.get(col), member, mode)
                            label = f"{member['locale_name']} ({member['vnum']}) · {SPECS[col]['label']}"
                            checked, error = common.validate(SPECS[col], member_value, label)
                            if error:
                                errors.append(error)
                            else:
                                values[col] = checked
                        if values:
                            updates.append((int(member["vnum"]), values))
                            members_changed += 1
            if errors:
                for error in errors[:8]:
                    flash(error, "error")
                flash("Nic nie zapisano – popraw zaznaczone pola.", "error")
            elif not changed:
                flash("Brak zmian do zapisania.", "success")
                return redirect(url_for("dbeditor.item_edit", vnum=vnum))
            else:
                names = {int(m["vnum"]): m["locale_name"] for m in family}
                names[vnum] = new.get("locale_name", item["locale_name"])
                try:
                    _batch, saved = common.save_rows(
                        TABLE, updates, note=("Edycja przedmiotu" + (" z rodziną +0..+9" if members_changed else "")),
                        label_of=lambda key: names.get(int(key), ""))
                except Exception as exc:
                    flash(f"Nie udało się zapisać: {exc}", "error")
                    return redirect(url_for("dbeditor.item_edit", vnum=vnum))
                touched = len({key for key, *_rest in saved})
                lost = sorted({col for key, col, *_rest in saved
                               if col in protected.overwritten_cols(key, item.get("type") if int(key) == vnum else None)})
                if lost:
                    flash(f"{protected.WARNING}: {', '.join(lost)}.", "warning")
                flash(f"Zapisano {len(saved)} zmian w {touched} przedmiot(ach). "
                      "Zmiany czekają na zastosowanie (restart gry).", "success")
                return redirect(url_for("dbeditor.item_edit", vnum=vnum))
        return render_edit(item, family, raw)

    def render_edit(item, family, raw=None, extra_raw=None):
        vnum = int(item["vnum"])
        raw = raw or {}
        try:
            history = common.history_batches(10, TABLE, vnum)
        except Exception:
            history = []
        # MT2009_PLUS_ITEM_EXTRA_APPLY_V1: the extra lines of the item and its family
        extras, extra_error = {}, None
        try:
            ensure_extra_table(rows)
            extras = load_extras(rows, [vnum] + [int(m["vnum"]) for m in family])
        except Exception as exc:  # no table yet (apply.sh has not run): the page still opens
            extra_error = str(exc)
        try:
            prefix = f"{vnum}:"
            extra_history = [b for b in common.history_batches(50, EXTRA_TABLE)
                             if any(str(r["row_key"]).startswith(prefix) for r in b["rows"])][:10]
        except Exception:
            extra_history = []
        own_extras = extras.get(vnum, [])
        for member in family:
            member["extra_text"] = extra_text(extras.get(int(member["vnum"]), []))
        regular, technical = point_options()
        boot_rules = protected.rules_for(vnum, item.get("type"))
        slot = level_slot(item)
        friendly = friendly_columns(item)
        item_type, item_subtype = int(item.get("type") or 0), int(item.get("subtype") or 0)
        return render_template(
            "dbeditor/items_edit.html", item=item, raw=raw, family=family, history=history,
            value_fields=value_fields(item_type, item_subtype), level_slot=slot,
            advanced=[c for c in ADVANCED_ORDER if c not in friendly], specs=SPECS, limit_types=LIMIT_TYPES,
            point_regular=regular, point_technical=technical, point_labels=POINT_LABELS,
            boot_rules=boot_rules, boot_always=protected.overwritten_cols(vnum, item.get("type")),
            boot_warning=protected.WARNING,
            extras=own_extras, extra_text=extra_text(own_extras), extra_error=extra_error,
            # one empty row to start from (more with "Dodaj bonus")
            extra_raw=(extra_raw if extra_raw is not None else
                       [{"apply_type": l["apply_type"], "apply_value": l["apply_value"]} for l in own_extras])
            or [{"apply_type": 0, "apply_value": 0}],
            extra_history=extra_history, extra_max=EXTRA_MAX,
            extra_worn=item_type in WORN_TYPES, extra_mount=(item_type, item_subtype) == COSTUME_MOUNT,
            dbe_csrf=common.csrf_token(), **common.template_helpers(), **page_context())

    @bp.route("/items/<int:vnum>/extra", methods=["POST"])
    @login_required
    def item_extra_save(vnum):
        """MT2009_PLUS_ITEM_EXTRA_APPLY_V1: the "Dodatkowe bonusy (ponad 3)"
        form - the item's whole list of extra lines, optionally copied to
        the whole +0..+9 family."""
        item = load(vnum)
        if not item:
            abort(404)
        if not common.check_csrf():
            return redirect(url_for("dbeditor.item_edit", vnum=vnum))
        family = family_of(item, rows)
        lines, errors, extra_raw = parse_extra_form(request.form)
        if errors:
            for error in errors[:8]:
                flash(error, "error")
            flash("Nic nie zapisano – popraw dodatkowe bonusy.", "error")
            return render_edit(item, family, extra_raw=extra_raw)
        with_family = bool(family) and request.form.get("extra_family") == "1"
        targets = [vnum] + ([int(m["vnum"]) for m in family if int(m["vnum"]) != vnum] if with_family else [])
        names = {int(m["vnum"]): m["locale_name"] for m in family}
        names[vnum] = item["locale_name"]
        try:
            ensure_extra_table(rows)
            current = load_extras(rows, targets)
        except Exception as exc:
            flash(f"Nie udało się odczytać tabeli {EXTRA_TABLE}: {exc}", "error")
            return redirect(url_for("dbeditor.item_edit", vnum=vnum))
        inserts, deletes = [], []
        for target in targets:
            ins, dels = extra_changes(target, current.get(target, []), lines)
            inserts += ins
            deletes += dels
        if not inserts and not deletes:
            flash("Brak zmian do zapisania.", "success")
            return redirect(url_for("dbeditor.item_edit", vnum=vnum))
        try:
            _batch, done = common.write_rows(
                EXTRA_TABLE, inserts=inserts, deletes=deletes,
                note="Dodatkowe bonusy" + (" z rodziną +0..+9" if with_family else ""),
                label_of=lambda key: names.get(int(str(key).split(":")[0]), ""))
        except Exception as exc:
            flash(f"Nie udało się zapisać: {exc}", "error")
            return redirect(url_for("dbeditor.item_edit", vnum=vnum))
        touched = len({str(key).split(":")[0] for key, _op in done})
        flash(f"Zapisano dodatkowe bonusy ({len(lines)} na przedmiot) w {touched} przedmiot(ach). "
              "Gra je założy po restarcie („Zastosuj”); gracze zobaczą je w opisie przedmiotu po pobraniu "
              "plików klienta (zip) ze strony „Zastosuj”.", "success")
        return redirect(url_for("dbeditor.item_edit", vnum=vnum))

    import dbeditor
    dbeditor.add_section("dbeditor.items", "⚔️", "Przedmioty",
                         "nazwy, wymagany poziom, bonusy, atak i obrona – także dla całej rodziny +0…+9")
    dbeditor.add_section("dbeditor.proto_history", "🕘", "Historia zmian przedmiotów i umiejętności",
                         "kto, kiedy i co zmienił – z cofaniem jednym kliknięciem")
