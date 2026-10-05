// MT2009_PLUS_AREZZO_MODULE_V1 - the Arezzo places as a module that can be switched on and off
// (.env M2_AREZZO, the classic panel's "Modul Arezzo" page live, the launcher's difficulty window).
// One world switch, the event flag mt2009_arezzo_closed (1 = off), written at the start by
// mariadb/playerbot/apply.sh from M2_AREZZO and live by web_admin.quest (AREZZO).
//
// What reads it:
//   * the quests: the Teleporter and the Teleportation Ring (360, 361), the Ochao portal's
//     choice (362) and the four dungeon guards (363-366) - as before;
//   * the dungeon panel (playerbot_dungeon_panel.h, IsArezzo): no Arezzo line, no warp;
//   * this file, on every core, every TICK_SECONDS:
//       - the three entrance guards that stand on the old maps (Straznik Biblioteki in every
//         empire's M2, Straznik Wzgorza in the Hwang Temple, Straznik Ruin on the Fire Land) are
//         spawned here instead of npc.txt: on while the module is on, gone while it is off;
//       - a player (not a GM) on an Arezzo map or in one of its dungeons while the module is
//         off is sent to his empire's town - but the operator's dungeon test cohort on its
//         own dungeon's map (playerbot_arezzo_dungeon_bots.h).
// The client always has the files (client 2.0.30+); without the module nothing on the server
// leads there. The event flags reach a core a moment after its boot, so the first look waits
// START_DELAY_SECONDS.

#ifndef __INC_MT2009_PLUS_AREZZO_H__
#define __INC_MT2009_PLUS_AREZZO_H__

#include "char.h"
#include "char_manager.h"
#include "desc.h"
#include "desc_manager.h"
#include "sectree_manager.h"
#include "questmanager.h"
#include "start_position.h"
#include "event.h"
#include "utils.h"

namespace mt2009_arezzo
{
	const int	TICK_SECONDS = 5;
	const int	START_DELAY_SECONDS = 30;
	const int	WARP_RETRY_SECONDS = 20;

	struct Guard
	{
		DWORD	vnum;
		long	map;
		long	cellX, cellY;	// map cells, as in npc.txt
		int		rot;			// degrees
		DWORD	vid;
	};

	// The npc.txt lines the Dockerfile used to append (direction 5 -> 180 degrees).
	Guard s_guards[] = {
		// Straznik Biblioteki - M2 of every empire, beside the Teleporter (moved from Orc Valley on
		// 30 September; cells checked walkable on each map's server_attr).
		{ 20430,  3, 496,  575, 180, 0 },	// metin2_map_a3 (Shinsoo M2)
		{ 20430, 23, 345,  351, 180, 0 },	// metin2_map_b3 (Chunjo M2)
		{ 20430, 43, 476,  351, 180, 0 },	// metin2_map_c3 (Jinno M2)
		{ 20423, 65, 165,  941, 180, 0 },	// Straznik Wzgorza - Swiatynia Hwang (metin2_map_milgyo)
		{ 20424, 62, 101,  910, 180, 0 },	// Straznik Ruin - Ognista Ziemia (metin2_map_n_flame_01)
	};
	const int GUARDS = sizeof(s_guards) / sizeof(s_guards[0]);

	LPEVENT	s_pkTick = NULL;
	DWORD	s_dwStartedAt = 0;
	int		s_iLastClosed = -1;
	std::map<DWORD, DWORD> s_warped;	// pid -> global time of the last send-off

	inline bool Closed()
	{
		return quest::CQuestManager::instance().GetEventFlag("mt2009_arezzo_closed") > 0;
	}

	// 360-366 and their dungeon instances (map index * 10000 + n).
	inline bool IsArezzoMap(long mapIndex)
	{
		const long base = mapIndex >= 10000 ? mapIndex / 10000 : mapIndex;
		return base >= 360 && base <= 366;
	}

	inline LPCHARACTER FindGuard(const Guard& g)
	{
		if (!g.vid)
			return NULL;
		LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(g.vid);
		if (!ch || ch->GetMapIndex() != g.map || ch->GetRaceNum() != g.vnum)
			return NULL;
		return ch;
	}

