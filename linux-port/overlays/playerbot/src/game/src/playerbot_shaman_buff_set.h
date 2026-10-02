#ifndef __INC_METIN2_PLAYERBOT_SHAMAN_BUFF_SET_H__
#define __INC_METIN2_PLAYERBOT_SHAMAN_BUFF_SET_H__

// MT2009_PLUS_BOT_SHAMAN_INT_SET_V1: a Shaman's Intelligence set.
//
// A Shaman's buffs - Blessing, Dragon's Aid, Swiftness, Attack Up and the
// heal - are as strong as its Intelligence when it casts them, and stay that
// strong for their whole time. A player keeps a second weapon, shield,
// earrings and necklace with INT lines in the bag, changes into them for the
// cast and back into the fighting gear afterwards. The bots do the same:
//
// - the set is chosen from the bag, a place at a time - the weapon (one the
//   bot's group can use), the shield, the earrings and the necklace with the
//   most Intelligence that the bot can wear now, kept only where it beats the
//   piece worn by PLAYERBOT_BUFF_SET_MIN_GAIN and the four together by
//   PLAYERBOT_BUFF_SET_MIN_TOTAL;
// - a piece of the set (and, while the set is on, the fighting piece in the
//   bag) is kept from the merchant, the counter, the storekeeper and the junk
//   rule like a backup weapon (IsPlayerBotKeptBackupWeapon and
//   IsPlayerBotKeptBackupArmour ask IsPlayerBotBuffSetPiece);
// - on its own a Shaman changes into the set briefly: when a buff is missing
//   (ManagePlayerBotCombatBuffs) it holds its blows for the engine's
//   "stand still" second and a half, swaps the pieces in place (the engine's
//   EquipItem puts the worn piece in the new one's bag cell, no free cell
//   needed), casts what is missing and changes back once nothing is left to
//   cast - PLAYERBOT_BUFF_SET_CAST_MAX_MS at most;
// - in a party with a person it fights in the set all the time: the person's
//   buffs are what the party is for, and changing back and forth in front of
//   the person would be every buff's few seconds of a bot doing nothing.
//
// The equipment pass leaves a Shaman in its set alone (ManagePlayerBotEquipment
// asks IsPlayerBotBuffSetDressed): it would put the fighting pieces straight
// back on. A companion is left out - what it wears is its owner's.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_gear.h (SumPlayerBotItemLines,
// IsPlayerBotWeaponSubTypeFor) and before playerbot_combat.h, whose buff pass
// calls it.

namespace
{
	const long PLAYERBOT_BUFF_SET_MIN_GAIN = 3;
	const long PLAYERBOT_BUFF_SET_MIN_TOTAL = 6;
	const BYTE PLAYERBOT_BUFF_SET_MIN_LEVEL = 20;
	const DWORD PLAYERBOT_BUFF_SET_PICK_MS = 60000;
	const DWORD PLAYERBOT_BUFF_SET_CAST_MAX_MS = 20000;
	const DWORD PLAYERBOT_BUFF_SET_STILL_WAIT_MS = 5000;
	const DWORD PLAYERBOT_BUFF_SET_GIVE_UP_MS = 60000;
	const DWORD PLAYERBOT_BUFF_SET_PERSON_CHECK_MS = 5000;
	const int PLAYERBOT_BUFF_SET_PERSON_RANGE = 5000;

	enum
	{
		PLAYERBOT_BUFF_SET_WEAPON,
		PLAYERBOT_BUFF_SET_SHIELD,
		PLAYERBOT_BUFF_SET_EAR,
		PLAYERBOT_BUFF_SET_NECK,
		PLAYERBOT_BUFF_SET_PLACES
	};

	const int PLAYERBOT_BUFF_SET_WEAR[PLAYERBOT_BUFF_SET_PLACES] = { WEAR_WEAPON, WEAR_SHIELD, WEAR_EAR, WEAR_NECK };
	const char* const PLAYERBOT_BUFF_SET_PLACE_NAMES[PLAYERBOT_BUFF_SET_PLACES] = { "weapon", "shield", "ear", "neck" };

