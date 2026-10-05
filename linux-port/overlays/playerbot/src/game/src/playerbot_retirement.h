#ifndef PLAYERBOT_RETIREMENT_H
#define PLAYERBOT_RETIREMENT_H
// =============================================================================
//  playerbot_retirement.h -- "koniec gry": an operator picks a handful of bots
//  from the middle of the level table, not the best and not the worst, spread
//  over a number of hours. A picked bot walks (or is warped) to a market pitch
//  of its own kingdom, gets off its horse, takes everything off, and puts a
//  stall up with everything it owns - worn pieces first in the sense that they
//  are on the counter, the most valuable lines first - at the price every other
//  bot's stall asks. When the stall has stood for PLAYERBOT_RETIRE_SHOP_MINUTES
//  (or has sold out) the bot logs out and its character is deleted; the same
//  account and identity get a brand new level-1 character.
//
//  What the first version of this file got wrong, and what is done instead:
//   - Despawn() erases from m_mapBots; calling it from inside the Update loop
//     over m_mapBots invalidated the loop's iterator. Now the per-bot hook only
//     marks the bot as CLOSING and ProcessRetirementResets, which runs before
//     the loop, does the despawn.
//   - The db core answers a login from its own player/item cache and writes that
//     cache back over the row later, so a raw SQL reset was undone. Now the
//     reset first sends HEADER_GD_PLAYERBOT_RETIRE_PURGE (db core drops the
//     caches and the Ikarus shop) and only then rewrites the rows.
//   - DELETE ... FROM player.item WHERE owner_id=<pid> also removed the SAFEBOX
//     of the account whose id equals the pid (a safebox's owner_id is the
//     ACCOUNT id). It is now limited to the character's own windows.
//   - The stall was opened wherever the bot stood; the Ikarus shop opens only
//     in a town (CShopManager::CanOpenOnMap) - the bot now goes to its own
//     kingdom's market and walks to a pitch like every other keeper.
//   - Items were priced at a fifth of the asking price and the sign was a
//     custom text; both now follow the ordinary stall (asking price with the
//     demand correction, ChoosePlayerBotShopName).
//   - A bot that could not open a stall within 5 minutes was wiped without
//     selling anything. Now it keeps trying and, after 30 minutes, the
//     retirement is called off (the bot simply plays on) and the slot is given
//     to another bot.
//   - The request journal of the offline shop is drained here, because a
//     retiring bot skips the ordinary AI that used to do it.
//
//  ONE-SHOT, NOT RECURRING: PLAYERBOT_RETIRE_COUNT bots retire once per
//  PLAYERBOT_RETIRE_BATCH_ID. See README.md for the .env values.
//
//  Included from playerbot_manager.cpp right after playerbot_offline_shop.h.
// =============================================================================
#if !defined(PLAYERBOT_ENGINE_MT2009) || !defined(ENABLE_IKASHOP_RENEWAL)
#error "playerbot_retirement.h needs the mt2009 engine with the Ikarus offline shop"
#endif

extern bool NewPlayerTable2(TPlayerTable* table, const char* name, BYTE race,
		BYTE shape, BYTE bEmpire);

namespace {

// MT2009_PLUS_BOT_RETIREMENT_FIX_V1: the cohorts a retirement must never
// touch are declared in fragments included after this one.
bool IsPlayerBotArezzoDungeonCohortPID(DWORD pid);
bool IsPlayerBotTakeoverHold(DWORD dwPlayerID);

// The market town of a kingdom's first village: Shinsoo 1, Chunjo 21, Jinno 41.
long GetPlayerBotRetireHomeMapForEmpire(unsigned int empire)
{
	switch (empire)
	{
		case 1: return 1;
		case 2: return 21;
		case 3: return 41;
	}
	return 0;
}

DWORD s_dwPlayerBotRetireCount = 0;
DWORD s_dwPlayerBotRetireBatchId = 0;
DWORD s_dwPlayerBotRetireWindowMs = 24u * 3600000u;
DWORD s_dwPlayerBotRetireShopMs = 3600000u;
bool  s_bPlayerBotRetireConfigLoaded = false;

// A pick that could not be carried out (no stall in 30 minutes, or the bot
// never logged in) is called off; the scheduler gives its place to another bot.
size_t s_uPlayerBotRetireRequeue = 0;

void LoadPlayerBotRetireConfig()
{
	if (s_bPlayerBotRetireConfigLoaded)
		return;
	s_bPlayerBotRetireConfigLoaded = true;

	const char* count = std::getenv("PLAYERBOT_RETIRE_COUNT");
	if (count && *count)
	{
		int v = std::atoi(count);
		s_dwPlayerBotRetireCount = (DWORD)std::max(0, v);
	}
	const char* batchId = std::getenv("PLAYERBOT_RETIRE_BATCH_ID");
	if (batchId && *batchId)
	{
		int v = std::atoi(batchId);
		s_dwPlayerBotRetireBatchId = (DWORD)std::max(0, v);
	}
	const char* windowHours = std::getenv("PLAYERBOT_RETIRE_WINDOW_HOURS");
	const char* windowMinutes = std::getenv("PLAYERBOT_RETIRE_WINDOW_MINUTES");
	if (windowMinutes && *windowMinutes)
	{
		int m = std::atoi(windowMinutes);
		if (m > 0)
			s_dwPlayerBotRetireWindowMs = (DWORD)std::min<long long>(
					(long long)m * 60000LL, 7LL * 24LL * 3600000LL);
	}
	else if (windowHours && *windowHours)
	{
		int h = std::atoi(windowHours);
		if (h > 0)
			s_dwPlayerBotRetireWindowMs = (DWORD)h * 3600000u;
	}
	const char* shopHours = std::getenv("PLAYERBOT_RETIRE_SHOP_HOURS");
	const char* shopMinutes = std::getenv("PLAYERBOT_RETIRE_SHOP_MINUTES");
	if (shopMinutes && *shopMinutes)
	{
		int m = std::atoi(shopMinutes);
		if (m > 0)
			s_dwPlayerBotRetireShopMs = (DWORD)std::min<long long>(
					(long long)m * 60000LL, 7LL * 24LL * 3600000LL);
	}
	else if (shopHours && *shopHours)
	{
		int h = std::atoi(shopHours);
		if (h > 0)
			s_dwPlayerBotRetireShopMs = (DWORD)h * 3600000u;
	}
	sys_log(0, "PLAYERBOT_RETIRE: count=%u batch_id=%u window_minutes=%u shop_minutes=%u",
			(unsigned int)s_dwPlayerBotRetireCount, (unsigned int)s_dwPlayerBotRetireBatchId,
			(unsigned int)(s_dwPlayerBotRetireWindowMs / 60000u),
			(unsigned int)(s_dwPlayerBotRetireShopMs / 60000u));
}

// SHOPPING   picked; getting to a market pitch, or waiting for shop creation
// SELLING    the offline shop is visible and the bot itself is logged out
// CLOSING    the sale is over; the character is prepared for reset by the next
//            ProcessRetirementResets (never from inside the bot loop)
// DESPAWNED  logged out; waiting for its own last packets to land, then the db
//            core is told to drop its caches
// PURGED     caches dropped; waiting a moment, then the rows are rewritten
enum EPlayerBotRetireStage
{
	PLAYERBOT_RETIRE_SHOPPING,
	PLAYERBOT_RETIRE_SELLING,
	PLAYERBOT_RETIRE_CLOSING,
	PLAYERBOT_RETIRE_DESPAWNED,
	PLAYERBOT_RETIRE_PURGE_SENT,
	PLAYERBOT_RETIRE_PURGED,
};

struct TPlayerBotRetireEntry
{
	EPlayerBotRetireStage stage;
	DWORD dwBatchId;       // only this batch may receive a called-off slot back
	DWORD dwPickedAt;      // give-up clock: a stall must be standing within 30 minutes
	DWORD dwLastSeen;      // last tick the character was in this core's world
	DWORD dwNextAttempt;   // next stall attempt / warp attempt
	DWORD dwSubmittedAt;   // when a create request went to the db core
	DWORD dwStallUntil;    // when the standing stall is closed, sold or not
	DWORD dwPurgeAt;       // DESPAWNED: earliest tick to send the purge packet
	DWORD dwResetAt;       // PURGED: earliest tick to rewrite the rows
	unsigned int uAttempts;      // refused stall attempts (moves the pitch each time)
	unsigned int uUnequipTries;  // passes that could not empty the equipment
	unsigned int uResetFails;
	bool bStallSeen;       // the Ikarus shop of this bot has been seen standing
	std::set<DWORD> setOriginallyWornItemIds; // listed before bag stock

