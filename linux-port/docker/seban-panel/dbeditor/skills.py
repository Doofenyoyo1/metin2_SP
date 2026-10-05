"""MT2009_PLUS_DB_EDITOR_V1: "Umiejętności" - world.skill_proto for an operator.

The cores read world.skill_proto at boot (CSkillManager::Initialize). Every
formula is parsed there by libpoly (CPoly::Analyze) and ONE formula with a
syntax error makes the whole skill table fail to load - so nothing is saved
here that this module's port of the parser (PolyParser, a line-by-line copy
of libpoly/Poly.cc's lexan/expr/term/factor/expo, only stricter at the end of
the text) rejects.

What the engine does with the columns (game/src/char_skill.cpp):
  szPointPoly        the effect / damage (HP: negative = damage) up to M10;
  szMasterBonusPoly  the same from G1 (GetUsedSkillMasterType >= GRAND_MASTER
                     replaces szPointPoly with it, an empty one gives 0);
  szSPCostPoly / szGrandMasterAddSPCostPoly  the MP cost, below / from G1;
  szDurationPoly     the effect's duration in seconds (int);
  szCooldownPoly     the cooldown in seconds (int);
  k = SKILL_POWER_BY_LEVEL[level] * bMaxLevel / 100, the table being
  common.locale's SKILL_POWER_BY_LEVEL row (read live, default below).
"""
import math
import os
import random
import time

from flask import abort, current_app, flash, jsonify, redirect, render_template, request, url_for

from dbeditor import common_items as common

TABLE = "world.skill_proto"

# Polish names, the same pairs as app.py SKILLS (Tieru's panel).
SKILL_NAMES_PL = {
    1: "Trzystronne Cięcie", 2: "Wir Miecza", 3: "Berserk", 4: "Aura Miecza", 5: "Szarża",
    16: "Duchowe Uderzenie", 17: "Tąpnięcie", 18: "Uderzenie Miecza", 19: "Silne Ciało", 20: "Walnięcie",
    31: "Zasadzka", 32: "Szybki Atak", 33: "Wirujący Sztylet", 34: "Krycie się", 35: "Trująca Chmura",
    46: "Powtarzalny Strzał", 47: "Deszcz Strzał", 48: "Ognista Strzała", 49: "Bezszelestny Chód", 50: "Trująca Strzała",
    51: "Grad Strzał",
    61: "Uderzenie Palcem", 62: "Smoczy Wir", 63: "Czarowane Ostrze", 64: "Strach", 65: "Czarowana Zbroja", 66: "Rozproszenie Magii",
    76: "Mroczne Uderzenie", 77: "Ogniste Uderzenie", 78: "Ognisty Duch", 79: "Mroczna Ochrona", 80: "Duchowy Cios", 81: "Mroczna Sfera",
    91: "Latający Talizman", 92: "Strzelający Smok", 93: "Smoczy Skowyt", 94: "Błogosławieństwo", 95: "Odbicie", 96: "Pomoc Smoka",
    106: "Błyskawiczny Rzut", 107: "Przywołanie Błyskawicy", 108: "Burzowy Szpon", 109: "Leczenie", 110: "Zwinność", 111: "Zwiększenie Ataku",
    121: "Przywództwo", 122: "Kombinacja", 124: "Górnictwo", 125: "Wytwarzanie", 126: "Język Shinsoo", 127: "Język Chunjo",
    128: "Język Jinno", 129: "Przemiana", 130: "Jazda konna", 131: "Przywołanie konia", 132: "Sprint",
    137: "Cios z konia", 138: "Tąpnięcie z konia", 139: "Fala mocy z konia", 140: "Grad strzał z konia",
    152: "Krew Smoczego Boga", 153: "Błogosławieństwo Smoczego Boga", 154: "Święty Pancerz", 155: "Przyspieszenie",
    156: "Gniew Smoczego Boga", 157: "Pomoc w rzucaniu",
}
SKILL_PATHS = {
    1: ("Wojownik", ((1, 2, 3, 4, 5), "Ciało"), ((16, 17, 18, 19, 20), "Umysł")),
    2: ("Ninja", ((31, 32, 33, 34, 35), "Ostrze"), ((46, 47, 48, 49, 50, 51), "Łuk")),
    3: ("Sura", ((61, 62, 63, 64, 65, 66), "Broń"), ((76, 77, 78, 79, 80, 81), "Czarna Magia")),
    4: ("Szaman", ((91, 92, 93, 94, 95, 96), "Smok"), ((106, 107, 108, 109, 110, 111), "Leczenie")),
}
CLASS_TABS = ((1, "Wojownik"), (2, "Ninja"), (3, "Sura"), (4, "Szaman"), (5, "Konne"), (0, "Pozostałe"))

