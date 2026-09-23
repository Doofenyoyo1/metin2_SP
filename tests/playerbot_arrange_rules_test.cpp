// "Scal i uporzadkuj" as a plan (playerbot_arrange_rules.h): the merges keep
// every unit and every surviving id, the layout is legal and complete, what is
// pinned stays, and a second run on the result changes nothing.
// Build: g++ -std=c++17 -Wall -Wextra -I linux-port/overlays/playerbot/src/game/src tests/playerbot_arrange_rules_test.cpp
#include "playerbot_arrange_rules.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <set>
#include <vector>
using namespace playerbot_arrange_rules;

namespace {

int failures = 0;

void check(bool ok, const char* what)
{
	if (!ok) {
		std::printf("FAIL: %s\n", what);
		++failures;
	}
}

Item make(uint32_t id, int cell, int height, int64_t category = 0, uint32_t count = 1,
		uint32_t maxStack = 1, uint32_t group = 0, bool pinned = false)
{
	Item item;
	item.id = id;
	item.cell = cell;
	item.height = height;
	item.count = count;
	item.maxStack = maxStack;
	item.mergeGroup = group;
	item.pinned = pinned;
	item.key[0] = category;
	return item;
}

int cellOf(const Plan& plan, uint32_t id)
{
	for (const Placement& p : plan.placements)
		if (p.id == id)
			return p.cell;
	return -1;
}

// The bag after the plan: the survivors at their new cells with their new
// counts - what the engine side ends up holding.
std::vector<Item> apply(const std::vector<Item>& before, const Plan& plan)
{
	std::vector<Item> after;
	for (const Item& item : before) {
		const int cell = cellOf(plan, item.id);
		if (cell < 0)
			continue;
		Item moved = item;
		moved.cell = cell;
		std::map<uint32_t, uint32_t>::const_iterator it = plan.finalCount.find(item.id);
		if (it != plan.finalCount.end())
			moved.count = it->second;
		after.push_back(moved);
	}
	return after;
}

std::map<uint32_t, uint64_t> unitsByGroup(const std::vector<Item>& items)
{
	std::map<uint32_t, uint64_t> units;
	for (const Item& item : items)
		units[item.mergeGroup == 0 ? 1000000 + item.id : item.mergeGroup] += item.count;
	return units;
}

// Everything a plan must hold, whatever the bag.
void checkPlan(const std::vector<Item>& before, const Plan& plan, int pages, const char* name)
{
	char what[160];
	std::snprintf(what, sizeof(what), "%s: plan made", name);
	check(plan.ok, what);
	if (!plan.ok)
		return;
	const std::vector<Item> after = apply(before, plan);
	std::snprintf(what, sizeof(what), "%s: every survivor placed exactly once", name);
	std::set<uint32_t> ids;
	for (const Placement& p : plan.placements)
		ids.insert(p.id);
	check(ids.size() == plan.placements.size() && after.size() == plan.placements.size(), what);
	std::snprintf(what, sizeof(what), "%s: the new layout is legal", name);
	check(ValidLayout(after, pages), what);
	std::snprintf(what, sizeof(what), "%s: no unit lost or made", name);
	check(unitsByGroup(before) == unitsByGroup(after), what);
	for (const Item& item : before)
		if (item.pinned) {
			std::snprintf(what, sizeof(what), "%s: pinned item %u stayed", name, item.id);
			check(cellOf(plan, item.id) == item.cell, what);
		}
	for (const Transfer& t : plan.transfers) {
		std::snprintf(what, sizeof(what), "%s: a transfer moves units", name);
		check(t.units > 0 && t.from != t.to, what);
	}
	for (const Item& item : after) {
		std::snprintf(what, sizeof(what), "%s: no stack over its ceiling", name);
		check(item.count <= item.maxStack || item.maxStack < 2, what);
	}
	// Idempotent: the same plan asked of the result moves and merges nothing.
	const Plan again = MakePlan(after, pages);
	std::snprintf(what, sizeof(what), "%s: a second run changes nothing", name);
	check(again.ok && again.moved == 0 && again.transfers.empty() && again.emptied.empty(), what);
}

void testMerges()
{
	// 150 + 100 + 30 of one potion, ceiling 200: 200 and 80, the fullest
	// stack keeps its id, the emptied one's quickslot follows it.
	std::vector<Item> bag;
	bag.push_back(make(1, 0, 1, 0, 100, 200, 7));
	bag.push_back(make(2, 1, 1, 0, 150, 200, 7));
	bag.push_back(make(3, 2, 1, 0, 30, 200, 7));
	bag.push_back(make(4, 3, 1, 0, 50, 200, 8));  // another kind
	bag.push_back(make(5, 4, 1, 0, 10, 200, 0));  // never pours
	const Plan plan = MakePlan(bag, 1);
	checkPlan(bag, plan, 1, "merges");
	check(plan.emptied.size() == 1 && plan.emptied[0] == 3, "merges: the smallest stack is the one emptied");
	check(plan.survivorOf.count(3) && plan.survivorOf.at(3) == 2, "merges: its quickslot follows the fullest");
	check(plan.finalCount.at(2) == 200 && plan.finalCount.at(1) == 80, "merges: 200 and 80");
	check(cellOf(plan, 4) >= 0 && cellOf(plan, 5) >= 0, "merges: the other kinds stay");
}

void testPinnedNeverPours()
{
	std::vector<Item> bag;
	bag.push_back(make(1, 0, 1, 0, 10, 200, 3, true));  // locked: an active elixir
	bag.push_back(make(2, 7, 1, 0, 10, 200, 3));
	const Plan plan = MakePlan(bag, 1);
	checkPlan(bag, plan, 1, "pinned");
	check(plan.transfers.empty(), "pinned: nothing pours into or out of a pinned stack");
	check(cellOf(plan, 1) == 0, "pinned: stays in its cell");
	check(cellOf(plan, 2) == 1, "pinned: the rest goes round it");
}

void testCeilings()
{
	// Twenty to a stack (Medal Konny): 20, 20, 15 and 5 make 20, 20, 20.
	std::vector<Item> bag;
	bag.push_back(make(1, 0, 1, 0, 20, 20, 9));
	bag.push_back(make(2, 1, 1, 0, 15, 20, 9));
	bag.push_back(make(3, 2, 1, 0, 5, 20, 9));
	bag.push_back(make(4, 3, 1, 0, 20, 20, 9));
	const Plan plan = MakePlan(bag, 1);
	checkPlan(bag, plan, 1, "ceilings");
	check(plan.placements.size() == 3, "ceilings: three stacks of twenty");
	check(plan.transfers.size() == 1 && plan.transfers[0].units == 5, "ceilings: one transfer of five");
}

void testOrder()
{
	// Categories in reading order: the potions (0) first, then a weapon (1)
	// and an armour (2), wherever they stood.
	std::vector<Item> bag;
	bag.push_back(make(10, 44, 1, 2));        // armour piece of one cell, last cell of page one
	bag.push_back(make(11, 30, 3, 1));        // weapon, rows 6-8
	bag.push_back(make(12, 12, 1, 0, 5, 200, 4));
	const Plan plan = MakePlan(bag, 1);
	checkPlan(bag, plan, 1, "order");
	check(plan.strategy == STRATEGY_ORDER, "order: the sorted order fits");
	check(cellOf(plan, 12) == 0 && cellOf(plan, 11) == 1 && cellOf(plan, 10) == 2, "order: potion, weapon, armour");
}

void testTallFirst()
{
	// Three one-cell items sorted ahead of fourteen three-cell ones fill a
	// page exactly, but only with the tall ones placed first.
	std::vector<Item> bag;
	uint32_t id = 1;
	for (int column = 0; column < 5; ++column)
		for (int band = 0; band < 3; ++band) {
			if (column == 4 && band == 2)
				continue;
			bag.push_back(make(id++, band * 15 + column, 3, 1));
		}
	for (int row = 6; row < 9; ++row)
		bag.push_back(make(id++, row * 5 + 4, 1, 0));
	check(ValidLayout(bag, 1), "tall first: the bag is legal");
	const Plan plan = MakePlan(bag, 1);
	checkPlan(bag, plan, 1, "tall first");
	check(plan.strategy == STRATEGY_TALL_FIRST, "tall first: chosen when the order does not fit");
}

void testPacked()
{
	// Two free runs: rows 0-3 of column 0 (four cells) and rows 4-8 of
	// column 1 (five); everything else is pinned. A three-cell item and three
	// two-cell ones fill them only as 2+2 | 3+2. The three sorts first, so the
	// sorted order puts it at the top of the run of four; tallest-first does
	// the same; only the exact packing puts it in the run of five.
	std::vector<Item> bag;
	uint32_t id = 1;
	for (int column = 2; column < 5; ++column)
		for (int band = 0; band < 3; ++band)
			bag.push_back(make(id++, band * 15 + column, 3, 9, 1, 1, 0, true));
	bag.push_back(make(id++, 4 * 5 + 0, 3, 9, 1, 1, 0, true));  // column 0, rows 4-6
	bag.push_back(make(id++, 7 * 5 + 0, 2, 9, 1, 1, 0, true));  // column 0, rows 7-8
	bag.push_back(make(id++, 0 * 5 + 1, 3, 9, 1, 1, 0, true));  // column 1, rows 0-2
	bag.push_back(make(id++, 3 * 5 + 1, 1, 9, 1, 1, 0, true));  // column 1, row 3
	bag.push_back(make(id++, 0 * 5 + 0, 2, 1));                 // column 0, rows 0-1
	bag.push_back(make(id++, 2 * 5 + 0, 2, 1));                 // column 0, rows 2-3
	const uint32_t three = id;
	bag.push_back(make(id++, 4 * 5 + 1, 3, 0));                 // column 1, rows 4-6
	bag.push_back(make(id++, 7 * 5 + 1, 2, 1));                 // column 1, rows 7-8
	check(ValidLayout(bag, 1), "packed: the bag is legal");
	const Plan plan = MakePlan(bag, 1);
	checkPlan(bag, plan, 1, "packed");
	check(plan.strategy == STRATEGY_PACKED, "packed: only the exact packing fits");
	check(cellOf(plan, three) % 5 == 1, "packed: the three goes into the run of five");
}

void testRefusesBrokenBags()
{
	std::vector<Item> overlap;
	overlap.push_back(make(1, 0, 3));
	overlap.push_back(make(2, 5, 1));  // under the first
	check(!MakePlan(overlap, 1).ok, "broken: overlapping items are refused");
	std::vector<Item> acrossPage;
	acrossPage.push_back(make(1, 40, 2));  // row 8 of page one, two tall
	check(!MakePlan(acrossPage, 2).ok, "broken: an item across a page's edge is refused");
	std::vector<Item> sameId;
	sameId.push_back(make(1, 0, 1));
	sameId.push_back(make(1, 1, 1));
	check(!MakePlan(sameId, 1).ok, "broken: two items with one id are refused");
	std::vector<Item> tooTall;
	tooTall.push_back(make(1, 0, 4));
	check(!MakePlan(tooTall, 1).ok, "broken: a height the engine has not is refused");
}

void testPageEdges()
{
	// Four pages: tall items at the last legal row of each page, one-cell
	// items at the page edges 44/45, 89/90, 134/135 and 179.
	std::vector<Item> bag;
	uint32_t id = 1;
	for (int page = 0; page < 4; ++page)
		bag.push_back(make(id++, page * 45 + 6 * 5 + 2, 3, 1));
	const int edges[] = {44, 45, 89, 90, 134, 135, 179};
	for (int cell : edges)
		bag.push_back(make(id++, cell, 1, 0));
	const Plan plan = MakePlan(bag, 4);
	checkPlan(bag, plan, 4, "page edges");
}

// A small deterministic generator, so a failing bag can be reproduced.
struct Lcg {
	uint32_t state;
	explicit Lcg(uint32_t seed) : state(seed) {}
	uint32_t next() { state = state * 1664525u + 1013904223u; return state >> 8; }
	int below(int n) { return (int)(next() % (uint32_t)n); }
};

std::vector<Item> randomBag(uint32_t seed, int pages, int fillPercent)
{
	Lcg rng(seed);
	std::vector<Item> bag;
	std::vector<uint8_t> occupied(pages * PAGE_CELLS, 0);
	const int cells = pages * PAGE_CELLS;
	const int want = cells * fillPercent / 100;
	int used = 0;
	uint32_t id = 1000;
	for (int tries = 0; tries < cells * 20 && used < want; ++tries) {
		const int height = 1 + rng.below(3);
		const int cell = rng.below(cells);
		if (!FitsAt(occupied, cell, height))
			continue;
		Occupy(occupied, cell, height);
		used += height;
		Item item = make(id++, cell, height, rng.below(8));
		item.key[1] = rng.below(4);
		item.key[2] = rng.below(50);
		if (height == 1 && rng.below(2)) {
			item.mergeGroup = 1 + rng.below(6);
			item.maxStack = item.mergeGroup == 6 ? 20 : 200;
			item.count = 1 + rng.below((int)item.maxStack);
			item.key[0] = 0;
			item.key[2] = item.mergeGroup;
		}
		item.pinned = rng.below(40) == 0;
		bag.push_back(item);
	}
	return bag;
}

void testRandomBags()
{
	int packed = 0;
	for (uint32_t seed = 1; seed <= 400; ++seed) {
		const int pages = 1 + (int)(seed % 4);
		const int fill = seed % 3 == 0 ? 100 : (seed % 3 == 1 ? 90 : 60);
		const std::vector<Item> bag = randomBag(seed, pages, fill);
		char name[64];
		std::snprintf(name, sizeof(name), "random seed %u", seed);
		const Plan plan = MakePlan(bag, pages);
		checkPlan(bag, plan, pages, name);
		if (plan.strategy == STRATEGY_PACKED)
			++packed;
		check(plan.strategy != STRATEGY_KEEP, "random: a packing is always found");
	}
	std::printf("random bags: %d of 400 needed the exact packing\n", packed);
}

// The safebox's transfers (PlanTransfer): a whole stack moves, a part is cut
// off, the same item takes only what its stack has room for, and the
// destination's limit is its own - a scroll's twenty, not the potion's two
// hundred.
void testTransfers()
{
	TransferPlan t = PlanTransfer(50, 0, true, true, false, 0, 0);
	check(t.kind == TRANSFER_KIND_MOVE && t.units == 50 && t.sourceEmptied, "transfer: a stack taken up whole moves whole");
	t = PlanTransfer(50, 80, true, true, false, 0, 0);
	check(t.kind == TRANSFER_KIND_MOVE && t.units == 50, "transfer: more than the stack is the stack");
	t = PlanTransfer(50, 50, true, true, false, 0, 0);
	check(t.kind == TRANSFER_KIND_MOVE && t.sourceEmptied, "transfer: all of it picked is a move");
	t = PlanTransfer(50, 20, true, true, false, 0, 0);
	check(t.kind == TRANSFER_KIND_SPLIT && t.units == 20 && !t.sourceEmptied, "transfer: a part into an empty place is a split");
	t = PlanTransfer(1, 1, false, true, false, 0, 0);
	check(t.kind == TRANSFER_KIND_MOVE && t.units == 1, "transfer: a single item moves");
	t = PlanTransfer(5, 2, false, true, false, 0, 0);
	check(t.kind == TRANSFER_KIND_MOVE && t.units == 5, "transfer: what may not be cut moves whole");

	t = PlanTransfer(50, 0, true, false, true, 120, 200);
	check(t.kind == TRANSFER_KIND_POUR && t.units == 50 && t.sourceEmptied, "transfer: a stack that fits pours whole");
	t = PlanTransfer(50, 0, true, false, true, 180, 200);
	check(t.kind == TRANSFER_KIND_POUR && t.units == 20 && !t.sourceEmptied, "transfer: only the room pours, the rest stays");
	t = PlanTransfer(50, 10, true, false, true, 180, 200);
	check(t.kind == TRANSFER_KIND_POUR && t.units == 10 && !t.sourceEmptied, "transfer: a part pours");
	t = PlanTransfer(30, 0, true, false, true, 12, 20);
	check(t.kind == TRANSFER_KIND_POUR && t.units == 8, "transfer: the destination's own limit");
	t = PlanTransfer(30, 0, true, false, true, 200, 200);
	check(t.kind == TRANSFER_KIND_NONE && t.refusal == TRANSFER_REFUSED_FULL, "transfer: a full stack takes nothing");
	t = PlanTransfer(30, 0, true, false, false, 7, 200);
	check(t.kind == TRANSFER_KIND_NONE && t.refusal == TRANSFER_REFUSED_OCCUPIED, "transfer: another item is no destination");
	t = PlanTransfer(1, 0, false, false, true, 1, 1);
	check(t.kind == TRANSFER_KIND_NONE && t.refusal == TRANSFER_REFUSED_OCCUPIED, "transfer: what does not stack never pours");
	t = PlanTransfer(0, 0, true, true, false, 0, 0);
	check(t.kind == TRANSFER_KIND_NONE && t.refusal == TRANSFER_REFUSED_EMPTY, "transfer: an empty source");
	// The units never exceed what the source has or what the destination takes.
	for (uint32_t have = 1; have <= 40; ++have)
		for (uint32_t asked = 0; asked <= 45; asked += 3)
			for (uint32_t there = 0; there <= 20; there += 4) {
				const TransferPlan p = PlanTransfer(have, asked, true, false, true, there, 20);
				if (p.kind == TRANSFER_KIND_POUR) {
					check(p.units >= 1 && p.units <= have && there + p.units <= 20, "transfer: a pour stays inside both limits");
					check(p.sourceEmptied == (p.units == have), "transfer: emptied means all of it");
				} else {
					check(there == 20, "transfer: only a full stack refuses a pour");
				}
			}
}

// A safebox may hold an item across a page edge (CSafebox::IsEmpty asks a
// grid with no pages in it). Such a box is read (ValidGrid) and laid out
// inside the pages; the bag's rule still refuses it, and an overlap is refused
// by both.
void testSafeboxPageEdge()
{
	std::vector<Item> box;
	box.push_back(make(1, 40, 2, 3));                 // page 1's last row into page 2's first
	box.push_back(make(2, 0, 1, 1, 30, 200, 7));
	box.push_back(make(3, 46, 1, 1, 50, 200, 7));
	box.push_back(make(4, 12, 3, 2));
	check(!MakePlan(box, 2).ok, "safebox edge: the bag's rule refuses an item across a page");
	const Plan plan = MakePlan(box, 2, false);
	check(plan.ok, "safebox edge: the safebox's rule reads it");
	checkPlan(box, plan, 2, "safebox edge");
	const int cell = cellOf(plan, 1);
	check(cell >= 0 && RowOf(cell) + 2 <= PAGE_ROWS, "safebox edge: laid out inside one page");
	check(plan.transfers.size() == 1 && plan.transfers[0].units == 30, "safebox edge: the two stacks poured");

	std::vector<Item> overlap;
	overlap.push_back(make(1, 40, 2, 3));
	overlap.push_back(make(2, 45, 1, 1));
	check(!MakePlan(overlap, 2, false).ok, "safebox edge: an overlap is still refused");
	std::vector<Item> outside;
	outside.push_back(make(1, 85, 3, 3));
	check(!MakePlan(outside, 2, false).ok, "safebox edge: past the last page is refused");
}

}  // namespace

int main()
{
	testTransfers();
	testSafeboxPageEdge();
	testMerges();
	testPinnedNeverPours();
	testCeilings();
	testOrder();
	testTallFirst();
	testPacked();
	testRefusesBrokenBags();
	testPageEdges();
	testRandomBags();
	if (failures) {
		std::printf("%d failure(s)\n", failures);
		return 1;
	}
	std::printf("OK\n");
	return 0;
}