	TPlayerBotRetireEntry() : stage(PLAYERBOT_RETIRE_SHOPPING), dwBatchId(0), dwPickedAt(0), dwLastSeen(0),
			dwNextAttempt(0), dwSubmittedAt(0), dwStallUntil(0), dwPurgeAt(0), dwResetAt(0),
			uAttempts(0), uUnequipTries(0), uResetFails(0), bStallSeen(false) {}
};

std::map<DWORD, TPlayerBotRetireEntry> s_mapPlayerBotRetiring;

const DWORD PLAYERBOT_RETIRE_GIVE_UP_MS = 30u * 60000u;
const DWORD PLAYERBOT_RETIRE_LOST_MS = 30u * 60000u;
const DWORD PLAYERBOT_RETIRE_PURGE_DELAY_MS = 15000;
const DWORD PLAYERBOT_RETIRE_PURGE_RETRY_MS = 5000;
const DWORD PLAYERBOT_RETIRE_RESET_DELAY_MS = 1000;
const DWORD PLAYERBOT_RETIRE_STALL_RETRY_MS = 15000;
const unsigned int PLAYERBOT_RETIRE_MAX_RESET_FAILS = 5;

bool IsPlayerBotRetiring(DWORD dwPlayerID)
{
	return s_mapPlayerBotRetiring.find(dwPlayerID) != s_mapPlayerBotRetiring.end();
}

// A bot that has been taken out of the world for its reset must not be spawned
// again until the rows are new: CPlayerBotManager::Spawn and the top-up ask.
bool IsPlayerBotRetirementHold(DWORD dwPlayerID)
{
	std::map<DWORD, TPlayerBotRetireEntry>::const_iterator it = s_mapPlayerBotRetiring.find(dwPlayerID);
	return it != s_mapPlayerBotRetiring.end() && it->second.stage != PLAYERBOT_RETIRE_SHOPPING;
}

// ---------------------------------------------------------------------------
// The operator's own record, readable with one query:
//
//   SELECT pid, name, level, stage, FROM_UNIXTIME(picked_at) picked,
//          FROM_UNIXTIME(reset_at) reset
//     FROM common.playerbot_retire_pick ORDER BY picked_at DESC;
//
// stage: shopping -> closing -> reset, or aborted / error.
// ---------------------------------------------------------------------------
bool PlayerBotRetireQuery(const char* query)
{
	std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
	if (!msg.get() || msg->uiSQLErrno != 0)
	{
		sys_err("PLAYERBOT_RETIRE: SQL failed: %s", query);
		return false;
	}
	return true;
}

// Recreated identities must return as ordinary new adventurers. Without this
// ledger the operator's fixed medal-farmer selector sees their level 1 and
// consumes almost every fresh life from the end of the registry.
std::set<DWORD> s_setRetiredPlayerBotIdentities;
bool s_bRetiredPlayerBotIdentitiesLoaded = false;

void LoadRetiredPlayerBotIdentities()
{
	if (s_bRetiredPlayerBotIdentitiesLoaded)
		return;
	s_bRetiredPlayerBotIdentitiesLoaded = true;
	std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(
			"SELECT pid FROM common.playerbot_retire_pick WHERE stage='reset'"));
	if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() ||
			!msg->Get()->pSQLResult)
		return;
	MYSQL_ROW row;
	while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
	{
		DWORD pid = 0;
		if (row[0])
			str_to_number(pid, row[0]);
		if (pid != 0)
			s_setRetiredPlayerBotIdentities.insert(pid);
	}
}

// The pids GivePlayerBotRetireStarterKit has looked at in this run.
std::set<DWORD> s_setPlayerBotRetireKitChecked;

bool IsRetiredPlayerBotIdentity(DWORD pid)
{
	LoadRetiredPlayerBotIdentities();
	return s_setRetiredPlayerBotIdentities.find(pid) !=
			s_setRetiredPlayerBotIdentities.end();
}

void RememberRetiredPlayerBotIdentity(DWORD pid)
{
	LoadRetiredPlayerBotIdentities();
	if (pid != 0)
		s_setRetiredPlayerBotIdentities.insert(pid);
}

void EnsurePlayerBotRetirePickTable()
{
	AccountDB::instance().DirectQuery(
			"CREATE TABLE IF NOT EXISTS common.playerbot_retire_pick ("
			"pid INT UNSIGNED NOT NULL PRIMARY KEY, "
			"batch_id INT UNSIGNED NOT NULL, "
			"name VARCHAR(24) NOT NULL, "
			"level TINYINT UNSIGNED NOT NULL, "
			"picked_at INT UNSIGNED NOT NULL, "
			"stage VARCHAR(16) NOT NULL DEFAULT 'shopping', "
			"reset_at INT UNSIGNED NULL)");
	AccountDB::instance().DirectQuery(
			"CREATE TABLE IF NOT EXISTS common.playerbot_retire_event ("
			"id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, "
			"batch_id INT UNSIGNED NOT NULL, pid INT UNSIGNED NOT NULL, "
			"event_time INT UNSIGNED NOT NULL, event_type VARCHAR(32) NOT NULL, "
			"details VARCHAR(255) NOT NULL DEFAULT '', "
			"KEY batch_pid (batch_id,pid), KEY event_time (event_time)) ENGINE=InnoDB");
	AccountDB::instance().DirectQuery(
			"CREATE TABLE IF NOT EXISTS common.playerbot_retire_item ("
			"batch_id INT UNSIGNED NOT NULL, pid INT UNSIGNED NOT NULL, "
			"item_id INT UNSIGNED NOT NULL, vnum INT UNSIGNED NOT NULL, "
			"listed_count INT UNSIGNED NOT NULL, listed_price BIGINT UNSIGNED NOT NULL, "
			"sold_count INT UNSIGNED NOT NULL DEFAULT 0, sold_yang BIGINT UNSIGNED NOT NULL DEFAULT 0, "
			"status VARCHAR(24) NOT NULL DEFAULT 'listed', "
			"listed_at INT UNSIGNED NOT NULL, sold_at INT UNSIGNED NULL, "
			"PRIMARY KEY (batch_id,pid,item_id), KEY pid_status (pid,status)) ENGINE=InnoDB");
}

void AuditPlayerBotRetireEvent(DWORD batchId, DWORD pid, const char* type, const char* details)
{
	char escaped[511];
	size_t out = 0;
	for (const char* c = details ? details : ""; *c && out + 2 < sizeof(escaped); ++c)
	{
		if (*c == '\\' || *c == '\'') escaped[out++] = '\\';
		escaped[out++] = *c;
	}
	escaped[out] = '\0';
	char query[900];
	snprintf(query, sizeof(query),
			"INSERT INTO common.playerbot_retire_event "
			"(batch_id,pid,event_time,event_type,details) VALUES (%u,%u,UNIX_TIMESTAMP(),'%s','%s')",
			batchId, pid, type, escaped);
	AccountDB::instance().AsyncQuery(query);
}

struct TPlayerBotRetireAuditLine
{
	DWORD itemId;
	DWORD vnum;
	DWORD count;
	DWORD price;
};