POINT_ON_PL = {
    "HP": "obrażenia (liczba ujemna = obrażenia zadane celowi)", "NONE": "brak",
    "ATT_SPEED": "szybkość ataku", "MOV_SPEED": "szybkość ruchu", "ATT_GRADE": "wartość ataku",
    "DEF_GRADE": "obrona", "MAX_HP": "maks. PŻ", "MAX_SP": "maks. PM", "CRITICAL": "szansa na cios krytyczny",
    "CASTING_SPEED": "szybkość zaklęć", "RESIST_NORMAL": "odporność na zwykłe ataki", "REFLECT_MELEE": "odbicie obrażeń",
    "TERROR": "strach", "MANASHIELD": "tarcza many", "HIT_HP_RECOVER": "odzysk PŻ przy trafieniu",
    "MAGIC_ATT_MOB": "magiczny atak na potwory",
}

# The formula columns: (column, label, group, which variables the engine sets
# for it, which levels use it: "low" below G1, "high" from G1, "all").
POINT_VARS = ("k", "lv", "iq", "str", "dex", "con", "atk", "def", "odef", "wep", "mwep", "mtk", "amwep",
              "ar", "chain", "maxhp", "maxsp", "maxv", "gr", "sl", "ek")
FORMULAS = (
    ("szPointPoly", "Siła / obrażenia (poziomy 1 – M10)", "main", POINT_VARS, "low"),
    ("szMasterBonusPoly", "Siła / obrażenia od Arcymistrza (G1 – P)", "main", POINT_VARS, "high"),
    ("szDurationPoly", "Czas trwania efektu (sekundy)", "main", ("k", "iq"), "all"),
    ("szCooldownPoly", "Czas odnowienia – ładowania (sekundy)", "main", ("k",), "all"),
    ("szSPCostPoly", "Koszt PM (poziomy 1 – M10)", "main", ("k", "lv", "maxhp", "v", "maxv"), "low"),
    ("szGrandMasterAddSPCostPoly", "Koszt PM od Arcymistrza (G1 – P)", "main", ("k", "lv", "maxhp", "v", "maxv"), "high"),
    ("szPointPoly2", "Drugi efekt – siła", "adv", POINT_VARS, "all"),
    ("szDurationPoly2", "Drugi efekt – czas trwania (s)", "adv", ("k", "iq"), "all"),
    ("szPointPoly3", "Trzeci efekt – siła", "adv", POINT_VARS, "all"),
    ("szDurationPoly3", "Trzeci efekt – czas trwania (s)", "adv", ("k", "iq"), "all"),
    ("szDurationSPCostPoly", "Koszt PM utrzymania efektu (co pewien czas)", "adv", ("k",), "all"),
    ("szSplashAroundDamageAdjustPoly", "Mnożnik obrażeń obszarowych", "adv", ("k",), "all"),
)
FORMULA_INFO = {f[0]: f for f in FORMULAS}
# SetDurationVar("iq") runs on one code path only (char_skill.cpp, the
# splash's affect); elsewhere the duration formulas get k alone.
PARTIAL_VARS = {"szDurationPoly": {"iq"}, "szDurationPoly2": {"iq"}, "szDurationPoly3": {"iq"}}
NUMBERS = (
    ("bLevelLimit", "Wymagany poziom postaci (0 = bez wymagania)", 0, 255),
    ("iMaxHit", "Maks. liczba trafionych celów (0 = bez limitu)", 0, 127),
    ("dwTargetRange", "Zasięg do celu (cm)", 0, 100000),
    ("dwSplashRange", "Zasięg obszaru (cm)", 0, 100000),
)
VAR_HELP = {
    "k": "moc umiejętności na danym poziomie (tabela SKILL_POWER_BY_LEVEL)", "lv": "poziom postaci",
    "iq": "inteligencja", "str": "siła", "dex": "zręczność", "con": "witalność",
    "atk": "obrażenia zwykłego ataku postaci w cel", "def": "obrona", "odef": "obrona bez premii",
    "wep": "atak broni", "mwep": "magiczny atak broni", "mtk": "magiczny atak broni", "amwep": "średni magiczny atak broni",
    "ar": "współczynnik trafienia (zwykle ok. 1)", "chain": "numer celu w łańcuchu (0, 1, 2…)",
    "maxhp": "maks. PŻ", "maxsp": "maks. PM", "maxv": "maks. PM (koszt)", "v": "obecne PM/PŻ (koszt)",
    "gr": "stopień (0 zwykły, 1 M, 2 G, 3 P)", "sl": "poziom umiejętności", "ek": "(nieużywane)",
}
ALL_VARS = set(VAR_HELP) | {"pi", "e"}
SAMPLE = {"lv": 90, "str": 90, "dex": 60, "con": 60, "iq": 60, "atk": 1500, "def": 600, "odef": 500,
          "wep": 350, "mwep": 350, "mtk": 350, "amwep": 350, "ar": 1, "chain": 0, "maxhp": 20000,
          "maxsp": 6000, "maxv": 6000, "v": 6000, "ek": 0}
