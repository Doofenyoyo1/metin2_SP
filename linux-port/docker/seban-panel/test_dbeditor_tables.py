"""MT2009_PLUS_DB_EDITOR_V1: python3 -m unittest test_dbeditor_tables

The database editor's "Bonusy do zmiany i 6/7" (dbeditor/attrs.py), "Tabela
doświadczenia" (exptable.py) and "Łowienie ryb" (fishing.py) parts through
Flask's test client. The database is an in-memory SQLite (SqliteDB below:
%s -> ?, FOR UPDATE dropped, world/common/player attached), so the very
statements of common_items run; the game side of the fishing table
(bin/m2-fishing) runs under sh on a temporary share."""
import os
import re
import shutil
import sqlite3
import subprocess
import tempfile
import unittest
from pathlib import Path

from flask import Blueprint, Flask
from jinja2 import ChoiceLoader, DictLoader, FileSystemLoader

import dbeditor
from dbeditor import attrs, common_items, exptable, fishing
from dbeditor import dropfiles as df

HERE = Path(__file__).resolve().parent
M2_FISHING = HERE.parent / "game" / "bin" / "m2-fishing"
ENUM = "enum(" + ",".join(f"'{n}'" for n in attrs.POINT_NAMES) + ")"
KIND_COLS = ("weapon", "body", "wrist", "foots", "neck", "head", "shield", "ear", "costume_body", "costume_hair",
             "costume_weapon", "pendant", "glove")


class SqliteDB:
    def __init__(self):
        self.con = sqlite3.connect(":memory:", isolation_level=None, check_same_thread=False)
        self.con.row_factory = sqlite3.Row
        for schema in ("world", "common", "player"):
            self.con.execute(f"ATTACH DATABASE ':memory:' AS {schema}")
        kinds = ", ".join(f"{c} INTEGER NOT NULL DEFAULT 0" for c in KIND_COLS)
        levels = ", ".join(f"lv{i} INTEGER NOT NULL DEFAULT 0" for i in range(1, 6))
        for table in ("world.item_attr", "world.item_attr_rare"):
            self.con.execute(f"CREATE TABLE {table} (apply TEXT NOT NULL, prob INTEGER NOT NULL DEFAULT 0, {levels}, {kinds})")
        self.con.execute("CREATE TABLE common.exp_table (level INTEGER PRIMARY KEY, exp INTEGER NOT NULL DEFAULT 0)")
        self.con.execute("CREATE TABLE player.item_proto (vnum INTEGER PRIMARY KEY, locale_name TEXT, type INTEGER, "
                         "value0 INTEGER DEFAULT 0, value5 INTEGER DEFAULT 0)")
        self.con.execute(f"""CREATE TABLE {common_items.HISTORY_TABLE} (id INTEGER PRIMARY KEY AUTOINCREMENT,
            changed_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, who TEXT NOT NULL DEFAULT '', tbl TEXT NOT NULL,
            row_key TEXT NOT NULL, label TEXT NOT NULL DEFAULT '', col TEXT NOT NULL, old_value TEXT NULL,
            new_value TEXT NULL, batch TEXT NOT NULL, note TEXT NOT NULL DEFAULT '', reverted_in TEXT NULL,
            applied_at TEXT NULL)""")

    def run(self, sql, params=()):
        if "information_schema.COLUMNS" in sql:
            return [{"COLUMN_TYPE": ENUM}], 1
        sql = sql.replace("%s", "?").replace(" FOR UPDATE", "").replace("NOW()", "CURRENT_TIMESTAMP")
        cur = self.con.execute(sql, tuple(params or ()))
        return [dict(r) for r in cur.fetchall()], cur.rowcount

    def rows(self, sql, params=()):
        return self.run(sql, params)[0]

    def db(self):
        return Conn(self)


