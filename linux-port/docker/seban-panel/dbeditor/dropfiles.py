"""MT2009_PLUS_DB_EDITOR_V1: the drop files of the game, read and written the
way the game's core reads them (item_manager_read_tables.cpp), shared by the
database editor's "Drop" (drops.py) and "Szkatułki" (chests.py) parts.

How a change reaches the game (the mechanism the old /drops and /chests pages
already use - same files, same backups, so both views stay in step):

  the panel writes into the spool volume both containers mount (/opt/m2spool):
    drops/mob_drop_item.custom.txt      whole groups, one per monster and kind
    drops/common_drop_item.custom.txt   the whole common table  (new)
    drops/etc_drop_item.custom.txt      the whole etc table     (new)
    chests/special_item_group.custom.txt whole chest groups, one per Vnum
  and before the cores boot (m2-supervise -> boot_cores) the game's m2-drops
  and m2-chests rebuild the live locale files from the image's copy plus
  these, check every item against item_proto, and write drops/status,
  drops/<table>.status and chests/status (state, sha of the custom file it
  used). The image's own files come back as drops/*.base.txt and
  chests/special_item_group.base.txt - that is what the editor shows.

"Pending - restart needed": a change is live when the sha the game reports
equals the sha of the custom file the panel wrote. pending_changes(spool)
compares them for all four files; every save also drops a flag file
<spool>/dbeditor/pending/<file>.json, which pending_changes() removes once the
game reports that file applied (see pending_changes()).
"""
import hashlib
import json
import os
import re
import unicodedata
from datetime import datetime
from pathlib import Path

SPOOL_DEFAULT = "/opt/m2spool"
SPOOL_GID = 2050
BACKUP_KEEP = 100

MOB_KINDS = ("drop", "kill", "limit", "thiefgloves")
MOB_KIND_NAMES = {
    "drop": "Drop (każda pozycja losowana osobno)",
    "kill": "Co N zabójstw (1 przedmiot z listy)",
    "limit": "Od poziomu gracza",
    "thiefgloves": "Tylko z premium / rękawicami złodzieja",
}
MOB_KIND_SHORT = {"drop": "Drop", "kill": "Co N zabójstw", "limit": "Od poziomu", "thiefgloves": "Rękawice złodzieja"}
MOB_RANK_NAMES = {0: "Zwykły", 1: "Silniejszy", 2: "Rycerz", 3: "Elitarny", 4: "Boss", 5: "Król"}
COMMON_RANKS = ("Zwykłe potwory", "Silniejsze potwory", "Rycerze", "Elitarne potwory")
MOB_MAX_LINES = 255       # ReadMonsterDropItemGroup reads the indexes 1..255
MOB_MAX_COUNT = 2000
CHEST_MAX_LINES = 1023    # ReadSpecialDropItemFile reads the indexes 1..1023
CHEST_TOKENS = {
    "gold": "Yang", "exp": "Doświadczenie", "mob": "Potwór (VNUM w polu Ilość)",
    "group": "Grupa potworów (numer w polu Ilość)", "slow": "Spowolnienie",
    "drain_hp": "Utrata HP", "poison": "Trucizna", "bleeding": "Krwawienie",
}
CHEST_TYPES = {"": "Losuje 1 pozycję (wg wag)", "pct": "Każda pozycja osobno (%)",
               "quest": "Questowa", "special": "Specjalna", "attr": "Bonusy (attr)"}
CHEST_FIXED_TYPES = ("quest", "special", "attr")

# file key -> (folder, custom file, status file, Polish title)
FILES = {
    "mob": ("drops", "mob_drop_item.custom.txt", "status", "Drop potworów (grupy)"),
    "common": ("drops", "common_drop_item.custom.txt", "common_drop_item.status", "Zwykły drop (wg poziomu)"),
    "etc": ("drops", "etc_drop_item.custom.txt", "etc_drop_item.status", "Drop specjalny (etc_drop_item)"),
    "chest": ("chests", "special_item_group.custom.txt", "status", "Szkatułki"),
}
BASES = {
    "mob": ("drops", "mob_drop_item.base.txt"),
    "common": ("drops", "common_drop_item.base.txt"),
    "etc": ("drops", "etc_drop_item.base.txt"),
    "group": ("drops", "drop_item_group.base.txt"),
    "chest": ("chests", "special_item_group.base.txt"),
}
CHEST_SNAPSHOT = Path(__file__).resolve().parent.parent / "special_item_group.snapshot.txt"
NO_CHANGES_MARK = b"# (przed tym zapisem nie bylo zadnych zmian z panelu - obraz gry)\r\n"


# ---------------------------------------------------------------- numbers ---
def num(value, default=0.0):
    try:
        return float(str(value).strip().replace(",", "."))
    except (TypeError, ValueError):
        return default


