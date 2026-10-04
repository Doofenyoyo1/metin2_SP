#ifndef __INC_METIN2_PLAYERBOT_ECONOMY_H__
#define __INC_METIN2_PLAYERBOT_ECONOMY_H__

// What a bot does with money and with the contents of its bag: deciding what is
// junk, selling it to the right merchant, upgrading gear at the blacksmith,
// rerolling bonus lines, and running a market stall of its own.
//
// The one rule worth knowing before changing anything here: IsPlayerBotJunkItem
// **defaults to true**. Anything worth keeping needs an explicit exemption, or
// bots vendor it on their next trip to town.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once,
// after playerbot_gear.h - it prices and sells what that file decides to wear.

#if defined(PLAYERBOT_ENGINE_MT2009)
// MT2009_PLUS_BOT_HAIR_V1: the costume sets' lists (item.cpp,
// server-patches/playerqol, MT2009_PLUS_COSTUME_SET_V1's costume_sets.txt).
bool Mt2009PlusIsCostumeSetHair(DWORD hairVnum);
bool Mt2009PlusIsCostumeSetPair(DWORD bodyVnum, DWORD hairVnum);
#endif

namespace
{
	// Defined with the market-stall code, which comes later because it needs
	// the town. Refining announces a good result the moment it happens, so it
	// cannot wait for that file.
	void BroadcastPlayerBotRefineSuccess(LPCHARACTER ch, DWORD resultVnum, int newPlus);
	// Defined beside HasPlayerBotRefineOpportunity; the blacksmith pass asks it
	// before taking a worn piece off for the anvil.
	bool CanPlayerBotAttemptRefineItem(LPCHARACTER ch, LPITEM item);
	bool CanPlayerBotPayRefineStep(LPCHARACTER ch, LPITEM item);
	// Defined beside them too; the armour merchant asks it, and its weapon
	// twin the weapon merchant (R8 of Iwakura's audit).
	bool NeedsPlayerBotBackupArmour(LPCHARACTER ch);
	bool NeedsPlayerBotBackupWeapon(LPCHARACTER ch);
	// MT2009_PLUS_AWAKENING_V1: defined in playerbot_awakening.h (Digi Rasta's
	// awakening and soul stones +5..+9), which is included later.
	bool IsPlayerBotAwakeningGoods(DWORD vnum);
	bool IsPlayerBotAwakenedWeaponVnum(DWORD vnum);
	// The Ritual of Awakening at the blacksmith, for a bot that qualifies
	// (playerbot_awakening.h): the refining pass asks it first, and the
	// question of a blacksmith trip counts it as a reason.
	bool HasPlayerBotAwakeningRitual(LPCHARACTER ch);
	bool ManagePlayerBotAwakeningRitual(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow);
	// MT2009_PLUS_HEAVEN_OIL_V1 (Autor: Digi Rasta, v0.17): a bag soul stone
	// +4..+8 raised at the blacksmith with the dust and Olejek Niebios, and
	// the oil kept and wanted for that step (playerbot_awakening.h).
	bool HasPlayerBotSoulStoneStep(LPCHARACTER ch);
	bool ManagePlayerBotSoulStoneStep(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow);
	int GetPlayerBotHeavenOilKeep(LPCHARACTER ch);
	bool PlayerBotWantsHeavenOil(LPCHARACTER ch);

	PIXEL_POSITION GetPlayerBotGeneralStorePos(long mapIndex)
	{
		PIXEL_POSITION pos;
		pos.x = 0;
		pos.y = 0;
		pos.z = 0;

		if (mapIndex == 21 || mapIndex == 23) // Chunjo M1 / M3
		{
			pos.x = 59000;
			pos.y = 68900;
		}
		else if (mapIndex == 1 || mapIndex == 3) // Shinsoo M1 / M3
		{
			pos.x = 67800;
			pos.y = 56500;
		}
		else if (mapIndex == 41 || mapIndex == 43) // Jinno M1 / M3
		{
			pos.x = 38300;
			pos.y = 69300;
		}

		return pos;
	}

	DWORD GetPlayerBotSkillBookSkillVnum(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_SKILLBOOK)
			return 0;
		return item->GetVnum() == 50300 ? (DWORD)item->GetSocket(0) : (DWORD)item->GetValue(0);
	}

	bool IsPlayerBotOwnSkill(LPCHARACTER ch, DWORD skillVnum)
	{
		if (!ch || skillVnum == 0 || ch->GetSkillGroup() == 0)
			return false;
		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
		for (BYTE i = 0; i < build.bSkillCount; ++i)
			if (build.dwSkills[i] == skillVnum)
				return true;
		return false;
	}

	// The trader's book purse (community patch 2, point 5): what is left of
	// PLAYERBOT_BOOK_VISIT_BUDGET_PERCENT of the yang its window began with.
	// A window the town visit did not open (HandlePlayerBotTownVisit) opens on
	// the first question and lasts PLAYERBOT_BOOK_BUDGET_WINDOW_MS.
	long long GetPlayerBotBookBudgetLeft(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (st == s_mapPlayerBotAIStates.end())
			return 0;
		TPlayerBotPersona& p = st->second.persona;
		const DWORD now = get_dword_time();
		// Iwakura's Patch 3, point 7: the mad scientist's books come out of 70
		// percent of the purse it began with.
		if (IsPlayerBotRareNow(p, playerbot_persona::RARE_NAUKOWIEC, now))
			return std::max(0LL, p.llRareGoldStart * PLAYERBOT_NAUKOWIEC_BUDGET_PERCENT / 100 - p.llRareSpent);
		if (p.dwBookBudgetSince == 0 || now - p.dwBookBudgetSince >= PLAYERBOT_BOOK_BUDGET_WINDOW_MS)
		{
			p.llBookBudgetBase = (long long)ch->GetGold();
			p.llBookBudgetSpent = 0;
			p.dwBookBudgetSince = now;
		}
		return std::max(0LL, p.llBookBudgetBase * PLAYERBOT_BOOK_VISIT_BUDGET_PERCENT / 100 -
				p.llBookBudgetSpent);
	}

	void NotePlayerBotBookBought(LPCHARACTER ch, long long price)
	{
		if (!ch || price <= 0)
			return;
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (st != s_mapPlayerBotAIStates.end())
		{
			st->second.persona.llBookBudgetSpent += price;
			if (IsPlayerBotRareNow(st->second.persona, playerbot_persona::RARE_NAUKOWIEC, get_dword_time()))
				st->second.persona.llRareSpent += price;
		}
	}

	// A bot in town as the Trader: the personality says so, or the town visit
	// does (the blacksmith's part of a visit is the Perfectionist's).
	bool PlayerBotBuysBooksAsTrader(LPCHARACTER ch)
	{
		if (!ch || !IsPlayerBotPersonaEnabled())
			return false;
		TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (st == s_mapPlayerBotAIStates.end())
			return false;
		return (st->second.persona.bPersona == playerbot_persona::PERSONA_HANDLARZ ||
				st->second.bVisitingShop ||
				IsPlayerBotRareNow(st->second.persona, playerbot_persona::RARE_NAUKOWIEC, get_dword_time())) &&
				GetPlayerBotBookBudgetLeft(ch) > 0;
	}

	// Iwakura's list (playerbot_lpp.h, later): a piece on it the list does not
	// keep, which is counter goods.
	bool IsPlayerBotLppSurplusGoods(LPCHARACTER ch, LPITEM item);

	// A gambler's goods and one of Community Patch 5's four gamblers' bases
	// (playerbot_gambler.h, later): neither is the merchant's.
	bool IsPlayerBotGambleForSale(LPCHARACTER ch, LPITEM item);
	bool IsPlayerBotRareGambleHeldBase(LPCHARACTER ch, LPITEM item);

	// A weapon with a line a player stops rerolling at: average damage from
	// PLAYERBOT_BONUS_KEEP_AVERAGE, or skill damage from
	// PLAYERBOT_WEAPON_PRIZE_SKILL_PERCENT. In this engine every failed
	// refine destroys the item - DoRefine has no grade that only drops a
	// level - so a +8 bow with 51% average burned at the blacksmith on a
	// forty-percent roll ("i spalil u kowala"). Such a weapon is refined only
	// under a Blessing Scroll, which hands it back a level down instead.
	bool IsPlayerBotPrizeWeapon(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_WEAPON)
			return false;
		long avg = 0, skill = 0;
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			const BYTE t = item->GetAttributeType(i);
			if (t == APPLY_NORMAL_HIT_DAMAGE_BONUS) avg += item->GetAttributeValue(i);
			else if (t == APPLY_SKILL_DAMAGE_BONUS) skill += item->GetAttributeValue(i);
		}
		return avg >= PLAYERBOT_BONUS_KEEP_AVERAGE || skill >= PLAYERBOT_WEAPON_PRIZE_SKILL_PERCENT;
	}

	// The one weapon a bot keeps in the bag for its lines: of the bot's class,
	// wearable now, outside the level-30 family (FindPlayerBotClassLevel30Weapon
	// has those), a prize (IsPlayerBotPrizeWeapon), under the plus of the one in
	// the hand, and at that plus a blow over the hand's by
	// PLAYERBOT_LINES_PROJECT_MARGIN_PERCENT. The equipment pass judges the plus
	// a piece has, so better lines under a lower plus were never worn: the
	// merchant took them at +0..+3 and the counter above that ("jak boty sobie
	// wydropia bron z wysokimi srednimi dla siebie to za wszelka cene probuja ja
	// sprzedac zamiast sobie ja zostawic", sosen, 27 September). The best one
	// only - the bag is not a collection - refined to the hand's plus
	// (GetPlayerBotRefineTarget), where the equipment pass takes it.
	LPITEM FindPlayerBotLinesProject(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return NULL;
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		if (!worn)
			return NULL;
		const long long wornBlow = GetPlayerBotWeaponHitDamage(worn, ch);
		if (wornBlow <= 0)
			return NULL;
		const BYTE plus = worn->GetRefineLevel();
		LPITEM best = NULL;
		long long bestBlow = wornBlow * (100 + PLAYERBOT_LINES_PROJECT_MARGIN_PERCENT) / 100;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->GetType() != ITEM_WEAPON ||
					item->GetRefineLevel() >= plus || IsPlayerBotSpecialLevel30Weapon(item) ||
					!IsPlayerBotPrizeWeapon(item) || !IsPlayerBotWeapon(ch, item) || !item->CanUsedBy(ch) ||
					item->GetLevelLimit() > ch->GetLevel() || IsPlayerBotBannedLowWeapon(ch, item))
				continue;
			const TItemTable* proto = GetPlayerBotWeaponProtoAtPlus(item, plus);
			const long long blow = proto ? GetPlayerBotWeaponHitDamageAt(item, proto, ch) : 0;
			if (blow > bestBlow)
			{
				best = item;
				bestBlow = blow;
			}
		}
		return best;
	}

	// Anything a player would not put on the anvil without a scroll: a prize
	// weapon, or a piece already carrying PLAYERBOT_PRIZE_LINES lines.
	bool IsPlayerBotPrizeItem(LPITEM item)
	{
		if (!item)
			return false;
		if (IsPlayerBotPrizeWeapon(item))
			return true;
		int lines = 0;
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			if (item->GetAttributeType(i) != 0 && item->GetAttributeValue(i) != 0)
				++lines;
		// A jewel filled by the stones is four lines, which the stones never
		// take past, so it never met the five: a necklace of four with +1000
		// HP went to the plain anvil on its way to +9 and survived one time in
		// thirty-five (Iwakura's audit, R7: 12 such burns in a day, 5 of them
		// at a step of 60% or worse).
		if (item->GetType() == ITEM_ARMOR && IsPlayerBotJewelSubType(item->GetSubType()) &&
				lines >= PLAYERBOT_BONUS_MAX_LINES)
			return true;
		return lines >= PLAYERBOT_PRIZE_LINES;
	}

	// A weapon PLAYERBOT_JUNK_WEAPON_MARKET_CAP caps (community patch 2,
	// point 13). One rolled with prize lines is not low quality: a Krwawy
	// Miecz +0 with a forty percent average is what players cross a market
	// for, and the cap was written against the ones nobody buys.
	bool IsPlayerBotBonusGoodsPiece(LPITEM item);
	bool IsPlayerBotCappedJunkWeapon(LPITEM item)
	{
		// MT2009_PLUS_BONUS_COUNT_PRICE_V1: nor one whose lines the owner's
		// bonus-count prices pay for (IsPlayerBotBonusGoodsPiece).
		return item && item->GetType() == ITEM_WEAPON && IsPlayerBotJunkWeaponVnum(item->GetVnum()) &&
				!IsPlayerBotPrizeItem(item) && !IsPlayerBotBonusGoodsPiece(item);
	}

	// A body armour Iwakura's Patch 3, point 4 caps on the market: +0..+4, and
	// not one rolled with prize lines, which is not what flooded it. Nor a
	// black-steel armour of sixty-six (playerbot_stalki.h): the flood was the
	// level-34 families, and a Stalki is never the merchant's, so one the cap
	// sent home would stand in its bag for good.
	// MT2009_PLUS_MARKET_V3, point 5 (playerbot_bonus.h): a piece whose lines
	// price it as a +7 or better is no low plus to any cap below.
	bool IsPlayerBotBonusGoodsPiece(LPITEM item);

	// MT2009_PLUS_BOT_LIST_HELM_SHIELD_V1 (the owner, 2 October: "na
	// sklepach nie ma w ogole helmow i tarcz"): the helmets of level 21 and
	// 41 of every class and the Pieciokatna Tarcza (13020-13029) and the
	// Czarna Okragla Tarcza (13040-13049), at every plus, are counter goods:
	// never put down as dead stock, and let out of the safebox
	// (CollectPlayerBotLppBoxRelease). Their listing and the merchant's share
	// are MT2009_PLUS_BOT_LIST_ALL_GEAR_V1's now, below, for every family.
	bool IsPlayerBotListedHelmShieldProto(const TItemTable* proto, DWORD vnum)
	{
		if (!proto || proto->bType != ITEM_ARMOR)
			return false;
		const DWORD family = vnum - vnum % 10;
		if (proto->bSubType == ARMOR_SHIELD)
			return family == 13020 || family == 13040;
		if (proto->bSubType != ARMOR_HEAD)
			return false;
		for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
			if (proto->aLimits[i].bType == LIMIT_LEVEL)
				return proto->aLimits[i].lValue == 21 || proto->aLimits[i].lValue == 41;
		return false;
	}

	bool IsPlayerBotListedHelmShield(LPITEM item)
	{
		return item && IsPlayerBotListedHelmShieldProto(item->GetProto(), item->GetVnum());
	}

	// MT2009_PLUS_BOT_LIST_ALL_GEAR_V1 (the owner, 2 October: the counters
	// had almost no boots, helmets or shields - Skorzane Kozaki +0 on 146
	// bags of the supporters' world and 3 lines, Pieciokatna Tarcza +0 on 67
	// bags and 3 lines, the helmets of 41 on 194 bags and 11 lines). Every
	// spare piece of gear a bot means to sell - a weapon, a body armour, a
	// helmet, a shield, boots or a jewel - is taken to PLAYERBOT_SPARE_SALE_PLUS
	// at the anvil first while the purse and the bag can pay the step
	// (PlayerBotRefinesSpareForSale), and is goods at what it is when they
	// cannot: the boots, helmets and shields of every family and level, the
	// gear under level thirty below its old floor of +6, and the weapons and
	// armour from thirty at +0..+3 (ScorePlayerBotShopStockRules). A counter
	// shows PLAYERBOT_ALL_GEAR_KIND_LINES such lines of a kind
	// (BotOfflineCounterRefuses), the bag keeps PLAYERBOT_ALL_GEAR_BAG_KEEP of
	// them for it from the merchant (IsPlayerBotJunkItem). Not a level-30
	// weapon nor a Stalki, which have rules of their own, nor starter gear - a
	// weapon, a body armour or a jewel of level one - which stays the
	// merchant's under +7 (GetPlayerBotLowGearMinRefine). Before, a helmet,
	// shield or boots under +6 below level thirty, and under +4 from it, was
	// nobody's goods, so the merchant took it on the next town visit.
	// (Its numbers are in playerbot_types.h.)

	// The piece's kind for the counter's cap - 1 a weapon, 2 a body armour, 3
	// a helmet, 4 a shield, 5 boots, 6 to 8 the bracelet, the necklace and
	// the earrings - or 0 for what this rule leaves alone.
	int GetPlayerBotSaleGearKindOf(const TItemTable* proto, DWORD vnum)
	{
		if (!proto || IsPlayerBotSpecialLevel30WeaponVnum(vnum) ||
				playerbot_stalki_rules::KindOf(vnum) != playerbot_stalki_rules::KIND_NONE)
			return 0;
		int kind = 0;
		if (proto->bType == ITEM_WEAPON)
			kind = proto->bSubType == WEAPON_ARROW ? 0 : 1;
		else if (proto->bType == ITEM_ARMOR)
		{
			switch (proto->bSubType)
			{
				case ARMOR_BODY: kind = 2; break;
				case ARMOR_HEAD: kind = 3; break;
				case ARMOR_SHIELD: kind = 4; break;
				case ARMOR_FOOTS: kind = 5; break;
				case ARMOR_WRIST: kind = 6; break;
				case ARMOR_NECK: kind = 7; break;
				case ARMOR_EAR: kind = 8; break;
				default: break;
			}
		}
		// Starter gear keeps the operator's rule; a helmet, a shield or boots
		// of level one are goods like any other.
		if ((kind == 1 || kind == 2 || kind >= 6) &&
				GetPlayerBotProtoLevelLimit(proto) <= PLAYERBOT_SHOP_STARTER_GEAR_MAX_LEVEL)
			return 0;
		return kind;
	}

	int GetPlayerBotSaleGearKind(LPITEM item)
	{
		return item ? GetPlayerBotSaleGearKindOf(item->GetProto(), item->GetVnum()) : 0;
	}

	bool IsPlayerBotSaleGear(LPITEM item)
	{
		return GetPlayerBotSaleGearKind(item) != 0;
	}

	// A line this rule put up, and the ones the cap counts: under +7 below
	// level thirty, under +4 from it. A +4 from thirty was goods before
	// (PLAYERBOT_PRECIOUS_REFINE) and is not counted.
	bool IsPlayerBotAllGearLowLineOf(const TItemTable* proto, DWORD vnum)
	{
		if (GetPlayerBotSaleGearKindOf(proto, vnum) == 0)
			return false;
		const int plus = (int)(vnum % 10);
		return plus < 7 && (GetPlayerBotProtoLevelLimit(proto) < PLAYERBOT_SHOP_MIN_GEAR_LEVEL ||
				plus < PLAYERBOT_PRECIOUS_REFINE);
	}

	bool IsPlayerBotAllGearLowLine(LPITEM item)
	{
		return item && IsPlayerBotAllGearLowLineOf(item->GetProto(), item->GetVnum());
	}

	// Those pieces in the bag's cells before this one: the bag keeps the
	// first PLAYERBOT_ALL_GEAR_BAG_KEEP for its counter (IsPlayerBotJunkItem).
	int CountPlayerBotAllGearAhead(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetWindow() != INVENTORY)
			return 0;
		int ahead = 0;
		for (WORD cell = 0; cell < item->GetCell() && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM held = ch->GetInventoryItem(cell);
			if (held && held != item && held->GetCell() == cell && !held->IsEquipped() &&
					IsPlayerBotAllGearLowLine(held))
				++ahead;
		}
		return ahead;
	}

	// Defined beside the low armour's anvil rule, below.
	bool PlayerBotRefinesSpareForSale(LPCHARACTER ch, LPITEM item);

	bool IsPlayerBotCappedLowArmour(LPITEM item)
	{
		return item && item->GetType() == ITEM_ARMOR && item->GetSubType() == ARMOR_BODY &&
				item->GetRefineLevel() <= PLAYERBOT_LOW_ARMOUR_MAX_PLUS && !IsPlayerBotPrizeItem(item) &&
				!IsPlayerBotStalkiItem(item) && !IsPlayerBotBonusGoodsPiece(item);
	}

	// And a jewel his answer of 26 September holds to the same bound: +0..+3,
	// no prize lines (PLAYERBOT_LOW_JEWEL_MARKET_CAP), counted with the armour
	// by family (GetPlayerBotLowArmourFamily).
	bool IsPlayerBotCappedLowJewel(LPITEM item)
	{
		return item && item->GetType() == ITEM_ARMOR && IsPlayerBotJewelSubType(item->GetSubType()) &&
				item->GetRefineLevel() <= PLAYERBOT_LOW_PLUS_MARKET_MAX_PLUS && !IsPlayerBotPrizeItem(item) &&
				!IsPlayerBotBonusGoodsPiece(item);
	}

	// A body armour or a jewel at +0..+3 is every bot's counter goods (Iwakura's
	// answer of 26 September, "postacie normalnie je wystawiac"), under level
	// thirty as well: the gamblers' bases and a young world's players' gear,
	// which only a scrap keeper put up - from level thirty, and nobody under it,
	// where the operator's floor of +6 stood - while the merchant got the rest
	// for a few hundred yang. On m2zip on 26 September the bots in the world
	// carried 560 such armours and none of them was on a counter under level
	// thirty. Not a level-one piece (a wooden jewel, a starter armour), which
	// stays the merchant's under +7 (GetPlayerBotLowGearMinRefine), and not one
	// with prize lines, which the other rules price. The family caps above
	// still hold, and one counter still takes only
	// PLAYERBOT_SHOP_LOW_GEAR_MAX_LINES of the gear under level thirty.
	bool IsPlayerBotLowPlusMarketGear(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_ARMOR ||
				(int)item->GetRefineLevel() > PLAYERBOT_LOW_PLUS_MARKET_MAX_PLUS ||
				(int)item->GetLevelLimit() <= PLAYERBOT_SHOP_STARTER_GEAR_MAX_LEVEL || IsPlayerBotPrizeItem(item))
			return false;
		return item->GetSubType() == ARMOR_BODY || IsPlayerBotJewelSubType(item->GetSubType());
	}

	// Those pieces in the bag's cells before this one: the bag keeps the first
	// PLAYERBOT_LOW_PLUS_BAG_KEEP for its counter (IsPlayerBotJunkItem).
	int CountPlayerBotLowPlusGearAhead(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetWindow() != INVENTORY)
			return 0;
		int ahead = 0;
		for (WORD cell = 0; cell < item->GetCell() && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM held = ch->GetInventoryItem(cell);
			if (held && held != item && held->GetCell() == cell && !held->IsEquipped() &&
					IsPlayerBotLowPlusMarketGear(held))
				++ahead;
		}
		return ahead;
	}

	// Every skill book in the bag, whatever the skill.
	int CountPlayerBotSkillBooks(LPCHARACTER ch)
	{
		int books = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetType() == ITEM_SKILLBOOK)
				books += item->GetCount();
		}
		return books;
	}

	// Whether a book is beyond the bag's working stock: somebody else's skill,
	// or one of its own past GetPlayerBotBookKeepLimit. What the counter
	// sells, and what the safebox takes when the counter has not.
	bool IsPlayerBotSurplusSkillBook(LPCHARACTER ch, LPITEM item);

	// How many books of one of its own skills a bot keeps in the bag: the
	// working stock while the skill is readable, a few before that, none once
	// a book can do nothing more for it. Every rule that keeps, lists or buys
	// a book asks this, so the bag, the counter and the market agree.
	int GetPlayerBotBookKeepLimit(LPCHARACTER ch, DWORD skillVnum)
	{
		if (!ch || skillVnum == 0)
			return 0;
		if (ch->GetSkillMasterType(skillVnum) >= SKILL_GRAND_MASTER)
			return 0;
		const BYTE level = ch->GetSkillLevel(skillVnum);
		if (ch->GetSkillMasterType(skillVnum) == SKILL_MASTER && level >= 20 && level < 30)
			return PLAYERBOT_BOOK_KEEP_PER_SKILL;
		return PLAYERBOT_BOOK_KEEP_UNREADABLE;
	}

	// The engine's stacking rule, asked of two bag items: MoveItem pours one
	// into the other only for the same vnum with every socket equal.
	bool PlayerBotStacksTogether(LPITEM item, LPITEM other)
	{
		if (!item || !other || item == other || item->GetVnum() != other->GetVnum())
			return false;
		if (!item->IsStackable() || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
			return false;
		for (int s = 0; s < ITEM_SOCKET_MAX_NUM; ++s)
			if (item->GetSocket(s) != other->GetSocket(s))
				return false;
		return true;
	}

	// The stack an item can grow to. mt2009 keeps one per proto (dwMaxStack:
	// twenty for a Medal Konny or a Blessing Scroll, a thousand for arrows,
	// two hundred for most things); r40250 has the one ITEM_MAX_COUNT. Every
	// count the fragments compared with PLAYERBOT_STACK_MAX read a full stack
	// of twenty medals as room for a hundred and eighty more: MoveItem into a
	// full stack moves nothing and still answers true, so ten stacks of twenty
	// were "merged" four at a time every five seconds for ever - 110 bots and
	// 14 321 lines in ten minutes on the test world, every medal dropper among
	// them (16 September). SetCount clamps to the same limit, silently, which
	// is what the safebox top-up below has to know before it removes the bag
	// stack it thinks it poured in.
	int PlayerBotMaxStack(LPITEM item)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		return item ? (int)item->GetMaxStack() : PLAYERBOT_STACK_MAX;
#else
		(void)item;
		return PLAYERBOT_STACK_MAX;
#endif
	}

	// Pour split stacks together, a few at a time. MoveItem with a count of
	// zero moves as much of the source as the destination has room for and
	// removes the source when it is emptied - the same thing a player's drag
	// does, packets and item log included. A merge is counted only when the
	// destination grew: a MoveItem that moved nothing is not work done.
	int MergePlayerBotStacks(LPCHARACTER ch, int maxMerges)
	{
		int merged = 0;
		for (WORD i = 0; i < PLAYERBOT_BAG_CELLS && merged < maxMerges; ++i)
		{
			LPITEM item = ch->GetInventoryItem(i);
			if (!item || item->IsEquipped() || item->isLocked())
				continue;
			const int maxStack = PlayerBotMaxStack(item);
			if ((int)item->GetCount() >= maxStack)
				continue;
			for (WORD j = i + 1; j < PLAYERBOT_BAG_CELLS && merged < maxMerges; ++j)
			{
				LPITEM other = ch->GetInventoryItem(j);
				if (!other || other->IsEquipped() || other->isLocked() ||
						!PlayerBotStacksTogether(item, other))
					continue;
				const DWORD before = item->GetCount();
				if (ch->MoveItem(TItemPos(INVENTORY, j), TItemPos(INVENTORY, i), 0) &&
						item->GetCount() > before)
					++merged;
				if ((int)item->GetCount() >= maxStack)
					break;
			}
		}
		return merged;
	}

#if !defined(PLAYERBOT_ENGINE_MT2009)
	// The order the bag is tidied into: the red and blue potions, then every
	// other potion - green and purple, the timed boosters, the auto potions -
	// then chests and keys ("wszelakie potki pierwsze a potem reszte",
	// 15 September). Everything else is 99 and stays behind them.
	int GetPlayerBotSortPriority(LPITEM item)
	{
		if (!item)
			return 99;
		const DWORD vnum = item->GetVnum();
		const BYTE type = item->GetType();
		if (type == ITEM_USE)
		{
			const BYTE sub = item->GetSubType();
			if (sub == USE_POTION || sub == USE_POTION_NODELAY)
				return 0;   // red/blue/big HP-SP potions
			if (sub == USE_POTION_CONTINUE || IsPlayerBotBoosterItem(item) ||
					GetPlayerBotAutoPotionAffect(vnum) != 0)
				return 1;   // green/purple, Hand of Critic/Penetration, Swiftness, the elixirs
		}
#if defined(PLAYERBOT_ENGINE_MT2009)
		if (type == ITEM_POTION)
			return 1;   // the green and purple potions are a type of their own here
#endif
		if ((vnum >= 27100 && vnum <= 27105) || vnum == 27053 || vnum == 27054)
			return 1;
		if (vnum == PLAYERBOT_MOONLIGHT_CHEST_VNUM || type == ITEM_TREASURE_BOX ||
				type == ITEM_TREASURE_KEY || type == ITEM_GIFTBOX)
			return 2;   // Moonlight chests, silver/gold chests and keys, boss caskets
		return 99;
	}

	// Potions to the front, chests and keys behind them, the rest after
	// (GetPlayerBotSortPriority). A selection sort over the cells a single-cell
	// item can stand in: the best item not yet in place is swapped with the one
	// in its way through a free cell - three MoveItems, each into an empty cell,
	// so nothing is merged, overwritten or lost, and a move the engine refuses
	// leaves an item where it was or in a free cell of the same bag. Gear of two
	// and three cells never moves, and nothing moves into the cells it covers.
	// The first version only moved a potion into an empty cell before it, and a
	// bag full from the front has none: the operator's screenshots showed
	// potions scattered through two pages of full bags.
	void SortPlayerBotConsumablesToFront(LPCHARACTER ch)
	{
		if (!ch)
			return;
		const long kRest = 99L * 1000000L;
		long key[PLAYERBOT_BAG_CELLS];
		bool empty[PLAYERBOT_BAG_CELLS];
		bool anyFront = false;
		for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			key[cell] = -1;   // a cell the sort does not touch
			empty[cell] = false;
			LPITEM item = ch->GetInventoryItem(cell);
			if (item)
			{
				if (item->GetCell() != cell || item->GetSize() != 1 || item->IsEquipped() ||
						item->isLocked() || item->IsExchanging())
					continue;
				const int prio = GetPlayerBotSortPriority(item);
				key[cell] = prio >= 99 ? kRest : (long)prio * 1000000L + (long)(item->GetVnum() % 1000000U);
				anyFront = anyFront || prio < 99;
			}
			else if (ch->IsEmptyItemGrid(TItemPos(INVENTORY, (WORD)cell), 1))
			{
				key[cell] = kRest;
				empty[cell] = true;
			}
		}
		if (!anyFront)
			return;
		int moves = 0;
		for (int i = 0; i < PLAYERBOT_BAG_CELLS && moves + 3 <= PLAYERBOT_SORT_MAX_MOVES; ++i)
		{
			if (key[i] < 0)
				continue;
			int best = -1;
			for (int j = i + 1; j < PLAYERBOT_BAG_CELLS; ++j)
				if (key[j] >= 0 && !empty[j] && key[j] < key[i] && (best < 0 || key[j] < key[best]))
					best = j;
			if (best < 0)
				continue;
			if (empty[i])
			{
				if (!ch->MoveItem(TItemPos(INVENTORY, (WORD)best), TItemPos(INVENTORY, (WORD)i), 0))
					break;
				++moves;
			}
			else
			{
				int buffer = -1;
				for (int k = PLAYERBOT_BAG_CELLS - 1; k >= 0; --k)
					if (empty[k] && k != i && k != best)
					{
						buffer = k;
						break;
					}
				if (buffer < 0)
					break;
				if (!ch->MoveItem(TItemPos(INVENTORY, (WORD)best), TItemPos(INVENTORY, (WORD)buffer), 0))
					break;
				++moves;
				if (!ch->MoveItem(TItemPos(INVENTORY, (WORD)i), TItemPos(INVENTORY, (WORD)best), 0))
					break;
				++moves;
				if (!ch->MoveItem(TItemPos(INVENTORY, (WORD)buffer), TItemPos(INVENTORY, (WORD)i), 0))
					break;
				++moves;
			}
			std::swap(key[i], key[best]);
			std::swap(empty[i], empty[best]);
		}
		if (moves > 0)
			sys_log(0, "PLAYERBOT_BAG: sorted pid=%u name=%s moves=%d",
					ch->GetPlayerID(), ch->GetName(), moves);
	}
