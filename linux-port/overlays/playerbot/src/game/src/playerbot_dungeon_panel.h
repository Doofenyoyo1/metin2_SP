#pragma once
// MT2009_PLUS_DUNGEON_PANEL_V1: the dungeon panel ("Lochy") - the window of the Arezzo files
// (uidungeoninfo.py, "Dungeon Information System" layout) on our chat-command protocol: the exe
// has no dungeonInfo module and no packets 140/141, so everything goes through "/lochy" and
// CHAT_TYPE_COMMAND lines, as the Treasure Hunt's window does (playerbot_goblin.h).
//
// The dungeons are <locale>/dungeon_info.txt (game/arezzo/dungeon_info.txt), read at the first
// use and again on "/lochy reload" (GM):
//   dungeon <key> <map> <type> <xy|town> <entry map[/map2/map3]> <x> <y> <lv min> <lv max 0=max>
//           <party min> <party max> <boss> <cooldown quest.flag|-> <cooldown s> <item> <count>
//           <name> <entry name>                      (names with _ for spaces, CP1250)
//   drop <key> <vnum> <count> <percent>
// "xy": the entrance at x, y (world units / 100) of the entry map; "town": the entry map's town
// position for the player's empire (three maps "a/b/c" = Shinsoo/Chunjo/Jinno).
//
// To the client (game.py "DungeonInfo", dungeoninfo.py):
//   DungeonInfo clear
//   DungeonInfo name <map> <name>
//   DungeonInfo add <i> <type> <map> <entry map> <lv min> <lv max> <party min> <party max> <boss>
//                   <cooldown left s> <finished> <best time s> <best damage> <item> <count>
//   DungeonInfo drop <i> <vnum> <count> <percent>
//   DungeonInfo open
//   DungeonInfo rank <type> <line> <name> <level> <points>   DungeonInfo rankme <place> <points>
//   DungeonInfo rankend <type>
// From the client: "/lochy [open|warp <i>|rank <i> <1 finished|2 time|3 damage>]"; a GM also
// "/lochy reload".
//
// The results are the character's quest flags dungeon_panel.<key>_f (finished), _t (best time,
// seconds) and _d (most damage to the boss), written by the quest function d.update_ranking(key)
// when a dungeon's boss dies (questlua_dungeon.cpp): for everyone in the instance, or on an open
// map (the Temple of Ochao, the Spider Dungeon, the Monkey Dungeons) for every player who hurt
// the boss. The time is the instance's age (dungeon flag mt2009_start, quest/dungeon_panel.quest)
// unless the quest passes one. The ranking reads the saved flags (player.quest), as the Treasure
// Hunt's does: a few minutes behind; the asker's own line is live.
//
// Arezzo's panel took the dungeon index of the packet unchecked (a crash from any client) and
// warped without a look at the fight, the trade or the cooldown: here every index is checked, a
// warp needs the player alive, out of a fight for 10 s, able to warp (CanWarp, IsHack), outside
// any dungeon and in the level range, and bots never get a line.
#include "locale_service.h"
#include "dungeon.h"
#include "questmanager.h"

namespace mt2009_dpanel
{
	struct Drop
	{
		DWORD vnum;
		int count;
		int pct;
	};

	struct Def
	{
		std::string key, name, entryName;
		int type;
		long map;
		bool town;
		long entryMaps[3];
		long x, y;
		int lvMin, lvMax, partyMin, partyMax;
		DWORD boss;
		std::string cdFlag;
		int cdSec;
		DWORD reqVnum;
		int reqCount;
		std::vector<Drop> drops;
		// MT2009_PLUS_DUNGEON_FEE_V1: the panel teleport's price ("cost" line, 0 = free) and an
		// entrance per empire ("entry" line: x y for Shinsoo, Chunjo, Jinno; world units / 100).
		long long warpCost;
		bool empireXY;
		long ex[3], ey[3];
		Def() : warpCost(0), empireXY(false) { ex[0] = ex[1] = ex[2] = ey[0] = ey[1] = ey[2] = 0; }
	};

	static std::vector<Def> s_defs;
	static bool s_loaded = false;

