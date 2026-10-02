#ifndef __INC_METIN2_PLAYERBOT_PROGRESSION_H__
#define __INC_METIN2_PLAYERBOT_PROGRESSION_H__

// MT2009_PLUS_PROGRESSION_V1: the engine's half of playerbot_progression_rules.h
// - the file the Seban panel writes, the checklist held against every ordinary
// bot, the push towards what it lacks, the fishing cap, and the status file
// the panel reads back ("kto stoi i dlaczego").
//
// The hold is the engine's AFFECT_EXP_BLOCK through ManagePlayerBotExpLock,
// the same lock a dropper and a Grinder at its tier carry: a bot may reach a
// gate's level and earns no experience there until the gate's requirements
// are met. Not for a player's companion (it follows its owner), a shouter, a
// dropper (its own lock), the operator's medal cohort, the Arezzo and Ochao
// test cohorts.
//
// So that the world never freezes on a requirement nobody can meet - an empty
// market, a map another core hosts - a bot that has stood timeoutMin minutes
// of play at one gate is let through, the gate written down as given up in
// its quest flags (playerbot.prog_waived) and logged.
//
// What a held bot does about it (the push), by requirement:
//   gear (weapon/armour/... level and plus) - its ambition becomes EQUIPMENT,
//     so the planner offers the blacksmith; REFINE's weight gate is open for
//     it; the equipment, weapon-goal and market passes do the buying as ever;
//   hp from items - EQUIPMENT as well, and a worn piece that can take a health
//     line (body, boots, bracelet, necklace) and has none is one the bonus pass
//     wants to change (PlayerBotWantsBonusChange);
//   skills - SKILLS; it buys the books of its own skills whatever its gear
//     (PlayerBotStudiesBooks) and SKILL's gate is open;
//   horse - HORSE; the medal hunt and the battle/military horse trials are
//     the stable's own (playerbot_battle_horse.h);
//   Metins - METINS, and a Metin expedition starts at once where the map has
//     stones;
//   Orc Teeth - BIOLOGIST; the Biologist's row already takes the bot to the
//     valley (GetPlayerBotFrontierMapForLevelRaw), BIOLOG's gate is open;
//   a quest flag, yang - nothing particular: the bot plays on and the timeout
//     answers it.
// While held with something missing it does not fish, dig or pick herbs
// unless the operator says so (fishWhenHeld, sideWhenHeld).
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_shouters.h - it asks every cohort and the
// whole bag.

namespace
{
	BYTE GetPlayerBotStableAmbition(LPCHARACTER ch, BYTE personality);

	const char* const PLAYERBOT_PROGRESSION_PATH = "/opt/m2spool/playerbot_progression.tsv";
	const char* const PLAYERBOT_PROGRESSION_STATUS_PATH = "playerbot_progression_status.tsv";
	const char* const PLAYERBOT_PROGRESSION_FLAG_WAIVED = "playerbot.prog_waived";
	const char* const PLAYERBOT_PROGRESSION_FLAG_GATE = "playerbot.prog_gate";
	const char* const PLAYERBOT_PROGRESSION_FLAG_HELD = "playerbot.prog_held_s";
	const DWORD PLAYERBOT_PROGRESSION_EVAL_MS = 15000;
	const DWORD PLAYERBOT_PROGRESSION_LOG_MS = 30 * 60 * 1000;
	const DWORD PLAYERBOT_PROGRESSION_STATUS_MS = 30000;
	const DWORD PLAYERBOT_PROGRESSION_CENSUS_MS = 5000;

	time_t s_tPlayerBotProgressionMtime = 0;
	long s_lPlayerBotProgressionSize = -1;
	DWORD s_dwPlayerBotProgressionNextCheck = 0;
	bool s_bPlayerBotProgressionApplied = false;