#else
	// The whole bag, as a player's "Scal i uporzadkuj" lays it out
	// (playerbot_arrange.cpp) - which keeps the potions-first order the
	// sort above gave the r40250 bags - on its own clock, by pid, so a
	// restart does not arrange the whole population in one minute.
	std::map<DWORD, DWORD> s_mapPlayerBotArrangeNext;

	void ManagePlayerBotArrange(LPCHARACTER ch, DWORD dwNow)
	{
		const DWORD pid = ch->GetPlayerID();
		const DWORD spread = PlayerBotNavHash(pid ^ 0x41524e47U);
		std::map<DWORD, DWORD>::iterator next = s_mapPlayerBotArrangeNext.find(pid);
		if (next == s_mapPlayerBotArrangeNext.end())
		{
			s_mapPlayerBotArrangeNext[pid] = dwNow + spread % PLAYERBOT_ARRANGE_INTERVAL;
			return;
		}
		if (dwNow < next->second)
			return;
		next->second = dwNow + PLAYERBOT_ARRANGE_INTERVAL + spread % PLAYERBOT_ARRANGE_SPREAD;
		const playerbot_arrange::TResult result = playerbot_arrange::ArrangeInventory(ch, false);
		if (result.code == playerbot_arrange::RESULT_DONE)
			sys_log(0, "PLAYERBOT_BAG: arranged pid=%u name=%s items=%d moved=%d merged=%d units=%u pinned=%d strategy=%d us=%u",
					pid, ch->GetName(), result.items, result.moved, result.merged, result.units,
					result.pinned, result.strategy, result.micros);
		else if (result.code == playerbot_arrange::RESULT_BUSY)
			next->second = dwNow + PLAYERBOT_ARRANGE_BUSY_RETRY;
	}
#endif

	void ManagePlayerBotStackMerge(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || dwNow < state.dwNextStackMergeTime)
			return;
		state.dwNextStackMergeTime = dwNow + PLAYERBOT_STACK_MERGE_INTERVAL +
				PlayerBotNavHash(ch->GetPlayerID() ^ 0x53544b4dU) % 60000U;
		// Not behind a counter: the singles there were split on purpose, and
		// the shop table points at the cells they are in.
		if (!ch->IsItemLoaded() || ch->GetMyShop())
			return;
		const int merged = MergePlayerBotStacks(ch, PLAYERBOT_STACK_MERGES_PER_PASS);
		if (merged > 0)
			sys_log(0, "PLAYERBOT_BAG: merged stacks pid=%u name=%s merges=%d",
					ch->GetPlayerID(), ch->GetName(), merged);
		// Then tidy: potions, boosters and chests to the front.
#if defined(PLAYERBOT_ENGINE_MT2009)
		ManagePlayerBotArrange(ch, dwNow);
#else
		SortPlayerBotConsumablesToFront(ch);
