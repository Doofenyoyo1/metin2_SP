#pragma once
// MT2009_PLUS_WEEKLY_RANKING_V1: the weekly ranking and its titles (the owner,
// 3 October: "Bierzemy ranking i tytuly z Arezzo"), on the basis of the weekly
// ranking of the Arezzo files (byLUZER's char_weekly.cpp, char_titles.cpp and
// the season block of main.cpp) - made over into our own code on the
// chat-command protocol: no packet, no exe change, no TPlayerTable column.
//
// Eight categories, players and bots alike (a bot's deeds count as anybody's):
//   1 monsters killed       (the last blow; a companion's kill is its owner's,
//                            as for the quests - BattlePassOnKill)
//   2 Metin stones          (the last blow)
//   3 bosses (rank >= boss, not a stone; everyone who hurt it)
//   4 players killed        (PvP of any kind; one killer and one victim count
//                            once in PVP_PAIR_COOLDOWN_SEC)
//   5 dungeons finished     (d.update_ranking - every character in the
//                            instance, or who hurt the boss on an open map)
//   6 successful refines    (the smith and the scrolls, PLAYER_STATS_REFINE_SUCCESS_FLAG)
//   7 alchemy               (a Dragon Stone's grade, step or strength refine
//                            that took - DSManager, server-patches/weeklyrank)
//   8 level                 (the level, then the experience; player.player)
// The counts are kept in memory and added to player.weekly_rank_score
// (value = value + delta) once a minute in a few statements - never a write a
// kill. Every core counts its own and only ever adds.
//
// The season: player.weekly_rank_state (one row, the panels' settings): on or
// off, the length in days (7 = Monday 00:00 to Monday 00:00 of the server's
// clock), the number and its end. ROLLOVER_GRACE_SEC after the end (every core
// has written its counts by then) one core - GET_LOCK and a conditional UPDATE
// - fixes each category's top 3 as the title holders of the next season
// (player.weekly_rank_title, season = the season they hold it in), opens the
// next season and says the winners on the notice line once.
//
// A title holder's bonus, for the whole season, its own affects (500-599: kept
// through a death), summed over every title it holds, put on and taken off by
// each core once a minute and at login:
//   place 1/2/3   monsters, Metins, dungeons: strong against monsters 15/8/4%
//                 bosses: strong against bosses 15/8/4%
//                 players: strong against people 15/8/4%
//                 level: strong against monsters and people 15/8/4%
//                 refines: max HP +2500/+2000/+1500
//                 alchemy: attack value +75 (the owner wrote one value)
// The titles ("Lowca I" ... "Mistrz Poziomow III", uiweeklyrank.py) show in the
// ranking window, under the character window and above the nick (the holder's
// best title, "WRANK tail" around it every TAIL_TICKS, the row the System
// Legend's titles use - playerbot_status_tail.py).
//
// To the client (game.py "WRANK", uiweeklyrank.py):
//   WRANK season <season> <seconds left> <on> <days> <your pid>
//   WRANK holder <cat> <place> <pid> <bot> <level> <empire> <value> <name>
//   WRANK holdersend
//   WRANK begin <cat> <season>
//   WRANK row <cat> <pos> <pid> <bot> <level> <empire> <value> <name>
//   WRANK me <cat> <pos> <value>
//   WRANK end <cat>
//   WRANK tail <vid> <cat> <place>          (around a title holder)
// From the client: "/ranking info", "/ranking lista <cat>"; a GM also
// "/ranking koniec" (the season ends now) and "/ranking przeladuj".
//
// An implementation fragment in the sense playerbot_types.h describes:
// included once, after playerbot_dungeon_panel.h.
#include <ctime>

namespace mt2009_wrank
{
	enum ECategory
	{
		CAT_NONE = 0,
		CAT_MONSTER = 1,
		CAT_METIN = 2,
		CAT_BOSS = 3,
		CAT_PLAYER = 4,
		CAT_DUNGEON = 5,
		CAT_REFINE = 6,
		CAT_ALCHEMY = 7,
		CAT_LEVEL = 8,
		CAT_COUNT = CAT_LEVEL,
	};

	const int TOP = 3;
	const int LIST_SIZE = 50;
	// The bonuses' own affect types (500-599 are kept through a death).
	const DWORD AFF_MONSTER = 592;
	const DWORD AFF_BOSS = 593;
	const DWORD AFF_HUMAN = 594;
	const DWORD AFF_HP = 595;
	const DWORD AFF_ATT = 596;
	const int PCT_BY_PLACE[TOP + 1] = { 0, 15, 8, 4 };
	const int HP_BY_PLACE[TOP + 1] = { 0, 2500, 2000, 1500 };
	const int ATT_BY_PLACE[TOP + 1] = { 0, 75, 75, 75 };
	const int TICK_SEC = 10;
	const int MINUTE_TICKS = 6;
	const int TAIL_TICKS = 1;
	const time_t ROLLOVER_GRACE_SEC = 90;
	const time_t LIST_CACHE_SEC = 60;
	const time_t HOLDERS_RELOAD_SEC = 300;
	const time_t PVP_PAIR_COOLDOWN_SEC = 600;
	const DWORD KEEP_SEASONS = 4;
	const DWORD REQUEST_GAP_MS = 700;