SAMPLE_EDITABLE = ("lv", "str", "dex", "con", "iq", "atk", "mwep", "wep", "def", "maxhp", "maxsp")
DEFAULT_POWER = [0, 5, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 50, 52, 54, 56, 58, 60,
                 63, 66, 69, 72, 82, 85, 88, 91, 94, 98, 102, 106, 110, 115, 125]
_POWER_CACHE = {"at": 0.0, "table": None, "source": "default"}


# ---- libpoly, in Python ------------------------------------------------------

class PolyError(ValueError):
    def __init__(self, message, pos):
        super().__init__(message)
        self.pos = pos


UNARY = {"rt": "sqrt", "sqrt": "sqrt", "cos": "cos", "sin": "sin", "tan": "tan", "cot": "cot", "csc": "csc",
         "cosec": "csc", "sec": "sec", "ln": "ln", "abs": "abs", "floor": "floor", "sign": "sign"}
BINARY = {"log": "log", "min": "min", "max": "max", "number": "irand", "irandom": "irand", "irand": "irand",
          "frandom": "frand", "frand": "frand", "mod": "mod"}
CONSTS = {"pi": math.pi, "e": math.e}
EOS, NUM, ID = "EOS", "NUM", "ID"


class PolyParser:
    """CPoly::Analyze. Tokens are what lexan() returns: NUM, ID (a variable
    or pi/e), a function name, a single character or EOS. The engine accepts
    two things this does not: text after a closing bracket at the top
    ("1+2)x" - the engine ignores it) and an operator with nothing after it
    ("1+" - the engine accepts it and then reads past its stack); both are
    reported as errors here."""

    def __init__(self, text):
        self.s = text
        self.pos = 0
        self.rpn = []
        self.names = set()

    def lexan(self):
        s = self.s
        while self.pos < len(s):
            ch = s[self.pos]
            if ch in " \t":
                self.pos += 1
                continue
            if ch.isascii() and ch.isdigit():
                start = self.pos
                while self.pos < len(s) and s[self.pos].isascii() and s[self.pos].isdigit():
                    self.pos += 1
                if self.pos < len(s) and s[self.pos] == ".":
                    self.pos += 1
                    while self.pos < len(s) and s[self.pos].isascii() and s[self.pos].isdigit():
                        self.pos += 1
                self.value = float(s[start:self.pos].rstrip(".") or "0")
                return NUM
            if ch.isascii() and ch.isalpha():
                start = self.pos
                while self.pos < len(s) and s[self.pos].isascii() and s[self.pos].isalpha():
                    self.pos += 1
                name = s[start:self.pos]
                self.name = name
                if name in UNARY or name in BINARY:
                    return name
                return ID
            self.pos += 1
            return ch
        return EOS

    def error(self, message):
        raise PolyError(message, self.pos)

    def match(self, token):
        if self.look == token:
            self.look = self.lexan()
        else:
            want = {EOS: "koniec wzoru", NUM: "liczba", ID: "zmienna"}.get(token, f"„{token}”")
            self.error(f"spodziewano się: {want}")

    def parse(self):
        if not self.s.strip():
            if self.s:
                self.error("wzór z samych spacji (gra go odrzuci) – zostaw puste pole")
            return []
        self.look = self.lexan()
        self.expr()
        if self.look != EOS:
            self.error("nadmiarowy znak – sprawdź nawiasy" if self.look == ")" else "nadmiarowy znak (np. przecinek poza funkcją)")
        return self.rpn

    def expr(self):
        if self.look in ("+", "-"):
            # libpoly: a leading sign is read as "0 <sign> ..."
            self.pos -= 1
            self.look = NUM
            self.value = 0.0
        self.term()
        while True:
            if self.look in ("+", "-"):
                op = self.look
                self.match(op)
                self.term()
                self.rpn.append(("op", op))
                continue
            if self.look in (EOS, ")", ","):
                return
            self.error(f"nieoczekiwany znak „{self.look}”" if len(str(self.look)) == 1 else "brakuje działania (+ - * /) między wartościami")

    def term(self):
        self.factor()
        while self.look in ("*", "/", "%"):
            op = self.look
            self.match(op)
            self.factor()
            self.rpn.append(("op", op))

    def factor(self):
        self.expo()
        while self.look == "^":
            self.match("^")
            self.expo()
            self.rpn.append(("op", "^"))

    def expo(self):
        look = self.look
        if look == "(":
            self.match("(")
            self.expr()
            self.match(")")
        elif look == NUM:
            self.rpn.append(("num", self.value))
            self.match(NUM)
        elif look == ID:
            self.names.add(self.name)
            self.rpn.append(("var", self.name))
            self.match(ID)
        elif look in UNARY:
            self.match(look)
            self.match("(")
            self.expr()
            self.match(")")
            self.rpn.append(("fn1", UNARY[look]))
        elif look in BINARY:
            self.match(look)
            self.match("(")
            self.expr()
            self.match(",")
            self.expr()
            self.match(")")
            self.rpn.append(("fn2", BINARY[look]))
        elif look == EOS:
            self.error("wzór urywa się – brakuje liczby lub zmiennej na końcu")
        else:
            self.error(f"nieoczekiwany znak „{look}”" if len(str(look)) == 1 else "tu powinna stać liczba lub zmienna")