#endif
		// A pass that used its whole budget has more to do: back soon, not in
		// five minutes - a closed counter leaves eight packs of one material.
		if (merged >= PLAYERBOT_STACK_MERGES_PER_PASS)
			state.dwNextStackMergeTime = dwNow + PLAYERBOT_STACK_MERGE_AFTER_SHOP_MS;
	}

	// Goods priced by the heap: Iwakura's sheet at
	// PLAYERBOT_SHOP_BULK_MAX_BASE_PRICE or less before the yang rate, and of
	// a kind nobody buys one of - a material, a resource, an ore. The sheet's
	// own number rather than the asking price, which moves with the market,
	// the jitter and the markdown, so a line's size does not change between
	// two visits. No refine material is priced this low; a potion, a stone
	// or a key has a line of its own below whatever it costs.
	bool IsPlayerBotBulkGoods(LPITEM item)
	{
		if (!item || !item->IsStackable() || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
			return false;
		const BYTE type = item->GetType();
		if (type != ITEM_MATERIAL && type != ITEM_RESOURCE && type != ITEM_SPECIAL)
			return false;
		const DWORD vnum = item->GetVnum();
		for (size_t i = 0; i < sizeof(PLAYERBOT_MATERIAL_PRICES) / sizeof(PLAYERBOT_MATERIAL_PRICES[0]); ++i)
			if (PLAYERBOT_MATERIAL_PRICES[i].dwVnum == vnum)
				return false;
		for (size_t i = 0; i < sizeof(PLAYERBOT_EXTRA_MATERIAL_PRICES) / sizeof(PLAYERBOT_EXTRA_MATERIAL_PRICES[0]); ++i)
			if (PLAYERBOT_EXTRA_MATERIAL_PRICES[i].dwVnum == vnum)
				return PLAYERBOT_EXTRA_MATERIAL_PRICES[i].dwPrice != 0 &&
						PLAYERBOT_EXTRA_MATERIAL_PRICES[i].dwPrice <= PLAYERBOT_SHOP_BULK_MAX_BASE_PRICE;
		return false;
	}

	// The part of Iwakura's sheet no rule of its own looks after: items he
	// prices by name above the heap line that are ITEM_USE of the special or
	// the detachment kind (the horse and polymorph books, the stone scroll,
	// the Cape of Courage, the rose) or a material or special no recipe
	// consumes. They reached the junk rule's default and the merchant paid a
	// few hundred yang for what the sheet prices at forty to a hundred and
	// thirty-five thousand. The rest of the sheet keeps the rules it has -
	// rings, gloves and uniques (the unique slots), bonus stones (never on a
	// counter), refine scrolls, recipes, chests and keys, the shell and the
	// pearls - because a price is no reason to overrule them. Built once
	// from the two tables: the junk rule runs for every cell of every scan.
	bool IsPlayerBotSheetGoods(LPITEM item)
	{
		static std::set<DWORD> s_goods;
		static bool s_loaded = false;
		if (!s_loaded)
		{
			s_loaded = true;
			for (size_t i = 0; i < sizeof(PLAYERBOT_MATERIAL_PRICES) / sizeof(PLAYERBOT_MATERIAL_PRICES[0]); ++i)
				if (PLAYERBOT_MATERIAL_PRICES[i].dwPrice > PLAYERBOT_SHOP_BULK_MAX_BASE_PRICE)
					s_goods.insert(PLAYERBOT_MATERIAL_PRICES[i].dwVnum);
			for (size_t i = 0; i < sizeof(PLAYERBOT_EXTRA_MATERIAL_PRICES) / sizeof(PLAYERBOT_EXTRA_MATERIAL_PRICES[0]); ++i)
				if (PLAYERBOT_EXTRA_MATERIAL_PRICES[i].dwPrice > PLAYERBOT_SHOP_BULK_MAX_BASE_PRICE)
					s_goods.insert(PLAYERBOT_EXTRA_MATERIAL_PRICES[i].dwVnum);
		}
		if (!item || s_goods.find(item->GetVnum()) == s_goods.end())
			return false;
		const DWORD vnum = item->GetVnum();
		if (vnum == PLAYERBOT_SHELLFISH_VNUM ||
				(vnum >= PLAYERBOT_PEARL_FIRST_VNUM && vnum <= PLAYERBOT_PEARL_LAST_VNUM))
			return false;
		const BYTE type = item->GetType();
		if (type == ITEM_USE)
			return item->GetSubType() == USE_SPECIAL || item->GetSubType() == USE_DETACHMENT;
		return type == ITEM_MATERIAL || type == ITEM_SPECIAL;
	}

	// How many units of a stackable go on one counter line. A private shop
	// sells a line whole, so a stack of twenty scrolls on one line is twenty
	// scrolls or nothing: what a player buys one at a time - potions,
	// stones, the shell and the pearls - is a single; a safe refine scroll
	// is a line of PLAYERBOT_SHOP_SCROLL_LINE_UNITS (one to five, the
	// Discord's number); a material is a pack of PLAYERBOT_SHOP_PACK_UNITS,
	// small enough to buy for one refine and few enough lines to leave room
	// on the counter. Zero for anything that does not stack.
	// The three stones a bot rerolls gear with: the change stone, the add
	// stone and the blessing marble, by their subtype rather than by vnum -
	// each comes in an ordinary and an ItemShop flavour (the 76xxx copies),
	// and the green pair for gear of forty and under is a fourth and fifth.
	bool IsPlayerBotBonusStoneItem(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_USE)
			return false;
		const BYTE sub = item->GetSubType();
		return sub == USE_ADD_ATTRIBUTE || sub == USE_CHANGE_ATTRIBUTE ||
				sub == USE_ADD_ATTRIBUTE2;
	}

	int GetPlayerBotStallLineUnits(LPITEM item)
	{
		if (!item || !item->IsStackable() || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
			return 0;
		if (IsPlayerBotSafeRefineScroll(item->GetVnum()))
			return PLAYERBOT_SHOP_SCROLL_LINE_UNITS;
		// Asked before the ITEM_USE singles below, which the medal is one of.
		if (item->GetVnum() == PLAYERBOT_HORSE_MEDAL_VNUM)
			return PLAYERBOT_SHOP_HORSE_MEDAL_LINE_UNITS;
		if (item->GetType() == ITEM_SKILLBOOK || item->GetVnum() == PLAYERBOT_GRAND_MASTER_STONE_VNUM)
			return 1;
		// A Cor Draconis goes up five and more a line (operator, 26 September
		// 2026); a sash one at a time.
		if (IsPlayerBotCorVnum(item->GetVnum()))
			return 5;
		if (GetPlayerBotRareGoodsKind(item->GetVnum()) != PLAYERBOT_RARE_GOODS_NONE)
			return 1;
		// A bean is bought a handful at a time (PLAYERBOT_ZEN_BEAN_LINE_UNITS).
		if (item->GetVnum() == PLAYERBOT_ZEN_BEAN_VNUM)
			return PLAYERBOT_ZEN_BEAN_LINE_UNITS;
		// A bonus stone is bought a few at a time, and a bot that found more
		// than it can spend has hundreds: one a line would take a day and a
		// half to shift a single bag of them.
		if (IsPlayerBotBonusStoneItem(item))
			return PLAYERBOT_SHOP_PACK_UNITS;
		// The green and purple potions in packs (Iwakura's Patch 3, point 5);
		// the offline cut takes the largest pack the spare fills.
		if (IsPlayerBotPackedPotion(item))
			return PLAYERBOT_SHOP_POTION_PACK_MIN;
		if (item->GetType() == ITEM_USE || item->GetType() == ITEM_METIN ||
				item->GetType() == ITEM_TREASURE_KEY ||
				(item->GetVnum() >= 27992 && item->GetVnum() <= 27994))
			return 1;
		if (item->GetVnum() == PLAYERBOT_MOONLIGHT_CHEST_VNUM)
			return PLAYERBOT_CHEST_LINE_UNITS;
		// A root worth pennies is sold by the heap (IsPlayerBotBulkGoods).
		if (IsPlayerBotBulkGoods(item))
			return PLAYERBOT_SHOP_BULK_PACK_UNITS;
		return PLAYERBOT_SHOP_PACK_UNITS;
	}

	// Iwakura's Patch 3, point 6: a bot's counter is laid out the way a bag is
	// sorted - "bronie, zbroje, bizuteria, ksiegi umiejetnosci (KU) oraz
	// ulepszacze itp." - so its categories in that order, the rest after them.
	int GetPlayerBotShopCategoryOf(BYTE type, BYTE subType, DWORD vnum)
	{
		if (type == ITEM_WEAPON && subType != WEAPON_ARROW)
			return 0;
		if (type == ITEM_ARMOR)
			return subType == ARMOR_WRIST || subType == ARMOR_NECK || subType == ARMOR_EAR ? 2 : 1;
		// The skill books, and the general ones - Leadership and Combo.
		if (type == ITEM_SKILLBOOK || (vnum >= 50301 && vnum <= 50306))
			return 3;
		if (type == ITEM_MATERIAL || (type == ITEM_USE && subType == USE_TUNING))
			return 4;
		if (type == ITEM_METIN)
			return 5;
		if (type == ITEM_USE && (subType == USE_CHANGE_ATTRIBUTE || subType == USE_ADD_ATTRIBUTE ||
				subType == USE_ADD_ATTRIBUTE2))
			return 6;
#if defined(PLAYERBOT_ENGINE_MT2009)
		if (type == ITEM_POTION)
			return 7;
#endif
		if (type == ITEM_USE && (subType == USE_POTION || subType == USE_POTION_NODELAY ||
				subType == USE_ABILITY_UP))
			return 7;
		return 8;
	}

	int GetPlayerBotShopCategory(LPITEM item)
	{
		return item ? GetPlayerBotShopCategoryOf(item->GetType(), item->GetSubType(), item->GetVnum()) : 8;
	}

	bool IsPlayerBotSinglyTradedGoods(LPITEM item)
	{
		return GetPlayerBotStallLineUnits(item) == 1;
	}

	// Everything this bot wears or carries that is still below its refine target,
	// against what those refines actually consume. A materialVnum of zero asks
	// the looser question - short of anything at all - which is what decides
	// whether walking to the market is worth the trip; a real vnum asks about the
	// one thing on the counter in front of it.
	bool PlayerBotIsShortOfRefineMaterial(LPCHARACTER ch, DWORD materialVnum)
	{
		if (!ch)
			return false;
		// MT2009_PLUS_HEAVEN_OIL_V1 (Autor: Digi Rasta): Olejek Niebios feeds no gear recipe, only
		// the soul stone steps - short of it when a bag stone, the fee and the
		// dust wait for nothing else. Asked of the oil by name only: the
		// looser question (zero, "walk to the market") leaves it out, so a
		// world without oil on any counter sends nobody there for it.
		if (materialVnum == PLAYERBOT_HEAVEN_OIL_VNUM)
			return PlayerBotWantsHeavenOil(ch);

		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		std::vector<LPITEM> gear;
		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
			if (ch->GetWear(wearSlots[i]))
				gear.push_back(ch->GetWear(wearSlots[i]));
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM candidate = ch->GetInventoryItem(cell);
			if (IsPlayerBotEquipmentCandidate(ch, candidate))
				gear.push_back(candidate);
		}

		for (size_t i = 0; i < gear.size(); ++i)
		{
			LPITEM item = gear[i];
			if (!item || item->GetRefinedVnum() == 0 ||
					item->GetRefineLevel() >= GetPlayerBotRefineTarget(ch, item))
				continue;
			const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
			if (!recipe)
				continue;
			for (int m = 0; m < recipe->material_count; ++m)
			{
				const DWORD vnum = recipe->materials[m].vnum;
				if (vnum == 0 || recipe->materials[m].count == 0)
					continue;
				if (materialVnum != 0 && vnum != materialVnum)
					continue;
				if (ch->CountSpecifyItem(vnum) < recipe->materials[m].count * 2)
					return true;
			}
		}
		return false;
	}

	bool PlayerBotNeedsRefineMaterial(LPCHARACTER ch, DWORD materialVnum)
	{
		return materialVnum != 0 &&
				PlayerBotIsShortOfRefineMaterial(ch, materialVnum);
	}

	// What the next step of a piece under Iwakura's scroll rule lacks - the
	// weapon in the hand (IsPlayerBotScrollRuleWeapon) and, once that one
	// stands at +8, the armour on the back (IsPlayerBotScrollRuleArmour) - by
	// vnum: the recipe's count less what the bag holds over the Biologist's
	// share, the question CanPlayerBotPayRefineStep asks. Empty when the rule
	// has no piece or nothing is lacking. The offline buyer and the
	// progression trip buy exactly this ("W razie braku potrzebnych
	// ulepszaczy, bot najpierw udaje sie na rynek, aby je dokupic"); the
	// rule's bots were short of a material 21 times in 23 on m2zip (27
	// September), and only the 64-line browse ever looked.
	void CollectPlayerBotScrollRuleMissing(LPCHARACTER ch, std::map<DWORD, int>& missing)
	{
		missing.clear();
		if (!ch || !ch->IsItemLoaded())
			return;
		LPITEM pieces[2] = { GetPlayerBotHandWeapon(ch), GetPlayerBotBodyArmour(ch) };
		if (!IsPlayerBotScrollRuleWeapon(ch, pieces[0]))
			pieces[0] = NULL;
		if (!IsPlayerBotScrollRuleArmour(ch, pieces[1]))
			pieces[1] = NULL;
		// Both recipes' counts against one bag: what the two steps want of a
		// material together.
		std::map<DWORD, int> need;
		for (int p = 0; p < 2; ++p)
		{
			const TRefineTable* recipe = pieces[p]
					? CRefineManager::instance().GetRefineRecipe(pieces[p]->GetRefineSet()) : NULL;
			for (int m = 0; recipe && m < recipe->material_count; ++m)
				if (recipe->materials[m].vnum != 0 && recipe->materials[m].count > 0)
					need[recipe->materials[m].vnum] += (int)recipe->materials[m].count;
		}
		for (std::map<DWORD, int>::const_iterator it = need.begin(); it != need.end(); ++it)
		{
			const int have = (int)ch->CountSpecifyItem(it->first) - GetPlayerBotBiologistReserve(ch, it->first);
			if (have < it->second)
				missing[it->first] = it->second - std::max(0, have);
		}
	}

	// MT2009_PLUS_DROPPER_INVEST_V1: what a dropper's next steps lack - the
	// weapon, the body armour and the shield it wears, each up to +9 - by
	// vnum: the recipes' materials over the Biologist's share, and a safe
	// scroll for each step the bag has none for. Its shopping window buys
	// these first (ManagePlayerBotOfflineShopping).
	void CollectPlayerBotDropperInvestMissing(LPCHARACTER ch, std::map<DWORD, int>& missing)
	{
		missing.clear();
		if (!ch || !ch->IsItemLoaded())
			return;
		const BYTE wears[] = { WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD };
		std::map<DWORD, int> need;
		int steps = 0;
		for (size_t i = 0; i < sizeof(wears) / sizeof(wears[0]); ++i)
		{
			LPITEM piece = ch->GetWear(wears[i]);
			if (!piece || piece->GetRefinedVnum() == 0 ||
					(int)piece->GetRefineLevel() >= PLAYERBOT_DROPPER_INVEST_MAX_PLUS ||
					IsPlayerBotScrollFreeGear(piece))
				continue;
			const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(piece->GetRefineSet());
			if (!recipe)
				continue;
			++steps;
			for (int m = 0; m < recipe->material_count; ++m)
				if (recipe->materials[m].vnum != 0 && recipe->materials[m].count > 0)
					need[recipe->materials[m].vnum] += (int)recipe->materials[m].count;
		}
		for (std::map<DWORD, int>::const_iterator it = need.begin(); it != need.end(); ++it)
		{
			const int have = (int)ch->CountSpecifyItem(it->first) - GetPlayerBotBiologistReserve(ch, it->first);
			if (have < it->second)
				missing[it->first] = it->second - std::max(0, have);
		}
		const int scrolls = CountPlayerBotSafeRefineScrolls(ch);
		if (steps > scrolls)
		{
			const DWORD scrollVnums[] = { PLAYERBOT_BLESSING_SCROLL_VNUM, 25041, 25043, 25045, 70039 };
			for (size_t i = 0; i < sizeof(scrollVnums) / sizeof(scrollVnums[0]); ++i)
				if (IsPlayerBotSafeRefineScroll(scrollVnums[i]))
					missing[scrollVnums[i]] = steps - scrolls;
		}
	}

	bool WantsPlayerBotDropperInvestOffer(LPCHARACTER ch, DWORD vnum)
	{
		if (!ch || !IsPlayerBotDropperShopping(ch->GetPlayerID(), get_dword_time()))
			return false;
		std::map<DWORD, int> missing;
		CollectPlayerBotDropperInvestMissing(ch, missing);
		return missing.find(vnum) != missing.end();
	}

	// How many units of a material this bot keeps back for its own anvil:
	// twice the largest recipe count among the pieces it would raise - the
	// same measure "short" uses above. The counter lists only what is over
	// it. Without it a bot short by one bought a pack of two, was no longer
	// short, and put both on its own counter at the price it had just paid
	// (Zolc Niedzwiedzia x2, sizowski) - then was short again.
	const int PLAYERBOT_MATERIAL_KEEP_LEVELS = 15;
	int GetPlayerBotRefineMaterialReserve(LPCHARACTER ch, DWORD materialVnum)
	{
		if (!ch || materialVnum == 0)
			return 0;
		// MT2009_PLUS_HEAVEN_OIL_V1 (Autor: Digi Rasta): the oil of the next soul stone step while a
		// bag stone waits for one; the counter lists the rest.
		if (materialVnum == PLAYERBOT_HEAVEN_OIL_VNUM)
			return GetPlayerBotHeavenOilKeep(ch);
		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		std::vector<LPITEM> gear;
		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
			if (ch->GetWear(wearSlots[i]))
				gear.push_back(ch->GetWear(wearSlots[i]));
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM candidate = ch->GetInventoryItem(cell);
			if (IsPlayerBotEquipmentCandidate(ch, candidate))
				gear.push_back(candidate);
		}
		// The level-30 weapon worked in the bag is kept for too, wherever it
		// stands among the gear above.
		TPlayerBotLevel30View level30;
		ReadPlayerBotLevel30View(ch, level30);
		if (level30.project && std::find(gear.begin(), gear.end(), level30.project) == gear.end())
			gear.push_back(level30.project);
		LPITEM hand = ch->GetWear(WEAR_WEAPON);
		int reserve = 0;
		for (size_t i = 0; i < gear.size(); ++i)
		{
			LPITEM item = gear[i];
			if (!item || item->GetRefinedVnum() == 0 ||
					item->GetRefineLevel() >= GetPlayerBotRefineTarget(ch, item))
				continue;
			// Materials only for pieces within PLAYERBOT_MATERIAL_KEEP_LEVELS
			// of the bot's own level: what a lower map's gear takes goes on
			// sale, at every look over the goods. Never the weapon in the hand
			// nor the level-30 project: a bot of 46 with a weapon at +6..+8
			// lost the materials of its next plus to that rule.
			if (item != hand && item != level30.project &&
					(int)item->GetLevelLimit() + PLAYERBOT_MATERIAL_KEEP_LEVELS < (int)ch->GetLevel())
				continue;
			const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
			if (!recipe)
				continue;
			for (int m = 0; m < recipe->material_count; ++m)
				if (recipe->materials[m].vnum == materialVnum)
					reserve = std::max(reserve, (int)recipe->materials[m].count * 2);
		}
		return reserve;
	}

	bool PlayerBotNeedsAnyRefineMaterial(LPCHARACTER ch)
	{
		return PlayerBotIsShortOfRefineMaterial(ch, 0);
	}

	// Every material this bot is short of, in one set. The same walk as the
	// question above, asked once per target scan instead of once per monster:
	// a scan looks at dozens of candidates a second and each answer costs a
	// pass over the bag.
	void CollectPlayerBotWantedMaterials(LPCHARACTER ch, std::set<DWORD>& out)
	{
		out.clear();
		if (!ch || !ch->IsItemLoaded())
			return;
		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		std::vector<LPITEM> gear;
		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
			if (ch->GetWear(wearSlots[i]))
				gear.push_back(ch->GetWear(wearSlots[i]));
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM candidate = ch->GetInventoryItem(cell);
			if (IsPlayerBotEquipmentCandidate(ch, candidate))
				gear.push_back(candidate);
		}
		for (size_t i = 0; i < gear.size(); ++i)
		{
			LPITEM item = gear[i];
			if (!item || item->GetRefinedVnum() == 0 ||
					item->GetRefineLevel() >= GetPlayerBotRefineTarget(ch, item))
				continue;
			const TRefineTable* recipe =
					CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
			if (!recipe)
				continue;
			for (int m = 0; m < recipe->material_count; ++m)
			{
				const DWORD vnum = recipe->materials[m].vnum;
				if (vnum == 0 || recipe->materials[m].count == 0)
					continue;
				if (ch->CountSpecifyItem(vnum) < recipe->materials[m].count * 2)
					out.insert(vnum);
			}
		}
	}

	// Whether this monster can still give this bot a material at all.
	// CreateDropItem multiplies every drop by the same PERCENT_LVDELTA as
	// experience, so fifteen levels over a monster leave one percent of the
	// chance, and under PLAYERBOT_MATERIAL_MIN_DROP_PERCENT a need is no
	// reason to farm it. One question for the fight's material exception
	// (BuildPlayerBotCombatContext) and for the errand that walks the bot to
	// that fight (StartPlayerBotMaterialHunt): the errand asked nothing, and
	// rode a bot of forty across its first village to a wolf of four that the
	// fight then refused.
	bool CanPlayerBotFarmMaterialFrom(LPCHARACTER ch, LPCHARACTER mob)
	{
		return ch && mob && mob->IsMonster() &&
				PERCENT_LVDELTA(ch->GetLevel(), mob->GetLevel()) >=
					PLAYERBOT_MATERIAL_MIN_DROP_PERCENT;
	}

	// Every material any recipe in the game consumes, collected once. There is
	// no iterator over the recipe table, so the ids are walked; what comes back
	// is the only list worth trading, because a counter slot spent on something
	// no anvil asks for is a slot the market cannot use. Eighty-four of them in
	// this world, against two hundred and forty items of type ITEM_MATERIAL.
	const std::set<DWORD>& GetPlayerBotRefineMaterialVnums()
	{
		static std::set<DWORD> s_materials;
		static bool s_loaded = false;
		if (!s_loaded)
		{
			s_loaded = true;
			// Every recipe an item names (wRefineSet), whatever its id, besides
			// the fixed range. The walk used to stop at PLAYERBOT_REFINE_RECIPE_MAX_ID
			// while mt2009's refine_proto runs to 1362, so the thirteen materials
			// only its upper recipes consume were junk to every bot - the Red
			// Seed (30354, recipes 1089-1338) among them, 496 sold to the
			// merchant in one day on the test world (18 September).
			std::set<DWORD> recipeIds;
			for (DWORD id = 1; id <= PLAYERBOT_REFINE_RECIPE_MAX_ID; ++id)
				recipeIds.insert(id);
			const std::vector<TItemTable>& protos = ITEM_MANAGER::instance().GetTable();
			for (size_t i = 0; i < protos.size(); ++i)
				if (protos[i].wRefineSet != 0)
					recipeIds.insert(protos[i].wRefineSet);
			// MT2009_PLUS_AWAKENING_V1: and the Ritual of Awakening's recipe, which
			// no item names (the engine reads it, playerbot_awakening.h) - its
			// Kamien Przebudzenia is a counter's goods, never the merchant's.
			recipeIds.insert(7110); // mt2009_awakening::AWAKENING_REFINE_SET, included later
			// MT2009_PLUS_HEAVEN_OIL_V1 (Autor: Digi Rasta): Olejek Niebios, which only the soul stone
			// steps 7204-7208 consume - a counter's goods. The steps' dust stays
			// out: it has rules of its own (the marble's keep, the Alchemist).
			s_materials.insert(PLAYERBOT_HEAVEN_OIL_VNUM);
			for (std::set<DWORD>::const_iterator id = recipeIds.begin(); id != recipeIds.end(); ++id)
			{
				const TRefineTable* recipe =
						CRefineManager::instance().GetRefineRecipe(*id);
				if (!recipe)
					continue;
				for (int m = 0; m < recipe->material_count; ++m)
					if (recipe->materials[m].vnum != 0)
						s_materials.insert(recipe->materials[m].vnum);
			}
			sys_log(0, "PLAYERBOT_ECONOMY: %u refine materials are worth a counter slot",
					(unsigned int)s_materials.size());
		}
		return s_materials;
	}

	// The materials no weapon or armour recipe asks for. Every one of them in
	// this world feeds only the Herbalist's Knife (item type 35): the herbs of
	// 50721-50736 and the two brews 30341 and 30342, measured on refine_proto
	// against item_proto. No bot carries that knife, so to a bot they are not
	// materials at all - and the rules that keep a material for the anvil, the
	// counter and the storekeeper kept these in 850 bags and 400 safeboxes of
	// our own world ("skladniki na mikstury, ktorych boty nie craftuja,
	// blokuja eq", uxietoszef). They are merchant scrap. Asked of the tables
	// rather than listed, so a world whose recipes differ gets its own answer.
	bool IsPlayerBotNonGearMaterial(DWORD vnum)
	{
		static std::set<DWORD> s_nonGear;
		static bool s_loaded = false;
		if (!s_loaded)
		{
			s_loaded = true;
			std::set<DWORD> gear, other;
			const std::vector<TItemTable>& protos = ITEM_MANAGER::instance().GetTable();
			for (size_t i = 0; i < protos.size(); ++i)
			{
				if (protos[i].wRefineSet == 0)
					continue;
				const TRefineTable* recipe =
						CRefineManager::instance().GetRefineRecipe(protos[i].wRefineSet);
				if (!recipe)
					continue;
				const bool forGear = protos[i].bType == ITEM_WEAPON || protos[i].bType == ITEM_ARMOR;
				for (int m = 0; m < recipe->material_count; ++m)
					if (recipe->materials[m].vnum != 0)
						(forGear ? gear : other).insert(recipe->materials[m].vnum);
			}
			for (std::set<DWORD>::const_iterator it = other.begin(); it != other.end(); ++it)
			{
				// ITEM_MATERIAL only. The walk also finds items of other types that
				// feed nothing but such recipes - 29 of the 47 it found on the first
				// live run: the eleven ores of the pickaxe, the seventeen fish and the
				// shrimp of the rod. Those have rules of their own (the ore trade, the
				// grill) that this one must not overrule.
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(*it);
				if (gear.find(*it) == gear.end() && proto && proto->bType == ITEM_MATERIAL)
					s_nonGear.insert(*it);
			}
			sys_log(0, "PLAYERBOT_ECONOMY: %u ITEM_MATERIAL items feed no weapon or armour recipe",
					(unsigned int)s_nonGear.size());
		}
		return s_nonGear.find(vnum) != s_nonGear.end();
	}

	// The fisherman's keepsakes: kept whatever else is true, because they are
	// the entire point of a fishing trip and the road to +7 and beyond. They are
	// counted here so the stock cap below does not spend its eight cells on them.
	bool IsPlayerBotFishingKeepsake(DWORD vnum)
	{
		return vnum == PLAYERBOT_SHELLFISH_VNUM ||
				(vnum >= PLAYERBOT_PEARL_FIRST_VNUM && vnum <= PLAYERBOT_PEARL_LAST_VNUM);
	}

	// A refine material is whatever a recipe consumes, whatever type the proto
	// gives it. The first version asked for ITEM_MATERIAL as well, and eight of
	// the eighty-four are not: the fishbone is ITEM_RESOURCE, the shellfish and
	// the blessing scroll are ITEM_USE, the three pearls are ITEM_RESOURCE. The
	// fishbone alone is in thirteen recipes and was being sold as "the angler's
	// pocket money".
	bool IsPlayerBotTradeableMaterial(LPITEM item)
	{
		if (!item)
			return false;
		const std::set<DWORD>& materials = GetPlayerBotRefineMaterialVnums();
		return materials.find(item->GetVnum()) != materials.end() &&
				!IsPlayerBotNonGearMaterial(item->GetVnum());
	}

	// A hoard of a refine material: PLAYERBOT_SHOP_HOARD_MIN_UNITS or more over
	// what the bot's own anvil keeps back - two hundred of one material in one
	// bag is not a market, it is a bag, and it opens a counter by itself
	// (PLAYERBOT_SHOP_REASON_HOARD).
	bool IsPlayerBotHoardedMaterial(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !IsPlayerBotTradeableMaterial(item))
			return false;
		const int total = (int)ch->CountSpecifyItem(item->GetVnum());
		if (total < PLAYERBOT_SHOP_HOARD_MIN_UNITS)
			return false;
		return total - GetPlayerBotRefineMaterialReserve(ch, item->GetVnum()) >=
				PLAYERBOT_SHOP_HOARD_MIN_UNITS;
	}

	// Iwakura's Patch 4, point 3: the next counter line of this kind cut the way
	// a player cuts one (playerbot_stall_rules.h) - a refine material one or two
	// at a time and fives past a holding of fifty, a refine scroll mostly one or
	// two, a heap of herbs or hay ten, twenty, fifty or two hundred - given the
	// lines of it the counter shows and how many of those are small (two or
	// fewer). spare is what of the kind may go, over its keep and counted over
	// every stack; avail is that and no more than the stack the line is cut
	// from. Zero when no line can be made of it. A hoard sold in tens until
	// this patch, and "towary zajmuja sloty w pakietach po 50, 11, 5, 4 czy 3
	// sztuki" was the counter it made. A heap is sized from the kind's whole
	// spare (HeapLineFromStack), so a line already cut is that line when the
	// add looks at it again (B01 of Iwakura's audit).
	int GetPlayerBotNaturalLineUnits(LPCHARACTER ch, LPITEM item, int avail, int spare,
			int linesOnCounter, int smallLinesOnCounter)
	{
		if (!ch || !item || avail <= 0)
			return 0;
		const DWORD vnum = item->GetVnum();
		const unsigned seed = PlayerBotNavHash(ch->GetPlayerID() ^ (vnum * 2654435761U) ^
				((DWORD)linesOnCounter << 24) ^ 0x4c4e4553U);
		if (IsPlayerBotSafeRefineScroll(vnum))
			return playerbot_stall_rules::FitSmallGoodsLine(
					playerbot_stall_rules::ScrollLineUnits(seed), avail);
		if (IsPlayerBotBulkGoods(item))
			return playerbot_stall_rules::HeapLineFromStack((int)item->GetCount(), spare, seed);
		if (IsPlayerBotTradeableMaterial(item))
			return playerbot_stall_rules::FitSmallGoodsLine(playerbot_stall_rules::MaterialLineUnits(
					(int)ch->CountSpecifyItem(vnum), smallLinesOnCounter, seed), avail);
		return std::min(GetPlayerBotStallLineUnits(item), avail);
	}

	// Whether a counter line of this kind has a shape a player would cut: the
	// refine materials and scrolls one, two or five, the heaps ten, twenty,
	// fifty or two hundred. Anything else has no such rule and passes.
	bool IsPlayerBotNaturalLine(LPITEM item)
	{
		if (!item)
			return true;
		const int count = (int)item->GetCount();
		if (IsPlayerBotBulkGoods(item))
			return playerbot_stall_rules::IsHeapLine(count);
		if (IsPlayerBotSafeRefineScroll(item->GetVnum()) || IsPlayerBotTradeableMaterial(item))
			return playerbot_stall_rules::IsSmallGoodsLine(count);
		// MT2009_PLUS_DIGI_STACK_V1 (Autor: Digi Rasta's stacking): the soul
		// stones stack to 200 now, and a buyer wants one for one socket - a
		// stone line is one stone (GetPlayerBotStallLineUnits); a stack is no
		// line, it is cut (SplitPlayerBotStallSingles, BotOfflinePrepareLine).
		if (item->GetType() == ITEM_METIN)
			return count == 1;
		return true;
	}

	// The refine scrolls a bot keeps for its own anvil (defined in
	// playerbot_town.h beside the rule that asks it).
	int GetPlayerBotRefineScrollKeep(LPCHARACTER ch);
	// A worn piece the marble's fifth line would go on (playerbot_bonus.h).
	bool PlayerBotWantsBlessingMarble(LPCHARACTER ch);
	// What a bag keeps of a bonus stone kind: PLAYERBOT_BONUS_STONE_KEEP, and
	// no green one for a bot past their band (playerbot_bonus.h).
	int GetPlayerBotBonusStoneKeep(LPCHARACTER ch, LPITEM item);

	// What the stack a counter's lines are cut from keeps back: the anvil's
	// reserve of a material, the keys the bot holds on to, the scrolls of its
	// own scroll work, one of anything else.
	int GetPlayerBotStallBaseKeep(LPCHARACTER ch, LPITEM item)
	{
		if (!item)
			return 1;
		// Asked before the materials: the Blessing Scroll is what recipe 501
		// consumes, so it is a tradeable material too, and the anvil's reserve
		// of it - twice the recipe count of every piece under scroll work - is
		// larger than most stacks. Measured against that, no cut ever had a
		// scroll to spare: a bot with twenty-six put a marble up instead
		// (16 September). The scroll's keep is the scroll rule's.
		if (IsPlayerBotSafeRefineScroll(item->GetVnum()))
			return GetPlayerBotRefineScrollKeep(ch);
		if (IsPlayerBotTradeableMaterial(item))
			return std::max(1, GetPlayerBotRefineMaterialReserve(ch, item->GetVnum()));
		if (item->GetType() == ITEM_TREASURE_KEY)
			return PLAYERBOT_TREASURE_KEY_KEEP;
		if (IsPlayerBotBonusStoneItem(item))
			return GetPlayerBotBonusStoneKeep(ch, item);
		// The medal dropper is the medal shop and keeps one back; everybody else
		// keeps the ladder's two (PLAYERBOT_HORSE_MEDAL_KEEP) and lists the rest.
		// MT2009_PLUS_HORSE30_V1: the next trainings' medals (GetPlayerBotHorseMedalKeep).
		if (item->GetVnum() == PLAYERBOT_HORSE_MEDAL_VNUM)
			return ch && GetPlayerBotPersonalityByPID(ch->GetPlayerID()) ==
					BOT_PERSONALITY_MEDAL_DROPPER ? 1 : (ch ? GetPlayerBotHorseMedalKeep(ch) : PLAYERBOT_HORSE_MEDAL_KEEP);
		// Nobody keeps a root back: the heap is the whole of what it is for.
		if (IsPlayerBotBulkGoods(item))
			return 0;
		// The Alchemist's dust is saved up to the hundred a marble takes
		// (Iwakura's Patch 4, point 11) by every bot that puts bonus lines on
		// its gear, not only while a worn piece already waits for its fifth: it
		// arrives six or seven at a time, and kept only while a piece waited it
		// went to the counters in between - on m2zip (27 September) two bots in
		// 544 held a hundred and one marble was made in two days.
		if (item->GetVnum() == PLAYERBOT_MAGIC_DUST_VNUM)
			return ch && (ch->GetLevel() >= PLAYERBOT_BONUS_MIN_LEVEL || PlayerBotWantsBlessingMarble(ch))
					? PLAYERBOT_DUST_PER_MARBLE : 0;
		// MT2009_PLUS_BOT_CAPE_V1: a bot that uses Peleryna Mestwa keeps its
		// own off the counter (IsPlayerBotCapeBuild).
		if (IsPlayerBotValourCapeVnum(item->GetVnum()) && ch && IsPlayerBotCapeBuild(ch))
			return PLAYERBOT_CAPE_KEEP;
		// MT2009_PLUS_BOT_HERBALIST_ACTIVITY_V1: a potion's split leaves the
		// bot's reserve in the bag (GetPlayerBotCraftedPotionKeep).
		if (IsPlayerBotCraftedPotion(item))
			return std::max(1, GetPlayerBotCraftedPotionKeep(ch, item->GetVnum()));
		return 1;
	}

	// Junk goes to the general-goods merchant on the next town visit, and for a
	// refine material that was the end of it. Five hundred and twenty-six bots
	// on this world are short of one; the top of that list is three hundred and
	// seventeen bots wanting fourteen hundred Orc Amulets between them, against
	// five in existence. A material its finder did not personally need was being
	// destroyed at the rate it dropped, which is why a counter carried one only
	// when somebody happened to have a spare, and why an evening's whole market
	// saw four material purchases.
	//
	// So a spare is kept and put on the counter instead, where the bot that
	// needs it walks up and buys it. Nothing is added to any NPC: the supply is
	// what the world already drops, and the trade is between bots.
	//
	// Bounded, or a bag would fill - there are ninety cells and no shortage of
	// materials to find. The cap counts every material cell, the ones held for
	// this bot's own anvil included, because those are already spoken for above:
	// a material on the wishlist never reaches this test.
	bool IsPlayerBotSurplusMaterial(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !IsPlayerBotTradeableMaterial(item))
			return true;

		// Cell order decides, so the same spares stay put from one town visit
		// to the next rather than the bag reshuffling itself every trip.
		size_t ahead = 0;
		const WORD ownCell = item->GetCell();
		for (WORD cell = 0; cell < ownCell && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM other = ch->GetInventoryItem(cell);
			if (other && other != item && IsPlayerBotTradeableMaterial(other) &&
					!IsPlayerBotFishingKeepsake(other->GetVnum()) &&
					++ahead >= PLAYERBOT_MATERIAL_STOCK_SLOTS)
				return true;
		}
		return false;
	}

	// CountPlayerBotFreeInventoryCells jest w playerbot_consumables.h, ktory
	// jest wlaczany wczesniej: skrzynie musza pytac o to samo, a w jednej
	// jednostce kompilacji definicja moze byc tylko jedna.

	// Occupied cells against PLAYERBOT_BAG_FULL_PERCENT of the bag. Counted
	// by cell rather than by item, so a weapon's three cells count as three.
	bool IsPlayerBotBagFull(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		// The saddlebags' free cells count as room: what lands there comes
		// back down as the bag empties (playerbot_saddlebag.h).
		const int occupied = PLAYERBOT_BAG_CELLS - CountPlayerBotFreeInventoryCells(ch) -
				CountPlayerBotSaddlebagFreeCells(ch);
		return occupied * 100 >= PLAYERBOT_BAG_CELLS * PLAYERBOT_BAG_FULL_PERCENT;
	}

	// Defined with the stall's keeps in playerbot_town.h.
	int CountPlayerBotVnumUnitsAhead(LPCHARACTER ch, LPITEM item);

	// Fewer free cells than the loot and the chests need to land in. The
	// junk rule below reads this together with PlayerBotCanOpenShop: a bag
	// under pressure with a counter to sell from keeps its goods, a bag
	// under pressure with no counter (mt2009 before level 15 and 800 kills)
	// has nowhere but the merchant, and a bot that kept hunting with a full
	// bag "mowi ze podnosi lup ale nie robi nic" (JaroszV2, 11 September).
	bool IsPlayerBotBagUnderPressure(LPCHARACTER ch)
	{
		return ch && ch->IsItemLoaded() &&
				CountPlayerBotFreeInventoryCells(ch) + CountPlayerBotSaddlebagFreeCells(ch) <=
					PLAYERBOT_BAG_PRESSURE_FREE_CELLS;
	}

	// A bag piece this bot would put on: its slot is empty or it outscores
	// what is worn there. Everything else the junk rule may let go under
	// pressure.
	bool IsPlayerBotUpgradeForSelf(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !IsPlayerBotEquipmentCandidate(ch, item) ||
				item->GetLevelLimit() > ch->GetLevel())
			return false;
		const int wearCell = item->FindEquipCell(ch);
		if (wearCell < 0 || wearCell >= WEAR_MAX_NUM)
			return false;
		LPITEM worn = ch->GetWear(wearCell);
		return !worn || GetPlayerBotEquipmentScore(item, ch) > GetPlayerBotEquipmentScore(worn, ch);
	}

	// Books of one skill in the cells before this one, in books. Cell order
	// decides, so the same books stay put from one town visit to the next. It
	// used to count rows, and a book stacked to ten on mt2009 (two hundred
	// since 2.2.26), so "keep twelve" kept twelve stacks - up to a hundred and
	// twenty books of a skill nothing was selling. An item outside the bag (the safebox's, asked
	// whether to come out) has no cells before it: the whole bag is in front.
	int CountPlayerBotSkillBooksAhead(LPCHARACTER ch, LPITEM item, DWORD skillVnum)
	{
		int ahead = 0;
		const WORD ownCell = item->GetWindow() == INVENTORY
				? item->GetCell() : (WORD)PLAYERBOT_BAG_CELLS;
		for (WORD cell = 0; cell < ownCell && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM other = ch->GetInventoryItem(cell);
			if (other && other != item && other->GetCell() == cell &&
					other->GetType() == ITEM_SKILLBOOK &&
					GetPlayerBotSkillBookSkillVnum(other) == skillVnum)
				ahead += std::max<int>(1, other->GetCount());
		}
		return ahead;
	}

	// Defined further down (playerbot_progression_needs.h): a bot's books of
	// one skill over the whole bag, and whether a skill of its build is at a
	// Grand Master grade the soul stone trains.
	int CountPlayerBotOwnedSkillBooks(LPCHARACTER ch, DWORD skill);
	bool PlayerBotHasGrandMasterToTrain(LPCHARACTER ch);

	// The goods a bot keeps a number of and sells the rest of one at a time: a
	// skill book (its kind is its skill) and the soul stone. A counter line of
	// either is a single (GetPlayerBotStallLineUnits), because a buyer takes a
	// line only when all of it fits what it is short of.
	// The goods a bot keeps by count over the whole bag and cuts lines of from
	// what is over the keep: its own skill books, the soul stone and the Zen
	// bean (GetPlayerBotCountedGoodsKeep).
	// MT2009_PLUS_BOTLIFE_V1: the jewellery's refine stones ("przetopy"):
	// the Diament opens a socket (USE_ADD_ACCESSORY_SOCKET), the Ebonit and
	// the rest go into one (USE_PUT_INTO_ACCESSORY_SOCKET). By subtype, so
	// every stone item_proto carries is one - 50621 and 50623-50638 here.
	// They were never goods: the scorer had no branch for them and the junk
	// rule keeps them from the merchant, so they rode in the bags for good
	// (kuszaa, 30 September). Now a bot keeps what its own jewellery takes
	// (GetPlayerBotAccessoryStoneKeep) and the rest is counted goods, a
	// stone a line, at the price list's price.
	bool IsPlayerBotAccessoryStoneOf(BYTE type, BYTE subType)
	{
		return type == ITEM_USE &&
				(subType == USE_ADD_ACCESSORY_SOCKET || subType == USE_PUT_INTO_ACCESSORY_SOCKET);
	}

	bool IsPlayerBotAccessoryStone(LPITEM item)
	{
		return item && IsPlayerBotAccessoryStoneOf(item->GetType(), item->GetSubType());
	}

	bool IsPlayerBotAccessoryStoneVnum(DWORD vnum)
	{
		const TItemTable* proto = vnum ? ITEM_MANAGER::instance().GetTable(vnum) : NULL;
		return proto && IsPlayerBotAccessoryStoneOf(proto->bType, proto->bSubType);
	}

	// How many of this stone the bot's own jewellery still takes: for a stone
	// that goes into a socket, every socket of a worn earring, bracelet or
	// necklace it fits (CItem::CanPutInto) that it does not fill yet - the
	// ones still to be opened included, a Diament opens them - and for the
	// Diament every socket still to be opened. The pieces the Gornik's own
	// work fills (ManagePlayerBotAccessorySockets, playerbot_mining.h).
	int GetPlayerBotAccessoryStoneKeep(LPCHARACTER ch, LPITEM stone)
	{
		if (!ch || !IsPlayerBotAccessoryStone(stone))
			return 0;
		static const BYTE slots[] = { WEAR_EAR, WEAR_WRIST, WEAR_NECK };
		int keep = 0;
		for (size_t s = 0; s < sizeof(slots) / sizeof(slots[0]); ++s)
		{
			LPITEM piece = ch->GetWear(slots[s]);
			if (!piece || !piece->IsAccessoryForSocket())
				continue;
			if (stone->GetSubType() == USE_ADD_ACCESSORY_SOCKET)
				keep += std::max(0, (int)ITEM_ACCESSORY_SOCKET_MAX_NUM - piece->GetAccessorySocketMaxGrade());
			else if (stone->CanPutInto(piece))
				keep += std::max(0, (int)ITEM_ACCESSORY_SOCKET_MAX_NUM - piece->GetAccessorySocketGrade());
		}
		return keep;
	}

	bool IsPlayerBotCountedSingleGoods(LPITEM item)
	{
		return item && (item->GetType() == ITEM_SKILLBOOK ||
				item->GetVnum() == PLAYERBOT_GRAND_MASTER_STONE_VNUM ||
				item->GetVnum() == PLAYERBOT_ZEN_BEAN_VNUM ||
				IsPlayerBotAccessoryStone(item));   // MT2009_PLUS_BOTLIFE_V1
	}

	// What PLAYERBOT_SHOP_SAME_VNUM_LINES caps by vnum: everything but the
	// goods counted by kind, a Forgetting Scroll (its skill is its kind) and a
	// marble (PLAYERBOT_SHOP_MARBLE_LINES, one a monster).
	bool IsPlayerBotSameVnumCapped(LPITEM item)
	{
		return item && !IsPlayerBotCountedSingleGoods(item) &&
				item->GetType() != ITEM_SKILLFORGET && item->GetType() != ITEM_POLYMORPH;
	}

	// How many lines of one item a counter shows: a heap's
	// PLAYERBOT_SHOP_BULK_LINES, a refine scroll's PLAYERBOT_SHOP_SCROLL_LINES,
	// a refine material's PLAYERBOT_SHOP_MATERIAL_LINES (eight since Iwakura's
	// Patch 4 - lines of one and two need more of them), anything else capped
	// by vnum PLAYERBOT_SHOP_SAME_VNUM_LINES; zero for what is counted by kind
	// or by monster elsewhere. The classic collector, the offline add and the
	// take-home all ask this one question.
	int GetPlayerBotCounterLineCap(LPITEM item)
	{
		if (!item)
			return 0;
		if (IsPlayerBotBulkGoods(item))
			return PLAYERBOT_SHOP_BULK_LINES;
		if (IsPlayerBotSafeRefineScroll(item->GetVnum()))
			return PLAYERBOT_SHOP_SCROLL_LINES;
		if (IsPlayerBotTradeableMaterial(item))
			return PLAYERBOT_SHOP_MATERIAL_LINES;
		return IsPlayerBotSameVnumCapped(item) ? PLAYERBOT_SHOP_SAME_VNUM_LINES : 0;
	}

	// One key per kind of those goods, zero for anything else: a book's skill
	// with the top bit set (socket 0 of the engine's 50300, the first value of
	// a named book - GetPlayerBotSkillBookSkillVnum), the stone's vnum. Asked of
	// the raw fields too, because an offline counter's line is not an item.
	DWORD GetPlayerBotStallKindKeyOf(DWORD vnum, BYTE type, long socket0, long value0)
	{
		if (type == ITEM_SKILLBOOK)
			return 0x80000000U | (DWORD)(vnum == 50300 ? socket0 : value0);
		// MT2009_PLUS_BOTLIFE_V1: a refine stone's kind is its vnum.
		if (type == ITEM_USE && IsPlayerBotAccessoryStoneVnum(vnum))
			return vnum;
#if defined(PLAYERBOT_ENGINE_MT2009)
		// MT2009_PLUS_BOT_HERBALIST_ACTIVITY_V1: a potion's kind is its vnum,
		// so the classic stall never lists the stack that holds the bot's
		// reserve (MayListWhole, GetPlayerBotCountedGoodsKeep).
		if (type == ITEM_POTION)
			return vnum;
#endif
		return vnum == PLAYERBOT_GRAND_MASTER_STONE_VNUM || vnum == PLAYERBOT_ZEN_BEAN_VNUM ? vnum : 0;
	}

	// The beans this bot keeps (PLAYERBOT_ZEN_BEAN_KEEP_MIN..MAX), the same
	// number every time it is asked.
	int GetPlayerBotZenBeanKeep(LPCHARACTER ch)
	{
		if (!ch)
			return PLAYERBOT_ZEN_BEAN_KEEP_MAX;
		return PLAYERBOT_ZEN_BEAN_KEEP_MIN + (int)(PlayerBotNavHash(ch->GetPlayerID() ^ 0x5a454e42U) %
				(DWORD)(PLAYERBOT_ZEN_BEAN_KEEP_MAX - PLAYERBOT_ZEN_BEAN_KEEP_MIN + 1));
	}

	DWORD GetPlayerBotStallKindKey(LPITEM item)
	{
		return item ? GetPlayerBotStallKindKeyOf(item->GetVnum(), item->GetType(),
				item->GetSocket(0), item->GetValue(0)) : 0;
	}

	// What the bag keeps of that kind: the books of its own skill it will read
	// (GetPlayerBotBookKeepLimit) and none of anybody else's; three stones while
	// a skill stands at a grade they train, one against the day one will. The
	// same numbers the buyer's side asks for (GetPlayerBotProgressionNeed), so a
	// counter never sells what its keeper would walk to the market to buy back.
	int GetPlayerBotCountedGoodsKeep(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return 0;
		if (item->GetType() == ITEM_SKILLBOOK)
		{
			const DWORD skill = GetPlayerBotSkillBookSkillVnum(item);
			return ch->GetSkillGroup() != 0 && IsPlayerBotOwnSkill(ch, skill)
					? GetPlayerBotBookKeepLimit(ch, skill) : 0;
		}
		if (item->GetVnum() == PLAYERBOT_GRAND_MASTER_STONE_VNUM)
			return PlayerBotHasGrandMasterToTrain(ch) ? PLAYERBOT_GRAND_MASTER_STONE_KEEP : 1;
		if (item->GetVnum() == PLAYERBOT_ZEN_BEAN_VNUM)
			return GetPlayerBotZenBeanKeep(ch);
		if (IsPlayerBotBonusStoneItem(item))
			return GetPlayerBotBonusStoneKeep(ch, item);
		// MT2009_PLUS_BOTLIFE_V1: what the bot's jewellery still takes.
		if (IsPlayerBotAccessoryStone(item))
			return GetPlayerBotAccessoryStoneKeep(ch, item);
		// MT2009_PLUS_BOT_HERBALIST_ACTIVITY_V1: the potion reserve.
		if (IsPlayerBotCraftedPotion(item))
			return GetPlayerBotCraftedPotionKeep(ch, item->GetVnum());
		return 0;
	}

	// Units of the item's kind over the whole bag.
	int CountPlayerBotStallKindUnits(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return 0;
		if (item->GetType() == ITEM_SKILLBOOK)
			return CountPlayerBotOwnedSkillBooks(ch, GetPlayerBotSkillBookSkillVnum(item));
		return (int)ch->CountSpecifyItem(item->GetVnum());
	}

	// A key whose lock matches this chest, anywhere in the bag.
	bool PlayerBotHasTreasureKeyFor(LPCHARACTER ch, LPITEM box)
	{
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM key = ch->GetInventoryItem(cell);
			if (key && key->GetType() == ITEM_TREASURE_KEY && key->GetValue(0) == box->GetValue(0))
				return true;
		}
		return false;
	}

	// A chest this key opens, anywhere in the bag.
	bool PlayerBotHasTreasureBoxFor(LPCHARACTER ch, LPITEM key)
	{
		for (WORD cell = 0; ch && key && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM box = ch->GetInventoryItem(cell);
			if (box && box->GetType() == ITEM_TREASURE_BOX && box->GetValue(0) == key->GetValue(0))
				return true;
		}
		return false;
	}

	// A key for a chest the bot holds and has no key for: what a counter or the
	// storekeeper should hand back to it.
	bool PlayerBotWantsTreasureKey(LPCHARACTER ch, LPITEM key)
	{
		if (!ch || !key || key->GetType() != ITEM_TREASURE_KEY || !PlayerBotHasTreasureBoxFor(ch, key))
			return false;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM held = ch->GetInventoryItem(cell);
			if (held && held != key && held->GetType() == ITEM_TREASURE_KEY &&
					held->GetValue(0) == key->GetValue(0))
				return false;
		}
		return true;
	}

	// A key past the PLAYERBOT_TREASURE_KEY_KEEP of its kind the bot holds on
	// to, with no chest in the bag it opens: goods for a counter, and the
	// storekeeper's when the bag is short of room. A key was never the
	// merchant's and never a counter's, while the chests it opens go to the
	// merchant under bag pressure - so on 15 September 2598 gold and silver keys
	// lay in 1057 bags and not one of those bags held a chest ("srebrne i zlote
	// klucze ... chomikuja to bez konca"). The last units in cell order
	// are the ones kept.
	bool IsPlayerBotSurplusTreasureKey(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetType() != ITEM_TREASURE_KEY || PlayerBotHasTreasureBoxFor(ch, item))
			return false;
		const int total = (int)ch->CountSpecifyItem(item->GetVnum());
		int ahead = 0;
		for (WORD cell = 0; cell < item->GetCell() && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM other = ch->GetInventoryItem(cell);
			if (other && other != item && other->GetVnum() == item->GetVnum())
				ahead += std::max<int>(1, other->GetCount());
		}
		return ahead < total - PLAYERBOT_TREASURE_KEY_KEEP;
	}

	// How many bots are short of a material and can pay for it, from the
	// market ledger (playerbot_market.h, which comes after this fragment).
	DWORD GetPlayerBotLedgerDemand(DWORD vnum);

	// A spare of a higher tier than the piece worn in its slot: not an
	// upgrade yet, but one the blacksmith can make into one, so neither the
	// merchant nor the refine pass treats it as scrap. Only the best such
	// spare per slot counts; the rest are still scrap.
	// A candidate that outranks the worn piece: by the engine's level limit,
	// or by Iwakura's PvE tier where his list rates both families - a bot
	// wearing Miedziane Kolczyki with Ebonitowe in the bag has an upgrade to
	// make whatever the level limits say, and his instruction is to refine
	// and bonus it before wearing it, not to swap blindly (16 September).
	bool PlayerBotOutranksWornTier(LPCHARACTER ch, LPITEM cand, LPITEM worn)
	{
		if (cand->GetLevelLimit() > worn->GetLevelLimit())
			return true;
		const int candTier = GetPlayerBotItemTierOf(cand, ch);
		const int wornTier = GetPlayerBotItemTierOf(worn, ch);
		return candTier > 0 && wornTier > 0 && candTier > wornTier;
	}

	bool IsPlayerBotHigherTierSpare(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !IsPlayerBotEquipmentCandidate(ch, item))
			return false;
		const int wearCell = item->FindEquipCell(ch);
		if (wearCell < 0 || wearCell >= WEAR_MAX_NUM)
			return false;
		if (item->GetLevelLimit() > ch->GetLevel())
			return false;
		LPITEM worn = ch->GetWear(wearCell);
		if (!worn || !PlayerBotOutranksWornTier(ch, item, worn))
			return false;
		// A piece already at +7 or past that the worn one matches or beats is
		// goods, not a project for the anvil (Iwakura's Patch 4, point 7).
		if (IsPlayerBotFinishedSpareGoods(ch, item))
			return false;
		const long long itemScore = GetPlayerBotEquipmentScore(item, ch);
		for (WORD otherCell = 0; otherCell < PLAYERBOT_BAG_CELLS; ++otherCell)
		{
			LPITEM other = ch->GetInventoryItem(otherCell);
			if (!other || other == item || !IsPlayerBotEquipmentCandidate(ch, other) ||
					other->GetLevelLimit() > ch->GetLevel() ||
					!PlayerBotOutranksWornTier(ch, other, worn) ||
					other->FindEquipCell(ch) != wearCell)
				continue;
			const long long otherScore = GetPlayerBotEquipmentScore(other, ch);
			if (otherScore > itemScore ||
					(otherScore == itemScore && other->GetID() < item->GetID()))
				return false;
		}
		return true;
	}

	// Gear under PLAYERBOT_SHOP_MIN_GEAR_LEVEL: what a counter carries only at
	// PLAYERBOT_SHOP_LOW_GEAR_MIN_REFINE and only a little of, and what the
	// merchant takes below that.
	bool IsPlayerBotLowLevelGear(LPITEM item)
	{
		return item && (item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR) &&
				item->GetLevelLimit() < PLAYERBOT_SHOP_MIN_GEAR_LEVEL;
	}

	// The refine that gear needs for a counter: +6, and +7 for a weapon or a body
	// armour of level one (PLAYERBOT_SHOP_STARTER_GEAR_MIN_REFINE). It is one
	// number for two decisions - what a counter takes and what the junk rule
	// scraps under - so raising it past seven is what sent a starter +7 to a
	// merchant, against "+7 to nigdy nie jest zlom".
	BYTE GetPlayerBotLowGearMinRefine(LPITEM item)
	{
		if (item && (int)item->GetLevelLimit() <= PLAYERBOT_SHOP_STARTER_GEAR_MAX_LEVEL &&
				(item->GetType() == ITEM_WEAPON ||
				 (item->GetType() == ITEM_ARMOR && item->GetSubType() == ARMOR_BODY)))
			return PLAYERBOT_SHOP_STARTER_GEAR_MIN_REFINE;
		return PLAYERBOT_SHOP_LOW_GEAR_MIN_REFINE;
	}

	// Whether a counter line of that gear takes one of the
	// PLAYERBOT_SHOP_LOW_GEAR_MAX_LINES places: +7 and better does not.
	bool CountsAgainstPlayerBotLowGearCap(LPITEM item)
	{
		// MT2009_PLUS_BOT_LIST_ALL_GEAR_V1: the gear that rule lists is held
		// to its own lines of a kind (PLAYERBOT_ALL_GEAR_KIND_LINES) instead.
		return IsPlayerBotLowLevelGear(item) && !IsPlayerBotSaleGear(item) &&
				item->GetRefineLevel() < PLAYERBOT_SHOP_LOW_GEAR_CAP_BELOW_REFINE;
	}

	// Iwakura's Useful Items List (playerbot_lpp.h).
	bool IsPlayerBotLppKeptItem(LPCHARACTER ch, LPITEM item);

	// "Jesli bot chce wystawic taki przedmiot, musi najpierw ulepszyc go
	// minimum do poziomu +5" (Iwakura's Patch 3, point 4): a body armour of a
	// family whose cap is full goes to the plain anvil for +5 while the purse
	// and the bag can pay the next step, and is goods once it is there. From
	// level thirty only: under it a +5 is no counter's (the operator's +6,
	// GetPlayerBotLowGearMinRefine), and the anvil would be paid for a piece the
	// junk rule then hands the merchant - which never came up while no armour
	// under thirty went up at +0..+4 at all.
	bool PlayerBotRefinesLowArmourForSale(LPCHARACTER ch, LPITEM item)
	{
		return ch && IsPlayerBotCappedLowArmour(item) && !IsPlayerBotLowLevelGear(item) && !item->IsEquipped() &&
				IsPlayerBotLowArmourMarketFull(item->GetVnum()) &&
				!IsPlayerBotLppKeptItem(ch, item) && !IsPlayerBotKeptBackupArmour(ch, item) &&
				CanPlayerBotPayRefineStep(ch, item);
	}

	// MT2009_PLUS_BOT_LIST_ALL_GEAR_V1: a spare piece in the bag of a bot
	// with a counter, under PLAYERBOT_SPARE_SALE_PLUS, goes to the plain
	// anvil for it while the purse (over the reserve) and the bag can pay the
	// next step - the usual refine pass, last in its order - and is goods
	// once it is there, or at what it is when the step cannot be paid. Not a
	// piece the bot wears, means to wear, keeps for the day its own burns,
	// keeps for the gambler or the operator, nor one another rule refines.
	bool PlayerBotRefinesSpareForSale(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->IsEquipped() || item->GetWindow() != INVENTORY ||
				item->GetRefinedVnum() == 0 || item->GetRefineLevel() >= PLAYERBOT_SPARE_SALE_PLUS ||
				!IsPlayerBotSaleGear(item) || !PlayerBotHasCounter(ch) ||
				GetPlayerBotItemPolicy(item) != PLAYERBOT_ITEM_POLICY_NONE)
			return false;
		if (IsPlayerBotSidekickPinned(ch, item) || IsPlayerBotLppKeptItem(ch, item) ||
				IsPlayerBotGambleForSale(ch, item) || IsPlayerBotRareGambleHeldBase(ch, item) ||
				IsPlayerBotSashGrailProject(ch, item) || IsPlayerBotArcherStoneWeapon(ch, item) ||
				PlayerBotRefinesLowArmourForSale(ch, item))
			return false;
		if (IsPlayerBotUpgradeForSelf(ch, item) || IsPlayerBotHigherTierSpare(ch, item) ||
				IsPlayerBotWearableUpgrade(ch, item, item->GetCell()))
			return false;
		if (item->GetType() == ITEM_WEAPON &&
				(IsPlayerBotKeptBackupWeapon(ch, item) || item == FindPlayerBotLinesProject(ch)))
			return false;
		if (item->GetType() == ITEM_ARMOR && IsPlayerBotKeptBackupArmour(ch, item))
			return false;
		return CanPlayerBotPayRefineStep(ch, item);
	}

	// Iwakura's Patch 4, point 13: the mission books - Latwa, Normalna, Trudna,
	// ekspert - stand on one village's counters thirty at most, the four kinds
	// together (playerbot_stall_rules::MISSION_BOOK_MAP_CAP). On m2zip on 25
	// September each first village's counters held some 2 100 of them in 1 500
	// lines ("zbyt duza liczba Ksiag Misji generowala niepotrzebny chaos w
	// sklepach offline"), and the bags 5 500 more.
	bool IsPlayerBotMissionBook(DWORD vnum)
	{
		return vnum >= PLAYERBOT_MISSION_BOOK_FIRST_VNUM && vnum <= PLAYERBOT_MISSION_BOOK_LAST_VNUM;
	}

	// The mission books the bots' counters of a village hold
	// (s_mapPlayerBotMissionBooksByMap, rebuilt with the ledger once a minute
	// and kept up between by every add and take-off). A person's counter is
	// not the bots' cap to keep.
	int CountPlayerBotMissionBooksOnMap(long lMapIndex)
	{
		std::map<long, int>::const_iterator it = s_mapPlayerBotMissionBooksByMap.find(lMapIndex);
		return it == s_mapPlayerBotMissionBooksByMap.end() ? 0 : it->second;
	}

	// The village whose counters a bot's mission books would stand on: its own
	// stand's, else the village it stands in, else its kingdom's first village.
	long GetPlayerBotMissionBookMarketMap(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		auto shop = ikashop::GetManager().GetShopByOwnerID(ch->GetPlayerID());
		if (shop)
			return shop->GetSpawn().map;
#endif
		if (IsPlayerBotVillageMap(ch->GetMapIndex()))
			return ch->GetMapIndex();
		return playerbot_empire_rules::GetHomeMap((int)ch->GetEmpire(),
				playerbot_empire_rules::MAP_ROLE_M1);
	}

	bool IsPlayerBotMissionBookMarketFull(LPCHARACTER ch)
	{
		const long map = GetPlayerBotMissionBookMarketMap(ch);
		return map != 0 && CountPlayerBotMissionBooksOnMap(map) >=
				playerbot_stall_rules::MISSION_BOOK_MAP_CAP;
	}

	// Where a bot's own go while its village is full of them: the storekeeper
	// or the general merchant, a coin for each kind ("rzut moneta, szansa
	// 50/50").
	bool PlayerBotMissionBookGoesToSafebox(LPCHARACTER ch, DWORD vnum)
	{
		return ch && playerbot_stall_rules::MissionBookGoesToSafebox(
				PlayerBotNavHash(ch->GetPlayerID() ^ (vnum << 8) ^ 0x4d495342U));
	}

	// MT2009_PLUS_BOT_HAIR_V1 (the owner, 2 October): bots bought plain
	// hairstyles from the ItemShop and listed them by the hundred (750 pages
	// of "Dlugie Wlosy" and "Modnie Sciete" at 2 100 000 yang each). A bot
	// buys and wears only a hairstyle of a costume set now - one that
	// costume_sets.txt names, the set bonus's own lists - and the one of its
	// worn costume's set first (playerbot_itemshop.h). A hairstyle goes on
	// a counter in one case alone: the one the bot wore until a set one
	// replaced it. Every other hairstyle off its head - one for another
	// class or sex, a plain one, a stand's old stock - comes home and is
	// thrown away at the merchant, for nothing (no yang made of it).
	bool IsPlayerBotHairItem(LPITEM item)
	{
		return item && item->GetType() == ITEM_COSTUME && item->GetSubType() == COSTUME_HAIR;
	}

	bool IsPlayerBotSetHairVnum(DWORD vnum)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		return Mt2009PlusIsCostumeSetHair(vnum);
