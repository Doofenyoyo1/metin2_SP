"""MT2009_PLUS_DB_EDITOR_V1: python3 test_dbeditor_shops.py

The database editor's "Sklepy NPC" (dbeditor/shops.py) and "Ulepszanie u
Kowala" (dbeditor/refine.py) through Flask's test client. The database is
SqliteDB below: an in-memory SQLite with the world / player schemas
attached, fed the MariaDB statements these modules send after a few
mechanical rewrites (CAST AS BINARY, FOR UPDATE, %s); names are kept as
text there. apply.sh is read to
check the parts' "rewritten at every start" lists against it."""
import os
import re
import sqlite3
import tempfile
import unittest
from unittest.mock import patch

os.environ.setdefault("DB_USER", "test")
os.environ.setdefault("DB_PASSWORD", "test")
os.environ.setdefault("DB_HOST", "127.0.0.1")
os.environ.setdefault("DB_PORT", "1")
os.environ.setdefault("DBEDITOR_SPOOL_ROOT", tempfile.mkdtemp(prefix="dbe-spool-"))

import app as panel  # noqa: E402
from dbeditor import common_items, refine, shops  # noqa: E402

APPLY = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "mariadb", "playerbot", "apply.sh")

SCHEMA = """
CREATE TABLE world.shop (vnum INTEGER PRIMARY KEY, name BLOB, npc_vnum INTEGER);
CREATE TABLE world.shop_item (shop_vnum INTEGER, item_vnum INTEGER, count INTEGER, UNIQUE (shop_vnum, item_vnum, count));
CREATE TABLE world.item_proto (vnum INTEGER PRIMARY KEY, locale_name BLOB, type INTEGER, subtype INTEGER, size INTEGER,
    stack INTEGER, flag INTEGER, gold INTEGER, shop_buy_price INTEGER, refined_vnum INTEGER, refine_set INTEGER);
CREATE TABLE player.mob_proto (vnum INTEGER PRIMARY KEY, locale_name BLOB);
CREATE TABLE world.refine_proto (id INTEGER PRIMARY KEY, vnum0 INTEGER DEFAULT 0, count0 INTEGER DEFAULT 0,
    vnum1 INTEGER DEFAULT 0, count1 INTEGER DEFAULT 0, vnum2 INTEGER DEFAULT 0, count2 INTEGER DEFAULT 0,
    vnum3 INTEGER DEFAULT 0, count3 INTEGER DEFAULT 0, vnum4 INTEGER DEFAULT 0, count4 INTEGER DEFAULT 0,
    cost INTEGER DEFAULT 0, src_vnum INTEGER DEFAULT 0, result_vnum INTEGER DEFAULT 0, prob INTEGER DEFAULT 100);
CREATE TABLE player.web_dbeditor_history (id INTEGER PRIMARY KEY AUTOINCREMENT,
    changed_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, who TEXT NOT NULL DEFAULT '', tbl TEXT NOT NULL,
    row_key TEXT NOT NULL, label TEXT NOT NULL DEFAULT '', col TEXT NOT NULL, old_value TEXT, new_value TEXT,
    batch TEXT NOT NULL, note TEXT NOT NULL DEFAULT '', reverted_in TEXT, applied_at TEXT);
"""


def name(text):
    # TEXT, so that LIKE works in SQLite; the code reads bytes and str alike (common_items.text_of).
    return text


