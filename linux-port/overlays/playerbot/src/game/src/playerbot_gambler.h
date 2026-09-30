#ifndef __INC_METIN2_PLAYERBOT_GAMBLER_H__
#define __INC_METIN2_PLAYERBOT_GAMBLER_H__

// Iwakura's gambler (Hazardzista, "SYSTEM OSOBOWOSCI v2.0", 19 September): the
// bot's investment mode. With a heavy purse, its own gear done and no party, a
// bot may turn the end of a town visit into an evening at the anvil: pieces
// from the storekeeper and the bag, each with a risk of its own - +7 six times
// in ten, +8 three times, +9 once - worked at the plain blacksmith up to +7 and
// under nothing but the Blessing Scroll past it, until forty percent of the
// purse it came in with is spent or a piece reaches +9. What it makes is goods
// for the counter, never gear to wear: its own gear is the Perfectionist's.
//
// The rules are playerbot_persona_rules.h (RollGambleTarget, NextGambleStep,
// BudgetLeft), unit-tested; this file is the engine's half. It runs inside the
// town visit - ContinuePlayerBotVisitAsGambler turns a visit that has finished
// its errands into the walk to the storekeeper and the anvil, and the
// blacksmith's two phases call ManagePlayerBotGamble - because a visit is what
// every other pass (the stall, the market, the road) already stands back for;
// a session begun after the visit had ended would have been walked away from
// the anvil before it reached it.
//
// Iwakura's Community Patch 5, point 1 (27 September) took the Hazardzista out
// ("dzialala slabo lub wcale i byla trudna do zbalansowania"): no bot turns
// gambler by nature, by the town trigger or by a roll at the end of a visit
// any more. The session is the addict's (Nalogowiec) and the four rare
// gamblers' now - Mlodszy, Starszy, Naczelny and Szalony Hazardzista, drawn
// among the richest characters (playerbot_rare_persona.h), each with its
// terms (playerbot_persona::GetGamblerTerms). A gambler looks at its bag:
// the category of his list it holds most pieces of is what it works, two
// that tie are worked together, the Szalony works all four; with no piece
// in its bag it buys one to six at +0..+5 in a drawn category off the
// counters first. Each piece goes to +7 seven times in ten, +8 and +9
// fifteen each, under a Blessing Scroll for the steps to +7..+9 when it has
// one and at the plain anvil when it has not, until its share of the purse
// is spent; what comes off the anvil better than its gear it wears, and the
// rest - with the piece it took off - goes on its offline counter.
//
// An implementation fragment in the sense playerbot_types.h describes. Include
// it exactly once, after playerbot_town.h, which declares what it asks of it.

namespace
{
	bool IsPlayerBotGambling(const TPlayerBotAIState& state, DWORD dwNow)
	{
		return state.persona.bGambling && dwNow < state.persona.dwGambleUntil;
	}

	// Which of Community Patch 5's four gamblers the bot is now, or 0.
	BYTE GetPlayerBotRareGamblerKind(const TPlayerBotPersona& p, DWORD dwNow)
	{
		const BYTE rare = GetPlayerBotRareNow(p, dwNow);
		return playerbot_persona::IsRareGambler(rare) ? rare : 0;
	}

	// Whose session this is now that the Hazardzista is gone: the addict's or
	// one of the four gamblers'.
	bool IsPlayerBotSessionGambler(const TPlayerBotPersona& p, DWORD dwNow)
	{
		return IsPlayerBotRareNow(p, playerbot_persona::RARE_NALOGOWIEC, dwNow) ||
				GetPlayerBotRareGamblerKind(p, dwNow) != 0;
	}

	// Iwakura's Patch 3, point 7: the addict (Nalogowiec) stakes 85 percent of
	// its purse where the gambler staked forty; Community Patch 5's four stake
	// 80, 70, 60 and 90.
	int GetPlayerBotGambleBudgetPercent(const TPlayerBotPersona& p, DWORD dwNow)
	{
		const BYTE kind = GetPlayerBotRareGamblerKind(p, dwNow);
		if (kind != 0)
			return playerbot_persona::GetGamblerTerms(kind).budgetPercent;
		return IsPlayerBotRareNow(p, playerbot_persona::RARE_NALOGOWIEC, dwNow)
				? PLAYERBOT_NALOGOWIEC_BUDGET_PERCENT : playerbot_persona::GAMBLE_BUDGET_PERCENT;
	}

	// The jewellery and boots of his list for the four gamblers, as the +0
	// vnum of each family (read off world.item_proto; "Kolczyki Z Niebian.
	// Lez" and "Bransol. Z Niebian.Lez" are cut short there). Two of them -
	// the copper earrings of level 8 and the silver bracelet of 15 - are under
	// the category's twenty-two, and count because he names them.
	const DWORD PLAYERBOT_GAMBLER_LISTED_JEWELS[] = {
		17020, // Miedziane Kolczyki
		17060, // Zlote Kolczyki
		17080, // Jadeitowe Kolczyki
		17100, // Ebonitowe Kolczyki
		17120, // Perlowe Kolczyki
		17140, // Kolczyki Z Bial. Zlota
		17160, // Krysztalowe Kolczyki
		17200, // Kolczyki Z Niebian. Lez
		14040, // Srebrna Bransoleta
		14100, // Ebonitowa Bransoleta
		14140, // Bransol. Z Bial. Zlota
		14200, // Bransol. Z Niebian. Lez
		16060, // Zloty Naszyjnik
		16080, // Jadeitowy Naszyjnik
		16120, // Perlowy Naszyjnik
		15080, // Skorzane Kozaki
		15120, // Buty z Brazu
		15140, // Jadeitowe Buty
		15160, // Ekstazyjne Buty
		15200, // Buty Feniksa
	};

	bool IsPlayerBotGamblerListedJewel(LPITEM item)
	{
		if (!item)
			return false;
		const BYTE refine = item->GetRefineLevel();
		if (refine > 9 || item->GetVnum() < refine)
			return false;
		const DWORD base = item->GetVnum() - refine;
		for (size_t i = 0; i < sizeof(PLAYERBOT_GAMBLER_LISTED_JEWELS) / sizeof(PLAYERBOT_GAMBLER_LISTED_JEWELS[0]); ++i)
			if (PLAYERBOT_GAMBLER_LISTED_JEWELS[i] == base)
				return true;
		return false;
	}

	// The category of his list a piece is in (playerbot_persona::EGambleCategory),
	// or -1: a weapon a hand swings - not an arrow, a lance or a quiver - a
	// body armour, a shield or a helmet, and the four pieces of jewellery.
	int GetPlayerBotGambleCategory(LPITEM item)
	{
		if (!item)
			return -1;
		if (item->GetType() == ITEM_WEAPON)
		{
			switch (item->GetSubType())
			{
				case WEAPON_SWORD:
				case WEAPON_DAGGER:
				case WEAPON_BOW:
				case WEAPON_TWO_HANDED:
				case WEAPON_BELL:
				case WEAPON_FAN:
					return playerbot_persona::GAMBLE_CAT_WEAPON;
				default:
					return -1;
			}
		}
		if (item->GetType() != ITEM_ARMOR)
			return -1;
		switch (item->GetSubType())
		{
			case ARMOR_BODY: return playerbot_persona::GAMBLE_CAT_ARMOUR;
			case ARMOR_SHIELD:
			case ARMOR_HEAD: return playerbot_persona::GAMBLE_CAT_SHIELD_HELMET;
			case ARMOR_NECK:
			case ARMOR_WRIST:
			case ARMOR_EAR:
			case ARMOR_FOOTS: return playerbot_persona::GAMBLE_CAT_JEWEL;
			default: return -1;
		}
	}

