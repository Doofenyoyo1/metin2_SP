#ifndef __INC_METIN2_PLAYERBOT_AREZZO_BOTS_H__
#define __INC_METIN2_PLAYERBOT_AREZZO_BOTS_H__

// MT2009_PLUS_AREZZO_BOTS_V1 - the bots on the Arezzo module's three open maps
// (playerbot_arezzo.h is the module's switch, flag mt2009_arezzo_closed):
//
//   * Dolina Cyklopow (360, metin2_map_exp): cyclopes 9601-9605 of 43-49,
//     Arges (9606, three points, every 50-70 min) and Polifem (9607, every
//     2-3 h); twelve Metins of Shadow (8009);
//   * Pustkowie Faraona (361, metin2_map_pustynia): the pyramid's 9671-9680
//     of 53-58, Bastet (9675, three points, 50-70 min) and Anubis (9681, two
//     points, 2-3 h); fourteen desert Metins (9677, 9678);
//   * Zaczarowany Las (362, natural_map): lemurs 9611-9615 of 97-101, Lemur
//     Hrabia (3390, three points, 50-70 min) and Straz Przyboczna Lemur (3391,
//     two points, 2-3 h); Metins 8052, 8053. Straznik Dzungli (20425) stands
//     at its gate - the Jungle dungeon's guard, which no bot ever talks to.
//
// For now only the operator's test cohorts go there (the owner, 30
// September); every other bot keeps its frontier, and TransitionPlayerBotMap
// refuses any bot this file has not sent (RoutePlayerBotArezzoTransition).
//
// What a bot knows of each map, measured on the map's own files by
// tools/arezzo_bot_map.py (server_attr at 100 units, a cell blocked when any
// of its four 50-unit cells carries ATTR_BLOCK or ATTR_OBJECT):
//
//   * the way in: 360 and 361 from the village Teleporter's first page (the
//     frontier's own road, GetPlayerBotFrontierMapForLevel), landing on the
//     map's Town.txt; 362 only through the Temple of Ochao - Orc Valley,
//     Straznik Swiatyni (20426), the labyrinth (playerbot_ochao_bots.h) to
//     Straznik En-Tai (6400), his fall, and his Portal (20415) for the minute
//     it stands, whose first choice is the Las (temple_of_the_ochao.quest);
//   * where to hunt: PLAYERBOT_AREZZO_HUBS_<map>, the regen's spawn lines
//     clustered (24/23/24 spots), each on open ground of the map's one
//     walkable piece, with the walk between every two of them;
//   * the routes: PLAYERBOT_AREZZO_TREE_<map>, a tree of straight legs
//     rooted at the map's Teleporter whose leaves are the arrival, every
//     spot, every boss point and every stone - the walk out, the walk to a
//     far spot and the boss raid's walk go leg by leg along it
//     (WalkPlayerBotInArezzo), with the navigation only for the last stretch;
//   * the way out: the map's Teleporter (9012 beside the arrival) to the
//     bot's village, or the Teleport Ring. Another pass's warp from the
//     middle of the map (a shop's upkeep, Uriel) is walked out to the
//     Teleporter first, as in the Temple of Ochao;
//   * the bosses: boss raids (playerbot_boss_raid.h rows) of the bots
//     already on the map, walked there by the routes.
//
// The test hook, for the operator: a file "playerbot_arezzo_test" in the
// core's directory (the core that hosts 360-362 and the temple):
//     send <map> <N> [level-min level-max [any]]
//                                           N online bots of those levels go
//                                           to the map and stay while sent -
//                                           only the cohort file's bots for
//                                           that map when it names any, unless
//                                           "any";
//     leave <map> <N>                       N of them leave by the Teleporter;
//     leaveall                              every one of them leaves;
//     reset                                 forget every order;
//     cohort                                read playerbot_arezzo_cohort.txt
//                                           again and log its bots in;
//     spawn <race>                          a boss at his first boss.txt point.
// "playerbot_arezzo_cohort.txt" ("<map> <pid>" a line) names the test
// characters: they are logged in on top of the population a minute after the
// core starts (CPlayerBotManager::ScheduleExtraBots) and never sent to rest.
// The orders are kept in playerbot_arezzo_orders.txt and read back after a
// restart. The watch writes playerbot_arezzo_track.tsv every 15 s and ARZ_BOT
// lines to syslog.
//
// An implementation fragment in the sense playerbot_types.h describes:
// include it once, after playerbot_ochao_bots.h and before playerbot_travel.h.

namespace
{
	// From playerbot_travel.h and playerbot_demon_tower.h, which come later.
	bool TransitionPlayerBotMap(LPCHARACTER ch, TPlayerBotAIState& state,
			long targetMap, long targetX, long targetY, DWORD dwNow, const char* reason);
	bool MovePlayerBotToWorldPortal(LPCHARACTER ch, TPlayerBotAIState& state,
			long portalX, long portalY, long targetMap, long targetX, long targetY,
			DWORD dwNow, const char* reason);
	bool GetPlayerBotVillageReturn(LPCHARACTER ch, playerbot_empire_rules::EMapRole role,
			long& destMap, long& destX, long& destY);
	bool BlocksPlayerBotTravel(LPCHARACTER ch);
	void CountPlayerBotPotions(LPCHARACTER ch, size_t& redCount, size_t& blueCount);
	bool IsPlayerBotHumanLedParty(LPPARTY party);
	bool IsPlayerBotHeldForCompany(LPCHARACTER ch);
	bool IsPlayerBotOnMercContract(DWORD pid);
	bool FightPlayerBotTowerObjective(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER foe, DWORD dwNow);

	// ------------------------------------------------------------ the maps

	struct TPlayerBotArezzoNode { long x; long y; short next; };

