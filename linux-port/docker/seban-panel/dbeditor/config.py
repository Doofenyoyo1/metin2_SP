"""MT2009_PLUS_DB_EDITOR_CONFIG_V1: "Eksport / import konfiguracji" (/db/config)
- everything the operator changed through the database editor as one text,
to keep, compare or share (Discord), and the way back in.

What is exported: the NET effect of the editor against the stock game -
for every changed field its value in the game image ("oryginał") and its
value now ("teraz"), never the raw history, never accounts or characters.
Where the stock value comes from, per part:

  * database tables (items, extra bonus lines, skills, monsters, shop goods,
    refine recipes, bonus pools 1-5 and 6/7, the exp table): the editor's
    history (player.web_dbeditor_history, common_items.net_changes(
    include_applied=True)) - the FIRST old_value of every (table, key,
    column) is the stock value, because every write of the editor goes
    through that history. "Teraz" is read from the database itself at
    export time, so a field put back by hand, by an undo or by apply.sh at
    a start is not exported. Whole rows (shop goods, refine clones, extra
    bonus lines) are exported the same way as JSON rows ("-" = no row).
  * spool files (drop groups, common/etc drop, chests, fishing, boss and
    metin respawn files, map spawns): the panel's custom file IS the change
    against the image's file (which the game publishes as *.base.* /
    base/... next to it); a custom file is exported whole, as a block, when
    it differs from the image's file.

The text (format v1, diff friendly, one record a line):

    # MT2009 PLUS - konfiguracja edytora bazy danych v1
    # serwer: 2.22.0
    # data: 2026-10-05 14:03
    # Podsumowanie: ...
    [items] world.item_proto
    19	gold	1000 -> 2000	# Miecz+9 · Cena w sklepie NPC (gold)
    [shops] world.shop_item
    9001:11:5	*	- -> {"count":5,"item_vnum":11,"shop_vnum":9001}	# ...
    [drops]
    <<< mob enc=ascii eol=crlf nl=1 linie=12 sha256=0123456789abcdef
    |Group	MT2009_panel_101_drop
    ...
    >>> mob

  key, column, stock -> now; a value is an integer, a JSON string, a JSON
  object (a whole row) or "-" (no value / no row); "#" starts a comment.
  The compact form for a Discord message is "MT2009CFG1:" + base64 of the
  gzipped text.

Import parses and checks the text (version, known parts, every value by the
editor's own column specs, keys that exist on this server), shows what
would change, and on confirmation writes through the editor's own paths:
common_items.save_rows / write_rows (history with undo, "Zastosuj", the
client data zip) under ONE batch id, and the parts' write_custom for the
spool files (backups, pending). The import's record in
<spool>/dbeditor/imports/<batch>.json keeps the files' previous versions,
so "Cofnij import" undoes the whole import - database and files.
"""
import base64
import binascii
import difflib
import gzip
import hashlib
import io
import json
import os
import re
import time
import uuid
from datetime import datetime
from pathlib import Path

from flask import Response, flash, redirect, render_template, request, url_for

from dbeditor import common_items as common
from dbeditor import dropfiles as df

FORMAT_TITLE = "MT2009 PLUS - konfiguracja edytora bazy danych"
FORMAT_VERSION = 1
HEADER_RE = re.compile(r"^#?\s*MT2009 PLUS\s*[-–]\s*konfiguracja edytora bazy danych v(\d+)\s*$", re.I)
COMPACT_PREFIX = "MT2009CFG1:"
DISCORD_LIMIT = 2000
MAX_TEXT = 48 * 1024 * 1024          # the whole text, unpacked
MAX_BLOCK = 8 * 1024 * 1024          # one file block
MAX_RECORDS = 200000
SHOW_TEXT_MAX = 300 * 1024           # the page shows the text in a box up to this size
PREVIEW_ROWS = 400                   # rows of one part's table in the preview
IMPORT_KEEP = 60

# (part id, title, table) - the database parts, in the order of the text.
DB_PARTS = (
    ("items", "Przedmioty", "world.item_proto"),
    ("extra", "Dodatkowe bonusy przedmiotów (ponad 3)", "world.item_extra_apply"),
    ("skills", "Umiejętności", "world.skill_proto"),
    ("mobs", "Potwory i bossowie", "world.mob_proto"),
    ("shops", "Sklepy NPC – towary", "world.shop_item"),
    ("refine", "Ulepszanie u Kowala – przepisy", "world.refine_proto"),
    ("attrs", "Bonusy 1–5 (item_attr)", "world.item_attr"),
    ("attrs_rare", "Bonusy 6/7 (item_attr_rare)", "world.item_attr_rare"),
    ("exp", "Tabela doświadczenia", "common.exp_table"),
)
# The order an import writes them: refine clones before the items pointing at them.
APPLY_ORDER = ("refine", "items", "skills", "mobs", "attrs", "attrs_rare", "exp", "extra", "shops")
FILE_PARTS = (
    ("drops", "Drop potworów, zwykły i specjalny"),
    ("chests", "Szkatułki"),
    ("fishing", "Łowienie ryb"),
    ("spawns", "Respawn bossów i metinów"),
    ("regen", "Spawny potworów na mapach"),
)
PART_TITLES = {p[0]: p[1] for p in DB_PARTS + FILE_PARTS}
PART_TABLES = {p[0]: p[2] for p in DB_PARTS}
TABLE_PARTS = {p[2]: p[0] for p in DB_PARTS}
ALL_PARTS = tuple(p[0] for p in DB_PARTS + FILE_PARTS)

KEY_RE = re.compile(r"^[A-Za-z0-9_:.\-]{1,64}$")
COL_RE = re.compile(r"^(\*|[A-Za-z_][A-Za-z0-9_]{0,63})$")
SECTION_RE = re.compile(r"^\[([A-Za-z0-9_]+)\](?:\s+(\S+))?\s*(#.*)?$")
BLOCK_RE = re.compile(r"^<<<\s+(\S+)((?:\s+[a-z0-9_]+=\S*)*)\s*$")
_DECODER = json.JSONDecoder()

_CTX = {}


# ======================================================================
# values
# ======================================================================

def dump(value):
    """One value of the text: an int, a JSON string / object, "-" for none."""
    if value is None:
        return "-"
    if isinstance(value, bool):
        return str(int(value))
    if isinstance(value, int):
        return str(value)
    if isinstance(value, dict):
        return json.dumps(value, sort_keys=True, separators=(",", ":"))
    return json.dumps(str(value), ensure_ascii=False)


def _value_at(line, pos):
    """(value, next position) of a value starting at line[pos]."""
    if line.startswith("-", pos) and (pos + 1 == len(line) or line[pos + 1] in " \t"):
        return None, pos + 1
    value, end = _DECODER.raw_decode(line, pos)
    if isinstance(value, (list, float)) or isinstance(value, bool):
        raise ValueError("dozwolone są tylko liczby całkowite, teksty w cudzysłowie, wiersze {…} i „-”")
    return value, end


def parse_record(line):
    """'key col old -> new  # comment' -> dict (ValueError with the reason)."""
    match = re.match(r"(\S+)\s+(\S+)\s+", line)
    if not match:
        raise ValueError("oczekiwano: klucz kolumna oryginał -> nowa wartość")
    key, col = match.group(1), match.group(2)
    if not KEY_RE.match(key):
        raise ValueError(f"nieprawidłowy klucz „{key[:40]}”")
    if not COL_RE.match(col):
        raise ValueError(f"nieprawidłowa kolumna „{col[:40]}”")
    try:
        old, pos = _value_at(line, match.end())
        arrow = re.compile(r"\s*->\s*").match(line, pos)
        if not arrow:
            raise ValueError("brak „->” między oryginałem a nową wartością")
        new, pos = _value_at(line, arrow.end())
    except json.JSONDecodeError as exc:
        raise ValueError(f"wartość nie do odczytania ({exc.msg})")
    rest = line[pos:].strip()
    if rest and not rest.startswith("#"):
        raise ValueError(f"nadmiarowy tekst „{rest[:40]}”")
    comment = rest[1:].strip()
    return {"key": key, "col": col, "old": old, "new": new, "comment": comment,
            "label": comment.split(" · ", 1)[0].strip()[:100] if " · " in comment else ""}


