#ifndef __INC_PLAYERBOT_SESSION_RULES_H__
#define __INC_PLAYERBOT_SESSION_RULES_H__

// MT2009_PLUS_BOT_SESSIONS_V1: a bot's hours of play - the numbers of
// log.playerbot_session's login_reason and logout_reason, which the classic
// panel's "Sesje gry" card and its "Tylko boty" list say in words
// (admin_panel.py, ss_in_N and ss_out_N). Pure, no engine types; the engine's
// half is playerbot_session.h. A number, once written, never changes meaning:
// eight days of rows carry it, so a new reason takes a new number.

namespace playerbot_session_rules
{
	// Why a session began.
	const unsigned char IN_UNKNOWN = 0;
	// The start-up cohort, a late joiner, the top-up, a takeover's return.
	const unsigned char IN_START = 1;
	// The life schedule's rest (LIFE, LIFE_HOURS) was over.
	const unsigned char IN_REST = 2;
	// The two channels' coordinator moved it here from the other channel.
	const unsigned char IN_CHANNEL = 3;
	// A player's companion, in the world with its owner.
	const unsigned char IN_SIDEKICK = 4;

	// Why a session ended.
	// Nobody closed it: the core stopped without saying so, and the next
	// start of that core closed it where it was last seen.
	const unsigned char OUT_UNKNOWN = 0;
	// The life schedule's rest; rest_until says until when.
	const unsigned char OUT_REST = 1;
	// A GM's ban (account.account_block).
	const unsigned char OUT_BAN = 2;
	// Moved to the other channel.
	const unsigned char OUT_CHANNEL = 3;
	// Logged out by a GM's command or by the panel (a takeover, a switch
	// turned off). Also what an engine caller of Despawn without a reason is.
	const unsigned char OUT_GM = 4;
	// A player's companion, gone with its owner or at the owner's word.
	const unsigned char OUT_SIDEKICK = 5;
	// The character's connection went without the manager asking (a kick).
	const unsigned char OUT_DISCONNECT = 6;
	// The core was stopped.
	const unsigned char OUT_STOP = 7;
	// "Koniec gry" (playerbot_retirement.h): the bot sold everything and its
	// character is made new.
	const unsigned char OUT_RETIRE = 8;

	// How long a row is kept, counted from its end.
	const int KEEP_DAYS = 8;
	// seen_at of the open sessions, so a core that dies leaves an end close
	// to the truth; the panel believes an open session this long after it.
	const unsigned int HEARTBEAT_SECONDS = 5 * 60;
}

#endif
