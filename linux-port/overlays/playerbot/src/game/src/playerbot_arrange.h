#ifndef PLAYERBOT_ARRANGE_H
#define PLAYERBOT_ARRANGE_H
// "Scal i uporzadkuj": one request, and the server pours the bag's stacks
// together and lays its four pages out again. The one place this is done, for
// a player's inventory button (/inventory_arrange in cmd_general.cpp, the
// client answering "InventoryArrangeResult") and for the bots' tidy pass
// alike; the plan itself is playerbot_arrange_rules.h, the engine's half is
// playerbot_arrange.cpp. The safebox has the same button and the stacks moved
// by count across it and the bag, below. A normal header, included after
// stdafx.h by the engine TUs that call it - not a fragment of the manager.
namespace playerbot_arrange {

enum EResult {
	RESULT_DONE = 0,          // moved or merged something
	RESULT_NOTHING = 1,       // already arranged: nothing to move or merge
	RESULT_BUSY = 2,          // an exchange, a shop, the safebox, a quest, a window
	RESULT_COOLDOWN = 3,      // a player's second click inside two seconds
	RESULT_NO_LAYOUT = 4,     // no legal layout found; nothing changed
	RESULT_DEAD = 5,
	RESULT_INCONSISTENT = 6,  // the bag as the engine holds it is not a legal layout
	RESULT_UNSUPPORTED = 7,   // this engine (r40250) has no four-page bag to arrange
	RESULT_BAD_REQUEST = 8,   // an option the command does not know
	RESULT_NO_SAFEBOX = 9,    // the safebox's arrange with no safebox open
};

struct TResult {
	int code = RESULT_UNSUPPORTED;
	int items = 0;           // items in the four pages
	int moved = 0;           // items whose cell changed
	int merged = 0;          // stacks poured into others and gone
	unsigned int units = 0;  // units poured
	int pinned = 0;          // items left where they were (an active auto potion)
	int kept = 0;            // of those, locked by the player (MT2009_PLUS_INVENTORY_SORT_LOCK_V1)
	int strategy = 0;        // playerbot_arrange_rules::Strategy
	unsigned int micros = 0; // what the whole operation cost
};

// fromPlayer: a player's click, which waits two seconds between two; the bots'
// pass keeps its own clock.
TResult ArrangeInventory(LPCHARACTER ch, bool fromPlayer);

// The stacks poured together and nothing else moved - "/inventory_arrange
// merge" ("samo laczenie w stacki bez sortowania", the operator, 28
// September). The same pours ArrangeInventory makes, on the same clock.
TResult MergeInventoryStacks(LPCHARACTER ch, bool fromPlayer);

// MT2009_PLUS_INVENTORY_SORT_LOCK_V1 (server-patches/sortlock): a player's
// "/inventory_arrange [merge] [keep=<hex>]" - the inventory's "Uloz i scal"
// (no word) or "Tylko scal stosy" ("merge"), with the cells the player
// locked with Alt + left click ("keep=", playerbot_arrange_rules::
// ParseArrangeWords). A locked item stays in its cell and the rest of the bag
// is laid out round it; a locked stack takes units in a merge but never gives
// its own. A word the command does not know is RESULT_BAD_REQUEST.
TResult InventoryArrangeCommand(LPCHARACTER ch, const char* argument);

// The open safebox's pages, poured and laid out the same way
// (/safebox_arrange, answered "SafeboxArrangeResult"; the bots at the end of
// their safebox visit). `items` counts the safebox's items, and RESULT_BUSY
// covers the item shop as well as the other windows.
TResult ArrangeSafebox(LPCHARACTER ch, bool fromPlayer);

// MT2009_PLUS_SAFEBOX_MERGE_V1: the open safebox's stacks poured together and
// nothing else moved - "/safebox_arrange merge", the safebox's "Tylko scal
// stosy" button. The same pours ArrangeSafebox makes, on the same clock.
TResult MergeSafeboxStacks(LPCHARACTER ch, bool fromPlayer);

// A stack moved by count between the bag and the open safebox, or inside the
// safebox (blasty's proposal, 19 September): /safebox_put, /safebox_take and
// /safebox_move, answered "SafeboxTransferResult <op> <code> <units>" and
// shown by client-root/safeboxtransfer.py. A count of 0 is the whole stack.
// Only the four bag pages take part; the belt, the horse's page and the dragon
// soul window keep the engine's own packets.
enum ETransferOp {
	TRANSFER_OP_PUT = 1,   // bag -> safebox
	TRANSFER_OP_TAKE = 2,  // safebox -> bag
	TRANSFER_OP_MOVE = 3,  // safebox -> safebox
};

enum ETransferResult {
	TRANSFER_DONE = 0,          // moved, cut off or poured
	TRANSFER_BUSY = 1,          // another window, the item shop, a quest
	TRANSFER_NO_SAFEBOX = 2,    // no safebox open
	TRANSFER_NO_ITEM = 3,       // nothing (any more) at the source
	TRANSFER_OCCUPIED = 4,      // something else stands at the destination
	TRANSFER_FULL = 5,          // the same item, and its stack is full
	TRANSFER_REFUSED = 6,       // the item may not go there
	TRANSFER_BAD_REQUEST = 7,   // a cell or a count out of range
	TRANSFER_COOLDOWN = 8,      // faster than the engine's own safebox pulses
	TRANSFER_UNSUPPORTED = 9,   // this engine (r40250)
	TRANSFER_DEAD = 10,
};

struct TTransfer {
	int code = TRANSFER_UNSUPPORTED;
	unsigned int units = 0;  // units that changed place
};

TTransfer PutIntoSafebox(LPCHARACTER ch, unsigned int bagCell, unsigned int safePos, unsigned int count);
TTransfer TakeFromSafebox(LPCHARACTER ch, unsigned int safePos, unsigned int bagCell, unsigned int count);
TTransfer MoveInSafebox(LPCHARACTER ch, unsigned int fromPos, unsigned int toPos, unsigned int count);

}  // namespace playerbot_arrange

#endif
