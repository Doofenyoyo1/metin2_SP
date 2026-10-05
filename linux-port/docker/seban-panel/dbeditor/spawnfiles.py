"""MT2009_PLUS_DB_EDITOR_V1: the game's respawn files, read and written the
way the game's core reads them - shared by "Respawn bossów i metinów"
(spawns.py) and the map filter of "Potwory i bossowie" (mobs.py).

The files (share/locale/poland in the game image):
  special_spawns.txt       the timed bosses (SpecialSpawnManager.cpp): Group
                           blocks with vnum (the key), spawn_vnum, spawn_type
                           mob|group, map_index, time a-b (SECONDS), time_type
                           normal|hour, count, save_kill, initial_delay a-b,
                           channel, event_flag, notify_level and the places
                           "1 x y rot", "2 ..." (map cells). count above the
                           number of places stops the core (thecore_shutdown).
  map/<dir>/boss.txt       11 words a line (regen.cpp read_line): type sx sy
  map/<dir>/stone.txt      rx ry z dir time pct count vnum - type m (monster),
                           g (group.txt), r (group_group.txt), s (anywhere),
                           e (6 words, an exception area); a 2nd letter a/h/d.
                           The words run on across lines, so a line with a
                           word missing shifts every line after it, and an
                           unknown type exit()s the core.

How a change reaches the game (the m2-drops way, game/bin/m2-spawns):

  the panel writes into the spool volume both containers mount:
    spawns/special_spawns.custom.txt   only the groups it changed, added or
                                       removed (a removed one carries
                                       "m2_removed 1"), keyed by vnum;
    spawns/maps/<dir>/boss.custom.txt  a whole boss.txt / stone.txt;
    spawns/maps/<dir>/stone.custom.txt
  and before the cores boot (m2-supervise -> boot_cores) m2-spawns apply
  checks them as the core would read them, builds the live files from the
  image's copy (*.image.txt) plus these, and reports per file in
  spawns/status ("<key>=<state>|<sha>|<message>"). A file that fails a check
  is not used: that file stays as the image has it. The image's own files
  come back as spawns/base/... - what the editor shows and edits.

"Pending - restart needed": the sha the game reports for a file differs
from the sha of the custom file now (pending_changes()).
"""
import hashlib
import json
import os
import re
from datetime import datetime
from pathlib import Path

from . import dropfiles

SPOOL_GID = 2050
BACKUP_KEEP = 100
NO_CHANGES_MARK = b"# (przed tym zapisem nie bylo zadnych zmian z panelu - obraz gry)\r\n"
MAP_FILES = {"boss": "Bossowie", "stone": "Metiny"}
DIR_RE = re.compile(r"^[A-Za-z0-9_]{1,64}$")
GROUP_NAME_RE = re.compile(r"^[A-Za-z0-9_]{1,48}$")
FLAG_RE = re.compile(r"^[A-Za-z0-9_]{1,40}$")
# regen.cpp reads digits and h/m/s and skips anything else ("55m-85mm" in the image is 55-85 min).
TIME_TOKEN_RE = re.compile(r"^[0-9hms]*\d[0-9hms]*(-[0-9hms]*\d[0-9hms]*)?$")
ROW_TYPES = {"m": "Potwór", "g": "Grupa", "r": "Grupa grup (losowa)", "s": "Potwór w losowym miejscu mapy"}
ROW_FLAGS = {"": "", "a": "agresywny", "h": "tylko 17–22", "d": "pierwszy raz 5–20 min po starcie"}
SPECIAL_KEYS = ("vnum", "spawn_vnum", "spawn_type", "channel", "map_index", "time", "time_type", "count",
                "save_kill", "initial_delay", "event_flag", "notify_level")
MAX_SECONDS = 7 * 24 * 3600

