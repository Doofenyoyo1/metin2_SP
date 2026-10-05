#ifndef __INC_METIN2_PLAYERBOT_DUNGEON_RUNS_H__
#define __INC_METIN2_PLAYERBOT_DUNGEON_RUNS_H__

// MT2009_PLUS_BOT_DUNGEON_RUNS_V1 - the bots' own dungeon runs (the engine half).
//
// The owner, 4 October: "Do wersji na githuba niech wszystkie boty lataja na
// dungeony zgodnie ze swoimi lvl, niech zwoluja sobie party i chodza
// normalnie". It reverses the rule of 30 September ("no bot in the new
// dungeons", IsPlayerBotOffLimitsMap) for these runs and these runs alone: a
// bot on its own still never wanders onto the Arezzo maps (the Arezzo rules
// of 1 October stay), and every move below is let through the Arezzo and
// Ochao routes by IsPlayerBotDungeonRunMove.
//
// A run, end to end:
//
//   - the choice (PlanPlayerBotDungeonRun, every PLAYERBOT_DGRUN_PLAN_MS): one
//     pass over this core's bots files each free one under the dungeons whose
//     level band it is in - the dungeon's level to fifteen above it, the
//     dungeon finder's (playerbot_dungeon_lfg.h) - and whose rest it is out of:
//     the dungeon's own cooldown (dungeon_info.txt's quest flag and seconds,
//     the one a player's entry sets), the quests' five expeditions a day
//     (<quest>.day / <quest>.runs, a player's), and a rest of its own after
//     every run (PLAYERBOT_DGRUN_REST_*). Free is what the finder says
//     (GetPlayerBotLfgRefusal: no companion, mercenary, shouter, cohort, raid,
//     war, duel, stall, market, fishing, mining, person's party, retirement,
//     AFK, a person's LFG wait...), a weapon and an armour on, and a share of
//     the bots by personality (PLAYERBOT_DGRUN_SHARE_*): a merchant seldom
//     goes. A dungeon and a kingdom with PLAYERBOT_DGRUN_CALL_MIN such bots
//     are drawn, within the caps (a dungeon's own, one Demon Tower run at a
//     time and none while a guild raids it, PLAYERBOT_DGRUN_MAX_RUNS for the
//     core, more with more bots), and a kingdom's shout carries one call per
//     PLAYERBOT_DGRUN_SHOUT_GAP_MS.
//   - the call: the party is drawn (playerbot_dgrun::PickParty: 4-8 by the
//     dungeon's limits, no class more than its third, a Shaman when one is
//     free); its first is the leader, who shouts the call on the kingdom's
//     shout - "szukam pt na biblioteke 30-45 lvl, kto chetny?" - and the
//     others answer over the next half a minute, a couple of them aloud ("ja",
//     "biore", "wbijam, 52 sura wp"), each one looked at again as it answers.
//   - the gathering: each goes to its own spot round the entrance - the
//     dungeon panel's entrance for its kingdom (ResolvePlayerBotLfgPlace, so a
//     Classic core without the row, or with the Arezzo module closed, never
//     calls one), the Metin of Toughness's ground floor for the Demon Tower -
//     by the map change every move of the AI is, and waits there drinking and
//     fighting off what comes; the leader makes the party there (a party is
//     one kingdom's, as the engine wants). Everybody there, or
//     PLAYERBOT_DGRUN_GATHER_MS gone with PLAYERBOT_DGRUN_START_MIN there: in;
//     too few: the leader says so ("nikt nie przyszedl, rozwiazuje pt") and
//     the party goes.
//   - the way in: what the guard's dialog does for a player is done here, as
//     the Arezzo cohort does it - a new instance of the dungeon, the party
//     jumped in at the quest's entry point (WarpSet through WarpBot, which
//     keeps the party), the saved way out set to the gathering spot - and the
//     quest's own start asked for through its server timer "<x>_dgrun", which
//     runs init() for an instance carrying the dungeon flag "mt2009_dgrun"
//     (nothing a player can set). A player's price is paid: the cooldown flag,
//     the day's count and the entry fee (when the bot holds three of it -
//     the fee of a poorer one is let go, logged as fee=0). Unlike the cohort's,
//     such a run keeps the player's time limit, end countdown and way out.
//     The Demon Tower is entered the tower's way: the party breaks the
//     Metin of Toughness and the quest jumps everybody on the ground floor in.
//   - inside: the Arezzo dungeons (Wukong, Skorpion, Dzungla) are fought by
//     the cohort's plan (ManagePlayerBotArzDgInside: the stage's targets, the
//     wave camp, the seals, the pack's middle given by this run), the Demon
//     Tower by the tower's floors (ManagePlayerBotDemonTower, which takes any
//     bot in its instance), and the rest by a leader and a pack: the leader
//     goes for the stage's objective (DrivePlayerBotDgRunLeader: the
//     Biblioteka's Metins, Queens and Baroness; Razador's Ignitor, Metin and
//     Razador; Nemere's Metin, Szels, Pillar and Nemere; the Blue Dragon's four
//     stones before him), else the nearest monster; the others fight round it
//     (FightPlayerBotPartyDungeon - what attacks them or the leader, what the
//     leader fights, the stones, the nearest). The dungeons' items are the
//     party dungeon pass's (ManagePlayerBotPdgItems: the seals and keys used,
//     Razador's cog and Maat stones and Nemere's crystals handed in), and the
//     two dialogs a leader would answer - Razador's statue, Nemere's lion - are
//     the quests' "<x>_dgrun_next" server timers. Between the fights the bots
//     loot as they do anywhere (HandleLoot); a boss in a bots' instance does
//     not heal back what it lost (HoldPlayerBotArzDgBossRegen).
//   - the way out: the quest's own end (the last boss's countdown, or its
//     time limit) and d.exit_all; whoever is still inside after it, or after
//     PLAYERBOT_DGRUN_STALL_MS without progress, PLAYERBOT_DGRUN_MAX_WIPES
//     wipes or the time limit and a margin, is warped out here - no bot is
//     ever left in an instance. Out on an Arezzo map or in the Temple of Ochao
//     (the Jungle's way out is the Las) a bot is sent back to where it stood
//     before the call. The leader may say how it went ("gg, zaliczylismy
//     razadora"), the party goes, and every member rests.
//
// The switch: the DUNGEONS key of playerbot_weights.tsv (the Seban panel's
// "Dungeony botow"), on by default, and the FILE playerbot_dungeon_runs_off
// in the core's directory, which turns it off whatever the panel says. Off:
// no new call; the runs under way end as they would.
//
// The operator's hook: a file "playerbot_dungeon_runs_test" in the core's
// directory, read every few seconds and renamed to .done:
//     now <key|any>           a call now (biblioteka, wieza, wukong, razador,
//                             skorpion, nemere, smok, dzungla), off the clock;
//     abort <key|all>         those runs out now;
//     status                  one BOT_DGRUN status line per dungeon.
//
// The log: BOT_DGRUN lines in syslog (called, answered, gathered, entered,
// stage, finished, stalled, abandoned, wipe, out, closed, status; and from V2
// break-off, regrouped, fallback, fallback over, cure, leader holds) and one row
// a run in playerbot_dungeon_runs.tsv: unix time, run, dungeon, instance,
// kingdom, members, lowest and highest level, result (finished, timeout,
// abandoned, wipe, left, lost, disbanded, no_stone, aborted), seconds, the
// stage reached, each stage's seconds, deaths, wipes, the fees paid, the
// leader and the names.
//
// Not here: the Devil's Catacomb, which needs the tower's ninth floor and a
// Dried Head and has its own party raids of bots (playerbot_catacomb.h); the
// Monkey and Spider Dungeons, open maps the bots hunt anyway; a person
// answering the call (people form their own parties with the dungeon finder).
//
// MT2009_PLUS_BOT_DUNGEON_RUNS_V2 - the first night's lesson (4/5 October,
// the test server: Razador 114 runs and none won, Nemere 78 and one; 24 198
// deaths, 1.25 a bot a minute). Read from BOT_DGRUN and PLAYERBOT_AI lines:
//   - a death loop, not a fight: a bot stood up where it fell after ten
//     seconds, rested to three quarters invisible a thousand units off in a
//     direction its pid drew - into the next group as often as not - and went
//     back at the same monsters: the median time between two deaths of one
//     bot was 29 s, 94% of them came within a minute of the one before;
//   - the leader pulled the room: every monster of 351/352 sees twenty
//     metres (aggressive_sight 2000), a room holds 48-60 of them back after a
//     minute, and the leader went for the nearest wherever the others were - a
//     Shaman of 68 lost 3 600 of its 9 280 health in two seconds, potions
//     (1 200 a draught, one a second) and all;
//   - the break-off at a fifth of health came too late: 58% (Razador) and 53%
//     (Nemere) of the deaths came with no target, in the second between the
//     break-off and the invisibility that protects a recovering bot;
//   - too few and too weak: a third of the parties went in with four, the
//     bots' gear is twenty levels behind them (75-79: weapons of 48, armour of
//     44 on average), and the Shaman's Cure waited for 60% health.
// Hence (the band and the party size are the rules' - RUN_RULES):
//   - KeepPlayerBotDgRunAlive: drink at 85%, break off at 30% invisible at
//     once, and a fallen or broken-off bot walks back to its pack invisible
//     (to the leader, or to the run's camp - the last spot where the pack
//     stood with no monster within PLAYERBOT_DGRUN_QUIET_RANGE) and fights again
//     only healed to 85% and there, as the Demon Tower's bots do;
//   - the leader pulls nothing new while fewer than two of the others stand
//     by it or its own health is under PLAYERBOT_DGRUN_LEADER_HOLD_HP - it
//     fights what is already on the party and waits;
//   - the fallback (wipe protection): fewer than half of those inside fit
//     to fight, and everybody standing goes back to the camp and fights only
//     what follows, until three quarters are fit again or
//     PLAYERBOT_DGRUN_FALLBACK_MAX_MS is up;
//   - the Shaman's Cure first: the run's most hurt under 75% before anything.
//
// An implementation fragment in the sense playerbot_types.h describes:
// include it once, after playerbot_dungeon_lfg.h (whose places, words and
// refusal it borrows), and so after the party dungeon pass, the Arezzo
// cohort and the tower, whose fights and items it uses.

#include "playerbot_dungeon_runs_rules.h"

namespace
{
	// ------------------------------------------------------------ the dungeons

	enum EPlayerBotDgRunKind
	{
		DGRUN_KIND_BIBLIO = 0,	// stage flag; waves and seals, Metins, Queens, the Baroness
		DGRUN_KIND_AREZZO,	// Wukong, Skorpion, Dzungla: the cohort's plan
		DGRUN_KIND_RAZADOR,	// the statue's six tasks in their order, then Razador
		DGRUN_KIND_NEMERE,	// ten rooms, then Nemere
		DGRUN_KIND_SMOK,	// four stones, then the Blue Dragon
		DGRUN_KIND_TOWER	// the Demon Tower: the stone, then the tower's floors
	};

	struct TPlayerBotDgRunDef
	{
		const char* szKey;		// dungeon_info.txt's key
		BYTE bKind;
		long lMap;			// the dungeon's map; its instances are lMap * 10000 + n
		const char* szQuest;		// the quest whose day / runs flags count a player's expeditions
		const char* szInitTimer;	// the quest's server timer that runs init() for a bot run
		const char* szNextTimer;	// the quest's server timer for a leader's dialog, or NULL
		long lEntryCellX, lEntryCellY;	// cfg().entry: cells off the map's base
		long long llFee;		// cfg().fee
		int iDaily;			// cfg().daily
		int iLimitMin;			// cfg().limit_min (the tower: the run's own limit)
		DWORD dwEndWaitMs;		// the quest's countdown after the last boss, and a margin
		int iCap;			// runs of it at once on this core
	};

	// MT2009_PLUS_AREZZO_BALANCE_V1: the fees are half of what they were (the owner, 5 October) -
	// the quests' cfg().fee: 1 / 2.5 / 3.5 / 5 / 7.5 / 7.5 / 7.5 million.
	const TPlayerBotDgRunDef PLAYERBOT_DGRUN_DEFS[] = {
		{ "biblioteka", DGRUN_KIND_BIBLIO, 363, "biblioteka_wiedzy", "biblioteka_dgrun", NULL, 271, 252,
			1000000LL, 5, 30, 60000, 2 },
		{ "wieza", DGRUN_KIND_TOWER, 66, NULL, NULL, NULL, 0, 0, 0LL, 0, 120, 0, 1 },
		{ "wukong", DGRUN_KIND_AREZZO, 364, "wzgorze_wukonga", "wukong_dgrun", NULL, 264, 273,
			2500000LL, 5, 30, 60000, 2 },
		{ "razador", DGRUN_KIND_RAZADOR, 351, "razador_dungeon", "razador_dgrun", "razador_dgrun_next", 342, 584,
			3500000LL, 5, 60, 120000, 2 },
		{ "skorpion", DGRUN_KIND_AREZZO, 365, "ruiny_skorpiona", "skorpion_dgrun", NULL, 268, 228,
			5000000LL, 5, 45, 60000, 2 },
		{ "nemere", DGRUN_KIND_NEMERE, 352, "nemere_dungeon", "nemere_dgrun", "nemere_dgrun_next", 171, 270,
			7500000LL, 5, 60, 120000, 2 },
		{ "smok", DGRUN_KIND_SMOK, 208, "blue_dragon_lair", "blue_dragon_dgrun", NULL, 237, 172,
			7500000LL, 5, 60, 240000, 1 },
		{ "dzungla", DGRUN_KIND_AREZZO, 366, "starozytna_dzungla", "dzungla_dgrun", NULL, 384, 374,
			7500000LL, 5, 60, 60000, 2 },
	};
	const int PLAYERBOT_DGRUN_DEF_COUNT = (int)(sizeof(PLAYERBOT_DGRUN_DEFS) / sizeof(PLAYERBOT_DGRUN_DEFS[0]));

	// ------------------------------------------------------------ the numbers

