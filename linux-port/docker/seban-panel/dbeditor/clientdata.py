"""MT2009_PLUS_DB_EDITOR_V1: "Zastosuj" - the editor's pending changes and
one button that restarts the game cores (ctx["queue_restart"]: the same
queued restart as the chests' "Zastosuj teraz" and "Gra i serwer") - and
the client files as a zip to download.

Pending changes come from the editor's parts themselves:
  * items and skills: common_items.pending_changes() (the history rows not
    yet applied, net per field); common_items.mark_applied() once the
    restart is queued;
  * drop files and chests: dropfiles.pending_changes(<spool>) - the files
    the game does not run yet;
  * map spawns: regen.pending_changes(<spool>) - the maps' regen.txt the
    game does not run yet (MT2009_PLUS_DB_EDITOR_V1, m2-regen);
  * monsters (mobs.py): the same history as items/skills (tbl world.mob_proto);
  * respawn files (spawns.py): spawnfiles.pending_changes(<spool>).

Client files (m2clientpack.dbdata): nothing is downloaded automatically - a
player's client may list several servers (localhost, COOP 1, COOP 2). The
operator clicks "Pobierz aktualne pliki klienta" and gets a zip with
pack/dbdata.index + pack/dbdata.data (the release's dbdata pack with every
item/skill field the editor ever changed, common_items.net_changes(
include_applied=True)) and CZYTAJ_MNIE.txt; he unpacks it into his client or
sends it to a friend who plays on this server. "Pobierz oryginalne pliki"
gives the release's own dbdata pack back.

MT2009_PLUS_DBDATA_STAMP_V1: the stamp of what the client shows - the
client base's version alone with nothing client-visible edited, else
"<version>-<12 hex>" of the changed client files (m2clientpack.dbdata.stamp).
  * the zip carries it as dbdata_stamp.txt at the client root (with the
    sizes of its pack files); the original files carry the version alone;
  * the panel keeps the current one in <spool>/dbdata_stamp.txt (SERVER_STAMP),
    the file every game core reads at a person's login and sends on as
    "DbDataStamp <stamp>" (playerbot_dbdata_stamp.h); the client
    (dbdatastamp.py) tells a player whose dbdata_stamp.txt differs to
    download the zip again. Written at "Zastosuj", after every saved change
    to a client-visible table (common_items.CHANGE_LISTENERS), at every
    download of the current zip, and when a page finds it missing or made
    for another client base;
  * the last download of the current zip is kept (<spool>/dbeditor/
    clientdata-download.json): the hub and "Zastosuj" say when a newer
    client base came with the panel since (new_client_banner).
"""
import json
import os
import threading
import time
from pathlib import Path

from flask import Response, flash, redirect, render_template, request, url_for

def _read_json(path, default):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return default


def _write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".tmp")
    tmp.write_text(json.dumps(data, ensure_ascii=False, indent=1, default=str), encoding="utf-8")
    os.replace(tmp, path)


# MT2009_PLUS_DBDATA_STAMP_V1 (see the docstring)
SERVER_STAMP = "dbdata_stamp.txt"
DOWNLOAD_LOG = "clientdata-download.json"
# the tables whose edits reach the client files (m2clientpack/dbsource.py)
CLIENT_TABLES = {"world.item_proto", "player.item_proto", "world.skill_proto", "player.skill_proto",
                 "world.item_extra_apply"}


def new_client_banner(last_download, latest_version):
    """The hub's and "Zastosuj"'s notice when the client base is newer than
    the one of the last current zip downloaded (None otherwise, also before
    the first download)."""
    downloaded = (last_download or {}).get("base")
    if not downloaded or not latest_version or downloaded == latest_version:
        return None
    return ("Wyszła nowa wersja klienta (%s) – pobierz ponownie pliki klienta, bo stary zip nie zawiera "
            "nowych przedmiotów (ostatni zip był dla klienta %s)." % (latest_version, downloaded))


def read_server_stamp(spool):
    """The stamp in <spool>/dbdata_stamp.txt, or None."""
    import m2clientpack.dbdata as dbdata
    try:
        return dbdata.read_stamp_text((Path(spool) / SERVER_STAMP).read_bytes())
    except OSError:
        return None


