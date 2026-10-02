#ifndef __INC_METIN2_PLAYERBOT_L30_DROPPER_H__
#define __INC_METIN2_PLAYERBOT_L30_DROPPER_H__

// MT2009_PLUS_L30_WEAPON_DROPPER_V1: the level-30 weapon droppers of Orc
// Valley's first island (the owner, 2 October): "dropki broni 30 lv", two or
// three of them in every kingdom on every channel, held at level twenty-one.
//
//   - who: on each core that hosts Orc Valley, every minute, the kingdom's
//     count is made up to its number from the bots playing here - ordinary
//     ones the checklist would take (IsPlayerBotProgressionEligible: no
//     dropper, no cohort, no companion, no shouter), no trader, no party
//     fighter, of PLAYERBOT_L30_DROPPER_MIN_LEVEL to twenty-three (the lock
//     and the two levels every dropper is allowed past it). A bot that was
//     one before (its quest flag) comes first, then the lowest hash of pid,
//     channel and kingdom - the same bots after every restart. For the first
//     ten minutes of a core only those with the flag are taken back, so a
//     restart does not hand the place to whoever logged in first;
//   - how many: two or three, decided by the hash of PLAYERBOT_L30_DROPPER_SEED,
//     the channel and the kingdom (GetPlayerBotL30DropperTarget) - fixed for
//     a channel and kingdom, different between them;
//   - what it is: BOT_PERSONALITY_L30_WEAPON_DROPPER - a dropper like the
//     others (IsPlayerBotDropper): its exp lock at twenty-one
//     (ManagePlayerBotExpLock), a stall keeper's counter and the dropper's
//     round, no Metin expedition, no fishing, no Monkey Dungeon, no M3;
//   - where: from twenty-one its frontier is Orc Valley
//     (GetPlayerBotFrontierMapForLevelRaw), its hubs its kingdom's first
//     island (PLAYERBOT_L30_DROPPER_HUBS, round the spawn), its priority
//     prey that island's Bestial (531/532/533, GetPlayerBotL30DropperBestial)
//     and the island's ordinary monsters between its respawns, up to
//     PLAYERBOT_L30_DROPPER_MAX_TARGET_LEVEL;
//   - back to town (the frontier branch of ManagePlayerBotWorldTravel) when
//     the potions run out, a level-30 weapon has dropped - to the first
//     village, whose counter it serves at once and waits for up to twenty
//     minutes - or the bag holds PLAYERBOT_SELL_RUN_JUNK_ITEMS pieces of
//     scrap or is full; and then back to the island.
//
// An implementation fragment in the sense playerbot_types.h describes:
// included once, after playerbot_progression.h.

namespace
{
	BYTE GetPlayerBotStablePersonality(LPCHARACTER ch, BYTE role);
	bool IsPlayerBotPastDropperBand(LPCHARACTER ch, BYTE personality);

	const char* const PLAYERBOT_L30_DROPPER_FLAG = "playerbot.l30_dropper";
	const DWORD PLAYERBOT_L30_DROPPER_SEED = 0x4c333057U;
	const BYTE PLAYERBOT_L30_DROPPER_MIN_LEVEL = 17;
	const DWORD PLAYERBOT_L30_DROPPER_PASS_MS = 60 * 1000;
	const DWORD PLAYERBOT_L30_DROPPER_SETTLE_MS = 10 * 60 * 1000;
	const DWORD PLAYERBOT_L30_DROPPER_CENSUS_MS = 10 * 60 * 1000;

	DWORD s_dwPlayerBotL30DropperFirstPass = 0;
	DWORD s_dwPlayerBotL30DropperNextPass = 0;
	DWORD s_dwPlayerBotL30DropperNextCensus = 0;

	// Two or three, by channel and kingdom.
	int GetPlayerBotL30DropperTarget(BYTE empire)
	{
		return 2 + (int)(PlayerBotNavHash(PLAYERBOT_L30_DROPPER_SEED ^
				((DWORD)g_bChannel * 0x9e3779b9U) ^ ((DWORD)empire * 0x85ebca6bU)) % 2U);
	}

