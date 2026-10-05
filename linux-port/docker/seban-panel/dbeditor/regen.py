"""MT2009_PLUS_DB_EDITOR_V1: "Spawny potworów na mapach" (/db/regen) - the
monsters every map keeps alive (map/<map>/regen.txt), in plain words and on
the map's picture, edited without touching the file.

The file (regen.cpp read_line, the format the core reads; one entry is 11
whitespace-separated words, "//" at the start of a word comments out the rest
of the line, line breaks mean nothing to the reader):

    type  x  y  sx  sy  z  dir  time  percent  count  vnum
    m/g/r/s (+ a = aggressive, h = only 17:00-22:00, d = first spawn after
    5-20 min); x, y the centre in metres from the map's corner, sx/sy the
    half-width of the area (the core spawns in x-sx .. x+sx); z and dir
    (0 = any direction, else (dir-1)*45 degrees); time "60s", "5m", "1h30m",
    "5m-7m" (a random time in the range; 0 = the line never spawns); percent
    (read and thrown away by the core); count - how many monsters (m, s) or
    groups (g, r) the line keeps standing; vnum - mob_proto (m, s),
    group.txt (g) or group_group.txt (r). "e" is a no-spawn area of 6 words.
    An unknown type letter stops the core (exit(1)) - never written here.

How a change reaches the game (the same road as the drop files, m2-drops):

  the panel writes the whole file of a map into the spool volume both
  containers mount (/opt/m2spool):
      regen/custom/<map folder>.txt         (the map's regen.txt as edited)
      regen/custom/<map folder>.meta.json   (sha of the image's file it came from)
      regen/backup/<map folder>.<stamp>.txt (the previous version, 30 per map)
  and before the cores boot (m2-supervise -> boot_cores) the game's
  m2-regen checks it (types, numbers, the coordinates inside the map's
  Setting.txt, every monster in mob_proto, every group in group.txt /
  group_group.txt) and puts it live in place of the image's regen.txt (kept
  as regen.image.txt), or - when a check fails - the image's file, and says
  how it went in regen/status/<map folder>.status (state, sha of the custom
  file). The image's own files come back read-only as regen/base/
  (index, group.txt, group_group.txt, maps/<map>/regen.txt + Setting.txt):
  that is what the editor starts from.

"Czeka na restart": a map is live when the sha the game reports equals the
sha of the custom file (no custom file: no sha). pending_changes(spool) lists
the maps that are not - the "Zastosuj" page (clientdata.py) shows them.
"""
import hashlib
import json
import math
import os
import re
from datetime import datetime
from pathlib import Path

from flask import abort, current_app, flash, g, jsonify, redirect, render_template, request, url_for

from . import dropfiles as df

FOLDER = "regen"
BACKUP_KEEP = 30
MAX_COUNT = 100           # per line - a few hundred monsters on one spot would freeze the core
MAX_TIME = 86400          # one day
MAX_RADIUS = 500
MAX_ROWS = 5000
FOLDER_RE = re.compile(r"[A-Za-z0-9_]{1,64}")
WORD_RE = re.compile(r"[^ \t\r\n]+")
GROUP_WORD_RE = re.compile(r'"[^"]*"|[^\s"]+')
NO_CHANGES_MARK = b"// (przed tym zapisem nie bylo zmian z panelu - plik z obrazu gry)\r\n"

TYPE_NAMES = {"m": "Potwór", "g": "Grupa", "r": "Losowa grupa", "s": "Potwór w losowym miejscu mapy",
              "e": "Strefa bez spawnów"}
FLAG_NAMES = {"": "zwykłe", "a": "agresywne – same atakują", "h": "tylko 17:00–22:00",
              "d": "pierwsze pojawienie po 5–20 min od startu"}
COUNT_FACTORS = ("0.5", "1.5", "2", "3")
TIME_FACTORS = ("0.25", "0.5", "2")
# heatmap.js' pictures of the maps (static/maps), by map index.
MAP_IMAGES = {21: "chunjo-m1", 23: "chunjo-m2", 24: "guild-map-02", 25: "easy-monkey", 61: "mount-sohan",
              62: "doyyumhwaji", 64: "orc-valley", 63: "yongbi-desert", 104: "spider-dungeon-v1",
              108: "medium-monkey", 109: "hard-monkey", 65: "hwang-temple", 71: "spider-dungeon-v1",
              4: "shinsoo-guild", 44: "jinno-guild", 5: "easy-monkey", 45: "easy-monkey", 1: "shinsoo-m1",
              3: "shinsoo-m2", 41: "jinno-m1", 43: "jinno-m2", 67: "trent-forest", 68: "trent02-red-forest",
              66: "deviltower", 72: "grotto-v1", 73: "grotto-v2", 209: "ochao-temple",
              360: "arezzo-cyclops-valley", 361: "arezzo-pharaoh-wastes", 362: "arezzo-enchanted-forest",
              363: "arezzo-library", 364: "arezzo-wukong-hill", 365: "arezzo-scorpion-ruins",
              366: "arezzo-ancient-jungle"}
# Names of the maps app.MAP_NAMES does not list (the panel's table wins).
EXTRA_MAP_NAMES = {
    6: "Wioska gildii Shinsoo", 26: "Wioska gildii Chunjo", 46: "Wioska gildii Jinno", 69: "Przełęcz",
    70: "Wyspa Nusluck", 79: "Labirynt", 81: "Mapa ślubna", 90: "Lodowy loch", 100: "Loch polowy",
    101: "Strefa surowców", 107: "Loch Małp (stary)", 112: "Arena pojedynków", 113: "Mapa OX",
    181: "Wojna królestw 1", 182: "Wojna królestw 2", 183: "Wojna królestw 3", 200: "Budowa gildii (GM)",
    208: "Smocza Grota (Błękitny Smok)", 216: "Katakumby Diabła", 217: "Loch Pająków V3",
    250: "Studio", 301: "Przylądek Smoczej Głowy", 302: "Las Mglistego Świtu", 303: "Zatoka Czarnego Piasku",
    304: "Góra Grzmotów", 351: "Siedziba Razadora", 352: "Siedziba Nemere", 419: "Poszukiwanie skarbów",
}
MINI_BOSS_RANGES = ((191, 194), (491, 494), (681, 681), (2181, 2182), (781, 782), (1181, 1182),
                    (2281, 2282), (2381, 2382))   # constants.cpp IsMiniBoss


