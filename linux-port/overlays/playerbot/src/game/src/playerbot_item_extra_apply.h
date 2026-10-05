#ifndef __INC_METIN2_PLAYERBOT_ITEM_EXTRA_APPLY_H__
#define __INC_METIN2_PLAYERBOT_ITEM_EXTRA_APPLY_H__

// MT2009_PLUS_ITEM_EXTRA_APPLY_V1: "Dodatkowe bonusy przedmiotu" - bonus
// lines of an item beyond the three item_proto has room for.
//
// item_proto carries three bonuses (applytype0..2 / applyvalue0..2): that is
// ITEM_APPLY_MAX_NUM in the engine's TItemTable and aApplies[3] in the client
// exe's item record, and neither format can grow. The rest live in their own
// table, world.item_extra_apply (vnum, slot, apply_type, apply_value - one row
// a line, mariadb/playerbot/apply.sh), which the Seban panel's database editor
// fills ("Dodatkowe bonusy (ponad 3)" on an item's page). apply_type is the
// same number as item_proto's applytype: a POINT_* index (mt2009 has no
// separate APPLY_* enum - playerbot_engine_compat.h), which CItem::ModifyPoints
// hands to PointChange as it is. So the lines are worn exactly like the
// proto's own (server-patches/enginefixes, item.cpp hook): added on equip,
// taken off on unequip, utility points skipped, a jewellery piece's ore socket
// raising them the same way.
//
// A refine level is its own vnum (+0..+9 are ten rows of item_proto), so each
// carries its own lines; the editor copies them to the whole family at once.
//
// Read once, on the first item worn after the core starts - the editor's
// "Zastosuj" restarts the cores, as for every item_proto change. A table that
// is not there yet (apply.sh has not run) reads as empty, with one syserr line.
// The client shows the lines from gamedata/item_extra_apply.txt of the dbdata
// pack the editor hands out (uitooltip.py); the server applies them either way.
//
// A real header (include guard, inline functions): item.cpp includes it, and
// so does playerbot_manager.cpp for the bots' gear scoring (playerbot_gear.h).
// The function-local static is one object for the whole binary.

#include <unordered_map>
#include <vector>
#include <memory>

#include "db.h"

struct Mt2009ItemExtraApplyTable
{
	bool bLoaded = false;
	std::unordered_map<DWORD, std::vector<TItemApply> > lines;
};

inline Mt2009ItemExtraApplyTable& GetMt2009ItemExtraApplyTable()
{
	static Mt2009ItemExtraApplyTable s_table;
	return s_table;
}

inline void LoadMt2009ItemExtraApplies(Mt2009ItemExtraApplyTable& table)
{
	table.bLoaded = true;
	table.lines.clear();
	std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(
			"SELECT vnum, apply_type, apply_value FROM world.item_extra_apply "
			"WHERE apply_type <> 0 ORDER BY vnum, slot"));
	if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
	{
		sys_err("ITEM_EXTRA_APPLY: world.item_extra_apply could not be read - items carry only their item_proto bonuses");
		return;
	}
	size_t count = 0;
	MYSQL_ROW row;
	while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
	{
		if (!row[0] || !row[1] || !row[2])
			continue;
		const DWORD dwVnum = (DWORD)strtoul(row[0], NULL, 10);
		const long lType = strtol(row[1], NULL, 10);
		const long lValue = strtol(row[2], NULL, 10);
		if (dwVnum == 0 || lType <= 0 || lType > 255 || lType >= POINT_MAX_NUM)
			continue;
		TItemApply apply;
		apply.bType = (BYTE)lType;
		apply.lValue = lValue;
		table.lines[dwVnum].push_back(apply);
		++count;
	}
	sys_log(0, "ITEM_EXTRA_APPLY: %u extra bonus line(s) on %u item(s)", (unsigned)count, (unsigned)table.lines.size());
}

// The extra lines of one item (vnum), or NULL when it has none.
inline const std::vector<TItemApply>* GetMt2009ItemExtraApplies(DWORD dwVnum)
{
	Mt2009ItemExtraApplyTable& table = GetMt2009ItemExtraApplyTable();
	if (!table.bLoaded)
	{
		// Not before the database is there: the next item worn asks again.
		if (!AccountDB::instance().IsConnected())
			return NULL;
		LoadMt2009ItemExtraApplies(table);
	}
	if (table.lines.empty())
		return NULL;
	const auto it = table.lines.find(dwVnum);
	return it == table.lines.end() ? NULL : &it->second;
}

// The sum of one point over an item's extra lines (bots' gear scoring).
inline long SumMt2009ItemExtraApply(DWORD dwVnum, BYTE bType)
{
	const std::vector<TItemApply>* lines = GetMt2009ItemExtraApplies(dwVnum);
	if (!lines || bType == 0)
		return 0;
	long total = 0;
	for (const TItemApply& apply : *lines)
		if (apply.bType == bType)
			total += apply.lValue;
	return total;
}

#endif
