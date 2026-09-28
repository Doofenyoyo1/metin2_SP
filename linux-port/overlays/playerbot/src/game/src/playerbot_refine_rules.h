#ifndef __INC_METIN2_PLAYERBOT_REFINE_RULES_H__
#define __INC_METIN2_PLAYERBOT_REFINE_RULES_H__

// What a bot of thirty may fight with, and when the scrolls in its bag belong
// to its weapon and its armour, as pure policy: Iwakura's Community Patch 5,
// points 2 and 4, and the backup weapon of his audit's R8.
//
// Point 2, "Blokada noszenia broni na 1-10 poziom przez postacie 30+": a bot
// whose weapon burned "zadowala sie byle jaka bronia na 1. poziom +6 - i to
// pomimo posiadania Yang, a czesto nawet majac w plecaku bron na 30. poziom
// +0". The damage model is right about that sword: a refine's attack
// (value5, doubled) is added after the attack rating, so a Miecz +6 (84 of
// it) out-hits a Full Moon Sword +0 and every weapon the merchant sells at +0.
// What was wrong is that nothing asked whether a bot of seventy should be
// holding it at all - the recovery put on the first weapon in the bag and
// bought the level-one sword for a hundred yang. From
// LOW_WEAPON_BAN_FROM_LEVEL no weapon of LOW_WEAPON_MAX_LEVEL or under is
// worn, bought or kept as the backup; the bag's better weapon goes on, or one
// from the market or the weapon merchant (25 to 36 there) is bought first.
//
// The one way out (IsLowWeaponFallback) keeps a bot from standing unarmed for
// good: nothing over the line to fight with and no way to get one - too little
// yang for the cheapest weapon over the line a merchant sells its class, or a
// purchase of one refused for a reason yang does not mend (no room in the bag).
//
// Point 4, "Logika ulepszania broni": a bot with at least SCROLL_RULE_MIN_SCROLLS
// Blessing Scrolls whose weapon is for level SCROLL_RULE_MIN_LEVEL or more and
// under +SCROLL_RULE_PLUS refines it under them - "sura na 70. poziomie
// biegajacy z bronia +5, mimo 37 Zwojow" - and once that weapon stands at
// SCROLL_RULE_ARMOUR_AFTER_WEAPON_PLUS or more, "dokladnie ta sama zasada" for
// the body armour it wears. What such a step waits for in a fight is
// DecideScrollStep: the engine takes nothing off or puts nothing on within a
// second and a half of the character's own blow, and a bot that always has a
// monster never has that second and a half unless it makes it.
//
// No engine types. Unit-tested in tests/playerbot_refine_rules_test.cpp; the
// engine side is playerbot_gear.h and playerbot_economy.h.

namespace playerbot_refine_rules
{
	// Point 2: from this level of the character...
	const int LOW_WEAPON_BAN_FROM_LEVEL = 30;
	// ...no weapon for this level or under. A weapon of level nought is the
	// "bron na 1. poziom" of the report: level one is the first a character has.
	const int LOW_WEAPON_MAX_LEVEL = 10;

	inline bool IsLowWeaponFor(int botLevel, int weaponLevel)
	{
		return botLevel >= LOW_WEAPON_BAN_FROM_LEVEL && weaponLevel <= LOW_WEAPON_MAX_LEVEL;
	}

	// A weapon over the line this character may wear now.
	inline bool IsProperWeaponFor(int botLevel, int weaponLevel)
	{
		return weaponLevel <= botLevel && !IsLowWeaponFor(botLevel, weaponLevel);
	}

	// Whether the ban gives way. cheapestProperPrice is the cheapest weapon
	// over the line a village merchant sells this class at this level, 0 when
	// none does - a ban nothing can answer is no ban.
	inline bool IsLowWeaponFallback(bool holdsProperWeapon, long long gold,
			long long cheapestProperPrice, bool purchaseRefusedLately)
	{
		if (holdsProperWeapon)
			return false;
		if (purchaseRefusedLately || cheapestProperPrice <= 0)
			return true;
		return gold < cheapestProperPrice;
	}

	// Point 4.
	const int SCROLL_RULE_MIN_SCROLLS = 3;
	const int SCROLL_RULE_MIN_LEVEL = 30;
	const int SCROLL_RULE_PLUS = 7;
	// "Dokladnie ta sama zasada" for the body armour, whose ladder has no tier
	// of thirty: the level-26 plate is the armour merchant's last, so the rule
	// reaches the armour from the level-34 tier up - the drops and the counters.
	const int SCROLL_RULE_ARMOUR_MIN_LEVEL = 30;
	const int SCROLL_RULE_ARMOUR_AFTER_WEAPON_PLUS = 8;

