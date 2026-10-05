"""MT2009_PLUS_DB_EDITOR_V1: "Sklepy NPC" - what the village merchants sell,
for an operator who is not a programmer.

What the game reads (db/src/ClientManagerBoot.cpp InitializeShopTable, once
at boot, from the world database):

    SELECT shop.vnum, shop.npc_vnum, shop_item.item_vnum, shop_item.count
      FROM shop LEFT JOIN shop_item ON shop.vnum = shop_item.shop_vnum
     ORDER BY shop.vnum, shop_item.item_vnum

  * world.shop (vnum, name, npc_vnum): one shop per NPC race - clicking an
    NPC of that race opens it (shop_manager.cpp GetByNPCVnum); npc_vnum 0 is
    a shop only a quest opens (npc.open_shop(<vnum>)).
  * world.shop_item (shop_vnum, item_vnum, count; unique on all three): the
    goods. There is no price and no position column: the order in the window
    is the item's vnum (the ORDER BY above), each piece is put into the 5x8
    window at the first free place (shop.cpp, CGrid::FindBlank by its size);
    what does not fit is left out (syserr "not enough shop window"), and
    more than 40 lines overflow the db core's table (tables.h TShopTable
    items[SHOP_HOST_ITEM_MAX_NUM = 40]) - both are refused here.
  * The price is the item's own world.item_proto.gold x count (shop.cpp;
    with ITEM_FLAG_COUNT_PER_1GOLD it is count / gold) - one price for every
    shop that sells the item, also shown by the client's tooltip, so it is
    an item_proto edit (pending like every other, in the client data too).
    A player of another kingdom pays x3.
  * What the NPC pays back (shop_manager.cpp Sell): shop_buy_price x count,
    / 5, minus a tax of 3-20 % (+3..8 % for items over level 40). A buy-back
    above what the shop asks would be free yang - refused here.

Bots (linux-port/overlays/playerbot): they buy weapons and armour from the
shops of NPC 9001, 9002 and 9003 at the shop's price (playerbot_gear.h
CollectPlayerBotProperMerchantWeapons, GetPlayerBotMerchantWeaponCeiling),
and sell their junk for shop_buy_price / 5 (playerbot_economy.h
GetPlayerBotJunkSalePrice, gold when it is 0). Their market prices are
their own table (playerbot_price_tables.h) and do not follow gold.

apply.sh (linux-port/docker/mariadb/playerbot) rewrites some shop rows at
every start - BOOT_* below, checked against it by test_dbeditor_shops.py.

Every change goes through common_items (history, undo, pending, "Zastosuj").
"""
import re

from flask import abort, current_app, flash, redirect, render_template, request, url_for

from dbeditor import common_items as common
from dbeditor import protected

SHOP = "world.shop"
GOODS = "world.shop_item"
PROTO = "world.item_proto"

GRID_W, GRID_H = 5, 8                       # common/length.h SHOP_DEFAULT_WIDTH / HEIGHT
MAX_LINES = GRID_W * GRID_H                 # SHOP_HOST_ITEM_MAX_NUM
FLAG_COUNT_PER_1GOLD = 1 << 3               # item_length.h ITEM_FLAG_COUNT_PER_1GOLD
GOLD_MAX = 2000000000                       # what a character may carry
COUNT_MAX = 65535                           # shop_item.count smallint unsigned

# The three merchants the bots shop at (playerbot_gear.h, playerbot_economy.h).
BOT_MERCHANTS = (9001, 9002, 9003)

# Where the NPCs stand (share/locale/poland/map/*/npc.txt of the image).
_V1 = "wioski M1 (Yongan, Joan, Pyongmoo)"
_V2 = "miasta M2 (Jayang, Bokjung, Bakra)"
_CAPE = "Przylądek Smoczej Głowy"
_GUILD = "wioski gildii"
NPC_PLACES = {
    9001: [_V1, _V2, _CAPE], 9002: [_V1, _V2, _CAPE], 9003: [_V1, _V2, _CAPE, _GUILD],
    9005: [_V1, _V2, _CAPE, _GUILD], 9009: [_V1, _CAPE], 20001: [_V1, _CAPE], 20003: [_V1],
    20015: [_V2], 20042: [_V2], 20349: [_V1, _V2, _CAPE],
}

