#ifndef __INC_METIN2_PLAYERBOT_WORLD_EVENTS_H__
#define __INC_METIN2_PLAYERBOT_WORLD_EVENTS_H__

// Pirate Tanaka and Zuo: the two timed events that put something into the
// world, scheduled and switched on from the panel's Events page like the
// chests and the rates (playerbot_event_rules.h is the file and the clock,
// playerbot_events.h judges it once a second), and answered by the bots
// (the operator, 26 September: "Tanaka i zuo z integracja z panelem www, do tego
// boty niech sie motywuja do uczestniczenia w evencie").
//
// Tanaka is the treasure goblin. Pirate Tanaka (5001) is a level-one COWARD
// of thirty thousand health who runs from whoever hits him, scatters yang at
// every fifth of his health and more at his fall, and leaves his ear to the
// one who hurt him most and is still in the fight (playerbotify
// apply_tanaka_goblin); Yonah in each first village gives a Purple Ebony
// Chest for an ear (tanaka_ears.quest, and ManagePlayerBotTanakaEars for a
// bot). The event keeps `value` pirates in the world at once, spread over
// the maps the panel named or Tanaka's own list, puts a new one down a minute
// or two after one falls, and lets one that nobody has hit for half a minute
// run away after ten minutes, the way the goblin goes through its portal.
//
// Zuo is a rain of Metin stones and then of bosses over one map: a wave of
// `value` stones of the map's band every five minutes round a drop point the
// notice names by its coordinates, and from half the event on, bosses of the
// map's band instead. What stands at the end is taken back, except what
// somebody is still fighting.
//
// Both are run by one core: the one on the first channel hosting the map the
// panel chose, or for "the event picks" the events leader (Joan's core,
// IsPlayerBotEventLeader), from the maps it hosts. Its bots answer: a share
// of those whose level suits the map (the panel's "bots" line, drawn by pid,
// so the same bots come every time), up to a few chasers a pirate and a crowd
// for Zuo that grows with the stones. A war, the Demon Tower, a boss raid,
// the Catacomb, a duel and a person's party all come first - a bot on any of
// them is never called, and one called before is let go the moment the other
// takes it ("jak sa na wojnie czy na dt to niech na czas co robia ignoruja
// eventy"). So is a bot that has run out of what a fight needs.
//
// An implementation fragment in the sense playerbot_types.h describes:
// include it once, after playerbot_catacomb.h, beside the raids whose fight
// and keeping alive it borrows.

namespace
{
	const DWORD PLAYERBOT_WORLD_EVENT_CHECK_MS = 1000;
	const DWORD PLAYERBOT_WORLD_EVENT_CALL_MS = 5000;
	// The cohort spawns over the first minutes after a start, and a call made
	// then takes whoever happened to be first.
	const DWORD PLAYERBOT_WORLD_EVENT_FIRST_CALL_DELAY_MS = 90 * 1000;
	const DWORD PLAYERBOT_WORLD_EVENT_REMINDER_MS = 15 * 60 * 1000;
	// What a bot needs to be called: the boss raid's own floor for health, and
	// potions enough for a fight away from town.
	const int PLAYERBOT_WORLD_EVENT_MIN_HP_PERCENT = 60;
	const size_t PLAYERBOT_WORLD_EVENT_MIN_RED_POTIONS = 20;
	const size_t PLAYERBOT_WORLD_EVENT_MIN_BLUE_POTIONS = 15;
	// Where a bot brought from elsewhere is put down: a spot by pid round the
	// thing it was sent after, on open ground its own ground joins.
	const long PLAYERBOT_WORLD_EVENT_RALLY_MIN = 900;
	const long PLAYERBOT_WORLD_EVENT_RALLY_SPREAD = 900;
	// Further than this on the same map is a crossing, made the raid's way.
	const int PLAYERBOT_WORLD_EVENT_WALK_MAX = 30000;
	// A spawn point keeps this far from the safe zone and from the map's edge.
	const long PLAYERBOT_WORLD_EVENT_SAFE_MARGIN = 1500;
	const long PLAYERBOT_WORLD_EVENT_EDGE_MARGIN = 1500;
	// Every call brings at most this many, so arrivals spread.
	const int PLAYERBOT_WORLD_EVENT_CALLS_PER_PASS = 4;

	// Tanaka.
	const DWORD PLAYERBOT_TANAKA_FILL_GAP_MS = 3000;
	const DWORD PLAYERBOT_TANAKA_RESPAWN_MIN_MS = 60 * 1000;
	const DWORD PLAYERBOT_TANAKA_RESPAWN_MAX_MS = 120 * 1000;
	const DWORD PLAYERBOT_TANAKA_LIFETIME_MS = 10 * 60 * 1000;
	const DWORD PLAYERBOT_TANAKA_FIGHT_GRACE_MS = 30 * 1000;
	// A person gets the first go: no bot is sent at a pirate for this long
	// after the notice that he is there.
	const DWORD PLAYERBOT_TANAKA_BOT_HEAD_START_MS = 45 * 1000;
	const int PLAYERBOT_TANAKA_CHASERS = 3;
	const int PLAYERBOT_TANAKA_REMOTE_CHASERS = 2;
	const DWORD PLAYERBOT_TANAKA_NOTICE_GAP_MS = 15 * 1000;

	// Zuo.
	const DWORD PLAYERBOT_ZUO_FIRST_WAVE_MS = 60 * 1000;
	const DWORD PLAYERBOT_ZUO_WAVE_MS = 5 * 60 * 1000;
	const DWORD PLAYERBOT_ZUO_BOSS_WAVE_MS = 8 * 60 * 1000;
	const long PLAYERBOT_ZUO_WAVE_RADIUS = 1800;
	// Standing stones at most three waves' worth: a map nobody breaks does not
	// fill up.
	const int PLAYERBOT_ZUO_STANDING_WAVES = 3;
	const int PLAYERBOT_ZUO_STONE_ATTACKERS = 4;
	const int PLAYERBOT_ZUO_BOSS_ATTACKERS = 12;
	// At the end, a stone or a boss hit this recently stays for whoever is
	// breaking it; the rest is taken back.
	const DWORD PLAYERBOT_WORLD_EVENT_KEEP_FOUGHT_MS = 60 * 1000;

	// The maps an event may run on: their names for the notices, the level of
	// the bots sent there, the band of Zuo's stones, whether Tanaka's or Zuo's
	// own list takes them when the panel names no map, and Zuo's bosses. The
	// bands are the maps' own monsters (CLAUDE.md's measurements); the bosses
	// are each map's band read off world.mob_proto (26 September) - the first
	// villages' four S_KNIGHTs, the second villages' four and the Bestial
	// Captain, the frontier's own bosses and their elites.
	struct TPlayerBotEventMap
	{
		long lMap;
		const char* szName;
		const char* szAt;
		const char* szFrom;
		const char* szNameEn;
		BYTE bMinLevel;
		BYTE bMaxLevel;
		BYTE bStoneMinLevel;
		BYTE bStoneMaxLevel;
		bool bTanakaAuto;
		bool bZuoAuto;
		WORD awBosses[5];
	};

	const TPlayerBotEventMap PLAYERBOT_EVENT_MAPS[] =
	{
		{ 21, "Joan", "w Joan", "z Joan", "Joan", 1, 35, 5, 30, false, false, { 191, 192, 193, 194, 0 } },
		{ 1, "Yongan", "w Yongan", "z Yongan", "Yongan", 1, 35, 5, 30, false, false, { 191, 192, 193, 194, 0 } },
		{ 41, "Pyongmoo", "w Pyongmoo", "z Pyongmoo", "Pyongmoo", 1, 35, 5, 30, false, false, { 191, 192, 193, 194, 0 } },
		{ 23, "Bokjung", "w Bokjung", "z Bokjung", "Bokjung", 20, 45, 20, 40, false, false, { 491, 492, 493, 494, 591 } },
		{ 3, "Jayang", "w Jayang", "z Jayang", "Jayang", 20, 45, 20, 40, false, false, { 491, 492, 493, 494, 591 } },
		{ 43, "Bakra", "w Bakra", "z Bakra", "Bakra", 20, 45, 20, 40, false, false, { 491, 492, 493, 494, 591 } },
		{ 64, "Dolina Orkow", "w Dolinie Orkow", "z Doliny Orkow", "Orc Valley", 28, 60, 30, 50, true, true, { 591, 681, 692, 691, 0 } },
		{ 63, "Pustynia Yongbi", "na Pustyni Yongbi", "z Pustyni Yongbi", "Yongbi Desert", 35, 65, 35, 55, true, true, { 2181, 691, 791, 2191, 0 } },
		{ 61, "Gora Sohan", "na Gorze Sohan", "z Gory Sohan", "Mount Sohan", 45, 75, 45, 65, true, true, { 791, 1902, 1901, 0, 0 } },
		{ 65, "Swiatynia Hwang", "w Swiatyni Hwang", "ze Swiatyni Hwang", "Hwang Temple", 55, 85, 55, 75, false, true, { 791, 794, 792, 1304, 0 } },
		{ 62, "Ognista Ziemia", "na Ognistej Ziemi", "z Ognistej Ziemi", "Doyyumhwaji", 60, 90, 60, 80, true, true, { 2206, 2207, 1191, 0, 0 } },
		{ 67, "Las Duchow", "w Lesie Duchow", "z Lasu Duchow", "Ghost Wood", 60, 95, 65, 85, false, false, { 1191, 2306, 1304, 0, 0 } },
		{ 68, "Czerwony Las", "w Czerwonym Lesie", "z Czerwonego Lasu", "Red Wood", 65, 99, 70, 90, false, false, { 2306, 1192, 1191, 0, 0 } },
	};
	const size_t PLAYERBOT_EVENT_MAP_COUNT = sizeof(PLAYERBOT_EVENT_MAPS) / sizeof(PLAYERBOT_EVENT_MAPS[0]);

