# -*- coding: utf-8 -*-
"""Carry the service definitions the r40250 compose file has for the panels,
the ItemShop and the updater into the mt2009 compose file, verbatim but for
the build contexts: those images are built from the same directories under
linux-port/docker, so the contexts point back there instead of being copied.

Usage:  python composify.py

Idempotent: the block is replaced between two marker lines.
"""
import io
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, '..', '..', 'linux-port', 'docker', 'docker-compose.yml'))
DST = os.path.normpath(os.path.join(HERE, '..', 'docker', 'docker-compose.yml'))
# The player's copy: on their machine the mt2009 tree IS linux-port/docker,
# with the panel, ItemShop and updater contexts beside it, so the contexts
# are ./<name> there. The packager publishes this file as docker-compose.yml.
DEPLOY = os.path.normpath(os.path.join(HERE, '..', 'docker', 'docker-compose.deploy.yml'))

BEGIN = '  # ==== services shared with linux-port (rendered by port/composify.py) ====\n'
END = '  # ==== end of the shared services ====\n'


def block(text, start_key, end_key):
    a = text.index(start_key)
    b = text.index(end_key, a)
    return text[a:b]


def main():
    src = io.open(SRC, encoding='utf-8', newline='').read()
    assert '\r' not in src
    panel = block(src, '  panel:\n', '  client-builder:\n')
    rest = block(src, '  seban-panel:\n', '\nvolumes:\n')
    shared = panel + rest
    # The images are built from linux-port/docker/<name>: same Dockerfiles,
    # same app sources, one copy.
    shared, n = re.subn(r'context: \./(panel|seban-panel|itemshop|updater)\b',
                        r'context: ../../linux-port/docker/\1', shared)
    assert n == 4, n
    # Which engine the panels are looking at: the two schema differences
    # (account.empire, player.bank_value) and the item-attribute numbering
    # (POINT_* here, APPLY_* on r40250) are switched on this in their code.
    shared = shared.replace('      M2_PANEL_PORT: "7788"\n',
                            '      M2_PANEL_PORT: "7788"\n      M2PANEL_ENGINE: mt2009\n')
    shared = shared.replace('      PLAYERBOTS_GAME_HOST: game\n',
                            '      PLAYERBOTS_GAME_HOST: game\n      PLAYERBOTS_ENGINE: mt2009\n')
    assert shared.count('M2PANEL_ENGINE') == 1 and shared.count('PLAYERBOTS_ENGINE') == 1
    # This line has its own version (linux-port-mt2009/VERSION, 2.x).
    shared = re.sub(r'PLAYERBOTS_VERSION: "\$\{M2_PLAYERBOTS_VERSION:-[0-9.]+\}"',
                    'PLAYERBOTS_VERSION: "${M2_PLAYERBOTS_VERSION:-' + version() + '}"', shared)
    # The updater: on this line an update is the package zip the manifest
    # names, unpacked over the server folder by linux-port/tools/update.sh -
    # never the 1.x git checkout, whose root VERSION is the 1.x line's and
    # whose installer staged that line over a 2.x server (l0st3k, 12 September).
    # The container is the 1.x image (sh, python3, docker compose) running our
    # script in watch mode; the "stack dir" is the server folder itself.
    updater_head = ('    profiles: ["update"]\n'
                    '    restart: unless-stopped\n'
                    '    init: true\n')
    assert shared.count(updater_head) == 1, shared.count(updater_head)
    shared = shared.replace(updater_head, updater_head +
        '    # The 2.x line updates from the package zip the manifest names - the same\n'
        '    # zip the Windows launcher installs - and not from a git checkout of the\n'
        '    # repository: the repository\'s root VERSION is the 1.x line\'s, and the 1.x\n'
        '    # updater staged that line over a 2.x server ("checkout z main melduje\n'
        '    # 1.33.3", rates stuck, no stalls - l0st3k, 12 September).\n'
        '    # linux-port/tools/update.sh in watch mode reads the request the panel\n'
        '    # writes and answers in the same update.status the panel reads.\n'
        '    entrypoint: ["/bin/sh", "${M2_UPDATE_STACK_DIR:-/opt/metin2}/linux-port/tools/update.sh"]\n'
        '    command: ["watch"]\n')
    old_env = '      M2_UPDATE_STACK_DIR: "${M2_UPDATE_STACK_DIR:-/opt/metin2/stack}"\n'
    assert shared.count(old_env) == 1
    shared = shared.replace(old_env,
        '      # On this line the "stack dir" is the server folder itself: the one\n'
        '      # with VERSION, CHANGELOG.md and linux-port/ in it.\n'
        '      M2_UPDATE_STACK_DIR: "${M2_UPDATE_STACK_DIR:-/opt/metin2}"\n')
    old_vol = '      - "${M2_UPDATE_STACK_DIR:-/opt/metin2/stack}:${M2_UPDATE_STACK_DIR:-/opt/metin2/stack}"\n'
    assert shared.count(old_vol) == 1
    shared = shared.replace(old_vol,
        '      - "${M2_UPDATE_STACK_DIR:-/opt/metin2}:${M2_UPDATE_STACK_DIR:-/opt/metin2}"\n')
    # The panel wants the migrator done, like the game.
    rendered = BEGIN + shared.rstrip('\n') + '\n\n' + END

    dst = io.open(DST, encoding='utf-8', newline='').read()
    if BEGIN in dst:
        a = dst.index(BEGIN)
        b = dst.index(END) + len(END)
        dst = dst[:a] + rendered + dst[b:]
    else:
        anchor = 'volumes:\n  db-data:\n'
        assert dst.count(anchor) == 1
        dst = dst.replace(anchor, rendered + '\n' + anchor)
    for vol in ('panel-conf', 'panel-data', 'update-spool'):
        line = '  %s:\n' % vol
        if line not in dst:
            dst = dst.replace('volumes:\n  db-data:\n', 'volumes:\n  db-data:\n' + line)
    io.open(DST, 'w', encoding='utf-8', newline='').write(dst)
    print('composify: %d shared service lines rendered into %s' % (rendered.count('\n'), os.path.relpath(DST)))

    deploy, n = re.subn(r'context: \.\./\.\./linux-port/docker/(panel|seban-panel|itemshop|updater)\b',
                        r'context: ./\1', dst)
    assert n == 4, n
    head = ('# Rendered by linux-port-mt2009/port/composify.py from docker-compose.yml for\n'
            '# the deployed tree, where this directory is linux-port/docker and the shared\n'
            '# contexts sit beside it. DO NOT EDIT; edit docker-compose.yml and re-run.\n')
    io.open(DEPLOY, 'w', encoding='utf-8', newline='').write(head + deploy)
    print('composify: deploy layout rendered into %s' % os.path.relpath(DEPLOY))


def version():
    p = os.path.normpath(os.path.join(HERE, '..', 'VERSION'))
    return io.open(p, encoding='utf-8').read().strip()


if __name__ == '__main__':
    main()
