#ifndef PLAYERBOT_WEAPON_GOAL_H
#define PLAYERBOT_WEAPON_GOAL_H

// What a bot is playing for, weapon-wise. "To jest zart, postac 75 lv nosi
// gilotynowe ostrze ... opracuj wszystkie bronie dostepne na serwerze, aby
// boty wiedzialy po co graja" (Tieru, 15 September): a warrior of 75 in a
// two-hander of level ten, with nothing in the AI that knew a better weapon
// existed, where it came from, or that it was worth saving for.
//
// The atlas (playerbot_weapon_atlas.h, generated from the world's own tables)
// names every weapon family, who may carry it and where one comes from. From
// it each bot has a goal: the family of its class and build, at or under its
// level, that it can get on a map the bots walk, with the hardest blow at +0
// by the damage model. The goal against the weapon in the hand is what sends
// a bot that can pay to the market, what lets it spend its own savings on a
// counter weapon far better than its own, and what the census counts.
//
// Included after playerbot_town.h (the price sheet) and before
// playerbot_market.h, which buys by it.
namespace
{
	const WORD PLAYERBOT_WEAPON_SOURCE_REACHABLE = PLAYERBOT_WEAPON_SOURCE_MERCHANT |
			PLAYERBOT_WEAPON_SOURCE_COMMON | PLAYERBOT_WEAPON_SOURCE_DROP | PLAYERBOT_WEAPON_SOURCE_CHEST;

	BYTE GetPlayerBotWeaponClassBit(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		switch (ch->GetJob())
		{
			case JOB_WARRIOR:  return PLAYERBOT_WEAPON_CLASS_WARRIOR;
			case JOB_ASSASSIN: return PLAYERBOT_WEAPON_CLASS_ASSASSIN;
			case JOB_SURA:     return PLAYERBOT_WEAPON_CLASS_SURA;
			case JOB_SHAMAN:   return PLAYERBOT_WEAPON_CLASS_SHAMAN;
			default:           return 0;
		}
	}

	// The best family of the atlas this character may carry now and can get
	// on a map the bots walk, by its blow at +0 - the unrefined weapon a drop
	// or a counter hands it. NULL when the atlas has none for it.
	const TPlayerBotWeaponFamily* FindPlayerBotWeaponGoal(LPCHARACTER ch, long long& blowOut)
	{
		blowOut = 0;
		const BYTE classBit = GetPlayerBotWeaponClassBit(ch);
		if (classBit == 0)
			return NULL;
		const TPlayerBotWeaponFamily* best = NULL;
		for (size_t i = 0; i < sizeof(PLAYERBOT_WEAPON_ATLAS) / sizeof(PLAYERBOT_WEAPON_ATLAS[0]); ++i)
		{
			const TPlayerBotWeaponFamily& row = PLAYERBOT_WEAPON_ATLAS[i];
			if ((row.bClassMask & classBit) == 0 || (int)row.bLevel > (int)ch->GetLevel() ||
					(row.wSources & PLAYERBOT_WEAPON_SOURCE_REACHABLE) == 0 ||
					!IsPlayerBotWeaponSubTypeFor(ch, row.bSubType))
				continue;
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(row.dwBaseVnum);
			if (!proto || !IsPlayerBotProtoForCharacter(ch, proto))
				continue;
			const long long blow = GetPlayerBotWeaponHitDamageAt(NULL, proto, ch);
			if (!best || blow > blowOut)
			{
				best = &row;
				blowOut = blow;
			}
		}
		return best;
	}

	struct TPlayerBotWeaponGoal
	{
		const TPlayerBotWeaponFamily* family;
		long long goalBlow;
		long long handBlow;
		DWORD handVnum;
		DWORD when;
	};
	std::map<DWORD, TPlayerBotWeaponGoal> s_mapPlayerBotWeaponGoals;

	bool IsPlayerBotWeaponOutclassed(const TPlayerBotWeaponGoal& goal)
	{
		return goal.family != NULL &&
				goal.goalBlow * 100 >= goal.handBlow * (100 + PLAYERBOT_WEAPON_OUTCLASSED_PERCENT);
	}

