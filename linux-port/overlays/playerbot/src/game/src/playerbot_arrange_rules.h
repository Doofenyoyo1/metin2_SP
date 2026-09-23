#ifndef PLAYERBOT_ARRANGE_RULES_H
#define PLAYERBOT_ARRANGE_RULES_H
// "Scal i uporzadkuj" as a plan, made before anything in the bag moves: which
// stacks pour into which, and the cell every item then stands in. No engine
// types; unit-tested in tests/playerbot_arrange_rules_test.cpp. The engine
// side (playerbot_arrange.cpp) reads the bag into Items, asks MakePlan, and
// applies the answer only when the answer is complete.
//
// What the public "sort inventory" systems get wrong is what this is shaped
// round (Codex's audit of 18 September): they take every item out first and
// only then find that the bag cannot hold them again in the new order, and
// they rebuild stacks from a vnum, which loses the sockets and the item's id.
// Here a stack keeps its id, a transfer moves units between two existing
// stacks, and the layout is proven before it is used.
//
// A bag is pages of five columns and nine rows. An item stands in one column
// of one page, `height` cells from its top cell down, and never crosses into
// the next page - which is the engine's is_empty_page_grid rule. A layout is
// therefore a packing of every column's free runs, and the one the bag
// already has is always a valid one: merging only takes items away.
#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

