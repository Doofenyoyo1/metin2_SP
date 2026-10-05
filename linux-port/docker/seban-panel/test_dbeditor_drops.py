"""MT2009_PLUS_DB_EDITOR_V1: python3 -m unittest test_dbeditor_drops

The database editor's Drop and Szkatułki parts: the parsers (round trips on
the game's real files when they are around - DBE_REAL_FILES or the test
stack's spool volume - and on small samples always), the pages and their
saves through the Flask test client on a temporary spool, and the game side
(m2-drops) taking what the panel wrote."""
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from flask import Blueprint, Flask
from jinja2 import ChoiceLoader, DictLoader, FileSystemLoader

import dbeditor
from dbeditor import chests, drops
from dbeditor import dropfiles as df

HERE = Path(__file__).resolve().parent
REAL_CANDIDATES = [os.environ.get("DBE_REAL_FILES", ""),
                   "/var/lib/docker/volumes/mt2009plustest_rates-spool/_data"]
M2_DROPS = HERE.parent / "game" / "bin" / "m2-drops"

MOB_SAMPLE = """Group\tWolf
{
\tMob\t101\t-- Dziki Pies
\tType\tdrop
\t1\t27001\t1\t4\t-- Czerwona Mikstura
\t2\t50011\t1\t0.4
}
Group\tWolf2
{
\tMob\t102
\tType\tdrop
\t1\t27001\t1\t4
\t2\t50011\t1\t0.4
}
Group\tWolfKill
{
\tMob\t101
\tType\tkill
\tkill_drop\t100
\t1\t27002\t2\t3\t0
\t2\t27003\t1\t1\t5
}
Group\tWolfKillIgnored
{
\tMob\t101
\tType\tkill
\tkill_drop\t5
\t1\t27004\t1\t1\t0
}
Group\tMetin
{
\tMob\t8001
\tType\tdrop
\t1\t27001\t1\t8
}
Group\tMetinMore
{
\tMob\t8001
\tType\tdrop
\t1\ts50011\t1\t2
}
Group\tLimit
{
\tMob\t8001
\tType\tlimit
\tlevel_limit\t30
\t1\t27005\t1\t50
}
"""
COMMON_SAMPLE = ("PAWN\t\t\t\t\t\tS_PAWN\t\t\t\t\t\tKNIGHT\t\t\t\t\t\tS_KNIGHT\t\t\t\t\t\r\n"
                 "\xc3\xca\t1\t15\t0.08\t27001\t5000\t\t1\t15\t0.1\t27001\t4000\t\t1\t15\t0.12\t27001\t3333\t\t1\t15\t0.32\t27001\t1250\r\n"
                 "\t5\t15\t0.04\t27002\t10000\t\t5\t15\t0.05\t27002\t8000\t\t5\t15\t0.06\t27002\t6666\t\t5\t15\t0.16\t27002\t2500\r\n")
ETC_SAMPLE = "30073\t1.80\r\n30034\t1.26\r\n"
CHEST_SAMPLE = """Group\tMoonlight
{
\tVnum\t50011
\t1\t27001\t1\t30\t0\t-- Czerwona Mikstura
\t2\t27002\t2\t10\t0
\t3\tgold\t1000\t60
}
Group\tPctBox
{
\tVnum\t50012
\tType\tpct
\t1\t27003\t1\t50
\t2\ts50011\t1\t10
}
"""


def real_dir():
    for candidate in REAL_CANDIDATES:
        if candidate and (Path(candidate) / "drops" / "mob_drop_item.base.txt").is_file():
            return Path(candidate)
    return None


class FakeDb:
    """item_proto: every VNUM below 900000 exists; mob_proto: the given ones."""

    def __init__(self, mob_vnums):
        self.mob_vnums = sorted(mob_vnums)

    def __call__(self, sql, params=()):
        if "FROM player.mob_proto" in sql:
            return [{"vnum": v, "locale_name": f"Potwor {v}", "name": "", "rank": 2 if v >= 8000 else 0,
                     "type": 2 if 8000 <= v < 8100 else 0, "level": v % 100 + 1, "drop_item": 30073 if v == 101 else 0}
                    for v in self.mob_vnums]
        if "FROM player.item_proto WHERE vnum IN" in sql:
            return [{"vnum": v, "locale_name": f"Przedmiot {v}", "type": 23 if 50000 <= v < 50100 else 1}
                    for v in params if v < 900000]
        if "FROM player.item_proto WHERE vnum=%s OR" in sql:
            vnum, like, _ = params
            if vnum > 0 and vnum < 900000:
                return [{"vnum": vnum, "locale_name": f"Przedmiot {vnum}", "type": 1}]
            if "Przedmiot" in like or "przedmiot" in like:
                return [{"vnum": 27001, "locale_name": "Przedmiot 27001", "type": 1}]
            return []
        raise AssertionError("unexpected SQL " + sql)


