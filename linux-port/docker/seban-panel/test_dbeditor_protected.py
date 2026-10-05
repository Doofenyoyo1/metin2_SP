"""MT2009_PLUS_DB_EDITOR_V1: python3 test_dbeditor_protected.py

dbeditor/protected.py against apply.sh: every "UPDATE world.item_proto ...
WHERE vnum ..." that apply.sh runs at each start must have its vnums and
columns listed, so the item page warns before an edit that the next start
undoes. Skipped where apply.sh is not next to the panel (an image)."""
import os
import re
import unittest

from dbeditor import protected

APPLY = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'mariadb', 'playerbot', 'apply.sh')


def statements(text):
    for m in re.finditer(r'UPDATE world\.item_proto\b(.*?)(?:;|"\s*\|\||"\s*$)', text, re.S | re.M):
        body = m.group(1)
        if 'WHERE' not in body:
            continue
        head, where = body.split('WHERE', 1)
        cols = set(re.findall(r'(?:SET|,)\s*`?([a-z_0-9]+)`?\s*=', head))
        vnums = set(int(v) for v in re.findall(r'vnum\s*=\s*(\d+)', where))
        for group in re.findall(r'vnum\s+IN\s*\(([\d,\s]+)\)', where):
            vnums.update(int(v) for v in group.replace('\n', ' ').split(',') if v.strip())
        for lo, hi in re.findall(r'vnum\s+BETWEEN\s+(\d+)\s+AND\s+(\d+)', where):
            vnums.update(range(int(lo), int(hi) + 1))
        types = set()
        for group in re.findall(r'type\s+IN\s*\(([\d,\s]+)\)', where):
            types.update(int(v) for v in group.split(','))
        types.update(int(v) for v in re.findall(r'\btype\s*=\s*(\d+)', where))
        yield text[:m.start()].count('\n') + 1, cols, vnums, types


@unittest.skipUnless(os.path.isfile(APPLY), 'apply.sh not next to the panel')
class ProtectedTests(unittest.TestCase):
    def test_apply_sh_covered(self):
        with open(APPLY, encoding='utf-8', errors='replace') as f:
            text = f.read()
        seen = 0
        for line, cols, vnums, types in statements(text):
            cols.discard('vnum')
            for vnum in sorted(vnums)[:2000]:
                listed = {c for r in protected.rules_for(vnum) for c in r['cols']}
                self.assertFalse(cols - listed, 'apply.sh line %d: vnum %d, columns %s' % (line, vnum, sorted(cols - listed)))
                seen += 1
            for item_type in types:
                listed = {c for r in protected.rules_for(0, item_type) for c in r['cols']}
                self.assertFalse(cols - listed, 'apply.sh line %d: type %d, columns %s' % (line, item_type, sorted(cols - listed)))
        self.assertGreater(seen, 100)

    def test_lookup(self):
        self.assertIn('applyvalue0', protected.overwritten_cols(215))
        self.assertIn('locale_name', protected.overwritten_cols(8010))
        self.assertEqual(protected.overwritten_cols(19), [])
        self.assertTrue(protected.rules_for(120000))           # dragon stone: antiflag bits, partial
        self.assertFalse(protected.is_protected(120000))
        self.assertTrue(protected.rules_for(1, item_type=13))  # fishing rods by type


if __name__ == '__main__':
    unittest.main()
