# -*- coding: utf-8 -*-
"""The log tables the mt2009 engine writes and the package's log.sql lacks.

Usage:  python logschemify.py

The package ships fourteen log tables; its game core (game/src/log.cpp) writes
twenty-four, and log.log itself lacks the `ip` column every ITEM/CHARACTER
line carries. Every missing write is one syserr line per event - thousands a
minute with a thousand bots refining and trading - and the gear history the
panels read off log.log never gets written at all.

Writes docker/mariadb/playerbot/log_schema.sql: CREATE TABLE IF NOT EXISTS
for each missing table (definitions taken from the r40250 dump where that
lineage has the table, written from the engine's own INSERT for the rest) and
the ALTERs log.log needs. Applied by initdb on a fresh world and by the
migrator on every start; both are no-ops once the schema is there.
"""
import io
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
R40250_LOG = os.path.normpath(os.path.join(HERE, '..', '..', 'linux-port', 'docker', 'mariadb', 'initdb.d', 'dumps', 'log.sql'))
OUT = os.path.normpath(os.path.join(HERE, '..', 'docker', 'mariadb', 'playerbot', 'log_schema.sql'))
INITDB_COPY = os.path.normpath(os.path.join(HERE, '..', 'docker', 'mariadb', 'initdb.d', '20-log-schema.sql'))

# Tables r40250's dump defines with the columns this engine's INSERTs use.
FROM_R40250 = ['bootlog', 'command_log', 'cube', 'dragon_slay_log', 'goldlog',
               'levellog', 'loginlog2', 'money_log', 'quest_reward_log', 'refinelog', 'speed_hack']

