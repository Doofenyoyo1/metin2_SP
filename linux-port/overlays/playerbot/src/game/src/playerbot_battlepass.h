// MT2009 PLUS Battle Pass (the operator, 28 September: "battle pass z oknem i
// przyciskiem w gui, boty nie korzystaja, zamiast biletu - nagroda kupon SM
// 50, postep w bazie"; the missions are set afterwards).
//
// No ticket: every player takes part, and the bots too (MT2009_PLUS_BP_BOTS_V1,
// the operator, 28 September: "integracja botow z battlepassem ... boty maja
// dazyc do tego, aby zamknac wszystkie misje"): their kills, fish and shouts
// count like anybody's, a reward goes into the bot's bag, and the season's
// final reward is taken the moment the last mission is done - a bot has no
// window. What a bot sets out to do about a mission is playerbot_bpbots.h. A season is a calendar
// month of the server's clock (YYYYMM), so progress starts again on the
// first; nothing has to be deleted. Everything lives in the database:
//   player.battlepass_mission  - the missions, the operator's to fill: type,
//                                target (0 = any), the target's level (0 =
//                                any), the mission that must be done first
//                                (0 = none), count, up to three rewards
//                                of its own (given the moment the mission is
//                                done), an optional name and description (raw
//                                CP1250; an empty name is worded by the client
//                                from the type, "//" breaks a description);
//   player.battlepass_progress - pid, season, mission, progress, claimed;
//                                mission 0 is the season's final reward.
// Finishing every mission of the season gives the final reward: up to three
// items of player.battlepass_config, a Kupon SM (50) - 80017 - at first.
// The advanced panel (Seban Panel, Battle Pass page) edits both tables; the
// game reads them again every 30 seconds.
//
// Mission types (the window words them, uibattlepass.py):
//   1 monsters killed   2 Metin stones   3 bosses        4 fish caught
//   5 refines that took 6 yang picked up 7 chests opened 8 herbs picked
//   9 ore mined        10 dungeons done 11 mission books 12 minutes played
//  13 items used (target = the vnum)
//  14 shout messages (any text on the shout channel, a bot's too)
// 1-3 and 13 honour the target vnum; a companion's kill is its owner's, as
// for the quests. The engine calls in through server-patches/playerqol
// (MT2009_PLUS_BATTLE_PASS_V1): the kill, AddPlayerStat, UseItem and the
// /battlepass command.
//
// The database is the truth. A player's character moves between cores (each
// hosts other maps), so a core never writes a total: it counts in memory and
// adds its count to the row (progress = progress + delta) every minute, and
// at once when a mission may be done; the window reads the rows again. A
// reward is taken with a conditional UPDATE, so no two cores give it.
// The bots' side of the Battle Pass (playerbot_bpbots.h, included by the
// manager after every fragment it asks about), declared here for the bot code
// that asks it first: the stone band, the target score, the frontier draw,
// the angler and the boss raid's call (MT2009_PLUS_BP_BOTS_V1).
namespace playerbot_bpbots
{
	// A stone this bot has a Battle Pass mission for, and can break.
	bool WantsStone(LPCHARACTER ch, LPCHARACTER stone);
	// What a candidate is worth on top of the targeting's own score.
	int TargetBonus(LPCHARACTER ch, LPCHARACTER candidate);
	// A Battle Pass errand's map: true with a frontier map, or with 0 for the
	// bot's own villages; false when no errand names one.
	bool GetErrandMap(LPCHARACTER ch, long& map);
	// Gone fishing for a Battle Pass mission.
	bool WantsFishing(LPCHARACTER ch);
	// Extra weight for the boss raid's call.
	int BossRecruitBonus(DWORD pid, DWORD race);
	// A mission of this bot's was just settled: look again soon.
	void OnProgressSettled(DWORD pid);
	// On a Battle Pass errand now: no bot party for it meanwhile.
	bool IsOnErrand(DWORD pid);
}

namespace mt2009_battlepass
{
	enum EType
	{
		TYPE_MONSTER = 1,
		TYPE_METIN = 2,
		TYPE_BOSS = 3,
		TYPE_FISH = 4,
		TYPE_REFINE = 5,
		TYPE_YANG = 6,
		TYPE_CHEST = 7,
		TYPE_HERB = 8,
		TYPE_MINING = 9,
		TYPE_DUNGEON = 10,
		TYPE_QUESTBOOK = 11,
		TYPE_PLAYTIME = 12,
		TYPE_USE_ITEM = 13,
		// Every message a character sends on the shout channel, whatever it
		// says (MT2009_PLUS_BATTLE_PASS_V1 (shout), input_main.cpp); a bot's
		// own shouts count too (playerbot_bpbots.h: "!BP" when it has nothing
		// else to say).
		TYPE_SHOUT = 14,
		TYPE_LAST = TYPE_SHOUT,
	};

	// The season's final reward: up to three items, player.battlepass_config
	// (the advanced panel's Battle Pass page); a Kupon SM (50) - 80017 - until
	// the operator sets another.
	const DWORD FINAL_REWARD_VNUM = 80017;
	const DWORD FINAL_REWARD_COUNT = 1;
	// The panel's changes reach the game within this.
	const DWORD MISSIONS_RELOAD_MS = 30 * 1000;

