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
// przeznaczanie na potrzebne zakupy w itemshop" (16 September).
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
// MT2009 Plus (24 September 2026): the look is the whole of it now - every
// bot from level 30 with coins buys, in this order and one at a time, a
// costume, a hairstyle, a weapon skin for its weapon and a pet (no mount),
// wears them (the pet is summoned from its seal), and buys a piece again
// when its time runs out and the slot is empty.
// Tieru 2.2.20 (26 September) added the Exorcism Scroll, Kamien Duchowy and
// the single change stone to what a bot buys; see the wishes below.
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
#if defined(__PET_SYSTEM__)
#include "PetSystem.h"
#endif
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

	// The catalogue as the bots read it: every line per vnum for each
	// currency, and the hairstyles for sale. Rebuilt every hour from the
	// manager, which holds what the db core sent at boot - an operator who
	// edits common.itemshop_items restarts the world anyway.
	//
	// Every line, not the cheapest unit alone: that kept the change stone's
	// four-pack (207 coins) and never the single (69), and four accounts of
	// 418 on m2zip held 207 - so the wish was one nobody could pay for
	// (26 September). The buyer takes the cheapest unit it can afford
	// (FindPlayerBotItemShopEntry).
	typedef std::map<DWORD, std::vector<TPlayerBotItemShopEntry> > TPlayerBotItemShopTable;
	TPlayerBotItemShopTable s_mapPlayerBotItemShopCoins;
	TPlayerBotItemShopTable s_mapPlayerBotItemShopMarks;
	std::vector<DWORD> s_vecPlayerBotItemShopHair;
	// MT2009 Plus: the rest of a bot's look, for coins - costumes, weapon
	// skins and pet seals (PET_PAY); a pet with its own loot is left out, it
	// costs more and a bot's pet never loots (PetSystem.cpp).
	std::vector<DWORD> s_vecPlayerBotItemShopBody;
	std::vector<DWORD> s_vecPlayerBotItemShopWeaponSkin;
	std::vector<DWORD> s_vecPlayerBotItemShopPet;
	std::vector<DWORD> s_vecPlayerBotItemShopMount;
	DWORD s_dwNextPlayerBotItemShopScan = 0;

	// MT2009_PLUS_SIDEKICK_COINS_V1: the player's companion and the shop
	// (playerbot_sidekick.h, later in the include order). Its owner's switch
	// "Smocze Monety: wydaje / nie wydaje" in the window decides, not the
	// world's ISHOP; it buys only what it uses - no look of any kind - and
	// tells its owner every voucher cashed and every purchase.
	bool IsPlayerBotSidekickPID(DWORD pid);
	bool IsPlayerBotSidekickCoinsOn(DWORD pid);
	void NotePlayerBotSidekickVouchers(LPCHARACTER sk, long long coins, int balance);
	void NotePlayerBotSidekickItemShopBuy(LPCHARACTER sk, DWORD vnum, DWORD count, DWORD price, bool marks,
			const char* reason, int left);

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
		s_vecPlayerBotItemShopBody.clear();
		s_vecPlayerBotItemShopWeaponSkin.clear();
		s_vecPlayerBotItemShopPet.clear();
		s_vecPlayerBotItemShopMount.clear();

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
			TPlayerBotItemShopTable& table = marks ? s_mapPlayerBotItemShopMarks : s_mapPlayerBotItemShopCoins;
			table[item.dwVnum].push_back(entry);
			// MT2009_PLUS_BOT_HAIR_V1: only a costume set's hairstyle is a
			// bot's to buy (IsPlayerBotSetHairVnum, playerbot_economy.h).
			if (!marks && proto->bType == ITEM_COSTUME && proto->bSubType == COSTUME_HAIR &&
					IsPlayerBotSetHairVnum(item.dwVnum))
				s_vecPlayerBotItemShopHair.push_back(item.dwVnum);
			if (!marks && proto->bType == ITEM_COSTUME && proto->bSubType == COSTUME_BODY)
				s_vecPlayerBotItemShopBody.push_back(item.dwVnum);
			if (!marks && proto->bType == ITEM_COSTUME && proto->bSubType == COSTUME_WEAPON)
				s_vecPlayerBotItemShopWeaponSkin.push_back(item.dwVnum);
			if (!marks && proto->bType == ITEM_PET && proto->bSubType == PET_PAY && proto->alValues[0] != 0 &&
					proto->alValues[2] == 0)
				s_vecPlayerBotItemShopPet.push_back(item.dwVnum);
#if defined(ENABLE_MOUNT_COSTUME_SYSTEM)
			if (!marks && proto->bType == ITEM_COSTUME && proto->bSubType == COSTUME_MOUNT && proto->alValues[1] != 0)
				s_vecPlayerBotItemShopMount.push_back(item.dwVnum);