	struct TPlayerBotProgress
	{
		DWORD dwSeen;
		DWORD dwLastTick;
		DWORD dwNextEval;
		DWORD dwNextLog;
		DWORD dwHeldMs;
		BYTE bLevel;
		BYTE bGate;
		BYTE bWaived;
		bool bLoaded;
		bool bEligible;
		bool bFishing;
		bool bHeld;
		bool bAmbition;
		int iNeeds;
		std::string strWhy;
		std::string strName;
		TPlayerBotProgress() : dwSeen(0), dwLastTick(0), dwNextEval(0), dwNextLog(0), dwHeldMs(0),
			bLevel(0), bGate(0), bWaived(0), bLoaded(false), bEligible(false), bFishing(false),
			bHeld(false), bAmbition(false), iNeeds(0) {}
	};
	std::map<DWORD, TPlayerBotProgress> s_mapPlayerBotProgress;

	// The fishing census by ten-level band (index level / 10).
	int s_aiPlayerBotProgressLive[26];
	int s_aiPlayerBotProgressAnglers[26];
	DWORD s_dwPlayerBotProgressNextCensus = 0;

	// The tables playerbot_persona reads - the Grinder's tiers and the law.
	void ApplyPlayerBotProgressionToPersona()
	{
		const playerbot_progression::TConfig& c = playerbot_progression::Config();
		for (unsigned int i = 0; i < playerbot_persona::GRINDER_TIER_COUNT && i < playerbot_progression::TIER_ROWS; ++i)
		{
			playerbot_persona::TGrinderTier& t = playerbot_persona::GRINDER_TIERS[i];
			const playerbot_progression::TTierRow& r = c.tiers[i];
			t.minLevel = r.bandFrom;
			t.maxLevel = r.bandTo;
			t.lockMin = r.lockFrom;
			t.lockMax = r.lockTo;
			playerbot_persona::GRINDER_TIER_HOLDS[i] = r.on;
		}
		playerbot_persona::GRINDER_TIER1_SKIP_PERCENT = (uint8_t)c.tier1SkipPct;
		playerbot_persona::AWANS_LAW_COUNT = std::min<unsigned int>(c.lawCount, playerbot_persona::AWANS_LAW_MAX);
		for (unsigned int i = 0; i < playerbot_persona::AWANS_LAW_COUNT; ++i)
		{
			playerbot_persona::AWANS_LAW[i].fromLevel = c.laws[i].fromLevel;
			playerbot_persona::AWANS_LAW[i].weapon = c.laws[i].weapon;
			playerbot_persona::AWANS_LAW[i].armour = c.laws[i].armour;
			playerbot_persona::AWANS_LAW[i].shield = c.laws[i].shield;
			playerbot_persona::AWANS_LAW[i].helmet = c.laws[i].helmet;
		}
		playerbot_persona::AWANS_LEVEL_WINDOW = (uint8_t)c.lawWindow;
		playerbot_persona::AWANS_PREMIUM_LEVEL_WINDOW = (uint8_t)c.lawPremiumWindow;
		s_bPlayerBotProgressionApplied = true;
	}

	void ReadPlayerBotProgressionFile(const char* path)
	{
		playerbot_progression::TConfig c = playerbot_progression::Defaults();
		playerbot_progression::TParseState st;
		FILE* fp = fopen(path, "r");
		if (fp)
		{
			char line[512];
			while (fgets(line, sizeof(line), fp))
				playerbot_progression::ParseLine(c, st, line);
			fclose(fp);
		}
		playerbot_progression::SortGates(c);
		playerbot_progression::Config() = c;
		ApplyPlayerBotProgressionToPersona();
		// Everybody is weighed again against the new table at once.
		for (std::map<DWORD, TPlayerBotProgress>::iterator it = s_mapPlayerBotProgress.begin();
				it != s_mapPlayerBotProgress.end(); ++it)
			it->second.dwNextEval = 0;
		sys_log(0, "PLAYERBOT_PROGRESS: table read from %s rows=%d checklist=%d gates=%u timeout_min=%u retro=%u fish_cap=%u%% laws=%u",
				path, st.rows, c.enabled ? 1 : 0, (unsigned)c.gates.size(), c.timeoutMin, c.retro,
				c.fishCapPct, c.lawCount);
	}

