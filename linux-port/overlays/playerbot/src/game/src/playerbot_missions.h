#ifndef __INC_METIN2_PLAYERBOT_MISSIONS_H__
#define __INC_METIN2_PLAYERBOT_MISSIONS_H__

// The two quest lines a bot runs on its own: the Biologist's collections and
// the official level-up hunt.
//
// Both are ordinarily driven by a quest dialog. A bot has no client to press
// Confirm, so the accepting, the counting and the reward are all done here
// against the same quest flags the real quest reads - which is why this is
// bookkeeping rather than behaviour, and why the flag names matter more than
// the code around them.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once,
// after playerbot_gear.h - a finished mission hands out an item.

namespace
{
	std::string GetPlayerBotBiologistFlag(const TPlayerBotBiologistMission& mission,
			const char* flag)
	{
		return std::string(mission.questName) + "." + flag;
	}

	// See GetPlayerBotBiologistStateIndex: a state index the quest does not have.
	const int PLAYERBOT_QUEST_STATE_UNKNOWN = INT_MIN;

	int GetPlayerBotBiologistStateIndex(size_t missionIndex, const char* stateName)
	{
		// Sized by the table, not by a literal: a seventh mission with a
		// six-entry initialiser would have read a zero as a real state index.
		// A state index is a hash of the state's name (quest/object/state/): it
		// is as often negative as not - collect_quest_lv30's key_item is
		// -1726153001 - so "unknown" cannot be a sign. Every caller used to test
		// `>= 0`, which made the Orc Tooth's second half unreachable: after ten
		// teeth the state was never set, collect_count sat at ten, and the bot
		// went on handing teeth in - twenty-two of them (martynka19cm, 12
		// September). The engine answers 0 for a name it does not know, which is
		// also "start"; PLAYERBOT_QUEST_STATE_UNKNOWN is what the callers test.
		static std::vector<int> s_complete(PLAYERBOT_BIOLOGIST_MISSION_COUNT, PLAYERBOT_QUEST_STATE_UNKNOWN);
		static std::vector<int> s_collecting(PLAYERBOT_BIOLOGIST_MISSION_COUNT, PLAYERBOT_QUEST_STATE_UNKNOWN);
		static std::vector<int> s_keyItem(PLAYERBOT_BIOLOGIST_MISSION_COUNT, PLAYERBOT_QUEST_STATE_UNKNOWN);
		static std::vector<char> s_resolved(PLAYERBOT_BIOLOGIST_MISSION_COUNT * 3, 0);
		if (missionIndex >= PLAYERBOT_BIOLOGIST_MISSION_COUNT)
			return PLAYERBOT_QUEST_STATE_UNKNOWN;

		const int kind = strcmp(stateName, "__complete") == 0 ? 0
				: (strcmp(stateName, "key_item") == 0 ? 1 : 2);
		std::vector<int>& cache = kind == 0 ? s_complete : (kind == 1 ? s_keyItem : s_collecting);
		char& resolved = s_resolved[missionIndex * 3 + kind];
		if (!resolved)
		{
			const int index = quest::CQuestManager::instance().GetQuestStateIndex(
					PLAYERBOT_BIOLOGIST_MISSIONS[missionIndex].questName, stateName);
			cache[missionIndex] = (index == 0 && strcmp(stateName, "start") != 0)
					? PLAYERBOT_QUEST_STATE_UNKNOWN : index;
			resolved = 1;
		}
		return cache[missionIndex];
	}

	bool IsPlayerBotBiologistMissionComplete(LPCHARACTER ch, size_t missionIndex)
	{
		if (!ch || missionIndex >= PLAYERBOT_BIOLOGIST_MISSION_COUNT)
			return false;
		const TPlayerBotBiologistMission& mission = PLAYERBOT_BIOLOGIST_MISSIONS[missionIndex];
		const int completeState = GetPlayerBotBiologistStateIndex(missionIndex, "__complete");
		return completeState != PLAYERBOT_QUEST_STATE_UNKNOWN &&
				ch->GetQuestFlag(GetPlayerBotBiologistFlag(mission, "__status")) == completeState;
	}

	// The collect rows are one chain in the quests themselves: the Orc Tooth's
	// last state starts the Curse Book, and the Curse Book's the Demon Souvenir.
	// A bot takes its missions without the quest's dialog, so it keeps that
	// order itself - it did not, and bots of seventy finished the Demon Souvenir
	// with the Orc Tooth still open (Tieru, 15 September: "Ksiegi Klatw sa po
	// Zebach Orka, a po Ksiegach Klatw sa Pamiatki Po Demonie"). A collect row
	// after the first is open once the row before it is complete.
	bool IsPlayerBotBiologistMissionOpen(LPCHARACTER ch, size_t missionIndex)
	{
		if (!ch || missionIndex == 0 || missionIndex >= PLAYERBOT_BIOLOGIST_MISSION_COUNT)
			return true;
		const TPlayerBotBiologistMission& mission = PLAYERBOT_BIOLOGIST_MISSIONS[missionIndex];
		const TPlayerBotBiologistMission& previous = PLAYERBOT_BIOLOGIST_MISSIONS[missionIndex - 1];
		if (mission.requiredLevel <= PLAYERBOT_BIOLOGIST_COLLECT_QUEST_LEVEL ||
				previous.requiredLevel < PLAYERBOT_BIOLOGIST_COLLECT_QUEST_LEVEL)
			return true;
		return IsPlayerBotBiologistMissionComplete(ch, missionIndex - 1);
	}

	// Anything a row collects, and anything a row's second half waits for.
	// Both used to be spelled out as vnums wherever they mattered - the junk
	// rule and the stall each carried "50701..50706, the tooth, the stone" -
	// so a new row meant finding every such list, and the two rows added for
	// the level-40 and level-50 quests would have been sold by the first bot
	// that walked past a merchant.
	bool IsPlayerBotBiologistSpecimen(DWORD vnum)
	{
		if (vnum == 0)
			return false;
		for (size_t i = 0; i < PLAYERBOT_BIOLOGIST_MISSION_COUNT; ++i)
			if (PLAYERBOT_BIOLOGIST_MISSIONS[i].itemVnum == vnum)
				return true;
		return false;
	}

	bool IsPlayerBotBiologistKeyItem(DWORD vnum)
	{
		if (vnum == 0)
			return false;
		for (size_t i = 0; i < PLAYERBOT_BIOLOGIST_MISSION_COUNT; ++i)
			if (PLAYERBOT_BIOLOGIST_MISSIONS[i].keyItemVnum == vnum)
				return true;
		return false;
	}