#else
		(void)vnum;
		return true;
#endif
	}

	bool IsPlayerBotHairOfBodySet(DWORD bodyVnum, DWORD hairVnum)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		return Mt2009PlusIsCostumeSetPair(bodyVnum, hairVnum);
#else
		(void)bodyVnum;
		(void)hairVnum;
		return false;
#endif
	}

	// Whether this hairstyle would be better on the bot's head than what is
	// there: a set one over none or over a plain one, and the worn costume's
	// own set's over another set's.
	bool IsPlayerBotHairUpgrade(LPCHARACTER ch, DWORD hairVnum)
	{
		if (!ch || !IsPlayerBotSetHairVnum(hairVnum))
			return false;
		LPITEM worn = ch->GetWear(WEAR_COSTUME_HAIR);
		if (!worn)
			return true;
		if (worn->GetVnum() == hairVnum)
			return false;
		if (!IsPlayerBotSetHairVnum(worn->GetVnum()))
			return true;
		LPITEM body = ch->GetWear(WEAR_COSTUME_BODY);
		return body && !IsPlayerBotHairOfBodySet(body->GetVnum(), worn->GetVnum()) &&
				IsPlayerBotHairOfBodySet(body->GetVnum(), hairVnum);
	}

	// A hairstyle in the bag on its way to the bot's head.
	bool IsPlayerBotHairToWear(LPCHARACTER ch, LPITEM item)
	{
		return IsPlayerBotHairItem(item) && !item->IsEquipped() && item->CanUsedBy(ch) &&
				IsPlayerBotHairUpgrade(ch, item->GetVnum());
	}

	// The hairstyle the bot wore until a set one replaced it: the only one
	// that goes on a counter. Asked of a counter's line too (a preview).
	bool IsPlayerBotReplacedHair(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !IsPlayerBotHairItem(item) || item->IsEquipped() || !item->CanUsedBy(ch))
			return false;
		LPITEM worn = ch->GetWear(WEAR_COSTUME_HAIR);
		return worn && worn != item && IsPlayerBotSetHairVnum(worn->GetVnum()) &&
				!IsPlayerBotHairUpgrade(ch, item->GetVnum());
	}

	// Any other hairstyle off the bot's head: thrown away at the merchant.
	bool IsPlayerBotUnwantedHair(LPCHARACTER ch, LPITEM item)
	{
		return IsPlayerBotHairItem(item) && !item->IsEquipped() &&
				!IsPlayerBotHairToWear(ch, item) && !IsPlayerBotReplacedHair(ch, item);
	}

	bool IsPlayerBotJunkItem(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->IsEquipped() || item->isLocked())
			return false;
		// What a player handed a companion is the player's choice, not the
		// merchant's (playerbot_sidekick.h), and so is what the player put on
		// it, waiting in the bag for its slot.
		if (IsPlayerBotSidekickGift(ch, item) || IsPlayerBotSidekickPinned(ch, item))
			return false;

		// The operator's word first: merchant is scrap whatever the rules
		// below would keep it for; keep, stall and drop are never scrap (drop
		// is thrown away by the merchant leg, not sold).
		{
			const BYTE policy = GetPlayerBotItemPolicy(item);
			if (policy == PLAYERBOT_ITEM_POLICY_MERCHANT)
				return true;
			if (policy != PLAYERBOT_ITEM_POLICY_NONE)
				return false;
		}

		// The guild building materials are goods now (playerbot_guild_land.h):
		// the counters, never the merchant.
		if (IsPlayerBotGuildBuildMaterial(item->GetVnum()))
			return false;

		// MT2009_PLUS_AWAKENING_V1 / MT2009_PLUS_SOULSTONE9_V1 (Digi Rasta's
		// systems): the Awakening Stone, an awakened weapon and a soul stone
		// +5..+9 are never the merchant's - a counter's or the bot's own
		// (playerbot_awakening.h).
		if (IsPlayerBotAwakeningGoods(item->GetVnum()))
			return false;

		// A piece Iwakura's list keeps for the storekeeper is never the
		// merchant's, whatever the rules below would make of it.
		if (IsPlayerBotLppKeptItem(ch, item))
			return false;
		// Nor what a gambler's session made or took off for it, which goes on
		// the counter at any plus, nor a base one of Community Patch 5's four
		// gamblers holds for its anvil (playerbot_gambler.h): the merchant's
		// round comes before the anvil in a visit, and a base the gambler
		// bought off a counter was scrap to the rules below.
		if (IsPlayerBotGambleForSale(ch, item) || IsPlayerBotRareGambleHeldBase(ch, item))
			return false;
		// Nor a Stalki, of any class and at any plus (the operator's decision of
		// 28 September, playerbot_stalki.h): the one a bot keeps for its next
		// levels waits in the bag and the rest are a counter's. The default
		// below sold a level-75 weapon of another class at +0..+3 for a fifth
		// of its merchant price, and the level rule under it sold the armour a
		// bot of sixty-four had just picked up for the level it was reaching.
		if (IsPlayerBotStalkiItem(item))
			return false;
		// MT2009_PLUS_BONUS_COUNT_PRICE_V1: nor a piece whose lines are its
		// worth - a +7's by MARKET_V3, or the owner's add-on a line while no
		// Moonlight chests are in the world (IsPlayerBotBonusGoodsPiece): the
		// counter's, and the merchant's only from a bag under pressure that
		// has no counter, the rule a polymorph marble keeps. The merchant paid
		// a few thousand for a bracelet a player pays millions for.
		if ((item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR) && IsPlayerBotBonusGoodsPiece(item) &&
				!(IsPlayerBotBagUnderPressure(ch) && !PlayerBotHasCounter(ch)))
			return false;

		// A Cor Draconis or a sash (MT2009 Plus) is the counter's. The
		// merchant takes it once a line of its kind came home from this bot's
		// counter unsold (NotePlayerBotRareGoodsUnsold), and from a bag under
		// pressure that has no counter for it: none at all, or the counters'
		// share of the kind is taken (IsPlayerBotRareGoodsShopQuotaFull).
		{
			if (IsPlayerBotKeptSash(ch, item) || IsPlayerBotKeptCor(ch, item))
				return false;
			const int rareKind = GetPlayerBotRareGoodsKind(item->GetVnum());
			if (rareKind != PLAYERBOT_RARE_GOODS_NONE)
			{
				if (IsPlayerBotRareGoodsForMerchant(ch->GetPlayerID(), item->GetVnum(), get_dword_time()))
					return true;
				// MT2009_PLUS_BOT_SASH_FLOW_V1: two or more sashes for a counter
				// go up whatever the counters' share (BotOfflineCounterRefuses);
				// under pressure the merchant takes only the plain +0 ones past
				// what the counter holds (IsPlayerBotSashForMerchant).
				if (rareKind == PLAYERBOT_RARE_GOODS_SASH && PlayerBotHasCounter(ch) &&
						GetPlayerBotSashGoodsInBag(ch) >= 2)
					return IsPlayerBotSashForMerchant(ch, item);
				return IsPlayerBotBagUnderPressure(ch) &&
						(!PlayerBotHasCounter(ch) || IsPlayerBotRareGoodsShopQuotaFull(rareKind));
			}
		}

		const DWORD vnum = item->GetVnum();

		// Maska Sabaha left the world with the Hwang curse (IsPlayerBotRetiredItem):
		// the merchant takes the ones still in bags.
		if (IsPlayerBotRetiredItem(vnum))
			return true;
		// So are the uniques a bot leaves on the ground (IsPlayerBotLeftOnGroundItem).
		if (IsPlayerBotLeftOnGroundItem(vnum))
			return true;
		// The Demon Tower's keys are the floor's while the bot is in the tower
		// and nothing anywhere else (the quest takes a player's on logout).
		if (IsPlayerBotDemonTowerKey(vnum))
			return !IsPlayerBotDemonTowerInstance(ch->GetMapIndex());
		// So are the Catacomb's key and totem, in the Catacomb.
		if (IsPlayerBotCatacombKey(vnum))
			return ch->GetMapIndex() != PLAYERBOT_MAP_CATACOMB && !IsPlayerBotCatacombInstance(ch->GetMapIndex());
