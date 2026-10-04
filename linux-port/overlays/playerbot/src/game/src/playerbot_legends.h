#ifndef __INC_METIN2_PLAYERBOT_LEGENDS_H__
#define __INC_METIN2_PLAYERBOT_LEGENDS_H__

// MT2009_PLUS_LEGENDS_V1: the System Legend (the owner's design, 2 October).
//
// Four tiers a bot keeps for good, each one a row of player.playerbot_legend:
//
//   - Wyrozniajacy sie (about 4% of the bots) and Specjalny (about 2%), drawn
//     once from the pid's hash (GetPlayerBotLegendDrawnTier) - a bot's own
//     upkeep writes its row the first time a core sees it, and the first
//     channel writes the rows of every registered bot once an hour, so a
//     bot that never logged in since is in the ranking as well;
//   - Chodzaca Legenda: the 27 of PLAYERBOT_LEGEND_NAMES (MT2009_PLUS_LEGEND_NAMES_V1,
//     playerbot_legend_tier.h), always all 27 and no other, each wearing its
//     place's name - see AssignPlayerBotLegends;
//   - Czempion Krolestwa: at most one a kingdom - the Legend whose guild is
//     the first of its kingdom's guild ranking (the ladder points, then the
//     level and the experience, as the guild window ranks them), looked at
//     every PLAYERBOT_LEGEND_CHAMPION_MS. Another Legend's guild in front
//     takes the title over; a person's guild in front leaves the kingdom
//     without a Champion until a Legend's guild leads again.
//
// What a tier gives (playerbot_legend_tier.h, playerbot_types.h): HP (a
// hidden affect, PLAYERBOT_LEGEND_HP_AFFECT), strong against people and
// monsters and the experience (the engine's hooks below, server-patches/
// legends), the refine's chance (GoblinRefineBonus), a book or a Spirit Stone
// worth more reads (ManagePlayerBotSkillBooks, ManagePlayerBotGrandMasterTraining),
// and from the Specjalny up the fight with a person: the potions sooner, the
// break-off in time, the weaker foe first, the skills strongest first, and
// the gear weighed by its plus and its bonuses. A Legend founds or takes over
// a strong guild of its kingdom and builds it (more invitations, a wider
// table, a larger share of the members' experience), never shares a party or
// a guild with another Legend, and its kingdom's pick for a war is the two
// Legends' guilds more often than not. A Champion is a Legend with the best
// numbers.
//
// The reputation ("status legendy") counts the wars won, the people killed,
// the bosses' last blows and the achievements, and the notable moments are
// said on the notice line (throttled) and kept in player.playerbot_legend_event
// for the panels' ranking page. The title above a bot's nick is the client's
// (playerbot_status_tail.py), from the tier the PlayerBotTitle command carries
// (ManagePlayerBotPersonalityTitle).
//
// Kingdom wars: this world runs none - the engine's CThreeWayWar is there,
// but no quest of the server starts it (forked_road.quest is not compiled) -
// so the reputation comes from the guild wars and the fights on the maps.
//
// The LEGENDS switch of the weights file (both panels) turns all of it off:
// no bonus, no title, no notice, no Legend's guild; the table is kept.
//
// An implementation fragment in the sense playerbot_types.h describes:
// included once, after playerbot_l30_dropper.h.

namespace
{
	bool s_bPlayerBotLegendTablesReady = false;
	DWORD s_dwPlayerBotLegendTablesTry = 0;
	bool s_bPlayerBotLegendLoadedOnce = false;
	DWORD s_dwPlayerBotLegendNextReload = 0;
	DWORD s_dwPlayerBotLegendNextSeed = 0;
	DWORD s_dwPlayerBotLegendNextAssign = 0;
	DWORD s_dwPlayerBotLegendNextChampion = 0;
	DWORD s_dwPlayerBotLegendLastNotice = 0;
	std::map<std::string, DWORD> s_mapPlayerBotLegendNoticeAt;
	std::map<DWORD, DWORD> s_mapPlayerBotLegendUpkeepAt;
	std::map<DWORD, DWORD> s_mapPlayerBotLegendGuildAt;
	std::map<DWORD, DWORD> s_mapPlayerBotLegendJoinUntil;
	std::set<DWORD> s_setPlayerBotLegendRowAsked;

	const char* GetPlayerBotLegendTierName(BYTE tier)
	{
		switch (tier)
		{
			case BOT_LEGEND_DISTINGUISHED: return "Wyrozniajacy sie";
			case BOT_LEGEND_SPECIAL: return "Specjalny";
			case BOT_LEGEND_WALKING: return "Chodzaca Legenda";
			case BOT_LEGEND_CHAMPION: return "Czempion Krolestwa";
			default: return "-";
		}
	}

	// Text for a notice and for the event table: printable ASCII without the
	// quote, the backslash and the percent (DBManager::Query is a format).
	std::string CleanPlayerBotLegendText(const char* text)
	{
		std::string out;
		for (const char* p = text ? text : ""; *p && out.size() < 250; ++p)
		{
			const unsigned char c = (unsigned char)*p;
			if (c < 32 || c > 126 || c == '\'' || c == '"' || c == '\\' || c == '%')
				continue;
			out += (char)c;
		}
		return out;
	}

	// ------------------------------------------------------------- the table

	bool EnsurePlayerBotLegendTables(DWORD dwNow)
	{
		if (s_bPlayerBotLegendTablesReady)
			return true;
		if (s_dwPlayerBotLegendTablesTry != 0 && dwNow < s_dwPlayerBotLegendTablesTry)
			return false;
		s_dwPlayerBotLegendTablesTry = dwNow + 60 * 1000;
		// apply.sh creates both as well; asked here so a core that starts ahead
		// of its migrator still has them. Idempotent.
		std::unique_ptr<SQLMsg> legend(AccountDB::instance().DirectQuery(
				"CREATE TABLE IF NOT EXISTS player.playerbot_legend ("
				"pid INT UNSIGNED NOT NULL PRIMARY KEY, tier TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"empire TINYINT UNSIGNED NOT NULL DEFAULT 0, reputation INT NOT NULL DEFAULT 0, "
				"player_kills INT UNSIGNED NOT NULL DEFAULT 0, player_deaths INT UNSIGNED NOT NULL DEFAULT 0, "
				"wars_won INT UNSIGNED NOT NULL DEFAULT 0, wars_lost INT UNSIGNED NOT NULL DEFAULT 0, "
				"boss_kills INT UNSIGNED NOT NULL DEFAULT 0, achievements INT UNSIGNED NOT NULL DEFAULT 0, "
				"champion_count INT UNSIGNED NOT NULL DEFAULT 0, since DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
				"tier_since DATETIME NULL, updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
				"KEY tier_empire (tier, empire), KEY reputation_idx (reputation)) ENGINE=InnoDB"));
		std::unique_ptr<SQLMsg> events(AccountDB::instance().DirectQuery(
				"CREATE TABLE IF NOT EXISTS player.playerbot_legend_event ("
				"id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
				"pid INT UNSIGNED NOT NULL DEFAULT 0, empire TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"kind VARCHAR(24) NOT NULL DEFAULT '', text VARCHAR(255) NOT NULL DEFAULT '', "
				"KEY at_idx (at)) ENGINE=InnoDB"));
		// MT2009_PLUS_LEGEND_NAMES_V1: who holds which of the 27 places.
		std::unique_ptr<SQLMsg> names(AccountDB::instance().DirectQuery(
				"CREATE TABLE IF NOT EXISTS player.playerbot_legend_name ("
				"slot TINYINT UNSIGNED NOT NULL PRIMARY KEY, pid INT UNSIGNED NOT NULL DEFAULT 0, "
				"name VARCHAR(24) NOT NULL DEFAULT '', updated_at DATETIME NULL) ENGINE=InnoDB"));
		if (!legend.get() || legend->uiSQLErrno != 0 || !events.get() || events->uiSQLErrno != 0)
		{
			sys_err("PLAYERBOT_LEGEND: player.playerbot_legend cannot be created; the System Legend waits");
			return false;
		}
		s_bPlayerBotLegendTablesReady = true;
		return true;
	}