	// The ordinary Metin stones and their levels (world.mob_proto): not the
	// Demon Tower's 8015-8019, whose fall warps a map, not the kingdom stones,
	// the Easter ones or the 81xx copies of a hundred-odd health.
	const WORD PLAYERBOT_ZUO_STONES[][2] =
	{
		{ 8001, 5 }, { 8002, 10 }, { 8003, 15 }, { 8004, 20 }, { 8005, 25 }, { 8006, 30 }, { 8007, 35 },
		{ 8008, 40 }, { 8009, 45 }, { 8010, 50 }, { 8011, 55 }, { 8012, 60 }, { 8013, 65 }, { 8014, 70 },
		{ 8024, 75 }, { 8025, 80 }, { 8026, 85 }, { 8027, 90 }, { 8051, 95 }, { 8052, 95 },
		{ 8053, 100 }, { 8054, 100 }, { 8055, 105 }, { 8056, 105 },
	};
	const size_t PLAYERBOT_ZUO_STONE_COUNT = sizeof(PLAYERBOT_ZUO_STONES) / sizeof(PLAYERBOT_ZUO_STONES[0]);

	const TPlayerBotEventMap* GetPlayerBotEventMap(long lMap)
	{
		for (size_t i = 0; i < PLAYERBOT_EVENT_MAP_COUNT; ++i)
			if (PLAYERBOT_EVENT_MAPS[i].lMap == lMap)
				return &PLAYERBOT_EVENT_MAPS[i];
		return NULL;
	}

	// A pirate, a stone or a boss the event put down.
	struct TPlayerBotEventThing
	{
		DWORD vid;
		DWORD race;
		long map;
		DWORD spawnedAt;
		DWORD lastHitAt;
		int lastHp;
		bool boss;
	};

	struct TPlayerBotWorldEvent
	{
		bool running;
		int kind;
		int value;
		long since;
		long until;
		// The map the panel asked for (0: the event picks), and the maps it
		// runs on: Tanaka's list, or Zuo's one.
		long requestedMap;
		std::vector<long> maps;
		std::vector<TPlayerBotEventThing> things;
		std::set<DWORD> participants;
		// How many participants are on each thing, counted once a second.
		std::map<DWORD, int> attackers;
		DWORD startedTick;
		DWORD nextSpawnAt;
		DWORD nextBossAt;
		DWORD nextCallAt;
		DWORD nextReminderAt;
		DWORD lastNoticeAt;
		bool filling;
		bool bossHalfAnnounced;
		long dropX;
		long dropY;
		unsigned int waves;
		unsigned int spawned;
		unsigned int killed;
		unsigned int escaped;
		unsigned int bossesSpawned;
		unsigned int bossesKilled;
		unsigned int called;

		TPlayerBotWorldEvent() :
			running(false), kind(0), value(0), since(0), until(0), requestedMap(0), startedTick(0),
			nextSpawnAt(0), nextBossAt(0), nextCallAt(0), nextReminderAt(0), lastNoticeAt(0),
			filling(false), bossHalfAnnounced(false), dropX(0), dropY(0), waves(0), spawned(0),
			killed(0), escaped(0), bossesSpawned(0), bossesKilled(0), called(0) {}
	};

	TPlayerBotWorldEvent s_aPlayerBotWorldEvents[2];
	DWORD s_dwNextPlayerBotWorldEventCheck = 0;
	DWORD s_dwPlayerBotWorldEventsFirstSeen = 0;

	TPlayerBotWorldEvent* GetPlayerBotWorldEvent(int kind)
	{
		if (kind == playerbot_events::KIND_TANAKA)
			return &s_aPlayerBotWorldEvents[0];
		if (kind == playerbot_events::KIND_ZUO)
			return &s_aPlayerBotWorldEvents[1];
		return NULL;
	}

	// Who runs an event: the first channel's core that hosts the map the
	// panel named, or the events leader for "the event picks". Every core
	// judges the file the same way, so exactly one of them answers.
	bool IsPlayerBotWorldEventDriver(long requestedMap)
	{
		if (g_bChannel != 1)
			return false;
		if (requestedMap != 0)
			return SECTREE_MANAGER::instance().GetMap(requestedMap) != NULL;
		return IsPlayerBotEventLeader();
	}

	void SayPlayerBotWorldEvent(const char* szFormat, ...)
	{
		char text[256];
		va_list args;
		va_start(args, szFormat);
		vsnprintf(text, sizeof(text), szFormat, args);
		va_end(args);
		BroadcastNotice(text);
		sys_log(0, "PLAYERBOT_EVENT: notice \"%s\"", text);
	}

	// A point as a player's minimap shows it: cells from the map's own corner.
	void GetPlayerBotEventLocal(long lMap, long x, long y, long& outX, long& outY)
	{
		PIXEL_POSITION base;
		if (!SECTREE_MANAGER::instance().GetMapBasePositionByMapIndex(lMap, base))
		{
			base.x = 0;
			base.y = 0;
		}
		outX = (x - base.x) / 100;
		outY = (y - base.y) / 100;
	}

	// Open ground somewhere on the map: neither blocked nor near the safe
	// zone, on no pocket of ground the rest of the map does not join.
	bool PickPlayerBotEventPoint(long lMap, long& outX, long& outY)
	{
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(lMap);
		if (!pMap)
			return false;
		const TMapSetting& setting = pMap->m_setting;
		const long spanX = (long)setting.iWidth - 2 * PLAYERBOT_WORLD_EVENT_EDGE_MARGIN;
		const long spanY = (long)setting.iHeight - 2 * PLAYERBOT_WORLD_EVENT_EDGE_MARGIN;
		if (spanX <= 0 || spanY <= 0)
			return false;
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(lMap);
		const bool nav = navigation.Init(lMap);
		for (int attempt = 0; attempt < 60; ++attempt)
		{
			const long x = setting.iBaseX + PLAYERBOT_WORLD_EVENT_EDGE_MARGIN + number(0, (int)spanX);
			const long y = setting.iBaseY + PLAYERBOT_WORLD_EVENT_EDGE_MARGIN + number(0, (int)spanY);
			if (!IsPlayerBotWarGroundOpen(lMap, x, y) ||
					!IsPlayerBotWarGroundClearOfSafeZone(lMap, x, y, PLAYERBOT_WORLD_EVENT_SAFE_MARGIN))
				continue;
			if (nav)
			{
				const DWORD component = navigation.GetComponentAtWorld(x, y, 1);
				if (component == 0 || navigation.IsPocketComponent(component))
					continue;
			}
			outX = x;
			outY = y;
			return true;
		}
		return false;
	}

	// A bot's own spot by what it was sent after, the boss raid's way.
	void GetPlayerBotWorldEventRally(DWORD pid, long lMap, long x, long y, long& outX, long& outY)
	{
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(lMap);
		const bool nav = navigation.Init(lMap);
		const DWORD ground = nav ? navigation.GetComponentAtWorld(x, y, 12) : 0;
		const long radius = PLAYERBOT_WORLD_EVENT_RALLY_MIN +
				(long)(PlayerBotNavHash(pid ^ 0x45565241U) % (DWORD)PLAYERBOT_WORLD_EVENT_RALLY_SPREAD);
		const DWORD firstAngle = PlayerBotNavHash(pid ^ 0x45565242U) % 360U;
		for (int attempt = 0; attempt < 8; ++attempt)
		{
			const double rad = (double)((firstAngle + (DWORD)attempt * 45U) % 360U) * 3.14159265 / 180.0;
			long px = x + (long)(cos(rad) * radius);
			long py = y + (long)(sin(rad) * radius);
			long openX = 0, openY = 0;
			if (FindPlayerBotWarGround(lMap, px, py, 800, openX, openY))
			{
				px = openX;
				py = openY;
			}
			if (!nav || ground == 0 || navigation.GetComponentAtWorld(px, py, 4) == ground)
			{
				outX = px;
				outY = py;
				return;
			}
		}
		outX = x;
		outY = y;
	}