class SqliteDB:
    def __init__(self):
        self.con = sqlite3.connect(":memory:", isolation_level=None, check_same_thread=False)
        self.con.row_factory = sqlite3.Row
        self.con.execute("ATTACH DATABASE ':memory:' AS world")
        self.con.execute("ATTACH DATABASE ':memory:' AS player")
        self.con.executescript(SCHEMA)
        self.statements = []

    @staticmethod
    def translate(sql):
        sql = " ".join(sql.split())
        sql = re.sub(r"CAST\((`?\w+`?) AS BINARY\)", r"\1", sql)
        sql = sql.replace(" FOR UPDATE", "").replace("NOW()", "CURRENT_TIMESTAMP")
        if sql.startswith("DELETE"):
            sql = sql.replace(" LIMIT 1", "")
        return sql.replace("%s", "?")

    def execute(self, sql, params=()):
        self.statements.append(" ".join(sql.split()))
        if sql.strip().startswith("CREATE TABLE IF NOT EXISTS player.web_dbeditor_history"):
            return [], 0
        cur = self.con.execute(self.translate(sql), [p for p in (params or ())])
        found = [dict(r) for r in cur.fetchall()] if cur.description else []
        return found, cur.rowcount

    def rows(self, sql, params=()):
        return self.execute(sql, params)[0]

    def one(self, sql, params=()):
        found = self.rows(sql, params)
        return found[0] if found else None

    def db(self):
        return Connection(self)

    def insert(self, table, **values):
        cols = ", ".join(values)
        self.con.execute(f"INSERT INTO {table} ({cols}) VALUES ({', '.join('?' * len(values))})", list(values.values()))


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

    def begin(self):
        self.fake.con.execute("BEGIN")

    def commit(self):
        self.fake.con.execute("COMMIT")

    def rollback(self):
        if self.fake.con.in_transaction:
            self.fake.con.execute("ROLLBACK")

    def cursor(self):
        return Cursor(self.fake)

    def close(self):
        pass


def seed(fake):
    item = lambda vnum, text, **kw: fake.insert("world.item_proto", vnum=vnum, locale_name=name(text), **{
        "type": 3, "subtype": 0, "size": 1, "stack": 200, "flag": 4, "gold": 0, "shop_buy_price": 0,
        "refined_vnum": 0, "refine_set": 0, **kw})
    item(27001, "Czerwona Mikstura(M)", gold=32, shop_buy_price=30)
    item(27002, "Czerwona Mikstura(D)", gold=96, shop_buy_price=90)
    item(50200, "Tobolek", gold=500, shop_buy_price=500, stack=1, flag=0)
    item(70063, "Transformuj kostium", gold=125000)
    item(70065, "Transfer bonusów", gold=10000000)
    item(55001, "Karma zwierzaka", gold=100)
    item(27992, "Biała Perła", gold=1000)
    item(27993, "Niebieska Perła", gold=1000)
    item(30138, "Pustynny Piasek", gold=10)
    item(27799, "Rybia Ość", gold=10)
    for k in range(10):  # Miecz +0..+9: recipes 19..27; the +0 recipe 19 is shared with the Zbroja +0
        item(10 + k, f"Miecz+{k}", type=1, size=2, stack=1, flag=0, gold=100 * (k + 1), shop_buy_price=100,
             refined_vnum=(11 + k) if k < 9 else 0, refine_set=(19 + k) if k < 9 else 0)
    for k in range(3):
        item(11200 + k, f"Zbroja+{k}", type=2, size=2, stack=1, flag=0, gold=1000, shop_buy_price=1000,
             refined_vnum=11201 + k if k < 2 else 0, refine_set=19 if k == 0 else (577 + k if k < 2 else 0))
    for k in range(9):
        fake.insert("world.refine_proto", id=19 + k, vnum0=27799 if k >= 4 else 0, count0=1 if k >= 4 else 0,
                    vnum1=30138 if k >= 6 else 0, count1=2 if k >= 6 else 0, cost=1000 * (k + 1), prob=90 - 8 * k)
    fake.insert("world.refine_proto", id=578, cost=5000, prob=50)
    fake.insert("world.refine_proto", id=7204, vnum0=30360, count0=8, cost=5000000, prob=50)
    fake.insert("world.shop", vnum=3, name=b"General Store Saleswoman", npc_vnum=9003)
    fake.insert("world.shop", vnum=999, name=b"beta", npc_vnum=0)
    fake.insert("world.shop", vnum=2, name=b"Fisherman", npc_vnum=9009)
    fake.insert("player.mob_proto", vnum=9003, locale_name=name("Handlarka Różności"))
    fake.insert("player.mob_proto", vnum=9009, locale_name=name("Rybak"))
    for shop, vnum, count in ((3, 27001, 5), (3, 27001, 20), (3, 27002, 5), (3, 50200, 1), (3, 70065, 1),
                              (3, 70063, 20), (999, 27001, 200), (2, 10, 1)):
        fake.insert("world.shop_item", shop_vnum=shop, item_vnum=vnum, count=count)