	// CP1250, for the notice line.
	const char* const TITLE_NAMES[CAT_COUNT + 1] = {
		"", "\xa3owca", "Niszczyciel", "Pogromca Boss\xf3w", "Zab\xf3jca", "Podr\xf3\xbfnik", "Kowal",
		"Alchemik", "Mistrz Poziom\xf3w",
	};
	const char* const CATEGORY_NAMES[CAT_COUNT + 1] = {
		"", "Potwory", "Metiny", "Bossy", "Gracze", "Wyprawy", "Ulepszenia", "Alchemia", "Poziom",
	};

	struct State
	{
		bool loaded;
		bool enabled;
		int days;
		DWORD season;
		time_t start;
		time_t end;
		State() : loaded(false), enabled(true), days(7), season(1), start(0), end(0) {}
	};

	struct Holder
	{
		BYTE cat;
		BYTE place;
		DWORD pid;
		bool bot;
		int level;
		BYTE empire;
		long long value;
		std::string name;
	};

	struct Pending
	{
		DWORD season;
		DWORD v[CAT_COUNT + 1];
		bool bot;
		Pending() : season(0), bot(false) { memset(v, 0, sizeof(v)); }
	};

	struct Row
	{
		DWORD pid;
		bool bot;
		int level;
		BYTE empire;
		long long value;
		std::string name;
	};

	struct ListCache
	{
		DWORD season;
		time_t at;
		std::vector<Row> rows;
		ListCache() : season(0), at(0) {}
	};

	State s_state;
	bool s_bTables = false;
	time_t s_tablesTry = 0;
	std::vector<Holder> s_holders;
	DWORD s_dwHoldersSeason = 0;
	time_t s_holdersAt = 0;
	std::map<DWORD, std::vector<std::pair<BYTE, BYTE> > > s_titles;
	std::set<DWORD> s_applied;
	std::map<DWORD, Pending> s_pending;
	ListCache s_lists[CAT_COUNT + 1];
	std::map<unsigned long long, time_t> s_pvpPairs;
	std::map<DWORD, DWORD> s_requestAt;
	LPEVENT s_pkTick = NULL;
	int s_iTicks = 0;

	bool IsBotChar(LPCHARACTER ch)
	{
		return ch && ((ch->GetDesc() && ch->GetDesc()->IsBot()) ||
				CPlayerBotManager::instance().IsRegisteredBotPID(ch->GetPlayerID()));
	}

	// Who counts and who gets a bonus: a character in the game, a person's
	// or a bot's.
	bool Counts(LPCHARACTER ch)
	{
		// MT2009_PLUS_WEEKLY_RANKING_UX_V1: never a GM's character (the
		// owner, 3 October: admin and every GM out of the ranking).
		return ch && ch->IsPC() && ch->GetDesc() && ch->GetGMLevel() <= GM_PLAYER;
	}

	// A person who opens the window.
	bool Asker(LPCHARACTER ch)
	{
		return ch && ch->IsPC() && ch->GetDesc() && !ch->GetDesc()->IsBot();
	}

	// A name for a statement and for a command line: what a character's name
	// may hold, nothing that breaks either.
	std::string CleanName(const char* name)
	{
		std::string out;
		for (const char* p = name ? name : ""; *p && out.size() < 24; ++p)
		{
			const unsigned char c = (unsigned char)*p;
			if (c <= 32 || c == '\'' || c == '"' || c == '\\' || c == '%' || c == 127)
				continue;
			out += (char)c;
		}
		return out.empty() ? std::string("?") : out;
	}

	bool Exec(const char* query)
	{
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		return msg.get() && msg->uiSQLErrno == 0;
	}

	// The tables and the settings' row; apply.sh makes them too. Idempotent.
	bool EnsureTables()
	{
		if (s_bTables)
			return true;
		const time_t now = (time_t)get_global_time();
		if (s_tablesTry != 0 && now < s_tablesTry)
			return false;
		s_tablesTry = now + 60;
		const bool ok =
				Exec("CREATE TABLE IF NOT EXISTS player.weekly_rank_state ("
					"id TINYINT UNSIGNED NOT NULL PRIMARY KEY, enabled TINYINT UNSIGNED NOT NULL DEFAULT 1, "
					"season_days TINYINT UNSIGNED NOT NULL DEFAULT 7, season INT UNSIGNED NOT NULL DEFAULT 1, "
					"season_start INT UNSIGNED NOT NULL DEFAULT 0, season_end INT UNSIGNED NOT NULL DEFAULT 0) ENGINE=InnoDB") &&
				Exec("CREATE TABLE IF NOT EXISTS player.weekly_rank_score ("
					"season INT UNSIGNED NOT NULL, cat TINYINT UNSIGNED NOT NULL, pid INT UNSIGNED NOT NULL, "
					"value INT UNSIGNED NOT NULL DEFAULT 0, is_bot TINYINT UNSIGNED NOT NULL DEFAULT 0, "
					"PRIMARY KEY (season, cat, pid), KEY rank_idx (season, cat, value)) ENGINE=InnoDB") &&
				Exec("CREATE TABLE IF NOT EXISTS player.weekly_rank_title ("
					"season INT UNSIGNED NOT NULL, cat TINYINT UNSIGNED NOT NULL, place TINYINT UNSIGNED NOT NULL, "
					"pid INT UNSIGNED NOT NULL, name VARCHAR(24) NOT NULL DEFAULT '', level SMALLINT UNSIGNED NOT NULL DEFAULT 0, "
					"empire TINYINT UNSIGNED NOT NULL DEFAULT 0, value BIGINT UNSIGNED NOT NULL DEFAULT 0, "
					"is_bot TINYINT UNSIGNED NOT NULL DEFAULT 0, PRIMARY KEY (season, cat, place)) ENGINE=InnoDB") &&
				Exec("INSERT IGNORE INTO player.weekly_rank_state (id) VALUES (1)");
		if (!ok)
		{
			sys_err("WEEKLY_RANK: the tables cannot be created; the ranking waits");
			return false;
		}
		s_bTables = true;
		return true;
	}

