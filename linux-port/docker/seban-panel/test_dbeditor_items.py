"""MT2009_PLUS_DB_EDITOR_V1: python3 test_dbeditor_items.py

The database editor's items and skills parts (dbeditor/items.py, skills.py,
common_items.py) through Flask's test client, with the database replaced by
FakeDB below - a small interpreter of exactly the statements these modules
send: world.item_proto, world.skill_proto, common.locale and the history
table player.web_dbeditor_history."""
import datetime
import io
import json
import os
import re
import tempfile
import unittest
import zipfile
from unittest.mock import patch

os.environ.setdefault("DB_USER", "test")
os.environ.setdefault("DB_PASSWORD", "test")
os.environ.setdefault("DB_HOST", "127.0.0.1")
os.environ.setdefault("DB_PORT", "1")
# "Zastosuj" (clientdata.py) builds the client data under <spool>/dbeditor.
SPOOL = tempfile.mkdtemp(prefix="dbe-spool-")
os.environ["DBEDITOR_SPOOL_ROOT"] = SPOOL

import app as panel  # noqa: E402
from dbeditor import clientdata, common_items, items, skills  # noqa: E402
from m2clientpack import clientfiles, dbdata, eterpack  # noqa: E402


def item(vnum, name, type_=1, subtype=0, level=0, applies=((17, 22), (0, 0), (0, 0)), values=(0, 15, 19, 13, 15, 0)):
    row = {c: 0 for c in items.SPECS}
    row.update(vnum=vnum, locale_name=name, type=type_, subtype=subtype, size=2, stack=1,
               limittype0=1, limitvalue0=level)
    for i, (t, v) in enumerate(applies):
        row[f"applytype{i}"], row[f"applyvalue{i}"] = t, v
    for i, v in enumerate(values):
        row[f"value{i}"] = v
    for i in range(6):
        row[f"socket{i}"] = -1
    return row


def skill(vnum, name, type_, point="-(2*atk + (atk + str*7)*k)", cooldown="12", duration=""):
    row = {c: "" for c in skills.FORMULA_INFO}
    row.update(dwVnum=vnum, szName=name.encode("latin1"), bType=type_, bMaxLevel=1, bLevelLimit=0, iMaxHit=0,
               dwTargetRange=1000, dwSplashRange=0, szPointOn="HP", szPointOn2="NONE", szPointOn3="",
               setFlag="ATTACK", szPointPoly=point, szMasterBonusPoly=point, szCooldownPoly=cooldown,
               szDurationPoly=duration, szSPCostPoly="40+100*k", szGrandMasterAddSPCostPoly="40+100*k")
    return row


class FakeCursor:
    def __init__(self, fake):
        self.fake = fake
        self.result = []
        self.rowcount = 0
        self.lastrowid = None

    def execute(self, sql, params=()):
        self.result, self.rowcount = self.fake.execute(sql, list(params or ()))

    def fetchall(self):
        return self.result

    def fetchone(self):
        return self.result[0] if self.result else None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


