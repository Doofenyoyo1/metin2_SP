#ifndef __INC_METIN2_PLAYERBOT_STALKI_H__
#define __INC_METIN2_PLAYERBOT_STALKI_H__

// The Stalki as the bots hold them (playerbot_stalki_rules.h is the policy,
// pure and tested): which piece a bot keeps for the level it is about to
// reach, whether it is in the market for one, what it may pay, and whether
// its first village's counters hold one for it. The operator's decision of 28
// September; the Spider Baroness who drops them is the image's and the boss
// raid's (special_spawns.baroness.txt, playerbot_boss_raid.h).
//
// Who asks: the junk rule (never the merchant), the counter's scorer (the kept
// piece is no goods, any other is this market's second prize), the loot (never
// left on a floor), the purchase and its purse (playerbot_market.h), the
// offline buyer's look over the whole map (playerbot_offline_market.h), the
// trip to the first village's counters (playerbot_progression_needs.h), the
// storekeeper's box, and the two anvils that could stake or burn a kept piece
// (the gambler's and the Demon Tower's smith).
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it once, after playerbot_gear.h, whose candidate test and score it asks.

namespace
{
	// The rules' numbers under the names the fragments use.
	const int PLAYERBOT_STALKI_KEEP_AHEAD_LEVELS = playerbot_stalki_rules::KEEP_AHEAD_LEVELS;
	const int PLAYERBOT_STALKI_BUY_AHEAD_LEVELS = playerbot_stalki_rules::BUY_AHEAD_LEVELS;
	const int PLAYERBOT_STALKI_FAIR_PRICE_PERCENT = playerbot_stalki_rules::FAIR_PRICE_PERCENT;
	// What a piece a bot does not keep scores on a counter: under the level-30
	// weapons (2000), the prize of this market, over a gambler's goods
	// (PLAYERBOT_SHOP_GAMBLE_GOODS_SCORE, 1800), at any plus - and one line of
	// it carries a stall on its own (PLAYERBOT_SHOP_PRIZE_SCORE). It used to be
	// merchant scrap at +0..+3 for a weapon of the wrong class and counter goods
	// at the low-armour score for the armour.
	const int PLAYERBOT_SHOP_STALKI_SCORE = 1900;
	// How long a bot's answer to "is it in the market for one" is kept for the
	// passes that only plan with it - the trip, the reason to look: the travel
	// pass asks it on every tick of a trip, and it reads the whole bag. A
	// purchase asks afresh.
	const DWORD PLAYERBOT_STALKI_SHOPPER_CACHE_MS = 20 * 1000;

	// Defined with the town code, after this file: the price sheet's curve at
	// this world's yang rate.
	DWORD ScalePlayerBotIwakuraPrice(DWORD base);

	playerbot_stalki_rules::EKind GetPlayerBotStalkiKind(LPITEM item)
	{
		if (!item || (item->GetType() != ITEM_WEAPON && item->GetType() != ITEM_ARMOR))
			return playerbot_stalki_rules::KIND_NONE;
		return playerbot_stalki_rules::KindOf(item->GetVnum());
	}

	bool IsPlayerBotStalkiItem(LPITEM item)
	{
		return GetPlayerBotStalkiKind(item) != playerbot_stalki_rules::KIND_NONE;
	}

	int GetPlayerBotStalkiWearCell(playerbot_stalki_rules::EKind kind)
	{
		if (kind == playerbot_stalki_rules::KIND_ARMOUR)
			return WEAR_BODY;
		if (kind == playerbot_stalki_rules::KIND_WEAPON)
			return WEAR_WEAPON;
		return -1;
	}

	// A piece of the bot's own for the kind's slot, whatever its level: its
	// class, its sex and the weapon its build wields (IsPlayerBotEquipmentCandidate
	// asks no level), in the slot the kind is worn in.
	bool IsPlayerBotStalkiSlotPiece(LPCHARACTER ch, LPITEM item, playerbot_stalki_rules::EKind kind)
	{
		const int wearCell = GetPlayerBotStalkiWearCell(kind);
		return ch && item && wearCell >= 0 && IsPlayerBotEquipmentCandidate(ch, item) &&
				item->FindEquipCell(ch) == wearCell;
	}

