"""MT2009_PLUS_DB_EDITOR_V1: "Ulepszanie u Kowala" - what a +N -> +N+1 step
costs, needs and how often it succeeds, for an operator who is not a
programmer.

What the game reads, once at boot (PROTO_FROM_DB):
  * world.item_proto.refined_vnum - the item a refine turns this one into
    (0: no further), and refine_set - the id of the recipe;
  * world.refine_proto (db/src/ClientManagerBoot.cpp InitializeRefineTable:
    id, cost, prob, vnum0..4/count0..4; src_vnum and result_vnum are not
    read). CHARACTER::DoRefine (char_item.cpp): the yang cost (a guild
    smith: ComputeRefineFee, cost x4, x3 more for another kingdom - an int,
    so a cost over ~179 million overflows there), each of the five
    materials checked and taken separately (two slots of one item are
    merged here, or the check would ask for less than is taken), then
    number(1, 100) <= prob succeeds (a guild smith -10, the Treasure Hunt
    blessing; scrolls of blessing etc. change it in DoRefineWithScroll).
    A failed normal refine destroys the item.
  * Many items share one recipe (a weapon and an armour of one level often
    use the same refine_set). Editing it changes all of them; "only this
    family" clones it into a new refine_proto id (CLONE_FIRST_ID and up)
    and points the family's items at the clone.
  * Not in item_proto, so not on a family page: the Ritual of Awakening
    (weapon 75 +9 -> awakened +0, recipe 7110) and the soul stones +4..+8
    (7204-7208) - char_item.cpp AwakeningSpecialRefineSet; their recipe
    page (/db/refine/set/<id>) edits them.

Bots (playerbot_economy.h, playerbot_gear.h, playerbot_demon_tower.h) refine
by the very same recipes (CRefineManager::GetRefineRecipe) - they collect
and keep the materials a recipe names, pay its yang and take its chance.

apply.sh rewrites some recipes at every start (BOOT_RULES, checked by
test_dbeditor_shops.py) and the refine_set of the awakened weapons and soul
stones (dbeditor/protected.py).
"""
import re
from collections import OrderedDict

from flask import abort, flash, redirect, render_template, request, url_for

from dbeditor import common_items as common
from dbeditor import protected
from dbeditor.shops import endpoints, load_items

REFINE = "world.refine_proto"
PROTO = "world.item_proto"
SLOTS = 5                                   # REFINE_MATERIAL_MAX_NUM
VALUE_COLS = tuple(f"{p}{k}" for k in range(SLOTS) for p in ("vnum", "count")) + ("cost", "prob")
ROW_COLS = ("id",) + VALUE_COLS + ("src_vnum", "result_vnum")
CLONE_FIRST_ID = 30000                      # clones never meet the package's ids (apply.sh: 7100-7208)
REFINE_SET_MAX = 65535                      # item_proto.refine_set smallint unsigned
COST_MAX = 2000000000                       # what a character may carry
GUILD_SAFE_COST = 178956970                 # x4 (guild smith) x3 (other kingdom) still fits an int
COUNT_MAX = 32767                           # refine_proto.countN smallint

# apply.sh at every start (INSERT ... ON DUPLICATE KEY UPDATE / DELETE).
BOOT_RULES = [
    (7100, 7108, "always", VALUE_COLS, "Rytuał Przebudzenia: broń przebudzona +0…+9 (Digi Rasta)"),
    (7110, 7110, "always", VALUE_COLS, "Rytuał Przebudzenia: broń 75 +9 → broń przebudzona"),
    (7204, 7208, "always", ("vnum0", "count0", "vnum1", "count1", "cost", "prob"), "kamienie duszy +4…+8 → +5…+9"),
    (7200, 7203, "deleted", (), "stare przepisy kamieni duszy – usuwane"),
]
SPECIAL_RECIPES = {7110: "Rytuał Przebudzenia (broń 75 +9 → przebudzona +0)",
                   7204: "Kamień duszy +4 → +5", 7205: "Kamień duszy +5 → +6", 7206: "Kamień duszy +6 → +7",
                   7207: "Kamień duszy +7 → +8", 7208: "Kamień duszy +8 → +9"}
