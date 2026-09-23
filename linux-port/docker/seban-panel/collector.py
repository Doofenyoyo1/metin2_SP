import os
import time
from datetime import datetime
from pathlib import Path

import pymysql

INTERVAL = int(os.environ.get("SEBAN_COLLECTOR_INTERVAL", "300"))
STATUS_GLOB = os.environ.get("PLAYERBOTS_STATUS_GLOB", "/opt/metin2/var/channel*/*/playerbot_status.tsv")


def connect():
    return pymysql.connect(host=os.environ.get("DB_HOST", "mariadb"), port=int(os.environ.get("DB_PORT", "3306")), user=os.environ["DB_USER"], password=os.environ["DB_PASSWORD"], charset="utf8mb4", autocommit=True)


def decode_cp1250(value):
    if isinstance(value, (bytes, bytearray)):
        return bytes(value).decode("cp1250", "replace")
    return value or ""


def check_plus9_refines(cur):
    """Gold "/b"-style server announcement when a real player (not a bot --
    they refine to +9 constantly, that would be pure spam) upgrades
    something to +9. Off by default, toggled from /manage
    ('announce_plus9_refines' in common.m2_switches). Queues one NOTICE
    command (see web_admin.quest) for the player who just refined -- they
    are guaranteed online, they just acted -- and that quest's own poll
    loop calls notice_all(), the same Lua function /b uses, next tick.
    The watermark ('plus9_announce_watermark') is a unix timestamp, not a
    formatted datetime: common.m2_switches.value is only VARCHAR(16)."""
    try:
        cur.execute("SELECT value FROM common.m2_switches WHERE name='announce_plus9_refines'")
    except pymysql.MySQLError:
        # No switch table on this database (init() could not create it):
        # nothing to announce, and no error line every cycle (Playerbots 2.0.55).
        return
    row = cur.fetchone()
    if not row or str(row[0]) != "1":
        return
    cur.execute("SELECT value FROM common.m2_switches WHERE name='plus9_announce_watermark'")
    row = cur.fetchone()
    last_time = datetime.fromtimestamp(int(row[0])) if row else datetime(2000, 1, 1)
    cur.execute("""SELECT l.time, l.hint, p.name FROM log.log l
      JOIN player.player p ON p.id = l.who
      WHERE l.how='REFINE SUCCESS' AND l.hint LIKE '%%+9' AND l.time > %s
        AND NOT (EXISTS (SELECT 1 FROM account.account ba WHERE ba.id=p.account_id AND LEFT(ba.login,10)='playerbot_') OR p.name LIKE 'bot%%')
      ORDER BY l.time ASC LIMIT 20""", (last_time,))
    events = cur.fetchall()
    newest = last_time
    for time_val, hint, name in events:
        message = f"{decode_cp1250(name) if isinstance(name, (bytes, bytearray)) else name} ulepszył {decode_cp1250(hint)}"[:250]
        cur.execute("INSERT INTO player.web_admin_queue (player_name,cmd,arg1,arg2) VALUES (%s,'NOTICE',%s,'')", (name, message))
        newest = time_val
    if events:
        cur.execute(
            "INSERT INTO common.m2_switches (name, value) VALUES ('plus9_announce_watermark', %s) "
            "ON DUPLICATE KEY UPDATE value = VALUES(value)",
            (str(int(newest.timestamp())),))