# apply.sh at every start: these items leave every shop (DELETE FROM
# world.shop_item WHERE item_vnum ...) ...
BOOT_REMOVED = [
    (72731, 72731, "Maska Sabaha (klątwa Hwang)"),
    (72735, 72735, "Maska Sabaha (klątwa Hwang)"),
    (55001, 55999, "przedmioty zwierzaków (New Pet System) – są w ItemShopie"),
]
# ... and these lines come back (INSERT IGNORE INTO world.shop_item).
BOOT_ADDED = {
    (3, 70065, 1): "Transfer bonusów kostiumu",
    (3, 70064, 20): "Zaczaruj kostium",
    (3, 70063, 20): "Transformuj kostium",
}


def boot_removed(vnum):
    for low, high, note in BOOT_REMOVED:
        if low <= int(vnum) <= high:
            return note
    return None


def _text(value):
    return common.text_of(value) or ""


# ---- reading ------------------------------------------------------------------

def load_items(rows, vnums):
    """{vnum: {vnum, name, type, subtype, size, stack, flag, gold, shop_buy_price}}."""
    wanted = sorted({int(v) for v in vnums if str(v).strip().lstrip("-").isdigit() and int(v) > 0})
    found = {}
    for start in range(0, len(wanted), 500):
        part = wanted[start:start + 500]
        marks = ",".join(["%s"] * len(part))
        for r in rows(f"SELECT vnum, CAST(locale_name AS BINARY) AS locale_name, type, subtype, size, stack, flag, "
                      f"gold, shop_buy_price FROM {PROTO} WHERE vnum IN ({marks})", part):
            found[int(r["vnum"])] = {
                "vnum": int(r["vnum"]), "name": _text(r["locale_name"]) or f"Przedmiot {r['vnum']}",
                "type": int(r.get("type") or 0), "subtype": int(r.get("subtype") or 0),
                "size": max(1, min(3, int(r.get("size") or 1))), "stack": int(r.get("stack") or 1),
                "flag": int(r.get("flag") or 0), "gold": int(r.get("gold") or 0),
                "shop_buy_price": int(r.get("shop_buy_price") or 0)}
    return found


def unknown_item(vnum):
    return {"vnum": int(vnum), "name": f"NIEZNANY PRZEDMIOT {vnum}", "type": 0, "subtype": 0, "size": 1, "stack": 1,
            "flag": 0, "gold": 0, "shop_buy_price": 0, "unknown": True}


def npc_names(rows, vnums):
    wanted = sorted({int(v) for v in vnums if int(v or 0) > 0})
    if not wanted:
        return {}
    marks = ",".join(["%s"] * len(wanted))
    try:
        found = rows(f"SELECT vnum, CAST(locale_name AS BINARY) AS locale_name FROM player.mob_proto "
                     f"WHERE vnum IN ({marks})", wanted)
    except Exception:  # a name is never worth a broken page
        return {}
    return {int(r["vnum"]): _text(r["locale_name"]) for r in found}


def shop_label(shop, names=None):
    npc = int(shop.get("npc_vnum") or 0)
    if npc:
        return (names or {}).get(npc) or f"NPC {npc}"
    return shop.get("name") or f"Sklep {shop['vnum']}"


def load_shop(rows, vnum):
    found = rows(f"SELECT vnum, CAST(name AS BINARY) AS name, npc_vnum FROM {SHOP} WHERE vnum=%s", (vnum,))
    if not found:
        return None
    shop = {"vnum": int(found[0]["vnum"]), "name": _text(found[0]["name"]), "npc_vnum": int(found[0]["npc_vnum"] or 0)}
    shop["npc_name"] = npc_names(rows, [shop["npc_vnum"]]).get(shop["npc_vnum"]) if shop["npc_vnum"] else None
    shop["label"] = shop["npc_name"] or shop_label(shop)
    shop["places"] = NPC_PLACES.get(shop["npc_vnum"], [])
    shop["bots"] = shop["npc_vnum"] in BOT_MERCHANTS
    return shop


def load_goods(rows, shop_vnum):
    """The lines in the game's order (by item vnum, then count)."""
    lines = [{"item_vnum": int(r["item_vnum"]), "count": int(r["count"])}
             for r in rows(f"SELECT shop_vnum, item_vnum, count FROM {GOODS} WHERE shop_vnum=%s "
                           "ORDER BY item_vnum, count", (shop_vnum,))]
    return lines