def make_app(spool, mob_vnums):
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
    ctx = {"app": app, "db": None, "rows": FakeDb(mob_vnums), "one": None,
           "login_required": lambda view: view, "game_text": lambda value: value or ""}
    drops.install(bp, ctx)
    chests.install(bp, ctx)
    app.register_blueprint(bp)
    return app


class ParserTests(unittest.TestCase):
    def test_mob_sample(self):
        groups = df.parse_mob_drop(MOB_SAMPLE)
        self.assertEqual(len(groups), 7)
        self.assertEqual(groups[0]["items"][0], {"item": "27001", "count": "1", "prob": "4", "rare": "0",
                                                 "comment": "Czerwona Mikstura"})
        effective = df.effective_mob_drops(groups, {})
        self.assertEqual(len(effective[(8001, "drop")]["items"]), 2)          # drop groups merge
        self.assertEqual(effective[(8001, "drop")]["names"], ["Metin", "MetinMore"])
        self.assertEqual(effective[(101, "kill")]["kill_drop"], "100")        # the first kill wins
        self.assertEqual(effective[(101, "kill")]["ignored"], 1)
        sets = df.build_mob_sets(effective)
        shared = [s for s in sets.values() if s["mobs"] == [101, 102]]
        self.assertEqual(len(shared), 1)                                      # 101 and 102 drop the same
        self.assertEqual(shared[0]["kind"], "drop")
        removed = df.effective_mob_drops(groups, {(101, "drop"): {"items": [], "type": "drop"}})
        self.assertNotIn((101, "drop"), removed)

    def test_mob_render_round_trip(self):
        group = {"type": "kill", "kill_drop": "50", "level_limit": "0",
                 "items": [{"item": "27002", "count": "2", "prob": "3", "rare": "7", "comment": "Zółta Mikstura ł"}]}
        text = "\r\n".join(df.render_mob_group(101, "kill", group))
        self.assertIn("-- Zolta Mikstura l", text)
        again = df.parse_mob_drop(text)[0]
        self.assertEqual((again["mob"], again["type"], again["kill_drop"]), (101, "kill", "50"))
        self.assertEqual(again["items"][0]["rare"], "7")
        self.assertEqual(df.mob_set_id(again), df.mob_set_id(dict(group)))

    def test_chances_and_multiplier(self):
        drop = {"type": "drop", "items": [{"item": "1", "prob": "4"}, {"item": "2", "prob": "300"}]}
        self.assertAlmostEqual(df.mob_chance("drop", drop["items"][0], drop), 1.0)
        scaled = df.scale_mob_group(drop, 2)
        self.assertEqual([e["prob"] for e in scaled["items"]], ["8", "400"])  # capped at 100%
        kill = {"type": "kill", "kill_drop": "100", "items": [{"prob": "3"}, {"prob": "1"}]}
        self.assertAlmostEqual(df.mob_chance("kill", kill["items"][0], kill), 0.75)
        self.assertEqual(df.scale_mob_group(kill, 2)["kill_drop"], "50")
        self.assertEqual(df.mob_file_prob("drop", 0.05), "0.2")
        form = {"r0_pct": "0,0000096", "r0_shown": "0,0000096", "r0_orig": "0.0000384"}
        self.assertEqual(df.form_chance(form, "r0_", "x", 4), ("0.0000384", 0.0000096))   # untouched: exact
        self.assertEqual(df.form_chance(dict(form, r0_pct="1"), "r0_", "x", 4), ("4", 1.0))
        with self.assertRaises(ValueError):
            df.form_chance(dict(form, r0_pct="101"), "r0_", "x", 4)
        self.assertEqual(df.mob_file_prob("limit", 12.5), "12.5")
        self.assertEqual(df.chance_text(0.05), "0,05% (ok. 1 na 2 000)")

    def test_common_round_trip_and_rank_edit(self):
        raw = COMMON_SAMPLE.encode("latin-1")
        table = df.parse_common(raw.decode("latin-1"))
        self.assertEqual(df.render_common(table), raw)
        self.assertEqual([sum(s["active"] for s in r) for r in table["ranks"]], [2, 2, 2, 2])
        slots = [s for s in table["ranks"][1] if s["active"]][:1] + [df.common_slot(10, 20, "2", 27003)]
        edited = df.common_set_rank(table, 1, slots)
        again = df.parse_common(df.render_common(edited).decode("latin-1"))
        self.assertEqual(again["ranks"][0], table["ranks"][0])               # other ranks untouched
        active = [s for s in again["ranks"][1] if s["active"]]
        self.assertEqual([(s["item"], s["pct"], s["n"]) for s in active], [("27001", "0.1", "4000"), ("27003", "2", "200")])
        self.assertEqual(again["ranks"][1][0]["label"], "S_PAWN")            # the label row stays

    def test_etc_and_drop_item_group(self):
        entries = df.parse_etc(ETC_SAMPLE)
        self.assertEqual(df.render_etc(entries), ETC_SAMPLE.encode())
        self.assertEqual(entries[0], {"item": "30073", "prob": "1.80", "active": True})
        groups = df.parse_drop_item_group("Group\tx\n{\n\tVnum\t1\n\tMob\t101\n\t1\t27001\t0.5\t2\n}\n")
        self.assertEqual(groups[0]["items"][0], {"item": "27001", "prob": "0.5", "count": "2"})

    def test_chests(self):
        groups = {g["vnum"]: g for g in df.parse_chests(CHEST_SAMPLE)}
        moon = groups[50011]
        self.assertEqual(len(moon["items"]), 3)
        self.assertAlmostEqual(df.chest_chance(moon, moon["items"][0]), 30.0)
        self.assertAlmostEqual(df.chest_chance(groups[50012], groups[50012]["items"][0]), 50.0)
        again = df.parse_chests("\r\n".join(df.render_chest_group(moon)))[0]
        self.assertEqual([(e["item"], e["count"], e["prob"]) for e in again["items"]],
                         [(e["item"], e["count"], e["prob"]) for e in moon["items"]])
        scaled = df.scale_chest_entries("", moon["items"], 2, {0})
        self.assertEqual([e["prob"] for e in scaled], ["60", "10", "60"])
        self.assertEqual([e["prob"] for e in df.scale_chest_entries("pct", groups[50012]["items"], 3)], ["100", "30"])


