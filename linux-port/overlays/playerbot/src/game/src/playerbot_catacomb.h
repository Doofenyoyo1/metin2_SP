#ifndef __INC_METIN2_PLAYERBOT_CATACOMB_H__
#define __INC_METIN2_PLAYERBOT_CATACOMB_H__

// The Devil's Catacomb, raided by a party of bots ("dodamy obsluge rajdu na
// azraela", the operator, 26 September).
//
// The dungeon is devilcatacomb_zone.quest (quest/, compiled by the game
// image), map 216 on game1: a public first floor and six more in an
// instance of it. What a character needs is the quest's: level 75, the
// Demon Tower's ninth floor done (deviltower_zone.9_done, which the Reaper's
// fall gives everybody in his instance), thirty minutes since the last run,
// and for the third floor a Dried Head (IsPlayerBotCatacombHead), one taken
// from every member at the turtle rock. A bot goes from PLAYERBOT_CATACOMB_MIN_LEVEL
// - the floors are 79 to 86 and the bosses 89 to 91 - and only with a head
// in its bag, because the rock sends a member without one out.
//
// A raid is one party of one kingdom on this core, called by the world pass
// every PLAYERBOT_CATACOMB_INTERVAL: the eligible bots of the kingdom with
// most of them, the strongest first, a Shaman among them when there is one.
// They gather by the Guardian in Hwang Temple, go down to the first floor,
// form their party there, hunt its monsters by the statue until the key
// (30311, 1% a kill) drops, and the holder gives it to the statue: the
// quest's new_jump_party takes the whole party into an instance
// (CDungeon::JumpParty, which is why WarpBot keeps a party on a dungeon's
// jump). Inside, the floor is the dungeon's "level" flag:
//
//   2  the Gates of Perdition - for each of the four groups either set of
//      doors - and then the turtle rock, clicked;
//   3  the seven Metins of Revenge until the real one falls;
//   4  the maze of GOTO doors to the rune stake, clicked;
//   5  Tartar, and his totem given to the obelisk;
//   6  every monster and Charon;
//   7  every monster and Azrael - then the quest sends everybody to Hwang.
//
// The three windows a character passes (the statue, the rock, the stake)
// ask pc.is_playerbot() in the quest's own copy and let a bot through
// without the dialog; the rock and the stake are clicked through
// CQuestManager::Click, the key and the totem handed over with
// CHARACTER::GiveItem, which is what a player's drag does. A raid with a
// person in it is left to the person: the AI never clicks for a party whose
// instance holds anybody who can.
//
// Nothing of this could be watched yet: no bot of this world has reached
// the Reaper, so no bot carries 9_done or a head. The fight is the tower's
// (FightPlayerBotTowerObjective, KeepPlayerBotTowerAlive,
// BuffPlayerBotTowerFellows); the maze is walked door by door with the
// engine's own 45-second dwell between two crossings, and after
// PLAYERBOT_CATACOMB_MAZE_MAX_MS the leader passes the stake where it
// stands.
//
// An implementation fragment in the sense playerbot_types.h describes:
// include it once, after playerbot_boss_raid.h. mt2009 only: the r40250
// line ships neither the map nor the quest.

namespace
{
#if defined(PLAYERBOT_ENGINE_MT2009)
	// ------------------------------------------------------------ the place

	// The quest's own numbers (devilcatacomb_zone.setting): cells off the
	// map's base, a hundred units a cell.
	const long PLAYERBOT_CATACOMB_BASE_CELL_X = 3072;
	const long PLAYERBOT_CATACOMB_BASE_CELL_Y = 12032;
	long PlayerBotCatacombX(long cell) { return (PLAYERBOT_CATACOMB_BASE_CELL_X + cell) * 100; }
	long PlayerBotCatacombY(long cell) { return (PLAYERBOT_CATACOMB_BASE_CELL_Y + cell) * 100; }
	const long PLAYERBOT_CATACOMB_F1_ENTRY[2] = { 73, 63 };
	const long PLAYERBOT_CATACOMB_STATUE_CELL[2] = { 307, 323 };
	const long PLAYERBOT_CATACOMB_ROCK_CELL[2] = { 741, 217 };
	const long PLAYERBOT_CATACOMB_STAKE_CELL[2] = { 500, 717 };
	const long PLAYERBOT_CATACOMB_OBELISK_CELL[2] = { 848, 735 };
	const DWORD PLAYERBOT_CATACOMB_NPC_STATUE = 30101;
	const DWORD PLAYERBOT_CATACOMB_NPC_OBELISK = 30102;
	const DWORD PLAYERBOT_CATACOMB_NPC_ROCK = 30103;
	const DWORD PLAYERBOT_CATACOMB_NPC_STAKE = 30104;
	const DWORD PLAYERBOT_CATACOMB_KEY = 30311;
	const DWORD PLAYERBOT_CATACOMB_TOTEM = 30312;
	const DWORD PLAYERBOT_CATACOMB_TARTAR = 2591;
	const DWORD PLAYERBOT_CATACOMB_CHARON = 2597;
	const DWORD PLAYERBOT_CATACOMB_AZRAEL = 2598;
	// The Guardian in Hwang Temple (20367, cell 548,494 of metin2_map_milgyo)
	// and the quest's way out (5914, 992 on map 65).
	const long PLAYERBOT_CATACOMB_GUARDIAN_X = 592400;
	const long PLAYERBOT_CATACOMB_GUARDIAN_Y = 100600;
	const long PLAYERBOT_CATACOMB_EXIT_CELL_X = 5914;
	const long PLAYERBOT_CATACOMB_EXIT_CELL_Y = 992;

