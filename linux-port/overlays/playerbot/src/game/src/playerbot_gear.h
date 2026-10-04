#ifndef __INC_METIN2_PLAYERBOT_GEAR_H__
#define __INC_METIN2_PLAYERBOT_GEAR_H__

// What a bot wears and what it carries: equipment scoring, the progression
// ladder of weapons and armour, arrows, and the potion supply.
//
// Same kind of file as playerbot_types.h -- an implementation fragment, not a
// normal header. It defines objects, it relies on the engine headers
// playerbot_manager.cpp includes above it, and its anonymous namespace is the
// same one the manager reopens. Include it exactly once, from
// playerbot_manager.cpp, after playerbot_navigation.h.
//
// The order of these fragments is the dependency order: everything here is
// written against the world (navigation) and the bot's own state (types), and
// against nothing that comes after it. The few things it needs from later
// subsystems are forward-declared below rather than pulled in.

namespace
{
	// "Write this item's row now." The db core keeps a changed item in its
	// cache for PLAYER_CACHE_FLUSH_SECONDS - seven minutes by default, which
	// is what this world runs - before MariaDB sees it, so anything reading
	// player.item (both panels) is that far behind a bot's bag. The engine's
	// own answer is HEADER_GD_ITEM_FLUSH, which CInputMain sends after a shop
	// deal; it costs one write, so it is for the rare, visible changes - what
	// a bot wears - and never for a bag that turns over every few seconds.
	void FlushPlayerBotItemRow(LPITEM item)
	{
		if (!item || item->GetID() == 0 || !db_clientdesc)
			return;
		ITEM_MANAGER::instance().FlushDelayedSave(item);
		const DWORD dwID = item->GetID();
		db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_FLUSH, 0, sizeof(DWORD));
		db_clientdesc->Packet(&dwID, sizeof(DWORD));
	}

	// Defined with the town code. Buying anything means standing at an NPC
	// first, and where exactly is a town concern, not a gear one.
	void GetPlayerBotNpcApproach(DWORD playerID, long npcX, long npcY, DWORD salt,
			long& approachX, long& approachY);

	// Defined with the bag rules (playerbot_economy.h): an item's own stack
	// limit, which on mt2009 is the proto's and not always two hundred.
	int PlayerBotMaxStack(LPITEM item);

	// Defined with the chest pass (playerbot_consumables.h): a box the engine
	// refused this bot, remembered by bot and vnum. The two passes here that
	// open a starter-chain chest by themselves ask it before they try and tell
	// it when UseItem says no, or a box that cannot open is asked for again on
	// every one of their passes.
	bool IsPlayerBotChestRefused(DWORD dwPlayerID, DWORD dwVnum, DWORD dwNow);
	void NotePlayerBotChestRefused(DWORD dwPlayerID, DWORD dwVnum, DWORD dwNow);
	// And the free column of three a giftbox opens into, made the same way
	// (FreePlayerBotGiftboxColumn, playerbot_consumables.h).
	bool FreePlayerBotGiftboxColumn(LPCHARACTER ch);

	// Whether this character fights with a weapon of this kind. Asked of an
	// item by IsPlayerBotWeapon and of a merchant's proto by
	// GetPlayerBotMerchantWeaponCeiling.
	bool IsPlayerBotWeaponSubTypeFor(LPCHARACTER ch, BYTE subType)
	{
		if (ch)
		{
			switch (ch->GetJob())
			{
				case JOB_ASSASSIN:
					if (ch->GetSkillGroup() == 2)
						return subType == WEAPON_BOW;
					// Before selecting a profession and on Dagger training, never equip
					// a bow: melee Ninja skills ask CalcMeleeDamage and reject bows.
					return subType == WEAPON_DAGGER || subType == WEAPON_SWORD;
				case JOB_WARRIOR:
					if (ch->GetSkillGroup() == 2)
						return subType == WEAPON_TWO_HANDED || subType == WEAPON_SWORD;
					return subType == WEAPON_SWORD;
				case JOB_SURA:
					return subType == WEAPON_SWORD;
				case JOB_SHAMAN:
					return subType == WEAPON_BELL || subType == WEAPON_FAN;
			}
		}

		switch (subType)
		{
			case WEAPON_SWORD:
			case WEAPON_DAGGER:
			case WEAPON_TWO_HANDED:
			case WEAPON_BELL:
			case WEAPON_FAN:
			case WEAPON_MOUNT_SPEAR:
				return true;
			case WEAPON_BOW:
				return true;
		}

		return false;
	}

	bool IsPlayerBotWeapon(LPCHARACTER ch, LPITEM item)
	{
		return item && item->GetType() == ITEM_WEAPON &&
				IsPlayerBotWeaponSubTypeFor(ch, item->GetSubType());
	}

	// Defined further down, with the ladders and the merchants' stock.
	int GetPlayerBotProtoLevelLimit(const TItemTable* proto);
	bool IsPlayerBotProtoForCharacter(LPCHARACTER ch, const TItemTable* proto);

	// MT2009_PLUS_BOT_SHAMAN_INT_SET_V1: a Shaman's Intelligence set
	// (playerbot_shaman_buff_set.h, included after this file).
	bool IsPlayerBotBuffSetPiece(LPCHARACTER ch, LPITEM item);
	bool IsPlayerBotBuffSetDressed(LPCHARACTER ch);
	bool MaintainPlayerBotBuffSet(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow);

	// Iwakura's Community Patch 5, point 2 (playerbot_refine_rules.h): a weapon
	// for the tenth level or under in the hands of a bot of thirty. Its burned
	// weapon's place was taken by "byle jaka bron na 1. poziom +6", with yang
	// in the purse and a level-30 weapon at +0 in the bag - even at sixty.
	bool IsPlayerBotLowWeaponFor(LPCHARACTER ch, LPITEM item)
	{
		return ch && item && item->GetType() == ITEM_WEAPON && item->GetSubType() != WEAPON_ARROW &&
				playerbot_refine_rules::IsLowWeaponFor((int)ch->GetLevel(), (int)item->GetLevelLimit());
	}

	// A weapon over that line this bot could put on now: its job's kind, its
	// class's, a level it has reached. Asked without the equipment candidate
	// test below, which asks the ban this is the answer to.
	bool IsPlayerBotProperWeaponFor(LPCHARACTER ch, LPITEM item)
	{
		return ch && item && IsPlayerBotWeapon(ch, item) && !item->IsExchanging() && item->CanUsedBy(ch) &&
				!IsPlayerBotSidekickUnwanted(ch, item) &&
				playerbot_refine_rules::IsProperWeaponFor((int)ch->GetLevel(), (int)item->GetLevelLimit());
	}

	// What a village merchant's line costs: the shop's price, the proto's when
	// the shop names none, never under a hundred - the arithmetic
	// BuyPlayerBotProgressionGear pays by.
	long long GetPlayerBotMerchantOfferPrice(const CShop::SHOP_ITEM& offer, const TItemTable* proto)
	{
		long long price = (long long)offer.price;
		if (price <= 0 && proto)
			price = proto->dwShopBuyPrice > 0 ? (long long)proto->dwShopBuyPrice : (long long)proto->dwGold;
		return std::max<long long>(100, price);
	}

	// A weapon over the line one of the three village merchants sells this
	// bot, and what it costs.
	struct TPlayerBotMerchantWeapon
	{
		DWORD dwVnum;
		long long llPrice;
		int iLevel;
		long long llRoll;
	};

	void CollectPlayerBotProperMerchantWeapons(LPCHARACTER ch, std::vector<TPlayerBotMerchantWeapon>& out)
	{
		out.clear();
		if (!ch)
			return;
		static const DWORD merchants[] = { 9001, 9002, 9003 };
		for (size_t i = 0; i < sizeof(merchants) / sizeof(merchants[0]); ++i)
		{
			LPSHOP shop = CShopManager::instance().GetByNPCVnum(merchants[i]);
			if (!shop)
				continue;
			const std::vector<CShop::SHOP_ITEM>& offers = shop->GetItemVector();
			for (size_t k = 0; k < offers.size(); ++k)
			{
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(offers[k].vnum);
				if (!proto || proto->bType != ITEM_WEAPON || !IsPlayerBotWeaponSubTypeFor(ch, proto->bSubType) ||
						!IsPlayerBotProtoForCharacter(ch, proto))
					continue;
				const int level = GetPlayerBotProtoLevelLimit(proto);
				if (!playerbot_refine_rules::IsProperWeaponFor((int)ch->GetLevel(), level))
					continue;
				bool known = false;
				for (size_t j = 0; j < out.size() && !known; ++j)
					known = out[j].dwVnum == offers[k].vnum;
				if (known)
					continue;
				TPlayerBotMerchantWeapon offer;
				offer.dwVnum = offers[k].vnum;
				offer.llPrice = GetPlayerBotMerchantOfferPrice(offers[k], proto);
				offer.iLevel = level;
				offer.llRoll = (long long)proto->alValues[3] + proto->alValues[4];
				out.push_back(offer);
			}
		}
	}

	// The best level a village merchant sells this bot a weapon at (over the
	// low line from level thirty), 0 when none: what a burned weapon would be
	// replaced with, and so the level a backup has to reach
	// (playerbot_refine_rules::IsBackupWeaponFor).
	int GetPlayerBotProperMerchantTopLevel(LPCHARACTER ch)
	{
		std::vector<TPlayerBotMerchantWeapon> offers;
		CollectPlayerBotProperMerchantWeapons(ch, offers);
		int top = 0;
		for (size_t i = 0; i < offers.size(); ++i)
			top = std::max(top, offers[i].iLevel);
		return top;
	}

	// The ban's answer for one bot, kept PLAYERBOT_LOW_WEAPON_BAN_CACHE_MS: the
	// candidate test asks it for every bag cell of every pass, and it walks the
	// bag and the merchants' stock.
	struct TPlayerBotLowWeaponBan
	{
		DWORD dwTime;
		bool bBanned;
		bool bHoldsProper;
		long long llCheapest;
		TPlayerBotLowWeaponBan() : dwTime(0), bBanned(false), bHoldsProper(false), llCheapest(0) {}
	};
	std::map<DWORD, TPlayerBotLowWeaponBan> s_mapPlayerBotLowWeaponBan;
	// When a purchase of a weapon over the line last failed for want of room.
	std::map<DWORD, DWORD> s_mapPlayerBotProperWeaponRefused;

	const TPlayerBotLowWeaponBan& ReadPlayerBotLowWeaponBan(LPCHARACTER ch, bool fresh)
	{
		TPlayerBotLowWeaponBan& ban = s_mapPlayerBotLowWeaponBan[ch->GetPlayerID()];
		const DWORD now = get_dword_time();
		if (!fresh && ban.dwTime != 0 && now - ban.dwTime < PLAYERBOT_LOW_WEAPON_BAN_CACHE_MS)
			return ban;
		ban.dwTime = now != 0 ? now : 1;
		ban.bHoldsProper = IsPlayerBotProperWeaponFor(ch, ch->GetWear(WEAR_WEAPON));
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && !ban.bHoldsProper; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			ban.bHoldsProper = item && item->GetCell() == cell && !item->IsEquipped() &&
					IsPlayerBotProperWeaponFor(ch, item);
		}
		// The merchants' prices matter only to a bot with nothing over the
		// line: one that holds a weapon over it keeps off the low one whatever
		// its purse holds.
		std::vector<TPlayerBotMerchantWeapon> offers;
		if (!ban.bHoldsProper)
			CollectPlayerBotProperMerchantWeapons(ch, offers);
		ban.llCheapest = 0;
		for (size_t i = 0; i < offers.size(); ++i)
			if (ban.llCheapest == 0 || offers[i].llPrice < ban.llCheapest)
				ban.llCheapest = offers[i].llPrice;
		std::map<DWORD, DWORD>::const_iterator refused = s_mapPlayerBotProperWeaponRefused.find(ch->GetPlayerID());
		const bool refusedLately = refused != s_mapPlayerBotProperWeaponRefused.end() &&
				now - refused->second < PLAYERBOT_LOW_WEAPON_REFUSED_MS;
		ban.bBanned = !playerbot_refine_rules::IsLowWeaponFallback(ban.bHoldsProper, (long long)ch->GetGold(),
				ban.llCheapest, refusedLately);
		return ban;
	}

	// Whether the ban holds for this bot now: of the level, and not given way
	// (IsLowWeaponFallback: nothing over the line to fight with and no way to
	// buy it). fresh reads the bag and the purse again.
	bool IsPlayerBotLowWeaponBanned(LPCHARACTER ch, bool fresh = false)
	{
		if (!ch || (int)ch->GetLevel() < PLAYERBOT_LOW_WEAPON_BAN_LEVEL || !ch->IsItemLoaded())
			return false;
		return ReadPlayerBotLowWeaponBan(ch, fresh).bBanned;
	}

	// This weapon, for this bot, now. What a companion's owner put on is the
	// owner's word (playerbot_sidekick.h).
	bool IsPlayerBotBannedLowWeapon(LPCHARACTER ch, LPITEM item, bool fresh = false)
	{
		return IsPlayerBotLowWeaponFor(ch, item) && !IsPlayerBotSidekickPinned(ch, item) &&
				IsPlayerBotLowWeaponBanned(ch, fresh);
	}

	// A bot of thirty with no weapon over the line, worn or in the bag, while
	// the ban holds: the weapon merchant's errand (BuyPlayerBotProperWeapon) or
	// the market's, and on the frontier a reason to go home for it.
	bool NeedsPlayerBotProperWeapon(LPCHARACTER ch)
	{
		if (!ch || (int)ch->GetLevel() < PLAYERBOT_LOW_WEAPON_BAN_LEVEL || !ch->IsItemLoaded() ||
				IsPlayerBotSidekickPinned(ch, ch->GetWear(WEAR_WEAPON)))
			return false;
		// A weapon over the line in the hand - nearly every bot - is answered
		// without the bag and the merchants behind the kept answer.
		if (IsPlayerBotProperWeaponFor(ch, ch->GetWear(WEAR_WEAPON)))
			return false;
		const TPlayerBotLowWeaponBan& ban = ReadPlayerBotLowWeaponBan(ch, false);
		return ban.bBanned && !ban.bHoldsProper;
	}

	// A purchase of one refused for want of room: the low weapon may be worn
	// for PLAYERBOT_LOW_WEAPON_REFUSED_MS rather than none.
	void NotePlayerBotProperWeaponRefused(LPCHARACTER ch)
	{
		if (!ch)
			return;
		const DWORD now = get_dword_time();
		s_mapPlayerBotProperWeaponRefused[ch->GetPlayerID()] = now != 0 ? now : 1;
		s_mapPlayerBotLowWeaponBan.erase(ch->GetPlayerID());
	}

	bool IsPlayerBotEquipmentCandidate(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->IsExchanging() || !item->IsEquipable())
			return false;
		// A piece a companion's owner took off in the window stays off
		// (playerbot_sidekick.h).
		if (IsPlayerBotSidekickUnwanted(ch, item))
			return false;

		// IsEquipable only describes the item type.  The class restrictions live
		// in the anti flags and were previously checked only for sex, so a Warrior
		// could keep (and repeatedly try to equip/refine) entire inventories of
		// Ninja, Sura and Shaman armour.  CanUsedBy is the engine's canonical job
		// anti-flag check and deliberately does not depend on combat state.
		if (!item->CanUsedBy(ch))
			return false;

		if (item->GetType() == ITEM_WEAPON && !IsPlayerBotWeapon(ch, item))
			return false;
		// Nothing for the tenth level or under in the hands of a bot of thirty
		// while anything better can be had (Community Patch 5, point 2): not
		// worn, not bought, not kept as the backup, not refined.
		if (IsPlayerBotBannedLowWeapon(ch, item))
			return false;
		// Two uniques are never worn, and the rings and gloves on a clock are
		// the unique-slot pass's to put on and take off (playerbot_unique_slots.h):
		// an empty unique slot took any unique here, which is how a ring that
		// hides the level ended up on eleven bots.
		// MT2009_PLUS_BOT_RANK_GLOVE_V1: so are the Prophecy King's Glove and
		// Symbol, worn only at a negative rank (IsPlayerBotRankUnique).
		if (item->GetType() == ITEM_UNIQUE &&
				(IsPlayerBotNeverWornUnique(item->GetVnum()) || IsPlayerBotTimedUnique(item->GetVnum()) ||
				 IsPlayerBotRankUnique(item->GetVnum())))
			return false;
		// A ride seal is a player's mount: worn, it puts its wearer on the
		// animal (mount_seals.quest), and a bot has no business riding a war
		// boar into a pack because a unique slot stood empty.
		if (item->GetType() == ITEM_UNIQUE && item->GetSubType() == UNIQUE_SPECIAL_RIDE)
			return false;

		switch (item->GetType())
		{
			case ITEM_WEAPON:
			case ITEM_ARMOR:
			case ITEM_UNIQUE:
			case ITEM_RING:
			case ITEM_BELT:
				break;
			default:
				return false;
		}

		if ((item->GetAntiFlag() & ITEM_ANTIFLAG_MALE) && GET_SEX(ch) == SEX_MALE)
			return false;
		if ((item->GetAntiFlag() & ITEM_ANTIFLAG_FEMALE) && GET_SEX(ch) == SEX_FEMALE)
			return false;

		return true;
	}

	// Outdated gear (BeakGo and other bots of seventy wore a level-1 shield
	// +9): from PLAYERBOT_OUTDATED_GEAR_MIN_LEVEL a body armour, helmet or
	// shield PLAYERBOT_OUTDATED_GEAR_LEVELS or more under the bot gives way
	// to any wearable piece of its slot that is not outdated itself, whatever
	// the two score - a +9 of level one out-scores a +0 of level sixty on its
	// numbers, and the higher piece is refined from there. The market finds
	// the new piece (FindPlayerBotOutdatedGearPick), a finished +6 first.
	const int PLAYERBOT_OUTDATED_GEAR_MIN_LEVEL = 50;
	const int PLAYERBOT_OUTDATED_GEAR_LEVELS = 30;
	const int PLAYERBOT_OUTDATED_GEAR_BUDGET_PERCENT = 30;
	const BYTE PLAYERBOT_OUTDATED_HELMET_PLUS = 6;

	bool IsPlayerBotOutdatedGearSubType(BYTE subType)
	{
		return subType == ARMOR_BODY || subType == ARMOR_HEAD || subType == ARMOR_SHIELD;
	}

	bool IsPlayerBotOutdatedGear(LPCHARACTER ch, LPITEM item)
	{
		return ch && item && item->GetType() == ITEM_ARMOR && IsPlayerBotOutdatedGearSubType(item->GetSubType()) &&
				(int)ch->GetLevel() >= PLAYERBOT_OUTDATED_GEAR_MIN_LEVEL &&
				(int)item->GetLevelLimit() + PLAYERBOT_OUTDATED_GEAR_LEVELS <= (int)ch->GetLevel();
	}

	// Whether `worn` gives way to `item` by that rule.
	bool PlayerBotOutdatedGearGivesWay(LPCHARACTER ch, LPITEM worn, LPITEM item)
	{
		return item && item->GetType() == ITEM_ARMOR && IsPlayerBotOutdatedGear(ch, worn) &&
				!IsPlayerBotOutdatedGear(ch, item) && item->GetSubType() == worn->GetSubType() &&
				(int)item->GetLevelLimit() <= (int)ch->GetLevel();
	}

	// A counter's piece that would replace an outdated one worn, for a bot
	// with none such in its bag already.
	bool IsPlayerBotOutdatedGearOffer(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !offer || offer->GetType() != ITEM_ARMOR || !IsPlayerBotOutdatedGearSubType(offer->GetSubType()) ||
				(int)ch->GetLevel() < PLAYERBOT_OUTDATED_GEAR_MIN_LEVEL || !IsPlayerBotEquipmentCandidate(ch, offer))
			return false;
		const int wearCell = offer->FindEquipCell(ch);
		LPITEM worn = wearCell >= 0 && wearCell < WEAR_MAX_NUM ? ch->GetWear((BYTE)wearCell) : NULL;
		if (!worn || !PlayerBotOutdatedGearGivesWay(ch, worn, offer))
			return false;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM held = ch->GetInventoryItem(cell);
			if (held && held != offer && held->GetCell() == cell && !held->IsEquipped() &&
					PlayerBotOutdatedGearGivesWay(ch, worn, held) && IsPlayerBotEquipmentCandidate(ch, held))
				return false;
		}
		return true;
	}

	// An item a wear slot points at that the engine really wears: owned by this
	// character, flagged equipped, and in that slot's own cell. The first
	// version of the emergency weapon purchase equipped whatever AutoGiveItem
	// handed back, and with a full bag that is an item lying on the ground: a
	// Miecz+0 then sat in the slot and on the ground at once (BROLID, 15
	// September, 11:10:34), its ground timer fired five minutes later ("Owner
	// exist"), and the next blacksmith visit ended with RemoveFromCharacter's
	// "Invalid Item Position", a destroyed item still in the weapon slot, and
	// twenty-one equips of a sword over it in one second - FAST_ITEM_SWAP, and
	// a bot thrown out of the game. Whatever put an item there, nothing is
	// swapped, refined or taken off through a slot that fails this.
	bool IsPlayerBotWornItemSound(LPCHARACTER ch, LPITEM item, int wearCell)
	{
		return ch && item && wearCell >= 0 && item->GetOwner() == ch && item->IsEquipped() &&
				(int)item->GetCell() == (int)INVENTORY_MAX_NUM + wearCell;
	}

	// The stat this character fights with. A warrior swings with strength and a
	// shaman casts with intelligence, so the same earring is a good piece for one
	// and jewellery for the other. Sura splits: the weaponry build hits with
	// strength, the black-magic one with intelligence, and the skill group is
	// what says which - the same question the weapon ladder already asks it.
	BYTE GetPlayerBotPrimaryStatApply(LPCHARACTER ch)
	{
		if (!ch)
			return APPLY_NONE;
		switch (ch->GetJob())
		{
			case JOB_WARRIOR:  return APPLY_STR;
			case JOB_ASSASSIN: return APPLY_DEX;
			case JOB_SURA:     return ch->GetSkillGroup() == 2 ? APPLY_INT : APPLY_STR;
			case JOB_SHAMAN:   return APPLY_INT;
			default:           return APPLY_NONE;
		}
	}

	// Which of the two ways of doing damage a school lives by. Warrior Body,
	// Sura Weaponry and Ninja Dagger earn with ordinary blows; Warrior Mental,
	// Sura Black Magic, Ninja Archer and both Shaman schools with skills. +1 for
	// a skill school, -1 for a blow school, 0 before the profession is chosen -
	// and 0 is a real answer, not a default: it means weight both the same.
	int GetPlayerBotSchoolStyle(LPCHARACTER ch)
	{
		if (!ch || ch->GetSkillGroup() == 0)
			return 0;
		switch (ch->GetJob())
		{
			case JOB_WARRIOR:  return ch->GetSkillGroup() == 2 ? 1 : -1;
			case JOB_SURA:     return ch->GetSkillGroup() == 2 ? 1 : -1;
			case JOB_ASSASSIN: return ch->GetSkillGroup() == 2 ? 1 : -1;
			case JOB_SHAMAN:   return 1;
			default:           return 0;
		}
	}

	// And of the skill schools, which ones roll the weapon's *magic* values.
	// SetPolyVarForAttack (char_skill.cpp) hands every skill both numbers - wep
	// from VALUE3/4 and mwep from VALUE1/2 - and the skill's own formula picks.
	// Black Magic and both Shaman schools pick mwep; Mental and Archer are skill
	// schools that still hit with wep.
	bool IsPlayerBotMagicSchool(LPCHARACTER ch)
	{
		if (!ch || ch->GetSkillGroup() == 0)
			return false;
		if (ch->GetJob() == JOB_SHAMAN)
			return true;
		return ch->GetJob() == JOB_SURA && ch->GetSkillGroup() == 2;
	}

	long long ScorePlayerBotApply(BYTE bType, long lValue, LPCHARACTER ch = NULL)
	{
		switch (bType)
		{
			case APPLY_NONE:
			case APPLY_SKILL:
				return 0;
			// Fifteen a point: fifteen hundred health - half again what a
			// character of forty has - is worth more than twenty-two points of
			// defence, and at ten it was worth fifteen, less than the seven
			// points that separated two armours a bot chose between.
			case APPLY_MAX_HP:
				return (long long)lValue * 15;
			case APPLY_MAX_SP:
			// The sprint bar, and the reason a bot would otherwise wear the
			// level-0 bracelet for ever: ten points of it through the catch-all
			// of fifty scored five hundred, more than anything the line offers
			// below level 46. It is not a combat stat for a player either.
			case APPLY_MAX_STAMINA:
				return (long long)lValue * 3;
			case APPLY_CON:
			case APPLY_STR:
			case APPLY_DEX:
			case APPLY_INT:
			{
				// 250 for all four was the class-blind figure, and it is still
				// what an unknown character gets. Knowing the class, the stat it
				// fights with is worth twice that and the two it does not use
				// half. Constitution stays in between for everybody: nobody
				// builds around it and nobody is sorry to have it.
				const BYTE primary = GetPlayerBotPrimaryStatApply(ch);
				long long weight = 250;
				if (primary != APPLY_NONE)
					weight = (bType == primary) ? 500
							: (bType == APPLY_CON ? 300 : 120);
				return (long long)lValue * weight;
			}
			case APPLY_ATT_SPEED:
				return (long long)lValue * 200;
			case APPLY_MOV_SPEED:
				return (long long)lValue * 100;
			case APPLY_HP_REGEN:
			case APPLY_POTION_BONUS:
				return (long long)lValue * 100;
			case APPLY_POISON_PCT:
				// A quarter of a boss's health a proc: PLAYERBOT_POISON_BOSS_LEVEL.
				return (long long)lValue *
						(ch && (int)ch->GetLevel() >= PLAYERBOT_POISON_BOSS_LEVEL ? 400 : 200);
			case APPLY_STUN_PCT:
			case APPLY_SLOW_PCT:
				return (long long)lValue * 200;
			case APPLY_CRITICAL_PCT:
			case APPLY_PENETRATE_PCT:
			case APPLY_BLOCK:
			case APPLY_DODGE:
				return (long long)lValue * 400;
			case APPLY_ATTBONUS_ANIMAL:
			case APPLY_ATTBONUS_ORC:
			case APPLY_ATTBONUS_MILGYO:
			case APPLY_ATTBONUS_UNDEAD:
			case APPLY_ATTBONUS_DEVIL:
			case APPLY_ATTBONUS_MONSTER:
				return (long long)lValue * 300;
			case APPLY_STEAL_HP:
			case APPLY_KILL_HP_RECOVER:
				return (long long)lValue * 250;
			case APPLY_ATT_GRADE_BONUS:
			case APPLY_DEF_GRADE_BONUS:
			case APPLY_DEF_GRADE:
				return (long long)lValue * 300;
			// Average damage is for ordinary blows and skill damage is for skills,
			// and which of the two a bot wants depends on how its school earns.
			// These used to be 500 and 50 - the skill bonus fell into the default
			// bucket - so a Black Magic sura or a shaman rated a 10% skill roll
			// below a single point of strength. Nothing is worth nothing to
			// anybody: a Body warrior still casts, a shaman still swings. Undecided
			// weights both equally.
			case APPLY_NORMAL_HIT_DAMAGE_BONUS:
			{
				const int style = GetPlayerBotSchoolStyle(ch);
				return (long long)lValue * (style < 0 ? 500 : style > 0 ? 250 : 400);
			}
			case APPLY_SKILL_DAMAGE_BONUS:
			{
				const int style = GetPlayerBotSchoolStyle(ch);
				return (long long)lValue * (style > 0 ? 500 : style < 0 ? 250 : 400);
			}
			// Defending against ordinary blows is worth the same to everybody:
			// every monster in this world swings.
			case APPLY_NORMAL_HIT_DEFEND_BONUS:
				return (long long)lValue * 500;
			// A flat magic attack bonus is the magic schools' strength line.
			case APPLY_MAGIC_ATT_GRADE:
				return (long long)lValue * (IsPlayerBotMagicSchool(ch) ? 300 : 60);
			case APPLY_IMMUNE_STUN:
			case APPLY_IMMUNE_SLOW:
			case APPLY_IMMUNE_FALL:
				return (long long)lValue * 1000;
			default:
				return (long long)lValue * 50;
		}
	}

	bool IsPlayerBotSpecialLevel30WeaponVnum(DWORD vnum)
	{
		return (vnum >= 290 && vnum <= 299) || (vnum >= 1170 && vnum <= 1179) ||
				(vnum >= 2150 && vnum <= 2159) || (vnum >= 3210 && vnum <= 3219) ||
				(vnum >= 5110 && vnum <= 5119) || (vnum >= 7160 && vnum <= 7169);
	}

	// Every line of one apply type an item carries: the proto's fixed applies and
	// the rolled attributes together.
	long SumPlayerBotItemLines(LPITEM item, BYTE applyType)
	{
		if (!item || !item->GetProto() || applyType == 0)
			return 0;
		long total = 0;
		for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
			if (item->GetProto()->aApplies[i].bType == applyType)
				total += item->GetProto()->aApplies[i].lValue;
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			if (item->GetAttributeType(i) == applyType)
				total += item->GetAttributeValue(i);
		return total;
	}

	// The lines a hit is made of, and which the hit model below reads itself -
	// the flat scorer must not count them a second time on a weapon.
	bool IsPlayerBotHitModelApply(BYTE applyType, LPCHARACTER ch)
	{
		switch (applyType)
		{
			case APPLY_NORMAL_HIT_DAMAGE_BONUS:
			case APPLY_SKILL_DAMAGE_BONUS:
			case APPLY_ATT_GRADE_BONUS:
			case APPLY_STR:
			case APPLY_CRITICAL_PCT:
			case APPLY_PENETRATE_PCT:
			case APPLY_ATTBONUS_MONSTER:
				return true;
			default:
				break;
		}
		int racePercent = 0;
		const int dominant = ch ? GetPlayerBotFightingRace(ch, &racePercent) : PLAYERBOT_RACE_NONE;
		return dominant != PLAYERBOT_RACE_NONE && applyType == GetPlayerBotRaceApplyType(dominant);
	}

	// What a character brings to a hit on one point, with the weapon it wears
	// taken back out: the candidate's own lines are then added, so the weapon in
	// the hand and two in the bag are all read against the same body.
	long PlayerBotPointWithoutWornWeapon(LPCHARACTER ch, BYTE point, BYTE applyType, LPITEM candidate)
	{
		if (!ch)
			return 0;
		long value = ch->GetPoint(point);
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		if (worn && worn != candidate)
			value -= SumPlayerBotItemLines(worn, applyType);
		return value;
	}

	// The lines of one apply type a weapon carries at another plus: the fixed
	// applies of that plus's proto and the attributes rolled on this piece,
	// which every refine copies across (ITEM_MANAGER::CopyAllAttrTo). A NULL
	// piece is a weapon nobody holds yet: the proto's lines alone.
	long SumPlayerBotLinesAt(LPITEM item, const TItemTable* proto, BYTE applyType)
	{
		if (!proto || applyType == 0)
			return 0;
		long total = 0;
		for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
			if (proto->aApplies[i].bType == applyType)
				total += proto->aApplies[i].lValue;
		if (item)
			for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				if (item->GetAttributeType(i) == applyType)
					total += item->GetAttributeValue(i);
		return total;
	}

	// The share mt2009 adds to a normal hit on a monster by the weapon's own
	// level (CHARACTER::Damage, DAMAGE_TYPE_NORMAL): a level limit of 32 to 65
	// adds limit * 30 / 100 - 3 percent, the level-65 elite families excepted,
	// and 70 or 75 adds ten. No tooltip shows it, and it is why a Krwawy Miecz
	// (level 45) hits ten percent harder than its numbers say while a level-30
	// weapon gets nothing. r40250 has no such rule.
	int GetPlayerBotWeaponLevelBonusPercent(const TItemTable* proto)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		if (!proto || proto->bType != ITEM_WEAPON)
			return 0;
		long limit = 0;
		for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
			if (proto->aLimits[i].bType == LIMIT_LEVEL)
				limit = proto->aLimits[i].lValue;
		const DWORD vnum = proto->dwVnum;
		// The engine's list; its last range (7140..5149) is empty and excepts
		// no fan, so neither does this one.
		const bool elite65 = (vnum >= 140 && vnum <= 159) || (vnum >= 1100 && vnum <= 1109) ||
				(vnum >= 2140 && vnum <= 2149) || (vnum >= 3130 && vnum <= 3139) ||
				(vnum >= 5100 && vnum <= 5109);
		if (limit >= 32 && limit <= 65 && !elite65)
			return (int)(limit * 30 / 100 - 3);
		if (limit == 70 || limit == 75)
			return 10;
		return 0;