# The panel's own map names (app.MAP_NAMES) win; these fill the maps it does not list.
EXTRA_MAP_NAMES = {
    4: "Shinsoo – Dolina Gildii", 24: "Chunjo – Dolina Gildii", 44: "Jinno – Dolina Gildii",
    6: "Shinsoo – Wioska Gildii", 26: "Chunjo – Wioska Gildii", 46: "Jinno – Wioska Gildii",
    62: "Ognista Ziemia", 65: "Świątynia Hwang", 67: "Las", 68: "Czerwony Las", 69: "Przełęcz",
    70: "Ognista Pustynia (nusluck)", 107: "Loch Małp", 301: "Przylądek Smoczej Głowy", 302: "Las Mglistego Świtu",
    303: "Zatoka Czarnego Piasku", 304: "Góra Grzmotów",
}
# The panel's map pictures (static/maps, as static/heatmap.js names them).
MAP_PICTURES = {
    1: "shinsoo-m1.png", 3: "shinsoo-m2.png", 21: "chunjo-m1.png", 23: "chunjo-m2.png", 41: "jinno-m1.png",
    43: "jinno-m2.png", 61: "mount-sohan.png", 62: "doyyumhwaji.png", 63: "yongbi-desert.png",
    64: "orc-valley.png", 65: "hwang-temple.png", 67: "trent-forest.png", 68: "trent02-red-forest.png",
    104: "spider-dungeon-v1.png", 71: "spider-dungeon-v1.png", 72: "grotto-v1.png", 73: "grotto-v2.png",
    108: "medium-monkey.webp", 109: "hard-monkey.webp", 209: "ochao-temple.png",
    360: "arezzo-cyclops-valley.png", 361: "arezzo-pharaoh-wastes.png", 362: "arezzo-enchanted-forest.png",
}


def is_mini_boss(vnum):
    """constants.cpp IsMiniBoss - counted with the bosses by the respawn settings."""
    v = int(vnum)
    return (191 <= v <= 194 or 491 <= v <= 494 or v == 681 or 2181 <= v <= 2182 or 781 <= v <= 782
            or 1181 <= v <= 1182 or 2281 <= v <= 2282 or 2381 <= v <= 2382)


# ------------------------------------------------------------ the paths ---
def spool_dir(app=None):
    return dropfiles.spool_dir(app)


def root(spool):
    return Path(spool) / "spawns"


def base_path(spool, name):
    return root(spool) / "base" / name


def special_custom_path(spool):
    return root(spool) / "special_spawns.custom.txt"


def map_custom_path(spool, mapdir, kind):
    if not DIR_RE.match(mapdir or "") or kind not in MAP_FILES:
        raise ValueError("Nieprawidłowa mapa albo plik.")
    return root(spool) / "maps" / mapdir / f"{kind}.custom.txt"


def map_base_path(spool, mapdir, name):
    if not DIR_RE.match(mapdir or ""):
        raise ValueError("Nieprawidłowa mapa.")
    return root(spool) / "base" / "maps" / mapdir / name


def status_path(spool):
    return root(spool) / "status"


def key_of(mapdir=None, kind=None):
    return "special" if mapdir is None else f"map.{mapdir}.{kind}"


def key_title(key, titles=None):
    if key == "special":
        return "Bossowie czasowi (special_spawns.txt)"
    _m, mapdir, kind = key.split(".", 2)
    name = (titles or {}).get(mapdir, mapdir)
    return f"{MAP_FILES.get(kind, kind)} – {name} ({kind}.txt)"


def read_bytes(path):
    try:
        return Path(path).read_bytes()
    except OSError:
        return None


def read_text(path):
    data = read_bytes(path)
    if data is None:
        return None
    if data.startswith(b"\xef\xbb\xbf"):
        data = data[3:]
    return data.decode("latin-1")


def file_sha(path):
    data = read_bytes(path)
    return hashlib.sha256(data).hexdigest() if data is not None else ""


_CACHE = {}


def cached(path, parser):
    """parser(text) of a file, kept while its mtime and size stay."""
    try:
        stat = Path(path).stat()
    except OSError:
        return None
    key = (str(path), parser.__name__)
    hit = _CACHE.get(key)
    if hit and hit[0] == (stat.st_mtime_ns, stat.st_size):
        return hit[1]
    value = parser(read_text(path) or "")
    _CACHE[key] = ((stat.st_mtime_ns, stat.st_size), value)
    return value


# ---------------------------------------------------------------- tokens ---
def tokens_of(line):
    """Words of a line the way CTextFileLoader splits it: blanks, "quoted words"."""
    out, current, quoted, started = [], [], False, False
    for ch in line:
        if quoted:
            if ch == '"':
                quoted = False
                out.append("".join(current))
                current, started = [], False
            else:
                current.append(ch)
        elif ch == '"' and not started:
            quoted, started = True, True
        elif ch in " \t\r\n":
            if started:
                out.append("".join(current))
                current, started = [], False
        else:
            current.append(ch)
            started = True
    if started:
        out.append("".join(current))
    return out


def parse_time(token):
    """regen.cpp MODE_REGEN_TIME: "15m-20m" -> (900, 1200), "600s" -> (600, 600)."""
    start = end = tmp = 0
    second = False
    for ch in token or "":
        if ch == "-":
            start += tmp
            tmp = 0
            second = True
        elif ch in "hms":
            tmp *= {"h": 3600, "m": 60, "s": 1}[ch]
            if second:
                end += tmp
            else:
                start += tmp
            tmp = 0
        elif ch.isdigit():
            tmp = tmp * 10 + int(ch)
    if tmp > 0:
        if second:
            end += tmp
        else:
            start += tmp
    return start, (end if end > 0 else start)


