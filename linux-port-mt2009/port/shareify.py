# -*- coding: utf-8 -*-
"""What the r40250 image adds to its share tree, for the mt2009 package.

Usage:  python shareify.py

Copies linux-port/docker/game/{mob_drop_item.m3.append.txt,
special_item_group.moonlight.txt} into linux-port-mt2009/docker/game/, writes
special_item_group.starter.txt beside them and puts the append step into the
runtime stage of docker/game/Dockerfile:

  * mob_drop_item.m3.append.txt - the level-30 weapon ladder's kill groups
    on the guild map's cursed wolves. 132/133/135/136 stand on map 24 in
    this package too (group_group 1101-1104, the same layout); the 127-130
    groups name mobs the package lacks and register on nothing.
  * special_item_group.moonlight.txt - the Moonlight chest (50011) the bots
    open, replacing the stock block: the reader keeps the first group it
    reads for a vnum, so the stock one is cut before ours goes on the end.
  * special_item_group.starter.txt - the starter chest chain (50187, 50212,
    50213 for the three job pairs, then 50188..50196 by level), as r40250's
    share defines it. The playerbot seed puts the lv1 chest in every bot's
    bag and this package has no group for any of them: 1500 chests refused
    fifty-five thousand times an hour. The lv30 and lv60 chests here lack
    r40250's 76016 and 76009 lines, items this package does not have; one
    missing item fails the whole file ("cannot load SpecialItemGroup").
    The chain used to stop at lv60, which hands out the lv70 chest, and a
    giftbox with no group is refused at every use and named only then
    ("cannot find special item group 50194", 560 a minute from the bots of
    seventy). check_chain refuses a chain like that here.

Idempotent: re-run after editing an original.
"""
import io
import os
import re
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
R40250_GAME = os.path.normpath(os.path.join(HERE, '..', '..', 'linux-port', 'docker', 'game'))
GAME = os.path.normpath(os.path.join(HERE, '..', 'docker', 'game'))
WORLD_SQL = os.path.normpath(os.path.join(HERE, '..', 'docker', 'mariadb', 'initdb.d', 'dumps', 'world.sql'))


ITEM_GIFTBOX = 23  # common/item_length.h


def dump_values(table):
    """The VALUES the package's world dump inserts into a proto table."""
    text = io.open(WORLD_SQL, encoding='utf-8', errors='replace', newline='').read()
    m = re.search(r"INSERT INTO `%s` VALUES(.*?);\n" % table, text, re.S)
    if not m:
        raise SystemExit('shareify: no INSERT INTO `%s` in %s' % (table, WORLD_SQL))
    return m.group(1)


def dump_vnums(table):
    """The vnums the package's world dump inserts into a proto table."""
    return set(int(v) for v in re.findall(r"[(]\s*(\d+)\s*,\s*'", dump_values(table)))


def dump_giftboxes():
    """The item vnums of type ITEM_GIFTBOX; item_proto's columns run vnum,
    name, locale_name, type."""
    rows = re.findall(r"[(]\s*(\d+)\s*,\s*'(?:[^'\\]|\\.)*'\s*,\s*'(?:[^'\\]|\\.)*'\s*,\s*(\d+)\s*,",
                      dump_values('item_proto'))
    return set(int(v) for v, t in rows if int(t) == ITEM_GIFTBOX)


def group_items(text):
    """Item vnums named by the lines of a special_item_group / mob_drop_item file."""
    return set(int(v) for v in re.findall(r"^\s*\d+\s+(\d+)\s", text, re.M))


def group_vnums(text):
    """The vnums a special_item_group file defines groups under, in file order."""
    return [int(v) for v in re.findall(r"^\s*Vnum\s+(\d+)", text, re.M)]


def check_items(name, text, items):
    missing = sorted(group_items(text) - items)
    if missing:
        # One unknown item fails the whole special_item_group.txt at boot
        # ("cannot load SpecialItemGroup"), and with it every chest in the
        # world. Refuse here rather than find out in syserr.
        raise SystemExit('shareify: %s names items the package lacks: %s' % (name, missing))


def check_chain(name, text, giftboxes):
    orphans = sorted((group_items(text) & giftboxes) - set(group_vnums(text)))
    if orphans:
        # A giftbox opens through a group under its own vnum, and one with no
        # group loads without a word and is refused at every use: the lv60
        # chest handed out Skrzynia Mistrza II with no lv70 group behind it,
        # and syserr took "cannot find special item group 50194" 560 times a
        # minute from the bots of seventy asking for it.
        raise SystemExit('shareify: %s hands out giftboxes with no group of their own: %s' % (name, orphans))

