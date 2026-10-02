#ifndef __INC_METIN2_PLAYERBOT_BPBOTS_H__
#define __INC_METIN2_PLAYERBOT_BPBOTS_H__

// MT2009_PLUS_BP_BOTS_V1: the bots and the Battle Pass (the operator, 28
// September: "caly battlepass niech dla botow beda taski, ktore musza
// wykonywac; moga sie zdecydowac czy chca robic je celowo, czy przy okazji,
// ale jak ma do zbicia metiny 25 to idzie szukac metinow 25 ... boty maja
// dazyc do tego, aby zamknac wszystkie misje").
//
// A bot's kills, fish, refines and shouts count like a player's
// (playerbot_battlepass.h). This is what it sets out to do about them. Every
// minute and a half or so it looks at its missions that are neither done nor
// locked behind another, and - more or less often by its personality - takes
// one on as an errand of up to an hour:
//   - a Metin mission: the map where that stone stands (the table below, read
//     off the maps' stone.txt) - its own village, or a frontier this core
//     hosts - named as its frontier for the errand (GetErrandMap), and a stone
//     expedition for as long; the stone it wants scores over any other
//     (TargetBonus) and is broken even when the bot has outgrown it
//     (WantsStone). Only a stone no more than STONE_MAX_OVER levels above the
//     bot: a bot of twenty is not sent to the Metins of fifty. Any Metin: an
//     expedition where it stands;
//   - fish: a fishing trip, as an angler (WantsFishing);
//   - refines: the town visit now, when it has something the smith takes;
//   - bosses: first to be called when a boss raid of its level forms
//     (BossRecruitBonus).
// The rest - monsters, yang, chests, play time - it does by the way; a
// mission for one kind of monster makes that monster score higher when it is
// in sight. The Metin errands are drawn within a budget of the core's bots,
// a few to a stone map at once, and a bot far over the stone travels for it
// only on a further draw - no stampede to the Metins of twenty-five.
// And the shout channel lives: in each kingdom on each core a bot shouts
// every fifteen to thirty seconds, a different one each time - "!BP", a
// trade line, a party, the stones, a boss, its weapon, small talk - with a
// share of the lines to the one furthest along its shout missions (every
// line counts for them).
//
// Everything here is per core and in memory; the progress is the Battle
// Pass's, in the database.

extern int g_iShoutLimitLevel;

namespace playerbot_bpbots
{
	// How often a bot looks at its missions, and how long an errand lasts.
	const DWORD THINK_MS = 90 * 1000;
	const DWORD THINK_SPREAD_MS = 60 * 1000;
	const DWORD METIN_ERRAND_MS = 40 * 60 * 1000;
	const DWORD FISH_ERRAND_MS = 90 * 60 * 1000;
	const DWORD REFINE_ERRAND_MS = 10 * 60 * 1000;
	const DWORD BOSS_ERRAND_MS = 45 * 60 * 1000;
	// After an errand, and after a roll that chose "by the way".
	const DWORD REST_AFTER_ERRAND_MIN_MS = 3 * 60 * 1000;
	const DWORD REST_AFTER_ERRAND_MAX_MS = 8 * 60 * 1000;
	const DWORD REST_BY_THE_WAY_MIN_MS = 10 * 60 * 1000;
	const DWORD REST_BY_THE_WAY_MAX_MS = 25 * 60 * 1000;
	// A stone this far above the bot at most: one it can break.
	const int STONE_MAX_OVER = 5;
	// The Metin errands are drawn, never a stampede (the operator, 28
	// September: "nie moze byc tak, ze wszystkie boty naraz leca zbijac
	// Metiny 25 lvl - musi to byc unormowane ... losowaniem"):
	//   - bots on one errand for one stone on one map at once;
	const int STONE_CROWD_MAX = 4;
	//   - bots of this core on an errand for a named stone at once: this share
	//     of the bots seen lately, never under the minimum;
	const int METIN_BUDGET_PERCENT = 6;
	const int METIN_BUDGET_MIN = 3;
	//   - and on an errand for any stone where they stand (no travel);
	const int METIN_ANY_BUDGET_PERCENT = 12;
	//   - a bot this many levels over the stone travels for it only when it
	//     wins a further draw of this many percent (on the stone's map
	//     already: no draw); otherwise it breaks the stone by the way.
	const int METIN_FAR_GAP_1 = 10;
	const int METIN_FAR_CHANCE_1 = 40;
	const int METIN_FAR_GAP_2 = 20;
	const int METIN_FAR_CHANCE_2 = 15;
	// The shout channel ("ma byc spam na czacie ... musimy ozywic ten czat"):
	// in each kingdom on each core a bot shouts every CHATTER_MIN_MS to
	// CHATTER_MAX_MS, a different one each time (a bot again after
	// CHATTER_BOT_COOLDOWN at the least, and never under the engine's 15 s),
	// with CHATTER_REGULAR_PERCENT of the lines going to the one furthest
	// along its shout missions, so that somebody does finish them.
	const DWORD CHATTER_MIN_MS = 15 * 1000;
	const DWORD CHATTER_MAX_MS = 30 * 1000;
	const DWORD CHATTER_BOT_COOLDOWN_MIN_MS = 3 * 60 * 1000;
	const DWORD CHATTER_BOT_COOLDOWN_MAX_MS = 6 * 60 * 1000;
	const DWORD CHATTER_REGULAR_COOLDOWN_MS = 45 * 1000;
	const int CHATTER_REGULAR_PERCENT = 30;
	// A trade shout (playerbot_chat_trade.h) this recent holds the next line.
	const DWORD CHATTER_QUIET_MS = 5 * 1000;
	// A bot whose tick has not come round for this long is not a speaker.
	const DWORD SEEN_RECENTLY_MS = 3 * 60 * 1000;
	const DWORD CENSUS_MS = 10 * 60 * 1000;

	enum EGoal
	{
		GOAL_NONE = 0,
		GOAL_METIN,
		GOAL_FISH,
		GOAL_REFINE,
		GOAL_BOSS,
	};

	const char* GoalName(BYTE goal)
	{
		switch (goal)
		{
			case GOAL_METIN: return "metin";
			case GOAL_FISH: return "fish";
			case GOAL_REFINE: return "refine";
			case GOAL_BOSS: return "boss";
			default: return "none";
		}
	}

	// Where each stone stands (the maps' stone.txt, 28 September): the first
	// village, the second, and up to two shared maps. The two forests carry
	// stones too, but no bot hunts stones there (PlayerBotMapHasMetinStones).
	struct TStoneSpawn
	{
		DWORD vnum;
		bool m1;
		bool m2;
		long shared[2];
	};

