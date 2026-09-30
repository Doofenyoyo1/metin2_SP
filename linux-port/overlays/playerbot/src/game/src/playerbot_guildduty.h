#ifndef __INC_METIN2_PLAYERBOT_GUILDDUTY_H__
#define __INC_METIN2_PLAYERBOT_GUILDDUTY_H__

// MT2009_PLUS_GUILD_DUTY_V1 - the guild leader's duties (the operator, 28
// September: "Od teraz jesli jestes liderem gildii, mozesz zlecac gildyjne
// obowiazki ... niech to bedzie osobny panel lidera w gildii").
//
// A player who leads a guild gives its bots three kinds of duty, from the
// leader's panel (client uiguildduty.py, "/gildia_obowiazki"):
//
//  1. Zrzutka yang - a collection for the guild's treasury (guild.gold, the
//     one the guild window shows and the leader withdraws from, for the land
//     or the buildings). The leader names the sum and the time (12 hours by
//     default); the guild's bots pay it in over that time, a bot at a time,
//     the way the bot guilds' own collection takes it (playerbot_guild_land.h:
//     every bot keeps its reserve, gives a share of what is above it) - never
//     ahead of the clock, so the treasury grows through the day instead of
//     filling at once. Every payment is the engine's own guild deposit
//     (HEADER_GD_GUILD_DEPOSIT_MONEY): the db core adds it to the guild's gold
//     and tells every core.
//  2. Misja na przedmioty - "go and gather N logs" (or cornerstones, or
//     plywood - the three building materials of object_proto). The bots of
//     the guild of the level for the material's ground are sent there
//     (playerbot_guild_land.h's PLAYERBOT_GUILD_MATERIAL_GROUNDS, through the
//     guild errand's hook in playerbot_travel.h), hunt, and hand what drops
//     into the guild's item bank - which remembers which bot gave what. The
//     leader takes it out of the bank in the panel. A cancelled mission gives
//     every piece still in the bank back to the bot that brought it.
//  3. Wyprawa na Wieze Demonow - the leader calls it; the guild's bots of
//     the tower's level, with bots of the kingdom hired to fill the party,
//     go on the tower's raid (playerbot_demon_tower.h's raid, called for the
//     player's guild). One raid at a time: an expedition waits while another
//     guild is in the tower. The raid is the first channel's, on the core
//     that hosts the tower.
//
// One collection and one item mission at a time for a guild (the tables'
// active_guild: a UNIQUE column that holds the guild id while the duty is
// running and NULL after, so two cores cannot start two).
//
// Multi-core: a guild's bots live on several cores, and a player moves
// between them. Everything is in the database (player.guild_duty_*), every
// core works on its own bots only, and every change is atomic: a payment is
// a conditional "collected = collected + x" that cannot pass the schedule or
// the target, a delivery the same against the mission's target, a worker
// takes a place with "workers = workers + 1 WHERE workers < workers_max", a
// withdrawal is "count = count - n WHERE count >= n" before the item is
// given, a return "count = 0 WHERE count = n AND return_pending = 1" before
// it goes back to the bot.

namespace
{
	// ---------------------------------------------------------------- rules

	const DWORD GUILD_DUTY_PASS_MS = 20 * 1000;
	// The collection: from 100 000 yang to 2 000 000 000, over 1 to 72 hours.
	const long long GUILD_DUTY_COLLECT_MIN = 100000LL;
	const long long GUILD_DUTY_COLLECT_MAX = 2000000000LL;
	const int GUILD_DUTY_COLLECT_HOURS_DEFAULT = 12;
	const int GUILD_DUTY_COLLECT_HOURS_MAX = 72;
	// A collection behind its clock at the end may catch up for this long.
	const DWORD GUILD_DUTY_COLLECT_GRACE_S = 2 * 60 * 60;
	// A bot pays at most this share of what it has above its reserve at once.
	const int GUILD_DUTY_DONOR_SHARE_PERCENT = 12;
	// A payment waits until the schedule is this part of the sum behind.
	const int GUILD_DUTY_COLLECT_CHUNKS = 150;
	// The item mission: 1 to 500 pieces, 2 to 8 bots.
	const int GUILD_DUTY_MISSION_MAX = 500;
	const int GUILD_DUTY_WORKERS_MIN = 2;
	const int GUILD_DUTY_WORKERS_MAX = 8;
	// A worker nobody has seen on any core for this long gives its place up.
	const DWORD GUILD_DUTY_WORKER_STALE_S = 30 * 60;
	// The tower: the party the leader asks for, and how long an expedition
	// that cannot gather its bots waits before it is given up.
	const int GUILD_DUTY_TOWER_MIN = 3;
	const int GUILD_DUTY_TOWER_DEFAULT = 8;
	const DWORD GUILD_DUTY_TOWER_WAIT_S = 45 * 60;
	// A bot's line in the guild chat, at most once in this long a guild.
	const DWORD GUILD_DUTY_CHAT_MS = 3 * 60 * 1000;

	enum EGuildDutyState
	{
		GUILD_DUTY_ACTIVE = 1,
		GUILD_DUTY_DONE = 2,
		GUILD_DUTY_CANCELLED = 3,
		GUILD_DUTY_EXPIRED = 4,
	};

	// The tower expedition's states.
	enum EGuildDutyTowerState
	{
		GUILD_TOWER_WAITING = 1,
		GUILD_TOWER_GATHER = 2,
		GUILD_TOWER_STONE = 3,
		GUILD_TOWER_INSIDE = 4,
		GUILD_TOWER_DONE = 5,
		GUILD_TOWER_CANCELLED = 6,
		GUILD_TOWER_FAILED = 7,
	};

	bool s_bGuildDutyTables = false;
	DWORD s_dwGuildDutyNextPass = 0;
	// Worker pid -> the mission, its material and the map it hunts on (this
	// core's bots; rebuilt every pass from the database).
	struct TGuildDutyWorker { DWORD mission; DWORD vnum; long map; };
	std::map<DWORD, TGuildDutyWorker> s_mapGuildDutyWorkers;
	std::map<DWORD, DWORD> s_mapGuildDutyChatAt;
	// The tower expedition this core called the raid for, and the best floor.
	DWORD s_dwGuildDutyTowerId = 0;
	DWORD s_dwGuildDutyTowerGuild = 0;
	int s_iGuildDutyTowerFloor = 0;

	// ------------------------------------------------------------ database

	std::unique_ptr<SQLMsg> GuildDutyQuery(const char* fmt, ...)
	{
		char query[2048];
		va_list args;
		va_start(args, fmt);
		vsnprintf(query, sizeof(query), fmt, args);
		va_end(args);
		return DBManager::instance().DirectQuery("%s", query);
	}

	bool GuildDutyOk(const std::unique_ptr<SQLMsg>& msg)
	{
		return msg.get() && msg->uiSQLErrno == 0 && msg->Get();
	}

	bool GuildDutyAffected(const std::unique_ptr<SQLMsg>& msg)
	{
		return GuildDutyOk(msg) && msg->Get()->uiAffectedRows == 1;
	}