	// The Gates of Perdition (the quest's dc2_door_set1 and dc2_door_set2):
	// four groups, two sets each, and either set of a group opens it. `walk`
	// is the walk from the floor's entry (550,45) to the door, in units, on
	// the bots' own grid (fifty-unit cells, ATTR_BLOCK|ATTR_OBJECT blocked,
	// map 216's server_attr): the floor is one long corridor to the turtle
	// rock at 174 300, and the doors stand along it.
	struct TPlayerBotCatacombDoor
	{
		DWORD race;
		long cellX;
		long cellY;
		long walk;
	};
	const int PLAYERBOT_CATACOMB_DOOR_SET_SIZE[4] = { 4, 2, 3, 2 };
	const TPlayerBotCatacombDoor PLAYERBOT_CATACOMB_DOORS[4][2][4] = {
		{ { { 30111, 566, 117, 7900 }, { 30112, 562, 311, 27100 }, { 30118, 663, 434, 48200 }, { 30119, 881, 434, 70000 } },
		  { { 30115, 942, 141, 46900 }, { 30116, 942, 245, 57300 }, { 30117, 942, 321, 64900 }, { 30115, 763, 64, 22100 } } },
		{ { { 30116, 743, 390, 90200 }, { 30119, 612, 251, 115500 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
		  { { 30118, 643, 116, 124600 }, { 30114, 900, 167, 95200 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } } },
		{ { { 30113, 654, 211, 130800 }, { 30111, 707, 338, 147600 }, { 30112, 775, 336, 154300 }, { 0, 0, 0, 0 } },
		  { { 30114, 850, 293, 158600 }, { 30113, 715, 164, 133200 }, { 30114, 817, 162, 143500 }, { 0, 0, 0, 0 } } },
		{ { { 30117, 733, 294, 165900 }, { 30113, 694, 271, 171300 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
		  { { 30111, 802, 277, 161700 }, { 30112, 800, 241, 165200 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } } },
	};
	// What a unit of that walk is worth in a door's health, for choosing a
	// group's set: a raid of four walks some 450 units a second and breaks
	// some 20 000 health a second, so a set with more health left is still the
	// better one when its farthest door stands nearer.
	const long long PLAYERBOT_CATACOMB_DOOR_HP_PER_WALK = 44;

	// ------------------------------------------------------------ the tuning

	// The floors are 79 to 86, Tartar 89, Charon and Azrael 91.
	const BYTE PLAYERBOT_CATACOMB_MIN_LEVEL = 80;
	// A party of up to eight (PARTY_MAX_MEMBER), and a raid of fewer than four
	// is no raid - Azrael alone heals four and a half thousand a second.
	const int PLAYERBOT_CATACOMB_MIN_MEMBERS = 4;
	const int PLAYERBOT_CATACOMB_MAX_MEMBERS = 8;
	// The world pass looks every CHECK_MS, the first raid not before
	// FIRST_DELAY_MS after a start, and the next INTERVAL_MS after one ends.
	const DWORD PLAYERBOT_CATACOMB_CHECK_MS = 60 * 1000;
	const DWORD PLAYERBOT_CATACOMB_FIRST_DELAY_MS = 20 * 60 * 1000;
	const DWORD PLAYERBOT_CATACOMB_INTERVAL_MS = 120 * 60 * 1000;
	// The quest's own wait between two runs of one character.
	const int PLAYERBOT_CATACOMB_COOLDOWN_SECONDS = 1800;
	// How long the members have to reach the Guardian, and then the key.
	const DWORD PLAYERBOT_CATACOMB_GATHER_MS = 5 * 60 * 1000;
	const DWORD PLAYERBOT_CATACOMB_KEY_HUNT_MS = 20 * 60 * 1000;
	// Inside: a run the quest ends at its sixtieth minute; the raid gives up
	// sooner when nothing of a floor has died for STALL_MS.
	const DWORD PLAYERBOT_CATACOMB_RUN_MAX_MS = 65 * 60 * 1000;
	const DWORD PLAYERBOT_CATACOMB_STALL_MS = 12 * 60 * 1000;
	// The maze: after this long on the fourth floor the leader passes the
	// stake where it stands, in case a door is out of the bots' reach.
	const DWORD PLAYERBOT_CATACOMB_MAZE_MAX_MS = 12 * 60 * 1000;
	// Who is fought: within FIGHT_RANGE of the pack and on the bot's own
	// ground; a monster within THREAT_RANGE of the bot first.
	const long PLAYERBOT_CATACOMB_FIGHT_RANGE = 4000;
	const long PLAYERBOT_CATACOMB_THREAT_RANGE = 800;
	// The first floor is hunted round the statue, so the key drops where it is
	// handed in.
	const long PLAYERBOT_CATACOMB_STATUE_HUNT_RANGE = 3000;
	// A door is the quest's cell within this.
	const long PLAYERBOT_CATACOMB_DOOR_MATCH = 300;
	const DWORD PLAYERBOT_CATACOMB_SCAN_MS = 1500;
	const DWORD PLAYERBOT_CATACOMB_CLICK_RETRY_MS = 15 * 1000;
	const char* const PLAYERBOT_CATACOMB_NOW_PATH = "/opt/m2spool/playerbot_catacomb_now";

	// ------------------------------------------------------------ the raid

	enum EPlayerBotCatacombPhase
	{
		CATACOMB_PHASE_NONE = 0,
		CATACOMB_PHASE_GATHER,	// on the way to the Guardian
		CATACOMB_PHASE_FLOOR1,	// the first floor: the party, the key
		CATACOMB_PHASE_INSIDE	// the instance exists
	};

	struct TPlayerBotCatacombRaid
	{
		BYTE bPhase;
		BYTE bEmpire;
		DWORD dwLeader;
		std::set<DWORD> members;
		long lInstance;
		DWORD dwCalledAt;
		DWORD dwPhaseSince;
		DWORD dwLastProgress;
		int iFloor;
		int iAlive;
		DWORD dwFloorSince;
		DWORD dwNextClick;
		bool bAzraelSeen;
		bool bAzraelDown;

		TPlayerBotCatacombRaid() :
			bPhase(CATACOMB_PHASE_NONE), bEmpire(0), dwLeader(0), lInstance(0), dwCalledAt(0),
			dwPhaseSince(0), dwLastProgress(0), iFloor(0), iAlive(-1), dwFloorSince(0), dwNextClick(0),
			bAzraelSeen(false), bAzraelDown(false) {}
	};

	TPlayerBotCatacombRaid s_PlayerBotCatacombRaid;
	DWORD s_dwNextPlayerBotCatacombCheck = 0;
	DWORD s_dwNextPlayerBotCatacombRaidTime = 0;
	unsigned int s_uPlayerBotCatacombRaids = 0;
	time_t s_tPlayerBotCatacombNowSeen = (time_t)-1;

	// Each member's own clocks and the floor it last saw, by pid.
	struct TPlayerBotCatacombBot
	{
		int iFloor;
		DWORD dwNextMove;
		long lLastX;
		long lLastY;
		TPlayerBotCatacombBot() : iFloor(-1), dwNextMove(0), lLastX(0), lLastY(0) {}
	};
	std::map<DWORD, TPlayerBotCatacombBot> s_mapPlayerBotCatacombBots;

	bool IsPlayerBotCatacombRaider(DWORD pid)
	{
		const TPlayerBotCatacombRaid& raid = s_PlayerBotCatacombRaid;
		return raid.bPhase != CATACOMB_PHASE_NONE && raid.members.find(pid) != raid.members.end();
	}

	// ------------------------------------------------------------ the scan

	enum EPlayerBotCatacombKind
	{
		CATACOMB_KIND_MONSTER = 0,
		CATACOMB_KIND_STONE,
		CATACOMB_KIND_DOOR,
		CATACOMB_KIND_NPC,
		CATACOMB_KIND_GOTO
	};

	struct TPlayerBotCatacombEntity
	{
		DWORD vid;
		long x;
		long y;
		DWORD race;
		BYTE kind;
		// A GOTO door's destination, in world units.
		long toX;
		long toY;
	};

	struct TPlayerBotCatacombScan
	{
		DWORD dwScannedAt;
		std::vector<TPlayerBotCatacombEntity> entities;
		int alive;
		long packX;
		long packY;
		int packN;
		bool bPerson;
		TPlayerBotCatacombScan() : dwScannedAt(0), alive(0), packX(0), packY(0), packN(0), bPerson(false) {}
	};
	std::map<long, TPlayerBotCatacombScan> s_mapPlayerBotCatacombScans;

	struct FPlayerBotCatacombCollect
	{
		TPlayerBotCatacombScan& m_scan;
		long m_baseX;
		long m_baseY;
		FPlayerBotCatacombCollect(TPlayerBotCatacombScan& scan, long baseX, long baseY) :
			m_scan(scan), m_baseX(baseX), m_baseY(baseY) {}

		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER c = (LPCHARACTER)ent;
			if (c->IsPC())
			{
				if (c->GetDesc() && !c->GetDesc()->IsBot())
					m_scan.bPerson = true;
				else if (!c->IsDead() && IsPlayerBotCatacombRaider(c->GetPlayerID()))
				{
					m_scan.packX += c->GetX();
					m_scan.packY += c->GetY();
					++m_scan.packN;
				}
				return;
			}
			if (c->IsDead())
				return;
			TPlayerBotCatacombEntity e;
			e.vid = (DWORD)c->GetVID();
			e.x = c->GetX();
			e.y = c->GetY();
			e.race = c->GetRaceNum();
			e.toX = 0;
			e.toY = 0;
			if (c->IsStone())
				e.kind = CATACOMB_KIND_STONE;
			else if (c->IsDoor())
				e.kind = CATACOMB_KIND_DOOR;
			else if (c->IsMonster())
				e.kind = CATACOMB_KIND_MONSTER;
			else if (c->IsGoto())
			{
				// Where the engine's FuncCheckWarp sends whoever stands by it: the
				// name's two numbers, cells off the map's base.
				char buf[64];
				long cx = 0, cy = 0;
				if (sscanf(c->GetName(), " %63s %ld %ld", buf, &cx, &cy) != 3)
					return;
				e.kind = CATACOMB_KIND_GOTO;
				e.toX = m_baseX + cx * 100;
				e.toY = m_baseY + cy * 100;
			}
			else
				e.kind = CATACOMB_KIND_NPC;
			if (e.kind == CATACOMB_KIND_MONSTER || e.kind == CATACOMB_KIND_STONE || e.kind == CATACOMB_KIND_DOOR)
				++m_scan.alive;
			m_scan.entities.push_back(e);
		}
	};

	const TPlayerBotCatacombScan* ScanPlayerBotCatacombMap(long lMapIndex, DWORD dwNow)
	{
		TPlayerBotCatacombScan& scan = s_mapPlayerBotCatacombScans[lMapIndex];
		if (scan.dwScannedAt != 0 && dwNow - scan.dwScannedAt < PLAYERBOT_CATACOMB_SCAN_MS)
			return &scan;
		scan = TPlayerBotCatacombScan();
		scan.dwScannedAt = dwNow;
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(lMapIndex);
		if (!pMap)
			return &scan;
		FPlayerBotCatacombCollect f(scan, pMap->m_setting.iBaseX, pMap->m_setting.iBaseY);
		pMap->for_each(f);
		if (scan.packN > 0)
		{
			scan.packX /= scan.packN;
			scan.packY /= scan.packN;
		}
		return &scan;
	}

	LPCHARACTER FindPlayerBotCatacombNpc(const TPlayerBotCatacombScan* scan, DWORD race, long x, long y)
	{
		DWORD bestVid = 0;
		int best = INT_MAX;
		for (size_t i = 0; scan && i < scan->entities.size(); ++i)
		{
			const TPlayerBotCatacombEntity& e = scan->entities[i];
			if (e.race != race)
				continue;
			const int d = DISTANCE_APPROX(x - e.x, y - e.y);
			if (d < best)
			{
				best = d;
				bestVid = e.vid;
			}
		}
		return bestVid ? CHARACTER_MANAGER::instance().Find(bestVid) : NULL;
	}

	// ------------------------------------------------------------ the fight

	// The ground a bot is on: the floors are rooms of one map, and the maze's
	// rooms are joined by its doors alone.
	DWORD GetPlayerBotCatacombGround(long lMapIndex, long x, long y)
	{
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(lMapIndex);
		if (!navigation.Init(lMapIndex))
			return 0;
		return navigation.GetComponentAtWorld(x, y, 4);
	}

	// A monster or a stone to fight: one within THREAT_RANGE of the bot first,
	// then the nearest to the pack on the bot's own ground within FIGHT_RANGE,
	// with `preferRace` (Tartar, a stone) ahead of the rest. Doors are the
	// second floor's and are asked for there.
	LPCHARACTER PickPlayerBotCatacombFoe(LPCHARACTER ch, const TPlayerBotCatacombScan* scan, DWORD preferRace,
			bool stones)
	{
		if (!scan)
			return NULL;
		const long map = ch->GetMapIndex();
		const DWORD ground = GetPlayerBotCatacombGround(map, ch->GetX(), ch->GetY());
		const long fromX = scan->packN >= 2 ? scan->packX : ch->GetX();
		const long fromY = scan->packN >= 2 ? scan->packY : ch->GetY();
		DWORD bestVid = 0;
		long bestCost = LONG_MAX;
		for (size_t i = 0; i < scan->entities.size(); ++i)
		{
			const TPlayerBotCatacombEntity& e = scan->entities[i];
			if (e.kind != CATACOMB_KIND_MONSTER && !(stones && e.kind == CATACOMB_KIND_STONE))
				continue;
			const int mine = DISTANCE_APPROX(ch->GetX() - e.x, ch->GetY() - e.y);
			const int pack = DISTANCE_APPROX(fromX - e.x, fromY - e.y);
			if (mine > PLAYERBOT_CATACOMB_THREAT_RANGE && pack > PLAYERBOT_CATACOMB_FIGHT_RANGE)
				continue;
			if (ground != 0 && GetPlayerBotCatacombGround(map, e.x, e.y) != ground)
				continue;
			long cost = mine <= PLAYERBOT_CATACOMB_THREAT_RANGE ? (long)mine - 100000 : (long)pack;
			if (preferRace != 0 && e.race == preferRace)
				cost -= 50000;
			if (cost < bestCost)
			{
				bestCost = cost;
				bestVid = e.vid;
			}
		}
		return bestVid ? CHARACTER_MANAGER::instance().Find(bestVid) : NULL;
	}

	// A standing door of the table: the door character at its quest cell.
	LPCHARACTER FindPlayerBotCatacombDoor(const TPlayerBotCatacombScan* scan, long baseX, long baseY,
			const TPlayerBotCatacombDoor& door)
	{
		const long dx = baseX + door.cellX * 100;
		const long dy = baseY + door.cellY * 100;
		for (size_t i = 0; scan && i < scan->entities.size(); ++i)
		{
			const TPlayerBotCatacombEntity& e = scan->entities[i];
			if (e.kind != CATACOMB_KIND_DOOR || e.race != door.race ||
					DISTANCE_APPROX(e.x - dx, e.y - dy) > PLAYERBOT_CATACOMB_DOOR_MATCH)
				continue;
			LPCHARACTER c = CHARACTER_MANAGER::instance().Find(e.vid);
			return c && !c->IsDead() ? c : NULL;
		}
		return NULL;
	}

	// The second floor's doors, in the order they stand along the corridor.
	// The first version took the door nearest the pack in a straight line, and
	// the floor winds: the target changed with every step the pack took and
	// the route with it (189 waypoints, then 199, 171, 150, 93 inside two
	// minutes of the self-test), a tour of 560 000 to 800 000 units against
	// 176 000 along the corridor. For each group not yet open, the set whose
	// standing doors cost less - their health left, plus DOOR_HP_PER_WALK for
	// every unit to its farthest door - and of those doors, across the groups,
	// the one nearest the entry by `walk`. The answer does not depend on where
	// anybody stands, so every raider hits the same door and keeps it.
	LPCHARACTER PickPlayerBotCatacombDoor(const TPlayerBotCatacombScan* scan, long baseX, long baseY, bool& allOpen)
	{
		allOpen = true;
		LPCHARACTER best = NULL;
		long bestWalk = LONG_MAX;
		for (int g = 0; g < 4; ++g)
		{
			int standing[2] = { 0, 0 };
			long long cost[2] = { 0, 0 };
			long farthest[2] = { 0, 0 };
			LPCHARACTER first[2] = { NULL, NULL };
			long firstWalk[2] = { LONG_MAX, LONG_MAX };
			for (int s = 0; s < 2; ++s)
				for (int j = 0; j < PLAYERBOT_CATACOMB_DOOR_SET_SIZE[g]; ++j)
				{
					const TPlayerBotCatacombDoor& door = PLAYERBOT_CATACOMB_DOORS[g][s][j];
					LPCHARACTER c = FindPlayerBotCatacombDoor(scan, baseX, baseY, door);
					if (!c)
						continue;
					++standing[s];
					cost[s] += c->GetHP();
					farthest[s] = std::max(farthest[s], door.walk);
					if (door.walk < firstWalk[s])
					{
						firstWalk[s] = door.walk;
						first[s] = c;
					}
				}
			if (standing[0] == 0 || standing[1] == 0)
				continue;
			allOpen = false;
			const int s = cost[0] + PLAYERBOT_CATACOMB_DOOR_HP_PER_WALK * farthest[0] <=
					cost[1] + PLAYERBOT_CATACOMB_DOOR_HP_PER_WALK * farthest[1] ? 0 : 1;
			if (firstWalk[s] < bestWalk)
			{
				bestWalk = firstWalk[s];
				best = first[s];
			}
		}
		return best;
	}

	// The maze: the next door on the way from the bot's room to the stake's,
	// a breadth-first walk over the rooms the GOTO doors join.
	LPCHARACTER PickPlayerBotCatacombMazeDoor(LPCHARACTER ch, const TPlayerBotCatacombScan* scan,
			DWORD stakeGround)
	{
		const long map = ch->GetMapIndex();
		const DWORD start = GetPlayerBotCatacombGround(map, ch->GetX(), ch->GetY());
		if (!scan || start == 0 || stakeGround == 0 || start == stakeGround)
			return NULL;
		struct TEdge { DWORD from; DWORD to; DWORD vid; };
		std::vector<TEdge> edges;
		for (size_t i = 0; i < scan->entities.size(); ++i)
		{
			const TPlayerBotCatacombEntity& e = scan->entities[i];
			if (e.kind != CATACOMB_KIND_GOTO)
				continue;
			TEdge edge;
			edge.from = GetPlayerBotCatacombGround(map, e.x, e.y);
			edge.to = GetPlayerBotCatacombGround(map, e.toX, e.toY);
			edge.vid = e.vid;
			if (edge.from != 0 && edge.to != 0 && edge.from != edge.to)
				edges.push_back(edge);
		}
		// Rooms reached, and the first door taken out of the bot's room to reach each.
		std::map<DWORD, DWORD> firstDoor;
		std::vector<DWORD> queue;
		firstDoor[start] = 0;
		queue.push_back(start);
		for (size_t head = 0; head < queue.size(); ++head)
		{
			const DWORD room = queue[head];
			for (size_t i = 0; i < edges.size(); ++i)
			{
				if (edges[i].from != room || firstDoor.find(edges[i].to) != firstDoor.end())
					continue;
				firstDoor[edges[i].to] = room == start ? edges[i].vid : firstDoor[room];
				if (edges[i].to == stakeGround)
					return CHARACTER_MANAGER::instance().Find(firstDoor[edges[i].to]);
				queue.push_back(edges[i].to);
			}
		}
		return NULL;
	}

	// ------------------------------------------------------------ the raid's steps

	void WalkPlayerBotCatacomb(LPCHARACTER ch, TPlayerBotAIState& state, long x, long y, DWORD dwNow)
	{
		TPlayerBotCatacombBot& self = s_mapPlayerBotCatacombBots[ch->GetPlayerID()];
		state.dwTargetVID = 0;
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		if (dwNow < self.dwNextMove)
			return;
		self.dwNextMove = dwNow + 1500;
		MovePlayerBot(ch, x, y, dwNow, 8, true, false);
	}

	// Out of the Catacomb by the quest's own way (Hwang, 5914,992): the
	// location the quest leaves is often (0,0), which WarpBot refuses.
	void SendPlayerBotOutOfCatacomb(LPCHARACTER ch)
	{
		ch->SetWarpLocation(65, PLAYERBOT_CATACOMB_EXIT_CELL_X, PLAYERBOT_CATACOMB_EXIT_CELL_Y);
		ch->ExitToSavedLocation();
	}

	void EndPlayerBotCatacombRaid(DWORD dwNow, const char* why)
	{
		TPlayerBotCatacombRaid& raid = s_PlayerBotCatacombRaid;
		sys_log(0, "PLAYERBOT_CATACOMB: raid over empire=%u phase=%d floor=%d members=%u after_min=%u why=%s",
				(unsigned int)raid.bEmpire, (int)raid.bPhase, raid.iFloor, (unsigned int)raid.members.size(),
				(dwNow - raid.dwCalledAt) / 60000U, why);
		const std::set<DWORD> members = raid.members;
		raid = TPlayerBotCatacombRaid();
		for (std::set<DWORD>::const_iterator it = members.begin(); it != members.end(); ++it)
		{
			s_mapPlayerBotCatacombBots.erase(*it);
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(*it);
			if (!ch)
				continue;
			if (ch->GetParty() && !IsPlayerBotHumanLedParty(ch->GetParty()))
				LeavePlayerBotParty(ch);
			if (ch->GetMapIndex() == PLAYERBOT_MAP_CATACOMB || IsPlayerBotCatacombInstance(ch->GetMapIndex()))
				SendPlayerBotOutOfCatacomb(ch);
		}
		s_dwNextPlayerBotCatacombRaidTime = dwNow + PLAYERBOT_CATACOMB_INTERVAL_MS;
	}

	// Azrael's fall is news, as the Reaper's is.
	void AnnouncePlayerBotAzrael(const TPlayerBotCatacombRaid& raid, DWORD dwNow)
	{
		char msg[220];
		LPCHARACTER leader = CHARACTER_MANAGER::instance().FindByPID(raid.dwLeader);
		snprintf(msg, sizeof(msg), "Druzyna %s (%s) pokonala Azraela w Katakumbach Diabla!",
				leader ? leader->GetName() : "?", GetPlayerBotKingdomName(raid.bEmpire));
		BroadcastNotice(msg);
		sys_log(0, "PLAYERBOT_CATACOMB: azrael down leader=%s empire=%u after_min=%u",
				leader ? leader->GetName() : "?", (unsigned int)raid.bEmpire, (dwNow - raid.dwCalledAt) / 60000U);
	}

	// Whether a bot may go: the quest's four conditions, a head, and nothing
	// of its own that the dungeon would take it from.
	bool IsPlayerBotCatacombEligible(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		if (!ch || ch->IsDead() || ch->GetLevel() < PLAYERBOT_CATACOMB_MIN_LEVEL)
			return false;
		if (ch->GetQuestFlag("deviltower_zone.9_done") == 0)
			return false;
		if (get_global_time() - ch->GetQuestFlag("devilcatacomb_zone.last_exit_time") < PLAYERBOT_CATACOMB_COOLDOWN_SECONDS)
			return false;
		if (ch->CountSpecifyItem(30319) + ch->CountSpecifyItem(30320) + ch->CountSpecifyItem(76002) <= 0)
			return false;
		if (ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()))
			return false;
		if (IsPlayerBotSidekickPID(ch->GetPlayerID()) || IsPlayerBotDropper(state.bPersonality))
			return false;
		if (IsPlayerBotOnTowerBusiness(ch, state) || state.dwGuildWarEnemyGID != 0)
			return false;
		if (IsPlayerBotAngler(ch, state) || IsPlayerBotMiner(ch, state) || state.bFishingSession)
			return false;
		return true;
	}

	struct TPlayerBotCatacombCandidate
	{
		DWORD pid;
		int strength;
		bool shaman;
	};

	bool PlayerBotCatacombCandidateOrder(const TPlayerBotCatacombCandidate& a, const TPlayerBotCatacombCandidate& b)
	{
		if (a.strength != b.strength)
			return a.strength > b.strength;
		return a.pid < b.pid;
	}

	// The kingdom with most eligible bots on this core, and its strongest -
	// a Shaman among them when it has one.
	bool CallPlayerBotCatacombRaid(DWORD dwNow, const char* why)
	{
		std::map<BYTE, std::vector<TPlayerBotCatacombCandidate> > byKingdom;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch || !IsPlayerBotCatacombEligible(ch, it->second))
				continue;
			TPlayerBotCatacombCandidate c;
			c.pid = it->first;
			c.strength = GetPlayerBotStrengthCached(it->first);
			if (c.strength <= 0)
				c.strength = ch->GetLevel() * 1000;
			c.shaman = ch->GetJob() == JOB_SHAMAN;
			byKingdom[ch->GetEmpire()].push_back(c);
		}
		BYTE empire = 0;
		size_t most = 0;
		for (std::map<BYTE, std::vector<TPlayerBotCatacombCandidate> >::const_iterator it = byKingdom.begin();
				it != byKingdom.end(); ++it)
			if (it->second.size() > most)
			{
				most = it->second.size();
				empire = it->first;
			}
		if (most < (size_t)PLAYERBOT_CATACOMB_MIN_MEMBERS)
		{
			sys_log(0, "PLAYERBOT_CATACOMB: nobody to call eligible_best=%u why=%s", (unsigned int)most, why);
			return false;
		}
		std::vector<TPlayerBotCatacombCandidate>& all = byKingdom[empire];
		std::sort(all.begin(), all.end(), PlayerBotCatacombCandidateOrder);
		std::vector<TPlayerBotCatacombCandidate> picked(all.begin(),
				all.begin() + std::min(all.size(), (size_t)PLAYERBOT_CATACOMB_MAX_MEMBERS));
		bool shaman = false;
		for (size_t i = 0; i < picked.size(); ++i)
			shaman = shaman || picked[i].shaman;
		if (!shaman && picked.size() == (size_t)PLAYERBOT_CATACOMB_MAX_MEMBERS)
			for (size_t i = picked.size(); i < all.size(); ++i)
				if (all[i].shaman)
				{
					picked.back() = all[i];
					break;
				}

		TPlayerBotCatacombRaid& raid = s_PlayerBotCatacombRaid;
		raid = TPlayerBotCatacombRaid();
		raid.bPhase = CATACOMB_PHASE_GATHER;
		raid.bEmpire = empire;
		raid.dwLeader = picked[0].pid;
		raid.dwCalledAt = dwNow;
		raid.dwPhaseSince = dwNow;
		raid.dwLastProgress = dwNow;
		for (size_t i = 0; i < picked.size(); ++i)
		{
			raid.members.insert(picked[i].pid);
			s_mapPlayerBotCatacombBots[picked[i].pid] = TPlayerBotCatacombBot();
		}
		++s_uPlayerBotCatacombRaids;
		LPCHARACTER leader = CHARACTER_MANAGER::instance().FindByPID(raid.dwLeader);
		char msg[220];
		snprintf(msg, sizeof(msg), "Druzyna %s (%s) zbiera sie przy Strazniku Katakumb w Swiatyni Hwang - rusza na Azraela.",
				leader ? leader->GetName() : "?", GetPlayerBotKingdomName(empire));
		BroadcastNotice(msg);
		sys_log(0, "PLAYERBOT_CATACOMB: raid called empire=%u members=%u leader=%s eligible=%u shaman=%d why=%s",
				(unsigned int)empire, (unsigned int)picked.size(), leader ? leader->GetName() : "?",
				(unsigned int)all.size(), shaman ? 1 : 0, why);
		return true;
	}

