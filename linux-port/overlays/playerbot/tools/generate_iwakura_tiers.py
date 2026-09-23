# -*- coding: utf-8 -*-
"""Renderuje playerbot_item_tiers.h z listy tierow Iwakury (data/iwakura_tiery.txt).

Jego plik z 16 wrzesnia ("TIERY ITEMOW + BONUSOW"): kazda rodzina bizuterii,
butow i broni oraz kazdy bonus dostaje dwie oceny od 1 (bardzo zly) do 6
(wspanialy) - osobno do PvP i do PvE - a niektore wiersze dopisek "+1 dla
Wojownika" albo "+1 PVE dla Szamana/Sury". Zbroje i tarcze ocenia po poziomie
i po bonusach, nie po nazwie, wiec te dwie sekcje sa opisem, nie tabela.

Wersja z 19 wrzesnia ("TIERY ITEMOW, BONUSOW, KD") doklada sekcje [TIERY KD]:
calkowity zakaz Kamieni Duszy +0, +1 i +2 w broniach i zbrojach, lista
jedynych dozwolonych (+3 i +4, kazdy z ocena PvP/PvE) i cztery kamienie
klasowe +4, ktore ida WYLACZNIE do broni PvP. Zakaz stopni jest czytany z jego
zdania, a nie wpisany na sztywno: gdyby kiedys zezwolil na +2, wystarczy
podmienic plik. Kazdy kamien z listy musi sie zwiazac z vnumem z item_proto
(28000 + stopien*100 + rodzaj) o tej samej nazwie i typie ITEM_METIN.

Wejscie: jego plik plus zrzut item_proto (ten sam, co dla cennika):
    docker exec -i <db> sh -c 'exec mariadb -uroot -p"$MARIADB_ROOT_PASSWORD" -N' > item_proto_hex.tsv
      SELECT vnum, HEX(locale_name), type, subtype, COALESCE(limitvalue0,0) FROM world.item_proto ORDER BY vnum;
Uzycie (z katalogu linux-port/overlays/playerbot):
    python tools/generate_iwakura_tiers.py item_proto_hex.tsv data/iwakura_tiery.txt \\
        src/game/src/playerbot_item_tiers.h

Nazwy rodzin i bonusow rozwiazuje tak samo jak generator cennika (aliasy z
generate_iwakura_prices.py plus kilka pisowni tylko z tego pliku) i tak samo
**przerywa z bledem**, gdy jakiejs nie zwiaze: tabela, w ktorej po cichu brakuje
polowy wierszy, jest gorsza niz jej brak.
"""
import io
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from generate_iwakura_prices import (BONUS_APPLIES, GEAR_ALIASES, MT2009_ONLY, Resolver,
                                     ascii_comment, load_hex, norm)

# Pisownie tylko z listy tierow -> klucz z BONUS_APPLIES.
TIER_BONUS_ALIASES = {
    'x% obrażeń dodanych do pż': 'x% obrażen dodanych do pż',
    'x% obrażeń dodanych do pe': 'x% obrażen dodanych do pe',
    'odporność na magię': 'odporność na magie',
    'szansa na uniknięcie strzały': 'szansa na unik. strzały',
    'silny przeciwko diabłom': 'silny przeciwko diablom',
    'silny na nieumarłe': 'silny przeciwko nieumarłym',
    'silny na orki': 'silny przeciwko orkom',
    'silny na zwierzęta': 'silny przeciwko zwierzętom',
}

# Nazwy rodzin tylko z listy tierow -> nazwa z item_proto. Wersja z 19 wrzesnia
# pisze "Miecz Zadlowy"; gra zna jeden miecz tej rodziny, "Miecz Zadlo" (170).
TIER_GEAR_ALIASES = {
    'miecz żądłowy': 'miecz żądło',
}

