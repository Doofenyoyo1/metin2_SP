#ifndef __INC_METIN2_PLAYERBOT_DUNGEON_LFG_H__
#define __INC_METIN2_PLAYERBOT_DUNGEON_LFG_H__

// MT2009_PLUS_BOT_DUNGEON_LFG_V1 - the bots' dungeon finder (the engine half).
//
// "Robimy wyszukiwarke botow na dungi" (the owner, 4 October): a person
// writes in the chat that they are going to a dungeon and want company -
// "chce isc na dunga biblioteka", "ktos na biblioteke?", "szukam ekipy na
// smoka", "lf dung malpy" - and a few bots of that dungeon's level whisper
// them: "Czesc, moge isc z toba, mam 47 lvl, sura wp, moge przyjsc?". A yes
// ("tak", "jasne", "chodz", "dawaj") and the bot is at the entrance - the same
// map change the AI makes for every move (TransitionPlayerBotMap) - and says
// "Czekam pod wejsciem, bede tutaj 5 minut"; a no ("nie trzeba", "juz mam
// ekipe") and it says a short okay and lets it be.
//
//   - HandlePlayerBotDungeonLfgCall: a person's line on the normal chat, the
//     shout, the '@' trade chat or the guild chat (CPlayerBotManager's
//     OnPlayerShout / OnPlayerTradeChat / OnPlayerLocalChat). The words are
//     playerbot_dungeon_lfg_rules.h (ParseCall). A call is the finder's
//     whether or not a bot answers it: it is read before the trade and the
//     shout's questions, so "szukam ekipy na smoka" is never a bot selling a
//     dragon, and a trade line is never a call.
//   - which bots: one to three of the person's kingdom whose level is the
//     dungeon's to fifteen above it (a dungeon for 30: bots of 30-45), in no
//     business of their own that matters - see GetPlayerBotLfgRefusal - drawn
//     with a lean to the ones on the person's map and to the idle. Each whispers
//     a few seconds after the other (4-9 s, then 5-12 s more each), never the
//     same bot twice for one call, and a person's calls are answered once a
//     minute at most - the same dungeon again only once the offers lapsed.
//   - the offer goes out through the conversation engine (QueueBotLine) in
//     the bot's own hand, and the pair's memory keeps it (TConvMemory::lfg),
//     so the yes or the no whispered back is read as its answer
//     (ResolveContext) and answered by GenLfgAnswer, which asks this file to
//     act (CPlayerBotConvWorld's LfgAccept / LfgDecline / LfgChoose). An offer
//     not answered in three minutes lapses: a "tak" after that is only a
//     "tak". Several bots offered and every one told yes: every one comes.
//   - "kto na dunga?" names no dungeon: one bot of about the person's level
//     asks "na jaki dung?", and the dungeon named back is offered if it fits
//     the bot (LfgChoose) - "na X mam za maly lvl, sorki" if it does not.
//   - the wait (ManagePlayerBotDungeonLfgWait, the bot's tick, after the
//     summon): the bot stands at its own spot round the entrance, fights only
//     what attacks it, and is "held for company" (IsPlayerBotHeldForCompany),
//     which keeps the market, the travel, the town errands and the party
//     rotation off it. The person's party invitation is accepted as any is
//     (AcceptPlayerBotPartyInvite) and from then on the bot is a party bot
//     like any a person invited: the follow pass takes it along, and into
//     every dungeon with the person - the Demon Tower and the Catacomb by the
//     dungeon's own jump of the party (CDungeon::JumpParty through WarpBot),
//     the dungeons whose guard jumps the people alone (the Blue Dragon,
//     Razador, Nemere, the four Arezzo dungeons) by the party dungeon pass,
//     which puts it beside the person in the same instance, fights there and
//     brings it out (MT2009_PLUS_BOT_DUNGEONS_ALL_V1, playerbot_party_dungeon.h).
//     Five minutes with no party and it whispers "nie doczekalem sie, lece
//     dalej" and goes back to its life.
//
// Which dungeons: the panel's (dungeon_info.txt, read at runtime through
// mt2009_dpanel - so a Classic image, whose file has no Biblioteka, Wukong,
// Razador, Skorpion, Nemere, Smok or Dzungla rows, answers none of those, and
// the Arezzo rows go with the module's switch), and the open maps that left
// the panel: the three Monkey Dungeons (the person's kingdom's easy one, the
// shared medium and hard ones) and the Spider Dungeon, waited for at the
// desert's gate to it. Every dungeon of this world is on game1 with the bots
// since MT2009_PLUS_BOT_DUNGEONS_ALL_V1 (m2-render-config), so each is
// answered; the Jungle is waited for at the Temple of Ochao's gate, where the
// panel puts a person (a bot of its level gets in as the Guardian's warp puts
// one), and the person takes it through the temple and the Las to the guard.
// An entrance on a map this core does not host, or on an Arezzo map - where a
// bot on its own may only go by the operator's own routes - is answered by
// nobody.
//
// The FILE playerbot_lfg_off in the game core's directory switches the finder
// off (checked every 30 s, as the conversation's switches).
//
// An implementation fragment: include it once, after playerbot_chat_world.h
// (and so after the conversation, the companions, the raids and the cohorts
// whose business it asks about).

#include "playerbot_dungeon_lfg_rules.h"

namespace
{
	const DWORD PLAYERBOT_LFG_FIRST_DELAY_MIN_MS = 4000;
	const DWORD PLAYERBOT_LFG_FIRST_DELAY_SPREAD_MS = 5000;
	const DWORD PLAYERBOT_LFG_NEXT_DELAY_MIN_MS = 5000;
	const DWORD PLAYERBOT_LFG_NEXT_DELAY_SPREAD_MS = 7000;
	// One call of a person answered this often; the same dungeon again only
	// when the offers it brought have lapsed.
	const DWORD PLAYERBOT_LFG_PERSON_GAP_MS = 60 * 1000;
	const DWORD PLAYERBOT_LFG_SAME_CALL_GAP_MS = 4 * 60 * 1000;
	// The engine keeps an offer a little longer than the conversation does, so
	// a yes in the last second still finds it.
	const DWORD PLAYERBOT_LFG_OFFER_SLACK_MS = 20 * 1000;
	// A bot walking to the entrance on its own map gets the walk on top.
	const DWORD PLAYERBOT_LFG_WALK_EXTRA_MS = 90 * 1000;
	const int PLAYERBOT_LFG_LEVEL_SPAN = 15;
	const int PLAYERBOT_LFG_MAX_BOTS = 3;
	// "na jaki dung?": bots this far round the person's own level.
	const int PLAYERBOT_LFG_ASK_BELOW = 3;
	const int PLAYERBOT_LFG_ASK_ABOVE = 12;
	const int PLAYERBOT_LFG_ASK_MIN_LEVEL = 18;
	// Closer than this on the entrance's map: the bot walks, no teleport.
	const int PLAYERBOT_LFG_WALK_RANGE = 3000;
	const int PLAYERBOT_LFG_SPOT_RADIUS = 260;
	const int PLAYERBOT_LFG_STAY_DISTANCE = 320;
	const int PLAYERBOT_LFG_GUARD_RANGE = 1200;
	const DWORD PLAYERBOT_LFG_GUARD_SCAN_MS = 700;
	const DWORD PLAYERBOT_LFG_SWITCH_CHECK_MS = 30 * 1000;
	const DWORD PLAYERBOT_LFG_CALL_PRUNE_MS = 10 * 60 * 1000;
	const int PLAYERBOT_LFG_MIN_HP_PERCENT = 50;

