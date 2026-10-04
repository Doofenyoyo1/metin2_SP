#ifndef PLAYERBOT_SALE_TAX_H
#define PLAYERBOT_SALE_TAX_H

// MT2009_PLUS_SALE_TAX_V1: the panels' "Podatek od sprzedazy miedzy graczami".
//
// A share of what a player or a bot is paid for an item that another player
// or bot bought from it never reaches the seller: it leaves the game, a yang
// sink for an operator who wants less yang in circulation (the owner, 3
// October: "pomoze to ograniczyc ilosc yang w obiegu"). The buyer pays the
// asking price as before; a sale to an NPC merchant is not touched.
//
// The figure is the SALE_TAX line of playerbot_weights.tsv - the panels'
// slider, 0-50 percent - which playerbot_config.h applies on its five-second
// reload like every other line of that file. 0, the default and what a file
// without the line means, is the world as it was.
//
// Inline and engine-free, the way playerbot_offline_policy.h is, because the
// engine translation units of server-patches/saletax read it:
//   shop.cpp             CShop::Buy - a player's or a bot's stall;
//   ikarus_shop_manager  the offline counters and the Dom Towarowy (its
//                        search window buys through them): added to the tax
//                        the db core already takes off a sale, and shown in
//                        the owner's window;
//   char_item.cpp        the tax the "open a shop" window shows.
// The db core settles accepted offers and auctions in a process of its own
// and reads the same line itself (ClientManagerIkarusShop.cpp).
//
// Every translation unit that includes this has stdafx.h (sys_log) first.

#include <ctime>

namespace playerbot_sale_tax {

const int MAX_PERCENT = 50;
// How often the summary line is written, at the most: per core, at the first
// taxed sale after the interval.
const time_t REPORT_SECONDS = 600;

// The slider's figure as the last reload of the weights file left it.
inline int percent = 0;

struct Totals {
    unsigned int sales = 0;
    long long gross = 0;
    long long burned = 0;
    time_t since = 0;
};
inline Totals totals;

inline int Percent() {
    return percent < 0 ? 0 : (percent > MAX_PERCENT ? MAX_PERCENT : percent);
}

// The yang the slider takes out of a sale of `gross`. Rounded down, so a sale
// of a few yang may go untaxed; never more than half the price.
inline long long TaxOf(long long gross) {
    return gross > 0 ? gross * Percent() / 100 : 0;
}

// Counts a taxed sale for the summary line.
inline void Note(long long gross, long long tax) {
    if (tax <= 0)
        return;
    const time_t now = time(0);
    if (!totals.since)
        totals.since = now;
    ++totals.sales;
    totals.gross += gross;
    totals.burned += tax;
    if (now - totals.since >= REPORT_SECONDS) {
        sys_log(0, "MT2009_SALE_TAX: %d%% - %u sales worth %lld yang, %lld yang taken out of the game in %ld s",
                Percent(), totals.sales, totals.gross, totals.burned, (long)(now - totals.since));
        totals = Totals();
        totals.since = now;
    }
}

// What the seller keeps of a sale of `gross`, the tax counted.
inline long long Keep(long long gross) {
    const long long tax = TaxOf(gross);
    Note(gross, tax);
    return gross - tax;
}

}  // namespace playerbot_sale_tax

#endif