	bool EnsureGuildDutyTables()
	{
		if (s_bGuildDutyTables)
			return true;
		std::unique_ptr<SQLMsg> a(GuildDutyQuery(
				"CREATE TABLE IF NOT EXISTS player.guild_duty_collect ("
				"id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, "
				"guild_id INT UNSIGNED NOT NULL, "
				"leader_pid INT UNSIGNED NOT NULL DEFAULT 0, "
				"target BIGINT NOT NULL, "
				"collected BIGINT NOT NULL DEFAULT 0, "
				"start_ts INT UNSIGNED NOT NULL, "
				"end_ts INT UNSIGNED NOT NULL, "
				"state TINYINT UNSIGNED NOT NULL DEFAULT 1, "
				"active_guild INT UNSIGNED NULL DEFAULT NULL, "
				"finished_ts INT UNSIGNED NOT NULL DEFAULT 0, "
				"UNIQUE KEY uq_active (active_guild), KEY k_guild (guild_id)) ENGINE=InnoDB"));
		std::unique_ptr<SQLMsg> b(GuildDutyQuery(
				"CREATE TABLE IF NOT EXISTS player.guild_duty_donation ("
				"collect_id INT UNSIGNED NOT NULL, "
				"pid INT UNSIGNED NOT NULL, "
				"amount BIGINT NOT NULL DEFAULT 0, "
				"times INT UNSIGNED NOT NULL DEFAULT 0, "
				"last_ts INT UNSIGNED NOT NULL DEFAULT 0, "
				"PRIMARY KEY (collect_id, pid)) ENGINE=InnoDB"));
		std::unique_ptr<SQLMsg> c(GuildDutyQuery(
				"CREATE TABLE IF NOT EXISTS player.guild_duty_mission ("
				"id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, "
				"guild_id INT UNSIGNED NOT NULL, "
				"leader_pid INT UNSIGNED NOT NULL DEFAULT 0, "
				"vnum INT UNSIGNED NOT NULL, "
				"target INT UNSIGNED NOT NULL, "
				"collected INT UNSIGNED NOT NULL DEFAULT 0, "
				"workers INT UNSIGNED NOT NULL DEFAULT 0, "
				"workers_max INT UNSIGNED NOT NULL DEFAULT 2, "
				"state TINYINT UNSIGNED NOT NULL DEFAULT 1, "
				"active_guild INT UNSIGNED NULL DEFAULT NULL, "
				"created_ts INT UNSIGNED NOT NULL DEFAULT 0, "
				"finished_ts INT UNSIGNED NOT NULL DEFAULT 0, "
				"UNIQUE KEY uq_active (active_guild), KEY k_guild (guild_id)) ENGINE=InnoDB"));
		std::unique_ptr<SQLMsg> d(GuildDutyQuery(
				"CREATE TABLE IF NOT EXISTS player.guild_duty_worker ("
				"mission_id INT UNSIGNED NOT NULL, "
				"pid INT UNSIGNED NOT NULL, "
				"delivered INT UNSIGNED NOT NULL DEFAULT 0, "
				"joined_ts INT UNSIGNED NOT NULL DEFAULT 0, "
				"last_seen INT UNSIGNED NOT NULL DEFAULT 0, "
				"PRIMARY KEY (mission_id, pid), KEY k_pid (pid)) ENGINE=InnoDB"));
		std::unique_ptr<SQLMsg> e(GuildDutyQuery(
				"CREATE TABLE IF NOT EXISTS player.guild_duty_bank ("
				"mission_id INT UNSIGNED NOT NULL, "
				"guild_id INT UNSIGNED NOT NULL, "
				"bot_pid INT UNSIGNED NOT NULL, "
				"vnum INT UNSIGNED NOT NULL, "
				"count INT UNSIGNED NOT NULL DEFAULT 0, "
				"delivered INT UNSIGNED NOT NULL DEFAULT 0, "
				"withdrawn INT UNSIGNED NOT NULL DEFAULT 0, "
				"returned INT UNSIGNED NOT NULL DEFAULT 0, "
				"return_pending TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"last_ts INT UNSIGNED NOT NULL DEFAULT 0, "
				"PRIMARY KEY (mission_id, bot_pid, vnum), KEY k_guild (guild_id, vnum), "
				"KEY k_return (return_pending, bot_pid)) ENGINE=InnoDB"));
		std::unique_ptr<SQLMsg> f(GuildDutyQuery(
				"CREATE TABLE IF NOT EXISTS player.guild_duty_tower ("
				"id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, "
				"guild_id INT UNSIGNED NOT NULL, "
				"leader_pid INT UNSIGNED NOT NULL DEFAULT 0, "
				"empire TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"wanted INT UNSIGNED NOT NULL DEFAULT 8, "
				"members INT UNSIGNED NOT NULL DEFAULT 0, "
				"recruits INT UNSIGNED NOT NULL DEFAULT 0, "
				"floor INT UNSIGNED NOT NULL DEFAULT 0, "
				"state TINYINT UNSIGNED NOT NULL DEFAULT 1, "
				"active_guild INT UNSIGNED NULL DEFAULT NULL, "
				"note VARCHAR(96) NOT NULL DEFAULT '', "
				"created_ts INT UNSIGNED NOT NULL DEFAULT 0, "
				"updated_ts INT UNSIGNED NOT NULL DEFAULT 0, "
				"finished_ts INT UNSIGNED NOT NULL DEFAULT 0, "
				"UNIQUE KEY uq_active (active_guild), KEY k_guild (guild_id)) ENGINE=InnoDB"));
		s_bGuildDutyTables = GuildDutyOk(a) && GuildDutyOk(b) && GuildDutyOk(c) && GuildDutyOk(d) &&
				GuildDutyOk(e) && GuildDutyOk(f);
		if (!s_bGuildDutyTables)
			sys_err("GUILD_DUTY: tables not ready");
		return s_bGuildDutyTables;
	}

	// ------------------------------------------------------------- helpers

	std::string GuildDutyHex(const char* text)
	{
		static const char* const digits = "0123456789abcdef";
		std::string out;
		for (const unsigned char* p = (const unsigned char*)(text ? text : ""); *p; ++p)
		{
			out += digits[*p >> 4];
			out += digits[*p & 15];
		}
		return out.empty() ? std::string("-") : out;
	}

	std::string GuildDutyMoney(long long n)
	{
		char raw[32];
		snprintf(raw, sizeof(raw), "%lld", n < 0 ? -n : n);
		std::string digits(raw), out;
		for (size_t i = 0; i < digits.size(); ++i)
		{
			if (i > 0 && (digits.size() - i) % 3 == 0)
				out += '.';
			out += digits[i];
		}
		return n < 0 ? "-" + out : out;
	}

	const char* GuildDutyItemName(DWORD vnum)
	{
		const TItemTable* t = ITEM_MANAGER::instance().GetTable(vnum);
		return t ? t->szLocaleName : "?";
	}

	const char* GuildDutyMapName(long map)
	{
		switch (map)
		{
			case PLAYERBOT_MAP_SOHAN: return "Gora Sohan";
			case PLAYERBOT_MAP_FIRE_LAND: return "Ognista Ziemia";
			case PLAYERBOT_MAP_ORC_VALLEY: return "Dolina Orkow";
			case PLAYERBOT_MAP_HWANG: return "Swiatynia Hwang";
		}
		return "teren";
	}

	bool IsGuildDutyMaterial(DWORD vnum)
	{
		return vnum == 90010 || vnum == 90011 || vnum == 90012;
	}

	// The lowest level any ground of the material asks (the panel's hint and
	// the choice of the bots), whether or not this core hosts it.
	int GetGuildDutyMaterialMinLevel(DWORD vnum)
	{
		int best = 0;
		for (size_t g = 0; g < sizeof(PLAYERBOT_GUILD_MATERIAL_GROUNDS) / sizeof(PLAYERBOT_GUILD_MATERIAL_GROUNDS[0]); ++g)
			if (PLAYERBOT_GUILD_MATERIAL_GROUNDS[g].vnum == vnum &&
					(best == 0 || PLAYERBOT_GUILD_MATERIAL_GROUNDS[g].minLevel < best))
				best = PLAYERBOT_GUILD_MATERIAL_GROUNDS[g].minLevel;
		return best;
	}

	// Where this bot hunts the material: the ground of the highest level it
	// has reached, on a map of this core. Zero when it has none.
	long GetGuildDutyGroundFor(LPCHARACTER ch, DWORD vnum)
	{
		long best = 0;
		int bestLevel = -1;
		for (size_t g = 0; g < sizeof(PLAYERBOT_GUILD_MATERIAL_GROUNDS) / sizeof(PLAYERBOT_GUILD_MATERIAL_GROUNDS[0]); ++g)
		{
			const TPlayerBotGuildMaterialGround& ground = PLAYERBOT_GUILD_MATERIAL_GROUNDS[g];
			if (ground.vnum != vnum || (int)ch->GetLevel() < ground.minLevel ||
					!IsPlayerBotFrontierMapIndex(ground.map) || !IsPlayerBotMapHostedHere(ground.map))
				continue;
			if (ground.minLevel > bestLevel)
			{
				best = ground.map;
				bestLevel = ground.minLevel;
			}
		}
		return best;
	}

	bool IsGuildDutyPlayerGuild(CGuild* guild)
	{
		return guild && !CPlayerBotManager::instance().IsRegisteredBotPID(guild->GetMasterPID());
	}

	// A bot's line in the guild chat, the way a player's reads.
	void GuildDutyBotSays(LPCHARACTER bot, const char* text, bool always = false)
	{
		CGuild* guild = bot ? bot->GetGuild() : NULL;
		if (!guild || !text)
			return;
		const DWORD now = get_dword_time();
		DWORD& at = s_mapGuildDutyChatAt[guild->GetID()];
		if (!always && at != 0 && now - at < GUILD_DUTY_CHAT_MS)
			return;
		at = now;
		char line[CHAT_MAX_LEN + 1];
		snprintf(line, sizeof(line), "%s : %s", bot->GetName(), text);
		guild->Chat(line);
	}

	void GuildDutyTell(CGuild* guild, DWORD pid, const char* text)
	{
		LPCHARACTER leader = pid ? CHARACTER_MANAGER::instance().FindByPID(pid) : NULL;
		if (leader && leader->GetDesc())
		{
			leader->ChatPacket(CHAT_TYPE_INFO, "[Gildia] %s", text);
			leader->ChatPacket(CHAT_TYPE_COMMAND, "GDUpdate");
		}
		else if (guild)
			guild->Chat(text);
	}

