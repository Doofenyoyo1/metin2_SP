#ifndef __INC_METIN2_PLAYERBOT_SHOUTERS_H__
#define __INC_METIN2_PLAYERBOT_SHOUTERS_H__

// MT2009_PLUS_SHOUTERS_V1: "Krzykacze w M1" - three bots of their own on every
// world, one a kingdom, on top of the population (PLAYERBOT_AUTOSPAWN_COUNT
// does not count them and cannot take them) and switched by the SHOUTERS key
// of the weights file (both panels, on by default, read every five seconds).
//
//   - apka2009 (Shinsoo), appka2009 (Chunjo), apppka2009 (Jinno): a seeded
//     identity of the kingdom, never played, picked once, renamed and written
//     into player.playerbot_shouter - the same character at every start. A
//     name somebody else already wears is reported (PLAYERBOT_SHOUTER: ... is
//     taken) and that kingdom goes without, never under another name;
//   - under level PLAYERBOT_SHOUTER_LEVEL it is any bot: it hunts and levels;
//   - at the level its experience stops (AFFECT_EXP_BLOCK) and it stands by
//     the general store (9003, Handlarka Roznosci) of its kingdom's first
//     village with no weapon and no armour on (kept in the bag): no hunt, no
//     trade, no party, no guild, no whisper answered (they are not answered
//     at any level) - its tick is this file's;
//   - and it calls out one of PLAYERBOT_SHOUTER_LINES (playerbot_shouter_lines.h)
//     once in every 20-30 lines its kingdom's shout channel carries - the
//     bots' and the players', of this core and of the others (the P2P shout,
//     CPlayerBotManager::OnPeerShout) - never sooner than
//     PLAYERBOT_SHOUTER_MIN_GAP_MS after its last one and never the same line
//     twice in a row. A quiet channel keeps it quiet.
//
// Every core that hosts a kingdom's first village on the first channel keeps
// that kingdom's shouter; every other core only knows who they are (the
// exclusions: the cohort, the late joiners, the life schedule, the channel
// moves, the chatter, the companions' picker, whispers and guild invites).
//
// An implementation fragment in the sense playerbot_types.h describes:
// included once, after playerbot_bpbots.h.

#include "playerbot_shouter_lines.h"

namespace
{
	const char* const PLAYERBOT_SHOUTER_NAMES[4] = { "", "apka2009", "appka2009", "apppka2009" };
	// A line every this many lines of the channel, drawn anew after each.
	const int PLAYERBOT_SHOUTER_EVERY_MIN = 20;
	const int PLAYERBOT_SHOUTER_EVERY_MAX = 30;
	// And never two of one bot closer together than this.
	const DWORD PLAYERBOT_SHOUTER_MIN_GAP_MS = 10 * 60 * 1000;
	// Its post: this far round the merchant, on walkable ground.
	const int PLAYERBOT_SHOUTER_POST_MIN_RADIUS = 170;
	const int PLAYERBOT_SHOUTER_POST_MAX_RADIUS = 260;
	const int PLAYERBOT_SHOUTER_ARRIVED = 150;
	// Further than this, or on another map, or walking longer than the
	// second, and it is put at its post the way a return scroll does.
	const int PLAYERBOT_SHOUTER_WALK_MAX = 6000;
	const DWORD PLAYERBOT_SHOUTER_WALK_MAX_MS = 60 * 1000;
	const DWORD PLAYERBOT_SHOUTER_SPAWN_RETRY_MS = 30 * 1000;
	const DWORD PLAYERBOT_SHOUTER_CREATE_RETRY_MS = 10 * 60 * 1000;
	const DWORD PLAYERBOT_SHOUTER_RELOAD_MS = 5 * 60 * 1000;
	// An identity saved this long ago is out of the db core's cache, so its
	// row may be renamed (as the companions' picker has it).
	const int PLAYERBOT_SHOUTER_IDLE_MINUTES = 30;