# ------------------------------------------------------------- the file ---
def time_value(raw):
    """(time, end) in seconds the way read_line reads "5m-7m" / "1h30m" / "90"."""
    start = end = tmp = 0
    to_end = False
    for ch in raw:
        if ch == "-":
            start += tmp
            tmp = 0
            to_end = True
        elif ch in "hms":
            tmp *= 3600 if ch == "h" else 60 if ch == "m" else 1
            if to_end:
                end += tmp
            else:
                start += tmp
            tmp = 0
        elif ch.isdigit():
            tmp = tmp * 10 + int(ch)
    if tmp > 0:
        if to_end:
            end += tmp
        else:
            start += tmp
    return start, end


def time_raw(start, end=0):
    return f"{int(start)}s" + (f"-{int(end)}s" if end and end > start else "")


def _int(text):
    match = re.match(r"\s*[-+]?\d+", text or "")
    return int(match.group(0)) if match else 0


def tokenize(text):
    """[(word, line)] as the core's get_word() + read_line() see them: words
    split at blanks and line ends, a word starting "//" skips to the end of
    the line, a word in "..." may hold blanks."""
    if '"' in text:
        return tokenize_exact(text)
    # without quotes the same, a line at a time (ten times faster)
    out = []
    for number, raw in enumerate(text.split("\n")):
        for word in WORD_RE.findall(raw):
            if word.startswith("//"):
                break
            out.append((word, number))
    return out


def tokenize_exact(text):
    """tokenize() a character at a time, as get_word() reads."""
    out, i, n, line = [], 0, len(text), 0
    while i < n:
        ch = text[i]
        if ch in " \t\r\n":
            if ch == "\n":
                line += 1
            i += 1
            continue
        first = line
        if ch == '"':
            j = text.find('"', i + 1)
            word = text[i + 1:] if j < 0 else text[i + 1:j]
            line += word.count("\n")
            i = n if j < 0 else j + 1
            out.append((word, first))
            continue
        j = i
        while j < n and text[j] not in " \t\r\n":
            j += 1
            if j - i == 2 and text[i:j] == "//":
                break
        word = text[i:j]
        i = j
        if word.startswith("//"):
            k = text.find("\n", i)
            if k < 0:
                i = n
            else:
                i = k + 1
                line += 1
            continue
        out.append((word, first))
    return out


def parse(text):
    """The regen file -> {"lines", "rows", "eol", "final_eol", "editable", "problems"}.

    Every line is kept as it was ("text"); a line that holds exactly one
    entry carries it ("row"), and rendering writes back the lines untouched
    except the rows changed here - so a file the panel only reads comes back
    byte for byte. A file whose entries the core would read differently from
    one-entry-per-line (an entry over two lines, two on one, quotes) is
    shown but not editable."""
    problems = []
    entries, current = [], None
    for word, line in tokenize(text):
        if current is None:
            if not word or word[0] not in "mgrse":
                problems.append(f"linia {line + 1}: nieznany typ „{word[:12]}” – rdzeń gry zatrzymałby się na nim")
                break
            current = {"words": [word], "start": line}
        else:
            current["words"].append(word)
        need = 6 if current["words"][0][0] == "e" else 11
        if len(current["words"]) == need:
            current["end"] = line
            entries.append(current)
            current = None
    if current is not None:
        problems.append(f"linia {current['start'] + 1}: niepełny wpis na końcu pliku – gra go pomija")

    eol = "\r\n" if "\r\n" in text else "\n"
    final_eol = text.endswith("\n")
    raw_lines = text.split("\n")
    if final_eol:
        raw_lines.pop()
    lines = [{"text": raw[:-1] if raw.endswith("\r") else raw, "cr": raw.endswith("\r"), "row": None}
             for raw in raw_lines]
    editable = '"' not in text
    by_line = {}
    for entry in entries:
        if entry["start"] != entry["end"] or entry["start"] in by_line:
            editable = False
        by_line.setdefault(entry["start"], []).append(entry)
    for number, found in by_line.items():
        if len(found) != 1 or number >= len(lines):
            editable = False
            continue
        words = found[0]["words"]
        line = lines[number]
        found_words = []
        for match in WORD_RE.finditer(line["text"]):
            found_words.append(match.group(0))
            if len(found_words) == len(words):
                break
        if found_words != words:
            editable = False
            continue
        line["suffix"] = line["text"][match.end():]
        line["row"] = make_row(words)
    # a line with words that are not exactly one entry (a part of one)
    if editable:
        for _word, number in tokenize(text):
            if number >= len(lines) or lines[number]["row"] is None:
                editable = False
                break
    rows = [line["row"] for line in lines if line["row"] is not None]
    return {"lines": lines, "rows": rows, "eol": eol, "final_eol": final_eol, "editable": editable,
            "problems": problems, "entries": len(entries)}


def make_row(words):
    kind = words[0][0]
    row = {"type": kind, "flag": words[0][1:2] if words[0][1:2] in ("a", "h", "d") else "", "type_raw": words[0],
           "x": _int(words[1]), "y": _int(words[2]), "sx": _int(words[3]), "sy": _int(words[4]),
           "z": _int(words[5]), "words": list(words), "dirty": False}
    if kind == "e":
        row.update(dir=0, time_raw="0", time=0, time_to=0, percent="0", count=0, vnum=0)
        return row
    start, end = time_value(words[7])
    row.update(dir=_int(words[6]), time_raw=words[7], time=start, time_to=end, percent=words[8],
               count=_int(words[9]), vnum=_int(words[10]))
    return row


def row_words(row):
    if not row["dirty"]:
        return row["words"]
    kind = row["type"] + (row["flag"] if row["type"] in "mgrs" else "")
    if row["type"] == "e":
        return [kind] + [str(int(row[k])) for k in ("x", "y", "sx", "sy", "z")]
    return [kind] + [str(int(row[k])) for k in ("x", "y", "sx", "sy", "z", "dir")] + [
        row["time_raw"], str(row["percent"]), str(int(row["count"])), str(int(row["vnum"]))]


def render(doc):
    out = []
    for line in doc["lines"]:
        row = line["row"]
        text = line["text"]
        if row is not None and row["dirty"]:
            text = "\t".join(row_words(row)) + line.get("suffix", "")
        out.append(text)
    text = doc["eol"].join(out)
    if doc["final_eol"] or any(line.get("new") for line in doc["lines"]):
        text += doc["eol"]
    return text


def touch(row, **changes):
    row.update(changes)
    if "time" in changes or "time_to" in changes:
        row["time_raw"] = time_raw(row["time"], row.get("time_to") or 0)
    row["dirty"] = True