class Base(unittest.TestCase):
    def setUp(self):
        self.fake = SqliteDB()
        seed(self.fake)
        common_items._STATE["table_ready"] = False
        common_items._CTX.update(rows=self.fake.rows, db=self.fake.db)
        self.patches = [
            patch.object(panel, "settings", return_value={"setup_complete": "1", "auth_enabled": "0"}),
            patch.object(panel, "check_all_notifications", return_value=None, create=True),
            patch.object(panel, "rows", side_effect=self.panel_rows),
        ]
        for p in self.patches:
            p.start()
        panel.app.config["TESTING"] = True
        self.client = panel.app.test_client()
        with self.client.session_transaction() as sess:
            sess["seban_update_csrf"] = "tok"

    def tearDown(self):
        for p in self.patches:
            p.stop()

    def panel_rows(self, sql, params=()):
        if re.search(r"world\.|player\.mob_proto|web_dbeditor_history", sql):
            return self.fake.rows(sql, params)
        return []

    def post(self, url, data, **kw):
        data = dict(data)
        data.setdefault("dbe_csrf", "tok")
        return self.client.post(url, data=data, **kw)

    def goods(self, shop=3):
        return [(r["item_vnum"], r["count"]) for r in self.fake.rows(
            "SELECT item_vnum, count FROM world.shop_item WHERE shop_vnum=%s ORDER BY item_vnum, count", (shop,))]

    def proto(self, vnum, col):
        return self.fake.one(f"SELECT {col} FROM world.item_proto WHERE vnum=%s", (vnum,))[col]

    def recipe(self, rid):
        return self.fake.one("SELECT * FROM world.refine_proto WHERE id=%s", (rid,))

    def history(self):
        return self.fake.rows("SELECT * FROM player.web_dbeditor_history ORDER BY id")