	enum EPlayerBotLfgPhase
	{
		LFG_PHASE_DUE = 1,   // picked, the whisper not out yet
		LFG_PHASE_OFFERED,   // "moge przyjsc?"
		LFG_PHASE_ASKED,     // "na jaki dung?"
		LFG_PHASE_WAITING    // at the entrance
	};

	enum EPlayerBotLfgSource
	{
		LFG_SOURCE_TALK = 0,
		LFG_SOURCE_SHOUT,
		LFG_SOURCE_TRADE,
		LFG_SOURCE_GUILD
	};

	inline const char* PlayerBotLfgSourceName(int source)
	{
		switch (source)
		{
			case LFG_SOURCE_TALK: return "talk";
			case LFG_SOURCE_SHOUT: return "shout";
			case LFG_SOURCE_TRADE: return "trade";
			case LFG_SOURCE_GUILD: return "guild";
			default: return "?";
		}
	}

	// Where a dungeon is entered, for whom.
	struct TPlayerBotLfgPlace
	{
		std::string key;
		int lvMin;
		int lvMax;
		long map;
		long x;
		long y;
		TPlayerBotLfgPlace() : lvMin(0), lvMax(0), map(0), x(0), y(0) {}
	};

	// One bot's part in one person's call.
	struct TPlayerBotLfg
	{
		DWORD personPID;
		std::string personName;
		BYTE personEmpire;
		int personLevel;
		BYTE phase;
		BYTE source;
		bool ask;          // "na jaki dung?" rather than the offer
		bool walking;
		TPlayerBotLfgPlace place;
		DWORD dueAt;
		DWORD offeredAt;
		DWORD acceptedAt;
		DWORD waitUntil;
		long spotX;
		long spotY;
		DWORD guardVID;
		DWORD nextGuardScanAt;
		TPlayerBotLfg() : personPID(0), personEmpire(0), personLevel(0), phase(LFG_PHASE_DUE), source(0), ask(false),
			walking(false), dueAt(0), offeredAt(0), acceptedAt(0), waitUntil(0), spotX(0), spotY(0), guardVID(0),
			nextGuardScanAt(0) {}
	};

	struct TPlayerBotLfgCall
	{
		DWORD at;
		std::string key;
		TPlayerBotLfgCall() : at(0) {}
	};

	std::map<DWORD, TPlayerBotLfg> s_mapPlayerBotLfg;           // by bot pid
	std::map<DWORD, TPlayerBotLfgCall> s_mapPlayerBotLfgCalls;  // by person pid: the last call answered
	bool s_bPlayerBotLfgOff = false;
	DWORD s_dwPlayerBotLfgSwitchAt = 0;
	DWORD s_dwPlayerBotLfgPruneAt = 0;
	unsigned int s_uPlayerBotLfgCalls = 0;
	unsigned int s_uPlayerBotLfgOffers = 0;
	unsigned int s_uPlayerBotLfgAccepted = 0;

	bool IsPlayerBotDungeonLfgOn(DWORD dwNow)
	{
		if (s_dwPlayerBotLfgSwitchAt == 0 || dwNow - s_dwPlayerBotLfgSwitchAt >= PLAYERBOT_LFG_SWITCH_CHECK_MS)
		{
			s_dwPlayerBotLfgSwitchAt = dwNow ? dwNow : 1;
			const bool off = PlayerBotConvFlagFile("playerbot_lfg_off");
			if (off != s_bPlayerBotLfgOff)
				sys_log(0, "PLAYERBOT_LFG: %s", off ? "off (playerbot_lfg_off)" : "on");
			s_bPlayerBotLfgOff = off;
		}
		return !s_bPlayerBotLfgOff;
	}

	// Waiting at an entrance: the passes that would take a bot away ask this
	// through IsPlayerBotHeldForCompany (playerbot_companions.h), and the party
	// rotation directly (ManagePlayerBotParty).
	bool IsPlayerBotDungeonLfgHeld(DWORD botPID)
	{
		std::map<DWORD, TPlayerBotLfg>::const_iterator it = s_mapPlayerBotLfg.find(botPID);
		return it != s_mapPlayerBotLfg.end() && it->second.phase == LFG_PHASE_WAITING;
	}

	// The maps a bot reaches only by the operator's own routes.
	// MT2009_PLUS_BOT_DUNGEONS_ALL_V1: no longer the Temple of Ochao - the
	// Jungle's entrance; a bot is put at its gate (RoutePlayerBotOchaoTransition,
	// "dungeon_lfg", from the temple's level - the Jungle's own).
	bool IsPlayerBotLfgClosedMap(long mapIndex)
	{
		return IsPlayerBotArezzoMap(mapIndex);
	}

