"""MT2009_PLUS_DB_EDITOR_V1: python3 test_dbeditor_mobs.py

"Potwory i bossowie" (dbeditor/mobs.py) and "Respawn bossów i metinów"
(dbeditor/spawns.py, spawnfiles.py) through Flask's test client. The
database is SQLite in memory with the schemas "world" and "player" attached,
so the modules' own SQL runs (MariaDB-only bits translated below); the
spool is a temporary folder with the files m2-spawns publishes; the game
side (m2-spawns) is run on what the panel wrote."""
import datetime
import os
import re
import shutil
import sqlite3
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

os.environ.setdefault("DB_USER", "test")
os.environ.setdefault("DB_PASSWORD", "test")
os.environ.setdefault("DB_HOST", "127.0.0.1")
os.environ.setdefault("DB_PORT", "1")
SPOOL = Path(tempfile.mkdtemp(prefix="dbe-mobs-spool-"))
os.environ["DBEDITOR_SPOOL_ROOT"] = str(SPOOL)

import app as panel  # noqa: E402
from dbeditor import common_items, mobs  # noqa: E402
from dbeditor import spawnfiles as sf  # noqa: E402

HERE = Path(__file__).resolve().parent
M2_SPAWNS = HERE.parent / "game" / "bin" / "m2-spawns"
APPLY_SH = HERE.parent / "mariadb" / "playerbot" / "apply.sh"

RESISTS = [f"resist_{k}" for k, _l in mobs.RESISTS]
MOB_COLS = ["vnum", "name", "locale_name", "rank", "type", "battle_type", "level", "ai_flag", "max_hp", "damage_min",
            "damage_max", "def", "exp", "gold_min", "gold_max", "aggressive_sight", "attack_speed", "move_speed",
            "st", "dx", "ht", "iq", "dam_multiply", "drop_item", "regen_cycle", "regen_percent", "aggressive_hp_pct",
            "attack_range", "summon", "resurrection_vnum"] + RESISTS


def mob(vnum, name, level, rank=0, type_=0, hp=1000, dmg=(10, 20), exp=100, aggr=False):
    row = {c: 0 for c in MOB_COLS}
    row.update(vnum=vnum, name=name.encode("cp1250"), locale_name=name.encode("cp1250"), rank=rank, type=type_,
               level=level, ai_flag="AGGR" if aggr else "", max_hp=hp, damage_min=dmg[0], damage_max=dmg[1], def_=0,
               exp=exp, gold_min=10, gold_max=20, aggressive_sight=1000 if aggr else 0, attack_speed=100,
               move_speed=100, dam_multiply=1.0)
    row["def"] = row.pop("def_")
    return row


MOBS = [
    mob(101, "Dziki Pies", 1),
    mob(171, "Wilk", 3, aggr=True),
    mob(691, "Wodz Orkow", 42, rank=4, hp=50000, dmg=(200, 260), exp=20000, aggr=True),
    mob(692, "Orkowy Wojownik", 41, rank=2),
    mob(2091, "Krolowa Pajakow", 65, rank=4, hp=600000, exp=80000),
    mob(1491, "Chegal", 96, rank=4, hp=1000000),
    mob(701, "Kapitan Hwang", 70, rank=3, exp=4000),
    mob(702, "Mnich Hwang", 75, rank=1, exp=3000),
    mob(8001, "Metin Cierpienia", 5, rank=5, type_=2, hp=6000),
    mob(8005, "Metin Czerni", 25, rank=5, type_=2, hp=30000),
    mob(6091, "Razador", 68, rank=5, hp=700000),
    mob(20001, "Sprzedawca", 1, type_=1),
]

SPECIAL_BASE = (
    "Group\tBoss_Giant\r\n{\r\n\tvnum\t1491\r\n\tspawn_vnum\t1491\r\n\tspawn_type\tmob\r\n\tchannel\t0\r\n"
    "\tmap_index\t64\r\n\ttime\t12600-16200\r\n\ttime_type\tnormal\r\n\tcount\t1\r\n\tsave_kill\t1\r\n"
    "\tinitial_delay\t900-1500\r\n\t1\t154\t177\t0\r\n\t2\t320\t122\t0\r\n}\r\n\r\n"
    "# a comment the image has\r\n"
    "Group\tWodzOrkowLotny\r\n{\r\n\tvnum\t601\r\n\tspawn_vnum\t621\r\n\tspawn_type\tgroup\r\n\tchannel\t0\r\n"
    "\tmap_index\t64\r\n\ttime\t3300-5100\r\n\ttime_type\tnormal\r\n\tcount\t1\r\n\tsave_kill\t0\r\n"
    "\tinitial_delay\t900-1500\r\n\t1\t570\t137\t0\r\n\t2\t536\t1148\t0\r\n\t3\t1392\t1224\t0\r\n}\r\n")
GROUPS = ("Group\tL01\r\n{\r\n\tLeader\tWodz\t691\r\n\tVnum\t621\r\n\t1\t\"Orkowy Wojownik\"\t692\r\n\t2\tOrk\t692\r\n}\r\n"
          "Group\tL02\r\n{\r\n\tLeader\tHwang\t701\r\n\tVnum\t317\r\n\t1\tMnich\t702\r\n}\r\n")
GROUP_GROUPS = "Group\ta1_01\r\n{\r\n\tVnum\t2001\r\n\t1\t621\t1\r\n\t2\t317\t1\r\n}\r\n"
A1_BOSS = ("g\t916\t1023\t100\t100\t0\t0\t1000s\t100\t1\t317\r\n"
           "g\t895\t903\t100\t100\t0\t0\t15m-20m\t100\t1\t621\r\n"
           "e\t10\t10\t5\t5\t0\r\n")