@unittest.skipUnless(real_dir(), "the game's real drop files are not around (set DBE_REAL_FILES)")
class RealFileTests(unittest.TestCase):
    def setUp(self):
        self.real = real_dir()

    def test_common_and_etc_byte_round_trip(self):
        raw = (self.real / "drops" / "common_drop_item.base.txt").read_bytes()
        self.assertEqual(df.render_common(df.parse_common(raw.decode("latin-1"))), raw)
        raw = (self.real / "drops" / "etc_drop_item.base.txt").read_bytes()
        self.assertEqual(df.render_etc(df.parse_etc(raw.decode("utf-8"))), raw)

    def test_mob_drop_every_group(self):
        text = (self.real / "drops" / "mob_drop_item.base.txt").read_bytes().decode("utf-8", "replace")
        groups = df.parse_mob_drop(text)
        self.assertEqual(len(groups), len(re.findall(r"(?im)^\s*group\s", text)))
        effective = df.effective_mob_drops(groups, {})
        sets = df.build_mob_sets(effective)
        self.assertEqual(sum(len(s["mobs"]) for s in sets.values()), len(effective))
        for (mob, kind), group in effective.items():  # what the editor writes reads back the same
            again = df.parse_mob_drop("\r\n".join(df.render_mob_group(mob, kind, group)))[0]
            self.assertEqual(df.mob_set_id(again), df.mob_set_id(group), (mob, kind))

    def test_chests_every_group(self):
        text = (self.real / "chests" / "special_item_group.base.txt").read_bytes().decode("utf-8", "replace")
        groups = df.parse_chests(text)
        self.assertGreater(len(groups), 100)
        for group in groups:
            again = df.parse_chests("\r\n".join(df.render_chest_group(group)))[0]
            self.assertEqual([(e["item"], e["count"], e["prob"], e["rare"]) for e in again["items"]],
                             [(e["item"], e["count"], e["prob"], e["rare"]) for e in group["items"]], group["vnum"])


class PageTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.spool = Path(self.tmp.name)
        (self.spool / "drops").mkdir()
        (self.spool / "chests").mkdir()
        (self.spool / "drops" / "mob_drop_item.base.txt").write_text(MOB_SAMPLE, encoding="utf-8")
        (self.spool / "drops" / "common_drop_item.base.txt").write_bytes(COMMON_SAMPLE.encode("latin-1"))
        (self.spool / "drops" / "etc_drop_item.base.txt").write_text(ETC_SAMPLE, encoding="ascii")
        (self.spool / "drops" / "drop_item_group.base.txt").write_text("", encoding="ascii")
        (self.spool / "chests" / "special_item_group.base.txt").write_text(CHEST_SAMPLE, encoding="utf-8")
        self.sections = list(dbeditor.SECTIONS)
        self.app = make_app(self.spool, [101, 102, 103, 8001])
        self.client = self.app.test_client()
        self.client.get("/db/drops")
        with self.client.session_transaction() as session:
            self.csrf = session["dbe_drops_csrf"]

    def tearDown(self):
        dbeditor.SECTIONS[:] = self.sections
        self.tmp.cleanup()

    def custom(self, key="mob"):
        path = df.custom_path(self.spool, key)
        return path.read_bytes().decode("latin-1") if path.exists() else None

    def set_id(self, mobs, kind="drop"):
        base = df.parse_mob_drop(MOB_SAMPLE)
        sets = df.build_mob_sets(df.effective_mob_drops(base, df.read_mob_custom(self.spool)))
        return next(s["id"] for s in sets.values() if s["mobs"] == mobs and s["kind"] == kind)

    def test_overview_and_search(self):
        page = self.client.get("/db/drops").get_data(as_text=True)
        for text in ("Zwykły drop", "Drop potworów (grupy)", "Drop specjalny", "Potwor 101", "Przedmiot 50011", "plik jest pusty"):
            self.assertIn(text, page)
        who = self.client.get("/db/drops?q=27001").get_data(as_text=True)
        self.assertIn("Kto dropi", who)
        self.assertIn("Zwykły drop", who)
        self.assertIn("Szkatułka", who)
        self.assertIn("1% (ok. 1 na 100)", who)
        mob = self.client.get("/db/drops?q=8001").get_data(as_text=True)
        self.assertIn("Od poziomu", mob)
        self.assertEqual(self.client.get("/db/api/items?q=27001").get_json()["items"][0]["vnum"], 27001)

    def test_save_whole_set(self):
        set_id = self.set_id([101, 102])
        self.assertEqual(self.client.get(f"/db/drops/set/{set_id}").status_code, 200)
        form = {"dbe_csrf": self.csrf, "action": "save", "member": ["101", "102"], "row_count": "3",
                "r0_item": "27001", "r0_count": "1", "r0_pct": "2",
                "r1_item": "50011", "r1_count": "1", "r1_pct": "0.1", "r1_delete": "1",
                "r2_item": "27009", "r2_count": "3", "r2_pct": "0,5"}
        answer = self.client.post(f"/db/drops/set/{set_id}", data=form)
        self.assertEqual(answer.status_code, 302)
        custom = df.read_mob_custom(self.spool)
        self.assertEqual(sorted(custom), [(101, "drop"), (102, "drop")])
        self.assertEqual([(e["item"], e["count"], e["prob"]) for e in custom[(102, "drop")]["items"]],
                         [("27001", "1", "8"), ("27009", "3", "2")])
        self.assertTrue(list((self.spool / "drops" / "backup").glob("mob_drop_item.custom.*.txt")))
        self.assertTrue((self.spool / "dbeditor" / "pending" / "drops-mob.json").exists())
        self.assertEqual([p["key"] for p in df.pending_changes(self.spool)], ["mob"])
        # the game applies it: the sha it reports matches - nothing pending, flag gone
        sha = df.file_sha(df.custom_path(self.spool, "mob"))
        (self.spool / "drops" / "status").write_text(f"state=ok\ntime=1\nsha={sha}\n", encoding="utf-8")
        self.assertEqual(df.pending_changes(self.spool), [])
        self.assertFalse((self.spool / "dbeditor" / "pending" / "drops-mob.json").exists())

    def test_multiply_copy_remove(self):
        set_id = self.set_id([101, 102])
        base = {"dbe_csrf": self.csrf, "member": ["101", "102"], "row_count": "2",
                "r0_item": "27001", "r0_count": "1", "r0_pct": "1", "r1_item": "50011", "r1_count": "1", "r1_pct": "0.1"}
        self.client.post(f"/db/drops/set/{set_id}", data=dict(base, action="multiply", factor="3"))
        custom = df.read_mob_custom(self.spool)
        self.assertEqual([e["prob"] for e in custom[(101, "drop")]["items"]], ["12", "1.2"])
        new_id = self.set_id([101, 102])
        self.client.post(f"/db/drops/set/{new_id}", data=dict(base, action="copy", copy_to="103, 8001",
                                                              r0_pct="3", r1_pct="0.3"))
        custom = df.read_mob_custom(self.spool)
        self.assertEqual([e["prob"] for e in custom[(8001, "drop")]["items"]], ["12", "1.2"])
        self.assertIn((103, "drop"), custom)
        kill_id = self.set_id([101], "kill")
        self.client.post(f"/db/drops/set/{kill_id}", data={"dbe_csrf": self.csrf, "member": "101", "action": "remove"})
        self.assertEqual(df.read_mob_custom(self.spool)[(101, "kill")]["items"], [])
        self.assertIn("Group\tMT2009_panel_101_kill", self.custom())

    def test_validation_and_csrf(self):
        set_id = self.set_id([101, 102])
        form = {"dbe_csrf": self.csrf, "action": "save", "member": ["101"], "row_count": "1",
                "r0_item": "999999", "r0_count": "1", "r0_pct": "1"}
        answer = self.client.post(f"/db/drops/set/{set_id}", data=form, follow_redirects=True)
        self.assertIn("nie istnieje w item_proto", answer.get_data(as_text=True))
        self.assertIsNone(self.custom())
        for bad in ({"r0_item": "27001", "r0_pct": "150"}, {"r0_item": "27001", "r0_count": "0"}):
            self.client.post(f"/db/drops/set/{set_id}", data=dict(form, **bad))
            self.assertIsNone(self.custom())
        self.client.post(f"/db/drops/set/{set_id}", data=dict(form, r0_item="27001", dbe_csrf="wrong"))
        self.assertIsNone(self.custom())
        self.client.post(f"/db/drops/set/{set_id}", data=dict(form, r0_item="27001", copy_to="555", action="copy"))
        self.assertIsNone(self.custom())

    def test_new_group(self):
        form = {"dbe_csrf": self.csrf, "kind": "kill", "mobs": "103", "row_count": "1", "r0_item": "27001",
                "r0_count": "1", "r0_weight": "1", "r0_pct": "1", "kill_drop": "20"}
        self.assertEqual(self.client.post("/db/drops/new", data=form).status_code, 302)
        self.assertEqual(df.read_mob_custom(self.spool)[(103, "kill")]["kill_drop"], "20")
        self.client.post("/db/drops/new", data=dict(form, mobs="101"))  # 101 already has a kill group
        self.assertNotIn((101, "kill"), df.read_mob_custom(self.spool))

    def test_common_table(self):
        self.assertIn("Zwykłe potwory", self.client.get("/db/drops/common").get_data(as_text=True))
        form = {"dbe_csrf": self.csrf, "rank": "2", "action": "save", "row_count": "4",
                "r0_item": "27001", "r0_from": "1", "r0_to": "15", "r0_pct": "0.03", "r0_label": "\xc3\xca",
                "r3_item": "27004", "r3_from": "2", "r3_to": "3", "r3_pct": "0.00001", "r3_shown": "0.00001",
                "r3_orig": "0.000041234", "r3_n": "77",
                "r1_item": "27002", "r1_from": "5", "r1_to": "15", "r1_pct": "0.015", "r1_delete": "1",
                "r2_item": "27003", "r2_from": "20", "r2_to": "30", "r2_pct": "1"}
        self.client.post("/db/drops/common", data=form)
        table = df.parse_common(self.custom("common"))
        self.assertEqual([(s["item"], s["pct"], s["n"]) for s in table["ranks"][2] if s["active"]],
                         [("27001", "0.12", "3333"), ("27003", "4", "100"), ("27004", "0.000041234", "77")])
        self.assertEqual([s["pct"] for s in table["ranks"][0] if s["active"]], ["0.08", "0.04"])
        self.client.post("/db/drops/common", data=dict(form, action="multiply", factor="2", scope="all"))
        table = df.parse_common(self.custom("common"))
        self.assertEqual([s["pct"] for s in table["ranks"][0] if s["active"]], ["0.16", "0.08"])
        self.assertEqual([s["pct"] for s in table["ranks"][2] if s["active"]], ["0.24", "8", "0.000082468"])
        self.client.post("/db/drops/common", data=dict(form, r0_to="0"))  # to < from: refused
        self.assertEqual([s["pct"] for s in df.parse_common(self.custom("common"))["ranks"][0] if s["active"]], ["0.16", "0.08"])
        self.client.post("/db/drops/common", data={"dbe_csrf": self.csrf, "rank": "0", "action": "reset"})
        self.assertIsNone(self.custom("common"))
        backups = df.list_backups(self.spool, ("common",))
        self.assertEqual(len(backups), 3)
        newest_with_data = next(b for b in backups if not b["empty"])
        self.client.post("/db/drops/restore", data={"dbe_csrf": self.csrf, "key": "common", "backup": newest_with_data["name"]})
        self.assertIsNotNone(self.custom("common"))

    def test_etc_table(self):
        self.assertIn("Potwor 101", self.client.get("/db/drops/etc").get_data(as_text=True))
        form = {"dbe_csrf": self.csrf, "action": "save", "row_count": "3", "r0_item": "30073", "r0_pct": "1",
                "r1_item": "30034", "r1_pct": "0.315", "r2_item": "30035", "r2_pct": "2"}
        self.client.post("/db/drops/etc", data=form)
        self.assertEqual(self.custom("etc"), "30073\t4\r\n30034\t1.26\r\n30035\t8\r\n")
        self.client.post("/db/drops/etc", data=dict(form, r2_item="30073"))  # twice: refused
        self.assertEqual(self.custom("etc"), "30073\t4\r\n30034\t1.26\r\n30035\t8\r\n")
        self.client.post("/db/drops/etc", data=dict(form, action="multiply", factor="0.5"))
        self.assertEqual(self.custom("etc"), "30073\t2\r\n30034\t0.63\r\n30035\t4\r\n")

    def test_chests(self):
        page = self.client.get("/db/chests").get_data(as_text=True)
        self.assertIn("Przedmiot 50011", page)
        self.assertIn("Yang", page)
        self.assertIn("Losowanie z grupy 50011", self.client.get("/db/chests/50012").get_data(as_text=True))
        form = {"dbe_csrf": self.csrf, "action": "save", "group_type": "", "row_count": "3",
                "r0_item": "27001", "r0_count": "1", "r0_prob": "30", "r0_rare": "0",
                "r1_item": "27002", "r1_count": "2", "r1_prob": "10", "r1_rare": "0", "r1_sel": "1",
                "r2_item": "gold", "r2_count": "1000", "r2_prob": "60", "r2_rare": "0"}
        answer = self.client.post("/db/chests/50011", data=dict(form, action="multiply", factor="2", r1_sel=""),
                                  follow_redirects=True)
        self.assertIn("zaznacz pozycje", answer.get_data(as_text=True))
        self.assertIsNone(self.custom("chest"))
        self.client.post("/db/chests/50011", data=dict(form, action="multiply", factor="3"))
        group = df.read_chest_custom(self.spool)[50011]
        self.assertEqual([e["prob"] for e in group["items"]], ["30", "30", "60"])
        self.client.post("/db/chests/50011", data=dict(form, r0_item="999999"))
        self.assertEqual([e["prob"] for e in df.read_chest_custom(self.spool)[50011]["items"]], ["30", "30", "60"])
        self.client.post("/db/chests/50012", data={"dbe_csrf": self.csrf, "action": "multiply", "factor": "5",
                                                   "group_type": "pct", "row_count": "1", "r0_item": "27003",
                                                   "r0_count": "1", "r0_prob": "50", "r0_rare": "0"})
        self.assertEqual(df.read_chest_custom(self.spool)[50012]["items"][0]["prob"], "100")
        self.client.post("/db/chests/new", data={"dbe_csrf": self.csrf, "vnum": "50020"})
        self.assertIn(50020, df.read_chest_custom(self.spool))
        self.client.post("/db/chests/50011", data={"dbe_csrf": self.csrf, "action": "reset"})
        self.assertNotIn(50011, df.read_chest_custom(self.spool))
        self.assertEqual(df.live_state(self.spool, "chest")["kind"], "unknown")
        self.assertIn("drops-chest.json", os.listdir(self.spool / "dbeditor" / "pending"))
        self.assertEqual(self.client.get("/db/chests?q=27003").status_code, 200)

    def test_sections_registered(self):
        endpoints = [s[0] for s in dbeditor.SECTIONS]
        self.assertIn("dbeditor.drops_index", endpoints)
        self.assertIn("dbeditor.chests_index", endpoints)