	void UpdatePlayerBotProgressCensus(DWORD dwNow)
	{
		if (dwNow < s_dwPlayerBotProgressNextCensus)
			return;
		s_dwPlayerBotProgressNextCensus = dwNow + PLAYERBOT_PROGRESSION_CENSUS_MS;
		memset(s_aiPlayerBotProgressLive, 0, sizeof(s_aiPlayerBotProgressLive));
		memset(s_aiPlayerBotProgressAnglers, 0, sizeof(s_aiPlayerBotProgressAnglers));
		for (std::map<DWORD, TPlayerBotProgress>::iterator it = s_mapPlayerBotProgress.begin();
				it != s_mapPlayerBotProgress.end(); )
		{
			// A bot gone for ten minutes is forgotten; its flags keep what matters.
			if (dwNow - it->second.dwSeen > 600000U)
			{
				s_mapPlayerBotProgress.erase(it++);
				continue;
			}
			if (dwNow - it->second.dwSeen <= 60000U)
			{
				const int band = std::min(25, (int)it->second.bLevel / 10);
				++s_aiPlayerBotProgressLive[band];
				if (it->second.bFishing)
					++s_aiPlayerBotProgressAnglers[band];
			}
			++it;
		}
	}

	void WritePlayerBotProgressionStatus(DWORD dwNow);

	// Once a manager tick: the file (a stat every five seconds), the census,
	// the status file.
	void RefreshPlayerBotProgression(DWORD dwNow)
	{
		if (!s_bPlayerBotProgressionApplied)
			ApplyPlayerBotProgressionToPersona();
		UpdatePlayerBotProgressCensus(dwNow);
		WritePlayerBotProgressionStatus(dwNow);
		if (dwNow < s_dwPlayerBotProgressionNextCheck)
			return;
		s_dwPlayerBotProgressionNextCheck = dwNow + 5000;
		struct stat st;
		if (stat(PLAYERBOT_PROGRESSION_PATH, &st) != 0)
		{
			if (s_lPlayerBotProgressionSize >= 0)
			{
				sys_log(0, "PLAYERBOT_PROGRESS: %s is gone, built-in defaults", PLAYERBOT_PROGRESSION_PATH);
				s_tPlayerBotProgressionMtime = 0;
				s_lPlayerBotProgressionSize = -1;
				ReadPlayerBotProgressionFile(PLAYERBOT_PROGRESSION_PATH);
			}
			return;
		}
		if (st.st_mtime == s_tPlayerBotProgressionMtime && (long)st.st_size == s_lPlayerBotProgressionSize)
			return;
		s_tPlayerBotProgressionMtime = st.st_mtime;
		s_lPlayerBotProgressionSize = (long)st.st_size;
		ReadPlayerBotProgressionFile(PLAYERBOT_PROGRESSION_PATH);
	}

	// Who the checklist is for.
	bool IsPlayerBotProgressionEligible(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		if (!ch)
			return false;
		const DWORD pid = ch->GetPlayerID();
		return !IsPlayerBotSidekickPID(pid) && !IsPlayerBotShouterPID(pid) &&
				!IsPlayerBotDropper(state.bPersonality) &&
				!CPlayerBotManager::instance().IsMedalDropperCohortPID(pid) &&
				!IsPlayerBotArezzoCohortPID(pid) && !IsPlayerBotArezzoBound(ch) &&
				!IsPlayerBotArezzoDungeonCohortPID(pid) && !IsPlayerBotOchaoForced(ch);
	}