	bool LoadPlayerBotLegendRows()
	{
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(
				"SELECT pid, tier, empire, reputation, player_kills, player_deaths, wars_won, wars_lost, "
				"boss_kills, achievements, champion_count FROM player.playerbot_legend WHERE tier > 0"));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return false;
		std::map<DWORD, TPlayerBotLegendRow> fresh;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			DWORD pid = 0, tier = 0, empire = 0;
			if (row[0]) str_to_number(pid, row[0]);
			if (row[1]) str_to_number(tier, row[1]);
			if (row[2]) str_to_number(empire, row[2]);
			if (pid == 0 || tier == 0)
				continue;
			TPlayerBotLegendRow r;
			r.bTier = ClampPlayerBotLegendTier((BYTE)std::min<DWORD>(tier, 255));
			r.bEmpire = (BYTE)std::min<DWORD>(empire, 3);
			if (row[3]) str_to_number(r.iReputation, row[3]);
			if (row[4]) str_to_number(r.dwPlayerKills, row[4]);
			if (row[5]) str_to_number(r.dwPlayerDeaths, row[5]);
			if (row[6]) str_to_number(r.dwWarsWon, row[6]);
			if (row[7]) str_to_number(r.dwWarsLost, row[7]);
			if (row[8]) str_to_number(r.dwBossKills, row[8]);
			if (row[9]) str_to_number(r.dwAchievements, row[9]);
			if (row[10]) str_to_number(r.dwChampionCount, row[10]);
			fresh[pid] = r;
		}
		s_mapPlayerBotLegendRows.swap(fresh);
		RebuildPlayerBotLegendIndex();
		if (!s_bPlayerBotLegendLoadedOnce)
			sys_log(0, "PLAYERBOT_LEGEND: loaded %u tiered bots (%u Legends and Champions)",
					(unsigned int)s_mapPlayerBotLegendRows.size(), (unsigned int)s_vecPlayerBotLegendPids.size());
		s_bPlayerBotLegendLoadedOnce = true;
		return true;
	}

	TPlayerBotLegendRow* FindPlayerBotLegendRow(DWORD pid)
	{
		std::map<DWORD, TPlayerBotLegendRow>::iterator it = s_mapPlayerBotLegendRows.find(pid);
		return it != s_mapPlayerBotLegendRows.end() ? &it->second : NULL;
	}

	// ------------------------------------------------------------ the notices

	// The event for the panels and, when it is one to say, the notice line:
	// never two notices closer than PLAYERBOT_LEGEND_NOTICE_GAP_MS and never
	// the same key twice in PLAYERBOT_LEGEND_NOTICE_SAME_MS, unless forced (a
	// crowning, which comes once an hour at most anyway).
	void NotePlayerBotLegendEvent(DWORD pid, BYTE empire, const char* kind, const char* text,
			bool notice, bool force, const char* key = NULL)
	{
		const std::string clean = CleanPlayerBotLegendText(text);
		const std::string cleanKind = CleanPlayerBotLegendText(kind);
		DBManager::instance().Query(
				"INSERT INTO player.playerbot_legend_event (at, pid, empire, kind, text) VALUES (NOW(), %u, %u, '%s', '%s')",
				pid, (unsigned int)empire, cleanKind.c_str(), clean.c_str());
		sys_log(0, "PLAYERBOT_LEGEND: event %s pid=%u empire=%u %s", cleanKind.c_str(), pid, (unsigned int)empire, clean.c_str());
		if (!notice || !IsPlayerBotLegendsEnabled())
			return;
		const DWORD dwNow = get_dword_time();
		if (!force)
		{
			if (s_dwPlayerBotLegendLastNotice != 0 && dwNow - s_dwPlayerBotLegendLastNotice < PLAYERBOT_LEGEND_NOTICE_GAP_MS)
				return;
			if (key)
			{
				std::map<std::string, DWORD>::const_iterator at = s_mapPlayerBotLegendNoticeAt.find(key);
				if (at != s_mapPlayerBotLegendNoticeAt.end() && dwNow - at->second < PLAYERBOT_LEGEND_NOTICE_SAME_MS)
					return;
			}
		}
		if (key)
			s_mapPlayerBotLegendNoticeAt[key] = dwNow;
		s_dwPlayerBotLegendLastNotice = dwNow;
		BroadcastNotice(clean.c_str());
	}

	// ---------------------------------------------------------- the reputation

	// Reputation, on this core's copy at once and in the table by an atomic
	// add (any core may count one); never below zero. extraColumn, when
	// given, is one of the row's counters to raise by one.
	void AddPlayerBotLegendReputation(DWORD pid, int delta, const char* extraColumn = NULL)
	{
		TPlayerBotLegendRow* row = FindPlayerBotLegendRow(pid);
		if (!row)
			return;
		row->iReputation = std::max(0, row->iReputation + delta);
		if (extraColumn)
		{
			if (!strcmp(extraColumn, "player_kills")) ++row->dwPlayerKills;
			else if (!strcmp(extraColumn, "player_deaths")) ++row->dwPlayerDeaths;
			else if (!strcmp(extraColumn, "boss_kills")) ++row->dwBossKills;
			else
				extraColumn = NULL;
		}
		if (extraColumn)
			DBManager::instance().Query(
					"UPDATE player.playerbot_legend SET reputation=GREATEST(0, reputation + (%d)), %s=%s+1 WHERE pid=%u",
					delta, extraColumn, extraColumn, pid);
		else
			DBManager::instance().Query(
					"UPDATE player.playerbot_legend SET reputation=GREATEST(0, reputation + (%d)) WHERE pid=%u",
					delta, pid);
	}

	// An achievement, once: the bit, the reputation, and for a Legend the
	// notice. The table's own test keeps a second core from counting it again.
	void GrantPlayerBotLegendAchievement(LPCHARACTER ch, DWORD bit, int reputation, const char* notice)
	{
		if (!ch)
			return;
		TPlayerBotLegendRow* row = FindPlayerBotLegendRow(ch->GetPlayerID());
		if (!row || (row->dwAchievements & bit) != 0)
			return;
		row->dwAchievements |= bit;
		row->iReputation += reputation;
		DBManager::instance().Query(
				"UPDATE player.playerbot_legend SET achievements=achievements | %u, reputation=reputation + %d "
				"WHERE pid=%u AND (achievements & %u)=0",
				bit, reputation, ch->GetPlayerID(), bit);
		char text[200];
		snprintf(text, sizeof(text), "[Legenda] %s (%s) %s", ch->GetName(),
				GetPlayerBotLegendTierName(row->bTier), notice ? notice : "");
		NotePlayerBotLegendEvent(ch->GetPlayerID(), ch->GetEmpire(), "achievement", text,
				notice != NULL && IsPlayerBotLegendTier(row->bTier), false, NULL);
	}

	// ---------------------------------------------- the draw and the places

	// The rows of every registered bot that draws a tier and has none yet,
	// written by the first channel once an hour (and a minute after a start):
	// the ranking holds the bots that have not logged in since, and the
	// Legends' places can be filled from them.
	void SeedPlayerBotLegendRows()
	{
		std::set<DWORD> excluded;
		{
			std::unique_ptr<SQLMsg> shouters(AccountDB::instance().DirectQuery("SELECT pid FROM player.playerbot_shouter"));
			if (shouters.get() && shouters->uiSQLErrno == 0 && shouters->Get() && shouters->Get()->pSQLResult)
			{
				MYSQL_ROW row;
				while (NULL != (row = mysql_fetch_row(shouters->Get()->pSQLResult)))
				{
					DWORD pid = 0;
					if (row[0]) str_to_number(pid, row[0]);
					excluded.insert(pid);
				}
			}
			std::unique_ptr<SQLMsg> sidekicks(AccountDB::instance().DirectQuery("SELECT sidekick_pid FROM player.playerbot_sidekick"));
			if (sidekicks.get() && sidekicks->uiSQLErrno == 0 && sidekicks->Get() && sidekicks->Get()->pSQLResult)
			{
				MYSQL_ROW row;
				while (NULL != (row = mysql_fetch_row(sidekicks->Get()->pSQLResult)))
				{
					DWORD pid = 0;
					if (row[0]) str_to_number(pid, row[0]);
					excluded.insert(pid);
				}
			}
		}
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(
				"SELECT s.pid, pi.empire FROM common.playerbot_seed_state AS s "
				"JOIN player.player AS p ON p.id=s.pid "
				"JOIN player.player_index AS pi ON pi.id=p.account_id "
				"LEFT JOIN player.playerbot_legend AS l ON l.pid=s.pid "
				"WHERE l.pid IS NULL"));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
		{
			sys_log(0, "PLAYERBOT_LEGEND: the bots' registry is not readable; no rows seeded");
			return;
		}
		std::string values;
		unsigned int seeded = 0;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			DWORD pid = 0, empire = 0;
			if (row[0]) str_to_number(pid, row[0]);
			if (row[1]) str_to_number(empire, row[1]);
			if (pid == 0 || empire < 1 || empire > 3 || excluded.count(pid))
				continue;
			const BYTE tier = GetPlayerBotLegendDrawnTier(pid);
			if (tier == BOT_LEGEND_NONE)
				continue;
			char one[64];
			snprintf(one, sizeof(one), "%s(%u,%u,%u,NOW(),NOW())", values.empty() ? "" : ",",
					pid, (unsigned int)tier, empire);
			values += one;
			++seeded;
			if (values.size() > 6000)
			{
				DBManager::instance().Query("INSERT IGNORE INTO player.playerbot_legend (pid, tier, empire, since, tier_since) VALUES %s",
						values.c_str());
				values.clear();
			}
		}
		if (!values.empty())
			DBManager::instance().Query("INSERT IGNORE INTO player.playerbot_legend (pid, tier, empire, since, tier_since) VALUES %s",
					values.c_str());
		if (seeded > 0)
			sys_log(0, "PLAYERBOT_LEGEND: seeded %u tier rows of the registry", seeded);
	}

	struct TPlayerBotLegendPlace
	{
		DWORD pid;
		BYTE bTier;
		std::string strName;
		int iDaysAway;
		bool bRegistered;
		int iReputation;
	};

	void SetPlayerBotLegendTier(DWORD pid, BYTE tier)
	{
		DBManager::instance().Query("UPDATE player.playerbot_legend SET tier=%u, tier_since=NOW() WHERE pid=%u",
				(unsigned int)tier, pid);
		TPlayerBotLegendRow* row = FindPlayerBotLegendRow(pid);
		if (row)
			row->bTier = tier;
		RebuildPlayerBotLegendIndex();
	}

	// MT2009_PLUS_LEGEND_NAMES_V1: the Legends' places are the 27 names of
	// PLAYERBOT_LEGEND_NAMES (the owner, 3 October), the most legendary
	// first, and the world always has those 27 and no other Legend:
	//
	//   - a bot that wears a place's name is its Legend (Chodzaca Legenda, or
	//     the Champion it is already) - unless it is under
	//     PLAYERBOT_LEGEND_MIN_LEVEL, a companion, a krzykacz or a medal
	//     dropper: then it gives the name up (MovePlayerBotOffShouterName) and
	//     the place is filled as a vacant one; a person's character that wears
	//     it is reported and that place waits;
	//   - a vacant place goes to the best bot of the kingdom with the fewest
	//     Legends - a former Legend first, then the highest level - which is
	//     logged out, held out of the world and renamed once its save is old
	//     enough (PLAYERBOT_SHOUTER_IDLE_MINUTES, as the krzykacze);
	//   - a Legend never loses its place for being away (no stale rule any
	//     more); every other bot of the tiers above Specjalny goes back to
	//     Specjalny.
	//
	// player.playerbot_legend_name keeps who holds which place, for the panels.
	struct TPlayerBotLegendWearer
	{
		DWORD pid;
		BYTE bEmpire;
		int iLevel;
		bool bBot;
		bool bExact;
		std::string strLogin;
	};

	std::map<int, DWORD> s_mapPlayerBotLegendPending;

	void PromotePlayerBotLegendRow(DWORD pid, BYTE empire)
	{
		DBManager::instance().Query(
				"INSERT INTO player.playerbot_legend (pid, tier, empire, since, tier_since) VALUES (%u, %u, %u, NOW(), NOW()) "
				"ON DUPLICATE KEY UPDATE tier_since=IF(tier < %u, NOW(), tier_since), tier=GREATEST(tier, %u), empire=%u",
				pid, (unsigned int)BOT_LEGEND_WALKING, (unsigned int)empire,
				(unsigned int)BOT_LEGEND_WALKING, (unsigned int)BOT_LEGEND_WALKING, (unsigned int)empire);
		TPlayerBotLegendRow* row = FindPlayerBotLegendRow(pid);
		if (row)
		{
			if (row->bTier < BOT_LEGEND_WALKING)
				row->bTier = BOT_LEGEND_WALKING;
			row->bEmpire = empire;
		}
		else
		{
			TPlayerBotLegendRow fresh;
			fresh.bTier = BOT_LEGEND_WALKING;
			fresh.bEmpire = empire;
			s_mapPlayerBotLegendRows[pid] = fresh;
		}
		RebuildPlayerBotLegendIndex();
	}

	bool IsPlayerBotLegendSpecialPID(DWORD pid)
	{
		CPlayerBotManager& mgr = CPlayerBotManager::instance();
		return IsPlayerBotSidekickPID(pid) || IsPlayerBotShouterPID(pid) || IsPlayerBotMedalShouterPID(pid) ||
				mgr.IsMedalDropperCohortPID(pid) || IsPlayerBotArezzoCohortPID(pid) ||
				IsPlayerBotArezzoDungeonCohortPID(pid) || IsPlayerBotTakeoverHold(pid) || IsPlayerBotRetirementHold(pid);
	}

	// One vacant place: the pending pick renamed when it can be, or a new
	// pick. True when the place has its Legend now.
	bool FillPlayerBotLegendPlace(int slot, std::set<DWORD>& holders, int (&perEmpire)[4], DWORD& holderOut, BYTE& empireOut)
	{
		const char* name = PLAYERBOT_LEGEND_NAMES[slot];
		CPlayerBotManager& mgr = CPlayerBotManager::instance();
		char query[1400];

		DWORD pick = 0;
		std::map<int, DWORD>::iterator pending = s_mapPlayerBotLegendPending.find(slot);
		if (pending != s_mapPlayerBotLegendPending.end())
		{
			pick = pending->second;
			if (holders.count(pick) || IsPlayerBotLegendSpecialPID(pick) || !mgr.IsRegisteredBotPID(pick))
			{
				s_mapPlayerBotLegendPending.erase(pending);
				s_mapPlayerBotLegendNameHold.erase(pick);
				pick = 0;
			}
		}
		if (pick == 0)
		{
			// The kingdom with the fewest Legends, then the others.
			int order[3] = { 1, 2, 3 };
			for (int a = 0; a < 3; ++a)
				for (int b = a + 1; b < 3; ++b)
					if (perEmpire[order[b]] < perEmpire[order[a]])
						std::swap(order[a], order[b]);
			for (int k = 0; k < 3 && pick == 0; ++k)
			{
				const int empire = order[k];
				snprintf(query, sizeof(query),
						"SELECT p.id FROM common.playerbot_seed_state AS s "
						"JOIN player.player AS p ON p.id=s.pid "
						"JOIN account.account AS a ON a.id=p.account_id "
						"JOIN player.player_index AS pi ON pi.id=p.account_id "
						"LEFT JOIN player.playerbot_legend AS l ON l.pid=p.id "
						"WHERE pi.empire=%d AND p.level >= %d AND a.login LIKE 'playerbot\\_%%' "
						"ORDER BY (IFNULL(l.tier, 0) >= %u) DESC, p.level DESC, IFNULL(l.tier, 0) DESC, p.exp DESC, p.id ASC LIMIT 80",
						empire, PLAYERBOT_LEGEND_MIN_LEVEL, (unsigned int)BOT_LEGEND_WALKING);
				std::unique_ptr<SQLMsg> cand(AccountDB::instance().DirectQuery(query));
				if (!cand.get() || cand->uiSQLErrno != 0 || !cand->Get() || !cand->Get()->pSQLResult)
					continue;
				MYSQL_ROW row;
				while (NULL != (row = mysql_fetch_row(cand->Get()->pSQLResult)))
				{
					DWORD pid = 0;
					if (row[0]) str_to_number(pid, row[0]);
					if (pid == 0 || holders.count(pid) || IsPlayerBotLegendSpecialPID(pid) || !mgr.IsRegisteredBotPID(pid))
						continue;
					bool pendingElsewhere = false;
					for (std::map<int, DWORD>::const_iterator it = s_mapPlayerBotLegendPending.begin();
							it != s_mapPlayerBotLegendPending.end(); ++it)
						if (it->second == pid)
							pendingElsewhere = true;
					if (pendingElsewhere)
						continue;
					// Playing on another core: the next one.
					if (!mgr.IsManaged(pid) && (CHARACTER_MANAGER::instance().FindByPID(pid) || P2P_MANAGER::instance().FindByPID(pid)))
						continue;
					pick = pid;
					break;
				}
			}
			if (pick == 0)
			{
				sys_err("PLAYERBOT_LEGEND: no bot of level %d+ for the Legend's place %d (%s)",
						PLAYERBOT_LEGEND_MIN_LEVEL, slot + 1, name);
				return false;
			}
			s_mapPlayerBotLegendPending[slot] = pick;
			sys_log(0, "PLAYERBOT_LEGEND: place %d (%s) goes to pid=%u", slot + 1, name, pick);
		}

		// Out of the world first, then a save old enough to rename under.
		s_mapPlayerBotLegendNameHold[pick] = get_dword_time() + 2 * 60 * 60 * 1000;
		if (mgr.IsManaged(pick))
		{
			mgr.Despawn(pick);
			return false;
		}
		if (CHARACTER_MANAGER::instance().FindByPID(pick) || P2P_MANAGER::instance().FindByPID(pick))
			return false;
		snprintf(query, sizeof(query),
				"SELECT IFNULL(p.name,''), (p.last_play < NOW() - INTERVAL %d MINUTE), IFNULL(pi.empire,0) FROM player.player AS p "
				"JOIN player.player_index AS pi ON pi.id=p.account_id WHERE p.id=%u",
				PLAYERBOT_SHOUTER_IDLE_MINUTES, pick);
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		MYSQL_ROW row = NULL;
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult ||
				!(row = mysql_fetch_row(msg->Get()->pSQLResult)))
			return false;
		if (!row[1] || strcmp(row[1], "1") != 0)
			return false;
		const std::string oldName = row[0] ? row[0] : "";
		DWORD empire = 0;
		if (row[2]) str_to_number(empire, row[2]);
		if (empire < 1 || empire > 3 || !IsPlayerBotShouterNameFree(name))
			return false;
		// The name history keeps the old name as the bot's own, so the name
		// pool reads the Legend's as a deliberate one and never renames it.
		snprintf(query, sizeof(query),
				"INSERT IGNORE INTO common.playerbot_name_history (pid, seed_name, human_name, pool_version, renamed_at) "
				"SELECT id, name, name, 'legend', NOW() FROM player.player WHERE id=%u", pick);
		std::unique_ptr<SQLMsg> history(AccountDB::instance().DirectQuery(query));
		snprintf(query, sizeof(query), "UPDATE player.player SET name='%s' WHERE id=%u", name, pick);
		std::unique_ptr<SQLMsg> rename(AccountDB::instance().DirectQuery(query));
		if (!rename.get() || rename->uiSQLErrno != 0)
		{
			sys_err("PLAYERBOT_LEGEND: cannot rename pid=%u to %s errno=%u", pick, name,
					rename.get() ? rename->uiSQLErrno : 0U);
			return false;
		}
		s_mapPlayerBotLegendPending.erase(slot);
		s_mapPlayerBotLegendNameHold.erase(pick);
		PromotePlayerBotLegendRow(pick, (BYTE)empire);
		holders.insert(pick);
		++perEmpire[empire];
		holderOut = pick;
		empireOut = (BYTE)empire;
		sys_log(0, "PLAYERBOT_LEGEND: pid=%u %s is now the Legend %s (place %d)", pick, oldName.c_str(), name, slot + 1);
		char text[200];
		snprintf(text, sizeof(text), "[Legenda] %s z krolestwa %s zostaje Chodzaca Legenda!",
				name, GetPlayerBotKingdomName((BYTE)empire));
		NotePlayerBotLegendEvent(pick, (BYTE)empire, "legend", text, true, false, NULL);
		return true;
	}

	void AssignPlayerBotLegends()
	{
		// Who wears each name now (the name column compares without case).
		std::string list;
		for (int i = 0; i < PLAYERBOT_LEGEND_NAME_COUNT; ++i)
		{
			list += i ? ",'" : "'";
			list += PLAYERBOT_LEGEND_NAMES[i];
			list += "'";
		}
		std::string query = "SELECT p.id, p.name, IFNULL(pi.empire,0), p.level, IFNULL(a.login,'') FROM player.player AS p "
				"LEFT JOIN account.account AS a ON a.id=p.account_id "
				"LEFT JOIN player.player_index AS pi ON pi.id=p.account_id WHERE p.name IN (" + list + ")";
		std::unique_ptr<SQLMsg> worn(AccountDB::instance().DirectQuery(query.c_str()));
		if (!worn.get() || worn->uiSQLErrno != 0 || !worn->Get() || !worn->Get()->pSQLResult)
		{
			sys_err("PLAYERBOT_LEGEND: cannot read who wears the Legends' names; the places wait");
			return;
		}
		std::vector<TPlayerBotLegendWearer> wearers[PLAYERBOT_LEGEND_NAME_COUNT];
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(worn->Get()->pSQLResult)))
		{
			const int slot = GetPlayerBotLegendNameSlot(row[1]);
			if (slot < 0)
				continue;
			TPlayerBotLegendWearer w;
			DWORD empire = 0;
			w.pid = 0;
			w.iLevel = 0;
			if (row[0]) str_to_number(w.pid, row[0]);
			if (row[2]) str_to_number(empire, row[2]);
			if (row[3]) str_to_number(w.iLevel, row[3]);
			w.bEmpire = (BYTE)std::min<DWORD>(empire, 3);
			w.strLogin = row[4] ? row[4] : "";
			w.bBot = strncmp(w.strLogin.c_str(), "playerbot_", 10) == 0;
			w.bExact = row[1] && strcmp(row[1], PLAYERBOT_LEGEND_NAMES[slot]) == 0;
			wearers[slot].push_back(w);
		}

		CPlayerBotManager& mgr = CPlayerBotManager::instance();
		std::set<DWORD> holders;
		DWORD holderOf[PLAYERBOT_LEGEND_NAME_COUNT] = {};
		int perEmpire[4] = { 0, 0, 0, 0 };
		bool vacant[PLAYERBOT_LEGEND_NAME_COUNT] = {};
		bool blocked[PLAYERBOT_LEGEND_NAME_COUNT] = {};

		// The places worn already.
		for (int slot = 0; slot < PLAYERBOT_LEGEND_NAME_COUNT; ++slot)
		{
			const char* name = PLAYERBOT_LEGEND_NAMES[slot];
			for (size_t i = 0; i < wearers[slot].size(); ++i)
			{
				const TPlayerBotLegendWearer& w = wearers[slot][i];
				if (!w.bBot)
				{
					sys_err("PLAYERBOT_LEGEND: the Legend's name %s is worn by a person's character pid=%u login=%s; "
							"place %d waits (rename that character)", name, w.pid, w.strLogin.c_str(), slot + 1);
					blocked[slot] = true;
					continue;
				}
				const bool fit = holderOf[slot] == 0 && w.bExact && w.bEmpire >= 1 && w.bEmpire <= 3 &&
						w.iLevel >= PLAYERBOT_LEGEND_MIN_LEVEL && mgr.IsRegisteredBotPID(w.pid) &&
						!IsPlayerBotLegendSpecialPID(w.pid);
				if (fit)
				{
					holderOf[slot] = w.pid;
					holders.insert(w.pid);
					++perEmpire[w.bEmpire];
					if (GetPlayerBotLegendTier(w.pid) < BOT_LEGEND_WALKING)
						PromotePlayerBotLegendRow(w.pid, w.bEmpire);
					s_mapPlayerBotLegendPending.erase(slot);
					continue;
				}
				// Any other bot gives the name up.
				if (MovePlayerBotOffShouterName(w.pid, name, "PLAYERBOT_LEGEND"))
					sys_log(0, "PLAYERBOT_LEGEND: pid=%u gave the Legend's name %s up", w.pid, name);
				else
					blocked[slot] = true;
			}
			if (holderOf[slot] == 0 && !blocked[slot])
				vacant[slot] = true;
		}

		// The vacant places, the most legendary first.
		for (int slot = 0; slot < PLAYERBOT_LEGEND_NAME_COUNT; ++slot)
		{
			if (!vacant[slot])
				continue;
			BYTE empire = 0;
			FillPlayerBotLegendPlace(slot, holders, perEmpire, holderOf[slot], empire);
		}

		// Every other Legend or Champion back to Specjalny; a pick waiting for
		// its rename keeps its tier until it has the name.
		std::set<DWORD> keep = holders;
		for (std::map<int, DWORD>::const_iterator it = s_mapPlayerBotLegendPending.begin();
				it != s_mapPlayerBotLegendPending.end(); ++it)
			keep.insert(it->second);
		std::vector<std::pair<DWORD, BYTE> > demote;
		for (std::map<DWORD, TPlayerBotLegendRow>::const_iterator it = s_mapPlayerBotLegendRows.begin();
				it != s_mapPlayerBotLegendRows.end(); ++it)
			if (it->second.bTier >= BOT_LEGEND_WALKING && !keep.count(it->first))
				demote.push_back(std::make_pair(it->first, it->second.bEmpire));
		for (size_t i = 0; i < demote.size(); ++i)
		{
			SetPlayerBotLegendTier(demote[i].first, BOT_LEGEND_SPECIAL);
			NotePlayerBotLegendEvent(demote[i].first, demote[i].second, "legend_lost",
					"[Legenda] Miejsce Chodzacej Legendy przechodzi na jedna z 27 Legend.", false, false);
		}

		// One Champion a kingdom at most.
		bool champion[4] = { false, false, false, false };
		for (int slot = 0; slot < PLAYERBOT_LEGEND_NAME_COUNT; ++slot)
		{
			const DWORD pid = holderOf[slot];
			TPlayerBotLegendRow* r = pid ? FindPlayerBotLegendRow(pid) : NULL;
			if (!r || r->bTier != BOT_LEGEND_CHAMPION || r->bEmpire < 1 || r->bEmpire > 3)
				continue;
			if (champion[r->bEmpire])
				SetPlayerBotLegendTier(pid, BOT_LEGEND_WALKING);
			champion[r->bEmpire] = true;
		}

		// The places, for the panels.
		std::string values;
		for (int slot = 0; slot < PLAYERBOT_LEGEND_NAME_COUNT; ++slot)
		{
			char one[96];
			snprintf(one, sizeof(one), "%s(%d,%u,'%s',NOW())", slot ? "," : "", slot + 1, holderOf[slot],
					PLAYERBOT_LEGEND_NAMES[slot]);
			values += one;
		}
		DBManager::instance().Query("REPLACE INTO player.playerbot_legend_name (slot, pid, name, updated_at) VALUES %s",
				values.c_str());
		sys_log(0, "PLAYERBOT_LEGEND: places %u of %d held (Shinsoo %d, Chunjo %d, Jinno %d), %u waiting for a rename",
				(unsigned int)holders.size(), PLAYERBOT_LEGEND_NAME_COUNT, perEmpire[1], perEmpire[2], perEmpire[3],
				(unsigned int)s_mapPlayerBotLegendPending.size());
	}

	// ------------------------------------------------------------ the Champions

	// The kingdoms' guild rankings and the Legends' guilds, from the table:
	// a Legend may be in another core's world, or in none.
	void ElectPlayerBotChampions()
	{
		struct TRank { DWORD guild; std::string name; int ladder; };
		TRank top[4];
		for (int e = 0; e < 4; ++e)
		{
			top[e].guild = 0;
			top[e].ladder = 0;
		}
		std::unique_ptr<SQLMsg> ranking(AccountDB::instance().DirectQuery(
				"SELECT g.id, g.name, g.ladder_point, pi.empire FROM player.guild AS g "
				"JOIN player.player AS p ON p.id=g.master "
				"JOIN player.player_index AS pi ON pi.id=p.account_id "
				"ORDER BY g.ladder_point DESC, g.level DESC, g.exp DESC, g.id ASC"));
		if (!ranking.get() || ranking->uiSQLErrno != 0 || !ranking->Get() || !ranking->Get()->pSQLResult)
			return;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(ranking->Get()->pSQLResult)))
		{
			DWORD gid = 0, empire = 0;
			int ladder = 0;
			if (row[0]) str_to_number(gid, row[0]);
			if (row[2]) str_to_number(ladder, row[2]);
			if (row[3]) str_to_number(empire, row[3]);
			if (gid == 0 || empire < 1 || empire > 3 || top[empire].guild != 0)
				continue;
			top[empire].guild = gid;
			top[empire].name = row[1] ? row[1] : "";
			top[empire].ladder = ladder;
		}

		struct TLegend { DWORD pid; BYTE tier; std::string name; DWORD guild; };
		std::vector<TLegend> legends[4];
		std::unique_ptr<SQLMsg> list(AccountDB::instance().DirectQuery(
				"SELECT l.pid, l.tier, l.empire, IFNULL(p.name, ''), IFNULL(m.guild_id, 0) "
				"FROM player.playerbot_legend AS l "
				"LEFT JOIN player.player AS p ON p.id=l.pid "
				"LEFT JOIN player.guild_member AS m ON m.pid=l.pid "
				"WHERE l.tier >= 3"));
		if (!list.get() || list->uiSQLErrno != 0 || !list->Get() || !list->Get()->pSQLResult)
			return;
		while (NULL != (row = mysql_fetch_row(list->Get()->pSQLResult)))
		{
			TLegend l;
			DWORD tier = 0, empire = 0;
			l.pid = 0;
			l.guild = 0;
			if (row[0]) str_to_number(l.pid, row[0]);
			if (row[1]) str_to_number(tier, row[1]);
			if (row[2]) str_to_number(empire, row[2]);
			l.name = row[3] ? row[3] : "";
			if (row[4]) str_to_number(l.guild, row[4]);
			l.tier = (BYTE)tier;
			if (l.pid != 0 && empire >= 1 && empire <= 3)
				legends[empire].push_back(l);
		}

		for (int empire = 1; empire <= 3; ++empire)
		{
			const TLegend* current = NULL;
			const TLegend* elected = NULL;
			for (size_t i = 0; i < legends[empire].size(); ++i)
			{
				const TLegend& l = legends[empire][i];
				if (l.tier == BOT_LEGEND_CHAMPION && !current)
					current = &l;
				if (top[empire].guild != 0 && l.guild == top[empire].guild && !elected)
					elected = &l;
			}
			if (current == elected || (current && elected && current->pid == elected->pid))
				continue;
			const char* kingdom = GetPlayerBotKingdomName((BYTE)empire);
			if (current)
			{
				SetPlayerBotLegendTier(current->pid, BOT_LEGEND_WALKING);
				char text[220];
				snprintf(text, sizeof(text), "[Legenda] %s traci tytul Czempiona Krolestwa %s - gildia %s prowadzi w rankingu krolestwa.",
						current->name.c_str(), kingdom, top[empire].name.empty() ? "?" : top[empire].name.c_str());
				NotePlayerBotLegendEvent(current->pid, (BYTE)empire, "champion_lost", text, elected == NULL, true);
			}
			if (elected)
			{
				SetPlayerBotLegendTier(elected->pid, BOT_LEGEND_CHAMPION);
				DBManager::instance().Query(
						"UPDATE player.playerbot_legend SET champion_count=champion_count+1, reputation=reputation+%d, "
						"achievements=achievements | %u WHERE pid=%u",
						PLAYERBOT_LEGEND_REP_CROWNED, (unsigned int)BOT_LEGEND_ACH_CROWNED, elected->pid);
				TPlayerBotLegendRow* r = FindPlayerBotLegendRow(elected->pid);
				if (r)
				{
					++r->dwChampionCount;
					r->iReputation += PLAYERBOT_LEGEND_REP_CROWNED;
					r->dwAchievements |= BOT_LEGEND_ACH_CROWNED;
				}
				char text[220];
				snprintf(text, sizeof(text), "[Legenda] %s zostaje Czempionem Krolestwa %s - jego gildia %s prowadzi w rankingu krolestwa!",
						elected->name.c_str(), kingdom, top[empire].name.c_str());
				NotePlayerBotLegendEvent(elected->pid, (BYTE)empire, "champion", text, true, true);
			}
		}
	}

	// ------------------------------------------------------- the world's pass

	// Every core: the table, and its copy every PLAYERBOT_LEGEND_RELOAD_MS.
	// The first channel's core that holds the lock (GET_LOCK, so two cores of
	// the first channel never fill one place twice): the draw, the places,
	// the Champions, the old events.
	void ManagePlayerBotLegends(DWORD dwNow)
	{
		if (!EnsurePlayerBotLegendTables(dwNow))
			return;
		if (s_dwPlayerBotLegendNextReload == 0 || dwNow >= s_dwPlayerBotLegendNextReload)
		{
			s_dwPlayerBotLegendNextReload = dwNow + PLAYERBOT_LEGEND_RELOAD_MS;
			LoadPlayerBotLegendRows();
		}
		if (g_bChannel != 1 || !IsPlayerBotLegendsEnabled())
			return;
		if (s_dwPlayerBotLegendNextSeed == 0)
		{
			s_dwPlayerBotLegendNextSeed = dwNow + 60 * 1000;
			s_dwPlayerBotLegendNextAssign = dwNow + 90 * 1000;
			s_dwPlayerBotLegendNextChampion = dwNow + PLAYERBOT_LEGEND_FIRST_CHAMPION_MS;
			return;
		}
		const bool seed = dwNow >= s_dwPlayerBotLegendNextSeed;
		const bool assign = dwNow >= s_dwPlayerBotLegendNextAssign;
		const bool champion = dwNow >= s_dwPlayerBotLegendNextChampion;
		if (!seed && !assign && !champion)
			return;
		std::unique_ptr<SQLMsg> lock(AccountDB::instance().DirectQuery("SELECT GET_LOCK('mt2009_playerbot_legends', 0)"));
		bool locked = false;
		if (lock.get() && lock->uiSQLErrno == 0 && lock->Get() && lock->Get()->pSQLResult)
		{
			MYSQL_ROW row = mysql_fetch_row(lock->Get()->pSQLResult);
			locked = row && row[0] && atoi(row[0]) == 1;
		}
		if (!locked)
		{
			// Another core of the first channel does it; this one looks again
			// on the next clock.
			if (seed) s_dwPlayerBotLegendNextSeed = dwNow + PLAYERBOT_LEGEND_SEED_MS;
			if (assign) s_dwPlayerBotLegendNextAssign = dwNow + PLAYERBOT_LEGEND_ASSIGN_MS;
			if (champion) s_dwPlayerBotLegendNextChampion = dwNow + PLAYERBOT_LEGEND_CHAMPION_MS;
			return;
		}
		if (seed)
		{
			s_dwPlayerBotLegendNextSeed = dwNow + PLAYERBOT_LEGEND_SEED_MS;
			SeedPlayerBotLegendRows();
			DBManager::instance().Query("DELETE FROM player.playerbot_legend_event WHERE at < NOW() - INTERVAL %d DAY",
					PLAYERBOT_LEGEND_EVENT_KEEP_DAYS);
		}
		if (assign)
		{
			s_dwPlayerBotLegendNextAssign = dwNow + PLAYERBOT_LEGEND_ASSIGN_MS;
			LoadPlayerBotLegendRows();
			AssignPlayerBotLegends();
		}
		if (champion)
		{
			s_dwPlayerBotLegendNextChampion = dwNow + PLAYERBOT_LEGEND_CHAMPION_MS;
			LoadPlayerBotLegendRows();
			ElectPlayerBotChampions();
		}
		std::unique_ptr<SQLMsg> unlock(AccountDB::instance().DirectQuery("SELECT RELEASE_LOCK('mt2009_playerbot_legends')"));
	}

	// ------------------------------------------------------- a Legend's guild

	// The guild a Legend without one joins to take it over: a bot guild of
	// its kingdom with room, no Legend for a master and no war on, the
	// highest tier, then the highest level, then the most ladder points.
	CGuild* FindPlayerBotLegendGuildToTake(LPCHARACTER ch)
	{
		if (!s_bPlayerBotGuildInfoLoaded)
			LoadPlayerBotGuildInfo();
		CGuild* best = NULL;
		long long bestScore = -1;
		for (std::map<DWORD, TPlayerBotGuildInfo>::const_iterator it = s_mapPlayerBotGuildInfo.begin();
				it != s_mapPlayerBotGuildInfo.end(); ++it)
		{
			if (it->second.bEmpire != ch->GetEmpire())
				continue;
			CGuild* g = CGuildManager::instance().FindGuild(it->first);
			if (!g || !CPlayerBotManager::instance().IsRegisteredBotPID(g->GetMasterPID()) ||
					IsPlayerBotLegendGuild(g) || g->UnderAnyWar() != 0 || IsPlayerBotGuildRaidingTower(it->first) ||
					g->GetMemberCount() >= g->GetMaxMemberCount())
				continue;
			const long long score = (long long)(GUILD_TIER_ORDINARY - it->second.bTier) * 1000000000LL +
					(long long)g->GetLevel() * 10000000LL + (long long)std::max(0, g->GetLadderPoint()) * 100LL +
					g->GetMemberCount();
			if (score > bestScore)
			{
				bestScore = score;
				best = g;
			}
		}
		return best;
	}

	// The Legend's guild, every PLAYERBOT_LEGEND_GUILD_CHECK_MS on the first
	// channel (where the guilds are founded and recruited): a rival's guild
	// left, a bot guild taken over, none - one joined to be taken over, or
	// one founded. A person's guild the Legend was invited into is the
	// person's and is left alone.
	void ManagePlayerBotLegendGuild(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		const DWORD pid = ch->GetPlayerID();
		if (g_bChannel != 1 || IsPlayerBotDropper(state.bPersonality) || IsPlayerBotSidekickPID(pid) ||
				IsPlayerBotShouterPID(pid) || IsPlayerBotAwaitingGuildInvite(pid))
			return;
		DWORD& next = s_mapPlayerBotLegendGuildAt[pid];
		if (next != 0 && dwNow < next)
			return;
		next = dwNow + PLAYERBOT_LEGEND_GUILD_CHECK_MS;

		CGuild* guild = ch->GetGuild();
		if (guild)
		{
			if (guild->GetMasterPID() == pid)
			{
				// Its guild is of the strong: the elite's place in the tiers.
				if (!s_bPlayerBotGuildInfoLoaded)
					LoadPlayerBotGuildInfo();
				std::map<DWORD, TPlayerBotGuildInfo>::iterator info = s_mapPlayerBotGuildInfo.find(guild->GetID());
				if (info != s_mapPlayerBotGuildInfo.end() && info->second.bTier != GUILD_TIER_ELITE)
				{
					info->second.bTier = GUILD_TIER_ELITE;
					SavePlayerBotGuildInfo(guild->GetID(), info->second);
					sys_log(0, "PLAYERBOT_LEGEND: guild %s of the Legend %s raised to the elite", guild->GetName(), ch->GetName());
				}
				return;
			}
			if (!IsPlayerBotGuild(guild) || guild->UnderAnyWar() != 0 || IsPlayerBotGuildRaidingTower(guild->GetID()))
				return;
			if (IsPlayerBotLegendGuild(guild))
			{
				// A rival's guild: never one guild with another Legend.
				if (guild->RequestRemoveMember(pid))
				{
					s_mapPlayerBotLegendJoinUntil[pid] = dwNow + 10 * 60 * 1000;
					sys_log(0, "PLAYERBOT_LEGEND: %s leaves %s, the guild of a rival Legend", ch->GetName(), guild->GetName());
				}
				return;
			}
			if (guild->ChangeMasterTo(pid))
			{
				char text[200];
				snprintf(text, sizeof(text), "[Legenda] Chodzaca Legenda %s przejmuje gildie %s!", ch->GetName(), guild->GetName());
				NotePlayerBotLegendEvent(pid, ch->GetEmpire(), "guild_taken", text, true, false, NULL);
			}
			return;
		}
		if (ch->GetLevel() < PLAYERBOT_GUILD_MIN_LEVEL)
			return;
		std::map<DWORD, DWORD>::const_iterator wait = s_mapPlayerBotLegendJoinUntil.find(pid);
		if (wait != s_mapPlayerBotLegendJoinUntil.end() && dwNow < wait->second)
			return;
		if (CGuild* take = FindPlayerBotLegendGuildToTake(ch))
		{
			take->RequestAddMember(ch, PLAYERBOT_GUILD_MEMBER_GRADE);
			s_mapPlayerBotLegendJoinUntil[pid] = dwNow + 3 * 60 * 1000;
			sys_log(0, "PLAYERBOT_LEGEND: %s joins %s to lead it", ch->GetName(), take->GetName());
			return;
		}
		if (ch->GetGold() >= (int)(PLAYERBOT_GUILD_CREATE_FEE + PLAYERBOT_GUILD_GOLD_RESERVE) &&
				FoundPlayerBotGuild(ch, GUILD_TIER_ELITE))
		{
			state.bFoundedGuild = true;
			CGuild* founded = ch->GetGuild();
			char text[200];
			snprintf(text, sizeof(text), "[Legenda] Chodzaca Legenda %s zaklada gildie%s%s!", ch->GetName(),
					founded ? " " : "", founded ? founded->GetName() : "");
			NotePlayerBotLegendEvent(pid, ch->GetEmpire(), "guild_founded", text, true, false, NULL);
		}
	}

	// ------------------------------------------------------- a bot's upkeep

	// The hidden affect that carries the tier's HP: its share of the bot's
	// health without it, put back when the health moved by more than a
	// fiftieth (a level, an armour).
	void SyncPlayerBotLegendHealth(LPCHARACTER ch, BYTE tier)
	{
		CAffect* aff = ch->FindAffect(PLAYERBOT_LEGEND_HP_AFFECT);
		const long current = aff ? aff->lApplyValue : 0;
		const int permille = PLAYERBOT_LEGEND_HP_PERMILLE[ClampPlayerBotLegendTier(tier)];
		const long base = (long)ch->GetMaxHP() - current;
		const long want = permille > 0 && base > 0 ? base * permille / 1000 : 0;
		if (want <= 0)
		{
			if (aff)
				ch->RemoveAffect(PLAYERBOT_LEGEND_HP_AFFECT);
			return;
		}
		if (aff && labs(want - current) <= std::max(20L, want / 50))
			return;
		ch->AddAffect(PLAYERBOT_LEGEND_HP_AFFECT, POINT_MAX_HP, want, 0, INFINITE_AFFECT_DURATION, 0, true);
	}

	// One bot, every PLAYERBOT_LEGEND_UPKEEP_MS: its row, its HP, its
	// achievements, a rival in its party, and a Legend's guild.
	void ManagePlayerBotLegend(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead() || !s_bPlayerBotLegendLoadedOnce)
			return;
		const DWORD pid = ch->GetPlayerID();
		DWORD& next = s_mapPlayerBotLegendUpkeepAt[pid];
		if (next != 0 && dwNow < next)
			return;
		next = dwNow + PLAYERBOT_LEGEND_UPKEEP_MS + (DWORD)(PlayerBotLegendHash(pid) % 2000U);

		if (IsPlayerBotSidekickPID(pid) || IsPlayerBotShouterPID(pid))
		{
			if (ch->FindAffect(PLAYERBOT_LEGEND_HP_AFFECT))
				ch->RemoveAffect(PLAYERBOT_LEGEND_HP_AFFECT);
			return;
		}
		// The draw's row, once a core sees the bot.
		if (!FindPlayerBotLegendRow(pid) && s_setPlayerBotLegendRowAsked.insert(pid).second)
		{
			const BYTE drawn = GetPlayerBotLegendDrawnTier(pid);
			if (drawn != BOT_LEGEND_NONE)
			{
				DBManager::instance().Query(
						"INSERT IGNORE INTO player.playerbot_legend (pid, tier, empire, since, tier_since) VALUES (%u, %u, %u, NOW(), NOW())",
						pid, (unsigned int)drawn, (unsigned int)ch->GetEmpire());
				TPlayerBotLegendRow fresh;
				fresh.bTier = drawn;
				fresh.bEmpire = ch->GetEmpire();
				s_mapPlayerBotLegendRows[pid] = fresh;
			}
		}

		const BYTE tier = GetPlayerBotLegendTier(pid);
		SyncPlayerBotLegendHealth(ch, tier);
		if (tier == BOT_LEGEND_NONE)
			return;

		TPlayerBotLegendRow* row = FindPlayerBotLegendRow(pid);
		if (row)
		{
			LPITEM weapon = ch->GetWear(WEAR_WEAPON);
			if (weapon && weapon->GetType() == ITEM_WEAPON && weapon->GetRefineLevel() >= 9)
				GrantPlayerBotLegendAchievement(ch, BOT_LEGEND_ACH_WEAPON_PLUS9, PLAYERBOT_LEGEND_REP_WEAPON_PLUS9,
						"wykuwa bron +9!");
			if (ch->GetLevel() >= 75)
				GrantPlayerBotLegendAchievement(ch, BOT_LEGEND_ACH_LEVEL75, PLAYERBOT_LEGEND_REP_LEVEL75, NULL);
			if (ch->GetLevel() >= 99)
				GrantPlayerBotLegendAchievement(ch, BOT_LEGEND_ACH_LEVEL99, PLAYERBOT_LEGEND_REP_LEVEL99,
						"osiaga 99 poziom!");
			if (row->dwPlayerKills >= 100)
				GrantPlayerBotLegendAchievement(ch, BOT_LEGEND_ACH_HUNDRED_KILLS, PLAYERBOT_LEGEND_REP_HUNDRED_KILLS,
						"pokonal juz stu graczy!");
		}

		if (!IsPlayerBotLegendTier(tier))
			return;
		// A party with a rival in it is left (one from before the promotion).
		if (ch->GetParty() && PlayerBotPartyHasLegendRival(ch, ch->GetParty()))
		{
			LeavePlayerBotParty(ch);
			sys_log(0, "PLAYERBOT_LEGEND: %s leaves a party with a rival Legend", ch->GetName());
		}
		ManagePlayerBotLegendGuild(ch, state, dwNow);
	}

	// ------------------------------------------------------------ the wars

	// A field war's end (ManagePlayerBotGuildWars, the first channel): the
	// reputation and the counters of every bot of a tier in the two guilds,
	// by the table's membership (in any core's world or none), and the
	// notice when a Champion's or a Legend's guild won.
	void NotePlayerBotLegendGuildWarOver(CGuild* g1, CGuild* g2, CGuild* winner, bool playerWar)
	{
		if (!g1 || !g2 || !winner || !IsPlayerBotLegendsEnabled())
			return;
		CGuild* loser = winner == g1 ? g2 : g1;
		DBManager::instance().Query(
				"UPDATE player.playerbot_legend AS l JOIN player.guild_member AS m ON m.pid=l.pid "
				"SET l.wars_won=l.wars_won+1, l.reputation=l.reputation+IF(l.pid=%u, %d, %d) "
				"WHERE m.guild_id=%u AND l.tier > 0",
				winner->GetMasterPID(), PLAYERBOT_LEGEND_REP_WAR_WIN_MASTER, PLAYERBOT_LEGEND_REP_WAR_WIN_MEMBER,
				winner->GetID());
		DBManager::instance().Query(
				"UPDATE player.playerbot_legend AS l JOIN player.guild_member AS m ON m.pid=l.pid "
				"SET l.wars_lost=l.wars_lost+1 WHERE m.guild_id=%u AND l.tier > 0", loser->GetID());

		const DWORD winMaster = winner->GetMasterPID();
		const DWORD loseMaster = loser->GetMasterPID();
		const BYTE winTier = GetPlayerBotLegendTier(winMaster);
		const BYTE loseTier = GetPlayerBotLegendTier(loseMaster);
		const BYTE empire = GetPlayerBotGuildEmpire(IsPlayerBotGuild(winner) ? winner : loser);
		LPCHARACTER winChar = CHARACTER_MANAGER::instance().FindByPID(winMaster);
		LPCHARACTER loseChar = CHARACTER_MANAGER::instance().FindByPID(loseMaster);
		const char* winName = winChar ? winChar->GetName() : (winner->GetMasterCharacter() ? winner->GetMasterCharacter()->GetName() : "?");
		const char* loseName = loseChar ? loseChar->GetName() : (loser->GetMasterCharacter() ? loser->GetMasterCharacter()->GetName() : "?");
		char text[240];
		if (IsPlayerBotLegendTier(winTier) && IsPlayerBotLegendTier(loseTier))
		{
			snprintf(text, sizeof(text), "[Legenda] Rywalizacja Legend: gildia %s (%s) pokonuje gildie %s (%s)!",
					winner->GetName(), winName, loser->GetName(), loseName);
			NotePlayerBotLegendEvent(winMaster, empire, "rival_war", text, true, false, NULL);
		}
		else if (winTier == BOT_LEGEND_CHAMPION)
		{
			snprintf(text, sizeof(text), "[Legenda] Gildia Czempiona %s, %s, wygrywa wojne z %s!",
					winName, winner->GetName(), loser->GetName());
			NotePlayerBotLegendEvent(winMaster, empire, "champion_war", text, true, false, NULL);
		}
		else if (IsPlayerBotLegendTier(loseTier) && playerWar && !IsPlayerBotGuild(winner))
		{
			snprintf(text, sizeof(text), "[Legenda] Gildia graczy %s pokonuje gildie %s %s (%s)!",
					winner->GetName(), loseTier == BOT_LEGEND_CHAMPION ? "Czempiona" : "Chodzacej Legendy",
					loseName, loser->GetName());
			NotePlayerBotLegendEvent(loseMaster, empire, "legend_war_lost", text, true, false, NULL);
		}
		else if (IsPlayerBotLegendTier(winTier))
		{
			snprintf(text, sizeof(text), "[Legenda] Gildia Chodzacej Legendy %s, %s, wygrywa wojne z %s.",
					winName, winner->GetName(), loser->GetName());
			NotePlayerBotLegendEvent(winMaster, empire, "legend_war", text, false, false, NULL);
		}
	}

	// One read of a class book that landed and raised nothing: the tier's
	// further reads, each the step another landed read would have made - a
	// level once the count is full (LearnSkillByBook's own rule).
	void ApplyPlayerBotLegendBookReads(LPCHARACTER ch, DWORD skillVnum, BYTE oldLevel, int readCountBefore)
	{
		const int reads = GetPlayerBotLegendBookReads(ch);
		if (reads <= 1 || !ch || ch->GetSkillLevel(skillVnum) != oldLevel ||
				ch->GetSkillMasterType(skillVnum) != SKILL_MASTER)
			return;
		char flag[64];
		snprintf(flag, sizeof(flag), "traning_master_skill.%u.read_count", skillVnum);
		int count = ch->GetQuestFlag(flag);
		if (count <= readCountBefore)
			return;   // the read failed
		const int need = (int)ch->GetSkillLevel(skillVnum) - 20;
		for (int extra = 1; extra < reads; ++extra)
		{
			if (count >= need)
			{
				ch->SkillLevelUp(skillVnum, CHARACTER::SKILL_UP_BY_BOOK);
				ch->SetQuestFlag(flag, 0);
				sys_log(0, "PLAYERBOT_LEGEND: book read counted %d times pid=%u name=%s skill=%u level=%u->%u",
						reads, ch->GetPlayerID(), ch->GetName(), skillVnum, (unsigned int)oldLevel,
						(unsigned int)ch->GetSkillLevel(skillVnum));
				return;
			}
			++count;
		}
		ch->SetQuestFlag(flag, count);
	}

}