STARTER = """\
# The starter chest chain, as r40250's special_item_group.txt defines it
# (rendered by linux-port-mt2009/port/shareify.py). The playerbot seed puts
# the lv1 chest in every bot's bag; the package has no group for it.
Group	lv1(sura_warrior)
{
	Vnum	50187
	Type	Pct
	1	50188	1	100
	2	10	1	100
	3	27051	20	100
	4	27052	10	100
	5	27053	5	100
	6	27054	5	100
}
Group	lv1(assassin)
{
	Vnum	50212
	Type	Pct
	1	50188	1	100
	2	1000	1	100
	3	27051	20	100
	4	27052	10	100
	5	27053	5	100
	6	27054	5	100
}
Group	lv1(shaman)
{
	Vnum	50213
	Type	Pct
	1	50188	1	100
	2	7000	1	100
	3	27051	20	100
	4	27052	10	100
	5	27053	5	100
	6	27054	5	100
}
Group	lv10
{
	Vnum	50188
	Type	Pct
	1	76012	3	100
	2	76017	3	100
	3	76021	1	100
	4	76008	3	100
	5	50189	1	100
}
Group	lv20
{
	Vnum	50189
	Type	Pct
	1	76012	3	100
	2	76017	3	100
	3	76004	1	100
	4	76006	1	100
	5	50190	1	100
	6	76023	3	100
	7	76024	3	100
}
Group	lv30
{
	Vnum	50190
	Type	Pct
	1	76012	3	100
	2	76017	3	100
	3	76011	1	100
	4	50191	1	100
}
Group	lv40
{
	Vnum	50191
	Type	Pct
	1	76012	3	100
	2	76018	3	100
	3	71153	1	100
	4	76011	1	100
	5	50192	1	100
}
Group	lv50
{
	Vnum	50192
	Type	Pct
	1	76003	3	100
	2	76018	3	100
	3	76007	20	100
	4	76019	5	100
	5	70058	1	100
	6	50193	1	100
}
Group	lv60
{
	Vnum	50193
	Type	Pct
	1	76003	3	100
	2	76018	3	100
	3	76000	5	100
	4	50194	1	100
}
Group	lv70
{
	Vnum	50194
	Type	Pct
	1	76003	3	100
	2	76018	3	100
	3	76013	3	100
	4	76014	3	100
	5	76001	1	100
	6	50195	1	100
}
Group	lv80
{
	Vnum	50195
	Type	Pct
	1	76003	3	100
	2	76018	3	100
	3	76005	1	100
	4	76020	3	100
	5	50196	1	100
}
Group	lv90
{
	Vnum	50196
	Type	Pct
	1	76022	1	100
	2	76015	1	100
	3	76010	3	100
	4	76002	3	100
}
"""

# Goes right after the share COPYs of the runtime stage. The package's file
# is UTF-8 with CRLF; the appended text is given the same endings so the
# file stays one thing. The awk keeps every Group..} block that does not
# name one of the vnums ours define. @CUT@ is that list, read out of the two
# files, and a Dockerfile that already carries the step has its list brought
# up to date (AWK_CUT), so a group added to STARTER is never shadowed by a
# stock block the reader met first.
DOCKERFILE_ANCHOR = 'COPY src/serverfiles/share/package /opt/metin2/share/package\n'
DOCKERFILE_STEP = r'''
# What the r40250 image adds to its share, for the poland locale here
# (port/shareify.py renders the three files and this step):
#  * mob_drop_item.m3.append.txt - the level-30 weapon ladder's kill groups on
#    the guild map's cursed wolves;
#  * special_item_group.moonlight.txt - the Moonlight chest (50011) the bots
#    open, replacing the stock block (the reader keeps the first group per
#    vnum, so the stock one is cut out first);
#  * special_item_group.starter.txt - the starter chest chain the playerbot
#    seed hands out and the package has no group for.
COPY mob_drop_item.m3.append.txt special_item_group.moonlight.txt special_item_group.starter.txt /tmp/share-add/
RUN set -eu; L=/opt/metin2/share/locale/poland \
 && for f in /tmp/share-add/*.txt; do sed -i 's/\r$//; s/$/\r/' "$f"; done \
 && cat /tmp/share-add/mob_drop_item.m3.append.txt >> "$L/mob_drop_item.txt" \
 && f="$L/special_item_group.txt" \
 && awk 'BEGIN{keep=1} /^Group/{blk=""; keep=1} {blk=blk $0 "\n"} /Vnum[ \t]+(@CUT@)([^0-9]|$)/{keep=0} /^}/{ if (keep) printf "%s", blk; blk=""; keep=1 }' "$f" > "$f.new" \
 && cat /tmp/share-add/special_item_group.moonlight.txt /tmp/share-add/special_item_group.starter.txt >> "$f.new" \
 && mv "$f.new" "$f" \
 && rm -rf /tmp/share-add \
 && echo "share: moonlight + starter chests, M3 drops appended"
'''
AWK_CUT = re.compile(r"(/Vnum\[ \\t\]\+\()([0-9|]+)(\)\(\[\^0-9\]\|\$\)/)")