	const TStoneSpawn STONE_SPAWNS[] =
	{
		{ 8001, true, false, { 0, 0 } },
		{ 8002, true, false, { 0, 0 } },
		{ 8003, true, false, { 0, 0 } },
		{ 8004, true, false, { 0, 0 } },
		// 8005 is on M2 only for the errand: the first village has one spawn
		// of it every 20-25 minutes, the second 3+2 every 12-15, and the level
		// rule takes a bot of 22 and more to M2 anyway - on an M1 errand it
		// stood on the stone's map 37% of the time (29 September).
		{ 8005, false, true, { 0, 0 } },
		{ 8006, false, true, { 0, 0 } },
		{ 8007, false, true, { 0, 0 } },
		{ 8008, false, false, { PLAYERBOT_MAP_DESERT, PLAYERBOT_MAP_ORC_VALLEY } },
		{ 8009, false, false, { PLAYERBOT_MAP_DESERT, PLAYERBOT_MAP_ORC_VALLEY } },
		{ 8010, false, false, { PLAYERBOT_MAP_DESERT, 0 } },
		{ 8011, false, false, { PLAYERBOT_MAP_SOHAN, PLAYERBOT_MAP_HWANG } },
		{ 8012, false, false, { PLAYERBOT_MAP_HWANG, 0 } },
		{ 8013, false, false, { PLAYERBOT_MAP_SOHAN, 0 } },
		{ 8014, false, false, { PLAYERBOT_MAP_FIRE_LAND, 0 } },
		{ 8024, false, false, { PLAYERBOT_MAP_FOREST, 0 } },
		{ 8025, false, false, { PLAYERBOT_MAP_FOREST, 0 } },
		{ 8026, false, false, { PLAYERBOT_MAP_RED_FOREST, 0 } },
		{ 8027, false, false, { PLAYERBOT_MAP_RED_FOREST, 0 } },
	};
	const size_t STONE_SPAWN_COUNT = sizeof(STONE_SPAWNS) / sizeof(STONE_SPAWNS[0]);

	// A stone a mission names: by its vnum, or any stone of a level (vnum 0).
	struct TStoneWant
	{
		DWORD vnum;
		DWORD level;
	};

	struct TBot
	{
		DWORD dwNextThink;
		DWORD dwRestUntil;
		BYTE bGoal;
		DWORD dwMission;
		DWORD dwStone;
		long lMap;
		bool bVillage;
		DWORD dwUntil;
		DWORD dwExpeditionUntil;
		// What its open missions want, as of its last look.
		std::vector<TStoneWant> stones;
		std::vector<DWORD> mobs;
		std::vector<DWORD> bosses;
		bool bAnyBoss;
		bool bShout;
		DWORD dwShoutProgress;
		DWORD dwLastSeen;
		// The errand just done: the next of its kind is taken on at once (the
		// next Metin of a chain, where the bot already stands), no roll.
		BYTE bFollowGoal;
		// When it may shout again (the chatter).
		DWORD dwNextChatter;

		TBot() : dwNextThink(0), dwRestUntil(0), bGoal(GOAL_NONE), dwMission(0), dwStone(0), lMap(0),
			bVillage(false), dwUntil(0), dwExpeditionUntil(0), bAnyBoss(false), bShout(false),
			dwShoutProgress(0), dwLastSeen(0), bFollowGoal(GOAL_NONE), dwNextChatter(0) {}
	};

