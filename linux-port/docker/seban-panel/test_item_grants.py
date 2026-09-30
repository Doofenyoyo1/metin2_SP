import unittest
from unittest.mock import MagicMock, patch
from flask import Flask
import item_grants as grants


class WorkerTests(unittest.TestCase):
    def run_status(self, status, age=0, cancel_wins=1):
        con = MagicMock()
        cur = con.cursor.return_value.__enter__.return_value
        cur.fetchone.side_effect = [{'acquired': 1}, {'n': 5}]
        cur.fetchall.side_effect = [[{'id': 1, 'queue_id': 42, 'queue_status': status, 'age': age}], []]
        cur.rowcount = cancel_wins
        grants.tick(con)
        return con, [c.args for c in cur.execute.call_args_list]

    def test_claimed_command_is_not_retried(self):
        con, calls = self.run_status('w1x234t1', 150)
        self.assertTrue(any(args == ('review', 1) for _, *rest in calls for args in rest))
        self.assertFalse(any('INSERT INTO player.web_admin_queue' in c[0] for c in calls))
        con.commit.assert_called_once()

    def test_offline_returns_to_waiting(self):
        _, calls = self.run_status('player_offline')
        self.assertTrue(any("UPDATE player.web_seban_grants SET status='waiting'" in c[0] for c in calls))

    def test_expired_pending_is_cancelled_before_retry(self):
        _, calls = self.run_status('pending', 70)
        cancel = next(i for i, c in enumerate(calls) if 'UPDATE player.web_admin_queue' in c[0])
        retry = next(i for i, c in enumerate(calls) if "UPDATE player.web_seban_grants SET status='waiting'" in c[0])
        self.assertLess(cancel, retry)

    def test_lost_cancel_race_does_not_retry(self):
        _, calls = self.run_status('pending', 70, 0)
        self.assertFalse(any("UPDATE player.web_seban_grants SET status='waiting'" in c[0] for c in calls))

    def test_full_inventory_and_success_are_terminal(self):
        for status in ('full', 'done', 'has_item', 'no_skill', 'failed'):
            _, calls = self.run_status(status)
            self.assertTrue(any(len(c)>1 and c[1] == (status, 1) for c in calls))
            self.assertFalse(any("UPDATE player.web_seban_grants SET status='waiting'" in c[0] for c in calls))

    def test_missing_queue_row_is_not_retried(self):
        _, calls = self.run_status(None)
        self.assertTrue(any(len(c)>1 and c[1] == ('review', 1) for c in calls))


class FormTests(unittest.TestCase):
    def setUp(self):
        self.app = Flask(__name__)
        self.app.secret_key = 'test-only'
        self.db = MagicMock()
        grants.install(self.app, self.db, lambda fn: fn, str)
        self.client = self.app.test_client()

    def test_bad_vnum_never_touches_database(self):
        for vnum in ('-1', '0', '1 OR 1=1', '2147483648'):
            self.assertEqual(self.client.get('/manage/items?vnum='+vnum).status_code, 400)
        self.db.assert_not_called()

    def test_post_without_csrf_cannot_enqueue(self):
        response = self.client.post('/manage/items', data={'vnum': '50051'})
        self.assertEqual(response.status_code, 400)
        cur = self.db.return_value.__enter__.return_value.cursor.return_value.__enter__.return_value
        self.assertFalse(any('INSERT' in c.args[0] for c in cur.execute.call_args_list))

    def test_candidates_only_select_playerbots(self):
        cur = MagicMock()
        cur.fetchall.return_value = []
        grants.candidates(cur, 50823, {'min_level': 10, 'min_horse': 2}, True)
        query, params = cur.execute.call_args.args
        self.assertIn("LEFT JOIN account.account", query)
        self.assertIn("LEFT(a.login,10)='playerbot_' OR p.name LIKE 'bot%%'", query)
        self.assertEqual(params, [10, 2, 50823])


class SmTests(unittest.TestCase):
    def test_amount_splits_into_largest_coupons(self):
        self.assertEqual(grants.sm_coupons(1750), [(80016, 1), (80015, 1), (80018, 1)])
        self.assertEqual(grants.sm_coupons(50), [(80017, 1)])
        self.assertEqual(grants.sm_coupons(2950), [(80016, 2), (80015, 1), (80018, 1), (80014, 2)])
        self.assertEqual(grants.sm_coupons(2800), [(80016, 2), (80015, 1), (80018, 1), (80017, 1)])

    def test_large_amount_is_split_into_full_stacks(self):
        self.assertEqual(grants.sm_coupons(450000), [(80016, 200), (80016, 200), (80016, 50)])
        total = sum(dict(grants.SM_COUPONS)[v] * n for v, n in grants.sm_coupons(grants.SM_MAX))
        self.assertEqual(total, grants.SM_MAX)

    def test_amount_not_in_coupons_is_refused(self):
        for amount in (0, -50, 30, 1025):
            with self.assertRaises(ValueError):
                grants.sm_coupons(amount)

    def test_form_without_token_never_touches_database(self):
        app = Flask(__name__)
        app.secret_key = 'test-only'
        db = MagicMock()
        grants.install(app, db, lambda fn: fn, str)
        client = app.test_client()
        self.assertEqual(client.post('/manage/sm', data={'amount': '1000'}).status_code, 400)
        with client.session_transaction() as s:
            s['seban_update_csrf'] = 'tok'
        self.assertEqual(client.post('/manage/sm', data={'amount': '1025', 'sm_csrf': 'tok'}).status_code, 400)
        db.assert_not_called()

    def test_every_bot_gets_every_coupon_line(self):
        app = Flask(__name__)
        app.secret_key = 'test-only'
        db = MagicMock()
        cur = db.return_value.__enter__.return_value.cursor.return_value.__enter__.return_value
        cur.fetchone.return_value = {'acquired': 1}
        bots = [{'id': 7, 'name': 'botA'}, {'id': 9, 'name': 'botB'}]
        with patch.object(grants, 'candidates', return_value=bots), patch.object(grants, 'init'):
            grants.install(app, db, lambda fn: fn, str)
            client = app.test_client()
            with client.session_transaction() as s:
                s['seban_update_csrf'] = 'tok'
            response = client.post('/manage/sm', data={'amount': '1500', 'sm_csrf': 'tok'})
        self.assertEqual(response.status_code, 302)
        inserts = [c.args[1] for c in cur.execute.call_args_list if 'INSERT INTO player.web_seban_grants' in c.args[0]]
        self.assertEqual(sorted((r[1], r[3], r[4]) for r in inserts), [(7, 80015, 1), (7, 80016, 1), (9, 80015, 1), (9, 80016, 1)])
        self.assertEqual(len({r[0] for r in inserts}), 2)


if __name__ == '__main__':
    unittest.main()