	// Midnight of the server's clock, days after t's day.
	time_t MidnightAfter(time_t t, int days)
	{
		struct tm lt;
		localtime_r(&t, &lt);
		lt.tm_hour = lt.tm_min = lt.tm_sec = 0;
		lt.tm_mday += days;
		lt.tm_isdst = -1;
		return mktime(&lt);
	}

	// The end of a season that starts now: the next Monday 00:00 for a week,
	// otherwise midnight that many days on.
	time_t SeasonEndFrom(time_t now, int days)
	{
		if (days == 7)
		{
			struct tm lt;
			localtime_r(&now, &lt);
			int toMonday = (8 - lt.tm_wday) % 7;
			if (toMonday == 0)
				toMonday = 7;
			return MidnightAfter(now, toMonday);
		}
		return MidnightAfter(now, std::max(1, days));
	}

	void ClearLists()
	{
		for (int i = 0; i <= CAT_COUNT; ++i)
			s_lists[i] = ListCache();
	}

	void LoadHolders()
	{
		s_holdersAt = (time_t)get_global_time();
		char query[256];
		snprintf(query, sizeof(query),
				"SELECT cat, place, pid, is_bot, level, empire, value, name FROM player.weekly_rank_title "
				"WHERE season=%u ORDER BY cat, place", s_state.season);
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return;
		std::vector<Holder> fresh;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			Holder h;
			unsigned int cat = 0, place = 0, bot = 0, empire = 0;
			h.pid = 0;
			h.level = 0;
			h.value = 0;
			str_to_number(cat, row[0]);
			str_to_number(place, row[1]);
			str_to_number(h.pid, row[2]);
			str_to_number(bot, row[3]);
			str_to_number(h.level, row[4]);
			str_to_number(empire, row[5]);
			str_to_number(h.value, row[6]);
			if (cat < 1 || cat > CAT_COUNT || place < 1 || place > (unsigned int)TOP || !h.pid)
				continue;
			h.cat = (BYTE)cat;
			h.place = (BYTE)place;
			h.bot = bot != 0;
			h.empire = (BYTE)empire;
			h.name = CleanName(row[7]);
			fresh.push_back(h);
		}
		s_holders.swap(fresh);
		s_dwHoldersSeason = s_state.season;
		s_titles.clear();
		for (size_t i = 0; i < s_holders.size(); ++i)
			s_titles[s_holders[i].pid].push_back(std::make_pair(s_holders[i].cat, s_holders[i].place));
	}

	// The settings' row, read again (a minute's clock); a new season's holders
	// and lists with it.
	void LoadState()
	{
		if (!EnsureTables())
			return;
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(
				"SELECT enabled, season_days, season, season_start, season_end FROM player.weekly_rank_state WHERE id=1"));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return;
		MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
		if (!row)
			return;
		unsigned int enabled = 1, days = 7, season = 1, start = 0, end = 0;
		str_to_number(enabled, row[0]);
		str_to_number(days, row[1]);
		str_to_number(season, row[2]);
		str_to_number(start, row[3]);
		str_to_number(end, row[4]);
		if (days < 1 || days > 28)
			days = 7;
		const time_t now = (time_t)get_global_time();
		if (end == 0)
		{
			// The first season: from now to the next boundary.
			const time_t first = SeasonEndFrom(now, (int)days);
			char query[200];
			snprintf(query, sizeof(query),
					"UPDATE player.weekly_rank_state SET season_start=%u, season_end=%u WHERE id=1 AND season_end=0",
					(unsigned int)now, (unsigned int)first);
			Exec(query);
			start = (unsigned int)now;
			end = (unsigned int)first;
		}
		const bool seasonChanged = !s_state.loaded || s_state.season != season;
		s_state.enabled = enabled != 0;
		s_state.days = (int)days;
		s_state.season = season ? season : 1;
		s_state.start = (time_t)start;
		s_state.end = (time_t)end;
		s_state.loaded = true;
		if (seasonChanged)
			ClearLists();
		if (seasonChanged || s_dwHoldersSeason != s_state.season || now - s_holdersAt >= HOLDERS_RELOAD_SEC)
			LoadHolders();
	}

	void EnsureLoaded()
	{
		if (!s_state.loaded)
			LoadState();
	}

	void Bonus(DWORD pid, long want[5])
	{
		for (int i = 0; i < 5; ++i)
			want[i] = 0;
		if (!s_state.enabled)
			return;
		std::map<DWORD, std::vector<std::pair<BYTE, BYTE> > >::const_iterator it = s_titles.find(pid);
		if (it == s_titles.end())
			return;
		for (size_t i = 0; i < it->second.size(); ++i)
		{
			const BYTE cat = it->second[i].first;
			const BYTE place = it->second[i].second;
			if (place < 1 || place > TOP)
				continue;
			switch (cat)
			{
				case CAT_MONSTER:
				case CAT_METIN:
				case CAT_DUNGEON:
					want[0] += PCT_BY_PLACE[place];
					break;
				case CAT_BOSS:
					want[1] += PCT_BY_PLACE[place];
					break;
				case CAT_PLAYER:
					want[2] += PCT_BY_PLACE[place];
					break;
				case CAT_LEVEL:
					want[0] += PCT_BY_PLACE[place];
					want[2] += PCT_BY_PLACE[place];
					break;
				case CAT_REFINE:
					want[3] += HP_BY_PLACE[place];
					break;
				case CAT_ALCHEMY:
					want[4] += ATT_BY_PLACE[place];
					break;
			}
		}
	}

	// One character's bonuses as its titles say: put on, changed or taken off.
	void Sync(LPCHARACTER ch)
	{
		if (!Counts(ch))
			return;
		static const DWORD types[5] = { AFF_MONSTER, AFF_BOSS, AFF_HUMAN, AFF_HP, AFF_ATT };
		static const BYTE points[5] = { POINT_ATTBONUS_MONSTER, POINT_ATTBONUS_BOSS, POINT_ATTBONUS_HUMAN,
				POINT_MAX_HP, POINT_ATT_GRADE_BONUS };
		long want[5];
		Bonus(ch->GetPlayerID(), want);
		bool any = false;
		for (int i = 0; i < 5; ++i)
		{
			CAffect* aff = ch->FindAffect(types[i]);
			if (want[i] <= 0)
			{
				if (aff)
					ch->RemoveAffect(types[i]);
				continue;
			}
			any = true;
			if (aff && aff->bApplyOn == points[i] && aff->lApplyValue == want[i])
				continue;
			if (aff)
				ch->RemoveAffect(types[i]);
			ch->AddAffect(types[i], points[i], want[i], 0, INFINITE_AFFECT_DURATION, 0, true);
		}
		if (any)
			s_applied.insert(ch->GetPlayerID());
		else
			s_applied.erase(ch->GetPlayerID());
	}

	// Every holder on this core and everyone this core gave a bonus before.
	void SyncAll()
	{
		std::set<DWORD> pids(s_applied);
		for (size_t i = 0; i < s_holders.size(); ++i)
			pids.insert(s_holders[i].pid);
		for (std::set<DWORD>::const_iterator it = pids.begin(); it != pids.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(*it);
			if (ch)
				Sync(ch);
			else
				s_applied.erase(*it);
		}
	}

	// A holder's best title: the best place, then the category's order.
	bool BestTitle(DWORD pid, BYTE& cat, BYTE& place)
	{
		std::map<DWORD, std::vector<std::pair<BYTE, BYTE> > >::const_iterator it = s_titles.find(pid);
		if (it == s_titles.end() || it->second.empty())
			return false;
		cat = 0;
		place = TOP + 1;
		for (size_t i = 0; i < it->second.size(); ++i)
			if (it->second[i].second < place)
			{
				place = it->second[i].second;
				cat = it->second[i].first;
			}
		return cat != 0;
	}

	// The title above every holder's nick on this core, for whoever is near.
	void SendTails()
	{
		if (!s_state.enabled)
			return;
		for (std::map<DWORD, std::vector<std::pair<BYTE, BYTE> > >::const_iterator it = s_titles.begin();
				it != s_titles.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			BYTE cat = 0, place = 0;
			if (!ch || !ch->GetSectree() || !BestTitle(it->first, cat, place))
				continue;
			char command[64];
			int len = snprintf(command, sizeof(command), "WRANK tail %u %u %u",
					(unsigned int)ch->GetVID(), (unsigned int)cat, (unsigned int)place);
			if (len <= 0 || len >= (int)sizeof(command))
				continue;
			++len;   // the trailing NUL every chat packet carries
			TPacketGCChat pack;
			pack.header = HEADER_GC_CHAT;
			pack.size = sizeof(TPacketGCChat) + len;
			pack.type = CHAT_TYPE_COMMAND;
			pack.id = 0;
			pack.bEmpire = 0;
			TEMP_BUFFER buf;
			buf.write(&pack, sizeof(TPacketGCChat));
			buf.write(command, len);
			ch->PacketAround(buf.read_peek(), buf.size());
		}
	}

	// This core's counts into the table, added to what is there, in
	// statements of up to ~3.5 kB (DBManager::Query holds 4096 bytes).
	void Flush()
	{
		if (s_pending.empty() || !EnsureTables())
			return;
		const std::string head = "INSERT INTO player.weekly_rank_score (season, cat, pid, value, is_bot) VALUES ";
		const std::string tail = " ON DUPLICATE KEY UPDATE value = value + VALUES(value)";
		const size_t room = 3500 - head.size() - tail.size();
		std::string values;
		unsigned int rows = 0;
		for (std::map<DWORD, Pending>::const_iterator it = s_pending.begin(); it != s_pending.end(); ++it)
			for (int cat = 1; cat < CAT_LEVEL; ++cat)
			{
				if (!it->second.v[cat])
					continue;
				char row[80];
				snprintf(row, sizeof(row), "%s(%u,%u,%u,%u,%u)", values.empty() ? "" : ",", it->second.season,
						(unsigned int)cat, it->first, it->second.v[cat], it->second.bot ? 1U : 0U);
				if (!values.empty() && values.size() + strlen(row) > room)
				{
					DBManager::instance().Query("%s", (head + values + tail).c_str());
					values.clear();
					snprintf(row, sizeof(row), "(%u,%u,%u,%u,%u)", it->second.season, (unsigned int)cat, it->first,
							it->second.v[cat], it->second.bot ? 1U : 0U);
				}
				values += row;
				++rows;
			}
		if (!values.empty())
			DBManager::instance().Query("%s", (head + values + tail).c_str());
		s_pending.clear();
		if (rows)
			sys_log(1, "WEEKLY_RANK: %u counts written", rows);
	}

	void Add(LPCHARACTER ch, BYTE cat, DWORD n);
	void EnsureTick();

	// The top 3 of a category of the season that ends: (pid, bot, level,
	// empire, value, name).
	void ReadTop(DWORD season, BYTE cat, int limit, std::vector<Row>& out)
	{
		out.clear();
		char query[700];
		if (cat == CAT_LEVEL)
			snprintf(query, sizeof(query),
					"SELECT p.id, IF(LEFT(IFNULL(a.login,''),10)='playerbot_',1,0), p.level, IFNULL(pi.empire,0), p.exp, p.name "
					"FROM player.player p LEFT JOIN player.player_index pi ON pi.id=p.account_id "
					"LEFT JOIN account.account a ON a.id=p.account_id "
					"WHERE p.name NOT LIKE '[%%'"
					" AND p.name NOT IN (SELECT mName FROM common.gmlist) AND IFNULL(a.login,'') NOT IN (SELECT mAccount FROM common.gmlist) ORDER BY p.level DESC, p.exp DESC, p.id ASC LIMIT %d", limit);
		else
			snprintf(query, sizeof(query),
					"SELECT s.pid, s.is_bot, p.level, IFNULL(pi.empire,0), s.value, p.name "
					"FROM player.weekly_rank_score s JOIN player.player p ON p.id=s.pid "
					"LEFT JOIN player.player_index pi ON pi.id=p.account_id "
					"LEFT JOIN account.account a ON a.id=p.account_id "
					"WHERE s.season=%u AND s.cat=%u AND s.value>0 AND p.name NOT LIKE '[%%' "
					"AND p.name NOT IN (SELECT mName FROM common.gmlist) AND IFNULL(a.login,'') NOT IN (SELECT mAccount FROM common.gmlist) "
					"ORDER BY s.value DESC, s.pid ASC LIMIT %d", season, (unsigned int)cat, limit);
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			Row r;
			unsigned int bot = 0, empire = 0;
			r.pid = 0;
			r.level = 0;
			r.value = 0;
			str_to_number(r.pid, row[0]);
			str_to_number(bot, row[1]);
			str_to_number(r.level, row[2]);
			str_to_number(empire, row[3]);
			str_to_number(r.value, row[4]);
			r.bot = bot != 0;
			r.empire = (BYTE)empire;
			r.name = CleanName(row[5]);
			if (r.pid)
				out.push_back(r);
		}
	}

	// The season ends: its top 3 become the next season's title holders, the
	// next season opens, the winners are said once. One core does it.
	void TryRollover()
	{
		const time_t now = (time_t)get_global_time();
		if (!s_state.loaded || !s_state.enabled || s_state.end == 0 || now < s_state.end + ROLLOVER_GRACE_SEC)
			return;
		std::unique_ptr<SQLMsg> lock(AccountDB::instance().DirectQuery("SELECT GET_LOCK('mt2009_weekly_rank', 0)"));
		bool locked = false;
		if (lock.get() && lock->uiSQLErrno == 0 && lock->Get() && lock->Get()->pSQLResult)
		{
			MYSQL_ROW row = mysql_fetch_row(lock->Get()->pSQLResult);
			locked = row && row[0] && atoi(row[0]) == 1;
		}
		if (!locked)
			return;
		const DWORD before = s_state.season;
		LoadState();
		if (s_state.season == before && s_state.enabled && now >= s_state.end + ROLLOVER_GRACE_SEC)
		{
			const DWORD season = s_state.season;
			const DWORD next = season + 1;
			char query[1024];
			snprintf(query, sizeof(query), "DELETE FROM player.weekly_rank_title WHERE season=%u", next);
			Exec(query);
			std::string notice;
			for (int cat = 1; cat <= CAT_COUNT; ++cat)
			{
				std::vector<Row> top;
				ReadTop(season, (BYTE)cat, TOP, top);
				for (size_t i = 0; i < top.size(); ++i)
				{
					const Row& r = top[i];
					snprintf(query, sizeof(query),
							"INSERT INTO player.weekly_rank_title (season, cat, place, pid, name, level, empire, value, is_bot) "
							"VALUES (%u, %u, %u, %u, '%s', %d, %u, %lld, %u)",
							next, (unsigned int)cat, (unsigned int)(i + 1), r.pid, r.name.c_str(), r.level,
							(unsigned int)r.empire, r.value, r.bot ? 1U : 0U);
					Exec(query);
				}
				if (!top.empty())
				{
					if (!notice.empty())
						notice += ", ";
					notice += CATEGORY_NAMES[cat];
					notice += " - ";
					notice += top[0].name;
				}
			}
			const time_t newEnd = SeasonEndFrom(now, s_state.days);
			snprintf(query, sizeof(query),
					"UPDATE player.weekly_rank_state SET season=%u, season_start=%u, season_end=%u WHERE id=1 AND season=%u",
					next, (unsigned int)now, (unsigned int)newEnd, season);
			Exec(query);
			if (next > KEEP_SEASONS)
			{
				snprintf(query, sizeof(query), "DELETE FROM player.weekly_rank_score WHERE season < %u", next - KEEP_SEASONS);
				DBManager::instance().Query("%s", query);
				snprintf(query, sizeof(query), "DELETE FROM player.weekly_rank_title WHERE season < %u", next - KEEP_SEASONS);
				DBManager::instance().Query("%s", query);
			}
			char text[512];
			if (notice.empty())
				snprintf(text, sizeof(text), "[Ranking] Sezon %u zako\xf1" "czony. Rozpoczyna si\xea sezon %u!", season, next);
			else
				snprintf(text, sizeof(text), "[Ranking] Sezon %u zako\xf1" "czony! Najlepsi: %s. Tytu\xb3y i bonusy na sezon %u - okno Rankingu.",
						season, notice.c_str(), next);
			BroadcastNotice(text);
			sys_log(0, "WEEKLY_RANK: season %u closed, %u opens, ends %u", season, next, (unsigned int)newEnd);
			LoadState();
			SyncAll();
		}
		std::unique_ptr<SQLMsg> unlock(AccountDB::instance().DirectQuery("SELECT RELEASE_LOCK('mt2009_weekly_rank')"));
	}

	EVENTINFO(weekly_rank_tick_info)
	{
		int dummy;
	};

	EVENTFUNC(weekly_rank_tick)
	{
		++s_iTicks;
		if (s_iTicks % MINUTE_TICKS == 0 || !s_state.loaded)
		{
			const DWORD before = s_state.season;
			LoadState();
			Flush();
			TryRollover();
			if (s_state.season != before)
				ClearLists();
			SyncAll();
			const time_t now = (time_t)get_global_time();
			for (std::map<unsigned long long, time_t>::iterator it = s_pvpPairs.begin(); it != s_pvpPairs.end(); )
			{
				if (now - it->second >= PVP_PAIR_COOLDOWN_SEC)
					s_pvpPairs.erase(it++);
				else
					++it;
			}
			const DWORD ms = get_dword_time();
			for (std::map<DWORD, DWORD>::iterator it = s_requestAt.begin(); it != s_requestAt.end(); )
			{
				if (ms - it->second > 60000)
					s_requestAt.erase(it++);
				else
					++it;
			}
		}
		if (s_iTicks % TAIL_TICKS == 0)
			SendTails();
		return PASSES_PER_SEC(TICK_SEC);
	}

	void EnsureTick()
	{
		if (s_pkTick)
			return;
		weekly_rank_tick_info* info = AllocEventInfo<weekly_rank_tick_info>();
		s_pkTick = event_create(weekly_rank_tick, info, PASSES_PER_SEC(TICK_SEC));
	}

	void Add(LPCHARACTER ch, BYTE cat, DWORD n)
	{
		if (!n || cat < 1 || cat >= CAT_LEVEL || !Counts(ch))
			return;
		EnsureTick();
		EnsureLoaded();
		if (!s_state.loaded || !s_state.enabled)
			return;
		Pending& p = s_pending[ch->GetPlayerID()];
		if (p.season != s_state.season)
		{
			// Counts of a season gone stay with it (written below).
			bool any = false;
			for (int i = 1; i < CAT_LEVEL; ++i)
				any = any || p.v[i];
			if (any && p.season)
			{
				for (int i = 1; i < CAT_LEVEL; ++i)
					if (p.v[i])
						DBManager::instance().Query(
								"INSERT INTO player.weekly_rank_score (season, cat, pid, value, is_bot) VALUES (%u,%u,%u,%u,%u) "
								"ON DUPLICATE KEY UPDATE value = value + VALUES(value)",
								p.season, (unsigned int)i, ch->GetPlayerID(), p.v[i], p.bot ? 1U : 0U);
			}
			p = Pending();
			p.season = s_state.season;
		}
		p.v[cat] += n;
		p.bot = IsBotChar(ch);
	}

	void Cmd(LPCHARACTER ch, const char* format, ...)
	{
		char buf[480];
		va_list args;
		va_start(args, format);
		vsnprintf(buf, sizeof(buf), format, args);
		va_end(args);
		ch->ChatPacket(CHAT_TYPE_COMMAND, "WRANK %s", buf);
	}

	long SecondsLeft()
	{
		const time_t now = (time_t)get_global_time();
		return s_state.end > now ? (long)(s_state.end - now) : 0L;
	}

	void SendInfo(LPCHARACTER ch)
	{
		Cmd(ch, "season %u %ld %u %d %u", s_state.season, SecondsLeft(), s_state.enabled ? 1U : 0U, s_state.days,
				ch->GetPlayerID());
		for (size_t i = 0; i < s_holders.size(); ++i)
		{
			const Holder& h = s_holders[i];
			Cmd(ch, "holder %u %u %u %u %d %u %lld %s", (unsigned int)h.cat, (unsigned int)h.place, h.pid,
					h.bot ? 1U : 0U, h.level, (unsigned int)h.empire, h.value, h.name.c_str());
		}
		Cmd(ch, "holdersend");
	}

	// The asker's own place and value: its row and this core's count on top
	// (a level: the live one).
	void MyPlace(LPCHARACTER ch, BYTE cat, unsigned int& pos, long long& value)
	{
		pos = 0;
		value = 0;
		char query[600];
		if (cat == CAT_LEVEL)
		{
			value = (long long)ch->GetExp();
			snprintf(query, sizeof(query),
					"SELECT COUNT(*) FROM player.player p LEFT JOIN account.account a ON a.id=p.account_id "
					"WHERE p.name NOT LIKE '[%%' AND p.id<>%u AND p.name NOT IN (SELECT mName FROM common.gmlist) AND IFNULL(a.login,'') NOT IN (SELECT mAccount FROM common.gmlist) AND "
					"(level>%d OR (level=%d AND exp>%lld))",
					ch->GetPlayerID(), ch->GetLevel(), ch->GetLevel(), value);
		}
		else
		{
			snprintf(query, sizeof(query),
					"SELECT value FROM player.weekly_rank_score WHERE season=%u AND cat=%u AND pid=%u",
					s_state.season, (unsigned int)cat, ch->GetPlayerID());
			std::unique_ptr<SQLMsg> mine(AccountDB::instance().DirectQuery(query));
			if (mine.get() && mine->uiSQLErrno == 0 && mine->Get() && mine->Get()->pSQLResult)
			{
				MYSQL_ROW row = mysql_fetch_row(mine->Get()->pSQLResult);
				if (row && row[0])
					str_to_number(value, row[0]);
			}
			std::map<DWORD, Pending>::const_iterator p = s_pending.find(ch->GetPlayerID());
			if (p != s_pending.end() && p->second.season == s_state.season)
				value += p->second.v[cat];
			if (value <= 0)
				return;
			snprintf(query, sizeof(query),
					"SELECT COUNT(*) FROM player.weekly_rank_score WHERE season=%u AND cat=%u AND value>%lld AND pid<>%u",
					s_state.season, (unsigned int)cat, value, ch->GetPlayerID());
		}
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return;
		MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
		unsigned int ahead = 0;
		if (row && row[0])
			str_to_number(ahead, row[0]);
		pos = ahead + 1;
	}

	void SendList(LPCHARACTER ch, BYTE cat)
	{
		ListCache& c = s_lists[cat];
		const time_t now = (time_t)get_global_time();
		if (c.season != s_state.season || now - c.at >= LIST_CACHE_SEC)
		{
			ReadTop(s_state.season, cat, LIST_SIZE, c.rows);
			c.season = s_state.season;
			c.at = now;
		}
		Cmd(ch, "begin %u %u", (unsigned int)cat, s_state.season);
		for (size_t i = 0; i < c.rows.size(); ++i)
		{
			const Row& r = c.rows[i];
			Cmd(ch, "row %u %u %u %u %d %u %lld %s", (unsigned int)cat, (unsigned int)(i + 1), r.pid,
					r.bot ? 1U : 0U, r.level, (unsigned int)r.empire, r.value, r.name.c_str());
		}
		unsigned int pos = 0;
		long long value = 0;
		MyPlace(ch, cat, pos, value);
		Cmd(ch, "me %u %u %lld", (unsigned int)cat, pos, value);
		Cmd(ch, "end %u", (unsigned int)cat);
	}
}