def time_token(low, high):
    """Seconds -> the file's notation: whole minutes as "15m", else seconds."""
    def one(seconds):
        return f"{seconds // 60}m" if seconds and seconds % 60 == 0 else f"{seconds}s"
    return one(low) if low == high else f"{one(low)}-{one(high)}"


def minutes_text(seconds):
    """600 -> '10', 630 -> '10,5', 601 -> '10,02' (what the forms show)."""
    value = round(seconds / 60.0, 2)
    text = (f"{value:.2f}").rstrip("0").rstrip(".")
    return text.replace(".", ",")


def duration_text(seconds):
    """A time the way a layman reads it: '45 s', '12 min', '1 h 30 min'."""
    seconds = int(seconds)
    if seconds < 120:
        return f"{seconds} s"
    minutes = seconds / 60.0
    if minutes < 60:
        return (f"{minutes:.1f}".rstrip("0").rstrip(".")).replace(".", ",") + " min"
    hours, rest = divmod(int(round(minutes)), 60)
    return f"{hours} h" + (f" {rest} min" if rest else "")


def range_text(low, high):
    if low == high:
        return duration_text(low)
    a, b = duration_text(low), duration_text(high)
    unit_a, unit_b = a.split(" ", 1)[-1], b.split(" ", 1)[-1]
    if unit_a == unit_b and " " not in unit_a:
        return f"{a.split(' ')[0]}–{b}"
    return f"{a} – {b}"


def parse_minutes(raw, label, allow_zero=True):
    text = (raw or "").strip().replace(",", ".").replace(" ", "")
    if not re.fullmatch(r"\d{1,6}(\.\d{1,3})?", text):
        raise ValueError(f"{label}: „{raw}” to nie liczba minut (np. 15 albo 7,5).")
    seconds = int(round(float(text) * 60))
    if seconds > MAX_SECONDS:
        raise ValueError(f"{label}: najwyżej 7 dni ({MAX_SECONDS // 60} minut).")
    if seconds == 0 and not allow_zero:
        raise ValueError(f"{label}: musi być więcej niż 0.")
    return seconds


def parse_int(raw, label, low, high):
    text = (raw or "").strip().replace(" ", "")
    if not re.fullmatch(r"-?\d{1,10}", text):
        raise ValueError(f"{label}: „{raw}” to nie liczba całkowita.")
    value = int(text)
    if not low <= value <= high:
        raise ValueError(f"{label}: dozwolone {low}–{high}, podano {value}.")
    return value


# ------------------------------------------------------- the map files ---
def parse_regen(text):
    """[{"raw": line, "row": dict|None}] of a regen/boss/stone file - a row
    for each line of exactly 11 words (or an "e" line of 6) and None for
    comments, blank and unreadable lines (kept as they are)."""
    lines = []
    for raw in (text or "").replace("\r\n", "\n").replace("\r", "\n").split("\n"):
        words = raw.split()
        row = None
        if words and not words[0].startswith("//"):
            kind = words[0]
            if re.fullmatch(r"[mgrs][ahd]?", kind) and len(words) == 11 and all(
                    re.fullmatch(r"-?\d+", w) for w in words[1:7] + words[8:11]) and TIME_TOKEN_RE.match(words[7]):
                low, high = parse_time(words[7])
                row = {"type": kind[0], "flag": kind[1:], "x": int(words[1]), "y": int(words[2]),
                       "rx": int(words[3]), "ry": int(words[4]), "z": int(words[5]), "dir": int(words[6]),
                       "time": words[7], "low": low, "high": high, "pct": int(words[8]),
                       "count": int(words[9]), "vnum": int(words[10])}
            else:
                row = {"bad": True}
        lines.append({"raw": raw, "row": row})
    while lines and not lines[-1]["raw"].strip():
        lines.pop()
    return lines


def render_row(row):
    return "\t".join(str(v) for v in (row["type"] + row.get("flag", ""), row["x"], row["y"], row["rx"], row["ry"],
                                      row.get("z", 0), row.get("dir", 0), row["time"], row.get("pct", 100),
                                      row["count"], row["vnum"]))


def render_regen(lines, crlf=True):
    newline = "\r\n" if crlf else "\n"
    return (newline.join(line["raw"] for line in lines) + newline).encode("latin-1")