	// A piece the four gamblers could work, judged by the item alone, in the
	// categories of `mask`: his "predefiniowana baza" - every weapon from
	// thirty, body armour from twenty-six, shield and helmet from twenty-one,
	// and the jewellery his list names - or, for the share that takes a piece
	// off it (bGambleOffList), any jewel from twenty-two. What it buys
	// (`purchase`) is a weapon of level thirty for PLAYERBOT_RARE_GAMBLE_
	// LEVEL30_PERCENT of the gamblers and of a higher level for the rest,
	// who take one of thirty where the market has nothing higher. The
	// operator's rules stay: a player's gift, a scroll-only weapon and a
	// prize line are never staked.
	bool IsPlayerBotRareGambleStock(LPCHARACTER ch, LPITEM item, const TPlayerBotPersona& p, BYTE mask, bool purchase)
	{
		if (!ch || !item || IsPlayerBotSidekickGift(ch, item))
			return false;
		const int category = GetPlayerBotGambleCategory(item);
		if (category < 0 || (mask & (1u << category)) == 0)
			return false;
		if (item->GetRefinedVnum() == 0 || item->GetRefineLevel() >= 9)
			return false;
		const int level = (int)GetPlayerBotPersonaLevelLimit(item);
		if (category == playerbot_persona::GAMBLE_CAT_JEWEL)
		{
			if (!IsPlayerBotGamblerListedJewel(item) &&
					(!p.bGambleOffList || level < playerbot_persona::GambleCategoryMinLevel(category)))
				return false;
		}
		else if (level < playerbot_persona::GambleCategoryMinLevel(category))
			return false;
		if (purchase && category == playerbot_persona::GAMBLE_CAT_WEAPON && !p.bGambleOffList &&
				!p.bGambleWeaponHigh && level != playerbot_persona::GambleCategoryMinLevel(category))
			return false;
		return !IsPlayerBotScrollOnlyWeapon(item) && !IsPlayerBotPrizeItem(item);
	}

	// What the gamblers did and why the rest did not start, for the census
	// every ten minutes (ReportPlayerBotGambleCensus): "czy strategia
	// hazardzisty sie odpala" (Iwakura, 23 September) is a question the log
	// could only answer one session at a time.
	// noBases is a visit whose bag held no base at all and whose box no gear
	// of the list. Of the visits that went on with no base the anvil could
	// work there and then, fromBox had none in the bag - the storekeeper
	// gives them - and unworkable had bases short of their materials, the fee
	// or a scroll, which the counters may give (ManagePlayerBotGambleMarket).
	// marketLines are the lines a session bought on the counters on its way
	// to the anvil, basesBought the bases a gambler bought between sessions.
	struct TPlayerBotGambleCensus
	{
		unsigned int started, ended, attempts, finished, burned, nines;
		long long spent;
		unsigned int notHere, noBases;
		unsigned int fromBox, unworkable, marketLines, basesBought;
		TPlayerBotGambleCensus() : started(0), ended(0), attempts(0), finished(0), burned(0), nines(0),
			spent(0), notHere(0), noBases(0),
			fromBox(0), unworkable(0), marketLines(0), basesBought(0) {}
	};
	TPlayerBotGambleCensus s_PlayerBotGambleCensus;

	// The list's own word on a finished piece: at the gambler's +7 or past it,
	// or worked by a session (playerbot_lpp.h, which comes later in the
	// include order).
	bool IsPlayerBotLppFinished(const TPlayerBotPersona& p, LPITEM item);
	// And whether a bot is a gambler by nature (playerbot_lpp.h).
	bool IsPlayerBotGamblerByNature(DWORD pid, const TPlayerBotAIState& state);

	// A piece a session worked on is goods from then on: the list keeps it
	// no longer and no session takes it again.
	bool IsPlayerBotGambleForSale(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return false;
		TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		return st != s_mapPlayerBotAIStates.end() &&
				st->second.persona.setGambleForSale.find(item->GetID()) != st->second.persona.setGambleForSale.end();
	}

	// Iwakura's price of this piece at another refine, on his curve, or zero
	// when his sheet does not carry the family - GetPlayerBotGearAskingBase at
	// a refine of the caller's choosing. What the session stakes and loses is
	// counted in it: a burn costs the piece, a scroll's failure the grade.
	DWORD GetPlayerBotGambleValueAt(LPITEM item, int refine)
	{
		if (!item || refine < 0 || refine > 9 ||
				(item->GetType() != ITEM_WEAPON && item->GetType() != ITEM_ARMOR))
			return 0;
		const BYTE now = item->GetRefineLevel();
		if (now > 9 || (DWORD)now > item->GetVnum())
			return 0;
		const DWORD baseVnum = item->GetVnum() - now;
		for (size_t i = 0; i < sizeof(PLAYERBOT_GEAR_PRICES) / sizeof(PLAYERBOT_GEAR_PRICES[0]); ++i)
			if (PLAYERBOT_GEAR_PRICES[i].dwBaseVnum == baseVnum)
			{
				const DWORD price = PLAYERBOT_GEAR_PRICES[i].adwPrice[refine];
				if (price == 0)
					return 0;
				return ScalePlayerBotIwakuraPrice((DWORD)((unsigned long long)price *
						(unsigned long long)GetPlayerBotSocketStonePercent(item) / 100ULL));
			}
		return 0;
	}

	// Iwakura's Patch 3, point 2: nothing under level thirty at the gambler's
	// anvil ("aby wyeliminowac sytuacje ulepszania ekwipunku na 1. poziom"),
	// a body armour from eighteen and earrings from twenty-two. The level is
	// the item's own limit. The list asks it too (ClassifyPlayerBotLppItem):
	// what the gambler will not work is no stock of the gambler's.
	int GetPlayerBotGambleMinItemLevel(LPITEM item)
	{
		if (item && item->GetType() == ITEM_ARMOR)
		{
			if (item->GetSubType() == ARMOR_BODY)
				return PLAYERBOT_GAMBLE_MIN_ARMOUR_LEVEL;
			if (item->GetSubType() == ARMOR_EAR)
				return PLAYERBOT_GAMBLE_MIN_EARRING_LEVEL;
		}
		return PLAYERBOT_GAMBLE_MIN_ITEM_LEVEL;
	}

	bool IsPlayerBotGambleLevelOk(LPITEM item)
	{
		return item && (int)GetPlayerBotPersonaLevelLimit(item) >= GetPlayerBotGambleMinItemLevel(item);
	}

	// A piece the gambler could work, judged by the item alone - which is all
	// a piece in the safebox has. The LPP's "cenny ekwipunek": a family his tier
	// list rates PLAYERBOT_GAMBLE_MIN_TIER or better in PvE or PvP, or the body
	// armour, helmets and shields he judges by level; and his sheet must price
	// it higher at +9 than as it is. Not what the operator already rules on: a
	// level-30 weapon has a ladder of its own, a scroll-only weapon never meets
	// the plain anvil, a prize line is not staked, and level-one gear is
	// merchant scrap under +8 whatever the anvil makes of it.
	bool IsPlayerBotGambleStock(LPCHARACTER ch, LPITEM item)
	{
		// What a player handed a companion is not stock to gamble with
		// (playerbot_sidekick.h).
		if (!ch || !item || IsPlayerBotSidekickGift(ch, item))
			return false;
		const BYTE type = item->GetType();
		if (type != ITEM_WEAPON && type != ITEM_ARMOR)
			return false;
		if (item->GetRefinedVnum() == 0 || item->GetRefineLevel() >= 9)
			return false;
		if (!IsPlayerBotGambleLevelOk(item))
			return false;
		if (IsPlayerBotSpecialLevel30Weapon(item) || IsPlayerBotScrollOnlyWeapon(item) ||
				IsPlayerBotPrizeItem(item) || IsPlayerBotMerchantOnlyGear(item))
			return false;
		if (IsPlayerBotLowLevelGear(item) &&
				GetPlayerBotLowGearMinRefine(item) > playerbot_persona::GAMBLE_SAFE_PLUS)
			return false;
		const DWORD valueNow = GetPlayerBotGambleValueAt(item, item->GetRefineLevel());
		const DWORD valueTop = GetPlayerBotGambleValueAt(item, 9);
		if (valueTop == 0 || valueTop <= valueNow)
			return false;
		const bool judgedByLevel = type == ITEM_ARMOR && (item->GetSubType() == ARMOR_BODY ||
				item->GetSubType() == ARMOR_HEAD || item->GetSubType() == ARMOR_SHIELD);
		if (!judgedByLevel)
		{
			const DWORD baseVnum = item->GetVnum() - item->GetRefineLevel();
			const int tier = std::max(GetPlayerBotItemTier(baseVnum, -1, false),
					GetPlayerBotItemTier(baseVnum, -1, true));
			if (tier < PLAYERBOT_GAMBLE_MIN_TIER)
				return false;
		}
		return true;
	}

