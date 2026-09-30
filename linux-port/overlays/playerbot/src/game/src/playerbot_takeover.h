#ifndef PLAYERBOT_TAKEOVER_H
#define PLAYERBOT_TAKEOVER_H
// =============================================================================
//  playerbot_takeover.h -- "Przejmij bota na xxx minut" (the operator, 27
//  September 2026): the advanced panel hands a person a bot's account for a
//  while. The bot leaves the world, its account opens with a password the
//  panel made, the person plays the character; when the time is up the
//  account closes again, a person still on it is sent off, and the bot comes
//  back to its own business.
//
//  The panel writes one row of common.playerbot_takeover per bot (the password
//  hash, the account's own password and status to put back, the minutes), and
//  every core reads the table every PLAYERBOT_TAKEOVER_POLL_MS. The row walks:
//
//    requested  the core the bot plays on logs it out; once it has been gone
//               from every core (this one and P2P) for PLAYERBOT_TAKEOVER_
//               SETTLE_SEC, whichever core sees it first moves the row on and
//               opens the account (a conditional UPDATE, so exactly one does);
//    active     nobody spawns the bot (IsPlayerBotTakeoverHold: Spawn and the
//               top-up ask it); the person may log in until `until`;
//    returning  the account is closed as it was, and every core sends off a
//               person it hosts on that character; once nobody has been on it
//               for PLAYERBOT_TAKEOVER_RETURN_SEC the row is done;
//    done       the hold is gone, and the core that schedules the bot brings
//               it back with its next top-up (TopUpMissingBots), as a bot.
//
//  Nothing here waits: every step is a short query of a small table, and a
//  step that did not happen this poll happens on the next.
//
//  Included from playerbot_manager.cpp after playerbot_retirement.h.
// =============================================================================

namespace {

std::set<DWORD> s_setPlayerBotTakeoverHold;
DWORD s_dwPlayerBotTakeoverNextPoll = 0;
bool s_bPlayerBotTakeoverTableReady = false;
const DWORD PLAYERBOT_TAKEOVER_POLL_MS = 3000;
const int PLAYERBOT_TAKEOVER_SETTLE_SEC = 8;
const int PLAYERBOT_TAKEOVER_RETURN_SEC = 6;

bool IsPlayerBotTakeoverHold(DWORD dwPlayerID)
{
	return s_setPlayerBotTakeoverHold.find(dwPlayerID) != s_setPlayerBotTakeoverHold.end();
}

// Rows the statement changed, or -1 when it failed.
long long PlayerBotTakeoverExec(const char* query)
{
	std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
	if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get())
	{
		sys_err("PLAYERBOT_TAKEOVER: SQL failed: %s", query);
		return -1;
	}
	return (long long)msg->Get()->uiAffectedRows;
}

bool IsPlayerBotTakeoverOnline(DWORD dwPlayerID)
{
	return CHARACTER_MANAGER::instance().FindByPID(dwPlayerID) != NULL ||
			P2P_MANAGER::instance().FindByPID(dwPlayerID) != NULL;
}