def host_metrics(previous=None):
    try:
        with open("/host/proc/stat") as f:
            parts = f.readline().split()[1:]
        total = sum(map(int, parts)); idle = int(parts[3]) + int(parts[4])
        with open("/host/proc/meminfo") as f:
            mem = {line.split(":")[0]: int(line.split()[1]) for line in f if ":" in line}
        total_mb = mem["MemTotal"] // 1024; used_mb = (mem["MemTotal"] - mem.get("MemAvailable", mem.get("MemFree", 0))) // 1024
        disk = os.statvfs("/hostfs")
        disk_total_mb = (disk.f_blocks * disk.f_frsize) // (1024 * 1024)
        disk_used_mb = ((disk.f_blocks - disk.f_bavail) * disk.f_frsize) // (1024 * 1024)
        if previous:
            cpu = round(100 * (1 - (idle - previous[1]) / max(1, total - previous[0])), 1)
        else:
            with open("/host/proc/loadavg") as f:
                load_1m = float(f.read().split()[0])
            with open("/host/proc/cpuinfo") as f:
                cpu_count = max(1, sum(1 for line in f if line.startswith("processor")))
            cpu = round(min(100, 100 * load_1m / cpu_count), 1)
        return (total, idle), cpu, used_mb, total_mb, disk_used_mb, disk_total_mb
    except (OSError, KeyError, ValueError):
        return previous, 0, 0, 0, 0, 0


