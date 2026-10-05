#ifndef __INC_METIN2_PLAYERBOT_GEAR_UPGRADE_H__
#define __INC_METIN2_PLAYERBOT_GEAR_UPGRADE_H__

// MT2009_PLUS_BOT_GEAR_UPGRADE_V1: a bot plays for the hardest blow it can
// buy. MT2009_PLUS_BOT_GEAR_UPGRADE_V2: and for the best armour (the armour
// section at the end of this file). "Boty niech kupuja lepsze bronie i daza do tego aby miec jak
// najwieksze obrazenia. Jesli tylko dana bron jest dostepna na rynku, to ja
// kupuja i ulepszaja" (the owner, 5 October), after the dungeon runs of the
// test world showed bots of 75 to 79 swinging a weapon of level 48 on
// average, 122 of 160 a weapon more than twenty levels under them, and a
// Shaman in a dungeon with a pickaxe in its hand.
//
// What the counters held that day: 30 765 weapons on the bots' stands, 18 000
// of them of level 40 and up, 16 100 of those at +4 and 31 at +6 or more. The
// market's gear rule bought a finished piece only (+6 and a grade over the
// one worn - Iwakura's Patch 4, point 2), which is right for armour bought to
// be worn as it is and wrong for a weapon two tiers above the hand: none of
// the sixteen thousand was ever a candidate, and the weapon goal
// (playerbot_weapon_goal.h) sent bots to the market for a weapon the market
// then refused to sell them. Under the old rule 76 of the 1539 bots of 40 and
// up could pay for any line that beat their weapon by a tenth; under this one
// 334 can, from the same counters and the same purses.
//
// The rule: a counter weapon at any plus the bot can wear now, whose blow by
// the damage model (GetPlayerBotWeaponHitDamage - the dice, the plus, the
// level bonus, the lines) beats the best weapon it owns by
// PLAYERBOT_WEAPON_UPGRADE_MIN_GAIN_PERCENT, and which the equipment pass will
// put on (its score over the hand's). It is the blow the weapon has now, not
// the one it might reach: what is bought goes on at once, and the anvil takes
// it on from there (GetPlayerBotRefineTarget: the weapon in the hand climbs to
// the bot's aim, +6 at least). Paid from half of what the bot holds over the
// floor, or four fifths of it for a gain of a quarter or more - one weapon at
// a time, since the next one has to beat this one by a tenth again.
//
// Also here: the village merchant's best weapon for a bot under 36 whose
// ladder names a tier the merchant does not stock (the ladder's piece for a
// bot of 20 to 24 is the level-20 one, which no merchant sells, so the bots of
// 20 kept the level-one sword +6), and the tool guard - a rod, a pickaxe or
// a herbalist's knife never stays in the hand outside its own session.
//
// Included after playerbot_herbalism.h (the tool sessions) and before
// playerbot_economy.h, playerbot_progression_needs.h and playerbot_market.h,
// which ask it.
namespace
{
	// The best weapon this bot owns that it could fight with now - the one in
	// the hand when it is a weapon of its kind, else the best of the bag - by
	// blow and by the equipment score the equipment pass ranks with.
	struct TPlayerBotOwnedWeapon
	{
		LPITEM item;
		long long blow;
		long long score;
		int subType;

		TPlayerBotOwnedWeapon() : item(NULL), blow(0), score(0), subType(-1)
		{
		}
	};