void AuditPlayerBotRetireListing(DWORD pid, const TPlayerBotRetireEntry& entry,
		const std::vector<TPlayerBotRetireAuditLine>& lines, const char* sign)
{
	char detail[192];
	snprintf(detail, sizeof(detail), "sign=%s lines=%u", sign ? sign : "", (unsigned)lines.size());
	AuditPlayerBotRetireEvent(entry.dwBatchId, pid, "shop_requested", detail);
	for (size_t i = 0; i < lines.size(); ++i)
	{
		char query[640];
		snprintf(query, sizeof(query),
				"INSERT INTO common.playerbot_retire_item "
				"(batch_id,pid,item_id,vnum,listed_count,listed_price,status,listed_at) "
				"VALUES (%u,%u,%u,%u,%u,%u,'listed',UNIX_TIMESTAMP()) "
				"ON DUPLICATE KEY UPDATE vnum=VALUES(vnum),listed_count=VALUES(listed_count),"
				"listed_price=VALUES(listed_price),status='listed',listed_at=VALUES(listed_at)",
				entry.dwBatchId, pid, lines[i].itemId, lines[i].vnum,
				lines[i].count, lines[i].price);
		AccountDB::instance().AsyncQuery(query);
	}
}

void AuditPlayerBotRetireSales(DWORD pid, const TPlayerBotRetireEntry& entry)
{
	auto found = playerbot_offline::sold.find(pid);
	if (found == playerbot_offline::sold.end())
		return;
	for (const auto& line : found->second)
	{
		char query[640];
		snprintf(query, sizeof(query),
				"UPDATE common.playerbot_retire_item SET sold_count=sold_count+%u,"
				"sold_yang=sold_yang+%lld,status=IF(sold_count>=listed_count,'sold','partial'),"
				"sold_at=UNIX_TIMESTAMP() WHERE batch_id=%u AND pid=%u AND item_id=%u",
				(unsigned)line.count, (long long)line.price,
				entry.dwBatchId, pid, (unsigned)line.item);
		AccountDB::instance().AsyncQuery(query);
		snprintf(query, sizeof(query), "item_id=%u vnum=%u count=%u price=%lld",
				(unsigned)line.item, (unsigned)line.vnum, (unsigned)line.count,
				(long long)line.price);
		AuditPlayerBotRetireEvent(entry.dwBatchId, pid, "item_sold", query);
	}
	// The owner is deliberately offline while the shop stands, so the normal
	// bot AI cannot drain this journal. We consumed it into the retirement
	// audit above; keeping it would count the same sale again on every tick.
	playerbot_offline::sold.erase(found);
}

// The in-memory offline-shop journal is normally enough, but a purchase can
// race the tick which closes an expired shop (or a restart can recover the
// owner after that journal disappeared). ikarusshop_log is the engine's
// durable BUY_ITEM record, so reconcile it once during CLOSING before the
// remaining offers are marked unsold and the character is purged.
void ReconcilePlayerBotRetireSales(DWORD pid, const TPlayerBotRetireEntry& entry)
{
	char query[2048];
	snprintf(query, sizeof(query),
			"INSERT INTO common.playerbot_retire_event "
			"(batch_id,pid,event_time,event_type,details) "
			"SELECT %u,%u,UNIX_TIMESTAMP(),'item_sold_reconciled',"
			"CONCAT('item_id=',ri.item_id,' count=',sales.sold_count,' price=',sales.sold_yang) "
			"FROM common.playerbot_retire_item ri JOIN ("
			"SELECT itemid,SUM(count) sold_count,SUM(yang) sold_yang,"
			"MAX(UNIX_TIMESTAMP(time)) sold_at FROM log.ikarusshop_log "
			"WHERE what='BUY_ITEM' AND shop_owner=%u GROUP BY itemid"
			") sales ON sales.itemid=ri.item_id "
			"WHERE ri.batch_id=%u AND ri.pid=%u AND sales.sold_at>=ri.listed_at "
			"AND (ri.sold_count<>sales.sold_count OR ri.sold_yang<>sales.sold_yang "
			"OR ri.status NOT IN ('sold','partial'))",
			entry.dwBatchId, pid, pid, entry.dwBatchId, pid);
	AccountDB::instance().AsyncQuery(query);

	snprintf(query, sizeof(query),
			"UPDATE common.playerbot_retire_item ri JOIN ("
			"SELECT itemid,SUM(count) sold_count,SUM(yang) sold_yang,"
			"MAX(UNIX_TIMESTAMP(time)) sold_at FROM log.ikarusshop_log "
			"WHERE what='BUY_ITEM' AND shop_owner=%u GROUP BY itemid"
			") sales ON sales.itemid=ri.item_id SET "
			"ri.sold_count=sales.sold_count,ri.sold_yang=sales.sold_yang,"
			"ri.status=IF(sales.sold_count>=ri.listed_count,'sold','partial'),"
			"ri.sold_at=sales.sold_at WHERE ri.batch_id=%u AND ri.pid=%u "
			"AND sales.sold_at>=ri.listed_at",
			pid, entry.dwBatchId, pid);
	AccountDB::instance().AsyncQuery(query);
}

void AuditPlayerBotRetireRemaining(DWORD pid, const TPlayerBotRetireEntry& entry)
{
	NativeShop shop = ikashop::GetManager().GetShopByOwnerID(pid);
	if (shop)
	{
		for (const auto& line : shop->GetItems())
		{
			char query[384];
			snprintf(query, sizeof(query),
					"UPDATE common.playerbot_retire_item SET "
					"status=IF(sold_count>0,'partial','unsold') "
					"WHERE batch_id=%u AND pid=%u AND item_id=%u",
					entry.dwBatchId, pid, (unsigned)line.first);
			AccountDB::instance().AsyncQuery(query);
		}
	}
	else
	{
		char query[320];
		snprintf(query, sizeof(query),
				"UPDATE common.playerbot_retire_item SET status='missing_at_close' "
				"WHERE batch_id=%u AND pid=%u AND status='listed'",
				entry.dwBatchId, pid);
		AccountDB::instance().AsyncQuery(query);
	}
}

void SetPlayerBotRetireStage(DWORD dwPlayerID, const char* stage)
{
	char query[192];
	snprintf(query, sizeof(query),
			"UPDATE common.playerbot_retire_pick SET stage='%s' WHERE pid=%u", stage, dwPlayerID);
	AccountDB::instance().AsyncQuery(query);
}

bool RecordPlayerBotRetirePick(DWORD dwPlayerID, DWORD dwBatchId, const char* pszName, BYTE bLevel)
{
	char escapedName[2 * PLAYER_NAME_MAX_LEN + 1];
	size_t o = 0;
	for (const char* p = pszName; *p && o + 2 < sizeof(escapedName); ++p)
	{
		if (*p == '\'' || *p == '\\')
			escapedName[o++] = '\\';
		escapedName[o++] = *p;
	}
	escapedName[o] = '\0';

	char query[512];
	snprintf(query, sizeof(query),
			"INSERT INTO common.playerbot_retire_pick (pid, batch_id, name, level, picked_at, stage) "
			"VALUES (%u, %u, '%s', %u, UNIX_TIMESTAMP(), 'shopping') "
			"ON DUPLICATE KEY UPDATE batch_id=VALUES(batch_id), name=VALUES(name), "
			"level=VALUES(level), picked_at=VALUES(picked_at), stage='shopping', reset_at=NULL",
			dwPlayerID, dwBatchId, escapedName, (unsigned int)bLevel);
	return PlayerBotRetireQuery(query);
}

