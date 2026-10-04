#ifndef __INC_METIN2_PLAYERBOT_SADDLEBAG_H__
#define __INC_METIN2_PLAYERBOT_SADDLEBAG_H__

// Horse saddlebags (MT2009 Plus): the fifth bag page, and the craft
// materials that pay for it.
//
// The engine and the quests are the original MT2009's and were live all
// along; nobody used them. The page is 45 cells after the four bag pages
// (INVENTORY_DEFAULT_MAX_NUM..INVENTORY_MAX_NUM), open in rows of five by the
// special flag "horse_inventory_slot" (0..9, CHARACTER::SetHorseInventory),
// and only while the horse is out or ridden (CanUseHorseInventory).
//
//   * horse_inventory_init.quest, the Stajenny (20349): the first row, for a
//     horse of level 1, one horse medal and five Materialy Rzemieslnicze;
//   * horse_inventory.quest: each next row, for a higher horse - since
//     MT2009_PLUS_HORSE30_V1 (Digi Rasta's horse to 30) 4, 8, 12, 16, 20, 24,
//     27, 30 - and 1-6 medals and 20-200 materials; 25 medals and 685
//     materials for all nine;
//   * xyz_refine_exchange.quest, the Dozorca (9005): refine goods (30000-30092,
//     30192-30199, 30343-30359, 30367) into Materialy Rzemieslnicze (30378),
//     1000 yang a piece, 55 in a hundred come out (item_exchange.lua; the
//     world's difficulty may change that, GetPlayerBotExchangeChance).
//
// Both windows are a client's, so the pass does server-side what the quests
// do, with their numbers (the Alchemist's exchange in playerbot_town.h is the
// same shape). Who (operator, 25 September 2026):
//
//   * PLAYERBOT_SADDLEBAG_PERCENT of the bots, by player id, raise their horse
//     (every bot does, CanPlayerBotAdvanceHorse) and open
//     saddlebag rows - each its own number of them (3-9), one every few hours
//     at most, as the horse allows. They exchange their spare refine goods,
//     buy materials and medals off the counters, and keep what the next rows
//     need.
//   * Every other bot takes the refine goods its counters did not sell to the
//     Dozorca instead of keeping them, and sells the materials at
//     PLAYERBOT_CRAFT_MATERIAL_PRICE a piece.
//
// And the page itself: the engine puts a drop there once the four pages are
// full and the horse is out. The bot's own passes read the four pages only
// (PLAYERBOT_BAG_CELLS), so what lands there is moved back down as soon as a
// cell frees - the page is room to keep hunting, not a place things get lost.

namespace
{
	// MT2009_PLUS_SADDLEBAG_MARKET_V1: half the bots raise saddlebags (it was
	// 30%): 775 pages of Materialy Rzemieslnicze on the supporters' world had
	// too few buyers.
	const int PLAYERBOT_SADDLEBAG_PERCENT = 50;
	const DWORD PLAYERBOT_CRAFT_MATERIAL_VNUM = 30378;
	const DWORD PLAYERBOT_CRAFT_MATERIAL_PRICE = 100000;
	const long long PLAYERBOT_CRAFT_EXCHANGE_FEE = 1000;
	// A trip to the Dozorca for fewer goods than this is not worth the walk -
	// unless it is a saddlebag bot short of materials.
	const int PLAYERBOT_CRAFT_EXCHANGE_MIN_UNITS = 10;
	// A refine-goods line on an offline counter this long unsold comes home to
	// be exchanged (BotOfflineUnwantedLine).
	const DWORD PLAYERBOT_CRAFT_UNSOLD_RECALL_MS = PLAYERBOT_CRAFT_UNSOLD_RECALL_MS_PRE;
	// And a line of the classic stall that came home unsold this many times.
	const int PLAYERBOT_CRAFT_UNSOLD_STANDS = 2;
	const DWORD PLAYERBOT_SADDLEBAG_CHECK_MIN_MS = 4 * 60 * 1000;
	const DWORD PLAYERBOT_SADDLEBAG_CHECK_MAX_MS = 9 * 60 * 1000;
	// Between two rows: the bot does not open them all in one visit.
	// MT2009_PLUS_HORSE_ECONOMY_V1: one to two hours (it was two to four).
	const int PLAYERBOT_SADDLEBAG_ROW_GAP_MIN_S = 1 * 60 * 60;
	const int PLAYERBOT_SADDLEBAG_ROW_GAP_MAX_S = 2 * 60 * 60;
	const char* PLAYERBOT_SADDLEBAG_FLAG = "horse_inventory_slot";
	const char* PLAYERBOT_SADDLEBAG_NEXT_ROW_FLAG = "playerbot.saddlebag_next_row";
	// A material off a counter: at most this a piece.
	const long long PLAYERBOT_CRAFT_MATERIAL_MAX_BUY = 150000;
	const int PLAYERBOT_CRAFT_MATERIAL_PURSE_PERCENT = 40;
	// MT2009_PLUS_SADDLEBAG_MARKET_V1: with this many pieces on the counters
	// of the world the market is full, and a saddlebag bot buys for every row
	// it still wants (not only the next) out of a larger share of its purse.
	const DWORD PLAYERBOT_SADDLEBAG_MARKET_FULL_UNITS = 400;
	const int PLAYERBOT_CRAFT_MATERIAL_FULL_PURSE_PERCENT = 60;
	const DWORD PLAYERBOT_SADDLEBAG_MOVE_MS = 15000;

