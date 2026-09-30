#ifndef __INC_METIN2_PLAYERBOT_STALKI_RULES_H__
#define __INC_METIN2_PLAYERBOT_STALKI_RULES_H__

// "Stalki", as the players say it: the black-steel body armours of level 66
// (Zbroja z Czarnej Stali, Ubranie Czarnego Wiatru, Zbroja Plytowa Czarnej
// Magii, Czarna Szata) and the weapons of level 75 (Zatruty Miecz, Lwi Miecz,
// Skrzydla Demona Chakram, Stalowy Luk Kruka, Miecz Zalu, Bambusowy Dzwon,
// Wachlarz 8 Trygramow), as pure policy. The operator's decision of 28
// September ("Tak"): bots of 66-77 progress into them.
//
// Measured before it: this world drops them only from bosses a bot of sixty
// or seventy never meets - Azrael's casket, the Ice Witch, the Reaper's casket
// (a weapon one time in a hundred and eighty), the Setaou (0.04-0.2%) and the
// Spider Baroness, whom nothing spawned - and the armours come from Min-Sun's
// crafting window after a long quest chain. The few copies a bot did get went
// to the merchant under the junk rule's default or onto a counter, because a
// piece a bot cannot wear yet was goods by the rule that holds nothing "on a
// promise" (IsPlayerBotWearableUpgrade).
//
// So: a piece of the bot's own class and build a few levels ahead of it is
// kept (KeepsAhead) - one per slot, the best (Outranks) - and put on by the
// equipment pass the day it can be; no family ever goes to the merchant; and
// a bot in the window that holds nothing of the tier for the slot buys one off
// a counter (InBuyWindow), at no more than FAIR_PRICE_PERCENT of what the
// price sheet asks for it.
//
// No engine types. Unit-tested in tests/playerbot_stalki_rules_test.cpp; the
// engine side is playerbot_stalki.h.

namespace playerbot_stalki_rules
{
	enum EKind
	{
		KIND_NONE = 0,
		KIND_ARMOUR,
		KIND_WEAPON
	};

	// Each family by its +0 vnum; a family is its ten grades, +0 to +9, the
	// refine chain the blacksmith walks (item_proto: vnum + 1 is the next).
	const unsigned ARMOUR_FAMILIES[] = {
		11290,	// Zbroja z Czarnej Stali, warrior
		11490,	// Ubranie Czarnego Wiatru, ninja
		11690,	// Zbroja Plytowa Czarnej Magii, sura
		11890	// Czarna Szata, shaman
	};
	const unsigned WEAPON_FAMILIES[] = {
		180,	// Zatruty Miecz (warrior, ninja, sura)
		190,	// Lwi Miecz (sura)
		1130,	// Skrzydla Demona Chakram (ninja, dagger)
		2170,	// Stalowy Luk Kruka (ninja, bow)
		3160,	// Miecz Zalu (warrior, two-handed)
		5120,	// Bambusowy Dzwon (shaman, bell)
		7180	// Wachlarz 8 Trygramow (shaman, fan)
	};
	const unsigned FAMILY_GRADES = 10;

	// The level each kind's +0 asks for (item_proto's LIMIT_LEVEL). A weapon
	// asks seventy-five at every grade; an armour climbs with its plus - 66 to
	// +2, 67 at +3 and +4, 68 at +5 and +6, 69 at +7 and +8, 70 at +9 - so
	// what a piece asks is always read off the piece, and this is where the
	// tier begins.
	const int ARMOUR_LEVEL = 66;
	const int WEAPON_LEVEL = 75;

	// How far ahead of its level a bot keeps a piece it cannot wear yet. Eight
	// is the top of "a few levels" (the operator's own example): at sixty and
	// seventy a level is an hour or two of play, so a piece kept eight levels
	// ahead waits a day at most, and a bot of fifty-seven is not sitting on an
	// armour of sixty-six that a bot of sixty-six would buy today.
	const int KEEP_AHEAD_LEVELS = 8;
	// How early a bot goes to the market for one: two levels before it can
	// wear it, so the piece is in the bag when the level comes, and not the
	// eight of the keep - a purchase ties up millions, and the gear it fights
	// in now has a better claim on them.
	const int BUY_AHEAD_LEVELS = 2;
	// The most a bot pays for one, in percent of what the price sheet asks for
	// the line (GetPlayerBotShopAskingPrice): a counter's one zero too many is
	// no purchase, whatever the purse holds.
	const int FAIR_PRICE_PERCENT = 200;

	// The +0 vnum of the family `vnum` belongs to, 0 when it is none of them.
	inline unsigned FamilyOf(unsigned vnum)
	{
		const unsigned base = vnum - vnum % FAMILY_GRADES;
		for (unsigned i = 0; i < sizeof(ARMOUR_FAMILIES) / sizeof(ARMOUR_FAMILIES[0]); ++i)
			if (ARMOUR_FAMILIES[i] == base)
				return base;
		for (unsigned i = 0; i < sizeof(WEAPON_FAMILIES) / sizeof(WEAPON_FAMILIES[0]); ++i)
			if (WEAPON_FAMILIES[i] == base)
				return base;
		return 0;
	}

	inline EKind KindOf(unsigned vnum)
	{
		const unsigned family = FamilyOf(vnum);
		if (family == 0)
			return KIND_NONE;
		for (unsigned i = 0; i < sizeof(ARMOUR_FAMILIES) / sizeof(ARMOUR_FAMILIES[0]); ++i)
			if (ARMOUR_FAMILIES[i] == family)
				return KIND_ARMOUR;
		return KIND_WEAPON;
	}

	inline bool IsStalki(unsigned vnum)
	{
		return FamilyOf(vnum) != 0;
	}

	inline int KindLevel(EKind kind)
	{
		return kind == KIND_ARMOUR ? ARMOUR_LEVEL : kind == KIND_WEAPON ? WEAPON_LEVEL : 0;
	}

	// A piece of the bot's own the bot keeps for later: one it cannot wear
	// yet, at most `margin` levels over its own. A piece it can wear is the
	// equipment pass's and the anvil's (an upgrade, or a higher tier to raise),
	// and one further ahead is goods like any other.
	inline bool KeepsAhead(int botLevel, int pieceLevel, int margin)
	{
		return margin > 0 && pieceLevel > botLevel && pieceLevel - botLevel <= margin;
	}

	// Whether a bot is at the level to buy a piece of this kind: from
	// `buyAhead` levels before the kind's level, and past it for good - a bot
	// of ninety that holds nothing of the tier has nothing better on its way.
	// holdsTier: a piece of the slot of the kind's level or over, worn or in
	// the bag, which ends the want whatever it is.
	inline bool InBuyWindow(int botLevel, EKind kind, int buyAhead, bool holdsTier)
	{
		const int level = KindLevel(kind);
		return level > 0 && !holdsTier && botLevel + (buyAhead > 0 ? buyAhead : 0) >= level;
	}

	// Of two pieces a slot could keep, the one it does: the better score, the
	// lower item id breaking a tie, so the answer is the same on every pass.
	inline bool Outranks(long long scoreA, unsigned idA, long long scoreB, unsigned idB)
	{
		return scoreA > scoreB || (scoreA == scoreB && idA < idB);
	}

	// Whether a line's price is one a bot pays: `fair` is the sheet's asking
	// price of the line, 0 when there is none, and then nothing is paid.
	inline bool WithinFairPrice(long long price, long long fair, int percent)
	{
		return price > 0 && fair > 0 && percent > 0 && price <= fair * percent / 100;
	}
}

#endif