	// The Orc Tooth row of the Biologist done with its teeth: in its key
	// phase or complete. A core that hosts no Orc stands it as met - it is a
	// requirement nobody here could ever fulfil.
	bool HasPlayerBotHandedOrcTeeth(LPCHARACTER ch)
	{
		for (size_t i = 0; i < PLAYERBOT_BIOLOGIST_MISSION_COUNT; ++i)
		{
			const TPlayerBotBiologistMission& m = PLAYERBOT_BIOLOGIST_MISSIONS[i];
			if (m.itemVnum != PLAYERBOT_ORC_TOOTH_VNUM)
				continue;
			if (!IsPlayerBotHuntingMobHosted(m.mobVnum))
				return true;
			return IsPlayerBotBiologistMissionComplete(ch, i) || IsPlayerBotBiologistKeyPhase(ch, i);
		}
		return true;
	}

	void MeasurePlayerBotProgress(LPCHARACTER ch, playerbot_progression::TSnapshot& s)
	{
		s.level = (uint8_t)std::min<int>(255, ch->GetLevel());
		static const BYTE slots[playerbot_progression::SLOT_COUNT] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_HEAD, WEAR_SHIELD, WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		for (int i = 0; i < playerbot_progression::SLOT_COUNT; ++i)
		{
			// The hand's weapon, not a rod or a pickaxe a session put there.
			LPITEM item = i == 0 ? GetPlayerBotHandWeapon(ch) : ch->GetWear(slots[i]);
			if (!item)
				continue;
			s.slot[i].present = true;
			s.slot[i].level = GetPlayerBotPersonaLevelLimit(item);
			s.slot[i].plus = (uint8_t)std::max(0, std::min(255, (int)item->GetRefineLevel()));
		}
		s.wantsShield = PlayerBotWantsShield(ch);
		long long hp = 0;
		for (int w = 0; w < WEAR_MAX_NUM; ++w)
		{
			LPITEM item = ch->GetWear((BYTE)w);
			if (!item)
				continue;
			for (int a = 0; a < ITEM_ATTRIBUTE_MAX_NUM; ++a)
				if (item->GetAttributeType(a) == APPLY_MAX_HP && item->GetAttributeValue(a) > 0)
					hp += item->GetAttributeValue(a);
			const TItemTable* proto = item->GetProto();
			for (int a = 0; proto && a < ITEM_APPLY_MAX_NUM; ++a)
				if (proto->aApplies[a].bType == APPLY_MAX_HP && proto->aApplies[a].lValue > 0)
					hp += proto->aApplies[a].lValue;
		}
		s.hp = hp;
		s.skillCount = 0;
		if (ch->GetSkillGroup() != 0)
		{
			const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
			for (BYTE i = 0; i < build.bSkillCount && s.skillCount < 16; ++i)
				if (build.dwSkills[i] != 0)
					s.skills[s.skillCount++] = (uint8_t)std::max(0, std::min(255, ch->GetSkillLevel(build.dwSkills[i])));
		}
		s.horse = ch->GetHorseLevel();
#if defined(PLAYERBOT_ENGINE_MT2009)
		// The engine's own count (char_battle.cpp, AddPlayerStat on a stone's
		// death): kept with the character, in player_special_flag.
		s.metins = ch->GetSpecialFlag((DWORD)PLAYER_STATS_STONE_FLAG);
#else
		s.metins = 0;
#endif
		s.orcTeeth = HasPlayerBotHandedOrcTeeth(ch);
		s.gold = (long long)ch->GetGold();
	}

	// The horse a gate asks for, as far as this core can give it: the battle
	// horse's trial is in the desert and the military one's in the Demon
	// Tower, and a core hosting neither holds a horse at the step below.
	long long GetPlayerBotProgressHorseCap(long long wanted)
	{
		if (wanted >= PLAYERBOT_MILITARY_HORSE_LEVEL && !IsPlayerBotHorseTrialOpenHere(PLAYERBOT_MAP_DEMON_TOWER))
			wanted = PLAYERBOT_MILITARY_HORSE_FROM_HORSE_LEVEL;
		if (wanted >= PLAYERBOT_BATTLE_HORSE_LEVEL && !IsPlayerBotHorseTrialOpenHere(PLAYERBOT_MAP_DESERT))
			wanted = PLAYERBOT_BATTLE_HORSE_FROM_HORSE_LEVEL;
		return wanted;
	}

