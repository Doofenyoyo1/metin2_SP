"""MT2009_PLUS_DB_EDITOR_V1: the items linux-port/docker/mariadb/playerbot/
apply.sh writes into world.item_proto at EVERY start of the server (the
playerbot-migrate step runs before the db core boots). An edit of such a
field in the database editor lasts until the next start - the item page
warns about it.

Three kinds of rule, as apply.sh writes them:
  always   - the field is set to apply.sh's value on every start;
  partial  - only some bits / a floor (e.g. "stack at least 200", "flag | 4")
             are put back, the rest of an edit stays;
  when     - only while the field still has the package's old value (an
             edit that changes it is kept).

Kept in step with apply.sh by test_dbeditor_protected.py, which reads every
"UPDATE world.item_proto ... WHERE vnum ..." of apply.sh and checks that its
vnums and columns are listed here. mod/*.sql (REPLACE of the costume pack)
runs once per install, not at every start, and is not listed.
"""

AWAKENED_COLS = ("type", "subtype", "antiflag", "flag", "wearflag", "gold", "shop_buy_price", "limittype0",
                 "limitvalue0", "limittype1", "limitvalue1", "applytype0", "applyvalue0", "applytype1",
                 "applyvalue1", "applytype2", "applyvalue2", "value0", "value1", "value2", "value3", "value4",
                 "value5", "refined_vnum", "refine_set", "magic_pct", "specular", "socket_pct", "addon_type")
SOULSTONE_COLS = ("type", "subtype", "wearflag", "applytype0", "applyvalue0", "applytype1", "applyvalue1",
                  "value0", "value5", "refined_vnum", "refine_set")
ITEM_ROW_COLS = ("name", "locale_name", "type", "subtype", "stack", "size", "antiflag", "flag", "wearflag", "gold",
                 "shop_buy_price", "limittype0", "limitvalue0", "limittype1", "limitvalue1", "applytype0",
                 "applyvalue0", "applytype1", "applyvalue1", "applytype2", "applyvalue2", "value0", "value1",
                 "value2", "value3", "value4", "value5")
TRADE_SASHES = (50252, 50255, 50256, 50257, 50258, 50259, 50260, 51501, 51502, 51503, 51504, 51505, 51506, 51507,
                51508, 51509, 51510, 51541, 51548, 51549, 51562, 51569, 51576, 51583, 51590, 51597, 51604, 51611,
                51618, 51625, 51632, 76040)
POTION_PRICES = (55401, 55402, 55403, 55404, 55405, 55406, 55409, 55410, 55411, 55001, 55002, 55007, 55008, 55009,
                 55010, 55011, 55012, 55013, 55014, 55015, 55016, 55017, 55018, 55019, 55020, 55021, 55022, 55023,
                 55024, 55025, 55026, 55027, 55028, 55029, 55030, 55031, 55032, 55033, 55034, 55035, 55036, 55101,
                 55102, 55103, 55104, 55108, 55109, 55110, 55111, 55115, 55116, 55117, 55118)
STACK200 = (30118, 50006, 50007, 50011, 50012, 50013, 50033, 50034, 50037, 50070, 50071, 50072, 50073, 50074, 50075,
            50076, 50077, 50078, 50079, 50080, 50081, 50082, 50090, 50097, 50098, 50109, 50110, 50111, 50112, 50113,
            50114, 50115, 50120, 50218, 70009, 70619, 30670, 30300, 38054, 38056, 38057, 50130, 50132, 50133, 50134,
            50135, 50136, 50137)
BOOK_PIECES = (30765, 30766, 30767, 30768, 30773, 30774, 30775, 30776)


def _r(lo, hi=None):
    return (lo, lo if hi is None else hi)


