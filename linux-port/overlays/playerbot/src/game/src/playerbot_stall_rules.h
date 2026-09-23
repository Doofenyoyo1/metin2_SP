#ifndef PLAYERBOT_STALL_RULES_H
#define PLAYERBOT_STALL_RULES_H
// What a counter may take out of a kind the bot keeps by count - a skill book
// of its own skill, the soul stone - as pure arithmetic. No engine types;
// unit-tested in tests/playerbot_stall_rules_test.cpp, which drives these
// against a bag laid out the way the engine lays one out.
//
// A kind is counted over the whole bag, never by the cells in front of one
// stack. Counting only the cells in front kept a bot's one stack whole
// whatever it held: a single stack of ten soul stones against a keep of three
// was never goods, and the skill books were counted by row, so "keep twelve"
// kept twelve stacks of up to ten. The keep is also the buyer's need
// (GetPlayerBotProgressionNeed), so a counter never sells what its keeper
// would walk to the market to buy back.
namespace playerbot_stall_rules {

// Units of a kind over its keep.
inline int SpareUnits(int kindTotal, int keep)
{
	return kindTotal > keep ? kindTotal - keep : 0;
}

// Whether a stack is goods: it holds at least one unit over the keep. The keep
// is spent on the units in the cells before it first, as the engine's own
// cell order puts them.
inline bool HoldsSpare(int unitsAhead, int stackCount, int keep)
{
	return stackCount > 0 && unitsAhead + stackCount > keep;
}

// How many units of this stack make one counter line: the kind's spare, up to
// a line and up to the stack. Zero when nothing can go; the whole stack when
// the answer is its count; otherwise the rest stays in the stack it was cut
// from.
inline int LineTake(int stackCount, int kindTotal, int keep, int lineUnits)
{
	int take = SpareUnits(kindTotal, keep);
	if (take > lineUnits)
		take = lineUnits;
	if (take > stackCount)
		take = stackCount;
	return take > 0 ? take : 0;
}

// The classic stall lists a stack whole: it may, when what stays behind in the
// bag - the kind less what this counter already carries and this stack - still
// holds the keep.
inline bool MayListWhole(int kindTotal, int listedSoFar, int stackCount, int keep)
{
	return stackCount > 0 && kindTotal - listedSoFar - stackCount >= keep;
}

}
#endif
