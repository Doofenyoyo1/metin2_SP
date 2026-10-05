"""Writes base/<client version>/ from a released client's pack folder - run
once per client release from 2.0.52 on (the client with the "dbdata" pack):

    python3 -m m2clientpack.make_base <client>/pack 2.0.53

base.json  the client version, the release's dbdata.index/.data (size,
           SHA-256) and per file of the pack its size, hash and type.
dbdata.index, dbdata.data: the release's pack as it is (the "original
files" download); files/<name>: its contents, which the edits patch.
"""
import json
import os
import sys

from . import dbdata, eterpack


def make_base(pack_dir, version, root=dbdata.BASE_ROOT):
    out = os.path.join(root, version)
    os.makedirs(os.path.join(out, 'files'), exist_ok=True)
    blobs = {}
    for ext in ('index', 'data'):
        with open(os.path.join(pack_dir, '%s.%s' % (dbdata.PACK, ext)), 'rb') as f:
            blobs[ext] = f.read()
    _ver, entries = eterpack.read_index_bytes(blobs['index'])
    meta = {'client': version, 'pack': {}, 'files': {}}
    for ext in ('index', 'data'):
        meta['pack'][ext + 'Sha256'] = dbdata.sha256(blobs[ext])
        meta['pack'][ext + 'Size'] = len(blobs[ext])
        with open(os.path.join(out, '%s.%s' % (dbdata.PACK, ext)), 'wb') as f:
            f.write(blobs[ext])
    for e in entries:
        data = eterpack.read_entry(blobs['data'], e)
        path = os.path.join(out, 'files', *e.name.split('/'))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'wb') as g:
            g.write(data)
        meta['files'][e.name] = {'size': len(data), 'sha256': dbdata.sha256(data), 'ctype': e.ctype}
    with open(os.path.join(out, 'base.json'), 'w', encoding='utf-8') as f:
        json.dump(meta, f, indent=1, sort_keys=True)
    return out


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    print(make_base(sys.argv[1], sys.argv[2]))
