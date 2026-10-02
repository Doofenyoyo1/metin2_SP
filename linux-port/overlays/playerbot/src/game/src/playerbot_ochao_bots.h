#ifndef __INC_METIN2_PLAYERBOT_OCHAO_BOTS_H__
#define __INC_METIN2_PLAYERBOT_OCHAO_BOTS_H__

// MT2009_PLUS_OCHAO_BOTS_V1 - the bots in the Temple of Ochao (Swiatynia
// Ochao, map 209, metin2_map_mt_th_dungeon_01; the map and the En-Tai
// Guardian's clock are playerbot_ochao.h's, the quest is
// temple_of_the_ochao.quest).
//
// What a bot has to know about the place, measured on the map's own files
// (server_attr decoded at 50 units, regen.txt, boss.txt, npc.txt, Town.txt;
// the generator is tools/ochao_bot_map.py):
//
//   * the way in: a bot of PLAYERBOT_OCHAO_MIN_LEVEL (95, the quest's gate)
//     goes to Orc Valley, walks to Straznik Swiatyni (20426, cell 289,1448
//     beside Koe-Pung) and is taken to the temple's gate (Town.txt 89,84),
//     the same warp the quest gives a player;
//   * the map: one walkable piece of 582 236 cells, a square labyrinth of
//     concentric corridors round a hall in the middle - the gate is its
//     top-left corner and the walk from there to the middle is about
//     190 km, against 43 km in a straight line;
//   * where to hunt: PLAYERBOT_OCHAO_HUBS, the spawn lines clustered, and
//     three boss rows - Straznik En-Tai (6400, walks between eleven rooms),
//     the Ochao Bodyguard (6311, three points, hourly) and the Ochao Lord
//     (6390, hourly), each one row because a boss hub follows its boss;
//   * the way out: the Teleporter (9012, npc.txt 400,385) in the middle hall
//     sends a character to its villages, at his outside-the-city price, and
//     for a minute after the Guardian falls his Portal (20415) opens where he
//     fell and leads to Orc Valley beside Koe-Pung. A bot that leaves walks
//     PLAYERBOT_OCHAO_EXIT_TREE - every corner of the shortest walk from any
//     hunting spot, room or the gate to the Teleporter - one straight leg at
//     a time, or takes the Portal when it stands open within reach.
//
// Leaving is the frontier's own decision (ManagePlayerBotWorldTravel): the
// visit clock, a full bag, potions running out, no weapon; only the way out
// is this file's. And a test hook, for the operator: a file
// "playerbot_ochao_test" in the core's directory ("send 150", "leave 30",
// "leaveall", "reset"), plus a position log (playerbot_ochao_track.tsv)
// and OCHAO_BOT lines in syslog.
//
// An implementation fragment in the sense playerbot_types.h describes:
// include it once, after playerbot_ochao.h and before playerbot_travel.h.

namespace
{
	// From playerbot_travel.h, which comes after this file.
	bool TransitionPlayerBotMap(LPCHARACTER ch, TPlayerBotAIState& state,
			long targetMap, long targetX, long targetY, DWORD dwNow, const char* reason);
	bool MovePlayerBotToWorldPortal(LPCHARACTER ch, TPlayerBotAIState& state,
			long portalX, long portalY, long targetMap, long targetX, long targetY,
			DWORD dwNow, const char* reason);
	bool GetPlayerBotFrontierArrivalFor(LPCHARACTER ch, long mapIndex, long& outX, long& outY);
	bool BlocksPlayerBotTravel(LPCHARACTER ch);
	long GetPlayerBotFrontierMapForLevel(LPCHARACTER ch);
	bool IsPlayerBotHumanLedParty(LPPARTY party);
	// From playerbot_arezzo_bots.h, which comes after this file.
	bool IsPlayerBotArezzoBoundForLas(LPCHARACTER ch);
	bool IsPlayerBotArezzoBound(LPCHARACTER ch);

	// Straznik Swiatyni in Orc Valley: the point walked to is where the
	// quest's Portal puts a player down, two steps from him.
	const long PLAYERBOT_OCHAO_GUARDIAN_X = 284700;
	const long PLAYERBOT_OCHAO_GUARDIAN_Y = 810600;
	// The Portal's way out (the quest's pc.warp).
	const long PLAYERBOT_OCHAO_PORTAL_TO_X = 284700;
	const long PLAYERBOT_OCHAO_PORTAL_TO_Y = 810600;
	// A bot takes the open Portal when it stands this near, in a straight
	// line, and the navigation joins the two.
	const int PLAYERBOT_OCHAO_PORTAL_REACH = 7000;
	// A corner of the way out is passed within this many units.
	const int PLAYERBOT_OCHAO_NODE_REACHED = 350;
	// A leg that has not shortened by this much in this long is a stall; after
	// this many stalls the bot walks the rest on the navigation alone.
	const int PLAYERBOT_OCHAO_LEG_PROGRESS = 250;
	const DWORD PLAYERBOT_OCHAO_LEG_TIMEOUT = 15000;
	const int PLAYERBOT_OCHAO_MAX_STALLS = 4;
	// How far a bot looks for a corner of the way out it can see.
	const int PLAYERBOT_OCHAO_NODE_SIGHT = 30000;
	// Before this long in the temple only what stops the fight (no weapon, no
	// potions, BlocksPlayerBotTravel) takes a bot out: the way in and out is
	// some six minutes of walking, and two of hunting were not worth it.
	const DWORD PLAYERBOT_OCHAO_MIN_VISIT_TIME = 12 * 60 * 1000;
	// The temple's boss raids (playerbot_boss_raid.h): a recruit is a bot
	// inside within this walk of the boss, and the gathering waits this long -
	// the labyrinth's walks are longer than the open maps'.
	const int PLAYERBOT_OCHAO_RAID_WALK_MAX = 60000;
	const DWORD PLAYERBOT_OCHAO_RAID_GATHER_MS = 5 * 60 * 1000;
	// The muster: the raid meets out of his group's reach - the corner of the
	// way out this far from him along the corridors - and goes in together
	// (a member arriving alone at his side was his escort's, one by one). A
	// member within this of the muster has come.
	const int PLAYERBOT_OCHAO_RAID_MUSTER_WALK = 4500;
	const int PLAYERBOT_OCHAO_RAID_MUSTER_ARRIVED = 1500;
	// He has walked this far from where the muster was chosen: chosen again.
	const int PLAYERBOT_OCHAO_RAID_MUSTER_MOVE = 4000;
	// Every this often while it gathers, a member sent back to the gate (or
	// otherwise out of the walk) is let go and the raid is filled up again
	// from the nearest free bots of its kingdom.
	const DWORD PLAYERBOT_OCHAO_RAID_TOPUP_MS = 30000;
	// Let go only past this walk (the gate is 45-100 km from the bosses; the
	// walk's estimate moves a few km as the nearest spot changes).
	const int PLAYERBOT_OCHAO_RAID_DROP_WALK = PLAYERBOT_OCHAO_RAID_WALK_MAX + 30000;
	// In the fight, a member further from him than this walks the corners to
	// him (WalkPlayerBotInOchao) instead of being sent at him straight.
	const int PLAYERBOT_OCHAO_RAID_FIGHT_WALK = 3000;
	// Monitoring: a bot on the map that has not moved this far in this long,
	// and has fought nothing in the last ten seconds, counts as stuck.
	const int PLAYERBOT_OCHAO_STUCK_DISTANCE = 200;
	const DWORD PLAYERBOT_OCHAO_STUCK_MS = 60000;
	const DWORD PLAYERBOT_OCHAO_TRACK_MS = 15000;
	const char* const PLAYERBOT_OCHAO_TEST_FILE = "playerbot_ochao_test";
	const char* const PLAYERBOT_OCHAO_TRACK_FILE = "playerbot_ochao_track.tsv";