class ShopTests(Base):
    def shop_form(self, shop=3, **changes):
        """What the shop page's form sends unchanged, plus changes."""
        lines = self.goods(shop)
        form = {"action": "save", "rows_n": str(len(lines)), "new_n": "1"}
        for i, (vnum, count) in enumerate(lines):
            form[f"item_{i}"], form[f"count0_{i}"], form[f"count_{i}"] = str(vnum), str(count), str(count)
        for vnum in {v for v, _c in lines}:
            form[f"gold_{vnum}"] = str(self.proto(vnum, "gold"))
            form[f"buy_{vnum}"] = str(self.proto(vnum, "shop_buy_price"))
        form.update(changes)
        return form

    def test_hub_and_list(self):
        hub = self.client.get("/db/").get_data(as_text=True)
        self.assertIn("Sklepy NPC", hub)
        self.assertIn("Ulepszanie u Kowala", hub)
        page = self.client.get("/db/shops").get_data(as_text=True)
        self.assertIn("Handlarka Różności", page)
        self.assertIn("Rybak", page)
        self.assertIn("wioski M1", page)
        self.assertIn("kupują tu boty", page)
        self.assertIn("beta", page)  # a quest shop, by its own name
        page = self.client.get("/db/shops?q=Tobolek").get_data(as_text=True)
        self.assertIn("Handlarka Różności", page)
        self.assertNotIn(">Rybak<", page)

    def test_shop_page(self):
        page = self.client.get("/db/shops/3").get_data(as_text=True)
        self.assertIn("Czerwona Mikstura(M)", page)
        self.assertIn("640 Yang", page)          # 32 x 20
        self.assertIn("dbs-window", page)
        self.assertIn("Transfer bonusów kostiumu ×1", page)  # apply.sh adds it at every start
        self.assertIn("Boty kupują tu", page)
        self.assertEqual(self.client.get("/db/shops/77").status_code, 404)

    def test_add_remove_and_count_with_history_and_undo(self):
        form = self.shop_form(new_item_0="27002", new_count_0="20", del_3="1", count_0="10")
        res = self.post("/db/shops/3", form, follow_redirects=True)
        text = res.get_data(as_text=True)
        self.assertIn("Zapisano", text)
        self.assertEqual(self.goods(), [(27001, 10), (27001, 20), (27002, 5), (27002, 20), (70063, 20), (70065, 1)])
        hist = self.history()
        self.assertEqual({h["col"] for h in hist}, {"*"})
        self.assertEqual(len({h["batch"] for h in hist}), 1)
        self.assertTrue(any(h["label"] == "Handlarka Różności · Tobolek ×1" and h["new_value"] is None for h in hist))
        pending = common_items.pending_changes()
        self.assertEqual(len(pending), 4)  # 27001x5 out, 27001x10 in, 50200 out, 27002x20 in
        apply_page = self.client.get("/db/apply").get_data(as_text=True)
        self.assertIn("towar w sklepie NPC", apply_page)
        self.assertIn("w ofercie ×20", apply_page)
        self.assertIn("Tobolek", self.client.get("/db/historia").get_data(as_text=True))
        res = self.post("/db/historia/cofnij", {"batch": hist[0]["batch"]}, follow_redirects=True)
        self.assertIn("Cofnięto", res.get_data(as_text=True))
        self.assertEqual(self.goods(), [(27001, 5), (27001, 20), (27002, 5), (50200, 1), (70063, 20), (70065, 1)])
        self.assertEqual(common_items.pending_changes(), [])

    def test_prices_are_per_item_and_checked(self):
        res = self.post("/db/shops/3", self.shop_form(gold_27001="40", buy_27001="35"), follow_redirects=True)
        text = res.get_data(as_text=True)
        self.assertEqual((self.proto(27001, "gold"), self.proto(27001, "shop_buy_price")), (40, 35))
        self.assertIn("zmieniła się też w innych sklepach", text)  # shop 999 sells it too
        cols = {h["col"] for h in self.history()}
        self.assertEqual(cols, {"gold", "shop_buy_price"})
        # A buy-back paying more than the price: free yang - refused.
        res = self.post("/db/shops/3", self.shop_form(buy_27001="400"), follow_redirects=True)
        self.assertIn("darmowy Yang", res.get_data(as_text=True))
        self.assertEqual(self.proto(27001, "shop_buy_price"), 35)
        res = self.post("/db/shops/3", self.shop_form(gold_27002="0"), follow_redirects=True)
        self.assertIn("cena 0", res.get_data(as_text=True))
        self.assertEqual(self.proto(27002, "gold"), 96)

    def test_refused_lines(self):
        before = self.goods()
        cases = [
            ({"new_item_0": "99999"}, "nie istnieje"),
            ({"new_item_0": "27001", "new_count_0": "20"}, "dwa razy"),
            ({"new_item_0": "50200", "new_count_0": "5"}, "najwyżej po 1"),
            ({"new_item_0": "55001"}, "usuwa go ze wszystkich"),
            ({"count_0": "0"}, "dozwolone 1"),
        ]
        for changes, expected in cases:
            res = self.post("/db/shops/3", self.shop_form(**changes), follow_redirects=True)
            text = res.get_data(as_text=True)
            self.assertIn("Nic nie zapisano", text, changes)
            self.assertIn(expected, text, changes)
        self.assertEqual(self.goods(), before)
        self.assertEqual(self.history(), [])

    def test_window_is_5x8(self):
        for k in range(40):
            self.fake.insert("world.item_proto", vnum=60000 + k, locale_name=name(f"Rzecz {k}"), type=3, subtype=0,
                             size=1, stack=200, flag=4, gold=10, shop_buy_price=1, refined_vnum=0, refine_set=0)
        form = self.shop_form(new_n="35", **{f"new_item_{j}": str(60000 + j) for j in range(35)})
        res = self.post("/db/shops/3", form, follow_redirects=True)
        self.assertIn("najwyżej 40 pozycji", res.get_data(as_text=True))
        # Size-3 items: 5 columns x 2 rows of three fit, and the rest does not.
        lines = [{"item_vnum": 1, "count": 1}] * 11
        placed = shops.layout(lines, {1: {"size": 3}})
        self.assertEqual(sum(p["fits"] for p in placed), 10)
        self.assertEqual((placed[5]["row"], placed[5]["col"]), (4, 1))

    def test_an_overflowing_shop_may_not_get_worse(self):
        for k in range(11):  # 11 two-handed swords (3 fields high): one is left out of the window already
            self.fake.insert("world.item_proto", vnum=3000 + 10 * k, locale_name=name(f"Glewia {k}"), type=1, subtype=3,
                             size=3, stack=1, flag=0, gold=100, shop_buy_price=10, refined_vnum=0, refine_set=0)
            self.fake.insert("world.shop_item", shop_vnum=999, item_vnum=3000 + 10 * k, count=1)
        res = self.post("/db/shops/999", self.shop_form(999, gold_3000="150"), follow_redirects=True)
        self.assertIn("Już wcześniej nie mieściło się", res.get_data(as_text=True))
        self.assertEqual(self.proto(3000, "gold"), 150)
        self.fake.insert("world.item_proto", vnum=3200, locale_name=name("Glewia 12"), type=1, subtype=3,
                         size=3, stack=1, flag=0, gold=100, shop_buy_price=10, refined_vnum=0, refine_set=0)
        res = self.post("/db/shops/999", self.shop_form(999, new_item_0="3200"), follow_redirects=True)
        self.assertIn("Nie mieszczą się w oknie", res.get_data(as_text=True))
        self.assertNotIn((3200, 1), self.goods(999))
        self.post("/db/shops/999", self.shop_form(999, new_item_0="50200"))  # one field is still free
        self.assertIn((50200, 1), self.goods(999))

    def test_removing_a_boot_line_warns(self):
        res = self.post("/db/shops/3", self.shop_form(del_5="1"), follow_redirects=True)
        self.assertIn("wróci do sklepu", res.get_data(as_text=True))
        self.assertNotIn((70065, 1), self.goods())

    def test_mass_prices(self):
        res = self.post("/db/shops/3", {"action": "mass", "factor": "0,8"}, follow_redirects=True)
        text = res.get_data(as_text=True)
        # 27001: 32 -> 26 while the NPC pays back 30/5: still fine; 50200: 500 -> 400 < 500/5*0.97? no: 97 < 400.
        self.assertIn("Ceny ×0.8", text)
        self.assertEqual(self.proto(27001, "gold"), 26)
        self.assertEqual(self.proto(70065, "gold"), 8000000)
        self.assertEqual(self.proto(27001, "shop_buy_price"), 30)
        # x0.05 would let 50200 (buy-back 500) be bought for 25 and sold for 97: refused...
        res = self.post("/db/shops/3", {"action": "mass", "factor": "0.05"}, follow_redirects=True)
        self.assertIn("darmowy Yang", res.get_data(as_text=True))
        self.assertEqual(self.proto(50200, "gold"), 400)
        # ... unless the buy-back goes down with it.
        self.post("/db/shops/3", {"action": "mass", "factor": "0.05", "with_buy": "1"})
        self.assertEqual((self.proto(50200, "gold"), self.proto(50200, "shop_buy_price")), (20, 25))
        res = self.post("/db/shops/3", {"action": "mass", "factor": "abc"}, follow_redirects=True)
        self.assertIn("Mnożnik", res.get_data(as_text=True))

    def test_csrf(self):
        res = self.client.post("/db/shops/3", data=self.shop_form(del_0="1"), follow_redirects=True)
        self.assertIn("Sesja formularza wygasła", res.get_data(as_text=True))
        self.assertIn((27001, 5), self.goods())