	struct Mission
	{
		DWORD id;
		BYTE type;
		DWORD target;
		DWORD count;
		DWORD rewardVnum[3];
		DWORD rewardCount[3];
		std::string nameHex;
		std::string descHex;
		// Only a victim of this level counts (0 = any): "Metins of level 25".
		DWORD targetLevel;
		// Locked, and counting nothing, until this mission is done (0 = none):
		// a chain such as 2000, 5000, 10000 monsters.
		DWORD requiredId;
	};

	// What this core knows of one mission of one player: the total the
	// database had at the last read, and what this core counted since. The
	// database is the truth - a player's characters move between cores (every
	// core hosts other maps), and each core only ever adds its own count to it
	// (progress = progress + delta), never writes a total over another core's.
	struct Progress
	{
		DWORD value;
		DWORD delta;
		bool claimed;
		Progress() : value(0), delta(0), claimed(false) {}
	};

	struct Cache
	{
		DWORD season;
		std::map<DWORD, Progress> missions;
		Progress final;
		Cache() : season(0) {}
	};

	std::vector<Mission> s_vecMissions;
	DWORD s_adwFinalVnum[3] = { FINAL_REWARD_VNUM, 0, 0 };
	DWORD s_adwFinalCount[3] = { FINAL_REWARD_COUNT, 0, 0 };
	DWORD s_dwMissionsLoadedAt = 0;
	bool s_bMissionsLoaded = false;
	bool s_bTables = false;
	std::map<DWORD, Cache> s_mapCaches;
	LPEVENT s_pkTick = NULL;

	DWORD CurrentSeason()
	{
		const time_t now = time(NULL);
		struct tm local;
		localtime_r(&now, &local);
		return (DWORD)((local.tm_year + 1900) * 100 + local.tm_mon + 1);
	}

	int DaysLeft()
	{
		const time_t now = time(NULL);
		struct tm local;
		localtime_r(&now, &local);
		struct tm next = local;
		next.tm_mday = 1;
		next.tm_hour = next.tm_min = next.tm_sec = 0;
		next.tm_mon += 1;
		next.tm_isdst = -1;
		const time_t end = mktime(&next);
		return end > now ? (int)((end - now + 86399) / 86400) : 0;
	}

	// Who opens the window and gives the commands: a player.
	bool Eligible(LPCHARACTER ch)
	{
		return ch && ch->IsPC() && ch->GetDesc() && !ch->GetDesc()->IsBot();
	}

	bool IsBot(LPCHARACTER ch)
	{
		return ch && ch->GetDesc() && ch->GetDesc()->IsBot();
	}

	// Whose deeds count: a player's and a bot's alike (MT2009_PLUS_BP_BOTS_V1).
	bool Counts(LPCHARACTER ch)
	{
		return ch && ch->IsPC() && ch->GetDesc();
	}

