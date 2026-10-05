"""MT2009_PLUS_DB_EDITOR_CONFIG_V1: python3 -m unittest test_dbeditor_config

"Eksport / import konfiguracji" (dbeditor/config.py) through Flask's test
client: two "servers" (two in-memory SQLite databases - SqliteDB of
test_dbeditor_tables, the very statements of common_items run - and two
spool folders) with the same stock game. Server A is changed through the
editor's own write paths, its export is imported into server B, B's export
must then say the same; invalid input is rejected, the preview shows what
changes, and one undo takes the whole import back."""
import io
import re
import tempfile
import unittest
from pathlib import Path

from flask import Blueprint, Flask
from jinja2 import ChoiceLoader, DictLoader, FileSystemLoader

import dbeditor
from dbeditor import attrs, chests, common_items, config, drops, exptable, fishing, items, mobs, refine, regen
from dbeditor import shops, skills, spawnfiles, spawns
from dbeditor import dropfiles as df
from test_dbeditor_tables import SqliteDB

HERE = Path(__file__).resolve().parent
FISH_VNUMS = sorted({int(v) for v in re.findall(r"^\s*(\d+),", fishing.SNAPSHOT.read_text(encoding="latin-1"), re.M)})
MOB_DROP_BASE = "Group\twolf\r\n{\r\n\tMob\t101\r\n\tType\tdrop\r\n\t1\t27001\t1\t50\r\n}\r\n"
REGEN_BASE = ("// spawns of the test map\r\n"
              "m\t195\t690\t10\t10\t0\t0\t5m-7m\t100\t1\t8015\r\n"
              "s\t0\t0\t0\t0\t0\t0\t2m\t100\t3\t103\r\n")
BOSS_BASE = "m\t100\t100\t5\t5\t0\t0\t30m\t100\t1\t191\r\n"


class GameDB(SqliteDB):
    """test_dbeditor_tables' database plus the other tables of the editor."""

    def __init__(self):
        super().__init__()
        c = self.con
        c.execute("CREATE TABLE world.item_proto (vnum INTEGER PRIMARY KEY, locale_name BLOB, type INTEGER DEFAULT 0, "
                  "subtype INTEGER DEFAULT 0, gold INTEGER DEFAULT 0, shop_buy_price INTEGER DEFAULT 0, "
                  "refine_set INTEGER DEFAULT 0, refined_vnum INTEGER DEFAULT 0, value0 INTEGER DEFAULT 0)")
        c.execute("CREATE TABLE world.mob_proto (vnum INTEGER PRIMARY KEY, locale_name BLOB, level INTEGER DEFAULT 1, "
                  "max_hp INTEGER DEFAULT 100, exp INTEGER DEFAULT 0, ai_flag TEXT DEFAULT '')")
        c.execute("CREATE TABLE world.skill_proto (dwVnum INTEGER PRIMARY KEY, szName TEXT, szPointPoly TEXT DEFAULT '')")
        c.execute("CREATE TABLE world.shop (vnum INTEGER PRIMARY KEY, name TEXT, npc_vnum INTEGER)")
        c.execute("CREATE TABLE world.shop_item (shop_vnum INTEGER, item_vnum INTEGER, count INTEGER, "
                  "UNIQUE (shop_vnum, item_vnum, count))")
        cols = ", ".join(f"vnum{k} INTEGER DEFAULT 0, count{k} INTEGER DEFAULT 0" for k in range(5))
        c.execute(f"CREATE TABLE world.refine_proto (id INTEGER PRIMARY KEY, {cols}, cost INTEGER DEFAULT 0, "
                  "prob INTEGER DEFAULT 100, src_vnum INTEGER DEFAULT 0, result_vnum INTEGER DEFAULT 0)")
        c.execute("CREATE TABLE world.item_extra_apply (vnum INTEGER, slot INTEGER, apply_type INTEGER DEFAULT 0, "
                  "apply_value INTEGER DEFAULT 0, PRIMARY KEY (vnum, slot))")

    def run(self, sql, params=()):
        # CAST(x AS BINARY) is a number in SQLite; the bytes are what MariaDB gives
        sql = sql.replace(" AS BINARY)", " AS BLOB)").replace(" ENGINE=InnoDB", "")
        return super().run(sql, params)


