# -*- coding: utf-8 -*-
"""Apply the Linux port to a staged mt2009 (martysama0134 r41023) server tree.

Usage:  python linuxify.py <staged server dir>   (the one holding game/, db/, libthecore/)

The package is a FreeBSD/clang tree. Everything that stands between it and a
gcc -m32 build on Linux is listed here, as exact-string edits that keep each
file's own line endings (the tree is CRLF, and `patch` cannot be trusted on
mixed endings - see CLAUDE.md). Each edit is idempotent: applied once, or found
already applied, or the script fails naming the anchor it could not find.

The epoll fdwatch.c/fdwatch.h are copied whole from linux-port's r40250 port,
which this script also does; the two APIs are identical (checked against the
package's headers), and Martysama's own changes to the kqueue backend - the
tv_nsec fix, a sys_err on kevent failure, unsigned casts in the asserts - are
all inside what the epoll rewrite replaces.
"""
import io
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
R40250_THECORE = os.path.normpath(os.path.join(
    HERE, '..', '..', 'linux-port', 'docker', 'game', 'src', 'server', 'libthecore'))


def read(path):
    with io.open(path, 'rb') as f:
        return f.read()


def write(path, data):
    with io.open(path, 'wb') as f:
        f.write(data)


def eol_of(data):
    return b'\r\n' if b'\r\n' in data else b'\n'


def edit(path, old, new, marker=None):
    """Replace `old` with `new` once. `marker` is a string proving the edit is
    already there (defaults to `new`). Both are written with \\n and converted
    to the file's own line ending."""
    data = read(path)
    eol = eol_of(data)
    old_b = old.encode('latin-1').replace(b'\n', eol)
    new_b = new.encode('latin-1').replace(b'\n', eol)
    mark_b = (marker or new).encode('latin-1').replace(b'\n', eol)
    if mark_b in data:
        print('  already: %s' % os.path.relpath(path))
        return
    n = data.count(old_b)
    if n != 1:
        raise SystemExit('linuxify: anchor found %d times in %s:\n%s' % (n, path, old))
    write(path, data.replace(old_b, new_b, 1))
    print('  edited:  %s' % os.path.relpath(path))