class Cursor:
    def __init__(self, fake):
        self.fake, self.result, self.rowcount = fake, [], 0

    def execute(self, sql, params=()):
        self.result, self.rowcount = self.fake.run(sql, params)

    def fetchone(self):
        return self.result[0] if self.result else None

    def fetchall(self):
        return self.result

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


class Conn:
    def __init__(self, fake):
        self.fake = fake

    def cursor(self):
        return Cursor(self.fake)

    def begin(self):
        self.fake.con.execute("BEGIN")

    def commit(self):
        self.fake.con.execute("COMMIT")

    def rollback(self):
        self.fake.con.execute("ROLLBACK")

    def close(self):
        pass


def seed(fake):
    def attr(table, apply, prob, lv, **kinds):
        cols = ["apply", "prob", "lv1", "lv2", "lv3", "lv4", "lv5"] + list(kinds)
        fake.con.execute(f"INSERT INTO {table} ({','.join(cols)}) VALUES ({','.join('?' * len(cols))})",
                         [apply, prob, *lv, *kinds.values()])
    attr("world.item_attr", "POINT_MAX_HP", 28, (300, 500, 800, 1000, 1500), body=5, wrist=5, costume_body=5)
    attr("world.item_attr", "POINT_ATTBONUS_HUMAN", 15, (1, 2, 3, 5, 10), weapon=5, wrist=5, head=5)
    attr("world.item_attr", "POINT_CRITICAL_PCT", 18, (1, 2, 3, 5, 10), weapon=5, foots=5)
    attr("world.item_attr", "POINT_ST", 11, (2, 4, 6, 8, 12), weapon=3)
    attr("world.item_attr_rare", "POINT_MAX_HP", 1, (150, 250, 500, 800, 850), weapon=5, body=5)
    for level in range(1, 120):           # level 120 has no row: the compiled value
        fake.con.execute("INSERT INTO common.exp_table VALUES (?, ?)", (level, exptable.BUILTIN[level]))
    items = [(27400 + 10 * lv, f"Wedka+{lv}", 13, fishing.ROD_BONUS_DEFAULT[lv], lv) for lv in range(10)]
    items += [(v, f"Przedmiot {v}", 3, 0, 0) for v in (27801, 27802, 27803, 27804, 27805, 27806, 27807, 27808, 27809,
                                                         27810, 27811, 27812, 27813, 27814, 27815, 27816, 27818, 27819,
                                                         27820, 27821, 27822, 27823, 70201, 70202, 70007, 70051, 70050,
                                                         50002, 70049, 70048, 70102, 50009, 50008, 70006, 27798, 25040,
                                                         30370, 30373, 30374, 71025, 71026)]
    fake.con.executemany("INSERT INTO player.item_proto VALUES (?, ?, ?, ?, ?)", items)


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
    ctx = {"app": app, "db": fake.db, "rows": fake.rows, "one": None,
           "login_required": lambda view: view, "game_text": lambda value: value or ""}
    for module in (attrs, exptable, fishing):
        module.install(bp, ctx)
    app.register_blueprint(bp)
    return app


class Base(unittest.TestCase):
    def setUp(self):
        self.sections = list(dbeditor.SECTIONS)
        self.tables = dict(common_items.TABLES)
        self.tmp = tempfile.TemporaryDirectory()
        self.spool = Path(self.tmp.name)
        self.fake = SqliteDB()
        seed(self.fake)
        common_items._CTX.clear()
        common_items._STATE.update(table_ready=True, history_installed=False)
        self.app = make_app(self.fake, self.spool)
        self.client = self.app.test_client()
        with self.client.session_transaction() as session:
            session["seban_update_csrf"] = "tok"
            session["dbe_drops_csrf"] = "tok"

    def tearDown(self):
        dbeditor.SECTIONS[:] = self.sections
        common_items.TABLES.clear()
        common_items.TABLES.update(self.tables)
        common_items._STATE.update(table_ready=False, history_installed=False)
        self.tmp.cleanup()

    def post(self, url, data):
        return self.client.post(url, data=dict(data, dbe_csrf="tok"), follow_redirects=True)

    def attr_row(self, table, apply):
        return self.fake.rows(f"SELECT * FROM {table} WHERE apply=%s", (apply,))[0]


