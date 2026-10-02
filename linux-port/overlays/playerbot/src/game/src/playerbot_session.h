#ifndef __INC_METIN2_PLAYERBOT_SESSION_H__
#define __INC_METIN2_PLAYERBOT_SESSION_H__

// MT2009_PLUS_BOT_SESSIONS_V1: a bot's hours of play (the operator, 30
// September: "czy przy kazdym bocie mozemy podejrzec ich godziny gry kiedy
// grali i czy sa online"). One row of log.playerbot_session a session
// (mariadb/playerbot/log_schema.sql): opened as the bot enters the world
// (CPlayerBotManager::OnPlayerLoaded), closed as it leaves - Despawn with its
// reason, or the descriptor going without the manager asking - with why, and
// for the life schedule's rest until when. The reasons are
// playerbot_session_rules.h's numbers; the classic panel's "Sesje gry" card
// and its "Tylko boty" list read the rows.
//
// The rows are the database's clock: login_at and logout_at are its NOW(), so
// the panel, which asks the same database for its NOW(), never compares two
// clocks. A session is found again by pid, channel and core (the core's port)
// with no end yet, so a companion that follows its owner to another core
// never has its new row closed by the core it left.
//
// Nothing here waits for the database but the core's stop: the rows go to the
// log database's own asynchronous connection (LogManager::Query). At a stop
// the engine destroys the descriptors after that connection's last chance to
// run, so the first session closed while the core is shutting down closes
// every open session of this core in one direct statement, and the rest are
// only forgotten. A core that dies leaves its sessions open; its next start
// closes them where they were last seen (seen_at, every
// HEARTBEAT_SECONDS), their reason unknown. Rows are kept KEEP_DAYS days.
//
// Included once in playerbot_manager.cpp, after playerbot_explain.h.

#include "playerbot_session_rules.h"

extern bool g_bShutdown;

namespace
{
	namespace pss = playerbot_session_rules;

	const DWORD PLAYERBOT_SESSION_PURGE_MS = 60 * 60 * 1000;
	const DWORD PLAYERBOT_SESSION_PURGE_FIRST_MS = 10 * 60 * 1000;
	const int PLAYERBOT_SESSION_PURGE_ROWS = 20000;

	// The pids with an open row of this core.
	std::set<DWORD> s_setPlayerBotSessionOpen;
	// Whose next entry is the end of a rest, or an arrival from the other
	// channel: marked where the manager decides it, taken at the entry.
	std::set<DWORD> s_setPlayerBotSessionFromRest;
	std::set<DWORD> s_setPlayerBotSessionFromChannel;
	bool s_bPlayerBotSessionStarted = false;
	bool s_bPlayerBotSessionStopped = false;
	DWORD s_dwPlayerBotSessionBeatAt = 0;
	DWORD s_dwPlayerBotSessionPurgeAt = 0;
	char s_szPlayerBotSessionCore[17] = "";

