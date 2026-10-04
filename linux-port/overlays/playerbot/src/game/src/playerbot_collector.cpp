// MT2009_PLUS_COLLECTOR_STORAGE_V1: the collector's storage ("Magazyn
// kolekcjonera"), the server's half (playerbot_collector.h has the protocol;
// the window is the client's uicollector.py). The upstream window's features,
// written here for a move that is instant, as the companion's quick transfer
// is.
//
// Why the upstream one waited: its window allowed one move at a time, sent
// nothing sooner than 0.2 s after the last command (the engine's flood guard
// dropped a sixth command in half a second), and after every answer asked the
// server for the whole page again, which the server laid out and sent back
// row by row - a put was a round trip, a database statement that had to be
// confirmed, then a second round trip and a page. Ten items were seconds.
//
// Here:
//  * the window holds the whole store (sent once, when it opens) and lays it
//    out, filters and searches by itself; a move changes its picture at once
//    and the server's answer is one small line or two - "set" or "del" for
//    the entry it touched, "res" for the move;
//  * "/kolekcjoner" is off the engine's flood guard (cmd.cpp) and keeps its
//    own, generous limit (OPS_PER_SECOND), so no move waits for another;
//  * nothing is read from the database after the open. Every entry is the
//    item's own row of player.item (playerbot_collector.h, OWNER_BASE), so a
//    move of a whole stack changes one row and never makes a second copy: the
//    row is either the bag's or the store's, whatever stops when. The db core
//    writes a SAFEBOX row at once (QUERY_ITEM_SAVE) and a bag row is flushed
//    with HEADER_GD_ITEM_FLUSH right after the move, as the safebox's
//    checkout does - both are queued, so no move waits for the database;
//  * a stackable item without bonuses goes into the entry of its kind (same
//    vnum and sockets) whatever its count: a hundred stacks of one material
//    are one entry.
//
// What a move checks is what the classic safebox checks (CInputMain::
// SafeboxCheckin): the bag's own cells, no ANTI_SAFEBOX, no locked item, the
// belt only empty, and hands free (CanHandleItem, no quest running, alive);
// the store opens only while the classic safebox is open (its password and
// its storekeeper), and every move is refused once the character is
// MAX_DISTANCE from where it opened.
#include "stdafx.h"
#include "playerbot_collector.h"

#include "utils.h"
#include "config.h"
#include "char.h"
#include "char_manager.h"
#include "item.h"
#include "item_manager.h"
#include "questmanager.h"
#include "questpc.h"
#include "packet.h"
#include "desc.h"
#include "desc_client.h"
#include "db.h"
#include "log.h"
#include "cmd.h"
#include "unique_item.h"
#include "belt_inventory_helper.h"
#include "../../common/length.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <unordered_map>