#else
		(void)proto;
		return 0;
#endif
	}

	// One blow with this weapon at the plus whose proto is given, expected
	// over the dice, against a monster of the bot's own level - the way
	// battle.cpp and char_battle.cpp deal it ("moze warto aby postacie znaly
	// algorytm obrazen danej broni", 15 September):
	//
	//   AR     = (min(90, (DX*4 + level*2) / 6) + 210) / 300
	//            - (2*ER + 5) / (ER + 95) * 0.3, ER the monster's own rating
	//   atk    = (ATT_GRADE + roll*2 - level*2) * AR + level*2 + value5*2
	//   atk   *= 100 + attack percent, then the race line for its share
	//   hit    = atk - defence (about level + 15 on this proto)
	//   hit   *= 100 + average line, then the weapon-level bonus (mt2009)
	//   skill  = atk before the defence (a magic school: grade + magic roll),
	//            under 100 + skill-damage line
	//
	// A critical is a second hit one time in a hundred per percent; piercing
	// hands the defence back, counted at half. What the candidate would change
	// on the character - STR, grade, the percent lines - is measured against
	// the character without the weapon it wears, so the weapon in the hand and
	// two in the bag are read against the same body. The build decides the mix
	// (PLAYERBOT_WEAPON_OWN_LINE_PERCENT / _OTHER_LINE_PERCENT): a skill school
	// earns with skills and still swings, a blow school the other way round.
	// Skill damage used to be left out as "a PvP line", while char_battle.cpp
	// multiplies every skill on a monster by it, so a shaman's skill line was
	// worth nothing. And the defence is what the old model lacked most: a
	// percent line multiplies what is left after it, so a big line on a weak
	// base is worth less than it reads. assumedAverage stands in for the lines
	// of a weapon nobody holds (item NULL).
	// A stat as the character has it on foot. On a mount the engine lifts ST,
	// DX, HT and IQ to the horse's (ComputePoints) and adds a share of the
	// horse's level to the attack grade (ComputeBattlePoints), so every
	// weapon read in the saddle was read against another body: two close
	// weapons changed places at every mounting and dismounting. The base
	// stat and the lines of what the character wears are the same on foot
	// and in the saddle.
	// Iwakura's tier of a weapon's family, a grade on its blow alone
	// (GetPlayerBotEquipmentScore).
	const int PLAYERBOT_WEAPON_TIER_BLOW_PERCENT = 2;

	long PlayerBotFootStat(LPCHARACTER ch, BYTE point, BYTE applyType)
	{
		if (!ch)
			return 0;
		if (!ch->GetMountVnum())
			return ch->GetPoint(point);
		long value = ch->GetRealPoint(point);
		for (int wear = 0; wear < WEAR_MAX_NUM; ++wear)
			if (LPITEM worn = ch->GetWear(wear))
				value += SumPlayerBotItemLines(worn, applyType);
		return value;
	}

	// The attack grade the engine would give this character on foot: two a
	// level, the class's own stats (ComputeBattlePoints) and the grade bonus.
	long PlayerBotFootAttackGrade(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		if (!ch->GetMountVnum())
			return ch->GetPoint(POINT_ATT_GRADE);
		const long st = PlayerBotFootStat(ch, POINT_ST, APPLY_STR);
		const long dx = PlayerBotFootStat(ch, POINT_DX, APPLY_DEX);
		const long iq = PlayerBotFootStat(ch, POINT_IQ, APPLY_INT);
		long stat = 2 * st;
		switch (ch->GetJob())
		{
			case JOB_ASSASSIN: stat = dx + st; break;
			case JOB_SHAMAN: stat = (5 * iq + st) / 3; break;
			case JOB_SURA: stat = st + iq; break;
			default: break;
		}
		return 2L * ch->GetLevel() + stat + ch->GetPoint(POINT_ATT_GRADE_BONUS);
	}

	long long GetPlayerBotWeaponHitDamageAt(LPITEM item, const TItemTable* proto, LPCHARACTER ch,
			long assumedAverage = 0, TPlayerBotScoreTerms* parts = NULL)
	{
		if (!proto || proto->bType != ITEM_WEAPON)
			return 0;
		long long roll = (long long)proto->alValues[3] + proto->alValues[4];
		const long long magicRoll = (long long)proto->alValues[1] + proto->alValues[2];
		const long long plusAttack = 2LL * proto->alValues[5];
		// As before: a dagger's interval is half a sword's and a bow's roll is
		// doubled by CalcArrowDamage, both counted as a doubled roll.
		if (proto->bSubType == WEAPON_DAGGER || proto->bSubType == WEAPON_BOW)
			roll *= 2;
		long level = ch ? (long)ch->GetLevel() : 0;
		if (!ch)
			for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
				if (proto->aLimits[i].bType == LIMIT_LEVEL)
					level = proto->aLimits[i].lValue;

		LPITEM worn = ch ? ch->GetWear(WEAR_WEAPON) : NULL;
		// The weapon in the hand at its own plus is already in every point; any
		// other reading takes the worn weapon's lines out and puts these in.
		const bool inHand = item != NULL && item == worn && proto == item->GetProto();
		LPITEM kept = inHand ? item : NULL;
		const long assumed = item ? 0 : assumedAverage;
		long long grade = 0, attPct = 0, avgPct = 0, skillPct = 0, critPct = 0, penPct = 0;
		if (ch)
		{
			grade = PlayerBotPointWithoutWornWeapon(ch, POINT_ATT_GRADE, APPLY_ATT_GRADE_BONUS, kept);
			// Read on foot whatever the bot is riding (PlayerBotFootAttackGrade).
			if (ch->GetMountVnum())
				grade += PlayerBotFootAttackGrade(ch) - ch->GetPoint(POINT_ATT_GRADE);
			// No item carries an attack-percent line; POINT_ATT_BONUS comes from
			// affects and skills alone, the same for every candidate.
			attPct = ch->GetPoint(POINT_ATT_BONUS);
			avgPct = PlayerBotPointWithoutWornWeapon(ch, POINT_NORMAL_HIT_DAMAGE_BONUS, APPLY_NORMAL_HIT_DAMAGE_BONUS, kept);
			skillPct = PlayerBotPointWithoutWornWeapon(ch, POINT_SKILL_DAMAGE_BONUS, APPLY_SKILL_DAMAGE_BONUS, kept);
			critPct = PlayerBotPointWithoutWornWeapon(ch, POINT_CRITICAL_PCT, APPLY_CRITICAL_PCT, kept);
			penPct = PlayerBotPointWithoutWornWeapon(ch, POINT_PENETRATE_PCT, APPLY_PENETRATE_PCT, kept);
			if (!inHand)
			{
				if (worn)
					grade -= 2 * SumPlayerBotItemLines(worn, APPLY_STR);
				grade += SumPlayerBotLinesAt(item, proto, APPLY_ATT_GRADE_BONUS) +
						2 * SumPlayerBotLinesAt(item, proto, APPLY_STR);
				avgPct += SumPlayerBotLinesAt(item, proto, APPLY_NORMAL_HIT_DAMAGE_BONUS) + assumed;
				skillPct += SumPlayerBotLinesAt(item, proto, APPLY_SKILL_DAMAGE_BONUS);
				critPct += SumPlayerBotLinesAt(item, proto, APPLY_CRITICAL_PCT);
				penPct += SumPlayerBotLinesAt(item, proto, APPLY_PENETRATE_PCT);
			}
		}
		else
		{
			grade = SumPlayerBotLinesAt(item, proto, APPLY_ATT_GRADE_BONUS) +
					2 * SumPlayerBotLinesAt(item, proto, APPLY_STR);
			avgPct = SumPlayerBotLinesAt(item, proto, APPLY_NORMAL_HIT_DAMAGE_BONUS) + assumed;
			skillPct = SumPlayerBotLinesAt(item, proto, APPLY_SKILL_DAMAGE_BONUS);
			critPct = SumPlayerBotLinesAt(item, proto, APPLY_CRITICAL_PCT);
			penPct = SumPlayerBotLinesAt(item, proto, APPLY_PENETRATE_PCT);
		}
		long long racePct = SumPlayerBotLinesAt(item, proto, APPLY_ATTBONUS_MONSTER);
		int racePercent = 0;
		const int dominant = ch ? GetPlayerBotFightingRace(ch, &racePercent) : PLAYERBOT_RACE_NONE;
		if (dominant != PLAYERBOT_RACE_NONE && racePercent > 0)
			racePct += SumPlayerBotLinesAt(item, proto, GetPlayerBotRaceApplyType(dominant)) * racePercent / 100;

		// The attack rating against a monster of the bot's own level, whose DX
		// runs with its level: CalcAttackRating in thousandths.
		const long dx = ch ? PlayerBotFootStat(ch, POINT_DX, APPLY_DEX) : level;
		const long arSrc = std::min<long>(90, (dx * 4 + level * 2) / 6);
		const long erSrc = std::min<long>(90, level);
		const long long ar = std::max<long long>(100,
				(long long)(arSrc + 210) * 1000 / 300 - (long long)(2 * erSrc + 5) * 300 / (erSrc + 95));
		const long long levelPart = 2LL * level;
		long long attack = (grade + roll - levelPart) * ar / 1000 + levelPart + plusAttack;
		if (attack < 1)
			attack = 1;
		attack = attack * std::max<long long>(20, 100 + attPct) / 100;
		attack = attack * std::max<long long>(20, 100 + racePct) / 100;

		const long long defence = (long long)level + PLAYERBOT_MONSTER_DEFENCE_OVER_LEVEL;
		long long hit = std::max<long long>(1, attack - defence);
		hit += defence * std::max<long long>(0, std::min<long long>(100, penPct)) / 200;
		hit = hit * std::max<long long>(20, 100 + avgPct) / 100;
		hit = hit * (100 + GetPlayerBotWeaponLevelBonusPercent(proto)) / 100;
		hit = hit * std::max<long long>(100, 100 + critPct) / 100;

		long long skill = IsPlayerBotMagicSchool(ch)
				? std::max<long long>(1, grade + magicRoll + plusAttack) : attack;
		skill = skill * std::max<long long>(20, 100 + skillPct) / 100;

		const int style = GetPlayerBotSchoolStyle(ch);
		const long long hitShare = style > 0 ? PLAYERBOT_WEAPON_OTHER_LINE_PERCENT : PLAYERBOT_WEAPON_OWN_LINE_PERCENT;
		const long long skillShare = style < 0 ? PLAYERBOT_WEAPON_OTHER_LINE_PERCENT : PLAYERBOT_WEAPON_OWN_LINE_PERCENT;
		const long long total = (hit * hitShare + skill * skillShare) / (hitShare + skillShare);
		// The blow's parts, for an explained equipment decision (playerbot_explain.h).
		if (parts)
		{
			parts->Set(per::TERM_BLOW, total < 1 ? 1 : total);
			parts->Set(per::TERM_ROLL, roll);
			parts->Set(per::TERM_MAGIC_ROLL, magicRoll);
			parts->Set(per::TERM_PLUS_ATTACK, plusAttack);
			parts->Set(per::TERM_GRADE, grade);
			parts->Set(per::TERM_ATT_PCT, attPct);
			parts->Set(per::TERM_RACE_PCT, racePct);
			parts->Set(per::TERM_AVG_PCT, avgPct);
			parts->Set(per::TERM_SKILL_PCT, skillPct);
			parts->Set(per::TERM_CRIT_PCT, critPct);
			parts->Set(per::TERM_PEN_PCT, penPct);
			parts->Set(per::TERM_LEVEL_BONUS_PCT, GetPlayerBotWeaponLevelBonusPercent(proto));
			parts->Set(per::TERM_ATTACK, attack);
			parts->Set(per::TERM_MOB_DEFENCE, defence);
			parts->Set(per::TERM_HIT, hit);
			parts->Set(per::TERM_SKILL_HIT, skill);
			parts->Set(per::TERM_STYLE, style);
			parts->Set(per::TERM_HIT_SHARE, hitShare);
			parts->Set(per::TERM_SKILL_SHARE, skillShare);
		}
		return total < 1 ? 1 : total;
	}

	long long GetPlayerBotWeaponHitDamage(LPITEM item, LPCHARACTER ch, TPlayerBotScoreTerms* parts = NULL)
	{
		if (!item || !item->GetProto() || item->GetType() != ITEM_WEAPON)
			return 0;
		return GetPlayerBotWeaponHitDamageAt(item, item->GetProto(), ch, 0, parts);
	}

	// Iwakura's PvE tier of this piece's family for this character, or 0
	// when his list does not rate the family (body armour, helmets, shields,
	// everything he judges by level and lines). The family is the +0 vnum,
	// the same arithmetic the price table uses.
	int GetPlayerBotItemTierOf(LPITEM item, LPCHARACTER ch)
	{
		if (!item || (item->GetType() != ITEM_WEAPON && item->GetType() != ITEM_ARMOR))
			return 0;
		const BYTE refine = item->GetRefineLevel();
		if (refine > 9)
			return 0;
		return GetPlayerBotItemTier(item->GetVnum() - refine, ch ? (int)ch->GetJob() : -1, false);
	}

	// A line's worth, scaled by Iwakura's PvE tier of that kind of line
	// (PLAYERBOT_BONUS_TIER_PERCENT) - the same scale the reroll pass applies
	// in ScorePlayerBotBonusLine, so the pass that buys a piece and the pass
	// that rerolls it agree about what a line is worth.
	long long ScorePlayerBotApplyTiered(BYTE bType, long lValue, LPCHARACTER ch)
	{
		const long long raw = ScorePlayerBotApply(bType, lValue, ch);
		const int tier = ch ? GetPlayerBotBonusTier(bType, (int)ch->GetJob(), false) : 0;
		return tier > 0 ? raw * PLAYERBOT_BONUS_TIER_PERCENT[tier] / 100 : raw;
	}

	// A stone already in a socket, as the equipment score counts it: its own
	// lines, by his tier of the stone the way a bonus line goes by his tier of
	// the line. A stone seated before his list (a +0 to +2, or one he rates
	// low) still does what it does and counts at face value - a socket cannot
	// be emptied, so the piece is worth exactly what it holds.
	long long ScorePlayerBotSeatedSoulStones(LPITEM item, LPCHARACTER ch)
	{
		if (!item || (item->GetType() != ITEM_WEAPON && item->GetType() != ITEM_ARMOR))
			return 0;
		long long score = 0;
		for (int socketIdx = 0; socketIdx < ITEM_SOCKET_MAX_NUM; ++socketIdx)
		{
			const DWORD inSocket = (DWORD)item->GetSocket(socketIdx);
			if (inSocket <= 2 || inSocket == PLAYERBOT_BROKEN_SOUL_STONE_VNUM)
				continue;
			const TItemTable* stone = ITEM_MANAGER::instance().GetTable(inSocket);
			if (!stone || stone->bType != ITEM_METIN)
				continue;
			const int tier = GetPlayerBotSoulStoneTier(inSocket, false);
			for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
			{
				const long long raw = ScorePlayerBotApply(stone->aApplies[i].bType, stone->aApplies[i].lValue, ch);
				score += tier > 0 ? raw * PLAYERBOT_BONUS_TIER_PERCENT[tier] / 100 : raw;
			}
		}
		return score;
	}

	// Iwakura's Patch 4, point 1: the jewellery and boots worth taking from +4
	// to +9, by class and for every class, as the +0 vnum of each family (read
	// off world.item_proto: the family's ten grades are the base and the nine
	// after it). "Jesli bot ma do wyboru zalozenie podstawowego przedmiotu (np.
	// butow na 1 lvl +0) lub jednego ze wskazanych tutaj przedmiotow,
	// preferuje zalozenie przedmiotu z powyzszej listy" - for
	// PLAYERBOT_JEWEL_LIST_FOLLOWER_PERCENT of the bots; the rest wear what
	// scores best, "aby uniknac sztywnych regul i zachowac roznorodnosc".
	struct TPlayerBotListedJewel
	{
		DWORD dwBase;
		int iJob;   // -1 for every class
	};
	const TPlayerBotListedJewel PLAYERBOT_LISTED_JEWELS[] = {
		// Wojownik
		{ 17020, JOB_WARRIOR },   // Miedziane Kolczyki
		{ 17100, JOB_WARRIOR },   // Ebonitowe Kolczyki
		{ 17120, JOB_WARRIOR },   // Perlowe Kolczyki
		{ 16140, JOB_WARRIOR },   // Naszyj. Z Bial. Zlota
		// Sura
		{ 16040, JOB_SURA },      // Srebrny Naszyjnik
		{ 17060, JOB_SURA },      // Zlote Kolczyki
		{ 16080, JOB_SURA },      // Jadeitowy Naszyjnik
		{ 16120, JOB_SURA },      // Perlowy Naszyjnik
		{ 17140, JOB_SURA },      // Kolczyki Z Bial. Zlota
		{ 17200, JOB_SURA },      // Kolczyki Z Niebian.Lez
		// Ninja
		{ 17080, JOB_ASSASSIN },  // Jadeitowe Kolczyki
		{ 16160, JOB_ASSASSIN },  // Krysztalowy Naszyjnik
		{ 17160, JOB_ASSASSIN },  // Krysztalowe Kolczyki
		// Szaman
		{ 16040, JOB_SHAMAN },    // Srebrny Naszyjnik
		{ 17060, JOB_SHAMAN },    // Zlote Kolczyki
		{ 16080, JOB_SHAMAN },    // Jadeitowy Naszyjnik
		{ 17140, JOB_SHAMAN },    // Kolczyki Z Bial. Zlota
		{ 17200, JOB_SHAMAN },    // Kolczyki Z Niebian.Lez
		// Every class
		{ 14040, -1 },            // Srebrna Bransoleta
		{ 14140, -1 },            // Bransol. Z Bial. Zlota
		{ 14200, -1 },            // Bransol. Z Niebian.Lez
		{ 16180, -1 },            // Ametystowy Naszyjnik
		{ 16200, -1 },            // Naszyj. Z Niebian.Lez
		{ 14160, -1 },            // Krysztalowa Bransoleta
		// Boots
		{ 15200, -1 },            // Buty Feniksa
		{ 15080, -1 },            // Skorzane Kozaki
		{ 15180, -1 },            // Deszczowe Buty
		{ 15220, -1 },            // Buty Ognistego Ptaka
		{ 15160, -1 },            // Ekstazyjne Buty
	};

	bool IsPlayerBotJewelListFollower(LPCHARACTER ch)
	{
		return ch && PlayerBotNavHash(ch->GetPlayerID() ^ 0x4a45574cU) % 100U <
				PLAYERBOT_JEWEL_LIST_FOLLOWER_PERCENT;
	}

	bool IsPlayerBotListedJewel(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetType() != ITEM_ARMOR)
			return false;
		const BYTE sub = item->GetSubType();
		if (sub != ARMOR_WRIST && sub != ARMOR_NECK && sub != ARMOR_EAR && sub != ARMOR_FOOTS)
			return false;
		const BYTE refine = item->GetRefineLevel();
		if (refine > 9 || item->GetVnum() < refine)
			return false;
		const DWORD base = item->GetVnum() - refine;
		for (size_t i = 0; i < sizeof(PLAYERBOT_LISTED_JEWELS) / sizeof(PLAYERBOT_LISTED_JEWELS[0]); ++i)
			if (PLAYERBOT_LISTED_JEWELS[i].dwBase == base &&
					(PLAYERBOT_LISTED_JEWELS[i].iJob < 0 || PLAYERBOT_LISTED_JEWELS[i].iJob == (int)ch->GetJob()))
				return true;
		return false;
	}

	// Iwakura's Patch 4, point 9: a pair of boots is worth its rolled lines,
	// and only the lines his tier list rates three of six or better ("maks. PZ,
	// szansa na cios krytyczny"); its defence is ignored - "w przypadku butow
	// obrona jest wartoscia marginalna" - and so is its plus, and two clean
	// pairs are told apart by his PvE tier of the family. The equipment score
	// used to put the defence at a thousand a point, which made the boots
	// with the most of it the pair a bot wore whatever was rolled on the
	// others.
	long long GetPlayerBotBootsScore(LPITEM item, LPCHARACTER ch, TPlayerBotScoreTerms* terms = NULL)
	{
		const int familyTier = std::max(1, GetPlayerBotItemTierOf(item, ch));
		long long score = 1 + (long long)familyTier * PLAYERBOT_BOOTS_FAMILY_TIER_SCORE;
		long long lines = 0;
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			const BYTE t = item->GetAttributeType(i);
			if (t == APPLY_NONE || GetPlayerBotBonusTier(t, (int)ch->GetJob(), false) < PLAYERBOT_BOOTS_MIN_LINE_TIER)
				continue;
			lines += ScorePlayerBotApplyTiered(t, item->GetAttributeValue(i), ch);
		}
		score += lines;
		score += (long long)item->GetLevelLimit() * PLAYERBOT_ARMOR_LEVEL_TIE_BREAK;
		const bool listed = IsPlayerBotListedJewel(ch, item) && IsPlayerBotJewelListFollower(ch);
		if (listed)
			score = score * (100 + PLAYERBOT_JEWEL_LIST_PREFERENCE_PERCENT) / 100;
		if (terms)
		{
			terms->Set(per::TERM_BOOTS_FAMILY, (long long)familyTier * PLAYERBOT_BOOTS_FAMILY_TIER_SCORE);
			terms->Set(per::TERM_LINES, lines);
			terms->Set(per::TERM_LEVEL_TIE, (long long)item->GetLevelLimit() * PLAYERBOT_ARMOR_LEVEL_TIE_BREAK);
			if (listed)
				terms->Set(per::TERM_LISTED_JEWEL_PCT, PLAYERBOT_JEWEL_LIST_PREFERENCE_PERCENT);
			terms->Set(per::TERM_TOTAL, score);
		}
		return score;
	}

	// The score, and with `terms` its parts (playerbot_explain.h).
	long long GetPlayerBotEquipmentScoreTerms(LPITEM item, LPCHARACTER ch, TPlayerBotScoreTerms* terms);

	long long GetPlayerBotEquipmentScore(LPITEM item, LPCHARACTER ch = NULL)
	{
		// MT2009_PLUS_LEGENDS_V1 (gear): a Specjalny and up weighs a piece's
		// plus and bonuses more (playerbot_legend_tier.h).
		return AdjustPlayerBotLegendGearScore(item, ch, GetPlayerBotEquipmentScoreTerms(item, ch, NULL));
	}

	long long GetPlayerBotEquipmentScoreTerms(LPITEM item, LPCHARACTER ch, TPlayerBotScoreTerms* terms)
	{
		if (!item || !item->GetProto())
			return 0;
		if (terms)
		{
			terms->Set(per::TERM_LEVEL_LIMIT, item->GetLevelLimit());
			terms->Set(per::TERM_PLUS, item->GetRefineLevel());
		}
		if (ch && item->GetType() == ITEM_ARMOR && item->GetSubType() == ARMOR_FOOTS)
			return GetPlayerBotBootsScore(item, ch, terms);

		long long score = 1;
		if (item->GetType() == ITEM_WEAPON)
		{
			// One expected hit, a thousand a point, so the flat lines and the
			// class preferences below keep the proportions they always had.
			// Iwakura's tier of the family goes on the blow alone, and
			// PLAYERBOT_WEAPON_TIER_BLOW_PERCENT a grade: at eight a grade on
			// the whole score it outweighed the blow, and 32 of 99 weapon
			// changes went to a weapon that hits softer. At an equal blow the
			// better family wins (the tier as a tie-break).
			long long blow = GetPlayerBotWeaponHitDamage(item, ch, terms) * 1000;
			if (ch)
			{
				const int tier = GetPlayerBotItemTierOf(item, ch);
				if (tier > 0)
				{
					blow = blow * (100 + (tier - 3) * PLAYERBOT_WEAPON_TIER_BLOW_PERCENT) / 100;
					score += tier;
					if (terms)
						terms->Set(per::TERM_TIER_PCT, (tier - 3) * PLAYERBOT_WEAPON_TIER_BLOW_PERCENT);
				}
			}
			score += blow;
			if (terms)
				terms->Set(per::TERM_BLOW_X1000, blow);
			const long long beforePreference = score;

			// A level-30 average-damage weapon used to be handed a flat 350000
			// here. Damage is scored at a thousand a point, so that was more than
			// any weapon in the game is worth and no bot ever replaced one: an
			// FMS at +4 scored 524000 against 208000 for a level-36 sword at +7,
			// which is the better weapon by the numbers above. These weapons are
			// still protected from a merchant - IsPlayerBotJunkItem says a
			// level-30 weapon is never junk - but protecting them is not the same
			// as pretending nothing can beat them.

			if (ch)
			{
				if (ch->GetJob() == JOB_ASSASSIN)
				{
					if (ch->GetSkillGroup() == 2 && item->GetSubType() == WEAPON_BOW)
						score += 500000; // Prefer bows for Archer Ninja
					else if (ch->GetSkillGroup() == 1 && item->GetSubType() == WEAPON_DAGGER)
						score += 300000; // Prefer daggers for Dagger Ninja
				}
				else if (ch->GetJob() == JOB_WARRIOR)
				{
					// The two-handed weapon is worth the preference once the bot
					// fights from the battle horse (level 11): that is where a
					// Mental Warrior breaks Metin stones with it. Before that the
					// flat bonus made a +4 spike win over a +6 sword with a
					// thirty-percent bonus against monsters.
					// A share of its own blow, not a flat two hundred points of it:
					// at 75 two hundred is most of what a level-ten two-hander hits
					// for, so the flat bonus let one outscore a far better sword
					// (a Gilotynowe Ostrze +7 in the hand of a warrior of 75,
					// 15 September). score is one plus the blow here.
					if (ch->GetSkillGroup() == 2 && item->GetSubType() == WEAPON_TWO_HANDED &&
							ch->GetHorseLevel() >= PLAYERBOT_BATTLE_HORSE_LEVEL)
						score += (score - 1) * PLAYERBOT_TWO_HANDED_PREFERENCE_PERCENT / 100;
					else if (ch->GetSkillGroup() == 1 && item->GetSubType() == WEAPON_SWORD)
						score += 200000; // Prefer sword for Body Warrior
				}
			}
			if (terms)
				terms->Set(per::TERM_CLASS_PREF, score - beforePreference);
		}
		else if (item->GetType() == ITEM_ARMOR &&
				(item->GetSubType() == ARMOR_BODY || item->GetSubType() == ARMOR_HEAD ||
				 item->GetSubType() == ARMOR_FOOTS || item->GetSubType() == ARMOR_SHIELD))
		{
			score += (long long)(item->GetValue(1) + 2 * item->GetValue(5)) * 1000;
			if (terms)
				terms->Set(per::TERM_DEFENCE_X1000, (long long)(item->GetValue(1) + 2 * item->GetValue(5)) * 1000);
			// A piece is worth what it gives, whatever level it asks for. An
			// outgrown piece used to lose five percent of its defence for every
			// level past twenty, to move a bot up the tiers, and so a bot of
			// forty or so wore a Pieciokatna Tarcza +4 (34 defence, -6% speed,
			// level 21) over a Bojowa Tarcza +7 in its bag (45, -2%, level 0)
			// - "pomimo ze bojowa+7 daje lepsze staty on woli nosic
			// pieciokatna", "wbudowane bonusy to tez bonusy" (Iwakura,
			// 23 September): the shield's own defence and speed are its lines
			// as much as anything rolled on it. The tiers are climbed another
			// way: the merchant sells the next tier by level whatever the bot
			// wears (BuyPlayerBotBestMerchantSlotGear), and the higher-tier
			// piece in the bag is kept and refined (IsPlayerBotHigherTierSpare)
			// until its own numbers win - the Pieciokatna at +6 does. The level
			// only breaks a tie (PLAYERBOT_ARMOR_LEVEL_TIE_BREAK).
			score += (long long)item->GetLevelLimit() * PLAYERBOT_ARMOR_LEVEL_TIE_BREAK;
			if (terms)
				terms->Set(per::TERM_LEVEL_TIE, (long long)item->GetLevelLimit() * PLAYERBOT_ARMOR_LEVEL_TIE_BREAK);
		}

		// A weapon's two damage-percent lines were folded into its attack
		// above; everything else is a flat line.
		const bool bWeaponHitDone = item->GetType() == ITEM_WEAPON;
		long long protoLines = 0, rolledLines = 0;
		for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
		{
			const BYTE t = item->GetProto()->aApplies[i].bType;
			if (bWeaponHitDone && IsPlayerBotHitModelApply(t, ch))
				continue;
			protoLines += ScorePlayerBotApplyTiered(t, item->GetProto()->aApplies[i].lValue, ch);
		}
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			const BYTE t = item->GetAttributeType(i);
			if (bWeaponHitDone && IsPlayerBotHitModelApply(t, ch))
				continue;
			rolledLines += ScorePlayerBotApplyTiered(t, item->GetAttributeValue(i), ch);
		}
		score += protoLines + rolledLines;
		// The soul stones in its sockets are lines of the piece too: a weapon
		// holding Potwora and Smierci +4 is a different weapon from the same
		// one with the sockets open, and a swap for a bare one of a point
		// more would throw both stones away.
		const long long seated = ScorePlayerBotSeatedSoulStones(item, ch);
		score += seated;

		if (item->GetImmuneFlag() != 0)
			score += 1000;
		if (terms)
		{
			terms->Set(per::TERM_PROTO_APPLIES, protoLines);
			terms->Set(per::TERM_LINES, rolledLines);
			terms->Set(per::TERM_SOUL_STONES, seated);
			terms->Set(per::TERM_IMMUNE, item->GetImmuneFlag() != 0 ? 1000 : 0);
		}

		// A race-attack bonus is only worth carrying where that race is what you
		// actually fight, and it is worth what share of the map that race is:
		// "strong against orcs" covers 63% of Orc Valley, "strong against
		// animals" the whole of a Monkey Dungeon, and nothing at all on the
		// desert, which is made of a race no item can reach. The share comes
		// from the same call the reroll scorer uses, so the pass that buys an
		// item and the pass that rerolls it can no longer disagree about the
		// line that made the bot pick it up.
		if (ch && item->GetType() != ITEM_WEAPON)
		{
			int racePercent = 0;
			const int dominant = GetPlayerBotFightingRace(ch, &racePercent);
			if (dominant != PLAYERBOT_RACE_NONE && racePercent > 0)
			{
				const BYTE wanted = GetPlayerBotRaceApplyType(dominant);
				const long long perPoint = (long long)PLAYERBOT_GEAR_RACE_LINE_VALUE *
						racePercent / 100;
				const long long beforeRace = score;
				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					if (item->GetAttributeType(i) == wanted)
						score += (long long)item->GetAttributeValue(i) * perPoint;
				}
				for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
				{
					if (item->GetProto()->aApplies[i].bType == wanted)
						score += (long long)item->GetProto()->aApplies[i].lValue * perPoint;
				}
				if (terms)
					terms->Set(per::TERM_RACE_LINES, score - beforeRace);
			}
		}

		// Iwakura's tier of the family, as a nudge on the whole: the family's
		// own lines are already in the score, so this is his verdict on what
		// they are worth together, not a second count of them. Bounded by
		// PLAYERBOT_TIER_SCORE_PERCENT a step so that a +9 with lines still
		// beats a +1 of a better family with none.
		// A weapon's tier went on its blow above.
		if (ch && item->GetType() != ITEM_WEAPON)
		{
			const int tier = GetPlayerBotItemTierOf(item, ch);
			if (tier > 0)
			{
				score = score * (100 + (tier - 3) * PLAYERBOT_TIER_SCORE_PERCENT) / 100;
				if (terms)
					terms->Set(per::TERM_TIER_PCT, (tier - 3) * PLAYERBOT_TIER_SCORE_PERCENT);
			}
		}
		// And his list of the jewellery worth wearing, for the bots that go by
		// it (IsPlayerBotJewelListFollower; Patch 4, point 1).
		if (ch && IsPlayerBotListedJewel(ch, item) && IsPlayerBotJewelListFollower(ch))
		{
			score = score * (100 + PLAYERBOT_JEWEL_LIST_PREFERENCE_PERCENT) / 100;
			if (terms)
				terms->Set(per::TERM_LISTED_JEWEL_PCT, PLAYERBOT_JEWEL_LIST_PREFERENCE_PERCENT);
		}
		if (terms)
			terms->Set(per::TERM_TOTAL, score);
		return score;
	}

	// The best weapon in the bag this character can wear now, other than
	// `except`, and its score.
	LPITEM FindPlayerBotBestBagWeapon(LPCHARACTER ch, LPITEM except, long long* scoreOut)
	{
		LPITEM best = NULL;
		long long bestScore = 0;
		for (WORD cell = 0; ch && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item == except || item->GetCell() != cell || item->IsEquipped() ||
					item->GetType() != ITEM_WEAPON || !IsPlayerBotEquipmentCandidate(ch, item) ||
					item->GetLevelLimit() > ch->GetLevel() || item->FindEquipCell(ch) != WEAR_WEAPON)
				continue;
			const long long score = GetPlayerBotEquipmentScore(item, ch);
			if (!best || score > bestScore || (score == bestScore && item->GetID() < best->GetID()))
			{
				best = item;
				bestScore = score;
			}
		}
		if (scoreOut)
			*scoreOut = bestScore;
		return best;
	}

	// A blacksmith session's memory of what it took off (B12 of Iwakura's
	// audit), forgotten when the session puts things back on or runs out.
	void ClearPlayerBotRefineTakenOff(TPlayerBotAIState& state)
	{
		for (int wear = 0; wear < WEAR_MAX_NUM; ++wear)
			state.adwRefineTakenOffItem[wear] = 0;
	}

	// The wear slot this bag piece was taken off by the session, -1 for none:
	// by its id, while the slot stands empty. No bag walk - the refine pass
	// asks it of every piece in the bag.
	int GetPlayerBotTakenOffSlotOf(LPCHARACTER ch, const TPlayerBotAIState& state, LPITEM item)
	{
		if (!ch || !item || item->GetID() == 0 || item->IsEquipped() ||
				get_dword_time() - state.dwRefineTakenOffAt >= PLAYERBOT_REFINE_TAKEN_OFF_MS)
			return -1;
		const int slot = item->FindEquipCell(ch);
		if (slot < 0 || slot >= WEAR_MAX_NUM || state.adwRefineTakenOffItem[slot] != item->GetID() ||
				ch->GetWear((WORD)slot) != NULL)
			return -1;
		return slot;
	}

	// The piece a blacksmith session took off this slot for the anvil
	// (TPlayerBotAIState::adwRefineTakenOffItem), while the slot is still
	// empty and the piece still in the bag: the worn one, whatever the bag
	// holds beside it. Asked with the slot empty only, which is a session's
	// few seconds, so the lookup costs nothing the rest of the time.
	LPITEM FindPlayerBotTakenOffPiece(LPCHARACTER ch, int wearCell)
	{
		if (!ch || wearCell < 0 || wearCell >= WEAR_MAX_NUM || ch->GetWear(wearCell))
			return NULL;
		TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (st == s_mapPlayerBotAIStates.end())
			return NULL;
		const DWORD id = st->second.adwRefineTakenOffItem[wearCell];
		if (id == 0 || get_dword_time() - st->second.dwRefineTakenOffAt >= PLAYERBOT_REFINE_TAKEN_OFF_MS)
			return NULL;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetID() == id && item->GetCell() == cell && !item->IsEquipped())
				return item;
		}
		return NULL;
	}

	// The weapon in the hand, or - with the hand empty, as it is for a whole
	// blacksmith session, or holding a rod or a pickaxe - the one that goes
	// back into it. A tool is not a weapon with a blow of nothing: read as one,
	// an angler's hand was outclassed by the whole atlas and every weapon on a
	// counter was worth its savings. With the hand emptied by the anvil it is
	// the piece the session took off, not whatever scores best in the bag.
	LPITEM GetPlayerBotHandWeapon(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return NULL;
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		if (worn && worn->GetType() == ITEM_WEAPON)
			return worn;
		LPITEM taken = FindPlayerBotTakenOffPiece(ch, WEAR_WEAPON);
		if (taken && taken->GetType() == ITEM_WEAPON && IsPlayerBotEquipmentCandidate(ch, taken) &&
				taken->GetLevelLimit() <= ch->GetLevel())
			return taken;
		return FindPlayerBotBestBagWeapon(ch, NULL, NULL);
	}

	// A piece's family: the +0 of its refine chain, as the price table and
	// the level-30 keep count one.
	DWORD GetPlayerBotItemFamily(LPITEM item)
	{
		if (!item)
			return 0;
		const int plus = std::max(0, (int)item->GetRefineLevel());
		return item->GetVnum() >= (DWORD)plus ? item->GetVnum() - (DWORD)plus : item->GetVnum();
	}

	// The weapon a bot keeps for the day the one in its hand burns: the best
	// other weapon in the bag it can wear that playerbot_refine_rules::
	// IsBackupWeaponFor takes - scoring PLAYERBOT_REFINE_BACKUP_SCORE_PERCENT
	// of the hand's, a copy of the hand's family, or one of the hand's level
	// or of the merchant's best, whichever is lower. It is not a gift, not
	// merchant scrap and not counter goods; without one the refine pass holds
	// the hand's weapon off the plain anvil (IsPlayerBotWornWeaponAtRisk), and
	// the weapon merchant sells one (NeedsPlayerBotBackupWeapon).
	LPITEM FindPlayerBotBackupWeapon(LPCHARACTER ch)
	{
		LPITEM hand = GetPlayerBotHandWeapon(ch);
		if (!hand)
			return NULL;
		const long long handScore = GetPlayerBotEquipmentScore(hand, ch);
		const DWORD handFamily = GetPlayerBotItemFamily(hand);
		const int merchantTop = GetPlayerBotProperMerchantTopLevel(ch);
		// Never a spare at +7 or past that the hand matches or beats: that one
		// is goods (IsPlayerBotFinishedSpareGoods; Iwakura's Patch 4, point 7:
		// "bot zawsze zachowuje dla siebie tylko jeden, najlepszy egzemplarz").
		// The backup is the best of the rest.
		LPITEM backup = NULL;
		long long backupScore = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item == hand || item->GetCell() != cell || item->IsEquipped() ||
					item->GetType() != ITEM_WEAPON || !IsPlayerBotEquipmentCandidate(ch, item) ||
					item->GetLevelLimit() > ch->GetLevel() || item->FindEquipCell(ch) != WEAR_WEAPON)
				continue;
			const long long score = GetPlayerBotEquipmentScore(item, ch);
			if (item->GetRefineLevel() >= PLAYERBOT_SPARE_GOODS_MIN_PLUS && score <= handScore)
				continue;
			if (!playerbot_refine_rules::IsBackupWeaponFor(handScore, score, PLAYERBOT_REFINE_BACKUP_SCORE_PERCENT,
					GetPlayerBotItemFamily(item) == handFamily, (int)hand->GetLevelLimit(), (int)item->GetLevelLimit(),
					merchantTop))
				continue;
			if (!backup || score > backupScore || (score == backupScore && item->GetID() < backup->GetID()))
			{
				backup = item;
				backupScore = score;
			}
		}
		return backup;
	}

	// FindPlayerBotBackupWeapon scores every weapon in the bag, and the junk
	// rule and the planner's refine question ask it once for every weapon in
	// the bag - the square of the bag's weapons, on every pass of 1100 bots
	// (the tick went from 8-9 s of 60 to 11 on its first deploy). The answer is
	// kept by item id for PLAYERBOT_BACKUP_WEAPON_CACHE_MS; what acts on it at
	// once - a gift, the refine at the anvil - asks afresh.
	struct TPlayerBotBackupWeaponAnswer
	{
		DWORD dwTime;
		DWORD dwItemID;
	};
	std::map<DWORD, TPlayerBotBackupWeaponAnswer> s_mapPlayerBotBackupWeapon;

	DWORD GetPlayerBotBackupWeaponID(LPCHARACTER ch, bool fresh)
	{
		if (!ch)
			return 0;
		const DWORD now = get_dword_time();
		TPlayerBotBackupWeaponAnswer& answer = s_mapPlayerBotBackupWeapon[ch->GetPlayerID()];
		if (fresh || answer.dwTime == 0 || now - answer.dwTime >= PLAYERBOT_BACKUP_WEAPON_CACHE_MS)
		{
			LPITEM backup = FindPlayerBotBackupWeapon(ch);
			answer.dwTime = now != 0 ? now : 1;
			answer.dwItemID = backup ? backup->GetID() : 0;
		}
		return answer.dwItemID;
	}

	bool IsPlayerBotKeptBackupWeapon(LPCHARACTER ch, LPITEM item, bool fresh = false)
	{
		// MT2009_PLUS_BOT_SHAMAN_INT_SET_V1: the Shaman's INT weapon is kept
		// the way the backup is.
		if (ch && item && item->GetType() == ITEM_WEAPON && IsPlayerBotBuffSetPiece(ch, item))
			return true;
		return ch && item && item->GetType() == ITEM_WEAPON && item->GetID() != 0 &&
				GetPlayerBotBackupWeaponID(ch, fresh) == item->GetID();
	}

	// A bot gives nothing away. It used to hand a spare it had outgrown to a
	// weaker bot of its class standing nearby, and the counter never saw it: a
	// bot raised Srebrne Kolczyki from +1 to +6 in a minute and gave them to
	// another while it wore copper ones itself (AkhiGubernator, 15 September:
	// "dobry samarytanin"). "Niech handluja ale nie daja za darmo" :
	// what comes off stays in the bag, and the junk rule and the counter
	// decide what becomes of it. Nor does a party pass anything on any more:
	// the book of another class and the material a member was short of went
	// the same way ("usun", 15 September).

	// The Archer's stone weapon (by build, whatever is in the hand - the
	// IsPlayerBotArcher of playerbot_targeting.h asks for the bow). A bow cannot break a Metin: the stone does
	// not move, the arrows run out, the shot's rhythm is a fraction of a
	// swing's, and the bot "fell over x times and gave up" (Kuszaa). A dagger
	// or a sword the ninja can wear, kept in the bag, goes into the hand for
	// the stone and comes out afterwards. Both are one item per bot - the
	// best by the equipment score - and the junk rule and the counter leave
	// that one alone.
	bool IsPlayerBotArcherBuild(LPCHARACTER ch)
	{
		return ch && ch->GetJob() == JOB_ASSASSIN && ch->GetSkillGroup() == 2;
	}

	bool IsPlayerBotStoneMeleeWeapon(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetType() != ITEM_WEAPON)
			return false;
		const BYTE sub = item->GetSubType();
		return (sub == WEAPON_DAGGER || sub == WEAPON_SWORD) && item->CanUsedBy(ch) &&
				item->GetLevelLimit() <= ch->GetLevel();
	}

	// The best stone weapon the bot holds: in the bag, or - with includeWorn -
	// in the hand as well. NULL when there is none. A dagger beats a sword
	// whatever the score: it swings faster and costs less, and a stone has no
	// armour worth a heavier blow.
	LPITEM FindPlayerBotStoneWeapon(LPCHARACTER ch, bool includeWorn)
	{
		if (!ch)
			return NULL;
		LPITEM best = NULL;
		long long bestScore = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || !IsPlayerBotStoneMeleeWeapon(ch, item))
				continue;
			const bool dagger = item->GetSubType() == WEAPON_DAGGER;
			const bool bestDagger = best && best->GetSubType() == WEAPON_DAGGER;
			if (best && bestDagger && !dagger)
				continue;
			const long long score = GetPlayerBotEquipmentScore(item, ch);
			if (!best || (dagger && !bestDagger) || score > bestScore)
			{
				best = item;
				bestScore = score;
			}
		}
		if (includeWorn && !best)
		{
			LPITEM worn = ch->GetWear(WEAR_WEAPON);
			if (worn && IsPlayerBotStoneMeleeWeapon(ch, worn))
				best = worn;
		}
		return best;
	}

	// The one blade an Archer keeps for Metin stones - the chosen bag weapon, or
	// the one worn while it is on a stone. It is worth refining even though it is
	// never a wearable upgrade or a higher-tier spare, because a bow cannot break
	// a stone and a +0 dagger barely can.
	bool IsPlayerBotArcherStoneWeapon(LPCHARACTER ch, LPITEM item)
	{
		if (!item || !IsPlayerBotArcherBuild(ch) || !IsPlayerBotStoneMeleeWeapon(ch, item))
			return false;
		return FindPlayerBotStoneWeapon(ch, false) == item ||
				ch->GetWear(WEAR_WEAPON) == item;
	}

	// Whether the Archer holds a dagger good enough to break a stone: at least
	// +4, worn or in the bag. Below that a stone is not worth taking on alone -
	// the dagger is refined at the blacksmith towards this first.
	bool HasPlayerBotUsableStoneDagger(LPCHARACTER ch)
	{
		LPITEM w = FindPlayerBotStoneWeapon(ch, true);
		return w && w->GetRefineLevel() >= PLAYERBOT_ARCHER_STONE_MIN_REFINE;
	}

	// What the hand should hold right now: the job's weapon, or the stone
	// weapon while an Archer is on a stone.
	bool PlayerBotWeaponFitsNow(LPCHARACTER ch, const TPlayerBotAIState& state, LPITEM item)
	{
		if (IsPlayerBotArcherBuild(ch) && state.bMeleeForStone)
			return IsPlayerBotStoneMeleeWeapon(ch, item);
		return IsPlayerBotWeapon(ch, item);
	}

	// What the rolled lines of a piece are worth, by Iwakura's tiers - the
	// equipment score's own count of them (ScorePlayerBotApplyTiered), without
	// the base the piece is made of.
	long long GetPlayerBotItemLineScore(LPITEM item, LPCHARACTER ch)
	{
		long long lines = 0;
		for (int i = 0; item && i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			if (item->GetAttributeType(i) != 0 && item->GetAttributeValue(i) > 0)
				lines += ScorePlayerBotApplyTiered(item->GetAttributeType(i), item->GetAttributeValue(i), ch);
		return lines;
	}

	// Defined in playerbot_bonus.h, later: a stone in the bag for this piece.
	bool PlayerBotHoldsBonusStoneFor(LPCHARACTER ch, LPITEM item);

	// Iwakura's community patch 2, point 3 ("zasada wymiany ekwipunku"): a
	// bot wearing a piece with, say, fifteen hundred health keeps a new one in
	// the bag until it has bonused it and the new lines outweigh the old -
	// whatever the new base is worth. The weapon is outside it: its average
	// line is in its blow already (GetPlayerBotWeaponHitDamage). And the wait
	// lasts only while it can end: with no stone in the bag for the new piece
	// the swap goes by the score as ever, or a bot without stones would keep
	// its level-one boots for life.
	bool IsPlayerBotSwapHeldForBonus(LPCHARACTER ch, LPITEM candidate, LPITEM worn)
	{
		if (!ch || !candidate || !worn || !IsPlayerBotPersonaEnabled() ||
				candidate->GetType() == ITEM_WEAPON || worn->GetType() == ITEM_WEAPON)
			return false;
		const long long wornLines = GetPlayerBotItemLineScore(worn, ch);
		if (wornLines <= 0 || GetPlayerBotItemLineScore(candidate, ch) >= wornLines)
			return false;
		return PlayerBotHoldsBonusStoneFor(ch, candidate);
	}

	// The same wait for an empty slot. A blacksmith session takes the worn
	// piece off to refine it, so when the equipment pass comes to put things
	// back the slot is empty, the test above has nothing to compare with, and
	// the new piece that was waiting for its lines went on in front of the old
	// one it was waiting behind (B27 of Iwakura's audit). The piece in the bag
	// with better lines that could go on now stands in for the worn one.
	LPITEM FindPlayerBotBetterLinedRival(LPCHARACTER ch, LPITEM item, int wearCell)
	{
		if (!ch || !item || !IsPlayerBotPersonaEnabled() || item->GetType() == ITEM_WEAPON)
			return NULL;
		long long rivalLines = GetPlayerBotItemLineScore(item, ch);
		LPITEM rival = NULL;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM other = ch->GetInventoryItem(cell);
			if (!other || other == item || other->GetCell() != cell || other->IsEquipped() ||
					other->GetType() == ITEM_WEAPON || !IsPlayerBotEquipmentCandidate(ch, other) ||
					other->FindEquipCell(ch) != wearCell ||
					!PlayerBotCanEquipNow(ch, other, TItemPos(INVENTORY, cell)))
				continue;
			const long long lines = GetPlayerBotItemLineScore(other, ch);
			if (lines > rivalLines)
			{
				rival = other;
				rivalLines = lines;
			}
		}
		return rival && PlayerBotHoldsBonusStoneFor(ch, item) ? rival : NULL;
	}

	// Defined further down, with the arrows' purchase.
	int CountPlayerBotArrows(LPCHARACTER ch);
	bool UpgradePlayerBotArrows(LPCHARACTER ch);

	// A weapon change needs PLAYERBOT_WEAPON_SWAP_MARGIN_PERCENT over the
	// weapon in the hand, and a return to the one taken off within
	// PLAYERBOT_WEAPON_RETURN_WINDOW_MS PLAYERBOT_WEAPON_RETURN_MARGIN_PERCENT:
	// two weapons a point apart took turns in the hand. Keyed by pid, what
	// came off and when.
	const int PLAYERBOT_WEAPON_SWAP_MARGIN_PERCENT = 1;
	const int PLAYERBOT_WEAPON_RETURN_MARGIN_PERCENT = 5;
	const DWORD PLAYERBOT_WEAPON_RETURN_WINDOW_MS = 60 * 60 * 1000;
	std::map<DWORD, std::pair<DWORD, DWORD> > s_mapPlayerBotWeaponTakenOff;

	bool PlayerBotWeaponSwapClears(LPCHARACTER ch, LPITEM item, long long itemScore, long long oldScore, DWORD dwNow)
	{
		if (oldScore <= 0)
			return true;
		int margin = PLAYERBOT_WEAPON_SWAP_MARGIN_PERCENT;
		std::map<DWORD, std::pair<DWORD, DWORD> >::const_iterator off =
				s_mapPlayerBotWeaponTakenOff.find(ch->GetPlayerID());
		if (off != s_mapPlayerBotWeaponTakenOff.end() && off->second.first == item->GetID() &&
				dwNow - off->second.second < PLAYERBOT_WEAPON_RETURN_WINDOW_MS)
			margin = PLAYERBOT_WEAPON_RETURN_MARGIN_PERCENT;
		return itemScore * 100 > oldScore * (100 + margin);
	}

	bool ManagePlayerBotEquipment(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || IsPlayerBotGearFrozen(ch))
			return false;
		// MT2009_PLUS_BOT_SHAMAN_INT_SET_V1: a Shaman in its INT set keeps it
		// on until the set's own pass takes it off - this pass would put the
		// fighting pieces straight back.
		if (MaintainPlayerBotBuffSet(ch, state, dwNow) || IsPlayerBotBuffSetDressed(ch))
			return false;

		// The Archer's stone mode, decided here because this pass is what
		// puts a weapon in the hand: on while the target is a standing stone
		// and a stone weapon is at hand, off the moment it is not - either
		// flip is looked at on this very tick.
		if (IsPlayerBotArcherBuild(ch))
		{
			LPCHARACTER target = state.dwTargetVID != 0
					? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
			// Only a dagger at +4 or better puts the Archer into melee; below that
			// it stays on the bow and reaches a stone only when others are already
			// breaking it (CanPlayerBotEngageStone). And never while a person has
			// asked this bot to lure: IsPlayerBotArcher wants the bow in the hand,
			// so a dagger drawn for one stone makes the whole course "ineligible"
			// until the stone is gone - with nothing anywhere saying why.
			// The Demon Tower is the other exception, and there the bow stays
			// in the hand for the stones too, unless the arrows are gone: its
			// stones stand among the floor's demons, and an Archer that walked
			// in to stab one was a bot in melee with a pack it cannot hold
			// ("ninja archerzy fajnie jakby stali z daleka i strzelali, a nie
			// podbiegali i bili z bliska", prodnathin, 23 September). A raid
			// breaks those stones together, so the shot's rhythm is not what
			// decides them.
			const bool inTower = ch->GetMapIndex() == PLAYERBOT_MAP_DEMON_TOWER ||
					IsPlayerBotDemonTowerInstance(ch->GetMapIndex());
			const bool wantMelee = target && !target->IsDead() &&
					state.dwLurePlayerPID == 0 && HasPlayerBotUsableStoneDagger(ch) &&
					(inTower ? CountPlayerBotArrows(ch) == 0 : target->IsStone());
			if (wantMelee != state.bMeleeForStone)
			{
				state.bMeleeForStone = wantMelee;
				state.dwNextEquipmentCheckTime = dwNow;
				sys_log(0, "PLAYERBOT_GEAR: archer %s pid=%u name=%s target_vid=%u",
						wantMelee ? "draws the stone weapon" : "takes the bow back",
						ch->GetPlayerID(), ch->GetName(), (unsigned int)state.dwTargetVID);
			}
		}

		if (dwNow < state.dwNextEquipmentCheckTime && !state.bEquipPending)
			return false;
		// What each slot holds now, for the explanation of a later decision
		// (CONTEXT_WENT_TO_COUNTER, playerbot_explain.h).
		NotePlayerBotExplainWorn(ch);

		// A better arrow goes in on this pass's clock: the quiver is never
		// emptied, so running out no longer brings the next one to the slot.
		if (IsPlayerBotArcherBuild(ch))
			UpgradePlayerBotArrows(ch);

		LPITEM bestItem = NULL;
		LPITEM bestOldItem = NULL;
		int bestWearCell = -1;
		long long bestImprovement = 0;
		long long bestScore = 0;
		// The rule that took the best candidate, and whether the worn piece's
		// score was set to nothing by one (playerbot_explain.h).
		int bestRule = per::RULE_SCORE_UPGRADE;
		bool bestOldZeroed = false;

		const bool stoneMode = IsPlayerBotArcherBuild(ch) && state.bMeleeForStone;
		// The one stone weapon the bot has chosen (dagger first), not any
		// blade in the bag: the score alone would put a heavier sword ahead.
		LPITEM chosenStoneWeapon = stoneMode ? FindPlayerBotStoneWeapon(ch, false) : NULL;
		// What a companion's owner put on goes on first, whatever it scores,
		// and nothing below is ranked against it (playerbot_sidekick.h): the
		// owner's word over the pass's, or the two would take turns.
		int pinnedWear = -1;
		LPITEM pinned = FindPlayerBotSidekickPinnedInBag(ch, pinnedWear, true);
		if (pinned)
		{
			bestItem = pinned;
			bestOldItem = ch->GetWear(pinnedWear);
			bestWearCell = pinnedWear;
			bestImprovement = 1;
			bestScore = GetPlayerBotEquipmentScore(pinned, ch);
			bestRule = per::RULE_OWNER_PIN;
		}
		for (WORD cell = 0; !pinned && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;
			const bool stoneWeapon = stoneMode && item == chosenStoneWeapon &&
					!item->IsExchanging();
			if (!stoneWeapon && !IsPlayerBotEquipmentCandidate(ch, item))
				continue;
			if (item->GetType() == ITEM_WEAPON && !PlayerBotWeaponFitsNow(ch, state, item))
				continue;

			const int wearCell = item->FindEquipCell(ch);
			if (wearCell < 0 || wearCell >= WEAR_MAX_NUM)
				continue;

			LPITEM oldItem = ch->GetWear(wearCell);
			if (oldItem && !IsPlayerBotWornItemSound(ch, oldItem, wearCell))
			{
				PlayerBotLogThrottled("wear_slot_unsound", dwNow,
						"PLAYERBOT_AI: wear slot holds an item the engine does not wear pid=%u name=%s wear=%d",
						ch->GetPlayerID(), ch->GetName(), wearCell);
				continue;
			}
			if (oldItem && IS_SET(oldItem->GetFlag(), ITEM_FLAG_IRREMOVABLE))
				continue;
			if (oldItem && IsPlayerBotSidekickPinned(ch, oldItem))
				continue;
#if defined(PLAYERBOT_ENGINE_MT2009)
			// A fishing pass the fishing asked for a moment ago stays on; see
			// IsPlayerBotFishingPassHeld for the loop that taking it off made.
			if (oldItem && oldItem->GetVnum() == UNIQUE_ITEM_FISHING_PASS &&
					IsPlayerBotFishingPassHeld(ch->GetPlayerID(), dwNow))
				continue;
#endif
			// A ring or a glove on its clock comes off only through the
			// unique-slot pass, which knows whether the bot is hunting.
			// MT2009_PLUS_BOT_RANK_GLOVE_V1: and so does the Prophecy King's
			// Glove or Symbol, which it wears only at a negative rank.
			if (oldItem && (IsPlayerBotTimedUnique(oldItem->GetVnum()) ||
					IsPlayerBotRankUnique(oldItem->GetVnum())))
				continue;

			if (!PlayerBotCanEquipNow(ch, item, TItemPos(INVENTORY, cell)))
				continue;

			const long long itemScore = GetPlayerBotEquipmentScore(item, ch);
			// A weapon in the hand that does not fit the moment - the bow while
			// the Archer is on a stone, the dagger once the stone is gone - is
			// worth nothing against the one that does. Nor does the level-one
			// sword +6 a bot of thirty holds (IsPlayerBotBannedLowWeapon): by the
			// damage model it out-hits a level-30 weapon at +0, whose refine is
			// the whole point, and any weapon over the line takes its place. The
			// Archer's stone tool keeps its own rule (FindPlayerBotStoneWeapon).
			// And an outdated shield, helmet or body armour gives way to any
			// piece of its slot that is not (PlayerBotOutdatedGearGivesWay).
			const bool outdatedGivesWay = oldItem && PlayerBotOutdatedGearGivesWay(ch, oldItem, item);
			// Nor does an outdated piece ever go on over one that is not: the
			// two took turns in the slot every second.
			if (oldItem && PlayerBotOutdatedGearGivesWay(ch, item, oldItem))
				continue;
			// The same test as ever, its two halves kept apart for the explanation.
			const bool oldDoesNotFit = oldItem && wearCell == WEAR_WEAPON && !PlayerBotWeaponFitsNow(ch, state, oldItem);
			const bool oldBanned = oldItem && wearCell == WEAR_WEAPON && !oldDoesNotFit && !stoneMode &&
					IsPlayerBotBannedLowWeapon(ch, oldItem);
			const long long oldScore = oldItem
					? ((oldDoesNotFit || oldBanned || outdatedGivesWay) ? 0 : GetPlayerBotEquipmentScore(oldItem, ch))
					: 0;
			if (oldItem && itemScore <= oldScore)
				continue;
			if (oldItem && wearCell == WEAR_WEAPON && !stoneMode &&
					!PlayerBotWeaponSwapClears(ch, item, itemScore, oldScore, dwNow))
				continue;
			// A new piece waits in the bag while its lines are worth less than
			// the worn one's and a stone can change that (community patch 2,
			// point 3); the bonus pass works on it there.
			if (oldItem && !outdatedGivesWay && IsPlayerBotSwapHeldForBonus(ch, item, oldItem))
			{
				PlayerBotLogThrottled("swap_held_for_bonus", dwNow,
						"PLAYERBOT_BONUS: new piece waits in the bag for its lines pid=%u name=%s wear=%d new_vnum=%u old_vnum=%u new_lines=%lld old_lines=%lld",
						ch->GetPlayerID(), ch->GetName(), wearCell, item->GetVnum(), oldItem->GetVnum(),
						GetPlayerBotItemLineScore(item, ch), GetPlayerBotItemLineScore(oldItem, ch));
				continue;
			}
			if (!oldItem)
			{
				LPITEM rival = FindPlayerBotBetterLinedRival(ch, item, wearCell);
				if (rival)
				{
					PlayerBotLogThrottled("swap_held_for_bonus_empty", dwNow,
							"PLAYERBOT_BONUS: new piece waits in the bag for its lines, the slot empty pid=%u name=%s wear=%d new_vnum=%u rival_vnum=%u new_lines=%lld rival_lines=%lld",
							ch->GetPlayerID(), ch->GetName(), wearCell, item->GetVnum(), rival->GetVnum(),
							GetPlayerBotItemLineScore(item, ch), GetPlayerBotItemLineScore(rival, ch));
					continue;
				}
			}

			const long long improvement = oldItem ? itemScore - oldScore : 1000000000000LL + itemScore;
			if (!bestItem || improvement > bestImprovement)
			{
				bestItem = item;
				bestOldItem = oldItem;
				bestWearCell = wearCell;
				bestImprovement = improvement;
				bestScore = itemScore;
				bestRule = !oldItem ? per::RULE_EMPTY_SLOT
						: (oldDoesNotFit && IsPlayerBotArcherBuild(ch) ? per::RULE_ARCHER_STONE_SWITCH
							: (oldBanned ? per::RULE_OLD_LOW_WEAPON_BANNED : per::RULE_SCORE_UPGRADE));
				bestOldZeroed = oldItem && (oldDoesNotFit || oldBanned || outdatedGivesWay);
			}
		}

		if (!bestItem)
		{
			state.bEquipPending = false;
			state.dwNextEquipmentCheckTime = dwNow + PLAYERBOT_EQUIPMENT_CHECK_INTERVAL;
			return false;
		}

		// Equipping is forbidden for 1.5 seconds after an attack or skill, or right after spawn.
		// Hold bEquipPending and do not disrupt active combat.
		if (IsPlayerBotEquipWindowShut(ch, state))
		{
			state.bEquipPending = true;
			return false;
		}

		state.bEquipPending = false;
		state.dwNextEquipmentCheckTime = dwNow + PLAYERBOT_EQUIPMENT_CHECK_INTERVAL;

		const DWORD newVnum = bestItem->GetVnum();
		const DWORD oldVnum = bestOldItem ? bestOldItem->GetVnum() : 0;
		const long long oldScore = bestOldItem ? GetPlayerBotEquipmentScore(bestOldItem, ch) : 0;
		// EquipItem's optional integer is a candidate slot for rings/uniques, not a
		// wear slot.  Passing WEAR_WEAPON/WEAR_BODY here made swaps of differently
		// sized items fail.  Put the old item into a genuinely free inventory area,
		// then let FindEquipCell choose the normal destination.
		// The owner's piece for a companion goes on by the engine's swap in
		// place first, which puts the old piece in the new one's cell and needs
		// no free one: taking the old piece off first did, and a companion that
		// loots everything and never goes to town alone has a full bag - the
		// piece its owner gave it waited for ever.
		// The decision's explanation, read before anything moves (playerbot_explain.h).
		TPlayerBotEquipExplain explained;
		if (IsPlayerBotExplainOn())
			PreparePlayerBotEquipExplain(explained, ch, bestWearCell, per::PATH_EQUIP_PASS, bestRule,
					bestItem, bestOldItem, bestOldZeroed);
		const bool swapped = pinned && bestOldItem && bestWearCell != WEAR_UNIQUE1 &&
				bestWearCell != WEAR_UNIQUE2 && PlayerBotEquipItem(ch, bestItem) && bestItem->IsEquipped();
		if (!swapped && bestOldItem)
		{
			if (ch->GetEmptyInventory(bestOldItem->GetSize()) < 0 ||
					!ch->UnequipItem(bestOldItem))
			{
				state.dwNextEquipmentCheckTime = dwNow + PLAYERBOT_GEAR_LOG_INTERVAL;
				return false;
			}
		}

		if (swapped || PlayerBotEquipItem(ch, bestItem))
		{
			sys_log(0, "PLAYERBOT_AI: equipped upgrade pid=%u name=%s wear=%d old_vnum=%u new_vnum=%u old_score=%lld new_score=%lld",
					ch->GetPlayerID(), ch->GetName(), bestWearCell, oldVnum, newVnum, oldScore, bestScore);
			if (bestWearCell == WEAR_WEAPON && bestOldItem)
				s_mapPlayerBotWeaponTakenOff[ch->GetPlayerID()] = std::make_pair(bestOldItem->GetID(), dwNow);
			// The one line a player asks about first - "why is my top Sura
			// suddenly without her +8" - is the swap, so it goes to log.log
			// with what came off.
			char szHint[64];
			snprintf(szHint, sizeof(szHint), "slot %d zamiast %u", bestWearCell, oldVnum);
			LogManager::instance().ItemLog(ch, bestItem, "PLAYERBOT_EQUIP", szHint);
			QueuePlayerBotEquip(explained);
			// And the row is written now, not in seven minutes. The panel reads
			// player.item, while the db core keeps a changed item in its cache
			// for PLAYER_CACHE_FLUSH_SECONDS - so a bot that had just put a
			// shield on showed an empty shield slot in the panel for minutes
			// ("chyba na www klasycznym jest bug synchronizacji eq",
			// 17 September; the row was there five minutes later). Both pieces:
			// the one worn and the one taken off.
			FlushPlayerBotItemRow(bestItem);
			FlushPlayerBotItemRow(bestOldItem);
			// What came off stays in the bag: a bot trades its spares, it does
			// not give them away.
			return true;
		}

		state.dwNextEquipmentCheckTime = dwNow + PLAYERBOT_GEAR_LOG_INTERVAL;
		// Never leave the slot empty over a refusal: what came off goes back on.
		const bool restored = bestOldItem && !bestOldItem->IsEquipped() && PlayerBotEquipItem(ch, bestOldItem);
		sys_err("PLAYERBOT_AI: failed to equip upgrade pid=%u name=%s wear=%d vnum=%u old=%u restored=%d polymorphed=%d busy=%d",
				ch->GetPlayerID(), ch->GetName(), bestWearCell, newVnum, oldVnum, restored ? 1 : 0,
				ch->IsPolymorphed() ? 1 : 0, ch->IsBusy() ? 1 : 0);
		return false;
	}

	void RestorePlayerBotEquipmentAfterRefining(LPCHARACTER ch,
			TPlayerBotAIState& state, DWORD dwNow)
	{
		// What the session took off goes back on here, so the pieces it
		// remembers are the equipment pass's again (ManagePlayerBotRefining).
		ClearPlayerBotRefineTakenOff(state);
		// What goes on now goes on after the blacksmith (playerbot_explain.h).
		NotePlayerBotExplainBlacksmith(ch ? ch->GetPlayerID() : 0, dwNow);
		// A blacksmith session can temporarily remove more than one worn item.
		// ManagePlayerBotEquipment intentionally equips only one upgrade per call,
		// so force a short bounded pass before the bot leaves the NPC.  This makes
		// the visible sequence match a real player: remove, refine through the
		// desired + level, then put the best surviving result back on.
		for (int attempt = 0; attempt < 8; ++attempt)
		{
			state.dwNextEquipmentCheckTime = 0;
			state.bEquipPending = true;
			if (!ManagePlayerBotEquipment(ch, state, dwNow))
				break;
		}
	}

	DWORD GetStarterChestVnum(BYTE bJob)
	{
		switch (bJob)
		{
			case JOB_WARRIOR:
			case JOB_SURA:
				return 50187;
			case JOB_ASSASSIN:
				return 50212;
			case JOB_SHAMAN:
				return 50213;
		}

		return 0;
	}

	// The apprentice chain: Skrzynia Ucznia I of the three kinds of class (50187
	// a warrior's and a sura's, 50212 an assassin's, 50213 a shaman's) and the
	// nine that come out of one another, each at its tenth level, up to
	// Skrzynia Arcymistrza (50188-50196, special_item_group.starter.txt). What
	// they hold is potions, elixirs, the green stones and the next chest -
	// never a piece of gear above the class's +0 starter weapon.
	bool IsPlayerBotApprenticeChestVnum(DWORD dwVnum)
	{
		return (dwVnum >= 50187 && dwVnum <= 50196) || dwVnum == 50212 || dwVnum == 50213;
	}

	// Whether the world gives the apprentice chest at all: the event flag
	// m2_starter_chest_off, which the migrator writes from M2_STARTER_CHEST and
	// both panels set live (web_admin.quest STARTER_CHEST). starter_chest.quest
	// asks it at a person's first login and the seed at a bot's creation; off,
	// no bot keeps or opens a chest of the chain either
	// (ManagePlayerBotProgressionChests). r40250 never writes it, and its bots
	// keep the chest the seed gives them. A map lookup.
	bool IsPlayerBotApprenticeChestOff()
	{
		return quest::CQuestManager::instance().GetEventFlag("m2_starter_chest_off") > 0;
	}

	DWORD GetPlayerBotEmergencyWeaponVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 10;

		switch (ch->GetJob())
		{
			case JOB_WARRIOR:
			case JOB_SURA:
				return 10;   // Sword +0
			case JOB_ASSASSIN:
				if (ch->GetSkillGroup() == 2)
					return 2000; // Bow +0
				return 1000; // Dagger +0
			case JOB_SHAMAN:
				return 7000; // Fan +0
		}

		return 10;
	}

	long long GetPlayerBotEmergencyWeaponPrice(LPCHARACTER ch)
	{
		const DWORD vnum = GetPlayerBotEmergencyWeaponVnum(ch);
		return vnum == 7000 ? 600 : 100;
	}

	int GetPlayerBotProtoLevelLimit(const TItemTable* proto)
	{
		if (!proto)
			return 0;
		for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
			if (proto->aLimits[i].bType == LIMIT_LEVEL)
				return proto->aLimits[i].lValue;
		return 0;
	}

	DWORD GetPlayerBotProgressionWeaponVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;

		DWORD familyBase = 10;
		switch (ch->GetJob())
		{
			case JOB_WARRIOR:
				familyBase = ch->GetSkillGroup() == 2 ? 3000 : 10;
				break;
			case JOB_ASSASSIN:
				familyBase = ch->GetSkillGroup() == 2 ? 2000 : 1000;
				break;
			case JOB_SURA:
				familyBase = 10;
				break;
			case JOB_SHAMAN:
				familyBase = 7000;
				break;
		}

		DWORD bestVnum = familyBase;
		int bestLevel = -1;
		// Twenty tiers, not eight. Eight stopped at familyBase+70, which is the
		// level-36 weapon in four of the five families - so every bot past 36
		// wanted nothing better than what it was already holding and stopped
		// upgrading for good. One was reported still swinging a +0 at level 49.
		//
		// Twenty is safe as well as sufficient: checked against item_proto, all
		// five families keep the same weapon subtype for twenty tiers, and only a
		// strictly higher requirement wins - so a family that runs out early stops
		// contributing, and the special level-30 weapons sitting at the far end of
		// three of these ranges can never displace a higher-level piece.
		for (int tier = 0; tier < 20; ++tier)
		{
			const DWORD candidateVnum = familyBase + tier * 10;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			// Strictly higher, so a level-0 starter belonging to the next class
			// can never displace a piece this character actually qualifies for.
			if (reqLevel <= (int)ch->GetLevel() && reqLevel > bestLevel)
			{
				bestVnum = candidateVnum;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	// The dagger ladder, for the Archer's stone weapon.
	DWORD GetPlayerBotProgressionStoneWeaponVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		const DWORD familyBase = 1000;
		DWORD bestVnum = familyBase;
		int bestLevel = -1;
		for (int tier = 0; tier < 20; ++tier)
		{
			const DWORD candidateVnum = familyBase + tier * 10;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			if (reqLevel <= (int)ch->GetLevel() && reqLevel > bestLevel)
			{
				bestVnum = candidateVnum;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	DWORD GetPlayerBotProgressionArmorVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;

		DWORD baseVnum = 11200;
		switch (ch->GetJob())
		{
			case JOB_ASSASSIN: baseVnum = 11400; break;
			case JOB_SURA:     baseVnum = 11600; break;
			case JOB_SHAMAN:   baseVnum = 11800; break;
			default: break;
		}
		DWORD bestVnum = baseVnum;
		int bestLevel = -1;
		// Ten tiers, not eight. Each class's body armour runs base+0 to base+90 -
		// levels 0, 9, 18, 26, 34, 42, 48, 54, 61, 66 - and eight of them stopped
		// at 54, so the last two pieces were unreachable however high a bot got.
		// Ten is also the ceiling: base+100 begins a different series with its own
		// numbering, and the classes are 200 apart, so this cannot reach one.
		for (int tier = 0; tier < 10; ++tier)
		{
			const DWORD candidateVnum = baseVnum + tier * 10;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			// Strictly higher, so a level-0 starter belonging to the next class
			// can never displace a piece this character actually qualifies for.
			if (reqLevel <= (int)ch->GetLevel() && reqLevel > bestLevel)
			{
				bestVnum = candidateVnum;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	DWORD GetPlayerBotProgressionShieldVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		DWORD bestVnum = 13000;
		int bestLevel = -1;
		for (int tier = 0; tier < 8; ++tier)
		{
			const DWORD candidateVnum = 13000 + tier * 20;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			// Strictly higher, so a level-0 starter belonging to the next class
			// can never displace a piece this character actually qualifies for.
			if (reqLevel <= (int)ch->GetLevel() && reqLevel > bestLevel)
			{
				bestVnum = candidateVnum;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	DWORD GetPlayerBotProgressionHelmetVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		DWORD baseVnum = 12200;
		switch (ch->GetJob())
		{
			case JOB_ASSASSIN: baseVnum = 12340; break;
			case JOB_SURA:     baseVnum = 12480; break;
			case JOB_SHAMAN:   baseVnum = 12620; break;
			default: break;
		}
		DWORD bestVnum = baseVnum;
		int bestLevel = -1;
		// Seven, and seven is the whole family: helmet ranges are 140 vnums apart
		// and the stride is 20, so the eighth tier was the next class's first
		// helmet. Nothing came of that - those entries are level-0 items and a
		// level-0 item cannot outrank a real one - but a ladder has no business
		// reading another class's gear to decide what this one should wear.
		// Nothing is lost either: every class reaches its best helmet by the
		// fourth tier (level 41 for a warrior, level 80 for the other three).
		for (int tier = 0; tier < 7; ++tier)
		{
			const DWORD candidateVnum = baseVnum + tier * 20;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			// Strictly higher, so a level-0 starter belonging to the next class
			// can never displace a piece this character actually qualifies for.
			if (reqLevel <= (int)ch->GetLevel() && reqLevel > bestLevel)
			{
				bestVnum = candidateVnum;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	DWORD GetPlayerBotProgressionBootsVnum(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		DWORD bestVnum = 15000;
		int bestLevel = -1;
		for (int tier = 0; tier < 12; ++tier)
		{
			const DWORD candidateVnum = 15000 + tier * 20;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			// Strictly higher, so a level-0 starter belonging to the next class
			// can never displace a piece this character actually qualifies for.
			if (reqLevel <= (int)ch->GetLevel() && reqLevel > bestLevel)
			{
				bestVnum = candidateVnum;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	bool BuyPlayerBotProgressionGear(LPCHARACTER ch, DWORD vnum, const char* category);
	bool HasPlayerBotProgressionGear(LPCHARACTER ch, DWORD desiredVnum, int wearCell)
	{
		if (!ch || desiredVnum == 0)
			return false;
		TItemTable* desiredProto = ITEM_MANAGER::instance().GetTable(desiredVnum);
		if (!desiredProto)
			return false;

		const int desiredLevel = GetPlayerBotProtoLevelLimit(desiredProto);
		for (int pass = 0; pass < 2; ++pass)
		{
			const int count = pass == 0 ? 1 : PLAYERBOT_BAG_CELLS;
			for (int index = 0; index < count; ++index)
			{
				LPITEM item = pass == 0 ? ch->GetWear(wearCell) : ch->GetInventoryItem(index);
				if (!item || !IsPlayerBotEquipmentCandidate(ch, item) ||
						item->FindEquipCell(ch) != wearCell)
					continue;
				if (wearCell == WEAR_WEAPON && !IsPlayerBotWeapon(ch, item))
					continue;
				if (item->GetLevelLimit() >= desiredLevel && item->GetLevelLimit() <= ch->GetLevel())
					return true;
			}
		}
		return false;
	}

	// What a proto is worth to this character, before anything has been rolled
	// on it. The ladders below compare candidates they cannot hold yet.
	long long ScorePlayerBotProtoApplies(const TItemTable* proto, LPCHARACTER ch)
	{
		if (!proto)
			return 0;
		long long score = 0;
		for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
			score += ScorePlayerBotApply(proto->aApplies[i].bType,
					proto->aApplies[i].lValue, ch);
		return score;
	}

	// Bracelets, necklaces and earrings. Not a ladder in the sense the other
	// slots are: the earring line rotates dexterity, strength, constitution and
	// intelligence as it climbs, so the newest tier a bot qualifies for is the
	// right one only for the class that tier favours. Worth decides, and the
	// required level only breaks a tie - which for the bracelets and necklaces,
	// whose lines climb straight, comes to the same answer as before.
	DWORD GetPlayerBotProgressionAccessoryVnum(LPCHARACTER ch, DWORD baseVnum,
			DWORD stride, int tiers)
	{
		if (!ch)
			return 0;
		DWORD bestVnum = 0;
		long long bestScore = -1;
		int bestLevel = -1;
		for (int tier = 0; tier < tiers; ++tier)
		{
			const DWORD candidateVnum = baseVnum + (DWORD)tier * stride;
			TItemTable* proto = ITEM_MANAGER::instance().GetTable(candidateVnum);
			if (!proto)
				continue;
			const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
			if (reqLevel > (int)ch->GetLevel())
				continue;
			const long long score = ScorePlayerBotProtoApplies(proto, ch);
			if (score > bestScore || (score == bestScore && reqLevel > bestLevel))
			{
				bestVnum = candidateVnum;
				bestScore = score;
				bestLevel = reqLevel;
			}
		}
		return bestVnum;
	}

	// Twelve tiers each, which is the whole of every one of the three families
	// below level 80: the next entries after them sit at 85 and above, past
	// anything this world levels to.
	DWORD GetPlayerBotProgressionWristVnum(LPCHARACTER ch)
	{
		return GetPlayerBotProgressionAccessoryVnum(ch, 14000, 20, 12);
	}

	DWORD GetPlayerBotProgressionNecklaceVnum(LPCHARACTER ch)
	{
		return GetPlayerBotProgressionAccessoryVnum(ch, 16000, 20, 12);
	}

	DWORD GetPlayerBotProgressionEarringVnum(LPCHARACTER ch)
	{
		return GetPlayerBotProgressionAccessoryVnum(ch, 17000, 20, 12);
	}

	// The other slots ask "is what I am wearing for a lower level than what I
	// could buy". That question is wrong here, because a better earring can be
	// an older one. This asks what the slot is actually worth instead.
	bool NeedsPlayerBotProgressionAccessory(LPCHARACTER ch, BYTE wearCell,
			DWORD desiredVnum)
	{
		if (!ch || desiredVnum == 0)
			return false;
		const TItemTable* desired = ITEM_MANAGER::instance().GetTable(desiredVnum);
		if (!desired)
			return false;
		const long long wanted = ScorePlayerBotProtoApplies(desired, ch);
		for (int pass = 0; pass < 2; ++pass)
		{
			const int count = pass == 0 ? 1 : PLAYERBOT_BAG_CELLS;
			for (int index = 0; index < count; ++index)
			{
				LPITEM item = pass == 0 ? ch->GetWear(wearCell)
						: ch->GetInventoryItem(index);
				if (!item || !IsPlayerBotEquipmentCandidate(ch, item) ||
						item->FindEquipCell(ch) != wearCell)
					continue;
				if (item->GetLevelLimit() > ch->GetLevel())
					continue;
				if (ScorePlayerBotProtoApplies(item->GetProto(), ch) >= wanted)
					return false;
			}
		}
		return true;
	}

	bool NeedsPlayerBotProgressionWrist(LPCHARACTER ch)
	{
		return NeedsPlayerBotProgressionAccessory(ch, WEAR_WRIST,
				GetPlayerBotProgressionWristVnum(ch));
	}

	bool NeedsPlayerBotProgressionNecklace(LPCHARACTER ch)
	{
		return NeedsPlayerBotProgressionAccessory(ch, WEAR_NECK,
				GetPlayerBotProgressionNecklaceVnum(ch));
	}

	bool NeedsPlayerBotProgressionEarring(LPCHARACTER ch)
	{
		return NeedsPlayerBotProgressionAccessory(ch, WEAR_EAR,
				GetPlayerBotProgressionEarringVnum(ch));
	}

	bool NeedsPlayerBotProgressionWeapon(LPCHARACTER ch)
	{
		return ch && !HasPlayerBotProgressionGear(
				ch, GetPlayerBotProgressionWeaponVnum(ch), WEAR_WEAPON);
	}

	bool NeedsPlayerBotProgressionArmor(LPCHARACTER ch)
	{
		return ch && !HasPlayerBotProgressionGear(
				ch, GetPlayerBotProgressionArmorVnum(ch), WEAR_BODY);
	}

	bool NeedsPlayerBotProgressionShield(LPCHARACTER ch)
	{
		return ch && !HasPlayerBotProgressionGear(
				ch, GetPlayerBotProgressionShieldVnum(ch), WEAR_SHIELD);
	}

	bool NeedsPlayerBotProgressionHelmet(LPCHARACTER ch)
	{
		return ch && !HasPlayerBotProgressionGear(
				ch, GetPlayerBotProgressionHelmetVnum(ch), WEAR_HEAD);
	}

	bool NeedsPlayerBotProgressionBoots(LPCHARACTER ch)
	{
		return ch && !HasPlayerBotProgressionGear(
				ch, GetPlayerBotProgressionBootsVnum(ch), WEAR_FOOTS);
	}

	// "Full eq" as a player says it: every slot filled and nothing on the
	// progression ladder left to buy.
	bool IsPlayerBotFullyEquipped(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		static const BYTE slots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_HEAD, WEAR_SHIELD,
			WEAR_WRIST, WEAR_FOOTS, WEAR_NECK, WEAR_EAR
		};
		for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i)
			if (!ch->GetWear(slots[i]))
				return false;
		return !NeedsPlayerBotProgressionWeapon(ch) &&
				!NeedsPlayerBotProgressionArmor(ch) &&
				!NeedsPlayerBotProgressionShield(ch) &&
				!NeedsPlayerBotProgressionHelmet(ch) &&
				!NeedsPlayerBotProgressionBoots(ch) &&
				!NeedsPlayerBotProgressionWrist(ch) &&
				!NeedsPlayerBotProgressionNecklace(ch) &&
				!NeedsPlayerBotProgressionEarring(ch);
	}

	bool IsPlayerBotSpecialLevel30Weapon(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_WEAPON)
			return false;
		return IsPlayerBotSpecialLevel30WeaponVnum(item->GetVnum());
	}

	// MT2009_PLUS_L30_WEAPON_DROPPER_V2: the island's dropper is there to put
	// level-30 weapons on the market, and it never wears one - it is held at
	// twenty-one and let go at twenty-four. Its class's own blade was kept for
	// an anvil it would never use (PlayerBotKeepsLevel30ForAnvil, the class's
	// own and PLAYERBOT_LEVEL30_KEEP_PERCENT of the rest) and another class's
	// for the grind for sale, so its counter took none of them: PrecelekxD
	// stood twenty minutes in Yongan with three Black Leaf Knives, and the
	// island stood empty. Every one it holds is counter goods as it dropped.
	bool IsPlayerBotL30WeaponSeller(LPCHARACTER ch)
	{
		return ch && GetPlayerBotPersonalityByPID(ch->GetPlayerID()) == BOT_PERSONALITY_L30_WEAPON_DROPPER;
	}

	// What a bot picks up and keeps for a player's crafting whatever the
	// merchant pays for it (PLAYERBOT_PICKUP_GOODS_VNUMS): the herbalist's
	// Gango Root and Tue Mushroom, the Crystal Earrings and the Ghost Face
	// Armour a smith takes further, every weapon of level sixty-five, the Zen
	// Bean and the Blood Pill. The loot filter never leaves these on a floor,
	// the junk rule never hands them to the merchant while a counter can sell
	// them, and the counter ranks them beside the materials.
	bool IsPlayerBotPickupGoods(LPITEM item)
	{
		if (!item || !item->GetProto())
			return false;
		const DWORD vnum = item->GetVnum();
		for (size_t i = 0; i < sizeof(PLAYERBOT_PICKUP_GOODS_VNUMS) / sizeof(PLAYERBOT_PICKUP_GOODS_VNUMS[0]); ++i)
			if (PLAYERBOT_PICKUP_GOODS_VNUMS[i] == vnum)
				return true;
		// Every herb of the herbalist's range, not the two the Biologist's rows
		// happen to want: they are the materials Baek-Go's board runs on.
		if (vnum >= PLAYERBOT_HERB_VNUM_FIRST && vnum <= PLAYERBOT_HERB_VNUM_LAST)
			return true;
		if ((vnum >= PLAYERBOT_PICKUP_EARRING_FIRST && vnum <= PLAYERBOT_PICKUP_EARRING_FIRST + 9) ||
				(vnum >= PLAYERBOT_PICKUP_ARMOUR_FIRST && vnum <= PLAYERBOT_PICKUP_ARMOUR_FIRST + 9))
			return true;
		return item->GetType() == ITEM_WEAPON && item->GetSubType() != WEAPON_ARROW &&
				(int)item->GetLevelLimit() == PLAYERBOT_PICKUP_WEAPON_LEVEL;
	}

	// The pickup goods that are gear: a cell each, where the herbs, the beans
	// and the books stack, so only these can fill a bag by their number
	// (PLAYERBOT_PICKUP_GEAR_BAG_KEEP).
	bool IsPlayerBotPickupGear(LPITEM item)
	{
		return IsPlayerBotPickupGoods(item) &&
				(item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR);
	}

	bool HasPlayerBotSpecialLevel30Weapon(LPCHARACTER ch, bool requireAverageDamage)
	{
		if (!ch)
			return false;
		for (int pass = 0; pass < 2; ++pass)
		{
			const int count = pass == 0 ? 1 : PLAYERBOT_BAG_CELLS;
			for (int index = 0; index < count; ++index)
			{
				LPITEM item = pass == 0 ? ch->GetWear(WEAR_WEAPON) : ch->GetInventoryItem(index);
				if (!IsPlayerBotSpecialLevel30Weapon(item) || !IsPlayerBotWeapon(ch, item))
					continue;
				if (!requireAverageDamage)
					return true;
				for (int attr = 0; attr < ITEM_ATTRIBUTE_MAX_NUM; ++attr)
					if (item->GetAttributeType(attr) == APPLY_NORMAL_HIT_DAMAGE_BONUS &&
							item->GetAttributeValue(attr) > 0)
						return true;
			}
		}
		return false;
	}

	// The proto of this piece's family at a higher plus, walking the refine
	// chain the blacksmith walks; the piece's own proto at or below its plus,
	// NULL past the top of the chain.
	const TItemTable* GetPlayerBotWeaponProtoAtPlus(LPITEM item, BYTE plus)
	{
		if (!item || !item->GetProto())
			return NULL;
		const TItemTable* proto = item->GetProto();
		BYTE at = item->GetRefineLevel();
		while (at < plus && proto && proto->dwRefinedVnum != 0)
		{
			proto = ITEM_MANAGER::instance().GetTable(proto->dwRefinedVnum);
			++at;
		}
		return at >= plus ? proto : NULL;
	}

	// A weapon whose lines are worth more than any blacksmith's odds: refined
	// under a scroll at every step or not at all (PLAYERBOT_WEAPON_SCROLL_ONLY_*).
	bool IsPlayerBotScrollOnlyWeapon(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_WEAPON)
			return false;
		long avg = 0, skill = 0;
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			const BYTE t = item->GetAttributeType(i);
			if (t == APPLY_NORMAL_HIT_DAMAGE_BONUS)
				avg += item->GetAttributeValue(i);
			else if (t == APPLY_SKILL_DAMAGE_BONUS)
				skill += item->GetAttributeValue(i);
		}
		return avg >= PLAYERBOT_WEAPON_SCROLL_ONLY_AVERAGE || skill >= PLAYERBOT_WEAPON_SCROLL_ONLY_SKILL;
	}

	// A piece no scroll is put on (PLAYERBOT_SCROLL_FREE_GEAR_MAX_LEVEL). The
	// blacksmith pass, the scroll pass in the field, the planner, the refine
	// target and the scroll purchase all ask it, through
	// FindPlayerBotRefineScrollCellFor where they look for the scroll itself.
	bool IsPlayerBotScrollFreeGear(LPITEM item)
	{
		return item && item->GetLevelLimit() <= PLAYERBOT_SCROLL_FREE_GEAR_MAX_LEVEL;
	}

	// The weapons the operator's anvil table reaches: the level-30 family and
	// every weapon from PLAYERBOT_ANVIL_TABLE_WEAPON_MIN_LEVEL.
	bool IsPlayerBotAnvilTableWeapon(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_WEAPON || item->GetSubType() == WEAPON_ARROW)
			return false;
		return IsPlayerBotSpecialLevel30Weapon(item) ||
				item->GetLevelLimit() >= PLAYERBOT_ANVIL_TABLE_WEAPON_MIN_LEVEL;
	}

	// The operator's anvil ceiling for a weapon of the table, by its average
	// line. Below it the bot grinds at the blacksmith and takes the risk; at or
	// above it the step belongs to a scroll. A weapon over the scroll-only line
	// never reaches this at all - IsPlayerBotScrollOnlyWeapon answers first.
	int GetPlayerBotLevel30AnvilCeiling(long average)
	{
		if (average <= PLAYERBOT_LEVEL30_ANVIL_AVG_CHEAP)
			return PLAYERBOT_LEVEL30_ANVIL_PLUS_CHEAP;
		if (average <= PLAYERBOT_LEVEL30_ANVIL_AVG_GOOD)
			return PLAYERBOT_LEVEL30_ANVIL_PLUS_GOOD;
		if (average <= PLAYERBOT_LEVEL30_ANVIL_AVG_BETTER)
			return PLAYERBOT_LEVEL30_ANVIL_PLUS_BETTER;
		// From 37% the operator's scroll-only rule answers first. Community
		// patch 2's "from 34% under a Blessing Scroll from +3" is gone: Iwakura
		// took it back on 23 September (PLAYERBOT_LEVEL30_MIN_PLUS).
		if (average <= PLAYERBOT_LEVEL30_ANVIL_AVG_HIGH)
			return PLAYERBOT_LEVEL30_ANVIL_PLUS_HIGH;
		return 0;
	}

	// Whether this bot grinds THIS level-30 weapon of another class for sale
	// (PLAYERBOT_LEVEL30_SALE_REFINE_PERCENT). The id is the item's own, or an
	// offline counter line's, which is the same item. A weapon over the
	// scroll-only line never meets the plain anvil, so it is sold as it is.
	bool IsPlayerBotLevel30SaleDraw(LPCHARACTER ch, DWORD itemId)
	{
		const DWORD salt = ch->GetPlayerID() ^ (itemId * 2246822519U) ^ 0x53414c45U;
		return (int)(PlayerBotNavHash(salt) % 100U) < PLAYERBOT_LEVEL30_SALE_REFINE_PERCENT;
	}

	bool PlayerBotRefinesLevel30ForSale(LPCHARACTER ch, LPITEM item, DWORD itemId)
	{
		if (!ch || !item || !IsPlayerBotSpecialLevel30Weapon(item) || item->CanUsedBy(ch) ||
				IsPlayerBotL30WeaponSeller(ch) || // MT2009_PLUS_L30_WEAPON_DROPPER_V2
				IsPlayerBotScrollOnlyWeapon(item) || !IsPlayerBotLevel30SaleDraw(ch, itemId))
			return false;
		// The bow or fan a keeper builds for its sash is not goods.
		if (IsPlayerBotSashGrailProject(ch, item))
			return false;
		// Two of a family in the bag at most, and the rest are goods at once
		// (PLAYERBOT_HELD_FAMILY_LIMIT): what a bot grinds for sale is still
		// what it holds. A copy counts when it lies ahead of this one - or
		// always, for a line on a counter, which has no cell in the bag.
		const DWORD family = item->GetVnum() - (DWORD)std::max(0, item->GetRefineLevel());
		const bool inBag = item->GetWindow() == INVENTORY && ch->GetInventoryItem(item->GetCell()) == item;
		int ahead = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM other = ch->GetInventoryItem(cell);
			if (!other || other == item || other->GetID() == itemId || other->GetCell() != cell ||
					!IsPlayerBotSpecialLevel30Weapon(other) || other->CanUsedBy(ch) ||
					IsPlayerBotScrollOnlyWeapon(other) ||
					other->GetVnum() - (DWORD)std::max(0, other->GetRefineLevel()) != family ||
					!IsPlayerBotLevel30SaleDraw(ch, other->GetID()))
				continue;
			if (!inBag || cell < item->GetCell())
				++ahead;
		}
		return ahead < PLAYERBOT_HELD_FAMILY_LIMIT;
	}

	bool PlayerBotRefinesLevel30ForSale(LPCHARACTER ch, LPITEM item)
	{
		return item && PlayerBotRefinesLevel30ForSale(ch, item, item->GetID());
	}

	// How far: the operator's anvil ceiling for its average line.
	BYTE GetPlayerBotLevel30SaleTarget(LPITEM item)
	{
		return (BYTE)GetPlayerBotLevel30AnvilCeiling(SumPlayerBotItemLines(item, APPLY_NORMAL_HIT_DAMAGE_BONUS));
	}

	// What a level-30 weapon will hit for once ground to
	// PLAYERBOT_LEVEL30_PROJECT_PLUS, or as it is above that.
	long long GetPlayerBotLevel30Potential(LPCHARACTER ch, LPITEM item)
	{
		if (!item)
			return 0;
		const BYTE plus = std::max<BYTE>(item->GetRefineLevel(), PLAYERBOT_LEVEL30_PROJECT_PLUS);
		const TItemTable* proto = GetPlayerBotWeaponProtoAtPlus(item, plus);
		return proto ? GetPlayerBotWeaponHitDamageAt(item, proto, ch) : 0;
	}

	// Iwakura's community patch 2, point 1: the level-30 weapon of this bot's
	// class - the Full Moon Sword, the Ostrze z Czerwonej Stali, the bow, the
	// dagger, the bell or the fan, whichever its build wields - is every bot's
	// from level thirty, whatever its level now: a bot of forty-two on an old
	// world without one is sent for it as surely as a bot of thirty.
	bool IsPlayerBotClassLevel30Weapon(LPCHARACTER ch, LPITEM item)
	{
		return ch && item && IsPlayerBotSpecialLevel30Weapon(item) && IsPlayerBotWeapon(ch, item) &&
				item->CanUsedBy(ch);
	}

	// Defined below, beside the scroll count it asks.
	bool IsPlayerBotScrollRuleWeapon(LPCHARACTER ch, LPITEM item);

	// Where the plain anvil stops for a weapon of the table
	// (IsPlayerBotAnvilTableWeapon): the operator's ceiling for its average
	// line, and the class's own level-30 weapon never under
	// PLAYERBOT_LEVEL30_MIN_PLUS. The blacksmith pass, the planner, the scroll
	// pass, the scroll purchase and the Demon Tower's smith all ask this, so
	// none of them sends a bot for a step another one holds.
	int GetPlayerBotWeaponAnvilCeiling(LPCHARACTER ch, LPITEM item)
	{
		// Iwakura's scroll rule: while it holds, no step of the weapon in the
		// hand is the plain anvil's, so every pass that asks here - the
		// blacksmith, the field scroll pass, the planner, the tower's smith -
		// puts it under a scroll.
		if (IsPlayerBotScrollRuleWeapon(ch, item))
			return 0;
		int ceiling = GetPlayerBotLevel30AnvilCeiling(SumPlayerBotItemLines(item, APPLY_NORMAL_HIT_DAMAGE_BONUS));
		if (IsPlayerBotClassLevel30Weapon(ch, item))
			ceiling = std::max<int>(ceiling, PLAYERBOT_LEVEL30_MIN_PLUS);
		return ceiling;
	}

	// A level-30 weapon whose roll is cheap enough that the blacksmith pass
	// still gambles it at the plain anvil over its ceiling
	// (PLAYERBOT_LEVEL30_CHEAP_ANVIL_PERCENT): the family is everywhere and the
	// scroll is not. The planner counts such a step as an errand, since the
	// pass may take it.
	bool IsPlayerBotCheapLevel30Roll(LPITEM item)
	{
		return IsPlayerBotSpecialLevel30Weapon(item) &&
				SumPlayerBotItemLines(item, APPLY_NORMAL_HIT_DAMAGE_BONUS) <= PLAYERBOT_LEVEL30_ANVIL_AVG_CHEAP;
	}

	// The class's own level-30 weapon, wearable now and under
	// PLAYERBOT_LEVEL30_MIN_PLUS: refined whatever its average, under a scroll
	// when there is one for the step and at the plain anvil when there is not
	// - no scroll-only hold and no ceiling below the floor.
	bool IsPlayerBotLevel30UnderFloor(LPCHARACTER ch, LPITEM item)
	{
		return IsPlayerBotClassLevel30Weapon(ch, item) && item->GetLevelLimit() <= ch->GetLevel() &&
				item->GetRefineLevel() < PLAYERBOT_LEVEL30_MIN_PLUS;
	}

	// What a level-30 weapon hits for at a plus, or at its own above it.
	long long GetPlayerBotLevel30PotentialAt(LPCHARACTER ch, LPITEM item, BYTE plus)
	{
		if (!item)
			return 0;
		const TItemTable* proto = GetPlayerBotWeaponProtoAtPlus(item, std::max<BYTE>(item->GetRefineLevel(), plus));
		return proto ? GetPlayerBotWeaponHitDamageAt(item, proto, ch) : 0;
	}

	// The one it works on: the best of them by what it will hit for, the
	// weapon in the hand winning a tie. All of them are judged at one plus -
	// the hand's, PLAYERBOT_LEVEL30_PROJECT_PLUS if that is higher - so a copy
	// with the better lines is the one worked on under a hand at +8 or +9 too.
	// Each judged at its own plus, the hand at +8 always beat a fresh +0 whose
	// lines were better, and the fresh one went on a counter ("jak maja fmsa +4
	// 30 srednie a dropna fmsa 35 srednie to ... powinny go ulepszac zeby go
	// zalozyc", sosen, 27 September).
	LPITEM FindPlayerBotClassLevel30Weapon(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return NULL;
		// MT2009_PLUS_L30_WEAPON_DROPPER_V2: none is the island dropper's own.
		if (IsPlayerBotL30WeaponSeller(ch))
			return NULL;
		LPITEM best = NULL;
		long long bestPotential = -1;
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		const bool wornOwn = IsPlayerBotClassLevel30Weapon(ch, worn);
		const BYTE plus = std::max<BYTE>(wornOwn ? worn->GetRefineLevel() : 0, PLAYERBOT_LEVEL30_PROJECT_PLUS);
		if (wornOwn)
		{
			best = worn;
			bestPotential = GetPlayerBotLevel30PotentialAt(ch, worn, plus);
		}
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || !IsPlayerBotClassLevel30Weapon(ch, item))
				continue;
			const long long potential = GetPlayerBotLevel30PotentialAt(ch, item, plus);
			if (potential > bestPotential)
			{
				best = item;
				bestPotential = potential;
			}
		}
		return best;
	}

	// Who has to go and get one: thirty or more, not a dropper (its time is
	// its farm's, IsPlayerBotDropper), and none in the hand or the bag.
	bool PlayerBotLacksClassLevel30Weapon(LPCHARACTER ch)
	{
		return ch && ch->GetLevel() >= 30 && ch->IsItemLoaded() &&
				!IsPlayerBotDropper(GetPlayerBotPersonalityByPID(ch->GetPlayerID())) &&
				FindPlayerBotClassLevel30Weapon(ch) == NULL;
	}

	// Community patch 2, point 1, as one budget: the class's level-30 weapon's
	// purchase and its anvil share PLAYERBOT_LEVEL30_BUDGET_PERCENT of the purse
	// the first of them found (over the reserve and the shopping floor), for
	// PLAYERBOT_LEVEL30_BUDGET_WINDOW_MS. Measured apart - the purchase against
	// the purse it met, the anvil against the purse its visit began with - the
	// two came to some eighty percent, and the anvil's own sixty never
	// counted: the Perfectionist's half of the purse stopped the pass first
	// (B25 of Iwakura's audit of 26 September).
	long long GetPlayerBotLevel30BudgetLeft(LPCHARACTER ch, DWORD dwNow)
	{
		if (!ch)
			return 0;
		const long long spare = std::max<long long>(0, (long long)ch->GetGold() - GetPlayerBotReservedGold(ch) -
				(long long)PLAYERBOT_SHOPPING_GOLD_FLOOR);
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (st == s_mapPlayerBotAIStates.end())
			return spare * PLAYERBOT_LEVEL30_BUDGET_PERCENT / 100;
		TPlayerBotPersona& p = st->second.persona;
		if (p.dwLevel30BudgetSince == 0 || dwNow - p.dwLevel30BudgetSince >= PLAYERBOT_LEVEL30_BUDGET_WINDOW_MS)
		{
			p.dwLevel30BudgetSince = dwNow != 0 ? dwNow : 1;
			p.llLevel30BudgetBase = spare;
			p.llLevel30BudgetSpent = 0;
		}
		return std::max<long long>(0,
				p.llLevel30BudgetBase * PLAYERBOT_LEVEL30_BUDGET_PERCENT / 100 - p.llLevel30BudgetSpent);
	}

	// What the purchase or the anvil spent of it.
	void NotePlayerBotLevel30Spend(LPCHARACTER ch, long long amount)
	{
		if (!ch || amount <= 0)
			return;
		GetPlayerBotLevel30BudgetLeft(ch, get_dword_time());
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		if (st != s_mapPlayerBotAIStates.end())
			st->second.persona.llLevel30BudgetSpent += amount;
	}

	// What the purchase may cost: what is left of that budget.
	// MT2009_PLUS_PROGRESSION_V2: past the hunt's last level the bot "does
	// everything to buy it from the market" (the owner): all it can spare.
	long long GetPlayerBotLevel30PurchaseCap(LPCHARACTER ch)
	{
		const long long budget = GetPlayerBotLevel30BudgetLeft(ch, get_dword_time());
		if (!ch || ch->GetLevel() <= PLAYERBOT_LEVEL30_WEAPON_HUNT_MAX_LEVEL)
			return budget;
		const long long spare = std::max<long long>(0, (long long)ch->GetGold() - GetPlayerBotReservedGold(ch) -
				(long long)PLAYERBOT_SHOPPING_GOLD_FLOOR);
		return std::max(budget, spare);
	}

	// The first plus, drawn by pid so a bot keeps the same one for life.
	BYTE GetPlayerBotLevel30FirstPlus(DWORD pid)
	{
		const int roll = (int)(PlayerBotNavHash(pid ^ 0x4c333046U) % 100U);
		int edge = PLAYERBOT_LEVEL30_FIRST_PLUS6_PERCENT;
		if (roll < edge)
			return 6;
		edge += PLAYERBOT_LEVEL30_FIRST_PLUS7_PERCENT;
		if (roll < edge)
			return 7;
		edge += PLAYERBOT_LEVEL30_FIRST_PLUS8_PERCENT;
		if (roll < edge)
			return 8;
		edge += PLAYERBOT_LEVEL30_FIRST_PLUS9_PERCENT;
		if (roll < edge)
			return 9;
		return 6;
	}

	// And the one it keeps trying for at every later visit.
	BYTE GetPlayerBotLevel30LongPlus(DWORD pid)
	{
		return std::max<BYTE>(GetPlayerBotLevel30FirstPlus(pid), PLAYERBOT_LEVEL30_LONG_TERM_PLUS);
	}

	// This visit's aim: the first plus while the weapon walked into town
	// under it - one visit's push, as far as the purse allows - and the
	// long-term plus from the visit after that.
	BYTE GetPlayerBotLevel30Aim(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return 0;
		const DWORD pid = ch->GetPlayerID();
		const BYTE first = GetPlayerBotLevel30FirstPlus(pid);
		BYTE startPlus = item->GetRefineLevel();
		TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(pid);
		if (st != s_mapPlayerBotAIStates.end() && st->second.persona.bLevel30VisitStartPlus != 0xFF)
			startPlus = st->second.persona.bLevel30VisitStartPlus;
		return startPlus < first ? first : GetPlayerBotLevel30LongPlus(pid);
	}

	// The best average line of the class's level-30 weapons the bot holds.
	long GetPlayerBotBestClassLevel30Average(LPCHARACTER ch)
	{
		long best = -1;
		if (!ch || !ch->IsItemLoaded())
			return best;
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		if (IsPlayerBotClassLevel30Weapon(ch, worn))
			best = SumPlayerBotItemLines(worn, APPLY_NORMAL_HIT_DAMAGE_BONUS);
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetCell() == cell && IsPlayerBotClassLevel30Weapon(ch, item))
				best = std::max(best, SumPlayerBotItemLines(item, APPLY_NORMAL_HIT_DAMAGE_BONUS));
		}
		return best;
	}

	// A counter's level-30 weapon of the bot's own class it has to buy: any,
	// when it has none - after a burn too ("w przypadku zniszczenia ... bot
	// ma obowiazek zakupic kolejna sztuke ... jesli pozwala na to jego
	// budzet"; the budget is CanPlayerBotPayForOffer's). Community patch 2
	// also made it buy every one of 34% or more that beat its own; Iwakura
	// took that back on 23 September, and a better one is bought only when
	// it would hit harder (IsPlayerBotBetterLevel30Offer).
	bool IsPlayerBotMandatedLevel30Offer(LPCHARACTER ch, LPITEM offer)
	{
		// MT2009_PLUS_DROPPER_INVEST_V1: a dropper too, in its shopping window.
		if (!IsPlayerBotClassLevel30Weapon(ch, offer) || offer->GetLevelLimit() > ch->GetLevel() ||
				ch->GetLevel() < 30 || (IsPlayerBotDropper(GetPlayerBotPersonalityByPID(ch->GetPlayerID())) &&
					!IsPlayerBotDropperShopping(ch->GetPlayerID(), get_dword_time())))
			return false;
		return GetPlayerBotBestClassLevel30Average(ch) < 0;
	}

	// What a level-30 weapon has to beat, and the one worth grinding for it.
	// toBeat is the best blow the bot has today - the weapon in its hand and
	// every weapon of its class in the bag it could put on - with a level-30
	// weapon it already wears counted at its potential, so a bot on a Full
	// Moon Sword +3 does not start a second one. The project is the bag's
	// level-30 weapon with the best potential, when that beats toBeat by
	// PLAYERBOT_LEVEL30_PROJECT_MARGIN_PERCENT. Nothing is stored: the bag is
	// read each time, so a sale, a burn or a better find changes the answer.
	struct TPlayerBotLevel30View
	{
		long long toBeat;
		LPITEM project;
		long long projectPotential;
		TPlayerBotLevel30View() : toBeat(0), project(NULL), projectPotential(0)
		{
		}
	};

	void ReadPlayerBotLevel30View(LPCHARACTER ch, TPlayerBotLevel30View& view)
	{
		view = TPlayerBotLevel30View();
		if (!ch || !ch->IsItemLoaded())
			return;
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		// A weapon a bot of thirty will not fight with (IsPlayerBotBannedLowWeapon)
		// is nothing a level-30 weapon has to beat: the Miecz +6 in the hand
		// out-hit every copy at +0 and kept the project from ever starting.
		if (worn && worn->GetType() == ITEM_WEAPON && !IsPlayerBotBannedLowWeapon(ch, worn))
		{
			view.toBeat = GetPlayerBotWeaponHitDamage(worn, ch);
			if (IsPlayerBotSpecialLevel30Weapon(worn) && IsPlayerBotWeapon(ch, worn))
				view.toBeat = std::max(view.toBeat, GetPlayerBotLevel30Potential(ch, worn));
		}
		LPITEM best = NULL;
		long long bestPotential = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->IsEquipped() ||
					!IsPlayerBotWeapon(ch, item) || item->GetLevelLimit() > ch->GetLevel() ||
					IsPlayerBotBannedLowWeapon(ch, item))
				continue;
			if (!IsPlayerBotSpecialLevel30Weapon(item))
			{
				view.toBeat = std::max(view.toBeat, GetPlayerBotWeaponHitDamage(item, ch));
				continue;
			}
			const long long potential = GetPlayerBotLevel30Potential(ch, item);
			if (potential > bestPotential)
			{
				best = item;
				bestPotential = potential;
			}
		}
		if (best && bestPotential * 100 > view.toBeat * (100 + PLAYERBOT_LEVEL30_PROJECT_MARGIN_PERCENT))
		{
			view.project = best;
			view.projectPotential = bestPotential;
		}
	}

	bool IsPlayerBotLevel30Project(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !IsPlayerBotSpecialLevel30Weapon(item) || item->IsEquipped())
			return false;
		TPlayerBotLevel30View view;
		ReadPlayerBotLevel30View(ch, view);
		return view.project == item;
	}

	// Whether this bot works THIS level-30 weapon at the anvil rather than
	// listing it. "Niech botom zalezy na takich broniach ... 65% do kowala,
	// reszta na rynek" - so the draw is per weapon, not per bot, and it is a
	// hash of the pair rather than a roll: a keeper that changed its mind would
	// put the same weapon up and take it back every service visit. The bag
	// still has a ceiling, because the bots hold 276 of these at +0 between
	// them and grinding all of them would be a purse emptied for nothing.
	bool PlayerBotKeepsLevel30ForAnvil(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !IsPlayerBotSpecialLevel30Weapon(item))
			return false;
		// MT2009_PLUS_L30_WEAPON_DROPPER_V2: the island's dropper keeps none.
		if (IsPlayerBotL30WeaponSeller(ch))
			return false;
		TPlayerBotLevel30View view;
		ReadPlayerBotLevel30View(ch, view);
		if (!item->IsEquipped() && item == view.project)
			return true;   // the project is kept whatever the draw says
		// So is the class's own (community patch 2, point 1).
		const LPITEM classOwn = FindPlayerBotClassLevel30Weapon(ch);
		if (item == classOwn)
			return true;
		// Another class's is never kept for this bot's anvil: it cannot wear
		// it, so it was kept to be kept - the grind for sale is its own rule
		// (PlayerBotRefinesLevel30ForSale), and the rest is counter goods at
		// once (community patch 2, point 9).
		if (!IsPlayerBotClassLevel30Weapon(ch, item))
			return false;
		const DWORD salt = ch->GetPlayerID() ^ (item->GetID() * 2654435761U);
		if ((int)(PlayerBotNavHash(salt) % 100U) >= PLAYERBOT_LEVEL30_KEEP_PERCENT)
			return false;
		// Count what the bag already works on, so a bot keeps a few and lists
		// the rest instead of hoarding every one it picks up: four at most, the
		// project and the class's own among them - Iwakura's four are pieces,
		// not reasons ("w ekwipunku i magazynie moga znajdowac sie
		// maksymalnie ... wyjatek: bron na 30. poziom dla klasy bota, ktorej
		// limit wynosi 4 sztuki", 23 September). A drawn copy counts only when
		// it lies ahead of this one in the bag; counting every other copy, as
		// this did, let none of five be kept and all five back the next time.
		const bool inBag = item->GetWindow() == INVENTORY && !item->IsEquipped();
		int kept = 0;
		for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM other = ch->GetInventoryItem(cell);
			if (!other || other == item || other->IsEquipped() || !IsPlayerBotClassLevel30Weapon(ch, other))
				continue;
			if (other == view.project || other == classOwn)
			{
				++kept;
				continue;
			}
			const DWORD otherSalt = ch->GetPlayerID() ^ (other->GetID() * 2654435761U);
			if ((int)(PlayerBotNavHash(otherSalt) % 100U) < PLAYERBOT_LEVEL30_KEEP_PERCENT &&
					(!inBag || other->GetCell() < item->GetCell()))
				++kept;
		}
		return kept < PLAYERBOT_LEVEL30_KEEP_MAX;
	}


	// A level-30 weapon on somebody's counter is worth buying when its
	// potential beats both the best blow the bot has and its own project.
	bool IsPlayerBotBetterLevel30Offer(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !IsPlayerBotSpecialLevel30Weapon(offer) || !IsPlayerBotWeapon(ch, offer) ||
				offer->GetLevelLimit() > ch->GetLevel())
			return false;
		TPlayerBotLevel30View view;
		ReadPlayerBotLevel30View(ch, view);
		const long long bar = std::max(view.toBeat, view.projectPotential);
		return GetPlayerBotLevel30Potential(ch, offer) * 100 >
				bar * (100 + PLAYERBOT_LEVEL30_PROJECT_MARGIN_PERCENT);
	}

	// Before a walk to a market for one: level thirty, no project in the bag,
	// no finished one in the hand, and the level-30 weapon of the kind it
	// wields - at PLAYERBOT_LEVEL30_PROJECT_PLUS with a
	// PLAYERBOT_LEVEL30_HOPED_AVERAGE line - would beat what it has. A bot of
	// seventy on a level-65 weapon +9 has nothing to look for there.
	bool PlayerBotCouldUseLevel30Weapon(LPCHARACTER ch)
	{
		if (!ch || ch->GetLevel() < 30 || !ch->IsItemLoaded())
			return false;
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		if (worn && !IsPlayerBotWeapon(ch, worn))
			return false;
		// A finished refine is not a finished weapon. This used to answer "no
		// thank you" to the whole market for any bot already wearing a special
		// level-30 weapon at +7, whatever was rolled on it - so a Full Moon
		// Sword +7 with nothing on it stopped its owner from ever looking at a
		// better one (audit of 17 September). The comparison below is the real
		// test and it is the stricter one where it should be: toBeat counts a
		// worn level-30 weapon AT ITS POTENTIAL, so a good +7 still refuses
		// every offer, and only a poor one lets the search go on.
		TPlayerBotLevel30View view;
		ReadPlayerBotLevel30View(ch, view);
		if (view.project)
			return false;
		if (!worn)
			return true;
		static const DWORD families[] = { 290, 1170, 2150, 3210, 5110, 7160 };
		for (size_t i = 0; i < sizeof(families) / sizeof(families[0]); ++i)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(families[i] + PLAYERBOT_LEVEL30_PROJECT_PLUS);
			if (!proto || proto->bType != ITEM_WEAPON || proto->bSubType != worn->GetSubType())
				continue;
			return GetPlayerBotWeaponHitDamageAt(NULL, proto, ch, PLAYERBOT_LEVEL30_HOPED_AVERAGE) * 100 >
					view.toBeat * (100 + PLAYERBOT_LEVEL30_PROJECT_MARGIN_PERCENT);
		}
		return false;
	}

	// Defined in playerbot_persona.h, further down: M3's door by Iwakura's
	// document - a weapon at +6 and an armour at +5, the Mental Warrior with the
	// weapon alone.
	bool MeetsPlayerBotM3Survival(LPCHARACTER ch);

	bool HasPlayerBotM3ReadyEquipment(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		// The document's second tier starts at fifteen, as the old rule did.
		if (IsPlayerBotPersonaEnabled())
			return ch->GetLevel() >= 15 && MeetsPlayerBotM3Survival(ch);
		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		LPITEM armor = ch->GetWear(WEAR_BODY);
		LPITEM shield = ch->GetWear(WEAR_SHIELD);
		LPITEM helmet = ch->GetWear(WEAR_HEAD);
		LPITEM boots = ch->GetWear(WEAR_FOOTS);
		if (!weapon || !armor || !shield || !helmet || !boots)
			return false;
		if (ch->GetLevel() >= 20)
			return true;
		return ch->GetLevel() >= 15 && weapon->GetRefineLevel() >= 4 &&
				armor->GetRefineLevel() >= 4 && shield->GetRefineLevel() >= 4;
	}

	bool IsPlayerBotCoreProgressionItem(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !IsPlayerBotEquipmentCandidate(ch, item))
			return false;

		const int wearCell = item->FindEquipCell(ch);
		DWORD desiredVnum = 0;
		if (wearCell == WEAR_WEAPON)
		{
			if (!IsPlayerBotWeapon(ch, item))
				return false;
			desiredVnum = GetPlayerBotProgressionWeaponVnum(ch);
		}
		else if (wearCell == WEAR_BODY)
		{
			desiredVnum = GetPlayerBotProgressionArmorVnum(ch);
		}
		else if (wearCell == WEAR_SHIELD)
		{
			desiredVnum = GetPlayerBotProgressionShieldVnum(ch);
		}
		else if (wearCell == WEAR_HEAD)
		{
			desiredVnum = GetPlayerBotProgressionHelmetVnum(ch);
		}
		else if (wearCell == WEAR_FOOTS)
		{
			desiredVnum = GetPlayerBotProgressionBootsVnum(ch);
		}
		else
		{
			return false;
		}

		const TItemTable* desiredProto = ITEM_MANAGER::instance().GetTable(desiredVnum);
		const int desiredLevel = GetPlayerBotProtoLevelLimit(desiredProto);
		return item->GetLevelLimit() >= desiredLevel &&
				item->GetLevelLimit() <= ch->GetLevel();
	}

	// Could this character put the item this vnum names on? Asked of a refine
	// *result* before the anvil is used, because the engine will not ask.
	// DoRefine checks the result's level limit only under !g_iUseLocale - "in
	// korea only", says the comment - and this server runs with a locale, so a
	// level-40 bot could turn its Upiorna Kusza +1 (level 40) into a +2 that
	// needs 42, and then stand there unable to equip the weapon it just paid
	// for. Twenty-four families on this proto raise their level with the plus;
	// five of them - that crossbow, the Three Lords shield and the three
	// level-52 bells - sit inside the levels bots reach.
	bool IsPlayerBotWearableAtLevel(LPCHARACTER ch, DWORD vnum)
	{
		if (!ch || vnum == 0)
			return false;
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		if (!proto)
			return false;
		for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
			if (proto->aLimits[i].bType == LIMIT_LEVEL &&
					proto->aLimits[i].lValue > ch->GetLevel())
				return false;
		return true;
	}

	// How many Blessing and Dragon God scrolls the bag holds.
	int CountPlayerBotSafeRefineScrolls(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		int scrolls = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM held = ch->GetInventoryItem(cell);
			if (held && held->GetCell() == cell && IsPlayerBotSafeRefineScroll(held->GetVnum()))
				scrolls += std::max<int>(1, held->GetCount());
		}
		return scrolls;
	}

	// Iwakura's scroll rule (27 September): the weapon a bot fights with - worn,
	// or the one a blacksmith session keeps in the bag (GetPlayerBotHandWeapon)
	// - of level thirty or more and under +7 goes under a scroll at every step
	// while the bag holds PLAYERBOT_SCROLL_RULE_MIN_SCROLLS safe scrolls, in the
	// field, past the operator's anvil table. Without it a +5 with thirty-seven
	// scrolls and the materials in the bag waited for the blacksmith and the
	// plain anvil. Under SCROLL_FROM the floor still answers. "Zwoje
	// Blogoslawienstwa" are counted as the scrolls that hand a failed piece
	// back rather than burn it (IsPlayerBotSafeRefineScroll): the Blessing
	// Scroll and the kinds better than it; never the War God's, which stops at
	// +3, nor a Gwarancja. The policy is playerbot_refine_rules.h's.
	bool IsPlayerBotScrollRuleWeapon(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetType() != ITEM_WEAPON || item->GetSubType() == WEAPON_ARROW)
			return false;
		const int level = (int)item->GetLevelLimit();
		const int plus = (int)item->GetRefineLevel();
		const bool refinable = item->GetRefinedVnum() != 0;
		const bool stepAllowed = IsPlayerBotScrollStepAllowed(item->GetRefineLevel());
		// The piece's own half first, as if the bag held the scrolls: the hand
		// and the count walk the bag, and the refine target asks this of every
		// piece there is.
		if (!playerbot_refine_rules::IsScrollRuleWeapon(level, plus, refinable, stepAllowed,
					PLAYERBOT_SCROLL_RULE_MIN_SCROLLS) ||
				item != GetPlayerBotHandWeapon(ch))
			return false;
		return playerbot_refine_rules::IsScrollRuleWeapon(level, plus, refinable, stepAllowed,
				CountPlayerBotSafeRefineScrolls(ch));
	}

	// Defined below, beside the backup armour it is the other half of.
	LPITEM GetPlayerBotBodyArmour(LPCHARACTER ch);

	// Community Patch 5, point 4, its second half: "W przypadku, gdy bot
	// posiada juz bron ulepszona na przynajmniej +8, dokladnie ta sama zasada
	// ... powinna zaczac dotyczyc jego zbroi." The body armour on the back - or
	// the one a blacksmith session keeps in the bag (GetPlayerBotBodyArmour) -
	// of level thirty or more (the level-34 tier up: the ladder has none at
	// thirty) and under +7, once the weapon in the hand stands at +8, goes
	// under the scrolls at every step, and what the step lacks is bought at the
	// market first (CollectPlayerBotScrollRuleMissing), as for the weapon.
	bool IsPlayerBotScrollRuleArmour(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetType() != ITEM_ARMOR || item->GetSubType() != ARMOR_BODY)
			return false;
		const int level = (int)item->GetLevelLimit();
		const int plus = (int)item->GetRefineLevel();
		const bool refinable = item->GetRefinedVnum() != 0;
		const bool stepAllowed = IsPlayerBotScrollStepAllowed(item->GetRefineLevel());
		// The piece's own half first, as for the weapon.
		if (!playerbot_refine_rules::IsScrollRuleArmour(level, plus, refinable, stepAllowed,
					PLAYERBOT_SCROLL_RULE_MIN_SCROLLS, PLAYERBOT_SCROLL_RULE_ARMOUR_WEAPON_PLUS) ||
				item != GetPlayerBotBodyArmour(ch))
			return false;
		LPITEM hand = GetPlayerBotHandWeapon(ch);
		return playerbot_refine_rules::IsScrollRuleArmour(level, plus, refinable, stepAllowed,
				CountPlayerBotSafeRefineScrolls(ch), hand ? (int)hand->GetRefineLevel() : -1);
	}

	// Either half of the rule.
	bool IsPlayerBotScrollRulePiece(LPCHARACTER ch, LPITEM item)
	{
		return IsPlayerBotScrollRuleWeapon(ch, item) || IsPlayerBotScrollRuleArmour(ch, item);
	}

	// Whether the rule has a piece to work on now: while it has, the scrolls
	// are that piece's - the stall keeps them all and the gambler takes none
	// (GetPlayerBotRefineScrollKeep). A counter sold them down to the three the
	// rule needs, the first step spent one, and the rule stopped at two.
	bool PlayerBotHasScrollRuleWork(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		return IsPlayerBotScrollRuleWeapon(ch, GetPlayerBotHandWeapon(ch)) ||
				IsPlayerBotScrollRuleArmour(ch, GetPlayerBotBodyArmour(ch));
	}

	bool PlayerBotWantsShield(LPCHARACTER ch);

	// Iwakura's Perfectionist ranks the gear: the weapon first, the armour,
	// the shield, and the rest - helmet, boots, jewellery - only once those
	// three stand at +7 ("wielka trojca"). A bow or a two-hander has no shield
	// to wait for.
	bool IsPlayerBotBigThreeSlot(int wearCell)
	{
		return wearCell == WEAR_WEAPON || wearCell == WEAR_BODY || wearCell == WEAR_SHIELD;
	}

	bool IsPlayerBotBigThreeAtPlus(LPCHARACTER ch, BYTE plus)
	{
		if (!ch)
			return false;
		LPITEM weapon = GetPlayerBotHandWeapon(ch);
		LPITEM body = ch->GetWear(WEAR_BODY);
		if (!weapon || weapon->GetRefineLevel() < plus || !body || body->GetRefineLevel() < plus)
			return false;
		if (!PlayerBotWantsShield(ch))
			return true;
		LPITEM shield = ch->GetWear(WEAR_SHIELD);
		return shield && shield->GetRefineLevel() >= plus;
	}

	// The Perfectionist's rank of a piece for the anvil: 0 the weapon, 1 the
	// armour, 2 the shield, 3 the rest.
	BYTE GetPlayerBotPerfectionistRank(LPCHARACTER ch, LPITEM item)
	{
		const int cell = item ? item->FindEquipCell(ch) : -1;
		return cell == WEAR_WEAPON ? 0 : (cell == WEAR_BODY ? 1 : (cell == WEAR_SHIELD ? 2 : 3));
	}

	// How far a bot means to take a piece on the plain anvil, where a failed
	// step burns it. What a scroll in the bag changes is GetPlayerBotRefineTarget.
	BYTE GetPlayerBotRefineAmbitionDrawn(LPCHARACTER ch, LPITEM item);
	// Defined below, beside the backup armour it is the other half of.
	LPITEM GetPlayerBotBodyArmour(LPCHARACTER ch);

	// The weapon, body armour or shield a bot fights in: the one worn, or -
	// with the slot empty, as it is for a whole blacksmith session - the one
	// that goes back on (GetPlayerBotHandWeapon, GetPlayerBotBodyArmour, and the
	// best shield in the bag). A spare in the bag is not one: it is the backup
	// the day the worn piece burns, or a project the anvil raises until its
	// numbers win.
	bool IsPlayerBotBigThreePieceInUse(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return false;
		if (item->IsEquipped())
			return true;
		const int cell = item->FindEquipCell(ch);
		if (cell == WEAR_WEAPON)
			return item == GetPlayerBotHandWeapon(ch);
		if (cell == WEAR_BODY)
			return item == GetPlayerBotBodyArmour(ch);
		if (cell != WEAR_SHIELD || ch->GetWear(WEAR_SHIELD))
			return false;
		LPITEM best = NULL;
		long long bestScore = 0;
		for (WORD bagCell = 0; bagCell < PLAYERBOT_BAG_CELLS; ++bagCell)
		{
			LPITEM shield = ch->GetInventoryItem(bagCell);
			if (!shield || shield->GetCell() != bagCell || shield->IsEquipped() ||
					!IsPlayerBotEquipmentCandidate(ch, shield) || shield->GetLevelLimit() > ch->GetLevel() ||
					shield->FindEquipCell(ch) != WEAR_SHIELD)
				continue;
			const long long score = GetPlayerBotEquipmentScore(shield, ch);
			if (!best || score > bestScore || (score == bestScore && shield->GetID() < best->GetID()))
			{
				best = shield;
				bestScore = score;
			}
		}
		return best == item;
	}

	BYTE GetPlayerBotRefineAmbition(LPCHARACTER ch, LPITEM item)
	{
		const BYTE drawn = GetPlayerBotRefineAmbitionDrawn(ch, item);
		if (!ch || !item || drawn == 0 || !IsPlayerBotPersonaEnabled() || IsPlayerBotArcherStoneWeapon(ch, item))
			return drawn;
		// The Perfectionist takes all its gear to +9 (Iwakura's Patch 4, point
		// 2: "bezwzglednie dazyc do ulepszenia calego swojego sprzetu (broni,
		// zbroi, bizuterii) na poziom +9"), the weapon, the armour and the
		// shield first. The rest - helmet, boots, jewellery - goes to +4 while
		// those three are under +7 (point 1: "nie beda juz korzystac z
		// ekwipunku na poziomach od +0 do +3"), and on to +9 once they stand
		// there. It waited at +0 for the three before, and bots with a sword,
		// an armour and a shield at +7 walked about in jewellery at +0. The draw
		// is what a bot aims at with the switch off.
		// "Swojego sprzetu" is the gear a bot fights in: a weapon, armour or
		// shield in the bag keeps the drawn aim. Taken to +9 as well, the backup
		// burned in the same session as the piece it was kept for - on m2zip,
		// the evening Patch 4 went in, Avatarv2 lost its sword and its long
		// sword at +8 within four seconds and went back to the merchant for a
		// Miecz six times in seventeen minutes.
		const int cell = item->FindEquipCell(ch);
		if (IsPlayerBotBigThreeSlot(cell))
			return IsPlayerBotBigThreePieceInUse(ch, item) ? PLAYERBOT_PERFECT_TARGET_PLUS : drawn;
		return IsPlayerBotBigThreeAtPlus(ch, 7) ? PLAYERBOT_PERFECT_TARGET_PLUS : PLAYERBOT_GEAR_MIN_PLUS;
	}

	BYTE GetPlayerBotRefineAmbitionDrawn(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return 0;
		// The Archer's stone dagger is a tool, not a prize: carry it to +4, where
		// the steps are still 90% and a burn is rare, and stop - going for +6
		// without a scroll would burn it and leave the bot breaking stones with a
		// bow again. (A scroll still takes it higher: GetPlayerBotRefineTarget.)
		if (IsPlayerBotArcherStoneWeapon(ch, item))
			return PLAYERBOT_ARCHER_STONE_MIN_REFINE;

		// Equipment is a primary progression system, not a side activity. Every bot
		// aims for at least +6, while a stable per-character/per-family personality
		// decides who risks +7, +8 or +9. The actual attempt still goes through
		// DoRefine(false), so every result pays the real fee, consumes real materials
		// and can burn at the normal server success rate.
		const DWORD familyVnum = item->GetVnum() >= item->GetRefineLevel()
				? item->GetVnum() - item->GetRefineLevel() : item->GetVnum();
		const int wearCell = item->FindEquipCell(ch);
		const DWORD seed = ch->GetPlayerID() ^ (familyVnum * 0x9e3779b9U) ^
				((DWORD)(wearCell + 2) * 0x85ebca6bU);
		const DWORD ambition = PlayerBotNavHash(seed ^ 0x52454649U) % 1000U;
		TPlayerBotAIStateMap::const_iterator stateIt =
				s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		const BYTE personality = stateIt != s_mapPlayerBotAIStates.end()
				? stateIt->second.bPersonality : BOT_PERSONALITY_STEADY_ADVENTURER;
		// Gear specialists deliberately accept more upgrade risk. Careful collectors
		// still has a small chance to become the lucky +8/+9 outlier, but normally
		// protects the equipment already earned.
		const DWORD plusNineChance = personality == BOT_PERSONALITY_GEAR_SPECIALIST
				? 120U : (personality == BOT_PERSONALITY_CAREFUL_COLLECTOR ? 20U : 50U);
		const DWORD plusEightChance = personality == BOT_PERSONALITY_GEAR_SPECIALIST
				? 320U : (personality == BOT_PERSONALITY_CAREFUL_COLLECTOR ? 90U : 150U);
		const DWORD plusSevenChance = personality == BOT_PERSONALITY_GEAR_SPECIALIST
				? 650U : (personality == BOT_PERSONALITY_CAREFUL_COLLECTOR ? 290U : 400U);
		if (ambition < plusNineChance)
			return 9; // exceptional 5% cohort
		if (ambition < plusEightChance)
			return 8; // another 10%
		if (ambition < plusSevenChance)
			return 7; // another 25%
		return 6;
	}

	// Defined in playerbot_economy.h, after the junk rule it stands beside.
	bool PlayerBotRefinesLowArmourForSale(LPCHARACTER ch, LPITEM item);
	// MT2009_PLUS_BOT_LIST_ALL_GEAR_V1: and its twin for every spare piece.
	bool PlayerBotRefinesSpareForSale(LPCHARACTER ch, LPITEM item);
	// Defined in playerbot_economy.h, after the backup rules it gives way to.
	bool PlayerBotRisksPlainAnvil(LPCHARACTER ch, LPITEM item);
	// Defined in playerbot_economy.h, beside the prize line it asks for.
	LPITEM FindPlayerBotLinesProject(LPCHARACTER ch);

	BYTE GetPlayerBotRefineTargetOwnBase(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return 0;
		// A body armour taken to +5 before it may go on a counter (Iwakura's
		// Patch 3, point 4). Not the one on the back under the scroll rule,
		// which a blacksmith session keeps in the bag from its first step on,
		// where that rule would have read it as a spare for the counter.
		if (!IsPlayerBotScrollRuleArmour(ch, item) && PlayerBotRefinesLowArmourForSale(ch, item))
			return PLAYERBOT_LOW_ARMOUR_SALE_PLUS;
		// MT2009_PLUS_BOT_LIST_ALL_GEAR_V1: a spare piece for the counter, to
		// +4 (PlayerBotRefinesSpareForSale).
		if (!IsPlayerBotScrollRuleArmour(ch, item) && PlayerBotRefinesSpareForSale(ch, item))
			return PLAYERBOT_SPARE_SALE_PLUS;
		// A level-30 weapon of its own class in the hand, or the one it is
		// grinding, goes to +9 whatever the personality: that is what the
		// weapon is for. A scroll-only one gets there under scrolls or not at
		// all (CanPlayerBotAttemptRefineItem).
		// Under the personalities the class's own goes to this visit's aim
		// instead (community patch 2, point 1: GetPlayerBotLevel30Aim).
		if (IsPlayerBotSpecialLevel30Weapon(item) && IsPlayerBotWeapon(ch, item) &&
				(item->IsEquipped() || IsPlayerBotLevel30Project(ch, item) ||
					(IsPlayerBotPersonaEnabled() && item == FindPlayerBotClassLevel30Weapon(ch))))
		{
			const BYTE aim = IsPlayerBotPersonaEnabled() ? GetPlayerBotLevel30Aim(ch, item)
					: PLAYERBOT_SCROLL_REFINE_MAX_PLUS;
			// The visit's aim is +6 for six bots in ten and holds until the next
			// town visit, so the field pass left a +6 alone: under the scroll
			// rule the one in the hand goes to +7 whatever the aim.
			return IsPlayerBotScrollRuleWeapon(ch, item)
					? std::max<BYTE>(aim, PLAYERBOT_SCROLL_RULE_WEAPON_PLUS) : aim;
		}
		// One of another class, ground for sale, as far as its ceiling.
		if (PlayerBotRefinesLevel30ForSale(ch, item))
			return GetPlayerBotLevel30SaleTarget(item);
		// Under Iwakura's personalities the helmet, the boots and the jewellery
		// go no further than +4 until the weapon, the armour and the shield
		// stand at +7 (GetPlayerBotRefineAmbition) - and a scroll in the bag,
		// which would otherwise make a ladder of any piece, does not change that.
		if (IsPlayerBotPersonaEnabled() && !IsPlayerBotArcherStoneWeapon(ch, item) &&
				!IsPlayerBotBigThreeSlot(item->FindEquipCell(ch)) && !IsPlayerBotBigThreeAtPlus(ch, 7))
			return PLAYERBOT_GEAR_MIN_PLUS;
		// A scroll in the bag is a ladder to +9 for everybody: under it a
		// failure costs a level or nothing, never the piece, so the ambition -
		// which is about not burning what was earned - does not apply while
		// one is there. See PLAYERBOT_SCROLL_REFINE_MAX_PLUS. Not for a piece
		// no scroll goes on (IsPlayerBotScrollFreeGear): the plain anvil takes
		// that one as far as the bot would risk it without.
		if (CountPlayerBotSafeRefineScrolls(ch) == 0 || IsPlayerBotScrollFreeGear(item))
			return GetPlayerBotRefineAmbition(ch, item);
		// Nor for a step the coin sends to the plain anvil (Iwakura's "tylko w
		// 50% uzywaja bodzi"): the scroll is not the way this time, so it is
		// no reason to climb past what the bot would risk without one.
		if (PlayerBotRisksPlainAnvil(ch, item))
			return GetPlayerBotRefineAmbition(ch, item);
		// The operator's SCROLL_FROM can put the ladder's first rung above
		// where a piece would climb by itself, and the steps under that rung
		// go to the plain anvil. So the ladder is the answer only for a piece
		// already on the rung or one whose own ambition carries it there; the
		// rest keep their ambition, or a bot content with +6 and scrolls kept
		// for +8 would try +7 unprotected for the sake of a scroll it may not
		// use. With no floor the first rung is +0 and every piece climbs.
		const int firstRung = GetPlayerBotScrollFromPlus() - 1;
		if ((int)item->GetRefineLevel() >= firstRung)
			return PLAYERBOT_SCROLL_REFINE_MAX_PLUS;
		const BYTE ambition = GetPlayerBotRefineAmbition(ch, item);
		return (int)ambition >= firstRung ? PLAYERBOT_SCROLL_REFINE_MAX_PLUS : ambition;
	}

	// And a piece under Iwakura's scroll rule - the weapon in the hand, the
	// armour on the back after it - goes to +7 at least, whatever the aim
	// above says (playerbot_refine_rules::ScrollRuleTarget): every pass that
	// asks a target, the planner, the material shopping and both refine passes
	// then agree that the step is wanted.
	BYTE GetPlayerBotRefineTargetOwn(LPCHARACTER ch, LPITEM item)
	{
		BYTE target = GetPlayerBotRefineTargetOwnBase(ch, item);
		// The helmet worn from level fifty goes to +6 at least, past the
		// persona's cap on the small pieces: it was the outdated gear's slot
		// that never climbed (PLAYERBOT_OUTDATED_HELMET_PLUS).
		if (ch && item && item->IsEquipped() && item->GetType() == ITEM_ARMOR && item->GetSubType() == ARMOR_HEAD &&
				(int)ch->GetLevel() >= PLAYERBOT_OUTDATED_GEAR_MIN_LEVEL && target < PLAYERBOT_OUTDATED_HELMET_PLUS)
			target = PLAYERBOT_OUTDATED_HELMET_PLUS;
		return IsPlayerBotScrollRulePiece(ch, item)
				? (BYTE)playerbot_refine_rules::ScrollRuleTarget((int)target) : target;
	}

	// A weapon worked on in the bag under the one in the hand - the class's
	// level-30 copy with the better lines, or the weapon kept for its lines
	// (FindPlayerBotLinesProject) - is taken at least to the hand's plus, where
	// it hits harder and the equipment pass puts it on. An aim under the hand's
	// plus left it in the bag for good.
	BYTE GetPlayerBotRefineTarget(LPCHARACTER ch, LPITEM item)
	{
		// The sash's grail goes to PLAYERBOT_SASH_GRAIL_PLUS; the anvil's
		// table and scrolls decide how (playerbot_sash.h).
		if (IsPlayerBotSashGrailProject(ch, item))
			return std::max<BYTE>(GetPlayerBotRefineTargetOwn(ch, item), PLAYERBOT_SASH_GRAIL_PLUS);
		const BYTE target = GetPlayerBotRefineTargetOwn(ch, item);
		if (!ch || !item || item->IsEquipped() || item->GetType() != ITEM_WEAPON)
			return target;
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		if (!worn || worn->GetRefineLevel() <= target)
			return target;
		const bool classCopy = IsPlayerBotClassLevel30Weapon(ch, item) && item == FindPlayerBotClassLevel30Weapon(ch);
		if (!classCopy && item != FindPlayerBotLinesProject(ch))
			return target;
		return std::min<BYTE>(worn->GetRefineLevel(), PLAYERBOT_SCROLL_REFINE_MAX_PLUS);
	}

	// What the village merchants actually stock, and what they charge for it.
	//
	// The three shops hold sixty-four rows between them in this world, and
	// nothing outside them can be bought by a player at any counter. The
	// progression ladder walks item_proto by vnum stride instead, so it named
	// pieces no shop has ever sold - and the purchase below simply created
	// them. What a player saw was a bot in a level-60 Mask of Fear bought for
	// twenty thousand yang at the armour merchant, with the log line to prove
	// it (jaksiezabic). A bot buys what a player could buy at the same counter,
	// at the same price; everything above that comes from drops, the counters
	// and the blacksmith, exactly as it does for a player.
	bool FindPlayerBotMerchantOffer(DWORD vnum, long long* priceOut)
	{
		static const DWORD merchants[] = { 9001, 9002, 9003 };
		for (size_t i = 0; i < sizeof(merchants) / sizeof(merchants[0]); ++i)
		{
			LPSHOP shop = CShopManager::instance().GetByNPCVnum(merchants[i]);
			if (!shop)
				continue;
			const std::vector<CShop::SHOP_ITEM>& offers = shop->GetItemVector();
			for (size_t k = 0; k < offers.size(); ++k)
			{
				if (offers[k].vnum != vnum)
					continue;
				if (priceOut)
					*priceOut = offers[k].price > 0 ? offers[k].price : 0;
				return true;
			}
		}
		return false;
	}

	// The class's body-armour family base (11200/11400/11600/11800), so a
	// warrior never buys a shaman's robe. Shields and helmets are shared.
	DWORD GetPlayerBotArmorClassBase(LPCHARACTER ch)
	{
		if (!ch)
			return 11200;
		switch (ch->GetJob())
		{
			case JOB_ASSASSIN: return 11400;
			case JOB_SURA:     return 11600;
			case JOB_SHAMAN:   return 11800;
			default:           return 11200;
		}
	}

	// The best piece an NPC merchant actually stocks for a wear slot that
	// this bot's level and class can use. The progression ladder walks
	// item_proto by stride and names tiers no shop sells - body armour at
	// level 9, and everything from level 34 up - so a bot between two
	// stocked tiers, or above the top one, could never buy and walked the
	// world in an empty slot: a quarter of the cohort had no body armour
	// (13 September). This finds the highest stocked piece the bot
	// qualifies for, so the slot is filled and the blacksmith can raise it.
	// Whether this character's class and sex may wear a proto at all: the
	// anti-flag half of CItem::CanUsedBy and IsPlayerBotEquipmentCandidate, for
	// a piece the bot does not hold yet.
	bool IsPlayerBotProtoForCharacter(LPCHARACTER ch, const TItemTable* proto)
	{
		if (!ch || !proto)
			return false;
		DWORD classFlag = 0;
		switch (ch->GetJob())
		{
			case JOB_WARRIOR:  classFlag = ITEM_ANTIFLAG_WARRIOR; break;
			case JOB_ASSASSIN: classFlag = ITEM_ANTIFLAG_ASSASSIN; break;
			case JOB_SURA:     classFlag = ITEM_ANTIFLAG_SURA; break;
			case JOB_SHAMAN:   classFlag = ITEM_ANTIFLAG_SHAMAN; break;
			default: break;
		}
		if (classFlag != 0 && IS_SET(proto->dwAntiFlags, classFlag))
			return false;
		return !IS_SET(proto->dwAntiFlags,
				GET_SEX(ch) == SEX_MALE ? ITEM_ANTIFLAG_MALE : ITEM_ANTIFLAG_FEMALE);
	}

	// The body armour a bot keeps for the day the one on its back burns: the
	// best other body armour in the bag it can put on now, which is the set
	// IsPlayerBotWornArmourAtRisk counts. That rule holds the armour on the
	// back off every step of the plain anvil that can burn it while there is
	// none (THC, 16 September), and nothing kept one - the old armour went to
	// the merchant at the first visit after an upgrade, as scrap. On m2zip on
	// 24 September 377 of the 1027 bots of 25 and up wore a body armour whose
	// next step could burn it and had no other in the bag, the hold logged
	// some twelve thousand times a minute, and the whole world held 39
	// Blessing Scrolls to take the step instead ("do 25 lvla ladnie ulepszaja
	// itemy na +9 a potem nic", Iwakura). Like the backup
	// weapon it is neither scrap nor counter goods nor the storekeeper's nor
	// the gambler's, and the armour merchant sells one when there is none
	// (NeedsPlayerBotBackupArmour).
	bool IsPlayerBotBackupArmourCandidate(LPCHARACTER ch, LPITEM spare)
	{
		return ch && spare && spare->GetType() == ITEM_ARMOR && spare->GetSubType() == ARMOR_BODY &&
				!spare->IsEquipped() && spare->GetLevelLimit() <= (int)ch->GetLevel() &&
				IsPlayerBotProtoForCharacter(ch, spare->GetProto());
	}

	// The best body armour in the bag this bot can put on now, leaving one out.
	LPITEM FindPlayerBotBestBagArmour(LPCHARACTER ch, LPITEM exclude)
	{
		if (!ch || !ch->IsItemLoaded())
			return NULL;
		LPITEM best = NULL;
		long long bestScore = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM spare = ch->GetInventoryItem(cell);
			if (!spare || spare == exclude || spare->GetCell() != cell ||
					!IsPlayerBotBackupArmourCandidate(ch, spare))
				continue;
			const long long score = GetPlayerBotEquipmentScore(spare, ch);
			if (!best || score > bestScore)
			{
				best = spare;
				bestScore = score;
			}
		}
		return best;
	}

	// The armour on the back, or - with the slot empty - the one that goes
	// back on: the weapon's GetPlayerBotHandWeapon for the other slot. A
	// blacksmith session keeps the piece in the bag from its first step to its
	// last, and asking the slot alone protected the worn armour on the first
	// step and on none after it.
	LPITEM GetPlayerBotBodyArmour(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return NULL;
		LPITEM worn = ch->GetWear(WEAR_BODY);
		if (worn)
			return worn;
		// The piece the session took off, not the best of the bag (B12).
		LPITEM taken = FindPlayerBotTakenOffPiece(ch, WEAR_BODY);
		if (taken && IsPlayerBotBackupArmourCandidate(ch, taken))
			return taken;
		return FindPlayerBotBestBagArmour(ch, NULL);
	}

	LPITEM FindPlayerBotBackupArmour(LPCHARACTER ch)
	{
		LPITEM body = GetPlayerBotBodyArmour(ch);
		if (!body)
			return NULL;
		// Never a spare at +7 or past that the armour on the back matches or
		// beats (Iwakura's Patch 4, point 7): that one is goods.
		const long long bodyScore = GetPlayerBotEquipmentScore(body, ch);
		LPITEM best = NULL;
		long long bestScore = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM spare = ch->GetInventoryItem(cell);
			if (!spare || spare == body || spare->GetCell() != cell ||
					!IsPlayerBotBackupArmourCandidate(ch, spare))
				continue;
			const long long score = GetPlayerBotEquipmentScore(spare, ch);
			if (spare->GetRefineLevel() >= PLAYERBOT_SPARE_GOODS_MIN_PLUS && score <= bodyScore)
				continue;
			if (!best || score > bestScore)
			{
				best = spare;
				bestScore = score;
			}
		}
		return best;
	}

	// Iwakura's Patch 4, point 7: a weapon, body armour or shield of this bot's
	// own at +7 or past that the piece it fights in matches or beats is goods -
	// "zamiast bezczynnie lezec w ekwipunku, przedmioty te sa obowiazkowo
	// wystawiane na prywatnym sklepie offline podczas najblizszej wizyty w
	// miescie" - whether it lies in the bag or in the box: neither the backup
	// (FindPlayerBotBackupWeapon, FindPlayerBotBackupArmour) nor a project for
	// the anvil (IsPlayerBotHigherTierSpare), and the box gives it back to the
	// counter (WithdrawPlayerBotSafebox).
	bool IsPlayerBotFinishedSpareGoods(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->IsEquipped() || item->GetRefineLevel() < PLAYERBOT_SPARE_GOODS_MIN_PLUS ||
				!IsPlayerBotEquipmentCandidate(ch, item))
			return false;
		LPITEM worn = NULL;
		if (item->GetType() == ITEM_WEAPON && item->GetSubType() != WEAPON_ARROW)
			worn = GetPlayerBotHandWeapon(ch);
		else if (item->GetType() == ITEM_ARMOR && item->GetSubType() == ARMOR_BODY)
			worn = GetPlayerBotBodyArmour(ch);
		else if (item->GetType() == ITEM_ARMOR && item->GetSubType() == ARMOR_SHIELD)
			worn = ch->GetWear(WEAR_SHIELD);
		if (!worn || worn == item)
			return false;
		return GetPlayerBotEquipmentScore(worn, ch) >= GetPlayerBotEquipmentScore(item, ch);
	}

	// The junk rule asks this of every body armour in the bag, so the answer
	// is kept for PLAYERBOT_BACKUP_WEAPON_CACHE_MS, as the weapon's is.
	std::map<DWORD, TPlayerBotBackupWeaponAnswer> s_mapPlayerBotBackupArmour;

	DWORD GetPlayerBotBackupArmourID(LPCHARACTER ch, bool fresh)
	{
		if (!ch)
			return 0;
		const DWORD now = get_dword_time();
		TPlayerBotBackupWeaponAnswer& answer = s_mapPlayerBotBackupArmour[ch->GetPlayerID()];
		if (fresh || answer.dwTime == 0 || now - answer.dwTime >= PLAYERBOT_BACKUP_WEAPON_CACHE_MS)
		{
			LPITEM backup = FindPlayerBotBackupArmour(ch);
			answer.dwTime = now != 0 ? now : 1;
			answer.dwItemID = backup ? backup->GetID() : 0;
		}
		return answer.dwItemID;
	}

	bool IsPlayerBotKeptBackupArmour(LPCHARACTER ch, LPITEM item, bool fresh = false)
	{
		// MT2009_PLUS_BOT_SHAMAN_INT_SET_V1: and its INT shield, earrings and
		// necklace (and, while the set is on, the fighting pieces in the bag).
		if (ch && item && item->GetType() == ITEM_ARMOR && IsPlayerBotBuffSetPiece(ch, item))
			return true;
		return ch && item && item->GetType() == ITEM_ARMOR && item->GetSubType() == ARMOR_BODY &&
				item->GetID() != 0 && GetPlayerBotBackupArmourID(ch, fresh) == item->GetID();
	}

	// Whether some counter in this world holds a level-30 weapon this bot
	// could wield, read off the market ledger without walking to one.
	bool PlayerBotMarketHasClassLevel30Weapon(LPCHARACTER ch)
	{
		static const DWORD families[] = { 290, 1170, 2150, 3210, 5110, 7160 };
		for (size_t i = 0; ch && i < sizeof(families) / sizeof(families[0]); ++i)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(families[i]);
			if (!proto || proto->bType != ITEM_WEAPON || !IsPlayerBotWeaponSubTypeFor(ch, proto->bSubType) ||
					!IsPlayerBotProtoForCharacter(ch, proto))
				continue;
			for (DWORD plus = 0; plus < 10; ++plus)
			{
				const TPlayerBotMarketLedgerEntry* entry = GetPlayerBotMarketLedgerEntry(families[i] + plus);
				if (entry && entry->dwSupplyUnits > 0)
					return true;
			}
		}
		return false;
	}

	DWORD FindPlayerBotBestMerchantSlotVnum(LPCHARACTER ch, int wearCell)
	{
		if (!ch)
			return 0;
		DWORD lo = 0, hi = 0;
		BYTE subtype = 0;
		switch (wearCell)
		{
			case WEAR_BODY:   lo = GetPlayerBotArmorClassBase(ch); hi = lo + 199; subtype = ARMOR_BODY; break;
			case WEAR_HEAD:   lo = 12000; hi = 12999; subtype = ARMOR_HEAD; break;
			case WEAR_SHIELD: lo = 13000; hi = 13999; subtype = ARMOR_SHIELD; break;
			case WEAR_FOOTS:  lo = 15000; hi = 15999; subtype = ARMOR_FOOTS; break;
			default: return 0;
		}
		static const DWORD merchants[] = { 9001, 9002, 9003 };
		DWORD bestVnum = 0;
		int bestLevel = -1;
		for (size_t i = 0; i < sizeof(merchants) / sizeof(merchants[0]); ++i)
		{
			LPSHOP shop = CShopManager::instance().GetByNPCVnum(merchants[i]);
			if (!shop)
				continue;
			const std::vector<CShop::SHOP_ITEM>& offers = shop->GetItemVector();
			for (size_t k = 0; k < offers.size(); ++k)
			{
				const DWORD vnum = offers[k].vnum;
				if (vnum < lo || vnum > hi)
					continue;
				TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
				if (!proto || proto->bType != ITEM_ARMOR || proto->bSubType != subtype)
					continue;
				// A merchant's helmets are every class's at once, told apart only
				// by the anti-flags. A sura was sold the warrior's Tradycyjny Helm,
				// could never wear it, and went without a helmet to level 74
				// (NaCoPaczysz, 14 September).
				if (!IsPlayerBotProtoForCharacter(ch, proto))
					continue;
				const int reqLevel = GetPlayerBotProtoLevelLimit(proto);
				if (reqLevel > (int)ch->GetLevel())
					continue;
				if (reqLevel > bestLevel)
				{
					bestVnum = vnum;
					bestLevel = reqLevel;
				}
			}
		}
		return bestVnum;
	}

	// Fill an armour slot from the merchant with the best it stocks, unless
	// the bot already holds (worn or in the bag) a piece of at least that
	// level for the slot - so it never buys a second copy of a piece the
	// merchant cannot better, and never a downgrade.
	bool BuyPlayerBotBestMerchantSlotGear(LPCHARACTER ch, int wearCell, const char* category)
	{
		if (!ch)
			return false;
		const DWORD vnum = FindPlayerBotBestMerchantSlotVnum(ch, wearCell);
		if (vnum == 0 || HasPlayerBotProgressionGear(ch, vnum, wearCell))
			return false;
		return BuyPlayerBotProgressionGear(ch, vnum, category);
	}

	bool BuyPlayerBotProgressionGear(LPCHARACTER ch, DWORD vnum, const char* category)
	{
		if (!ch || vnum == 0)
			return false;
		TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		if (!proto || ch->GetEmptyInventory(std::max(1, (int)proto->bSize)) < 0)
			return false;

		long long price = 0;
		if (!FindPlayerBotMerchantOffer(vnum, &price))
			return false;
		if (price <= 0)
			price = proto->dwShopBuyPrice > 0 ? proto->dwShopBuyPrice : proto->dwGold;
		price = std::max<long long>(100, price);
		if (ch->GetGold() < price)
			return false;

		LPITEM item = ch->AutoGiveItem(vnum, 1, -1, false);
		if (!item)
			return false;
		PlayerBotChangeGold(ch, -price);
		// The gear history reads log.log, and a bot's purchase from a merchant
		// wrote nothing there: the card showed a piece that came from nowhere
		// (B23 of Iwakura's audit of 26 September).
		LogManager::instance().ItemLog(ch, item, "PLAYERBOT_NPC_BUY", item->GetName());
		NotePlayerBotExplainOrigin(item, per::ORIGIN_NPC_LADDER, per::LadderCategoryCode(category));
		sys_log(0, "PLAYERBOT_GEAR: bought progression %s pid=%u name=%s vnum=%u required_level=%d price=%lld",
				category ? category : "gear", ch->GetPlayerID(), ch->GetName(), vnum,
				item->GetLevelLimit(), price);
		return true;
	}

	// What an arrow adds to a shot, as the engine counts it: value3, which
	// both engines add to the bow's roll (wooden 3 to silver 25). -1 for an
	// arrow that cannot hurt anything at a bow's range: mt2009's
	// CalcArrowDamage fades a shot past value4 down to value2 per cent (twice
	// that against a monster) at value5, and the four elemental arrows
	// (8006-8009) carry 0 in all three there, so past point-blank they deal
	// nothing. Worn with a quiver that never empties, one of them would be an
	// Archer that shoots for nothing for life. r40250 fades a shot by the
	// distance alone and reads no value2.
	int GetPlayerBotArrowGrade(LPITEM arrow)
	{
		if (!arrow || arrow->GetType() != ITEM_WEAPON || arrow->GetSubType() != WEAPON_ARROW)
			return -1;
#if defined(PLAYERBOT_ENGINE_MT2009)
		if (arrow->GetValue(2) <= 0)
			return -1;
#endif
		return std::max<int>(0, (int)arrow->GetValue(3));
	}

	// MT2009_PLUS_QUIVER_V1: a quiver - Kolczan (8010), the ItemShop's, 14 days -
	// is an arrow with a real-time limit, the engine's own rule
	// (Mt2009PlusIsQuiver, server-patches/quiver): its arrows never run out, so
	// one worn is a full quiver for every count below, the equipment pass never
	// trades it for a stack, and one in the bag is nocked before any stack and
	// never junk. Bots never buy one (playerbot_itemshop.h has no wish for it);
	// this is for one a GM hands over.
	bool IsPlayerBotQuiver(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_WEAPON || item->GetSubType() != WEAPON_ARROW)
			return false;
		for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
			if (item->GetLimitType(i) == LIMIT_REAL_TIME)
				return true;
		return false;
	}

	// Arrows this bot can nock now. A progression chest hands an archer the
	// next tier early - 8003 wants level forty, 8004 forty-five - and counting
	// those said "a hundred arrows, no need to buy" to a bot of thirty-four
	// whose bow had nothing to fire: PrepareWeapon failed on every tick, the
	// tick left through a town visit no frontier map can start, and twelve
	// archers stood at arrival points for twenty minutes at a time.
	bool IsPlayerBotUsableArrow(LPCHARACTER ch, LPITEM item)
	{
		return item && GetPlayerBotArrowGrade(item) >= 0 &&
				item->GetCount() > 0 && item->GetLevelLimit() <= ch->GetLevel() &&
				!IsPlayerBotSidekickHeld(ch, item); // MT2009_PLUS_SIDEKICK_SHOP_ERRAND_V1: not what a companion holds for its owner
	}

	// The bag's best arrow this bot can nock now, by grade; the first of equals.
	LPITEM FindPlayerBotBestBagArrow(LPCHARACTER ch)
	{
		LPITEM best = NULL;
		int bestGrade = -1;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!IsPlayerBotUsableArrow(ch, item))
				continue;
			// MT2009_PLUS_QUIVER_V1: a quiver before any stack.
			const int grade = GetPlayerBotArrowGrade(item) + (IsPlayerBotQuiver(item) ? 1000 : 0);
			if (grade > bestGrade)
			{
				best = item;
				bestGrade = grade;
			}
		}
		return best;
	}

	int CountPlayerBotArrows(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		int count = 0;
		LPITEM worn = ch->GetWear(WEAR_ARROW);
		if (IsPlayerBotUsableArrow(ch, worn))
			count += IsPlayerBotQuiver(worn) ? 1000 : worn->GetCount(); // MT2009_PLUS_QUIVER_V1
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (IsPlayerBotUsableArrow(ch, item))
				count += item->GetCount();
		}
		return count;
	}

	bool EnsurePlayerBotArrowsEquipped(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		if (IsPlayerBotUsableArrow(ch, ch->GetWear(WEAR_ARROW)))
			return true;
		LPITEM best = FindPlayerBotBestBagArrow(ch);
		return best && PlayerBotEquipItem(ch, best, WEAR_ARROW);
	}

	// The quiver is never emptied - a bot's shot spends no arrow - so a better
	// arrow picked up later was never nocked by running out, as it used to
	// be: the equipment pass puts it in the slot on its own clock, and what
	// comes off is scrap to the junk rule. An arrow slot is exempt from
	// EquipItem's stand-still rule, so this may run in the middle of a fight.
	bool UpgradePlayerBotArrows(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		LPITEM worn = ch->GetWear(WEAR_ARROW);
		// MT2009_PLUS_QUIVER_V1: a worn quiver stays; one in the bag goes in.
		if (IsPlayerBotUsableArrow(ch, worn) && IsPlayerBotQuiver(worn))
			return false;
		const int wornGrade = IsPlayerBotUsableArrow(ch, worn) ? GetPlayerBotArrowGrade(worn) : -1;
		LPITEM best = FindPlayerBotBestBagArrow(ch);
		if (!best || (!IsPlayerBotQuiver(best) && GetPlayerBotArrowGrade(best) <= wornGrade))
			return false;
		const DWORD oldVnum = worn ? worn->GetVnum() : 0;
		const DWORD newVnum = best->GetVnum();
		if (!PlayerBotEquipItem(ch, best, WEAR_ARROW))
			return false;
		sys_log(0, "PLAYERBOT_GEAR: nocked a better arrow pid=%u name=%s old_vnum=%u new_vnum=%u",
				ch->GetPlayerID(), ch->GetName(), oldVnum, newVnum);
		FlushPlayerBotItemRow(ch->GetWear(WEAR_ARROW));
		return true;
	}

	// Whether a shield slot is a slot this bot can fill at all: never with a
	// bow or a two-handed weapon in hand. Counting it as "missing" for an
	// archer made every archer critically short of town services for life -
	// sent out of M3 the moment it arrived and straight back by the weapon
	// hunt, fifteen seconds a round trip.
	bool PlayerBotWantsShield(LPCHARACTER ch)
	{
		LPITEM weapon = ch ? ch->GetWear(WEAR_WEAPON) : NULL;
		return !(weapon && weapon->GetType() == ITEM_WEAPON &&
				(weapon->GetSubType() == WEAPON_BOW || weapon->GetSubType() == WEAPON_TWO_HANDED));
	}

	// An Archer with no arrow it can nock: the only time it has to buy any,
	// since the quiver is never emptied (PLAYERBOT_ARROW_RESTOCK_THRESHOLD).
	// A dropper's archer and a trial archer used to fill their quivers to a
	// thousand here, because a Monkey Dungeon visit and the desert trial shot
	// their bundles away in minutes; nothing is shot away now.
	bool NeedsPlayerBotArrows(LPCHARACTER ch)
	{
		if (!ch || ch->GetJob() != JOB_ASSASSIN || ch->GetSkillGroup() != 2)
			return false;
		return CountPlayerBotArrows(ch) < PLAYERBOT_ARROW_RESTOCK_THRESHOLD;
	}

	long long GetPlayerBotNpcPurchasePrice(const TItemTable* proto, int count)
	{
		if (!proto || count <= 0)
			return 0;
		if (IS_SET(proto->dwFlags, ITEM_FLAG_COUNT_PER_1GOLD))
			return proto->dwGold == 0 ? count : count / proto->dwGold;
		return (long long)proto->dwGold * count;
	}

	// Kamien Duszy, the stone for a weapon or armour socket. The vnum is
	// 28[grade][kind]: 28037 is Potwora +0, 28437 Potwora +4 - the grade is the
	// hundreds digit, and reading it from the last digit (as this once did)
	// made every stone a +0 and switched the grade rules off.
	int GetPlayerBotSoulStoneGrade(DWORD vnum) { return (int)((vnum / 100) % 10); }
	// MT2009_PLUS_SOULSTONE9_V1 (Digi Rasta's soul stones +5..+9): +6..+9 are
	// 28g00+k, not 28g30+k - read them as the same kind (30 + k).
	int GetPlayerBotSoulStoneKind(DWORD vnum) { const int k = (int)(vnum % 100); return ((vnum / 100) % 10 >= 6 && k < 14) ? k + 30 : k; }
	bool IsPlayerBotWeaponSoulStoneKind(int kind) { return kind >= 30 && kind <= 37; }
	bool IsPlayerBotArmorSoulStoneKind(int kind) { return kind >= 38 && kind <= 43; }

	// What a stone is worth to the hunting set, by Iwakura's list alone: his
	// PvE tier when he lets it into a socket and rates it neutral or better,
	// else nothing. Before his list this was a table of our own by school, and
	// it seated a +0 to +2 on anything under +6 - which his list forbids
	// ("Boty maja calkowity zakaz umieszczania Kamieni Duszy +0, +1 i +2 w
	// broniach i zbrojach") outside the operator's exception for weak pieces
	// (PLAYERBOT_SOUL_STONE_WEAK_GEAR_*). A stone of a banned grade takes the
	// best tier his list gives its kind, so the exception seats the kinds he
	// wants and not Magii or Powtorki. The class stones he keeps for a PvP
	// weapon, which no bot assembles, so they are never seated.
	int GetPlayerBotSoulStoneSeatTier(DWORD vnum)
	{
		int tier = GetPlayerBotSoulStoneTier(vnum, false);
		if (GetPlayerBotSoulStoneGrade(vnum) < PLAYERBOT_SOUL_STONE_MIN_GRADE)
		{
			const int kind = GetPlayerBotSoulStoneKind(vnum);
			for (size_t i = 0; i < sizeof(PLAYERBOT_SOUL_STONE_TIERS) / sizeof(PLAYERBOT_SOUL_STONE_TIERS[0]); ++i)
			{
				const TPlayerBotSoulStoneTier& row = PLAYERBOT_SOUL_STONE_TIERS[i];
				if (GetPlayerBotSoulStoneKind(row.dwVnum) == kind && !row.bPvpOnly)
					tier = std::max<int>(tier, row.bPve);
			}
		}
		return tier >= PLAYERBOT_SOUL_STONE_MIN_PVE_TIER ? tier : 0;
	}

	// The operator's weak piece: level 21 or less and +6 or less.
	bool IsPlayerBotWeakSoulStoneGear(LPITEM gear)
	{
		return gear && gear->GetLevelLimit() <= PLAYERBOT_SOUL_STONE_WEAK_GEAR_MAX_LEVEL &&
				gear->GetRefineLevel() <= PLAYERBOT_SOUL_STONE_WEAK_GEAR_MAX_REFINE;
	}

	// Whether the worn piece for this kind of stone has a socket open for it
	// and does not hold the same kind already (the engine refuses a second).
	bool FindPlayerBotSoulStoneSocket(LPCHARACTER ch, int kind, DWORD stoneValue5, LPITEM* outGear, int* outSocket)
	{
		if (!ch)
			return false;
		LPITEM gear = IsPlayerBotWeaponSoulStoneKind(kind) ? ch->GetWear(WEAR_WEAPON)
				: (IsPlayerBotArmorSoulStoneKind(kind) ? ch->GetWear(WEAR_BODY) : NULL);
		// Seven seatings in ten weld a cracked stone into the socket: not in
		// the piece a companion's owner put on.
		if (!gear || IsPlayerBotSidekickPinned(ch, gear))
			return false;
		int openSocket = -1;
		for (int socketIdx = 0; socketIdx < ITEM_SOCKET_MAX_NUM; ++socketIdx)
		{
			const DWORD inSocket = (DWORD)gear->GetSocket(socketIdx);
			if (inSocket == 1 && openSocket < 0)
				openSocket = socketIdx;
			if (inSocket <= 2 || inSocket == PLAYERBOT_BROKEN_SOUL_STONE_VNUM)
				continue;
			const TItemTable* seated = ITEM_MANAGER::instance().GetTable(inSocket);
			if (seated && (DWORD)seated->alValues[5] == stoneValue5)
				return false;
		}
		if (openSocket < 0)
			return false;
		if (outGear)
			*outGear = gear;
		if (outSocket)
			*outSocket = openSocket;
		return true;
	}

	// A seating is a 30% roll, and the other 70% welds a cracked stone into
	// the socket for good. A stone under Iwakura's lowest grade goes only into
	// the operator's weak piece; a +3 or +4 is never spent on a piece below
	// +6, and on a +8 or +9 the socket waits for the +4.
	bool ShouldPlayerBotSeatSoulStone(LPITEM gear, int grade)
	{
		if (!gear)
			return false;
		if (grade < PLAYERBOT_SOUL_STONE_MIN_GRADE)
			return grade >= PLAYERBOT_SOUL_STONE_WEAK_MIN_GRADE &&
					IsPlayerBotWeakSoulStoneGear(gear);
		const int refine = gear->GetRefineLevel();
		if (refine >= PLAYERBOT_SOUL_STONE_TOP_GEAR_REFINE)
			return grade >= PLAYERBOT_SOUL_STONE_TOP_GEAR_MIN_GRADE;
		return refine >= PLAYERBOT_SOUL_STONE_MIN_GEAR_REFINE;
	}

	// Whether this stone would go into a socket of what the bot wears now:
	// what the counter must not sell.
	bool CanPlayerBotSeatSoulStone(LPCHARACTER ch, DWORD vnum, DWORD stoneValue5)
	{
		const int kind = GetPlayerBotSoulStoneKind(vnum);
		if (GetPlayerBotSoulStoneSeatTier(vnum) <= 0)
			return false;
		LPITEM gear = NULL;
		int socket = -1;
		return FindPlayerBotSoulStoneSocket(ch, kind, stoneValue5, &gear, &socket) &&
				ShouldPlayerBotSeatSoulStone(gear, GetPlayerBotSoulStoneGrade(vnum));
	}

	// Whether the bot would buy this stone off a counter: one it would seat,
	// of Iwakura's grades - the weak piece's +0..+2 are for what drops.
	bool WantsPlayerBotSoulStone(LPCHARACTER ch, DWORD vnum, DWORD stoneValue5)
	{
		return GetPlayerBotSoulStoneGrade(vnum) >= PLAYERBOT_SOUL_STONE_MIN_GRADE &&
				CanPlayerBotSeatSoulStone(ch, vnum, stoneValue5);
	}

	// Kamien Duszy proper: 28[grade][kind] with a kind of 30 to 43. Other
	// ITEM_METIN exist, and the Alchemist's own test (grade = vnum / 100 - 280)
	// would read them as nonsense grades.
	bool IsPlayerBotSoulStoneVnum(DWORD vnum)
	{
		return vnum >= 28000 && vnum < 29000 && // MT2009_PLUS_SOULSTONE9_V1: +5..+9 too
				GetPlayerBotSoulStoneKind(vnum) >= 30 && GetPlayerBotSoulStoneKind(vnum) <= 43;
	}

	// The fifteen in a hundred of the banned grades that stay goods for a
	// counter (PLAYERBOT_SOUL_STONE_MARKET_PERCENT), by item id: the same
	// stone gets the same answer in the bag, on the counter and at the
	// Alchemist, and a line taken off a counter keeps its id.
	bool IsPlayerBotSoulStoneForMarket(DWORD itemId)
	{
		return PlayerBotNavHash(itemId ^ 0x4b44504dU) % 100U < (DWORD)PLAYERBOT_SOUL_STONE_MARKET_PERCENT;
	}

	// A stone the Alchemist turns into dust: a soul stone of a grade Iwakura
	// bans from sockets, not one of the fifteen kept for the market, and not
	// one the operator's weak piece would take now. Asked of an offline
	// counter's line by its id and vnum, before it is an item in the bag.
	bool IsPlayerBotSoulStoneForDustOf(LPCHARACTER ch, DWORD vnum, DWORD itemId, DWORD stoneValue5)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		if (!IsPlayerBotSoulStoneVnum(vnum) ||
				GetPlayerBotSoulStoneGrade(vnum) > PLAYERBOT_SOUL_STONE_DUST_MAX_GRADE)
			return false;
		if (IsPlayerBotSoulStoneForMarket(itemId))
			return false;
		return !ch || !CanPlayerBotSeatSoulStone(ch, vnum, stoneValue5);