	bool IsPlayerBotL30DropperCandidate(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		if (!ch || ch->GetEmpire() < 1 || ch->GetEmpire() > 3)
			return false;
		if (IsPlayerBotPersonaEnabled() && !state.persona.bRestored)
			return false;
		if (ch->GetLevel() < PLAYERBOT_L30_DROPPER_MIN_LEVEL ||
				ch->GetLevel() > PLAYERBOT_EXP_LOCK_L30_WEAPON_DROPPER + PLAYERBOT_DROPPER_OUTGROWN_LEVELS)
			return false;
		if (state.bPersonality == BOT_PERSONALITY_MERCHANT || state.bBotRole == BOT_ROLE_PARTY_FIGHTER)
			return false;
		if (ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()))
			return false;
		return IsPlayerBotProgressionEligible(ch, state);
	}

	void MakePlayerBotL30Dropper(LPCHARACTER ch, TPlayerBotAIState& state, int count, int target, bool flagged)
	{
		state.bPersonality = BOT_PERSONALITY_L30_WEAPON_DROPPER;
		state.persona.bDrawnPersonality = BOT_PERSONALITY_L30_WEAPON_DROPPER;
		state.bBotRole = BOT_ROLE_MOB_GRINDER;
		state.bAmbition = GetPlayerBotStableAmbition(ch, BOT_PERSONALITY_L30_WEAPON_DROPPER);
		state.dwMetinExpeditionUntil = 0;
		state.dwHubChosenTime = 0;
		ch->SetQuestFlag(PLAYERBOT_L30_DROPPER_FLAG, 1);
		sys_log(0, "PLAYERBOT_L30_DROPPER: %s pid=%u name=%s empire=%u channel=%u level=%u map=%ld count=%d/%d",
				flagged ? "back" : "chosen", ch->GetPlayerID(), ch->GetName(), (unsigned)ch->GetEmpire(),
				(unsigned)g_bChannel, (unsigned)ch->GetLevel(), ch->GetMapIndex(), count, target);
	}

	void ReleasePlayerBotL30Dropper(LPCHARACTER ch, TPlayerBotAIState& state, const char* why)
	{
		const BYTE drawn = GetPlayerBotStablePersonality(ch, state.bBotRole);
		state.persona.bDrawnPersonality = drawn;
		state.bPersonality = IsPlayerBotPersonaEnabled() ? GetPlayerBotCharakter(drawn) : drawn;
		state.bAmbition = GetPlayerBotStableAmbition(ch, state.bPersonality);
		ch->SetQuestFlag(PLAYERBOT_L30_DROPPER_FLAG, 0);
		s_mapPlayerBotL30DropperTown.erase(ch->GetPlayerID());
		sys_log(0, "PLAYERBOT_L30_DROPPER: released pid=%u name=%s empire=%u level=%u why=%s",
				ch->GetPlayerID(), ch->GetName(), (unsigned)ch->GetEmpire(), (unsigned)ch->GetLevel(), why);
	}

	void ManagePlayerBotL30WeaponDroppers(DWORD dwNow)
	{
		if (s_dwPlayerBotL30DropperNextPass != 0 && (int)(dwNow - s_dwPlayerBotL30DropperNextPass) < 0)
			return;
		s_dwPlayerBotL30DropperNextPass = dwNow + PLAYERBOT_L30_DROPPER_PASS_MS;
		if (s_dwPlayerBotL30DropperFirstPass == 0)
			s_dwPlayerBotL30DropperFirstPass = dwNow;
		// Its island is on this core, or nobody here is one.
		if (!IsPlayerBotMapHostedHere(PLAYERBOT_MAP_ORC_VALLEY))
			return;
		const bool settled = dwNow - s_dwPlayerBotL30DropperFirstPass >= PLAYERBOT_L30_DROPPER_SETTLE_MS;

		int count[4] = { 0, 0, 0, 0 };
		// (no flag, hash, pid) per kingdom: the flagged first, then the hash.
		std::vector<std::pair<std::pair<int, DWORD>, DWORD> > candidates[4];
		for (TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch || !ch->IsPC())
				continue;
			TPlayerBotAIState& state = it->second;
			const BYTE empire = ch->GetEmpire();
			if (state.bPersonality == BOT_PERSONALITY_L30_WEAPON_DROPPER)
			{
				if (empire < 1 || empire > 3 || IsPlayerBotPastDropperBand(ch, BOT_PERSONALITY_L30_WEAPON_DROPPER))
					ReleasePlayerBotL30Dropper(ch, state, "past_band");
				else if (count[empire] >= GetPlayerBotL30DropperTarget(empire))
					ReleasePlayerBotL30Dropper(ch, state, "over_count");
				else
					++count[empire];
				continue;
			}
			if (!IsPlayerBotL30DropperCandidate(ch, state))
				continue;
			const bool flagged = ch->GetQuestFlag(PLAYERBOT_L30_DROPPER_FLAG) != 0;
			if (!flagged && !settled)
				continue;
			candidates[empire].push_back(std::make_pair(std::make_pair(flagged ? 0 : 1,
					PlayerBotNavHash(it->first ^ PLAYERBOT_L30_DROPPER_SEED ^ ((DWORD)g_bChannel << 24))), it->first));
		}
		for (BYTE empire = 1; empire <= 3; ++empire)
		{
			const int target = GetPlayerBotL30DropperTarget(empire);
			std::sort(candidates[empire].begin(), candidates[empire].end());
			for (size_t i = 0; i < candidates[empire].size() && count[empire] < target; ++i)
			{
				const DWORD pid = candidates[empire][i].second;
				LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pid);
				TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(pid);
				if (!ch || st == s_mapPlayerBotAIStates.end())
					continue;
				++count[empire];
				MakePlayerBotL30Dropper(ch, st->second, count[empire], target, candidates[empire][i].first.first == 0);
			}
		}
		if (s_dwPlayerBotL30DropperNextCensus == 0 || (int)(dwNow - s_dwPlayerBotL30DropperNextCensus) >= 0)
		{
			s_dwPlayerBotL30DropperNextCensus = dwNow + PLAYERBOT_L30_DROPPER_CENSUS_MS;
			sys_log(0, "PLAYERBOT_L30_DROPPER: census channel=%u shinsoo=%d/%d chunjo=%d/%d jinno=%d/%d",
					(unsigned)g_bChannel, count[1], GetPlayerBotL30DropperTarget(1),
					count[2], GetPlayerBotL30DropperTarget(2), count[3], GetPlayerBotL30DropperTarget(3));
		}
	}
}

#endif
