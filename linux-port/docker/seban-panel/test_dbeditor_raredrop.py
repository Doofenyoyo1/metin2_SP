"""MT2009_PLUS_RARE_DROP_SWITCHES_V1: python3 -m unittest test_dbeditor_raredrop

The database editor's "Kupony SM, szarfy, Cor" page (dbeditor/raredrop.py):
six switches kept as world event flags in player.quest (dwPID 0, 1 = off),
saved through common_items (history, undo, pending until "Zastosuj"). The
database is an in-memory SQLite (%s -> ?, INSERT IGNORE -> INSERT OR IGNORE).

With the repository around (server-patches next to linux-port) it also checks
the engine side: the flag names the engine reads are the page's, and a drop row
the operator saved for one Metin (Group MT2009_panel_*, m2-drops) is not a
built-in row, so it still drops with every switch off - while the game's own
row (Baronowna's Cor) is held back."""
import json
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
from dbeditor import common_items, raredrop
from dbeditor import dropfiles as df

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2] if len(HERE.parents) > 2 else HERE
RAREMOBRULES = REPO / "server-patches" / "raremobrules" / "edits.json"
DROPWIKI = REPO / "server-patches" / "dropwiki" / "edits.json"
M2_DROPS = HERE.parent / "game" / "bin" / "m2-drops"
MARKER = "MT2009_PLUS_RARE_DROP_SWITCHES_V1"


class SqliteDB:
    def __init__(self):
        self.con = sqlite3.connect(":memory:", isolation_level=None, check_same_thread=False)
        self.con.row_factory = sqlite3.Row
        for schema in ("world", "common", "player"):
            self.con.execute(f"ATTACH DATABASE ':memory:' AS {schema}")
        self.con.execute("CREATE TABLE player.quest (dwPID INTEGER NOT NULL DEFAULT 0, szName TEXT NOT NULL DEFAULT '', "
                         "szState TEXT NOT NULL DEFAULT '', lValue INTEGER NOT NULL DEFAULT 0, "
                         "PRIMARY KEY (dwPID, szName, szState))")
        self.con.execute(f"""CREATE TABLE {common_items.HISTORY_TABLE} (id INTEGER PRIMARY KEY AUTOINCREMENT,
            changed_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, who TEXT NOT NULL DEFAULT '', tbl TEXT NOT NULL,
            row_key TEXT NOT NULL, label TEXT NOT NULL DEFAULT '', col TEXT NOT NULL, old_value TEXT NULL,
            new_value TEXT NULL, batch TEXT NOT NULL, note TEXT NOT NULL DEFAULT '', reverted_in TEXT NULL,
            applied_at TEXT NULL)""")
        # a player's own quest row and the world's global switches (raretoggle)
        self.con.execute("INSERT INTO player.quest VALUES (7, 'collect_quest_lv30', '__status', 3)")
        self.con.execute("INSERT INTO player.quest VALUES (0, 'm2_alchemy_off', '', 0)")
        self.con.execute("INSERT INTO player.quest VALUES (0, 'm2_sash_off', '', 0)")

    def run(self, sql, params=()):
        sql = (sql.replace("%s", "?").replace(" FOR UPDATE", "").replace("NOW()", "CURRENT_TIMESTAMP")
               .replace("INSERT IGNORE", "INSERT OR IGNORE"))
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


def make_app(fake):
    app = Flask(__name__, static_folder=str(HERE / "static"))
    app.secret_key = "test-only"
    app.config.update(TESTING=True)
    app.jinja_loader = ChoiceLoader([
        DictLoader({"base.html": "<main>{% with m=get_flashed_messages(with_categories=true) %}{% for c, t in m %}"
                                 "<p class='flash {{ c }}'>{{ t }}</p>{% endfor %}{% endwith %}"
                                 "{% block content %}{% endblock %}</main>"}),
        FileSystemLoader(str(HERE / "templates"))])
    app.jinja_env.globals["static_asset_url"] = lambda name: "/static/" + name
    bp = Blueprint("dbeditor", "dbeditor", url_prefix="/db")
    bp.add_url_rule("/", "index", lambda: "hub")
    ctx = {"app": app, "db": fake.db, "rows": fake.rows, "one": None,
           "login_required": lambda view: view, "game_text": lambda value: value or ""}
    raredrop.install(bp, ctx)
    app.register_blueprint(bp)
    return app


