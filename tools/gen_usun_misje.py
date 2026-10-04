#!/usr/bin/env python3
# MT2009_PLUS_CLEAR_MISSIONS_V1: builds the mission table of
# linux-port/docker/game/quest/usun_misje.quest (the /usunmisje window) from
# the quest sources, between its "-- BEGIN GENERATED" / "-- END GENERATED"
# lines.
#
#   python3 tools/gen_usun_misje.py [stock quest dir]
#
# The stock quest dir is the package's share/locale/poland/quest (default: the
# test tree's copy). Its object/state lists the quests the cores load; their
# sources are in its quest/ (quest/_unused is skipped) and ours in
# linux-port/docker/game/quest, which win over a stock file of the same name.
#
# A quest is in the table only when its name is on the whitelist below (the
# story, the side quests, the Biologist, Baek-Go's herbs) and it has a
# finished state; everything else - the Companion, Cor Draconis (dragon_soul),
# the horse, guild, skill, fishing/herbalism onboarding, reputation, quest
# books, hunting, events, dungeons, Seon-Hae, our own system quests - is never
# listed. Per quest the table keeps:
#   the states that show a letter (and the text of the letter: a gameforge. /
#   translate. path the quest resolves at run time), the finished state, the
#   quests its finish starts (set_quest_state(...) - the next part of a
#   chain, started as the quest itself would), the level it opens at and the
#   quest's own items (given and taken back by the quest itself, 30000-30999,
#   no Biologist hand-ins, none another non-listed quest uses).
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OURS = os.path.join(ROOT, 'linux-port', 'docker', 'game', 'quest')
TARGET = os.path.join(OURS, 'usun_misje.quest')
STOCK = sys.argv[1] if len(sys.argv) > 1 else \
	'/opt/metin2/mt2009plustest/linux-port/docker/game/src/serverfiles/share/locale/poland/quest'

# category: 1 story, 2 side quest, 3 Biologist, 4 Baek-Go's herbs
WHITELIST = [
	(r'main_quest_lv\d+$', 1),
	(r'find_squareguard$', 1),
	(r'find_brother_article$', 1),
	(r'patrol_townaround$', 1),
	(r'subquest_\d+$', 2),
	(r'new_quest_lv\d+$', 2),
	(r'new_quest_premium_lv\d+$', 2),
	(r'collect_quest_lv\d+$', 3),
	(r'make_herb_lv\d+$', 4),
]
# never, whatever the whitelist says: their finish is a reward or a system
NEVER = {
	'towarzysz', 'dragon_soul', 'dragon_soul_refine', 'dragon_soul_shop',
	'hwang_introduction',  # its finished state opens Hwang's shop
	'trade_chat', 'black_steel_crafting', 'warehouse_expand',
}
FINAL = ['__COMPLETE__', '__complete', 'COMPLETE', 'complete', '__THEEND__', 'THEEND']


def read(path):
	raw = open(path, 'rb').read()
	for enc in ('utf-8', 'cp1250'):
		try:
			return raw.decode(enc)
		except UnicodeDecodeError:
			pass
	return raw.decode('latin-1')


def quest_name(text):
	m = re.search(r'^\s*quest\s+(\w+)\s+begin', text, re.M)
	return m.group(1) if m else None


def no_comments(text):
	return '\n'.join(re.sub(r'--.*$', '', line) for line in text.split('\n'))


def category(name):
	if name in NEVER:
		return 0
	for pattern, cat in WHITELIST:
		if re.match(pattern, name):
			return cat
	return 0


