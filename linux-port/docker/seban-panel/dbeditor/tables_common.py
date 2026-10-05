"""MT2009_PLUS_DB_EDITOR_V1: what the "Bonusy" (attrs.py), "Tabela
doświadczenia" (exptable.py) and "Łowienie ryb" (fishing.py) pages share:
the template helpers of templates/dbeditor/_tables_head.html."""


def register(bp):
    """Template helpers for the three pages (once per blueprint)."""
    if getattr(bp, "_dbt_helpers", False):
        return
    bp._dbt_helpers = True

    @bp.context_processor
    def _dbt_context():
        from flask import current_app
        return {"dbt_endpoints": current_app.view_functions, "dbt_num": num_text}


def num_text(value):
    """1234567 -> '1 234 567' (Polish thousands)."""
    try:
        return f"{int(value):,}".replace(",", " ")
    except (TypeError, ValueError):
        return str(value)