	// The dungeon a key names, for a person of this kingdom and level. False
	// when this world has no such dungeon now, or this core cannot send a bot
	// to its entrance.
	bool ResolvePlayerBotLfgPlace(const std::string& key, int difficulty, int empire, int personLevel,
			TPlayerBotLfgPlace& out)
	{
		out = TPlayerBotLfgPlace();
		std::string k = key;
		if (k == "malpy")
		{
			int d = difficulty;
			if (d == 0)
				d = personLevel >= PLAYERBOT_MONKEY_HARD_MIN_LEVEL ? 3 : personLevel >= PLAYERBOT_MONKEY_MEDIUM_MIN_LEVEL ? 2 : 1;
			k = d == 3 ? "malpy3" : d == 2 ? "malpy2" : "malpy1";
		}
		out.key = k;
		int levelCap = gPlayerMaxLevel;
		if (k == "malpy1" || k == "malpy2" || k == "malpy3")
		{
			if (k == "malpy1")
			{
				out.map = playerbot_empire_rules::GetMonkeyEasyMap(empire >= 1 && empire <= 3 ? empire : 1);
				out.lvMin = PLAYERBOT_MONKEY_MIN_LEVEL;
			}
			else if (k == "malpy2")
			{
				out.map = PLAYERBOT_MAP_MONKEY_MEDIUM;
				out.lvMin = PLAYERBOT_MONKEY_MEDIUM_MIN_LEVEL;
			}
			else
			{
				out.map = PLAYERBOT_MAP_MONKEY_HARD;
				out.lvMin = PLAYERBOT_MONKEY_HARD_MIN_LEVEL;
			}
			if (!GetPlayerBotMonkeyArrival(out.map, out.x, out.y))
				return false;
		}
		else if (k == "pajaki" || k == "pajaki2")
		{
			// The Spider Dungeon is entered across the desert, through the
			// gate the bots' own crossing uses - waited for a few steps off
			// it, where a bot coming out of the dungeon stands, and never on
			// the gate's warp.
			out.map = PLAYERBOT_MAP_DESERT;
			out.x = PLAYERBOT_DESERT_FROM_V1_X;
			out.y = PLAYERBOT_DESERT_FROM_V1_Y;
			out.lvMin = playerbot_progression::MapFrom(k == "pajaki2" ? playerbot_progression::MAP_SPIDER2
					: playerbot_progression::MAP_SPIDER1);
		}
		else
		{
			const mt2009_dpanel::Def* d = mt2009_dpanel::FindKey(k.c_str());
			if (!d || mt2009_dpanel::Hidden(*d))
				return false;
			const int e = empire >= 1 && empire <= 3 ? empire - 1 : 0;
			out.map = d->entryMaps[e];
			out.lvMin = d->lvMin;
			levelCap = mt2009_dpanel::LevelMax(*d);
			if (d->town)
			{
				PIXEL_POSITION pos;
				if (!SECTREE_MANAGER::instance().GetRecallPositionByEmpire(out.map, (BYTE)(e + 1), pos))
					return false;
				out.x = pos.x;
				out.y = pos.y;
			}
			else if (d->empireXY)
			{
				out.x = d->ex[e] * 100;
				out.y = d->ey[e] * 100;
			}
			else
			{
				out.x = d->x * 100;
				out.y = d->y * 100;
			}
		}
		out.lvMax = std::min(out.lvMin + PLAYERBOT_LFG_LEVEL_SPAN, levelCap);
		if (out.map == 0 || out.lvMin <= 0 || IsPlayerBotLfgClosedMap(out.map) || !IsPlayerBotMapHostedHere(out.map))
			return false;
		return true;
	}