class AttrsTests(Base):
    def test_numbering_matches_the_engine(self):
        # apply+0 is the POINT_* number: the ENUM position, named by items.POINT_LABELS
        self.assertEqual(attrs.POINT_NAMES.index("POINT_ST") + 1, 12)
        self.assertEqual(attrs.POINT_NAMES.index("POINT_ATTBONUS_HUMAN") + 1, 43)
        self.assertEqual(attrs.POINT_NAMES.index("POINT_MAGIC_ATT") + 1, 161)
        self.assertEqual(attrs.bonus_name(43)[0], "Silny przeciw ludziom")

    def test_kind_view(self):
        bonuses = [attrs.decorate(r, attrs.POINT_NAMES) for r in self.fake.rows("SELECT * FROM world.item_attr")]
        weapon = attrs.kind_view(bonuses, "weapon", 5)
        self.assertEqual([e["bonus"]["apply"] for e in weapon["entries"]],
                         ["POINT_CRITICAL_PCT", "POINT_ATTBONUS_HUMAN", "POINT_ST"])
        self.assertAlmostEqual(sum(e["chance"] for e in weapon["entries"]), 100.0)
        st = weapon["entries"][-1]
        self.assertEqual((st["level"], st["max_value"]), (3, 6))       # weapon column 3 -> lv3
        self.assertTrue(any("Tylko 3 bonusów" in w for w in weapon["warnings"]))
        self.assertIn("Na tym rodzaju nie wylosuje się żaden bonus.", attrs.kind_view(bonuses, "ear", 5)["warnings"])

    def test_overview_page(self):
        page = self.client.get("/db/attrs").get_data(as_text=True)
        for text in ("Na broni mogą się wylosować", "Silny przeciw ludziom", "Maks. PŻ", "apply.sh", "Siła"):
            self.assertIn(text, page)
        rare = self.client.get("/db/attrs?pula=rzadkie").get_data(as_text=True)
        self.assertIn("rare_6_7_v2", rare)
        self.assertEqual(self.client.get("/db/attrs?pula=nic").status_code, 404)

    def test_edit_save_history_pending_and_undo(self):
        page = self.client.get("/db/attrs/zwykle/POINT_ST").get_data(as_text=True)
        self.assertIn("numer bonusu w grze: <b>12</b>", page)
        form = {"prob": "20", "lv1": "3", "lv2": "4", "lv3": "6", "lv4": "8", "lv5": "15"}
        form.update({col: "0" for col in attrs.KIND_COLS})
        form.update(weapon="5", body="2")
        page = self.post("/db/attrs/zwykle/POINT_ST", form).get_data(as_text=True)
        self.assertIn("Zapisano", page)
        row = self.attr_row("world.item_attr", "POINT_ST")
        self.assertEqual((row["prob"], row["lv5"], row["weapon"], row["body"]), (20, 15, 5, 2))
        pending = [c for c in common_items.pending_changes() if c["tbl"] == "world.item_attr"]
        self.assertEqual(sorted(c["col"] for c in pending), ["body", "lv1", "lv5", "prob", "weapon"])
        self.assertIn("Siła", pending[0]["label"])
        self.assertEqual(common_items.format_value("world.item_attr", "body", "2"), "do poziomu 2")
        batch = self.fake.rows(f"SELECT batch FROM {common_items.HISTORY_TABLE}")[0]["batch"]
        with self.app.test_request_context():
            _new, message, ok = common_items.revert(batch=batch)
        self.assertTrue(ok, message)
        row = self.attr_row("world.item_attr", "POINT_ST")
        self.assertEqual((row["prob"], row["lv5"], row["weapon"], row["body"]), (11, 12, 3, 0))
        self.assertEqual([c for c in common_items.pending_changes() if c["tbl"] == "world.item_attr"], [])

    def test_validation(self):
        page = self.post("/db/attrs/zwykle/POINT_ST", {"lv5": "40000", "weapon": "7"}).get_data(as_text=True)
        self.assertIn("dozwolone 0…32767", page)
        self.assertIn("dozwolone 0…5", page)
        self.assertEqual(self.attr_row("world.item_attr", "POINT_ST")["lv5"], 12)

    def test_kind_add_remove_and_costume_warning(self):
        self.post("/db/attrs/rodzaj/zwykle", {"kind": "ear", "apply": "POINT_ST", "level": "4"})
        self.assertEqual(self.attr_row("world.item_attr", "POINT_ST")["ear"], 4)
        page = self.post("/db/attrs/rodzaj/zwykle", {"kind": "costume_body", "apply": "POINT_MAX_HP", "level": "0"})
        self.assertEqual(self.attr_row("world.item_attr", "POINT_MAX_HP")["costume_body"], 0)
        self.assertIn("apply.sh skopiuje", page.get_data(as_text=True))
        page = self.post("/db/attrs/rodzaj/rzadkie", {"kind": "shield", "apply": "POINT_MAX_HP", "level": "5"})
        self.assertEqual(self.attr_row("world.item_attr_rare", "POINT_MAX_HP")["shield"], 5)
        self.assertNotIn("apply.sh skopiuje", page.get_data(as_text=True))

    def test_new_bonus_is_inert_until_edited(self):
        page = self.post("/db/attrs/nowy/rzadkie", {"apply": "POINT_ATTBONUS_MONSTER"}).get_data(as_text=True)
        self.assertIn("Silny przeciw potworom", page)
        row = self.attr_row("world.item_attr_rare", "POINT_ATTBONUS_MONSTER")
        self.assertEqual((row["prob"], row["weapon"]), (0, 0))
        self.post("/db/attrs/nowy/rzadkie", {"apply": "POINT_LEVEL"})                 # technical: refused
        self.assertEqual(self.fake.rows("SELECT * FROM world.item_attr_rare WHERE apply='POINT_LEVEL'"), [])