	// A bot that may take a duty: a guild member of this core's world, alive,
	// no player's companion, not busy with a raid, a war or a player's party.
	bool IsGuildDutyFreeBot(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		if (!ch || ch->IsDead() || IsPlayerBotSidekickPID(ch->GetPlayerID()) ||
				IsPlayerBotOnTowerBusiness(ch, state) || state.dwGuildWarEnemyGID != 0)
			return false;
		if (ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()))
			return false;
		return true;
	}

	// This core's bots, by guild (players' guilds only).
	void CollectGuildDutyBots(std::map<DWORD, std::vector<LPCHARACTER> >& out)
	{
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch || !ch->GetDesc() || !ch->GetSectree())
				continue;
			CGuild* guild = ch->GetGuild();
			if (!IsGuildDutyPlayerGuild(guild))
				continue;
			out[guild->GetID()].push_back(ch);
		}
	}

	// ------------------------------------------------------------ the yang

	long long GetGuildDutyDonorCap(LPCHARACTER ch)
	{
		// The reserve counts the bot's reserved gold, a guild fund in its purse too.
		const long long spare = (long long)ch->GetGold() - GetPlayerBotGuildDonorReserve(ch);
		return spare > 0 ? spare * GUILD_DUTY_DONOR_SHARE_PERCENT / 100 : 0;
	}

	// Into the guild's treasury, the engine's way (CGuild::RequestDepositMoney
	// without the player's pulse): the db core adds it and tells every core.
	void GuildDutyDeposit(CGuild* guild, LPCHARACTER bot, long long amount)
	{
		PlayerBotChangeGold(bot, -amount);
		TPacketGDGuildMoney p;
		p.dwGuild = guild->GetID();
		p.iGold = amount;
		db_clientdesc->DBPacket(HEADER_GD_GUILD_DEPOSIT_MONEY, 0, &p, sizeof(p));
		char buf[64 + 1];
		snprintf(buf, sizeof(buf), "%u %s", guild->GetID(), guild->GetName());
		LogManager::instance().CharLog(bot, amount, "GUILD_DUTY_DEPOSIT", buf);
	}

	void ManageGuildDutyCollections(std::map<DWORD, std::vector<LPCHARACTER> >& bots, time_t now)
	{
		std::unique_ptr<SQLMsg> msg(GuildDutyQuery(
				"SELECT id, guild_id, leader_pid, target, collected, start_ts, end_ts FROM player.guild_duty_collect "
				"WHERE state = %d", GUILD_DUTY_ACTIVE));
		if (!GuildDutyOk(msg) || !msg->Get()->pSQLResult)
			return;
		struct TRow { DWORD id, gid, leader; long long target, collected; DWORD start, end; };
		std::vector<TRow> rows;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			TRow r = { 0, 0, 0, 0, 0, 0, 0 };
			str_to_number(r.id, row[0]);
			str_to_number(r.gid, row[1]);
			str_to_number(r.leader, row[2]);
			str_to_number(r.target, row[3]);
			str_to_number(r.collected, row[4]);
			str_to_number(r.start, row[5]);
			str_to_number(r.end, row[6]);
			rows.push_back(r);
		}
		for (size_t i = 0; i < rows.size(); ++i)
		{
			const TRow& r = rows[i];
			CGuild* guild = CGuildManager::instance().FindGuild(r.gid);
			if (r.collected >= r.target || (DWORD)now >= r.end + GUILD_DUTY_COLLECT_GRACE_S || !guild)
			{
				const bool full = r.collected >= r.target;
				std::unique_ptr<SQLMsg> done(GuildDutyQuery(
						"UPDATE player.guild_duty_collect SET state = %d, active_guild = NULL, finished_ts = %u "
						"WHERE id = %u AND state = %d", full ? GUILD_DUTY_DONE : GUILD_DUTY_EXPIRED, (DWORD)now,
						r.id, GUILD_DUTY_ACTIVE));
				if (GuildDutyAffected(done) && guild)
				{
					char text[160];
					snprintf(text, sizeof(text), full ? "Zrzutka gildii zakonczona: zebrano %s yang." :
							"Czas zrzutki minal: zebrano %s yang.", GuildDutyMoney(r.collected).c_str());
					GuildDutyTell(guild, r.leader, text);
					sys_log(0, "GUILD_DUTY: collection %u guild=%u over full=%d collected=%lld target=%lld",
							r.id, r.gid, full ? 1 : 0, r.collected, r.target);
				}
				continue;
			}
			std::map<DWORD, std::vector<LPCHARACTER> >::iterator local = bots.find(r.gid);
			if (local == bots.end() || local->second.empty())
				continue;
			// A pass now and then is left out, so the payments do not come
			// on a clock.
			if (number(1, 100) <= 25)
				continue;
			const long long span = std::max<long long>(1, (long long)r.end - (long long)r.start);
			const long long elapsed = std::max<long long>(0, (long long)now - (long long)r.start);
			const long long expected = elapsed >= span ? r.target :
					(long long)((long double)r.target * (long double)elapsed / (long double)span);
			const long long deficit = expected - r.collected;
			const long long chunk = std::max<long long>(1000, r.target / GUILD_DUTY_COLLECT_CHUNKS);
			if (deficit <= 0 || (deficit < chunk && r.target - r.collected > chunk && elapsed < span))
				continue;
			std::vector<LPCHARACTER> donors = local->second;
			for (size_t k = donors.size(); k > 1; --k)
				std::swap(donors[k - 1], donors[number(0, (int)k - 1)]);
			for (size_t d = 0; d < donors.size(); ++d)
			{
				LPCHARACTER bot = donors[d];
				TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(bot->GetPlayerID());
				if (st == s_mapPlayerBotAIStates.end() || IsPlayerBotSidekickPID(bot->GetPlayerID()) || bot->IsDead())
					continue;
				const long long cap = GetGuildDutyDonorCap(bot);
				long long amount = std::min(deficit, cap);
				if (amount >= 10000)
					amount -= amount % 1000;
				if (amount < 1000 && amount < r.target - r.collected)
					continue;
				// Never ahead of the clock, never past the sum - whichever
				// core pays first.
				std::unique_ptr<SQLMsg> paid(GuildDutyQuery(
						"UPDATE player.guild_duty_collect SET collected = collected + %lld "
						"WHERE id = %u AND state = %d AND collected + %lld <= target AND collected + %lld <= %lld",
						amount, r.id, GUILD_DUTY_ACTIVE, amount, amount, expected));
				if (!GuildDutyAffected(paid))
					break;
				GuildDutyDeposit(guild, bot, amount);
				std::unique_ptr<SQLMsg> logged(GuildDutyQuery(
						"INSERT INTO player.guild_duty_donation (collect_id, pid, amount, times, last_ts) "
						"VALUES (%u, %u, %lld, 1, %u) ON DUPLICATE KEY UPDATE amount = amount + %lld, times = times + 1, "
						"last_ts = %u", r.id, bot->GetPlayerID(), amount, (DWORD)now, amount, (DWORD)now));
				sys_log(0, "GUILD_DUTY: collection %u guild=%s pid=%u name=%s paid=%lld collected=%lld/%lld",
						r.id, guild->GetName(), bot->GetPlayerID(), bot->GetName(), amount, r.collected + amount, r.target);
				if (number(1, 100) <= 40)
				{
					static const char* const lines[] = {
						"Wplacilem %s yang na zrzutke gildii.",
						"Dorzucam %s yang do skarbca gildii.",
						"%s yang ode mnie na zrzutke.",
						"Mam troche grosza, %s yang idzie do gildii.",
					};
					char text[128];
					snprintf(text, sizeof(text), lines[number(0, 3)], GuildDutyMoney(amount).c_str());
					GuildDutyBotSays(bot, text);
				}
				break;
			}
		}
	}

	// ----------------------------------------------------------- the items

	struct TGuildDutyMissionRow
	{
		DWORD id, gid, leader, vnum, target, collected, workers, workersMax;
	};

	void LoadGuildDutyMissions(std::vector<TGuildDutyMissionRow>& out)
	{
		std::unique_ptr<SQLMsg> msg(GuildDutyQuery(
				"SELECT id, guild_id, leader_pid, vnum, target, collected, workers, workers_max "
				"FROM player.guild_duty_mission WHERE state = %d", GUILD_DUTY_ACTIVE));
		if (!GuildDutyOk(msg) || !msg->Get()->pSQLResult)
			return;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			TGuildDutyMissionRow r = { 0, 0, 0, 0, 0, 0, 0, 0 };
			str_to_number(r.id, row[0]);
			str_to_number(r.gid, row[1]);
			str_to_number(r.leader, row[2]);
			str_to_number(r.vnum, row[3]);
			str_to_number(r.target, row[4]);
			str_to_number(r.collected, row[5]);
			str_to_number(r.workers, row[6]);
			str_to_number(r.workersMax, row[7]);
			out.push_back(r);
		}
	}

	// A bot's pieces into the bank, as many as the mission still wants.
	int DeliverGuildDutyItems(LPCHARACTER bot, CGuild* guild, TGuildDutyMissionRow& m, time_t now)
	{
		const int have = (int)bot->CountSpecifyItem(m.vnum);
		if (have <= 0 || m.collected >= m.target)
			return 0;
		int give = std::min(have, (int)(m.target - m.collected));
		std::unique_ptr<SQLMsg> took(GuildDutyQuery(
				"UPDATE player.guild_duty_mission SET collected = collected + %d "
				"WHERE id = %u AND state = %d AND collected + %d <= target",
				give, m.id, GUILD_DUTY_ACTIVE, give));
		if (!GuildDutyAffected(took))
		{
			// Another core delivered meanwhile: what is left, read again.
			std::unique_ptr<SQLMsg> fresh(GuildDutyQuery(
					"SELECT collected, state FROM player.guild_duty_mission WHERE id = %u", m.id));
			MYSQL_ROW row = GuildDutyOk(fresh) && fresh->Get()->pSQLResult ? mysql_fetch_row(fresh->Get()->pSQLResult) : NULL;
			int state = 0;
			if (!row)
				return 0;
			str_to_number(m.collected, row[0]);
			str_to_number(state, row[1]);
			if (state != GUILD_DUTY_ACTIVE || m.collected >= m.target)
				return 0;
			give = std::min(have, (int)(m.target - m.collected));
			std::unique_ptr<SQLMsg> again(GuildDutyQuery(
					"UPDATE player.guild_duty_mission SET collected = collected + %d "
					"WHERE id = %u AND state = %d AND collected + %d <= target",
					give, m.id, GUILD_DUTY_ACTIVE, give));
			if (!GuildDutyAffected(again))
				return 0;
		}
		bot->RemoveSpecifyItem(m.vnum, give);
		m.collected += give;
		std::unique_ptr<SQLMsg> bank(GuildDutyQuery(
				"INSERT INTO player.guild_duty_bank (mission_id, guild_id, bot_pid, vnum, count, delivered, last_ts) "
				"VALUES (%u, %u, %u, %u, %d, %d, %u) ON DUPLICATE KEY UPDATE count = count + %d, "
				"delivered = delivered + %d, last_ts = %u",
				m.id, m.gid, bot->GetPlayerID(), m.vnum, give, give, (DWORD)now, give, give, (DWORD)now));
		std::unique_ptr<SQLMsg> worker(GuildDutyQuery(
				"UPDATE player.guild_duty_worker SET delivered = delivered + %d WHERE mission_id = %u AND pid = %u",
				give, m.id, bot->GetPlayerID()));
		sys_log(0, "GUILD_DUTY: mission %u guild=%s pid=%u name=%s delivered vnum=%u count=%d now=%u/%u",
				m.id, guild->GetName(), bot->GetPlayerID(), bot->GetName(), m.vnum, give, m.collected, m.target);
		char text[160];
		snprintf(text, sizeof(text), "Oddalem %d x %s do banku gildii (%u/%u).", give, GuildDutyItemName(m.vnum),
				m.collected, m.target);
		GuildDutyBotSays(bot, text, m.collected >= m.target);
		return give;
	}

	void FinishGuildDutyMission(const TGuildDutyMissionRow& m, CGuild* guild, int state, time_t now)
	{
		std::unique_ptr<SQLMsg> done(GuildDutyQuery(
				"UPDATE player.guild_duty_mission SET state = %d, active_guild = NULL, finished_ts = %u "
				"WHERE id = %u AND state = %d", state, (DWORD)now, m.id, GUILD_DUTY_ACTIVE));
		if (!GuildDutyAffected(done))
			return;
		std::unique_ptr<SQLMsg> freed(GuildDutyQuery("DELETE FROM player.guild_duty_worker WHERE mission_id = %u", m.id));
		if (guild)
		{
			char text[160];
			snprintf(text, sizeof(text), "Misja gildii zakonczona: %u x %s czeka w banku gildii.", m.collected,
					GuildDutyItemName(m.vnum));
			GuildDutyTell(guild, m.leader, text);
		}
		sys_log(0, "GUILD_DUTY: mission %u guild=%u done vnum=%u count=%u", m.id, m.gid, m.vnum, m.collected);
	}

	void ManageGuildDutyMissions(std::map<DWORD, std::vector<LPCHARACTER> >& bots, time_t now)
	{
		std::vector<TGuildDutyMissionRow> missions;
		LoadGuildDutyMissions(missions);
		std::map<DWORD, TGuildDutyWorker> workers;
		for (size_t i = 0; i < missions.size(); ++i)
		{
			TGuildDutyMissionRow& m = missions[i];
			CGuild* guild = CGuildManager::instance().FindGuild(m.gid);
			if (!guild)
				continue;
			if (m.collected >= m.target)
			{
				FinishGuildDutyMission(m, guild, GUILD_DUTY_DONE, now);
				continue;
			}
			// Places no core has kept up: given up, the count with them.
			std::unique_ptr<SQLMsg> stale(GuildDutyQuery(
					"SELECT pid FROM player.guild_duty_worker WHERE mission_id = %u AND last_seen < %u",
					m.id, (DWORD)now - GUILD_DUTY_WORKER_STALE_S));
			if (GuildDutyOk(stale) && stale->Get()->pSQLResult)
			{
				std::vector<DWORD> gone;
				MYSQL_ROW row;
				while (NULL != (row = mysql_fetch_row(stale->Get()->pSQLResult)))
				{
					DWORD pid = 0;
					str_to_number(pid, row[0]);
					// Not in this core's world, or out of the guild: its place.
					LPCHARACTER who = pid ? CHARACTER_MANAGER::instance().FindByPID(pid) : NULL;
					if (pid && (!who || !who->GetGuild() || who->GetGuild()->GetID() != m.gid))
						gone.push_back(pid);
				}
				for (size_t g = 0; g < gone.size(); ++g)
				{
					std::unique_ptr<SQLMsg> del(GuildDutyQuery(
							"DELETE FROM player.guild_duty_worker WHERE mission_id = %u AND pid = %u AND last_seen < %u",
							m.id, gone[g], (DWORD)now - GUILD_DUTY_WORKER_STALE_S));
					if (GuildDutyAffected(del))
					{
						std::unique_ptr<SQLMsg> dec(GuildDutyQuery(
								"UPDATE player.guild_duty_mission SET workers = workers - 1 WHERE id = %u AND workers > 0", m.id));
						if (m.workers > 0)
							--m.workers;
					}
				}
			}

			std::map<DWORD, std::vector<LPCHARACTER> >::iterator local = bots.find(m.gid);
			if (local == bots.end())
				continue;
			// Who of this core already works on it.
			std::set<DWORD> mine;
			std::unique_ptr<SQLMsg> rows(GuildDutyQuery(
					"SELECT pid FROM player.guild_duty_worker WHERE mission_id = %u", m.id));
			if (GuildDutyOk(rows) && rows->Get()->pSQLResult)
			{
				MYSQL_ROW row;
				while (NULL != (row = mysql_fetch_row(rows->Get()->pSQLResult)))
				{
					DWORD pid = 0;
					str_to_number(pid, row[0]);
					mine.insert(pid);
				}
			}
			std::string seen;
			for (size_t b = 0; b < local->second.size(); ++b)
			{
				LPCHARACTER bot = local->second[b];
				if (!mine.count(bot->GetPlayerID()))
					continue;
				char pid[16];
				snprintf(pid, sizeof(pid), "%s%u", seen.empty() ? "" : ",", bot->GetPlayerID());
				seen += pid;
				const long map = GetGuildDutyGroundFor(bot, m.vnum);
				TGuildDutyWorker w = { m.id, m.vnum, map };
				workers[bot->GetPlayerID()] = w;
			}
			if (!seen.empty())
			{
				std::unique_ptr<SQLMsg> touch(GuildDutyQuery(
						"UPDATE player.guild_duty_worker SET last_seen = %u WHERE mission_id = %u AND pid IN (%s)",
						(DWORD)now, m.id, seen.c_str()));
			}

			// New hands while there is room: the strongest of the level first.
			if (m.workers < m.workersMax)
			{
				std::vector<std::pair<int, LPCHARACTER> > fit;
				for (size_t b = 0; b < local->second.size(); ++b)
				{
					LPCHARACTER bot = local->second[b];
					TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(bot->GetPlayerID());
					if (st == s_mapPlayerBotAIStates.end() || mine.count(bot->GetPlayerID()) ||
							workers.count(bot->GetPlayerID()) || s_mapGuildDutyWorkers.count(bot->GetPlayerID()) ||
							!IsGuildDutyFreeBot(bot, st->second) || GetGuildDutyGroundFor(bot, m.vnum) == 0)
						continue;
					fit.push_back(std::make_pair(-(int)bot->GetLevel(), bot));
				}
				std::sort(fit.begin(), fit.end());
				for (size_t f = 0; f < fit.size() && m.workers < m.workersMax; ++f)
				{
					LPCHARACTER bot = fit[f].second;
					std::unique_ptr<SQLMsg> place(GuildDutyQuery(
							"UPDATE player.guild_duty_mission SET workers = workers + 1 "
							"WHERE id = %u AND state = %d AND workers < workers_max", m.id, GUILD_DUTY_ACTIVE));
					if (!GuildDutyAffected(place))
						break;
					std::unique_ptr<SQLMsg> join(GuildDutyQuery(
							"INSERT IGNORE INTO player.guild_duty_worker (mission_id, pid, joined_ts, last_seen) "
							"VALUES (%u, %u, %u, %u)", m.id, bot->GetPlayerID(), (DWORD)now, (DWORD)now));
					if (!GuildDutyAffected(join))
					{
						std::unique_ptr<SQLMsg> back(GuildDutyQuery(
								"UPDATE player.guild_duty_mission SET workers = workers - 1 WHERE id = %u AND workers > 0", m.id));
						continue;
					}
					++m.workers;
					const long map = GetGuildDutyGroundFor(bot, m.vnum);
					TGuildDutyWorker w = { m.id, m.vnum, map };
					workers[bot->GetPlayerID()] = w;
					mine.insert(bot->GetPlayerID());
					char text[160];
					snprintf(text, sizeof(text), "Ide zbierac %s dla gildii (%s).", GuildDutyItemName(m.vnum),
							GuildDutyMapName(map));
					GuildDutyBotSays(bot, text, true);
					sys_log(0, "GUILD_DUTY: mission %u guild=%s worker pid=%u name=%s level=%u map=%ld",
							m.id, guild->GetName(), bot->GetPlayerID(), bot->GetName(), (unsigned int)bot->GetLevel(), map);
				}
			}

			// What the guild's bots of this core carry of it goes to the bank:
			// the workers', and anybody else's who has some on him.
			for (size_t b = 0; b < local->second.size() && m.collected < m.target; ++b)
			{
				LPCHARACTER bot = local->second[b];
				if (bot->IsDead() || IsPlayerBotSidekickPID(bot->GetPlayerID()))
					continue;
				DeliverGuildDutyItems(bot, guild, m, now);
			}
			if (m.collected >= m.target)
			{
				FinishGuildDutyMission(m, guild, GUILD_DUTY_DONE, now);
				for (std::map<DWORD, TGuildDutyWorker>::iterator w = workers.begin(); w != workers.end(); )
				{
					if (w->second.mission == m.id)
						workers.erase(w++);
					else
						++w;
				}
			}
		}
		s_mapGuildDutyWorkers.swap(workers);
	}

	// A cancelled mission's pieces back to the bots that brought them, on
	// whichever core each is (and later, for one that is not in the world).
	void ManageGuildDutyReturns(time_t now)
	{
		std::unique_ptr<SQLMsg> msg(GuildDutyQuery(
				"SELECT mission_id, bot_pid, vnum, count FROM player.guild_duty_bank "
				"WHERE return_pending = 1 AND count > 0 LIMIT 200"));
		if (!GuildDutyOk(msg) || !msg->Get()->pSQLResult)
			return;
		MYSQL_ROW row;
		struct TRet { DWORD mission, pid, vnum, count; };
		std::vector<TRet> rets;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			TRet r = { 0, 0, 0, 0 };
			str_to_number(r.mission, row[0]);
			str_to_number(r.pid, row[1]);
			str_to_number(r.vnum, row[2]);
			str_to_number(r.count, row[3]);
			rets.push_back(r);
		}
		for (size_t i = 0; i < rets.size(); ++i)
		{
			const TRet& r = rets[i];
			if (!s_mapPlayerBotAIStates.count(r.pid))
				continue;
			LPCHARACTER bot = CHARACTER_MANAGER::instance().FindByPID(r.pid);
			if (!bot || !bot->IsItemLoaded() || (bot->CountSpecifyItem(r.vnum) == 0 && bot->GetEmptyInventory(1) < 0))
				continue;
			std::unique_ptr<SQLMsg> took(GuildDutyQuery(
					"UPDATE player.guild_duty_bank SET count = 0, returned = returned + %u, last_ts = %u "
					"WHERE mission_id = %u AND bot_pid = %u AND vnum = %u AND return_pending = 1 AND count = %u",
					r.count, (DWORD)now, r.mission, r.pid, r.vnum, r.count));
			if (!GuildDutyAffected(took))
				continue;
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(r.vnum);
			const DWORD stack = proto && proto->dwMaxStack > 0 ? proto->dwMaxStack : 200;
			for (DWORD left = r.count; left > 0; )
			{
				const DWORD n = std::min(left, stack);
				bot->AutoGiveItem(r.vnum, n, -1, false);
				left -= n;
			}
			sys_log(0, "GUILD_DUTY: returned mission=%u pid=%u name=%s vnum=%u count=%u",
					r.mission, r.pid, bot->GetName(), r.vnum, r.count);
		}
	}

	// ----------------------------------------------------------- the tower

	int GetGuildDutyTowerFloorNow()
	{
		const TPlayerBotTowerRaid& raid = s_PlayerBotTowerRaid;
		if (raid.bPhase != TOWER_PHASE_INSIDE)
			return 0;
		std::map<long, TPlayerBotTowerRun>::const_iterator run = s_mapPlayerBotTowerRuns.find(raid.lInstance);
		return run != s_mapPlayerBotTowerRuns.end() ? std::max(1, run->second.iLevel + 2) : 1;
	}

	// The party: the guild's bots of the level (GatherPlayerBotTowerMembers),
	// and bots of the kingdom hired to fill the places the leader asked for.
	void GatherGuildDutyTowerParty(CGuild* guild, BYTE empire, int wanted, TPlayerBotTowerGuildEntry& e, int& recruits)
	{
		e.guild = guild;
		e.empire = empire;
		GatherPlayerBotTowerMembers(guild, e.members, e.upper);
		if ((int)e.members.size() > wanted)
			e.members.resize(wanted);
		recruits = 0;
		if ((int)e.members.size() >= wanted)
			return;
		std::set<DWORD> taken;
		for (size_t i = 0; i < e.members.size(); ++i)
			taken.insert(e.members[i].pid);
		std::vector<TPlayerBotTowerCandidate> hired;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch || taken.count(it->first) || ch->GetGuild() == guild || ch->GetEmpire() != empire ||
					ch->GetLevel() < PLAYERBOT_TOWER_MIN_LEVEL || !IsGuildDutyFreeBot(ch, it->second) ||
					IsPlayerBotDropper(it->second.bPersonality) || IsPlayerBotAngler(ch, it->second) ||
					IsPlayerBotMiner(ch, it->second) || it->second.bFishingSession ||
					IsPlayerBotCatacombRaider(it->first) || s_mapGuildDutyWorkers.count(it->first))
				continue;
			TPlayerBotTowerCandidate c;
			c.pid = it->first;
			c.strength = GetPlayerBotStrengthCached(it->first);
			if (c.strength <= 0)
				c.strength = ch->GetLevel() * 1000;
			c.level = ch->GetLevel();
			hired.push_back(c);
		}
		std::sort(hired.begin(), hired.end(), PlayerBotTowerCandidateOrder);
		for (size_t i = 0; i < hired.size() && (int)e.members.size() < wanted; ++i)
		{
			e.members.push_back(hired[i]);
			if (hired[i].level >= PLAYERBOT_TOWER_UPPER_LEVEL)
				++e.upper;
			++recruits;
		}
	}

	void ManageGuildDutyTower(DWORD dwNow, time_t now)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		// The raid is the first channel's, on the core that hosts the tower.
		if (g_bChannel != 1 || !IsPlayerBotMapHostedHere(PLAYERBOT_MAP_DEMON_TOWER))
			return;
		std::unique_ptr<SQLMsg> msg(GuildDutyQuery(
				"SELECT id, guild_id, leader_pid, empire, wanted, state, created_ts FROM player.guild_duty_tower "
				"WHERE state IN (%d, %d, %d, %d) ORDER BY id", GUILD_TOWER_WAITING, GUILD_TOWER_GATHER,
				GUILD_TOWER_STONE, GUILD_TOWER_INSIDE));
		if (!GuildDutyOk(msg) || !msg->Get()->pSQLResult)
			return;
		struct TRow { DWORD id, gid, leader, empire, wanted, state, created; };
		std::vector<TRow> rows;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			TRow r = { 0, 0, 0, 0, 0, 0, 0 };
			str_to_number(r.id, row[0]);
			str_to_number(r.gid, row[1]);
			str_to_number(r.leader, row[2]);
			str_to_number(r.empire, row[3]);
			str_to_number(r.wanted, row[4]);
			str_to_number(r.state, row[5]);
			str_to_number(r.created, row[6]);
			rows.push_back(r);
		}
		TPlayerBotTowerRaid& raid = s_PlayerBotTowerRaid;

		// The expedition this core runs: its raid followed, or ended when the
		// leader called it off before the tower.
		if (s_dwGuildDutyTowerId)
		{
			const TRow* mine = NULL;
			for (size_t i = 0; i < rows.size(); ++i)
				if (rows[i].id == s_dwGuildDutyTowerId)
					mine = &rows[i];
			const bool raiding = raid.bPhase != TOWER_PHASE_NONE && raid.dwGuildID == s_dwGuildDutyTowerGuild;
			if (!mine)
			{
				if (raiding && raid.bPhase != TOWER_PHASE_INSIDE)
					EndPlayerBotTowerRaid(dwNow, "guild_duty_cancel");
				s_dwGuildDutyTowerId = 0;
			}
			else if (raiding)
			{
				s_iGuildDutyTowerFloor = std::max(s_iGuildDutyTowerFloor, GetGuildDutyTowerFloorNow());
				const int state = raid.bPhase == TOWER_PHASE_GATHER ? GUILD_TOWER_GATHER :
						raid.bPhase == TOWER_PHASE_STONE ? GUILD_TOWER_STONE : GUILD_TOWER_INSIDE;
				std::unique_ptr<SQLMsg> up(GuildDutyQuery(
						"UPDATE player.guild_duty_tower SET state = %d, floor = GREATEST(floor, %d), updated_ts = %u "
						"WHERE id = %u AND state IN (%d, %d, %d)", state, s_iGuildDutyTowerFloor, (DWORD)now, mine->id,
						GUILD_TOWER_GATHER, GUILD_TOWER_STONE, GUILD_TOWER_INSIDE));
			}
			else
			{
				std::unique_ptr<SQLMsg> up(GuildDutyQuery(
						"UPDATE player.guild_duty_tower SET state = %d, active_guild = NULL, floor = GREATEST(floor, %d), "
						"finished_ts = %u, updated_ts = %u WHERE id = %u AND state IN (%d, %d, %d)",
						s_iGuildDutyTowerFloor > 0 ? GUILD_TOWER_DONE : GUILD_TOWER_FAILED, s_iGuildDutyTowerFloor,
						(DWORD)now, (DWORD)now, mine->id, GUILD_TOWER_GATHER, GUILD_TOWER_STONE, GUILD_TOWER_INSIDE));
				CGuild* guild = CGuildManager::instance().FindGuild(mine->gid);
				if (GuildDutyAffected(up) && guild)
				{
					char text[160];
					if (s_iGuildDutyTowerFloor > 0)
						snprintf(text, sizeof(text), "Wyprawa na Wieze Demonow zakonczona - doszlismy do pietra %d.",
								s_iGuildDutyTowerFloor);
					else
						snprintf(text, sizeof(text), "Wyprawa na Wieze Demonow nie weszla do wiezy.");
					GuildDutyTell(guild, mine->leader, text);
				}
				sys_log(0, "GUILD_DUTY: tower %u guild=%u over floor=%d", mine->id, mine->gid, s_iGuildDutyTowerFloor);
				s_dwGuildDutyTowerId = 0;
			}
		}

		// Rows another start of this core left behind: over.
		for (size_t i = 0; i < rows.size(); ++i)
		{
			const TRow& r = rows[i];
			if (r.state == GUILD_TOWER_WAITING || r.id == s_dwGuildDutyTowerId)
				continue;
			std::unique_ptr<SQLMsg> up(GuildDutyQuery(
					"UPDATE player.guild_duty_tower SET state = %d, active_guild = NULL, finished_ts = %u, "
					"note = 'przerwana' WHERE id = %u AND state = %u", GUILD_TOWER_FAILED, (DWORD)now, r.id, r.state));
		}

		// The next one waiting, when the tower is free.
		bool waiting = false;
		for (size_t i = 0; i < rows.size() && !s_dwGuildDutyTowerId; ++i)
		{
			const TRow& r = rows[i];
			if (r.state != GUILD_TOWER_WAITING)
				continue;
			waiting = true;
			CGuild* guild = CGuildManager::instance().FindGuild(r.gid);
			if (!guild)
			{
				std::unique_ptr<SQLMsg> up(GuildDutyQuery(
						"UPDATE player.guild_duty_tower SET state = %d, active_guild = NULL, finished_ts = %u "
						"WHERE id = %u AND state = %d", GUILD_TOWER_FAILED, (DWORD)now, r.id, GUILD_TOWER_WAITING));
				continue;
			}
			if (raid.bPhase != TOWER_PHASE_NONE)
			{
				std::unique_ptr<SQLMsg> up(GuildDutyQuery(
						"UPDATE player.guild_duty_tower SET note = 'wieza zajeta - czeka', updated_ts = %u "
						"WHERE id = %u AND state = %d", (DWORD)now, r.id, GUILD_TOWER_WAITING));
				break;
			}
			TPlayerBotTowerGuildEntry e;
			int recruits = 0;
			const int wanted = std::max(GUILD_DUTY_TOWER_MIN, std::min(PLAYERBOT_TOWER_MAX_MEMBERS, (int)r.wanted));
			GatherGuildDutyTowerParty(guild, (BYTE)r.empire, wanted, e, recruits);
			if ((int)e.members.size() < GUILD_DUTY_TOWER_MIN)
			{
				const bool late = (DWORD)now >= r.created + GUILD_DUTY_TOWER_WAIT_S;
				std::unique_ptr<SQLMsg> up(GuildDutyQuery(
						"UPDATE player.guild_duty_tower SET state = %d, active_guild = %s, note = 'za malo botow (%u/%d)', "
						"updated_ts = %u, finished_ts = %u WHERE id = %u AND state = %d",
						late ? GUILD_TOWER_FAILED : GUILD_TOWER_WAITING, late ? "NULL" : "active_guild",
						(unsigned int)e.members.size(), GUILD_DUTY_TOWER_MIN, (DWORD)now, late ? (DWORD)now : 0U,
						r.id, GUILD_TOWER_WAITING));
				if (late)
					GuildDutyTell(guild, r.leader, "Wyprawa na Wieze Demonow odwolana - za malo botow.");
				continue;
			}
			std::unique_ptr<SQLMsg> up(GuildDutyQuery(
					"UPDATE player.guild_duty_tower SET state = %d, members = %u, recruits = %d, note = '', updated_ts = %u "
					"WHERE id = %u AND state = %d", GUILD_TOWER_GATHER, (unsigned int)e.members.size(), recruits,
					(DWORD)now, r.id, GUILD_TOWER_WAITING));
			if (!GuildDutyAffected(up))
				continue;
			CallPlayerBotTowerRaid(e, dwNow, "guild_duty");
			s_dwGuildDutyTowerId = r.id;
			s_dwGuildDutyTowerGuild = r.gid;
			s_iGuildDutyTowerFloor = 0;
			sys_log(0, "GUILD_DUTY: tower %u guild=%s called members=%u recruits=%d",
					r.id, guild->GetName(), (unsigned int)e.members.size(), recruits);
		}
		// While a leader's expedition waits, the tower's own clock does not
		// send a bot guild in ahead of it.
		if (waiting && s_dwNextPlayerBotTowerRaidTime < dwNow + 5 * 60 * 1000)
			s_dwNextPlayerBotTowerRaidTime = dwNow + 5 * 60 * 1000;