A1_STONE = ("m\t224\t153\t150\t200\t0\t0\t20m-25m\t100\t1\t8005\r\n"
            "m\t179\t817\t150\t200\t0\t0\t20m-25m\t100\t1\t8001\r\n"
            "\r\n// a comment line\r\n"
            "mh\t556\t901\t200\t150\t0\t0\t601s\t100\t1\t8001\r\n")
A1_REGEN = "m\t300\t300\t10\t10\t0\t0\t30s\t100\t5\t101\r\nr\t76\t70\t100\t100\t0\t0\t3555s\t100\t1\t2001\r\n"
THREEWAY_BOSS = "gd\t770\t757\t150\t150\t0\t0\t55m-85mm\t100\t1\t621\r\n"
SETTING = "ScriptType\tMapSetting\n\nCellScale\t200\nMapSize\t4\t5\n"


def publish_base(spool):
    """What m2-spawns apply publishes for the panel."""
    base = Path(spool) / "spawns" / "base"
    files = {
        "map_index.txt": "1\tmetin2_map_a1\r\n64 map_n_threeway\r\n65 metin2_map_milgyo\r\n",
        "group.txt": GROUPS, "group_group.txt": GROUP_GROUPS, "special_spawns.txt": SPECIAL_BASE,
        "maps/metin2_map_a1/boss.txt": A1_BOSS, "maps/metin2_map_a1/stone.txt": A1_STONE,
        "maps/metin2_map_a1/regen.txt": A1_REGEN, "maps/metin2_map_a1/Setting.txt": SETTING,
        "maps/map_n_threeway/boss.txt": THREEWAY_BOSS, "maps/map_n_threeway/Setting.txt": SETTING.replace("4\t5", "6\t6"),
        "maps/metin2_map_milgyo/regen.txt": "m\t10\t10\t5\t5\t0\t0\t60s\t100\t3\t702\r\n",
    }
    for name, text in files.items():
        path = base / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(text.encode("latin-1"))
    return files


class SqliteDB:
    """The panel's rows()/db() on SQLite: world and player attached as
    schemas, the MariaDB-only bits of the modules' SQL translated."""

    def __init__(self):
        self.con = sqlite3.connect(":memory:", check_same_thread=False, isolation_level=None)
        self.con.execute("ATTACH ':memory:' AS world")
        self.con.execute("ATTACH ':memory:' AS player")
        self.con.create_function("UNHEX", 1, lambda h: bytes.fromhex(h))
        self.con.create_function("NOW", 0, lambda: "2026-10-04 13:00:00")
        cols = ", ".join(f"`{c}`" for c in MOB_COLS)
        # column affinities as MariaDB compares: a vnum given as text still matches
        typed = ", ".join(f"`{c}` " + ("BLOB" if c in ("name", "locale_name") else "TEXT" if c == "ai_flag"
                                         else "REAL" if c == "dam_multiply" else "INTEGER") for c in MOB_COLS)
        self.con.execute(f"CREATE TABLE world.mob_proto ({typed}, PRIMARY KEY(vnum))")
        for row in MOBS:
            marks = ",".join("?" * len(MOB_COLS))
            self.con.execute(f"INSERT INTO world.mob_proto ({cols}) VALUES ({marks})", [row[c] for c in MOB_COLS])
        self.con.execute("CREATE TABLE player.quest (dwPID INT, szName TEXT, szState TEXT, lValue INT)")

    @staticmethod
    def translate(sql):
        if "CREATE TABLE IF NOT EXISTS player.web_dbeditor_history" in sql:
            return ("CREATE TABLE IF NOT EXISTS player.web_dbeditor_history (id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "changed_at TEXT DEFAULT '2026-10-04 12:00:00', who TEXT, tbl TEXT, row_key TEXT, label TEXT, "
                    "col TEXT, old_value TEXT, new_value TEXT, batch TEXT, note TEXT DEFAULT '', reverted_in TEXT, "
                    "applied_at TEXT)")
        sql = re.sub(r"CAST\((`\w+`) AS (BINARY|CHAR)\)", r"\1", sql)
        return sql.replace(" FOR UPDATE", "").replace("%s", "?")

    def execute(self, sql, params=()):
        cur = self.con.execute(self.translate(sql), list(params or ()))
        names = [d[0] for d in cur.description] if cur.description else []
        return [dict(zip(names, r)) for r in cur.fetchall()] if names else [], cur.rowcount

    def rows(self, sql, params=()):
        return self.execute(sql, params)[0]

    def db(self):
        return Connection(self)

    def mob(self, vnum):
        return self.rows("SELECT * FROM world.mob_proto WHERE vnum=%s", (vnum,))[0]

    def set_flag(self, name, value):
        self.con.execute("INSERT INTO player.quest VALUES (0, ?, '', ?)", (name, value))


class Cursor:
    def __init__(self, fake):
        self.fake, self.result, self.rowcount = fake, [], 0

    def execute(self, sql, params=()):
        self.result, self.rowcount = self.fake.execute(sql, params)

    def fetchone(self):
        return self.result[0] if self.result else None

    def fetchall(self):
        return self.result

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