namespace playerbot_collector {

namespace {

const int OPS_PER_SECOND = 60;
const size_t LINE_BUDGET = 400;
const DWORD LOAD_LIMIT = 20000;

struct TSession {
	DWORD account = 0;
	DWORD handle = 0;
	DWORD nonce = 0;
	bool loaded = false;
	bool tierWritable = false;
	int tier = 0;
	long mapIndex = 0;
	long x = 0;
	long y = 0;
	std::map<DWORD, TPlayerItem> entries;	// by item id
	DWORD opWindowAt = 0;
	int opsInWindow = 0;
};

std::unordered_map<DWORD, TSession> s_sessions;	// by player id
DWORD s_nonce = 0;

DWORD OwnerOf(DWORD account)
{
	return OWNER_BASE + account;
}

DWORD Capacity(int tier)
{
	return TIER_ENTRIES[MINMAX(0, tier, TIER_COUNT - 1)];
}

const TItemTable* Proto(DWORD vnum)
{
	return ITEM_MANAGER::instance().GetTable(vnum);
}

bool ProtoStacks(DWORD vnum)
{
	const TItemTable* t = Proto(vnum);
	return t && IS_SET(t->dwFlags, ITEM_FLAG_STACKABLE) && !IS_SET(t->dwAntiFlags, ITEM_ANTIFLAG_STACK);
}

DWORD StackOf(DWORD vnum)
{
	const TItemTable* t = Proto(vnum);
	if (!t || !ProtoStacks(vnum))
		return 1;
	return MAX(1u, (DWORD)t->dwMaxStack);
}

bool NoAttrs(const TPlayerItem& r)
{
	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		if (r.aAttr[i].bType || r.aAttr[i].sValue)
			return false;
	return true;
}

bool ItemNoAttrs(LPITEM item)
{
	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		if (item->GetAttributeType(i) || item->GetAttributeValue(i))
			return false;
	return true;
}

// Whether an entry takes more units of this kind: a stackable item without
// bonuses and with the same sockets.
bool Mergeable(const TPlayerItem& r, DWORD vnum, const long* sockets)
{
	if (r.vnum != vnum || !ProtoStacks(vnum) || !NoAttrs(r))
		return false;
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		if (r.alSockets[i] != sockets[i])
			return false;
	return true;
}

std::string Encode(const TPlayerItem& r)
{
	char buf[256];
	int n = snprintf(buf, sizeof(buf), "%u,%u,%u,%ld,%ld,%ld", r.id, r.vnum, (unsigned)r.count,
			r.alSockets[0], r.alSockets[1], r.alSockets[2]);
	std::string s(buf, n > 0 ? n : 0);
	if (!NoAttrs(r)) {
		s += ',';
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i) {
			n = snprintf(buf, sizeof(buf), "%s%u:%d", i ? ";" : "", (unsigned)r.aAttr[i].bType, (int)r.aAttr[i].sValue);
			s.append(buf, n > 0 ? n : 0);
		}
	}
	return s;
}

void Say(LPCHARACTER ch, const char* fmt, const std::string& s)
{
	ch->ChatPacket(CHAT_TYPE_COMMAND, fmt, s.c_str());
}

void SendSet(LPCHARACTER ch, const TPlayerItem& r)
{
	Say(ch, "COLL set %s", Encode(r));
}

void SendDel(LPCHARACTER ch, DWORD id)
{
	ch->ChatPacket(CHAT_TYPE_COMMAND, "COLL del %u", id);
}

void SendRes(LPCHARACTER ch, DWORD op, int code, unsigned long long units)
{
	ch->ChatPacket(CHAT_TYPE_COMMAND, "COLL res %u %d %llu", op, code, units);
}

// The entry's row, now: window SAFEBOX is written at once by the db core.
void SaveRecord(const TPlayerItem& r)
{
	if (!db_clientdesc)
		return;
	db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_SAVE, 0, sizeof(TPlayerItem));
	db_clientdesc->Packet(&r, sizeof(TPlayerItem));
}

void DeleteRecord(DWORD id)
{
	if (!db_clientdesc || !id)
		return;
	DWORD pid = 0;
	db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_DESTROY, 0, sizeof(DWORD) + sizeof(DWORD));
	db_clientdesc->Packet(&id, sizeof(DWORD));
	db_clientdesc->Packet(&pid, sizeof(DWORD));
}

// A bag item's row written now (playerbot_arrange.cpp FlushRow's shape).
void FlushRow(LPITEM item)
{
	if (!item || !item->GetID() || !db_clientdesc)
		return;
	ITEM_MANAGER::instance().FlushDelayedSave(item);
	const DWORD id = item->GetID();
	db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_FLUSH, 0, sizeof(DWORD));
	db_clientdesc->Packet(&id, sizeof(DWORD));
}

TPlayerItem RecordOf(LPITEM item, DWORD account)
{
	TPlayerItem r;
	memset(&r, 0, sizeof(r));
	r.id = item->GetID();
	r.window = SAFEBOX;
	r.pos = 0;
	r.count = item->GetCount();
	r.vnum = item->GetOriginalVnum();
	thecore_memcpy(r.alSockets, item->GetSockets(), sizeof(r.alSockets));
	thecore_memcpy(r.aAttr, item->GetAttributes(), sizeof(r.aAttr));
	r.owner = OwnerOf(account);
	return r;
}