LEVEL = re.compile(r"\+\s*(\d+)\s*$")
FAMILY_SEARCHES = ("Miecz", "Ostrze", "Łuk", "Dzwon", "Wachlarz", "Zbroja", "Hełm", "Tarcza", "Bransoleta", "Buty",
                   "Naszyjnik", "Kolczyki")


def boot_rule(recipe_id):
    for low, high, kind, cols, note in BOOT_RULES:
        if low <= int(recipe_id) <= high:
            return {"kind": kind, "cols": list(cols), "note": note}
    return None


def _text(value):
    return common.text_of(value) or ""


def level_of(item):
    match = LEVEL.search(item.get("name") or "")
    return int(match.group(1)) if match else None


def _spec(low, high, label):
    return {"kind": "int", "min": low, "max": high, "label": label}


SPECS = {"cost": _spec(0, COST_MAX, "Koszt w Yang (cost)"), "prob": _spec(0, 100, "Szansa % (prob)")}
for _k in range(SLOTS):
    SPECS[f"vnum{_k}"] = _spec(0, 4294967295, f"Materiał {_k + 1} – przedmiot (vnum{_k})")
    SPECS[f"count{_k}"] = _spec(-32768, COUNT_MAX, f"Materiał {_k + 1} – ilość (count{_k})")


# ---- reading ----------------------------------------------------------------------

def item_rows(rows, where, params):
    found = rows(f"SELECT vnum, CAST(locale_name AS BINARY) AS locale_name, type, subtype, refined_vnum, refine_set "
                 f"FROM {PROTO} WHERE {where} ORDER BY vnum", params)
    return [{"vnum": int(r["vnum"]), "name": _text(r["locale_name"]) or f"Przedmiot {r['vnum']}",
             "type": int(r.get("type") or 0), "subtype": int(r.get("subtype") or 0),
             "refined_vnum": int(r.get("refined_vnum") or 0), "refine_set": int(r.get("refine_set") or 0)}
            for r in found]