def clean_comment(text):
    return re.sub(r"[\x00-\x1f]+", " ", str(text or "")).strip()


def plural(n, one, few, many):
    n = abs(int(n))
    if n == 1:
        return one
    if n % 10 in (2, 3, 4) and n % 100 not in (12, 13, 14):
        return few
    return many


# ======================================================================
# file blocks
# ======================================================================

def encode_block(data):
    """(attrs, lines) of a file's bytes - readable text when it can be one,
    base64 otherwise; the bytes come back exactly (decode_block)."""
    sha = hashlib.sha256(data).hexdigest()[:16]
    text, enc = None, None
    for candidate in ("ascii", "utf-8", "latin-1"):
        try:
            text = data.decode(candidate)
            enc = candidate
            break
        except UnicodeDecodeError:
            continue
    eol = "crlf" if "\r\n" in text else "lf"
    sep = "\r\n" if eol == "crlf" else "\n"
    body, final = (text[:-len(sep)], 1) if text.endswith(sep) else (text, 0)
    lines = body.split(sep) if (body or final) else []
    readable = (not any(("\r" in l or "\n" in l) for l in lines)
                and not re.search(r"[\x00-\x08\x0b\x0c\x0e-\x1f\x7f-\x9f]", body))
    if not readable:
        raw = base64.b64encode(data).decode("ascii")
        lines = [raw[i:i + 100] for i in range(0, len(raw), 100)]
        return {"enc": "base64", "linie": len(lines), "sha256": sha}, lines
    return {"enc": enc, "eol": eol, "nl": final, "linie": len(lines), "sha256": sha}, lines


def decode_block(attrs, lines):
    """bytes of a block (ValueError with the reason)."""
    enc = attrs.get("enc", "utf-8")
    if enc == "base64":
        try:
            return base64.b64decode("".join(l.strip() for l in lines), validate=True)
        except (binascii.Error, ValueError):
            raise ValueError("uszkodzony zapis base64")
    if enc not in ("ascii", "utf-8", "latin-1"):
        raise ValueError(f"nieznane kodowanie „{enc}”")
    sep = "\r\n" if attrs.get("eol", "lf") == "crlf" else "\n"
    text = sep.join(lines) + (sep if str(attrs.get("nl", "1")) == "1" and lines else "")
    try:
        return text.encode(enc)
    except UnicodeEncodeError:
        raise ValueError(f"znaki spoza kodowania {enc}")


# ======================================================================
# the parts kept in spool files
# ======================================================================

def _read(path):
    try:
        return Path(path).read_bytes()
    except (OSError, ValueError):
        return None


def _has_group(data):
    return bool(re.search(rb"(?im)^\s*group\s", data or b""))


class FilePart:
    """A part kept in spool files: keys, read, the image's copy, write."""

    def __init__(self, part):
        self.part = part

    def available(self):
        return True

    def keys(self, spool):
        return []

    def valid_key(self, key):
        return False

    def read(self, spool, key):
        return None

    def base(self, spool, key):
        return None

    def write(self, spool, key, data, reason):
        raise NotImplementedError

    def title(self, key):
        return key

    def effective(self, key, data):
        """False for a custom file that changes nothing (no group in it)."""
        return data is not None

    def check(self, spool, key, data, lookup):
        """(errors, warnings) of a file to import."""
        return [], []


class DropFiles(FilePart):
    KEYS = {"drops": ("mob", "common", "etc"), "chests": ("chest",), "fishing": ("fishing",)}

    def available(self):
        if self.part == "fishing":
            try:
                from dbeditor import fishing
                fishing.register_file()
            except ImportError:
                return False
        return True

    def keys(self, spool):
        return [k for k in self.KEYS[self.part] if k in df.FILES and df.custom_path(spool, k).exists()]

    def valid_key(self, key):
        return key in self.KEYS[self.part] and key in df.FILES

    def read(self, spool, key):
        return _read(df.custom_path(spool, key))

    def base(self, spool, key):
        data = _read(df.base_path(spool, key))
        if data is None and key == "chest":
            data = _read(df.CHEST_SNAPSHOT)
        if data is None and key == "fishing":
            from dbeditor import fishing
            data = _read(fishing.SNAPSHOT)
        return data

    def write(self, spool, key, data, reason):
        df.write_custom(spool, key, data, reason)

    def title(self, key):
        return df.FILES[key][3] if key in df.FILES else key

    def effective(self, key, data):
        if data is None:
            return False
        if key in ("mob", "chest"):
            return _has_group(data)
        if key == "fishing":
            from dbeditor import fishing
            return bool(fishing.parse_custom(data.decode("latin-1")))
        return True

    def check(self, spool, key, data, lookup):
        text = data.decode("latin-1")
        errors, warnings, items, mobs = [], [], set(), set()
        if key == "mob":
            groups = df.parse_mob_drop(text)
            if not groups and text.strip() and _has_group(data):
                errors.append("plik dropu bez żadnej poprawnej grupy")
            for g in groups:
                if g["type"] not in df.MOB_KINDS:
                    errors.append(f"grupa {g['name'][:30]}: nieznany rodzaj „{g['type'][:20]}”")
                if g["mob"]:
                    mobs.add(g["mob"])
                items.update(int(e["item"]) for e in g["items"] if e["item"].isdigit())
                if len(g["items"]) > df.MOB_MAX_LINES:
                    errors.append(f"grupa {g['name'][:30]}: więcej niż {df.MOB_MAX_LINES} pozycji")
        elif key == "common":
            table = df.parse_common(text)
            items.update(int(s["item"]) for rank in table["ranks"] for s in rank if s["active"])
        elif key == "etc":
            items.update(int(e["item"]) for e in df.parse_etc(text) if e["active"])
        elif key == "chest":
            groups = df.parse_chests(text)
            if not groups and _has_group(data):
                errors.append("plik szkatułek bez żadnej poprawnej grupy")
            for g in groups:
                if len(g["items"]) > df.CHEST_MAX_LINES:
                    errors.append(f"szkatułka {g['vnum']}: więcej niż {df.CHEST_MAX_LINES} pozycji")
                items.update(int(e["item"]) for e in g["items"] if e["item"].isdigit() and int(e["item"]) > 1)
        elif key == "fishing":
            from dbeditor import fishing
            entries = fishing.parse_custom(text)
            if not entries:
                errors.append("tabela łowienia bez żadnej pozycji")
            items.update(int(e["item"]) for e in entries if e["item"].isdigit())
            problems = fishing.validate(entries, None, dict(fishing.ROD_BONUS_DEFAULT))
            errors += [str(p) for p in problems[:10]]
        unknown_items = sorted(items - lookup("item", sorted(items))) if items else []
        unknown_mobs = sorted(mobs - lookup("mob", sorted(mobs))) if mobs else []
        if unknown_items:
            warnings.append("nieznane na tym serwerze przedmioty: " + ", ".join(map(str, unknown_items[:12]))
                            + (" …" if len(unknown_items) > 12 else ""))
        if unknown_mobs:
            warnings.append("nieznane na tym serwerze potwory: " + ", ".join(map(str, unknown_mobs[:12]))
                            + (" …" if len(unknown_mobs) > 12 else ""))
        return errors, warnings