bool AllowedOp(TSession& s)
{
	const DWORD now = get_dword_time();
	if (now - s.opWindowAt >= 1000) {
		s.opWindowAt = now;
		s.opsInWindow = 0;
	}
	return ++s.opsInWindow <= OPS_PER_SECOND;
}

void PurgeOffline()
{
	if (s_sessions.size() < 16)
		return;
	for (auto it = s_sessions.begin(); it != s_sessions.end();) {
		LPCHARACTER other = CHARACTER_MANAGER::instance().FindByPID(it->first);
		if (!other || !other->GetDesc() || other->GetDesc()->GetHandle() != it->second.handle)
			it = s_sessions.erase(it);
		else
			++it;
	}
}

bool RealPlayer(LPCHARACTER ch)
{
	return ch && ch->IsPC() && ch->GetDesc() && ch->GetDesc()->IsPhase(PHASE_GAME) && ch->IsItemLoaded() &&
			ch->GetDesc()->GetAccountTable().id != 0;
}

// The session this character's command works on, or NULL with the answer
// already sent.
TSession* Live(LPCHARACTER ch, DWORD op)
{
	auto it = s_sessions.find(ch->GetPlayerID());
	if (it == s_sessions.end() || it->second.handle != ch->GetDesc()->GetHandle() ||
			it->second.account != ch->GetDesc()->GetAccountTable().id) {
		if (it != s_sessions.end())
			s_sessions.erase(it);
		SendRes(ch, op, RESULT_NO_SESSION, 0);
		ch->ChatPacket(CHAT_TYPE_COMMAND, "COLL close %d", CLOSE_GONE);
		return NULL;
	}
	TSession& s = it->second;
	if (!s.loaded) {
		SendRes(ch, op, RESULT_LOADING, 0);
		return NULL;
	}
	if (ch->GetMapIndex() != s.mapIndex || DISTANCE_APPROX(ch->GetX() - s.x, ch->GetY() - s.y) > MAX_DISTANCE) {
		s_sessions.erase(it);
		SendRes(ch, op, RESULT_TOO_FAR, 0);
		ch->ChatPacket(CHAT_TYPE_COMMAND, "COLL close %d", CLOSE_FAR);
		return NULL;
	}
	if (!AllowedOp(s)) {
		SendRes(ch, op, RESULT_COOLDOWN, 0);
		return NULL;
	}
	return &s;
}

// Hands free for an item move, as the safebox asks them (playerbot_arrange.cpp
// SafeboxHands without the box itself).
int Hands(LPCHARACTER ch)
{
	if (ch->IsDead())
		return RESULT_DEAD;
	if (quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID())->IsRunning())
		return RESULT_BUSY;
	if (!ch->CanHandleItem(false, false, BUSY_SAFEBOX))
		return RESULT_BUSY;
	return RESULT_DONE;
}

bool BagCell(LPCHARACTER ch, int cell)
{
	return cell >= 0 && cell < ch->GetInventoryMaxCount();
}

// Whether this bag item may go into the store: the classic safebox's rules.
int Storable(LPCHARACTER ch, LPITEM item)
{
	if (!item || item->GetWindow() != INVENTORY || !BagCell(ch, item->GetCell()) || item->IsEquipped())
		return RESULT_NO_ITEM;
	if (item->GetCell() >= INVENTORY_DEFAULT_MAX_NUM && IS_SET(item->GetFlag(), ITEM_FLAG_IRREMOVABLE))
		return RESULT_REFUSED;
	if (item->GetVnum() == UNIQUE_ITEM_SAFEBOX_EXPAND || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_SAFEBOX))
		return RESULT_REFUSED;
	if (item->isLocked() || item->IsExchanging() || item->IsDragonSoul())
		return RESULT_REFUSED;
	if (item->GetType() == ITEM_BELT && CBeltInventoryHelper::IsExistItemInBeltInventory(ch))
		return RESULT_REFUSED;
	if (!Proto(item->GetOriginalVnum()))
		return RESULT_REFUSED;
	return RESULT_DONE;
}