#else
		(void)dwNow;
		(void)now;
#endif
	}

	// -------------------------------------------------------- the world

	// Every GUILD_DUTY_PASS_MS on every core (CPlayerBotManager::Update).
	void ManagePlayerBotGuildDuties(DWORD dwNow)
	{
		if (s_dwGuildDutyNextPass != 0 && dwNow < s_dwGuildDutyNextPass)
			return;
		s_dwGuildDutyNextPass = dwNow + GUILD_DUTY_PASS_MS + number(0, 3000);
		if (!EnsureGuildDutyTables())
			return;
		const time_t now = time(0);
		std::map<DWORD, std::vector<LPCHARACTER> > bots;
		CollectGuildDutyBots(bots);
		ManageGuildDutyCollections(bots, now);
		ManageGuildDutyMissions(bots, now);
		ManageGuildDutyReturns(now);
		ManageGuildDutyTower(dwNow, now);
	}

	// The map a worker of an item mission hunts on (asked through
	// GetPlayerBotGuildErrandMap, playerbot_guild_land.h), or zero.
	long GetPlayerBotGuildDutyMap(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		std::map<DWORD, TGuildDutyWorker>::const_iterator it = s_mapGuildDutyWorkers.find(ch->GetPlayerID());
		return it != s_mapGuildDutyWorkers.end() ? it->second.map : 0;
	}

	// A worker keeps its mission's material off its counter.
	bool IsPlayerBotGuildDutyKeptItem(LPCHARACTER ch, DWORD vnum)
	{
		if (!ch)
			return false;
		std::map<DWORD, TGuildDutyWorker>::const_iterator it = s_mapGuildDutyWorkers.find(ch->GetPlayerID());
		return it != s_mapGuildDutyWorkers.end() && it->second.vnum == vnum;
	}

	// ------------------------------------------------------- the panel

	bool IsGuildDutyLeader(LPCHARACTER ch, CGuild*& guild)
	{
		guild = ch ? ch->GetGuild() : NULL;
		if (!guild || !ch->GetDesc() || ch->GetDesc()->IsBot())
			return false;
		return guild->GetMasterPID() == ch->GetPlayerID();
	}

	void SendGuildDutyPanel(LPCHARACTER ch, CGuild* guild)
	{
		const DWORD gid = guild->GetID();
		const time_t now = time(0);
		ch->ChatPacket(CHAT_TYPE_COMMAND, "GDBegin %lld %u %d %d %d", (long long)guild->GetGuildMoney(), (DWORD)now,
				GUILD_DUTY_COLLECT_HOURS_DEFAULT, GUILD_DUTY_TOWER_DEFAULT, PLAYERBOT_TOWER_MIN_LEVEL);

		// The collection: the running one, or the last.
		std::unique_ptr<SQLMsg> c(GuildDutyQuery(
				"SELECT id, state, target, collected, start_ts, end_ts FROM player.guild_duty_collect "
				"WHERE guild_id = %u ORDER BY (state = %d) DESC, id DESC LIMIT 1", gid, GUILD_DUTY_ACTIVE));
		MYSQL_ROW row = GuildDutyOk(c) && c->Get()->pSQLResult ? mysql_fetch_row(c->Get()->pSQLResult) : NULL;
		if (row)
		{
			DWORD id = 0;
			str_to_number(id, row[0]);
			std::unique_ptr<SQLMsg> n(GuildDutyQuery(
					"SELECT COUNT(*) FROM player.guild_duty_donation WHERE collect_id = %u", id));
			MYSQL_ROW nrow = GuildDutyOk(n) && n->Get()->pSQLResult ? mysql_fetch_row(n->Get()->pSQLResult) : NULL;
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GDCollect %s %s %s %s %s %s %s", row[0], row[1], row[2], row[3], row[4],
					row[5], nrow && nrow[0] ? nrow[0] : "0");
			std::unique_ptr<SQLMsg> d(GuildDutyQuery(
					"SELECT d.amount, IFNULL(p.name, '?') FROM player.guild_duty_donation d "
					"LEFT JOIN player.player p ON p.id = d.pid WHERE d.collect_id = %u ORDER BY d.amount DESC LIMIT 5", id));
			if (GuildDutyOk(d) && d->Get()->pSQLResult)
			{
				MYSQL_ROW drow;
				while (NULL != (drow = mysql_fetch_row(d->Get()->pSQLResult)))
					ch->ChatPacket(CHAT_TYPE_COMMAND, "GDDonor %s %s", drow[0], GuildDutyHex(drow[1]).c_str());
			}
		}
		else
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GDCollect 0 0 0 0 0 0 0");

		// The item mission: the running one, or the last.
		std::unique_ptr<SQLMsg> m(GuildDutyQuery(
				"SELECT id, state, vnum, target, collected, workers, workers_max FROM player.guild_duty_mission "
				"WHERE guild_id = %u ORDER BY (state = %d) DESC, id DESC LIMIT 1", gid, GUILD_DUTY_ACTIVE));
		row = GuildDutyOk(m) && m->Get()->pSQLResult ? mysql_fetch_row(m->Get()->pSQLResult) : NULL;
		if (row)
		{
			DWORD id = 0, vnum = 0;
			str_to_number(id, row[0]);
			str_to_number(vnum, row[2]);
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GDMission %s %s %s %s %s %s %s %d", row[0], row[1], row[2], row[3],
					row[4], row[5], row[6], GetGuildDutyMaterialMinLevel(vnum));
			std::unique_ptr<SQLMsg> w(GuildDutyQuery(
					"SELECT IFNULL(p.level, 0), w.delivered, IFNULL(p.name, '?') FROM player.guild_duty_worker w "
					"LEFT JOIN player.player p ON p.id = w.pid WHERE w.mission_id = %u ORDER BY w.delivered DESC LIMIT 8", id));
			if (GuildDutyOk(w) && w->Get()->pSQLResult)
			{
				MYSQL_ROW wrow;
				while (NULL != (wrow = mysql_fetch_row(w->Get()->pSQLResult)))
					ch->ChatPacket(CHAT_TYPE_COMMAND, "GDWorker %s %s %s", wrow[0], wrow[1], GuildDutyHex(wrow[2]).c_str());
			}
		}
		else
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GDMission 0 0 0 0 0 0 0 0");

		// The bank: what is in it, by material.
		std::unique_ptr<SQLMsg> b(GuildDutyQuery(
				"SELECT vnum, SUM(count) FROM player.guild_duty_bank WHERE guild_id = %u AND return_pending = 0 "
				"AND count > 0 GROUP BY vnum ORDER BY vnum LIMIT 12", gid));
		if (GuildDutyOk(b) && b->Get()->pSQLResult)
		{
			MYSQL_ROW brow;
			while (NULL != (brow = mysql_fetch_row(b->Get()->pSQLResult)))
				ch->ChatPacket(CHAT_TYPE_COMMAND, "GDBank %s %s", brow[0], brow[1]);
		}

		// The tower: the running expedition, or the last.
		std::unique_ptr<SQLMsg> t(GuildDutyQuery(
				"SELECT id, state, wanted, members, recruits, floor, created_ts, note FROM player.guild_duty_tower "
				"WHERE guild_id = %u ORDER BY (active_guild IS NOT NULL) DESC, id DESC LIMIT 1", gid));
		row = GuildDutyOk(t) && t->Get()->pSQLResult ? mysql_fetch_row(t->Get()->pSQLResult) : NULL;
		if (row)
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GDTower %s %s %s %s %s %s %s %s", row[0], row[1], row[2], row[3], row[4],
					row[5], row[6], GuildDutyHex(row[7]).c_str());
		else
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GDTower 0 0 0 0 0 0 0 -");
		ch->ChatPacket(CHAT_TYPE_COMMAND, "GDEnd");
	}

	void StartGuildDutyCollect(LPCHARACTER ch, CGuild* guild, long long amount, int hours)
	{
		if (amount < GUILD_DUTY_COLLECT_MIN || amount > GUILD_DUTY_COLLECT_MAX)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] Zrzutka: od %s do %s yang.",
					GuildDutyMoney(GUILD_DUTY_COLLECT_MIN).c_str(), GuildDutyMoney(GUILD_DUTY_COLLECT_MAX).c_str());
			return;
		}
		if (hours <= 0)
			hours = GUILD_DUTY_COLLECT_HOURS_DEFAULT;
		hours = std::min(hours, GUILD_DUTY_COLLECT_HOURS_MAX);
		const DWORD now = (DWORD)time(0);
		std::unique_ptr<SQLMsg> ins(GuildDutyQuery(
				"INSERT INTO player.guild_duty_collect (guild_id, leader_pid, target, start_ts, end_ts, state, active_guild) "
				"VALUES (%u, %u, %lld, %u, %u, %d, %u)", guild->GetID(), ch->GetPlayerID(), amount, now,
				now + (DWORD)hours * 3600U, GUILD_DUTY_ACTIVE, guild->GetID()));
		if (!GuildDutyAffected(ins))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] Jedna zrzutka juz trwa - poczekaj, az sie skonczy, albo ja anuluj.");
			return;
		}
		char text[200];
		snprintf(text, sizeof(text), "Lider %s oglasza zrzutke na gildie: %s yang w %d godz. Boty wplacaja do skarbca gildii.",
				ch->GetName(), GuildDutyMoney(amount).c_str(), hours);
		guild->Chat(text);
		sys_log(0, "GUILD_DUTY: collection started guild=%s leader=%s amount=%lld hours=%d",
				guild->GetName(), ch->GetName(), amount, hours);
	}

	void CancelGuildDutyCollect(LPCHARACTER ch, CGuild* guild)
	{
		std::unique_ptr<SQLMsg> up(GuildDutyQuery(
				"UPDATE player.guild_duty_collect SET state = %d, active_guild = NULL, finished_ts = %u "
				"WHERE guild_id = %u AND state = %d", GUILD_DUTY_CANCELLED, (DWORD)time(0), guild->GetID(), GUILD_DUTY_ACTIVE));
		if (GuildDutyAffected(up))
			ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] Zrzutka zakonczona. Wplacone yang zostaje w skarbcu gildii.");
		else
			ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] Nie ma trwajacej zrzutki.");
	}

	void StartGuildDutyMission(LPCHARACTER ch, CGuild* guild, DWORD vnum, int count)
	{
		if (!IsGuildDutyMaterial(vnum))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] Boty zbieraja tylko materialy na budowe: Pien, Kamien Weglowy, Dykta.");
			return;
		}
		if (count < 1 || count > GUILD_DUTY_MISSION_MAX)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] Ilosc: od 1 do %d sztuk.", GUILD_DUTY_MISSION_MAX);
			return;
		}
		const int workersMax = std::max(GUILD_DUTY_WORKERS_MIN, std::min(GUILD_DUTY_WORKERS_MAX, (count + 24) / 25));
		std::unique_ptr<SQLMsg> ins(GuildDutyQuery(
				"INSERT INTO player.guild_duty_mission (guild_id, leader_pid, vnum, target, workers_max, state, "
				"active_guild, created_ts) VALUES (%u, %u, %u, %d, %d, %d, %u, %u)", guild->GetID(), ch->GetPlayerID(),
				vnum, count, workersMax, GUILD_DUTY_ACTIVE, guild->GetID(), (DWORD)time(0)));
		if (!GuildDutyAffected(ins))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] Jedna misja na przedmioty juz trwa - anuluj ja, zeby zlecic nowa.");
			return;
		}
		char text[200];
		snprintf(text, sizeof(text), "Lider %s zleca misje: zebrac %d x %s (boty od %d poziomu).",
				ch->GetName(), count, GuildDutyItemName(vnum), GetGuildDutyMaterialMinLevel(vnum));
		guild->Chat(text);
		sys_log(0, "GUILD_DUTY: mission started guild=%s leader=%s vnum=%u count=%d workers=%d",
				guild->GetName(), ch->GetName(), vnum, count, workersMax);
	}

	void CancelGuildDutyMission(LPCHARACTER ch, CGuild* guild)
	{
		std::unique_ptr<SQLMsg> find(GuildDutyQuery(
				"SELECT id FROM player.guild_duty_mission WHERE guild_id = %u AND state = %d", guild->GetID(), GUILD_DUTY_ACTIVE));
		MYSQL_ROW row = GuildDutyOk(find) && find->Get()->pSQLResult ? mysql_fetch_row(find->Get()->pSQLResult) : NULL;
		DWORD id = 0;
		if (row)
			str_to_number(id, row[0]);
		std::unique_ptr<SQLMsg> up(GuildDutyQuery(
				"UPDATE player.guild_duty_mission SET state = %d, active_guild = NULL, finished_ts = %u "
				"WHERE id = %u AND state = %d", GUILD_DUTY_CANCELLED, (DWORD)time(0), id, GUILD_DUTY_ACTIVE));
		if (!id || !GuildDutyAffected(up))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] Nie ma trwajacej misji na przedmioty.");
			return;
		}
		std::unique_ptr<SQLMsg> back(GuildDutyQuery(
				"UPDATE player.guild_duty_bank SET return_pending = 1 WHERE mission_id = %u AND count > 0", id));
		std::unique_ptr<SQLMsg> freed(GuildDutyQuery("DELETE FROM player.guild_duty_worker WHERE mission_id = %u", id));
		ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] Misja anulowana - przedmioty z banku wracaja do botow, ktore je przyniosly.");
		guild->Chat("Misja na przedmioty odwolana przez lidera.");
		sys_log(0, "GUILD_DUTY: mission %u cancelled guild=%s leader=%s", id, guild->GetName(), ch->GetName());
		ManageGuildDutyReturns(time(0));
	}

	// Out of the bank to the leader: every row taken with a conditional
	// UPDATE before its items are given, so no two cores give the same piece.
	void WithdrawGuildDutyItems(LPCHARACTER ch, CGuild* guild, DWORD vnum, int want)
	{
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		if (!proto || want <= 0)
			return;
		const DWORD stack = proto->dwMaxStack > 0 ? proto->dwMaxStack : 200;
		// What the bag can take: room in the stacks there and the free cells.
		long long room = 0;
		for (int i = 0; i < ch->GetInventoryMaxCount(); ++i)
		{
			LPITEM item = ch->GetInventoryItem(i);
			if (item && item->GetVnum() == vnum && item->GetCount() < stack)
				room += stack - item->GetCount();
			else if (!item && ch->IsEmptyItemGrid(TItemPos(INVENTORY, i), 1))
				room += stack;
		}
		if (room <= 0)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] Brak miejsca w ekwipunku.");
			return;
		}
		want = (int)std::min<long long>(want, room);
		std::unique_ptr<SQLMsg> rows(GuildDutyQuery(
				"SELECT mission_id, bot_pid, count FROM player.guild_duty_bank WHERE guild_id = %u AND vnum = %u "
				"AND count > 0 AND return_pending = 0 ORDER BY mission_id, bot_pid", guild->GetID(), vnum));
		if (!GuildDutyOk(rows) || !rows->Get()->pSQLResult)
			return;
		struct TBank { DWORD mission, pid, count; };
		std::vector<TBank> bank;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(rows->Get()->pSQLResult)))
		{
			TBank b = { 0, 0, 0 };
			str_to_number(b.mission, row[0]);
			str_to_number(b.pid, row[1]);
			str_to_number(b.count, row[2]);
			bank.push_back(b);
		}
		int given = 0;
		for (size_t i = 0; i < bank.size() && given < want; ++i)
		{
			const int take = std::min<int>((int)bank[i].count, want - given);
			std::unique_ptr<SQLMsg> took(GuildDutyQuery(
					"UPDATE player.guild_duty_bank SET count = count - %d, withdrawn = withdrawn + %d, last_ts = %u "
					"WHERE mission_id = %u AND bot_pid = %u AND vnum = %u AND count >= %d AND return_pending = 0",
					take, take, (DWORD)time(0), bank[i].mission, bank[i].pid, vnum, take));
			if (!GuildDutyAffected(took))
				continue;
			given += take;
		}
		for (int left = given; left > 0; )
		{
			const int n = std::min<int>(left, (int)stack);
			ch->AutoGiveItem(vnum, n);
			left -= n;
		}
		if (given > 0)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] Wyplacono z banku gildii: %d x %s.", given, proto->szLocaleName);
			sys_log(0, "GUILD_DUTY: withdrawn guild=%s leader=%s vnum=%u count=%d", guild->GetName(), ch->GetName(), vnum, given);
		}
		else
			ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] W banku gildii nie ma tego przedmiotu.");
	}

	void StartGuildDutyTower(LPCHARACTER ch, CGuild* guild, int wanted)
	{
		if (wanted <= 0)
			wanted = GUILD_DUTY_TOWER_DEFAULT;
		wanted = std::max(GUILD_DUTY_TOWER_MIN, std::min(PLAYERBOT_TOWER_MAX_MEMBERS, wanted));
		const DWORD now = (DWORD)time(0);
		std::unique_ptr<SQLMsg> ins(GuildDutyQuery(
				"INSERT INTO player.guild_duty_tower (guild_id, leader_pid, empire, wanted, state, active_guild, note, "
				"created_ts, updated_ts) VALUES (%u, %u, %u, %d, %d, %u, 'czeka na boty', %u, %u)", guild->GetID(),
				ch->GetPlayerID(), (unsigned int)ch->GetEmpire(), wanted, GUILD_TOWER_WAITING, guild->GetID(), now, now));
		if (!GuildDutyAffected(ins))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] Wyprawa na Wieze Demonow juz jest zorganizowana.");
			return;
		}
		char text[220];
		snprintf(text, sizeof(text), "Lider %s organizuje wyprawe na Wieze Demonow (%d osob, od %d poziomu). "
				"Zbiorka na parterze wiezy, kanal 1.", ch->GetName(), wanted, PLAYERBOT_TOWER_MIN_LEVEL);
		guild->Chat(text);
		sys_log(0, "GUILD_DUTY: tower started guild=%s leader=%s wanted=%d", guild->GetName(), ch->GetName(), wanted);
	}

	void CancelGuildDutyTower(LPCHARACTER ch, CGuild* guild)
	{
		std::unique_ptr<SQLMsg> up(GuildDutyQuery(
				"UPDATE player.guild_duty_tower SET state = %d, active_guild = NULL, finished_ts = %u, note = 'odwolana' "
				"WHERE guild_id = %u AND state IN (%d, %d, %d)", GUILD_TOWER_CANCELLED, (DWORD)time(0), guild->GetID(),
				GUILD_TOWER_WAITING, GUILD_TOWER_GATHER, GUILD_TOWER_STONE));
		if (GuildDutyAffected(up))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] Wyprawa na Wieze Demonow odwolana.");
			guild->Chat("Wyprawa na Wieze Demonow odwolana przez lidera.");
		}
		else
			ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] Nie ma wyprawy do odwolania (w wiezy nie da sie juz jej odwolac).");
	}
}