def write_server_stamp(spool, value):
    """<spool>/dbdata_stamp.txt for the game cores (read at every login;
    group-readable like the spool's other files). Unchanged: not rewritten."""
    import m2clientpack.dbdata as dbdata
    spool = Path(spool)
    if read_server_stamp(spool) == value:
        return False
    spool.mkdir(parents=True, exist_ok=True)
    temporary = spool / (SERVER_STAMP + ".new")
    temporary.write_bytes(dbdata.stamp_text(value))
    os.chmod(temporary, 0o664)
    os.replace(temporary, spool / SERVER_STAMP)
    return True


# MT2009_PLUS_DBDATA_STAMP_V1 (popup): when the panel last restarted the game
# cores - app.py's queue_rate_restart (every restart button, the editor's
# "Zastosuj" too) and the world reset write <spool>/panel-restart.time. After
# one, the editor's pages ask in a popup to download the client files again,
# until the zip is downloaded (here, or the cookie the page sets) or the
# popup is closed with "Rozumiem" (the cookie holds the restart it closed).
RESTART_FILE = "panel-restart.time"
POPUP_COOKIE = "dbe_zip_seen"
POPUP_TEXT = ("UWAGA! Aby zmiany z edytora bazy danych były widoczne w Twoim kliencie gry, musisz pobrać ten plik "
              "ZIP i rozpakować go do folderu z klientem (zastąp pliki). Bez tego w grze zobaczysz stare nazwy, "
              "bonusy i opisy.")


def last_restart(spool, last_apply=None):
    """Epoch seconds of the panel's last core restart (0: none known)."""
    times = [0]
    try:
        times.append(int((Path(spool) / RESTART_FILE).read_text(encoding="ascii").split()[0]))
    except (OSError, ValueError, IndexError):
        pass
    try:
        times.append(int((last_apply or {}).get("time") or 0))
    except (TypeError, ValueError):
        pass
    return max(times)


def zip_popup_due(restart_time, downloaded_time, dismissed_time, has_edits, base_changed):
    """The popup shows after a restart newer than the last download of the
    current zip and the last "Rozumiem" - only while the server has
    client-visible edits or the client base changed since the last zip."""
    if not (has_edits or base_changed) or not restart_time:
        return False
    return restart_time > max(int(downloaded_time or 0), int(dismissed_time or 0))


# The stamp after a saved change is worked out in the background (patching
# item_proto takes a second or more): one worker at a time, a change during
# its run makes it go once more.
_STAMP_JOB = {"thread": None, "dirty": False}
_STAMP_LOCK = threading.Lock()


def _stamp_soon(refresh):
    def worker():
        while True:
            with _STAMP_LOCK:
                if not _STAMP_JOB["dirty"]:
                    _STAMP_JOB["thread"] = None
                    return
                _STAMP_JOB["dirty"] = False
            try:
                refresh()
            except Exception:
                pass

    with _STAMP_LOCK:
        _STAMP_JOB["dirty"] = True
        if _STAMP_JOB["thread"] is not None:
            return
        thread = threading.Thread(target=worker, name="dbdata-stamp", daemon=True)
        _STAMP_JOB["thread"] = thread
    thread.start()


def wait_stamp(timeout=60):
    """Until the background stamp is written (tests)."""
    thread = _STAMP_JOB["thread"]
    if thread is not None:
        thread.join(timeout)


