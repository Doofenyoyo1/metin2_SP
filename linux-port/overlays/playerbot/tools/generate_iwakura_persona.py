# -*- coding: utf-8 -*-
"""Renderuje playerbot_persona_tables.h z dokumentu osobowosci Iwakury
(data/iwakura_osobowosci.txt, "SYSTEM OSOBOWOSCI v2.0", 19 wrzesnia).

Z dokumentu biore to, co jest lista przedmiotow, a nie opisem zachowania:

  * "Predefiniowana lista wartosciowych przedmiotow" z Bot Mood System - drop
    czegokolwiek z niej podnosi botowi nastroj o poziom. Ksiegi Umiejetnosci
    i "Opaski Zapomnienia" sa w grze jednym vnumem na wszystkie umiejetnosci
    (50300 i 70037, umiejetnosc w gniezdzie 0), wiec ida jako listy
    umiejetnosci; "Marmur Polimorfi (kazdy rodzaj)" jako caly typ; rodziny
    sprzetu jako vnum +0 (reszta rodziny to +1..+9).
  * Wskazniki poziomowe LPP dla broni (5-29, 30+, 65+) i docelowe zbroje oraz
    tarcze z "3. Zbroje i Tarcze" - to, co bot chomikuje w magazynie.

Tiery przedmiotow, bonusow i KD z tego samego pliku sa identyczne z
data/iwakura_tiery.txt (227 wierszy, sprawdzone przy przyjeciu dokumentu), a
renderuje je generate_iwakura_tiers.py - tu nie sa czytane.

Wejscie: dokument plus zrzut item_proto (ten sam, co dla cennika):
    docker exec -i <db> sh -c 'exec mariadb -uroot -p"$MARIADB_ROOT_PASSWORD" -N' > item_proto_hex.tsv
      SELECT vnum, HEX(locale_name), type, subtype, COALESCE(limitvalue0,0) FROM world.item_proto ORDER BY vnum;
Uzycie (z katalogu linux-port/overlays/playerbot):
    python tools/generate_iwakura_persona.py item_proto_hex.tsv data/iwakura_osobowosci.txt \\
        src/game/src/playerbot_persona_tables.h

Jak generatory cennika i tierow: nazwa, ktorej nie da sie zwiazac z vnumem,
**przerywa render**. Lista, w ktorej po cichu brakuje polowy pozycji, jest
gorsza niz jej brak.
"""
import io
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from generate_iwakura_prices import (GEAR_ALIASES, GOODS_ALIASES, Resolver, SKILL_IDS,
                                     SOUL_STONE_KINDS, VNUM_OVERRIDES, ascii_comment,
                                     goods_key, load_hex, norm)

SKILL_BOOK_VNUM = 50300
FORGET_BOOK_VNUM = 70037
ITEM_POLYMORPH = 19

# Nazwy tylko z tego dokumentu -> nazwa rodziny w item_proto.
PERSONA_GEAR_ALIASES = {
    'zbroje z czarnej stali': 'zbroja z czarnej stali',
    'zbroja płyt. czarnej magii': 'zbr. płyt. czar. magii',
    'ubranie czarnego wiatru': 'ubranie czarn. wiatru',
}

VALUABLE_HEADER = 'Predefiniowana lista wartościowych przedmiotów:'
BOOK_PREFIX = 'księga umiejętności '
FORGET_PREFIX = 'opaska zapomnienia '
POLYMORPH_RE = re.compile(r'^marmur polimorfi', re.I)
SOUL_STONE_RE = re.compile(r'^kamień duszy (\S+) \+(\d)$')
WEAPON_BAND_RE = re.compile(r'^-\s*(5-29 lvl|30 lvl\+|65 lvl\+):\s*(.+)$')
ARMOUR_TARGETS_RE = re.compile(r'\(Tarcze z odpornością:\s*([^)]*)\)\s*oraz\s*70 lvl\s*\(([^)]*)\)')
BANDS = {'5-29 lvl': 1, '30 lvl+': 2, '65 lvl+': 3}


