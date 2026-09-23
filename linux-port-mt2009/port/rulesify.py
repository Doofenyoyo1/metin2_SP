# -*- coding: utf-8 -*-
"""The client's terms-of-use window, from a file a human can edit.

Usage:  python rulesify.py

Reads client-locale-src/rules.pl.txt (UTF-8, LF) and writes
client-locale/locale/pl/rules.txt the way the client reads it (gamerules.py:
pairs of lines - a section title, then its text with [ENTER] between the
points - CP1250, CRLF). That file replaces the stock one in the client's
locale pack:

    python tools/eterpack.py --profile mt2009 repack <Klient>/pack/locale <out>/locale linux-port-mt2009/client-locale

The stock text is the public Mt2009 server's; ours says what this project
is, where to support it and where the Discord is. Idempotent.
"""
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, '..', 'client-locale-src', 'rules.pl.txt'))
DST = os.path.normpath(os.path.join(HERE, '..', 'client-locale', 'locale', 'pl', 'rules.txt'))


def main():
    text = io.open(SRC, encoding='utf-8', newline='').read().replace('\r\n', '\n')
    lines = [line for line in text.split('\n') if line.strip()]
    if len(lines) % 2:
        raise SystemExit('rulesify: %s must hold pairs of lines (title, text); it has %d' % (SRC, len(lines)))
    for i in range(0, len(lines), 2):
        if '[ENTER]' in lines[i]:
            raise SystemExit('rulesify: line %d looks like a text line where a title is due' % (i + 1))
    out = '\r\n'.join(lines)
    os.makedirs(os.path.dirname(DST), exist_ok=True)
    io.open(DST, 'w', encoding='cp1250', newline='').write(out)
    print('rulesify: %s (%d sections)' % (os.path.relpath(DST, os.path.dirname(HERE)), len(lines) // 2))


if __name__ == '__main__':
    main()
