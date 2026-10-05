"""The client files the database editor's changes reach, patched from the
database: gamedata/item_proto (binary MIPX), gamedata/skilltable.txt,
locale/pl/itemdesc.txt and locale/pl/skilldesc.txt.

Only what the operator edited is touched. The client's item_proto differs
from the server's table on purpose in thousands of places (sockets 0 against
-1, client-only types, antiflags the client never needs...), so an edited
item gets just the fields its edit named - or, when the edit did not say,
the SAFE_FIELDS a tooltip shows. A vnum the client does not have at all
gets a whole record made from the database row.
"""
import struct

from . import eterpack

ITEM_KEY = (173217, 72619434, 408587239, 27973291)
RECORD = 184
TEXT_ENCODING = 'cp1250'

# The client record (TItemTable_r156, 184 bytes): field -> (offset, struct
# format, or the byte length of a NUL-terminated string).
ITEM_FIELDS = {
    'name': (8, 33), 'locale_name': (41, 33),
    'type': (74, 'B'), 'subtype': (75, 'B'), 'weight': (76, 'B'), 'size': (77, 'B'),
    'stack': (78, 'I'), 'antiflag': (82, 'I'), 'flag': (86, 'I'), 'wearflag': (90, 'I'),
    'immuneflag': (94, 'I'), 'gold': (98, 'q'), 'shop_buy_price': (106, 'q'),
    'limittype0': (114, 'B'), 'limitvalue0': (115, 'i'),
    'limittype1': (119, 'B'), 'limitvalue1': (120, 'i'),
    'applytype0': (124, 'B'), 'applyvalue0': (125, 'i'),
    'applytype1': (129, 'B'), 'applyvalue1': (130, 'i'),
    'applytype2': (134, 'B'), 'applyvalue2': (135, 'i'),
    'value0': (139, 'i'), 'value1': (143, 'i'), 'value2': (147, 'i'),
    'value3': (151, 'i'), 'value4': (155, 'i'), 'value5': (159, 'i'),
    'socket0': (163, 'i'), 'socket1': (167, 'i'), 'socket2': (171, 'i'),
    'refined_vnum': (175, 'I'), 'refine_set': (179, 'H'),
    'magic_pct': (181, 'B'), 'specular': (182, 'B'), 'socket_pct': (183, 'B'),
}
# What an edit that does not list its fields changes in the client: what the
# tooltip and the refine window show. Never type/subtype/flags/sockets there.
SAFE_FIELDS = (
    'locale_name', 'stack', 'gold', 'shop_buy_price',
    'limittype0', 'limitvalue0', 'limittype1', 'limitvalue1',
    'applytype0', 'applyvalue0', 'applytype1', 'applyvalue1', 'applytype2', 'applyvalue2',
    'value0', 'value1', 'value2', 'value3', 'value4', 'value5',
    'refined_vnum', 'refine_set', 'magic_pct', 'specular', 'socket_pct',
)
IMMUNE_BITS = ('PARA', 'CURSE', 'STUN', 'SLEEP', 'SLOW', 'POISON', 'TERROR')

# skill_proto columns in the order of the client's gamedata/skilltable.txt
# (PythonSkill.h TABLE_TOKEN_TYPE_*; the *3 and GrandMaster columns of the
# server table are not in it).
SKILL_COLUMNS = (
    'dwVnum', 'szName', 'bType', 'bLevelStep', 'bMaxLevel', 'bLevelLimit',
    'szPointOn', 'szPointPoly', 'szSPCostPoly', 'szDurationPoly',
    'szDurationSPCostPoly', 'szCooldownPoly', 'szMasterBonusPoly',
    'szAttackGradePoly', 'setFlag', 'setAffectFlag', 'szPointOn2',
    'szPointPoly2', 'szDurationPoly2', 'setAffectFlag2',
    'prerequisiteSkillVnum', 'prerequisiteSkillLevel', 'eSkillType', 'iMaxHit',
    'szSplashAroundDamageAdjustPoly', 'dwTargetRange', 'dwSplashRange',
)


def _text(value):
    if value is None:
        return ''
    if isinstance(value, (bytes, bytearray)):
        return bytes(value).decode(TEXT_ENCODING, 'replace')
    if isinstance(value, (set, frozenset)):
        return ','.join(sorted(value))
    return str(value)


def _fixed(value, size):
    b = _text(value).encode(TEXT_ENCODING, 'replace')[:size - 1]
    return b + b'\0' * (size - len(b))


def _immune(value):
    if isinstance(value, int):
        return value
    names = value if isinstance(value, (set, frozenset)) else [p for p in _text(value).split(',') if p]
    bits = 0
    for name in names:
        name = _text(name).strip().upper()
        if name in IMMUNE_BITS:
            bits |= 1 << IMMUNE_BITS.index(name)
    return bits


def _int(value):
    if value is None or value == '':
        return 0
    return int(value)


def set_field(rec, field, value):
    off, fmt = ITEM_FIELDS[field]
    if isinstance(fmt, int):
        rec[off:off + fmt] = _fixed(value, fmt)
        return
    if field == 'immuneflag':
        value = _immune(value)
    else:
        value = _int(value)
    if field.startswith('socket') and value < 0:
        value = 0  # the server's "no socket" -1 is 0 in the client's records
    size = struct.calcsize('<' + fmt)
    if fmt in 'BHI':
        value &= (1 << (8 * size)) - 1
    elif fmt == 'b':
        value = max(-128, min(127, value))
    struct.pack_into('<' + fmt, rec, off, value)