	// The choice: this often, with this chance, never in the first minutes of
	// a core (the world settles first).
	const DWORD PLAYERBOT_DGRUN_PLAN_MS = 45000;
	const int PLAYERBOT_DGRUN_PLAN_CHANCE = 70;
	const DWORD PLAYERBOT_DGRUN_FIRST_PLAN_MS = 5 * 60 * 1000;
	// Runs at once on this core: one a PLAYERBOT_DGRUN_BOTS_PER_RUN bots of
	// it, between the two figures.
	const int PLAYERBOT_DGRUN_MIN_RUNS = 4;
	const int PLAYERBOT_DGRUN_MAX_RUNS = 12;
	const int PLAYERBOT_DGRUN_BOTS_PER_RUN = 250;
	// One call a kingdom's shout this often at most.
	const DWORD PLAYERBOT_DGRUN_SHOUT_GAP_MS = 75000;
	// The answers come over this, after the call; the walk to the entrance
	// begins this long after the answer.
	const DWORD PLAYERBOT_DGRUN_ANSWER_MIN_MS = 4000;
	const DWORD PLAYERBOT_DGRUN_ANSWER_SPREAD_MS = 22000;
	const DWORD PLAYERBOT_DGRUN_TRAVEL_MIN_MS = 2000;
	const DWORD PLAYERBOT_DGRUN_TRAVEL_SPREAD_MS = 10000;
	const int PLAYERBOT_DGRUN_MAX_ANSWER_SHOUTS = 2;
	// The gathering: this long at most from the call.
	const DWORD PLAYERBOT_DGRUN_GATHER_MS = 3 * 60 * 1000;
	const int PLAYERBOT_DGRUN_ARRIVE_RANGE = 1500;
	const int PLAYERBOT_DGRUN_WALK_RANGE = 3000;
	const int PLAYERBOT_DGRUN_SPOT_RADIUS = 380;
	const int PLAYERBOT_DGRUN_MOVE_FAILS = 3;
	// The Demon Tower: the stone broken within this, or the run is over.
	const DWORD PLAYERBOT_DGRUN_STONE_MS = 3 * 60 * 1000;
	// Inside: no flag of the run moved this long - logged, and at the second
	// figure the run is given up; the quest's countdown after "closed".
	const DWORD PLAYERBOT_DGRUN_STALL_LOG_MS = 4 * 60 * 1000;
	const DWORD PLAYERBOT_DGRUN_STALL_MS = 12 * 60 * 1000;
	const DWORD PLAYERBOT_DGRUN_CLOSED_GRACE_MS = 20000;
	const DWORD PLAYERBOT_DGRUN_LIMIT_MARGIN_MS = 2 * 60 * 1000;
	const int PLAYERBOT_DGRUN_MAX_WIPES = 3;
	const DWORD PLAYERBOT_DGRUN_PULL_RETRY_MS = 5000;
	const DWORD PLAYERBOT_DGRUN_OUT_GRACE_MS = 3000;
	const DWORD PLAYERBOT_DGRUN_TOWER_OUT_GRACE_MS = 20000;
	// Razador's statue and Nemere's lion asked this often while no task runs.
	const DWORD PLAYERBOT_DGRUN_NEXT_STEP_MS = 6000;
	// A monster no route reaches is left alone this long.
	const DWORD PLAYERBOT_DGRUN_UNREACHABLE_MS = 60000;
	const int PLAYERBOT_DGRUN_THREAT_RANGE = 1500;
	// A bot's rest after a run, and after a call that came to nothing.
	const DWORD PLAYERBOT_DGRUN_REST_MIN_MS = 40 * 60 * 1000;
	const DWORD PLAYERBOT_DGRUN_REST_SPREAD_MS = 40 * 60 * 1000;
	const DWORD PLAYERBOT_DGRUN_REST_DISBAND_MS = 10 * 60 * 1000;
	const DWORD PLAYERBOT_DGRUN_REST_REFUSED_MS = 5 * 60 * 1000;
	// The entry fee is paid by a bot holding this many of it.
	const long long PLAYERBOT_DGRUN_FEE_RESERVE = 3;
	// The lowest dungeon's level: no bot under it is looked at.
	const int PLAYERBOT_DGRUN_MIN_LEVEL = 30;
	const DWORD PLAYERBOT_DGRUN_SWITCH_MS = 30000;
	const DWORD PLAYERBOT_DGRUN_STATUS_MS = 10 * 60 * 1000;
	const char* const PLAYERBOT_DGRUN_OFF_FILE = "playerbot_dungeon_runs_off";
	const char* const PLAYERBOT_DGRUN_TEST_FILE = "playerbot_dungeon_runs_test";
	const char* const PLAYERBOT_DGRUN_RUNS_FILE = "playerbot_dungeon_runs.tsv";
	// MT2009_PLUS_BOT_DUNGEON_RUNS_V2: staying alive inside (the header).
	// Drinking: a red at this much health (the tower's), a blue at the second.
	const int PLAYERBOT_DGRUN_POTION_HP = 85;
	const int PLAYERBOT_DGRUN_POTION_SP = 50;
	// The break-off: at this much health a bot leaves the fight invisible.
	const int PLAYERBOT_DGRUN_BREAK_OFF_HP = 30;
	// A recovering bot fights again healed to this, with its pack...
	const int PLAYERBOT_DGRUN_REJOIN_HP = 85;
	const int PLAYERBOT_DGRUN_REGROUP_RADIUS = 600;
	// ...or this long after its fall, wherever it got to.
	const DWORD PLAYERBOT_DGRUN_REGROUP_MAX_MS = 45000;
	// The leader pulls nothing new under this health, or with fewer than two
	// of the others (all, when fewer stand) within the second figure of it.
	const int PLAYERBOT_DGRUN_LEADER_HOLD_HP = 55;
	const int PLAYERBOT_DGRUN_PACK_NEAR = 1000;
	// What is on the party within this of the leader is fought while it holds.
	const int PLAYERBOT_DGRUN_HOLD_THREAT_RANGE = 900;
	// Fit: standing, not recovering, at least this much health.
	const int PLAYERBOT_DGRUN_FIT_HP = 50;
	// The camp: the leader's spot when no monster stood within this of it.
	const int PLAYERBOT_DGRUN_QUIET_RANGE = 2200;
	// The fallback: at most this long, and then not again for the second.
	const DWORD PLAYERBOT_DGRUN_FALLBACK_MAX_MS = 90000;
	const DWORD PLAYERBOT_DGRUN_FALLBACK_REST_MS = 30000;
	// The Shaman's Cure (109) for a member under this much health.
	const DWORD PLAYERBOT_DGRUN_CURE_SKILL = 109;
	const int PLAYERBOT_DGRUN_CURE_HP = 75;

	// ------------------------------------------------------------ the state

	enum EPlayerBotDgRunPhase
	{
		DGRUN_PHASE_GATHER = 1,	// the call answered, the party on its way to the entrance
		DGRUN_PHASE_STONE,	// the Demon Tower: breaking the Metin of Toughness
		DGRUN_PHASE_INSIDE	// in the instance
	};

	// One bot of a run.
	struct TPlayerBotDgRunBot
	{
		int iRun;
		DWORD dwAnswerAt;
		bool bAnswered;
		DWORD dwTravelAt;
		DWORD dwNextMove;
		int iMoveFails;
		bool bArrived;
		long lSpotX, lSpotY;
		// Where it stood at the call: the way home from an Arezzo map.
		long lHomeMap, lHomeX, lHomeY;
		// The party dungeon pass's record (the items, the catch-up) and the
		// Arezzo cohort's (the restock in place, the seal), for this bot.
		TPlayerBotPdgBot pdg;
		TPlayerBotArzDgBot arz;
		// The leader's monsters no route reached, by vid, and until when.
		std::map<DWORD, DWORD> unreachable;
		// Where it stood on its last tick inside: a quest's jump to another
		// room (d.jump_all is a Show) leaves the route of the room before.
		long lLastX, lLastY;
		// V2: the leader waiting for its pack since.
		DWORD dwHoldSince;
		TPlayerBotDgRunBot() : iRun(0), dwAnswerAt(0), bAnswered(false), dwTravelAt(0), dwNextMove(0), iMoveFails(0),
				bArrived(false), lSpotX(0), lSpotY(0), lHomeMap(0), lHomeX(0), lHomeY(0), lLastX(0), lLastY(0),
				dwHoldSince(0) {}
	};

	struct TPlayerBotDgRun
	{
		int iId;
		int iDef;
		BYTE bEmpire;
		BYTE bPhase;
		DWORD dwLeader;
		DWORD dwAnchor;			// the leader inside, or whoever stands for it
		std::vector<DWORD> members;	// the leader first
		TPlayerBotLfgPlace place;	// the gathering
		int iLvMin, iLvMax;
		DWORD dwCalledAt, dwPhaseSince, dwEnteredAt, dwLastProgress, dwFinishedAt, dwClosedSeen, dwOutSince;
		DWORD dwNextStep, dwNextPull;
		long lInstance;
		int iStage;
		DWORD dwStageSince;
		std::string stageSecs;
		long long llSignature;
		bool bStallLogged;
		int iDeaths, iWipes;
		std::set<DWORD> dead;
		bool bAllDead;
		long long llFees;
		int iAnswerShouts;
		long lPackX, lPackY;
		int iPackN;
		const char* szResult;		// set when the run is being ended
		bool bPartyMade;
		// MT2009_PLUS_BOT_DUNGEON_RUNS_V2: the camp (the last quiet spot of
		// the leader), the fallback to it, and the counts for the log.
		long lCampX, lCampY;
		bool bFallback;
		DWORD dwFallbackSince, dwFallbackRestUntil;
		int iFit, iFallbacks, iBreakOffs;
		TPlayerBotDgRun() : iId(0), iDef(0), bEmpire(0), bPhase(DGRUN_PHASE_GATHER), dwLeader(0), dwAnchor(0), iLvMin(0),
				iLvMax(0), dwCalledAt(0), dwPhaseSince(0), dwEnteredAt(0), dwLastProgress(0), dwFinishedAt(0),
				dwClosedSeen(0), dwOutSince(0), dwNextStep(0), dwNextPull(0), lInstance(0), iStage(0), dwStageSince(0),
				llSignature(0), bStallLogged(false), iDeaths(0), iWipes(0), bAllDead(false), llFees(0), iAnswerShouts(0),
				lPackX(0), lPackY(0), iPackN(0), szResult(NULL), bPartyMade(false), lCampX(0), lCampY(0), bFallback(false),
				dwFallbackSince(0), dwFallbackRestUntil(0), iFit(0), iFallbacks(0), iBreakOffs(0) {}
	};

	std::map<int, TPlayerBotDgRun> s_mapPlayerBotDgRuns;
	std::map<DWORD, TPlayerBotDgRunBot> s_mapPlayerBotDgRunBots;
	// pid -> no new run before this (get_dword_time).
	std::map<DWORD, DWORD> s_mapPlayerBotDgRunRest;
	int s_iPlayerBotDgRunNextId = 1;
	DWORD s_adwPlayerBotDgRunShoutAt[4] = { 0, 0, 0, 0 };
	// The bot whose refusal is being asked: its own run does not count.
	DWORD s_dwPlayerBotDgRunAsking = 0;
	bool s_bPlayerBotDgRunOffFile = false;

	struct TPlayerBotDgRunStats
	{
		DWORD dwCalled, dwStarted, dwFinished, dwAbandoned, dwTimeout, dwWipe, dwLost, dwDisbanded, dwDeaths;
		unsigned long long ullFinishedMs;
		TPlayerBotDgRunStats() : dwCalled(0), dwStarted(0), dwFinished(0), dwAbandoned(0), dwTimeout(0), dwWipe(0),
				dwLost(0), dwDisbanded(0), dwDeaths(0), ullFinishedMs(0) {}
	};
	TPlayerBotDgRunStats s_aPlayerBotDgRunStats[sizeof(PLAYERBOT_DGRUN_DEFS) / sizeof(PLAYERBOT_DGRUN_DEFS[0])];

	// ------------------------------------------------------------ the questions others ask

	bool IsPlayerBotOnDungeonRun(DWORD pid)
	{
		if (s_mapPlayerBotDgRunBots.empty() || pid == 0 || pid == s_dwPlayerBotDgRunAsking)
			return false;
		return s_mapPlayerBotDgRunBots.find(pid) != s_mapPlayerBotDgRunBots.end();
	}