class ExpTests(Base):
    def exp(self, level):
        found = self.fake.rows("SELECT exp FROM common.exp_table WHERE level=%s", (level,))
        return found[0]["exp"] if found else None

    def test_builtin_and_preset_math(self):
        self.assertEqual(exptable.BUILTIN[1], 300)
        self.assertEqual(exptable.BUILTIN[120], 2500000000)
        out = exptable.apply_preset({75: 1000, 99: 10}, 75, 99, 0.8)
        self.assertEqual((out[75], out[99], len(out)), (800, 8, 25))
        self.assertEqual(exptable.apply_preset({}, 119, 130, None), {119: 2490000000, 120: 2500000000})
        levels = [{"level": n, "exp": exptable.BUILTIN[n], "builtin": exptable.BUILTIN[n]} for n in range(1, 121)]
        chart = exptable.chart(levels)
        self.assertEqual(len(chart["points"]), 120)
        self.assertEqual(len(chart["cur"].split()), 120)

    def test_page_and_preset(self):
        page = self.client.get("/db/exp?vnum=95").get_data(as_text=True)
        self.assertIn("Tabela doświadczenia", page)
        self.assertIn('id="l95" class="hl"', page)
        self.assertIn("(wbudowane)", page)                         # level 120 has no row
        page = self.post("/db/exp", {"action": "preset0"}).get_data(as_text=True)
        self.assertIn("Zapisano 25 poziom", page)
        self.assertEqual(self.exp(75), round(exptable.BUILTIN[75] * 0.8))
        self.assertEqual(self.exp(74), exptable.BUILTIN[74])
        pending = [c for c in common_items.pending_changes() if c["tbl"] == "common.exp_table"]
        self.assertEqual(len(pending), 25)
        self.assertIn("Poziom 75", {c["label"] for c in pending})

    def test_manual_save_inserts_missing_level_and_undo(self):
        self.post("/db/exp", {"action": "save", "l120": "2400000000", "l5": "4300"})
        self.assertEqual(self.exp(120), 2400000000)
        pending = [c for c in common_items.pending_changes() if c["tbl"] == "common.exp_table"]
        self.assertEqual([(c["row_key"], c["old_value"], c["new_value"]) for c in pending],
                         [("120", "2500000000", "2400000000")])
        batch = self.fake.rows(f"SELECT batch FROM {common_items.HISTORY_TABLE}")[0]["batch"]
        with self.app.test_request_context():
            self.assertTrue(common_items.revert(batch=batch)[2])
        self.assertEqual(self.exp(120), 2500000000)                # back to the compiled value

    def test_bad_values(self):
        page = self.post("/db/exp", {"action": "save", "l10": "abc", "l11": "0"}).get_data(as_text=True)
        self.assertIn("nie jest liczbą", page)
        self.assertIn("dozwolone 1…4000000000", page)
        page = self.post("/db/exp", {"action": "custom", "from": "99", "to": "75", "factor": "0,8"}).get_data(as_text=True)
        self.assertIn("Podaj poziomy", page)
        self.assertEqual(self.exp(10), exptable.BUILTIN[10])
        page = self.post("/db/exp", {"action": "custom", "from": "100", "to": "100", "factor": "1,5"}).get_data(as_text=True)
        self.assertIn("2 500 000 000", page)                        # over the engine's own ceiling: warned
        self.assertEqual(self.exp(100), 3225000000)