class SpawnFiles(FilePart):
    def available(self):
        try:
            from dbeditor import spawnfiles  # noqa: F401
        except ImportError:
            return False
        return True

    def keys(self, spool):
        from dbeditor import spawnfiles as sf
        return sf.custom_keys(spool)

    def valid_key(self, key):
        from dbeditor import spawnfiles as sf
        if key == "special":
            return True
        parts = key.split(".")
        return len(parts) == 3 and parts[0] == "map" and bool(sf.DIR_RE.match(parts[1])) and parts[2] in sf.MAP_FILES

    def read(self, spool, key):
        from dbeditor import spawnfiles as sf
        return _read(sf.custom_path_of(spool, key))

    def base(self, spool, key):
        from dbeditor import spawnfiles as sf
        if key == "special":
            return _read(sf.base_path(spool, "special_spawns.txt"))
        _m, mapdir, kind = key.split(".", 2)
        return _read(sf.map_base_path(spool, mapdir, f"{kind}.txt"))

    def write(self, spool, key, data, reason):
        from dbeditor import spawnfiles as sf
        sf.write_custom(spool, key, data, reason)

    def title(self, key):
        from dbeditor import spawnfiles as sf
        return sf.key_title(key)

    def effective(self, key, data):
        if data is None:
            return False
        return _has_group(data) if key == "special" else True

    def check(self, spool, key, data, lookup):
        from dbeditor import spawnfiles as sf
        text = data.decode("latin-1")
        errors, warnings, mobs = [], [], set()
        if key == "special":
            groups = sf.parse_special(text)
            if not groups and _has_group(data):
                errors.append("plik bossów czasowych bez żadnej poprawnej grupy")
            for g in groups:
                if g["vnum"] is None:
                    errors.append(f"grupa {g['name'][:30]}: brak vnum")
                if g["spawn_type"] == "mob" and g["spawn_vnum"]:
                    mobs.add(g["spawn_vnum"])
        else:
            lines = sf.parse_regen(text)
            errors += sf.check_regen_lines(lines)[:10]
            mobs.update(l["row"]["vnum"] for l in lines if l["row"] and not l["row"].get("bad")
                        and l["row"]["type"] in ("m", "s"))
            _m, mapdir, _kind = key.split(".", 2)
            maps = sf.root(spool) / "base" / "maps"
            if maps.is_dir() and not (maps / mapdir).is_dir():
                errors.append(f"na tym serwerze nie ma mapy „{mapdir}”")
        unknown = sorted(mobs - lookup("mob", sorted(mobs))) if mobs else []
        if unknown:
            warnings.append("nieznane na tym serwerze potwory: " + ", ".join(map(str, unknown[:12])))
        return errors, warnings


class RegenFiles(FilePart):
    def available(self):
        try:
            from dbeditor import regen  # noqa: F401
        except ImportError:
            return False
        return True

    def keys(self, spool):
        from dbeditor import regen
        try:
            return sorted(p.name[:-4] for p in (regen.root(spool) / "custom").glob("*.txt")
                          if regen.FOLDER_RE.fullmatch(p.name[:-4]))
        except OSError:
            return []

    def valid_key(self, key):
        from dbeditor import regen
        return bool(regen.FOLDER_RE.fullmatch(key or ""))

    def read(self, spool, key):
        from dbeditor import regen
        return _read(regen.custom_path(spool, key))

    def base(self, spool, key):
        from dbeditor import regen
        return _read(regen.base_file(spool, key))

    def write(self, spool, key, data, reason):
        from dbeditor import regen
        regen.write_custom(spool, key, None if data is None else data.decode("latin-1"), reason)

    def title(self, key):
        return f"Spawny potworów: {key}"

    def check(self, spool, key, data, lookup):
        from dbeditor import regen
        doc = regen.parse(data.decode("latin-1"))
        errors = list(doc["problems"][:10])
        rows = [r for r in doc["rows"] if r["type"] != "e"]
        if doc["entries"] > regen.MAX_ROWS:
            errors.append(f"więcej niż {regen.MAX_ROWS} wpisów")
        maps = regen.base_dir(spool) / "maps"
        if maps.is_dir() and not (maps / key).is_dir():
            errors.append(f"na tym serwerze nie ma mapy „{key}”")
        mobs = {r["vnum"] for r in rows if r["type"] in ("m", "s")}
        warnings = []
        unknown = sorted(mobs - lookup("mob", sorted(mobs))) if mobs else []
        if unknown:
            warnings.append("nieznane na tym serwerze potwory: " + ", ".join(map(str, unknown[:12])))
        return errors, warnings


FILE_ADAPTERS = {"drops": DropFiles("drops"), "chests": DropFiles("chests"), "fishing": DropFiles("fishing"),
                 "spawns": SpawnFiles("spawns"), "regen": RegenFiles("regen")}


# ======================================================================
# the database
# ======================================================================

def _rows(sql, params=()):
    return common.ctx()["rows"](sql, params)


def _key_param(key):
    return int(key) if re.fullmatch(r"-?\d+", str(key)) else str(key)


def read_fields(table, keys, cols):
    """{row_key: {col: value}} of the rows that exist (in chunks)."""
    meta = common.TABLES[table]
    specs = meta["cols"]
    keycol = meta["key"]
    cols = [c for c in cols if c in specs or c == "type"]
    exprs = [common._select_expr(c, specs[c]) if c in specs else f"`{c}`" for c in cols]
    out = {}
    keys = sorted({str(k) for k in keys})
    for start in range(0, len(keys), 400):
        chunk = keys[start:start + 400]
        marks = ",".join(["%s"] * len(chunk))
        sql = (f"SELECT `{keycol}` AS dbe_key{''.join(', ' + e for e in exprs)} FROM {table} "
               f"WHERE `{keycol}` IN ({marks})")
        for row in _rows(sql, [_key_param(k) for k in chunk]):
            out[str(common.text_of(row["dbe_key"]))] = row
    return out


def whole_row(table, key):
    current = common.read_whole_row(table, key)
    if current is None:
        return None
    return {c: int(current.get(c) or 0) for c in common.TABLES[table]["row_cols"]}


def existing(table, keycol, keys):
    """The keys (ints) of the table that exist."""
    keys = sorted({int(k) for k in keys})
    found = set()
    for start in range(0, len(keys), 500):
        chunk = keys[start:start + 500]
        marks = ",".join(["%s"] * len(chunk))
        for row in _rows(f"SELECT `{keycol}` AS k FROM {table} WHERE `{keycol}` IN ({marks})", chunk):
            found.add(int(row["k"]))
    return found


def make_lookup():
    """lookup(kind, vnums) -> the set of those that exist ('item' / 'mob' /
    'shop'), cached for one request; on a database error everything counts
    as known (the game checks again when it boots)."""
    cache = {}
    tables = {"item": ("world.item_proto", "vnum"), "mob": ("world.mob_proto", "vnum"), "shop": ("world.shop", "vnum")}

    def lookup(kind, vnums):
        vnums = {int(v) for v in vnums}
        known = cache.setdefault(kind, {})
        missing = sorted(v for v in vnums if v not in known)
        if missing:
            try:
                found = existing(*tables[kind], missing)
            except Exception:
                found = set(missing)
            for v in missing:
                known[v] = v in found
        return {v for v in vnums if known.get(v)}
    return lookup


def _text_value(spec, value):
    if value is None:
        return None
    if spec and spec["kind"] == "int":
        try:
            return int(value)
        except (TypeError, ValueError):
            return common.text_of(value)
    return common.text_of(value)


# ======================================================================
# export
# ======================================================================

def server_version():
    getter = common.ctx().get("server_version")
    if callable(getter):
        try:
            value = str(getter() or "").strip()
            if value:
                return value
        except Exception:
            pass
    try:
        return (Path(__file__).resolve().parent.parent / "VERSION").read_text(encoding="utf-8-sig").strip() or "?"
    except OSError:
        return "?"


def panel_name():
    getter = common.ctx().get("panel_name")
    try:
        return str(getter() if callable(getter) else (getter or "")).strip()
    except Exception:
        return ""