	bool PlayerBotCatacombNowRequested()
	{
		struct stat st;
		if (stat(PLAYERBOT_CATACOMB_NOW_PATH, &st) != 0)
			return false;
		if (s_tPlayerBotCatacombNowSeen == (time_t)-1)
		{
			s_tPlayerBotCatacombNowSeen = st.st_mtime;
			return false;
		}
		if (st.st_mtime <= s_tPlayerBotCatacombNowSeen)
			return false;
		s_tPlayerBotCatacombNowSeen = st.st_mtime;
		return true;
	}

	// The world's pass: a raid under way moved along, or the next one called.
	void ManagePlayerBotCatacombRaids(DWORD dwNow)
	{
		if (g_bChannel != 1)
			return;
		if (s_dwNextPlayerBotCatacombCheck != 0 && dwNow < s_dwNextPlayerBotCatacombCheck)
			return;
		s_dwNextPlayerBotCatacombCheck = dwNow + PLAYERBOT_CATACOMB_CHECK_MS;
		if (!IsPlayerBotMapHostedHere(PLAYERBOT_MAP_CATACOMB) || !IsPlayerBotMapHostedHere(PLAYERBOT_MAP_HWANG))
			return;
		if (s_tPlayerBotCatacombNowSeen == (time_t)-1)
			s_tPlayerBotCatacombNowSeen = time(NULL);
		const bool now = PlayerBotCatacombNowRequested();

		TPlayerBotCatacombRaid& raid = s_PlayerBotCatacombRaid;
		if (raid.bPhase == CATACOMB_PHASE_NONE)
		{
			if (s_dwNextPlayerBotCatacombRaidTime == 0)
				s_dwNextPlayerBotCatacombRaidTime = dwNow + PLAYERBOT_CATACOMB_FIRST_DELAY_MS;
			if (!IsPlayerBotCatacombRaidsEnabled() || quest::CQuestManager::instance().GetEventFlag("dc_closed") > 0)
				return;
			if (!now && dwNow < s_dwNextPlayerBotCatacombRaidTime)
				return;
			if (!CallPlayerBotCatacombRaid(dwNow, now ? "now" : "interval"))
				s_dwNextPlayerBotCatacombRaidTime = dwNow + (now ? PLAYERBOT_CATACOMB_CHECK_MS : PLAYERBOT_CATACOMB_INTERVAL_MS / 4);
			return;
		}

		// Where the members are.
		int onHwang = 0, nearGuardian = 0, onFloor1 = 0, inside = 0, online = 0;
		long instance = 0;
		for (std::set<DWORD>::const_iterator it = raid.members.begin(); it != raid.members.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(*it);
			if (!ch)
				continue;
			++online;
			const long map = ch->GetMapIndex();
			if (map == PLAYERBOT_MAP_HWANG)
			{
				++onHwang;
				if (DISTANCE_APPROX(ch->GetX() - PLAYERBOT_CATACOMB_GUARDIAN_X, ch->GetY() - PLAYERBOT_CATACOMB_GUARDIAN_Y) <= 1500)
					++nearGuardian;
			}
			else if (map == PLAYERBOT_MAP_CATACOMB)
				++onFloor1;
			else if (IsPlayerBotCatacombInstance(map))
			{
				++inside;
				instance = map;
			}
		}
		if (raid.members.empty())
		{
			EndPlayerBotCatacombRaid(dwNow, raid.bAzraelDown ? "done" : "members_gone");
			return;
		}
		if (online == 0)
		{
			EndPlayerBotCatacombRaid(dwNow, "nobody_online");
			return;
		}

		switch (raid.bPhase)
		{
			case CATACOMB_PHASE_GATHER:
				if (nearGuardian >= (int)raid.members.size() ||
						(dwNow - raid.dwPhaseSince > PLAYERBOT_CATACOMB_GATHER_MS && nearGuardian >= PLAYERBOT_CATACOMB_MIN_MEMBERS))
				{
					// Whoever did not come is not in it.
					std::set<DWORD> came;
					for (std::set<DWORD>::const_iterator it = raid.members.begin(); it != raid.members.end(); ++it)
					{
						LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(*it);
						if (ch && ch->GetMapIndex() == PLAYERBOT_MAP_HWANG &&
								DISTANCE_APPROX(ch->GetX() - PLAYERBOT_CATACOMB_GUARDIAN_X, ch->GetY() - PLAYERBOT_CATACOMB_GUARDIAN_Y) <= 1500)
							came.insert(*it);
						else
							s_mapPlayerBotCatacombBots.erase(*it);
					}
					raid.members = came;
					if (raid.members.find(raid.dwLeader) == raid.members.end())
						raid.dwLeader = *raid.members.begin();
					raid.bPhase = CATACOMB_PHASE_FLOOR1;
					raid.dwPhaseSince = dwNow;
					raid.dwLastProgress = dwNow;
					sys_log(0, "PLAYERBOT_CATACOMB: down to the first floor members=%u", (unsigned int)raid.members.size());
				}
				else if (dwNow - raid.dwPhaseSince > PLAYERBOT_CATACOMB_GATHER_MS)
					EndPlayerBotCatacombRaid(dwNow, "too_few_came");
				break;
			case CATACOMB_PHASE_FLOOR1:
				if (inside > 0)
				{
					raid.bPhase = CATACOMB_PHASE_INSIDE;
					raid.lInstance = instance;
					raid.dwPhaseSince = dwNow;
					raid.dwLastProgress = dwNow;
					sys_log(0, "PLAYERBOT_CATACOMB: inside instance=%ld members_inside=%d", instance, inside);
				}
				else if (dwNow - raid.dwPhaseSince > PLAYERBOT_CATACOMB_KEY_HUNT_MS)
					EndPlayerBotCatacombRaid(dwNow, "no_key");
				break;
			case CATACOMB_PHASE_INSIDE:
				if (inside == 0 && onFloor1 == 0)
					EndPlayerBotCatacombRaid(dwNow, raid.bAzraelDown ? "done" : "left");
				else if (dwNow - raid.dwPhaseSince > PLAYERBOT_CATACOMB_RUN_MAX_MS)
					EndPlayerBotCatacombRaid(dwNow, "run_timeout");
				else if (dwNow - raid.dwLastProgress > PLAYERBOT_CATACOMB_STALL_MS)
					EndPlayerBotCatacombRaid(dwNow, "stalled");
				break;
			default:
				break;
		}
	}