def check_regen_lines(lines):
    """Problems that would stop the core or shift its reading ([] = fine)."""
    problems = []
    for number, line in enumerate(lines, 1):
        row = line["row"]
        if row is None:
            continue
        if row.get("bad"):
            words = line["raw"].split()
            if words and words[0] in ("e",) and len(words) == 6 and all(re.fullmatch(r"-?\d+", w) for w in words[1:]):
                continue
            problems.append(f"wiersz {number}: „{line['raw'].strip()[:60]}” – gra by go nie odczytała")
    return problems


# --------------------------------------------------- special_spawns.txt ---
def parse_special(text):
    """[{"name", "keys": {key: [words]}, "order": [keys], "places": [(x, y, rot)],
    "removed": bool}] of the top-level Group blocks, keys lower-cased, the
    first value of a key kept (as the loader does)."""
    groups, current, depth = [], None, 0
    for raw in (text or "").replace("\r", "").split("\n"):
        words = tokens_of(raw)
        if not words:
            continue
        head = words[0].lower()
        if current is None:
            if head == "group" and len(words) == 2:
                current = {"name": words[1], "keys": {}, "order": [], "places": {}}
                depth = 0
            continue
        if head.startswith("{"):
            depth += 1
            continue
        if head.startswith("}"):
            depth -= 1
            if depth <= 0:
                places, index = [], 1
                while index in current["places"]:
                    places.append(current["places"][index])
                    index += 1
                current["places"] = places
                current["removed"] = (current["keys"].get("m2_removed") or ["0"])[0] == "1"
                groups.append(decorate_special(current))
                current = None
            continue
        if words[0].isdigit():
            nums = [int(w) if re.fullmatch(r"-?\d+", w) else None for w in words[1:4]]
            if len(nums) == 3 and None not in nums:
                current["places"].setdefault(int(words[0]), tuple(nums))
            continue
        if len(words) >= 2 and head not in current["keys"]:
            current["keys"][head] = words[1:]
            current["order"].append(head)
    return groups


def _range(words):
    text = (words or [""])[0]
    match = re.fullmatch(r"(\d+)(?:-(\d+))?", text)
    if not match:
        return None
    low = int(match.group(1))
    return low, int(match.group(2)) if match.group(2) else low


def _num(words, default=0):
    text = (words or [""])[0]
    return int(text) if re.fullmatch(r"-?\d+", text) else default


def decorate_special(group):
    keys = group["keys"]
    group["vnum"] = _num(keys.get("vnum"), None)
    group["spawn_vnum"] = _num(keys.get("spawn_vnum"), None)
    group["spawn_type"] = (keys.get("spawn_type") or [""])[0]
    group["map_index"] = _num(keys.get("map_index"), None)
    group["time"] = _range(keys.get("time"))
    group["time_type"] = (keys.get("time_type") or ["normal"])[0]
    group["count"] = _num(keys.get("count"), 0)
    group["save_kill"] = _num(keys.get("save_kill"), 0)
    group["initial_delay"] = _range(keys.get("initial_delay"))
    group["channel"] = _num(keys.get("channel"), 0)
    group["event_flag"] = (keys.get("event_flag") or [""])[0]
    group["notify_level"] = _num(keys.get("notify_level"), 0)
    group.setdefault("removed", False)
    return group


def render_special_group(group, crlf=True):
    """A Group block from the group's fields (unknown keys kept)."""
    newline = "\r\n" if crlf else "\n"
    lines = [f"Group\t{group['name']}", "{"]
    values = {
        "vnum": str(group["vnum"]), "spawn_vnum": str(group["spawn_vnum"]), "spawn_type": group["spawn_type"],
        "channel": str(group.get("channel", 0)), "map_index": str(group["map_index"]),
        "time": (f"{group['time'][0]}" if group["time"][0] == group["time"][1] else f"{group['time'][0]}-{group['time'][1]}"),
        "time_type": group.get("time_type") or "normal", "count": str(group["count"]),
        "save_kill": str(int(group.get("save_kill") or 0)),
    }
    if group.get("initial_delay"):
        low, high = group["initial_delay"]
        values["initial_delay"] = f"{low}" if low == high else f"{low}-{high}"
    if group.get("event_flag"):
        values["event_flag"] = group["event_flag"]
    if group.get("notify_level"):
        values["notify_level"] = str(group["notify_level"])
    if group.get("removed"):
        values["m2_removed"] = "1"
    for key in SPECIAL_KEYS + ("m2_removed",):
        if key in values:
            lines.append(f"\t{key}\t{values[key]}")
    for key in group.get("order", []):
        if key not in values and key not in SPECIAL_KEYS and key != "m2_removed" and re.fullmatch(r"[a-z_]+", key):
            lines.append(f"\t{key}\t" + "\t".join(group["keys"][key]))
    for number, (x, y, rot) in enumerate(group.get("places") or [], 1):
        lines.append(f"\t{number}\t{x}\t{y}\t{rot}")
    lines.append("}")
    return newline.join(lines) + newline


