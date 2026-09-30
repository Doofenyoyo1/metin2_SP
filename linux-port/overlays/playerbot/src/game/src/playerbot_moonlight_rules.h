#ifndef PLAYERBOT_MOONLIGHT_RULES_H
#define PLAYERBOT_MOONLIGHT_RULES_H
// The Moonlight chest (Szkatulka Blasku Ksiezyca, 50011) and the bonus items
// that come out of it, as pure policy: which bot keeps a chest closed for its
// counter, what a chest is worth by what it holds, the floor no markdown takes
// a chest or a bonus item under, and how many bonus items a bag keeps back from
// a counter. No engine types; unit-tested in
// tests/playerbot_moonlight_rules_test.cpp. The engine's half is the chest pass
// (playerbot_consumables.h), the asking price and the floor of every counter
// line (playerbot_town.h, playerbot_offline_shop.h) and the bonus keep
// (playerbot_bonus.h).
//
// blipu, 28 September, on a hard world (the operator's answer: "A tak"): half
// an hour of a chest window, and the bots' counters held chests a player could
// buy up for next to nothing and bonus a whole set out of - "fajnie zeby boty
// faktycznie to uzywaly zamiast wystawiac za grosze na rynek". The change and
// add stones themselves never reach a counter: 71084 and 71085 carry
// ANTI_MYSHOP (and ANTI_GIVE) on both engines, for a player as much as for a
// bot. The chest carries no antiflag at all, so it was the one way out, and it
// asked Iwakura's hundred thousand for a draw that is a stone of his two
// million one time in four: 534 940 by his own sheet, at a yang rate of a
// hundred, for the group this project ships.
#include <climits>

namespace playerbot_moonlight_rules {

// One line of a special item group as the engine keeps it
// (CSpecialItemGroup::m_vecProbs and m_vecItems): the draw's running total up
// to and including this line, how many units the line hands out, and what one
// of them is worth.
struct TGroupLine
{
	long long cumulative;
	long long count;
	long long unit;
};

// Saturating, because an operator writes the group (shareify.py takes a
// special_item_group.moonlight.custom.txt over ours) and a floor that wrapped
// round would be pennies again.
inline long long SatMul(long long a, long long b)
{
	if (a <= 0 || b <= 0)
		return 0;
	return a > LLONG_MAX / b ? LLONG_MAX : a * b;
}

inline long long SatAdd(long long a, long long b)
{
	if (b <= 0)
		return a;
	return a > LLONG_MAX - b ? LLONG_MAX : a + b;
}

// What one opening hands out on average, by the worth of a unit of each line.
// A line's own chance is its step in the running total, as the engine reads
// it: a PCT group rolls every line against its step in a hundred
// (GetMultiIndex, so a step over a hundred is a certainty), any other draws
// one line out of the whole total (GetOneIndex). A step of nothing or less is
// no line - AddItem never stores one, and a total that went backwards is not a
// chance of anything.
inline long long ExpectedOpeningValue(const TGroupLine* lines, int count, bool everyLine)
{
	if (!lines || count <= 0)
		return 0;
	long long weighed = 0;
	long long total = 0;
	long long previous = 0;
	for (int i = 0; i < count; ++i)
	{
		const long long step = lines[i].cumulative - previous;
		previous = lines[i].cumulative;
		if (step <= 0)
			continue;
		const long long chance = everyLine && step > 100 ? 100 : step;
		total = SatAdd(total, chance);
		weighed = SatAdd(weighed, SatMul(SatMul(lines[i].count, lines[i].unit), chance));
	}
	if (everyLine)
		return weighed / 100;
	return total > 0 ? weighed / total : 0;
}

// The least a chest asks on any counter: floorPercent of its worth. Nothing
// when the worth is unknown - the chest keeps its sheet price then, and no
// floor is better than a made-up one.
inline long long ChestFloor(long long worth, int floorPercent)
{
	if (worth <= 0 || floorPercent <= 0)
		return 0;
	return SatMul(worth, floorPercent) / 100;
}

// Where a chest's asking price starts: Iwakura's number for it, or its worth
// by what it holds, whichever is more. His hundred thousand fits a chest of
// the stock group's sort - kingdom languages, a fugitive's cape, Lucy's ring -
// and not the one this world drops, which holds his two-million stones.
inline long long ChestAskingBase(long long sheetPrice, long long worth)
{
	return sheetPrice > worth ? sheetPrice : worth;
}

// Whether a Moonlight chest stays closed for its keeper's counter. Only a
// dropper keeps any - a drop character's counter is its trade (the operator, 15
// September: "dodaj im mozliwosc podnoszenia tego i dawania na sklep") - and
// it opens what is over its hold. Everybody else opens every chest it gets,
// the resource trader included: its one bot in five used to hold six for its
// counter, and with the dropper's those were the chests of blipu's report.
inline bool KeepsChestClosed(bool dropper, int chestsHeld, int dropperHold)
{
	return dropper && chestsHeld <= dropperHold;
}

// A bonus item's kind, which is what its floor is read by: a green stone does
// on the weapon or body armour of forty or less it fits exactly what the
// ordinary stone of its kind does, the level-30 weapons the market lives on
// included, so it is floored as that kind.
enum EBonusKind { BONUS_NONE, BONUS_CHANGE, BONUS_ADD, BONUS_MARBLE };

// The least a bonus item asks, a unit: the sheet's price of its kind
// (Zaczarowanie Przedmiotu two million, Wzmocnienie Przedmiotu one million
// nine hundred thousand, through the yang curve), and for the marble what a
// bot pays to make one - a hundred of the Alchemist's dust - since his sheet
// does not price it.
inline long long BonusGoodsFloorUnit(EBonusKind kind, long long changeUnit, long long addUnit,
		long long marbleUnit)
{
	switch (kind)
	{
		case BONUS_CHANGE: return changeUnit > 0 ? changeUnit : 0;
		case BONUS_ADD: return addUnit > 0 ? addUnit : 0;
		case BONUS_MARBLE: return marbleUnit > 0 ? marbleUnit : 0;
		default: return 0;
	}
}

// How many bonus items of one kind a bag keeps back from a counter. Every one
// while a piece of the bot's own could take one now - the bonus pass spends it
// within minutes, and a counter is for what a bot cannot use. None of a green
// one past its band (Community Patch 5, point 9: they were held "az do
// poznych faz gry"). The reserve otherwise, for the next piece.
const int KEEP_ALL = 1 << 20;

inline int BonusGoodsKeep(bool spendableNow, bool greenPastBand, int reserve)
{
	if (spendableNow)
		return KEEP_ALL;
	if (greenPastBand)
		return 0;
	return reserve > 0 ? reserve : 0;
}

// A counter line that asks less than its floor, put right at its keeper's
// next visit. A floor of nothing is no floor.
inline bool UnderFloor(long long price, long long floor)
{
	return floor > 0 && price < floor;
}

} // namespace playerbot_moonlight_rules

#endif