	// The character a thing is, or NULL once it is dead or gone - or its VID
	// belongs to something else by now.
	LPCHARACTER GetPlayerBotEventThing(const TPlayerBotEventThing& thing)
	{
		LPCHARACTER c = CHARACTER_MANAGER::instance().Find(thing.vid);
		if (!c || c->IsPC() || c->GetRaceNum() != thing.race || c->GetMapIndex() != thing.map)
			return NULL;
		return c;
	}

	void ClearPlayerBotWorldEventState(TPlayerBotAIState& state, LPCHARACTER ch)
	{
		const DWORD target = state.dwWorldEventTargetVID;
		state.bWorldEventKind = 0;
		state.lWorldEventMap = 0;
		state.dwWorldEventTargetVID = 0;
		state.dwNextWorldEventMoveTime = 0;
		state.dwWorldEventJoinedAt = 0;
		if (target != 0 && state.dwTargetVID == target)
			state.dwTargetVID = 0;
		if (ch && target != 0 && ch->GetVictim() && (DWORD)ch->GetVictim()->GetVID() == target)
			ch->SetVictim(NULL);
	}

	void ReleasePlayerBotWorldEventParticipant(TPlayerBotWorldEvent& ev, DWORD pid, const char* why)
	{
		ev.participants.erase(pid);
		TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.find(pid);
		if (it == s_mapPlayerBotAIStates.end() || it->second.bWorldEventKind != (BYTE)ev.kind)
			return;
		LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pid);
		ClearPlayerBotWorldEventState(it->second, ch);
		if (why)
			PlayerBotLogThrottled("world_event_release", get_dword_time(),
					"PLAYERBOT_EVENT: %s let go pid=%u name=%s why=%s", playerbot_events::KindName(ev.kind),
					pid, ch ? ch->GetName() : "?", why);
	}

	// Why this bot takes no part in an event now, or NULL when it may. The
	// order is the promise: whatever outranks an event is asked first.
	const char* GetPlayerBotWorldEventRefusal(LPCHARACTER c, const TPlayerBotAIState& st,
			int minLevel, int maxLevel, long eventMap, DWORD dwNow)
	{
		if (!c || c->IsDead() || !c->GetDesc() || !c->GetDesc()->IsBot())
			return "gone";
		const int level = (int)c->GetLevel();
		if (level < minLevel || level > maxLevel)
			return "level";
		if (st.bWorldEventKind != 0)
			return "already";
		const DWORD pid = c->GetPlayerID();
		// A war, the tower, a raid, the Catacomb, a duel: all of them first.
		if (IsPlayerBotOnTowerBusiness(c, st) || st.dwGuildWarEnemyGID != 0 ||
				playerbot_pvp::GetDuelOpponent(pid, dwNow) != 0)
			return "busy";
		if (IsPlayerBotSidekickPID(pid) || IsPlayerBotSummoned(pid) || IsPlayerBotOnMercContract(pid) ||
				IsPlayerBotHeldForCompany(c))
			return "company";
		if (c->GetParty() && IsPlayerBotHumanLedParty(c->GetParty()))
			return "person";
		if (IsPlayerBotDropper(st.bPersonality))
			return "dropper";
		if (st.bFishingSession || IsPlayerBotMiningNow(pid, dwNow))
			return "tool";
		if (IsPlayerBotOnBattleHorseTrial(c) || IsPlayerBotOnMilitaryHorseTrial(c))
			return "trial";
		if (st.bTownVisitPhase != BOT_TOWN_PHASE_NONE || c->GetMyShop())
			return "town";
		if (c->GetSkillGroup() == 0)
			return "no_skills";
		// A negative rank keeps the bot in its village (KeepPlayerBotNegativeRankInTown).
		if (c->GetRealAlignment() < 0)
			return "rank";
		const long map = c->GetMapIndex();
		if (map >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN || map == PLAYERBOT_MAP_DEMON_TOWER ||
				IsPlayerBotDemonTowerInstance(map) || IsPlayerBotCatacombInstance(map))
			return "dungeon";
		// The Spider Dungeons are left across the desert on foot, which takes
		// longer than a pirate lives.
		if (map != eventMap && IsPlayerBotSpiderMap(map))
			return "far";
		if (st.bRecoveringAfterDeath || c->GetMaxHP() <= 0 ||
				(long long)c->GetHP() * 100 < (long long)c->GetMaxHP() * PLAYERBOT_WORLD_EVENT_MIN_HP_PERCENT)
			return "health";
		if (BlocksPlayerBotTravel(c))
			return "supplies";
		size_t red = 0, blue = 0;
		CountPlayerBotPotions(c, red, blue);
		const bool caster = c->GetJob() == JOB_SHAMAN || (c->GetJob() == JOB_SURA && c->GetSkillGroup() == 2);
		if (red < PLAYERBOT_WORLD_EVENT_MIN_RED_POTIONS ||
				(caster && blue < PLAYERBOT_WORLD_EVENT_MIN_BLUE_POTIONS))
			return "potions";
		return NULL;
	}

	void EnlistPlayerBotWorldEventParticipant(TPlayerBotWorldEvent& ev, DWORD pid, long lMap,
			DWORD targetVID, DWORD dwNow)
	{
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(pid);
		if (st == s_mapPlayerBotAIStates.end())
			return;
		ev.participants.insert(pid);
		++ev.called;
		st->second.bWorldEventKind = (BYTE)ev.kind;
		st->second.lWorldEventMap = lMap;
		st->second.dwWorldEventTargetVID = targetVID;
		st->second.dwWorldEventJoinedAt = dwNow;
		// The departures spread over the first few seconds by pid.
		st->second.dwNextWorldEventMoveTime = dwNow + PlayerBotNavHash(pid ^ 0x45564E4CU) % 6000U;
	}

	// ------------------------------------------------------------------
	// Tanaka
	// ------------------------------------------------------------------

	int CountPlayerBotLivePirates(const TPlayerBotWorldEvent& ev, long lMap)
	{
		int n = 0;
		for (size_t i = 0; i < ev.things.size(); ++i)
			if (ev.things[i].map == lMap)
				++n;
		return n;
	}

	bool SpawnPlayerBotTanaka(TPlayerBotWorldEvent& ev, DWORD dwNow)
	{
		if (ev.maps.empty())
			return false;
		// The map with the fewest pirates on it, and among those the next in turn.
		long lMap = 0;
		int fewest = INT_MAX;
		for (size_t k = 0; k < ev.maps.size(); ++k)
		{
			const long candidate = ev.maps[(ev.spawned + k) % ev.maps.size()];
			const int n = CountPlayerBotLivePirates(ev, candidate);
			if (n < fewest)
			{
				fewest = n;
				lMap = candidate;
			}
		}
		long x = 0, y = 0;
		if (!PickPlayerBotEventPoint(lMap, x, y))
		{
			PlayerBotLogThrottled("tanaka_no_ground", dwNow,
					"PLAYERBOT_EVENT: tanaka found no open ground on map=%ld", lMap);
			return false;
		}
		LPCHARACTER pirate = CHARACTER_MANAGER::instance().SpawnMob(
				PLAYERBOT_TANAKA_VNUM, lMap, x, y, 0, true, -1, true);
		if (!pirate)
		{
			PlayerBotLogThrottled("tanaka_refused", dwNow,
					"PLAYERBOT_EVENT: tanaka refused by the engine map=%ld pos=(%ld,%ld)", lMap, x, y);
			return false;
		}
		TPlayerBotEventThing thing;
		thing.vid = (DWORD)pirate->GetVID();
		thing.race = PLAYERBOT_TANAKA_VNUM;
		thing.map = lMap;
		thing.spawnedAt = dwNow;
		thing.lastHitAt = 0;
		thing.lastHp = pirate->GetHP();
		thing.boss = false;
		ev.things.push_back(thing);
		++ev.spawned;
		long lx = 0, ly = 0;
		GetPlayerBotEventLocal(lMap, x, y, lx, ly);
		sys_log(0, "PLAYERBOT_EVENT: tanaka spawned vid=%u map=%ld pos=(%ld,%ld) local=(%ld,%ld) alive=%u of %d",
				thing.vid, lMap, x, y, lx, ly, (unsigned int)ev.things.size(), ev.value);
		const TPlayerBotEventMap* row = GetPlayerBotEventMap(lMap);
		if (!ev.filling && dwNow - ev.lastNoticeAt >= PLAYERBOT_TANAKA_NOTICE_GAP_MS)
		{
			ev.lastNoticeAt = dwNow;
			SayPlayerBotWorldEvent("Pirat Tanaka pojawil sie %s (%ld, %ld)!", row ? row->szAt : "", lx, ly);
		}
		return true;
	}

	// ------------------------------------------------------------------
	// Zuo
	// ------------------------------------------------------------------

	DWORD PickPlayerBotZuoStone(const TPlayerBotEventMap& row)
	{
		DWORD pool[PLAYERBOT_ZUO_STONE_COUNT];
		size_t n = 0;
		for (size_t i = 0; i < PLAYERBOT_ZUO_STONE_COUNT; ++i)
			if (PLAYERBOT_ZUO_STONES[i][1] >= row.bStoneMinLevel && PLAYERBOT_ZUO_STONES[i][1] <= row.bStoneMaxLevel)
				pool[n++] = PLAYERBOT_ZUO_STONES[i][0];
		return n == 0 ? (DWORD)PLAYERBOT_ZUO_STONES[0][0] : pool[number(0, (int)n - 1)];
	}

	int CountPlayerBotStandingThings(const TPlayerBotWorldEvent& ev, bool bosses)
	{
		int n = 0;
		for (size_t i = 0; i < ev.things.size(); ++i)
			if (ev.things[i].boss == bosses)
				++n;
		return n;
	}

	// One thing round a point: a spot on a ring, on open ground.
	LPCHARACTER SpawnPlayerBotEventThingNear(TPlayerBotWorldEvent& ev, DWORD race, long lMap, long cx, long cy,
			int index, int count, long radius, bool boss, DWORD dwNow)
	{
		const double angle = (double)index * (6.28318530718 / (double)std::max(1, count)) +
				(double)number(-20, 20) * 3.14159265 / 180.0;
		const long r = count <= 1 ? 0 : number((int)(radius / 3), (int)radius);
		long x = cx + (long)(cos(angle) * r);
		long y = cy + (long)(sin(angle) * r);
		long openX = 0, openY = 0;
		if (!FindPlayerBotWarGround(lMap, x, y, 700, openX, openY, 400))
			return NULL;
		LPCHARACTER c = CHARACTER_MANAGER::instance().SpawnMob(race, lMap, openX, openY, 0, true, -1, true);
		if (!c)
			return NULL;
		TPlayerBotEventThing thing;
		thing.vid = (DWORD)c->GetVID();
		thing.race = race;
		thing.map = lMap;
		thing.spawnedAt = dwNow;
		thing.lastHitAt = 0;
		thing.lastHp = c->GetHP();
		thing.boss = boss;
		ev.things.push_back(thing);
		return c;
	}

	void SpawnPlayerBotZuoWave(TPlayerBotWorldEvent& ev, DWORD dwNow)
	{
		const long lMap = ev.maps.empty() ? 0 : ev.maps[0];
		const TPlayerBotEventMap* row = GetPlayerBotEventMap(lMap);
		if (!row)
			return;
		const int room = ev.value * PLAYERBOT_ZUO_STANDING_WAVES - CountPlayerBotStandingThings(ev, false);
		const int count = std::min(ev.value, room);
		if (count <= 0)
		{
			sys_log(0, "PLAYERBOT_EVENT: zuo wave skipped, %d stones still standing on map=%ld",
					CountPlayerBotStandingThings(ev, false), lMap);
			return;
		}
		long cx = 0, cy = 0;
		if (!PickPlayerBotEventPoint(lMap, cx, cy))
		{
			PlayerBotLogThrottled("zuo_no_ground", dwNow,
					"PLAYERBOT_EVENT: zuo found no open ground on map=%ld", lMap);
			return;
		}
		int made = 0;
		for (int i = 0; i < count; ++i)
			if (SpawnPlayerBotEventThingNear(ev, PickPlayerBotZuoStone(*row), lMap, cx, cy, i, count,
					PLAYERBOT_ZUO_WAVE_RADIUS, false, dwNow))
				++made;
		if (made == 0)
			return;
		++ev.waves;
		ev.dropX = cx;
		ev.dropY = cy;
		long lx = 0, ly = 0;
		GetPlayerBotEventLocal(lMap, cx, cy, lx, ly);
		sys_log(0, "PLAYERBOT_EVENT: zuo wave %u map=%ld pos=(%ld,%ld) local=(%ld,%ld) stones=%d standing=%d",
				ev.waves, lMap, cx, cy, lx, ly, made, CountPlayerBotStandingThings(ev, false));
		SayPlayerBotWorldEvent("Zuo: spadlo %d metinow %s, okolice (%ld, %ld)!", made, row->szAt, lx, ly);
	}

	void SpawnPlayerBotZuoBosses(TPlayerBotWorldEvent& ev, DWORD dwNow)
	{
		const long lMap = ev.maps.empty() ? 0 : ev.maps[0];
		const TPlayerBotEventMap* row = GetPlayerBotEventMap(lMap);
		if (!row)
			return;
		WORD pool[5];
		int poolSize = 0;
		for (int i = 0; i < 5; ++i)
			if (row->awBosses[i] != 0 && CMobManager::instance().Get(row->awBosses[i]))
				pool[poolSize++] = row->awBosses[i];
		if (poolSize == 0)
			return;
		const int perWave = std::max(1, std::min(3, 1 + ev.value / 10));
		const int count = std::min(perWave, perWave * 2 - CountPlayerBotStandingThings(ev, true));
		if (count <= 0)
			return;
		long cx = 0, cy = 0;
		if (!PickPlayerBotEventPoint(lMap, cx, cy))
			return;
		std::string names;
		int made = 0;
		for (int i = 0; i < count; ++i)
		{
			const WORD race = pool[number(0, poolSize - 1)];
			LPCHARACTER boss = SpawnPlayerBotEventThingNear(ev, race, lMap, cx, cy, i, count, 700, true, dwNow);
			if (!boss)
				continue;
			++made;
			if (!names.empty())
				names += ", ";
			names += boss->GetName();
		}
		if (made == 0)
			return;
		ev.bossesSpawned += made;
		ev.dropX = cx;
		ev.dropY = cy;
		long lx = 0, ly = 0;
		GetPlayerBotEventLocal(lMap, cx, cy, lx, ly);
		sys_log(0, "PLAYERBOT_EVENT: zuo bosses map=%ld pos=(%ld,%ld) local=(%ld,%ld) count=%d",
				lMap, cx, cy, lx, ly, made);
		SayPlayerBotWorldEvent("Zuo przyzwal: %s %s, okolice (%ld, %ld)!", names.c_str(), row->szAt, lx, ly);
	}

	// ------------------------------------------------------------------
	// The world's pass
	// ------------------------------------------------------------------

	// The things standing: their health watched (a pirate hit this recently
	// does not run), the fallen counted, and a pirate that has run his time
	// out taken back.
	void UpdatePlayerBotWorldEventThings(TPlayerBotWorldEvent& ev, DWORD dwNow)
	{
		for (size_t i = 0; i < ev.things.size();)
		{
			TPlayerBotEventThing& thing = ev.things[i];
			LPCHARACTER c = GetPlayerBotEventThing(thing);
			const TPlayerBotEventMap* row = GetPlayerBotEventMap(thing.map);
			if (!c || c->IsDead())
			{
				const bool fell = c != NULL || thing.lastHitAt != 0;
				if (ev.kind == playerbot_events::KIND_TANAKA)
				{
					if (fell)
					{
						++ev.killed;
						const char* winner = NULL;
#if defined(PLAYERBOT_ENGINE_MT2009)
						// His damage map outlives his fall (Reward returns
						// before it is cleared), and GetMostAttacked is the
						// same active attacker his ear went to.
						LPCHARACTER best = c ? c->GetMostAttacked() : NULL;
						if (best && best->IsPC())
							winner = best->GetName();
#endif
						sys_log(0, "PLAYERBOT_EVENT: tanaka fell vid=%u map=%ld winner=%s killed=%u",
								thing.vid, thing.map, winner ? winner : "?", ev.killed);
						if (winner)
							SayPlayerBotWorldEvent("Pirat Tanaka padl %s - zwyciezca: %s!",
									row ? row->szAt : "", winner);
						else
							SayPlayerBotWorldEvent("Pirat Tanaka padl %s!", row ? row->szAt : "");
					}
					// The next one after a minute or two, wherever there are fewest.
					const DWORD next = dwNow + (DWORD)number((int)PLAYERBOT_TANAKA_RESPAWN_MIN_MS,
							(int)PLAYERBOT_TANAKA_RESPAWN_MAX_MS);
					if (ev.nextSpawnAt < next)
						ev.nextSpawnAt = next;
				}
				else if (fell)
				{
					if (thing.boss)
					{
						++ev.bossesKilled;
						const CMob* mob = CMobManager::instance().Get(thing.race);
						SayPlayerBotWorldEvent("Boss Zuo padl %s: %s.", row ? row->szAt : "",
								mob ? mob->m_table.szLocaleName : "boss");
					}
					else
						++ev.killed;
				}
				ev.things.erase(ev.things.begin() + i);
				continue;
			}
			const int hp = c->GetHP();
			if (hp < thing.lastHp)
				thing.lastHitAt = dwNow;
			thing.lastHp = hp;
			if (ev.kind == playerbot_events::KIND_TANAKA &&
					dwNow - thing.spawnedAt >= PLAYERBOT_TANAKA_LIFETIME_MS &&
					(thing.lastHitAt == 0 || dwNow - thing.lastHitAt >= PLAYERBOT_TANAKA_FIGHT_GRACE_MS))
			{
				++ev.escaped;
				sys_log(0, "PLAYERBOT_EVENT: tanaka escaped vid=%u map=%ld hp=%d/%d", thing.vid, thing.map,
						hp, c->GetMaxHP());
				SayPlayerBotWorldEvent("Pirat Tanaka uciekl %s!", row ? row->szFrom : "");
				M2_DESTROY_CHARACTER(c);
				ev.things.erase(ev.things.begin() + i);
				const DWORD next = dwNow + (DWORD)number((int)PLAYERBOT_TANAKA_RESPAWN_MIN_MS,
						(int)PLAYERBOT_TANAKA_RESPAWN_MAX_MS);
				if (ev.nextSpawnAt < next)
					ev.nextSpawnAt = next;
				continue;
			}
			++i;
		}
	}

	// The participants gone from the world, taken by something that outranks
	// the event, or let go by their own pass; and who is on what.
	void PrunePlayerBotWorldEventParticipants(TPlayerBotWorldEvent& ev, DWORD dwNow)
	{
		ev.attackers.clear();
		for (std::set<DWORD>::iterator it = ev.participants.begin(); it != ev.participants.end();)
		{
			const DWORD pid = *it++;
			TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(pid);
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pid);
			if (!ch || st == s_mapPlayerBotAIStates.end() || st->second.bWorldEventKind != (BYTE)ev.kind)
			{
				ev.participants.erase(pid);
				continue;
			}
			if (IsPlayerBotOnTowerBusiness(ch, st->second) || st->second.dwGuildWarEnemyGID != 0 ||
					playerbot_pvp::GetDuelOpponent(pid, dwNow) != 0)
			{
				ReleasePlayerBotWorldEventParticipant(ev, pid, "outranked");
				continue;
			}
			if (st->second.dwWorldEventTargetVID != 0)
				++ev.attackers[st->second.dwWorldEventTargetVID];
		}
	}

	struct TPlayerBotEventRecruit
	{
		DWORD pid;
		long map;
		long x;
		long y;
		int level;
		DWORD order;
	};

	bool PlayerBotEventRecruitOrder(const TPlayerBotEventRecruit& a, const TPlayerBotEventRecruit& b)
	{
		if (a.order != b.order)
			return a.order < b.order;
		return a.pid < b.pid;
	}

	// Every bot of this core that could answer now, the draw first and the
	// bag - the dearest question - last. Built once a call, whatever the
	// number of pirates it serves.
	void CollectPlayerBotEventRecruits(int minLevel, int maxLevel, long eventMap, DWORD dwNow,
			std::vector<TPlayerBotEventRecruit>& out)
	{
		out.clear();
		const int percent = s_PlayerBotEventSettings.botsPercent;
		if (percent <= 0)
			return;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			if (it->second.bWorldEventKind != 0 || !playerbot_events::BotTakesPart(it->first, percent))
				continue;
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!c || GetPlayerBotWorldEventRefusal(c, it->second, minLevel, maxLevel, eventMap, dwNow))
				continue;
			TPlayerBotEventRecruit r;
			r.pid = it->first;
			r.map = c->GetMapIndex();
			r.x = c->GetX();
			r.y = c->GetY();
			r.level = (int)c->GetLevel();
			r.order = 0;
			out.push_back(r);
		}
	}

	void CallPlayerBotTanakaChasers(TPlayerBotWorldEvent& ev, DWORD dwNow)
	{
		// Only a pirate past the head start and short of chasers needs a call,
		// and the pool is built once for all of them, from the lowest floor
		// among their maps.
		int floorLevel = INT_MAX;
		for (size_t i = 0; i < ev.things.size(); ++i)
		{
			const TPlayerBotEventThing& thing = ev.things[i];
			std::map<DWORD, int>::const_iterator have = ev.attackers.find(thing.vid);
			const TPlayerBotEventMap* row = GetPlayerBotEventMap(thing.map);
			if (row && dwNow - thing.spawnedAt >= PLAYERBOT_TANAKA_BOT_HEAD_START_MS &&
					(have == ev.attackers.end() || have->second < PLAYERBOT_TANAKA_CHASERS))
				floorLevel = std::min(floorLevel, (int)row->bMinLevel);
		}
		if (floorLevel == INT_MAX)
			return;
		std::vector<TPlayerBotEventRecruit> all;
		CollectPlayerBotEventRecruits(floorLevel, PLAYER_MAX_LEVEL_CONST, 0, dwNow, all);
		int budget = PLAYERBOT_WORLD_EVENT_CALLS_PER_PASS;
		for (size_t i = 0; i < ev.things.size() && budget > 0; ++i)
		{
			const TPlayerBotEventThing& thing = ev.things[i];
			if (dwNow - thing.spawnedAt < PLAYERBOT_TANAKA_BOT_HEAD_START_MS)
				continue;
			std::map<DWORD, int>::const_iterator have = ev.attackers.find(thing.vid);
			int chasers = have == ev.attackers.end() ? 0 : have->second;
			if (chasers >= PLAYERBOT_TANAKA_CHASERS)
				continue;
			LPCHARACTER pirate = GetPlayerBotEventThing(thing);
			const TPlayerBotEventMap* row = GetPlayerBotEventMap(thing.map);
			if (!pirate || !row)
				continue;
			// His map's floor, and nobody from a Spider Dungeon for another map.
			std::vector<TPlayerBotEventRecruit> pool;
			for (size_t k = 0; k < all.size(); ++k)
			{
				TPlayerBotEventRecruit r = all[k];
				if (r.level < (int)row->bMinLevel || (r.map != thing.map && IsPlayerBotSpiderMap(r.map)))
					continue;
				// On his map by distance, then the rest by pid.
				if (r.map == thing.map)
					r.order = (DWORD)DISTANCE_APPROX(r.x - pirate->GetX(), r.y - pirate->GetY());
				else
					r.order = 0x80000000U + (PlayerBotNavHash(r.pid ^ 0x54414E4BU) & 0x7FFFFFFFU);
				pool.push_back(r);
			}
			std::sort(pool.begin(), pool.end(), PlayerBotEventRecruitOrder);
			int remote = 0;
			for (std::set<DWORD>::const_iterator p = ev.participants.begin(); p != ev.participants.end(); ++p)
			{
				TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(*p);
				LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(*p);
				if (st != s_mapPlayerBotAIStates.end() && c && st->second.dwWorldEventTargetVID == thing.vid &&
						c->GetMapIndex() != thing.map)
					++remote;
			}
			for (size_t k = 0; k < pool.size() && chasers < PLAYERBOT_TANAKA_CHASERS && budget > 0; ++k)
			{
				const bool fromElsewhere = pool[k].map != thing.map;
				if (fromElsewhere && remote >= PLAYERBOT_TANAKA_REMOTE_CHASERS)
					continue;
				TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(pool[k].pid);
				if (st == s_mapPlayerBotAIStates.end() || st->second.bWorldEventKind != 0)
					continue;
				EnlistPlayerBotWorldEventParticipant(ev, pool[k].pid, thing.map, thing.vid, dwNow);
				++ev.attackers[thing.vid];
				++chasers;
				--budget;
				if (fromElsewhere)
					++remote;
				LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(pool[k].pid);
				sys_log(0, "PLAYERBOT_EVENT: tanaka chaser pid=%u name=%s level=%d vid=%u map=%ld from_map=%ld",
						pool[k].pid, c ? c->GetName() : "?", c ? (int)c->GetLevel() : 0, thing.vid,
						thing.map, pool[k].map);
			}
		}
	}

	void CallPlayerBotZuoParticipants(TPlayerBotWorldEvent& ev, DWORD dwNow)
	{
		const long lMap = ev.maps.empty() ? 0 : ev.maps[0];
		const TPlayerBotEventMap* row = GetPlayerBotEventMap(lMap);
		if (!row)
			return;
		const int cap = std::max(6, std::min(40, ev.value * 2));
		if ((int)ev.participants.size() >= cap)
			return;
		std::vector<TPlayerBotEventRecruit> pool;
		CollectPlayerBotEventRecruits(row->bMinLevel, row->bMaxLevel, lMap, dwNow, pool);
		const long cx = ev.dropX, cy = ev.dropY;
		for (size_t k = 0; k < pool.size(); ++k)
		{
			TPlayerBotEventRecruit& r = pool[k];
			if (r.map == lMap)
				r.order = (cx == 0 && cy == 0) ? 0 : (DWORD)DISTANCE_APPROX(r.x - cx, r.y - cy);
			else
				r.order = 0x80000000U + (PlayerBotNavHash(r.pid ^ 0x5A554F31U) & 0x7FFFFFFFU);
		}
		std::sort(pool.begin(), pool.end(), PlayerBotEventRecruitOrder);
		int budget = PLAYERBOT_WORLD_EVENT_CALLS_PER_PASS;
		for (size_t k = 0; k < pool.size() && (int)ev.participants.size() < cap && budget > 0; ++k)
		{
			EnlistPlayerBotWorldEventParticipant(ev, pool[k].pid, lMap, 0, dwNow);
			--budget;
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(pool[k].pid);
			sys_log(0, "PLAYERBOT_EVENT: zuo called pid=%u name=%s level=%d map=%ld from_map=%ld participants=%u of %d",
					pool[k].pid, c ? c->GetName() : "?", c ? (int)c->GetLevel() : 0, lMap, pool[k].map,
					(unsigned int)ev.participants.size(), cap);
		}
	}

	std::string DescribePlayerBotWorldEventMaps(const TPlayerBotWorldEvent& ev)
	{
		std::string out;
		for (size_t i = 0; i < ev.maps.size(); ++i)
		{
			const TPlayerBotEventMap* row = GetPlayerBotEventMap(ev.maps[i]);
			if (!row)
				continue;
			if (!out.empty())
				out += ", ";
			out += row->szAt;
		}
		return out;
	}

	void BeginPlayerBotWorldEvent(int kind, const playerbot_events::Status& st, DWORD dwNow)
	{
		TPlayerBotWorldEvent* evp = GetPlayerBotWorldEvent(kind);
		if (!evp)
			return;
		TPlayerBotWorldEvent& ev = *evp;
		ev = TPlayerBotWorldEvent();
		ev.running = true;
		ev.kind = kind;
		ev.value = playerbot_events::WorldEventCount(kind, st.value);
		const long nowEpoch = (long)time(NULL);
		ev.since = st.since > 0 ? st.since : nowEpoch;
		ev.until = st.until;
		ev.requestedMap = st.map;
		ev.startedTick = dwNow;
		ev.nextCallAt = dwNow + PLAYERBOT_WORLD_EVENT_CALL_MS;
		ev.nextReminderAt = dwNow + PLAYERBOT_WORLD_EVENT_REMINDER_MS;
		// The maps: the one the panel named if this core hosts it (it would not
		// be driving otherwise), or the kind's own list from what it hosts.
		if (st.map != 0 && GetPlayerBotEventMap(st.map) && IsPlayerBotMapHostedHere(st.map))
			ev.maps.push_back(st.map);
		else
		{
			std::vector<long> auto_;
			for (size_t i = 0; i < PLAYERBOT_EVENT_MAP_COUNT; ++i)
			{
				const TPlayerBotEventMap& row = PLAYERBOT_EVENT_MAPS[i];
				const bool listed = kind == playerbot_events::KIND_TANAKA ? row.bTanakaAuto : row.bZuoAuto;
				if (listed && IsPlayerBotMapHostedHere(row.lMap))
					auto_.push_back(row.lMap);
			}
			if (kind == playerbot_events::KIND_TANAKA)
				ev.maps = auto_;
			else if (!auto_.empty())
				// Drawn by the event's own first second: the same map after a
				// restart, another the next time.
				ev.maps.push_back(auto_[PlayerBotNavHash((DWORD)ev.since ^ 0x5A554F4DU) % auto_.size()]);
		}
		char when[16];
		FormatPlayerBotEventClock(ev.until, when, sizeof(when));
		const std::string where = DescribePlayerBotWorldEventMaps(ev);
		sys_log(0, "PLAYERBOT_EVENT: %s begins here value=%d maps=%u requested_map=%ld since=%ld until=%ld bots=%d%%",
				playerbot_events::KindName(kind), ev.value, (unsigned int)ev.maps.size(), st.map, ev.since,
				ev.until, s_PlayerBotEventSettings.botsPercent);
		if (ev.maps.empty())
		{
			sys_err("PLAYERBOT_EVENT: %s has no map this core can run it on (requested %ld)",
					playerbot_events::KindName(kind), st.map);
			return;
		}
		if (kind == playerbot_events::KIND_TANAKA)
		{
			ev.filling = true;
			ev.nextSpawnAt = dwNow;
			SayPlayerBotWorldEvent("Event: Pirat Tanaka grasuje %s do %s! Kto go pokona, zgarnie jego yang, "
					"a Yonah w pierwszej wiosce da szkatulke za jego ucho.", where.c_str(), when);
		}
		else
		{
			const TPlayerBotEventMap* row = GetPlayerBotEventMap(ev.maps[0]);
			// A restart inside the event picks up where the clock is: no rain
			// again in the second half.
			ev.nextSpawnAt = dwNow + PLAYERBOT_ZUO_FIRST_WAVE_MS;
			ev.nextBossAt = dwNow + PLAYERBOT_ZUO_FIRST_WAVE_MS;
			SayPlayerBotWorldEvent("Event Zuo: deszcz metinow %s do %s! Od polowy eventu Zuo przyzywa bossow.",
					row ? row->szAt : "", when);
		}
	}

	void EndPlayerBotWorldEvent(int kind, const char* why, bool announce, DWORD dwNow)
	{
		TPlayerBotWorldEvent* evp = GetPlayerBotWorldEvent(kind);
		if (!evp || !evp->running)
			return;
		TPlayerBotWorldEvent& ev = *evp;
		int taken = 0, left = 0;
		for (size_t i = 0; i < ev.things.size(); ++i)
		{
			LPCHARACTER c = GetPlayerBotEventThing(ev.things[i]);
			if (!c || c->IsDead())
				continue;
			// A pirate always runs; a stone or a boss somebody is still
			// breaking stays theirs.
			if (kind == playerbot_events::KIND_ZUO && ev.things[i].lastHitAt != 0 &&
					dwNow - ev.things[i].lastHitAt < PLAYERBOT_WORLD_EVENT_KEEP_FOUGHT_MS)
			{
				++left;
				continue;
			}
			M2_DESTROY_CHARACTER(c);
			++taken;
		}
		const std::set<DWORD> participants = ev.participants;
		for (std::set<DWORD>::const_iterator it = participants.begin(); it != participants.end(); ++it)
			ReleasePlayerBotWorldEventParticipant(ev, *it, NULL);
		sys_log(0, "PLAYERBOT_EVENT: %s over here why=%s spawned=%u killed=%u escaped=%u waves=%u bosses=%u/%u "
				"called=%u taken_back=%d left_fought=%d after_s=%u",
				playerbot_events::KindName(kind), why, ev.spawned, ev.killed, ev.escaped, ev.waves,
				ev.bossesKilled, ev.bossesSpawned, ev.called, taken, left, (dwNow - ev.startedTick) / 1000U);
		if (announce && !ev.maps.empty())
		{
			if (kind == playerbot_events::KIND_TANAKA)
				SayPlayerBotWorldEvent("Event zakonczony: Pirat Tanaka odplynal. Pokonany %u razy.", ev.killed);
			else
				SayPlayerBotWorldEvent("Event Zuo zakonczony: rozbite metiny %u, pokonani bossowie %u z %u.",
						ev.killed, ev.bossesKilled, ev.bossesSpawned);
		}
		ev = TPlayerBotWorldEvent();
	}

	void AdvancePlayerBotWorldEvent(int kind, const playerbot_events::Status& st, DWORD dwNow)
	{
		TPlayerBotWorldEvent& ev = *GetPlayerBotWorldEvent(kind);
		ev.value = playerbot_events::WorldEventCount(kind, st.value);
		if (st.until > 0)
			ev.until = st.until;
		if (ev.maps.empty())
			return;
		UpdatePlayerBotWorldEventThings(ev, dwNow);
		PrunePlayerBotWorldEventParticipants(ev, dwNow);
		if (kind == playerbot_events::KIND_TANAKA)
		{
			if ((int)ev.things.size() < ev.value && dwNow >= ev.nextSpawnAt)
			{
				SpawnPlayerBotTanaka(ev, dwNow);
				if (ev.filling)
				{
					ev.nextSpawnAt = dwNow + PLAYERBOT_TANAKA_FILL_GAP_MS;
					if ((int)ev.things.size() >= ev.value)
						ev.filling = false;
				}
			}
			else if (ev.filling && (int)ev.things.size() >= ev.value)
				ev.filling = false;
		}
		else
		{
			const long nowEpoch = (long)time(NULL);
			if (!playerbot_events::IsZuoBossHalf(ev.since, ev.until, nowEpoch))
			{
				if (dwNow >= ev.nextSpawnAt)
				{
					ev.nextSpawnAt = dwNow + PLAYERBOT_ZUO_WAVE_MS;
					SpawnPlayerBotZuoWave(ev, dwNow);
				}
			}
			else
			{
				if (!ev.bossHalfAnnounced)
				{
					ev.bossHalfAnnounced = true;
					if (ev.nextBossAt < dwNow + 20000)
						ev.nextBossAt = dwNow + 20000;
					SayPlayerBotWorldEvent("Zuo: metiny przestaja spadac - nadchodza bossowie!");
				}
				if (dwNow >= ev.nextBossAt)
				{
					ev.nextBossAt = dwNow + PLAYERBOT_ZUO_BOSS_WAVE_MS;
					SpawnPlayerBotZuoBosses(ev, dwNow);
				}
			}
		}
		if (dwNow >= ev.nextCallAt && dwNow - s_dwPlayerBotWorldEventsFirstSeen >= PLAYERBOT_WORLD_EVENT_FIRST_CALL_DELAY_MS)
		{
			ev.nextCallAt = dwNow + PLAYERBOT_WORLD_EVENT_CALL_MS;
			if (kind == playerbot_events::KIND_TANAKA)
				CallPlayerBotTanakaChasers(ev, dwNow);
			else
				CallPlayerBotZuoParticipants(ev, dwNow);
		}
		if (dwNow >= ev.nextReminderAt)
		{
			ev.nextReminderAt = dwNow + PLAYERBOT_WORLD_EVENT_REMINDER_MS;
			char when[16];
			FormatPlayerBotEventClock(ev.until, when, sizeof(when));
			if (kind == playerbot_events::KIND_TANAKA)
				SayPlayerBotWorldEvent("Trwa event: Pirat Tanaka grasuje %s do %s. Pokonany juz %u razy.",
						DescribePlayerBotWorldEventMaps(ev).c_str(), when, ev.killed);
			else
				SayPlayerBotWorldEvent("Trwa event Zuo %s do %s: rozbite metiny %u, pokonani bossowie %u.",
						DescribePlayerBotWorldEventMaps(ev).c_str(), when, ev.killed, ev.bossesKilled);
		}
	}

	// Once a second for the world: each event begun, run and ended by the
	// core that drives it. Called from Update and, on a core no bot has woken
	// yet, from the world clock beside the events themselves.
	void ManagePlayerBotWorldEvents(DWORD dwNow)
	{
		if (dwNow < s_dwNextPlayerBotWorldEventCheck)
			return;
		s_dwNextPlayerBotWorldEventCheck = dwNow + PLAYERBOT_WORLD_EVENT_CHECK_MS;
		if (s_dwPlayerBotWorldEventsFirstSeen == 0)
			s_dwPlayerBotWorldEventsFirstSeen = dwNow;
		const int kinds[2] = { playerbot_events::KIND_TANAKA, playerbot_events::KIND_ZUO };
		for (int k = 0; k < 2; ++k)
		{
			const int kind = kinds[k];
			const playerbot_events::Status& st = s_aPlayerBotEventStatus[kind];
			TPlayerBotWorldEvent& ev = *GetPlayerBotWorldEvent(kind);
			const bool drive = st.active && IsPlayerBotWorldEventDriver(st.map);
			if (!drive)
			{
				if (ev.running)
					EndPlayerBotWorldEvent(kind, st.active ? "handed_over" : "over", !st.active, dwNow);
				continue;
			}
			// Another map asked for in the middle: the event starts again there.
			if (ev.running && st.map != ev.requestedMap)
				EndPlayerBotWorldEvent(kind, "map_changed", false, dwNow);
			if (!ev.running)
				BeginPlayerBotWorldEvent(kind, st, dwNow);
			else
				AdvancePlayerBotWorldEvent(kind, st, dwNow);
		}
	}

	// "host alive killed bots phase" for the status file (playerbot_events.h).
	void FormatPlayerBotWorldEventColumns(int kind, char* out, size_t size)
	{
		const TPlayerBotWorldEvent* ev = GetPlayerBotWorldEvent(kind);
		if (!ev || !ev->running)
		{
			snprintf(out, size, "0\t0\t0\t0\t-");
			return;
		}
		const char* phase = "tanaka";
		if (kind == playerbot_events::KIND_ZUO)
			phase = playerbot_events::IsZuoBossHalf(ev->since, ev->until, (long)time(NULL)) ? "bosses" : "stones";
		const unsigned int killed = kind == playerbot_events::KIND_ZUO ? ev->killed + ev->bossesKilled : ev->killed;
		snprintf(out, size, "1\t%u\t%u\t%u\t%s", (unsigned int)ev->things.size(), killed,
				(unsigned int)ev->participants.size(), phase);
	}

	// ------------------------------------------------------------------
	// A bot's part
	// ------------------------------------------------------------------

	// The thing a Zuo participant goes for: the one it has while it stands,
	// else the nearest with room for one more - a boss before a stone in the
	// second half, as the notice says.
	LPCHARACTER PickPlayerBotZuoTarget(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotWorldEvent& ev)
	{
		if (state.dwWorldEventTargetVID != 0)
		{
			for (size_t i = 0; i < ev.things.size(); ++i)
			{
				if (ev.things[i].vid != state.dwWorldEventTargetVID)
					continue;
				LPCHARACTER c = GetPlayerBotEventThing(ev.things[i]);
				if (c && !c->IsDead())
					return c;
				break;
			}
			state.dwWorldEventTargetVID = 0;
		}
		LPCHARACTER best = NULL;
		long bestScore = LONG_MAX;
		for (size_t i = 0; i < ev.things.size(); ++i)
		{
			const TPlayerBotEventThing& thing = ev.things[i];
			if (thing.map != ch->GetMapIndex())
				continue;
			std::map<DWORD, int>::const_iterator have = ev.attackers.find(thing.vid);
			const int on = have == ev.attackers.end() ? 0 : have->second;
			if (on >= (thing.boss ? PLAYERBOT_ZUO_BOSS_ATTACKERS : PLAYERBOT_ZUO_STONE_ATTACKERS))
				continue;
			LPCHARACTER c = GetPlayerBotEventThing(thing);
			if (!c || c->IsDead())
				continue;
			long score = (long)DISTANCE_APPROX(ch->GetX() - c->GetX(), ch->GetY() - c->GetY());
			if (thing.boss)
				score -= 3000;
			if (score < bestScore)
			{
				bestScore = score;
				best = c;
			}
		}
		if (best)
		{
			state.dwWorldEventTargetVID = (DWORD)best->GetVID();
			++ev.attackers[state.dwWorldEventTargetVID];
		}
		return best;
	}

	// The per-bot pass: a bot that answered an event owns its tick while it
	// has something there to go for. After the raids' hooks, so a war, the
	// tower, a boss raid and the Catacomb come first, and after the loot, so
	// a pirate's yang and ear are picked up.
	bool ManagePlayerBotWorldEvent(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (state.bWorldEventKind == 0 || !ch || ch->IsDead())
			return false;
		const DWORD pid = ch->GetPlayerID();
		TPlayerBotWorldEvent* evp = GetPlayerBotWorldEvent(state.bWorldEventKind);
		if (!evp || !evp->running || evp->participants.find(pid) == evp->participants.end())
		{
			ClearPlayerBotWorldEventState(state, ch);
			return false;
		}
		TPlayerBotWorldEvent& ev = *evp;
		// What outranks an event, and what a person asks of the bot.
		if (IsPlayerBotOnTowerBusiness(ch, state) || state.dwGuildWarEnemyGID != 0 ||
				playerbot_pvp::GetDuelOpponent(pid, dwNow) != 0)
		{
			ReleasePlayerBotWorldEventParticipant(ev, pid, "outranked");
			return false;
		}
		if ((ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty())) || IsPlayerBotSummoned(pid) ||
				IsPlayerBotOnMercContract(pid))
		{
			ReleasePlayerBotWorldEventParticipant(ev, pid, "company");
			return false;
		}
		// A fight needs a weapon, potions and room for its drops: without them
		// the bot goes to town, as it would from any hunt.
		if (BlocksPlayerBotTravel(ch))
		{
			ReleasePlayerBotWorldEventParticipant(ev, pid, "supplies");
			return false;
		}
		// A town visit under way is let finish; the event takes the bot back after.
		if (state.bTownVisitPhase != BOT_TOWN_PHASE_NONE)
			return false;

		LPCHARACTER target = NULL;
		if (ev.kind == playerbot_events::KIND_TANAKA)
		{
			for (size_t i = 0; i < ev.things.size(); ++i)
				if (ev.things[i].vid == state.dwWorldEventTargetVID)
				{
					target = GetPlayerBotEventThing(ev.things[i]);
					break;
				}
			if (!target || target->IsDead())
			{
				ReleasePlayerBotWorldEventParticipant(ev, pid, NULL);
				return false;
			}
			state.lWorldEventMap = target->GetMapIndex();
		}
		else if (ch->GetMapIndex() == state.lWorldEventMap)
			target = PickPlayerBotZuoTarget(ch, state, ev);

		// Brought to the event's map: by its thing, or where the last wave fell.
		if (ch->GetMapIndex() != state.lWorldEventMap)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			if (dwNow < state.dwNextWorldEventMoveTime)
				return true;
			state.dwNextWorldEventMoveTime = dwNow + 5000;
			long toX = 0, toY = 0;
			if (target)
			{
				toX = target->GetX();
				toY = target->GetY();
			}
			else if (ev.dropX != 0 || ev.dropY != 0)
			{
				toX = ev.dropX;
				toY = ev.dropY;
			}
			else if (!PickPlayerBotEventPoint(state.lWorldEventMap, toX, toY))
				return false;
			long rallyX = 0, rallyY = 0;
			GetPlayerBotWorldEventRally(pid, state.lWorldEventMap, toX, toY, rallyX, rallyY);
			TransitionPlayerBotMap(ch, state, state.lWorldEventMap, rallyX, rallyY, dwNow,
					ev.kind == playerbot_events::KIND_TANAKA ? "tanaka_event" : "zuo_event");
			return true;
		}
		// Between Zuo's waves the bot's own life goes on, on this map: the
		// travel pass holds it here (ManagePlayerBotWorldTravel).
		if (!target)
			return false;
		if (KeepPlayerBotTowerAlive(ch, state, dwNow))
			return true;
		const int distance = DISTANCE_APPROX(ch->GetX() - target->GetX(), ch->GetY() - target->GetY());
		if (distance > PLAYERBOT_WORLD_EVENT_WALK_MAX)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			if (dwNow < state.dwNextWorldEventMoveTime)
				return true;
			state.dwNextWorldEventMoveTime = dwNow + 5000;
			long rallyX = 0, rallyY = 0;
			GetPlayerBotWorldEventRally(pid, state.lWorldEventMap, target->GetX(), target->GetY(), rallyX, rallyY);
			TransitionPlayerBotMap(ch, state, state.lWorldEventMap, rallyX, rallyY, dwNow,
					ev.kind == playerbot_events::KIND_TANAKA ? "tanaka_event_far" : "zuo_event_far");
			return true;
		}
		// What hits the bot on the way is answered on the way.
		LPCHARACTER engaged = FindPlayerBotEngagedTarget(ch, &state, dwNow);
		if (engaged && engaged != target && !engaged->IsDead() &&
				DISTANCE_APPROX(ch->GetX() - engaged->GetX(), ch->GetY() - engaged->GetY()) <= 1200)
			return FightPlayerBotTowerObjective(ch, state, engaged, dwNow);
		return FightPlayerBotTowerObjective(ch, state, target, dwNow);
	}

	// The line over a bot's head (playerbot_status.h).
	bool DescribePlayerBotWorldEvent(LPCHARACTER ch, const TPlayerBotAIState& state, bool en,
			char* out, size_t size)
	{
		if (!ch || state.bWorldEventKind == 0)
			return false;
		const TPlayerBotEventMap* row = GetPlayerBotEventMap(state.lWorldEventMap);
		const char* place = row ? (en ? row->szNameEn : row->szName) : "";
		if (ch->GetMapIndex() != state.lWorldEventMap)
		{
			if (state.bWorldEventKind == playerbot_events::KIND_TANAKA)
				snprintf(out, size, PBT(en, "Ide na Pirata Tanake (%s)", "Going after Pirate Tanaka (%s)"), place);
			else
				snprintf(out, size, PBT(en, "Ide na event Zuo (%s)", "Going to the Zuo event (%s)"), place);
			return true;
		}
		if (state.bWorldEventKind == playerbot_events::KIND_TANAKA)
		{
			snprintf(out, size, PBT(en, "Gonie Pirata Tanake (%s)", "Chasing Pirate Tanaka (%s)"), place);
			return true;
		}
		if (state.dwWorldEventTargetVID == 0)
		{
			snprintf(out, size, "%s", PBT(en, "Event Zuo: poluje miedzy falami", "Zuo event: hunting between waves"));
			return true;
		}
		LPCHARACTER target = CHARACTER_MANAGER::instance().Find(state.dwWorldEventTargetVID);
		if (target && target->IsStone())
			snprintf(out, size, "%s", PBT(en, "Event Zuo: rozbijam metina", "Zuo event: breaking a Metin stone"));
		else
			snprintf(out, size, "%s", PBT(en, "Event Zuo: walcze z bossem", "Zuo event: fighting a boss"));
		return true;
	}

	// ------------------------------------------------------------------
	// Yonah and the ears
	// ------------------------------------------------------------------