	// A specimen the bot has no mission left for: the row that wants it is
	// handed in. Those used to stay in the bag for good ("niech dadza sklepik
	// z zebami jesli maja nadmiar"); now they are goods. A key item is never
	// surplus - it is what the second half of its own row is waiting for.
	bool IsPlayerBotBiologistKeyPhase(LPCHARACTER ch, size_t missionIndex);

	bool IsPlayerBotBiologistSpecimenSurplus(LPCHARACTER ch, DWORD vnum)
	{
		if (!ch || IsPlayerBotBiologistKeyItem(vnum))
			return false;
		// So is one whose row waits in key_item: the Biologist has every
		// specimen he wanted and asks only for the key now.
		for (size_t i = 0; i < PLAYERBOT_BIOLOGIST_MISSION_COUNT; ++i)
			if (PLAYERBOT_BIOLOGIST_MISSIONS[i].itemVnum == vnum)
				return IsPlayerBotBiologistMissionComplete(ch, i) ||
						IsPlayerBotBiologistKeyPhase(ch, i);
		return false;
	}

	// A row with a key item has a second half: the specimens are in and the
	// quest waits in key_item for the key. Which rows those are is the table's
	// to say - this used to test one hard-coded index, so the Curse Book's own
	// second half would have been invisible.
	bool IsPlayerBotBiologistKeyPhase(LPCHARACTER ch, size_t missionIndex)
	{
		if (!ch || missionIndex >= PLAYERBOT_BIOLOGIST_MISSION_COUNT)
			return false;
		if (PLAYERBOT_BIOLOGIST_MISSIONS[missionIndex].keyItemVnum == 0)
			return false;
		const int keyState = GetPlayerBotBiologistStateIndex(missionIndex, "key_item");
		return keyState != PLAYERBOT_QUEST_STATE_UNKNOWN && ch->GetQuestFlag(GetPlayerBotBiologistFlag(
				PLAYERBOT_BIOLOGIST_MISSIONS[missionIndex], "__status")) == keyState;
	}

	// How many of a specimen the Biologist is still owed: for every row that
	// collects it, that the bot is old enough for and has neither finished
	// nor filled (a row in key_item wants the key), the rest of its count over
	// the accept roll - ten teeth at sixty percent is seventeen. Only rows from
	// PLAYERBOT_BIOLOGIST_COLLECT_QUEST_LEVEL, whose specimens are refine
	// materials; a herb is nobody's material. The anvil leaves this many alone
	// (CanPlayerBotAttemptRefineItem).
	int GetPlayerBotBiologistReserve(LPCHARACTER ch, DWORD vnum)
	{
		if (!ch || vnum == 0)
			return 0;
		int reserve = 0;
		for (size_t i = 0; i < PLAYERBOT_BIOLOGIST_MISSION_COUNT; ++i)
		{
			const TPlayerBotBiologistMission& mission = PLAYERBOT_BIOLOGIST_MISSIONS[i];
			if (mission.itemVnum != vnum ||
					mission.requiredLevel < PLAYERBOT_BIOLOGIST_COLLECT_QUEST_LEVEL ||
					ch->GetLevel() < mission.requiredLevel ||
					!IsPlayerBotHuntingMobHosted(mission.mobVnum) ||
					IsPlayerBotBiologistMissionComplete(ch, i) ||
					IsPlayerBotBiologistKeyPhase(ch, i))
				continue;
			const int accepted = std::max(0, ch->GetQuestFlag(
					GetPlayerBotBiologistFlag(mission, "collect_count")));
			const int remaining = std::max(0, (int)mission.requiredCount - accepted);
			const int percent = std::max(1, (int)mission.acceptPercent);
			reserve += (remaining * 100 + percent - 1) / percent;
		}
		return reserve;
	}

	// What the mission wants carried right now, and how many: the collection
	// item, or in a row's second half, the one key item.
	DWORD GetPlayerBotBiologistWantedItem(LPCHARACTER ch, size_t missionIndex, int* outRequired)
	{
		const TPlayerBotBiologistMission& mission = PLAYERBOT_BIOLOGIST_MISSIONS[missionIndex];
		if (IsPlayerBotBiologistKeyPhase(ch, missionIndex))
		{
			if (outRequired)
				*outRequired = 1;
			return mission.keyItemVnum;
		}
		if (outRequired)
			*outRequired = mission.requiredCount;
		return mission.itemVnum;
	}

	// Who is away on an outgrown herb row right now (pid -> since), so the
	// trip to the first village is a trickle and not the whole population
	// at once: PLAYERBOT_BIOLOGIST_HERB_TRIP_PER_MILLE of the live bots hold
	// a place, a place expires after PLAYERBOT_BIOLOGIST_HERB_ERRAND_MAX_MS
	// and is given back when the bot's herb rows are done.
	std::map<DWORD, DWORD> s_mapPlayerBotHerbErrand;
	// When a bot's last trip ended, so the place it gives back does not come
	// straight back to it (PLAYERBOT_BIOLOGIST_ERRAND_COOLDOWN_MS).
	std::map<DWORD, DWORD> s_mapPlayerBotHerbErrandDone;
	std::map<DWORD, DWORD> s_mapPlayerBotCollectErrandDone;

	// A place given back is remembered, and the bot waits its turn before it
	// may take one again. Called from every path that frees a place.
	void NotePlayerBotErrandOver(std::map<DWORD, DWORD>& done, DWORD pid, DWORD dwNow)
	{
		done[pid] = dwNow;
		// The map would otherwise grow with every bot that ever travelled.
		if (done.size() > 4096)
			done.clear();
	}

	bool IsPlayerBotErrandOnCooldown(const std::map<DWORD, DWORD>& done, DWORD pid, DWORD dwNow)
	{
		std::map<DWORD, DWORD>::const_iterator it = done.find(pid);
		return it != done.end() &&
				dwNow - it->second < PLAYERBOT_BIOLOGIST_ERRAND_COOLDOWN_MS;
	}

	bool PlayerBotHerbErrandOutgrown(LPCHARACTER ch, size_t missionIndex)
	{
		const TPlayerBotBiologistMission& mission = PLAYERBOT_BIOLOGIST_MISSIONS[missionIndex];
		return mission.mobVnum < 500 &&
				(int)ch->GetLevel() > mission.requiredLevel + PLAYERBOT_BIOLOGIST_OUTGROWN_LEVELS;
	}