class FishingTests(Base):
    def snapshot(self):
        return fishing.parse_lua(fishing.SNAPSHOT.read_text(encoding="utf-8"))

    def test_parse_snapshot_and_odds(self):
        entries = self.snapshot()
        self.assertEqual(len(entries), 40)
        self.assertNotIn("71282", [e["item"] for e in entries])            # commented out by the Dockerfile
        self.assertEqual(entries[0], fishing.entry("27802", 1, 0, 5, 0, 393, comment=""))
        self.assertIn("farba", [e["item"] for e in entries])
        for level in fishing.ROD_LEVELS:
            odds, total = fishing.chances(entries, level, fishing.ROD_BONUS_DEFAULT[level])
            self.assertGreater(total, 0, level)
            self.assertAlmostEqual(sum(odds.values()), 100.0)
        self.assertEqual(fishing.validate(entries, None, fishing.ROD_BONUS_DEFAULT), [])
        again = fishing.parse_custom(fishing.render_custom(entries, "test ł").decode("ascii"))
        self.assertEqual([{k: e[k] for k in fishing.LIMITS} for e in again],
                         [{k: e[k] for k in fishing.LIMITS} for e in entries])

    def test_validate(self):
        only_big = [fishing.entry("27802", 1, 0, 9, 50, 10)]
        problems = fishing.validate(only_big, {27802}, fishing.ROD_BONUS_DEFAULT)
        self.assertTrue(any("Wędka +0" in p for p in problems))
        self.assertTrue(fishing.validate([fishing.entry("123", 1, 0, 9, 0, 1)], {27802}, fishing.ROD_BONUS_DEFAULT))
        self.assertTrue(fishing.validate([fishing.entry("27802", 1, 5, 2, 0, 1)], None, fishing.ROD_BONUS_DEFAULT))

    def form(self, entries, **extra):
        data = {"row_count": str(len(entries)), "action": "save"}
        for i, e in enumerate(entries):
            for key in ("item", "count", "rod_min", "rod_max", "bonus", "weight"):
                data[f"r{i}_{key}"] = str(e[key])
        data.update(extra)
        return data

    def test_page_save_pending_reset(self):
        page = self.client.get("/db/fishing").get_data(as_text=True)
        for text in ("Co łowi każda wędka", "Farba do włosów", "Przedmiot 27802", "fishing.txt", "Bez zmian z panelu"):
            self.assertIn(text, page)
        entries = self.snapshot()
        entries[0]["weight"] = 500
        page = self.post("/db/fishing", self.form(entries, r5_delete="1")).get_data(as_text=True)
        self.assertIn("Zapisano tabelę łowienia (39 pozycji)", page)
        saved = fishing.parse_custom(df.custom_path(self.spool, "fishing").read_text(encoding="ascii"))
        self.assertEqual((len(saved), saved[0]["weight"]), (39, 500))
        self.assertEqual([p["key"] for p in df.pending_changes(self.spool)], ["fishing"])      # on /db/apply
        page = self.post("/db/fishing", self.form([fishing.entry("99999", 1, 0, 9, 0, 1)])).get_data(as_text=True)
        self.assertIn("nie ma w grze", page)
        self.assertEqual(len(fishing.parse_custom(df.custom_path(self.spool, "fishing").read_text())), 39)
        self.post("/db/fishing", {"action": "reset"})
        self.assertFalse(df.custom_path(self.spool, "fishing").exists())
        self.assertEqual(len(df.list_backups(self.spool, ["fishing"])), 2)
        self.post("/db/fishing", self.form(self.snapshot()))           # same as the image: no file
        self.assertFalse(df.custom_path(self.spool, "fishing").exists())


