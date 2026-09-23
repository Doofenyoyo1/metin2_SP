#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""The guild names the bots found guilds under, rendered from Iwakura's
list into a header the guild fragment includes.

    python linux-port/overlays/playerbot/tools/generate_guild_names.py

Reads data/guild_names_iwakura.txt (one name a line, in the order she
wrote them) and writes src/game/src/playerbot_guild_names.h. A name is
kept when the engine's own check_name would keep it - letters and digits
only - and the length limit is applied at run time against the engine's
GUILD_NAME_MAX_LEN (fourteen on mt2009, twelve on r40250), so one header
serves both lines and a name too long for one of them is simply skipped
there. Names are unique here; the founder checks FindGuildByName anyway.
"""
import io
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, '..', 'data', 'guild_names_iwakura.txt')
OUT = os.path.join(HERE, '..', 'src', 'game', 'src', 'playerbot_guild_names.h')


def main():
    names = []
    dropped = []
    seen = set()
    for raw in io.open(DATA, encoding='utf-8'):
        name = raw.strip()
        if not name:
            continue
        if not re.match(r'^[A-Za-z0-9]+$', name) or name.lower() in seen:
            dropped.append(name)
            continue
        seen.add(name.lower())
        names.append(name)
    lines = [
        '// Rendered by linux-port/overlays/playerbot/tools/generate_guild_names.py',
        '// from data/guild_names_iwakura.txt. DO NOT EDIT; edit the list and re-run.',
        '//',
        '// The names the bots found guilds under, in the order Iwakura wrote them.',
        '// A founder walks this pool from a pid-based offset and takes the first',
        '// name no guild in the world wears and the engine\'s GUILD_NAME_MAX_LEN',
        '// admits (fourteen on mt2009, twelve on r40250), so the same header serves',
        '// both lines. %d names.' % len(names),
        '#ifndef __INC_METIN2_PLAYERBOT_GUILD_NAMES_H__',
        '#define __INC_METIN2_PLAYERBOT_GUILD_NAMES_H__',
        '',
        'namespace',
        '{',
        '\tconst char* const PLAYERBOT_GUILD_NAME_POOL[] = {',
    ]
    for i in range(0, len(names), 4):
        lines.append('\t\t' + ', '.join('"%s"' % n for n in names[i:i + 4]) + ',')
    lines += [
        '\t};',
        '\tconst size_t PLAYERBOT_GUILD_NAME_POOL_SIZE = sizeof(PLAYERBOT_GUILD_NAME_POOL) / sizeof(PLAYERBOT_GUILD_NAME_POOL[0]);',
        '}',
        '',
        '#endif',
        '',
    ]
    io.open(OUT, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines))
    print('generate_guild_names: %d names -> %s' % (len(names), os.path.relpath(OUT)))
    if dropped:
        print('  dropped (not letters and digits, or a repeat): %s' % ', '.join(dropped))


if __name__ == '__main__':
    main()