def stock(fake):
    """The same stock game on both servers."""
    c = fake.con
    item_rows = [(19, "Miecz+9".encode("cp1250"), 1, 0, 1000, 500, 1, 0, 0),
                 (20, b"Miecz+10", 1, 0, 1100, 0, 0, 0, 0), (11, b"Miecz+1", 1, 0, 100, 0, 0, 0, 0),
                 (27001, b"Mikstura", 0, 0, 50, 0, 0, 0, 0), (27002, b"Mikstura 2", 0, 0, 60, 0, 0, 0, 0)]
    item_rows += [(v, f"Ryba {v}".encode(), 0, 0, 0, 0, 0, 0, 0) for v in FISH_VNUMS + list(fishing.DYE_VNUMS)
                  if v not in {r[0] for r in item_rows}]
    c.executemany("INSERT INTO world.item_proto VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)", item_rows)
    c.executemany("INSERT INTO world.mob_proto (vnum, locale_name, level, max_hp, exp, ai_flag) VALUES (?, ?, ?, ?, ?, ?)",
                  [(101, b"Dziki Pies", 1, 100, 10, ""), (103, b"Alfa Wilk", 5, 300, 40, "AGGR"),
                   (8015, b"Metin", 40, 9000, 500, ""), (191, b"Lykos", 25, 5000, 300, "")])
    c.execute("INSERT INTO world.skill_proto VALUES (1, 'Trzy Ciecia', '-(1.1*atk)')")
    c.execute("INSERT INTO world.shop VALUES (9001, 'Bronie', 9001)")
    c.executemany("INSERT INTO world.shop_item VALUES (?, ?, ?)", [(9001, 11, 1), (9001, 19, 1)])
    c.execute("INSERT INTO world.refine_proto (id, vnum0, count0, cost, prob) VALUES (1, 27001, 2, 1000, 90)")
    c.execute("INSERT INTO world.item_attr (apply, prob, lv1, lv2, lv3, lv4, lv5, weapon) "
              "VALUES ('POINT_ST', 11, 2, 4, 6, 8, 12, 3)")
    c.executemany("INSERT INTO common.exp_table VALUES (?, ?)", [(lv, exptable.builtin(lv)) for lv in range(1, 120)])


def stock_spool(spool):
    (spool / "drops").mkdir(parents=True)
    (spool / "drops" / "mob_drop_item.base.txt").write_bytes(MOB_DROP_BASE.encode("ascii"))
    maps = spool / "regen" / "base" / "maps" / "metin2_map_a1"
    maps.mkdir(parents=True)
    (maps / "regen.txt").write_bytes(REGEN_BASE.encode("latin-1"))
    (spool / "regen" / "base" / "index").write_text("1\tmetin2_map_a1\n", encoding="ascii")
    boss = spool / "spawns" / "base" / "maps" / "metin2_map_a1"
    boss.mkdir(parents=True)
    (boss / "boss.txt").write_bytes(BOSS_BASE.encode("latin-1"))


def game_text(value):
    if isinstance(value, (bytes, bytearray, memoryview)):
        return bytes(value).decode("cp1250")
    return value or ""


def make_app(fake, spool):
    app = Flask(__name__, static_folder=str(HERE / "static"))
    app.secret_key = "test-only"
    app.config.update(TESTING=True, DBE_SPOOL=str(spool))
    app.jinja_loader = ChoiceLoader([
        DictLoader({"base.html": "<main>{% with m=get_flashed_messages(with_categories=true) %}{% for c, t in m %}"
                                 "<p class='flash {{ c }}'>{{ t }}</p>{% endfor %}{% endwith %}"
                                 "{% block content %}{% endblock %}</main>"}),
        FileSystemLoader(str(HERE / "templates"))])
    bp = Blueprint("dbeditor", "dbeditor", url_prefix="/db")
    bp.add_url_rule("/", "index", lambda: "hub")
    bp.add_url_rule("/apply", "apply_page", lambda: "apply")
    ctx = {"app": app, "db": fake.db, "rows": fake.rows, "one": None, "login_required": lambda view: view,
           "game_text": game_text, "server_version": lambda: "2.22.0", "panel_name": lambda: "Serwer testowy"}
    for module in (items, skills, drops, chests, mobs, spawns, regen, shops, refine, attrs, exptable, fishing, config):
        module.install(bp, ctx)
    app.register_blueprint(bp)
    return app


