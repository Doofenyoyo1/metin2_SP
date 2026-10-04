#ifndef __INC_METIN2_PLAYERBOT_AREZZO_DUNGEON_BOTS_H__
#define __INC_METIN2_PLAYERBOT_AREZZO_DUNGEON_BOTS_H__

// MT2009_PLUS_AREZZO_DUNGEON_BOTS_V1 - the operator's test cohort for the three
// Arezzo dungeons (the owner, 1 October: "Zrob specjalnie boty po 30 sztuk na
// 3 nowe dungi Arezzo ... niech biegaja w kolko", the test server only):
//
//   * Wzgorze Wukonga (364, wzgorze_wukonga.quest): three waves of 60 monkeys,
//     each ending in the Seal of the Hill (30766) the last killer uses; 6 Hill
//     Stones (9701); 3 Cloud Defenders (9683); 4 Phoenix Eggs (9702); the
//     Flaming Phoenix (9684); WuKong (9682);
//   * Ruiny Skorpiona (365, ruiny_skorpiona.quest): three waves of 50
//     scorpions (seal 30767); 6 Scorpion Metins (9696); 3 Red Scorpions
//     (9695); 4 Scorpion Metins (9696); the Scorpion King (9694);
//   * Starozytna Dzungla (366, starozytna_dzungla.quest): three waves of 40
//     (seal 30768); 6 Ancient Stones (9710); 3 Faethorns (9712); 4 Primeval
//     Stones (9711); the Royal Owl (9713); the Jungle Queen (9714).
//
// Every stage is fought in one open arena round the guard's spot (all of it
// one walkable piece: tools/arezzo_dungeon_map.py measured every regen, stone
// and boss point from the entry), so the plan of a run is the quests' own
// stage flag and a target list per stage: what attacks the bot, then the
// stage's objective (the stones, the defenders, the eggs, the boss), then
// the nearest monster - and the seal used as soon as a squad member holds it.
//
// Where: the dungeons live on game2 of channel 1 (MAP_ALLOW 364 365 366) and
// a bot cannot cross cores, so the cohort lives there too. Each character's
// save point is the dungeon map itself, beside its guard - the lobby the
// quests already have for a player whose one-load entry falls back
// ("Otworz przejscie", map 364/365/366 not in an instance). The cohort file
// "playerbot_arezzo_dungeon_cohort.txt" ("<wukong|skorpion|dzungla> <pid>"
// a line) is read on the core that hosts the maps (its bots are logged in on
// top of nothing - game2 has no population) and on every other core, which
// refuses those pids (CPlayerBotManager::Spawn), so no character is loaded
// twice. tools/arezzo_dungeon_cohort.py picks and equips the characters.
//
// The run: bots of one kingdom gather in the lobby in fives (a Shaman's buffs
// reach its own kingdom only); a squad, or whoever has waited
// PLAYERBOT_ARZDG_SQUAD_WAIT_MS, goes in. What the guard's "Wejdz" does is
// done here server-side - a new instance of the map, the squad jumped into it
// at the quest's entry point (WarpSet, which moves a bot through WarpBot),
// the save point set to the lobby - and the quest's own start is asked for
// through its server timer "<x>_dgbot", which runs init() for an instance
// carrying the dungeon flag "mt2009_dgbot" (nothing a player can set). Such
// an instance has no time limit (the quests' tick leaves its minutes alone),
// no fee and no daily limit (no guard, no login payment), and its end sends
// the squad back to the lobby instead of the entry map. After a pause it goes
// in again, for as long as the cohort file names it.
//
// The test hook, for the operator: a file "playerbot_arezzo_dungeon_test" in
// the hosting core's directory:
//     cohort                  read the cohort file again and log its bots in;
//     stop <key|all>          no new runs of that dungeon (the running end);
//     start <key|all>         new runs again;
//     abort <key|all>         every squad of it out to the lobby now;
//     status                  one ARZ_DG line per dungeon.
// The watch writes ARZ_DG lines to syslog (entered, stage, seal, finished,
// stalled, abandoned, died, wipe, stuck), one row per run to
// playerbot_arezzo_dungeon_runs.tsv and the cohort's state to
// playerbot_arezzo_dungeon_track.tsv every 30 s.
//
// An implementation fragment in the sense playerbot_types.h describes:
// include it once, after playerbot_catacomb.h (the fight is the tower's).

#include "dungeon.h"
#include "questevent.h"

namespace
{
	// ------------------------------------------------------------ the dungeons

	struct TPlayerBotArzDg
	{
		const char* szKey;
		const char* szName;
		const char* szInitTimer;	// the quest's server timer that runs init() for a bot instance
		long lMap;
		long lEntryCellX, lEntryCellY;	// the quests' cfg().entry: the jump in and the lobby
		int iStages;
		DWORD dwSeal;
		DWORD adwTargets[6][4];		// the objective of stage 1..6
		// Where the squad fights the waves (stage 1), or 0: the guard's spot.
		long lCampCellX, lCampCellY;
	};

	const TPlayerBotArzDg PLAYERBOT_ARZDG[] = {
		{ "wukong", "Wzgorze Wukonga", "wukong_dgbot", 364, 264, 273, 6, 30766,
			{ { 9690, 9691, 9692, 9693 }, { 9701 }, { 9683 }, { 9702 }, { 9684 }, { 9682 } }, 0, 0 },
		{ "skorpion", "Ruiny Skorpiona", "skorpion_dgbot", 365, 268, 228, 5, 30767,
			{ { 9697, 9698, 9699, 9700 }, { 9696 }, { 9695 }, { 9696 }, { 9694 }, { 0 } }, 258, 250 },
		{ "dzungla", "Starozytna Dzungla", "dzungla_dgbot", 366, 384, 374, 6, 30768,
			{ { 9707, 9708, 9709 }, { 9710 }, { 9712 }, { 9711 }, { 9713 }, { 9714 } }, 354, 368 },
	};
	const int PLAYERBOT_ARZDG_COUNT = (int)(sizeof(PLAYERBOT_ARZDG) / sizeof(PLAYERBOT_ARZDG[0]));

	// ------------------------------------------------------------ the numbers

	const size_t PLAYERBOT_ARZDG_SQUAD_SIZE = 5;
	// Fewer go in after this long in the lobby (a squad that lost members to
	// a restart, the kingdom's last four).
	const DWORD PLAYERBOT_ARZDG_SQUAD_WAIT_MS = 120000;
	// The pause in the lobby between two runs.
	const DWORD PLAYERBOT_ARZDG_PAUSE_MS = 30000;
	// A run whose flags and monsters have not moved this long is logged as
	// stalled, and after the second figure pulled out and counted abandoned.
	const DWORD PLAYERBOT_ARZDG_STALL_MS = 4 * 60 * 1000;
	const DWORD PLAYERBOT_ARZDG_ABANDON_MS = 15 * 60 * 1000;
	// The quests send everybody out a minute after the last boss; whoever is
	// still inside this long after it (or after "closed") is walked out here.
	const DWORD PLAYERBOT_ARZDG_EXIT_GRACE_MS = 90000;
	// A bot that has not moved this far in this long, while not fighting, is stuck.
	const int PLAYERBOT_ARZDG_STUCK_DISTANCE = 200;
	const DWORD PLAYERBOT_ARZDG_STUCK_MS = 60000;
	// Potions: drunk from this much health; bought in place under these.
	const int PLAYERBOT_ARZDG_POTION_HP = 80;
	const int PLAYERBOT_ARZDG_POTION_SP = 40;
	const size_t PLAYERBOT_ARZDG_KEEP_RED = 150;
	const size_t PLAYERBOT_ARZDG_KEEP_BLUE = 100;
	// What attacks a bot is fought first within this.
	const int PLAYERBOT_ARZDG_THREAT_RANGE = 1500;
	// The squad fights as one pack (round 1: five bots each after its own
	// nearest monster pulled five packs at once and died 190 times in five
	// minutes in the Ruins): a target is chosen from the pack's middle, and a
	// bot further than this from it with nothing on it walks back.
	const int PLAYERBOT_ARZDG_PACK_LEASH = 1200;
	const DWORD PLAYERBOT_ARZDG_TRACK_MS = 30000;
	const DWORD PLAYERBOT_ARZDG_COHORT_DELAY_MS = 20000;
	const char* const PLAYERBOT_ARZDG_COHORT_FILE = "playerbot_arezzo_dungeon_cohort.txt";
	const char* const PLAYERBOT_ARZDG_TEST_FILE = "playerbot_arezzo_dungeon_test";
	const char* const PLAYERBOT_ARZDG_TRACK_FILE = "playerbot_arezzo_dungeon_track.tsv";
	const char* const PLAYERBOT_ARZDG_RUNS_FILE = "playerbot_arezzo_dungeon_runs.tsv";

	// ------------------------------------------------------------ the state