def add_row(doc, row):
    row = dict({"flag": "", "z": 0, "dir": 0, "percent": "100", "time_to": 0, "words": []}, **row)
    touch(row, time=row["time"], time_to=row["time_to"])
    if doc["lines"] and doc["lines"][-1]["text"] == "" and not doc["final_eol"]:
        doc["lines"].pop()
    doc["final_eol"] = True
    doc["lines"].append({"text": "", "row": row, "suffix": "", "new": True})
    doc["rows"].append(row)
    return row


def delete_rows(doc, indexes):
    doomed = {id(doc["rows"][i]) for i in indexes if 0 <= i < len(doc["rows"])}
    doc["lines"] = [line for line in doc["lines"] if line["row"] is None or id(line["row"]) not in doomed]
    doc["rows"] = [row for row in doc["rows"] if id(row) not in doomed]
    return len(doomed)


def round_half(value):
    return int(math.floor(value + 0.5))


def scale_counts(doc, factor, eligible=None):
    changed = 0
    for row in doc["rows"]:
        if row["type"] == "e" or (eligible is not None and not eligible(row)):
            continue
        new = max(1, min(MAX_COUNT, round_half(row["count"] * factor)))
        if new != row["count"]:
            touch(row, count=new)
            changed += 1
    return changed


def scale_times(doc, factor, eligible=None):
    changed = 0
    for row in doc["rows"]:
        if row["type"] == "e" or row["time"] <= 0 or (eligible is not None and not eligible(row)):
            continue
        start = max(1, min(MAX_TIME, round_half(row["time"] * factor)))
        end = max(start, min(MAX_TIME, round_half(row["time_to"] * factor))) if row["time_to"] else 0
        if (start, end) != (row["time"], row["time_to"]):
            touch(row, time=start, time_to=end if end > start else 0)
            changed += 1
    return changed


# ------------------------------------------------------ groups, settings ---
def parse_groups(text, kind="group"):
    """group.txt / group_group.txt -> {vnum: {"name", "members": [vnums]}} the
    way CMobManager::LoadGroup / LoadGroupGroup read them (leader first; for
    group_group the members are group numbers)."""
    groups = {}
    name, body, depth = None, None, 0
    for raw in (text or "").splitlines():
        # CTextFileLoader: a word in "..." may hold blanks ("Hungriger Wildhund")
        words = [w[1:-1] if w.startswith('"') and w.endswith('"') and len(w) > 1 else w
                 for w in GROUP_WORD_RE.findall(raw)]
        if not words or words[0].startswith(("//", "#", "--")):
            continue
        key = words[0].lower()
        if body is None:
            if key == "group":
                name = words[1] if len(words) > 1 else ""
                body = {}
            continue
        if words[0] == "{":
            depth += 1
            continue
        if words[0] == "}":
            vnum = body.get("vnum", [""])[0]
            if vnum.isdigit():
                members = []
                if kind == "group":
                    leader = body.get("leader", [])
                    if len(leader) >= 2 and leader[1].isdigit():
                        members.append(int(leader[1]))
                    else:
                        members = None
                for k in range(1, 256):
                    entry = body.get(str(k))
                    if not entry:
                        break
                    value = entry[1] if kind == "group" and len(entry) > 1 else entry[0]
                    if members is not None and value.isdigit() and int(value):
                        members.append(int(value))
                if members is not None:
                    groups[int(vnum)] = {"vnum": int(vnum), "name": name, "members": members}
            body, depth = None, 0
            continue
        body.setdefault(key, words[1:])
    return groups


def parse_setting(text):
    """Setting.txt -> (width, height) in the regen file's metres, or None."""
    size = re.search(r"(?im)^\s*MapSize\s+(\d+)\s+(\d+)", text or "")
    cell = re.search(r"(?im)^\s*CellScale\s+(\d+)", text or "")
    if not size or not cell or not int(cell.group(1)):
        return None
    scale = int(cell.group(1)) * 128
    return int(size.group(1)) * scale // 100, int(size.group(2)) * scale // 100


def parse_index(text):
    result = []
    for raw in (text or "").splitlines():
        words = raw.split()
        if len(words) >= 2 and words[0].isdigit() and not raw.lstrip().startswith(("/", "#")):
            result.append((int(words[0]), words[1]))
    return result


def is_mini_boss(vnum):
    return any(low <= vnum <= high for low, high in MINI_BOSS_RANGES)


# ------------------------------------------------------------- the spool ---
def root(spool):
    return Path(spool) / FOLDER


def base_dir(spool):
    override = os.environ.get("DBE_REGEN_BASE")
    return Path(override) if override else root(spool) / "base"


def custom_path(spool, folder):
    return root(spool) / "custom" / f"{folder}.txt"


def meta_path(spool, folder):
    return root(spool) / "custom" / f"{folder}.meta.json"


def status_path(spool, folder):
    return root(spool) / "status" / f"{folder}.status"


def backup_dir(spool):
    return root(spool) / "backup"


def base_file(spool, folder, name="regen.txt"):
    return base_dir(spool) / "maps" / folder / name


def read_latin(path):
    try:
        return Path(path).read_bytes().decode("latin-1")
    except OSError:
        return None


def sha_text(text):
    return hashlib.sha256(text.encode("latin-1")).hexdigest() if text is not None else ""


def map_list(spool):
    """[(index, folder)] of the maps the game published, in the index order."""
    published = set()
    try:
        published = {p.name for p in (base_dir(spool) / "maps").iterdir() if (p / "regen.txt").is_file()}
    except OSError:
        pass
    result, seen = [], set()
    for index, folder in parse_index(read_latin(base_dir(spool) / "index")):
        if folder in published and folder not in seen:
            result.append((index, folder))
            seen.add(folder)
    result += [(0, folder) for folder in sorted(published - seen)]
    return result


def map_names():
    names = dict(EXTRA_MAP_NAMES)
    panel = df.panel_function("MAP_NAMES")
    if isinstance(panel, dict):
        names.update(panel)
    return names


def map_label(index, folder, names=None):
    names = map_names() if names is None else names
    if index and index in names:
        return names[index]
    return f"{folder} (mapa #{index})" if index else folder


def effective_text(spool, folder):
    """(text, "custom"|"image"|None)."""
    text = read_latin(custom_path(spool, folder))
    if text is not None:
        return text, "custom"
    text = read_latin(base_file(spool, folder))
    return text, ("image" if text is not None else None)


_SUMMARY_CACHE = {}