	bool EnsureTables()
	{
		if (s_bTables)
			return true;
		std::unique_ptr<SQLMsg> m(AccountDB::instance().DirectQuery(
				"CREATE TABLE IF NOT EXISTS player.battlepass_mission ("
				"id INT UNSIGNED NOT NULL PRIMARY KEY, "
				"type TINYINT UNSIGNED NOT NULL, "
				"target INT UNSIGNED NOT NULL DEFAULT 0, "
				"count INT UNSIGNED NOT NULL DEFAULT 1, "
				"reward_vnum INT UNSIGNED NOT NULL DEFAULT 0, "
				"reward_count INT UNSIGNED NOT NULL DEFAULT 0, "
				"name VARBINARY(96) NOT NULL DEFAULT '', "
				"active TINYINT UNSIGNED NOT NULL DEFAULT 1) ENGINE=InnoDB"));
		std::unique_ptr<SQLMsg> more(AccountDB::instance().DirectQuery(
				"ALTER TABLE player.battlepass_mission "
				"ADD COLUMN IF NOT EXISTS reward2_vnum INT UNSIGNED NOT NULL DEFAULT 0 AFTER reward_count, "
				"ADD COLUMN IF NOT EXISTS reward2_count INT UNSIGNED NOT NULL DEFAULT 0 AFTER reward2_vnum, "
				"ADD COLUMN IF NOT EXISTS reward3_vnum INT UNSIGNED NOT NULL DEFAULT 0 AFTER reward2_count, "
				"ADD COLUMN IF NOT EXISTS reward3_count INT UNSIGNED NOT NULL DEFAULT 0 AFTER reward3_vnum, "
				"ADD COLUMN IF NOT EXISTS description VARBINARY(255) NOT NULL DEFAULT '' AFTER name, "
				"ADD COLUMN IF NOT EXISTS target_level INT UNSIGNED NOT NULL DEFAULT 0 AFTER target, "
				"ADD COLUMN IF NOT EXISTS requires_id INT UNSIGNED NOT NULL DEFAULT 0 AFTER active"));
		std::unique_ptr<SQLMsg> p(AccountDB::instance().DirectQuery(
				"CREATE TABLE IF NOT EXISTS player.battlepass_progress ("
				"pid INT UNSIGNED NOT NULL, "
				"season INT UNSIGNED NOT NULL, "
				"mission INT UNSIGNED NOT NULL, "
				"progress INT UNSIGNED NOT NULL DEFAULT 0, "
				"claimed TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"PRIMARY KEY (pid, season, mission)) ENGINE=InnoDB"));
		std::unique_ptr<SQLMsg> config(AccountDB::instance().DirectQuery(
				"CREATE TABLE IF NOT EXISTS player.battlepass_config ("
				"id TINYINT UNSIGNED NOT NULL PRIMARY KEY, "
				"final1_vnum INT UNSIGNED NOT NULL DEFAULT 0, final1_count INT UNSIGNED NOT NULL DEFAULT 0, "
				"final2_vnum INT UNSIGNED NOT NULL DEFAULT 0, final2_count INT UNSIGNED NOT NULL DEFAULT 0, "
				"final3_vnum INT UNSIGNED NOT NULL DEFAULT 0, final3_count INT UNSIGNED NOT NULL DEFAULT 0) ENGINE=InnoDB"));
		std::unique_ptr<SQLMsg> configRow(AccountDB::instance().DirectQuery(
				"INSERT IGNORE INTO player.battlepass_config (id, final1_vnum, final1_count) VALUES (1, 80017, 1)"));
		s_bTables = m.get() && m->uiSQLErrno == 0 && p.get() && p->uiSQLErrno == 0;
		if (!s_bTables)
		{
			sys_err("BATTLEPASS: no tables errno=%u/%u", m.get() ? m->uiSQLErrno : 0U, p.get() ? p->uiSQLErrno : 0U);
			return false;
		}
		// The season's missions until the operator writes their own (only into an
		// empty table, once): Metins of level 25 to 50 (8005-8010), monsters,
		// fish, refines and shouts (1000, 2000, 10000 messages on the shout
		// channel) as chains - each unlocks when the one before is done -
		// bosses and any Metins; 5 Cor Draconis and a 50 SM coupon for each.
		std::unique_ptr<SQLMsg> seeded(AccountDB::instance().DirectQuery(
				"INSERT INTO player.battlepass_mission (id, type, target, count, reward_vnum, reward_count, "
				"reward2_vnum, reward2_count, requires_id) "
				"SELECT t.id, t.type, t.target, t.count, 50255, 5, 80017, 1, t.req FROM ("
				"SELECT 1 AS id, 2 AS type, 8005 AS target, 10 AS count, 0 AS req"
				" UNION ALL SELECT 2, 2, 8006, 10, 1"
				" UNION ALL SELECT 3, 2, 8007, 10, 2"
				" UNION ALL SELECT 4, 2, 8008, 20, 3"
				" UNION ALL SELECT 5, 2, 8009, 20, 4"
				" UNION ALL SELECT 6, 2, 8010, 30, 5"
				" UNION ALL SELECT 7, 1, 0, 2000, 0"
				" UNION ALL SELECT 8, 1, 0, 5000, 7"
				" UNION ALL SELECT 9, 1, 0, 10000, 8"
				" UNION ALL SELECT 10, 4, 0, 10, 0"
				" UNION ALL SELECT 11, 4, 0, 30, 10"
				" UNION ALL SELECT 12, 4, 0, 50, 11"
				" UNION ALL SELECT 13, 3, 0, 20, 0"
				" UNION ALL SELECT 14, 2, 0, 200, 0"
				" UNION ALL SELECT 15, 5, 0, 50, 0"
				" UNION ALL SELECT 16, 5, 0, 100, 15"
				" UNION ALL SELECT 17, 14, 0, 1000, 0"
				" UNION ALL SELECT 18, 14, 0, 2000, 17"
				" UNION ALL SELECT 19, 14, 0, 10000, 18"
				") AS t WHERE NOT EXISTS (SELECT 1 FROM player.battlepass_mission)"));
		return true;
	}

	std::string HexOf(const char* data, size_t len)
	{
		static const char* const digits = "0123456789abcdef";
		std::string out;
		for (size_t i = 0; i < len; ++i)
		{
			const unsigned char c = (unsigned char)data[i];
			out += digits[c >> 4];
			out += digits[c & 15];
		}
		return out;
	}

	void LoadMissions(bool force)
	{
		const DWORD now = get_dword_time();
		if (!force && s_bMissionsLoaded && now - s_dwMissionsLoadedAt < MISSIONS_RELOAD_MS)
			return;
		s_dwMissionsLoadedAt = now;
		if (!EnsureTables())
			return;
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(
				"SELECT id, type, target, count, reward_vnum, reward_count, reward2_vnum, reward2_count, "
				"reward3_vnum, reward3_count, name, description, target_level, requires_id FROM player.battlepass_mission "
				"WHERE active <> 0 ORDER BY id"));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return;
		std::vector<Mission> fresh;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			unsigned long* lengths = mysql_fetch_lengths(msg->Get()->pSQLResult);
			Mission m;
			unsigned int type = 0;
			str_to_number(m.id, row[0]);
			str_to_number(type, row[1]);
			str_to_number(m.target, row[2]);
			str_to_number(m.count, row[3]);
			for (int r = 0; r < 3; ++r)
			{
				m.rewardVnum[r] = m.rewardCount[r] = 0;
				str_to_number(m.rewardVnum[r], row[4 + r * 2]);
				str_to_number(m.rewardCount[r], row[5 + r * 2]);
			}
			m.type = (BYTE)type;
			if (m.id == 0 || m.count == 0 || m.type < TYPE_MONSTER || m.type > TYPE_LAST)
				continue;
			if (row[10] && lengths && lengths[10] > 0)
				m.nameHex = HexOf(row[10], lengths[10]);
			if (row[11] && lengths && lengths[11] > 0)
				m.descHex = HexOf(row[11], lengths[11]);
			m.targetLevel = m.requiredId = 0;
			str_to_number(m.targetLevel, row[12]);
			str_to_number(m.requiredId, row[13]);
			fresh.push_back(m);
		}
		if (!s_bMissionsLoaded || fresh.size() != s_vecMissions.size())
			sys_log(0, "BATTLEPASS: %u missions", (unsigned int)fresh.size());
		std::unique_ptr<SQLMsg> config(AccountDB::instance().DirectQuery(
				"SELECT final1_vnum, final1_count, final2_vnum, final2_count, final3_vnum, final3_count "
				"FROM player.battlepass_config WHERE id=1"));
		if (config.get() && config->uiSQLErrno == 0 && config->Get() && config->Get()->pSQLResult)
		{
			MYSQL_ROW crow = mysql_fetch_row(config->Get()->pSQLResult);
			if (crow)
				for (int r = 0; r < 3; ++r)
				{
					s_adwFinalVnum[r] = s_adwFinalCount[r] = 0;
					str_to_number(s_adwFinalVnum[r], crow[r * 2]);
					str_to_number(s_adwFinalCount[r], crow[r * 2 + 1]);
				}
		}
		s_vecMissions.swap(fresh);
		s_bMissionsLoaded = true;
	}

	const Mission* FindMission(DWORD id)
	{
		for (size_t i = 0; i < s_vecMissions.size(); ++i)
			if (s_vecMissions[i].id == id)
				return &s_vecMissions[i];
		return NULL;
	}

	// The player's rows of this season, read again; this core's own counts
	// not yet written stay on top of them.
	void ReadProgress(DWORD pid, Cache& cache)
	{
		for (std::map<DWORD, Progress>::iterator it = cache.missions.begin(); it != cache.missions.end(); ++it)
		{
			it->second.value = 0;
			it->second.claimed = false;
		}
		cache.final.value = 0;
		cache.final.claimed = false;
		char query[160];
		snprintf(query, sizeof(query),
				"SELECT mission, progress, claimed FROM player.battlepass_progress WHERE pid=%u AND season=%u",
				pid, cache.season);
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			DWORD mission = 0, progress = 0, claimed = 0;
			str_to_number(mission, row[0]);
			str_to_number(progress, row[1]);
			str_to_number(claimed, row[2]);
			Progress& p = mission == 0 ? cache.final : cache.missions[mission];
			p.value = progress;
			p.claimed = claimed != 0;
		}
	}

	// Every bot on this core with no rows read for the season, read at once:
	// hundreds of bots would otherwise each ask the database on their first
	// kill (MT2009_PLUS_BP_BOTS_V1). A few hundred pids to a query.
	void ReadBotCaches(DWORD season)
	{
		std::vector<DWORD> pids;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			std::map<DWORD, Cache>::const_iterator have = s_mapCaches.find(it->first);
			if (have != s_mapCaches.end() && have->second.season == season)
				continue;
			if (CHARACTER_MANAGER::instance().FindByPID(it->first))
				pids.push_back(it->first);
		}
		const size_t CHUNK = 300;
		for (size_t first = 0; first < pids.size(); first += CHUNK)
		{
			const size_t last = std::min(pids.size(), first + CHUNK);
			std::string query = "SELECT pid, mission, progress, claimed FROM player.battlepass_progress WHERE season=";
			char number[24];
			snprintf(number, sizeof(number), "%u AND pid IN (", season);
			query += number;
			for (size_t i = first; i < last; ++i)
			{
				Cache& cache = s_mapCaches[pids[i]];
				// This core's counts not yet written stay (a new season has none).
				if (cache.season != season)
				{
					cache = Cache();
					cache.season = season;
				}
				snprintf(number, sizeof(number), i + 1 < last ? "%u," : "%u)", pids[i]);
				query += number;
			}
			std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query.c_str()));
			if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
				continue;
			MYSQL_ROW row;
			while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
			{
				DWORD pid = 0, mission = 0, progress = 0, claimed = 0;
				str_to_number(pid, row[0]);
				str_to_number(mission, row[1]);
				str_to_number(progress, row[2]);
				str_to_number(claimed, row[3]);
				std::map<DWORD, Cache>::iterator c = s_mapCaches.find(pid);
				if (c == s_mapCaches.end())
					continue;
				Progress& p = mission == 0 ? c->second.final : c->second.missions[mission];
				p.value = progress;
				p.claimed = claimed != 0;
			}
		}
	}

	Cache& GetCache(LPCHARACTER ch)
	{
		const DWORD pid = ch->GetPlayerID();
		const DWORD season = CurrentSeason();
		std::map<DWORD, Cache>::iterator it = s_mapCaches.find(pid);
		if (it != s_mapCaches.end() && it->second.season == season)
			return it->second;
		if (IsBot(ch) && EnsureTables())
		{
			ReadBotCaches(season);
			it = s_mapCaches.find(pid);
			if (it != s_mapCaches.end() && it->second.season == season)
				return it->second;
		}
		Cache& cache = s_mapCaches[pid];
		cache = Cache();
		cache.season = season;
		if (EnsureTables())
			ReadProgress(pid, cache);
		return cache;
	}

	// This core's counts into the database, added to what is there (capped at
	// the mission's count). Synchronous when a decision hangs on the result.
	void WriteDeltas(DWORD pid, Cache& cache, bool now)
	{
		for (std::map<DWORD, Progress>::iterator it = cache.missions.begin(); it != cache.missions.end(); ++it)
		{
			Progress& p = it->second;
			if (!p.delta)
				continue;
			const Mission* m = FindMission(it->first);
			const DWORD cap = m ? m->count : 0xFFFFFFFFu;
			char query[320];
			snprintf(query, sizeof(query),
					"INSERT INTO player.battlepass_progress (pid, season, mission, progress, claimed) "
					"VALUES (%u, %u, %u, LEAST(%u, %u), 0) "
					"ON DUPLICATE KEY UPDATE progress = LEAST(%u, progress + %u)",
					pid, cache.season, it->first, cap, p.delta, cap, p.delta);
			if (now)
			{
				std::unique_ptr<SQLMsg> done(AccountDB::instance().DirectQuery(query));
			}
			else
				DBManager::instance().Query("%s", query);
			p.value = std::min<DWORD>(cap, p.value + p.delta);
			p.delta = 0;
		}
	}

	// Every count of this core into the database in a few statements, each
	// row added to what is there and capped at its mission's count: with
	// hundreds of bots counting, a statement per row was thousands a minute
	// (MT2009_PLUS_BP_BOTS_V1). DBManager::Query holds 4096 bytes.
	void WriteAllDeltas()
	{
		std::string caps = "CASE mission";
		for (size_t i = 0; i < s_vecMissions.size(); ++i)
		{
			char when[48];
			snprintf(when, sizeof(when), " WHEN %u THEN %u", s_vecMissions[i].id, s_vecMissions[i].count);
			caps += when;
		}
		caps += " ELSE 4294967295 END";
		if (caps.size() > 2000)
			caps = "4294967295";
		const std::string head = "INSERT INTO player.battlepass_progress (pid, season, mission, progress, claimed) VALUES ";
		const std::string tail = " ON DUPLICATE KEY UPDATE progress = LEAST(" + caps + ", progress + VALUES(progress))";
		const size_t room = 4000 - head.size() - tail.size();
		std::string values;
		unsigned int statements = 0, rows = 0;
		for (std::map<DWORD, Cache>::iterator c = s_mapCaches.begin(); c != s_mapCaches.end(); ++c)
			for (std::map<DWORD, Progress>::iterator it = c->second.missions.begin(); it != c->second.missions.end(); ++it)
			{
				Progress& p = it->second;
				if (!p.delta)
					continue;
				const Mission* m = FindMission(it->first);
				const DWORD cap = m ? m->count : 0xFFFFFFFFu;
				char row[80];
				snprintf(row, sizeof(row), "%s(%u,%u,%u,%u,0)", values.empty() ? "" : ",",
						c->first, c->second.season, it->first, std::min<DWORD>(cap, p.delta));
				if (!values.empty() && values.size() + strlen(row) > room)
				{
					DBManager::instance().Query("%s", (head + values + tail).c_str());
					++statements;
					values.clear();
					snprintf(row, sizeof(row), "(%u,%u,%u,%u,0)", c->first, c->second.season, it->first,
							std::min<DWORD>(cap, p.delta));
				}
				values += row;
				++rows;
				p.value = std::min<DWORD>(cap, p.value + p.delta);
				p.delta = 0;
			}
		if (!values.empty())
		{
			DBManager::instance().Query("%s", (head + values + tail).c_str());
			++statements;
		}
		if (rows)
			sys_log(1, "BATTLEPASS: %u counts written in %u statements", rows, statements);
	}

	// Marks a reward as taken in the database, once: the core whose UPDATE
	// changes the row gives it, a second core (or a second click) finds it
	// taken. mission 0 is the season's final reward.
	bool TakeClaim(DWORD pid, DWORD season, DWORD mission)
	{
		char query[256];
		snprintf(query, sizeof(query),
				"INSERT IGNORE INTO player.battlepass_progress (pid, season, mission, progress, claimed) "
				"VALUES (%u, %u, %u, 0, 0)", pid, season, mission);
		std::unique_ptr<SQLMsg> ensure(AccountDB::instance().DirectQuery(query));
		snprintf(query, sizeof(query),
				"UPDATE player.battlepass_progress SET claimed=1 WHERE pid=%u AND season=%u AND mission=%u AND claimed=0",
				pid, season, mission);
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		return msg.get() && msg->uiSQLErrno == 0 && msg->Get() && msg->Get()->uiAffectedRows == 1;
	}

	bool AllDone(Cache& cache)
	{
		if (s_vecMissions.empty())
			return false;
		for (size_t i = 0; i < s_vecMissions.size(); ++i)
		{
			const Mission& m = s_vecMissions[i];
			std::map<DWORD, Progress>::const_iterator it = cache.missions.find(m.id);
			if (it == cache.missions.end() || it->second.value + it->second.delta < m.count)
				return false;
		}
		return true;
	}

	void EnsureTick();
	bool GiveFinal(LPCHARACTER ch, Cache& cache);

	// A mission whose required one is not done yet; a requirement that is not
	// an active mission locks nothing.
	bool IsLocked(Cache& cache, const Mission& m)
	{
		if (!m.requiredId || m.requiredId == m.id)
			return false;
		const Mission* req = FindMission(m.requiredId);
		if (!req)
			return false;
		const Progress& p = cache.missions[req->id];
		return p.value + p.delta < req->count;
	}

	bool HasReward(const Mission& m)
	{
		return m.rewardVnum[0] || m.rewardVnum[1] || m.rewardVnum[2];
	}

	// The database brought up to date for this player, then every mission
	// done and not yet rewarded rewarded - once, whichever core gets there.
	void Settle(LPCHARACTER ch, Cache& cache, bool announce)
	{
		const DWORD pid = ch->GetPlayerID();
		WriteDeltas(pid, cache, true);
		ReadProgress(pid, cache);
		for (size_t i = 0; i < s_vecMissions.size(); ++i)
		{
			const Mission& m = s_vecMissions[i];
			Progress& p = cache.missions[m.id];
			if (p.value < m.count || p.claimed)
				continue;
			if (!TakeClaim(pid, cache.season, m.id))
			{
				p.claimed = true;
				continue;
			}
			p.claimed = true;
			if (announce)
				ch->ChatPacket(CHAT_TYPE_INFO, "Battle Pass: misja ukonczona!%s",
						HasReward(m) ? " Nagroda trafila do ekwipunku." : "");
			for (int r = 0; r < 3; ++r)
				if (m.rewardVnum[r])
					ch->AutoGiveItem(m.rewardVnum[r], (ITEM_COUNT)std::max<DWORD>(1, m.rewardCount[r]));
			sys_log(0, "BATTLEPASS: %s%s done mission %u", IsBot(ch) ? "bot " : "", ch->GetName(), m.id);
			if (announce && AllDone(cache) && !cache.final.claimed)
				ch->ChatPacket(CHAT_TYPE_INFO, "Battle Pass: wszystkie misje sezonu ukonczone - odbierz nagrode koncowa!");
		}
		// A bot has no window to press: the final reward is its own the moment
		// the season's last mission is done, and it looks again at what is left
		// (playerbot_bpbots.h) (MT2009_PLUS_BP_BOTS_V1).
		if (IsBot(ch))
		{
			if (AllDone(cache) && !cache.final.claimed)
				GiveFinal(ch, cache);
			playerbot_bpbots::OnProgressSettled(pid);
		}
	}

	void Add(LPCHARACTER ch, BYTE type, DWORD target, long long amount, DWORD level = 0)
	{
		if (amount <= 0 || !Counts(ch))
			return;
		LoadMissions(false);
		bool any = false;
		for (size_t i = 0; i < s_vecMissions.size() && !any; ++i)
			any = s_vecMissions[i].type == type;
		if (!any)
			return;
		EnsureTick();
		Cache& cache = GetCache(ch);
		// Locks as they were before this count: the kill that finishes a mission
		// does not count for the next one in its chain too.
		std::vector<bool> locked(s_vecMissions.size());
		for (size_t i = 0; i < s_vecMissions.size(); ++i)
			locked[i] = IsLocked(cache, s_vecMissions[i]);
		bool reached = false;
		for (size_t i = 0; i < s_vecMissions.size(); ++i)
		{
			const Mission& m = s_vecMissions[i];
			if (m.type != type || (m.target != 0 && m.target != target) ||
					(m.targetLevel != 0 && m.targetLevel != level) || locked[i])
				continue;
			Progress& p = cache.missions[m.id];
			const DWORD have = p.value + p.delta;
			if (have >= m.count)
				continue;
			const long long room = (long long)(m.count - have);
			p.delta += (DWORD)(amount >= room ? room : amount);
			if (p.value + p.delta >= m.count)
				reached = true;
		}
		if (reached)
		{
			const bool player = Eligible(ch);
			Settle(ch, cache, player);
			if (player)
				ch->ChatPacket(CHAT_TYPE_COMMAND, "BPUpdate");
		}
	}

	EVENTINFO(battlepass_tick_info)
	{
		int dummy;
	};

	// Every minute: a minute played for everybody in the game on this core,
	// this core's counts written, the departed forgotten, the missions re-read.
	EVENTFUNC(battlepass_tick)
	{
		LoadMissions(false);
		bool playtime = false;
		for (size_t i = 0; i < s_vecMissions.size() && !playtime; ++i)
			playtime = s_vecMissions[i].type == TYPE_PLAYTIME;
		if (playtime)
		{
			const DESC_MANAGER::DESC_SET& descs = DESC_MANAGER::instance().GetClientSet();
			for (DESC_MANAGER::DESC_SET::const_iterator it = descs.begin(); it != descs.end(); ++it)
			{
				LPDESC d = *it;
				LPCHARACTER ch = d ? d->GetCharacter() : NULL;
				if (ch && Eligible(ch) && ch->GetSectree())
					Add(ch, TYPE_PLAYTIME, 0, 1);
			}
			// And the bots on this core (MT2009_PLUS_BP_BOTS_V1).
			for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
					it != s_mapPlayerBotAIStates.end(); ++it)
			{
				LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
				if (ch && IsBot(ch) && ch->GetSectree())
					Add(ch, TYPE_PLAYTIME, 0, 1);
			}
		}
		// Every count in a few statements (hundreds of bots count too).
		WriteAllDeltas();
		for (std::map<DWORD, Cache>::iterator it = s_mapCaches.begin(); it != s_mapCaches.end(); )
		{
			if (!CHARACTER_MANAGER::instance().FindByPID(it->first))
				s_mapCaches.erase(it++);
			else
				++it;
		}
		return PASSES_PER_SEC(60);
	}

	void EnsureTick()
	{
		if (s_pkTick)
			return;
		battlepass_tick_info* info = AllocEventInfo<battlepass_tick_info>();
		s_pkTick = event_create(battlepass_tick, info, PASSES_PER_SEC(60));
	}

	void SendWindow(LPCHARACTER ch)
	{
		LoadMissions(false);
		EnsureTick();
		Cache& cache = GetCache(ch);
		// What the other cores counted too, and anything done rewarded.
		Settle(ch, cache, true);
		// The final reward's first item where it always was, the other two after
		// the mission count (a window older than them reads the first alone).
		ch->ChatPacket(CHAT_TYPE_COMMAND, "BPBegin %u %d %u %u %d %u %u %u %u %u", cache.season, DaysLeft(),
				s_adwFinalVnum[0], s_adwFinalCount[0], cache.final.claimed ? 1 : 0, (unsigned int)s_vecMissions.size(),
				s_adwFinalVnum[1], s_adwFinalCount[1], s_adwFinalVnum[2], s_adwFinalCount[2]);
		for (size_t i = 0; i < s_vecMissions.size(); ++i)
		{
			const Mission& m = s_vecMissions[i];
			const Progress& p = cache.missions[m.id];
			// The level, the required mission and whether it is locked after the
			// name: a window older than them reads the rest alone.
			ch->ChatPacket(CHAT_TYPE_COMMAND, "BPMission %u %u %u %u %u %d %u %u %u %u %u %u %s %u %u %d", m.id,
					(unsigned int)m.type, m.target, m.count, std::min<DWORD>(m.count, p.value + p.delta),
					p.claimed ? 1 : 0,
					m.rewardVnum[0], m.rewardCount[0], m.rewardVnum[1], m.rewardCount[1], m.rewardVnum[2], m.rewardCount[2],
					m.nameHex.empty() ? "-" : m.nameHex.c_str(), m.targetLevel, m.requiredId, IsLocked(cache, m) ? 1 : 0);
			// The description on a line of its own: a chat command is at most
			// 512 bytes, and hex doubles it.
			if (!m.descHex.empty())
				ch->ChatPacket(CHAT_TYPE_COMMAND, "BPDesc %u %s", m.id, m.descHex.substr(0, 440).c_str());
		}
		ch->ChatPacket(CHAT_TYPE_COMMAND, "BPEnd %d", AllDone(cache) ? 1 : 0);
	}

	void Claim(LPCHARACTER ch, DWORD)
	{
		LoadMissions(false);
		Settle(ch, GetCache(ch), true);
		SendWindow(ch);
	}

	void ClaimFinal(LPCHARACTER ch)
	{
		LoadMissions(false);
		Cache& cache = GetCache(ch);
		Settle(ch, cache, false);
		if (cache.final.claimed)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Battle Pass: nagroda koncowa tego sezonu jest juz odebrana.");
			return;
		}
		if (!AllDone(cache))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Battle Pass: najpierw ukoncz wszystkie misje sezonu.");
			return;
		}
		if (!GiveFinal(ch, cache))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Battle Pass: nagroda koncowa tego sezonu jest juz odebrana.");
			return;
		}
		SendWindow(ch);
	}

	// The season's final reward, taken once whichever core gets there.
	bool GiveFinal(LPCHARACTER ch, Cache& cache)
	{
		if (!TakeClaim(ch->GetPlayerID(), cache.season, 0))
		{
			cache.final.claimed = true;
			return false;
		}
		cache.final.claimed = true;
		for (int r = 0; r < 3; ++r)
			if (s_adwFinalVnum[r])
				ch->AutoGiveItem(s_adwFinalVnum[r], (ITEM_COUNT)std::max<DWORD>(1, s_adwFinalCount[r]));
		sys_log(0, "BATTLEPASS: %s%s claimed the final reward of %u", IsBot(ch) ? "bot " : "", ch->GetName(),
				cache.season);
		return true;
	}
}

