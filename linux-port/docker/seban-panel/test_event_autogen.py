"""MT2009_PLUS_EVENTS_AUTOGEN_V1: python3 test_event_autogen.py"""
import unittest

import event_autogen as gen

ALL = ("chest", "exp", "drop", "yang", "tanaka", "zuo", "bossloot", "metinloot",
       "goblin", "catchking", "rumi", "yutnori", "flower", "easter")


def spans_of(rows):
    return gen.existing_spans(rows)


def same_kind_overlaps(rows):
    spans = spans_of(rows)
    for i, (kind, start, length) in enumerate(spans):
        for other_kind, other_start, other_length in spans[i + 1:]:
            if kind == other_kind and gen._overlaps(start, length, other_start, other_length):
                return True
    return False


class GenerateWeekTests(unittest.TestCase):
    def test_even_spread_and_balance(self):
        result = gen.generate_week(["exp", "drop", "chest"], 14, 120, 16, 24, seed=1,
                                   values={"exp": 50, "drop": 30})
        self.assertEqual(result["placed"], 14)
        per_day = [sum(1 for e in result["events"] if e["day"] == day) for day in range(1, 8)]
        self.assertEqual(per_day, [2] * 7)
        per_kind = sorted(sum(1 for e in result["events"] if e["kind"] == k) for k in ("exp", "drop", "chest"))
        self.assertLessEqual(per_kind[-1] - per_kind[0], 1)
        for event in result["events"]:
            start = gen.parse_hhmm(event["start"])
            self.assertGreaterEqual(start, 16 * 60)
            self.assertLessEqual(start + 120, 24 * 60)
        self.assertFalse(same_kind_overlaps(result["rows"]))
        self.assertTrue(all(row["value"] == 0 for row in result["rows"] if row["kind"] == "chest"))
        self.assertTrue(all(row["value"] == 50 for row in result["rows"] if row["kind"] == "exp"))
        self.assertLessEqual(len(result["rows"]), 14)
        self.assertEqual(sum(len(r["days"]) for r in result["rows"]), 14)

    def test_quotas(self):
        self.assertEqual(gen.day_quotas(10, [1, 2, 3, 4, 5, 6, 7]), [1, 2, 1, 2, 1, 2, 1])
        self.assertEqual(sum(gen.day_quotas(10, [1, 2, 3, 4, 5, 6, 7], True)), 10)
        weekend = gen.day_quotas(10, [1, 2, 3, 4, 5, 6, 7], True)
        self.assertGreater(sum(weekend[4:]), sum(weekend[:4]))

    def test_single_kind_never_overlaps_and_reports_overflow(self):
        # 64 two-hour events of one kind in a 4-hour window on 7 days: two a day fit.
        result = gen.generate_week(["exp"], 64, 120, 18, 22, seed=3)
        self.assertEqual(result["placed"], 14)
        self.assertFalse(same_kind_overlaps(result["rows"]))
        self.assertTrue(result["warnings"])

    def test_past_midnight_window(self):
        result = gen.generate_week(["zuo"], 7, 180, 22, 4, seed=5, values={"zuo": 8})
        self.assertEqual(result["placed"], 7)
        for row in result["rows"]:
            start, end = gen.parse_hhmm(row["start"]), gen.parse_hhmm(row["end"])
            self.assertEqual(gen.row_length(start, end), 180)
            self.assertTrue(start >= 22 * 60 or start <= 60)
        self.assertFalse(same_kind_overlaps(result["rows"]))

    def test_whole_day_events(self):
        result = gen.generate_week(["goblin", "rumi"], 7, 1440, 0, 24, seed=2)
        self.assertEqual(result["placed"], 7)
        self.assertTrue(all((row["start"], row["end"]) == ("00:00", "24:00") for row in result["rows"]))
        self.assertFalse(same_kind_overlaps(result["rows"]))

    def test_append_keeps_clear_of_existing(self):
        existing = [{"kind": "exp", "days": [1, 2, 3, 4, 5, 6, 7], "start": "00:00", "end": "24:00", "value": 50, "on": True}]
        result = gen.generate_week(["exp", "drop"], 7, 60, 18, 22, seed=4, existing=existing)
        self.assertTrue(all(e["kind"] == "drop" for e in result["events"]))
        self.assertEqual(result["placed"], 7)
        self.assertFalse(same_kind_overlaps(existing + result["rows"]))

    def test_exclusive(self):
        result = gen.generate_week(list(ALL), 21, 60, 18, 21, seed=6, exclusive=True)
        spans = spans_of(result["rows"])
        for i, (_k, start, length) in enumerate(spans):
            for _o, other_start, other_length in spans[i + 1:]:
                self.assertFalse(gen._overlaps(start, length, other_start, other_length))
        self.assertEqual(result["placed"], 21)

    def test_row_limit(self):
        existing = [{"kind": "exp", "days": [1], "start": "01:00", "end": "02:00", "value": 5, "on": True}] * 60
        result = gen.generate_week(list(ALL), 20, 60, 10, 22, seed=7, existing=existing)
        self.assertLessEqual(len(result["rows"]), 4)
        self.assertTrue(any("Limit" in text for text in result["warnings"]))
        with self.assertRaises(ValueError):
            gen.generate_week(["exp"], 1, 60, 10, 22, existing=existing + existing[:4])

    def test_bad_input(self):
        for args in ((["exp"], 0, 60, 10, 20), ([], 5, 60, 10, 20), (["exp"], 5, 45, 10, 20), (["exp"], 5, 240, 10, 12)):
            with self.assertRaises(ValueError):
                gen.generate_week(*args)

    def test_seed_changes_week(self):
        a = gen.generate_week(list(ALL), 30, 120, 12, 24, seed=1)
        b = gen.generate_week(list(ALL), 30, 120, 12, 24, seed=2)
        self.assertNotEqual(a["events"], b["events"])
        self.assertEqual(a["events"], gen.generate_week(list(ALL), 30, 120, 12, 24, seed=1)["events"])


if __name__ == "__main__":
    unittest.main()