	TPlayerBotOwnedWeapon ReadPlayerBotBestOwnedWeapon(LPCHARACTER ch)
	{
		TPlayerBotOwnedWeapon best;
		if (!ch || !ch->IsItemLoaded())
			return best;
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		if (worn && worn->GetType() == ITEM_WEAPON && IsPlayerBotWeapon(ch, worn))
		{
			best.item = worn;
			best.blow = GetPlayerBotWeaponHitDamage(worn, ch);
			best.score = GetPlayerBotEquipmentScore(worn, ch);
		}
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->IsEquipped() || item->GetType() != ITEM_WEAPON ||
					!IsPlayerBotEquipmentCandidate(ch, item) || item->GetLevelLimit() > ch->GetLevel() ||
					item->FindEquipCell(ch) != WEAR_WEAPON)
				continue;
			const long long blow = GetPlayerBotWeaponHitDamage(item, ch);
			if (best.item && blow <= best.blow)
				continue;
			best.item = item;
			best.blow = blow;
			best.score = GetPlayerBotEquipmentScore(item, ch);
		}
		best.subType = best.item ? (int)best.item->GetSubType() : -1;
		return best;
	}

	// The gain of a blow over the best weapon owned, in percent; a bot with no
	// weapon at all gains everything.
	long long GetPlayerBotWeaponGainPercent(long long blow, long long ownedBlow)
	{
		if (ownedBlow <= 0)
			return 1000;
		return blow * 100 / ownedBlow - 100;
	}

	// What a weapon upgrade with this gain may cost: a share of what the bot
	// holds over its reserve and PLAYERBOT_WEAPON_UPGRADE_GOLD_FLOOR.
	long long GetPlayerBotWeaponUpgradeBudget(LPCHARACTER ch, long long gainPercent)
	{
		if (!ch)
			return 0;
		const long long spare = (long long)ch->GetGold() - (long long)GetPlayerBotReservedGold(ch) -
				PLAYERBOT_WEAPON_UPGRADE_GOLD_FLOOR;
		if (spare <= 0)
			return 0;
		return spare * (gainPercent >= PLAYERBOT_WEAPON_UPGRADE_BIG_GAIN_PERCENT
				? PLAYERBOT_WEAPON_UPGRADE_BIG_BUDGET_PERCENT : PLAYERBOT_WEAPON_UPGRADE_SMALL_BUDGET_PERCENT) / 100;
	}

	// A weapon nobody holds yet, by its proto, that this bot could carry now:
	// its kind, its class and sex, a level it has reached and not one of the
	// tenth level or under in the hand of a bot of thirty.
	bool IsPlayerBotWeaponProtoFor(LPCHARACTER ch, const TItemTable* proto)
	{
		if (!ch || !proto || proto->bType != ITEM_WEAPON || proto->bSubType == WEAPON_ARROW ||
				!IsPlayerBotWeaponSubTypeFor(ch, proto->bSubType) || !IsPlayerBotProtoForCharacter(ch, proto))
			return false;
		const int level = GetPlayerBotProtoLevelLimit(proto);
		return level <= (int)ch->GetLevel() &&
				!playerbot_refine_rules::IsLowWeaponFor((int)ch->GetLevel(), level);
	}

	// A counter weapon this bot should buy: one it can wear now whose blow
	// beats the best weapon it owns by PLAYERBOT_WEAPON_UPGRADE_MIN_GAIN_PERCENT
	// and which the equipment pass would put on over it. gainOut, when given,
	// is the gain in percent the purse is drawn by.
	bool IsPlayerBotWeaponUpgradeOffer(LPCHARACTER ch, LPITEM offer, long long* gainOut)
	{
		if (!ch || !offer || offer->GetType() != ITEM_WEAPON || offer->GetSubType() == WEAPON_ARROW ||
				!IsPlayerBotEquipmentCandidate(ch, offer) || offer->GetLevelLimit() > ch->GetLevel() ||
				offer->FindEquipCell(ch) != WEAR_WEAPON || IsPlayerBotArcherStoneWeapon(ch, offer))
			return false;
		const TPlayerBotOwnedWeapon owned = ReadPlayerBotBestOwnedWeapon(ch);
		if (owned.item == offer)
			return false;
		const long long blow = GetPlayerBotWeaponHitDamage(offer, ch);
		if (owned.item && blow * 100 < owned.blow * (100 + PLAYERBOT_WEAPON_UPGRADE_MIN_GAIN_PERCENT))
			return false;
		// The class's preferences (a bow for an Archer, a dagger for a Dagger
		// Ninja) are in the score and not in the blow: a weapon the equipment
		// pass would leave in the bag is no upgrade, however hard it hits.
		if (owned.item && GetPlayerBotEquipmentScore(offer, ch) <= owned.score)
			return false;
		if (gainOut)
			*gainOut = GetPlayerBotWeaponGainPercent(blow, owned.item ? owned.blow : 0);
		return true;
	}

	// The cheapest line of every weapon on the stands, by map: written with the
	// ledger once a minute (AddPlayerBotOfflineLedger), so a bot on the
	// frontier knows without reading a counter whether its first village holds
	// a weapon it would buy.
	std::map<unsigned long long, long long> s_mapPlayerBotWeaponLineCheapest;

	void ClearPlayerBotWeaponLinePrices()
	{
		s_mapPlayerBotWeaponLineCheapest.clear();
	}

	void NotePlayerBotWeaponLinePrice(long lMapIndex, const TItemTable* table, DWORD count, long long price)
	{
		if (lMapIndex <= 0 || !table || table->bType != ITEM_WEAPON || table->bSubType == WEAPON_ARROW ||
				count != 1 || price <= 0)
			return;
		long long& cheapest = s_mapPlayerBotWeaponLineCheapest[PlayerBotMarketLocalKey(lMapIndex, table->dwVnum)];
		if (cheapest <= 0 || price < cheapest)
			cheapest = price;
	}

	// One bot's last look at its first village's weapons.
	struct TPlayerBotWeaponUpgradeLook
	{
		DWORD when;
		DWORD vnum;
		long long price;
		long long gain;

		TPlayerBotWeaponUpgradeLook() : when(0), vnum(0), price(0), gain(0)
		{
		}
	};
	std::map<DWORD, TPlayerBotWeaponUpgradeLook> s_mapPlayerBotWeaponUpgradeLooks;
	// Bots that set off for or picked such a weapon since the last report.
	DWORD s_dwPlayerBotWeaponUpgradePicks = 0;
	DWORD s_dwPlayerBotMerchantWeaponUpgrades = 0;

	// Whether a stand of the bot's first village holds a weapon it would buy
	// and could pay for now - by the cheapest line of each weapon and its blow
	// at that line's plus, the line's rolled lines left out (they only ever
	// make the line better). Read again every PLAYERBOT_WEAPON_UPGRADE_LOOK_MS.
	// The kind of the weapon owned is kept, so an Archer is never sent for a
	// sword its preference would leave in the bag.
	const TPlayerBotWeaponUpgradeLook& LookPlayerBotWeaponUpgrade(LPCHARACTER ch, DWORD dwNow)
	{
		TPlayerBotWeaponUpgradeLook& look = s_mapPlayerBotWeaponUpgradeLooks[ch->GetPlayerID()];
		if (look.when != 0 && dwNow - look.when < PLAYERBOT_WEAPON_UPGRADE_LOOK_MS)
			return look;
		look.when = dwNow != 0 ? dwNow : 1;
		look.vnum = 0;
		look.price = 0;
		look.gain = 0;
		if (!ch->IsItemLoaded() || s_mapPlayerBotWeaponLineCheapest.empty() ||
				GetPlayerBotWeaponUpgradeBudget(ch, PLAYERBOT_WEAPON_UPGRADE_BIG_GAIN_PERCENT) <= 0)
			return look;
		const long firstVillage = playerbot_empire_rules::GetHomeMap(ch->GetEmpire(),
				playerbot_empire_rules::MAP_ROLE_M1);
		if (firstVillage <= 0)
			return look;
		const TPlayerBotOwnedWeapon owned = ReadPlayerBotBestOwnedWeapon(ch);
		long long bestBlow = 0;
		const unsigned long long lo = PlayerBotMarketLocalKey(firstVillage, 0);
		const unsigned long long hi = PlayerBotMarketLocalKey(firstVillage, 0xFFFFFFFFU);
		for (std::map<unsigned long long, long long>::const_iterator it = s_mapPlayerBotWeaponLineCheapest.lower_bound(lo);
				it != s_mapPlayerBotWeaponLineCheapest.end() && it->first <= hi; ++it)
		{
			const DWORD vnum = (DWORD)(it->first & 0xFFFFFFFFULL);
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
			if (!IsPlayerBotWeaponProtoFor(ch, proto) ||
					(owned.subType >= 0 && (int)proto->bSubType != owned.subType))
				continue;
			const long long blow = GetPlayerBotWeaponHitDamageAt(NULL, proto, ch);
			if (blow <= bestBlow ||
					(owned.item && blow * 100 < owned.blow * (100 + PLAYERBOT_WEAPON_UPGRADE_MIN_GAIN_PERCENT)))
				continue;
			const long long gain = GetPlayerBotWeaponGainPercent(blow, owned.item ? owned.blow : 0);
			if (it->second > GetPlayerBotWeaponUpgradeBudget(ch, gain))
				continue;
			bestBlow = blow;
			look.vnum = vnum;
			look.price = it->second;
			look.gain = gain;
		}
		return look;
	}

	bool PlayerBotWantsWeaponUpgradeFromMarket(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded() || IsPlayerBotGearFrozen(ch))
			return false;
		return LookPlayerBotWeaponUpgrade(ch, get_dword_time()).vnum != 0;
	}

	// The village merchant's best weapon for this bot, when it beats the best
	// one owned by the upgrade's share and the purse pays for it. The ladder
	// (GetPlayerBotProgressionWeaponVnum) names the family's own tier for the
	// bot's level, and the merchants stock the tiers of 15, 25 and 36 only.
	DWORD FindPlayerBotMerchantWeaponUpgrade(LPCHARACTER ch, long long* priceOut)
	{
		if (priceOut)
			*priceOut = 0;
		if (!ch || !ch->IsItemLoaded())
			return 0;
		static const DWORD merchants[] = { 9001, 9002, 9003 };
		TPlayerBotOwnedWeapon owned;
		bool ownedRead = false;
		DWORD bestVnum = 0;
		long long bestBlow = 0;
		for (size_t i = 0; i < sizeof(merchants) / sizeof(merchants[0]); ++i)
		{
			LPSHOP shop = CShopManager::instance().GetByNPCVnum(merchants[i]);
			if (!shop)
				continue;
			const std::vector<CShop::SHOP_ITEM>& offers = shop->GetItemVector();
			for (size_t k = 0; k < offers.size(); ++k)
			{
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(offers[k].vnum);
				if (!IsPlayerBotWeaponProtoFor(ch, proto))
					continue;
				if (!ownedRead)
				{
					owned = ReadPlayerBotBestOwnedWeapon(ch);
					ownedRead = true;
				}
				if (owned.subType >= 0 && (int)proto->bSubType != owned.subType)
					continue;
				const long long blow = GetPlayerBotWeaponHitDamageAt(NULL, proto, ch);
				if (blow <= bestBlow ||
						(owned.item && blow * 100 < owned.blow * (100 + PLAYERBOT_WEAPON_UPGRADE_MIN_GAIN_PERCENT)))
					continue;
				// Half of the purse over PLAYERBOT_WEAPON_UPGRADE_MERCHANT_FLOOR:
				// the counters' floor would keep a bot of twenty, whose purse is a
				// few hundred thousand, from a sword of 5 000.
				const long long price = GetPlayerBotMerchantOfferPrice(offers[k], proto);
				if (price * 2 > (long long)ch->GetGold() - (long long)GetPlayerBotReservedGold(ch) -
						PLAYERBOT_WEAPON_UPGRADE_MERCHANT_FLOOR)
					continue;
				bestVnum = offers[k].vnum;
				bestBlow = blow;
				if (priceOut)
					*priceOut = price;
			}
		}
		return bestVnum;
	}

	bool PlayerBotWantsMerchantWeaponUpgrade(LPCHARACTER ch)
	{
		return ch && ch->IsItemLoaded() && !IsPlayerBotGearFrozen(ch) &&
				FindPlayerBotMerchantWeaponUpgrade(ch, NULL) != 0;
	}

	bool BuyPlayerBotMerchantWeaponUpgrade(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded() || IsPlayerBotGearFrozen(ch))
			return false;
		long long price = 0;
		const DWORD vnum = FindPlayerBotMerchantWeaponUpgrade(ch, &price);
		if (vnum == 0 || !BuyPlayerBotProgressionGear(ch, vnum, "weapon upgrade"))
			return false;
		++s_dwPlayerBotMerchantWeaponUpgrades;
		return true;
	}

	// A rod, a pickaxe or a herbalist's knife stays in the hand only while its
	// own session owns the bot, and never in a dungeon. Every session puts its
	// tool away when it ends, and every swing and cast takes it out of the
	// hand first (ReadyPlayerBotHandForFight) - but the dungeon runs, the
	// Arezzo cohort, a person's dungeon and the shouters claim the tick above
	// the sessions and above the equipment pass, so a bot called away from the
	// water or the vein kept its tool for as long as the call lasted, and a
	// healing Shaman, which never swings, walked a whole dungeon with its
	// pickaxe (the dungeon runs of 5 October). Asked at the top of the tick,
	// above every pass that claims it.
	std::map<DWORD, DWORD> s_mapPlayerBotToolGuardNext;

	bool ManagePlayerBotToolHand(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead() || !ch->IsItemLoaded())
			return false;
		LPITEM held = ch->GetWear(WEAR_WEAPON);
		if (!held || !IsPlayerBotToolType(held->GetType()) || IsPlayerBotGearFrozen(ch) || ch->GetMyShop() ||
				ch->GetExchange())
			return false;
		const DWORD pid = ch->GetPlayerID();
		bool sessionLive = false;
		if (held->GetType() == ITEM_ROD)
			sessionLive = state.bFishingSession;
		else if (held->GetType() == ITEM_PICK)
			sessionLive = IsPlayerBotMiningNow(pid, dwNow);
		else
			sessionLive = IsPlayerBotHerbPickingNow(ch, dwNow);
		const bool calledAway = IsPlayerBotOnDungeonRun(pid) || IsPlayerBotInDungeonBusiness(ch, state);
		if (sessionLive && !calledAway)
			return false;
		std::map<DWORD, DWORD>::iterator next = s_mapPlayerBotToolGuardNext.find(pid);
		if (next != s_mapPlayerBotToolGuardNext.end() && (int)(dwNow - next->second) < 0)
			return false;
		s_mapPlayerBotToolGuardNext[pid] = dwNow + PLAYERBOT_TOOL_GUARD_RETRY_MS;
		const DWORD toolVnum = held->GetVnum();
		// A digging session whose clock ran out is ended here with its rest:
		// its own end is asked only while the clock still runs, and the next
		// look at the veins would otherwise start another at once.
		if (held->GetType() == ITEM_PICK && !sessionLive && !calledAway)
			EndPlayerBotMiningSession(ch, state, dwNow, "tool_guard");
		ReadyPlayerBotHandForFight(ch, state, dwNow, calledAway ? "tool_guard_dungeon" : "tool_guard_stale");
		LPITEM now = ch->GetWear(WEAR_WEAPON);
		PlayerBotLogThrottled("tool_guard", dwNow,
				"PLAYERBOT_GEAR: tool put away pid=%u name=%s tool=%u weapon=%u session=%d called_away=%d map=%ld",
				pid, ch->GetName(), toolVnum, now ? now->GetVnum() : 0, sessionLive ? 1 : 0, calledAway ? 1 : 0,
				ch->GetMapIndex());
		return false;
	}

	// Once a report interval with the weapon census (ReportPlayerBotWeaponGoals):
	// how many bots' last look found a weapon on their first village's stands
	// they would buy now, and how many went for one since the last report.
	void ReportPlayerBotWeaponUpgrades(DWORD dwNow)
	{
		DWORD fresh = 0, wanting = 0;
		long long gainSum = 0;
		for (std::map<DWORD, TPlayerBotWeaponUpgradeLook>::iterator it = s_mapPlayerBotWeaponUpgradeLooks.begin();
				it != s_mapPlayerBotWeaponUpgradeLooks.end(); )
		{
			if (it->second.when == 0 || dwNow - it->second.when >= 4 * PLAYERBOT_WEAPON_UPGRADE_LOOK_MS)
			{
				s_mapPlayerBotWeaponUpgradeLooks.erase(it++);
				continue;
			}
			++fresh;
			if (it->second.vnum != 0)
			{
				++wanting;
				gainSum += std::min<long long>(it->second.gain, 1000);
			}
			++it;
		}
		sys_log(0, "PLAYERBOT_GEAR_UPGRADE: census looked=%u affordable_upgrade=%u avg_gain=%lld%% lines_priced=%u picks=%u merchant_buys=%u",
				fresh, wanting, wanting ? gainSum / wanting : 0LL,
				(unsigned int)s_mapPlayerBotWeaponLineCheapest.size(), s_dwPlayerBotWeaponUpgradePicks,
				s_dwPlayerBotMerchantWeaponUpgrades);
		s_dwPlayerBotWeaponUpgradePicks = 0;
		s_dwPlayerBotMerchantWeaponUpgrades = 0;
	}

	// MT2009_PLUS_BOT_GEAR_UPGRADE_V2: the same for the armour. "Tak, rob to
	// samo ze zbroja" (the owner, 5 October), after the census of the test
	// world: bots of 70 to 79 wore a body armour of level 44.5 on average, 236
	// of the 324 one more than twenty levels under them, while 1 247 of the
	// 1 539 bots of 40 and up had a body armour a tenth better on their first
	// village's stands. The market's gear rule bought a finished piece only
	// (+6 and a grade over the worn one) and the stands' armour is +0 to +5:
	// 482 of the 2 835 bots of 10 and up could pay for some armour that rule
	// (or the outdated-gear one) took, 58 for a body armour. Under this one,
	// from the same stands and purses, 1 670 can, 194 for a body armour, 717
	// for a helmet, and a bracelet, necklace or earrings a thousand each -
	// the purse is what holds the body armour back (a stand's body armour of
	// 34 and up asks 600 000 to 1.3 million, a bot of 50 to 79 holds some
	// 430 000).
	//
	// The rule: a piece of the seven armour slots - body armour, helmet,
	// shield, bracelet, boots, necklace, earrings - at any plus the bot can
	// wear now, whose equipment score (GetPlayerBotEquipmentScore: defence,
	// level, its lines, the family's tier; for boots the lines alone, as the
	// equipment pass wears them) beats the best piece of that slot it owns by
	// PLAYERBOT_ARMOUR_UPGRADE_MIN_GAIN_PERCENT. Paid from the weapon's
	// shares of the purse, after the weapon its own look found has been set
	// aside (GetPlayerBotArmourUpgradeBudget): the weapon comes first, and the
	// armour never spends what the next weapon costs. Of several, the most
	// score a yang buys (the helmets ask a quarter of a body armour's price).
	// What is bought goes on at once, and the anvil takes it on from there
	// (GetPlayerBotRefineTarget, the worn piece's aim).
	//
	// And the village merchants: a slot with nothing in it or with less than
	// the merchant's piece by the same share takes the merchant's best one -
	// the bracelet, necklace and earring ladder names a tier no merchant
	// stocks from level 8, the boots' from level 9, and nothing fell back to
	// the level-0 piece the General Store sells (wrist slot empty on 315 of
	// the 1 140 bots of 50 and up, boots on 199, necklace on 178, earrings on
	// 171). The body armour ladder already falls back to the merchant's best
	// (BuyPlayerBotBestMerchantSlotGear), and so do the helmet and the shield.

	// The slots, body armour first: an equal score per yang goes to the
	// earlier one.
	const BYTE PLAYERBOT_ARMOUR_UPGRADE_WEARS[] = {
		WEAR_BODY, WEAR_HEAD, WEAR_SHIELD, WEAR_WRIST, WEAR_FOOTS, WEAR_NECK, WEAR_EAR };
	const int PLAYERBOT_ARMOUR_UPGRADE_SLOTS = 7;
	const char* const PLAYERBOT_ARMOUR_UPGRADE_SLOT_NAMES[] = {
		"body", "head", "shield", "wrist", "foots", "neck", "ear" };

	int GetPlayerBotArmourUpgradeSlot(BYTE subType)
	{
		switch (subType)
		{
			case ARMOR_BODY:   return 0;
			case ARMOR_HEAD:   return 1;
			case ARMOR_SHIELD: return 2;
			case ARMOR_WRIST:  return 3;
			case ARMOR_FOOTS:  return 4;
			case ARMOR_NECK:   return 5;
			case ARMOR_EAR:    return 6;
			default:           return -1;
		}
	}

	// The best piece of each slot this bot owns that it could wear now: the
	// worn one, else the best of the bag, by the equipment score. onlySlot
	// reads one slot.
	struct TPlayerBotOwnedArmour
	{
		LPITEM item[PLAYERBOT_ARMOUR_UPGRADE_SLOTS];
		long long score[PLAYERBOT_ARMOUR_UPGRADE_SLOTS];

		TPlayerBotOwnedArmour()
		{
			for (int i = 0; i < PLAYERBOT_ARMOUR_UPGRADE_SLOTS; ++i)
			{
				item[i] = NULL;
				score[i] = 0;
			}
		}
	};

	void ReadPlayerBotBestOwnedArmour(LPCHARACTER ch, TPlayerBotOwnedArmour& owned, int onlySlot = -1)
	{
		owned = TPlayerBotOwnedArmour();
		if (!ch || !ch->IsItemLoaded())
			return;
		// What is worn counts whatever the candidate test says of it: it is on.
		for (int slot = 0; slot < PLAYERBOT_ARMOUR_UPGRADE_SLOTS; ++slot)
		{
			if (onlySlot >= 0 && slot != onlySlot)
				continue;
			LPITEM worn = ch->GetWear(PLAYERBOT_ARMOUR_UPGRADE_WEARS[slot]);
			if (!worn || worn->GetType() != ITEM_ARMOR)
				continue;
			owned.item[slot] = worn;
			owned.score[slot] = GetPlayerBotEquipmentScore(worn, ch);
		}
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->IsEquipped() || item->GetType() != ITEM_ARMOR)
				continue;
			const int slot = GetPlayerBotArmourUpgradeSlot(item->GetSubType());
			if (slot < 0 || (onlySlot >= 0 && slot != onlySlot) || item->GetLevelLimit() > ch->GetLevel() ||
					!IsPlayerBotEquipmentCandidate(ch, item) || item->FindEquipCell(ch) != PLAYERBOT_ARMOUR_UPGRADE_WEARS[slot])
				continue;
			const long long score = GetPlayerBotEquipmentScore(item, ch);
			if (owned.item[slot] && score <= owned.score[slot])
				continue;
			owned.item[slot] = item;
			owned.score[slot] = score;
		}
	}

	// The gain of a score over the best piece owned, in percent; an empty
	// slot gains everything.
	long long GetPlayerBotArmourGainPercent(long long score, long long ownedScore)
	{
		if (ownedScore <= 0)
			return 1000;
		return score * 100 / ownedScore - 100;
	}

	// What an armour upgrade with this gain may cost: the weapon's shares of
	// what the bot holds over its reserve and the floor, once the weapon its
	// look found on the stands (LookPlayerBotWeaponUpgrade) is paid for.
	long long GetPlayerBotArmourUpgradeSpare(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		long long spare = (long long)ch->GetGold() - (long long)GetPlayerBotReservedGold(ch) -
				PLAYERBOT_ARMOUR_UPGRADE_GOLD_FLOOR;
		if (spare <= 0)
			return 0;
		if (ch->IsItemLoaded() && !IsPlayerBotGearFrozen(ch))
		{
			const TPlayerBotWeaponUpgradeLook& weapon = LookPlayerBotWeaponUpgrade(ch, get_dword_time());
			if (weapon.vnum != 0)
				spare -= weapon.price;
		}
		return std::max<long long>(0, spare);
	}

	long long GetPlayerBotArmourUpgradeShare(long long spare, long long gainPercent)
	{
		return spare <= 0 ? 0 : spare * (gainPercent >= PLAYERBOT_ARMOUR_UPGRADE_BIG_GAIN_PERCENT
				? PLAYERBOT_ARMOUR_UPGRADE_BIG_BUDGET_PERCENT : PLAYERBOT_ARMOUR_UPGRADE_SMALL_BUDGET_PERCENT) / 100;
	}

	long long GetPlayerBotArmourUpgradeBudget(LPCHARACTER ch, long long gainPercent)
	{
		return GetPlayerBotArmourUpgradeShare(GetPlayerBotArmourUpgradeSpare(ch), gainPercent);
	}

	// An armour nobody holds yet, by its proto, that this bot could wear now:
	// one of the seven slots, its class and sex, a level it has reached, and
	// no body armour, helmet or shield the outdated-gear rule would take off
	// again (IsPlayerBotOutdatedGear).
	bool IsPlayerBotArmourProtoFor(LPCHARACTER ch, const TItemTable* proto)
	{
		if (!ch || !proto || proto->bType != ITEM_ARMOR || GetPlayerBotArmourUpgradeSlot(proto->bSubType) < 0 ||
				!IsPlayerBotProtoForCharacter(ch, proto))
			return false;
		const int level = GetPlayerBotProtoLevelLimit(proto);
		if (level > (int)ch->GetLevel())
			return false;
		return !(IsPlayerBotOutdatedGearSubType(proto->bSubType) &&
				(int)ch->GetLevel() >= PLAYERBOT_OUTDATED_GEAR_MIN_LEVEL &&
				level + PLAYERBOT_OUTDATED_GEAR_LEVELS <= (int)ch->GetLevel());
	}

	// The equipment score of a piece nobody holds yet, by its proto - what
	// GetPlayerBotEquipmentScore gives a clean copy of it: the defence, the
	// level, the proto's own lines and the family's tier (for boots the
	// family's tier and the level). The rolled lines, the race lines and a
	// Specjalny's weight on the plus are left out; they only make a line
	// better.
	long long GetPlayerBotArmourProtoScore(LPCHARACTER ch, const TItemTable* proto)
	{
		if (!ch || !proto || proto->bType != ITEM_ARMOR)
			return 0;
		const int level = GetPlayerBotProtoLevelLimit(proto);
		const DWORD refine = proto->dwVnum % 10;
		const int tier = GetPlayerBotItemTier(proto->dwVnum - refine, (int)ch->GetJob(), false);
		if (proto->bSubType == ARMOR_FOOTS)
			return 1 + (long long)std::max(1, tier) * PLAYERBOT_BOOTS_FAMILY_TIER_SCORE +
					(long long)level * PLAYERBOT_ARMOR_LEVEL_TIE_BREAK;
		long long score = 1;
		if (proto->bSubType == ARMOR_BODY || proto->bSubType == ARMOR_HEAD || proto->bSubType == ARMOR_SHIELD)
			score += (long long)(proto->alValues[1] + 2 * proto->alValues[5]) * 1000;
		score += (long long)level * PLAYERBOT_ARMOR_LEVEL_TIE_BREAK;
		for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
			score += ScorePlayerBotApplyTiered(proto->aApplies[i].bType, proto->aApplies[i].lValue, ch);
		if (const std::vector<TItemApply>* extraApplies = GetMt2009ItemExtraApplies(proto->dwVnum))
			for (const TItemApply& extra : *extraApplies)
				score += ScorePlayerBotApplyTiered(extra.bType, extra.lValue, ch);
		if (proto->dwImmuneFlag != 0)
			score += 1000;
		if (tier > 0)
			score = score * (100 + (tier - 3) * PLAYERBOT_TIER_SCORE_PERCENT) / 100;
		return score;
	}

	// A counter's armour this bot should buy: one it can wear now whose score
	// beats the best piece of its slot it owns by
	// PLAYERBOT_ARMOUR_UPGRADE_MIN_GAIN_PERCENT - so the equipment pass puts
	// it on over the worn one. gainOut, when given, is the gain in percent the
	// purse is drawn by.
	bool IsPlayerBotArmourUpgradeOffer(LPCHARACTER ch, LPITEM offer, long long* gainOut)
	{
		if (!ch || !offer || offer->GetType() != ITEM_ARMOR || IsPlayerBotGearFrozen(ch))
			return false;
		const int slot = GetPlayerBotArmourUpgradeSlot(offer->GetSubType());
		if (slot < 0 || !IsPlayerBotEquipmentCandidate(ch, offer) || offer->GetLevelLimit() > ch->GetLevel() ||
				offer->FindEquipCell(ch) != PLAYERBOT_ARMOUR_UPGRADE_WEARS[slot] || IsPlayerBotOutdatedGear(ch, offer))
			return false;
		TPlayerBotOwnedArmour owned;
		ReadPlayerBotBestOwnedArmour(ch, owned, slot);
		if (owned.item[slot] == offer)
			return false;
		const long long score = GetPlayerBotEquipmentScore(offer, ch);
		if (owned.item[slot] &&
				score * 100 < owned.score[slot] * (100 + PLAYERBOT_ARMOUR_UPGRADE_MIN_GAIN_PERCENT))
			return false;
		if (gainOut)
			*gainOut = GetPlayerBotArmourGainPercent(score, owned.item[slot] ? owned.score[slot] : 0);
		return true;
	}

	// The cheapest line of every armour on the stands, by map, written with
	// the weapons' (AddPlayerBotOfflineLedger).
	std::map<unsigned long long, long long> s_mapPlayerBotArmourLineCheapest;

	void ClearPlayerBotArmourLinePrices()
	{
		s_mapPlayerBotArmourLineCheapest.clear();
	}

	void NotePlayerBotArmourLinePrice(long lMapIndex, const TItemTable* table, DWORD count, long long price)
	{
		if (lMapIndex <= 0 || !table || table->bType != ITEM_ARMOR ||
				GetPlayerBotArmourUpgradeSlot(table->bSubType) < 0 || count != 1 || price <= 0)
			return;
		long long& cheapest = s_mapPlayerBotArmourLineCheapest[PlayerBotMarketLocalKey(lMapIndex, table->dwVnum)];
		if (cheapest <= 0 || price < cheapest)
			cheapest = price;
	}

	// One bot's last look at its first village's armour.
	struct TPlayerBotArmourUpgradeLook
	{
		DWORD when;
		DWORD vnum;
		long long price;
		long long gain;
		int slot;

		TPlayerBotArmourUpgradeLook() : when(0), vnum(0), price(0), gain(0), slot(-1)
		{
		}
	};
	std::map<DWORD, TPlayerBotArmourUpgradeLook> s_mapPlayerBotArmourUpgradeLooks;
	DWORD s_dwPlayerBotArmourUpgradePicks = 0;
	DWORD s_dwPlayerBotMerchantArmourUpgrades = 0;

	// What a score gain is worth for its price: score points a million yang.
	long long GetPlayerBotArmourValuePerYang(long long score, long long ownedScore, long long price)
	{
		const long long gain = score - std::max<long long>(0, ownedScore);
		return gain <= 0 ? 0 : gain * 1000000LL / std::max<long long>(1, price);
	}

	// Whether a stand of the bot's first village holds an armour it would buy
	// and could pay for now - by the cheapest line of each piece and the
	// score of a clean copy of it (GetPlayerBotArmourProtoScore), the most
	// score a yang buys first. Read again every PLAYERBOT_ARMOUR_UPGRADE_LOOK_MS,
	// on the weapon look's clock.
	const TPlayerBotArmourUpgradeLook& LookPlayerBotArmourUpgrade(LPCHARACTER ch, DWORD dwNow)
	{
		TPlayerBotArmourUpgradeLook& look = s_mapPlayerBotArmourUpgradeLooks[ch->GetPlayerID()];
		if (look.when != 0 && dwNow - look.when < PLAYERBOT_ARMOUR_UPGRADE_LOOK_MS)
			return look;
		look.when = dwNow != 0 ? dwNow : 1;
		look.vnum = 0;
		look.price = 0;
		look.gain = 0;
		look.slot = -1;
		if (!ch->IsItemLoaded() || s_mapPlayerBotArmourLineCheapest.empty())
			return look;
		const long long spare = GetPlayerBotArmourUpgradeSpare(ch);
		if (GetPlayerBotArmourUpgradeShare(spare, PLAYERBOT_ARMOUR_UPGRADE_BIG_GAIN_PERCENT) <= 0)
			return look;
		const long firstVillage = playerbot_empire_rules::GetHomeMap(ch->GetEmpire(),
				playerbot_empire_rules::MAP_ROLE_M1);
		if (firstVillage <= 0)
			return look;
		TPlayerBotOwnedArmour owned;
		ReadPlayerBotBestOwnedArmour(ch, owned);
		long long bestValue = 0;
		const unsigned long long lo = PlayerBotMarketLocalKey(firstVillage, 0);
		const unsigned long long hi = PlayerBotMarketLocalKey(firstVillage, 0xFFFFFFFFU);
		for (std::map<unsigned long long, long long>::const_iterator it = s_mapPlayerBotArmourLineCheapest.lower_bound(lo);
				it != s_mapPlayerBotArmourLineCheapest.end() && it->first <= hi; ++it)
		{
			const DWORD vnum = (DWORD)(it->first & 0xFFFFFFFFULL);
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
			if (!IsPlayerBotArmourProtoFor(ch, proto))
				continue;
			const int slot = GetPlayerBotArmourUpgradeSlot(proto->bSubType);
			const long long ownedScore = owned.item[slot] ? owned.score[slot] : 0;
			const long long score = GetPlayerBotArmourProtoScore(ch, proto);
			if (owned.item[slot] && score * 100 < ownedScore * (100 + PLAYERBOT_ARMOUR_UPGRADE_MIN_GAIN_PERCENT))
				continue;
			const long long gain = GetPlayerBotArmourGainPercent(score, ownedScore);
			if (it->second > GetPlayerBotArmourUpgradeShare(spare, gain))
				continue;
			const long long value = GetPlayerBotArmourValuePerYang(score, ownedScore, it->second);
			if (value < bestValue || (value == bestValue && look.vnum != 0 && slot >= look.slot))
				continue;
			bestValue = value;
			look.vnum = vnum;
			look.price = it->second;
			look.gain = gain;
			look.slot = slot;
		}
		return look;
	}

	bool PlayerBotWantsArmourUpgradeFromMarket(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded() || IsPlayerBotGearFrozen(ch))
			return false;
		return LookPlayerBotArmourUpgrade(ch, get_dword_time()).vnum != 0;
	}

	// Defined in playerbot_economy.h: the slot a burned piece is being
	// replaced in from the market (Iwakura's Patch 4, point 7).
	bool IsPlayerBotRebuildingFromMarket(LPCHARACTER ch, BYTE slot);

	// The village merchants' best piece of one slot for this bot, when the
	// slot is empty or the piece beats the best one owned by the upgrade's
	// share, and half of the purse over PLAYERBOT_WEAPON_UPGRADE_MERCHANT_FLOOR
	// pays for it - the weapon's merchant rule.
	DWORD FindPlayerBotMerchantArmourUpgrade(LPCHARACTER ch, int slot, const TPlayerBotOwnedArmour& owned,
			long long* priceOut)
	{
		if (priceOut)
			*priceOut = 0;
		if (!ch || slot < 0 || slot >= PLAYERBOT_ARMOUR_UPGRADE_SLOTS)
			return 0;
		static const DWORD merchants[] = { 9001, 9002, 9003 };
		const long long ownedScore = owned.item[slot] ? owned.score[slot] : 0;
		DWORD bestVnum = 0;
		long long bestScore = 0;
		for (size_t i = 0; i < sizeof(merchants) / sizeof(merchants[0]); ++i)
		{
			LPSHOP shop = CShopManager::instance().GetByNPCVnum(merchants[i]);
			if (!shop)
				continue;
			const std::vector<CShop::SHOP_ITEM>& offers = shop->GetItemVector();
			for (size_t k = 0; k < offers.size(); ++k)
			{
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(offers[k].vnum);
				if (!IsPlayerBotArmourProtoFor(ch, proto) || GetPlayerBotArmourUpgradeSlot(proto->bSubType) != slot)
					continue;
				const long long score = GetPlayerBotArmourProtoScore(ch, proto);
				if (score <= bestScore ||
						(owned.item[slot] && score * 100 < ownedScore * (100 + PLAYERBOT_ARMOUR_UPGRADE_MIN_GAIN_PERCENT)))
					continue;
				const long long price = GetPlayerBotMerchantOfferPrice(offers[k], proto);
				if (price * 2 > (long long)ch->GetGold() - (long long)GetPlayerBotReservedGold(ch) -
						PLAYERBOT_WEAPON_UPGRADE_MERCHANT_FLOOR)
					continue;
				bestVnum = offers[k].vnum;
				bestScore = score;
				if (priceOut)
					*priceOut = price;
			}
		}
		return bestVnum;
	}

	bool PlayerBotWantsMerchantArmourUpgrade(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded() || IsPlayerBotGearFrozen(ch))
			return false;
		TPlayerBotOwnedArmour owned;
		ReadPlayerBotBestOwnedArmour(ch, owned);
		for (int slot = 0; slot < PLAYERBOT_ARMOUR_UPGRADE_SLOTS; ++slot)
		{
			if (IsPlayerBotRebuildingFromMarket(ch, PLAYERBOT_ARMOUR_UPGRADE_WEARS[slot]))
				continue;
			const DWORD vnum = FindPlayerBotMerchantArmourUpgrade(ch, slot, owned, NULL);
			const TItemTable* proto = vnum != 0 ? ITEM_MANAGER::instance().GetTable(vnum) : NULL;
			// Room for it, or the visit would be made for a purchase that
			// cannot happen (BuyPlayerBotProgressionGear asks the same).
			if (proto && ch->GetEmptyInventory(std::max(1, (int)proto->bSize)) >= 0)
				return true;
		}
		return false;
	}

	// At the armour merchant, after the ladder: the merchants' best piece of
	// every slot it betters, body armour first.
	bool BuyPlayerBotMerchantArmourUpgrades(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded() || IsPlayerBotGearFrozen(ch))
			return false;
		TPlayerBotOwnedArmour owned;
		ReadPlayerBotBestOwnedArmour(ch, owned);
		bool bought = false;
		for (int slot = 0; slot < PLAYERBOT_ARMOUR_UPGRADE_SLOTS; ++slot)
		{
			if (IsPlayerBotRebuildingFromMarket(ch, PLAYERBOT_ARMOUR_UPGRADE_WEARS[slot]))
				continue;
			const DWORD vnum = FindPlayerBotMerchantArmourUpgrade(ch, slot, owned, NULL);
			if (vnum == 0 || !BuyPlayerBotProgressionGear(ch, vnum, "armour upgrade"))
				continue;
			++s_dwPlayerBotMerchantArmourUpgrades;
			bought = true;
		}
		return bought;
	}

	// Once a report interval with the weapon census: how many bots' last look
	// found an armour on their first village's stands they would buy now, of
	// which slot, and how many went for one since the last report.
	void ReportPlayerBotArmourUpgrades(DWORD dwNow)
	{
		DWORD fresh = 0, wanting = 0;
		DWORD bySlot[PLAYERBOT_ARMOUR_UPGRADE_SLOTS] = { 0 };
		long long gainSum = 0;
		for (std::map<DWORD, TPlayerBotArmourUpgradeLook>::iterator it = s_mapPlayerBotArmourUpgradeLooks.begin();
				it != s_mapPlayerBotArmourUpgradeLooks.end(); )
		{
			if (it->second.when == 0 || dwNow - it->second.when >= 4 * PLAYERBOT_ARMOUR_UPGRADE_LOOK_MS)
			{
				s_mapPlayerBotArmourUpgradeLooks.erase(it++);
				continue;
			}
			++fresh;
			if (it->second.vnum != 0)
			{
				++wanting;
				gainSum += std::min<long long>(it->second.gain, 1000);
				if (it->second.slot >= 0 && it->second.slot < PLAYERBOT_ARMOUR_UPGRADE_SLOTS)
					++bySlot[it->second.slot];
			}
			++it;
		}
		sys_log(0, "PLAYERBOT_GEAR_UPGRADE: armour census looked=%u affordable_upgrade=%u avg_gain=%lld%% body=%u head=%u shield=%u wrist=%u foots=%u neck=%u ear=%u lines_priced=%u picks=%u merchant_buys=%u",
				fresh, wanting, wanting ? gainSum / wanting : 0LL,
				bySlot[0], bySlot[1], bySlot[2], bySlot[3], bySlot[4], bySlot[5], bySlot[6],
				(unsigned int)s_mapPlayerBotArmourLineCheapest.size(), s_dwPlayerBotArmourUpgradePicks,
				s_dwPlayerBotMerchantArmourUpgrades);
		s_dwPlayerBotArmourUpgradePicks = 0;
		s_dwPlayerBotMerchantArmourUpgrades = 0;
	}
}

#endif