	// The cohort: pid -> dungeon (an index of PLAYERBOT_ARZDG).
	std::map<DWORD, int> s_mapPlayerBotArzDgCohort;
	bool s_bPlayerBotArzDgCohortLoaded = false;
	bool s_bPlayerBotArzDgHosting = false;
	bool s_abPlayerBotArzDgStopped[PLAYERBOT_ARZDG_COUNT] = { false, false, false };
	// A start that failed (no instance, nobody jumped) waits this long.
	DWORD s_adwPlayerBotArzDgNextStart[PLAYERBOT_ARZDG_COUNT] = { 0, 0, 0 };

	struct TPlayerBotArzDgRun
	{
		int iId;
		int iDg;
		long lInstance;
		BYTE bEmpire;
		std::vector<DWORD> members;
		DWORD dwStart;
		int iStage;
		DWORD dwStageSince;
		DWORD adwStageMs[8];		// how long each stage (1..7) took
		long long llSignature;
		int iMonsters;
		DWORD dwLastProgress;
		bool bStallLogged;
		DWORD dwFinishedAt;
		DWORD dwClosedSeen;
		int iDeaths;
		int iWipes;
		bool bAllDead;
		std::set<DWORD> dead;
		// The pack: where its living members stand, on average (each second).
		long lPackX, lPackY;
		int iPackN;
		TPlayerBotArzDgRun() : lPackX(0), lPackY(0), iPackN(0), iId(0), iDg(0), lInstance(0), bEmpire(0), dwStart(0), iStage(0), dwStageSince(0),
				llSignature(0), iMonsters(-1), dwLastProgress(0), bStallLogged(false), dwFinishedAt(0),
				dwClosedSeen(0), iDeaths(0), iWipes(0), bAllDead(false)
		{
			for (int i = 0; i < 8; ++i)
				adwStageMs[i] = 0;
		}
	};
	std::map<int, TPlayerBotArzDgRun> s_mapPlayerBotArzDgRuns;
	int s_iPlayerBotArzDgNextRun = 1;
	std::map<DWORD, int> s_mapPlayerBotArzDgBotRun;

	struct TPlayerBotArzDgBot
	{
		DWORD dwLobbySince;
		DWORD dwRestUntil;
		DWORD dwNextRestock;
		DWORD dwNextSeal;
		DWORD dwNextHome;
		long lAnchorX, lAnchorY;
		DWORD dwAnchorSince;
		bool bStuck;
		DWORD dwDeaths;
		DWORD dwRuns;
		DWORD dwFinished;
		TPlayerBotArzDgBot() : dwLobbySince(0), dwRestUntil(0), dwNextRestock(0), dwNextSeal(0), dwNextHome(0),
				lAnchorX(0), lAnchorY(0), dwAnchorSince(0), bStuck(false), dwDeaths(0), dwRuns(0), dwFinished(0) {}
	};
	std::map<DWORD, TPlayerBotArzDgBot> s_mapPlayerBotArzDgBots;

	struct TPlayerBotArzDgStats
	{
		DWORD dwStarted, dwFinished, dwAbandoned, dwLost;
		unsigned long long ullFinishedMs;
		DWORD dwBestMs, dwWorstMs;
		DWORD dwDeaths, dwWipes;
		TPlayerBotArzDgStats() : dwStarted(0), dwFinished(0), dwAbandoned(0), dwLost(0), ullFinishedMs(0),
				dwBestMs(0), dwWorstMs(0), dwDeaths(0), dwWipes(0) {}
	};
	TPlayerBotArzDgStats s_aPlayerBotArzDgStats[PLAYERBOT_ARZDG_COUNT];

	// ------------------------------------------------------------ small things

	int GetPlayerBotArzDgIndex(long mapIndex)
	{
		const long base = mapIndex >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN ? mapIndex / 10000 : mapIndex;
		for (int i = 0; i < PLAYERBOT_ARZDG_COUNT; ++i)
			if (PLAYERBOT_ARZDG[i].lMap == base)
				return i;
		return -1;
	}

	bool IsPlayerBotArezzoDungeonCohortPID(DWORD pid)
	{
		return s_mapPlayerBotArzDgCohort.find(pid) != s_mapPlayerBotArzDgCohort.end();
	}

	// A cohort pid on a core that does not host its dungeon: never logged in here.
	bool IsPlayerBotArezzoDungeonReservedPID(DWORD pid)
	{
		return !s_bPlayerBotArzDgHosting && IsPlayerBotArezzoDungeonCohortPID(pid);
	}

	// The lobby's (and the jump's) point in world units, and in cells for a
	// save point.
	bool GetPlayerBotArzDgEntry(int dg, long& x, long& y, long* cellX = NULL, long* cellY = NULL)
	{
		const TPlayerBotArzDg& info = PLAYERBOT_ARZDG[dg];
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(info.lMap);
		if (!pMap)
			return false;
		x = pMap->m_setting.iBaseX + info.lEntryCellX * 100;
		y = pMap->m_setting.iBaseY + info.lEntryCellY * 100;
		if (cellX)
			*cellX = pMap->m_setting.iBaseX / 100 + info.lEntryCellX;
		if (cellY)
			*cellY = pMap->m_setting.iBaseY / 100 + info.lEntryCellY;
		return true;
	}

	// A point as the quests' regen files write it: cells off the map's base.
	long GetPlayerBotArzDgCellX(int dg, long x)
	{
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(PLAYERBOT_ARZDG[dg].lMap);
		return pMap ? (x - pMap->m_setting.iBaseX) / 100 : x / 100;
	}
	long GetPlayerBotArzDgCellY(int dg, long y)
	{
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(PLAYERBOT_ARZDG[dg].lMap);
		return pMap ? (y - pMap->m_setting.iBaseY) / 100 : y / 100;
	}

	bool IsPlayerBotArzDgTarget(const TPlayerBotArzDg& info, int stage, DWORD race)
	{
		if (stage < 1 || stage > info.iStages || stage > 6)
			return false;
		for (int k = 0; k < 4; ++k)
			if (info.adwTargets[stage - 1][k] != 0 && info.adwTargets[stage - 1][k] == race)
				return true;
		return false;
	}