	struct RankCache
	{
		DWORD at;
		std::vector<std::string> names;
		std::vector<int> levels;
		std::vector<int> points;
		RankCache() : at(0) {}
	};
	static std::map<std::string, RankCache> s_rank;
	static std::map<DWORD, DWORD> s_lastCmd;
	// MT2009_PLUS_DUNGEON_PANEL_V1 (rows): the window numbers its rows 0, 1, 2... as they come
	// (dungeoninfo.py appends them) and sends that position back with "warp"/"rank" - so the lines
	// carry the row's position among the rows SENT, not its place in dungeon_info.txt. With the Arezzo
	// rows hidden (mt2009_arezzo_closed) the file's index skipped them while the client's did not:
	// "Razador" warped to the Demon Tower, "Nemere" to Razador. Every player keeps the list he was
	// shown (row -> s_defs index) until his next "open".
	static std::map<DWORD, std::vector<int> > s_shown;

	const int MAX_RANK_LINES = 10;

	void Load()
	{
		s_loaded = true;
		s_defs.clear();
		s_rank.clear();
		const std::string path = LocaleService_GetBasePath() + "/dungeon_info.txt";
		FILE* fp = fopen(path.c_str(), "r");
		if (!fp)
		{
			sys_err("DUNGEON_PANEL: cannot open %s", path.c_str());
			return;
		}
		char line[1024];
		int lineNo = 0;
		while (fgets(line, sizeof(line), fp))
		{
			++lineNo;
			char* p = line;
			while (*p == ' ' || *p == '\t')
				++p;
			if (!*p || *p == '#' || *p == '\r' || *p == '\n')
				continue;
			char tag[16] = "";
			if (sscanf(p, "%15s", tag) != 1)
				continue;
			if (!strcmp(tag, "dungeon"))
			{
				char key[32], mode[8], maps[32], cd[64], name[128], entryName[128];
				Def d;
				long map = 0;
				unsigned long boss = 0, req = 0;
				const int n = sscanf(p, "%*s %31s %ld %d %7s %31s %ld %ld %d %d %d %d %lu %63s %d %lu %d %127s %127s",
						key, &map, &d.type, mode, maps, &d.x, &d.y, &d.lvMin, &d.lvMax, &d.partyMin, &d.partyMax,
						&boss, cd, &d.cdSec, &req, &d.reqCount, name, entryName);
				if (n != 18)
				{
					sys_err("DUNGEON_PANEL: %s:%d: %d fields of 18", path.c_str(), lineNo, n);
					continue;
				}
				d.key = key;
				d.map = map;
				d.town = !strcmp(mode, "town");
				d.boss = (DWORD) boss;
				d.reqVnum = (DWORD) req;
				d.cdFlag = strcmp(cd, "-") ? cd : "";
				d.name = name;
				d.entryName = entryName;
				d.entryMaps[0] = d.entryMaps[1] = d.entryMaps[2] = 0;
				if (sscanf(maps, "%ld/%ld/%ld", &d.entryMaps[0], &d.entryMaps[1], &d.entryMaps[2]) < 1)
				{
					sys_err("DUNGEON_PANEL: %s:%d: entry map %s", path.c_str(), lineNo, maps);
					continue;
				}
				if (!d.entryMaps[1])
					d.entryMaps[1] = d.entryMaps[0];
				if (!d.entryMaps[2])
					d.entryMaps[2] = d.entryMaps[0];
				s_defs.push_back(d);
			}
			else if (!strcmp(tag, "cost"))
			{
				// MT2009_PLUS_DUNGEON_FEE_V1: cost <key> <yang>
				char key[32];
				long long cost = 0;
				if (sscanf(p, "%*s %31s %lld", key, &cost) != 2 || cost < 0)
				{
					sys_err("DUNGEON_PANEL: %s:%d: bad cost line", path.c_str(), lineNo);
					continue;
				}
				for (size_t i = 0; i < s_defs.size(); ++i)
					if (s_defs[i].key == key)
						s_defs[i].warpCost = cost;
			}
			else if (!strcmp(tag, "entry"))
			{
				// MT2009_PLUS_DUNGEON_FEE_V1: entry <key> <x y> <x y> <x y> (Shinsoo, Chunjo, Jinno)
				char key[32];
				long v[6];
				if (sscanf(p, "%*s %31s %ld %ld %ld %ld %ld %ld", key, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 7)
				{
					sys_err("DUNGEON_PANEL: %s:%d: bad entry line", path.c_str(), lineNo);
					continue;
				}
				for (size_t i = 0; i < s_defs.size(); ++i)
					if (s_defs[i].key == key)
					{
						s_defs[i].empireXY = true;
						for (int e = 0; e < 3; ++e)
						{
							s_defs[i].ex[e] = v[e * 2];
							s_defs[i].ey[e] = v[e * 2 + 1];
						}
					}
			}
			else if (!strcmp(tag, "drop"))
			{
				char key[32];
				Drop dr;
				unsigned long vnum = 0;
				if (sscanf(p, "%*s %31s %lu %d %d", key, &vnum, &dr.count, &dr.pct) != 4)
				{
					sys_err("DUNGEON_PANEL: %s:%d: bad drop line", path.c_str(), lineNo);
					continue;
				}
				dr.vnum = (DWORD) vnum;
				for (size_t i = 0; i < s_defs.size(); ++i)
					if (s_defs[i].key == key && s_defs[i].drops.size() < 60)
						s_defs[i].drops.push_back(dr);
			}
		}
		fclose(fp);
		sys_log(0, "DUNGEON_PANEL: %u dungeons from %s", (unsigned) s_defs.size(), path.c_str());
	}

	void EnsureLoaded()
	{
		if (!s_loaded)
			Load();
	}

	const Def* FindKey(const char* key)
	{
		EnsureLoaded();
		for (size_t i = 0; i < s_defs.size(); ++i)
			if (s_defs[i].key == key)
				return &s_defs[i];
		return NULL;
	}

	bool Eligible(LPCHARACTER ch)
	{
		return ch && ch->IsPC() && ch->GetDesc() && !ch->GetDesc()->IsBot();
	}

	void Cmd(LPCHARACTER ch, const char* format, ...)
	{
		if (!Eligible(ch))
			return;
		char buf[480];
		va_list args;
		va_start(args, format);
		vsnprintf(buf, sizeof(buf), format, args);
		va_end(args);
		ch->ChatPacket(CHAT_TYPE_COMMAND, "DungeonInfo %s", buf);
	}

	bool TooSoon(LPCHARACTER ch, DWORD ms)
	{
		const DWORD now = get_dword_time();
		DWORD& last = s_lastCmd[ch->GetPlayerID()];
		if (last && now - last < ms)
			return true;
		last = now;
		return false;
	}

	int EmpireIndex(LPCHARACTER ch)
	{
		const int e = ch->GetEmpire();
		return (e >= 1 && e <= 3) ? e - 1 : 0;
	}

	int LevelMax(const Def& d)
	{
		return d.lvMax > 0 ? d.lvMax : gPlayerMaxLevel;
	}

	std::string Flag(const Def& d, const char* suffix)
	{
		return std::string("dungeon_panel.") + d.key + suffix;
	}

	// MT2009_PLUS_AREZZO_MODULE_V1: a dungeon on an Arezzo map (360-366) or entered from one is
	// listed and warped to only while the module is on (event flag mt2009_arezzo_closed = 0).
	bool IsArezzo(const Def& d)
	{
		if (d.map >= 360 && d.map <= 366)
			return true;
		for (int i = 0; i < 3; ++i)
			if (d.entryMaps[i] >= 360 && d.entryMaps[i] <= 366)
				return true;
		return false;
	}

	bool Hidden(const Def& d)
	{
		return IsArezzo(d) && quest::CQuestManager::instance().GetEventFlag("mt2009_arezzo_closed") > 0;
	}

	int CooldownLeft(LPCHARACTER ch, const Def& d)
	{
		if (d.cdFlag.empty() || d.cdSec <= 0)
			return 0;
		const int at = ch->GetQuestFlag(d.cdFlag);
		if (at <= 0)
			return 0;
		const int left = d.cdSec - (get_global_time() - at);
		return left > 0 ? left : 0;
	}

	std::vector<int> VisibleRows()
	{
		std::vector<int> rows;
		for (size_t i = 0; i < s_defs.size(); ++i)
			if (!Hidden(s_defs[i]))
				rows.push_back((int) i);
		return rows;
	}

	// the window's row -> the dungeon (s_defs index), -1 when there is no such row
	int RowToDef(LPCHARACTER ch, int row)
	{
		std::map<DWORD, std::vector<int> >::const_iterator it = s_shown.find(ch->GetPlayerID());
		const std::vector<int> rows = it != s_shown.end() ? it->second : VisibleRows();
		if (row < 0 || row >= (int) rows.size() || rows[row] < 0 || rows[row] >= (int) s_defs.size())
			return -1;
		return rows[row];
	}

	void Open(LPCHARACTER ch)
	{
		EnsureLoaded();
		Cmd(ch, "clear");
		std::set<long> named;
		const std::vector<int> rows = VisibleRows();
		s_shown[ch->GetPlayerID()] = rows;
		for (size_t r = 0; r < rows.size(); ++r)
		{
			const size_t i = (size_t) rows[r];
			const Def& d = s_defs[i];
			const long entryMap = d.entryMaps[EmpireIndex(ch)];
			if (named.insert(d.map).second)
				Cmd(ch, "name %ld %s", d.map, d.name.c_str());
			if (named.insert(entryMap).second)
				Cmd(ch, "name %ld %s", entryMap, d.entryName.c_str());
			Cmd(ch, "add %u %d %ld %ld %d %d %d %d %u %d %d %d %d %u %d", (unsigned) r, d.type, d.map, entryMap,
					d.lvMin, LevelMax(d), d.partyMin, d.partyMax, d.boss, CooldownLeft(ch, d),
					ch->GetQuestFlag(Flag(d, "_f")), ch->GetQuestFlag(Flag(d, "_t")), ch->GetQuestFlag(Flag(d, "_d")),
					d.reqVnum, d.reqCount);
			for (size_t j = 0; j < d.drops.size(); ++j)
				Cmd(ch, "drop %u %u %d %d", (unsigned) r, d.drops[j].vnum, d.drops[j].count, d.drops[j].pct);
		}
		Cmd(ch, "open");
	}

	void Warp(LPCHARACTER ch, int row)
	{
		EnsureLoaded();
		const int index = RowToDef(ch, row);
		if (index < 0)
			return;
		const Def& d = s_defs[index];
		if (ch->IsDead() || Hidden(d))
			return;
		if (ch->GetMapIndex() >= 10000 || ch->GetDungeon())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Nie mo\xbf" "esz si\xea teleportowa\xe6 z lochu.");
			return;
		}
		if (ch->GetLevel() < d.lvMin || ch->GetLevel() > LevelMax(d))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Ten loch jest dla poziom\xf3w %d - %d.", d.lvMin, LevelMax(d));
			return;
		}
		if (get_dword_time() - ch->GetLastAttackTime() < 10000)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Poczekaj 10 sekund po walce.");
			return;
		}
		if (!ch->CanWarp() || ch->IsHack())
			return;
		// MT2009_PLUS_DUNGEON_FEE_V1: the teleport's price, checked before and taken on the warp.
		if (d.warpCost > 0 && !ch->IsGM() && (long long) ch->GetGold() < d.warpCost)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Teleport pod wej\x9c" "cie kosztuje %lld Yang - nie masz tyle przy sobie.", d.warpCost);
			return;
		}
		long x = d.x * 100, y = d.y * 100;
		if (d.empireXY)
		{
			x = d.ex[EmpireIndex(ch)] * 100;
			y = d.ey[EmpireIndex(ch)] * 100;
		}
		if (d.town)
		{
			PIXEL_POSITION pos;
			if (!SECTREE_MANAGER::instance().GetRecallPositionByEmpire(d.entryMaps[EmpireIndex(ch)], ch->GetEmpire(), pos))
				return;
			x = pos.x;
			y = pos.y;
		}
		sys_log(0, "DUNGEON_PANEL: %s warps to %s (%ld %ld), cost %lld", ch->GetName(), d.key.c_str(), x, y, d.warpCost);
		if (!ch->WarpSet(x, y))
			return;
		if (d.warpCost > 0 && !ch->IsGM())
		{
			PlayerBotChangeGold(ch, -d.warpCost);
			ch->ChatPacket(CHAT_TYPE_INFO, "Teleport pod wej\x9c" "cie: zap\xb3" "acono %lld Yang.", d.warpCost);
		}
	}

	// type 1: most finished, 2: best time (least), 3: most damage
	void LoadRank(const Def& d, int type, RankCache& c)
	{
		const DWORD now = get_dword_time();
		if (c.at && now - c.at < 30000)
			return;
		c.at = now;
		c.names.clear();
		c.levels.clear();
		c.points.clear();
		const char* suffix = type == 1 ? "_f" : (type == 2 ? "_t" : "_d");
		char query[512];
		snprintf(query, sizeof(query),
				"SELECT p.name, p.level, q.lValue FROM player.quest q JOIN player.player p ON p.id = q.dwPID "
				"WHERE q.szName = 'dungeon_panel' AND q.szState = '%s%s' AND q.lValue > 0 "
				"ORDER BY q.lValue %s, q.dwPID LIMIT %d", d.key.c_str(), suffix, type == 2 ? "ASC" : "DESC", 100);
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			int level = 0, value = 0;
			str_to_number(level, row[1]);
			str_to_number(value, row[2]);
			c.names.push_back(row[0] ? row[0] : "");
			c.levels.push_back(level);
			c.points.push_back(value);
		}
	}

	void Rank(LPCHARACTER ch, int row, int type)
	{
		EnsureLoaded();
		const int index = RowToDef(ch, row);
		if (index < 0 || type < 1 || type > 3)
			return;
		const Def& d = s_defs[index];
		char ck[64];
		snprintf(ck, sizeof(ck), "%s:%d", d.key.c_str(), type);
		RankCache& c = s_rank[ck];
		LoadRank(d, type, c);
		const char* suffix = type == 1 ? "_f" : (type == 2 ? "_t" : "_d");
		const int mine = ch->GetQuestFlag(Flag(d, suffix));
		// the asker's live figure in place of the saved one
		std::vector<std::string> names;
		std::vector<int> levels, points;
		for (size_t i = 0; i < c.names.size(); ++i)
		{
			if (c.names[i] == ch->GetName())
				continue;
			names.push_back(c.names[i]);
			levels.push_back(c.levels[i]);
			points.push_back(c.points[i]);
		}
		int place = 0;
		if (mine > 0)
		{
			size_t at = 0;
			while (at < points.size() && (type == 2 ? points[at] <= mine : points[at] >= mine))
				++at;
			names.insert(names.begin() + at, ch->GetName());
			levels.insert(levels.begin() + at, ch->GetLevel());
			points.insert(points.begin() + at, mine);
			place = (int) at + 1;
		}
		for (size_t i = 0; i < names.size() && (int) i < MAX_RANK_LINES; ++i)
			Cmd(ch, "rank %d %u %s %d %d", type, (unsigned) i, names[i].c_str(), levels[i], points[i]);
		Cmd(ch, "rankme %d %d", place, mine);
		Cmd(ch, "rankend %d", type);
	}

	struct FCollectPC
	{
		std::vector<LPCHARACTER>* out;
		void operator()(LPENTITY ent)
		{
			if (!ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER ch = (LPCHARACTER) ent;
			if (Eligible(ch))
				out->push_back(ch);
		}
	};
}