def other_shops(rows, vnums, shop_vnum):
    """{item vnum: [other shop vnums selling it]}."""
    wanted = sorted({int(v) for v in vnums})
    if not wanted:
        return {}
    marks = ",".join(["%s"] * len(wanted))
    out = {}
    for r in rows(f"SELECT shop_vnum, item_vnum, count FROM {GOODS} WHERE item_vnum IN ({marks})", wanted):
        if int(r["shop_vnum"]) != int(shop_vnum):
            out.setdefault(int(r["item_vnum"]), set()).add(int(r["shop_vnum"]))
    return {k: sorted(v) for k, v in out.items()}


# ---- prices ---------------------------------------------------------------------

def line_price(item, count, gold=None):
    """What the shop asks for one line (shop.cpp)."""
    gold = item["gold"] if gold is None else gold
    if item["flag"] & FLAG_COUNT_PER_1GOLD:
        return count if gold == 0 else count // gold
    return gold * count


def sell_back(item, count=1, buy=None):
    """What the NPC pays a player for count pieces (shop_manager.cpp Sell),
    at the lowest tax (3 %, an item up to level 40, under 3000 yang)."""
    buy = item["shop_buy_price"] if buy is None else buy
    if item["flag"] & FLAG_COUNT_PER_1GOLD:
        price = count if buy == 0 else count // buy
    else:
        price = buy * count
    gold_bar = 80003 <= item["vnum"] <= 80008
    if not gold_bar:
        price //= 5
    return price - price * 3 // 100


def price_problems(item, gold, buy, count=1):
    """Errors of one item's shop price / buy-back price."""
    errors = []
    name = f"{item['name']} ({item['vnum']})"
    if item["flag"] & FLAG_COUNT_PER_1GOLD:
        return errors
    if gold <= 0:
        errors.append(f"{name}: cena 0 – gra nie pozwala kupić przedmiotu za 0 Yang. Podaj cenę.")
    elif sell_back(item, 1, buy) > gold:
        errors.append(f"{name}: kupiony za {gold:,} Yang odsprzedałby się za {sell_back(item, 1, buy):,} Yang – "
                      "darmowy Yang. Obniż cenę odkupu (gracz dostaje 1/5 jej minus podatek) albo podnieś cenę."
                      .replace(",", " "))
    if gold * count > GOLD_MAX:
        errors.append(f"{name} ×{count}: {gold * count:,} Yang – więcej, niż postać może mieć (2 mld).".replace(",", " "))
    return errors


# ---- the window -------------------------------------------------------------------

def layout(lines, items):
    """Place the lines like shop.cpp does (CGrid::FindBlank(1, size), row by
    row). Each line gets cell/row/col, or fits=False when the window is full."""
    taken = [[False] * GRID_W for _ in range(GRID_H)]
    out = []
    for line in lines:
        item = items.get(line["item_vnum"])
        size = item["size"] if item else 1
        spot = None
        for row in range(GRID_H - size + 1):
            for col in range(GRID_W):
                if all(not taken[row + k][col] for k in range(size)):
                    spot = (row, col)
                    break
            if spot:
                break
        placed = dict(line, size=size, fits=spot is not None)
        if spot:
            for k in range(size):
                taken[spot[0] + k][spot[1]] = True
            placed.update(row=spot[0] + 1, col=spot[1] + 1)
        out.append(placed)
    return out


def check_goods(shop_vnum, lines, items, before=None):
    """Errors and warnings of the shop's final lines. A window that already
    overflowed (before: the lines as they were) only may not get worse."""
    errors, warnings = [], []
    seen = set()
    for line in lines:
        key = (line["item_vnum"], line["count"])
        if key in seen:
            errors.append(f"{items[line['item_vnum']]['name']} ×{line['count']} jest w sklepie dwa razy.")
        seen.add(key)
    if len(lines) > MAX_LINES:
        errors.append(f"Sklep może mieć najwyżej {MAX_LINES} pozycji (okno 5×8) – jest {len(lines)}. "
                      "Więcej wywraca grę przy starcie.")
    else:
        missing = [p for p in layout(lines, items) if not p["fits"]]
        was = len([p for p in layout(before, items) if not p["fits"]]) if before else 0
        if missing and len(missing) <= was:
            warnings.append("Już wcześniej nie mieściło się w oknie (gra to pomija): "
                            + ", ".join(f"{items[p['item_vnum']]['name']} ×{p['count']}" for p in missing[:6]) + ".")
        elif missing:
            errors.append("Nie mieszczą się w oknie sklepu (5×8 pól, broń i zbroje zajmują 2–3 pola): "
                          + ", ".join(f"{items[p['item_vnum']]['name']} ×{p['count']}" for p in missing[:6])
                          + ". Gra by je pominęła – usuń coś.")
    for line in lines:
        note = boot_removed(line["item_vnum"])
        if note:
            errors.append(f"{items[line['item_vnum']]['name']}: skrypt startowy (apply.sh) usuwa go ze wszystkich "
                          f"sklepów przy każdym starcie ({note}).")
    for key, note in BOOT_ADDED.items():
        if key[0] == int(shop_vnum) and (key[1], key[2]) not in seen:
            warnings.append(f"{note} ×{key[2]} wróci do sklepu przy najbliższym starcie – dodaje go skrypt startowy (apply.sh).")
    return errors, warnings