// ---------------------------------------------------------------- the hooks

// A monster's death (BattlePassOnKill, the killer as for the quests).
void WeeklyRankOnKill(LPCHARACTER killer, LPCHARACTER victim)
{
	using namespace mt2009_wrank;
	if (!killer || !victim || victim->IsPC())
		return;
	if (victim->IsStone())
		Add(killer, CAT_METIN, 1);
	else
	{
		Add(killer, CAT_MONSTER, 1);
		if (victim->GetMobRank() >= MOB_RANK_BOSS)
			Add(killer, CAT_BOSS, 1);
	}
}

// A boss counts for everyone who hurt it (BattlePassOnKillShared; the killer
// was counted above).
void WeeklyRankOnKillShared(LPCHARACTER killer, LPCHARACTER victim, const std::vector<LPCHARACTER>& hurt)
{
	using namespace mt2009_wrank;
	if (!victim || victim->IsPC() || victim->IsStone() || victim->GetMobRank() < MOB_RANK_BOSS)
		return;
	std::set<DWORD> done;
	if (killer)
		done.insert(killer->GetPlayerID());
	for (size_t i = 0; i < hurt.size(); ++i)
	{
		LPCHARACTER ch = hurt[i];
		if (ch && ch->IsPC() && ch->GetMapIndex() == victim->GetMapIndex() && done.insert(ch->GetPlayerID()).second)
			Add(ch, CAT_BOSS, 1);
	}
}