// A Dried Head is a player's way into the Catacomb's third floor: the
		// counter's, never the merchant's, who paid nothing for it.
		if (IsPlayerBotCatacombHead(vnum))
			return false;
		// Pirate Tanaka's ear is Yonah's, for a Purple Ebony Chest
		// (ManagePlayerBotTanakaEars): the merchant pays 2 500 for it.
		if (vnum == PLAYERBOT_TANAKA_EAR_VNUM)
			return false;
		// One of Iwakura's fifty-four weapons nobody buys at +0..+3 goes to the
		// merchant once the bots' counters carry PLAYERBOT_JUNK_WEAPON_MARKET_CAP
		// of them - unless this bot will wear it, refine it or keep it as the
		// weapon for the day its own burns. Asked before the pickup goods below,
		// which would keep the level-65 ones for a counter that has no room.
		if (IsPlayerBotCappedJunkWeapon(item) && IsPlayerBotJunkWeaponMarketFull() &&
				!IsPlayerBotUpgradeForSelf(ch, item) && !IsPlayerBotHigherTierSpare(ch, item) &&
				!IsPlayerBotKeptBackupWeapon(ch, item) &&
				// MT2009_PLUS_BOT_LIST_ALL_GEAR_V1: nor one the anvil takes to
				// +4 for the counter, where the cap no longer holds it.
				!(PlayerBotHasCounter(ch) && PlayerBotRefinesSpareForSale(ch, item)))
			return true;
		// And a body armour at +0..+4 of a family at its cap on the market
		// (Iwakura's Patch 3, point 4), unless the bot wears it, raises it for
		// itself, takes it to +5 for the counter, or keeps it for the gambler.
		if (IsPlayerBotCappedLowArmour(item) && IsPlayerBotLowArmourMarketFull(item->GetVnum()) &&
				!IsPlayerBotUpgradeForSelf(ch, item) && !IsPlayerBotHigherTierSpare(ch, item) &&
				!PlayerBotRefinesLowArmourForSale(ch, item) && !IsPlayerBotLppKeptItem(ch, item) &&
				!IsPlayerBotKeptBackupArmour(ch, item))
			return true;
		// A mission book whose village is full of them is the merchant's for
		// half the kinds and the storekeeper's for the other half
		// (PlayerBotMissionBookGoesToSafebox; Patch 4, point 13). Asked before
		// the pickup goods, which would keep it for a counter with no room.
		if (IsPlayerBotMissionBook(vnum) && IsPlayerBotMissionBookMarketFull(ch))
			return !PlayerBotMissionBookGoesToSafebox(ch, vnum);
		// MT2009_PLUS_MARKET_V3, point 8: Siano is never a counter's. What the
		// General Store did not change for potions (ExchangePlayerBotHay) it
		// buys - asked before the pickup goods, which would keep it for one.
		// MT2009_PLUS_BOT_HORSE_HAY_V1: the horse's PLAYERBOT_HAY_KEEP stay in
		// the bag; only the hay over them is junk, and the sale below sells
		// just that much of the stack (SellPlayerBotSurplusHay).
		if (vnum == PLAYERBOT_HAY_VNUM)
			return (int)ch->CountSpecifyItem(PLAYERBOT_HAY_VNUM) > PLAYERBOT_HAY_KEEP;
		// MT2009_PLUS_HORSE30_V1: Marchewka and Czerwony Zen-szen are the
		// training's feed of 11-19 and 21-28 (playerbot_horse30.h): the five
		// the next training eats stay, the rest is the merchant's (the
		// stable sells what the bag lacks at the training itself).
		if (vnum == PLAYERBOT_HORSE_FEED_CARROT || vnum == PLAYERBOT_HORSE_FEED_GINSENG)
			return (int)ch->CountSpecifyItem(vnum) > GetPlayerBotHorseFeedKeep(ch, vnum);
		// The goods a player crafts further (IsPlayerBotPickupGoods) wait for a
		// counter, and reach the merchant only from a bag under pressure that
		// has no counter to sell from - the rule a polymorph marble keeps. Gear
		// among them waits only up to PLAYERBOT_PICKUP_GEAR_BAG_KEEP of a piece,
		// the first in the bag; a counter shows no more than that of one thing,
		// and every one past it was a cell lost for good.
		if (IsPlayerBotPickupGoods(item))
		{
			if (IsPlayerBotPickupGear(item) && !IsPlayerBotLppKeptItem(ch, item) &&
					!IsPlayerBotUpgradeForSelf(ch, item) &&
					CountPlayerBotVnumUnitsAhead(ch, item) >= PLAYERBOT_PICKUP_GEAR_BAG_KEEP)
				return true;
			return IsPlayerBotBagUnderPressure(ch) && !PlayerBotHasCounter(ch);
		}
		// Kamien Duchowy is its owner's training (ManagePlayerBotGrandMasterTraining),
		// never the merchant's: he paid 194 yang for one.
		if (vnum == PLAYERBOT_GRAND_MASTER_STONE_VNUM)
			return false;
		// A pet seal from the ItemShop (MT2009 Plus, playerbot_itemshop.h) is the
		// bot's own pet, summoned from the bag: never the merchant's.
		if (item->GetType() == ITEM_PET)
			return false;
		// MT2009_PLUS_BOT_HAIR_V1: a hairstyle the bot will neither wear nor
		// list is thrown away at the merchant (SellPlayerBotJunkAtMerchant).
		if (IsPlayerBotUnwantedHair(ch, item))
			return true;
		// MT2009_PLUS_BOT_LIST_ALL_GEAR_V1: a spare piece the anvil takes to
		// +4 for the counter, and the first PLAYERBOT_ALL_GEAR_BAG_KEEP of the
		// gear this rule lists, wait for the counter while there is one and
		// the bag has room - the rest is the rules' below, as before.
		if (IsPlayerBotSaleGear(item) && PlayerBotHasCounter(ch) && !IsPlayerBotBagUnderPressure(ch) &&
				(PlayerBotRefinesSpareForSale(ch, item) ||
				 (IsPlayerBotAllGearLowLine(item) && CountPlayerBotAllGearAhead(ch, item) < PLAYERBOT_ALL_GEAR_BAG_KEEP &&
				  !((IsPlayerBotCappedLowArmour(item) || IsPlayerBotCappedLowJewel(item)) &&
					IsPlayerBotLowArmourMarketFull(item->GetVnum())))))
			return false;
		// A hairstyle from the ItemShop (playerbot_itemshop.h) is worn, not sold:
		// the rule's default would vendor it on the next town trip.
		if (item->GetType() == ITEM_COSTUME)
			return false;
		// The look's bonus reagents from Handlarka (70063/70064,
		// ManagePlayerBotCostumeBonus) are bought for the bot's own costume.
		if (item->GetType() == ITEM_USE && (item->GetSubType() == USE_CHANGE_COSTUME_ATTR ||
				item->GetSubType() == USE_RESET_COSTUME_ATTR))
			return false;

		// Baek-Go's board (playerbot_herbalism.h) gave two kinds of item a
		// worth this rule had no branch for, so its default sold both. A recipe
		// is knowledge - and the only source of it in this world is the Metin
		// stones the bots break all day, so the whole system was being fed to
		// the merchant a few hundred yang at a time - and a potion is what the
		// herbs finally turn into. Neither is ever merchant scrap; the surplus
		// of both goes on a counter, where a player can reach it at last.
		if (IsPlayerBotCraftRecipeItem(item) || IsPlayerBotCraftedPotion(item))
			return false;

		// A specimen of a Biologist row already handed in is scrap, not goods:
		// "niech ich nie wystawiaja, sprzedaja u handlarza albo wyrzucaja".
		// Before the anti-sell test on purpose - the quest items carry it, and
		// the merchant leg pays the merchant's pennies for them anyway. The Orc
		// Tooth is a refine material and takes the material branch below.
		if (vnum >= 50701 && vnum <= 50706 && !IsPlayerBotTradeableMaterial(item) &&
				IsPlayerBotBiologistSpecimenSurplus(ch, vnum))
			return true;

		// A material only the Herbalist's Knife consumes is nothing to a bot:
		// see IsPlayerBotNonGearMaterial. MT2009_PLUS_BOT_HERBALIST_FIX_V1:
		// except the herbs to a gatherer, who picked them for Baek-Go's board.
		if (IsPlayerBotHerbalismHerb(vnum) && IsPlayerBotHerbGatherer(ch))   // MT2009_PLUS_BOT_HERBALIST_ACTIVITY_V1
			return false;
		if (item->GetType() == ITEM_MATERIAL && IsPlayerBotNonGearMaterial(vnum))
			return true;

		if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_SELL))
			return false;
		// What the bot uses is never scrap, whatever the vnum: a timed buff or an
		// ability potion is drunk in the next fight, a bonus stone or marble goes
		// on its gear, an auto potion with anything left in it keeps its recovery
		// running (an empty one is scrap), and the Metin detector is a player
		// thing for the counter. Only the ItemShop copies
		// (76xxx) lack ANTI_SELL, which is how a bot handed them from the panel
		// vendored the lot (Pasywny, 13 September).
		if (item->GetType() == ITEM_USE)
		{
			const BYTE sub = item->GetSubType();
			// A Combo or Leadership book is read or sold on a counter, never
			// vendored, the way a skill book is.
			if (IsPlayerBotGeneralSkillBook(vnum))
				return false;
			if (IsPlayerBotBoosterItem(item) || IsPlayerBotMetinDetector(vnum) ||
					(GetPlayerBotAutoPotionAffect(vnum) != 0 && !IsPlayerBotAutoPotionEmpty(item)) ||
					IsPlayerBotBookAffectItem(item) ||
					sub == USE_ADD_ATTRIBUTE || sub == USE_CHANGE_ATTRIBUTE || sub == USE_ADD_ATTRIBUTE2)
				return false;
		}

		// Level-30 weapons with average/skill damage are strategic market assets.
		// Never vendor them: this also applies when the current owner is below level
		// 30 or belongs to another class. They remain available for future playerbot
		// trading/private shops instead of disappearing for a trivial NPC price.
		if (IsPlayerBotSpecialLevel30Weapon(item))
			return false;

		// The Archer's one stone weapon (playerbot_gear.h) is kept.
		if (IsPlayerBotArcherBuild(ch) && IsPlayerBotStoneMeleeWeapon(ch, item) &&
				FindPlayerBotStoneWeapon(ch, false) == item)
			return false;
		// So is the weapon kept for the day the one in the hand burns
		// (FindPlayerBotBackupWeapon), and the armour kept for the day the one
		// on the back does (FindPlayerBotBackupArmour).
		if (IsPlayerBotKeptBackupWeapon(ch, item) || IsPlayerBotKeptBackupArmour(ch, item))
			return false;
		// And the weapon kept for its lines, on its way to the hand
		// (FindPlayerBotLinesProject).
		if (item->GetType() == ITEM_WEAPON && item == FindPlayerBotLinesProject(ch))
			return false;

		// Gear the counter could not sell in six stands is scrap, whatever the
		// rules below would keep it for - up to PLAYERBOT_SHOP_UNSOLD_SCRAP_MAX_REFINE.
		// This has to come before the precious-refine keep below, or it never
		// applies to the +4 and +5 the counter actually keeps, which is what it
		// was written for: with it underneath, a bag of unsold +5 was for life.
		// Only when the bag is under pressure, though: a keeper that simply has
		// extra and does not need the yang can stand as long as the loop needs,
		// re-listing the piece; the merchant is for a cornered bot, not a bored
		// one ("jesli maja extra a nie potrzebuja yang, moga stac", akhigubernator).
		if ((item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR) &&
				item->GetRefineLevel() <= PLAYERBOT_SHOP_UNSOLD_SCRAP_MAX_REFINE &&
				IsPlayerBotBagUnderPressure(ch))
		{
			TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
			if (st != s_mapPlayerBotAIStates.end())
			{
				std::map<DWORD, BYTE>::const_iterator unsold = st->second.mapStallUnsold.find(item->GetID());
				if (unsold != st->second.mapStallUnsold.end() &&
						unsold->second >= PLAYERBOT_SHOP_UNSOLD_SCRAP_STANDS)
					return true;
			}
		}

		// A full bag with no counter to sell from. On mt2009 a bot under level
		// 15 or 800 kills cannot open a stall, so "goods for the counter" was
		// a bag for life: full, every drop refused, the bot hunting on with
		// "Podnosze lup" over its head. Gear the merchant may have (up to +4,
		// the operator's line) goes to him now when the bag is under pressure
		// and no counter is possible - unless this bot would wear it.
		if ((item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR) &&
				item->GetRefineLevel() <= PLAYERBOT_MERCHANT_MAX_REFINE &&
				IsPlayerBotBagUnderPressure(ch) && !PlayerBotHasCounter(ch) &&
				!IsPlayerBotUpgradeForSelf(ch, item))
			return true;

		// Gear under level thirty below +6 is not counter goods any more (see
		// PLAYERBOT_SHOP_LOW_GEAR_MIN_REFINE), so once the bot has no use for it
		// the merchant takes it, +4 and +5 included. Kept, it would ride in the
		// bag for good: the counter it used to be kept for no longer takes it,
		// and the unsold-stands rule above only counts what went up.
		if (IsPlayerBotLowLevelGear(item) &&
				item->GetRefineLevel() >= PLAYERBOT_PRECIOUS_REFINE &&
				item->GetRefineLevel() < GetPlayerBotLowGearMinRefine(item) &&
				!IsPlayerBotUpgradeForSelf(ch, item) && !IsPlayerBotHigherTierSpare(ch, item) &&
				item->GetID() != GetPlayerBotBackupWeaponID(ch, false))
			return true;

		// A body armour or a jewel at +0..+3 waits for the counter while its
		// family has room there (IsPlayerBotLowPlusMarketGear) - for a bot that
		// keeps a counter, while its bag has room, and the first
		// PLAYERBOT_LOW_PLUS_BAG_KEEP of them in bag order. Anything else is the
		// rules below, as before: the one upgrade a slot stays, the rest is
		// the merchant's.
		if (IsPlayerBotLowPlusMarketGear(item) && PlayerBotHasCounter(ch) && !IsPlayerBotBagUnderPressure(ch) &&
				!IsPlayerBotLowArmourMarketFull(item->GetVnum()) &&
				CountPlayerBotLowPlusGearAhead(ch, item) < PLAYERBOT_LOW_PLUS_BAG_KEEP)
			return false;

		// A piece of Iwakura's list past what the list keeps goes on a counter
		// for the gamblers (community patch 2, point 9), and to the merchant
		// only from a bag under pressure with no counter to sell from. Gear
		// under level thirty keeps the operator's rule above.
		if (IsPlayerBotLppSurplusGoods(ch, item) && !IsPlayerBotLowLevelGear(item) &&
				!IsPlayerBotUpgradeForSelf(ch, item))
			return IsPlayerBotBagUnderPressure(ch) && !PlayerBotHasCounter(ch);

		// Tackle: one rod is the angler's and one pickaxe the miner's, and a
		// second of either is scrap - one that another of its kind, worn or in
		// the bag, matches or beats in grade (the grades are consecutive vnums).
		// The bots that bought a rod per session (see CountPlayerBotRods) were
		// carrying fifteen, and a vendored rod or pickaxe (eighty thousand yang)
		// would only be bought again for the next session. Above the refine keep
		// below, because a tool's grade is the plus in its name: from Wedka+4 a
		// second rod was never scrap, and the counter took any rod from +4 for a
		// precious spare - the bot's only one too, stowed in the bag between two
		// sessions, so the next session bought another at the Rybak (Octodan,
		// 26 September). A tool is never counter goods now
		// (ScorePlayerBotShopStock), so the merchant is where a second one goes,
		// whatever its plus, or it would ride in the bag for good.
		if (IsPlayerBotToolType(item->GetType()))   // MT2009_PLUS_BOT_HERBALIST_FIX_V1: the knife too
		{
			const BYTE tool = item->GetType();
			LPITEM worn = ch->GetWear(WEAR_WEAPON);
			if (worn && worn != item && worn->GetType() == tool && worn->GetVnum() >= vnum)
				return true;
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM other = ch->GetInventoryItem(cell);
				if (other && other != item && other->GetType() == tool &&
						(other->GetVnum() > vnum || (other->GetVnum() == vnum && other->GetID() < item->GetID())))
					return true;
			}
			return false;
		}

		// Whatever else it is, a +5 or better is not something to hand an NPC for
		// a fifth of the shop price. The reserve rule below keeps one spare per
		// slot and sold the rest; that is how a Riba +9 went to a merchant
		// because the same bot was carrying an axe +9. These go on a stall -
		// or to the blacksmith first and then on a stall - and never to him.
		if (item->GetRefineLevel() > PLAYERBOT_MERCHANT_MAX_REFINE)
			return false;
		// +4 exactly is the counter's own threshold: kept as goods while the
		// counter can sell it, scrap after six unsold stands (above).
		if (item->GetRefineLevel() >= PLAYERBOT_PRECIOUS_REFINE)
			return false;

		// A scrap keeper's low refines are its stock, not its junk - until the
		// bag runs short, and then the merchant gets them like anyone else's.

		// Not gear under level thirty, which no counter takes below +6 now.
		if ((item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR) &&
				!IsPlayerBotLowLevelGear(item) &&
				IsPlayerBotScrapKeeper(ch->GetPlayerID()) &&
				CountPlayerBotFreeInventoryCells(ch) > PLAYERBOT_SCRAP_KEEP_FREE_CELLS)
			return false;

		// Quest progress must survive every merchant visit. In particular, Horse
		// Medals used to look like ordinary miscellaneous loot and could be sold
		// before the world-travel state machine returned the bot to the Stable Boy.
		//
		// And so must what the progress was for. The battle horse scroll is the
		// end of that whole chain - twenty-one medals, a hundred kills in the
		// desert and 500 000 yang at the stable keeper - and it is a miscellaneous
		// item like any other to the junk rule, which says "junk" unless told
		// otherwise. The first bot ever to finish the trial sold it three minutes
		// later, for its share of 1020 gold, and left the world with no battle
		// horse in it at all.
		if (vnum == PLAYERBOT_HORSE_MEDAL_VNUM ||
				vnum == PLAYERBOT_BATTLE_HORSE_BOOK_VNUM ||
				IsPlayerBotBiologistSpecimen(vnum) || IsPlayerBotBiologistKeyItem(vnum))
			return false;
		// A chest is opened on the next pass, not sold; the bonus scrolls, boosters
		// and big potions it holds carry ANTI_SELL and never reach this rule.
		if (vnum == PLAYERBOT_MOONLIGHT_CHEST_VNUM)
			return false;
		// A treasure chest waits for its key, a key for its chest, and a
		// Forgetting Scroll for a skill stuck at seventeen - this bot's or, across
		// a counter, another's.
		// A chest without its key is kept while there is room for it. Keys are
		// rare and chests are not: a bag under pressure lets the merchant have
		// the chests, so the loot and the Moonlight chests that open by
		// themselves still have somewhere to land.
		// A polymorph marble is counter goods; the merchant takes it only
		// under bag pressure with no counter to sell from, like a material -
		// or past the PLAYERBOT_SHOP_MARBLE_LINES a counter shows, which no
		// counter will take and which would otherwise ride in the bag for good.
		if (item->GetType() == ITEM_POLYMORPH)
			return IsPlayerBotBagUnderPressure(ch) && (!PlayerBotHasCounter(ch) ||
					CountPlayerBotVnumUnitsAhead(ch, item) >= PLAYERBOT_SHOP_MARBLE_LINES);
		if (item->GetType() == ITEM_TREASURE_BOX)
			return CountPlayerBotFreeInventoryCells(ch) <= PLAYERBOT_BAG_PRESSURE_FREE_CELLS &&
					!PlayerBotHasTreasureKeyFor(ch, item);
		if (item->GetType() == ITEM_TREASURE_KEY ||
				item->GetType() == ITEM_GIFTBOX || vnum == PLAYERBOT_SKILL_FORGET_SCROLL_VNUM)
			return false;
		// A refine scroll stays in the bag or goes on a counter, never to the
		// merchant. It is still stall goods: another bot needs one too.
		if (IsPlayerBotRefineScroll(vnum))
			return false;
		// A soul stone is somebody's socket: this bot's, or across a counter
		// another's. The merchant paid one yang for a Potwora +4. One of the
		// grades Iwakura bans waits in the bag for the Alchemist.
		if (item->GetType() == ITEM_METIN)
			return false;
		// And what the Alchemist gave for it is counter goods: the merchant
		// pays fifty yang for a dust that cost five hundred and a stone.
		if (vnum == PLAYERBOT_MAGIC_DUST_VNUM)
			return false;

		// The catch worth keeping. Pearls are the entire point of a fishing trip
		// -- they are what carries equipment to +7/+8/+9. Ordinary fish and bones
		// stay sellable: that is the angler's pocket money. (The rod and the
		// pickaxe are tackle, judged above the refine keep.)
		if (vnum == PLAYERBOT_FISHING_BAIT_VNUM ||
				vnum == PLAYERBOT_SHELLFISH_VNUM || vnum == PLAYERBOT_CAMPFIRE_VNUM ||
				(vnum >= PLAYERBOT_PEARL_FIRST_VNUM && vnum <= PLAYERBOT_PEARL_LAST_VNUM))
			return false;
		// Ore, raw and smelted. A hundred raw make one smelted piece and the
		// smelted ones are what a player crosses a market for, so neither is
		// ever the merchant's - they are the whole point of the digging, and
		// the counter is where the operator asked the trade to happen.
		if (IsPlayerBotRawOre(vnum) || IsPlayerBotSmeltedOre(vnum))
			return false;
		// MT2009_PLUS_BOTLIFE_V1: nor any refine stone of the jewellery, the
		// ones no vein here gives included - the counter sells them.
		if (IsPlayerBotAccessoryStone(item))
			return false;
		// Hair dye is never the merchant's: the item shop's is goods, and one
		// from the water is thrown away (DiscardPlayerBotFishedDyes) but for
		// the colour a bot has yet to use and the few kept for a counter.
		if (IsPlayerBotHairDye(vnum))
			return false;
		// A dead fish waits for the campfire at the end of the next session, as
		// long as the bot has the wood for one; a grilled fish is a potion.
		if (item->GetType() == ITEM_FISH && item->GetSubType() == FISH_DEAD &&
				ch->CountSpecifyItem(PLAYERBOT_CAMPFIRE_VNUM) > 0)
		{
			// As many as a fire is worth: the first PLAYERBOT_DEAD_FISH_KEEP in
			// bag order, and the rest go to the merchant.
			int ahead = 0;
			for (WORD cell = 0; cell < item->GetCell() && cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM held = ch->GetInventoryItem(cell);
				if (held && held->GetCell() == cell && held->GetType() == ITEM_FISH &&
						held->GetSubType() == FISH_DEAD)
					ahead += std::max<int>(1, held->GetCount());
			}
			if (ahead < PLAYERBOT_DEAD_FISH_KEEP)
				return false;
		}
		if (vnum >= PLAYERBOT_GRILLED_FISH_FIRST_VNUM && vnum <= PLAYERBOT_GRILLED_FISH_LAST_VNUM)
			return false;

		// Arrows are ammunition, not a primary weapon/equipment candidate, and
		// other classes sell accidental arrow drops at the Weapon Merchant. An
		// Archer's quiver never empties, so the stack in the slot is all it
		// ever shoots: a bag stack is kept only while it is better than that
		// one - the next tier, waiting for its level, or the one the equipment
		// pass is about to nock (UpgradePlayerBotArrows) - and the rest is
		// scrap, or the bag of a dropper's archer would carry its old thousand
		// for good. A Ninja about to choose the Bow profession keeps what can
		// hit, and nothing keeps an arrow that cannot (GetPlayerBotArrowGrade).
		if (item->GetType() == ITEM_WEAPON && item->GetSubType() == WEAPON_ARROW)
		{
			// MT2009_PLUS_QUIVER_V1: a quiver is never scrap (nor sellable).
			if (IsPlayerBotQuiver(item))
				return false;
			const bool isOrWillBeArcher = ch->GetJob() == JOB_ASSASSIN &&
					(ch->GetSkillGroup() == 2 ||
					 (ch->GetSkillGroup() == 0 && (ch->GetPlayerID() % 2) != 0));
			const int grade = GetPlayerBotArrowGrade(item);
			if (!isOrWillBeArcher || grade < 0)
				return true;
			LPITEM worn = ch->GetWear(WEAR_ARROW);
			if (!IsPlayerBotUsableArrow(ch, worn))
				return false;
			return grade <= GetPlayerBotArrowGrade(worn);
		}

		// A skill book never goes to the merchant. Its own working stock stays
		// in the bag (GetPlayerBotBookKeepLimit), the surplus - somebody else's
		// skill, or more of its own than it can read - is goods for the counter,
		// and what the bag cannot hold beyond PLAYERBOT_SAFEBOX_BOOK_KEEP of
		// those goes to the storekeeper's safebox on the next town visit
		// (IsPlayerBotSafeboxBook). The merchant paid pennies for Aura Miecza
		// while the warrior three stalls away would have paid a fortune, and
		// "under bag pressure" turned out to be most of a dropper's life.
		if (item->GetType() == ITEM_SKILLBOOK)
			return false;

		// Preserve health, mana, green and purple speed potions
		if (vnum == 27051 || vnum == 27001 || vnum == 27002 || vnum == 27003 ||
			vnum == 27052 || vnum == 27004 || vnum == 27005 || vnum == 27006 ||
			(vnum >= 27100 && vnum <= 27105) || vnum == 27053 || vnum == 27054)
			return false;

		// Preserve every Apprentice Chest until the bot can open it. Class-specific
		// first chests use 50212/50213, while later progression boxes use 50187-50196.
		if (vnum == GetStarterChestVnum(ch->GetJob()) ||
				(vnum >= 50187 && vnum <= 50196))
			return false;

		// What this bot is about to refine with stays in its bag. A spare that
		// some recipe wants stays too, as stock for its own counter - see
		// IsPlayerBotSurplusMaterial for why that is worth eight cells. Judged by
		// the recipe table, not by item type: that is what brings the fishbone
		// and the blessing scroll in.
		// A material somebody on this world is short of is counter goods,
		// not merchant scrap: a Scorpion Tail went to the merchant for a
		// few hundred yang while the next stall along sold one for 58 894.
		// Only a material nobody wants, and only under bag pressure - or a
		// material this bot has no counter to sell from, whoever wants it.
		// And only under bag pressure at all: "Stalowy Grot, Futro Yeti, Kawalek
		// Lodu ... sprzedane handlarzowi" non stop from bags with room to spare.
		// A refine material is never merchant scrap. What it does not sell on a
		// counter and the bag has no room for goes to the storekeeper
		// (CollectPlayerBotSafeboxMaterials), not to the merchant for pennies:
		// "jak nie ma miejsca to materialy niech traf ia do magazynu u Dozorcy"
		// (13 September).
		if (IsPlayerBotTradeableMaterial(item))
			return false;
		// The rest of the 30000 block is eight gift boxes and two quest items.
		// No counter would carry those, so there junk still means junk.
		if (vnum >= 30000 && vnum <= 30200)
			return !PlayerBotNeedsRefineMaterial(ch, vnum) &&
					GetPlayerBotLedgerDemand(vnum) == 0 &&
					CountPlayerBotFreeInventoryCells(ch) <= PLAYERBOT_BAG_PRESSURE_FREE_CELLS;
		if (vnum >= 70038 && vnum <= 70060)
			return false;

		// What Iwakura's sheet prices by name and no rule above placed - the
		// horse books (50060-50062), the polymorph books (50314-50316), the
		// stone detachment scroll (25100) - is goods, the way a polymorph
		// marble is: the counter's (ScorePlayerBotShopStock), and the
		// merchant's only from a bag under pressure with no counter to sell
		// from. The default below sold them all - on the test world some
		// thousand of each in a day, for a few hundred yang against 40 000 to
		// 135 000 on the sheet (18 September).
		// A saddlebag bot's materials for its rows are nobody's scrap.
		if (IsPlayerBotKeptCraftMaterial(ch, item))
			return false;
		// A polymorph book or a Mining Guide it can read is read, not sold.
		if (IsPlayerBotExtraSkillBook(vnum) && CanPlayerBotReadExtraSkillBookNow(ch, vnum))
			return false;
		if (IsPlayerBotSheetGoods(item))
			return IsPlayerBotBagUnderPressure(ch) && !PlayerBotHasCounter(ch);

		// Keep at most one immediately usable upgrade for each wear slot.  The old
		// test kept every item that scored above the currently worn one; at high
		// drop rates that meant dozens of near-identical weapons and armours could
		// never become junk even though only the best one would ever be equipped.
		if (IsPlayerBotEquipmentCandidate(ch, item))
		{
			const int wearCell = item->FindEquipCell(ch);
			if (wearCell >= 0 && wearCell < WEAR_MAX_NUM)
			{
				if (item->GetLevelLimit() > ch->GetLevel())
					return true;

				LPITEM oldItem = ch->GetWear(wearCell);
				const long long itemScore = GetPlayerBotEquipmentScore(item, ch);
				const long long oldScore = oldItem ? GetPlayerBotEquipmentScore(oldItem, ch) : 0;
				if (!oldItem || itemScore > oldScore)
				{
					for (WORD otherCell = 0; otherCell < PLAYERBOT_BAG_CELLS; ++otherCell)
					{
						LPITEM other = ch->GetInventoryItem(otherCell);
						if (!other || other == item || !IsPlayerBotEquipmentCandidate(ch, other) ||
								other->GetLevelLimit() > ch->GetLevel() ||
								other->FindEquipCell(ch) != wearCell)
							continue;

						const long long otherScore = GetPlayerBotEquipmentScore(other, ch);
						if (otherScore > itemScore ||
								(otherScore == itemScore && other->GetID() < item->GetID()))
							return true;
					}
					return false;
				}

				// A well-refined item replaced by genuinely stronger progression gear is
				// still valuable to another bot.  Keep only the single best +6-or-higher
				// reserve for this wear slot; the nearby sharing pass will hand the real
				// item (including sockets/attributes) to a lower-level compatible build.
				if (IsPlayerBotHigherTierSpare(ch, item))
					return false;

				if (item->GetRefineLevel() >= PLAYERBOT_RESERVE_GEAR_MIN_REFINE)
				{
					for (WORD otherCell = 0; otherCell < PLAYERBOT_BAG_CELLS; ++otherCell)
					{
						LPITEM other = ch->GetInventoryItem(otherCell);
						if (!other || other == item ||
								other->GetRefineLevel() < PLAYERBOT_RESERVE_GEAR_MIN_REFINE ||
								!IsPlayerBotEquipmentCandidate(ch, other) ||
								other->GetLevelLimit() > ch->GetLevel() ||
								other->FindEquipCell(ch) != wearCell)
							continue;

						const long long otherScore = GetPlayerBotEquipmentScore(other, ch);
						if (otherScore > itemScore ||
								(otherScore == itemScore && other->GetID() < item->GetID()))
							return true;
					}
					return false;
				}
			}
		}

		return true;
	}

	EPlayerBotMerchantCategory GetPlayerBotJunkMerchant(LPITEM item)
	{
		if (!item)
			return BOT_MERCHANT_MISC;

		if (item->GetType() == ITEM_WEAPON)
			return BOT_MERCHANT_WEAPON;
		if (item->GetType() == ITEM_ARMOR || item->GetType() == ITEM_UNIQUE ||
				item->GetType() == ITEM_RING || item->GetType() == ITEM_BELT)
			return BOT_MERCHANT_ARMOR;
		return BOT_MERCHANT_MISC;
	}

	bool HasPlayerBotJunkForMerchant(LPCHARACTER ch, EPlayerBotMerchantCategory category)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && IsPlayerBotJunkItem(ch, item) &&
					GetPlayerBotJunkMerchant(item) == category)
				return true;
		}
		return false;
	}

	size_t CountPlayerBotJunkItems(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return 0;
		size_t count = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			if (IsPlayerBotJunkItem(ch, ch->GetInventoryItem(cell)))
				++count;
		return count;
	}

	// A dozen pieces of scrap send a bot to the merchant under the old rules.
	// Iwakura's Trader goes at eighty percent of the bag (IsPlayerBotBagFull),
	// so under his system the scrap is sold on whatever visit comes and starts
	// none. The town visit and the first village's hold on a departure ask
	// this one function, because that hold is lifted by a visit: a need that
	// holds the bot and starts no visit is lifted by nothing but a relog.
	bool PlayerBotWantsSellRun(LPCHARACTER ch)
	{
		return !IsPlayerBotPersonaEnabled() &&
				CountPlayerBotJunkItems(ch) >= PLAYERBOT_SELL_RUN_JUNK_ITEMS;
	}

	// Boosters nobody but their holder can use, past PLAYERBOT_BOOSTER_KEEP_PER_VNUM,
	// thrown away at the merchant visit. Only the ones that may neither be sold
	// nor put on a counter: an ItemShop copy without ANTI_SELL is the operator's
	// gift and stays. The first stacks are kept and whole stacks go after them,
	// so a bot is left with at most one stack over the keep.
	void DiscardPlayerBotSurplusBoosters(LPCHARACTER ch)
	{
		std::map<DWORD, int> kept;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->IsEquipped() || item->isLocked() ||
					!IsPlayerBotBoosterItem(item) ||
					!IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_SELL) ||
					!IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_MYSHOP) ||
					IsPlayerBotSidekickLockedItem(ch, item))	// MT2009_PLUS_SIDEKICK_EQUIP_LOCK_V1
				continue;
			int& held = kept[item->GetVnum()];
			if (held < PLAYERBOT_BOOSTER_KEEP_PER_VNUM)
			{
				held += (int)item->GetCount();
				continue;
			}
			sys_log(0, "PLAYERBOT_CHEST: discarded surplus booster pid=%u name=%s vnum=%u count=%u kept=%d",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)item->GetCount(), held);
			ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_DISCARD_BOOSTER");
		}
	}

	// MT2009_PLUS_BOT_HORSE_HAY_V1: the General Store buys the Siano over the
	// horse's PLAYERBOT_HAY_KEEP - by count, so a stack of two hundred leaves
	// five behind instead of going whole. The yang it paid.
	long long SellPlayerBotSurplusHay(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		const int surplus = (int)ch->CountSpecifyItem(PLAYERBOT_HAY_VNUM) - PLAYERBOT_HAY_KEEP;
		if (surplus <= 0)
			return 0;
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(PLAYERBOT_HAY_VNUM);
		long long unit = proto ? (long long)proto->dwShopBuyPrice : 0;
		if (unit <= 0)
			unit = proto ? (long long)proto->dwGold : 100;
		const long long price = std::max<long long>(10, unit * surplus / 5);
		ch->RemoveSpecifyItem(PLAYERBOT_HAY_VNUM, surplus);
		PlayerBotChangeGold(ch, price);
		sys_log(0, "PLAYERBOT_MARKET: hay sold pid=%u name=%s hay=%d gold=%lld hay_left=%d",
				ch->GetPlayerID(), ch->GetName(), surplus, price,
				(int)ch->CountSpecifyItem(PLAYERBOT_HAY_VNUM));
		return price;
	}

	// What the merchant pays for a piece of junk (the whole line). Also what a
	// bot is paid for the junk it lets go to make room for a Battle Pass
	// reward (playerbot_bpbots.h, MT2009_PLUS_BOT_BP_ROOM_V1).
	long long GetPlayerBotJunkSalePrice(LPITEM item)
	{
		DWORD price = item->GetShopBuyPrice();
		if (price == 0)
			price = item->GetProto() ? item->GetProto()->dwGold : 100;
		price = std::max<DWORD>(10, price / 5);
		// MT2009_PLUS_DIGI_STACK_V1: a soul stone, a gift box or a treasure
		// box stacks to 200 now - the stack goes whole, so it is paid by
		// count (it was one unit's price for the stack).
		long long salePrice = (long long)price;
		if (item->GetCount() > 1 && (item->GetType() == ITEM_METIN ||
				item->GetType() == ITEM_GIFTBOX || item->GetType() == ITEM_TREASURE_BOX))
			salePrice *= (long long)item->GetCount();
		return salePrice;
	}

	bool SellPlayerBotJunkAtMerchant(LPCHARACTER ch, EPlayerBotMerchantCategory category,
			const char* merchantName)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		DiscardPlayerBotSurplusBoosters(ch);
		// Sold rather than thrown away under Iwakura's system (the Rybak's way).
		DiscardPlayerBotFishedDyes(ch, IsPlayerBotPersonaEnabled());

		size_t soldCount = 0;
		long long totalSoldGold = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			// The operator said "drop": thrown away here, at the merchant,
			// without a sale - the one place a bag is emptied on purpose.
			if (item && !item->IsEquipped() && !item->isLocked() &&
					GetPlayerBotItemPolicy(item) == PLAYERBOT_ITEM_POLICY_DROP &&
					!IsPlayerBotSidekickLockedItem(ch, item))	// MT2009_PLUS_SIDEKICK_EQUIP_LOCK_V1
			{
				sys_log(0, "PLAYERBOT_AI: discarded by policy pid=%u name=%s vnum=%u count=%u",
						ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)item->GetCount());
				ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_DISCARD");
				continue;
			}
			// MT2009_PLUS_BOT_HAIR_V1: a hairstyle the bot will neither wear nor
			// list goes for nothing, at whichever merchant: the ItemShop's
			// heads are worth no yang the world should mint.
			if (item && !item->isLocked() && IsPlayerBotUnwantedHair(ch, item) &&
					!IsPlayerBotSidekickGift(ch, item) && !IsPlayerBotSidekickPinned(ch, item) &&
					GetPlayerBotItemPolicy(item) == PLAYERBOT_ITEM_POLICY_NONE)
			{
				sys_log(0, "PLAYERBOT_ISHOP: hairstyle thrown away pid=%u name=%s vnum=%u",
						ch->GetPlayerID(), ch->GetName(), item->GetVnum());
				ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_HAIR_DISCARD");
				continue;
			}
			if (!item || !IsPlayerBotJunkItem(ch, item) ||
					GetPlayerBotJunkMerchant(item) != category)
				continue;
			// MT2009_PLUS_BOT_HORSE_HAY_V1: hay by count, the horse's share kept.
			if (item->GetVnum() == PLAYERBOT_HAY_VNUM)
			{
				const long long hayGold = SellPlayerBotSurplusHay(ch);
				if (hayGold > 0)
				{
					totalSoldGold += hayGold;
					++soldCount;
				}
				continue;
			}

			const long long salePrice = GetPlayerBotJunkSalePrice(item);
			totalSoldGold += salePrice;
			PlayerBotChangeGold(ch, salePrice);
			ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_SHOP_SELL");
			++soldCount;
		}

		if (soldCount > 0)
		{
			sys_log(0, "PLAYERBOT_AI: sold %u items at %s pid=%u name=%s gold_gained=%lld total_gold=%lld",
					(unsigned int)soldCount, merchantName ? merchantName : "merchant",
					ch->GetPlayerID(), ch->GetName(), totalSoldGold, (long long)ch->GetGold());
		}
		return soldCount > 0;
	}

	// A piece in the bag for the slot besides `except` - which is the worn
	// piece itself once a blacksmith session has taken it off.
	bool HasPlayerBotBackupGear(LPCHARACTER ch, BYTE wearCell, LPITEM except = NULL)
	{
		if (!ch)
			return false;

		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item == except || !IsPlayerBotEquipmentCandidate(ch, item))
				continue;
			if (item->FindEquipCell(ch) == wearCell)
				return true;
		}

		return false;
	}

	// The highest level a village merchant sells this character a weapon at:
	// what a burned weapon is replaced with for yang, on the spot.
	int GetPlayerBotMerchantWeaponCeiling(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		static const DWORD merchants[] = { 9001, 9002, 9003 };
		int ceiling = 0;
		for (size_t i = 0; i < sizeof(merchants) / sizeof(merchants[0]); ++i)
		{
			LPSHOP shop = CShopManager::instance().GetByNPCVnum(merchants[i]);
			if (!shop)
				continue;
			const std::vector<CShop::SHOP_ITEM>& offers = shop->GetItemVector();
			for (size_t k = 0; k < offers.size(); ++k)
			{
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(offers[k].vnum);
				if (!proto || proto->bType != ITEM_WEAPON || !IsPlayerBotProtoForCharacter(ch, proto) ||
						!IsPlayerBotWeaponSubTypeFor(ch, proto->bSubType))
					continue;
				const int level = GetPlayerBotProtoLevelLimit(proto);
				if (level <= (int)ch->GetLevel())
					ceiling = std::max(ceiling, level);
			}
		}
		return ceiling;
	}

	// What the weapon merchant sells as the backup for this weapon in the hand
	// (R8 of Iwakura's audit): a copy of its family when a village merchant
	// stocks the family's +0, else the best weapon the merchants sell the bot
	// - what a burn would be replaced with anyway - either of which
	// playerbot_refine_rules::IsBackupWeaponFor takes. Never one a bot of
	// thirty may not wear (Community Patch 5, point 2).
	bool FindPlayerBotBackupWeaponOffer(LPCHARACTER ch, LPITEM hand, DWORD& vnumOut, long long& priceOut)
	{
		vnumOut = 0;
		priceOut = 0;
		if (!ch || !hand || hand->GetType() != ITEM_WEAPON)
			return false;
		const DWORD family = GetPlayerBotItemFamily(hand);
		std::vector<TPlayerBotMerchantWeapon> offers;
		CollectPlayerBotProperMerchantWeapons(ch, offers);
		const TPlayerBotMerchantWeapon* best = NULL;
		for (size_t i = 0; i < offers.size(); ++i)
		{
			if (offers[i].dwVnum == family)
			{
				best = &offers[i];
				break;
			}
			if (!best || offers[i].iLevel > best->iLevel ||
					(offers[i].iLevel == best->iLevel && offers[i].llRoll > best->llRoll))
				best = &offers[i];
		}
		if (!best)
			return false;
		vnumOut = best->dwVnum;
		priceOut = best->llPrice;
		return true;
	}

	// The weapon in the hand (GetPlayerBotHandWeapon) at a step the plain anvil
	// can burn it, with nothing to fall back on: no backup in the bag
	// (FindPlayerBotBackupWeapon) and nothing a merchant sells at its level.
	// Such a step goes under a scroll or waits for one. A level-30 weapon is
	// left out - grinding one at the anvil and buying the next is the
	// operator's rule - and so is a scroll-only weapon, held on its own.
	bool IsPlayerBotWornWeaponAtRisk(LPCHARACTER ch, LPITEM item, bool fresh = false)
	{
		if (!ch || !item || item->GetType() != ITEM_WEAPON || item->GetRefinedVnum() == 0 ||
				IsPlayerBotSpecialLevel30Weapon(item) || IsPlayerBotScrollOnlyWeapon(item))
			return false;
		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
		if (!recipe || recipe->prob > PLAYERBOT_WORN_SCROLL_MAX_PROB)
			return false;
		if (item != GetPlayerBotHandWeapon(ch))
			return false;
		// A weapon a merchant sells at the bot's level goes to the anvil as far
		// as the bot's own drawn aim: a burned one is bought again for yang. Past
		// that aim - Iwakura's Patch 4 takes every hand weapon to +9, and from
		// +0 a plain anvil gets there two times in a hundred - a burn is a sword
		// of +8 traded for a Miecz of +0, raised and burned again: 68 emergency
		// purchases by 35 bots in the half hour after the deploy against one in
		// the half hour before, each bot standing without a weapon in the square
		// between them. Past the drawn aim - and past
		// PLAYERBOT_MERCHANT_WEAPON_RISK_PLUS, where the ninety-percent steps
		// end - it waits for a backup or a scroll like any other weapon.
		if (item->GetLevelLimit() <= GetPlayerBotMerchantWeaponCeiling(ch) &&
				(int)item->GetRefineLevel() < std::min<int>(GetPlayerBotRefineAmbitionDrawn(ch, item),
					PLAYERBOT_MERCHANT_WEAPON_RISK_PLUS))
			return false;
		return GetPlayerBotBackupWeaponID(ch, fresh) == 0;
	}

	// The armour on the bot's back, at a step that can burn it, with no
	// other body armour in the bag it could put on: the weapon's rule for
	// the other slot a bot cannot do without. Every failed refine destroys
	// the piece on this engine, so a bot of twenty raised its only plate at
	// the anvil, lost it, and went on farming bare with the upgrade
	// materials it had kept for it (THC, 16 September). Under a scroll or
	// not at all; the merchant's re-stock is a town visit away, and that is
	// exactly the bare walk the report was about. The spare it asks for is
	// the one the junk rule keeps (FindPlayerBotBackupArmour) and the armour
	// merchant sells when there is none (NeedsPlayerBotBackupArmour).
	bool IsPlayerBotWornArmourAtRisk(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetType() != ITEM_ARMOR || item->GetSubType() != ARMOR_BODY ||
				item->GetRefinedVnum() == 0)
			return false;
		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
		if (!recipe || recipe->prob > PLAYERBOT_WORN_SCROLL_MAX_PROB)
			return false;
		// The armour on the back, or the one going back on: the anvil keeps it
		// in the bag for the whole session (GetPlayerBotBodyArmour).
		if (GetPlayerBotBodyArmour(ch) != item)
			return false;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM spare = ch->GetInventoryItem(cell);
			if (spare && spare->GetCell() == cell && spare != item &&
					IsPlayerBotBackupArmourCandidate(ch, spare))
				return false;
		}
		return true;
	}

	// Iwakura's "tylko w 50% uzywaja bodzi" (24 September): this share of the
	// steps that would go under a scroll, or wait for one, goes to the plain
	// anvil - a burn is part of the game, and a world whose scrolls come from
	// the stones cannot put one on every step. The coin is the piece's own
	// (its id, which every refine renews, and its plus) and a bucket of
	// PLAYERBOT_SCROLL_SKIP_BUCKET_SECONDS, so the planner and the pass that
	// acts read the same coin, and a piece it keeps waiting for a scroll gets
	// another toss later. Never for what a rule of its own protects: a weapon
	// on the scroll-only line (the operator's), and the weapon in the hand or
	// the armour on the back with nothing to fall back on (the backup rule -
	// "nigdy nie ryzykuje ... jesli nie posiada w ekwipunku broni
	// zastepczej", Iwakura's own). Nor for a weapon of the operator's anvil
	// table (IsPlayerBotAnvilTableWeapon), which has its own answer: the plain
	// anvil to its ceiling and the scrolls from there, every step of it. Nor
	// for the armour under his scroll rule (IsPlayerBotScrollRuleArmour):
	// "powinien wykorzystac je do ulepszenia noszonej zbroi" leaves no coin.
	bool PlayerBotRisksPlainAnvil(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || PLAYERBOT_SCROLL_SKIP_PERCENT <= 0 || IsPlayerBotAnvilTableWeapon(item) ||
				IsPlayerBotScrollRuleArmour(ch, item))
			return false;
		const DWORD bucket = (DWORD)(get_global_time() / PLAYERBOT_SCROLL_SKIP_BUCKET_SECONDS);
		const DWORD seed = (item->GetID() * 2654435761U) ^ ((DWORD)item->GetRefineLevel() * 0x9e3779b9U) ^
				(bucket * 0x85ebca6bU) ^ 0x534b4950U;
		if ((int)(PlayerBotNavHash(seed) % 100U) >= PLAYERBOT_SCROLL_SKIP_PERCENT)
			return false;
		return !IsPlayerBotScrollOnlyWeapon(item) && !IsPlayerBotWornWeaponAtRisk(ch, item) &&
				!IsPlayerBotWornArmourAtRisk(ch, item);
	}

	// The scroll a refine goes under: from PLAYERBOT_DRAGON_GOD_SCROLL_MIN_PLUS
	// the Zwoj Boga Smokow when the bag has one, otherwise the Blessing
	// Scroll. Both are read by DoRefineWithScroll from the cell SetRefineMode
	// names, no blacksmith needed - which is also why the scroll pass runs
	// wherever the bot stands. plusLevel 0 means "any scroll that is here".
	int FindPlayerBotRefineScrollCell(LPCHARACTER ch, BYTE plusLevel, int stepProb = 100)
	{
		if (!ch)
			return -1;
#if defined(PLAYERBOT_ENGINE_MT2009)
		// mt2009 knows a scroll by what DoRefineWithScroll reads off it, not by
		// vnum: value0 the kind, value1 the percent added to the step. The War
		// God scroll (UP_TO_3TH_LEVEL) makes a step under +4 certain; a plain
		// scroll hands the piece back a level down on failure, the more value1
		// the better; the Magic Stone (NO_REDUCTION_WHEN_FAIL) keeps the level
		// and is saved for steps at PLAYERBOT_NO_REDUCTION_SCROLL_MAX_PROB and
		// under; the Gwarancja (REFINE_BONUS) burns what it fails and is never
		// taken. By vnum only 25040 and 71032 were ever used, and every other
		// kind a bot found was goods to it. plusLevel 0 is "any scroll here".
		int bestCell = -1, bestRank = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM scroll = ch->GetInventoryItem(cell);
			if (!scroll || scroll->GetCell() != cell || scroll->GetType() != ITEM_USE ||
					scroll->GetSubType() != USE_TUNING)
				continue;
			int rank = 0;
			switch (scroll->GetValue(0))
			{
				case UP_TO_3TH_LEVEL_SCROLL:
					rank = plusLevel < 4 ? 1000 : 0;
					break;
				case NO_REDUCTION_WHEN_FAIL_SCROLL:
					rank = stepProb <= PLAYERBOT_NO_REDUCTION_SCROLL_MAX_PROB ? 900 : 100;
					break;
				case NORMAL_REFINE_SCROLL:
					rank = 200 + std::max<int>(0, (int)scroll->GetValue(1));
					break;
				default:
					break;
			}
			if (rank > bestRank)
			{
				bestRank = rank;
				bestCell = cell;
			}
		}
		return bestCell;
#else
		(void)stepProb;
		int blessing = -1, dragonGod = -1;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM scroll = ch->GetInventoryItem(cell);
			if (!scroll)
				continue;
			const DWORD vnum = scroll->GetVnum();
			if (vnum == PLAYERBOT_BLESSING_SCROLL_VNUM && blessing < 0)
				blessing = cell;
			for (size_t i = 0; i < sizeof(PLAYERBOT_DRAGON_GOD_SCROLL_VNUMS) / sizeof(PLAYERBOT_DRAGON_GOD_SCROLL_VNUMS[0]); ++i)
				if (vnum == PLAYERBOT_DRAGON_GOD_SCROLL_VNUMS[i] && dragonGod < 0)
					dragonGod = cell;
		}
		if (dragonGod >= 0 && (plusLevel >= PLAYERBOT_DRAGON_GOD_SCROLL_MIN_PLUS || blessing < 0))
			return dragonGod;
		return blessing;