	// Whether the bot holds a piece of the kind's tier for its slot: of the
	// kind's level or over - a Stalki or anything bigger - worn, going back on
	// after the anvil, or in the bag, whether it can wear it yet or not. What
	// ends the want for another.
	bool PlayerBotHoldsStalkiTier(LPCHARACTER ch, playerbot_stalki_rules::EKind kind)
	{
		const int level = playerbot_stalki_rules::KindLevel(kind);
		if (!ch || level <= 0 || !ch->IsItemLoaded())
			return false;
		LPITEM fought = kind == playerbot_stalki_rules::KIND_WEAPON
				? GetPlayerBotHandWeapon(ch) : GetPlayerBotBodyArmour(ch);
		if (fought && (int)fought->GetLevelLimit() >= level && IsPlayerBotStalkiSlotPiece(ch, fought, kind))
			return true;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetCell() == cell && !item->IsEquipped() &&
					(int)item->GetLevelLimit() >= level && IsPlayerBotStalkiSlotPiece(ch, item, kind))
				return true;
		}
		return false;
	}

	// The one piece of the kind a bot keeps for the level it is about to
	// reach: its own, in the bag, a level it has not reached and at most
	// PLAYERBOT_STALKI_KEEP_AHEAD_LEVELS over its own, the best of them by the
	// equipment pass's own score (the lower id on a tie). NULL for none. The
	// equipment pass puts it on the day it can; until then it is neither the
	// merchant's nor the counter's, and a second copy is goods.
	LPITEM FindPlayerBotKeptStalki(LPCHARACTER ch, playerbot_stalki_rules::EKind kind)
	{
		if (!ch || !ch->IsItemLoaded() || kind == playerbot_stalki_rules::KIND_NONE)
			return NULL;
		const int level = (int)ch->GetLevel();
		LPITEM best = NULL;
		long long bestScore = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->IsEquipped() ||
					GetPlayerBotStalkiKind(item) != kind ||
					!playerbot_stalki_rules::KeepsAhead(level, (int)item->GetLevelLimit(),
						PLAYERBOT_STALKI_KEEP_AHEAD_LEVELS) ||
					!IsPlayerBotStalkiSlotPiece(ch, item, kind))
				continue;
			const long long score = GetPlayerBotEquipmentScore(item, ch);
			if (!best || playerbot_stalki_rules::Outranks(score, item->GetID(), bestScore, best->GetID()))
			{
				best = item;
				bestScore = score;
			}
		}
		return best;
	}

	// This piece is the one its slot keeps (FindPlayerBotKeptStalki). The level
	// is asked first: a piece the bot can wear or is too far from needs no walk
	// over the bag.
	bool IsPlayerBotKeptStalki(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->IsEquipped() || item->GetWindow() != INVENTORY)
			return false;
		const playerbot_stalki_rules::EKind kind = GetPlayerBotStalkiKind(item);
		if (kind == playerbot_stalki_rules::KIND_NONE ||
				!playerbot_stalki_rules::KeepsAhead((int)ch->GetLevel(), (int)item->GetLevelLimit(),
					PLAYERBOT_STALKI_KEEP_AHEAD_LEVELS))
			return false;
		return FindPlayerBotKeptStalki(ch, kind) == item;
	}

	// A bot the market may sell a piece of this kind to: not a dropper (its
	// time is its farm's) nor a person's companion (its owner dresses it), at
	// the kind's level or PLAYERBOT_STALKI_BUY_AHEAD_LEVELS before it, holding
	// nothing of the tier for the slot. The level is asked before the bag.
	bool IsPlayerBotStalkiShopperNow(LPCHARACTER ch, playerbot_stalki_rules::EKind kind)
	{
		if (!ch || !ch->IsItemLoaded() || kind == playerbot_stalki_rules::KIND_NONE)
			return false;
		const int level = (int)ch->GetLevel();
		if (!playerbot_stalki_rules::InBuyWindow(level, kind, PLAYERBOT_STALKI_BUY_AHEAD_LEVELS, false))
			return false;
		const DWORD pid = ch->GetPlayerID();
		if (IsPlayerBotDropper(GetPlayerBotPersonalityByPID(pid)) || IsPlayerBotSidekickPID(pid))
			return false;
		return playerbot_stalki_rules::InBuyWindow(level, kind, PLAYERBOT_STALKI_BUY_AHEAD_LEVELS,
				PlayerBotHoldsStalkiTier(ch, kind));
	}

	// The same answer kept for PLAYERBOT_STALKI_SHOPPER_CACHE_MS, one bit a
	// kind, for the passes that plan with it.
	struct TPlayerBotStalkiShopper
	{
		DWORD dwTime;
		BYTE bKinds;
		TPlayerBotStalkiShopper() : dwTime(0), bKinds(0) {}
	};
	std::map<DWORD, TPlayerBotStalkiShopper> s_mapPlayerBotStalkiShoppers;

	bool IsPlayerBotStalkiShopper(LPCHARACTER ch, playerbot_stalki_rules::EKind kind)
	{
		if (!ch || kind == playerbot_stalki_rules::KIND_NONE)
			return false;
		// Nobody under the window walks the bag, cached or not.
		if (!playerbot_stalki_rules::InBuyWindow((int)ch->GetLevel(), kind, PLAYERBOT_STALKI_BUY_AHEAD_LEVELS, false))
			return false;
		const DWORD now = get_dword_time();
		TPlayerBotStalkiShopper& known = s_mapPlayerBotStalkiShoppers[ch->GetPlayerID()];
		if (known.dwTime == 0 || now - known.dwTime >= PLAYERBOT_STALKI_SHOPPER_CACHE_MS)
		{
			known.dwTime = now != 0 ? now : 1;
			known.bKinds = 0;
			if (IsPlayerBotStalkiShopperNow(ch, playerbot_stalki_rules::KIND_ARMOUR))
				known.bKinds |= 1 << playerbot_stalki_rules::KIND_ARMOUR;
			if (IsPlayerBotStalkiShopperNow(ch, playerbot_stalki_rules::KIND_WEAPON))
				known.bKinds |= 1 << playerbot_stalki_rules::KIND_WEAPON;
		}
		return (known.bKinds & (1 << kind)) != 0;
	}

	// A counter's piece this bot buys as the next step of its gear: its own,
	// for a slot it is in the market for, asked afresh - a purchase that read
	// a kept answer could buy a second piece in the seconds after the first.
	bool IsPlayerBotStalkiProjectOffer(LPCHARACTER ch, LPITEM offer)
	{
		const playerbot_stalki_rules::EKind kind = GetPlayerBotStalkiKind(offer);
		return kind != playerbot_stalki_rules::KIND_NONE && IsPlayerBotStalkiSlotPiece(ch, offer, kind) &&
				IsPlayerBotStalkiShopperNow(ch, kind);
	}

	// What such a piece may cost the bot: the strategic share of what it can
	// spend (PLAYERBOT_STRATEGIC_BUDGET_PERCENT), the level-30 weapon's and the
	// horse medal's purse - the arithmetic of GetPlayerBotStrategicPurchaseCap
	// in playerbot_market.h, which the trip below comes before.
	long long GetPlayerBotStalkiBudget(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		const long long spare = (long long)ch->GetGold() - GetPlayerBotReservedGold(ch) -
				(long long)PLAYERBOT_SHOPPING_GOLD_FLOOR;
		return spare > 0 ? spare * PLAYERBOT_STRATEGIC_BUDGET_PERCENT / 100 : 0;
	}

	// The price sheet's +0 of a family (PLAYERBOT_GEAR_PRICES) at this world's
	// yang rate, 0 when the sheet has no row: the least a piece of it asks.
	DWORD GetPlayerBotStalkiSheetPrice(DWORD family)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_GEAR_PRICES) / sizeof(PLAYERBOT_GEAR_PRICES[0]); ++i)
			if (PLAYERBOT_GEAR_PRICES[i].dwBaseVnum == family && PLAYERBOT_GEAR_PRICES[i].adwPrice[0] != 0)
				return ScalePlayerBotIwakuraPrice(PLAYERBOT_GEAR_PRICES[i].adwPrice[0]);
		return 0;
	}

	// Whether a family is one this character wears: its class, and for a
	// weapon the kind its build wields.
	bool IsPlayerBotStalkiFamilyFor(LPCHARACTER ch, DWORD family)
	{
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(family);
		if (!ch || !proto || !IsPlayerBotProtoForCharacter(ch, proto))
			return false;
		return proto->bType != ITEM_WEAPON || IsPlayerBotWeaponSubTypeFor(ch, proto->bSubType);
	}

	// Whether the counters of the bot's first village hold a piece it is in
	// the market for and it could pay the sheet's +0 of: what a trip there is
	// made for (playerbot_progression_needs.h), read off the ledger without a
	// counter - the purchase asks the line itself on arrival.
	bool PlayerBotStalkiSupplyExists(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		const bool armour = IsPlayerBotStalkiShopper(ch, playerbot_stalki_rules::KIND_ARMOUR);
		const bool weapon = IsPlayerBotStalkiShopper(ch, playerbot_stalki_rules::KIND_WEAPON);
		if (!armour && !weapon)
			return false;
		const long long budget = GetPlayerBotStalkiBudget(ch);
		if (budget <= 0)
			return false;
		const long firstVillage = playerbot_empire_rules::GetHomeMap((int)ch->GetEmpire(),
				playerbot_empire_rules::MAP_ROLE_M1);
		for (int pass = 0; pass < 2; ++pass)
		{
			if (pass == 0 ? !armour : !weapon)
				continue;
			const unsigned* families = pass == 0 ? playerbot_stalki_rules::ARMOUR_FAMILIES
					: playerbot_stalki_rules::WEAPON_FAMILIES;
			const size_t count = pass == 0
					? sizeof(playerbot_stalki_rules::ARMOUR_FAMILIES) / sizeof(playerbot_stalki_rules::ARMOUR_FAMILIES[0])
					: sizeof(playerbot_stalki_rules::WEAPON_FAMILIES) / sizeof(playerbot_stalki_rules::WEAPON_FAMILIES[0]);
			for (size_t i = 0; i < count; ++i)
			{
				const DWORD family = families[i];
				if (!IsPlayerBotStalkiFamilyFor(ch, family))
					continue;
				const DWORD price = GetPlayerBotStalkiSheetPrice(family);
				if (price == 0 || (long long)price > budget)
					continue;
				for (DWORD plus = 0; plus < playerbot_stalki_rules::FAMILY_GRADES; ++plus)
					if (GetPlayerBotMarketLocalSupply(firstVillage, family + plus) > 0)
						return true;
			}
		}
		return false;
	}

	// A piece in the storekeeper's box the bot would keep or wear now: its
	// own, the tier missing from what it holds for the slot, a level it has
	// reached or will within the keep. The box's own rules put a piece of
	// Iwakura's list down for the gambler, and the withdrawal asks those
	// first (WithdrawPlayerBotSafebox).
	bool PlayerBotWantsStalkiFromBox(LPCHARACTER ch, LPITEM item)
	{
		const playerbot_stalki_rules::EKind kind = GetPlayerBotStalkiKind(item);
		if (!ch || kind == playerbot_stalki_rules::KIND_NONE || !IsPlayerBotStalkiSlotPiece(ch, item, kind) ||
				(int)item->GetLevelLimit() > (int)ch->GetLevel() + PLAYERBOT_STALKI_KEEP_AHEAD_LEVELS)
			return false;
		return !PlayerBotHoldsStalkiTier(ch, kind);
	}
}

#endif
