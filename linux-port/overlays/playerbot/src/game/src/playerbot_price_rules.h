#ifndef PLAYERBOT_PRICE_RULES_H
#define PLAYERBOT_PRICE_RULES_H
// Iwakura's Community Patch 5 on prices, as pure arithmetic: the inflation
// compounded (point 10), what several maximal lines on one piece add (point
// 11), and which of his two kinds of bonus row prices a line (point 13). No
// engine types; unit-tested in tests/playerbot_price_rules_test.cpp. The
// engine side is ScalePlayerBotIwakuraPrice (playerbot_town.h) and
// GetPlayerBotBonusPricePercent (playerbot_bonus.h).
#include <cmath>

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

}  // namespace playerbot_price_rules

#endif