	std::map<DWORD, TBot> s_mapBots;
	// When each kingdom's next line on this core is due.
	DWORD s_adwNextShout[4] = { 0, 0, 0, 0 };
	DWORD s_dwNextCensus = 0;
	unsigned int s_uAdopted = 0, s_uFinished = 0, s_uShouts = 0;
	// The draws since the last census (PLAYERBOT_BP: draws).
	unsigned int s_uRolls = 0, s_uByTheWay = 0, s_uNothing = 0, s_uMetinBudget = 0,
			s_uMetinFar = 0, s_uMetinCrowd = 0, s_uFollow = 0;
	unsigned int s_auAdoptedGoal[5] = { 0, 0, 0, 0, 0 };
	// The chatter's lines since the last census, by kind.
	unsigned int s_auChatter[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	TBot* FindBot(DWORD pid)
	{
		std::map<DWORD, TBot>::iterator it = s_mapBots.find(pid);
		return it == s_mapBots.end() ? NULL : &it->second;
	}

	int StoneLevel(DWORD vnum)
	{
		const CMob* mob = CMobManager::instance().Get(vnum);
		return mob ? (int)mob->m_table.bLevel : 0;
	}

	bool IsActive(const TBot& b, DWORD dwNow)
	{
		return b.bGoal != GOAL_NONE && (int)(dwNow - b.dwUntil) < 0;
	}

	bool MatchesStone(const TBot& b, DWORD race, int level)
	{
		for (size_t i = 0; i < b.stones.size(); ++i)
			if ((b.stones[i].vnum != 0 && b.stones[i].vnum == race) ||
					(b.stones[i].vnum == 0 && (int)b.stones[i].level == level))
				return true;
		return false;
	}

	// ---- the hooks the bot code asks (declared in playerbot_battlepass.h) ----

	bool WantsStone(LPCHARACTER ch, LPCHARACTER stone)
	{
		if (!ch || !stone)
			return false;
		const TBot* b = FindBot(ch->GetPlayerID());
		if (!b || b->stones.empty())
			return false;
		const int level = (int)stone->GetLevel();
		return level <= (int)ch->GetLevel() + STONE_MAX_OVER && MatchesStone(*b, stone->GetRaceNum(), level);
	}

	int TargetBonus(LPCHARACTER ch, LPCHARACTER candidate)
	{
		if (!ch || !candidate)
			return 0;
		const TBot* b = FindBot(ch->GetPlayerID());
		if (!b)
			return 0;
		const DWORD race = candidate->GetRaceNum();
		const int level = (int)candidate->GetLevel();
		if (candidate->IsStone())
		{
			if (b->stones.empty() || level > (int)ch->GetLevel() + STONE_MAX_OVER || !MatchesStone(*b, race, level))
				return 0;
			// The stone the errand is for, then any stone a mission names.
			return b->bGoal == GOAL_METIN && b->dwStone == race ? 900000 : 400000;
		}
		for (size_t i = 0; i < b->mobs.size(); ++i)
			if (b->mobs[i] == race)
				return 250000;
		// A boss of the bot's own level or under, when it is on its way: the
		// raids (playerbot_boss_raid.h) are for the rest.
		if (candidate->GetMobRank() >= MOB_RANK_BOSS && level <= (int)ch->GetLevel())
		{
			if (b->bAnyBoss)
				return 250000;
			for (size_t i = 0; i < b->bosses.size(); ++i)
				if (b->bosses[i] == race)
					return 250000;
		}
		return 0;
	}

	bool GetErrandMap(LPCHARACTER ch, long& map)
	{
		if (!ch)
			return false;
		const TBot* b = FindBot(ch->GetPlayerID());
		if (!b || b->bGoal != GOAL_METIN || !IsActive(*b, get_dword_time()) || (!b->bVillage && b->lMap == 0))
			return false;
		map = b->bVillage ? 0 : b->lMap;
		return true;
	}

	bool WantsFishing(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		const TBot* b = FindBot(ch->GetPlayerID());
		return b && b->bGoal == GOAL_FISH && IsActive(*b, get_dword_time());
	}

	int BossRecruitBonus(DWORD pid, DWORD race)
	{
		const TBot* b = FindBot(pid);
		if (!b)
			return 0;
		bool wants = b->bAnyBoss;
		for (size_t i = 0; i < b->bosses.size() && !wants; ++i)
			wants = b->bosses[i] == race;
		if (!wants)
			return 0;
		return b->bGoal == GOAL_BOSS ? (1 << 24) : (1 << 22);
	}

	bool IsOnErrand(DWORD pid)
	{
		const TBot* b = FindBot(pid);
		return b && IsActive(*b, get_dword_time());
	}

	void OnProgressSettled(DWORD pid)
	{
		TBot* b = FindBot(pid);
		if (b)
			b->dwNextThink = 0;
	}

	// ---- the look -----------------------------------------------------------

	// The missions of this bot neither done nor locked.
	void CollectOpen(mt2009_battlepass::Cache& cache, std::vector<const mt2009_battlepass::Mission*>& out)
	{
		using namespace mt2009_battlepass;
		out.clear();
		for (size_t i = 0; i < s_vecMissions.size(); ++i)
		{
			const Mission& m = s_vecMissions[i];
			const Progress& p = cache.missions[m.id];
			if (p.value + p.delta >= m.count || IsLocked(cache, m))
				continue;
			out.push_back(&m);
		}
	}

	void Refresh(TBot& b, mt2009_battlepass::Cache& cache, const std::vector<const mt2009_battlepass::Mission*>& open)
	{
		using namespace mt2009_battlepass;
		b.stones.clear();
		b.mobs.clear();
		b.bosses.clear();
		b.bAnyBoss = false;
		b.bShout = false;
		b.dwShoutProgress = 0;
		for (size_t i = 0; i < open.size(); ++i)
		{
			const Mission& m = *open[i];
			switch (m.type)
			{
				case TYPE_METIN:
					if (m.target != 0 || m.targetLevel != 0)
					{
						TStoneWant w;
						w.vnum = m.target;
						w.level = m.targetLevel;
						b.stones.push_back(w);
					}
					break;
				case TYPE_MONSTER:
					if (m.target != 0)
						b.mobs.push_back(m.target);
					break;
				case TYPE_BOSS:
					if (m.target != 0)
						b.bosses.push_back(m.target);
					else
						b.bAnyBoss = true;
					break;
				case TYPE_SHOUT:
					b.bShout = true;
					break;
				default:
					break;
			}
		}
		for (size_t i = 0; i < s_vecMissions.size(); ++i)
			if (s_vecMissions[i].type == TYPE_SHOUT)
			{
				const Progress& p = cache.missions[s_vecMissions[i].id];
				b.dwShoutProgress += std::min<DWORD>(s_vecMissions[i].count, p.value + p.delta);
			}
	}

	// Whatever the bot is doing that no errand of this kind interrupts.
	bool IsBusy(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		const DWORD pid = ch->GetPlayerID();
		// A bot already in a party plays it out; one on an errand joins none.
		return ch->GetParty() != NULL || IsPlayerBotHeldForCompany(ch) || IsPlayerBotSidekickPID(pid) ||
				IsPlayerBotOnMercContract(pid) || IsPlayerBotInDungeonBusiness(ch, state) ||
				state.bWorldEventKind != 0 || state.wBossRaidRace != 0 || state.bFishingSession ||
				state.bRecoveringAfterDeath || state.bTacticalRetreat || ch->GetMyShop() ||
				IsPlayerBotOnBattleHorseTrial(ch) || IsPlayerBotOnMilitaryHorseTrial(ch) ||
				!(IsPlayerBotVillageMap(ch->GetMapIndex()) || IsPlayerBotFrontierMap(ch->GetMapIndex()));
	}

	// How keen the bot is on its missions at all, by personality, give or
	// take ten by its pid: a percentage per look.
	int AdoptChance(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		int chance = 45;
		switch (state.bPersonality)
		{
			case BOT_PERSONALITY_METIN_BREAKER: chance = 70; break;
			case BOT_PERSONALITY_WANDERER: chance = 55; break;
			case BOT_PERSONALITY_STEADY_ADVENTURER: chance = 50; break;
			case BOT_PERSONALITY_GEAR_SPECIALIST: chance = 50; break;
			case BOT_PERSONALITY_CAREFUL_COLLECTOR: chance = 45; break;
			case BOT_PERSONALITY_TEAM_COMPANION: chance = 40; break;
			case BOT_PERSONALITY_MERCHANT: chance = 20; break;
			case BOT_PERSONALITY_METIN_DROPPER: chance = 35; break;
			default:
				if (IsPlayerBotDropper(state.bPersonality))
					chance = 15;
				break;
		}
		return chance + (int)(PlayerBotNavHash(ch->GetPlayerID() ^ 0x42504250U) % 21U) - 10;
	}

	int CountStoneErrands(long map, DWORD vnum, DWORD dwNow)
	{
		int n = 0;
		for (std::map<DWORD, TBot>::const_iterator it = s_mapBots.begin(); it != s_mapBots.end(); ++it)
			if (it->second.bGoal == GOAL_METIN && IsActive(it->second, dwNow) &&
					it->second.dwStone == vnum && it->second.lMap == map)
				++n;
		return n;
	}

	struct TPlan
	{
		BYTE goal;
		const mt2009_battlepass::Mission* mission;
		DWORD stone;
		long map;
		bool village;
		int weight;
	};

	// Where this bot would break stone `vnum`, or false: its own village or a
	// shared map this core hosts, the one it stands on first.
	bool PlanStoneMap(LPCHARACTER ch, DWORD vnum, DWORD dwNow, long& outMap, bool& outVillage)
	{
		const TStoneSpawn* row = NULL;
		for (size_t i = 0; i < STONE_SPAWN_COUNT && !row; ++i)
			if (STONE_SPAWNS[i].vnum == vnum)
				row = &STONE_SPAWNS[i];
		if (!row)
			return false;
		const long cur = ch->GetMapIndex();
		const long m1 = GetPlayerBotHomeMap(ch, playerbot_empire_rules::MAP_ROLE_M1);
		const long m2 = GetPlayerBotHomeMap(ch, playerbot_empire_rules::MAP_ROLE_M2);
		std::vector<long> shared;
		for (int k = 0; k < 2; ++k)
			if (row->shared[k] != 0 && IsPlayerBotMapHostedHere(row->shared[k]) &&
					IsPlayerBotFrontierMapIndex(row->shared[k]) && PlayerBotMapHasMetinStones(row->shared[k]) &&
					CountStoneErrands(row->shared[k], vnum, dwNow) < STONE_CROWD_MAX)
				shared.push_back(row->shared[k]);
		const bool atM1 = row->m1 && cur == m1 && CountStoneErrands(m1, vnum, dwNow) < STONE_CROWD_MAX;
		const bool atM2 = row->m2 && IsPlayerBotMapHostedHere(m2) && CountStoneErrands(m2, vnum, dwNow) < STONE_CROWD_MAX;
		if (!atM1 && !atM2 && shared.empty())
		{
			++s_uMetinCrowd;
			return false;
		}
		if (atM1)
		{
			outMap = m1;
			outVillage = true;
			return true;
		}
		if (atM2)
		{
			outMap = m2;
			outVillage = true;
			return true;
		}
		for (size_t k = 0; k < shared.size(); ++k)
			if (shared[k] == cur)
			{
				outMap = cur;
				outVillage = false;
				return true;
			}
		if (shared.empty() || ch->GetGold() < (long long)GetPlayerBotTeleporterFee(ch))
			return false;
		// The shared map with fewer on the same errand.
		outMap = shared[0];
		if (shared.size() > 1 && CountStoneErrands(shared[1], vnum, dwNow) < CountStoneErrands(shared[0], vnum, dwNow))
			outMap = shared[1];
		outVillage = false;
		return true;
	}

	bool PlanMetin(LPCHARACTER ch, const TPlayerBotAIState& state, const mt2009_battlepass::Mission& m,
			DWORD dwNow, TPlan& plan)
	{
		// A dropper farms its own table; a medal or M3 dropper's expedition is
		// ended by the planner (RollPlayerBotMetinExpedition).
		if (IsPlayerBotDropper(state.bPersonality) && state.bPersonality != BOT_PERSONALITY_METIN_DROPPER)
			return false;
		if (ch->GetLevel() < PLAYERBOT_METIN_EXPEDITION_MIN_LEVEL)
			return false;
		const int botLevel = (int)ch->GetLevel();
		plan.goal = GOAL_METIN;
		plan.mission = &m;
		if (m.target == 0 && m.targetLevel == 0)
		{
			// Any stone: where it stands, when there are stones here.
			if (!PlayerBotMapHasMetinStones(ch->GetMapIndex()))
				return false;
			plan.stone = 0;
			plan.map = 0;
			plan.village = false;
			return true;
		}
		for (size_t i = 0; i < STONE_SPAWN_COUNT; ++i)
		{
			const DWORD vnum = STONE_SPAWNS[i].vnum;
			if (m.target != 0 && vnum != m.target)
				continue;
			const int level = StoneLevel(vnum);
			if (level <= 0 || level > botLevel + STONE_MAX_OVER ||
					(m.targetLevel != 0 && level != (int)m.targetLevel))
				continue;
			long map = 0;
			bool village = false;
			if (!PlanStoneMap(ch, vnum, dwNow, map, village))
				continue;
			// Far over the stone and not on its map: it goes only when it wins
			// the draw; otherwise it breaks the stone when it meets one.
			const int gap = botLevel - level;
			if (map != ch->GetMapIndex() && gap > METIN_FAR_GAP_1 &&
					number(1, 100) > (gap > METIN_FAR_GAP_2 ? METIN_FAR_CHANCE_2 : METIN_FAR_CHANCE_1))
			{
				++s_uMetinFar;
				continue;
			}
			plan.stone = vnum;
			plan.map = map;
			plan.village = village;
			return true;
		}
		return false;
	}

	bool PlanBoss(LPCHARACTER ch, const TPlayerBotAIState& state, const mt2009_battlepass::Mission& m)
	{
		if (IsPlayerBotDropper(state.bPersonality) || ch->GetSkillGroup() == 0)
			return false;
		const int level = (int)ch->GetLevel();
		for (size_t i = 0; i < PLAYERBOT_WORLD_BOSS_COUNT; ++i)
		{
			const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[i];
			if ((m.target != 0 && m.target != row.wRace) || level < (int)row.bMinLevel ||
					level > (int)row.bMaxLevel || !IsPlayerBotMapHostedHere(row.lMap))
				continue;
			if (m.targetLevel != 0 && StoneLevel(row.wRace) != (int)m.targetLevel)
				continue;
			return true;
		}
		return false;
	}

	bool PlanFish(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		return !IsPlayerBotDropper(state.bPersonality) && ch->GetLevel() >= PLAYERBOT_FISHING_MIN_LEVEL &&
				CanPlayerBotUseFishingRod(ch) &&
				GetPlayerBotFishingBank(GetPlayerBotHomeMap(ch, playerbot_empire_rules::MAP_ROLE_M1)) != NULL;
	}

	bool PlanRefine(LPCHARACTER ch)
	{
		return IsPlayerBotVillageMap(ch->GetMapIndex()) && HasPlayerBotRefineOpportunity(ch);
	}

	void EndErrand(LPCHARACTER ch, TPlayerBotAIState& state, TBot& b, DWORD dwNow, const char* reason)
	{
		if (b.bGoal == GOAL_NONE)
			return;
		// The stone expedition this errand began ends with it.
		if (b.bGoal == GOAL_METIN && b.dwExpeditionUntil != 0 &&
				state.dwMetinExpeditionUntil == b.dwExpeditionUntil && (int)(dwNow - b.dwExpeditionUntil) < 0)
			state.dwMetinExpeditionUntil = dwNow;
		sys_log(0, "PLAYERBOT_BP: end pid=%u name=%s level=%u goal=%s mission=%u stone=%u map=%ld reason=%s",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), GoalName(b.bGoal), b.dwMission,
				b.dwStone, b.lMap, reason);
		const bool done = !strcmp(reason, "done");
		if (done)
			++s_uFinished;
		b.bFollowGoal = done ? b.bGoal : (BYTE)GOAL_NONE;
		b.bGoal = GOAL_NONE;
		b.dwMission = b.dwStone = 0;
		b.lMap = 0;
		b.bVillage = false;
		b.dwUntil = b.dwExpeditionUntil = 0;
		b.dwRestUntil = done ? 0 : dwNow + number(REST_AFTER_ERRAND_MIN_MS, REST_AFTER_ERRAND_MAX_MS);
	}