// Called once, on the first tick, whatever the current .env says: a restart in
// the middle of a batch (an update, a crash) must not leave a bot stranded in
// 'shopping' or 'closing' for ever.
void RecoverPlayerBotRetirements(DWORD dwNow)
{
	EnsurePlayerBotRetirePickTable();
	// MT2009_PLUS_BOT_RETIREMENT_FIX_V1: with the kingdoms' towns on different
	// cores (M2_PLAYERBOT_WORLD_LAYOUT=split) every one of them runs the
	// retirement for its own bots, so each takes up only the picks of the
	// kingdoms it hosts - a pick of another core's bot would be "lost" here
	// after 30 minutes and called off under its owner's feet.
	std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(
			"SELECT r.pid, r.batch_id, r.stage, COALESCE(pi.empire,0) "
			"FROM common.playerbot_retire_pick r "
			"LEFT JOIN player.player p ON p.id=r.pid "
			"LEFT JOIN player.player_index pi ON pi.id=p.account_id "
			"WHERE r.stage IN ('shopping','selling','closing')"));
	if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
		return;
	MYSQL_ROW row;
	unsigned int recovered = 0;
	while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
	{
		DWORD pid = 0;
		if (row[0])
			str_to_number(pid, row[0]);
		if (!pid || !row[1])
			continue;
		unsigned int empire = 0;
		if (row[3])
			str_to_number(empire, row[3]);
		const long home = GetPlayerBotRetireHomeMapForEmpire(empire);
		if (!IsPlayerBotMapHostedHere(home != 0 ? home : 1))
			continue;
		TPlayerBotRetireEntry entry;
		if (row[1])
			str_to_number(entry.dwBatchId, row[1]);
		entry.dwPickedAt = dwNow;
		entry.dwLastSeen = dwNow;
		if (row[2] && 0 == strcmp(row[2], "selling"))
		{
			entry.stage = PLAYERBOT_RETIRE_SELLING;
			entry.bStallSeen = true;
			entry.dwStallUntil = dwNow + s_dwPlayerBotRetireShopMs;
			AuditPlayerBotRetireEvent(entry.dwBatchId, pid, "recovered_selling",
					"server restarted while offline shop was standing");
		}
		else if (row[2] && 0 == strcmp(row[2], "closing"))
		{
			AuditPlayerBotRetireRemaining(pid, entry);
			AuditPlayerBotRetireEvent(entry.dwBatchId, pid, "recovered_closing",
					"server restarted after the sale ended");
			entry.stage = PLAYERBOT_RETIRE_DESPAWNED;
			entry.dwPurgeAt = dwNow + PLAYERBOT_RETIRE_PURGE_DELAY_MS;
		}
		s_mapPlayerBotRetiring[pid] = entry;
		++recovered;
	}
	if (recovered)
		sys_log(0, "PLAYERBOT_RETIRE: recovered %u unfinished retirements after a restart", recovered);
}

// Called off, not carried out: the bot plays on and another one takes its place.
void AbortPlayerBotRetirement(DWORD dwPlayerID, const char* reason)
{
	std::map<DWORD, TPlayerBotRetireEntry>::const_iterator found =
			s_mapPlayerBotRetiring.find(dwPlayerID);
	const DWORD batchId = found == s_mapPlayerBotRetiring.end() ? 0 : found->second.dwBatchId;
	s_mapPlayerBotRetiring.erase(dwPlayerID);
	SetPlayerBotRetireStage(dwPlayerID, "aborted");
	// A recovered pick may belong to an older batch. It must never take a slot
	// away from the batch currently selected in .env.
	if (batchId != 0 && batchId == s_dwPlayerBotRetireBatchId)
		++s_uPlayerBotRetireRequeue;
	AuditPlayerBotRetireEvent(batchId, dwPlayerID, "aborted", reason);
	sys_log(0, "PLAYERBOT_RETIRE: pid=%u retirement called off: %s", dwPlayerID, reason);
}

// ---------------------------------------------------------------------------
// The character rows. Everything below runs only after the db core has dropped
// its caches (SendPlayerBotRetirePurge) and the character has been logged out.
// ---------------------------------------------------------------------------
void SendPlayerBotRetirePurge(DWORD dwPlayerID)
{
	TPacketGDPlayerBotRetirePurge packet;
	packet.dwPID = dwPlayerID;
	packet.dwAccountID = 0;
	char query[256];
	snprintf(query, sizeof(query),
			"SELECT account_id FROM player.player WHERE id=%u", dwPlayerID);
	std::unique_ptr<SQLMsg> selected(AccountDB::instance().DirectQuery(query));
	MYSQL_ROW row = (selected.get() && selected->uiSQLErrno == 0 && selected->Get() &&
			selected->Get()->pSQLResult) ? mysql_fetch_row(selected->Get()->pSQLResult) : NULL;
	if (row && row[0])
		str_to_number(packet.dwAccountID, row[0]);
	db_clientdesc->DBPacket(HEADER_GD_PLAYERBOT_RETIRE_PURGE, 0, &packet, sizeof(packet));
}