def render_special(groups, crlf=True):
    newline = "\r\n" if crlf else "\n"
    return newline.join(render_special_group(g, crlf) for g in groups).encode("latin-1")


def merge_special(base, custom):
    """What the game runs: the image's groups, each replaced by the custom
    group of the same vnum (or dropped when that one is removed), then the
    custom groups the image does not have - m2-spawns does the same."""
    by_vnum = {g["vnum"]: g for g in custom if g.get("vnum") is not None}
    out = []
    for group in base:
        if group["vnum"] in by_vnum:
            replacement = by_vnum.pop(group["vnum"])
            if not replacement["removed"]:
                out.append(dict(replacement, origin="changed", base=group))
        else:
            out.append(dict(group, origin="image"))
    for group in custom:
        if group["vnum"] in by_vnum and not group["removed"]:
            out.append(dict(group, origin="added"))
    return out


# --------------------------------------------------- group.txt and co. ---
def parse_mob_groups(text):
    """{vnum: {"name", "leader", "members": [mob vnums, leader first]}} of group.txt."""
    groups, current = {}, None
    for raw in (text or "").replace("\r", "").split("\n"):
        words = tokens_of(raw)
        if not words:
            continue
        head = words[0].lower()
        if head == "group" and len(words) >= 2:
            current = {"name": words[1], "vnum": None, "leader": None, "members": []}
            continue
        if current is None:
            continue
        if head.startswith("}"):
            if current["vnum"] is not None:
                members = ([current["leader"]] if current["leader"] else []) + current["members"]
                groups[current["vnum"]] = {"name": current["name"], "leader": current["leader"], "members": members}
            current = None
        elif head == "vnum" and len(words) >= 2 and words[1].isdigit():
            current["vnum"] = int(words[1])
        elif head == "leader" and len(words) >= 3 and words[-1].isdigit():
            current["leader"] = int(words[-1])
        elif words[0].isdigit() and len(words) >= 3 and words[-1].isdigit():
            current["members"].append(int(words[-1]))
    return groups


def parse_group_groups(text):
    """{vnum: [group vnums]} of group_group.txt ("k <group> <weight>")."""
    groups, current = {}, None
    for raw in (text or "").replace("\r", "").split("\n"):
        words = tokens_of(raw)
        if not words:
            continue
        head = words[0].lower()
        if head == "group":
            current = {"vnum": None, "members": []}
        elif current is None:
            continue
        elif head.startswith("}"):
            if current["vnum"] is not None:
                groups[current["vnum"]] = current["members"]
            current = None
        elif head == "vnum" and len(words) >= 2 and words[1].isdigit():
            current["vnum"] = int(words[1])
        elif words[0].isdigit() and len(words) >= 2 and words[1].isdigit():
            current["members"].append(int(words[1]))
    return groups


def parse_map_index(text):
    """{map index: folder} of map/index."""
    out = {}
    for raw in (text or "").replace("\r", "").split("\n"):
        words = raw.split()
        if len(words) >= 2 and words[0].isdigit() and DIR_RE.match(words[1]):
            out.setdefault(int(words[0]), words[1])
    return out


def parse_setting(text):
    """(width, height) in cells from Setting.txt MapSize (256 cells a block)."""
    match = re.search(r"(?im)^\s*MapSize\s+(\d+)\s+(\d+)", text or "")
    return (int(match.group(1)) * 256, int(match.group(2)) * 256) if match else None