	bool IsPlayerBotSessionOn()
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		return true;
#else
		return false;
#endif
	}

	// This core among the channel's cores: its port, which no other core of
	// the world has.
	const char* PlayerBotSessionCore()
	{
		if (s_szPlayerBotSessionCore[0] == '\0')
			snprintf(s_szPlayerBotSessionCore, sizeof(s_szPlayerBotSessionCore), "%u", (unsigned int)mother_port);
		return s_szPlayerBotSessionCore;
	}

	// Once, before this core's first row: what an earlier run of this core
	// left open was never closed - it ended where it was last seen, why
	// unknown - and a rest it granted is over, since a start brings every bot
	// back whatever it was doing. Queued ahead of any row this run writes.
	void StartPlayerBotSessions()
	{
		if (s_bPlayerBotSessionStarted || !IsPlayerBotSessionOn())
			return;
		s_bPlayerBotSessionStarted = true;
		const char* core = PlayerBotSessionCore();
		LogManager::instance().Query(
				"UPDATE playerbot_session SET logout_at = COALESCE(seen_at, login_at), logout_reason = %u "
				"WHERE channel = %u AND core = '%s' AND logout_at IS NULL",
				(unsigned int)pss::OUT_UNKNOWN, (unsigned int)g_bChannel, core);
		LogManager::instance().Query(
				"UPDATE playerbot_session SET rest_until = NOW() "
				"WHERE channel = %u AND core = '%s' AND rest_until > NOW()",
				(unsigned int)g_bChannel, core);
		sys_log(0, "PLAYERBOT_SESSION: recording the bots' sessions channel=%u core=%s",
				(unsigned int)g_bChannel, core);
	}

	void NotePlayerBotSessionRestOver(DWORD pid)
	{
		if (IsPlayerBotSessionOn())
			s_setPlayerBotSessionFromRest.insert(pid);
	}

	void NotePlayerBotSessionFromChannel(DWORD pid)
	{
		if (IsPlayerBotSessionOn())
			s_setPlayerBotSessionFromChannel.insert(pid);
	}

	// Every rest this core granted ended now (the LIFE switch turned off, or
	// LIFE_HOURS set to the whole day): the rows stop promising a return.
	void EndPlayerBotSessionRests()
	{
		if (!IsPlayerBotSessionOn())
			return;
		StartPlayerBotSessions();
		LogManager::instance().Query(
				"UPDATE playerbot_session SET rest_until = NOW() "
				"WHERE channel = %u AND core = '%s' AND rest_until > NOW()",
				(unsigned int)g_bChannel, PlayerBotSessionCore());
	}

	void OpenPlayerBotSession(DWORD pid, BYTE reason)
	{
		if (!IsPlayerBotSessionOn() || pid == 0 || s_bPlayerBotSessionStopped)
			return;
		StartPlayerBotSessions();
		const char* core = PlayerBotSessionCore();
		if (s_setPlayerBotSessionOpen.find(pid) != s_setPlayerBotSessionOpen.end())
		{
			// Entered again without leaving: whatever came between is unknown.
			LogManager::instance().Query(
					"UPDATE playerbot_session SET logout_at = NOW(), seen_at = NOW(), logout_reason = %u "
					"WHERE pid = %u AND channel = %u AND core = '%s' AND logout_at IS NULL",
					(unsigned int)pss::OUT_UNKNOWN, pid, (unsigned int)g_bChannel, core);
		}
		s_setPlayerBotSessionOpen.insert(pid);
		// Out and in within one second is one session: the key is the second.
		LogManager::instance().Query(
				"INSERT INTO playerbot_session (pid, login_at, channel, core, login_reason, seen_at) "
				"VALUES (%u, NOW(), %u, '%s', %u, NOW()) "
				"ON DUPLICATE KEY UPDATE channel = VALUES(channel), core = VALUES(core), "
				"seen_at = VALUES(seen_at), logout_at = NULL, logout_reason = 0, rest_until = NULL",
				pid, (unsigned int)g_bChannel, core, (unsigned int)reason);
	}

	// The reason the bot entered: a companion is its owner's, then whatever
	// the manager marked. Both marks are taken either way.
	BYTE TakePlayerBotSessionEntryReason(DWORD pid, bool sidekick)
	{
		const bool fromChannel = s_setPlayerBotSessionFromChannel.erase(pid) > 0;
		const bool fromRest = s_setPlayerBotSessionFromRest.erase(pid) > 0;
		if (sidekick)
			return pss::IN_SIDEKICK;
		if (fromChannel)
			return pss::IN_CHANNEL;
		if (fromRest)
			return pss::IN_REST;
		return pss::IN_START;
	}

	// The core is stopping: every open session of this core ends now, in one
	// statement that is done before this returns (the log connection's thread
	// may not run again). The player database's connection reaches `log` as
	// the retirement's and the ban check's reach `account`.
	void StopPlayerBotSessions()
	{
		if (s_bPlayerBotSessionStopped)
			return;
		s_bPlayerBotSessionStopped = true;
		if (!IsPlayerBotSessionOn() || !s_bPlayerBotSessionStarted)
			return;
		char query[512];
		snprintf(query, sizeof(query),
				"UPDATE log.playerbot_session SET logout_at = NOW(), seen_at = NOW(), logout_reason = %u "
				"WHERE channel = %u AND core = '%s' AND logout_at IS NULL",
				(unsigned int)pss::OUT_STOP, (unsigned int)g_bChannel, PlayerBotSessionCore());
		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("%s", query));
		sys_log(0, "PLAYERBOT_SESSION: core stopping, %u open sessions closed%s",
				(unsigned int)s_setPlayerBotSessionOpen.size(),
				msg.get() && msg->uiSQLErrno == 0 ? "" : " (the statement failed)");
		s_setPlayerBotSessionOpen.clear();
	}

	// restSeconds only with OUT_REST: when the bot is due back.
	void ClosePlayerBotSession(DWORD pid, BYTE reason, DWORD restSeconds)
	{
		if (!IsPlayerBotSessionOn())
			return;
		if (g_bShutdown)
		{
			StopPlayerBotSessions();
			return;
		}
		if (s_setPlayerBotSessionOpen.erase(pid) == 0)
			return;
		char restUntil[64];
		if (reason == pss::OUT_REST && restSeconds > 0)
			snprintf(restUntil, sizeof(restUntil), "NOW() + INTERVAL %u SECOND", (unsigned int)restSeconds);
		else
			strlcpy(restUntil, "NULL", sizeof(restUntil));
		LogManager::instance().Query(
				"UPDATE playerbot_session SET logout_at = NOW(), seen_at = NOW(), logout_reason = %u, rest_until = %s "
				"WHERE pid = %u AND channel = %u AND core = '%s' AND logout_at IS NULL",
				(unsigned int)reason, restUntil, pid, (unsigned int)g_bChannel, PlayerBotSessionCore());
	}

	// Once a manager tick: the open sessions' heartbeat, and the rows older
	// than KEEP_DAYS days. Every core asks the purge (the statement is the
	// same), spread by the core's port like the explanations' cleanup.
	void ManagePlayerBotSessions(DWORD now)
	{
		if (!IsPlayerBotSessionOn() || !s_bPlayerBotSessionStarted || s_bPlayerBotSessionStopped)
			return;
		if (s_dwPlayerBotSessionBeatAt == 0)
			s_dwPlayerBotSessionBeatAt = now + pss::HEARTBEAT_SECONDS * 1000U;
		else if ((int)(now - s_dwPlayerBotSessionBeatAt) >= 0)
		{
			s_dwPlayerBotSessionBeatAt = now + pss::HEARTBEAT_SECONDS * 1000U;
			if (!s_setPlayerBotSessionOpen.empty())
				LogManager::instance().Query(
						"UPDATE playerbot_session SET seen_at = NOW() "
						"WHERE channel = %u AND core = '%s' AND logout_at IS NULL",
						(unsigned int)g_bChannel, PlayerBotSessionCore());
		}
		if (s_dwPlayerBotSessionPurgeAt == 0)
		{
			s_dwPlayerBotSessionPurgeAt = now + PLAYERBOT_SESSION_PURGE_FIRST_MS +
					(DWORD)(mother_port % 60) * 1000U;
			return;
		}
		if ((int)(now - s_dwPlayerBotSessionPurgeAt) < 0)
			return;
		s_dwPlayerBotSessionPurgeAt = now + PLAYERBOT_SESSION_PURGE_MS;
		// An open session is kept while it is seen, however long ago it began.
		LogManager::instance().Query(
				"DELETE FROM playerbot_session WHERE login_at < NOW() - INTERVAL %d DAY "
				"AND COALESCE(logout_at, seen_at, login_at) < NOW() - INTERVAL %d DAY LIMIT %d",
				pss::KEEP_DAYS, pss::KEEP_DAYS, PLAYERBOT_SESSION_PURGE_ROWS);
	}
}

#endif