def init(cur):
    # The switches /manage writes (rankings with real players, the +9
    # announcements, the starter chest) live in common.m2_switches, a table
    # Seban's own VPS scripts made and a stock Playerbots install never had:
    # 1.54.1's rankings, dashboard and /manage answered 500 without it. Made
    # here like the panel's other tables (Playerbots 2.0.55); an existing one
    # is left as it is.
    cur.execute("""CREATE TABLE IF NOT EXISTS common.m2_switches (
      name VARCHAR(64) NOT NULL PRIMARY KEY,
      value VARCHAR(16) NOT NULL DEFAULT '0'
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4""")
    cur.execute("""CREATE TABLE IF NOT EXISTS player.web_admin_queue (
      id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
      player_name VARCHAR(24) NOT NULL, cmd VARCHAR(32) NOT NULL,
      arg1 VARCHAR(255) NOT NULL DEFAULT '', arg2 VARCHAR(255) NOT NULL DEFAULT '',
      status VARCHAR(24) NOT NULL DEFAULT 'pending',
      created DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
      updated DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
      KEY pending (status, created), KEY player_status (player_name, status)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4""")
    cur.execute("""CREATE TABLE IF NOT EXISTS player.web_seban_settings (
      name VARCHAR(64) NOT NULL PRIMARY KEY, value VARCHAR(255) NOT NULL,
      updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP) ENGINE=InnoDB""")
    cur.execute("""INSERT IGNORE INTO player.web_seban_settings (name,value) VALUES
      ('panel_name','Metin2 Singleplayer'),('stuck_minutes','5'),('theme','ocean'),('monitor_mode','vps'),
      ('setup_complete','1'),('auth_enabled','0'),('auth_password_hash','')""")
    # One-time branding migration for deployments created before the public-ready build.
    cur.execute("UPDATE player.web_seban_settings SET value='Metin2 Singleplayer' WHERE name='panel_name' AND value='Mt2009'")
    # Single-player suite: no setup wizard, no passphrase - one player at their
    # own machine (Tieru, 13 September). Skip the wizard for installs seeded
    # before this; an operator can still turn auth on from the panel.
    cur.execute("UPDATE player.web_seban_settings SET value='1' WHERE name='setup_complete' AND value='0'")
    # socket0 added 2026-09-13 so a generic Skill Book (vnum 50300 -- the
    # actual skill lives only in socket0, see resolve_item_display_name in
    # app.py) shows up as distinct rows instead of one lump sum. Disposable
    # monitoring history, not player data, so a schema change here drops and
    # recreates rather than an in-place ALTER of the primary key.
    # SHOW COLUMNS on a table that is not there is an error (1146), not an
    # empty answer: on an install that never had the table the check threw
    # before the CREATE and the page reading it answered 500. information_schema
    # counts zero for a missing table instead (Playerbots 2.0.47, kept in 2.0.49).
    # A row or none, never a column read by position: app.py calls init()
    # at start with its DictCursor, where [0] is a KeyError (2.0.49).
    cur.execute("""SELECT 1 FROM information_schema.columns WHERE table_schema='player'
      AND table_name='web_seban_item_snapshot' AND column_name='socket0' LIMIT 1""")
    if cur.fetchone() is None:
        cur.execute("DROP TABLE IF EXISTS player.web_seban_item_snapshot")
    cur.execute("""CREATE TABLE IF NOT EXISTS player.web_seban_item_snapshot (
      captured_at DATETIME NOT NULL, vnum INT UNSIGNED NOT NULL, socket0 INT NOT NULL DEFAULT 0,
      amount BIGINT UNSIGNED NOT NULL,
      PRIMARY KEY(captured_at,vnum,socket0), KEY(vnum,captured_at)) ENGINE=InnoDB""")
    cur.execute("""CREATE TABLE IF NOT EXISTS player.web_seban_map_snapshot (
      captured_at DATETIME NOT NULL, map_index INT UNSIGNED NOT NULL, character_count INT UNSIGNED NOT NULL,
      PRIMARY KEY(captured_at,map_index), KEY(map_index,captured_at)) ENGINE=InnoDB""")
    cur.execute("""CREATE TABLE IF NOT EXISTS player.web_seban_system_snapshot (
      captured_at DATETIME NOT NULL PRIMARY KEY, cpu_percent DECIMAL(5,1) NOT NULL,
      ram_percent DECIMAL(5,1) NOT NULL, ram_used_mb INT UNSIGNED NOT NULL, ram_total_mb INT UNSIGNED NOT NULL,
      disk_percent DECIMAL(5,1) NOT NULL DEFAULT 0, disk_used_mb INT UNSIGNED NOT NULL DEFAULT 0,
      disk_total_mb INT UNSIGNED NOT NULL DEFAULT 0) ENGINE=InnoDB""")
    cur.execute("ALTER TABLE player.web_seban_system_snapshot ADD COLUMN IF NOT EXISTS disk_percent DECIMAL(5,1) NOT NULL DEFAULT 0")
    cur.execute("ALTER TABLE player.web_seban_system_snapshot ADD COLUMN IF NOT EXISTS disk_used_mb INT UNSIGNED NOT NULL DEFAULT 0")
    cur.execute("ALTER TABLE player.web_seban_system_snapshot ADD COLUMN IF NOT EXISTS disk_total_mb INT UNSIGNED NOT NULL DEFAULT 0")
    cur.execute("""CREATE TABLE IF NOT EXISTS player.web_seban_metric_snapshot (
      captured_at DATETIME NOT NULL, metric VARCHAR(64) NOT NULL, value BIGINT NOT NULL,
      PRIMARY KEY(captured_at,metric), KEY(metric,captured_at)) ENGINE=InnoDB""")
    cur.execute("""CREATE TABLE IF NOT EXISTS player.web_seban_bot_position_snapshot (
      captured_at DATETIME NOT NULL, pid INT UNSIGNED NOT NULL, map_index INT UNSIGNED NOT NULL,
      x INT NOT NULL, y INT NOT NULL, PRIMARY KEY(captured_at,pid), KEY(pid,captured_at)) ENGINE=InnoDB""")
    # Offline shops (IkarusShop "stragany"): player.ikashop_offlineshop is one
    # row per open shop, player.item WHERE window='IKASHOP_OFFLINESHOP' is one
    # row per listed offer, and the offer's price for its whole stack (not
    # per unit) lives in that item's own ikashop_data JSON column
    # ({"yang":N,...}) -- there is no separate price/listing table for this
    # engine's offline shops, confirmed against a live test shop.
    cur.execute("""CREATE TABLE IF NOT EXISTS player.web_seban_shop_snapshot (
      captured_at DATETIME NOT NULL, map_index INT UNSIGNED NOT NULL, empire TINYINT UNSIGNED NOT NULL,
      shop_count INT UNSIGNED NOT NULL, offer_count INT UNSIGNED NOT NULL,
      item_count BIGINT UNSIGNED NOT NULL, total_value BIGINT UNSIGNED NOT NULL,
      PRIMARY KEY(captured_at,map_index), KEY(map_index,captured_at)) ENGINE=InnoDB""")
    # Per-vnum history of what is offered in shops, so the shop page can show
    # a price/quantity trend and answer "was this item ever on the market"
    # for items with zero active offers right now -- web_seban_shop_snapshot
    # above only keeps the per-map/empire rollup, not individual vnums.
    # socket0 added 2026-09-13, same reason and same drop/recreate approach
    # as web_seban_item_snapshot above.
    # SHOW COLUMNS on a table that is not there is an error (1146), not an
    # empty answer: on an install that never had the table the check threw
    # before the CREATE and the page reading it answered 500. information_schema
    # counts zero for a missing table instead (Playerbots 2.0.47, kept in 2.0.49).
    # A row or none, never a column read by position: app.py calls init()
    # at start with its DictCursor, where [0] is a KeyError (2.0.49).
    cur.execute("""SELECT 1 FROM information_schema.columns WHERE table_schema='player'
      AND table_name='web_seban_shop_item_snapshot' AND column_name='socket0' LIMIT 1""")
    if cur.fetchone() is None:
        cur.execute("DROP TABLE IF EXISTS player.web_seban_shop_item_snapshot")
    cur.execute("""CREATE TABLE IF NOT EXISTS player.web_seban_shop_item_snapshot (
      captured_at DATETIME NOT NULL, vnum INT UNSIGNED NOT NULL, socket0 INT NOT NULL DEFAULT 0,
      offers INT UNSIGNED NOT NULL, total_units BIGINT UNSIGNED NOT NULL, total_value BIGINT UNSIGNED NOT NULL,
      PRIMARY KEY(captured_at,vnum,socket0), KEY(vnum,captured_at)) ENGINE=InnoDB""")