class Base(unittest.TestCase):
    def setUp(self):
        self.sections = list(dbeditor.SECTIONS)
        self.tables = dict(common_items.TABLES)
        self.tmp = tempfile.TemporaryDirectory()
        self.a_spool, self.b_spool = Path(self.tmp.name) / "a", Path(self.tmp.name) / "b"
        self.a, self.b = GameDB(), GameDB()
        for fake, spool in ((self.a, self.a_spool), (self.b, self.b_spool)):
            stock(fake)
            stock_spool(spool)
        common_items._CTX.clear()
        common_items._STATE.update(table_ready=True, history_installed=False)
        items._EXTRA_STATE["ready"] = False
        self.app = make_app(self.a, self.a_spool)
        self.client = self.app.test_client()
        with self.client.session_transaction() as session:
            session["seban_update_csrf"] = "tok"
            session["dbe_drops_csrf"] = "tok"
        self.server("a")

    def tearDown(self):
        dbeditor.SECTIONS[:] = self.sections
        common_items.TABLES.clear()
        common_items.TABLES.update(self.tables)
        common_items._STATE.update(table_ready=False, history_installed=False)
        self.tmp.cleanup()

    def server(self, name):
        fake, spool = (self.a, self.a_spool) if name == "a" else (self.b, self.b_spool)
        common_items._CTX.update(rows=fake.rows, db=fake.db)
        self.app.config["DBE_SPOOL"] = str(spool)
        return fake, spool

    def post(self, url, data, **kw):
        return self.client.post(url, data=dict(data, dbe_csrf="tok"), follow_redirects=True, **kw)

    def one(self, fake, sql, params=()):
        return fake.rows(sql, params)[0]

    def change_server_a(self):
        """Edits made the way the editor's pages make them."""
        with self.app.test_request_context():
            common_items.save_rows("world.item_proto", [(19, {"gold": 2000, "locale_name": "Złoty Miecz+9"})],
                                   label_of=lambda k: "Miecz+9")
            batch, _ = common_items.save_rows("world.item_proto", [(20, {"gold": 5})])
            common_items.revert(batch=batch)                          # undone: not in the export
            common_items.save_rows("world.mob_proto", [(101, {"max_hp": 250, "ai_flag": "AGGR"})])
            common_items.save_rows("world.skill_proto", [(1, {"szPointPoly": "-(1.5*atk)"})])
            common_items.write_rows("world.shop_item", inserts=[{"shop_vnum": 9001, "item_vnum": 27001, "count": 5}],
                                    deletes=[{"shop_vnum": 9001, "item_vnum": 11, "count": 1}])
            clone = {c: 0 for c in refine.ROW_COLS}
            clone.update(id=30000, vnum0=27002, count0=3, cost=5000, prob=70)
            common_items.write_rows("world.refine_proto", inserts=[clone])
            common_items.save_rows("world.item_proto", [(19, {"refine_set": 30000})])
            common_items.save_rows("world.refine_proto", [(1, {"cost": 1500})])
            items.ensure_extra_table(self.a.rows)
            common_items.write_rows("world.item_extra_apply",
                                    inserts=[{"vnum": 19, "slot": 1, "apply_type": 12, "apply_value": 5}])
            common_items.save_rows("world.item_attr", [("POINT_ST", {"prob": 20})])
            common_items.save_rows("common.exp_table", [(5, {"exp": 2000})])
            self.a.rows("INSERT INTO common.exp_table VALUES (120, ?)", (exptable.builtin(120),))
            common_items.save_rows("common.exp_table", [(120, {"exp": 2600000000})])
            group = {"items": [{"item": "27002", "count": "1", "prob": "80"}], "kill_drop": "0", "level_limit": "0"}
            df.write_custom(self.a_spool, "mob", df.render_mob_custom({(101, "drop"): group}, "test"), "test")
            entries = fishing.parse_lua(fishing.SNAPSHOT.read_text(encoding="latin-1"))
            entries[0]["weight"] += 7
            df.write_custom(self.a_spool, "fishing", fishing.render_custom(entries, "test"), "test")
            regen.write_custom(self.a_spool, "metin2_map_a1", REGEN_BASE.replace("\t1\t8015", "\t4\t8015"), "test")
            spawnfiles.write_custom(self.a_spool, "map.metin2_map_a1.boss",
                                    BOSS_BASE.replace("30m", "60m").encode("latin-1"), "test")

    def export(self, **args):
        response = self.client.get("/db/config/eksport.txt", query_string=args)
        self.assertEqual(response.status_code, 200)
        return response.get_data(as_text=True)

    def body(self, text):
        """The text without what may differ between servers: the date, the
        comments of the records (the history's labels)."""
        return "\n".join(re.sub(r"\t# .*$", "", l) for l in text.splitlines() if not l.startswith("# data:"))