class Base(unittest.TestCase):
    def setUp(self):
        self.sections = list(dbeditor.SECTIONS)
        self.tables = dict(common_items.TABLES)
        # the hub as the parts before it left it: the switches go right after "Drop"
        dbeditor.SECTIONS[:] = [("dbeditor.items", "", "Przedmioty", ""), ("dbeditor.drops_index", "", "Drop", ""),
                                ("dbeditor.chests_index", "", "Szkatułki", "")]
        self.fake = SqliteDB()
        common_items._CTX.clear()
        common_items._STATE.update(table_ready=True, history_installed=False)
        self.app = make_app(self.fake)
        self.client = self.app.test_client()
        with self.client.session_transaction() as session:
            session["seban_update_csrf"] = "tok"

    def tearDown(self):
        dbeditor.SECTIONS[:] = self.sections
        common_items.TABLES.clear()
        common_items.TABLES.update(self.tables)
        common_items._STATE.update(table_ready=False, history_installed=False)

    def form(self, off=(), **extra):
        """All six switches sent; the flags in off unchecked."""
        data = {"dbe_csrf": "tok"}
        for flag in raredrop.FLAGS:
            data["present_" + flag] = "1"
            if flag not in off:
                data[flag] = "1"
        data.update(extra)
        return data

    def flag(self, name):
        found = self.fake.rows("SELECT lValue FROM player.quest WHERE dwPID=0 AND szName=%s", (name,))
        return found[0]["lValue"] if found else None


class PageTests(Base):
    def test_hub_entry_after_drop(self):
        endpoints = [s[0] for s in dbeditor.SECTIONS]
        self.assertEqual(endpoints, ["dbeditor.items", "dbeditor.drops_index", "dbeditor.raredrop", "dbeditor.chests_index"])

    def test_page_defaults_all_on(self):
        page = self.client.get("/db/rzadki-drop").get_data(as_text=True)
        for text in ("Kupony SM z bossów", "Kupony SM z metinów", "Szarfy z bossów", "Szarfy z metinów",
                     "Cor Draconis z bossów", "Cor Draconis z metinów", "Baronówna Pająków", "skrzynia bossa"):
            self.assertIn(text, page)
        self.assertEqual(page.count("checked"), 6)
        self.assertNotIn("wyłączony</span>", page)
        # nothing is written by looking
        self.assertIsNone(self.flag("m2_cor_boss_off"))

    def test_save_pending_and_undo(self):
        page = self.client.post("/db/rzadki-drop", data=self.form(off=("m2_cor_boss_off", "m2_sash_metin_off")),
                                follow_redirects=True).get_data(as_text=True)
        self.assertIn("Zapisano 2", page)
        self.assertEqual(self.flag("m2_cor_boss_off"), 1)
        self.assertEqual(self.flag("m2_sash_metin_off"), 1)
        self.assertIsNone(self.flag("m2_sm_boss_off"))               # untouched switches get no row
        # a player's quest row is never touched
        self.assertEqual(self.fake.rows("SELECT lValue FROM player.quest WHERE dwPID=7")[0]["lValue"], 3)
        self.assertEqual(page.count("checked"), 4)
        self.assertIn("czeka na Zastosuj", page)
        pending = [c for c in common_items.pending_changes() if c["tbl"] == raredrop.TABLE]
        self.assertEqual(sorted(c["row_key"] for c in pending), ["m2_cor_boss_off", "m2_sash_metin_off"])
        cor = next(c for c in pending if c["row_key"] == "m2_cor_boss_off")
        self.assertEqual(cor["label"], "Cor Draconis z bossów")
        self.assertEqual(common_items.format_value(raredrop.TABLE, "lValue", cor["old_value"]), "włączony")
        self.assertEqual(common_items.format_value(raredrop.TABLE, "lValue", cor["new_value"]), "wyłączony")
        # the same form again changes nothing
        page = self.client.post("/db/rzadki-drop", data=self.form(off=("m2_cor_boss_off", "m2_sash_metin_off")),
                                follow_redirects=True).get_data(as_text=True)
        self.assertIn("Nic się nie zmieniło", page)
        # "Zastosuj" marks them applied
        common_items.mark_applied()
        self.assertEqual([c for c in common_items.pending_changes() if c["tbl"] == raredrop.TABLE], [])
        # the common undo puts one back (pending again)
        batch = common_items.history_batches(5, raredrop.TABLE)[0]["batch"]
        page = self.client.post("/db/historia/cofnij", data={"dbe_csrf": "tok", "batch": batch, "back": "/db/rzadki-drop"},
                                follow_redirects=True).get_data(as_text=True)
        self.assertIn("Cofnięto 2", page)
        self.assertEqual(self.flag("m2_cor_boss_off"), 0)
        self.assertEqual(self.flag("m2_sash_metin_off"), 0)
        self.assertEqual(len([c for c in common_items.pending_changes() if c["tbl"] == raredrop.TABLE]), 2)

    def test_back_on_and_partial_form(self):
        self.client.post("/db/rzadki-drop", data=self.form(off=raredrop.FLAGS))
        self.assertTrue(all(self.flag(f) == 1 for f in raredrop.FLAGS))
        # a form that only carries one switch leaves the rest alone
        self.client.post("/db/rzadki-drop", data={"dbe_csrf": "tok", "present_m2_sm_metin_off": "1", "m2_sm_metin_off": "1"})
        self.assertEqual(self.flag("m2_sm_metin_off"), 0)
        self.assertEqual(self.flag("m2_sm_boss_off"), 1)

    def test_bad_token_writes_nothing(self):
        data = self.form(off=("m2_cor_metin_off",))
        data["dbe_csrf"] = "zly"
        self.client.post("/db/rzadki-drop", data=data)
        self.assertIsNone(self.flag("m2_cor_metin_off"))

    def test_global_switch_warning(self):
        self.fake.con.execute("UPDATE player.quest SET lValue=1 WHERE szName='m2_alchemy_off'")
        page = self.client.get("/db/rzadki-drop").get_data(as_text=True)
        self.assertIn("Alchemia jest wyłączona dla całego świata", page)

    def test_history_on_the_page(self):
        self.client.post("/db/rzadki-drop", data=self.form(off=("m2_sm_boss_off",), note="test"))
        page = self.client.get("/db/rzadki-drop").get_data(as_text=True)
        history = page.split("Ostatnie zmiany przełączników")[1]
        self.assertIn("Kupony SM z bossów", history)
        self.assertIn("włączony", history)
        self.assertIn("Cofnij całą zmianę", history)
        self.assertIn("czeka na restart", history)