	// The hunting spots: the regen's spawn lines clustered in 3200-unit
	// windows, the densest first, each snapped to a cell with five open cells
	// all round and at least 5000 from the next (22 spots, 293 regen lines).
	const TPlayerBotHuntingHub PLAYERBOT_OCHAO_HUBS[] = {
		{ 877875, 1419175, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 912825, 1416875, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 899625, 1417275, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 853725, 1466875, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 869675, 1423675, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 857975, 1423025, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 851675, 1424575, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 907775, 1441575, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 886525, 1470725, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 911725, 1466175, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 906325, 1458125, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 892575, 1416325, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 890225, 1421975, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 914475, 1427925, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 909625, 1429475, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 892975, 1427775, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 860475, 1434225, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 901775, 1449575, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 867675, 1434075, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 875125, 1444975, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 875975, 1453325, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		{ 890875, 1476625, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 0 },
		// The bosses: a hub is wherever the boss stands (IsPlayerBotBossAlive
		// asks the whole map), so one row each. Straznik En-Tai walks between
		// the eleven rooms (playerbot_ochao.h); the Ochao Bodyguard (6311) and
		// the Ochao Lord (6390) come from boss.txt every hour.
		{ 864100, 1422500, PLAYERBOT_OCHAO_MIN_LEVEL, 255, false, 6400 },
		// The Ochao Bodyguard (6311) and the Ochao Lord (6390) are the boss
		// raid's (playerbot_boss_raid.h): called, gathered and walked there
		// together, not a hub any passer-by takes.
	};
	// The way out, as a tree rooted at the Teleporter: every node's next node
	// is the next corner towards him on the shortest walk (Dijkstra on the
	// map's server_attr at 50 units, corridor middles preferred), each edge
	// a straight line with three open cells either side. The leaves are the
	// gate, the eleven rooms, the four boss points and every hunting spot.
	struct TPlayerBotOchaoNode { long x; long y; short next; };
	const TPlayerBotOchaoNode PLAYERBOT_OCHAO_EXIT_TREE[] = {
		{ 884825, 1446525, -1 }, { 884825, 1450975, 0 }, { 885525, 1454125, 1 }, { 889375, 1454425, 2 },
		{ 889875, 1454225, 3 }, { 891025, 1450975, 4 }, { 894275, 1449525, 5 }, { 894525, 1438625, 6 },
		{ 894175, 1437975, 7 }, { 872675, 1437875, 8 }, { 872175, 1438225, 9 }, { 872075, 1457675, 10 },
		{ 872425, 1458325, 11 }, { 879375, 1458425, 12 }, { 881525, 1459525, 13 }, { 881625, 1460775, 14 },
		{ 882725, 1463825, 15 }, { 898175, 1464025, 16 }, { 898825, 1463675, 17 }, { 898925, 1434125, 18 },
		{ 898675, 1433475, 19 }, { 895525, 1432225, 20 }, { 894175, 1427975, 21 }, { 892675, 1427775, 22 },
		{ 890875, 1426575, 23 }, { 890775, 1424725, 24 }, { 889675, 1422575, 25 }, { 884725, 1422375, 26 },
		{ 878175, 1421275, 27 }, { 878075, 1420325, 28 }, { 876825, 1417025, 29 }, { 853725, 1416425, 30 },
		{ 886375, 1437875, 8 }, { 885275, 1436775, 32 }, { 885175, 1435875, 33 }, { 883975, 1432575, 34 },
		{ 868225, 1432475, 35 }, { 867775, 1432725, 36 }, { 867675, 1436925, 37 }, { 866625, 1439675, 38 },
		{ 865725, 1439825, 39 }, { 863275, 1440725, 40 }, { 863175, 1467425, 41 }, { 863375, 1468175, 42 },
		{ 868925, 1469525, 43 }, { 869025, 1472075, 44 }, { 868825, 1472575, 45 }, { 857925, 1472725, 46 },
		{ 857475, 1472475, 47 }, { 857375, 1453425, 48 }, { 856275, 1446875, 49 }, { 854525, 1446775, 50 },
		{ 851975, 1445475, 51 }, { 851875, 1423475, 52 }, { 852225, 1422975, 53 }, { 864125, 1422525, 54 },
		{ 856075, 1422875, 54 }, { 856425, 1423225, 56 }, { 857125, 1429625, 57 }, { 863425, 1468225, 43 },
		{ 867725, 1468325, 59 }, { 876575, 1468425, 60 }, { 877225, 1468075, 61 }, { 877325, 1465425, 62 },
		{ 876975, 1464775, 63 }, { 869825, 1464675, 64 }, { 867675, 1463575, 65 }, { 867225, 1446325, 66 },
		{ 889425, 1463925, 16 }, { 890625, 1465125, 68 }, { 890675, 1468025, 69 }, { 887475, 1469525, 70 },
		{ 887425, 1472425, 71 }, { 890625, 1473925, 72 }, { 890725, 1477675, 73 }, { 890475, 1478125, 74 },
		{ 879625, 1478825, 75 }, { 890575, 1468125, 70 }, { 888675, 1468325, 77 }, { 882325, 1468825, 78 },
		{ 891175, 1450825, 5 }, { 893125, 1450675, 80 }, { 894225, 1451025, 81 }, { 894525, 1458625, 82 },
		{ 894175, 1459125, 83 }, { 887825, 1459625, 84 }, { 882925, 1446525, 0 }, { 881475, 1446275, 86 },
		{ 881375, 1444075, 87 }, { 881975, 1442525, 88 }, { 888275, 1442375, 89 }, { 888775, 1442575, 90 },
		{ 889225, 1446225, 91 }, { 875875, 1432475, 35 }, { 874775, 1431375, 93 }, { 874575, 1428575, 94 },
		{ 874925, 1428075, 95 }, { 883625, 1427525, 96 }, { 898575, 1433375, 20 }, { 896575, 1433275, 98 },
		{ 889425, 1432725, 99 }, { 890775, 1423675, 25 }, { 890775, 1423425, 101 }, { 891225, 1422575, 102 },
		{ 904025, 1421925, 103 }, { 898925, 1451175, 18 }, { 899925, 1450175, 105 }, { 903075, 1448825, 106 },
		{ 904375, 1442125, 107 }, { 908625, 1440675, 108 }, { 909425, 1423225, 109 }, { 893225, 1422475, 103 },
		{ 899275, 1422375, 111 }, { 899825, 1421825, 112 }, { 867575, 1448875, 66 }, { 866925, 1448225, 114 },
		{ 883725, 1478325, 75 }, { 883125, 1478925, 116 }, { 897825, 1422375, 111 }, { 898925, 1421275, 118 },
		{ 899025, 1417925, 119 }, { 898875, 1417075, 120 }, { 888125, 1416225, 121 }, { 878075, 1419375, 29 },
		{ 877875, 1419175, 123 }, { 908675, 1440625, 109 }, { 908825, 1431775, 125 }, { 909975, 1430625, 126 },
		{ 914225, 1429275, 127 }, { 914525, 1417625, 128 }, { 914275, 1416975, 129 }, { 912825, 1416875, 130 },
		{ 899025, 1417875, 120 }, { 899625, 1417275, 132 }, { 857375, 1468075, 48 }, { 856275, 1466975, 134 },
		{ 853725, 1466875, 135 }, { 879275, 1422375, 27 }, { 871625, 1422375, 137 }, { 870175, 1422525, 138 },
		{ 869675, 1423675, 139 }, { 857825, 1422875, 56 }, { 857975, 1423025, 141 }, { 851875, 1424775, 52 },
		{ 851675, 1424575, 143 }, { 904425, 1442075, 108 }, { 907325, 1441975, 145 }, { 907775, 1441575, 146 },
		{ 887375, 1469875, 71 }, { 886525, 1470725, 148 }, { 890725, 1466925, 69 }, { 891875, 1468075, 150 },
		{ 898525, 1469425, 151 }, { 899925, 1472525, 152 }, { 908025, 1472825, 153 }, { 908675, 1472625, 154 },
		{ 909975, 1467125, 155 }, { 911725, 1466175, 156 }, { 892025, 1468225, 151 }, { 897525, 1468425, 158 },
		{ 902575, 1468425, 159 }, { 903225, 1468075, 160 }, { 904175, 1459525, 161 }, { 906325, 1458125, 162 },
		{ 898825, 1417025, 121 }, { 893125, 1416875, 164 }, { 892575, 1416325, 165 }, { 890775, 1423025, 102 },
		{ 890225, 1421975, 167 }, { 914325, 1428075, 128 }, { 914475, 1427925, 169 }, { 908825, 1430275, 126 },
		{ 909625, 1429475, 171 }, { 892975, 1427775, 22 }, { 857375, 1447975, 49 }, { 857375, 1435625, 174 },
		{ 857575, 1435125, 175 }, { 860475, 1434225, 176 }, { 900025, 1450075, 106 }, { 901475, 1449875, 178 },
		{ 901775, 1449575, 179 }, { 867675, 1434075, 37 }, { 881375, 1443375, 88 }, { 880575, 1442575, 182 },
		{ 877125, 1442275, 183 }, { 876275, 1442425, 184 }, { 875125, 1444975, 185 }, { 884825, 1453425, 1 },
		{ 884175, 1454125, 187 }, { 876575, 1454425, 188 }, { 876125, 1454125, 189 }, { 875975, 1453325, 190 },
		{ 890725, 1476475, 73 }, { 890875, 1476625, 192 }
	};

	// Walking distance between the hunting spots, in units (a breadth-first
	// walk on the map's server_attr, times 0.85 for the diagonal): the spot
	// choice measures the labyrinth by this, not by the straight line.
	const int PLAYERBOT_OCHAO_HUB_WALK[22][22] = {
		{ 0, 31662, 20442, 61072, 10795, 68467, 62475, 44455, 71910, 68722, 57290, 14917, 12877, 42117, 47302, 20145, 27582, 46155, 113135, 156697, 148877, 78837 },
		{ 31662, 0, 11560, 92735, 42457, 100130, 94137, 27837, 70677, 45390, 43137, 17680, 23545, 10795, 15980, 29537, 59245, 39737, 128392, 155465, 147645, 71995 },
		{ 20442, 11560, 0, 81175, 30897, 88570, 82577, 39057, 69742, 56610, 54357, 6800, 11985, 22015, 27200, 17977, 47685, 43987, 127457, 154530, 146710, 76670 },
		{ 61072, 92735, 81175, 0, 50277, 43690, 37697, 77222, 34382, 59670, 61922, 75990, 69190, 94520, 89080, 75437, 33490, 65322, 58607, 124950, 117130, 39865 },
		{ 10795, 42457, 30897, 50277, 0, 57672, 51680, 49470, 76925, 73737, 62305, 25712, 18912, 52912, 58097, 25160, 16787, 51170, 102340, 161712, 153892, 83597 },
		{ 68467, 100130, 88570, 43690, 57672, 0, 6672, 107142, 78072, 103360, 105612, 83385, 76585, 110585, 115770, 82832, 40885, 108842, 102297, 168640, 160820, 83555 },
		{ 62475, 94137, 82577, 37697, 51680, 6672, 0, 101150, 72080, 97367, 99620, 77392, 70592, 104592, 109777, 76840, 34892, 102850, 96305, 162647, 154827, 77562 },
		{ 44455, 27837, 39057, 77222, 49470, 107142, 101150, 0, 42840, 24267, 16915, 45517, 31577, 17297, 11857, 24310, 66257, 11900, 100555, 127627, 119807, 44157 },
		{ 71910, 70677, 69742, 34382, 76925, 78072, 72080, 42840, 0, 28517, 27540, 75182, 59032, 60137, 54697, 51765, 61327, 30940, 57715, 103317, 95497, 8712 },
		{ 68722, 45390, 56610, 59670, 73737, 103360, 97367, 24267, 28517, 0, 11432, 63070, 55845, 34850, 32980, 48577, 86615, 22567, 83002, 125035, 117215, 26605 },
		{ 57290, 43137, 54357, 61922, 62305, 105612, 99620, 16915, 27540, 11432, 0, 60562, 44412, 32597, 27157, 37145, 79092, 11135, 85255, 119807, 111987, 28857 },
		{ 14917, 17680, 6800, 75990, 25712, 83385, 77392, 45517, 75182, 63070, 60562, 0, 17425, 28475, 33660, 23417, 42500, 49427, 128052, 159970, 152150, 82110 },
		{ 12877, 23545, 11985, 69190, 18912, 76585, 70592, 31577, 59032, 55845, 44412, 17425, 0, 34000, 39185, 7267, 35700, 33277, 116747, 143820, 136000, 65960 },
		{ 42117, 10795, 22015, 94520, 52912, 110585, 104592, 17297, 60137, 34850, 32597, 28475, 34000, 0, 5440, 39992, 69700, 29197, 117852, 144925, 137105, 61455 },
		{ 47302, 15980, 27200, 89080, 58097, 115770, 109777, 11857, 54697, 32980, 27157, 33660, 39185, 5440, 0, 34552, 74885, 23757, 112412, 139485, 131665, 56015 },
		{ 20145, 29537, 17977, 75437, 25160, 82832, 76840, 24310, 51765, 48577, 37145, 23417, 7267, 39992, 34552, 0, 41947, 26010, 109480, 136552, 128732, 58692 },
		{ 27582, 59245, 47685, 33490, 16787, 40885, 34892, 66257, 61327, 86615, 79092, 42500, 35700, 69700, 74885, 41947, 0, 67957, 85552, 151895, 144075, 66810 },
		{ 46155, 39737, 43987, 65322, 51170, 108842, 102850, 11900, 30940, 22567, 11135, 49427, 33277, 29197, 23757, 26010, 67957, 0, 88655, 115727, 107907, 34552 },
		{ 113135, 128392, 127457, 58607, 102340, 102297, 96305, 100555, 57715, 83002, 85255, 128052, 116747, 117852, 112412, 109480, 85552, 88655, 0, 66342, 58522, 63197 },
		{ 156697, 155465, 154530, 124950, 161712, 168640, 162647, 127627, 103317, 125035, 119807, 159970, 143820, 144925, 139485, 136552, 151895, 115727, 66342, 0, 7820, 110245 },
		{ 148877, 147645, 146710, 117130, 153892, 160820, 154827, 119807, 95497, 117215, 111987, 152150, 136000, 137105, 131665, 128732, 144075, 107907, 58522, 7820, 0, 102425 },
		{ 78837, 71995, 76670, 39865, 83597, 83555, 77562, 44157, 8712, 26605, 28857, 82110, 65960, 61455, 56015, 58692, 66810, 34552, 63197, 110245, 102425, 0 }
	};
	const int PLAYERBOT_OCHAO_WALK_HUBS = 22; // the rows above: the spots, not the bosses
	const size_t PLAYERBOT_OCHAO_HUB_COUNT = sizeof(PLAYERBOT_OCHAO_HUBS) / sizeof(PLAYERBOT_OCHAO_HUBS[0]);
	const int PLAYERBOT_OCHAO_NODE_COUNT = (int)(sizeof(PLAYERBOT_OCHAO_EXIT_TREE) / sizeof(PLAYERBOT_OCHAO_EXIT_TREE[0]));

	// ------------------------------------------------------------ the state

	// A bot on its way in: sent to Orc Valley, walking to the Guardian.
	struct TPlayerBotOchaoCrossing { DWORD dwSince; };
	std::map<DWORD, TPlayerBotOchaoCrossing> s_mapPlayerBotOchaoCrossing;
	// A bot on its way out: the corner it walks to, and how the leg goes.
	struct TPlayerBotOchaoExit
	{
		int iNode;
		DWORD dwStarted;
		DWORD dwLegSince;
		int iLegBest;
		long lLegX, lLegY;
		BYTE bStalls;
		bool bNavOnly;
		bool bPortal;
		char szReason[40];
		TPlayerBotOchaoExit() : iNode(-1), dwStarted(0), dwLegSince(0), iLegBest(0),
				lLegX(0), lLegY(0), bStalls(0), bNavOnly(false), bPortal(false) { szReason[0] = 0; }
	};
	std::map<DWORD, TPlayerBotOchaoExit> s_mapPlayerBotOchaoExit;
	// Another pass's warp out of the temple (a shop's upkeep, a raid far
	// away, Uriel): no warp is made from the middle of the labyrinth - the bot
	// walks out to the Teleporter or the Portal first and goes from there.
	struct TPlayerBotOchaoPending
	{
		long lMap, lX, lY;
		DWORD dwSince;
		char szReason[40];
	};
	std::map<DWORD, TPlayerBotOchaoPending> s_mapPlayerBotOchaoPending;
	// A warp out of the temple is made within this reach of its Teleporter or
	// its open Portal; and a walk out that has not got there in this long
	// gives up and lets the warp through.
	const int PLAYERBOT_OCHAO_WARP_REACH = 450;
	const DWORD PLAYERBOT_OCHAO_PENDING_MAX_MS = 15 * 60 * 1000;
	// The operator's test: sent bots hunt here whatever their draw says, and
	// a leave order sends one out through the maze.
	std::set<DWORD> s_setPlayerBotOchaoForced;
	std::set<DWORD> s_setPlayerBotOchaoLeave;

	// What the monitoring keeps per bot.
	struct TPlayerBotOchaoTrack
	{
		DWORD dwEntered;		// first seen on the map this visit
		DWORD dwCrossStart;		// crossing began (0 if unknown)
		DWORD dwKills;
		DWORD dwBossKills;
		unsigned long long ullExp;
		DWORD dwDeaths;
		DWORD dwLastExp;
		DWORD dwLastNext;
		BYTE bLastLevel;
		bool bWasDead;
		long lAnchorX, lAnchorY;
		DWORD dwAnchorSince;
		bool bStuck;
		DWORD dwStuckEpisodes;
		DWORD dwStuckMs;
		DWORD dwVisitDeaths;
		DWORD adwDeathAt[2];	// the two last deaths, for the restart at the gate
		TPlayerBotOchaoTrack() : dwEntered(0), dwCrossStart(0), dwKills(0), dwBossKills(0), ullExp(0),
				dwDeaths(0), dwLastExp(0), dwLastNext(0), bLastLevel(0), bWasDead(false),
				lAnchorX(0), lAnchorY(0), dwAnchorSince(0), bStuck(false), dwStuckEpisodes(0), dwStuckMs(0),
				dwVisitDeaths(0) { adwDeathAt[0] = adwDeathAt[1] = 0; }
	};
	std::map<DWORD, TPlayerBotOchaoTrack> s_mapPlayerBotOchaoTrack;

	// The Guardian as the bots see him.
	DWORD s_dwPlayerBotOchaoGuardianVID = 0;
	DWORD s_dwPlayerBotOchaoGuardianSeen = 0;
	DWORD s_dwPlayerBotOchaoGuardianEngaged = 0;
	std::set<DWORD> s_setPlayerBotOchaoGuardianFighters;

	bool IsPlayerBotOchaoBot(LPCHARACTER ch)
	{
		return ch && ch->IsPC() && s_mapPlayerBotAIStates.find(ch->GetPlayerID()) != s_mapPlayerBotAIStates.end();
	}

	bool IsPlayerBotOchaoForced(LPCHARACTER ch)
	{
		return ch && s_setPlayerBotOchaoForced.count(ch->GetPlayerID()) != 0;
	}

	// A bot that dies this often in one visit is outclassed here and goes.
	const DWORD PLAYERBOT_OCHAO_VISIT_DEATHS_LEAVE = 6;
	// Two deaths within this long: the next revival is at the gate, the
	// temple's Town.txt, as "restart in town" puts a player - not back into
	// the pack that killed it.
	const DWORD PLAYERBOT_OCHAO_DEATH_WINDOW_MS = 240000;
	bool IsPlayerBotOchaoLeaveOrdered(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		if (s_setPlayerBotOchaoLeave.count(ch->GetPlayerID()) != 0)
			return true;
		std::map<DWORD, TPlayerBotOchaoTrack>::const_iterator t = s_mapPlayerBotOchaoTrack.find(ch->GetPlayerID());
		return t != s_mapPlayerBotOchaoTrack.end() && t->second.dwVisitDeaths >= PLAYERBOT_OCHAO_VISIT_DEATHS_LEAVE;
	}

	bool IsPlayerBotOchaoHosted()
	{
		return SECTREE_MANAGER::instance().GetMap(PLAYERBOT_MAP_OCHAO) != NULL &&
				SECTREE_MANAGER::instance().GetMap(PLAYERBOT_MAP_ORC_VALLEY) != NULL;
	}

	// The level draw: from ninety-five two bots in three go to the temple;
	// the third keeps the Grotto. A test's sent bot always does.
	bool WantsPlayerBotOchao(LPCHARACTER ch, DWORD draw, bool stoneHunter)
	{
		if (!ch || ch->GetLevel() < PLAYERBOT_OCHAO_MIN_LEVEL || !IsPlayerBotOchaoHosted())
			return false;
		// MT2009_PLUS_PROGRESSION_V2: the owner's upper limit.
		if (ch->GetLevel() > PLAYERBOT_OCHAO_MAX_LEVEL)
			return false;
		if (IsPlayerBotOchaoForced(ch))
			return true;
		return !stoneHunter && (draw % 3U) != 1;
	}

	// The spot a point belongs to: the nearest one it can see (of the four
	// nearest), or the nearest.
	int GetPlayerBotOchaoSpotOf(long x, long y, bool needSight)
	{
		int order[PLAYERBOT_OCHAO_WALK_HUBS];
		int dist[PLAYERBOT_OCHAO_WALK_HUBS];
		for (int i = 0; i < PLAYERBOT_OCHAO_WALK_HUBS; ++i)
		{
			order[i] = i;
			dist[i] = DISTANCE_APPROX(x - PLAYERBOT_OCHAO_HUBS[i].x, y - PLAYERBOT_OCHAO_HUBS[i].y);
		}
		std::sort(order, order + PLAYERBOT_OCHAO_WALK_HUBS, [&dist](int a, int b) { return dist[a] < dist[b]; });
		if (!needSight)
			return order[0];
		CPlayerBotNavigation& nav = CPlayerBotNavigation::instance(PLAYERBOT_MAP_OCHAO);
		if (nav.Init(PLAYERBOT_MAP_OCHAO))
			for (int k = 0; k < 4; ++k)
				if (nav.SegmentClearWorld(x, y, PLAYERBOT_OCHAO_HUBS[order[k]].x, PLAYERBOT_OCHAO_HUBS[order[k]].y))
					return order[k];
		return order[0];
	}

	// How far a bot walks to a point of the temple: to the spot it stands by,
	// the labyrinth between the two spots, and on. Asked by the spot choice
	// (ChoosePlayerBotHuntingHub) in place of the straight line.
	int GetPlayerBotOchaoWalk(LPCHARACTER ch, long x, long y)
	{
		static DWORD s_dwPid = 0, s_dwStamp = 0;
		static int s_iFrom = -1;
		const DWORD dwNow = get_dword_time();
		if (s_dwPid != ch->GetPlayerID() || dwNow - s_dwStamp > 2000 || s_iFrom < 0)
		{
			s_dwPid = ch->GetPlayerID();
			s_dwStamp = dwNow;
			s_iFrom = GetPlayerBotOchaoSpotOf(ch->GetX(), ch->GetY(), true);
		}
		const int to = GetPlayerBotOchaoSpotOf(x, y, false);
		return DISTANCE_APPROX(ch->GetX() - PLAYERBOT_OCHAO_HUBS[s_iFrom].x, ch->GetY() - PLAYERBOT_OCHAO_HUBS[s_iFrom].y) +
				PLAYERBOT_OCHAO_HUB_WALK[s_iFrom][to] +
				DISTANCE_APPROX(x - PLAYERBOT_OCHAO_HUBS[to].x, y - PLAYERBOT_OCHAO_HUBS[to].y);
	}

	// On its way out (the frontier's exit, or another pass's warp walked out).
	bool IsPlayerBotOchaoLeaving(LPCHARACTER ch)
	{
		return ch && (s_mapPlayerBotOchaoExit.count(ch->GetPlayerID()) != 0 ||
				s_mapPlayerBotOchaoPending.count(ch->GetPlayerID()) != 0 ||
				IsPlayerBotOchaoLeaveOrdered(ch));
	}

	const TPlayerBotHuntingHub* GetPlayerBotOchaoHubs(size_t& count)
	{
		count = PLAYERBOT_OCHAO_HUB_COUNT;
		return PLAYERBOT_OCHAO_HUBS;
	}

	// ------------------------------------------------------------ the way in

	// Called at the top of TransitionPlayerBotMap. -1: not ours, go on;
	// 0/1: the answer.
	int RoutePlayerBotOchaoTransition(LPCHARACTER ch, TPlayerBotAIState& state,
			long targetMap, long targetX, long targetY, DWORD dwNow, const char* reason)
	{
		if (!ch)
			return -1;
		const DWORD pid = ch->GetPlayerID();
		if (ch->GetMapIndex() == PLAYERBOT_MAP_OCHAO && targetMap != PLAYERBOT_MAP_OCHAO)
		{
			// Out only from the Teleporter or the Portal, or with the ring.
			const bool atTeleporter = DISTANCE_APPROX(ch->GetX() - PLAYERBOT_OCHAO_EXIT_X,
					ch->GetY() - PLAYERBOT_OCHAO_EXIT_Y) <= PLAYERBOT_OCHAO_WARP_REACH;
			LPCHARACTER openPortal = mt2009_ochao::s_dwPortalVID ? mt2009_ochao::FindOnMap(mt2009_ochao::s_dwPortalVID) : NULL;
			// MT2009_PLUS_AREZZO_BOTS_V1 (las): the Portal to the Las is talked to
			// from a few steps, as a player does - the NPC stands on its own cell.
			const int portalReach = reason && strcmp(reason, "arezzo_las_portal") == 0
					? 800 : PLAYERBOT_OCHAO_WARP_REACH;
			const bool atPortal = openPortal && DISTANCE_APPROX(ch->GetX() - openPortal->GetX(),
					ch->GetY() - openPortal->GetY()) <= portalReach;
			std::map<DWORD, TPlayerBotOchaoPending>::iterator pend = s_mapPlayerBotOchaoPending.find(pid);
			// Or once it has died too often in this visit to walk anywhere: the
			// warp is its way out of a pack it cannot get past.
			std::map<DWORD, TPlayerBotOchaoTrack>::const_iterator track = s_mapPlayerBotOchaoTrack.find(pid);
			const bool gaveUp = pend != s_mapPlayerBotOchaoPending.end() &&
					(dwNow - pend->second.dwSince >= PLAYERBOT_OCHAO_PENDING_MAX_MS ||
					 (track != s_mapPlayerBotOchaoTrack.end() && track->second.dwVisitDeaths >= PLAYERBOT_OCHAO_VISIT_DEATHS_LEAVE));
			const bool ring = reason && strstr(reason, "ring") != NULL;
			if (!atTeleporter && !atPortal && !gaveUp && !ring)
			{
				if (pend == s_mapPlayerBotOchaoPending.end())
				{
					TPlayerBotOchaoPending& p = s_mapPlayerBotOchaoPending[pid];
					p.lMap = targetMap; p.lX = targetX; p.lY = targetY; p.dwSince = dwNow;
					strlcpy(p.szReason, reason ? reason : "?", sizeof(p.szReason));
					sys_log(0, "OCHAO_BOT: walks out first pid=%u name=%s to=%ld reason=%s pos=(%ld,%ld)",
							pid, ch->GetName(), targetMap, reason ? reason : "?", ch->GetX(), ch->GetY());
				}
				return 1;
			}
			if (gaveUp)
				sys_log(0, "OCHAO_BOT: walk out gave up pid=%u name=%s pos=(%ld,%ld)", pid, ch->GetName(), ch->GetX(), ch->GetY());
			s_mapPlayerBotOchaoPending.erase(pid);
			std::map<DWORD, TPlayerBotOchaoTrack>::iterator t = s_mapPlayerBotOchaoTrack.find(pid);
			std::map<DWORD, TPlayerBotOchaoExit>::iterator e = s_mapPlayerBotOchaoExit.find(pid);
			sys_log(0, "OCHAO_BOT: left pid=%u name=%s to=%ld reason=%s via=%s pos=(%ld,%ld) stay_s=%u exit_walk_s=%u stalls=%u kills=%u exp=%llu deaths=%u",
					pid, ch->GetName(), targetMap, reason ? reason : "?",
					e != s_mapPlayerBotOchaoExit.end() ? (e->second.bPortal ? "portal" : (e->second.bNavOnly ? "teleporter_nav" : "teleporter_tree")) : "other",
					ch->GetX(), ch->GetY(),
					t != s_mapPlayerBotOchaoTrack.end() && t->second.dwEntered ? (dwNow - t->second.dwEntered) / 1000 : 0,
					e != s_mapPlayerBotOchaoExit.end() ? (dwNow - e->second.dwStarted) / 1000 : 0,
					e != s_mapPlayerBotOchaoExit.end() ? (unsigned int)e->second.bStalls : 0U,
					t != s_mapPlayerBotOchaoTrack.end() ? t->second.dwKills : 0,
					t != s_mapPlayerBotOchaoTrack.end() ? t->second.ullExp : 0ULL,
					t != s_mapPlayerBotOchaoTrack.end() ? t->second.dwDeaths : 0);
			s_mapPlayerBotOchaoExit.erase(pid);
			s_setPlayerBotOchaoLeave.erase(pid);
			if (t != s_mapPlayerBotOchaoTrack.end())
				t->second.dwEntered = 0;
			return -1;
		}
		if (targetMap != PLAYERBOT_MAP_OCHAO || ch->GetMapIndex() == PLAYERBOT_MAP_OCHAO)
			return -1;
		// The quest's gate, for bots as for players.
		if (ch->GetLevel() < PLAYERBOT_OCHAO_MIN_LEVEL)
		{
			PlayerBotLogThrottled("ochao_level_gate", dwNow,
					"OCHAO_BOT: refused under level %u pid=%u name=%s level=%u reason=%s",
					(unsigned int)PLAYERBOT_OCHAO_MIN_LEVEL, pid, ch->GetName(),
					(unsigned int)ch->GetLevel(), reason ? reason : "?");
			s_mapPlayerBotOchaoCrossing.erase(pid);
			return 0;
		}
		// The Guardian's own warp: through.
		if (reason && strcmp(reason, "ochao_temple_guardian") == 0)
		{
			std::map<DWORD, TPlayerBotOchaoCrossing>::iterator c = s_mapPlayerBotOchaoCrossing.find(pid);
			TPlayerBotOchaoTrack& t = s_mapPlayerBotOchaoTrack[pid];
			t.dwEntered = dwNow;
			t.dwVisitDeaths = 0;
			t.adwDeathAt[0] = t.adwDeathAt[1] = 0;
			t.dwCrossStart = c != s_mapPlayerBotOchaoCrossing.end() ? c->second.dwSince : 0;
			sys_log(0, "OCHAO_BOT: entered pid=%u name=%s level=%u job=%u crossing_s=%u forced=%d",
					pid, ch->GetName(), (unsigned int)ch->GetLevel(), (unsigned int)ch->GetJob(),
					c != s_mapPlayerBotOchaoCrossing.end() ? (dwNow - c->second.dwSince) / 1000 : 0,
					IsPlayerBotOchaoForced(ch) ? 1 : 0);
			s_mapPlayerBotOchaoCrossing.erase(pid);
			return -1;
		}
		if (s_mapPlayerBotOchaoCrossing.find(pid) == s_mapPlayerBotOchaoCrossing.end())
		{
			s_mapPlayerBotOchaoCrossing[pid].dwSince = dwNow;
			sys_log(0, "OCHAO_BOT: crossing begins pid=%u name=%s level=%u from=%ld reason=%s",
					pid, ch->GetName(), (unsigned int)ch->GetLevel(), ch->GetMapIndex(), reason ? reason : "?");
		}
		// Already in the valley: the walk to the Guardian is the travel pass's.
		if (ch->GetMapIndex() == PLAYERBOT_MAP_ORC_VALLEY)
			return 1;
		long valleyX = 0, valleyY = 0;
		GetPlayerBotFrontierArrivalFor(ch, PLAYERBOT_MAP_ORC_VALLEY, valleyX, valleyY);
		return TransitionPlayerBotMap(ch, state, PLAYERBOT_MAP_ORC_VALLEY, valleyX, valleyY, dwNow,
				"ochao_via_orc_valley") ? 1 : 0;
	}

	// In Orc Valley, from the travel pass: the walk to Straznik Swiatyni.
	// -1: nothing to do here.
	int ManagePlayerBotOchaoCrossing(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->GetMapIndex() != PLAYERBOT_MAP_ORC_VALLEY)
			return -1;
		const DWORD pid = ch->GetPlayerID();
		std::map<DWORD, TPlayerBotOchaoCrossing>::iterator c = s_mapPlayerBotOchaoCrossing.find(pid);
		// MT2009_PLUS_AREZZO_BOTS_V1 (las): a bot sent to the Las crosses the
		// temple on its way (playerbot_arezzo_bots.h, later in this unit).
		const bool wants = GetPlayerBotFrontierMapForLevel(ch) == PLAYERBOT_MAP_OCHAO ||
				IsPlayerBotArezzoBoundForLas(ch);
		if (c == s_mapPlayerBotOchaoCrossing.end())
		{
			// Landed in the valley by its own road (the Portal, a raid) with the
			// temple as its ground: it goes on to him.
			if (!wants || BlocksPlayerBotTravel(ch))
				return -1;
			s_mapPlayerBotOchaoCrossing[pid].dwSince = dwNow;
			sys_log(0, "OCHAO_BOT: crossing begins pid=%u name=%s level=%u from=%ld reason=in_valley",
					pid, ch->GetName(), (unsigned int)ch->GetLevel(), ch->GetMapIndex());
		}
		else if (!wants || BlocksPlayerBotTravel(ch))
		{
			sys_log(0, "OCHAO_BOT: crossing dropped pid=%u name=%s wants=%d blocked=%d after_s=%u",
					pid, ch->GetName(), wants ? 1 : 0, BlocksPlayerBotTravel(ch) ? 1 : 0,
					(dwNow - c->second.dwSince) / 1000);
			s_mapPlayerBotOchaoCrossing.erase(c);
			return -1;
		}
		return MovePlayerBotToWorldPortal(ch, state, PLAYERBOT_OCHAO_GUARDIAN_X, PLAYERBOT_OCHAO_GUARDIAN_Y,
				PLAYERBOT_MAP_OCHAO, PLAYERBOT_OCHAO_ARRIVAL_X, PLAYERBOT_OCHAO_ARRIVAL_Y, dwNow,
				"ochao_temple_guardian") ? 1 : 0;
	}