	// Whether the bot may work an outgrown herb row: a place it already holds,
	// or a free one. Nothing is taken here - the row loop asks this for every
	// herb row it passes, and a place granted to a row that is not picked was
	// given back in the same call, once a tick, for as long as the map had
	// room (525 "herb errand" and 514 "over" lines in two minutes on m2zip,
	// 17 September, the moment the trial bots stopped holding every place).
	bool PlayerBotMayTakeHerbErrand(LPCHARACTER ch, size_t missionIndex, DWORD dwNow)
	{
		// A horse trial comes first: the frontier draw sends a trial bot to
		// the desert or the tower and the hunt row already yields to it
		// (GetPlayerBotBiologistHuntMob), but the herb errand did not, so a
		// bot of seventy-six walked to Joan for the fourth herb row under the
		// status "Zdobywam konia bojowego na pustyni" - 88 of 124 trial bots
		// on m2zip had the Biologist as their goal (17 September). The herbs
		// wait for the horse; a hand-in already carried still walks.
		if (IsPlayerBotOnBattleHorseTrial(ch) || IsPlayerBotOnMilitaryHorseTrial(ch))
			return false;
		const DWORD pid = ch->GetPlayerID();
		std::map<DWORD, DWORD>::iterator it = s_mapPlayerBotHerbErrand.find(pid);
		if (it != s_mapPlayerBotHerbErrand.end())
		{
			// A trip with specimens already in the bag is finished rather than
			// cut off at the hour: it was collected for a hand-in that has not
			// happened yet, and the place going back before it means the trip
			// was spent for nothing.
			int required = 0;
			const DWORD wanted = GetPlayerBotBiologistWantedItem(ch, missionIndex, &required);
			const DWORD limit = (wanted != 0 && ch->CountSpecifyItem(wanted) > 0)
					? PLAYERBOT_BIOLOGIST_HERB_ERRAND_CARRY_MAX_MS
					: PLAYERBOT_BIOLOGIST_HERB_ERRAND_MAX_MS;
			if (dwNow - it->second < limit)
				return true;
			s_mapPlayerBotHerbErrand.erase(it);
			NotePlayerBotErrandOver(s_mapPlayerBotHerbErrandDone, pid, dwNow);
		}
		// Back of the queue: a bot whose quantum has just run out waits before
		// it may take another place, so the share rotates instead of sticking
		// to whoever asks most often.
		if (IsPlayerBotErrandOnCooldown(s_mapPlayerBotHerbErrandDone, pid, dwNow))
			return false;
		// The sweep cannot ask another bot's bag, so it uses the longer of the
		// two limits; a place held a little longer is what the cap is for.
		for (std::map<DWORD, DWORD>::iterator old = s_mapPlayerBotHerbErrand.begin();
				old != s_mapPlayerBotHerbErrand.end();)
		{
			if (dwNow - old->second >= PLAYERBOT_BIOLOGIST_HERB_ERRAND_CARRY_MAX_MS)
			{
				NotePlayerBotErrandOver(s_mapPlayerBotHerbErrandDone, old->first, dwNow);
				s_mapPlayerBotHerbErrand.erase(old++);
			}
			else
				++old;
		}
		const size_t cap = std::max<size_t>(1, (size_t)GetPlayerBotsAlive() * PLAYERBOT_BIOLOGIST_HERB_TRIP_PER_MILLE / 1000);
		return s_mapPlayerBotHerbErrand.size() < cap;
	}

	// Where a bot started its stay in a first village (pid -> map, since), for
	// the catch-up that costs the world nothing: a bot that is in a first
	// village anyway - services, the market, a hand-in - may work an outgrown
	// herb row while it is there, without taking a place on the errand, since
	// it adds no map change at all. The share above is what bounds the travel
	// to M1; this is what fills the rows in ("jak mozna bezpiecznie zrobic by
	// boty nadrabialy sobie biologa", Tieru, 17 September).
	std::map<DWORD, std::pair<long, DWORD> > s_mapPlayerBotHerbVillageStay;

	// True while that window is open. It is bounded per arrival -
	// PLAYERBOT_BIOLOGIST_HERB_VILLAGE_MS from the first ask on that map - and
	// opens again only after the bot has been somewhere else: without the
	// bound, the bots an update draws into the villages stay for all six rows,
	// which is exactly what the errand share was written for. Called once per
	// mission choice, never inside the row loop: it starts the window, and a
	// gate consulted inside a loop must not have a side effect.
	bool PlayerBotMayWorkHerbRowHere(LPCHARACTER ch, DWORD dwNow)
	{
		if (!ch)
			return false;
		const DWORD pid = ch->GetPlayerID();
		if (!IsPlayerBotM1Map(ch->GetMapIndex()))
		{
			s_mapPlayerBotHerbVillageStay.erase(pid);
			return false;
		}
		std::map<DWORD, std::pair<long, DWORD> >::iterator it =
				s_mapPlayerBotHerbVillageStay.find(pid);
		if (it == s_mapPlayerBotHerbVillageStay.end() || it->second.first != ch->GetMapIndex())
		{
			s_mapPlayerBotHerbVillageStay[pid] = std::make_pair(ch->GetMapIndex(), dwNow);
			return true;
		}
		return dwNow - it->second.second < PLAYERBOT_BIOLOGIST_HERB_VILLAGE_MS;
	}

	// Who is away on an outgrown collect row (pid -> since): the valley and the
	// tower, PLAYERBOT_BIOLOGIST_COLLECT_TRIP_PER_MILLE of the live bots at a
	// time. The frontier draw sends a bot with such a row open where its
	// monster stands whatever its level (GetPlayerBotFrontierMapForLevelRaw),
	// and on 17 September that was 997 of 1621 bots in Orc Valley on
	// SIZOWSKI's world, 277 of 1098 on m2zip, every other map empty.
	std::map<DWORD, DWORD> s_mapPlayerBotCollectErrand;

	bool PlayerBotCollectErrandOutgrown(LPCHARACTER ch, size_t missionIndex)
	{
		const TPlayerBotBiologistMission& mission = PLAYERBOT_BIOLOGIST_MISSIONS[missionIndex];
		return mission.mobVnum >= 500 &&
				mission.requiredLevel >= PLAYERBOT_BIOLOGIST_COLLECT_QUEST_LEVEL &&
				(int)ch->GetLevel() > mission.requiredLevel + PLAYERBOT_BIOLOGIST_OUTGROWN_LEVELS;
	}