# Maska Sabaha leaves the world with the Hwang curse (playerbotify's
# apply_hwang_curse_removed). A drop line is rolled at nothing rather than
# deleted, because a mob_drop_item group stops reading at the first index it
# lacks; the Hwang loot box rolls it at nothing too and the temple's
# introduction no longer hands one out. world.shop_item loses it in apply.sh.
DOCKERFILE_MASK_ANCHOR = ' && echo "share: moonlight + starter chests, M3 drops appended"\n'
DOCKERFILE_MASK_MARKER = 'echo "share: Maska Sabaha removed"'
DOCKERFILE_MASK_STEP = r'''
# Maska Sabaha goes with the Hwang curse (playerbotify apply_hwang_curse_removed,
# port/shareify.py renders this step): its drop lines and the Hwang loot box roll
# it at nothing - a group stops reading at the first index it lacks, so a line is
# zeroed, not deleted - and the temple's introduction hands none out.
RUN set -eu; L=/opt/metin2/share/locale/poland \
 && sed -i -E 's/^([[:blank:]]*[0-9]+[[:blank:]]+7273[15][[:blank:]]+[0-9]+[[:blank:]]+)[0-9.]+/\10/' "$L/mob_drop_item.txt" \
 && sed -i -E '/^Group[[:blank:]]+LootBox_Hwang/,/^}/ s/^([[:blank:]]*[0-9]+[[:blank:]]+7273[15][[:blank:]]+[0-9]+[[:blank:]]+)[0-9.]+/\10/' "$L/special_item_group.txt" \
 && sed -i '/^reward_data\.hwang_introduction/,/^}/ { /{72731, 1},/d }' "$L/quest/libs/other/reward_data.lua" \
 && ! grep -E '^[[:blank:]]*[0-9]+[[:blank:]]+7273[15][[:blank:]]+[0-9]+[[:blank:]]+[1-9]' "$L/mob_drop_item.txt" \
 && ! grep -q '{72731, 1}' "$L/quest/libs/other/reward_data.lua" \
 && echo "share: Maska Sabaha removed"
'''


# The quests call this world Metin2009 - main_quest_lv1's letter and greeting,
# the guard's warning, the quiz - where it is Metin2 SinglePlayer (l0st3k, 15
# September, who sent translate.lua with those sixteen lines changed). Both
# copies of translate.lua carry the strings; every other byte of them, the
# charset test line at the head included, stays as the package has it.
DOCKERFILE_BRAND_ANCHOR = ' && echo "share: Maska Sabaha removed"\n'
DOCKERFILE_BRAND_MARKER = 'echo "share: Metin2 SinglePlayer in the quest texts"'
DOCKERFILE_BRAND_STEP = r"""
# The quest texts name this world, not Metin2009 (port/shareify.py renders this
# step): the substitution l0st3k's translate.lua of 15 September makes, on both
# copies of the file.
RUN set -eu; L=/opt/metin2/share/locale/poland \
 && for f in "$L/translate.lua" "$L/quest/libs/translate/translate.lua"; do \
      [ -f "$f" ] || continue; \
      LC_ALL=C sed -i 's/Metin2009/Metin2 SinglePlayer/g' "$f"; \
      if LC_ALL=C grep -q 'Metin2009' "$f"; then echo "share: Metin2009 left in $f" >&2; exit 1; fi; \
    done \
 && echo "share: Metin2 SinglePlayer in the quest texts"
"""