# Tables only this engine writes (log.cpp), columns in the INSERT's own order.
OWN = {
    # fish_log was taken from r40250's dump until 2.0.39, and the two engines do
    # not agree about it: r40250 writes eight columns (map_index, fishing_level,
    # waiting_time, success, size) and this one writes six -
    # FishLog(playerId, itemVnum, count, rodLevel, baitVnum). Every catch on a
    # 2.x world therefore failed with "Column count doesn't match value count"
    # (errno 1136), one syserr line per fish. Nobody had seen it because nobody
    # fished: the rod's own LIMIT_LEVEL was fifty and the bots stopped short of
    # it. Lowering that to thirty is what finally made the fish bite - and the
    # error appear 364 times in the first ten minutes.
    'fish_log': """CREATE TABLE IF NOT EXISTS `fish_log` (
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `player_id` int(10) unsigned NOT NULL DEFAULT 0,
  `item_vnum` int(10) unsigned NOT NULL DEFAULT 0,
  `count` int(11) NOT NULL DEFAULT 0,
  `rod_level` int(11) NOT NULL DEFAULT 0,
  `bait_vnum` int(10) unsigned NOT NULL DEFAULT 0,
  KEY `player_id_idx` (`player_id`),
  KEY `time_idx` (`time`)
) ENGINE=InnoDB;""",
    'loginlog': """CREATE TABLE IF NOT EXISTS `loginlog` (
  `type` varchar(10) NOT NULL DEFAULT 'LOGIN',
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `channel` int(11) NOT NULL DEFAULT 0,
  `account_id` int(10) unsigned NOT NULL DEFAULT 0,
  `pid` int(10) unsigned NOT NULL DEFAULT 0,
  `mapIndex` int(11) NOT NULL DEFAULT 0,
  `x` int(11) NOT NULL DEFAULT 0,
  `y` int(11) NOT NULL DEFAULT 0,
  `playtime` int(11) NOT NULL DEFAULT 0,
  `ip` varchar(20) DEFAULT NULL,
  `hwid` varchar(255) DEFAULT NULL,
  `info` varchar(255) DEFAULT NULL,
  KEY `pid_idx` (`pid`),
  KEY `time_idx` (`time`)
) ENGINE=InnoDB;""",
    'exchange_log': """CREATE TABLE IF NOT EXISTS `exchange_log` (
  `type` varchar(20) NOT NULL DEFAULT '',
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `who` int(10) unsigned NOT NULL DEFAULT 0,
  `target` int(10) unsigned NOT NULL DEFAULT 0,
  `itemid` int(10) unsigned NOT NULL DEFAULT 0,
  `vnum` int(10) unsigned NOT NULL DEFAULT 0,
  `count` bigint(20) NOT NULL DEFAULT 0,
  `hint` varchar(255) DEFAULT NULL,
  KEY `who_idx` (`who`)
) ENGINE=InnoDB;""",
    'item_meta_log': """CREATE TABLE IF NOT EXISTS `item_meta_log` (
  `itemid` int(10) unsigned NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `who` int(10) unsigned NOT NULL DEFAULT 0,
  `what` varchar(50) NOT NULL DEFAULT '',
  `value` varchar(255) DEFAULT NULL,
  `vnum` int(10) unsigned NOT NULL DEFAULT 0,
  KEY `itemid_idx` (`itemid`)
) ENGINE=InnoDB;""",
    'request_info_log': """CREATE TABLE IF NOT EXISTS `request_info_log` (
  `pid` int(10) unsigned NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `type` int(10) unsigned NOT NULL DEFAULT 0,
  `arg1` varchar(255) DEFAULT NULL,
  `arg2` varchar(255) DEFAULT NULL
) ENGINE=InnoDB;""",
    'quest_state_log': """CREATE TABLE IF NOT EXISTS `quest_state_log` (
  `quest_name` varchar(64) NOT NULL DEFAULT '',
  `quest_state` varchar(64) NOT NULL DEFAULT '',
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `pid` int(10) unsigned NOT NULL DEFAULT 0
) ENGINE=InnoDB;""",
    'chat_log': """CREATE TABLE IF NOT EXISTS `chat_log` (
  `where` int(10) unsigned NOT NULL DEFAULT 0,
  `who_id` int(10) unsigned NOT NULL DEFAULT 0,
  `who_name` varchar(24) NOT NULL DEFAULT '',
  `whom_id` int(10) unsigned NOT NULL DEFAULT 0,
  `whom_name` varchar(24) NOT NULL DEFAULT '',
  `type` varchar(20) NOT NULL DEFAULT '',
  `msg` text DEFAULT NULL,
  `when` datetime NOT NULL DEFAULT current_timestamp(),
  `ip` varchar(20) DEFAULT NULL
) ENGINE=InnoDB;""",
    'captcha_log': """CREATE TABLE IF NOT EXISTS `captcha_log` (
  `pid` int(10) unsigned NOT NULL DEFAULT 0,
  `invoker_pid` int(10) unsigned NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `action` varchar(32) NOT NULL DEFAULT '',
  `info` varchar(255) DEFAULT NULL,
  `left_duration` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB;""",
    'voucher_code_log': """CREATE TABLE IF NOT EXISTS `voucher_code_log` (
  `code` varchar(64) NOT NULL DEFAULT '',
  `pid` int(10) unsigned NOT NULL DEFAULT 0,
  `account_id` int(10) unsigned NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp()
) ENGINE=InnoDB;""",
    'gold_session_log': """CREATE TABLE IF NOT EXISTS `gold_session_log` (
  `pid` int(10) unsigned NOT NULL DEFAULT 0,
  `gold` bigint(20) NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `duration` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB;""",
    'report_player_log': """CREATE TABLE IF NOT EXISTS `report_player_log` (
  `who` int(10) unsigned NOT NULL DEFAULT 0,
  `target` int(10) unsigned NOT NULL DEFAULT 0,
  `target_name` varchar(24) NOT NULL DEFAULT '',
  `reason` varchar(255) DEFAULT NULL,
  `time` datetime NOT NULL DEFAULT current_timestamp()
) ENGINE=InnoDB;""",
    'acce': """CREATE TABLE IF NOT EXISTS `acce` (
  `pid` int(10) unsigned NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `x` int(11) NOT NULL DEFAULT 0,
  `y` int(11) NOT NULL DEFAULT 0,
  `item_vnum` int(10) unsigned NOT NULL DEFAULT 0,
  `item_uid` int(10) unsigned NOT NULL DEFAULT 0,
  `item_count` int(11) NOT NULL DEFAULT 0,
  `item_abs_chance` int(11) NOT NULL DEFAULT 0,
  `success` tinyint(4) NOT NULL DEFAULT 0
) ENGINE=InnoDB;""",
    # Written by the game's CItemShopManager::BuyItem (itemshop_manager.cpp) for
    # every purchase in the in-game ItemShop, a bot's included since 2.0.60:
    # INSERT INTO itemshop VALUES (pid, aid, item_index, vnum, quantity, price,
    # currency, item_id, NOW(), money_before). No dump in the package defines
    # it (its itemshop_log is another shape nothing writes), so every purchase
    # logged "Table 'log.itemshop' doesn't exist". item_id arrives as the
    # literal null when the goods went to item_award.
    'itemshop': """CREATE TABLE IF NOT EXISTS `itemshop` (
  `pid` int(10) unsigned NOT NULL DEFAULT 0,
  `aid` int(10) unsigned NOT NULL DEFAULT 0,
  `item_index` int(11) NOT NULL DEFAULT 0,
  `vnum` int(10) unsigned NOT NULL DEFAULT 0,
  `quantity` int(11) NOT NULL DEFAULT 0,
  `price` bigint(20) NOT NULL DEFAULT 0,
  `currency` tinyint(4) NOT NULL DEFAULT 0,
  `item_id` int(10) unsigned DEFAULT NULL,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `money_before` int(10) unsigned NOT NULL DEFAULT 0
) ENGINE=InnoDB;""",
    # Written by the package's own itemshop_manage quest (object/8001[4-6]/use):
    # INSERT INTO itemshop_dragon_scroll VALUES (pid, aid, NOW(), id, value).
    # No dump in the package defines it, so every purchase logged
    # "Table 'log.itemshop_dragon_scroll' doesn't exist" (l0st3k, sizowski).
    'itemshop_dragon_scroll': """CREATE TABLE IF NOT EXISTS `itemshop_dragon_scroll` (
  `pid` int(10) unsigned NOT NULL DEFAULT 0,
  `aid` int(10) unsigned NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `id` int(11) NOT NULL DEFAULT 0,
  `value` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB;""",
    # Written by the db core's IkarusShopLog (db/src/ClientManagerIkarusShop.cpp)
    # for every offline-shop action - CREATE_SHOP, a sale, a withdrawal. No
    # dump in the package defines it, so a world logged "Table
    # 'log.ikarusshop_log' doesn't exist" for each one (sizowski, 12
    # September: 92 in a bundle). Columns in the INSERT's own order; `cheque`
    # is only written under ENABLE_CHEQUE_SYSTEM and harmless otherwise.
    'ikarusshop_log': """CREATE TABLE IF NOT EXISTS `ikarusshop_log` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `who` int(10) unsigned NOT NULL DEFAULT 0,
  `itemid` int(10) unsigned NOT NULL DEFAULT 0,
  `what` varchar(32) NOT NULL DEFAULT '',
  `shop_owner` int(10) unsigned NOT NULL DEFAULT 0,
  `extra` varchar(255) NOT NULL DEFAULT '',
  `vnum` int(10) unsigned NOT NULL DEFAULT 0,
  `count` int(10) unsigned NOT NULL DEFAULT 0,
  `yang` bigint(20) NOT NULL DEFAULT 0,
  `cheque` int(11) NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`id`),
  KEY `who_idx` (`who`),
  KEY `shop_owner_idx` (`shop_owner`)
) ENGINE=InnoDB;""",
}