bool ResetRetiredPlayerBotRow(DWORD dwPlayerID)
{
	// Keep only the identity the bot registry needs; everything else about the
	// old character is regenerated through the engine's own new-character
	// initializer (level-1 stats, the kingdom's start point).
	char query[1024];
	snprintf(query, sizeof(query),
			"SELECT p.account_id,p.name,p.job,p.part_base,pi.empire "
			"FROM player.player p JOIN player.player_index pi ON pi.id=p.account_id "
			"WHERE p.id=%u", dwPlayerID);
	std::unique_ptr<SQLMsg> selected(AccountDB::instance().DirectQuery(query));
	MYSQL_ROW row = (selected.get() && selected->uiSQLErrno == 0 && selected->Get() &&
			selected->Get()->pSQLResult) ? mysql_fetch_row(selected->Get()->pSQLResult) : NULL;
	if (!row)
	{
		sys_err("PLAYERBOT_RETIRE: pid=%u cannot be recreated: identity row missing", dwPlayerID);
		return false;
	}

	DWORD accountID = 0;
	unsigned race = 0, shape = 0, empire = 0;
	str_to_number(accountID, row[0]);
	char name[CHARACTER_NAME_MAX_LEN + 1];
	strlcpy(name, row[1] ? row[1] : "", sizeof(name));
	str_to_number(race, row[2]);
	str_to_number(shape, row[3]);
	str_to_number(empire, row[4]);
	if (!accountID || !*name || empire < 1 || empire > 3)
	{
		sys_err("PLAYERBOT_RETIRE: pid=%u has invalid identity account=%u empire=%u",
				dwPlayerID, accountID, empire);
		return false;
	}

	TPlayerTable fresh;
	if (!NewPlayerTable2(&fresh, name, (BYTE)race, (BYTE)shape, (BYTE)empire))
	{
		sys_err("PLAYERBOT_RETIRE: pid=%u NewPlayerTable2 refused race=%u shape=%u empire=%u",
				dwPlayerID, race, shape, empire);
		return false;
	}

	char escaped[2 * CHARACTER_NAME_MAX_LEN + 1];
	size_t out = 0;
	for (const char* c = name; *c && out + 2 < sizeof(escaped); ++c)
	{
		if (*c == '\\' || *c == '\'') escaped[out++] = '\\';
		escaped[out++] = *c;
	}
	escaped[out] = '\0';

	if (!PlayerBotRetireQuery("START TRANSACTION"))
		return false;
	bool ok = true;
	// player.item.owner_id is the PID for the character's own windows and the
	// ACCOUNT id for SAFEBOX/MALL: without the window filter this used to delete
	// the safebox of whichever account happened to have the same number. The
	// list is the one PLAYER_DELETE uses, plus the two Ikarus windows.
	static const char* const wipes[] = {
		"DELETE FROM player.item WHERE owner_id=%u AND `window` IN ("
			"'INVENTORY','EQUIPMENT','DRAGON_SOUL_INVENTORY','BELT_INVENTORY',"
			"'IKASHOP_OFFLINESHOP','IKASHOP_SAFEBOX')",
		"DELETE FROM player.ikashop_offlineshop WHERE owner=%u",
		"DELETE FROM player.ikashop_safebox WHERE owner=%u",
		"DELETE FROM player.ikashop_notification WHERE owner=%u",
		"DELETE FROM player.ikashop_private_offer WHERE seller=%u OR buyer=%u",
		"DELETE FROM player.ikashop_auction_offer WHERE seller=%u OR buyer=%u",
		"DELETE FROM player.myshop_pricelist WHERE owner_id=%u",
		"DELETE FROM player.quest WHERE dwPID=%u",
		"DELETE FROM player.affect WHERE dwPID=%u",
		"DELETE FROM player.player_special_flag WHERE pid=%u",
		"DELETE FROM player.messenger_block_list WHERE pid=%u",
		"DELETE FROM player.marriage WHERE pid1=%u OR pid2=%u",
		"DELETE FROM player.player WHERE id=%u",
	};
	for (size_t i = 0; ok && i < sizeof(wipes) / sizeof(wipes[0]); ++i)
	{
		snprintf(query, sizeof(query), wipes[i], dwPlayerID, dwPlayerID);
		ok = PlayerBotRetireQuery(query);
	}
	if (ok)
	{
		// The ordinary safebox belongs to the account, while character windows
		// belong to the PID. A new life must not inherit the old account chest.
		snprintf(query, sizeof(query),
				"DELETE FROM player.item WHERE owner_id=%u AND `window`='SAFEBOX'",
				accountID);
		ok = PlayerBotRetireQuery(query);
	}
	if (ok)
	{
		// The engine's own INSERT (CClientManager::__QUERY_PLAYER_CREATE) sets
		// part_main to the shape too; everything else is the column's default.
		snprintf(query, sizeof(query),
				"INSERT INTO player.player "
				"(id,account_id,name,level,st,ht,dx,iq,job,voice,dir,x,y,z,hp,mp,stamina,"
				"part_base,part_main,part_hair,part_acce,gold,playtime,exp,stat_point) VALUES "
				"(%u,%u,'%s',1,%u,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%d,%u,%u,0,0,0,0,0,0)",
				dwPlayerID, accountID, escaped,
				(unsigned)fresh.st, (unsigned)fresh.ht, (unsigned)fresh.dx, (unsigned)fresh.iq,
				(unsigned)fresh.job, (unsigned)fresh.voice, (unsigned)fresh.dir,
				(int)fresh.x, (int)fresh.y, (int)fresh.z, (int)fresh.hp, (int)fresh.sp, (int)fresh.stamina,
				(unsigned)fresh.part_base, (unsigned)fresh.part_base);
		ok = PlayerBotRetireQuery(query);
	}
	// No item rows here. This used to INSERT the seed's five-slot starter set
	// with no id, so MariaDB numbered them MAX(id)+1 - inside the range the db
	// core hands the game cores for new items, while the server runs. The next
	// item the world made took the same number, and the db core's cache wrote
	// it over the starter row: recreated bots came back with no weapon and no
	// chest and wandered without a target (26 September). The set now comes
	// through the engine, which numbers items itself, on the bot's first tick
	// with its bag loaded (GivePlayerBotRetireStarterKit).
	if (ok)
	{
		// The starter set comes from GivePlayerBotRetireStarterKit.
		// Mark both login rewards claimed so the first quest login cannot duplicate the kit.
		snprintf(query, sizeof(query),
				"INSERT INTO player.quest (dwPID,szName,szState,lValue) VALUES "
				"(%u,'give_basic_weapon','basic_weapon',1),"
				"(%u,'starter_chest','given',1) "
				"ON DUPLICATE KEY UPDATE lValue=GREATEST(lValue,VALUES(lValue))",
				dwPlayerID, dwPlayerID);
		ok = PlayerBotRetireQuery(query);
	}
	if (!ok)
	{
		PlayerBotRetireQuery("ROLLBACK");
		return false;
	}
	if (!PlayerBotRetireQuery("COMMIT"))
	{
		PlayerBotRetireQuery("ROLLBACK");
		return false;
	}

	snprintf(query, sizeof(query),
			"UPDATE common.playerbot_retire_pick SET stage='reset',reset_at=UNIX_TIMESTAMP() "
			"WHERE pid=%u", dwPlayerID);
	AccountDB::instance().AsyncQuery(query);
	// The new character is looked at again for its starter set, whatever the
	// old one was found to have earlier in this run.
	s_setPlayerBotRetireKitChecked.erase(dwPlayerID);
	sys_log(0, "PLAYERBOT_RETIRE: recreated pid=%u (%s) at level 1", dwPlayerID, name);
	return true;
}

// The seed's starter set (playerbots_seed.sql) for a recreated character: the
// class's first weapon and armour, worn, 200 red and 200 blue potions and the
// class's starter chest. Given through AutoGiveItem, so the engine numbers the
// items (see ResetRetiredPlayerBotRow for what a raw INSERT did).
//
// Once per pid and run, on the first tick with the bag loaded, and only to a
// recreated identity that has neither a weapon nor a starter chest - which is
// also what a bot recreated by an older version of this file looks like, so
// those get their set on the next start as well. A bot that has either is left
// alone; the ordinary AI puts on or opens what it has.
void GivePlayerBotRetireStarterKit(LPCHARACTER ch)
{
	const DWORD pid = ch->GetPlayerID();
	if (!ch->IsItemLoaded() || ch->IsDead() ||
			s_setPlayerBotRetireKitChecked.find(pid) != s_setPlayerBotRetireKitChecked.end())
		return;
	s_setPlayerBotRetireKitChecked.insert(pid);
	if (!IsRetiredPlayerBotIdentity(pid) || ch->GetLevel() > 10 || ch->GetWear(WEAR_WEAPON))
		return;

	const BYTE job = ch->GetJob();
	const DWORD weapon = (job == JOB_ASSASSIN) ? 1000 : (job == JOB_SHAMAN ? 7000 : 10);
	const DWORD armor = (job == JOB_WARRIOR) ? 11200 :
			(job == JOB_ASSASSIN ? 11400 : (job == JOB_SURA ? 11600 : 11800));
	const DWORD chest = (job == JOB_ASSASSIN) ? 50212 : (job == JOB_SHAMAN ? 50213 : 50187);
	for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
	{
		LPITEM item = ch->GetInventoryItem(cell);
		if (!item)
			continue;
		const DWORD vnum = item->GetVnum();
		if (item->GetType() == ITEM_WEAPON || vnum == chest || vnum == 50187 ||
				vnum == 50212 || vnum == 50213)
			return;
	}

	int given = 0;
	LPITEM piece = ch->AutoGiveItem(weapon, 1, -1, false);
	if (piece && piece->GetOwner() == ch && piece->GetWindow() == INVENTORY)
	{
		PlayerBotEquipItem(ch, piece);
		++given;
	}
	if (!ch->GetWear(WEAR_BODY))
	{
		piece = ch->AutoGiveItem(armor, 1, -1, false);
		if (piece && piece->GetOwner() == ch && piece->GetWindow() == INVENTORY)
		{
			PlayerBotEquipItem(ch, piece);
			++given;
		}
	}
	if (ch->CountSpecifyItem(27001) == 0 && ch->AutoGiveItem(27001, 200, -1, false))
		++given;
	if (ch->CountSpecifyItem(27004) == 0 && ch->AutoGiveItem(27004, 200, -1, false))
		++given;
	if (ch->AutoGiveItem(chest, 1, -1, false))
		++given;
	ch->Save();
	sys_log(0, "PLAYERBOT_RETIRE: starter set given pid=%u name=%s job=%u pieces=%d weapon_worn=%d",
			pid, ch->GetName(), (unsigned int)job, given, ch->GetWear(WEAR_WEAPON) ? 1 : 0);
}