class FormatTests(unittest.TestCase):
    def test_blocks_round_trip(self):
        for data in (b"", b"a\r\nb\r\n", b"a\nb", b"\r\n", b"one line", "zażółć\n".encode("utf-8"),
                     "Gr\xfcn\r\n".encode("latin-1"), b"mixed\r\nends\n", b"\x00\x01binary", b"bare\rcr"):
            attrs, lines = config.encode_block(data)
            self.assertEqual(config.decode_block({k: str(v) for k, v in attrs.items()}, lines), data, data)
        self.assertEqual(config.encode_block(b"a\r\nb\r\n")[0]["enc"], "ascii")
        self.assertEqual(config.encode_block(b"\x00\x01binary")[0]["enc"], "base64")

    def test_records(self):
        rec = config.parse_record('19\tlocale_name\t"Miecz+9" -> "Złoty \\"M\\"+9"\t# Miecz+9 · Nazwa')
        self.assertEqual((rec["key"], rec["col"], rec["old"], rec["new"], rec["label"]),
                         ("19", "locale_name", "Miecz+9", 'Złoty "M"+9', "Miecz+9"))
        rec = config.parse_record('9001:11:1 * {"count":1,"item_vnum":11,"shop_vnum":9001} -> -')
        self.assertEqual((rec["old"]["item_vnum"], rec["new"]), (11, None))
        self.assertEqual(config.parse_record("5 exp - -> -12")["new"], -12)
        for bad in ("19 gold 1000 2000", "19 gold 1000 -> abc", "19 gold 1.5 -> 2", "19 gold [1] -> 2",
                    "19 gold 1 -> 2 extra", "1 9 gold"):
            with self.assertRaises(ValueError, msg=bad):
                config.parse_record(bad)

    def test_compact_and_fences(self):
        text = f"# {config.FORMAT_TITLE} v1\n[items] world.item_proto\n19\tgold\t1000 -> 2000\n"
        short = config.compact(text)
        self.assertTrue(short.startswith("MT2009CFG1:"))
        wrapped = "```\n" + "\n".join(short[i:i + 60] for i in range(0, len(short), 60)) + "\n```"
        self.assertEqual(config.unpack(wrapped).strip(), text.strip())
        self.assertEqual(config.parse("```text\n" + text + "```")["parts"]["items"]["records"][0]["new"], 2000)
        with self.assertRaises(config.ConfigError):
            config.unpack("MT2009CFG1:" + short[len("MT2009CFG1:"):-12] + "xyz")
        with self.assertRaises(config.ConfigError):
            config.parse("MT2009 PLUS - konfiguracja edytora bazy danych v2\n")
        with self.assertRaises(config.ConfigError):
            config.parse("cokolwiek\n[items]\n")