def map_summary(spool, folder):
    """{"rows", "monsters", "groups", "source"} of a map for the list page,
    cached by the file's size and time (73 maps, ~30 000 lines)."""
    path = custom_path(spool, folder)
    source = "custom"
    if not path.exists():
        path, source = base_file(spool, folder), "image"
    try:
        info = path.stat()
        key = (str(path), info.st_mtime_ns, info.st_size)
    except OSError:
        return {"rows": 0, "monsters": 0, "groups": 0, "source": None}
    cached = _SUMMARY_CACHE.get(folder)
    if cached and cached[0] == key:
        return cached[1]
    spawns = [r for r in parse(read_latin(path) or "")["rows"] if r["type"] != "e"]
    summary = {"rows": len(spawns), "source": source,
               "monsters": sum(r["count"] for r in spawns if r["type"] in ("m", "s")),
               "groups": sum(r["count"] for r in spawns if r["type"] in ("g", "r"))}
    _SUMMARY_CACHE[folder] = (key, summary)
    return summary


def _prepare_folder(folder):
    df._prepare_folder(folder)


def write_custom(spool, folder, text, reason=""):
    """The map's whole file into the spool (the previous version to backup/
    first, under the folder's lock). None, or a text equal to the image's
    file, removes the custom file - the game goes back to the image's."""
    import fcntl
    path = custom_path(spool, folder)
    backups = backup_dir(spool)
    for folder_path in (root(spool), path.parent, backups):
        _prepare_folder(folder_path)
    base = read_latin(base_file(spool, folder))
    if text is not None and base is not None and text == base:
        text = None
    with open(root(spool) / ".lock", "w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")[:-3]
        backup = backups / f"{folder}.{stamp}.txt"
        backup.write_bytes(path.read_bytes() if path.exists() else NO_CHANGES_MARK)
        if text is None:
            for doomed in (path, meta_path(spool, folder)):
                try:
                    doomed.unlink()
                except FileNotFoundError:
                    pass
        else:
            temporary = path.parent / f"{path.name}.new{os.getpid()}"
            temporary.write_bytes(text.encode("latin-1"))
            os.chmod(temporary, 0o664)
            os.replace(temporary, path)
            meta = path.parent / f".{folder}.meta.new{os.getpid()}"
            meta.write_text(json.dumps({"base_sha": sha_text(base), "time": int(datetime.now().timestamp()),
                                        "reason": reason}), encoding="utf-8")
            os.chmod(meta, 0o664)
            os.replace(meta, meta_path(spool, folder))
        for old in sorted(backups.glob(f"{folder}.*.txt"))[:-BACKUP_KEEP]:
            try:
                old.unlink()
            except OSError:
                pass
    mark_pending(spool, folder, reason)
    return backup.name


def list_backups(spool, folder, limit=30):
    result = []
    for path in backup_dir(spool).glob(f"{folder}.*.txt"):
        if not re.fullmatch(re.escape(folder) + r"\.[0-9-]+\.txt", path.name):
            continue
        try:
            data = path.read_bytes()
            result.append({"name": path.name, "time": datetime.fromtimestamp(path.stat().st_mtime),
                           "empty": data.startswith(NO_CHANGES_MARK), "size": len(data)})
        except OSError:
            continue
    result.sort(key=lambda b: b["name"], reverse=True)
    return result[:limit]


def backup_path(spool, folder, name):
    if not re.fullmatch(re.escape(folder) + r"\.[0-9-]+\.txt", name or ""):
        return None
    path = backup_dir(spool) / name
    return path if path.is_file() else None


def restore_backup(spool, folder, name):
    path = backup_path(spool, folder, name)
    if path is None:
        raise ValueError("Nie ma takiej kopii.")
    data = path.read_bytes()
    return write_custom(spool, folder, None if data.startswith(NO_CHANGES_MARK) else data.decode("latin-1"),
                        f"przywrocono kopie {name}")


def live_state(spool, folder):
    """How the game took the map's custom file: ok / pending / error / unknown."""
    path = custom_path(spool, folder)
    sha = df.file_sha(path) if path.exists() else ""
    status = df.read_values(status_path(spool, folder))
    when = datetime.fromtimestamp(int(status["time"])) if status.get("time", "").isdigit() else None
    if not status:
        if not sha:
            return {"kind": "ok", "pending": False, "time": None,
                    "text": "Bez zmian z panelu – gra używa pliku z obrazu."}
        return {"kind": "unknown", "pending": True, "time": None,
                "text": "Zapisane. Gra nie zgłosiła jeszcze, że wczytała ten plik (obraz gry bez tej funkcji albo "
                        "rdzenie jeszcze nie startowały) – zadziała po restarcie na aktualnym obrazie gry."}
    if status.get("state") == "rejected" and status.get("sha") == sha:
        return {"kind": "error", "pending": False, "time": when,
                "text": "Gra ODRZUCIŁA zapisany plik i działa na pliku z obrazu: " + status.get("message", "")}
    if (status.get("sha") or "") == sha:
        return {"kind": "ok", "pending": False, "time": when,
                "text": "W grze działa dokładnie to, co widzisz." if sha else
                        "Bez zmian z panelu – gra używa pliku z obrazu."}
    return {"kind": "pending", "pending": True, "time": when,
            "text": "Zapisane zmiany czekają na restart rdzeni gry (przycisk „Zastosuj”)."
                    if sha else "Powrót do pliku z obrazu czeka na restart rdzeni gry (przycisk „Zastosuj”)."}


def pending_dir(spool):
    return Path(spool) / "dbeditor" / "pending"


def mark_pending(spool, folder, detail=""):
    folder_path = pending_dir(spool)
    try:
        _prepare_folder(folder_path.parent)
        _prepare_folder(folder_path)
        data = {"part": "regen", "file": folder, "detail": detail, "time": int(datetime.now().timestamp())}
        temporary = folder_path / f".regen-{folder}.json.new{os.getpid()}"
        temporary.write_text(json.dumps(data, ensure_ascii=False), encoding="utf-8")
        os.chmod(temporary, 0o664)
        os.replace(temporary, folder_path / f"regen-{folder}.json")
    except OSError:
        pass


def pending_changes(spool):
    """[{key, title, kind, text, detail, time}] of the maps whose saved spawns
    the game does not run yet (or rejected) - the "Zastosuj" page's list;
    the same shape as dropfiles.pending_changes()."""
    folders = set()
    for sub, suffix in (("custom", ".txt"), ("status", ".status")):
        try:
            for path in (root(spool) / sub).iterdir():
                if path.name.endswith(suffix) and FOLDER_RE.fullmatch(path.name[:-len(suffix)]):
                    folders.add(path.name[:-len(suffix)])
        except OSError:
            pass
    if not folders:
        return []
    names = map_names()
    indexes = {folder: index for index, folder in parse_index(read_latin(base_dir(spool) / "index"))}
    result = []
    for folder in sorted(folders):
        state = live_state(spool, folder)
        flag = pending_dir(spool) / f"regen-{folder}.json"
        if state["pending"] or state["kind"] == "error":
            info = {}
            try:
                info = json.loads(flag.read_text(encoding="utf-8"))
            except (OSError, ValueError):
                pass
            result.append({"key": "regen-" + folder,
                           "title": "Spawny potworów: " + map_label(indexes.get(folder, 0), folder, names),
                           "kind": state["kind"], "text": state["text"], "detail": info.get("detail", ""),
                           "time": info.get("time")})
        else:
            try:
                flag.unlink()
            except OSError:
                pass
    return result