	struct TPlayerBotSaddlebagRow
	{
		BYTE horseLevel;
		int medals;
		int materials;
	};
	// Indexed by the rows already open: [0] is horse_inventory_init.quest,
	// [1..8] horse_inventory.quest's table.
	const TPlayerBotSaddlebagRow PLAYERBOT_SADDLEBAG_ROWS[INVENTORY_PAGE_ROW] = {
		// MT2009_PLUS_HORSE30_V1: horse_inventory.quest's rows to thirty.
		{ 1, 1, 5 }, { 4, 1, 20 }, { 8, 1, 30 }, { 12, 2, 40 }, { 16, 2, 60 },
		{ 20, 3, 80 }, { 24, 4, 100 }, { 27, 5, 150 }, { 30, 6, 200 },
	};

	enum
	{
		PLAYERBOT_SADDLEBAG_ERRAND_NONE,
		PLAYERBOT_SADDLEBAG_ERRAND_DOZORCA,
		PLAYERBOT_SADDLEBAG_ERRAND_STABLE,
	};

	struct TPlayerBotSaddlebagStats
	{
		unsigned exchanges;
		unsigned long long unitsIn;
		unsigned long long materialsOut;
		unsigned rows;
		unsigned recalled;
		unsigned movedBack;
		unsigned materialsBought;
		unsigned medalsBought;
		unsigned goodsBought;
		// MT2009_PLUS_SADDLEBAG_MARKET_V1: the materials' census - pieces and
		// yang asked for, and the lines a saddlebag bot wanted but let stand.
		unsigned long long materialUnitsBought;
		unsigned long long materialYang;
		unsigned materialRefusedCap;
		unsigned materialRefusedPurse;
	};
	TPlayerBotSaddlebagStats s_kPlayerBotSaddlebagStats = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	// Refine-goods lines that came home unsold from an offline counter, by
	// item id: the Dozorca's, not the counter's again.
	std::set<DWORD> s_setPlayerBotCraftRecalled;

	bool IsPlayerBotSaddlebagKeeperPID(DWORD pid)
	{
		const DWORD h = (pid * 2246822519U) ^ 0x4a554b49U;
		return (int)((h >> 13) % 100) < PLAYERBOT_SADDLEBAG_PERCENT;
	}

	bool IsPlayerBotSaddlebagKeeper(LPCHARACTER ch)
	{
		if (!ch || !ch->IsPC())
			return false;
		const DWORD pid = ch->GetPlayerID();
		return IsPlayerBotSaddlebagKeeperPID(pid) && !IsPlayerBotSidekickPID(pid) &&
				!CPlayerBotManager::instance().IsMedalDropperCohortPID(pid) &&
				GetPlayerBotPersonalityByPID(pid) != BOT_PERSONALITY_MEDAL_DROPPER;
	}

	// How many rows this bot wants: its own number, three to nine.
	int GetPlayerBotSaddlebagTargetRows(DWORD pid)
	{
		return 3 + (int)(((pid * 2654435761U) >> 11) % 7);
	}

	int GetPlayerBotSaddlebagRows(LPCHARACTER ch)
	{
		return ch ? (int)std::min<long long>(INVENTORY_PAGE_ROW,
				std::max<long long>(0, ch->GetSpecialFlag(PLAYERBOT_SADDLEBAG_FLAG))) : 0;
	}

	// The next row, when a saddlebag bot still wants one; NULL otherwise.
	const TPlayerBotSaddlebagRow* GetPlayerBotNextSaddlebagRow(LPCHARACTER ch)
	{
		if (!IsPlayerBotSaddlebagKeeper(ch))
			return NULL;
		const int rows = GetPlayerBotSaddlebagRows(ch);
		if (rows >= INVENTORY_PAGE_ROW || rows >= GetPlayerBotSaddlebagTargetRows(ch->GetPlayerID()))
			return NULL;
		return &PLAYERBOT_SADDLEBAG_ROWS[rows];
	}

	// The next row can be opened now, resources aside: the horse is there and
	// the gap since the last one has passed.
	bool IsPlayerBotSaddlebagRowDue(LPCHARACTER ch)
	{
		const TPlayerBotSaddlebagRow* row = GetPlayerBotNextSaddlebagRow(ch);
		return row && ch->GetHorseLevel() >= row->horseLevel &&
				get_global_time() >= (time_t)std::max(0, ch->GetQuestFlag(PLAYERBOT_SADDLEBAG_NEXT_ROW_FLAG));
	}

	int CountPlayerBotCraftMaterials(LPCHARACTER ch);

	// The medals the horse pass leaves alone: the due row's.
	// MT2009_PLUS_HORSE_ECONOMY_V1: only while the row has its materials. A
	// due row short of them held its medals back from the horse for as long
	// as the materials did not come - and they did not, the counters' lines
	// of two hundred being past every purse - so a keeper's horse stood at
	// eleven or twelve with the medals for the next levels in its bag.
	int GetPlayerBotSaddlebagMedalReserve(LPCHARACTER ch)
	{
		// MT2009_PLUS_HORSE_GOODS_MARKET_V1: rows only past 35 (the band rule,
		// GetPlayerBotHorseGoodsTargetLevel); a younger bot keeps its horse's.
		if (GetPlayerBotHorseGoodsTargetLevel(ch) > 0)
			return 0;
		const TPlayerBotSaddlebagRow* row = GetPlayerBotNextSaddlebagRow(ch);
		return row && IsPlayerBotSaddlebagRowDue(ch) && CountPlayerBotCraftMaterials(ch) >= row->materials
				? row->medals : 0;
	}