#if defined(PLAYERBOT_ENGINE_MT2009)
	// Yonah (20005), in each first village: an ear for a Purple Ebony Chest,
	// the exchange tanaka_ears.quest makes for a player, made here the way the
	// Alchemist's is - walked to, a few seconds at the NPC, then the swap. A
	// bot with ears in its bag makes it whenever it is in its first village;
	// the chest pass opens the chests.
	bool ManagePlayerBotTanakaEars(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || state.bVisitingShop || state.bVisitingBiologist ||
				state.bVisitingHerbalist || state.bVisitingStable || state.bVisitingAlchemist ||
				state.bFishingSession || state.bWorldEventKind != 0)
			return false;
		if (ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()))
		{
			state.bVisitingYonah = false;
			return false;
		}
		if (!state.bVisitingYonah && dwNow < state.dwNextYonahCheckTime)
			return false;
		playerbot_empire_rules::TPoint yonahPos;
		if (!playerbot_empire_rules::GetYonah(ch->GetMapIndex(), yonahPos))
		{
			state.bVisitingYonah = false;
			state.dwNextYonahCheckTime = dwNow + number(PLAYERBOT_YONAH_CHECK_MIN_MS, PLAYERBOT_YONAH_CHECK_MAX_MS);
			return false;
		}
		if (!state.bVisitingYonah)
		{
			const int ears = (int)ch->CountSpecifyItem(PLAYERBOT_TANAKA_EAR_VNUM);
			if (ears <= 0 || CountPlayerBotFreeInventoryCells(ch) < 2)
			{
				state.dwNextYonahCheckTime = dwNow + number(PLAYERBOT_YONAH_CHECK_MIN_MS, PLAYERBOT_YONAH_CHECK_MAX_MS);
				return false;
			}
			state.bVisitingYonah = true;
			state.dwNextYonahActionTime = 0;
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ch->Stop();
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_EVENT: going to Yonah pid=%u name=%s ears=%d", ch->GetPlayerID(), ch->GetName(), ears);
		}

		SetPlayerBotAction(state, BOT_ACTION_SHOP, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		long approachX = 0, approachY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), yonahPos.x, yonahPos.y, 0x594F4E41U, approachX, approachY);
		if (DISTANCE_APPROX(ch->GetX() - approachX, ch->GetY() - approachY) > 650)
		{
			if (!MovePlayerBot(ch, approachX, approachY, dwNow, 20, true, true, false, true) &&
					state.bStuckCounter >= 6)
			{
				state.bVisitingYonah = false;
				state.dwNextYonahCheckTime = dwNow + number(PLAYERBOT_YONAH_CHECK_MIN_MS, PLAYERBOT_YONAH_CHECK_MAX_MS);
				ClearPlayerBotRoute(state, true);
				sys_err("PLAYERBOT_EVENT: route to Yonah failed pid=%u name=%s from=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetX(), ch->GetY());
				return false;
			}
			return true;
		}

		ch->Stop();
		ch->SetPosition(POS_STANDING);
		if (state.dwNextYonahActionTime == 0)
		{
			state.dwNextYonahActionTime = dwNow + number(3000, 8000);
			return true;
		}
		if (dwNow < state.dwNextYonahActionTime)
			return true;

		// At Yonah: every ear the bag has room for, a chest a cell, one cell
		// kept free - counted again, the bag may have changed on the walk.
		const int ears = (int)ch->CountSpecifyItem(PLAYERBOT_TANAKA_EAR_VNUM);
		const int room = CountPlayerBotFreeInventoryCells(ch) - 1;
		const int n = std::min(std::min(ears, room), PLAYERBOT_YONAH_EARS_PER_VISIT);
		int given = 0;
		if (n > 0)
		{
			ch->RemoveSpecifyItem(PLAYERBOT_TANAKA_EAR_VNUM, n);
			for (int i = 0; i < n; ++i)
				if (ch->AutoGiveItem(PLAYERBOT_PURPLE_EBONY_CHEST_VNUM, 1, -1, false))
					++given;
			sys_log(0, "PLAYERBOT_EVENT: Yonah took ears pid=%u name=%s ears=%d chests=%d left=%d",
					ch->GetPlayerID(), ch->GetName(), n, given, ears - n);
		}
		state.bVisitingYonah = false;
		state.dwNextYonahActionTime = 0;
		state.dwNextYonahCheckTime = dwNow + number(PLAYERBOT_YONAH_CHECK_MIN_MS, PLAYERBOT_YONAH_CHECK_MAX_MS);
		ClearPlayerBotRoute(state, true);
		return given > 0;
	}
#else
	// r40250 drops no ear (the edit that makes one is playerbotify's) and has
	// no quest at Yonah to take it.
	bool ManagePlayerBotTanakaEars(LPCHARACTER, TPlayerBotAIState&, DWORD) { return false; }
#endif
}

#endif