// ---------------------------------------------------------- the engine's hooks
//
// server-patches/legends (MT2009_PLUS_LEGENDS_V1): called from battle.cpp's
// CalcAttBonus, char_battle.cpp's GiveExp and CHARACTER::Dead. Bots only;
// the switch off, each gives back what it was handed.

// The blow of a bot of a tier: stronger against people and against monsters.
int PlayerBotLegendAttackBonus(LPCHARACTER pkAttacker, LPCHARACTER pkVictim, int iAtk)
{
	if (!pkAttacker || !pkVictim || iAtk <= 0 || !pkAttacker->IsPC())
		return iAtk;
	const BYTE tier = ClampPlayerBotLegendTier(GetPlayerBotLegendTierOf(pkAttacker));
	if (tier == BOT_LEGEND_NONE)
		return iAtk;
	const int permille = pkVictim->IsPC() ? PLAYERBOT_LEGEND_VS_HUMAN_PERMILLE[tier]
			: (pkVictim->IsNPC() ? PLAYERBOT_LEGEND_VS_MONSTER_PERMILLE[tier] : 0);
	if (permille <= 0)
		return iAtk;
	return (int)((long long)iAtk * (1000 + permille) / 1000);
}

// The experience of a bot of a tier.
int PlayerBotLegendExpBonus(LPCHARACTER to, int iExp)
{
	if (!to || iExp <= 0)
		return iExp;
	const BYTE tier = ClampPlayerBotLegendTier(GetPlayerBotLegendTierOf(to));
	if (tier == BOT_LEGEND_NONE || PLAYERBOT_LEGEND_EXP_PERMILLE[tier] <= 0)
		return iExp;
	const long long boosted = (long long)iExp * (1000 + PLAYERBOT_LEGEND_EXP_PERMILLE[tier]) / 1000;
	return boosted > INT_MAX ? INT_MAX : (int)boosted;
}