	// The materials the rows this bot still wants will take.
	int GetPlayerBotSaddlebagMaterialsWanted(LPCHARACTER ch, bool nextOnly)
	{
		(void)nextOnly;
		if (!IsPlayerBotSaddlebagKeeper(ch))
			return 0;
		// MT2009_PLUS_HORSE_GOODS_MARKET_V1: up to 35 none; past 35 only the due
		// row's, bought when the row is due. (HORSE_ECONOMY_V2 kept two rows
		// ahead - 48 213 materials stood in the bags of the supporters' world;
		// on the test world every bot from its first level kept them and the
		// counters had none.)
		if (GetPlayerBotHorseGoodsTargetLevel(ch) > 0)
			return 0;
		const TPlayerBotSaddlebagRow* row = GetPlayerBotNextSaddlebagRow(ch);
		return row && IsPlayerBotSaddlebagRowDue(ch) ? row->materials : 0;
	}

	int CountPlayerBotCraftMaterials(LPCHARACTER ch)
	{
		return ch ? (int)ch->CountSpecifyItem(PLAYERBOT_CRAFT_MATERIAL_VNUM) : 0;
	}

	// MT2009_PLUS_HORSE30_V1: every bot's horse training takes materials too
	// (5 / 10 / 20 a level, playerbot_horse30.h), so what a bot keeps, buys
	// and exchanges for is its rows' and its next training's together - a bot
	// that raises no saddlebag still keeps its training's.
	int GetPlayerBotCraftMaterialsWanted(LPCHARACTER ch, bool nextOnly)
	{
		return GetPlayerBotSaddlebagMaterialsWanted(ch, nextOnly) +
				GetPlayerBotHorseGoodsMaterialsWanted(ch, nextOnly); // MT2009_PLUS_HORSE_GOODS_MARKET_V1
	}

	bool IsPlayerBotCraftExchangeVnum(DWORD vnum)
	{
		return (vnum >= 30000 && vnum <= 30092) || (vnum >= 30192 && vnum <= 30199) ||
				(vnum >= 30343 && vnum <= 30359) || vnum == 30367;
	}

	void NotePlayerBotCraftRecalled(DWORD pid, DWORD itemId)
	{
		if (s_setPlayerBotCraftRecalled.insert(itemId).second)
		{
			++s_kPlayerBotSaddlebagStats.recalled;
			if (s_setPlayerBotCraftRecalled.size() > 200000)
				s_setPlayerBotCraftRecalled.clear();
		}
	}

	bool IsPlayerBotCraftUnsold(LPCHARACTER ch, LPITEM item)
	{
		if (s_setPlayerBotCraftRecalled.count(item->GetID()))
			return true;
		TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (st == s_mapPlayerBotAIStates.end())
			return false;
		std::map<DWORD, BYTE>::const_iterator unsold = st->second.mapStallUnsold.find(item->GetID());
		return unsold != st->second.mapStallUnsold.end() && unsold->second >= PLAYERBOT_CRAFT_UNSOLD_STANDS;
	}