// A character killed by a character (PlayerBotLegendOnDeath, every death).
void WeeklyRankOnPlayerDeath(LPCHARACTER victim, LPCHARACTER killer)
{
	using namespace mt2009_wrank;
	if (!victim || !killer || victim == killer || !victim->IsPC() || !killer->IsPC())
		return;
	const unsigned long long key = ((unsigned long long)killer->GetPlayerID() << 32) | victim->GetPlayerID();
	const time_t now = (time_t)get_global_time();
	std::map<unsigned long long, time_t>::iterator it = s_pvpPairs.find(key);
	if (it != s_pvpPairs.end() && now - it->second < PVP_PAIR_COOLDOWN_SEC)
		return;
	s_pvpPairs[key] = now;
	Add(killer, CAT_PLAYER, 1);
}

// A player stat (BattlePassOnStat): a refine that took.
void WeeklyRankOnStat(LPCHARACTER ch, DWORD stat, long long value)
{
	if (stat == PLAYER_STATS_REFINE_SUCCESS_FLAG && value > 0)
		mt2009_wrank::Add(ch, mt2009_wrank::CAT_REFINE, (DWORD)value);
}

// A Dragon Stone refine that took (server-patches/weeklyrank, DragonSoul.cpp).
void WeeklyRankOnAlchemy(LPCHARACTER ch)
{
	mt2009_wrank::Add(ch, mt2009_wrank::CAT_ALCHEMY, 1);
}

