"""MT2009_PLUS_DB_EDITOR_V1: "Szkatułki" in the database editor - every group
of special_item_group.txt (chests, boxes, the s<vnum> draws of monster drops)
with names and icons, what it holds and how likely each line is; whole groups
edited at once (contents, chances, ×N for the chosen lines).

Same files and backups as the old "Szkatułki" page (/chests): the changed
groups go whole to chests/special_item_group.custom.txt in the spool, the
game's m2-chests puts them into the live file before the cores boot
(dropfiles.py explains the whole path).
"""
from flask import abort, flash, redirect, render_template, request, url_for

from . import dropfiles as df

FEATURED = (50011, 83023, 83024, 83025, 83026, 83027)
FACTORS = ("0.5", "2", "3", "5")
PREVIEW = 6
GIFTBOX = 23


def install(bp, ctx):
    import dbeditor
    df.register(bp)

    rows, game_text, login_required = ctx["rows"], ctx["game_text"], ctx["login_required"]

    def names_for(vnums):
        return df.item_names(rows, game_text, vnums)

    def state():
        spool = df.spool_dir()
        base, source = df.read_chest_base(spool)
        custom = df.read_chest_custom(spool)
        groups = dict(base)
        groups.update(custom)
        return {"spool": spool, "base": base, "custom": custom, "groups": groups, "source": source}

    def entry_info(entry, names, groups):
        item = str(entry["item"]).lower()
        if item.isdigit():
            known = names.get(int(item))
            return {"label": known["name"] if known else "NIEZNANY PRZEDMIOT", "icon": df.icon_url(item),
                    "bad": known is None, "nested": None}
        if item in df.CHEST_TOKENS:
            return {"label": df.CHEST_TOKENS[item], "icon": None, "bad": False, "nested": None}
        if item.startswith("s") and item[1:].isdigit():
            vnum = int(item[1:])
            inner = names.get(vnum)
            nested = groups.get(vnum)
            label = "Losowanie z grupy " + item[1:] + (f" ({inner['name']})" if inner else f" ({nested['name']})" if nested else "")
            return {"label": label + ("" if nested else " – BRAK TAKIEJ GRUPY"), "icon": df.icon_url(vnum),
                    "bad": nested is None, "nested": vnum}
        return {"label": "?", "icon": None, "bad": True, "nested": None}

    @bp.route("/chests")
    @login_required
    def chests_index():
        st = state()
        query = request.args.get("q", "").strip()
        groups = st["groups"]
        hits, needle, item_hits = set(), "", set()
        if query:
            needle = query.casefold()
            item_hits = {str(i["vnum"]) for i in df.search_items(rows, game_text, query, 200)} if len(query) >= 2 or query.isdigit() else set()
        names = names_for(list(groups.keys()) + [e["item"] for grp in groups.values()
                                                 for e in (grp["items"] if query else grp["items"][:PREVIEW])])
        listed = []
        for vnum, group in groups.items():
            known = names.get(vnum)
            title = known["name"] if known else ""
            if query:
                own = query == str(vnum) or (not query.isdigit() and (needle in title.casefold() or needle in group["name"].casefold()))
                contains = [e for e in group["items"] if str(e["item"]).lower().lstrip("s") in item_hits]
                if not own and not contains:
                    continue
                if contains:
                    hits.add(vnum)
            listed.append({"vnum": vnum, "name": title or group["name"], "group_name": group["name"],
                           "icon": df.icon_url(vnum) if known else None, "is_box": bool(known and known["type"] == GIFTBOX),
                           "is_item": bool(known), "type": group["type"], "lines": len(group["items"]),
                           "custom": vnum in st["custom"], "new": vnum not in st["base"], "featured": vnum in FEATURED,
                           "contains": vnum in hits,
                           "preview": [dict(e, **entry_info(e, names, groups), pct=df.chest_chance(group, e))
                                       for e in group["items"][:PREVIEW]]})
        listed.sort(key=lambda c: (not c["featured"], not c["custom"], not c["is_box"], not c["is_item"], c["vnum"]))
        return render_template("dbeditor/chests.html", chests=listed, query=query, st=st,
                               status=df.live_state(st["spool"], "chest"), types=df.CHEST_TYPES,
                               backups=df.list_backups(st["spool"], ("chest",), 40), csrf=df.csrf_token(),
                               pending=df.pending_changes(st["spool"]))

    @bp.route("/chests/<int:vnum>")
    @login_required
    def chests_edit(vnum):
        st = state()
        group = st["groups"].get(vnum)
        if group is None:
            flash(f"Nie ma szkatułki o VNUM {vnum}. Utwórz ją przyciskiem „Nowa szkatułka”.", "error")
            return redirect(url_for("dbeditor.chests_index"))
        names = names_for([vnum] + [e["item"] for e in group["items"]])
        entries = []
        for entry in group["items"]:
            entries.append(dict(entry, **entry_info(entry, names, st["groups"]), pct=df.chest_chance(group, entry)))
        used_by = [grp for grp in st["groups"].values()
                   if any(str(e["item"]).lower() == f"s{vnum}" for e in grp["items"])]
        return render_template("dbeditor/chests_edit.html", group=group, entries=entries, vnum=vnum,
                               title=(names.get(vnum) or {}).get("name", "") or group["name"],
                               icon=df.icon_url(vnum) if vnum in names else None,
                               custom=vnum in st["custom"], has_base=vnum in st["base"], types=df.CHEST_TYPES,
                               tokens=df.CHEST_TOKENS, fixed=group["type"] in df.CHEST_FIXED_TYPES,
                               status=df.live_state(st["spool"], "chest"), used_by=used_by, factors=FACTORS,
                               pct_text=df.pct_text, chance_text=df.chance_text, csrf=df.csrf_token())

    def validate(group_vnum, group_type, entries, groups):
        errors = []
        names = names_for([e["item"] for e in entries])
        if not entries:
            errors.append("Szkatułka musi mieć co najmniej jedną pozycję.")
        if len(entries) > df.CHEST_MAX_LINES:
            errors.append(f"Najwyżej {df.CHEST_MAX_LINES} pozycji w szkatułce.")
        for number, entry in enumerate(entries, 1):
            item = entry["item"]
            if item.isdigit():
                if int(item) not in names:
                    errors.append(f"Pozycja {number}: przedmiot {item} nie istnieje w item_proto (rdzeń gry by nie wstał).")
                elif int(entry["count"]) < 1:
                    errors.append(f"Pozycja {number}: ilość przedmiotu musi być co najmniej 1.")
            elif item in df.CHEST_TOKENS:
                pass
            elif item.startswith("s") and item[1:].isdigit():
                if int(item[1:]) == group_vnum:
                    errors.append(f"Pozycja {number}: szkatułka nie może losować sama z siebie.")
                elif int(item[1:]) not in groups:
                    errors.append(f"Pozycja {number}: nie ma grupy {item[1:]} do losowania.")
            else:
                errors.append(f"Pozycja {number}: „{item}” to nie VNUM ani znane słowo (gold, exp, mob, group, sNNNN...).")
            if group_type == "pct" and int(entry["prob"]) > 100:
                errors.append(f"Pozycja {number}: w szkatułce procentowej szansa to 0–100%.")
        if entries and not any(int(e["prob"]) > 0 for e in entries):
            errors.append("Co najmniej jedna pozycja musi mieć szansę większą od 0 (pusta szkatułka wywraca rdzeń przy otwarciu).")
        return errors, names

    def write(custom, reason):
        df.write_custom(df.spool_dir(), "chest", df.render_chest_custom(custom, reason), reason)

    @bp.post("/chests/<int:vnum>")
    @login_required
    def chests_save(vnum):
        back = url_for("dbeditor.chests_edit", vnum=vnum)
        if not df.csrf_ok(request.form):
            flash("Sesja formularza wygasła – odśwież stronę i spróbuj jeszcze raz.", "error")
            return redirect(back)
        st = state()
        group = st["groups"].get(vnum)
        custom = dict(st["custom"])
        action = request.form.get("action", "save")
        try:
            if action == "reset":
                if vnum not in custom:
                    flash("Ta szkatułka i tak jest taka jak w obrazie gry.")
                    return redirect(back)
                del custom[vnum]
                write(custom, f"przywrocono grupe {vnum} z obrazu gry")
                flash(f"Szkatułka {vnum} wróci do zawartości z obrazu gry po restarcie rdzeni (Zastosuj).", "success")
                return redirect(back if vnum in st["base"] else url_for("dbeditor.chests_index"))
            if group is None:
                raise ValueError(f"Nie ma szkatułki o VNUM {vnum}.")
            if group["type"] == "attr":
                raise ValueError("Grupy bonusów (attr) można tu tylko oglądać.")
            group_type = (request.form.get("group_type", group["type"]) or "").strip().lower()
            if group_type not in df.CHEST_TYPES or group_type in df.CHEST_FIXED_TYPES or group["type"] in df.CHEST_FIXED_TYPES:
                group_type = group["type"]
            entries, selected = [], set()
            count = min(int(request.form.get("row_count", "0") or 0), df.CHEST_MAX_LINES + 60)
            for index in range(count):
                prefix = f"r{index}_"
                item = (request.form.get(prefix + "item") or "").strip().lower()
                if request.form.get(prefix + "delete") == "1" or not item:
                    continue
                item = item.split()[0]
                label = f"Pozycja „{item}”"
                if request.form.get(prefix + "sel") == "1":
                    selected.add(len(entries))
                entries.append({
                    "item": item,
                    "count": str(int(df.parse_number(request.form.get(prefix + "count"), label + " – ilość", 0, 2000000000, True))),
                    "prob": str(int(df.parse_number(request.form.get(prefix + "prob"), label + (" – szansa %" if group_type == "pct" else " – waga"),
                                                    0, 100 if group_type == "pct" else 1000000000, True))),
                    "rare": str(int(df.parse_number(request.form.get(prefix + "rare") or "0", label + " – rare", 0, 100, True))),
                    "comment": ""})
            if action == "multiply":
                factor = df.parse_number(request.form.get("factor"), "Mnożnik", 0.01, 100)
                if group_type != "pct" and not selected:
                    raise ValueError("W szkatułce, która losuje 1 pozycję, pomnożenie WSZYSTKICH wag nic nie zmienia – "
                                     "zaznacz pozycje (kolumna ×), którym chcesz zwiększyć szansę.")
                entries = df.scale_chest_entries(group_type, entries, factor, selected or None)
            errors, names = validate(vnum, group_type, entries, st["groups"])
            if errors:
                for message in errors[:12]:
                    flash(message, "error")
                flash("Nic nie zostało zapisane – popraw błędy i zapisz jeszcze raz.", "error")
                return redirect(back)
            for entry in entries:
                info = entry_info(entry, names, st["groups"])
                entry["comment"] = info["label"]
            custom[vnum] = {"name": group["name"], "vnum": vnum, "type": group_type,
                            "extras": [x for x in group.get("extras", []) if x.split()[0].lower() == "effect"],
                            "items": entries}
            write(custom, f"zmieniono grupe {vnum}" + (" (mnoznik)" if action == "multiply" else ""))
        except ValueError as exc:
            flash(str(exc), "error")
            return redirect(back)
        except OSError as exc:
            flash(f"Nie udało się zapisać pliku w wolumenie spool: {exc}", "error")
            return redirect(back)
        flash(("Pomnożono szanse i zapisano" if action == "multiply" else "Zapisano") +
              f" {len(entries)} pozycji. Gracze dostaną nową zawartość po restarcie rdzeni (Zastosuj).", "success")
        return redirect(back)

    @bp.post("/chests/new")
    @login_required
    def chests_new():
        back = url_for("dbeditor.chests_index")
        if not df.csrf_ok(request.form):
            flash("Sesja formularza wygasła – odśwież stronę i spróbuj jeszcze raz.", "error")
            return redirect(back)
        raw = (request.form.get("vnum") or "").strip()
        if not raw.isdigit():
            flash("Podaj VNUM przedmiotu (szkatułki), dla którego ma powstać zawartość.", "error")
            return redirect(back)
        vnum = int(raw)
        st = state()
        if vnum in st["groups"]:
            return redirect(url_for("dbeditor.chests_edit", vnum=vnum))
        names = names_for([vnum])
        if vnum not in names:
            flash(f"Przedmiot {vnum} nie istnieje w item_proto.", "error")
            return redirect(back)
        custom = dict(st["custom"])
        custom[vnum] = {"name": f"PanelChest_{vnum}", "vnum": vnum, "type": "", "extras": [],
                        "items": [{"item": "27001", "count": "1", "prob": "1", "rare": "0", "comment": "Czerwona Mikstura (M) - do zmiany"}]}
        try:
            write(custom, f"nowa grupa {vnum}")
        except OSError as exc:
            flash(f"Nie udało się zapisać pliku w wolumenie spool: {exc}", "error")
            return redirect(back)
        note = "" if names[vnum]["type"] == GIFTBOX else " Uwaga: ten przedmiot nie jest typu szkatułka (GIFTBOX), więc gra może nie otwierać go jak szkatułki."
        flash(f"Utworzono zawartość dla {names[vnum]['name']} z jedną pozycją startową – ustaw ją i zapisz.{note}", "success")
        return redirect(url_for("dbeditor.chests_edit", vnum=vnum))

    @bp.post("/chests/restore")
    @login_required
    def chests_restore():
        back = url_for("dbeditor.chests_index")
        if not df.csrf_ok(request.form):
            flash("Sesja formularza wygasła – odśwież stronę i spróbuj jeszcze raz.", "error")
            return redirect(back)
        try:
            df.restore_backup(df.spool_dir(), "chest", request.form.get("backup", ""))
        except (ValueError, OSError) as exc:
            flash(f"Nie udało się przywrócić kopii: {exc}", "error")
            return redirect(back)
        flash("Przywrócono kopię (obecny stan też trafił do kopii). Zadziała po restarcie rdzeni (Zastosuj).", "success")
        return redirect(back)

    @bp.route("/chests/backup/<name>")
    @login_required
    def chests_backup_view(name):
        path = df.backup_file(df.spool_dir(), "chest", name)
        if path is None:
            abort(404)
        return path.read_bytes().decode("utf-8", "replace"), 200, {"Content-Type": "text/plain; charset=utf-8"}

    dbeditor.add_section("dbeditor.chests_index", "🧰", "Szkatułki",
                         "Co jest w każdej szkatułce i z jaką szansą – zawartość, szanse, mnożnik, nowe pozycje.")