def fmt_num(value, digits=6):
    """A number as the files write it: no exponent, no trailing zeros."""
    text = f"{float(value):.{digits}f}".rstrip("0").rstrip(".")
    return text if text not in ("", "-0") else "0"


def pct_text(pct):
    """0.05 -> '0,05%' - Polish decimal comma, at most 4 significant digits."""
    if pct is None:
        return "—"
    if pct <= 0:
        return "0%"
    text = f"{pct:.4g}"
    if "e" in text:
        text = fmt_num(pct, 8)
    return text.replace(".", ",") + "%"


def chance_text(pct):
    """'0,05% (ok. 1 na 2 000)' - the chance the way a layman reads it."""
    if pct is None or pct <= 0:
        return "0% (nigdy)"
    if pct >= 100:
        return "100% (zawsze)"
    one_in = 100.0 / pct
    if one_in < 1.5:
        return pct_text(pct)
    thousands = f"{round(one_in):,}".replace(",", " ")
    return f"{pct_text(pct)} (ok. 1 na {thousands})"


def parse_number(raw, label, low, high, integer=False):
    raw = (raw or "").strip().replace(",", ".").replace("%", "").strip()
    pattern = r"\d{1,10}" if integer else r"\d{1,10}(\.\d{1,8})?|\.\d{1,8}"
    if not re.fullmatch(pattern, raw):
        kind = "liczba całkowita" if integer else "liczba (np. 1 albo 0,05)"
        raise ValueError(f"{label}: „{raw}” to nie {kind}.")
    value = int(raw) if integer else float(raw)
    if not low <= value <= high:
        raise ValueError(f"{label}: dozwolone {fmt_num(low)}–{fmt_num(high)}.")
    return value


def form_chance(form, prefix, label, scale, high=100):
    """A chance typed in % -> (value for the file, %). The form carries the
    file's own value (orig) and the % it was shown as (shown): a line the
    operator did not touch keeps its exact value (the files have up to 9
    decimals), a changed one is %×scale."""
    raw = (form.get(prefix + "pct") or "").strip()
    pct = parse_number(raw, label, 0, high)
    orig = (form.get(prefix + "orig") or "").strip()
    shown = form.get(prefix + "shown")
    if shown is not None and raw == shown.strip() and re.fullmatch(r"\d{1,10}(\.\d{1,12})?", orig):
        return orig, pct
    return fmt_num(pct * scale, 9), pct


def ascii_comment(text):
    """Comments in mob_drop_item go in as plain ASCII: the core's line reader
    takes any byte above 127 as the first half of a two-byte character."""
    text = (text or "").replace("ł", "l").replace("Ł", "L")
    text = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode("ascii")
    return re.sub(r"[^A-Za-z0-9 .,:;()+'/%-]", " ", text).strip()[:60]


# ------------------------------------------------------------- the paths ---
def spool_dir(app=None):
    if app is None:
        try:
            from flask import current_app
            app = current_app._get_current_object()
        except RuntimeError:
            app = None
    configured = app.config.get("DBE_SPOOL") if app is not None else None
    return Path(configured or os.environ.get("M2PANEL_RATES_SPOOL") or SPOOL_DEFAULT)


def custom_path(spool, key):
    folder, name, _, _ = FILES[key]
    return Path(spool) / folder / name


def status_path(spool, key):
    folder, _, name, _ = FILES[key]
    return Path(spool) / folder / name


def base_path(spool, key):
    folder, name = BASES[key]
    return Path(spool) / folder / name


def backup_dir(spool, key):
    return Path(spool) / FILES[key][0] / "backup"


def read_text(path, encoding="utf-8-sig"):
    try:
        return Path(path).read_bytes().decode(encoding, "replace")
    except OSError:
        return None


def file_sha(path):
    try:
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()
    except OSError:
        return ""


def read_values(path):
    result = {}
    try:
        for line in Path(path).read_text(encoding="utf-8", errors="replace").splitlines():
            key, separator, value = line.partition("=")
            if separator:
                result[key.strip()] = value.strip()
    except OSError:
        pass
    return result


