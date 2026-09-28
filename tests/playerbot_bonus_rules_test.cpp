// playerbot_bonus_rules.h: which stone a bot puts on which piece next
// (Iwakura's Community Patch 5, points 5 and 9). Ours: upstream shipped the
// header without its test.
//
//   g++ -Wall -Wextra -o /tmp/t tests/playerbot_bonus_rules_test.cpp && /tmp/t
#include <cassert>
#include <cstdio>

#include "../linux-port/overlays/playerbot/src/game/src/playerbot_bonus_rules.h"

using namespace playerbot_bonus_rules;

namespace
{
	const TLimits LIMITS = { 4, 3 };

	TPiece Piece(int lines, EPlain plain)
	{
		TPiece p = {};
		p.lines = lines;
		p.lineRolls = true;
		p.plain = plain;
		p.greenFits = false;
		p.marbleAllowed = false;
		p.wantsPlainChange = true;
		p.wantsGreenChange = true;
		p.pinned = false;
		p.goods = false;
		p.greenRank = -1;
		return p;
	}

	TBag Bag(bool greenAdd, bool greenChange, bool plainAdd, bool plainChange, bool marble)
	{
		TBag b = { greenAdd, greenChange, plainAdd, plainChange, marble };
		return b;
	}

	bool Is(const TStepChoice& c, EStep step, EStone stone)
	{
		return c.step == step && c.stone == stone;
	}
}