// The live side: what the character itself still does between now and
// Despawn() must not write anything back. Runs once, from
// ProcessRetirementResets, right before the despawn.
void WipePlayerBotForRetirement(LPCHARACTER ch)
{
	// Leave the guild properly (async, through the db core). A guild's leader
	// cannot leave; that pid keeps its guild membership, logged so it is no
	// silent gap. The scheduler never picks a leader.
	if (CGuild* pGuild = ch->GetGuild())
	{
		if (!pGuild->RequestRemoveMember(ch->GetPlayerID()))
			sys_log(0, "PLAYERBOT_RETIRE: pid=%u is its guild's leader -- not removing "
					"it from the guild, only resetting its character", ch->GetPlayerID());
	}

	ch->SkipFutureSaves();

	// SetCount(0) detaches the item and destroys it through the ordinary path
	// (the db core deletes the row and the cache entry).
	for (BYTE w = 0; w < WEAR_MAX_NUM; ++w)
	{
		LPITEM wear = ch->GetWear(w);
		if (wear)
			wear->SetCount(0);
	}
	for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
	{
		LPITEM item = ch->GetInventoryItem(cell);
		if (item)
			item->SetCount(0);
	}
}

// ---------------------------------------------------------------------------
// The stall.
// ---------------------------------------------------------------------------
// The market town of a kingdom's first village: Shinsoo 1, Chunjo 21, Jinno 41.
long GetPlayerBotRetireHomeMap(LPCHARACTER ch)
{
	return GetPlayerBotRetireHomeMapForEmpire(ch->GetEmpire());
}

// The asking price of the ordinary stall (CollectPlayerBotShopItems' fill loop
// in playerbot_town.h) - asking price, the kind's shortage markup
// (GetPlayerBotListingPrice, which replaced the demand correction in base
// 2.2.36), never under the listing floor - without the two markdowns that only
// mean something to a bot that keeps a stall for a living.
DWORD GetPlayerBotRetirementPrice(LPITEM item, DWORD dwNow)
{
	(void)dwNow;
	const DWORD price = GetPlayerBotListingPrice(item, GetPlayerBotShopAskingPrice(item), 0);
	return std::max<DWORD>(price, GetPlayerBotListingFloor(item));
}

bool IsPlayerBotRetirementPotion(DWORD vnum)
{
	// Same hard exclusion used by CollectPlayerBotShopItems for red/blue HP/MP
	// potions, plus the green/purple personal reserves rejected by the scorer.
	return vnum == 27001 || vnum == 27002 || vnum == 27003 || vnum == 27051 ||
			vnum == 27004 || vnum == 27005 || vnum == 27006 || vnum == 27052 ||
			IsPlayerBotPersonalBuffPotion(vnum);
}

enum EPlayerBotRetireStallResult
{
	PLAYERBOT_RETIRE_STALL_WAIT,   // not there yet / refused / submitted: look again
	PLAYERBOT_RETIRE_STALL_EMPTY,  // nothing at all that could be sold
};

