#ifndef __INC_METIN2_PLAYERBOT_ITEMSHOP_H__
#define __INC_METIN2_PLAYERBOT_ITEMSHOP_H__

// The bots and the ItemShop, on the 2.x line.
//
// mt2009 carries a shop of its own inside the game (CItemShopManager,
// common.itemshop_items, "/itemshop" from the client): a hundred and fifty
// lines priced in Dragon Coins - the account's `cash` - and in Dragon Marks -
// `cash_mark`, credited one for one for every coin spent. The coins come into
// the world as Kupon SM vouchers (80014-80018, a Metin stone's or a boss's
// drop by M2_DRAGON_COIN_*_PERMILLE), and until 2.0.60 a bot kept every one
// in its bag for good: ninety-seven of them on the test world after three
// days, and not one coin on any account. "Uzywanie wydropionych kuponow SM i
// przeznaczanie na potrzebne zakupy w itemshop" (Tieru, 16 September).
//
// What a bot does with the shop is what a careful player does with a small
// allowance. A voucher is cashed the moment it is found - the same charge the
// package's itemshop_manage quest makes for a player (AddCash through the db
// core, the itemshop_dragon_scroll row), without its dialog. The balance is
// kept in the state and read back from the account once an hour, because
// GetPlayerAccountBalance is a synchronous query. A purchase is the engine's
// own CItemShopManager::BuyItem behind the browse flag a client sets by
// opening the window: the level floor, the price, the account and the
// engine's purchase log are all its, and the goods arrive through
// AutoGiveItem. One purchase an hour a bot, and only what the bot's own rules
// would use: Kamien Duchowy for a Grand Master skill it can train, the change
// stone for a worn weapon whose lines the reroll pass would still reroll,
// with marks a Blessing Scroll for a piece under scroll work and the Dragon
// God's attack potions, and a hairstyle - one in four bots, once - because a
// crowd of a thousand identical heads is the thing a player notices first.
// Nothing timed: every bot already holds the premium subscription for five
// years (SpawnBot, the operator's rule), so the VIP rings and the Przepustka
// Triumfu are worth nothing to it, and BuyItem refuses a VIP item to a
// subscriber anyway.
//
// r40250 has no such shop and this file is empty there: the tick calls the
// two functions below and gets nothing.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_offline_shop.h (it asks
// PlayerBotWearsScrollWork of playerbot_town.h and the reroll rules of
// playerbot_bonus.h) and before playerbot_market.h.

#if defined(PLAYERBOT_ENGINE_MT2009)
#include "itemshop_manager.h"
#include "log.h"
#endif

namespace
{
#if defined(PLAYERBOT_ENGINE_MT2009)
	struct TPlayerBotItemShopEntry
	{
		DWORD dwIndex;
		DWORD dwPrice;
		DWORD dwCount;
		BYTE bMinLevel;
	};

	// The catalogue as the bots read it: the cheapest line per vnum for each
	// currency, and the hairstyles for sale. Rebuilt every hour from the
	// manager, which holds what the db core sent at boot - an operator who
	// edits common.itemshop_items restarts the world anyway.
	std::map<DWORD, TPlayerBotItemShopEntry> s_mapPlayerBotItemShopCoins;
	std::map<DWORD, TPlayerBotItemShopEntry> s_mapPlayerBotItemShopMarks;
	std::vector<DWORD> s_vecPlayerBotItemShopHair;
	DWORD s_dwNextPlayerBotItemShopScan = 0;

	unsigned int s_uPlayerBotVouchersUsed = 0;
	unsigned int s_uPlayerBotCoinsCharged = 0;
	unsigned int s_uPlayerBotItemShopBuys = 0;
	unsigned int s_uPlayerBotItemShopRefusals = 0;
	// Looks that found a wish the balance could not pay for.
	unsigned int s_uPlayerBotItemShopSaving = 0;
	std::map<DWORD, unsigned int> s_mapPlayerBotItemShopBought;
	DWORD s_dwNextPlayerBotItemShopCensus = 0;