TPlayerItem* MergeTarget(TSession& s, DWORD vnum, const long* sockets)
{
	for (auto& kv : s.entries)
		if (Mergeable(kv.second, vnum, sockets))
			return &kv.second;
	return NULL;
}

// Puts `count` (0: all) of the bag item into the store. The touched entry's
// id goes to `touched`; the units moved are returned through `moved`.
int PutOne(LPCHARACTER ch, TSession& s, LPITEM item, DWORD count, std::vector<DWORD>& touched, unsigned long long& moved)
{
	int code = Storable(ch, item);
	if (code != RESULT_DONE)
		return code;
	const DWORD have = item->GetCount();
	const bool stacks = ProtoStacks(item->GetOriginalVnum()) && ItemNoAttrs(item);
	DWORD n = (count == 0 || count >= have || !stacks) ? have : count;
	const bool whole = n == have;
	const WORD cell = item->GetCell();
	char hint[96];

	TPlayerItem* target = stacks ? MergeTarget(s, item->GetOriginalVnum(), item->GetSockets()) : NULL;
	if (target) {
		const DWORD room = MAX_ENTRY_COUNT - MIN(MAX_ENTRY_COUNT, (DWORD)target->count);
		if (room == 0)
			return RESULT_FULL;
		n = MIN(n, room);
		target->count += n;
		SaveRecord(*target);
		touched.push_back(target->id);
		snprintf(hint, sizeof(hint), "%u -> %u (%u)", n, target->id, (unsigned)target->count);
		if (n == have) {
			LogManager::instance().ItemLog(ch, item, "COLLECTOR PUT MERGE", hint);
			ITEM_MANAGER::instance().RemoveItem(item, "COLLECTOR_PUT_MERGE");
			ch->SyncQuickslot(QUICKSLOT_TYPE_ITEM, cell, 255);
		} else {
			item->SetCount(have - n);
			FlushRow(item);
			LogManager::instance().ItemLog(ch, item, "COLLECTOR PUT PART", hint);
		}
		moved += n;
		return RESULT_DONE;
	}

	if (s.entries.size() >= Capacity(s.tier))
		return RESULT_FULL;

	if (whole) {
		// The item's own row becomes the entry: one row, never two copies.
		TPlayerItem r = RecordOf(item, s.account);
		snprintf(hint, sizeof(hint), "%u", (unsigned)r.count);
		LogManager::instance().ItemLog(ch, item, "COLLECTOR PUT", hint);
		item->SetSkipSave(true);
		ITEM_MANAGER::instance().RemoveFromDelayedSave(item);
		item->RemoveFromCharacter();
		ch->SyncQuickslot(QUICKSLOT_TYPE_ITEM, cell, 255);
		M2_DESTROY_ITEM(item);
		SaveRecord(r);
		s.entries[r.id] = r;
		touched.push_back(r.id);
		moved += r.count;
		return RESULT_DONE;
	}

	// Part of a stack: a new entry with a new id.
	TPlayerItem r = RecordOf(item, s.account);
	r.id = ITEM_MANAGER::instance().GetNewID();
	if (!r.id)
		return RESULT_BUSY;
	r.count = n;
	SaveRecord(r);
	s.entries[r.id] = r;
	touched.push_back(r.id);
	item->SetCount(have - n);
	FlushRow(item);
	snprintf(hint, sizeof(hint), "%u -> new %u", n, r.id);
	LogManager::instance().ItemLog(ch, item, "COLLECTOR PUT PART", hint);
	moved += n;
	return RESULT_DONE;
}