#endif
	}

	// The scroll for this piece's next step, and none for a piece no scroll
	// goes on (IsPlayerBotScrollFreeGear). Every pass that puts a scroll on
	// the bot's own gear looks through here, and so does the planner, so a
	// piece is never held for a scroll that would not be used on it.
	int FindPlayerBotRefineScrollCellFor(LPCHARACTER ch, LPITEM item, int stepProb = 100)
	{
		if (!ch || !item || IsPlayerBotScrollFreeGear(item))
			return -1;
		return FindPlayerBotRefineScrollCell(ch, item->GetRefineLevel(), stepProb);
	}

	// Whether a piece lying in the bag is one this bot would actually raise.
	//
	// It exists because the planner and the pass that does the refining used to
	// answer that question differently: the planner accepted anything the
	// general equipment selector liked, the executor then also rejected
	// whatever the junk rule had marked for the merchant. A bot could therefore
	// see an opportunity, commit to a blacksmith visit - and BOT_TOWN_PHASE
	// treats a started visit as a commitment that outranks the ordinary goal
	// choice - walk there, find nothing to do, and come back. Reported as "mam
	// wszystko +9 zalozone, a bot dalej lezie do kowala".
	//
	// Worn pieces do not go through here: the junk rule does not apply to
	// something the bot is wearing, and a full +9 set is not a reason to refuse
	// a legitimate upgrade waiting in the bag.
	//
	// And "not scrap" is not "worth the bot's yang". The junk rule keeps a
	// great deal on purpose - a collector's stock, anything at +4 for the
	// counter, a piece with prize lines - and the refine pass took all of it
	// to the anvil: a bot under a Guillotine Blade +4 with 14 000 yang left
	// raised level-one swords, glaives, wooden earrings and copper necklaces
	// from +1 to +3 one by one, and burned one (elgrandebgc, 12 September,
	// "ulepszaja wszystko co popadnie, zeby potem wystawic za bezcen"). Only
	// what the bot will wear is worth refining in the bag: an upgrade the
	// equipment pass is about to put on, or the one higher-tier spare per slot
	// the blacksmith can make into one. Goods are sold at what they are.
	//
	// Whether the engine would take this piece's next step at all: the fee in
	// the purse and every material in the bag, the two things DoRefine and
	// DoRefineWithScroll refuse on before they take anything. A bag piece is
	// asked this and not CanPlayerBotPayRefineStep, whose reserve and
	// Biologist's share the bag's pieces have never been held to.
	bool CanPlayerBotAffordRefineAttempt(LPCHARACTER ch, LPITEM item)
	{
		const TRefineTable* recipe = item ? CRefineManager::instance().GetRefineRecipe(item->GetRefineSet()) : NULL;
		if (!ch || !recipe || ch->GetGold() < ch->ComputeRefineFee(recipe->cost))
			return false;
		for (int i = 0; i < recipe->material_count; ++i)
		{
			if (ch->CountSpecifyItem(recipe->materials[i].vnum) < recipe->materials[i].count)
				return false;
		}
		return true;
	}

	bool IsPlayerBotRefineBagCandidate(LPCHARACTER ch, LPITEM item)
	{
		if (!item || item->GetRefinedVnum() == 0 || IsPlayerBotSidekickPinned(ch, item))
			return false;
		// The bow or fan a keeper builds for its sash (playerbot_sash.h).
		if (IsPlayerBotSashGrailProject(ch, item))
			return item->GetRefineLevel() < GetPlayerBotRefineTarget(ch, item);
		// The piece a blacksmith session took off is still the worn one (B12):
		// the junk rule and the spare rules are not asked of it, as they are not
		// of a piece on the bot. With a higher-tier spare beside it in the bag it
		// was junk to the one-upgrade-a-slot rule, and its session stopped.
		if (ch)
		{
			TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
			if (st != s_mapPlayerBotAIStates.end() && GetPlayerBotTakenOffSlotOf(ch, st->second, item) >= 0)
				return item->GetRefineLevel() < GetPlayerBotRefineTarget(ch, item);
		}
		// A level-30 weapon of a class this bot cannot wear, ground for sale
		// (PlayerBotRefinesLevel30ForSale): no equipment candidate of its own,
		// and never junk.
		if (PlayerBotRefinesLevel30ForSale(ch, item))
			return item->GetRefineLevel() < GetPlayerBotRefineTarget(ch, item);
		// A body armour taken to +5 before it may go on a counter (Iwakura's
		// Patch 3, point 4).
		if (PlayerBotRefinesLowArmourForSale(ch, item))
			return item->GetRefineLevel() < PLAYERBOT_LOW_ARMOUR_SALE_PLUS;
		// MT2009_PLUS_BOT_LIST_ALL_GEAR_V1: and any spare piece taken to +4
		// before it goes on the counter.
		if (PlayerBotRefinesSpareForSale(ch, item))
			return item->GetRefineLevel() < PLAYERBOT_SPARE_SALE_PLUS;
		// The class's own level-30 weapon, whatever the damage model makes of
		// it today (community patch 2, point 1).
		if (IsPlayerBotPersonaEnabled() && item == FindPlayerBotClassLevel30Weapon(ch))
			return item->GetRefineLevel() < GetPlayerBotRefineTarget(ch, item);
		// The weapon kept for its lines, taken to the hand's plus.
		if (item->GetType() == ITEM_WEAPON && item == FindPlayerBotLinesProject(ch))
			return item->GetRefineLevel() < GetPlayerBotRefineTarget(ch, item);
		if (!IsPlayerBotEquipmentCandidate(ch, item) ||
				IsPlayerBotJunkItem(ch, item))
			return false;
		// An Archer's stone dagger is worn only on a stone, so it is neither a
		// wearable upgrade nor a higher-tier spare - yet it must reach +4 to break
		// stones at all . Refine it in the bag like a worn piece.
		if (IsPlayerBotArcherStoneWeapon(ch, item))
			return item->GetRefineLevel() < GetPlayerBotRefineTarget(ch, item);
		// And the level-30 weapon it is grinding: not worn yet because it is not
		// yet better, and not better until it is refined.
		// The project, and every other level-30 weapon this bot keeps for the
		// anvil: it is not worn yet because it is not better yet, and it will
		// not be better until it is refined.
		if (PlayerBotKeepsLevel30ForAnvil(ch, item))
			return item->GetRefineLevel() < GetPlayerBotRefineTarget(ch, item);
		return IsPlayerBotHigherTierSpare(ch, item) ||
				IsPlayerBotWearableUpgrade(ch, item, item->GetCell());
	}

	// Iwakura's community patch 2, point 11: the Perfectionists who buy a
	// finished piece rather than make one, fifteen in a hundred by pid.
	bool IsPlayerBotMarketPerfectionist(DWORD pid)
	{
		if ((int)(PlayerBotNavHash(pid ^ 0x52454459U) % 100U) < PLAYERBOT_READY_GEAR_PERCENT)
			return true;
		// So are the Grinders who never hold and the ones who gave grinding
		// up: "maja glownie kupowac ulepszacze i przedmioty z rynku"
		// (community patch 2, point 2).
		if (playerbot_persona::NeverHoldsAtLocks(pid))
			return true;
		TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(pid);
		// And a medal dropper that met its goal (community patch 2, point 4).
		return st != s_mapPlayerBotAIStates.end() &&
				(st->second.persona.bQuitGrinding || st->second.persona.bMedalGoalDone);
	}

	// Whether a wear slot is one the finished piece is looked for in: the
	// weapon, the armour, the shield and the helmet.
	bool IsPlayerBotReadyGearSlot(int wearCell)
	{
		return wearCell == WEAR_WEAPON || wearCell == WEAR_BODY || wearCell == WEAR_SHIELD ||
				wearCell == WEAR_HEAD;
	}

	// The slot's place in TPlayerBotPersona's ready-gear clocks, or -1.
	int GetPlayerBotReadyGearSlotIndex(int wearCell)
	{
		switch (wearCell)
		{
			case WEAR_WEAPON: return 0;
			case WEAR_BODY: return 1;
			case WEAR_SHIELD: return 2;
			case WEAR_HEAD: return 3;
		}
		return -1;
	}

	// A finished piece for this bot: its class's, of the slot, at +8 or +9,
	// no higher than its level and at most PLAYERBOT_READY_GEAR_LEVEL_WINDOW
	// under it ("bot majacy 34. poziom zauwazy Smiertelna Zbroje Plytowa +8").
	bool IsPlayerBotReadyGearProto(LPCHARACTER ch, const TItemTable* proto, DWORD vnum, int wearCell)
	{
		if (!ch || !proto || (proto->bType != ITEM_WEAPON && proto->bType != ITEM_ARMOR) ||
				(int)(vnum % 10) < PLAYERBOT_READY_GEAR_MIN_PLUS || !IsPlayerBotProtoForCharacter(ch, proto))
			return false;
		if (proto->bType == ITEM_WEAPON)
		{
			if (wearCell != WEAR_WEAPON || !IsPlayerBotWeaponSubTypeFor(ch, proto->bSubType))
				return false;
		}
		else if ((wearCell == WEAR_BODY && proto->bSubType != ARMOR_BODY) ||
				(wearCell == WEAR_SHIELD && proto->bSubType != ARMOR_SHIELD) ||
				(wearCell == WEAR_HEAD && proto->bSubType != ARMOR_HEAD) || wearCell == WEAR_WEAPON)
			return false;
		const int level = GetPlayerBotProtoLevelLimit(proto);
		return level <= (int)ch->GetLevel() && level + PLAYERBOT_READY_GEAR_LEVEL_WINDOW >= (int)ch->GetLevel();
	}

	// Whether a counter on this bot's map holds one for this slot that the
	// purchase could take: better than the piece worn there (none worn, one
	// worn under +8, or a lower level) and at least
	// PLAYERBOT_MARKET_GEAR_PLUS_OVER_WORN grades over it - the purchase's own
	// rule (WantsPlayerBotStallItem). Read off the ledger, without walking to
	// a counter. It asked for any +8 anywhere on the core, so a bot wearing a
	// +7 held its anvil twenty minutes for a +8 it could not buy (B03 of
	// Iwakura's audit).
	// The purchase's rule for a finished piece against the one worn in its
	// slot (WantsPlayerBotStallItem): none worn, one worn under +8, or a lower
	// level - and PLAYERBOT_MARKET_GEAR_PLUS_OVER_WORN grades over it.
	bool IsPlayerBotReadyGearOverWorn(LPCHARACTER ch, const TItemTable* proto, DWORD vnum, int wearCell)
	{
		LPITEM worn = ch->GetWear((WORD)wearCell);
		if (!worn)
			return true;
		if ((int)(vnum % 10) < (int)worn->GetRefineLevel() + PLAYERBOT_MARKET_GEAR_PLUS_OVER_WORN)
			return false;
		const int wornLevel = worn->GetProto() ? GetPlayerBotProtoLevelLimit(worn->GetProto()) : -1;
		return worn->GetRefineLevel() < PLAYERBOT_READY_GEAR_MIN_PLUS ||
				GetPlayerBotProtoLevelLimit(proto) > wornLevel;
	}

	// Whether a counter on this bot's map holds one for this slot that the
	// purchase could take, read off the ledger without walking to a counter.
	// It asked for any +8 anywhere on the core, so a bot wearing a +7 held its
	// anvil twenty minutes for a +8 it could not buy, on all four slots at
	// once (B03 of Iwakura's audit of 26 September).
	bool PlayerBotMarketHasReadyGear(LPCHARACTER ch, int wearCell)
	{
		if (!ch || !IsPlayerBotReadyGearSlot(wearCell))
			return false;
		for (TPlayerBotMarketLedger::const_iterator it = s_mapMarketLedger.begin(); it != s_mapMarketLedger.end(); ++it)
		{
			if (it->second.dwSupplyUnits == 0 ||
					GetPlayerBotMarketLocalSupply(ch->GetMapIndex(), it->first) == 0)
				continue;
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(it->first);
			if (IsPlayerBotReadyGearProto(ch, proto, it->first, wearCell) &&
					IsPlayerBotReadyGearOverWorn(ch, proto, it->first, wearCell))
				return true;
		}
		return false;
	}

	// Whether the buyer would take such a piece off a counter of this map and
	// could pay for it - the one question the anvil's wait and the purchase
	// answer alike. On the 2.x line the stands are read line by line and the
	// line found is handed to the buyer (playerbot_offline_market.h), because
	// a browse reads sixty-four lines a look and would not reach it inside
	// the wait; elsewhere the ledger's answer is all there is, and the market
	// trip the wait asks for is what finds the counter.
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
	bool PlayerBotFindReadyGearToBuy(LPCHARACTER ch, TPlayerBotAIState& state, int wearCell);
	// MT2009_PLUS_BOSS_RAID_V2 (2.2.52, burn): playerbot_offline_market.h.
	bool PlayerBotFindBurnReplacementToBuy(LPCHARACTER ch, TPlayerBotAIState& state, int wearCell, int minLevel);
#else
	// Without the stands' lines the market trip is what finds a counter, and
	// the merchant waits for it as before.
	bool PlayerBotFindBurnReplacementToBuy(LPCHARACTER, TPlayerBotAIState&, int, int)
	{
		return true;
	}
	bool PlayerBotFindReadyGearToBuy(LPCHARACTER ch, TPlayerBotAIState&, int wearCell)
	{
		return PlayerBotMarketHasReadyGear(ch, wearCell);
	}