// One step towards a standing stall. Called on every tick the bot is safe to
// steer and has no stall and no request in flight.
EPlayerBotRetireStallResult AdvancePlayerBotRetirementStall(LPCHARACTER ch,
		TPlayerBotAIState& state, TPlayerBotRetireEntry& entry, DWORD dwNow)
{
	const DWORD pid = ch->GetPlayerID();

	// 1. A town of the bot's own kingdom, with a stall ring, on this core. The
	// Ikarus shop opens only on maps CShopManager::CanOpenOnMap knows.
	const long mapIndex = ch->GetMapIndex();
	long pitchX = 0, pitchY = 0;
	const bool inMarket = GetPlayerBotShopCentre(mapIndex, pitchX, pitchY) &&
			playerbot_empire_rules::GetMapOwnerEmpire(mapIndex) == (int)ch->GetEmpire() &&
			IsPlayerBotMapHostedHere(mapIndex);
	if (!inMarket)
	{
		if (dwNow < entry.dwNextAttempt)
			return PLAYERBOT_RETIRE_STALL_WAIT;
		entry.dwNextAttempt = dwNow + PLAYERBOT_RETIRE_STALL_RETRY_MS;
		const long home = GetPlayerBotRetireHomeMap(ch);
		long homeX = 0, homeY = 0;
		if (home == 0 || !GetPlayerBotShopCentre(home, homeX, homeY) || !IsPlayerBotMapHostedHere(home))
		{
			PlayerBotLogThrottled("retire_no_home", dwNow,
					"PLAYERBOT_RETIRE: pid=%u no market map for empire=%u on this core",
					pid, (unsigned int)ch->GetEmpire());
			return PLAYERBOT_RETIRE_STALL_WAIT;
		}
		if (!TransitionPlayerBotMap(ch, state, home, homeX, homeY, dwNow, "retirement_stall"))
			sys_log(0, "PLAYERBOT_RETIRE: pid=%u could not warp to market map %ld", pid, home);
		return PLAYERBOT_RETIRE_STALL_WAIT;
	}

	// 2. A pitch on the ring, the way every keeper picks one: stable per bot,
	// and a different one after every refusal (a neighbour too close, the map's
	// stall limit) so a refused spot is not asked for again.
	SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
	long offsetX = 0, offsetY = 0;
	GetPlayerBotStableOffset(pid + (DWORD)entry.uAttempts * 7919U,
			0x4d4b5450U ^ (DWORD)entry.uAttempts,
			PLAYERBOT_SHOP_RING_MIN, PLAYERBOT_SHOP_RING_RADIUS, offsetX, offsetY);
	long stallX = pitchX + offsetX;
	long stallY = pitchY + offsetY;
	{
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(mapIndex);
		if (navigation.Init(mapIndex))
		{
			int tries = 0;
			while (tries < PLAYERBOT_SHOP_PITCH_TRIES &&
					!navigation.CanReach(ch->GetX(), ch->GetY(), stallX, stallY))
			{
				++tries;
				GetPlayerBotStableOffset(pid + (DWORD)(entry.uAttempts + tries) * 7919U,
						0x4d4b5450U ^ (DWORD)(entry.uAttempts + tries),
						PLAYERBOT_SHOP_RING_MIN, PLAYERBOT_SHOP_RING_RADIUS, offsetX, offsetY);
				stallX = pitchX + offsetX;
				stallY = pitchY + offsetY;
			}
			if (!navigation.CanReach(ch->GetX(), ch->GetY(), stallX, stallY))
			{
				++entry.uAttempts;
				entry.dwNextAttempt = dwNow + 60000;
				ClearPlayerBotRoute(state, true);
				return PLAYERBOT_RETIRE_STALL_WAIT;
			}
		}
	}
	if (!MovePlayerBotTownLeg(ch, state, dwNow, stallX, stallY, PLAYERBOT_MARKET_ARRIVE))
		return PLAYERBOT_RETIRE_STALL_WAIT; // still walking

	// 3. On the pitch. SubmitPlayerBotOfflineShop refuses in the first two
	// minutes after a spawn; and a refusal is retried on a clock, not every tick.
	if (dwNow - state.dwSpawnTime < 120000 || dwNow < entry.dwNextAttempt)
		return PLAYERBOT_RETIRE_STALL_WAIT;
	entry.dwNextAttempt = dwNow + PLAYERBOT_RETIRE_STALL_RETRY_MS;

	// 4. The rider and the party. OpenMyShop refuses a character that is on its
	// horse; StopRiding() puts the horse down beside the bot and HorseSummon(false)
	// sends it away, exactly what the ordinary stall does. A bot still in the
	// saddle after that is not sent on (and says so in the log).
	if (ch->GetParty())
		ch->GetParty()->Quit(pid);
	ch->SetVictim(NULL);
	ch->Stop();
	if (ch->IsRiding())
		ch->StopRiding();
	ch->HorseSummon(false);
	if (ch->IsHorseRiding())
	{
		PlayerBotLogThrottled("retire_riding", dwNow,
				"PLAYERBOT_RETIRE: pid=%u name=%s still riding after StopRiding, trying again",
				pid, ch->GetName());
		return PLAYERBOT_RETIRE_STALL_WAIT;
	}

	// 5. The shop bundle, on the house, asked for BEFORE the equipment lands in
	// the bag: a bag with one free cell would otherwise have none left for it.
	if (ch->CountSpecifyItem(50200) == 0)
	{
		if (ch->GetEmptyInventory(1) < 0)
			MergePlayerBotStacks(ch, PLAYERBOT_STACK_MERGES_PER_PASS * 2);
		if (ch->GetEmptyInventory(1) < 0)
		{
			PlayerBotLogThrottled("retire_no_bundle_room", dwNow,
					"PLAYERBOT_RETIRE: pid=%u name=%s no room for the shop bundle, trying again",
					pid, ch->GetName());
			return PLAYERBOT_RETIRE_STALL_WAIT;
		}
		ch->AutoGiveItem(50200, 1);
	}

	// 6. Everything worn comes off, so that it can go on the counter. A bag with
	// no room for it is tidied and asked again; after four passes the stall is
	// opened with what is in the bag rather than never.
	for (BYTE w = 0; w < WEAR_MAX_NUM; ++w)
	{
		LPITEM wear = ch->GetWear(w);
		if (wear)
		{
			entry.setOriginallyWornItemIds.insert(wear->GetID());
			ch->UnequipItem(wear);
		}
	}
	unsigned int stillWorn = 0;
	for (BYTE w = 0; w < WEAR_MAX_NUM; ++w)
		if (ch->GetWear(w))
			++stillWorn;
	if (stillWorn > 0 && entry.uUnequipTries < 4)
	{
		++entry.uUnequipTries;
		MergePlayerBotStacks(ch, PLAYERBOT_STACK_MERGES_PER_PASS * 2);
		PlayerBotLogThrottled("retire_unequip", dwNow,
				"PLAYERBOT_RETIRE: pid=%u name=%s %u pieces would not come off (pass %u), trying again",
				pid, ch->GetName(), stillWorn, entry.uUnequipTries);
		return PLAYERBOT_RETIRE_STALL_WAIT;
	}

	// 7. The counter. Eligibility comes from the exact same policy function as
	// an ordinary playerbot shop. This rejects potions (including 27101/27104),
	// KEEP/MERCHANT/DROP stock and every other item the configured bot economy
	// does not allow on a stall. Within that allowed set, pieces removed from
	// the character go first, then the most valuable bag stock.
	RefreshPlayerBotItemPolicy(dwNow);
	typedef std::pair<std::pair<int, DWORD>, WORD> TRetireCandidate;
	std::vector<TRetireCandidate> candidates;
	for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
	{
		LPITEM item = ch->GetInventoryItem(cell);
		if (!item || item->IsEquipped() || item->isLocked() || item->GetVnum() == 50200)
			continue;
		const TItemTable* proto = item->GetProto();
		if (!proto || IS_SET(proto->dwAntiFlags, ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_MYSHOP))
			continue;
		if (IsPlayerBotRetirementPotion(item->GetVnum()) ||
				ScorePlayerBotShopStock(ch, item, false, false) <= 0)
			continue;
		const int wasWorn = entry.setOriginallyWornItemIds.find(item->GetID()) !=
				entry.setOriginallyWornItemIds.end() ? 1 : 0;
		candidates.push_back(std::make_pair(
				std::make_pair(wasWorn, GetPlayerBotShopAskingPrice(item)), cell));
	}
	if (candidates.empty())
	{
		sys_log(0, "PLAYERBOT_RETIRE: pid=%u had nothing to put up", pid);
		return PLAYERBOT_RETIRE_STALL_EMPTY;
	}
	std::sort(candidates.begin(), candidates.end(), std::greater<TRetireCandidate>());

	bool grid[PLAYERBOT_SHOP_GRID_CELLS];
	memset(grid, 0, sizeof(grid));
	TShopItemTable table[PLAYERBOT_SHOP_GRID_CELLS];
	memset(table, 0, sizeof(table));
	BYTE tableCount = 0;
	long long total = ch->GetGold();
	std::vector<LPITEM> signGoods;
	std::vector<TPlayerBotRetireAuditLine> auditLines;
	for (size_t i = 0; i < candidates.size() && tableCount < PLAYERBOT_SHOP_GRID_CELLS; ++i)
	{
		const WORD cell = candidates[i].second;
		LPITEM item = ch->GetInventoryItem(cell);
		if (!item)
			continue;
		const int height = std::max<int>(1, std::min<int>(PLAYERBOT_SHOP_GRID_ROWS, item->GetSize()));
		const int slot = FindPlayerBotShopSlot(grid, height);
		if (slot < 0)
			continue;
		const int displayPos = PlayerBotShopSlotToEngine(slot);
		// SubmitPlayerBotOfflineShop refuses the WHOLE shop over one line the
		// engine would not take; the same test, made line by line here.
		if (!BotOfflineValid(ch, item, displayPos))
			continue;
		const DWORD price = GetPlayerBotRetirementPrice(item, dwNow);
		if (price == 0 || price >= GOLD_MAX || total + (long long)price >= (long long)GOLD_MAX - 1)
			continue;
		total += price;
		PutPlayerBotShopSlot(grid, slot, height);

		table[tableCount].vnum = item->GetVnum();
		table[tableCount].count = item->GetCount();
		table[tableCount].pos = TItemPos(INVENTORY, cell);
		table[tableCount].price = price;
		table[tableCount].display_pos = (BYTE)displayPos;
		++tableCount;
		signGoods.push_back(item);
		TPlayerBotRetireAuditLine audit = { item->GetID(), item->GetVnum(),
				item->GetCount(), price };
		auditLines.push_back(audit);
	}
	if (tableCount == 0)
	{
		// Something was there but none of it fits the counter right now: not a
		// reason to destroy it unsold. Another spot, another try.
		++entry.uAttempts;
		sys_log(0, "PLAYERBOT_RETIRE: pid=%u no line of %u candidates could be placed, trying again",
				pid, (unsigned int)candidates.size());
		return PLAYERBOT_RETIRE_STALL_WAIT;
	}

	// The sign is one of Iwakura's names, as for every other stall.
	char sign[SHOP_SIGN_MAX_LEN + 1];
	const char* how = "none";
	if (!ChoosePlayerBotShopName(ch, signGoods, sign, sizeof(sign), &how))
		strlcpy(sign, playerbot_shop_names::SIGN_NAMES[0].szName, sizeof(sign));

	SubmitPlayerBotOfflineShop(ch, state, dwNow, sign, table, tableCount);
	// SubmitPlayerBotOfflineShop returns false even after a good submission (the
	// stall is an independent entity); the journal and the owner map say what
	// really happened.
	if (!HasPlayerBotOfflineShop(ch))
	{
		++entry.uAttempts;
		PlayerBotLogThrottled("retire_refused", dwNow,
				"PLAYERBOT_RETIRE: pid=%u name=%s stall refused (map=%ld lines=%u attempt=%u), trying again",
				pid, ch->GetName(), mapIndex, (unsigned int)tableCount, entry.uAttempts);
		return PLAYERBOT_RETIRE_STALL_WAIT;
	}
	AuditPlayerBotRetireListing(pid, entry, auditLines, sign);
	entry.dwSubmittedAt = dwNow;
	sys_log(0, "PLAYERBOT_RETIRE: pid=%u name=%s stall requested map=%ld lines=%u total_asking=%lld",
			pid, ch->GetName(), mapIndex, (unsigned int)tableCount, total - ch->GetGold());
	return PLAYERBOT_RETIRE_STALL_WAIT;
}

void BeginPlayerBotRetirementClose(DWORD dwPlayerID, TPlayerBotRetireEntry& entry, const char* reason)
{
	entry.stage = PLAYERBOT_RETIRE_CLOSING;
	SetPlayerBotRetireStage(dwPlayerID, "closing");
	AuditPlayerBotRetireEvent(entry.dwBatchId, dwPlayerID, "sale_over", reason);
	sys_log(0, "PLAYERBOT_RETIRE: pid=%u sale over (%s)", dwPlayerID, reason);
}

// A pick worth acting on: alive, standing where the plain "warp to my own
// town" works, and not tied up in anything the retirement would break.
// MT2009_PLUS_BOT_RETIREMENT_WIDEN_V1: why the last look refused its bots, for
// the "no bot can be picked" line (offline, busy, party, guild master, stall, other).
unsigned int s_auPlayerBotRetireRefused[6];