	void RefreshPlayerBotItemShopCatalogue(DWORD dwNow)
	{
		if (s_dwNextPlayerBotItemShopScan != 0 && dwNow < s_dwNextPlayerBotItemShopScan)
			return;
		s_dwNextPlayerBotItemShopScan = dwNow + PLAYERBOT_ISHOP_CATALOGUE_INTERVAL;
		s_mapPlayerBotItemShopCoins.clear();
		s_mapPlayerBotItemShopMarks.clear();
		s_vecPlayerBotItemShopHair.clear();

		CItemShopManager& shop = CItemShopManager::instance();
		unsigned int entries = 0;
		for (int index = 1; index <= PLAYERBOT_ISHOP_MAX_INDEX; ++index)
		{
			if (!shop.IsItemExist(index))
				continue;
			TItemShopItem item = shop.GetItem(index);
			if (!shop.IsProperItem(item) || item.dwCount == 0 || shop.IsItemTimeAuction(item))
				continue;
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(item.dwVnum);
			if (!proto)
				continue;
			++entries;
			TPlayerBotItemShopEntry entry;
			entry.dwIndex = item.dwIndex;
			entry.dwPrice = shop.GetItemPrice(item);
			entry.dwCount = item.dwCount;
			entry.bMinLevel = item.bMinLevel;
			const bool marks = shop.GetItemCurrency(item) == CItemShopManager::CURRENCY_DRAGON_MARK;
			std::map<DWORD, TPlayerBotItemShopEntry>& table = marks ? s_mapPlayerBotItemShopMarks : s_mapPlayerBotItemShopCoins;
			std::map<DWORD, TPlayerBotItemShopEntry>::iterator it = table.find(item.dwVnum);
			// The cheapest unit: "Zaczarowanie x4 for 207" beats "x1 for 69"
			// only when the bot can afford four, and it buys one at a time.
			if (it == table.end() || entry.dwPrice * it->second.dwCount < it->second.dwPrice * entry.dwCount)
				table[item.dwVnum] = entry;
			if (!marks && proto->bType == ITEM_COSTUME && proto->bSubType == COSTUME_HAIR)
				s_vecPlayerBotItemShopHair.push_back(item.dwVnum);
		}
		if (entries == 0)
		{
			// The db core has not sent the catalogue yet: ask again soon.
			s_dwNextPlayerBotItemShopScan = dwNow + 5 * 60 * 1000;
			return;
		}
		sys_log(0, "PLAYERBOT_ISHOP: catalogue entries=%u coins=%u marks=%u hairstyles=%u",
				entries, (unsigned int)s_mapPlayerBotItemShopCoins.size(),
				(unsigned int)s_mapPlayerBotItemShopMarks.size(), (unsigned int)s_vecPlayerBotItemShopHair.size());
	}

	void RefreshPlayerBotDragonBalance(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		const CItemShopManager::PlayerAccountBalance balance = CItemShopManager::instance().GetPlayerAccountBalance(ch);
		state.iDragonCoins = (int)std::min<DWORD>(balance.dwDragonCoins, INT_MAX);
		state.iDragonMarks = (int)std::min<DWORD>(balance.dwDragonMarks, INT_MAX);
		state.bDragonBalanceKnown = true;
		state.dwNextItemShopBalanceTime = dwNow + PLAYERBOT_ISHOP_BALANCE_INTERVAL;
	}