#endif

	// Whether the market Perfectionist's anvil waits for a finished piece for
	// this slot now.
	bool IsPlayerBotReadyGearHeld(const TPlayerBotAIState& state, int wearCell, DWORD dwNow)
	{
		const int index = GetPlayerBotReadyGearSlotIndex(wearCell);
		return index >= 0 && state.persona.adwReadyGearWaitUntil[index] != 0 &&
				dwNow < state.persona.adwReadyGearWaitUntil[index];
	}

	// The same wait as the planner sees it: a piece for a slot the anvil holds
	// is no reason to walk to the blacksmith. The class's level-30 weapon is
	// never held (its own rule's, in the refining pass), nor a piece under
	// Iwakura's scroll rule, whose scrolls are the answer already.
	bool IsPlayerBotRefineHeldForReadyGear(LPCHARACTER ch, LPITEM item, int wearCell, DWORD dwNow)
	{
		if (!ch || !item || !IsPlayerBotPersonaEnabled())
			return false;
		TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (st == s_mapPlayerBotAIStates.end())
			return false;
		const int slot = wearCell >= 0 ? wearCell : item->FindEquipCell(ch);
		return IsPlayerBotReadyGearHeld(st->second, slot, dwNow) &&
				item != FindPlayerBotClassLevel30Weapon(ch) && !IsPlayerBotScrollRulePiece(ch, item);
	}

	// A counter's piece the market Perfectionist buys: a finished one by the
	// rule above, and an upgrade on what it wears (WantsPlayerBotStallItem's
	// gear test still decides that). Paid from the Perfectionist's share.
	bool IsPlayerBotReadyGearOffer(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !offer || !IsPlayerBotPersonaEnabled() ||
				!IsPlayerBotMarketPerfectionist(ch->GetPlayerID()) || !IsPlayerBotEquipmentCandidate(ch, offer))
			return false;
		const int wearCell = offer->FindEquipCell(ch);
		return IsPlayerBotReadyGearSlot(wearCell) &&
				IsPlayerBotReadyGearProto(ch, offer->GetProto(), offer->GetVnum(), wearCell) &&
				offer->GetRefineLevel() >= PLAYERBOT_READY_GEAR_MIN_PLUS;
	}

	bool IsPlayerBotHeldForCompany(LPCHARACTER ch); // playerbot_companions.h

	// Iwakura's Patch 4, point 7, "Protokol Odbudowy": a bot whose only weapon,
	// body armour or shield burnt at the anvil looks at the market for a
	// finished one before the merchant's plain piece - "ma bezwzgledny obowiazek
	// przeszukac rynek (sklepy offline) i zakupic gotowy, nowy przedmiot
	// zastepczy za posiadane Yang". The merchant waits PLAYERBOT_REBUILD_MARKET_MS
	// for it (ManagePlayerBotWeaponMerchant, ManagePlayerBotArmorMerchant).
	struct TPlayerBotRebuild
	{
		DWORD dwUntil;
		BYTE bSlot;
	};
	std::map<DWORD, TPlayerBotRebuild> s_mapPlayerBotRebuild;

	bool IsPlayerBotRebuildingFromMarket(LPCHARACTER ch, BYTE slot)
	{
		if (!ch)
			return false;
		std::map<DWORD, TPlayerBotRebuild>::iterator it = s_mapPlayerBotRebuild.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotRebuild.end())
			return false;
		if ((int)(it->second.dwUntil - get_dword_time()) <= 0)
		{
			s_mapPlayerBotRebuild.erase(it);
			return false;
		}
		return it->second.bSlot == slot;
	}

	// Anything this bot can put on in the slot, worn or in the bag.
	bool PlayerBotHasPieceForSlot(LPCHARACTER ch, BYTE slot)
	{
		if (!ch || ch->GetWear(slot))
			return true;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetCell() == cell && !item->IsEquipped() &&
					IsPlayerBotEquipmentCandidate(ch, item) && item->GetLevelLimit() <= ch->GetLevel() &&
					item->FindEquipCell(ch) == (int)slot)
				return true;
		}
		return false;
	}

	bool ManagePlayerBotRefining(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || dwNow < state.dwNextRefineCheckTime || IsPlayerBotGearFrozen(ch))
			return false;

		state.dwNextRefineCheckTime = dwNow + PLAYERBOT_REFINE_INTERVAL;

		// MT2009_PLUS_AWAKENING_V1: a worn weapon 75 +9, the Awakening Stone and
		// the fee make the ritual this visit's first step (playerbot_awakening.h).
		if (ManagePlayerBotAwakeningRitual(ch, state, dwNow))
			return true;
		// MT2009_PLUS_HEAVEN_OIL_V1 (Autor: Digi Rasta): then one soul stone step, when the bag holds
		// a stone +4..+8, the dust, the oil and the fee (playerbot_awakening.h).
		if (ManagePlayerBotSoulStoneStep(ch, state, dwNow))
			return true;

		// Iwakura's Perfectionist spends at most PERFECT_BUDGET_PERCENT of what
		// it walked into town with ("max 80% yang"), and keeps the rest. The
		// class's level-30 weapon is the exception: it has a budget of its own
		// (GetPlayerBotLevel30BudgetLeft), which this one used to stop first.
		const bool personaOn = IsPlayerBotPersonaEnabled();
		const bool perfectSpent = personaOn && state.persona.llVisitGoldStart > 0 &&
				(long long)ch->GetGold() * 100 <
					state.persona.llVisitGoldStart * (100 - playerbot_persona::PERFECT_BUDGET_PERCENT);
		if (perfectSpent)
			PlayerBotLogThrottled("perfectionist_budget", dwNow,
					"PLAYERBOT_PERSONA: perfectionist budget spent pid=%u name=%s gold=%lld start=%lld",
					ch->GetPlayerID(), ch->GetName(), (long long)ch->GetGold(),
					state.persona.llVisitGoldStart);

		// Collect all upgradable worn items and inventory candidates
		struct TRefineCandidate
		{
			BYTE wearCell;
			LPITEM item;
			BYTE plusLevel;
			BYTE priority;
		};

		std::vector<TRefineCandidate> candidates;
		// The pieces this session took off (adwRefineTakenOffItem). The first
		// step unequips the worn piece and every step after it finds the piece
		// in the bag, so it was a spare to every rule that asks the slot: no
		// scroll for the eighty and sixty percent steps of the piece in the
		// hand, no market first after its burn (B12 of Iwakura's audit of 26
		// September). The piece that was taken off - by its id, which each
		// step moves on to what the anvil hands back - is the worn one, and no
		// other piece for its slot.
		if (dwNow - state.dwRefineTakenOffAt >= PLAYERBOT_REFINE_TAKEN_OFF_MS)
			ClearPlayerBotRefineTakenOff(state);
		// The class's own level-30 weapon goes to the anvil first, ahead of
		// everything the Perfectionist's order ranks (community patch 2,
		// point 1: "ABSOLUTNY PRIORYTET").
		LPITEM classLevel30 = personaOn ? FindPlayerBotClassLevel30Weapon(ch) : NULL;
		// And a piece under Iwakura's scroll rule goes on past the
		// Perfectionist's half: its step is the scroll's and the materials'
		// (Community Patch 5, point 4), and a rule a purse's share can stop at
		// every visit is not the rule.
		if (perfectSpent && !classLevel30 && !PlayerBotHasScrollRuleWork(ch))
			return false;
		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};

		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
		{
			LPITEM item = ch->GetWear(wearSlots[i]);
			if (!item || item->GetRefinedVnum() == 0 ||
					!IsPlayerBotWornItemSound(ch, item, wearSlots[i]))
				continue;

			const BYTE plusLevel = item->GetRefineLevel();
			const bool coreProgression = IsPlayerBotCoreProgressionItem(ch, item);
			if (plusLevel >= GetPlayerBotRefineTarget(ch, item))
				continue;
			// A worn piece is taken off for the anvil below, so a step the bag
			// cannot pay for - a material short, or the fee - is not queued at
			// all. It was: the engine refused the attempt after the unequip, the
			// equipment pass put the piece back, and the next blacksmith tick
			// took it off again, every three seconds for the whole visit
			// ("refine SKIPPED ... materials=30057:2/21,27799:1/0" beside
			// "equipped upgrade wear=0 old_vnum=0", bandyciaras, 14 September).
			if (!CanPlayerBotAttemptRefineItem(ch, item))
				continue;

			TRefineCandidate cand;
			cand.wearCell = wearSlots[i];
			cand.item = item;
			cand.plusLevel = plusLevel;
			// The Perfectionist's order: the weapon, the armour, the shield,
			// then the rest (GetPlayerBotPerfectionistRank).
			cand.priority = item == classLevel30 ? 0 : 1 + (personaOn ? GetPlayerBotPerfectionistRank(ch, item)
					: (coreProgression ? 0 : 2));
			// A medal dropper that met its goal: the weapon first from level
			// thirty, the armour first under it (community patch 2, point 4).
			if (personaOn && state.persona.bMedalGoalDone && cand.priority != 0 &&
					wearSlots[i] == (ch->GetLevel() >= PLAYERBOT_MEDAL_GOAL_WEAPON_FIRST_LEVEL ? WEAR_WEAPON : WEAR_BODY))
				cand.priority = 1;
			candidates.push_back(cand);
		}

		// Also collect candidate gear in inventory
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			// What the merchant would take on the next town visit is not
			// worth a refine now: Ametystowy Naszyjnik+0 was raised to +1 at
			// 17:58 and sold for scrap at 18:19. A spare that is kept - an
			// upgrade, a higher tier than the worn piece, a reserve at +6 -
			// is worth raising; the rest is scrap and stays at what it is.
			// The planner asks the same function, above.
			LPITEM item = ch->GetInventoryItem(cell);
			// "Na 341 broni na serwerze praktycznie wszystkie sa +0 (max +2),
			// boty ich wcale nie ulepszaja" (Iwakura, 13 September). Four
			// different rules can pass a bag piece over here and from outside
			// they look identical - which is why that report could be neither
			// confirmed nor explained from any log. A level-30 weapon is the
			// one the market watches, so when one is passed over it says which
			// rule did it, once a minute for the whole population.
			if (!IsPlayerBotRefineBagCandidate(ch, item))
			{
				if (IsPlayerBotSpecialLevel30Weapon(item))
					PlayerBotLogThrottled("refine_l30_skipped", dwNow,
							"PLAYERBOT_AI: level-30 weapon not refined pid=%u name=%s vnum=%u plus=%u level=%u reason=not_a_candidate junk=%d spare=%d upgrade=%d",
							ch->GetPlayerID(), ch->GetName(), item->GetVnum(),
							(unsigned int)item->GetRefineLevel(), (unsigned int)ch->GetLevel(),
							IsPlayerBotJunkItem(ch, item) ? 1 : 0,
							IsPlayerBotHigherTierSpare(ch, item) ? 1 : 0,
							IsPlayerBotWearableUpgrade(ch, item, item->GetCell()) ? 1 : 0);
				continue;
			}

			const BYTE plusLevel = item->GetRefineLevel();
			if (plusLevel >= GetPlayerBotRefineTarget(ch, item))
			{
				if (IsPlayerBotSpecialLevel30Weapon(item))
					PlayerBotLogThrottled("refine_l30_target", dwNow,
							"PLAYERBOT_AI: level-30 weapon not refined pid=%u name=%s vnum=%u plus=%u level=%u reason=target_reached target=%u",
							ch->GetPlayerID(), ch->GetName(), item->GetVnum(),
							(unsigned int)plusLevel, (unsigned int)ch->GetLevel(),
							(unsigned int)GetPlayerBotRefineTarget(ch, item));
				continue;
			}
			// A step the engine would refuse is no candidate, as it is not for a
			// worn piece above: a body armour at +7 short of a material was
			// tried at every blacksmith tick of every visit, "refine SKIPPED ...
			// vnum=11647 plus=7 materials=30006:2/0" twenty to forty times an
			// hour a bot on m2zip (28 September).
			if (!CanPlayerBotAffordRefineAttempt(ch, item))
				continue;

			TRefineCandidate cand;
			cand.wearCell = 255;
			{
				const int slot = GetPlayerBotTakenOffSlotOf(ch, state, item);
				if (slot >= 0)
					cand.wearCell = (BYTE)slot;
			}
			cand.item = item;
			cand.plusLevel = plusLevel;
			const bool coreProgression = IsPlayerBotCoreProgressionItem(ch, item);
			cand.priority = item == classLevel30 ? 0 : 1 + (personaOn ? GetPlayerBotPerfectionistRank(ch, item)
					: (coreProgression ? 0 : 2));
			// MT2009_PLUS_BOT_LIST_ALL_GEAR_V1: a spare for the counter comes
			// after everything the bot refines for itself.
			if (PlayerBotRefinesSpareForSale(ch, item))
				cand.priority = PLAYERBOT_SPARE_SALE_REFINE_PRIORITY;
			candidates.push_back(cand);
		}

		if (candidates.empty())
			return false;

		// Core level-appropriate weapon/body gear comes first. Within the same
		// priority, raise the lowest plus level so both essentials progress evenly.
		for (size_t i = 0; i < candidates.size(); ++i)
		{
			for (size_t j = i + 1; j < candidates.size(); ++j)
			{
				if (candidates[j].priority < candidates[i].priority ||
						(candidates[j].priority == candidates[i].priority &&
						 candidates[j].plusLevel < candidates[i].plusLevel))
				{
					TRefineCandidate tmp = candidates[i];
					candidates[i] = candidates[j];
					candidates[j] = tmp;
				}
			}
		}

		int refinedCount = 0;
		for (size_t i = 0; i < candidates.size() && refinedCount < 2; ++i)
		{
			LPITEM item = candidates[i].item;
			if (!item || item->GetRefinedVnum() == 0)
				continue;
			// A guild smith takes the gear of level thirty and up, and the plain
			// blacksmith leaves it to one on this map (playerbot_guild_land.h).
			LPCHARACTER guildSmith = NULL;
			if (!PlayerBotRefineAnvilTakes(ch, state, item, &guildSmith))
				continue;

			const DWORD oldVnum = item->GetVnum();
			const DWORD nextVnum = item->GetRefinedVnum();
			const BYTE plusLevel = candidates[i].plusLevel;

			// MT2009_PLUS_BOT_LIST_ALL_GEAR_V1: a spare for the counter is not
			// worn, so its level is the buyer's, and no ready piece off a
			// counter is looked for in its stead.
			const bool saleSpare = candidates[i].priority == PLAYERBOT_SPARE_SALE_REFINE_PRIORITY;
			// What comes off the anvil must still fit. See IsPlayerBotWearableAtLevel
			// for why the engine will not stop this on its own.
			if (!saleSpare && !IsPlayerBotWearableAtLevel(ch, nextVnum))
			{
				PlayerBotLogThrottled("refine_outgrows", dwNow,
						"PLAYERBOT_AI: refine would outgrow the bot pid=%u name=%s level=%u vnum=%u next=%u plus=%u",
						ch->GetPlayerID(), ch->GetName(), ch->GetLevel(),
						oldVnum, nextVnum, (unsigned int)plusLevel + 1);
				continue;
			}
			const BYTE wearCell = candidates[i].wearCell;
			// A piece under Iwakura's scroll rule (Community Patch 5, point 4):
			// the step is the scroll's, so no wait for a counter's finished
			// piece, no Perfectionist's share and no backup's caution stops it.
			const bool rulePiece = IsPlayerBotScrollRulePiece(ch, item);
			// The market Perfectionist looks at the counters first (community
			// patch 2, point 11): a finished +8 or +9 for this slot there, and
			// the anvil waits PLAYERBOT_READY_GEAR_WAIT_MS for the purchase;
			// nothing there, or the wait over, and it refines as ever. The
			// class's level-30 weapon is its own rule's.
			if (personaOn && item != classLevel30 && !rulePiece && !saleSpare && IsPlayerBotMarketPerfectionist(ch->GetPlayerID()))
			{
				const int slot = wearCell != 255 ? (int)wearCell : item->FindEquipCell(ch);
				const int index = GetPlayerBotReadyGearSlotIndex(slot);
				if (index >= 0)
				{
					TPlayerBotPersona& p = state.persona;
					// A wait whose slot now wears what no counter can better by
					// the purchase's rule - the piece it waited for, bought -
					// ends here rather than at its clock.
					LPITEM wornHere = ch->GetWear((WORD)slot);
					if (p.adwReadyGearWaitUntil[index] != 0 && wornHere &&
							(int)wornHere->GetRefineLevel() + PLAYERBOT_MARKET_GEAR_PLUS_OVER_WORN > 9)
						p.adwReadyGearWaitUntil[index] = 0;
					// Looked at once in PLAYERBOT_READY_GEAR_RECHECK_MS a slot,
					// found or not: the look on the 2.x line reads the stands
					// line by line.
					if (p.adwReadyGearCheckedAt[index] == 0 ||
							dwNow - p.adwReadyGearCheckedAt[index] >= PLAYERBOT_READY_GEAR_RECHECK_MS)
					{
						p.adwReadyGearCheckedAt[index] = dwNow;
						if (PlayerBotFindReadyGearToBuy(ch, state, slot))
						{
							p.adwReadyGearWaitUntil[index] = dwNow + PLAYERBOT_READY_GEAR_WAIT_MS;
							sys_log(0, "PLAYERBOT_MARKET: perfectionist looks for a finished piece pid=%u name=%s level=%u slot=%d worn_plus=%d",
									ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), slot,
									ch->GetWear((WORD)slot) ? (int)ch->GetWear((WORD)slot)->GetRefineLevel() : -1);
						}
					}
					if (IsPlayerBotReadyGearHeld(state, slot, dwNow))
						continue;
				}
			}
			// Past the Perfectionist's half only the level-30 weapon goes on,
			// and the scroll rule's piece.
			if (perfectSpent && item != classLevel30 && !rulePiece)
				continue;
			// The level-30 weapon's purchase and anvil share one budget
			// (GetPlayerBotLevel30BudgetLeft): the step's fee has to fit in
			// what is left of it.
			if (item == classLevel30)
			{
				const TRefineTable* l30Recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
				const long long l30Fee = l30Recipe ? (long long)ch->ComputeRefineFee(l30Recipe->cost) : 0;
				const long long l30Left = GetPlayerBotLevel30BudgetLeft(ch, dwNow);
				if (l30Left < l30Fee)
				{
					PlayerBotLogThrottled("refine_l30_budget", dwNow,
							"PLAYERBOT_AI: level-30 weapon budget spent pid=%u name=%s vnum=%u plus=%u gold=%lld left=%lld fee=%lld",
							ch->GetPlayerID(), ch->GetName(), oldVnum, (unsigned int)plusLevel,
							(long long)ch->GetGold(), l30Left, l30Fee);
					continue;
				}
			}
			// The caution of a piece with nothing behind it is the plain anvil's:
			// under the scroll rule a failure hands the piece back.
			const bool hasBackup = rulePiece || ((wearCell != 255) ? HasPlayerBotBackupGear(ch, wearCell, item) : true);

			if (wearCell == WEAR_WEAPON && !hasBackup && plusLevel >= 4 && ch->GetGold() < 5000)
				continue;

			if (plusLevel >= 5 && !hasBackup && ch->GetGold() < 15000)
				continue;

			if (plusLevel == 4 && !hasBackup && number(1, 100) > 75)
				continue;

			// Asked of every piece at the attempt, worn or not: the step before
			// this one may have spent the fee or the material this one counted
			// on, and a specimen the Biologist is still owed is no material at
			// all (GetPlayerBotBiologistReserve) - DoRefine would take it anyway.
			if (!CanPlayerBotAttemptRefineItem(ch, item))
				continue;
			// Equipment management after an earlier attempt may have equipped another
			// queued candidate, so inspect its live position instead of trusting the
			// location captured when the list was built.
			if (item->IsEquipped())
			{
				int emptyCell = ch->GetEmptyInventory(item->GetSize());
				if (emptyCell < 0)
					continue;
				if (!ch->UnequipItem(item) || item->IsEquipped())
					continue;
				if (wearCell < WEAR_MAX_NUM)
				{
					state.adwRefineTakenOffItem[wearCell] = item->GetID();
					state.dwRefineTakenOffAt = dwNow;
				}
			}
			// Only a piece in this bot's own bag goes to the anvil. DoRefine
			// removes what it burns by its cell, and a piece in a slot the engine
			// does not count as worn is removed from nowhere - the slot keeps
			// pointing at a destroyed item (IsPlayerBotWornItemSound).
			if (item->GetOwner() != ch || item->GetWindow() != INVENTORY ||
					item->GetCell() >= PLAYERBOT_BAG_CELLS || ch->GetInventoryItem(item->GetCell()) != item)
				continue;

			// DoRefine(false) is the regular blacksmith path: it reads refine_proto,
			// charges the exact fee, consumes every required material and applies the
			// normal success/failure roll.  The return value only says that an attempt
			// was performed, so compare the result item count to log its real outcome.
			const int resultCountBefore = ch->CountSpecifyItem(nextVnum);
			// What the recipe asks and what the bag holds, taken before the
			// attempt takes it. "The bot refined to +8 without Orkowe Jadra" was
			// read off a bag after the refine had consumed them (jaksiezabic,
			// 14 September), and nothing in the log could say otherwise: need/have
			// by vnum, "none" for a step whose recipe names no material.
			char materials[128] = "none";
			if (const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet()))
			{
				size_t used = 0;
				for (int m = 0; m < recipe->material_count && used + 24 < sizeof(materials); ++m)
					used += snprintf(materials + used, sizeof(materials) - used, "%s%u:%d/%d",
							m ? "," : "", (unsigned int)recipe->materials[m].vnum, (int)recipe->materials[m].count,
							(int)ch->CountSpecifyItem(recipe->materials[m].vnum));
			}
			// With a Blessing Scroll in the bag and a level worth protecting, go
			// the scroll's way: the engine reads the scroll from the cell set by
			// SetRefineMode, spends it, and on failure hands back the item one
			// level down rather than nothing.
			int scrollCell = -1;
			// A prize piece may ask for a scroll at any plus, not only from +6.
			// The +6 rule is about not spending a scarce scroll on an ordinary
			// item; a piece the bot refuses to risk needs one wherever it
			// stands, and without this it was refused a scroll below +6 and
			// then held for want of one - the deadlock that parked 451 weapons
			// on +4.
			// And the piece in the bot's hands, at a step that can burn it, goes
			// under a scroll whatever its plus. GetPlayerBotRefineTarget reads a
			// scroll in the bag as a ladder to +9, and then the +4 and +5 steps -
			// eighty and sixty percent - went to the plain anvil and burned the
			// worn weapon: "biegaja do kowala, pala swoj glowny item" with 55 000
			// Blessing Scrolls in the bags of 1100 bots (uxietoszef). A spare in
			// the bag keeps the +6 rule: its burn is the price of not spending a
			// scarce scroll on it.
			const TRefineTable* stepRecipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
			// MT2009_PLUS_AWAKENING_V1: an awakened weapon's step never burns it
			// (server-patches/digirasta), so it asks no scroll for that.
			const bool wornStepCanBurn = wearCell != 255 && stepRecipe &&
					stepRecipe->prob <= PLAYERBOT_WORN_SCROLL_MAX_PROB &&
					!IsPlayerBotAwakenedWeaponVnum(item->GetVnum());
			// Every rule above gives way to the operator's floor: under
			// SCROLL_FROM no scroll goes on the step, whatever the piece.
			const bool scrollStepAllowed = IsPlayerBotScrollStepAllowed(plusLevel);
			const int stepProb = stepRecipe ? (int)stepRecipe->prob : 100;
			// From PLAYERBOT_WEAPON_SCROLL_ONLY_AVERAGE a weapon goes under a
			// scroll at every step and never to the plain anvil - past the
			// operator's floor too, or under SCROLL_FROM it could never be refined
			// at all (15 September). A weapon of the operator's anvil table
			// under that line - the level-30 family and every weapon from level
			// thirty - goes to the plain anvil as far as its ceiling and under a
			// scroll from there (GetPlayerBotWeaponAnvilCeiling).
			const bool scrollOnly = IsPlayerBotScrollOnlyWeapon(item);
			const bool tableWeapon = !scrollOnly && IsPlayerBotAnvilTableWeapon(item);
			// Iwakura's quick fix of 23 September: the class's own level-30
			// weapon goes to +6 at least whatever its average - under a scroll
			// when the bag holds one for the step, at the plain anvil when it
			// does not - rather than wait at +0 for a scroll that never comes.
			const bool level30Floor = IsPlayerBotLevel30UnderFloor(ch, item);
			// The weapon in the hand - or the one going back into it, since the
			// session keeps it in the bag - at a step that can burn it, with no
			// backup and nothing a merchant sells at its level: under a scroll or
			// not at all (IsPlayerBotWornWeaponAtRisk). CanPlayerBotAttemptRefineItem
			// above already refused it without a scroll; this is the scroll's half.
			const bool handAtRisk = IsPlayerBotWornWeaponAtRisk(ch, item, true) ||
					IsPlayerBotWornArmourAtRisk(ch, item);
			// Iwakura's coin (PlayerBotRisksPlainAnvil): half the steps a scroll
			// would take, or wait for, go to the plain anvil. What the backup
			// rule and the scroll-only line protect never comes up heads.
			const bool coinAnvil = !scrollOnly && !handAtRisk && PlayerBotRisksPlainAnvil(ch, item);
			const char* coinWhy = NULL;
			if (scrollOnly)
				scrollCell = FindPlayerBotRefineScrollCellFor(ch, item, stepProb);
			else if (tableWeapon)
			{
				// The operator's table (GetPlayerBotWeaponAnvilCeiling): below
				// the ceiling the bot grinds at the anvil and takes the burn
				// risk, at or above it the step is a scroll's. The better the
				// average, the lower the ceiling - what is being protected is
				// the roll, not the plus.
				const long average = SumPlayerBotItemLines(item, APPLY_NORMAL_HIT_DAMAGE_BONUS);
				const int anvilCeiling = GetPlayerBotWeaponAnvilCeiling(ch, item);
				const bool aboveCeiling = (int)plusLevel >= anvilCeiling;
				// A common level-30 roll is worth a gamble even above its
				// ceiling: the family is everywhere and the scroll is not.
				// Iwakura's coin never comes up for a weapon of the table
				// (PlayerBotRisksPlainAnvil).
				// Not the weapon under Iwakura's scroll rule, whose ceiling is +0
				// because every step of it is a scroll's.
				const bool cheapGamble = aboveCeiling && !IsPlayerBotScrollRuleWeapon(ch, item) &&
						IsPlayerBotCheapLevel30Roll(item) && number(1, 100) <= PLAYERBOT_LEVEL30_CHEAP_ANVIL_PERCENT;
				// Under the ceiling the only weapon the bot has, at a step that
				// burns (IsPlayerBotWornWeaponAtRisk), still goes under a scroll
				// the bag holds, or waits for one below.
				if (scrollStepAllowed && ((aboveCeiling && !cheapGamble) || handAtRisk))
					scrollCell = FindPlayerBotRefineScrollCellFor(ch, item, stepProb);
				// Waiting for a scroll is the point of the ceiling: the anvil
				// here is how a good roll is lost. Under the operator's
				// SCROLL_FROM no scroll may go on the step, and the anvil's odds
				// stand, as for every other piece.
				if (aboveCeiling && !cheapGamble && scrollStepAllowed && scrollCell < 0)
				{
					PlayerBotLogThrottled("refine_weapon_ceiling", dwNow,
							"PLAYERBOT_AI: weapon waits for a scroll pid=%u name=%s vnum=%u level=%d plus=%u avg=%ld ceiling=%d prob=%d where=anvil",
							ch->GetPlayerID(), ch->GetName(), oldVnum, item->GetLevelLimit(),
							(unsigned int)plusLevel, average, anvilCeiling, stepProb);
					continue;
				}
			}
			else if (scrollStepAllowed &&
					(plusLevel >= PLAYERBOT_SCROLL_REFINE_MIN_PLUS || IsPlayerBotPrizeItem(item) ||
						wornStepCanBurn || handAtRisk || rulePiece))
			{
				scrollCell = FindPlayerBotRefineScrollCellFor(ch, item, stepProb);
				// The coin keeps the scroll for another step, or for the counter.
				// Never the scroll rule's (PlayerBotRisksPlainAnvil says no for it).
				if (coinAnvil && scrollCell >= 0)
				{
					coinWhy = "scroll_kept";
					scrollCell = -1;
				}
			}
			// The armour under the scroll rule goes under a scroll at every step
			// or waits; the bag holds three by the rule's own count, so this is
			// a scroll kind no step may take (the War God's past +3).
			if (scrollCell < 0 && rulePiece && item->GetType() == ITEM_ARMOR)
			{
				PlayerBotLogThrottled("refine_rule_armour_no_scroll", dwNow,
						"PLAYERBOT_AI: scroll rule armour waits, no scroll for the step pid=%u name=%s vnum=%u plus=%u prob=%d",
						ch->GetPlayerID(), ch->GetName(), oldVnum, (unsigned int)plusLevel, stepProb);
				continue;
			}
			if (scrollCell < 0 && scrollOnly && !level30Floor)
			{
				PlayerBotLogThrottled("refine_scroll_only", dwNow,
						"PLAYERBOT_AI: refine held, scroll-only weapon and no scroll pid=%u name=%s vnum=%u plus=%u prob=%d",
						ch->GetPlayerID(), ch->GetName(), oldVnum, (unsigned int)plusLevel, stepProb);
				continue;
			}
			if (scrollCell < 0 && scrollStepAllowed && handAtRisk)
				continue;
			// No scroll, a roll that can fail, and a weapon worth more than the
			// next plus: leave it. The blacksmith burns what he fails. A weapon of
			// the anvil table is not held here: its ceiling answers for it. Only
			// where a scroll may go at all: a piece held for a scroll the floor
			// forbids is held for good, the shape of the deadlock that once
			// parked 451 weapons on +4. Under SCROLL_FROM it takes the plain
			// anvil's odds like everything else, which is the setting - and so
			// does a piece no scroll goes on (IsPlayerBotScrollFreeGear). The
			// planner asks the same (CanPlayerBotAttemptRefineItem).
			if (scrollCell < 0 && scrollStepAllowed && !tableWeapon && !level30Floor &&
					!IsPlayerBotScrollFreeGear(item) && IsPlayerBotPrizeItem(item))
			{
				const TRefineTable* prt = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
				// Hold only where a failure really costs something. Ninety and
				// eighty percent are not odds worth freezing a weapon over, and
				// freezing it is what happened: every step of refine_proto is
				// under a hundred, so "prob < 100" held every prize item at
				// whatever plus it happened to have.
				if (prt && prt->prob < PLAYERBOT_PRIZE_SAFE_REFINE_PROB && coinAnvil)
					coinWhy = coinWhy ? coinWhy : "prize";
				else if (prt && prt->prob < PLAYERBOT_PRIZE_SAFE_REFINE_PROB)
				{
					PlayerBotLogThrottled("refine_prize_no_scroll", dwNow,
							"PLAYERBOT_AI: refine held, prize line and no blessing scroll pid=%u name=%s vnum=%u plus=%u prob=%d",
							ch->GetPlayerID(), ch->GetName(), oldVnum, (unsigned int)plusLevel, prt->prob);
					continue;
				}
			}
			// What the coin sent to the plain anvil, once a minute for everybody.
			if (coinWhy && scrollCell < 0)
				PlayerBotLogThrottled("refine_coin", dwNow,
						"PLAYERBOT_AI: refine at the plain anvil, the coin said so pid=%u name=%s vnum=%u plus=%u prob=%d why=%s",
						ch->GetPlayerID(), ch->GetName(), oldVnum, (unsigned int)plusLevel, stepProb, coinWhy);
			// Asked before the attempt, which may destroy the item.
			const bool classLevel30 = IsPlayerBotClassLevel30Weapon(ch, item);
			const long long goldBeforeAttempt = (long long)ch->GetGold();
			const WORD cellBefore = item->GetCell();
			const DWORD idBefore = item->GetID();
			bool attempted = false;
			if (scrollCell >= 0)
			{
				ch->SetRefineMode(scrollCell);
				attempted = ch->DoRefineWithScroll(item);
				ch->ClearRefineMode();
			}
			else
			{
				// At a guild smith the engine's own rules: its fee, the
				// guild's share of it, and ten points on the odds.
				if (guildSmith)
					ch->SetRefineNPC(guildSmith);
				attempted = ch->DoRefine(item, false);
				if (guildSmith)
					ch->SetRefineNPC(NULL);
			}
			// The piece taken off for this step is still the worn one in the
			// shape the anvil handed it back - a new item in the same cell,
			// whatever the outcome - or gone (B12).
			if (attempted && wearCell < WEAR_MAX_NUM && state.adwRefineTakenOffItem[wearCell] != 0)
			{
				LPITEM after = ch->GetInventoryItem(cellBefore);
				state.adwRefineTakenOffItem[wearCell] = after ? after->GetID() : 0;
			}
			// The piece the anvil handed back - a new item in the same cell -
			// remembers what it was made from (playerbot_explain.h).
			if (attempted && IsPlayerBotExplainOn())
				if (LPITEM made = ch->GetInventoryItem(cellBefore))
					if (made->GetID() != idBefore)
						NotePlayerBotExplainOrigin(made, per::ORIGIN_REFINED, idBefore);
			if (attempted)
			{
				// The step's fee goes on the level-30 weapon's budget.
				if (classLevel30 && personaOn)
					NotePlayerBotLevel30Spend(ch, goldBeforeAttempt - (long long)ch->GetGold());
				const bool success = ch->CountSpecifyItem(nextVnum) > resultCountBefore;
				// Only a refine that landed is news. A scroll's failure hands the
				// piece back a grade down and a plain one burns it, and both were
				// shouted as luck ("ulepszylem zbroje +4 na +3", 15 September).
				if (success)
				{
					BroadcastPlayerBotRefineSuccess(ch, nextVnum, (int)plusLevel + 1);
					// +8 and +9 are Iwakura's euphoria (playerbot_mood.h).
					NotePlayerBotMoodRefine(ch, (int)plusLevel + 1);
				}
				else
				{
					// And a failure on the way to them costs a level of mood.
					NotePlayerBotMoodRefineFailure(ch, (int)plusLevel + 1,
							scrollCell >= 0 ? "downgraded" : "burned");
					// A worn piece burned: what goes on in its slot next goes on
					// after the burn (playerbot_explain.h).
					if (scrollCell < 0 && wearCell < WEAR_MAX_NUM)
						NotePlayerBotExplainBurn(ch->GetPlayerID(), wearCell, dwNow);
					// A burnt class weapon is bought again straight away, while the
					// bot still stands in the village ("bot ma obowiazek zakupic
					// kolejna sztuke broni na 30. poziom z rynku, jesli pozwala na to
					// jego budzet", Iwakura): the market's next look is now, not in
					// two to four minutes, when the bot may be on its way out.
					if (classLevel30 && scrollCell < 0)
					{
						state.dwNextShoppingTime = 0;
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
						state.offlineShop.nextBrowse = 0;
#endif
					}
					// The only piece of its slot gone: the market first, now
					// (IsPlayerBotRebuildingFromMarket; Patch 4, point 7).
					// MT2009_PLUS_BOSS_RAID_V2 (2.2.52): and only while a stand of
					// the map holds a replacement of the burnt piece's level that
					// the bot can pay for - and never for the Companion or a bot
					// held for company, which do not go to the stands at all
					// (BotOfflineBusyReason "company"): their merchant's piece
					// waited out the three minutes for a market they never saw.
					const bool marketReplacement = scrollCell < 0 &&
							(wearCell == WEAR_WEAPON || wearCell == WEAR_BODY || wearCell == WEAR_SHIELD) &&
							!PlayerBotHasPieceForSlot(ch, wearCell) &&
							!IsPlayerBotSidekickPID(ch->GetPlayerID()) && !IsPlayerBotHeldForCompany(ch) &&
							PlayerBotFindBurnReplacementToBuy(ch, state, wearCell,
									GetPlayerBotProtoLevelLimit(ITEM_MANAGER::instance().GetTable(oldVnum)));
					if (scrollCell < 0 && !marketReplacement && (wearCell == WEAR_WEAPON ||
							wearCell == WEAR_BODY || wearCell == WEAR_SHIELD) && !PlayerBotHasPieceForSlot(ch, wearCell))
					{
						// Straight to the merchant: the town visit's next look at
						// him buys the plain piece, and the anvil's next pass
						// raises it.
						std::map<DWORD, TPlayerBotRebuild>::iterator stale = s_mapPlayerBotRebuild.find(ch->GetPlayerID());
						if (stale != s_mapPlayerBotRebuild.end() && stale->second.bSlot == wearCell)
							s_mapPlayerBotRebuild.erase(stale);
						state.dwNextShoppingTime = 0;
						sys_log(0, "PLAYERBOT_MARKET: no market replacement after a burn, merchant now pid=%u name=%s slot=%u vnum=%u plus=%u gold=%lld",
								ch->GetPlayerID(), ch->GetName(), (unsigned int)wearCell, oldVnum,
								(unsigned int)plusLevel, (long long)ch->GetGold());
					}
					if (marketReplacement)
					{
						TPlayerBotRebuild& rebuild = s_mapPlayerBotRebuild[ch->GetPlayerID()];
						rebuild.dwUntil = dwNow + PLAYERBOT_REBUILD_MARKET_MS;
						rebuild.bSlot = wearCell;
						state.dwNextShoppingTime = 0;
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
						state.offlineShop.nextBrowse = 0;
#endif
						sys_log(0, "PLAYERBOT_MARKET: rebuild after a burn pid=%u name=%s slot=%u vnum=%u plus=%u gold=%lld",
								ch->GetPlayerID(), ch->GetName(), (unsigned int)wearCell, oldVnum,
								(unsigned int)plusLevel, (long long)ch->GetGold());
					}
				}
				sys_log(0, "PLAYERBOT_AI: refine %s pid=%u name=%s old_vnum=%u new_vnum=%u plus=%u scroll=%d materials=%s",
						success ? "SUCCESS" : (scrollCell >= 0 ? "FAILED_DOWNGRADED" : "FAILED_BURNED"),
						ch->GetPlayerID(), ch->GetName(), oldVnum, nextVnum, plusLevel + 1, scrollCell >= 0 ? 1 : 0,
						materials);
				++refinedCount;
			}
			else
			{
				sys_log(0, "PLAYERBOT_AI: refine SKIPPED pid=%u name=%s vnum=%u plus=%u materials=%s (requirements/state)",
						ch->GetPlayerID(), ch->GetName(), oldVnum, plusLevel, materials);
			}

			// Do not equip the result again between consecutive + levels.  Keep it
			// visibly in the inventory for the complete blacksmith session and let
			// the town state equip the final/best result once refining is finished.
		}

		return refinedCount > 0;
	}

	// Whether the next step of this worn piece can go under a scroll where the
	// bot stands: every test the field pass asks of a candidate, asked again of
	// the step it chose when the engine's equip window opens.
	bool CanPlayerBotTakeFieldScrollStep(LPCHARACTER ch, LPITEM item, int wearCell)
	{
		if (!ch || !item || item->GetRefinedVnum() == 0 || item->isLocked() || item->IsExchanging() ||
				!IsPlayerBotWornItemSound(ch, item, wearCell))
			return false;
		const BYTE plus = item->GetRefineLevel();
		// The target is PLAYERBOT_SCROLL_REFINE_MAX_PLUS here by construction:
		// this pass only runs with a scroll in the bag. A scroll-only weapon
		// (IsPlayerBotScrollOnlyWeapon) is taken at any plus and past the
		// floor, since it is never raised any other way; a weapon of the
		// operator's anvil table from its ceiling, where the blacksmith stops
		// (GetPlayerBotWeaponAnvilCeiling); everything else from +6.
		const bool scrollOnly = IsPlayerBotScrollOnlyWeapon(item);
		// The weapon in the hand or the armour on the back with nothing to
		// fall back on goes under a scroll at any plus too: Iwakura's Patch
		// 4, point 14 - "jesli bot decyduje sie na ulepszanie przy uzyciu
		// Zwoju Blogoslawienstwa ..., zasada posiadania kopii zapasowej jest
		// calkowicie ignorowana", and a bot with the scroll, the materials and
		// the yang "ma bezwzgledny obowiazek natychmiast podjac proby
		// ulepszenia". Here it waited for the next town visit and the
		// blacksmith, or for +6. So does a piece under his scroll rule.
		const bool atRisk = IsPlayerBotWornWeaponAtRisk(ch, item) || IsPlayerBotWornArmourAtRisk(ch, item) ||
				IsPlayerBotScrollRulePiece(ch, item);
		const int scrollFrom = scrollOnly || atRisk ? 0
				: IsPlayerBotAnvilTableWeapon(item)
					? GetPlayerBotWeaponAnvilCeiling(ch, item) : (int)PLAYERBOT_SCROLL_REFINE_MIN_PLUS;
		if ((!scrollOnly && ((int)plus < scrollFrom || !IsPlayerBotScrollStepAllowed(plus))) ||
				plus >= GetPlayerBotRefineTarget(ch, item))
			return false;
		// A step Iwakura's coin sends to the plain anvil waits for the
		// blacksmith rather than taking a scroll here.
		if (PlayerBotRisksPlainAnvil(ch, item))
			return false;
		if (!IsPlayerBotWearableAtLevel(ch, item->GetRefinedVnum()))
			return false;
		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());
		if (!recipe)
			return false;
		// A scroll this step can go under, not merely one in the bag: the War
		// God scroll stops at +4, and no scroll goes on a piece of
		// PLAYERBOT_SCROLL_FREE_GEAR_MAX_LEVEL or under.
		if (FindPlayerBotRefineScrollCellFor(ch, item, (int)recipe->prob) < 0)
			return false;
		// Off, refined and back on: the piece needs a place in the bag between.
		if (ch->GetEmptyInventory(item->GetSize()) < 0)
			return false;
		// The fee, the materials and the Biologist's reserve, as the blacksmith
		// pass asks them.
		return CanPlayerBotAttemptRefineItem(ch, item);
	}

	// Why a piece under the scroll rule has no step to take now, once a
	// minute for the whole population: what the recipe lacks (which the market
	// is asked for, CollectPlayerBotScrollRuleMissing), the fee, the room.
	void ReportPlayerBotScrollRuleWait(LPCHARACTER ch, LPITEM piece, DWORD dwNow)
	{
		if (!ch || !piece)
			return;
		std::map<DWORD, int> missing;
		CollectPlayerBotScrollRuleMissing(ch, missing);
		char lacking[96] = "none";
		size_t used = 0;
		for (std::map<DWORD, int>::const_iterator it = missing.begin();
				it != missing.end() && used + 24 < sizeof(lacking); ++it)
			used += snprintf(lacking + used, sizeof(lacking) - used, "%s%u:%d",
					used ? "," : "", (unsigned int)it->first, it->second);
		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(piece->GetRefineSet());
		PlayerBotLogThrottled("scroll_rule_wait", dwNow,
				"PLAYERBOT_AI: scroll rule waits pid=%u name=%s vnum=%u plus=%u scrolls=%d gold=%lld fee=%lld lacking=%s room=%d",
				ch->GetPlayerID(), ch->GetName(), piece->GetVnum(), (unsigned int)piece->GetRefineLevel(),
				CountPlayerBotSafeRefineScrolls(ch), (long long)ch->GetGold(),
				recipe ? (long long)ch->ComputeRefineFee(recipe->cost) : 0LL, lacking,
				ch->GetEmptyInventory(piece->GetSize()) >= 0 ? 1 : 0);
	}

	void ClearPlayerBotScrollStep(TPlayerBotAIState& state)
	{
		state.dwScrollStepItem = 0;
		state.dwScrollStepSince = 0;
		state.bScrollStepHolds = false;
	}

	// One step under the scroll for a worn piece: off, under the scroll the
	// blacksmith pass would put on the same step - the Dragon God from
	// PLAYERBOT_DRAGON_GOD_SCROLL_MIN_PLUS - and back on. What the anvil hands
	// back, NULL when nothing was attempted; equippedAgain says whether that
	// is worn again.
	LPITEM TakePlayerBotFieldScrollStep(LPCHARACTER ch, LPITEM piece, BYTE wear, bool ruleStep,
			bool& attempted, bool& equippedAgain)
	{
		attempted = false;
		equippedAgain = false;
		if (IsPlayerBotSidekickLockedItem(ch, piece))	// MT2009_PLUS_SIDEKICK_EQUIP_LOCK_V1
			return NULL;
		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(piece->GetRefineSet());
		const int scrollCell = FindPlayerBotRefineScrollCellFor(ch, piece, recipe ? (int)recipe->prob : 100);
		if (scrollCell < 0 || ch->GetEmptyInventory(piece->GetSize()) < 0)
			return NULL;
		// Off, refined, and back on: the engine will not touch a worn piece, and
		// the result is a new item in the same cell whatever the outcome.
		const DWORD oldVnum = piece->GetVnum();
		const DWORD nextVnum = piece->GetRefinedVnum();
		const BYTE plus = piece->GetRefineLevel();
		if (!ch->UnequipItem(piece) || piece->IsEquipped() || piece->GetWindow() != INVENTORY ||
				piece->GetCell() >= PLAYERBOT_BAG_CELLS)
			return NULL;
		const WORD cell = piece->GetCell();
		ch->SetRefineMode(scrollCell);
		attempted = ch->DoRefineWithScroll(piece);
		ch->ClearRefineMode();
		LPITEM after = ch->GetInventoryItem(cell);
		// The outcome is read off the cell before the piece goes back on. It
		// was CountSpecifyItem after the re-equip, which counts the bag only,
		// so a success worn again was logged FAILED_DOWNGRADED and cost a
		// level of mood (log.log: REFINE SUCCESS in the same second).
		const bool success = attempted && after && after->GetVnum() == nextVnum;
		if (after)
			equippedAgain = PlayerBotEquipItem(ch, after) && after->IsEquipped();
		if (attempted)
		{
			if (success)
			{
				BroadcastPlayerBotRefineSuccess(ch, nextVnum, (int)plus + 1);
				NotePlayerBotMoodRefine(ch, (int)plus + 1);
			}
			else
				NotePlayerBotMoodRefineFailure(ch, (int)plus + 1, "downgraded");
			sys_log(0, "PLAYERBOT_AI: refine %s pid=%u name=%s old_vnum=%u new_vnum=%u plus=%u scroll=1 place=field wear=%u rule=%d",
					success ? "SUCCESS" : "FAILED_DOWNGRADED", ch->GetPlayerID(), ch->GetName(),
					oldVnum, nextVnum, (unsigned int)plus + 1, (unsigned int)wear, ruleStep ? 1 : 0);
		}
		return attempted ? after : NULL;
	}

	// The Blessing Scroll works from the bag, wherever the bot stands - a
	// player uses one in the field, not at the anvil - so a bot carrying one
	// does not wait for its next town visit to put it to use. In a quiet
	// moment it takes the lowest worn piece at +6 or better, pays the table's
	// fee and materials, and refines it under the scroll: on failure the piece
	// comes back one level down instead of not at all. Never on a step under
	// the operator's SCROLL_FROM (playerbot_config.h).
	//
	// A quiet moment is one without a fight, and a bot in a dense hunting
	// ground has none: its action is a fight from one monster to the next,
	// and this pass - at the bottom of the tick, behind every errand - never
	// ran for it. That is the sura of seventy at +5 with thirty-seven scrolls
	// and every material (Iwakura's Community Patch 5, point 4): its rule
	// waited for a town visit, which a frontier bot makes when its visit runs
	// out, forty minutes and more apart. So a step of Iwakura's scroll rule
	// (IsPlayerBotScrollRulePiece) stops the fight for it: the bot drops its
	// target and stands until the engine's equip window opens - a second and
	// a half after its last blow - and then takes up to
	// PLAYERBOT_SCROLL_RULE_STEPS_PER_PASS steps in that window
	// (playerbot_refine_rules::DecideScrollStep). mayHoldFight is the tick's
	// word that no person counts on the bot's blows. True while the pass
	// claims the tick: a hold, or a step taken.
	bool ManagePlayerBotScrollRefine(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow, bool mayHoldFight)
	{
		if (!ch || !ch->IsItemLoaded() || IsPlayerBotGearFrozen(ch) || ch->IsDead() || state.bVisitingShop ||
				state.bRecoveringAfterDeath || state.bTacticalRetreat)
		{
			ClearPlayerBotScrollStep(state);
			return false;
		}
		// A hold of this pass's own counts as the fight it stopped.
		const bool fighting = state.bCurrentAction == BOT_ACTION_FIGHT || state.bScrollStepHolds;
		// Never where a pull or a lure counts on the bot's blows.
		const bool mayHold = mayHoldFight && !state.bMultiPullActive && state.dwLurePlayerPID == 0;

		// "1. Bronie, 2. Zbroje, 3. Tarcze, 4. Helmy" (Community Patch 1) -
		// the order every other refine pass already used, and this one did not.
		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		LPITEM best = NULL;
		BYTE bestWear = 0;
		if (state.dwScrollStepItem != 0)
		{
			// The step chosen on an earlier tick, still worn where it was.
			for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]) && !best; ++i)
			{
				LPITEM worn = ch->GetWear(wearSlots[i]);
				if (worn && worn->GetID() == state.dwScrollStepItem)
				{
					best = worn;
					bestWear = wearSlots[i];
				}
			}
			if (!best)
			{
				ClearPlayerBotScrollStep(state);
				return false;
			}
		}
		else
		{
			if (dwNow < state.dwNextScrollRefineTime)
				return false;
			// In a fight only the rule's step is looked for, and only by a bot
			// that may stop for it; the rest wait for a quiet moment, as ever.
			if (fighting && !mayHold)
				return false;
			state.dwNextScrollRefineTime = dwNow + PLAYERBOT_SCROLL_REFINE_INTERVAL;
			if (FindPlayerBotRefineScrollCell(ch, 0) < 0)
				return false;
			// The rule's pieces first - the weapon in the hand, then the armour
			// on the back - ahead of a lower piece that would spend the scrolls
			// the rule counts on.
			LPITEM hand = ch->GetWear(WEAR_WEAPON);
			LPITEM body = ch->GetWear(WEAR_BODY);
			if (IsPlayerBotScrollRuleWeapon(ch, hand))
			{
				if (CanPlayerBotTakeFieldScrollStep(ch, hand, WEAR_WEAPON))
				{
					best = hand;
					bestWear = WEAR_WEAPON;
				}
				else
					ReportPlayerBotScrollRuleWait(ch, hand, dwNow);
			}
			if (!best && IsPlayerBotScrollRuleArmour(ch, body))
			{
				if (CanPlayerBotTakeFieldScrollStep(ch, body, WEAR_BODY))
				{
					best = body;
					bestWear = WEAR_BODY;
				}
				else
					ReportPlayerBotScrollRuleWait(ch, body, dwNow);
			}
			for (size_t i = 0; !best && !fighting && i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
			{
				LPITEM item = ch->GetWear(wearSlots[i]);
				if (!item || !CanPlayerBotTakeFieldScrollStep(ch, item, wearSlots[i]))
					continue;
				if (!best || item->GetRefineLevel() < best->GetRefineLevel())
				{
					best = item;
					bestWear = wearSlots[i];
				}
			}
			if (!best)
				return false;
			state.dwScrollStepItem = best->GetID();
			state.dwScrollStepSince = dwNow;
			state.bScrollStepHolds = false;
		}

		// Asked while the piece is still worn.
		const bool ruleStep = IsPlayerBotScrollRulePiece(ch, best);
		const int hpPercent = ch->GetMaxHP() > 0 ? (int)((long long)ch->GetHP() * 100 / ch->GetMaxHP()) : 100;
		switch (playerbot_refine_rules::DecideScrollStep(IsPlayerBotEquipWindowShut(ch, state), fighting,
					ruleStep && mayHold, dwNow - state.dwScrollStepSince, hpPercent))
		{
			case playerbot_refine_rules::SCROLL_STEP_WAIT:
				return false;
			case playerbot_refine_rules::SCROLL_STEP_HOLD:
				if (!state.bScrollStepHolds)
					PlayerBotLogThrottled("scroll_rule_hold", dwNow,
							"PLAYERBOT_AI: scroll rule stops the fight for the equip window pid=%u name=%s vnum=%u plus=%u wear=%u scrolls=%d",
							ch->GetPlayerID(), ch->GetName(), best->GetVnum(), (unsigned int)best->GetRefineLevel(),
							(unsigned int)bestWear, CountPlayerBotSafeRefineScrolls(ch));
				state.bScrollStepHolds = true;
				// No blow, from this pass or the light one between: the window is
				// a second and a half after the bot's own last swing or cast.
				state.dwTargetVID = 0;
				ch->SetVictim(NULL);
				if (ch->IsStateMove())
					ch->Stop();
				SetPlayerBotAction(state, BOT_ACTION_REFINE, dwNow);
				return true;
			case playerbot_refine_rules::SCROLL_STEP_DROP:
				if (ruleStep)
					PlayerBotLogThrottled("scroll_rule_drop", dwNow,
							"PLAYERBOT_AI: scroll rule step put off pid=%u name=%s vnum=%u plus=%u waited_ms=%u hp=%d fighting=%d may_hold=%d",
							ch->GetPlayerID(), ch->GetName(), best->GetVnum(), (unsigned int)best->GetRefineLevel(),
							(unsigned int)(dwNow - state.dwScrollStepSince), hpPercent, fighting ? 1 : 0, mayHold ? 1 : 0);
				ClearPlayerBotScrollStep(state);
				return false;
			case playerbot_refine_rules::SCROLL_STEP_NOW:
			default:
				break;
		}
		ClearPlayerBotScrollStep(state);

		// The step, and while the rule still has the piece, the next ones in
		// the same open window: a player clicks the scroll until it lands.
		const int steps = ruleStep ? PLAYERBOT_SCROLL_RULE_STEPS_PER_PASS : 1;
		bool any = false;
		for (int n = 0; n < steps && best; ++n)
		{
			if (!CanPlayerBotTakeFieldScrollStep(ch, best, bestWear) ||
					(n > 0 && !IsPlayerBotScrollRulePiece(ch, best)))
				break;
			bool attempted = false, equippedAgain = false;
			LPITEM after = TakePlayerBotFieldScrollStep(ch, best, bestWear, ruleStep, attempted, equippedAgain);
			any = any || attempted;
			if (!attempted || !equippedAgain)
				break;
			best = after;
		}
		// The rule's next steps soon, not in PLAYERBOT_SCROLL_REFINE_INTERVAL.
		if (any && ruleStep && PlayerBotHasScrollRuleWork(ch))
			state.dwNextScrollRefineTime = dwNow + PLAYERBOT_SCROLL_RULE_STEP_INTERVAL;
		return any;
	}

	// MT2009_PLUS_MARKET_V3, point 8: at the General Store a bot gives its
	// Siano for Red Potions (D), PLAYERBOT_HAY_POTIONS a bundle, while its
	// belt is under PLAYERBOT_POTION_FILL_RED and the bag has room for them -
	// the room counted as the potion purchase below counts it, since
	// AutoGiveItem puts what does not fit on the ground. Whatever is left of
	// the hay the store then buys (IsPlayerBotJunkItem). The potions given.
	DWORD ExchangePlayerBotHay(LPCHARACTER ch, size_t redCount)
	{
		if (!ch || redCount >= PLAYERBOT_POTION_FILL_RED)
			return 0;
		// MT2009_PLUS_BOT_HORSE_HAY_V1: the horse's share is never changed.
		const int hay = (int)ch->CountSpecifyItem(PLAYERBOT_HAY_VNUM) - PLAYERBOT_HAY_KEEP;
		if (hay <= 0)
			return 0;
		const int freeCells = std::max(0, ch->GetEmptyInventory(1) < 0 ? 0 : CountPlayerBotFreeInventoryCells(ch));
		const int held = (int)ch->CountSpecifyItem(PLAYERBOT_HAY_POTION_VNUM);
		const int room = freeCells * 200 + (200 - held % 200) % 200;
		const int wanted = (int)((PLAYERBOT_POTION_FILL_RED - redCount + PLAYERBOT_HAY_POTIONS - 1) / PLAYERBOT_HAY_POTIONS);
		const int bundles = std::min(std::min(hay, wanted), room / PLAYERBOT_HAY_POTIONS);
		if (bundles <= 0)
			return 0;
		ch->RemoveSpecifyItem(PLAYERBOT_HAY_VNUM, bundles);
		ch->AutoGiveItem(PLAYERBOT_HAY_POTION_VNUM, bundles * PLAYERBOT_HAY_POTIONS);
		sys_log(0, "PLAYERBOT_MARKET: hay exchanged pid=%u name=%s hay=%d potions=%d had_red=%u hay_left=%d",
				ch->GetPlayerID(), ch->GetName(), bundles, bundles * PLAYERBOT_HAY_POTIONS,
				(unsigned int)redCount, hay - bundles + PLAYERBOT_HAY_KEEP);
		return (DWORD)(bundles * PLAYERBOT_HAY_POTIONS);
	}

	bool ManagePlayerBotMiscMerchant(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;

		CompactPlayerBotPotionStacks(ch);
		SellPlayerBotExcessPotions(ch);

		// Count red and blue potions
		size_t redCount = 0;
		size_t blueCount = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;

			const DWORD vnum = item->GetVnum();
			if (vnum == 27001 || vnum == 27002 || vnum == 27003 || vnum == 27051)
				redCount += item->GetCount();
			else if (vnum == 27004 || vnum == 27005 || vnum == 27006 || vnum == 27052)
				blueCount += item->GetCount();
		}

		// The hay first, for potions; the rest of it is junk below.
		redCount += ExchangePlayerBotHay(ch, redCount);

		// Miscellaneous loot belongs to Handlarka. Weapons and wearable equipment
		// are deliberately left for their own specialist merchants.
		SellPlayerBotJunkAtMerchant(ch, BOT_MERCHANT_MISC, "misc_merchant");

		// Economical potion purchase at Handlarka.  Refining is intentionally
		// performed in the separate blacksmith phase after the bot walks there.
		const bool isMage = (ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA);
		const BYTE botLvl = ch->GetLevel();
		// What this visit actually bought. The purchase used to be silent - the
		// only line was "misc merchant visit" - so "do the bots really buy the
		// big stock?" could only be answered by querying the database, which is
		// not a question an operator should have to take that far.
		DWORD boughtRed = 0, boughtBlue = 0;

		if (botLvl <= 10)
		{
			if (redCount < 30 && ch->GetGold() >= 300)
			{
				PlayerBotChangeGold(ch, -240);
				ch->AutoGiveItem(27001, 30); // Red Potion (S) 30x
				boughtRed += 30;
			}
			if (isMage && blueCount < 20 && ch->GetGold() >= 400)
			{
				PlayerBotChangeGold(ch, -360);
				ch->AutoGiveItem(27004, 15); // Blue Potion (S) 15x
				boughtBlue += 15;
			}
		}
		else
		{
			// Unit prices are the ones the old fixed purchases implied: 20 yang for
			// a Red Potion (M), 32 for a Blue Potion (M).
			const DWORD RED_TARGET = (DWORD)PLAYERBOT_POTION_FILL_RED;
			const DWORD BLUE_TARGET = (DWORD)PLAYERBOT_POTION_FILL_BLUE;
			// From forty the bot buys the big (D) potions, not the medium (S).
			// A level-47 bot heals in the hundreds per hit and a medium potion
			// is a sip; "na tych poziomach to juz duze potki u handlarki", as
			// the Discord put it. The unit prices follow the same rule as the
			// medium ones - what the old fixed purchases implied - scaled by the
			// proto's sell-price ratio (160/96, 480/288) and rounded up.
			const bool bBig = botLvl >= PLAYERBOT_BIG_POTION_MIN_LEVEL;
			const DWORD RED_VNUM = bBig ? 27003 : 27002;
			const DWORD BLUE_VNUM = bBig ? 27006 : 27005;
			const DWORD RED_UNIT = bBig ? 40 : 20;
			const DWORD BLUE_UNIT = bBig ? 64 : 32;
			// And never more than the bag can hold, because AutoGiveItem does not
			// refuse a full one - it fills whatever stack has room and puts the
			// rest on the ground at the bot's feet, paid for. A stack is 200. The
			// estimate below counts the headroom of one partial stack plus every
			// free cell, which is at most what the engine will find, never more.
			const int freeCells = std::max(0, ch->GetEmptyInventory(1) < 0 ? 0 :
					CountPlayerBotFreeInventoryCells(ch));
			const DWORD redRoom = (DWORD)freeCells * 200 + (200 - redCount % 200) % 200;
			const DWORD blueRoom = (DWORD)freeCells * 200 + (200 - blueCount % 200) % 200;
			// Standing at the merchant already: fill the belt right up whatever the
			// level, because this costs nothing extra. The decision to make the
			// trip at all lives in NeedsPlayerBotPotions and is far stricter.
			// Never spend more than half the purse, so shopping can't leave the
			// bot unable to afford a refine.
			if (redCount < RED_TARGET && ch->GetGold() >= 1200)
			{
				const DWORD want = (DWORD)(RED_TARGET - redCount);
				const DWORD affordable = (DWORD)(ch->GetGold() / 2 / RED_UNIT);
				DWORD buy = want < affordable ? want : affordable;
				if (buy > redRoom)
					buy = redRoom;
				if (buy > 0)
				{
					PlayerBotChangeGold(ch, -(int)(buy * RED_UNIT));
					ch->AutoGiveItem(RED_VNUM, buy);
					boughtRed += buy;
				}
			}
			// Skills spend SP continuously, so a warrior wants a reserve too. It
			// simply must never be the thing that forbids travelling.
			if (blueCount < BLUE_TARGET && ch->GetGold() >= 1200)
			{
				const DWORD want = (DWORD)(BLUE_TARGET - blueCount);
				const DWORD affordable = (DWORD)(ch->GetGold() / 2 / BLUE_UNIT);
				DWORD buy = want < affordable ? want : affordable;
				if (buy > blueRoom)
					buy = blueRoom;
				if (buy > 0)
				{
					PlayerBotChangeGold(ch, -(int)(buy * BLUE_UNIT));
					ch->AutoGiveItem(BLUE_VNUM, buy);
					boughtBlue += buy;
				}
			}
		}

		if (boughtRed != 0 || boughtBlue != 0)
			sys_log(0, "PLAYERBOT_RESTOCK: bought pid=%u name=%s level=%u red=%u blue=%u "
					"had_red=%u had_blue=%u gold_left=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(),
					(unsigned int)boughtRed, (unsigned int)boughtBlue,
					(unsigned int)redCount, (unsigned int)blueCount,
					(int)(ch->GetGold() / 1000));

		// Even the level-one shoes add movement speed. Missing footwear is therefore
		// a progression problem, not cosmetic equipment.
		if (NeedsPlayerBotProgressionBoots(ch))
			BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionBootsVnum(ch), "boots");

		return true;
	}

	bool ManagePlayerBotWeaponMerchant(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		const bool sold = SellPlayerBotJunkAtMerchant(
				ch, BOT_MERCHANT_WEAPON, "weapon_merchant");
		bool bought = false;
		const bool isArcher = ch->GetJob() == JOB_ASSASSIN && ch->GetSkillGroup() == 2;
		// A missing weapon is essential, so restore the cheap functional weapon
		// first. With a bow already equipped, ammunition takes priority over a
		// level-tier upgrade: buying a better bow and leaving zero Yang for arrows
		// merely creates a better-equipped idle bot.
		// Not while the market is being looked at for a finished one after a
		// burn (IsPlayerBotRebuildingFromMarket; Patch 4, point 7).
		if (!ch->GetWear(WEAR_WEAPON) && !IsPlayerBotRebuildingFromMarket(ch, WEAR_WEAPON))
			bought = BuyPlayerBotEmergencyWeapon(ch) || bought;
		if (isArcher)
			bought = BuyPlayerBotArrowsAtMerchant(ch) || bought;
		// ...and a stone weapon, the dagger of its level, when the bag has none.
		if (isArcher && !FindPlayerBotStoneWeapon(ch, true))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionStoneWeaponVnum(ch), "stone dagger") || bought;
		// The ladder's weapon waits for the market after a burn too: the gate
		// stood on the emergency weapon alone, and the ladder bought the
		// merchant's plain piece the protocol was written to keep off (B13 of
		// Iwakura's audit of 26 September).
		if (NeedsPlayerBotProgressionWeapon(ch) && !IsPlayerBotRebuildingFromMarket(ch, WEAR_WEAPON) &&
				(!isArcher || CountPlayerBotArrows(ch) >= PLAYERBOT_ARROW_RESTOCK_THRESHOLD))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionWeaponVnum(ch), "weapon") || bought;
		// A bot of thirty whose only weapon is one of the tenth level or under
		// buys one over the line (Community Patch 5, point 2): the ladder
		// above names a tier the merchant stocks only at levels 30-31 and
		// 36-39, so from 32 and from 40 it bought nothing and the level-one
		// sword stayed in the hand. The equipment pass puts the new one on,
		// the old one scoring nothing against it.
		if (NeedsPlayerBotProperWeapon(ch) && !IsPlayerBotRebuildingFromMarket(ch, WEAR_WEAPON) &&
				(!isArcher || CountPlayerBotArrows(ch) >= PLAYERBOT_ARROW_RESTOCK_THRESHOLD))
			bought = BuyPlayerBotProperWeapon(ch) || bought;
		// The weapon in the hand held at a step that can burn it for want of a
		// backup and a scroll (R8 of Iwakura's audit): the merchant's copy is
		// that backup, bought only where the step can still be paid after it.
		DWORD backupVnum = 0;
		long long backupPrice = 0;
		if (NeedsPlayerBotBackupWeapon(ch) && FindPlayerBotBackupWeaponOffer(ch, ch->GetWear(WEAR_WEAPON),
					backupVnum, backupPrice))
		{
			LPITEM worn = ch->GetWear(WEAR_WEAPON);
			const TRefineTable* recipe = worn ? CRefineManager::instance().GetRefineRecipe(worn->GetRefineSet()) : NULL;
			if (recipe && (long long)ch->GetGold() - GetPlayerBotReservedGold(ch) - backupPrice >=
						(long long)ch->ComputeRefineFee(recipe->cost) &&
					BuyPlayerBotProgressionGear(ch, backupVnum, "backup weapon"))
			{
				bought = true;
				// The step waits for the anvil's next visit; the copy is kept
				// from the merchant from now on (IsPlayerBotKeptBackupWeapon).
				GetPlayerBotBackupWeaponID(ch, true);
			}
		}
		return sold || bought;
	}

	bool ManagePlayerBotArmorMerchant(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		const bool sold = SellPlayerBotJunkAtMerchant(
				ch, BOT_MERCHANT_ARMOR, "armor_merchant");
		// The exact ladder tier first; when the merchant does not stock it -
		// which is every tier below the bot except the three the shop carries,
		// and every tier above level 26 - the best piece it does stock, so a
		// naked slot is filled and the blacksmith can raise it to +6.
		bool bought = false;
		// Not the slot the market is being looked at for after a burn
		// (IsPlayerBotRebuildingFromMarket; Patch 4, point 7).
		if (NeedsPlayerBotProgressionArmor(ch) && !IsPlayerBotRebuildingFromMarket(ch, WEAR_BODY))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionArmorVnum(ch), "armor") ||
				BuyPlayerBotBestMerchantSlotGear(ch, WEAR_BODY, "armor") || bought;
		if (NeedsPlayerBotProgressionShield(ch) && !IsPlayerBotRebuildingFromMarket(ch, WEAR_SHIELD))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionShieldVnum(ch), "shield") ||
				BuyPlayerBotBestMerchantSlotGear(ch, WEAR_SHIELD, "shield") || bought;
		if (NeedsPlayerBotProgressionHelmet(ch))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionHelmetVnum(ch), "helmet") ||
				BuyPlayerBotBestMerchantSlotGear(ch, WEAR_HEAD, "helmet") || bought;
		// The armour on the back held off its next step for want of a spare:
		// the best body armour this merchant stocks for the bot is that spare.
		// Bought only where the step can still be paid after it, and never the
		// copy BuyPlayerBotBestMerchantSlotGear refuses as no better than what
		// the bot wears - that refusal is for an upgrade, and this is not one.
		if (NeedsPlayerBotBackupArmour(ch))
		{
			const DWORD vnum = FindPlayerBotBestMerchantSlotVnum(ch, WEAR_BODY);
			long long price = 0;
			LPITEM worn = ch->GetWear(WEAR_BODY);
			const TRefineTable* recipe = worn ? CRefineManager::instance().GetRefineRecipe(worn->GetRefineSet()) : NULL;
			if (vnum != 0 && recipe && FindPlayerBotMerchantOffer(vnum, &price) &&
					ch->GetGold() - GetPlayerBotReservedGold(ch) - std::max<long long>(100, price) >=
						ch->ComputeRefineFee(recipe->cost) &&
					BuyPlayerBotProgressionGear(ch, vnum, "backup armor"))
			{
				bought = true;
				// The step waits for the anvil's next visit; the spare is kept
				// from the merchant from now on (IsPlayerBotKeptBackupArmour).
				GetPlayerBotBackupArmourID(ch, true);
			}
		}
		// The three slots nothing ever filled. A bot wore a bracelet, a necklace
		// or an earring only when one happened to drop for it, because no ladder
		// asked for them - so most of them went their whole lives with three
		// empty slots on the character sheet.
		if (NeedsPlayerBotProgressionWrist(ch))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionWristVnum(ch), "wrist") || bought;
		if (NeedsPlayerBotProgressionNecklace(ch))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionNecklaceVnum(ch), "necklace") || bought;
		if (NeedsPlayerBotProgressionEarring(ch))
			bought = BuyPlayerBotProgressionGear(ch,
					GetPlayerBotProgressionEarringVnum(ch), "earring") || bought;
		return sold || bought;
	}

	// The purse and the bag against the next step of this piece: the fee over
	// the reserve, and every material with the Biologist's share left alone.
	// Asked of an offline counter's line too (BotOfflineUnwantedLine), which
	// is why it takes the item and not its place.
	bool CanPlayerBotPayRefineStep(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return false;
		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(
				item->GetRefineSet());
		if (!recipe || ch->GetGold() - GetPlayerBotReservedGold(ch) <
				ch->ComputeRefineFee(recipe->cost))
			return false;

		for (int i = 0; i < recipe->material_count; ++i)
		{
			// What the Biologist is still owed is not the anvil's: an Orc Tooth
			// goes to him first and into a recipe after (15 September).
			if (ch->CountSpecifyItem(recipe->materials[i].vnum) -
					GetPlayerBotBiologistReserve(ch, recipe->materials[i].vnum) < recipe->materials[i].count)
				return false;
		}
		return true;
	}

	bool CanPlayerBotAttemptRefineItem(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetRefinedVnum() == 0 ||
				item->GetRefineLevel() >= GetPlayerBotRefineTarget(ch, item))
			return false;
		// What a companion's owner put on is the owner's to refine: a burn at
		// the companion's anvil would lose the piece the owner chose.
		if (IsPlayerBotSidekickPinned(ch, item))
			return false;
		// A weapon for the tenth level or under is no refine for a bot of
		// thirty, worn or not (Community Patch 5, point 2): it is what the bot
		// wears only until something over the line can be had, and the yang
		// is that weapon's. The Archer's stone dagger is a tool with a rule of
		// its own (IsPlayerBotArcherStoneWeapon), bought by level where the
		// merchant has one.
		if (IsPlayerBotLowWeaponFor(ch, item) && !IsPlayerBotArcherStoneWeapon(ch, item))
			return false;
		if (!CanPlayerBotPayRefineStep(ch, item))
			return false;
		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(
				item->GetRefineSet());
		if (!recipe)
			return false;
		// A weapon refined only under a scroll is no errand without one: the
		// planner asks this before it sends a bot to the blacksmith. The
		// class's own level-30 weapon under +6 is, scroll or no scroll
		// (IsPlayerBotLevel30UnderFloor).
		const bool scrollOnly = IsPlayerBotScrollOnlyWeapon(item);
		if (scrollOnly && !IsPlayerBotLevel30UnderFloor(ch, item) &&
				FindPlayerBotRefineScrollCellFor(ch, item, (int)recipe->prob) < 0)
			return false;
		// Nor a weapon of the operator's anvil table at its ceiling: the step is
		// a scroll's (GetPlayerBotWeaponAnvilCeiling), and the blacksmith pass
		// would only say it waits - once the ceiling came down to +6 for every
		// weapon from level thirty, that was a walk to town for most bots of
		// thirty and up. A cheap level-30 roll is an errand still: the pass may
		// gamble it at the plain anvil (IsPlayerBotCheapLevel30Roll).
		const bool scrollStepAllowed = IsPlayerBotScrollStepAllowed(item->GetRefineLevel());
		if (!scrollOnly && scrollStepAllowed && IsPlayerBotAnvilTableWeapon(item) &&
				!IsPlayerBotCheapLevel30Roll(item) &&
				(int)item->GetRefineLevel() >= GetPlayerBotWeaponAnvilCeiling(ch, item) &&
				FindPlayerBotRefineScrollCellFor(ch, item, (int)recipe->prob) < 0)
		{
			PlayerBotLogThrottled("refine_weapon_ceiling_plan", get_dword_time(),
					"PLAYERBOT_AI: weapon waits for a scroll pid=%u name=%s vnum=%u level=%d plus=%u avg=%ld ceiling=%d prob=%d where=plan",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum(), item->GetLevelLimit(),
					(unsigned int)item->GetRefineLevel(), SumPlayerBotItemLines(item, APPLY_NORMAL_HIT_DAMAGE_BONUS),
					GetPlayerBotWeaponAnvilCeiling(ch, item), (int)recipe->prob);
			return false;
		}
		// Nor a prize piece at odds the blacksmith pass will not take without a
		// scroll (PLAYERBOT_PRIZE_SAFE_REFINE_PROB) and no scroll for the step:
		// the pass holds it unless Iwakura's coin sends it to the plain anvil,
		// and the planner has to say the same or it sends the bot for nothing.
		if (!scrollOnly && scrollStepAllowed && recipe->prob < PLAYERBOT_PRIZE_SAFE_REFINE_PROB &&
				!IsPlayerBotAnvilTableWeapon(item) && !IsPlayerBotLevel30UnderFloor(ch, item) &&
				!IsPlayerBotScrollFreeGear(item) && IsPlayerBotPrizeItem(item) &&
				!PlayerBotRisksPlainAnvil(ch, item) &&
				FindPlayerBotRefineScrollCellFor(ch, item, (int)recipe->prob) < 0)
			return false;
		// Nor is the weapon in the hand at a step that can burn it with nothing
		// to fall back on (IsPlayerBotWornWeaponAtRisk). Under the operator's
		// SCROLL_FROM no scroll may go on the step, and the anvil's odds stand.
		if (scrollStepAllowed &&
				(IsPlayerBotWornWeaponAtRisk(ch, item) || IsPlayerBotWornArmourAtRisk(ch, item)) &&
				FindPlayerBotRefineScrollCellFor(ch, item, (int)recipe->prob) < 0)
		{
			PlayerBotLogThrottled("refine_hand_weapon", get_dword_time(),
					"PLAYERBOT_AI: refine held, the only weapon or armour and no scroll pid=%u name=%s vnum=%u plus=%u prob=%d level=%u",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)item->GetRefineLevel(),
					(int)recipe->prob, (unsigned int)ch->GetLevel());
			return false;
		}
		return true;
	}

	// The armour on the back held off a step it is meant to take (below its
	// refine target, the fee and the materials in hand) only for want of a
	// spare: no scroll would take the step instead, and the operator's
	// SCROLL_FROM leaves it the anvil's. What the armour merchant answers with
	// a spare of its own (ManagePlayerBotArmorMerchant), and what sends a bot
	// to that merchant (StartPlayerBotTownVisit).
	bool NeedsPlayerBotBackupArmour(LPCHARACTER ch)
	{
		LPITEM worn = ch ? ch->GetWear(WEAR_BODY) : NULL;
		if (!worn || !IsPlayerBotWornArmourAtRisk(ch, worn) ||
				worn->GetRefineLevel() >= GetPlayerBotRefineTarget(ch, worn) ||
				!IsPlayerBotScrollStepAllowed(worn->GetRefineLevel()) ||
				!CanPlayerBotPayRefineStep(ch, worn))
			return false;
		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(worn->GetRefineSet());
		return recipe && FindPlayerBotRefineScrollCellFor(ch, worn, (int)recipe->prob) < 0;
	}

	// Its weapon twin (R8 of Iwakura's audit): the weapon in the hand held at a
	// step that can burn it (IsPlayerBotWornWeaponAtRisk - past
	// PLAYERBOT_MERCHANT_WEAPON_RISK_PLUS for a merchant's weapon) only for want
	// of a backup, with no scroll for the step, and a merchant that sells one
	// (FindPlayerBotBackupWeaponOffer). Gear of
	// PLAYERBOT_SCROLL_FREE_GEAR_MAX_LEVEL or under never takes a scroll, and no
	// backup weapon was ever bought, so CanPlayerBotAttemptRefineItem held such
	// a weapon at +4 for good. The weapon merchant answers it
	// (ManagePlayerBotWeaponMerchant), and a town visit takes the merchant in
	// for it (StartPlayerBotTownVisit).
	bool NeedsPlayerBotBackupWeapon(LPCHARACTER ch)
	{
		LPITEM worn = ch ? ch->GetWear(WEAR_WEAPON) : NULL;
		// A weapon for the tenth level or under in the hand of a bot of thirty
		// is replaced, not backed up (NeedsPlayerBotProperWeapon).
		if (!worn || worn->GetType() != ITEM_WEAPON || IsPlayerBotLowWeaponFor(ch, worn) ||
				!IsPlayerBotWornWeaponAtRisk(ch, worn) ||
				worn->GetRefineLevel() >= GetPlayerBotRefineTarget(ch, worn) ||
				!IsPlayerBotScrollStepAllowed(worn->GetRefineLevel()) ||
				!CanPlayerBotPayRefineStep(ch, worn))
			return false;
		const TRefineTable* recipe = CRefineManager::instance().GetRefineRecipe(worn->GetRefineSet());
		if (!recipe || FindPlayerBotRefineScrollCellFor(ch, worn, (int)recipe->prob) >= 0)
			return false;
		// The merchant sells it only where the step can still be paid after it
		// (ManagePlayerBotWeaponMerchant): a need the purchase would refuse is
		// a walk to the merchant at every visit for nothing.
		DWORD vnum = 0;
		long long price = 0;
		return FindPlayerBotBackupWeaponOffer(ch, worn, vnum, price) &&
				(long long)ch->GetGold() - GetPlayerBotReservedGold(ch) - price >=
					(long long)ch->ComputeRefineFee(recipe->cost);
	}

	// MT2009_PLUS_BOT_TOWN_SPREAD_V1: a bot whose last visit to the anvil
	// refined nothing although this said it would (the engine refused every
	// step - a material short, a state the planner does not ask about) is no
	// refiner for a while: without it the planner sent it straight back, and it
	// stood at the anvil for good, one visit every fifteen to twenty seconds.
	// Set by the blacksmith's wait (playerbot_town.h).
	const DWORD PLAYERBOT_ANVIL_FUTILE_MIN_MS = 15 * 60 * 1000;
	const DWORD PLAYERBOT_ANVIL_FUTILE_MAX_MS = 25 * 60 * 1000;
	std::map<DWORD, DWORD> s_mapPlayerBotAnvilFutileUntil;

	bool IsPlayerBotAnvilFutile(DWORD pid, DWORD dwNow)
	{
		std::map<DWORD, DWORD>::iterator it = s_mapPlayerBotAnvilFutileUntil.find(pid);
		if (it == s_mapPlayerBotAnvilFutileUntil.end())
			return false;
		if ((int)(dwNow - it->second) >= 0)
		{
			s_mapPlayerBotAnvilFutileUntil.erase(it);
			return false;
		}
		return true;
	}

	bool HasPlayerBotRefineOpportunity(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		if (IsPlayerBotAnvilFutile(ch->GetPlayerID(), get_dword_time()))
			return false;
		// The REFINE weight under neutral closes the anvil for a share of the
		// bots (IsPlayerBotWeightGateOpen). This is the one question the
		// planner, the town trip, the blacksmith's stop and the cross-map
		// return all ask; a gambler's session is its own errand and is asked
		// beside it, and a bot already at the anvil refines what it can.
		if (!IsPlayerBotWeightGateOpen(ch->GetPlayerID(), PLAYERBOT_WEIGHT_REFINE,
				PLAYERBOT_WEIGHT_GATE_SALT_REFINE, get_dword_time()))
			return false;

		// MT2009_PLUS_AWAKENING_V1: the ritual is a reason for the anvil too.
		if (HasPlayerBotAwakeningRitual(ch))
			return true;
		// MT2009_PLUS_HEAVEN_OIL_V1 (Autor: Digi Rasta): and a soul stone step with everything in the
		// bag (resting after any attempt, so a refusal is no loop).
		if (HasPlayerBotSoulStoneStep(ch))
			return true;

		const BYTE wearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		// A slot whose anvil waits for a finished piece off a counter is no
		// reason either (IsPlayerBotRefineHeldForReadyGear).
		const DWORD dwNow = get_dword_time();
		for (size_t i = 0; i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
		{
			LPITEM item = ch->GetWear(wearSlots[i]);
			if (CanPlayerBotAttemptRefineItem(ch, item) &&
					!IsPlayerBotRefineHeldForReadyGear(ch, item, wearSlots[i], dwNow))
				return true;
		}

		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			// The same test the refining pass applies, not a looser one: a
			// promise the executor will refuse is a walk to town for nothing.
			LPITEM item = ch->GetInventoryItem(cell);
			if (IsPlayerBotRefineBagCandidate(ch, item) &&
					CanPlayerBotAttemptRefineItem(ch, item) &&
					!IsPlayerBotRefineHeldForReadyGear(ch, item, -1, dwNow))
				return true;
		}
		return false;
	}

	bool HasPlayerBotPriorityRefineOpportunity(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		if (IsPlayerBotAnvilFutile(ch->GetPlayerID(), get_dword_time()))
			return false;
		if (!IsPlayerBotWeightGateOpen(ch->GetPlayerID(), PLAYERBOT_WEIGHT_REFINE,
				PLAYERBOT_WEIGHT_GATE_SALT_REFINE, get_dword_time()))
			return false;

		// Cross-map blacksmith trips are reserved for currently worn essentials.
		// A routine accessory or spare can wait until the next normal M1 visit, but
		// a weapon/body/shield/helmet/boots upgrade should not sit unused in M2/M3.
		const BYTE coreWearSlots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD, WEAR_FOOTS
		};
		for (size_t i = 0; i < sizeof(coreWearSlots) / sizeof(coreWearSlots[0]); ++i)
		{
			LPITEM item = ch->GetWear(coreWearSlots[i]);
			if (item && CanPlayerBotAttemptRefineItem(ch, item))
				return true;
		}
		return false;
	}
}

#endif