class _Zero(Exception):
    """CPoly::Eval returns 0 for the whole formula (division by zero...)."""


def evaluate(rpn, variables, mode="mid"):
    """CPoly::Eval. mode picks number()/frandom(): "min", "max", "mid"
    (their middle) or "rand"."""
    stack = []
    try:
        for kind, arg in rpn:
            if kind == "num":
                stack.append(arg)
            elif kind == "var":
                stack.append(CONSTS[arg] if arg in CONSTS else float(variables.get(arg, 0)))
            elif kind == "op":
                b = stack.pop()
                a = stack.pop()
                if arg == "+":
                    stack.append(a + b)
                elif arg == "-":
                    stack.append(a - b)
                elif arg == "*":
                    stack.append(a * b)
                elif arg in ("/", "%"):
                    if b == 0:
                        raise _Zero()
                    stack.append(a / b if arg == "/" else math.fmod(a, b))
                elif arg == "^":
                    stack.append(math.pow(a, b))
            elif kind == "fn1":
                a = stack.pop()
                stack.append(_unary(arg, a))
            else:
                b = stack.pop()
                a = stack.pop()
                stack.append(_binary(arg, a, b, mode))
    except (_Zero, OverflowError, ValueError, ZeroDivisionError):
        return 0.0
    return stack[-1] if stack else 0.0


def _unary(name, a):
    if name == "sqrt":
        if a < 0:
            raise _Zero()
        return math.sqrt(a)
    if name in ("cos", "sin", "abs", "floor"):
        return {"cos": math.cos, "sin": math.sin, "abs": math.fabs, "floor": math.floor}[name](a)
    if name == "sign":
        return 0.0 if a == 0 else (-1.0 if a < 0 else 1.0)
    if name == "tan":
        if not math.cos(a):
            raise _Zero()
        return math.tan(a)
    if name == "csc":
        if not math.sin(a):
            raise _Zero()
        return 1 / math.sin(a)
    if name == "sec":
        if not math.cos(a):
            raise _Zero()
        return 1 / math.cos(a)
    if name == "cot":
        if not math.sin(a):
            raise _Zero()
        return math.cos(a) / math.sin(a)
    if name == "ln":
        if a <= 0:
            raise _Zero()
        return math.log(a)
    raise _Zero()


