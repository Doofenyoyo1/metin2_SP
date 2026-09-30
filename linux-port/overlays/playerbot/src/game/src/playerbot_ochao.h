// MT2009_PLUS_OCHAO_V1 - Swiatynia Ochao (Temple of Ochao, map 209,
// metin2_map_mt_th_dungeon_01), the server half of the "Version 16.2" package
// (TempleOchao.cpp / questlua_TempleOchao.cpp of the archive), rewritten as an
// overlay so that no engine file needs a hook.
//
// The temple is an open map for characters of 95 and more (the entrance is
// Straznik Swiatyni, 20426, by Koe-Pung in Orc Valley and at the package's own
// spot in Dawnmistwood; quest/temple_of_the_ochao.quest). Its monsters come
// from the map's regen.txt and boss.txt like on any other map. What this file
// adds is the package's roaming mini-boss:
//
//   * one Straznik En-Tai (6400) stands in one of the temple's eleven rooms;
//   * nobody fights him for 3 minutes -> he moves to another room;
//   * the first time he is hit he stays for 10 minutes (a fight in progress
//     is never interrupted: while he has a victim the move waits);
//   * killed -> a Portal (20415) opens where he fell for 60 seconds, and the
//     Guardian comes back in another room RESPAWN_SECONDS after his death.
//
// The package drove this with two hooks in char_battle.cpp (GuardianAttacked
// in Damage, OnGuardianKilled in Dead) and a quest call on login. Here one
// event every TICK_SECONDS looks at the Guardian instead: his victim or a
// missing hit point is "attacked", IsDead() or his disappearance is "killed"
// (the last position seen is where the portal opens). The event starts with
// the core's world clock (CPlayerBotManager::StartWorldClock) and only on the
// core that hosts map 209 (m2-render-config: game1, beside the bots since
// MT2009_PLUS_OCHAO_BOTS_V1). The bots' side of the temple - the way in, the
// hunting spots, the way out - is playerbot_ochao_bots.h, whose watch runs on
// this clock.
//
// The package's restart_city_pos hook (sectree_manager.cpp) is not needed: a
// "restart in town" on 209 uses the map's own Town.txt, the temple's gate.

#ifndef __INC_MT2009_PLUS_OCHAO_H__
#define __INC_MT2009_PLUS_OCHAO_H__

#include "char.h"
#include "char_manager.h"
#include "sectree_manager.h"
#include "event.h"
#include "utils.h"

// MT2009_PLUS_OCHAO_BOTS_V1 (watch): playerbot_ochao_bots.h.
namespace { void TickPlayerBotOchao(); }

namespace mt2009_ochao
{
	const long	MAP_INDEX = 209;
	const DWORD	GUARDIAN_VNUM = 6400;	// Straznik En-Tai
	const DWORD	PORTAL_VNUM = 20415;	// Portal
	const int	TICK_SECONDS = 1;
	const int	NO_ACTIVITY_SECONDS = 180;	// TEMPLE_OCHAO_NO_ACTIVITY
	const int	ATTACKED_SECONDS = 600;		// TEMPLE_OCHAO_ATTACKED
	const int	PORTAL_SECONDS = 60;		// TEMPLE_OCHAO_PORTAL_SHOW
	// The package brought him back when the portal closed (60 s). He drops a
	// Kamien Duchowy (50513) four times in five, so on this world he rests for
	// five minutes after a death.
	const int	RESPAWN_SECONDS = 300;
	const int	BUSY_RETRY_SECONDS = 30;

	// The eleven rooms, in map cells (TempleOchao.cpp's table), all measured
	// walkable on the map's server_attr.
	const int	ROOMS = 11;
	const int	ROOM_CELLS[ROOMS][2] = {
		{193, 145}, {123, 216}, {224, 383}, {348, 708}, {375, 608}, {430, 516},
		{444, 382}, {388, 195}, {446, 247}, {592, 139}, {646, 152},
	};

	LPEVENT	s_pkTick = NULL;
	DWORD	s_dwGuardianVID = 0;
	DWORD	s_dwPortalVID = 0;
	int		s_iRoom = -1;
	bool	s_bAttacked = false;
	bool	s_bDeathSeen = false;
	DWORD	s_dwNextMove = 0;		// global time the idle Guardian changes room
	DWORD	s_dwPortalClose = 0;	// global time the portal goes away
	DWORD	s_dwRespawnAt = 0;		// global time a new Guardian appears (0: none due)
	long	s_lLastX = 0, s_lLastY = 0;

	inline bool Hosted()
	{
		return SECTREE_MANAGER::instance().GetMap(MAP_INDEX) != NULL;
	}

	inline LPCHARACTER FindOnMap(DWORD vid)
	{
		if (!vid)
			return NULL;
		LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(vid);
		if (!ch || ch->GetMapIndex() != MAP_INDEX)
			return NULL;
		return ch;
	}

	inline int PickRoom()
	{
		int room = number(0, ROOMS - 1);
		if (room == s_iRoom)
			room = (room + number(1, ROOMS - 1)) % ROOMS;
		return room;
	}

	inline void DestroyPortal()
	{
		LPCHARACTER portal = FindOnMap(s_dwPortalVID);
		if (portal)
			M2_DESTROY_CHARACTER(portal);
		s_dwPortalVID = 0;
		s_dwPortalClose = 0;
	}

