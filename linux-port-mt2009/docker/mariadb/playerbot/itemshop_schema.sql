-- ItemShop (OskarPWA) - schemat i zasiew. Idempotentny: uruchamiany przy
-- kazdym starcie przez playerbot-migrate jako root. Kolumny wynikaja z tego,
-- co czyta PHP (app/itemshop/pages/*.php); paczka Oskara nie miala schematu.
CREATE DATABASE IF NOT EXISTS itemshop CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
GRANT ALL PRIVILEGES ON itemshop.* TO 'metin2'@'%';
FLUSH PRIVILEGES;

CREATE TABLE IF NOT EXISTS itemshop.ishop_category (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  name VARCHAR(64) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS itemshop.ishop_items (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  category INT NOT NULL,
  name_item VARCHAR(96) NOT NULL,
  `desc` TEXT,
  price INT NOT NULL DEFAULT 0,
  currency ENUM('cash','mileage') NOT NULL DEFAULT 'cash',
  vnum INT NOT NULL,
  count INT NOT NULL DEFAULT 1,
  socket0 INT NOT NULL DEFAULT 0,
  socket1 INT NOT NULL DEFAULT 0,
  socket2 INT NOT NULL DEFAULT 0,
  vnum_icon VARCHAR(16) NOT NULL,
  date_added DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  KEY category (category)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS itemshop.ishop_bundle_items (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  item_id INT NOT NULL,
  vnum INT NOT NULL,
  count INT NOT NULL DEFAULT 1,
  socket0 INT NOT NULL DEFAULT 0,
  socket1 INT NOT NULL DEFAULT 0,
  socket2 INT NOT NULL DEFAULT 0,
  KEY item_id (item_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS itemshop.ishop_log (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  buyer_name VARCHAR(30) NOT NULL,
  item_name VARCHAR(96) NOT NULL,
  date_of_buy DATETIME NOT NULL,
  vnum_icon VARCHAR(16) NOT NULL DEFAULT '',
  KEY buyer_name (buyer_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS itemshop.wheel_prizes (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  level TINYINT NOT NULL DEFAULT 1,
  is_jackpot TINYINT NOT NULL DEFAULT 0,
  name_item VARCHAR(96) NOT NULL,
  item_desc VARCHAR(255) NOT NULL DEFAULT '',
  vnum INT NOT NULL,
  vnum_icon VARCHAR(16) NOT NULL,
  count INT NOT NULL DEFAULT 1,
  weight INT NOT NULL DEFAULT 10,
  socket0 INT NOT NULL DEFAULT 0,
  socket1 INT NOT NULL DEFAULT 0,
  socket2 INT NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS itemshop.wheel_spin_log (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  buyer_name VARCHAR(30) NOT NULL,
  level TINYINT NOT NULL DEFAULT 1,
  prize_name VARCHAR(96) NOT NULL,
  vnum_icon VARCHAR(16) NOT NULL DEFAULT '',
  date_of_spin DATETIME NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Dostawa zakupow: rdzen db czyta player.item_award (ItemAwardManager).
CREATE TABLE IF NOT EXISTS player.item_award (
  id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  pid INT UNSIGNED NOT NULL DEFAULT 0,
  login VARCHAR(30) NOT NULL,
  vnum INT(6) UNSIGNED NOT NULL DEFAULT 0,
  count INT UNSIGNED NOT NULL DEFAULT 0,
  given_time DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00',
  taken_time DATETIME DEFAULT NULL,
  item_id INT DEFAULT NULL,
  why VARCHAR(128) DEFAULT NULL,
  socket0 INT NOT NULL DEFAULT 0,
  socket1 INT NOT NULL DEFAULT 0,
  socket2 INT NOT NULL DEFAULT 0,
  mall TINYINT(1) NOT NULL DEFAULT 0,
  KEY pid (pid), KEY given_time (given_time), KEY taken_time (taken_time)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- Zasiew: tylko gdy sklep jest pusty, zeby operator mogl go potem urzadzic
-- po swojemu bez walki z kazdym startem.
INSERT IGNORE INTO itemshop.ishop_category (id, name) VALUES
  (1, 'Ulepszanie'), (2, 'Bonusy'), (3, 'Kon i pomoc'), (4, 'Za Smocze Znaki');

INSERT INTO itemshop.ishop_items (id, category, name_item, `desc`, price, currency, vnum, count, vnum_icon)
SELECT * FROM (SELECT
   1 AS id, 1 AS category, 'Zwoj Blogoslawienstwa' AS name_item, 'Nieudane ulepszenie cofa przedmiot o poziom zamiast go niszczyc.' AS `desc`, 20 AS price, 'cash' AS currency, 25040 AS vnum, 1 AS count, '25040' AS vnum_icon
 UNION ALL SELECT  2, 1, 'Blogoslawienstwo Przedmiotu', 'Przedmiot nie znika przy nieudanym ulepszeniu.', 25, 'cash', 71052, 1, '71052'
 UNION ALL SELECT  3, 1, 'Podrecznik Kowala', 'Ulepszenie bez kowala, gdziekolwiek jestes.', 15, 'cash', 39007, 1, '39007'
 UNION ALL SELECT  4, 2, 'Zaczarowanie Przedmiotu', 'Losuje na nowo bonusy zbroi.', 30, 'cash', 71084, 1, '71084'
 UNION ALL SELECT  5, 2, 'Wzmocnienie Przedmiotu', 'Losuje na nowo bonusy broni.', 30, 'cash', 71085, 1, '71085'
 UNION ALL SELECT  6, 2, 'Atak Boga Smokow', '+10% obrazen przez godzine.', 10, 'cash', 71028, 1, '71028'
 UNION ALL SELECT  7, 2, 'Zycie Boga Smokow', '+20% HP przez godzine.', 10, 'cash', 71027, 1, '71027'
 UNION ALL SELECT  8, 2, 'Obrona Boga Smokow', '+10% obrony przez godzine.', 10, 'cash', 71030, 1, '71030'
 UNION ALL SELECT  9, 2, 'Peleryna Mestwa', 'Przyciaga potwory z okolicy.', 10, 'cash', 70038, 1, '70038'
 UNION ALL SELECT 10, 3, 'Siano x10', 'Pokarm dla konia.', 5, 'cash', 50054, 10, '50054'
 UNION ALL SELECT 11, 3, 'Marchewka x10', 'Pokarm dla konia.', 5, 'cash', 50055, 10, '50055'
 UNION ALL SELECT 12, 3, 'Medal Konny', 'Dla Stajennego: nastepny poziom konia.', 40, 'cash', 50050, 1, '50050'
 UNION ALL SELECT 13, 3, 'Rekawica Zlodzieja', 'Wiecej przedmiotow z potworow.', 15, 'cash', 70043, 1, '70043'
 UNION ALL SELECT 14, 3, 'Plaszcz Uciekiniera', 'Teleport do miasta.', 10, 'cash', 70048, 1, '70048'
 UNION ALL SELECT 15, 4, 'Niebieska Perla', 'Material do wysokich ulepszen.', 30, 'mileage', 27993, 1, '27993'
 UNION ALL SELECT 16, 4, 'Krwawa Perla', 'Material do wysokich ulepszen.', 50, 'mileage', 27994, 1, '27994'
 UNION ALL SELECT 17, 4, 'Wino z Kwiatu Brzoskwini x5', 'Napoj.', 8, 'mileage', 70020, 5, '70020'
 UNION ALL SELECT 18, 2, 'Ksiega Zapomnienia', 'Cofa jeden punkt wybranej umiejetnosci - dla umiejetnosci, ktora utknela na 17 po trzydziestym poziomie.', 30, 'cash', 70037, 1, '70037'
) AS seed
WHERE NOT EXISTS (SELECT 1 FROM itemshop.ishop_items);

-- Added after the first shops were seeded; a shop that already exists gets
-- it once, under the next free id, and never twice.
INSERT INTO itemshop.ishop_items (id, category, name_item, `desc`, price, currency, vnum, count, vnum_icon)
SELECT (SELECT COALESCE(MAX(id), 0) + 1 FROM (SELECT id FROM itemshop.ishop_items) AS ids), 2,
   'Ksiega Zapomnienia', 'Cofa jeden punkt wybranej umiejetnosci - dla umiejetnosci, ktora utknela na 17 po trzydziestym poziomie.', 30, 'cash', 70037, 1, '70037'
FROM DUAL
WHERE NOT EXISTS (SELECT 1 FROM itemshop.ishop_items WHERE vnum = 70037);

INSERT INTO itemshop.wheel_prizes (id, level, is_jackpot, name_item, item_desc, vnum, vnum_icon, count, weight)
SELECT * FROM (SELECT
   1 AS id, 1 AS level, 1 AS is_jackpot, 'Medal Konny' AS name_item, 'Glowna wygrana' AS item_desc, 50050 AS vnum, '50050' AS vnum_icon, 1 AS count, 1 AS weight
 UNION ALL SELECT  2, 1, 1, 'Krwawa Perla', 'Glowna wygrana', 27994, '27994', 1, 1
 UNION ALL SELECT  3, 1, 0, 'Zwoj Blogoslawienstwa', '', 25040, '25040', 1, 6
 UNION ALL SELECT  4, 1, 0, 'Blogoslawienstwo Przedmiotu', '', 71052, '71052', 1, 5
 UNION ALL SELECT  5, 1, 0, 'Podrecznik Kowala', '', 39007, '39007', 1, 8
 UNION ALL SELECT  6, 1, 0, 'Zaczarowanie Przedmiotu', '', 71084, '71084', 1, 4
 UNION ALL SELECT  7, 1, 0, 'Wzmocnienie Przedmiotu', '', 71085, '71085', 1, 4
 UNION ALL SELECT  8, 1, 0, 'Atak Boga Smokow', '', 71028, '71028', 1, 10
 UNION ALL SELECT  9, 1, 0, 'Zycie Boga Smokow', '', 71027, '71027', 1, 10
 UNION ALL SELECT 10, 1, 0, 'Obrona Boga Smokow', '', 71030, '71030', 1, 10
 UNION ALL SELECT 11, 1, 0, 'Peleryna Mestwa', '', 70038, '70038', 1, 10
 UNION ALL SELECT 12, 1, 0, 'Siano x10', '', 50054, '50054', 10, 12
 UNION ALL SELECT 13, 1, 0, 'Marchewka x10', '', 50055, '50055', 10, 12
 UNION ALL SELECT 14, 1, 0, 'Rekawica Zlodzieja', '', 70043, '70043', 1, 6
 UNION ALL SELECT 15, 1, 0, 'Plaszcz Uciekiniera', '', 70048, '70048', 1, 10
 UNION ALL SELECT 16, 1, 0, 'Niebieska Perla', '', 27993, '27993', 1, 3
 UNION ALL SELECT 17, 1, 0, 'Wino z Kwiatu Brzoskwini x5', '', 70020, '70020', 5, 12
) AS seed
WHERE NOT EXISTS (SELECT 1 FROM itemshop.wheel_prizes);
