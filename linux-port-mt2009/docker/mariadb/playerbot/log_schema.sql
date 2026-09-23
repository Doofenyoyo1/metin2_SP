-- Rendered by linux-port-mt2009/port/logschemify.py. DO NOT EDIT.
-- The log tables game/src/log.cpp writes that the package dump lacks,
-- and the columns log.log is missing. Idempotent.
USE log;

ALTER TABLE `log` ADD COLUMN IF NOT EXISTS `ip` varbinary(20) DEFAULT NULL AFTER `hint`;
ALTER TABLE `log` ADD INDEX IF NOT EXISTS `who_idx` (`who`);
ALTER TABLE `log` ADD INDEX IF NOT EXISTS `what_idx` (`what`);
ALTER TABLE `log` ADD INDEX IF NOT EXISTS `how_idx` (`how`);

CREATE TABLE IF NOT EXISTS `bootlog` (
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `hostname` char(128) NOT NULL DEFAULT 'UNKNOWN',
  `channel` tinyint(1) NOT NULL DEFAULT 0
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `command_log` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `userid` int(11) NOT NULL DEFAULT 0,
  `server` int(11) NOT NULL DEFAULT 0,
  `ip` varchar(15) NOT NULL DEFAULT '',
  `port` int(6) NOT NULL DEFAULT 0,
  `username` varchar(50) NOT NULL DEFAULT '',
  `command` text NOT NULL,
  `date` datetime NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`id`)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `cube` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `pid` int(11) unsigned NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `x` int(11) unsigned NOT NULL DEFAULT 0,
  `y` int(11) unsigned NOT NULL DEFAULT 0,
  `item_vnum` int(11) unsigned NOT NULL DEFAULT 0,
  `item_uid` int(11) unsigned NOT NULL DEFAULT 0,
  `item_count` int(5) unsigned NOT NULL DEFAULT 0,
  `success` tinyint(1) NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  KEY `pid` (`pid`) USING BTREE,
  KEY `item_vnum` (`item_vnum`) USING BTREE,
  KEY `item_uid` (`item_uid`) USING BTREE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `dragon_slay_log` (
  `guild_id` int(11) unsigned NOT NULL,
  `vnum` int(11) unsigned NOT NULL,
  `start_time` timestamp NOT NULL DEFAULT current_timestamp(),
  `end_time` timestamp NOT NULL DEFAULT current_timestamp()
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `goldlog` (
  `date` varchar(10) NOT NULL DEFAULT current_date(),
  `time` varchar(8) NOT NULL DEFAULT '00:00:00',
  `pid` int(10) unsigned NOT NULL DEFAULT 0,
  `what` int(11) NOT NULL DEFAULT 0,
  `how` set('BUY','SELL','SHOP_SELL','SHOP_BUY','EXCHANGE_TAKE','EXCHANGE_GIVE','QUEST') DEFAULT NULL,
  `hint` varchar(50) DEFAULT NULL,
  KEY `date_idx` (`date`) USING BTREE,
  KEY `pid_idx` (`pid`) USING BTREE,
  KEY `what_idx` (`what`) USING BTREE,
  KEY `how_idx` (`how`) USING BTREE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `levellog` (
  `name` char(24) NOT NULL DEFAULT '',
  `level` tinyint(4) NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `playtime` int(11) NOT NULL DEFAULT 0,
  `account_id` int(11) NOT NULL,
  `pid` int(11) NOT NULL,
  PRIMARY KEY (`name`,`level`)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `loginlog2` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `type` text DEFAULT NULL,
  `is_gm` int(11) DEFAULT NULL,
  `login_time` datetime DEFAULT NULL,
  `channel` int(11) DEFAULT NULL,
  `account_id` int(11) DEFAULT NULL,
  `pid` int(11) DEFAULT NULL,
  `client_version` text DEFAULT NULL,
  `ip` text DEFAULT NULL,
  `logout_time` datetime DEFAULT NULL,
  `playtime` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `money_log` (
  `time` datetime DEFAULT NULL,
  `type` enum('MONSTER','SHOP','REFINE','QUEST','GUILD','MISC','KILL','DROP') DEFAULT NULL,
  `vnum` int(11) NOT NULL DEFAULT 0,
  `gold` int(11) NOT NULL DEFAULT 0,
  KEY `type` (`type`,`vnum`) USING BTREE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `quest_reward_log` (
  `quest_name` varchar(32) DEFAULT NULL,
  `player_id` int(10) unsigned DEFAULT NULL,
  `player_level` tinyint(4) DEFAULT NULL,
  `reward_type` enum('EXP','ITEM') DEFAULT NULL,
  `reward_value1` int(10) unsigned DEFAULT NULL,
  `reward_value2` int(11) DEFAULT NULL,
  `time` datetime DEFAULT NULL,
  KEY `player_id` (`player_id`) USING BTREE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `refinelog` (
  `pid` int(10) unsigned DEFAULT NULL,
  `item_name` varchar(24) NOT NULL DEFAULT '',
  `item_id` int(11) NOT NULL DEFAULT 0,
  `step` varchar(50) NOT NULL DEFAULT '',
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `is_success` tinyint(1) NOT NULL DEFAULT 0,
  `setType` set('SOCKET','POWER','ROD','GUILD','SCROLL','HYUNIRON','GOD_SCROLL','MUSIN_SCROLL') DEFAULT NULL
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `speed_hack` (
  `pid` int(11) DEFAULT NULL,
  `time` datetime DEFAULT NULL,
  `x` int(11) DEFAULT NULL,
  `y` int(11) DEFAULT NULL,
  `hack_count` varchar(20) CHARACTER SET big5 COLLATE big5_bin DEFAULT NULL
) ENGINE=InnoDB;

ALTER TABLE `loginlog2` ADD COLUMN IF NOT EXISTS `hwid` varchar(255) DEFAULT NULL;

ALTER TABLE `hack_log` ADD COLUMN IF NOT EXISTS `login` varbinary(30) DEFAULT NULL AFTER `time`;
ALTER TABLE `hack_log` ADD COLUMN IF NOT EXISTS `ip` varbinary(20) DEFAULT NULL AFTER `name`;
ALTER TABLE `hack_log` MODIFY COLUMN IF EXISTS `name` varbinary(24) DEFAULT NULL;

ALTER TABLE `refinelog` MODIFY COLUMN IF EXISTS `setType` varchar(40) DEFAULT NULL;
ALTER TABLE `refinelog` ADD INDEX IF NOT EXISTS `pid_time_idx` (`pid`, `time`);

CREATE TABLE IF NOT EXISTS `fish_log` (
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `player_id` int(10) unsigned NOT NULL DEFAULT 0,
  `item_vnum` int(10) unsigned NOT NULL DEFAULT 0,
  `count` int(11) NOT NULL DEFAULT 0,
  `rod_level` int(11) NOT NULL DEFAULT 0,
  `bait_vnum` int(10) unsigned NOT NULL DEFAULT 0,
  KEY `player_id_idx` (`player_id`),
  KEY `time_idx` (`time`)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `loginlog` (
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
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `exchange_log` (
  `type` varchar(20) NOT NULL DEFAULT '',
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `who` int(10) unsigned NOT NULL DEFAULT 0,
  `target` int(10) unsigned NOT NULL DEFAULT 0,
  `itemid` int(10) unsigned NOT NULL DEFAULT 0,
  `vnum` int(10) unsigned NOT NULL DEFAULT 0,
  `count` bigint(20) NOT NULL DEFAULT 0,
  `hint` varchar(255) DEFAULT NULL,
  KEY `who_idx` (`who`)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `item_meta_log` (
  `itemid` int(10) unsigned NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `who` int(10) unsigned NOT NULL DEFAULT 0,
  `what` varchar(50) NOT NULL DEFAULT '',
  `value` varchar(255) DEFAULT NULL,
  `vnum` int(10) unsigned NOT NULL DEFAULT 0,
  KEY `itemid_idx` (`itemid`)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `request_info_log` (
  `pid` int(10) unsigned NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `type` int(10) unsigned NOT NULL DEFAULT 0,
  `arg1` varchar(255) DEFAULT NULL,
  `arg2` varchar(255) DEFAULT NULL
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `quest_state_log` (
  `quest_name` varchar(64) NOT NULL DEFAULT '',
  `quest_state` varchar(64) NOT NULL DEFAULT '',
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `pid` int(10) unsigned NOT NULL DEFAULT 0
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `chat_log` (
  `where` int(10) unsigned NOT NULL DEFAULT 0,
  `who_id` int(10) unsigned NOT NULL DEFAULT 0,
  `who_name` varchar(24) NOT NULL DEFAULT '',
  `whom_id` int(10) unsigned NOT NULL DEFAULT 0,
  `whom_name` varchar(24) NOT NULL DEFAULT '',
  `type` varchar(20) NOT NULL DEFAULT '',
  `msg` text DEFAULT NULL,
  `when` datetime NOT NULL DEFAULT current_timestamp(),
  `ip` varchar(20) DEFAULT NULL
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `captcha_log` (
  `pid` int(10) unsigned NOT NULL DEFAULT 0,
  `invoker_pid` int(10) unsigned NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `action` varchar(32) NOT NULL DEFAULT '',
  `info` varchar(255) DEFAULT NULL,
  `left_duration` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `voucher_code_log` (
  `code` varchar(64) NOT NULL DEFAULT '',
  `pid` int(10) unsigned NOT NULL DEFAULT 0,
  `account_id` int(10) unsigned NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp()
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `gold_session_log` (
  `pid` int(10) unsigned NOT NULL DEFAULT 0,
  `gold` bigint(20) NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `duration` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `report_player_log` (
  `who` int(10) unsigned NOT NULL DEFAULT 0,
  `target` int(10) unsigned NOT NULL DEFAULT 0,
  `target_name` varchar(24) NOT NULL DEFAULT '',
  `reason` varchar(255) DEFAULT NULL,
  `time` datetime NOT NULL DEFAULT current_timestamp()
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `acce` (
  `pid` int(10) unsigned NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `x` int(11) NOT NULL DEFAULT 0,
  `y` int(11) NOT NULL DEFAULT 0,
  `item_vnum` int(10) unsigned NOT NULL DEFAULT 0,
  `item_uid` int(10) unsigned NOT NULL DEFAULT 0,
  `item_count` int(11) NOT NULL DEFAULT 0,
  `item_abs_chance` int(11) NOT NULL DEFAULT 0,
  `success` tinyint(4) NOT NULL DEFAULT 0
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `itemshop` (
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
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `itemshop_dragon_scroll` (
  `pid` int(10) unsigned NOT NULL DEFAULT 0,
  `aid` int(10) unsigned NOT NULL DEFAULT 0,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `id` int(11) NOT NULL DEFAULT 0,
  `value` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `ikarusshop_log` (
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
) ENGINE=InnoDB;