	// A place held, or a free one - nothing taken, the same shape as the herb
	// errand's question above and for the same reason.
	bool PlayerBotMayTakeCollectErrand(LPCHARACTER ch, DWORD dwNow)
	{
		if (IsPlayerBotOnBattleHorseTrial(ch) || IsPlayerBotOnMilitaryHorseTrial(ch))
			return false;
		const DWORD pid = ch->GetPlayerID();
		std::map<DWORD, DWORD>::iterator it = s_mapPlayerBotCollectErrand.find(pid);
		if (it != s_mapPlayerBotCollectErrand.end())
		{
			if (dwNow - it->second < PLAYERBOT_BIOLOGIST_COLLECT_ERRAND_MAX_MS)
				return true;
			s_mapPlayerBotCollectErrand.erase(it);
			NotePlayerBotErrandOver(s_mapPlayerBotCollectErrandDone, pid, dwNow);
		}
		if (IsPlayerBotErrandOnCooldown(s_mapPlayerBotCollectErrandDone, pid, dwNow))
			return false;
		for (std::map<DWORD, DWORD>::iterator old = s_mapPlayerBotCollectErrand.begin();
				old != s_mapPlayerBotCollectErrand.end();)
		{
			if (dwNow - old->second >= PLAYERBOT_BIOLOGIST_COLLECT_ERRAND_MAX_MS)
			{
				NotePlayerBotErrandOver(s_mapPlayerBotCollectErrandDone, old->first, dwNow);
				s_mapPlayerBotCollectErrand.erase(old++);
			}
			else
				++old;
		}
		const size_t cap = std::max<size_t>(1, (size_t)GetPlayerBotsAlive() * PLAYERBOT_BIOLOGIST_COLLECT_TRIP_PER_MILLE / 1000);
		return s_mapPlayerBotCollectErrand.size() < cap;
	}

	void PlayerBotTakeCollectErrand(LPCHARACTER ch, size_t missionIndex, DWORD dwNow)
	{
		const DWORD pid = ch->GetPlayerID();
		if (s_mapPlayerBotCollectErrand.count(pid))
			return;
		s_mapPlayerBotCollectErrand[pid] = dwNow;
		const size_t cap = std::max<size_t>(1, (size_t)GetPlayerBotsAlive() * PLAYERBOT_BIOLOGIST_COLLECT_TRIP_PER_MILLE / 1000);
		sys_log(0, "PLAYERBOT_BIOLOGIST: collect errand pid=%u name=%s level=%d row=%u map=%ld away=%u/%u",
				pid, ch->GetName(), (int)ch->GetLevel(), (unsigned int)missionIndex, ch->GetMapIndex(),
				(unsigned int)s_mapPlayerBotCollectErrand.size(), (unsigned int)cap);
	}

	// The place itself, taken once the pick is an outgrown herb row.
	void PlayerBotTakeHerbErrand(LPCHARACTER ch, size_t missionIndex, DWORD dwNow)
	{
		const DWORD pid = ch->GetPlayerID();
		if (s_mapPlayerBotHerbErrand.count(pid))
			return;
		s_mapPlayerBotHerbErrand[pid] = dwNow;
		const size_t cap = std::max<size_t>(1, (size_t)GetPlayerBotsAlive() * PLAYERBOT_BIOLOGIST_HERB_TRIP_PER_MILLE / 1000);
		sys_log(0, "PLAYERBOT_BIOLOGIST: herb errand pid=%u name=%s level=%d row=%u map=%ld away=%u/%u",
				pid, ch->GetName(), (int)ch->GetLevel(), (unsigned int)missionIndex, ch->GetMapIndex(),
				(unsigned int)s_mapPlayerBotHerbErrand.size(), (unsigned int)cap);
	}

