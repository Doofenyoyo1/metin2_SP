# -*- coding: utf-8 -*-
"""Every window script of the mt2009 client, loaded the way the client loads it.

PythonScriptLoader.LoadScriptFile (ui.py) runs a uiscript/*.py file inside
three `except` branches, and every one of them ends in exception.Abort, which
is app.Abort: the client closes with no dialog. So one missing localeInfo or
uiScriptLocale key, one engine constant the exe does not export, one flag the
script asks the app module for and does not find, is a game that shuts down the
moment a player opens that window - and in one language only, if the key is
missing only there. The only trace is "Failed to load script file" in the
client folder's syserr.txt.

This loads every uiscript/*.py of a client root, in every language of the
locale pack, at 800x600 and 1920x1080, with the engine modules stubbed and the
app module's flags read out of metin2client.exe itself (PyModule_AddIntConstant
is a `push <value>; push "<NAME>"` in the exe's code), so a branch a script
takes on a flag is the branch the client takes. With ENABLE_LOCALE_COMMON the
client reads its window scripts from UIScript/ only; the locale pack's
locale/<lang>/ui/*.py are not loaded and not tested.

A script whose only loader is imported behind a flag the exe has at 0 is
skipped (GATED below), and only then: the day the flag goes to 1, it is tested.

Usage (Python 2.7, the client's own):
    python tests/client_uiscript_load_test.py <root> <locale dir> <metin2client.exe>

<root> and <locale dir> are extractions of pack/root and pack/locale:
    python3 tools/eterpack.py --profile mt2009 extract <client>/pack/root <root>
    python3 tools/eterpack.py --profile mt2009 extract <client>/pack/locale <locale dir>
"""
import os
import re
import struct
import sys
import types

# The window scripts nothing loads while a flag is 0, and the flag.
GATED = {
    'acce_absorbwindow.py': 'ENABLE_ACCE_COSTUME_SYSTEM',     # uiacce.py, imported under it
    'acce_combinewindow.py': 'ENABLE_ACCE_COSTUME_SYSTEM',
    'characterdetailswindow.py': 'ENABLE_CONQUEROR_UI',      # uicharacternew.py, interfacemodule
}

FLAG_NAME = re.compile(r'^(?:ENABLE_|DISABLE_|WJ_|__[A-Z0-9_]+__$)')


def read_exe_flags(path):
    """{name: value} of every flag constant the exe's app module registers."""
    data = open(path, 'rb').read()
    pe = data.find(b'PE\0\0')
    count = struct.unpack_from('<H', data, pe + 6)[0]
    optional = pe + 24
    base = struct.unpack_from('<I', data, optional + 28)[0]
    table = optional + struct.unpack_from('<H', data, pe + 20)[0]
    sections = []
    for i in range(count):
        at = table + 40 * i
        name = data[at:at + 8].rstrip(b'\0')
        vsize, va, rsize, raw = struct.unpack_from('<IIII', data, at + 8)
        sections.append((name, va, raw, rsize))

    def file_to_va(offset):
        for _name, va, raw, rsize in sections:
            if raw <= offset < raw + rsize:
                return base + va + offset - raw
        return None

    text = [s for s in sections if s[0] == b'.text'][0]
    code = data[text[2]:text[2] + text[3]]
    flags = {}
    # Lookarounds, not the NULs themselves: two names one NUL apart are both names.
    for m in re.finditer(br'(?<=\0)([A-Z_][A-Z0-9_]{2,60})(?=\0)', data):
        name = m.group(1).decode('ascii')
        if not FLAG_NAME.match(name):
            continue
        va = file_to_va(m.start(1))
        if va is None:
            continue
        values = set()
        for push in re.finditer(re.escape(b'\x68' + struct.pack('<I', va)), code):
            at = push.start()
            if code[at - 2:at - 1] == b'\x6a':                  # push imm8
                values.add(ord(code[at - 1:at]))
            elif code[at - 5:at - 4] == b'\x68':                # push imm32
                values.add(struct.unpack_from('<I', code, at - 4)[0])
        if len(values) == 1:
            flags[name] = values.pop()
    return flags


class Anything(object):
    """What a stubbed engine call returns: usable wherever a script puts it."""
    def __call__(self, *args, **kwargs):
        return Anything()

    def __getattr__(self, name):
        return Anything()

    def __nonzero__(self):
        return False

    def __int__(self):
        return 0

    def __add__(self, other):
        return other

    __radd__ = __add__

    def __str__(self):
        return ''


class EngineModule(types.ModuleType):
    """An engine module: the exe's flags as the exe has them, and a flag the exe
    does not register raises, as it does in the client."""
    flags = {}

    def __getattr__(self, name):
        if name in self.flags:
            return self.flags[name]
        if name.startswith('__') or FLAG_NAME.match(name):
            raise AttributeError(name)
        return Anything()