class FakeConnection:
    def __init__(self, fake):
        self.fake = fake

    def cursor(self):
        return FakeCursor(self.fake)

    def begin(self):
        self.fake.snapshot = self.fake.copy_state()

    def commit(self):
        self.fake.snapshot = None

    def rollback(self):
        if self.fake.snapshot:
            self.fake.restore(self.fake.snapshot)

    def close(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


class FakeDB:
    SELECT = re.compile(r"^\s*SELECT (?P<cols>.+?) FROM (?P<table>[\w.]+)(?: WHERE (?P<where>.+?))?"
                        r"(?: GROUP BY (?P<group>\w+))?(?: ORDER BY (?P<order>[\w ,]+?))?(?: LIMIT (?P<limit>\d+))?"
                        r"(?P<lock> FOR UPDATE)?\s*$", re.S)

    def __init__(self):
        self.tables = {
            "world.item_proto": {r["vnum"]: r for r in [
                item(140, "Miecz Bojowy+0", level=65, values=(0, 18, 40, 100, 140, 0)),
                item(141, "Miecz Bojowy+1", level=65, values=(0, 18, 40, 100, 140, 15)),
                item(149, "Miecz Bojowy+9", level=65, values=(0, 18, 40, 100, 140, 137)),
                item(11299, "Zbroja Z Czarnej Stali+9", type_=2, level=70, applies=((19, -6), (0, 0), (0, 0)),
                     values=(0, 90, 0, 12, 0, 27)),
                item(27001, "Czerwona Mikstura(M)", type_=3, applies=((0, 0),) * 3, values=(300, 0, 0, 0, 0, 0)),
            ]},
            "world.skill_proto": {r["dwVnum"]: r for r in [
                skill(1, "THREE_WAY_CUT", 1, "-( 1.1*atk + (0.5*atk +  1.5 * str)*k)"),
                skill(3, "BERSERK", 1, "33*k", "63+10*k", "60+90*k"),
                skill(91, "FLYING_TALISMAN", 4, "-(70 + 4*lv + (20*iq+5*mwep+50)*ar*k)", "7"),
            ]},
            # MT2009_PLUS_ITEM_EXTRA_APPLY_V1: (vnum, slot) -> row
            "world.item_extra_apply": {},
        }
        for row in self.tables["world.item_proto"].values():
            row["locale_name"] = row["locale_name"].encode("cp1250")  # stored as the game's bytes
        self.history = []
        self.snapshot = None
        self.statements = []

    # -- state -----------------------------------------------------------------
    def copy_state(self):
        return ({t: {k: dict(v) for k, v in rows.items()} for t, rows in self.tables.items()},
                [dict(h) for h in self.history])

    def restore(self, state):
        self.tables, self.history = state

    def rows(self, sql, params=()):
        return self.execute(sql, list(params or ()))[0]

    def db(self):
        return FakeConnection(self)

    # -- SQL ---------------------------------------------------------------------
    def execute(self, sql, params):
        sql = " ".join(sql.split())
        self.statements.append(sql)
        if sql.startswith("CREATE TABLE"):
            return [], 0
        if sql.startswith("INSERT INTO player.web_dbeditor_history"):
            cols = re.search(r"\(([^)]+)\) VALUES", sql).group(1).split(",")
            row = dict(zip(cols, params))
            row.update(id=len(self.history) + 1, changed_at=datetime.datetime(2026, 10, 4, 12, 0, len(self.history) % 60),
                       reverted_in=None, applied_at=None)
            self.history.append(row)
            return [], 1
        if sql.startswith("UPDATE player.web_dbeditor_history SET applied_at"):
            count = 0
            for row in self.history:
                if row["applied_at"] is None:
                    row["applied_at"] = datetime.datetime(2026, 10, 4, 13, 0)
                    count += 1
            return [], count
        if sql.startswith("UPDATE player.web_dbeditor_history SET reverted_in"):
            batch, ids = params[0], set(params[1:])
            for row in self.history:
                if row["id"] in ids:
                    row["reverted_in"] = batch
            return [], len(ids)
        if sql.startswith("UPDATE world."):
            match = re.match(r"UPDATE ([\w.]+) SET (.+) WHERE `(\w+)`=%s$", sql)
            table, sets, _key = match.groups()
            row = self.tables[table][int(params[-1])]
            for part, value in zip(sets.split(", "), params):
                col = re.match(r"`(\w+)`=", part).group(1)
                row[col] = bytes.fromhex(value) if "UNHEX" in part else value
            return [], 1
        if "FROM player.web_dbeditor_history" in sql:
            return self.history_select(sql, params), 0
        if sql.startswith("INSERT INTO world.item_extra_apply"):
            cols = [c.strip("` ") for c in re.search(r"\(([^)]+)\) VALUES", sql).group(1).split(",")]
            row = dict(zip(cols, (int(p) for p in params)))
            key = (row["vnum"], row["slot"])
            assert key not in self.tables["world.item_extra_apply"], "duplicate primary key"
            self.tables["world.item_extra_apply"][key] = row
            return [], 1
        if sql.startswith("DELETE FROM world.item_extra_apply"):
            assert re.match(r"DELETE FROM world\.item_extra_apply WHERE `vnum`=%s AND `slot`=%s LIMIT 1$", sql), sql
            gone = self.tables["world.item_extra_apply"].pop((int(params[0]), int(params[1])), None)
            return [], 1 if gone else 0
        if "FROM common.locale" in sql:
            return [{"mValue": " ".join(str(v) for v in skills.DEFAULT_POWER)}], 0
        match = self.SELECT.match(sql)
        if not match:
            raise AssertionError("FakeDB cannot run: " + sql)
        table = self.tables[match["table"]]
        params = list(params)
        found = [r for r in table.values() if self.where(match["where"], r, params[:])] if match["where"] else list(table.values())
        if match["group"]:
            counts = {}
            for r in found:
                counts[r[match["group"]]] = counts.get(r[match["group"]], 0) + 1
            return [{match["group"]: k, "count": v} for k, v in sorted(counts.items())], 0
        found.sort(key=lambda r: r.get("vnum", r.get("dwVnum")))
        if match["limit"]:
            found = found[:int(match["limit"])]
        return [self.project(match["cols"], r) for r in found], 0

    @staticmethod
    def project(cols, row):
        out = {}
        for part in re.split(r",\s*", cols):
            if part == "*":
                out.update(row)
                continue
            cast = re.match(r"CAST\(`(\w+)` AS BINARY\) AS `(\w+)`", part)
            if cast:
                value = row.get(cast.group(1), b"")
                out[cast.group(2)] = value if isinstance(value, bytes) else str(value).encode("cp1250")
                continue
            name = part.strip("`")
            value = row[name]
            # The connection is utf8mb4: a cp1250 column comes back as text.
            out[name] = value.decode("cp1250") if isinstance(value, bytes) and name == "locale_name" else value
        return out

    def where(self, where, row, params):
        def name(r):
            value = r["locale_name"]
            return value.decode("cp1250") if isinstance(value, bytes) else value

        def like(pattern, text):
            return pattern.strip("%").lower() in text.lower()

        where = where.replace("BETWEEN %s AND %s", "BETWEEN %s _AND_ %s")
        for part in re.split(r" AND (?![^(]*\))", where):
            part = part.strip().replace("_AND_", "AND")
            if part in ("vnum=%s", "`vnum`=%s"):
                ok = row["vnum"] == int(params.pop(0))
            elif part == "`slot`=%s":
                ok = row["slot"] == int(params.pop(0))
            elif part == "apply_type <> %s":
                ok = row["apply_type"] != int(params.pop(0))
            elif part in ("dwVnum=%s", "`dwVnum`=%s"):
                ok = row["dwVnum"] == int(params.pop(0))
            elif part == "(vnum=%s OR locale_name LIKE %s)":
                vnum, pattern = params.pop(0), params.pop(0)
                ok = row["vnum"] == vnum or like(pattern, name(row))
            elif part == "locale_name LIKE %s":
                ok = like(params.pop(0), name(row))
            elif part in ("type=%s", "subtype=%s"):
                ok = row[part[:-3]] == int(params.pop(0))
            elif re.fullmatch(r"(vnum|dwVnum) IN \((%s,?)+\)", part):
                count = part.count("%s")
                wanted, params[:] = params[:count], params[count:]
                ok = row["vnum" if part.startswith("vnum") else "dwVnum"] in wanted
            elif part == "vnum BETWEEN %s AND %s":
                low, high = params.pop(0), params.pop(0)
                ok = low <= row["vnum"] <= high
            elif part.startswith("(MOD(vnum,10)=0"):
                ok = row["vnum"] % 10 == 0 or not re.search(r"\+\d$", name(row))
            else:
                raise AssertionError("FakeDB: unknown condition " + part)
            if not ok:
                return False
        return True

    def history_select(self, sql, params):
        rows = self.history
        if "GROUP BY batch" in sql:
            if "tbl=%s" in sql:
                wanted = params.pop(0)
                rows = [r for r in rows if r["tbl"] == wanted]
            if "row_key=%s" in sql:
                wanted = params.pop(0)
                rows = [r for r in rows if r["row_key"] == wanted]
            batches = {}
            for r in rows:
                b = batches.setdefault(r["batch"], {"batch": r["batch"], "first_id": r["id"], "changed_at": r["changed_at"],
                                                    "who": r["who"], "note": r["note"], "tbl": r["tbl"], "fields": 0,
                                                    "row_count": 0, "reverted_in": None, "unapplied": 0})
                b["fields"] += 1
                b["unapplied"] += r["applied_at"] is None
                b["reverted_in"] = b["reverted_in"] or r["reverted_in"]
            return sorted(batches.values(), key=lambda b: -b["first_id"])
        if "WHERE batch IN" in sql:
            return [dict(r) for r in rows if r["batch"] in params]
        if "WHERE id=%s" in sql:
            return [dict(r) for r in rows if r["id"] == params[0] and r["reverted_in"] is None]
        if "WHERE batch=%s" in sql:
            return [dict(r) for r in reversed(rows) if r["batch"] == params[0] and r["reverted_in"] is None]
        if "WHERE applied_at IS NULL" in sql:
            return [dict(r) for r in rows if r["applied_at"] is None]
        if re.search(r"FROM player\.web_dbeditor_history ORDER BY id$", sql):
            return [dict(r) for r in rows]
        raise AssertionError("FakeDB: history query " + sql)


class DbEditorItemsTests(unittest.TestCase):
    def setUp(self):
        self.fake = FakeDB()
        common_items._STATE["table_ready"] = False
        common_items._CTX.update(rows=self.fake.rows, db=self.fake.db)
        self.patches = [
            patch.object(panel, "settings", return_value={"setup_complete": "1", "auth_enabled": "0"}),
            patch.object(panel, "check_all_notifications", return_value=None, create=True),
            # The rest of the panel (menus, notifications) sees an empty database.
            patch.object(panel, "rows", side_effect=self.panel_rows),
            patch.object(skills, "_POWER_CACHE", {"at": 0.0, "table": None, "source": "default"}),
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
        if re.search(r"item_proto|skill_proto|web_dbeditor_history|common\.locale", sql):
            return self.fake.rows(sql, params)
        return []

    def post(self, url, data, **kw):
        data = dict(data)
        data.setdefault("dbe_csrf", "tok")
        return self.client.post(url, data=data, **kw)

    def item_form(self, vnum, **changes):
        """What the edit page's form sends for an unchanged item, plus changes."""
        row = self.fake.tables["world.item_proto"][vnum]
        decorated = items.decorate(dict(row, locale_name=row["locale_name"]))
        form = {"locale_name": decorated["locale_name"], "level": decorated["level"]}
        for i in range(3):
            form[f"applytype{i}"], form[f"applyvalue{i}"] = row[f"applytype{i}"], row[f"applyvalue{i}"]
        for col, _l, _h in items.value_fields(row["type"], row["subtype"]):
            form[col] = row[col]
        friendly = items.friendly_columns(decorated)
        for col in items.ADVANCED_ORDER:
            if col not in friendly:
                form[f"adv_{col}"] = row[col]
        form.update(changes)
        return form

    # ---- items -------------------------------------------------------------
    def test_hub_lists_the_parts(self):
        page = self.client.get("/db/").get_data(as_text=True)
        self.assertIn("Przedmioty", page)
        self.assertIn("Umiejętności", page)
        self.assertIn("Historia zmian", page)

    def test_search_by_name_vnum_and_type(self):
        page = self.client.get("/db/items?q=bojowy").get_data(as_text=True)
        self.assertIn("Miecz Bojowy+9", page)
        self.assertNotIn("Czerwona Mikstura", page)
        page = self.client.get("/db/items?q=27001").get_data(as_text=True)
        self.assertIn("Czerwona Mikstura(M)", page)
        page = self.client.get("/db/items?typ=2").get_data(as_text=True)
        self.assertIn("Zbroja Z Czarnej Stali+9", page)
        self.assertNotIn("Miecz Bojowy+0", page)
        page = self.client.get("/db/items?q=bojowy&rodziny=1").get_data(as_text=True)
        self.assertIn("Miecz Bojowy+0", page)
        self.assertNotIn("Miecz Bojowy+9", page)

    def test_edit_page_labels_by_type(self):
        page = self.client.get("/db/items/149").get_data(as_text=True)
        self.assertIn("Atak – minimum", page)
        self.assertIn("Atak magiczny – maksimum", page)
        self.assertIn("Szybkość ataku (%)", page)          # applytype 17 in Polish
        self.assertIn("Rodzina ulepszeń", page)
        page = self.client.get("/db/items/11299").get_data(as_text=True)
        self.assertIn("Obrona", page)
        self.assertIn("Szybkość ruchu", page)
        self.assertNotIn("Atak – minimum", page)
        self.assertEqual(self.client.get("/db/items/999").status_code, 404)

    def test_save_name_level_bonus_and_history(self):
        form = self.item_form(149, locale_name="Miecz Wojny+9", level="50", applytype1="43", applyvalue1="10", value4="160")
        res = self.post("/db/items/149", form, follow_redirects=True)
        self.assertEqual(res.status_code, 200)
        row = self.fake.tables["world.item_proto"][149]
        self.assertEqual(row["locale_name"], "Miecz Wojny+9".encode("cp1250"))
        self.assertEqual((row["limittype0"], row["limitvalue0"]), (1, 50))
        self.assertEqual((row["applytype1"], row["applyvalue1"]), (43, 10))
        self.assertEqual(row["value4"], 160)
        self.assertEqual(self.fake.tables["world.item_proto"][141]["limitvalue0"], 65)  # family untouched
        cols = {h["col"]: (h["old_value"], h["new_value"]) for h in self.fake.history}
        self.assertEqual(cols["limitvalue0"], ("65", "50"))
        self.assertEqual(cols["locale_name"], ("Miecz Bojowy+9", "Miecz Wojny+9"))
        self.assertEqual(len({h["batch"] for h in self.fake.history}), 1)
        self.assertIn("Zmiany czekają na zastosowanie", res.get_data(as_text=True))
        self.assertIn("dbe-pending", self.client.get("/db/items").get_data(as_text=True))

    def test_polish_name_is_written_as_cp1250(self):
        self.post("/db/items/149", self.item_form(149, locale_name="Żółć Ostrza+9"))
        self.assertEqual(self.fake.tables["world.item_proto"][149]["locale_name"], "Żółć Ostrza+9".encode("cp1250"))
        self.assertTrue(any("UNHEX(%s)" in s for s in self.fake.statements))

    def test_invalid_input_saves_nothing(self):
        before = dict(self.fake.tables["world.item_proto"][149])
        for bad in ({"level": "abc"}, {"applyvalue0": "9999999999"}, {"locale_name": "x" * 40},
                    {"locale_name": "Miecz ☃"}, {"value3": "500", "value4": "100"}):
            res = self.post("/db/items/149", self.item_form(149, **bad))
            self.assertIn("Nic nie zapisano", res.get_data(as_text=True), bad)
        self.assertEqual(self.fake.tables["world.item_proto"][149], before)
        self.assertEqual(self.fake.history, [])

    def test_csrf_is_required(self):
        res = self.client.post("/db/items/149", data=self.item_form(149, level="1"), follow_redirects=True)
        self.assertIn("Sesja formularza wygasła", res.get_data(as_text=True))
        self.assertEqual(self.fake.tables["world.item_proto"][149]["limitvalue0"], 65)

    def test_family_same_value_and_names(self):
        form = self.item_form(149, locale_name="Miecz Wojny+9", level="50", family="1", family_mode="same")
        self.post("/db/items/149", form)
        names = {v: r["locale_name"].decode("cp1250") for v, r in self.fake.tables["world.item_proto"].items() if v < 150}
        self.assertEqual(names, {140: "Miecz Wojny+0", 141: "Miecz Wojny+1", 149: "Miecz Wojny+9"})
        self.assertEqual({r["limitvalue0"] for v, r in self.fake.tables["world.item_proto"].items() if v < 150}, {50})
        self.assertEqual(len({h["batch"] for h in self.fake.history}), 1)
        self.assertEqual(self.fake.tables["world.item_proto"][11299]["limitvalue0"], 70)

    def test_family_delta(self):
        form = self.item_form(149, value5="157", family="1", family_mode="delta")  # +20 on every refine bonus
        self.post("/db/items/149", form)
        protos = self.fake.tables["world.item_proto"]
        self.assertEqual([protos[v]["value5"] for v in (140, 141, 149)], [20, 35, 157])

    def test_armour_defence(self):
        # value2 of an armour is an advanced column - no attack min/max check
        res = self.post("/db/items/11299", self.item_form(11299, value1="95"), follow_redirects=True)
        self.assertNotIn("Nic nie zapisano", res.get_data(as_text=True))
        self.assertEqual(self.fake.tables["world.item_proto"][11299]["value1"], 95)

    def test_advanced_raw_column(self):
        self.post("/db/items/149", self.item_form(149, adv_gold="12345"))
        self.assertEqual(self.fake.tables["world.item_proto"][149]["gold"], 12345)

    def test_no_change_records_nothing(self):
        res = self.post("/db/items/149", self.item_form(149), follow_redirects=True)
        self.assertIn("Brak zmian", res.get_data(as_text=True))
        self.assertEqual(self.fake.history, [])

    # ---- history, undo, pending ------------------------------------------------
    def test_revert_batch_and_pending(self):
        self.post("/db/items/149", self.item_form(149, level="50", applyvalue0="30"))
        self.assertEqual(common_items.pending_count(), 2)
        batch = self.fake.history[0]["batch"]
        page = self.client.get("/db/historia").get_data(as_text=True)
        self.assertIn("Miecz Bojowy+9", page)
        self.assertIn("czeka na restart", page)
        res = self.post("/db/historia/cofnij", {"batch": batch, "back": "/db/historia"}, follow_redirects=True)
        self.assertIn("Cofnięto 2", res.get_data(as_text=True))
        row = self.fake.tables["world.item_proto"][149]
        self.assertEqual((row["limitvalue0"], row["applyvalue0"]), (65, 22))
        self.assertTrue(all(h["reverted_in"] for h in self.fake.history if h["batch"] == batch))
        # change + its undo before a restart: nothing is pending any more
        self.assertEqual(common_items.pending_changes(), [])
        res = self.post("/db/historia/cofnij", {"batch": batch}, follow_redirects=True)
        self.assertIn("już cofnięta", res.get_data(as_text=True))

    def test_revert_conflict_needs_force(self):
        self.post("/db/items/149", self.item_form(149, level="50"))
        first = self.fake.history[0]["batch"]
        self.post("/db/items/149", self.item_form(149, level="40"))
        res = self.post("/db/historia/cofnij", {"batch": first}, follow_redirects=True)
        self.assertIn("zmieniono później", res.get_data(as_text=True))
        self.assertEqual(self.fake.tables["world.item_proto"][149]["limitvalue0"], 40)
        self.post("/db/historia/cofnij", {"batch": first, "force": "1"})
        self.assertEqual(self.fake.tables["world.item_proto"][149]["limitvalue0"], 65)

    def test_revert_single_field(self):
        self.post("/db/items/149", self.item_form(149, level="50", applyvalue0="30"))
        field = next(h for h in self.fake.history if h["col"] == "applyvalue0")
        self.post("/db/historia/cofnij", {"id": str(field["id"])})
        row = self.fake.tables["world.item_proto"][149]
        self.assertEqual((row["limitvalue0"], row["applyvalue0"]), (50, 22))

    def test_mark_applied_clears_pending(self):
        self.post("/db/items/149", self.item_form(149, level="50"))
        self.assertEqual(common_items.pending_count(), 1)
        self.assertEqual(common_items.mark_applied(), 1)
        self.assertEqual(common_items.pending_count(), 0)
        self.client.get("/db/items")  # the save's flash message
        self.assertNotIn("dbe-pending", self.client.get("/db/items").get_data(as_text=True))

    # ---- skills -----------------------------------------------------------------
    def test_skill_list_and_edit_page(self):
        page = self.client.get("/db/skills?klasa=1").get_data(as_text=True)
        self.assertIn("Trzystronne Cięcie", page)
        self.assertIn("Berserk", page)
        self.assertNotIn("Latający Talizman", page)
        page = self.client.get("/db/skills/3").get_data(as_text=True)
        self.assertIn("Czas odnowienia", page)
        self.assertIn(">M1<", page)
        self.assertIn(">G1<", page)
        # 63+10*k at P: k = 125/100 -> 75
        self.assertIn(">75<", page)

    def test_skill_save_and_master_mirror(self):
        form = {c: self.fake.tables["world.skill_proto"][3][c] for c in skills.SPECS}
        form.update(szCooldownPoly="(63+10*k)*0.8", szPointPoly="40*k", same_master="1")
        res = self.post("/db/skills/3", form, follow_redirects=True)
        self.assertIn("Zapisano", res.get_data(as_text=True))
        row = self.fake.tables["world.skill_proto"][3]
        self.assertEqual(row["szCooldownPoly"], "(63+10*k)*0.8")
        self.assertEqual(row["szMasterBonusPoly"], "40*k")
        self.assertEqual({h["tbl"] for h in self.fake.history}, {"world.skill_proto"})

    def test_skill_bad_formula_is_refused(self):
        form = {c: self.fake.tables["world.skill_proto"][3][c] for c in skills.SPECS}
        for bad in ("63+", "63+10*kk", "(63+10*k", "63+10*-k", "63+ś"):
            form["szCooldownPoly"] = bad
            res = self.post("/db/skills/3", form)
            self.assertIn("Nic nie zapisano", res.get_data(as_text=True), bad)
        self.assertEqual(self.fake.tables["world.skill_proto"][3]["szCooldownPoly"], "63+10*k")

    def test_skill_preview_endpoint(self):
        res = self.client.post("/db/skills/preview", json={"column": "szDurationPoly", "formula": "60+90*k", "max_level": 1})
        data = res.get_json()
        self.assertTrue(data["ok"])
        self.assertEqual([v["label"] for v in data["values"]], ["1", "10", "M1", "G1", "P"])
        self.assertEqual(data["values"][-1]["min"], 172)     # 60 + 90*1.25 = 172.5 -> int
        res = self.client.post("/db/skills/preview", json={"column": "szPointPoly", "formula": "-(atk*k + number(100,200))",
                                                             "sample": {"atk": "1000"}})
        data = res.get_json()
        self.assertEqual((data["values"][2]["min"], data["values"][2]["max"]), (-700, -600))  # M1, k = 0.5
        self.assertFalse(self.client.post("/db/skills/preview", json={"column": "szCooldownPoly", "formula": "1+"}).get_json()["ok"])

    # ---- the parser itself ---------------------------------------------------------
    def test_parser_matches_libpoly(self):
        cases = {"-k": -1.25, "2^3": 8, "min(3, -2)": -2, "log(10, 1000)": 3, "7 % 4": 3, "floor(2.7)": 2,
                 "(iq*0.3+5)*(2*k+0.5)/(k+1.5)": (60 * 0.3 + 5) * 3 / 2.75, "1/0": 0, "sqrt(0-4)": 0}
        for text, expected in cases.items():
            rpn, error, _w, _r = skills.check_formula(text)
            self.assertIsNone(error, text)
            self.assertAlmostEqual(skills.evaluate(rpn, {"k": 1.25, "iq": 60}), expected, places=6, msg=text)
        for bad in ("", "  ", "1+", "2*-3", ")", "log10(5)", ".5", "x", "a b", "min(1)", "1,2"):
            _rpn, error, _w, _r = skills.check_formula(bad)
            if bad == "":
                self.assertIsNone(error)
            else:
                self.assertIsNotNone(error, bad)
        _rpn, _e, warnings, _r = skills.check_formula("atk*k", "szCooldownPoly")
        self.assertTrue(warnings)


    # ---- "Zastosuj" and the client data (clientdata.py) --------------------
    def test_apply_restarts_marks_and_zips_client_files(self):
        stamp_path = os.path.join(SPOOL, "dbdata_stamp.txt")
        if os.path.exists(stamp_path):
            os.remove(stamp_path)
        self.post("/db/items/149", self.item_form(149, locale_name="Miecz Próby+9"))
        # MT2009_PLUS_DBDATA_STAMP_V1: the save put the game's stamp in the spool
        clientdata.wait_stamp()
        with open(stamp_path, "rb") as f:
            saved = dbdata.read_stamp_text(f.read())
        self.assertTrue(saved.startswith(dbdata.latest_base().version + "-"), saved)
        page = self.client.get("/db/apply").get_data(as_text=True)
        self.assertIn("1</b> zmian czeka", page)
        self.assertIn("Miecz Próby+9", page)
        queued = []
        with patch.object(panel, "restart_in_flight", return_value=False), \
                patch.object(panel, "queue_rate_restart", side_effect=lambda rates: queued.append(rates)), \
                patch.object(panel, "read_rates", return_value={"exp": 100, "drop": 100, "yang": 100}):
            res = self.post("/db/apply", {"action": "apply", "confirmation": "restart"}, follow_redirects=True)
        self.assertIn("Restart rdzeni zlecony", res.get_data(as_text=True))
        self.assertEqual(len(queued), 1)
        self.assertTrue(all(r["applied_at"] for r in self.fake.history))
        self.assertEqual(common_items.pending_count(), 0)
        # MT2009_PLUS_DBDATA_STAMP_V1 (popup): after the restart every editor page asks for the zip ...
        clientdata.wait_stamp()
        page = self.client.get("/db/").get_data(as_text=True)
        self.assertIn('id="dbe-zip-popup"', page)
        self.assertIn("UWAGA! Aby zmiany z edytora bazy danych były widoczne w Twoim kliencie gry", page)
        self.assertIn("Pobierz pliki klienta (zip)", page)
        res = self.client.get("/db/clientdata.zip")
        self.assertEqual(res.status_code, 200)
        self.assertIn("attachment; filename=\"dbdata-MT2009-PLUS-klient-", res.headers["Content-Disposition"])
        with zipfile.ZipFile(io.BytesIO(res.get_data())) as z:
            self.assertEqual(sorted(z.namelist()), ["CZYTAJ_MNIE.txt", "dbdata_stamp.txt", "pack/dbdata.data",
                                                    "pack/dbdata.index"])
            index, data = z.read("pack/dbdata.index"), z.read("pack/dbdata.data")
            self.assertEqual(dbdata.read_stamp_text(z.read("dbdata_stamp.txt")), saved)  # the zip's = the game's
        entry = [e for e in eterpack.read_index_bytes(index)[1] if e.name == "gamedata/item_proto"][0]
        recs = clientfiles.read_item_proto(eterpack.read_entry(data, entry))[2]
        self.assertEqual(clientfiles.get_field(recs[149], "locale_name"), "Miecz Próby+9")
        res = self.client.get("/db/clientdata.zip?oryginal=1")
        with zipfile.ZipFile(io.BytesIO(res.get_data())) as z:
            self.assertEqual((z.read("pack/dbdata.index"), z.read("pack/dbdata.data")), dbdata.latest_base().original())
            self.assertEqual(dbdata.read_stamp_text(z.read("dbdata_stamp.txt")), dbdata.latest_base().version)
        with open(stamp_path, "rb") as f:  # the original files leave the game's stamp as it was
            self.assertEqual(dbdata.read_stamp_text(f.read()), saved)
        with open(os.path.join(SPOOL, "dbeditor", "clientdata-download.json"), encoding="utf-8") as f:
            download = json.load(f)
        self.assertEqual((download["base"], download["stamp"]), (dbdata.latest_base().version, saved))
        page = self.client.get("/db/apply").get_data(as_text=True)
        self.assertIn("Pobierz aktualne pliki klienta (zip)", page)
        self.assertNotIn('id="dbe-zip-popup"', page)  # ... until the zip is downloaded
        self.assertIn(dbdata.latest_base().version, page)
        with open(os.path.join(SPOOL, "dbeditor", "last-apply.json"), encoding="utf-8") as f:
            self.assertEqual(json.load(f)["changes"], 1)

    def test_apply_needs_confirmation_and_waits_for_a_running_restart(self):
        self.post("/db/items/149", self.item_form(149, locale_name="Miecz Próby+9"))
        res = self.post("/db/apply", {"action": "apply", "confirmation": ""}, follow_redirects=True)
        self.assertIn("wpisz RESTART", res.get_data(as_text=True))
        with patch.object(panel, "restart_in_flight", return_value=True):
            res = self.post("/db/apply", {"action": "apply", "confirmation": "RESTART"}, follow_redirects=True)
        self.assertIn("Poprzedni restart jeszcze trwa", res.get_data(as_text=True))
        self.assertEqual(common_items.pending_count(), 1)

    # ---- MT2009_PLUS_ITEM_EXTRA_APPLY_V1: "Dodatkowe bonusy (ponad 3)" --------
    STAT_LINES = [("15", "20"), ("13", "20"), ("14", "20"), ("12", "20")]  # INT, WIT, ZR, SIL

    def extras(self, vnum):
        lines = [r for (v, _s), r in self.fake.tables["world.item_extra_apply"].items() if v == vnum]
        return [(r["slot"], r["apply_type"], r["apply_value"]) for r in sorted(lines, key=lambda r: r["slot"])]

    def post_extras(self, vnum, lines, **extra):
        data = {"extra_type": [t for t, _v in lines], "extra_value": [v for _t, v in lines]}
        data.update(extra)
        return self.post(f"/db/items/{vnum}/extra", data, follow_redirects=True)

    def test_extra_section_on_the_edit_page(self):
        page = self.client.get("/db/items/149").get_data(as_text=True)
        self.assertIn("Dodatkowe bonusy (ponad 3)", page)
        self.assertIn("Pierwsze 3 bonusy to własne bonusy przedmiotu", page)
        self.assertIn("pobraniu plików klienta (zip)", page)
        self.assertIn("Skopiuj tę listę na wszystkie poziomy +0…+9", page)
        self.assertIn('name="extra_type"', page)
        self.assertIn("data-dbe-extra-template", page)
        self.assertNotIn("nie jest zakładany", page)
        page = self.client.get("/db/items/27001").get_data(as_text=True)   # a potion
        self.assertIn("nie jest zakładany", page)
        self.assertNotIn("Skopiuj tę listę", page)                          # no family

    def test_extra_lines_saved_with_history_and_pending(self):
        res = self.post_extras(149, self.STAT_LINES)
        page = res.get_data(as_text=True)
        self.assertIn("Zapisano dodatkowe bonusy (4 na przedmiot) w 1 przedmiot", page)
        self.assertEqual(self.extras(149), [(1, 15, 20), (2, 13, 20), (3, 14, 20), (4, 12, 20)])
        self.assertEqual(self.extras(141), [])                              # family untouched
        self.assertIn("Inteligencja +20, Witalność +20, Zręczność +20, Siła +20", page)
        self.assertEqual(self.fake.tables["world.item_proto"][149]["applytype0"], 17)  # the proto's own stay
        hist = [h for h in self.fake.history if h["tbl"] == "world.item_extra_apply"]
        self.assertEqual(len(hist), 4)
        self.assertEqual({h["col"] for h in hist}, {"*"})
        self.assertEqual({h["row_key"] for h in hist}, {"149:1", "149:2", "149:3", "149:4"})
        self.assertEqual(len({h["batch"] for h in hist}), 1)
        self.assertEqual(common_items.pending_count(), 4)
        page = self.client.get("/db/apply").get_data(as_text=True)
        self.assertIn("4</b> zmian czeka", page)
        self.assertIn("dodatkowy bonus", page)
        self.assertIn("1. Inteligencja +20 [15]", page)
        page = self.client.get("/db/historia").get_data(as_text=True)
        self.assertIn('href="/db/items/149"', page)
        self.assertIn("4. Siła +20 [12]", page)
        # the item page's own list of these changes
        self.assertIn("Ostatnie zmiany dodatkowych bonusów", self.client.get("/db/items/149").get_data(as_text=True))
        # the same list again: nothing to save
        res = self.post_extras(149, self.STAT_LINES)
        self.assertIn("Brak zmian", res.get_data(as_text=True))
        self.assertEqual(len(self.fake.history), 4)

    def test_extra_lines_change_remove_and_undo(self):
        self.post_extras(149, self.STAT_LINES)
        first = self.fake.history[0]["batch"]
        # the second line changed, the last one removed (kind "brak bonusu"), one added
        self.post_extras(149, [("15", "20"), ("13", "30"), ("14", "20"), ("0", "20"), ("43", "5")])
        self.assertEqual(self.extras(149), [(1, 15, 20), (2, 13, 30), (3, 14, 20), (4, 43, 5)])
        second = self.fake.history[-1]["batch"]
        self.assertNotEqual(first, second)
        res = self.post("/db/historia/cofnij", {"batch": second}, follow_redirects=True)
        self.assertIn("Cofnięto", res.get_data(as_text=True))
        self.assertEqual(self.extras(149), [(1, 15, 20), (2, 13, 20), (3, 14, 20), (4, 12, 20)])
        self.post("/db/historia/cofnij", {"batch": first})
        self.assertEqual(self.extras(149), [])
        self.assertEqual(common_items.pending_changes(), [])
        # everything removed at once
        self.post_extras(149, self.STAT_LINES)
        self.post_extras(149, [])
        self.assertEqual(self.extras(149), [])

    def test_extra_lines_copied_to_the_family(self):
        res = self.post_extras(149, self.STAT_LINES, extra_family="1")
        self.assertIn("w 3 przedmiot", res.get_data(as_text=True))
        for vnum in (140, 141, 149):
            self.assertEqual(self.extras(vnum), [(1, 15, 20), (2, 13, 20), (3, 14, 20), (4, 12, 20)], vnum)
        self.assertEqual(self.extras(11299), [])
        self.assertEqual(len({h["batch"] for h in self.fake.history}), 1)
        labels = {h["row_key"]: h["label"] for h in self.fake.history}
        self.assertEqual(labels["140:1"], "Miecz Bojowy+0")
        # the family table shows them
        page = self.client.get("/db/items/140").get_data(as_text=True)
        self.assertEqual(page.count("Inteligencja +20, Witalność +20, Zręczność +20, Siła +20"), 4)  # header + 3 rows
        # a shorter list for the family: the fourth line goes everywhere
        self.post_extras(140, self.STAT_LINES[:3], extra_family="1")
        for vnum in (140, 141, 149):
            self.assertEqual(len(self.extras(vnum)), 3, vnum)

    def test_extra_lines_invalid_input_saves_nothing(self):
        for bad in ([("15", "0")], [("15", "abc")], [("300", "5")], [("15", "9999999999")],
                    [("15", "1")] * 31):
            res = self.post_extras(149, bad)
            self.assertIn("Nic nie zapisano – popraw dodatkowe bonusy", res.get_data(as_text=True), bad[:1])
        self.assertEqual(self.fake.tables["world.item_extra_apply"], {})
        self.assertEqual(self.fake.history, [])
        # the page comes back with what was typed
        res = self.post_extras(149, [("15", "20"), ("13", "0")])
        page = res.get_data(as_text=True)
        self.assertIn("podaj wartość różną od 0", page)
        self.assertEqual(page.count('name="extra_value"'), 3)  # two typed + the template's
        res = self.client.post("/db/items/149/extra", data={"extra_type": ["15"], "extra_value": ["5"]},
                               follow_redirects=True)
        self.assertIn("Sesja formularza wygasła", res.get_data(as_text=True))
        self.assertEqual(self.fake.tables["world.item_extra_apply"], {})

    def test_extra_lines_reach_the_client_zip(self):
        self.post_extras(149, self.STAT_LINES)
        res = self.client.get("/db/clientdata.zip")
        self.assertEqual(res.status_code, 200)
        with zipfile.ZipFile(io.BytesIO(res.get_data())) as z:
            index, data = z.read("pack/dbdata.index"), z.read("pack/dbdata.data")
            readme = z.read("CZYTAJ_MNIE.txt").decode("utf-8-sig")
        entries = {e.name: e for e in eterpack.read_index_bytes(index)[1]}
        self.assertIn("gamedata/item_extra_apply.txt", entries)
        text = eterpack.read_entry(data, entries["gamedata/item_extra_apply.txt"]).decode("ascii")
        body = [l for l in text.split("\r\n") if l and not l.startswith("#")]
        self.assertEqual(body, ["149\t15\t20", "149\t13\t20", "149\t14\t20", "149\t12\t20"])
        self.assertIn("gamedata/item_extra_apply.txt", readme)
        page = self.client.get("/db/apply").get_data(as_text=True)
        self.assertIn("Dodatkowe bonusy (ponad 3): 4 linii na 1 przedmiotach", page)

    def test_item_overwritten_at_start_is_flagged(self):
        self.fake.tables["world.item_proto"][215] = dict(self.fake.tables["world.item_proto"][149], vnum=215)
        page = self.client.get("/db/items/215").get_data(as_text=True)
        self.assertIn("ustawiany przy każdym starcie serwera", page)
        page = self.client.get("/db/items/149").get_data(as_text=True)
        self.assertNotIn("ustawiany przy każdym starcie serwera", page)


if __name__ == "__main__":
    unittest.main()