	inline void SyncGuards(bool closed)
	{
		for (int i = 0; i < GUARDS; ++i)
		{
			Guard& g = s_guards[i];
			LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(g.map);
			if (!map)
				continue;
			LPCHARACTER ch = FindGuard(g);
			if (closed)
			{
				if (ch)
				{
					M2_DESTROY_CHARACTER(ch);
					sys_log(0, "AREZZO: module off - guard %u removed from map %ld", g.vnum, g.map);
				}
				g.vid = 0;
				continue;
			}
			if (ch)
				continue;
			const long x = map->m_setting.iBaseX + g.cellX * 100;
			const long y = map->m_setting.iBaseY + g.cellY * 100;
			ch = CHARACTER_MANAGER::instance().SpawnMob(g.vnum, g.map, x, y, 0, false, g.rot, true);
			if (!ch)
			{
				sys_err("AREZZO: cannot spawn guard %u on map %ld at %ld %ld", g.vnum, g.map, x, y);
				g.vid = 0;
				continue;
			}
			g.vid = ch->GetVID();
			sys_log(0, "AREZZO: module on - guard %u on map %ld (cell %ld %ld), vid %u", g.vnum, g.map, g.cellX, g.cellY, g.vid);
		}
	}

	inline void SendPlayersOut()
	{
		const DWORD now = get_global_time();
		const DESC_MANAGER::DESC_SET& descs = DESC_MANAGER::instance().GetClientSet();
		for (DESC_MANAGER::DESC_SET::const_iterator d = descs.begin(); d != descs.end(); ++d)
		{
			LPCHARACTER ch = (*d)->GetCharacter();
			if (!ch || !ch->IsPC() || ch->IsGM() || !IsArezzoMap(ch->GetMapIndex()))
				continue;
			// MT2009_PLUS_AREZZO_DUNGEON_BOTS_V1: the operator's dungeon test cohort
			// on its own dungeon's map, on the core that hosts it, is the test's
			// (playerbot_arezzo_dungeon_bots.h, the test server runs with
			// M2_AREZZO=0). On game2 this warp to the town was refused (no town
			// there) and the cohort ran; on game1 it lands, and every 20 s.
			if (IsPlayerBotArezzoDungeonCohortHome(ch))
				continue;
			std::map<DWORD, DWORD>::iterator it = s_warped.find(ch->GetPlayerID());
			if (it != s_warped.end() && now - it->second < (DWORD) WARP_RETRY_SECONDS)
				continue;
			s_warped[ch->GetPlayerID()] = now;
			ch->ChatPacket(CHAT_TYPE_INFO, "Miejsca Arezzo s\xb9 wy\xb3\xb9" "czone na tym serwerze - wracasz do miasta.");
			sys_log(0, "AREZZO: module off - %s sent from map %ld to the town", ch->GetName(), ch->GetMapIndex());
			ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
		}
		if (s_warped.size() > 1000)
			s_warped.clear();
	}

	inline void Tick()
	{
		if (get_global_time() - s_dwStartedAt < (DWORD) START_DELAY_SECONDS)
			return;
		const bool closed = Closed();
		if (s_iLastClosed != (closed ? 1 : 0))
		{
			sys_log(0, "AREZZO: module %s", closed ? "off" : "on");
			s_iLastClosed = closed ? 1 : 0;
		}
		SyncGuards(closed);
		if (closed)
			SendPlayersOut();
	}

	EVENTINFO(arezzo_tick_info)
	{
		int dummy;
	};

	EVENTFUNC(arezzo_tick)
	{
		Tick();
		return PASSES_PER_SEC(TICK_SECONDS);
	}

	// From CPlayerBotManager::StartWorldClock, once per core (every core: a player can stand on
	// an Arezzo map of any core that hosts one, and the guards' maps are spread over them).
	inline void Start()
	{
		if (s_pkTick)
			return;
		s_dwStartedAt = get_global_time();
		arezzo_tick_info* info = AllocEventInfo<arezzo_tick_info>();
		s_pkTick = event_create(arezzo_tick, info, PASSES_PER_SEC(TICK_SECONDS));
		sys_log(0, "AREZZO: module clock started (flag mt2009_arezzo_closed)");
	}
}

#endif