# ------------------------------------------------------- what is wanted ---
class SpawnIndex:
    """The respawn files as the game will run them after the next restart:
    the image's files (published by m2-spawns) with the panel's custom ones."""

    def __init__(self, spool):
        self.spool = Path(spool)
        self.maps = cached(base_path(spool, "map_index.txt"), parse_map_index) or {}
        self.dirs = {d: i for i, d in self.maps.items()}
        self.groups = cached(base_path(spool, "group.txt"), parse_mob_groups) or {}
        self.group_groups = cached(base_path(spool, "group_group.txt"), parse_group_groups) or {}
        self.special_base = cached(base_path(spool, "special_spawns.txt"), parse_special) or []
        custom = special_custom_path(spool)
        self.special_custom = (cached(custom, parse_special) or []) if custom.exists() else []
        self.special = merge_special(self.special_base, self.special_custom)
        self._names = None

    @classmethod
    def load(cls, spool):
        index = cls(spool)
        if not index.maps:
            return None
        return index

    # names --------------------------------------------------------------
    def map_title(self, index):
        index = int(index)
        names = dropfiles.panel_function("MAP_NAMES") or {}
        if index in names:
            return names[index]
        if index in EXTRA_MAP_NAMES:
            return EXTRA_MAP_NAMES[index]
        folder = self.maps.get(index)
        return f"mapa {index}" + (f" ({folder})" if folder else "")

    def dir_titles(self):
        return {d: self.map_title(i) for i, d in self.maps.items()}

    def map_size(self, mapdir):
        return cached(map_base_path(self.spool, mapdir, "Setting.txt"), parse_setting)

    @staticmethod
    def map_picture(index):
        return MAP_PICTURES.get(int(index))

    # files --------------------------------------------------------------
    def map_dirs_with_files(self):
        folder = self.spool / "spawns" / "base" / "maps"
        out = []
        for mapdir in sorted(self.dirs, key=lambda d: self.dirs[d]):
            if any((folder / mapdir / f"{k}.txt").exists() for k in MAP_FILES):
                out.append(mapdir)
        return out

    def map_lines(self, mapdir, kind, wanted=True):
        """(lines, origin): origin 'custom' when the panel's file is used."""
        custom = map_custom_path(self.spool, mapdir, kind)
        if wanted and custom.exists():
            return cached(custom, parse_regen) or [], "custom"
        base = map_base_path(self.spool, mapdir, f"{kind}.txt")
        return (cached(base, parse_regen) or []), "image"

    def regen_lines(self, mapdir):
        return cached(map_base_path(self.spool, mapdir, "regen.txt"), parse_regen) or []

    # members --------------------------------------------------------------
    def members(self, row_type, vnum):
        if row_type in ("m", "s"):
            return [vnum]
        if row_type == "g":
            return list((self.groups.get(vnum) or {}).get("members") or [])
        if row_type == "r":
            out = []
            for group in self.group_groups.get(vnum, []):
                out += (self.groups.get(group) or {}).get("members") or []
            return out
        return []

    def special_members(self, group):
        return self.members("g" if group["spawn_type"] == "group" else "m", group["spawn_vnum"] or 0)

    def mobs_by_map(self):
        """{map index: {mob vnums}} - every monster a respawn line or a timed
        boss can put on the map (groups expanded)."""
        if getattr(self, "_by_map", None) is not None:
            return self._by_map
        out = {}
        with_files = self.map_dirs_with_files()
        for mapdir in with_files + [d for d in self.dirs if d not in with_files]:
            index = self.dirs[mapdir]
            found = set()
            for kind in list(MAP_FILES) + ["regen"]:
                lines = self.regen_lines(mapdir) if kind == "regen" else self.map_lines(mapdir, kind)[0]
                for line in lines:
                    row = line["row"]
                    if row and not row.get("bad"):
                        found.update(self.members(row["type"], row["vnum"]))
            if found:
                out.setdefault(index, set()).update(found)
        for group in self.special:
            if group.get("map_index") is not None:
                out.setdefault(group["map_index"], set()).update(self.special_members(group))
        self._by_map = out
        return out

    def maps_of_mob(self, vnum):
        return [index for index, vnums in sorted(self.mobs_by_map().items()) if int(vnum) in vnums]

    def spawns_of_mob(self, vnum):
        """Where the respawn files put this monster: [{map, file, row...}]."""
        vnum = int(vnum)
        out = []
        for group in self.special:
            if vnum in self.special_members(group):
                out.append({"kind": "special", "map": group["map_index"], "low": group["time"][0] if group["time"] else 0,
                            "high": group["time"][1] if group["time"] else 0, "count": group["count"],
                            "vnum": group["vnum"], "places": len(group["places"])})
        for mapdir in self.map_dirs_with_files():
            for kind in MAP_FILES:
                for line in self.map_lines(mapdir, kind)[0]:
                    row = line["row"]
                    if row and not row.get("bad") and vnum in self.members(row["type"], row["vnum"]):
                        out.append({"kind": kind, "map": self.dirs[mapdir], "dir": mapdir, "low": row["low"],
                                    "high": row["high"], "count": row["count"], "x": row["x"], "y": row["y"]})
        return out