bool IsPlayerBotRetirementCandidateInner(LPCHARACTER ch, BYTE bLevelLo, BYTE bLevelHi);

bool IsPlayerBotRetirementCandidate(LPCHARACTER ch, BYTE bLevelLo, BYTE bLevelHi)
{
	const bool ok = IsPlayerBotRetirementCandidateInner(ch, bLevelLo, bLevelHi);
	if (!ok)
	{
		int why = 5;
		if (!ch)
			why = 0;
		else if (ch->GetExchange() || ch->GetShop() || ch->GetSafebox() || ch->IsBusy() || ch->IsDead())
			why = 1;
		else if (ch->GetParty())
			why = 2;
		else if (ch->GetGuild() && ch->GetGuild()->GetMasterPID() == ch->GetPlayerID())
			why = 3;
		else if (HasPlayerBotOfflineShop(ch))
			why = 4;
		++s_auPlayerBotRetireRefused[why];
	}
	return ok;
}

bool IsPlayerBotRetirementCandidateInner(LPCHARACTER ch, BYTE bLevelLo, BYTE bLevelHi)
{
	if (!ch || ch->IsDead() || ch->IsStun() || !ch->IsItemLoaded())
		return false;
	if (ch->GetLevel() < bLevelLo || ch->GetLevel() > bLevelHi)
		return false;
	// MT2009_PLUS_MEDAL_SHOUTERS_V1: a krzykacz keeps its name and its life -
	// apka2009's three and Tieru's.
	if (IsPlayerBotShouterPID(ch->GetPlayerID()) || IsPlayerBotMedalShouterPID(ch->GetPlayerID()))
		return false;
	// MT2009_PLUS_LEGEND_NAMES_V1: nor one of the 27 Legends.
	if (IsPlayerBotLegendTier(GetPlayerBotLegendTier(ch->GetPlayerID())) ||
			GetPlayerBotLegendNameSlot(ch->GetName()) >= 0)
		return false;
	// MT2009_PLUS_BOT_RETIREMENT_FIX_V1: nor a player's companion (its owner
	// would find a level-1 stranger at his side), an Arezzo test cohort or a
	// bot a person has taken over from the panel.
	if (IsPlayerBotSidekickPID(ch->GetPlayerID()) || IsPlayerBotArezzoCohortPID(ch->GetPlayerID()) ||
			IsPlayerBotArezzoDungeonCohortPID(ch->GetPlayerID()) || IsPlayerBotTakeoverHold(ch->GetPlayerID()))
		return false;
	// MT2009_PLUS_BOT_DUNGEON_RUNS_V1: nor a bot of a dungeon run, until it is out.
	if (IsPlayerBotOnDungeonRun(ch->GetPlayerID()))
		return false;
	if (ch->GetExchange() || ch->GetShop() || ch->GetSafebox() || ch->IsBusy() || ch->GetMyShop())
		return false;
	if (ch->GetParty())
		return false;
	// The crossing of the spiders' desert and a Monkey Dungeon need the ordinary
	// AI to walk out; a retiring bot does not run it.
	if (IsPlayerBotSpiderMap(ch->GetMapIndex()) || IsPlayerBotMonkeyMap(ch->GetMapIndex()))
		return false;
	// A guild's leader cannot leave it, and a bot with a stall standing is a
	// keeper with its own arrangements.
	if (CGuild* guild = ch->GetGuild())
		if (guild->GetMasterPID() == ch->GetPlayerID())
			return false;
	if (HasPlayerBotOfflineShop(ch))
		return false;
	return true;
}

// Called for every live bot from the hook at the top of the per-bot loop in
// CPlayerBotManager::Update. true = handled, skip the ordinary AI this tick.
//
// The ordinary AI keeps running whenever the bot is dead, fighting or busy
// (it knows how to recover); a bot the retirement can steer is steered every
// tick, or the AI would put the gear back on between two attempts. Nothing in
// here despawns: the loop iterates m_mapBots.
bool ManagePlayerBotRetirement(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
{
	const DWORD pid = ch->GetPlayerID();
	GivePlayerBotRetireStarterKit(ch);
	std::map<DWORD, TPlayerBotRetireEntry>::iterator it = s_mapPlayerBotRetiring.find(pid);
	if (it == s_mapPlayerBotRetiring.end())
		return false;
	TPlayerBotRetireEntry& entry = it->second;
	entry.dwLastSeen = dwNow;

	// Waiting for ProcessRetirementResets to take it out of the world.
	if (entry.stage != PLAYERBOT_RETIRE_SHOPPING)
		return true;

	// Shops are the first channel's alone; a retired bot never leaves it on its
	// own, and a bot that the channel coordinator moved is called off from the
	// sweep in ProcessRetirementResets once it is gone.
	if (g_bChannel != playerbot_channel_rules::SHOP_CHANNEL)
		return false;

	// Let the ordinary AI deal with whatever it deals with best.
	if (ch->IsDead() || ch->IsStun() || !ch->IsItemLoaded() || ch->GetExchange() ||
			ch->GetShop() || ch->GetSafebox() || ch->IsBusy())
		return false;

	// The request journal and the sales, drained here because the AI that
	// normally does it is skipped.
	const bool inFlight = BotOfflinePoll(ch, dwNow);
	AuditPlayerBotRetireSales(pid, entry);
	BotOfflineDrainSales(ch, state, dwNow);

	// A standing stall needs nothing from the bot; when it closes is decided by
	// MonitorPlayerBotRetirementStall, which does not depend on the bot being
	// steerable at that moment.
	if (entry.bStallSeen || ikashop::GetManager().GetShopByOwnerID(pid))
		return true;
	if (inFlight)
	{
		// A create request that the db core never answered must not hold the bot
		// for ever: give the journal up after five minutes and try again.
		if (dwNow - entry.dwSubmittedAt > 300000)
		{
			playerbot_offline::requests.erase(pid);
			sys_err("PLAYERBOT_RETIRE: pid=%u create request unanswered for 5 minutes, dropped", pid);
		}
		return true;
	}

	if (AdvancePlayerBotRetirementStall(ch, state, entry, dwNow) == PLAYERBOT_RETIRE_STALL_EMPTY)
		BeginPlayerBotRetirementClose(pid, entry, "nothing to sell");
	return true;
}

// When the stall closes. Called for every SHOPPING pick from
// ProcessRetirementResets, so it runs whatever the bot itself is doing.
void MonitorPlayerBotRetirementStall(DWORD pid, TPlayerBotRetireEntry& entry, DWORD dwNow)
{
	NativeShop shop = ikashop::GetManager().GetShopByOwnerID(pid);
	if (shop)
	{
		if (!entry.bStallSeen)
		{
			entry.bStallSeen = true;
			entry.dwStallUntil = dwNow + s_dwPlayerBotRetireShopMs;
			entry.stage = PLAYERBOT_RETIRE_SELLING;
			SetPlayerBotRetireStage(pid, "selling");
			AuditPlayerBotRetireEvent(entry.dwBatchId, pid, "shop_standing", "offline shop visible");
			sys_log(0, "PLAYERBOT_RETIRE: pid=%u stall standing lines=%u for %u minutes",
					pid, (unsigned int)shop->GetItems().size(),
					(unsigned int)(s_dwPlayerBotRetireShopMs / 60000u));
		}
		if (shop->GetItems().empty())
			BeginPlayerBotRetirementClose(pid, entry, "sold out");
		else if (dwNow >= entry.dwStallUntil)
			BeginPlayerBotRetirementClose(pid, entry, "time is up");
		return;
	}
	// It stood, and it is gone: sold out (an empty shop is deleted). Not while
	// a request is still in the journal - that is the shop being created.
	if (entry.bStallSeen && playerbot_offline::requests.find(pid) == playerbot_offline::requests.end())
		BeginPlayerBotRetirementClose(pid, entry, "sold out");
}

} // anonymous namespace
#endif
