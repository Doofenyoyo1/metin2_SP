"""MT2009_PLUS_DB_EDITOR_V1: python3 -m unittest test_dbeditor_regen

The database editor's "Spawny potworów" part (dbeditor/regen.py): the
regen.txt reader (round trips on the game's real files when they are around -
DBE_REGEN_REAL, the test stack's spool volume or the test tree - and on small
samples always), the pages and their saves through the Flask test client on
a temporary spool, the "Zastosuj" page listing the map, and the game side
(m2-regen) taking what the panel wrote."""
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
from dbeditor import regen as R

HERE = Path(__file__).resolve().parent
M2_REGEN = HERE.parent / "game" / "bin" / "m2-regen"
REAL_CANDIDATES = [os.environ.get("DBE_REGEN_REAL", ""),
                   "/var/lib/docker/volumes/mt2009plustest_rates-spool/_data/regen/base/maps",
                   "/opt/metin2/mt2009plustest/linux-port/docker/game/src/serverfiles/share/locale/poland/map"]

REGEN_SAMPLE = ("// spawns of the test map\r\n"
                "r\t751\t311\t10\t10\t0\t0\t5s\t100\t1\t101\r\n"
                "ga\t300\t400\t20\t15\t0\t0\t60s\t100\t2\t102\t// wolves\r\n"
                "m\t195\t690\t10\t10\t0\t0\t5m-7m\t100\t1\t8015\r\n"
                "\r\n"
                "s\t0\t0\t0\t0\t0\t0\t2m\t100\t3\t103\r\n"
                "e\t10\t10\t5\t5\t0\r\n")
SETTING = "ScriptType\tMapSetting\r\n\r\nCellScale\t200\r\nMapSize\t4\t5\r\nBasePosition\t409600\t896000\r\n"
GROUPS = ("Group\tWolves\n{\n\tLeader\tWolf\t102\n\tVnum\t102\n\t1\tWildDog\t101\n\t2\tWildDog\t101\n}\n"
          "Group\tDogs\n{\n\tLeader\tWildDog\t101\n\tVnum\t101\n\t1\tWildDog\t101\n}\n"
          "Group\tHorse\n{\n\tLeader\tHorse\t20101\n\tVnum\t900\n}\n"
          "Group\tHungry\n{\n\tLeader\t\"Hungriger Wildhund\"\t101\n\tVnum\t171\n\t1\t\"Hungriger Wildhund\"\t101\n}\n")
GROUP_GROUPS = "Group\ta1_01\r\n{\r\n\tVnum\t101\r\n\t1\t101\t1\r\n\t2\t102\t1\r\n}\r\n"
MOBS = {101: ("Dziki Pies", 0, 0, 1), 102: ("Wilk", 0, 0, 3), 103: ("Alfa Wilk", 0, 0, 5),
        8015: ("Metin Gniewu", 0, 2, 40), 191: ("Lykos", 1, 0, 25), 20101: ("Koń", 0, 1, 1)}


def real_maps():
    for candidate in REAL_CANDIDATES:
        if candidate and Path(candidate).is_dir() and any(Path(candidate).glob("*/regen.txt")):
            return Path(candidate)
    return None


class FakeDb:
    """mob_proto: MOBS; player.quest: the given flags."""

    def __init__(self, flags=None):
        self.flags = flags or {}

    def __call__(self, sql, params=()):
        if "FROM player.mob_proto" in sql:
            return [{"vnum": v, "locale_name": n, "name": "", "rank": rank, "type": kind, "level": level, "drop_item": 0}
                    for v, (n, rank, kind, level) in MOBS.items()]
        if "FROM player.quest" in sql:
            return [{"szName": k, "lValue": v} for k, v in self.flags.items() if k in params]
        return []


def publish(spool, maps=None):
    base = spool / "regen" / "base"
    for folder, text in (maps or {"metin2_map_a1": REGEN_SAMPLE}).items():
        (base / "maps" / folder).mkdir(parents=True, exist_ok=True)
        (base / "maps" / folder / "regen.txt").write_bytes(text.encode("latin-1"))
        (base / "maps" / folder / "Setting.txt").write_text(SETTING, encoding="ascii")
    (base / "index").write_text("1\tmetin2_map_a1\n64\tmap_n_threeway\n", encoding="ascii")
    (base / "group.txt").write_text(GROUPS, encoding="latin-1")
    (base / "group_group.txt").write_text(GROUP_GROUPS, encoding="latin-1")