	// ------------------------------------------------------------ the way out

	int ChoosePlayerBotOchaoNode(LPCHARACTER ch)
	{
		CPlayerBotNavigation& nav = CPlayerBotNavigation::instance(PLAYERBOT_MAP_OCHAO);
		if (!nav.Init(PLAYERBOT_MAP_OCHAO))
			return -1;
		std::vector<std::pair<int, int> > order;
		order.reserve(PLAYERBOT_OCHAO_NODE_COUNT);
		for (int i = 0; i < PLAYERBOT_OCHAO_NODE_COUNT; ++i)
			order.push_back(std::make_pair(DISTANCE_APPROX(ch->GetX() - PLAYERBOT_OCHAO_EXIT_TREE[i].x,
					ch->GetY() - PLAYERBOT_OCHAO_EXIT_TREE[i].y), i));
		std::sort(order.begin(), order.end());
		for (size_t k = 0; k < order.size() && order[k].first <= PLAYERBOT_OCHAO_NODE_SIGHT; ++k)
		{
			const int i = order[k].second;
			if (nav.SegmentClearWorld(ch->GetX(), ch->GetY(), PLAYERBOT_OCHAO_EXIT_TREE[i].x, PLAYERBOT_OCHAO_EXIT_TREE[i].y))
			{
				// A corner in sight whose next corner is in sight too: go
				// straight for the next one.
				const int n = PLAYERBOT_OCHAO_EXIT_TREE[i].next;
				if (n >= 0 && nav.SegmentClearWorld(ch->GetX(), ch->GetY(),
						PLAYERBOT_OCHAO_EXIT_TREE[n].x, PLAYERBOT_OCHAO_EXIT_TREE[n].y))
					return n;
				return i;
			}
		}
		return order.empty() ? -1 : order[0].second;
	}

