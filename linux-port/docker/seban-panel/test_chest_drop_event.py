"""MT2009_PLUS_CHEST_DROP_EVENT_V1: python3 test_chest_drop_event.py

The events file round trip and the events page's save/now/stop for the chest
drop, on a temporary spool and with the item table mocked."""
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

import app as panel

ITEMS = {50011: "Szkatułka Blasku Księżyca", 50037: "Szkatułka Smoka"}


def fake_item_info(vnum):
    try:
        vnum = int(vnum)
    except (TypeError, ValueError):
        return None
    return {"vnum": vnum, "name": ITEMS[vnum], "icon": None} if vnum in ITEMS else None


class ChestDropEventTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        spool = Path(self.tmp.name)
        self.file = spool / "playerbot_events.tsv"
        self.patches = [
            patch.object(panel, "RATES_SPOOL", spool),
            patch.object(panel, "EVENTS_FILE", self.file),
            patch.object(panel, "event_item_info", side_effect=fake_item_info),
            patch.object(panel, "settings", return_value={"setup_complete": "1", "auth_enabled": "0"}),
            patch.object(panel, "check_all_notifications", return_value=None),
            patch.object(panel, "rows", return_value=[]),
            patch.object(panel, "read_events_status", return_value={}),
            patch.object(panel, "read_world_events_status", return_value={}),
        ]
        for item in self.patches:
            item.start()
        panel.app.config["TESTING"] = True
        self.client = panel.app.test_client()

    def tearDown(self):
        for item in self.patches:
            item.stop()
        self.tmp.cleanup()

    def lines(self):
        return [line for line in self.file.read_text(encoding="utf-8").splitlines() if line and not line.startswith("# ")]

    def test_old_file_and_round_trip(self):
        until = int(time.time()) + 3600
        self.file.write_text(
            "exp\t6,7\t18:00\t20:00\t50\n"
            "zuo\t5\t21:00\t22:00\t8\t43\n"
            "chestdrop\t*\t20:00\t21:00\t5\t50011\n"
            "#off\tchestdrop\t1\t10:00\t11:00\t1000\t50037\n"
            "chestdrop\t2\t12:00\t13:00\t25\n"  # no vnum column: read as 0
            f"now\tdrop\t{until}\t30\n"
            f"now\tzuo\t{until}\t8\t63\t{until - 60}\n"
            f"now\tchestdrop\t{until}\t5\t50011\t{until - 600}\n"
            f"now\tchestdrop\t{until}\t200\t50037\t{until - 300}\n"
            "bots\t40\n", encoding="utf-8")
        rows, nows = panel.read_events()
        chest_rows = [row for row in rows if row["kind"] == "chestdrop"]
        self.assertEqual([(row["value"], row["map"], row["on"]) for row in chest_rows],
                         [(5, 50011, True), (1000, 50037, False), (25, 0, True)])
        self.assertEqual(nows["chestdrop@50011"]["value"], 5)
        self.assertEqual(nows["chestdrop@50037"]["since"], until - 300)
        self.assertIn("zuo@63", nows)
        self.assertIn("drop", nows)
        panel.write_events(rows, nows)
        lines = self.lines()
        self.assertIn("chestdrop\t*\t20:00\t21:00\t5\t50011", lines)
        self.assertIn("#off\tchestdrop\t1\t10:00\t11:00\t1000\t50037", lines)
        self.assertIn(f"now\tchestdrop\t{until}\t5\t50011\t{until - 600}", lines)
        self.assertIn(f"now\tchestdrop\t{until}\t200\t50037\t{until - 300}", lines)
        self.assertIn(f"now\tdrop\t{until}\t30", lines)
        self.assertIn(f"now\tzuo\t{until}\t8\t63\t{until - 60}", lines)
        self.assertIn("exp\t6,7\t18:00\t20:00\t50", lines)
        self.assertIn("bots\t40", lines)
        again = panel.read_events()
        self.assertEqual(again, (rows, nows))

    def test_chance_conversion(self):
        self.assertEqual(panel.event_chance_permille("0,1"), 1)
        self.assertEqual(panel.event_chance_permille("0.5"), 5)
        self.assertEqual(panel.event_chance_permille("12.34"), 123)
        self.assertEqual(panel.event_chance_permille("100"), 1000)
        self.assertEqual(panel.event_chance_permille("250"), 1000)
        self.assertEqual(panel.event_chance_permille("0.01"), 1)
        for bad in ("", "0", "-3", "abc", "nan"):
            self.assertIsNone(panel.event_chance_permille(bad))
        self.assertEqual(panel.event_chance_text(5), "0,5")
        self.assertEqual(panel.event_chance_text(1000), "100")

    def test_window_active(self):
        when = time.mktime((2026, 10, 5, 23, 30, 0, 0, 0, -1))  # a Monday, 23:30
        row = {"kind": "chestdrop", "days": [1], "start": "22:00", "end": "02:00", "on": True}
        self.assertEqual(panel.event_window_active(row, when), int(when) + 150 * 60)
        self.assertEqual(panel.event_window_active(dict(row, days=[2]), when), 0)
        self.assertEqual(panel.event_window_active(dict(row, on=False), when), 0)
        after = time.mktime((2026, 10, 6, 1, 0, 0, 0, 0, -1))  # Tuesday 01:00, still Monday's
        self.assertEqual(panel.event_window_active(row, after), int(after) + 60 * 60)

    def test_save_now_stop(self):
        form = {"action": "save",
                "r0_kind": "exp", "r0_start": "18:00", "r0_end": "20:00", "r0_value": "50", "r0_d6": "1", "r0_on": "1",
                "r1_kind": "chestdrop", "r1_start": "20:00", "r1_end": "21:00", "r1_value": "5",
                "r1_chance": "0,5", "r1_vnum": "50011", "r1_map": "50011", "r1_d1": "1", "r1_d2": "1", "r1_on": "1"}
        self.assertEqual(self.client.post("/events", data=form).status_code, 302)
        self.assertIn("chestdrop\t1,2\t20:00\t21:00\t5\t50011", self.lines())
        # An unknown vnum and a bad chance are refused, the file stays as it was.
        before = self.file.read_text(encoding="utf-8")
        for key, value in (("r1_vnum", "99999"), ("r1_vnum", ""), ("r1_chance", "0"), ("r1_chance", "x")):
            bad = dict(form, **{key: value})
            response = self.client.post("/events", data=bad, follow_redirects=True)
            self.assertEqual(response.status_code, 200)
            self.assertEqual(self.file.read_text(encoding="utf-8"), before)
        response = self.client.post("/events", data=dict(form, r1_vnum="99999"), follow_redirects=True)
        self.assertIn("nie ma przedmiotu o VNUM 99999", response.get_data(as_text=True))

        for vnum, chance in (("50011", "2.5"), ("50037", "100")):
            response = self.client.post("/events", data={"action": "now", "kind": "chestdrop", "minutes": "60",
                                                          "chance": chance, "vnum": vnum})
            self.assertEqual(response.status_code, 302)
        _rows, nows = panel.read_events()
        self.assertEqual(nows["chestdrop@50011"]["value"], 25)
        self.assertEqual(nows["chestdrop@50037"]["value"], 1000)
        self.assertGreater(nows["chestdrop@50011"]["until"], time.time() + 3500)
        before = self.file.read_text(encoding="utf-8")
        self.client.post("/events", data={"action": "now", "kind": "chestdrop", "minutes": "60", "chance": "1", "vnum": "123"})
        self.assertEqual(self.file.read_text(encoding="utf-8"), before)

        page = self.client.get("/events").get_data(as_text=True)
        self.assertIn("Szkatułka Smoka · 100% · do", page)
        self.assertIn("Szkatułka Blasku Księżyca · 2,5% · do", page)
        self.assertIn("VNUM szkatułki", page)
        self.assertIn("Szansa (%)", page)
        self.assertNotIn('value="chestdrop" checked', page)
        self.assertNotIn('name="autogen-kind" value="chestdrop"', page)

        # The Stop beside one chest ends that chest alone.
        self.client.post("/events", data={"action": "stop", "kind": "chestdrop", "vnum": "50011"})
        _rows, nows = panel.read_events()
        self.assertNotIn("chestdrop@50011", nows)
        self.assertIn("chestdrop@50037", nows)
        self.assertIn("chestdrop\t1,2\t20:00\t21:00\t5\t50011", self.lines())
        self.client.post("/events", data={"action": "stop", "kind": "chestdrop", "minutes": "60"})
        self.assertFalse([key for key in panel.read_events()[1] if key.startswith("chestdrop")])

    def test_autogen_never_draws_chestdrop(self):
        response = self.client.post("/events/autogen", json={"kinds": ["chestdrop"], "count": 3, "length": 60,
                                                             "from": 16, "to": 24, "days": [1, 2, 3]})
        self.assertEqual(response.status_code, 400)
        response = self.client.post("/events/autogen", json={"kinds": ["chestdrop", "exp"], "count": 3, "length": 60,
                                                             "from": 16, "to": 24, "days": [1, 2, 3]})
        data = response.get_json()
        self.assertTrue(data["ok"])
        self.assertTrue(all(row["kind"] == "exp" for row in data["rows"]))

    def test_item_lookup(self):
        data = self.client.get("/events/item/50037").get_json()
        self.assertEqual((data["ok"], data["name"]), (True, "Szkatułka Smoka"))
        self.assertEqual(self.client.get("/events/item/99999").status_code, 404)

    def test_history_label_and_runs(self):
        self.assertEqual(panel.event_run_label("chestdrop@50037"), "Drop szkatułek · Szkatułka Smoka")
        self.assertEqual(panel.event_run_label("chestdrop@7"), "Drop szkatułek · VNUM 7")
        self.assertEqual(panel.event_run_label("zuo@43"), "Zuo: deszcz Metinów · Bakra")
        now = int(time.time())
        with panel.app.test_request_context("/events"):
            runs = panel.event_chestdrop_runs(
                [{"kind": "chestdrop", "days": list(range(1, 8)), "start": "00:00", "end": "00:00", "value": 30, "map": 50011, "on": True}],
                {"chestdrop@50011": {"kind": "chestdrop", "until": now + 60, "value": 5, "map": 50011, "since": now},
                 "chestdrop@50037": {"kind": "chestdrop", "until": now - 1, "value": 5, "map": 50037, "since": now - 99}},
                now)
        self.assertEqual(len(runs), 1)
        self.assertEqual((runs[0]["vnum"], runs[0]["value"], runs[0]["manual"], runs[0]["chance"]), (50011, 30, True, "3"))
        self.assertGreater(runs[0]["until"], now)


if __name__ == "__main__":
    unittest.main()
