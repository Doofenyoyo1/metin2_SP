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

// The heap cut from one stack of a kind: the size the kind's whole spare asks
// for, or - the stack being smaller than that - the largest heap the stack
// holds. Asked again of a line it cut, with the same spare and seed, it
// answers the line itself. The service visit cuts a line before the board
// opens and looks at it again when it adds it, and HeapLineUnits of the line
// alone stepped a size down one time in three: fifty cut again to twenty, or
// the line left behind in the bag (Iwakura's audit of 26 September, B01).
inline int HeapLineFromStack(int stack, int spare, unsigned seed)
{
	if (stack <= 0 || spare <= 0)
		return 0;
	const int want = HeapLineUnits(spare, seed);
	if (want <= stack)
		return want;
	static const int sizes[] = { 200, 50, 20, 10 };
	const int avail = stack < spare ? stack : spare;
	for (int i = 0; i < 4; ++i)
		if (avail >= sizes[i])
			return sizes[i];
	return 0;
}

// Point 4, "ludzka pomylka": one listing in a thousand of a skill book or of a
// refine material put up singly asks one zero too many - up, never down.
const unsigned PRICE_SLIP_ONE_IN = 1000;
const long long PRICE_SLIP_FACTOR = 10;

// What can slip: a line of one unit - "pojedynczej ksiegi lub ulepszacza", as
// Community Patch 5, point 6 restates it - of a book a skill is read from or of
// a refine material. A book went up slipped whatever the line held, and a
// refine scroll with the materials; his own Patch 4 names "ulepszacze ...
// oraz zwoje" apart, and the census of his 65 percent counts no scroll.
inline bool IsSlipKind(int count, bool book, bool refineMaterial)
{
	return count == 1 && (book || refineMaterial);
}

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

// Community Patch 5, point 6: no bot buys a slipped line - "bezwzgledny
// zakaz". The draw is the line's item id, which every core reads the same and
// no restart forgets, so a buyer anywhere knows which lines were drawn. One of
// them is a slip still standing while it asks PRICE_SLIP_SEEN_MULTIPLE times
// what the buyer would ask for the same thing. A book or a hand-priced
// material draws its price between 80 and 125 percent at every asking, so on
// one core a slip stands at 6.4 to 15.6 times the buyer's price and a line put
// back at 0.64 to 1.56 times it. Twice leaves room for the market to have
// moved in the hours between - a yang rate raised fourfold still leaves a
// slip at 2.5 - and errs towards refusing: a line wrongly refused is one line
// in a thousand a bot does without, a slip bought is the rule broken. And
// whatever its draw, no bot pays PRICE_SLIP_NEVER_MULTIPLE times its own price
// for such a line - the net under the mark, should a slip ever be made some
// other way. A person may still buy one.
const long long PRICE_SLIP_SEEN_MULTIPLE = 2;
const long long PRICE_SLIP_NEVER_MULTIPLE = 5;

inline bool IsStandingSlip(bool drawn, long long unitPrice, long long fairUnit)
{
	return drawn && fairUnit > 0 && unitPrice >= fairUnit * PRICE_SLIP_SEEN_MULTIPLE;
}

inline bool BuyerRefusesSlip(bool drawn, long long unitPrice, long long fairUnit)
{
	if (fairUnit <= 0)
		return false;
	return IsStandingSlip(drawn, unitPrice, fairUnit) || unitPrice >= fairUnit * PRICE_SLIP_NEVER_MULTIPLE;
}

// And a slip stands PRICE_SLIP_HOLD_MS at most - "maksymalnie przez 4
// godziny" - and then asks the price it meant. Due `lead` before the hold is
// out: the keeper is called that much early, the core corrects it itself at
// the end. slippedAt 0 is a slip this core did not see made (a restart, a
// keeper from the other channel): its age is not known, so it is due at once,
// which can only shorten it. The clocks are the core's milliseconds, and the
// age is their unsigned distance: a wrap between the two does not matter, and
// a slip nothing looked at for weeks still reads as old, where a signed one
// reads it as young again from 24.8 days.
const unsigned PRICE_SLIP_HOLD_MS = 4u * 60u * 60u * 1000u;

inline bool SlipDue(unsigned slippedAt, unsigned now, unsigned lead)
{
	if (slippedAt == 0)
		return true;
	const unsigned hold = lead < PRICE_SLIP_HOLD_MS ? PRICE_SLIP_HOLD_MS - lead : 0u;
	return now - slippedAt >= hold;
}

// Point 13: the mission books on one village's counters, all four kinds
// together, and where a bot's own go once that is full - its storekeeper or the
// general merchant, a coin for each.
const int MISSION_BOOK_MAP_CAP = 30;

inline bool MissionBookGoesToSafebox(unsigned seed)
{
	return (seed & 1u) == 0;
}

// Point 2, as Iwakura answered it on 26 September: whether a bot buys a piece
// off a counter for a slot it wears. +6 at least (minPlus); a grade over the
// worn piece (plusOverWorn) when it scores over it; the worn grade when it
// scores over it and its lines are worth marginPct more. The scores are the
// equipment score and its count of the rolled lines (GetPlayerBotEquipmentScore,
// GetPlayerBotItemLineScore). It asked two grades and the score's margin on
// top, which a level-34 armour - forty-seven defence and three a grade - met
// only from +6 to +9. A grade over also used to be bought for a valuable line
// the worn piece lacked, whatever it scored (R2 of Iwakura's audit): the
// equipment pass puts on nothing that scores no better than the piece worn,
// so such a piece stayed in the bag and went back onto a counter. What the
// bot buys it has to be able to wear.
inline bool BuysGearOverWorn(int offerPlus, int wornPlus, long long offerScore, long long wornScore,
		long long offerLines, long long wornLines, int minPlus, int plusOverWorn, long long marginPct)
{
	if (offerPlus < minPlus)
		return false;
	if (offerScore <= wornScore)
		return false;
	if (offerPlus < wornPlus + plusOverWorn)
		return offerPlus == wornPlus && offerLines > 0 &&
				offerLines * 100 > wornLines * (100 + marginPct);
	return true;
}

}
#endif