	// The experience stops where the cohort was set: the test is of the
	// dungeon at its level, and a squad that farmed its way twenty levels past
	// it would test nothing (the persona pass lifts it when the bot is let go).
	void LockPlayerBotArzDgExp(LPCHARACTER ch)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		if (ch && ch->FindAffect(AFFECT_EXP_BLOCK) == NULL)
			ch->AddAffect(AFFECT_EXP_BLOCK, POINT_NONE, 0, 0, INFINITE_AFFECT_DURATION, 0, true, true);
#else
		(void)ch;
#endif
	}

	// The belt, kept full in place out of the bot's own gold at the
	// merchant's price (the Las cohort's rule): a trip to a town is a trip off
	// this core.
	void RestockPlayerBotArzDg(LPCHARACTER ch, TPlayerBotArzDgBot& bot, DWORD dwNow)
	{
		if (dwNow < bot.dwNextRestock || ch->IsDead() || !ch->IsItemLoaded())
			return;
		bot.dwNextRestock = dwNow + 20000;
		size_t red = 0, blue = 0;
		CountPlayerBotPotions(ch, red, blue);
		const bool big = ch->GetLevel() >= PLAYERBOT_BIG_POTION_MIN_LEVEL;
		const bool caster = ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA;
		int boughtRed = 0, boughtBlue = 0;
		for (int i = 0; i < 2 && red + boughtRed < PLAYERBOT_ARZDG_KEEP_RED && ch->GetEmptyInventory(1) >= 0; ++i)
		{
			const long long cost = 200LL * (big ? 40 : 20);
			if ((long long)ch->GetGold() < cost)
				break;
			PlayerBotChangeGold(ch, -cost);
			ch->AutoGiveItem(big ? 27003 : 27002, 200);
			boughtRed += 200;
		}
		for (int i = 0; caster && i < 2 && blue + boughtBlue < PLAYERBOT_ARZDG_KEEP_BLUE && ch->GetEmptyInventory(1) >= 0; ++i)
		{
			const long long cost = 200LL * (big ? 64 : 32);
			if ((long long)ch->GetGold() < cost)
				break;
			PlayerBotChangeGold(ch, -cost);
			ch->AutoGiveItem(big ? 27006 : 27005, 200);
			boughtBlue += 200;
		}
		if (boughtRed || boughtBlue)
			sys_log(0, "ARZ_DG: restock pid=%u name=%s map=%ld red=%u blue=%u bought_red=%d bought_blue=%d gold=%lld",
					ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), (unsigned int)red, (unsigned int)blue,
					boughtRed, boughtBlue, (long long)ch->GetGold());
		else if (red < 10)
			PlayerBotLogThrottled("arzdg_restock_failed", dwNow,
					"ARZ_DG: restock impossible pid=%u name=%s red=%u free_cell=%d gold=%lld",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)red, ch->GetEmptyInventory(1) >= 0 ? 1 : 0,
					(long long)ch->GetGold());
	}

	// To the lobby by the engine's own warp (WarpBot): out of an instance, or
	// back from anywhere else on this core.
	bool SendPlayerBotArzDgHome(LPCHARACTER ch, int dg, const char* why)
	{
		long x = 0, y = 0;
		if (!ch || ch->IsDead() || !GetPlayerBotArzDgEntry(dg, x, y))
			return false;
		const long from = ch->GetMapIndex();
		const DWORD h = PlayerBotNavHash(ch->GetPlayerID() ^ 0x41525a44U);
		x += (long)(h % 600) - 300;
		y += (long)((h / 600) % 600) - 300;
		const bool ok = ch->WarpSet(x, y);
		sys_log(0, "ARZ_DG: sent to the lobby pid=%u name=%s dungeon=%s from=%ld why=%s ok=%d",
				ch->GetPlayerID(), ch->GetName(), PLAYERBOT_ARZDG[dg].szKey, from, why, ok ? 1 : 0);
		return ok;
	}

	// ------------------------------------------------------------ the instance's monsters

	struct TPlayerBotArzDgFoe
	{
		DWORD dwVID;
		DWORD dwRace;
		long lX, lY;
	};
	struct TPlayerBotArzDgScan
	{
		DWORD dwAt;
		std::vector<TPlayerBotArzDgFoe> foes;
		TPlayerBotArzDgScan() : dwAt(0) {}
	};
	std::map<long, TPlayerBotArzDgScan> s_mapPlayerBotArzDgScan;

	struct FPlayerBotArzDgScan
	{
		std::vector<TPlayerBotArzDgFoe>* out;
		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER c = (LPCHARACTER)ent;
			if (c->IsPC() || c->IsDead() || (!c->IsMonster() && !c->IsStone()))
				return;
			TPlayerBotArzDgFoe f;
			f.dwVID = (DWORD)c->GetVID();
			f.dwRace = c->GetRaceNum();
			f.lX = c->GetX();
			f.lY = c->GetY();
			out->push_back(f);
		}
	};

	const TPlayerBotArzDgScan& ScanPlayerBotArzDg(long instance, DWORD dwNow)
	{
		TPlayerBotArzDgScan& s = s_mapPlayerBotArzDgScan[instance];
		if (s.dwAt != 0 && dwNow - s.dwAt < 1000)
			return s;
		s.dwAt = dwNow;
		s.foes.clear();
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(instance);
		if (pMap)
		{
			FPlayerBotArzDgScan f;
			f.out = &s.foes;
			pMap->for_each(f);
		}
		return s;
	}

	// What attacks the bot, then the stage's objective, then the nearest
	// monster; the target in hand is kept while it is the same kind of choice.
	LPCHARACTER PickPlayerBotArzDgFoe(LPCHARACTER ch, TPlayerBotAIState& state, const TPlayerBotArzDg& info,
			int stage, const TPlayerBotArzDgScan& scan, long anchorX, long anchorY, int maxFromAnchor)
	{
		LPCHARACTER threat = NULL, objective = NULL, any = NULL;
		int dThreat = INT_MAX, dObjective = INT_MAX, dAny = INT_MAX;
		for (size_t i = 0; i < scan.foes.size(); ++i)
		{
			const TPlayerBotArzDgFoe& f = scan.foes[i];
			LPCHARACTER c = CHARACTER_MANAGER::instance().Find(f.dwVID);
			if (!c || c->IsDead() || c->GetMapIndex() != ch->GetMapIndex())
				continue;
			const int d = DISTANCE_APPROX(ch->GetX() - c->GetX(), ch->GetY() - c->GetY());
			// What attacks the pack's members counts for each of them.
			LPCHARACTER v = c->GetVictim();
			const bool onPack = v && v->IsPC() && DISTANCE_APPROX(v->GetX() - anchorX, v->GetY() - anchorY) <= PLAYERBOT_ARZDG_PACK_LEASH;
			if ((v == ch || onPack) && d <= PLAYERBOT_ARZDG_THREAT_RANGE && d < dThreat)
			{
				threat = c;
				dThreat = d;
			}
			const int fromPack = DISTANCE_APPROX(anchorX - c->GetX(), anchorY - c->GetY());
			if (fromPack > maxFromAnchor)
				continue;
			if (IsPlayerBotArzDgTarget(info, stage, c->GetRaceNum()) && fromPack < dObjective)
			{
				objective = c;
				dObjective = fromPack;
			}
			if (fromPack < dAny)
			{
				any = c;
				dAny = fromPack;
			}
		}
		LPCHARACTER best = threat ? threat : (objective ? objective : any);
		LPCHARACTER cur = state.dwTargetVID ? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		if (cur && !cur->IsDead() && cur->GetMapIndex() == ch->GetMapIndex() && !cur->IsPC() && cur != best && best)
		{
			const bool curThreat = cur->GetVictim() == ch &&
					DISTANCE_APPROX(ch->GetX() - cur->GetX(), ch->GetY() - cur->GetY()) <= PLAYERBOT_ARZDG_THREAT_RANGE;
			const bool curObjective = IsPlayerBotArzDgTarget(info, stage, cur->GetRaceNum());
			if (!curThreat && DISTANCE_APPROX(anchorX - cur->GetX(), anchorY - cur->GetY()) > maxFromAnchor)
				return best;
			if ((best == threat && curThreat) || (best == objective && !threat && curObjective) ||
					(best == any && !threat && !objective))
				return cur;
		}
		return best;
	}

	// MT2009_PLUS_AREZZO_DG_BOSS_BREAK_V1: the bosses (the Red Scorpions, the
	// King, the Faethorns, the Owl, the Queen) strike one bot at a time, and the
	// Queen took a bot of 100 from 11 600 of 20 300 health to 2 800 in two
	// seconds: the tower's break-off at a fifth of its health came after the blow
	// that killed it (1 October, 15:15-15:50: 188 deaths in the Jungle, 92 of
	// them at the Queen; 134 in the Ruins, 63 at the King, 49 at the Reds). The
	// boss's own victim breaks off at this much - it steps away hidden, heals
	// and comes back - and the boss turns to the next one, as a party of players
	// passes the boss between them.
	const int PLAYERBOT_ARZDG_BOSS_BREAK_HP = 45;

	bool BreakOffPlayerBotArzDgBoss(LPCHARACTER ch, TPlayerBotAIState& state, long map, DWORD dwNow)
	{
		if (state.bRecoveringAfterDeath || ch->GetMaxHP() <= 0 ||
				(long long)ch->GetHP() * 100 > (long long)ch->GetMaxHP() * PLAYERBOT_ARZDG_BOSS_BREAK_HP)
			return false;
		const TPlayerBotArzDgScan& scan = ScanPlayerBotArzDg(map, dwNow);
		LPCHARACTER boss = NULL;
		for (size_t i = 0; i < scan.foes.size() && !boss; ++i)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().Find(scan.foes[i].dwVID);
			if (c && !c->IsDead() && !c->IsStone() && c->GetMobRank() >= MOB_RANK_BOSS && c->GetVictim() == ch &&
					c->GetMapIndex() == ch->GetMapIndex())
				boss = c;
		}
		if (!boss)
			return false;
		state.bRecoveringAfterDeath = true;
		state.dwLastDeathTime = dwNow;
		state.lDeathX = ch->GetX();
		state.lDeathY = ch->GetY();
		state.dwNextRecoveryProtectionTime = 0;
		state.dwNextRecoveryHealTime = dwNow;
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		ClearPlayerBotRoute(state, true);
		sys_log(0, "ARZ_DG: boss break-off pid=%u name=%s boss=%u hp=%d/%d map=%ld",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)boss->GetRaceNum(), ch->GetHP(), ch->GetMaxHP(), map);
		return true;
	}

	// MT2009_PLUS_AREZZO_DG_UNSTICK_V1: a boss that chased a bot onto ground the
	// bots' routes do not reach (the Jungle Queen at (368,306), 1 October
	// 19:52: four bots at 46 cells with nothing but "unreachable", her health
	// at 99 %) never dies. Once a monster has been out of every route this
	// long it is set down where the bot stands, as the tower does with a
	// wedged monster (UnstickPlayerBotTowerMonsters).
	const DWORD PLAYERBOT_ARZDG_UNSTICK_MS = 10000;
	std::map<DWORD, DWORD> s_mapPlayerBotArzDgUnreachable;

	std::map<DWORD, DWORD> s_mapPlayerBotArzDgSetDown;

	void UnstickPlayerBotArzDgFoe(LPCHARACTER ch, LPCHARACTER foe, long anchorX, long anchorY, DWORD dwNow)
	{
		if (!ch || !foe || foe->IsStone() || foe->IsDead())
			return;
		const DWORD vid = (DWORD)foe->GetVID();
		std::map<DWORD, DWORD>::iterator it = s_mapPlayerBotArzDgUnreachable.find(vid);
		if (it == s_mapPlayerBotArzDgUnreachable.end() || dwNow - it->second > 3 * PLAYERBOT_ARZDG_UNSTICK_MS)
		{
			s_mapPlayerBotArzDgUnreachable[vid] = dwNow;
			return;
		}
		if (dwNow - it->second < PLAYERBOT_ARZDG_UNSTICK_MS)
			return;
		s_mapPlayerBotArzDgUnreachable.erase(it);
		// Once a minute at most, to the pack's middle: a bot apart from the
		// others asking too threw it between them every few seconds (20:26).
		DWORD& last = s_mapPlayerBotArzDgSetDown[vid];
		if (last != 0 && dwNow - last < 60000)
			return;
		last = dwNow;
		const long fromX = foe->GetX(), fromY = foe->GetY();
		if (foe->IsStateMove())
			foe->Stop();
		const bool toPack = IsPlayerBotReachable(ch->GetMapIndex(), ch->GetX(), ch->GetY(), anchorX, anchorY);
		if (!foe->Show(foe->GetMapIndex(), toPack ? anchorX : ch->GetX(), toPack ? anchorY : ch->GetY()))
			return;
		sys_log(0, "ARZ_DG: unreachable monster set down by the pack map=%ld vnum=%u from=(%ld,%ld) to=(%ld,%ld) by=%s",
				ch->GetMapIndex(), (unsigned int)foe->GetRaceNum(), fromX, fromY, ch->GetX(), ch->GetY(), ch->GetName());
	}

	// ------------------------------------------------------------ the bot's tick

	// In the lobby: healed, packed, its points spent, standing by the guard.
	bool ManagePlayerBotArzDgLobby(LPCHARACTER ch, TPlayerBotAIState& state, int dg, TPlayerBotArzDgBot& bot,
			DWORD dwNow)
	{
		if (bot.dwLobbySince == 0)
			bot.dwLobbySince = dwNow;
		if (KeepPlayerBotTowerAlive(ch, state, dwNow, 95, 60))
			return true;
		ManagePlayerBotStats(ch, state, dwNow);
		ManagePlayerBotSkills(ch, state, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		long x = 0, y = 0;
		if (!GetPlayerBotArzDgEntry(dg, x, y))
			return true;
		const DWORD h = PlayerBotNavHash(ch->GetPlayerID() ^ 0x4c4f4242U);
		x += (long)(h % 700) - 350;
		y += (long)((h / 700) % 700) - 350;
		if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) > 400)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			MovePlayerBot(ch, x, y, dwNow, 4, true, false);
		}
		else
		{
			if (ch->IsStateMove())
				ch->Stop();
			SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		}
		return true;
	}

	bool ManagePlayerBotArzDgInside(LPCHARACTER ch, TPlayerBotAIState& state, int dg, TPlayerBotArzDgBot& bot,
			DWORD dwNow)
	{
		const TPlayerBotArzDg& info = PLAYERBOT_ARZDG[dg];
		const long map = ch->GetMapIndex();
		bot.dwLobbySince = 0;
		LPDUNGEON d = ch->GetDungeon();
		if (!d)
		{
			d = CDungeonManager::instance().FindByMapIndex(map);
			if (d)
				ch->SetDungeon(d);
		}
		if (!d)
		{
			if (dwNow >= bot.dwNextHome)
			{
				bot.dwNextHome = dwNow + 10000;
				SendPlayerBotArzDgHome(ch, dg, "no_instance");
			}
			return true;
		}
		const int stage = d->GetFlag("stage");
		long anchorX = ch->GetX(), anchorY = ch->GetY();
		{
			std::map<DWORD, int>::const_iterator br = s_mapPlayerBotArzDgBotRun.find(ch->GetPlayerID());
			std::map<int, TPlayerBotArzDgRun>::const_iterator r = br != s_mapPlayerBotArzDgBotRun.end()
					? s_mapPlayerBotArzDgRuns.find(br->second) : s_mapPlayerBotArzDgRuns.end();
			if (r != s_mapPlayerBotArzDgRuns.end() && r->second.lInstance == map && r->second.iPackN >= 2)
			{
				anchorX = r->second.lPackX;
				anchorY = r->second.lPackY;
			}
		}
		// The waves of the Ruins and the Jungle: sixteen groups of three or
		// four respawning every fifteen seconds round the guard, and a squad
		// standing there had eight of them on it at once (round 2: a bot of
		// 70 from 13 000 to 800 health in three seconds, 250 deaths in ten
		// minutes). It fights them at a spot within sight of only two spawn
		// lines (tools/arezzo_dungeon_map.py), as a party of players would.
		int maxFromAnchor = INT_MAX;
		if ((stage == 1 || stage == 10) && info.lCampCellX != 0)
		{
			LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(info.lMap);
			if (pMap)
			{
				anchorX = pMap->m_setting.iBaseX + info.lCampCellX * 100;
				anchorY = pMap->m_setting.iBaseY + info.lCampCellY * 100;
				// What comes to the camp is fought; what stands further is left.
				maxFromAnchor = 2500;
			}
		}
		// The fall and the break-off at the last fifth of health are the
		// tower's (KeepPlayerBotTowerAlive). A walk back to the pack while
		// recovering (round 3, 1 October 12:53) was followed by six crashes of
		// game2 in twenty minutes (signal 11 in idle(); none in the 45 minutes of
		// runs before it), so it is left out until that is understood.
		if (KeepPlayerBotTowerAlive(ch, state, dwNow, PLAYERBOT_ARZDG_POTION_HP, PLAYERBOT_ARZDG_POTION_SP))
			return true;
		if (BreakOffPlayerBotArzDgBoss(ch, state, map, dwNow))
			return true;
		if (ch->IsRiding() && !HasPlayerBotBattleHorse(ch))
		{
			SetPlayerBotRidingForTravel(ch, state, false, dwNow, "arezzo_dungeon");
			return true;
		}
		if (BuffPlayerBotTowerFellows(ch, state, dwNow))
			return true;
		// The seal: its holder breaks it at once (30766.use and the like).
		if (stage == 1 && d->GetFlag("seal_out") == 1 && dwNow >= bot.dwNextSeal)
		{
			const int cell = FindPlayerBotTowerItemCell(ch, info.dwSeal);
			if (cell >= 0)
			{
				bot.dwNextSeal = dwNow + 3000;
				if (ch->IsStateMove())
					ch->Stop();
				const bool ok = ch->UseItem(TItemPos(INVENTORY, (WORD)cell));
				sys_log(0, "ARZ_DG: seal used pid=%u name=%s dungeon=%s instance=%ld seals=%d ok=%d",
						ch->GetPlayerID(), ch->GetName(), info.szKey, map, d->GetFlag("seals"), ok ? 1 : 0);
				return true;
			}
		}
		const TPlayerBotArzDgScan& scan = ScanPlayerBotArzDg(map, dwNow);
		LPCHARACTER foe = PickPlayerBotArzDgFoe(ch, state, info, stage, scan, anchorX, anchorY, maxFromAnchor);
		const int fromPack = DISTANCE_APPROX(ch->GetX() - anchorX, ch->GetY() - anchorY);
		// Strayed from the pack with nothing on it: back to the others first.
		if (fromPack > PLAYERBOT_ARZDG_PACK_LEASH && (!foe || foe->GetVictim() != ch) &&
				(!foe || DISTANCE_APPROX(foe->GetX() - anchorX, foe->GetY() - anchorY) > PLAYERBOT_ARZDG_PACK_LEASH))
		{
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			MovePlayerBot(ch, anchorX, anchorY, dwNow, 4, true, false);
			return true;
		}
		if (foe)
		{
			state.bLastNavOutcome = PLAYERBOT_NAV_OUT_NONE;
			const bool fought = FightPlayerBotTowerObjective(ch, state, foe, dwNow);
			if (state.bLastNavOutcome == PLAYERBOT_NAV_OUT_UNREACHABLE)
			{
				// A big monster stands on cells the routes do not end on: the
				// route asked again for any cell near it before it is moved.
				state.bLastNavOutcome = PLAYERBOT_NAV_OUT_NONE;
				ClearPlayerBotRoute(state, false);
				state.dwNextNavPlanTime = 0;
				MovePlayerBot(ch, foe->GetX(), foe->GetY(), dwNow, 24, true, false);
				if (state.bLastNavOutcome == PLAYERBOT_NAV_OUT_UNREACHABLE)
					UnstickPlayerBotArzDgFoe(ch, foe, anchorX, anchorY, dwNow);
			}
			return fought;
		}
		// Nothing standing (between two stages, after the boss): back to the
		// middle of the arena, where the next stage starts.
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		long x = 0, y = 0;
		if (GetPlayerBotArzDgEntry(dg, x, y))
		{
			const DWORD h = PlayerBotNavHash(ch->GetPlayerID() ^ 0x43454e54U);
			x += (long)(h % 800) - 400;
			y += (long)((h / 800) % 800) - 400;
			if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) > 500)
			{
				SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
				MovePlayerBot(ch, x, y, dwNow, 4, true, false);
				return true;
			}
		}
		if (ch->IsStateMove())
			ch->Stop();
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		return true;
	}

	// From the top of the bot's tick, ahead of every errand: a cohort bot on
	// its dungeon's map (the lobby or an instance of it) belongs to this pass
	// alone. False for everybody else.
	bool ManagePlayerBotArezzoDungeon(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (s_mapPlayerBotArzDgCohort.empty() || !s_bPlayerBotArzDgHosting || !ch)
			return false;
		std::map<DWORD, int>::const_iterator it = s_mapPlayerBotArzDgCohort.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotArzDgCohort.end())
			return false;
		const int dg = it->second;
		TPlayerBotArzDgBot& bot = s_mapPlayerBotArzDgBots[ch->GetPlayerID()];
		state.dwLastMeaningfulActivityTime = dwNow;
		if (ch->IsDead())
			return true;
		LockPlayerBotArzDgExp(ch);
		// MT2009_PLUS_BOT_SASH_FLOW_V1: never in a town, so its sashes are
		// combined and sold here (playerbot_sash.h) - a full bag of them took
		// the room the potions are bought into.
		ManagePlayerBotSashFlow(ch, dwNow, (BYTE)PLAYERBOT_SASH_FLOW_IN_PLACE);
		RestockPlayerBotArzDg(ch, bot, dwNow);
		const long map = ch->GetMapIndex();
		if (GetPlayerBotArzDgIndex(map) != dg)
		{
			// Somewhere else on this core (another dungeon's lobby, game2's
			// other maps): home to its own lobby.
			if (dwNow >= bot.dwNextHome)
			{
				bot.dwNextHome = dwNow + 30000;
				SendPlayerBotArzDgHome(ch, dg, "elsewhere");
			}
			return true;
		}
		if (map < PLAYERBOT_INSTANCE_MAP_INDEX_MIN)
			return ManagePlayerBotArzDgLobby(ch, state, dg, bot, dwNow);
		return ManagePlayerBotArzDgInside(ch, state, dg, bot, dwNow);
	}

	// ------------------------------------------------------------ the runs

	void ClosePlayerBotArzDgRun(TPlayerBotArzDgRun& run, const char* result, DWORD dwNow)
	{
		const TPlayerBotArzDg& info = PLAYERBOT_ARZDG[run.iDg];
		TPlayerBotArzDgStats& st = s_aPlayerBotArzDgStats[run.iDg];
		const DWORD took = run.dwFinishedAt ? run.dwFinishedAt - run.dwStart : dwNow - run.dwStart;
		if (!strcmp(result, "finished"))
		{
			++st.dwFinished;
			st.ullFinishedMs += took;
			if (st.dwBestMs == 0 || took < st.dwBestMs)
				st.dwBestMs = took;
			if (took > st.dwWorstMs)
				st.dwWorstMs = took;
		}
		else if (!strcmp(result, "abandoned"))
			++st.dwAbandoned;
		else
			++st.dwLost;
		char stages[160] = "";
		size_t off = 0;
		for (int s = 1; s <= info.iStages && off < sizeof(stages) - 12; ++s)
			off += snprintf(stages + off, sizeof(stages) - off, "%s%u", s > 1 ? "," : "", run.adwStageMs[s] / 1000);
		sys_log(0, "ARZ_DG: run closed id=%d dungeon=%s instance=%ld result=%s took_s=%u stage=%d stage_s=%s deaths=%d wipes=%d members=%u",
				run.iId, info.szKey, run.lInstance, result, took / 1000, run.iStage, stages, run.iDeaths, run.iWipes,
				(unsigned int)run.members.size());
		FILE* fp = fopen(PLAYERBOT_ARZDG_RUNS_FILE, "a");
		if (fp)
		{
			fprintf(fp, "%ld\t%d\t%s\t%ld\t%u\t%s\t%u\t%d\t%s\t%d\t%d\n", (long)time(0), run.iId, info.szKey, run.lInstance,
					(unsigned int)run.bEmpire, result, took / 1000, run.iStage, stages, run.iDeaths, run.iWipes);
			fclose(fp);
		}
		for (size_t i = 0; i < run.members.size(); ++i)
		{
			const DWORD pid = run.members[i];
			s_mapPlayerBotArzDgBotRun.erase(pid);
			TPlayerBotArzDgBot& bot = s_mapPlayerBotArzDgBots[pid];
			bot.dwRestUntil = dwNow + PLAYERBOT_ARZDG_PAUSE_MS;
			bot.dwLobbySince = 0;
			++bot.dwRuns;
			if (!strcmp(result, "finished"))
				++bot.dwFinished;
		}
		s_mapPlayerBotArzDgScan.erase(run.lInstance);
		// The instance goes once it is empty (the quests' own keep-until is
		// for a player's "Wroc do lochu").
		LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(run.lInstance);
		if (d)
		{
			d->SetFlag("mt2009_keep_until", 0);
			d->SetFlag("closed", 1);
		}
	}

	// Everybody still inside out to the lobby (the dead once they stand).
	void PullPlayerBotArzDgRun(TPlayerBotArzDgRun& run, const char* why)
	{
		for (size_t i = 0; i < run.members.size(); ++i)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(run.members[i]);
			if (ch && ch->GetMapIndex() == run.lInstance && !ch->IsDead())
				SendPlayerBotArzDgHome(ch, run.iDg, why);
		}
	}

	// One second of a run: its stage, its progress, its dead, its end.
	// False when the run is over and can be forgotten.
	bool UpdatePlayerBotArzDgRun(TPlayerBotArzDgRun& run, DWORD dwNow)
	{
		const TPlayerBotArzDg& info = PLAYERBOT_ARZDG[run.iDg];
		LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(run.lInstance);
		int inside = 0, alive = 0;
		std::vector<LPCHARACTER> here;
		for (size_t i = 0; i < run.members.size(); ++i)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(run.members[i]);
			if (!ch || ch->GetMapIndex() != run.lInstance)
				continue;
			++inside;
			here.push_back(ch);
			const bool wasDead = run.dead.count(run.members[i]) != 0;
			if (ch->IsDead() && !wasDead)
			{
				run.dead.insert(run.members[i]);
				++run.iDeaths;
				++s_aPlayerBotArzDgStats[run.iDg].dwDeaths;
				++s_mapPlayerBotArzDgBots[run.members[i]].dwDeaths;
				LPCHARACTER killer = ch->GetVictim();
				sys_log(0, "ARZ_DG: died pid=%u name=%s dungeon=%s run=%d stage=%d level=%u job=%u pos=(%ld,%ld) near=%u",
						ch->GetPlayerID(), ch->GetName(), info.szKey, run.iId, run.iStage, (unsigned int)ch->GetLevel(),
						(unsigned int)(ch->GetJob() % 4), ch->GetX(), ch->GetY(),
						killer && !killer->IsPC() ? (unsigned int)killer->GetRaceNum() : 0U);
			}
			else if (!ch->IsDead() && wasDead)
				run.dead.erase(run.members[i]);
			if (!ch->IsDead())
				++alive;
		}
		{
			long long sx = 0, sy = 0;
			int n = 0;
			for (size_t i = 0; i < here.size(); ++i)
				if (!here[i]->IsDead())
				{
					sx += here[i]->GetX();
					sy += here[i]->GetY();
					++n;
				}
			run.iPackN = n;
			if (n > 0)
			{
				run.lPackX = (long)(sx / n);
				run.lPackY = (long)(sy / n);
			}
		}
		if (!d || inside == 0)
		{
			ClosePlayerBotArzDgRun(run, run.dwFinishedAt ? "finished" : (d ? "left" : "lost"), dwNow);
			return false;
		}
		if (alive == 0 && !run.bAllDead)
		{
			++run.iWipes;
			++s_aPlayerBotArzDgStats[run.iDg].dwWipes;
			sys_log(0, "ARZ_DG: wipe dungeon=%s run=%d stage=%d inside=%d after_s=%u", info.szKey, run.iId, run.iStage,
					inside, (dwNow - run.dwStart) / 1000);
		}
		run.bAllDead = alive == 0;
		// The stage flag: 1..N while a stage runs, 10+N in the five seconds
		// after stage N (next_stage), N+1 once the last boss is down.
		const int stage = d->GetFlag("stage");
		if (stage < 10 && stage != run.iStage)
		{
			if (run.iStage >= 1 && run.iStage <= 7)
				run.adwStageMs[run.iStage] = dwNow - run.dwStageSince;
			sys_log(0, "ARZ_DG: stage dungeon=%s run=%d instance=%ld stage=%d/%d after_s=%u prev_stage_s=%u inside=%d alive=%d",
					info.szKey, run.iId, run.lInstance, stage, info.iStages, (dwNow - run.dwStart) / 1000,
					run.iStage >= 1 && run.iStage <= 7 ? run.adwStageMs[run.iStage] / 1000 : 0, inside, alive);
			run.iStage = stage;
			run.dwStageSince = dwNow;
			run.dwLastProgress = dwNow;
			run.bStallLogged = false;
		}
		// The end: the last boss moved the stage past the last one.
		if (!run.dwFinishedAt && stage == info.iStages + 1)
		{
			run.dwFinishedAt = dwNow;
			sys_log(0, "ARZ_DG: finished dungeon=%s run=%d instance=%ld took_s=%u deaths=%d wipes=%d inside=%d",
					info.szKey, run.iId, run.lInstance, (dwNow - run.dwStart) / 1000, run.iDeaths, run.iWipes, inside);
		}
		// Progress: a flag of the run moved, or the monsters' count did.
		const long long sig = (long long)d->GetFlag("kills") * 1000003LL + (long long)d->GetFlag("left") * 1009LL +
				(long long)d->GetFlag("seals") * 101LL + (long long)d->GetFlag("seal_out") * 7LL + stage;
		const int monsters = (int)ScanPlayerBotArzDg(run.lInstance, dwNow).foes.size();
		if (sig != run.llSignature || monsters != run.iMonsters)
		{
			run.llSignature = sig;
			run.iMonsters = monsters;
			run.dwLastProgress = dwNow;
			run.bStallLogged = false;
		}
		if (d->GetFlag("closed") == 1 && run.dwClosedSeen == 0)
			run.dwClosedSeen = dwNow;
		if ((run.dwFinishedAt && dwNow - run.dwFinishedAt > PLAYERBOT_ARZDG_EXIT_GRACE_MS) ||
				(run.dwClosedSeen && dwNow - run.dwClosedSeen > 15000))
		{
			PlayerBotLogThrottled("arzdg_exit_late", dwNow,
					"ARZ_DG: still inside after the end dungeon=%s run=%d inside=%d - walked out here",
					info.szKey, run.iId, inside);
			PullPlayerBotArzDgRun(run, "after_the_end");
			return true;
		}
		// MT2009_PLUS_AREZZO_DG_NO_BOSS_REGEN_V1: in the bots' instances a boss
		// does not heal back what it lost (the owner, 1 October: the King and
		// the Queen regained 5 % every 15-20 s, more than five bots took off -
		// 91-99 % health after half an hour). Players' instances are untouched.
		{
			static std::map<DWORD, int> s_mapBossLowHP;
			const TPlayerBotArzDgScan& sc = ScanPlayerBotArzDg(run.lInstance, dwNow);
			for (size_t i = 0; i < sc.foes.size(); ++i)
			{
				LPCHARACTER c = CHARACTER_MANAGER::instance().Find(sc.foes[i].dwVID);
				if (!c || c->IsDead() || c->IsStone() || c->GetMobRank() < MOB_RANK_BOSS)
					continue;
				std::map<DWORD, int>::iterator low = s_mapBossLowHP.find(sc.foes[i].dwVID);
				if (low == s_mapBossLowHP.end() || c->GetHP() < low->second)
					s_mapBossLowHP[sc.foes[i].dwVID] = c->GetHP();
				else if (c->GetHP() > low->second)
					c->PointChange(POINT_HP, low->second - c->GetHP());
			}
			if (s_mapBossLowHP.size() > 512)
				s_mapBossLowHP.clear();
		}
		// MT2009_PLUS_AREZZO_DG_STALL_DETAIL_V1 (boss): the bosses' health once a minute.
		{
			static std::map<int, DWORD> s_mapNextBossLog;
			DWORD& next = s_mapNextBossLog[run.iId];
			if (!run.dwFinishedAt && dwNow >= next)
			{
				next = dwNow + 60000;
				const TPlayerBotArzDgScan& sc = ScanPlayerBotArzDg(run.lInstance, dwNow);
				for (size_t i = 0; i < sc.foes.size(); ++i)
				{
					LPCHARACTER c = CHARACTER_MANAGER::instance().Find(sc.foes[i].dwVID);
					if (c && !c->IsStone() && c->GetMobRank() >= MOB_RANK_BOSS && c->GetMaxHP() > 0)
						sys_log(0, "ARZ_DG: boss hp dungeon=%s run=%d stage=%d vnum=%u hp=%d%% cell=(%ld,%ld) victim=%s",
								info.szKey, run.iId, stage, (unsigned int)c->GetRaceNum(),
								(int)((long long)c->GetHP() * 100 / c->GetMaxHP()),
								GetPlayerBotArzDgCellX(run.iDg, c->GetX()), GetPlayerBotArzDgCellY(run.iDg, c->GetY()),
								c->GetVictim() ? c->GetVictim()->GetName() : "-");
				}
			}
		}
		if (!run.dwFinishedAt && dwNow - run.dwLastProgress >= PLAYERBOT_ARZDG_STALL_MS && !run.bStallLogged)
		{
			run.bStallLogged = true;
			char where[256] = "";
			size_t off = 0;
			for (size_t i = 0; i < here.size() && off < sizeof(where) - 32; ++i)
				off += snprintf(where + off, sizeof(where) - off, "%s(%ld,%ld)", i ? "," : "",
						GetPlayerBotArzDgCellX(run.iDg, here[i]->GetX()), GetPlayerBotArzDgCellY(run.iDg, here[i]->GetY()));
			sys_log(0, "ARZ_DG: stalled dungeon=%s run=%d stage=%d kills=%d left=%d seals=%d seal_out=%d monsters=%d inside=%d alive=%d for_s=%u bots=%s",
					info.szKey, run.iId, stage, d->GetFlag("kills"), d->GetFlag("left"), d->GetFlag("seals"),
					d->GetFlag("seal_out"), monsters, inside, alive, (dwNow - run.dwLastProgress) / 1000, where);
			// MT2009_PLUS_AREZZO_DG_STALL_DETAIL_V1: what stands and what each bot does.
			char foes[512] = "";
			off = 0;
			const TPlayerBotArzDgScan& sc = ScanPlayerBotArzDg(run.lInstance, dwNow);
			for (size_t i = 0; i < sc.foes.size() && i < 12 && off < sizeof(foes) - 48; ++i)
			{
				LPCHARACTER c = CHARACTER_MANAGER::instance().Find(sc.foes[i].dwVID);
				off += snprintf(foes + off, sizeof(foes) - off, "%s%u@(%ld,%ld)hp%d", i ? "," : "",
						(unsigned int)sc.foes[i].dwRace, GetPlayerBotArzDgCellX(run.iDg, sc.foes[i].lX),
						GetPlayerBotArzDgCellY(run.iDg, sc.foes[i].lY),
						c && c->GetMaxHP() > 0 ? (int)((long long)c->GetHP() * 100 / c->GetMaxHP()) : -1);
			}
			char acts[512] = "";
			off = 0;
			for (size_t i = 0; i < here.size() && off < sizeof(acts) - 48; ++i)
			{
				LPCHARACTER v = here[i]->GetVictim();
				off += snprintf(acts + off, sizeof(acts) - off, "%s%s:hp%d,vic=%u,move=%d", i ? " " : "", here[i]->GetName(),
						here[i]->GetMaxHP() > 0 ? (int)((long long)here[i]->GetHP() * 100 / here[i]->GetMaxHP()) : -1,
						v ? (unsigned int)v->GetRaceNum() : 0U, here[i]->IsStateMove() ? 1 : 0);
			}
			sys_log(0, "ARZ_DG: stall detail dungeon=%s run=%d foes=%s bots=%s", info.szKey, run.iId, foes, acts);
		}
		if (!run.dwFinishedAt && dwNow - run.dwLastProgress >= PLAYERBOT_ARZDG_ABANDON_MS)
		{
			sys_log(0, "ARZ_DG: abandoned dungeon=%s run=%d stage=%d monsters=%d after_s=%u",
					info.szKey, run.iId, stage, monsters, (dwNow - run.dwStart) / 1000);
			ClosePlayerBotArzDgRun(run, "abandoned", dwNow);
			PullPlayerBotArzDgRun(run, "abandoned");
			return false;
		}
		return true;
	}

	// A squad into a new instance: what the guard's "Wejdz do lochu" does for
	// a player, without its fee, its daily limit and its time limit.
	bool StartPlayerBotArzDgRun(int dg, const std::vector<LPCHARACTER>& squad, DWORD dwNow)
	{
		const TPlayerBotArzDg& info = PLAYERBOT_ARZDG[dg];
		long x = 0, y = 0, cellX = 0, cellY = 0;
		if (squad.empty() || !GetPlayerBotArzDgEntry(dg, x, y, &cellX, &cellY))
			return false;
		LPDUNGEON d = CDungeonManager::instance().Create(info.lMap);
		if (!d)
		{
			PlayerBotLogThrottled("arzdg_create_failed", dwNow, "ARZ_DG: instance refused dungeon=%s", info.szKey);
			return false;
		}
		d->SetFlag("mt2009_dgbot", 1);
		TPlayerBotArzDgRun run;
		run.iId = s_iPlayerBotArzDgNextRun++;
		run.iDg = dg;
		run.lInstance = d->GetMapIndex();
		run.bEmpire = squad[0]->GetEmpire();
		run.dwStart = dwNow;
		run.dwStageSince = dwNow;
		run.dwLastProgress = dwNow;
		char names[256] = "";
		size_t off = 0;
		for (size_t i = 0; i < squad.size(); ++i)
		{
			LPCHARACTER ch = squad[i];
			// The save point and every way out: the lobby (d.exit_all reads it).
			ch->SetWarpLocation(info.lMap, cellX, cellY);
			TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
			if (st != s_mapPlayerBotAIStates.end())
			{
				st->second.dwTargetVID = 0;
				ClearPlayerBotRoute(st->second, true);
			}
			ch->SetVictim(NULL);
			const long jx = x + (long)(i % 3) * 150 - 150;
			const long jy = y + (long)(i / 3) * 150 - 75;
			if (!ch->WarpSet(jx, jy, d->GetMapIndex()))
			{
				s_mapPlayerBotArzDgBots[ch->GetPlayerID()].dwRestUntil = dwNow + 5 * 60 * 1000;
				sys_log(0, "ARZ_DG: jump refused pid=%u name=%s dungeon=%s instance=%ld",
						ch->GetPlayerID(), ch->GetName(), info.szKey, run.lInstance);
				continue;
			}
			run.members.push_back(ch->GetPlayerID());
			s_mapPlayerBotArzDgBotRun[ch->GetPlayerID()] = run.iId;
			if (off < sizeof(names) - 24)
				off += snprintf(names + off, sizeof(names) - off, "%s%s", run.members.size() > 1 ? "," : "", ch->GetName());
		}
		if (run.members.empty())
		{
			CDungeonManager::instance().Destroy(d->GetId());
			return false;
		}
		// The quest's start (init(): the stage flags, the first wave in five
		// seconds, the minute tick), through the server timer it keeps for a
		// bot instance.
		quest::CQuestManager& q = quest::CQuestManager::instance();
		const unsigned int npc = q.LoadTimerScript(info.szInitTimer);
		LPEVENT ev = quest::quest_create_server_timer_event(info.szInitTimer, 1, npc, false, (unsigned int)run.lInstance);
		q.AddServerTimer(info.szInitTimer, (DWORD)run.lInstance, ev);
		++s_aPlayerBotArzDgStats[dg].dwStarted;
		sys_log(0, "ARZ_DG: entered dungeon=%s run=%d instance=%ld empire=%u members=%u names=%s",
				info.szKey, run.iId, run.lInstance, (unsigned int)run.bEmpire, (unsigned int)run.members.size(), names);
		s_mapPlayerBotArzDgRuns[run.iId] = run;
		return true;
	}

	// The lobby of each dungeon: its standing bots in fives of a kingdom.
	void FormPlayerBotArzDgSquads(DWORD dwNow)
	{
		for (int dg = 0; dg < PLAYERBOT_ARZDG_COUNT; ++dg)
		{
			if (s_abPlayerBotArzDgStopped[dg] || dwNow < s_adwPlayerBotArzDgNextStart[dg] ||
					!SECTREE_MANAGER::instance().GetMap(PLAYERBOT_ARZDG[dg].lMap))
				continue;
			std::map<BYTE, std::vector<LPCHARACTER> > ready;
			std::map<BYTE, DWORD> oldest;
			for (std::map<DWORD, int>::const_iterator it = s_mapPlayerBotArzDgCohort.begin();
					it != s_mapPlayerBotArzDgCohort.end(); ++it)
			{
				if (it->second != dg || s_mapPlayerBotArzDgBotRun.count(it->first))
					continue;
				LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
				if (!ch || ch->IsDead() || ch->GetMapIndex() != PLAYERBOT_ARZDG[dg].lMap || !ch->GetDesc() ||
						!ch->GetDesc()->IsPhase(PHASE_GAME))
					continue;
				TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(it->first);
				if (st == s_mapPlayerBotAIStates.end() || st->second.bRecoveringAfterDeath)
					continue;
				TPlayerBotArzDgBot& bot = s_mapPlayerBotArzDgBots[it->first];
				if (dwNow < bot.dwRestUntil || bot.dwLobbySince == 0 ||
						(ch->GetMaxHP() > 0 && ch->GetHP() * 100 < ch->GetMaxHP() * 70))
					continue;
				ready[ch->GetEmpire()].push_back(ch);
				DWORD& o = oldest[ch->GetEmpire()];
				if (o == 0 || bot.dwLobbySince < o)
					o = bot.dwLobbySince;
			}
			for (std::map<BYTE, std::vector<LPCHARACTER> >::iterator r = ready.begin(); r != ready.end(); ++r)
			{
				std::vector<LPCHARACTER>& v = r->second;
				size_t i = 0;
				while (v.size() - i >= PLAYERBOT_ARZDG_SQUAD_SIZE)
				{
					std::vector<LPCHARACTER> squad(v.begin() + i, v.begin() + i + PLAYERBOT_ARZDG_SQUAD_SIZE);
					if (!StartPlayerBotArzDgRun(dg, squad, dwNow))
						s_adwPlayerBotArzDgNextStart[dg] = dwNow + 60000;
					i += PLAYERBOT_ARZDG_SQUAD_SIZE;
				}
				if (i < v.size() && dwNow - oldest[r->first] >= PLAYERBOT_ARZDG_SQUAD_WAIT_MS)
				{
					std::vector<LPCHARACTER> squad(v.begin() + i, v.end());
					if (!StartPlayerBotArzDgRun(dg, squad, dwNow))
						s_adwPlayerBotArzDgNextStart[dg] = dwNow + 60000;
				}
			}
		}
	}

	// ------------------------------------------------------------ the cohort and the hook

	void LoadPlayerBotArzDgCohort(bool schedule)
	{
		s_bPlayerBotArzDgCohortLoaded = true;
		FILE* fp = fopen(PLAYERBOT_ARZDG_COHORT_FILE, "r");
		if (!fp)
		{
			s_mapPlayerBotArzDgCohort.clear();
			return;
		}
		std::map<DWORD, int> cohort;
		std::vector<DWORD> pids;
		int per[PLAYERBOT_ARZDG_COUNT] = { 0, 0, 0 };
		char line[128];
		while (fgets(line, sizeof(line), fp))
		{
			char key[32] = "";
			unsigned int pid = 0;
			if (line[0] == '#' || sscanf(line, "%31s %u", key, &pid) != 2 || pid == 0)
				continue;
			for (int i = 0; i < PLAYERBOT_ARZDG_COUNT; ++i)
				if (!strcmp(key, PLAYERBOT_ARZDG[i].szKey))
				{
					cohort[pid] = i;
					pids.push_back(pid);
					++per[i];
				}
		}
		fclose(fp);
		s_mapPlayerBotArzDgCohort.swap(cohort);
		size_t scheduled = 0;
		if (schedule && s_bPlayerBotArzDgHosting)
			scheduled = CPlayerBotManager::instance().ScheduleExtraBots(pids);
		sys_log(0, "ARZ_DG: cohort file pids=%u wukong=%d skorpion=%d dzungla=%d hosting=%d scheduled_now=%u",
				(unsigned int)pids.size(), per[0], per[1], per[2], s_bPlayerBotArzDgHosting ? 1 : 0, (unsigned int)scheduled);
	}

	int ParsePlayerBotArzDgKey(const char* key)
	{
		if (!strcmp(key, "all"))
			return PLAYERBOT_ARZDG_COUNT;
		for (int i = 0; i < PLAYERBOT_ARZDG_COUNT; ++i)
			if (!strcmp(key, PLAYERBOT_ARZDG[i].szKey))
				return i;
		return -1;
	}

	void LogPlayerBotArzDgStatus()
	{
		for (int i = 0; i < PLAYERBOT_ARZDG_COUNT; ++i)
		{
			const TPlayerBotArzDgStats& st = s_aPlayerBotArzDgStats[i];
			int running = 0;
			for (std::map<int, TPlayerBotArzDgRun>::const_iterator r = s_mapPlayerBotArzDgRuns.begin();
					r != s_mapPlayerBotArzDgRuns.end(); ++r)
				if (r->second.iDg == i)
					++running;
			sys_log(0, "ARZ_DG: status dungeon=%s stopped=%d running=%d started=%u finished=%u abandoned=%u lost=%u avg_s=%u best_s=%u worst_s=%u deaths=%u wipes=%u",
					PLAYERBOT_ARZDG[i].szKey, s_abPlayerBotArzDgStopped[i] ? 1 : 0, running, st.dwStarted, st.dwFinished,
					st.dwAbandoned, st.dwLost, st.dwFinished ? (unsigned int)(st.ullFinishedMs / st.dwFinished / 1000) : 0U,
					st.dwBestMs / 1000, st.dwWorstMs / 1000, st.dwDeaths, st.dwWipes);
		}
	}

	void RunPlayerBotArzDgTestFile()
	{
		FILE* fp = fopen(PLAYERBOT_ARZDG_TEST_FILE, "r");
		if (!fp)
			return;
		std::vector<std::string> lines;
		char line[128];
		while (fgets(line, sizeof(line), fp))
			lines.push_back(line);
		fclose(fp);
		char done[128];
		snprintf(done, sizeof(done), "%s.done", PLAYERBOT_ARZDG_TEST_FILE);
		rename(PLAYERBOT_ARZDG_TEST_FILE, done);
		for (size_t l = 0; l < lines.size(); ++l)
		{
			char cmd[32] = "", arg[32] = "";
			const int got = sscanf(lines[l].c_str(), "%31s %31s", cmd, arg);
			if (got < 1 || cmd[0] == '#')
				continue;
			const int dg = got >= 2 ? ParsePlayerBotArzDgKey(arg) : -1;
			if (!strcmp(cmd, "cohort"))
				LoadPlayerBotArzDgCohort(true);
			else if (!strcmp(cmd, "status"))
				LogPlayerBotArzDgStatus();
			else if ((!strcmp(cmd, "stop") || !strcmp(cmd, "start")) && dg >= 0)
			{
				for (int i = 0; i < PLAYERBOT_ARZDG_COUNT; ++i)
					if (dg == PLAYERBOT_ARZDG_COUNT || dg == i)
						s_abPlayerBotArzDgStopped[i] = !strcmp(cmd, "stop");
				sys_log(0, "ARZ_DG: test %s %s", cmd, arg);
			}
			else if (!strcmp(cmd, "abort") && dg >= 0)
			{
				const DWORD dwNow = get_dword_time();
				for (std::map<int, TPlayerBotArzDgRun>::iterator r = s_mapPlayerBotArzDgRuns.begin();
						r != s_mapPlayerBotArzDgRuns.end();)
				{
					if (dg != PLAYERBOT_ARZDG_COUNT && r->second.iDg != dg)
					{
						++r;
						continue;
					}
					ClosePlayerBotArzDgRun(r->second, "abandoned", dwNow);
					PullPlayerBotArzDgRun(r->second, "test_abort");
					s_mapPlayerBotArzDgRuns.erase(r++);
				}
				sys_log(0, "ARZ_DG: test abort %s", arg);
			}
			else
				sys_log(0, "ARZ_DG: test unknown line=%s", lines[l].c_str());
		}
	}

	// The cohort's state, every PLAYERBOT_ARZDG_TRACK_MS.
	void WritePlayerBotArzDgTrack(DWORD dwNow)
	{
		FILE* out = fopen(PLAYERBOT_ARZDG_TRACK_FILE, "a");
		if (!out)
			return;
		const long wall = (long)time(0);
		int online = 0, lobby = 0, inside = 0;
		for (std::map<DWORD, int>::const_iterator it = s_mapPlayerBotArzDgCohort.begin();
				it != s_mapPlayerBotArzDgCohort.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch)
				continue;
			++online;
			const long map = ch->GetMapIndex();
			if (map >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN)
				++inside;
			else
				++lobby;
			TPlayerBotArzDgBot& bot = s_mapPlayerBotArzDgBots[it->first];
			// Stuck: not moved, not fighting, alive, inside, for a minute.
			if (map >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN && !ch->IsDead() && !ch->GetVictim() &&
					DISTANCE_APPROX(ch->GetX() - bot.lAnchorX, ch->GetY() - bot.lAnchorY) <= PLAYERBOT_ARZDG_STUCK_DISTANCE)
			{
				if (!bot.bStuck && bot.dwAnchorSince && dwNow - bot.dwAnchorSince >= PLAYERBOT_ARZDG_STUCK_MS)
				{
					bot.bStuck = true;
					LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(map);
					sys_log(0, "ARZ_DG: stuck pid=%u name=%s dungeon=%s instance=%ld stage=%d pos=(%ld,%ld) cell=(%ld,%ld)",
							it->first, ch->GetName(), PLAYERBOT_ARZDG[it->second].szKey, map, d ? d->GetFlag("stage") : -1,
							ch->GetX(), ch->GetY(), GetPlayerBotArzDgCellX(it->second, ch->GetX()),
							GetPlayerBotArzDgCellY(it->second, ch->GetY()));
				}
			}
			else
			{
				bot.bStuck = false;
				bot.lAnchorX = ch->GetX();
				bot.lAnchorY = ch->GetY();
				bot.dwAnchorSince = dwNow;
			}
			std::map<DWORD, int>::const_iterator r = s_mapPlayerBotArzDgBotRun.find(it->first);
			LPDUNGEON d = map >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN ? CDungeonManager::instance().FindByMapIndex(map) : NULL;
			fprintf(out, "%ld\t%u\t%s\t%s\t%u\t%u\t%ld\t%ld\t%ld\t%d\t%d\t%d\t%u\t%u\t%u\t%d\t%u\n",
					wall, it->first, ch->GetName(), PLAYERBOT_ARZDG[it->second].szKey, (unsigned int)ch->GetLevel(),
					(unsigned int)(ch->GetJob() % 4), map, ch->GetX(), ch->GetY(),
					ch->GetMaxHP() > 0 ? (int)(ch->GetHP() * 100LL / ch->GetMaxHP()) : 0,
					r != s_mapPlayerBotArzDgBotRun.end() ? r->second : 0, d ? d->GetFlag("stage") : 0,
					bot.dwRuns, bot.dwFinished, bot.dwDeaths, bot.bStuck ? 1 : 0, (unsigned int)ch->CountSpecifyItem(27003));
		}
		fprintf(out, "#\t%ld\tcohort=%u\tonline=%d\tlobby=%d\tinside=%d\truns=%u", wall,
				(unsigned int)s_mapPlayerBotArzDgCohort.size(), online, lobby, inside,
				(unsigned int)s_mapPlayerBotArzDgRuns.size());
		for (int i = 0; i < PLAYERBOT_ARZDG_COUNT; ++i)
		{
			const TPlayerBotArzDgStats& st = s_aPlayerBotArzDgStats[i];
			fprintf(out, "\t%s=%u/%u/%u/%u avg_s=%u", PLAYERBOT_ARZDG[i].szKey, st.dwStarted, st.dwFinished,
					st.dwAbandoned, st.dwLost, st.dwFinished ? (unsigned int)(st.ullFinishedMs / st.dwFinished / 1000) : 0U);
		}
		fprintf(out, "\n");
		fclose(out);
	}

	void TickPlayerBotArzDg()
	{
		const DWORD dwNow = get_dword_time();
		static DWORD s_dwStarted = 0, s_dwNextFile = 0, s_dwNextTrack = 0, s_dwNextStatus = 0;
		if (s_dwStarted == 0)
			s_dwStarted = dwNow;
		if (!s_bPlayerBotArzDgCohortLoaded && dwNow - s_dwStarted >= PLAYERBOT_ARZDG_COHORT_DELAY_MS)
			LoadPlayerBotArzDgCohort(true);
		if (dwNow >= s_dwNextFile)
		{
			s_dwNextFile = dwNow + 5000;
			RunPlayerBotArzDgTestFile();
		}
		for (std::map<int, TPlayerBotArzDgRun>::iterator r = s_mapPlayerBotArzDgRuns.begin();
				r != s_mapPlayerBotArzDgRuns.end();)
		{
			if (UpdatePlayerBotArzDgRun(r->second, dwNow))
				++r;
			else
				s_mapPlayerBotArzDgRuns.erase(r++);
		}
		// A bot whose run is gone (closed above) but whose map says otherwise
		// is no member any more; one in the lobby with a run that lost it, too.
		for (std::map<DWORD, int>::iterator b = s_mapPlayerBotArzDgBotRun.begin(); b != s_mapPlayerBotArzDgBotRun.end();)
		{
			if (!s_mapPlayerBotArzDgRuns.count(b->second))
				s_mapPlayerBotArzDgBotRun.erase(b++);
			else
				++b;
		}
		if (s_bPlayerBotArzDgCohortLoaded)
			FormPlayerBotArzDgSquads(dwNow);
		if (dwNow >= s_dwNextTrack)
		{
			s_dwNextTrack = dwNow + PLAYERBOT_ARZDG_TRACK_MS;
			if (!s_mapPlayerBotArzDgCohort.empty())
				WritePlayerBotArzDgTrack(dwNow);
		}
		if (dwNow >= s_dwNextStatus)
		{
			s_dwNextStatus = dwNow + 10 * 60 * 1000;
			if (!s_mapPlayerBotArzDgCohort.empty())
				LogPlayerBotArzDgStatus();
		}
	}

	EVENTINFO(playerbot_arzdg_watch_info)
	{
		int dummy;
	};

	LPEVENT s_pkPlayerBotArzDgWatch = NULL;

	EVENTFUNC(playerbot_arzdg_watch)
	{
		TickPlayerBotArzDg();
		return PASSES_PER_SEC(1);
	}

	// From CPlayerBotManager::StartWorldClock, on every core: the one that
	// hosts the dungeons runs the cohort, every other one only reads whom it
	// must not log in.
	void StartPlayerBotArezzoDungeonWatch()
	{
		if (s_pkPlayerBotArzDgWatch)
			return;
		s_bPlayerBotArzDgHosting = false;
		for (int i = 0; i < PLAYERBOT_ARZDG_COUNT; ++i)
			s_bPlayerBotArzDgHosting = s_bPlayerBotArzDgHosting ||
					SECTREE_MANAGER::instance().GetMap(PLAYERBOT_ARZDG[i].lMap) != NULL;
		if (!s_bPlayerBotArzDgHosting)
		{
			LoadPlayerBotArzDgCohort(false);
			return;
		}
		playerbot_arzdg_watch_info* info = AllocEventInfo<playerbot_arzdg_watch_info>();
		s_pkPlayerBotArzDgWatch = event_create(playerbot_arzdg_watch, info, PASSES_PER_SEC(5));
		sys_log(0, "ARZ_DG: watch started (cohort file %s, test file %s, track %s, runs %s)",
				PLAYERBOT_ARZDG_COHORT_FILE, PLAYERBOT_ARZDG_TEST_FILE, PLAYERBOT_ARZDG_TRACK_FILE, PLAYERBOT_ARZDG_RUNS_FILE);
	}
}

#endif