// "/lochy" (server-patches/playerqol, MT2009_PLUS_DUNGEON_PANEL_V1 (command)).
void DungeonPanelCommand(LPCHARACTER ch, const char* argument)
{
	using namespace mt2009_dpanel;
	if (!Eligible(ch))
		return;
	char sub[16] = "", a1[16] = "", a2[16] = "";
	const char* rest = one_argument(argument, sub, sizeof(sub));
	rest = one_argument(rest, a1, sizeof(a1));
	one_argument(rest, a2, sizeof(a2));
	if (!*sub || !strcmp(sub, "open"))
	{
		if (TooSoon(ch, 700))
			return;
		Open(ch);
	}
	else if (!strcmp(sub, "warp"))
	{
		if (TooSoon(ch, 700))
			return;
		int i = -1;
		if (!str_to_number(i, a1))
			return;
		Warp(ch, i);
	}
	else if (!strcmp(sub, "rank"))
	{
		if (TooSoon(ch, 700))
			return;
		int i = -1, t = 0;
		if (!str_to_number(i, a1) || !str_to_number(t, a2))
			return;
		Rank(ch, i, t);
	}
	else if (!strcmp(sub, "reload") && ch->GetGMLevel() >= GM_HIGH_WIZARD)
	{
		Load();
		s_shown.clear();
		ch->ChatPacket(CHAT_TYPE_INFO, "dungeon_info.txt: %u", (unsigned) s_defs.size());
	}
}

