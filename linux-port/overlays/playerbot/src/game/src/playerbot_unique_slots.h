#ifndef __INC_METIN2_PLAYERBOT_UNIQUE_SLOTS_H__
#define __INC_METIN2_PLAYERBOT_UNIQUE_SLOTS_H__

// The two unique slots: what a bot never wears there, and the rings and gloves
// it wears only while it hunts.
//
// The equipment pass fills an empty unique slot with any unique in the bag - a
// unique with no line on it scores nothing, and nothing beats an empty slot - so
// it put on whatever came to hand, the ring that hides the level included. It
// leaves both kinds alone now (IsPlayerBotEquipmentCandidate, and the worn one it
// will not displace), and this pass decides for them.
//
// A ring or a glove is worth wearing only while it pays: its minutes run while
// it is worn and a town pays nothing. So it goes on after a blow on a hunting
// map and comes off in a safe zone, on an errand, at the water or the vein,
// behind a counter, in a duel and after PLAYERBOT_TIMED_UNIQUE_IDLE_MS without a
// blow - one change a pass, on its own clock, never inside the engine's swing
// window. Included after playerbot_mining.h, which says who is at a vein.

namespace
{
	// MT2009_PLUS_BOT_RANK_GLOVE_V1: a bot at a negative rank that cannot stand
	// it out in town - no Fasolka Zen in the bag, none it can afford on the
	// stalls, or none bought in PLAYERBOT_NEGATIVE_RANK_TOWN_PATIENCE_MS of
	// waiting - hunts it back instead, wearing the Prophecy King's Glove
	// (KeepPlayerBotNegativeRankInTown decides, playerbot_manager.cpp). It
	// stood in town for good: time outside a safe zone and kills are the only
	// other ways up, and the ring gives neither. dwHoldSince/iHoldRank are the
	// wait in town and the rank it began at, dwHuntUntil the hunt's end, after
	// which the town is asked again.
	struct TPlayerBotRankRecovery
	{
		DWORD dwHoldSince;
		int iHoldRank;
		DWORD dwHuntUntil;
		TPlayerBotRankRecovery() : dwHoldSince(0), iHoldRank(0), dwHuntUntil(0) {}
	};
	std::map<DWORD, TPlayerBotRankRecovery> s_mapPlayerBotRankRecovery;

	bool IsPlayerBotRankHunting(LPCHARACTER ch, DWORD dwNow)
	{
		if (!ch || ch->GetRealAlignment() >= 0)
			return false;
		std::map<DWORD, TPlayerBotRankRecovery>::const_iterator it =
				s_mapPlayerBotRankRecovery.find(ch->GetPlayerID());
		return it != s_mapPlayerBotRankRecovery.end() && it->second.dwHuntUntil != 0 &&
				dwNow < it->second.dwHuntUntil;
	}

	// MT2009_PLUS_BOT_RANK_GLOVE_V1: what holds its unique slot against a ring
	// or a thief's glove looking for one - a ring or glove on its clock, and
	// the Prophecy King's Glove or Symbol while the rank is below zero.
	bool IsPlayerBotUniqueSlotHeld(LPCHARACTER ch, DWORD vnum)
	{
		return IsPlayerBotTimedUnique(vnum) ||
				(IsPlayerBotRankUnique(vnum) && ch && ch->GetRealAlignment() < 0);
	}

	const char* GetPlayerBotUniqueIdleReason(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()))
			return "town";
		if (state.bVisitingShop || state.bVisitingBiologist || state.bVisitingStable || state.bMarketTrip)
			return "errand";
		if (state.bFishingSession || IsPlayerBotMiningNow(ch->GetPlayerID(), dwNow))
			return "session";
		if (ch->GetMyShop())
			return "stall";
		if (playerbot_pvp::IsInDuel(ch->GetPlayerID(), dwNow))
			return "duel";
		if (state.dwLastCombatActionTime == 0 ||
				dwNow - state.dwLastCombatActionTime > PLAYERBOT_TIMED_UNIQUE_IDLE_MS)
			return "idle";
		return NULL;
	}

	// A dropper held at its level earns nothing from a ring.
	bool IsPlayerBotExpLocked(LPCHARACTER ch)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		return ch && ch->FindAffect(AFFECT_EXP_BLOCK) != NULL;
#else
		return false;