def read_document(path):
    text = io.open(path, encoding='utf-8-sig').read().replace('\r\n', '\n')
    return text.split('\n')


def split_names(text):
    # "(ZMIANA ZACHOWANIA - ...)" i "(PVE 5)" to uwagi, nie nazwy. Kropka na
    # koncu zostaje: raz konczy zdanie, raz skrot ("Kozik Czar. Lis."), i
    # dopiero item_proto mowi, ktore z nich.
    text = re.sub(r'\((?:ZMIANA ZACHOWANIA[^)]*|PVE \d[^)]*)\)', '', text)
    names = []
    for part in text.split(','):
        name = part.strip()
        while name.endswith(' .') or name.endswith('..'):
            name = name[:-1].strip()
        if name:
            names.append(name)
    return names


def name_variants(name):
    """Nazwa tak jak stoi, potem bez kropki konczacej zdanie."""
    first = name.strip()
    out = [first]
    stripped = first.rstrip('.').strip()
    if stripped != first:
        out.append(stripped)
    return out


def find_gear(resolver, name):
    for variant in name_variants(name):
        key = norm(variant)
        key = PERSONA_GEAR_ALIASES.get(key, GEAR_ALIASES.get(key, key))
        if key + '+0' in resolver.items:
            return resolver.items[key + '+0']
    return None


def find_goods(resolver, name):
    # Bez SKIPPED_ROWS cennika: tam "Waleczna Dusza Zaprzys" jest pomijana,
    # bo inny wiersz ja wycenia, a tu to jedyna jej wzmianka.
    for variant in name_variants(name):
        key = goods_key(variant)
        key = GOODS_ALIASES.get(key, key)
        if key in VNUM_OVERRIDES:
            return VNUM_OVERRIDES[key]
        vnums = sorted(v for v, t in resolver.goods.get(key, []) if t not in (1, 2))
        if vnums:
            return vnums
    return []


def resolve_gear(resolver, items_by_vnum, name):
    base = find_gear(resolver, name)
    if base is None:
        resolver.missing.append('rodzina sprzetu: %s' % name)
    return base


def parse_valuables(lines, resolver, errors):
    for i, line in enumerate(lines):
        if line.strip().startswith(VALUABLE_HEADER):
            body = lines[i + 1].strip()
            break
    else:
        raise SystemExit('brak listy "%s"' % VALUABLE_HEADER)

    vnums = {}        # vnum -> nazwa z dokumentu
    families = {}     # vnum +0 -> nazwa
    books = {}        # umiejetnosc -> nazwa
    forgets = {}
    polymorph = False
    for raw in body.split(','):
        name = raw.strip()
        if not name:
            continue
        key = norm(name).rstrip('.').strip()
        if key.startswith(BOOK_PREFIX):
            skill = SKILL_IDS.get(key[len(BOOK_PREFIX):])
            if skill is None:
                errors.append('umiejetnosc ksiegi: %s' % name)
            else:
                books[skill] = name
            continue
        if key.startswith(FORGET_PREFIX):
            skill = SKILL_IDS.get(key[len(FORGET_PREFIX):])
            if skill is None:
                errors.append('umiejetnosc opaski: %s' % name)
            else:
                forgets[skill] = name
            continue
        if POLYMORPH_RE.match(key):
            polymorph = True
            continue
        stone = SOUL_STONE_RE.match(key)
        if stone:
            kind = SOUL_STONE_KINDS.get(stone.group(1))
            if kind is None:
                errors.append('kamien duszy: %s' % name)
            else:
                vnums[28000 + int(stone.group(2)) * 100 + kind] = name
            continue
        # Rodzina sprzetu ma w grze "+0" na koncu; najpierw ona, potem towar.
        base = find_gear(resolver, name)
        if base is not None:
            families[base] = name
            continue
        found = find_goods(resolver, name)
        if not found:
            errors.append('przedmiot: %s' % name)
        for v in found:
            vnums[v] = name
    return vnums, families, books, forgets, polymorph