def engine_edits():
    edits = []
    for path in (RAREMOBRULES, DROPWIKI):
        edits += [e for e in json.loads(path.read_text(encoding="utf-8")) if MARKER in e["marker"]]
    return edits


# Mob drop sample: the game's own group for the boss Baronowna (Cor x3) and a
# Metin's plain group; the operator then saves the Metin's group with a sash.
IMAGE = """Group\tMT2009_dropedit_9706
{
\tmob\t9706
\tType\tdrop
\t1\t50255\t3\t20
\t2\t25040\t2\t40
}
Group\tMetin_8005
{
\tMob\t8005
\tType\tdrop
\t1\t27002\t1\t4
}
"""


def engine_rows(text, prefix):
    """(mob, item, builtin) of every "drop" row, the way the patched loader
    (ReadMonsterDropItemGroup) marks them: built-in unless the group's name
    starts with prefix."""
    rows = []
    for group in re.finditer(r"Group\s+(\S+)\s*\{(.*?)\}", text, re.S):
        name, body = group.group(1), group.group(2)
        mob = int(re.search(r"(?im)^\s*mob\s+(\d+)", body).group(1))
        if not re.search(r"(?im)^\s*type\s+drop\b", body):
            continue
        for item in re.findall(r"(?m)^\s*\d+\s+(\d+)\s+\d+\s+\S+", body):
            rows.append((mob, int(item), not name.startswith(prefix)))
    return rows