ENGINE_MODULES = (
    'app', 'pack', 'systemSetting', 'dbg', 'flamewindPath', 'item', 'player', 'chr', 'chrmgr',
    'net', 'grp', 'grpText', 'grpImage', 'wndMgr', 'snd', 'ime', 'chat', 'skill', 'nonplayer',
    'shop', 'quest', 'guild', 'messenger', 'safebox', 'textTail', 'miniMap', 'background',
    'effect', 'fly', 'event', 'exchange', 'ikashop', 'renderTarget', 'cube', 'acce',
    'ServerStateChecker', 'dragonSoul', 'systemInfo',
)


class RootImporter(object):
    """system.py's __hybrid_import: a root module by any case of its name."""

    def __init__(self, root):
        self.root = root
        self.files = dict((f[:-3].lower(), f) for f in os.listdir(root) if f.endswith('.py'))
        self.opener = None
        self.loaded = []

    def find_module(self, name, path=None):
        if '.' in name or name.lower() not in self.files:
            return None
        return self

    def load_module(self, name):
        if name in sys.modules:
            return sys.modules[name]
        module = types.ModuleType(name)
        module.__file__ = os.path.join(self.root, self.files[name.lower()])
        module.open = self.opener
        sys.modules[name] = module
        self.loaded.append(name)
        source = open(module.__file__, 'rb').read().replace(b'\r\n', b'\n')
        try:
            exec compile(source, self.files[name.lower()], 'exec') in module.__dict__
        except BaseException:
            sys.modules.pop(name, None)
            raise
        return module

    def start_language(self, locale_dir, language):
        for name in self.loaded:
            sys.modules.pop(name, None)
        self.loaded = []
        for name in ENGINE_MODULES:
            sys.modules[name] = EngineModule(name)
        app = sys.modules['app']
        app.GetLocalePath = lambda: 'locale/pl'
        app.GetDefaultCodePage = lambda: 1250
        app.GetLocaleServiceName = lambda: 'EUROPE'
        sys.modules['pack'].Exist = lambda path: os.path.exists(os.path.join(locale_dir, path))
        sys.modules['systemSetting'].GetLanguage = lambda: language
        sys.modules['dbg'].LogBox = lambda *args: sys.stdout.write('  LogBox %r\n' % (args,))
        sys.modules['dbg'].TraceError = lambda *args: None
        self.opener = lambda path, mode='r': open(os.path.join(locale_dir, path), 'rU')
        __import__('localeInfo')
        __import__('uiScriptLocale')
        # What the game's start does before any window is made (gameFont).
        __import__('gameFont').SetCurrentFont('Tahoma', 12)


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    root, locale_dir, exe = sys.argv[1:4]
    EngineModule.flags = read_exe_flags(exe)
    for name in ('ENABLE_LOCALE_COMMON',) + tuple(sorted(set(GATED.values()))):
        if name not in EngineModule.flags:
            raise SystemExit('client_uiscript_load_test: no %s in %s' % (name, exe))
    if not EngineModule.flags['ENABLE_LOCALE_COMMON']:
        raise SystemExit('client_uiscript_load_test: this exe reads locale/<lang>/ui, which this test does not')

    importer = RootImporter(root)
    sys.meta_path.insert(0, importer)
    languages = sorted(d for d in os.listdir(os.path.join(locale_dir, 'locale'))
                       if d != 'common' and os.path.isdir(os.path.join(locale_dir, 'locale', d)))
    scripts = sorted(f for f in os.listdir(os.path.join(root, 'uiscript')) if f.endswith('.py'))
    skipped = sorted(f for f in scripts if f in GATED and not EngineModule.flags[GATED[f]])
    failures = []
    real_stdout = sys.stdout
    for language in languages:
        # The loaders print the lines of a locale file they cannot read.
        sys.stdout = open(os.devnull, 'w')
        try:
            importer.start_language(locale_dir, language)
        finally:
            sys.stdout = real_stdout
        for name in scripts:
            if name in skipped:
                continue
            source = open(os.path.join(root, 'uiscript', name), 'rb').read().replace(b'\r\n', b'\n')
            for (width, height) in ((800, 600), (1920, 1080)):
                ns = {'SCREEN_WIDTH': width, 'SCREEN_HEIGHT': height, 'PLAYER_NAME_MAX_LEN': 24,
                      'DRAGON_SOUL_EQUIPMENT_SLOT_START': 0, 'LOCALE_PATH': 'locale/pl',
                      '__name__': '__main__'}
                try:
                    exec compile(source, 'uiscript/' + name, 'exec') in ns
                    if 'window' not in ns:
                        raise KeyError('window')
                except Exception, exc:                           # noqa: E722
                    failures.append('%s uiscript/%s %dx%d: %s: %s'
                                    % (language, name, width, height, exc.__class__.__name__, exc))
        print('  %s: %d window scripts' % (language, len(scripts) - len(skipped)))

    if skipped:
        print('  not loaded while the flag is 0: %s' % ', '.join(skipped))
    if failures:
        print('\nWould close the client (exception.Abort), %d:' % len(failures))
        for line in failures[:40]:
            print('  ' + line)
        sys.exit(1)
    print('client_uiscript_load_test: PASS')


if __name__ == '__main__':
    main()