	// And one in this bot's bag it may work now. Its own gear stays its own:
	// what the equipment pass is about to put on, the spare the blacksmith is
	// making into one, the backup weapon, the level-30 weapon it is grinding,
	// the dagger it breaks stones with, anything the ordinary refine pass is
	// raising. What the junk rule sends to the merchant is not worth a fee.
	bool IsPlayerBotGambleBase(LPCHARACTER ch, LPITEM item, LPITEM backup)
	{
		if (!ch || !item)
			return false;
		TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		const TPlayerBotPersona* persona = st != s_mapPlayerBotAIStates.end() ? &st->second.persona : NULL;
		// Community Patch 5's four judge a piece by his list and by the
		// categories their session chose - all four before it has.
		const BYTE kind = persona ? GetPlayerBotRareGamblerKind(*persona, get_dword_time()) : 0;
		if (kind != 0)
		{
			const BYTE mask = persona->bGambling && persona->bGambleCategories != 0
					? persona->bGambleCategories : playerbot_persona::GAMBLE_CAT_ALL;
			if (!IsPlayerBotRareGambleStock(ch, item, *persona, mask, false))
				return false;
		}
		else if (!IsPlayerBotGambleStock(ch, item))
			return false;
		if (item->IsEquipped() || item->isLocked() || item->IsExchanging() ||
				item->GetOwner() != ch || item->GetWindow() != INVENTORY ||
				item->GetCell() >= PLAYERBOT_BAG_CELLS || ch->GetInventoryItem(item->GetCell()) != item)
			return false;
		// Nor the Stalki it keeps for the level ahead (playerbot_stalki.h): the
		// gambler's anvil burns what it fails on, and that piece is the bot's
		// next armour or weapon, not its stake.
		if (item == backup || IsPlayerBotKeptBackupArmour(ch, item) ||
				IsPlayerBotWearableUpgrade(ch, item, item->GetCell()) ||
				IsPlayerBotHigherTierSpare(ch, item) || IsPlayerBotLevel30Project(ch, item) ||
				IsPlayerBotArcherStoneWeapon(ch, item) || IsPlayerBotRefineBagCandidate(ch, item) ||
				IsPlayerBotKeptStalki(ch, item))
			return false;
		// What an earlier session made is for sale as it is. The set that says
		// so lives in the process, and after a restart it is empty: a piece at
		// the gambler's +7 or past it (IsPlayerBotLppFinished) is taken as made
		// unless a plan of this session is working it, or the first session
		// after a restart took its own +7s and +8s back to the anvil (B22 of
		// Iwakura's audit of 26 September).
		if (IsPlayerBotGambleForSale(ch, item))
			return false;
		if (persona && IsPlayerBotLppFinished(*persona, item))
		{
			bool underWay = false;
			const std::vector<TPlayerBotGamblePlan>& plans = persona->vecGamblePlans;
			for (size_t i = 0; i < plans.size() && !underWay; ++i)
				underWay = plans[i].dwItemId == item->GetID() && !plans[i].bDone;
			if (!underWay)
				return false;
		}
		// The four work what they bought for the anvil whatever the junk rule
		// would make of it, and that rule leaves their bases alone
		// (IsPlayerBotRareGambleHeldBase) - it is not asked here, or the two
		// would ask each other.
		return kind != 0 || !IsPlayerBotJunkItem(ch, item);
	}