class RoundTripTests(Base):
    def test_export_lists_the_net_changes(self):
        self.change_server_a()
        text = self.export()
        self.assertTrue(text.startswith("# MT2009 PLUS - konfiguracja edytora bazy danych v1\n# serwer: 2.22.0\n"))
        self.assertIn("19\tgold\t1000 -> 2000", text)
        self.assertIn('19\tlocale_name\t"Miecz+9" -> "Złoty Miecz+9"', text)
        self.assertNotIn("20\tgold", text)                             # undone in A
        self.assertIn('101\tai_flag\t"" -> "AGGR"', text)
        self.assertIn('9001:11:1\t*\t{"count":1,"item_vnum":11,"shop_vnum":9001} -> -', text)
        self.assertIn("120\texp\t2500000000 -> 2600000000", text)
        for section in ("[items] world.item_proto", "[extra] world.item_extra_apply", "[skills]", "[mobs]",
                        "[shops]", "[refine]", "[attrs]", "[exp]", "[drops]", "[fishing]", "[spawns]", "[regen]"):
            self.assertIn(section, text)
        self.assertIn("<<< mob enc=ascii eol=crlf", text)
        self.assertIn("<<< map.metin2_map_a1.boss", text)
        self.assertNotIn("[chests]", text)
        only = self.export(czesc=["mobs", "regen"])
        self.assertIn("[mobs]", only)
        self.assertNotIn("[items]", only)
        page = self.client.get("/db/config").get_data(as_text=True)
        self.assertIn("Eksport / import konfiguracji", page)
        self.assertIn("MT2009CFG1:", page)
        self.assertIn("Dodatkowe bonusy", page)

    def test_round_trip_preview_import_and_undo(self):
        self.change_server_a()
        text = self.export()
        b, b_spool = self.server("b")
        with self.app.test_request_context():                         # B's own change: a conflict
            common_items.save_rows("world.item_proto", [(19, {"gold": 1500})])
        # the preview writes nothing
        page = self.post("/db/config/podglad", {"tekst": config.compact(text)}).get_data(as_text=True)
        self.assertIn("Podgląd importu", page)
        self.assertIn("tu zmienione inaczej", page)                     # 1500 is not the stock 1000
        self.assertRegex(page, r"1500</td><td>→</td><td class=\"dbc-val\"><b>2000</b>")
        self.assertIn("+1 / −1 linii", page)                            # the regen file
        self.assertEqual(self.one(b, "SELECT gold FROM world.item_proto WHERE vnum=19")["gold"], 1500)
        self.assertFalse(df.custom_path(b_spool, "mob").exists())
        payload = re.search(r'name="payload" value="([^"]+)"', page).group(1)
        parts = re.findall(r'name="czesc" value="(\w+)" checked', page)
        self.assertEqual(sorted(parts), sorted(["items", "extra", "skills", "mobs", "shops", "refine", "attrs", "exp",
                                                "drops", "fishing", "spawns", "regen"]))
        page = self.post("/db/config/importuj", {"payload": payload, "czesc": parts}).get_data(as_text=True)
        self.assertIn("Zaimportowano konfigurację", page)
        item = self.one(b, "SELECT gold, CAST(locale_name AS BLOB) AS n, refine_set FROM world.item_proto WHERE vnum=19")
        self.assertEqual((item["gold"], game_text(item["n"]), item["refine_set"]), (2000, "Złoty Miecz+9", 30000))
        self.assertEqual(self.one(b, "SELECT max_hp, ai_flag FROM world.mob_proto WHERE vnum=101"),
                         {"max_hp": 250, "ai_flag": "AGGR"})
        self.assertEqual(sorted((r["item_vnum"], r["count"]) for r in b.rows("SELECT * FROM world.shop_item")),
                         [(19, 1), (27001, 5)])
        self.assertEqual(self.one(b, "SELECT cost FROM world.refine_proto WHERE id=30000")["cost"], 5000)
        self.assertEqual(self.one(b, "SELECT exp FROM common.exp_table WHERE level=120")["exp"], 2600000000)
        self.assertEqual(self.one(b, "SELECT apply_value FROM world.item_extra_apply WHERE vnum=19")["apply_value"], 5)
        self.assertEqual(df.custom_path(b_spool, "mob").read_bytes(), df.custom_path(self.a_spool, "mob").read_bytes())
        self.assertEqual(regen.custom_path(b_spool, "metin2_map_a1").read_bytes(),
                         regen.custom_path(self.a_spool, "metin2_map_a1").read_bytes())
        # history: one batch, pending for "Zastosuj", files pending too
        batches = {r["batch"] for r in b.rows(f"SELECT batch FROM {common_items.HISTORY_TABLE} WHERE note LIKE 'Import%'")}
        self.assertEqual(len(batches), 1)
        self.assertTrue(any(c["tbl"] == "world.mob_proto" for c in common_items.pending_changes()))
        self.assertTrue(any(c["key"] == "mob" for c in df.pending_changes(b_spool)))
        # B's export now says what A's says (B's own 1500 is history before the stock: still 1000 -> 2000)
        self.assertEqual(self.body(self.export()).replace("Serwer testowy", ""),
                         self.body(text).replace("Serwer testowy", ""))
        # importing again changes nothing
        page = self.post("/db/config/podglad", {"tekst": text}).get_data(as_text=True)
        self.assertIn("Nie ma nic do zaimportowania", page)
        # one undo takes the whole import back
        page = self.client.get("/db/config").get_data(as_text=True)
        self.assertIn("Cofnij import", page)
        batch = batches.pop()
        page = self.post("/db/config/cofnij", {"batch": batch}).get_data(as_text=True)
        self.assertIn("Cofnięto import", page)
        self.assertEqual(self.one(b, "SELECT gold FROM world.item_proto WHERE vnum=19")["gold"], 1500)
        self.assertEqual(self.one(b, "SELECT max_hp FROM world.mob_proto WHERE vnum=101")["max_hp"], 100)
        self.assertEqual(sorted((r["item_vnum"], r["count"]) for r in b.rows("SELECT * FROM world.shop_item")),
                         [(11, 1), (19, 1)])
        self.assertEqual(b.rows("SELECT * FROM world.refine_proto WHERE id=30000"), [])
        self.assertEqual(b.rows("SELECT * FROM world.item_extra_apply"), [])
        self.assertFalse(df.custom_path(b_spool, "mob").exists())
        self.assertFalse(regen.custom_path(b_spool, "metin2_map_a1").exists())
        self.assertFalse(spawnfiles.custom_path_of(b_spool, "map.metin2_map_a1.boss").exists())
        page = self.post("/db/config/cofnij", {"batch": batch}).get_data(as_text=True)
        self.assertIn("już cofnięty", page)
        self.assertNotIn("[mobs]", self.export())

    def test_skip_conflicts_and_part_choice(self):
        self.change_server_a()
        text = self.export()
        b, b_spool = self.server("b")
        with self.app.test_request_context():
            common_items.save_rows("world.item_proto", [(19, {"gold": 1500})])
        page = self.post("/db/config/podglad", {"tekst": text, "pomin_konflikty": "1"}).get_data(as_text=True)
        payload = re.search(r'name="payload" value="([^"]+)"', page).group(1)
        self.post("/db/config/importuj", {"payload": payload, "czesc": ["items", "mobs"], "pomin_konflikty": "1"})
        self.assertEqual(self.one(b, "SELECT gold, refine_set FROM world.item_proto WHERE vnum=19"),
                         {"gold": 1500, "refine_set": 30000})              # the conflict kept, the rest imported
        self.assertEqual(self.one(b, "SELECT max_hp FROM world.mob_proto WHERE vnum=101")["max_hp"], 250)
        self.assertEqual(self.one(b, "SELECT cost FROM world.refine_proto WHERE id=1")["cost"], 1000)   # not chosen
        self.assertFalse(df.custom_path(b_spool, "mob").exists())


