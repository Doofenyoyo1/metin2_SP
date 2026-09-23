#ifndef __INC_PLAYERBOT_CHANNEL_RULES_H__
#define __INC_PLAYERBOT_CHANNEL_RULES_H__

// The second channel as pure policy: which channel a registered bot lives on,
// and how much of the operator's number each channel starts. No engine types,
// unit-tested (tests/playerbot_channel_rules_test.cpp).
//
// A bot must live on exactly one channel. Every core loads the registry for
// itself and nothing tells one channel what another decided, so the answer has
// to come out the same on every core from the same inputs: the pid, the switch
// and the share from the container's environment, and whether the bot is
// pinned. Two cores that disagreed about one pid would both log it in - the
// db core serves a bot's load to anyone who asks, P2P_MANAGER overwrites its
// entry without a word, and the two copies' saves would duplicate every item
// either of them sold ("a same pid on two cores", read in the engine on 18
// September before any of this was written).
//
// A pinned bot is one that has kept an offline shop. Shops are the first
// channel's alone (the operator's rule: "wszystkie sklepy tylko na ch1"), a
// shop's entity stands on the channel it was opened on, and a keeper on the
// other channel could never serve its counter or take its yang out again - so
// every bot that has ever owned a shop lives on the first channel for good.
// The pins only grow (player.playerbot_channel_pin), which is what keeps the
// answer the same on a core restarted in the middle of a session: nobody on
// the second channel can open a shop, so no second-channel bot can become
// pinned while the other channel's cores are running.
//
// Only the first two channels carry bots. A third or fourth is players'.
namespace playerbot_channel_rules
{
	// The share of the population on the second channel, clamped: the first
	// channel is where the shops are, so it always keeps some.
	const int CH2_SHARE_MIN = 10;
	const int CH2_SHARE_MAX = 90;
	const int CH2_SHARE_DEFAULT = 40;

	inline int ClampShare(int percent)
	{
		if (percent < CH2_SHARE_MIN)
			return CH2_SHARE_MIN;
		if (percent > CH2_SHARE_MAX)
			return CH2_SHARE_MAX;
		return percent;
	}

	// A stable spread of pids over 0..99. Consecutive pids - the seed creates
	// them in runs by kingdom - must not land in blocks, so the pid is mixed
	// (the finaliser of MurmurHash3) before it is reduced.
	inline unsigned int SpreadPercent(unsigned int pid)
	{
		unsigned int h = pid ^ 0x43483232u;
		h ^= h >> 16;
		h *= 0x85ebca6bu;
		h ^= h >> 13;
		h *= 0xc2b2ae35u;
		h ^= h >> 16;
		return h % 100u;
	}

	// The channel a registered bot lives on.
	inline int ChannelOf(unsigned int pid, bool ch2Enabled, int sharePercent, bool pinned)
	{
		if (!ch2Enabled || pinned)
			return 1;
		return SpreadPercent(pid) < (unsigned int)ClampShare(sharePercent) ? 2 : 1;
	}

	// How many of the operator's number `channel` starts. The second channel
	// takes its share, never more than the identities it has; the first takes
	// the rest, so the two add up to the number whenever the identities allow.
	// With the switch off the first channel takes all of it and no other
	// channel takes anything - which is also what a third channel always gets.
	inline int ShareOfTotal(int total, bool ch2Enabled, int sharePercent, int channel,
			int secondChannelIdentities)
	{
		if (total <= 0 || channel < 1 || channel > 2)
			return 0;
		if (!ch2Enabled)
			return channel == 1 ? total : 0;
		int second = total * ClampShare(sharePercent) / 100;
		if (secondChannelIdentities >= 0 && second > secondChannelIdentities)
			second = secondChannelIdentities;
		return channel == 2 ? second : total - second;
	}

	// ------------------------------------------------------------------
	// The two channels with moves (mt2009, SIZOWSKI's design of 18-19
	// September). The pins above had a flaw that only a world which has
	// played shows: nearly every bot keeps an offline shop there - 2 404
	// shops for 2 500 bots on one player's world - so nearly every bot was
	// pinned to the first channel and the second carried 42 ("% botow na
	// channelach nie dziala poprawnie", Xewi and Mkls, 19 September).
	//
	// With the second channel on, a bot's channel is its row of
	// common.playerbot_channel_assignment: one row a pid, so one channel a
	// pid. A bot on the second channel with business at a shop - its own
	// stand to serve or renew, a stand to open, another bot's counter to buy
	// from - asks to be moved to the shop channel, and the coordinator (the
	// shop channel's core that hosts Joan) moves it: straight in while the
	// shop channel is under its cap, one for one against a free bot of the
	// shop channel at the cap, and when nobody waits it eases the shop
	// channel back to its target. A move is a row changed and nothing else:
	// the old core despawns the bot when it reads the change, the new one
	// spawns it once the row's ready time has passed and the P2P table no
	// longer knows it - the bot is never on two cores at once.
	// ------------------------------------------------------------------
	const int SHOP_CHANNEL = 1;