	// A piece one of the four gamblers holds for its session, which the junk
	// rule and the counter leave alone until the anvil has had it. Nothing
	// here asks the junk rule back.
	bool IsPlayerBotRareGambleHeldBase(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->IsEquipped() || (item->GetType() != ITEM_WEAPON && item->GetType() != ITEM_ARMOR))
			return false;
		TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (st == s_mapPlayerBotAIStates.end())
			return false;
		const TPlayerBotPersona& p = st->second.persona;
		if (GetPlayerBotRareGamblerKind(p, get_dword_time()) == 0 || p.setGambleForSale.count(item->GetID()))
			return false;
		const BYTE mask = p.bGambling && p.bGambleCategories != 0 ? p.bGambleCategories : playerbot_persona::GAMBLE_CAT_ALL;
		return IsPlayerBotRareGambleStock(ch, item, p, mask, false) && item->GetRefineLevel() < playerbot_persona::GAMBLE_SAFE_PLUS;
	}

	void CollectPlayerBotGambleBases(LPCHARACTER ch, std::vector<LPITEM>& out)
	{
		out.clear();
		if (!ch || !ch->IsItemLoaded())
			return;
		LPITEM backup = FindPlayerBotBackupWeapon(ch);
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetCell() == cell && IsPlayerBotGambleBase(ch, item, backup))
				out.push_back(item);
		}
	}

	// What a base's plain steps consume from its grade to GAMBLE_SAFE_PLUS, by
	// the recipes of the grades on the way (item_proto's refined vnum and
	// refine set): this world's +0 to +4 ask for yang alone, and from +4 each
	// step one or two materials, one or two of each. The next step's alone was
	// all a session knew of, so a base at +4 came to the anvil with the one
	// material for +5 and was set aside at +5.
	void AddPlayerBotGambleChainNeeds(LPITEM item, std::map<DWORD, int>& need)
	{
		if (!item)
			return;
		const TItemTable* proto = item->GetProto();
		int plus = item->GetRefineLevel();
		for (int step = 0; proto && proto->dwRefinedVnum != 0 && plus < (int)playerbot_persona::GAMBLE_SAFE_PLUS &&
				step < 10; ++step, ++plus)
		{
			const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(proto->wRefineSet);
			if (!recipe)
				break;
			for (int m = 0; m < recipe->material_count; ++m)
				if (recipe->materials[m].vnum != 0 && recipe->materials[m].count > 0)
					need[recipe->materials[m].vnum] += recipe->materials[m].count;
			proto = ITEM_MANAGER::instance().GetTable(proto->dwRefinedVnum);
		}
	}

	// What the plain steps of every base in the bag consume to the gambler's
	// +7: the storekeeper's half of "ulepszacze" (WithdrawPlayerBotSafebox)
	// and what a session or the addict wants off the counters
	// (WantsPlayerBotGambleOffer).
	void CollectPlayerBotGambleMaterials(LPCHARACTER ch, std::set<DWORD>& out)
	{
		out.clear();
		std::vector<LPITEM> bases;
		CollectPlayerBotGambleBases(ch, bases);
		std::map<DWORD, int> need;
		for (size_t i = 0; i < bases.size(); ++i)
			AddPlayerBotGambleChainNeeds(bases[i], need);
		for (std::map<DWORD, int>::const_iterator it = need.begin(); it != need.end(); ++it)
			out.insert(it->first);
	}

	// What of that the bag lacks, for the session's walk along the counters
	// (ManagePlayerBotGambleMarket): the bases in the order the anvil takes
	// them, the most valuable at +7 first, and the materials of the first base
	// whose steps - with those of the bases before it - the bag cannot cover,
	// so the purse goes into one piece the anvil can finish rather than a
	// step of each. Held means over what the Biologist is owed and the bot's
	// own anvil keeps back, the spare PlayerBotGambleHasMaterials works with.
	void CollectPlayerBotGambleMissingMaterials(LPCHARACTER ch, std::map<DWORD, int>& missing)
	{
		missing.clear();
		std::vector<LPITEM> bases;
		CollectPlayerBotGambleBases(ch, bases);
		std::vector<std::pair<DWORD, LPITEM> > order;
		for (size_t i = 0; i < bases.size(); ++i)
			order.push_back(std::make_pair(GetPlayerBotGambleValueAt(bases[i], playerbot_persona::GAMBLE_SAFE_PLUS),
					bases[i]));
		std::sort(order.begin(), order.end(),
				[](const std::pair<DWORD, LPITEM>& a, const std::pair<DWORD, LPITEM>& b) { return a.first > b.first; });
		std::map<DWORD, int> need;
		for (size_t i = 0; i < order.size() && missing.empty(); ++i)
		{
			AddPlayerBotGambleChainNeeds(order[i].second, need);
			for (std::map<DWORD, int>::const_iterator it = need.begin(); it != need.end(); ++it)
			{
				const int spare = (int)ch->CountSpecifyItem(it->first) - GetPlayerBotBiologistReserve(ch, it->first) -
						GetPlayerBotRefineMaterialReserve(ch, it->first);
				if (it->second > spare)
					missing[it->first] = it->second - std::max(0, spare);
			}
		}
	}

	// The materials a step consumes, if the bag holds them over what the
	// Biologist is still owed and what the bot's own anvil keeps back - the
	// gambler works with what is spare, never with the Perfectionist's stock.
	// Their worth on Iwakura's sheet goes to the budget like the fee.
	bool PlayerBotGambleHasMaterials(LPCHARACTER ch, const TRefineTable* recipe, long long& value)
	{
		value = 0;
		if (!ch || !recipe)
			return false;
		for (int i = 0; i < recipe->material_count; ++i)
		{
			const DWORD vnum = recipe->materials[i].vnum;
			const int need = recipe->materials[i].count;
			if (vnum == 0 || need <= 0)
				continue;
			const int spare = (int)ch->CountSpecifyItem(vnum) - GetPlayerBotBiologistReserve(ch, vnum) -
					GetPlayerBotRefineMaterialReserve(ch, vnum);
			if (spare < need)
				return false;
			value += (long long)need * (long long)GetPlayerBotMaterialAskingBase(vnum);
		}
		return true;
	}

	// "Wylacznie ze Zwojow Blogoslawienstwa": 25040 and nothing else, and only
	// what the bot's own scroll work does not keep back (GetPlayerBotRefineScrollKeep).
	int FindPlayerBotGambleScrollCell(LPCHARACTER ch)
	{
		if (!ch || CountPlayerBotSafeRefineScrolls(ch) - GetPlayerBotRefineScrollKeep(ch) <= 0)
			return -1;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM scroll = ch->GetInventoryItem(cell);
			if (scroll && scroll->GetCell() == cell && !scroll->isLocked() &&
					scroll->GetVnum() == PLAYERBOT_BLESSING_SCROLL_VNUM)
				return cell;
		}
		return -1;
	}

	// Whether the anvil could do anything with this base right now: the next
	// step's recipe, its materials spare, and for a step past +7 a Blessing
	// Scroll to go under. The session is only begun for a bag that holds one:
	// the first version began it for any base and a third of the sessions
	// were a walk to the storekeeper and the anvil for nothing - every piece
	// stood at +4, where this world's recipes start asking for materials.
	bool IsPlayerBotGambleWorkable(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return false;
		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
		if (!recipe)
			return false;
		long long materials = 0;
		if (!PlayerBotGambleHasMaterials(ch, recipe, materials))
			return false;
		if ((long long)ch->GetGold() - (long long)GetPlayerBotReservedGold(ch) <
				(long long)ch->ComputeRefineFee(recipe->cost))
			return false;
		return item->GetRefineLevel() < playerbot_persona::GAMBLE_SAFE_PLUS ||
				FindPlayerBotGambleScrollCell(ch) >= 0;
	}

	TPlayerBotGamblePlan* FindPlayerBotGamblePlan(TPlayerBotPersona& p, DWORD itemId)
	{
		for (size_t i = 0; i < p.vecGamblePlans.size(); ++i)
			if (p.vecGamblePlans[i].dwItemId == itemId)
				return &p.vecGamblePlans[i];
		return NULL;
	}

	// How many pieces fit for the anvil the bag holds in each of his four
	// categories, for the choice of a session's (ChooseGambleCategories).
	void CountPlayerBotRareGambleBases(LPCHARACTER ch, unsigned int counts[playerbot_persona::GAMBLE_CAT_COUNT])
	{
		for (int i = 0; i < playerbot_persona::GAMBLE_CAT_COUNT; ++i)
			counts[i] = 0;
		std::vector<LPITEM> bases;
		CollectPlayerBotGambleBases(ch, bases);
		for (size_t i = 0; i < bases.size(); ++i)
		{
			const int category = GetPlayerBotGambleCategory(bases[i]);
			if (category >= 0)
				++counts[category];
		}
	}

	// Whether one of the four, its bag empty of his list when it began, is
	// still buying its bases: fewer than it meant to, inside
	// PLAYERBOT_RARE_GAMBLE_BUY_WINDOW_MS, and with its share of the purse
	// not spent. After that its session starts with what it got.
	bool IsPlayerBotRareGamblerBuying(const TPlayerBotPersona& p, DWORD dwNow)
	{
		return GetPlayerBotRareGamblerKind(p, dwNow) != 0 && !p.bGambling && p.bRareStage == 0 &&
				p.bRareBought < p.bRareBuyWant &&
				(DWORD)(dwNow - p.dwRareSince) < PLAYERBOT_RARE_GAMBLE_BUY_WINDOW_MS &&
				playerbot_persona::BudgetLeft(p.llRareGoldStart, GetPlayerBotGambleBudgetPercent(p, dwNow),
						p.llRareSpent) > 0;
	}

	void EndPlayerBotGamble(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow, const char* reason)
	{
		TPlayerBotPersona& p = state.persona;
		if (!p.bGambling)
			return;
		p.bGambling = false;
		p.bGambleMarketPending = false;
		p.dwGambleMarketUntil = 0;
		const bool addict = IsPlayerBotRareNow(p, playerbot_persona::RARE_NALOGOWIEC, dwNow);
		const BYTE kind = GetPlayerBotRareGamblerKind(p, dwNow);
		// Community Patch 5, point 9: the four put their green stones on what
		// they have just refined - a weapon or a body armour of forty and
		// under - before it goes on the counter or on their back
		// (ApplyPlayerBotGreenBonusToItem: a few stones a call, false once it
		// spends none).
		if (kind != 0 && ch)
			for (size_t i = 0; i < p.vecGamblePlans.size(); ++i)
			{
				LPITEM made = ITEM_MANAGER::instance().Find(p.vecGamblePlans[i].dwItemId);
				if (!made || made->GetOwner() != ch || made->GetWindow() != INVENTORY)
					continue;
				for (int round = 0; round < PLAYERBOT_RARE_GAMBLE_GREEN_ROUNDS &&
						ApplyPlayerBotGreenBonusToItem(ch, state, made, dwNow); ++round)
					;
			}
		// Every piece the session took is for sale now, finished or stopped
		// short: the list used to keep the best copy of a family - the highest
		// plus - so what the anvil had just made went back to the storekeeper
		// and the plain copies to the counter ("boty dobrze skladuja eq ale nie
		// ulepszaja na sell", Iwakura, 23 September).
		for (size_t i = 0; i < p.vecGamblePlans.size(); ++i)
		{
			// The addict and the four wear what came off the anvil better than
			// their own gear ("jesli ulepszony ekwipunek jest lepszy od jego
			// obecnego - zaklada go"); the rest, worse or another class's, is
			// for sale.
			if ((addict || kind != 0) && ch)
			{
				LPITEM made = ITEM_MANAGER::instance().Find(p.vecGamblePlans[i].dwItemId);
				if (made && made->GetOwner() == ch && made->GetWindow() == INVENTORY &&
						IsPlayerBotWearableUpgrade(ch, made, made->GetCell()))
				{
					// Community Patch 5, point 1: "jesli ma gorszy ekwipunek
					// podmienia go i reszta (ulepszone przedmioty + stary
					// zalozony) ida na sklep offline" - the piece it takes off
					// goes on the counter with the rest, not to the merchant.
					if (kind != 0)
					{
						const int wearCell = made->FindEquipCell(ch);
						LPITEM worn = wearCell >= 0 ? ch->GetWear((BYTE)wearCell) : NULL;
						if (worn)
						{
							if (p.setGambleForSale.size() >= 64)
								p.setGambleForSale.erase(p.setGambleForSale.begin());
							p.setGambleForSale.insert(worn->GetID());
						}
					}
					continue;
				}
			}
			if (p.setGambleForSale.size() >= 64)
				p.setGambleForSale.erase(p.setGambleForSale.begin());
			p.setGambleForSale.insert(p.vecGamblePlans[i].dwItemId);
		}
		++s_PlayerBotGambleCensus.ended;
		s_PlayerBotGambleCensus.attempts += p.wGambleAttempts;
		s_PlayerBotGambleCensus.finished += p.bGambleFinished;
		s_PlayerBotGambleCensus.burned += p.bGambleBurned;
		s_PlayerBotGambleCensus.nines += p.bGambleNines;
		s_PlayerBotGambleCensus.spent += p.llGambleSpent;
		p.dwNextGambleAt = dwNow + number(PLAYERBOT_GAMBLE_REST_MIN_MS, PLAYERBOT_GAMBLE_REST_MAX_MS);
		p.dwNextDecide = 0;
		const long long budget = p.llGambleGoldStart * GetPlayerBotGambleBudgetPercent(p, dwNow) / 100;
		sys_log(0, "PLAYERBOT_PERSONA: gambler done pid=%u name=%s reason=%s spent=%lld budget=%lld attempts=%u set_aside=%u burned=%u downgraded=%u nines=%u gold=%lld addict=%d kind=%s",
				ch ? ch->GetPlayerID() : 0, ch ? ch->GetName() : "?", reason ? reason : "?",
				p.llGambleSpent, budget, (unsigned int)p.wGambleAttempts,
				(unsigned int)p.bGambleFinished, (unsigned int)p.bGambleBurned,
				(unsigned int)p.bGambleDowngraded, (unsigned int)p.bGambleNines,
				ch ? (long long)ch->GetGold() : 0LL, addict ? 1 : 0,
				kind != 0 ? GetPlayerBotPersonaName(playerbot_persona::GetRareRule(kind).persona) : "-");
		if (addict || kind != 0)
			p.llRareSpent = p.llGambleSpent;
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		// "Bot aktualizuje swoj sklep offline": the stand is served as soon as
		// the visit lets the bot go, so what came off the anvil goes up.
		if ((p.bGambleFinished > 0 || kind != 0) && state.offlineShop.nextService > dwNow + 5000)
			state.offlineShop.nextService = dwNow + 5000;
#endif
		p.vecGamblePlans.clear();
	}

	// The session's walk from the end of a visit whose errands are done: the
	// storekeeper and the anvil, by the same phases as any visit.
	void SendPlayerBotVisitToGambleAnvil(LPCHARACTER ch, TPlayerBotAIState& state)
	{
		const long mapIndex = ch->GetMapIndex();
		state.bTownNeedMisc = false;
		state.bTownNeedWeaponMerchant = false;
		state.bTownNeedArmorMerchant = false;
		state.bTownNeedTrainer = false;
		state.bTownNeedSkillReset = false;
		state.bTownNeedSafebox = true;
		state.bTownNeedBlacksmith = true;
		const bool bDirect = IsPlayerBotM2Map(mapIndex) || !IsPlayerBotGatedVillage(mapIndex);
		if (bDirect)
			state.bTownVisitPhase = GetPlayerBotFirstDirectTownPhase(state);
		else
		{
			// A walled village's visit ends outside the gate, where its
			// storekeeper stands; the anvil is inside.
			state.bTownVisitPhase = GetPlayerBotFirstExteriorTownPhase(state);
			if (state.bTownVisitPhase == BOT_TOWN_PHASE_NONE)
				state.bTownVisitPhase = BOT_TOWN_PHASE_GATE_IN;
		}
		state.dwTownWaitUntil = 0;
		ClearPlayerBotRoute(state, true);
	}

	// A session's clock, purse and counters. Only a rare personality sits at
	// the anvil for its own sake now, so the budget counts from the purse the
	// state began with, and what it bought off the counters since is already
	// spent from it.
	void BeginPlayerBotGambleSession(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow, const char* how,
			size_t pieces, size_t workable)
	{
		TPlayerBotPersona& p = state.persona;
		const BYTE kind = GetPlayerBotRareGamblerKind(p, dwNow);
		const long long gold = (long long)ch->GetGold();
		++s_PlayerBotGambleCensus.started;
		p.bGambling = true;
		p.llGambleGoldStart = p.llRareGoldStart > 0 ? p.llRareGoldStart : gold;
		p.llGambleSpent = p.llRareSpent;
		p.bRareStage = 1;
		p.dwGambleUntil = dwNow + (kind != 0 ? PLAYERBOT_RARE_GAMBLE_MAX_MS : PLAYERBOT_GAMBLE_MAX_MS);
		p.dwNextGambleStep = 0;
		p.bGambleNines = 0;
		p.bGambleBurned = 0;
		p.bGambleFinished = 0;
		p.bGambleDowngraded = 0;
		p.wGambleAttempts = 0;
		p.bGambleSafeboxChecked = false;
		p.bGambleSafeboxTaken = 0;
		p.bGambleMarketPending = true;
		p.bGambleMarketBuys = 0;
		p.dwGambleMarketUntil = 0;
		p.vecGamblePlans.clear();
		p.dwNextDecide = 0;
		sys_log(0, "PLAYERBOT_PERSONA: gambler begins pid=%u name=%s kind=%s gold=%lld budget=%lld pieces=%u workable=%u categories=%u bought=%u/%u map=%ld how=%s",
				ch->GetPlayerID(), ch->GetName(),
				kind != 0 ? GetPlayerBotPersonaName(playerbot_persona::GetRareRule(kind).persona) : "Nalogowiec",
				gold, p.llGambleGoldStart * GetPlayerBotGambleBudgetPercent(p, dwNow) / 100,
				(unsigned int)pieces, (unsigned int)workable, (unsigned int)p.bGambleCategories,
				(unsigned int)p.bRareBought, (unsigned int)p.bRareBuyWant, ch->GetMapIndex(), how ? how : "?");
	}

	// Community Patch 5's four in a village: the session begins when the bag
	// holds pieces of his list and the shopping for them is over. The
	// categories are chosen here, from the bag ("przed wybraniem kategorii,
	// bot sprawdza swoj ekwipunek"). True when it has begun; the caller walks
	// the visit to the storekeeper and the anvil, or - at the start of a visit
	// (StartPlayerBotTownVisit) - lets the visit's own list take it there.
	bool BeginPlayerBotRareGambleSession(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !IsPlayerBotPersonaEnabled() || !ch->IsItemLoaded())
			return false;
		TPlayerBotPersona& p = state.persona;
		const BYTE kind = GetPlayerBotRareGamblerKind(p, dwNow);
		if (kind == 0 || !p.bRestored || p.bGambling || p.bRareStage != 0 ||
				!IsPlayerBotVillageMap(ch->GetMapIndex()) || ch->GetParty() || IsPlayerBotRareGamblerBuying(p, dwNow))
			return false;
		unsigned int counts[playerbot_persona::GAMBLE_CAT_COUNT];
		CountPlayerBotRareGambleBases(ch, counts);
		const uint8_t mask = playerbot_persona::ChooseGambleCategories(counts,
				playerbot_persona::GetGamblerTerms(kind).everything);
		if (mask == 0)
			return false;
		p.bGambleCategories = mask;
		std::vector<LPITEM> bases;
		CollectPlayerBotGambleBases(ch, bases);
		size_t workable = 0;
		for (size_t i = 0; i < bases.size(); ++i)
			if (IsPlayerBotGambleWorkable(ch, bases[i]))
				++workable;
		BeginPlayerBotGambleSession(ch, state, dwNow, "bag", bases.size(), workable);
		return true;
	}

	// The end of a visit that did its errands: the moment the addict or one of
	// the four goes on to the storekeeper and the anvil instead of ending -
	// ended, the visit hands the bot to the stall, the market and the road,
	// and any of them would walk it away from the anvil before it got there.
	// Nobody else: Community Patch 5, point 1 took the Hazardzista out, and
	// with it the roll by character, the town trigger (community patch 2,
	// point 10) and the gambler by nature between its sessions.
	bool ContinuePlayerBotVisitAsGambler(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !IsPlayerBotPersonaEnabled())
			return false;
		TPlayerBotPersona& p = state.persona;
		if (!p.bRestored || p.bGambling)
			return false;
		if (GetPlayerBotRareGamblerKind(p, dwNow) != 0)
		{
			if (!BeginPlayerBotRareGambleSession(ch, state, dwNow))
				return false;
			SendPlayerBotVisitToGambleAnvil(ch, state);
			return true;
		}
		// Iwakura's Patch 3, point 7: the addict goes to the anvil at the end
		// of every visit, past the rest, the purse, its own gear and the roll -
		// the village and the party are all it asks.
		if (!IsPlayerBotRareNow(p, playerbot_persona::RARE_NALOGOWIEC, dwNow))
			return false;
		const long mapIndex = ch->GetMapIndex();
		// "Nie jest aktualnie przypisany do zadnej aktywnej grupy (PT)"; and a
		// dropper is a drop character, whose purse is its counter's.
		if (!IsPlayerBotVillageMap(mapIndex) || ch->GetParty() || IsPlayerBotDropper(state.bPersonality))
		{
			++s_PlayerBotGambleCensus.notHere;
			return false;
		}
		std::vector<LPITEM> bases;
		CollectPlayerBotGambleBases(ch, bases);
		size_t workable = 0;
		for (size_t i = 0; i < bases.size(); ++i)
			if (IsPlayerBotGambleWorkable(ch, bases[i]))
				++workable;
		// Iwakura's answer of 26 September: a session "zabiera baze z magazynu
		// do ekwipunku ... i kupuje ulepszacze na rynku i ulepsza". So a box
		// holding gear starts one as bases in the bag do, and so do bases whose
		// materials the bag lacks: the storekeeper first, then the counters
		// (ManagePlayerBotGambleMarket), then the anvil.
		const bool boxStock = (p.bLppStoredKnown && p.wLppBoxGearKept > 0) || !p.mapLppStored.empty();
		if (bases.empty() && !boxStock)
		{
			++s_PlayerBotGambleCensus.noBases;
			return false;
		}
		if (workable == 0)
			++(bases.empty() ? s_PlayerBotGambleCensus.fromBox : s_PlayerBotGambleCensus.unworkable);
		BeginPlayerBotGambleSession(ch, state, dwNow, "addict", bases.size(), workable);
		SendPlayerBotVisitToGambleAnvil(ch, state);
		return true;
	}

	// One step at the anvil, a player's click every PLAYERBOT_GAMBLE_STEP_*_MS,
	// from the blacksmith's two phases. True when a refine was attempted.
	bool ManagePlayerBotGamble(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return false;
		TPlayerBotPersona& p = state.persona;
		if (!p.bGambling)
			return false;
		if (!IsPlayerBotPersonaEnabled())
		{
			EndPlayerBotGamble(ch, state, dwNow, "switch_off");
			return false;
		}
		if (dwNow >= p.dwGambleUntil)
		{
			EndPlayerBotGamble(ch, state, dwNow, "time");
			return false;
		}
		if (state.bTownVisitPhase != BOT_TOWN_PHASE_BLACKSMITH &&
				state.bTownVisitPhase != BOT_TOWN_PHASE_BLACKSMITH_WAIT)
			return false;
		if (!ch->IsItemLoaded() || ch->IsDead() || ch->GetMyShop() || ch->GetExchange())
			return false;
		if (dwNow < p.dwNextGambleStep)
			return false;
		p.dwNextGambleStep = dwNow + number(PLAYERBOT_GAMBLE_STEP_MIN_MS, PLAYERBOT_GAMBLE_STEP_MAX_MS);

		const long long left = playerbot_persona::BudgetLeft(p.llGambleGoldStart,
				GetPlayerBotGambleBudgetPercent(p, dwNow), p.llGambleSpent);
		if (left <= 0)
		{
			EndPlayerBotGamble(ch, state, dwNow, "budget");
			return false;
		}
		const BYTE kind = GetPlayerBotRareGamblerKind(p, dwNow);

		// The piece on the anvil: the one under way, else the most valuable
		// base not yet worked this session - its plan is rolled as it is taken.
		std::vector<LPITEM> bases;
		CollectPlayerBotGambleBases(ch, bases);
		LPITEM item = NULL;
		TPlayerBotGamblePlan* plan = NULL;
		for (size_t i = 0; i < bases.size() && !item; ++i)
		{
			TPlayerBotGamblePlan* known = FindPlayerBotGamblePlan(p, bases[i]->GetID());
			if (known && !known->bDone)
			{
				item = bases[i];
				plan = known;
			}
		}
		if (!item)
		{
			DWORD bestValue = 0;
			for (size_t i = 0; i < bases.size(); ++i)
			{
				if (FindPlayerBotGamblePlan(p, bases[i]->GetID()))
					continue;
				const DWORD value = GetPlayerBotGambleValueAt(bases[i], playerbot_persona::GAMBLE_SAFE_PLUS);
				if (!item || value > bestValue)
				{
					item = bases[i];
					bestValue = value;
				}
			}
			if (item)
			{
				TPlayerBotGamblePlan fresh;
				fresh.dwItemId = item->GetID();
				// Community Patch 5's four: +7 seven times in ten, +8 and +9
				// fifteen each; the addict keeps the old 60/30/10.
				fresh.bTarget = kind != 0 ? playerbot_persona::RollRareGambleTarget((uint32_t)number(0, 99))
						: playerbot_persona::RollGambleTarget((uint32_t)number(0, 99));
				fresh.bScrollFailed = false;
				fresh.bDone = false;
				fresh.bScrollFails = 0;
				p.vecGamblePlans.push_back(fresh);
				plan = &p.vecGamblePlans.back();
				sys_log(0, "PLAYERBOT_PERSONA: gambler takes pid=%u name=%s vnum=%u plus=%u target=%u value_at_7=%u left=%lld",
						ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)item->GetRefineLevel(),
						(unsigned int)plan->bTarget, bestValue, left);
			}
		}
		if (!item || !plan)
		{
			EndPlayerBotGamble(ch, state, dwNow, "no_pieces");
			return false;
		}

		const BYTE plus = item->GetRefineLevel();
		// The four take a Blessing Scroll for the steps to +7..+9 while the bag
		// holds one and go on at the plain anvil when it does not; the addict
		// works to +7 plainly and past it under nothing but a scroll.
		const playerbot_persona::EGambleStep step = kind != 0
				? playerbot_persona::NextRareGambleStep(plus, plan->bTarget, FindPlayerBotGambleScrollCell(ch) >= 0)
				: playerbot_persona::NextGambleStep(plus, plan->bTarget, plan->bScrollFailed);
		if (step == playerbot_persona::GAMBLE_STEP_DONE)
		{
			// For sale as it stands: at its target, or back at +7 after a
			// scroll failed on it.
			plan->bDone = true;
			++p.bGambleFinished;
			sys_log(0, "PLAYERBOT_PERSONA: gambler sets aside pid=%u name=%s vnum=%u plus=%u target=%u scroll_failed=%d",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)plus,
					(unsigned int)plan->bTarget, plan->bScrollFailed ? 1 : 0);
			p.dwNextGambleStep = dwNow + 500;
			return false;
		}

		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
		if (!recipe)
		{
			plan->bDone = true;
			return false;
		}
		const long long fee = (long long)ch->ComputeRefineFee(recipe->cost);
		long long materialsValue = 0;
		if (!PlayerBotGambleHasMaterials(ch, recipe, materialsValue))
		{
			// Not with the Perfectionist's stock: the piece is sold as it is.
			plan->bDone = true;
			PlayerBotLogThrottled("gamble_no_materials", dwNow,
					"PLAYERBOT_PERSONA: gambler lacks spare materials pid=%u name=%s vnum=%u plus=%u",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)plus);
			return false;
		}
		if ((long long)ch->GetGold() - (long long)GetPlayerBotReservedGold(ch) < fee)
		{
			EndPlayerBotGamble(ch, state, dwNow, "gold");
			return false;
		}
		int scrollCell = -1;
		long long scrollValue = 0;
		if (step == playerbot_persona::GAMBLE_STEP_SCROLL)
		{
			scrollCell = FindPlayerBotGambleScrollCell(ch);
			if (scrollCell < 0)
			{
				// No scroll, no +8: "wylacznie ze Zwojow Blogoslawienstwa".
				plan->bDone = true;
				++p.bGambleFinished;
				sys_log(0, "PLAYERBOT_PERSONA: gambler has no blessing scroll pid=%u name=%s vnum=%u plus=%u target=%u",
						ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)plus,
						(unsigned int)plan->bTarget);
				return false;
			}
			scrollValue = (long long)GetPlayerBotMaterialAskingBase(PLAYERBOT_BLESSING_SCROLL_VNUM);
		}

		// The stake: the fee, the materials, the scroll, and what a failure
		// takes - the whole piece at the plain anvil, a grade under a scroll.
		// It has to fit in what is left of the forty percent ("nigdy nie
		// stawia wszystkiego na jedna karte"); a piece whose step would not
		// is sold as it is and the next one is tried.
		const long long valueNow = std::max<long long>(GetPlayerBotGambleValueAt(item, plus),
				(long long)GetPlayerBotRefineInvestment(item));
		const long long valueDown = plus > 0 ? (long long)GetPlayerBotGambleValueAt(item, plus - 1) : 0;
		const long long failLoss = scrollCell >= 0 ? std::max<long long>(0, valueNow - valueDown) : valueNow;
		if (fee + materialsValue + scrollValue + failLoss > left)
		{
			plan->bDone = true;
			if (plus >= playerbot_persona::GAMBLE_SAFE_PLUS)
				++p.bGambleFinished;
			sys_log(0, "PLAYERBOT_PERSONA: gambler will not stake pid=%u name=%s vnum=%u plus=%u stake=%lld left=%lld",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)plus,
					fee + materialsValue + scrollValue + failLoss, left);
			return false;
		}

		// What the recipe asks and what the bag holds, before the attempt
		// takes it - the same field the refine pass writes.
		char materials[128] = "none";
		{
			size_t used = 0;
			for (int m = 0; m < recipe->material_count && used + 24 < sizeof(materials); ++m)
				used += snprintf(materials + used, sizeof(materials) - used, "%s%u:%d/%d",
						m ? "," : "", (unsigned int)recipe->materials[m].vnum, (int)recipe->materials[m].count,
						(int)ch->CountSpecifyItem(recipe->materials[m].vnum));
		}

		// The engine puts what comes off the anvil in the cell the piece left:
		// the next grade, the grade under it after a scroll, or nothing after a
		// burn. The piece itself is gone after the call either way.
		const WORD cell = item->GetCell();
		const DWORD itemId = item->GetID();
		const DWORD oldVnum = item->GetVnum();
		const DWORD nextVnum = item->GetRefinedVnum();
		const long long goldBefore = (long long)ch->GetGold();
		bool attempted = false;
		if (scrollCell >= 0)
		{
			ch->SetRefineMode(scrollCell);
			attempted = ch->DoRefineWithScroll(item);
			ch->ClearRefineMode();
		}
		else
			attempted = ch->DoRefine(item, false);
		item = NULL;
		if (!attempted)
		{
			plan->bDone = true;
			sys_log(0, "PLAYERBOT_AI: refine SKIPPED pid=%u name=%s vnum=%u plus=%u materials=%s gamble=1 (requirements/state)",
					ch->GetPlayerID(), ch->GetName(), oldVnum, (unsigned int)plus, materials);
			return false;
		}
		++p.wGambleAttempts;

		LPITEM after = ch->GetInventoryItem(cell);
		const long long paid = std::max<long long>(0, goldBefore - (long long)ch->GetGold());
		long long lost = 0;
		const char* outcome = "FAILED_KEPT";
		const bool success = after && after->GetVnum() == nextVnum;
		if (success)
		{
			outcome = "SUCCESS";
			plan->dwItemId = after->GetID();
		}
		else if (!after)
		{
			outcome = "FAILED_BURNED";
			lost = valueNow;
			plan->bDone = true;
			++p.bGambleBurned;
			NotePlayerBotMoodRefineFailure(ch, (int)plus + 1, "burned");
		}
		else if (after->GetID() != itemId)
		{
			// A scroll's failure: the grade under it, in the same cell. The
			// four work the piece on, PLAYERBOT_RARE_GAMBLE_SCROLL_FAILS times
			// at most; the addict sells it as it stands.
			outcome = "FAILED_DOWNGRADED";
			lost = failLoss;
			plan->dwItemId = after->GetID();
			if (kind != 0)
			{
				if (++plan->bScrollFails >= PLAYERBOT_RARE_GAMBLE_SCROLL_FAILS)
				{
					plan->bDone = true;
					++p.bGambleFinished;
				}
			}
			else
				plan->bScrollFailed = true;
			++p.bGambleDowngraded;
			NotePlayerBotMoodRefineFailure(ch, (int)plus + 1, "downgraded");
		}
		// A refined, downgraded or burned piece is not the item the stall
		// registries remember by id.
		if (!after || after->GetID() != itemId)
		{
			state.mapStallUnsold.erase(itemId);
			state.mapStockFirstListed.erase(itemId);
		}
		const long long spent = paid + materialsValue + (scrollCell >= 0 ? scrollValue : 0) + lost;
		p.llGambleSpent += spent;
		sys_log(0, "PLAYERBOT_AI: refine %s pid=%u name=%s old_vnum=%u new_vnum=%u plus=%u scroll=%d materials=%s gamble=1 target=%u spent=%lld left=%lld",
				outcome, ch->GetPlayerID(), ch->GetName(), oldVnum, nextVnum, (unsigned int)plus + 1,
				scrollCell >= 0 ? 1 : 0, materials, (unsigned int)plan->bTarget, spent,
				std::max<long long>(0, left - spent));

		if (success)
		{
			BroadcastPlayerBotRefineSuccess(ch, nextVnum, (int)plus + 1);
			// +8 and +9 are Iwakura's euphoria (playerbot_mood.h).
			NotePlayerBotMoodRefine(ch, (int)plus + 1);
			// "Wielki sukces (+9)": the addict's appetite is sated, whatever is
			// left of the budget, and the masterpiece goes to the counter. The
			// four go on to the next piece ("bot stara sie ulepszac przedmioty
			// po kolei, do wyczerpania limitu finansowego").
			if (plus + 1 >= 9)
			{
				plan->bDone = true;
				++p.bGambleNines;
				++p.bGambleFinished;
				if (kind == 0)
					EndPlayerBotGamble(ch, state, dwNow, "nine");
			}
		}
		return true;
	}

	// Every ten minutes: the sessions and what they made, and why the visits
	// that could have turned gambler did not - the gates in the order they are
	// asked, each visit counted at the first one that stopped it.
	void ReportPlayerBotGambleCensus(DWORD dwNow)
	{
		static DWORD s_dwReported = 0;
		if (s_dwReported != 0 && dwNow - s_dwReported < 600000)
			return;
		const bool first = s_dwReported == 0;
		s_dwReported = dwNow;
		if (!first && IsPlayerBotPersonaEnabled())
		{
			const TPlayerBotGambleCensus& c = s_PlayerBotGambleCensus;
			sys_log(0, "PLAYERBOT_PERSONA: gambler census started=%u ended=%u attempts=%u finished=%u burned=%u nines=%u spent=%lld not_started: not_here=%u no_bases=%u begun: from_box=%u unworkable=%u market_lines=%u bases_bought=%u",
					c.started, c.ended, c.attempts, c.finished, c.burned, c.nines, c.spent,
					c.notHere, c.noBases, c.fromBox, c.unworkable, c.marketLines, c.basesBought);
		}
		s_PlayerBotGambleCensus = TPlayerBotGambleCensus();
	}

	// A gambler by nature between its sessions, short of pieces to work: the
	// market's half of "Jesli brakuje mu bazy lub ulepszaczy, przeszukuje
	// sklepy offline na rynku i skupuje je po najnizszych cenach". The session
	// itself is a town visit - the storekeeper, then the anvil - and a bot on a
	// visit does not browse, so only the addict ever read the counters for a
	// base: of the 1 823 pieces of gear the bots bought off them on m2zip on 25
	// and 26 September, one was a body armour at +0..+3. The bag's bases and
	// the gear the box keeps count together, so a box of plain pieces is never
	// topped up from the counters, and a box no visit has seen since the start
	// is not guessed at. Any plus the gambler works (IsPlayerBotGambleStock):
	// "hazardzisci maja normalnie kupowac przedmioty na rynek bez wzgledu na
	// +" (Iwakura, 26 September). The answer is kept a minute, because the
	// browse asks it of every line it reads.
	std::map<DWORD, std::pair<DWORD, bool> > s_mapPlayerBotGamblerWantsBases;

	bool PlayerBotGamblerWantsBases(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded() || !IsPlayerBotPersonaEnabled())
			return false;
		const DWORD dwNow = get_dword_time();
		std::pair<DWORD, bool>& cached = s_mapPlayerBotGamblerWantsBases[ch->GetPlayerID()];
		if (cached.first != 0 && dwNow - cached.first < PLAYERBOT_GAMBLE_BASES_RECHECK_MS)
			return cached.second;
		cached.first = dwNow ? dwNow : 1;
		cached.second = false;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotAIStates.end())
			return false;
		const TPlayerBotAIState& state = it->second;
		if (IsPlayerBotGambling(state, dwNow) ||
				IsPlayerBotRareNow(state.persona, playerbot_persona::RARE_NALOGOWIEC, dwNow) ||
				!state.persona.bLppStoredKnown || !IsPlayerBotGamblerByNature(ch->GetPlayerID(), state) ||
				(long long)ch->GetGold() < (long long)ScalePlayerBotIwakuraPrice(PLAYERBOT_GAMBLE_MIN_PURSE_BASE))
			return false;
		std::vector<LPITEM> bases;
		CollectPlayerBotGambleBases(ch, bases);
		cached.second = (int)bases.size() + (int)state.persona.wLppBoxGearKept < PLAYERBOT_GAMBLE_MARKET_BASES;
		return cached.second;
	}

	// One of the four with no piece of his list in its bag, buying: an offer
	// of the category it drew, at +0..+5 ("kupuje od 1 do 3 przedmiotow (od +0
	// do +5) spelniajacych kryteria"), while it has bought fewer than it meant
	// to (IsPlayerBotRareGamblerBuying).
	bool IsPlayerBotRareGamblerBaseOffer(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !offer || !IsPlayerBotPersonaEnabled() ||
				(offer->GetType() != ITEM_WEAPON && offer->GetType() != ITEM_ARMOR) ||
				offer->GetRefineLevel() > PLAYERBOT_RARE_GAMBLE_BASE_MAX_PLUS)
			return false;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotAIStates.end())
			return false;
		const TPlayerBotPersona& p = it->second.persona;
		return IsPlayerBotRareGamblerBuying(p, get_dword_time()) &&
				IsPlayerBotRareGambleStock(ch, offer, p, (BYTE)(1u << p.bGambleBuyCategory), true);
	}

	// An offer such a gambler would buy as a base (PlayerBotGamblerWantsBases),
	// or one of the four would (IsPlayerBotRareGamblerBaseOffer).
	bool IsPlayerBotGamblerBaseOffer(LPCHARACTER ch, LPITEM offer)
	{
		if (IsPlayerBotRareGamblerBaseOffer(ch, offer))
			return true;
		return ch && offer && (offer->GetType() == ITEM_WEAPON || offer->GetType() == ITEM_ARMOR) &&
				PlayerBotGamblerWantsBases(ch) && IsPlayerBotGambleStock(ch, offer);
	}

	// A base bought: the census counts it, the next question recounts the bag,
	// and one of the four counts it against the bases it meant to buy.
	void NotePlayerBotGambleBaseBought(LPCHARACTER ch, DWORD vnum, long long paid)
	{
		if (!ch)
			return;
		++s_PlayerBotGambleCensus.basesBought;
		s_mapPlayerBotGamblerWantsBases.erase(ch->GetPlayerID());
		BYTE bought = 0, want = 0;
		TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (it != s_mapPlayerBotAIStates.end() && GetPlayerBotRareGamblerKind(it->second.persona, get_dword_time()) != 0)
		{
			TPlayerBotPersona& p = it->second.persona;
			if (p.bRareBought < 255)
				++p.bRareBought;
			bought = p.bRareBought;
			want = p.bRareBuyWant;
		}
		sys_log(0, "PLAYERBOT_PERSONA: gambler bought a base pid=%u name=%s vnum=%u paid=%lld gold=%lld bought=%u/%u",
				ch->GetPlayerID(), ch->GetName(), vnum, paid, (long long)ch->GetGold(),
				(unsigned int)bought, (unsigned int)want);
	}

	// The gambler's second source, after the storekeeper: the counters. "Jesli
	// brakuje mu bazy lub ulepszaczy, przeszukuje sklepy offline na rynku i
	// skupuje je po najnizszych cenach" - what it wants of an offer is a base
	// it could work (the item's own test; the bag's run at the anvil) while it
	// has fewer than it means to have, or a material one of its pieces' next
	// steps consumes. The market's own rules price it.
	bool WantsPlayerBotGambleOffer(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !offer || !IsPlayerBotPersonaEnabled())
			return false;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		const DWORD dwNow = get_dword_time();
		if (it == s_mapPlayerBotAIStates.end())
			return false;
		const TPlayerBotPersona& p = it->second.persona;
		// The addict and the four buy what their anvil will want before
		// their session too ("wlacznie z kupowaniem ulepszaczy i itemow do
		// +6"), out of the purse they began the state with.
		const bool gambling = IsPlayerBotGambling(it->second, dwNow);
		const bool addict = IsPlayerBotRareNow(p, playerbot_persona::RARE_NALOGOWIEC, dwNow);
		const BYTE kind = GetPlayerBotRareGamblerKind(p, dwNow);
		// Between the sessions, a gambler short of pieces buys a base and
		// nothing else (PlayerBotGamblerWantsBases).
		if (!gambling && !addict && kind == 0)
			return IsPlayerBotGamblerBaseOffer(ch, offer);
		const long long start = gambling ? p.llGambleGoldStart : p.llRareGoldStart;
		const long long spent = gambling ? p.llGambleSpent : p.llRareSpent;
		if (playerbot_persona::BudgetLeft(start, GetPlayerBotGambleBudgetPercent(p, dwNow), spent) <= 0)
			return false;
		if (offer->GetType() == ITEM_WEAPON || offer->GetType() == ITEM_ARMOR)
		{
			// One of the four buys its bases before its session, of the
			// category it drew, and gear for nothing else.
			if (kind != 0)
				return !gambling && IsPlayerBotRareGamblerBaseOffer(ch, offer);
			if (addict && offer->GetRefineLevel() > PLAYERBOT_NALOGOWIEC_BASE_MAX_PLUS)
				return false;
			std::vector<LPITEM> bases;
			CollectPlayerBotGambleBases(ch, bases);
			return (int)bases.size() < (addict ? PLAYERBOT_NALOGOWIEC_MARKET_BASES : PLAYERBOT_GAMBLE_MARKET_BASES) &&
					IsPlayerBotGambleStock(ch, offer);
		}
		std::set<DWORD> materials;
		CollectPlayerBotGambleMaterials(ch, materials);
		return materials.find(offer->GetVnum()) != materials.end();
	}

	// Whether the addict still looks for bases on the counters before its
	// session: nothing else sends it to a market, and with no base in the bag
	// its session never starts (no_bases) - BohenHleba, drawn on m2zip on
	// 24 September, visited three merchants and no anvil in a quarter of an hour.
	// What is left of the addict's 85 percent - or of one of the four's share
	// - or 0 for any other bot. They pay for their anvil's bases and
	// materials out of this, not out of the share of the median wallet an
	// ordinary buyer is held to: at a yang rate of 3000% that share was 0.8
	// million against bases of several.
	long long GetPlayerBotAddictBudgetLeft(LPCHARACTER ch)
	{
		if (!ch || !IsPlayerBotPersonaEnabled())
			return 0;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotAIStates.end())
			return 0;
		const DWORD dwNow = get_dword_time();
		const TPlayerBotPersona& p = it->second.persona;
		if (!IsPlayerBotSessionGambler(p, dwNow))
			return 0;
		const bool gambling = IsPlayerBotGambling(it->second, dwNow);
		const long long left = playerbot_persona::BudgetLeft(gambling ? p.llGambleGoldStart : p.llRareGoldStart,
				GetPlayerBotGambleBudgetPercent(p, dwNow), gambling ? p.llGambleSpent : p.llRareSpent);
		return std::max(0LL, left);
	}

	bool PlayerBotAddictWantsBases(LPCHARACTER ch)
	{
		if (GetPlayerBotAddictBudgetLeft(ch) <= 0)
			return false;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotAIStates.end() || IsPlayerBotGambling(it->second, get_dword_time()))
			return false;
		// One of the four buys only while its bag held none of his list when
		// it began (IsPlayerBotRareGamblerBuying).
		if (GetPlayerBotRareGamblerKind(it->second.persona, get_dword_time()) != 0)
			return IsPlayerBotRareGamblerBuying(it->second.persona, get_dword_time());
		std::vector<LPITEM> bases;
		CollectPlayerBotGambleBases(ch, bases);
		return (int)bases.size() < PLAYERBOT_NALOGOWIEC_MARKET_BASES;
	}

	// One of the four that wants a village: its bases to buy or its session to
	// begin (BeginPlayerBotRareGambleSession). The travel pass takes it home
	// for that (NeedsPlayerBotCriticalTownServices) and the tick begins a visit
	// on a village map (`sessionOnly`: the bases are in the bag and the
	// shopping is over).
	bool PlayerBotRareGamblerWantsTown(LPCHARACTER ch, bool sessionOnly)
	{
		if (!ch || !ch->IsItemLoaded() || ch->GetParty() || !IsPlayerBotPersonaEnabled())
			return false;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotAIStates.end())
			return false;
		const DWORD dwNow = get_dword_time();
		const TPlayerBotPersona& p = it->second.persona;
		if (GetPlayerBotRareGamblerKind(p, dwNow) == 0 || p.bGambling || p.bRareStage != 0)
			return false;
		if (IsPlayerBotRareGamblerBuying(p, dwNow))
			return !sessionOnly;
		std::vector<LPITEM> bases;
		CollectPlayerBotGambleBases(ch, bases);
		return !bases.empty();
	}

	// Anything a gambler buys comes out of the session's budget, the fees and
	// the burned pieces beside it ("Kazdy zakup ulepszacza, oplata u Kowala,
	// zuzycie Zwoju Blogoslawienstwa czy spalenie przedmiotu bezposrednio
	// obciaza wydzielony budzet").
	void NotePlayerBotGamblePurchase(LPCHARACTER ch, long long paid)
	{
		if (!ch || paid <= 0)
			return;
		TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotAIStates.end())
			return;
		if (IsPlayerBotGambling(it->second, get_dword_time()))
		{
			it->second.persona.llGambleSpent += paid;
			// A line of the session's walk along the counters
			// (ManagePlayerBotGambleMarket).
			if (it->second.bTownVisitPhase == BOT_TOWN_PHASE_GAMBLE_MARKET)
				++s_PlayerBotGambleCensus.marketLines;
		}
		else if (IsPlayerBotSessionGambler(it->second.persona, get_dword_time()))
			it->second.persona.llRareSpent += paid;
	}
}

#endif