// A dungeon finished (DungeonPanelUpdateRanking, d.update_ranking): everyone
// in the instance, or on an open map whoever hurt the boss, and the killer -
// bots as well as people, once each.
namespace mt2009_wrank
{
	struct FCollectAnyPC
	{
		std::vector<LPCHARACTER>* out;
		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER ch = (LPCHARACTER)ent;
			if (ch->IsPC())
				out->push_back(ch);
		}
	};
}

void WeeklyRankOnDungeon(LPCHARACTER pc, LPCHARACTER npc)
{
	using namespace mt2009_wrank;
	if (!pc)
		return;
	std::vector<LPCHARACTER> chars;
	const long mapIndex = pc->GetMapIndex();
	if (mapIndex >= 10000)
	{
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(mapIndex);
		if (pMap)
		{
			FCollectAnyPC f;
			f.out = &chars;
			pMap->for_each(f);
		}
	}
	else if (npc)
	{
		const CHARACTER::TDamageMap& dm = npc->Mt2009PlusGetDamageMap();
		for (CHARACTER::TDamageMap::const_iterator it = dm.begin(); it != dm.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(it->first);
			if (ch && ch->IsPC() && ch->GetMapIndex() == mapIndex)
				chars.push_back(ch);
		}
	}
	chars.push_back(pc);
	std::set<DWORD> done;
	for (size_t i = 0; i < chars.size(); ++i)
		if (done.insert(chars[i]->GetPlayerID()).second)
			Add(chars[i], CAT_DUNGEON, 1);
}