	// `mayReserve` is what separates reading the plan from changing it. The
	// errand places below are a queue over the live population, and this
	// function used to take them and give them back on EVERY call - including
	// the ones the panel makes, because BuildPlayerBotStatusText asks it for
	// the line over a bot's head. Looking at a bot could hand it a trip or end
	// one ("GetActive... nie jest czystym odczytem", audit of 17 September).
	// Only the passes that actually decide to travel pass true; every reader -
	// the status, the planner, the target picker, the kill note - leaves the
	// queue exactly as it found it.
	const TPlayerBotBiologistMission* GetActivePlayerBotBiologistMission(
			LPCHARACTER ch, size_t* outIndex = NULL, bool mayReserve = false)
	{
		if (!ch)
			return NULL;
		// A dropper is a drop character: its table, its gear and its counter,
		// and no quests ("jesli to osobowosc typowo dropek medali to powinien
		// sie skupic tylko na lochu i eq ... a nie na robieniu questow", Tieru,
		// 15 September - dropki of twenty-five doing the Biologist in M2). The
		// planner, the pass, the travel and the status all ask this.
		if (IsPlayerBotDropper(GetPlayerBotPersonalityByPID(ch->GetPlayerID())))
			return NULL;
		// Four passes: a mission whose specimens the bot already carries, then
		// one whose monster stands on this map, then the first left undone that
		// the bot has not outgrown, and last the highest one it has left. A bot
		// that outgrew the mushrooms and lives among the Orcs collects teeth
		// instead of collecting nothing - and hands in what it carries before the
		// map it hands in on offers it something else.
		//
		// The fourth pass is what the Discord was missing. "First left undone"
		// on its own hands a Sura of forty-two the Gango Root of level fifteen,
		// whose monster stands in Joan; it never goes there, so the goal reads
		// "Korzen Gango 0/5" while the bot hits Orcs and the whole chain stops
		// at that row for the rest of the world too. The rows are seven separate
		// quests, not one chain, so an outgrown row is stepped over rather than
		// blocking the ones behind it.
		int carrying = -1, here = -1, first = -1, last = -1;
		// Asked once, before the loop: it opens the village window, and a gate
		// consulted inside a loop must not have a side effect.
		const DWORD dwNowHerb = get_dword_time();
		const bool herbHere = PlayerBotMayWorkHerbRowHere(ch, dwNowHerb);
		for (size_t i = 0; i < PLAYERBOT_BIOLOGIST_MISSION_COUNT; ++i)
		{
			const TPlayerBotBiologistMission& mission = PLAYERBOT_BIOLOGIST_MISSIONS[i];
			if (ch->GetLevel() < mission.requiredLevel)
				break;
			if (IsPlayerBotBiologistMissionComplete(ch, i))
				continue;
			// A chain row waits for the one before it (IsPlayerBotBiologistMissionOpen).
			if (!IsPlayerBotBiologistMissionOpen(ch, i))
				continue;
			// A row whose monster stands on no map this world hosts can never
			// be finished, and choosing it means saying so above the bot's head
			// for ever. The Demon Souvenir is that row here: 1001 lives only on
			// map 66, which game2 hosts and no bot can reach. It has to be
			// stepped over by every pass including `last`, which is the one
			// that takes the highest row left when everything else is outgrown
			// - otherwise every bot of fifty would read "Pamiatka Po Demonie
			// 0/15" exactly the way they once all read "Korzen Gango 0/5".
			if (!IsPlayerBotHuntingMobHosted(mission.mobVnum))
				continue;
			// An outgrown first-village herb row is a stay in the first village
			// - and those are a trickle (PLAYERBOT_BIOLOGIST_HERB_TRIP_PER_MILLE),
			// wherever the bot stands: with the place free for a bot already
			// there, the 483 bots the update had drawn into the first villages
			// stayed for all six rows. A bot without a place steps over the row,
			// in every pass, and takes what stands next in order - which sends a
			// bot of fifty out of Joan the way it always left; a hand-in it
			// already holds waits for the place too. A bot standing in a first
			// village works the row without a place (herbHere): that is the
			// catch-up, and it costs no map change.
			//
			// Carrying outranks the queue, because handing in what the bag
			// already holds is NOT a trip: the share limits who travels for a
			// row, and a bot that walks past the Biologist with five of his
			// specimens helps nobody. The gates below used to stand in front of
			// the `carrying` test further down, so a bot without a place
			// stepped over its own half-finished row and kept the specimens
			// (audit of 17 September, A.3).
			int heldRequired = 0;
			const DWORD heldWanted = GetPlayerBotBiologistWantedItem(ch, i, &heldRequired);
			const bool holdsSpecimens = heldWanted != 0 &&
					ch->CountSpecifyItem(heldWanted) > 0;
			if (!holdsSpecimens && PlayerBotHerbErrandOutgrown(ch, i) && !herbHere &&
					!PlayerBotMayTakeHerbErrand(ch, i, dwNowHerb))
				continue;
			// And an outgrown collect row is a stay in the valley or the tower,
			// PLAYERBOT_BIOLOGIST_COLLECT_TRIP_PER_MILLE of the world at a time.
			if (!holdsSpecimens && PlayerBotCollectErrandOutgrown(ch, i) &&
					!PlayerBotMayTakeCollectErrand(ch, get_dword_time()))
				continue;
			last = (int)i;
			// No row is ever "too low": rows are done in order, whatever the
			// bot's level, and a bot of seventy-eight with the Gango Root undone
			// goes back to Joan for it. Until 2.0.60 a row more than
			// PLAYERBOT_BIOLOGIST_OUTGROWN_LEVELS under the bot was stepped
			// over by the middle passes, so the panel read "Zab Orka 4/10, za
			// niskie dla bota, pominiete: 4" over a bot that would never finish
			// either ("nie ma czegos takiego jak za niskie dla bota", Tieru,
			// 16 September). The travel and the wander take the bot to the
			// row's monster (playerbot_travel.h, playerbot_wandering.h).
			if (first < 0)
				first = (int)i;
			int required = 0;
			const DWORD wanted = GetPlayerBotBiologistWantedItem(ch, i, &required);
			// "Carrying" still comes first: a row half done is finished before
			// an earlier one is begun, which is what "niech je zrobi do konca by
			// przejsc do kolejnej misji" asks for.
			const int held = ch->CountSpecifyItem(wanted);
			if (carrying < 0 && held > 0)
				carrying = (int)i;
			if (here < 0 && IsPlayerBotHuntingMobHosted(mission.mobVnum, ch->GetMapIndex()))
				here = (int)i;
		}
		// The row whose specimens the bag holds, else the first in order; a
		// row whose monster happens to stand on this map is taken ahead of an
		// earlier one only when the earlier one is not yet begun, because a
		// trip for it is the same trip either way.
		const int pick = carrying >= 0 ? carrying
				: (here >= 0 ? here : (first >= 0 ? first : last));
		// A place on the herb errand is given back the moment the bot's
		// active row is not a first-village one any more.
		if (mayReserve && (pick < 0 || PLAYERBOT_BIOLOGIST_MISSIONS[pick].mobVnum >= 500) &&
				s_mapPlayerBotHerbErrand.erase(ch->GetPlayerID()) != 0)
			sys_log(0, "PLAYERBOT_BIOLOGIST: herb errand over pid=%u name=%s away=%u",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)s_mapPlayerBotHerbErrand.size());
		// A collect place is given back when the row picked is no longer an
		// outgrown collect row - the Orc Tooth handed in and the Curse Book
		// next keeps it, the whole chain done gives it back.
		if (mayReserve && (pick < 0 || !PlayerBotCollectErrandOutgrown(ch, (size_t)pick)) &&
				s_mapPlayerBotCollectErrand.erase(ch->GetPlayerID()) != 0)
			sys_log(0, "PLAYERBOT_BIOLOGIST: collect errand over pid=%u name=%s away=%u",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)s_mapPlayerBotCollectErrand.size());
		if (pick < 0)
			return NULL;
		// The place is taken for the row picked, and only then - and never by a
		// bot that is only working the row because it stands in the village
		// anyway, or the share would be spent on trips nobody makes.
		if (mayReserve && PlayerBotHerbErrandOutgrown(ch, (size_t)pick) && !herbHere)
			PlayerBotTakeHerbErrand(ch, (size_t)pick, dwNowHerb);
		if (mayReserve && PlayerBotCollectErrandOutgrown(ch, (size_t)pick))
			PlayerBotTakeCollectErrand(ch, (size_t)pick, get_dword_time());
		if (outIndex)
			*outIndex = (size_t)pick;
		return &PLAYERBOT_BIOLOGIST_MISSIONS[pick];
	}

	bool HasPlayerBotCompletedEarlyBiologist(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		for (size_t i = 0; i < PLAYERBOT_BIOLOGIST_MISSION_COUNT; ++i)
		{
			if (!IsPlayerBotBiologistMissionComplete(ch, i))
				return false;
		}
		return true;
	}

	// The bag holds what the next hand-in takes: the same threshold the hand-in
	// itself uses, or the trip to Joan would never start for a bot the
	// Biologist would happily serve.
	bool PlayerBotBiologistHoldsHandIn(LPCHARACTER ch, const TPlayerBotBiologistMission* mission, size_t missionIndex)
	{
		if (!ch || !mission)
			return false;
		int required = mission->requiredCount;
		const DWORD wantedVnum = GetPlayerBotBiologistWantedItem(ch, missionIndex, &required);
		const int accepted = IsPlayerBotBiologistKeyPhase(ch, missionIndex) ? 0 : std::max(0, ch->GetQuestFlag(
				GetPlayerBotBiologistFlag(*mission, "collect_count")));
		const int remaining = std::max(0, required - accepted);
		return remaining > 0 && ch->CountSpecifyItem(wantedVnum) >=
				std::min(remaining, PLAYERBOT_BIOLOGIST_MIN_HANDIN);
	}

	// The monster the active row still wants killed, or zero: the row's own
	// while specimens are short, the key's monster in the key phase, nothing
	// while the bag already holds the hand-in.
	// `mayReserve` is passed straight through to the mission read: the
	// frontier draw is the third place a trip is decided (the collect rows
	// live in Orc Valley and the Demon Tower), and it must be able to take a
	// place or the share those rows are limited to would stop being a limit.
	DWORD GetPlayerBotBiologistHuntMob(LPCHARACTER ch, bool mayReserve = false)
	{
		// The horse trial comes first. A bot of seventy-seven with its horse
		// at ten read "Zdobywam konia bojowego na pustyni (0/100)" in Jayang
		// for the whole evening (Tieru, 16 September): the Gango Root's monster
		// stands in the first village, so the herb row's hunt sent it there
		// through NeedsPlayerBotM1OnlyServices ahead of the frontier draw, which
		// wanted the desert - 35 such bots on map 3, and the same on every
		// other village map. While a horse trial is open the row's monster is
		// not a destination; the row waits, and the hand-in still walks.
		if (IsPlayerBotOnBattleHorseTrial(ch) || IsPlayerBotOnMilitaryHorseTrial(ch))
			return 0;
		size_t missionIndex = 0;
		const TPlayerBotBiologistMission* mission =
				GetActivePlayerBotBiologistMission(ch, &missionIndex, mayReserve);
		if (!mission || PlayerBotBiologistHoldsHandIn(ch, mission, missionIndex))
			return 0;
		return IsPlayerBotBiologistKeyPhase(ch, missionIndex) ? mission->keyMobVnum : mission->mobVnum;
	}

	// A carrier of the active row's specimen died to this bot (the kill note in
	// playerbot_battle_horse.h). The engine has already rolled its etc drop
	// with the level gap in it - one percent of the chance at fifteen levels
	// over the monster - and a row is done at any level ("nie ma czegos
	// takiego jak za niskie dla bota", Tieru, 16 September), so the part the
	// gap took is rolled here: GetDropPct's own percent, which carries the
	// world's rate and the premium, times (100 - fade) / fade. A bot at its
	// row's level gets nothing extra and a bot of seventy what a player of the
	// monster's level gets. Only while the Biologist is still owed the item
	// (GetPlayerBotBiologistReserve), and never onto the ground.
	void NotePlayerBotBiologistCarrierKill(LPCHARACTER ch, LPCHARACTER target)
	{
		if (!ch || !target || !target->IsMonster())
			return;
		const DWORD race = target->GetRaceNum();
		bool carrier = false;
		for (size_t i = 0; i < sizeof(PLAYERBOT_BIOLOGIST_SPECIMEN_CARRIERS) /
				sizeof(PLAYERBOT_BIOLOGIST_SPECIMEN_CARRIERS[0]) && !carrier; ++i)
			carrier = PLAYERBOT_BIOLOGIST_SPECIMEN_CARRIERS[i].mobVnum == race;
		if (!carrier)
			return;
		size_t missionIndex = 0;
		const TPlayerBotBiologistMission* mission = GetActivePlayerBotBiologistMission(ch, &missionIndex);
		if (!mission || IsPlayerBotBiologistKeyPhase(ch, missionIndex))
			return;
		DWORD prob = 0;
		for (size_t i = 0; i < sizeof(PLAYERBOT_BIOLOGIST_SPECIMEN_CARRIERS) /
				sizeof(PLAYERBOT_BIOLOGIST_SPECIMEN_CARRIERS[0]); ++i)
			if (PLAYERBOT_BIOLOGIST_SPECIMEN_CARRIERS[i].mobVnum == race &&
					PLAYERBOT_BIOLOGIST_SPECIMEN_CARRIERS[i].itemVnum == mission->itemVnum)
				prob = PLAYERBOT_BIOLOGIST_SPECIMEN_CARRIERS[i].dropProb;
		if (prob == 0)
			return;
		const int fade = PERCENT_LVDELTA(ch->GetLevel(), target->GetLevel());
		if (fade >= 100 || fade <= 0)
			return;
		if ((int)ch->CountSpecifyItem(mission->itemVnum) >=
				GetPlayerBotBiologistReserve(ch, mission->itemVnum))
			return;
		int deltaPercent = 0, randRange = 0;
		if (!ITEM_MANAGER::instance().GetDropPct(target, ch, deltaPercent, randRange) || randRange <= 0)
			return;
		const long long gapPercent = (long long)deltaPercent * (100 - fade) / fade;
		const long long chance = (long long)prob * gapPercent / 100;
		if (chance < number(1, randRange))
			return;
		if (ch->GetEmptyInventory(1) < 0)
			return;
		ch->AutoGiveItem(mission->itemVnum, 1, -1, false);
		PlayerBotLogThrottled("biologist_level_gap", get_dword_time(),
				"PLAYERBOT_BIOLOGIST: specimen past the level gap pid=%u name=%s level=%d item=%u mob=%u mob_level=%d fade=%d",
				ch->GetPlayerID(), ch->GetName(), (int)ch->GetLevel(), mission->itemVnum,
				race, (int)target->GetLevel(), fade);
	}

	// A first-village herb row is being hunted: the reason to go to a first
	// village (NeedsPlayerBotM1OnlyServices) and, once there, the reason to
	// stay. The M1 branch of the world travel did not ask, and a bot of forty
	// with a place on the herb errand crossed Joan <-> Bokjung every four
	// seconds - "level_to_m2" out, "m1_only_service" back - because both
	// gates' arrival points stand beside the return gate (Greess, Logi.txt,
	// 16 September, "nie przechodza przez teleporty").
	bool PlayerBotHuntsVillageHerbs(LPCHARACTER ch)
	{
		const DWORD mob = GetPlayerBotBiologistHuntMob(ch);
		return mob != 0 && mob < 500;
	}

	// The level the first village's hubs are chosen for: the active herb row's
	// own level while its monster is wanted - a bot of seventy-eight after the
	// Gango Root stands where the Gango Root's monster is, not with the
	// tigers - and the bot's otherwise.
	int GetPlayerBotVillageHuntLevel(LPCHARACTER ch)
	{
		if (!ch)
			return 1;
		size_t missionIndex = 0;
		const TPlayerBotBiologistMission* mission = GetActivePlayerBotBiologistMission(ch, &missionIndex);
		if (!mission || mission->mobVnum >= 500 || IsPlayerBotBiologistKeyPhase(ch, missionIndex) ||
				PlayerBotBiologistHoldsHandIn(ch, mission, missionIndex))
			return ch->GetLevel();
		return std::max<int>(1, mission->requiredLevel);
	}

	bool EnsurePlayerBotBiologistMissionStarted(LPCHARACTER ch, size_t missionIndex)
	{
		if (!ch || missionIndex >= PLAYERBOT_BIOLOGIST_MISSION_COUNT)
			return false;
		const TPlayerBotBiologistMission& mission = PLAYERBOT_BIOLOGIST_MISSIONS[missionIndex];
		const int collectingState = GetPlayerBotBiologistStateIndex(missionIndex, "go_to_disciple");
		if (collectingState == PLAYERBOT_QUEST_STATE_UNKNOWN)
			return false;

		const std::string statusFlag = GetPlayerBotBiologistFlag(mission, "__status");
		if (IsPlayerBotBiologistKeyPhase(ch, missionIndex))
			return true;
		if (ch->GetQuestFlag(statusFlag) != collectingState)
		{
			quest::PC* pc = quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID());
			if (!pc)
				return false;
			pc->SetQuestState(mission.questName, collectingState);
			ch->SetQuestFlag(GetPlayerBotBiologistFlag(mission, "collect_count"), 0);
			ch->SetQuestFlag(GetPlayerBotBiologistFlag(mission, "drink_drug"), 0);
			sys_log(0, "PLAYERBOT_BIOLOGIST: mission started pid=%u name=%s quest=%s item=%u mob=%u need=%u",
					ch->GetPlayerID(), ch->GetName(), mission.questName,
					mission.itemVnum, mission.mobVnum, mission.requiredCount);
		}
		return true;
	}

	const TPlayerBotHuntingMission* GetActivePlayerBotHuntingMission(
			LPCHARACTER ch, int* outLevel = NULL, int* outSelection = NULL,
			int* outRemaining = NULL)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		// The player level-up hunt (levelup.quest) ships in quest/_unused on
		// this line: no kill hook fires, so levelup.remain never decrements
		// and every bot reads "0/40" for good, while the mission steered
		// under-geared bots at its target mob. Disabled here; bots hunt by
		// the frontier draw and the level-banded hubs instead (Tieru, 13
		// September).
		(void)outLevel; (void)outSelection; (void)outRemaining;
		return NULL;