class RefineTests(Base):
    def chain_form(self, vnum=10, **changes):
        page = self.client.get(f"/db/refine/{vnum}").get_data(as_text=True)
        form = {}
        for m in re.finditer(r'<input[^>]*name="([^"]+)"[^>]*>', page):
            tag = m.group(0)
            if 'type="radio"' in tag and "checked" not in tag:
                continue
            if 'type="checkbox"' in tag:
                continue
            value = re.search(r'value="([^"]*)"', tag)
            form[m.group(1)] = value.group(1) if value else ""
        form.pop("preset_amount", None)
        form.update(changes)
        return form

    def test_search_and_chain(self):
        page = self.client.get("/db/refine?q=Miecz").get_data(as_text=True)
        self.assertIn("Miecz", page)
        self.assertIn("+0 … +9", page)
        page = self.client.get("/db/refine/14").get_data(as_text=True)
        for k in range(10):
            self.assertIn(f"Miecz+{k}", page)
        self.assertIn("+8 → +9", page)
        self.assertIn("Rybia Ość", page)
        self.assertIn("używa go też 1 inn.", page)  # recipe 19: also Zbroja+0
        self.assertIn('name="scope_19"', page)
        self.assertIn("bez Pustynny Piasek", page)
        self.assertEqual(self.client.get("/db/refine/77777").status_code, 404)

    def test_edit_in_place_and_history(self):
        form = self.chain_form(prob_8="25", cost_8="99000", mv_8_2="27992", mc_8_2="3")
        res = self.post("/db/refine/10", form, follow_redirects=True)
        self.assertIn("Zapisano", res.get_data(as_text=True))
        r = self.recipe(27)
        self.assertEqual((r["prob"], r["cost"], r["vnum2"], r["count2"]), (25, 99000, 27992, 3))
        hist = self.history()
        self.assertEqual({h["tbl"] for h in hist}, {"world.refine_proto"})
        self.assertEqual({h["row_key"] for h in hist}, {"27"})
        self.assertTrue(common_items.pending_changes())
        self.assertIn("Przepis ulepszenia", self.client.get("/db/historia").get_data(as_text=True))

    def test_shared_recipe_family_clone_and_undo(self):
        form = self.chain_form(prob_0="100")  # recipe 19 is the Zbroja+0's too; default: only this family
        self.post("/db/refine/10", form)
        self.assertEqual(self.recipe(19)["prob"], 90)
        clone = self.recipe(30000)
        self.assertEqual((clone["prob"], clone["cost"]), (100, 1000))
        self.assertEqual(self.proto(10, "refine_set"), 30000)
        self.assertEqual(self.proto(11200, "refine_set"), 19)
        hist = self.history()
        self.assertEqual(len({h["batch"] for h in hist}), 1)
        self.assertEqual({(h["tbl"], h["col"]) for h in hist}, {("world.refine_proto", "*"), ("world.item_proto", "refine_set")})
        res = self.post("/db/historia/cofnij", {"batch": hist[0]["batch"]}, follow_redirects=True)
        self.assertIn("Cofnięto", res.get_data(as_text=True))
        self.assertIsNone(self.recipe(30000))
        self.assertEqual(self.proto(10, "refine_set"), 19)
        self.assertEqual(common_items.pending_changes(), [])

    def test_shared_recipe_for_all(self):
        self.post("/db/refine/10", self.chain_form(prob_0="77", scope_19="all"))
        self.assertEqual(self.recipe(19)["prob"], 77)
        self.assertIsNone(self.recipe(30000))
        self.assertEqual(self.proto(10, "refine_set"), 19)

    def test_validation(self):
        for changes, expected in (({"prob_3": "0"}, "szansa"), ({"prob_3": "101"}, "szansa"),
                                  ({"mv_3_0": "99999", "mc_3_0": "1"}, "nie istnieje"),
                                  ({"mv_3_0": "abc"}, "nie jest numerem"), ({"cost_3": "-5"}, "koszt")):
            res = self.post("/db/refine/10", self.chain_form(**changes), follow_redirects=True)
            text = res.get_data(as_text=True)
            self.assertIn("Nic nie zapisano", text, changes)
            self.assertIn(expected, text, changes)
        self.assertEqual(self.history(), [])
        # Two slots of one material are one slot of both counts.
        self.post("/db/refine/10", self.chain_form(mv_5_0="27799", mc_5_0="1", mv_5_1="27799", mc_5_1="2"))
        r = self.recipe(24)
        self.assertEqual((r["vnum0"], r["count0"], r["vnum1"], r["count1"]), (27799, 3, 0, 0))

    def test_presets_preview_only(self):
        res = self.post("/db/refine/10", self.chain_form(preset="chance_mul:1.2:7:9"), follow_redirects=True)
        text = res.get_data(as_text=True)
        self.assertIn("Podgląd", text)
        # +6 -> +7 is step 6 (recipe 25, prob 42 -> 50); +5 -> +6 untouched (50).
        self.assertIn('name="prob_6" min="1" max="100" value="50"', text)
        self.assertIn('name="prob_5" min="1" max="100" value="50"', text)
        self.assertEqual(self.recipe(25)["prob"], 42)
        res = self.post("/db/refine/10", self.chain_form(preset="drop:30138:1:99"), follow_redirects=True)
        text = res.get_data(as_text=True)
        self.assertNotIn('value="30138"', text)
        self.assertEqual(self.history(), [])
        res = self.post("/db/refine/10", self.chain_form(preset="custom", preset_kind="cost_mul", preset_amount="0,5",
                                                         preset_from="1", preset_to="1"), follow_redirects=True)
        self.assertIn('name="cost_0" min="0" max="2000000000" value="500"', res.get_data(as_text=True))

    def test_recipe_page_and_boot_warning(self):
        page = self.client.get("/db/refine/set/19").get_data(as_text=True)
        self.assertIn("Zbroja+0", page)
        self.assertIn("Miecz+0", page)
        res = self.post("/db/refine/set/7204", {"cost_r": "4000000", "prob_r": "60", "mv_r_0": "30138", "mc_r_0": "8"},
                        follow_redirects=True)
        text = res.get_data(as_text=True)
        self.assertIn("apply.sh", text)
        self.assertEqual((self.recipe(7204)["prob"], self.recipe(7204)["cost"]), (60, 4000000))