	bool IsPlayerBotProgressionHeld(DWORD pid)
	{
		std::map<DWORD, TPlayerBotProgress>::const_iterator it = s_mapPlayerBotProgress.find(pid);
		return it != s_mapPlayerBotProgress.end() && it->second.bHeld && it->second.bEligible;
	}

	int GetPlayerBotProgressionNeeds(DWORD pid)
	{
		std::map<DWORD, TPlayerBotProgress>::const_iterator it = s_mapPlayerBotProgress.find(pid);
		return it != s_mapPlayerBotProgress.end() && it->second.bHeld && it->second.bEligible
				? it->second.iNeeds : 0;
	}

	// The weight gates (IsPlayerBotWeightGateOpen): open for what a held bot
	// lacks, closed for the veins and the board while it lacks anything.
	bool PlayerBotProgressionWeightGate(DWORD pid, BYTE weight, bool& open)
	{
		const int needs = GetPlayerBotProgressionNeeds(pid);
		if (needs == 0)
			return false;
		switch (weight)
		{
			case PLAYERBOT_WEIGHT_REFINE:
				if (needs & (playerbot_progression::NEED_GEAR | playerbot_progression::NEED_HP)) { open = true; return true; }
				return false;
			case PLAYERBOT_WEIGHT_SKILL:
				if (needs & playerbot_progression::NEED_SKILLS) { open = true; return true; }
				return false;
			case PLAYERBOT_WEIGHT_BIOLOG:
				if (needs & playerbot_progression::NEED_ORC_TEETH) { open = true; return true; }
				return false;
			case PLAYERBOT_WEIGHT_HORSE:
				if (needs & playerbot_progression::NEED_HORSE) { open = true; return true; }
				return false;
			case PLAYERBOT_WEIGHT_METIN:
				if (needs & playerbot_progression::NEED_METINS) { open = true; return true; }
				return false;
			case PLAYERBOT_WEIGHT_MINING:
			case PLAYERBOT_WEIGHT_HERB:
				if (!playerbot_progression::Config().sideWhenHeld) { open = false; return true; }
				return false;
			default:
				return false;
		}
	}