	LPCHARACTER FindPlayerBotOchaoPortal()
	{
		if (!mt2009_ochao::s_dwPortalVID)
			return NULL;
		return mt2009_ochao::FindOnMap(mt2009_ochao::s_dwPortalVID);
	}

	// From the frontier branch of the travel pass, once it has decided the bot
	// leaves the temple for destMap.
	bool MovePlayerBotOutOfOchao(LPCHARACTER ch, TPlayerBotAIState& state,
			long destMap, long destX, long destY, DWORD dwNow, const char* reason)
	{
		const DWORD pid = ch->GetPlayerID();
		std::map<DWORD, TPlayerBotOchaoExit>::iterator it = s_mapPlayerBotOchaoExit.find(pid);
		if (it == s_mapPlayerBotOchaoExit.end())
		{
			it = s_mapPlayerBotOchaoExit.insert(std::make_pair(pid, TPlayerBotOchaoExit())).first;
			it->second.dwStarted = dwNow;
			strlcpy(it->second.szReason, reason ? reason : "?", sizeof(it->second.szReason));
			sys_log(0, "OCHAO_BOT: leaving pid=%u name=%s reason=%s pos=(%ld,%ld) to=%ld ordered=%d",
					pid, ch->GetName(), reason ? reason : "?", ch->GetX(), ch->GetY(), destMap,
					IsPlayerBotOchaoLeaveOrdered(ch) ? 1 : 0);
		}
		TPlayerBotOchaoExit& exit = it->second;
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		// The Guardian's Portal, while it stands and the bot is near it.
		LPCHARACTER portal = FindPlayerBotOchaoPortal();
		if (portal)
		{
			const int d = DISTANCE_APPROX(ch->GetX() - portal->GetX(), ch->GetY() - portal->GetY());
			CPlayerBotNavigation& nav = CPlayerBotNavigation::instance(PLAYERBOT_MAP_OCHAO);
			if (exit.bPortal || (d <= PLAYERBOT_OCHAO_PORTAL_REACH && nav.Init(PLAYERBOT_MAP_OCHAO) &&
					nav.CanReach(ch->GetX(), ch->GetY(), portal->GetX(), portal->GetY())))
			{
				if (!exit.bPortal)
					sys_log(0, "OCHAO_BOT: takes the portal pid=%u name=%s distance=%d", pid, ch->GetName(), d);
				exit.bPortal = true;
				return MovePlayerBotToWorldPortal(ch, state, portal->GetX(), portal->GetY(),
						PLAYERBOT_MAP_ORC_VALLEY, PLAYERBOT_OCHAO_PORTAL_TO_X, PLAYERBOT_OCHAO_PORTAL_TO_Y,
						dwNow, "ochao_portal");
			}
		}
		else if (exit.bPortal)
		{
			// Closed before the bot got there: the Teleporter after all.
			exit.bPortal = false;
			exit.iNode = -1;
			sys_log(0, "OCHAO_BOT: portal closed on the way pid=%u name=%s", pid, ch->GetName());
		}

		// The Teleporter: straight to him from the last corner, or on the
		// navigation alone once the corners have failed.
		if (exit.bNavOnly || exit.iNode == 0)
			return MovePlayerBotToWorldPortal(ch, state, PLAYERBOT_OCHAO_EXIT_X, PLAYERBOT_OCHAO_EXIT_Y,
					destMap, destX, destY, dwNow, reason);

		if (exit.iNode < 0)
		{
			exit.iNode = ChoosePlayerBotOchaoNode(ch);
			exit.dwLegSince = 0;
			if (exit.iNode < 0)
			{
				exit.bNavOnly = true;
				return MovePlayerBotToWorldPortal(ch, state, PLAYERBOT_OCHAO_EXIT_X, PLAYERBOT_OCHAO_EXIT_Y,
						destMap, destX, destY, dwNow, reason);
			}
			ClearPlayerBotRoute(state, true);
		}
		long nodeX = PLAYERBOT_OCHAO_EXIT_TREE[exit.iNode].x;
		long nodeY = PLAYERBOT_OCHAO_EXIT_TREE[exit.iNode].y;
		int distance = DISTANCE_APPROX(ch->GetX() - nodeX, ch->GetY() - nodeY);
		if (distance <= PLAYERBOT_OCHAO_NODE_REACHED)
		{
			exit.iNode = PLAYERBOT_OCHAO_EXIT_TREE[exit.iNode].next;
			exit.dwLegSince = 0;
			ClearPlayerBotRoute(state, true);
			if (exit.iNode <= 0)
			{
				exit.iNode = 0;
				return MovePlayerBotToWorldPortal(ch, state, PLAYERBOT_OCHAO_EXIT_X, PLAYERBOT_OCHAO_EXIT_Y,
						destMap, destX, destY, dwNow, reason);
			}
			nodeX = PLAYERBOT_OCHAO_EXIT_TREE[exit.iNode].x;
			nodeY = PLAYERBOT_OCHAO_EXIT_TREE[exit.iNode].y;
			distance = DISTANCE_APPROX(ch->GetX() - nodeX, ch->GetY() - nodeY);
		}
		// Progress is ground covered, not only the straight line shortening: a
		// corner out of sight is reached round the walls.
		if (exit.dwLegSince == 0 || distance + PLAYERBOT_OCHAO_LEG_PROGRESS <= exit.iLegBest ||
				DISTANCE_APPROX(ch->GetX() - exit.lLegX, ch->GetY() - exit.lLegY) >= PLAYERBOT_OCHAO_LEG_PROGRESS * 2)
		{
			exit.dwLegSince = dwNow;
			exit.iLegBest = distance;
			exit.lLegX = ch->GetX();
			exit.lLegY = ch->GetY();
		}
		else if (dwNow - exit.dwLegSince >= PLAYERBOT_OCHAO_LEG_TIMEOUT)
		{
			++exit.bStalls;
			sys_log(0, "OCHAO_BOT: exit leg stalled pid=%u name=%s node=%d pos=(%ld,%ld) node_pos=(%ld,%ld) distance=%d stalls=%u",
					pid, ch->GetName(), exit.iNode, ch->GetX(), ch->GetY(), nodeX, nodeY, distance,
					(unsigned int)exit.bStalls);
			exit.dwLegSince = 0;
			exit.iNode = -1;
			ClearPlayerBotRoute(state, true);
			if (exit.bStalls >= PLAYERBOT_OCHAO_MAX_STALLS)
				exit.bNavOnly = true;
			return true;
		}
		MovePlayerBot(ch, nodeX, nodeY, dwNow, 6, true, true, false, true);
		return true;
	}