// MT2009_PLUS_OCHAO_BOTS_V1 (kills): the Temple of Ochao's watch counts a
// bot's kills there (playerbot_ochao_bots.h, later in this unit).
namespace { void NoteOchaoBotKill(LPCHARACTER killer, LPCHARACTER victim); }
// MT2009_PLUS_AREZZO_BOTS_V1 (kills): and the Arezzo maps' (playerbot_arezzo_bots.h).
namespace { void NoteArezzoBotKill(LPCHARACTER killer, LPCHARACTER victim); }

// The engine's calls (server-patches/playerqol, MT2009_PLUS_BATTLE_PASS_V1).
void BattlePassOnKill(LPCHARACTER killer, LPCHARACTER victim)
{
	NoteOchaoBotKill(killer, victim); // MT2009_PLUS_OCHAO_BOTS_V1 (kills)
	NoteArezzoBotKill(killer, victim); // MT2009_PLUS_AREZZO_BOTS_V1 (kills)
	if (!killer || !victim || victim->IsPC() || !mt2009_battlepass::Counts(killer))
		return;
	const DWORD race = victim->GetRaceNum();
	const DWORD level = victim->GetLevel();
	if (victim->IsStone())
		mt2009_battlepass::Add(killer, mt2009_battlepass::TYPE_METIN, race, 1, level);
	else
	{
		mt2009_battlepass::Add(killer, mt2009_battlepass::TYPE_MONSTER, race, 1, level);
		if (victim->GetMobRank() >= MOB_RANK_BOSS)
			mt2009_battlepass::Add(killer, mt2009_battlepass::TYPE_BOSS, race, 1, level);
	}
}