	// The first floor: the party formed where they stand, the monsters by the
	// statue hunted, the key fetched and handed over.
	bool ManagePlayerBotCatacombFloor1(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotCatacombRaid& raid = s_PlayerBotCatacombRaid;
		// The party: the leader's, which the others join.
		LPCHARACTER leader = CHARACTER_MANAGER::instance().FindByPID(raid.dwLeader);
		if (leader && leader->GetMapIndex() == PLAYERBOT_MAP_CATACOMB)
		{
			LPPARTY party = leader->GetParty();
			if (ch == leader && (!party || party->GetLeaderPID() != leader->GetPlayerID()))
			{
				if (party)
					LeavePlayerBotParty(leader);
				party = CPartyManager::instance().CreateParty(leader);
				if (party)
				{
					party->Link(leader);
					party->SetParameter(PARTY_EXP_DISTRIBUTION_PARITY);
					sys_log(0, "PLAYERBOT_CATACOMB: party made leader=%s", leader->GetName());
				}
			}
			else if (ch != leader && party && party->GetLeaderPID() == leader->GetPlayerID() &&
					ch->GetParty() != party && party->GetMemberCount() < PLAYERBOT_CATACOMB_MAX_MEMBERS)
			{
				if (ch->GetParty())
					LeavePlayerBotParty(ch);
				party->Join(ch->GetPlayerID());
				party->Link(ch);
			}
		}

		if (KeepPlayerBotTowerAlive(ch, state, dwNow))
			return true;
		const TPlayerBotCatacombScan* scan = ScanPlayerBotCatacombMap(PLAYERBOT_MAP_CATACOMB, dwNow);
		const long statueX = PlayerBotCatacombX(PLAYERBOT_CATACOMB_STATUE_CELL[0]);
		const long statueY = PlayerBotCatacombY(PLAYERBOT_CATACOMB_STATUE_CELL[1]);

		// The key in the bag goes to the statue - once the party is whole on
		// this floor, which is what the jump takes in.
		const int keyCell = FindPlayerBotTowerItemCell(ch, PLAYERBOT_CATACOMB_KEY);
		if (keyCell >= 0 && ch->GetParty() && leader && ch->GetParty() == leader->GetParty())
		{
			LPCHARACTER statue = FindPlayerBotCatacombNpc(scan, PLAYERBOT_CATACOMB_NPC_STATUE, statueX, statueY);
			if (statue)
			{
				const int distance = DISTANCE_APPROX(ch->GetX() - statue->GetX(), ch->GetY() - statue->GetY());
				if (distance > 1200)
				{
					WalkPlayerBotCatacomb(ch, state, statue->GetX(), statue->GetY(), dwNow);
					return true;
				}
				if (ch->IsStateMove())
					ch->Stop();
				if (dwNow >= raid.dwNextClick)
				{
					raid.dwNextClick = dwNow + PLAYERBOT_CATACOMB_CLICK_RETRY_MS;
					const bool ok = ch->GiveItem(statue, TItemPos(INVENTORY, (WORD)keyCell));
					sys_log(0, "PLAYERBOT_CATACOMB: key to the statue pid=%u name=%s ok=%d party=%d",
							ch->GetPlayerID(), ch->GetName(), ok ? 1 : 0, ch->GetParty()->GetMemberCount());
				}
				return true;
			}
		}

		// The hunt: the monsters round the statue.
		if (DISTANCE_APPROX(ch->GetX() - statueX, ch->GetY() - statueY) > PLAYERBOT_CATACOMB_STATUE_HUNT_RANGE)
		{
			WalkPlayerBotCatacomb(ch, state, statueX, statueY, dwNow);
			return true;
		}
		if (BuffPlayerBotTowerFellows(ch, state, dwNow))
			return true;
		LPCHARACTER foe = PickPlayerBotCatacombFoe(ch, scan, 0, false);
		if (foe)
			return FightPlayerBotTowerObjective(ch, state, foe, dwNow);
		state.dwTargetVID = 0;
		return true;
	}