// "/gildia_obowiazki" - the leader's panel (MT2009_PLUS_GUILD_DUTY_V1, the
// engine's command in cmd_general.cpp):
//   (nothing)                   the panel's data
//   zrzutka <yang> [hours]      a collection for the treasury
//   zrzutka_anuluj              the collection ends (what came in stays)
//   misja <vnum> <count>        bots gather a building material
//   misja_anuluj                the mission ends, the bank's pieces go back
//   wyplac <vnum> <count>       out of the item bank
//   dt [bots]                   an expedition to the Demon Tower
//   dt_anuluj                   the expedition called off (before the tower)
void GuildDutyCommand(LPCHARACTER ch, const char* argument)
{
	if (!ch || !ch->GetDesc())
		return;
	static std::map<DWORD, DWORD> s_mapAskedAt;
	DWORD& asked = s_mapAskedAt[ch->GetPlayerID()];
	const DWORD dwNow = get_dword_time();
	if (asked != 0 && dwNow - asked < 400)
		return;
	asked = dwNow;
	CGuild* guild = NULL;
	if (!IsGuildDutyLeader(ch, guild))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] Obowiazki gildii zleca tylko lider gildii.");
		return;
	}
	if (!EnsureGuildDutyTables())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "[Gildia] Obowiazki gildii sa chwilowo niedostepne.");
		return;
	}
	char sub[32], arg1[32], arg2[32];
	const char* rest = one_argument(argument, sub, sizeof(sub));
	rest = one_argument(rest, arg1, sizeof(arg1));
	one_argument(rest, arg2, sizeof(arg2));
	long long n1 = 0;
	int n2 = 0;
	str_to_number(n1, arg1);
	str_to_number(n2, arg2);
	if (!*sub || !strcmp(sub, "otworz"))
		;
	else if (!strcmp(sub, "zrzutka"))
		StartGuildDutyCollect(ch, guild, n1, n2);
	else if (!strcmp(sub, "zrzutka_anuluj"))
		CancelGuildDutyCollect(ch, guild);
	else if (!strcmp(sub, "misja"))
		StartGuildDutyMission(ch, guild, (DWORD)n1, n2);
	else if (!strcmp(sub, "misja_anuluj"))
		CancelGuildDutyMission(ch, guild);
	else if (!strcmp(sub, "wyplac"))
		WithdrawGuildDutyItems(ch, guild, (DWORD)n1, n2);
	else if (!strcmp(sub, "dt"))
		StartGuildDutyTower(ch, guild, (int)n1);
	else if (!strcmp(sub, "dt_anuluj"))
		CancelGuildDutyTower(ch, guild);
	SendGuildDutyPanel(ch, guild);
}

#endif