def main():
	loaded = set(os.listdir(os.path.join(STOCK, 'object', 'state')))
	sources = {}
	for root, dirs, files in os.walk(os.path.join(STOCK, 'quest')):
		if '_unused' in root.split(os.sep):
			continue
		for f in sorted(files):
			if f.endswith('.quest'):
				t = read(os.path.join(root, f))
				n = quest_name(t)
				if n and n not in sources:
					sources[n] = t
	for f in sorted(os.listdir(OURS)):
		if f.endswith('.quest') and f != 'usun_misje.quest':
			t = read(os.path.join(OURS, f))
			n = quest_name(t)
			if n:
				sources[n] = t
				loaded.add(n)

	items_of = {}   # vnum -> quests that name it
	entries = []
	for n in sorted(loaded):
		if n not in sources:
			continue
		t = no_comments(sources[n])
		for v in set(int(x) for x in re.findall(r'\b(3\d{4})\b', t)):
			items_of.setdefault(v, set()).add(n)
		cat = category(n)
		if not cat:
			continue
		parts = re.split(r'^\s*state\s+(\w+)\s+begin', t, flags=re.M)
		states = {}
		for i in range(1, len(parts), 2):
			states[parts[i]] = parts[i + 1]
		letters = {}
		for s, body in states.items():
			if re.search(r'when\s+letter\b|send_letter|q\.start\s*\(', body):
				m = re.search(r'(?:send_letter(?:_ex)?|q\.set_title)\s*\(\s*((?:gameforge|translate)\.[\w.]+)', body)
				letters[s] = m.group(1) if m else ''
		final = [f for f in FINAL if f in states]
		if not letters or not final:
			continue
		for s in ('start', 'run') + tuple(final):
			letters.pop(s, None)
		# a state without a letter text of its own: the quest's first one
		named = [letters[s] for s in sorted(letters) if letters[s]]
		for s in letters:
			if not letters[s] and named:
				letters[s] = named[0]
		chain = []
		for a, b in re.findall(r'set_quest_state\s*\(\s*"(\w+)"\s*,\s*"(\w+)"', t):
			if a != n and (a, b) not in chain:
				chain.append((a, b))
		level = 0
		head = states.get('start', '') + states.get('run', '')
		m = re.search(r'(?:pc\.level|pc\.get_level\(\))\s*>=\s*(\d+)', head)
		if m:
			level = int(m.group(1))
		else:
			m = re.search(r'lv(\d+)$', n)
			if m:
				level = int(m.group(1))
		given = set(int(x) for x in re.findall(r'pc\.give_item2?\s*\(\s*"?(\d+)', t))
		taken = set(int(x) for x in re.findall(r'pc\.remove_?item\s*\(\s*"?(\d+)', t))
		own = sorted(v for v in given & taken if 30000 <= v <= 30999)
		if cat == 3:
			own = []
		entries.append((n, cat, level, final[0], letters, chain, own))

	listed = set(e[0] for e in entries)
	out = []
	out.append('\t\t-- generated by tools/gen_usun_misje.py: %d quests' % len(entries))
	out.append('\t\t-- { name, category, level, finished state, { [letter state] = letter text }, { { next quest, its state } }, { own items } }')
	out.append('\t\tfunction data()')
	out.append('\t\t\treturn {')
	for n, cat, level, final, letters, chain, own in entries:
		# an item another quest outside the table also uses stays
		own = [v for v in own if items_of.get(v, set()) <= listed]
		ls = ', '.join('["%s"] = "%s"' % (s, letters[s]) for s in sorted(letters))
		cs = ', '.join('{ "%s", "%s" }' % c for c in chain)
		os_ = ', '.join(str(v) for v in own)
		out.append('\t\t\t\t{ "%s", %d, %d, "%s", { %s }, { %s }, { %s } },' % (n, cat, level, final, ls, cs, os_))
	out.append('\t\t\t}')
	out.append('\t\tend')
	out.append('')
	out.append('\t\t-- own item -> the listed quests that use it')
	out.append('\t\tfunction item_users()')
	out.append('\t\t\treturn {')
	used = sorted(set(v for e in entries for v in e[6] if items_of.get(v, set()) <= listed))
	for v in used:
		out.append('\t\t\t\t[%d] = { %s },' % (v, ', '.join('"%s"' % q for q in sorted(items_of[v]))))
	out.append('\t\t\t}')
	out.append('\t\tend')

	text = open(TARGET, 'rb').read().decode('cp1250')
	nl = '\r\n' if '\r\n' in text else '\n'
	begin = text.index('-- BEGIN GENERATED')
	begin = text.index('\n', begin) + 1
	end = text.index('-- END GENERATED')
	end = text.rindex('\n', 0, end) + 1
	text = text[:begin] + nl.join(out) + nl + text[end:]
	open(TARGET, 'wb').write(text.encode('cp1250'))
	print('%s: %d quests, %d own items' % (TARGET, len(entries), len(used)))


if __name__ == '__main__':
	main()