# -------------------------------------------------------- mob_drop_item ---
def parse_mob_drop(text):
    """mob_drop_item.txt -> groups, read the way the core reads it: keys in
    any case, the first value of a key wins, index lines 1, 2, 3... up to the
    first missing one, "--" starts a comment, "#" a comment line."""
    groups, current = [], None
    for number, raw in enumerate((text or "").replace("\r", "").split("\n"), 1):
        tokens = raw.split()
        if not tokens or tokens[0].startswith("#") or tokens[0].startswith("//"):
            continue
        comment = ""
        for position, token in enumerate(tokens):
            if position and token.startswith("--"):
                comment = " ".join(tokens[position:]).lstrip("-").strip()
                tokens = tokens[:position]
                break
        key = tokens[0].lower()
        if current is None:
            if key == "group" and len(tokens) > 1:
                current = {"name": tokens[1], "keys": {}, "entries": {}, "line": number}
            continue
        if tokens[0].startswith("{") or key.startswith("--"):
            continue
        if tokens[0].startswith("}"):
            keys = current["keys"]
            items, index = [], 1
            while index in current["entries"]:
                items.append(current["entries"][index])
                index += 1
            mob = keys.get("mob", [""])[0]
            group = {"name": current["name"], "line": current["line"], "type": keys.get("type", [""])[0],
                     "mob": int(mob) if mob.isdigit() else None, "items": items,
                     "kill_drop": (keys.get("kill_drop") or ["0"])[0],
                     "level_limit": (keys.get("level_limit") or ["0"])[0],
                     "mob_comment": keys.get("_mob_comment", "")}
            if group["mob"] is not None:
                groups.append(group)
            current = None
            continue
        if len(tokens) < 2:
            continue
        if tokens[0].isdigit():
            fields = tokens[1:]
            current["entries"].setdefault(int(tokens[0]), {
                "item": fields[0].lower(), "count": fields[1] if len(fields) > 1 else "0",
                "prob": fields[2] if len(fields) > 2 else "0",
                "rare": fields[3] if len(fields) > 3 and fields[3].isdigit() else "0", "comment": comment})
        elif key not in current["keys"]:
            current["keys"][key] = tokens[1:]
            if key == "mob":
                current["keys"]["_mob_comment"] = comment
    return groups


def render_mob_group(mob, kind, group):
    """One group in the layout m2-drops checks (and the old /drops page
    writes): ASCII name, tabs, rare always on a kill line."""
    lines = [f"Group\tMT2009_panel_{mob}_{kind}", "{", f"\tMob\t{mob}", f"\tType\t{kind}"]
    if kind == "kill":
        lines.append(f"\tkill_drop\t{group['kill_drop']}")
    if kind == "limit":
        lines.append(f"\tlevel_limit\t{group['level_limit']}")
    for index, entry in enumerate(group["items"], 1):
        line = f"\t{index}\t{entry['item']}\t{entry['count']}\t{entry['prob']}"
        if kind == "kill":
            line += f"\t{entry.get('rare') or 0}"
        comment = ascii_comment(entry.get("comment"))
        if comment:
            line += f"\t-- {comment}"
        lines.append(line)
    lines.append("}")
    return lines


def render_mob_custom(custom, reason):
    lines = ["# MT2009_PLUS_DROP_EDITOR_V1 - grupy mob_drop_item.txt zmienione w panelu Seban",
             f"# zapis: {datetime.now():%Y-%m-%d %H:%M:%S} - {ascii_comment(reason)}",
             "# Kazda grupa zastepuje WSZYSTKIE grupy z obrazu gry o tym samym Mob i Type (m2-drops, przy starcie rdzeni).",
             "# Grupa bez pozycji = ten potwor nie ma dropu tego rodzaju.", ""]
    for mob, kind in sorted(custom):
        lines += render_mob_group(mob, kind, custom[(mob, kind)]) + [""]
    return "\r\n".join(lines).encode("ascii", "replace")


def read_mob_custom(spool):
    custom = {}
    for group in parse_mob_drop(read_text(custom_path(spool, "mob")) or ""):
        if group["type"] in MOB_KINDS:
            custom.setdefault((group["mob"], group["type"]), group)
    return custom


def effective_mob_drops(base_groups, custom):
    """What the engine ends up with per (monster, kind): the operator's group,
    or the image's - all "drop" groups of a monster merged (CDropItemGroup is
    shared per monster), only the first group of the other kinds
    (std::map::emplace keeps the first). A kill group with kill_drop 0 is
    skipped by the core."""
    result = {}
    for group in base_groups:
        kind = group["type"]
        if kind not in MOB_KINDS:
            continue
        key = (group["mob"], kind)
        if kind == "kill" and int(num(group["kill_drop"])) == 0:
            continue
        current = result.get(key)
        if current is None:
            result[key] = {"mob": group["mob"], "type": kind, "items": list(group["items"]),
                           "kill_drop": group["kill_drop"], "level_limit": group["level_limit"],
                           "names": [group["name"]], "source": "image", "ignored": 0}
        elif kind == "drop":
            current["items"] = current["items"] + list(group["items"])
            current["names"].append(group["name"])
        else:
            current["ignored"] += 1
    for key, group in custom.items():
        if not group["items"]:
            result.pop(key, None)
            continue
        old = result.get(key)
        result[key] = {"mob": key[0], "type": key[1], "items": list(group["items"]),
                       "kill_drop": group.get("kill_drop", "0"), "level_limit": group.get("level_limit", "0"),
                       "names": old["names"] if old else [], "source": "custom", "ignored": 0}
    return result


