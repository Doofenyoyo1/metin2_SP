#ifndef PLAYERBOT_STALL_RULES_H
#define PLAYERBOT_STALL_RULES_H
// What a counter may take out of a kind the bot keeps by count - a skill book
// of its own skill, the soul stone - and how big one line of anything is
// (Iwakura's Patch 4), as pure arithmetic. No engine types; unit-tested in
// tests/playerbot_stall_rules_test.cpp, which drives these against a bag laid
// out the way the engine lays one out.
//
// A kind is counted over the whole bag, never by the cells in front of one
// stack. Counting only the cells in front kept a bot's one stack whole
// whatever it held: a single stack of ten soul stones against a keep of three
// was never goods, and the skill books were counted by row, so "keep twelve"
// kept twelve stacks of up to ten. The keep is also the buyer's need
// (GetPlayerBotProgressionNeed), so a counter never sells what its keeper
// would walk to the market to buy back.
namespace playerbot_stall_rules {

// Units of a kind over its keep.
inline int SpareUnits(int kindTotal, int keep)
{
	return kindTotal > keep ? kindTotal - keep : 0;
}

// Whether a stack is goods: it holds at least one unit over the keep. The keep
// is spent on the units in the cells before it first, as the engine's own
// cell order puts them.
inline bool HoldsSpare(int unitsAhead, int stackCount, int keep)
{
	return stackCount > 0 && unitsAhead + stackCount > keep;
}

// How many units of this stack make one counter line: the kind's spare, up to
// a line and up to the stack. Zero when nothing can go; the whole stack when
// the answer is its count; otherwise the rest stays in the stack it was cut
// from.
inline int LineTake(int stackCount, int kindTotal, int keep, int lineUnits)
{
	int take = SpareUnits(kindTotal, keep);
	if (take > lineUnits)
		take = lineUnits;
	if (take > stackCount)
		take = stackCount;
	return take > 0 ? take : 0;
}

// The classic stall lists a stack whole: it may, when what stays behind in the
// bag - the kind less what this counter already carries and this stack - still
// holds the keep.
inline bool MayListWhole(int kindTotal, int listedSoFar, int stackCount, int keep)
{
	return stackCount > 0 && kindTotal - listedSoFar - stackCount >= keep;
}

// Iwakura's Patch 4, point 3: a counter line cut the way a player cuts one.
// "Towary zajmuja sloty w pakietach m.in. po 50, 11, 5, 4 czy 3 sztuki. Takie
// zachowanie natychmiast odroznia je od prawdziwych graczy." Every seed below
// is a hash of the bot, the kind and the line's place on the counter, so the
// same counter is cut the same way however often it is asked.
//
// A refine material a bot holds little of goes up one or two at a time; a
// holding of MATERIAL_BIG_HOLDING or more shows MATERIAL_SMALL_LINES_WHEN_BIG
// lines of two and then lines of five.
const int MATERIAL_BIG_HOLDING = 50;
const int MATERIAL_SMALL_LINES_WHEN_BIG = 5;

inline int MaterialLineUnits(int heldUnits, int smallLinesOnCounter, unsigned seed)
{
	if (heldUnits >= MATERIAL_BIG_HOLDING)
		return smallLinesOnCounter < MATERIAL_SMALL_LINES_WHEN_BIG ? 2 : 5;
	return (seed & 1u) ? 2 : 1;
}

// A refine material's and a refine scroll's lines are one, two or five: the
// largest of those that is no more than the line wanted and no more than what
// may go. Zero when nothing may.
inline bool IsSmallGoodsLine(int count)
{
	return count == 1 || count == 2 || count == 5;
}

inline int FitSmallGoodsLine(int want, int avail)
{
	static const int sizes[] = { 5, 2, 1 };
	for (int i = 0; i < 3; ++i)
		if (sizes[i] <= want && sizes[i] <= avail)
			return sizes[i];
	return 0;
}

// A refine scroll: mostly one or two, one line in ten five ("glownie po 1 lub
// 2 sztuki, z rzadkimi przypadkami pakietow po 5").
inline int ScrollLineUnits(unsigned seed)
{
	if (seed % 10u == 0)
		return 5;
	return ((seed / 10u) & 1u) ? 2 : 1;
}

// Herbs, hay and the other goods sold by the heap: ten, twenty, fifty or two
// hundred, the largest the spare fills - one line in three the size below it,
// so a counter of herbs is not a row of equal heaps either. Zero under ten.
inline bool IsHeapLine(int count)
{
	return count == 10 || count == 20 || count == 50 || count == 200;
}

inline int HeapLineUnits(int spare, unsigned seed)
{
	static const int sizes[] = { 200, 50, 20, 10 };
	for (int i = 0; i < 4; ++i)
		if (spare >= sizes[i])
			return (i + 1 < 4 && seed % 3u == 0) ? sizes[i + 1] : sizes[i];
	return 0;
}

// Point 4, "ludzka pomylka": one listing in a thousand of a skill book or of a
// refine material put up singly asks one zero too many - up, never down.
const unsigned PRICE_SLIP_ONE_IN = 1000;
const long long PRICE_SLIP_FACTOR = 10;

inline bool PriceSlips(unsigned seed)
{
	return seed % PRICE_SLIP_ONE_IN == 0;
}

// The slipped price, or the price as it was when ten times it would pass the
// ceiling the counter can hold.
inline long long SlippedPrice(long long price, long long ceiling)
{
	if (price <= 0 || price > ceiling / PRICE_SLIP_FACTOR)
		return price;
	return price * PRICE_SLIP_FACTOR;
}

// Point 13: the mission books on one village's counters, all four kinds
// together, and where a bot's own go once that is full - its storekeeper or the
// general merchant, a coin for each.
const int MISSION_BOOK_MAP_CAP = 30;

inline bool MissionBookGoesToSafebox(unsigned seed)
{
	return (seed & 1u) == 0;
}

}
#endif