	// tools/arezzo_bot_map.py gen metin2_map_exp 360 (walkable 47.8 %, one piece
	// holding 99.8 % of it; the walk between spots is 1.12 times the straight
	// line at the median, 1.71 at p90).
	// map 360: arrival (265050, 305150), Teleporter (265450, 305450); 24 spots, 121 corners.
	const TPlayerBotHuntingHub PLAYERBOT_AREZZO_HUBS_360[] = {
		{ 258150, 307050, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 8 spawn lines
		{ 242950, 334950, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 6 spawn lines
		{ 264750, 369350, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 6 spawn lines
		{ 308850, 332850, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 6 spawn lines
		{ 269850, 294450, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 247950, 325450, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 261350, 317950, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 263250, 339350, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 288050, 362650, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 318850, 339850, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 321050, 318150, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 267150, 323550, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 255750, 372150, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 275550, 352250, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 296650, 372350, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 312850, 311950, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 322450, 292150, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 292550, 292650, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 272950, 344650, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 288050, 341250, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 244050, 291650, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 3 spawn lines
		{ 261450, 328050, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 3 spawn lines
		{ 255650, 344250, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 3 spawn lines
		{ 244450, 352450, PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, 255, false, 0 },	// 3 spawn lines
	};
	const TPlayerBotArezzoNode PLAYERBOT_AREZZO_TREE_360[] = {
		{ 265450, 305450, -1 }, { 265050, 305150, 0 }, { 265350, 305450, 0 }, { 262950, 304450, 2 },
		{ 258150, 307050, 3 }, { 262750, 304350, 3 }, { 262250, 304850, 5 }, { 262050, 312450, 6 },
		{ 242950, 334950, 7 }, { 262050, 315450, 7 }, { 262050, 316650, 9 }, { 254850, 353050, 10 },
		{ 264750, 369350, 11 }, { 262050, 332850, 10 }, { 268450, 339150, 13 }, { 270950, 340250, 14 },
		{ 280650, 340650, 15 }, { 308850, 332850, 16 }, { 269850, 294450, 0 }, { 262050, 314050, 7 },
		{ 252750, 323150, 19 }, { 247950, 325450, 20 }, { 262050, 317250, 10 }, { 261350, 317950, 22 },
		{ 262050, 338150, 13 }, { 263250, 339350, 24 }, { 262050, 339750, 24 }, { 279450, 357150, 26 },
		{ 288050, 362650, 27 }, { 301050, 340650, 16 }, { 308550, 339050, 29 }, { 318850, 339850, 30 },
		{ 262050, 312250, 6 }, { 269550, 319550, 32 }, { 284850, 324750, 33 }, { 301250, 317750, 34 },
		{ 305650, 316050, 35 }, { 310650, 315350, 36 }, { 321050, 318150, 37 }, { 262050, 318450, 22 },
		{ 267150, 323550, 39 }, { 254850, 356450, 11 }, { 258350, 365650, 41 }, { 258750, 369150, 42 },
		{ 255750, 372150, 43 }, { 262050, 338750, 24 }, { 275550, 352250, 45 }, { 279950, 357650, 27 },
		{ 282250, 357950, 47 }, { 296650, 372350, 48 }, { 309350, 315450, 36 }, { 312850, 311950, 50 },
		{ 269950, 319850, 33 }, { 272250, 320150, 52 }, { 272450, 319950, 53 }, { 279850, 311850, 54 },
		{ 285850, 306350, 55 }, { 291150, 302650, 56 }, { 311950, 292650, 57 }, { 322450, 292150, 58 },
		{ 274650, 317750, 54 }, { 275050, 316650, 60 }, { 292550, 292650, 61 }, { 262050, 333750, 13 },
		{ 272950, 344650, 63 }, { 287450, 340650, 16 }, { 288050, 341250, 65 }, { 265450, 301350, 0 },
		{ 265450, 297750, 67 }, { 264350, 295950, 68 }, { 262250, 294650, 69 }, { 244050, 291650, 70 },
		{ 262050, 327450, 39 }, { 261450, 328050, 72 }, { 262050, 337850, 63 }, { 255650, 344250, 74 },
		{ 262050, 334850, 63 }, { 244450, 352450, 76 }, { 258850, 294050, 70 }, { 245550, 293150, 78 },
		{ 240450, 298050, 79 }, { 238750, 306050, 80 }, { 289350, 324750, 34 }, { 293550, 320550, 82 },
		{ 314750, 338950, 30 }, { 323450, 347650, 84 }, { 262050, 339850, 26 }, { 267050, 344850, 86 },
		{ 279050, 360350, 87 }, { 255250, 351050, 86 }, { 244450, 361850, 89 }, { 242450, 368650, 90 },
		{ 258750, 293950, 78 }, { 248750, 293650, 92 }, { 245750, 290650, 93 }, { 258250, 319250, 9 },
		{ 256450, 321450, 95 }, { 255850, 322150, 96 }, { 253550, 326950, 97 }, { 254850, 355250, 11 },
		{ 265150, 365550, 99 }, { 281350, 305550, 61 }, { 282550, 302650, 101 }, { 286350, 292550, 102 },
		{ 268150, 346050, 87 }, { 268650, 349550, 104 }, { 287050, 367950, 105 }, { 289550, 340650, 65 },
		{ 290950, 339250, 107 }, { 286150, 324750, 34 }, { 299250, 311250, 109 }, { 312550, 292450, 58 },
		{ 315450, 289550, 111 }, { 292350, 340650, 107 }, { 315750, 364150, 113 }, { 316050, 367250, 114 },
		{ 321350, 374750, 115 }, { 317950, 338950, 84 }, { 323650, 338950, 117 }, { 318050, 315350, 37 },
		{ 324050, 309150, 119 },
	};
	const int PLAYERBOT_AREZZO_WALK_360[24][24] = {
		{ 0, 33980, 70140, 80400, 18240, 24100, 12180, 34340, 69420, 88320, 75640, 20100, 70740, 52160, 81900, 67680, 81940, 55740, 43520, 56720, 36480, 22320, 39640, 52320 },
		{ 33980, 0, 59160, 74820, 51260, 11500, 25440, 25760, 60840, 82740, 87060, 28760, 59760, 43580, 73320, 79100, 97040, 70840, 37400, 51140, 46540, 21260, 28660, 41340 },
		{ 70060, 59079, 0, 84520, 85900, 54360, 59000, 38360, 66040, 92440, 107900, 55720, 11400, 49380, 78520, 99940, 122020, 96780, 43900, 60360, 91980, 48940, 30420, 29900 },
		{ 80400, 74820, 84600, 0, 94560, 70100, 68220, 49240, 38120, 12800, 113520, 61340, 85200, 41060, 44700, 105559, 127640, 102400, 40620, 24160, 107059, 58079, 57760, 72240 },
		{ 18240, 51260, 85980, 94560, 0, 41380, 26900, 48500, 83580, 102480, 89800, 34260, 86580, 66320, 96060, 81840, 96100, 69900, 57679, 70880, 26920, 36960, 55479, 68160 },
		{ 24100, 11500, 54440, 70100, 41380, 0, 16400, 21040, 56120, 78020, 78820, 20520, 55040, 38860, 68600, 70860, 88800, 62600, 32680, 46420, 38640, 14540, 23940, 36620 },
		{ 12180, 25440, 59079, 68220, 26900, 16400, 0, 22160, 57240, 76140, 67300, 8040, 59679, 39980, 69720, 59340, 73600, 47400, 31339, 44540, 46100, 10140, 28580, 41260 },
		{ 34340, 25760, 38440, 49240, 48500, 21040, 22160, 0, 35080, 57160, 69540, 17360, 39040, 17820, 47560, 61579, 83660, 58420, 11820, 25560, 58660, 12020, 9560, 24040 },
		{ 69420, 60840, 66120, 38120, 83580, 56120, 57240, 35080, 0, 42680, 102540, 50360, 66720, 17260, 13140, 94580, 116659, 91420, 25900, 32280, 93740, 47100, 39760, 53760 },
		{ 88320, 82740, 92520, 12800, 102480, 78020, 76140, 57160, 42680, 0, 121440, 69260, 93120, 48980, 49260, 113480, 135560, 110320, 48540, 32080, 114980, 66000, 65680, 80160 },
		{ 75640, 87060, 107980, 113520, 89800, 78820, 67300, 69540, 102540, 121440, 0, 59260, 108580, 85280, 115020, 10680, 27200, 42540, 76640, 89840, 109000, 65800, 78080, 92560 },
		{ 20100, 28760, 55800, 61340, 34260, 20520, 8040, 17360, 50360, 69260, 59260, 0, 56400, 33100, 62840, 51300, 68280, 42080, 24460, 37660, 53460, 7500, 25300, 37980 },
		{ 70740, 59760, 11480, 85200, 86580, 55040, 59679, 39040, 66720, 93120, 108580, 56400, 0, 50060, 79200, 100620, 122700, 97460, 44580, 61040, 92660, 49620, 31100, 27160 },
		{ 52160, 43580, 49460, 41060, 66320, 38860, 39980, 17820, 17260, 48980, 85280, 33100, 50060, 0, 29739, 77320, 99400, 74160, 8640, 16900, 76480, 29839, 23100, 37100 },
		{ 81900, 73320, 78600, 44700, 96060, 68600, 69720, 47560, 13140, 49260, 115020, 62840, 79200, 29739, 0, 107059, 129140, 103900, 38380, 39180, 106220, 59579, 52240, 66240 },
		{ 67680, 79100, 100020, 105559, 81840, 70860, 59340, 61579, 94580, 113480, 10680, 51300, 100620, 77320, 107059, 0, 29439, 33060, 68680, 81880, 101040, 57840, 70120, 84600 },
		{ 81940, 97040, 122100, 127640, 96100, 88800, 73600, 83660, 116659, 135560, 27200, 68280, 122700, 99400, 129140, 29439, 0, 30100, 90760, 103959, 115300, 75780, 92200, 105240 },
		{ 55740, 70840, 96860, 102400, 69900, 62600, 47400, 58420, 91420, 110320, 42540, 42080, 97460, 74160, 103900, 33060, 30100, 0, 65520, 78720, 89100, 49580, 66360, 79040 },
		{ 43520, 37400, 43980, 40620, 57679, 32680, 31339, 11820, 25900, 48540, 76640, 24460, 44580, 8640, 38380, 68680, 90760, 65520, 0, 16460, 69640, 21200, 17460, 31620 },
		{ 56720, 51140, 60440, 24160, 70880, 46420, 44540, 25560, 32280, 32080, 89840, 37660, 61040, 16900, 39180, 81880, 103959, 78720, 16460, 0, 83380, 34400, 33600, 48080 },
		{ 36480, 46540, 92060, 107059, 26920, 38640, 46100, 58660, 93740, 114980, 109000, 53460, 92660, 76480, 106220, 101040, 115300, 89100, 69640, 83380, 0, 51500, 61560, 74240 },
		{ 22320, 21260, 49020, 58079, 36960, 14540, 10140, 12020, 47100, 66000, 65800, 7500, 49620, 29839, 59579, 57840, 75780, 49580, 21200, 34400, 51500, 0, 18520, 31200 },
		{ 39640, 28660, 30500, 57760, 55479, 23940, 28580, 9560, 39760, 65680, 78080, 25300, 31100, 23100, 52240, 70120, 92200, 66360, 17460, 33600, 61560, 18520, 0, 14480 },
		{ 52320, 41340, 29980, 72240, 68160, 36620, 41260, 24040, 53760, 80160, 92560, 37980, 27160, 37100, 66240, 84600, 105240, 79040, 31620, 48080, 74240, 31200, 14480, 0 },
	};
	// tools/arezzo_bot_map.py gen metin2_map_pustynia 361 (walkable 43.4 %, one
	// piece holding 99.8 %; walk/straight 1.14 median, 1.56 p90).
	// map 361: arrival (240950, 394050), Teleporter (241350, 394350); 23 spots, 138 corners.
	const TPlayerBotHuntingHub PLAYERBOT_AREZZO_HUBS_361[] = {
		{ 266550, 433350, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 6 spawn lines
		{ 252950, 475750, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 6 spawn lines
		{ 274850, 452250, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 307650, 455350, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 321550, 394050, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 244050, 429350, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 259150, 433150, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 262750, 443950, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 243150, 477250, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 265950, 472550, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 301650, 393650, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 318550, 423950, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 248750, 408950, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 3 spawn lines
		{ 240350, 410250, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 3 spawn lines
		{ 263950, 421050, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 3 spawn lines
		{ 291350, 472150, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 3 spawn lines
		{ 305850, 476650, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 3 spawn lines
		{ 300250, 456950, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 3 spawn lines
		{ 322750, 460050, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 3 spawn lines
		{ 325750, 446450, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 3 spawn lines
		{ 297650, 439350, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 3 spawn lines
		{ 312250, 431850, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 3 spawn lines
		{ 323750, 420250, PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, 255, false, 0 },	// 3 spawn lines
	};
	const TPlayerBotArezzoNode PLAYERBOT_AREZZO_TREE_361[] = {
		{ 241350, 394350, -1 }, { 240950, 394050, 0 }, { 260950, 394350, 0 }, { 263950, 396950, 2 },
		{ 264150, 409350, 3 }, { 266550, 433350, 4 }, { 264150, 430950, 4 }, { 264150, 432550, 6 },
		{ 254350, 447350, 7 }, { 245050, 455950, 8 }, { 243150, 458550, 9 }, { 242550, 467850, 10 },
		{ 245250, 470850, 11 }, { 252950, 475750, 12 }, { 264150, 437550, 7 }, { 255150, 453550, 14 },
		{ 254150, 455450, 15 }, { 253950, 459150, 16 }, { 257950, 463250, 17 }, { 260550, 464550, 18 },
		{ 265250, 464850, 19 }, { 267950, 462250, 20 }, { 274850, 452250, 21 }, { 264150, 420250, 4 },
		{ 270650, 426550, 23 }, { 275350, 428550, 24 }, { 307650, 455350, 25 }, { 261350, 394350, 2 },
		{ 263150, 394350, 27 }, { 279350, 405750, 28 }, { 281550, 406950, 29 }, { 305350, 399150, 30 },
		{ 321550, 394050, 31 }, { 264150, 400350, 3 }, { 261850, 402650, 33 }, { 247250, 414250, 34 },
		{ 244050, 419250, 35 }, { 244050, 429350, 36 }, { 264150, 428150, 23 }, { 259150, 433150, 38 },
		{ 264150, 442550, 14 }, { 262750, 443950, 40 }, { 242550, 468250, 11 }, { 243150, 477250, 42 },
		{ 253950, 459350, 17 }, { 265950, 472550, 44 }, { 288350, 406950, 30 }, { 301650, 393650, 46 },
		{ 291650, 406950, 46 }, { 297850, 409550, 48 }, { 318550, 423950, 49 }, { 261250, 403250, 34 },
		{ 258650, 404650, 51 }, { 256550, 404950, 52 }, { 248750, 408950, 53 }, { 255150, 405550, 53 },
		{ 251850, 405850, 55 }, { 240350, 410250, 56 }, { 264150, 420850, 23 }, { 263950, 421050, 58 },
		{ 279050, 428550, 25 }, { 298650, 448150, 60 }, { 299650, 450550, 61 }, { 299950, 453550, 62 },
		{ 291350, 472150, 63 }, { 299950, 453850, 63 }, { 307450, 461350, 65 }, { 308550, 465350, 66 },
		{ 308850, 469350, 67 }, { 305850, 476650, 68 }, { 299950, 456650, 65 }, { 300250, 456950, 70 },
		{ 286550, 406950, 30 }, { 287250, 407650, 72 }, { 292550, 413250, 73 }, { 292750, 427550, 74 },
		{ 296750, 431550, 75 }, { 307150, 431750, 76 }, { 318650, 442450, 77 }, { 322750, 460050, 78 },
		{ 298650, 410250, 49 }, { 304250, 411250, 80 }, { 314050, 421050, 81 }, { 325750, 446450, 82 },
		{ 292750, 427750, 75 }, { 297650, 439350, 84 }, { 308050, 431750, 77 }, { 312250, 431850, 86 },
		{ 305050, 411650, 81 }, { 306650, 412050, 88 }, { 323750, 420250, 89 }, { 243850, 425150, 36 },
		{ 241750, 427250, 91 }, { 293450, 406950, 48 }, { 315050, 422750, 82 }, { 315350, 426650, 94 },
		{ 324350, 435650, 95 }, { 268250, 461950, 21 }, { 269050, 460250, 97 }, { 269450, 457650, 98 },
		{ 275650, 451250, 99 }, { 308850, 473650, 68 }, { 307150, 477950, 101 }, { 252550, 405850, 55 },
		{ 240350, 418050, 103 }, { 264150, 416550, 4 }, { 263450, 417250, 105 }, { 264150, 435350, 7 },
		{ 265650, 436850, 107 }, { 267150, 394350, 28 }, { 280750, 407950, 109 }, { 280850, 428550, 60 },
		{ 288450, 429350, 111 }, { 308550, 412050, 89 }, { 322550, 426050, 113 }, { 305450, 399050, 31 },
		{ 312850, 397850, 115 }, { 318050, 397550, 116 }, { 324850, 397450, 117 }, { 264150, 416050, 4 },
		{ 256050, 424150, 119 }, { 245250, 432150, 120 }, { 243550, 434450, 121 }, { 241950, 440350, 122 },
		{ 242550, 472750, 42 }, { 243250, 473450, 124 }, { 265450, 464850, 20 }, { 274050, 460050, 126 },
		{ 283350, 428550, 111 }, { 293750, 438950, 128 }, { 305550, 411950, 88 }, { 310050, 416450, 130 },
		{ 308850, 469450, 68 }, { 314250, 474650, 132 }, { 317750, 475050, 133 }, { 320750, 472650, 134 },
		{ 315350, 430950, 95 }, { 323750, 439350, 136 },
	};
	const int PLAYERBOT_AREZZO_WALK_361[23][23] = {
		{ 0, 58320, 61100, 51900, 79540, 24100, 7480, 12120, 54160, 49040, 61779, 56479, 31520, 38620, 13340, 67980, 73800, 49460, 77040, 66440, 35500, 47100, 63160 },
		{ 58320, 0, 67100, 99140, 136780, 52900, 55560, 49200, 10400, 54320, 119020, 113720, 75180, 72280, 69580, 80760, 96580, 91100, 125559, 123680, 92740, 104340, 120400 },
		{ 61100, 67100, 0, 47580, 111420, 65220, 58340, 48980, 62940, 23860, 107459, 79200, 86460, 84600, 72360, 29200, 45500, 39540, 74000, 82400, 57279, 68780, 84980 },
		{ 51900, 99140, 47580, 0, 71580, 74000, 59220, 61560, 94980, 51760, 70180, 39360, 77460, 85340, 57420, 26200, 22980, 8040, 42960, 42560, 20000, 28939, 45140 },
		{ 79540, 136780, 111420, 71580, 0, 100440, 86860, 89200, 132620, 115600, 20060, 32220, 84840, 93760, 83220, 91300, 94560, 72540, 66640, 54079, 58200, 42640, 27080 },
		{ 24100, 52900, 65220, 74000, 100440, 0, 16620, 27239, 48740, 53160, 82680, 78580, 22440, 20580, 23220, 78880, 94700, 71560, 99140, 88540, 57600, 69200, 85260 },
		{ 7480, 55560, 58340, 59220, 86860, 16620, 0, 12240, 51400, 46280, 69100, 63800, 28360, 33600, 14019, 72000, 81120, 56779, 84360, 73760, 42820, 54420, 70480 },
		{ 12120, 49200, 48980, 61560, 89200, 27239, 12240, 0, 45040, 36920, 71440, 66140, 40600, 44220, 23380, 62640, 78460, 59120, 86700, 76100, 45160, 56760, 72820 },
		{ 54160, 10400, 62940, 94980, 132620, 48740, 51400, 45040, 0, 50160, 114859, 109559, 71020, 68120, 65420, 76600, 92420, 86940, 121400, 119520, 88580, 100180, 116240 },
		{ 49040, 54320, 23860, 51760, 115600, 53160, 46280, 36920, 50160, 0, 108359, 83380, 74400, 72540, 60300, 29560, 42260, 43720, 72040, 86580, 61460, 72960, 89160 },
		{ 61779, 119020, 107459, 70180, 20060, 82680, 69100, 71440, 114859, 108359, 0, 47480, 65100, 74020, 64879, 87340, 93160, 68820, 81900, 69340, 50180, 54479, 42340 },
		{ 56479, 113720, 79200, 39360, 32220, 78580, 63800, 66140, 109559, 83380, 47480, 0, 82040, 89920, 62000, 59079, 62340, 40320, 37780, 25380, 27060, 10420, 6680 },
		{ 31520, 75180, 86460, 77460, 84840, 22440, 28360, 40600, 71020, 74400, 65100, 82040, 0, 8920, 20040, 93540, 99360, 75020, 102600, 92000, 61060, 72660, 87200 },
		{ 38620, 72280, 84600, 85340, 93760, 20580, 33600, 44220, 68120, 72540, 74020, 89920, 8920, 0, 27920, 98260, 107240, 82900, 110480, 99880, 68940, 80540, 96120 },
		{ 13340, 69580, 72360, 57420, 83220, 23220, 14019, 23380, 65420, 60300, 64879, 62000, 20040, 27920, 0, 73500, 79320, 54979, 82560, 71960, 41020, 52620, 68520 },
		{ 67980, 80760, 29200, 26200, 91300, 78880, 72000, 62640, 76600, 29560, 87340, 59079, 93540, 98260, 73500, 0, 16300, 18760, 44800, 59600, 37160, 48660, 64860 },
		{ 73800, 96580, 45500, 22980, 94560, 94700, 81120, 78460, 92420, 42260, 93160, 62340, 99360, 107240, 79320, 16300, 0, 26020, 29780, 44580, 42980, 51920, 68120 },
		{ 49460, 91100, 39540, 8040, 72540, 71560, 56779, 59120, 86940, 43720, 68820, 40320, 75020, 82900, 54979, 18760, 26020, 0, 46000, 43520, 18640, 29900, 46100 },
		{ 77040, 125559, 74000, 42960, 66640, 99140, 84360, 86700, 121400, 72040, 81900, 37780, 102600, 110480, 82560, 44800, 29780, 46000, 0, 14800, 41940, 32400, 40200 },
		{ 66440, 123680, 82400, 42560, 54079, 88540, 73760, 76100, 119520, 86580, 69340, 25380, 92000, 99880, 71960, 59600, 44580, 43520, 14800, 0, 31339, 20000, 27000 },
		{ 35500, 92740, 57279, 20000, 58200, 57600, 42820, 45160, 88580, 61460, 50180, 27060, 61060, 68940, 41020, 37160, 42980, 18640, 41940, 31339, 0, 17600, 33740 },
		{ 47100, 104340, 68780, 28939, 42640, 69200, 54420, 56760, 100180, 72960, 54479, 10420, 72660, 80540, 52620, 48660, 51920, 29900, 32400, 20000, 17600, 0, 16200 },
		{ 63160, 120400, 84980, 45140, 27080, 85260, 70480, 72820, 116240, 89160, 42340, 6680, 87200, 96120, 68520, 64860, 68120, 46100, 40200, 27000, 33740, 16200, 0 },
	};
	// tools/arezzo_bot_map.py gen natural_map 362 (walkable 47.2 %, one piece
	// holding all of it; walk/straight 1.53 median, 2.34 p90 - a forest of
	// ridges, where the straight line lies most).
	// map 362: arrival (378750, 394650), Teleporter (379150, 394950); 24 spots, 149 corners.
	const TPlayerBotHuntingHub PLAYERBOT_AREZZO_HUBS_362[] = {
		{ 418550, 406950, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 11 spawn lines
		{ 388150, 458450, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 10 spawn lines
		{ 354050, 397450, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 8 spawn lines
		{ 350850, 416950, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 7 spawn lines
		{ 417550, 456550, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 7 spawn lines
		{ 395650, 455550, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 7 spawn lines
		{ 381950, 459050, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 7 spawn lines
		{ 347150, 467150, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 7 spawn lines
		{ 368050, 417750, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 6 spawn lines
		{ 423050, 399850, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 6 spawn lines
		{ 408750, 420950, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 6 spawn lines
		{ 400850, 399550, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 6 spawn lines
		{ 422950, 426050, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 6 spawn lines
		{ 420450, 473550, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 6 spawn lines
		{ 375950, 402050, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 367950, 410450, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 359650, 412550, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 396350, 421450, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 414850, 444550, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 423650, 445250, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 363950, 451550, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 355550, 464450, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 5 spawn lines
		{ 347150, 397150, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
		{ 385150, 408450, PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, 255, false, 0 },	// 4 spawn lines
	};
	const TPlayerBotArezzoNode PLAYERBOT_AREZZO_TREE_362[] = {
		{ 379150, 394950, -1 }, { 378750, 394650, 0 }, { 371150, 410750, 0 }, { 375650, 413850, 2 },
		{ 379150, 422450, 3 }, { 375450, 434250, 4 }, { 374750, 437250, 5 }, { 375450, 438350, 6 },
		{ 388750, 439250, 7 }, { 418550, 406950, 8 }, { 394750, 443950, 8 }, { 398750, 444150, 10 },
		{ 400650, 445050, 11 }, { 400650, 450250, 12 }, { 403450, 456150, 13 }, { 401450, 459550, 14 },
		{ 388750, 464150, 15 }, { 386150, 464550, 16 }, { 385050, 463650, 17 }, { 388150, 458450, 18 },
		{ 377450, 396650, 0 }, { 370450, 401450, 20 }, { 361350, 406050, 21 }, { 360050, 406250, 22 },
		{ 354050, 397450, 23 }, { 367250, 403150, 21 }, { 364450, 403350, 25 }, { 350850, 416950, 26 },
		{ 400650, 450650, 13 }, { 400850, 450850, 28 }, { 411050, 456050, 29 }, { 417550, 456550, 30 },
		{ 385050, 463250, 18 }, { 395650, 455550, 32 }, { 379150, 396050, 0 }, { 361150, 414050, 34 },
		{ 344750, 429150, 35 }, { 342550, 431950, 36 }, { 342350, 442150, 37 }, { 358950, 463950, 38 },
		{ 362650, 466650, 39 }, { 366250, 468250, 40 }, { 374350, 467950, 41 }, { 381950, 459050, 42 },
		{ 348150, 452450, 38 }, { 348550, 453550, 44 }, { 345150, 457450, 45 }, { 343950, 459350, 46 },
		{ 343650, 463650, 47 }, { 347150, 467150, 48 }, { 368050, 417750, 2 }, { 391550, 436150, 8 },
		{ 392450, 434350, 51 }, { 392850, 432650, 52 }, { 423050, 399850, 53 }, { 388950, 439250, 8 },
		{ 401950, 426250, 55 }, { 408750, 420950, 56 }, { 371550, 411150, 2 }, { 373350, 411550, 58 },
		{ 395150, 406150, 59 }, { 400850, 399550, 60 }, { 401150, 451150, 29 }, { 406350, 451350, 62 },
		{ 415250, 443150, 63 }, { 422950, 426050, 64 }, { 404950, 451350, 62 }, { 418650, 465050, 66 },
		{ 420350, 468450, 67 }, { 420450, 473550, 68 }, { 378150, 397150, 34 }, { 377950, 400050, 70 },
		{ 375950, 402050, 71 }, { 377950, 400450, 71 }, { 367950, 410450, 73 }, { 376750, 397350, 20 },
		{ 374550, 397650, 75 }, { 359650, 412550, 76 }, { 392950, 430150, 53 }, { 396350, 421450, 78 },
		{ 411950, 446450, 63 }, { 414850, 444550, 80 }, { 413150, 446250, 80 }, { 421250, 446950, 82 },
		{ 423650, 445250, 83 }, { 342350, 444150, 38 }, { 350150, 451950, 85 }, { 351850, 452750, 86 },
		{ 363950, 451550, 87 }, { 348550, 454050, 45 }, { 355550, 464450, 89 }, { 360150, 406350, 22 },
		{ 355050, 401550, 91 }, { 347150, 397150, 92 }, { 383150, 410450, 59 }, { 385150, 408450, 94 },
		{ 379150, 426050, 4 }, { 370250, 434950, 96 }, { 365450, 438250, 97 }, { 355250, 440750, 98 },
		{ 403450, 457550, 14 }, { 409650, 467050, 100 }, { 403950, 472250, 101 }, { 394450, 475450, 102 },
		{ 415450, 442950, 64 }, { 418650, 430350, 104 }, { 423850, 425050, 105 }, { 343650, 463850, 48 },
		{ 348550, 474450, 107 }, { 388450, 439250, 7 }, { 390450, 441250, 109 }, { 353550, 400550, 92 },
		{ 351150, 400250, 111 }, { 347550, 396650, 112 }, { 371150, 414650, 2 }, { 354950, 433050, 114 },
		{ 377950, 401350, 73 }, { 361350, 417950, 116 }, { 375350, 397650, 75 }, { 369250, 403750, 118 },
		{ 376450, 415350, 3 }, { 376750, 418050, 120 }, { 380950, 422250, 121 }, { 410650, 412250, 78 },
		{ 408950, 407750, 123 }, { 404850, 451350, 62 }, { 406850, 449350, 125 }, { 408650, 445050, 126 },
		{ 408850, 442250, 127 }, { 404250, 433350, 128 }, { 411350, 428550, 129 }, { 342350, 442950, 38 },
		{ 345050, 445650, 131 }, { 359150, 464150, 39 }, { 361650, 465850, 133 }, { 364850, 469050, 134 },
		{ 379350, 438550, 7 }, { 381450, 440650, 136 }, { 401350, 459650, 15 }, { 395250, 460750, 138 },
		{ 387150, 468850, 139 }, { 399750, 444150, 11 }, { 402350, 445450, 141 }, { 418850, 465250, 67 },
		{ 419150, 467050, 143 }, { 417750, 468450, 144 }, { 417850, 473750, 145 }, { 417950, 446150, 82 },
		{ 420350, 448550, 147 },
	};
	const int PLAYERBOT_AREZZO_WALK_362[24][24] = {
		{ 0, 95660, 115659, 112259, 78840, 103300, 95060, 147280, 95380, 8900, 17920, 112440, 97920, 92640, 102180, 94880, 102340, 28000, 76780, 85940, 133060, 121380, 121000, 92640 },
		{ 95660, 0, 115220, 91960, 42560, 8660, 6440, 64020, 105180, 104559, 78640, 124280, 67480, 50520, 114020, 106720, 100660, 72280, 46340, 53179, 49800, 38120, 120380, 104480 },
		{ 115659, 115220, 0, 23260, 110680, 123880, 109559, 82820, 25900, 124559, 98760, 59460, 129759, 124480, 28860, 20780, 17340, 92280, 108620, 117780, 79460, 79440, 7020, 39660 },
		{ 112259, 91960, 23260, 0, 107280, 100620, 86300, 59560, 17520, 121159, 95360, 57500, 126359, 121080, 31060, 19700, 10560, 88880, 105220, 114380, 56200, 56179, 28420, 37700 },
		{ 78840, 42560, 110680, 107280, 0, 50200, 41960, 94180, 90400, 87740, 61820, 107459, 32660, 18240, 97200, 89900, 97360, 55460, 13080, 13740, 79960, 68280, 116020, 87660 },
		{ 103300, 8660, 123880, 100620, 50200, 0, 15100, 72680, 113840, 112200, 86280, 131920, 75120, 58160, 121659, 114359, 109320, 79920, 53979, 60820, 58460, 46780, 129040, 112120 },
		{ 95060, 6440, 109559, 86300, 41960, 15100, 0, 58360, 99520, 103959, 78040, 123680, 66880, 49920, 113420, 102340, 95000, 71680, 45740, 52579, 44140, 32460, 114720, 103880 },
		{ 147280, 64020, 82820, 59560, 94180, 72680, 58360, 0, 72780, 156180, 130259, 113400, 119100, 101420, 86960, 75600, 68260, 123900, 97960, 104800, 32980, 25900, 87980, 93600 },
		{ 95380, 105180, 25900, 17520, 90400, 113840, 99520, 72780, 0, 104280, 78480, 40620, 109480, 104200, 18860, 7340, 10480, 72000, 88340, 97500, 69420, 69400, 31060, 20820 },
		{ 8900, 104559, 124559, 121159, 87740, 112200, 103959, 156180, 104280, 0, 26820, 121340, 106820, 101540, 111080, 103780, 111240, 35460, 85680, 94840, 141960, 130280, 129900, 101540 },
		{ 17920, 78640, 98760, 95360, 61820, 86280, 78040, 130259, 78480, 26820, 0, 95540, 80900, 75620, 85280, 77980, 85440, 12600, 59760, 68920, 116040, 104359, 104100, 75740 },
		{ 112440, 124280, 59460, 57500, 107459, 131920, 123680, 113400, 40620, 121340, 95540, 0, 126540, 121259, 45980, 38680, 46940, 89060, 105400, 114559, 110040, 110020, 64800, 19800 },
		{ 97920, 67480, 129759, 126359, 32660, 75120, 66880, 119100, 109480, 106820, 80900, 126540, 0, 50260, 116280, 108980, 116440, 74540, 21740, 20760, 104880, 93200, 135100, 106740 },
		{ 92640, 50520, 124480, 121080, 18240, 58160, 49920, 101420, 104200, 101540, 75620, 121259, 50260, 0, 111000, 103700, 111159, 69260, 31320, 31100, 87200, 75520, 129820, 101460 },
		{ 102180, 114020, 28860, 31060, 97200, 121659, 113420, 86960, 18860, 111080, 85280, 45980, 116280, 111000, 0, 11600, 20500, 78800, 95140, 104300, 83600, 83580, 34200, 26180 },
		{ 94880, 106720, 20780, 19700, 89900, 114359, 102340, 75600, 7340, 103780, 77980, 38680, 108980, 103700, 11600, 0, 9140, 71500, 87840, 97000, 72240, 72220, 26120, 18880 },
		{ 102340, 100660, 17340, 10560, 97360, 109320, 95000, 68260, 10480, 111240, 85440, 46940, 116440, 111159, 20500, 9140, 0, 78960, 95300, 104459, 64900, 64879, 22500, 27139 },
		{ 28000, 72280, 92280, 88880, 55460, 79920, 71680, 123900, 72000, 35460, 12600, 89060, 74540, 69260, 78800, 71500, 78960, 0, 53400, 62560, 109680, 98000, 97620, 69260 },
		{ 76780, 46340, 108620, 105220, 13080, 53979, 45740, 97960, 88340, 85680, 59760, 105400, 21740, 31320, 95140, 87840, 95300, 53400, 0, 10440, 83740, 72060, 113959, 85600 },
		{ 85940, 53179, 117780, 114380, 13740, 60820, 52579, 104800, 97500, 94840, 68920, 114559, 20760, 31100, 104300, 97000, 104459, 62560, 10440, 0, 90580, 78900, 123120, 94760 },
		{ 133060, 49800, 79460, 56200, 79960, 58460, 44140, 32980, 69420, 141960, 116040, 110040, 104880, 87200, 83600, 72240, 64900, 109680, 83740, 90580, 0, 16260, 84620, 90240 },
		{ 121380, 38120, 79440, 56179, 68280, 46780, 32460, 25900, 69400, 130280, 104359, 110020, 93200, 75520, 83580, 72220, 64879, 98000, 72060, 78900, 16260, 0, 84600, 90220 },
		{ 121000, 120380, 7020, 28420, 116020, 129040, 114720, 87980, 31060, 129900, 104100, 64800, 135100, 129820, 34200, 26120, 22500, 97620, 113959, 123120, 84620, 84600, 0, 45000 },
		{ 92640, 104480, 39660, 37700, 87660, 112120, 103880, 93600, 20820, 101540, 75740, 19800, 106740, 101460, 26180, 18880, 27139, 69260, 85600, 94760, 90240, 90220, 45000, 0 },
	};

	struct TPlayerBotArezzoMap
	{
		long lMap;
		const char* szName;
		BYTE bMinLevel;
		long lArrivalX, lArrivalY;
		long lExitX, lExitY;
		const TPlayerBotHuntingHub* pHubs;
		int iHubs;
		const TPlayerBotArezzoNode* pTree;
		int iNodes;
		const int* pWalk;	// iHubs x iHubs
	};

#define PLAYERBOT_AREZZO_ROW(map, name, lvl, pfx) \
	{ map, name, lvl, pfx##_ARRIVAL_X, pfx##_ARRIVAL_Y, pfx##_EXIT_X, pfx##_EXIT_Y, \
	  PLAYERBOT_AREZZO_HUBS_##map, (int)(sizeof(PLAYERBOT_AREZZO_HUBS_##map) / sizeof(PLAYERBOT_AREZZO_HUBS_##map[0])), \
	  PLAYERBOT_AREZZO_TREE_##map, (int)(sizeof(PLAYERBOT_AREZZO_TREE_##map) / sizeof(PLAYERBOT_AREZZO_TREE_##map[0])), \
	  &PLAYERBOT_AREZZO_WALK_##map[0][0] }
	const TPlayerBotArezzoMap PLAYERBOT_AREZZO_MAPS[] = {
		PLAYERBOT_AREZZO_ROW(360, "Dolina Cyklopow", PLAYERBOT_AREZZO_CYCLOPS_MIN_LEVEL, PLAYERBOT_AREZZO_CYCLOPS),
		PLAYERBOT_AREZZO_ROW(361, "Pustkowie Faraona", PLAYERBOT_AREZZO_PHARAOH_MIN_LEVEL, PLAYERBOT_AREZZO_PHARAOH),
		PLAYERBOT_AREZZO_ROW(362, "Zaczarowany Las", PLAYERBOT_AREZZO_FOREST_MIN_LEVEL, PLAYERBOT_AREZZO_FOREST),
	};
#undef PLAYERBOT_AREZZO_ROW
	const int PLAYERBOT_AREZZO_MAP_COUNT = (int)(sizeof(PLAYERBOT_AREZZO_MAPS) / sizeof(PLAYERBOT_AREZZO_MAPS[0]));

	const TPlayerBotArezzoMap* GetPlayerBotArezzoMapInfo(long mapIndex)
	{
		for (int i = 0; i < PLAYERBOT_AREZZO_MAP_COUNT; ++i)
			if (PLAYERBOT_AREZZO_MAPS[i].lMap == mapIndex)
				return &PLAYERBOT_AREZZO_MAPS[i];
		return NULL;
	}

	// The bosses of the three maps, with the first boss.txt point of each (the
	// test hook's "spawn").
	struct TPlayerBotArezzoBoss { DWORD dwRace; long lMap; long lCellX; long lCellY; };
	const TPlayerBotArezzoBoss PLAYERBOT_AREZZO_BOSSES[] = {
		{ 9606, 360, 83, 244 },		// Arges
		{ 9607, 360, 486, 787 },	// Polifem
		{ 9675, 361, 113, 432 },	// Bastet
		{ 9681, 361, 452, 672 },	// Anubis
		{ 3390, 362, 224, 567 },	// Lemur Hrabia
		{ 3391, 362, 157, 904 },	// Straz Przyboczna Lemur
	};
	bool IsPlayerBotArezzoBossRace(DWORD race)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_AREZZO_BOSSES) / sizeof(PLAYERBOT_AREZZO_BOSSES[0]); ++i)
			if (PLAYERBOT_AREZZO_BOSSES[i].dwRace == race)
				return true;
		return false;
	}

	// ------------------------------------------------------------ the numbers

	// A corner is passed within this; a leg that has not gained this much
	// ground in this long is a stall, and after this many the walk goes on the
	// navigation alone (the Temple of Ochao's numbers).
	const int PLAYERBOT_AREZZO_NODE_REACHED = 350;
	const int PLAYERBOT_AREZZO_LEG_PROGRESS = 250;
	const DWORD PLAYERBOT_AREZZO_LEG_TIMEOUT = 15000;
	const int PLAYERBOT_AREZZO_MAX_STALLS = 4;
	// How far a corner may be for a bot to look for it, and how many of the
	// nearest are looked at (each look is a line of up to 600 cells).
	const int PLAYERBOT_AREZZO_NODE_SIGHT = 30000;
	const int PLAYERBOT_AREZZO_NODE_LOOKS = 10;
	// A walk longer than this whose goal is out of sight goes by the corners.
	const int PLAYERBOT_AREZZO_TREE_WALK_MIN = 6000;
	// The goal moved this far: the walk is planned again.
	const int PLAYERBOT_AREZZO_WALK_REPLAN = 1500;
	// A warp off the map is made within this of its Teleporter; a walk out
	// that has not got there in this long lets the warp through.
	const int PLAYERBOT_AREZZO_WARP_REACH = 450;
	const DWORD PLAYERBOT_AREZZO_PENDING_MAX_MS = 15 * 60 * 1000;
	// The Las is a long way in (the temple, its Guardian, his Portal): only
	// what stops the fight takes a bot out of it sooner than this.
	const DWORD PLAYERBOT_AREZZO_FOREST_MIN_VISIT_TIME = 12 * 60 * 1000;
	// Deaths: two within this long and the next revival is at the arrival
	// (the map's Town.txt, "restart in town"); this many in one visit and the
	// bot goes home.
	const DWORD PLAYERBOT_AREZZO_DEATH_WINDOW_MS = 240000;
	const DWORD PLAYERBOT_AREZZO_VISIT_DEATHS_LEAVE = 8;
	// The Las is an expedition (the owner, 1.10): a bot packs for it before
	// the road (this many reds, a caster this many blues, in stacks of 200)
	// and once in stays - deaths, an empty belt and the errands wait this
	// long from its arrival; only a lost weapon or armour takes it out sooner.
	const size_t PLAYERBOT_AREZZO_LAS_KIT_RED = 600;
	const size_t PLAYERBOT_AREZZO_LAS_KIT_BLUE = 400;
	const DWORD PLAYERBOT_AREZZO_LAS_STAY_MS = 2 * 60 * 60 * 1000;
	// The boss raids: a recruit is a bot on the map within this walk of the
	// boss; the gathering waits this long; a member further from him than
	// this walks the corners.
	const int PLAYERBOT_AREZZO_RAID_WALK_MAX = 70000;
	const DWORD PLAYERBOT_AREZZO_RAID_GATHER_MS = 5 * 60 * 1000;
	// The Las (round 3: no raid formed, "nobody to call ... free=0/0/0" 14
	// times): a dozen to forty bots over 24 spots on ground where the walk is
	// 1.5 times the straight line - the whole map's walk, and longer to gather.
	const int PLAYERBOT_AREZZO_FOREST_RAID_WALK_MAX = 160000;
	const DWORD PLAYERBOT_AREZZO_FOREST_RAID_GATHER_MS = 8 * 60 * 1000;
	const int PLAYERBOT_AREZZO_RAID_FIGHT_WALK = 3000;
	// The way into the Las from the temple: the Portal is taken when the walk
	// to it (by the labyrinth's corners) is no longer than this - it stands a
	// minute; the Guardian is walked to by the corners and fought from this near.
	const int PLAYERBOT_AREZZO_LAS_PORTAL_WALK_MAX = 12000;
	const int PLAYERBOT_AREZZO_LAS_PORTAL_NEAR = 2500;
	const int PLAYERBOT_AREZZO_LAS_GUARDIAN_NEAR = 1500;
	// The watch.
	const int PLAYERBOT_AREZZO_STUCK_DISTANCE = 200;
	const DWORD PLAYERBOT_AREZZO_STUCK_MS = 60000;
	const DWORD PLAYERBOT_AREZZO_TRACK_MS = 15000;
	const DWORD PLAYERBOT_AREZZO_COHORT_DELAY_MS = 60000;
	const char* const PLAYERBOT_AREZZO_TEST_FILE = "playerbot_arezzo_test";
	const char* const PLAYERBOT_AREZZO_TRACK_FILE = "playerbot_arezzo_track.tsv";
	const char* const PLAYERBOT_AREZZO_COHORT_FILE = "playerbot_arezzo_cohort.txt";

	// ------------------------------------------------------------ the state

	// The operator's orders: the map a bot is sent to, and the bots told to
	// leave. The cohort file's bots and the map each is for.
	std::map<DWORD, long> s_mapPlayerBotArezzoForced;
	std::set<DWORD> s_setPlayerBotArezzoLeave;
	std::map<DWORD, long> s_mapPlayerBotArezzoCohort;
	bool s_bPlayerBotArezzoCohortLoaded = false;
	// The Teleport Ring's warp under way (TryPlayerBotTeleportRingHome).
	bool s_bPlayerBotArezzoRingWarp = false;
	// The orders outlive a restart of the core: a deploy in the middle of a
	// test round is not the end of it ("reset" forgets them).
	const char* const PLAYERBOT_AREZZO_ORDERS_FILE = "playerbot_arezzo_orders.txt";
	bool s_bPlayerBotArezzoOrdersLoaded = false;

	// Another pass's warp off the map, walked out to the Teleporter first.
	struct TPlayerBotArezzoPending
	{
		long lMap, lX, lY;
		DWORD dwSince;
		char szReason[40];
	};
	std::map<DWORD, TPlayerBotArezzoPending> s_mapPlayerBotArezzoPending;
	// A bot on its way out (the frontier's own decision).
	struct TPlayerBotArezzoExit { DWORD dwStarted; char szReason[40]; };
	std::map<DWORD, TPlayerBotArezzoExit> s_mapPlayerBotArezzoExit;
	// A bot on its way into the Las: since when, and what it was last seen doing.
	struct TPlayerBotArezzoLas
	{
		DWORD dwSince; BYTE bPhase; DWORD dwPhaseSince;
		DWORD dwPortalVID;	// the Portal this bot has set out for
		DWORD dwLastLog;
		TPlayerBotArezzoLas() : dwSince(0), bPhase(0), dwPhaseSince(0), dwPortalVID(0), dwLastLog(0) {}
	};
	// A Portal within this is used (the quest's choice "Zaczarowany Las"),
	// without the portal walk's stall-and-wait: the NPC stands on the cell the
	// walk aims at, and the minute it stands is too short to wait out a stall.
	const int PLAYERBOT_AREZZO_LAS_PORTAL_USE = 700;
	enum { AREZZO_LAS_ROAD = 0, AREZZO_LAS_TEMPLE, AREZZO_LAS_GUARDIAN, AREZZO_LAS_PORTAL };
	std::map<DWORD, TPlayerBotArezzoLas> s_mapPlayerBotArezzoLas;

	struct TPlayerBotArezzoTrack
	{
		DWORD dwEntered;
		long lMap;
		DWORD dwSent;			// the test's send (0 if none)
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
		DWORD adwDeathAt[2];
		DWORD dwAwaySince;		// a sent bot seen off its map since (0: on it)
		DWORD dwNextResend;
		// The last monster (or player) seen fighting the bot, for "died by=".
		DWORD dwFoeRace;
		DWORD dwFoeAt;
		bool bFoePC;
		DWORD dwNextRestock;
		TPlayerBotArezzoTrack() : dwEntered(0), lMap(0), dwSent(0), dwKills(0), dwBossKills(0), ullExp(0),
				dwDeaths(0), dwLastExp(0), dwLastNext(0), bLastLevel(0), bWasDead(false),
				lAnchorX(0), lAnchorY(0), dwAnchorSince(0), bStuck(false), dwStuckEpisodes(0), dwStuckMs(0),
				dwVisitDeaths(0), dwAwaySince(0), dwNextResend(0), dwFoeRace(0), dwFoeAt(0), bFoePC(false), dwNextRestock(0) { adwDeathAt[0] = adwDeathAt[1] = 0; }
	};
	std::map<DWORD, TPlayerBotArezzoTrack> s_mapPlayerBotArezzoTrack;
	unsigned int s_uPlayerBotArezzoWalksPlanned = 0;
	unsigned int s_uPlayerBotArezzoWalksNavOnly = 0;
	unsigned int s_uPlayerBotArezzoRefused = 0;

	bool IsPlayerBotArezzoOpen()
	{
		return quest::CQuestManager::instance().GetEventFlag("mt2009_arezzo_closed") <= 0;
	}

	bool IsPlayerBotArezzoHosted(long mapIndex)
	{
		return SECTREE_MANAGER::instance().GetMap(mapIndex) != NULL;
	}

	// The map the operator sent this bot to, or 0: the module open, the map on
	// this core, and the Las only from the temple's level.
	long GetPlayerBotArezzoForcedMap(LPCHARACTER ch)
	{
		if (!ch || s_mapPlayerBotArezzoForced.empty())
			return 0;
		std::map<DWORD, long>::const_iterator it = s_mapPlayerBotArezzoForced.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotArezzoForced.end() || !IsPlayerBotArezzoOpen() || !IsPlayerBotArezzoHosted(it->second))
			return 0;
		// MT2009_PLUS_PROGRESSION_V2: not past the map's upper limit.
		if (ch->GetLevel() > GetPlayerBotArezzoMaxLevel(it->second))
			return 0;
		if (it->second == PLAYERBOT_MAP_AREZZO_FOREST &&
				(ch->GetLevel() < PLAYERBOT_AREZZO_FOREST_MIN_LEVEL || !IsPlayerBotArezzoHosted(PLAYERBOT_MAP_OCHAO) ||
				 !IsPlayerBotArezzoHosted(PLAYERBOT_MAP_ORC_VALLEY)))
			return 0;
		return it->second;
	}

	// Sent to one of the maps, wherever it stands now.
	bool IsPlayerBotArezzoBound(LPCHARACTER ch)
	{
		return GetPlayerBotArezzoForcedMap(ch) != 0;
	}

	// Sent to the Las and not told to leave: the temple's crossing in Orc
	// Valley takes it on to Straznik Swiatyni (playerbot_ochao_bots.h).
	bool IsPlayerBotArezzoBoundForLas(LPCHARACTER ch)
	{
		return ch && s_setPlayerBotArezzoLeave.count(ch->GetPlayerID()) == 0 &&
				GetPlayerBotArezzoForcedMap(ch) == PLAYERBOT_MAP_AREZZO_FOREST;
	}

	bool IsPlayerBotArezzoCohortPID(DWORD pid)
	{
		return s_mapPlayerBotArezzoCohort.find(pid) != s_mapPlayerBotArezzoCohort.end();
	}

	// In the Las and not yet PLAYERBOT_AREZZO_LAS_STAY_MS there.
	bool IsPlayerBotArezzoLasStaying(LPCHARACTER ch)
	{
		if (!ch || ch->GetMapIndex() != PLAYERBOT_MAP_AREZZO_FOREST)
			return false;
		std::map<DWORD, TPlayerBotArezzoTrack>::const_iterator t = s_mapPlayerBotArezzoTrack.find(ch->GetPlayerID());
		return t != s_mapPlayerBotArezzoTrack.end() && t->second.dwEntered != 0 &&
				t->second.lMap == PLAYERBOT_MAP_AREZZO_FOREST &&
				get_dword_time() - t->second.dwEntered < PLAYERBOT_AREZZO_LAS_STAY_MS;
	}

	bool IsPlayerBotArezzoLeaveOrdered(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		if (s_setPlayerBotArezzoLeave.count(ch->GetPlayerID()) != 0)
			return true;
		std::map<DWORD, TPlayerBotArezzoTrack>::const_iterator t = s_mapPlayerBotArezzoTrack.find(ch->GetPlayerID());
		return t != s_mapPlayerBotArezzoTrack.end() && t->second.dwVisitDeaths >= PLAYERBOT_AREZZO_VISIT_DEATHS_LEAVE &&
				IsPlayerBotArezzoMap(ch->GetMapIndex()) && !IsPlayerBotArezzoLasStaying(ch);
	}

	// Sent by the test and not told to leave: the bot stays on its map (and
	// on its road into the Las) whatever errand another pass has for it - a
	// shop's upkeep, the alchemist, Uriel, a horse, the river. Only what stops
	// the fight (no weapon, no armour, no potions, no arrows) takes it to town,
	// and it comes back by itself: the order stands.
	bool IsPlayerBotArezzoHeld(LPCHARACTER ch)
	{
		return GetPlayerBotArezzoForcedMap(ch) != 0 && !IsPlayerBotArezzoLeaveOrdered(ch);
	}

	// Held and standing on the ground the order is about: its map, or for the
	// Las the temple and Orc Valley on the way.
	bool IsPlayerBotArezzoHeldHere(LPCHARACTER ch)
	{
		if (!IsPlayerBotArezzoHeld(ch))
			return false;
		const long forced = GetPlayerBotArezzoForcedMap(ch);
		const long map = ch->GetMapIndex();
		return map == forced || (forced == PLAYERBOT_MAP_AREZZO_FOREST &&
				(map == PLAYERBOT_MAP_OCHAO || map == PLAYERBOT_MAP_ORC_VALLEY));
	}

	// What really stops a held bot: no weapon, no armour, no arrows for a bow,
	// or the red potions gone (under three). Not a full bag (it fights on
	// without picking up) and not a thin belt: round 3 sent 130 Las bots home
	// on "frontier_services" (BlocksPlayerBotTravel: fewer than ten reds, or no
	// free three-cell column) and each paid 18-60 minutes of temple to come
	// back. The belt is kept full on the map instead (RestockPlayerBotArezzo).
	bool IsPlayerBotArezzoTrulyBlocked(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return false;
		if (ch->GetWear(WEAR_WEAPON) == NULL || ch->GetWear(WEAR_BODY) == NULL)
			return true;
		// The Las: the belt and the arrows wait for the stay to run out.
		if (IsPlayerBotArezzoLasStaying(ch))
			return false;
		size_t red = 0, blue = 0;
		CountPlayerBotPotions(ch, red, blue);
		return NeedsPlayerBotArrows(ch) || red < 3;
	}

	bool IsPlayerBotArezzoLeaving(LPCHARACTER ch)
	{
		return ch && (s_mapPlayerBotArezzoExit.count(ch->GetPlayerID()) != 0 ||
				s_mapPlayerBotArezzoPending.count(ch->GetPlayerID()) != 0 ||
				IsPlayerBotArezzoLeaveOrdered(ch));
	}

	// The frontier for a bot the operator sent (GetPlayerBotFrontierMapForLevel).
	long GetPlayerBotArezzoFrontier(LPCHARACTER ch)
	{
		if (!ch || s_setPlayerBotArezzoLeave.count(ch->GetPlayerID()) != 0)
			return 0;
		return GetPlayerBotArezzoForcedMap(ch);
	}

	const TPlayerBotHuntingHub* GetPlayerBotArezzoHubs(long mapIndex, size_t& count)
	{
		const TPlayerBotArezzoMap* info = GetPlayerBotArezzoMapInfo(mapIndex);
		count = info ? (size_t)info->iHubs : 0;
		return info ? info->pHubs : NULL;
	}

	// ------------------------------------------------------------ the walk between spots

	// The spot a point belongs to: the nearest one it can see (of the four
	// nearest), or the nearest.
	int GetPlayerBotArezzoSpotOf(const TPlayerBotArezzoMap& info, long x, long y, bool needSight)
	{
		std::vector<std::pair<int, int> > order;
		order.reserve(info.iHubs);
		for (int i = 0; i < info.iHubs; ++i)
			order.push_back(std::make_pair(DISTANCE_APPROX(x - info.pHubs[i].x, y - info.pHubs[i].y), i));
		std::sort(order.begin(), order.end());
		if (order.empty())
			return -1;
		if (!needSight)
			return order[0].second;
		CPlayerBotNavigation& nav = CPlayerBotNavigation::instance(info.lMap);
		if (nav.Init(info.lMap))
			for (size_t k = 0; k < order.size() && k < 4; ++k)
				if (nav.SegmentClearWorld(x, y, info.pHubs[order[k].second].x, info.pHubs[order[k].second].y))
					return order[k].second;
		return order[0].second;
	}

	// How far a bot walks to a point of its Arezzo map: to the spot it stands
	// by, the ground between the two spots, and on. The spot choice
	// (ChoosePlayerBotHuntingHub) and the boss raid's recruiting ask it.
	int GetPlayerBotArezzoWalk(LPCHARACTER ch, long x, long y)
	{
		const TPlayerBotArezzoMap* info = ch ? GetPlayerBotArezzoMapInfo(ch->GetMapIndex()) : NULL;
		if (!info || info->iHubs == 0)
			return ch ? DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) : 0;
		static DWORD s_dwPid = 0, s_dwStamp = 0;
		static long s_lMap = 0;
		static int s_iFrom = -1;
		const DWORD dwNow = get_dword_time();
		if (s_dwPid != ch->GetPlayerID() || s_lMap != info->lMap || dwNow - s_dwStamp > 2000 || s_iFrom < 0)
		{
			s_dwPid = ch->GetPlayerID();
			s_lMap = info->lMap;
			s_dwStamp = dwNow;
			s_iFrom = GetPlayerBotArezzoSpotOf(*info, ch->GetX(), ch->GetY(), true);
		}
		const int to = GetPlayerBotArezzoSpotOf(*info, x, y, false);
		if (s_iFrom < 0 || to < 0)
			return DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y);
		const int straight = DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y);
		const int walked = DISTANCE_APPROX(ch->GetX() - info->pHubs[s_iFrom].x, ch->GetY() - info->pHubs[s_iFrom].y) +
				info->pWalk[s_iFrom * info->iHubs + to] +
				DISTANCE_APPROX(x - info->pHubs[to].x, y - info->pHubs[to].y);
		// Two points by one spot: the straight line is the walk.
		return std::max(straight, s_iFrom == to ? straight : walked);
	}

	// ------------------------------------------------------------ the walk by the corners

	struct TPlayerBotArezzoWalk
	{
		long lMap;
		long lGoalX, lGoalY;
		std::vector<short> path;
		size_t idx;
		DWORD dwLastUsed;
		DWORD dwLegSince;
		int iLegBest;
		long lLegX, lLegY;
		BYTE bStalls;
		bool bNavOnly;
		TPlayerBotArezzoWalk() : lMap(0), lGoalX(0), lGoalY(0), idx(0), dwLastUsed(0), dwLegSince(0), iLegBest(0),
				lLegX(0), lLegY(0), bStalls(0), bNavOnly(false) {}
	};
	std::map<DWORD, TPlayerBotArezzoWalk> s_mapPlayerBotArezzoWalk;

	void ForgetPlayerBotArezzoWalk(DWORD pid)
	{
		s_mapPlayerBotArezzoWalk.erase(pid);
	}

	// The nearest corner in sight of a point, or the nearest corner.
	int GetPlayerBotArezzoNodeInSight(const TPlayerBotArezzoMap& info, CPlayerBotNavigation& nav, long x, long y)
	{
		std::vector<std::pair<int, int> > order;
		order.reserve(info.iNodes);
		for (int i = 0; i < info.iNodes; ++i)
			order.push_back(std::make_pair(DISTANCE_APPROX(x - info.pTree[i].x, y - info.pTree[i].y), i));
		std::sort(order.begin(), order.end());
		for (size_t k = 0; k < order.size() && (int)k < PLAYERBOT_AREZZO_NODE_LOOKS &&
				order[k].first <= PLAYERBOT_AREZZO_NODE_SIGHT; ++k)
			if (nav.SegmentClearWorld(x, y, info.pTree[order[k].second].x, info.pTree[order[k].second].y))
				return order[k].second;
		return order.empty() ? -1 : order[0].second;
	}

	// The corners from one point to another through the tree: up from the
	// corner the start sees to the first corner the goal's branch shares, and
	// down that branch to the corner the goal is seen from.
	bool PlanPlayerBotArezzoWalk(const TPlayerBotArezzoMap& info, CPlayerBotNavigation& nav,
			long fromX, long fromY, long toX, long toY, std::vector<short>& path)
	{
		path.clear();
		const int a = GetPlayerBotArezzoNodeInSight(info, nav, fromX, fromY);
		const int b = GetPlayerBotArezzoNodeInSight(info, nav, toX, toY);
		if (a < 0 || b < 0)
			return false;
		std::vector<short> up, down;
		std::vector<char> onUp(info.iNodes, 0);
		for (int n = a, guard = 0; n >= 0 && guard <= info.iNodes; n = info.pTree[n].next, ++guard)
		{
			up.push_back((short)n);
			onUp[n] = 1;
		}
		int meet = -1;
		for (int n = b, guard = 0; n >= 0 && guard <= info.iNodes; n = info.pTree[n].next, ++guard)
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

	// A step along a straight open line is at most this long: a goal 15-30 km
	// down the line was one far plan a call, and the core's budget of eighty a
	// minute refused them in turn (round 3: 42 leg stalls on 360, twenty of
	// them at one corner 16 km off).
	const int PLAYERBOT_AREZZO_STEP = 3000;
	bool StepPlayerBotArezzoToward(LPCHARACTER ch, CPlayerBotNavigation& nav, long x, long y, DWORD dwNow, bool horse)
	{
		const long dx = x - ch->GetX(), dy = y - ch->GetY();
		const double d = sqrt((double)dx * dx + (double)dy * dy);
		if (d > PLAYERBOT_AREZZO_STEP + 500 && nav.SegmentClearWorld(ch->GetX(), ch->GetY(), x, y))
		{
			const long px = ch->GetX() + (long)(dx * PLAYERBOT_AREZZO_STEP / d);
			const long py = ch->GetY() + (long)(dy * PLAYERBOT_AREZZO_STEP / d);
			return MovePlayerBot(ch, px, py, dwNow, 8, true, horse, false, true);
		}
		return MovePlayerBot(ch, x, y, dwNow, 8, true, horse, false, true);
	}

	// One step of the walk to (x, y) on the bot's Arezzo map; false when the
	// bot is not on one. The goal in sight, or near: straight on the
	// navigation. Otherwise leg by leg along the corners.
	bool WalkPlayerBotInArezzo(LPCHARACTER ch, TPlayerBotAIState& state, long x, long y, DWORD dwNow)
	{
		const TPlayerBotArezzoMap* info = ch ? GetPlayerBotArezzoMapInfo(ch->GetMapIndex()) : NULL;
		if (!info)
			return false;
		CPlayerBotNavigation& nav = CPlayerBotNavigation::instance(info->lMap);
		if (!nav.Init(info->lMap))
			return false;
		const DWORD pid = ch->GetPlayerID();
		const int toGoal = DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y);
		TPlayerBotArezzoWalk& w = s_mapPlayerBotArezzoWalk[pid];
		if (w.lMap != info->lMap || (w.bNavOnly && (dwNow - w.dwLastUsed > 60000 ||
				DISTANCE_APPROX(x - w.lGoalX, y - w.lGoalY) > PLAYERBOT_AREZZO_WALK_REPLAN)))
		{
			w = TPlayerBotArezzoWalk();
			w.lMap = info->lMap;
		}
		if (w.bNavOnly || toGoal <= PLAYERBOT_AREZZO_TREE_WALK_MIN ||
				(toGoal <= PLAYERBOT_AREZZO_NODE_SIGHT && nav.SegmentClearWorld(ch->GetX(), ch->GetY(), x, y)))
		{
			w.dwLastUsed = dwNow;
			w.path.clear();
			StepPlayerBotArezzoToward(ch, nav, x, y, dwNow, toGoal > 4000);
			return true;
		}
		if (w.path.empty() || w.idx >= w.path.size() || dwNow - w.dwLastUsed > 60000 ||
				DISTANCE_APPROX(x - w.lGoalX, y - w.lGoalY) > PLAYERBOT_AREZZO_WALK_REPLAN)
		{
			const BYTE stalls = (dwNow - w.dwLastUsed > 60000) ? 0 : w.bStalls;
			w = TPlayerBotArezzoWalk();
			w.lMap = info->lMap;
			w.bStalls = stalls;
			w.lGoalX = x;
			w.lGoalY = y;
			if (!PlanPlayerBotArezzoWalk(*info, nav, ch->GetX(), ch->GetY(), x, y, w.path))
			{
				w.bNavOnly = true;
				++s_uPlayerBotArezzoWalksNavOnly;
				w.dwLastUsed = dwNow;
				MovePlayerBot(ch, x, y, dwNow, 8, true, true, false, true);
				return true;
			}
			++s_uPlayerBotArezzoWalksPlanned;
			ClearPlayerBotRoute(state, true);
		}
		w.dwLastUsed = dwNow;
		while (w.idx < w.path.size())
		{
			const TPlayerBotArezzoNode& n = info->pTree[w.path[w.idx]];
			if (DISTANCE_APPROX(ch->GetX() - n.x, ch->GetY() - n.y) <= PLAYERBOT_AREZZO_NODE_REACHED)
			{
				++w.idx;
				w.dwLegSince = 0;
				continue;
			}
			if (w.idx + 1 < w.path.size())
			{
				const TPlayerBotArezzoNode& m = info->pTree[w.path[w.idx + 1]];
				if (DISTANCE_APPROX(ch->GetX() - m.x, ch->GetY() - m.y) <= PLAYERBOT_AREZZO_NODE_SIGHT &&
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
			StepPlayerBotArezzoToward(ch, nav, x, y, dwNow, toGoal > 4000);
			return true;
		}
		const long nodeX = info->pTree[w.path[w.idx]].x;
		const long nodeY = info->pTree[w.path[w.idx]].y;
		const int distance = DISTANCE_APPROX(ch->GetX() - nodeX, ch->GetY() - nodeY);
		if (w.dwLegSince == 0 || distance + PLAYERBOT_AREZZO_LEG_PROGRESS <= w.iLegBest ||
				DISTANCE_APPROX(ch->GetX() - w.lLegX, ch->GetY() - w.lLegY) >= PLAYERBOT_AREZZO_LEG_PROGRESS * 2)
		{
			w.dwLegSince = dwNow;
			w.iLegBest = distance;
			w.lLegX = ch->GetX();
			w.lLegY = ch->GetY();
		}
		else if (dwNow - w.dwLegSince >= PLAYERBOT_AREZZO_LEG_TIMEOUT)
		{
			++w.bStalls;
			sys_log(0, "ARZ_BOT: walk leg stalled pid=%u name=%s map=%ld node=%d pos=(%ld,%ld) node_pos=(%ld,%ld) goal=(%ld,%ld) stalls=%u",
					pid, ch->GetName(), info->lMap, (int)w.path[w.idx], ch->GetX(), ch->GetY(), nodeX, nodeY, x, y,
					(unsigned int)w.bStalls);
			ClearPlayerBotRoute(state, true);
			if (w.bStalls >= PLAYERBOT_AREZZO_MAX_STALLS)
			{
				w.bNavOnly = true;
				++s_uPlayerBotArezzoWalksNavOnly;
			}
			else
				w.path.clear();
			return true;
		}
		StepPlayerBotArezzoToward(ch, nav, nodeX, nodeY, dwNow, true);
		return true;
	}

	// ------------------------------------------------------------ the way out

	// To the map's Teleporter by the corners and through it to destMap: the
	// frontier's own decision to leave (ManagePlayerBotWorldTravel) and
	// another pass's warp walked out.
	bool MovePlayerBotOutOfArezzo(LPCHARACTER ch, TPlayerBotAIState& state,
			long destMap, long destX, long destY, DWORD dwNow, const char* reason)
	{
		const TPlayerBotArezzoMap* info = ch ? GetPlayerBotArezzoMapInfo(ch->GetMapIndex()) : NULL;
		if (!info)
			return false;
		const DWORD pid = ch->GetPlayerID();
		std::map<DWORD, TPlayerBotArezzoExit>::iterator it = s_mapPlayerBotArezzoExit.find(pid);
		if (it == s_mapPlayerBotArezzoExit.end())
		{
			it = s_mapPlayerBotArezzoExit.insert(std::make_pair(pid, TPlayerBotArezzoExit())).first;
			it->second.dwStarted = dwNow;
			strlcpy(it->second.szReason, reason ? reason : "?", sizeof(it->second.szReason));
			sys_log(0, "ARZ_BOT: leaving pid=%u name=%s map=%ld reason=%s pos=(%ld,%ld) to=%ld ordered=%d walk_to_teleporter=%d",
					pid, ch->GetName(), info->lMap, reason ? reason : "?", ch->GetX(), ch->GetY(), destMap,
					IsPlayerBotArezzoLeaveOrdered(ch) ? 1 : 0, GetPlayerBotArezzoWalk(ch, info->lExitX, info->lExitY));
		}
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		const int distance = DISTANCE_APPROX(ch->GetX() - info->lExitX, ch->GetY() - info->lExitY);
		if (distance > PLAYERBOT_AREZZO_LAS_PORTAL_NEAR)
			return WalkPlayerBotInArezzo(ch, state, info->lExitX, info->lExitY, dwNow);
		return MovePlayerBotToWorldPortal(ch, state, info->lExitX, info->lExitY, destMap, destX, destY, dwNow, reason);
	}

	// From the top of the bot's tick: a warp another pass asked for while the
	// bot stood on the map is walked out first (the fight and the recovery come
	// before it).
	bool ManagePlayerBotArezzoPendingExit(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (s_mapPlayerBotArezzoPending.empty() || !ch)
			return false;
		std::map<DWORD, TPlayerBotArezzoPending>::iterator it = s_mapPlayerBotArezzoPending.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotArezzoPending.end())
			return false;
		if (!IsPlayerBotArezzoMap(ch->GetMapIndex()))
		{
			s_mapPlayerBotArezzoPending.erase(it);
			return false;
		}
		if (ch->IsDead() || state.bRecoveringAfterDeath || state.bTacticalRetreat ||
				(ch->GetMaxHP() > 0 && ch->GetHP() * 100 < ch->GetMaxHP() * 40))
			return false;
		LPCHARACTER victim = ch->GetVictim();
		if (victim && !victim->IsDead() && victim->GetVictim() == ch &&
				DISTANCE_APPROX(ch->GetX() - victim->GetX(), ch->GetY() - victim->GetY()) < 600)
			return false;
		const TPlayerBotArezzoPending p = it->second;
		return MovePlayerBotOutOfArezzo(ch, state, p.lMap, p.lX, p.lY, dwNow, p.szReason);
	}

	// ------------------------------------------------------------ the gate (TransitionPlayerBotMap)

	void NotePlayerBotArezzoEntered(LPCHARACTER ch, long targetMap, DWORD dwNow, const char* reason)
	{
		const DWORD pid = ch->GetPlayerID();
		TPlayerBotArezzoTrack& t = s_mapPlayerBotArezzoTrack[pid];
		std::map<DWORD, TPlayerBotArezzoLas>::const_iterator las = s_mapPlayerBotArezzoLas.find(pid);
		const DWORD since = las != s_mapPlayerBotArezzoLas.end() ? las->second.dwSince : t.dwSent;
		sys_log(0, "ARZ_BOT: entered pid=%u name=%s map=%ld level=%u job=%u empire=%u reason=%s travel_s=%u",
				pid, ch->GetName(), targetMap, (unsigned int)ch->GetLevel(), (unsigned int)(ch->GetJob() % 4),
				(unsigned int)ch->GetEmpire(), reason ? reason : "?", since ? (dwNow - since) / 1000 : 0);
		t.dwEntered = dwNow;
		t.lMap = targetMap;
		t.dwVisitDeaths = 0;
		t.adwDeathAt[0] = t.adwDeathAt[1] = 0;
		t.dwAnchorSince = dwNow;
		t.bStuck = false;
		s_mapPlayerBotArezzoLas.erase(pid);
	}

	// Called at the top of TransitionPlayerBotMap, before the temple's own
	// gate. -1: not ours, go on; 0/1: the answer.
	int RoutePlayerBotArezzoTransition(LPCHARACTER ch, TPlayerBotAIState& state,
			long targetMap, long targetX, long targetY, DWORD dwNow, const char* reason)
	{
		if (!ch)
			return -1;
		const DWORD pid = ch->GetPlayerID();
		const long fromMap = ch->GetMapIndex();
		const long targetBase = targetMap >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN ? targetMap / 10000 : targetMap;
		// Off one of the maps: from its Teleporter, with the ring, when the
		// module closes, or after a walk out that did not get there.
		if (IsPlayerBotArezzoMap(fromMap) && targetMap != fromMap)
		{
			const TPlayerBotArezzoMap* info = GetPlayerBotArezzoMapInfo(fromMap);
			const bool atTeleporter = info && DISTANCE_APPROX(ch->GetX() - info->lExitX,
					ch->GetY() - info->lExitY) <= PLAYERBOT_AREZZO_WARP_REACH;
			std::map<DWORD, TPlayerBotArezzoPending>::iterator pend = s_mapPlayerBotArezzoPending.find(pid);
			std::map<DWORD, TPlayerBotArezzoTrack>::iterator t = s_mapPlayerBotArezzoTrack.find(pid);
			const bool gaveUp = pend != s_mapPlayerBotArezzoPending.end() &&
					(dwNow - pend->second.dwSince >= PLAYERBOT_AREZZO_PENDING_MAX_MS ||
					 (t != s_mapPlayerBotArezzoTrack.end() && t->second.dwVisitDeaths >= PLAYERBOT_AREZZO_VISIT_DEATHS_LEAVE));
			const bool ring = s_bPlayerBotArezzoRingWarp || (reason && strstr(reason, "ring") != NULL);
			const bool closed = !IsPlayerBotArezzoOpen();
			// A held bot goes nowhere on another pass's errand.
			if (!closed && IsPlayerBotArezzoHeldHere(ch) && !IsPlayerBotArezzoTrulyBlocked(ch) &&
					!(reason && strncmp(reason, "arezzo_", 7) == 0))
			{
				char tag[48];
				snprintf(tag, sizeof(tag), "arezzo_errand:%s", reason ? reason : "?");
				PlayerBotLogThrottled(tag, dwNow,
						"ARZ_BOT: errand refused pid=%u name=%s map=%ld to=%ld reason=%s (held by the test)",
						pid, ch->GetName(), fromMap, targetMap, reason ? reason : "?");
				s_mapPlayerBotArezzoPending.erase(pid);
				return 0;
			}
			if (!atTeleporter && !gaveUp && !ring && !closed)
			{
				if (pend == s_mapPlayerBotArezzoPending.end())
				{
					TPlayerBotArezzoPending& p = s_mapPlayerBotArezzoPending[pid];
					p.lMap = targetMap; p.lX = targetX; p.lY = targetY; p.dwSince = dwNow;
					strlcpy(p.szReason, reason ? reason : "?", sizeof(p.szReason));
					sys_log(0, "ARZ_BOT: walks out first pid=%u name=%s map=%ld to=%ld reason=%s pos=(%ld,%ld)",
							pid, ch->GetName(), fromMap, targetMap, reason ? reason : "?", ch->GetX(), ch->GetY());
				}
				return 1;
			}
			std::map<DWORD, TPlayerBotArezzoExit>::iterator e = s_mapPlayerBotArezzoExit.find(pid);
			sys_log(0, "ARZ_BOT: left pid=%u name=%s map=%ld to=%ld reason=%s via=%s pos=(%ld,%ld) stay_s=%u exit_walk_s=%u kills=%u exp=%llu deaths=%u",
					pid, ch->GetName(), fromMap, targetMap, reason ? reason : "?",
					closed ? "closed" : (ring ? "ring" : (atTeleporter ? "teleporter" : "gave_up")),
					ch->GetX(), ch->GetY(),
					t != s_mapPlayerBotArezzoTrack.end() && t->second.dwEntered ? (dwNow - t->second.dwEntered) / 1000 : 0,
					e != s_mapPlayerBotArezzoExit.end() ? (dwNow - e->second.dwStarted) / 1000 : 0,
					t != s_mapPlayerBotArezzoTrack.end() ? t->second.dwKills : 0,
					t != s_mapPlayerBotArezzoTrack.end() ? t->second.ullExp : 0ULL,
					t != s_mapPlayerBotArezzoTrack.end() ? t->second.dwDeaths : 0);
			s_mapPlayerBotArezzoPending.erase(pid);
			s_mapPlayerBotArezzoExit.erase(pid);
			s_setPlayerBotArezzoLeave.erase(pid);
			ForgetPlayerBotArezzoWalk(pid);
			if (t != s_mapPlayerBotArezzoTrack.end())
			{
				t->second.dwEntered = 0;
				t->second.lMap = 0;
				t->second.dwSent = dwNow;	// the next way in is timed from here
			}
			// On to the gate below when the target is another place of the list.
		}
		// The Las-bound bot in the temple or in Orc Valley: the same, except
		// for the road itself (the temple's gate, its Portal, the valley).
		if ((fromMap == PLAYERBOT_MAP_OCHAO || fromMap == PLAYERBOT_MAP_ORC_VALLEY) && targetMap != fromMap &&
				targetMap != PLAYERBOT_MAP_OCHAO && targetMap != PLAYERBOT_MAP_AREZZO_FOREST &&
				targetMap != PLAYERBOT_MAP_ORC_VALLEY && IsPlayerBotArezzoOpen() &&
				IsPlayerBotArezzoHeldHere(ch) && !IsPlayerBotArezzoTrulyBlocked(ch) &&
				!(reason && (strncmp(reason, "arezzo_", 7) == 0 || strncmp(reason, "ochao_", 6) == 0)))
		{
			char tag[48];
			snprintf(tag, sizeof(tag), "arezzo_errand:%s", reason ? reason : "?");
			PlayerBotLogThrottled(tag, dwNow,
					"ARZ_BOT: errand refused pid=%u name=%s map=%ld to=%ld reason=%s (on the road to the Las)",
					pid, ch->GetName(), fromMap, targetMap, reason ? reason : "?");
			return 0;
		}
		if (!IsPlayerBotOffLimitsMap(targetMap) || targetBase == fromMap ||
				(fromMap >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN && fromMap / 10000 == targetBase))
			return -1;
		// Into the new places: the dungeons and the Blue Dragon never; the maps
		// only for a bot the operator sent there, while the module is open.
		const long forced = GetPlayerBotArezzoForcedMap(ch);
		const char* why = NULL;
		if (!IsPlayerBotArezzoMap(targetBase))
			why = "dungeon";
		else if (!IsPlayerBotArezzoOpen())
			why = "module_closed";
		else if (forced != targetBase)
			why = "not_sent";
		if (why)
		{
			++s_uPlayerBotArezzoRefused;
			char tag[48];
			snprintf(tag, sizeof(tag), "arezzo_refused:%ld:%s", targetBase, why);
			PlayerBotLogThrottled(tag, dwNow,
					"ARZ_BOT: refused pid=%u name=%s from=%ld to=%ld why=%s reason=%s level=%u refused_total=%u",
					pid, ch->GetName(), fromMap, targetMap, why, reason ? reason : "?",
					(unsigned int)ch->GetLevel(), s_uPlayerBotArezzoRefused);
			return 0;
		}
		if (targetBase == PLAYERBOT_MAP_AREZZO_FOREST)
		{
			// Only by the Guardian's Portal, from the temple.
			if (fromMap == PLAYERBOT_MAP_OCHAO && reason && strcmp(reason, "arezzo_las_portal") == 0)
			{
				NotePlayerBotArezzoEntered(ch, targetBase, dwNow, reason);
				return -1;
			}
			std::map<DWORD, TPlayerBotArezzoLas>::iterator las = s_mapPlayerBotArezzoLas.find(pid);
			if (las == s_mapPlayerBotArezzoLas.end())
			{
				TPlayerBotArezzoLas& l = s_mapPlayerBotArezzoLas[pid];
				l.dwSince = dwNow;
				l.bPhase = fromMap == PLAYERBOT_MAP_OCHAO ? AREZZO_LAS_TEMPLE : AREZZO_LAS_ROAD;
				l.dwPhaseSince = dwNow;
				sys_log(0, "ARZ_BOT: las crossing begins pid=%u name=%s level=%u from=%ld reason=%s",
						pid, ch->GetName(), (unsigned int)ch->GetLevel(), fromMap, reason ? reason : "?");
			}
			// In the temple the walk to the Guardian is the travel pass's.
			if (fromMap == PLAYERBOT_MAP_OCHAO)
				return 1;
			return TransitionPlayerBotMap(ch, state, PLAYERBOT_MAP_OCHAO, PLAYERBOT_OCHAO_ARRIVAL_X,
					PLAYERBOT_OCHAO_ARRIVAL_Y, dwNow, "arezzo_las_via_ochao") ? 1 : 0;
		}
		NotePlayerBotArezzoEntered(ch, targetBase, dwNow, reason);
		return -1;
	}

	// ------------------------------------------------------------ the way into the Las

	// In the Temple of Ochao, from the travel pass, for a bot sent to the Las:
	// the Portal while it stands within reach, else the Guardian - walked to
	// by the labyrinth's corners and fought - else the temple's own hunting
	// while he is away (the travel pass does nothing more: 0).
	int ManagePlayerBotArezzoLasInTemple(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		const DWORD pid = ch->GetPlayerID();
		TPlayerBotArezzoLas& las = s_mapPlayerBotArezzoLas[pid];
		if (las.dwSince == 0)
		{
			las.dwSince = dwNow;
			sys_log(0, "ARZ_BOT: las crossing begins pid=%u name=%s level=%u from=%ld reason=in_temple",
					pid, ch->GetName(), (unsigned int)ch->GetLevel(), ch->GetMapIndex());
		}
		if (ch->IsDead() || state.bRecoveringAfterDeath)
			return 0;
		BYTE phase = AREZZO_LAS_TEMPLE;
		int result = 0;
		LPCHARACTER portal = FindPlayerBotOchaoPortal();
		LPCHARACTER guardian = mt2009_ochao::FindOnMap(mt2009_ochao::s_dwGuardianVID);
		// Set out for a Portal once, while it is within the walk, and keep to
		// it until it closes: the walk's estimate moves as the bot turns the
		// labyrinth's corners, and choosing again every tick turned it round.
		if (!portal)
			las.dwPortalVID = 0;
		else if (las.dwPortalVID != (DWORD)portal->GetVID() &&
				GetPlayerBotOchaoWalk(ch, portal->GetX(), portal->GetY()) <= PLAYERBOT_AREZZO_LAS_PORTAL_WALK_MAX)
		{
			las.dwPortalVID = (DWORD)portal->GetVID();
			sys_log(0, "ARZ_BOT: las takes the portal pid=%u name=%s pos=(%ld,%ld) portal=(%ld,%ld) distance=%d after_s=%u",
					pid, ch->GetName(), ch->GetX(), ch->GetY(), portal->GetX(), portal->GetY(),
					DISTANCE_APPROX(ch->GetX() - portal->GetX(), ch->GetY() - portal->GetY()), (dwNow - las.dwSince) / 1000);
			las.dwLastLog = dwNow;
		}
		if (portal && las.dwPortalVID == (DWORD)portal->GetVID())
		{
			phase = AREZZO_LAS_PORTAL;
			CPlayerBotNavigation& nav = CPlayerBotNavigation::instance(PLAYERBOT_MAP_OCHAO);
			const int d = DISTANCE_APPROX(ch->GetX() - portal->GetX(), ch->GetY() - portal->GetY());
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			if (d <= PLAYERBOT_AREZZO_LAS_PORTAL_USE)
			{
				// Beside it: the quest's first choice, "Zaczarowany Las".
				ch->Stop();
				if (TransitionPlayerBotMap(ch, state, PLAYERBOT_MAP_AREZZO_FOREST, PLAYERBOT_AREZZO_FOREST_ARRIVAL_X,
						PLAYERBOT_AREZZO_FOREST_ARRIVAL_Y, dwNow, "arezzo_las_portal"))
					return 1;
				PlayerBotLogThrottled("arezzo_las_portal_failed", dwNow,
						"ARZ_BOT: las portal refused pid=%u name=%s pos=(%ld,%ld) portal=(%ld,%ld) distance=%d",
						pid, ch->GetName(), ch->GetX(), ch->GetY(), portal->GetX(), portal->GetY(), d);
				result = 1;
			}
			else if (d <= PLAYERBOT_AREZZO_LAS_PORTAL_NEAR && nav.Init(PLAYERBOT_MAP_OCHAO) &&
					nav.SegmentClearWorld(ch->GetX(), ch->GetY(), portal->GetX(), portal->GetY()))
			{
				MovePlayerBot(ch, portal->GetX(), portal->GetY(), dwNow, 8, true, false, false, false);
				result = 1;
			}
			else
				result = WalkPlayerBotInOchao(ch, state, portal->GetX(), portal->GetY(), dwNow) ? 1 : 0;
		}
		else if (guardian && !guardian->IsDead())
		{
			phase = AREZZO_LAS_GUARDIAN;
			const int d = DISTANCE_APPROX(ch->GetX() - guardian->GetX(), ch->GetY() - guardian->GetY());
			if (d > PLAYERBOT_AREZZO_LAS_GUARDIAN_NEAR)
			{
				// What attacks it on the way is fought where it comes.
				LPCHARACTER victim = ch->GetVictim();
				if (victim && !victim->IsDead() && victim->GetVictim() == ch &&
						DISTANCE_APPROX(ch->GetX() - victim->GetX(), ch->GetY() - victim->GetY()) < 800)
					result = 0;
				else
					result = WalkPlayerBotInOchao(ch, state, guardian->GetX(), guardian->GetY(), dwNow) ? 1 : 0;
			}
			else
				result = FightPlayerBotTowerObjective(ch, state, guardian, dwNow) ? 1 : 0;
		}
		// The other phases once a minute at most for a bot (the portal's own
		// line is above, once a Portal).
		if (phase != las.bPhase)
		{
			if (phase != AREZZO_LAS_PORTAL && (las.dwLastLog == 0 || dwNow - las.dwLastLog >= 60000))
			{
				sys_log(0, "ARZ_BOT: las %s pid=%u name=%s pos=(%ld,%ld) after_s=%u",
						phase == AREZZO_LAS_GUARDIAN ? "goes for the guardian" : "waits in the temple",
						pid, ch->GetName(), ch->GetX(), ch->GetY(), (dwNow - las.dwSince) / 1000);
				las.dwLastLog = dwNow;
			}
			las.bPhase = phase;
			las.dwPhaseSince = dwNow;
		}
		return result;
	}

	// From ManagePlayerBotWorldTravel, before the temple's own crossing: the
	// module closing under a bot on one of the maps, and the Las-bound bot in
	// the temple. -1: nothing to do here; 0/1: the travel pass's answer.
	int ManagePlayerBotArezzoTravel(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return -1;
		const long mapIndex = ch->GetMapIndex();
		if (IsPlayerBotArezzoMap(mapIndex) && !IsPlayerBotArezzoOpen())
		{
			long destMap = 0, destX = 0, destY = 0;
			if (GetPlayerBotVillageReturn(ch, playerbot_empire_rules::MAP_ROLE_M2, destMap, destX, destY))
				return TransitionPlayerBotMap(ch, state, destMap, destX, destY, dwNow, "arezzo_closed") ? 1 : 0;
			return -1;
		}
		// Out of potions or a weapon: the frontier's own exit takes it out of
		// the temple (to town and back later), not on to the Guardian.
		if (mapIndex == PLAYERBOT_MAP_OCHAO && GetPlayerBotArezzoForcedMap(ch) == PLAYERBOT_MAP_AREZZO_FOREST &&
				s_setPlayerBotArezzoLeave.count(ch->GetPlayerID()) == 0 && !IsPlayerBotOchaoLeaving(ch) &&
				!IsPlayerBotArezzoTrulyBlocked(ch))
			return ManagePlayerBotArezzoLasInTemple(ch, state, dwNow);
		return -1;
	}

	// ------------------------------------------------------------ keeping alive

	// The Las (97-105) against bots of 95-97 took 57 of round 2's 93 deaths:
	// there, and on the road through the temple, a bot drinks from 85 % (the
	// Demon Tower's rule) and steps back from 50 %.
	bool IsPlayerBotArezzoHardGround(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		const long map = ch->GetMapIndex();
		return map == PLAYERBOT_MAP_AREZZO_FOREST ||
				(map == PLAYERBOT_MAP_OCHAO && s_mapPlayerBotArezzoLas.count(ch->GetPlayerID()) != 0);
	}
	int GetPlayerBotArezzoPotionPercent(LPCHARACTER ch)
	{
		return IsPlayerBotArezzoHardGround(ch) ? 85 : PLAYERBOT_POTION_HP_PERCENT;
	}
	int GetPlayerBotArezzoRetreatPercent(LPCHARACTER ch)
	{
		return IsPlayerBotArezzoHardGround(ch) ? 50 : PLAYERBOT_RETREAT_START_HP_PERCENT;
	}

	// ------------------------------------------------------------ the kills

	void NoteArezzoBotKill(LPCHARACTER killer, LPCHARACTER victim)
	{
		if (!killer || !victim || victim->IsPC() || !IsPlayerBotArezzoMap(killer->GetMapIndex()) ||
				s_mapPlayerBotAIStates.find(killer->GetPlayerID()) == s_mapPlayerBotAIStates.end())
			return;
		TPlayerBotArezzoTrack& t = s_mapPlayerBotArezzoTrack[killer->GetPlayerID()];
		++t.dwKills;
		const DWORD race = victim->GetRaceNum();
		if (IsPlayerBotArezzoBossRace(race))
		{
			++t.dwBossKills;
			sys_log(0, "ARZ_BOT: boss killed race=%u name=%s map=%ld killer=%u killer_name=%s pos=(%ld,%ld)",
					race, victim->GetName(), victim->GetMapIndex(), killer->GetPlayerID(), killer->GetName(),
					victim->GetX(), victim->GetY());
		}
		else if (victim->IsStone())
			sys_log(0, "ARZ_BOT: stone broken race=%u map=%ld killer=%u pos=(%ld,%ld)",
					race, victim->GetMapIndex(), killer->GetPlayerID(), victim->GetX(), victim->GetY());
	}

	// ------------------------------------------------------------ the test hook

	// The online bots a "send" may take: alive, not a person's company, not in
	// a dungeon, of the levels asked; the cohort file's bots for this map
	// first, then (for the Las) the ones already in the temple.
	std::vector<DWORD> CollectPlayerBotArezzoCandidates(long map, int levelMin, int levelMax, bool any)
	{
		// With test characters named for this map, only they are sent (unless
		// the line says "any"): the rest of the world keeps its own life.
		bool cohortOnly = false;
		for (std::map<DWORD, long>::const_iterator c = s_mapPlayerBotArezzoCohort.begin();
				!any && c != s_mapPlayerBotArezzoCohort.end() && !cohortOnly; ++c)
			cohortOnly = c->second == map;
		std::vector<std::pair<int, DWORD> > ranked;
		for (TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.begin(); it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch || !ch->IsPC() || ch->IsDead() || !ch->GetDesc() || !ch->GetDesc()->IsBot())
				continue;
			const long m = ch->GetMapIndex();
			if ((int)ch->GetLevel() < levelMin || (int)ch->GetLevel() > levelMax)
				continue;
			if (m >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN || (IsPlayerBotOffLimitsMap(m) && m != map) ||
					(ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty())) ||
					IsPlayerBotSidekickPID(it->first) || IsPlayerBotHeldForCompany(ch) ||
					IsPlayerBotOnMercContract(it->first))
				continue;
			std::map<DWORD, long>::const_iterator f = s_mapPlayerBotArezzoForced.find(it->first);
			if (f != s_mapPlayerBotArezzoForced.end() && f->second == map)
				continue;
			int rank = 3;
			std::map<DWORD, long>::const_iterator c = s_mapPlayerBotArezzoCohort.find(it->first);
			if (c != s_mapPlayerBotArezzoCohort.end() && c->second == map)
				rank = 0;
			else if (map == PLAYERBOT_MAP_AREZZO_FOREST && m == PLAYERBOT_MAP_OCHAO)
				rank = 1;
			else if (c != s_mapPlayerBotArezzoCohort.end())
				continue;	// another map's test character
			if (cohortOnly && rank != 0)
				continue;
			ranked.push_back(std::make_pair(rank, it->first));
		}
		std::sort(ranked.begin(), ranked.end());
		std::vector<DWORD> out;
		for (size_t i = 0; i < ranked.size(); ++i)
			out.push_back(ranked[i].second);
		return out;
	}

	void SavePlayerBotArezzoOrders()
	{
		if (s_mapPlayerBotArezzoForced.empty())
		{
			remove(PLAYERBOT_AREZZO_ORDERS_FILE);
			return;
		}
		FILE* fp = fopen(PLAYERBOT_AREZZO_ORDERS_FILE, "w");
		if (!fp)
			return;
		for (std::map<DWORD, long>::const_iterator f = s_mapPlayerBotArezzoForced.begin(); f != s_mapPlayerBotArezzoForced.end(); ++f)
			fprintf(fp, "%u %ld\n", f->first, f->second);
		fclose(fp);
	}

	void LoadPlayerBotArezzoOrders()
	{
		s_bPlayerBotArezzoOrdersLoaded = true;
		FILE* fp = fopen(PLAYERBOT_AREZZO_ORDERS_FILE, "r");
		if (!fp)
			return;
		char line[64];
		int n = 0;
		while (fgets(line, sizeof(line), fp))
		{
			unsigned int pid = 0;
			long map = 0;
			if (sscanf(line, "%u %ld", &pid, &map) == 2 && pid != 0 && IsPlayerBotArezzoMap(map))
			{
				s_mapPlayerBotArezzoForced[pid] = map;
				++n;
			}
		}
		fclose(fp);
		sys_log(0, "ARZ_BOT: orders restored from %s: %d bots sent before the restart", PLAYERBOT_AREZZO_ORDERS_FILE, n);
	}

	void LoadPlayerBotArezzoCohort(DWORD dwNow)
	{
		s_bPlayerBotArezzoCohortLoaded = true;
		FILE* fp = fopen(PLAYERBOT_AREZZO_COHORT_FILE, "r");
		if (!fp)
			return;
		char line[128];
		std::vector<DWORD> pids;
		int perMap[3] = { 0, 0, 0 };
		while (fgets(line, sizeof(line), fp))
		{
			long map = 0;
			unsigned int pid = 0;
			if (line[0] == '#' || sscanf(line, "%ld %u", &map, &pid) != 2 || !IsPlayerBotArezzoMap(map) || pid == 0)
				continue;
			s_mapPlayerBotArezzoCohort[pid] = map;
			pids.push_back(pid);
			++perMap[map - PLAYERBOT_MAP_AREZZO_CYCLOPS];
		}
		fclose(fp);
		const size_t scheduled = CPlayerBotManager::instance().ScheduleExtraBots(pids);
		sys_log(0, "ARZ_BOT: cohort file pids=%u m360=%d m361=%d m362=%d scheduled_now=%u",
				(unsigned int)pids.size(), perMap[0], perMap[1], perMap[2], (unsigned int)scheduled);
	}

	void RunPlayerBotArezzoTestFile(DWORD dwNow)
	{
		FILE* fp = fopen(PLAYERBOT_AREZZO_TEST_FILE, "r");
		if (!fp)
			return;
		char line[128];
		std::vector<std::string> lines;
		while (fgets(line, sizeof(line), fp))
			lines.push_back(line);
		fclose(fp);
		char done[128];
		snprintf(done, sizeof(done), "%s.done", PLAYERBOT_AREZZO_TEST_FILE);
		rename(PLAYERBOT_AREZZO_TEST_FILE, done);
		for (size_t l = 0; l < lines.size(); ++l)
		{
			char cmd[32] = "", opt[16] = "";
			long a = 0, b = 0;
			int lmin = -1, lmax = -1;
			const int got = sscanf(lines[l].c_str(), "%31s %ld %ld %d %d %15s", cmd, &a, &b, &lmin, &lmax, opt);
			if (got < 1 || cmd[0] == '#')
				continue;
			if (!strcmp(cmd, "send"))
			{
				const TPlayerBotArezzoMap* info = GetPlayerBotArezzoMapInfo(a);
				if (!info || got < 3 || b <= 0)
				{
					sys_log(0, "ARZ_BOT: test send refused line=%s", lines[l].c_str());
					continue;
				}
				if (!IsPlayerBotArezzoOpen() || !IsPlayerBotArezzoHosted(a))
				{
					sys_log(0, "ARZ_BOT: test send refused map=%ld open=%d hosted=%d", a,
							IsPlayerBotArezzoOpen() ? 1 : 0, IsPlayerBotArezzoHosted(a) ? 1 : 0);
					continue;
				}
				if (got < 5)
				{
					lmin = info->bMinLevel;
					lmax = a == PLAYERBOT_MAP_AREZZO_FOREST ? 255 : (int)info->bMinLevel + 30;
				}
				lmax = std::min(lmax, (int)GetPlayerBotArezzoMaxLevel(a));
				if (a == PLAYERBOT_MAP_AREZZO_FOREST && lmin < (int)PLAYERBOT_AREZZO_FOREST_MIN_LEVEL)
					lmin = PLAYERBOT_AREZZO_FOREST_MIN_LEVEL;
				std::vector<DWORD> pids = CollectPlayerBotArezzoCandidates(a, lmin, lmax, !strcmp(opt, "any"));
				int sent = 0, failed = 0, already = 0;
				for (size_t i = 0; i < pids.size() && sent + already < b; ++i)
				{
					LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pids[i]);
					TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(pids[i]);
					if (!ch || st == s_mapPlayerBotAIStates.end())
						continue;
					s_mapPlayerBotArezzoForced[pids[i]] = a;
					s_setPlayerBotArezzoLeave.erase(pids[i]);
					s_setPlayerBotOchaoForced.erase(pids[i]);
					s_setPlayerBotOchaoLeave.erase(pids[i]);
					s_mapPlayerBotArezzoTrack[pids[i]].dwSent = dwNow;
					if (ch->GetMapIndex() == a || (a == PLAYERBOT_MAP_AREZZO_FOREST && ch->GetMapIndex() == PLAYERBOT_MAP_OCHAO))
					{
						if (ch->GetMapIndex() == PLAYERBOT_MAP_OCHAO)
						{
							TPlayerBotArezzoLas& las = s_mapPlayerBotArezzoLas[pids[i]];
							las = TPlayerBotArezzoLas();
							las.dwSince = dwNow;
							las.bPhase = AREZZO_LAS_TEMPLE;
							las.dwPhaseSince = dwNow;
							sys_log(0, "ARZ_BOT: las crossing begins pid=%u name=%s level=%u from=%ld reason=test_send",
									pids[i], ch->GetName(), (unsigned int)ch->GetLevel(), ch->GetMapIndex());
						}
						++already;
						continue;
					}
					// A warp other passes asked for, and a raid, are dropped: the
					// test's send comes first.
					st->second.wBossRaidRace = 0;
					st->second.lBossRaidMap = 0;
					if (TransitionPlayerBotMap(ch, st->second, a, info->lArrivalX, info->lArrivalY, dwNow, "arezzo_test_send"))
						++sent;
					else
						++failed;
				}
				sys_log(0, "ARZ_BOT: test send map=%ld asked=%ld levels=%d-%d any=%d sent=%d already_there=%d failed=%d candidates=%u forced_total=%u",
						a, b, lmin, lmax, !strcmp(opt, "any") ? 1 : 0, sent, already, failed, (unsigned int)pids.size(),
						(unsigned int)s_mapPlayerBotArezzoForced.size());
				SavePlayerBotArezzoOrders();
			}
			else if (!strcmp(cmd, "leave") || !strcmp(cmd, "leaveall"))
			{
				const bool all = !strcmp(cmd, "leaveall");
				int ordered = 0, released = 0;
				for (std::map<DWORD, long>::iterator f = s_mapPlayerBotArezzoForced.begin();
						f != s_mapPlayerBotArezzoForced.end() && (all || ordered + released < b);)
				{
					if (!all && f->second != a)
					{
						++f;
						continue;
					}
					LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(f->first);
					if (ch && IsPlayerBotArezzoMap(ch->GetMapIndex()))
					{
						s_setPlayerBotArezzoLeave.insert(f->first);
						++ordered;
					}
					else
						++released;	// still on its way: it simply is no longer sent
					s_mapPlayerBotArezzoLas.erase(f->first);
					s_mapPlayerBotArezzoForced.erase(f++);
				}
				sys_log(0, "ARZ_BOT: test %s map=%ld ordered=%d released=%d forced_left=%u",
						cmd, all ? 0L : a, ordered, released, (unsigned int)s_mapPlayerBotArezzoForced.size());
				SavePlayerBotArezzoOrders();
			}
			else if (!strcmp(cmd, "cohort"))
				LoadPlayerBotArezzoCohort(dwNow);
			else if (!strcmp(cmd, "spawn"))
			{
				LPCHARACTER boss = NULL;
				for (size_t i = 0; i < sizeof(PLAYERBOT_AREZZO_BOSSES) / sizeof(PLAYERBOT_AREZZO_BOSSES[0]); ++i)
				{
					const TPlayerBotArezzoBoss& row = PLAYERBOT_AREZZO_BOSSES[i];
					LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(row.lMap);
					if (row.dwRace != (DWORD)a || !map)
						continue;
					boss = CHARACTER_MANAGER::instance().SpawnMob(row.dwRace, row.lMap,
							map->m_setting.iBaseX + row.lCellX * 100, map->m_setting.iBaseY + row.lCellY * 100, 0, true, -1, true);
					break;
				}
				sys_log(0, "ARZ_BOT: test spawn race=%ld vid=%u", a, boss ? (unsigned int)boss->GetVID() : 0U);
			}
			else if (!strcmp(cmd, "reset"))
			{
				s_mapPlayerBotArezzoForced.clear();
				s_setPlayerBotArezzoLeave.clear();
				s_mapPlayerBotArezzoLas.clear();
				SavePlayerBotArezzoOrders();
				sys_log(0, "ARZ_BOT: test reset");
			}
			else
				sys_log(0, "ARZ_BOT: test unknown line=%s", lines[l].c_str());
		}
	}

	// ------------------------------------------------------------ the watch

	// A bot the order still sends that is off its map - home for potions or a
	// repair, logged in elsewhere after a restart - goes back by itself: after
	// PLAYERBOT_AREZZO_RESEND_AFTER_MS away, when it is not shopping, not
	// fighting, not short of what the fight needs, and not a person's company.
	const DWORD PLAYERBOT_AREZZO_RESEND_AFTER_MS = 90000;
	const DWORD PLAYERBOT_AREZZO_RESEND_RETRY_MS = 30000;
	void ResendPlayerBotArezzo(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotArezzoTrack& t, long map, DWORD dwNow)
	{
		const TPlayerBotArezzoMap* info = GetPlayerBotArezzoMapInfo(map);
		if (!info || !IsPlayerBotArezzoHeld(ch) || GetPlayerBotArezzoForcedMap(ch) != map)
			return;
		const long here = ch->GetMapIndex();
		if (t.dwAwaySince == 0)
			t.dwAwaySince = dwNow;
		if (dwNow - t.dwAwaySince < PLAYERBOT_AREZZO_RESEND_AFTER_MS || dwNow < t.dwNextResend)
			return;
		t.dwNextResend = dwNow + PLAYERBOT_AREZZO_RESEND_RETRY_MS;
		if (ch->IsDead() || state.bRecoveringAfterDeath || here >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN ||
				(map == PLAYERBOT_MAP_AREZZO_FOREST && (here == PLAYERBOT_MAP_OCHAO || here == PLAYERBOT_MAP_ORC_VALLEY)) ||
				(ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty())) || IsPlayerBotHeldForCompany(ch) ||
				state.bTownVisitPhase != BOT_TOWN_PHASE_NONE || ch->GetMyShop() != NULL ||
				state.wBossRaidRace != 0 || BlocksPlayerBotTravel(ch) ||
				(ch->GetVictim() != NULL && !ch->GetVictim()->IsDead()))
			return;
		const bool moved = TransitionPlayerBotMap(ch, state, map, info->lArrivalX, info->lArrivalY, dwNow, "arezzo_resend");
		sys_log(0, "ARZ_BOT: re-sent pid=%u name=%s map=%ld from=%ld away_s=%u ok=%d",
				ch->GetPlayerID(), ch->GetName(), map, here, (dwNow - t.dwAwaySince) / 1000, moved ? 1 : 0);
		if (moved)
			t.dwSent = dwNow;
	}

	// A held bot's belt, kept on its map at the merchant's price out of its own
	// gold (the owner's "simply have them carry enough potions", round 3): the
	// trip home for a belt would cost a Las bot the whole temple again. Under
	// 60 reds it buys a stack of 200 (the big ones from level 40, 40 yang each),
	// a caster under 40 blues a stack of 200; only with a free cell, never more
	// than half its gold.
	void RestockPlayerBotArezzo(LPCHARACTER ch, TPlayerBotArezzoTrack& t, DWORD dwNow)
	{
		// Bound for the Las and not in it yet (town, Orc Valley, the temple):
		// it packs the expedition's kit before and on the road.
		const bool packing = IsPlayerBotArezzoBoundForLas(ch) && ch->GetMapIndex() != PLAYERBOT_MAP_AREZZO_FOREST;
		if (dwNow < t.dwNextRestock || ch->IsDead() || !ch->IsItemLoaded() || !(packing || IsPlayerBotArezzoHeldHere(ch)))
			return;
		t.dwNextRestock = dwNow + 10000;
		size_t red = 0, blue = 0;
		CountPlayerBotPotions(ch, red, blue);
		const bool big = ch->GetLevel() >= PLAYERBOT_BIG_POTION_MIN_LEVEL;
		const bool caster = ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA;
		const size_t wantRed = packing ? PLAYERBOT_AREZZO_LAS_KIT_RED : 60;
		const size_t wantBlue = packing ? PLAYERBOT_AREZZO_LAS_KIT_BLUE : 40;
		int boughtRed = 0, boughtBlue = 0;
		for (int i = 0; i < 4 && red + boughtRed < wantRed && ch->GetEmptyInventory(1) >= 0; ++i)
		{
			const long long cost = 200LL * (big ? 40 : 20);
			if ((long long)ch->GetGold() < cost * 2)
				break;
			PlayerBotChangeGold(ch, -cost);
			ch->AutoGiveItem(big ? 27003 : 27002, 200);
			boughtRed += 200;
		}
		for (int i = 0; caster && i < 3 && blue + boughtBlue < wantBlue && ch->GetEmptyInventory(1) >= 0; ++i)
		{
			const long long cost = 200LL * (big ? 64 : 32);
			if ((long long)ch->GetGold() < cost * 2)
				break;
			PlayerBotChangeGold(ch, -cost);
			ch->AutoGiveItem(big ? 27006 : 27005, 200);
			boughtBlue += 200;
		}
		if (boughtRed || boughtBlue)
			sys_log(0, "ARZ_BOT: restock pid=%u name=%s map=%ld red=%u blue=%u bought_red=%d bought_blue=%d gold=%lld kit=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), (unsigned int)red, (unsigned int)blue,
					boughtRed, boughtBlue, (long long)ch->GetGold(), packing ? 1 : 0);
		else if (red < 10)
			PlayerBotLogThrottled("arezzo_restock_failed", dwNow,
					"ARZ_BOT: restock impossible pid=%u name=%s map=%ld red=%u free_cell=%d gold=%lld",
					ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), (unsigned int)red,
					ch->GetEmptyInventory(1) >= 0 ? 1 : 0, (long long)ch->GetGold());
	}

	void TickPlayerBotArezzo()
	{
		const DWORD dwNow = get_dword_time();
		static DWORD s_dwStarted = 0, s_dwNextFile = 0, s_dwNextTrack = 0;
		if (s_dwStarted == 0)
			s_dwStarted = dwNow;
		if (!s_bPlayerBotArezzoOrdersLoaded)
			LoadPlayerBotArezzoOrders();
		// Once the population has begun to come in (the registry is loaded by
		// then), so the test characters queue behind nothing that refuses them.
		if (!s_bPlayerBotArezzoCohortLoaded && dwNow - s_dwStarted >= PLAYERBOT_AREZZO_COHORT_DELAY_MS &&
				CPlayerBotManager::instance().GetCount() > 0)
			LoadPlayerBotArezzoCohort(dwNow);
		if (dwNow >= s_dwNextFile)
		{
			s_dwNextFile = dwNow + 5000;
			RunPlayerBotArezzoTestFile(dwNow);
		}
		const bool writeTrack = dwNow >= s_dwNextTrack;
		FILE* out = NULL;
		if (writeTrack)
		{
			s_dwNextTrack = dwNow + PLAYERBOT_AREZZO_TRACK_MS;
			out = fopen(PLAYERBOT_AREZZO_TRACK_FILE, "a");
		}
		const time_t wall = time(0);
		int onMap[3] = { 0, 0, 0 }, lasTemple = 0, lasRoad = 0, sentAway = 0, cohortOnline = 0, restingNow = 0;
		for (TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.begin(); it != s_mapPlayerBotAIStates.end(); ++it)
		{
			const DWORD pid = it->first;
			if (IsPlayerBotArezzoCohortPID(pid))
				++cohortOnline;
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pid);
			if (!ch)
				continue;
			const long map = ch->GetMapIndex();
			const bool here = IsPlayerBotArezzoMap(map);
			std::map<DWORD, long>::const_iterator forced = s_mapPlayerBotArezzoForced.find(pid);
			const bool las = s_mapPlayerBotArezzoLas.count(pid) != 0;
			if (!here && forced == s_mapPlayerBotArezzoForced.end() && !las)
				continue;
			TPlayerBotAIState& state = it->second;
			TPlayerBotArezzoTrack& t = s_mapPlayerBotArezzoTrack[pid];
			int mode = 0;	// 0 hunting, 1 leaving, 2 on the way in (road), 3 in the temple for the Las
			RestockPlayerBotArezzo(ch, t, dwNow);
			if (here)
			{
				++onMap[map - PLAYERBOT_MAP_AREZZO_CYCLOPS];
				if (t.dwEntered == 0 || t.lMap != map)
				{
					// Logged in on the map (a restart), or came by a road the
					// gate did not see.
					t.dwEntered = dwNow;
					t.lMap = map;
					t.dwAnchorSince = dwNow;
					t.dwVisitDeaths = 0;
					t.adwDeathAt[0] = t.adwDeathAt[1] = 0;
				}
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
					sys_log(0, "ARZ_BOT: level up pid=%u name=%s map=%ld level=%u", pid, ch->GetName(), map, (unsigned int)ch->GetLevel());
				}
				else if (exp > t.dwLastExp)
					t.ullExp += exp - t.dwLastExp;
				t.bLastLevel = ch->GetLevel();
				t.dwLastExp = exp;
				t.dwLastNext = ch->GetNextExp();
				// Who is fighting it, while it lives: its target striking back, or
				// the threat it retreats from (the engine keeps no killer).
				if (!ch->IsDead())
				{
					LPCHARACTER foe = NULL;
					LPCHARACTER v = ch->GetVictim();
					if (v && !v->IsDead() && v->GetVictim() == ch)
						foe = v;
					if (!foe && state.dwRetreatThreatVID)
						foe = CHARACTER_MANAGER::instance().Find(state.dwRetreatThreatVID);
					if (!foe && state.dwTargetVID)
					{
						LPCHARACTER tv = CHARACTER_MANAGER::instance().Find(state.dwTargetVID);
						if (tv && !tv->IsDead() && tv->GetVictim() == ch)
							foe = tv;
					}
					if (foe && foe != ch)
					{
						t.dwFoeRace = foe->IsPC() ? 0 : foe->GetRaceNum();
						t.bFoePC = foe->IsPC();
						t.dwFoeAt = dwNow;
					}
				}
				if (ch->IsDead() && !t.bWasDead)
				{
					++t.dwDeaths;
					++t.dwVisitDeaths;
					t.adwDeathAt[0] = t.adwDeathAt[1];
					t.adwDeathAt[1] = dwNow;
					const bool known = t.dwFoeAt != 0 && dwNow - t.dwFoeAt <= 15000;
					const CMob* mob = known && !t.bFoePC ? CMobManager::instance().Get(t.dwFoeRace) : NULL;
					sys_log(0, "ARZ_BOT: died pid=%u name=%s map=%ld level=%u job=%u pos=(%ld,%ld) visit_deaths=%u by=%u by_name=%s pvp=%d potions_red=%d",
							pid, ch->GetName(), map, (unsigned int)ch->GetLevel(), (unsigned int)(ch->GetJob() % 4),
							ch->GetX(), ch->GetY(), t.dwVisitDeaths,
							known ? (t.bFoePC ? 1U : t.dwFoeRace) : 0U,
							known ? (t.bFoePC ? "player" : (mob ? mob->m_table.szLocaleName : "?")) : "unknown",
							known && t.bFoePC ? 1 : 0, (int)ch->CountSpecifyItem(27003) + (int)ch->CountSpecifyItem(27002) +
							(int)ch->CountSpecifyItem(27001));
				}
				// A second death within the window: the next revival at the
				// arrival, not in the pack that killed it (not on the way out).
				if (t.bWasDead && !ch->IsDead() && t.adwDeathAt[0] != 0 &&
						!s_mapPlayerBotArezzoExit.count(pid) && !s_mapPlayerBotArezzoPending.count(pid) &&
						dwNow - t.adwDeathAt[0] <= PLAYERBOT_AREZZO_DEATH_WINDOW_MS)
				{
					const TPlayerBotArezzoMap* info = GetPlayerBotArezzoMapInfo(map);
					state.dwTargetVID = 0;
					ch->SetVictim(NULL);
					ClearPlayerBotRoute(state, true);
					ForgetPlayerBotArezzoWalk(pid);
					ch->Stop();
					const long fromX = ch->GetX(), fromY = ch->GetY();
					if (info && ch->Show(map, info->lArrivalX, info->lArrivalY, 0))
					{
						ch->Stop();
						ch->SendMovePacket(FUNC_MOVE, 0, info->lArrivalX, info->lArrivalY, 0, dwNow);
						sys_log(0, "ARZ_BOT: restarted at the arrival pid=%u name=%s map=%ld from=(%ld,%ld) visit_deaths=%u",
								pid, ch->GetName(), map, fromX, fromY, t.dwVisitDeaths);
					}
					t.adwDeathAt[0] = t.adwDeathAt[1] = 0;
				}
				t.bWasDead = ch->IsDead();
				const bool fighting = ch->GetVictim() != NULL ||
						(state.dwLastCombatActionTime != 0 && dwNow - state.dwLastCombatActionTime < 10000);
				// Standing still on purpose is no stuck: the mood's "away from
				// the keyboard" (2-4 min) and pause between packs, and the rest
				// after a revival. 101 of round 2's 126 "stuck" were the first,
				// 16 the last, all on open ground.
				bool resting = (state.persona.dwAfkUntil != 0 && dwNow < state.persona.dwAfkUntil) ||
						(state.persona.dwPauseUntil != 0 && dwNow < state.persona.dwPauseUntil) ||
						state.bRecoveringAfterDeath;
				// A party member beside its leader goes at the leader's pace
				// (145 of round 3's 169 "stuck" on 360/361 were party members
				// waiting while the leader stopped); the leader is watched itself.
				if (!resting && ch->GetParty())
				{
					LPCHARACTER leader = ch->GetParty()->GetLeaderCharacter();
					if (leader && leader != ch && leader->GetMapIndex() == map &&
							DISTANCE_APPROX(ch->GetX() - leader->GetX(), ch->GetY() - leader->GetY()) <= 2500)
						resting = true;
				}
				if (resting)
					++restingNow;
				if (fighting || ch->IsDead() || resting ||
						DISTANCE_APPROX(ch->GetX() - t.lAnchorX, ch->GetY() - t.lAnchorY) > PLAYERBOT_AREZZO_STUCK_DISTANCE)
				{
					if (t.bStuck)
						sys_log(0, "ARZ_BOT: unstuck pid=%u name=%s map=%ld after_s=%u", pid, ch->GetName(), map, (dwNow - t.dwAnchorSince) / 1000);
					t.bStuck = false;
					t.lAnchorX = ch->GetX();
					t.lAnchorY = ch->GetY();
					t.dwAnchorSince = dwNow;
				}
				else if (!t.bStuck && dwNow - t.dwAnchorSince >= PLAYERBOT_AREZZO_STUCK_MS)
				{
					t.bStuck = true;
					++t.dwStuckEpisodes;
					LPCHARACTER tv = state.dwTargetVID ? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
					sys_log(0, "ARZ_BOT: stuck pid=%u name=%s map=%ld pos=(%ld,%ld) action=%u route=%u/%u leaving=%d hp=%d target=%u target_dist=%d hub=%u",
							pid, ch->GetName(), map, ch->GetX(), ch->GetY(), (unsigned int)state.bCurrentAction,
							(unsigned int)state.uRouteIndex, (unsigned int)state.vecRoute.size(),
							IsPlayerBotArezzoLeaving(ch) ? 1 : 0,
							ch->GetMaxHP() > 0 ? (int)(ch->GetHP() * 100LL / ch->GetMaxHP()) : 0,
							tv ? (unsigned int)tv->GetRaceNum() : 0U,
							tv ? DISTANCE_APPROX(ch->GetX() - tv->GetX(), ch->GetY() - tv->GetY()) : -1,
							(unsigned int)state.wHuntingHub);
				}
				if (t.bStuck)
					t.dwStuckMs += 1000;
				if (IsPlayerBotArezzoLeaving(ch))
					mode = 1;
			}
			else if (las)
			{
				mode = map == PLAYERBOT_MAP_OCHAO ? 3 : 2;
				if (mode == 3)
					++lasTemple;
				else
					++lasRoad;
			}
			else
			{
				mode = 2;
				++sentAway;
				ResendPlayerBotArezzo(ch, state, t, forced != s_mapPlayerBotArezzoForced.end() ? forced->second : 0, dwNow);
			}
			if (here || las)
				t.dwAwaySince = 0;
			if (out)
			{
				LPCHARACTER victim = ch->GetVictim();
				fprintf(out, "%ld\t%u\t%s\t%u\t%u\t%ld\t%ld\t%ld\t%d\t%u\t%u\t%llu\t%u\t%u\t%d\t%d\t%u\t%u\t%u\t%ld\t%u\n",
						(long)wall, pid, ch->GetName(), (unsigned int)ch->GetLevel(), (unsigned int)(ch->GetJob() % 4),
						map, ch->GetX(), ch->GetY(),
						ch->GetMaxHP() > 0 ? (int)(ch->GetHP() * 100LL / ch->GetMaxHP()) : 0,
						(unsigned int)state.bCurrentAction, t.dwKills, t.ullExp, t.dwDeaths, t.dwStuckEpisodes,
						t.bStuck ? 1 : 0, mode, victim ? (unsigned int)victim->GetRaceNum() : 0U, t.dwBossKills,
						t.dwStuckMs / 1000, forced != s_mapPlayerBotArezzoForced.end() ? forced->second : 0L,
						(unsigned int)state.wBossRaidRace);
			}
		}
		if (out)
		{
			fprintf(out, "#\t%ld\tm360=%d\tm361=%d\tm362=%d\tlas_temple=%d\tlas_road=%d\tsent_away=%d\tforced=%u\tcohort_online=%d\tportal=%u\tguardian=%u\twalks=%u\twalks_nav=%u\trefused=%u\topen=%d\tresting=%d\n",
					(long)wall, onMap[0], onMap[1], onMap[2], lasTemple, lasRoad, sentAway,
					(unsigned int)s_mapPlayerBotArezzoForced.size(), cohortOnline,
					(unsigned int)mt2009_ochao::s_dwPortalVID, (unsigned int)mt2009_ochao::s_dwGuardianVID,
					s_uPlayerBotArezzoWalksPlanned, s_uPlayerBotArezzoWalksNavOnly, s_uPlayerBotArezzoRefused,
					IsPlayerBotArezzoOpen() ? 1 : 0, restingNow);
			fclose(out);
			for (std::map<DWORD, TPlayerBotArezzoWalk>::iterator w = s_mapPlayerBotArezzoWalk.begin();
					w != s_mapPlayerBotArezzoWalk.end();)
			{
				if (dwNow - w->second.dwLastUsed > 300000)
					s_mapPlayerBotArezzoWalk.erase(w++);
				else
					++w;
			}
			// A bot gone from the world keeps no walk out.
			for (std::map<DWORD, TPlayerBotArezzoExit>::iterator e = s_mapPlayerBotArezzoExit.begin();
					e != s_mapPlayerBotArezzoExit.end();)
			{
				LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(e->first);
				if (!ch || !IsPlayerBotArezzoMap(ch->GetMapIndex()))
					s_mapPlayerBotArezzoExit.erase(e++);
				else
					++e;
			}
		}
	}

	EVENTINFO(playerbot_arezzo_watch_info)
	{
		int dummy;
	};

	LPEVENT s_pkPlayerBotArezzoWatch = NULL;

	EVENTFUNC(playerbot_arezzo_watch)
	{
		TickPlayerBotArezzo();
		return PASSES_PER_SEC(1);
	}

	// From CPlayerBotManager::StartWorldClock, on the core that hosts any of
	// the three maps.
	void StartPlayerBotArezzoWatch()
	{
		if (s_pkPlayerBotArezzoWatch)
			return;
		bool hosted = false;
		for (int i = 0; i < PLAYERBOT_AREZZO_MAP_COUNT; ++i)
			hosted = hosted || IsPlayerBotArezzoHosted(PLAYERBOT_AREZZO_MAPS[i].lMap);
		if (!hosted)
			return;
		playerbot_arezzo_watch_info* info = AllocEventInfo<playerbot_arezzo_watch_info>();
		s_pkPlayerBotArezzoWatch = event_create(playerbot_arezzo_watch, info, PASSES_PER_SEC(5));
		sys_log(0, "ARZ_BOT: watch started (test file %s, cohort file %s, track %s)",
				PLAYERBOT_AREZZO_TEST_FILE, PLAYERBOT_AREZZO_COHORT_FILE, PLAYERBOT_AREZZO_TRACK_FILE);
	}
}

#endif