def _norm_prob(value):
    return fmt_num(num(value), 6)


def mob_set_key(group):
    kind = group["type"]
    items = tuple((str(e["item"]).lower(), str(int(num(e["count"]))), _norm_prob(e["prob"]),
                   str(int(num(e.get("rare") or 0))) if kind == "kill" else "0") for e in group["items"])
    params = (str(int(num(group.get("kill_drop")))) if kind == "kill" else "",
              str(int(num(group.get("level_limit")))) if kind == "limit" else "")
    return (kind,) + params + items


def mob_set_id(group):
    return hashlib.sha1(repr(mob_set_key(group)).encode("utf-8")).hexdigest()[:12]


def build_mob_sets(effective):
    """Monsters whose drop of one kind is exactly the same - a "zestaw" the
    operator edits once for all of them."""
    sets = {}
    for (mob, kind), group in effective.items():
        set_id = mob_set_id(group)
        entry = sets.get(set_id)
        if entry is None:
            entry = sets[set_id] = {"id": set_id, "kind": kind, "items": group["items"],
                                    "kill_drop": group["kill_drop"], "level_limit": group["level_limit"],
                                    "mobs": [], "names": [], "custom": False}
        entry["mobs"].append(mob)
        for name in group["names"]:
            if name not in entry["names"]:
                entry["names"].append(name)
        entry["custom"] = entry["custom"] or group["source"] == "custom"
    for entry in sets.values():
        entry["mobs"].sort()
    return sets


def mob_chance(kind, entry, group):
    """Chance per kill in %, as CreateDropItem computes it for a killer of the
    monster's level, no bonus (iDeltaPercent 100, iRandRange 4 000 000)."""
    prob = num(entry["prob"])
    if kind in ("drop", "thiefgloves"):
        return min(100.0, max(0.0, prob / 4.0))
    if kind == "limit":
        return min(100.0, max(0.0, prob))
    if kind == "kill":
        weights = sum(max(0.0, num(e["prob"])) for e in group["items"])
        every = max(1, int(num(group.get("kill_drop", 1)) or 1))
        return (100.0 * max(0.0, prob) / weights / every) if weights else 0.0
    return 0.0


def mob_file_prob(kind, pct):
    """The value written to the file for a chance in % (drop/thiefgloves keep
    it ×4, limit as is)."""
    return fmt_num(pct * 4 if kind in ("drop", "thiefgloves") else pct, 9)


def scale_mob_group(group, factor):
    """×factor for the whole group: every chance (drop/limit/thiefgloves,
    capped at 100%), for kill the "every N kills" divided by it."""
    kind = group["type"]
    result = dict(group)
    if kind == "kill":
        result["kill_drop"] = str(max(1, int(round(num(group["kill_drop"]) / factor))))
        result["items"] = [dict(e) for e in group["items"]]
        return result
    cap = 100.0 if kind == "limit" else 400.0
    result["items"] = [dict(e, prob=fmt_num(min(cap, num(e["prob"]) * factor), 9)) for e in group["items"]]
    return result


# ------------------------------------------------------ common_drop_item ---
COMMON_FIELDS = 24  # 4 ranks (PAWN, S_PAWN, KNIGHT, S_KNIGHT) x 6 columns


def parse_common(text):
    """common_drop_item.txt (latin-1, so the Korean labels round-trip byte for
    byte) -> {"lines": [[24 fields]...], "ranks": [[slot...] x4]}.
    ReadCommonDropItemFile: per rank 6 tab-separated columns - label, level
    from, level to (of the KILLER), percent (×4 of the chance), item, a
    count the core ignores (the files keep "1 in N" there). A slot whose item
    is empty or <= 1 or whose level is 0 drops nothing."""
    lines = []
    for raw in (text or "").split("\n"):
        line = raw[:-1] if raw.endswith("\r") else raw
        if not line:
            continue
        fields = line.split("\t")
        lines.append((fields + [""] * COMMON_FIELDS)[:COMMON_FIELDS])
    ranks = [[] for _ in range(4)]
    for number, fields in enumerate(lines):
        for rank in range(4):
            part = fields[rank * 6: rank * 6 + 6]
            slot = {"label": part[0], "lv_start": part[1].strip(), "lv_end": part[2].strip(), "pct": part[3].strip(),
                    "item": part[4].strip(), "n": part[5].strip(), "line": number}
            slot["active"] = common_slot_active(slot)
            ranks[rank].append(slot)
    return {"lines": lines, "ranks": ranks}