def parse_lpp(lines, resolver, items_by_vnum, errors):
    weapons = []
    for line in lines:
        m = WEAPON_BAND_RE.match(line.strip())
        if not m:
            continue
        band = BANDS[m.group(1)]
        for name in split_names(m.group(2)):
            base = resolve_gear(resolver, items_by_vnum, name)
            if base is None:
                continue
            level = items_by_vnum[base][1]
            # "ograniczenie do 1 broni per magazyn dla broni na 15 i 20 lvl"
            weapons.append((base, band, level, 1 if band == 1 and level in (15, 20) else 0, name))
    if not weapons:
        errors.append('brak wskaznikow poziomowych broni (5-29 / 30+ / 65+)')
    text = ' '.join(l.strip() for l in lines)
    m = ARMOUR_TARGETS_RE.search(text)
    shields, armours = [], []
    if not m:
        errors.append('brak docelowych tarcz 61 lvl i zbroi 70 lvl w "3. Zbroje i Tarcze"')
    else:
        for name in split_names(m.group(1)):
            base = resolve_gear(resolver, items_by_vnum, name)
            if base is not None:
                shields.append((base, name))
        for name in split_names(m.group(2)):
            base = resolve_gear(resolver, items_by_vnum, name)
            if base is not None:
                armours.append((base, name))
    return weapons, shields, armours