	// Inside the instance: the floor the dungeon's "level" flag names.
	bool ManagePlayerBotCatacombFloor(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotCatacombRaid& raid = s_PlayerBotCatacombRaid;
		const long map = ch->GetMapIndex();
		LPDUNGEON d = ch->GetDungeon();
		if (!d)
		{
			d = CDungeonManager::instance().FindByMapIndex(map);
			if (d)
				ch->SetDungeon(d);
		}
		if (!d)
		{
			SendPlayerBotOutOfCatacomb(ch);
			return true;
		}
		const int floor = d->GetFlag("level");
		TPlayerBotCatacombBot& self = s_mapPlayerBotCatacombBots[ch->GetPlayerID()];
		// A jump inside the instance is a Show on the same map: the route and
		// the target belong to the floor left behind.
		// The fourth floor's flag goes up three seconds before its jump, so a
		// move of more than a run of a few ticks is a jump too.
		const bool jumped = self.lLastX != 0 &&
				DISTANCE_APPROX(ch->GetX() - self.lLastX, ch->GetY() - self.lLastY) > 3000;
		self.lLastX = ch->GetX();
		self.lLastY = ch->GetY();
		if (self.iFloor != floor || jumped)
		{
			self.iFloor = floor;
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ClearPlayerBotRoute(state, true);
			if (ch->IsStateMove())
				ch->Stop();
		}
		if (floor != raid.iFloor)
		{
			sys_log(0, "PLAYERBOT_CATACOMB: floor %d instance=%ld after_min=%u", floor, map,
					(dwNow - raid.dwCalledAt) / 60000U);
			raid.iFloor = floor;
			raid.dwFloorSince = dwNow;
			raid.dwLastProgress = dwNow;
			raid.dwNextClick = 0;
		}

		if (KeepPlayerBotTowerAlive(ch, state, dwNow))
			return true;
		if (ch->IsRiding() && !HasPlayerBotBattleHorse(ch))
		{
			SetPlayerBotRidingForTravel(ch, state, false, dwNow, "catacomb");
			return true;
		}
		const TPlayerBotCatacombScan* scan = ScanPlayerBotCatacombMap(map, dwNow);
		if (scan->alive != raid.iAlive)
		{
			raid.iAlive = scan->alive;
			raid.dwLastProgress = dwNow;
		}
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(map);
		const long baseX = pMap ? pMap->m_setting.iBaseX : PlayerBotCatacombX(0);
		const long baseY = pMap ? pMap->m_setting.iBaseY : PlayerBotCatacombY(0);
		const bool leader = ch->GetPlayerID() == raid.dwLeader ||
				!CHARACTER_MANAGER::instance().FindByPID(raid.dwLeader);
		// A person in the instance does the clicking; the bots only fight.
		const bool clicks = leader && !scan->bPerson;

		if (BuffPlayerBotTowerFellows(ch, state, dwNow))
			return true;

		// The floor's objective.
		switch (floor)
		{
			case 2:
			{
				LPCHARACTER threat = PickPlayerBotCatacombFoe(ch, scan, 0, false);
				if (threat && DISTANCE_APPROX(ch->GetX() - threat->GetX(), ch->GetY() - threat->GetY()) <= PLAYERBOT_CATACOMB_THREAT_RANGE)
					return FightPlayerBotTowerObjective(ch, state, threat, dwNow);
				bool allOpen = false;
				LPCHARACTER door = PickPlayerBotCatacombDoor(scan, baseX, baseY, allOpen);
				if (door)
					return FightPlayerBotTowerObjective(ch, state, door, dwNow);
				if (allOpen && clicks)
				{
					LPCHARACTER rock = FindPlayerBotCatacombNpc(scan, PLAYERBOT_CATACOMB_NPC_ROCK,
							baseX + PLAYERBOT_CATACOMB_ROCK_CELL[0] * 100, baseY + PLAYERBOT_CATACOMB_ROCK_CELL[1] * 100);
					if (rock)
					{
						if (DISTANCE_APPROX(ch->GetX() - rock->GetX(), ch->GetY() - rock->GetY()) > 800)
						{
							WalkPlayerBotCatacomb(ch, state, rock->GetX(), rock->GetY(), dwNow);
							return true;
						}
						if (dwNow >= raid.dwNextClick)
						{
							raid.dwNextClick = dwNow + PLAYERBOT_CATACOMB_CLICK_RETRY_MS;
							const bool ok = quest::CQuestManager::instance().Click(ch->GetPlayerID(), rock);
							sys_log(0, "PLAYERBOT_CATACOMB: the turtle rock pid=%u name=%s ok=%d", ch->GetPlayerID(), ch->GetName(), ok ? 1 : 0);
						}
						return true;
					}
				}
				break;
			}
			case 3:
			{
				// The Metins of Revenge, one of which is real and ends the floor:
				// what hits the bot first, then the stone nearest the pack wherever
				// it stands on this ground, and only then the monsters - four
				// hundred of them, born twice, stand between the stones.
				LPCHARACTER threat = PickPlayerBotCatacombFoe(ch, scan, 0, false);
				if (threat && DISTANCE_APPROX(ch->GetX() - threat->GetX(), ch->GetY() - threat->GetY()) <= PLAYERBOT_CATACOMB_THREAT_RANGE)
					return FightPlayerBotTowerObjective(ch, state, threat, dwNow);
				const long fromX = scan->packN >= 2 ? scan->packX : ch->GetX();
				const long fromY = scan->packN >= 2 ? scan->packY : ch->GetY();
				const DWORD ground = GetPlayerBotCatacombGround(map, ch->GetX(), ch->GetY());
				DWORD stoneVid = 0;
				int best = INT_MAX;
				for (size_t i = 0; i < scan->entities.size(); ++i)
				{
					const TPlayerBotCatacombEntity& e = scan->entities[i];
					if (e.kind != CATACOMB_KIND_STONE)
						continue;
					if (ground != 0 && GetPlayerBotCatacombGround(map, e.x, e.y) != ground)
						continue;
					const int d = DISTANCE_APPROX(fromX - e.x, fromY - e.y);
					if (d < best)
					{
						best = d;
						stoneVid = e.vid;
					}
				}
				LPCHARACTER stone = stoneVid ? CHARACTER_MANAGER::instance().Find(stoneVid) : NULL;
				if (stone)
					return FightPlayerBotTowerObjective(ch, state, stone, dwNow);
				if (threat)
					return FightPlayerBotTowerObjective(ch, state, threat, dwNow);
				break;
			}
			case 4:
			{
				LPCHARACTER threat = PickPlayerBotCatacombFoe(ch, scan, 0, false);
				if (threat && DISTANCE_APPROX(ch->GetX() - threat->GetX(), ch->GetY() - threat->GetY()) <= PLAYERBOT_CATACOMB_THREAT_RANGE)
					return FightPlayerBotTowerObjective(ch, state, threat, dwNow);
				const long stakeX = baseX + PLAYERBOT_CATACOMB_STAKE_CELL[0] * 100;
				const long stakeY = baseY + PLAYERBOT_CATACOMB_STAKE_CELL[1] * 100;
				LPCHARACTER stake = FindPlayerBotCatacombNpc(scan, PLAYERBOT_CATACOMB_NPC_STAKE, stakeX, stakeY);
				const DWORD stakeGround = stake ? GetPlayerBotCatacombGround(map, stake->GetX(), stake->GetY()) : 0;
				const DWORD myGround = GetPlayerBotCatacombGround(map, ch->GetX(), ch->GetY());
				const bool overdue = dwNow - raid.dwFloorSince > PLAYERBOT_CATACOMB_MAZE_MAX_MS;
				if (stake && clicks && (myGround == stakeGround || overdue))
				{
					if (!overdue && DISTANCE_APPROX(ch->GetX() - stake->GetX(), ch->GetY() - stake->GetY()) > 800)
					{
						WalkPlayerBotCatacomb(ch, state, stake->GetX(), stake->GetY(), dwNow);
						return true;
					}
					if (dwNow >= raid.dwNextClick)
					{
						raid.dwNextClick = dwNow + PLAYERBOT_CATACOMB_CLICK_RETRY_MS;
						const bool ok = quest::CQuestManager::instance().Click(ch->GetPlayerID(), stake);
						sys_log(0, "PLAYERBOT_CATACOMB: the rune stake pid=%u name=%s ok=%d overdue=%d",
								ch->GetPlayerID(), ch->GetName(), ok ? 1 : 0, overdue ? 1 : 0);
					}
					return true;
				}
				if (myGround != stakeGround)
				{
					LPCHARACTER door = PickPlayerBotCatacombMazeDoor(ch, scan, stakeGround);
					if (door)
					{
						// Up to the door: the engine moves whoever stands within 300.
						WalkPlayerBotCatacomb(ch, state, door->GetX(), door->GetY(), dwNow);
						return true;
					}
				}
				else if (stake && DISTANCE_APPROX(ch->GetX() - stake->GetX(), ch->GetY() - stake->GetY()) > 1500)
				{
					WalkPlayerBotCatacomb(ch, state, stake->GetX(), stake->GetY(), dwNow);
					return true;
				}
				if (threat)
					return FightPlayerBotTowerObjective(ch, state, threat, dwNow);
				break;
			}
			case 5:
			{
				// Tartar's totem to the obelisk; Tartar first, then his floor.
				const int totemCell = FindPlayerBotTowerItemCell(ch, PLAYERBOT_CATACOMB_TOTEM);
				if (totemCell >= 0)
				{
					LPCHARACTER obelisk = FindPlayerBotCatacombNpc(scan, PLAYERBOT_CATACOMB_NPC_OBELISK,
							baseX + PLAYERBOT_CATACOMB_OBELISK_CELL[0] * 100, baseY + PLAYERBOT_CATACOMB_OBELISK_CELL[1] * 100);
					if (obelisk)
					{
						if (DISTANCE_APPROX(ch->GetX() - obelisk->GetX(), ch->GetY() - obelisk->GetY()) > 1200)
						{
							WalkPlayerBotCatacomb(ch, state, obelisk->GetX(), obelisk->GetY(), dwNow);
							return true;
						}
						if (dwNow >= raid.dwNextClick)
						{
							raid.dwNextClick = dwNow + PLAYERBOT_CATACOMB_CLICK_RETRY_MS;
							const bool ok = ch->GiveItem(obelisk, TItemPos(INVENTORY, (WORD)totemCell));
							sys_log(0, "PLAYERBOT_CATACOMB: the totem to the obelisk pid=%u name=%s ok=%d",
									ch->GetPlayerID(), ch->GetName(), ok ? 1 : 0);
						}
						return true;
					}
				}
				LPCHARACTER foe = PickPlayerBotCatacombFoe(ch, scan, PLAYERBOT_CATACOMB_TARTAR, false);
				if (!foe)
				{
					// Tartar stands at one of five points, maybe out of the pack's
					// reach: the nearest of him anywhere on the floor.
					LPCHARACTER tartar = FindPlayerBotCatacombNpc(scan, PLAYERBOT_CATACOMB_TARTAR, ch->GetX(), ch->GetY());
					if (tartar && !tartar->IsDead())
						foe = tartar;
				}
				if (foe)
					return FightPlayerBotTowerObjective(ch, state, foe, dwNow);
				break;
			}
			case 6:
			case 7:
			{
				if (floor == 7)
				{
					bool azrael = false;
					for (size_t i = 0; i < scan->entities.size() && !azrael; ++i)
						azrael = scan->entities[i].race == PLAYERBOT_CATACOMB_AZRAEL;
					if (azrael)
						raid.bAzraelSeen = true;
					else if (raid.bAzraelSeen && !raid.bAzraelDown && dwNow - raid.dwFloorSince > 5000)
					{
						raid.bAzraelDown = true;
						AnnouncePlayerBotAzrael(raid, dwNow);
					}
				}
				LPCHARACTER foe = PickPlayerBotCatacombFoe(ch, scan,
						floor == 6 ? PLAYERBOT_CATACOMB_CHARON : PLAYERBOT_CATACOMB_AZRAEL, true);
				if (!foe)
				{
					// The count the quest waits for is every monster of the floor:
					// the nearest one left anywhere, walked to.
					DWORD bestVid = 0;
					int best = INT_MAX;
					const DWORD ground = GetPlayerBotCatacombGround(map, ch->GetX(), ch->GetY());
					for (size_t i = 0; i < scan->entities.size(); ++i)
					{
						const TPlayerBotCatacombEntity& e = scan->entities[i];
						if (e.kind != CATACOMB_KIND_MONSTER && e.kind != CATACOMB_KIND_STONE)
							continue;
						if (ground != 0 && GetPlayerBotCatacombGround(map, e.x, e.y) != ground)
							continue;
						const int dd = DISTANCE_APPROX(ch->GetX() - e.x, ch->GetY() - e.y);
						if (dd < best)
						{
							best = dd;
							bestVid = e.vid;
						}
					}
					foe = bestVid ? CHARACTER_MANAGER::instance().Find(bestVid) : NULL;
				}
				if (foe)
					return FightPlayerBotTowerObjective(ch, state, foe, dwNow);
				break;
			}
			default:
				break;
		}
		// Nothing to do: the floor's own script is at work (a jump in seconds),
		// or the pack is elsewhere.
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		if (scan->packN >= 2 && DISTANCE_APPROX(ch->GetX() - scan->packX, ch->GetY() - scan->packY) > 1500)
			WalkPlayerBotCatacomb(ch, state, scan->packX, scan->packY, dwNow);
		else if (ch->IsStateMove())
			ch->Stop();
		return true;
	}