@unittest.skipUnless(shutil.which("sh") and M2_DROPS.is_file(), "no sh or m2-drops")
class GameSideTests(unittest.TestCase):
    """What the panel writes, taken by the game's m2-drops (no database:
    the item check is the panel's)."""

    def test_m2_drops_takes_the_panels_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            locale = root / "share" / "locale" / "poland"
            locale.mkdir(parents=True)
            (root / "spool" / "drops").mkdir(parents=True)
            (locale / "mob_drop_item.txt").write_text(MOB_SAMPLE.replace("\n", "\r\n"), encoding="utf-8")
            (locale / "common_drop_item.txt").write_bytes(COMMON_SAMPLE.encode("latin-1"))
            (locale / "etc_drop_item.txt").write_text(ETC_SAMPLE, encoding="ascii")
            (locale / "drop_item_group.txt").write_text("", encoding="ascii")
            spool = root / "spool"
            group = {"type": "kill", "kill_drop": "7", "level_limit": "0",
                     "items": [{"item": "27001", "count": "1", "prob": "1", "rare": "0", "comment": "Mikstura ł"}]}
            df.write_custom(spool, "mob", df.render_mob_custom({(101, "kill"): group, (102, "drop"): {"items": []}}, "test"))
            table = df.parse_common(COMMON_SAMPLE)
            df.write_custom(spool, "common", df.render_common(df.common_set_rank(table, 0, [df.common_slot(1, 9, "4", 27009)])))
            df.write_custom(spool, "etc", df.render_etc([{"item": "30073", "prob": "abc"}]))
            env = dict(os.environ, M2_SHARE_DIR=str(root / "share"), M2_VAR_DIR=str(root / "var"),
                       M2_RATES_SPOOL=str(spool), PATH="/usr/bin:/bin")
            for command in ("prepare", "apply"):
                subprocess.run(["sh", str(M2_DROPS), command], env=env, check=True, capture_output=True)
            live = (locale / "mob_drop_item.txt").read_text(encoding="utf-8")
            self.assertIn("MT2009_panel_101_kill", live)
            self.assertNotIn("WolfKill\r\n", live)
            self.assertNotIn("Wolf2", live)                 # an empty group cuts the image's out
            self.assertEqual(df.live_state(spool, "mob")["kind"], "ok")
            self.assertEqual((locale / "common_drop_item.txt").read_bytes(), df.custom_path(spool, "common").read_bytes())
            self.assertEqual(df.live_state(spool, "common")["kind"], "ok")
            self.assertEqual((locale / "etc_drop_item.txt").read_bytes(), ETC_SAMPLE.encode())   # rejected: the image's
            self.assertEqual(df.live_state(spool, "etc")["kind"], "error")
            self.assertEqual((spool / "drops" / "common_drop_item.base.txt").read_bytes(), COMMON_SAMPLE.encode("latin-1"))
            self.assertEqual([p["key"] for p in df.pending_changes(spool)], ["etc"])


if __name__ == "__main__":
    unittest.main()
