#ifndef PLAYERBOT_OFFLINE_POLICY_H
#define PLAYERBOT_OFFLINE_POLICY_H
#include <cstdint>
#include <map>
#include <vector>

// Process-local request journal; no character/item pointers. Only the AI arms
// requests. Native DB handlers mark transmission and completion. Never retry a
// transmitted request on timeout: native Ikarus has no idempotency key.
namespace playerbot_offline {
enum Op { None, Create, Add, Edit, Remove, WithdrawItem, Buy };
struct Request {
    Op op = None;
    uint32_t item = 0, started = 0;
    bool sent = false, done = false, success = false, warned = false;
    uint32_t vnum = 0, count = 0, unitPrice = 0, skill = 0;
    uint8_t refine = 0;
    // A purchase of the buyer's class's level-30 weapon, charged to the
    // budget its anvil shares once the db core confirms it.
    bool level30 = false;
    // A base a gambler bought between its sessions, counted by the gambler's
    // census once the db core confirms it.
    bool gambleBase = false;
};
inline std::map<uint32_t, Request> requests;
// A line that sold while its owner was off hunting. On this engine the goods
// on a counter belong to the shop entity, not to the bag, so the classic
// stall's way of noticing a sale - one pass over the owner's own inventory -
// cannot work here; and that whole branch of ManagePlayerBotShopLifetime is
// unreachable on 2.x anyway, which is why the fast-sale memory and the
// PLAYERBOT_STALL_SOLD history row have been dead since the offline shops
// arrived (diagnosed from the binary by AkhiGubernator, 13 September). The
// native manager is the one place that knows a line has gone, so it records
// the sale here and the owner's own tick drains it.
struct SoldLine {
    uint32_t item = 0, vnum = 0, count = 0;
    long long price = 0;
};
inline std::map<uint32_t, std::vector<SoldLine> > sold;
// And every line sold, by vnum, until the market ledger takes them
// (UpdatePlayerBotShortageMarkups, once a minute): what the markup of a kind
// the market keeps running out of counts as "sells all the time" (Iwakura,
// 28 September). The db core sends a purchase to every game core of every
// channel (SendIkarusShopBuyItemPacket walks all its peers), so every core
// counts every sale of every counter in the world, a player's purchase
// included - the one buyer the ledger could never see. Bounded for a core
// whose ledger never runs.
inline std::map<uint32_t, uint32_t> soldLinesByVnum;
// An owner of 0 counts the line and records no sale: the engine passes the
// owner only on the core the owner is on (MT2009_PLUS_SALE_ONCE_V1,
// ikarus_shop_manager.cpp). Every core recorded it, and a bot that changed
// channel drained the same sale a second time where it arrived - twice in the
// log and in the panel's gear history.
inline void NoteSold(uint32_t ownerid, uint32_t item, uint32_t vnum, uint32_t count, long long price) {
    if (vnum && (soldLinesByVnum.size() < 4096 || soldLinesByVnum.count(vnum)))
        ++soldLinesByVnum[vnum];
    if (!ownerid) return;
    auto& lines = sold[ownerid];
    // An owner nobody drains - a real player, or a bot that has left the
    // world - must not grow this without bound.
    if (lines.size() >= 32) lines.erase(lines.begin());
    lines.push_back(SoldLine{item, vnum, count, price});
}
// What the bot put on the counter and when, so the drain can say what sold
// and how fast. Keyed by item id, kept in that bot's own state.
struct ListedLine {
    uint32_t vnum = 0, skill = 0, when = 0;
    uint8_t refine = 0;
    uint32_t observedSince = 0;
    // When this line's price slipped, if this core saw it slip (Community
    // Patch 5, point 6: four hours at most); 0 for a price as meant, or a
    // slip whose age nobody here knows.
    uint32_t slippedAt = 0;
};
inline bool Due(uint32_t now, uint32_t at) {
    return at == 0 || int32_t(now - at) >= 0;
}
inline bool Begin(uint32_t pid, Op op, uint32_t item, uint32_t now) {
    if (requests.count(pid)) return false;
    requests.emplace(pid, Request{op, item, now});
    return true;
}
inline void Sent(uint32_t pid, Op op, uint32_t item) {
    auto it = requests.find(pid);
    if (it != requests.end() && it->second.op == op && it->second.item == item)
        it->second.sent = true;
}
inline void Complete(uint32_t pid, Op op, uint32_t item, bool ok = true) {
    auto it = requests.find(pid);
    if (it != requests.end() && it->second.sent && it->second.op == op && it->second.item == item) {
        it->second.done = true;
        it->second.success = ok;
    }
}
inline bool EndCall(uint32_t pid) {
    auto it = requests.find(pid);
    if (it == requests.end()) return false;
    if (it->second.sent) return true;
    requests.erase(it); // synchronous refusal, no DB mutation submitted
    return false;
}
struct State {
    uint32_t nextService = 0, visitUntil = 0, nextStep = 0;
    uint32_t nextReprice = 0, repriceItem = 0;
    uint32_t nextBrowse = 0, buyOwner = 0, buyItem = 0, buyUntil = 0;
    uint32_t observedShop = 0;
    uint32_t browseOwner = 0, browseItem = 0;
    // The first village's stands, read from a second village before the
    // walk over: a cursor of their own, so the look at the far market does
    // not lose the place of the browse at the near one, and the line the
    // walk is for, which becomes the buyer's pick on arrival.
    uint32_t farBrowseOwner = 0, farBrowseItem = 0;
    uint32_t farPickOwner = 0, farPickItem = 0;
    // The buyer's pick is the one a walk over was made for, from the hand-over
    // to the purchase or the moment it is given up, which says why.
    bool farBuy = false;
    uint32_t repriceSteps = 0;
    // How many visits in a row a restock or a take-off has chained, two
    // seconds apart (PLAYERBOT_OFFLINE_RESTOCK_CHAIN).
    uint32_t chainSteps = 0;
    // Which compiled price table this shop was last priced against.
    uint32_t priceGeneration = 0;
    // The empty-hand probe of the counter, and the last line taken back to
    // wear with when, so a piece the bot will not put on is not taken back
    // and listed again every visit.
    uint32_t nextReclaimProbe = 0, lastReclaimItem = 0, lastReclaimAt = 0;
    // When the counter is next looked over for a slipped price whose time is
    // out, which calls the keeper whatever its round says.
    uint32_t nextSlipProbe = 0;
    // The line cut out of its stack before the shop board opened, for the
    // add of the same visit (BotOfflinePrepareVisitLine): item id and cell.
    uint32_t preparedItem = 0, preparedCell = 0;
    // A finished piece the market Perfectionist's anvil waits for, found on
    // a stand of the bot's map while it stood at the blacksmith: the buyer
    // takes it as its pick once the town visit has let the bot go
    // (PlayerBotFindReadyGearToBuy).
    uint32_t readyPickOwner = 0, readyPickItem = 0, readyPickUntil = 0;
    // When the keeper last stood at its shop and served it: a shop on
    // another map waits PLAYERBOT_OFFLINE_FAR_SERVICE_MIN_MS from here.
    uint32_t lastServedAt = 0;
    // When the visit under way began, and how many visits in a row ended
    // before the keeper stood at its counter (BotOfflineInterruptVisit).
    uint32_t visitStarted = 0;
    uint32_t interrupted = 0;
    std::map<uint32_t, ListedLine> listed;
    bool visiting = false;
    bool restockTurn = false;
};
    template<class Shops, class Visitor>
    unsigned BrowseLines(const Shops& shops, State& state, unsigned limit, Visitor visit) {
        unsigned checked = 0;
        size_t start = 0;
        for (size_t i = 0; i < shops.size(); ++i)
            if (shops[i].second->GetOwnerPID() == state.browseOwner) { start = i; break; }
        for (size_t n = 0; n < shops.size() && checked < limit; ++n) {
            auto shop = shops[(start+n) % shops.size()].second;
            const auto& items = shop->GetItems();
            auto it = n == 0 && shop->GetOwnerPID() == state.browseOwner
                ? items.upper_bound(state.browseItem) : items.begin();
            for (; it != items.end() && checked < limit; ++it) {
                ++checked;
                state.browseOwner = shop->GetOwnerPID();
                state.browseItem = it->first;
                visit(shop, it->first, it->second);
            }
            if (it == items.end()) {
                state.browseOwner = shops[(start+n+1) % shops.size()].second->GetOwnerPID();
                state.browseItem = 0;
            }
        }
        return checked;
    }
inline bool Fits(int cell, int height, int width, int cells) {
    return width > 0 && height > 0 && cell >= 0 && cell < cells &&
        height <= cells / width && cell + (height - 1) * width < cells;
}
// A bot's offline stand is this many pages of a person's grid: cell = page *
// width * rows + row * width + column. A person's has the first page alone,
// the one the client's owner window lays out, and the guest window shows a
// bot's pages side by side, one grid twenty columns wide (clientrootify's
// offlineshopguest.py; since client 2.0.51 - 2.0.50 showed a page at a time
// behind tabs, and the operator wanted one bigger page). Players asked for it: a bot
// cannot stand a second character's shop beside its first ("boty to nie
// ludzie wiec nie postawia sobie drugiej postaci zeby otworzyc sklepik",
// prodnathin, 28 September). The engine's add path (playerbotify's
// apply_bot_shop_two_pages) reads this same number, so the two sides cannot
// disagree.
constexpr int BOT_SHOP_PAGES = 2;
// Whether a line `height` cells tall at `cell` stands inside one page of a
// shop of `pages` pages `width` wide and `rows` tall. Never across two: the
// window draws each page in a grid of its own, side by side, and a page's
// last row was the bottom of the whole counter before there was a second.
inline bool FitsOnPage(int cell, int height, int width, int rows, int pages) {
    if (width <= 0 || rows <= 0 || pages <= 0 || height <= 0 || height > rows) return false;
    const int pageCells = width * rows;
    return cell >= 0 && cell < pageCells * pages &&
        cell % pageCells + (height - 1) * width < pageCells;
}
// Whether a line `height` tall at `cell` would cover a cell of a line already
// standing, each given as a pair (cell, height), on a grid `width` wide. A line
// takes its own column, one cell a row.
template<class Lines>
inline bool Overlaps(const Lines& lines, int cell, int height, int width) {
    if (width <= 0) return true;
    const int column = cell % width, top = cell / width;
    for (const auto& line : lines) {
        if (line.first < 0 || line.first % width != column) continue;
        const int lineTop = line.first / width;
        if (lineTop < top + height && top < lineTop + line.second) return true;
    }
    return false;
}
inline int64_t Affordable(int64_t wallet, int64_t reserve, int64_t floor) {
    return wallet > reserve + floor ? wallet - reserve - floor : 0;
}
}
#endif