# ------------------------------------------------- the respawn settings ---
def read_flags(rows):
    """The event flags the respawn code reads (regen.cpp): fastBossSpawn,
    fastMobSpawn (world-wide, the /respawns page), fastBossSpawn<map> and
    fastMobSpawn<map> (per map, the GM command map_spawn_delay), and the
    count multipliers m2_boss_count / m2_mob_count."""
    flags = {}
    try:
        for row in rows("SELECT szName, lValue FROM player.quest WHERE dwPID=0 AND (szName LIKE %s OR szName LIKE %s "
                        "OR szName IN (%s, %s))", ("fastBossSpawn%", "fastMobSpawn%", "m2_boss_count", "m2_mob_count")):
            name = row.get("szName")
            if isinstance(name, bytes):
                name = name.decode("latin-1")
            try:
                flags[str(name)] = int(row.get("lValue") or 0)
            except (TypeError, ValueError):
                continue
    except Exception:  # no database: the files' own times
        return {}
    return flags


def delay_percent(flags, boss_or_stone, map_index):
    """(percent, source) the core uses for a respawn line's time - 100 = the
    file's time. regen_event: the map's own flag, else the world-wide one;
    0 (or >= 100) = untouched; never below 3 seconds."""
    prefix = "fastBossSpawn" if boss_or_stone else "fastMobSpawn"
    value = max(0, min(100, int(flags.get(f"{prefix}{int(map_index)}", 0) or 0)))
    source = "map"
    if value == 0:
        value = max(0, min(100, int(flags.get(prefix, 0) or 0)))
        source = "world"
    if value == 0 or value >= 100:
        return 100, None
    return value, source