def r40250_create(sql, table):
    m = re.search(r"CREATE TABLE `%s` \((.*?)\n\) ENGINE=[^;]*;" % re.escape(table), sql, re.S)
    if not m:
        raise SystemExit('logschemify: %s not in %s' % (table, R40250_LOG))
    body = m.group(1)
    # MariaDB 11 refuses the zero date under its defaults; NOW() is what the
    # engine writes anyway.
    body = body.replace("DEFAULT '0000-00-00 00:00:00'", 'DEFAULT current_timestamp()')
    body = body.replace("DEFAULT '0000-00-00'", 'DEFAULT current_date()')
    body = body.replace("DEFAULT '00:00:00'", "DEFAULT '00:00:00'")
    return 'CREATE TABLE IF NOT EXISTS `%s` (%s\n) ENGINE=InnoDB;' % (table, body)


def main():
    sql = io.open(R40250_LOG, encoding='latin-1', newline='').read().replace('\r\n', '\n')
    parts = ['-- Rendered by linux-port-mt2009/port/logschemify.py. DO NOT EDIT.',
             '-- The log tables game/src/log.cpp writes that the package dump lacks,',
             '-- and the columns log.log is missing. Idempotent.',
             'USE log;', '']
    parts.append("ALTER TABLE `log` ADD COLUMN IF NOT EXISTS `ip` varbinary(20) DEFAULT NULL AFTER `hint`;")
    parts.append("ALTER TABLE `log` ADD INDEX IF NOT EXISTS `who_idx` (`who`);")
    parts.append("ALTER TABLE `log` ADD INDEX IF NOT EXISTS `what_idx` (`what`);")
    parts.append("ALTER TABLE `log` ADD INDEX IF NOT EXISTS `how_idx` (`how`);")
    parts.append('')
    for t in FROM_R40250:
        parts.append(r40250_create(sql, t))
        parts.append('')
    # loginlog2 gained its hwid column after the first worlds were made, and
    # CREATE TABLE IF NOT EXISTS never adds a column to a table that exists:
    # "Unknown column 'hwid' in 'INSERT INTO'" on every login of such a world.
    parts.append("ALTER TABLE `loginlog2` ADD COLUMN IF NOT EXISTS `hwid` varchar(255) DEFAULT NULL;")
    parts.append('')
    # hack_log came with the package's dump as time, name, server and why,
    # while LogManager::HackLog writes login and ip as well - so every line it
    # tried failed with "Unknown column 'login' in 'INSERT INTO'" and the
    # table never held a row (a few hundred a day on the test world, each a
    # bot's FAST_ITEM_SWAP). The two columns go where r40250's table has them,
    # and name widens to CHARACTER_NAME_MAX_LEN, which the dump's sixteen bytes
    # were short of. ADD COLUMN IF NOT EXISTS for a world made before this;
    # the MODIFY is a no-op on a table already that wide.
    parts.append("ALTER TABLE `hack_log` ADD COLUMN IF NOT EXISTS `login` varbinary(30) DEFAULT NULL AFTER `time`;")
    parts.append("ALTER TABLE `hack_log` ADD COLUMN IF NOT EXISTS `ip` varbinary(20) DEFAULT NULL AFTER `name`;")
    parts.append("ALTER TABLE `hack_log` MODIFY COLUMN IF EXISTS `name` varbinary(24) DEFAULT NULL;")
    parts.append('')
    # refinelog.setType says how a refine was made, and the classic panel puts
    # it in brackets in a character's gear history. The dump's SET held SCROLL
    # for every scroll and dropped the three longer names mt2009 writes, and
    # playerbotify's apply_refine_log_way now writes DEVILTOWER for the Demon
    # Tower smith and SCROLL:<vnum> for a scroll; a varchar holds all of them
    # and keeps what the SET held. The index is for the history's lookup by
    # character and time on a table of a few hundred thousand rows.
    parts.append("ALTER TABLE `refinelog` MODIFY COLUMN IF EXISTS `setType` varchar(40) DEFAULT NULL;")
    parts.append("ALTER TABLE `refinelog` ADD INDEX IF NOT EXISTS `pid_time_idx` (`pid`, `time`);")
    parts.append('')
    for t, ddl in OWN.items():
        parts.append(ddl)
        parts.append('')
    out = '\n'.join(parts)
    # Two copies: initdb.d runs it on a fresh world (the official image runs
    # every *.sql there, after 10-import-dumps.sh by name), the migrator runs
    # it on every start for a world made before it existed.
    for path in (OUT, INITDB_COPY):
        io.open(path, 'w', encoding='latin-1', newline='').write(out)
        print('logschemify: %s (%d tables)' % (os.path.relpath(path), len(FROM_R40250) + len(OWN)))


if __name__ == '__main__':
    main()