class Connection:
    def __init__(self, fake):
        self.fake = fake

    def cursor(self):
        return Cursor(self.fake)

    def begin(self):
        self.fake.con.execute("BEGIN")

    def commit(self):
        self.fake.con.execute("COMMIT")

    def rollback(self):
        try:
            self.fake.con.execute("ROLLBACK")
        except sqlite3.OperationalError:
            pass

    def close(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


class Base(unittest.TestCase):
    def setUp(self):
        self.fake = SqliteDB()
        # one spool for the whole run: "Zastosuj" (clientdata.py) takes its
        # folder once, at install time (DBEDITOR_SPOOL_ROOT above)
        self.spool = SPOOL
        for child in self.spool.iterdir():
            shutil.rmtree(child) if child.is_dir() else child.unlink()
        publish_base(self.spool)
        sf._CACHE.clear()
        common_items._STATE["table_ready"] = False
        common_items._CTX.update(rows=self.fake.rows, db=self.fake.db)
        self.patches = [
            patch.object(panel, "settings", return_value={"setup_complete": "1", "auth_enabled": "0"}),
            patch.object(panel, "check_all_notifications", return_value=None, create=True),
            patch.object(panel, "rows", side_effect=lambda sql, params=(): []),
        ]
        for p in self.patches:
            p.start()
        panel.app.config["TESTING"] = True
        panel.app.config["DBE_SPOOL"] = str(self.spool)
        self.client = panel.app.test_client()
        with self.client.session_transaction() as sess:
            sess["seban_update_csrf"] = "tok"

    def tearDown(self):
        for p in self.patches:
            p.stop()

    def post(self, url, data, **kw):
        data = dict(data)
        data.setdefault("dbe_csrf", "tok")
        return self.client.post(url, data=data, **kw)

    def text(self, response):
        return response.get_data(as_text=True)


class MobTests(Base):
    def mob_form(self, vnum, **changes):
        row = self.fake.mob(vnum)
        form = {c: row[c] for c in mobs.EDIT_COLS if c != "ai_flag"}
        form["ai_flag_sent"] = "1"
        form.update(changes)
        return form

    def test_search_by_kind_level_and_map(self):
        page = self.text(self.client.get("/db/mobs?rodzaj=bossowie"))
        self.assertIn("Wodz Orkow", page)
        self.assertIn("Krolowa Pajakow", page)
        self.assertNotIn("Dziki Pies", page)
        self.assertNotIn("Metin Czerni", page)
        page = self.text(self.client.get("/db/mobs?rodzaj=potwory&lvl_od=60&lvl_do=80"))
        self.assertIn("Kapitan Hwang", page)
        self.assertNotIn("Wodz Orkow", page)
        # map 1: its regen (101, the group of groups 2001 -> 691/692/701/702), boss and stone files
        page = self.text(self.client.get("/db/mobs?rodzaj=wszystko&mapa=1"))
        for name in ("Dziki Pies", "Wodz Orkow", "Kapitan Hwang", "Metin Cierpienia", "Metin Czerni"):
            self.assertIn(name, page)
        self.assertNotIn("Chegal", page)                      # the timed boss of map 64
        page = self.text(self.client.get("/db/mobs?q=1491"))
        self.assertIn("Chegal", page)
        self.assertIn("Dolina Orków", page)                   # where: map 64 (the panel's own map name)

    def test_edit_saves_history_and_is_pending(self):
        response = self.post("/db/mobs/691", self.mob_form(691, max_hp="80000", damage_min="250", damage_max="300",
                                                           move_speed="150"), follow_redirects=True)
        self.assertIn("czekają na zastosowanie", self.text(response))
        row = self.fake.mob(691)
        self.assertEqual((row["max_hp"], row["damage_min"], row["damage_max"], row["move_speed"]), (80000, 250, 300, 150))
        self.assertEqual(row["ai_flag"], "")                  # the AGGR box was not ticked
        pending = common_items.pending_changes()
        self.assertEqual({p["col"] for p in pending}, {"max_hp", "damage_min", "damage_max", "move_speed", "ai_flag"})
        self.assertTrue(all(p["tbl"] == "world.mob_proto" and p["row_key"] == "691" for p in pending))
        # the AGGR box back on, plus another flag: written as a SET value
        self.post("/db/mobs/691", dict(self.mob_form(691), ai_flag=["NOMOVE", "AGGR"]))
        self.assertEqual(self.fake.mob(691)["ai_flag"], "AGGR,NOMOVE")
        apply_page = self.text(self.client.get("/db/apply"))
        self.assertIn("potwór", apply_page)
        self.assertIn("Wodz Orkow", apply_page)

    def test_edit_rejects_bad_values(self):
        page = self.text(self.post("/db/mobs/691", self.mob_form(691, damage_min="500", damage_max="100"),
                                   follow_redirects=True))
        self.assertIn("minimum (500) jest większe niż maksimum", page)
        page = self.text(self.post("/db/mobs/691", self.mob_form(691, level="300"), follow_redirects=True))
        self.assertIn("dozwolone 1…255", page)
        self.assertEqual(self.fake.mob(691)["level"], 42)
        self.assertEqual(common_items.pending_changes(), [])

    def test_boot_protected_warning(self):
        page = self.text(self.client.get("/db/mobs/6091"))
        self.assertIn("KAŻDYM starcie", page)
        page = self.text(self.post("/db/mobs/6091", self.mob_form(6091, max_hp="900000"), follow_redirects=True))
        self.assertIn("zniknie po restarcie", page)

    def test_mass_change_preview_apply_and_undo(self):
        query = "/db/mobs/masowo?podglad=1&rodzaj=bossowie&pole=max_hp&operacja=mul&wartosc=2"
        page = self.text(self.client.get(query))
        self.assertIn("4 potworów", page)                    # 691, 2091, 1491 and the King-rank 6091 - no Metin
        self.assertIn("50000</span> → <b>100000", page)
        self.assertNotIn("Metin Czerni", page.split("Podgląd")[1])
        self.assertIn("ustawianą przez skrypt startowy", page)  # 6091 (Razador) is put back by apply.sh
        vnums = re.search(r'name="vnums" value="([\d,]+)"', page).group(1)
        self.assertEqual(set(vnums.split(",")), {"691", "2091", "1491", "6091"})
        response = self.post("/db/mobs/masowo", {"rodzaj": "bossowie", "pole": "max_hp", "operacja": "mul",
                                                 "wartosc": "2", "vnums": vnums}, follow_redirects=True)
        self.assertIn("Zmieniono 4 potworów", self.text(response))
        self.assertEqual(self.fake.mob(691)["max_hp"], 100000)
        self.assertEqual(self.fake.mob(1491)["max_hp"], 2000000)
        self.assertEqual(self.fake.mob(8005)["max_hp"], 30000)
        batches = common_items.history_batches(10, "world.mob_proto")
        self.assertEqual(len(batches), 1)
        self.assertEqual(batches[0]["note"], "Masowo: PŻ ×2")
        self.post("/db/historia/cofnij", {"batch": batches[0]["batch"]})
        self.assertEqual(self.fake.mob(691)["max_hp"], 50000)
        self.assertEqual(self.fake.mob(1491)["max_hp"], 1000000)
        self.assertEqual(common_items.pending_changes(), [])  # undone before a restart: nothing waits

    def test_mass_exp_by_level_and_stale_list(self):
        query = {"rodzaj": "potwory", "lvl_od": "60", "lvl_do": "80", "pole": "exp", "operacja": "mul", "wartosc": "1,5"}
        page = self.text(self.client.get("/db/mobs/masowo", query_string=dict(query, podglad=1)))
        self.assertIn("2 potworów", page)
        # a list that is not the previewed one is not applied
        page = self.text(self.post("/db/mobs/masowo", dict(query, vnums="701"), follow_redirects=True))
        self.assertIn("zmieniła się od podglądu", page)
        self.assertEqual(self.fake.mob(701)["exp"], 4000)
        self.post("/db/mobs/masowo", dict(query, vnums="701,702"))
        self.assertEqual((self.fake.mob(701)["exp"], self.fake.mob(702)["exp"]), (6000, 4500))

    def test_mass_clamps_and_keeps_min_below_max(self):
        rows = [mobs.decorate(self.fake.mob(2091))]
        plan = mobs.mass_plan(rows, "max_hp", "mul", 10000)
        self.assertEqual(plan[0][1]["max_hp"][1], mobs.SPECS["max_hp"]["max"])
        self.assertTrue(plan[0][2])
        rows = [mobs.decorate(dict(self.fake.mob(691), damage_min=60000, damage_max=60000))]
        plan = mobs.mass_plan(rows, "damage", "add", 10000)
        self.assertEqual((plan[0][1]["damage_min"][1], plan[0][1]["damage_max"][1]), (65535, 65535))
        self.assertEqual(mobs.parse_mass({"pole": "exp", "operacja": "mul", "wartosc": "x1,5"})[2], 1.5)
        self.assertIsNotNone(mobs.parse_mass({"pole": "exp", "operacja": "mul", "wartosc": "500"})[3])

    def test_boot_rules_match_apply_sh(self):
        if not APPLY_SH.is_file():
            self.skipTest("no apply.sh")
        text = APPLY_SH.read_text(encoding="utf-8", errors="replace")
        for match in re.finditer(r"UPDATE world\.mob_proto\s+SET\s+(.*?)\s+WHERE\s+(.*?)(;|\")", text, re.S):
            cols = set(re.findall(r"(?:^|,\s*)`?(\w+)`?\s*=", match.group(1))) & set(mobs.ALL_SPECS)
            where = match.group(2).strip()
            single = re.fullmatch(r"vnum\s*=\s*(\d+)", where)
            many = re.fullmatch(r"vnum\s+IN\s*\(([\d,\s]+)\)", where)
            if not cols or not (single or many):
                continue   # a conditional fix (only while the old value is there) keeps an edit
            vnums = [int(single.group(1))] if single else [int(v) for v in many.group(1).split(",")]
            for vnum in vnums:
                self.assertLessEqual(cols, mobs.boot_cols(vnum), f"apply.sh sets {cols} of {vnum} at every start")


class MeleeTests(Base):
    """MT2009_PLUS_DB_EDITOR_MOB_MELEE_V1: "Zwykłe potwory: atak z dystansu → wręcz"."""

    RANGED = [
        # (vnum, name, level, rank, type, battle_type, attack_range, matched)
        (503, "Dziki Lucznik", 35, 1, 0, 1, 850, True),
        (604, "Ork Czarodziej", 33, 2, 0, 2, 150, True),
        (301, "Zaprzys Zolnierz", 18, 0, 0, 2, 175, True),        # range already 175: only battle_type changes
        (533, "Bestialski Lucznik", 40, 3, 0, 1, 850, True),
        (2493, "Boss Lucznik", 60, 4, 0, 1, 850, False),          # a boss
        (9706, "Baronowa Pajakow", 35, 5, 0, 1, 250, False),      # a king
        (8099, "Metin Strzelec", 40, 3, 2, 1, 850, False),        # a Metin stone (type 2)
        (20099, "Straznik Lucznik", 50, 0, 1, 1, 850, False),     # an NPC (type 1)
        (777, "Przywolywacz Specjalny", 57, 3, 0, 3, 850, False),  # battle_type 3 (special) is not 1/2
    ]

    def setUp(self):
        super().setUp()
        for vnum, name, level, rank, type_, battle, rng, _m in self.RANGED:
            row = mob(vnum, name, level, rank=rank, type_=type_)
            row.update(battle_type=battle, attack_range=rng)
            self.fake.con.execute(f"INSERT INTO world.mob_proto ({', '.join('`%s`' % c for c in MOB_COLS)}) "
                                  f"VALUES ({','.join('?' * len(MOB_COLS))})", [row[c] for c in MOB_COLS])
        self.matched = {v for v, *_r, m in self.RANGED if m}

    def state(self):
        return {r["vnum"]: (r["battle_type"], r["attack_range"])
                for r in self.fake.rows("SELECT vnum, battle_type, attack_range FROM world.mob_proto")}

    def test_where_is_the_owners_sql(self):
        self.assertEqual(mobs.MELEE_WHERE, "battle_type IN (1, 2) AND `rank` IN (0, 1, 2, 3) AND `type` = 0")
        self.assertEqual(mobs.MELEE_VALUES, {"battle_type": 0, "attack_range": 175})
        # what the SQL would match on the same rows
        found = {r["vnum"] for r in self.fake.rows(f"SELECT vnum FROM world.mob_proto WHERE {mobs.MELEE_WHERE}")}
        self.assertEqual(found, self.matched)

    def test_preview_selects_only_ordinary_ranged(self):
        page = self.text(self.client.get("/db/mobs/wrecz"))
        self.assertIn("Podgląd: 4 potworów", page)
        vnums = re.search(r'name="vnums" value="([\d,]+)"', page).group(1)
        self.assertEqual({int(v) for v in vnums.split(",")}, self.matched)
        for name in ("Dziki Lucznik", "Ork Czarodziej", "Zaprzys Zolnierz", "Bestialski Lucznik"):
            self.assertIn(name, page)
        for name in ("Boss Lucznik", "Baronowa Pajakow", "Metin Strzelec", "Straznik Lucznik", "Przywolywacz Specjalny",
                     "Wodz Orkow"):
            self.assertNotIn(name, page)
        self.assertIn("dystans (łuk) [1]", page)
        self.assertIn("magia (dystans) [2]", page)
        self.assertIn("850</span> → <b>175", page)
        self.assertIn("(bez zmian)", page)                    # 301: range already 175
        self.assertIn("po „Zastosuj”", page)
        self.assertEqual(self.client.get("/db/mobs").status_code, 200)
        self.assertIn("Dystans → wręcz", self.text(self.client.get("/db/mobs")))

    def test_apply_writes_one_batch_and_undo_restores(self):
        before = self.state()
        page = self.text(self.client.get("/db/mobs/wrecz"))
        vnums = re.search(r'name="vnums" value="([\d,]+)"', page).group(1)
        page = self.text(self.post("/db/mobs/wrecz", {"vnums": vnums}, follow_redirects=True))
        self.assertIn("4 potworów walczy teraz wręcz", page)
        after = self.state()
        for vnum in self.matched:
            self.assertEqual(after[vnum], (0, 175))
        for vnum in set(before) - self.matched:
            self.assertEqual(after[vnum], before[vnum])      # bosses, Metins, NPCs, the rest untouched
        batches = common_items.history_batches(10, "world.mob_proto")
        self.assertEqual(len(batches), 1)
        self.assertEqual(batches[0]["note"], mobs.MELEE_NOTE)
        self.assertEqual(batches[0]["row_count"], 4)
        fields = {(r["row_key"], r["col"]): (r["old_value"], r["new_value"]) for r in batches[0]["rows"]}
        self.assertEqual(fields, {("503", "battle_type"): ("1", "0"), ("503", "attack_range"): ("850", "175"),
                                  ("604", "battle_type"): ("2", "0"), ("604", "attack_range"): ("150", "175"),
                                  ("301", "battle_type"): ("2", "0"),
                                  ("533", "battle_type"): ("1", "0"), ("533", "attack_range"): ("850", "175")})
        self.assertEqual(len(common_items.pending_changes()), 7)
        apply_page = self.text(self.client.get("/db/apply"))
        self.assertIn("Dziki Lucznik", apply_page)
        history = self.text(self.client.get("/db/historia?tbl=world.mob_proto"))
        self.assertIn("Sposób walki (battle_type)", history)
        self.assertIn("dystans (łuk) [1]", history)
        # nothing left to change, the save listed with its undo
        page = self.text(self.client.get("/db/mobs/wrecz"))
        self.assertIn("nie ma nic do zmiany", page)
        self.assertIn("czeka na restart", page)
        self.assertIn("Cofnij tę zmianę", page)
        page = self.text(self.post("/db/historia/cofnij", {"batch": batches[0]["batch"], "back": "/db/mobs/wrecz"},
                                   follow_redirects=True))
        self.assertIn("Cofnięto 7 zmian", page)
        self.assertEqual(self.state(), before)
        self.assertEqual(common_items.pending_changes(), [])  # undone before a restart: nothing waits
        page = self.text(self.client.get("/db/mobs/wrecz"))
        self.assertIn("cofnięta", page)
        self.assertIn("Podgląd: 4 potworów", page)

    def test_stale_preview_is_refused(self):
        page = self.text(self.post("/db/mobs/wrecz", {"vnums": "503,604"}, follow_redirects=True))
        self.assertIn("zmieniła się od podglądu", page)
        self.assertEqual(self.state()[503], (1, 850))
        self.assertEqual(common_items.pending_changes(), [])
        page = self.text(self.post("/db/mobs/wrecz", {"vnums": "503,604,301,533", "dbe_csrf": "zly"},
                                   follow_redirects=True))
        self.assertIn("Sesja formularza wygasła", page)
        self.assertEqual(self.state()[503], (1, 850))

    def test_start_script_does_not_revert(self):
        # apply.sh writes attack_range only for 9706 (a king, never matched), never battle_type
        for vnum in self.matched:
            self.assertFalse({"battle_type", "attack_range"} & mobs.boot_cols(vnum))
        self.assertIn("attack_range", mobs.boot_cols(9706))
        self.assertIn("Zasięg ataku", self.text(self.client.get("/db/mobs/9706")))   # the edit page's start-up warning
        if APPLY_SH.is_file():
            text = APPLY_SH.read_text(encoding="utf-8", errors="replace")
            self.assertNotRegex(text, r"(?i)UPDATE\s+world\.mob_proto\s+SET[^;\"]*\bbattle_type\s*=")
            for match in re.finditer(r"UPDATE world\.mob_proto\s+SET\s+([^;\"]*?)\s+WHERE\s+([^;\"]*)", text):
                if re.search(r"\battack_range\s*=", match.group(1)):
                    self.assertRegex(match.group(2), r"^vnum\s*=\s*9706\b")
            self.assertNotRegex(text, r"(?i)REPLACE\s+INTO\s+world\.mob_proto")


class SpawnTests(Base):
    def test_overview_shows_base_and_effective_times(self):
        self.fake.set_flag("fastBossSpawn", 50)
        self.fake.set_flag("m2_boss_count", 200)
        page = self.text(self.client.get("/db/spawns"))
        self.assertIn("Metiny i bossy co 50%", page)
        self.assertIn("Chegal", page)                         # the timed boss
        self.assertIn("co 3 h 30 min – 4 h 30 min", page)
        page = self.text(self.client.get("/db/spawns/mapa/metin2_map_a1"))
        self.assertIn("20–25 min", page)                      # no: 20m-25m shows in the inputs, the game's half here
        self.assertIn("10–12,5 min", page)
        self.assertIn("→ 2 w grze", page)                     # m2_boss_count 200% on a Metin line
        self.assertIn("Kapitan Hwang", page)                  # group 317's leader

    def test_effective_time_rules(self):
        flags = {"fastBossSpawn": 50, "fastMobSpawn64": 20, "fastMobSpawn": 0}
        self.assertEqual(sf.delay_percent(flags, True, 1), (50, "world"))
        self.assertEqual(sf.delay_percent(flags, False, 64), (20, "map"))
        self.assertEqual(sf.delay_percent(flags, False, 1), (100, None))
        self.assertEqual(sf.effective_seconds(5, 20), 3)       # never below 3 s
        self.assertEqual(sf.effective_seconds(1500, 50), 750)
        self.assertEqual(sf.count_percent({"m2_mob_count": 300}, False, 1, True), 300)
        self.assertEqual(sf.count_percent({"m2_mob_count": 300}, False, 1, False), 100)   # NPC in the line
        self.assertEqual(sf.count_percent({"m2_mob_count": 300}, False, 10001, True), 100)  # a dungeon instance
        self.assertEqual(sf.parse_time("55m-85mm"), (3300, 5100))
        self.assertEqual(sf.parse_time("1000s"), (1000, 1000))
        self.assertEqual(sf.parse_time("1h30m"), (5400, 5400))

    def map_form(self, kind="stone"):
        page = self.text(self.client.get("/db/spawns/mapa/metin2_map_a1"))
        section = page.split(f'id="plik-{kind}"')[1].split('data-dbe-rows>')[1].split("</form>")[0]
        form = {"plik": kind, "nowe": "3"}
        for name, value in re.findall(r'<input name="([rn]\d+_\w+)" value="([^"]*)"', section):
            form[name] = value
        for name, value in re.findall(r'<select name="([rn]\d+_typ)">.*?<option value="(\w)" selected', section):
            form[name] = value
        form["wersja"] = re.search(r'name="wersja" value="(\w+)"', section).group(1)
        return form

    def test_edit_map_file_keeps_untouched_lines(self):
        form = self.map_form()
        self.assertEqual(form["r0_od"], "20")
        self.assertEqual(form["r4_od"], "10,02")                 # 601 s
        response = self.post("/db/spawns/mapa/metin2_map_a1", self.map_form(), follow_redirects=True)
        self.assertIn("Brak zmian", self.text(response))
        self.assertFalse(sf.map_custom_path(self.spool, "metin2_map_a1", "stone").exists())
        form = dict(form, r0_od="5", r0_do="7,5", r0_ile="2")
        response = self.post("/db/spawns/mapa/metin2_map_a1", form, follow_redirects=True)
        self.assertIn("Zapisano", self.text(response))
        written = sf.map_custom_path(self.spool, "metin2_map_a1", "stone").read_bytes().decode()
        lines = written.split("\r\n")
        self.assertEqual(lines[0], "m\t224\t153\t150\t200\t0\t0\t5m-450s\t100\t2\t8005")
        self.assertEqual(lines[1:], A1_STONE.split("\r\n")[1:])    # every other line byte for byte
        self.assertEqual(sf.parse_time("5m-450s"), (300, 450))
        pending = sf.pending_changes(self.spool)
        self.assertEqual([p["key"] for p in pending], ["map.metin2_map_a1.stone"])
        self.assertEqual(pending[0]["kind"], "unknown")          # no game status yet
        self.assertIn("Respawn: Metiny – Shinsoo M1", pending[0]["title"])
        self.assertIn("Respawn: Metiny", self.text(self.client.get("/db/apply")))
        self.assertEqual(len(sf.list_backups(self.spool)), 1)
        # the same form again: the page changed meanwhile
        response = self.post("/db/spawns/mapa/metin2_map_a1", form, follow_redirects=True)
        self.assertIn("zmienił się od otwarcia strony", self.text(response))

    def test_add_delete_and_validation(self):
        form = dict(self.map_form("boss"), n0_typ="m", n0_vnum="691", n0_x="500", n0_y="600", n0_od="30", n0_do="40",
                    n0_ile="1", r1_usun="1")
        self.post("/db/spawns/mapa/metin2_map_a1", form)
        written = sf.map_custom_path(self.spool, "metin2_map_a1", "boss").read_bytes().decode()
        self.assertNotIn("\t621\r\n", written)
        self.assertIn("e\t10\t10\t5\t5\t0", written)            # the other kind of line is kept
        self.assertTrue(written.endswith("m\t500\t600\t0\t0\t0\t0\t30m-40m\t100\t1\t691\r\n"))
        for bad, message in ((dict(n0_vnum="99999"), "nie ma potwora 99999"), (dict(n0_x="9000"), "X: dozwolone"),
                             (dict(n0_od="50"), "mniejsze niż „od”"), (dict(n0_typ="g", n0_vnum="5"), "nie ma grupy 5")):
            form = dict(self.map_form("boss"), n0_typ="m", n0_vnum="691", n0_x="1", n0_y="1", n0_od="30", n0_do="40",
                        n0_ile="1")
            form.update(bad)
            before = sf.map_custom_path(self.spool, "metin2_map_a1", "boss").read_bytes()
            page = self.text(self.post("/db/spawns/mapa/metin2_map_a1", form, follow_redirects=True))
            self.assertIn(message, page)
            self.assertEqual(sf.map_custom_path(self.spool, "metin2_map_a1", "boss").read_bytes(), before)

    def test_reset_to_image_and_status(self):
        form = dict(self.map_form(), r1_ile="3")
        self.post("/db/spawns/mapa/metin2_map_a1", form)
        custom = sf.map_custom_path(self.spool, "metin2_map_a1", "stone")
        sha = sf.file_sha(custom)
        (self.spool / "spawns" / "status").write_text(f"time=1\nmap.metin2_map_a1.stone=ok|{sha}|applied\n")
        self.assertEqual(sf.pending_changes(self.spool), [])
        self.post("/db/spawns/mapa/metin2_map_a1/stone/przywroc", {})
        self.assertFalse(custom.exists())
        pending = sf.pending_changes(self.spool)
        self.assertEqual(len(pending), 1)
        self.assertIn("Powrót do pliku z obrazu", pending[0]["text"])
        (self.spool / "spawns" / "status").write_text("time=2\n")
        self.assertEqual(sf.pending_changes(self.spool), [])
        (self.spool / "spawns" / "status").write_text(f"time=3\nmap.metin2_map_a1.stone=rejected|{sha}|bad\n")
        self.post("/db/spawns/kopie", {"kopia": sf.list_backups(self.spool)[0]["name"]})
        self.assertEqual(sf.file_sha(custom), sha)                # the backup of the edited file came back
        self.assertEqual(sf.pending_changes(self.spool)[0]["kind"], "error")

    def special_form(self, vnum):
        page = self.text(self.client.get(f"/db/spawns/specjalne/{vnum}"))
        form = dict(re.findall(r'<input name="(\w+)" value="([^"]*)"', page))
        form.update(re.findall(r'<select name="(\w+)"[^>]*>(?:(?!</select>).)*?<option value="(\w*)" selected', page, re.S))
        form["miejsca"] = re.search(r'name="miejsca" value="(\d+)"', page).group(1)
        if 'name="zapamietaj" value="1" checked' in page:
            form["zapamietaj"] = "1"
        return form

    def test_special_edit_add_remove(self):
        form = self.special_form(1491)
        self.assertEqual((form["od"], form["do"], form["mapa"], form["rodzaj"]), ("210", "270", "64", "mob"))
        self.post("/db/spawns/specjalne/1491", dict(form, od="60", do="90", p2_x="400", p2_y="500"))
        custom = sf.parse_special(sf.read_text(sf.special_custom_path(self.spool)))
        self.assertEqual([(g["vnum"], g["time"], len(g["places"])) for g in custom], [(1491, (3600, 5400), 3)])
        self.assertEqual(custom[0]["initial_delay"], (900, 1500))
        # count above the places is refused (the core would shut down)
        page = self.text(self.post("/db/spawns/specjalne/1491", dict(form, ile="5"), follow_redirects=True))
        self.assertIn("nie może być większe niż liczba miejsc", page)
        # a new one
        new = {"nazwa": "Boss_Nowy", "rodzaj": "group", "spawn_vnum": "621", "mapa": "1", "czas_typ": "normal",
               "od": "45", "do": "60", "ile": "1", "miejsca": "4", "p0_x": "100", "p0_y": "200", "p0_rot": "0"}
        self.post("/db/spawns/specjalne/nowy", new)
        index = sf.SpawnIndex(self.spool)
        added = [g for g in index.special if g.get("origin") == "added"]
        self.assertEqual([(g["name"], g["vnum"], g["spawn_type"]) for g in added], [("Boss_Nowy", 1492, "group")])
        # remove an image one, then bring it back
        self.post("/db/spawns/specjalne/601/usun", {})
        index = sf.SpawnIndex(self.spool)
        self.assertNotIn(601, [g["vnum"] for g in index.special])
        self.assertIn("m2_removed\t1", sf.read_text(sf.special_custom_path(self.spool)))
        self.post("/db/spawns/specjalne/601/przywroc", {})
        index = sf.SpawnIndex(self.spool)
        self.assertIn(601, [g["vnum"] for g in index.special])
        # editing back to the image's values leaves no custom group for it
        self.post("/db/spawns/specjalne/1491/przywroc", {})
        self.assertEqual([g["vnum"] for g in sf.parse_special(sf.read_text(sf.special_custom_path(self.spool)))], [1492])
        self.assertEqual(self.client.get("/db/spawns").status_code, 200)

    def test_hour_spawn_and_names(self):
        form = self.special_form(601)
        self.post("/db/spawns/specjalne/601", dict(form, czas_typ="hour", godz_od="18:00", godz_do="18:30"))
        group = sf.parse_special(sf.read_text(sf.special_custom_path(self.spool)))[0]
        self.assertEqual((group["time_type"], group["time"]), ("hour", (64800, 66600)))
        self.assertIn("codziennie o 18:00–18:30", self.text(self.client.get("/db/spawns")))

    def test_sections_and_missing_index(self):
        import dbeditor
        endpoints = [s[0] for s in dbeditor.SECTIONS]
        self.assertIn("dbeditor.mobs", endpoints)
        self.assertIn("dbeditor.spawns", endpoints)
        shutil.rmtree(self.spool / "spawns")
        self.assertIn("nie opublikowała", self.text(self.client.get("/db/spawns")))
        self.assertEqual(self.client.get("/db/mobs?rodzaj=bossowie").status_code, 200)


@unittest.skipUnless(shutil.which("sh") and M2_SPAWNS.is_file(), "no sh or m2-spawns")
class GameSideTests(Base):
    """What the panel writes, taken by the game's m2-spawns (no database:
    the monster check is the panel's)."""

    def run_spawns(self, root, *commands):
        env = dict(os.environ, M2_SHARE_DIR=str(root / "share"), M2_VAR_DIR=str(root / "var"),
                   M2_RATES_SPOOL=str(self.spool), PATH="/usr/bin:/bin")
        for command in commands:
            result = subprocess.run(["sh", str(M2_SPAWNS), command], env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_m2_spawns_takes_the_panels_files(self):
        root = Path(tempfile.mkdtemp(prefix="dbe-game-"))
        self.addCleanup(shutil.rmtree, root, True)
        locale = root / "share" / "locale" / "poland"
        files = {"special_spawns.txt": SPECIAL_BASE, "group.txt": GROUPS, "group_group.txt": GROUP_GROUPS,
                 "map/index": "1\tmetin2_map_a1\r\n64 map_n_threeway\r\n",
                 "map/metin2_map_a1/boss.txt": A1_BOSS, "map/metin2_map_a1/stone.txt": A1_STONE,
                 "map/metin2_map_a1/regen.txt": A1_REGEN, "map/metin2_map_a1/Setting.txt": SETTING,
                 "map/map_n_threeway/boss.txt": THREEWAY_BOSS}
        for name, text in files.items():
            (locale / name).parent.mkdir(parents=True, exist_ok=True)
            (locale / name).write_bytes(text.encode("latin-1"))
        shutil.rmtree(self.spool / "spawns")
        self.run_spawns(root, "prepare", "apply")
        self.assertEqual((self.spool / "spawns" / "base" / "maps" / "metin2_map_a1" / "stone.txt").read_bytes(),
                         A1_STONE.encode())
        sf._CACHE.clear()
        # the panel's edits: a Metin line, a timed boss changed, one removed, one added
        self.post("/db/spawns/mapa/metin2_map_a1", dict(SpawnTests.map_form(self), r0_od="5", r0_do="5"))
        form = SpawnTests.special_form(self, 1491)
        self.post("/db/spawns/specjalne/1491", dict(form, od="60", do="60"))
        self.post("/db/spawns/specjalne/601/usun", {})
        self.post("/db/spawns/specjalne/nowy", {"nazwa": "Boss_Nowy", "rodzaj": "mob", "spawn_vnum": "691", "mapa": "1",
                                                "czas_typ": "normal", "od": "45", "do": "60", "ile": "1", "miejsca": "4",
                                                "p0_x": "100", "p0_y": "200", "p0_rot": "0"})
        # and a broken boss.txt for the other map (written by hand)
        broken = sf.map_custom_path(self.spool, "map_n_threeway", "boss")
        broken.parent.mkdir(parents=True, exist_ok=True)
        broken.write_bytes(b"g\t770\t757\t150\t150\t0\t0\t55m\t100\t1\r\n")      # 10 words
        self.assertEqual(len(sf.pending_changes(self.spool)), 3)
        self.run_spawns(root, "apply")
        live = (locale / "special_spawns.txt").read_bytes().decode()
        self.assertIn("\ttime\t3600\r\n", live)
        self.assertNotIn("12600-16200", live)
        self.assertNotIn("WodzOrkowLotny", live)
        self.assertIn("Group\tBoss_Nowy\r\n", live)
        self.assertIn("# a comment the image has", live)
        self.assertNotIn("m2_removed", live)
        groups = sf.parse_special(live)
        self.assertEqual(sorted(g["vnum"] for g in groups), [1491, 1492])
        stone = (locale / "map" / "metin2_map_a1" / "stone.txt").read_bytes()
        self.assertTrue(stone.startswith(b"m\t224\t153\t150\t200\t0\t0\t5m\t100\t1\t8005\r\n"))
        self.assertEqual((locale / "map" / "map_n_threeway" / "boss.txt").read_bytes(), THREEWAY_BOSS.encode())
        status = sf.read_status(self.spool)
        self.assertEqual(status["special"]["state"], "ok")
        self.assertEqual(status["map.map_n_threeway.boss"]["state"], "rejected")
        self.assertIn("11 words", status["map.map_n_threeway.boss"]["message"])
        pending = sf.pending_changes(self.spool)
        self.assertEqual([(p["key"], p["kind"]) for p in pending], [("map.map_n_threeway.boss", "error")])
        # back to the image: the panel's file removed, the image's file live again
        self.post("/db/spawns/mapa/metin2_map_a1/stone/przywroc", {})
        self.run_spawns(root, "apply")
        self.assertEqual((locale / "map" / "metin2_map_a1" / "stone.txt").read_bytes(), A1_STONE.encode())
        self.assertNotIn("map.metin2_map_a1.stone", sf.read_status(self.spool))
        # a special file the core would die on is not used
        sf.special_custom_path(self.spool).write_bytes(
            b"Group\tBad\r\n{\r\n\tvnum\t1491\r\n\tspawn_vnum\t1491\r\n\tspawn_type\tmob\r\n\tmap_index\t64\r\n"
            b"\ttime\t60\r\n\ttime_type\tnormal\r\n\tcount\t3\r\n\t1\t1\t1\t0\r\n}\r\n")
        self.run_spawns(root, "apply")
        self.assertEqual((locale / "special_spawns.txt").read_bytes(), SPECIAL_BASE.encode())
        self.assertIn("above the 1 places", sf.read_status(self.spool)["special"]["message"])


if __name__ == "__main__":
    unittest.main()