def collect(parts=None, spool=None):
    """{part: {"records": [...], "files": [...]}} - the net changes now."""
    wanted = set(parts or ALL_PARTS)
    out = {}
    common.ensure_table()
    _rows(f"SELECT id FROM {common.HISTORY_TABLE} LIMIT 1")  # a database error is shown, not "no changes"
    net = common.net_changes(include_applied=True)
    by_table = {}
    for row in net:
        by_table.setdefault(row["tbl"], []).append(row)
    for part, _title, table in DB_PARTS:
        if part not in wanted or table not in common.TABLES or table not in by_table:
            continue
        meta = common.TABLES[table]
        records = []
        fields = [r for r in by_table[table] if r["col"] != common.ROW_COL and r["col"] in meta["cols"]]
        current = read_fields(table, {r["row_key"] for r in fields}, {r["col"] for r in fields}) if fields else {}
        for r in fields:
            spec = meta["cols"][r["col"]]
            if r["row_key"] not in current:
                continue  # the row is gone (a removed clone): nothing to set
            now = current[r["row_key"]][r["col"]]
            if common._same(spec, now, r["old_value"]):
                continue  # back to the stock value
            records.append({"key": r["row_key"], "col": r["col"], "old": _text_value(spec, r["old_value"]),
                            "new": _text_value(spec, now), "label": r["label"] or ""})
        if meta.get("row_cols"):
            for r in by_table[table]:
                if r["col"] != common.ROW_COL:
                    continue
                try:
                    now = whole_row(table, r["row_key"])
                    old = json.loads(r["old_value"]) if r["old_value"] else None
                except (ValueError, TypeError):
                    continue
                if (now or None) == (old or None):
                    continue
                records.append({"key": r["row_key"], "col": common.ROW_COL, "old": old, "new": now,
                                "label": r["label"] or ""})
        records.sort(key=lambda rec: (_sort_key(rec["key"]), rec["col"] != common.ROW_COL, rec["col"]))
        if records:
            out[part] = {"records": records, "files": []}
    spool = spool or df.spool_dir()
    for part, _title in FILE_PARTS:
        adapter = FILE_ADAPTERS[part]
        if part not in wanted or not adapter.available():
            continue
        files = []
        for key in adapter.keys(spool):
            data = adapter.read(spool, key)
            if not adapter.effective(key, data) or data == adapter.base(spool, key):
                continue
            files.append({"key": key, "data": data, "title": adapter.title(key)})
        if files:
            out[part] = {"records": [], "files": files}
    return out


def _sort_key(key):
    return tuple((0, int(p), "") if p.isdigit() else (1, 0, p) for p in str(key).split(":"))


def summary_of(part, content):
    records, files = content.get("records", []), content.get("files", [])
    if files:
        return f"{len(files)} {plural(len(files), 'plik', 'pliki', 'plików')}: " + ", ".join(f["key"] for f in files[:6]) \
            + (" …" if len(files) > 6 else "")
    fields = [r for r in records if r["col"] != common.ROW_COL]
    rows = [r for r in records if r["col"] == common.ROW_COL]
    bits = []
    if fields:
        keys = len({r["key"] for r in fields})
        bits.append(f"{len(fields)} {plural(len(fields), 'pole', 'pola', 'pól')} w {keys} "
                    f"{plural(keys, 'wierszu', 'wierszach', 'wierszach')}")
    if rows:
        added = sum(1 for r in rows if r["old"] is None and r["new"] is not None)
        removed = sum(1 for r in rows if r["new"] is None and r["old"] is not None)
        changed = len(rows) - added - removed
        bits.append(", ".join(t for t in (f"+{added} nowych" if added else "", f"-{removed} usuniętych" if removed else "",
                                          f"{changed} zmienionych" if changed else "") if t) + " wierszy")
    return "; ".join(bits)


def render_text(content, now=None):
    now = now or datetime.now()
    lines = [f"# {FORMAT_TITLE} v{FORMAT_VERSION}",
             f"# serwer: {server_version()}",
             f"# data: {now:%Y-%m-%d %H:%M}"]
    name = clean_comment(panel_name())
    if name:
        lines.append(f"# nazwa: {name}")
    lines += ["# Tylko zmiany z edytora względem gry z obrazu: klucz, kolumna, oryginał -> teraz",
              "# („-” = brak wartości / brak wiersza). Pliki: cały plik zmian panelu jako blok.",
              "# Import: panel Seban → Edytor bazy danych → Eksport / import konfiguracji.",
              "#", "# Podsumowanie:"]
    if not content:
        lines.append("#   (brak zmian – gra działa na danych z obrazu)")
    for part in ALL_PARTS:
        if part in content:
            lines.append(f"#   {part:<11} {PART_TITLES[part]}: {summary_of(part, content[part])}")
    for part, title, table in DB_PARTS:
        if part not in content:
            continue
        lines += ["", f"[{part}] {table}", f"# {title}"]
        for rec in content[part]["records"]:
            label = clean_comment(rec.get("label"))
            col_label = clean_comment(common.column_label(table, rec["col"]))
            comment = " · ".join(t for t in (label, col_label) if t)
            lines.append(f"{rec['key']}\t{rec['col']}\t{dump(rec['old'])} -> {dump(rec['new'])}"
                         + (f"\t# {comment}" if comment else ""))
    for part, title in FILE_PARTS:
        if part not in content:
            continue
        lines += ["", f"[{part}]", f"# {title}"]
        for f in content[part]["files"]:
            attrs, body = encode_block(f["data"])
            lines.append(f"# {clean_comment(f['title'])}")
            lines.append(f"<<< {f['key']} " + " ".join(f"{k}={v}" for k, v in attrs.items()))
            lines += ["|" + l for l in body]
            lines.append(f">>> {f['key']}")
    lines += ["", "# koniec"]
    return "\n".join(lines) + "\n"


def compact(text):
    packed = gzip.compress(text.encode("utf-8"), 9, mtime=0)
    return COMPACT_PREFIX + base64.b64encode(packed).decode("ascii")


# ======================================================================
# parsing
# ======================================================================

class ConfigError(ValueError):
    pass


def unpack(raw):
    """The text of a pasted / uploaded config: code fences and a BOM gone,
    the compact form unpacked."""
    if isinstance(raw, bytes):
        if raw[:2] == b"\x1f\x8b":
            raw = _gunzip(raw)
        try:
            raw = raw.decode("utf-8-sig")
        except UnicodeDecodeError:
            raise ConfigError("Plik nie jest tekstem UTF-8.")
    text = raw.lstrip("﻿").strip()
    text = re.sub(r"^```[A-Za-z0-9_-]*[ \t]*\n?", "", text)
    text = re.sub(r"\n?```\s*$", "", text).strip()
    squeezed = re.sub(r"[\s`]+", "", text)
    if squeezed.startswith(COMPACT_PREFIX):
        try:
            packed = base64.b64decode(squeezed[len(COMPACT_PREFIX):], validate=True)
        except (binascii.Error, ValueError):
            raise ConfigError("Skrót jest uszkodzony (base64) – skopiuj go jeszcze raz w całości.")
        try:
            text = _gunzip(packed).decode("utf-8")
        except UnicodeDecodeError:
            raise ConfigError("Skrót jest uszkodzony (tekst).")
    if len(text) > MAX_TEXT:
        raise ConfigError(f"Konfiguracja jest za duża (ponad {MAX_TEXT // (1024 * 1024)} MB).")
    return text


def _gunzip(packed):
    try:
        with gzip.GzipFile(fileobj=io.BytesIO(packed)) as handle:
            data = handle.read(MAX_TEXT + 1)
    except (OSError, EOFError, ValueError):
        raise ConfigError("Skrót jest uszkodzony (gzip) – skopiuj go jeszcze raz w całości.")
    if len(data) > MAX_TEXT:
        raise ConfigError(f"Konfiguracja jest za duża (ponad {MAX_TEXT // (1024 * 1024)} MB).")
    return data