	LPITEM FindPlayerBotVoucher(LPCHARACTER ch)
	{
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && !item->isLocked() && !item->IsExchanging() &&
					item->GetVnum() >= PLAYERBOT_ISHOP_VOUCHER_MIN_VNUM &&
					item->GetVnum() <= PLAYERBOT_ISHOP_VOUCHER_MAX_VNUM)
				return item;
		}
		return NULL;
	}

	// One voucher a pass, the way the package's quest cashes one for a player:
	// the charge goes through the db core (ChargeCash: cash += value), the row
	// in itemshop_dragon_scroll is the same, and the voucher goes. The quest
	// itself is not run - item.remove() there takes the whole stack for one
	// charge, and a dialog-free UseItem would still cost the bot the flag the
	// quest checks; the flag is honoured here.
	bool UsePlayerBotVoucher(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		LPITEM voucher = FindPlayerBotVoucher(ch);
		if (!voucher)
			return false;
		if (quest::CQuestManager::instance().GetEventFlag("block_dragon_voucher") > 0)
			return false;
		const int value = voucher->GetValue(0);
		if (value <= 0)
			return false;
		const DWORD id = voucher->GetID();
		if (!CItemShopManager::instance().AddCash(ch, ERequestCharge_Cash, value, false))
			return false;
		LogManager::instance().Query("INSERT INTO itemshop_dragon_scroll VALUES (%u, %u, NOW(), %u, %d)",
				ch->GetPlayerID(), ch->GetDesc()->GetAccountTable().id, id, value);
		if (voucher->GetCount() > 1)
			voucher->SetCount(voucher->GetCount() - 1);
		else
			ITEM_MANAGER::instance().RemoveItem(voucher, "PLAYERBOT_VOUCHER");
		state.iDragonCoins += value;
		++s_uPlayerBotVouchersUsed;
		s_uPlayerBotCoinsCharged += (unsigned int)value;
		sys_log(0, "PLAYERBOT_ISHOP: voucher cashed pid=%u name=%s coins=%d balance=%d",
				ch->GetPlayerID(), ch->GetName(), value, state.iDragonCoins);
		return true;
	}

	// --------------------------------------------------------------- wishes

	struct TPlayerBotItemShopWish
	{
		DWORD dwVnum;
		bool bMarks;
		const char* szReason;
	};

	// A Grand Master skill the training pass could train now, and no stone.
	bool PlayerBotWantsGrandMasterStone(LPCHARACTER ch)
	{
		if (ch->GetSkillGroup() == 0)
			return false;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetVnum() == PLAYERBOT_GRAND_MASTER_STONE_VNUM)
				return false;
		}
		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
		for (BYTE i = 0; i < build.bSkillCount; ++i)
		{
			const DWORD vnum = build.dwSkills[i];
			if (vnum == 0 || ch->GetSkillMasterType(vnum) != SKILL_GRAND_MASTER)
				continue;
			const int level = ch->GetSkillLevel(vnum);
			if (level < 30 || level >= 40)
				continue;
			const int cost = 1000 + 500 * (level - 30);
			if (ch->GetRealAlignment() - cost >= 0)
				return true;
		}
		return false;
	}

	// The worn weapon the reroll pass would reroll if it had a stone, and no
	// stone in the bag: the same tests ManagePlayerBotBonusReroll makes.
	bool PlayerBotWantsChangeStone(LPCHARACTER ch)
	{
		if (ch->GetLevel() < PLAYERBOT_BONUS_MIN_LEVEL)
			return false;
		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		if (!CanPlayerBotRerollItem(weapon))
			return false;
		if (weapon->GetAttributeCount() < PLAYERBOT_BONUS_MAX_LINES ||
				weapon->GetRefineLevel() < PLAYERBOT_BONUS_CHANGE_MIN_REFINE)
			return false;
		if (HasPlayerBotFinishedBonus(ch, weapon, WEAR_WEAPON))
			return false;
		if (ScorePlayerBotItemBonuses(ch, weapon, WEAR_WEAPON) >= PLAYERBOT_BONUS_KEEP_SCORE &&
				!IsPlayerBotSpecialLevel30WeaponVnum(weapon->GetVnum()))
			return false;
		return !HasPlayerBotBonusStone(ch, PLAYERBOT_BONUS_CHANGE_VNUM);
	}

	bool PlayerBotHoldsBooster(LPCHARACTER ch)
	{
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && IsPlayerBotBoosterItem(item))
				return true;
		}
		return false;
	}

	// One bot in PLAYERBOT_ISHOP_HAIR_SHARE, once, from the level the shop
	// sells them at: a head of hair it can wear (the anti-flags name the class
	// and the sex), chosen by pid among what the catalogue holds.
	DWORD PickPlayerBotHairstyle(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		if (state.bBoughtHairstyle || ch->GetLevel() < PLAYERBOT_ISHOP_HAIR_MIN_LEVEL ||
				ch->GetWear(WEAR_COSTUME_HAIR) != NULL || s_vecPlayerBotItemShopHair.empty())
			return 0;
		if ((PlayerBotNavHash(ch->GetPlayerID() ^ 0x48414952U) % PLAYERBOT_ISHOP_HAIR_SHARE) != 0)
			return 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetType() == ITEM_COSTUME && item->GetSubType() == COSTUME_HAIR &&
					item->CanUsedBy(ch))
				return 0;
		}
		std::vector<DWORD> mine;
		for (size_t i = 0; i < s_vecPlayerBotItemShopHair.size(); ++i)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(s_vecPlayerBotItemShopHair[i]);
			if (proto && IsPlayerBotProtoForCharacter(ch, proto))
				mine.push_back(s_vecPlayerBotItemShopHair[i]);
		}
		if (mine.empty())
			return 0;
		return mine[PlayerBotNavHash(ch->GetPlayerID() ^ 0x48414953U) % mine.size()];
	}

	// A head for the counter (PLAYERBOT_ISHOP_HAIR_TRADE_SHARE): one this bot
	// cannot wear, so the pass that dresses it never takes it, bought by a
	// keeper with a stand and none on the way already - in the bag or on the
	// counter.
	DWORD PickPlayerBotHairstyleForCounter(LPCHARACTER ch)
	{
		if (!ch || s_vecPlayerBotItemShopHair.empty() || !PlayerBotCanOpenShop(ch) ||
				(PlayerBotNavHash(ch->GetPlayerID() ^ 0x48545244U) % PLAYERBOT_ISHOP_HAIR_TRADE_SHARE) != 0)
			return 0;
#if defined(ENABLE_IKASHOP_RENEWAL)
		auto shop = ikashop::GetManager().GetShopByOwnerID(ch->GetPlayerID());
		if (!shop)
			return 0;
		for (const auto& [id, line] : shop->GetItems())
			if (line && line->GetTable() && line->GetTable()->bType == ITEM_COSTUME &&
					line->GetTable()->bSubType == COSTUME_HAIR)
				return 0;
#endif
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetType() == ITEM_COSTUME && item->GetSubType() == COSTUME_HAIR &&
					!item->CanUsedBy(ch))
				return 0;
		}
		std::vector<DWORD> others;
		for (size_t i = 0; i < s_vecPlayerBotItemShopHair.size(); ++i)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(s_vecPlayerBotItemShopHair[i]);
			if (proto && !IsPlayerBotProtoForCharacter(ch, proto))
				others.push_back(s_vecPlayerBotItemShopHair[i]);
		}
		if (others.empty())
			return 0;
		return others[PlayerBotNavHash(ch->GetPlayerID() ^ (DWORD)(get_global_time() / 3600)) % others.size()];
	}

	// Every wish the bot has, in the order it would spend on them. The buyer
	// takes the first it can afford: the first build picked one wish only,
	// and a bot with no marks whose first wish was the marks' Blessing Scroll
	// never got as far as the hairstyle its coins would have bought.
	const int PLAYERBOT_ISHOP_MAX_WISHES = 5;

	int CollectPlayerBotItemShopWishes(LPCHARACTER ch, const TPlayerBotAIState& state, TPlayerBotItemShopWish* wishes)
	{
		int n = 0;
		if (PlayerBotWantsGrandMasterStone(ch))
		{
			wishes[n].dwVnum = PLAYERBOT_GRAND_MASTER_STONE_VNUM;
			wishes[n].bMarks = false;
			wishes[n++].szReason = "grand_master_stone";
		}
		if (PlayerBotWantsChangeStone(ch))
		{
			wishes[n].dwVnum = PLAYERBOT_BONUS_CHANGE_VNUM;
			wishes[n].bMarks = false;
			wishes[n++].szReason = "change_stone";
		}
		if (PlayerBotWearsScrollWork(ch) && CountPlayerBotSafeRefineScrolls(ch) == 0 &&
				s_mapPlayerBotItemShopMarks.find(PLAYERBOT_ISHOP_BLESSING_SCROLL_VNUM) != s_mapPlayerBotItemShopMarks.end())
		{
			wishes[n].dwVnum = PLAYERBOT_ISHOP_BLESSING_SCROLL_VNUM;
			wishes[n].bMarks = true;
			wishes[n++].szReason = "blessing_scroll";
		}
		if (!PlayerBotHoldsBooster(ch) &&
				s_mapPlayerBotItemShopMarks.find(PLAYERBOT_ISHOP_ATTACK_POTION_VNUM) != s_mapPlayerBotItemShopMarks.end())
		{
			wishes[n].dwVnum = PLAYERBOT_ISHOP_ATTACK_POTION_VNUM;
			wishes[n].bMarks = true;
			wishes[n++].szReason = "attack_potion";
		}
		const DWORD hair = PickPlayerBotHairstyle(ch, state);
		if (hair != 0)
		{
			wishes[n].dwVnum = hair;
			wishes[n].bMarks = false;
			wishes[n++].szReason = "hairstyle";
		}
		// Its own come first; a head for the counter only when nothing else
		// is wanted, or the coins saved for a Kamien Duchowy would go on it.
		if (n == 0)
		{
			const DWORD stock = PickPlayerBotHairstyleForCounter(ch);
			if (stock != 0)
			{
				wishes[n].dwVnum = stock;
				wishes[n].bMarks = false;
				wishes[n++].szReason = "counter_hairstyle";
			}
		}
		return n;
	}

	const TPlayerBotItemShopEntry* FindPlayerBotItemShopEntry(const TPlayerBotItemShopWish& wish)
	{
		const std::map<DWORD, TPlayerBotItemShopEntry>& table = wish.bMarks ? s_mapPlayerBotItemShopMarks : s_mapPlayerBotItemShopCoins;
		std::map<DWORD, TPlayerBotItemShopEntry>::const_iterator it = table.find(wish.dwVnum);
		return it == table.end() ? NULL : &it->second;
	}

	bool CanPlayerBotAffordWish(LPCHARACTER ch, const TPlayerBotAIState& state, const TPlayerBotItemShopWish& wish)
	{
		const TPlayerBotItemShopEntry* entry = FindPlayerBotItemShopEntry(wish);
		if (!entry || ch->GetLevel() < entry->bMinLevel)
			return false;
		const int have = wish.bMarks ? state.iDragonMarks : state.iDragonCoins;
		return have >= (int)entry->dwPrice;
	}

	// ------------------------------------------------------------- purchase

	// A bought hairstyle goes on when the engine lets it: EquipItem refuses
	// within a second and a half of a blow or a cast, and the bot bought it
	// mid-hunt, so the pass asks again on every look while a bare head has a
	// hairstyle in the bag.
	void WearPlayerBotBoughtHairstyle(LPCHARACTER ch, TPlayerBotAIState& state)
	{
		if (ch->GetWear(WEAR_COSTUME_HAIR) != NULL)
			return;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetType() != ITEM_COSTUME || item->GetSubType() != COSTUME_HAIR ||
					item->isLocked() || item->IsExchanging() || !item->CanUsedBy(ch))
				continue;
			state.bBoughtHairstyle = true;
			if (ch->EquipItem(item))
				sys_log(0, "PLAYERBOT_ISHOP: hairstyle on pid=%u name=%s vnum=%u",
						ch->GetPlayerID(), ch->GetName(), item->GetVnum());
			return;
		}
	}

	// The engine's own purchase, as a client makes it: the browse flag the
	// window sets, BuyItem (level, price, the account's balance read back, the
	// charge through the db core, AutoGiveItem, the purchase log), the flag off
	// on the same tick so IsBusy does not hold the rest of the tick.
	bool BuyPlayerBotItemShop(LPCHARACTER ch, TPlayerBotAIState& state, const TPlayerBotItemShopWish& wish, DWORD dwNow)
	{
		const TPlayerBotItemShopEntry* pEntry = FindPlayerBotItemShopEntry(wish);
		if (!pEntry || !CanPlayerBotAffordWish(ch, state, wish))
			return false;
		const TPlayerBotItemShopEntry& entry = *pEntry;
		if (!ch->HasPlayerData() || ch->IsBusy() || !ch->CanHandleItem() || !ch->HasSlotForItem(wish.dwVnum))
		{
			PlayerBotLogThrottled("ishop_not_now", dwNow,
					"PLAYERBOT_ISHOP: cannot buy now pid=%u name=%s vnum=%u reason=%s busy=%d handle=%d slot=%d",
					ch->GetPlayerID(), ch->GetName(), wish.dwVnum, wish.szReason, (int)ch->IsBusy(),
					(int)ch->CanHandleItem(), (int)ch->HasSlotForItem(wish.dwVnum));
			return false;
		}

		ch->playerData->SetItemShopBrowse(true);
		const bool bought = CItemShopManager::instance().BuyItem(ch, (int)entry.dwIndex, 1);
		ch->playerData->SetItemShopBrowse(false);
		state.dwNextItemShopBuyTime = dwNow + PLAYERBOT_ISHOP_BUY_INTERVAL;
		if (!bought)
		{
			++s_uPlayerBotItemShopRefusals;
			// The balance the engine read may not be the one kept here.
			state.bDragonBalanceKnown = false;
			PlayerBotLogThrottled("ishop_refused", dwNow,
					"PLAYERBOT_ISHOP: purchase refused pid=%u name=%s vnum=%u index=%u price=%u %s reason=%s",
					ch->GetPlayerID(), ch->GetName(), wish.dwVnum, entry.dwIndex, entry.dwPrice,
					wish.bMarks ? "marks" : "coins", wish.szReason);
			return false;
		}
		if (wish.bMarks)
			state.iDragonMarks -= (int)entry.dwPrice;
		else
		{
			state.iDragonCoins -= (int)entry.dwPrice;
			state.iDragonMarks += (int)entry.dwPrice;
		}
		++s_uPlayerBotItemShopBuys;
		++s_mapPlayerBotItemShopBought[wish.dwVnum];
		sys_log(0, "PLAYERBOT_ISHOP: bought pid=%u name=%s vnum=%u x%u index=%u price=%u %s reason=%s coins_left=%d marks_left=%d",
				ch->GetPlayerID(), ch->GetName(), wish.dwVnum, entry.dwCount, entry.dwIndex, entry.dwPrice,
				wish.bMarks ? "marks" : "coins", wish.szReason, state.iDragonCoins, state.iDragonMarks);
		if (wish.szReason[0] == 'h')
			WearPlayerBotBoughtHairstyle(ch, state);
		return true;
	}

	// ----------------------------------------------------------------- tick

	// Upkeep, never the tick's owner: a voucher cashed, a purchase made, and
	// the rest of the tick goes on. Every ten minutes a bot; the account is
	// asked for its balance only when the bot wants something.
	void ManagePlayerBotItemShop(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || ch->IsDead() || !ch->GetDesc())
			return;
		if (dwNow < state.dwNextItemShopCheckTime)
			return;
		state.dwNextItemShopCheckTime = dwNow + PLAYERBOT_ISHOP_CHECK_INTERVAL +
				PlayerBotNavHash(ch->GetPlayerID() ^ 0x49534850U) % 60000U;
		if (!IsPlayerBotItemShopEnabled())
			return;
		if (ch->IsBusy() || ch->GetMyShop() || ch->GetExchange())
			return;
		RefreshPlayerBotItemShopCatalogue(dwNow);
		WearPlayerBotBoughtHairstyle(ch, state);

		const bool voucher = FindPlayerBotVoucher(ch) != NULL;
		TPlayerBotItemShopWish wishes[PLAYERBOT_ISHOP_MAX_WISHES];
		const int wants = CollectPlayerBotItemShopWishes(ch, state, wishes);
		if (!voucher && wants == 0)
			return;
		// The account is read before a voucher is cashed, never after: the
		// charge is a round trip through the db core, and a read on its heels
		// would put the old balance back over the coins just added.
		if (!state.bDragonBalanceKnown || dwNow >= state.dwNextItemShopBalanceTime)
			RefreshPlayerBotDragonBalance(ch, state, dwNow);
		// And a purchase waits for the next look for the same reason: BuyItem
		// reads the account itself.
		if (voucher && UsePlayerBotVoucher(ch, state, dwNow))
			return;
		if (dwNow < state.dwNextItemShopBuyTime)
			return;
		for (int i = 0; i < wants; ++i)
		{
			if (!CanPlayerBotAffordWish(ch, state, wishes[i]))
				continue;
			BuyPlayerBotItemShop(ch, state, wishes[i], dwNow);
			return;
		}
		++s_uPlayerBotItemShopSaving;
	}

	void WritePlayerBotItemShopCensus(DWORD dwNow)
	{
		if (s_dwNextPlayerBotItemShopCensus != 0 && dwNow < s_dwNextPlayerBotItemShopCensus)
			return;
		s_dwNextPlayerBotItemShopCensus = dwNow + PLAYERBOT_ISHOP_CENSUS_INTERVAL;
		if (s_uPlayerBotVouchersUsed == 0 && s_uPlayerBotItemShopBuys == 0 && s_uPlayerBotItemShopRefusals == 0 &&
				s_uPlayerBotItemShopSaving == 0)
			return;
		std::string bought;
		char buf[48];
		for (std::map<DWORD, unsigned int>::const_iterator it = s_mapPlayerBotItemShopBought.begin();
				it != s_mapPlayerBotItemShopBought.end(); ++it)
		{
			snprintf(buf, sizeof(buf), "%s%u:%u", bought.empty() ? "" : ",", it->first, it->second);
			bought += buf;
		}
		sys_log(0, "PLAYERBOT_ISHOP: census vouchers=%u coins=%u purchases=%u refused=%u saving_looks=%u bought=%s",
				s_uPlayerBotVouchersUsed, s_uPlayerBotCoinsCharged, s_uPlayerBotItemShopBuys,
				s_uPlayerBotItemShopRefusals, s_uPlayerBotItemShopSaving, bought.empty() ? "-" : bought.c_str());
	}
#else
	void ManagePlayerBotItemShop(LPCHARACTER, TPlayerBotAIState&, DWORD)
	{
	}

	void WritePlayerBotItemShopCensus(DWORD)
	{
	}
#endif
}

#endif