// A Metin or a boss counts for everyone who hurt it and for the killer's party
// members near it, not only for the last blow: a raid of eight kills a boss
// once and all eight need it for their mission (the owner, 29 September).
// The killer itself was counted by BattlePassOnKill already.
namespace mt2009_battlepass
{
	struct FShareKill
	{
		LPCHARACTER victim;
		std::set<DWORD>* done;
		void operator()(LPCHARACTER ch)
		{
			if (!ch || !ch->IsPC() || !Counts(ch) || ch->GetMapIndex() != victim->GetMapIndex() ||
					DISTANCE_APPROX(ch->GetX() - victim->GetX(), ch->GetY() - victim->GetY()) > 5000 ||
					!done->insert(ch->GetPlayerID()).second)
				return;
			const DWORD race = victim->GetRaceNum();
			const DWORD level = victim->GetLevel();
			if (victim->IsStone())
				Add(ch, TYPE_METIN, race, 1, level);
			else
				Add(ch, TYPE_BOSS, race, 1, level);
		}
	};
}

void BattlePassOnKillShared(LPCHARACTER killer, LPCHARACTER victim, const std::vector<LPCHARACTER>& hurt)
{
	if (!victim || victim->IsPC() || !(victim->IsStone() || victim->GetMobRank() >= MOB_RANK_BOSS))
		return;
	std::set<DWORD> done;
	if (killer && killer->IsPC())
		done.insert(killer->GetPlayerID());
	mt2009_battlepass::FShareKill share;
	share.victim = victim;
	share.done = &done;
	for (size_t i = 0; i < hurt.size(); ++i)
		share(hurt[i]);
	if (killer && killer->GetParty())
		killer->GetParty()->ForEachOnlineMember(share);
}