void SendTouched(LPCHARACTER ch, TSession& s, std::vector<DWORD>& touched)
{
	std::sort(touched.begin(), touched.end());
	touched.erase(std::unique(touched.begin(), touched.end()), touched.end());
	for (DWORD id : touched) {
		auto it = s.entries.find(id);
		if (it != s.entries.end())
			SendSet(ch, it->second);
		else
			SendDel(ch, id);
	}
}

// A new bag item of `n` units of the entry, at `cell` when it fits there.
LPITEM NewBagItem(LPCHARACTER ch, const TPlayerItem& r, DWORD n, int cell, DWORD id)
{
	const TItemTable* t = Proto(r.vnum);
	if (!t)
		return NULL;
	const BYTE size = t->bSize;
	if (!BagCell(ch, cell) || !ch->IsEmptyItemGrid(TItemPos(INVENTORY, cell), size))
		cell = ch->GetEmptyInventory(size);
	if (cell < 0)
		return NULL;
	LPITEM item = ITEM_MANAGER::instance().CreateItem(r.vnum, n, id);
	if (!item)
		return NULL;
	item->SetSkipSave(true);
	item->SetSockets(r.alSockets);
	item->SetAttributes(r.aAttr);
	if (!item->AddToCharacter(ch, TItemPos(INVENTORY, cell))) {
		M2_DESTROY_ITEM(item);
		return NULL;
	}
	item->OnAfterCreatedItem();
	item->SetSkipSave(false);
	item->Save();
	FlushRow(item);
	return item;
}

int Take(LPCHARACTER ch, TSession& s, DWORD op, DWORD id, DWORD count, int cell)
{
	auto it = s.entries.find(id);
	if (it == s.entries.end())
		return RESULT_NO_ITEM;
	TPlayerItem& r = it->second;
	// An item of this id already lives (a row read before its move out was
	// written): the entry is a ghost - it leaves the picture, never the bag.
	if (ITEM_MANAGER::instance().Find(id)) {
		s.entries.erase(it);
		SendDel(ch, id);
		return RESULT_NO_ITEM;
	}
	const bool stacks = ProtoStacks(r.vnum) && NoAttrs(r);
	const DWORD stack = stacks ? StackOf(r.vnum) : MAX(1u, (DWORD)r.count);
	DWORD want = count == 0 ? MIN((DWORD)r.count, stack) : MIN(count, (DWORD)r.count);
	if (!stacks)
		want = r.count;
	DWORD left = want;
	char hint[96];

	// A stackable entry tops up the bag's own stacks of its kind first - all of
	// them for a right click, the one it was dropped on for a drag.
	if (stacks) {
		for (int c = 0; c < ch->GetInventoryMaxCount() && left; ++c) {
			if (cell >= 0 && c != cell)
				continue;
			LPITEM it2 = ch->GetInventoryItem(c);
			if (!it2 || it2->GetCell() != c || it2->GetOriginalVnum() != r.vnum || it2->isLocked() || !ItemNoAttrs(it2))
				continue;
			bool same = true;
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				if (it2->GetSocket(i) != r.alSockets[i])
					same = false;
			if (!same || it2->GetCount() >= stack)
				continue;
			const DWORD add = MIN(left, stack - it2->GetCount());
			it2->SetCount(it2->GetCount() + add);
			FlushRow(it2);
			left -= add;
		}
	}

	// The whole entry into one new bag stack: the entry's own row moves.
	if (left == r.count && left <= stack) {
		const TPlayerItem copy = r;
		LPITEM item = NewBagItem(ch, copy, left, cell, copy.id);
		if (!item)
			return want == left ? RESULT_NO_ROOM : RESULT_PARTIAL;
		snprintf(hint, sizeof(hint), "%u", (unsigned)copy.count);
		LogManager::instance().ItemLog(ch, item, "COLLECTOR GET", hint);
		s.entries.erase(it);
		SendDel(ch, id);
		return RESULT_DONE;
	}

	// Otherwise new stacks with new ids, as many as asked and as fit.
	while (left > 0) {
		const DWORD n = MIN(left, stack);
		LPITEM item = NewBagItem(ch, r, n, cell, 0);
		if (!item)
			break;
		cell = -1;
		snprintf(hint, sizeof(hint), "%u of %u", n, r.id);
		LogManager::instance().ItemLog(ch, item, "COLLECTOR GET PART", hint);
		left -= n;
	}
	const DWORD taken = want - left;
	if (taken == 0)
		return RESULT_NO_ROOM;
	if (taken >= r.count) {
		DeleteRecord(id);
		s.entries.erase(it);
		SendDel(ch, id);
	} else {
		r.count -= taken;
		SaveRecord(r);
		SendSet(ch, r);
	}
	return left ? RESULT_PARTIAL : RESULT_DONE;
}

