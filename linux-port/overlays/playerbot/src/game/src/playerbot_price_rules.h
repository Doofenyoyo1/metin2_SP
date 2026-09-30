#ifndef PLAYERBOT_PRICE_RULES_H
#define PLAYERBOT_PRICE_RULES_H
// Iwakura's Community Patch 5 on prices, as pure arithmetic: the inflation
// compounded (point 10), what several maximal lines on one piece add (point
// 11), and which of his two kinds of bonus row prices a line (point 13) - and
// his answer of 28 September on the counters: the markdown of a line nobody
// buys and the markup of a kind the market keeps running out of. No engine
// types; unit-tested in tests/playerbot_price_rules_test.cpp. The engine side
// is ScalePlayerBotIwakuraPrice and GetPlayerBotListingPrice
// (playerbot_town.h), GetPlayerBotBonusPricePercent (playerbot_bonus.h) and
// UpdatePlayerBotShortageMarkups (playerbot_world_memory.h).
#include <cmath>
#include <cstdint>

namespace playerbot_price_rules {

// Point 10: "zamiast dodawac plaskie +5% do mnoznika za kazde wygenerowane
// 2,5 mld Yang, system powinien mnozyc obecne wartosci przez 1.05 (x1.05) za
// kazde kolejne 2,5 mld Yang". The multiplier is kept in ten-thousandths, so
// one step is exactly Community Patch 2's +5% and the caller's integer
// arithmetic keeps its shape: 10 000 is no inflation, 10 500 one step, 11 025
// two, 11 576 three, where adding five points a step made 11 500.
const long long INFLATION_FACTOR_ONE = 10000;

// The multiplier after `steps` whole steps of `stepPercent` each, compounded,
// and never over `maxFactor`: a power of a step count outgrows any integer
// long before it outgrows a world's purses, so the ceiling is arithmetic and
// nothing else.
inline long long InflationFactor(long long steps, int stepPercent, long long maxFactor)
{
	if (steps <= 0 || stepPercent <= 0)
		return INFLATION_FACTOR_ONE;
	const double factor = (double)INFLATION_FACTOR_ONE *
			std::pow(1.0 + (double)stepPercent / 100.0, (double)steps);
	if (!(factor < (double)maxFactor))
		return maxFactor;
	return (long long)(factor + 0.5);
}

// Point 11: "Jezeli przedmiot posiada wiecej niz jeden maksymalny bonus, jego
// cena koncowa jest dodatkowo mnozona" - two lines at their top x1.7, three
// x2.5, four x4.0, over what the lines already asked on their own. A fifth,
// which only the blessing marble adds, asks what four do: his list stops at
// four. Hundredths, by the count.
const int MAX_LINES_COUNTED = 4;
const int MAX_LINES_PERCENT[MAX_LINES_COUNTED + 1] = { 100, 100, 170, 250, 400 };

inline int MaxLinesPercent(int maxLines)
{
	if (maxLines < 0)
		maxLines = 0;
	if (maxLines > MAX_LINES_COUNTED)
		maxLines = MAX_LINES_COUNTED;
	return MAX_LINES_PERCENT[maxLines];
}

// "Srednie obrazenia na poziomie 40% lub wyzszym (40+) sa zawsze traktowane i
// liczone jako maksymalny bonus": a weapon's average damage counts as a line
// at its top from forty, though the tiers of his sheet run to sixty.
const long AVERAGE_DAMAGE_MAX_FROM = 40;

// One of his bonus rows for a line: what it multiplies by at the line's top
// and at any other value (hundredths), and the top it names itself - 0 for
// the rows of his first sheet, whose top is whatever this world's table rolls.
struct BonusRow
{
	int maxPct;
	int otherPct;
	long top;
};

// Whether a line is at its top: what this world's item_attr rolls for it on
// the piece (tableMax, 0 when the table has no such line), or the top his
// point-13 row names for a line only upstream's 2.2.31 table rolled (writtenTop, 0
// when no row names one). A line over the table's top - regeneration at 20 or
// 30, rolled under 2.2.31, where this table stops at 12 - is at it.
inline bool IsMaxLine(long value, long tableMax, long writtenTop)
{
	if (value <= 0)
		return false;
	return (tableMax > 0 && value >= tableMax) || (writtenTop > 0 && value >= writtenTop);
}

// What one line asks, in hundredths, when his sheet may price it twice: the
// slot's row, judged against this world's table, and a point-13 row with a top
// of its own. Only regeneration has both - 1.3 | 1.1 on a helmet and a
// necklace at this table's 12, 1.5 | 1.2 at upstream's 2.2.31 table's 30. The new
// row's maximum is for a line at its own top; under it the world's row
// decides, so a 20, over this table's top, still asks that row's maximum and
// never less than the 12 under it. A line no slot row prices - the elemental
// resistances, the two chances, a mana regeneration his first sheet put at
// x1.0 - takes the new row's "any other value". 100 when no row applies.
inline int LinePercent(long value, long tableMax, const BonusRow* slotRow, const BonusRow* topRow)
{
	if (value <= 0)
		return 100;
	if (topRow && topRow->top > 0 && value >= topRow->top)
		return topRow->maxPct;
	if (slotRow)
		return (tableMax > 0 && value >= tableMax) ? slotRow->maxPct : slotRow->otherPct;
	if (topRow)
		return topRow->otherPct;
	return 100;
}

// The lines' product, one more line in: his multipliers compound, and the
// product stops at capPct (hundredths) - that ceiling is ours, not his, so a
// weapon of sixty average and thirty skill is not 2800 times its base.
inline long long CompoundLinePercent(long long product, int linePct, long long capPct)
{
	product = product * linePct / 100;
	return product > capPct ? capPct : product;
}

// What a piece asks over its base, in percent: the lines' product, then the
// maximal lines' multiplier over the whole of it, as he wrote it - on the
// final price, so over our ceiling too.
inline long long PiecePremiumPercent(long long product, int maxLines)
{
	return product * MaxLinesPercent(maxLines) / 100 - 100;
}

// His answer of 28 September to the operator's four questions on the bots'
// prices, points 4 and 5, which are one rule seen from its two sides. A line
// nobody buys comes down ten percent for every three hours it has stood, to
// forty - "To zbyt agresywne, zrobilbym 10% co 3h do max -40%", where it was
// ten every two hours to fifty. And a kind that keeps selling and keeps being
// missing from the counters goes up ten percent for every three hours of
// that, to forty: "jesli jakies przedmioty sprzedaja sie caly czas, np.
// medale konne, i ciagle ich brakuje na rynku, to cena rosnie o 10% do max
// 40% - tylko nie wiem jak dokladnie to technicznie wyliczyc". How it is
// worked out is ours, below.

// `steps` whole steps of `stepPercent` each, never over `maxPercent`.
inline int SteppedPercent(long long steps, int stepPercent, int maxPercent)
{
	if (steps <= 0 || stepPercent <= 0 || maxPercent <= 0)
		return 0;
	// A step is a percent at least, so this many steps are the ceiling
	// whatever the step, and the product below cannot overflow.
	if (steps >= maxPercent)
		return maxPercent;
	const long long percent = steps * (long long)stepPercent;
	return percent >= maxPercent ? maxPercent : (int)percent;
}

// Point 4: the markdown of a line that has stood `standingMs` unsold - a step
// for every whole `stepMs`, none before the first.
inline int UnsoldMarkdownPercent(uint32_t standingMs, uint32_t stepMs, int stepPercent, int maxPercent)
{
	return stepMs == 0 ? 0 : SteppedPercent((long long)(standingMs / stepMs), stepPercent, maxPercent);
}

// What a line asks, in hundredths of what the market asks for it: under it
// by its own markdown, or over it by its kind's markup - never both. The two
// are opposite states of one kind: a line that has stood unsold is the market
// saying its kind is not short, whatever the kind's markup still says.
inline int ListingPercent(int markdownPercent, int markupPercent)
{
	if (markdownPercent > 0)
		return markdownPercent >= 100 ? 0 : 100 - markdownPercent;
	return markupPercent > 0 ? 100 + markupPercent : 100;
}

// A price moved by ListingPercent, in 64 bits - the stall's own arithmetic
// was 32 and wrapped for a line over forty-odd million - and never under one
// yang for a price that was one.
inline long long ApplyListingPercent(long long price, int percent)
{
	if (price <= 0 || percent == 100)
		return price;
	const long long moved = price * (long long)(percent < 0 ? 0 : percent) / 100;
	return moved < 1 ? 1 : moved;
}

// Point 5, as a window per kind. The evidence of one step is gathered for
// windowMs (three hours, the markdown's step) and then judged whole, so a
// line that flickers on and off the counters between two looks is neither a
// shortage nor its end:
//
//   - missing: the ledger found none of the kind on any counter at
//     missingPercent of its looks in the window or more ("ciagle ich
//     brakuje");
//   - selling: at least minLinesSold lines of it were sold in the window
//     ("sprzedaja sie caly czas").
//
// Missing and selling is a step up, to maxPercent. Stock that stood on the
// counters for more of the window than that is a step down, to nothing:
// there is stock and it is not selling out. Missing without the sales is no
// evidence either way - nothing to buy is nothing sold - and the kind keeps
// its markup; whatever is put up next either sells out or stands, and that
// window decides. A kind at no markup whose window did not raise it is let go,
// and its next sale opens a new window.
struct ShortageRules
{
	uint32_t windowMs;
	int stepPercent;
	int maxPercent;
	uint32_t minLinesSold;
	int missingPercent;
};

struct ShortageState
{
	bool watching;
	uint32_t windowStart;
	uint32_t looks;
	uint32_t missingLooks;
	uint32_t linesSold;
	int markupPercent;
	ShortageState()
		: watching(false), windowStart(0), looks(0), missingLooks(0), linesSold(0),
		  markupPercent(0)
	{
	}
};

enum ShortageVerdict
{
	SHORTAGE_OPEN,  // the window is still gathering
	SHORTAGE_HOLD,  // missing, and too little sold to say: the markup stays
	SHORTAGE_UP,    // missing and selling: a step up
	SHORTAGE_DOWN,  // stock stood on the counters: a step down
};

// What a closed window was, for the line that says why a markup moved.
struct ShortageJudgement
{
	ShortageVerdict verdict;
	uint32_t looks;
	uint32_t missingLooks;
	uint32_t linesSold;
	int markupBefore;
	int markupAfter;
};

inline ShortageVerdict JudgeShortageWindow(uint32_t looks, uint32_t missingLooks, uint32_t linesSold,
		const ShortageRules& rules)
{
	if (looks == 0)
		return SHORTAGE_HOLD;
	const uint64_t missingPercent = (uint64_t)(rules.missingPercent < 0 ? 0 : rules.missingPercent);
	if ((uint64_t)missingLooks * 100u < (uint64_t)looks * missingPercent)
		return SHORTAGE_DOWN;
	return linesSold >= rules.minLinesSold ? SHORTAGE_UP : SHORTAGE_HOLD;
}

// Lines of the kind sold since the last look. A kind nobody was watching is
// watched from now: its window opens with the sale.
inline void NoteShortageSale(ShortageState& state, uint32_t now, uint32_t lines)
{
	if (!state.watching)
	{
		state.watching = true;
		state.windowStart = now;
		state.looks = state.missingLooks = state.linesSold = 0;
	}
	state.linesSold = lines > 0xFFFFFFFFu - state.linesSold ? 0xFFFFFFFFu : state.linesSold + lines;
}

// One look of the ledger at a watched kind: counted, and once the window has
// run its windowMs, judged and begun again. The clock is the core's
// millisecond one, so the difference is taken unsigned and a wrap costs
// nothing.
inline ShortageJudgement NoteShortageLook(ShortageState& state, uint32_t now, bool missing,
		const ShortageRules& rules)
{
	ShortageJudgement judged = { SHORTAGE_OPEN, 0, 0, 0, state.markupPercent, state.markupPercent };
	if (!state.watching)
		return judged;
	++state.looks;
	if (missing)
		++state.missingLooks;
	if ((uint32_t)(now - state.windowStart) < rules.windowMs)
		return judged;
	judged.verdict = JudgeShortageWindow(state.looks, state.missingLooks, state.linesSold, rules);
	judged.looks = state.looks;
	judged.missingLooks = state.missingLooks;
	judged.linesSold = state.linesSold;
	if (judged.verdict == SHORTAGE_UP)
		state.markupPercent = state.markupPercent + rules.stepPercent > rules.maxPercent
				? rules.maxPercent : state.markupPercent + rules.stepPercent;
	else if (judged.verdict == SHORTAGE_DOWN)
		state.markupPercent = state.markupPercent - rules.stepPercent < 0
				? 0 : state.markupPercent - rules.stepPercent;
	judged.markupAfter = state.markupPercent;
	state.windowStart = now;
	state.looks = state.missingLooks = state.linesSold = 0;
	if (state.markupPercent <= 0)
	{
		state.markupPercent = 0;
		state.watching = false;
	}
	return judged;
}

}  // namespace playerbot_price_rules

#endif