def parse_factor(text):
    try:
        value = float(str(text or "").strip().replace(",", ".").replace("×", "").replace("x", ""))
    except ValueError:
        return None
    return value if 0.05 <= value <= 20 else None


def endpoints():
    """The installed endpoints - a link to another part only when it is there."""
    return {rule.endpoint for rule in current_app.url_map.iter_rules()}


def history_formatter(col, value):
    if col == common.ROW_COL:
        try:
            import json
            row = json.loads(value)
            return f"w ofercie ×{row['count']}"
        except (ValueError, KeyError, TypeError):
            return None
    return None


# ---- the pages ----------------------------------------------------------------------

def install(bp, ctx):
    import dbeditor

    common.init(ctx)
    common.register_table(GOODS, ("shop_vnum", "item_vnum", "count"), {}, "Towar w sklepie NPC",
                          formatter=history_formatter, row_cols=("shop_vnum", "item_vnum", "count"),
                          row_label="towar w sklepie")
    if PROTO not in common.TABLES:  # items.py registers it with every column; enough for prices otherwise
        common.register_table(PROTO, "vnum", {
            "gold": {"kind": "int", "min": -2147483648, "max": 2147483647, "label": "Cena w sklepie NPC (gold)"},
            "shop_buy_price": {"kind": "int", "min": 0, "max": 4294967295, "label": "Cena odkupu przez NPC (shop_buy_price)"},
        }, "Przedmiot")
    common.install_history(bp, ctx)
    login_required = ctx["login_required"]

    def rows(sql, params=()):
        return common.ctx()["rows"](sql, params)

    def page_context():
        try:
            return common.pending_context()
        except Exception:
            return {"pending_total": 0}

    @bp.route("/shops")
    @login_required
    def shops():
        query = (request.args.get("q") or "").strip()
        found = rows(f"SELECT vnum, CAST(name AS BINARY) AS name, npc_vnum FROM {SHOP} ORDER BY vnum")
        all_lines = rows(f"SELECT shop_vnum, item_vnum, count FROM {GOODS} ORDER BY shop_vnum, item_vnum, count")
        by_shop = {}
        for line in all_lines:
            by_shop.setdefault(int(line["shop_vnum"]), []).append(int(line["item_vnum"]))
        names = npc_names(rows, [r["npc_vnum"] for r in found])
        hits, matched = [], None
        if query:
            if query.isdigit():
                matched = {int(query)}
            elif len(query) >= 2:
                matched = {int(r["vnum"]) for r in rows(
                    f"SELECT vnum FROM {PROTO} WHERE locale_name LIKE %s LIMIT 300", (f"%{query}%",))}
            else:
                matched = set()
        preview = load_items(rows, {v for vnums in by_shop.values() for v in vnums[:8]} | (matched or set()))
        listing = {"npc": [], "quest": []}
        for r in found:
            shop = {"vnum": int(r["vnum"]), "name": _text(r["name"]), "npc_vnum": int(r["npc_vnum"] or 0)}
            shop["label"] = shop_label(shop, names)
            shop["places"] = NPC_PLACES.get(shop["npc_vnum"], [])
            shop["bots"] = shop["npc_vnum"] in BOT_MERCHANTS
            vnums = by_shop.get(shop["vnum"], [])
            shop["goods"] = len(vnums)
            shop["preview"] = [preview[v] for v in dict.fromkeys(vnums) if v in preview][:8]
            if matched is not None:
                shop["hits"] = [preview[v] for v in dict.fromkeys(vnums) if v in matched and v in preview]
                if not shop["hits"]:
                    continue
                hits.append(shop)
            listing["npc" if shop["npc_vnum"] else "quest"].append(shop)
        return render_template("dbeditor/shops.html", listing=listing, query=query, searched=matched is not None,
                               hits=hits, **page_context())

    def view_context(shop, lines, raw=None, problems=None):
        items = load_items(rows, [l["item_vnum"] for l in lines])
        for line in lines:
            items.setdefault(line["item_vnum"], unknown_item(line["item_vnum"]))
        others = other_shops(rows, items, shop["vnum"])
        placed = layout(lines, items)
        first = set()
        view = []
        for line in placed:
            item = items[line["item_vnum"]]
            entry = dict(line, item=item, price=line_price(item, line["count"]), sell=sell_back(item, line["count"]),
                         first=line["item_vnum"] not in first, others=others.get(line["item_vnum"], []),
                         boot_added=(shop["vnum"], line["item_vnum"], line["count"]) in BOOT_ADDED,
                         boot_cols=[c for c in protected.overwritten_cols(item["vnum"], item.get("type"))
                                    if c in ("gold", "shop_buy_price")],
                         boot_partial=[r["note"] for r in protected.rules_for(item["vnum"], item.get("type"))
                                       if r["kind"] != "always" and {"gold", "shop_buy_price"} & set(r["cols"])])
            first.add(line["item_vnum"])
            view.append(entry)
        grid = [p for p in placed if p["fits"]]
        try:
            history = [b for b in common.history_batches(60, GOODS) if any(
                str(r["row_key"]).startswith(f"{shop['vnum']}:") for r in b["rows"])][:8]
        except Exception:
            history = []
        return dict(shop=shop, lines=view, grid=grid, grid_w=GRID_W, grid_h=GRID_H, max_lines=MAX_LINES,
                    raw=raw or {}, problems=problems or [], history=history, items=items,
                    boot_added=[dict(zip(("shop", "item", "count"), k), note=n) for k, n in BOOT_ADDED.items()
                                if k[0] == shop["vnum"]],
                    url_map_endpoints=endpoints(),
                    dbe_csrf=common.csrf_token(), **common.template_helpers(), **page_context())

    @bp.route("/shops/<int:vnum>", methods=["GET", "POST"])
    @login_required
    def shop_edit(vnum):
        shop = load_shop(rows, vnum)
        if not shop:
            abort(404)
        lines = load_goods(rows, vnum)
        if request.method == "POST":
            if not common.check_csrf():
                return redirect(url_for("dbeditor.shop_edit", vnum=vnum))
            action = request.form.get("action", "save")
            if action == "mass":
                return mass_prices(shop, lines)
            return save_shop(shop, lines)
        return render_template("dbeditor/shops_edit.html", **view_context(shop, lines))

    def save_shop(shop, lines):
        form = request.form
        errors, warnings = [], []
        current = {(l["item_vnum"], l["count"]) for l in lines}
        deletes, inserts, final = [], [], []
        new_prices = {}
        try:
            n_rows = int(form.get("rows_n", "0"))
            n_new = int(form.get("new_n", "0"))
        except ValueError:
            n_rows = n_new = 0
        wanted = [form.get(f"item_{i}", "") for i in range(n_rows)] + [form.get(f"new_item_{j}", "") for j in range(n_new)]
        items = load_items(rows, [w for w in wanted if str(w).strip().isdigit()] + [l["item_vnum"] for l in lines])
        for line in lines:  # a line whose item left item_proto can still be removed
            items.setdefault(line["item_vnum"], unknown_item(line["item_vnum"]))

        def count_of(text, item, label):
            value, error = common.validate({"kind": "int", "min": 1, "max": COUNT_MAX}, text, label)
            if error:
                errors.append(error)
                return None
            limit = max(1, item["stack"])
            if value > limit:
                errors.append(f"{label}: {item['name']} łączy się najwyżej po {limit} szt. – podano {value}.")
                return None
            return value

        kept = set()
        for i in range(n_rows):
            try:
                vnum, old = int(form.get(f"item_{i}", "")), int(form.get(f"count0_{i}", ""))
            except ValueError:
                continue
            if (vnum, old) not in current:
                errors.append("Sklep zmienił się w międzyczasie (ktoś inny go zapisał) – odśwież stronę.")
                break
            kept.add((vnum, old))
            item = items[vnum]
            if form.get(f"del_{i}") == "1":
                deletes.append({"shop_vnum": shop["vnum"], "item_vnum": vnum, "count": old})
                continue
            text = str(form.get(f"count_{i}", old)).strip()
            count = old if text == str(old) else count_of(text, item, f"{item['name']} – ilość")
            if count is None:
                count = old
            final.append({"item_vnum": vnum, "count": count})
            if count != old:
                deletes.append({"shop_vnum": shop["vnum"], "item_vnum": vnum, "count": old})
                inserts.append({"shop_vnum": shop["vnum"], "item_vnum": vnum, "count": count})
        if not errors and kept != current:
            # A line the form did not carry (another tab added it): kept as it is.
            final += [{"item_vnum": v, "count": c} for v, c in current - kept]
        for j in range(n_new):
            text = str(form.get(f"new_item_{j}", "")).strip()
            if not text:
                continue
            if not text.isdigit() or int(text) not in items or items[int(text)].get("unknown"):
                errors.append(f"Nowy towar: przedmiot „{text}” nie istnieje (podaj VNUM z listy).")
                continue
            item = items[int(text)]
            count = count_of(form.get(f"new_count_{j}", "1") or "1", item, f"Nowy towar {item['name']} – ilość")
            if count is None:
                continue
            final.append({"item_vnum": item["vnum"], "count": count})
            inserts.append({"shop_vnum": shop["vnum"], "item_vnum": item["vnum"], "count": count})
            gold_text = str(form.get(f"new_gold_{j}", "")).strip()
            if gold_text:
                value, error = common.validate({"kind": "int", "min": 0, "max": GOLD_MAX}, gold_text,
                                               f"Nowy towar {item['name']} – cena")
                if error:
                    errors.append(error)
                else:
                    new_prices.setdefault(item["vnum"], {})["gold"] = value
        # Prices: one per item (item_proto), the inputs of the item's first line.
        for vnum in sorted({l["item_vnum"] for l in final}):
            item = items[vnum]
            for col, field, low, high, label in (("gold", f"gold_{vnum}", 0, GOLD_MAX, "cena w sklepie"),
                                                 ("shop_buy_price", f"buy_{vnum}", 0, 4294967295, "cena odkupu")):
                if field not in form:
                    continue
                value, error = common.validate({"kind": "int", "min": low, "max": high}, form.get(field),
                                               f"{item['name']} – {label}")
                if error:
                    errors.append(error)
                elif value != item[col]:
                    new_prices.setdefault(vnum, {}).setdefault(col, value)
        ordered = sorted(final, key=lambda l: (l["item_vnum"], l["count"]))
        if not errors:
            problems, warnings = check_goods(shop["vnum"], ordered, items, before=lines)
            errors += problems
            for vnum in sorted({l["item_vnum"] for l in ordered}):
                item = items[vnum]
                gold = new_prices.get(vnum, {}).get("gold", item["gold"])
                buy = new_prices.get(vnum, {}).get("shop_buy_price", item["shop_buy_price"])
                if vnum in new_prices or any(i["item_vnum"] == vnum for i in inserts):
                    top = max(l["count"] for l in ordered if l["item_vnum"] == vnum)
                    errors += price_problems(item, gold, buy, top)
        if errors:
            for error in errors[:8]:
                flash(error, "error")
            flash("Nic nie zapisano – popraw zaznaczone pola.", "error")
            raw = {k: v for k, v in form.items()}
            return render_template("dbeditor/shops_edit.html", **view_context(shop, lines, raw=raw, problems=errors))
        if not deletes and not inserts and not new_prices:
            flash("Brak zmian do zapisania.", "success")
            return redirect(url_for("dbeditor.shop_edit", vnum=shop["vnum"]))
        names = {v: i["name"] for v, i in items.items()}

        def label_goods(key):
            _s, item_vnum, count = (int(x) for x in str(key).split(":"))
            return f"{shop['label']} · {names.get(item_vnum, item_vnum)} ×{count}"

        try:
            batch, done = common.write_rows(GOODS, inserts=inserts, deletes=deletes,
                                            note=f"Sklep NPC: {shop['label']}", label_of=label_goods)
            saved = []
            if new_prices:
                _b, saved = common.save_rows(PROTO, sorted(new_prices.items()), note=f"Ceny w sklepie NPC: {shop['label']}",
                                             label_of=lambda k: names.get(int(k), ""), batch=batch)
        except Exception as exc:
            flash(f"Nie udało się zapisać: {exc}", "error")
            return redirect(url_for("dbeditor.shop_edit", vnum=shop["vnum"]))
        report_price_side_effects(shop, saved, items)
        for warning in warnings:
            flash(warning, "warning")
        added = sum(1 for _k, op in done if op == "insert")
        removed = sum(1 for _k, op in done if op == "delete")
        flash(f"Zapisano: dodane pozycje {added}, usunięte {removed}, zmienione ceny {len(saved)}. "
              "Zmiany czekają na zastosowanie (restart gry).", "success")
        return redirect(url_for("dbeditor.shop_edit", vnum=shop["vnum"]))

    def report_price_side_effects(shop, saved, items):
        if not saved:
            return
        vnums = sorted({int(k) for k, *_r in saved})
        others = other_shops(rows, vnums, shop["vnum"])
        shared = [items[v]["name"] for v in vnums if others.get(v)]
        if shared:
            flash("Cena przedmiotu jest jedna dla wszystkich sklepów – zmieniła się też w innych sklepach dla: "
                  + ", ".join(shared[:8]) + (" …" if len(shared) > 8 else "") + ".", "warning")
        lost = sorted({items[int(k)]["name"] for k, col, *_r in saved
                       if col in protected.overwritten_cols(int(k), items[int(k)].get("type"))})
        if lost:
            flash(f"{protected.WARNING}: {', '.join(lost)}.", "warning")
        if shop["bots"] or any(col == "shop_buy_price" for _k, col, *_r in saved):
            flash("Boty kupują broń i zbroje u Handlarza Bronią, Handlarza Zbrojami i Handlarki Różności po tych cenach, "
                  "a śmieci sprzedają za cenę odkupu / 5 – zmiana wpływa też na ich Yang.", "warning")

    def mass_prices(shop, lines):
        factor = parse_factor(request.form.get("factor"))
        if factor is None:
            flash("Mnożnik: podaj liczbę od 0,05 do 20 (np. 0,8).", "error")
            return redirect(url_for("dbeditor.shop_edit", vnum=shop["vnum"]))
        with_buy = request.form.get("with_buy") == "1"
        items = load_items(rows, [l["item_vnum"] for l in lines])
        updates, errors = [], []
        for vnum in sorted(items):
            item = items[vnum]
            if item["flag"] & FLAG_COUNT_PER_1GOLD or item["gold"] <= 0:
                continue
            values = {"gold": max(1, int(round(item["gold"] * factor)))}
            if with_buy:
                values["shop_buy_price"] = max(0, int(round(item["shop_buy_price"] * factor)))
            top = max(l["count"] for l in lines if l["item_vnum"] == vnum)
            errors += price_problems(item, values["gold"], values.get("shop_buy_price", item["shop_buy_price"]), top)
            updates.append((vnum, values))
        if errors:
            for error in errors[:8]:
                flash(error, "error")
            flash("Nic nie zmieniono. Zaznacz „zmień też cenę odkupu” albo wybierz inny mnożnik.", "error")
            return redirect(url_for("dbeditor.shop_edit", vnum=shop["vnum"]))
        try:
            _batch, saved = common.save_rows(PROTO, updates, note=f"Ceny ×{factor:g} w sklepie: {shop['label']}",
                                             label_of=lambda k: items[int(k)]["name"])
        except Exception as exc:
            flash(f"Nie udało się zapisać: {exc}", "error")
            return redirect(url_for("dbeditor.shop_edit", vnum=shop["vnum"]))
        report_price_side_effects(shop, saved, items)
        flash(f"Ceny ×{factor:g}: zmieniono {len({k for k, *_r in saved})} przedmiot(ów). "
              "Zmiany czekają na zastosowanie (restart gry).", "success")
        return redirect(url_for("dbeditor.shop_edit", vnum=shop["vnum"]))

    dbeditor.add_section("dbeditor.shops", "🛒", "Sklepy NPC",
                         "co sprzedają Handlarka, Handlarz Bronią, Rybak, Alchemik… – towary, ilości, ceny i odkup")