namespace playerbot_arrange_rules {

const int PAGE_COLUMNS = 5;
const int PAGE_ROWS = 9;
const int PAGE_CELLS = PAGE_COLUMNS * PAGE_ROWS;
const int MAX_HEIGHT = 3;
const int SORT_KEYS = 6;

struct Item {
	uint32_t id = 0;          // unique in the bag: the engine's item id
	int cell = -1;            // top cell: page * 45 + row * 5 + column
	int height = 1;           // cells tall, 1..3
	uint32_t count = 1;
	uint32_t maxStack = 1;    // what one stack of this item holds at most
	// Equal and non-zero for two stacks the engine would pour into each
	// other; the engine side decides it by comparing the items themselves,
	// never by a hash, because a collision here is a lost socket.
	uint32_t mergeGroup = 0;
	// Stays in its cell and takes no part in a merge: an active auto potion,
	// which the engine locks and MoveItem refuses to move.
	bool pinned = false;
	int64_t key[SORT_KEYS] = {};  // the order, most significant first
};

struct Transfer {
	uint32_t from = 0;
	uint32_t to = 0;
	uint32_t units = 0;
};

struct Placement {
	uint32_t id = 0;
	int cell = -1;
};

enum Strategy {
	STRATEGY_NONE = 0,
	STRATEGY_ORDER = 1,       // the sorted order, first fit in reading order
	STRATEGY_TALL_FIRST = 2,  // the same with the tallest items placed first
	STRATEGY_PACKED = 3,      // an exact packing of the free runs
	STRATEGY_KEEP = 4,        // every item where it was; the merges still apply
};

struct Plan {
	bool ok = false;
	int strategy = STRATEGY_NONE;
	std::vector<Transfer> transfers;          // in the order they are applied
	std::vector<uint32_t> emptied;            // stacks the transfers take to nothing
	std::map<uint32_t, uint32_t> survivorOf;  // an emptied stack -> the one its quickslot follows
	std::map<uint32_t, uint32_t> finalCount;  // every stack a transfer touches
	std::vector<Placement> placements;        // every item that remains, pinned included
	int moved = 0;                            // placements whose cell changes
};

inline int RowOf(int cell) { return (cell % PAGE_CELLS) / PAGE_COLUMNS; }

// Whether an item of `height` cells can stand with its top in `cell`: inside
// the bag, inside its page, and on nothing.
inline bool FitsAt(const std::vector<uint8_t>& occupied, int cell, int height)
{
	if (cell < 0 || height < 1 || height > MAX_HEIGHT || cell >= (int)occupied.size())
		return false;
	if (RowOf(cell) + height > PAGE_ROWS)
		return false;
	for (int k = 0; k < height; ++k)
		if (occupied[cell + k * PAGE_COLUMNS])
			return false;
	return true;
}

inline void Occupy(std::vector<uint8_t>& occupied, int cell, int height)
{
	for (int k = 0; k < height; ++k)
		occupied[cell + k * PAGE_COLUMNS] = 1;
}

// Whether items at the given cells form a legal bag of `pages` pages. What
// the engine loaded is checked with this before anything is planned, and
// every plan is checked with it again before it is handed back.
inline bool ValidLayout(const std::vector<Item>& items, const std::vector<int>& cells, int pages)
{
	if (items.size() != cells.size())
		return false;
	std::vector<uint8_t> occupied(pages * PAGE_CELLS, 0);
	for (size_t i = 0; i < items.size(); ++i) {
		if (!FitsAt(occupied, cells[i], items[i].height))
			return false;
		Occupy(occupied, cells[i], items[i].height);
	}
	return true;
}

inline bool ValidLayout(const std::vector<Item>& items, int pages)
{
	std::vector<int> cells;
	cells.reserve(items.size());
	for (const Item& item : items)
		cells.push_back(item.cell);
	return ValidLayout(items, cells, pages);
}

// The safebox's own rule for what it holds: one grid five columns wide and nine
// rows a page tall, with no page edge in it. CSafebox::IsEmpty asks CGrid,
// which knows no pages, so a two-cell item dropped on a page's last row stands
// on the next page's first - legal to the engine, and a box the planner must
// still read. What it lays out keeps to the pages (ValidLayout).
inline bool ValidGrid(const std::vector<Item>& items, int pages)
{
	std::vector<uint8_t> occupied(pages * PAGE_CELLS, 0);
	for (const Item& item : items) {
		if (item.cell < 0 || item.height < 1 || item.height > MAX_HEIGHT ||
				item.cell + (item.height - 1) * PAGE_COLUMNS >= (int)occupied.size())
			return false;
		for (int k = 0; k < item.height; ++k)
			if (occupied[item.cell + k * PAGE_COLUMNS])
				return false;
		Occupy(occupied, item.cell, item.height);
	}
	return true;
}

// The sort key, then the id: a total order, so the same bag always gives the
// same layout and a second "Scal i uporzadkuj" changes nothing.
inline bool KeyLess(const Item& a, const Item& b)
{
	for (int k = 0; k < SORT_KEYS; ++k)
		if (a.key[k] != b.key[k])
			return a.key[k] < b.key[k];
	return a.id < b.id;
}

// The merges, on a copy of the counts. In each group the fullest stack
// receives first (the fewest units move, and the stack a player is most
// likely to have on a quickslot keeps its id), and the emptiest gives first.
// The group's fullest stack is never a giver, so it always survives, and an
// emptied stack's quickslot follows it.
inline void PlanMerges(std::vector<Item>& items, Plan& plan)
{
	std::map<uint32_t, std::vector<size_t> > groups;
	for (size_t i = 0; i < items.size(); ++i) {
		const Item& item = items[i];
		if (item.mergeGroup == 0 || item.pinned || item.count == 0 || item.maxStack < 2)
			continue;
		groups[item.mergeGroup].push_back(i);
	}
	for (auto& entry : groups) {
		std::vector<size_t>& members = entry.second;
		if (members.size() < 2)
			continue;
		std::sort(members.begin(), members.end(), [&items](size_t a, size_t b) {
			if (items[a].count != items[b].count)
				return items[a].count > items[b].count;
			return items[a].id < items[b].id;
		});
		const uint32_t primary = items[members[0]].id;
		size_t receiver = 0;
		size_t giver = members.size() - 1;
		while (receiver < giver) {
			Item& to = items[members[receiver]];
			Item& from = items[members[giver]];
			const uint32_t room = to.count < to.maxStack ? to.maxStack - to.count : 0;
			if (room == 0) {
				++receiver;
				continue;
			}
			const uint32_t units = std::min(room, from.count);
			Transfer transfer;
			transfer.from = from.id;
			transfer.to = to.id;
			transfer.units = units;
			plan.transfers.push_back(transfer);
			to.count += units;
			from.count -= units;
			plan.finalCount[to.id] = to.count;
			plan.finalCount[from.id] = from.count;
			if (from.count == 0) {
				plan.emptied.push_back(from.id);
				plan.survivorOf[from.id] = primary;
				--giver;
			}
		}
	}
	// What the merges emptied is gone from the bag.
	items.erase(std::remove_if(items.begin(), items.end(),
				[](const Item& item) { return item.count == 0; }),
			items.end());
}

// First fit in reading order - page by page, row by row, left to right - of
// the items in the order given, round what is pinned.
inline bool PlaceFirstFit(const std::vector<const Item*>& order, std::vector<uint8_t> occupied,
		std::map<uint32_t, int>& cellOf)
{
	const int cells = (int)occupied.size();
	for (const Item* item : order) {
		int placed = -1;
		for (int cell = 0; cell < cells && placed < 0; ++cell)
			if (FitsAt(occupied, cell, item->height))
				placed = cell;
		if (placed < 0)
			return false;
		Occupy(occupied, placed, item->height);
		cellOf[item->id] = placed;
	}
	return true;
}

// A column's run of free cells between pinned items and the page's edges.
struct Run {
	int top = 0;     // top cell
	int length = 0;  // cells
};

inline std::vector<Run> FreeRuns(const std::vector<uint8_t>& occupied, int pages)
{
	std::vector<Run> runs;
	for (int page = 0; page < pages; ++page)
		for (int column = 0; column < PAGE_COLUMNS; ++column) {
			int row = 0;
			while (row < PAGE_ROWS) {
				const int cell = page * PAGE_CELLS + row * PAGE_COLUMNS + column;
				if (occupied[cell]) {
					++row;
					continue;
				}
				Run run;
				run.top = cell;
				while (row < PAGE_ROWS && !occupied[page * PAGE_CELLS + row * PAGE_COLUMNS + column]) {
					++run.length;
					++row;
				}
				runs.push_back(run);
			}
		}
	return runs;
}

// An exact packing. Every item stands inside one free run, and a run holds any
// set of items whose heights add up to its length or less, stacked - so the
// question is bin packing with sizes one to three, which a table answers:
// best[a] is the most two-cell items the runs so far can take beside `a`
// three-cell ones. One-cell items fill whatever is left, and what is left is
// the free total less what the taller items take, however they were spread.
// The greedy strategies above can miss a packing that exists (three, three and
// six twos into two columns of nine: tallest first puts both threes in one
// column); this cannot.
inline bool PlacePacked(const std::vector<const Item*>& order, const std::vector<uint8_t>& occupied,
		int pages, std::map<uint32_t, int>& cellOf)
{
	std::vector<const Item*> byHeight[MAX_HEIGHT + 1];
	for (const Item* item : order)
		byHeight[item->height].push_back(item);
	const int n3 = (int)byHeight[3].size();
	const int n2 = (int)byHeight[2].size();
	const int n1 = (int)byHeight[1].size();

	const std::vector<Run> runs = FreeRuns(occupied, pages);
	int freeCells = 0;
	for (const Run& run : runs)
		freeCells += run.length;
	if (3 * n3 + 2 * n2 + n1 > freeCells)
		return false;

	const int NONE = -1;
	std::vector<int> best(n3 + 1, NONE);
	best[0] = 0;
	// threesIn[r][a]: how many three-cell items run r takes when the first r+1
	// runs hold `a` of them at best - read back from the last run.
	std::vector<std::vector<int> > threesIn(runs.size(), std::vector<int>(n3 + 1, 0));
	for (size_t r = 0; r < runs.size(); ++r) {
		std::vector<int> next(n3 + 1, NONE);
		for (int a = 0; a <= n3; ++a) {
			if (best[a] == NONE)
				continue;
			for (int x3 = 0; 3 * x3 <= runs[r].length && a + x3 <= n3; ++x3) {
				const int twos = best[a] + (runs[r].length - 3 * x3) / 2;
				if (twos > next[a + x3]) {
					next[a + x3] = twos;
					threesIn[r][a + x3] = x3;
				}
			}
		}
		best.swap(next);
	}
	if (best[n3] == NONE || best[n3] < n2)
		return false;

	std::vector<int> threes(runs.size(), 0);
	int a = n3;
	for (size_t r = runs.size(); r-- > 0;) {
		threes[r] = threesIn[r][a];
		a -= threes[r];
	}
	if (a != 0)
		return false;

	size_t next3 = 0, next2 = 0, next1 = 0;
	for (size_t r = 0; r < runs.size(); ++r) {
		int row = 0;  // rows into the run
		for (int i = 0; i < threes[r]; ++i, row += 3)
			cellOf[byHeight[3][next3++]->id] = runs[r].top + row * PAGE_COLUMNS;
		while (next2 < byHeight[2].size() && row + 2 <= runs[r].length) {
			cellOf[byHeight[2][next2++]->id] = runs[r].top + row * PAGE_COLUMNS;
			row += 2;
		}
		while (next1 < byHeight[1].size() && row + 1 <= runs[r].length) {
			cellOf[byHeight[1][next1++]->id] = runs[r].top + row * PAGE_COLUMNS;
			row += 1;
		}
	}
	return next3 == byHeight[3].size() && next2 == byHeight[2].size() && next1 == byHeight[1].size();
}

// The plan for a bag of `pages` pages. A bag that is not a legal layout to
// begin with is refused whole: a plan made from items the engine has on top
// of each other would only move the damage somewhere else. pageBoundInput
// false reads the input by the safebox's rule (ValidGrid) instead; the layout
// handed back keeps to the pages either way.
inline Plan MakePlan(std::vector<Item> items, int pages, bool pageBoundInput = true)
{
	Plan plan;
	if (pages < 1 || !(pageBoundInput ? ValidLayout(items, pages) : ValidGrid(items, pages)))
		return plan;
	std::map<uint32_t, int> original;
	for (const Item& item : items)
		original[item.id] = item.cell;
	if (original.size() != items.size())
		return plan;  // two items with one id

	PlanMerges(items, plan);

	std::vector<uint8_t> pinnedCells(pages * PAGE_CELLS, 0);
	std::vector<const Item*> movable;
	for (const Item& item : items) {
		if (item.pinned)
			Occupy(pinnedCells, item.cell, item.height);
		else
			movable.push_back(&item);
	}
	std::sort(movable.begin(), movable.end(),
			[](const Item* a, const Item* b) { return KeyLess(*a, *b); });

	std::map<uint32_t, int> cellOf;
	int strategy = STRATEGY_NONE;
	if (PlaceFirstFit(movable, pinnedCells, cellOf)) {
		strategy = STRATEGY_ORDER;
	} else {
		cellOf.clear();
		std::vector<const Item*> tall(movable);
		std::stable_sort(tall.begin(), tall.end(),
				[](const Item* a, const Item* b) { return a->height > b->height; });
		if (PlaceFirstFit(tall, pinnedCells, cellOf)) {
			strategy = STRATEGY_TALL_FIRST;
		} else {
			cellOf.clear();
			if (PlacePacked(movable, pinnedCells, pages, cellOf))
				strategy = STRATEGY_PACKED;
		}
	}
	if (strategy == STRATEGY_NONE) {
		cellOf.clear();
		for (const Item* item : movable)
			cellOf[item->id] = item->cell;
		strategy = STRATEGY_KEEP;
	}

	std::vector<int> cells;
	for (const Item& item : items) {
		Placement placement;
		placement.id = item.id;
		placement.cell = item.pinned ? item.cell : cellOf[item.id];
		plan.placements.push_back(placement);
		cells.push_back(placement.cell);
		if (placement.cell != original[item.id])
			++plan.moved;
	}
	if (!ValidLayout(items, cells, pages)) {
		// Never expected: every strategy is checked here before it is used.
		Plan refused;
		return refused;
	}
	plan.strategy = strategy;
	plan.ok = true;
	return plan;
}

// A stack moved by count: from the bag into the safebox, out of it, or from one
// safebox cell to another (/safebox_put, /safebox_take and /safebox_move in
// playerbot_arrange.cpp). What stands at the destination decides it: an empty
// place takes the whole stack or a part cut off it, the same item takes what
// its stack has room for, and anything else takes nothing - never a swap, which
// no path in the engine makes across two windows either.
enum TransferKind {
	TRANSFER_KIND_NONE = 0,   // refused, and `refusal` says why
	TRANSFER_KIND_MOVE = 1,   // the whole stack changes place
	TRANSFER_KIND_SPLIT = 2,  // `units` cut off into a new stack at the destination
	TRANSFER_KIND_POUR = 3,   // `units` poured into the stack standing there
};

enum TransferRefusal {
	TRANSFER_REFUSED_NONE = 0,
	TRANSFER_REFUSED_OCCUPIED = 1,  // something else stands there
	TRANSFER_REFUSED_FULL = 2,      // the same item, and its stack is full
	TRANSFER_REFUSED_EMPTY = 3,     // a source with nothing in it
};

struct TransferPlan {
	int kind = TRANSFER_KIND_NONE;
	int refusal = TRANSFER_REFUSED_NONE;
	uint32_t units = 0;
	bool sourceEmptied = false;  // the source stack is gone afterwards
};

// have: the units in the source stack. asked: the units the player picked, where
// 0 - a stack taken up whole - and anything over `have` mean all of them.
// splittable: the source may be cut in two (stackable, no ITEM_ANTIFLAG_STACK);
// a stack that may not is only ever moved whole. The destination is either
// empty and wide enough for the item, or holds a stack that pours with this one
// (sameStack, with its count and its own limit), or holds something else.
inline TransferPlan PlanTransfer(uint32_t have, uint32_t asked, bool splittable, bool destinationEmpty,
		bool sameStack, uint32_t destinationCount, uint32_t destinationMax)
{
	TransferPlan plan;
	if (have == 0) {
		plan.refusal = TRANSFER_REFUSED_EMPTY;
		return plan;
	}
	const uint32_t units = (!splittable || asked == 0 || asked >= have) ? have : asked;
	if (destinationEmpty) {
		plan.kind = units == have ? TRANSFER_KIND_MOVE : TRANSFER_KIND_SPLIT;
		plan.units = units;
		plan.sourceEmptied = units == have;
		return plan;
	}
	if (!sameStack || !splittable) {
		plan.refusal = TRANSFER_REFUSED_OCCUPIED;
		return plan;
	}
	const uint32_t room = destinationCount < destinationMax ? destinationMax - destinationCount : 0;
	if (room == 0) {
		plan.refusal = TRANSFER_REFUSED_FULL;
		return plan;
	}
	plan.kind = TRANSFER_KIND_POUR;
	plan.units = units < room ? units : room;
	plan.sourceEmptied = plan.units == have;
	return plan;
}

}  // namespace playerbot_arrange_rules

#endif