// A death: a person killed by a bot of a tier, a Legend or a Champion killed
// by a person (the notice, nothing more), a boss's last blow.
// MT2009_PLUS_WEEKLY_RANKING_V1: a player killed, for the weekly ranking
// (playerbot_weekly_rank.h, included later) - whatever the LEGENDS switch says.
void WeeklyRankOnPlayerDeath(LPCHARACTER victim, LPCHARACTER killer);

void PlayerBotLegendOnDeath(LPCHARACTER victim, LPCHARACTER killer)
{
	WeeklyRankOnPlayerDeath(victim, killer);
	// MT2009_PLUS_GUILD_WAR_ARENA_V1: a kill of one of the bots' wars, for its
	// reward and the map of the arena's deaths (playerbot_guild_war.h).
	PlayerBotGuildWarOnDeath(victim, killer);
	if (!victim || !killer || victim == killer || !IsPlayerBotLegendsEnabled())
		return;
	const bool killerBot = IsPlayerBotLegendBot(killer);
	const bool victimBot = IsPlayerBotLegendBot(victim);
	const bool killerPerson = killer->IsPC() && killer->GetDesc() && !killer->GetDesc()->IsBot();
	const bool victimPerson = victim->IsPC() && victim->GetDesc() && !victim->GetDesc()->IsBot();

	if (killerBot && victimPerson)
	{
		const BYTE tier = GetPlayerBotLegendTierOf(killer);
		if (tier == BOT_LEGEND_NONE)
			return;
		AddPlayerBotLegendReputation(killer->GetPlayerID(), PLAYERBOT_LEGEND_REP_PLAYER_KILL, "player_kills");
		if (tier == BOT_LEGEND_CHAMPION)
		{
			char text[200];
			snprintf(text, sizeof(text), "[Legenda] Czempion Krolestwa %s, %s, pokonuje gracza %s.",
					GetPlayerBotKingdomName(killer->GetEmpire()), killer->GetName(), victim->GetName());
			char key[48];
			snprintf(key, sizeof(key), "champion_kill_%u", killer->GetPlayerID());
			NotePlayerBotLegendEvent(killer->GetPlayerID(), killer->GetEmpire(), "champion_kill", text, true, false, key);
		}
		return;
	}
	if (victimBot && killerPerson)
	{
		const BYTE tier = GetPlayerBotLegendTierOf(victim);
		if (tier == BOT_LEGEND_NONE)
			return;
		AddPlayerBotLegendReputation(victim->GetPlayerID(), -PLAYERBOT_LEGEND_REP_DEATH_BY_PLAYER, "player_deaths");
		if (!IsPlayerBotLegendTier(tier))
			return;
		char text[220];
		if (tier == BOT_LEGEND_CHAMPION)
			snprintf(text, sizeof(text), "[Legenda] Gracz %s pokonuje Czempiona Krolestwa %s, %s!",
					killer->GetName(), GetPlayerBotKingdomName(victim->GetEmpire()), victim->GetName());
		else
			snprintf(text, sizeof(text), "[Legenda] Gracz %s pokonuje Chodzaca Legende %s (%s)!",
					killer->GetName(), victim->GetName(), GetPlayerBotKingdomName(victim->GetEmpire()));
		char key[48];
		snprintf(key, sizeof(key), "defeated_%u", victim->GetPlayerID());
		NotePlayerBotLegendEvent(victim->GetPlayerID(), victim->GetEmpire(), "defeated", text, true, false, key);
		return;
	}
	if (killerBot && !victim->IsPC() && !victim->IsStone() && victim->GetMobRank() >= MOB_RANK_BOSS)
	{
		if (GetPlayerBotLegendTierOf(killer) == BOT_LEGEND_NONE)
			return;
		AddPlayerBotLegendReputation(killer->GetPlayerID(), PLAYERBOT_LEGEND_REP_BOSS_KILL, "boss_kills");
		GrantPlayerBotLegendAchievement(killer, BOT_LEGEND_ACH_FIRST_BOSS, PLAYERBOT_LEGEND_REP_FIRST_BOSS, NULL);
	}
}

#endif