void BattlePassOnStat(LPCHARACTER ch, DWORD stat, long long value)
{
	using namespace mt2009_battlepass;
	BYTE type = 0;
	switch (stat)
	{
		case PLAYER_STATS_FISHING_FLAG: type = TYPE_FISH; break;
		case PLAYER_STATS_GOLD_FLAG: type = TYPE_YANG; break;
		case PLAYER_STATS_CHEST_FLAG: type = TYPE_CHEST; break;
		case PLAYER_STATS_HERBALISM_FLAG: type = TYPE_HERB; break;
		case PLAYER_STATS_MINING_FLAG: type = TYPE_MINING; break;
		case PLAYER_STATS_DUNGEON_FLAG: type = TYPE_DUNGEON; break;
		case PLAYER_STATS_QUESTBOOK_FLAG: type = TYPE_QUESTBOOK; break;
		default: return;
	}
	Add(ch, type, 0, value);
}

// A refine attempt, won or lost (at a smith or with a scroll).
void BattlePassOnRefine(LPCHARACTER ch)
{
	mt2009_battlepass::Add(ch, mt2009_battlepass::TYPE_REFINE, 0, 1);
}

void BattlePassOnUse(LPCHARACTER ch, DWORD vnum)
{
	mt2009_battlepass::Add(ch, mt2009_battlepass::TYPE_USE_ITEM, vnum, 1);
}