@unittest.skipUnless(os.path.isfile(APPLY), "apply.sh not next to the panel")
class ApplyShTests(unittest.TestCase):
    def setUp(self):
        with open(APPLY, encoding="utf-8", errors="replace") as f:
            self.text = f.read()

    def test_shop_rows_covered(self):
        removed = set()
        for where in re.findall(r"DELETE FROM world\.shop_item WHERE ([^;\"]+)", self.text):
            for group in re.findall(r"item_vnum IN \(([\d,\s]+)\)", where):
                removed.update(int(v) for v in group.split(","))
            for low, high in re.findall(r"item_vnum BETWEEN (\d+) AND (\d+)", where):
                removed.update((int(low), int(high)))
            self.assertTrue(re.search(r"item_vnum (IN|BETWEEN)", where), where)
        self.assertTrue(removed)
        for vnum in removed:
            self.assertTrue(shops.boot_removed(vnum), vnum)
        added = set()
        for values in re.findall(r"INTO world\.shop_item \(shop_vnum, item_vnum, count\) VALUES ([^;\"]+)", self.text):
            added.update(tuple(int(x) for x in t) for t in re.findall(r"\((\d+), (\d+), (\d+)\)", values))
        self.assertTrue(added)
        self.assertEqual(added, set(shops.BOOT_ADDED))
        self.assertNotRegex(self.text, r"UPDATE world\.shop_item|INSERT INTO world\.shop\b|UPDATE world\.shop\b")

    def test_refine_rows_covered(self):
        ids = set()
        for values in re.findall(r"INSERT INTO world\.refine_proto \([^)]*\) VALUES(.*?)ON DUPLICATE", self.text, re.S):
            ids.update(int(i) for i in re.findall(r"^\((\d+),", values.strip(), re.M))
        for low, high in re.findall(r"DELETE FROM world\.refine_proto WHERE id BETWEEN (\d+) AND (\d+)", self.text):
            ids.update(range(int(low), int(high) + 1))
        self.assertTrue(ids)
        for rid in ids:
            self.assertTrue(refine.boot_rule(rid), rid)
            self.assertLess(rid, refine.CLONE_FIRST_ID)


if __name__ == "__main__":
    unittest.main()