// A character enters the game (server-patches/weeklyrank, input_login.cpp):
// its bonuses as its titles say - a season that turned while it was away
// takes the old ones off.
void WeeklyRankOnLogin(LPCHARACTER ch)
{
	using namespace mt2009_wrank;
	if (!Counts(ch))
		return;
	EnsureTick();
	EnsureLoaded();
	Sync(ch);
}

// "/ranking info|lista <cat>", for a GM also "koniec" and "przeladuj".
ACMD(do_weekly_rank)
{
	using namespace mt2009_wrank;
	if (!Asker(ch))
		return;
	EnsureTick();
	EnsureLoaded();
	if (!s_state.loaded)
		return;
	char sub[32], arg[32];
	const char* rest = one_argument(argument, sub, sizeof(sub));
	one_argument(rest, arg, sizeof(arg));
	const DWORD ms = get_dword_time();
	DWORD& last = s_requestAt[ch->GetPlayerID()];
	if (last != 0 && ms - last < REQUEST_GAP_MS && strcmp(sub, "koniec") && strcmp(sub, "przeladuj"))
		return;
	last = ms;
	if (!*sub || !strcmp(sub, "info"))
		SendInfo(ch);
	else if (!strcmp(sub, "lista"))
	{
		unsigned int cat = 0;
		str_to_number(cat, arg);
		if (cat >= 1 && cat <= (unsigned int)CAT_COUNT)
			SendList(ch, (BYTE)cat);
	}
	else if (!strcmp(sub, "koniec") && ch->GetGMLevel() > GM_PLAYER)
	{
		// The season ends now: rolled over on a core's next minute after the grace.
		char query[160];
		snprintf(query, sizeof(query), "UPDATE player.weekly_rank_state SET season_end=%u WHERE id=1",
				(unsigned int)get_global_time());
		Exec(query);
		LoadState();
		ch->ChatPacket(CHAT_TYPE_INFO, "Ranking: sezon %u konczy sie teraz (podsumowanie za ok. 2 minuty).", s_state.season);
	}
	else if (!strcmp(sub, "przeladuj") && ch->GetGMLevel() > GM_PLAYER)
	{
		Flush();
		LoadState();
		LoadHolders();
		ClearLists();
		SyncAll();
		ch->ChatPacket(CHAT_TYPE_INFO, "Ranking: sezon %u, %s, posiadaczy tytulow: %u.", s_state.season,
				s_state.enabled ? "wlaczony" : "wylaczony", (unsigned int)s_holders.size());
	}
}
