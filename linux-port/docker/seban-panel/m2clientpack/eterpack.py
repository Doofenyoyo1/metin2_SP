"""EterPack v2 of the MT2009 client (pack/<name>.index + <name>.data).

The index file is one MCOZ blob (LZO1X + XTEA, the index key) holding
"EPKD", version 2, the entry count and 192-byte entries:

    0   id                 u32
    4   file name          161 bytes, cp1252, NUL-padded (+3 padding)
    168 crc32(name)        u32   - the key the client's file dictionary uses
    172 real (slot) size   i32   - the stored size rounded up to 256
    176 stored size        i32
    180 crc32(stored blob) u32
    184 data position      i32   - offset in <name>.data
    188 compressed type    u8    - 0 raw, 1 MCOZ (LZO), 2 MCOZ + XTEA (data key)
    189 (3 bytes + MD5 slot in the struct; unused for types 0-2)

An entry of type 1/2 is a 16-byte MCOZ header (fourcc, encrypted size,
compressed size, real size), the body and four zero bytes. Keys from the
client exe (EterPack.cpp s_adwEterPackKey / s_adwEterPackSecurityKey), the
same as /opt/metin2/cache/cli/m2pack.py.
"""
import struct
import zlib

from . import lzo1x

MCOZ = 0x5A4F434D
EPKD = b'EPKD'
ENTRY = 192
IDX_KEY = (533489241, 64592187, 413438084, 181131063)
DAT_KEY = (183730646, 760506105, 952721118, 990624796)
DELTA = 0x9E3779B9
M32 = 0xffffffff


def _xtea(buf, key, decrypt):
    k0, k1, k2, k3 = key
    keys = (k0, k1, k2, k3)
    n = len(buf) // 8
    words = struct.unpack('<%dI' % (2 * n), buf[:8 * n])
    out = []
    append = out.append
    if decrypt:
        start = (DELTA * 32) & M32
        for i in range(0, 2 * n, 2):
            y, z = words[i], words[i + 1]
            s = start
            for _ in range(32):
                z = (z - ((((y << 4) ^ (y >> 5)) + y) ^ (s + keys[(s >> 11) & 3]))) & M32
                s = (s - DELTA) & M32
                y = (y - ((((z << 4) ^ (z >> 5)) + z) ^ (s + keys[s & 3]))) & M32
            append(y)
            append(z)
    else:
        for i in range(0, 2 * n, 2):
            y, z = words[i], words[i + 1]
            s = 0
            for _ in range(32):
                y = (y + ((((z << 4) ^ (z >> 5)) + z) ^ (s + keys[s & 3]))) & M32
                s = (s + DELTA) & M32
                z = (z + ((((y << 4) ^ (y >> 5)) + y) ^ (s + keys[(s >> 11) & 3]))) & M32
            append(y)
            append(z)
    return struct.pack('<%dI' % len(out), *out)


def tea_encrypt(buf, key):
    if len(buf) % 8:
        buf = buf + b'\0' * (8 - len(buf) % 8)
    return _xtea(buf, key, False)


def tea_decrypt(buf, key):
    return _xtea(buf, key, True)


def mcoz_decode(buf, key):
    """The raw bytes of an MCOZ blob (key None = not encrypted)."""
    four, enc, comp, real = struct.unpack_from('<4I', buf, 0)
    if four != MCOZ:
        raise ValueError('not an MCOZ blob')
    body = buf[16:]
    if enc:
        body = tea_decrypt(body[:enc], key)
        if struct.unpack_from('<I', body, 0)[0] != MCOZ:
            raise ValueError('MCOZ: wrong key')
        body = body[4:4 + comp]
    else:
        body = body[4:4 + comp]
    return lzo1x.decompress(body, real)