def install(bp, ctx):
    from dbeditor import add_section, common_items, dropfiles
    import m2clientpack.dbsource as dbsource
    import m2clientpack.dbdata as dbdata

    common_items.init(ctx)
    login_required = ctx["login_required"]
    rows = ctx["rows"]
    spool = Path(os.environ.get("DBEDITOR_SPOOL_ROOT") or ctx.get("spool") or "/opt/m2spool")
    last_apply = spool / "dbeditor" / "last-apply.json"
    build_log = spool / "dbeditor" / "clientdata-build.json"
    download_log = spool / "dbeditor" / DOWNLOAD_LOG

    def query(sql, params=()):
        # looked up per call like items.py does, so tests can swap the database
        return common_items.ctx().get("rows", rows)(sql, params)

    def pending():
        items = common_items.pending_changes()
        try:
            files = dropfiles.pending_changes(spool)
        except Exception:  # never break the page over a spool folder
            files = []
        # MT2009_PLUS_DB_EDITOR_V1 (regen): the maps' spawn files (regen.txt)
        # the game does not run yet - dbeditor/regen.py, applied by m2-regen.
        try:
            from dbeditor import regen
            files = files + regen.pending_changes(spool)
        except Exception:
            pass
        try:  # MT2009_PLUS_DB_EDITOR_V1: the respawn files (spawns.py / spawnfiles.py)
            from dbeditor import spawnfiles
            files = files + spawnfiles.pending_changes(spool)
        except Exception:
            pass
        return items, files

    def build_zip(original=False):
        """(file name, zip bytes, summary) of the client files."""
        # The zip's name and CZYTAJ_MNIE say which server it is for: the panel's
        # name, plus the address when it is not this machine's own.
        host = request.host.split(":")[0]
        try:
            server = (ctx.get("panel_name") or (lambda: ""))() or "MT2009 PLUS"
        except Exception:
            server = "MT2009 PLUS"
        if host and host not in ("127.0.0.1", "localhost", "::1", "[::1]"):
            server = "%s (%s)" % (server, host)
        if original:
            base = dbdata.latest_base()
            index, data = base.original()
            name, blob = dbdata.make_zip(base, index, data, server, [], original=True)
            return name, blob, {}
        started = time.time()
        try:
            base, index, data, changed, summary = dbsource.build_dbdata(
                query, common_items.net_changes(include_applied=True))
        except Exception as exc:
            _write_json(build_log, {"ok": False, "time": int(started), "error": "%s: %s" % (type(exc).__name__, exc)})
            raise
        summary["changed_files"] = changed
        _write_json(build_log, {"ok": True, "time": int(started), "when": time.strftime("%d.%m.%Y %H:%M"),
                                "seconds": round(time.time() - started, 1), "client": base.version, "summary": summary})
        name, blob = dbdata.make_zip(base, index, data, server, changed, stamp_value=summary.get("stamp"))
        # MT2009_PLUS_DBDATA_STAMP_V1: the game asks for what this zip carries,
        # and the hub remembers which client base the players' zip is for.
        save_stamp(summary.get("stamp"))
        _write_json(download_log, {"base": base.version, "stamp": summary.get("stamp"), "time": int(time.time()),
                                   "when": time.strftime("%d.%m.%Y %H:%M")})
        return name, blob, summary

    def save_stamp(value):
        try:
            if value:
                write_server_stamp(spool, value)
        except OSError:
            pass  # a spool the panel cannot write: the game simply sends nothing new

    def refresh_stamp():
        """MT2009_PLUS_DBDATA_STAMP_V1: the current stamp into the spool."""
        try:
            _base, value = dbsource.current_stamp(query, common_items.net_changes(include_applied=True))
        except Exception:
            return None
        save_stamp(value)
        return value

    def ensure_stamp():
        """A spool without the stamp, or with one of another client base (the
        panel came with a new one): made now."""
        current = read_server_stamp(spool)
        latest = base_version()
        if latest and (not current or current.split("-", 1)[0] != latest):
            refresh_stamp()

    def on_change(tables):
        if tables & CLIENT_TABLES:
            _stamp_soon(refresh_stamp)

    common_items.CHANGE_LISTENERS["clientdata"] = on_change

    # The game reads the stamp at a person's login, so it must be in the spool
    # from the panel's start, not from the first visit of the hub: the current
    # one is written in the background, again while the database is not up yet.
    def startup_stamp():
        for _attempt in range(40):
            if refresh_stamp():
                return
            time.sleep(15)

    app = ctx.get("app")
    if ctx.get("queue_restart") is not None and not (app is not None and app.testing):
        threading.Thread(target=startup_stamp, name="dbdata-stamp-start", daemon=True).start()

    def banner():
        return new_client_banner(_read_json(download_log, {}), base_version())

    def popup_state():
        """MT2009_PLUS_DBDATA_STAMP_V1 (popup): None, or what the popup needs."""
        restart = last_restart(spool, _read_json(last_apply, {}))
        if not restart:
            return None
        try:
            dismissed = int(request.cookies.get(POPUP_COOKIE) or 0)
        except ValueError:
            dismissed = 0
        downloaded = _read_json(download_log, {}).get("time") or 0
        stamp_now = read_server_stamp(spool) or ""
        if not zip_popup_due(restart, downloaded, dismissed, "-" in stamp_now, banner() is not None):
            return None
        return {"restart": restart, "text": POPUP_TEXT, "cookie": POPUP_COOKIE,
                "zip": url_for("dbeditor.clientdata_zip")}

    @bp.context_processor
    def zip_popup():
        try:
            return {"dbe_zip_popup": popup_state()}
        except Exception:  # never break an editor page over the popup
            return {"dbe_zip_popup": None}

    try:
        from dbeditor import HUB_NOTICES

        def hub_notice():
            ensure_stamp()
            return banner()

        HUB_NOTICES.append(hub_notice)
    except ImportError:
        pass

    def base_version():
        try:
            return dbdata.latest_base().version
        except Exception:
            return None

    def restart_state():
        try:
            return ctx["restart_progress"]() if ctx.get("restart_progress") else {}
        except Exception:
            return {}

    def table_kind(tbl):
        # MT2009_PLUS_DB_EDITOR_V1: what a history row is (Przedmiot, Umiejętność,
        # Bonus 1-5, Tabela doświadczenia ...) - common_items.register_table's title.
        meta = common_items.TABLES.get(tbl)
        title = (meta or {}).get("title") or ("umiejętność" if "skill" in tbl else "przedmiot")
        return title[:1].lower() + title[1:]

    @bp.route("/apply", methods=["GET"])
    @login_required
    def apply_page():
        items, files = pending()
        ensure_stamp()
        return render_template(
            "dbeditor/clientdata.html", items=items, files=files, last=_read_json(last_apply, {}),
            build=_read_json(build_log, {}), csrf=common_items.csrf_token(), client_version=base_version(),
            restart=restart_state(), column_label=common_items.column_label,
            format_value=common_items.format_value, table_kind=table_kind, new_client=banner(),
            server_stamp=read_server_stamp(spool))

    @bp.route("/apply", methods=["POST"])
    @login_required
    def apply_post():
        if not common_items.check_csrf():
            return redirect(url_for("dbeditor.apply_page"))
        if request.form.get("confirmation", "").strip().upper() != "RESTART":
            flash("Restartu nie zlecono: aby potwierdzić restart rdzeni, wpisz RESTART.", "error")
            return redirect(url_for("dbeditor.apply_page"))
        queue = ctx.get("queue_restart")
        if queue is None:
            flash("Ten panel nie ma funkcji restartu gry.", "error")
            return redirect(url_for("dbeditor.apply_page"))
        items, files = pending()
        try:
            queue()
        except FileExistsError:
            flash("Poprzedni restart jeszcze trwa - poczekaj na jego koniec i kliknij Zastosuj ponownie.", "error")
            return redirect(url_for("dbeditor.apply_page"))
        except (OSError, RuntimeError) as exc:
            flash("Nie udało się zlecić restartu: %s" % exc, "error")
            return redirect(url_for("dbeditor.apply_page"))
        try:
            marked = common_items.mark_applied()
        except Exception as exc:
            marked = 0
            flash("Restart zlecony, ale nie oznaczono zmian jako zastosowanych: %s" % exc, "warning")
        _write_json(last_apply, {"time": int(time.time()), "when": time.strftime("%d.%m.%Y %H:%M"),
                                 "changes": len(items) + len(files), "history_rows": marked})
        _stamp_soon(refresh_stamp)  # MT2009_PLUS_DBDATA_STAMP_V1
        flash("Restart rdzeni zlecony - gra wczyta zmiany (%d pól, %d plików) po starcie; gracze online zostaną "
              "rozłączeni na ok. minutę. Nowe nazwy i bonusy w kliencie: pobierz aktualne pliki klienta (zip) niżej."
              % (len(items), len(files)), "success")
        return redirect(url_for("dbeditor.apply_page"))

    @bp.route("/clientdata.zip")
    @login_required
    def clientdata_zip():
        original = request.args.get("oryginal") == "1"
        try:
            name, blob, _summary = build_zip(original)
        except Exception as exc:
            flash("Nie udało się zbudować plików klienta: %s" % exc, "error")
            return redirect(url_for("dbeditor.apply_page"))
        response = Response(blob, mimetype="application/zip", headers={
            "Content-Disposition": 'attachment; filename="%s"' % name, "Cache-Control": "no-store"})
        if not original:  # MT2009_PLUS_DBDATA_STAMP_V1 (popup): this browser has the zip of the last restart
            response.set_cookie(POPUP_COOKIE, str(last_restart(spool, _read_json(last_apply, {}))),
                                max_age=400 * 86400, samesite="Lax", path="/")
        return response

    add_section("dbeditor.apply_page", "✅", "Zastosuj",
                "Oczekujące zmiany, restart gry i pliki klienta (zip) dla graczy.")
