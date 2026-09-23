#ifndef PLAYERBOT_PARTY_POLICY_H
#define PLAYERBOT_PARTY_POLICY_H

#include <cstdint>
#include <map>

// A player's party invitation, kept until the bot's next tick answers it.
//
// CHARACTER::PartyInvite ends by sending HEADER_GC_PARTY_INVITE to the
// invitee's descriptor. A bot has a descriptor and no client behind it, so the
// packet goes nowhere, nobody presses Accept, and the ten-second
// party_invite_event expires: inviting a bot did exactly nothing, silently.
//
// The engine cannot answer for the bot either, because accepting is a method on
// the LEADER (leader->PartyInviteAccept(guest)) and it has to run while the
// invite event is still alive. So the engine's side only records that a bot was
// asked - playerbotify.py puts that call into PartyInvite - and the bot's own
// tick does the accepting, which is where every other bot decision already
// lives.
//
// This header is included from an engine translation unit as well as from the
// overlay, so everything in it is inline and it drags in nothing of the engine.
// Same shape as playerbot_offline_policy.h, for the same reason.
namespace playerbot_party {

struct Invite
{
	uint32_t leaderPid;
	uint32_t notedAt;
};

inline std::map<uint32_t, Invite> invites;

// Called from the engine, for a bot that was just invited. The newest
// invitation wins: a player who asks twice, or two players in a row, should not
// have the bot answer a request that is already stale.
inline void NoteInvite(uint32_t leaderPid, uint32_t botPid, uint32_t now)
{
	if (!leaderPid || !botPid)
		return;
	Invite& invite = invites[botPid];
	invite.leaderPid = leaderPid;
	invite.notedAt = now;
}

// Called from the bot's tick: takes the invitation away and answers it once.
// Taking rather than reading is deliberate - a refused join (full party, level
// gap, another kingdom) must not be retried every tick for ever.
inline bool TakeInvite(uint32_t botPid, uint32_t& leaderPid, uint32_t& notedAt)
{
	std::map<uint32_t, Invite>::iterator it = invites.find(botPid);
	if (it == invites.end())
		return false;
	leaderPid = it->second.leaderPid;
	notedAt = it->second.notedAt;
	invites.erase(it);
	return true;
}

inline void Forget(uint32_t botPid)
{
	invites.erase(botPid);
}

} // namespace playerbot_party

#endif