	// From the top of the bot's tick, before every errand: a warp another
	// pass asked for while the bot stood in the labyrinth is walked out first.
	bool ManagePlayerBotOchaoPendingExit(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (s_mapPlayerBotOchaoPending.empty() || !ch)
			return false;
		std::map<DWORD, TPlayerBotOchaoPending>::iterator it = s_mapPlayerBotOchaoPending.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotOchaoPending.end())
			return false;
		if (ch->GetMapIndex() != PLAYERBOT_MAP_OCHAO)
		{
			s_mapPlayerBotOchaoPending.erase(it);
			return false;
		}
		// Dead, low, or in a fight: the fight and the recovery come first.
		if (ch->IsDead() || state.bRecoveringAfterDeath || state.bTacticalRetreat ||
				(ch->GetMaxHP() > 0 && ch->GetHP() * 100 < ch->GetMaxHP() * 40))
			return false;
		LPCHARACTER victim = ch->GetVictim();
		if (victim && !victim->IsDead() && victim->GetVictim() == ch &&
				DISTANCE_APPROX(ch->GetX() - victim->GetX(), ch->GetY() - victim->GetY()) < 600)
			return false;
		const TPlayerBotOchaoPending p = it->second;
		return MovePlayerBotOutOfOchao(ch, state, p.lMap, p.lX, p.lY, dwNow, p.szReason);
	}

	// ------------------------------------------------------------ a walk inside
	//
	// A walk from one point of the labyrinth to another (the boss raid's way to
	// its boss, playerbot_boss_raid.h) goes by the same corners as the way out:
	// up the tree from the corner the bot can see to the first corner the
	// goal's own branch shares, and down that branch to the corner the goal can
	// be seen from. Each leg is a straight line with open cells either side,
	// so no leg is planned into a wall and the far-plan budget is not asked.
	// A corner further along the way that is already in sight is walked to
	// straight; the goal itself, once in sight, too.
	struct TPlayerBotOchaoWalk
	{
		long lGoalX, lGoalY;
		std::vector<short> path;
		size_t idx;
		DWORD dwLastUsed;
		DWORD dwLegSince;
		int iLegBest;
		long lLegX, lLegY;
		BYTE bStalls;
		bool bNavOnly;
		TPlayerBotOchaoWalk() : lGoalX(0), lGoalY(0), idx(0), dwLastUsed(0), dwLegSince(0), iLegBest(0),
				lLegX(0), lLegY(0), bStalls(0), bNavOnly(false) {}
	};
	std::map<DWORD, TPlayerBotOchaoWalk> s_mapPlayerBotOchaoWalk;
	// The goal moved this far (a boss who walks): the way is planned again.
	const int PLAYERBOT_OCHAO_WALK_REPLAN = 1500;
	// Counted for the watch: walks planned by the corners, walks given up to
	// the navigation after PLAYERBOT_OCHAO_MAX_STALLS stalls.
	unsigned int s_uPlayerBotOchaoWalksPlanned = 0;
	unsigned int s_uPlayerBotOchaoWalksNavOnly = 0;

	// The nearest corner in sight of a point (within PLAYERBOT_OCHAO_NODE_SIGHT),
	// or the nearest corner.
	int GetPlayerBotOchaoNodeInSight(CPlayerBotNavigation& nav, long x, long y)
	{
		std::vector<std::pair<int, int> > order;
		order.reserve(PLAYERBOT_OCHAO_NODE_COUNT);
		for (int i = 0; i < PLAYERBOT_OCHAO_NODE_COUNT; ++i)
			order.push_back(std::make_pair(DISTANCE_APPROX(x - PLAYERBOT_OCHAO_EXIT_TREE[i].x,
					y - PLAYERBOT_OCHAO_EXIT_TREE[i].y), i));
		std::sort(order.begin(), order.end());
		for (size_t k = 0; k < order.size() && order[k].first <= PLAYERBOT_OCHAO_NODE_SIGHT; ++k)
			if (nav.SegmentClearWorld(x, y, PLAYERBOT_OCHAO_EXIT_TREE[order[k].second].x,
					PLAYERBOT_OCHAO_EXIT_TREE[order[k].second].y))
				return order[k].second;
		return order.empty() ? -1 : order[0].second;
	}

	// The corners from (fromX, fromY) to (toX, toY) through the tree.
	bool PlanPlayerBotOchaoWalk(CPlayerBotNavigation& nav, long fromX, long fromY, long toX, long toY,
			std::vector<short>& path)
	{
		path.clear();
		const int a = GetPlayerBotOchaoNodeInSight(nav, fromX, fromY);
		const int b = GetPlayerBotOchaoNodeInSight(nav, toX, toY);
		if (a < 0 || b < 0)
			return false;
		std::vector<short> up, down;
		std::vector<char> onUp(PLAYERBOT_OCHAO_NODE_COUNT, 0);
		for (int n = a, guard = 0; n >= 0 && guard <= PLAYERBOT_OCHAO_NODE_COUNT; n = PLAYERBOT_OCHAO_EXIT_TREE[n].next, ++guard)
		{
			up.push_back((short)n);
			onUp[n] = 1;
		}
		int meet = -1;
		for (int n = b, guard = 0; n >= 0 && guard <= PLAYERBOT_OCHAO_NODE_COUNT; n = PLAYERBOT_OCHAO_EXIT_TREE[n].next, ++guard)
		{
			if (onUp[n])
			{
				meet = n;
				break;
			}
			down.push_back((short)n);
		}
		if (meet < 0)
			return false;
		for (size_t i = 0; i < up.size(); ++i)
		{
			path.push_back(up[i]);
			if (up[i] == meet)
				break;
		}
		for (size_t i = down.size(); i-- > 0;)
			path.push_back(down[i]);
		return !path.empty();
	}

	// Where a raid on a boss standing at (bossX, bossY) meets: up the tree from
	// the corner he can be seen from, to the first corner this far along it.
	bool GetPlayerBotOchaoMuster(long bossX, long bossY, long& outX, long& outY)
	{
		CPlayerBotNavigation& nav = CPlayerBotNavigation::instance(PLAYERBOT_MAP_OCHAO);
		if (!nav.Init(PLAYERBOT_MAP_OCHAO))
			return false;
		int n = GetPlayerBotOchaoNodeInSight(nav, bossX, bossY);
		if (n < 0)
			return false;
		long px = bossX, py = bossY;
		int walked = 0;
		for (int guard = 0; n >= 0 && guard <= PLAYERBOT_OCHAO_NODE_COUNT; ++guard)
		{
			const TPlayerBotOchaoNode& node = PLAYERBOT_OCHAO_EXIT_TREE[n];
			walked += DISTANCE_APPROX(node.x - px, node.y - py);
			px = node.x;
			py = node.y;
			if (walked >= PLAYERBOT_OCHAO_RAID_MUSTER_WALK &&
					DISTANCE_APPROX(node.x - bossX, node.y - bossY) >= PLAYERBOT_BOSS_RAID_RALLY_MIN)
			{
				outX = node.x;
				outY = node.y;
				return true;
			}
			n = node.next;
		}
		return false;
	}

	void ForgetPlayerBotOchaoWalk(DWORD pid)
	{
		s_mapPlayerBotOchaoWalk.erase(pid);
	}

	// One step of the walk to (x, y); false when the bot is not in the temple.
	bool WalkPlayerBotInOchao(LPCHARACTER ch, TPlayerBotAIState& state, long x, long y, DWORD dwNow)
	{
		if (!ch || ch->GetMapIndex() != PLAYERBOT_MAP_OCHAO)
			return false;
		CPlayerBotNavigation& nav = CPlayerBotNavigation::instance(PLAYERBOT_MAP_OCHAO);
		if (!nav.Init(PLAYERBOT_MAP_OCHAO))
			return false;
		const DWORD pid = ch->GetPlayerID();
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		const int toGoal = DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y);
		TPlayerBotOchaoWalk& w = s_mapPlayerBotOchaoWalk[pid];
		// Given up to the navigation for another goal, or long ago: the corners
		// again.
		if (w.bNavOnly && (dwNow - w.dwLastUsed > 60000 ||
				DISTANCE_APPROX(x - w.lGoalX, y - w.lGoalY) > PLAYERBOT_OCHAO_WALK_REPLAN))
			w = TPlayerBotOchaoWalk();
		// The goal in sight: straight to it.
		if (w.bNavOnly || (toGoal <= PLAYERBOT_OCHAO_NODE_SIGHT && nav.SegmentClearWorld(ch->GetX(), ch->GetY(), x, y)))
		{
			w.dwLastUsed = dwNow;
			w.path.clear();
			MovePlayerBot(ch, x, y, dwNow, 6, true, toGoal > 4000, false, true);
			return true;
		}
		if (w.path.empty() || w.idx >= w.path.size() || dwNow - w.dwLastUsed > 60000 ||
				DISTANCE_APPROX(x - w.lGoalX, y - w.lGoalY) > PLAYERBOT_OCHAO_WALK_REPLAN)
		{
			const BYTE stalls = (dwNow - w.dwLastUsed > 60000) ? 0 : w.bStalls;
			w = TPlayerBotOchaoWalk();
			w.bStalls = stalls;
			w.lGoalX = x;
			w.lGoalY = y;
			if (!PlanPlayerBotOchaoWalk(nav, ch->GetX(), ch->GetY(), x, y, w.path))
			{
				w.bNavOnly = true;
				++s_uPlayerBotOchaoWalksNavOnly;
				w.dwLastUsed = dwNow;
				MovePlayerBot(ch, x, y, dwNow, 6, true, true, false, true);
				return true;
			}
			++s_uPlayerBotOchaoWalksPlanned;
			ClearPlayerBotRoute(state, true);
		}
		w.dwLastUsed = dwNow;
		// Corners passed, or already in sight further along.
		while (w.idx < w.path.size())
		{
			const TPlayerBotOchaoNode& n = PLAYERBOT_OCHAO_EXIT_TREE[w.path[w.idx]];
			if (DISTANCE_APPROX(ch->GetX() - n.x, ch->GetY() - n.y) <= PLAYERBOT_OCHAO_NODE_REACHED)
			{
				++w.idx;
				w.dwLegSince = 0;
				continue;
			}
			if (w.idx + 1 < w.path.size())
			{
				const TPlayerBotOchaoNode& m = PLAYERBOT_OCHAO_EXIT_TREE[w.path[w.idx + 1]];
				if (DISTANCE_APPROX(ch->GetX() - m.x, ch->GetY() - m.y) <= PLAYERBOT_OCHAO_NODE_SIGHT &&
						nav.SegmentClearWorld(ch->GetX(), ch->GetY(), m.x, m.y))
				{
					++w.idx;
					w.dwLegSince = 0;
					continue;
				}
			}
			break;
		}
		if (w.idx >= w.path.size())
		{
			// Past the last corner and the goal still out of sight: straight on
			// the navigation from here.
			MovePlayerBot(ch, x, y, dwNow, 6, true, toGoal > 4000, false, true);
			return true;
		}
		const long nodeX = PLAYERBOT_OCHAO_EXIT_TREE[w.path[w.idx]].x;
		const long nodeY = PLAYERBOT_OCHAO_EXIT_TREE[w.path[w.idx]].y;
		const int distance = DISTANCE_APPROX(ch->GetX() - nodeX, ch->GetY() - nodeY);
		if (w.dwLegSince == 0 || distance + PLAYERBOT_OCHAO_LEG_PROGRESS <= w.iLegBest ||
				DISTANCE_APPROX(ch->GetX() - w.lLegX, ch->GetY() - w.lLegY) >= PLAYERBOT_OCHAO_LEG_PROGRESS * 2)
		{
			w.dwLegSince = dwNow;
			w.iLegBest = distance;
			w.lLegX = ch->GetX();
			w.lLegY = ch->GetY();
		}
		else if (dwNow - w.dwLegSince >= PLAYERBOT_OCHAO_LEG_TIMEOUT)
		{
			++w.bStalls;
			sys_log(0, "OCHAO_BOT: walk leg stalled pid=%u name=%s node=%d pos=(%ld,%ld) node_pos=(%ld,%ld) goal=(%ld,%ld) stalls=%u",
					pid, ch->GetName(), (int)w.path[w.idx], ch->GetX(), ch->GetY(), nodeX, nodeY, x, y,
					(unsigned int)w.bStalls);
			ClearPlayerBotRoute(state, true);
			if (w.bStalls >= PLAYERBOT_OCHAO_MAX_STALLS)
			{
				w.bNavOnly = true;
				++s_uPlayerBotOchaoWalksNavOnly;
			}
			else
				w.path.clear();	// planned again from here on the next step
			return true;
		}
		MovePlayerBot(ch, nodeX, nodeY, dwNow, 6, true, true, false, true);
		return true;
	}

	// ------------------------------------------------------------ the kills

	// Who struck the last blow on a boss of the temple, by his VID (the boss
	// raid asks it, playerbot_boss_raid.h).
	std::map<DWORD, DWORD> s_mapPlayerBotOchaoBossKiller;
	DWORD GetPlayerBotOchaoBossKiller(DWORD vid)
	{
		std::map<DWORD, DWORD>::const_iterator it = s_mapPlayerBotOchaoBossKiller.find(vid);
		return it == s_mapPlayerBotOchaoBossKiller.end() ? 0 : it->second;
	}

	void NoteOchaoBotKill(LPCHARACTER killer, LPCHARACTER victim)
	{
		if (!killer || !victim || victim->IsPC() || killer->GetMapIndex() != PLAYERBOT_MAP_OCHAO ||
				!IsPlayerBotOchaoBot(killer))
			return;
		TPlayerBotOchaoTrack& t = s_mapPlayerBotOchaoTrack[killer->GetPlayerID()];
		++t.dwKills;
		const DWORD race = victim->GetRaceNum();
		if (race == mt2009_ochao::GUARDIAN_VNUM || race == 6311 || race == 6390)
		{
			++t.dwBossKills;
			if (s_mapPlayerBotOchaoBossKiller.size() > 64)
				s_mapPlayerBotOchaoBossKiller.clear();
			s_mapPlayerBotOchaoBossKiller[(DWORD)victim->GetVID()] = killer->GetPlayerID();
			const DWORD dwNow = get_dword_time();
			sys_log(0, "OCHAO_BOT: boss killed race=%u name=%s killer=%u pos=(%ld,%ld) fighters=%u since_spawn_s=%u since_engaged_s=%u",
					race, victim->GetName(), killer->GetPlayerID(), victim->GetX(), victim->GetY(),
					race == mt2009_ochao::GUARDIAN_VNUM ? (unsigned int)s_setPlayerBotOchaoGuardianFighters.size() : 0U,
					race == mt2009_ochao::GUARDIAN_VNUM && s_dwPlayerBotOchaoGuardianSeen ? (dwNow - s_dwPlayerBotOchaoGuardianSeen) / 1000 : 0,
					race == mt2009_ochao::GUARDIAN_VNUM && s_dwPlayerBotOchaoGuardianEngaged ? (dwNow - s_dwPlayerBotOchaoGuardianEngaged) / 1000 : 0);
		}
	}

	// ------------------------------------------------------------ the test hook and the watch

	std::vector<DWORD> CollectPlayerBotOchaoCandidates(bool onTemple)
	{
		std::vector<DWORD> out;
		for (TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.begin(); it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch || !ch->IsPC() || ch->IsDead())
				continue;
			if (onTemple != (ch->GetMapIndex() == PLAYERBOT_MAP_OCHAO))
				continue;
			if (!onTemple && (ch->GetLevel() < PLAYERBOT_OCHAO_MIN_LEVEL ||
					(ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()))))
				continue;
			// MT2009_PLUS_AREZZO_BOTS_V1: the Arezzo test's bots are its own.
			if (IsPlayerBotArezzoBound(ch))
				continue;
			out.push_back(it->first);
		}
		return out;
	}

	void RunPlayerBotOchaoTestFile(DWORD dwNow)
	{
		FILE* fp = fopen(PLAYERBOT_OCHAO_TEST_FILE, "r");
		if (!fp)
			return;
		char line[128];
		std::vector<std::string> lines;
		while (fgets(line, sizeof(line), fp))
			lines.push_back(line);
		fclose(fp);
		char done[128];
		snprintf(done, sizeof(done), "%s.done", PLAYERBOT_OCHAO_TEST_FILE);
		rename(PLAYERBOT_OCHAO_TEST_FILE, done);
		for (size_t l = 0; l < lines.size(); ++l)
		{
			char cmd[32] = "";
			int n = 0;
			if (sscanf(lines[l].c_str(), "%31s %d", cmd, &n) < 1)
				continue;
			if (!strcmp(cmd, "send"))
			{
				std::vector<DWORD> pids = CollectPlayerBotOchaoCandidates(false);
				int sent = 0, failed = 0;
				for (size_t i = 0; i < pids.size() && sent < n; ++i)
				{
					LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pids[i]);
					TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(pids[i]);
					if (!ch || st == s_mapPlayerBotAIStates.end())
						continue;
					s_setPlayerBotOchaoForced.insert(pids[i]);
					s_mapPlayerBotOchaoTrack[pids[i]].dwCrossStart = dwNow;
					if (TransitionPlayerBotMap(ch, st->second, PLAYERBOT_MAP_OCHAO,
							PLAYERBOT_OCHAO_ARRIVAL_X, PLAYERBOT_OCHAO_ARRIVAL_Y, dwNow, "ochao_test_send"))
						++sent;
					else
						++failed;
				}
				sys_log(0, "OCHAO_BOT: test send asked=%d sent=%d failed=%d candidates=%u",
						n, sent, failed, (unsigned int)pids.size());
			}
			else if (!strcmp(cmd, "leave") || !strcmp(cmd, "leaveall"))
			{
				std::vector<DWORD> pids = CollectPlayerBotOchaoCandidates(true);
				int ordered = 0;
				for (size_t i = 0; i < pids.size() && (!strcmp(cmd, "leaveall") || ordered < n); ++i)
				{
					if (s_setPlayerBotOchaoLeave.count(pids[i]))
						continue;
					s_setPlayerBotOchaoLeave.insert(pids[i]);
					s_setPlayerBotOchaoForced.erase(pids[i]);
					++ordered;
				}
				sys_log(0, "OCHAO_BOT: test leave ordered=%d on_map=%u", ordered, (unsigned int)pids.size());
			}
			else if (!strcmp(cmd, "spawn"))
			{
				// A boss at his boss.txt point, for a test of the raid.
				LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(PLAYERBOT_MAP_OCHAO);
				long cx = 0, cy = 0;
				if (n == 6311) { cx = 550; cy = 138; }
				else if (n == 6390) { cx = 433; cy = 82; }
				LPCHARACTER boss = (map && cx) ? CHARACTER_MANAGER::instance().SpawnMob(n, PLAYERBOT_MAP_OCHAO,
						map->m_setting.iBaseX + cx * 100, map->m_setting.iBaseY + cy * 100, 0, true, -1, true) : NULL;
				sys_log(0, "OCHAO_BOT: test spawn race=%d vid=%u", n, boss ? (unsigned int)boss->GetVID() : 0U);
			}
			else if (!strcmp(cmd, "reset"))
			{
				s_setPlayerBotOchaoForced.clear();
				s_setPlayerBotOchaoLeave.clear();
				sys_log(0, "OCHAO_BOT: test reset");
			}
		}
	}

	// Every second from the temple's clock (playerbot_ochao.h).
	void TickPlayerBotOchao()
	{
		const DWORD dwNow = get_dword_time();
		static DWORD s_dwNextFile = 0, s_dwNextTrack = 0;
		if (dwNow >= s_dwNextFile)
		{
			s_dwNextFile = dwNow + 5000;
			RunPlayerBotOchaoTestFile(dwNow);
		}

		// The Guardian: when he appears, when a bot first fights him.
		LPCHARACTER guardian = mt2009_ochao::FindOnMap(mt2009_ochao::s_dwGuardianVID);
		if (guardian && guardian->GetVID() != s_dwPlayerBotOchaoGuardianVID)
		{
			s_dwPlayerBotOchaoGuardianVID = guardian->GetVID();
			s_dwPlayerBotOchaoGuardianSeen = dwNow;
			s_dwPlayerBotOchaoGuardianEngaged = 0;
			s_setPlayerBotOchaoGuardianFighters.clear();
		}

		const bool writeTrack = dwNow >= s_dwNextTrack;
		FILE* out = NULL;
		if (writeTrack)
		{
			s_dwNextTrack = dwNow + PLAYERBOT_OCHAO_TRACK_MS;
			out = fopen(PLAYERBOT_OCHAO_TRACK_FILE, "a");
		}
		const time_t wall = time(0);
		int onMap = 0, crossing = 0;
		for (TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.begin(); it != s_mapPlayerBotAIStates.end(); ++it)
		{
			const DWORD pid = it->first;
			const bool isCrossing = s_mapPlayerBotOchaoCrossing.count(pid) != 0;
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pid);
			if (!ch)
				continue;
			const bool here = ch->GetMapIndex() == PLAYERBOT_MAP_OCHAO;
			if (!here && !isCrossing)
				continue;
			TPlayerBotAIState& state = it->second;
			TPlayerBotOchaoTrack& t = s_mapPlayerBotOchaoTrack[pid];
			if (here)
			{
				++onMap;
				if (t.dwEntered == 0)
				{
					t.dwEntered = dwNow;
					t.dwAnchorSince = dwNow;
					t.dwVisitDeaths = 0;
					t.adwDeathAt[0] = t.adwDeathAt[1] = 0;
				}
				// Experience, across a level too.
				const DWORD exp = ch->GetExp();
				if (t.bLastLevel == 0)
				{
					t.bLastLevel = ch->GetLevel();
					t.dwLastExp = exp;
					t.dwLastNext = ch->GetNextExp();
				}
				else if (ch->GetLevel() > t.bLastLevel)
				{
					if (t.dwLastNext > t.dwLastExp)
						t.ullExp += t.dwLastNext - t.dwLastExp;
					t.ullExp += exp;
					sys_log(0, "OCHAO_BOT: level up pid=%u name=%s level=%u", pid, ch->GetName(), (unsigned int)ch->GetLevel());
				}
				else if (exp > t.dwLastExp)
					t.ullExp += exp - t.dwLastExp;
				t.bLastLevel = ch->GetLevel();
				t.dwLastExp = exp;
				t.dwLastNext = ch->GetNextExp();
				// Deaths.
				if (ch->IsDead() && !t.bWasDead)
				{
					++t.dwDeaths;
					++t.dwVisitDeaths;
					t.adwDeathAt[0] = t.adwDeathAt[1];
					t.adwDeathAt[1] = dwNow;
					sys_log(0, "OCHAO_BOT: died pid=%u name=%s pos=(%ld,%ld)", pid, ch->GetName(), ch->GetX(), ch->GetY());
				}
				// Revived where it fell, for the second time in a short while: up
				// at the gate instead, the temple's restart in town.
				// Not a bot on its way out: the gate is the far end of the labyrinth.
				if (t.bWasDead && !ch->IsDead() && t.adwDeathAt[0] != 0 &&
						!s_mapPlayerBotOchaoExit.count(pid) && !s_mapPlayerBotOchaoPending.count(pid) &&
						dwNow - t.adwDeathAt[0] <= PLAYERBOT_OCHAO_DEATH_WINDOW_MS)
				{
					state.dwTargetVID = 0;
					ch->SetVictim(NULL);
					ClearPlayerBotRoute(state, true);
					ch->Stop();
					const long fromX = ch->GetX(), fromY = ch->GetY();
					if (ch->Show(PLAYERBOT_MAP_OCHAO, PLAYERBOT_OCHAO_ARRIVAL_X, PLAYERBOT_OCHAO_ARRIVAL_Y, 0))
					{
						ch->Stop();
						ch->SendMovePacket(FUNC_MOVE, 0, PLAYERBOT_OCHAO_ARRIVAL_X, PLAYERBOT_OCHAO_ARRIVAL_Y, 0, dwNow);
						s_mapPlayerBotOchaoExit.erase(pid);
						sys_log(0, "OCHAO_BOT: restarted at the gate pid=%u name=%s from=(%ld,%ld) visit_deaths=%u",
								pid, ch->GetName(), fromX, fromY, t.dwVisitDeaths);
					}
					t.adwDeathAt[0] = t.adwDeathAt[1] = 0;
				}
				t.bWasDead = ch->IsDead();
				// The Guardian's fighters.
				if (guardian && (ch->GetVictim() == guardian || guardian->GetVictim() == ch ||
						state.dwTargetVID == guardian->GetVID()))
				{
					if (s_dwPlayerBotOchaoGuardianEngaged == 0)
					{
						s_dwPlayerBotOchaoGuardianEngaged = dwNow;
						sys_log(0, "OCHAO_BOT: guardian engaged first=%u name=%s after_spawn_s=%u pos=(%ld,%ld)",
								pid, ch->GetName(), (dwNow - s_dwPlayerBotOchaoGuardianSeen) / 1000,
								guardian->GetX(), guardian->GetY());
					}
					s_setPlayerBotOchaoGuardianFighters.insert(pid);
				}
				// Stuck: no ground covered for a minute with nothing fought.
				const bool fighting = ch->GetVictim() != NULL ||
						(state.dwLastCombatActionTime != 0 && dwNow - state.dwLastCombatActionTime < 10000);
				if (fighting || ch->IsDead() ||
						DISTANCE_APPROX(ch->GetX() - t.lAnchorX, ch->GetY() - t.lAnchorY) > PLAYERBOT_OCHAO_STUCK_DISTANCE)
				{
					if (t.bStuck)
						sys_log(0, "OCHAO_BOT: unstuck pid=%u name=%s after_s=%u", pid, ch->GetName(), (dwNow - t.dwAnchorSince) / 1000);
					t.bStuck = false;
					t.lAnchorX = ch->GetX();
					t.lAnchorY = ch->GetY();
					t.dwAnchorSince = dwNow;
				}
				else if (!t.bStuck && dwNow - t.dwAnchorSince >= PLAYERBOT_OCHAO_STUCK_MS)
				{
					t.bStuck = true;
					++t.dwStuckEpisodes;
					sys_log(0, "OCHAO_BOT: stuck pid=%u name=%s pos=(%ld,%ld) action=%u route=%u/%u leaving=%d",
							pid, ch->GetName(), ch->GetX(), ch->GetY(), (unsigned int)state.bCurrentAction,
							(unsigned int)state.uRouteIndex, (unsigned int)state.vecRoute.size(),
							s_mapPlayerBotOchaoExit.count(pid) ? 1 : 0);
				}
				if (t.bStuck)
					t.dwStuckMs += 1000;
			}
			else
				++crossing;
			if (out)
			{
				LPCHARACTER victim = ch->GetVictim();
				fprintf(out, "%ld\t%u\t%s\t%u\t%u\t%ld\t%ld\t%ld\t%d\t%u\t%u\t%llu\t%u\t%u\t%d\t%d\t%u\t%u\t%u\n",
						(long)wall, pid, ch->GetName(), (unsigned int)ch->GetLevel(), (unsigned int)(ch->GetJob() % 4),
						ch->GetMapIndex(), ch->GetX(), ch->GetY(),
						ch->GetMaxHP() > 0 ? (int)(ch->GetHP() * 100LL / ch->GetMaxHP()) : 0,
						(unsigned int)state.bCurrentAction, t.dwKills, t.ullExp, t.dwDeaths, t.dwStuckEpisodes,
						t.bStuck ? 1 : 0, s_mapPlayerBotOchaoExit.count(pid) ? 1 : (isCrossing ? 2 : 0),
						victim ? (unsigned int)victim->GetRaceNum() : 0U, t.dwBossKills, t.dwStuckMs / 1000);
			}
		}
		if (out)
		{
			fprintf(out, "#\t%ld\ton_map=%d\tcrossing=%d\tguardian=%u\tguardian_fighters=%u\tportal=%u\twalks=%u\twalks_nav=%u\n",
					(long)wall, onMap, crossing, guardian ? (unsigned int)guardian->GetVID() : 0U,
					(unsigned int)s_setPlayerBotOchaoGuardianFighters.size(), (unsigned int)mt2009_ochao::s_dwPortalVID,
					s_uPlayerBotOchaoWalksPlanned, s_uPlayerBotOchaoWalksNavOnly);
			fclose(out);
			// Walks nobody has stepped for five minutes (a raid over, a bot gone).
			for (std::map<DWORD, TPlayerBotOchaoWalk>::iterator w = s_mapPlayerBotOchaoWalk.begin();
					w != s_mapPlayerBotOchaoWalk.end();)
			{
				if (dwNow - w->second.dwLastUsed > 300000)
					s_mapPlayerBotOchaoWalk.erase(w++);
				else
					++w;
			}
		}
	}
}

#endif