def make_app(spool, flags=None):
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
    ctx = {"app": app, "db": None, "rows": FakeDb(flags), "one": None,
           "login_required": lambda view: view, "game_text": lambda value: value or ""}
    R.install(bp, ctx)
    app.register_blueprint(bp)
    return app


class ParserTests(unittest.TestCase):
    def test_sample_rows(self):
        doc = R.parse(REGEN_SAMPLE)
        self.assertTrue(doc["editable"])
        self.assertEqual(doc["problems"], [])
        self.assertEqual([r["type"] for r in doc["rows"]], ["r", "g", "m", "s", "e"])
        wolves = doc["rows"][1]
        self.assertEqual((wolves["flag"], wolves["x"], wolves["y"], wolves["sx"], wolves["sy"], wolves["time"],
                          wolves["count"], wolves["vnum"]), ("a", 300, 400, 20, 15, 60, 2, 102))
        self.assertEqual((doc["rows"][2]["time"], doc["rows"][2]["time_to"]), (300, 420))
        self.assertEqual(R.render(doc), REGEN_SAMPLE)
        self.assertEqual(R.tokenize(REGEN_SAMPLE), R.tokenize_exact(REGEN_SAMPLE))
        tricky = "m 1 2//x 3\n//only\n  m\t//\nab//c d\r\n"
        self.assertEqual(R.tokenize(tricky), R.tokenize_exact(tricky))

    def test_time_values(self):
        self.assertEqual(R.time_value("5s"), (5, 0))
        self.assertEqual(R.time_value("1h30m"), (5400, 0))
        self.assertEqual(R.time_value("90"), (90, 0))
        self.assertEqual(R.time_value("5m-7m"), (300, 420))
        self.assertEqual(R.time_raw(30, 45), "30s-45s")
        self.assertEqual(R.time_text(300, 420), "co 5 min–7 min (losowo)")
        self.assertEqual(R.duration_text(5400), "90 min")
        self.assertEqual(R.duration_text(9000), "2 h 30 min")

    def test_edit_add_delete_keep_the_rest(self):
        doc = R.parse(REGEN_SAMPLE)
        R.touch(doc["rows"][1], count=4, time=30, time_to=0)
        R.add_row(doc, {"type": "m", "vnum": 103, "x": 5, "y": 6, "sx": 1, "sy": 1, "time": 90, "count": 2})
        R.delete_rows(doc, [0])
        text = R.render(doc)
        lines = text.split("\r\n")
        self.assertEqual(lines[0], "// spawns of the test map")
        self.assertEqual(lines[1], "ga\t300\t400\t20\t15\t0\t0\t30s\t100\t4\t102\t// wolves")
        self.assertEqual(lines[2], "m\t195\t690\t10\t10\t0\t0\t5m-7m\t100\t1\t8015")   # untouched, as it was
        self.assertEqual(lines[-2], "m\t5\t6\t1\t1\t0\t0\t90s\t100\t2\t103")
        self.assertTrue(text.endswith("\r\n"))
        again = R.parse(text)
        self.assertTrue(again["editable"])
        self.assertEqual([r["vnum"] for r in again["rows"]], [102, 8015, 103, 0, 103])

    def test_scaling(self):
        doc = R.parse(REGEN_SAMPLE)
        self.assertEqual(R.scale_counts(doc, 2), 4)                     # the "e" area is never touched
        self.assertEqual([r["count"] for r in doc["rows"][:4]], [2, 4, 2, 6])
        self.assertEqual(R.scale_counts(doc, 0.25), 4)
        self.assertEqual([r["count"] for r in doc["rows"][:4]], [1, 1, 1, 2])   # never below 1
        self.assertEqual(R.scale_times(doc, 0.5), 4)
        self.assertEqual([(r["time"], r["time_to"]) for r in doc["rows"][:4]], [(3, 0), (30, 0), (150, 210), (60, 0)])
        self.assertEqual(R.parse(R.render(doc))["rows"][2]["time_raw"], "150s-210s")

    def test_not_editable_layouts(self):
        split = "m\t1\t2\t3\t4\t0\t0\r\n5s\t100\t1\t101\r\n"
        doc = R.parse(split)
        self.assertFalse(doc["editable"])                               # the core reads it as one entry
        self.assertEqual(doc["entries"], 1)
        self.assertFalse(R.parse("m 1 2 3 4 0 0 5s 100 1 101 m 1 2 3 4 0 0 5s 100 1 101\n")["editable"])
        self.assertIn("nieznany typ", R.parse("x 1 2 3 4 0 0 5s 100 1 101\n")["problems"][0])
        self.assertIn("niepełny", R.parse("m 1 2 3\n")["problems"][0])
        comment_inside = R.parse("m 1 2 3 4 //x\n0 0 5s 100 1 101\n")
        self.assertEqual(comment_inside["entries"], 1)                 # read_line goes on after a comment
        self.assertFalse(comment_inside["editable"])

    def test_groups_and_settings(self):
        groups = R.parse_groups(GROUPS)
        self.assertEqual(groups[102]["members"], [102, 101, 101])
        self.assertEqual(groups[900]["members"], [20101])
        self.assertEqual(groups[171]["members"], [101, 101])           # quoted names with blanks
        self.assertEqual(R.parse_groups(GROUP_GROUPS, "group_group")[101]["members"], [101, 102])
        self.assertEqual(R.parse_setting(SETTING), (1024, 1280))
        self.assertEqual(R.parse_index("1\tmetin2_map_a1\r\n// x\r\n64 map_n_threeway\n"),
                         [(1, "metin2_map_a1"), (64, "map_n_threeway")])

    @unittest.skipUnless(real_maps(), "no real map files")
    def test_real_files_round_trip(self):
        folder = real_maps()
        checked = rows = 0
        for path in sorted(folder.glob("*/regen.txt")):
            text = path.read_bytes().decode("latin-1")
            doc = R.parse(text)
            self.assertEqual(R.render(doc), text, path)
            if checked < 5:
                self.assertEqual(R.tokenize(text), R.tokenize_exact(text), path)
            self.assertTrue(doc["editable"], path)
            self.assertEqual(doc["problems"], [], path)
            self.assertEqual(doc["entries"], len(doc["rows"]), path)
            if doc["rows"]:                                             # a changed line survives a re-read
                R.touch(doc["rows"][0], count=doc["rows"][0]["count"] or 1)
                self.assertEqual(len(R.parse(R.render(doc))["rows"]), len(doc["rows"]), path)
            checked += 1
            rows += len(doc["rows"])
        self.assertGreater(checked, 30)
        self.assertGreater(rows, 10000)
        for name, kind in (("group.txt", "group"), ("group_group.txt", "group_group")):
            path = folder.parent / name
            if path.is_file():
                self.assertGreater(len(R.parse_groups(path.read_bytes().decode("latin-1"), kind)), 100)
        setting = folder / "metin2_map_a1" / "Setting.txt"
        if setting.is_file():
            self.assertEqual(R.parse_setting(setting.read_text(errors="replace")), (1024, 1280))


class PageTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.spool = Path(self.tmp.name)
        publish(self.spool)
        dbeditor.SECTIONS.clear()
        self.app = make_app(self.spool, {"fastMobSpawn": 50, "m2_mob_count": 200})
        self.client = self.app.test_client()
        with self.client.session_transaction() as session:
            session["dbe_drops_csrf"] = "tok"

    def tearDown(self):
        self.tmp.cleanup()

    def rev(self):
        return R.sha_text(R.effective_text(self.spool, "metin2_map_a1")[0])

    def post(self, action, **form):
        form.setdefault("dbe_csrf", "tok")
        form.setdefault("rev", self.rev())
        return self.client.post(f"/db/regen/metin2_map_a1/{action}", data=form, follow_redirects=True)

    def custom(self):
        return R.custom_path(self.spool, "metin2_map_a1").read_bytes().decode("latin-1")

    def test_pages(self):
        page = self.client.get("/db/regen").get_data(as_text=True)
        self.assertIn("metin2_map_a1", page)
        self.assertNotIn("map_n_threeway", page)                        # not published: not listed
        page = self.client.get("/db/regen/metin2_map_a1?edit=1").get_data(as_text=True)
        self.assertIn("Grupa: Wilk + 2× Dziki Pies (3 szt.)", page)
        self.assertIn("miejsce (300, 400) ± 20×15 m", page)
        self.assertIn("co 60 s · naraz 2 grupy", page)
        self.assertIn("w grze: co 30 s · naraz 4", page)                 # fastMobSpawn 50%, m2_mob_count 200%
        self.assertIn("Losowa grupa (1 z 2)", page)
        self.assertIn("Potwór w losowym miejscu mapy: Alfa Wilk", page)
        self.assertIn('class="dbr-mk k-g on"', page)
        self.assertIn('name="row" value="1"', page)
        self.assertEqual(self.client.get("/db/regen/nope").status_code, 404)
        self.assertEqual(self.client.get("/db/regen/..%2Fx").status_code, 404)
        self.assertIn("dbeditor.regen_index", [s[0] for s in dbeditor.SECTIONS])

    def test_search(self):
        data = self.client.get("/db/regen/api/search?q=wilk").get_json()
        self.assertEqual([m["vnum"] for m in data["mobs"]], [102, 103])
        self.assertIn(102, [g["vnum"] for g in data["groups"]])
        data = self.client.get("/db/regen/api/search?q=101").get_json()
        self.assertEqual(data["mobs"][0]["vnum"], 101)
        self.assertEqual({(g["type"], g["vnum"]) for g in data["groups"]}, {("g", 101), ("g", 102), ("g", 171), ("r", 101)})

    def test_edit_and_pending(self):
        page = self.post("edit", row="1", x="310", y="410", sx="5", sy="5", time="45", time_to="", count="3",
                         percent="100", flag="").get_data(as_text=True)
        self.assertIn("Zapisano wpis 2", page)
        self.assertIn("g\t310\t410\t5\t5\t0\t0\t45s\t100\t3\t102\t// wolves", self.custom())
        self.assertIn("m\t195\t690\t10\t10\t0\t0\t5m-7m\t100\t1\t8015", self.custom())
        pending = R.pending_changes(self.spool)
        self.assertEqual([p["key"] for p in pending], ["regen-metin2_map_a1"])
        self.assertEqual(pending[0]["kind"], "unknown")
        self.assertTrue((self.spool / "dbeditor" / "pending" / "regen-metin2_map_a1.json").is_file())
        # the game reports it applied
        status = R.status_path(self.spool, "metin2_map_a1")
        status.parent.mkdir(parents=True, exist_ok=True)
        sha = R.sha_text(self.custom())
        status.write_text(f"state=ok\ntime=1700000000\nrows=6\nsha={sha}\nmessage=applied\n", encoding="utf-8")
        self.assertEqual(R.pending_changes(self.spool), [])
        self.assertFalse((self.spool / "dbeditor" / "pending" / "regen-metin2_map_a1.json").exists())
        status.write_text(f"state=rejected\ntime=1700000000\nrows=0\nsha={sha}\nmessage=no such monster\n", encoding="utf-8")
        self.assertEqual(R.pending_changes(self.spool)[0]["kind"], "error")

    def test_checks(self):
        page = self.post("edit", row="1", x="5000", y="10", sx="5", sy="5", time="45", count="3").get_data(as_text=True)
        self.assertIn("poza mapą", page)
        page = self.post("add", type="m", vnum="999", x="10", y="10", sx="5", sy="5", time="60", count="1").get_data(as_text=True)
        self.assertIn("Nie ma potwora o numerze 999", page)
        page = self.post("add", type="g", vnum="555", x="10", y="10", sx="5", sy="5", time="60", count="1").get_data(as_text=True)
        self.assertIn("Nie ma grupy 555", page)
        page = self.post("add", type="m", vnum="101", x="10", y="10", sx="5", sy="5", time="0", count="1").get_data(as_text=True)
        self.assertIn("Czas odrodzenia", page)
        page = self.post("edit", rev="stale", row="1", x="1", y="1", sx="5", sy="5", time="45", count="3").get_data(as_text=True)
        self.assertIn("zmienił się w międzyczasie", page)
        page = self.post("edit", dbe_csrf="bad", row="1", x="1", y="1", sx="5", sy="5", time="45", count="3").get_data(as_text=True)
        self.assertIn("Sesja formularza wygasła", page)
        self.assertFalse(R.custom_path(self.spool, "metin2_map_a1").exists())

    def test_add_delete_scale_restore_reset(self):
        page = self.post("add", type="g", vnum="101", x="100", y="200", sx="8", sy="8", time="120", time_to="180",
                         count="2", flag="a").get_data(as_text=True)
        self.assertIn("Dodano wpis 6", page)
        self.assertTrue(self.custom().endswith("ga\t100\t200\t8\t8\t0\t0\t120s-180s\t100\t2\t101\r\n"))
        self.assertIn("Usunięto 2 wpisy", self.post("delete", rows=["0", "3"]).get_data(as_text=True))
        rows = R.parse(self.custom())["rows"]
        self.assertEqual([r["vnum"] for r in rows], [102, 8015, 0, 101])
        page = self.post("scale", what="count", factor="2", only="monsters").get_data(as_text=True)
        self.assertIn("zmieniono 3 wpisy", page)
        self.assertEqual([r["count"] for r in R.parse(self.custom())["rows"]], [4, 2, 0, 4])
        page = self.post("scale", what="time", factor="0.5").get_data(as_text=True)
        self.assertEqual([r["time"] for r in R.parse(self.custom())["rows"]], [30, 150, 0, 60])
        backups = R.list_backups(self.spool, "metin2_map_a1")
        self.assertEqual(len(backups), 4)
        self.assertTrue(backups[-1]["empty"])                           # the first: "no changes"
        page = self.post("restore", backup=backups[-1]["name"]).get_data(as_text=True)
        self.assertIn("Przywrócono kopię", page)
        self.assertFalse(R.custom_path(self.spool, "metin2_map_a1").exists())
        self.post("restore", backup=R.list_backups(self.spool, "metin2_map_a1")[0]["name"])
        self.assertTrue(R.custom_path(self.spool, "metin2_map_a1").exists())
        self.post("reset")
        self.assertFalse(R.custom_path(self.spool, "metin2_map_a1").exists())
        self.assertFalse(R.meta_path(self.spool, "metin2_map_a1").exists())
        # an edit that brings the file back to the image's removes the custom file
        self.post("edit", row="1", x="300", y="400", sx="20", sy="15", time="45", count="2", percent="100", flag="a")
        self.assertTrue(R.custom_path(self.spool, "metin2_map_a1").exists())
        self.post("edit", row="1", x="300", y="400", sx="20", sy="15", time="60", count="2", percent="100", flag="a")
        self.assertFalse(R.custom_path(self.spool, "metin2_map_a1").exists())

    def test_apply_page_lists_the_map(self):
        try:
            from dbeditor import clientdata, common_items
        except ImportError as exc:  # pragma: no cover - an image without the client pack
            self.skipTest(str(exc))
        app = Flask("apply", static_folder=str(HERE / "static"))
        app.secret_key = "t"
        app.config.update(TESTING=True, DBE_SPOOL=str(self.spool))
        app.jinja_loader = self.app.jinja_loader
        bp = Blueprint("dbeditor", "dbeditor", url_prefix="/db")
        bp.add_url_rule("/", "index", lambda: "hub")
        ctx = {"app": app, "db": None, "rows": FakeDb(), "one": None, "login_required": lambda v: v,
               "game_text": lambda v: v or "", "spool": self.spool}
        os.environ.pop("DBEDITOR_SPOOL_ROOT", None)
        old = dict(common_items.ctx())
        try:
            clientdata.install(bp, ctx)
            app.register_blueprint(bp)
            self.post("add", type="m", vnum="101", x="10", y="10", sx="5", sy="5", time="60", count="1")
            page = app.test_client().get("/db/apply").get_data(as_text=True)
        finally:
            common_items.ctx().clear()
            common_items.ctx().update(old)
        self.assertIn("Spawny potworów: metin2_map_a1", page)