	struct TPlayerBotShouter
	{
		DWORD pid;
		// This core keeps it (its village here, first channel).
		bool bHere;
		DWORD dwNextSpawnTry;
		DWORD dwNextCreateTry;
		// Its post, once worked out, and the walk to it.
		bool bPost;
		long lPostX, lPostY;
		DWORD dwWalkSince;
		DWORD dwNextJump;
		DWORD dwNextGearTry;
		bool bAtPost;
		// The shouting: the channel's count at its last line, how many more it
		// waits for, when it last spoke and what.
		bool bArmed;
		unsigned int uSeenAtLine;
		int iEvery;
		DWORD dwLastLine;
		int iLastLine;
		unsigned int uLines;

		TPlayerBotShouter() : pid(0), bHere(false), dwNextSpawnTry(0), dwNextCreateTry(0), bPost(false),
			lPostX(0), lPostY(0), dwWalkSince(0), dwNextJump(0), dwNextGearTry(0), bAtPost(false),
			bArmed(false), uSeenAtLine(0), iEvery(0), dwLastLine(0), iLastLine(-1), uLines(0) {}
	};

	TPlayerBotShouter s_aPlayerBotShouters[4];
	bool s_bPlayerBotShoutersLoaded = false;
	bool s_bPlayerBotShouterTable = false;
	DWORD s_dwPlayerBotShoutersLoadedAt = 0;
	int s_iPlayerBotShoutersSwitchSeen = -1;

	bool EnsurePlayerBotShouterTable()
	{
		if (s_bPlayerBotShouterTable)
			return true;
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(
				"CREATE TABLE IF NOT EXISTS player.playerbot_shouter ("
				"empire TINYINT UNSIGNED NOT NULL PRIMARY KEY, "
				"pid INT UNSIGNED NOT NULL, "
				"name VARCHAR(24) NOT NULL, "
				"created_at DATETIME NOT NULL, "
				"UNIQUE KEY pid (pid)) ENGINE=InnoDB"));
		s_bPlayerBotShouterTable = msg.get() && msg->uiSQLErrno == 0;
		if (!s_bPlayerBotShouterTable)
			sys_err("PLAYERBOT_SHOUTER: the table cannot be created errno=%u", msg.get() ? msg->uiSQLErrno : 0U);
		return s_bPlayerBotShouterTable;
	}

	void LoadPlayerBotShouters(DWORD dwNow)
	{
		s_bPlayerBotShoutersLoaded = true;
		s_dwPlayerBotShoutersLoadedAt = dwNow;
		if (!EnsurePlayerBotShouterTable())
			return;
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(
				"SELECT empire, pid FROM player.playerbot_shouter"));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			unsigned int empire = 0;
			DWORD pid = 0;
			if (row[0]) str_to_number(empire, row[0]);
			if (row[1]) str_to_number(pid, row[1]);
			if (empire >= 1 && empire <= 3 && pid != 0)
				s_aPlayerBotShouters[empire].pid = pid;
		}
	}

	// Who they are, on any core: read the first time it is asked and again
	// every few minutes, so a shouter made on another core is known here.
	bool IsPlayerBotShouterPID(DWORD pid)
	{
		if (pid == 0)
			return false;
		const DWORD dwNow = get_dword_time();
		if (!s_bPlayerBotShoutersLoaded || dwNow - s_dwPlayerBotShoutersLoadedAt >= PLAYERBOT_SHOUTER_RELOAD_MS)
			LoadPlayerBotShouters(dwNow);
		for (int e = 1; e <= 3; ++e)
			if (s_aPlayerBotShouters[e].pid == pid)
				return true;
		return false;
	}

	TPlayerBotShouter* GetPlayerBotShouter(DWORD pid)
	{
		if (!IsPlayerBotShouterPID(pid))
			return NULL;
		for (int e = 1; e <= 3; ++e)
			if (s_aPlayerBotShouters[e].pid == pid)
				return &s_aPlayerBotShouters[e];
		return NULL;
	}

	// The lines are written in UTF-8; the client reads CP1250. The Polish
	// letters and the typographic quotes and dashes have a place there, and
	// anything else becomes '?'.
	void ConvertPlayerBotShouterText(const char* in, char* out, size_t size)
	{
		size_t o = 0;
		const unsigned char* p = (const unsigned char*)in;
		while (*p && o + 1 < size)
		{
			unsigned int cp = *p;
			int extra = 0;
			if (cp >= 0xF0) { cp &= 0x07; extra = 3; }
			else if (cp >= 0xE0) { cp &= 0x0F; extra = 2; }
			else if (cp >= 0xC0) { cp &= 0x1F; extra = 1; }
			++p;
			for (int i = 0; i < extra && (*p & 0xC0) == 0x80; ++i, ++p)
				cp = (cp << 6) | (*p & 0x3F);
			unsigned char c = '?';
			if (cp < 0x80)
				c = (unsigned char)cp;
			else
			{
				switch (cp)
				{
					case 0x0105: c = 0xB9; break; case 0x0104: c = 0xA5; break;
					case 0x0107: c = 0xE6; break; case 0x0106: c = 0xC6; break;
					case 0x0119: c = 0xEA; break; case 0x0118: c = 0xCA; break;
					case 0x0142: c = 0xB3; break; case 0x0141: c = 0xA3; break;
					case 0x0144: c = 0xF1; break; case 0x0143: c = 0xD1; break;
					case 0x00F3: c = 0xF3; break; case 0x00D3: c = 0xD3; break;
					case 0x015B: c = 0x9C; break; case 0x015A: c = 0x8C; break;
					case 0x017A: c = 0x9F; break; case 0x0179: c = 0x8F; break;
					case 0x017C: c = 0xBF; break; case 0x017B: c = 0xAF; break;
					case 0x201E: c = 0x84; break; case 0x201D: c = 0x94; break;
					case 0x201C: c = 0x93; break; case 0x2019: c = 0x92; break;
					case 0x2018: c = 0x91; break; case 0x2013: c = 0x96; break;
					case 0x2014: c = 0x97; break; case 0x2026: c = 0x85; break;
					default: break;
				}
			}
			out[o++] = (char)c;
		}
		out[o] = 0;
	}

	// The experience stops at the shouter's level and nowhere below it,
	// whatever a personality or a dropper's band would lock it at.
	void ManagePlayerBotShouterExpLock(LPCHARACTER ch)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		if (!ch)
			return;
		const bool shouldLock = ch->GetLevel() >= PLAYERBOT_SHOUTER_LEVEL;
		const bool locked = ch->FindAffect(AFFECT_EXP_BLOCK) != NULL;
		if (shouldLock == locked)
			return;
		if (shouldLock)
			ch->AddAffect(AFFECT_EXP_BLOCK, POINT_NONE, 0, 0, INFINITE_AFFECT_DURATION, 0, true, true);
		else
			ch->RemoveAffect(AFFECT_EXP_BLOCK);
		sys_log(0, "PLAYERBOT_SHOUTER: exp %s pid=%u name=%s level=%u",
				shouldLock ? "locked" : "lock lifted", ch->GetPlayerID(), ch->GetName(), (unsigned)ch->GetLevel());