def parse(raw):
    """{"meta", "parts": {part: {"records", "files", "errors"}}, "warnings"}.
    A text that is not a config of a known version raises ConfigError."""
    text = unpack(raw)
    lines = text.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    first = next((i for i, l in enumerate(lines) if l.strip()), None)
    if first is None:
        raise ConfigError("Pusty tekst – wklej konfigurację albo wybierz plik.")
    match = HEADER_RE.match(lines[first].strip())
    if not match:
        raise ConfigError(f"To nie jest konfiguracja edytora – pierwsza linia musi brzmieć „# {FORMAT_TITLE} "
                          f"v{FORMAT_VERSION}”.")
    version = int(match.group(1))
    if version != FORMAT_VERSION:
        raise ConfigError(f"Konfiguracja w wersji v{version} – ten panel zna tylko v{FORMAT_VERSION}"
                          + (" (zaktualizuj panel)." if version > FORMAT_VERSION else "."))
    meta, parts, warnings = {"version": version}, {}, []
    part, block, records = None, None, 0
    seen = {}
    for number, raw_line in enumerate(lines[first + 1:], first + 2):
        line = raw_line.rstrip()
        if block is not None:
            if line.startswith("|"):
                block["lines"].append(raw_line[1:])
                block["size"] += len(raw_line)
                if block["size"] > MAX_BLOCK:
                    raise ConfigError(f"linia {number}: plik „{block['key']}” jest za duży "
                                      f"(ponad {MAX_BLOCK // (1024 * 1024)} MB).")
            elif line.strip().startswith(">>>"):
                _finish_block(parts[part], block)
                block = None
            elif line.strip():
                parts[part]["errors"].append(f"linia {number}: w bloku pliku „{block['key']}” każda linia musi "
                                             "zaczynać się od „|” (blok bez „>>>”?)")
                block = None
            continue
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("#"):
            info = re.match(r"#\s*(serwer|data|nazwa):\s*(.*)$", stripped)
            if info and part is None:
                meta[info.group(1)] = info.group(2).strip()[:80]
            continue
        section = SECTION_RE.match(stripped)
        if section:
            name = section.group(1).lower()
            if name not in PART_TITLES:
                warnings.append(f"linia {number}: nieznana część „{name[:30]}” – pominięta "
                                "(konfiguracja z nowszego panelu?)")
                part = "_unknown"
                parts.setdefault(part, {"records": [], "files": [], "errors": []})
                continue
            if name in PART_TABLES and section.group(2) and section.group(2) != PART_TABLES[name]:
                parts.setdefault(name, {"records": [], "files": [], "errors": []})["errors"].append(
                    f"linia {number}: część [{name}] to tabela {PART_TABLES[name]}, a nie {section.group(2)[:40]}")
            part = name
            parts.setdefault(part, {"records": [], "files": [], "errors": []})
            continue
        if part is None:
            raise ConfigError(f"linia {number}: wpis przed pierwszą częścią [nazwa].")
        if part == "_unknown":
            continue
        if stripped.startswith("<<<"):
            if part in PART_TABLES:
                parts[part]["errors"].append(f"linia {number}: blok pliku w części tabeli [{part}]")
                continue
            head = BLOCK_RE.match(stripped)
            if not head:
                parts[part]["errors"].append(f"linia {number}: nieprawidłowy nagłówek bloku pliku")
                continue
            attrs = dict(a.split("=", 1) for a in head.group(2).split())
            block = {"key": head.group(1), "attrs": attrs, "lines": [], "size": 0, "line": number}
            continue
        if part not in PART_TABLES:
            parts[part]["errors"].append(f"linia {number}: w części [{part}] mogą być tylko bloki plików")
            continue
        records += 1
        if records > MAX_RECORDS:
            raise ConfigError(f"Za dużo wpisów (ponad {MAX_RECORDS}).")
        try:
            rec = parse_record(stripped)
        except ValueError as exc:
            parts[part]["errors"].append(f"linia {number}: {exc}")
            continue
        rec["line"] = number
        ident = (part, rec["key"], rec["col"])
        if ident in seen:
            warnings.append(f"linia {number}: {rec['key']} {rec['col']} powtórzone (linia {seen[ident]}) – "
                            "liczy się ostatni wpis")
            parts[part]["records"] = [r for r in parts[part]["records"] if (r["key"], r["col"]) != ident[1:]]
        seen[ident] = number
        parts[part]["records"].append(rec)
    if block is not None:
        parts[part]["errors"].append(f"linia {block['line']}: blok pliku „{block['key']}” bez zakończenia „>>>” "
                                     "(tekst ucięty?)")
    parts.pop("_unknown", None)
    return {"meta": meta, "parts": parts, "warnings": warnings, "text": text}


def _finish_block(target, block):
    try:
        data = decode_block(block["attrs"], block["lines"])
    except ValueError as exc:
        target["errors"].append(f"linia {block['line']}: plik „{block['key']}”: {exc}")
        return
    sha = block["attrs"].get("sha256")
    target["files"] = [f for f in target["files"] if f["key"] != block["key"]]
    target["files"].append({"key": block["key"], "data": data, "line": block["line"],
                            "sha_ok": not sha or hashlib.sha256(data).hexdigest()[:len(sha)] == sha})


# ======================================================================
# the plan: what an import would change on this server
# ======================================================================

def _row_checks(table, key, new, lookup):
    """Errors of a whole row to add (beyond the column types)."""
    meta = common.TABLES[table]
    errors = []
    if set(new) != set(meta["row_cols"]):
        return [f"wiersz musi mieć dokładnie kolumny {', '.join(meta['row_cols'])}"]
    for col, value in new.items():
        if not isinstance(value, int) or isinstance(value, bool):
            return [f"kolumna {col}: oczekiwano liczby całkowitej"]
    if common.row_key_of(table, new) != key:
        return [f"klucz {key} nie pasuje do wiersza ({common.row_key_of(table, new)})"]
    for col, spec in meta["cols"].items():
        if col in new and spec.get("kind") == "int":
            _value, error = common.validate(spec, str(new[col]), common.column_label(table, col))
            if error:
                errors.append(error)
    part = TABLE_PARTS.get(table)
    if part == "shops":
        if not 1 <= new["count"] <= 65535:
            errors.append(f"ilość {new['count']} poza 1…65535")
    if part == "extra":
        if not 1 <= new["slot"] <= 255:
            errors.append(f"pozycja {new['slot']} poza 1…255")
    if part == "refine":
        if not 1 <= new["id"] <= 65535:
            errors.append(f"numer przepisu {new['id']} poza 1…65535")
    return errors


def _row_warnings(table, new, lookup):
    part = TABLE_PARTS.get(table)
    if not new:
        return []
    if part == "shops":
        out = []
        if not lookup("shop", [new["shop_vnum"]]):
            out.append(f"na tym serwerze nie ma sklepu {new['shop_vnum']}")
        if not lookup("item", [new["item_vnum"]]):
            out.append(f"na tym serwerze nie ma przedmiotu {new['item_vnum']}")
        return out
    if part == "extra" and not lookup("item", [new["vnum"]]):
        return [f"na tym serwerze nie ma przedmiotu {new['vnum']}"]
    return []


def _boot_note(table, key, col, current_row):
    """apply.sh puts this field back at every start (items, monsters)."""
    try:
        if table == "world.item_proto":
            from dbeditor import protected
            item_type = current_row.get("type") if current_row else None
            if col in protected.overwritten_cols(int(key), item_type):
                return "apply.sh ustawia to pole przy każdym starcie – zmiana zniknie po restarcie"
        if table == "world.mob_proto":
            from dbeditor import mobs
            if col in mobs.boot_cols(int(key)):
                return "apply.sh ustawia to pole przy każdym starcie – zmiana zniknie po restarcie"
    except (ImportError, ValueError, TypeError):
        pass
    return ""


