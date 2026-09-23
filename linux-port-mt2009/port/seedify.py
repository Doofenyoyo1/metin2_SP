# -*- coding: utf-8 -*-
"""Render the playerbot seed and the nickname pool for the mt2009 schema.

Usage:  python seedify.py

Reads linux-port/overlays/playerbot/sql/playerbots_seed.sql (the generator's
output, r40250 columns) and writes linux-port-mt2009/docker/mariadb/playerbot/
playerbots_seed.sql with the three things that schema does not have taken out:

  * account.account has no empire, name_checked or is_testor column - the
    kingdom lives in player_index.empire alone, which the seed writes anyway;
  * player.player has no bank_value.

Everything else the seed touches (player, player_index, item, quest,
common.playerbot_seed_state) has the same columns on both. playerbot_names.sql
needs nothing and is copied as it is. Idempotent; run after regenerating.
"""
import io
import os
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, '..', '..', 'linux-port', 'overlays', 'playerbot', 'sql'))
DST = os.path.normpath(os.path.join(HERE, '..', 'docker', 'mariadb', 'playerbot'))


def rewrite(sql):
    def rep(old, new):
        nonlocal sql
        n = sql.count(old)
        if n != 1:
            raise SystemExit('seedify: anchor found %d times:\n%s' % (n, old))
        sql = sql.replace(old, new)

    rep("        (login, password, social_id, email, create_time, is_testor, status,\n"
        "         empire, name_checked, availDt, last_play)\n"
        "    SELECT s.login, '!', s.social_id, '', UTC_TIMESTAMP(), 0, 'BLOCK',\n"
        "           s.empire, 1, UTC_TIMESTAMP(), UTC_TIMESTAMP()\n",
        "        (login, password, social_id, email, create_time, status,\n"
        "         availDt, last_play)\n"
        "    SELECT s.login, '!', s.social_id, '', UTC_TIMESTAMP(), 'BLOCK',\n"
        "           UTC_TIMESTAMP(), UTC_TIMESTAMP()\n")
    rep("         horse_level, horse_hp_droptime, horse_riding, horse_skill_point,\n"
        "         bank_value, last_play)\n"
        "    SELECT s.pid, a.id, s.player_name, s.job, 0, 0, s.x, s.y, 0, s.map_index,\n"
        "           s.x, s.y, s.map_index, s.hp, s.mp, 800, 1, 0,\n"
        "           s.st, s.ht, s.dx, s.iq, 0, 2500, 0, 0, 0,\n"
        "           0, 0, 0, 0, 0, 0, 0, 0, 0, UTC_TIMESTAMP()\n",
        "         horse_level, horse_hp_droptime, horse_riding, horse_skill_point,\n"
        "         last_play)\n"
        "    SELECT s.pid, a.id, s.player_name, s.job, 0, 0, s.x, s.y, 0, s.map_index,\n"
        "           s.x, s.y, s.map_index, s.hp, s.mp, 800, 1, 0,\n"
        "           s.st, s.ht, s.dx, s.iq, 0, 2500, 0, 0, 0,\n"
        "           0, 0, 0, 0, 0, 0, 0, 0, UTC_TIMESTAMP()\n")
    # The account has no empire column to keep in step with the index.
    rep("    UPDATE account.account AS a\n"
        "      JOIN player.player_index AS pi ON pi.id = a.id\n"
        "      JOIN playerbot_seed_spec AS s ON s.pid = pi.pid1\n"
        "       AND BINARY s.login = BINARY a.login\n"
        "       SET a.empire = s.empire\n"
        "     WHERE a.empire <> s.empire;\n",
        "    -- (mt2009: account.account carries no empire; player_index.empire is\n"
        "    --  the one the core reads and it is written above.)\n")
    # The social ids stay the generator's thirteen digits. The package's
    # account.social_id is varchar(7) - narrower than the engine's own
    # SOCIAL_ID_MAX_LEN of 18 - and under the server's non-strict sql_mode the
    # first run silently cut every id to '9000000' and the seed refused the
    # lot; initdb and the migrator widen the column to 18 instead.
    return ('-- Rendered for the mt2009 schema by linux-port-mt2009/port/seedify.py\n'
            '-- from linux-port/overlays/playerbot/sql/playerbots_seed.sql. DO NOT EDIT.\n'
            + sql)


def main():
    os.makedirs(DST, exist_ok=True)
    src = os.path.join(SRC, 'playerbots_seed.sql')
    sql = io.open(src, encoding='utf-8', newline='').read()
    out = rewrite(sql)
    dst = os.path.join(DST, 'playerbots_seed.sql')
    io.open(dst, 'w', encoding='utf-8', newline='').write(out)
    print('seedify: %s (%d bytes)' % (os.path.relpath(dst), len(out)))
    names_src = os.path.join(SRC, 'playerbot_names.sql')
    names_dst = os.path.join(DST, 'playerbot_names.sql')
    shutil.copyfile(names_src, names_dst)
    print('seedify: %s copied unchanged' % os.path.relpath(names_dst))


if __name__ == '__main__':
    main()