# (vnum ranges, item types, kind, columns, what apply.sh does)
RULES = [
    ([_r(210, 219), _r(220, 229), _r(1160, 1169), _r(2190, 2199), _r(3170, 3179), _r(5150, 5159), _r(7170, 7179)],
     (), "always", AWAKENED_COLS, "broń przebudzona +0..+9 (Digi Rasta)"),
    ([_r(28530 + k) for k in range(14)] + [_r(28000 + g * 100 + k) for g in range(6, 10) for k in range(14)],
     (), "always", SOULSTONE_COLS, "kamienie duszy +5..+9"),
    ([_r(71056)], (), "always", ("type", "subtype", "stack", "antiflag", "flag", "wearflag", "value0", "value1", "value2"),
     "Olejek Niebios"),
    ([_r(8010)], (), "always", ITEM_ROW_COLS + ("refined_vnum", "refine_set"), "Kołczan"),
    ([_r(41986), _r(40233)], (), "always", ITEM_ROW_COLS, "kostium Króla Wojowników / Święty Miecz Bogów (king03)"),
    ([_r(v) for v in BOOK_PIECES], (), "always", ("name", "locale_name", "stack", "antiflag", "flag"),
     "kawałki ksiąg (Biblioteka, Arezzo)"),
    ([_r(31073), _r(40002)], (), "always", ("locale_name", "antiflag", "flag"),
     "Auto Łowy (8h) / Pierścień Anty-Exp z ItemShopu"),
    ([_r(100002)], (), "always", ("gold",), "Eliksir Czasu (D) kosztuje 5 000 000"),
    ([_r(v) for v in STACK200], (10, 23), "always", ("stack",), "stos 200 (kamienie duszy, szkatułki, skrzynie)"),
    ([_r(v) for v in STACK200], (10, 23), "partial", ("flag", "antiflag"),
     "zawsze łączy się w stos (flag | 4, bez ANTI_STACK)"),
    ([_r(30670)], (), "always", ("stack",), "Kamień Przebudzenia: stos 200"),
    ([_r(30670), _r(30202), _r(70031)], (), "partial", ("flag",), "zawsze łączy się w stos (flag | 4)"),
    ([_r(70058)], (), "partial", ("flag",), "bit 8192 flagi jest zawsze zdejmowany"),
    ([_r(v) for v in TRADE_SASHES] + [_r(85001, 85024), _r(85101, 85104), _r(86061, 86064), _r(110000, 175499)],
     (), "partial", ("antiflag",), "można handlować i wystawić w sklepie (bity GIVE i MYSHOP antiflag są zdejmowane)"),
    ([_r(v) for v in POTION_PRICES], (), "partial", ("gold",), "cena w sklepie NPC nie niższa niż cena odkupu"),
    ([_r(3180, 3189)], (), "when", ("limittype0", "limitvalue0", "applytype0", "applyvalue0", "applytype1",
                                   "applyvalue1", "applytype2", "applyvalue2", "value3", "value4", "value5", "gold",
                                   "shop_buy_price", "antiflag", "socket_pct", "refined_vnum", "refine_set"),
     "Pogromca Nieb. Smoka - tylko gdy wymagany poziom i atak są 0"),
    ([_r(30270)], (), "when", ("limittype0", "limitvalue0"), "Odłamek Smoczego Kamienia - limit czasu 24 h jest zdejmowany"),
    ([_r(50513)], (17, 22), "when", ("stack",), "stos 10 jest podnoszony do 200 (księgi)"),
    ([], (13,), "when", ("limitvalue0",), "wędki: wymagany poziom 50 jest zmieniany na 30"),
]

WARNING = "Ten przedmiot jest ustawiany przy każdym starcie serwera – zmiana zostanie nadpisana"


def rules_for(vnum, item_type=None):
    """[{kind, cols, note}] of the start-up rules that touch this item."""
    vnum = int(vnum)
    out = []
    for ranges, types, kind, cols, note in RULES:
        hit = any(lo <= vnum <= hi for lo, hi in ranges)
        if not hit and item_type is not None and types:
            hit = int(item_type) in types
        if hit:
            out.append({"kind": kind, "cols": list(cols), "note": note})
    return out


def overwritten_cols(vnum, item_type=None):
    """The columns an edit of which is lost at the next start for sure."""
    return sorted({c for r in rules_for(vnum, item_type) if r["kind"] == "always" for c in r["cols"]})


def is_protected(vnum, item_type=None):
    return bool(overwritten_cols(vnum, item_type))