def render(out_path, vnums, families, books, forgets, polymorph, weapons, shields, armours):
    o = []
    o.append('// Rendered by linux-port/overlays/playerbot/tools/generate_iwakura_persona.py from')
    o.append('// Iwakura\'s personality document (data/iwakura_osobowosci.txt, "SYSTEM OSOBOWOSCI')
    o.append('// v2.0", 19 September) and world.item_proto. Do not edit by hand.')
    o.append('//')
    o.append('// Two lists of his, as data: what lifts a bot\'s mood when it drops (the Bot')
    o.append('// Mood System\'s "predefiniowana lista wartosciowych przedmiotow"), and the')
    o.append('// weapons, armours and shields his LPP keeps in the storekeeper\'s box by the')
    o.append('// band of the bot\'s level. The skill books and the Forgetting books are one')
    o.append('// vnum each with the skill in socket 0, so they are lists of skills; a family')
    o.append('// of gear is its +0 vnum and the family is +0..+9.')
    o.append('#ifndef __INC_METIN2_PLAYERBOT_PERSONA_TABLES_H__')
    o.append('#define __INC_METIN2_PLAYERBOT_PERSONA_TABLES_H__')
    o.append('')
    o.append('namespace')
    o.append('{')
    o.append('\tconst DWORD PLAYERBOT_MOOD_SKILL_BOOK_VNUM = %d;' % SKILL_BOOK_VNUM)
    o.append('\tconst DWORD PLAYERBOT_MOOD_FORGET_BOOK_VNUM = %d;' % FORGET_BOOK_VNUM)
    o.append('\t// "Marmur Polimorfi (kazdy rodzaj)": every ITEM_POLYMORPH counts.')
    o.append('\tconst bool PLAYERBOT_MOOD_ANY_POLYMORPH = %s;' % ('true' if polymorph else 'false'))
    o.append('')
    o.append('\t// Single items, sorted: materials, pearls, scrolls, stones, medals.')
    o.append('\tconst DWORD PLAYERBOT_MOOD_VALUABLE_VNUMS[] = {')
    for v in sorted(vnums):
        o.append('\t\t%d, // %s' % (v, ascii_comment(vnums[v])))
    o.append('\t};')
    o.append('')
    o.append('\t// Families of gear, by the +0 vnum.')
    o.append('\tconst DWORD PLAYERBOT_MOOD_VALUABLE_FAMILIES[] = {')
    for v in sorted(families):
        o.append('\t\t%d, // %s' % (v, ascii_comment(families[v])))
    o.append('\t};')
    o.append('')
    o.append('\t// Skill books (%d) by the skill in socket 0.' % SKILL_BOOK_VNUM)
    o.append('\tconst BYTE PLAYERBOT_MOOD_VALUABLE_BOOK_SKILLS[] = {')
    for s in sorted(books):
        o.append('\t\t%d, // %s' % (s, ascii_comment(books[s])))
    o.append('\t};')
    o.append('')
    o.append('\t// Forgetting books (%d) - his "Opaska Zapomnienia" - by the skill in socket 0.' % FORGET_BOOK_VNUM)
    o.append('\tconst BYTE PLAYERBOT_MOOD_VALUABLE_FORGET_SKILLS[] = {')
    for s in sorted(forgets):
        o.append('\t\t%d, // %s' % (s, ascii_comment(forgets[s])))
    o.append('\t};')
    o.append('')
    o.append('\t// LPP weapons. bBand: 1 = his "5-29 lvl", 2 = "30 lvl+", 3 = "65 lvl+";')
    o.append('\t// bLevel is the family\'s level limit in this world; bOnlyOne marks the')
    o.append('\t// weapons of level 15 and 20, of which a box keeps one.')
    o.append('\tstruct TPlayerBotLppWeapon { DWORD dwBaseVnum; BYTE bBand; BYTE bLevel; BYTE bOnlyOne; };')
    o.append('\tconst TPlayerBotLppWeapon PLAYERBOT_LPP_WEAPONS[] = {')
    for base, band, level, only_one, name in weapons:
        o.append('\t\t{ %d, %d, %d, %d }, // %s' % (base, band, level, only_one, ascii_comment(name)))
    o.append('\t};')
    o.append('')
    o.append('\t// The shields of level 61 with resistances, and the armours his "70 lvl" names')
    o.append('\t// (level 66 in this world): what a box keeps an armour or a shield for.')
    o.append('\tconst DWORD PLAYERBOT_LPP_TARGET_SHIELDS[] = {')
    for base, name in shields:
        o.append('\t\t%d, // %s' % (base, ascii_comment(name)))
    o.append('\t};')
    o.append('\tconst DWORD PLAYERBOT_LPP_TARGET_ARMOURS[] = {')
    for base, name in armours:
        o.append('\t\t%d, // %s' % (base, ascii_comment(name)))
    o.append('\t};')
    o.append('}')
    o.append('')
    o.append('#endif')
    o.append('')
    with io.open(out_path, 'w', encoding='ascii', newline='\n') as f:
        f.write('\n'.join(o))


def main(item_path, doc_path, out_path):
    items = load_hex(item_path)
    items_by_vnum = {}
    for vnum, name, rest in items:
        level = int(rest[2]) if len(rest) > 2 and rest[2].lstrip('-').isdigit() else 0
        items_by_vnum[vnum] = (name, level)
    resolver = Resolver(items, [])
    lines = read_document(doc_path)
    errors = []
    vnums, families, books, forgets, polymorph = parse_valuables(lines, resolver, errors)
    weapons, shields, armours = parse_lpp(lines, resolver, items_by_vnum, errors)
    problems = resolver.missing + errors
    if problems:
        sys.stderr.write('generate_iwakura_persona: %d problemow:\n' % len(problems))
        for p in problems:
            sys.stderr.write('  %s\n' % p)
        return 1
    render(out_path, vnums, families, books, forgets, polymorph, weapons, shields, armours)
    print('wartosciowe: %d vnumow, %d rodzin, %d ksiag, %d opasek, polimorfia=%s; '
          'LPP: %d broni, %d tarcz, %d zbroi -> %s' % (
              len(vnums), len(families), len(books), len(forgets), polymorph,
              len(weapons), len(shields), len(armours), out_path))
    return 0


if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.stderr.write(__doc__)
        sys.exit(2)
    sys.exit(main(*sys.argv[1:]))