def load_chain(rows, vnum):
    """The item's refine chain, lowest first: back along refined_vnum (the
    one of the same ten when several lead here), then forward."""
    found = item_rows(rows, "vnum=%s", (vnum,))
    if not found:
        return None
    chain, seen = [found[0]], {found[0]["vnum"]}
    current = found[0]
    for _ in range(30):
        before = [p for p in item_rows(rows, "refined_vnum=%s", (current["vnum"],)) if p["vnum"] not in seen]
        if not before:
            break
        current = min(before, key=lambda p: (p["vnum"] // 10 != current["vnum"] // 10, p["vnum"]))
        chain.insert(0, current)
        seen.add(current["vnum"])
    current = found[0]
    for _ in range(30):
        target = current["refined_vnum"]
        if not target or target in seen:
            break
        after = item_rows(rows, "vnum=%s", (target,))
        if not after:
            current["missing_result"] = target
            break
        current = after[0]
        chain.append(current)
        seen.add(current["vnum"])
    return chain


def load_recipes(rows, ids):
    wanted = sorted({int(i) for i in ids if int(i) > 0})
    if not wanted:
        return {}
    marks = ",".join(["%s"] * len(wanted))
    return {int(r["id"]): {c: int(r.get(c) or 0) for c in ROW_COLS}
            for r in rows(f"SELECT {', '.join(ROW_COLS)} FROM {REFINE} WHERE id IN ({marks})", wanted)}


def recipe_users(rows, ids):
    """{recipe id: [items using it]}."""
    wanted = sorted({int(i) for i in ids if int(i) > 0})
    if not wanted:
        return {}
    marks = ",".join(["%s"] * len(wanted))
    out = {}
    for item in item_rows(rows, f"refine_set IN ({marks})", wanted):
        out.setdefault(item["refine_set"], []).append(item)
    return out


def normalized(values):
    """The recipe as the game should get it: materials with an item and a
    count, one slot per item (counts merged), packed to the front."""
    merged = OrderedDict()
    for k in range(SLOTS):
        vnum, count = int(values.get(f"vnum{k}") or 0), int(values.get(f"count{k}") or 0)
        if vnum > 0 and count > 0:
            merged[vnum] = merged.get(vnum, 0) + count
    out = {"cost": int(values.get("cost") or 0), "prob": int(values.get("prob") or 0)}
    pairs = list(merged.items())[:SLOTS]
    for k in range(SLOTS):
        out[f"vnum{k}"], out[f"count{k}"] = pairs[k] if k < len(pairs) else (0, 0)
    return out


def key_of(values):
    return tuple(values[c] for c in VALUE_COLS)


def materials_of(values):
    return [(values[f"vnum{k}"], values[f"count{k}"]) for k in range(SLOTS) if values[f"vnum{k}"]]


# ---- the form ------------------------------------------------------------------------

def parse_step(form, prefix, label):
    """(normalized values, errors, vnums named) of one step's fields."""
    errors, values, vnums = [], {}, []
    for col, field, spec_label, low, high in (("cost", "cost", "koszt w Yang", 0, COST_MAX),
                                              ("prob", "prob", "szansa", 1, 100)):
        value, error = common.validate({"kind": "int", "min": low, "max": high}, form.get(f"{field}_{prefix}", ""),
                                       f"{label}: {spec_label}")
        if error:
            errors.append(error + (" (szansa 1–100%)" if col == "prob" else ""))
        else:
            values[col] = value
    for k in range(SLOTS):
        vtext = str(form.get(f"mv_{prefix}_{k}", "") or "").strip()
        ctext = str(form.get(f"mc_{prefix}_{k}", "") or "").strip()
        if vtext in ("", "0"):
            values[f"vnum{k}"], values[f"count{k}"] = 0, 0
            continue
        if not vtext.isdigit():
            errors.append(f"{label}: materiał {k + 1} – „{vtext}” nie jest numerem przedmiotu (wybierz z listy).")
            continue
        count, error = common.validate({"kind": "int", "min": 1, "max": COUNT_MAX}, ctext or "1",
                                       f"{label}: materiał {k + 1} – ilość")
        if error:
            errors.append(error)
            continue
        values[f"vnum{k}"], values[f"count{k}"] = int(vtext), count
        vnums.append(int(vtext))
    if errors:
        return None, errors, vnums
    return normalized(values), errors, vnums


def form_from(values, prefix):
    """The form fields of one step holding these values (presets)."""
    out = {f"cost_{prefix}": str(values["cost"]), f"prob_{prefix}": str(values["prob"])}
    for k in range(SLOTS):
        out[f"mv_{prefix}_{k}"] = str(values[f"vnum{k}"] or "")
        out[f"mc_{prefix}_{k}"] = str(values[f"count{k}"] or "")
    return out


def apply_preset(values, kind, amount):
    """One step's values after a preset; None when it does not apply."""
    out = dict(values)
    if kind == "chance_mul":
        out["prob"] = max(1, min(100, int(round(values["prob"] * amount))))
    elif kind == "chance_set":
        out["prob"] = max(1, min(100, int(amount)))
    elif kind == "cost_mul":
        out["cost"] = max(0, min(COST_MAX, int(round(values["cost"] * amount))))
    elif kind == "count_mul":
        for k in range(SLOTS):
            if out[f"vnum{k}"]:
                out[f"count{k}"] = max(1, min(COUNT_MAX, int(round(values[f"count{k}"] * amount))))
    elif kind == "drop":
        for k in range(SLOTS):
            if out[f"vnum{k}"] == int(amount):
                out[f"vnum{k}"], out[f"count{k}"] = 0, 0
        out = normalized(out)
    else:
        return None
    return out


PRESET_NAMES = {"chance_mul": "szansa ×{a}", "chance_set": "szansa = {a}%", "cost_mul": "koszt ×{a}",
                "count_mul": "ilość materiałów ×{a}", "drop": "bez materiału {a}"}


def parse_preset(text):
    """'kind:amount:from:to' -> (kind, amount, from, to) or None."""
    parts = str(text or "").split(":")
    if len(parts) != 4 or parts[0] not in PRESET_NAMES:
        return None
    try:
        amount = float(parts[1].replace(",", "."))
        low, high = int(parts[2]), int(parts[3])
    except ValueError:
        return None
    if not (0 < amount <= 1000000) or low > high:
        return None
    return parts[0], amount, low, high


# ---- the pages -----------------------------------------------------------------------------

def install(bp, ctx):
    import dbeditor

    common.init(ctx)
    common.register_table(REFINE, "id", SPECS, "Przepis ulepszenia", "dbeditor.refine_recipe",
                          row_cols=ROW_COLS, row_label="cały przepis (nowy / usunięty)")
    if PROTO not in common.TABLES or "refine_set" not in common.TABLES[PROTO]["cols"]:
        cols = dict(common.TABLES[PROTO]["cols"]) if PROTO in common.TABLES else {}
        cols["refine_set"] = _spec(0, REFINE_SET_MAX, "Przepis ulepszenia (refine_set)")
        common.register_table(PROTO, "vnum", cols, "Przedmiot")
    common.install_history(bp, ctx)
    login_required = ctx["login_required"]

    def rows(sql, params=()):
        return common.ctx()["rows"](sql, params)

    def page_context():
        try:
            return common.pending_context()
        except Exception:
            return {"pending_total": 0}

    @bp.route("/refine")
    @login_required
    def refine():
        query = (request.args.get("q") or "").strip()
        families = []
        more = False
        if query and (query.isdigit() or len(query) >= 2):
            if query.isdigit():
                found = item_rows(rows, "vnum=%s OR locale_name LIKE %s", (int(query), f"%{query}%"))
            else:
                found = item_rows(rows, "locale_name LIKE %s", (f"%{query}%",))
            seen = OrderedDict()
            for item in found[:3000]:
                stem = LEVEL.sub("", item["name"]).strip()
                key = (stem, item["vnum"] // 10)
                if key not in seen:
                    seen[key] = {"vnum": item["vnum"], "name": stem or item["name"], "levels": [], "refines": False}
                level = level_of(item)
                if level is not None:
                    seen[key]["levels"].append(level)
                seen[key]["refines"] = seen[key]["refines"] or bool(item["refined_vnum"] or item["refine_set"])
            for key in [k for k, f in seen.items() if not f["refines"]]:
                del seen[key]
            families = list(seen.values())
            more = len(families) > 200
            families = families[:200]
        return render_template("dbeditor/refine.html", query=query, families=families, more=more,
                               searches=FAMILY_SEARCHES, specials=SPECIAL_RECIPES, **page_context())

    def build_steps(chain):
        ids = [it["refine_set"] for it in chain if it["refined_vnum"]]
        recipes = load_recipes(rows, ids)
        users = recipe_users(rows, ids)
        chain_vnums = {it["vnum"] for it in chain}
        materials = load_items(rows, [v for r in recipes.values() for v, _c in materials_of(r)])
        steps = []
        for index, item in enumerate(chain):
            if not item["refined_vnum"]:
                continue
            result = chain[index + 1] if index + 1 < len(chain) and chain[index + 1]["vnum"] == item["refined_vnum"] else None
            recipe = recipes.get(item["refine_set"])
            using = users.get(item["refine_set"], [])
            step = {"index": len(steps), "item": item, "result": result, "set": item["refine_set"], "recipe": recipe,
                    "values": normalized(recipe) if recipe else None,
                    "users": using, "outside": [u for u in using if u["vnum"] not in chain_vnums],
                    "level": level_of(result) if result and level_of(result) is not None else len(steps) + 1,
                    "boot": boot_rule(item["refine_set"]) if item["refine_set"] else None,
                    "boot_set": "refine_set" in protected.overwritten_cols(item["vnum"], item["type"])}
            steps.append(step)
        # The scope choice is asked once per shared recipe, on its first step.
        asked = set()
        for step in steps:
            step["ask_scope"] = bool(step["recipe"] and step["outside"] and step["set"] not in asked)
            asked.add(step["set"])
        return steps, materials

    def chain_page(vnum, chain, raw=None, preview=None):
        steps, materials = build_steps(chain)
        named = set()
        for key, value in (raw or {}).items():
            if key.startswith("mv_") and str(value).strip().isdigit():
                named.add(int(value))
        if named - set(materials):
            materials.update(load_items(rows, named - set(materials)))
        used = OrderedDict()
        for step in steps:
            if step["values"]:
                for v, _c in materials_of(step["values"]):
                    used[v] = materials.get(v, {"name": f"Przedmiot {v}"})["name"]
        try:
            history = common.history_batches(10, REFINE)
            ids = {str(s["set"]) for s in steps}
            history = [b for b in history if any(str(r["row_key"]) in ids for r in b["rows"])][:6]
        except Exception:
            history = []
        return render_template("dbeditor/refine_edit.html", vnum=vnum, chain=chain, steps=steps, materials=materials,
                               used=used, raw=raw or {}, preview=preview, slots=SLOTS, history=history,
                               guild_safe=GUILD_SAFE_COST, url_map_endpoints=endpoints(),
                               dbe_csrf=common.csrf_token(), **common.template_helpers(), **page_context())

    @bp.route("/refine/<int:vnum>", methods=["GET", "POST"])
    @login_required
    def refine_edit(vnum):
        chain = load_chain(rows, vnum)
        if chain is None:
            abort(404)
        if request.method == "GET":
            return chain_page(vnum, chain)
        if not common.check_csrf():
            return redirect(url_for("dbeditor.refine_edit", vnum=vnum))
        if request.form.get("preset"):  # a preset button: a preview, nothing is saved
            return preset(vnum, chain)
        return save_chain(vnum, chain)

    def preset(vnum, chain):
        steps, _materials = build_steps(chain)
        chosen = request.form.get("preset", "")
        if chosen == "custom":
            chosen = ":".join(request.form.get(f"preset_{k}", "") for k in ("kind", "amount", "from", "to"))
        parsed = parse_preset(chosen)
        raw = {k: v for k, v in request.form.items() if k not in ("action", "preset")}
        if parsed is None:
            flash("Nie rozpoznano szablonu – wybierz rodzaj, wartość i zakres poziomów.", "error")
            return chain_page(vnum, chain, raw=raw)
        kind, amount, low, high = parsed
        touched = []
        for step in steps:
            if not low <= step["level"] <= high:
                continue
            if not step["recipe"] and raw.get(f"create_{step['index']}") != "1":
                continue
            values, errors, _v = parse_step(raw, step["index"], f"+{step['level']}")
            if values is None:
                for error in errors[:4]:
                    flash(error, "error")
                return chain_page(vnum, chain, raw=raw)
            changed = apply_preset(values, kind, amount)
            if changed is not None and changed != values:
                raw.update(form_from(changed, step["index"]))
                touched.append(f"+{step['level']}")
        name = PRESET_NAMES[kind].format(a=f"{amount:g}".replace(".", ","))
        if touched:
            flash(f"Podgląd „{name}” dla {', '.join(touched)} – nic jeszcze nie zapisano. Sprawdź i kliknij „Zapisz”.", "success")
        else:
            flash(f"„{name}” nic nie zmienia w krokach +{low}…+{high}.", "warning")
        return chain_page(vnum, chain, raw=raw, preview=name if touched else None)

    def next_ids(count):
        top = rows(f"SELECT MAX(id) AS top FROM {REFINE}")
        start = max(CLONE_FIRST_ID, int((top[0].get("top") if top else 0) or 0) + 1)
        ids = list(range(start, start + count))
        if ids and ids[-1] > REFINE_SET_MAX:
            raise ValueError(f"Brak wolnych numerów przepisów (najwyżej {REFINE_SET_MAX}).")
        return ids

    def save_chain(vnum, chain):
        steps, _materials = build_steps(chain)
        form = request.form
        errors, warnings, new_values, named = [], [], {}, set()
        for step in steps:
            label = f"Krok +{step['level'] - 1} → +{step['level']}"
            if str(form.get(f"set_{step['index']}", "")) != str(step["set"]):
                errors.append("Przepisy tej rodziny zmieniono w międzyczasie – odśwież stronę.")
                break
            if not step["recipe"] and form.get(f"create_{step['index']}") != "1":
                continue
            values, problems, vnums = parse_step(form, step["index"], label)
            errors += problems
            named.update(vnums)
            if values is not None:
                new_values[step["index"]] = values
        known = load_items(rows, named) if named else {}
        for vnum_named in sorted(named - set(known)):
            errors.append(f"Materiał {vnum_named} nie istnieje w bazie przedmiotów.")
        if errors:
            for error in errors[:8]:
                flash(error, "error")
            flash("Nic nie zapisano – popraw zaznaczone pola.", "error")
            return chain_page(vnum, chain, raw=dict(form.items()))
        changed = {i: v for i, v in new_values.items()
                   if steps[i]["values"] is None or key_of(v) != key_of(steps[i]["values"])}
        if not changed:
            flash("Brak zmian do zapisania.", "success")
            return redirect(url_for("dbeditor.refine_edit", vnum=vnum))
        for i, values in changed.items():
            if values["cost"] > GUILD_SAFE_COST:
                warnings.append(f"+{steps[i]['level']}: koszt ponad {GUILD_SAFE_COST:,} Yang – u kowala gildii (×4, ×3 dla "
                                "innego królestwa) liczba się przepełni.".replace(",", " "))
            if values["prob"] < 100 and not materials_of(values) and values["cost"] == 0:
                warnings.append(f"+{steps[i]['level']}: przepis bez kosztu i materiałów.")
        # What to write: in place, clones (new ids) and new recipes.
        updates, clones, creates, reassign = {}, [], [], {}
        by_set = OrderedDict()
        for step in steps:
            if step["recipe"]:
                by_set.setdefault(step["set"], []).append(step)
        for set_id, members in by_set.items():
            if not any(m["index"] in changed for m in members):
                continue
            current = key_of(members[0]["values"])
            groups = OrderedDict()
            for m in members:
                values = changed.get(m["index"], m["values"])
                groups.setdefault(key_of(values), {"values": values, "steps": []})["steps"].append(m)
            outside = members[0]["outside"]
            scope = form.get(f"scope_{set_id}", "family") if outside else "family"
            if outside and scope == "all":
                if len(groups) > 1:
                    errors.append(f"Przepis #{set_id} mają kroki " + ", ".join(f"+{m['level']}" for m in members)
                                  + " z różnymi wartościami – przy „dla wszystkich” muszą być takie same. "
                                  "Wybierz „tylko ta rodzina”.")
                    continue
                updates[set_id] = next(iter(groups.values()))["values"]
                continue
            in_place_used = outside or current in groups
            for key, group in groups.items():
                if key == current:
                    continue
                if not in_place_used:
                    updates[set_id] = group["values"]
                    in_place_used = True
                    continue
                clones.append(group)
        for step in steps:
            if not step["recipe"] and step["index"] in changed:
                if step["set"]:
                    creates.append({"id": step["set"], **changed[step["index"]]})
                else:
                    clones.append({"values": changed[step["index"]], "steps": [step]})
        if errors:
            for error in errors[:8]:
                flash(error, "error")
            flash("Nic nie zapisano.", "error")
            return chain_page(vnum, chain, raw=dict(form.items()))
        try:
            ids = next_ids(len(clones))
        except ValueError as exc:
            flash(str(exc), "error")
            return chain_page(vnum, chain, raw=dict(form.items()))
        inserts = list(creates)
        for new_id, group in zip(ids, clones):
            inserts.append({"id": new_id, **group["values"]})
            for m in group["steps"]:
                reassign[m["item"]["vnum"]] = new_id
                if m["boot_set"]:
                    warnings.append(f"{m['item']['name']}: przepis tego przedmiotu ustawia skrypt startowy (apply.sh) – "
                                    f"po restarcie wróci do #{m['set']}, a nowy przepis #{new_id} nie będzie używany.")
        family = chain[0]["name"]
        names = {it["vnum"]: it["name"] for it in chain}

        def recipe_label(key):
            for step in steps:
                if str(step["set"]) == str(key):
                    return f"{family}: +{step['level'] - 1}→+{step['level']}"
            for new_id, group in zip(ids, clones):
                if str(new_id) == str(key):
                    return f"{family}: +{group['steps'][0]['level'] - 1}→+{group['steps'][-1]['level']} (tylko ta rodzina)"
            return family

        note = f"Ulepszanie: {LEVEL.sub('', family).strip() or family}"
        try:
            batch = None
            if inserts:
                batch, _done = common.write_rows(REFINE, inserts=inserts, note=note, label_of=recipe_label)
            saved = []
            if updates:
                batch, saved = common.save_rows(REFINE, sorted(updates.items()), note=note, label_of=recipe_label,
                                                batch=batch)
            if reassign:
                batch, _r = common.save_rows(PROTO, sorted((v, {"refine_set": s}) for v, s in reassign.items()),
                                             note=note, label_of=lambda k: names.get(int(k), ""), batch=batch)
        except Exception as exc:
            flash(f"Nie udało się zapisać: {exc}", "error")
            return redirect(url_for("dbeditor.refine_edit", vnum=vnum))
        for set_id in updates:
            rule = boot_rule(set_id)
            if rule and rule["kind"] == "always":
                warnings.append(f"Przepis #{set_id} ({rule['note']}) ustawia skrypt startowy (apply.sh) przy każdym starcie "
                                "– zmiana zostanie nadpisana.")
            users = next(s for s in steps if s["set"] == set_id)["users"]
            if len(users) > len([s for s in steps if s["set"] == set_id]):
                warnings.append(f"Przepis #{set_id} zmienił się dla wszystkich {len(users)} przedmiotów, które go używają.")
        for warning in warnings:
            flash(warning, "warning")
        flash("Boty ulepszają według tych samych przepisów – zmiana dotyczy także ich (koszt, materiały, szansa).", "warning")
        flash(f"Zapisano: zmienione przepisy {len(updates)}, nowe {len(inserts)}"
              + (f" (przedmioty przepięte na nowe przepisy: {len(reassign)})" if reassign else "")
              + ". Zmiany czekają na zastosowanie (restart gry).", "success")
        return redirect(url_for("dbeditor.refine_edit", vnum=vnum))

    @bp.route("/refine/set/<int:vnum>", methods=["GET", "POST"])
    @login_required
    def refine_recipe(vnum):
        """One recipe by its id: who uses it, and its values for all of them."""
        recipe = load_recipes(rows, [vnum]).get(vnum)
        users = recipe_users(rows, [vnum]).get(vnum, [])
        if recipe is None and not users:
            abort(404)
        raw = {}
        if request.method == "POST" and recipe is not None:
            if not common.check_csrf():
                return redirect(url_for("dbeditor.refine_recipe", vnum=vnum))
            values, errors, named = parse_step(request.form, "r", f"Przepis #{vnum}")
            known = load_items(rows, named) if named else {}
            errors += [f"Materiał {v} nie istnieje w bazie przedmiotów." for v in sorted(set(named) - set(known))]
            if errors:
                for error in errors[:8]:
                    flash(error, "error")
                flash("Nic nie zapisano – popraw zaznaczone pola.", "error")
                raw = dict(request.form.items())
            elif key_of(values) == key_of(normalized(recipe)):
                flash("Brak zmian do zapisania.", "success")
                return redirect(url_for("dbeditor.refine_recipe", vnum=vnum))
            else:
                try:
                    common.save_rows(REFINE, [(vnum, values)], note=f"Przepis ulepszenia #{vnum}",
                                     label_of=lambda k: SPECIAL_RECIPES.get(vnum) or f"Przepis #{vnum}")
                except Exception as exc:
                    flash(f"Nie udało się zapisać: {exc}", "error")
                    return redirect(url_for("dbeditor.refine_recipe", vnum=vnum))
                rule = boot_rule(vnum)
                if rule and rule["kind"] == "always":
                    flash(f"Ten przepis ({rule['note']}) ustawia skrypt startowy (apply.sh) przy każdym starcie – "
                          "zmiana zostanie nadpisana.", "warning")
                flash("Boty ulepszają według tych samych przepisów – zmiana dotyczy także ich.", "warning")
                flash(f"Zapisano przepis #{vnum} (używa go {len(users)} przedmiot(ów)). "
                      "Zmiany czekają na zastosowanie (restart gry).", "success")
                return redirect(url_for("dbeditor.refine_recipe", vnum=vnum))
        values = normalized(recipe) if recipe else None
        materials = load_items(rows, [v for v, _c in materials_of(values)] if values else [])
        try:
            history = common.history_batches(10, REFINE, vnum)
        except Exception:
            history = []
        return render_template("dbeditor/refine_recipe.html", recipe_id=vnum, values=values, users=users,
                               materials=materials, raw=raw, slots=SLOTS, boot=boot_rule(vnum),
                               special=SPECIAL_RECIPES.get(vnum), history=history, guild_safe=GUILD_SAFE_COST,
                               url_map_endpoints=endpoints(), dbe_csrf=common.csrf_token(),
                               **common.template_helpers(), **page_context())

    dbeditor.add_section("dbeditor.refine", "⚒️", "Ulepszanie u Kowala",
                         "koszt, materiały i szansa każdego kroku +0…+9 – dla jednej rodziny albo dla wszystkich")