def common_slot_active(slot):
    item = slot["item"]
    return item.isdigit() and int(item) > 1 and slot["lv_start"].isdigit() and int(slot["lv_start"]) > 0


def render_common(table):
    """The table back to the file: one line per index, the ranks side by side
    (a rank with fewer slots gets empty ones), CRLF, latin-1."""
    height = max(len(r) for r in table["ranks"])
    out = []
    for index in range(height):
        fields = []
        for rank in range(4):
            slots = table["ranks"][rank]
            if index < len(slots):
                s = slots[index]
                fields += [s.get("label", ""), s["lv_start"], s["lv_end"], s["pct"], s["item"], s.get("n", "")]
            else:
                fields += [""] * 6
        out.append("\t".join(fields))
    return ("\r\n".join(out) + "\r\n").encode("latin-1", "replace")


def common_set_rank(table, rank, slots):
    """Rank `rank` gets `slots` (the label row on top stays). Returns a new
    table; the other ranks are untouched."""
    ranks = [list(r) for r in table["ranks"]]
    keep = [s for s in ranks[rank] if not common_slot_active(s) and s["line"] == 0 and s.get("label")]
    ranks[rank] = keep + slots
    return {"lines": table["lines"], "ranks": ranks}


def common_slot(lv_start, lv_end, file_pct, item, label="", n=None):
    """One slot; file_pct is the file's value (the chance in % ×4). The sixth
    column, which the core ignores, gets "1 in N" like the package's file."""
    value = num(file_pct)
    if n is None or not str(n).isdigit():
        n = str(max(1, int(round(400.0 / value)))) if value > 0 else "0"
    return {"label": label, "lv_start": str(lv_start), "lv_end": str(lv_end), "pct": str(file_pct),
            "item": str(item), "n": n, "line": -1, "active": True}


# --------------------------------------------------------- etc_drop_item ---
def parse_etc(text):
    """etc_drop_item.txt -> [{"item", "prob", "active"}] (ReadEtcDropItemFile:
    the text before the LAST tab is the item, after it the chance ×4; a line
    without a tab or with chance 0 is skipped). The item is the "drop_item"
    column of mob_proto: a monster with drop_item = X drops X this often."""
    entries = []
    for raw in (text or "").replace("\r", "").split("\n"):
        if "\t" not in raw:
            continue
        item, _, prob = raw.rpartition("\t")
        item = item.strip()
        entries.append({"item": item, "prob": prob.strip(),
                        "active": item.isdigit() and num(prob) != 0})
    return entries


def render_etc(entries):
    return ("\r\n".join(f"{e['item']}\t{e['prob']}" for e in entries) + "\r\n").encode("ascii", "replace")


# ------------------------------------------------------ drop_item_group ---
def parse_drop_item_group(text):
    """drop_item_group.txt (read only here): Group { Vnum, Mob, 1 <item>
    <percent> [count] } - merged by the core with the monster's "drop"
    groups. Note the order: chance BEFORE count."""
    groups, current = [], None
    for raw in (text or "").replace("\r", "").split("\n"):
        tokens = raw.split()
        if not tokens or tokens[0].startswith("#"):
            continue
        key = tokens[0].lower()
        if current is None:
            if key == "group" and len(tokens) > 1:
                current = {"name": tokens[1], "vnum": None, "mob": None, "items": {}}
            continue
        if tokens[0] == "{":
            continue
        if tokens[0] == "}":
            items, index = [], 1
            while index in current["items"]:
                items.append(current["items"][index])
                index += 1
            groups.append({"name": current["name"], "vnum": current["vnum"], "mob": current["mob"], "items": items})
            current = None
            continue
        if key in ("vnum", "mob") and len(tokens) > 1 and tokens[1].isdigit():
            current.setdefault(key, None)
            if current[key] is None:
                current[key] = int(tokens[1])
        elif tokens[0].isdigit() and len(tokens) > 2:
            current["items"].setdefault(int(tokens[0]), {"item": tokens[1], "prob": tokens[2],
                                                         "count": tokens[3] if len(tokens) > 3 else "1"})
    return groups