@unittest.skipUnless(RAREMOBRULES.is_file() and DROPWIKI.is_file(), "server-patches not next to the panel")
class EngineTests(unittest.TestCase):
    def test_engine_reads_the_pages_flags(self):
        code = "".join(e["new"] for e in engine_edits())
        flags = re.findall(r'"(m2_(?:sm|sash|cor)_(?:boss|metin)_off)"', code)
        self.assertEqual(set(flags), set(raredrop.FLAGS))
        for kind, boss, metin in (("SM", "m2_sm_boss_off", "m2_sm_metin_off"),
                                  ("SASH", "m2_sash_boss_off", "m2_sash_metin_off"),
                                  ("COR", "m2_cor_boss_off", "m2_cor_metin_off")):
            self.assertIn('{ "%s", "%s" }' % (boss, metin), code, kind)    # [kind][stone ? 1 : 0]
        # every switch is honoured where the game drops, previews and lists it
        markers = {e["marker"].split("(")[-1].rstrip(")") for e in engine_edits()}
        for part in ("kupon", "cor roll", "sash roll", "table rows", "boss chest", "built-in rows",
                     "preview kupon", "preview cor/sash", "wiki rows", "wiki kupon", "wiki cor", "wiki sash"):
            self.assertIn(part, markers)

    def test_manual_metin_row_survives_switches_off(self):
        code = "".join(e["new"] for e in engine_edits())
        prefix = re.search(r'stName\.compare\(0, (\d+), "([^"]+)"\) != 0', code)
        self.assertIsNotNone(prefix)
        self.assertEqual(int(prefix.group(1)), len(prefix.group(2)))
        self.assertTrue(df.render_mob_group(8005, "drop", {"items": []})[0].split("\t")[1].startswith(prefix.group(2)))
        # the drop loop skips only a built-in row whose switch is off
        self.assertIn("if (info.bBuiltin && Mt2009PlusRareTableRowOff(itemVnum, pkChr->IsStone()", code)
        live = IMAGE
        if shutil.which("sh") and M2_DROPS.is_file():
            # the real path: the panel's custom file merged by the game's m2-drops
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                locale = root / "share" / "locale" / "poland"
                locale.mkdir(parents=True)
                (root / "spool" / "drops").mkdir(parents=True)
                (locale / "mob_drop_item.txt").write_text(IMAGE.replace("\n", "\r\n"), encoding="ascii")
                (locale / "common_drop_item.txt").write_text("", encoding="ascii")
                (locale / "etc_drop_item.txt").write_text("", encoding="ascii")
                (locale / "drop_item_group.txt").write_text("", encoding="ascii")
                spool = root / "spool"
                group = {"items": [{"item": "27002", "count": "1", "prob": "1"},
                                   {"item": "85001", "count": "1", "prob": "10", "comment": "Szarfa +0"}]}
                df.write_custom(spool, "mob", df.render_mob_custom({(8005, "drop"): group}, "test"))
                env = dict(os.environ, M2_SHARE_DIR=str(root / "share"), M2_VAR_DIR=str(root / "var"),
                           M2_RATES_SPOOL=str(spool), PATH="/usr/bin:/bin")
                for command in ("prepare", "apply"):
                    subprocess.run(["sh", str(M2_DROPS), command], env=env, check=True, capture_output=True)
                live = (locale / "mob_drop_item.txt").read_text(encoding="ascii").replace("\r\n", "\n")
        else:
            live = IMAGE.replace("Group\tMetin_8005\n{\n\tMob\t8005\n\tType\tdrop\n\t1\t27002\t1\t4\n}\n",
                                 "\n".join(df.render_mob_group(8005, "drop", {"items": [
                                     {"item": "27002", "count": "1", "prob": "1"},
                                     {"item": "85001", "count": "1", "prob": "10"}]})) + "\n")
        rows = engine_rows(live, prefix.group(2))
        # switches all off: Mt2009PlusRareTableRowOff for a boss (9706) / a Metin (8005)
        sash = {85001}
        def held_back(mob, item, builtin):
            kind = "cor" if item == 50252 or 50255 <= item <= 50260 else ("sash" if item in sash else
                                                                            ("sm" if 80014 <= item <= 80018 else None))
            return builtin and kind is not None
        dropped = [(mob, item) for mob, item, builtin in rows if not held_back(mob, item, builtin)]
        self.assertIn((8005, 85001), dropped)                    # the operator's sash from one Metin
        self.assertNotIn((9706, 50255), dropped)                 # the game's own Cor from Baronowna
        self.assertIn((9706, 25040), dropped)                    # her other rows are not rare goods
        self.assertNotIn((8005, 27002, True), rows)              # the image's Metin group is replaced


if __name__ == "__main__":
    unittest.main()