# [TIERY KD]: slowo z jego nazwy kamienia -> rodzaj (ostatnie dwie cyfry vnumu)
# i to, co w nazwie z item_proto stoi zamiast tego slowa (gra skraca dwie z
# nich, a "Kamien Witalnosci" nie ma w nazwie "Duszy").
SOUL_STONE_KINDS = {
    'penetracji': (30, 'penetr.'),
    'śmierci': (31, 'śmierci'),
    'powtórki': (32, 'powtórki'),
    'wojownika': (33, 'wojownika'),
    'ninja': (34, 'ninja'),
    'sury': (35, 'sury'),
    'szamana': (36, 'szamana'),
    'potwora': (37, 'potwora'),
    'uchylenia': (38, 'uchylenia'),
    'uniku': (39, 'uniku'),
    'magii': (40, 'magii'),
    'witalności': (41, 'witalności'),
    'obrony': (42, 'obrony'),
    'przyspieszenia': (43, 'przysp.'),
}
SOUL_STONE_ROW_RE = re.compile(r'^kamień duszy (\S+) \+(\d)$')
SOUL_STONE_BAN_RE = re.compile(r'zakaz umieszczania kamieni duszy (.*?) w broniach', re.I)
SOUL_STONE_PVP_ONLY_RE = re.compile(r'wyłącznie w broniach pvp', re.I)
ITEM_METIN = 10

JOBS = {'wojownik': 0, 'wojownika': 0, 'ninja': 1, 'ninjy': 1, 'sura': 2, 'sury': 2,
        'szaman': 3, 'szamana': 3}
JOB_NAMES = ['JOB_WARRIOR', 'JOB_ASSASSIN', 'JOB_SURA', 'JOB_SHAMAN']

ITEM_SECTIONS = ['Bransolety', 'Kolczyki', 'Naszyjniki', 'Buty', 'Bronie']
LINE_RE = re.compile(r'^(.*?)\s+PVP:\s*(\d)\s+PVE:\s*(\d)\s*(?:\((.*?)\))?\s*$', re.I)
STRIP_RE = re.compile(r'\s*\((?:lvl\s*\d+|nno|nns)\)', re.I)
MOD_RE = re.compile(r'\+1\s*(PVE/PVP|PVP/PVE|PVE|PVP)?\s*dla\s+(.+)$', re.I)


def parse_modifier(note):
    """"+1 PVE dla Szamana/Sury" -> (pve_jobs_mask, pvp_jobs_mask)."""
    if not note:
        return 0, 0
    m = MOD_RE.search(note)
    if not m:
        return 0, 0
    mode = (m.group(1) or 'PVE/PVP').upper()
    mask = 0
    for word in re.split(r'[/ ,]+', m.group(2).strip().lower()):
        if word in JOBS:
            mask |= 1 << JOBS[word]
    if mask == 0:
        raise SystemExit('dopisek bez klasy: %s' % note)
    pve = mask if 'PVE' in mode else 0
    pvp = mask if 'PVP' in mode else 0
    return pve, pvp


def read_sheet(path):
    lines = io.open(path, encoding='utf-8-sig').read().replace('\r\n', '\n').split('\n')
    sections = {}
    current = None
    for line in lines:
        s = line.strip()
        # "[TIERY KD] <-- [NEW!]": the section is the first bracket.
        m = re.match(r'^\[([^\]]+)\]', s)
        if m:
            current = m.group(1).strip()
            sections.setdefault(current, [])
            continue
        if current is not None and s:
            sections[current].append(s)
    return sections


def parse_rows(block):
    for line in block:
        m = LINE_RE.match(line)
        if not m:
            continue
        name = STRIP_RE.sub('', m.group(1)).strip()
        yield name, int(m.group(3)), int(m.group(2)), (m.group(4) or '').strip()