NAME_SQL = {"world.item_proto": "SELECT vnum AS k, CAST(locale_name AS BINARY) AS n FROM world.item_proto WHERE vnum IN ({})",
            "world.mob_proto": "SELECT vnum AS k, CAST(locale_name AS BINARY) AS n FROM world.mob_proto WHERE vnum IN ({})",
            "world.skill_proto": "SELECT dwVnum AS k, szName AS n FROM world.skill_proto WHERE dwVnum IN ({})"}


def fill_names(table, changes):
    """The item's / monster's / skill's name for the rows the config gives
    no label for (shown in the preview, kept in the history)."""
    keys = sorted({int(c["key"]) for c in changes if not c["label"] and c["key"].isdigit()})
    if table not in NAME_SQL or not keys:
        return
    names = {}
    try:
        for start in range(0, len(keys), 500):
            chunk = keys[start:start + 500]
            for row in _rows(NAME_SQL[table].format(",".join(["%s"] * len(chunk))), chunk):
                names[str(int(row["k"]))] = clean_comment(common.text_of(row["n"]))[:100]
    except Exception:
        return
    for change in changes:
        if not change["label"]:
            change["label"] = names.get(change["key"], "")


def _creatable(table, key):
    """Rows an import may add before setting fields (as the editor's pages do):
    the bonus pools' inert row, an exp table level with the built-in value."""
    part = TABLE_PARTS.get(table)
    if part in ("attrs", "attrs_rare"):
        from dbeditor import attrs
        if key in attrs.POINT_NAMES and (attrs.POINT_NAMES.index(key) + 1) not in attrs.TECHNICAL_POINTS:
            return {"apply": key, "prob": 0}
    if part == "exp" and key.isdigit():
        from dbeditor import exptable
        if 1 <= int(key) <= exptable.MAX_LEVEL:
            return {"level": int(key), "exp": exptable.builtin(int(key))}
    return None


def plan_db(part, content, lookup, skip_conflicts=False):
    table = PART_TABLES[part]
    result = {"part": part, "title": PART_TITLES[part], "table": table, "kind": "db", "changes": [],
              "errors": list(content.get("errors", [])), "warnings": [], "unchanged": 0, "skipped": 0,
              "creates": [], "inserts": [], "deletes": [], "updates": {}}
    meta = common.TABLES.get(table)
    if meta is None:
        result["errors"].append("ta część edytora nie jest zainstalowana w tym panelu")
        return result
    records = content.get("records", [])
    if content.get("files"):
        result["errors"].append("bloki plików w części tabeli")
    row_recs = [r for r in records if r["col"] == common.ROW_COL]
    field_recs = [r for r in records if r["col"] != common.ROW_COL]
    planned_rows = {}
    # whole rows (shop goods, refine clones, extra bonus lines)
    if row_recs and not meta.get("row_cols"):
        result["errors"].append(f"tabela {table} nie przyjmuje całych wierszy („*”)")
        row_recs = []
    for rec in row_recs:
        where = f"linia {rec['line']}: {rec['key']}"
        if not re.fullmatch(r"\d+(:\d+)*", rec["key"]) or len(rec["key"].split(":")) != len(common.key_cols(table)):
            result["errors"].append(f"{where}: klucz musi mieć postać {':'.join(common.key_cols(table))}")
            continue
        new, old = rec["new"], rec["old"]
        bad = [x for x in (new, old) if x is not None and not isinstance(x, dict)]
        if bad:
            result["errors"].append(f"{where}: cały wiersz to {{…}} albo „-”")
            continue
        if new is not None:
            errors = _row_checks(table, rec["key"], new, lookup)
            if errors:
                result["errors"] += [f"{where}: {e}" for e in errors]
                continue
        try:
            current = whole_row(table, rec["key"])
        except Exception as exc:
            result["errors"].append(f"{where}: nie udało się odczytać wiersza ({exc})")
            continue
        if current == new:
            result["unchanged"] += 1
            continue
        problems = _row_warnings(table, new, lookup)
        if problems:
            result["warnings"].append(f"{where}: " + "; ".join(problems) + " – pominięto")
            result["skipped"] += 1
            continue
        conflict = current != old
        if conflict and skip_conflicts:
            result["skipped"] += 1
            continue
        change = {"key": rec["key"], "col": common.ROW_COL, "col_label": common.column_label(table, common.ROW_COL),
                  "label": rec["label"], "stock": old, "current": current, "new": new, "conflict": conflict,
                  "note": ""}
        result["changes"].append(change)
        if current is not None:
            result["deletes"].append(common.key_values(table, rec["key"]))
        if new is not None:
            result["inserts"].append(dict(new))
            planned_rows[rec["key"]] = new
    # shop windows: at most 40 lines a shop after the import
    if part == "shops" and (result["inserts"] or result["deletes"]):
        from dbeditor import shops
        touched = {r["shop_vnum"] for r in result["inserts"] + result["deletes"]}
        for shop in sorted(touched):
            try:
                have = {(int(r["item_vnum"]), int(r["count"])) for r in
                        _rows("SELECT item_vnum, `count` FROM world.shop_item WHERE shop_vnum=%s", (shop,))}
            except Exception:
                continue
            have -= {(d["item_vnum"], d["count"]) for d in result["deletes"] if d["shop_vnum"] == shop}
            have |= {(i["item_vnum"], i["count"]) for i in result["inserts"] if i["shop_vnum"] == shop}
            if len(have) > shops.MAX_LINES:
                result["warnings"].append(f"sklep {shop}: po imporcie miałby {len(have)} pozycji (najwyżej "
                                          f"{shops.MAX_LINES}) – jego zmiany pominięto")
                result["skipped"] += sum(1 for c in result["changes"] if c["key"].split(":")[0] == str(shop))
                result["changes"] = [c for c in result["changes"] if c["key"].split(":")[0] != str(shop)]
                result["inserts"] = [i for i in result["inserts"] if i["shop_vnum"] != shop]
                result["deletes"] = [d for d in result["deletes"] if d["shop_vnum"] != shop]
    # fields
    keys = {r["key"] for r in field_recs}
    cols = {r["col"] for r in field_recs if r["col"] in meta["cols"]}
    if table == "world.item_proto":
        cols.add("type")
    try:
        current_rows = read_fields(table, keys, cols) if keys and cols else {}
    except Exception as exc:
        result["errors"].append(f"nie udało się odczytać tabeli {table}: {exc}")
        return result
    for rec in field_recs:
        where = f"linia {rec['line']}: {rec['key']} · {rec['col']}"
        spec = meta["cols"].get(rec["col"])
        if spec is None:
            result["errors"].append(f"{where}: tej kolumny edytor nie zmienia")
            continue
        label = common.column_label(table, rec["col"])
        if rec["new"] is None or isinstance(rec["new"], dict):
            result["errors"].append(f"{where}: brak nowej wartości")
            continue
        value, error = common.validate(spec, str(rec["new"]), label)
        if error:
            result["errors"].append(f"linia {rec['line']}: {rec['key']} · {error}")
            continue
        row = current_rows.get(rec["key"])
        create = None
        if row is None:
            if rec["key"] in planned_rows:
                row = dict(planned_rows[rec["key"]])
            else:
                create = _creatable(table, rec["key"])
                if create is None:
                    result["warnings"].append(f"{where}: {meta['title']} „{rec['key']}” nie istnieje na tym "
                                              "serwerze – pominięto")
                    result["skipped"] += 1
                    continue
                row = dict({c: 0 for c in cols}, **create)
        current = row.get(rec["col"])
        if common._same(spec, current, value):
            result["unchanged"] += 1
            continue
        stock = rec["old"]
        conflict = stock is not None and not common._same(spec, current, stock)
        if conflict and skip_conflicts:
            result["skipped"] += 1
            continue
        if create is not None and rec["key"] not in [c["key"] for c in result["creates"]]:
            result["creates"].append({"key": rec["key"], "values": create})
        result["updates"].setdefault(rec["key"], {})[rec["col"]] = value
        result["changes"].append({"key": rec["key"], "col": rec["col"], "col_label": label, "label": rec["label"],
                                  "stock": stock, "current": _text_value(spec, current),
                                  "new": value, "conflict": conflict,
                                  "note": _boot_note(table, rec["key"], rec["col"], row)})
    fill_names(table, result["changes"])
    conflicts = sum(1 for c in result["changes"] if c["conflict"])
    if conflicts:
        result["warnings"].append(f"{conflicts} {plural(conflicts, 'pole/wiersz', 'pola/wiersze', 'pól/wierszy')} "
                                  "ma tu inną wartość niż oryginał z konfiguracji (zmienione już na tym serwerze) – "
                                  "import je nadpisze")
    boot = sum(1 for c in result["changes"] if c["note"])
    if boot:
        result["warnings"].append(f"{boot} {plural(boot, 'pole', 'pola', 'pól')} ustawia apply.sh przy każdym "
                                  "starcie serwera – ta zmiana zniknie po restarcie")
    return result