	// Why this bot may not answer a call now; NULL when it may. Everything
	// here is a claim on the bot that matters more than a person's dungeon:
	// a person it already serves, a raid, a war, the operator's cohorts, its
	// counter, its trade - and the bots that never answer anybody.
	const char* GetPlayerBotLfgRefusal(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead() || !ch->GetDesc() || !ch->GetDesc()->IsBot() || !ch->GetSectree())
			return "absent";
		if (ch->GetGMLevel() > GM_PLAYER)
			return "gm";
		const DWORD pid = ch->GetPlayerID();
		if (IsPlayerBotShouterPID(pid) || IsPlayerBotMedalShouterPID(pid))
			return "shouter";
		if (IsPlayerBotSidekickPID(pid))
			return "companion";
		if (IsPlayerBotOnMercContract(pid) || IsPlayerBotHiredClient(pid))
			return "mercenary";
		if (IsPlayerBotSummoned(pid) || state.dwLurePlayerPID != 0 || IsPlayerBotHeldForCompany(ch))
			return "with_person";
		if (IsPlayerBotRetiring(pid))
			return "retiring";
		if (IsPlayerBotArezzoCohortPID(pid) || IsPlayerBotArezzoDungeonCohortPID(pid) ||
				IsPlayerBotArezzoDungeonReservedPID(pid) || IsPlayerBotArezzoBound(ch) ||
				// MT2009_PLUS_BOT_DUNGEONS_ALL_V1: the Temple of Ochao's test group.
				// IsPlayerBotOchaoBot answered yes for every bot (it asks only
				// whether the character is a bot), so no bot ever answered a call.
				IsPlayerBotOchaoForced(ch))
			return "cohort";
		const long mapIndex = ch->GetMapIndex();
		if (mapIndex >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN || IsPlayerBotLfgClosedMap(mapIndex) ||
				IsPlayerBotSpiderMap(mapIndex) || state.lDesertCrossingTo != 0)
			return "far_map";
		if (IsPlayerBotInDungeonBusiness(ch, state) || state.wBossRaidRace != 0 || state.bWorldEventKind != 0 ||
				IsPlayerBotCatacombRaider(pid))
			return "raid";
		if (state.dwGuildWarEnemyGID != 0 || playerbot_pvp::IsInDuel(pid, dwNow))
			return "fight";
		if (ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()))
			return "party";
		if (ch->GetMyShop() || state.bFishingSession || state.bIsFishing || IsPlayerBotMiningNow(pid, dwNow))
			return "busy";
		switch (state.bCurrentAction)
		{
			case BOT_ACTION_STALL: case BOT_ACTION_LURE: case BOT_ACTION_FISHING: case BOT_ACTION_MINING:
			case BOT_ACTION_MARKET:
				return "busy";
			default:
				break;
		}
		if (IsPlayerBotDropper(state.bPersonality) || IsPlayerBotL30DropperAtWork(ch))
			return "dropper";
		if (state.bMultiPullActive || state.bTacticalRetreat || state.bRecoveringAfterDeath)
			return "fight";
		if (state.persona.dwAfkUntil != 0 && dwNow < state.persona.dwAfkUntil)
			return "afk";
		if (ch->GetMaxHP() > 0 && (long long)ch->GetHP() * 100 / ch->GetMaxHP() < PLAYERBOT_LFG_MIN_HP_PERCENT)
			return "hurt";
		// A Metin or a boss half broken is not left for a whisper.
		if (state.dwTargetVID != 0)
		{
			LPCHARACTER target = CHARACTER_MANAGER::instance().Find(state.dwTargetVID);
			if (target && !target->IsDead() && (target->IsStone() ||
					(target->IsMonster() && target->GetMobRank() >= MOB_RANK_BOSS)))
				return "fight";
		}
		return NULL;
	}

	// The person's part of the conversation's memory with this bot.
	playerbot_lfg::TTalk* GetPlayerBotLfgTalk(DWORD personPID, DWORD botPID)
	{
		playerbot_conv::TConvPair* pair = s_PlayerBotConvEngine.FindPair(personPID, botPID);
		return pair ? &pair->mem.lfg : NULL;
	}

	void ForgetPlayerBotLfgTalk(DWORD personPID, DWORD botPID)
	{
		playerbot_lfg::TTalk* talk = GetPlayerBotLfgTalk(personPID, botPID);
		if (talk)
			*talk = playerbot_lfg::TTalk();
	}

	// A line of the finder's, through the conversation engine: the bot's own
	// hand and pace, and the pair's memory of having said it.
	bool SayPlayerBotLfgLine(LPCHARACTER bot, DWORD personPID, const std::string& text, DWORD delayMs)
	{
		if (!bot || text.empty())
			return false;
		playerbot_conv::TBotSnapshot snap;
		if (!s_PlayerBotConvHost.BuildSnapshot(personPID, bot->GetPlayerID(), snap))
			return false;
		s_PlayerBotConvEngine.QueueBotLine(s_PlayerBotConvHost, personPID, bot->GetPlayerID(), text, "",
				playerbot_conv::I_LFG_ANSWER, snap, get_dword_time(), delayMs);
		EnsurePlayerBotConvTimer();
		return true;
	}

	playerbot_lfg::TFacts MakePlayerBotLfgFacts(LPCHARACTER bot, const TPlayerBotLfg& e)
	{
		playerbot_lfg::TFacts f;
		f.level = bot ? bot->GetLevel() : 0;
		f.job = bot ? bot->GetJob() : 0;
		f.group = bot ? bot->GetSkillGroup() : 0;
		f.botName = bot ? bot->GetName() : "";
		f.playerName = e.personName;
		f.key = e.place.key;
		return f;
	}

	playerbot_conv::TRng MakePlayerBotLfgRng()
	{
		return playerbot_conv::TRng((playerbot_conv::u32)number(1, 0x7ffffffe) ^ get_dword_time());
	}

	// What a line of the finder's is: which of its words to say.
	enum EPlayerBotLfgLine
	{
		LFG_LINE_NONE = 0,
		LFG_LINE_TIMEOUT,
		LFG_LINE_JOINED,
		LFG_LINE_CALLED_AWAY
	};

	// The end of a bot's part, whatever ended it. A bot that waited goes back
	// to its own life: what it was doing before kept its flags and resumes,
	// only the walk and the guard's target are dropped.
	void EndPlayerBotDungeonLfg(DWORD botPID, const char* reason, int line)
	{
		std::map<DWORD, TPlayerBotLfg>::iterator it = s_mapPlayerBotLfg.find(botPID);
		if (it == s_mapPlayerBotLfg.end())
			return;
		const TPlayerBotLfg e = it->second;
		s_mapPlayerBotLfg.erase(it);
		ForgetPlayerBotLfgTalk(e.personPID, botPID);
		const DWORD dwNow = get_dword_time();
		LPCHARACTER bot = CHARACTER_MANAGER::instance().FindByPID(botPID);
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(botPID);
		if (bot && st != s_mapPlayerBotAIStates.end() && e.phase == LFG_PHASE_WAITING)
		{
			TPlayerBotAIState& state = st->second;
			if (e.guardVID != 0 && state.dwTargetVID == e.guardVID)
			{
				state.dwTargetVID = 0;
				bot->SetVictim(NULL);
			}
			ClearPlayerBotRoute(state, true);
			state.dwLastMeaningfulActivityTime = dwNow;
			SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		}
		sys_log(0, "PLAYERBOT_LFG: over pid=%u name=%s person=%s key=%s phase=%u reason=%s waited_s=%u",
				botPID, bot ? bot->GetName() : "?", e.personName.c_str(), e.place.key.c_str(), (unsigned int)e.phase,
				reason ? reason : "?", e.acceptedAt ? (dwNow - e.acceptedAt) / 1000 : 0);
		if (!bot || line == LFG_LINE_NONE)
			return;
		playerbot_conv::TRng rng = MakePlayerBotLfgRng();
		const playerbot_lfg::TFacts f = MakePlayerBotLfgFacts(bot, e);
		std::string text;
		switch (line)
		{
			case LFG_LINE_TIMEOUT: text = playerbot_lfg::TimeoutLine(rng, f); break;
			case LFG_LINE_JOINED: text = playerbot_lfg::JoinedLine(rng, f); break;
			case LFG_LINE_CALLED_AWAY: text = playerbot_lfg::CalledAwayLine(rng, f); break;
			default: break;
		}
		SayPlayerBotLfgLine(bot, e.personPID, text, 300 + number(0, 900));
	}

	// The bot's own spot round the entrance: eight of them by pid, so three
	// bots do not stand in one another, on a cell the map lets it stand on.
	void PlacePlayerBotLfgSpot(LPCHARACTER bot, TPlayerBotLfg& e)
	{
		static const int kSide[8][2] = {
			{ 200, 0 }, { 141, 141 }, { 0, 200 }, { -141, 141 }, { -200, 0 }, { -141, -141 }, { 0, -200 }, { 141, -141 } };
		const int* side = kSide[bot->GetPlayerID() % 8];
		e.spotX = e.place.x + side[0] * PLAYERBOT_LFG_SPOT_RADIUS / 200;
		e.spotY = e.place.y + side[1] * PLAYERBOT_LFG_SPOT_RADIUS / 200;
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(e.place.map);
		PIXEL_POSITION safe;
		if (navigation.Init(e.place.map) &&
				navigation.FindNearestWalkableWorld(e.spotX, e.spotY, 12, safe, bot->GetPlayerID()))
		{
			e.spotX = safe.x;
			e.spotY = safe.y;
		}
	}

	// ------------------------------------------------------------ the call

	struct TPlayerBotLfgCandidate
	{
		DWORD pid;
		int weight;
	};

	// A person's call, from any of the chats. True when the line was a call
	// (answered or not): the trade and the shout's questions leave it alone.
	bool HandlePlayerBotDungeonLfgCall(LPCHARACTER person, const char* text, int source)
	{
		if (!IsPlayerBotPersonCharacter(person) || !text || !*text)
			return false;
		const DWORD dwNow = get_dword_time();
		if (!IsPlayerBotDungeonLfgOn(dwNow))
			return false;
		playerbot_lfg::TCall call;
		if (!playerbot_lfg::ParseCall(text, call))
			return false;
		const DWORD personPID = person->GetPlayerID();
		const int personLevel = person->GetLevel();
		const BYTE empire = person->GetEmpire();
		// A person in a dungeon is not asking for company at its door.
		if (person->GetMapIndex() >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN)
			return true;

		TPlayerBotLfgPlace place;
		if (call.kind == playerbot_lfg::CALL_DUNGEON &&
				!ResolvePlayerBotLfgPlace(call.key, call.difficulty, empire, personLevel, place))
		{
			sys_log(0, "PLAYERBOT_LFG: call from=%s source=%s key=%s - no such dungeon here", person->GetName(),
					PlayerBotLfgSourceName(source), call.key.c_str());
			return true;
		}
		const std::string key = call.kind == playerbot_lfg::CALL_DUNGEON ? place.key : std::string();

		// The person's calls: once a minute, and the same dungeon again only
		// when none of its offers is still open.
		{
			std::map<DWORD, TPlayerBotLfgCall>::const_iterator last = s_mapPlayerBotLfgCalls.find(personPID);
			if (last != s_mapPlayerBotLfgCalls.end())
			{
				if (dwNow - last->second.at < PLAYERBOT_LFG_PERSON_GAP_MS)
					return true;
				if (last->second.key == key && dwNow - last->second.at < PLAYERBOT_LFG_SAME_CALL_GAP_MS)
					for (std::map<DWORD, TPlayerBotLfg>::const_iterator it = s_mapPlayerBotLfg.begin();
							it != s_mapPlayerBotLfg.end(); ++it)
						if (it->second.personPID == personPID && it->second.place.key == key)
							return true;
			}
		}

		// The bots that may answer, weighted.
		std::vector<TPlayerBotLfgCandidate> candidates;
		CGuild* guild = person->GetGuild();
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin(); it != s_mapPlayerBotAIStates.end(); ++it)
		{
			if (s_mapPlayerBotLfg.find(it->first) != s_mapPlayerBotLfg.end())
				continue;
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!c || c == person || c->GetEmpire() != empire)
				continue;
			const int level = c->GetLevel();
			if (call.kind == playerbot_lfg::CALL_DUNGEON)
			{
				if (level < place.lvMin || level > place.lvMax)
					continue;
			}
			else if (level < std::max(PLAYERBOT_LFG_ASK_MIN_LEVEL, personLevel - PLAYERBOT_LFG_ASK_BELOW) ||
					level > personLevel + PLAYERBOT_LFG_ASK_ABOVE)
				continue;
			// The guild's chat is heard by the guild alone.
			if (source == LFG_SOURCE_GUILD && (!guild || c->GetGuild() != guild))
				continue;
			const TPlayerBotAIState& state = it->second;
			if (GetPlayerBotLfgRefusal(c, state, dwNow))
				continue;
			TPlayerBotLfgCandidate cand;
			cand.pid = it->first;
			cand.weight = 10;
			// The ones who heard it best, and the ones with nothing on.
			if (c->GetMapIndex() == person->GetMapIndex())
				cand.weight += source == LFG_SOURCE_TALK ? 40 : 10;
			if (call.kind == playerbot_lfg::CALL_DUNGEON && c->GetMapIndex() == place.map)
				cand.weight += 6;
			if (state.bCurrentAction == BOT_ACTION_IDLE || state.bCurrentAction == BOT_ACTION_TRAVEL ||
					state.bCurrentAction == BOT_ACTION_TOWN_REST)
				cand.weight += 6;
			if (GetPlayerBotAffinity(state, personPID) > 0)
				cand.weight += 8;
			if (MapPlayerBotConvStyle(state) == playerbot_conv::S_COMPANION)
				cand.weight += 6;
			candidates.push_back(cand);
		}

		int wanted = 1;
		if (call.kind == playerbot_lfg::CALL_DUNGEON)
		{
			const int roll = number(1, 100);
			wanted = roll <= 45 ? 1 : roll <= 80 ? 2 : PLAYERBOT_LFG_MAX_BOTS;
		}
		DWORD due = dwNow + PLAYERBOT_LFG_FIRST_DELAY_MIN_MS + number(0, (int)PLAYERBOT_LFG_FIRST_DELAY_SPREAD_MS);
		int picked = 0;
		std::string names;
		while (picked < wanted && !candidates.empty())
		{
			int total = 0;
			for (size_t i = 0; i < candidates.size(); ++i)
				total += candidates[i].weight;
			int roll = number(1, std::max(1, total));
			size_t chosen = 0;
			for (size_t i = 0; i < candidates.size(); ++i)
			{
				roll -= candidates[i].weight;
				if (roll <= 0)
				{
					chosen = i;
					break;
				}
			}
			const DWORD pid = candidates[chosen].pid;
			candidates.erase(candidates.begin() + chosen);
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(pid);
			if (!c)
				continue;
			TPlayerBotLfg& e = s_mapPlayerBotLfg[pid];
			e = TPlayerBotLfg();
			e.personPID = personPID;
			e.personName = person->GetName();
			e.personEmpire = empire;
			e.personLevel = personLevel;
			e.phase = LFG_PHASE_DUE;
			e.source = (BYTE)source;
			e.ask = call.kind != playerbot_lfg::CALL_DUNGEON;
			e.place = place;
			e.dueAt = due;
			due += PLAYERBOT_LFG_NEXT_DELAY_MIN_MS + number(0, (int)PLAYERBOT_LFG_NEXT_DELAY_SPREAD_MS);
			++picked;
			if (!names.empty())
				names += ",";
			names += c->GetName();
		}
		TPlayerBotLfgCall& noted = s_mapPlayerBotLfgCalls[personPID];
		noted.at = dwNow;
		noted.key = key;
		++s_uPlayerBotLfgCalls;
		sys_log(0, "PLAYERBOT_LFG: call from=%s level=%d source=%s key=%s levels=%d-%d map=%ld candidates=%u picked=%d bots=%s text=\"%s\"",
				person->GetName(), personLevel, PlayerBotLfgSourceName(source), key.empty() ? "?" : key.c_str(),
				place.lvMin, place.lvMax, place.map, (unsigned int)(candidates.size() + picked), picked, names.c_str(), text);
		return true;
	}

	// --------------------------------------------------------- the answers

	// The yes (IConvWorld::LfgAccept): to the entrance - by the map change
	// every move of the AI is, or on foot when it is already near - and the
	// wait begins.
	int AcceptPlayerBotDungeonLfg(LPCHARACTER bot, DWORD personPID)
	{
		if (!bot)
			return playerbot_lfg::GO_GONE;
		std::map<DWORD, TPlayerBotLfg>::iterator it = s_mapPlayerBotLfg.find(bot->GetPlayerID());
		if (it == s_mapPlayerBotLfg.end() || it->second.personPID != personPID)
			return playerbot_lfg::GO_GONE;
		TPlayerBotLfg& e = it->second;
		if (e.phase == LFG_PHASE_WAITING)
			return playerbot_lfg::GO_ALREADY;
		if (e.phase != LFG_PHASE_OFFERED)
			return playerbot_lfg::GO_GONE;
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(bot->GetPlayerID());
		const DWORD dwNow = get_dword_time();
		if (st == s_mapPlayerBotAIStates.end())
		{
			EndPlayerBotDungeonLfg(bot->GetPlayerID(), "no_state", LFG_LINE_NONE);
			return playerbot_lfg::GO_GONE;
		}
		TPlayerBotAIState& state = st->second;
		// Whatever has claimed the bot since the offer still has it.
		const char* refusal = GetPlayerBotLfgRefusal(bot, state, dwNow);
		if (refusal)
		{
			sys_log(0, "PLAYERBOT_LFG: yes refused pid=%u name=%s person=%s key=%s reason=%s", bot->GetPlayerID(),
					bot->GetName(), e.personName.c_str(), e.place.key.c_str(), refusal);
			EndPlayerBotDungeonLfg(bot->GetPlayerID(), refusal, LFG_LINE_NONE);
			return playerbot_lfg::GO_FAILED;
		}
		PlacePlayerBotLfgSpot(bot, e);
		const bool sameMap = bot->GetMapIndex() == e.place.map;
		const int distance = sameMap ? DISTANCE_APPROX(bot->GetX() - e.spotX, bot->GetY() - e.spotY) : INT_MAX;
		state.dwTargetVID = 0;
		bot->SetVictim(NULL);
		ClearPlayerBotRoute(state, true);
		if (distance > PLAYERBOT_LFG_WALK_RANGE)
		{
			if (!TransitionPlayerBotMap(bot, state, e.place.map, e.spotX, e.spotY, dwNow, "dungeon_lfg") ||
					bot->GetMapIndex() != e.place.map)
			{
				sys_log(0, "PLAYERBOT_LFG: move refused pid=%u name=%s to_map=%ld", bot->GetPlayerID(), bot->GetName(),
						e.place.map);
				EndPlayerBotDungeonLfg(bot->GetPlayerID(), "move_refused", LFG_LINE_NONE);
				return playerbot_lfg::GO_FAILED;
			}
			e.walking = false;
		}
		else
			e.walking = true;
		e.phase = LFG_PHASE_WAITING;
		e.acceptedAt = dwNow;
		e.waitUntil = dwNow + playerbot_lfg::WAIT_MS + (e.walking ? PLAYERBOT_LFG_WALK_EXTRA_MS : 0);
		state.dwLastMeaningfulActivityTime = dwNow;
		SetPlayerBotAction(state, e.walking ? BOT_ACTION_TRAVEL : BOT_ACTION_IDLE, dwNow);
		++s_uPlayerBotLfgAccepted;
		sys_log(0, "PLAYERBOT_LFG: yes pid=%u name=%s level=%u person=%s key=%s map=%ld spot=(%ld,%ld) by=%s",
				bot->GetPlayerID(), bot->GetName(), bot->GetLevel(), e.personName.c_str(), e.place.key.c_str(),
				e.place.map, e.spotX, e.spotY, e.walking ? "walk" : "teleport");
		return e.walking ? playerbot_lfg::GO_WALKING : playerbot_lfg::GO_TELEPORTED;
	}

	// The no (IConvWorld::LfgDecline), to the offer or while the bot waits.
	void DeclinePlayerBotDungeonLfg(LPCHARACTER bot, DWORD personPID)
	{
		if (!bot)
			return;
		std::map<DWORD, TPlayerBotLfg>::iterator it = s_mapPlayerBotLfg.find(bot->GetPlayerID());
		if (it == s_mapPlayerBotLfg.end() || it->second.personPID != personPID)
			return;
		EndPlayerBotDungeonLfg(bot->GetPlayerID(), "declined", LFG_LINE_NONE);
	}

	// The dungeon named to "na jaki dung?" (IConvWorld::LfgChoose): whether
	// it fits the bot, and if it does the question becomes the offer.
	int ChoosePlayerBotDungeonLfg(LPCHARACTER bot, DWORD personPID, const std::string& key, int difficulty,
			std::string& resolved)
	{
		resolved.clear();
		if (!bot)
			return playerbot_lfg::CHOOSE_UNKNOWN;
		std::map<DWORD, TPlayerBotLfg>::iterator it = s_mapPlayerBotLfg.find(bot->GetPlayerID());
		if (it == s_mapPlayerBotLfg.end() || it->second.personPID != personPID || it->second.phase != LFG_PHASE_ASKED)
			return playerbot_lfg::CHOOSE_UNKNOWN;
		TPlayerBotLfg& e = it->second;
		TPlayerBotLfgPlace place;
		if (!ResolvePlayerBotLfgPlace(key, difficulty, e.personEmpire, e.personLevel, place))
		{
			resolved = key == "malpy" ? std::string("malpy1") : key;
			EndPlayerBotDungeonLfg(bot->GetPlayerID(), "chosen_unknown", LFG_LINE_NONE);
			return playerbot_lfg::CHOOSE_UNKNOWN;
		}
		resolved = place.key;
		const int level = bot->GetLevel();
		if (level < place.lvMin || level > place.lvMax)
		{
			EndPlayerBotDungeonLfg(bot->GetPlayerID(), level < place.lvMin ? "chosen_low" : "chosen_high", LFG_LINE_NONE);
			return level < place.lvMin ? playerbot_lfg::CHOOSE_LOW : playerbot_lfg::CHOOSE_HIGH;
		}
		e.place = place;
		e.ask = false;
		e.phase = LFG_PHASE_OFFERED;
		e.offeredAt = get_dword_time();
		sys_log(0, "PLAYERBOT_LFG: chosen pid=%u name=%s person=%s key=%s", bot->GetPlayerID(), bot->GetName(),
				e.personName.c_str(), place.key.c_str());
		return playerbot_lfg::CHOOSE_OK;
	}

	// A whisper from the person to a bot that offered or waits: a yes, a no,
	// or the dungeon named goes straight to the conversation, whose memory
	// knows the offer - ahead of the trade, the lure order and the rest, whose
	// words ("stop", "dosc") a short answer may share.
	bool HandlePlayerBotDungeonLfgWhisper(DWORD personPID, const char* personName, LPCHARACTER bot, const char* text)
	{
		if (!personPID || !bot || !text || !*text)
			return false;
		std::map<DWORD, TPlayerBotLfg>::const_iterator it = s_mapPlayerBotLfg.find(bot->GetPlayerID());
		if (it == s_mapPlayerBotLfg.end() || it->second.personPID != personPID || it->second.phase == LFG_PHASE_DUE)
			return false;
		playerbot_conv::TTokens t;
		playerbot_conv::Normalize(text, t);
		int difficulty = 0;
		const bool named = it->second.phase == LFG_PHASE_ASKED && !playerbot_lfg::FindDungeonKey(t, difficulty).empty();
		if (!named && playerbot_lfg::ParseYesNo(t) == 0)
			return false;
		return HandlePlayerBotConversationWith(personPID, personName, bot, text);
	}

	// ------------------------------------------------------------ the wait

	struct FPlayerBotLfgThreat
	{
		LPCHARACTER m_bot;
		LPCHARACTER m_best;
		int m_bestDistance;
		explicit FPlayerBotLfgThreat(LPCHARACTER bot) : m_bot(bot), m_best(NULL), m_bestDistance(INT_MAX) {}
		void operator () (LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER mob = static_cast<LPCHARACTER>(ent);
			if (!mob->IsMonster() || mob->IsDead() || mob->GetVictim() != m_bot)
				return;
			const int d = DISTANCE_APPROX(mob->GetX() - m_bot->GetX(), mob->GetY() - m_bot->GetY());
			if (d > PLAYERBOT_LFG_GUARD_RANGE || d >= m_bestDistance)
				return;
			m_best = mob;
			m_bestDistance = d;
		}
	};

	// The bot's part of the tick while it waits at an entrance, right after
	// the summon's (playerbot_manager.cpp). It claims the tick: everything
	// below would take the bot somewhere of its own. What it does for itself
	// is what those passes would have - the potions, a fight with what attacks
	// it - and it walks back to its spot when a fight took it off it. Dead, it
	// lets the recovery have the tick; moved off the entrance's map by
	// something with the better claim, it says so and goes.
	bool ManagePlayerBotDungeonLfgWait(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return false;
		std::map<DWORD, TPlayerBotLfg>::iterator it = s_mapPlayerBotLfg.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotLfg.end() || it->second.phase != LFG_PHASE_WAITING)
			return false;
		TPlayerBotLfg& e = it->second;
		if (ch->IsDead())
			return false;
		// Invited: the follow pass has it now (the world tick says so too,
		// whichever comes first).
		if (ch->GetParty() && ch->GetParty()->IsMember(e.personPID))
		{
			EndPlayerBotDungeonLfg(ch->GetPlayerID(), "joined_party",
					number(1, 100) <= 50 ? LFG_LINE_JOINED : LFG_LINE_NONE);
			return false;
		}
		if (ch->GetMapIndex() != e.place.map)
		{
			EndPlayerBotDungeonLfg(ch->GetPlayerID(), "moved_away", LFG_LINE_CALLED_AWAY);
			return false;
		}
		if (HandlePostDeathRecovery(ch, state, dwNow))
			return true;
		UseHealthPotion(ch, state, dwNow);
		UseManaPotion(ch, state, dwNow);
		state.dwLastMeaningfulActivityTime = dwNow;
		state.lLastX = ch->GetX();
		state.lLastY = ch->GetY();

		// What attacks it, it fights.
		LPCHARACTER threat = e.guardVID ? CHARACTER_MANAGER::instance().Find(e.guardVID) : NULL;
		if (threat && (threat->IsDead() || threat->GetMapIndex() != ch->GetMapIndex() || threat->GetVictim() != ch))
			threat = NULL;
		if (!threat && dwNow >= e.nextGuardScanAt && ch->GetSectree())
		{
			e.nextGuardScanAt = dwNow + PLAYERBOT_LFG_GUARD_SCAN_MS;
			FPlayerBotLfgThreat finder(ch);
			ch->GetSectree()->ForEachAround(finder);
			threat = finder.m_best;
		}
		if (!threat && e.guardVID != 0)
		{
			if (state.dwTargetVID == e.guardVID)
			{
				state.dwTargetVID = 0;
				ch->SetVictim(NULL);
			}
			e.guardVID = 0;
		}
		if (threat && !IsPlayerBotSafeZone(threat->GetMapIndex(), threat->GetX(), threat->GetY()))
		{
			e.guardVID = (DWORD)threat->GetVID();
			state.dwTargetVID = e.guardVID;
			if (ch->IsRiding() && !CanPlayerBotFightOnHorse(ch, threat))
				SetPlayerBotRidingForTravel(ch, state, false, dwNow, "lfg_guard");
			LPITEM weapon = ch->GetWear(WEAR_WEAPON);
			const bool bow = weapon && weapon->GetType() == ITEM_WEAPON && weapon->GetSubType() == WEAPON_BOW;
			const int reach = bow ? 750 : 250;
			if (DISTANCE_APPROX(ch->GetX() - threat->GetX(), ch->GetY() - threat->GetY()) > reach)
				MovePlayerBot(ch, threat->GetX(), threat->GetY(), dwNow, 2, true, false, false, false);
			else
			{
				if (ch->IsStateMove())
					ch->Stop();
				ExecutePlayerBotBasicAttack(ch, threat, state, dwNow);
			}
			SetPlayerBotAction(state, BOT_ACTION_FIGHT, dwNow);
			return true;
		}

		// To its spot, and then it stands there.
		const int distance = DISTANCE_APPROX(ch->GetX() - e.spotX, ch->GetY() - e.spotY);
		if (distance > PLAYERBOT_LFG_STAY_DISTANCE)
		{
			MovePlayerBot(ch, e.spotX, e.spotY, dwNow, 6, true, true, false, false);
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			return true;
		}
		if (e.walking)
		{
			e.walking = false;
			sys_log(0, "PLAYERBOT_LFG: arrived pid=%u name=%s key=%s walk_s=%u", ch->GetPlayerID(), ch->GetName(),
					e.place.key.c_str(), (dwNow - e.acceptedAt) / 1000);
		}
		if (!state.vecRoute.empty())
			ClearPlayerBotRoute(state, true);
		if (ch->IsStateMove())
			ch->Stop();
		if (ch->IsRiding())
			SetPlayerBotRidingForTravel(ch, state, false, dwNow, "lfg_wait");
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		return true;
	}

	// ------------------------------------------------------- the world tick

	// The finder's clock, once a manager tick: the offers due out, the ones
	// lapsed, the waits ended by a party or by the five minutes.
	void ManagePlayerBotDungeonLfg(DWORD dwNow)
	{
		if (s_mapPlayerBotLfg.empty())
		{
			if (!s_mapPlayerBotLfgCalls.empty() && dwNow - s_dwPlayerBotLfgPruneAt >= PLAYERBOT_LFG_CALL_PRUNE_MS)
			{
				s_dwPlayerBotLfgPruneAt = dwNow;
				s_mapPlayerBotLfgCalls.clear();
			}
			return;
		}
		std::vector<std::pair<DWORD, int> > ends;   // bot pid, line
		std::vector<const char*> reasons;
		for (std::map<DWORD, TPlayerBotLfg>::iterator it = s_mapPlayerBotLfg.begin(); it != s_mapPlayerBotLfg.end(); ++it)
		{
			const DWORD botPID = it->first;
			TPlayerBotLfg& e = it->second;
			LPCHARACTER bot = CHARACTER_MANAGER::instance().FindByPID(botPID);
			TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(botPID);
			TPlayerBotConvPerson person;
			const bool personHere = FindPlayerBotConvPerson(e.personPID, person);
			if (!bot || st == s_mapPlayerBotAIStates.end())
			{
				ends.push_back(std::make_pair(botPID, (int)LFG_LINE_NONE));
				reasons.push_back("bot_gone");
				continue;
			}
			if (!personHere)
			{
				ends.push_back(std::make_pair(botPID, (int)LFG_LINE_NONE));
				reasons.push_back("person_gone");
				continue;
			}
			// In the person's party - invited at the entrance, or straight
			// after the offer: the follow pass has it from here. A bot at
			// the entrance says so now and then ("jestem :)"); one that never
			// went there has nothing to report.
			LPPARTY party = bot->GetParty();
			if (party && e.phase != LFG_PHASE_DUE && party->IsMember(e.personPID))
			{
				ends.push_back(std::make_pair(botPID, e.phase == LFG_PHASE_WAITING && number(1, 100) <= 50
						? (int)LFG_LINE_JOINED : (int)LFG_LINE_NONE));
				reasons.push_back("joined_party");
				continue;
			}
			switch (e.phase)
			{
				case LFG_PHASE_DUE:
				{
					if ((int)(dwNow - e.dueAt) < 0)
						break;
					const char* refusal = GetPlayerBotLfgRefusal(bot, st->second, dwNow);
					// Only a person of this core is whispered first: an offer
					// to one gone elsewhere since the call is no offer.
					if (refusal || !person.local)
					{
						ends.push_back(std::make_pair(botPID, (int)LFG_LINE_NONE));
						reasons.push_back(refusal ? refusal : "person_away");
						break;
					}
					playerbot_conv::TRng rng = MakePlayerBotLfgRng();
					const playerbot_lfg::TFacts f = MakePlayerBotLfgFacts(bot, e);
					const std::string text = e.ask ? playerbot_lfg::AskWhichLine(rng, f) : playerbot_lfg::OfferLine(rng, f);
					if (!SayPlayerBotLfgLine(bot, e.personPID, text, 0))
					{
						ends.push_back(std::make_pair(botPID, (int)LFG_LINE_NONE));
						reasons.push_back("unsaid");
						break;
					}
					e.phase = e.ask ? LFG_PHASE_ASKED : LFG_PHASE_OFFERED;
					e.offeredAt = dwNow;
					playerbot_lfg::TTalk* talk = GetPlayerBotLfgTalk(e.personPID, botPID);
					if (talk)
					{
						talk->state = e.ask ? playerbot_lfg::TALK_ASKED : playerbot_lfg::TALK_OFFERED;
						talk->key = e.place.key;
						talk->at = dwNow;
					}
					// A question the bot asked before is not what the next
					// line answers.
					playerbot_conv::TConvPair* pair = s_PlayerBotConvEngine.FindPair(e.personPID, botPID);
					if (pair)
						pair->mem.botAsk = playerbot_conv::ASK_NONE;
					++s_uPlayerBotLfgOffers;
					sys_log(0, "PLAYERBOT_LFG: %s pid=%u name=%s level=%u person=%s key=%s text=\"%s\"",
							e.ask ? "asked" : "offered", botPID, bot->GetName(), bot->GetLevel(), e.personName.c_str(),
							e.place.key.empty() ? "?" : e.place.key.c_str(), text.c_str());
					break;
				}
				case LFG_PHASE_OFFERED:
				case LFG_PHASE_ASKED:
					if (dwNow - e.offeredAt > playerbot_lfg::OFFER_TTL_MS + PLAYERBOT_LFG_OFFER_SLACK_MS)
					{
						ends.push_back(std::make_pair(botPID, (int)LFG_LINE_NONE));
						reasons.push_back("lapsed");
					}
					break;
				case LFG_PHASE_WAITING:
					if ((int)(dwNow - e.waitUntil) >= 0)
					{
						ends.push_back(std::make_pair(botPID, (int)LFG_LINE_TIMEOUT));
						reasons.push_back("timeout");
					}
					break;
				default:
					break;
			}
		}
		for (size_t i = 0; i < ends.size(); ++i)
			EndPlayerBotDungeonLfg(ends[i].first, reasons[i], ends[i].second);
		if (dwNow - s_dwPlayerBotLfgPruneAt >= PLAYERBOT_LFG_CALL_PRUNE_MS)
		{
			s_dwPlayerBotLfgPruneAt = dwNow;
			for (std::map<DWORD, TPlayerBotLfgCall>::iterator it = s_mapPlayerBotLfgCalls.begin();
					it != s_mapPlayerBotLfgCalls.end(); )
			{
				if (dwNow - it->second.at > PLAYERBOT_LFG_SAME_CALL_GAP_MS)
					s_mapPlayerBotLfgCalls.erase(it++);
				else
					++it;
			}
			sys_log(0, "PLAYERBOT_LFG: stats calls=%u offers=%u accepted=%u open=%u", s_uPlayerBotLfgCalls,
					s_uPlayerBotLfgOffers, s_uPlayerBotLfgAccepted, (unsigned int)s_mapPlayerBotLfg.size());
		}
	}

	// The line over the bot's head while it waits (playerbot_status.h asks it
	// after BuildPlayerBotSummonStatus). Inline, as that one is: declared there,
	// defined here.
	inline bool BuildPlayerBotDungeonLfgStatus(LPCHARACTER ch, const char* prefix, char* status, size_t statusSize, bool en)
	{
		if (!ch || !status || statusSize == 0)
			return false;
		std::map<DWORD, TPlayerBotLfg>::const_iterator it = s_mapPlayerBotLfg.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotLfg.end() || it->second.phase != LFG_PHASE_WAITING)
			return false;
		snprintf(status, statusSize, PBT(en, "%sCzekam na %s pod wejsciem", "%sWaiting for %s at the entrance"),
				prefix ? prefix : "", it->second.personName.c_str());
		return true;
	}
}

#endif