# -------------------------------------------------------------- words ---
def plural(n, one, few, many):
    n = abs(int(n))
    if n == 1:
        return one
    if 2 <= n % 10 <= 4 and not 12 <= n % 100 <= 14:
        return few
    return many


def duration_text(seconds):
    seconds = int(seconds)
    if seconds <= 0:
        return "0 s"
    if seconds < 120:
        return f"{seconds} s"
    if seconds < 7200:
        minutes, rest = divmod(seconds, 60)
        return f"{minutes} min" + (f" {rest} s" if rest else "")
    hours, rest = divmod(seconds, 3600)
    return f"{hours} h" + (f" {rest // 60} min" if rest // 60 else "")


def time_text(start, end=0):
    if start <= 0 and not end:
        return "nigdy (czas 0 – gra nie odradza tego wpisu)"
    if end and end > start:
        return f"co {duration_text(start)}–{duration_text(end)} (losowo)"
    return f"co {duration_text(start)}"


def form_int(form, name, label, low, high, default=None):
    raw = (form.get(name) or "").strip().replace(" ", "")
    if raw == "" and default is not None:
        return default
    if not re.fullmatch(r"\d{1,9}", raw):
        raise ValueError(f"{label}: „{raw}” to nie liczba całkowita.")
    value = int(raw)
    if not low <= value <= high:
        raise ValueError(f"{label}: dozwolone {low}–{high}.")
    return value


def form_factor(raw, low=0.1, high=10.0):
    raw = (raw or "").strip().replace(",", ".").lstrip("×x*")
    if not re.fullmatch(r"\d{1,3}(\.\d{1,3})?", raw):
        raise ValueError("Mnożnik: wpisz liczbę, np. 2 albo 0,5.")
    value = float(raw)
    if not low <= value <= high or value == 1:
        raise ValueError(f"Mnożnik: dozwolone {low:g}–{high:g} (bez 1).".replace(".", ","))
    return value


# ------------------------------------------------------------ install ---
def install(bp, ctx):
    import dbeditor
    df.register(bp)
    rows_db, game_text, login_required = ctx["rows"], ctx["game_text"], ctx["login_required"]

    # ------------------------------------------------------------ data ---
    def mobs():
        if "dbe_mobs" not in g:
            g.dbe_mobs = df.mob_table(rows_db, game_text)
        return g.dbe_mobs

    _group_cache = {}

    def groups(kind):
        """group.txt / group_group.txt as the game published them (cached by mtime)."""
        path = base_dir(df.spool_dir()) / ("group.txt" if kind == "group" else "group_group.txt")
        try:
            stamp = (str(path), path.stat().st_mtime_ns)
        except OSError:
            return None
        cached = _group_cache.get(kind)
        if cached and cached[0] == stamp:
            return cached[1]
        parsed = parse_groups(read_latin(path), kind)
        _group_cache[kind] = (stamp, parsed)
        return parsed

    def mob_name(vnum):
        info = mobs().get(vnum)
        return (info["name"] or f"Potwór {vnum}") if info else f"NIEZNANY POTWÓR {vnum}"

    def memo(name, function):
        def wrapped(vnum):
            cache = g.setdefault(name, {})
            if vnum not in cache:
                cache[vnum] = function(vnum)
            return cache[vnum]
        return wrapped

    def group_desc_uncached(vnum):
        group = (groups("group") or {}).get(vnum)
        if not group:
            return None
        counts = {}
        for member in group["members"]:
            counts[member] = counts.get(member, 0) + 1
        parts = [(f"{n}× " if n > 1 else "") + mob_name(m) for m, n in counts.items()]
        return {"text": " + ".join(parts), "size": len(group["members"]), "members": group["members"],
                "name": group["name"]}

    def group_group_desc_uncached(vnum):
        group = (groups("group_group") or {}).get(vnum)
        if not group:
            return None
        inner = [group_desc(v) for v in group["members"]]
        texts = [d["text"] for d in inner if d]
        members = [m for d in inner if d for m in d["members"]]
        shown = " / ".join(texts[:3]) + (f" / … (+{len(texts) - 3})" if len(texts) > 3 else "")
        return {"text": shown, "choices": len(group["members"]), "members": members,
                "sizes": sorted({d["size"] for d in inner if d}), "missing": len(texts) != len(group["members"])}

    group_desc = memo("dbr_group_desc", group_desc_uncached)
    group_group_desc = memo("dbr_group_group_desc", group_group_desc_uncached)

    def member_vnums(row):
        if row["type"] in ("m", "s"):
            return [row["vnum"]]
        if row["type"] == "g":
            desc = group_desc(row["vnum"])
            return desc["members"] if desc else []
        if row["type"] == "r":
            desc = group_group_desc(row["vnum"])
            return desc["members"] if desc else []
        return []

    def is_boss_row(row):
        for vnum in member_vnums(row):
            info = mobs().get(vnum)
            if (info and info["rank"] >= 4) or is_mini_boss(vnum):
                return True
        return False

    def counts_multiplied(row):
        """regen_target_count: only lines of monsters and stones only."""
        members = member_vnums(row)
        return bool(members) and all(mobs().get(v) and mobs()[v]["type"] in (0, 2) for v in members)

    def server_flags(index):
        names = ["fastMobSpawn", "fastBossSpawn", "m2_mob_count", "m2_boss_count"]
        if index:
            names += [f"fastMobSpawn{index}", f"fastBossSpawn{index}"]
        values = {}
        try:
            marks = ",".join(["%s"] * len(names))
            for row in rows_db(f"SELECT szName,lValue FROM player.quest WHERE dwPID=0 AND szName IN ({marks})", names):
                values[row["szName"]] = int(row.get("lValue") or 0)
        except Exception:  # the page works without the database's flags
            values["_error"] = 1

        def delay(kind):
            per_map = max(0, min(100, values.get(f"{kind}{index}", 0))) if index else 0
            return (per_map, "tej mapy") if per_map else (max(0, min(100, values.get(kind, 0))), "całego serwera")

        return {"mob_delay": delay("fastMobSpawn"), "boss_delay": delay("fastBossSpawn"),
                "mob_count": values.get("m2_mob_count", 0), "boss_count": values.get("m2_boss_count", 0),
                "error": "_error" in values}

    def effective(row, flags, index):
        boss = is_boss_row(row)
        percent, _where = flags["boss_delay" if boss else "mob_delay"]
        start, end = row["time"], row["time_to"]
        if percent and start > 0:
            start = max(3, start * percent // 100)
            end = max(3, end * percent // 100) if end else 0
        count = row["count"]
        count_pct = flags["boss_count" if boss else "mob_count"]
        if count_pct > 100 and index < 10000 and counts_multiplied(row):
            count = count * min(count_pct, 400) // 100
        return {"time": start, "time_to": end, "count": count, "boss": boss,
                "changed": (start, end, count) != (row["time"], row["time_to"], row["count"])}

    def describe(row, flags=None, index=0):
        kind = row["type"]
        bad = False
        unit = ("potwór", "potwory", "potworów")
        if kind in ("m", "s"):
            info = mobs().get(row["vnum"])
            bad = info is None
            what = f"{TYPE_NAMES[kind]}: {mob_name(row['vnum'])}" + (f" (poz. {info['level']})" if info else "")
        elif kind == "g":
            desc = group_desc(row["vnum"])
            bad = desc is None
            what = (f"Grupa: {desc['text']} ({desc['size']} szt.)" if desc
                    else f"Grupa {row['vnum']} – BRAK TAKIEJ GRUPY w group.txt")
            unit = ("grupa", "grupy", "grup")
        elif kind == "r":
            desc = group_group_desc(row["vnum"])
            bad = desc is None or desc["missing"]
            what = (f"Losowa grupa (1 z {desc['choices']}): {desc['text']}" if desc
                    else f"Losowa grupa {row['vnum']} – BRAK W group_group.txt")
            unit = ("grupa", "grupy", "grup")
        else:
            return {"text": f"Strefa bez spawnów · ({row['x']}, {row['y']}) ± {row['sx']}×{row['sy']} m", "bad": False,
                    "what": "Strefa bez spawnów", "where": "", "when": "", "count": "", "eff": None}
        if kind == "s":
            where = "w losowym miejscu mapy"
        elif row["sx"] == 0 and row["sy"] == 0:
            where = f"dokładnie w punkcie ({row['x']}, {row['y']})"
        elif row["sx"] == row["sy"]:
            where = f"miejsce ({row['x']}, {row['y']}) ± {row['sx']} m"
        else:
            where = f"miejsce ({row['x']}, {row['y']}) ± {row['sx']}×{row['sy']} m"
        when = time_text(row["time"], row["time_to"])
        count = f"naraz {row['count']} {plural(row['count'], *unit)}"
        eff = effective(row, flags, index) if flags else None
        text = f"{what} · {where} · {when} · {count}"
        if row["flag"]:
            text += f" · {FLAG_NAMES[row['flag']]}"
        return {"text": text, "what": what, "where": where, "when": when, "count": count, "bad": bad, "eff": eff}

    def load(folder):
        """The map's state for a page: abort 404 for a map the game did not publish."""
        if not FOLDER_RE.fullmatch(folder or ""):
            abort(404)
        spool = df.spool_dir()
        maps = dict((f, i) for i, f in map_list(spool))
        if folder not in maps:
            abort(404)
        text, source = effective_text(spool, folder)
        if text is None:
            abort(404)
        doc = parse(text)
        size = parse_setting(read_latin(base_file(spool, folder, "Setting.txt")))
        return {"spool": spool, "folder": folder, "index": maps[folder], "text": text, "source": source,
                "doc": doc, "rev": sha_text(text), "size": size}

    def check_row(st, row):
        """The panel's checks of one new or changed line (the game checks again)."""
        kind = row["type"]
        if kind in ("m", "s") and row["vnum"] not in mobs():
            raise ValueError(f"Nie ma potwora o numerze {row['vnum']} (mob_proto).")
        if kind == "g" and row["vnum"] not in (groups("group") or {}):
            raise ValueError(f"Nie ma grupy {row['vnum']} w group.txt.")
        if kind == "r" and row["vnum"] not in (groups("group_group") or {}):
            raise ValueError(f"Nie ma zestawu grup {row['vnum']} w group_group.txt.")
        if st["size"]:
            width, height = st["size"]
            if not (0 <= row["x"] <= width and 0 <= row["y"] <= height):
                raise ValueError(f"Miejsce ({row['x']}, {row['y']}) leży poza mapą – ta mapa ma {width}×{height} m "
                                 f"(x 0–{width}, y 0–{height}).")
            if row["sx"] > width or row["sy"] > height:
                raise ValueError("Promień większy niż cała mapa.")
        if kind != "e":
            if not 1 <= row["time"] <= MAX_TIME:
                raise ValueError(f"Czas odrodzenia: 1–{MAX_TIME} s.")
            if not 1 <= row["count"] <= MAX_COUNT:
                raise ValueError(f"Liczba naraz: 1–{MAX_COUNT}.")

    def save(st, reason):
        doc = st["doc"]
        if len(doc["rows"]) > MAX_ROWS:
            raise ValueError(f"Za dużo wpisów na jednej mapie (najwyżej {MAX_ROWS}).")
        text = render(doc)
        again = parse(text)
        if not again["editable"] or again["problems"] or len(again["rows"]) != len(doc["rows"]):
            raise ValueError("Wynikowy plik nie przeszedł kontroli (" + "; ".join(again["problems"]) + ").")
        write_custom(st["spool"], st["folder"], text, reason)
        return text

    def guard(st):
        """None when the form may change the file, else a redirect."""
        back = redirect(url_for("dbeditor.regen_map", folder=st["folder"]))
        if not df.csrf_ok(request.form):
            flash("Sesja formularza wygasła – odśwież stronę i spróbuj jeszcze raz.", "error")
            return back
        if request.form.get("rev") != st["rev"]:
            flash("Plik tej mapy zmienił się w międzyczasie (inna karta albo inna osoba) – sprawdź go i zapisz jeszcze raz.",
                  "error")
            return back
        if not st["doc"]["editable"]:
            flash("Tego pliku nie da się bezpiecznie edytować w panelu (nietypowy układ) – zmiany nie zapisano.", "error")
            return back
        return None

    def row_index(st):
        try:
            index = int(request.form.get("row", ""))
        except ValueError:
            index = -1
        if not 0 <= index < len(st["doc"]["rows"]):
            raise ValueError("Nie ma takiego wpisu – odśwież stronę.")
        return index

    def row_from_form(form, kind):
        time_from = form_int(form, "time", "Czas odrodzenia (s)", 1, MAX_TIME)
        time_to = form_int(form, "time_to", "Czas „do” (s)", 0, MAX_TIME, default=0)
        if time_to and time_to <= time_from:
            time_to = 0
        flag = form.get("flag", "")
        if flag not in FLAG_NAMES:
            flag = ""
        radius_x = form_int(form, "sx", "Promień X (m)", 0, MAX_RADIUS)
        values = {"x": form_int(form, "x", "Miejsce X", 0, 100000), "y": form_int(form, "y", "Miejsce Y", 0, 100000),
                  "sx": radius_x, "sy": form_int(form, "sy", "Promień Y (m)", 0, MAX_RADIUS, default=radius_x),
                  "time": time_from, "time_to": time_to,
                  "count": form_int(form, "count", "Liczba naraz", 1, MAX_COUNT),
                  "percent": str(form_int(form, "percent", "Procent", 0, 100, default=100)), "flag": flag}
        if kind == "s":
            values["flag"] = flag if flag != "h" else ""
        return values

    # ----------------------------------------------------------- pages ---
    @bp.route("/regen")
    @login_required
    def regen_index():
        spool = df.spool_dir()
        names = map_names()
        maps = []
        for index, folder in map_list(spool):
            maps.append(dict(map_summary(spool, folder), index=index, folder=folder, named=index in names,
                             label=map_label(index, folder, names), state=live_state(spool, folder)))
        maps.sort(key=lambda m: (not m["named"], m["rows"] == 0, m["label"].casefold()))
        return render_template("dbeditor/regen.html", maps=maps, published=bool(maps),
                               pending=pending_changes(spool))

    @bp.route("/regen/<folder>")
    @login_required
    def regen_map(folder):
        st = load(folder)
        doc, index = st["doc"], st["index"]
        flags = server_flags(index)
        rows_view, markers, summary = [], [], {}
        width, height = st["size"] or (0, 0)
        for number, row in enumerate(doc["rows"]):
            words = describe(row, flags, index)
            rows_view.append({"n": number, "row": row, **words})
            if row["type"] != "e":
                key = (row["type"] in ("g", "r"), row["type"], row["vnum"])
                item = summary.setdefault(key, {"what": words["what"], "rows": 0, "count": 0, "times": set(),
                                                "bad": words["bad"], "group": key[0]})
                item["rows"] += 1
                item["count"] += row["count"]
                item["times"].add(row["time"])
            if width and height and row["type"] not in ("s",):
                markers.append({"n": number, "px": round(row["x"] * 100 / width, 3), "py": round(row["y"] * 100 / height, 3),
                                "rx": round(max(row["sx"], 1) * 100 / width, 3),
                                "ry": round(max(row["sy"], 1) * 100 / height, 3),
                                "kind": "e" if row["type"] == "e" else "bad" if words["bad"] else
                                        "boss" if words["eff"] and words["eff"]["boss"] else row["type"],
                                "title": words["what"]})
        summary = sorted(summary.values(), key=lambda s: (-s["count"], s["what"]))
        for item in summary:
            times = sorted(item["times"])
            item["time"] = duration_text(times[0]) + (f"–{duration_text(times[-1])}" if len(times) > 1 else "")
        edit = None
        if request.args.get("edit", "").isdigit():
            number = int(request.args["edit"])
            if 0 <= number < len(rows_view):
                edit = rows_view[number]
        state = live_state(st["spool"], folder)
        meta = {}
        try:
            meta = json.loads(meta_path(st["spool"], folder).read_text(encoding="utf-8"))
        except (OSError, ValueError):
            pass
        base_text = read_latin(base_file(st["spool"], folder))
        drift = st["source"] == "custom" and meta.get("base_sha") and meta["base_sha"] != sha_text(base_text)
        image = MAP_IMAGES.get(index)
        image_url = url_for("static", filename=f"maps/{image}.{'webp' if index in (108, 109) else 'png'}") if image else None
        totals = {"rows": len([r for r in doc["rows"] if r["type"] != "e"]),
                  "monsters": sum(r["count"] for r in doc["rows"] if r["type"] in ("m", "s")),
                  "groups": sum(r["count"] for r in doc["rows"] if r["type"] in ("g", "r"))}
        return render_template(
            "dbeditor/regen_map.html", st=st, doc=doc, rows=rows_view, markers=markers, summary=summary,
            edit=edit, state=state, drift=drift, image_url=image_url, flags=flags, totals=totals,
            label=map_label(index, folder), csrf=df.csrf_token(), backups=list_backups(st["spool"], folder),
            flag_names=FLAG_NAMES, type_names=TYPE_NAMES, count_factors=COUNT_FACTORS, time_factors=TIME_FACTORS,
            max_count=MAX_COUNT, max_time=MAX_TIME, max_radius=MAX_RADIUS,
            has_respawns="respawns" in current_app.view_functions,
            time_text=time_text)

    @bp.route("/regen/<folder>/regen.txt")
    @login_required
    def regen_file(folder):
        st = load(folder)
        return st["text"], 200, {"Content-Type": "text/plain; charset=latin-1"}

    @bp.route("/regen/<folder>/backup/<name>")
    @login_required
    def regen_backup_view(folder, name):
        st = load(folder)
        path = backup_path(st["spool"], folder, name)
        if path is None:
            abort(404)
        return path.read_bytes().decode("latin-1"), 200, {"Content-Type": "text/plain; charset=utf-8"}

    @bp.route("/regen/api/search")
    @login_required
    def regen_search():
        query = (request.args.get("q") or "").strip()
        if not query or (len(query) < 2 and not query.isdigit()):
            return jsonify({"mobs": [], "groups": []})
        needle = query.casefold()
        found = []
        for info in mobs().values():
            if str(info["vnum"]) == query or (not query.isdigit() and needle in (info["name"] or "").casefold()):
                found.append(info)
        found.sort(key=lambda m: (str(m["vnum"]) != query, m["type"] != 0, m["level"], m["vnum"]))
        mob_list = [{"vnum": m["vnum"], "name": m["name"], "level": m["level"], "kind": m["kind_name"]}
                    for m in found[:25]]
        group_list = []
        for vnum in sorted(groups("group") or {}):
            desc = group_desc(vnum)
            hay = (desc["text"] + " " + desc["name"]).casefold()
            if str(vnum) == query or (not query.isdigit() and needle in hay) or \
                    (query.isdigit() and int(query) in desc["members"]):
                group_list.append({"type": "g", "vnum": vnum, "text": f"{desc['text']} ({desc['size']} szt.)"})
            if len(group_list) >= 25:
                break
        for vnum in sorted(groups("group_group") or {}):
            desc = group_group_desc(vnum)
            if str(vnum) == query or (not query.isdigit() and needle in desc["text"].casefold()):
                group_list.append({"type": "r", "vnum": vnum, "text": f"1 z {desc['choices']}: {desc['text']}"})
            if len(group_list) >= 40:
                break
        return jsonify({"mobs": mob_list, "groups": group_list})

    # ----------------------------------------------------------- saves ---
    def done(st, message, edit=None):
        flash(message + " Zadziała po restarcie rdzeni (Zastosuj).", "success")
        return redirect(url_for("dbeditor.regen_map", folder=st["folder"], **({"edit": edit, "_anchor": "edytuj"} if edit is not None else {})))

    def failed(st, exc, edit=None):
        flash(f"Nie zapisano: {exc}", "error")
        return redirect(url_for("dbeditor.regen_map", folder=st["folder"], **({"edit": edit, "_anchor": "edytuj"} if edit is not None else {})))

    @bp.post("/regen/<folder>/edit")
    @login_required
    def regen_edit(folder):
        st = load(folder)
        if (blocked := guard(st)) is not None:
            return blocked
        number = request.form.get("row")
        try:
            index = row_index(st)
            row = st["doc"]["rows"][index]
            if row["type"] == "e":
                raise ValueError("Strefy bez spawnów można tylko usunąć.")
            before = describe(row)["text"]
            values = row_from_form(request.form, row["type"])
            if row["type"] in ("m", "s"):
                values["dir"] = form_int(request.form, "dir", "Kierunek", 0, 8, default=row["dir"])
            changed = {k: v for k, v in values.items() if row.get(k) != v}
            if not changed:
                flash("Nic się nie zmieniło.", "info")
                return redirect(url_for("dbeditor.regen_map", folder=folder, edit=index))
            touch(row, **values)
            check_row(st, row)
            save(st, f"wpis {index + 1}: {df.ascii_comment(before)[:50]}")
        except (ValueError, OSError) as exc:
            return failed(st, exc, number)
        return done(st, f"Zapisano wpis {index + 1}: {describe(row)['text']}.", index)

    @bp.post("/regen/<folder>/add")
    @login_required
    def regen_add(folder):
        st = load(folder)
        if (blocked := guard(st)) is not None:
            return blocked
        try:
            kind = request.form.get("type", "")
            if kind not in ("m", "g", "r", "s"):
                raise ValueError("Wybierz potwora albo grupę z listy.")
            vnum = form_int(request.form, "vnum", "Numer potwora/grupy", 1, 10 ** 9)
            row = dict(row_from_form(request.form, kind), type=kind, vnum=vnum, z=0, dir=0)
            if kind == "s":
                row.update(x=0, y=0, sx=0, sy=0)
            row = add_row(st["doc"], row)
            check_row(st, row)
            text = describe(row)["text"]
            save(st, "nowy wpis: " + df.ascii_comment(text)[:50])
        except (ValueError, OSError) as exc:
            return failed(st, exc)
        return done(st, f"Dodano wpis {len(st['doc']['rows'])}: {text}.", len(st["doc"]["rows"]) - 1)

    @bp.post("/regen/<folder>/delete")
    @login_required
    def regen_delete(folder):
        st = load(folder)
        if (blocked := guard(st)) is not None:
            return blocked
        try:
            picked = sorted({int(v) for v in request.form.getlist("rows") if v.isdigit()})
            if not picked:
                raise ValueError("Zaznacz wpisy do usunięcia.")
            removed = delete_rows(st["doc"], picked)
            save(st, f"usunieto {removed} wpis(y)")
        except (ValueError, OSError) as exc:
            return failed(st, exc)
        return done(st, f"Usunięto {removed} {plural(removed, 'wpis', 'wpisy', 'wpisów')}.")

    @bp.post("/regen/<folder>/scale")
    @login_required
    def regen_scale(folder):
        st = load(folder)
        if (blocked := guard(st)) is not None:
            return blocked
        what = request.form.get("what")
        try:
            factor = form_factor(request.form.get("factor"))
            only = request.form.get("only", "")
            eligible = counts_multiplied if only == "monsters" else None
            if what == "count":
                changed = scale_counts(st["doc"], factor, eligible)
                label = f"liczba potworów ×{factor:g}"
            elif what == "time":
                changed = scale_times(st["doc"], factor, eligible)
                label = f"czas odrodzenia ×{factor:g}"
            else:
                raise ValueError("Nieznana operacja.")
            if not changed:
                raise ValueError("Żaden wpis się nie zmienił (np. liczba naraz jest już 1 albo czas 1 s).")
            save(st, label.replace("×", "x").replace("ł", "l").replace("ó", "o").replace("ś", "s"))
        except (ValueError, OSError) as exc:
            return failed(st, exc)
        return done(st, f"Cała mapa: {label.replace('.', ',')} – zmieniono {changed} "
                        f"{plural(changed, 'wpis', 'wpisy', 'wpisów')}.")

    @bp.post("/regen/<folder>/reset")
    @login_required
    def regen_reset(folder):
        st = load(folder)
        if not df.csrf_ok(request.form):
            flash("Sesja formularza wygasła – odśwież stronę i spróbuj jeszcze raz.", "error")
            return redirect(url_for("dbeditor.regen_map", folder=folder))
        try:
            write_custom(st["spool"], folder, None, "powrot do pliku z obrazu gry")
        except OSError as exc:
            return failed(st, exc)
        return done(st, "Mapa wraca do pliku z obrazu gry (obecna wersja trafiła do kopii).")

    @bp.post("/regen/<folder>/restore")
    @login_required
    def regen_restore(folder):
        st = load(folder)
        if not df.csrf_ok(request.form):
            flash("Sesja formularza wygasła – odśwież stronę i spróbuj jeszcze raz.", "error")
            return redirect(url_for("dbeditor.regen_map", folder=folder))
        try:
            restore_backup(st["spool"], folder, request.form.get("backup", ""))
        except (ValueError, OSError) as exc:
            return failed(st, exc)
        return done(st, "Przywrócono kopię (obecny stan też trafił do kopii).")

    dbeditor.add_section("dbeditor.regen_index", "🗺️", "Spawny potworów",
                         "Co, gdzie i jak często odradza się na każdej mapie – z podglądem mapy.")