	bool IsPlayerBotDungeonRunMove(LPCHARACTER ch, long targetMap, const char* reason)
	{
		if (!ch || s_mapPlayerBotDgRunBots.empty())
			return false;
		std::map<DWORD, TPlayerBotDgRunBot>::const_iterator it = s_mapPlayerBotDgRunBots.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotDgRunBots.end())
			return false;
		std::map<int, TPlayerBotDgRun>::const_iterator run = s_mapPlayerBotDgRuns.find(it->second.iRun);
		if (run == s_mapPlayerBotDgRuns.end())
			return false;
		// To the gathering, out of the instance, home ("dungeon_run",
		// "dungeon_run_out", "dungeon_run_home").
		if (reason && strncmp(reason, "dungeon_run", 11) == 0)
			return true;
		// Into the run's instance, and about inside it.
		return run->second.lInstance != 0 && targetMap == run->second.lInstance;
	}

	bool IsPlayerBotDgRunOn(DWORD dwNow)
	{
		static DWORD s_dwCheckedAt = 0;
		if (s_dwCheckedAt == 0 || dwNow - s_dwCheckedAt >= PLAYERBOT_DGRUN_SWITCH_MS)
		{
			s_dwCheckedAt = dwNow ? dwNow : 1;
			struct stat st;
			const bool off = stat(PLAYERBOT_DGRUN_OFF_FILE, &st) == 0;
			if (off != s_bPlayerBotDgRunOffFile)
				sys_log(0, "BOT_DGRUN: %s", off ? "off (playerbot_dungeon_runs_off)" : "file switch on again");
			s_bPlayerBotDgRunOffFile = off;
		}
		return !s_bPlayerBotDgRunOffFile && IsPlayerBotDungeonRunsPanelEnabled();
	}

	// ------------------------------------------------------------ small things

	playerbot_conv::TRng MakePlayerBotDgRunRng()
	{
		return playerbot_conv::TRng((playerbot_conv::u32)number(1, 0x7ffffffe) ^ get_dword_time());
	}

	playerbot_dgrun::TLine MakePlayerBotDgRunLine(LPCHARACTER ch, const TPlayerBotDgRun& run)
	{
		playerbot_dgrun::TLine l;
		l.key = PLAYERBOT_DGRUN_DEFS[run.iDef].szKey;
		if (ch)
		{
			l.botName = ch->GetName();
			l.level = ch->GetLevel();
			l.job = ch->GetJob();
			l.group = ch->GetSkillGroup();
		}
		l.lvMin = run.iLvMin;
		l.lvMax = run.iLvMax;
		return l;
	}

	// A line on the kingdom's shout, in the bot's name, as every bot shout is.
	void ShoutPlayerBotDgRun(LPCHARACTER ch, const std::string& text, const char* what)
	{
		if (!ch || text.empty())
			return;
		char msg[CHAT_MAX_LEN + 1];
		snprintf(msg, sizeof(msg), "%s : %s", ch->GetName(), text.c_str());
		SendPlayerBotShout(msg, ch->GetEmpire());
		BattlePassOnShout(ch);
		sys_log(0, "BOT_DGRUN: shout %s pid=%u name=%s empire=%u text=\"%s\"", what, ch->GetPlayerID(), ch->GetName(),
				(unsigned int)ch->GetEmpire(), text.c_str());
	}

	TPlayerBotAIState* GetPlayerBotDgRunState(DWORD pid)
	{
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(pid);
		return st != s_mapPlayerBotAIStates.end() ? &st->second : NULL;
	}

	// Why the bot may not go now, its own run left out of it (it is the run's
	// already while it answers).
	const char* GetPlayerBotDgRunRefusal(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD dwNow)
	{
		s_dwPlayerBotDgRunAsking = ch ? ch->GetPlayerID() : 0;
		const char* refusal = GetPlayerBotLfgRefusal(ch, state, dwNow);
		s_dwPlayerBotDgRunAsking = 0;
		if (refusal)
			return refusal;
		if (!ch->GetWear(WEAR_WEAPON) || !ch->GetWear(WEAR_BODY))
			return "gear";
		return NULL;
	}

	int GetPlayerBotDgRunShare(const TPlayerBotAIState& state)
	{
		switch (state.bPersonality)
		{
			case BOT_PERSONALITY_TEAM_COMPANION: return 95;
			case BOT_PERSONALITY_METIN_BREAKER: return 90;
			case BOT_PERSONALITY_STEADY_ADVENTURER: return 85;
			case BOT_PERSONALITY_WANDERER: return 80;
			case BOT_PERSONALITY_GEAR_SPECIALIST: return 85;
			case BOT_PERSONALITY_CAREFUL_COLLECTOR: return 60;
			case BOT_PERSONALITY_MERCHANT: return 30;
			default: return 70;
		}
	}

	int PlayerBotDgRunToday()
	{
		return (int)((get_global_time() + 7200) / 86400);
	}

	// The quest's own count of the day's expeditions (pay_entry's flags).
	int GetPlayerBotDgRunRunsToday(LPCHARACTER ch, const TPlayerBotDgRunDef& def)
	{
		if (!def.szQuest)
			return 0;
		const std::string q(def.szQuest);
		if (ch->GetQuestFlag(q + ".day") != PlayerBotDgRunToday())
			return 0;
		return ch->GetQuestFlag(q + ".runs");
	}

	// The quest's start (init()), or a leader's dialog, through its server timer.
	void CallPlayerBotDgRunTimer(const char* name, long instance)
	{
		if (!name || !*name || instance <= 0)
			return;
		quest::CQuestManager& q = quest::CQuestManager::instance();
		const unsigned int npc = q.LoadTimerScript(name);
		LPEVENT ev = quest::quest_create_server_timer_event(name, 1, npc, false, (unsigned int)instance);
		q.AddServerTimer(name, (DWORD)instance, ev);
	}

	// A spot round the gathering, by the bot's place in the party, on ground
	// the bot can stand on.
	void PlacePlayerBotDgRunSpot(const TPlayerBotDgRun& run, size_t index, DWORD pid, long& outX, long& outY)
	{
		if (PLAYERBOT_DGRUN_DEFS[run.iDef].bKind == DGRUN_KIND_TOWER)
		{
			GetPlayerBotTowerRally(pid, outX, outY);
			return;
		}
		static const int kSide[8][2] = {
			{ 200, 0 }, { 141, 141 }, { 0, 200 }, { -141, 141 }, { -200, 0 }, { -141, -141 }, { 0, -200 }, { 141, -141 } };
		const int* side = kSide[index % 8];
		outX = run.place.x + side[0] * PLAYERBOT_DGRUN_SPOT_RADIUS / 200;
		outY = run.place.y + side[1] * PLAYERBOT_DGRUN_SPOT_RADIUS / 200;
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(run.place.map);
		PIXEL_POSITION safe;
		if (navigation.Init(run.place.map) && navigation.FindNearestWalkableWorld(outX, outY, 12, safe, pid))
		{
			outX = safe.x;
			outY = safe.y;
		}
	}

	// A monster round the bot with the bot (or, given a run, a member of it) as its victim.
	struct FPlayerBotDgRunThreat
	{
		LPCHARACTER m_me;
		int m_iRun;
		LPCHARACTER m_found;
		int m_iBest;
		FPlayerBotDgRunThreat(LPCHARACTER me, int run) : m_me(me), m_iRun(run), m_found(NULL), m_iBest(PLAYERBOT_DGRUN_THREAT_RANGE + 1) {}
		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER c = (LPCHARACTER)ent;
			if (c->IsPC() || c->IsDead() || !c->IsMonster() || c->GetMapIndex() != m_me->GetMapIndex())
				return;
			LPCHARACTER v = c->GetVictim();
			if (!v)
				return;
			if (v != m_me)
			{
				if (m_iRun == 0 || !v->IsPC())
					return;
				std::map<DWORD, TPlayerBotDgRunBot>::const_iterator b = s_mapPlayerBotDgRunBots.find(v->GetPlayerID());
				if (b == s_mapPlayerBotDgRunBots.end() || b->second.iRun != m_iRun)
					return;
			}
			const int d = DISTANCE_APPROX(c->GetX() - m_me->GetX(), c->GetY() - m_me->GetY());
			if (d < m_iBest)
			{
				m_iBest = d;
				m_found = c;
			}
		}
	};

	LPCHARACTER FindPlayerBotDgRunThreat(LPCHARACTER ch, int run)
	{
		if (!ch || !ch->GetSectree())
			return NULL;
		FPlayerBotDgRunThreat f(ch, run);
		ch->GetSectree()->ForEachAround(f);
		return f.m_found;
	}

	// ------------------------------------------------------------ the stages

	// The stage a run is at, for the log: the quests' own flags.
	int GetPlayerBotDgRunStage(const TPlayerBotDgRunDef& def, LPDUNGEON d, int previous)
	{
		if (!d)
			return previous;
		switch (def.bKind)
		{
			case DGRUN_KIND_BIBLIO:
			case DGRUN_KIND_AREZZO:
			{
				// 1..N while a stage runs, 10+N between stages, N+1 at the end.
				const int s = d->GetFlag("stage");
				return s >= 10 ? previous : s;
			}
			case DGRUN_KIND_RAZADOR:
				if (d->GetFlag("boss") >= 2)
					return 8;
				if (d->GetFlag("boss") == 1)
					return 7;
				return d->GetFlag("done") + (d->GetFlag("active") == 1 ? 1 : 0);
			case DGRUN_KIND_NEMERE:
				return d->GetFlag("step");
			case DGRUN_KIND_SMOK:
				if (d->GetFlag("boss") >= 2)
					return 6;
				return d->GetFlag("init") == 1 ? 1 + std::max(0, 4 - d->GetFlag("stones")) : 0;
			default:
				return previous;
		}
	}

	// The last boss down.
	bool IsPlayerBotDgRunWon(const TPlayerBotDgRunDef& def, LPDUNGEON d)
	{
		if (!d)
			return false;
		switch (def.bKind)
		{
			case DGRUN_KIND_BIBLIO:
				return d->GetFlag("stage") == 6;
			case DGRUN_KIND_AREZZO:
			{
				const int dg = GetPlayerBotArzDgIndex(def.lMap);
				return dg >= 0 && d->GetFlag("stage") == PLAYERBOT_ARZDG[dg].iStages + 1;
			}
			case DGRUN_KIND_RAZADOR:
			case DGRUN_KIND_SMOK:
				return d->GetFlag("boss") >= 2;
			case DGRUN_KIND_NEMERE:
				return d->GetFlag("step") >= 11;
			default:
				return false;
		}
	}

	// What moves when a run makes progress: its flags, and the bosses' health
	// (which only goes down - HoldPlayerBotArzDgBossRegen).
	long long GetPlayerBotDgRunSignature(const TPlayerBotDgRunDef& def, LPDUNGEON d, long instance, DWORD dwNow)
	{
		if (!d)
			return 0;
		long long sig = 0;
		switch (def.bKind)
		{
			case DGRUN_KIND_BIBLIO:
			case DGRUN_KIND_AREZZO:
				sig = (long long)d->GetFlag("kills") * 1000003LL + (long long)d->GetFlag("left") * 1009LL +
						(long long)d->GetFlag("seals") * 101LL + (long long)d->GetFlag("seal_out") * 7LL + d->GetFlag("stage");
				break;
			case DGRUN_KIND_RAZADOR:
				sig = (long long)d->GetFlag("kills") * 1000003LL + (long long)d->GetFlag("done") * 1009LL +
						(long long)d->GetFlag("stones") * 101LL + (long long)d->GetFlag("active") * 7LL + d->GetFlag("boss");
				break;
			case DGRUN_KIND_NEMERE:
				sig = (long long)d->GetFlag("kills") * 1000003LL + (long long)d->GetFlag("step") * 1009LL +
						(long long)d->GetFlag("seals") * 101LL + d->GetFlag("ready");
				break;
			case DGRUN_KIND_SMOK:
				sig = (long long)d->GetFlag("stones") * 1009LL + d->GetFlag("boss");
				break;
			default:
				break;
		}
		const TPlayerBotArzDgScan& scan = ScanPlayerBotArzDg(instance, dwNow);
		long long bossHp = 0;
		for (size_t i = 0; i < scan.foes.size(); ++i)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().Find(scan.foes[i].dwVID);
			if (c && !c->IsDead() && (c->IsStone() || c->GetMobRank() >= MOB_RANK_BOSS) && c->GetMaxHP() > 0)
				bossHp += (long long)c->GetHP() * 1000 / c->GetMaxHP();
		}
		return sig * 7919LL + bossHp;
	}

	// The races the leader goes for at this stage (empty: the nearest monster).
	void GetPlayerBotDgRunObjectives(const TPlayerBotDgRunDef& def, LPDUNGEON d, const TPlayerBotArzDgScan& scan,
			std::vector<DWORD>& out)
	{
		out.clear();
		if (!d)
			return;
		switch (def.bKind)
		{
			case DGRUN_KIND_BIBLIO:
			{
				const int s = d->GetFlag("stage");
				if (s == 1)
					out.push_back(9703);
				else if (s == 2 || s == 4)
					out.push_back(8006);
				else if (s == 3)
					out.push_back(9705);
				else if (s == 5)
					out.push_back(9706);
				break;
			}
			case DGRUN_KIND_RAZADOR:
				if (d->GetFlag("boss") == 1)
					out.push_back(6091);
				else if (d->GetFlag("active") == 1 && d->GetFlag("step") == 4)
					out.push_back(6051);
				else if (d->GetFlag("active") == 1 && d->GetFlag("step") == 6)
					out.push_back(8057);
				break;
			case DGRUN_KIND_NEMERE:
				if (d->GetFlag("ready") != 1)
					break;
				switch (d->GetFlag("step"))
				{
					case 6: out.push_back(8058); break;
					case 7: out.push_back(6151); break;
					case 9: out.push_back(20399); break;
					case 10: out.push_back(6191); break;
					default: break;
				}
				break;
			case DGRUN_KIND_SMOK:
			{
				bool stones = false;
				for (size_t i = 0; i < scan.foes.size() && !stones; ++i)
					stones = scan.foes[i].dwRace >= 8031 && scan.foes[i].dwRace <= 8034;
				if (stones)
				{
					out.push_back(8031);
					out.push_back(8032);
					out.push_back(8033);
					out.push_back(8034);
				}
				else
					out.push_back(PLAYERBOT_PDG_BLUE_DRAGON);
				break;
			}
			default:
				break;
		}
	}

	// ------------------------------------------------------------ the bot's tick

	// On the way to the gathering, and waiting there.
	bool ManagePlayerBotDgRunGather(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotDgRun& run,
			TPlayerBotDgRunBot& rb, DWORD dwNow)
	{
		const TPlayerBotDgRunDef& def = PLAYERBOT_DGRUN_DEFS[run.iDef];
		const long gatherMap = run.place.map;
		const bool onMap = ch->GetMapIndex() == gatherMap;
		const int toSpot = onMap ? DISTANCE_APPROX(ch->GetX() - rb.lSpotX, ch->GetY() - rb.lSpotY) : INT_MAX;
		if (!onMap || toSpot > PLAYERBOT_DGRUN_WALK_RANGE)
		{
			// Finishing what it was doing for a few seconds first.
			if (dwNow < rb.dwTravelAt)
				return false;
			if (dwNow < rb.dwNextMove)
				return true;
			rb.dwNextMove = dwNow + 5000;
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ClearPlayerBotRoute(state, true);
			s_szPlayerBotTransitionRefusal = NULL;
			if (!TransitionPlayerBotMap(ch, state, gatherMap, rb.lSpotX, rb.lSpotY, dwNow, "dungeon_run") ||
					ch->GetMapIndex() != gatherMap)
			{
				++rb.iMoveFails;
				sys_log(0, "BOT_DGRUN: move refused pid=%u name=%s run=%d dungeon=%s from=%ld to=%ld refused=%s tries=%d",
						ch->GetPlayerID(), ch->GetName(), run.iId, def.szKey, ch->GetMapIndex(), gatherMap,
						s_szPlayerBotTransitionRefusal ? s_szPlayerBotTransitionRefusal : "?", rb.iMoveFails);
			}
			s_szPlayerBotTransitionRefusal = NULL;
			return true;
		}
		rb.bArrived = DISTANCE_APPROX(ch->GetX() - run.place.x, ch->GetY() - run.place.y) <= PLAYERBOT_DGRUN_ARRIVE_RANGE ||
				toSpot <= PLAYERBOT_DGRUN_ARRIVE_RANGE;
		if (KeepPlayerBotTowerAlive(ch, state, dwNow, PLAYERBOT_PDG_POTION_HP, PLAYERBOT_PDG_POTION_SP))
			return true;
		RestockPlayerBotArzDg(ch, rb.arz, dwNow);
		// The Demon Tower: the stone, all together.
		if (run.bPhase == DGRUN_PHASE_STONE)
		{
			if (BuffPlayerBotTowerFellows(ch, state, dwNow))
				return true;
			const TPlayerBotTowerScan* scan = ScanPlayerBotTowerMap(PLAYERBOT_MAP_DEMON_TOWER, dwNow);
			LPCHARACTER stone = PickPlayerBotTowerObjective(ch, scan, -1, true, 0);
			if (stone)
				return FightPlayerBotTowerObjective(ch, state, stone, dwNow);
		}
		// What attacks it, fought where it stands; nothing else.
		LPCHARACTER threat = FindPlayerBotDgRunThreat(ch, 0);
		if (threat)
			return FightPlayerBotTowerObjective(ch, state, threat, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		if (toSpot > 250)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			if (dwNow >= rb.dwNextMove)
			{
				rb.dwNextMove = dwNow + 2000;
				MovePlayerBot(ch, rb.lSpotX, rb.lSpotY, dwNow, 4, true, false);
			}
			return true;
		}
		if (ch->IsStateMove())
			ch->Stop();
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		return true;
	}

	bool IsPlayerBotDgRunUnreachable(TPlayerBotDgRunBot& rb, DWORD vid, DWORD dwNow)
	{
		std::map<DWORD, DWORD>::iterator it = rb.unreachable.find(vid);
		if (it == rb.unreachable.end())
			return false;
		if ((int)(it->second - dwNow) > 0)
			return true;
		rb.unreachable.erase(it);
		return false;
	}

	// ------------------------------------------------------------ staying alive (V2)

	// MT2009_PLUS_BOT_DUNGEON_RUNS_V2: the header says why.
	bool IsPlayerBotDgRunRecovering(DWORD pid)
	{
		const TPlayerBotAIState* st = GetPlayerBotDgRunState(pid);
		return st && st->bRecoveringAfterDeath;
	}

	bool IsPlayerBotDgRunMemberOf(LPCHARACTER v, int runId)
	{
		if (!v || !v->IsPC())
			return false;
		std::map<DWORD, TPlayerBotDgRunBot>::const_iterator b = s_mapPlayerBotDgRunBots.find(v->GetPlayerID());
		return b != s_mapPlayerBotDgRunBots.end() && b->second.iRun == runId;
	}

	// Fit to fight: standing, not recovering, PLAYERBOT_DGRUN_FIT_HP of its health.
	bool IsPlayerBotDgRunFit(LPCHARACTER c)
	{
		return c && !c->IsDead() && c->GetMaxHP() > 0 && !IsPlayerBotDgRunRecovering(c->GetPlayerID()) &&
				(long long)c->GetHP() * 100 >= (long long)c->GetMaxHP() * PLAYERBOT_DGRUN_FIT_HP;
	}

	// The others of the run on ch's map: alive (recovering or not), those not
	// recovering within `radius` of ch, and the nearest of those not recovering.
	LPCHARACTER CountPlayerBotDgRunPack(const TPlayerBotDgRun& run, LPCHARACTER ch, int radius, int& others, int& nearBy)
	{
		others = nearBy = 0;
		LPCHARACTER nearest = NULL;
		int dNearest = INT_MAX;
		for (size_t i = 0; i < run.members.size(); ++i)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(run.members[i]);
			if (!c || c == ch || c->IsDead() || c->GetMapIndex() != ch->GetMapIndex())
				continue;
			++others;
			if (IsPlayerBotDgRunRecovering(run.members[i]))
				continue;
			const int d = DISTANCE_APPROX(c->GetX() - ch->GetX(), c->GetY() - ch->GetY());
			if (d <= radius)
				++nearBy;
			if (d < dNearest)
			{
				dNearest = d;
				nearest = c;
			}
		}
		return nearest;
	}

	void GetPlayerBotDgRunCamp(const TPlayerBotDgRun& run, LPCHARACTER ch, long& x, long& y)
	{
		x = run.lCampX;
		y = run.lCampY;
		if (x == 0 && y == 0)
		{
			x = ch->GetX();
			y = ch->GetY();
		}
	}

	// A fallen bot stood up, or one that broke off: invisible (the recovery
	// keeps it so), drinking and resting, it walks to the leader - or to the
	// camp, while the pack falls back or nobody stands - and fights again
	// healed and there, or PLAYERBOT_DGRUN_REGROUP_MAX_MS after the fall
	// wherever it got to. The tower's regroup (RegroupPlayerBotTowerAfterDeath)
	// for a run's instance; the open world's step away from the spot - into
	// the next group of a dungeon room - never runs for it.
	bool RegroupPlayerBotDgRun(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotDgRun& run, DWORD dwNow)
	{
		SetPlayerBotAction(state, BOT_ACTION_RECOVER, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		state.lDeathX = 0;
		state.lDeathY = 0;
		UseHealthPotion(ch, state, dwNow, 95);
		UseManaPotion(ch, state, dwNow, 60);
		const bool rested = RestHealPlayerBot(ch, state, dwNow);
		long goalX = 0, goalY = 0;
		const char* to = "camp";
		LPCHARACTER anchor = run.dwAnchor ? CHARACTER_MANAGER::instance().FindByPID(run.dwAnchor) : NULL;
		if (!run.bFallback && anchor && anchor != ch && !anchor->IsDead() && anchor->GetMapIndex() == ch->GetMapIndex() &&
				!IsPlayerBotDgRunRecovering(run.dwAnchor))
		{
			goalX = anchor->GetX();
			goalY = anchor->GetY();
			to = "leader";
		}
		else
			GetPlayerBotDgRunCamp(run, ch, goalX, goalY);
		const int distance = DISTANCE_APPROX(ch->GetX() - goalX, ch->GetY() - goalY);
		const bool arrived = distance <= PLAYERBOT_DGRUN_REGROUP_RADIUS;
		const DWORD sinceFall = dwNow - state.dwLastDeathTime;
		const bool healed = ch->GetMaxHP() > 0 &&
				((long long)ch->GetHP() * 100 >= (long long)ch->GetMaxHP() * PLAYERBOT_DGRUN_REJOIN_HP ||
				(rested && sinceFall > PLAYERBOT_DGRUN_REGROUP_MAX_MS / 2));
		if (healed && (arrived || sinceFall > PLAYERBOT_DGRUN_REGROUP_MAX_MS))
		{
			sys_log(0, "BOT_DGRUN: regrouped pid=%u name=%s run=%d to=%s arrived=%d distance=%d hp=%d/%d after_s=%u",
					ch->GetPlayerID(), ch->GetName(), run.iId, to, arrived ? 1 : 0, distance, ch->GetHP(), ch->GetMaxHP(),
					sinceFall / 1000U);
			EndPlayerBotRecovery(ch, state);
			return false;
		}
		KeepPlayerBotRecoveryHidden(ch, state, dwNow);
		if (arrived)
		{
			if (ch->IsStateMove())
				ch->Stop();
			return true;
		}
		if (dwNow >= state.dwNextTowerMoveTime)
		{
			state.dwNextTowerMoveTime = dwNow + 1500;
			MovePlayerBot(ch, goalX, goalY, dwNow, 8, true, false);
		}
		return true;
	}

	// The potions, the break-off at PLAYERBOT_DGRUN_BREAK_OFF_HP - invisible on
	// the spot: the recovery that hides a bot came a tick later, and more than
	// half of the first night's deaths fell in that second - and the regroup.
	bool KeepPlayerBotDgRunAlive(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotDgRun& run, DWORD dwNow)
	{
		if (state.bRecoveringAfterDeath)
			return RegroupPlayerBotDgRun(ch, state, run, dwNow);
		UseHealthPotion(ch, state, dwNow, PLAYERBOT_DGRUN_POTION_HP);
		UseManaPotion(ch, state, dwNow, PLAYERBOT_DGRUN_POTION_SP);
		if (ch->GetMaxHP() <= 0 ||
				(long long)ch->GetHP() * 100 > (long long)ch->GetMaxHP() * PLAYERBOT_DGRUN_BREAK_OFF_HP)
			return false;
		state.bRecoveringAfterDeath = true;
		state.dwLastDeathTime = dwNow;
		state.lDeathX = 0;
		state.lDeathY = 0;
		state.dwNextRecoveryHealTime = dwNow;
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		ClearPlayerBotRoute(state, true);
		ch->ReviveInvisible(5);
		state.dwNextRecoveryProtectionTime = dwNow + PLAYERBOT_RECOVERY_PROTECTION_INTERVAL;
		++run.iBreakOffs;
		sys_log(0, "BOT_DGRUN: break-off pid=%u name=%s run=%d dungeon=%s stage=%d hp=%d/%d",
				ch->GetPlayerID(), ch->GetName(), run.iId, PLAYERBOT_DGRUN_DEFS[run.iDef].szKey, run.iStage, ch->GetHP(),
				ch->GetMaxHP());
		return true;
	}

	// A Shaman with Cure: the run's most hurt under PLAYERBOT_DGRUN_CURE_HP
	// within the skill's reach, itself included, before anything else - the
	// buff pass casts it too, but after the buffs and only under 60%.
	bool HealPlayerBotDgRunFellow(LPCHARACTER ch, TPlayerBotAIState& state, const TPlayerBotDgRun& run, DWORD dwNow)
	{
		if (ch->GetJob() != JOB_SHAMAN || ch->GetSkillGroup() == 0 || state.bRecoveringAfterDeath ||
				ch->GetSkillLevel(PLAYERBOT_DGRUN_CURE_SKILL) == 0 ||
				(ch->IsRiding() && !IsPlayerBotOnStandingMount(ch)) ||
				!CanPlayerBotAffordSkill(ch, state, PLAYERBOT_DGRUN_CURE_SKILL, dwNow))
			return false;
		CSkillProto* proto = CSkillManager::instance().Get(PLAYERBOT_DGRUN_CURE_SKILL);
		if (!proto)
			return false;
		const int reach = proto->dwTargetRange != 0 ? (int)proto->dwTargetRange : 1000;
		LPCHARACTER best = NULL;
		long long bestPct = PLAYERBOT_DGRUN_CURE_HP;
		for (size_t i = 0; i < run.members.size(); ++i)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(run.members[i]);
			if (!c || c->IsDead() || c->GetMapIndex() != ch->GetMapIndex() || c->GetMaxHP() <= 0)
				continue;
			if (c != ch && DISTANCE_APPROX(c->GetX() - ch->GetX(), c->GetY() - ch->GetY()) > reach)
				continue;
			const long long pct = (long long)c->GetHP() * 100 / c->GetMaxHP();
			if (pct < bestPct)
			{
				bestPct = pct;
				best = c;
			}
		}
		if (!best || !PlayerBotUseSkill(ch, state, PLAYERBOT_DGRUN_CURE_SKILL, best, dwNow))
			return false;
		SendPlayerBotSkillPacket(ch, PLAYERBOT_DGRUN_CURE_SKILL);
		state.dwLastBotSkillTime = dwNow;
		state.dwNextAttackTime = dwNow + PLAYERBOT_SKILL_ANIMATION_LOCK;
		PlayerBotLogThrottled("dgrun_cure", dwNow, "BOT_DGRUN: cure pid=%u name=%s on=%s hp=%d/%d run=%d",
				ch->GetPlayerID(), ch->GetName(), best->GetName(), best->GetHP(), best->GetMaxHP(), run.iId);
		return true;
	}

	// The fallback: to the camp, and there only what follows the pack.
	bool FallBackPlayerBotDgRun(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotDgRun& run, DWORD dwNow)
	{
		long x = 0, y = 0;
		GetPlayerBotDgRunCamp(run, ch, x, y);
		if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) > PLAYERBOT_DGRUN_REGROUP_RADIUS)
		{
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			if (dwNow >= state.dwNextTowerMoveTime)
			{
				state.dwNextTowerMoveTime = dwNow + 1500;
				MovePlayerBot(ch, x, y, dwNow, 8, true, false);
			}
			return true;
		}
		LPCHARACTER threat = FindPlayerBotDgRunThreat(ch, run.iId);
		if (threat)
			return FightPlayerBotTowerObjective(ch, state, threat, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		if (ch->IsStateMove())
			ch->Stop();
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		return true;
	}

	// The leader: what attacks the party near it, else the stage's objective,
	// else the nearest monster of the instance - wherever it stands; the party
	// follows (FightPlayerBotPartyDungeon round the leader).
	bool DrivePlayerBotDgRunLeader(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotDgRun& run, TPlayerBotDgRunBot& rb,
			LPDUNGEON d, DWORD dwNow)
	{
		const TPlayerBotDgRunDef& def = PLAYERBOT_DGRUN_DEFS[run.iDef];
		const long map = ch->GetMapIndex();
		if (KeepPlayerBotTowerAlive(ch, state, dwNow, PLAYERBOT_PDG_POTION_HP, PLAYERBOT_PDG_POTION_SP))
			return true;
		if (BreakOffPlayerBotArzDgBoss(ch, state, map, dwNow))
			return true;
		if (ch->IsRiding() && !HasPlayerBotBattleHorse(ch))
		{
			SetPlayerBotRidingForTravel(ch, state, false, dwNow, "dungeon_run");
			return true;
		}
		if (BuffPlayerBotTowerFellows(ch, state, dwNow))
			return true;
		const TPlayerBotArzDgScan& scan = ScanPlayerBotArzDg(map, dwNow);
		std::vector<DWORD> goals;
		GetPlayerBotDgRunObjectives(def, d, scan, goals);
		bool dragonShielded = false;
		if (def.bKind == DGRUN_KIND_SMOK)
			for (size_t i = 0; i < scan.foes.size() && !dragonShielded; ++i)
				dragonShielded = scan.foes[i].dwRace >= 8031 && scan.foes[i].dwRace <= 8034;
		LPCHARACTER threat = NULL, goal = NULL, any = NULL;
		int dThreat = INT_MAX, dGoal = INT_MAX, dAny = INT_MAX;
		for (size_t i = 0; i < scan.foes.size(); ++i)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().Find(scan.foes[i].dwVID);
			if (!IsPlayerBotPdgFoe(ch, c))
				continue;
			const DWORD race = c->GetRaceNum();
			if (dragonShielded && race == PLAYERBOT_PDG_BLUE_DRAGON)
				continue;
			const bool isGoal = std::find(goals.begin(), goals.end(), race) != goals.end();
			// The dungeons' NPC-like pieces (a seal, a door, a lion) are no foe
			// unless the stage names them.
			if (!isGoal && race >= 20000)
				continue;
			const int dist = DISTANCE_APPROX(ch->GetX() - c->GetX(), ch->GetY() - c->GetY());
			LPCHARACTER v = c->GetVictim();
			bool onParty = v == ch;
			if (!onParty && v && v->IsPC())
			{
				std::map<DWORD, TPlayerBotDgRunBot>::const_iterator b = s_mapPlayerBotDgRunBots.find(v->GetPlayerID());
				onParty = b != s_mapPlayerBotDgRunBots.end() && b->second.iRun == run.iId;
			}
			if (onParty && dist <= PLAYERBOT_DGRUN_THREAT_RANGE && dist < dThreat)
			{
				threat = c;
				dThreat = dist;
			}
			if (IsPlayerBotDgRunUnreachable(rb, (DWORD)c->GetVID(), dwNow))
				continue;
			if (isGoal && dist < dGoal)
			{
				goal = c;
				dGoal = dist;
			}
			if (dist < dAny)
			{
				any = c;
				dAny = dist;
			}
		}
		// The target in hand is kept while it is the same kind of choice.
		LPCHARACTER cur = state.dwTargetVID ? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		if (!IsPlayerBotPdgFoe(ch, cur) || (dragonShielded && cur->GetRaceNum() == PLAYERBOT_PDG_BLUE_DRAGON) ||
				IsPlayerBotDgRunUnreachable(rb, (DWORD)cur->GetVID(), dwNow))
			cur = NULL;
		LPCHARACTER foe = threat ? threat : (goal ? goal : any);
		if (cur && foe && cur != foe)
		{
			const bool curGoal = std::find(goals.begin(), goals.end(), cur->GetRaceNum()) != goals.end();
			if ((foe == threat && cur->GetVictim() == ch) || (foe == goal && curGoal) || (foe == any && !threat && !goal))
				foe = cur;
		}
		// MT2009_PLUS_BOT_DUNGEON_RUNS_V2: nothing new pulled while the leader
		// is hurt or fewer than two of the others (all, when fewer are up)
		// stand by it - what is on the party close by is fought, and it waits.
		{
			int others = 0, nearBy = 0;
			LPCHARACTER mate = CountPlayerBotDgRunPack(run, ch, PLAYERBOT_DGRUN_PACK_NEAR, others, nearBy);
			const bool hurt = ch->GetMaxHP() > 0 &&
					(long long)ch->GetHP() * 100 < (long long)ch->GetMaxHP() * PLAYERBOT_DGRUN_LEADER_HOLD_HP;
			const bool alone = others > 0 && nearBy < std::min(others, 2);
			if (!alone)
				rb.dwHoldSince = 0;
			else if (rb.dwHoldSince == 0)
				rb.dwHoldSince = dwNow;
			// A pack that cannot come is not waited for for ever.
			const bool waited = alone && dwNow - rb.dwHoldSince > PLAYERBOT_DGRUN_REGROUP_MAX_MS;
			if (hurt || (alone && !waited))
			{
				LPCHARACTER keep = NULL;
				if (cur && (cur->IsStone() || IsPlayerBotDgRunMemberOf(cur->GetVictim(), run.iId)) &&
						DISTANCE_APPROX(ch->GetX() - cur->GetX(), ch->GetY() - cur->GetY()) <= PLAYERBOT_DGRUN_HOLD_THREAT_RANGE)
					keep = cur;
				else if (threat && dThreat <= PLAYERBOT_DGRUN_HOLD_THREAT_RANGE)
					keep = threat;
				if (!keep)
				{
					state.dwTargetVID = 0;
					ch->SetVictim(NULL);
					// Towards the nearest of the others standing, when they
					// fight apart from it; else where it stands.
					if (!hurt && mate &&
							DISTANCE_APPROX(mate->GetX() - ch->GetX(), mate->GetY() - ch->GetY()) > PLAYERBOT_DGRUN_PACK_NEAR / 2)
					{
						SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
						if (dwNow >= state.dwNextTowerMoveTime)
						{
							state.dwNextTowerMoveTime = dwNow + 1500;
							MovePlayerBot(ch, mate->GetX(), mate->GetY(), dwNow, 6, true, false);
						}
						return true;
					}
					if (ch->IsStateMove())
						ch->Stop();
					SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
					PlayerBotLogThrottled("dgrun_hold", dwNow,
							"BOT_DGRUN: leader holds pid=%u name=%s run=%d hurt=%d others=%d near=%d hp=%d/%d", ch->GetPlayerID(),
							ch->GetName(), run.iId, hurt ? 1 : 0, others, nearBy, ch->GetHP(), ch->GetMaxHP());
					return true;
				}
				foe = keep;
			}
		}
		if (!foe)
		{
			// Between two stages, or the room cleared: what lies about, and
			// then a wait by the way in, where the next stage begins.
			if (HandleLoot(ch, state, dwNow))
				return true;
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(def.lMap);
			if (pMap && def.bKind != DGRUN_KIND_RAZADOR && def.bKind != DGRUN_KIND_NEMERE)
			{
				const long x = pMap->m_setting.iBaseX + def.lEntryCellX * 100;
				const long y = pMap->m_setting.iBaseY + def.lEntryCellY * 100;
				if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) > 600)
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
		state.bLastNavOutcome = PLAYERBOT_NAV_OUT_NONE;
		const bool fought = FightPlayerBotTowerObjective(ch, state, foe, dwNow);
		if (state.bLastNavOutcome == PLAYERBOT_NAV_OUT_UNREACHABLE)
		{
			// Any cell near it; then the objective is set down by the party
			// (the cohort's unstick), and anything else left alone a while.
			state.bLastNavOutcome = PLAYERBOT_NAV_OUT_NONE;
			ClearPlayerBotRoute(state, false);
			state.dwNextNavPlanTime = 0;
			MovePlayerBot(ch, foe->GetX(), foe->GetY(), dwNow, 24, true, false);
			if (state.bLastNavOutcome == PLAYERBOT_NAV_OUT_UNREACHABLE)
			{
				if (foe == goal || std::find(goals.begin(), goals.end(), foe->GetRaceNum()) != goals.end())
					UnstickPlayerBotArzDgFoe(ch, foe, ch->GetX(), ch->GetY(), dwNow);
				else
				{
					rb.unreachable[(DWORD)foe->GetVID()] = dwNow + PLAYERBOT_DGRUN_UNREACHABLE_MS;
					state.dwTargetVID = 0;
					ch->SetVictim(NULL);
				}
			}
		}
		return fought;
	}

	// Inside the run's instance.
	bool ManagePlayerBotDgRunInside(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotDgRun& run, TPlayerBotDgRunBot& rb,
			DWORD dwNow)
	{
		const TPlayerBotDgRunDef& def = PLAYERBOT_DGRUN_DEFS[run.iDef];
		LPDUNGEON d = ch->GetDungeon();
		if (!d)
		{
			d = CDungeonManager::instance().FindByMapIndex(ch->GetMapIndex());
			if (d)
				ch->SetDungeon(d);
		}
		if (!d)
			return false;
		state.lLastX = ch->GetX();
		state.lLastY = ch->GetY();
		if (rb.lLastX != 0 && DISTANCE_APPROX(ch->GetX() - rb.lLastX, ch->GetY() - rb.lLastY) > 3000)
		{
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ClearPlayerBotRoute(state, true);
			if (ch->IsStateMove())
				ch->Stop();
		}
		rb.lLastX = ch->GetX();
		rb.lLastY = ch->GetY();
		if (rb.pdg.lInstance != run.lInstance)
		{
			rb.pdg.lInstance = run.lInstance;
			rb.pdg.dwEnteredAt = dwNow;
		}
		RestockPlayerBotArzDg(ch, rb.arz, dwNow);
		// The drops, as on any hunt - while nothing is after the bot or the party.
		if (!state.bRecoveringAfterDeath && !FindPlayerBotDgRunThreat(ch, run.iId) && HandleLoot(ch, state, dwNow))
			return true;
		if (def.bKind == DGRUN_KIND_AREZZO)
		{
			const int dg = GetPlayerBotArzDgIndex(run.lInstance);
			if (dg >= 0)
				return ManagePlayerBotArzDgInside(ch, state, dg, rb.arz, dwNow, run.lPackX, run.lPackY, run.iPackN);
		}
		// MT2009_PLUS_BOT_DUNGEON_RUNS_V2: alive first, the Shaman's Cure, and
		// the fallback to the camp.
		if (KeepPlayerBotDgRunAlive(ch, state, run, dwNow))
			return true;
		if (HealPlayerBotDgRunFellow(ch, state, run, dwNow))
			return true;
		if (ManagePlayerBotPdgItems(ch, state, rb.pdg, dwNow))
			return true;
		if (run.bFallback)
			return FallBackPlayerBotDgRun(ch, state, run, dwNow);
		LPCHARACTER anchor = run.dwAnchor ? CHARACTER_MANAGER::instance().FindByPID(run.dwAnchor) : NULL;
		if (!anchor || anchor == ch || anchor->GetMapIndex() != ch->GetMapIndex())
			return DrivePlayerBotDgRunLeader(ch, state, run, rb, d, dwNow);
		return FightPlayerBotPartyDungeon(ch, state, rb.pdg, anchor, dwNow);
	}

	// From the top of the bot's tick, right after the Arezzo cohort's: a bot of
	// a run belongs to the run from its answer to the way out - except inside
	// the Demon Tower, whose floors are the tower pass's. False for everybody
	// else, and for a bot that has not answered yet (its own life goes on for
	// the few seconds).
	bool ManagePlayerBotDungeonRun(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || s_mapPlayerBotDgRunBots.empty())
			return false;
		std::map<DWORD, TPlayerBotDgRunBot>::iterator it = s_mapPlayerBotDgRunBots.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotDgRunBots.end())
			return false;
		std::map<int, TPlayerBotDgRun>::iterator r = s_mapPlayerBotDgRuns.find(it->second.iRun);
		if (r == s_mapPlayerBotDgRuns.end())
		{
			s_mapPlayerBotDgRunBots.erase(it);
			return false;
		}
		TPlayerBotDgRun& run = r->second;
		TPlayerBotDgRunBot& rb = it->second;
		if (!rb.bAnswered || ch->IsDead())
			return false;
		state.dwLastMeaningfulActivityTime = dwNow;
		switch (run.bPhase)
		{
			case DGRUN_PHASE_GATHER:
			case DGRUN_PHASE_STONE:
				return ManagePlayerBotDgRunGather(ch, state, run, rb, dwNow);
			case DGRUN_PHASE_INSIDE:
				if (PLAYERBOT_DGRUN_DEFS[run.iDef].bKind == DGRUN_KIND_TOWER)
					return false;
				if (run.lInstance == 0 || ch->GetMapIndex() != run.lInstance)
				{
					// Not in yet, or out already: standing, the monitor decides.
					if (ch->IsStateMove())
						ch->Stop();
					return true;
				}
				return ManagePlayerBotDgRunInside(ch, state, run, rb, dwNow);
			default:
				return false;
		}
	}

	// ------------------------------------------------------------ the run's end

	void RestPlayerBotDgRun(DWORD pid, DWORD dwNow, DWORD ms)
	{
		s_mapPlayerBotDgRunRest[pid] = dwNow + ms;
	}

	// A bot out of a run: its party, its run items, its way home from an
	// Arezzo map or the temple, its rest.
	void ReleasePlayerBotDgRunBot(DWORD pid, TPlayerBotDgRun& run, DWORD dwNow, DWORD restMs, const char* why)
	{
		std::map<DWORD, TPlayerBotDgRunBot>::iterator it = s_mapPlayerBotDgRunBots.find(pid);
		LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pid);
		TPlayerBotAIState* state = GetPlayerBotDgRunState(pid);
		if (ch && it != s_mapPlayerBotDgRunBots.end())
		{
			DropPlayerBotPdgItems(ch);
			// The run's party, never a person's.
			LPPARTY party = ch->GetParty();
			if (party && !IsPlayerBotHumanLedParty(party))
			{
				LPCHARACTER partyLeader = party->GetLeaderCharacter();
				std::map<DWORD, TPlayerBotDgRunBot>::const_iterator lb = partyLeader
						? s_mapPlayerBotDgRunBots.find(partyLeader->GetPlayerID()) : s_mapPlayerBotDgRunBots.end();
				if (lb != s_mapPlayerBotDgRunBots.end() && lb->second.iRun == run.iId)
					LeavePlayerBotParty(ch);
			}
			// Off an Arezzo map or out of the temple (the Jungle's way out is
			// the Las): home, while the run still lets the move through.
			const long map = ch->GetMapIndex();
			if (state && !ch->IsDead() && (IsPlayerBotArezzoMap(map) || map == PLAYERBOT_MAP_OCHAO ||
					IsPlayerBotOffLimitsMap(map)))
			{
				const TPlayerBotDgRunBot& rb = it->second;
				bool home = false;
				if (rb.lHomeMap > 0 && rb.lHomeMap < PLAYERBOT_INSTANCE_MAP_INDEX_MIN && !IsPlayerBotArezzoMap(rb.lHomeMap) &&
						!IsPlayerBotOffLimitsMap(rb.lHomeMap) && rb.lHomeMap != PLAYERBOT_MAP_OCHAO &&
						IsPlayerBotMapHostedHere(rb.lHomeMap))
					home = TransitionPlayerBotMap(ch, *state, rb.lHomeMap, rb.lHomeX, rb.lHomeY, dwNow, "dungeon_run_home");
				if (!home)
				{
					long destMap = 0, destX = 0, destY = 0;
					if (GetPlayerBotVillageReturn(ch, playerbot_empire_rules::MAP_ROLE_M2, destMap, destX, destY))
						home = TransitionPlayerBotMap(ch, *state, destMap, destX, destY, dwNow, "dungeon_run_home");
				}
				sys_log(0, "BOT_DGRUN: home pid=%u name=%s run=%d from=%ld ok=%d", pid, ch->GetName(), run.iId, map,
						home ? 1 : 0);
			}
			if (state)
			{
				state->dwTargetVID = 0;
				ch->SetVictim(NULL);
				ClearPlayerBotRoute(*state, true);
				SetPlayerBotAction(*state, BOT_ACTION_IDLE, dwNow);
				state->dwLastMeaningfulActivityTime = dwNow;
			}
		}
		if (it != s_mapPlayerBotDgRunBots.end())
			s_mapPlayerBotDgRunBots.erase(it);
		RestPlayerBotDgRun(pid, dwNow, restMs);
		if (why)
			sys_log(0, "BOT_DGRUN: out of the run pid=%u name=%s run=%d dungeon=%s why=%s", pid, ch ? ch->GetName() : "?",
					run.iId, PLAYERBOT_DGRUN_DEFS[run.iDef].szKey, why);
	}

	void DropPlayerBotDgRunMember(TPlayerBotDgRun& run, DWORD pid, DWORD dwNow, DWORD restMs, const char* why)
	{
		std::vector<DWORD>::iterator m = std::find(run.members.begin(), run.members.end(), pid);
		if (m != run.members.end())
			run.members.erase(m);
		ReleasePlayerBotDgRunBot(pid, run, dwNow, restMs, why);
	}

	// The run over: its line in the log and the table, the leader's word, the
	// party gone, everybody resting.
	void ClosePlayerBotDgRun(TPlayerBotDgRun& run, const char* result, DWORD dwNow)
	{
		const TPlayerBotDgRunDef& def = PLAYERBOT_DGRUN_DEFS[run.iDef];
		TPlayerBotDgRunStats& st = s_aPlayerBotDgRunStats[run.iDef];
		const bool entered = run.dwEnteredAt != 0;
		const DWORD took = entered ? ((run.dwFinishedAt ? run.dwFinishedAt : dwNow) - run.dwEnteredAt) : 0;
		if (!strcmp(result, "finished"))
		{
			++st.dwFinished;
			st.ullFinishedMs += took;
		}
		else if (!strcmp(result, "abandoned") || !strcmp(result, "aborted"))
			++st.dwAbandoned;
		else if (!strcmp(result, "timeout"))
			++st.dwTimeout;
		else if (!strcmp(result, "wipe"))
			++st.dwWipe;
		else if (!strcmp(result, "disbanded") || !strcmp(result, "no_stone"))
			++st.dwDisbanded;
		else
			++st.dwLost;
		int lvLow = 0, lvHigh = 0;
		std::string names;
		LPCHARACTER leader = CHARACTER_MANAGER::instance().FindByPID(run.dwLeader);
		for (size_t i = 0; i < run.members.size(); ++i)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(run.members[i]);
			if (!c)
				continue;
			const int lv = c->GetLevel();
			lvLow = lvLow == 0 || lv < lvLow ? lv : lvLow;
			lvHigh = lv > lvHigh ? lv : lvHigh;
			if (names.size() < 200)
			{
				if (!names.empty())
					names += ",";
				names += c->GetName();
			}
		}
		sys_log(0, "BOT_DGRUN: closed run=%d dungeon=%s instance=%ld empire=%u result=%s took_s=%u stage=%d stage_s=%s deaths=%d wipes=%d fees=%lld members=%u leader=%s break_offs=%d fallbacks=%d",
				run.iId, def.szKey, run.lInstance, (unsigned int)run.bEmpire, result, took / 1000, run.iStage,
				run.stageSecs.empty() ? "-" : run.stageSecs.c_str(), run.iDeaths, run.iWipes, run.llFees,
				(unsigned int)run.members.size(), leader ? leader->GetName() : "-", run.iBreakOffs, run.iFallbacks);
		FILE* fp = fopen(PLAYERBOT_DGRUN_RUNS_FILE, "a");
		if (fp)
		{
			fprintf(fp, "%ld\t%d\t%s\t%ld\t%u\t%u\t%d\t%d\t%s\t%u\t%d\t%s\t%d\t%d\t%lld\t%s\t%s\n", (long)time(0), run.iId,
					def.szKey, run.lInstance, (unsigned int)run.bEmpire, (unsigned int)run.members.size(), lvLow, lvHigh,
					result, took / 1000, run.iStage, run.stageSecs.empty() ? "-" : run.stageSecs.c_str(), run.iDeaths,
					run.iWipes, run.llFees, leader ? leader->GetName() : "-", names.empty() ? "-" : names.c_str());
			fclose(fp);
		}
		// The leader's word on the kingdom's shout, now and then.
		if (leader && entered && number(1, 100) <= 40)
		{
			playerbot_conv::TRng rng = MakePlayerBotDgRunRng();
			playerbot_dgrun::TLine l = MakePlayerBotDgRunLine(leader, run);
			l.minutes = (int)((took + 59999) / 60000);
			if (!strcmp(result, "finished"))
				ShoutPlayerBotDgRun(leader, playerbot_dgrun::DoneLine(rng, l), "done");
			else if (strcmp(result, "aborted"))
				ShoutPlayerBotDgRun(leader, playerbot_dgrun::FailLine(rng, l), "failed");
		}
		const DWORD rest = entered ? PLAYERBOT_DGRUN_REST_MIN_MS + (DWORD)number(0, (int)PLAYERBOT_DGRUN_REST_SPREAD_MS)
				: PLAYERBOT_DGRUN_REST_DISBAND_MS;
		const std::vector<DWORD> members = run.members;
		for (size_t i = 0; i < members.size(); ++i)
			ReleasePlayerBotDgRunBot(members[i], run, dwNow, rest, NULL);
		run.members.clear();
		if (run.lInstance != 0 && def.bKind != DGRUN_KIND_TOWER)
		{
			s_mapPlayerBotArzDgScan.erase(run.lInstance);
			LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(run.lInstance);
			if (d)
			{
				d->SetFlag("mt2009_keep_until", 0);
				d->SetFlag("closed", 1);
			}
		}
	}

	// Everybody alive still inside, out to the gathering (the dead once they stand).
	void PullPlayerBotDgRun(TPlayerBotDgRun& run, const char* why, DWORD dwNow)
	{
		if (!run.szResult)
			run.szResult = why;
		if (dwNow < run.dwNextPull)
			return;
		run.dwNextPull = dwNow + PLAYERBOT_DGRUN_PULL_RETRY_MS;
		LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(run.lInstance);
		if (d)
		{
			d->SetFlag("mt2009_keep_until", 0);
			d->SetFlag("closed", 1);
		}
		for (size_t i = 0; i < run.members.size(); ++i)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(run.members[i]);
			if (!ch || ch->IsDead() || ch->GetMapIndex() != run.lInstance)
				continue;
			s_szPlayerBotTransitionRefusal = NULL;
			bool ok = ch->WarpSet(run.place.x, run.place.y);
			if (!ok)
			{
				TPlayerBotAIState* state = GetPlayerBotDgRunState(run.members[i]);
				if (state)
					ok = TransitionPlayerBotMap(ch, *state, run.place.map, run.place.x, run.place.y, dwNow, "dungeon_run_out");
			}
			sys_log(0, "BOT_DGRUN: pulled out pid=%u name=%s run=%d dungeon=%s why=%s ok=%d refused=%s", ch->GetPlayerID(),
					ch->GetName(), run.iId, PLAYERBOT_DGRUN_DEFS[run.iDef].szKey, why, ok ? 1 : 0,
					s_szPlayerBotTransitionRefusal ? s_szPlayerBotTransitionRefusal : "-");
			s_szPlayerBotTransitionRefusal = NULL;
		}
	}

	// ------------------------------------------------------------ the way in

	// What a player pays at the guard: the cooldown flag, the day's count and,
	// from a bot that can spare it, the fee.
	long long ChargePlayerBotDgRunEntry(LPCHARACTER ch, const TPlayerBotDgRunDef& def)
	{
		const mt2009_dpanel::Def* panel = mt2009_dpanel::FindKey(def.szKey);
		if (panel && !panel->cdFlag.empty() && panel->cdSec > 0)
			ch->SetQuestFlag(panel->cdFlag, get_global_time());
		if (def.szQuest)
		{
			const std::string q(def.szQuest);
			const int today = PlayerBotDgRunToday();
			if (ch->GetQuestFlag(q + ".day") != today)
			{
				ch->SetQuestFlag(q + ".day", today);
				ch->SetQuestFlag(q + ".runs", 0);
			}
			ch->SetQuestFlag(q + ".runs", ch->GetQuestFlag(q + ".runs") + 1);
		}
		long long fee = def.llFee;
		if (fee > 0 && (long long)ch->GetGold() >= fee * PLAYERBOT_DGRUN_FEE_RESERVE)
			PlayerBotChangeGold(ch, -fee);
		else
			fee = 0;
		return fee;
	}

	// The gathered party into a new instance of the dungeon.
	bool StartPlayerBotDgRunInstance(TPlayerBotDgRun& run, DWORD dwNow)
	{
		const TPlayerBotDgRunDef& def = PLAYERBOT_DGRUN_DEFS[run.iDef];
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(def.lMap);
		if (!pMap)
			return false;
		LPDUNGEON d = CDungeonManager::instance().Create(def.lMap);
		if (!d)
		{
			PlayerBotLogThrottled("dgrun_create_failed", dwNow, "BOT_DGRUN: instance refused dungeon=%s", def.szKey);
			return false;
		}
		d->SetFlag("mt2009_dgrun", 1);
		run.lInstance = d->GetMapIndex();
		const long x = pMap->m_setting.iBaseX + def.lEntryCellX * 100;
		const long y = pMap->m_setting.iBaseY + def.lEntryCellY * 100;
		// V2: the first camp is the way in.
		run.lCampX = x;
		run.lCampY = y;
		std::vector<DWORD> in;
		const std::vector<DWORD> members = run.members;
		for (size_t i = 0; i < members.size(); ++i)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(members[i]);
			TPlayerBotAIState* state = GetPlayerBotDgRunState(members[i]);
			if (!ch || !state || ch->IsDead())
			{
				DropPlayerBotDgRunMember(run, members[i], dwNow, PLAYERBOT_DGRUN_REST_REFUSED_MS, "not_ready");
				continue;
			}
			// The way out: the gathering spot (d.exit_all reads it; the quests
			// that name their own exit set theirs at the end).
			ch->SetWarpLocation(run.place.map, run.place.x / 100, run.place.y / 100);
			state->dwTargetVID = 0;
			ch->SetVictim(NULL);
			ClearPlayerBotRoute(*state, true);
			const long jx = x + (long)(in.size() % 3) * 150 - 150;
			const long jy = y + (long)(in.size() / 3) * 150 - 75;
			s_szPlayerBotTransitionRefusal = NULL;
			if (!ch->WarpSet(jx, jy, run.lInstance) || ch->GetMapIndex() != run.lInstance)
			{
				sys_log(0, "BOT_DGRUN: jump refused pid=%u name=%s run=%d dungeon=%s instance=%ld refused=%s",
						ch->GetPlayerID(), ch->GetName(), run.iId, def.szKey, run.lInstance,
						s_szPlayerBotTransitionRefusal ? s_szPlayerBotTransitionRefusal : "?");
				s_szPlayerBotTransitionRefusal = NULL;
				DropPlayerBotDgRunMember(run, members[i], dwNow, PLAYERBOT_DGRUN_REST_REFUSED_MS, "jump_refused");
				continue;
			}
			s_szPlayerBotTransitionRefusal = NULL;
			run.llFees += ChargePlayerBotDgRunEntry(ch, def);
			in.push_back(members[i]);
		}
		if (in.empty())
		{
			CDungeonManager::instance().Destroy(d->GetId());
			run.lInstance = 0;
			return false;
		}
		CallPlayerBotDgRunTimer(def.szInitTimer, run.lInstance);
		return true;
	}

	// ------------------------------------------------------------ the monitor

	void UpdatePlayerBotDgRunParty(TPlayerBotDgRun& run)
	{
		LPCHARACTER leader = CHARACTER_MANAGER::instance().FindByPID(run.dwLeader);
		if (!leader)
			return;
		std::map<DWORD, TPlayerBotDgRunBot>::const_iterator lb = s_mapPlayerBotDgRunBots.find(run.dwLeader);
		if (lb == s_mapPlayerBotDgRunBots.end() || !lb->second.bArrived)
			return;
		LPPARTY party = leader->GetParty();
		// A party of its own, not one the leader happened to lead before the call.
		if (!party || party->GetLeaderPID() != leader->GetPlayerID() || !run.bPartyMade)
		{
			run.bPartyMade = true;
			if (party)
				LeavePlayerBotParty(leader);
			party = CPartyManager::instance().CreateParty(leader);
			if (!party)
				return;
			party->Link(leader);
			party->SetParameter(PARTY_EXP_DISTRIBUTION_PARITY);
			sys_log(0, "BOT_DGRUN: party made run=%d leader=%s", run.iId, leader->GetName());
		}
		for (size_t i = 0; i < run.members.size(); ++i)
		{
			if (run.members[i] == run.dwLeader)
				continue;
			std::map<DWORD, TPlayerBotDgRunBot>::const_iterator b = s_mapPlayerBotDgRunBots.find(run.members[i]);
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(run.members[i]);
			if (!ch || b == s_mapPlayerBotDgRunBots.end() || !b->second.bArrived || ch->GetParty() == party ||
					ch->GetEmpire() != leader->GetEmpire() || party->GetMemberCount() >= (DWORD)playerbot_dgrun::PARTY_MAX)
				continue;
			if (ch->GetParty())
				LeavePlayerBotParty(ch);
			party->Join(ch->GetPlayerID());
			party->Link(ch);
		}
	}

	// The call answered, the party on its way and gathering: false when the
	// run is over (closed here) and can be forgotten.
	bool UpdatePlayerBotDgRunGather(TPlayerBotDgRun& run, DWORD dwNow)
	{
		const TPlayerBotDgRunDef& def = PLAYERBOT_DGRUN_DEFS[run.iDef];
		const playerbot_dgrun::TRunRule& rule = playerbot_dgrun::RuleFor(def.szKey);
		int answered = 0, pending = 0, arrived = 0;
		// V2: a Shaman that can heal among those on their way, among those there.
		bool healerAnswered = false, healerArrived = false, healerPending = false;
		const std::vector<DWORD> members = run.members;
		for (size_t i = 0; i < members.size(); ++i)
		{
			const DWORD pid = members[i];
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pid);
			std::map<DWORD, TPlayerBotDgRunBot>::iterator b = s_mapPlayerBotDgRunBots.find(pid);
			TPlayerBotAIState* state = GetPlayerBotDgRunState(pid);
			if (!ch || !state || b == s_mapPlayerBotDgRunBots.end())
			{
				DropPlayerBotDgRunMember(run, pid, dwNow, PLAYERBOT_DGRUN_REST_REFUSED_MS, "gone");
				continue;
			}
			TPlayerBotDgRunBot& rb = b->second;
			if (!rb.bAnswered)
			{
				if (dwNow < rb.dwAnswerAt)
				{
					++pending;
					healerPending = healerPending || (ch->GetJob() == JOB_SHAMAN && ch->GetSkillGroup() != 0);
					continue;
				}
				// Looked at again as it answers: whatever claimed it since keeps it.
				const char* refusal = GetPlayerBotDgRunRefusal(ch, *state, dwNow);
				if (refusal)
				{
					DropPlayerBotDgRunMember(run, pid, dwNow, PLAYERBOT_DGRUN_REST_REFUSED_MS, refusal);
					continue;
				}
				rb.bAnswered = true;
				rb.lHomeMap = ch->GetMapIndex();
				rb.lHomeX = ch->GetX();
				rb.lHomeY = ch->GetY();
				rb.dwTravelAt = dwNow + PLAYERBOT_DGRUN_TRAVEL_MIN_MS + (DWORD)number(0, (int)PLAYERBOT_DGRUN_TRAVEL_SPREAD_MS);
				const bool aloud = run.iAnswerShouts < PLAYERBOT_DGRUN_MAX_ANSWER_SHOUTS && number(1, 100) <= 55;
				if (aloud)
				{
					++run.iAnswerShouts;
					playerbot_conv::TRng rng = MakePlayerBotDgRunRng();
					ShoutPlayerBotDgRun(ch, playerbot_dgrun::AnswerLine(rng, MakePlayerBotDgRunLine(ch, run)), "answer");
				}
				sys_log(0, "BOT_DGRUN: answered pid=%u name=%s level=%u job=%u run=%d dungeon=%s aloud=%d from=%ld",
						pid, ch->GetName(), (unsigned int)ch->GetLevel(), (unsigned int)ch->GetJob(), run.iId, def.szKey,
						aloud ? 1 : 0, ch->GetMapIndex());
			}
			if (rb.iMoveFails >= PLAYERBOT_DGRUN_MOVE_FAILS)
			{
				DropPlayerBotDgRunMember(run, pid, dwNow, PLAYERBOT_DGRUN_REST_REFUSED_MS, "cannot_get_there");
				continue;
			}
			++answered;
			const bool healer = ch->GetJob() == JOB_SHAMAN && ch->GetSkillGroup() != 0;
			healerAnswered = healerAnswered || healer;
			if (rb.bArrived && ch->GetMapIndex() == run.place.map)
			{
				++arrived;
				healerArrived = healerArrived || healer;
			}
		}
		// The leader gone: whoever came first leads.
		if (std::find(run.members.begin(), run.members.end(), run.dwLeader) == run.members.end())
		{
			run.dwLeader = 0;
			for (size_t i = 0; i < run.members.size() && !run.dwLeader; ++i)
			{
				std::map<DWORD, TPlayerBotDgRunBot>::const_iterator b = s_mapPlayerBotDgRunBots.find(run.members[i]);
				if (b != s_mapPlayerBotDgRunBots.end() && b->second.bAnswered)
					run.dwLeader = run.members[i];
			}
			if (run.dwLeader)
			{
				std::vector<DWORD>::iterator m = std::find(run.members.begin(), run.members.end(), run.dwLeader);
				std::rotate(run.members.begin(), m, m + 1);
				sys_log(0, "BOT_DGRUN: new leader run=%d pid=%u", run.iId, run.dwLeader);
			}
		}
		UpdatePlayerBotDgRunParty(run);
		const bool late = dwNow - run.dwCalledAt >= PLAYERBOT_DGRUN_GATHER_MS;
		const bool everyone = pending == 0 && answered > 0 && arrived >= answered;
		LPCHARACTER leader = run.dwLeader ? CHARACTER_MANAGER::instance().FindByPID(run.dwLeader) : NULL;
		const bool startOk = playerbot_dgrun::CanStart(arrived, healerArrived, rule);
		if (run.bPhase == DGRUN_PHASE_GATHER && leader && startOk && (everyone || late))
		{
			// Whoever is not there is not in it.
			const std::vector<DWORD> now = run.members;
			for (size_t i = 0; i < now.size(); ++i)
			{
				std::map<DWORD, TPlayerBotDgRunBot>::const_iterator b = s_mapPlayerBotDgRunBots.find(now[i]);
				LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(now[i]);
				if (b == s_mapPlayerBotDgRunBots.end() || !b->second.bArrived || !c || c->GetMapIndex() != run.place.map)
					DropPlayerBotDgRunMember(run, now[i], dwNow, PLAYERBOT_DGRUN_REST_DISBAND_MS, "did_not_come");
			}
			if (std::find(run.members.begin(), run.members.end(), run.dwLeader) == run.members.end())
				return true;	// the next second picks a leader among those there
			UpdatePlayerBotDgRunParty(run);
			if (number(1, 100) <= 35)
			{
				playerbot_conv::TRng rng = MakePlayerBotDgRunRng();
				ShoutPlayerBotDgRun(leader, playerbot_dgrun::StartLine(rng, MakePlayerBotDgRunLine(leader, run)), "start");
			}
			if (def.bKind == DGRUN_KIND_TOWER)
			{
				run.bPhase = DGRUN_PHASE_STONE;
				run.dwPhaseSince = dwNow;
				sys_log(0, "BOT_DGRUN: gathered run=%d dungeon=%s members=%u - breaking the stone", run.iId, def.szKey,
						(unsigned int)run.members.size());
				return true;
			}
			if (!StartPlayerBotDgRunInstance(run, dwNow))
			{
				ClosePlayerBotDgRun(run, "lost", dwNow);
				return false;
			}
			run.bPhase = DGRUN_PHASE_INSIDE;
			run.dwPhaseSince = run.dwEnteredAt = run.dwLastProgress = run.dwStageSince = dwNow;
			run.dwNextStep = dwNow + PLAYERBOT_DGRUN_NEXT_STEP_MS;
			++s_aPlayerBotDgRunStats[run.iDef].dwStarted;
			std::string names;
			for (size_t i = 0; i < run.members.size(); ++i)
			{
				LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(run.members[i]);
				if (c && names.size() < 200)
				{
					if (!names.empty())
						names += ",";
					names += c->GetName();
				}
			}
			sys_log(0, "BOT_DGRUN: entered run=%d dungeon=%s instance=%ld empire=%u members=%u fees=%lld names=%s",
					run.iId, def.szKey, run.lInstance, (unsigned int)run.bEmpire, (unsigned int)run.members.size(),
					run.llFees, names.c_str());
			return true;
		}
		// Too few, or nobody to lead: the leader says so, the party goes.
		// V2: nor without the dungeon's Shaman once none can come.
		const bool noHealer = rule.needHealer && !healerAnswered && !healerPending;
		if ((late && !startOk) || (pending == 0 && answered < rule.startMin) || noHealer || run.members.empty() || !leader)
		{
			if (leader && answered > 0)
			{
				playerbot_conv::TRng rng = MakePlayerBotDgRunRng();
				ShoutPlayerBotDgRun(leader, playerbot_dgrun::DisbandLine(rng, MakePlayerBotDgRunLine(leader, run)), "disband");
			}
			sys_log(0, "BOT_DGRUN: disbanded run=%d dungeon=%s answered=%d arrived=%d pending=%d shaman=%d after_s=%u", run.iId,
					def.szKey, answered, arrived, pending, healerAnswered ? 1 : 0, (dwNow - run.dwCalledAt) / 1000);
			ClosePlayerBotDgRun(run, "disbanded", dwNow);
			return false;
		}
		return true;
	}

	// The Demon Tower: the stone, then the floors - the tower's pass runs them.
	bool UpdatePlayerBotDgRunTower(TPlayerBotDgRun& run, DWORD dwNow)
	{
		int inside = 0;
		long instance = 0;
		for (size_t i = 0; i < run.members.size(); ++i)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(run.members[i]);
			if (ch && IsPlayerBotDemonTowerInstance(ch->GetMapIndex()))
			{
				++inside;
				instance = ch->GetMapIndex();
			}
		}
		if (run.bPhase == DGRUN_PHASE_STONE)
		{
			if (inside > 0)
			{
				run.bPhase = DGRUN_PHASE_INSIDE;
				run.lInstance = instance;
				run.dwPhaseSince = run.dwEnteredAt = run.dwLastProgress = run.dwStageSince = dwNow;
				++s_aPlayerBotDgRunStats[run.iDef].dwStarted;
				// Whoever did not go in with the jump is not in it.
				const std::vector<DWORD> now = run.members;
				for (size_t i = 0; i < now.size(); ++i)
				{
					LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(now[i]);
					if (!c || c->GetMapIndex() != instance)
						DropPlayerBotDgRunMember(run, now[i], dwNow, PLAYERBOT_DGRUN_REST_DISBAND_MS, "missed_the_jump");
				}
				sys_log(0, "BOT_DGRUN: entered run=%d dungeon=wieza instance=%ld empire=%u members=%u", run.iId, instance,
						(unsigned int)run.bEmpire, (unsigned int)run.members.size());
				return true;
			}
			if (dwNow - run.dwPhaseSince >= PLAYERBOT_DGRUN_STONE_MS)
			{
				ClosePlayerBotDgRun(run, "no_stone", dwNow);
				return false;
			}
			return true;
		}
		std::map<long, TPlayerBotTowerRun>::const_iterator tr = s_mapPlayerBotTowerRuns.find(run.lInstance);
		if (tr != s_mapPlayerBotTowerRuns.end() && tr->second.iLevel + 2 != run.iStage)
		{
			char buf[16];
			snprintf(buf, sizeof(buf), "%s%u", run.stageSecs.empty() ? "" : ",", (dwNow - run.dwStageSince) / 1000);
			if (run.iStage > 0 && run.stageSecs.size() < 120)
				run.stageSecs += buf;
			run.iStage = tr->second.iLevel + 2;
			run.dwStageSince = dwNow;
			sys_log(0, "BOT_DGRUN: stage run=%d dungeon=wieza floor=%d after_s=%u", run.iId, run.iStage,
					(dwNow - run.dwEnteredAt) / 1000);
		}
		if (tr != s_mapPlayerBotTowerRuns.end() && tr->second.bReaperDown && !run.dwFinishedAt)
			run.dwFinishedAt = dwNow;
		if (inside == 0)
		{
			if (run.dwOutSince == 0)
				run.dwOutSince = dwNow;
			if (dwNow - run.dwOutSince >= PLAYERBOT_DGRUN_TOWER_OUT_GRACE_MS)
			{
				ClosePlayerBotDgRun(run, run.dwFinishedAt ? "finished" : "left", dwNow);
				return false;
			}
			return true;
		}
		run.dwOutSince = 0;
		if (dwNow - run.dwEnteredAt >= (DWORD)PLAYERBOT_DGRUN_DEFS[run.iDef].iLimitMin * 60000U)
		{
			// The tower's own stall ends a run that goes nowhere; this is the
			// last word: the members are bystanders of the tower from here.
			ClosePlayerBotDgRun(run, "timeout", dwNow);
			return false;
		}
		return true;
	}

	// One second of a run inside a quest's instance: false when it is over.
	bool UpdatePlayerBotDgRunInside(TPlayerBotDgRun& run, DWORD dwNow)
	{
		const TPlayerBotDgRunDef& def = PLAYERBOT_DGRUN_DEFS[run.iDef];
		LPDUNGEON d = CDungeonManager::instance().FindByMapIndex(run.lInstance);
		int inside = 0, alive = 0, fit = 0;
		long long sx = 0, sy = 0;
		run.dwAnchor = 0;
		// V2: the leader while it is not recovering, else the first of the
		// others that is not, else whoever stands.
		DWORD standIn = 0;
		LPCHARACTER leader = CHARACTER_MANAGER::instance().FindByPID(run.dwLeader);
		if (leader && leader->GetMapIndex() == run.lInstance && !leader->IsDead())
		{
			if (!IsPlayerBotDgRunRecovering(run.dwLeader))
				run.dwAnchor = run.dwLeader;
			else
				standIn = run.dwLeader;
		}
		const std::vector<DWORD> members = run.members;
		for (size_t i = 0; i < members.size(); ++i)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(members[i]);
			if (!ch)
			{
				// Logged out (a restart, the life schedule): no longer the run's.
				DropPlayerBotDgRunMember(run, members[i], dwNow, PLAYERBOT_DGRUN_REST_REFUSED_MS, "offline");
				continue;
			}
			if (ch->GetMapIndex() != run.lInstance)
				continue;
			++inside;
			const bool wasDead = run.dead.count(members[i]) != 0;
			if (ch->IsDead() && !wasDead)
			{
				run.dead.insert(members[i]);
				++run.iDeaths;
				++s_aPlayerBotDgRunStats[run.iDef].dwDeaths;
				sys_log(0, "BOT_DGRUN: died pid=%u name=%s run=%d dungeon=%s stage=%d level=%u", members[i], ch->GetName(),
						run.iId, def.szKey, run.iStage, (unsigned int)ch->GetLevel());
			}
			else if (!ch->IsDead() && wasDead)
				run.dead.erase(members[i]);
			if (ch->IsDead())
				continue;
			++alive;
			sx += ch->GetX();
			sy += ch->GetY();
			if (IsPlayerBotDgRunFit(ch))
				++fit;
			if (run.dwAnchor == 0 && !IsPlayerBotDgRunRecovering(members[i]))
				run.dwAnchor = members[i];
			if (standIn == 0)
				standIn = members[i];
		}
		if (run.dwAnchor == 0)
			run.dwAnchor = standIn;
		run.iPackN = alive;
		run.iFit = fit;
		if (alive > 0)
		{
			run.lPackX = (long)(sx / alive);
			run.lPackY = (long)(sy / alive);
		}
		if (!d || inside == 0)
		{
			if (run.dwOutSince == 0)
				run.dwOutSince = dwNow;
			if (!d || dwNow - run.dwOutSince >= PLAYERBOT_DGRUN_OUT_GRACE_MS)
			{
				const char* result = run.dwFinishedAt ? "finished" : (run.szResult ? run.szResult
						: (run.dwClosedSeen ? "timeout" : (d ? "left" : "lost")));
				ClosePlayerBotDgRun(run, result, dwNow);
				return false;
			}
			return true;
		}
		run.dwOutSince = 0;
		if (alive == 0 && !run.bAllDead)
		{
			++run.iWipes;
			sys_log(0, "BOT_DGRUN: wipe run=%d dungeon=%s stage=%d inside=%d wipes=%d after_s=%u", run.iId, def.szKey,
					run.iStage, inside, run.iWipes, (dwNow - run.dwEnteredAt) / 1000);
		}
		run.bAllDead = alive == 0;
		// MT2009_PLUS_BOT_DUNGEON_RUNS_V2: the camp - where the leader (or who
		// stands for it) is while no monster stands within
		// PLAYERBOT_DGRUN_QUIET_RANGE of it, or wherever the quest jumped it -
		// and the fallback to it.
		if (def.bKind != DGRUN_KIND_AREZZO)
		{
			LPCHARACTER a = run.dwAnchor ? CHARACTER_MANAGER::instance().FindByPID(run.dwAnchor) : NULL;
			if (a && !a->IsDead() && a->GetMapIndex() == run.lInstance && !IsPlayerBotDgRunRecovering(run.dwAnchor))
			{
				bool quiet = true;
				const TPlayerBotArzDgScan& scan = ScanPlayerBotArzDg(run.lInstance, dwNow);
				for (size_t i = 0; i < scan.foes.size() && quiet; ++i)
					quiet = scan.foes[i].dwRace >= 8000 ||
							DISTANCE_APPROX(scan.foes[i].lX - a->GetX(), scan.foes[i].lY - a->GetY()) > PLAYERBOT_DGRUN_QUIET_RANGE;
				const bool jumped = run.lCampX != 0 &&
						DISTANCE_APPROX(run.lCampX - a->GetX(), run.lCampY - a->GetY()) > 3 * PLAYERBOT_DGRUN_QUIET_RANGE + 1600;
				if (quiet || jumped || run.lCampX == 0)
				{
					run.lCampX = a->GetX();
					run.lCampY = a->GetY();
				}
			}
			if (!run.bFallback)
			{
				if (inside >= 3 && alive > 0 && fit * 2 < inside && dwNow >= run.dwFallbackRestUntil && !run.dwFinishedAt)
				{
					run.bFallback = true;
					run.dwFallbackSince = dwNow;
					++run.iFallbacks;
					sys_log(0, "BOT_DGRUN: fallback run=%d dungeon=%s stage=%d inside=%d alive=%d fit=%d camp=(%ld,%ld)", run.iId,
							def.szKey, run.iStage, inside, alive, fit, run.lCampX, run.lCampY);
				}
			}
			else if (alive == 0 || fit * 4 >= inside * 3 || dwNow - run.dwFallbackSince >= PLAYERBOT_DGRUN_FALLBACK_MAX_MS)
			{
				run.bFallback = false;
				run.dwFallbackRestUntil = dwNow + PLAYERBOT_DGRUN_FALLBACK_REST_MS;
				sys_log(0, "BOT_DGRUN: fallback over run=%d dungeon=%s inside=%d alive=%d fit=%d after_s=%u", run.iId, def.szKey,
						inside, alive, fit, (dwNow - run.dwFallbackSince) / 1000);
			}
		}
		const int stage = GetPlayerBotDgRunStage(def, d, run.iStage);
		if (stage != run.iStage)
		{
			if (run.iStage > 0 && run.stageSecs.size() < 120)
			{
				char buf[16];
				snprintf(buf, sizeof(buf), "%s%u", run.stageSecs.empty() ? "" : ",", (dwNow - run.dwStageSince) / 1000);
				run.stageSecs += buf;
			}
			sys_log(0, "BOT_DGRUN: stage run=%d dungeon=%s instance=%ld stage=%d after_s=%u inside=%d alive=%d", run.iId,
					def.szKey, run.lInstance, stage, (dwNow - run.dwEnteredAt) / 1000, inside, alive);
			run.iStage = stage;
			run.dwStageSince = dwNow;
		}
		if (!run.dwFinishedAt && IsPlayerBotDgRunWon(def, d))
		{
			run.dwFinishedAt = dwNow;
			if (run.stageSecs.size() < 120)
			{
				char buf[16];
				snprintf(buf, sizeof(buf), "%s%u", run.stageSecs.empty() ? "" : ",", (dwNow - run.dwStageSince) / 1000);
				run.stageSecs += buf;
			}
			sys_log(0, "BOT_DGRUN: finished run=%d dungeon=%s instance=%ld took_s=%u deaths=%d wipes=%d inside=%d",
					run.iId, def.szKey, run.lInstance, (dwNow - run.dwEnteredAt) / 1000, run.iDeaths, run.iWipes, inside);
		}
		const long long sig = GetPlayerBotDgRunSignature(def, d, run.lInstance, dwNow);
		if (sig != run.llSignature)
		{
			run.llSignature = sig;
			run.dwLastProgress = dwNow;
			run.bStallLogged = false;
		}
		HoldPlayerBotArzDgBossRegen(run.lInstance, dwNow);
		if (d->GetFlag("closed") == 1 && run.dwClosedSeen == 0)
			run.dwClosedSeen = dwNow;
		// Razador's statue and Nemere's lion, whenever no task runs.
		if (def.szNextTimer && !run.dwFinishedAt && !run.szResult && run.dwClosedSeen == 0 && dwNow >= run.dwNextStep &&
				d->GetFlag("init") == 1)
		{
			const bool ask = def.bKind == DGRUN_KIND_RAZADOR
					? (d->GetFlag("active") == 0 && d->GetFlag("boss") == 0)
					: (def.bKind == DGRUN_KIND_NEMERE && d->GetFlag("step") == 0);
			if (ask)
			{
				run.dwNextStep = dwNow + PLAYERBOT_DGRUN_NEXT_STEP_MS;
				CallPlayerBotDgRunTimer(def.szNextTimer, run.lInstance);
			}
		}
		// The ends.
		if (run.szResult)
		{
			PullPlayerBotDgRun(run, run.szResult, dwNow);
			return true;
		}
		if (run.dwFinishedAt && dwNow - run.dwFinishedAt > def.dwEndWaitMs)
		{
			PullPlayerBotDgRun(run, "finished", dwNow);
			return true;
		}
		if (run.dwClosedSeen && dwNow - run.dwClosedSeen > PLAYERBOT_DGRUN_CLOSED_GRACE_MS)
		{
			PullPlayerBotDgRun(run, run.dwFinishedAt ? "finished" : "timeout", dwNow);
			return true;
		}
		if (run.dwFinishedAt)
			return true;
		if (run.iWipes >= PLAYERBOT_DGRUN_MAX_WIPES)
		{
			sys_log(0, "BOT_DGRUN: given up after %d wipes run=%d dungeon=%s stage=%d", run.iWipes, run.iId, def.szKey,
					run.iStage);
			PullPlayerBotDgRun(run, "wipe", dwNow);
			return true;
		}
		if (dwNow - run.dwLastProgress >= PLAYERBOT_DGRUN_STALL_LOG_MS && !run.bStallLogged)
		{
			run.bStallLogged = true;
			sys_log(0, "BOT_DGRUN: stalled run=%d dungeon=%s stage=%d inside=%d alive=%d monsters=%u for_s=%u", run.iId,
					def.szKey, run.iStage, inside, alive, (unsigned int)ScanPlayerBotArzDg(run.lInstance, dwNow).foes.size(),
					(dwNow - run.dwLastProgress) / 1000);
		}
		if (dwNow - run.dwLastProgress >= PLAYERBOT_DGRUN_STALL_MS)
		{
			sys_log(0, "BOT_DGRUN: abandoned run=%d dungeon=%s stage=%d after_s=%u", run.iId, def.szKey, run.iStage,
					(dwNow - run.dwEnteredAt) / 1000);
			PullPlayerBotDgRun(run, "abandoned", dwNow);
			return true;
		}
		if (dwNow - run.dwEnteredAt >= (DWORD)def.iLimitMin * 60000U + PLAYERBOT_DGRUN_LIMIT_MARGIN_MS)
		{
			PullPlayerBotDgRun(run, "timeout", dwNow);
			return true;
		}
		return true;
	}

	bool UpdatePlayerBotDgRun(TPlayerBotDgRun& run, DWORD dwNow)
	{
		if (run.bPhase == DGRUN_PHASE_GATHER)
			return UpdatePlayerBotDgRunGather(run, dwNow);
		if (PLAYERBOT_DGRUN_DEFS[run.iDef].bKind == DGRUN_KIND_TOWER)
		{
			// The operator's abort: the members are the tower's bystanders from here.
			if (run.szResult)
			{
				ClosePlayerBotDgRun(run, run.szResult, dwNow);
				return false;
			}
			// Whoever logged out is no longer the run's.
			const std::vector<DWORD> members = run.members;
			for (size_t i = 0; i < members.size(); ++i)
				if (!CHARACTER_MANAGER::instance().FindByPID(members[i]))
					DropPlayerBotDgRunMember(run, members[i], dwNow, PLAYERBOT_DGRUN_REST_REFUSED_MS, "offline");
			if (run.members.empty())
			{
				ClosePlayerBotDgRun(run, "lost", dwNow);
				return false;
			}
			if (run.bPhase == DGRUN_PHASE_STONE)
				UpdatePlayerBotDgRunParty(run);
			return UpdatePlayerBotDgRunTower(run, dwNow);
		}
		return UpdatePlayerBotDgRunInside(run, dwNow);
	}

	// ------------------------------------------------------------ the call

	int CountPlayerBotDgRuns(int def)
	{
		int n = 0;
		for (std::map<int, TPlayerBotDgRun>::const_iterator it = s_mapPlayerBotDgRuns.begin(); it != s_mapPlayerBotDgRuns.end(); ++it)
			if (def < 0 || it->second.iDef == def)
				++n;
		return n;
	}

	int GetPlayerBotDgRunCap()
	{
		const int byBots = (int)(s_mapPlayerBotAIStates.size() / PLAYERBOT_DGRUN_BOTS_PER_RUN);
		return std::min(PLAYERBOT_DGRUN_MAX_RUNS, std::max(PLAYERBOT_DGRUN_MIN_RUNS, byBots));
	}

	struct TPlayerBotDgRunSlot
	{
		bool ok;
		bool empireOk[4];
		TPlayerBotLfgPlace place[4];
		int lvMin, lvMax, partyMin, partyMax;
		const mt2009_dpanel::Def* panel;
		TPlayerBotDgRunSlot() : ok(false), lvMin(0), lvMax(0), partyMin(1), partyMax(8), panel(NULL)
		{
			for (int e = 0; e < 4; ++e)
				empireOk[e] = false;
		}
	};

	// The call: the party drawn, the leader's shout, the answers due.
	bool CallPlayerBotDungeonRun(int def, BYTE empire, const std::vector<playerbot_dgrun::TCand>& bucket,
			const TPlayerBotDgRunSlot& slot, DWORD dwNow, const char* why)
	{
		const TPlayerBotDgRunDef& info = PLAYERBOT_DGRUN_DEFS[def];
		const playerbot_dgrun::TRunRule& rule = playerbot_dgrun::RuleFor(info.szKey);
		playerbot_conv::TRng rng = MakePlayerBotDgRunRng();
		const int want = playerbot_dgrun::PartyWant(rng, (int)bucket.size(), slot.partyMin, slot.partyMax, rule.preferMin);
		std::vector<playerbot_dgrun::TCand> picked;
		playerbot_dgrun::PickParty(bucket, (size_t)std::max(0, want), rng, picked);
		if (!playerbot_dgrun::PartyOk(picked, rule))
		{
			if (strcmp(why, "clock"))
				sys_log(0, "BOT_DGRUN: no call (%s) dungeon=%s - the party drawn (%u) falls short of its rule", why, info.szKey,
						(unsigned int)picked.size());
			return false;
		}
		LPCHARACTER leader = CHARACTER_MANAGER::instance().FindByPID(picked[0].pid);
		if (!leader)
			return false;
		TPlayerBotDgRun run;
		run.iId = s_iPlayerBotDgRunNextId++;
		run.iDef = def;
		run.bEmpire = empire;
		run.bPhase = DGRUN_PHASE_GATHER;
		run.dwLeader = picked[0].pid;
		run.place = slot.place[empire];
		if (info.bKind == DGRUN_KIND_TOWER)
		{
			run.place.map = PLAYERBOT_MAP_DEMON_TOWER;
			run.place.x = PLAYERBOT_TOWER_STONE_X;
			run.place.y = PLAYERBOT_TOWER_STONE_Y;
		}
		run.iLvMin = slot.lvMin;
		run.iLvMax = slot.lvMax;
		run.dwCalledAt = run.dwPhaseSince = dwNow;
		bool shaman = false;
		std::string names;
		for (size_t i = 0; i < picked.size(); ++i)
		{
			const DWORD pid = picked[i].pid;
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(pid);
			if (!c)
				continue;
			TPlayerBotDgRunBot& rb = s_mapPlayerBotDgRunBots[pid];
			rb = TPlayerBotDgRunBot();
			rb.iRun = run.iId;
			rb.dwAnswerAt = i == 0 ? dwNow
					: dwNow + PLAYERBOT_DGRUN_ANSWER_MIN_MS + (DWORD)number(0, (int)PLAYERBOT_DGRUN_ANSWER_SPREAD_MS);
			if (i == 0)
			{
				rb.bAnswered = true;
				rb.lHomeMap = c->GetMapIndex();
				rb.lHomeX = c->GetX();
				rb.lHomeY = c->GetY();
				rb.dwTravelAt = dwNow + 3000;
			}
			PlacePlayerBotDgRunSpot(run, run.members.size(), pid, rb.lSpotX, rb.lSpotY);
			run.members.push_back(pid);
			shaman = shaman || picked[i].IsHealer();
			if (names.size() < 200)
			{
				if (!names.empty())
					names += ",";
				names += c->GetName();
			}
		}
		playerbot_dgrun::TLine line = MakePlayerBotDgRunLine(leader, run);
		line.need = (int)picked.size() - 1;
		line.wantShaman = !shaman;
		ShoutPlayerBotDgRun(leader, playerbot_dgrun::CallLine(rng, line), "call");
		s_adwPlayerBotDgRunShoutAt[empire] = dwNow;
		++s_aPlayerBotDgRunStats[def].dwCalled;
		sys_log(0, "BOT_DGRUN: called run=%d dungeon=%s empire=%u levels=%d-%d candidates=%u party=%u shaman=%d gather=%ld(%ld,%ld) why=%s names=%s",
				run.iId, info.szKey, (unsigned int)empire, slot.lvMin, slot.lvMax, (unsigned int)bucket.size(),
				(unsigned int)run.members.size(), shaman ? 1 : 0, run.place.map, run.place.x, run.place.y, why, names.c_str());
		s_mapPlayerBotDgRuns[run.iId] = run;
		return true;
	}

	// One pass over this core's bots: who may go where, and one call when a
	// dungeon and a kingdom have enough of them (forced: that dungeon, or any
	// with -2, off the clock and the kingdom's shout gap).
	void PlanPlayerBotDungeonRun(DWORD dwNow, int forced, const char* why)
	{
		if (CountPlayerBotDgRuns(-1) >= GetPlayerBotDgRunCap())
		{
			if (forced != -1)
				sys_log(0, "BOT_DGRUN: no call (%s) - %d runs already, the cap", why, CountPlayerBotDgRuns(-1));
			return;
		}
		TPlayerBotDgRunSlot slots[sizeof(PLAYERBOT_DGRUN_DEFS) / sizeof(PLAYERBOT_DGRUN_DEFS[0])];
		bool any = false;
		for (int i = 0; i < PLAYERBOT_DGRUN_DEF_COUNT; ++i)
		{
			const TPlayerBotDgRunDef& def = PLAYERBOT_DGRUN_DEFS[i];
			if (forced >= 0 && forced != i)
				continue;
			const mt2009_dpanel::Def* panel = mt2009_dpanel::FindKey(def.szKey);
			if (!panel || mt2009_dpanel::Hidden(*panel) || !IsPlayerBotMapHostedHere(def.lMap) ||
					!SECTREE_MANAGER::instance().GetMap(def.lMap) || CountPlayerBotDgRuns(i) >= def.iCap)
				continue;
			// The Demon Tower keeps its own bot raids (playerbot_demon_tower.h;
			// the owner, 4 October: "wieza demonow tez niech zostanie przy swoich
			// rajdach botow, to juz dobrze dzialalo") - never a run of this pass.
			if (def.bKind == DGRUN_KIND_TOWER)
				continue;
			TPlayerBotDgRunSlot& slot = slots[i];
			// V2: the bots' own floor for a dungeon (RUN_RULES) - a player's
			// entry keeps the panel's level.
			slot.lvMin = playerbot_dgrun::BandMin(panel->lvMin, PLAYERBOT_DGRUN_MIN_LEVEL, playerbot_dgrun::RuleFor(def.szKey));
			slot.lvMax = playerbot_dgrun::BandMax(slot.lvMin, mt2009_dpanel::LevelMax(*panel));
			if (slot.lvMax < slot.lvMin)
				continue;
			slot.partyMin = panel->partyMin;
			slot.partyMax = panel->partyMax;
			slot.panel = panel;
			for (int e = 1; e <= 3; ++e)
				slot.empireOk[e] = def.bKind == DGRUN_KIND_TOWER ||
						ResolvePlayerBotLfgPlace(def.szKey, 0, e, slot.lvMin, slot.place[e]);
			slot.ok = slot.empireOk[1] || slot.empireOk[2] || slot.empireOk[3];
			any = any || slot.ok;
		}
		if (!any)
		{
			// A forced call says why nothing happened: the dungeon is hidden
			// (the Arezzo module off hides 363-366), not hosted here, at its cap,
			// or the tower.
			if (forced != -1)
				sys_log(0, "BOT_DGRUN: no call (%s) - no open dungeon (hidden by the panel/Arezzo switch, not hosted here, at its cap or the Demon Tower)", why);
			return;
		}
		std::vector<playerbot_dgrun::TCand> buckets[sizeof(PLAYERBOT_DGRUN_DEFS) / sizeof(PLAYERBOT_DGRUN_DEFS[0])][4];
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin(); it != s_mapPlayerBotAIStates.end(); ++it)
		{
			const DWORD pid = it->first;
			if (s_mapPlayerBotDgRunBots.count(pid))
				continue;
			std::map<DWORD, DWORD>::iterator rest = s_mapPlayerBotDgRunRest.find(pid);
			if (rest != s_mapPlayerBotDgRunRest.end())
			{
				if ((int)(rest->second - dwNow) > 0)
					continue;
				s_mapPlayerBotDgRunRest.erase(rest);
			}
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pid);
			if (!ch || ch->GetLevel() < PLAYERBOT_DGRUN_MIN_LEVEL)
				continue;
			const BYTE empire = ch->GetEmpire();
			if (empire < 1 || empire > 3)
				continue;
			const TPlayerBotAIState& state = it->second;
			if (!playerbot_dgrun::IsDungeonGoer(pid, GetPlayerBotDgRunShare(state)))
				continue;
			const int level = ch->GetLevel();
			bool fits = false;
			for (int i = 0; i < PLAYERBOT_DGRUN_DEF_COUNT && !fits; ++i)
				fits = slots[i].ok && slots[i].empireOk[empire] && playerbot_dgrun::InBand(level, slots[i].lvMin, slots[i].lvMax);
			if (!fits || GetPlayerBotDgRunRefusal(ch, state, dwNow))
				continue;
			int weight = 10;
			if (state.bPersonality == BOT_PERSONALITY_TEAM_COMPANION)
				weight += 6;
			if (state.bCurrentAction == BOT_ACTION_IDLE || state.bCurrentAction == BOT_ACTION_TRAVEL ||
					state.bCurrentAction == BOT_ACTION_TOWN_REST)
				weight += 4;
			if (ch->GetJob() == JOB_SHAMAN)
				weight += 2;
			for (int i = 0; i < PLAYERBOT_DGRUN_DEF_COUNT; ++i)
			{
				const TPlayerBotDgRunSlot& slot = slots[i];
				if (!slot.ok || !slot.empireOk[empire] || !playerbot_dgrun::InBand(level, slot.lvMin, slot.lvMax))
					continue;
				const TPlayerBotDgRunDef& def = PLAYERBOT_DGRUN_DEFS[i];
				if (slot.panel && mt2009_dpanel::CooldownLeft(ch, *slot.panel) > 0)
					continue;
				if (def.iDaily > 0 && GetPlayerBotDgRunRunsToday(ch, def) >= def.iDaily)
					continue;
				buckets[i][empire].push_back(playerbot_dgrun::TCand(pid, level, ch->GetJob(), weight,
						ch->GetJob() == JOB_SHAMAN && ch->GetSkillGroup() != 0));
			}
		}
		// A dungeon and a kingdom, by how many could go.
		std::vector<std::pair<int, int> > options;
		std::vector<int> weights;
		int total = 0;
		for (int i = 0; i < PLAYERBOT_DGRUN_DEF_COUNT; ++i)
			for (int e = 1; e <= 3; ++e)
			{
				const int n = (int)buckets[i][e].size();
				// V2: the dungeon's own minimum, and its Shaman when it wants one.
				if (!playerbot_dgrun::CanCall(buckets[i][e], playerbot_dgrun::RuleFor(PLAYERBOT_DGRUN_DEFS[i].szKey)))
					continue;
				if (forced == -1 && s_adwPlayerBotDgRunShoutAt[e] != 0 &&
						dwNow - s_adwPlayerBotDgRunShoutAt[e] < PLAYERBOT_DGRUN_SHOUT_GAP_MS)
					continue;
				options.push_back(std::make_pair(i, e));
				weights.push_back(std::min(n, 24));
				total += std::min(n, 24);
			}
		if (options.empty())
		{
			if (forced != -1)
				sys_log(0, "BOT_DGRUN: no call (%s) - no dungeon has the free bots of its band in one kingdom (%d, or its own rule's - Razador and Nemere: 6 and a Shaman)",
						why, playerbot_dgrun::CALL_MIN);
			return;
		}
		int roll = number(1, std::max(1, total));
		size_t chosen = 0;
		for (size_t k = 0; k < options.size(); ++k)
		{
			roll -= weights[k];
			if (roll <= 0)
			{
				chosen = k;
				break;
			}
		}
		const int def = options[chosen].first;
		const BYTE empire = (BYTE)options[chosen].second;
		CallPlayerBotDungeonRun(def, empire, buckets[def][empire], slots[def], dwNow, why);
	}

	// ------------------------------------------------------------ the operator

	int ParsePlayerBotDgRunKey(const char* key)
	{
		for (int i = 0; i < PLAYERBOT_DGRUN_DEF_COUNT; ++i)
			if (!strcmp(key, PLAYERBOT_DGRUN_DEFS[i].szKey))
				return i;
		return -1;
	}

	void LogPlayerBotDgRunStatus()
	{
		for (int i = 0; i < PLAYERBOT_DGRUN_DEF_COUNT; ++i)
		{
			const TPlayerBotDgRunStats& st = s_aPlayerBotDgRunStats[i];
			sys_log(0, "BOT_DGRUN: status dungeon=%s running=%d called=%u started=%u finished=%u abandoned=%u timeout=%u wipe=%u lost=%u disbanded=%u deaths=%u avg_s=%u",
					PLAYERBOT_DGRUN_DEFS[i].szKey, CountPlayerBotDgRuns(i), st.dwCalled, st.dwStarted, st.dwFinished,
					st.dwAbandoned, st.dwTimeout, st.dwWipe, st.dwLost, st.dwDisbanded, st.dwDeaths,
					st.dwFinished ? (unsigned int)(st.ullFinishedMs / st.dwFinished / 1000) : 0U);
		}
		sys_log(0, "BOT_DGRUN: status runs=%d cap=%d bots_in_runs=%u resting=%u on=%d", CountPlayerBotDgRuns(-1),
				GetPlayerBotDgRunCap(), (unsigned int)s_mapPlayerBotDgRunBots.size(),
				(unsigned int)s_mapPlayerBotDgRunRest.size(), IsPlayerBotDgRunOn(get_dword_time()) ? 1 : 0);
	}

	void RunPlayerBotDgRunTestFile(DWORD dwNow)
	{
		FILE* fp = fopen(PLAYERBOT_DGRUN_TEST_FILE, "r");
		if (!fp)
			return;
		std::vector<std::string> lines;
		char line[128];
		while (fgets(line, sizeof(line), fp))
			lines.push_back(line);
		fclose(fp);
		char done[128];
		snprintf(done, sizeof(done), "%s.done", PLAYERBOT_DGRUN_TEST_FILE);
		rename(PLAYERBOT_DGRUN_TEST_FILE, done);
		for (size_t l = 0; l < lines.size(); ++l)
		{
			char cmd[32] = "", arg[32] = "";
			const int got = sscanf(lines[l].c_str(), "%31s %31s", cmd, arg);
			if (got < 1 || cmd[0] == '#')
				continue;
			if (!strcmp(cmd, "status"))
				LogPlayerBotDgRunStatus();
			else if (!strcmp(cmd, "now"))
			{
				const int def = got >= 2 && strcmp(arg, "any") ? ParsePlayerBotDgRunKey(arg) : -2;
				if (def == -1)
					sys_log(0, "BOT_DGRUN: test now - no dungeon %s", arg);
				else
					PlanPlayerBotDungeonRun(dwNow, def, "test");
			}
			else if (!strcmp(cmd, "abort") && got >= 2)
			{
				const int def = strcmp(arg, "all") ? ParsePlayerBotDgRunKey(arg) : -2;
				for (std::map<int, TPlayerBotDgRun>::iterator r = s_mapPlayerBotDgRuns.begin(); r != s_mapPlayerBotDgRuns.end(); ++r)
				{
					if (def != -2 && r->second.iDef != def)
						continue;
					if (r->second.bPhase == DGRUN_PHASE_INSIDE && PLAYERBOT_DGRUN_DEFS[r->second.iDef].bKind != DGRUN_KIND_TOWER)
						PullPlayerBotDgRun(r->second, "aborted", dwNow);
					else
						r->second.szResult = "aborted";
				}
				sys_log(0, "BOT_DGRUN: test abort %s", arg);
			}
			else
				sys_log(0, "BOT_DGRUN: test unknown line=%s", lines[l].c_str());
		}
	}

	// ------------------------------------------------------------ the world's pass

	void ManagePlayerBotDungeonRuns(DWORD dwNow)
	{
		static DWORD s_dwNext = 0, s_dwStart = 0, s_dwNextPlan = 0, s_dwNextFile = 0, s_dwNextStatus = 0, s_dwNextPrune = 0;
		if (s_dwStart == 0)
		{
			s_dwStart = dwNow ? dwNow : 1;
			s_dwNextPlan = dwNow + PLAYERBOT_DGRUN_FIRST_PLAN_MS;
			s_dwNextStatus = dwNow + PLAYERBOT_DGRUN_STATUS_MS;
		}
		if (s_dwNext != 0 && (int)(dwNow - s_dwNext) < 0)
			return;
		s_dwNext = dwNow + 1000;
		if ((int)(dwNow - s_dwNextFile) >= 0)
		{
			s_dwNextFile = dwNow + 5000;
			RunPlayerBotDgRunTestFile(dwNow);
		}
		for (std::map<int, TPlayerBotDgRun>::iterator r = s_mapPlayerBotDgRuns.begin(); r != s_mapPlayerBotDgRuns.end();)
		{
			TPlayerBotDgRun& run = r->second;
			// An operator's abort before the way in: over now.
			if (run.szResult && !strcmp(run.szResult, "aborted") && run.bPhase != DGRUN_PHASE_INSIDE)
			{
				ClosePlayerBotDgRun(run, "aborted", dwNow);
				s_mapPlayerBotDgRuns.erase(r++);
				continue;
			}
			if (UpdatePlayerBotDgRun(run, dwNow))
				++r;
			else
				s_mapPlayerBotDgRuns.erase(r++);
		}
		if ((int)(dwNow - s_dwNextPrune) >= 0)
		{
			s_dwNextPrune = dwNow + 60000;
			// A bot whose run is gone is no longer one of its bots; rests over go.
			for (std::map<DWORD, TPlayerBotDgRunBot>::iterator b = s_mapPlayerBotDgRunBots.begin(); b != s_mapPlayerBotDgRunBots.end();)
			{
				if (!s_mapPlayerBotDgRuns.count(b->second.iRun))
					s_mapPlayerBotDgRunBots.erase(b++);
				else
					++b;
			}
			for (std::map<DWORD, DWORD>::iterator e = s_mapPlayerBotDgRunRest.begin(); e != s_mapPlayerBotDgRunRest.end();)
			{
				if ((int)(e->second - dwNow) <= 0)
					s_mapPlayerBotDgRunRest.erase(e++);
				else
					++e;
			}
		}
		if ((int)(dwNow - s_dwNextPlan) >= 0)
		{
			s_dwNextPlan = dwNow + PLAYERBOT_DGRUN_PLAN_MS;
			if (IsPlayerBotDgRunOn(dwNow) && number(1, 100) <= PLAYERBOT_DGRUN_PLAN_CHANCE)
				PlanPlayerBotDungeonRun(dwNow, -1, "clock");
		}
		if ((int)(dwNow - s_dwNextStatus) >= 0)
		{
			s_dwNextStatus = dwNow + PLAYERBOT_DGRUN_STATUS_MS;
			LogPlayerBotDgRunStatus();
		}
	}
}

#endif