// "keep=<hex>": the sort-locked cells (inventorysortlock.py), four to a digit.
bool KeptCell(const std::string& mask, int cell)
{
	const size_t digit = cell / 4;
	if (digit >= mask.size())
		return false;
	const char c = mask[digit];
	int v = 0;
	if (c >= '0' && c <= '9') v = c - '0';
	else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
	else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
	return (v >> (cell % 4)) & 1;
}

void SendAll(LPCHARACTER ch, TSession& s)
{
	ch->ChatPacket(CHAT_TYPE_COMMAND, "COLL begin %d %u %u %d", s.tier, Capacity(s.tier), (unsigned)s.entries.size(),
			s.tierWritable ? 1 : 0);
	std::string line;
	for (auto& kv : s.entries) {
		const std::string e = Encode(kv.second);
		if (!line.empty() && line.size() + e.size() + 1 > LINE_BUDGET) {
			Say(ch, "COLL e%s", line);
			line.clear();
		}
		line += ' ';
		line += e;
	}
	if (!line.empty())
		Say(ch, "COLL e%s", line);
	ch->ChatPacket(CHAT_TYPE_COMMAND, "COLL end %u", (unsigned)s.entries.size());
}

LPCHARACTER Back(DWORD pid, DWORD handle, DWORD nonce, TSession*& s)
{
	s = NULL;
	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pid);
	if (!ch || !ch->GetDesc() || ch->GetDesc()->GetHandle() != handle)
		return NULL;
	auto it = s_sessions.find(pid);
	if (it == s_sessions.end() || it->second.nonce != nonce)
		return NULL;
	s = &it->second;
	return ch;
}

void LoadItems(DWORD pid, DWORD handle, DWORD nonce, DWORD account)
{
	DBManager::instance().FuncQuery([pid, handle, nonce](SQLMsg* msg) {
		TSession* s = NULL;
		LPCHARACTER ch = Back(pid, handle, nonce, s);
		if (!ch)
			return;
		if (!msg || msg->uiSQLErrno != 0 || !msg->Get()) {
			s_sessions.erase(pid);
			ch->ChatPacket(CHAT_TYPE_COMMAND, "COLL err %d", OPEN_DB);
			return;
		}
		s->entries.clear();
		if (msg->Get()->pSQLResult) {
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(msg->Get()->pSQLResult))) {
				TPlayerItem r;
				memset(&r, 0, sizeof(r));
				int col = 0;
				str_to_number(r.id, row[col++]);
				str_to_number(r.count, row[col++]);
				str_to_number(r.vnum, row[col++]);
				for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
					str_to_number(r.alSockets[i], row[col++]);
				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i) {
					str_to_number(r.aAttr[i].bType, row[col++]);
					str_to_number(r.aAttr[i].sValue, row[col++]);
				}
				if (!r.id || !r.vnum || !r.count || !Proto(r.vnum))
					continue;	// left in the database, never shown
				r.window = SAFEBOX;
				r.owner = OwnerOf(s->account);
				s->entries[r.id] = r;
			}
		}
		s->loaded = true;
		SendAll(ch, *s);
	}, "SELECT id, count, vnum, socket0, socket1, socket2, "
	   "attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, attrtype3, attrvalue3, "
	   "attrtype4, attrvalue4, attrtype5, attrvalue5, attrtype6, attrvalue6 "
	   "FROM player.item WHERE owner_id=%u AND `window`='SAFEBOX' ORDER BY id LIMIT %u", OwnerOf(account), LOAD_LIMIT);
}