	// Refine goods for the Dozorca: never what the bot's own anvil needs, never
	// what the operator put a policy on. For every bot: what came home unsold,
	// and under bag pressure a spare nobody on the market is short of (what
	// used to go down to the safebox). For a saddlebag bot short of materials:
	// any spare.
	bool IsPlayerBotCraftExchangeStock(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->IsEquipped() || item->isLocked() || item->IsExchanging() ||
				item->GetCell() >= PLAYERBOT_BAG_CELLS || !IsPlayerBotCraftExchangeVnum(item->GetVnum()) ||
				!IsPlayerBotTradeableMaterial(item))
			return false;
		if (GetPlayerBotItemPolicy(item) != PLAYERBOT_ITEM_POLICY_NONE ||
				PlayerBotNeedsRefineMaterial(ch, item->GetVnum()))
			return false;
		if (IsPlayerBotCraftUnsold(ch, item))
			return true;
		// A saddlebag bot short of materials takes every one its anvil does not
		// need - the ones it bought off the counters for this among them.
		// MT2009_PLUS_HORSE30_V1: and any bot short of its horse training's.
		if (CountPlayerBotCraftMaterials(ch) < GetPlayerBotCraftMaterialsWanted(ch, false))
			return true;
		if (!IsPlayerBotSurplusMaterial(ch, item))
			return false;
		return GetPlayerBotLedgerDemand(item->GetVnum()) == 0 && IsPlayerBotBagUnderPressure(ch);
	}

	int CollectPlayerBotCraftExchangeStock(LPCHARACTER ch, std::vector<LPITEM>* out, long long* fee)
	{
		int units = 0;
		const long long spendable = (long long)ch->GetGold() - (long long)GetPlayerBotReservedGold(ch);
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || !IsPlayerBotCraftExchangeStock(ch, item) ||
					IsPlayerBotSidekickLockedItem(ch, item))	// MT2009_PLUS_SIDEKICK_EQUIP_LOCK_V1
				continue;
			const int count = std::max<int>(1, (int)item->GetCount());
			if ((long long)(units + count) * PLAYERBOT_CRAFT_EXCHANGE_FEE > spendable)
				continue;
			units += count;
			if (out)
				out->push_back(item);
		}
		if (fee)
			*fee = (long long)units * PLAYERBOT_CRAFT_EXCHANGE_FEE;
		return units;
	}

	// A saddlebag bot's materials are its own up to what its rows still take;
	// past that, and every other bot's, they are goods.
	bool IsPlayerBotKeptCraftMaterial(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetVnum() != PLAYERBOT_CRAFT_MATERIAL_VNUM)
			return false;
		const int wanted = GetPlayerBotCraftMaterialsWanted(ch, false); // MT2009_PLUS_HORSE30_V1
		if (wanted <= 0)
			return false;
		// Counted in cell order, like the other stocks: the first `wanted`
		// pieces are kept.
		int ahead = 0;
		for (WORD cell = 0; cell < item->GetCell() && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM other = ch->GetInventoryItem(cell);
			if (other && other != item && other->GetCell() == cell &&
					other->GetVnum() == PLAYERBOT_CRAFT_MATERIAL_VNUM)
				ahead += (int)other->GetCount();
		}
		return ahead < wanted;
	}

	// ------------------------------------------------------------ the market

	// MT2009_PLUS_SADDLEBAG_MARKET_V1: whether the counters of the world are
	// full of materials (the ledger's supply, as the price curve reads it).
	bool IsPlayerBotCraftMaterialMarketFull()
	{
		return GetPlayerBotOperatorGoodsSupply(PLAYERBOT_CRAFT_MATERIAL_VNUM) >= PLAYERBOT_SADDLEBAG_MARKET_FULL_UNITS;
	}

	// The materials a saddlebag bot buys towards: the next row's, or with a
	// full market every row it still wants.
	int GetPlayerBotCraftMaterialBuyTarget(LPCHARACTER ch)
	{
		return GetPlayerBotCraftMaterialsWanted(ch, !IsPlayerBotCraftMaterialMarketFull()); // MT2009_PLUS_HORSE30_V1
	}

	bool WantsPlayerBotCraftMaterialOffer(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !offer || offer->GetVnum() != PLAYERBOT_CRAFT_MATERIAL_VNUM)
			return false;
		const int want = GetPlayerBotCraftMaterialBuyTarget(ch);
		const int have = CountPlayerBotCraftMaterials(ch);
		// A line no bigger than what is missing and one line's worth more: a
		// bot short of five does not buy a stack of two hundred.
		return want > 0 && have < want &&
				(int)offer->GetCount() <= want - have + PLAYERBOT_CRAFT_MATERIAL_LINE_UNITS;
	}

	// Refine goods off a counter, for the Dozorca: a saddlebag bot short of
	// materials for its next row buys the cheap ones its anvil does not need
	// (operator: "niech skupuja ulepszacze"). Two of them make a material on
	// average, which the market sells at PLAYERBOT_CRAFT_MATERIAL_PRICE.
	const long long PLAYERBOT_CRAFT_GOODS_MAX_UNIT = 20000;
	const int PLAYERBOT_CRAFT_GOODS_PURSE_PERCENT = 20;

	bool WantsPlayerBotCraftGoodsOffer(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !offer || !IsPlayerBotCraftExchangeVnum(offer->GetVnum()) ||
				!IsPlayerBotTradeableMaterial(offer) || PlayerBotNeedsRefineMaterial(ch, offer->GetVnum()))
			return false;
		const int want = GetPlayerBotCraftMaterialsWanted(ch, true); // MT2009_PLUS_HORSE30_V1
		return want > 0 && CountPlayerBotCraftMaterials(ch) < want &&
				CountPlayerBotFreeInventoryCells(ch) > 6;
	}

	bool CanPlayerBotPayForCraftGoods(LPCHARACTER ch, LPITEM item, long long price)
	{
		if (!ch || !item || price <= 0)
			return false;
		const long long unit = price / std::max<long long>(1, (long long)item->GetCount());
		const long long spare = (long long)ch->GetGold() - GetPlayerBotReservedGold(ch) -
				(long long)PLAYERBOT_SHOPPING_GOLD_FLOOR;
		return unit <= PLAYERBOT_CRAFT_GOODS_MAX_UNIT &&
				price <= spare * PLAYERBOT_CRAFT_GOODS_PURSE_PERCENT / 100;
	}

	bool CanPlayerBotPayForCraftMaterial(LPCHARACTER ch, LPITEM item, long long price)
	{
		if (!ch || !item || price <= 0)
			return false;
		const long long unit = price / std::max<long long>(1, (long long)item->GetCount());
		const long long spare = (long long)ch->GetGold() - GetPlayerBotReservedGold(ch) -
				(long long)PLAYERBOT_SHOPPING_GOLD_FLOOR;
		// MT2009_PLUS_OWNER_PRICES_V2: the curve's piece for this line, for a
		// bot of this level (playerbot_town.h), where that is over the cap.
		const long long cap = std::max<long long>(PLAYERBOT_CRAFT_MATERIAL_MAX_BUY *
				GetPlayerBotOperatorLevelPercent((int)ch->GetLevel()) / 100,
				GetPlayerBotOperatorBuyCap(ch, item->GetVnum(), PLAYERBOT_CRAFT_MATERIAL_UNIT_PRICE,
					(DWORD)std::max<int>(1, (int)item->GetCount())));
		// MT2009_PLUS_SADDLEBAG_MARKET_V1: a full market, a larger share.
		const int purse = IsPlayerBotCraftMaterialMarketFull() ? PLAYERBOT_CRAFT_MATERIAL_FULL_PURSE_PERCENT
				: PLAYERBOT_CRAFT_MATERIAL_PURSE_PERCENT;
		if (unit > cap)
		{
			++s_kPlayerBotSaddlebagStats.materialRefusedCap;
			return false;
		}
		if (price > spare * purse / 100)
		{
			++s_kPlayerBotSaddlebagStats.materialRefusedPurse;
			return false;
		}
		return true;
	}

	// A medal for the due row, over what the horse pass would spend.
	bool PlayerBotSaddlebagWantsMedal(LPCHARACTER ch)
	{
		if (GetPlayerBotHorseGoodsTargetLevel(ch) > 0)	// MT2009_PLUS_HORSE_GOODS_MARKET_V1
			return false;
		const TPlayerBotSaddlebagRow* row = GetPlayerBotNextSaddlebagRow(ch);
		return row && IsPlayerBotSaddlebagRowDue(ch) &&
				(int)ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) < row->medals;
	}

	// Asked before a market walk without reading a counter.
	bool PlayerBotWantsSaddlebagGoods(LPCHARACTER ch)
	{
		// MT2009_PLUS_HORSE30_V1: the materials of the horse's next training
		// for every bot, the rows' for a saddlebag bot.
		if (!IsPlayerBotSaddlebagKeeper(ch) && GetPlayerBotHorseTrainingMaterialsWanted(ch) <= 0)
			return false;
		if (PlayerBotSaddlebagWantsMedal(ch))
		{
			const TPlayerBotMarketLedgerEntry* medals = GetPlayerBotMarketLedgerEntry(PLAYERBOT_HORSE_MEDAL_VNUM);
			if (medals && medals->dwSupplyUnits > 0)
				return true;
		}
		const int want = GetPlayerBotCraftMaterialBuyTarget(ch); // MT2009_PLUS_SADDLEBAG_MARKET_V1
		if (want > 0 && CountPlayerBotCraftMaterials(ch) < want)
		{
			const TPlayerBotMarketLedgerEntry* mats = GetPlayerBotMarketLedgerEntry(PLAYERBOT_CRAFT_MATERIAL_VNUM);
			if (mats && mats->dwSupplyUnits > 0)
				return true;
		}
		return false;
	}

	void NotePlayerBotSaddlebagBought(LPCHARACTER ch, DWORD vnum, long long price, DWORD count)
	{
		if (vnum == PLAYERBOT_CRAFT_MATERIAL_VNUM)
		{
			++s_kPlayerBotSaddlebagStats.materialsBought;
			s_kPlayerBotSaddlebagStats.materialUnitsBought += count;
			s_kPlayerBotSaddlebagStats.materialYang += (unsigned long long)std::max<long long>(0, price);
			sys_log(0, "PLAYERBOT_SADDLEBAG: material bought pid=%u name=%s units=%u price=%lld have=%d want=%d rows=%d market_full=%d supply=%u",
					ch ? ch->GetPlayerID() : 0, ch ? ch->GetName() : "", (unsigned int)count, price,
					CountPlayerBotCraftMaterials(ch), GetPlayerBotCraftMaterialBuyTarget(ch),
					GetPlayerBotSaddlebagRows(ch), IsPlayerBotCraftMaterialMarketFull() ? 1 : 0,
					(unsigned int)GetPlayerBotOperatorGoodsSupply(PLAYERBOT_CRAFT_MATERIAL_VNUM));
			return;
		}
		else if (IsPlayerBotCraftExchangeVnum(vnum) && IsPlayerBotSaddlebagKeeper(ch) &&
				GetPlayerBotSaddlebagMaterialsWanted(ch, true) > 0)
			++s_kPlayerBotSaddlebagStats.goodsBought;
		// MT2009_PLUS_HORSE_ECONOMY_V1: every medal bought off a counter is
		// counted, a horse's as well as a saddlebag row's.
		else if (vnum == PLAYERBOT_HORSE_MEDAL_VNUM)
			++s_kPlayerBotSaddlebagStats.medalsBought;
		else
			return;
		sys_log(0, "PLAYERBOT_SADDLEBAG: bought pid=%u name=%s vnum=%u price=%lld",
				ch ? ch->GetPlayerID() : 0, ch ? ch->GetName() : "", vnum, price);
	}

	// ------------------------------------------------------------ the page

	// What the engine put on the saddlebag page goes back down to the four
	// pages the moment a cell there is free and the page is open.
	int MovePlayerBotSaddlebagDown(LPCHARACTER ch)
	{
		if (!ch || !ch->CanUseHorseInventory() || ch->GetHorseInventoryUnlock() == 0 ||
				ch->GetExchange() || ch->GetMyShop() || ch->IsAcceOpened())
			return 0;
		const int end = std::min<int>(INVENTORY_MAX_NUM,
				INVENTORY_DEFAULT_MAX_NUM + INVENTORY_PAGE_COLUMN * ch->GetHorseInventoryUnlock());
		int moved = 0;
		for (int cell = INVENTORY_DEFAULT_MAX_NUM; cell < end && moved < 8; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->isLocked() || item->IsExchanging())
				continue;
			int dest = -1;
			for (int d = 0; d < PLAYERBOT_BAG_CELLS; ++d)
				if (ch->IsEmptyItemGrid(TItemPos(INVENTORY, d), item->GetSize()))
				{
					dest = d;
					break;
				}
			if (dest < 0)
				break;
			const DWORD vnum = item->GetVnum();
			if (ch->MoveItem(TItemPos(INVENTORY, cell), TItemPos(INVENTORY, dest), item->GetCount()))
			{
				++moved;
				sys_log(1, "PLAYERBOT_SADDLEBAG: moved down pid=%u vnum=%u %d->%d",
						ch->GetPlayerID(), vnum, cell, dest);
			}
		}
		s_kPlayerBotSaddlebagStats.movedBack += moved;
		return moved;
	}

	// ------------------------------------------------------------ the errands

	bool CanPlayerBotOpenSaddlebagRow(LPCHARACTER ch)
	{
		const TPlayerBotSaddlebagRow* row = GetPlayerBotNextSaddlebagRow(ch);
		return row && IsPlayerBotSaddlebagRowDue(ch) &&
				(int)ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) >= row->medals &&
				CountPlayerBotCraftMaterials(ch) >= row->materials;
	}

	// The Stajenny's side: the quest's checks, the quest's items out, the row
	// in. The init quest's state is the flag's to answer (it asks for a flag
	// under one), so nothing else is set.
	bool OpenPlayerBotSaddlebagRow(LPCHARACTER ch)
	{
		const int rows = GetPlayerBotSaddlebagRows(ch);
		const TPlayerBotSaddlebagRow* row = GetPlayerBotNextSaddlebagRow(ch);
		if (!row || !CanPlayerBotOpenSaddlebagRow(ch))
			return false;
		ch->RemoveSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM, row->medals);
		ch->RemoveSpecifyItem(PLAYERBOT_CRAFT_MATERIAL_VNUM, row->materials);
		ch->SetSpecialFlag(PLAYERBOT_SADDLEBAG_FLAG, rows + 1, false);
		ch->SetQuestFlag(PLAYERBOT_SADDLEBAG_NEXT_ROW_FLAG, (int)get_global_time() +
				number(PLAYERBOT_SADDLEBAG_ROW_GAP_MIN_S, PLAYERBOT_SADDLEBAG_ROW_GAP_MAX_S));
		++s_kPlayerBotSaddlebagStats.rows;
		sys_log(0, "PLAYERBOT_SADDLEBAG: row opened pid=%u name=%s lv=%d horse=%u rows=%d->%d target=%d medals=%d materials=%d",
				ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), ch->GetHorseLevel(), rows, rows + 1,
				GetPlayerBotSaddlebagTargetRows(ch->GetPlayerID()), row->medals, row->materials);
		return true;
	}

	// The Dozorca's side of item_exchange.lua: the goods out, the fee, a roll
	// a piece, the materials in.
	int ExchangePlayerBotCraftGoods(LPCHARACTER ch)
	{
		std::vector<LPITEM> goods;
		long long fee = 0;
		const int units = CollectPlayerBotCraftExchangeStock(ch, &goods, &fee);
		if (units <= 0 || CountPlayerBotFreeInventoryCells(ch) < 2)
			return 0;
		for (size_t i = 0; i < goods.size(); ++i)
		{
			s_setPlayerBotCraftRecalled.erase(goods[i]->GetID());
			ITEM_MANAGER::instance().RemoveItem(goods[i], "PLAYERBOT_CRAFT_EXCHANGE");
		}
		PlayerBotChangeGold(ch, -fee);
		// MT2009_PLUS_EXCHANGE_CHANCE_V1: the players' chance, the world's
		// difficulty (55 in a hundred on every preset; custom's own).
		const int chance = GetPlayerBotExchangeChance(PLAYERBOT_NPC_EXCHANGE_MATERIAL);
		int made = 0;
		for (int i = 0; i < units; ++i)
			if (chance >= 100 || number(1, 100) <= chance)
				++made;
		for (int left = made; left > 0; )
		{
			const int chunk = std::min(left, (int)ITEM_MAX_COUNT);
			ch->AutoGiveItem(PLAYERBOT_CRAFT_MATERIAL_VNUM, chunk, -1, false);
			left -= chunk;
		}
		++s_kPlayerBotSaddlebagStats.exchanges;
		s_kPlayerBotSaddlebagStats.unitsIn += units;
		s_kPlayerBotSaddlebagStats.materialsOut += made;
		sys_log(0, "PLAYERBOT_CRAFT: exchanged pid=%u name=%s keeper=%d stacks=%u units=%d made=%d fee=%lld have=%d gold=%lld",
				ch->GetPlayerID(), ch->GetName(), IsPlayerBotSaddlebagKeeper(ch) ? 1 : 0,
				(unsigned int)goods.size(), units, made, fee, CountPlayerBotCraftMaterials(ch),
				(long long)ch->GetGold());
		return made;
	}

	void EndPlayerBotSaddlebagErrand(TPlayerBotAIState& state, DWORD dwNow)
	{
		state.bSaddlebagErrand = PLAYERBOT_SADDLEBAG_ERRAND_NONE;
		state.dwNextSaddlebagActionTime = 0;
		state.dwNextSaddlebagCheckTime = dwNow + number(PLAYERBOT_SADDLEBAG_CHECK_MIN_MS, PLAYERBOT_SADDLEBAG_CHECK_MAX_MS);
		ClearPlayerBotRoute(state, true);
	}

	// The tick. The page's move-back runs on its own short clock; the two
	// errands - the Dozorca's exchange and the Stajenny's row - are local to
	// the village the bot stands in (both NPCs are in all six), walked and
	// done like the Alchemist's.
	bool ManagePlayerBotSaddlebag(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return false;
		if (dwNow >= state.dwNextSaddlebagMoveTime)
		{
			state.dwNextSaddlebagMoveTime = dwNow + PLAYERBOT_SADDLEBAG_MOVE_MS;
			MovePlayerBotSaddlebagDown(ch);
		}
		if (state.bVisitingShop || state.bVisitingBiologist || state.bVisitingHerbalist ||
				state.bVisitingStable || state.bVisitingAlchemist || state.bVisitingUriel ||
				state.bFishingSession || state.bMarketTrip)
			return false;
		if (state.bSaddlebagErrand == PLAYERBOT_SADDLEBAG_ERRAND_NONE && dwNow < state.dwNextSaddlebagCheckTime)
			return false;

		playerbot_empire_rules::TTownServices svc;
		if (!playerbot_empire_rules::GetTownServices(ch->GetMapIndex(), svc) ||
				(ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty())) ||
				IsPlayerBotHeldForCompany(ch) || ch->GetMyShop() != NULL)
		{
			if (state.bSaddlebagErrand != PLAYERBOT_SADDLEBAG_ERRAND_NONE)
				EndPlayerBotSaddlebagErrand(state, dwNow);
			else
				state.dwNextSaddlebagCheckTime = dwNow + number(PLAYERBOT_SADDLEBAG_CHECK_MIN_MS, PLAYERBOT_SADDLEBAG_CHECK_MAX_MS);
			return false;
		}

		if (state.bSaddlebagErrand == PLAYERBOT_SADDLEBAG_ERRAND_NONE)
		{
			state.dwNextSaddlebagCheckTime = dwNow + number(PLAYERBOT_SADDLEBAG_CHECK_MIN_MS, PLAYERBOT_SADDLEBAG_CHECK_MAX_MS);
			if (ch->GetVictim() != NULL || state.dwTargetVID != 0)
				return false;
			if (CanPlayerBotOpenSaddlebagRow(ch))
				state.bSaddlebagErrand = PLAYERBOT_SADDLEBAG_ERRAND_STABLE;
			else
			{
				long long fee = 0;
				const int units = CollectPlayerBotCraftExchangeStock(ch, NULL, &fee);
				// MT2009_PLUS_HORSE30_V1: a horse training's materials count too.
				const bool shortOfMaterials =
						CountPlayerBotCraftMaterials(ch) < GetPlayerBotCraftMaterialsWanted(ch, true);
				if (units <= 0 || (units < PLAYERBOT_CRAFT_EXCHANGE_MIN_UNITS && !shortOfMaterials) ||
						CountPlayerBotFreeInventoryCells(ch) < 2)
					return false;
				state.bSaddlebagErrand = PLAYERBOT_SADDLEBAG_ERRAND_DOZORCA;
			}
			state.dwNextSaddlebagActionTime = 0;
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ch->Stop();
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_SADDLEBAG: going to %s pid=%u name=%s lv=%d horse=%u rows=%d materials=%d",
					state.bSaddlebagErrand == PLAYERBOT_SADDLEBAG_ERRAND_STABLE ? "the Stajenny" : "the Dozorca",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), ch->GetHorseLevel(),
					GetPlayerBotSaddlebagRows(ch), CountPlayerBotCraftMaterials(ch));
		}

		const bool stable = state.bSaddlebagErrand == PLAYERBOT_SADDLEBAG_ERRAND_STABLE;
		const playerbot_empire_rules::TPoint npc = stable ? svc.stableKeeper : svc.storekeeper;
		SetPlayerBotAction(state, stable ? BOT_ACTION_STABLE : BOT_ACTION_SHOP, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		long approachX = 0, approachY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), npc.x, npc.y,
				stable ? 0x4a554b53U : 0x444f5a4fU, approachX, approachY);
		if (DISTANCE_APPROX(ch->GetX() - approachX, ch->GetY() - approachY) > PLAYERBOT_STABLE_ARRIVE_DISTANCE)
		{
			if (!MovePlayerBotTownLeg(ch, state, dwNow, approachX, approachY, PLAYERBOT_STABLE_ARRIVE_DISTANCE) &&
					state.bStuckCounter >= 6)
			{
				sys_err("PLAYERBOT_SADDLEBAG: route failed pid=%u name=%s map=%ld to=%s from=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), stable ? "stable" : "dozorca",
						ch->GetX(), ch->GetY());
				EndPlayerBotSaddlebagErrand(state, dwNow);
				return false;
			}
			return true;
		}

		ch->Stop();
		ch->SetPosition(POS_STANDING);
		if (state.dwNextSaddlebagActionTime == 0)
		{
			state.dwNextSaddlebagActionTime = dwNow + number(3000, 8000);
			return true;
		}
		if (dwNow < state.dwNextSaddlebagActionTime)
			return true;

		if (stable)
			OpenPlayerBotSaddlebagRow(ch);
		else
		{
			ExchangePlayerBotCraftGoods(ch);
			// Straight on to the stable when the exchange made the row.
			if (CanPlayerBotOpenSaddlebagRow(ch))
			{
				state.bSaddlebagErrand = PLAYERBOT_SADDLEBAG_ERRAND_STABLE;
				state.dwNextSaddlebagActionTime = 0;
				ClearPlayerBotRoute(state, true);
				return true;
			}
		}
		EndPlayerBotSaddlebagErrand(state, dwNow);
		return false;
	}

	void LogPlayerBotSaddlebagCensus()
	{
		unsigned keepers = 0, withRows = 0, rowsTotal = 0, due = 0;
		unsigned byRows[INVENTORY_PAGE_ROW + 1] = { 0 };
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch || !IsPlayerBotSaddlebagKeeper(ch))
				continue;
			++keepers;
			const int rows = GetPlayerBotSaddlebagRows(ch);
			++byRows[rows];
			rowsTotal += rows;
			if (rows > 0)
				++withRows;
			if (IsPlayerBotSaddlebagRowDue(ch))
				++due;
		}
		// MT2009_PLUS_HORSE_ECONOMY_V1: the horses of every bot - how many at
		// each stage, the medals in bags, and who holds medals it could hand in.
		{
			// MT2009_PLUS_HORSE30_V1: to thirty.
			unsigned stage[8] = { 0 };   // 0, 1-9, 10, 11-19, 20, 21-28, 29, 30
			unsigned bots = 0, medalHolders = 0, canAdvance = 0, canWithMedal = 0, candidates = 0;
			unsigned long long medals = 0, horseSum = 0;
			for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
					it != s_mapPlayerBotAIStates.end(); ++it)
			{
				LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
				if (!ch || ch->GetLevel() < PLAYERBOT_HORSE_REQUIRED_LEVEL)
					continue;
				++bots;
				const int h = ch->GetHorseLevel();
				horseSum += h;
				++stage[h <= 0 ? 0 : h < 10 ? 1 : h == 10 ? 2 : h < 20 ? 3 : h == 20 ? 4 : h < 29 ? 5 : h == 29 ? 6 : 7];
				const int held = (int)ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM);
				medals += held;
				if (held > 0)
					++medalHolders;
				const bool can = CanPlayerBotAdvanceHorse(ch);
				if (can)
					++canAdvance;
				if (can && CanPlayerBotPayHorseTraining(ch)) // MT2009_PLUS_HORSE30_V1
					++canWithMedal;
				if (IsPlayerBotBattleHorseCandidate(ch))
					++candidates;
			}
			sys_log(0, "PLAYERBOT_HORSE: census bots_25plus=%u avg_horse=%.1f stages(0/1-9/10/11-19/20/21-28/29/30)=%u/%u/%u/%u/%u/%u/%u/%u medals_in_bags=%llu medal_holders=%u can_advance=%u can_pay_training=%u battle_candidates=%u medals_bought=%u",
					bots, bots ? (double)horseSum / bots : 0.0, stage[0], stage[1], stage[2], stage[3], stage[4], stage[5],
					stage[6], stage[7],
					medals, medalHolders, canAdvance, canWithMedal, candidates, s_kPlayerBotSaddlebagStats.medalsBought);
		}
		char dist[96] = "";
		size_t len = 0;
		for (int r = 0; r <= INVENTORY_PAGE_ROW; ++r)
			len += snprintf(dist + len, sizeof(dist) - len, "%s%u", r ? "/" : "", byRows[r]);
		sys_log(0, "PLAYERBOT_SADDLEBAG: census keepers=%u with_rows=%u rows=%u due=%u by_rows=%s exchanges=%u units_in=%llu materials_out=%llu rows_opened=%u recalled=%u moved_back=%u bought_materials=%u bought_medals=%u bought_goods=%u material_units=%llu material_yang=%llu material_refused_cap=%u material_refused_purse=%u material_supply=%u market_full=%d",
				keepers, withRows, rowsTotal, due, dist, s_kPlayerBotSaddlebagStats.exchanges,
				s_kPlayerBotSaddlebagStats.unitsIn, s_kPlayerBotSaddlebagStats.materialsOut,
				s_kPlayerBotSaddlebagStats.rows, s_kPlayerBotSaddlebagStats.recalled,
				s_kPlayerBotSaddlebagStats.movedBack, s_kPlayerBotSaddlebagStats.materialsBought,
				s_kPlayerBotSaddlebagStats.medalsBought, s_kPlayerBotSaddlebagStats.goodsBought,
				s_kPlayerBotSaddlebagStats.materialUnitsBought, s_kPlayerBotSaddlebagStats.materialYang,
				s_kPlayerBotSaddlebagStats.materialRefusedCap, s_kPlayerBotSaddlebagStats.materialRefusedPurse,
				(unsigned int)GetPlayerBotOperatorGoodsSupply(PLAYERBOT_CRAFT_MATERIAL_VNUM),
				IsPlayerBotCraftMaterialMarketFull() ? 1 : 0);
	}
}

#endif
