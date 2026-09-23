#ifndef PLAYERBOT_MONKEY_POLICY_H
#define PLAYERBOT_MONKEY_POLICY_H

#include <cstdint>
#include <map>

// How long a bot stays in one Monkey Dungeon chamber, and the one number that
// governs both ways of leaving it.
//
// The dungeon is eleven chambers joined only by GOTO NPCs, and warp_npc_event
// moves anyone within three hundred units of one, twice a second. A bot that
// comes through a door is put down beside the door that leads straight back,
// and two different things sent it through that door again. One was the
// engine: a bot that stopped there to fight was moved by warp_npc_event
// without deciding anything. The other was the AI: the route was cleared with
// its goal kept, so the next move towards the old goal - or towards the monster
// it had been fighting - was routed back through the same door. Of 87 returns
// to the entrance chamber within fifteen seconds of arriving, 52 had no route
// planned in between (the engine) and 35 had one (the AI). The "whole dungeon
// run in one line" that players photographed was this: 905 crossings on map
// 108, nearly all of them 0<->7 and 0<->1, and almost nothing past the first
// door.
//
// One number closes the engine half: char.cpp refuses to move a bot through a
// GOTO door for kChamberDwellMs after the last door moved it. The AI half has
// to close with it, not after it - UpdatePlayerBotMonkeyChamber drops the goal
// and the target the bot brought from the old room, and MovePlayerBot does not
// route through a door inside the dwell. Without that second half 86% of the
// routes through a door were planned inside the dwell, and each would now end
// with a bot waiting at a door that will not open. The AI's clock starts on the
// bot's own tick, after the engine's, so a chosen crossing always finds the
// block expired and a bounce always finds it standing.
//
// Included from an engine translation unit (char.cpp) as well as the overlay,
// so everything is inline and nothing of the engine is dragged in - the same
// shape as playerbot_party_policy.h and playerbot_pvp_policy.h.
namespace playerbot_monkey {

// Forty-five seconds - and the one number here worth re-measuring before it
// moves. It trades two things that pull opposite ways, counted over the 905
// visits and the engine returns of 14 September before any of this. A bot
// leaves the moment its medal drops, so the median visit is 75 s: a shorter
// dwell lets more of that visit be spent crossing, a longer one blocks more
// of the bounces.
//
//   dwell   visits with >=1 crossing   >=2    engine bounces caught
//   30 s          79%                   56%          65%
//   45 s          68%                   43%          80%
//   90 s          43%                   19%          80%
//   120 s         31%                   11%          80%
//
// Forty-five is the knee: every bounce ninety caught, and twice as many
// visits reaching a second chamber. At ninety a twenty-minute window after the
// engine half went in showed no crossing at all. The four minutes before that
// assumed a thirty-minute visit that never happened. The bounce sample is
// small - twenty returns with no route between - so read the 80% as a shape.
const uint32_t kChamberDwellMs = 45000;

inline std::map<uint32_t, uint32_t> gotoCrossedAt;

// Called by the engine on the pass a GOTO door moves a bot.
inline void NoteGotoCrossing(uint32_t botPid, uint32_t now)
{
	if (botPid)
		gotoCrossedAt[botPid] = now;
}

// True while a GOTO door has to leave this bot where it stands.
inline bool IsGotoCrossingBlocked(uint32_t botPid, uint32_t now)
{
	std::map<uint32_t, uint32_t>::iterator it = gotoCrossedAt.find(botPid);
	if (it == gotoCrossedAt.end())
		return false;
	if (now - it->second >= kChamberDwellMs)
	{
		gotoCrossedAt.erase(it);
		return false;
	}
	return true;
}

inline void Forget(uint32_t botPid)
{
	gotoCrossedAt.erase(botPid);
}

} // namespace playerbot_monkey

#endif