#endif
		if (!ch)
			return NULL;
		const int level = ch->GetQuestFlag("levelup.current");
		if (level < PLAYERBOT_HUNTING_FIRST_LEVEL ||
				level > PLAYERBOT_HUNTING_MAX_LEVEL || level > ch->GetLevel())
			return NULL;

		const int selection = ch->GetQuestFlag("levelup.select") == 2 ? 2 : 1;
		if (outLevel)
			*outLevel = level;
		if (outSelection)
			*outSelection = selection;
		if (outRemaining)
			*outRemaining = std::max(0, ch->GetQuestFlag("levelup.remain"));
		return &PLAYERBOT_HUNTING_MISSIONS[level];
	}

	DWORD GetActivePlayerBotHuntingMobVnum(LPCHARACTER ch, int* outRemaining = NULL)
	{
		int selection = 1;
		int remaining = 0;
		const TPlayerBotHuntingMission* mission = GetActivePlayerBotHuntingMission(
				ch, NULL, &selection, &remaining);
		if (outRemaining)
			*outRemaining = remaining;
		if (!mission || remaining <= 0)
			return 0;
		return selection == 2 ? mission->secondMobVnum : mission->firstMobVnum;
	}

	void GivePlayerBotHuntingReward(LPCHARACTER ch, int missionLevel)
	{
		if (!ch || missionLevel < PLAYERBOT_HUNTING_FIRST_LEVEL ||
				missionLevel > PLAYERBOT_HUNTING_MAX_LEVEL)
			return;

		DWORD rewardItem = 0;
		DWORD rewardCount = 1;
		if (missionLevel == 2)
		{
			const DWORD rewards[] = { 11200, 11400, 11600, 11800 };
			rewardItem = rewards[std::min<int>(ch->GetJob(), JOB_SHAMAN)];
		}
		else if (missionLevel == 3)
		{
			const DWORD rewards[] = { 12200, 12340, 12480, 12620 };
			rewardItem = rewards[std::min<int>(ch->GetJob(), JOB_SHAMAN)];
		}
		else if (missionLevel == 4)
			rewardItem = 13000;
		else if (missionLevel <= 21 || missionLevel == 25)
		{
			const int roll = number(1, 100);
			rewardItem = roll <= 33 ? 27002 : (roll <= 67 ? 27005 : 27114);
			rewardCount = rewardItem == 27114 ? 5 : 10;
		}
		else if (missionLevel >= 22 && missionLevel <= 24)
		{
			const DWORD bases[] = { 15080, 16080, 17080 };
			rewardItem = bases[missionLevel - 22] + number(0, 3) * 20;
		}

		if (rewardItem != 0)
			ch->AutoGiveItem(rewardItem, rewardCount, -1, false);
		if (missionLevel == 12 || missionLevel == 14 || missionLevel == 16 ||
				missionLevel == 18 || missionLevel == 20)
			ch->AutoGiveItem(50083, 1, -1, false);

		int expPercent = PLAYERBOT_HUNTING_MISSIONS[missionLevel].expPercent;
		DWORD rewardGold = 0;
		if (missionLevel >= 21)
		{
			const int goldRoll = number(0, 99);
			rewardGold = goldRoll < 20 ? 10000 :
					(goldRoll < 70 ? 20000 : (goldRoll < 95 ? 40000 :
					(goldRoll < 98 ? 80000 : 100000)));

			const int expRoll = number(0, 98);
			expPercent = expRoll < 9 ? 2 : (expRoll < 23 ? 3 :
					(expRoll < 62 ? 4 : (expRoll < 86 ? 6 :
					(expRoll < 95 ? 8 : 10))));
			// questlib narrows the range as the levels climb: "2-5" from 31,
			// "1-4" from 51.
			if (missionLevel >= 51)
				expPercent = std::min(expPercent, number(1, 4));
			else if (missionLevel >= 31)
				expPercent = std::min(expPercent, number(2, 5));
		}

		if (rewardGold > 0)
			PlayerBotChangeGold(ch, rewardGold);
		if (expPercent > 0)
		{
			const DWORD rewardExp = (DWORD)(((unsigned long long)
					exp_table[MINMAX(0, missionLevel, PLAYER_EXP_TABLE_MAX)] *
					expPercent) / 100);
			if (rewardExp > 0)
				ch->PointChange(POINT_EXP, rewardExp, true);
		}
	}

	void StartPlayerBotHuntingMission(LPCHARACTER ch, int missionLevel)
	{
		if (!ch || missionLevel < PLAYERBOT_HUNTING_FIRST_LEVEL ||
				missionLevel > PLAYERBOT_HUNTING_MAX_LEVEL || missionLevel > ch->GetLevel())
			return;

		static int s_startState = -1;
		if (s_startState < 0)
			s_startState = quest::CQuestManager::instance().GetQuestStateIndex(
					"levelup", "start");
		quest::PC* pc = quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID());
		if (pc && s_startState >= 0)
			pc->SetQuestState("levelup", s_startState);

		const TPlayerBotHuntingMission& mission =
				PLAYERBOT_HUNTING_MISSIONS[missionLevel];
		// The option that stands where the bot is; else one that stands anywhere
		// hosted; else the pid decides, as it always did for the rows where both
		// options are next door.
		int selection = ((ch->GetPlayerID() + missionLevel) % 2) + 1;
		const bool firstHere = IsPlayerBotHuntingMobHosted(mission.firstMobVnum, ch->GetMapIndex());
		const bool secondHere = IsPlayerBotHuntingMobHosted(mission.secondMobVnum, ch->GetMapIndex());
		const bool firstAnywhere = IsPlayerBotHuntingMobHosted(mission.firstMobVnum);
		const bool secondAnywhere = IsPlayerBotHuntingMobHosted(mission.secondMobVnum);
		if (firstHere != secondHere)
			selection = firstHere ? 1 : 2;
		else if (firstAnywhere != secondAnywhere)
			selection = firstAnywhere ? 1 : 2;
		ch->SetQuestFlag("levelup.botsince", (int)get_global_time());
		const int count = selection == 2 ? mission.secondCount : mission.firstCount;
		ch->SetQuestFlag("levelup.current", missionLevel);
		ch->SetQuestFlag("levelup.select", selection);
		ch->SetQuestFlag("levelup.remain", count);
		// levelup.quest decrements kills only after the human has clicked Confirm.
		// A playerbot has no quest UI, so -1 represents that exact accepted state.
		ch->SetQuestFlag("levelup.buttonstate", -1);
		sys_log(0, "PLAYERBOT_HUNTING: accepted pid=%u name=%s mission_level=%d select=%d mob=%u count=%d",
				ch->GetPlayerID(), ch->GetName(), missionLevel, selection,
				selection == 2 ? mission.secondMobVnum : mission.firstMobVnum, count);
	}

	void ManagePlayerBotHuntingProgress(LPCHARACTER ch)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		(void)ch;   // the level-up hunt is disabled on this line, see above.
		return;