#else
		(void)ch;
#endif
	}

	// The kingdom's shouter: its row, or a new one - the name looked up
	// first (somebody else's is reported and that kingdom goes without), then
	// a never-played identity of the kingdom from the far end of the seed,
	// renamed and written down. Only on the core that keeps it.
	DWORD CreatePlayerBotShouter(BYTE empire)
	{
		const char* name = PLAYERBOT_SHOUTER_NAMES[empire];
		char query[1600];
		snprintf(query, sizeof(query),
				"SELECT p.id, IFNULL(a.login,''), IFNULL(pi.empire,0) FROM player.player AS p "
				"LEFT JOIN account.account AS a ON a.id=p.account_id "
				"LEFT JOIN player.player_index AS pi ON pi.id=p.account_id WHERE p.name='%s'", name);
		std::unique_ptr<SQLMsg> taken(AccountDB::instance().DirectQuery(query));
		if (!taken.get() || taken->uiSQLErrno != 0 || !taken->Get() || !taken->Get()->pSQLResult)
		{
			sys_err("PLAYERBOT_SHOUTER: cannot look the name %s up", name);
			return 0;
		}
		DWORD pid = 0;
		if (MYSQL_ROW row = mysql_fetch_row(taken->Get()->pSQLResult))
		{
			DWORD owner = 0;
			unsigned int ownerEmpire = 0;
			if (row[0]) str_to_number(owner, row[0]);
			if (row[2]) str_to_number(ownerEmpire, row[2]);
			const bool bot = row[1] && strncmp(row[1], "playerbot_", 10) == 0;
			// Ours already - a row lost after the rename - is taken back; anybody
			// else's name is theirs.
			if (!bot || ownerEmpire != empire || !CPlayerBotManager::instance().IsRegisteredBotPID(owner) ||
					IsPlayerBotSidekickPID(owner))
			{
				sys_err("PLAYERBOT_SHOUTER: the name %s is taken by pid=%u login=%s empire=%u - "
						"kingdom %u gets no shouter (rename that character or change the name)",
						name, owner, row[1] ? row[1] : "", ownerEmpire, (unsigned int)empire);
				return 0;
			}
			pid = owner;
			sys_log(0, "PLAYERBOT_SHOUTER: empire=%u name=%s already worn by bot pid=%u, taken back",
					(unsigned int)empire, name, pid);
		}
		else
		{
			snprintf(query, sizeof(query),
					"SELECT l.pid FROM common.playerbot_seed_state AS l "
					"JOIN player.player AS p ON p.id=l.pid "
					"JOIN account.account AS a ON a.id=p.account_id "
					"JOIN player.player_index AS pi ON pi.id=a.id "
					"WHERE l.seed_version=1 AND l.state IN ('complete','adopted') "
					"AND BINARY a.login=BINARY CONCAT('playerbot_',LPAD(l.pid-3,GREATEST(3,LENGTH(l.pid-3)),'0')) "
					"AND pi.pid1=l.pid AND pi.pid2=0 AND pi.pid3=0 AND pi.pid4=0 "
					"AND pi.empire=%u AND p.level<=%u "
					"AND (p.playtime = 0 OR p.last_play < NOW() - INTERVAL %d MINUTE) "
					"ORDER BY (p.playtime > 0), l.pid DESC LIMIT 200",
					(unsigned int)empire, (unsigned int)PLAYERBOT_SHOUTER_LEVEL - 1, PLAYERBOT_SHOUTER_IDLE_MINUTES);
			std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
			if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			{
				sys_err("PLAYERBOT_SHOUTER: identity query failed errno=%u", msg.get() ? msg->uiSQLErrno : 0U);
				return 0;
			}
			MYSQL_ROW pick;
			while (NULL != (pick = mysql_fetch_row(msg->Get()->pSQLResult)))
			{
				DWORD candidate = 0;
				if (pick[0])
					str_to_number(candidate, pick[0]);
				CPlayerBotManager& mgr = CPlayerBotManager::instance();
				if (candidate == 0 || !mgr.IsRegistered(candidate) || mgr.GetRegisteredEmpire(candidate) != empire ||
						mgr.IsManaged(candidate) || mgr.IsScheduledBot(candidate) ||
						mgr.IsMedalDropperCohortPID(candidate) || IsPlayerBotSidekickPID(candidate) ||
						IsPlayerBotShouterPID(candidate) ||
						CHARACTER_MANAGER::instance().FindByPID(candidate) || P2P_MANAGER::instance().FindByPID(candidate))
					continue;
				pid = candidate;
				break;
			}
			if (pid == 0)
			{
				sys_err("PLAYERBOT_SHOUTER: no free identity in kingdom %u for %s", (unsigned int)empire, name);
				return 0;
			}
			// The seed's name stays in the name history as this identity's own,
			// so the name pool reads the new one as a person's choice and keeps it
			// at every later start (as the companions' rename does).
			snprintf(query, sizeof(query),
					"INSERT IGNORE INTO common.playerbot_name_history (pid, seed_name, human_name, pool_version, renamed_at) "
					"SELECT id, name, name, 'shouter', NOW() FROM player.player WHERE id=%u", pid);
			std::unique_ptr<SQLMsg> history(AccountDB::instance().DirectQuery(query));
			snprintf(query, sizeof(query), "UPDATE player.player SET name='%s' WHERE id=%u", name, pid);
			std::unique_ptr<SQLMsg> rename(AccountDB::instance().DirectQuery(query));
			if (!rename.get() || rename->uiSQLErrno != 0)
			{
				sys_err("PLAYERBOT_SHOUTER: cannot rename pid=%u to %s errno=%u", pid, name,
						rename.get() ? rename->uiSQLErrno : 0U);
				return 0;
			}
		}
		snprintf(query, sizeof(query),
				"INSERT INTO player.playerbot_shouter (empire, pid, name, created_at) VALUES (%u, %u, '%s', NOW()) "
				"ON DUPLICATE KEY UPDATE pid=VALUES(pid), name=VALUES(name)", (unsigned int)empire, pid, name);
		std::unique_ptr<SQLMsg> insert(AccountDB::instance().DirectQuery(query));
		if (!insert.get() || insert->uiSQLErrno != 0)
		{
			sys_err("PLAYERBOT_SHOUTER: cannot record the shouter of kingdom %u errno=%u", (unsigned int)empire,
					insert.get() ? insert->uiSQLErrno : 0U);
			return 0;
		}
		sys_log(0, "PLAYERBOT_SHOUTER: created empire=%u pid=%u name=%s", (unsigned int)empire, pid, name);
		return pid;
	}

	// The world's part, once a second on every core: the switch, and on the
	// core that keeps a kingdom's shouter its row and its presence.
	void ManagePlayerBotShouters(DWORD dwNow)
	{
		if (!s_bPlayerBotShoutersLoaded)
			LoadPlayerBotShouters(dwNow);
		const bool enabled = IsPlayerBotShoutersEnabled();
		if (s_iPlayerBotShoutersSwitchSeen != (enabled ? 1 : 0))
		{
			sys_log(0, "PLAYERBOT_SHOUTER: switch %s", enabled ? "on" : "off");
			s_iPlayerBotShoutersSwitchSeen = enabled ? 1 : 0;
		}
		CPlayerBotManager& mgr = CPlayerBotManager::instance();
		for (BYTE empire = 1; empire <= 3; ++empire)
		{
			TPlayerBotShouter& s = s_aPlayerBotShouters[empire];
			const long village = playerbot_empire_rules::GetHomeMap(empire, playerbot_empire_rules::MAP_ROLE_M1);
			s.bHere = g_bChannel == 1 && village != 0 && map_allow_find(village);
			if (!s.bHere)
				continue;
			if (!enabled)
			{
				if (s.pid != 0 && mgr.IsManaged(s.pid))
				{
					sys_log(0, "PLAYERBOT_SHOUTER: switched off, logging out pid=%u empire=%u", s.pid, (unsigned int)empire);
					mgr.Despawn(s.pid);
				}
				s.bArmed = false;
				s.bAtPost = false;
				continue;
			}
			if (s.pid == 0)
			{
				if (s.dwNextCreateTry != 0 && (int)(dwNow - s.dwNextCreateTry) < 0)
					continue;
				s.dwNextCreateTry = dwNow + PLAYERBOT_SHOUTER_CREATE_RETRY_MS;
				// Another core, or an earlier start, may have written it.
				LoadPlayerBotShouters(dwNow);
				if (s.pid == 0)
					s.pid = CreatePlayerBotShouter(empire);
				if (s.pid == 0)
					continue;
			}
			if (mgr.IsManaged(s.pid) || CHARACTER_MANAGER::instance().FindByPID(s.pid))
				continue;
			if (s.dwNextSpawnTry != 0 && (int)(dwNow - s.dwNextSpawnTry) < 0)
				continue;
			s.dwNextSpawnTry = dwNow + PLAYERBOT_SHOUTER_SPAWN_RETRY_MS;
			// Held at the door with the rest of the world (the panel's gate).
			if (IsPlayerBotSpawnHeld())
				continue;
			s.bArmed = false;
			s.bAtPost = false;
			s.bPost = false;
			const bool asked = mgr.Spawn(s.pid, empire);
			sys_log(0, "PLAYERBOT_SHOUTER: spawn pid=%u empire=%u name=%s %s", s.pid, (unsigned int)empire,
					PLAYERBOT_SHOUTER_NAMES[empire], asked ? "requested" : "refused");
		}
	}

	// Its line, when the channel has carried enough since the last one.
	void ManagePlayerBotShouterLine(LPCHARACTER ch, TPlayerBotShouter& s, DWORD dwNow)
	{
		const BYTE empire = ch->GetEmpire();
		if (empire < 1 || empire > 3 || PLAYERBOT_SHOUTER_LINE_COUNT <= 0)
			return;
		if (!s.bArmed)
		{
			// The first line after it takes its post: the count starts now,
			// and the pause is shortened so it does not wait the whole gap.
			s.bArmed = true;
			s.uSeenAtLine = s_auPlayerBotShoutsSeen[empire];
			s.iEvery = number(PLAYERBOT_SHOUTER_EVERY_MIN, PLAYERBOT_SHOUTER_EVERY_MAX);
			if (s.dwLastLine == 0)
				s.dwLastLine = dwNow - PLAYERBOT_SHOUTER_MIN_GAP_MS + (DWORD)number(60, 300) * 1000U;
			return;
		}
		if ((int)(s_auPlayerBotShoutsSeen[empire] - s.uSeenAtLine) < s.iEvery ||
				dwNow - s.dwLastLine < PLAYERBOT_SHOUTER_MIN_GAP_MS)
			return;
		int pick = number(0, PLAYERBOT_SHOUTER_LINE_COUNT - 1);
		if (PLAYERBOT_SHOUTER_LINE_COUNT > 1 && pick == s.iLastLine)
			pick = (pick + number(1, PLAYERBOT_SHOUTER_LINE_COUNT - 1)) % PLAYERBOT_SHOUTER_LINE_COUNT;
		char text[CHAT_MAX_LEN + 1];
		ConvertPlayerBotShouterText(PLAYERBOT_SHOUTER_LINES[pick], text, sizeof(text));
		char msg[CHAT_MAX_LEN + 1];
		snprintf(msg, sizeof(msg), "%s : %s", ch->GetName(), text);
		const unsigned int seen = s_auPlayerBotShoutsSeen[empire] - s.uSeenAtLine;
		SendPlayerBotShout(msg, empire);
		++s.uLines;
		sys_log(0, "PLAYERBOT_SHOUTER: shout pid=%u name=%s empire=%u line=%d after=%u/%d gap=%us total=%u text=\"%s\"",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)empire, pick, seen, s.iEvery,
				(unsigned int)((dwNow - s.dwLastLine) / 1000U), s.uLines, PLAYERBOT_SHOUTER_LINES[pick]);
		s.iLastLine = pick;
		s.dwLastLine = dwNow;
		s.uSeenAtLine = s_auPlayerBotShoutsSeen[empire];
		s.iEvery = number(PLAYERBOT_SHOUTER_EVERY_MIN, PLAYERBOT_SHOUTER_EVERY_MAX);
	}

	// Weapon and armour off (and their costumes), into the bag.
	bool IsPlayerBotShouterUnarmed(LPCHARACTER ch)
	{
		static const int wears[] = {
			WEAR_WEAPON, WEAR_BODY,
#if defined(PLAYERBOT_ENGINE_MT2009)
			WEAR_COSTUME_BODY, WEAR_COSTUME_WEAPON,
#endif
		};
		bool bare = true;
		for (size_t i = 0; i < sizeof(wears) / sizeof(wears[0]); ++i)
		{
			LPITEM worn = ch->GetWear(wears[i]);
			if (!worn)
				continue;
			if (ch->GetEmptyInventory(worn->GetSize()) < 0 || !ch->UnequipItem(worn) || worn->IsEquipped())
			{
				PlayerBotLogThrottled("shouter_unequip", get_dword_time(),
						"PLAYERBOT_SHOUTER: cannot take off vnum=%u pid=%u name=%s (bag full?)",
						worn->GetVnum(), ch->GetPlayerID(), ch->GetName());
				bare = false;
				continue;
			}
			sys_log(0, "PLAYERBOT_SHOUTER: took off vnum=%u pid=%u name=%s", worn->GetVnum(),
					ch->GetPlayerID(), ch->GetName());
		}
		return bare;
	}

	// The shouter's tick from its level on: true claims it. Under the level
	// it is any bot's.
	bool ManagePlayerBotShouterTick(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead())
			return false;
		TPlayerBotShouter* s = GetPlayerBotShouter(ch->GetPlayerID());
		if (!s || ch->GetLevel() < PLAYERBOT_SHOUTER_LEVEL)
			return false;
		ManagePlayerBotShouterExpLock(ch);
		state.dwLastMeaningfulActivityTime = dwNow;
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		if (ch->GetExchange())
			ch->GetExchange()->Cancel();
		if (ch->GetParty())
			LeavePlayerBotParty(ch);
		StopPlayerBotRiding(ch);

		const long village = playerbot_empire_rules::GetHomeMap(ch->GetEmpire(), playerbot_empire_rules::MAP_ROLE_M1);
		playerbot_empire_rules::TTownServices svc;
		if (village == 0 || !playerbot_empire_rules::GetTownServices(village, svc))
			return true;
		if (!s->bPost)
		{
			long dx = 0, dy = 0;
			GetPlayerBotStableOffset(ch->GetPlayerID(), 0x53484f55U, PLAYERBOT_SHOUTER_POST_MIN_RADIUS,
					PLAYERBOT_SHOUTER_POST_MAX_RADIUS, dx, dy);
			s->lPostX = svc.miscMerchant.x + dx;
			s->lPostY = svc.miscMerchant.y + dy;
			CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(village);
			PIXEL_POSITION safe;
			if (navigation.Init(village) &&
					navigation.FindNearestWalkableWorld(s->lPostX, s->lPostY, 8, safe, ch->GetPlayerID()))
			{
				s->lPostX = safe.x;
				s->lPostY = safe.y;
			}
			s->bPost = true;
		}

		const bool here = ch->GetMapIndex() == village;
		const int distance = here ? DISTANCE_APPROX(ch->GetX() - s->lPostX, ch->GetY() - s->lPostY) : INT_MAX;
		if (distance > PLAYERBOT_SHOUTER_ARRIVED)
		{
			s->bAtPost = false;
			if (s->dwWalkSince == 0)
				s->dwWalkSince = dwNow;
			const bool jump = !here || distance > PLAYERBOT_SHOUTER_WALK_MAX ||
					dwNow - s->dwWalkSince > PLAYERBOT_SHOUTER_WALK_MAX_MS;
			if (jump)
			{
				if (s->dwNextJump == 0 || (int)(dwNow - s->dwNextJump) >= 0)
				{
					s->dwNextJump = dwNow + 10000;
					ClearPlayerBotRoute(state, true);
					const bool moved = TransitionPlayerBotMap(ch, state, village, s->lPostX, s->lPostY, dwNow, "shouter_post");
					sys_log(0, "PLAYERBOT_SHOUTER: to the post pid=%u name=%s from map=%ld (%ld,%ld) to (%ld,%ld) %s",
							ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), ch->GetX(), ch->GetY(),
							s->lPostX, s->lPostY, moved ? "ok" : "refused");
					s->dwWalkSince = dwNow;
				}
				return true;
			}
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			MovePlayerBot(ch, s->lPostX, s->lPostY, dwNow);
			return true;
		}
		// At its post.
		s->dwWalkSince = 0;
		if (ch->IsStateMove())
			ch->Stop();
		ClearPlayerBotRoute(state, true);
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		if (s->dwNextGearTry == 0 || (int)(dwNow - s->dwNextGearTry) >= 0)
		{
			s->dwNextGearTry = dwNow + 10000;
			if (!IsPlayerBotShouterUnarmed(ch))
				return true;
		}
		if (!s->bAtPost)
		{
			s->bAtPost = true;
			sys_log(0, "PLAYERBOT_SHOUTER: at the post pid=%u name=%s level=%u map=%ld (%ld,%ld) weapon=%u body=%u",
					ch->GetPlayerID(), ch->GetName(), (unsigned)ch->GetLevel(), ch->GetMapIndex(),
					ch->GetX(), ch->GetY(), ch->GetWear(WEAR_WEAPON) ? ch->GetWear(WEAR_WEAPON)->GetVnum() : 0U,
					ch->GetWear(WEAR_BODY) ? ch->GetWear(WEAR_BODY)->GetVnum() : 0U);
		}
		if (!ch->GetWear(WEAR_WEAPON) && !ch->GetWear(WEAR_BODY))
			ManagePlayerBotShouterLine(ch, *s, dwNow);
		return true;
	}
}

#endif