def plan_files(part, content, spool, lookup):
    adapter = FILE_ADAPTERS[part]
    result = {"part": part, "title": PART_TITLES[part], "kind": "files", "changes": [],
              "errors": list(content.get("errors", [])), "warnings": [], "unchanged": 0, "skipped": 0}
    if content.get("records"):
        result["errors"].append("wpisy tabel w części plików")
    if not adapter.available():
        result["errors"].append("ta część edytora nie jest zainstalowana w tym panelu")
        return result
    for f in content.get("files", []):
        key, data = f["key"], f["data"]
        where = f"plik „{key}”"
        if not adapter.valid_key(key):
            result["errors"].append(f"{where}: nieznany plik tej części")
            continue
        if not f["sha_ok"]:
            result["warnings"].append(f"{where}: suma kontrolna się nie zgadza – plik zmieniono ręcznie albo "
                                      "uszkodzono przy kopiowaniu; sprawdź podgląd")
        errors, warnings = adapter.check(spool, key, data, lookup)
        if errors:
            result["errors"] += [f"{where}: {e}" for e in errors]
            continue
        if warnings:
            result["warnings"] += [f"{where}: {w} – gra odrzuciłaby ten plik, pominięto" for w in warnings]
            result["skipped"] += 1
            continue
        current = adapter.read(spool, key)
        base = adapter.base(spool, key)
        live = current if adapter.effective(key, current) else None
        target = None if (base is not None and data == base) else data
        if (live if live is not None else base) == data or (live is None and target is None):
            result["unchanged"] += 1
            continue
        before_text = (live if live is not None else (base or b"")).decode("latin-1").replace("\r\n", "\n").split("\n")
        after_text = data.decode("latin-1").replace("\r\n", "\n").split("\n")
        diff = [l for l in difflib.unified_diff(before_text, after_text, lineterm="", n=0)
                if not l.startswith(("---", "+++", "@@"))]
        result["changes"].append({"key": key, "title": adapter.title(key), "data": target,
                                  "source": "zmiany panelu" if live is not None else "plik z obrazu gry",
                                  "added": sum(1 for l in diff if l.startswith("+")),
                                  "removed": sum(1 for l in diff if l.startswith("-")),
                                  "diff": diff[:80], "diff_more": max(0, len(diff) - 80)})
    return result


def plan(config, parts=None, skip_conflicts=False, spool=None):
    spool = spool or df.spool_dir()
    lookup = make_lookup()
    out = []
    for part in ALL_PARTS:
        content = config["parts"].get(part)
        if content is None or (parts is not None and part not in parts):
            continue
        if part in PART_TABLES:
            out.append(plan_db(part, content, lookup, skip_conflicts))
        else:
            out.append(plan_files(part, content, spool, lookup))
    return out


# ======================================================================
# applying, undo
# ======================================================================

def imports_dir(spool):
    return Path(spool) / "dbeditor" / "imports"


def _write_record(spool, record):
    folder = imports_dir(spool)
    df._prepare_folder(folder.parent)
    df._prepare_folder(folder)
    temporary = folder / f".{record['batch']}.json.new{os.getpid()}"
    temporary.write_text(json.dumps(record, ensure_ascii=False), encoding="utf-8")
    os.replace(temporary, folder / f"{record['batch']}.json")
    for old in sorted(folder.glob("*.json"), key=lambda p: p.stat().st_mtime)[:-IMPORT_KEEP]:
        try:
            old.unlink()
        except OSError:
            pass