@unittest.skipUnless(shutil.which("sh") and M2_FISHING.is_file(), "no sh or m2-fishing")
class GameSideTests(unittest.TestCase):
    def run_m2(self, root, *commands):
        env = dict(os.environ, M2_SHARE_DIR=str(root / "share"), M2_VAR_DIR=str(root / "var"),
                   M2_RATES_SPOOL=str(root / "spool"), PATH="/usr/bin:/bin")
        for command in commands:
            subprocess.run(["sh", str(M2_FISHING), command], env=env, check=True, capture_output=True)

    def test_m2_fishing_takes_the_panels_table(self):
        fishing.register_file()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            lib = root / "share" / "locale" / "poland" / "quest" / "libs" / "fishing"
            lib.mkdir(parents=True)
            (root / "spool" / "fishing").mkdir(parents=True)
            image = fishing.SNAPSHOT.read_bytes()
            (lib / "fishing_drop.lua").write_bytes(image)
            spool = root / "spool"
            entries = fishing.parse_lua(image.decode())[:3] + [fishing.entry("farba", 1, 0, 9, 0, 5)]
            df.write_custom(spool, "fishing", fishing.render_custom(entries, "test"))
            self.run_m2(root, "prepare", "apply")
            live = (lib / "fishing_drop.lua").read_bytes().decode()
            self.assertEqual(fishing.parse_lua(live), [dict(e, comment="") for e in entries])
            self.assertIn('"farba", 1, 0, 9, 0, 5,', live)
            self.assertIn("local fishing_drop_data_length = 6", live)        # the rest of the image stays
            self.assertEqual((spool / "fishing" / "fishing_drop.base.lua").read_bytes(), image)
            self.assertEqual(df.live_state(spool, "fishing")["kind"], "ok")
            self.assertEqual(df.pending_changes(spool), [])
            # a rod that would catch nothing: rejected, the image's table goes back
            df.write_custom(spool, "fishing", fishing.render_custom([fishing.entry("27802", 1, 0, 9, 90, 5)], "bad"))
            self.run_m2(root, "apply")
            self.assertEqual((lib / "fishing_drop.lua").read_bytes(), image)
            state = df.live_state(spool, "fishing")
            self.assertEqual(state["kind"], "error")
            self.assertIn("would catch nothing", state["text"])
            # no file: the image's
            df.write_custom(spool, "fishing", None)
            self.run_m2(root, "apply")
            self.assertEqual(df.live_state(spool, "fishing")["kind"], "ok")


if __name__ == "__main__":
    unittest.main()