#endif
		}
		if (entries == 0)
		{
			// The db core has not sent the catalogue yet: ask again soon.
			s_dwNextPlayerBotItemShopScan = dwNow + 5 * 60 * 1000;
			return;
		}
		sys_log(0, "PLAYERBOT_ISHOP: catalogue entries=%u coins=%u marks=%u hairstyles=%u costumes=%u weapon_skins=%u pets=%u mounts=%u",
				entries, (unsigned int)s_mapPlayerBotItemShopCoins.size(),
				(unsigned int)s_mapPlayerBotItemShopMarks.size(), (unsigned int)s_vecPlayerBotItemShopHair.size(),
				(unsigned int)s_vecPlayerBotItemShopBody.size(), (unsigned int)s_vecPlayerBotItemShopWeaponSkin.size(),
				(unsigned int)s_vecPlayerBotItemShopPet.size(), (unsigned int)s_vecPlayerBotItemShopMount.size());
	}

	void RefreshPlayerBotDragonBalance(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		const CItemShopManager::PlayerAccountBalance balance = CItemShopManager::instance().GetPlayerAccountBalance(ch);
		state.iDragonCoins = (int)std::min<DWORD>(balance.dwDragonCoins, INT_MAX);
		state.iDragonMarks = (int)std::min<DWORD>(balance.dwDragonMarks, INT_MAX);
		state.bDragonBalanceKnown = true;
		state.dwNextItemShopBalanceTime = dwNow + PLAYERBOT_ISHOP_BALANCE_INTERVAL;
	}

	// The saddlebag page too (INVENTORY_MAX_NUM, not PLAYERBOT_BAG_CELLS): a
	// coupon the panel hands a bot with a full bag lands in its open rows.
	LPITEM FindPlayerBotVoucher(LPCHARACTER ch)
	{
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && !item->isLocked() && !item->IsExchanging() &&
					item->GetVnum() >= PLAYERBOT_ISHOP_VOUCHER_MIN_VNUM &&
					item->GetVnum() <= PLAYERBOT_ISHOP_VOUCHER_MAX_VNUM)
				return item;
		}
		return NULL;
	}

	// Every voucher in the bag in one pass, every unit of every stack - the
	// charge the package's quest makes for a player (AddCash through the db
	// core, cash += value, the itemshop_dragon_scroll row), without its
	// dialog. It was one unit a pass, and a pass comes every ten minutes: a
	// stack of 200 took a day and a half, and a bot handed coupons by the
	// panel sat on almost all of them ("boty maja klikac wszystkie kupony SM,
	// a nie tylko 50", operator, 26 September 2026). The quest itself is not
	// run - item.remove() there takes the whole stack for one charge, and a
	// dialog-free UseItem would still cost the bot the flag the quest checks;
	// the flag is honoured here.
	// What a bot's vouchers brought it since this core first cashed one for
	// it: the pace a need it cannot pay for yet is saved up at
	// (PlayerBotSavesForWish). A restart starts the count again.
	struct TPlayerBotCoinIncome
	{
		int iSince;
		long long llCoins;
	};
	std::map<DWORD, TPlayerBotCoinIncome> s_mapPlayerBotCoinIncome;

	void NotePlayerBotCoinIncome(DWORD pid, long long coins)
	{
		std::map<DWORD, TPlayerBotCoinIncome>::iterator it = s_mapPlayerBotCoinIncome.find(pid);
		if (it == s_mapPlayerBotCoinIncome.end())
		{
			TPlayerBotCoinIncome income = { get_global_time(), 0 };
			it = s_mapPlayerBotCoinIncome.insert(std::make_pair(pid, income)).first;
		}
		it->second.llCoins += coins;
	}

	bool UsePlayerBotVoucher(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (quest::CQuestManager::instance().GetEventFlag("block_dragon_voucher") > 0)
			return false;
		int cashed = 0;
		long long total = 0;
		for (LPITEM voucher = FindPlayerBotVoucher(ch); voucher; voucher = FindPlayerBotVoucher(ch))
		{
			const int value = voucher->GetValue(0);
			const int count = (int)voucher->GetCount();
			if (value <= 0 || count <= 0)
				break;
			// One charge for the stack, kept inside the packet's long.
			const long long coins = (long long)value * count;
			if (coins > INT_MAX || !CItemShopManager::instance().AddCash(ch, ERequestCharge_Cash, (long)coins, false))
				break;
			LogManager::instance().Query("INSERT INTO itemshop_dragon_scroll VALUES (%u, %u, NOW(), %u, %lld)",
					ch->GetPlayerID(), ch->GetDesc()->GetAccountTable().id, voucher->GetID(), coins);
			ITEM_MANAGER::instance().RemoveItem(voucher, "PLAYERBOT_VOUCHER");
			state.iDragonCoins = (int)std::min<long long>((long long)state.iDragonCoins + coins, INT_MAX);
			NotePlayerBotCoinIncome(ch->GetPlayerID(), coins);
			s_uPlayerBotVouchersUsed += (unsigned int)count;
			s_uPlayerBotCoinsCharged += (unsigned int)coins;
			cashed += count;
			total += coins;
		}
		if (cashed == 0)
			return false;
		sys_log(0, "PLAYERBOT_ISHOP: voucher cashed pid=%u name=%s vouchers=%d coins=%lld balance=%d",
				ch->GetPlayerID(), ch->GetName(), cashed, total, state.iDragonCoins);
		if (IsPlayerBotSidekickPID(ch->GetPlayerID()))
			NotePlayerBotSidekickVouchers(ch, total, state.iDragonCoins);
		return true;
	}

	// The vouchers, every minute and ahead of the stall: a bot standing at its
	// counter ends its pass in ManagePlayerBotPrivateShop and never reached
	// the ItemShop's upkeep, so the coupons the panel handed out sat in the
	// bags of every stall keeper - 13 cashed of 1578 delivered in half an
	// hour on the test world (26 September 2026). The account is read first
	// when its balance is not known or due, never on the heels of a charge.
	void CashPlayerBotVouchers(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || ch->IsDead() || !ch->GetDesc() || dwNow < state.dwNextVoucherCheckTime)
			return;
		state.dwNextVoucherCheckTime = dwNow + PLAYERBOT_ISHOP_VOUCHER_CHECK_INTERVAL +
				PlayerBotNavHash(ch->GetPlayerID() ^ 0x564f5543U) % 15000U;
		// MT2009_PLUS_SIDEKICK_COINS_V1: a companion by its owner's switch.
		const bool sidekick = IsPlayerBotSidekickPID(ch->GetPlayerID());
		if ((sidekick ? !IsPlayerBotSidekickCoinsOn(ch->GetPlayerID()) : !IsPlayerBotItemShopEnabled()) ||
				ch->GetExchange() || !FindPlayerBotVoucher(ch))
			return;
		if (!state.bDragonBalanceKnown || dwNow >= state.dwNextItemShopBalanceTime)
			RefreshPlayerBotDragonBalance(ch, state, dwNow);
		UsePlayerBotVoucher(ch, state, dwNow);
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
			const int cost = GetPlayerBotGrandMasterRankCost(level);
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
		// Whether an ordinary change would still move it towards its finish,
		// asked the way the pass asks it (PlayerBotWantsBonusChange).
		const TPlayerBotBonusTarget target = { weapon, (BYTE)WEAR_WEAPON, (BYTE)PLAYERBOT_BONUS_TARGET_WORN,
				(BYTE)playerbot_bonus_rules::PLAIN_CATEGORY };
		if (!PlayerBotWantsBonusChange(ch, target, false))
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

	// ------------------------------------------------------------- the look

	// MT2009 Plus (operator, 24 September 2026): a bot with Dragon Coins
	// dresses itself from the ItemShop - costume, then hairstyle, then weapon
	// skin, then pet - one piece at a time, and wears what it bought. Every bot
	// from PLAYERBOT_ISHOP_LOOK_MIN_LEVEL, not one in four once as the
	// hairstyle alone was: a piece that runs out (REAL_TIME) leaves its slot
	// empty, and the next look buys the missing piece again.

	// A weapon skin goes on only over a real weapon of its own kind: the engine
	// compares the skin's value3 with the weapon's subtype (CanEquipNow).
	bool IsPlayerBotWeaponSkinFor(LPCHARACTER ch, const TItemTable* proto)
	{
		if (!proto || proto->bType != ITEM_COSTUME || proto->bSubType != COSTUME_WEAPON ||
				!IsPlayerBotProtoForCharacter(ch, proto))
			return false;
		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		return weapon && weapon->GetType() == ITEM_WEAPON && proto->alValues[3] == (long)weapon->GetSubType();
	}

	// A piece whose level limit the bot has not reached would wait in the bag
	// for good, and a piece in the bag counts as had: never bought, never worn.
	bool IsPlayerBotLookLevelReached(LPCHARACTER ch, const TItemTable* proto)
	{
		for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
			if (proto->aLimits[i].bType == LIMIT_LEVEL && (long)ch->GetLevel() < proto->aLimits[i].lValue)
				return false;
		return true;
	}

	// Would this bot put the item on (or summon it) for the given piece?
	bool IsPlayerBotOwnLook(LPCHARACTER ch, const TItemTable* proto, int look)
	{
		if (!proto || !IsPlayerBotLookLevelReached(ch, proto))
			return false;
		switch (look)
		{
			case PLAYERBOT_ISHOP_LOOK_BODY:
				return proto->bType == ITEM_COSTUME && proto->bSubType == COSTUME_BODY &&
						IsPlayerBotProtoForCharacter(ch, proto);
			case PLAYERBOT_ISHOP_LOOK_HAIR:
				// MT2009_PLUS_BOT_HAIR_V1: a costume set's hairstyle only.
				return proto->bType == ITEM_COSTUME && proto->bSubType == COSTUME_HAIR &&
						IsPlayerBotProtoForCharacter(ch, proto) && IsPlayerBotSetHairVnum(proto->dwVnum);
			case PLAYERBOT_ISHOP_LOOK_WEAPON:
				return IsPlayerBotWeaponSkinFor(ch, proto);
			case PLAYERBOT_ISHOP_LOOK_PET:
				return proto->bType == ITEM_PET && proto->bSubType == PET_PAY && proto->alValues[0] != 0;
#if defined(ENABLE_MOUNT_COSTUME_SYSTEM)
			case PLAYERBOT_ISHOP_LOOK_MOUNT:
				return proto->bType == ITEM_COSTUME && proto->bSubType == COSTUME_MOUNT && proto->alValues[1] != 0;
#endif
		}
		return false;
	}

	bool IsPlayerBotPetSummoned(LPCHARACTER ch)
	{
#if defined(__PET_SYSTEM__)
		return ch->GetPetSystem() && ch->GetPetSystem()->CountSummoned() > 0;
#else
		return false;
#endif
	}

	// The piece is worn (summoned), or on its way: bought and in the bag.
	bool PlayerBotHasLook(LPCHARACTER ch, int look)
	{
		switch (look)
		{
			case PLAYERBOT_ISHOP_LOOK_BODY:
				if (ch->GetWear(WEAR_COSTUME_BODY))
					return true;
				break;
			case PLAYERBOT_ISHOP_LOOK_HAIR:
			{
				// MT2009_PLUS_BOT_HAIR_V1: a head is dressed while the ItemShop
				// sells nothing better for it (IsPlayerBotHairUpgrade) - a plain
				// hairstyle is replaced by a set one, a set one by the worn
				// costume's own set's - and a better one in the bag is on its
				// way; the hairstyle it replaced, in the bag too, is not.
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
					if (IsPlayerBotHairToWear(ch, ch->GetInventoryItem(cell)) &&
							IsPlayerBotOwnLook(ch, ch->GetInventoryItem(cell)->GetProto(), look))
						return true;
				if (!ch->GetWear(WEAR_COSTUME_HAIR))
					return false;
				for (size_t i = 0; i < s_vecPlayerBotItemShopHair.size(); ++i)
					if (IsPlayerBotHairUpgrade(ch, s_vecPlayerBotItemShopHair[i]) &&
							IsPlayerBotOwnLook(ch, ITEM_MANAGER::instance().GetTable(s_vecPlayerBotItemShopHair[i]), look))
						return false;
				return true;
			}
			case PLAYERBOT_ISHOP_LOOK_WEAPON:
			{
				if (ch->GetWear(WEAR_COSTUME_WEAPON))
					return true;
				// No real weapon to dress: nothing to buy for now.
				LPITEM weapon = ch->GetWear(WEAR_WEAPON);
				if (!weapon || weapon->GetType() != ITEM_WEAPON)
					return true;
				break;
			}
			case PLAYERBOT_ISHOP_LOOK_PET:
				if (IsPlayerBotPetSummoned(ch))
					return true;
				break;
			case PLAYERBOT_ISHOP_LOOK_MOUNT:
#if defined(ENABLE_MOUNT_COSTUME_SYSTEM)
				if (ch->GetWear(WEAR_COSTUME_MOUNT))
					return true;
				break;
#else
				return true;
#endif
		}
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && IsPlayerBotOwnLook(ch, item->GetProto(), look))
				return true;
		}
		return false;
	}

	const std::vector<DWORD>& GetPlayerBotLookCatalogue(int look)
	{
		switch (look)
		{
			case PLAYERBOT_ISHOP_LOOK_BODY: return s_vecPlayerBotItemShopBody;
			case PLAYERBOT_ISHOP_LOOK_HAIR: return s_vecPlayerBotItemShopHair;
			case PLAYERBOT_ISHOP_LOOK_WEAPON: return s_vecPlayerBotItemShopWeaponSkin;
			case PLAYERBOT_ISHOP_LOOK_MOUNT: return s_vecPlayerBotItemShopMount;
			default: return s_vecPlayerBotItemShopPet;
		}
	}

	const char* GetPlayerBotLookReason(int look)
	{
		switch (look)
		{
			case PLAYERBOT_ISHOP_LOOK_BODY: return "look_costume";
			case PLAYERBOT_ISHOP_LOOK_HAIR: return "hairstyle";
			case PLAYERBOT_ISHOP_LOOK_WEAPON: return "look_weapon_skin";
			case PLAYERBOT_ISHOP_LOOK_MOUNT: return "look_mount";
			default: return "look_pet";
		}
	}

	// The first piece missing, in the operator's order; 0 when the bot is
	// dressed or its piece is not for sale. The strict order is the point: a
	// bot saves for the costume before it spends on a hairstyle. Which item
	// of the kind is drawn at random among those the bot can wear.
	DWORD PickPlayerBotLook(LPCHARACTER ch, int* pLook)
	{
		if (ch->GetLevel() < PLAYERBOT_ISHOP_LOOK_MIN_LEVEL)
			return 0;
		for (int look = 0; look < PLAYERBOT_ISHOP_LOOK_COUNT; ++look)
		{
			if (PlayerBotHasLook(ch, look))
				continue;
			const std::vector<DWORD>& catalogue = GetPlayerBotLookCatalogue(look);
			std::vector<DWORD> mine;
			for (size_t i = 0; i < catalogue.size(); ++i)
				if (IsPlayerBotOwnLook(ch, ITEM_MANAGER::instance().GetTable(catalogue[i]), look) &&
						(look != PLAYERBOT_ISHOP_LOOK_HAIR || IsPlayerBotHairUpgrade(ch, catalogue[i])))
					mine.push_back(catalogue[i]);
			if (mine.empty())
				continue;
			// MT2009_PLUS_BOT_HAIR_V1: the worn costume's own set's hairstyle,
			// when the ItemShop has one - the set bonus is the point of it.
			if (look == PLAYERBOT_ISHOP_LOOK_HAIR)
			{
				LPITEM body = ch->GetWear(WEAR_COSTUME_BODY);
				std::vector<DWORD> matching;
				for (size_t i = 0; body && i < mine.size(); ++i)
					if (IsPlayerBotHairOfBodySet(body->GetVnum(), mine[i]))
						matching.push_back(mine[i]);
				if (!matching.empty())
					mine.swap(matching);
			}
			*pLook = look;
			// Any of them, drawn anew each time (operator: variety), so a
			// piece that runs out is followed by another.
			return mine[number(0, (int)mine.size() - 1)];
		}
		return 0;
	}

	// A head for the counter (PLAYERBOT_ISHOP_HAIR_TRADE_SHARE) was one this
	// bot could not wear, bought for its stand.
	// MT2009_PLUS_BOT_HAIR_V1: never any more - a hairstyle is no goods; the
	// only one a counter takes is the bot's own old one (IsPlayerBotReplacedHair).
	DWORD PickPlayerBotHairstyleForCounter(LPCHARACTER ch)
	{
		(void)ch;
		return 0;
	}

	// The book pass's own two answers (playerbot_manager.cpp, after this file).
	int FindPlayerBotBookAffectCell(LPCHARACTER ch, DWORD affectType);
	bool PlayerBotHasBookReadExp(LPCHARACTER ch);
	bool PlayerBotMarksCover(const TPlayerBotAIState& state, DWORD vnum);

	// A class book in the bag the bot could read but for the wait, with at
	// least PLAYERBOT_ISHOP_EXORCISM_MIN_WAIT_SECONDS of it left, and no
	// Exorcism Scroll of its own: the scroll the book pass already uses when it
	// has one. Nothing on a world whose bots do not wait (the easy difficulty).
	bool PlayerBotWantsExorcismScroll(LPCHARACTER ch)
	{
		if (IsPlayerBotFastBooksEnabled() || ch->GetSkillGroup() == 0 ||
				ch->FindAffect(AFFECT_SKILL_NO_BOOK_DELAY) ||
				FindPlayerBotBookAffectCell(ch, AFFECT_SKILL_NO_BOOK_DELAY) >= 0 ||
				!PlayerBotHasBookReadExp(ch))
			return false;
		const int now = get_global_time();
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetType() != ITEM_SKILLBOOK)
				continue;
			const DWORD skill = GetPlayerBotSkillBookSkillVnum(item);
			if (!IsPlayerBotOwnSkill(ch, skill) || ch->GetSkillMasterType(skill) != SKILL_MASTER ||
					ch->GetSkillLevel(skill) < 20 || ch->GetSkillLevel(skill) >= 30)
				continue;
			if ((int)ch->GetSkillNextReadTime(skill) - now >= PLAYERBOT_ISHOP_EXORCISM_MIN_WAIT_SECONDS)
				return true;
		}
		return false;
	}

	// Every wish the bot has, in the order it would spend on them. The buyer
	// takes the first it can afford: the first build picked one wish only,
	// and a bot with no marks whose first wish was the marks' Blessing Scroll
	// never got as far as the hairstyle its coins would have bought.
	const int PLAYERBOT_ISHOP_MAX_WISHES = 14;

	// ItemShop by needs (sosen94: on the test world 346 of 347 purchases in a
	// week were hairstyles). Besides the stone, the change stone and the
	// Exorcism Scroll: a Metin detector for a stone hunter, the Teleport Ring
	// on the frontier, and for marks the Rada Pustelnika for a book waiting
	// to be read and the boosters for a raid, the tower and a war - what the
	// bot is doing now first. A need the balance cannot pay yet is saved for
	// when the bot's vouchers would bring it within
	// PLAYERBOT_ISHOP_SAVE_DAYS, and the look is bought only from what is
	// over the needs' reserve.
	const DWORD PLAYERBOT_ISHOP_METIN_DETECTOR_VNUM = 27989;
	const DWORD PLAYERBOT_ISHOP_RADA_VNUM = 71094;
	const DWORD PLAYERBOT_ISHOP_BOOSTER_VNUMS[] = { 71028, 71044, 71027, 71045, 71030 };
	const int PLAYERBOT_ISHOP_SAVE_DAYS = 10;

	bool PlayerBotHoldsReadableClassBook(LPCHARACTER ch);
	bool IsPlayerBotFrontierMapIndex(long mapIndex);

	// The cheapest line of a wish the bot's level may buy, whatever the
	// balance; 0 when the shop has none.
	DWORD GetPlayerBotItemShopCheapest(LPCHARACTER ch, DWORD vnum, bool marks)
	{
		const TPlayerBotItemShopTable& table = marks ? s_mapPlayerBotItemShopMarks : s_mapPlayerBotItemShopCoins;
		TPlayerBotItemShopTable::const_iterator it = table.find(vnum);
		DWORD best = 0;
		if (it != table.end())
			for (size_t i = 0; i < it->second.size(); ++i)
				if (it->second[i].dwCount != 0 && ch->GetLevel() >= it->second[i].bMinLevel &&
						(best == 0 || it->second[i].dwPrice < best))
					best = it->second[i].dwPrice;
		return best;
	}

	// Whether a need the balance cannot pay yet is worth saving for: the
	// coins this bot's vouchers have brought, at their pace, would cover it
	// within PLAYERBOT_ISHOP_SAVE_DAYS.
	bool PlayerBotSavesForWish(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD price)
	{
		if (price == 0)
			return false;
		const long long have = state.iDragonCoins;
		if (have >= (long long)price)
			return true;
		std::map<DWORD, TPlayerBotCoinIncome>::const_iterator it = s_mapPlayerBotCoinIncome.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotCoinIncome.end() || it->second.llCoins <= 0)
			return false;
		const long long elapsed = std::max<long long>(86400, (long long)get_global_time() - it->second.iSince);
		return it->second.llCoins * PLAYERBOT_ISHOP_SAVE_DAYS * 86400LL / elapsed >= (long long)price - have;
	}

	bool PlayerBotHoldsAnyOf(LPCHARACTER ch, const DWORD* vnums, size_t count)
	{
		for (size_t i = 0; i < count; ++i)
			if (ch->CountSpecifyItem(vnums[i]) > 0)
				return true;
		return false;
	}

	// The coins the needs keep back from the look: each coin need the bot
	// can pay or is saving for, at its cheapest line.
	long long GetPlayerBotItemShopNeedReserve(LPCHARACTER ch, const TPlayerBotAIState& state,
			const TPlayerBotItemShopWish* wishes, int n)
	{
		long long reserve = 0;
		for (int i = 0; i < n; ++i)
		{
			if (wishes[i].bMarks)
				continue;
			const DWORD price = GetPlayerBotItemShopCheapest(ch, wishes[i].dwVnum, false);
			if (PlayerBotSavesForWish(ch, state, price))
				reserve += price;
		}
		return reserve;
	}

	// A look - hairstyle, costume, skin or pet - only from the coins over the
	// needs' reserve.
	bool PlayerBotLookFromSurplus(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD vnum, long long reserve)
	{
		const DWORD price = GetPlayerBotItemShopCheapest(ch, vnum, false);
		return price != 0 && (long long)state.iDragonCoins - (long long)price >= reserve;
	}

	int CollectPlayerBotItemShopNeeds(LPCHARACTER ch, const TPlayerBotAIState& state, TPlayerBotItemShopWish* wishes)
	{
		int n = 0;
		const DWORD now = get_dword_time();
		// What the bot is doing now goes first: a raid, the tower or a war,
		// a stone hunt, the frontier, a book waiting.
		const bool fighting = IsPlayerBotOnTowerBusiness(ch, state) || state.dwGuildWarEnemyGID != 0;
		const bool hunting = IsPlayerBotMetinHunting(state, now);
		const bool frontier = IsPlayerBotFrontierMapIndex(ch->GetMapIndex());
		TPlayerBotItemShopWish now_[PLAYERBOT_ISHOP_MAX_WISHES];
		TPlayerBotItemShopWish later[PLAYERBOT_ISHOP_MAX_WISHES];
		int nNow = 0, nLater = 0;
		auto add = [&](DWORD vnum, bool marks, const char* reason, bool current)
		{
			TPlayerBotItemShopWish w;
			w.dwVnum = vnum;
			w.bMarks = marks;
			w.szReason = reason;
			if (current && nNow < PLAYERBOT_ISHOP_MAX_WISHES)
				now_[nNow++] = w;
			else if (!current && nLater < PLAYERBOT_ISHOP_MAX_WISHES)
				later[nLater++] = w;
		};
		if (fighting && !PlayerBotHoldsBooster(ch))
			for (size_t i = 0; i < sizeof(PLAYERBOT_ISHOP_BOOSTER_VNUMS) / sizeof(PLAYERBOT_ISHOP_BOOSTER_VNUMS[0]); ++i)
				if (PlayerBotMarksCover(state, PLAYERBOT_ISHOP_BOOSTER_VNUMS[i]))
				{
					add(PLAYERBOT_ISHOP_BOOSTER_VNUMS[i], true, "raid_booster", true);
					break;
				}
		if (hunting && !PlayerBotHoldsAnyOf(ch, PLAYERBOT_METIN_DETECTOR_VNUMS,
				sizeof(PLAYERBOT_METIN_DETECTOR_VNUMS) / sizeof(PLAYERBOT_METIN_DETECTOR_VNUMS[0])))
			add(PLAYERBOT_ISHOP_METIN_DETECTOR_VNUM, false, "metin_detector", true);
		if (frontier && ch->GetLevel() >= PLAYERBOT_TELEPORT_RING_MIN_LEVEL &&
				ch->CountSpecifyItem(PLAYERBOT_TELEPORT_RING_VNUM) == 0)
			add(PLAYERBOT_TELEPORT_RING_VNUM, false, "teleport_ring", true);
		const bool bookWaits = PlayerBotHoldsReadableClassBook(ch);
		if (s_mapPlayerBotItemShopCoins.find(PLAYERBOT_ISHOP_EXORCISM_VNUM) != s_mapPlayerBotItemShopCoins.end() &&
				PlayerBotWantsExorcismScroll(ch))
			add(PLAYERBOT_ISHOP_EXORCISM_VNUM, false, "exorcism_scroll", true);
		if (bookWaits && !ch->FindAffect(AFFECT_SKILL_BOOK_BONUS) &&
				FindPlayerBotBookAffectCell(ch, AFFECT_SKILL_BOOK_BONUS) < 0 &&
				PlayerBotMarksCover(state, PLAYERBOT_ISHOP_RADA_VNUM))
			add(PLAYERBOT_ISHOP_RADA_VNUM, true, "rada_pustelnika", true);
		if (PlayerBotWantsGrandMasterStone(ch))
			add(PLAYERBOT_GRAND_MASTER_STONE_VNUM, false, "grand_master_stone", false);
		if (PlayerBotWantsChangeStone(ch))
			add(PLAYERBOT_BONUS_CHANGE_VNUM, false, "change_stone", false);
		if (PlayerBotWearsScrollWork(ch) && CountPlayerBotSafeRefineScrolls(ch) == 0 &&
				PlayerBotMarksCover(state, PLAYERBOT_ISHOP_BLESSING_SCROLL_VNUM))
			add(PLAYERBOT_ISHOP_BLESSING_SCROLL_VNUM, true, "blessing_scroll", false);
		for (int i = 0; i < nNow && n < PLAYERBOT_ISHOP_MAX_WISHES; ++i)
			wishes[n++] = now_[i];
		for (int i = 0; i < nLater && n < PLAYERBOT_ISHOP_MAX_WISHES; ++i)
			wishes[n++] = later[i];
		return n;
	}

	int CollectPlayerBotItemShopWishes(LPCHARACTER ch, const TPlayerBotAIState& state, TPlayerBotItemShopWish* wishes)
	{
		int n = CollectPlayerBotItemShopNeeds(ch, state, wishes);
		// MT2009_PLUS_SIDEKICK_COINS_V1: a companion spends on what it uses
		// alone - no hairstyle, costume, skin or pet, and nothing for a counter.
		if (IsPlayerBotSidekickPID(ch->GetPlayerID()))
			return n;
		const long long reserve = GetPlayerBotItemShopNeedReserve(ch, state, wishes, n);
		int look = 0;
		const DWORD lookVnum = PickPlayerBotLook(ch, &look);
		if (lookVnum != 0 && n < PLAYERBOT_ISHOP_MAX_WISHES && PlayerBotLookFromSurplus(ch, state, lookVnum, reserve))
		{
			wishes[n].dwVnum = lookVnum;
			wishes[n].bMarks = false;
			wishes[n++].szReason = GetPlayerBotLookReason(look);
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

	// The line a wish is bought from: of the lines the balance and the level
	// allow, the cheapest unit. NULL when none of them is affordable.
	const TPlayerBotItemShopEntry* FindPlayerBotItemShopEntry(LPCHARACTER ch, const TPlayerBotAIState& state,
			const TPlayerBotItemShopWish& wish)
	{
		const TPlayerBotItemShopTable& table = wish.bMarks ? s_mapPlayerBotItemShopMarks : s_mapPlayerBotItemShopCoins;
		TPlayerBotItemShopTable::const_iterator it = table.find(wish.dwVnum);
		if (it == table.end())
			return NULL;
		const int have = wish.bMarks ? state.iDragonMarks : state.iDragonCoins;
		const TPlayerBotItemShopEntry* best = NULL;
		for (size_t i = 0; i < it->second.size(); ++i)
		{
			const TPlayerBotItemShopEntry& line = it->second[i];
			if (line.dwCount == 0 || ch->GetLevel() < line.bMinLevel || have < (int)line.dwPrice)
				continue;
			if (!best || (unsigned long long)line.dwPrice * best->dwCount <
					(unsigned long long)best->dwPrice * line.dwCount)
				best = &line;
		}
		return best;
	}

	bool CanPlayerBotAffordWish(LPCHARACTER ch, const TPlayerBotAIState& state, const TPlayerBotItemShopWish& wish)
	{
		return FindPlayerBotItemShopEntry(ch, state, wish) != NULL;
	}

	// A marks wish only while the balance covers its cheapest line. Marks come
	// only from coins spent, one for one, so every mark in the world came from
	// a hairstyle - the largest balance on m2zip was 117 against the Blessing
	// Scroll's 719 and the potions' 249 - and the two marks wishes stood for
	// nearly every bot of thirty: an hourly account read for nothing (36 282
	// "saving" looks in eight hours) and the counter's hairstyle, which waits
	// for a bot with no other wish, blocked for good. The balance is kept in
	// the state and a bot's own purchases move it, so this asks no query.
	bool PlayerBotMarksCover(const TPlayerBotAIState& state, DWORD vnum)
	{
		if (!state.bDragonBalanceKnown)
			return false;
		TPlayerBotItemShopTable::const_iterator it = s_mapPlayerBotItemShopMarks.find(vnum);
		if (it == s_mapPlayerBotItemShopMarks.end())
			return false;
		for (size_t i = 0; i < it->second.size(); ++i)
			if (state.iDragonMarks >= (int)it->second[i].dwPrice)
				return true;
		return false;
	}

	// ------------------------------------------------------------- purchase

	// A bought piece goes on when the engine lets it: EquipItem refuses within
	// a second and a half of a blow or a cast, and the bot bought it mid-hunt,
	// so the pass asks again on every look while a slot is empty and its piece
	// waits in the bag. A pet seal is used once no pet is out: using it again
	// would send the pet away (CHARACTER::SummonPetFromItem is a toggle).
	void WearPlayerBotBoughtLook(LPCHARACTER ch, TPlayerBotAIState& state)
	{
		static const BYTE s_abWear[PLAYERBOT_ISHOP_LOOK_PET] = { WEAR_COSTUME_BODY, WEAR_COSTUME_HAIR, WEAR_COSTUME_WEAPON };
		for (int look = 0; look < PLAYERBOT_ISHOP_LOOK_COUNT; ++look)
		{
			// MT2009_PLUS_BOT_HAIR_V1: a better hairstyle goes on over the worn
			// one (EquipItem swaps them); the old one, in the bag, is the one
			// hairstyle a counter takes (IsPlayerBotReplacedHair).
			if (look == PLAYERBOT_ISHOP_LOOK_HAIR && ch->GetWear(WEAR_COSTUME_HAIR))
			{
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
				{
					LPITEM item = ch->GetInventoryItem(cell);
					if (!item || item->isLocked() || item->IsExchanging() || !IsPlayerBotHairToWear(ch, item) ||
							!IsPlayerBotOwnLook(ch, item->GetProto(), look) ||
							IsPlayerBotSidekickUnwanted(ch, item))	// MT2009_PLUS_SIDEKICK_MOUNT_FIX_V1
						continue;
					const DWORD oldVnum = ch->GetWear(WEAR_COSTUME_HAIR)->GetVnum();
					if (ch->EquipItem(item))
						sys_log(0, "PLAYERBOT_ISHOP: hairstyle changed pid=%u name=%s vnum=%u old=%u",
								ch->GetPlayerID(), ch->GetName(), item->GetVnum(), oldVnum);
					break;
				}
				continue;
			}
			if (look == PLAYERBOT_ISHOP_LOOK_PET ? IsPlayerBotPetSummoned(ch) :
					look == PLAYERBOT_ISHOP_LOOK_MOUNT ?
#if defined(ENABLE_MOUNT_COSTUME_SYSTEM)
						ch->GetWear(WEAR_COSTUME_MOUNT) != NULL
#else
						true
#endif
					: ch->GetWear(s_abWear[look]) != NULL)
				continue;
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				// MT2009_PLUS_SIDEKICK_MOUNT_FIX_V1: not a piece a companion's
				// owner took off it (its costumes window) - a seal, a costume.
				if (!item || item->isLocked() || item->IsExchanging() || !item->CanUsedBy(ch) ||
						!IsPlayerBotOwnLook(ch, item->GetProto(), look) || IsPlayerBotSidekickUnwanted(ch, item))
					continue;
				if (look == PLAYERBOT_ISHOP_LOOK_HAIR)
					state.bBoughtHairstyle = true;
				if (look == PLAYERBOT_ISHOP_LOOK_PET)
				{
#if defined(__PET_SYSTEM__) && defined(USE_PET_SEAL_ON_LOGIN)
					if (ch->GetPetSystem() && ch->SummonPetFromItem(item) && IsPlayerBotPetSummoned(ch))
						sys_log(0, "PLAYERBOT_ISHOP: pet out pid=%u name=%s vnum=%u",
								ch->GetPlayerID(), ch->GetName(), item->GetVnum());
#endif
				}
				else if (ch->EquipItem(item))
					sys_log(0, "PLAYERBOT_ISHOP: %s on pid=%u name=%s vnum=%u", GetPlayerBotLookReason(look),
							ch->GetPlayerID(), ch->GetName(), item->GetVnum());
				break;
			}
		}
	}

	// The engine's own purchase, as a client makes it: the browse flag the
	// window sets, BuyItem (level, price, the account's balance read back, the
	// charge through the db core, AutoGiveItem, the purchase log), the flag off
	// on the same tick so IsBusy does not hold the rest of the tick.
	bool BuyPlayerBotItemShop(LPCHARACTER ch, TPlayerBotAIState& state, const TPlayerBotItemShopWish& wish, DWORD dwNow)
	{
		const TPlayerBotItemShopEntry* pEntry = FindPlayerBotItemShopEntry(ch, state, wish);
		if (!pEntry)
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
		WearPlayerBotBoughtLook(ch, state);
		if (IsPlayerBotSidekickPID(ch->GetPlayerID()))
			NotePlayerBotSidekickItemShopBuy(ch, wish.dwVnum, entry.dwCount, entry.dwPrice, wish.bMarks, wish.szReason,
					wish.bMarks ? state.iDragonMarks : state.iDragonCoins);
		return true;
	}

	// ----------------------------------------------------------------- tick

	bool IsPlayerBotLookReason(const char* szReason)
	{
		for (int look = 0; look < PLAYERBOT_ISHOP_LOOK_COUNT; ++look)
			if (szReason && strcmp(szReason, GetPlayerBotLookReason(look)) == 0)
				return true;
		return false;
	}

	// After a purchase: another piece missing and the coins for it, and the
	// bot comes back in a few seconds instead of an hour.
	void ContinuePlayerBotLookSession(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		int look = 0;
		TPlayerBotItemShopWish next;
		next.dwVnum = PickPlayerBotLook(ch, &look);
		next.bMarks = false;
		next.szReason = GetPlayerBotLookReason(look);
		if (next.dwVnum == 0 || !CanPlayerBotAffordWish(ch, state, next))
			return;
		TPlayerBotItemShopWish needs[PLAYERBOT_ISHOP_MAX_WISHES];
		const int n = CollectPlayerBotItemShopNeeds(ch, state, needs);
		if (!PlayerBotLookFromSurplus(ch, state, next.dwVnum, GetPlayerBotItemShopNeedReserve(ch, state, needs, n)))
			return;
		state.bItemShopLookSession = true;
		state.dwNextItemShopBuyTime = dwNow + PLAYERBOT_ISHOP_SESSION_STEP;
		state.dwNextItemShopCheckTime = dwNow + PLAYERBOT_ISHOP_SESSION_STEP;
	}

	// Upkeep, never the tick's owner: a voucher cashed, a purchase made, and
	// the rest of the tick goes on. Every ten minutes a bot; the account is
	// asked for its balance only when the bot wants something.
	void ManagePlayerBotItemShop(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || ch->IsDead() || !ch->GetDesc())
			return;
#if defined(ENABLE_MOUNT_COSTUME_SYSTEM)
		// Death takes the seal off into the bag (server-patches/mountdeath); it
		// goes back on within seconds instead of at the next look, ten
		// minutes on.
		if (!ch->GetWear(WEAR_COSTUME_MOUNT) && dwNow >= state.dwNextMountRewearTime &&
				!ch->IsBusy() && !ch->GetMyShop() && !ch->GetExchange())
		{
			state.dwNextMountRewearTime = dwNow + 5000;
			WearPlayerBotBoughtLook(ch, state);
		}
#endif
		if (dwNow < state.dwNextItemShopCheckTime)
			return;
		state.dwNextItemShopCheckTime = dwNow + PLAYERBOT_ISHOP_CHECK_INTERVAL +
				PlayerBotNavHash(ch->GetPlayerID() ^ 0x49534850U) % 60000U;
		if (IsPlayerBotSidekickPID(ch->GetPlayerID()) ? !IsPlayerBotSidekickCoinsOn(ch->GetPlayerID()) :
				!IsPlayerBotItemShopEnabled())
			return;
		if (ch->IsBusy() || ch->GetMyShop() || ch->GetExchange())
			return;
		RefreshPlayerBotItemShopCatalogue(dwNow);
		WearPlayerBotBoughtLook(ch, state);

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
		// Within a session only the look is bought: a stone the bot still
		// wants after buying one would be bought again every few seconds.
		const bool session = state.bItemShopLookSession;
		state.bItemShopLookSession = false;
		for (int i = 0; i < wants; ++i)
		{
			if (session && !IsPlayerBotLookReason(wishes[i].szReason))
				continue;
			if (!CanPlayerBotAffordWish(ch, state, wishes[i]))
				continue;
			if (BuyPlayerBotItemShop(ch, state, wishes[i], dwNow))
				ContinuePlayerBotLookSession(ch, state, dwNow);
			return;
		}
		if (!session)
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
	void CashPlayerBotVouchers(LPCHARACTER, TPlayerBotAIState&, DWORD)
	{
	}

	void ManagePlayerBotItemShop(LPCHARACTER, TPlayerBotAIState&, DWORD)
	{
	}

	void WritePlayerBotItemShopCensus(DWORD)
	{
	}
#endif
}

#endif