def _binary(name, a, b, mode):
    if name == "min":
        return a if a < b else b
    if name == "max":
        return a if a > b else b
    if name == "mod":
        if b == 0:
            raise _Zero()
        return math.fmod(a, b)
    if name == "log":
        # log(base, x), CPoly: save[iSp-2] = log(save[iSp-1]) / log(save[iSp-2])
        if b <= 0 or a <= 0 or a == 1:
            raise _Zero()
        return math.log(b) / math.log(a)
    if name == "irand":
        low = int(a + 0.5)
        span = int(b - a + 0.5) + 1
        high = low + span - 1
        return {"min": low, "max": high, "mid": (low + high) / 2}.get(mode, low + int(random.random() * span))
    if name == "frand":
        return {"min": a, "max": b, "mid": (a + b) / 2}.get(mode, random.random() * (b - a) + a)
    raise _Zero()


def check_formula(text, column=None):
    """(rpn or None, error text or None, [warnings], uses random)."""
    try:
        parser = PolyParser(text)
        rpn = parser.parse()
    except PolyError as exc:
        return None, f"Błąd w znaku {exc.pos}: {exc}", [], False
    warnings = []
    unknown = sorted(parser.names - ALL_VARS)
    if unknown:
        return None, ("Nieznana zmienna: " + ", ".join(unknown) + ". Dozwolone: " + ", ".join(sorted(ALL_VARS))
                      + " oraz funkcje: " + ", ".join(sorted(set(UNARY) | set(BINARY)))), [], False
    if column in FORMULA_INFO:
        allowed = set(FORMULA_INFO[column][3]) | set(CONSTS)
        foreign = sorted(parser.names - allowed)
        if foreign:
            warnings.append("Gra nie podaje tu zmiennej " + ", ".join(foreign)
                            + " – w tym polu przyjmie 0 albo przypadkową, ostatnio użytą wartość.")
        partial = sorted(parser.names & PARTIAL_VARS.get(column, set()))
        if partial:
            warnings.append("Zmienna " + ", ".join(partial) + " jest tu ustawiana tylko w niektórych sytuacjach.")
    random_used = any(kind == "fn2" and arg in ("irand", "frand") for kind, arg in rpn)
    return rpn, None, warnings, random_used


# ---- skill power (k) -----------------------------------------------------------

def power_table():
    now = time.time()
    if _POWER_CACHE["table"] is not None and now - _POWER_CACHE["at"] < 300:
        return _POWER_CACHE["table"], _POWER_CACHE["source"]
    table, source = DEFAULT_POWER, "default"
    try:
        row = common.ctx()["rows"]("SELECT mValue FROM common.locale WHERE mKey='SKILL_POWER_BY_LEVEL' LIMIT 1")
        values = [int(v) for v in str(common.text_of(row[0]["mValue"]) if row else "").split()]
        if len(values) >= 41:
            table, source = values[:41], "common.locale"
    except Exception:
        pass
    _POWER_CACHE.update(at=now, table=table, source=source)
    return table, source


def level_label(level, max_level=1):
    if max_level != 1:
        return str(level)
    if level >= 40:
        return "P"
    if level >= 30:
        return f"G{level - 29}"
    if level >= 20:
        return f"M{level - 19}"
    return str(level)