// A message on the shout channel, whatever it says: a player's once the engine
// has let it out (MT2009_PLUS_BATTLE_PASS_V1 (shout), input_main.cpp), a bot's
// where the bot shouts (MT2009_PLUS_BP_BOTS_V1).
void BattlePassOnShout(LPCHARACTER ch)
{
	mt2009_battlepass::Add(ch, mt2009_battlepass::TYPE_SHOUT, 0, 1);
}

// "/battlepass" (the window), "/battlepass odbierz <id>", "/battlepass nagroda",
// and for a GM "/battlepass przeladuj" (the missions table read now).
void BattlePassCommand(LPCHARACTER ch, const char* argument)
{
	if (!mt2009_battlepass::Eligible(ch))
		return;
	char sub[32], arg[32];
	const char* rest = one_argument(argument, sub, sizeof(sub));
	one_argument(rest, arg, sizeof(arg));
	if (!*sub || !strcmp(sub, "otworz"))
		mt2009_battlepass::SendWindow(ch);
	else if (!strcmp(sub, "odbierz"))
	{
		DWORD id = 0;
		str_to_number(id, arg);
		mt2009_battlepass::Claim(ch, id);
	}
	else if (!strcmp(sub, "nagroda"))
		mt2009_battlepass::ClaimFinal(ch);
	else if (!strcmp(sub, "przeladuj") && ch->GetGMLevel() > GM_PLAYER)
	{
		mt2009_battlepass::LoadMissions(true);
		ch->ChatPacket(CHAT_TYPE_INFO, "Battle Pass: misje wczytane ponownie (%u).",
				(unsigned int)mt2009_battlepass::s_vecMissions.size());
	}
}