// d.update_ranking(key [, seconds]) (questlua_dungeon.cpp, MT2009_PLUS_DUNGEON_PANEL_V1 (lua)): in a
// boss's kill handler - pc is the killer, npc the boss.
void DungeonPanelUpdateRanking(LPCHARACTER pc, LPCHARACTER npc, const char* key, int seconds)
{
	using namespace mt2009_dpanel;
	if (!pc || !key || !*key)
		return;
	const Def* d = FindKey(key);
	if (!d)
	{
		// Not listed (the monkey/spider dungeons and the Temple of Ochao left the window on 30 September; their
		// quests still report) - nothing to rank.
		sys_log(0, "DUNGEON_PANEL: d.update_ranking(%s): not in dungeon_info.txt, not ranked", key);
		return;
	}
	std::vector<LPCHARACTER> players;
	const long mapIndex = pc->GetMapIndex();
	if (mapIndex >= 10000)
	{
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(mapIndex);
		if (pMap)
		{
			FCollectPC f;
			f.out = &players;
			pMap->for_each(f);
		}
		if (seconds <= 0)
		{
			LPDUNGEON pDungeon = CDungeonManager::instance().FindByMapIndex(mapIndex);
			const int start = pDungeon ? pDungeon->GetFlag("mt2009_start") : 0;
			if (start > 0)
				seconds = get_global_time() - start;
		}
	}
	else if (npc)
	{
		const CHARACTER::TDamageMap& dm = npc->Mt2009PlusGetDamageMap();
		for (CHARACTER::TDamageMap::const_iterator it = dm.begin(); it != dm.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(it->first);
			if (ch && Eligible(ch) && ch->GetMapIndex() == mapIndex)
				players.push_back(ch);
		}
	}
	if (Eligible(pc) && std::find(players.begin(), players.end(), pc) == players.end())
		players.push_back(pc);
	for (size_t i = 0; i < players.size(); ++i)
	{
		LPCHARACTER ch = players[i];
		ch->SetQuestFlag(Flag(*d, "_f"), ch->GetQuestFlag(Flag(*d, "_f")) + 1);
		if (seconds > 0)
		{
			const int best = ch->GetQuestFlag(Flag(*d, "_t"));
			if (best <= 0 || seconds < best)
				ch->SetQuestFlag(Flag(*d, "_t"), seconds);
		}
		if (npc)
		{
			const CHARACTER::TDamageMap& dm = npc->Mt2009PlusGetDamageMap();
			CHARACTER::TDamageMap::const_iterator it = dm.find(ch->GetVID());
			if (it != dm.end() && it->second.iTotalDamage > ch->GetQuestFlag(Flag(*d, "_d")))
				ch->SetQuestFlag(Flag(*d, "_d"), it->second.iTotalDamage);
		}
	}
	sys_log(0, "DUNGEON_PANEL: %s finished by %u player(s), %d s", key, (unsigned) players.size(), seconds);
}

// questlua_dungeon.cpp declares the ranking hook inside namespace quest (its edit sits in a
// function there), so the quest side calls quest::DungeonPanelUpdateRanking - forward it.
namespace quest
{
	void DungeonPanelUpdateRanking(LPCHARACTER pc, LPCHARACTER npc, const char* key, int seconds)
	{
		::DungeonPanelUpdateRanking(pc, npc, key, seconds);
	}
}