void Open(LPCHARACTER ch)
{
	if (ch->IsDead()) {
		ch->ChatPacket(CHAT_TYPE_COMMAND, "COLL err %d", OPEN_DEAD);
		return;
	}
	if (!ch->GetSafebox() || !ch->IsOpenSafebox()) {
		ch->ChatPacket(CHAT_TYPE_COMMAND, "COLL err %d", OPEN_NO_SAFEBOX);
		return;
	}
	if (ch->GetDistanceFromSafeboxOpen() > 1000) {
		ch->ChatPacket(CHAT_TYPE_COMMAND, "COLL err %d", OPEN_TOO_FAR);
		return;
	}
	auto old = s_sessions.find(ch->GetPlayerID());
	if (old != s_sessions.end() && old->second.handle == ch->GetDesc()->GetHandle() && !old->second.loaded) {
		ch->ChatPacket(CHAT_TYPE_COMMAND, "COLL err %d", OPEN_BUSY);	// still loading
		return;
	}
	PurgeOffline();
	TSession s;
	s.account = ch->GetDesc()->GetAccountTable().id;
	s.handle = ch->GetDesc()->GetHandle();
	s.nonce = ++s_nonce;
	s.mapIndex = ch->GetMapIndex();
	s.x = ch->GetX();
	s.y = ch->GetY();
	const DWORD pid = ch->GetPlayerID();
	const DWORD handle = s.handle;
	const DWORD nonce = s.nonce;
	const DWORD account = s.account;
	s_sessions[pid] = s;
	// The tier first, then the entries (one connection: in this order).
	DBManager::instance().FuncQuery([pid, handle, nonce, account](SQLMsg* msg) {
		TSession* s = NULL;
		LPCHARACTER ch = Back(pid, handle, nonce, s);
		if (!ch)
			return;
		s->tier = 0;
		s->tierWritable = msg && msg->uiSQLErrno == 0;
		if (s->tierWritable && msg->Get() && msg->Get()->pSQLResult) {
			MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
			if (row && row[0])
				str_to_number(s->tier, row[0]);
		}
		s->tier = MINMAX(0, s->tier, TIER_COUNT - 1);
		LoadItems(pid, handle, nonce, account);
	}, "SELECT tier FROM player.collector_storage WHERE account_id=%u", account);
}

void Expand(LPCHARACTER ch, TSession& s, DWORD op)
{
	if (!s.tierWritable) {
		SendRes(ch, op, RESULT_DISABLED, 0);
		return;
	}
	if (s.tier >= TIER_COUNT - 1) {
		SendRes(ch, op, RESULT_MAX_TIER, 0);
		return;
	}
	const int next = s.tier + 1;
	if (ch->GetLevel() < TIER_LEVEL[next]) {
		SendRes(ch, op, RESULT_LEVEL, 0);
		return;
	}
	if ((long long)ch->GetGold() < TIER_PRICE[next]) {
		SendRes(ch, op, RESULT_GOLD, 0);
		return;
	}
	ch->PointChange(POINT_GOLD, -(int)TIER_PRICE[next], true);
	s.tier = next;
	DBManager::instance().Query("INSERT INTO player.collector_storage (account_id, tier) VALUES (%u, %d) "
			"ON DUPLICATE KEY UPDATE tier=GREATEST(tier, VALUES(tier))", s.account, next);
	char hint[64];
	snprintf(hint, sizeof(hint), "tier %d for %lld", next, TIER_PRICE[next]);
	LogManager::instance().ItemLog(ch, 0, 0, "COLLECTOR EXPAND", hint);
	ch->ChatPacket(CHAT_TYPE_COMMAND, "COLL tier %d %u", s.tier, Capacity(s.tier));
	SendRes(ch, op, RESULT_DONE, 0);
}

}  // namespace