void ProcessPlayerBotTakeovers(DWORD dwNow)
{
	if (dwNow < s_dwPlayerBotTakeoverNextPoll)
		return;
	s_dwPlayerBotTakeoverNextPoll = dwNow + PLAYERBOT_TAKEOVER_POLL_MS;

	if (!s_bPlayerBotTakeoverTableReady)
	{
		// The panel creates it too; whichever comes first.
		s_bPlayerBotTakeoverTableReady = PlayerBotTakeoverExec(
				"CREATE TABLE IF NOT EXISTS common.playerbot_takeover ("
				"pid INT UNSIGNED NOT NULL PRIMARY KEY, "
				"account_id INT UNSIGNED NOT NULL, "
				"login VARCHAR(30) NOT NULL DEFAULT '', "
				"password_plain VARCHAR(32) NOT NULL DEFAULT '', "
				"password_hash VARCHAR(42) NOT NULL, "
				"old_password VARCHAR(42) NOT NULL, "
				"old_status VARCHAR(8) NOT NULL, "
				"minutes INT UNSIGNED NOT NULL, "
				"state VARCHAR(12) NOT NULL DEFAULT 'requested', "
				"requested_at INT UNSIGNED NOT NULL DEFAULT 0, "
				"active_at INT UNSIGNED NOT NULL DEFAULT 0, "
				"until INT UNSIGNED NOT NULL DEFAULT 0, "
				"returning_at INT UNSIGNED NOT NULL DEFAULT 0, "
				"done_at INT UNSIGNED NOT NULL DEFAULT 0) ENGINE=InnoDB") >= 0;
		if (!s_bPlayerBotTakeoverTableReady)
			return;
	}

	std::unique_ptr<SQLMsg> rows(AccountDB::instance().DirectQuery(
			"SELECT pid, state, login, "
			"CAST(UNIX_TIMESTAMP() AS SIGNED) - CAST(requested_at AS SIGNED), "
			"CAST(until AS SIGNED) - CAST(UNIX_TIMESTAMP() AS SIGNED), "
			"CAST(UNIX_TIMESTAMP() AS SIGNED) - CAST(returning_at AS SIGNED) "
			"FROM common.playerbot_takeover WHERE state IN ('requested','active','returning')"));
	if (!rows.get() || rows->uiSQLErrno != 0 || !rows->Get() || !rows->Get()->pSQLResult)
		return;

	std::set<DWORD> hold;
	char query[1024];
	MYSQL_ROW row;
	while (NULL != (row = mysql_fetch_row(rows->Get()->pSQLResult)))
	{
		DWORD pid = 0;
		long long sinceRequested = 0, untilLeft = 0, sinceReturning = 0;
		str_to_number(pid, row[0]);
		std::string state = row[1] ? row[1] : "";
		const std::string login = row[2] ? row[2] : "";
		if (row[3]) str_to_number(sinceRequested, row[3]);
		if (row[4]) str_to_number(untilLeft, row[4]);
		if (row[5]) str_to_number(sinceReturning, row[5]);
		if (pid == 0)
			continue;
		hold.insert(pid);

		LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pid);
		const bool localBot = ch && ch->GetDesc() && ch->GetDesc()->IsBot();

		// The bot out of the world for as long as a person may hold it.
		if ((state == "requested" || state == "active") && localBot)
		{
			sys_log(0, "PLAYERBOT_TAKEOVER: bot logged out for a takeover pid=%u name=%s state=%s",
					pid, ch->GetName(), state.c_str());
			CPlayerBotManager::instance().Despawn(pid);
			continue;
		}

		if (state == "requested" && sinceRequested >= PLAYERBOT_TAKEOVER_SETTLE_SEC &&
				!IsPlayerBotTakeoverOnline(pid))
		{
			snprintf(query, sizeof(query),
					"UPDATE common.playerbot_takeover SET state='active', active_at=UNIX_TIMESTAMP(), "
					"until=UNIX_TIMESTAMP()+minutes*60 WHERE pid=%u AND state='requested'", pid);
			if (PlayerBotTakeoverExec(query) > 0)
			{
				snprintf(query, sizeof(query),
						"UPDATE account.account AS a JOIN common.playerbot_takeover AS t ON t.account_id=a.id "
						"SET a.password=t.password_hash, a.status='OK' WHERE t.pid=%u", pid);
				PlayerBotTakeoverExec(query);
				sys_log(0, "PLAYERBOT_TAKEOVER: account opened pid=%u", pid);
			}
			continue;
		}

		if (state == "active" && untilLeft <= 0)
		{
			snprintf(query, sizeof(query),
					"UPDATE common.playerbot_takeover SET state='returning', returning_at=UNIX_TIMESTAMP() "
					"WHERE pid=%u AND state='active'", pid);
			if (PlayerBotTakeoverExec(query) > 0)
			{
				snprintf(query, sizeof(query),
						"UPDATE account.account AS a JOIN common.playerbot_takeover AS t ON t.account_id=a.id "
						"SET a.password=t.old_password, a.status=t.old_status WHERE t.pid=%u", pid);
				PlayerBotTakeoverExec(query);
				sys_log(0, "PLAYERBOT_TAKEOVER: time is up, account closed pid=%u", pid);
			}
			state = "returning";
			sinceReturning = 0;
		}

		if (state == "returning")
		{
			// A person still on the character is sent off, wherever the
			// channel: here directly, and by login on every other core (the
			// engine's own "somebody logged in to your account" packet, which
			// also reaches a core that hosts no bot and so runs no manager).
			if (ch && ch->GetDesc() && !ch->GetDesc()->IsBot())
			{
				ch->ChatPacket(CHAT_TYPE_NOTICE, "Czas przejecia bota minal - postac wraca do bota.");
				ch->GetDesc()->DelayedDisconnect(2);
				sys_log(0, "PLAYERBOT_TAKEOVER: person sent off pid=%u name=%s", pid, ch->GetName());
			}
			else if (!login.empty() && P2P_MANAGER::instance().FindByPID(pid) != NULL)
			{
				TPacketGGDisconnect pgg;
				pgg.bHeader = HEADER_GG_DISCONNECT;
				strlcpy(pgg.szLogin, login.c_str(), sizeof(pgg.szLogin));
				P2P_MANAGER::instance().Send(&pgg, sizeof(TPacketGGDisconnect));
				sys_log(0, "PLAYERBOT_TAKEOVER: person sent off on another core pid=%u login=%s", pid, login.c_str());
			}
			else if (sinceReturning >= PLAYERBOT_TAKEOVER_RETURN_SEC && !IsPlayerBotTakeoverOnline(pid))
			{
				snprintf(query, sizeof(query),
						"UPDATE common.playerbot_takeover SET state='done', done_at=UNIX_TIMESTAMP() "
						"WHERE pid=%u AND state='returning'", pid);
				if (PlayerBotTakeoverExec(query) > 0)
					sys_log(0, "PLAYERBOT_TAKEOVER: takeover over, the bot returns pid=%u", pid);
			}
		}
	}
	s_setPlayerBotTakeoverHold.swap(hold);
}

} // namespace

#endif