	// The weapon in the hand under the rule. refinable: the piece has a next
	// grade; stepAllowed: the operator's SCROLL_FROM lets a scroll on this step.
	inline bool IsScrollRuleWeapon(int weaponLevel, int plus, bool refinable, bool stepAllowed, int scrolls)
	{
		return refinable && stepAllowed && weaponLevel >= SCROLL_RULE_MIN_LEVEL &&
				plus < SCROLL_RULE_PLUS && scrolls >= SCROLL_RULE_MIN_SCROLLS;
	}

	// The body armour on the back under the rule. handWeaponPlus is the plus
	// of the weapon in the hand, -1 for none.
	inline bool IsScrollRuleArmour(int armourLevel, int plus, bool refinable, bool stepAllowed, int scrolls,
			int handWeaponPlus)
	{
		return handWeaponPlus >= SCROLL_RULE_ARMOUR_AFTER_WEAPON_PLUS && refinable && stepAllowed &&
				armourLevel >= SCROLL_RULE_ARMOUR_MIN_LEVEL && plus < SCROLL_RULE_PLUS &&
				scrolls >= SCROLL_RULE_MIN_SCROLLS;
	}

	// How far a piece under the rule is refined: whatever else it aims at, at
	// least the rule's plus.
	inline int ScrollRuleTarget(int target)
	{
		return target < SCROLL_RULE_PLUS ? SCROLL_RULE_PLUS : target;
	}

	// A step the field pass has chosen, asked on every tick until it is taken.
	enum EScrollStep
	{
		SCROLL_STEP_NOW = 0,	// the equip window is open: off, under the scroll, back on
		SCROLL_STEP_WAIT,		// not yet; the bot goes on with what it does
		SCROLL_STEP_HOLD,		// not yet; the bot stops swinging so the window opens
		SCROLL_STEP_DROP		// not this time: the pass's next look chooses again
	};

	// How long a step may wait for the window, and the health under which a
	// bot does not stand still for one.
	const unsigned SCROLL_STEP_WAIT_MAX_MS = 5000;
	const int SCROLL_STEP_HOLD_MIN_HP_PERCENT = 50;

	// windowShut: the engine would refuse the re-equip now; fighting: the bot's
	// action is a fight; holdsFight: the step is the rule's and nothing else
	// forbids a pause (a person's party, a lure, a pull). A step that is not
	// the rule's is taken only in a quiet moment, as it always was.
	inline EScrollStep DecideScrollStep(bool windowShut, bool fighting, bool holdsFight,
			unsigned waitedMs, int hpPercent)
	{
		if (waitedMs > SCROLL_STEP_WAIT_MAX_MS)
			return SCROLL_STEP_DROP;
		if (fighting && !holdsFight)
			return SCROLL_STEP_DROP;
		if (!windowShut)
			return SCROLL_STEP_NOW;
		if (!fighting)
			return SCROLL_STEP_WAIT;
		return hpPercent >= SCROLL_STEP_HOLD_MIN_HP_PERCENT ? SCROLL_STEP_HOLD : SCROLL_STEP_DROP;
	}

	// R8 of Iwakura's audit: the weapon kept for the day the one in the hand
	// burns. One scoring scorePercent of the hand's, as before - and, besides,
	// a copy of the hand's own family, or at any plus a weapon of the hand's
	// level or of the best level a village merchant sells the bot
	// (merchantTopLevel, 0 when none), whichever is lower: after a burn that
	// is what the bot would be sold anyway ("zakupic nowa ... od handlarza
	// bronia", Community Patch 5, point 2). A +0 copy of a +4 merchant sword
	// never scored half of it - a refine's attack is added outside the attack
	// rating - so no bot had a backup, and the hold on the burning steps
	// (PLAYERBOT_MERCHANT_WEAPON_RISK_PLUS) was for life under level nineteen,
	// where no scroll goes; and with the level-one sword +6 no longer a
	// bot-of-thirty's backup, a hand of forty-five would have waited for a
	// scroll at every step the anvil can burn.
	inline bool IsBackupWeaponFor(long long handScore, long long spareScore, int scorePercent,
			bool sameFamily, int handLevel, int spareLevel, int merchantTopLevel)
	{
		const int replacementLevel = merchantTopLevel > 0 && merchantTopLevel < handLevel
				? merchantTopLevel : handLevel;
		return spareScore * 100 >= handScore * scorePercent || sameFamily || spareLevel >= replacementLevel;
	}
}

#endif