# playerbot_status.tsv, read by its header: Iwakura's personalities (2.0.85)
# put four columns (persona, mood, mood_lock, lock_level) before the status
# text, which stays last because it may hold spaces. A core of before that
# writes the old fourteen columns under a header too; with no header at all
# the old fourteen are assumed.
STATUS_LEGACY_COLUMNS = ("pid", "personality", "ambition", "role", "in_party", "goal", "action",
                         "updated_ms", "map", "x", "y", "hp", "max_hp", "status")
PERSONA_NONE = 255


def parse_status_rows(text):
    header = None
    for line in text.splitlines():
        if line.startswith("pid\t"):
            header = line.split("\t")
            continue
        columns = header or STATUS_LEGACY_COLUMNS
        values = line.split("\t", len(columns) - 1)
        if len(values) != len(columns) or columns[-1] != "status":
            continue
        try:
            numbers = {name: int(value) for name, value in zip(columns[:-1], values[:-1])}
        except ValueError:
            continue
        if "pid" in numbers:
            yield numbers, values[-1]


def live_positions():
    result = {}
    for path in Path("/").glob(STATUS_GLOB.lstrip("/")):
        try:
            if time.time() - path.stat().st_mtime > 25:
                continue
            for n, _status in parse_status_rows(path.read_text(encoding="cp1250", errors="replace")):
                result[n["pid"]] = (n.get("map", 0), n.get("x", 0), n.get("y", 0))
        except (OSError, ValueError):
            continue
    return result


def live_map_counts():
    counts = {}
    for index, _, _ in live_positions().values():
        counts[index] = counts.get(index, 0) + 1
    return counts