	struct TPlayerBotBuffSet
	{
		// The set's pieces by item id, 0 where the place has none worth it.
		DWORD adwSetId[PLAYERBOT_BUFF_SET_PLACES];
		// While the set is on: what was worn before in each place it took
		// (0 for a place that was empty), to go back on.
		DWORD adwCombatId[PLAYERBOT_BUFF_SET_PLACES];
		bool abSwapped[PLAYERBOT_BUFF_SET_PLACES];
		bool bDressed;
		bool bStanding;
		// The cast session found nothing more to cast: change back.
		bool bDone;
		DWORD dwDressedAt;
		DWORD dwHoldSince;
		DWORD dwNextPick;
		DWORD dwNextPersonCheck;
		DWORD dwGiveUpUntil;
		bool bPersonNear;
		int iIqBefore;
		unsigned int uCasts;
		// Pieces the engine refused to put on (a sex flag, a bound piece):
		// never chosen again by this process.
		std::set<DWORD> setRefused;

		TPlayerBotBuffSet()
			: bDressed(false), bStanding(false), bDone(false), dwDressedAt(0), dwHoldSince(0),
			  dwNextPick(0), dwNextPersonCheck(0), dwGiveUpUntil(0), bPersonNear(false),
			  iIqBefore(0), uCasts(0)
		{
			for (int i = 0; i < PLAYERBOT_BUFF_SET_PLACES; ++i)
			{
				adwSetId[i] = 0;
				adwCombatId[i] = 0;
				abSwapped[i] = false;
			}
		}
	};
	std::map<DWORD, TPlayerBotBuffSet> s_mapPlayerBotBuffSets;
	unsigned int s_uPlayerBotBuffSetSessions = 0;
	unsigned int s_uPlayerBotBuffSetCasts = 0;

	bool IsPlayerBotBuffSetCandidate(LPCHARACTER ch)
	{
		return ch && ch->GetJob() == JOB_SHAMAN && ch->GetLevel() >= PLAYERBOT_BUFF_SET_MIN_LEVEL &&
				ch->GetSkillGroup() != 0 && !IsPlayerBotSidekickPID(ch->GetPlayerID());
	}

	// Whether an item may stand in the set's place: the right kind, wearable
	// by this bot now, in its bag.
	bool FitsPlayerBotBuffSetPlace(LPCHARACTER ch, LPITEM item, int place)
	{
		if (!ch || !item || item->IsEquipped() || item->GetWindow() != INVENTORY || item->GetOwner() != ch ||
				!item->CanUsedBy(ch) || item->GetLevelLimit() > ch->GetLevel())
			return false;
		switch (place)
		{
			case PLAYERBOT_BUFF_SET_WEAPON:
				return item->GetType() == ITEM_WEAPON && item->GetSubType() != WEAPON_ARROW &&
						IsPlayerBotWeaponSubTypeFor(ch, item->GetSubType());
			case PLAYERBOT_BUFF_SET_SHIELD:
				return item->GetType() == ITEM_ARMOR && item->GetSubType() == ARMOR_SHIELD;
			case PLAYERBOT_BUFF_SET_EAR:
				return item->GetType() == ITEM_ARMOR && item->GetSubType() == ARMOR_EAR;
			case PLAYERBOT_BUFF_SET_NECK:
				return item->GetType() == ITEM_ARMOR && item->GetSubType() == ARMOR_NECK;
		}
		return false;
	}