	void Adopt(LPCHARACTER ch, TPlayerBotAIState& state, TBot& b, const TPlan& plan, DWORD dwNow)
	{
		b.bGoal = plan.goal;
		b.dwMission = plan.mission->id;
		b.dwStone = plan.stone;
		b.lMap = plan.map;
		b.bVillage = plan.village;
		b.dwExpeditionUntil = 0;
		switch (plan.goal)
		{
			case GOAL_METIN:
				b.dwUntil = dwNow + METIN_ERRAND_MS + number(0, 10 * 60 * 1000);
				// A stone hunter by role hunts already; everybody else is on an
				// expedition for the errand.
				if (!IsPlayerBotMetinHunting(state, dwNow) ||
						(state.dwMetinExpeditionUntil != 0 && (int)(state.dwMetinExpeditionUntil - b.dwUntil) < 0))
				{
					if (state.bBotRole != BOT_ROLE_METIN_HUNTER)
					{
						state.dwMetinExpeditionUntil = b.dwUntil;
						b.dwExpeditionUntil = b.dwUntil;
					}
				}
				state.dwHubChosenTime = 0;
				break;
			case GOAL_FISH:
				b.dwUntil = dwNow + FISH_ERRAND_MS;
				state.dwNextFishingCheckTime = 0;
				break;
			case GOAL_REFINE:
				b.dwUntil = dwNow + REFINE_ERRAND_MS;
				state.dwNextShopCheckTime = 0;
				break;
			case GOAL_BOSS:
				b.dwUntil = dwNow + BOSS_ERRAND_MS;
				break;
			default:
				b.dwUntil = dwNow;
				break;
		}
		++s_uAdopted;
		if (plan.goal < 5)
			++s_auAdoptedGoal[plan.goal];
		sys_log(0, "PLAYERBOT_BP: adopt pid=%u name=%s level=%u personality=%u goal=%s mission=%u type=%u stone=%u map=%ld village=%d at=%ld minutes=%u",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), (unsigned int)state.bPersonality,
				GoalName(b.bGoal), b.dwMission, (unsigned int)plan.mission->type, b.dwStone, b.lMap,
				b.bVillage ? 1 : 0, ch->GetMapIndex(), (unsigned int)((b.dwUntil - dwNow) / 60000));
	}

	// ---- the shouts ---------------------------------------------------------

	// What the shout channel carries, one kind drawn per line. Every line
	// counts for the shout missions (BattlePassOnShout), whatever it says.
	enum EChatter
	{
		CHAT_BP = 0,
		CHAT_BUY,
		CHAT_SELL,
		CHAT_PARTY,
		CHAT_METIN,
		CHAT_BOSS,
		CHAT_GEAR,
		CHAT_TALK,
		CHAT_KIND_COUNT
	};
	// The draw's weights, in that order ("!BP" only for a bot with a shout
	// mission open; a kind with nothing to say falls through to small talk).
	const int CHATTER_WEIGHTS[CHAT_KIND_COUNT] = { 35, 12, 12, 10, 8, 5, 6, 12 };

	template <size_t N>
	const char* PickLine(const char* const (&pool)[N])
	{
		return pool[number(0, (int)N - 1)];
	}

	// How players call the maps on the channel.
	const char* PlaceName(long map)
	{
		if (IsPlayerBotVillageMap(map))
			return GetPlayerBotTownName(map);
		if (IsPlayerBotM3Map(map))
			return "M3";
		switch (map)
		{
			case PLAYERBOT_MAP_ORC_VALLEY: return "Dolinie Orkow";
			case PLAYERBOT_MAP_DESERT: return "Pustyni";
			case PLAYERBOT_MAP_SOHAN: return "Sohanie";
			case PLAYERBOT_MAP_SPIDER_V1: case PLAYERBOT_MAP_SPIDER_V2: return "Lochu Pajakow";
			case PLAYERBOT_MAP_HWANG: return "Hwang";
			case PLAYERBOT_MAP_FOREST: return "Lesie Duchow";
			case PLAYERBOT_MAP_RED_FOREST: return "Czerwonym Lesie";
			case PLAYERBOT_MAP_DEMON_TOWER: return "Wiezy Demonow";
			case PLAYERBOT_MAP_FIRE_LAND: return "Doyyumhwaji";
			case PLAYERBOT_MAP_GROTTO_V1: case PLAYERBOT_MAP_GROTTO_V2: return "Grocie";
			default: return "expowisku";
		}
	}

	const char* JobName(LPCHARACTER ch)
	{
		switch (ch->GetJob())
		{
			case JOB_WARRIOR: return "woj";
			case JOB_ASSASSIN: return "ninja";
			case JOB_SURA: return "sura";
			case JOB_SHAMAN: return "szaman";
			default: return "postac";
		}
	}

	bool LineBP(const TBot& b, char* out, size_t size)
	{
		if (!b.bShout)
			return false;
		static const char* const pool[] = {
			"!BP", "!BP", "!BP", "!BP", "!BP", "!BP", "!BP", "!BP",
			"!BP robie misje", "!BP kto jeszcze robi?", "!BP !BP", "!bp",
		};
		snprintf(out, size, "%s", PickLine(pool));
		return true;
	}

	// What the bot is short of (the market's own list), asked for.
	bool LineBuy(LPCHARACTER ch, char* out, size_t size)
	{
		std::set<DWORD> wanted;
		CollectPlayerBotWantedMaterials(ch, wanted);
		if (wanted.empty())
			return false;
		std::set<DWORD>::const_iterator it = wanted.begin();
		std::advance(it, number(0, (int)wanted.size() - 1));
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(*it);
		if (!proto)
			return false;
		static const char* const pool[] = {
			"Kupie %s, pw", "Kupie %s, place dobrze", "Ktos ma %s? kupie", "Szukam %s, oferty na pw",
		};
		snprintf(out, size, PickLine(pool), proto->szLocaleName);
		return true;
	}

	// Something from its bag: a piece of gear with a few pluses, a material,
	// a book.
	bool LineSell(LPCHARACTER ch, char* out, size_t size)
	{
		if (!ch->IsItemLoaded())
			return false;
		std::vector<LPITEM> goods;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->IsEquipped() || !item->GetProto())
				continue;
			const BYTE type = item->GetType();
			if (((type == ITEM_WEAPON || type == ITEM_ARMOR) && item->GetRefineLevel() >= 4) ||
					type == ITEM_MATERIAL || type == ITEM_SKILLBOOK || type == ITEM_METIN)
				goods.push_back(item);
		}
		if (goods.empty())
			return false;
		LPITEM item = goods[number(0, (int)goods.size() - 1)];
		const int pick = number(0, 3);
		if (pick == 3)
			snprintf(out, size, "Sprzedam %s tanio, jestem w %s", item->GetProto()->szLocaleName,
					PlaceName(ch->GetMapIndex()));
		else
		{
			static const char* const pool[] = { "Sprzedam %s, pw", "%s na sprzedaz, oferty pw", "Oddam %s za rozsadna cene" };
			snprintf(out, size, pool[pick], item->GetProto()->szLocaleName);
		}
		return true;
	}

	bool LineParty(LPCHARACTER ch, char* out, size_t size)
	{
		const char* place = PlaceName(ch->GetMapIndex());
		LPPARTY party = ch->GetParty();
		if (party && party->GetLeaderPID() == ch->GetPlayerID())
		{
			const int missing = std::max(1, 8 - (int)party->GetMemberCount());
			snprintf(out, size, "PT na %s, brakuje %d osob, %d+ lvl, wbijac", place, missing,
					std::max(1, (int)ch->GetLevel() - 5));
			return true;
		}
		if (party)
			return false;
		static const char* const pool[] = {
			"Szukam PT na %s, %d lvl %s", "Kto da PT? %s, %d lvl, %s", "%s - ktos expi? %d lvl %s, dolacze",
		};
		snprintf(out, size, PickLine(pool), place, (int)ch->GetLevel(), JobName(ch));
		return true;
	}

	bool LineMetin(LPCHARACTER ch, const TBot& b, char* out, size_t size)
	{
		const char* place = PlaceName(ch->GetMapIndex());
		if (b.bGoal == GOAL_METIN && b.dwStone != 0)
		{
			const int level = StoneLevel(b.dwStone);
			static const char* const pool[] = {
				"Bije metiny %d lvl w %s, kto dolaczy?", "Ktos widzial metina %d lvl w %s?",
				"Robie metiny %d lvl do BP, %s, pomoze ktos?",
			};
			snprintf(out, size, PickLine(pool), level, place);
			return true;
		}
		if (!PlayerBotMapHasMetinStones(ch->GetMapIndex()))
			return false;
		static const char* const pool[] = {
			"Kto na metiny w %s?", "Zbijam metiny w %s, kto chetny?", "Ile metinow stoi teraz w %s?",
		};
		snprintf(out, size, PickLine(pool), place);
		return true;
	}

	bool LineBoss(const TBot& b, char* out, size_t size)
	{
		if (!b.bAnyBoss && b.bosses.empty())
			return false;
		static const char* const pool[] = {
			"Kto idzie na bossa? robie misje BP", "Ktos zna respa bossa?", "Zbieram ekipe na bossa, pw",
			"Boss padl? kto bil?",
		};
		snprintf(out, size, "%s", PickLine(pool));
		return true;
	}

	bool LineGear(LPCHARACTER ch, char* out, size_t size)
	{
		LPITEM weapon = ch->IsItemLoaded() ? ch->GetWear(WEAR_WEAPON) : NULL;
		if (!weapon || !weapon->GetProto() || weapon->GetRefineLevel() < 3)
			return false;
		static const char* const pool[] = {
			"Mam %s, ile to warte?", "Ulepszac %s dalej czy nie ryzykowac?", "Ktos da cos za %s?",
			"%s, kowal znowu mnie oskubal",
		};
		snprintf(out, size, PickLine(pool), weapon->GetProto()->szLocaleName);
		return true;
	}

	void LineTalk(LPCHARACTER ch, char* out, size_t size)
	{
		static const char* const pool[] = {
			"siema wszystkim", "elo", "jaki dzis drop?", "ktos cos dropnal ciekawego?", "nudy dzis",
			"kto gra wieczorem?", "lag czy tylko u mnie?", "kowal dzis zly jak nigdy", "jak tam expienie?",
			"ile jeszcze do konca BP?", "kto robi BP?", "gdzie najlepiej expic na %d lvl?",
			"pozdro dla ekipy", "ale dzis tlok na mapie", "kto na wojne?", "sprzedam wszystko xd",
			"ile teraz stoi Cor Draconis?", "ktos ma wolny slot w gildii?", "szukam gildii, %d lvl",
			"dzieki za pomoc przy metinie", "hmm", "kto mi pomoze z misja?",
		};
		const char* line = PickLine(pool);
		if (strstr(line, "%d"))
			snprintf(out, size, line, (int)ch->GetLevel());
		else
			snprintf(out, size, "%s", line);
	}

	// A bot that may shout now: its tick running, its kingdom, the engine's
	// level, not somebody's companion or mercenary, its own pause over.
	LPCHARACTER ChatterCandidate(DWORD pid, const TBot& b, BYTE empire, DWORD dwNow)
	{
		if (dwNow - b.dwLastSeen >= SEEN_RECENTLY_MS || (b.dwNextChatter != 0 && (int)(dwNow - b.dwNextChatter) < 0))
			return NULL;
		LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(pid);
		if (!c || !mt2009_battlepass::IsBot(c) || c->GetEmpire() != empire || c->IsDead() ||
				(int)c->GetLevel() < g_iShoutLimitLevel || IsPlayerBotSidekickPID(pid) || IsPlayerBotOnMercContract(pid) ||
				IsPlayerBotShouterPID(pid) || // MT2009_PLUS_SHOUTERS_V1: its own lines only
				IsPlayerBotMedalShouterPID(pid)) // MT2009_PLUS_MEDAL_SHOUTERS_V1: and Tieru's kind
			return NULL;
		return c;
	}

	// Each kingdom's next line on this core, when it is due: a speaker drawn
	// among its bots (now and then the one furthest along its shout
	// missions), a kind of line drawn by the weights, and out it goes.
	void ManageChatter(DWORD dwNow)
	{
		for (BYTE empire = 1; empire <= 3; ++empire)
		{
			if (s_adwNextShout[empire] == 0)
			{
				s_adwNextShout[empire] = dwNow + number(CHATTER_MIN_MS / 3, CHATTER_MAX_MS);
				continue;
			}
			if ((int)(dwNow - s_adwNextShout[empire]) < 0)
				continue;
			if (s_dwPlayerBotTradeShoutTime != 0 && dwNow - s_dwPlayerBotTradeShoutTime < CHATTER_QUIET_MS)
				continue;
			const bool regular = number(1, 100) <= CHATTER_REGULAR_PERCENT;
			DWORD bestPid = 0;
			LPCHARACTER best = NULL;
			DWORD bestProgress = 0;
			int seen = 0;
			for (std::map<DWORD, TBot>::const_iterator it = s_mapBots.begin(); it != s_mapBots.end(); ++it)
			{
				if (regular && !it->second.bShout)
					continue;
				LPCHARACTER c = ChatterCandidate(it->first, it->second, empire, dwNow);
				if (!c)
					continue;
				++seen;
				// The regular: the furthest along. Anybody else: one drawn at
				// random from all of them (reservoir).
				if (regular ? (!best || it->second.dwShoutProgress > bestProgress) : number(1, seen) == 1)
				{
					best = c;
					bestPid = it->first;
					bestProgress = it->second.dwShoutProgress;
				}
			}
			if (!best)
			{
				s_adwNextShout[empire] = dwNow + (regular ? 1000 : CHATTER_MIN_MS);
				continue;
			}
			TBot& b = s_mapBots[bestPid];
			int total = 0;
			for (int k = 0; k < CHAT_KIND_COUNT; ++k)
				total += (k == CHAT_BP && !b.bShout) ? 0 : CHATTER_WEIGHTS[k];
			int roll = number(1, total);
			int kind = 0;
			for (; kind < CHAT_KIND_COUNT - 1; ++kind)
			{
				if (kind == CHAT_BP && !b.bShout)
					continue;
				roll -= CHATTER_WEIGHTS[kind];
				if (roll <= 0)
					break;
			}
			// The regular is there for the mission: "!BP" mostly.
			if (regular && b.bShout && number(1, 100) <= 70)
				kind = CHAT_BP;
			char text[CHAT_MAX_LEN + 1];
			text[0] = 0;
			bool said = false;
			switch (kind)
			{
				case CHAT_BP: said = LineBP(b, text, sizeof(text)); break;
				case CHAT_BUY: said = LineBuy(best, text, sizeof(text)); break;
				case CHAT_SELL: said = LineSell(best, text, sizeof(text)); break;
				case CHAT_PARTY: said = LineParty(best, text, sizeof(text)); break;
				case CHAT_METIN: said = LineMetin(best, b, text, sizeof(text)); break;
				case CHAT_BOSS: said = LineBoss(b, text, sizeof(text)); break;
				case CHAT_GEAR: said = LineGear(best, text, sizeof(text)); break;
				default: break;
			}
			if (!said)
			{
				kind = b.bShout && number(0, 1) == 0 ? CHAT_BP : CHAT_TALK;
				if (kind == CHAT_BP)
					LineBP(b, text, sizeof(text));
				else
					LineTalk(best, text, sizeof(text));
			}
			char msg[CHAT_MAX_LEN + 1];
			snprintf(msg, sizeof(msg), "%s : %s", best->GetName(), text);
			SendPlayerBotShout(msg, empire);
			BattlePassOnShout(best);
			++b.dwShoutProgress;
			++s_uShouts;
			++s_auChatter[kind];
			b.dwNextChatter = dwNow + (regular ? CHATTER_REGULAR_COOLDOWN_MS
					: (DWORD)number(CHATTER_BOT_COOLDOWN_MIN_MS, CHATTER_BOT_COOLDOWN_MAX_MS));
			s_adwNextShout[empire] = dwNow + number(CHATTER_MIN_MS, CHATTER_MAX_MS);
		}
	}

	void Census(DWORD dwNow)
	{
		if (s_dwNextCensus != 0 && (int)(dwNow - s_dwNextCensus) < 0)
			return;
		s_dwNextCensus = dwNow + CENSUS_MS;
		unsigned int goals[5] = { 0, 0, 0, 0, 0 };
		unsigned int shouters = 0, named = 0, seen = 0;
		for (std::map<DWORD, TBot>::iterator it = s_mapBots.begin(); it != s_mapBots.end(); )
		{
			// Gone from this core: forgotten.
			if (dwNow - it->second.dwLastSeen > 30 * 60 * 1000)
			{
				s_mapBots.erase(it++);
				continue;
			}
			if (dwNow - it->second.dwLastSeen < SEEN_RECENTLY_MS)
				++seen;
			if (IsActive(it->second, dwNow) && it->second.bGoal < 5)
			{
				++goals[it->second.bGoal];
				if (it->second.bGoal == GOAL_METIN && it->second.dwStone != 0)
					++named;
			}
			if (it->second.bShout)
				++shouters;
			++it;
		}
		sys_log(0, "PLAYERBOT_BP: census bots=%u seen=%u metin=%u(named=%u budget=%d any_budget=%d) fish=%u refine=%u boss=%u shout_open=%u adopted=%u done=%u shouts=%u",
				(unsigned int)s_mapBots.size(), seen, goals[GOAL_METIN], named,
				std::max(METIN_BUDGET_MIN, (int)seen * METIN_BUDGET_PERCENT / 100),
				std::max(METIN_BUDGET_MIN, (int)seen * METIN_ANY_BUDGET_PERCENT / 100),
				goals[GOAL_FISH], goals[GOAL_REFINE], goals[GOAL_BOSS], shouters, s_uAdopted, s_uFinished, s_uShouts);
		sys_log(0, "PLAYERBOT_BP: draws rolls=%u by_the_way=%u follow=%u nothing=%u metin_budget_full=%u metin_far_declined=%u metin_crowd=%u adopted_metin=%u fish=%u refine=%u boss=%u",
				s_uRolls, s_uByTheWay, s_uFollow, s_uNothing, s_uMetinBudget, s_uMetinFar, s_uMetinCrowd,
				s_auAdoptedGoal[GOAL_METIN], s_auAdoptedGoal[GOAL_FISH], s_auAdoptedGoal[GOAL_REFINE],
				s_auAdoptedGoal[GOAL_BOSS]);
		sys_log(0, "PLAYERBOT_BP: chatter bp=%u buy=%u sell=%u party=%u metin=%u boss=%u gear=%u talk=%u",
				s_auChatter[CHAT_BP], s_auChatter[CHAT_BUY], s_auChatter[CHAT_SELL], s_auChatter[CHAT_PARTY],
				s_auChatter[CHAT_METIN], s_auChatter[CHAT_BOSS], s_auChatter[CHAT_GEAR], s_auChatter[CHAT_TALK]);
		s_uRolls = s_uByTheWay = s_uFollow = s_uNothing = s_uMetinBudget = s_uMetinFar = s_uMetinCrowd = 0;
		for (int k = 0; k < 5; ++k)
			s_auAdoptedGoal[k] = 0;
		for (int k = 0; k < 10; ++k)
			s_auChatter[k] = 0;
	}

	// ---- the tick -----------------------------------------------------------

	// Every tick of every bot, before the planner: cheap until its next look.
	void Think(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !mt2009_battlepass::IsBot(ch))
			return;
		using namespace mt2009_battlepass;
		const DWORD pid = ch->GetPlayerID();
		TBot& b = s_mapBots[pid];
		b.dwLastSeen = dwNow;
		Census(dwNow);
		ManageChatter(dwNow);
		if (b.dwNextThink != 0 && (int)(dwNow - b.dwNextThink) < 0)
			return;
		b.dwNextThink = dwNow + THINK_MS + PlayerBotNavHash(pid ^ 0x42505448U) % THINK_SPREAD_MS;

		LoadMissions(false);
		if (s_vecMissions.empty())
		{
			EndErrand(ch, state, b, dwNow, "no_missions");
			b.stones.clear();
			b.mobs.clear();
			b.bosses.clear();
			b.bAnyBoss = b.bShout = false;
			return;
		}
		Cache& cache = GetCache(ch);
		std::vector<const Mission*> open;
		CollectOpen(cache, open);
		Refresh(b, cache, open);

		if (b.bGoal != GOAL_NONE)
		{
			bool stillOpen = false;
			for (size_t i = 0; i < open.size() && !stillOpen; ++i)
				stillOpen = open[i]->id == b.dwMission;
			if (!stillOpen)
				EndErrand(ch, state, b, dwNow, "done");
			else if (!IsActive(b, dwNow))
				EndErrand(ch, state, b, dwNow, "time");
			// The panel's BATTLEPASS will at zero: no bot keeps an errand.
			else if (s_iPlayerBotBattlePassPercent <= 0)
				EndErrand(ch, state, b, dwNow, "will_off");
			// A person's business comes first; a bot party it was in leaves it
			// (IsPlayerBotPartyEligible says no while the errand lasts).
			else if (IsPlayerBotHeldForCompany(ch) || IsPlayerBotInDungeonBusiness(ch, state) ||
					state.bWorldEventKind != 0 || (ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty())))
				EndErrand(ch, state, b, dwNow, "busy");
			// Another rule ended the expedition (a rare personality's end, the
			// dropper's rule): the errand gives way.
			else if (b.bGoal == GOAL_METIN && b.dwExpeditionUntil != 0 &&
					state.dwMetinExpeditionUntil != b.dwExpeditionUntil)
				EndErrand(ch, state, b, dwNow, "expedition_ended");
			else if (b.bGoal == GOAL_REFINE && !HasPlayerBotRefineOpportunity(ch))
				EndErrand(ch, state, b, dwNow, "nothing_to_refine");
			else
				return;
		}

		if (b.dwRestUntil != 0 && (int)(dwNow - b.dwRestUntil) < 0)
			return;
		const BYTE follow = b.bFollowGoal;
		if (open.empty())
		{
			b.bFollowGoal = GOAL_NONE;
			return;
		}
		// Still busy with what finished the mission (the fishing session that
		// caught the last fish, a raid): the chain's next errand waits for the
		// next free look instead of being lost - it was cleared before this
		// test, and 1 follow in 8468 draws came of 288 errands done (29 September).
		if (IsBusy(ch, state))
			return;
		b.bFollowGoal = GOAL_NONE;
		++s_uRolls;
		if (follow != GOAL_NONE)
			++s_uFollow;
		// The panel's BATTLEPASS will (playerbot_config.h): the chance scaled,
		// and the next errand of a chain drawn against it too - at 100 the
		// build's draw, at 0 no errand at all (what a bot does by the way
		// still counts for its missions).
		const int will = s_iPlayerBotBattlePassPercent;
		if (follow == GOAL_NONE ? number(1, 100) > AdoptChance(ch, state) * will / 100 :
				(will < 100 && number(1, 100) > will))
		{
			++s_uByTheWay;
			// By the way, this time; the next draw jittered by the pid.
			b.dwRestUntil = dwNow + REST_BY_THE_WAY_MIN_MS +
					(PlayerBotNavHash(pid ^ 0x52455354U ^ (dwNow / 60000U)) %
						(REST_BY_THE_WAY_MAX_MS - REST_BY_THE_WAY_MIN_MS));
			return;
		}

		// Every errand it could take on now, weighed by its personality.
		std::vector<TPlan> plans;
		const BYTE personality = state.bPersonality;
		for (size_t i = 0; i < open.size(); ++i)
		{
			const Mission& m = *open[i];
			TPlan plan;
			plan.goal = GOAL_NONE;
			plan.mission = &m;
			plan.stone = 0;
			plan.map = 0;
			plan.village = false;
			plan.weight = 0;
			switch (m.type)
			{
				case TYPE_METIN:
					if (PlanMetin(ch, state, m, dwNow, plan))
					{
						plan.weight = plan.stone != 0 ? 4 : 2;
						if (personality == BOT_PERSONALITY_METIN_BREAKER || personality == BOT_PERSONALITY_METIN_DROPPER ||
								state.bBotRole == BOT_ROLE_METIN_HUNTER)
							plan.weight *= 2;
					}
					break;
				case TYPE_FISH:
					if (PlanFish(ch, state))
					{
						plan.goal = GOAL_FISH;
						plan.weight = personality == BOT_PERSONALITY_CAREFUL_COLLECTOR ? 6 : 2;
					}
					break;
				case TYPE_REFINE:
					if (PlanRefine(ch))
					{
						plan.goal = GOAL_REFINE;
						plan.weight = personality == BOT_PERSONALITY_GEAR_SPECIALIST ? 6 : 2;
					}
					break;
				case TYPE_BOSS:
					if (PlanBoss(ch, state, m))
					{
						plan.goal = GOAL_BOSS;
						plan.weight = personality == BOT_PERSONALITY_TEAM_COMPANION ? 6 : 3;
					}
					break;
				default:
					break;
			}
			if (plan.goal != GOAL_NONE && plan.weight > 0)
			{
				if (follow != GOAL_NONE && plan.goal == follow)
					plan.weight *= 8;
				plans.push_back(plan);
			}
		}
		// The core's budget of stone errands: over it, the stones wait for
		// another draw and the bot takes one of its other errands, if any.
		{
			int named = 0, any = 0, seen = 0;
			for (std::map<DWORD, TBot>::const_iterator it = s_mapBots.begin(); it != s_mapBots.end(); ++it)
			{
				if (dwNow - it->second.dwLastSeen < SEEN_RECENTLY_MS)
					++seen;
				if (it->second.bGoal == GOAL_METIN && IsActive(it->second, dwNow))
				{
					if (it->second.dwStone != 0)
						++named;
					else
						++any;
				}
			}
			const int namedBudget = std::max(METIN_BUDGET_MIN, seen * METIN_BUDGET_PERCENT / 100);
			const int anyBudget = std::max(METIN_BUDGET_MIN, seen * METIN_ANY_BUDGET_PERCENT / 100);
			bool cut = false;
			for (size_t i = 0; i < plans.size(); )
				if (plans[i].goal == GOAL_METIN &&
						(plans[i].stone != 0 ? named >= namedBudget : any >= anyBudget))
				{
					plans.erase(plans.begin() + i);
					cut = true;
				}
				else
					++i;
			if (cut)
				++s_uMetinBudget;
		}
		if (plans.empty())
		{
			++s_uNothing;
			// Seeded by the pid, so the bots that drew nothing do not all look
			// again on the same tick.
			b.dwRestUntil = dwNow + REST_BY_THE_WAY_MIN_MS / 2 +
					(PlayerBotNavHash(pid ^ (dwNow / 60000U)) % (REST_BY_THE_WAY_MAX_MS / 2));
			return;
		}
		int total = 0;
		for (size_t i = 0; i < plans.size(); ++i)
			total += plans[i].weight;
		int roll = number(1, total);
		size_t pick = 0;
		for (; pick + 1 < plans.size(); ++pick)
		{
			roll -= plans[pick].weight;
			if (roll <= 0)
				break;
		}
		Adopt(ch, state, b, plans[pick], dwNow);
	}
}

#endif