	// The per-bot pass: a member's whole tick while the raid lasts, and any
	// bot in the Catacomb that is no member sent out of it.
	bool ManagePlayerBotCatacomb(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead())
			return false;
		const long map = ch->GetMapIndex();
		const bool inCatacomb = map == PLAYERBOT_MAP_CATACOMB || IsPlayerBotCatacombInstance(map);
		if (!IsPlayerBotCatacombRaider(ch->GetPlayerID()))
		{
			// A bot in a person's party, or a person's companion, is the
			// person's to take in and out.
			if (!inCatacomb || IsPlayerBotSidekickPID(ch->GetPlayerID()) ||
					(ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty())))
				return false;
			if (dwNow >= state.dwNextTowerMoveTime)
			{
				state.dwNextTowerMoveTime = dwNow + 5000;
				sys_log(0, "PLAYERBOT_CATACOMB: no raider, leaving pid=%u name=%s map=%ld",
						ch->GetPlayerID(), ch->GetName(), map);
				SendPlayerBotOutOfCatacomb(ch);
			}
			return true;
		}
		TPlayerBotCatacombRaid& raid = s_PlayerBotCatacombRaid;
		if (IsPlayerBotCatacombInstance(map))
			return ManagePlayerBotCatacombFloor(ch, state, dwNow);
		if (map == PLAYERBOT_MAP_CATACOMB)
		{
			// Back on the first floor with the raid inside: this one is out of it.
			if (raid.bPhase == CATACOMB_PHASE_INSIDE)
			{
				raid.members.erase(ch->GetPlayerID());
				s_mapPlayerBotCatacombBots.erase(ch->GetPlayerID());
				if (ch->GetParty() && !IsPlayerBotHumanLedParty(ch->GetParty()))
					LeavePlayerBotParty(ch);
				SendPlayerBotOutOfCatacomb(ch);
				return true;
			}
			return ManagePlayerBotCatacombFloor1(ch, state, dwNow);
		}
		// Out of the Catacomb with the raid inside or over it: no longer its.
		if (raid.bPhase == CATACOMB_PHASE_INSIDE || raid.bPhase == CATACOMB_PHASE_NONE)
		{
			raid.members.erase(ch->GetPlayerID());
			s_mapPlayerBotCatacombBots.erase(ch->GetPlayerID());
			return false;
		}
		if (KeepPlayerBotTowerAlive(ch, state, dwNow))
			return true;
		// The gathering by the Guardian, and the way down once it is over.
		if (map != PLAYERBOT_MAP_HWANG)
		{
			if (dwNow >= state.dwNextTowerMoveTime)
			{
				state.dwNextTowerMoveTime = dwNow + 5000;
				TransitionPlayerBotMap(ch, state, PLAYERBOT_MAP_HWANG, PLAYERBOT_CATACOMB_EXIT_CELL_X * 100,
						PLAYERBOT_CATACOMB_EXIT_CELL_Y * 100, dwNow, "catacomb_gather");
			}
			return true;
		}
		if (raid.bPhase == CATACOMB_PHASE_FLOOR1)
		{
			// Down, as the Guardian's dialog sends a player, and the quest's
			// clock for the next run started as its first floor starts it.
			if (dwNow >= state.dwNextTowerMoveTime)
			{
				state.dwNextTowerMoveTime = dwNow + 5000;
				ch->SetQuestFlag("devilcatacomb_zone.last_exit_time", get_global_time());
				TransitionPlayerBotMap(ch, state, PLAYERBOT_MAP_CATACOMB,
						PlayerBotCatacombX(PLAYERBOT_CATACOMB_F1_ENTRY[0]), PlayerBotCatacombY(PLAYERBOT_CATACOMB_F1_ENTRY[1]),
						dwNow, "catacomb_entry");
			}
			return true;
		}
		const int toGuardian = DISTANCE_APPROX(ch->GetX() - PLAYERBOT_CATACOMB_GUARDIAN_X,
				ch->GetY() - PLAYERBOT_CATACOMB_GUARDIAN_Y);
		if (toGuardian > 800)
		{
			WalkPlayerBotCatacomb(ch, state, PLAYERBOT_CATACOMB_GUARDIAN_X, PLAYERBOT_CATACOMB_GUARDIAN_Y, dwNow);
			return true;
		}
		if (ch->IsStateMove())
			ch->Stop();
		state.dwTargetVID = 0;
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		return true;
	}

	bool DescribePlayerBotCatacombRaid(LPCHARACTER ch, bool en, char* out, size_t size)
	{
		if (!ch || !IsPlayerBotCatacombRaider(ch->GetPlayerID()))
			return false;
		const TPlayerBotCatacombRaid& raid = s_PlayerBotCatacombRaid;
		if (IsPlayerBotCatacombInstance(ch->GetMapIndex()))
		{
			LPDUNGEON d = ch->GetDungeon();
			snprintf(out, size, PBT(en, "Katakumby Diabla: pietro %d", "Devil's Catacomb: floor %d"),
					d ? d->GetFlag("level") : 0);
		}
		else if (ch->GetMapIndex() == PLAYERBOT_MAP_CATACOMB)
			snprintf(out, size, "%s", PBT(en, "Katakumby Diabla: szukam klucza", "Devil's Catacomb: hunting the key"));
		else
			snprintf(out, size, "%s", PBT(en, "Zbiorka przy Strazniku Katakumb", "Gathering at the Catacomb's Guardian"));
		(void) raid;
		return true;
	}
#else
	// r40250 has neither the map nor the quest.
	bool IsPlayerBotCatacombRaider(DWORD pid)
	{
		(void) pid;
		return false;
	}

	void ManagePlayerBotCatacombRaids(DWORD dwNow)
	{
		(void) dwNow;
	}

	bool ManagePlayerBotCatacomb(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		(void) ch;
		(void) state;
		(void) dwNow;
		return false;
	}

	bool DescribePlayerBotCatacombRaid(LPCHARACTER ch, bool en, char* out, size_t size)
	{
		(void) ch;
		(void) en;
		(void) out;
		(void) size;
		return false;
	}
#endif
}

#endif