void Command(LPCHARACTER ch, const char* argument)
{
	if (!RealPlayer(ch))
		return;
	char word[32], a1[32], a2[32], a3[32], a4[64];
	const char* rest = one_argument(argument, word, sizeof(word));
	rest = one_argument(rest, a1, sizeof(a1));
	rest = one_argument(rest, a2, sizeof(a2));
	rest = one_argument(rest, a3, sizeof(a3));
	one_argument(rest, a4, sizeof(a4));

	if (!strcmp(word, "open")) {
		Open(ch);
		return;
	}
	if (!strcmp(word, "close")) {
		s_sessions.erase(ch->GetPlayerID());
		return;
	}

	DWORD op = 0;
	if (!*a1 || !str_to_number(op, a1)) {
		SendRes(ch, 0, RESULT_BAD_REQUEST, 0);
		return;
	}
	TSession* s = Live(ch, op);
	if (!s)
		return;

	if (!strcmp(word, "expand")) {
		Expand(ch, *s, op);
		return;
	}

	const int hands = Hands(ch);
	if (hands != RESULT_DONE) {
		SendRes(ch, op, hands, 0);
		return;
	}

	if (!strcmp(word, "put") || !strcmp(word, "putall")) {
		int cell = -1;
		DWORD count = 0;
		if (!*a2 || !str_to_number(cell, a2) || !BagCell(ch, cell)) {
			SendRes(ch, op, RESULT_BAD_REQUEST, 0);
			return;
		}
		LPITEM first = ch->GetInventoryItem(cell);
		if (!first) {
			SendRes(ch, op, RESULT_NO_ITEM, 0);
			return;
		}
		std::vector<DWORD> touched;
		unsigned long long moved = 0;
		int code = RESULT_DONE;
		if (word[3] == '\0') {
			if (*a3)
				str_to_number(count, a3);
			code = PutOne(ch, *s, first, count, touched, moved);
		} else {
			std::string keep;
			if (!strncmp(a3, "keep=", 5))
				keep = a3 + 5;
			const DWORD vnum = first->GetOriginalVnum();
			std::vector<LPITEM> all;
			all.push_back(first);
			for (int c = 0; c < ch->GetInventoryMaxCount(); ++c) {
				LPITEM other = ch->GetInventoryItem(c);
				if (other && other != first && other->GetCell() == c && other->GetOriginalVnum() == vnum && !KeptCell(keep, c))
					all.push_back(other);
			}
			int refused = 0;
			for (LPITEM it : all) {
				const int one = PutOne(ch, *s, it, 0, touched, moved);
				if (one == RESULT_FULL) {
					code = RESULT_FULL;
					break;
				}
				if (one != RESULT_DONE)
					++refused;
			}
			if (code == RESULT_DONE && refused)
				code = moved ? RESULT_PARTIAL : RESULT_REFUSED;
			if (code == RESULT_FULL && moved)
				code = RESULT_PARTIAL;
		}
		SendTouched(ch, *s, touched);
		SendRes(ch, op, code, moved);
		return;
	}

	if (!strcmp(word, "take")) {
		DWORD id = 0, count = 0;
		int cell = -1;
		if (!*a2 || !str_to_number(id, a2) || !*a3 || !str_to_number(count, a3)) {
			SendRes(ch, op, RESULT_BAD_REQUEST, 0);
			return;
		}
		if (*a4)
			str_to_number(cell, a4);
		if (!BagCell(ch, cell))
			cell = -1;
		const int code = Take(ch, *s, op, id, count, cell);
		if (code != RESULT_DONE && code != RESULT_PARTIAL) {
			// The picture back as the server has it (the window took it out).
			auto it = s->entries.find(id);
			if (it != s->entries.end())
				SendSet(ch, it->second);
		}
		SendRes(ch, op, code, 0);
		return;
	}

	SendRes(ch, op, RESULT_BAD_REQUEST, 0);
}

}  // namespace playerbot_collector

ACMD(do_collector)
{
	playerbot_collector::Command(ch, argument);
}