	// The set, read from the bag afresh: for every place the piece with the
	// most Intelligence over what is worn there, and nothing at all when the
	// four together would not be worth the change.
	void PickPlayerBotBuffSet(LPCHARACTER ch, TPlayerBotBuffSet& set, DWORD dwNow)
	{
		set.dwNextPick = dwNow + PLAYERBOT_BUFF_SET_PICK_MS;
		long gainTotal = 0;
		DWORD picked[PLAYERBOT_BUFF_SET_PLACES] = { 0, 0, 0, 0 };
		for (int place = 0; place < PLAYERBOT_BUFF_SET_PLACES; ++place)
		{
			LPITEM worn = ch->GetWear(PLAYERBOT_BUFF_SET_WEAR[place]);
			// A piece the owner of a stone weapon or a rod put in the hand is
			// not a fighting piece to be swapped: the set waits for the weapon.
			if (place == PLAYERBOT_BUFF_SET_WEAPON && (!worn || worn->GetType() != ITEM_WEAPON))
				continue;
			const long wornInt = worn ? SumPlayerBotItemLines(worn, APPLY_INT) : 0;
			long bestInt = wornInt + PLAYERBOT_BUFF_SET_MIN_GAIN - 1;
			LPITEM best = NULL;
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetCell() != cell || set.setRefused.count(item->GetID()) ||
						!FitsPlayerBotBuffSetPlace(ch, item, place))
					continue;
				const long iq = SumPlayerBotItemLines(item, APPLY_INT);
				if (iq > bestInt)
				{
					bestInt = iq;
					best = item;
				}
			}
			if (best)
			{
				picked[place] = best->GetID();
				gainTotal += bestInt - wornInt;
			}
		}
		const bool worth = gainTotal >= PLAYERBOT_BUFF_SET_MIN_TOTAL;
		for (int place = 0; place < PLAYERBOT_BUFF_SET_PLACES; ++place)
			set.adwSetId[place] = worth ? picked[place] : 0;
	}

	TPlayerBotBuffSet* GetPlayerBotBuffSet(LPCHARACTER ch, DWORD dwNow)
	{
		if (!IsPlayerBotBuffSetCandidate(ch))
			return NULL;
		TPlayerBotBuffSet& set = s_mapPlayerBotBuffSets[ch->GetPlayerID()];
		if (!set.bDressed && (set.dwNextPick == 0 || (long)(dwNow - set.dwNextPick) >= 0))
			PickPlayerBotBuffSet(ch, set, dwNow);
		return &set;
	}

	bool HasPlayerBotBuffSet(const TPlayerBotBuffSet& set)
	{
		for (int place = 0; place < PLAYERBOT_BUFF_SET_PLACES; ++place)
			if (set.adwSetId[place] != 0)
				return true;
		return false;
	}

	// Declared in playerbot_gear.h: a piece the junk rule, the merchant, the
	// counter and the storekeeper leave alone - the set's, and while the set
	// is on, the fighting piece waiting in the bag to go back on.
	bool IsPlayerBotBuffSetPiece(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || item->GetID() == 0 || ch->GetJob() != JOB_SHAMAN)
			return false;
		std::map<DWORD, TPlayerBotBuffSet>::const_iterator it = s_mapPlayerBotBuffSets.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotBuffSets.end())
			return false;
		const TPlayerBotBuffSet& set = it->second;
		for (int place = 0; place < PLAYERBOT_BUFF_SET_PLACES; ++place)
			if (set.adwSetId[place] == item->GetID() ||
					(set.bDressed && set.abSwapped[place] && set.adwCombatId[place] == item->GetID()))
				return true;
		return false;
	}

	bool IsPlayerBotBuffSetDressed(LPCHARACTER ch)
	{
		if (!ch || ch->GetJob() != JOB_SHAMAN)
			return false;
		std::map<DWORD, TPlayerBotBuffSet>::const_iterator it = s_mapPlayerBotBuffSets.find(ch->GetPlayerID());
		return it != s_mapPlayerBotBuffSets.end() && it->second.bDressed;
	}

	// A person in the bot's party, on its map and near: the set stays on.
	bool IsPlayerBotPartyWithPersonNear(LPCHARACTER ch)
	{
		LPPARTY party = ch ? ch->GetParty() : NULL;
		if (!party)
			return false;
		struct FFindPerson
		{
			LPCHARACTER m_ch;
			bool m_bFound;
			explicit FFindPerson(LPCHARACTER ch) : m_ch(ch), m_bFound(false) {}
			void operator()(LPCHARACTER member)
			{
				if (m_bFound || !member || member == m_ch || !member->IsPC() || !member->GetDesc() ||
						CPlayerBotManager::instance().IsRegisteredBotPID(member->GetPlayerID()))
					return;
				if (DISTANCE_APPROX(m_ch->GetX() - member->GetX(), m_ch->GetY() - member->GetY()) <=
						PLAYERBOT_BUFF_SET_PERSON_RANGE)
					m_bFound = true;
			}
		};
		FFindPerson find(ch);
		party->ForEachOnMapMember(find, ch->GetMapIndex());
		return find.m_bFound;
	}

	// The engine's "stand still" rule: no piece changes within a second and a
	// half of a blow or a skill. The bot holds its blows for it.
	bool HoldPlayerBotForBuffSet(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotBuffSet& set, DWORD dwNow)
	{
		if (!IsPlayerBotEquipWindowShut(ch, state))
		{
			set.dwHoldSince = 0;
			return false;
		}
		if (set.dwHoldSince == 0)
			set.dwHoldSince = dwNow;
		if (state.dwNextAttackTime < dwNow + PLAYERBOT_EQUIPMENT_COMBAT_DELAY)
			state.dwNextAttackTime = dwNow + PLAYERBOT_EQUIPMENT_COMBAT_DELAY;
		return true;
	}

	bool DressPlayerBotBuffSet(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotBuffSet& set, DWORD dwNow,
			bool standing)
	{
		const int iqBefore = ch->GetPoint(POINT_IQ);
		unsigned int places = 0;
		for (int place = 0; place < PLAYERBOT_BUFF_SET_PLACES; ++place)
		{
			set.abSwapped[place] = false;
			set.adwCombatId[place] = 0;
			if (set.adwSetId[place] == 0)
				continue;
			LPITEM piece = ITEM_MANAGER::instance().Find(set.adwSetId[place]);
			if (!piece || !FitsPlayerBotBuffSetPlace(ch, piece, place))
			{
				set.adwSetId[place] = 0;
				continue;
			}
			LPITEM worn = ch->GetWear(PLAYERBOT_BUFF_SET_WEAR[place]);
			const DWORD wornId = worn ? worn->GetID() : 0;
			if (!PlayerBotEquipItem(ch, piece) || !piece->IsEquipped())
			{
				set.setRefused.insert(piece->GetID());
				set.adwSetId[place] = 0;
				sys_log(0, "PLAYERBOT_BUFFSET: dress refused pid=%u name=%s place=%s vnum=%u",
						ch->GetPlayerID(), ch->GetName(), PLAYERBOT_BUFF_SET_PLACE_NAMES[place], piece->GetVnum());
				continue;
			}
			set.adwCombatId[place] = wornId;
			set.abSwapped[place] = true;
			++places;
		}
		if (places == 0)
		{
			set.dwGiveUpUntil = dwNow + PLAYERBOT_BUFF_SET_GIVE_UP_MS;
			return false;
		}
		set.bDressed = true;
		set.bStanding = standing;
		set.bDone = false;
		set.dwDressedAt = dwNow;
		set.dwHoldSince = 0;
		set.iIqBefore = iqBefore;
		set.uCasts = 0;
		++s_uPlayerBotBuffSetSessions;
		// Straight to the cast: the pieces are on, the buffs are what they
		// are for.
		state.dwNextBuffCheckTime = dwNow;
		sys_log(0, "PLAYERBOT_BUFFSET: dress pid=%u name=%s mode=%s places=%u iq=%d->%d",
				ch->GetPlayerID(), ch->GetName(), standing ? "standing" : "cast", places,
				iqBefore, ch->GetPoint(POINT_IQ));
		return true;
	}

	// Back into the fighting pieces, each swapped in place. True once nothing
	// of the set is worn any more.
	bool UndressPlayerBotBuffSet(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotBuffSet& set, DWORD dwNow,
			const char* reason)
	{
		const int iqAtCast = ch->GetPoint(POINT_IQ);
		bool clean = true;
		for (int place = 0; place < PLAYERBOT_BUFF_SET_PLACES; ++place)
		{
			if (!set.abSwapped[place])
				continue;
			LPITEM piece = ch->GetWear(PLAYERBOT_BUFF_SET_WEAR[place]);
			if (!piece || piece->GetID() != set.adwSetId[place])
			{
				// Something else is worn there now (the owner of the piece
				// took it, a burn at the anvil): nothing of the set to undo.
				set.abSwapped[place] = false;
				continue;
			}
			LPITEM combat = set.adwCombatId[place] ? ITEM_MANAGER::instance().Find(set.adwCombatId[place]) : NULL;
			bool back = false;
			if (combat && combat->GetOwner() == ch && combat->GetWindow() == INVENTORY && !combat->IsEquipped())
				back = PlayerBotEquipItem(ch, combat) && combat->IsEquipped();
			else if (!combat && ch->GetEmptyInventory(piece->GetSize()) >= 0)
				back = ch->UnequipItem(piece);
			else if (combat)
			{
				// The fighting piece is gone from the bag: the set's stays on
				// until the equipment pass finds better.
				sys_log(0, "PLAYERBOT_BUFFSET: hold lost pid=%u name=%s place=%s set_id=%u combat_id=%u",
						ch->GetPlayerID(), ch->GetName(), PLAYERBOT_BUFF_SET_PLACE_NAMES[place],
						set.adwSetId[place], set.adwCombatId[place]);
				back = true;
			}
			if (back)
				set.abSwapped[place] = false;
			else
			{
				clean = false;
				sys_log(0, "PLAYERBOT_BUFFSET: undress refused pid=%u name=%s place=%s combat_id=%u free=%d",
						ch->GetPlayerID(), ch->GetName(), PLAYERBOT_BUFF_SET_PLACE_NAMES[place],
						set.adwCombatId[place], ch->GetEmptyInventory(piece->GetSize()));
			}
		}
		if (!clean && dwNow - set.dwDressedAt < PLAYERBOT_BUFF_SET_CAST_MAX_MS * 3)
			return false;
		sys_log(0, "PLAYERBOT_BUFFSET: back in the combat set pid=%u name=%s mode=%s casts=%u iq_before=%d iq_at_cast=%d iq_now=%d ms=%u reason=%s",
				ch->GetPlayerID(), ch->GetName(), set.bStanding ? "standing" : "cast", set.uCasts,
				set.iIqBefore, iqAtCast, ch->GetPoint(POINT_IQ), (unsigned int)(dwNow - set.dwDressedAt), reason);
		set.bDressed = false;
		set.bStanding = false;
		set.bDone = false;
		set.dwHoldSince = 0;
		set.dwNextPick = 0;
		// The fighting set is the equipment pass's again, at once.
		state.dwNextEquipmentCheckTime = dwNow;
		return true;
	}

	// Asked on every tick a Shaman's buffs or equipment are looked at: the
	// party with a person puts the set on for good, the end of a cast session
	// (or its clock, or the person gone) takes it off. True while it holds the
	// tick - the blows held for the engine's still second.
	bool MaintainPlayerBotBuffSet(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotBuffSet* set = GetPlayerBotBuffSet(ch, dwNow);
		if (!set)
			return false;
		if (set->dwNextPersonCheck == 0 || (long)(dwNow - set->dwNextPersonCheck) >= 0)
		{
			set->dwNextPersonCheck = dwNow + PLAYERBOT_BUFF_SET_PERSON_CHECK_MS;
			set->bPersonNear = IsPlayerBotPartyWithPersonNear(ch);
		}
		if (set->bDressed)
		{
			bool off = false;
			const char* reason = "";
			if (set->bStanding && !set->bPersonNear)
			{
				off = true;
				reason = "person_gone";
			}
			else if (!set->bStanding && set->bPersonNear)
			{
				// A person joined during a cast: the set simply stays.
				set->bStanding = true;
				sys_log(0, "PLAYERBOT_BUFFSET: standing set on pid=%u name=%s iq_before=%d iq_now=%d",
						ch->GetPlayerID(), ch->GetName(), set->iIqBefore, ch->GetPoint(POINT_IQ));
			}
			else if (!set->bStanding && (set->bDone || dwNow - set->dwDressedAt >= PLAYERBOT_BUFF_SET_CAST_MAX_MS))
			{
				off = true;
				reason = set->bDone ? "cast_done" : "clock";
			}
			if (!off)
				return false;
			if (HoldPlayerBotForBuffSet(ch, state, *set, dwNow))
				return true;
			UndressPlayerBotBuffSet(ch, state, *set, dwNow, reason);
			return true;
		}
		if (!set->bPersonNear || !HasPlayerBotBuffSet(*set) || dwNow < set->dwGiveUpUntil)
			return false;
		if (HoldPlayerBotForBuffSet(ch, state, *set, dwNow))
		{
			if (dwNow - set->dwHoldSince < PLAYERBOT_BUFF_SET_STILL_WAIT_MS)
				return true;
			set->dwHoldSince = 0;
			set->dwGiveUpUntil = dwNow + PLAYERBOT_BUFF_SET_GIVE_UP_MS;
			return false;
		}
		DressPlayerBotBuffSet(ch, state, *set, dwNow, true);
		return true;
	}

	// ManagePlayerBotCombatBuffs found a buff to cast: into the set first,
	// when there is one. True while the change holds the tick (the still
	// second, or the change itself); false to cast as the bot stands.
	bool PreparePlayerBotBuffSetForCast(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotBuffSet* set = GetPlayerBotBuffSet(ch, dwNow);
		if (!set)
			return false;
		if (set->bDressed)
		{
			++set->uCasts;
			++s_uPlayerBotBuffSetCasts;
			return false;
		}
		if (!HasPlayerBotBuffSet(*set) || dwNow < set->dwGiveUpUntil)
			return false;
		if (HoldPlayerBotForBuffSet(ch, state, *set, dwNow))
		{
			if (dwNow - set->dwHoldSince < PLAYERBOT_BUFF_SET_STILL_WAIT_MS)
			{
				state.dwNextBuffCheckTime = dwNow + 300;
				return true;
			}
			// Never still long enough (a fight that does not let go): cast
			// as it stands, and the set waits a minute.
			set->dwHoldSince = 0;
			set->dwGiveUpUntil = dwNow + PLAYERBOT_BUFF_SET_GIVE_UP_MS;
			return false;
		}
		if (!DressPlayerBotBuffSet(ch, state, *set, dwNow, set->bPersonNear))
			return false;
		sys_log(0, "PLAYERBOT_BUFFSET: cast pid=%u name=%s iq=%d iq_before=%d",
				ch->GetPlayerID(), ch->GetName(), ch->GetPoint(POINT_IQ), set->iIqBefore);
		state.dwNextBuffCheckTime = dwNow + 200;
		return true;
	}

	// The set's place an item of a counter would take, by its kind alone (a
	// counter's line is a preview, in nobody's bag): -1 for none.
	int GetPlayerBotBuffSetPlaceOf(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !item->CanUsedBy(ch) || item->GetLevelLimit() > ch->GetLevel())
			return -1;
		if (item->GetType() == ITEM_WEAPON)
			return item->GetSubType() != WEAPON_ARROW && IsPlayerBotWeaponSubTypeFor(ch, item->GetSubType())
					? PLAYERBOT_BUFF_SET_WEAPON : -1;
		if (item->GetType() != ITEM_ARMOR)
			return -1;
		switch (item->GetSubType())
		{
			case ARMOR_SHIELD: return PLAYERBOT_BUFF_SET_SHIELD;
			case ARMOR_EAR: return PLAYERBOT_BUFF_SET_EAR;
			case ARMOR_NECK: return PLAYERBOT_BUFF_SET_NECK;
		}
		return -1;
	}

	// Whether a counter's piece would better the set by a place's worth, with
	// the set worth carrying afterwards: what a Shaman of thirty and up walks
	// to a stand for (FindPlayerBotBuffSetPick, playerbot_offline_market.h).
	bool WantsPlayerBotBuffSetPiece(LPCHARACTER ch, LPITEM item)
	{
		if (!IsPlayerBotBuffSetCandidate(ch) || ch->GetLevel() < 30)
			return false;
		const int place = GetPlayerBotBuffSetPlaceOf(ch, item);
		if (place < 0)
			return false;
		const long iq = SumPlayerBotItemLines(item, APPLY_INT);
		if (iq < PLAYERBOT_BUFF_SET_MIN_GAIN)
			return false;
		TPlayerBotBuffSet* set = GetPlayerBotBuffSet(ch, get_dword_time());
		if (!set)
			return false;
		long total = 0;
		long placeGain = 0;
		long wornAtPlace = 0;
		for (int p = 0; p < PLAYERBOT_BUFF_SET_PLACES; ++p)
		{
			LPITEM worn = ch->GetWear(PLAYERBOT_BUFF_SET_WEAR[p]);
			// While the set is on, what was worn before is the measure.
			if (set->bDressed && set->abSwapped[p])
				worn = set->adwCombatId[p] ? ITEM_MANAGER::instance().Find(set->adwCombatId[p]) : NULL;
			const long wornInt = worn ? SumPlayerBotItemLines(worn, APPLY_INT) : 0;
			LPITEM piece = set->adwSetId[p] ? ITEM_MANAGER::instance().Find(set->adwSetId[p]) : NULL;
			const long gain = piece ? std::max(0L, SumPlayerBotItemLines(piece, APPLY_INT) - wornInt) : 0;
			total += gain;
			if (p == place)
			{
				placeGain = gain;
				wornAtPlace = wornInt;
			}
		}
		// A weapon place needs a weapon in the hand to change from.
		if (place == PLAYERBOT_BUFF_SET_WEAPON)
		{
			LPITEM hand = ch->GetWear(WEAR_WEAPON);
			if (!hand || hand->GetType() != ITEM_WEAPON)
				return false;
		}
		const long newGain = iq - wornAtPlace;
		if (newGain < placeGain + PLAYERBOT_BUFF_SET_MIN_GAIN)
			return false;
		return total - placeGain + newGain >= PLAYERBOT_BUFF_SET_MIN_TOTAL;
	}

	// A set piece is bought out of a share of what the bot can spend, and a
	// person's piece no dearer than the bots' cap on a person's price
	// (IsPlayerBotPersonPriceFair, playerbot_market.h).
	// MT2009_PLUS_BOT_SHAMAN_INT_SET_V2: up to 40% of the bot's gold (it was
	// a twentieth, which bought almost nothing).
	const long long PLAYERBOT_BUFF_SET_BUDGET_PERCENT = 40;

	// The buff pass found nothing more to cast: a cast session is over.
	void NotePlayerBotBuffSetNothingToCast(LPCHARACTER ch)
	{
		if (!ch || ch->GetJob() != JOB_SHAMAN)
			return;
		std::map<DWORD, TPlayerBotBuffSet>::iterator it = s_mapPlayerBotBuffSets.find(ch->GetPlayerID());
		if (it != s_mapPlayerBotBuffSets.end() && it->second.bDressed && !it->second.bStanding)
			it->second.bDone = true;
	}
}

#endif
