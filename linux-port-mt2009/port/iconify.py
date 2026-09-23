# -*- coding: utf-8 -*-
"""The classic panel's item icons, from the mt2009 client.

Usage:  python iconify.py --icon-pack <dir icon pack was extracted to> \
                          --item-list <item_list.txt from the item pack> \
                          --vnums <file: one "vnum type" per line, from player.item_proto>

    python tools/eterpack.py --profile mt2009 extract <Klient>/pack/icon <dir>
    python tools/eterpack.py --profile mt2009 extract <Klient>/pack/item <dir2>
    (item_list.txt is <dir2>/d:/ymir work/item/949item_list.txt)

Writes files/static/icons/<NNNNN>.png for every icon/item/<NNNNN>.tga the
world's item_proto refers to, and files/static/item_icons.json (vnum ->
file). The panel asks item_icons.json first and falls back to the 5-digit
vnum, so the json only has to carry the vnums whose icon is not their own
number: a refine level shares its +0's icon, and item_list.txt names the
rest. Needs Pillow (the TGA reader); run it inside the m2-eterpack image
with `pip install pillow` if the host has none.

The r40250 line generated these on the operator's machine and never shipped
them; on 2.x nothing generated them anywhere, so every player's panel showed
a broken image for every item (11 September). Seven megabytes in the update
is the price of icons everybody has.
"""
import argparse
import io
import json
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(HERE, '..', '..', 'files', 'static', 'icons'))
OUT_JSON = os.path.normpath(os.path.join(HERE, '..', '..', 'files', 'static', 'item_icons.json'))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--icon-pack', required=True)
    parser.add_argument('--item-list', required=True)
    parser.add_argument('--vnums', required=True)
    parser.add_argument('--fallback', default='',
                        help='a directory with an older icons/ set and item_icons.json '
                             '(the r40250 panel staging) for vnums the client pack lacks')
    args = parser.parse_args()
    from PIL import Image

    sub = {}
    for line in io.open(args.item_list, encoding='latin1'):
        p = line.strip().split('\t')
        if len(p) >= 3 and p[0].isdigit():
            m = re.search(r'([0-9]+)\.sub$', p[2].replace('\\', '/'), re.I)
            if m:
                sub[int(p[0])] = m.group(1)
    icon_dir = os.path.join(args.icon_pack, 'icon', 'item')
    icons = set(f[:-4] for f in os.listdir(icon_dir) if f.lower().endswith('.tga'))
    vnums = [int(l.split()[0]) for l in io.open(args.vnums, encoding='utf-8') if l.strip()]

    def pick(v):
        for cand in ('%05d' % v, sub.get(v, ''), '%05d' % (v - v % 10)):
            if cand and cand in icons:
                return cand
        return None

    # The older set, for what this client's icon pack does not carry as a
    # per-item TGA (its item_list names .sub cut-outs of atlases for those:
    # the starter potions, the skill book, the starter chests). Such a file
    # is copied under a name that cannot clash with a client icon.
    fb_map, fb_dir = {}, ''
    if args.fallback:
        fb_dir = os.path.join(args.fallback, 'icons')
        fb_json = os.path.join(args.fallback, 'item_icons.json')
        if os.path.isfile(fb_json):
            fb_map = json.loads(io.open(fb_json, encoding='utf-8').read())

    os.makedirs(OUT_DIR, exist_ok=True)
    mapping = {}
    used = set()
    fallback_used = {}
    missing = 0
    for v in vnums:
        c = pick(v)
        if c is not None:
            used.add(c)
            if c != '%05d' % v:
                mapping[str(v)] = c + '.png'
            continue
        old = fb_map.get(str(v)) or ('%05d.png' % v)
        if fb_dir and os.path.isfile(os.path.join(fb_dir, old)):
            name = 'r40250-' + old
            fallback_used[name] = os.path.join(fb_dir, old)
            mapping[str(v)] = name
            continue
        missing += 1
    written = 0
    for c in sorted(used):
        src = os.path.join(icon_dir, c + '.tga')
        dst = os.path.join(OUT_DIR, c + '.png')
        img = Image.open(src)
        img = img.convert('RGBA')
        img.save(dst, optimize=True)
        written += 1
    for name, src in sorted(fallback_used.items()):
        with open(src, 'rb') as f, open(os.path.join(OUT_DIR, name), 'wb') as g:
            g.write(f.read())
        written += 1
    io.open(OUT_JSON, 'w', encoding='utf-8', newline='\n').write(
        json.dumps(mapping, indent=0, sort_keys=True, separators=(',', ':')) + '\n')
    print('iconify: %d vnums, %d png written (%d from the fallback set), %d mapped by json, %d without an icon'
          % (len(vnums), written, len(fallback_used), len(mapping), missing))


if __name__ == '__main__':
    main()