def parse_soul_stones(block, items, errors):
    """[TIERY KD] -> ({vnum: (pve, pvp, pvp_only, name)}, banned grades)."""
    if not block:
        errors.append('brak sekcji [TIERY KD]')
        return {}, set()
    protos = dict((vnum, (name, rest)) for vnum, name, rest in items)
    banned = None
    pvp_only = False
    rows = {}
    for line in block:
        ban = SOUL_STONE_BAN_RE.search(line)
        if ban:
            banned = set(int(g) for g in re.findall(r'\+(\d)', ban.group(1)))
            continue
        if SOUL_STONE_PVP_ONLY_RE.search(line):
            pvp_only = True
            continue
        m = LINE_RE.match(line)
        if not m:
            continue
        # "Kamien Duszy Penetracji<tab>+4": his columns are tabs, the name is words.
        name = ' '.join(m.group(1).split())
        row = SOUL_STONE_ROW_RE.match(norm(name))
        if not row or row.group(1) not in SOUL_STONE_KINDS:
            errors.append('kamien duszy bez rodzaju: "%s"' % name)
            continue
        kind, stem = SOUL_STONE_KINDS[row.group(1)]
        grade = int(row.group(2))
        vnum = 28000 + grade * 100 + kind
        proto = protos.get(vnum)
        proto_name = norm(proto[0]) if proto else ''
        proto_type = int(proto[1][0]) if proto and proto[1] and proto[1][0].isdigit() else -1
        if (not proto or proto_type != ITEM_METIN or stem not in proto_name or
                not proto_name.endswith('+%d' % grade)):
            errors.append('kamien duszy "%s" nie pasuje do vnum %d (%s)' % (name, vnum, proto_name or 'brak'))
            continue
        if vnum in rows:
            errors.append('kamien duszy dwa razy: "%s"' % name)
            continue
        rows[vnum] = (int(m.group(3)), int(m.group(2)), pvp_only, name)
    if banned is None:
        errors.append('[TIERY KD] bez zdania o zakazie stopni')
        banned = set()
    for vnum, (pve, pvp, only, name) in rows.items():
        if (vnum // 100) % 10 in banned:
            errors.append('kamien "%s" ma zakazany stopien' % name)
    return rows, banned


def main(item_path, sheet_path, out_path):
    items = load_hex(item_path)
    resolver = Resolver(items, [])
    sheet = read_sheet(sheet_path)
    for title in ITEM_SECTIONS + ['Bonusy']:
        if title not in sheet:
            raise SystemExit('brak sekcji [%s]' % title)

    item_rows = {}
    for title in ITEM_SECTIONS:
        for name, pve, pvp, note in parse_rows(sheet[title]):
            base = resolver.gear(TIER_GEAR_ALIASES.get(norm(name), name))
            if base is None:
                continue
            pve_jobs, pvp_jobs = parse_modifier(note)
            if base in item_rows:
                raise SystemExit('rodzina %s dwa razy (vnum %d)' % (name, base))
            item_rows[base] = (pve, pvp, pve_jobs, pvp_jobs, name)

    bonus_rows = {}
    errors = []
    for name, pve, pvp, note in parse_rows(sheet['Bonusy']):
        key = norm(name)
        key = TIER_BONUS_ALIASES.get(key, key)
        apply = BONUS_APPLIES.get(key)
        if apply is None:
            errors.append('bonus bez APPLY: "%s"' % name)
            continue
        pve_jobs, pvp_jobs = parse_modifier(note)
        row = (pve, pvp, pve_jobs, pvp_jobs)
        if apply in bonus_rows and bonus_rows[apply][:4] != row:
            errors.append('bonus %s ma dwie rozne oceny: %s i %s' % (apply, bonus_rows[apply][4], name))
            continue
        bonus_rows.setdefault(apply, row + (name,))

    stone_rows, banned_grades = parse_soul_stones(sheet.get('TIERY KD'), items, errors)

    problems = resolver.missing + errors
    if problems:
        sys.stderr.write('generate_iwakura_tiers: %d problemow:\n' % len(problems))
        for p in problems:
            sys.stderr.write('  %s\n' % p)
        return 1

    def jobs_text(mask):
        if mask == 0:
            return '0'
        return ' | '.join('(1 << %s)' % JOB_NAMES[j] for j in range(4) if mask & (1 << j))

    out = io.StringIO()
    out.write(u'''// Rendered by linux-port/overlays/playerbot/tools/generate_iwakura_tiers.py from Iwakura's
// tier list (data/iwakura_tiery.txt, 16 September, the soul stones added on
// 19 September). Do not edit by hand.
//
// Every family of bracelets, earrings, necklaces, boots and weapons, and every
// bonus line, rated 1 (bardzo zly) to 6 (wspanialy) - once for PvE, once for
// PvP - with the "+1 dla Wojownika" notes as job masks. Body armour, helmets
// and shields are judged by level and bonuses, not by name, so they are not
// here. The PvE column steers the hunting set today; the PvP column waits for
// the second set ("na przyszlosc pod posiadanie przez boty dwoch setow").
//
// The soul stones (Kamienie Duszy, the ITEM_METIN stones a socket takes) are
// his rule rather than a nudge: a stone of a banned grade (+0, +1 and +2 on
// his list) never goes into a weapon or an armour, only the stones listed
// here may, and the four class stones only into a PvP weapon.
#ifndef __INC_METIN2_PLAYERBOT_ITEM_TIERS_H__
#define __INC_METIN2_PLAYERBOT_ITEM_TIERS_H__

namespace
{
	const int PLAYERBOT_TIER_MIN = 1;
	const int PLAYERBOT_TIER_MAX = 6;

	// The +0 vnum of the family; the rest of the family is base + refine.
	struct TPlayerBotItemTier { DWORD dwBaseVnum; BYTE bPve; BYTE bPvp; BYTE bPveJobs; BYTE bPvpJobs; };
	const TPlayerBotItemTier PLAYERBOT_ITEM_TIERS[] = {
''')
    for base in sorted(item_rows):
        pve, pvp, pve_jobs, pvp_jobs, name = item_rows[base]
        out.write(u'\t\t{ %d, %d, %d, %s, %s }, // %s\n' % (
            base, pve, pvp, jobs_text(pve_jobs), jobs_text(pvp_jobs), ascii_comment(name)))
    out.write(u'''\t};

	struct TPlayerBotBonusTier { BYTE bApply; BYTE bPve; BYTE bPvp; BYTE bPveJobs; BYTE bPvpJobs; };
	const TPlayerBotBonusTier PLAYERBOT_BONUS_TIERS[] = {
''')
    for apply in sorted(bonus_rows, key=lambda a: (a in MT2009_ONLY, a)):
        pve, pvp, pve_jobs, pvp_jobs, name = bonus_rows[apply]
        row = u'\t\t{ %s, %d, %d, %s, %s }, // %s\n' % (
            apply, pve, pvp, jobs_text(pve_jobs), jobs_text(pvp_jobs), ascii_comment(name))
        if apply in MT2009_ONLY:
            out.write(u'#if defined(PLAYERBOT_ENGINE_MT2009)\n%s#endif\n' % row)
        else:
            out.write(row)
    out.write(u'''\t};

	// Iwakura's soul stones, by vnum (28000 + grade * 100 + kind). A stone of a
	// grade under PLAYERBOT_SOUL_STONE_MIN_GRADE, or one not on this list, is
	// never put into a weapon or an armour; a PvP-only stone never into the
	// hunting set.
	const int PLAYERBOT_SOUL_STONE_MIN_GRADE = @@MIN_GRADE@@;
	struct TPlayerBotSoulStoneTier { DWORD dwVnum; BYTE bPve; BYTE bPvp; bool bPvpOnly; };
	const TPlayerBotSoulStoneTier PLAYERBOT_SOUL_STONE_TIERS[] = {
@@STONES@@	};

	const TPlayerBotSoulStoneTier* FindPlayerBotSoulStoneTier(DWORD vnum)
	{
		if ((int)((vnum / 100) % 10) < PLAYERBOT_SOUL_STONE_MIN_GRADE)
			return NULL;
		for (size_t i = 0; i < sizeof(PLAYERBOT_SOUL_STONE_TIERS) / sizeof(PLAYERBOT_SOUL_STONE_TIERS[0]); ++i)
			if (PLAYERBOT_SOUL_STONE_TIERS[i].dwVnum == vnum)
				return &PLAYERBOT_SOUL_STONE_TIERS[i];
		return NULL;
	}

	// The stone's tier for the set it would go into, or 0 when it may not go
	// into that set at all: a banned grade, a stone off his list, or a PvP-only
	// stone asked about for the hunting set.
	int GetPlayerBotSoulStoneTier(DWORD vnum, bool pvp)
	{
		const TPlayerBotSoulStoneTier* row = FindPlayerBotSoulStoneTier(vnum);
		if (!row || (row->bPvpOnly && !pvp))
			return 0;
		return pvp ? row->bPvp : row->bPve;
	}

	int PlayerBotTierWithJob(int tier, BYTE jobs, int job)
	{
		if (tier <= 0)
			return 0;
		if (job >= 0 && job < 4 && (jobs & (1 << job)) != 0)
			++tier;
		return tier > PLAYERBOT_TIER_MAX ? PLAYERBOT_TIER_MAX : tier;
	}

	// The family's tier for this job, or 0 when his list does not carry it.
	// `job` is CHARACTER::GetJob(), 0..3; -1 asks without the job notes.
	int GetPlayerBotItemTier(DWORD baseVnum, int job, bool pvp)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_ITEM_TIERS) / sizeof(PLAYERBOT_ITEM_TIERS[0]); ++i)
			if (PLAYERBOT_ITEM_TIERS[i].dwBaseVnum == baseVnum)
				return pvp ? PlayerBotTierWithJob(PLAYERBOT_ITEM_TIERS[i].bPvp, PLAYERBOT_ITEM_TIERS[i].bPvpJobs, job)
						: PlayerBotTierWithJob(PLAYERBOT_ITEM_TIERS[i].bPve, PLAYERBOT_ITEM_TIERS[i].bPveJobs, job);
		return 0;
	}

	// The bonus line's tier for this job, or 0 when his list does not name it.
	int GetPlayerBotBonusTier(BYTE apply, int job, bool pvp)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_BONUS_TIERS) / sizeof(PLAYERBOT_BONUS_TIERS[0]); ++i)
			if (PLAYERBOT_BONUS_TIERS[i].bApply == apply)
				return pvp ? PlayerBotTierWithJob(PLAYERBOT_BONUS_TIERS[i].bPvp, PLAYERBOT_BONUS_TIERS[i].bPvpJobs, job)
						: PlayerBotTierWithJob(PLAYERBOT_BONUS_TIERS[i].bPve, PLAYERBOT_BONUS_TIERS[i].bPveJobs, job);
		return 0;
	}
}

#endif
''')
    stones = u''
    for vnum in sorted(stone_rows):
        pve, pvp, only, name = stone_rows[vnum]
        stones += u'\t\t{ %d, %d, %d, %s }, // %s\n' % (vnum, pve, pvp, 'true' if only else 'false', ascii_comment(name))
    min_grade = 0
    while min_grade in banned_grades:
        min_grade += 1
    text = out.getvalue().replace('@@STONES@@', stones).replace('@@MIN_GRADE@@', str(min_grade))
    if any(ord(ch) > 127 for ch in text):
        raise SystemExit('naglowek ma znaki spoza ASCII')
    io.open(out_path, 'w', encoding='ascii', newline='\n').write(text)
    print('zapisano %s' % out_path)
    print('  rodzin: %d, bonusow: %d (tylko mt2009: %d), kamieni duszy: %d (tylko PvP: %d), zakazane stopnie: %s' % (
        len(item_rows), len(bonus_rows), sum(1 for a in bonus_rows if a in MT2009_ONLY),
        len(stone_rows), sum(1 for r in stone_rows.values() if r[2]),
        ', '.join('+%d' % g for g in sorted(banned_grades))))
    return 0


if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.stderr.write(__doc__)
        sys.exit(2)
    sys.exit(main(*sys.argv[1:]))
