"""MT2009_PLUS_DB_EDITOR_V1: "Drop" in the database editor - every drop
source of the game at a glance, by plain Polish names, and whole groups
edited at once:

  * Zwykły drop (common_drop_item.txt) - every monster of a rank, by the
    killer's level;
  * Drop potworów (mob_drop_item.txt) - grouped into "zestawy": the monsters
    whose drop of one kind is exactly the same; one save changes all of them;
  * Drop specjalny (etc_drop_item.txt) - the item in mob_proto's drop_item;
  * drop_item_group.txt - shown read only (empty in this server);
  * search: a monster by name/VNUM, or "kto dropi X" by item name/VNUM.

Files, backups and how the game takes them over: dropfiles.py. The old
"Drop z potworów" page (/drops, one monster) works on the same files.
"""
import re

from flask import abort, flash, g, jsonify, redirect, render_template, request, url_for

from . import dropfiles as df

PAGE_SIZE = 40
FACTORS = ("0.5", "2", "3", "5")


def install(bp, ctx):
    import dbeditor
    df.register(bp)

    rows, game_text, login_required = ctx["rows"], ctx["game_text"], ctx["login_required"]

    # ------------------------------------------------------------ data ---
    def mobs():
        if "dbe_mobs" not in g:
            g.dbe_mobs = df.mob_table(rows, game_text)
        return g.dbe_mobs

    def names_for(vnums):
        cache = g.setdefault("dbe_names", {})
        missing = [v for v in vnums if str(v).lower().lstrip("s").isdigit() and int(str(v).lower().lstrip("s")) not in cache]
        if missing:
            found = df.item_names(rows, game_text, missing)
            cache.update(found)
            for value in missing:
                cache.setdefault(int(str(value).lower().lstrip("s")), None)
        return {k: v for k, v in cache.items() if v is not None}

    def state():
        if "dbe_drop_state" not in g:
            spool = df.spool_dir()
            text = df.read_text(df.base_path(spool, "mob"))
            base = df.parse_mob_drop(text or "")
            custom = df.read_mob_custom(spool)
            effective = df.effective_mob_drops(base, custom)
            g.dbe_drop_state = {"spool": spool, "base": base, "custom": custom, "effective": effective,
                                "sets": df.build_mob_sets(effective), "has_base": text is not None}
        return g.dbe_drop_state

    def chest_groups():
        spool = df.spool_dir()
        base, _ = df.read_chest_base(spool)
        groups = dict(base)
        groups.update(df.read_chest_custom(spool))
        return groups

    def common_table(spool):
        custom = df.custom_path(spool, "common")
        text = df.read_text(custom, "latin-1")
        source = "custom"
        if text is None:
            text, source = df.read_text(df.base_path(spool, "common"), "latin-1"), "image"
        return (df.parse_common(text) if text is not None else None), source

    def etc_table(spool):
        text = df.read_text(df.custom_path(spool, "etc"))
        source = "custom"
        if text is None:
            text, source = df.read_text(df.base_path(spool, "etc")), "image"
        return (df.parse_etc(text) if text is not None else None), source

    def mob_label(vnum):
        info = mobs().get(vnum)
        if info:
            return info["name"] or f"Potwór {vnum}"
        if vnum == 98006:
            return "Metin Ciemności w Bibliotece Wiedzy (grupa specjalna)"
        return f"Nieznany potwór {vnum}"

    def item_info(item, names, chests=None):
        """Name, icon, link and whether the line is broken, for one item
        field ("27001" or "s50011")."""
        item = str(item).lower()
        if item.isdigit():
            known = names.get(int(item))
            return {"label": known["name"] if known else "NIEZNANY PRZEDMIOT", "icon": df.icon_url(item),
                    "bad": known is None, "chest": None}
        if item.startswith("s") and item[1:].isdigit():
            vnum = int(item[1:])
            inner = names.get(vnum)
            exists = chests is None or vnum in chests
            label = "Losowanie z szkatułki " + item[1:] + (f" ({inner['name']})" if inner else "")
            return {"label": label + ("" if exists else " – BRAK TAKIEJ SZKATUŁKI"), "icon": df.icon_url(vnum),
                    "bad": not exists, "chest": vnum}
        return {"label": "?", "icon": None, "bad": True, "chest": None}

    def set_view(entry, names, chests=None):
        group = {"type": entry["kind"], "items": entry["items"], "kill_drop": entry["kill_drop"]}
        lines = []
        for item in entry["items"]:
            pct = df.mob_chance(entry["kind"], item, group)
            lines.append(dict(item, **item_info(item["item"], names, chests), pct=pct,
                              pct_input=df.fmt_num(df.num(item["prob"]) / 4 if entry["kind"] in ("drop", "thiefgloves")
                                                   else df.num(item["prob"]), 8)))
        return dict(entry, lines=lines, members=[{"vnum": m, "name": mob_label(m), "info": mobs().get(m)} for m in entry["mobs"]])

    # --------------------------------------------------------- overview ---
    @bp.route("/drops")
    @login_required
    def drops_index():
        spool = df.spool_dir()
        st = state()
        query = request.args.get("q", "").strip()
        kind = request.args.get("kind", "")
        kind = kind if kind in df.MOB_KINDS else ""
        try:
            page = max(1, int(request.args.get("page", "1")))
        except ValueError:
            page = 1
        sets = list(st["sets"].values())
        found_mobs, found_items, who = [], [], []
        if query:
            needle = query.casefold()
            found_mobs = [m for m in mobs().values() if query == str(m["vnum"]) or
                          (not query.isdigit() and len(query) >= 2 and needle in m["name"].casefold())]
            found_mobs.sort(key=lambda m: (str(m["vnum"]) != query, m["level"], m["vnum"]))
            found_mobs = found_mobs[:60]
            found_items = df.search_items(rows, game_text, query, 30)
            mob_set = {m["vnum"] for m in found_mobs}
            item_set = {str(i["vnum"]) for i in found_items}
            sets = [s for s in sets if mob_set.intersection(s["mobs"]) or
                    any(str(e["item"]).lstrip("s") in item_set for e in s["items"])]
            who = who_drops(found_items[:10], st, spool) if found_items else []
        if kind:
            sets = [s for s in sets if s["kind"] == kind]
        sets.sort(key=lambda s: (not s["custom"], df.MOB_KINDS.index(s["kind"]),
                                 min((mobs().get(m) or {"level": 999})["level"] for m in s["mobs"]), s["mobs"][0]))
        total = len(sets)
        pages = max(1, (total + PAGE_SIZE - 1) // PAGE_SIZE)
        page = min(page, pages)
        shown = sets[(page - 1) * PAGE_SIZE: page * PAGE_SIZE]
        names = names_for([e["item"] for s in shown for e in s["items"]])
        kinds = {k: sum(1 for s in st["sets"].values() if s["kind"] == k) for k in df.MOB_KINDS}
        common, common_source = common_table(spool)
        etc, etc_source = etc_table(spool)
        group_text = df.read_text(df.base_path(spool, "group"))
        drop_groups = df.parse_drop_item_group(group_text) if group_text else []
        status = {key: df.live_state(spool, key) for key in ("mob", "common", "etc")}
        return render_template(
            "dbeditor/drops.html", query=query, kind=kind, page=page, pages=pages, total=total,
            sets=[set_view(s, names) for s in shown], kinds=kinds, kind_names=df.MOB_KIND_SHORT,
            kind_help=df.MOB_KIND_NAMES, found_mobs=found_mobs, who=who, st=st, status=status,
            common_counts=[sum(1 for s in r if s["active"]) for r in common["ranks"]] if common else None,
            common_source=common_source, rank_names=df.COMMON_RANKS,
            etc_count=sum(1 for e in etc if e["active"]) if etc is not None else None, etc_source=etc_source,
            drop_groups=drop_groups, group_known=group_text is not None, pending=df.pending_changes(spool),
            mob_sets_of=lambda vnum: [(k, st["sets"].get(df.mob_set_id(st["effective"][(vnum, k)])))
                                      for k in df.MOB_KINDS if (vnum, k) in st["effective"]],
            pct_text=df.pct_text, chance_text=df.chance_text, mob_label=mob_label, csrf=df.csrf_token())

    def who_drops(items, st, spool):
        """For each item: every place it can come from, with the chance."""
        result = []
        chests = chest_groups()
        common, _ = common_table(spool)
        etc, _ = etc_table(spool)
        for item in items:
            vnum = str(item["vnum"])
            places = []
            for entry in st["sets"].values():
                group = {"type": entry["kind"], "items": entry["items"], "kill_drop": entry["kill_drop"]}
                for line in entry["items"]:
                    if str(line["item"]) == vnum:
                        places.append({"source": "Drop potworów · " + df.MOB_KIND_SHORT[entry["kind"]],
                                       "who": ", ".join(mob_label(m) for m in entry["mobs"][:6]) +
                                              (f" i {len(entry['mobs']) - 6} innych" if len(entry["mobs"]) > 6 else ""),
                                       "pct": df.mob_chance(entry["kind"], line, group),
                                       "link": url_for("dbeditor.drops_set", set_id=entry["id"]),
                                       "note": "co zabicie" + (f", gracz od poz. {entry['level_limit']}" if entry["kind"] == "limit" else "")})
                    elif str(line["item"]).lower() == "s" + vnum:
                        places.append({"source": "Drop potworów · losowanie ze szkatułki",
                                       "who": ", ".join(mob_label(m) for m in entry["mobs"][:6]),
                                       "pct": df.mob_chance(entry["kind"], line, group), "note": "wypada szkatułka/grupa",
                                       "link": url_for("dbeditor.drops_set", set_id=entry["id"])})
            if common:
                for rank, slots in enumerate(common["ranks"]):
                    for slot in slots:
                        if slot["active"] and slot["item"] == vnum:
                            places.append({"source": "Zwykły drop", "who": df.COMMON_RANKS[rank],
                                           "pct": df.num(slot["pct"]) / 4,
                                           "note": f"gracz na poziomie {slot['lv_start']}–{slot['lv_end']}",
                                           "link": url_for("dbeditor.drops_common", rank=rank)})
            if etc:
                for entry in etc:
                    if entry["active"] and entry["item"] == vnum:
                        owners = [m for m in mobs().values() if m["drop_item"] == item["vnum"]]
                        places.append({"source": "Drop specjalny", "pct": df.num(entry["prob"]) / 4,
                                       "who": ", ".join(m["name"] for m in owners[:6]) or "żaden potwór (pole drop_item)",
                                       "note": "", "link": url_for("dbeditor.drops_etc")})
            for chest in chests.values():
                for line in chest["items"]:
                    if str(line["item"]).lower() == vnum:
                        places.append({"source": "Szkatułka", "who": f"{chest['name']} ({chest['vnum']})",
                                       "pct": df.chest_chance(chest, line), "note": "na otwarcie",
                                       "link": url_for("dbeditor.chests_edit", vnum=chest["vnum"])
                                       if "dbeditor.chests_edit" in _endpoints() else None})
            result.append({"item": item, "icon": df.icon_url(item["vnum"]), "places": places})
        return result

    def _endpoints():
        from flask import current_app
        return current_app.view_functions

    # ------------------------------------------------------- one zestaw ---
    @bp.route("/drops/set/<set_id>")
    @login_required
    def drops_set(set_id):
        st = state()
        entry = st["sets"].get(set_id)
        if entry is None:
            flash("Tego zestawu już nie ma – drop tych potworów zmienił się. Wyszukaj potwora jeszcze raz.", "error")
            return redirect(url_for("dbeditor.drops_index"))
        chests = chest_groups()
        names = names_for([e["item"] for e in entry["items"]])
        view = set_view(entry, names, chests)
        custom_members = [m for m in entry["mobs"] if (m, entry["kind"]) in st["custom"]]
        image_members = {m for m in entry["mobs"] if any(gr["mob"] == m and gr["type"] == entry["kind"] for gr in st["base"])}
        return render_template("dbeditor/drops_set.html", s=view, kind_names=df.MOB_KIND_NAMES,
                               custom_members=custom_members, image_members=image_members,
                               status=df.live_state(st["spool"], "mob"), factors=FACTORS,
                               pct_text=df.pct_text, chance_text=df.chance_text, csrf=df.csrf_token(),
                               old_editor="drop_edit" in _endpoints())

    def read_group_form(kind, form):
        """The items table of a form -> a group, in the file's units."""
        entries = []
        count = min(int(form.get("row_count", "0") or 0), df.MOB_MAX_LINES + 60)
        for index in range(count):
            prefix = f"r{index}_"
            item = (form.get(prefix + "item") or "").strip().lower()
            if form.get(prefix + "delete") == "1" or not item:
                continue
            item = item.split()[0]
            label = f"Pozycja „{item}”"
            amount = int(df.parse_number(form.get(prefix + "count"), label + " – ilość", 1, df.MOB_MAX_COUNT, True))
            if kind == "kill":
                prob = str(int(df.parse_number(form.get(prefix + "weight"), label + " – waga", 1, 1000000, True)))
                rare = str(int(df.parse_number(form.get(prefix + "rare") or "0", label + " – rare", 0, 100, True)))
            else:
                prob, _ = df.form_chance(form, prefix, label + " – szansa %", 4 if kind in ("drop", "thiefgloves") else 1)
                rare = "0"
            entries.append({"item": item, "count": str(amount), "prob": prob, "rare": rare, "comment": ""})
        return {"type": kind, "items": entries,
                "kill_drop": str(int(df.parse_number(form.get("kill_drop") or "1", "Co ile zabójstw", 1, 1000000, True)))
                if kind == "kill" else "0",
                "level_limit": str(int(df.parse_number(form.get("level_limit") or "0", "Od poziomu", 0, 250, True)))
                if kind == "limit" else "0"}

    def validate_group(group, names, chests):
        errors = []
        kind, entries = group["type"], group["items"]
        if not entries:
            errors.append("Grupa musi mieć co najmniej jedną pozycję (żeby wyłączyć drop, użyj „Usuń ten drop”).")
        if len(entries) > df.MOB_MAX_LINES:
            errors.append(f"Najwyżej {df.MOB_MAX_LINES} pozycji w grupie (silnik czyta tylko numery 1–255).")
        for number, entry in enumerate(entries, 1):
            item = entry["item"]
            if item.isdigit():
                if int(item) not in names:
                    errors.append(f"Pozycja {number}: przedmiot {item} nie istnieje w item_proto (rdzeń gry by nie wstał).")
            elif item.startswith("s") and item[1:].isdigit():
                if kind != "drop":
                    errors.append(f"Pozycja {number}: losowanie ze szkatułki (s{item[1:]}) działa tylko w zwykłym dropie.")
                elif int(item[1:]) not in chests:
                    errors.append(f"Pozycja {number}: nie ma szkatułki {item[1:]} w special_item_group (nic by nie wypadło).")
            else:
                errors.append(f"Pozycja {number}: „{item}” to nie VNUM przedmiotu ani s+numer szkatułki.")
        if kind != "kill" and entries and not any(df.num(e["prob"]) > 0 for e in entries):
            errors.append("Co najmniej jedna pozycja musi mieć szansę większą od 0.")
        return errors

    def parse_targets(text):
        found = []
        for token in re.split(r"[\s,;]+", text or ""):
            if not token:
                continue
            if not token.isdigit():
                raise ValueError(f"„{token}” to nie VNUM potwora (wpisz numery oddzielone przecinkami).")
            if int(token) not in mobs():
                raise ValueError(f"Potwór {token} nie istnieje w mob_proto.")
            if int(token) not in found:
                found.append(int(token))
        return found

    def save_custom(custom, reason):
        df.write_custom(df.spool_dir(), "mob", df.render_mob_custom(custom, reason), reason)

    @bp.post("/drops/set/<set_id>")
    @login_required
    def drops_set_save(set_id):
        if not df.csrf_ok(request.form):
            flash("Sesja formularza wygasła – odśwież stronę i spróbuj jeszcze raz.", "error")
            return redirect(url_for("dbeditor.drops_set", set_id=set_id))
        st = state()
        entry = st["sets"].get(set_id)
        if entry is None:
            flash("Tego zestawu już nie ma – ktoś go właśnie zmienił. Nic nie zapisano.", "error")
            return redirect(url_for("dbeditor.drops_index"))
        kind = entry["kind"]
        action = request.form.get("action", "save")
        selected = [m for m in entry["mobs"] if str(m) in request.form.getlist("member")]
        custom = dict(st["custom"])
        back = url_for("dbeditor.drops_set", set_id=set_id)
        try:
            if action in ("remove", "reset"):
                if not selected:
                    raise ValueError("Zaznacz potwory, których to dotyczy.")
                for mob in selected:
                    has_base = any(gr["mob"] == mob and gr["type"] == kind for gr in st["base"])
                    if action == "reset":
                        custom.pop((mob, kind), None)
                    elif has_base:
                        custom[(mob, kind)] = {"mob": mob, "type": kind, "kill_drop": "1", "level_limit": "0", "items": []}
                    else:
                        custom.pop((mob, kind), None)
                save_custom(custom, f"{action} {kind} {len(selected)} potworow")
                flash((f"Drop „{df.MOB_KIND_SHORT[kind]}” usunięty u {len(selected)} potworów."
                       if action == "remove" else f"{len(selected)} potworów wróci do dropu z obrazu gry.") +
                      " Zadziała po restarcie rdzeni (Zastosuj).", "success")
                return redirect(url_for("dbeditor.drops_index", kind=kind))
            group = read_group_form(kind, request.form)
            targets = selected
            if action == "multiply":
                factor = df.parse_number(request.form.get("factor"), "Mnożnik", 0.01, 100)
                group = df.scale_mob_group(group, factor)
            elif action == "copy":
                targets = parse_targets(request.form.get("copy_to"))
                if not targets:
                    raise ValueError("Wpisz VNUM-y potworów, do których skopiować ten drop.")
            if not targets:
                raise ValueError("Zaznacz co najmniej jednego potwora, któremu zapisać zmiany.")
            names = names_for([e["item"] for e in group["items"]])
            errors = validate_group(group, names, chest_groups())
            if errors:
                for message in errors[:12]:
                    flash(message, "error")
                flash("Nic nie zostało zapisane – popraw błędy i zapisz jeszcze raz.", "error")
                return redirect(back)
            for line in group["items"]:
                known = names.get(int(line["item"])) if line["item"].isdigit() else None
                line["comment"] = known["name"] if known else line["item"]
            for mob in targets:
                custom[(mob, kind)] = dict(group, mob=mob)
            save_custom(custom, f"{action} {kind} dla {len(targets)} potworow")
        except ValueError as exc:
            flash(str(exc), "error")
            return redirect(back)
        except OSError as exc:
            flash(f"Nie udało się zapisać pliku w wolumenie spool: {exc}", "error")
            return redirect(back)
        g.pop("dbe_drop_state", None)
        new_id = df.mob_set_id(group)
        what = {"multiply": "Pomnożono szanse i zapisano", "copy": "Skopiowano drop do", "save": "Zapisano"}.get(action, "Zapisano")
        flash(f"{what} – {len(targets)} potworów, {len(group['items'])} pozycji. Gra użyje tego po restarcie rdzeni (Zastosuj).", "success")
        if action == "copy" and set(entry["mobs"]) - set(targets):
            return redirect(back)
        return redirect(url_for("dbeditor.drops_set", set_id=new_id))

    @bp.post("/drops/new")
    @login_required
    def drops_new():
        if not df.csrf_ok(request.form):
            flash("Sesja formularza wygasła – odśwież stronę i spróbuj jeszcze raz.", "error")
            return redirect(url_for("dbeditor.drops_index"))
        st = state()
        kind = request.form.get("kind", "")
        try:
            if kind not in df.MOB_KINDS:
                raise ValueError("Wybierz rodzaj dropu.")
            targets = parse_targets(request.form.get("mobs"))
            if not targets:
                raise ValueError("Wpisz VNUM co najmniej jednego potwora.")
            taken = [m for m in targets if (m, kind) in st["effective"]]
            if taken:
                raise ValueError("Te potwory już mają drop tego rodzaju (edytuj go albo skopiuj zestaw): " +
                                 ", ".join(f"{mob_label(m)} ({m})" for m in taken[:8]))
            group = read_group_form(kind, request.form)
            names = names_for([e["item"] for e in group["items"]])
            errors = validate_group(group, names, chest_groups())
            if errors:
                raise ValueError(" ".join(errors[:5]))
            for line in group["items"]:
                known = names.get(int(line["item"])) if line["item"].isdigit() else None
                line["comment"] = known["name"] if known else line["item"]
            custom = dict(st["custom"])
            for mob in targets:
                custom[(mob, kind)] = dict(group, mob=mob)
            save_custom(custom, f"nowy {kind} dla {len(targets)} potworow")
        except ValueError as exc:
            flash(str(exc), "error")
            return redirect(url_for("dbeditor.drops_index", q=request.form.get("mobs", "")))
        except OSError as exc:
            flash(f"Nie udało się zapisać pliku w wolumenie spool: {exc}", "error")
            return redirect(url_for("dbeditor.drops_index"))
        g.pop("dbe_drop_state", None)
        flash(f"Dodano drop „{df.MOB_KIND_SHORT[kind]}” dla {len(targets)} potworów. Zadziała po restarcie rdzeni (Zastosuj).", "success")
        return redirect(url_for("dbeditor.drops_set", set_id=df.mob_set_id(group)))

    # ----------------------------------------------------- common table ---
    @bp.route("/drops/common")
    @login_required
    def drops_common():
        spool = df.spool_dir()
        table, source = common_table(spool)
        try:
            rank = min(3, max(0, int(request.args.get("rank", "0"))))
        except ValueError:
            rank = 0
        slots = []
        if table:
            active = [s for s in table["ranks"][rank] if s["active"]]
            names = names_for([s["item"] for s in active])
            for slot in active:
                known = names.get(int(slot["item"]))
                slots.append(dict(slot, name=known["name"] if known else "NIEZNANY PRZEDMIOT", bad=known is None,
                                  icon=df.icon_url(slot["item"]), chance=df.num(slot["pct"]) / 4,
                                  pct_input=df.fmt_num(df.num(slot["pct"]) / 4, 8)))
        return render_template("dbeditor/drops_common.html", table=table, source=source, rank=rank, slots=slots,
                               rank_names=df.COMMON_RANKS, status=df.live_state(spool, "common"),
                               counts=[sum(1 for s in r if s["active"]) for r in table["ranks"]] if table else [],
                               factors=FACTORS, pct_text=df.pct_text, chance_text=df.chance_text, csrf=df.csrf_token())

    @bp.post("/drops/common")
    @login_required
    def drops_common_save():
        spool = df.spool_dir()
        try:
            rank = min(3, max(0, int(request.form.get("rank", "0"))))
        except ValueError:
            rank = 0
        back = url_for("dbeditor.drops_common", rank=rank)
        if not df.csrf_ok(request.form):
            flash("Sesja formularza wygasła – odśwież stronę i spróbuj jeszcze raz.", "error")
            return redirect(back)
        table, source = common_table(spool)
        if table is None:
            flash("Gra nie opublikowała jeszcze common_drop_item.txt – nie ma czego edytować.", "error")
            return redirect(back)
        action = request.form.get("action", "save")
        try:
            if action == "reset":
                if source != "custom":
                    flash("Zwykły drop i tak jest taki jak w obrazie gry.")
                    return redirect(back)
                df.write_custom(spool, "common", None, "zwykly drop z obrazu gry")
                flash("Zwykły drop wróci do stanu z obrazu gry po restarcie rdzeni (Zastosuj).", "success")
                return redirect(back)
            slots = []
            count = min(int(request.form.get("row_count", "0") or 0), 5000)
            for index in range(count):
                prefix = f"r{index}_"
                item = (request.form.get(prefix + "item") or "").strip()
                if request.form.get(prefix + "delete") == "1" or not item:
                    continue
                label = f"Pozycja „{item}”"
                if not item.isdigit() or int(item) <= 1:
                    raise ValueError(f"{label}: to nie VNUM przedmiotu.")
                start = int(df.parse_number(request.form.get(prefix + "from"), label + " – poziom od", 1, 250, True))
                end = int(df.parse_number(request.form.get(prefix + "to"), label + " – poziom do", 1, 250, True))
                if end < start:
                    raise ValueError(f"{label}: „poziom do” jest mniejszy niż „poziom od”.")
                file_pct, _ = df.form_chance(request.form, prefix, label + " – szansa %", 4)
                kept = file_pct == (request.form.get(prefix + "orig") or "").strip()
                slots.append(df.common_slot(start, end, file_pct, int(item), request.form.get(prefix + "label", "")[:30],
                                            request.form.get(prefix + "n") if kept else None))
            scopes = [rank]
            new_table = df.common_set_rank(table, rank, slots)
            if action == "multiply":
                factor = df.parse_number(request.form.get("factor"), "Mnożnik", 0.01, 100)
                scopes = list(range(4)) if request.form.get("scope") == "all" else [rank]
                for each in scopes:
                    current = slots if each == rank else [s for s in new_table["ranks"][each] if s["active"]]
                    scaled = [df.common_slot(s["lv_start"], s["lv_end"], df.fmt_num(min(400.0, df.num(s["pct"]) * factor), 9),
                                             s["item"], s.get("label", "")) for s in current]
                    # (a multiplied line gets its "1 in N" column recomputed)
                    new_table = df.common_set_rank(new_table, each, scaled)
            items = sorted({s["item"] for each in scopes for s in new_table["ranks"][each] if s["active"]})
            names = names_for(items)
            missing = [i for i in items if int(i) not in names]
            if missing:
                raise ValueError("Tych przedmiotów nie ma w item_proto (rdzeń gry by nie wstał): " + ", ".join(missing[:10]))
            df.write_custom(spool, "common", df.render_common(new_table),
                            f"zwykly drop {df.COMMON_RANKS[rank]}" + (f" x{request.form.get('factor')}" if action == "multiply" else ""))
        except ValueError as exc:
            flash(str(exc), "error")
            return redirect(back)
        except OSError as exc:
            flash(f"Nie udało się zapisać pliku w wolumenie spool: {exc}", "error")
            return redirect(back)
        flash(("Pomnożono szanse i zapisano" if action == "multiply" else "Zapisano") +
              " zwykły drop. Gra użyje go po restarcie rdzeni (Zastosuj).", "success")
        return redirect(back)

    # -------------------------------------------------------- etc table ---
    @bp.route("/drops/etc")
    @login_required
    def drops_etc():
        spool = df.spool_dir()
        entries, source = etc_table(spool)
        lines = []
        if entries is not None:
            names = names_for([e["item"] for e in entries if e["item"].isdigit()])
            owners = {}
            for mob in mobs().values():
                if mob["drop_item"]:
                    owners.setdefault(mob["drop_item"], []).append(mob)
            for entry in entries:
                if not entry["active"]:
                    continue
                known = names.get(int(entry["item"]))
                lines.append(dict(entry, name=known["name"] if known else "NIEZNANY PRZEDMIOT", bad=known is None,
                                  icon=df.icon_url(entry["item"]), chance=df.num(entry["prob"]) / 4,
                                  pct_input=df.fmt_num(df.num(entry["prob"]) / 4, 8),
                                  owners=owners.get(int(entry["item"]), [])))
        return render_template("dbeditor/drops_etc.html", lines=lines, source=source, known=entries is not None,
                               status=df.live_state(spool, "etc"), factors=FACTORS, pct_text=df.pct_text,
                               chance_text=df.chance_text, csrf=df.csrf_token())

    @bp.post("/drops/etc")
    @login_required
    def drops_etc_save():
        spool = df.spool_dir()
        back = url_for("dbeditor.drops_etc")
        if not df.csrf_ok(request.form):
            flash("Sesja formularza wygasła – odśwież stronę i spróbuj jeszcze raz.", "error")
            return redirect(back)
        entries, source = etc_table(spool)
        action = request.form.get("action", "save")
        try:
            if action == "reset":
                if source != "custom":
                    flash("Drop specjalny i tak jest taki jak w obrazie gry.")
                    return redirect(back)
                df.write_custom(spool, "etc", None, "drop specjalny z obrazu gry")
                flash("Drop specjalny wróci do stanu z obrazu gry po restarcie rdzeni (Zastosuj).", "success")
                return redirect(back)
            factor = df.parse_number(request.form.get("factor"), "Mnożnik", 0.01, 100) if action == "multiply" else 1.0
            new, seen = [], set()
            count = min(int(request.form.get("row_count", "0") or 0), 2000)
            for index in range(count):
                prefix = f"r{index}_"
                item = (request.form.get(prefix + "item") or "").strip()
                if request.form.get(prefix + "delete") == "1" or not item:
                    continue
                if not item.isdigit():
                    raise ValueError(f"„{item}” to nie VNUM przedmiotu.")
                if item in seen:
                    raise ValueError(f"Przedmiot {item} jest na liście dwa razy (gra wzięłaby tylko ostatni).")
                seen.add(item)
                prob, _ = df.form_chance(request.form, prefix, f"Pozycja „{item}” – szansa %", 4)
                new.append({"item": item, "prob": prob if factor == 1.0 else df.fmt_num(min(400.0, df.num(prob) * factor), 9)})
            names = names_for([e["item"] for e in new])
            missing = [e["item"] for e in new if int(e["item"]) not in names]
            if missing:
                raise ValueError("Tych przedmiotów nie ma w item_proto (rdzeń gry by nie wstał): " + ", ".join(missing[:10]))
            df.write_custom(spool, "etc", df.render_etc(new), "drop specjalny" + (f" x{factor:g}" if action == "multiply" else ""))
        except ValueError as exc:
            flash(str(exc), "error")
            return redirect(back)
        except OSError as exc:
            flash(f"Nie udało się zapisać pliku w wolumenie spool: {exc}", "error")
            return redirect(back)
        lonely = [e["item"] for e in new if not any(m["drop_item"] == int(e["item"]) for m in mobs().values())]
        flash("Zapisano drop specjalny. Gra użyje go po restarcie rdzeni (Zastosuj)." +
              (f" Uwaga: żaden potwór nie ma w mob_proto drop_item = {', '.join(lonely[:5])} – te pozycje nic nie dadzą." if lonely else ""),
              "success")
        return redirect(back)

    # ---------------------------------------------------------- backups ---
    @bp.route("/drops/backups")
    @login_required
    def drops_backups():
        spool = df.spool_dir()
        return render_template("dbeditor/drops_backups.html", backups=df.list_backups(spool, ("mob", "common", "etc"), 80),
                               endpoint="dbeditor.drops_backups", title="Drop", csrf=df.csrf_token())

    @bp.post("/drops/restore")
    @login_required
    def drops_restore():
        back = url_for("dbeditor.drops_backups")
        if not df.csrf_ok(request.form):
            flash("Sesja formularza wygasła – odśwież stronę i spróbuj jeszcze raz.", "error")
            return redirect(back)
        key = request.form.get("key", "")
        if key not in ("mob", "common", "etc"):
            abort(400)
        try:
            df.restore_backup(df.spool_dir(), key, request.form.get("backup", ""))
        except (ValueError, OSError) as exc:
            flash(f"Nie udało się przywrócić kopii: {exc}", "error")
            return redirect(back)
        flash("Przywrócono kopię (obecny stan też trafił do kopii). Zadziała po restarcie rdzeni (Zastosuj).", "success")
        return redirect(back)

    @bp.route("/drops/backup/<key>/<name>")
    @login_required
    def drops_backup_view(key, name):
        if key not in df.FILES:
            abort(404)
        path = df.backup_file(df.spool_dir(), key, name)
        if path is None:
            abort(404)
        encoding = "latin-1" if key == "common" else "utf-8"
        return path.read_bytes().decode(encoding, "replace"), 200, {"Content-Type": "text/plain; charset=utf-8"}

    # -------------------------------------------------------------- api ---
    @bp.route("/api/items")
    @login_required
    def api_items():
        found = df.search_items(rows, game_text, request.args.get("q", ""), 25)
        return jsonify({"ok": True, "items": [dict(i, icon=df.icon_url(i["vnum"])) for i in found]})

    @bp.route("/api/mobs")
    @login_required
    def api_mobs():
        query = request.args.get("q", "").strip()
        if len(query) < 2 and not query.isdigit():
            return jsonify({"ok": True, "mobs": []})
        needle = query.casefold()
        found = [m for m in mobs().values() if str(m["vnum"]) == query or (not query.isdigit() and needle in m["name"].casefold())]
        found.sort(key=lambda m: (str(m["vnum"]) != query, m["level"]))
        return jsonify({"ok": True, "mobs": [{"vnum": m["vnum"], "name": m["name"], "level": m["level"]} for m in found[:25]]})

    dbeditor.add_section("dbeditor.drops_index", "💀", "Drop",
                         "Wszystkie grupy dropu: zwykły drop, drop potworów (zestawy), drop specjalny. "
                         "Szukaj „kto dropi X”, zmieniaj całe grupy naraz.")