	// The goal and the hand, read again every PLAYERBOT_WEAPON_GOAL_REFRESH_MS.
	// A new goal the hand is outclassed by is said once, with where it comes
	// from - the line that tells an operator what a bot is playing for.
	const TPlayerBotWeaponGoal& GetPlayerBotWeaponGoal(LPCHARACTER ch, DWORD dwNow)
	{
		TPlayerBotWeaponGoal& goal = s_mapPlayerBotWeaponGoals[ch->GetPlayerID()];
		if (goal.when != 0 && dwNow - goal.when < PLAYERBOT_WEAPON_GOAL_REFRESH_MS)
			return goal;
		const TPlayerBotWeaponFamily* previous = goal.family;
		goal.when = dwNow != 0 ? dwNow : 1;
		goal.family = FindPlayerBotWeaponGoal(ch, goal.goalBlow);
		LPITEM hand = GetPlayerBotHandWeapon(ch);
		goal.handVnum = hand ? hand->GetVnum() : 0;
		goal.handBlow = hand ? GetPlayerBotWeaponHitDamage(hand, ch) : 0;
		if (goal.family != NULL && goal.family != previous && IsPlayerBotWeaponOutclassed(goal))
		{
			const TItemTable* goalProto = ITEM_MANAGER::instance().GetTable(goal.family->dwBaseVnum);
			sys_log(0, "PLAYERBOT_WEAPON: goal pid=%u name=%s level=%u hand=%u blow=%lld goal=%u(%s, level %u) blow=%lld sources=0x%02x mob=%u map=%u",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), goal.handVnum, goal.handBlow,
					goal.family->dwBaseVnum, goalProto ? goalProto->szLocaleName : "?",
					(unsigned int)goal.family->bLevel, goal.goalBlow, (unsigned int)goal.family->wSources,
					goal.family->dwSourceMob, (unsigned int)goal.family->wSourceMap);
		}
		return goal;
	}

	// A weapon on a counter worth this bot's savings: one it can wear now whose
	// blow beats the hand's by PLAYERBOT_WEAPON_STRATEGIC_GAIN_PERCENT. CiosZKarpia
	// held 125 million yang and a Gilotynowe Ostrze +7, and no counter weapon
	// could pass the share of the median wallet a purchase was capped at.
	bool IsPlayerBotStrategicWeaponOffer(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !offer || offer->GetType() != ITEM_WEAPON || !IsPlayerBotEquipmentCandidate(ch, offer) ||
				offer->GetLevelLimit() > ch->GetLevel() || offer->FindEquipCell(ch) != WEAR_WEAPON)
			return false;
		LPITEM hand = GetPlayerBotHandWeapon(ch);
		const long long handBlow = hand ? GetPlayerBotWeaponHitDamage(hand, ch) : 0;
		return GetPlayerBotWeaponHitDamage(offer, ch) * 100 >=
				handBlow * (100 + PLAYERBOT_WEAPON_STRATEGIC_GAIN_PERCENT);
	}

	// What the goal costs on a counter: Iwakura's +0 for the family at this
	// world's yang rate, or the fallback where his sheet has no row.
	DWORD GetPlayerBotWeaponGoalPrice(const TPlayerBotWeaponFamily* family)
	{
		if (!family)
			return 0;
		for (size_t i = 0; i < sizeof(PLAYERBOT_GEAR_PRICES) / sizeof(PLAYERBOT_GEAR_PRICES[0]); ++i)
			if (PLAYERBOT_GEAR_PRICES[i].dwBaseVnum == family->dwBaseVnum && PLAYERBOT_GEAR_PRICES[i].adwPrice[0] != 0)
				return ScalePlayerBotIwakuraPrice(PLAYERBOT_GEAR_PRICES[i].adwPrice[0]);
		return ScalePlayerBotIwakuraPrice(PLAYERBOT_WEAPON_GOAL_FALLBACK_PRICE);
	}

	// The pid the last census stopped working goals out at. The refresh and the
	// census share one interval, so a census that always started from the first
	// pid would read the same four hundred bots every time and nobody else.
	DWORD s_dwPlayerBotWeaponCensusCursor = 0;

	// How the population stands against its goals, once a report interval
	// from the market ledger: the bots whose hand the atlas outclasses, by how
	// many levels the goal is above the hand, the goals most of them are after
	// and the three furthest behind.
	void ReportPlayerBotWeaponGoals(DWORD dwNow)
	{
		DWORD bots = 0, outclassed = 0, gap10 = 0, gap20 = 0, noWeapon = 0, unread = 0;
		int refreshes = PLAYERBOT_WEAPON_CENSUS_REFRESHES;
		DWORD lastRefreshed = 0;
		std::map<DWORD, DWORD> chased;
		std::vector<std::pair<int, DWORD> > furthest;   // gap, pid
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch || !ch->IsItemLoaded())
				continue;
			++bots;
			// A stale goal is worked out again while this census has room, from
			// the pid after the one the last census stopped at; the rest are
			// counted from what they last read, and a bot never read is unread.
			const std::map<DWORD, TPlayerBotWeaponGoal>::const_iterator known =
					s_mapPlayerBotWeaponGoals.find(it->first);
			const bool read = known != s_mapPlayerBotWeaponGoals.end() && known->second.when != 0;
			const bool stale = !read || dwNow - known->second.when >= PLAYERBOT_WEAPON_GOAL_REFRESH_MS;
			const TPlayerBotWeaponGoal* reading = read ? &known->second : NULL;
			if (stale && refreshes > 0 && it->first > s_dwPlayerBotWeaponCensusCursor)
			{
				--refreshes;
				lastRefreshed = it->first;
				reading = &GetPlayerBotWeaponGoal(ch, dwNow);
			}
			if (!reading)
			{
				++unread;
				continue;
			}
			const TPlayerBotWeaponGoal& goal = *reading;
			if (goal.handVnum == 0)
				++noWeapon;
			if (!IsPlayerBotWeaponOutclassed(goal))
				continue;
			++outclassed;
			const TItemTable* handProto = goal.handVnum ? ITEM_MANAGER::instance().GetTable(goal.handVnum) : NULL;
			const int gap = (int)goal.family->bLevel - (handProto ? GetPlayerBotProtoLevelLimit(handProto) : 0);
			if (gap >= 20)
				++gap20;
			else if (gap >= 10)
				++gap10;
			++chased[goal.family->dwBaseVnum];
			furthest.push_back(std::make_pair(gap, it->first));
		}
		// A census that spent its room carries on from there next time; one that
		// had room left starts the next from the first pid again.
		s_dwPlayerBotWeaponCensusCursor = refreshes <= 0 ? lastRefreshed : 0;
		std::vector<std::pair<DWORD, DWORD> > ranked;   // bots, vnum
		for (std::map<DWORD, DWORD>::const_iterator c = chased.begin(); c != chased.end(); ++c)
			ranked.push_back(std::make_pair(c->second, c->first));
		std::sort(ranked.rbegin(), ranked.rend());
		std::string top;
		for (size_t i = 0; i < ranked.size() && i < 6; ++i)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(ranked[i].second);
			char buf[96];
			snprintf(buf, sizeof(buf), " %s(%u)=%u", proto ? proto->szLocaleName : "?",
					ranked[i].second, ranked[i].first);
			top += buf;
		}
		std::sort(furthest.rbegin(), furthest.rend());
		std::string behind;
		for (size_t i = 0; i < furthest.size() && i < 3; ++i)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(furthest[i].second);
			const std::map<DWORD, TPlayerBotWeaponGoal>::const_iterator g =
					s_mapPlayerBotWeaponGoals.find(furthest[i].second);
			if (!ch || g == s_mapPlayerBotWeaponGoals.end() || !g->second.family)
				continue;
			char buf[160];
			snprintf(buf, sizeof(buf), " %s(lv%u hand=%u goal=%u lv%u src=0x%02x map=%u)",
					ch->GetName(), (unsigned int)ch->GetLevel(), g->second.handVnum,
					g->second.family->dwBaseVnum, (unsigned int)g->second.family->bLevel,
					(unsigned int)g->second.family->wSources, (unsigned int)g->second.family->wSourceMap);
			behind += buf;
		}
		sys_log(0, "PLAYERBOT_WEAPON: census bots=%u unread=%u outclassed=%u goal_10_levels_up=%u goal_20_levels_up=%u no_weapon=%u top:%s furthest:%s",
				bots, unread, outclassed, gap10, gap20, noWeapon, top.c_str(), behind.c_str());
	}
}

#endif