#endif
		if (!ch || ch->GetLevel() < PLAYERBOT_HUNTING_FIRST_LEVEL)
			return;

		int current = ch->GetQuestFlag("levelup.current");
		const int completed = std::max(0, ch->GetQuestFlag("levelup.complete"));
		if (current == 0)
		{
			int next = std::max<int>(PLAYERBOT_HUNTING_FIRST_LEVEL, completed + 1);
			// Rows with nothing to hunt on any hosted map are passed over, so the
			// rows after them stay reachable. No reward for a hunt not hunted.
			// On a frontier map the bot is there to stay, so a row whose monsters
			// are all elsewhere is passed over too: the ones at Orc Valley were
			// found holding Sohan missions with the count untouched.
			const long mapIndex = ch->GetMapIndex();
			const bool settled = IsPlayerBotFrontierMapIndex(mapIndex);
			const char* why = NULL;
			while (next <= PLAYERBOT_HUNTING_MAX_LEVEL && next <= ch->GetLevel())
			{
				const TPlayerBotHuntingMission& row = PLAYERBOT_HUNTING_MISSIONS[next];
				if (next + PLAYERBOT_HUNTING_OUTGROWN_LEVELS < ch->GetLevel())
					why = "outgrown";
				else if (!IsPlayerBotHuntingMobHosted(row.firstMobVnum) &&
						!IsPlayerBotHuntingMobHosted(row.secondMobVnum))
					why = "no hosted monster";
				else if (settled && !IsPlayerBotHuntingMobHosted(row.firstMobVnum, mapIndex) &&
						!IsPlayerBotHuntingMobHosted(row.secondMobVnum, mapIndex))
					why = "elsewhere";
				else
					break;
				sys_log(0, "PLAYERBOT_HUNTING: passed over pid=%u name=%s mission_level=%d level=%d map=%ld (%s)",
						ch->GetPlayerID(), ch->GetName(), next, ch->GetLevel(), mapIndex, why);
				ch->SetQuestFlag("levelup.complete", next);
				++next;
			}
			if (next <= PLAYERBOT_HUNTING_MAX_LEVEL && next <= ch->GetLevel())
				StartPlayerBotHuntingMission(ch, next);
			return;
		}

		if (current < PLAYERBOT_HUNTING_FIRST_LEVEL ||
				current > PLAYERBOT_HUNTING_MAX_LEVEL || current > ch->GetLevel())
			return;

		const int remain = ch->GetQuestFlag("levelup.remain");
		if (remain > 0)
		{
			// Accepted long ago and not finished: the monster is somewhere this
			// bot is not going. Pass it over rather than hold every row after it.
			const int since = ch->GetQuestFlag("levelup.botsince");
			if (since <= 0)
				ch->SetQuestFlag("levelup.botsince", (int)get_global_time());
			const bool outgrown = current + PLAYERBOT_HUNTING_OUTGROWN_LEVELS < ch->GetLevel();
			const DWORD chosenMob = GetActivePlayerBotHuntingMobVnum(ch);
			const bool elsewhere = IsPlayerBotFrontierMapIndex(ch->GetMapIndex()) && chosenMob != 0 &&
					!IsPlayerBotHuntingMobHosted(chosenMob, ch->GetMapIndex());
			if (outgrown || elsewhere ||
					(since > 0 && (int)get_global_time() - since > PLAYERBOT_HUNTING_STALL_SECONDS))
			{
				sys_log(0, "PLAYERBOT_HUNTING: passed over pid=%u name=%s mission_level=%d level=%d map=%ld remain=%d (%s)",
						ch->GetPlayerID(), ch->GetName(), current, ch->GetLevel(), ch->GetMapIndex(), remain,
						outgrown ? "outgrown" : (elsewhere ? "elsewhere" : "stalled"));
				ch->SetQuestFlag("levelup.complete", current);
				ch->SetQuestFlag("levelup.current", 0);
				ch->SetQuestFlag("levelup.remain", 0);
				ch->SetQuestFlag("levelup.buttonstate", 0);
				return;
			}
			// Existing bots reached buttonstate=1 at login and waited forever for a
			// click. Accept once, preserving a mission already in progress.
			if (ch->GetQuestFlag("levelup.buttonstate") != -1)
			{
				if (remain == (int)PLAYERBOT_HUNTING_MISSIONS[current].firstCount &&
						ch->GetQuestFlag("levelup.select") == 1)
					StartPlayerBotHuntingMission(ch, current);
				else
					ch->SetQuestFlag("levelup.buttonstate", -1);
			}
			return;
		}

		if (completed != current)
		{
			GivePlayerBotHuntingReward(ch, current);
			ch->SetQuestFlag("levelup.complete", current);
			sys_log(0, "PLAYERBOT_HUNTING: completed pid=%u name=%s mission_level=%d",
					ch->GetPlayerID(), ch->GetName(), current);
		}

		const int next = current + 1;
		if (next <= PLAYERBOT_HUNTING_MAX_LEVEL && next <= ch->GetLevel())
			StartPlayerBotHuntingMission(ch, next);
		else
		{
			ch->SetQuestFlag("levelup.current", 0);
			ch->SetQuestFlag("levelup.remain", 0);
			ch->SetQuestFlag("levelup.buttonstate", 0);
		}
	}
}

#endif
