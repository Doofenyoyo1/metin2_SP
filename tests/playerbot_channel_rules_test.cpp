// Unit tests for playerbot_channel_rules.h - the second channel's partition.
//
//   docker run --rm -v "$(pwd -W)":/w -w /w gcc:13 bash -c
//     "g++ -Wall -Wextra -o /tmp/t tests/playerbot_channel_rules_test.cpp && /tmp/t"
#include <cassert>
#include <cstdio>

#include "../linux-port/overlays/playerbot/src/game/src/playerbot_channel_rules.h"

using namespace playerbot_channel_rules;

int main()
{
	// Switched off, every bot lives on the first channel and nothing else
	// starts any: a world that never asked for a second channel is unchanged.
	for (unsigned int pid = 4; pid < 2504; ++pid)
		assert(ChannelOf(pid, false, 40, false) == 1);
	assert(ShareOfTotal(1500, false, 40, 1, 1000) == 1500);
	assert(ShareOfTotal(1500, false, 40, 2, 1000) == 0);

	// Switched on, the spread matches the share across the seed's whole range
	// - and inside each kingdom's run of pids, which is contiguous.
	{
		int second = 0;
		for (unsigned int pid = 4; pid < 2504; ++pid)
			second += ChannelOf(pid, true, 40, false) == 2 ? 1 : 0;
		assert(second > 2500 * 36 / 100 && second < 2500 * 44 / 100);
		int shinsoo = 0;
		for (unsigned int pid = 1504; pid < 2004; ++pid)
			shinsoo += ChannelOf(pid, true, 40, false) == 2 ? 1 : 0;
		assert(shinsoo > 500 * 33 / 100 && shinsoo < 500 * 47 / 100);
	}

	// The same pid gets the same answer every time - every core asks.
	for (unsigned int pid = 4; pid < 2504; ++pid)
		assert(ChannelOf(pid, true, 40, false) == ChannelOf(pid, true, 40, false));

	// A bot that has kept a shop lives on the first channel whatever the spread.
	for (unsigned int pid = 4; pid < 2504; ++pid)
		assert(ChannelOf(pid, true, 90, true) == 1);

	// The share is clamped: the first channel always keeps some bots, and a
	// share of nothing is not a second channel.
	assert(ClampShare(0) == CH2_SHARE_MIN);
	assert(ClampShare(100) == CH2_SHARE_MAX);
	assert(ClampShare(40) == 40);

	// The two channels add up to the operator's number.
	assert(ShareOfTotal(1500, true, 40, 1, 1000) == 900);
	assert(ShareOfTotal(1500, true, 40, 2, 1000) == 600);
	assert(ShareOfTotal(1501, true, 40, 1, 1000) + ShareOfTotal(1501, true, 40, 2, 1000) == 1501);
	// The second channel short of identities: it takes what it has and the
	// first channel takes the rest.
	assert(ShareOfTotal(1500, true, 40, 2, 250) == 250);
	assert(ShareOfTotal(1500, true, 40, 1, 250) == 1250);
	// No bots on a third channel, ever; nothing from nothing.
	assert(ShareOfTotal(1500, true, 40, 3, 1000) == 0);
	assert(ShareOfTotal(0, true, 40, 1, 1000) == 0);

	// The moves. The slider's 40 is SIZOWSKI's own 60 and 50; the target never
	// falls under the slider's minimum.
	assert(ShopChannelCapPercent(40) == 60 && ShopChannelTargetPercent(40) == 50);
	assert(ShopChannelCapPercent(20) == 80 && ShopChannelTargetPercent(20) == 70);
	assert(ShopChannelCapPercent(90) == 10 && ShopChannelTargetPercent(90) == CH2_SHARE_MIN);
	assert(ShopChannelCapPercent(5) == 90);

	// Nobody waiting: a shop channel over its target eases back, at most two
	// percent of the bots a gate and never under the target.
	{
		TChannelMovePlan p = PlanChannelMoves(1000, 600, 0, 60, 50);
		assert(p.kind == MOVE_DRAIN && p.count == 20 && !p.overCap);
		p = PlanChannelMoves(1000, 505, 0, 60, 50);
		assert(p.kind == MOVE_DRAIN && p.count == 5);
		p = PlanChannelMoves(1000, 500, 0, 60, 50);
		assert(p.kind == MOVE_NONE);
		p = PlanChannelMoves(10, 9, 0, 60, 50);
		assert(p.kind == MOVE_DRAIN && p.count == 1);
	}
	// Over the cap with nobody waiting: back to the cap and no further, with
	// anybody not pinned - one over the cap is one bot, not a drain's worth.
	{
		TChannelMovePlan p = PlanChannelMoves(1099, 660, 0, 60, 50);
		assert(p.kind == MOVE_DRAIN && p.count == 1 && p.overCap);
		p = PlanChannelMoves(1000, 650, 0, 60, 50);
		assert(p.kind == MOVE_DRAIN && p.count == 20 && p.overCap);
		p = PlanChannelMoves(1000, 605, 0, 60, 50);
		assert(p.kind == MOVE_DRAIN && p.count == 5 && p.overCap);
	}
	// Somebody waiting with room under the cap: straight in, as many as fit.
	{
		TChannelMovePlan p = PlanChannelMoves(1000, 550, 80, 60, 50);
		assert(p.kind == MOVE_PROMOTE && p.count == 50);
		p = PlanChannelMoves(1000, 550, 7, 60, 50);
		assert(p.kind == MOVE_PROMOTE && p.count == 7);
	}
	// At the cap: one for one, three percent of the bots a gate at most.
	{
		TChannelMovePlan p = PlanChannelMoves(1000, 600, 80, 60, 50);
		assert(p.kind == MOVE_SWAP && p.count == 30 && p.extraOut == 0);
		p = PlanChannelMoves(1000, 640, 5, 60, 50);
		assert(p.kind == MOVE_SWAP && p.count == 5);
		p = PlanChannelMoves(20, 12, 4, 60, 50);
		assert(p.kind == MOVE_SWAP && p.count == 1 && p.extraOut == 0);
	}
	// Over the cap with somebody waiting - the slider moved, or a cohort
	// pinned to the shop channel holds places there: the swap sends out up to
	// a drain's worth more than it brings in, and never more than the excess.
	{
		TChannelMovePlan p = PlanChannelMoves(1000, 640, 5, 60, 50);
		assert(p.kind == MOVE_SWAP && p.count == 5 && p.extraOut == 20);
		p = PlanChannelMoves(1000, 605, 80, 60, 50);
		assert(p.kind == MOVE_SWAP && p.count == 30 && p.extraOut == 5);
		p = PlanChannelMoves(1098, 701, 38, 60, 50);
		assert(p.kind == MOVE_SWAP && p.count == 33 && p.extraOut == 21);
		p = PlanChannelMoves(20, 14, 4, 60, 50);
		assert(p.kind == MOVE_SWAP && p.count == 1 && p.extraOut == 1);
	}
	// Nobody plays, nothing moves; a target over the cap is the cap.
	assert(PlanChannelMoves(0, 0, 5, 60, 50).kind == MOVE_NONE);
	{
		TChannelMovePlan p = PlanChannelMoves(1000, 650, 0, 60, 90);
		assert(p.kind == MOVE_DRAIN && p.count == 20);
	}
	// The cost of moving a bot out of the shop channel.
	assert(MoveCost(false, false, false, false) == 0);
	assert(MoveCost(true, false, false, false) == 1);
	assert(MoveCost(false, false, false, true) == 1);
	assert(MoveCost(false, true, false, false) == 2);
	assert(MoveCost(true, true, false, false) == 3);
	assert(MoveCost(true, true, false, true) == 4);
	assert(MoveCost(true, true, true, true) == MOVE_COST_PINNED);
	// The drain takes what costs one at most: an idle bot in a village with
	// no stand is fair game, one with a stand never is.
	assert(MoveCost(false, false, false, true) <= 1 && MoveCost(false, true, false, true) > 1);

	std::printf("playerbot_channel_rules_test: OK\n");
	return 0;
}
