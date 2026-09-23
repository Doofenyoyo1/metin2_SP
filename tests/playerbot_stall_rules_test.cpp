// What a counter may take out of the goods a bot keeps by count - a skill
// book of its own skill, the soul stone (playerbot_stall_rules.h) - checked
// against a bag laid out the way the engine lays one out.
// Build: g++ -std=c++17 -Wall -Wextra -I linux-port/overlays/playerbot/src/game/src tests/playerbot_stall_rules_test.cpp
#include "playerbot_stall_rules.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <vector>
using namespace playerbot_stall_rules;

namespace {

// Kinds in these bags: a skill of the bot's own build, another class's skill,
// the soul stone.
enum { EMPTY = 0, OWN_BOOK = 1, OTHER_BOOK = 2, STONE = 3 };
const int STACK_MAX = 10;  // 50300 and 50513 on mt2009 (item_proto stack)

struct Stack { int kind; int count; };

// Cells in order, one stack each. What is cut off a stack goes to the first
// empty cell, which is what GetEmptyInventory hands BotOfflinePrepareLine -
// often a cell in front of every other stack of the kind.
struct Bag {
	std::vector<Stack> cells;
	explicit Bag(int n) : cells(n, Stack{EMPTY, 0}) {}
	void put(int cell, int kind, int count) { cells[cell] = Stack{kind, count}; }
	int total(int kind) const {
		int n = 0;
		for (const Stack& s : cells) if (s.kind == kind) n += s.count;
		return n;
	}
	int ahead(int kind, int cell) const {
		int n = 0;
		for (int c = 0; c < cell; ++c) if (cells[c].kind == kind) n += cells[c].count;
		return n;
	}
	int firstEmpty() const {
		for (int c = 0; c < (int)cells.size(); ++c) if (cells[c].kind == EMPTY) return c;
		return -1;
	}
};

struct World {
	Bag bag;
	std::map<int, int> keep;
	std::map<int, std::vector<int> > counter;  // kind -> the lines, in units
	int cap;
	World(int cells, int linesCap) : bag(cells), cap(linesCap) {}
	int onCounter(int kind) const {
		std::map<int, std::vector<int> >::const_iterator it = counter.find(kind);
		int n = 0;
		if (it != counter.end()) for (int u : it->second) n += u;
		return n;
	}
};

// One service visit, the way BotOfflinePrepareVisitLine and the add loop run
// for these goods: the scorer's goods are the stacks holding a unit over the
// keep (HoldsSpare, ahead counted in units), in cell order; a kind with `cap`
// lines standing is passed over; the first stack that gives a line gives it -
// the whole stack when LineTake says all of it, else a cut into the first
// empty cell, which a bag with no room cannot make. The line leaves the bag.
// Returns the units listed, 0 when nothing went up.
int Visit(World& w, bool roomToCut)
{
	for (int cell = 0; cell < (int)w.bag.cells.size(); ++cell)
	{
		const Stack s = w.bag.cells[cell];
		if (s.kind == EMPTY)
			continue;
		const int keep = w.keep[s.kind];
		if (!HoldsSpare(w.bag.ahead(s.kind, cell), s.count, keep))
			continue;
		if ((int)w.counter[s.kind].size() >= w.cap)
			continue;
		const int take = LineTake(s.count, w.bag.total(s.kind), keep, 1);
		if (take <= 0)
			continue;
		if (take < s.count)
		{
			const int to = w.bag.firstEmpty();
			if (!roomToCut || to < 0)
				continue;
			w.bag.cells[cell].count -= take;
			w.bag.put(to, s.kind, take);
			cell = to;
		}
		const int units = w.bag.cells[cell].count;
		w.bag.put(cell, EMPTY, 0);
		w.counter[s.kind].push_back(units);
		return units;
	}
	return 0;
}

// Visits until the counter has nothing more to take, a buyer emptying the
// counter now and then so the cap on lines is not what ends it.
void SellOut(World& w, std::map<int, int>& sold)
{
	for (int round = 0; round < 1000; ++round)
	{
		if (Visit(w, true))
			continue;
		bool any = false;
		for (std::map<int, std::vector<int> >::iterator it = w.counter.begin(); it != w.counter.end(); ++it)
		{
			for (int u : it->second) { sold[it->first] += u; any = true; }
			it->second.clear();
		}
		if (!any)
			return;
	}
	assert(!"a visit loop that never settles");
}

void CheckArithmetic()
{
	assert(SpareUnits(10, 3) == 7);
	assert(SpareUnits(3, 3) == 0);
	assert(SpareUnits(2, 3) == 0);
	assert(SpareUnits(5, 0) == 5);

	// The old stone rule asked only the cells in front (before >= keep): a
	// bot's one stack of ten against a keep of three was never goods.
	assert(HoldsSpare(0, 10, 3));
	assert(!HoldsSpare(0, 3, 3));
	assert(HoldsSpare(3, 1, 3));
	assert(!HoldsSpare(2, 1, 3));
	assert(HoldsSpare(0, 1, 0));
	assert(!HoldsSpare(5, 0, 0));

	assert(LineTake(10, 10, 3, 1) == 1);
	assert(LineTake(1, 4, 3, 1) == 1);
	assert(LineTake(1, 3, 3, 1) == 0);
	assert(LineTake(10, 20, 12, 1) == 1);
	assert(LineTake(10, 12, 12, 1) == 0);
	assert(LineTake(4, 4, 0, 1) == 1);
	assert(LineTake(3, 13, 12, 5) == 1);   // the spare, not the line, is short
	assert(LineTake(2, 30, 0, 5) == 2);    // the stack, not the line, is short
	assert(LineTake(0, 5, 0, 1) == 0);

	assert(MayListWhole(10, 0, 7, 3));
	assert(!MayListWhole(10, 0, 8, 3));
	assert(!MayListWhole(10, 7, 1, 3));
	assert(MayListWhole(10, 6, 1, 3));
	assert(!MayListWhole(10, 0, 0, 3));
}

// A single stack of ten stones against a keep of three - the case Codex's
// review named - goes up a stone at a time, seven lines, and the bag keeps
// exactly three.
void CheckStoneStack()
{
	World w(45, 3);
	w.keep[STONE] = 3;
	w.bag.put(7, STONE, STACK_MAX);
	std::map<int, int> sold;
	SellOut(w, sold);
	assert(sold[STONE] == 7);
	assert(w.bag.total(STONE) == 3);
	// And while nobody buys, three single lines stand and no more.
	World idle(45, 3);
	idle.keep[STONE] = 3;
	idle.bag.put(7, STONE, STACK_MAX);
	while (Visit(idle, true)) {}
	assert(idle.counter[STONE].size() == 3);
	for (int u : idle.counter[STONE]) assert(u == 1);
	assert(idle.bag.total(STONE) == 7);
}

// Books of the bot's own skill at Master (keep twelve) in stacks of up to ten:
// counted by row, twelve stacks were "the keep"; counted in books, twenty
// books give eight lines and the bag keeps twelve - whatever order the cut
// singles land in, the first empty cells in front of both stacks included.
void CheckOwnBooks()
{
	World w(45, 3);
	w.keep[OWN_BOOK] = 12;
	w.bag.put(10, OWN_BOOK, 10);
	w.bag.put(30, OWN_BOOK, 10);
	std::map<int, int> sold;
	SellOut(w, sold);
	assert(sold[OWN_BOOK] == 8);
	assert(w.bag.total(OWN_BOOK) == 12);

	// Within the keep nothing goes at all.
	World kept(45, 3);
	kept.keep[OWN_BOOK] = 12;
	kept.bag.put(3, OWN_BOOK, 5);
	kept.bag.put(4, OWN_BOOK, 7);
	assert(Visit(kept, true) == 0);
	assert(kept.bag.total(OWN_BOOK) == 12);
}

// Another class's books are all goods, as singles; the stone and the own
// skill beside them keep theirs, and every line is one unit.
void CheckMixedBag()
{
	World w(45, 3);
	w.keep[OWN_BOOK] = 3;
	w.keep[OTHER_BOOK] = 0;
	w.keep[STONE] = 1;
	w.bag.put(0, OTHER_BOOK, 4);
	w.bag.put(1, OWN_BOOK, 6);
	w.bag.put(2, STONE, 2);
	w.bag.put(5, OTHER_BOOK, 3);
	w.bag.put(6, OWN_BOOK, 1);
	std::map<int, int> sold;
	SellOut(w, sold);
	assert(sold[OTHER_BOOK] == 7);
	assert(sold[OWN_BOOK] == 4);
	assert(sold[STONE] == 1);
	assert(w.bag.total(OTHER_BOOK) == 0);
	assert(w.bag.total(OWN_BOOK) == 3);
	assert(w.bag.total(STONE) == 1);
}

// A bag with no cell to cut into lists what is already a line - a single over
// the keep - and leaves a longer stack where it is rather than put it up whole.
void CheckNoRoom()
{
	World w(45, 3);
	w.keep[STONE] = 3;
	w.bag.put(0, STONE, 6);
	assert(Visit(w, false) == 0);
	assert(w.bag.total(STONE) == 6);
	w.bag.put(9, STONE, 1);  // seven now: the single in the last stack is over the keep
	assert(Visit(w, false) == 1);
	assert(w.bag.total(STONE) == 6);
}

// The classic stall puts stacks up whole in the scorer's order: MayListWhole
// holds the keep whatever the stacks are, and a stack that would dig into it
// stays home while a smaller one after it may still go.
void CheckClassicWholeStacks()
{
	const int keep = 12;
	const int stacks[] = { 10, 10, 3, 1 };   // twenty-four books, twelve spare
	int total = 0;
	for (int s : stacks) total += s;
	int listed = 0;
	std::vector<int> table;
	for (int s : stacks)
		if (MayListWhole(total, listed, s, keep)) { listed += s; table.push_back(s); }
	assert(listed == 11);                    // 10 + 1; the 3 would leave eleven
	assert(total - listed >= keep);
	assert(table.size() == 2 && table[0] == 10 && table[1] == 1);
}

}

int main()
{
	CheckArithmetic();
	CheckStoneStack();
	CheckOwnBooks();
	CheckMixedBag();
	CheckNoRoom();
	CheckClassicWholeStacks();
	std::puts("playerbot_stall_rules_test: ok");
	return 0;
}