#endif
	}

	// CHARACTER::EquipItem refuses a change of gear this soon after a swing or a
	// cast; the equipment pass waits for the same window.
	bool IsPlayerBotUniqueSwapWindowOpen(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD dwNow)
	{
		(void) dwNow;
		return !IsPlayerBotEquipWindowShut(ch, state);
	}

	bool IsPlayerBotWearingTimedUnique(LPCHARACTER ch, bool ring)
	{
		for (int wear = WEAR_UNIQUE1; wear <= WEAR_UNIQUE2; ++wear)
		{
			LPITEM worn = ch->GetWear(wear);
			if (worn && (ring ? IsPlayerBotExpRing(worn->GetVnum()) : IsPlayerBotThiefGlove(worn->GetVnum())))
				return true;
		}
		return false;
	}

	// MT2009_PLUS_BOT_RANK_GLOVE_V1: the Prophecy King's Glove or Symbol in
	// the bag, or NULL.
	LPITEM FindPlayerBotBagRankUnique(LPCHARACTER ch, DWORD vnum, WORD& outCell)
	{
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetType() != ITEM_UNIQUE || item->GetVnum() != vnum ||
					item->isLocked() || item->IsExchanging() || IsPlayerBotSidekickUnwanted(ch, item))
				continue;
			outCell = cell;
			return item;
		}
		return NULL;
	}

	bool IsPlayerBotWearingVnum(LPCHARACTER ch, DWORD vnum)
	{
		for (int wear = WEAR_UNIQUE1; wear <= WEAR_UNIQUE2; ++wear)
		{
			LPITEM worn = ch->GetWear(wear);
			if (worn && worn->GetVnum() == vnum)
				return true;
		}
		return false;
	}

	LPITEM FindPlayerBotBagTimedUnique(LPCHARACTER ch, bool ring, WORD& outCell)
	{
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetType() != ITEM_UNIQUE || item->isLocked() || item->IsExchanging() ||
					IsPlayerBotSidekickUnwanted(ch, item))
				continue;
			if (ring ? IsPlayerBotExpRing(item->GetVnum()) : IsPlayerBotThiefGlove(item->GetVnum()))
			{
				outCell = cell;
				return item;
			}
		}
		return NULL;
	}

	void ManagePlayerBotUniqueSlots(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		static std::map<DWORD, DWORD> s_mapPlayerBotUniqueSlotNext;
		if (!ch || !ch->IsItemLoaded() || ch->IsDead() || IsPlayerBotGearFrozen(ch))
			return;
		DWORD& next = s_mapPlayerBotUniqueSlotNext[ch->GetPlayerID()];
		if (dwNow < next)
			return;
		next = dwNow + PLAYERBOT_TIMED_UNIQUE_INTERVAL;

		const char* idle = GetPlayerBotUniqueIdleReason(ch, state, dwNow);
		// Off first: a unique never worn wherever it is found, a ring or a glove
		// whenever the bot is not hunting.
		for (int wear = WEAR_UNIQUE1; wear <= WEAR_UNIQUE2; ++wear)
		{
			LPITEM worn = ch->GetWear(wear);
			if (!worn || !IsPlayerBotWornItemSound(ch, worn, wear) ||
					IS_SET(worn->GetFlag(), ITEM_FLAG_IRREMOVABLE) || IsPlayerBotSidekickPinned(ch, worn))
				continue;
			const DWORD vnum = worn->GetVnum();
			const bool never = IsPlayerBotNeverWornUnique(vnum);
			// MT2009_PLUS_BOT_RANK_GLOVE_V1: the Prophecy King's Glove or Symbol
			// pays nothing at a rank of zero or more, and its minutes run only
			// on the hand: off then, and off whenever the bot is not hunting.
			const bool rankUnique = IsPlayerBotRankUnique(vnum);
			const bool rankPaysNothing = rankUnique && ch->GetRealAlignment() >= 0;
			if (!never && !rankPaysNothing &&
					!(idle && (IsPlayerBotTimedUnique(vnum) || rankUnique)))
				continue;
			if (!IsPlayerBotUniqueSwapWindowOpen(ch, state, dwNow))
			{
				next = dwNow + PLAYERBOT_TIMED_UNIQUE_RETRY_MS;
				return;
			}
			if (ch->GetEmptyInventory(worn->GetSize()) < 0)
				return;
			const long remain = worn->GetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME);
			// A unique the bot never wears is a decision of its own (playerbot_explain.h).
			TPlayerBotEquipExplain explained;
			if (never && IsPlayerBotExplainOn())
				PreparePlayerBotEquipExplain(explained, ch, wear, per::PATH_UNIQUE, per::RULE_UNIQUE_NEVER_WORN,
						NULL, worn, false);
			if (ch->UnequipItem(worn))
			{
				sys_log(0, "PLAYERBOT_GEAR: unique off pid=%u name=%s vnum=%u reason=%s remain_min=%ld rank=%d",
						ch->GetPlayerID(), ch->GetName(), vnum,
						never ? "never_worn" : rankPaysNothing ? "rank_not_negative" : idle, remain,
						ch->GetRealAlignment());
				QueuePlayerBotEquip(explained);
			}
			return;
		}
		if (idle)
			return;

		// MT2009_PLUS_BOT_RANK_GLOVE_V1: at a negative rank the Prophecy King's
		// Glove first - it doubles what every kill gives back - and it may take
		// a ring's or a thief's glove's slot; the Symbol after the rings and
		// gloves, into a slot nothing on a clock holds.
		const bool rankNegative = ch->GetRealAlignment() < 0;
		WORD cell = 0;
		LPITEM item = NULL;
		bool rankGlove = false;
		if (rankNegative && !IsPlayerBotWearingVnum(ch, PLAYERBOT_RANK_GLOVE_VNUM))
		{
			item = FindPlayerBotBagRankUnique(ch, PLAYERBOT_RANK_GLOVE_VNUM, cell);
			rankGlove = item != NULL;
		}

		// On: the glove pays every bot that kills, the ring only one that still
		// levels - a dropper at its lock takes the glove and leaves the ring.
		if (!item && !IsPlayerBotWearingTimedUnique(ch, false))
			item = FindPlayerBotBagTimedUnique(ch, false, cell);
		if (!item && !IsPlayerBotExpLocked(ch) && (int)ch->GetLevel() < PLAYER_MAX_LEVEL_CONST &&
				!IsPlayerBotWearingTimedUnique(ch, true))
			item = FindPlayerBotBagTimedUnique(ch, true, cell);
		if (!item && rankNegative && !IsPlayerBotWearingVnum(ch, PLAYERBOT_RANK_SYMBOL_VNUM))
			item = FindPlayerBotBagRankUnique(ch, PLAYERBOT_RANK_SYMBOL_VNUM, cell);
		if (!item)
			return;
		if (!IsPlayerBotUniqueSwapWindowOpen(ch, state, dwNow))
		{
			next = dwNow + PLAYERBOT_TIMED_UNIQUE_RETRY_MS;
			return;
		}
		if (!PlayerBotCanEquipNow(ch, item, TItemPos(INVENTORY, cell)))
			return;

		// A free slot if there is one; otherwise the one whose unique pays a bot
		// nothing - the Prophet King's glove or symbol, the horse's tail - but
		// never a ring or glove already on its clock, a fishing pass the session
		// has just asked for, or what the engine will not let go of.
		LPITEM displaced = NULL;
		if (ch->GetWear(WEAR_UNIQUE1) && ch->GetWear(WEAR_UNIQUE2))
		{
			// MT2009_PLUS_BOT_RANK_GLOVE_V1: the rank glove's second pass may
			// take the slot of a ring (first) or a thief's glove - never the
			// Symbol's or its own kind's. The other passes leave a slot the
			// rank items hold at a negative rank (IsPlayerBotUniqueSlotHeld).
			for (int pass_ = 0; pass_ < (rankGlove ? 3 : 1) && !displaced; ++pass_)
			for (int wear = WEAR_UNIQUE1; wear <= WEAR_UNIQUE2 && !displaced; ++wear)
			{
				LPITEM worn = ch->GetWear(wear);
				if (!worn || !IsPlayerBotWornItemSound(ch, worn, wear) ||
						IS_SET(worn->GetFlag(), ITEM_FLAG_IRREMOVABLE) || IsPlayerBotSidekickPinned(ch, worn))
					continue;
				if (IsPlayerBotUniqueSlotHeld(ch, worn->GetVnum()) &&
						!(pass_ == 1 && IsPlayerBotExpRing(worn->GetVnum())) &&
						!(pass_ == 2 && IsPlayerBotThiefGlove(worn->GetVnum())))
					continue;
#if defined(PLAYERBOT_ENGINE_MT2009)
				if (worn->GetVnum() == UNIQUE_ITEM_FISHING_PASS &&
						IsPlayerBotFishingPassHeld(ch->GetPlayerID(), dwNow))
					continue;
#endif
				displaced = worn;
			}
			if (!displaced || ch->GetEmptyInventory(displaced->GetSize()) < 0 ||
					!ch->UnequipItem(displaced))
				return;
		}
		const DWORD vnum = item->GetVnum();
		const DWORD displacedVnum = displaced ? displaced->GetVnum() : 0;
		if (ch->EquipItem(item))
			sys_log(0, "PLAYERBOT_GEAR: unique on pid=%u name=%s vnum=%u kind=%s replaced=%u remain_min=%ld map=%ld rank=%d",
					ch->GetPlayerID(), ch->GetName(), vnum,
					IsPlayerBotExpRing(vnum) ? "exp_ring" : IsPlayerBotThiefGlove(vnum) ? "thief_glove" :
					vnum == PLAYERBOT_RANK_GLOVE_VNUM ? "rank_glove" : "rank_symbol", displacedVnum,
					item->GetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME), ch->GetMapIndex(), ch->GetRealAlignment());
	}
}

#endif