int main()
{
	const unsigned OPEN = REST_ADD | REST_CHANGE;

	// An empty line first; a green stone where the piece takes one.
	{
		TPiece p = Piece(2, PLAIN_CATEGORY);
		assert(Is(StepFor(p, Bag(false, false, true, true, false), LIMITS, OPEN, false), STEP_ADD, STONE_PLAIN));
		p.greenFits = true;
		assert(Is(StepFor(p, Bag(true, false, true, true, false), LIMITS, OPEN, false), STEP_ADD, STONE_GREEN));
		// The green round leaves the ordinary add to the ordinary pass.
		p.greenFits = false;
		assert(Is(StepFor(p, Bag(false, false, true, false, false), LIMITS, OPEN, true), STEP_NONE, STONE_NONE));
		// No line left to roll: an add is spent for nothing.
		p.lineRolls = false;
		assert(!Is(StepFor(p, Bag(false, false, true, false, false), LIMITS, OPEN, false), STEP_ADD, STONE_PLAIN));
	}

	// The fifth line by the marble, only on a worn category piece of four.
	{
		TPiece p = Piece(4, PLAIN_CATEGORY);
		const TBag marble = Bag(false, false, false, false, true);
		assert(Is(StepFor(p, marble, LIMITS, OPEN, false), STEP_NONE, STONE_NONE));
		p.marbleAllowed = true;
		assert(Is(StepFor(p, marble, LIMITS, OPEN, false), STEP_MARBLE, STONE_MARBLE));
		assert(Is(StepFor(p, marble, LIMITS, OPEN, true), STEP_NONE, STONE_NONE));
		p.plain = PLAIN_REST;
		assert(Is(StepFor(p, marble, LIMITS, OPEN, false), STEP_NONE, STONE_NONE));
	}

	// The ordinary change waits for three lines; the green one does not.
	{
		TPiece p = Piece(2, PLAIN_CATEGORY);
		p.lineRolls = false;
		assert(Is(StepFor(p, Bag(false, false, false, true, false), LIMITS, OPEN, false), STEP_NONE, STONE_NONE));
		p.lines = 3;
		assert(Is(StepFor(p, Bag(false, false, false, true, false), LIMITS, OPEN, false), STEP_CHANGE, STONE_PLAIN));
		TPiece g = Piece(1, PLAIN_NONE);
		g.lineRolls = false;
		g.greenFits = true;
		assert(Is(StepFor(g, Bag(false, true, false, false, false), LIMITS, OPEN, true), STEP_CHANGE, STONE_GREEN));
		// Nothing mixed on a piece with no line, nor on the owner's pin.
		g.lines = 0;
		assert(Is(StepFor(g, Bag(false, true, false, false, false), LIMITS, OPEN, true), STEP_NONE, STONE_NONE));
		p.pinned = true;
		assert(Is(StepFor(p, Bag(false, true, false, true, false), LIMITS, OPEN, false), STEP_NONE, STONE_NONE));
		// A piece that no longer wants a change is left alone.
		p.pinned = false;
		p.wantsPlainChange = false;
		assert(Is(StepFor(p, Bag(false, false, false, true, false), LIMITS, OPEN, false), STEP_NONE, STONE_NONE));
	}

	// The rest of the gear takes a kind of ordinary stone only while no
	// category piece can use it now - by kind.
	{
		TPiece pieces[2] = { Piece(2, PLAIN_CATEGORY), Piece(0, PLAIN_REST) };
		const TBag bag = Bag(false, false, true, true, false);
		assert(RestOpen(pieces, 2, bag, LIMITS) == (unsigned)REST_CHANGE);
		pieces[0].lines = 4;
		pieces[0].lineRolls = true;
		// Four lines: no add fits it any more, and it may still be mixed.
		assert(RestOpen(pieces, 2, bag, LIMITS) == (unsigned)REST_ADD);
		pieces[0].wantsPlainChange = false;
		assert(RestOpen(pieces, 2, bag, LIMITS) == OPEN);
		// Goods are never asked: the gear a bot fights in comes first.
		TPiece goods[1] = { Piece(1, PLAIN_CATEGORY) };
		goods[0].goods = true;
		assert(RestOpen(goods, 1, bag, LIMITS) == OPEN);
		// A rest piece takes the add only while the rest is open to it.
		TPiece rest = Piece(1, PLAIN_REST);
		assert(Is(StepFor(rest, bag, LIMITS, 0, false), STEP_NONE, STONE_NONE));
		assert(Is(StepFor(rest, bag, LIMITS, REST_ADD, false), STEP_ADD, STONE_PLAIN));
	}

	// The pick: the green round first, armour before weapon; then the piece
	// being worked; then the first that can take a line, then a change.
	{
		TPiece pieces[3] = { Piece(4, PLAIN_CATEGORY), Piece(1, PLAIN_CATEGORY), Piece(2, PLAIN_NONE) };
		pieces[0].lineRolls = false;
		pieces[2].greenFits = true;
		pieces[2].greenRank = 1;
		TStepChoice c;
		bool green = false;
		// A green add in the bag goes to the weapon of the green round.
		int i = Pick(pieces, 3, Bag(true, false, true, true, false), LIMITS, OPEN, -1, c, green);
		assert(i == 2 && green && Is(c, STEP_ADD, STONE_GREEN));
		// The armour ranks before it.
		pieces[1].greenFits = true;
		pieces[1].greenRank = 0;
		i = Pick(pieces, 3, Bag(true, false, true, true, false), LIMITS, OPEN, -1, c, green);
		assert(i == 1 && green);
		// With no green stone: filling before mixing, whatever the list order.
		i = Pick(pieces, 3, Bag(false, false, true, true, false), LIMITS, OPEN, -1, c, green);
		assert(i == 1 && !green && Is(c, STEP_ADD, STONE_PLAIN));
		// The piece being worked keeps the stones while one fits it.
		i = Pick(pieces, 3, Bag(false, false, true, true, false), LIMITS, OPEN, 0, c, green);
		assert(i == 0 && Is(c, STEP_CHANGE, STONE_PLAIN));
		// Nothing fits anything.
		i = Pick(pieces, 3, Bag(false, false, false, false, false), LIMITS, OPEN, -1, c, green);
		assert(i == -1 && c.step == STEP_NONE && !green);
	}

	assert(ChangeReachesFinish(false, false) && ChangeReachesFinish(true, true) && !ChangeReachesFinish(true, false));
	assert(!WantsChange(true, true, 0, 100));
	assert(WantsChange(false, true, 500, 100));
	assert(WantsChange(false, false, 50, 100) && !WantsChange(false, false, 100, 100));
	assert(PastGreenBand(41, 40, false, false));
	assert(!PastGreenBand(40, 40, false, false));
	assert(!PastGreenBand(55, 40, true, false) && !PastGreenBand(55, 40, false, true));

	std::printf("playerbot_bonus_rules: all checks passed\n");
	return 0;
}