def preview_levels(max_level):
    if max_level == 1:
        return [1, 10, 20, 30, 40]
    top = max(1, min(int(max_level or 1), 40))
    return sorted({1, max(1, top // 2), top})


def preview(column, text, max_level=1, sample=None):
    rpn, error, warnings, random_used = check_formula(text, column)
    result = {"ok": error is None, "error": error, "warnings": warnings, "random": random_used, "values": []}
    if error:
        return result
    table, _source = power_table()
    max_level = int(max_level or 1)
    use = FORMULA_INFO.get(column, (None, None, None, None, "all"))[4]
    for level in preview_levels(max_level):
        k = table[min(level, 40)] * max_level / 100
        grade = 0 if level < 20 else 1 if level < 30 else 2 if level < 40 else 3
        variables = dict(SAMPLE)
        variables.update(sample or {})
        variables.update(k=k, gr=grade, sl=level)
        low = int(evaluate(rpn, variables, "min"))
        high = int(evaluate(rpn, variables, "max"))
        used = use == "all" or (use == "low") == (level < 30) or max_level != 1
        result["values"].append({"level": level, "label": level_label(level, max_level), "k": round(k, 3),
                                 "min": min(low, high), "max": max(low, high), "used": used})
    if not text.strip():
        result["warnings"].append("Puste pole – gra liczy tu 0."
                                  + (" Od G1 gra używa właśnie tego wzoru, więc umiejętność straci siłę na G/P."
                                     if column == "szMasterBonusPoly" else ""))
    return result


def sample_from(source, prefix=""):
    sample = {}
    for name in SAMPLE_EDITABLE:
        try:
            sample[name] = float(str(source.get(prefix + name, "")).replace(",", "."))
        except (TypeError, ValueError):
            continue
    return sample


# ---- data -----------------------------------------------------------------------

SPECS = {column: {"kind": "ascii", "max": 100, "label": label} for column, label, *_rest in FORMULAS}
SPECS.update({column: {"kind": "int", "min": low, "max": high, "label": label} for column, label, low, high in NUMBERS})


def skill_name(row):
    vnum = int(row["dwVnum"])
    return SKILL_NAMES_PL.get(vnum) or common.text_of(row.get("szName")) or f"Umiejętność {vnum}"


def skill_icon(vnum):
    for name in (f"{vnum}.png", f"passive_{vnum}.png"):
        if os.path.isfile(os.path.join(current_app.static_folder, "skill_icons", name)):
            return url_for("static", filename=f"skill_icons/{name}")
    return None


def skill_path(vnum, skill_type):
    entry = SKILL_PATHS.get(int(skill_type))
    if not entry:
        return ""
    for vnums, path in entry[1:]:
        if int(vnum) in vnums:
            return f"{entry[0]} – {path}"
    return entry[0]


def install(bp, ctx):
    common.init(ctx)
    common.register_table(TABLE, "dwVnum", SPECS, "Umiejętność", "dbeditor.skill_edit")
    common.install_history(bp, ctx)
    login_required = ctx["login_required"]

    def rows(sql, params=()):
        # looked up per call, so the panel's helpers can be swapped (tests)
        return common.ctx()["rows"](sql, params)

    def load(vnum):
        cols = ["dwVnum", "szName", "bType", "bMaxLevel", "szPointOn", "szPointOn2", "szPointOn3",
                "setFlag"] + list(SPECS)
        found = rows(f"SELECT {', '.join('`%s`' % c for c in cols)} FROM {TABLE} WHERE dwVnum=%s", (vnum,))
        if not found:
            return None
        row = dict(found[0])
        for column in FORMULA_INFO:
            row[column] = common.text_of(row[column]) or ""
        for column in ("szName", "szPointOn", "szPointOn2", "szPointOn3", "setFlag"):
            row[column] = common.text_of(row[column]) or ""
        row["name_pl"] = skill_name(row)
        return row

    def page_context():
        try:
            return common.pending_context()
        except Exception:
            return {"pending_total": 0}

    @bp.route("/skills")
    @login_required
    def skills():
        tab = request.args.get("klasa", "1")
        tab = int(tab) if tab.lstrip("-").isdigit() else 1
        found = rows(f"SELECT dwVnum,szName,bType,bMaxLevel,bLevelLimit,szPointOn,szPointPoly,szMasterBonusPoly,"
                     f"szDurationPoly,szCooldownPoly,szSPCostPoly,szGrandMasterAddSPCostPoly FROM {TABLE} ORDER BY dwVnum")
        counts, listed = {}, []
        for row in found:
            skill_type = int(row["bType"] or 0)
            counts[skill_type] = counts.get(skill_type, 0) + 1
            if skill_type != tab:
                continue
            row = dict(row)
            row["szName"] = common.text_of(row["szName"]) or ""
            row["name_pl"] = skill_name(row)
            row["path"] = skill_path(row["dwVnum"], skill_type)
            row["icon"] = skill_icon(row["dwVnum"])
            level = 40 if int(row["bMaxLevel"] or 1) == 1 else int(row["bMaxLevel"] or 1)
            summary = {}
            for column, key in (("szCooldownPoly", "cooldown"), ("szDurationPoly", "duration")):
                result = preview(column, common.text_of(row[column]) or "", row["bMaxLevel"])
                summary[key] = result["values"][-1] if result["ok"] and result["values"] else None
                summary[key + "_bad"] = not result["ok"]
            row["summary"] = summary
            row["top_label"] = level_label(level, int(row["bMaxLevel"] or 1))
            listed.append(row)
        return render_template("dbeditor/skills.html", skills=listed, tab=tab, tabs=CLASS_TABS, counts=counts,
                               **page_context())

    @bp.route("/skills/<int:vnum>", methods=["GET", "POST"])
    @login_required
    def skill_edit(vnum):
        skill = load(vnum)
        if not skill:
            abort(404)
        form_values = dict(skill)
        if request.method == "POST":
            if not common.check_csrf():
                return redirect(url_for("dbeditor.skill_edit", vnum=vnum))
            errors, new = [], {}
            same_master = request.form.get("same_master") == "1"
            same_master_sp = request.form.get("same_master_sp") == "1"
            for column, spec in SPECS.items():
                if column not in request.form:
                    continue
                raw = request.form.get(column, "")
                if column == "szMasterBonusPoly" and same_master:
                    raw = request.form.get("szPointPoly", "")
                if column == "szGrandMasterAddSPCostPoly" and same_master_sp:
                    raw = request.form.get("szSPCostPoly", "")
                value, error = common.validate(spec, raw, spec["label"])
                form_values[column] = raw
                if error:
                    errors.append(error)
                    continue
                if spec["kind"] == "ascii":
                    _rpn, problem, _warn, _rand = check_formula(value, column)
                    if problem:
                        errors.append(f"{spec['label']}: {problem}")
                        continue
                new[column] = value
            if errors:
                for error in errors:
                    flash(error, "error")
                flash("Nic nie zapisano – popraw zaznaczone pola.", "error")
            else:
                changed_fields = {c: v for c, v in new.items() if str(v) != str(skill.get(c, ""))}
                if not changed_fields:
                    flash("Brak zmian do zapisania.", "success")
                    return redirect(url_for("dbeditor.skill_edit", vnum=vnum))
                try:
                    _batch, changed = common.save_rows(TABLE, [(vnum, changed_fields)], note="Edycja umiejętności",
                                                       label_of=lambda _k: skill["name_pl"])
                except Exception as exc:
                    flash(f"Nie udało się zapisać: {exc}", "error")
                    return redirect(url_for("dbeditor.skill_edit", vnum=vnum))
                flash(f"Zapisano {len(changed)} pól umiejętności {skill['name_pl']}. "
                      "Zmiany czekają na zastosowanie (restart gry).", "success")
                return redirect(url_for("dbeditor.skill_edit", vnum=vnum))
        sample = sample_from(request.form, "sample_") if request.method == "POST" else {}
        previews = {c: preview(c, form_values.get(c) or "", skill["bMaxLevel"], sample) for c in FORMULA_INFO}
        table, source = power_table()
        try:
            history = common.history_batches(10, TABLE, vnum)
        except Exception:
            history = []
        return render_template("dbeditor/skills_edit.html", skill=skill, values=form_values, formulas=FORMULAS,
                               numbers=NUMBERS, previews=previews, point_on=POINT_ON_PL, sample=SAMPLE,
                               sample_editable=SAMPLE_EDITABLE, sample_now={**SAMPLE, **sample}, var_help=VAR_HELP, power=table, power_source=source,
                               icon=skill_icon(vnum), path=skill_path(vnum, skill["bType"]), history=history,
                               dbe_csrf=common.csrf_token(), level_label=level_label, **common.template_helpers(),
                               **page_context())

    @bp.post("/skills/preview")
    @login_required
    def skill_preview():
        data = request.get_json(silent=True) or {}
        column = str(data.get("column", ""))
        if column not in FORMULA_INFO:
            return jsonify({"ok": False, "error": "Nieznane pole."}), 400
        text = str(data.get("formula", ""))
        value, error = common.validate(SPECS[column], text, SPECS[column]["label"])
        if error:
            return jsonify({"ok": False, "error": error, "warnings": [], "values": []})
        try:
            max_level = int(data.get("max_level", 1))
        except (TypeError, ValueError):
            max_level = 1
        return jsonify(preview(column, value, max_level, sample_from(data.get("sample") or {})))

    import dbeditor
    dbeditor.add_section("dbeditor.skills", "✨", "Umiejętności",
                     "czas trwania, czas odnowienia, obrażenia i koszt PM umiejętności, z podglądem wartości M1/G1/P")