@unittest.skipUnless(shutil.which("sh") and shutil.which("awk") and M2_REGEN.is_file(), "no sh/awk or m2-regen")
class GameSideTests(unittest.TestCase):
    """What the panel writes, taken by the game's m2-regen (no database: the
    monster check is the panel's; the groups are checked against group.txt)."""

    def run_regen(self, root, spool):
        env = dict(os.environ, M2_SHARE_DIR=str(root / "share"), M2_VAR_DIR=str(root / "var"),
                   M2_RATES_SPOOL=str(spool), PATH=str(root / "nobin") + ":/usr/bin:/bin")
        # no mariadb client on the PATH: the mob check is skipped like a silent database
        (root / "nobin").mkdir(exist_ok=True)
        (root / "nobin" / "mariadb").write_text("#!/bin/sh\nexit 1\n")
        (root / "nobin" / "mariadb").chmod(0o755)
        out = ""
        for command in ("prepare", "apply"):
            out += subprocess.run(["sh", str(M2_REGEN), command], env=env, check=True, capture_output=True,
                                  text=True).stdout
        return out

    def test_m2_regen_takes_the_panels_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            locale = root / "share" / "locale" / "poland"
            for folder in ("metin2_map_a1", "map_n_threeway"):
                (locale / "map" / folder).mkdir(parents=True)
                (locale / "map" / folder / "regen.txt").write_bytes(REGEN_SAMPLE.encode())
                (locale / "map" / folder / "Setting.txt").write_text(SETTING)
            (locale / "map" / "index").write_text("1\tmetin2_map_a1\n64\tmap_n_threeway\n")
            (locale / "group.txt").write_text(GROUPS)
            (locale / "group_group.txt").write_text(GROUP_GROUPS)
            spool = root / "spool"
            spool.mkdir()
            self.run_regen(root, spool)                                 # publishes the image's files
            self.assertEqual((spool / "regen" / "base" / "maps" / "metin2_map_a1" / "regen.txt").read_bytes(),
                             REGEN_SAMPLE.encode())
            self.assertEqual(R.map_list(spool), [(1, "metin2_map_a1"), (64, "map_n_threeway")])

            good = R.parse(REGEN_SAMPLE)
            R.touch(good["rows"][1], count=5)
            R.write_custom(spool, "metin2_map_a1", R.render(good), "test")
            bad = R.parse(REGEN_SAMPLE)
            R.touch(bad["rows"][1], vnum=555)                           # no such group
            R.write_custom(spool, "map_n_threeway", R.render(bad), "test")
            R.write_custom(spool, "metin2_map_gone", "m 1 1 1 1 0 0 5s 100 1 101\n", "test")
            out = self.run_regen(root, spool)
            live = (locale / "map" / "metin2_map_a1" / "regen.txt").read_bytes()
            self.assertEqual(live, R.custom_path(spool, "metin2_map_a1").read_bytes())
            self.assertEqual(R.live_state(spool, "metin2_map_a1")["kind"], "ok")
            self.assertEqual((locale / "map" / "map_n_threeway" / "regen.txt").read_bytes(), REGEN_SAMPLE.encode())
            state = R.live_state(spool, "map_n_threeway")
            self.assertEqual(state["kind"], "error")
            self.assertIn("555", state["text"])
            self.assertIn("has no map", R.live_state(spool, "metin2_map_gone")["text"])
            self.assertIn("could not check the monsters", out)
            self.assertEqual(sorted(p["key"] for p in R.pending_changes(spool)),
                             ["regen-map_n_threeway", "regen-metin2_map_gone"])

            # what the core would stop on, or never spawn, is refused
            for text, why in (("x\t1\t1\t1\t1\t0\t0\t5s\t100\t1\t101\n", "unknown type"),
                              ("m\t1\t1\t1\t1\t0\t0\t0s\t100\t1\t101\n", "time 0"),
                              ("m\t9999\t1\t1\t1\t0\t0\t5s\t100\t1\t101\n", "outside the map"),
                              ("m\t1\t1\t1\t1\t0\t0\t5s\t100\t1\n", "11 words"),
                              ("r\t1\t1\t1\t1\t0\t0\t5s\t100\t1\t77\n", "group of groups")):
                R.custom_path(spool, "metin2_map_a1").write_text(text)
                self.run_regen(root, spool)
                self.assertIn(why, R.live_state(spool, "metin2_map_a1")["text"], text)
                self.assertEqual((locale / "map" / "metin2_map_a1" / "regen.txt").read_bytes(), REGEN_SAMPLE.encode())

            # back to the image: the custom file removed, the image's file live
            R.write_custom(spool, "metin2_map_a1", R.render(good), "test")
            self.run_regen(root, spool)
            self.assertNotEqual((locale / "map" / "metin2_map_a1" / "regen.txt").read_bytes(), REGEN_SAMPLE.encode())
            R.write_custom(spool, "metin2_map_a1", None, "reset")
            self.assertTrue(R.live_state(spool, "metin2_map_a1")["pending"])
            self.run_regen(root, spool)
            self.assertEqual((locale / "map" / "metin2_map_a1" / "regen.txt").read_bytes(), REGEN_SAMPLE.encode())
            self.assertEqual(R.live_state(spool, "metin2_map_a1")["kind"], "ok")
            self.assertIn("state=base", R.status_path(spool, "metin2_map_a1").read_text())


if __name__ == "__main__":
    unittest.main()