def main(root):
    thecore = os.path.join(root, 'libthecore')
    game = os.path.join(root, 'game', 'src')
    db = os.path.join(root, 'db', 'src')

    # --- libthecore: the epoll backend, whole ---------------------------------
    for rel in ('src/fdwatch.c', 'include/fdwatch.h'):
        src = os.path.join(R40250_THECORE, *rel.split('/'))
        dst = os.path.join(thecore, *rel.split('/'))
        if not os.path.isfile(src):
            raise SystemExit('linuxify: %s missing (need the r40250 port beside this tree)' % src)
        if read(src) != read(dst):
            shutil.copyfile(src, dst)
            print('  copied:  %s' % os.path.relpath(dst))
        else:
            print('  already: %s' % os.path.relpath(dst))
    if b'fdwatch_sndbuf_left' not in read(os.path.join(thecore, 'src', 'fdwatch.c')):
        raise SystemExit('linuxify: the copied fdwatch.c is not the epoll one')

    # --- libthecore/include/stdafx.h ------------------------------------------
    stdafx = os.path.join(thecore, 'include', 'stdafx.h')
    # 1. Linux must not fall back to the select() backend: fdwatch.c has epoll.
    edit(stdafx,
         '#ifndef __FreeBSD__\n#define __USE_SELECT__\n',
         '/* Linux has the epoll backend in fdwatch.c; the select() fallback is for\n'
         ' * platforms with neither kqueue nor epoll. */\n'
         '#if !defined(__FreeBSD__) && !defined(__linux__)\n#define __USE_SELECT__\n')
    # 2. The headers epoll fdwatch.c and the getrandom seed need.
    edit(stdafx,
         '#ifdef __FreeBSD__\n#include <sys/event.h>\n#endif\n',
         '#ifdef __FreeBSD__\n#include <sys/event.h>\n#endif\n'
         '#if defined(__linux__)\n'
         '#include <features.h>\n'
         '#include <sys/epoll.h>\n'
         '#include <sys/ioctl.h>\n'
         '#include <linux/sockios.h>\t/* SIOCOUTQ: free space in the send buffer, as EVFILT_WRITE reports it */\n'
         '#include <sys/random.h>\t\t/* getrandom(): stands in for srandomdev() */\n'
         '#endif\n')

    # --- libthecore/include/signal.h: the header-shadowing guard -------------
    # glibc's <sys/signal.h> is one line, "#include <signal.h>", and with
    # -I../include on every compile that bracket include resolves to THIS file:
    # SIGPIPE, signal(), sigaction() silently vanish. #include_next resumes the
    # search past this directory; _SIGNAL_H is glibc's guard, so a system header
    # that got in first makes it a no-op. (FreeBSD includes the other way round.)
    edit(os.path.join(thecore, 'include', 'signal.h'),
         '#ifndef __INC_LIBTHECORE_SIGNAL_H__\n#define __INC_LIBTHECORE_SIGNAL_H__\n\n',
         '#ifndef __INC_LIBTHECORE_SIGNAL_H__\n#define __INC_LIBTHECORE_SIGNAL_H__\n\n'
         '/* Linux: glibc\'s <sys/signal.h> is "#include <signal.h>", which the\n'
         ' * -I../include on every compile resolves to this file. Resume the search\n'
         ' * past this directory so the real one is read; _SIGNAL_H is its guard. */\n'
         '#if defined(__linux__) && !defined(_SIGNAL_H)\n'
         '#include_next <signal.h>\n'
         '#endif\n\n')

    # --- libthecore/src/signal.c: POSIX signals are the same on Linux ---------
    edit(os.path.join(thecore, 'src', 'signal.c'),
         '#elif __FreeBSD__\n',
         '#elif defined(__FreeBSD__) || defined(__linux__)\n')

    # --- libthecore/src/signal.c: a crashing core says where it died --------
    # Docker Desktop's kernel pipes core dumps to /wsl-capture-crash and the
    # container's core limit is 0, so a segfault leaves nothing behind but
    # "(core dumped)" in the supervisor's log - which is what every "wywala co
    # 2 minuty" report arrives with. glibc's backtrace() needs no core file;
    # with -rdynamic at link the frames carry function names (mangled - run
    # them through c++filt). Written to crash.txt in the core's own directory
    # (m2-supervise prints and rotates it) and to stderr, which the supervisor
    # already keeps in container.log. Then the default action, so the exit
    # status the supervisor sees is still the signal's.
    edit(os.path.join(thecore, 'src', 'signal.c'),
         '#define RETSIGTYPE void\n',
         '#define RETSIGTYPE void\n'
         '\n'
         '#ifdef __linux__\n'
         '#include <execinfo.h>\n'
         '#include <fcntl.h>\n'
         '#include <stdio.h>\n'
         '#include <string.h>\n'
         '#include <unistd.h>\n'
         '\n'
         'static void crashsig(int sig)\n'
         '{\n'
         '    void* frames[64];\n'
         '    char head[160];\n'
         '    int n = backtrace(frames, 64);\n'
         '    int len = snprintf(head, sizeof(head), "=== fatal signal %d (%s), %d frames, pid %d ===\\n",\n'
         '                       sig, strsignal(sig), n, (int) getpid());\n'
         '    int fd = open("crash.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);\n'
         '    if (fd >= 0)\n'
         '    {\n'
         '        if (write(fd, head, len) < 0) {}\n'
         '        backtrace_symbols_fd(frames, n, fd);\n'
         '        close(fd);\n'
         '    }\n'
         '    if (write(2, head, len) < 0) {}\n'
         '    backtrace_symbols_fd(frames, n, 2);\n'
         '    signal(sig, SIG_DFL);\n'
         '    raise(sig);\n'
         '}\n'
         '#endif\n')
    edit(os.path.join(thecore, 'src', 'signal.c'),
         '    signal(SIGUSR1, usrsig);\n',
         '    signal(SIGUSR1, usrsig);\n'
         '#ifdef __linux__\n'
         '    signal(SIGSEGV, crashsig);\n'
         '    signal(SIGBUS, crashsig);\n'
         '    signal(SIGFPE, crashsig);\n'
         '    signal(SIGILL, crashsig);\n'
         '    signal(SIGABRT, crashsig);\n'
         '#endif\n')

    # --- libthecore/src/main.c: srandomdev() is BSD-only --------------------
    edit(os.path.join(thecore, 'src', 'main.c'),
         '#else\n    srandom(time(0) + getpid() + getuid());\n    srandomdev();\n#endif\n',
         '#elif defined(__linux__)\n'
         '    /* srandomdev(3) reseeds random() from the kernel pool; glibc has no\n'
         '     * equivalent, so do the same with getrandom(2) and keep the weak\n'
         '     * time/pid seed as the fallback where the syscall is unavailable. */\n'
         '    {\n'
         '\tunsigned int seed = 0;\n'
         '\tsrandom(time(0) + getpid() + getuid());\n'
         '\tif (getrandom(&seed, sizeof(seed), 0) == (ssize_t) sizeof(seed))\n'
         '\t    srandom(seed);\n'
         '    }\n'
         '#else\n    srandom(time(0) + getpid() + getuid());\n    srandomdev();\n#endif\n')

    # --- game/src/ClientPackageCryptInfo.cpp: xdirent.h is the Win32 shim ----
    # "#ifndef __FreeBSD__" meant Windows in a two-platform tree; on Linux the
    # shim's `typedef struct DIR DIR' collides with glibc's <dirent.h>.
    edit(os.path.join(game, 'ClientPackageCryptInfo.cpp'),
         '#ifndef __FreeBSD__\n#include "../../libthecore/include/xdirent.h"\n#endif\n',
         '#if defined(__WIN32__)\n#include "../../libthecore/include/xdirent.h"\n#endif\n')

    # --- game/src/Makefile, db/src/Makefile: link the archives as a group -----
    # The stock order puts libmysqlclient.a before -lsql, and GNU ld does not
    # rescan an archive for symbols an archive listed later turns out to need.
    for mk in (os.path.join(game, 'Makefile'), os.path.join(db, 'Makefile')):
        edit(mk,
             ' $(LIBS) -o $(MAIN_TARGET)\n',
             ' -Wl,--start-group $(LIBS) -Wl,--end-group -o $(MAIN_TARGET)\n')
        # -rdynamic puts every function into the dynamic symbol table, which
        # is what the crash handler's backtrace_symbols_fd() reads names from;
        # `strip --strip-unneeded` in the image leaves that table alone.
        edit(mk,
             ' -Wl,--start-group $(LIBS) -Wl,--end-group -o $(MAIN_TARGET)\n',
             ' -rdynamic -Wl,--start-group $(LIBS) -Wl,--end-group -o $(MAIN_TARGET)\n')

    # ======================================================================
    # game/ and db/: what the r40250 port found beyond libthecore, applied
    # where the fork has the same code.
    # ======================================================================
    db = os.path.join(root, 'db', 'src')

    # libstdc++ does not leak <memory> out of <string>/<vector> the way libc++
    # does, and both cores use std::unique_ptr without including it.
    edit(os.path.join(db, 'stdafx.h'),
         '#include "../../common/service.h"\n',
         '#include "../../common/service.h"\n\n#if defined(__linux__)\n#include <memory>\n#endif\n')
    edit(os.path.join(game, 'stdafx.h'),
         '#include <vector>\n',
         '#include <vector>\n#if defined(__linux__)\n#include <memory>\n#endif\n')

    # main.cpp: optreset is BSD getopt's; glibc has none and needs none here -
    # every valued option is a complete element, so the manual optind++ is
    # honoured (see the r40250 port's note for the two wrong translations).
    edit_all(os.path.join(game, 'main.cpp'),
             r'^([ \t]*)optreset = 1;[ \t]*$',
             r'#if !defined(__linux__)\n\1optreset = 1;\n#endif',
             marker='#if !defined(__linux__)\n\t\t\t\toptreset = 1;')

    # cmd_general.cpp: MD5 comes from libmd on Linux (-lmd is in LIBS and
    # libthecore does not build xmd5.c), so its own header is the right one.
    edit(os.path.join(game, 'cmd_general.cpp'),
         '#include "stdafx.h"\n#ifdef __FreeBSD__\n#include <md5.h>\n#else\n',
         '#include "stdafx.h"\n#if defined(__FreeBSD__) || defined(__linux__)\n#include <md5.h>\n#else\n')

    # FileMonitor_FreeBSD: a kqueue(2) client, dead on every platform (the
    # __FILEMONITOR__ switch is commented out), compiled by the wildcard.
    edit(os.path.join(game, 'FileMonitor_FreeBSD.h'),
         '#include "IFileMonitor.h"\n#include <unistd.h>\n#include <sys/event.h>\n',
         '#include "IFileMonitor.h"\n#if !defined(__linux__)\n#include <unistd.h>\n#include <sys/event.h>\n')
    edit(os.path.join(game, 'FileMonitor_FreeBSD.h'),
         '#endif //FILEMONITOR_FREEBSD_INCLUDED\n',
         '#endif // !__linux__\n#endif //FILEMONITOR_FREEBSD_INCLUDED\n')
    edit(os.path.join(game, 'FileMonitor_FreeBSD.cpp'),
         '#include "../../libthecore/include/log.h"\n',
         '#include "../../libthecore/include/log.h"\n#if !defined(__linux__)\n')
    edit(os.path.join(game, 'FileMonitor_FreeBSD.cpp'),
         '\tm_MonitoredEventLists.emplace_back(kMonitorEvent);\n}\n',
         '\tm_MonitoredEventLists.emplace_back(kMonitorEvent);\n}\n#endif // !__linux__\n')

    # config.cpp GetIPInfo: getifaddrs() on Linux also lists AF_PACKET and
    # AF_INET6 entries, and entries with a NULL ifa_addr; the cast below
    # reads sockaddr_in fields out of all of them.
    edit(os.path.join(game, 'config.cpp'),
         '\t\tstruct sockaddr_in * sai = (struct sockaddr_in *) ifap->ifa_addr;\n'
         '\n'
         '\t\tif (!ifap->ifa_netmask ||  // ignore if no netmask\n',
         '\t\tif (!ifap->ifa_addr || AF_INET != ifap->ifa_addr->sa_family)\n'
         '\t\t\tcontinue;\n'
         '\n'
         '\t\tstruct sockaddr_in * sai = (struct sockaddr_in *) ifap->ifa_addr;\n'
         '\n'
         '\t\tif (!ifap->ifa_netmask ||  // ignore if no netmask\n')

    # The container address split (the r40250 port's, reduced to what one
    # container needs): what bind() gets is not this core's identity.
    #   LISTEN_IP  what bind() gets (0.0.0.0 = every interface)
    #   BIND_IP    upstream's name for "public AND listen"; 0.0.0.0 here
    #              means LISTEN_IP only, anything else is unchanged
    #   PUBLIC_IP  this core's own address, when detection cannot be trusted
    edit(os.path.join(game, 'config.h'),
         'extern char\t\tg_szInternalIP[16];\n',
         'extern char\t\tg_szInternalIP[16];\n'
         '// Linux: the bind address alone; empty means "bind to g_szPublicIP" as upstream.\n'
         'extern char\t\tg_szBindIP[16];\n')
    edit(os.path.join(game, 'config.cpp'),
         'char\t\tg_szInternalIP[16] = "0";\n',
         'char\t\tg_szInternalIP[16] = "0";\n'
         'char\t\tg_szBindIP[16] = "";\n')
    edit(os.path.join(game, 'config.cpp'),
         '\t\tTOKEN("bind_ip")\n'
         '\t\t{\n'
         '\t\t\tstrlcpy(g_szPublicIP, value_string, sizeof(g_szPublicIP));\n'
         '\t\t}\n',
         '\t\tTOKEN("bind_ip")\n'
         '\t\t{\n'
         '\t\t\t// The wildcard is a bind address and nothing else: as a public\n'
         '\t\t\t// address it would name 0.0.0.0 as this core\'s only legal p2p\n'
         '\t\t\t// peer and publish 0.0.0.0 into the map-location table.\n'
         '\t\t\tif (!strcmp(value_string, "0.0.0.0"))\n'
         '\t\t\t{\n'
         '\t\t\t\tstrlcpy(g_szBindIP, value_string, sizeof(g_szBindIP));\n'
         '\t\t\t\tfprintf(stderr, "BIND_IP: 0.0.0.0 taken as LISTEN_IP (PUBLIC_IP left at %s)\\n", g_szPublicIP);\n'
         '\t\t\t}\n'
         '\t\t\telse\n'
         '\t\t\t\tstrlcpy(g_szPublicIP, value_string, sizeof(g_szPublicIP));\n'
         '\t\t}\n'
         '\n'
         '\t\tTOKEN("listen_ip")\n'
         '\t\t{\n'
         '\t\t\tstrlcpy(g_szBindIP, value_string, sizeof(g_szBindIP));\n'
         '\t\t\tfprintf(stderr, "LISTEN_IP: %s\\n", g_szBindIP);\n'
         '\t\t}\n'
         '\n'
         '\t\tTOKEN("public_ip")\n'
         '\t\t{\n'
         '\t\t\tstrlcpy(g_szPublicIP, value_string, sizeof(g_szPublicIP));\n'
         '\t\t\tfprintf(stderr, "PUBLIC_IP: %s (configured)\\n", g_szPublicIP);\n'
         '\t\t}\n')
    edit(os.path.join(game, 'main.cpp'),
         '\tif ((tcp_socket = socket_tcp_bind(g_szPublicIP, mother_port)) == INVALID_SOCKET)\n',
         '\tconst char * c_szBindIP = g_szBindIP[0] ? g_szBindIP : g_szPublicIP;\n'
         '\tif ((tcp_socket = socket_tcp_bind(c_szBindIP, mother_port)) == INVALID_SOCKET)\n')
    edit(os.path.join(game, 'main.cpp'),
         '\tif ((udp_socket = socket_udp_bind(g_szPublicIP, mother_port)) == INVALID_SOCKET)\n',
         '\tif ((udp_socket = socket_udp_bind(c_szBindIP, mother_port)) == INVALID_SOCKET)\n')
    edit(os.path.join(game, 'main.cpp'),
         '\tif ((p2p_socket = socket_tcp_bind(g_szPublicIP, p2p_port)) == INVALID_SOCKET)\n',
         '\tif ((p2p_socket = socket_tcp_bind(c_szBindIP, p2p_port)) == INVALID_SOCKET)\n')

    print('linuxify: done')


def edit_all(path, pattern, repl, marker):
    """Regex replacement of every match, line by line; `marker` proves it."""
    import re
    data = read(path)
    eol = eol_of(data)
    mark_b = marker.encode('latin-1').replace(b'\n', eol)
    if mark_b in data:
        print('  already: %s' % os.path.relpath(path))
        return
    text = data.decode('latin-1').replace(eol.decode('latin-1'), '\n')
    new, n = re.subn(pattern, repl, text, flags=re.M)
    if n == 0:
        raise SystemExit('linuxify: pattern found 0 times in %s: %s' % (path, pattern))
    write(path, new.replace('\n', eol.decode('latin-1')).encode('latin-1'))
    print('  edited:  %s (%d places)' % (os.path.relpath(path), n))


if __name__ == '__main__':
    if len(sys.argv) != 2 or not os.path.isdir(os.path.join(sys.argv[1], 'libthecore')):
        raise SystemExit(__doc__)
    main(os.path.abspath(sys.argv[1]))