# The world's difficulty. 2.0.55 took the Biologist's wait between two
# hand-ins out for everybody (a day, `time_until_hour` in collect_data.lua,
# asked through collect_data.is_wait; namiot_, 15 September) with a sed that
# made is_research_in_progress answer false. Since 2.0.57 that is "easy":
# quest/m2_difficulty.lua overrides the two collect_data functions with the
# event flags the migrator writes from M2_DIFFICULTY, and gives the horse
# quests m2_horse_wait(). The file is copied into the libs and dofile'd
# after them from _otherModuleLoader.lua (CRLF, stripped first). The bots'
# own hand-in and stable keeper never kept a wait.
DOCKERFILE_BIOLOGIST_ANCHOR = ' && echo "share: Metin2 SinglePlayer in the quest texts"\n'
DOCKERFILE_BIOLOGIST_MARKER = 'echo "share: difficulty hooked into the quest libraries"'
DOCKERFILE_BIOLOGIST_STEP = r"""
# The world's difficulty (port/shareify.py renders this step): m2_difficulty.lua
# is dofile'd after the quest libraries and puts the Biologist's wait and the
# stable keeper's waits on event flags the migrator writes from M2_DIFFICULTY.
# It replaces the 2.0.55 sed that made collect_data.is_research_in_progress
# answer false for everybody (namiot_, 15 September): that is what "easy" is.
COPY quest/m2_difficulty.lua /opt/metin2/share/locale/poland/quest/libs/other/m2_difficulty.lua
RUN set -eu; L=/opt/metin2/share/locale/poland/quest/libs/other/_otherModuleLoader.lua \
 && LC_ALL=C grep -q 'other/collect_data.lua' "$L" \
 && LC_ALL=C sed -i 's/\r$//' "$L" \
 && printf '\ndofile( LIBDIR .. "other/m2_difficulty.lua")\n' >> "$L" \
 && LC_ALL=C grep -q 'other/m2_difficulty.lua' "$L" \
 && test -s /opt/metin2/share/locale/poland/quest/libs/other/m2_difficulty.lua \
 && echo "share: difficulty hooked into the quest libraries"
"""

# The monkey curse. The package's monkey_curse quest (map_entrance) sets a
# timer at every login inside a Monkey Dungeon - 55 minutes on the three easy
# ones, 35 on 108, 25 on 109 - and when it runs out turns the
# character into a monkey (5003) for five minutes and warps it to its village,
# unless the herb of that dungeon's monkeys (50057-50059) is running. A bot
# has no herb and no answer to it: the medal droppers live in those dungeons
# and were thrown out as monkeys ("klatwa malp, z ktora boty nie potrafia
# sobie poradzic", SIZOWSKI, 12 September; "usuniemy to", Tieru, 18
# September). The cores load a quest's event handlers from its compiled
# object files, so those are deleted from the image; the state table stays,
# so a character's saved monkey_curse flags still name a quest the engine
# knows. The herbs stay in the world too: subquest_39 asks for the hard one.
DOCKERFILE_CURSE_ANCHOR = ' && echo "share: difficulty hooked into the quest libraries"\n'
DOCKERFILE_CURSE_MARKER = 'echo "share: monkey curse removed'
DOCKERFILE_CURSE_STEP = r"""
# The monkey curse (port/shareify.py renders this step): the package's
# monkey_curse quest turned anybody 55 minutes into a Monkey Dungeon (35 on
# 108, 25 on 109) into a monkey and warped them out, bots included. Its event
# handlers go; its state table stays for the flags characters already carry.
RUN set -eu; O=/opt/metin2/share/locale/poland/quest/object \
 && n=$(find "$O" -path "$O/state" -prune -o -type f -name 'monkey_curse.*' -print | wc -l) \
 && find "$O" -path "$O/state" -prune -o -type f -name 'monkey_curse.*' -exec rm -f {} + \
 && rm -rf "$O/monkey_curse" \
 && ! find "$O" -path "$O/state" -prune -o -name 'monkey_curse*' -print | grep -q . \
 && echo "share: monkey curse removed ($n handlers)"
"""


