#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EterPack (.eix/.epk) reader and writer for the r40250 client.

The client keeps its scripts in pack/root.epk and reads them through an index,
root.eix, that is one LZO object encrypted with the index key; every file in
the .epk is its own LZO object, encrypted with the data key when its type says
so. Nothing on a player's machine can add a file to such an archive except a
GUI packer, which is why the client-side half of a feature (the F9 GM panel,
say) used to be "repack root.epk by hand". This does it from a script:

    python tools/eterpack.py list   <pack/root>
    python tools/eterpack.py extract <pack/root> <outdir>
    python tools/eterpack.py repack  <pack/root> <outpack/root> <dir-with-replacements>

`repack` rewrites the archive with every file of the original, taking a file
from <dir> instead when one of the same name is there (a new name is appended).
Types are kept per file; a new file gets the type of the majority. Needs
python-lzo (apt liblzo2-dev + pip python-lzo; the docker python image does).

Keys are the stock r40250 ones (EterPack.cpp) unless `--profile mt2009` is
given before the command: that is the martysama0134 client, whose PackMakerLite
keys sit in pack/PackMakerLite.json and whose archives are <name>.index and
<name>.data. The format is the same to the byte; only the keys and the
extensions differ. A client with other keys fails the index check loudly - the
fourcc of the decrypted index is tested - rather than writing garbage.
"""
import os
import struct
import sys
import zlib

import lzo

PROFILES = {
    # EterPack.cpp of the r40250 client
    'r40250': {'index_key': (45129401, 92367215, 681285731, 1710201),
               'data_key': (78952482, 527348324, 1632463, 486575),
               'ext_index': '.eix', 'ext_data': '.epk', 'layout': 'stock'},
    # pack/PackMakerLite.json of the mt2009 client (eter_pack_index_key /
    # eter_pack_data_key; its EterPack.cpp carries the same numbers as
    # s_adwEterPackKey / s_adwEterPackSecurityKey)
    'mt2009': {'index_key': (533489241, 64592187, 413438084, 181131063),
               'data_key': (183730646, 760506105, 952721118, 990624796),
               'ext_index': '.index', 'ext_data': '.data',
               # PackMakerLite's layout, and a loader (ENABLE_CRC32_CHECK) that
               # refuses a type-2 file whose data_crc is not the CRC32 of its
               # on-disk bytes; see write_pack.
               'layout': 'packmakerlite'},
}
INDEX_KEY = PROFILES['r40250']['index_key']
DATA_KEY = PROFILES['r40250']['data_key']
EXT_INDEX = PROFILES['r40250']['ext_index']
EXT_DATA = PROFILES['r40250']['ext_data']
LAYOUT = PROFILES['r40250']['layout']
DELTA = 0x9E3779B9
MASK = 0xFFFFFFFF
ENTRY = 192
FOURCC_LZO = b'MCOZ'
FOURCC_INDEX = b'EPKD'


def xtea_decrypt_block(v0, v1, k):
    s = (DELTA * 32) & MASK
    for _ in range(32):
        v1 = (v1 - ((((v0 << 4) ^ (v0 >> 5)) + v0) ^ (s + k[(s >> 11) & 3]))) & MASK
        s = (s - DELTA) & MASK
        v0 = (v0 - ((((v1 << 4) ^ (v1 >> 5)) + v1) ^ (s + k[s & 3]))) & MASK
    return v0, v1


def xtea_encrypt_block(v0, v1, k):
    s = 0
    for _ in range(32):
        v0 = (v0 + ((((v1 << 4) ^ (v1 >> 5)) + v1) ^ (s + k[s & 3]))) & MASK
        s = (s + DELTA) & MASK
        v1 = (v1 + ((((v0 << 4) ^ (v0 >> 5)) + v0) ^ (s + k[(s >> 11) & 3]))) & MASK
    return v0, v1


def xtea(data, key, encrypt):
    out = bytearray(len(data))
    f = xtea_encrypt_block if encrypt else xtea_decrypt_block
    for i in range(0, len(data) - 7, 8):
        v0, v1 = struct.unpack_from('<II', data, i)
        a, b = f(v0, v1, key)
        struct.pack_into('<II', out, i, a, b)
    tail = len(data) % 8
    if tail:
        out[-tail:] = data[-tail:]
    return bytes(out)


def lzo_unpack(blob, key):
    """One CLZObject: fourcc, encrypted size, compressed size, real size, data."""
    fourcc, enc, comp, real = struct.unpack_from('<4sIII', blob, 0)
    if fourcc != FOURCC_LZO:
        raise ValueError('not an LZO object: %r' % fourcc)
    # The encrypted region is the fourcc again and then the LZO stream, padded
    # to the cipher's eight bytes; an unencrypted object carries the stream
    # straight after the header.
    if key:
        body = xtea(blob[16:16 + enc], key, False)
        if body[:4] != FOURCC_LZO:
            raise ValueError('wrong key: decrypted fourcc %r' % body[:4])
        return lzo.decompress(body[4:4 + comp], False, real)
    if blob[16:20] != FOURCC_LZO:
        raise ValueError('unencrypted object without its inner fourcc')
    return lzo.decompress(blob[20:20 + comp], False, real)


def lzo_pack(data, key):
    comp = lzo.compress(data, 9, False)
    enc = 0
    body = FOURCC_LZO + comp
    if key:
        # The mt2009 client's CLZObject::Encrypt ciphers dwCompressedSize + 19
        # bytes rounded up to the cipher block - fourcc, stream and whatever
        # follows in its buffer - and writes that as dwEncryptSize; the stock
        # client ciphers the fourcc and the stream alone. Either loader only
        # reads what the header says, so this is for byte-faithful output.
        target = len(body) + 15 if LAYOUT == 'packmakerlite' else len(body)
        body += bytes((-target) % 8 + (target - len(body)))
        enc = len(body)
        body = xtea(body, key, True)
    return struct.pack('<4sIII', FOURCC_LZO, enc, len(comp), len(data)) + body


def read_index(path_eix):
    raw = open(path_eix, 'rb').read()
    index = lzo_unpack(raw, INDEX_KEY)
    fourcc, version, count = struct.unpack_from('<4sII', index, 0)
    if fourcc != FOURCC_INDEX:
        raise ValueError('index fourcc %r: not a stock-key archive' % fourcc)
    entries = []
    for i in range(count):
        off = 12 + i * ENTRY
        (idx, name, crc, disk, real, size, pos, typ) = struct.unpack_from('<I161s3xIIIIIB3x', index, off)
        entries.append({'id': idx, 'name': name.split(b'\0', 1)[0].decode('latin-1'), 'crc': crc,
                        'disk': disk, 'real': real, 'size': size, 'pos': pos, 'type': typ})
    return version, entries


# COMPRESSED_TYPE_COMPRESS (1) is an LZO object, plain in this client and under
# a pack key in others; the first file decides and the writer uses the same.
TYPE1_KEY = [None]


def read_file(epk, e):
    blob = epk[e['pos']:e['pos'] + e['disk']]
    if e['type'] == 0:
        return blob
    if e['type'] == 1:
        if TYPE1_KEY[0] is None:
            for key in (None, INDEX_KEY, DATA_KEY):
                try:
                    data = lzo_unpack(blob, key)
                    TYPE1_KEY[0] = key
                    return data
                except Exception:
                    continue
            raise ValueError('file %s: no stock key opens a type-1 object' % e['name'])
        return lzo_unpack(blob, TYPE1_KEY[0])
    if e['type'] == 2:
        return lzo_unpack(blob, DATA_KEY)
    raise ValueError('file %s has type %d (not lzo/raw); refusing' % (e['name'], e['type']))


def write_pack(path_base, version, files):
    """files: list of (name, data, type). Writes <base>.eix and <base>.epk."""
    epk = bytearray()
    entries = []
    for i, (name, data, typ) in enumerate(files):
        if typ == 0:
            blob = data
        elif typ == 1:
            blob = lzo_pack(data, TYPE1_KEY[0])
        elif typ == 2:
            blob = lzo_pack(data, DATA_KEY)
        else:
            raise ValueError('type %d' % typ)
        # The client maps data_size bytes at data_position, and the stock
        # packer lays every file at a 128-byte boundary with data_size the
        # on-disk size rounded up to 128 and the archive padded to match; a
        # data_size larger than what is left in the file - the real size of
        # a compressed last file, say - maps past the end and the client dies
        # without a word at login. (Only the fourth DWORD, `size`, is
        # uninitialised garbage in the stock index; it gets the real size.)
        nm = name.encode('latin-1')
        pos = len(epk)
        if LAYOUT == 'packmakerlite':
            # PackMakerLite (the mt2009 client): the object on disk is the LZO
            # header, the ciphered bytes and four zero bytes (CLZObject::GetSize
            # counts an extra DWORD); data_size is exactly that, the slot it sits
            # in is data_size rounded up to 256 (kept in real_data_size, which
            # this loader never reads for a type-2 file - the LZO header carries
            # the real size), and data_crc is the CRC32 of the data_size bytes.
            # With ENABLE_CRC32_CHECK the loader refuses every type-2 file whose
            # data_crc does not match: the stock layout below in that slot put
            # the real size there, so the client opened not one script of root
            # and died at "RunMain Error".
            if typ == 2:
                blob = blob + bytes(4)
            slot = (len(blob) + 255) // 256 * 256
            epk += blob
            epk += bytes(slot - len(blob))
            entries.append(struct.pack('<I161s3xIIIIIB3x', i, nm, zlib.crc32(nm.lower()) & MASK,
                                       slot, len(blob), zlib.crc32(blob) & MASK, pos, typ))
            continue
        epk += blob
        padded = (len(blob) + 127) // 128 * 128
        epk += bytes(padded - len(blob))
        entries.append(struct.pack('<I161s3xIIIIIB3x', i, nm, zlib.crc32(nm.lower()) & MASK,
                                   len(blob), padded, len(data), pos, typ))
    index = struct.pack('<4sII', FOURCC_INDEX, version, len(files)) + b''.join(entries)
    open(path_base + EXT_DATA, 'wb').write(bytes(epk))
    open(path_base + EXT_INDEX, 'wb').write(lzo_pack(index, INDEX_KEY))


def use_profile(name):
    global INDEX_KEY, DATA_KEY, EXT_INDEX, EXT_DATA, LAYOUT
    p = PROFILES[name]
    INDEX_KEY, DATA_KEY = p['index_key'], p['data_key']
    EXT_INDEX, EXT_DATA = p['ext_index'], p['ext_data']
    LAYOUT = p['layout']


def main():
    if len(sys.argv) > 2 and sys.argv[1] == '--profile':
        use_profile(sys.argv[2])
        del sys.argv[1:3]
    cmd = sys.argv[1]
    base = sys.argv[2]
    version, entries = read_index(base + EXT_INDEX)
    if cmd == 'list':
        print('version', version, 'files', len(entries))
        for e in entries:
            print('%3d type=%d size=%7d disk=%7d %s' % (e['id'], e['type'], e['real'], e['disk'], e['name']))
        return
    epk = open(base + EXT_DATA, 'rb').read()
    if cmd == 'extract':
        out = sys.argv[3]
        os.makedirs(out, exist_ok=True)
        for e in entries:
            data = read_file(epk, e)
            p = os.path.join(out, e['name'].replace('\\', '/'))
            os.makedirs(os.path.dirname(p) or out, exist_ok=True)
            open(p, 'wb').write(data)
        print('extracted', len(entries), 'files to', out)
        return
    if cmd == 'repack':
        out_base, repl = sys.argv[3], sys.argv[4]
        # A file in the replacement directory that the archive does not already
        # hold is almost always a mistake - a note, a backup, an editor's stray
        # copy - and the client cannot read it anyway: nothing imports a module
        # that was not in root.epk before. It used to be added silently, which
        # is how a README.md written beside the four panel sources ended up
        # inside the pack. --allow-new says "yes, really add it".
        allow_new = '--allow-new' in sys.argv[5:]
        replacements = {}
        for root, _, names in os.walk(repl):
            for n in names:
                rel = os.path.relpath(os.path.join(root, n), repl).replace('\\', '/')
                replacements[rel.lower()] = open(os.path.join(root, n), 'rb').read()
        types = [e['type'] for e in entries]
        default_type = max(set(types), key=types.count)
        files = []
        seen = set()
        for e in entries:
            key = e['name'].replace('\\', '/').lower()
            data = replacements.pop(key, None)
            if data is None:
                data = read_file(epk, e)
            else:
                seen.add(key)
                print('replaced', e['name'], len(data))
            files.append((e['name'], data, e['type'] if e['type'] in (0, 1, 2) else default_type))
        if replacements and not allow_new:
            raise SystemExit(
                    'refusing to add files the archive does not have: ' +
                    ', '.join(sorted(replacements)) +
                    '\n(pass --allow-new if that is really what you want)')
        for key, data in sorted(replacements.items()):
            print('added', key, len(data))
            files.append((key, data, default_type))
        write_pack(out_base, version, files)
        print('wrote', out_base + EXT_INDEX + '/' + EXT_DATA, 'files', len(files))
        return
    raise SystemExit('usage: [--profile r40250|mt2009] list|extract|repack')


if __name__ == '__main__':
    main()
