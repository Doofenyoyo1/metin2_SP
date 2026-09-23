# -*- coding: utf-8 -*-
"""serverinfo.py (client-root) against stub game modules, on Python 2.7 (the
client's) and 3: no coop.cfg leaves one server, a good one adds the friend's
world with its own guild-mark name, a bad host or port is ignored.

    python tests/coop_serverinfo_test.py
"""
from __future__ import print_function
import os
import shutil
import sys
import tempfile
import types

HERE = os.path.dirname(os.path.abspath(__file__))
SERVERINFO = os.path.join(HERE, '..', 'linux-port-mt2009', 'client-root', 'serverinfo.py')


def stub(name, **attrs):
    mod = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    return mod


stub('app')
stub('localeInfo', CHANNEL_STATUS_OFFLINE='off', CHANNEL_STATUS_RECOMMENDED='ok',
     CHANNEL_STATUS_BUSY='busy', CHANNEL_STATUS_FULL='full')
stub('net', GetServerInfo=lambda: '')
stub('constInfo', TextColor=lambda text, color: '|cff%s%s|r' % (color, text))


def load(cfg_text):
    work = tempfile.mkdtemp()
    old = os.getcwd()
    try:
        if cfg_text is not None:
            with open(os.path.join(work, 'coop.cfg'), 'w') as f:
                f.write(cfg_text)
        os.chdir(work)
        ns = {'__name__': 'serverinfo'}
        with open(SERVERINFO) as f:
            code = compile(f.read(), SERVERINFO, 'exec')
        exec(code, ns)
        return ns['SERVER_LIST']
    finally:
        os.chdir(old)
        shutil.rmtree(work)


failures = []


def check(label, ok):
    print('%s %s' % ('OK  ' if ok else 'ZLE ', label))
    if not ok:
        failures.append(label)


servers = load(None)
check('bez coop.cfg jeden serwer', len(servers) == 1)
check('serwer 1 na 127.0.0.1:11000', servers[0]['main']['host'] == '127.0.0.1'
      and servers[0]['auth'][0]['port'] == [11000])

servers = load('# z zaproszenia\nname=Swiat Tieru\nhost=83.20.32.149\nauth=11000\nchannel=13000\nchannels=2\n')
check('z coop.cfg dwa serwery', len(servers) == 2)
friend = servers[1]
check('serwer 2: host z pliku', friend['main']['host'] == '83.20.32.149')
check('serwer 2: kanal 1 na 13000, kanal 2 na 13010',
      friend['channel'][0]['tcp_port'] == 13000 and friend['channel'][1]['tcp_port'] == 13010
      and friend['channel'][0]['ip'] == '83.20.32.149')
check('serwer 2: auth 11000 na hoscie', friend['auth'][0]['ip'] == '83.20.32.149'
      and friend['auth'][0]['port'] == [11000])
check('serwer 2: wlasna nazwa znaczkow gildii', friend['mark']['mark'] == '20.tga'
      and servers[0]['mark']['mark'] == '10.tga')
check('serwer 2: nazwa inna niz serwera 1', friend['main']['name'] != servers[0]['main']['name']
      and 'Swiat Tieru' in friend['main']['name'])

check('zly host (spacja, srednik) pominiety', len(load('host=1.2.3.4; rm\n')) == 1)
check('port poza zakresem pominiety', len(load('host=1.2.3.4\nauth=70000\n')) == 1)
check('port nie liczba pominiety', len(load('host=1.2.3.4\nchannel=abc\n')) == 1)
check('nazwa hosta (DDNS) przyjeta', load('host=moj-swiat.ddns.net\n')[1]['main']['host'] == 'moj-swiat.ddns.net')
check('kanalow najwyzej 4', load('host=1.2.3.4\nchannels=9\n')[1]['main']['channel_count'] == 4)

print('--- bledow: %d (python %s) ---' % (len(failures), sys.version.split()[0]))
sys.exit(1 if failures else 0)