class InvalidInputTests(Base):
    HEAD = "# MT2009 PLUS - konfiguracja edytora bazy danych v1\n"

    def preview(self, text):
        return self.post("/db/config/podglad", {"tekst": text}).get_data(as_text=True)

    def test_not_a_config(self):
        self.assertIn("To nie jest konfiguracja edytora", self.preview("hello\n[items]\n19 gold 1 -> 2\n"))
        self.assertIn("ten panel zna tylko v1", self.preview(self.HEAD.replace("v1", "v7")))
        self.assertIn("Skrót jest uszkodzony", self.preview("MT2009CFG1:!!!notbase64"))
        self.assertIn("Pusty tekst", self.preview("   "))
        self.assertIn("Sesja formularza wygasła",
                      self.client.post("/db/config/podglad", data={"tekst": self.HEAD, "dbe_csrf": "zly"},
                                       follow_redirects=True).get_data(as_text=True))

    def test_bad_values_block_the_part(self):
        text = (self.HEAD + "[items] world.item_proto\n19\tgold\t1000 -> \"abc\"\n20\tgold\t1100 -> 1200\n"
                "[mobs] world.mob_proto\n101\tmax_hp\t100 -> 999\n99999\tmax_hp\t1 -> 5\n101\tnope\t1 -> 2\n"
                "[refine] world.refine_proto\n1\tprob\t90 -> 101\n"
                "[shops] world.shop_item\n9001:27001:5\t*\t- -> {\"count\":5,\"item_vnum\":27001}\n"
                "[nowa_czesc]\ncokolwiek\n"
                "[drops]\n<<< mob enc=ascii eol=crlf nl=1 sha256=00\n|Group\tx\n")
        page = self.preview(text)
        self.assertIn("nie jest liczbą całkowitą", page)
        self.assertIn("tej kolumny edytor nie zmienia", page)
        self.assertIn("dozwolone 0…100, podano 101", page)
        self.assertIn("wiersz musi mieć dokładnie kolumny", page)
        self.assertIn("nieznana część „nowa_czesc”", page)
        self.assertIn("bez zakończenia „&gt;&gt;&gt;”", page)
        self.assertIn("Potwór „99999” nie istnieje na tym serwerze", page)
        checked = re.findall(r'name="czesc" value="(\w+)" checked', page)
        self.assertEqual(checked, [])                                   # every part has an error
        payload = re.search(r'name="payload" value="([^"]+)"', page).group(1)
        page = self.post("/db/config/importuj", {"payload": payload, "czesc": ["items", "mobs", "refine"]})
        self.assertIn("Pominięto część z błędami", page.get_data(as_text=True))
        self.assertEqual(self.one(self.a, "SELECT gold FROM world.item_proto WHERE vnum=20")["gold"], 1100)
        self.assertEqual(self.one(self.a, "SELECT max_hp FROM world.mob_proto WHERE vnum=101")["max_hp"], 100)
        self.assertEqual(self.a.rows(f"SELECT * FROM {common_items.HISTORY_TABLE}"), [])

    def test_unknown_vnums_are_skipped_with_a_warning(self):
        text = (self.HEAD + "[mobs] world.mob_proto\n101\tmax_hp\t100 -> 999\n99999\tmax_hp\t1 -> 5\n"
                "[shops] world.shop_item\n9001:55555:1\t*\t- -> {\"count\":1,\"item_vnum\":55555,\"shop_vnum\":9001}\n"
                "[drops]\n<<< mob enc=ascii eol=lf nl=1\n|Group\tx\n|{\n|\tMob\t101\n|\tType\tdrop\n|\t1\t44444\t1\t5\n|}\n"
                ">>> mob\n")
        page = self.preview(text)
        self.assertIn("pominięto", page)
        self.assertIn("nie ma przedmiotu 55555", page)
        self.assertIn("nieznane na tym serwerze przedmioty: 44444", page)
        payload = re.search(r'name="payload" value="([^"]+)"', page).group(1)
        self.post("/db/config/importuj", {"payload": payload, "czesc": ["mobs", "shops", "drops"]})
        self.assertEqual(self.one(self.a, "SELECT max_hp FROM world.mob_proto WHERE vnum=101")["max_hp"], 999)
        self.assertEqual(len(self.a.rows("SELECT * FROM world.shop_item")), 2)
        self.assertFalse(df.custom_path(self.a_spool, "mob").exists())

    def test_upload_and_hub(self):
        self.change_server_a()
        text = self.export()
        self.server("b")
        page = self.post("/db/config/podglad", {"plik": (io.BytesIO(text.encode("utf-8")), "konfiguracja.txt")},
                         content_type="multipart/form-data").get_data(as_text=True)
        self.assertIn("Podgląd importu", page)
        self.assertIn("Serwer testowy", page)
        self.assertIn(("dbeditor.config_page", "📤", "Eksport / import konfiguracji"),
                      [s[:3] for s in dbeditor.SECTIONS])


if __name__ == "__main__":
    unittest.main()