def collect(con, previous):
    now = datetime.now().replace(second=0, microsecond=0)
    previous, cpu, used, total, disk_used, disk_total = host_metrics(previous)
    ram = round(100 * used / total, 1) if total else 0
    disk_percent = round(100 * disk_used / disk_total, 1) if disk_total else 0
    with con.cursor() as cur:
        init(cur)
        cur.execute("""INSERT INTO player.web_seban_system_snapshot
          (captured_at,cpu_percent,ram_percent,ram_used_mb,ram_total_mb,disk_percent,disk_used_mb,disk_total_mb)
          VALUES (%s,%s,%s,%s,%s,%s,%s,%s)
          ON DUPLICATE KEY UPDATE cpu_percent=VALUES(cpu_percent), ram_percent=VALUES(ram_percent),
            ram_used_mb=VALUES(ram_used_mb), ram_total_mb=VALUES(ram_total_mb), disk_percent=VALUES(disk_percent),
            disk_used_mb=VALUES(disk_used_mb), disk_total_mb=VALUES(disk_total_mb)""",
            (now, cpu, ram, used, total, disk_percent, disk_used, disk_total))
        for map_index, count in live_map_counts().items():
            cur.execute("INSERT IGNORE INTO player.web_seban_map_snapshot VALUES (%s,%s,%s)", (now, map_index, count))
        for pid, (map_index, x, y) in live_positions().items():
            cur.execute("INSERT IGNORE INTO player.web_seban_bot_position_snapshot VALUES (%s,%s,%s,%s,%s)", (now, pid, map_index, x, y))
        # Split by socket0 only for vnum 50300 (the generic Skill Book -- see
        # resolve_item_display_name in app.py): splitting every socketed
        # item this way would fragment ordinary equipment into one row per
        # gem combination for no reason, since only 50300's socket0 changes
        # what the item actually *is*.
        cur.execute("""INSERT IGNORE INTO player.web_seban_item_snapshot (captured_at,vnum,socket0,amount)
          SELECT %s, vnum, IF(vnum=50300, socket0, 0), SUM(count)
          FROM player.item GROUP BY vnum, IF(vnum=50300, socket0, 0)""", (now,))
        cur.execute("""INSERT IGNORE INTO player.web_seban_shop_snapshot
          (captured_at, map_index, empire, shop_count, offer_count, item_count, total_value)
          SELECT %s, o.map, pi.empire, COUNT(DISTINCT o.owner), COUNT(i.id),
                 COALESCE(SUM(i.count),0),
                 COALESCE(SUM(CAST(JSON_UNQUOTE(JSON_EXTRACT(i.ikashop_data,'$.yang')) AS UNSIGNED)),0)
          FROM player.ikashop_offlineshop o
          JOIN player.player p ON p.id = o.owner
          JOIN player.player_index pi ON pi.id = p.account_id
          LEFT JOIN player.item i ON i.owner_id = o.owner AND i.window = 'IKASHOP_OFFLINESHOP'
          GROUP BY o.map, pi.empire""", (now,))
        cur.execute("""INSERT IGNORE INTO player.web_seban_shop_item_snapshot (captured_at, vnum, socket0, offers, total_units, total_value)
          SELECT %s, vnum, IF(vnum=50300, socket0, 0), COUNT(*), SUM(count),
                 COALESCE(SUM(CAST(JSON_UNQUOTE(JSON_EXTRACT(ikashop_data,'$.yang')) AS UNSIGNED)),0)
          FROM player.item WHERE window = 'IKASHOP_OFFLINESHOP' GROUP BY vnum, IF(vnum=50300, socket0, 0)""", (now,))
        cur.execute("SELECT COALESCE(SUM(gold),0) FROM player.player WHERE name NOT IN ('[SA]Admin','Test','Admin','AdminNinja','AdminSura','AdminSzaman')")
        yang = cur.fetchone()[0]
        cur.execute("INSERT IGNORE INTO player.web_seban_metric_snapshot VALUES (%s,'total_yang',%s)", (now, yang))
        check_plus9_refines(cur)
    return previous


def main():
    previous = None
    while True:
        try:
            with connect() as con:
                previous = collect(con, previous)
                print("[seban-collector] snapshot complete", flush=True)
            time.sleep(INTERVAL)
        except Exception as exc:
            # A failed attempt (most often: MariaDB not accepting connections
            # yet, moments after a fresh `docker compose up`) used to fall
            # through to the same full-length sleep as a success, so the
            # panel's tables -- and every page that reads them -- could stay
            # missing for a whole INTERVAL (default 300s) after a restart.
            # Retry soon instead of waiting out the normal cadence.
            print(f"[seban-collector] {exc}", flush=True)
            time.sleep(10)


if __name__ == "__main__":
    main()