def mcoz_encode(raw, key, tail=False):
    """raw as an MCOZ blob: LZO1X, then XTEA over magic + stream padded the
    way the client's own packer pads it (to 8 above comp + 19). tail adds the
    four zero bytes pack entries and index files end with."""
    comp = lzo1x.compress(raw)
    inner = struct.pack('<I', MCOZ) + comp
    if key is None:
        blob = struct.pack('<4I', MCOZ, 0, len(comp), len(raw)) + inner
    else:
        enc_len = (len(comp) + 19 + 7) // 8 * 8
        inner += b'\0' * (enc_len - len(inner))
        blob = struct.pack('<4I', MCOZ, enc_len, len(comp), len(raw)) + tea_encrypt(inner, key)
    return blob + (b'\0' * 4 if tail else b'')


def name_crc(name):
    return zlib.crc32(name.encode('cp1252')) & M32


class Entry(object):
    __slots__ = ('id', 'name', 'fcrc', 'real', 'size', 'dcrc', 'pos', 'ctype', 'raw')

    def __init__(self, raw):
        self.raw = bytes(raw)
        self.id = struct.unpack_from('<I', raw, 0)[0]
        self.name = raw[4:165].split(b'\0')[0].decode('cp1252')
        self.fcrc, self.real, self.size, self.dcrc, self.pos = struct.unpack_from('<IiiIi', raw, 168)
        self.ctype = raw[188]

    def packed(self):
        rec = bytearray(self.raw)
        struct.pack_into('<I', rec, 0, self.id)
        nb = self.name.encode('cp1252')
        if len(nb) > 160:
            raise ValueError('name too long: %s' % self.name)
        rec[4:165] = nb + b'\0' * (161 - len(nb))
        struct.pack_into('<IiiIi', rec, 168, self.fcrc, self.real, self.size, self.dcrc, self.pos)
        rec[188] = self.ctype
        return bytes(rec)


def read_index_bytes(blob):
    """(version, [Entry]) of an index file's bytes (encrypted or plain EPKD)."""
    if blob[:4] == EPKD:
        raw = blob
    else:
        raw = mcoz_decode(blob, IDX_KEY)
    four, ver, cnt = struct.unpack_from('<4sII', raw, 0)
    if four != EPKD:
        raise ValueError('not an EterPack index')
    if len(raw) < 12 + cnt * ENTRY:
        raise ValueError('index truncated')
    return ver, [Entry(raw[12 + i * ENTRY:12 + (i + 1) * ENTRY]) for i in range(cnt)]


def write_index_bytes(ver, entries):
    raw = EPKD + struct.pack('<II', ver, len(entries)) + b''.join(e.packed() for e in entries)
    return mcoz_encode(raw, IDX_KEY, tail=True)


def encode_entry(raw, ctype):
    if ctype == 0:
        return bytes(raw)
    if ctype == 1:
        return mcoz_encode(raw, None, tail=True)
    if ctype == 2:
        return mcoz_encode(raw, DAT_KEY, tail=True)
    raise ValueError('compressed type %d not supported' % ctype)


def decode_entry(blob, ctype):
    if ctype == 0:
        return bytes(blob)
    if ctype == 1:
        return mcoz_decode(blob, None)
    if ctype == 2:
        return mcoz_decode(blob, DAT_KEY)
    raise ValueError('compressed type %d not supported' % ctype)


def read_entry(data, entry):
    return decode_entry(data[entry.pos:entry.pos + entry.size], entry.ctype)


def write_pack(entries, ver=2):
    """(index bytes, data bytes) of a new pack: entries = [(name, raw bytes,
    compressed type)], ids in order, every blob in a 256-byte slot."""
    data = bytearray()
    out = []
    for i, (name, raw, ctype) in enumerate(entries):
        blob = encode_entry(raw, ctype)
        slot = (len(blob) + 255) // 256 * 256
        rec = bytearray(ENTRY)
        e = Entry(bytes(rec))
        e.id, e.name, e.fcrc, e.real, e.size = i, name, name_crc(name), slot, len(blob)
        e.dcrc, e.pos, e.ctype = zlib.crc32(blob) & M32, len(data), ctype
        out.append(e)
        data += blob + b'\0' * (slot - len(blob))
    return write_index_bytes(ver, out), bytes(data)