#else
		(void)ch; (void)vnum; (void)itemId; (void)stoneValue5;
		return false;
#endif
	}

	// The operator's word on the item (playerbot_item_policy.tsv, the panel's
	// item page) wins over Iwakura's rule here as everywhere: a stone put on
	// keep, stall, merchant or drop is not the Alchemist's.
	bool IsPlayerBotSoulStoneForDust(LPCHARACTER ch, LPITEM item)
	{
		return item && item->GetType() == ITEM_METIN &&
				GetPlayerBotItemPolicy(item) == PLAYERBOT_ITEM_POLICY_NONE &&
				IsPlayerBotSoulStoneForDustOf(ch, item->GetVnum(), item->GetID(),
						(DWORD)item->GetValue(5));
	}

	// Does this bot have a socket that a stone worth having could still fill?
	// The market question, asked before a trip.
	bool PlayerBotHasOpenSoulStoneSocket(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		const BYTE slots[2] = { WEAR_WEAPON, WEAR_BODY };
		for (int s = 0; s < 2; ++s)
		{
			LPITEM gear = ch->GetWear(slots[s]);
			if (!gear || gear->GetRefineLevel() < PLAYERBOT_SOUL_STONE_MIN_GEAR_REFINE)
				continue;
			for (int socketIdx = 0; socketIdx < ITEM_SOCKET_MAX_NUM; ++socketIdx)
				if ((DWORD)gear->GetSocket(socketIdx) == 1)
					return true;
		}
		return false;
	}

	DWORD GetPlayerBotNpcSellUnitPrice(LPITEM item)
	{
		if (!item || !item->GetProto() || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_SELL))
			return 0;

		DWORD price = item->GetShopBuyPrice();
		if (IS_SET(item->GetFlag(), ITEM_FLAG_COUNT_PER_1GOLD))
			price = price == 0 ? 1 : 1 / price;
		price /= 5;
		price -= price * 3 / 100;
		return price;
	}

	// Is this piece one the bot could put on right now, and better than what it
	// already wears there?
	//
	// The stall listed any weapon or armour whose slot was already filled, which
	// reads as "this is a spare" and nearly always is. A player handing a bot a
	// pair of +9 boots does not fill an empty slot, it beats a full one - and the
	// counter got there first, because the private shop pass runs at the top of
	// the tick and the equipment pass three hundred lines below it. Reported from
	// the Discord by three people in one afternoon, each of whom had just given a
	// bot something good and watched it go on sale at the top of the counter: a
	// spare at +6 or better is the highest-scoring thing a stall can carry.
	//
	// "Could put on right now" is the engine's own CanEquipNow, so a piece the
	// bot has not grown into is not held off the market on a promise: a level-30
	// sword in the bag of a bot of five is goods, and stays goods.
	bool IsPlayerBotWearableUpgrade(LPCHARACTER ch, LPITEM item, WORD cell)
	{
		if (!IsPlayerBotEquipmentCandidate(ch, item))
			return false;
		const int wearCell = item->FindEquipCell(ch);
		if (wearCell < 0 || wearCell >= WEAR_MAX_NUM)
			return false;
		LPITEM worn = ch->GetWear((BYTE)wearCell);
		if (worn && IS_SET(worn->GetFlag(), ITEM_FLAG_IRREMOVABLE))
			return false;
		// A bot with a rod or a pickaxe in the hand: the game refuses every
		// equip while it fishes, so a weapon better than its own was taken
		// for goods and went on the counter. Such a weapon is weighed against
		// the weapon the bot goes back to - the best of its bag - instead.
		if (wearCell == WEAR_WEAPON && item->GetType() == ITEM_WEAPON && worn &&
				IsPlayerBotToolType(worn->GetType()))   // MT2009_PLUS_BOT_HERBALIST_FIX_V1
		{
			if ((int)item->GetLevelLimit() > (int)ch->GetLevel())
				return false;
			long long bestScore = 0;
			LPITEM best = FindPlayerBotBestBagWeapon(ch, item, &bestScore);
			return !best || GetPlayerBotEquipmentScore(item, ch) > bestScore;
		}
		if (!PlayerBotCanEquipNow(ch, item, TItemPos(INVENTORY, cell)))
			return false;
		if (worn && PlayerBotOutdatedGearGivesWay(ch, item, worn))
			return false;
		return !worn || PlayerBotOutdatedGearGivesWay(ch, worn, item) ||
				GetPlayerBotEquipmentScore(item, ch) > GetPlayerBotEquipmentScore(worn, ch);
	}

	enum EPlayerBotPotionSupply
	{
		PLAYERBOT_POTION_SUPPLY_HP = 0,
		PLAYERBOT_POTION_SUPPLY_SP,
		PLAYERBOT_POTION_SUPPLY_GREEN,
		PLAYERBOT_POTION_SUPPLY_PURPLE,
		PLAYERBOT_POTION_SUPPLY_NONE
	};

	const DWORD PLAYERBOT_PERSONAL_GREEN_POTION_VNUM = 27101;
	const DWORD PLAYERBOT_PERSONAL_PURPLE_POTION_VNUM = 27104;
	const DWORD PLAYERBOT_PERSONAL_BUFF_POTION_KEEP = 200;

	bool IsPlayerBotPersonalBuffPotion(DWORD vnum)
	{
		return vnum == PLAYERBOT_PERSONAL_GREEN_POTION_VNUM ||
				vnum == PLAYERBOT_PERSONAL_PURPLE_POTION_VNUM;
	}

	EPlayerBotPotionSupply GetPlayerBotPotionSupply(DWORD vnum)
	{
		if (vnum == 27051 || (vnum >= 27001 && vnum <= 27003))
			return PLAYERBOT_POTION_SUPPLY_HP;
		if (vnum == 27052 || (vnum >= 27004 && vnum <= 27006))
			return PLAYERBOT_POTION_SUPPLY_SP;
		if (vnum == 27053 || (vnum >= 27100 && vnum <= 27102))
			return PLAYERBOT_POTION_SUPPLY_GREEN;
		if (vnum == 27054 || (vnum >= 27103 && vnum <= 27105))
			return PLAYERBOT_POTION_SUPPLY_PURPLE;
		return PLAYERBOT_POTION_SUPPLY_NONE;
	}

	DWORD GetPlayerBotPotionSupplyLimit(LPCHARACTER ch,
			EPlayerBotPotionSupply supply)
	{
		const bool lowLevel = !ch || ch->GetLevel() <= 10;
		const bool mage = ch && (ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA);
		switch (supply)
		{
			// A bot with yang in the bank should carry a real belt, not a token
			// one: potions are cheap next to what it earns, and running dry is
			// what sends it home in the middle of a good spot. These are also the
			// limits the excess-potion sale trims down to, so they have to move
			// together with the purchase below.
			case PLAYERBOT_POTION_SUPPLY_HP:     return lowLevel ? 160 : 800;
			case PLAYERBOT_POTION_SUPPLY_SP:     return lowLevel ? (mage ? 100 : 50) : 600;
			case PLAYERBOT_POTION_SUPPLY_GREEN:  return 30;
			case PLAYERBOT_POTION_SUPPLY_PURPLE: return 30;
			default: return 0;
		}
	}

	DWORD CountPlayerBotPotionSupply(LPCHARACTER ch,
			EPlayerBotPotionSupply supply)
	{
		if (!ch || supply == PLAYERBOT_POTION_SUPPLY_NONE)
			return 0;
		DWORD count = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && !IsPlayerBotSidekickHeld(ch, item) && GetPlayerBotPotionSupply(item->GetVnum()) == supply)
				count += item->GetCount();
		}
		return count;
	}

	DWORD CountPlayerBotMerchantTrimSupply(LPCHARACTER ch,
			EPlayerBotPotionSupply supply)
	{
		DWORD count = CountPlayerBotPotionSupply(ch, supply);
		if (supply == PLAYERBOT_POTION_SUPPLY_GREEN)
			count -= std::min<DWORD>(count,
					ch->CountSpecifyItem(PLAYERBOT_PERSONAL_GREEN_POTION_VNUM));
		else if (supply == PLAYERBOT_POTION_SUPPLY_PURPLE)
			count -= std::min<DWORD>(count,
					ch->CountSpecifyItem(PLAYERBOT_PERSONAL_PURPLE_POTION_VNUM));
		return count;
	}

	bool HasPlayerBotExcessPotions(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		if (ch->CountSpecifyItem(PLAYERBOT_PERSONAL_GREEN_POTION_VNUM) >
				PLAYERBOT_PERSONAL_BUFF_POTION_KEEP ||
			ch->CountSpecifyItem(PLAYERBOT_PERSONAL_PURPLE_POTION_VNUM) >
				PLAYERBOT_PERSONAL_BUFF_POTION_KEEP)
			return true;
		for (int supply = PLAYERBOT_POTION_SUPPLY_HP;
				supply < PLAYERBOT_POTION_SUPPLY_NONE; ++supply)
		{
			const EPlayerBotPotionSupply kind = (EPlayerBotPotionSupply)supply;
			if (CountPlayerBotMerchantTrimSupply(ch, kind) >
					GetPlayerBotPotionSupplyLimit(ch, kind))
				return true;
		}
		return false;
	}

	bool CanMergePlayerBotPotionStacks(LPITEM destination, LPITEM source)
	{
		if (!destination || !source || destination == source ||
				destination->GetVnum() != source->GetVnum() ||
				GetPlayerBotPotionSupply(destination->GetVnum()) == PLAYERBOT_POTION_SUPPLY_NONE ||
				!destination->IsStackable() || !source->IsStackable() ||
				IS_SET(destination->GetAntiFlag(), ITEM_ANTIFLAG_STACK) ||
				IS_SET(source->GetAntiFlag(), ITEM_ANTIFLAG_STACK) ||
				IsPlayerBotSidekickHeld(destination->GetOwner(), destination) || // MT2009_PLUS_SIDEKICK_SHOP_ERRAND_V1: not what a companion holds for its owner
				IsPlayerBotSidekickHeld(source->GetOwner(), source))
			return false;
		for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
			if (destination->GetSocket(socket) != source->GetSocket(socket))
				return false;
		for (int attr = 0; attr < ITEM_ATTRIBUTE_MAX_NUM; ++attr)
			if (destination->GetAttributeType(attr) != source->GetAttributeType(attr) ||
					destination->GetAttributeValue(attr) != source->GetAttributeValue(attr))
				return false;
		return true;
	}

	bool CompactPlayerBotPotionStacks(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		DWORD movedUnits = 0;
		DWORD removedStacks = 0;
		// Only a pass that frees a cell is worth its moves. A bot drinks from
		// the first stack of a kind, so a pass that topped the first stack up
		// from the last one ran again as soon as a few potions had gone: 29 000
		// passes an hour on one core of the test world, "freed_stacks=0" on
		// nearly every one, and every unit moved a save for the db core (23
		// September). Filling from the front leaves the fewest stacks there
		// can be, so a kind is poured only while its units would fit in fewer
		// stacks than it has from this one on.
		for (WORD destinationCell = 0; destinationCell < PLAYERBOT_BAG_CELLS; ++destinationCell)
		{
			LPITEM destination = ch->GetInventoryItem(destinationCell);
			if (!destination || IsPlayerBotSidekickHeld(ch, destination) ||
					GetPlayerBotPotionSupply(destination->GetVnum()) == PLAYERBOT_POTION_SUPPLY_NONE)
				continue;
			const DWORD maxStack = (DWORD)std::max(1, PlayerBotMaxStack(destination));
			if ((DWORD)destination->GetCount() >= maxStack)
				continue;
			DWORD units = (DWORD)destination->GetCount(), stacks = 1;
			for (WORD cell = destinationCell + 1; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM other = ch->GetInventoryItem(cell);
				if (!CanMergePlayerBotPotionStacks(destination, other))
					continue;
				units += (DWORD)other->GetCount();
				++stacks;
			}
			if (stacks <= (units + maxStack - 1) / maxStack)
				continue;
			for (WORD sourceCell = destinationCell + 1;
					sourceCell < PLAYERBOT_BAG_CELLS && (DWORD)destination->GetCount() < maxStack;
					++sourceCell)
			{
				LPITEM source = ch->GetInventoryItem(sourceCell);
				if (!CanMergePlayerBotPotionStacks(destination, source))
					continue;
				const DWORD sourceCount = source->GetCount();
				const DWORD transfer = std::min<DWORD>(maxStack - (DWORD)destination->GetCount(), sourceCount);
				if (transfer == 0)
					continue;
				destination->SetCount(destination->GetCount() + transfer);
				source->SetCount(sourceCount - transfer);
				movedUnits += transfer;
				if (transfer == sourceCount)
					++removedStacks;
			}
		}
		if (movedUnits > 0)
			sys_log(0, "PLAYERBOT_INVENTORY: compacted potions pid=%u name=%s moved=%u freed_stacks=%u",
					ch->GetPlayerID(), ch->GetName(), movedUnits, removedStacks);
		return movedUnits > 0;
	}

	bool SellPlayerBotExcessPotions(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		// Sell weaker variants first, while retaining a bounded combat/travel reserve.
		const DWORD saleOrder[] = {
			27051, 27001, 27002, 27003,
			27052, 27004, 27005, 27006,
			27053, 27100, 27101, 27102,
			27054, 27103, 27104, 27105
		};
		DWORD soldUnits = 0;
		long long earnedGold = 0;
		const DWORD personalVnums[] = {
			PLAYERBOT_PERSONAL_GREEN_POTION_VNUM,
			PLAYERBOT_PERSONAL_PURPLE_POTION_VNUM
		};
		for (size_t v = 0; v < sizeof(personalVnums) / sizeof(personalVnums[0]); ++v)
		{
			DWORD total = ch->CountSpecifyItem(personalVnums[v]);
			DWORD excess = total > PLAYERBOT_PERSONAL_BUFF_POTION_KEEP
					? total - PLAYERBOT_PERSONAL_BUFF_POTION_KEEP : 0;
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && excess > 0; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetVnum() != personalVnums[v] || IsPlayerBotSidekickHeld(ch, item))
					continue;
				const DWORD unitPrice = GetPlayerBotNpcSellUnitPrice(item);
				if (unitPrice == 0)
					continue;
				const DWORD count = std::min<DWORD>(excess, item->GetCount());
				item->SetCount(item->GetCount() - count);
				PlayerBotChangeGold(ch, (long long)unitPrice * count);
				excess -= count;
				soldUnits += count;
				earnedGold += (long long)unitPrice * count;
			}
		}
		for (int supply = PLAYERBOT_POTION_SUPPLY_HP;
				supply < PLAYERBOT_POTION_SUPPLY_NONE; ++supply)
		{
			const EPlayerBotPotionSupply kind = (EPlayerBotPotionSupply)supply;
			DWORD total = CountPlayerBotMerchantTrimSupply(ch, kind);
			const DWORD keep = GetPlayerBotPotionSupplyLimit(ch, kind);
			if (total <= keep)
				continue;
			DWORD excess = total - keep;
			for (size_t order = 0;
					order < sizeof(saleOrder) / sizeof(saleOrder[0]) && excess > 0; ++order)
			{
				if (IsPlayerBotPersonalBuffPotion(saleOrder[order]))
					continue;
				if (GetPlayerBotPotionSupply(saleOrder[order]) != kind)
					continue;
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && excess > 0; ++cell)
				{
					LPITEM item = ch->GetInventoryItem(cell);
					if (!item || item->GetVnum() != saleOrder[order] || IsPlayerBotSidekickHeld(ch, item))
						continue;
					const DWORD unitPrice = GetPlayerBotNpcSellUnitPrice(item);
					if (unitPrice == 0)
						continue;
					const DWORD count = std::min<DWORD>(excess, item->GetCount());
					item->SetCount(item->GetCount() - count);
					PlayerBotChangeGold(ch, (long long)unitPrice * count);
					excess -= count;
					soldUnits += count;
					earnedGold += (long long)unitPrice * count;
				}
			}
		}
		if (soldUnits > 0)
			sys_log(0, "PLAYERBOT_INVENTORY: sold excess potions pid=%u name=%s units=%u earned=%lld gold=%lld",
					ch->GetPlayerID(), ch->GetName(), soldUnits, earnedGold,
					(long long)ch->GetGold());
		return soldUnits > 0;
	}

	bool RaisePlayerBotEmergencyGold(LPCHARACTER ch, long long requiredGold,
			const char* reason)
	{
		if (!ch || ch->GetGold() >= requiredGold)
			return false;

		// The native NPC shop accepts potions too. Sell only as many surplus units
		// as are required to restore an essential weapon/ammunition purchase. Blue
		// potions go first and both HP/SP reserves remain protected.
		const DWORD potionVnums[] = {
			27004, 27005, 27006, 27052,
			27001, 27002, 27003, 27051
		};
		for (size_t v = 0; v < sizeof(potionVnums) / sizeof(potionVnums[0]); ++v)
		{
			const bool bluePotion = v < 4;
			const DWORD reserve = bluePotion ? 10 : 30;
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetVnum() != potionVnums[v] || item->GetCount() <= reserve ||
						IsPlayerBotSidekickHeld(ch, item))
					continue;

				const DWORD price = GetPlayerBotNpcSellUnitPrice(item);
				if (price == 0)
					continue;

				const long long deficit = requiredGold - ch->GetGold();
				const DWORD available = item->GetCount() - reserve;
				DWORD count = (DWORD)((deficit + price - 1) / price);
				count = std::max<DWORD>(1, std::min<DWORD>(count, available));
				item->SetCount(item->GetCount() - count);
				PlayerBotChangeGold(ch, (long long)price * count);
				sys_log(0, "PLAYERBOT_GEAR: emergency sale pid=%u name=%s reason=%s vnum=%u count=%u earned=%lld total_gold=%lld required=%lld",
						ch->GetPlayerID(), ch->GetName(), reason ? reason : "supply",
						potionVnums[v], count, (long long)price * count,
						(long long)ch->GetGold(), requiredGold);
				if (ch->GetGold() >= requiredGold)
					return true;
			}
		}
		return ch->GetGold() >= requiredGold;
	}

	bool BuyPlayerBotArrowsAtMerchant(LPCHARACTER ch)
	{
		if (!NeedsPlayerBotArrows(ch))
			return false;
		TItemTable* proto = ITEM_MANAGER::instance().GetTable(PLAYERBOT_WOODEN_ARROW_VNUM);
		if (!proto)
			return false;

		// One bundle, once: the quiver is never emptied, so this is a bot that
		// has no arrow at all - a new Archer, or one whose only arrows were of
		// a kind that cannot hit (GetPlayerBotArrowGrade) - and it is worth
		// selling potions for.
		const int bundle = PLAYERBOT_ARROW_SMALL_BUNDLE;
		const long long price = GetPlayerBotNpcPurchasePrice(proto, bundle);
		if (ch->GetGold() < price)
			RaisePlayerBotEmergencyGold(ch, price, "arrows");
		if (price <= 0 || ch->GetGold() < price)
			return false;
		// AutoGiveItem hands the item back even when it had nowhere to put it:
		// with no free cell the bundle goes on the ground at the bot's feet, the
		// bot pays, still "needs arrows", and buys again on the next pass - a
		// market square carpeted in Wooden Arrows, twenty purchases an hour per
		// archer. The junk sale has already run by now; a bag still full holds
		// things worth keeping, and the arrows wait for the next visit.
		if (ch->GetEmptyInventory(1) < 0)
		{
			sys_log(0, "PLAYERBOT_GEAR: no room for arrows pid=%u name=%s arrows=%d",
					ch->GetPlayerID(), ch->GetName(), CountPlayerBotArrows(ch));
			return false;
		}

		LPITEM arrows = ch->AutoGiveItem(
				PLAYERBOT_WOODEN_ARROW_VNUM, bundle, -1, false);
		if (!arrows)
			return false;
		PlayerBotChangeGold(ch, -price);
		const bool equipped = EnsurePlayerBotArrowsEquipped(ch);
		sys_log(0, "PLAYERBOT_GEAR: bought wooden arrows pid=%u name=%s vnum=%u count=%d price=%lld equipped=%d",
				ch->GetPlayerID(), ch->GetName(), PLAYERBOT_WOODEN_ARROW_VNUM,
				bundle, price, equipped ? 1 : 0);
		return true;
	}

	// A weapon over the low line for a bot of thirty whose hand has none to
	// take (Community Patch 5, point 2): the best a village merchant sells its
	// class at its level that the purse pays for - levels 25 to 36 there -
	// and not the level-one sword the empty hand was sold before. Surplus
	// potions are sold towards the cheapest of them, as for the emergency
	// weapon. A bag with no room for it is remembered
	// (NotePlayerBotProperWeaponRefused): the ban gives way for a while rather
	// than keep the bot unarmed.
	bool BuyPlayerBotProperWeapon(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded() || IsPlayerBotGearFrozen(ch))
			return false;
		std::vector<TPlayerBotMerchantWeapon> offers;
		CollectPlayerBotProperMerchantWeapons(ch, offers);
		if (offers.empty())
			return false;
		// The highest level first, then the heavier blow.
		std::sort(offers.begin(), offers.end(), [](const TPlayerBotMerchantWeapon& a, const TPlayerBotMerchantWeapon& b)
		{
			return a.iLevel != b.iLevel ? a.iLevel > b.iLevel : a.llRoll > b.llRoll;
		});
		long long cheapest = offers[0].llPrice;
		for (size_t i = 1; i < offers.size(); ++i)
			cheapest = std::min(cheapest, offers[i].llPrice);
		if ((long long)ch->GetGold() < cheapest)
			RaisePlayerBotEmergencyGold(ch, cheapest, "weapon_over_low_line");
		const TPlayerBotMerchantWeapon* pick = NULL;
		for (size_t i = 0; i < offers.size() && !pick; ++i)
			if ((long long)ch->GetGold() >= offers[i].llPrice)
				pick = &offers[i];
		if (!pick)
		{
			PlayerBotLogThrottled("proper_weapon_unpaid", get_dword_time(),
					"PLAYERBOT_AI: no yang for a weapon over the low line pid=%u name=%s level=%u gold=%lld cheapest=%lld",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), (long long)ch->GetGold(), cheapest);
			s_mapPlayerBotLowWeaponBan.erase(ch->GetPlayerID());
			return false;
		}
		// Room first: AutoGiveItem puts what the bag cannot take on the ground
		// and returns it (see the emergency weapon below).
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(pick->dwVnum);
		if (!proto || ch->GetEmptyInventory(std::max(1, (int)proto->bSize)) < 0)
		{
			PlayerBotLogThrottled("proper_weapon_no_room", get_dword_time(),
					"PLAYERBOT_AI: no room for a weapon over the low line pid=%u name=%s level=%u vnum=%u, the low one may go on",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), pick->dwVnum);
			NotePlayerBotProperWeaponRefused(ch);
			return false;
		}
		LPITEM weapon = ch->AutoGiveItem(pick->dwVnum, 1, -1, false);
		if (!weapon)
			return false;
		if (weapon->GetOwner() != ch || weapon->GetWindow() != INVENTORY)
		{
			sys_err("PLAYERBOT_AI: weapon over the low line did not reach the bag pid=%u name=%s vnum=%u",
					ch->GetPlayerID(), ch->GetName(), pick->dwVnum);
			return false;
		}
		PlayerBotChangeGold(ch, -pick->llPrice);
		LogManager::instance().ItemLog(ch, weapon, "PLAYERBOT_NPC_BUY", weapon->GetName());
		NotePlayerBotExplainOrigin(weapon, per::ORIGIN_NPC_PROPER_WEAPON, pick->llPrice);
		// The answer the ban gave a moment ago is the purse's and the bag's of
		// a moment ago.
		s_mapPlayerBotLowWeaponBan.erase(ch->GetPlayerID());
		LPITEM hand = ch->GetWear(WEAR_WEAPON);
		sys_log(0, "PLAYERBOT_AI: bought a weapon over the low line pid=%u name=%s level=%u vnum=%u weapon_level=%d price=%lld gold_left=%lld hand=%u",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), pick->dwVnum, pick->iLevel,
				pick->llPrice, (long long)ch->GetGold(), hand ? hand->GetVnum() : 0);
		return true;
	}

	// The weapon an empty hand takes from the bag: the best one it can put on
	// now, not the first. The first was how a bot of thirty whose weapon burned
	// came to fight with the level-one sword +6 lying ahead of its level-30
	// weapon in the bag; now that sword is not a candidate for it at all while
	// anything over the line can be had (IsPlayerBotBannedLowWeapon), and the
	// bag's best of the rest goes on.
	bool EquipFirstAvailablePlayerBotWeapon(LPCHARACTER ch)
	{
		if (!ch)
			return false;

		std::vector<std::pair<long long, LPITEM> > ranked;
		bool bannedLeft = false;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!IsPlayerBotWeapon(ch, item) || item->GetCell() != cell || IsPlayerBotSidekickUnwanted(ch, item))
				continue;
			if (IsPlayerBotBannedLowWeapon(ch, item))
			{
				bannedLeft = true;
				continue;
			}
			if (!PlayerBotCanEquipNow(ch, item, TItemPos(INVENTORY, cell)))
				continue;
			ranked.push_back(std::make_pair(GetPlayerBotEquipmentScore(item, ch), item));
		}
		std::stable_sort(ranked.begin(), ranked.end(),
				[](const std::pair<long long, LPITEM>& a, const std::pair<long long, LPITEM>& b)
				{
					return a.first > b.first;
				});
		for (size_t i = 0; i < ranked.size(); ++i)
		{
			LPITEM item = ranked[i].second;
			const DWORD vnum = item->GetVnum();
			const bool low = IsPlayerBotLowWeaponFor(ch, item);
			TPlayerBotEquipExplain explained;
			if (IsPlayerBotExplainOn() && !ch->GetWear(WEAR_WEAPON))
				PreparePlayerBotEquipExplain(explained, ch, WEAR_WEAPON, per::PATH_EMPTY_HAND,
						low ? per::RULE_EMPTY_HAND_LOW_FALLBACK : per::RULE_EMPTY_HAND_BEST, item, NULL, false);
			if (!PlayerBotEquipItem(ch, item))
				continue;
			if (item->IsEquipped())
				QueuePlayerBotEquip(explained);
			sys_log(0, "PLAYERBOT_AI: equipped weapon pid=%u name=%s vnum=%u",
					ch->GetPlayerID(), ch->GetName(), vnum);
			// A bot of thirty on the ban's way out: nothing over the line to
			// fight with and nothing to buy it with (IsLowWeaponFallback).
			if (low)
				PlayerBotLogThrottled("low_weapon_fallback", get_dword_time(),
						"PLAYERBOT_AI: low weapon worn, nothing over the line to be had pid=%u name=%s level=%u vnum=%u weapon_level=%d gold=%lld",
						ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), vnum,
						(int)item->GetLevelLimit(), (long long)ch->GetGold());
			return ch->GetWear(WEAR_WEAPON) != NULL;
		}
		if (bannedLeft)
			PlayerBotLogThrottled("low_weapon_banned", get_dword_time(),
					"PLAYERBOT_AI: low weapon left in the bag pid=%u name=%s level=%u gold=%lld rebuilding=%d",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), (long long)ch->GetGold(),
					NeedsPlayerBotProperWeapon(ch) ? 1 : 0);
		return false;
	}

	bool BuyPlayerBotEmergencyWeapon(LPCHARACTER ch)
	{
		if (!ch || ch->GetWear(WEAR_WEAPON))
			return ch && ch->GetWear(WEAR_WEAPON);
		// Nothing bought that could not be put on (IsPlayerBotGearFrozen).
		if (IsPlayerBotGearFrozen(ch))
			return false;

		// A weapon the bag already holds is put on, not bought a second time.
		// The weapon merchant bought for any empty hand, and a new bot's hand
		// is empty with its starter weapon or a chest's in the bag - twelve
		// purchases in five minutes on a world started that morning, and bots
		// of level one carrying four swords (Iwakura, 23 September). One the
		// engine refuses only for the moment (the second and a half after a
		// blow) is worn on the next try, so it stops the purchase too. The
		// low sword a bot of thirty may not fight with stops nothing.
		if (EquipFirstAvailablePlayerBotWeapon(ch))
			return true;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM held = ch->GetInventoryItem(cell);
			if (IsPlayerBotWeapon(ch, held) && held->CanUsedBy(ch) &&
					held->GetLevelLimit() <= ch->GetLevel() && !IsPlayerBotBannedLowWeapon(ch, held))
				return false;
		}

		// A bot of thirty buys a weapon over the low line (Community Patch 5,
		// point 2) - "zakupic nowa z wyzszym poziomem przed powrotem do walki".
		// When it cannot, the ban has given way by the time it is asked again
		// (no yang for the cheapest, or no room), and the bag's low weapon or
		// the old purchase below is what it fights with.
		if (IsPlayerBotLowWeaponBanned(ch, true))
		{
			if (BuyPlayerBotProperWeapon(ch) && EquipFirstAvailablePlayerBotWeapon(ch))
				return true;
			if (IsPlayerBotLowWeaponBanned(ch, true))
				return false;
			if (EquipFirstAvailablePlayerBotWeapon(ch))
				return true;
		}

		const DWORD vnum = GetPlayerBotEmergencyWeaponVnum(ch);
		const long long price = GetPlayerBotEmergencyWeaponPrice(ch);
		if (vnum == 0)
			return false;
		// Room first: AutoGiveItem puts what the bag cannot take on the ground
		// and returns it, and a weapon equipped from the ground is in the slot
		// and on the ground at once (IsPlayerBotWornItemSound).
		const TItemTable* weaponProto = ITEM_MANAGER::instance().GetTable(vnum);
		if (!weaponProto || ch->GetEmptyInventory(std::max(1, (int)weaponProto->bSize)) < 0)
			return false;
		if (ch->GetGold() < price)
			RaisePlayerBotEmergencyGold(ch, price, "weapon");
		if (ch->GetGold() < price)
			return false;

		LPITEM weapon = ch->AutoGiveItem(vnum, 1, -1, false);
		if (!weapon)
			return false;
		if (weapon->GetOwner() != ch || weapon->GetWindow() != INVENTORY)
		{
			sys_err("PLAYERBOT_AI: emergency weapon did not reach the bag pid=%u name=%s vnum=%u",
					ch->GetPlayerID(), ch->GetName(), vnum);
			return false;
		}

		PlayerBotChangeGold(ch, -price);
		LogManager::instance().ItemLog(ch, weapon, "PLAYERBOT_NPC_BUY", weapon->GetName());
		NotePlayerBotExplainOrigin(weapon, per::ORIGIN_NPC_EMERGENCY, price);
		TPlayerBotEquipExplain explained;
		if (IsPlayerBotExplainOn())
			PreparePlayerBotEquipExplain(explained, ch, WEAR_WEAPON, per::PATH_EMERGENCY_BUY,
					per::RULE_EMERGENCY_WEAPON, weapon, NULL, false);
		const bool equipped = PlayerBotEquipItem(ch, weapon);
		if (equipped && weapon->IsEquipped())
			QueuePlayerBotEquip(explained);

		sys_log(0, "PLAYERBOT_AI: bought emergency weapon pid=%u name=%s vnum=%u price=%lld equipped=%d",
				ch->GetPlayerID(), ch->GetName(), vnum, price, equipped ? 1 : 0);
		return equipped;
	}

	bool PrepareWeapon(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		// Transformed, the bot fights with the monster's attack and cannot put
		// anything on (IsPlayerBotGearFrozen): whatever is in the hand stays.
		if (IsPlayerBotGearFrozen(ch))
			return true;
		LPITEM equippedWeapon = ch->GetWear(WEAR_WEAPON);
		// PlayerBotWeaponFitsNow and not IsPlayerBotWeapon: the Archer's dagger
		// on a stone is the right weapon for the moment, not a profession
		// mismatch to be taken off. Nor is what a companion's owner put in its
		// hand, whatever the companion's path would choose.
		if (equippedWeapon && (PlayerBotWeaponFitsNow(ch, state, equippedWeapon) ||
				IsPlayerBotSidekickPinned(ch, equippedWeapon)))
		{
			state.dwEmergencyScavengeUntil = 0;
			if (equippedWeapon->GetSubType() == WEAPON_BOW)
				return EnsurePlayerBotArrowsEquipped(ch);
			return true;
		}
		if (equippedWeapon)
		{
			const DWORD wrongVnum = equippedWeapon->GetVnum();
			if (ch->GetEmptyInventory(equippedWeapon->GetSize()) >= 0)
			{
				TPlayerBotEquipExplain explained;
				if (IsPlayerBotExplainOn())
					PreparePlayerBotEquipExplain(explained, ch, WEAR_WEAPON, per::PATH_PROFESSION_OFF,
							per::RULE_PROFESSION_MISMATCH, NULL, equippedWeapon, false);
				ch->UnequipItem(equippedWeapon);
				if (!ch->GetWear(WEAR_WEAPON))
					QueuePlayerBotEquip(explained);
				sys_log(0, "PLAYERBOT_AI: unequipped profession-incompatible weapon pid=%u name=%s vnum=%u group=%u",
						ch->GetPlayerID(), ch->GetName(), wrongVnum, ch->GetSkillGroup());
			}
			if (ch->GetWear(WEAR_WEAPON))
				return false;
		}

		if (!ch->IsItemLoaded() || dwNow < state.dwNextGearAttemptTime)
			return false;

		state.dwNextGearAttemptTime = dwNow + PLAYERBOT_GEAR_RETRY_INTERVAL;

		if (EquipFirstAvailablePlayerBotWeapon(ch))
		{
			state.dwEmergencyScavengeUntil = 0;
			LPITEM weapon = ch->GetWear(WEAR_WEAPON);
			return weapon && (weapon->GetSubType() != WEAPON_BOW ||
					EnsurePlayerBotArrowsEquipped(ch));
		}

		const DWORD dwStarterChestVnum = GetStarterChestVnum(ch->GetJob());
		// With the world's apprentice chest off no chest of the chain is opened
		// (the progression pass takes them out of the bag), a weapon's want
		// included: the merchant trip below is the way to a weapon then.
		const bool bApprenticeChestOff = IsPlayerBotApprenticeChestOff();
		for (WORD cell = 0; !bApprenticeChestOff && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;

			const DWORD chestVnum = item->GetVnum();
			const bool classStarterChest = (dwStarterChestVnum != 0 && chestVnum == dwStarterChestVnum);
			const bool progressionChest = (chestVnum >= 50187 && chestVnum <= 50196);
			const int progressionLevel = (chestVnum == 50187) ? 1 : (int)(chestVnum - 50187) * 10;
			if (!classStarterChest && (!progressionChest || ch->GetLevel() < progressionLevel))
				continue;
			// This runs every second while the bot has no weapon, so a box the
			// engine refused waits out the chest pass's retry clock instead of
			// being asked for again on the next one.
			if (IsPlayerBotChestRefused(ch->GetPlayerID(), chestVnum, dwNow))
				continue;

			sys_log(0, "PLAYERBOT_AI: opening weapon recovery chest pid=%u name=%s vnum=%u cell=%u",
					ch->GetPlayerID(), ch->GetName(), chestVnum, cell);
			if (!ch->UseItem(TItemPos(INVENTORY, cell)))
			{
				NotePlayerBotChestRefused(ch->GetPlayerID(), chestVnum, dwNow);
				continue;
			}
			if (!EquipFirstAvailablePlayerBotWeapon(ch))
				return false;
			LPITEM weapon = ch->GetWear(WEAR_WEAPON);
			return weapon && (weapon->GetSubType() != WEAPON_BOW ||
					EnsurePlayerBotArrowsEquipped(ch));
		}

		// No remote purchase or free fallback.  The update loop first gives the
		// bot a chance to collect an owned weapon drop and then starts a visible
		// trip to the real Weapon Merchant.  Archer arrows follow the same rule.
		if (dwNow >= state.dwNextGearLogTime)
		{
			state.dwNextGearLogTime = dwNow + PLAYERBOT_GEAR_LOG_INTERVAL;
			sys_err("PLAYERBOT_AI: idle without weapon pid=%u name=%s expected_chest=%u",
					ch->GetPlayerID(), ch->GetName(), dwStarterChestVnum);
		}

		return false;
	}

	// Whether the bag can take everything a chest may hand out, judged the way
	// the engine places items: a piece needs its height in one column of one
	// page, a stackable merges into a stack of the same vnum first, and every
	// reward is placed on a copy of the grid before the next one is asked.
	//
	// The engine gives a chest's rewards one by one through AutoGiveItem, which
	// puts what does not fit on the ground and still reports success - so
	// "GetEmptyInventory(3) >= 0" before the chest let a full bag spill the
	// rest of the set (D01 of the 10 September audit: "przedmioty ze skrzyn
	// wypadaja na ziemie"). The set is the group's own list: for a PCT group
	// every line may come at once (the starter chests are that), for the others
	// exactly one line does, so the room asked for is the largest line. mt2009
	// exposes the type (GetGroupType, added by playerbotify.py) and the lines;
	// r40250 exposes neither, so that line keeps the old five-cell heuristic,
	// as does a group the manager does not know.
	bool PlayerBotBagTakesGroup(LPCHARACTER ch, DWORD dwGroupVnum, int& iCellsNeeded)
	{
		iCellsNeeded = 0;
		if (!ch || !ch->IsItemLoaded())
			return false;
		const CSpecialItemGroup* pGroup = ITEM_MANAGER::instance().GetSpecialItemGroup(dwGroupVnum);
#if !defined(PLAYERBOT_ENGINE_MT2009)
		// r40250's CSpecialItemGroup has neither GetItems() nor a size, so its
		// lines cannot be walked from here; that line keeps the old heuristic.
		pGroup = NULL;
#endif
		if (!pGroup)
		{
			// The grid, not the pointers: see CountPlayerBotFreeInventoryCells
			// (defined later in the include order, hence the loop repeated).
			int freeCells = 0;
			for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
				if (ch->IsEmptyItemGrid(TItemPos(INVENTORY, (WORD)cell), 1))
					++freeCells;
			return freeCells >= PLAYERBOT_CHEST_FREE_CELLS && ch->GetEmptyInventory(3) >= 0;
		}
#if defined(PLAYERBOT_ENGINE_MT2009)

		bool occupied[PLAYERBOT_BAG_CELLS];
		std::map<DWORD, int> headroom; // vnum -> units a stack of it can still take
		for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			occupied[cell] = false;
		for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM held = ch->GetInventoryItem(cell);
			if (!held || held->GetCell() != cell)
				continue;
			const int size = std::max<int>(1, held->GetSize());
			for (int k = 0; k < size; ++k)
				if (cell + k * 5 < PLAYERBOT_BAG_CELLS)
					occupied[cell + k * 5] = true;
			if (held->IsStackable() && held->GetCount() < ITEM_MAX_COUNT)
				headroom[held->GetVnum()] += ITEM_MAX_COUNT - held->GetCount();
		}

		const std::vector<CSpecialItemGroup::CSpecialItemInfo> lines = pGroup->GetItems();
		const bool everyLine = pGroup->GetGroupType() == CSpecialItemGroup::PCT;
		// The rewards to place: (size, units) per line, or one line - the one
		// that needs the most cells - when only one comes.
		std::vector<std::pair<int, int> > rewards;
		int biggestCells = -1;
		std::pair<int, int> biggest(0, 0);
		for (size_t i = 0; i < lines.size(); ++i)
		{
			const TItemTable* table = ITEM_MANAGER::instance().GetTable(lines[i].vnum);
			if (!table)
				continue;
			int units = std::max(1, lines[i].count);
			const bool stackable = IS_SET(table->dwFlags, ITEM_FLAG_STACKABLE);
			if (stackable)
			{
				std::map<DWORD, int>::iterator room = headroom.find(lines[i].vnum);
				if (room != headroom.end())
				{
					const int merged = std::min(room->second, units);
					units -= merged;
					if (everyLine)
						room->second -= merged;
				}
				if (units <= 0)
					continue;
				units = (units + ITEM_MAX_COUNT - 1) / ITEM_MAX_COUNT; // stacks to place
			}
			const int size = std::max<int>(1, table->bSize);
			if (everyLine)
				rewards.push_back(std::make_pair(size, units));
			else if (size * units > biggestCells)
			{
				biggestCells = size * units;
				biggest = std::make_pair(size, units);
			}
		}
		if (!everyLine && biggestCells > 0)
			rewards.push_back(biggest);

		const int rowsPerPage = PLAYERBOT_INVENTORY_PAGE_SIZE / 5;
		for (size_t r = 0; r < rewards.size(); ++r)
		{
			const int size = rewards[r].first;
			for (int n = 0; n < rewards[r].second; ++n)
			{
				int placed = -1;
				for (int cell = 0; cell < PLAYERBOT_BAG_CELLS && placed < 0; ++cell)
				{
					const int row = (cell % PLAYERBOT_INVENTORY_PAGE_SIZE) / 5;
					if (row + size > rowsPerPage)
						continue;
					bool free = true;
					for (int k = 0; k < size && free; ++k)
						if (occupied[cell + k * 5])
							free = false;
					if (free)
						placed = cell;
				}
				if (placed < 0)
					return false;
				for (int k = 0; k < size; ++k)
					occupied[placed + k * 5] = true;
				iCellsNeeded += size;
			}
		}
		return true;
