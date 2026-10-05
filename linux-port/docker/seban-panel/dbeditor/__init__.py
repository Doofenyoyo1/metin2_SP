"""MT2009_PLUS_DB_EDITOR_V1: "Edytor bazy danych" - the database editor of the
Seban panel, for an operator who is not a programmer: items, skills, monster
drops (with their groups) and chests, plus the client data the game's clients
download after a change.

Each part is a module of this package with an install(bp, ctx) function that
adds its routes to the one blueprint (url prefix /db). ctx carries the panel's
helpers (db, rows, one, login_required, game_text, ...), so no module imports
app.py. The hub page lists the parts that are installed.
"""
from flask import Blueprint, render_template

bp = Blueprint("dbeditor", __name__, url_prefix="/db")

# (endpoint, icon, title, description) - filled by the parts' install().
SECTIONS = []
# MT2009_PLUS_DBDATA_STAMP_V1: callables giving a notice for the hub's top
# (or None) - clientdata.py's "a new client came out, download the zip again".
HUB_NOTICES = []


def add_section(endpoint, icon, title, description):
    SECTIONS.append((endpoint, icon, title, description))


def install(app, ctx):
    login_required = ctx["login_required"]

    @bp.route("/")
    @login_required
    def index():
        notices = []
        for notice in HUB_NOTICES:
            try:
                text = notice()
            except Exception:  # never break the hub over a notice
                text = None
            if text:
                notices.append(text)
        return render_template("dbeditor/index.html", sections=SECTIONS, notices=notices)

    # The parts, in the order the hub shows them. A part that is missing (an
    # older image) is skipped.
    for name in ("items", "skills", "drops", "chests", "mobs", "spawns", "regen", "shops", "refine", "attrs", "exptable", "fishing", "config", "clientdata"):
        try:
            module = __import__("dbeditor." + name, fromlist=["install"])
        except ImportError:
            continue
        module.install(bp, ctx)
    __import__("dbeditor.raredrop", fromlist=["install"]).install(bp, ctx)  # MT2009_PLUS_RARE_DROP_SWITCHES_V1 (on the hub after "Drop")
    app.register_blueprint(bp)