# ---------------------------------------------------- special_item_group ---
def parse_chests(text):
    """special_item_group.txt -> groups, the way ReadSpecialDropItemFile
    reads it: the index lines 1, 2, 3... up to the first missing one; '--'
    starts a comment; a fourth number is the rare chance."""
    groups, current, entries = [], None, {}
    for number, raw in enumerate((text or "").replace("\r", "").split("\n"), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        tokens = line.split()
        key = tokens[0].lower()
        if current is None:
            if key == "group" and len(tokens) > 1:
                current, entries = {"name": tokens[1], "vnum": None, "type": "", "extras": [], "items": [], "line": number}, {}
            continue
        if tokens[0] == "{":
            continue
        if tokens[0] == "}":
            index = 1
            while index in entries:
                current["items"].append(entries[index])
                index += 1
            if current["vnum"] is not None:
                groups.append(current)
            current = None
            continue
        if key == "vnum" and len(tokens) > 1 and tokens[1].isdigit():
            if current["vnum"] is None:
                current["vnum"] = int(tokens[1])
        elif key == "type" and len(tokens) > 1:
            current["type"] = tokens[1].lower()
        elif tokens[0].isdigit():
            fields = tokens[1:]
            comment = ""
            for position, token in enumerate(fields):
                if token.startswith("--") or token == "-":
                    comment = " ".join(fields[position:]).lstrip("-").strip()
                    fields = fields[:position]
                    break
            entry = {"item": fields[0] if fields else "", "count": fields[1] if len(fields) > 1 else "0",
                     "prob": fields[2] if len(fields) > 2 else "0", "rare": "0", "comment": comment}
            if len(fields) > 3:
                if fields[3].lstrip("-").isdigit():
                    entry["rare"] = fields[3]
                    if not comment:
                        entry["comment"] = " ".join(fields[4:]).lstrip("-").strip()
                elif not comment:
                    entry["comment"] = " ".join(fields[3:]).lstrip("-").strip()
            entries.setdefault(int(tokens[0]), entry)
        else:
            current["extras"].append(line)
    return groups


def render_chest_group(group):
    """One group in the package's layout (tabs; rare always written so a
    comment is never read as one) - what m2-chests checks."""
    lines = [f"Group\t{group['name']}", "{", f"\tVnum\t{group['vnum']}"]
    if group.get("type"):
        lines.append(f"\tType\t{group['type']}")
    for extra in group.get("extras", []):
        lines.append("\t" + extra)
    for index, entry in enumerate(group["items"], 1):
        line = f"\t{index}\t{entry['item']}\t{entry['count']}\t{entry['prob']}\t{entry.get('rare') or 0}"
        comment = re.sub(r"[\"{}\r\n\t]", " ", entry.get("comment") or "").strip()
        if comment:
            line += f"\t-- {comment}"
        lines.append(line)
    lines.append("}")
    return lines


def render_chest_custom(custom, reason):
    lines = ["# MT2009_PLUS_CHEST_EDITOR_V1 - grupy special_item_group.txt zmienione w panelu Seban",
             f"# zapis: {datetime.now():%Y-%m-%d %H:%M:%S} - {reason}",
             "# Te grupy zastepuja grupy z obrazu gry o tym samym Vnum (m2-chests, przy starcie rdzeni).", ""]
    for vnum in sorted(custom):
        lines += render_chest_group(custom[vnum]) + [""]
    return "\r\n".join(lines).encode("utf-8")


def read_chest_base(spool):
    text, source = read_text(base_path(spool, "chest")), "game"
    if text is None:
        text, source = read_text(CHEST_SNAPSHOT) or "", "snapshot"
    base = {}
    for group in parse_chests(text):
        base.setdefault(group["vnum"], group)
    return base, source


def read_chest_custom(spool):
    custom = {}
    for group in parse_chests(read_text(custom_path(spool, "chest")) or ""):
        custom.setdefault(group["vnum"], group)
    return custom


def chest_chance(group, entry):
    """Chance in % that one opening gives this line."""
    prob = int(num(entry["prob"]))
    if group.get("type") == "pct":
        return float(min(100, max(0, prob)))
    total = sum(max(0, int(num(e["prob"]))) for e in group["items"])
    return (100.0 * prob / total) if total and prob > 0 else 0.0


def scale_chest_entries(group_type, entries, factor, selected=None):
    """×factor on the chosen lines (all when selected is None). In a "pct"
    group the chance itself (max 100); in a weighted group the weight - which
    only changes anything when not every line is multiplied."""
    out = []
    for index, entry in enumerate(entries):
        entry = dict(entry)
        if selected is None or index in selected:
            prob = int(num(entry["prob"]))
            if prob > 0:
                value = max(1, int(round(prob * factor)))
                entry["prob"] = str(min(100, value) if group_type == "pct" else min(1000000000, value))
        out.append(entry)
    return out


# ------------------------------------------------------- writing safely ---
def _prepare_folder(folder):
    folder.mkdir(parents=True, exist_ok=True)
    try:
        os.chown(folder, -1, SPOOL_GID)
        os.chmod(folder, 0o2770)
    except OSError:
        pass


def write_custom(spool, key, data, reason=""):
    """Writes one custom file atomically under the folder's lock (the same
    lock the old pages take), the previous version to backup/ first. data
    None deletes the file (back to the image's file)."""
    import fcntl
    path = custom_path(spool, key)
    backups = backup_dir(spool, key)
    _prepare_folder(path.parent)
    _prepare_folder(backups)
    stem = path.name[:-len(".txt")]
    with open(path.parent / ".lock", "w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")[:-3]
        backup = backups / f"{stem}.{stamp}.txt"
        backup.write_bytes(path.read_bytes() if path.exists() else NO_CHANGES_MARK)
        if data is None:
            try:
                path.unlink()
            except FileNotFoundError:
                pass
        else:
            temporary = path.parent / f"{path.name}.new{os.getpid()}"
            temporary.write_bytes(data)
            os.chmod(temporary, 0o664)
            os.replace(temporary, path)
        for old in sorted(backups.glob(f"{stem}.*.txt"))[:-BACKUP_KEEP]:
            try:
                old.unlink()
            except OSError:
                pass
    mark_pending(spool, key, reason)
    return backup.name


def list_backups(spool, keys, limit=40):
    result = []
    for key in keys:
        stem = FILES[key][1][:-len(".txt")]
        for path in backup_dir(spool, key).glob(f"{stem}.*.txt"):
            try:
                data = path.read_bytes()
                result.append({"key": key, "name": path.name, "title": FILES[key][3],
                               "time": datetime.fromtimestamp(path.stat().st_mtime),
                               "empty": data.startswith(NO_CHANGES_MARK), "size": len(data)})
            except OSError:
                continue
    result.sort(key=lambda b: b["name"].rsplit(".", 2)[-2], reverse=True)
    return result[:limit]


def backup_file(spool, key, name):
    stem = re.escape(FILES[key][1][:-len(".txt")])
    if not re.fullmatch(stem + r"\.[0-9-]+\.txt", name or ""):
        return None
    path = backup_dir(spool, key) / name
    return path if path.is_file() else None


def restore_backup(spool, key, name):
    """The backup becomes the custom file again (the current one is backed up
    first). A backup of "no changes" removes the custom file."""
    path = backup_file(spool, key, name)
    if path is None:
        raise ValueError("Nie ma takiej kopii.")
    data = path.read_bytes()
    return write_custom(spool, key, None if data.startswith(NO_CHANGES_MARK) else data, f"przywrocono kopie {name}")


# ------------------------------------------------- what the game reports ---
def live_state(spool, key):
    """How the game took the custom file: ok / pending / error / unknown."""
    path = custom_path(spool, key)
    status = read_values(status_path(spool, key))
    exists = path.exists()
    sha = file_sha(path) if exists else ""
    if key in ("mob", "chest") and exists:
        # an empty group list is the same as no file for m2-drops/m2-chests
        text = read_text(path) or ""
        if not re.search(r"(?im)^\s*group\s", text):
            sha = ""
    when = None
    if status.get("time", "").isdigit():
        when = datetime.fromtimestamp(int(status["time"]))
    if not status:
        if not sha:
            return {"kind": "ok", "pending": False, "time": None,
                    "text": "Bez zmian z panelu – gra używa pliku z obrazu."}
        return {"kind": "unknown", "pending": True, "time": None,
                "text": "Zapisane. Gra nie zgłosiła jeszcze, że umie wczytać ten plik (obraz gry bez tej funkcji albo rdzenie "
                        "jeszcze nie startowały) – zadziała po restarcie na aktualnym obrazie gry."}
    if status.get("state") == "rejected" and status.get("sha") == sha:
        return {"kind": "error", "pending": False, "time": when,
                "text": "Gra ODRZUCIŁA zapisany plik i działa na danych z obrazu: " + status.get("message", "")}
    if (status.get("sha") or "") == sha:
        return {"kind": "ok", "pending": False, "time": when,
                "text": "W grze działa dokładnie to, co widzisz." if sha else "Bez zmian z panelu – gra używa pliku z obrazu."}
    return {"kind": "pending", "pending": True, "time": when,
            "text": "Zapisane zmiany czekają na restart rdzeni gry (przycisk „Zastosuj”)."}


def pending_dir(spool):
    return Path(spool) / "dbeditor" / "pending"


def mark_pending(spool, key, detail=""):
    folder = pending_dir(spool)
    try:
        _prepare_folder(folder.parent)
        _prepare_folder(folder)
        data = {"part": "drops" if key != "chest" else "chests", "file": key, "title": FILES[key][3],
                "detail": detail, "time": int(datetime.now().timestamp()),
                "sha": file_sha(custom_path(spool, key))}
        temporary = folder / f".{key}.json.new{os.getpid()}"
        temporary.write_text(json.dumps(data, ensure_ascii=False), encoding="utf-8")
        os.chmod(temporary, 0o664)
        os.replace(temporary, folder / f"drops-{key}.json")
    except OSError:
        pass


def pending_changes(spool):
    """[{key, title, kind, text, detail, time}] of the drop/chest files whose
    saved state the game does not run yet - what the "Zastosuj" button
    (clientdata.py) restarts the cores for. Flag files of files the game
    reports applied (or rejected) are removed here."""
    result = []
    for key in FILES:
        state = live_state(spool, key)
        flag = pending_dir(spool) / f"drops-{key}.json"
        if state["pending"] or state["kind"] == "error":
            info = {}
            try:
                info = json.loads(flag.read_text(encoding="utf-8"))
            except (OSError, ValueError):
                pass
            result.append({"key": key, "title": FILES[key][3], "kind": state["kind"], "text": state["text"],
                           "detail": info.get("detail", ""), "time": info.get("time")})
        else:
            try:
                flag.unlink()
            except OSError:
                pass
    return result


# ------------------------------------------------------- database lookups ---
def item_names(rows, game_text, vnums):
    """{vnum: {"name", "type"}} from item_proto, a few hundred at a time."""
    wanted = set()
    for value in vnums:
        text = str(value).lower().lstrip("s")
        if text.isdigit():
            wanted.add(int(text))
    wanted = sorted(wanted)
    names = {}
    for start in range(0, len(wanted), 500):
        part = wanted[start:start + 500]
        marks = ",".join(["%s"] * len(part))
        for row in rows(f"SELECT vnum,locale_name,type FROM player.item_proto WHERE vnum IN ({marks})", part):
            names[int(row["vnum"])] = {"name": game_text(row["locale_name"]), "type": int(row.get("type") or 0)}
    return names


def search_items(rows, game_text, query, limit=25):
    query = (query or "").strip()
    if not query or (len(query) < 2 and not query.isdigit()):
        return []
    found = rows("SELECT vnum,locale_name,type FROM player.item_proto WHERE vnum=%s OR locale_name LIKE %s "
                 "ORDER BY vnum LIMIT %s", (int(query) if query.isdigit() else -1, f"%{query}%", int(limit)))
    return [{"vnum": int(r["vnum"]), "name": game_text(r["locale_name"]), "type": int(r.get("type") or 0)} for r in found]


def mob_table(rows, game_text):
    mobs = {}
    for row in rows("SELECT vnum,locale_name,name,`rank`,type,level,drop_item FROM player.mob_proto"):
        rank = int(row.get("rank") or 0)
        mob_type = int(row.get("type") or 0)
        kind = "metin" if mob_type == 2 else "boss" if rank >= 4 and mob_type == 0 else "mob" if mob_type == 0 else "other"
        mobs[int(row["vnum"])] = {
            "vnum": int(row["vnum"]), "name": game_text(row.get("locale_name")) or game_text(row.get("name")),
            "rank": rank, "rank_name": MOB_RANK_NAMES.get(rank, str(rank)), "type": mob_type,
            "level": int(row.get("level") or 0), "drop_item": int(row.get("drop_item") or 0), "kind": kind,
            "kind_name": {"metin": "Metin", "boss": "Boss", "mob": "Potwór"}.get(kind, "Inne")}
    return mobs


def panel_function(name):
    """A helper of app.py (item_icon_url ...), looked up only when needed:
    app.py runs as "app" under gunicorn, as "__main__" when started alone."""
    import sys
    for module_name in ("app", "__main__"):
        module = sys.modules.get(module_name)
        if module is not None and hasattr(module, name):
            return getattr(module, name)
    return None


def icon_url(vnum):
    function = panel_function("item_icon_url")
    if function is None:
        return None
    try:
        return function(vnum)
    except Exception:  # an icon is never worth a broken page
        return None


def csrf_token():
    import uuid
    from flask import session
    token = session.get("dbe_drops_csrf")
    if not token:
        token = session["dbe_drops_csrf"] = uuid.uuid4().hex
    return token


def csrf_ok(form):
    from flask import session
    token = session.get("dbe_drops_csrf")
    return bool(token) and form.get("dbe_csrf", "") == token


def register(bp):
    """Template helpers for the Drop/Szkatułki pages (once per blueprint)."""
    if getattr(bp, "_dbd_helpers", False):
        return
    bp._dbd_helpers = True

    @bp.context_processor
    def _dbd_context():
        from flask import current_app
        return {"dbd_endpoints": current_app.view_functions, "dbd_icon": icon_url,
                "pct_text": pct_text, "chance_text": chance_text, "kind_short": MOB_KIND_SHORT}