def effective_seconds(seconds, percent):
    if percent >= 100 or seconds <= 0:
        return seconds
    return max(3, seconds * percent // 100)


def count_percent(flags, boss_or_stone, map_index, all_fighters):
    """regen_target_count: a line of monsters/stones only, not in dungeons."""
    if int(map_index) >= 10000 or not all_fighters:
        return 100
    value = int(flags.get("m2_boss_count" if boss_or_stone else "m2_mob_count", 0) or 0)
    return min(value, 400) if value > 100 else 100


# ------------------------------------------------------ writing, backups ---
def _prepare_folder(folder):
    folder.mkdir(parents=True, exist_ok=True)
    try:
        os.chown(folder, -1, SPOOL_GID)
        os.chmod(folder, 0o2770)
    except OSError:
        pass


def backup_stem(key):
    return "special_spawns" if key == "special" else "map-" + key.split(".", 1)[1].replace(".", "-")


def backup_dir(spool):
    return root(spool) / "backup"


def custom_path_of(spool, key):
    if key == "special":
        return special_custom_path(spool)
    _m, mapdir, kind = key.split(".", 2)
    return map_custom_path(spool, mapdir, kind)


def write_custom(spool, key, data, reason=""):
    """Writes one custom file atomically under the spawns folder's lock, the
    previous version to backup/ first. data None deletes the file (back to
    the image's file)."""
    import fcntl
    path = custom_path_of(spool, key)
    backups = backup_dir(spool)
    _prepare_folder(root(spool))
    _prepare_folder(path.parent)
    _prepare_folder(backups)
    stem = backup_stem(key)
    with open(root(spool) / ".lock", "w") as lock:
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
    _CACHE.pop((str(path), "parse_regen"), None)
    _CACHE.pop((str(path), "parse_special"), None)
    log_change(spool, key, reason)
    return backup.name


def log_change(spool, key, reason):
    """The last change of each file, for the "Zastosuj" list."""
    path = root(spool) / "changes.json"
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        data = {}
    data[key] = {"time": int(datetime.now().timestamp()), "detail": reason[:200]}
    try:
        temporary = path.with_name(f".changes.json.new{os.getpid()}")
        temporary.write_text(json.dumps(data, ensure_ascii=False), encoding="utf-8")
        os.chmod(temporary, 0o664)
        os.replace(temporary, path)
    except OSError:
        pass


def list_backups(spool, limit=60):
    out = []
    for path in backup_dir(spool).glob("*.txt"):
        match = re.fullmatch(r"(special_spawns|map-[A-Za-z0-9_]+-(?:boss|stone))\.([0-9-]+)\.txt", path.name)
        if not match:
            continue
        stem = match.group(1)
        key = "special" if stem == "special_spawns" else "map." + stem[4:].rsplit("-", 1)[0] + "." + stem.rsplit("-", 1)[1]
        try:
            data = path.read_bytes()
            out.append({"name": path.name, "key": key, "stamp": match.group(2), "size": len(data),
                        "empty": data.startswith(NO_CHANGES_MARK), "time": datetime.fromtimestamp(path.stat().st_mtime)})
        except OSError:
            continue
    out.sort(key=lambda b: b["stamp"], reverse=True)
    return out[:limit]


def restore_backup(spool, name):
    entry = next((b for b in list_backups(spool, 100000) if b["name"] == name), None)
    if entry is None:
        raise ValueError("Nie ma takiej kopii.")
    data = (backup_dir(spool) / name).read_bytes()
    write_custom(spool, entry["key"], None if data.startswith(NO_CHANGES_MARK) else data, f"przywrócono kopię {name}")
    return entry["key"]


# ------------------------------------------------- what the game reports ---
def read_status(spool):
    """{key: {"state", "sha", "message"}} plus "_time" from spawns/status."""
    out = {}
    try:
        text = status_path(spool).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None
    for line in text.splitlines():
        key, sep, value = line.partition("=")
        if not sep:
            continue
        if key == "time":
            out["_time"] = int(value) if value.strip().isdigit() else None
            continue
        state, _s, rest = value.partition("|")
        sha, _s, message = rest.partition("|")
        out[key.strip()] = {"state": state.strip(), "sha": sha.strip(), "message": message.strip()}
    return out


def custom_keys(spool):
    keys = []
    if special_custom_path(spool).exists():
        keys.append("special")
    for path in sorted((root(spool) / "maps").glob("*/*.custom.txt")):
        mapdir, kind = path.parent.name, path.name.split(".", 1)[0]
        if DIR_RE.match(mapdir) and kind in MAP_FILES:
            keys.append(key_of(mapdir, kind))
    return keys


def effective_sha(spool, key):
    path = custom_path_of(spool, key)
    if not path.exists():
        return ""
    if key == "special" and not re.search(r"(?im)^\s*group\s", read_text(path) or ""):
        return ""  # no group = no change (m2-spawns does the same)
    return file_sha(path)


def live_state(spool, key, status=None):
    """How the game took a file: ok / pending / error / unknown."""
    status = read_status(spool) if status is None else status
    sha = effective_sha(spool, key)
    if status is None:
        if not sha:
            return {"kind": "ok", "pending": False, "text": "Bez zmian z panelu – gra używa pliku z obrazu."}
        return {"kind": "unknown", "pending": True,
                "text": "Zapisane. Gra nie zgłosiła jeszcze, że umie wczytać ten plik (obraz gry bez m2-spawns albo "
                        "rdzenie jeszcze nie startowały) – zadziała po restarcie na aktualnym obrazie gry."}
    reported = status.get(key) or {}
    if sha and reported.get("state") == "rejected" and reported.get("sha") == sha:
        return {"kind": "error", "pending": False,
                "text": "Gra ODRZUCIŁA ten plik i działa na pliku z obrazu: " + reported.get("message", "")}
    if (reported.get("sha") or "") == sha and (not sha or reported.get("state") == "ok"):
        return {"kind": "ok", "pending": False,
                "text": "W grze działa dokładnie to, co widzisz." if sha else "Bez zmian z panelu – gra używa pliku z obrazu."}
    if not sha:
        return {"kind": "pending", "pending": True,
                "text": "Powrót do pliku z obrazu gry czeka na restart rdzeni („Zastosuj”)."}
    return {"kind": "pending", "pending": True, "text": "Zapisane zmiany czekają na restart rdzeni gry („Zastosuj”)."}


def pending_changes(spool, titles=None):
    """[{key, title, kind, text, detail, time}] of the respawn files whose
    saved state the game does not run yet (or rejected) - for "Zastosuj"
    (clientdata.py), the same shape as dropfiles.pending_changes()."""
    status = read_status(spool)
    keys = set(custom_keys(spool))
    if status:
        keys |= {k for k, v in status.items() if not k.startswith("_") and isinstance(v, dict) and v.get("sha")}
    try:
        changes = json.loads((root(spool) / "changes.json").read_text(encoding="utf-8"))
    except (OSError, ValueError):
        changes = {}
    if titles is None:
        try:
            index = SpawnIndex.load(spool)
            titles = index.dir_titles() if index else {}
        except Exception:
            titles = {}
    out = []
    for key in sorted(keys):
        try:
            state = live_state(spool, key, status)
        except ValueError:
            continue
        if state["pending"] or state["kind"] == "error":
            info = changes.get(key) or {}
            out.append({"key": key, "title": "Respawn: " + key_title(key, titles), "kind": state["kind"],
                        "text": state["text"], "detail": info.get("detail", ""), "time": info.get("time")})
    return out