	// The shop channel's cap and the target it eases back to, as shares of
	// the bots that play. The operator's slider is the second channel's
	// share, so the shop channel never holds more than the rest of it and
	// rests ten points under that: at the slider's 40 that is SIZOWSKI's own
	// 60 and 50. Never under the slider's own minimum.
	inline int ShopChannelCapPercent(int sharePercent)
	{
		return 100 - ClampShare(sharePercent);
	}
	inline int ShopChannelTargetPercent(int sharePercent)
	{
		const int target = ShopChannelCapPercent(sharePercent) - 10;
		return target < CH2_SHARE_MIN ? CH2_SHARE_MIN : target;
	}

	// What it costs to move a bot out of the shop channel - the order the
	// coordinator picks candidates in. Nothing to lose is 0; standing in a
	// village +1, because that is where the market is and where players look;
	// an errand under way (a fight, a trip, a town visit, a purchase) +1; a
	// live stand +2 (its owner asks to come back for the next service, so
	// easing the shop channel back never takes one). A bot that must not
	// vanish - a shop operation in flight, a service visit, a player's party,
	// a war, a dungeon, a tower raid, a duel, the medal droppers' cohort - is
	// pinned where it is. SIZOWSKI's design kept every bot in a village out
	// of the swap altogether, which on his frontier world was a few; on a
	// world that is young, or whose bots live in their villages, it was
	// everybody - 654 of 693 on m2zip, and the swaps ran at six a gate while
	// eighty waited.
	const int MOVE_COST_PINNED = 9;
	inline int MoveCost(bool busy, bool liveStand, bool pinned, bool inVillage)
	{
		if (pinned)
			return MOVE_COST_PINNED;
		return (inVillage ? 1 : 0) + (busy ? 1 : 0) + (liveStand ? 2 : 0);
	}

	// One step of the coordinator, decided from a census: how many bots play
	// (seen in the last half minute, both channels), how many of those are on
	// the shop channel, and how many requests have stood long enough.
	enum EChannelMove { MOVE_NONE = 0, MOVE_DRAIN, MOVE_PROMOTE, MOVE_SWAP };
	// count: how many move (a swap: how many each way); extraOut: a swap
	// over the cap sends this many more out than it brings in; overCap: a
	// drain that brings the shop channel back to its cap, which anybody not
	// pinned may be taken for - a drain below the cap takes only bots with
	// no live stand.
	struct TChannelMovePlan { int kind; unsigned int count; unsigned int extraOut; bool overCap; };
	// A swap batch moves at most this share of the bots each way (a relog
	// storm shows as a longer tick), easing back at most this share a gate.
	const unsigned int MOVE_BATCH_PERCENT = 3;
	const unsigned int MOVE_DRAIN_PERCENT = 2;

	inline TChannelMovePlan PlanChannelMoves(unsigned int total, unsigned int onShopChannel,
			unsigned int waiting, int capPercent, int targetPercent)
	{
		TChannelMovePlan plan = { MOVE_NONE, 0, 0, false };
		if (total == 0)
			return plan;
		if (targetPercent > capPercent)
			targetPercent = capPercent;
		const unsigned int cap = total * (unsigned int)capPercent / 100U;
		const unsigned int target = total * (unsigned int)targetPercent / 100U;
		unsigned int drainMost = total * MOVE_DRAIN_PERCENT / 100U;
		if (drainMost < 1)
			drainMost = 1;
		if (waiting == 0)
		{
			// Over the cap: back to the cap and no further this gate - a
			// channel one over its cap lost twenty-one bots to the first
			// build, which drained towards the target at any cost.
			if (onShopChannel > cap)
			{
				plan.kind = MOVE_DRAIN;
				plan.count = onShopChannel - cap < drainMost ? onShopChannel - cap : drainMost;
				plan.overCap = true;
			}
			else if (onShopChannel > target)
			{
				plan.kind = MOVE_DRAIN;
				plan.count = onShopChannel - target < drainMost ? onShopChannel - target : drainMost;
			}
			return plan;
		}
		const unsigned int room = cap > onShopChannel ? cap - onShopChannel : 0;
		if (room > 0)
		{
			plan.kind = MOVE_PROMOTE;
			plan.count = waiting < room ? waiting : room;
			return plan;
		}
		unsigned int batch = (total * MOVE_BATCH_PERCENT + 99U) / 100U;
		if (batch < 1)
			batch = 1;
		plan.kind = MOVE_SWAP;
		plan.count = waiting < batch ? waiting : batch;
		// Over the cap - the slider moved, or bots pinned to the shop channel
		// hold places there - a swap alone would keep the shop channel where it
		// is for as long as anybody asks, and a world with shops always asks.
		// The swap sends a drain's worth more out than it brings in.
		if (onShopChannel > cap)
			plan.extraOut = onShopChannel - cap < drainMost ? onShopChannel - cap : drainMost;
		return plan;
	}
}

#endif