def load_record(spool, batch):
    if not re.fullmatch(r"[0-9a-f]{16}", batch or ""):
        return None
    try:
        return json.loads((imports_dir(spool) / f"{batch}.json").read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None


def list_records(spool, limit=20):
    out = []
    try:
        paths = sorted(imports_dir(spool).glob("*.json"), key=lambda p: p.stat().st_mtime, reverse=True)
    except OSError:
        return []
    for path in paths[:limit]:
        try:
            record = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            continue
        record["when"] = datetime.fromtimestamp(record.get("time") or 0).strftime("%d.%m.%Y %H:%M")
        out.append(record)
    return out


def _history_count(batch):
    found = _rows(f"SELECT COUNT(*) AS n FROM {common.HISTORY_TABLE} WHERE batch=%s AND reverted_in IS NULL", (batch,))
    return int((found[0] if found else {}).get("n") or 0)


def apply(plans, meta, spool=None):
    """Writes the plans (one batch). Returns the import's record. On an
    error everything written so far is undone and the error raised."""
    spool = spool or df.spool_dir()
    batch = uuid.uuid4().hex[:16]
    note = f"Import konfiguracji #{batch[:8]}" + (f" (serwer {meta.get('serwer')})" if meta.get("serwer") else "")
    by_part = {p["part"]: p for p in plans if not p["errors"]}
    files_done, counts = [], {}
    try:
        for part in APPLY_ORDER:
            p = by_part.get(part)
            if not p or not p["changes"]:
                continue
            table = p["table"]
            labels = {}
            for c in p["changes"]:
                labels[c["key"]] = labels.get(c["key"]) or c["label"]
            label_of = lambda key, labels=labels: labels.get(str(key), "")
            if part == "extra":
                from dbeditor import items
                items.ensure_extra_table(_rows)
            if p["inserts"] or p["deletes"]:
                common.write_rows(table, inserts=p["inserts"], deletes=p["deletes"], note=note, label_of=label_of,
                                  batch=batch)
            for create in p["creates"]:
                cols = list(create["values"])
                _rows(f"INSERT INTO {table} ({', '.join('`%s`' % c for c in cols)}) "
                      f"VALUES ({', '.join(['%s'] * len(cols))})", [create["values"][c] for c in cols])
            if p["updates"]:
                common.save_rows(table, [(_key_param(k), v) for k, v in p["updates"].items()], note=note,
                                 label_of=label_of, batch=batch)
            counts[part] = len(p["changes"])
        for part, _title in FILE_PARTS:
            p = by_part.get(part)
            if not p or not p["changes"]:
                continue
            adapter = FILE_ADAPTERS[part]
            for change in p["changes"]:
                before = adapter.read(spool, change["key"])
                adapter.write(spool, change["key"], change["data"], note)
                after = adapter.read(spool, change["key"])
                files_done.append({"part": part, "key": change["key"], "title": change["title"],
                                   "before": base64.b64encode(before).decode("ascii") if before is not None else None,
                                   "after_sha": hashlib.sha256(after).hexdigest() if after is not None else None})
            counts[part] = len(p["changes"])
    except Exception:
        _rollback(batch, files_done, spool, note)
        raise
    record = {"batch": batch, "time": int(time.time()), "who": common.who(), "source": meta,
              "counts": counts, "fields": sum(len(p["changes"]) for p in by_part.values() if p["kind"] == "db"),
              "files": files_done, "reverted_in": None}
    if counts:
        _write_record(spool, record)
    return record


def _rollback(batch, files_done, spool, note):
    try:
        if _history_count(batch):
            common.revert(batch=batch, force=True)
    except Exception:
        pass
    for f in reversed(files_done):
        try:
            before = base64.b64decode(f["before"]) if f["before"] else None
            FILE_ADAPTERS[f["part"]].write(spool, f["key"], before, "wycofano nieudany import")
        except Exception:
            pass


def undo(batch, force=False, spool=None):
    """(ok, message) - the whole import back: its history batch (common_items.
    revert) and its files (the versions from before the import)."""
    spool = spool or df.spool_dir()
    record = load_record(spool, batch)
    if record is None:
        return False, "Nie ma takiego importu."
    if record.get("reverted_in"):
        return False, "Ten import został już cofnięty."
    conflicts = []
    for f in record.get("files", []):
        now = FILE_ADAPTERS[f["part"]].read(spool, f["key"])
        sha = hashlib.sha256(now).hexdigest() if now is not None else None
        if sha != f.get("after_sha"):
            conflicts.append(f"{f['title']}: plik zmieniono po imporcie")
    if conflicts and not force:
        return False, ("Po imporcie zmieniono jeszcze: " + "; ".join(conflicts[:5])
                       + ". Cofnięcie nadpisałoby te zmiany – zaznacz „cofnij mimo to”, jeśli tego chcesz.")
    new_batch = None
    if _history_count(batch):
        new_batch, message, ok = common.revert(batch=batch, force=force)
        if not ok:
            return False, message
    note = f"Cofnięcie importu #{batch[:8]}"
    for f in record.get("files", []):
        before = base64.b64decode(f["before"]) if f.get("before") else None
        FILE_ADAPTERS[f["part"]].write(spool, f["key"], before, note)
    record["reverted_in"] = new_batch or "pliki"
    record["reverted_at"] = int(time.time())
    _write_record(spool, record)
    return True, (f"Cofnięto import #{batch[:8]}. Zmiany czekają na zastosowanie (restart gry – „Zastosuj”).")


# ======================================================================
# the page
# ======================================================================

def _selected_parts(form_or_args):
    chosen = [p for p in form_or_args.getlist("czesc") if p in PART_TITLES]
    return chosen or None


def install(bp, ctx):
    import dbeditor

    from dbeditor import tables_common

    _CTX.update(ctx)
    common.init(ctx)
    common.install_history(bp, ctx)
    tables_common.register(bp)
    login_required = ctx["login_required"]

    def export_view(parts):
        content = collect(parts)
        text = render_text(content)
        return content, text

    @bp.route("/config")
    @login_required
    def config_page():
        parts = _selected_parts(request.args)
        error = None
        try:
            content, text = export_view(parts)
        except Exception as exc:  # a database / spool problem: show it, never a 500
            content, text, error = {}, "", f"{type(exc).__name__}: {exc}"
        short = compact(text) if text else ""
        summary = [(p, PART_TITLES[p], summary_of(p, content[p])) for p in ALL_PARTS if p in content]
        try:
            records = list_records(df.spool_dir())
        except Exception:
            records = []
        return render_template(
            "dbeditor/config.html", text=text if len(text) <= SHOW_TEXT_MAX else None, text_size=len(text),
            short=short if len(short) <= SHOW_TEXT_MAX else None, short_size=len(short), summary=summary,
            parts=[(p, PART_TITLES[p]) for p in ALL_PARTS], chosen=parts, error=error, records=records,
            discord_limit=DISCORD_LIMIT, dbe_csrf=common.csrf_token(), version=FORMAT_VERSION)

    @bp.route("/config/eksport.txt")
    @login_required
    def config_export():
        parts = _selected_parts(request.args)
        _content, text = export_view(parts)
        short = request.args.get("skrot") == "1"
        body = (compact(text) + "\n") if short else text
        name = f"mt2009-konfiguracja-{datetime.now():%Y%m%d-%H%M}{'-skrot' if short else ''}.txt"
        return Response(body.encode("utf-8"), mimetype="text/plain; charset=utf-8", headers={
            "Content-Disposition": f'attachment; filename="{name}"', "Cache-Control": "no-store"})

    def read_submitted():
        upload = request.files.get("plik")
        if upload and upload.filename:
            data = upload.read(MAX_TEXT + 1)
            if len(data) > MAX_TEXT:
                raise ConfigError("Plik jest za duży.")
            return data
        return request.form.get("tekst") or request.form.get("payload") or ""

    @bp.post("/config/podglad")
    @login_required
    def config_preview():
        if not common.check_csrf():
            return redirect(url_for("dbeditor.config_page"))
        try:
            config = parse(read_submitted())
        except ConfigError as exc:
            flash(str(exc), "error")
            return redirect(url_for("dbeditor.config_page"))
        skip = request.form.get("pomin_konflikty") == "1"
        try:
            plans = plan(config, None, skip)
        except Exception as exc:
            flash(f"Nie udało się sprawdzić konfiguracji: {type(exc).__name__}: {exc}", "error")
            return redirect(url_for("dbeditor.config_page"))
        return render_template("dbeditor/config_preview.html", config=config, plans=plans,
                               payload=compact(config["text"]), skip=skip, dbe_csrf=common.csrf_token(),
                               dump=dump, preview_rows=PREVIEW_ROWS,
                               importable=[p["part"] for p in plans if not p["errors"] and p["changes"]])

    @bp.post("/config/importuj")
    @login_required
    def config_import():
        if not common.check_csrf():
            return redirect(url_for("dbeditor.config_page"))
        try:
            config = parse(request.form.get("payload", ""))
        except ConfigError as exc:
            flash(str(exc), "error")
            return redirect(url_for("dbeditor.config_page"))
        parts = [p for p in request.form.getlist("czesc") if p in PART_TITLES]
        if not parts:
            flash("Nie zaznaczono żadnej części do importu.", "error")
            return redirect(url_for("dbeditor.config_page"))
        skip = request.form.get("pomin_konflikty") == "1"
        try:
            plans = plan(config, set(parts), skip)
            blocked = [p["title"] for p in plans if p["errors"]]
            record = apply(plans, config["meta"])
        except Exception as exc:
            flash(f"Import się nie udał i nic nie zostało zmienione: {type(exc).__name__}: {exc}", "error")
            return redirect(url_for("dbeditor.config_page"))
        for title in blocked:
            flash(f"Pominięto część z błędami: {title}.", "warning")
        total = record["fields"] + len(record["files"])
        if not total:
            flash("Nic się nie zmieniło – ten serwer ma już tę konfigurację.", "info")
            return redirect(url_for("dbeditor.config_page"))
        flash(f"Zaimportowano konfigurację #{record['batch'][:8]}: {record['fields']} zmian w bazie, "
              f"{len(record['files'])} {plural(len(record['files']), 'plik', 'pliki', 'plików')}. Gra wczyta je po "
              "restarcie – kliknij „Zastosuj”. Cały import można cofnąć niżej.", "success")
        return redirect(url_for("dbeditor.config_page") + "#importy")

    @bp.post("/config/cofnij")
    @login_required
    def config_undo():
        if not common.check_csrf():
            return redirect(url_for("dbeditor.config_page"))
        try:
            ok, message = undo(request.form.get("batch", ""), request.form.get("force") == "1")
        except Exception as exc:
            ok, message = False, f"{type(exc).__name__}: {exc}"
        flash(message, "success" if ok else "error")
        return redirect(url_for("dbeditor.config_page") + "#importy")

    dbeditor.add_section("dbeditor.config_page", "📤", "Eksport / import konfiguracji",
                         "wszystkie zmiany edytora jako tekst do zapisania albo wklejenia na Discordzie – i z powrotem")