	// Whether this bot may start for the water (IsPlayerBotAngler): not while
	// held with something missing (unless the operator allows it), and not
	// once its ten-level band has its share at the water. A bot already at
	// the water or on its way is counted and goes on.
	bool PlayerBotProgressionBlocksFishing(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || state.bFishingSession ||
				(state.persona.dwRybakTripUntil != 0 && dwNow < state.persona.dwRybakTripUntil))
			return false;
		const playerbot_progression::TConfig& c = playerbot_progression::Config();
		if (!c.fishWhenHeld && GetPlayerBotProgressionNeeds(ch->GetPlayerID()) != 0)
			return true;
		if (c.fishCapPct >= 100)
			return false;
		const int band = std::min(25, (int)ch->GetLevel() / 10);
		const int live = s_aiPlayerBotProgressLive[band];
		if (live <= 0)
			return false;
		const int cap = (int)((long long)live * (long long)c.fishCapPct / 100LL);
		if (s_aiPlayerBotProgressAnglers[band] < std::max(c.fishCapPct > 0 ? 1 : 0, cap))
			return false;
		PlayerBotLogThrottled("progress_fish_cap", dwNow,
				"PLAYERBOT_PROGRESS: fishing cap pid=%u name=%s level=%u band=%d anglers=%d live=%d cap_pct=%u",
				ch->GetPlayerID(), ch->GetName(), (unsigned)ch->GetLevel(), band * 10,
				s_aiPlayerBotProgressAnglers[band], live, c.fishCapPct);
		return true;
	}

	BYTE GetPlayerBotProgressAmbition(int needs)
	{
		if (needs & playerbot_progression::NEED_HORSE)
			return BOT_AMBITION_HORSE;
		if (needs & playerbot_progression::NEED_ORC_TEETH)
			return BOT_AMBITION_BIOLOGIST;
		if (needs & playerbot_progression::NEED_METINS)
			return BOT_AMBITION_METINS;
		if (needs & playerbot_progression::NEED_SKILLS)
			return BOT_AMBITION_SKILLS;
		if (needs & (playerbot_progression::NEED_GEAR | playerbot_progression::NEED_HP))
			return BOT_AMBITION_EQUIPMENT;
		return 0xFF;
	}

	void ReleasePlayerBotProgress(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotProgress& e)
	{
		e.bHeld = false;
		e.iNeeds = 0;
		e.strWhy.clear();
		e.dwHeldMs = 0;
		e.bGate = 0;
		if (e.bAmbition && ch)
			state.bAmbition = GetPlayerBotStableAmbition(ch, state.bPersonality);
		e.bAmbition = false;
		if (ch)
		{
			ch->SetQuestFlag(PLAYERBOT_PROGRESSION_FLAG_GATE, 0);
			ch->SetQuestFlag(PLAYERBOT_PROGRESSION_FLAG_HELD, 0);
		}
	}

	// Every tick, before ManagePlayerBotExpLock: cheap but for the weighing,
	// which runs every PLAYERBOT_PROGRESSION_EVAL_MS and on a level change.
	void ManagePlayerBotProgression(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return;
		const DWORD pid = ch->GetPlayerID();
		TPlayerBotProgress& e = s_mapPlayerBotProgress[pid];
		const DWORD dt = e.dwLastTick != 0 ? dwNow - e.dwLastTick : 0;
		e.dwLastTick = dwNow;
		e.dwSeen = dwNow;
		if (e.strName.empty())
			e.strName = ch->GetName();
		const BYTE level = (BYTE)std::min<int>(255, ch->GetLevel());
		if (level != e.bLevel)
			e.dwNextEval = 0;
		e.bLevel = level;
		e.bFishing = state.bFishingSession ||
				(state.persona.dwRybakTripUntil != 0 && dwNow < state.persona.dwRybakTripUntil);
		const playerbot_progression::TConfig& c = playerbot_progression::Config();
		e.bEligible = IsPlayerBotProgressionEligible(ch, state);
		if (!e.bEligible || !c.enabled || c.gates.empty() ||
				(IsPlayerBotPersonaEnabled() && !state.persona.bRestored))
		{
			if (e.bHeld)
			{
				sys_log(0, "PLAYERBOT_PROGRESS: released pid=%u name=%s level=%u gate=%u why=%s",
						pid, ch->GetName(), (unsigned)level, (unsigned)e.bGate,
						!e.bEligible ? "not_eligible" : "checklist_off");
				ReleasePlayerBotProgress(ch, state, e);
			}
			return;
		}
		// Play time at the gate; a long gap is a logout, not play.
		if (e.bHeld && dt < 60000U)
			e.dwHeldMs += dt;
		if (e.dwNextEval != 0 && dwNow < e.dwNextEval)
			return;
		e.dwNextEval = dwNow + PLAYERBOT_PROGRESSION_EVAL_MS + pid % 3000U;
		if (!e.bLoaded)
		{
			e.bLoaded = true;
			e.bWaived = (BYTE)std::max(0, std::min(255, ch->GetQuestFlag(PLAYERBOT_PROGRESSION_FLAG_WAIVED)));
		}

		playerbot_progression::TSnapshot snap;
		MeasurePlayerBotProgress(ch, snap);
		BYTE holdGate = 0;
		int needs = 0;
		std::string why;
		for (size_t g = 0; g < c.gates.size() && holdGate == 0; ++g)
		{
			const playerbot_progression::TGate& gate = c.gates[g];
			if (gate.level > level || (unsigned)level > (unsigned)gate.level + c.retro || gate.level <= e.bWaived)
				continue;
			for (size_t r = 0; r < gate.reqs.size(); ++r)
			{
				playerbot_progression::TReq req = gate.reqs[r];
				if (!req.on)
					continue;
				if (req.type == playerbot_progression::REQ_HORSE)
					req.a = GetPlayerBotProgressHorseCap(req.a);
				long long flagValue = 0;
				if (req.type == playerbot_progression::REQ_QUEST_FLAG)
					flagValue = ch->GetQuestFlag(req.flag);
				std::string one;
				if (playerbot_progression::Meets(req, snap, flagValue, one))
					continue;
				holdGate = gate.level;
				needs |= playerbot_progression::NeedOf(req.type);
				if (!why.empty())
					why += ", ";
				why += one;
			}
		}

		if (holdGate == 0)
		{
			if (e.bHeld)
			{
				sys_log(0, "PLAYERBOT_PROGRESS: passed pid=%u name=%s level=%u gate=%u held_min=%u",
						pid, ch->GetName(), (unsigned)level, (unsigned)e.bGate, e.dwHeldMs / 60000U);
				ReleasePlayerBotProgress(ch, state, e);
			}
			return;
		}

		if (e.bGate != holdGate)
		{
			// Back at the same gate after a login: the minutes already stood.
			const int savedGate = ch->GetQuestFlag(PLAYERBOT_PROGRESSION_FLAG_GATE);
			e.dwHeldMs = savedGate == (int)holdGate
					? (DWORD)std::max(0, ch->GetQuestFlag(PLAYERBOT_PROGRESSION_FLAG_HELD)) * 1000U : 0U;
			e.bGate = holdGate;
			e.dwNextLog = 0;
		}
		if (e.dwHeldMs >= c.timeoutMin * 60000U)
		{
			sys_log(0, "PLAYERBOT_PROGRESS: gate given up pid=%u name=%s level=%u gate=%u held_min=%u missing=%s",
					pid, ch->GetName(), (unsigned)level, (unsigned)holdGate, e.dwHeldMs / 60000U, why.c_str());
			e.bWaived = holdGate;
			ch->SetQuestFlag(PLAYERBOT_PROGRESSION_FLAG_WAIVED, holdGate);
			ReleasePlayerBotProgress(ch, state, e);
			e.dwNextEval = 0;
			return;
		}
		const bool fresh = !e.bHeld;
		e.bHeld = true;
		e.iNeeds = needs;
		e.strWhy = why;
		ch->SetQuestFlag(PLAYERBOT_PROGRESSION_FLAG_GATE, holdGate);
		ch->SetQuestFlag(PLAYERBOT_PROGRESSION_FLAG_HELD, (int)(e.dwHeldMs / 1000U));
		if (fresh || dwNow >= e.dwNextLog)
		{
			e.dwNextLog = dwNow + PLAYERBOT_PROGRESSION_LOG_MS;
			sys_log(0, "PLAYERBOT_PROGRESS: held at %u pid=%u name=%s level=%u held_min=%u missing: %s",
					(unsigned)holdGate, pid, ch->GetName(), (unsigned)level, e.dwHeldMs / 60000U, why.c_str());
		}

		// The push.
		const BYTE ambition = GetPlayerBotProgressAmbition(needs);
		if (ambition != 0xFF && state.bAmbition != ambition)
		{
			state.bAmbition = ambition;
			e.bAmbition = true;
		}
		if ((needs & playerbot_progression::NEED_METINS) && state.dwMetinExpeditionUntil == 0 &&
				!(needs & (playerbot_progression::NEED_HORSE | playerbot_progression::NEED_ORC_TEETH)) &&
				PlayerBotMapHasMetinStones(ch->GetMapIndex()))
		{
			state.dwMetinExpeditionUntil = dwNow + PLAYERBOT_METIN_EXPEDITION_DURATION;
			state.dwHubChosenTime = 0;
			sys_log(0, "PLAYERBOT_METIN: expedition start pid=%u name=%s level=%u map=%ld minutes=%u why=progression",
					pid, ch->GetName(), (unsigned)level, ch->GetMapIndex(), PLAYERBOT_METIN_EXPEDITION_DURATION / 60000);
		}
	}

	// The panel's "kto stoi i dlaczego": counts per gate and per requirement,
	// the fishing census and the longest-held bots, every half minute.
	void WritePlayerBotProgressionStatus(DWORD dwNow)
	{
		static DWORD s_dwNext = 0;
		if (dwNow < s_dwNext)
			return;
		s_dwNext = dwNow + PLAYERBOT_PROGRESSION_STATUS_MS;
		const playerbot_progression::TConfig& c = playerbot_progression::Config();
		std::map<int, int> perGate;
		std::map<std::pair<int, std::string>, int> perReq;
		std::vector<std::pair<DWORD, DWORD> > held;  // held ms, pid
		int eligible = 0, heldCount = 0, waived = 0;
		for (std::map<DWORD, TPlayerBotProgress>::const_iterator it = s_mapPlayerBotProgress.begin();
				it != s_mapPlayerBotProgress.end(); ++it)
		{
			const TPlayerBotProgress& e = it->second;
			if (dwNow - e.dwSeen > 60000U)
				continue;
			if (e.bEligible)
				++eligible;
			if (e.bWaived != 0)
				++waived;
			if (!e.bHeld || !e.bEligible)
				continue;
			++heldCount;
			++perGate[e.bGate];
			// The requirement words up to the first space and digit run, so
			// "bron lv25+7 (ma lv20+5)" counts as "bron".
			size_t start = 0;
			while (start < e.strWhy.size())
			{
				size_t end = e.strWhy.find(", ", start);
				if (end == std::string::npos)
					end = e.strWhy.size();
				std::string part = e.strWhy.substr(start, end - start);
				const size_t paren = part.find(" (");
				if (paren != std::string::npos)
					part = part.substr(0, paren);
				++perReq[std::make_pair((int)e.bGate, part)];
				start = end + 2;
			}
			held.push_back(std::make_pair(e.dwHeldMs, it->first));
		}
		std::sort(held.rbegin(), held.rend());
		const char* tmp = "playerbot_progression_status.tsv.tmp";
		FILE* fp = fopen(tmp, "wb");
		if (!fp)
			return;
		fprintf(fp, "# MT2009_PLUS_PROGRESSION_V1 status, written %ld\n", (long)time(0));
		fprintf(fp, "summary\t%d\t%d\t%d\t%d\t%u\t%u\n", eligible, heldCount, waived,
				c.enabled ? 1 : 0, c.timeoutMin, c.fishCapPct);
		for (std::map<int, int>::const_iterator it = perGate.begin(); it != perGate.end(); ++it)
			fprintf(fp, "gate\t%d\t%d\n", it->first, it->second);
		for (std::map<std::pair<int, std::string>, int>::const_iterator it = perReq.begin(); it != perReq.end(); ++it)
			fprintf(fp, "req\t%d\t%s\t%d\n", it->first.first, it->first.second.c_str(), it->second);
		for (int band = 0; band < 26; ++band)
			if (s_aiPlayerBotProgressLive[band] > 0)
				fprintf(fp, "fish\t%d\t%d\t%d\n", band * 10, s_aiPlayerBotProgressLive[band], s_aiPlayerBotProgressAnglers[band]);
		for (size_t i = 0; i < held.size() && i < 300; ++i)
		{
			const TPlayerBotProgress& e = s_mapPlayerBotProgress[held[i].second];
			std::string why = e.strWhy;
			std::replace(why.begin(), why.end(), '\t', ' ');
			fprintf(fp, "bot\t%u\t%s\t%u\t%u\t%u\t%s\n", held[i].second, e.strName.c_str(),
					(unsigned)e.bLevel, (unsigned)e.bGate, held[i].first / 60000U, why.c_str());
		}
		fclose(fp);
		rename(tmp, PLAYERBOT_PROGRESSION_STATUS_PATH);
	}
}

#endif