def get_field(rec, field):
    off, fmt = ITEM_FIELDS[field]
    if isinstance(fmt, int):
        return bytes(rec[off:off + fmt]).split(b'\0')[0].decode(TEXT_ENCODING, 'replace')
    return struct.unpack_from('<' + fmt, rec, off)[0]


def read_item_proto(blob):
    """(version, stride, {vnum: bytearray record}) of a MIPX file."""
    if blob[:4] != b'MIPX':
        raise ValueError('item_proto: not MIPX')
    ver, stride, cnt, size = struct.unpack_from('<IIII', blob, 4)
    if stride != RECORD:
        raise ValueError('item_proto: stride %d, expected %d' % (stride, RECORD))
    raw = eterpack.mcoz_decode(blob[20:20 + size], ITEM_KEY)
    if len(raw) != cnt * stride:
        raise ValueError('item_proto: %d bytes for %d records' % (len(raw), cnt))
    recs = {}
    for i in range(cnt):
        r = bytearray(raw[i * stride:(i + 1) * stride])
        recs[struct.unpack_from('<I', r, 0)[0]] = r
    return ver, stride, recs


def write_item_proto(ver, stride, recs):
    raw = b''.join(bytes(recs[v]) for v in sorted(recs))
    blob = eterpack.mcoz_encode(raw, ITEM_KEY)
    return b'MIPX' + struct.pack('<IIII', ver, stride, len(recs), len(blob)) + blob


def patch_item_proto(base_blob, items, db_rows, notes):
    """base_blob patched: items = {vnum: fields (iterable) or None}, db_rows =
    {vnum: row dict}. Returns (blob, changed vnums). notes gets warnings."""
    ver, stride, recs = read_item_proto(base_blob)
    changed = []
    for vnum in sorted(items):
        row = db_rows.get(vnum)
        if row is None:
            notes.append('Przedmiot %d: nie ma go w bazie - klient zostaje bez zmian.' % vnum)
            continue
        if vnum in recs:
            fields = items[vnum]
            fields = [f for f in (fields or SAFE_FIELDS) if f in ITEM_FIELDS]
            rec = bytearray(recs[vnum])
        else:
            fields = [f for f in ITEM_FIELDS]
            rec = bytearray(RECORD)
            struct.pack_into('<I', rec, 0, vnum)
            notes.append('Przedmiot %d: nowy w kliencie (rekord z bazy; ikona z item_list.txt klienta '
                         'albo domyślna).' % vnum)
        for field in fields:
            if field in row:
                set_field(rec, field, row[field])
        if rec != recs.get(vnum):
            recs[vnum] = rec
            changed.append(vnum)
    if not changed:
        return base_blob, []
    return write_item_proto(ver, stride, recs), changed


def _split_lines(blob):
    text = blob.decode(TEXT_ENCODING, 'replace')
    newline = '\r\n' if '\r\n' in text else '\n'
    lines = text.split(newline)
    return lines, newline


def skill_line(row):
    return '\t'.join(_text(row.get(col, '')).replace('\t', ' ').replace('\r', ' ').replace('\n', ' ')
                     for col in SKILL_COLUMNS)


def patch_skilltable(base_blob, skill_rows, notes):
    """gamedata/skilltable.txt with the lines of skill_rows ({vnum: row})
    taken from the database. Returns (blob, changed vnums)."""
    lines, nl = _split_lines(base_blob)
    index = {}
    for i, line in enumerate(lines):
        head = line.split('\t', 1)[0].strip()
        if head.isdigit():
            index.setdefault(int(head), i)
    changed = []
    for vnum in sorted(skill_rows):
        new = skill_line(skill_rows[vnum])
        if vnum in index:
            if lines[index[vnum]] != new:
                lines[index[vnum]] = new
                changed.append(vnum)
        else:
            notes.append('Umiejętność %d: klient jej nie zna (brak w SkillDesc.txt), wiersz dopisany, ale '
                         'klient pokaże ją dopiero z opisem.' % vnum)
            if lines and lines[-1] == '':
                lines.insert(len(lines) - 1, new)
            else:
                lines.append(new)
            changed.append(vnum)
    if not changed:
        return base_blob, []
    return nl.join(lines).encode(TEXT_ENCODING, 'replace'), changed


def patch_tab_text(base_blob, updates, min_columns):
    """A tab-separated client text keyed by the vnum in column 0: updates =
    {vnum: {column index: text}}; a missing line is added (padded to
    min_columns). Returns (blob, changed vnums)."""
    lines, nl = _split_lines(base_blob)
    index = {}
    for i, line in enumerate(lines):
        head = line.split('\t', 1)[0].strip()
        if head.isdigit():
            index.setdefault(int(head), i)
    changed = []
    for vnum in sorted(updates):
        if vnum in index:
            cols = lines[index[vnum]].split('\t')
        else:
            cols = [str(vnum)]
        before = list(cols)
        for col, value in updates[vnum].items():
            while len(cols) <= col:
                cols.append('')
            cols[col] = _text(value).replace('\t', ' ').replace('\r', ' ').replace('\n', ' ')
        while len(cols) < min_columns:
            cols.append('')
        if vnum in index:
            if cols != before:
                lines[index[vnum]] = '\t'.join(cols)
                changed.append(vnum)
        else:
            new = '\t'.join(cols)
            if lines and lines[-1] == '':
                lines.insert(len(lines) - 1, new)
            else:
                lines.append(new)
            changed.append(vnum)
    if not changed:
        return base_blob, []
    return nl.join(lines).encode(TEXT_ENCODING, 'replace'), changed
