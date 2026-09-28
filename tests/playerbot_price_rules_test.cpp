// playerbot_price_rules.h: Iwakura's Community Patch 5 on prices - the
// inflation compounded, the maximal lines' multiplier, which bonus row prices
// a line. Ours: upstream shipped the header without its test.
//
//   g++ -Wall -Wextra -o /tmp/t tests/playerbot_price_rules_test.cpp && /tmp/t
#include <cassert>
#include <cstdio>

#include "../linux-port/overlays/playerbot/src/game/src/playerbot_price_rules.h"

using namespace playerbot_price_rules;

int main()
{
	// Point 10: x1.05 a step, compounded - two steps are x1.1025, three
	// x1.1576, where the old flat +5% a step made x1.15.
	assert(InflationFactor(0, 5, 1000000) == 10000);
	assert(InflationFactor(-3, 5, 1000000) == 10000);
	assert(InflationFactor(4, 0, 1000000) == 10000);
	assert(InflationFactor(1, 5, 1000000) == 10500);
	assert(InflationFactor(2, 5, 1000000) == 11025);
	assert(InflationFactor(3, 5, 1000000) == 11576);
	// The ceiling is the caller's, and a huge step count never overflows.
	assert(InflationFactor(10, 5, 15000) == 15000);
	assert(InflationFactor(1000000, 5, 99999) == 99999);
	for (long long steps = 1; steps < 40; ++steps)
		assert(InflationFactor(steps, 5, 10000000) > InflationFactor(steps - 1, 5, 10000000));

	// Point 11: two maximal lines x1.7, three x2.5, four x4.0; a fifth asks
	// what four do, and a count under one what none does.
	assert(MaxLinesPercent(-1) == 100 && MaxLinesPercent(0) == 100 && MaxLinesPercent(1) == 100);
	assert(MaxLinesPercent(2) == 170 && MaxLinesPercent(3) == 250 && MaxLinesPercent(4) == 400);
	assert(MaxLinesPercent(5) == 400 && MaxLinesPercent(99) == 400);
	assert(AVERAGE_DAMAGE_MAX_FROM == 40);

	// A line at its top: the world's table, or the top a row names itself.
	assert(!IsMaxLine(0, 2000, 0) && !IsMaxLine(-5, 2000, 0));
	assert(IsMaxLine(2000, 2000, 0) && !IsMaxLine(1500, 2000, 0));
	assert(IsMaxLine(30, 12, 0));      // over the table's top is at it
	assert(IsMaxLine(15, 0, 15) && !IsMaxLine(10, 0, 15));
	assert(!IsMaxLine(10, 0, 0));      // no table line and no row: never

	// Which row prices a line. Regeneration has both: 1.3 | 1.1 at this
	// table's 12, 1.5 | 1.2 at the other table's 30.
	const BonusRow slot = { 130, 110, 0 };
	const BonusRow top = { 150, 120, 30 };
	assert(LinePercent(0, 12, &slot, &top) == 100);
	assert(LinePercent(30, 12, &slot, &top) == 150);   // its own top
	assert(LinePercent(20, 12, &slot, &top) == 130);   // over the table's top, not the row's
	assert(LinePercent(12, 12, &slot, &top) == 130);
	assert(LinePercent(8, 12, &slot, &top) == 110);
	// A line no slot row prices takes the new row's "any other value".
	const BonusRow elemental = { 190, 150, 15 };
	assert(LinePercent(15, 0, nullptr, &elemental) == 190);
	assert(LinePercent(10, 0, nullptr, &elemental) == 150);
	assert(LinePercent(10, 0, nullptr, nullptr) == 100);
	assert(LinePercent(10, 12, &slot, nullptr) == 110);

	// The lines compound and stop at the ceiling.
	long long product = 100;
	product = CompoundLinePercent(product, 150, 10000);
	assert(product == 150);
	product = CompoundLinePercent(product, 200, 10000);
	assert(product == 300);
	assert(CompoundLinePercent(9000, 200, 10000) == 10000);

	// The piece: the product, then the maximal lines' multiplier over it.
	assert(PiecePremiumPercent(100, 0) == 0);
	assert(PiecePremiumPercent(200, 1) == 100);
	assert(PiecePremiumPercent(200, 2) == 240);    // x2.0 x1.7 = x3.4
	assert(PiecePremiumPercent(10000, 4) == 39900); // on the final price, over our ceiling

	std::printf("playerbot_price_rules: all checks passed\n");
	return 0;
}