def main():
    items = dump_vnums('item_proto')
    mobs = dump_vnums('mob_proto')
    giftboxes = dump_giftboxes()
    print('shareify: package has %d items (%d giftboxes), %d mobs' % (len(items), len(giftboxes), len(mobs)))
    for name in ('mob_drop_item.m3.append.txt', 'special_item_group.moonlight.txt'):
        text = io.open(os.path.join(R40250_GAME, name), encoding='latin-1', newline='').read()
        check_items(name, text, items)
        shutil.copyfile(os.path.join(R40250_GAME, name), os.path.join(GAME, name))
        print('shareify: %s copied' % name)
    m3 = io.open(os.path.join(GAME, 'mob_drop_item.m3.append.txt'), encoding='latin-1', newline='').read()
    absent = sorted(set(int(v) for v in re.findall(r"^\s*Mob\s+(\d+)", m3, re.M)) - mobs)
    if absent:
        # The reader registers a kill group under any vnum; a mob that never
        # spawns simply never drops. Said once so nobody measures it.
        print('shareify: note: M3 drop groups for mobs the package lacks (never spawn): %s' % absent)
    check_items('special_item_group.starter.txt', STARTER, items)
    check_chain('special_item_group.starter.txt', STARTER, giftboxes)
    with io.open(os.path.join(GAME, 'special_item_group.starter.txt'), 'w', encoding='ascii', newline='\n') as f:
        f.write(STARTER)
    print('shareify: special_item_group.starter.txt written')

    moonlight = io.open(os.path.join(GAME, 'special_item_group.moonlight.txt'), encoding='latin-1', newline='').read()
    cut = '|'.join(str(v) for v in group_vnums(moonlight) + group_vnums(STARTER))
    dockerfile = os.path.join(GAME, 'Dockerfile')
    s = io.open(dockerfile, encoding='utf-8', newline='').read()
    assert '\r' not in s
    if 'special_item_group.starter.txt /tmp/share-add/' in s:
        lists = AWK_CUT.findall(s)
        assert len(lists) == 1, len(lists)
        if lists[0][1] == cut:
            print('shareify: Dockerfile already carries the step')
        else:
            s = AWK_CUT.sub(lambda m: m.group(1) + cut + m.group(3), s)
            print('shareify: Dockerfile step now cuts the stock groups of %s' % cut)
    else:
        assert s.count(DOCKERFILE_ANCHOR) == 1, s.count(DOCKERFILE_ANCHOR)
        s = s.replace(DOCKERFILE_ANCHOR, DOCKERFILE_ANCHOR + DOCKERFILE_STEP.replace('@CUT@', cut))
        print('shareify: Dockerfile step added')
    if DOCKERFILE_MASK_MARKER in s:
        print('shareify: Dockerfile already removes Maska Sabaha')
    else:
        assert s.count(DOCKERFILE_MASK_ANCHOR) == 1, s.count(DOCKERFILE_MASK_ANCHOR)
        s = s.replace(DOCKERFILE_MASK_ANCHOR, DOCKERFILE_MASK_ANCHOR + DOCKERFILE_MASK_STEP)
        print('shareify: Maska Sabaha step added')
    if DOCKERFILE_BRAND_MARKER in s:
        print('shareify: Dockerfile already names Metin2 SinglePlayer in the quest texts')
    else:
        assert s.count(DOCKERFILE_BRAND_ANCHOR) == 1, s.count(DOCKERFILE_BRAND_ANCHOR)
        s = s.replace(DOCKERFILE_BRAND_ANCHOR, DOCKERFILE_BRAND_ANCHOR + DOCKERFILE_BRAND_STEP)
        print('shareify: Metin2 SinglePlayer step added')
    if DOCKERFILE_BIOLOGIST_MARKER in s:
        print('shareify: Dockerfile already hooks the difficulty into the quest libraries')
    else:
        assert s.count(DOCKERFILE_BIOLOGIST_ANCHOR) == 1, s.count(DOCKERFILE_BIOLOGIST_ANCHOR)
        s = s.replace(DOCKERFILE_BIOLOGIST_ANCHOR, DOCKERFILE_BIOLOGIST_ANCHOR + DOCKERFILE_BIOLOGIST_STEP)
        print('shareify: difficulty step added')
    if DOCKERFILE_CURSE_MARKER in s:
        print('shareify: Dockerfile already removes the monkey curse')
    else:
        assert s.count(DOCKERFILE_CURSE_ANCHOR) == 1, s.count(DOCKERFILE_CURSE_ANCHOR)
        s = s.replace(DOCKERFILE_CURSE_ANCHOR, DOCKERFILE_CURSE_ANCHOR + DOCKERFILE_CURSE_STEP)
        print('shareify: monkey curse step added')
    io.open(dockerfile, 'w', encoding='utf-8', newline='').write(s)


if __name__ == '__main__':
    main()