	// A new Guardian in another room; the old one, if any, goes away.
	inline void SpawnGuardian(const char* why)
	{
		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(MAP_INDEX);
		if (!map)
			return;
		const int room = PickRoom();
		const long x = map->m_setting.iBaseX + ROOM_CELLS[room][0] * 100;
		const long y = map->m_setting.iBaseY + ROOM_CELLS[room][1] * 100;

		LPCHARACTER guardian = CHARACTER_MANAGER::instance().SpawnMob(GUARDIAN_VNUM, MAP_INDEX, x, y, 0, true, -1, true);
		if (!guardian)
		{
			sys_err("OCHAO: cannot spawn the En-Tai Guardian (%u) in room %d at %ld %ld (%s)",
					GUARDIAN_VNUM, room + 1, x, y, why);
			s_dwRespawnAt = get_global_time() + BUSY_RETRY_SECONDS;
			return;
		}

		LPCHARACTER old = FindOnMap(s_dwGuardianVID);
		if (old && old != guardian && !old->IsDead())
			M2_DESTROY_CHARACTER(old);

		s_dwGuardianVID = guardian->GetVID();
		s_iRoom = room;
		s_bAttacked = false;
		s_bDeathSeen = false;
		s_dwRespawnAt = 0;
		s_dwNextMove = get_global_time() + NO_ACTIVITY_SECONDS;
		s_lLastX = x;
		s_lLastY = y;
		sys_log(0, "OCHAO: En-Tai Guardian in room %d (cell %d %d), vid %u (%s)",
				room + 1, ROOM_CELLS[room][0], ROOM_CELLS[room][1], s_dwGuardianVID, why);
	}

	inline void OnGuardianDown(bool bKilled)
	{
		const DWORD now = get_global_time();
		s_dwGuardianVID = 0;
		s_dwRespawnAt = now + (bKilled ? RESPAWN_SECONDS : PORTAL_SECONDS);
		if (!bKilled)
		{
			sys_log(0, "OCHAO: the En-Tai Guardian is gone without a death; another in %d s", PORTAL_SECONDS);
			return;
		}

		DestroyPortal();
		LPCHARACTER portal = CHARACTER_MANAGER::instance().SpawnMob(PORTAL_VNUM, MAP_INDEX, s_lLastX, s_lLastY, 0, true, -1, true);
		if (portal)
		{
			s_dwPortalVID = portal->GetVID();
			s_dwPortalClose = now + PORTAL_SECONDS;
		}
		else
			sys_err("OCHAO: cannot open the portal (%u) at %ld %ld", PORTAL_VNUM, s_lLastX, s_lLastY);
		sys_log(0, "OCHAO: the En-Tai Guardian fell at %ld %ld; portal %u for %d s, next Guardian in %d s",
				s_lLastX, s_lLastY, s_dwPortalVID, PORTAL_SECONDS, RESPAWN_SECONDS);
	}

	inline void Tick()
	{
		const DWORD now = get_global_time();

		if (s_dwPortalVID && now >= s_dwPortalClose)
			DestroyPortal();

		if (s_dwGuardianVID)
		{
			LPCHARACTER guardian = FindOnMap(s_dwGuardianVID);
			if (!guardian)
			{
				// A corpse is taken away a few seconds after the death; one
				// that was fought and is gone was killed between two ticks.
				OnGuardianDown(s_bDeathSeen || s_bAttacked);
				return;
			}
			if (guardian->IsDead())
			{
				if (!s_bDeathSeen)
				{
					s_bDeathSeen = true;
					s_lLastX = guardian->GetX();
					s_lLastY = guardian->GetY();
					OnGuardianDown(true);
				}
				return;
			}

			s_lLastX = guardian->GetX();
			s_lLastY = guardian->GetY();

			const bool bFighting = guardian->GetVictim() != NULL;
			if (!s_bAttacked && (bFighting || guardian->GetHP() < guardian->GetMaxHP()))
			{
				s_bAttacked = true;
				s_dwNextMove = now + ATTACKED_SECONDS;
				sys_log(0, "OCHAO: the En-Tai Guardian is under attack, he stays %d s", ATTACKED_SECONDS);
			}

			if (now >= s_dwNextMove)
			{
				if (bFighting)
					s_dwNextMove = now + BUSY_RETRY_SECONDS;
				else
					SpawnGuardian("moves on");
			}
			return;
		}

		if (s_dwRespawnAt && now >= s_dwRespawnAt)
			SpawnGuardian(s_iRoom < 0 ? "first" : "back");
	}

	EVENTINFO(ochao_tick_info)
	{
		int dummy;
	};

	EVENTFUNC(ochao_tick)
	{
		if (!Hosted())
		{
			s_pkTick = NULL;
			return 0;
		}
		Tick();
		TickPlayerBotOchao(); // MT2009_PLUS_OCHAO_BOTS_V1 (watch)
		return PASSES_PER_SEC(TICK_SECONDS);
	}

	// From CPlayerBotManager::StartWorldClock (MapLocations): once per core,
	// and only where map 209 is hosted.
	inline void Start()
	{
		if (s_pkTick || !Hosted())
			return;
		s_dwRespawnAt = get_global_time();
		ochao_tick_info* info = AllocEventInfo<ochao_tick_info>();
		s_pkTick = event_create(ochao_tick, info, PASSES_PER_SEC(5));
		sys_log(0, "OCHAO: Temple of Ochao (map %ld) hosted here, En-Tai Guardian clock started", MAP_INDEX);
	}
}

#endif