#endif
	}

	bool ManagePlayerBotProgressionChests(LPCHARACTER ch,
			TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || ch->IsDead() ||
			dwNow < state.dwNextProgressionChestCheckTime)
			return false;
		state.dwNextProgressionChestCheckTime = dwNow + 10000 +
				(PlayerBotNavHash(ch->GetPlayerID()) % 5001U);

		// The world's apprentice chest off (IsPlayerBotApprenticeChestOff): a
		// bot keeps no chest of the chain and opens none. Every one in the bag
		// is taken out through the engine, which is what makes it stay out - a
		// DELETE on a running world is written back from the db core's cache
		// (some twenty-five of the 3 510 seban latino took out by hand came
		// back, 28 September) - and the migrator sweeps the bags of the bots
		// that are not in the world before the cores start. A companion only
		// stops opening them: its bag is its owner's as well, and a chest in
		// it may be the owner's.
		if (IsPlayerBotApprenticeChestOff())
		{
			if (IsPlayerBotSidekickPID(ch->GetPlayerID()))
				return false;
			DWORD takenUnits = 0;
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetCell() != cell || !IsPlayerBotApprenticeChestVnum(item->GetVnum()) ||
						item->isLocked() || item->IsExchanging())
					continue;
				takenUnits += std::max<DWORD>(1, item->GetCount());
				ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_APPRENTICE_CHEST_OFF");
			}
			if (takenUnits > 0)
				PlayerBotLogThrottled("apprentice_chest_off", dwNow,
						"PLAYERBOT_GEAR: apprentice chest is off, taken out of the bag pid=%u name=%s units=%u",
						ch->GetPlayerID(), ch->GetName(), takenUnits);
			return false;
		}

		// The seed historically supplied one starter chest and the stock
		// give_basic_weapon quest supplied another on first login. Since every
		// apprentice chest contains the next tier, that duplicated the entire
		// progression chain. These boxes are one-per-character rewards: retain one
		// copy of each tier and remove only the artificial duplicates.
		std::map<DWORD, bool> seenProgressionChests;
		DWORD removedChestUnits = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;
			const DWORD vnum = item->GetVnum();
			if (!IsPlayerBotApprenticeChestVnum(vnum))
				continue;

			const DWORD count = std::max<DWORD>(1, item->GetCount());
			if (seenProgressionChests.find(vnum) != seenProgressionChests.end())
			{
				removedChestUnits += count;
				ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_DUPLICATE_CHEST");
				continue;
			}

			seenProgressionChests[vnum] = true;
			if (count > 1)
			{
				removedChestUnits += count - 1;
				item->SetCount(1);
			}
		}
		if (removedChestUnits > 0)
			sys_log(0, "PLAYERBOT_GEAR: removed duplicate progression chests pid=%u name=%s units=%u",
					ch->GetPlayerID(), ch->GetName(), removedChestUnits);

		LPCHARACTER target = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		if ((target && !target->IsDead()) ||
				(state.dwLastCombatActionTime != 0 &&
				 dwNow - state.dwLastCombatActionTime < 3000))
			return false;

		const DWORD starterVnum = GetStarterChestVnum(ch->GetJob());
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item)
				continue;
			const DWORD chestVnum = item->GetVnum();
			const bool classStarter = starterVnum != 0 && chestVnum == starterVnum;
			const bool progression = chestVnum >= 50187 && chestVnum <= 50196;
			const int requiredLevel = chestVnum == 50187
					? 1 : (int)(chestVnum - 50187) * 10;
			if (!classStarter && (!progression || ch->GetLevel() < requiredLevel))
				continue;
			// A box the engine refused this bot waits out the chest pass's retry
			// clock. This pass comes back every ten to fifteen seconds and used to
			// remember nothing, so the lv60 chest's Skrzynia Mistrza II, whose
			// group the mt2009 share did not have, was asked for by every bot of
			// seventy holding one: 258 175 lines of "cannot find special item
			// group 50194" in thirty hours on one core, 560 a minute by the end.
			if (IsPlayerBotChestRefused(ch->GetPlayerID(), chestVnum, dwNow))
				continue;

			// The whole set or nothing: a chest whose rewards would spill stays
			// closed until the town errand for a full bag has made room.
			int cellsNeeded = 0;
			if (!PlayerBotBagTakesGroup(ch, chestVnum, cellsNeeded))
			{
				if (dwNow >= state.dwNextBagFullLogTime)
				{
					state.dwNextBagFullLogTime = dwNow + 60000;
					sys_log(0, "PLAYERBOT_GEAR: chest waits for room pid=%u name=%s vnum=%u level=%u",
							ch->GetPlayerID(), ch->GetName(), chestVnum, ch->GetLevel());
				}
				continue;
			}

			// A giftbox opens only into a free column of three (UseItemEx asks
			// GetEmptyInventory(3)), and 16 of the 23 bags looked at that refused
			// Skrzynia Mistrza II had free cells and no such column. The chest pass
			// makes one; this pass asks for it the same way, and uses the chest by
			// its own cell afterwards, since a single-cell chest may be what moved.
			if (ch->GetEmptyInventory(3) < 0)
				FreePlayerBotGiftboxColumn(ch);
			if (!ch->UseItem(TItemPos(INVENTORY, item->GetCell())))
			{
				// group=0 is a box the share gives no group, room3=0 the engine's
				// own refusal for want of a three-cell space; anything else was a
				// refusal of the moment (a busy action, the item-use pulse).
				NotePlayerBotChestRefused(ch->GetPlayerID(), chestVnum, dwNow);
				PlayerBotLogThrottled("progression_chest_refused", dwNow,
						"PLAYERBOT_CHEST: progression chest refused pid=%u name=%s vnum=%u level=%u group=%d room3=%d",
						ch->GetPlayerID(), ch->GetName(), chestVnum, ch->GetLevel(),
						ITEM_MANAGER::instance().GetSpecialItemGroup(chestVnum) ? 1 : 0,
						ch->GetEmptyInventory(3) >= 0 ? 1 : 0);
				continue;
			}
			state.dwNextEquipmentCheckTime = 0;
			state.bEquipPending = true;
			state.dwNextGearAttemptTime = 0;
			sys_log(0, "PLAYERBOT_GEAR: opened progression chest pid=%u name=%s vnum=%u level=%u",
					ch->GetPlayerID(), ch->GetName(), chestVnum, ch->GetLevel());
			return true;
		}
		return false;
	}

	// The threshold is the caller's (a war drinks sooner), and the healing a
	// potion already under way will bring counts: the engine refuses a use
	// that it would carry past the top anyway.
	bool UseHealthPotion(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			int hpPercent = PLAYERBOT_POTION_HP_PERCENT)
	{
		// MT2009_PLUS_LEGENDS_V1 (potions): a Specjalny and up drinks sooner in
		// a fight with a person (the tier's health, when it is higher).
		if (GetPlayerBotLegendTierOf(ch) >= BOT_LEGEND_SPECIAL && IsPlayerBotFightingPerson(ch, state))
			hpPercent = GetPlayerBotLegendPvpPotionPercent(ch, hpPercent);
		if (ch->GetMaxHP() <= 0 ||
				((long long)ch->GetHP() + ch->GetPoint(POINT_HP_RECOVERY)) * 100 >
					(long long)ch->GetMaxHP() * hpPercent)
			return false;

		// A duel is fought without drinking. The operator's rule, and the only
		// thing that makes a bot-against-bot fight worth watching: two bots with
		// full bags of red potions do not have a fight, they have an endurance
		// test. Asked before the clock below, so a duel does not spend the
		// bot's next potion attempt either.
		if (playerbot_pvp::IsInDuel(ch->GetPlayerID(), dwNow))
			return false;

		if (dwNow < state.dwNextPotionTime)
			return false;

		state.dwNextPotionTime = dwNow + PLAYERBOT_POTION_INTERVAL;

		// 27051 is the beginner red potion supplied by the level-one chest.
		// 71018 and 71020 are the chest's Blessings of Life and of the Dragon: a
		// full restore each, drunk last, when the ordinary reds have run out.
		// ... and the grilled fish that heal: Crucian, Big Crucian, Tenchi.
		const DWORD redPotionVnums[] = { 27051, 27001, 27002, 27003, 71018, 71020, 27863, 27865, 27875 };
		for (size_t potionIndex = 0; potionIndex < sizeof(redPotionVnums) / sizeof(redPotionVnums[0]); ++potionIndex)
		{
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetVnum() != redPotionVnums[potionIndex] || IsPlayerBotSidekickHeld(ch, item))
					continue;

				const DWORD potionVnum = item->GetVnum();
				if (ch->UseItem(TItemPos(INVENTORY, cell)))
				{
					sys_log(0, "PLAYERBOT_AI: used health potion pid=%u name=%s vnum=%u hp=%d/%d",
							ch->GetPlayerID(), ch->GetName(), potionVnum, ch->GetHP(), ch->GetMaxHP());
					return true;
				}
			}
		}

		if (dwNow >= state.dwNextPotionLogTime)
		{
			state.dwNextPotionLogTime = dwNow + PLAYERBOT_POTION_LOG_INTERVAL;
			sys_log(0, "PLAYERBOT_AI: no usable health potion pid=%u name=%s hp=%d/%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetHP(), ch->GetMaxHP());
		}

		return false;
	}

	bool UseManaPotion(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow,
			int spPercent = PLAYERBOT_POTION_SP_PERCENT)
	{
		if (ch->GetMaxSP() <= 0 ||
				((long long)ch->GetSP() + ch->GetPoint(POINT_SP_RECOVERY)) * 100 >
					(long long)ch->GetMaxSP() * spPercent)
			return false;

		if (dwNow < state.dwNextManaPotionTime)
			return false;

		state.dwNextManaPotionTime = dwNow + PLAYERBOT_POTION_INTERVAL;

		// 27052 is the beginner blue potion, followed by standard small, medium, large blue potions.
		const DWORD bluePotionVnums[] = { 27052, 27004, 27005, 27006, 71020, 27864, 27876 };
		for (size_t potionIndex = 0; potionIndex < sizeof(bluePotionVnums) / sizeof(bluePotionVnums[0]); ++potionIndex)
		{
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetVnum() != bluePotionVnums[potionIndex] || IsPlayerBotSidekickHeld(ch, item))
					continue;

				const DWORD potionVnum = item->GetVnum();
				if (ch->UseItem(TItemPos(INVENTORY, cell)))
				{
					sys_log(0, "PLAYERBOT_AI: used mana potion pid=%u name=%s vnum=%u sp=%d/%d",
							ch->GetPlayerID(), ch->GetName(), potionVnum, ch->GetSP(), ch->GetMaxSP());
					return true;
				}
			}
		}

		return false;
	}

	bool UseUtilityPotions(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return false;
		LPCHARACTER target = state.dwTargetVID != 0
				? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		const bool activeCombat = target && !target->IsDead() &&
				(target->IsMonster() || target->IsStone());
		const bool importantFight = activeCombat && (target->IsStone() ||
				(target->IsMonster() && target->GetMobRank() >= MOB_RANK_BOSS));
		// One third of ordinary grinders plans a longer session and uses attack-speed
		// potions as well. Every bot uses them for Metins/bosses, but nobody drinks
		// one merely while waiting at an NPC or recovering from death.
		const bool longGrindingSession = activeCombat && target->IsMonster() &&
				state.bLongTermGoal == BOT_GOAL_LEVEL_UP &&
				(PlayerBotNavHash(ch->GetPlayerID() ^ 0x47524545U) % 3U) == 0;
		const bool shouldUseGreen = !state.bVisitingShop &&
				!state.bRecoveringAfterDeath && !state.bTacticalRetreat &&
				(importantFight || longGrindingSession);

		// 1. Green Potion (Zielona Mikstura - Attack Speed)
		if (shouldUseGreen && ch->FindAffect(AFFECT_ATT_SPEED) == NULL &&
				state.mapBuffActiveUntil[27102] <= dwNow)
		{
			const DWORD greenPotionVnums[] = { 27102, 27101, 27100, 27053 };
			for (size_t i = 0; i < sizeof(greenPotionVnums) / sizeof(greenPotionVnums[0]); ++i)
			{
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
				{
					LPITEM item = ch->GetInventoryItem(cell);
					if (!item || item->GetVnum() != greenPotionVnums[i] || IsPlayerBotSidekickHeld(ch, item))
						continue;

					const DWORD potionVnum = item->GetVnum();
					if (ch->UseItem(TItemPos(INVENTORY, cell)))
					{
						// FindAffect is authoritative for the real item duration. This short
						// guard only prevents a broken proto from being consumed every tick.
						state.mapBuffActiveUntil[27102] = dwNow + 30000;
						sys_log(0, "PLAYERBOT_AI: used green potion pid=%u name=%s vnum=%u",
								ch->GetPlayerID(), ch->GetName(), potionVnum);
						return true;
					}
				}
			}
		}

		// 2. Purple Potion (Fioletowa Mikstura - Movement Speed). Use it for travel,
		// loot runs and the approach to a distant target, not while standing at NPCs.
		const bool shouldUsePurple = !state.bVisitingShop &&
				!state.bRecoveringAfterDeath && !state.bTacticalRetreat &&
				(!activeCombat || DISTANCE_APPROX(ch->GetX() - target->GetX(),
					target->GetY() - ch->GetY()) > 500);
		if (shouldUsePurple && ch->FindAffect(AFFECT_MOV_SPEED) == NULL &&
				state.mapBuffActiveUntil[27105] <= dwNow)
		{
			const DWORD purplePotionVnums[] = { 27105, 27104, 27103, 27054 };
			for (size_t i = 0; i < sizeof(purplePotionVnums) / sizeof(purplePotionVnums[0]); ++i)
			{
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
				{
					LPITEM item = ch->GetInventoryItem(cell);
					if (!item || item->GetVnum() != purplePotionVnums[i] || IsPlayerBotSidekickHeld(ch, item))
						continue;

					const DWORD potionVnum = item->GetVnum();
					if (ch->UseItem(TItemPos(INVENTORY, cell)))
					{
						state.mapBuffActiveUntil[27105] = dwNow + 30000;
						sys_log(0, "PLAYERBOT_AI: used purple potion pid=%u name=%s vnum=%u",
								ch->GetPlayerID(), ch->GetName(), potionVnum);
						return true;
					}
				}
			}
		}

		return false;
	}

	// Every bot wears the third hand, and keeps wearing it.
	//
	// Without it a kill's yang lands on the ground as coin piles and the bot has
	// to walk to each one; with it the engine credits the money on the spot
	// (CHARACTER::RewardGold, IsEquipUniqueGroup(UNIQUE_GROUP_AUTOLOOT)). The
	// bot spends its ticks fighting rather than fetching, and the hunting
	// grounds stop filling with yang nobody collects.
	//
	// Nothing here is a purchase: the item is made, worn, and its wear clock
	// wound back up before it can run out. See PLAYERBOT_THIRD_HAND_VNUM for
	// why that clock has to be touched at all, and why it is 72018 and not the
	// 71010 an item shop would sell.
	void ManagePlayerBotThirdHand(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || ch->IsDead())
			return;
		if (dwNow < state.dwNextThirdHandTime)
			return;
		state.dwNextThirdHandTime = dwNow + PLAYERBOT_THIRD_HAND_INTERVAL;

		// Since patch 0010 every kill's yang goes straight to the purse, for
		// bots and players alike, so the Third Hand only takes a slot ("da sie
		// dodac status trzeciej reki bez zajmowania slota w eq?"). Whatever a
		// bot still wears or carries of the group is taken away.
		for (int pass = 0; pass < 2; ++pass)
		{
			LPITEM hand = NULL;
			for (BYTE wear = 0; wear < WEAR_MAX_NUM && !hand; ++wear)
			{
				LPITEM worn = ch->GetWear(wear);
				if (worn && worn->GetVnum() >= PLAYERBOT_THIRD_HAND_VNUM_FIRST &&
						worn->GetVnum() <= PLAYERBOT_THIRD_HAND_VNUM)
					hand = worn;
			}
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && !hand; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (item && item->GetVnum() >= PLAYERBOT_THIRD_HAND_VNUM_FIRST &&
						item->GetVnum() <= PLAYERBOT_THIRD_HAND_VNUM && !item->isLocked())
					hand = item;
			}
			if (!hand)
				return;
			sys_log(0, "PLAYERBOT_GEAR: third hand retired pid=%u name=%s vnum=%u worn=%d",
					ch->GetPlayerID(), ch->GetName(), hand->GetVnum(), hand->IsEquipped() ? 1 : 0);
			ITEM_MANAGER::instance().RemoveItem(hand, "PLAYERBOT_THIRD_HAND_RETIRED");
		}
	}
}

#endif
