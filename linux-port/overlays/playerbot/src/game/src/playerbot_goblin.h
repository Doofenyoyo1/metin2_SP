// MT2009 PLUS Poszukiwanie skarbow z Goblinem Skarbow - MT2009_PLUS_GOBLIN_V1
// (the operator, 28 September: "Teraz jeszcze event goblina ... Wszystkie GUI
// maja byc takie jak w plikach, nagrody rowniez takie jak w plikach").
//
// The official Treasure Hunt ("Hazine Avi", the "Official Treasure Hunt
// System" archive) on our chat-command protocol - no packet, nothing in the
// client's exe. The event is the events file's kind "goblin"
// (playerbot_event_rules.h, KIND_GOBLIN): both panels switch it like any
// other, and every core judges it itself once a second (playerbot_events.h
// calls GoblinEventTick).
//
// While it runs:
//  - a Treasure Ticket (70617) comes out of a chest now and then (the
//    archive: "every kind of treasure chest"); using it takes the player to
//    Treasure Island (map 419, a private copy for one), where the Treasure
//    Goblin (20856) waits. Talking to him is the quest goblin_skarbow (the
//    archive's minigame_treasure_hunt, in Polish): the story, the goblin's
//    blessings bought for yang (HP, defence, poison immunity, speed - the
//    archive's costs), then the escort along one of three paths with a
//    Giant Treasure Chest (20857) at its end, monster waves at the
//    checkpoints (each kill 1 Doubloon), the wrong chest sends everyone
//    back to try the next path, the right one gives 100 Doubloons. The goblin
//    or the player dying fails the run; thirty minutes at most.
//  - the Doubloons (at most 999) buy the reward board: 90 reveal the 25
//    rewards, "Szukaj skarbu" takes a random one of those the player's tier
//    opens (the first of a round free, then a Goblin Key - a virtual one won
//    on the island every fifth right chest, or the item 70618), nine make a
//    round; the round's reward (the archive's ten round pools, the 6th and
//    9th with a refine blessing for the first 100 / 50 characters) is taken
//    in the round window, and 90 Doubloons reset the board. Rounds 3, 5 and
//    10 open the rare, ancient and legendary slots. The ranking counts the
//    rounds.
//
// The archive's vnums, mapped to this world's where an official item does
// not exist here (the table below says which).
//
// The quest bridge: the quest reads "goblin.phase" (set here) and writes
// "goblin.req" (1 blessings, 2 escort, 20 wrong chest seen, 22 open the chest)
// and "goblin.buffs" (the blessings paid for, a bit mask); this polls the
// owners of its instances four times a second.
//
// To the client, one command with a sub-command (game.py "GOB", uigoblin.py):
//   GOB event <on> <end epoch>
//   GOB info <on> <doubloons> <rounds> <keys> <claims> <tier> <revealed> <can claim> <can take round> <claimed mask>
//   GOB slots <vnum>,<count>;...            (25)
//   GOB open                                (the window, after "/goblin")
//   GOB got <slot> <vnum> <count> <keys>
//   GOB acc <tier> <can take>
//   GOB accr <round> <vnum>,<count>,<affect 0|1>,<limited 0|1>,<left>;...
//   GOB rank <place> <name> <rounds>        GOB rankme <place> <name> <rounds>   GOB rankend
//   GOB dun <in> <phase> <hp> <max hp> <blessings> <seconds left>
//   GOB msg <id> <data>
// From the client: "/goblin [info|odkryj|szukaj|reset|tury|tura|ranking|wyjdz]",
// and for a GM "/goblin gm <doblony|tury|klucze|reset|wyspa|bilet> [n]".
#include "start_position.h"

namespace mt2009_goblin
{
	const DWORD TICKET_VNUM = 70617;
	const DWORD KEY_VNUM = 70618;
	const DWORD KEY_BOX_VNUM = 70619;
	const DWORD KEY_BOX_KEYS = 8;
	const DWORD GOBLIN_VNUM = 20856;
	const DWORD CHEST_VNUM = 20857;
	const long MAP_INDEX = 419;
	// Treasure Island's base and its Town.txt spawn (386, 372).
	const long MAP_BASE_X = 512000;
	const long MAP_BASE_Y = 1203200;
	const long SPAWN_X = MAP_BASE_X + 386 * 100;
	const long SPAWN_Y = MAP_BASE_Y + 372 * 100;
	// The goblin's own place (the archive: local 380, 378).
	const long GOBLIN_X = MAP_BASE_X + 380 * 100;
	const long GOBLIN_Y = MAP_BASE_Y + 378 * 100;

	const int GOLD_MAX = 999;
	const int GOLD_ENTER_LIMIT = 900;
	const int GOLD_REVEAL = 90;
	const int GOLD_CHEST = 100;
	const int ENTER_LEVEL = 70;
	const int BOARD_SLOTS = 25;
	const int CLAIMS_PER_ROUND = 9;
	const int ROUNDS = 10;
	const int RUN_SECONDS = 30 * 60;
	const int KEY_EVERY_CHESTS = 5;

	const int GOBLIN_HP = 180000;
	const int GOBLIN_HP_BUFFED = 225000;
	const int BUFF_COST[4] = { 500000, 500000, 500000, 1500000 };
	// The goblin's blessings, as affects on him: they outlive the engine's
	// ComputePoints, which a PointChange would not.
	const DWORD AFFECT_GOBLIN_DEF = 671;
	const DWORD AFFECT_GOBLIN_SPEED = 672;
	// The poison blessing: the engine never asks IsImmune(IMMUNE_POISON) (its
	// test in AttackedByPoison is commented out), so an immune flag alone did
	// nothing and a wave's poison took 10 x 8 % of the goblin's health. Poison
	// damage is GetPoisonDamageRate - POINT_POISON_REDUCE, so this makes it 0,
	// and KeepGoblin cures a poisoning the moment it lands.
	const DWORD AFFECT_GOBLIN_POISON = 673;
	// The round rewards' refine blessings (the archive's affect 668 and 669):
	// the next refine +10 %, the next refine without its materials. The
	// engine asks through MT2009_PLUS_GOBLIN_V1 (refine ...).
	const DWORD AFFECT_REFINE_PCT = 668;
	const DWORD AFFECT_REFINE_FREE = 669;

	enum EPhase
	{
		PHASE_NONE = 0,
		PHASE_INTRO = 1,
		PHASE_BUFF = 2,
		PHASE_ESCORT = 3,
		PHASE_CHEST_FAIL = 4,
		PHASE_CHEST_SUCCESS = 5,
		PHASE_REWARD = 6,
		PHASE_FAILED = 7,
	};

	enum EEscort { ESCORT_WALK = 0, ESCORT_FIGHT = 1, ESCORT_CHEST = 2 };
	enum EWaypoint { WP_TURN = 0, WP_MOB = 1, WP_CHEST = 2 };

	// The messages the client words (uigoblin.py), the archive's numbers.
	enum EMsg
	{
		MSG_EVENT_ON = 0,
		MSG_EVENT_OFF = 1,
		MSG_CAN_GET_ACC = 2,
		MSG_REMAIN_BUFF = 3,
		MSG_EXHAUSTED_BUFF = 4,
		MSG_ENTER_MAX_GOLD = 6,
		MSG_REMAINING_TIME = 7,
		MSG_OVER_TIME = 8,
		MSG_FOUND_BOX = 9,
		MSG_GOLD_100 = 10,
		MSG_ESCORT_START_1 = 11,
		MSG_ESCORT_START_2 = 12,
		MSG_GOLD_1 = 13,
		MSG_GOLD_FULL = 14,
		MSG_REPLAY_ESCORT = 15,
		MSG_GIVEUP_1 = 16,
		MSG_GIVEUP_2 = 17,
		MSG_ENTER_LEVEL = 18,
		MSG_ENTER_PARTY = 19,
		MSG_CANNOT_RESET_LIST = 20,
		MSG_CANNOT_RESET = 21,
		MSG_ACC_TOOLTIP = 22,
		MSG_NEW_ROUND = 23,
		MSG_NOT_ENOUGH_GOLD = 24,
		MSG_INVENTORY_FULL = 25,
		MSG_NO_KEY = 26,
		MSG_REVEAL_FIRST = 27,
		MSG_ROUND_DONE = 28,
		MSG_TICKET_PLACE = 29,
		// Ours.
		MSG_ISLAND_CLOSED = 40,
		MSG_TICKET_FOUND = 41,
		MSG_KEYS_FROM_BOX = 42,
		MSG_TALK_TO_GOBLIN = 43,
		MSG_KEY_WON = 44,
	};

	struct Reward
	{
		DWORD vnum;
		WORD count;
	};

	// The reward board, slot by slot (treasure_hunt.txt, Group event_reward).
	// Official vnum -> ours where this world lacks it:
	//   27209 Green Potion (L)        -> 27102 Zielona Mikstura (D)
	//   27212 Purple Potion (L)       -> 27105 Fioletowa Mikstura (D)
	//   76044 Pet Book Chest          -> 55009 the new pet system's chest of a random pet skill book
	//   50261 Cor Daemonis (Rough)    -> 50255 Cor Draconis (Rough)
	//   83074 Plat. Monster Card Box  -> 50037 Heksagonalna Szkatulka (no monster cards)
	//   72061 Medal of the Dragon+    -> 71004 Medal Smoka
	//   76040 Cor Draconis (Normal)   -> 50256 Cor Draconis (szlif.)
	//   79026 Iron Dragon Elixir (S)  -> 72723 Eliksir Slonca (M)
	//   72348 Time Spiral (20%)       -> 100002 Eliksir Czasu (D), the biggest one
	//   55031 Tasty Treat+            -> 50023 Sakiewka Pieniedzy (no pets in the client;
	//                                    1 000 000 - 10 000 000 yang, special_item_group)
	// MT2009_PLUS_GOBLIN_V1 (cors): every Cor is one of ours that opens into stones
	// (50255-50259, special_item_group), not the 515xx series (the owner, 29 September):
	// normal tier 50255, rare 50256, ancient 50257, round 10 50258.
	// MT2009_PLUS_GOBLIN_V1 (board): slot 17, the rare tier's Blessing of Life (71018),
	// is 20 Peleryna Mestwa (70038) in one stack (the owner, 29 September).
	const Reward BOARD[BOARD_SLOTS] =
	{
		{ 27102, 1 }, { 27105, 1 }, { KEY_VNUM, 1 }, { 72050, 1 }, { 55009, 1 },
		{ 76029, 1 }, { 50255, 1 }, { 76019, 1 }, { 71027, 1 }, { 50037, 1 },
		{ 71004, 1 }, { 70003, 1 }, { 70043, 1 }, { 71028, 1 }, { 76013, 1 },
		{ 71030, 1 }, { 71083, 1 }, { 70038, 20 }, { 50256, 1 }, { 76014, 1 },
		{ 72723, 1 }, { 70058, 1 }, { 50257, 1 }, { 100002, 1 }, { 50023, 1 },
	};

	// Which slots each tier opens (the archive's OPEN_SLOT_DATA_BY_REWARD_TYPE).
	const DWORD TIER_MASK[4] =
	{
		0x00001CE7UL, // normal:    0,1,2,5,6,7,10,11,12
		0x0007BDEFUL, // rare:      + 3,8,13,15,16,17,18
		0x01F7BDEFUL, // ancient:   + 20,21,22,23,24
		0x01FFFFFFUL, // legendary: all 25
	};

	struct RoundItem
	{
		DWORD vnum;
		WORD count;
		int weight;
	};

	struct Round
	{
		DWORD affect;       // 0: none
		int affectValue;
		int firstCome;      // characters per event that get the blessing
		int affectSeconds;
		RoundItem items[8];
		int itemCount;
	};

	// The round rewards (treasure_hunt.txt, event_accumulated_reward_list):
	// one of the items by weight, and the blessing while the first ones last.
	//   72057 Double Experience Ring (36h) -> 72049 Pierscien Doswiadczenia
	//   49993 Aura Fire Rune (100)         -> 50255 Cor Draconis (Rough)
	//   72784 Riding (Random)              -> 50060 Instr. Jazdy Konnej
	//   72774 Monster Hunter Book          -> 28437 Kamien Duszy Potwora+4
	//   79028 Iron Dragon Elixir (L)       -> 72725 Eliksir Slonca (D)
	//   79025 White Dragon Elixir (L)      -> 72729 Eliksir Ksiezyca (D)
	const Round ROUND[ROUNDS] =
	{
		{ 0, 0, 0, 0, { { 27102, 40, 50 }, { 50255, 3, 50 } }, 2 },
		{ 0, 0, 0, 0, { { 71044, 10, 50 }, { 71045, 10, 50 } }, 2 },
		{ 0, 0, 0, 0, { { 50255, 15, 50 }, { 72049, 1, 50 } }, 2 },
		{ 0, 0, 0, 0, { { KEY_BOX_VNUM, 1, 50 }, { 71020, 5, 50 } }, 2 },
		{ 0, 0, 0, 0, { { 113000, 1, 12 }, { 123000, 1, 12 }, { 133000, 1, 12 }, { 143000, 1, 12 },
				{ 153000, 1, 12 }, { 163000, 1, 12 }, { 173000, 1, 12 }, { 50255, 20, 12 } }, 8 },
		{ AFFECT_REFINE_PCT, 10, 100, 259200, { { 50255, 10, 100 } }, 1 },
		{ 0, 0, 0, 0, { { 50060, 1, 50 }, { 28437, 1, 50 } }, 2 },
		{ 0, 0, 0, 0, { { 113000, 1, 10 }, { 123000, 1, 10 }, { 133000, 1, 10 }, { 143000, 1, 10 },
				{ 153000, 1, 10 }, { 163000, 1, 10 }, { 173000, 1, 10 }, { 100700, 2, 10 } }, 8 },
		{ AFFECT_REFINE_FREE, 0, 50, 259200, { { 53315, 1, 100 } }, 1 },
		{ 0, 0, 0, 0, { { 72725, 1, 20 }, { 71020, 10, 20 }, { 76019, 20, 20 }, { 50258, 1, 20 },
				{ 72729, 1, 20 } }, 5 },
	};

	// A Treasure Ticket out of a chest (the archive: silver, gold, boss and
	// jewellery chests): per cent for the chest - or its key - used.
	int TicketChance(DWORD vnum)
	{
		switch (vnum)
		{
			// The gold and silver chests, their keys and their "+" twins.
			case 50006: case 50007: case 50008: case 50009: case 50012: case 50013:
				return 20;
			// The bosses' chests.
			case 50070: case 50071: case 50072: case 50073: case 50074: case 50075:
			case 50076: case 50077: case 50078: case 50079: case 50080: case 50081:
			case 50082: case 50186: case 50254:
				return 30;
			// The Moonlight chest, which a chest event pours out.
			case 50011:
				return 3;
		}
		return 0;
	}

	struct Waypoint
	{
		long x;
		long y;
		BYTE type;
		BYTE mobs;
	};

	struct Path
	{
		long chestX;
		long chestY;
		int count;
		Waypoint wp[6];
	};

	// treasure_hunt_dungeon.txt: path_0 right, path_1 middle, path_2 left.
	// Absolute pixels; a private copy of the map keeps the base, so they hold
	// in every instance.
	const Path PATHS[3] =
	{
		{ 558785, 1223165, 5, { { 555903, 1235225, WP_MOB, 6 }, { 558152, 1233310, WP_TURN, 0 },
				{ 558352, 1231819, WP_MOB, 8 }, { 558546, 1227574, WP_MOB, 10 }, { 558794, 1223860, WP_CHEST, 0 } } },
		{ 532575, 1233381, 4, { { 544539, 1235880, WP_MOB, 6 }, { 541168, 1233473, WP_MOB, 8 },
				{ 536669, 1233450, WP_MOB, 10 }, { 533130, 1233318, WP_CHEST, 0 } } },
		{ 557171, 1260999, 5, { { 551030, 1248945, WP_MOB, 6 }, { 550948, 1252456, WP_TURN, 0 },
				{ 552359, 1256590, WP_MOB, 8 }, { 553661, 1258130, WP_MOB, 10 }, { 556946, 1260797, WP_CHEST, 0 } } },
	};

	// treasure_hunt_dungeon.txt's mob_pool.
	const DWORD MOB_POOL[] =
	{
		3001, 3002, 3003, 3004, 3005, 3101, 3102, 3103, 3104, 3105,
		3201, 3202, 3203, 3204, 3205, 3301, 3302, 3303, 3304, 3305,
		3401, 3402, 3403, 3404, 3405, 3501, 3502, 3503, 3504, 3505,
		3551, 3552, 3553, 3554, 3555, 3601, 3602, 3603, 3604, 3605,
		3701, 3702, 3703, 3704, 3705, 3801, 3802, 3803, 3804, 3805,
	};

	// The goblin's lines (locale_quest.txt 14271-14301, in Polish).
	const char* const SAY_START = "Dobrze, w takim razie chodz za mna!";
	const char* const SAY_HELP = "Aaaa! Pomocy!";
	const char* const SAY_THANKS = "Dziekuje! Dobrze, idziemy dalej.";
	const char* const SAY_DISAPPOINTED = "Uff. Myslalem, ze jestes silniejszy.";
	const char* const SAY_PLEAD = "Nie, nie, pomoz mi! Pomocy...";
	const char* const SAY_WRONG_KEY = "Moj klucz nie pasuje. Szukajmy dalej!";
	const char* const SAY_RIGHT_CHEST = "Dziekuje! W koncu znalezlismy skrzynie.";
	const char* const SAY_WRONG_CHEST = "To nie ta skrzynia. Szukajmy dalej!";
	const char* const SAY_CONTINUE = "Chodzmy dalej!";

	// The player's own flags ("goblin.*", the quest table).
	const char* const F_GEN = "goblin.gen";
	const char* const F_GOLD = "goblin.gold";
	const char* const F_ROUND = "goblin.round";
	const char* const F_KEYS = "goblin.keys";
	const char* const F_CLAIMS = "goblin.claims";
	const char* const F_MASK = "goblin.mask";
	const char* const F_REVEALED = "goblin.revealed";
	const char* const F_ACC = "goblin.acc";
	const char* const F_CHESTS = "goblin.chests";
	const char* const F_PENDING = "goblin.pending";
	// Where the player used the ticket - where the island sends them back.
	const char* const F_RET_MAP = "goblin.retmap";
	const char* const F_RET_X = "goblin.retx";
	const char* const F_RET_Y = "goblin.rety";
	// The quest bridge.
	const char* const F_PHASE = "goblin.phase";
	const char* const F_REQ = "goblin.req";
	const char* const F_BUFFS = "goblin.buffs";
	// The world's (event flags).
	const char* const E_GEN = "goblin_event_gen";
	const char* const E_SEEN = "goblin_event_seen";
	const char* const E_FC6 = "goblin_first_come_6";
	const char* const E_FC9 = "goblin_first_come_9";
	// Open unless a GM closed it ("/goblin gm wyspa 0" sets goblin_island_off).
	const char* const E_ISLAND = "goblin_island_off";

	struct Instance
	{
		DWORD pid;
		long map;
		DWORD goblinVid;
		DWORD chestVid;
		BYTE phase;
		BYTE correctPath;
		BYTE path;
		BYTE order[3];
		BYTE attempted;
		BYTE buffs;
		int cleared;
		BYTE escort;
		BYTE wp;
		std::vector<DWORD> wave;
		DWORD fearAt;
		DWORD fightAt;
		bool slowSaid;
		DWORD walkAt;
		long lastX;
		long lastY;
		int stuck;
		DWORD createdAt;
		bool arrived;
		DWORD awaySince;
		long endsAt;        // epoch
		long nextTimeNote;  // epoch
		DWORD uiAt;
		DWORD leaveAt;      // 0, or when to warp the owner out
		bool success;

		Instance() : pid(0), map(0), goblinVid(0), chestVid(0), phase(PHASE_NONE), correctPath(0), path(0),
				attempted(0), buffs(0), cleared(0), escort(ESCORT_WALK), wp(0), fearAt(0), fightAt(0), slowSaid(false),
				walkAt(0), lastX(0), lastY(0), stuck(0), createdAt(0), arrived(false), awaySince(0), endsAt(0),
				nextTimeNote(0), uiAt(0), leaveAt(0), success(false)
		{
			order[0] = 0; order[1] = 1; order[2] = 2;
		}
	};

	std::map<DWORD, Instance> s_mapInstances;
	std::vector<std::pair<long, DWORD> > s_vecDestroyMaps; // map, when
	std::set<DWORD> s_setKnown;          // characters told the event's state
	std::map<DWORD, DWORD> s_mapLastCommand;
	// A warp asked for, not repeated for ten seconds (the character stays on
	// its map until the client has gone).
	std::map<DWORD, DWORD> s_mapWarping;
	DWORD s_dwBootAt = 0;
	LPEVENT s_pkTick = NULL;
	bool s_bWasOn = false;
	bool s_bLastOn = false;
	long s_lLastEnd = 0;
	DWORD s_dwNextMinute = 0;
	DWORD s_dwNextSecond = 0;

	struct RankRow
	{
		std::string name;
		int rounds;
	};
	std::vector<RankRow> s_vecRank;
	int s_iRankGen = -1;
	DWORD s_dwRankAt = 0;

	// ---------------------------------------------------------------- the event

	const playerbot_events::Status& Status()
	{
		return s_aPlayerBotEventStatus[playerbot_events::KIND_GOBLIN];
	}

	bool IsOn()
	{
		return Status().active;
	}

	long EndEpoch()
	{
		return IsOn() ? Status().until : 0;
	}

	int Gen()
	{
		return quest::CQuestManager::instance().GetEventFlag(E_GEN);
	}

	bool IslandOpen()
	{
		return quest::CQuestManager::instance().GetEventFlag(E_ISLAND) == 0;
	}

	bool HostsIsland()
	{
		return map_allow_find(MAP_INDEX);
	}

	bool IsIslandMap(long map)
	{
		return (map >= 10000 ? map / 10000 : map) == MAP_INDEX;
	}

	bool Eligible(LPCHARACTER ch)
	{
		return ch && ch->IsPC() && ch->GetDesc() && !ch->GetDesc()->IsBot();
	}

	void Cmd(LPCHARACTER ch, const char* format, ...)
	{
		if (!ch || !ch->GetDesc() || ch->GetDesc()->IsBot())
			return;
		char buf[480];
		va_list args;
		va_start(args, format);
		vsnprintf(buf, sizeof(buf), format, args);
		va_end(args);
		ch->ChatPacket(CHAT_TYPE_COMMAND, "GOB %s", buf);
	}

	void Msg(LPCHARACTER ch, int id, long data = 0)
	{
		Cmd(ch, "msg %d %ld", id, data);
	}

	// ------------------------------------------------------- the player's flags

	int Flag(LPCHARACTER ch, const char* flag)
	{
		const int v = ch->GetQuestFlag(flag);
		return v < 0 ? 0 : v;
	}

	int Gold(LPCHARACTER ch) { return std::min(GOLD_MAX, Flag(ch, F_GOLD)); }
	void SetGold(LPCHARACTER ch, int v) { ch->SetQuestFlag(F_GOLD, std::max(0, std::min(GOLD_MAX, v))); }
	int Rounds(LPCHARACTER ch) { return Flag(ch, F_ROUND); }
	int VirtualKeys(LPCHARACTER ch) { return Flag(ch, F_KEYS); }
	int Keys(LPCHARACTER ch) { return VirtualKeys(ch) + (int)ch->CountSpecifyItem(KEY_VNUM); }
	int Claims(LPCHARACTER ch) { return std::min(CLAIMS_PER_ROUND, Flag(ch, F_CLAIMS)); }
	DWORD ClaimedMask(LPCHARACTER ch) { return (DWORD)ch->GetQuestFlag(F_MASK); }
	bool Revealed(LPCHARACTER ch) { return Flag(ch, F_REVEALED) != 0; }
	bool AccTaken(LPCHARACTER ch) { return Flag(ch, F_ACC) != 0; }

	int Tier(LPCHARACTER ch)
	{
		const int r = Rounds(ch);
		return r >= 10 ? 3 : (r >= 5 ? 2 : (r >= 3 ? 1 : 0));
	}

	int RoundTier(LPCHARACTER ch)
	{
		return std::min(ROUNDS - 1, Rounds(ch));
	}

	bool SlotOpen(int tier, int slot)
	{
		return slot >= 0 && slot < BOARD_SLOTS && (TIER_MASK[tier] & (1UL << slot)) != 0;
	}

	bool CanTakeRound(LPCHARACTER ch)
	{
		return Revealed(ch) && Claims(ch) >= CLAIMS_PER_ROUND && !AccTaken(ch);
	}

	// A new event (the generation moved) begins with nothing: the Doubloons,
	// the rounds, the board and the keys - the archive's "removed from the
	// game at the end of the event" - and the tickets left from the last.
	void Reset(LPCHARACTER ch)
	{
		static const char* const flags[] = { F_GOLD, F_ROUND, F_KEYS, F_CLAIMS, F_MASK, F_REVEALED, F_ACC,
				F_CHESTS, F_PENDING, F_PHASE, F_REQ, F_BUFFS };
		for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i)
			if (ch->GetQuestFlag(flags[i]) != 0)
				ch->SetQuestFlag(flags[i], 0);
		const ITEM_COUNT keys = ch->CountSpecifyItem(KEY_VNUM);
		if (keys > 0)
			ch->RemoveSpecifyItem(KEY_VNUM, keys);
		const ITEM_COUNT tickets = ch->CountSpecifyItem(TICKET_VNUM);
		if (tickets > 0)
			ch->RemoveSpecifyItem(TICKET_VNUM, tickets);
	}

	void Sync(LPCHARACTER ch)
	{
		if (!ch || !IsOn())
			return;
		const int gen = Gen();
		if (gen <= 0)
			return;
		if (ch->GetQuestFlag(F_GEN) == gen)
			return;
		if (ch->GetQuestFlag(F_GEN) != 0)
			Reset(ch);
		ch->SetQuestFlag(F_GEN, gen);
	}

	// ------------------------------------------------------------ to the client

	void SendEvent(LPCHARACTER ch)
	{
		Cmd(ch, "event %d %ld", IsOn() ? 1 : 0, EndEpoch());
	}

	void SendInfo(LPCHARACTER ch)
	{
		Sync(ch);
		const bool on = IsOn();
		const bool revealed = Revealed(ch);
		const int claims = Claims(ch);
		Cmd(ch, "info %d %d %d %d %d %d %d %d %d %u", on ? 1 : 0, Gold(ch), Rounds(ch), std::min(255, Keys(ch)),
				claims, Tier(ch), revealed ? 1 : 0, (revealed && claims < CLAIMS_PER_ROUND) ? 1 : 0,
				CanTakeRound(ch) ? 1 : 0, (unsigned int)ClaimedMask(ch));
	}

	void SendSlots(LPCHARACTER ch)
	{
		std::string line;
		for (int i = 0; i < BOARD_SLOTS; ++i)
		{
			char part[32];
			snprintf(part, sizeof(part), "%u,%u;", BOARD[i].vnum, (unsigned int)BOARD[i].count);
			line += part;
		}
		Cmd(ch, "slots %s", line.c_str());
	}

	int FirstComeLeft(int round)
	{
		const Round& r = ROUND[round];
		if (!r.affect || r.firstCome <= 0)
			return 0;
		const int given = quest::CQuestManager::instance().GetEventFlag(round == 5 ? E_FC6 : E_FC9);
		return std::max(0, r.firstCome - given);
	}

	void SendRounds(LPCHARACTER ch)
	{
		Cmd(ch, "acc %d %d", RoundTier(ch), CanTakeRound(ch) ? 1 : 0);
		for (int r = 0; r < ROUNDS; ++r)
		{
			std::string line;
			const Round& cfg = ROUND[r];
			if (cfg.affect)
			{
				char part[48];
				snprintf(part, sizeof(part), "%u,%d,1,1,%d;", cfg.affect, cfg.affectValue, FirstComeLeft(r));
				line += part;
			}
			for (int i = 0; i < cfg.itemCount; ++i)
			{
				char part[48];
				snprintf(part, sizeof(part), "%u,%u,0,0,0;", cfg.items[i].vnum, (unsigned int)cfg.items[i].count);
				line += part;
			}
			Cmd(ch, "accr %d %s", r, line.c_str());
		}
	}

	// --------------------------------------------------------------- the board

	bool GiveReward(LPCHARACTER ch, DWORD vnum, WORD count)
	{
		if (vnum == KEY_VNUM)
		{
			ch->SetQuestFlag(F_KEYS, VirtualKeys(ch) + count);
			return true;
		}
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		if (!proto)
		{
			sys_err("GOBLIN: reward %u does not exist", vnum);
			return false;
		}
		if (ch->GetEmptyInventory(proto->bSize) < 0)
			return false;
		return ch->AutoGiveItem(vnum, (ITEM_COUNT)std::max<WORD>(1, count)) != NULL;
	}

	void Reveal(LPCHARACTER ch)
	{
		if (!IsOn())
			return Msg(ch, MSG_EVENT_OFF);
		if (Revealed(ch))
			return SendInfo(ch);
		if (Gold(ch) < GOLD_REVEAL)
			return Msg(ch, MSG_NOT_ENOUGH_GOLD, GOLD_REVEAL);
		if (Claims(ch) == 0 && ClaimedMask(ch) != 0)
			ch->SetQuestFlag(F_MASK, 0);
		SetGold(ch, Gold(ch) - GOLD_REVEAL);
		ch->SetQuestFlag(F_REVEALED, 1);
		sys_log(0, "GOBLIN: %s reveals the board (%d doubloons left)", ch->GetName(), Gold(ch));
		SendInfo(ch);
	}

	void ResetBoard(LPCHARACTER ch)
	{
		if (!IsOn() || !Revealed(ch))
			return SendInfo(ch);
		if (Claims(ch) <= 0 && ClaimedMask(ch) == 0)
			return Msg(ch, MSG_CANNOT_RESET_LIST);
		if (Claims(ch) >= CLAIMS_PER_ROUND && !AccTaken(ch))
			return Msg(ch, MSG_CANNOT_RESET);
		if (Gold(ch) < GOLD_REVEAL)
			return Msg(ch, MSG_NOT_ENOUGH_GOLD, GOLD_REVEAL);
		SetGold(ch, Gold(ch) - GOLD_REVEAL);
		ch->SetQuestFlag(F_MASK, 0);
		ch->SetQuestFlag(F_CLAIMS, 0);
		ch->SetQuestFlag(F_REVEALED, 0);
		ch->SetQuestFlag(F_ACC, 0);
		sys_log(0, "GOBLIN: %s resets the board", ch->GetName());
		SendInfo(ch);
	}

	void Claim(LPCHARACTER ch)
	{
		if (!IsOn())
			return Msg(ch, MSG_EVENT_OFF);
		if (!Revealed(ch))
			return Msg(ch, MSG_REVEAL_FIRST);
		const int claims = Claims(ch);
		if (claims >= CLAIMS_PER_ROUND)
			return Msg(ch, MSG_ROUND_DONE);
		std::vector<int> open;
		const int tier = Tier(ch);
		const DWORD mask = ClaimedMask(ch);
		for (int i = 0; i < BOARD_SLOTS; ++i)
			if (SlotOpen(tier, i) && !(mask & (1UL << i)))
				open.push_back(i);
		if (open.empty())
			return;
		// The first reward of a round is free; then a key - a won one first,
		// then one from the bag.
		bool usedVirtual = false, usedItem = false;
		if (claims > 0)
		{
			if (VirtualKeys(ch) > 0)
			{
				ch->SetQuestFlag(F_KEYS, VirtualKeys(ch) - 1);
				usedVirtual = true;
			}
			else if (ch->CountSpecifyItem(KEY_VNUM) > 0)
			{
				ch->RemoveSpecifyItem(KEY_VNUM, 1);
				usedItem = true;
			}
			else
				return Msg(ch, MSG_NO_KEY);
		}
		const int slot = open[number(0, (int)open.size() - 1)];
		if (!GiveReward(ch, BOARD[slot].vnum, BOARD[slot].count))
		{
			if (usedVirtual)
				ch->SetQuestFlag(F_KEYS, VirtualKeys(ch) + 1);
			else if (usedItem)
				ch->AutoGiveItem(KEY_VNUM, 1);
			return Msg(ch, MSG_INVENTORY_FULL);
		}
		ch->SetQuestFlag(F_MASK, (int)(mask | (1UL << slot)));
		ch->SetQuestFlag(F_CLAIMS, claims + 1);
		sys_log(0, "GOBLIN: %s takes slot %d (%u x%u), claim %d", ch->GetName(), slot, BOARD[slot].vnum,
				(unsigned int)BOARD[slot].count, claims + 1);
		if (claims + 1 >= CLAIMS_PER_ROUND)
			Msg(ch, MSG_CAN_GET_ACC);
		Cmd(ch, "got %d %u %u %d", slot, BOARD[slot].vnum, (unsigned int)BOARD[slot].count, std::min(255, Keys(ch)));
		SendInfo(ch);
	}

	void TakeRound(LPCHARACTER ch)
	{
		if (!IsOn())
			return;
		if (!Revealed(ch) || Claims(ch) < CLAIMS_PER_ROUND)
		{
			Msg(ch, MSG_ACC_TOOLTIP);
			return SendRounds(ch);
		}
		if (AccTaken(ch))
		{
			Msg(ch, MSG_CANNOT_RESET);
			return SendRounds(ch);
		}
		const int idx = RoundTier(ch);
		const Round& cfg = ROUND[idx];
		// One item by weight (the archive's iRandomPick 1).
		int total = 0;
		for (int i = 0; i < cfg.itemCount; ++i)
			total += std::max(1, cfg.items[i].weight);
		int roll = number(1, std::max(1, total));
		int pick = 0;
		for (int i = 0; i < cfg.itemCount; ++i)
		{
			roll -= std::max(1, cfg.items[i].weight);
			if (roll <= 0)
			{
				pick = i;
				break;
			}
		}
		const TItemTable* proto = cfg.itemCount ? ITEM_MANAGER::instance().GetTable(cfg.items[pick].vnum) : NULL;
		if (proto && ch->GetEmptyInventory(proto->bSize) < 0)
		{
			Msg(ch, MSG_INVENTORY_FULL);
			return SendRounds(ch);
		}
		if (cfg.affect)
		{
			const char* flag = idx == 5 ? E_FC6 : E_FC9;
			int given = quest::CQuestManager::instance().GetEventFlag(flag);
			if (cfg.firstCome <= 0 || given < cfg.firstCome)
			{
				ch->AddAffect(cfg.affect, POINT_NONE, cfg.affectValue, 0, cfg.affectSeconds, 0, true);
				++given;
				quest::CQuestManager::instance().SetEventFlag(flag, given);
				quest::CQuestManager::instance().RequestSetEventFlag(flag, given);
				const int left = cfg.firstCome - given;
				if (left > 0 && left <= 10)
					Msg(ch, MSG_REMAIN_BUFF, ((long)(idx + 1) << 16) | left);
			}
			else
				Msg(ch, MSG_EXHAUSTED_BUFF, idx + 1);
		}
		if (cfg.itemCount)
			GiveReward(ch, cfg.items[pick].vnum, cfg.items[pick].count);
		ch->SetQuestFlag(F_ROUND, Rounds(ch) + 1);
		ch->SetQuestFlag(F_ACC, 1);
		s_dwRankAt = 0;
		sys_log(0, "GOBLIN: %s takes round %d (%u x%u), rounds %d", ch->GetName(), idx + 1,
				cfg.itemCount ? cfg.items[pick].vnum : 0, cfg.itemCount ? (unsigned int)cfg.items[pick].count : 0,
				Rounds(ch));
		Msg(ch, MSG_NEW_ROUND);
		SendInfo(ch);
		SendRounds(ch);
	}

	// The ranking: the database's rounds of this event's generation. A
	// character's flags reach the table at its save, so the list is a few
	// minutes behind; the asker's own row is its live figure.
	void LoadRanking()
	{
		const int gen = Gen();
		const DWORD now = get_dword_time();
		if (gen == s_iRankGen && s_dwRankAt && now - s_dwRankAt < 30000)
			return;
		s_iRankGen = gen;
		s_dwRankAt = now;
		s_vecRank.clear();
		char query[640];
		snprintf(query, sizeof(query),
				"SELECT p.name, r.lValue FROM player.quest r "
				"JOIN player.quest g ON g.dwPID = r.dwPID AND g.szName = 'goblin' AND g.szState = 'gen' AND g.lValue = %d "
				"JOIN player.player p ON p.id = r.dwPID "
				"WHERE r.szName = 'goblin' AND r.szState = 'round' AND r.lValue > 0 "
				"ORDER BY r.lValue DESC, r.dwPID LIMIT 100", gen);
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			RankRow r;
			r.name = row[0] ? row[0] : "";
			r.rounds = 0;
			str_to_number(r.rounds, row[1]);
			s_vecRank.push_back(r);
		}
	}

	void SendRanking(LPCHARACTER ch)
	{
		Sync(ch);
		LoadRanking();
		// The asker's live figure in place of the saved one.
		std::vector<RankRow> rows;
		for (size_t i = 0; i < s_vecRank.size(); ++i)
			if (s_vecRank[i].name != ch->GetName())
				rows.push_back(s_vecRank[i]);
		const int mine = Rounds(ch);
		if (mine > 0)
		{
			RankRow me;
			me.name = ch->GetName();
			me.rounds = mine;
			size_t at = 0;
			while (at < rows.size() && rows[at].rounds >= mine)
				++at;
			rows.insert(rows.begin() + at, me);
		}
		int myPlace = 0;
		for (size_t i = 0; i < rows.size(); ++i)
		{
			if (i < 10)
				Cmd(ch, "rank %d %s %d", (int)i + 1, rows[i].name.c_str(), rows[i].rounds);
			if (rows[i].name == ch->GetName())
				myPlace = (int)i + 1;
		}
		Cmd(ch, "rankme %d %s %d", myPlace, ch->GetName(), mine);
		Cmd(ch, "rankend");
	}

	// ------------------------------------------------------------- the island

	Instance* Find(DWORD pid)
	{
		std::map<DWORD, Instance>::iterator it = s_mapInstances.find(pid);
		return it == s_mapInstances.end() ? NULL : &it->second;
	}

	LPCHARACTER Goblin(const Instance& inst)
	{
		if (!inst.goblinVid)
			return NULL;
		LPCHARACTER g = CHARACTER_MANAGER::instance().Find(inst.goblinVid);
		return (g && !g->IsDead()) ? g : NULL;
	}

	void SetPhase(Instance& inst, LPCHARACTER owner, BYTE phase)
	{
		inst.phase = phase;
		if (owner)
			owner->SetQuestFlag(F_PHASE, phase);
	}

	void Say(LPCHARACTER goblin, const char* text)
	{
		if (goblin)
			goblin->MonsterChat("%s", text);
	}

	void SendDungeon(LPCHARACTER ch, const Instance* inst)
	{
		if (!inst)
		{
			Cmd(ch, "dun 0 0 0 0 0 0");
			return;
		}
		LPCHARACTER g = Goblin(*inst);
		const long left = std::max(0L, inst->endsAt - (long)get_global_time());
		Cmd(ch, "dun 1 %d %d %d %d %ld", inst->phase, g ? g->GetHP() : 0, g ? g->GetMaxHP() : 0, inst->buffs, left);
	}

	void PurgeWave(Instance& inst)
	{
		for (size_t i = 0; i < inst.wave.size(); ++i)
		{
			LPCHARACTER mob = CHARACTER_MANAGER::instance().Find(inst.wave[i]);
			if (mob && !mob->IsPC())
				M2_DESTROY_CHARACTER(mob);
		}
		inst.wave.clear();
	}

	void KillEntities(Instance& inst)
	{
		PurgeWave(inst);
		if (inst.goblinVid)
		{
			LPCHARACTER g = CHARACTER_MANAGER::instance().Find(inst.goblinVid);
			if (g && !g->IsPC())
				M2_DESTROY_CHARACTER(g);
			inst.goblinVid = 0;
		}
		if (inst.chestVid)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().Find(inst.chestVid);
			if (c && !c->IsPC())
				M2_DESTROY_CHARACTER(c);
			inst.chestVid = 0;
		}
	}

	// A private map is destroyed only once nobody stands on it: the engine's
	// DestroyPrivateMap closes the connection of anyone it finds.
	void QueueDestroy(long map, DWORD delayMs)
	{
		if (map < 10000)
			return;
		for (size_t i = 0; i < s_vecDestroyMaps.size(); ++i)
			if (s_vecDestroyMaps[i].first == map)
				return;
		s_vecDestroyMaps.push_back(std::make_pair(map, get_dword_time() + delayMs));
	}

	void Close(DWORD pid, const char* why)
	{
		Instance* inst = Find(pid);
		if (!inst)
			return;
		KillEntities(*inst);
		QueueDestroy(inst->map, 3000);
		sys_log(0, "GOBLIN: island %ld of pid %u closed (%s)", inst->map, pid, why);
		s_mapInstances.erase(pid);
	}

	bool Warping(LPCHARACTER ch)
	{
		std::map<DWORD, DWORD>::iterator it = s_mapWarping.find(ch->GetPlayerID());
		return it != s_mapWarping.end() && get_dword_time() - it->second < 10000;
	}

	void MarkWarping(LPCHARACTER ch)
	{
		s_mapWarping[ch->GetPlayerID()] = get_dword_time();
	}

	// Back to where the ticket was used (goblin.ret*), or to the kingdom's
	// first village when that is not known. The engine's own exit location
	// is not loaded on a public map, which is where the second half of a
	// ticket used on another core lands.
	void GoHome(LPCHARACTER ch)
	{
		if (!ch || !ch->GetDesc() || Warping(ch))
			return;
		MarkWarping(ch);
		const long map = ch->GetQuestFlag(F_RET_MAP);
		const long x = ch->GetQuestFlag(F_RET_X);
		const long y = ch->GetQuestFlag(F_RET_Y);
		if (map > 0 && map < 10000 && !IsIslandMap(map) && x > 0 && y > 0 && ch->WarpSet(x, y))
			return;
		ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
	}

	// Out of the island.
	void Leave(LPCHARACTER ch)
	{
		if (!ch)
			return;
		ch->SetQuestFlag(F_PHASE, 0);
		ch->SetQuestFlag(F_REQ, 0);
		ch->SetQuestFlag(F_BUFFS, 0);
		SendDungeon(ch, NULL);
		if (IsIslandMap(ch->GetMapIndex()))
			GoHome(ch);
	}

	void SpawnGoblin(Instance& inst)
	{
		if (Goblin(inst))
			return;
		LPCHARACTER g = CHARACTER_MANAGER::instance().SpawnMob(GOBLIN_VNUM, inst.map, GOBLIN_X, GOBLIN_Y, 0, false, -1, true);
		if (!g)
		{
			sys_err("GOBLIN: the goblin did not spawn on %ld", inst.map);
			return;
		}
		g->SetRotationToXY(SPAWN_X, SPAWN_Y);
		g->SetMaxHP(GOBLIN_HP);
		g->SetHP(GOBLIN_HP);
		g->UpdatePacket();
		inst.goblinVid = g->GetVID();
	}

	void SpawnChest(Instance& inst)
	{
		if (inst.chestVid)
		{
			LPCHARACTER old = CHARACTER_MANAGER::instance().Find(inst.chestVid);
			if (old && !old->IsPC())
				M2_DESTROY_CHARACTER(old);
			inst.chestVid = 0;
		}
		const Path& path = PATHS[inst.path];
		const Waypoint& last = path.wp[path.count - 1];
		const int rot = (int)GetDegreeFromPositionXY((float)path.chestX, (float)path.chestY, (float)last.x, (float)last.y);
		LPCHARACTER c = CHARACTER_MANAGER::instance().SpawnMob(CHEST_VNUM, inst.map, path.chestX, path.chestY, 0, false, rot, true);
		if (c)
			inst.chestVid = c->GetVID();
	}

	void WalkTo(Instance& inst, LPCHARACTER g)
	{
		const Path& path = PATHS[inst.path];
		if (inst.wp >= path.count)
			return;
		const Waypoint& w = path.wp[inst.wp];
		g->SetNowWalking(true);
		g->SetRotationToXY(w.x, w.y);
		const bool going = g->Goto(w.x, w.y);
		g->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
		g->StartStateMachine(1);
		sys_log(0, "GOBLIN: goblin of pid %u walks path %d wp %d (%s) from %ld,%ld to %ld,%ld, %u ms%s", inst.pid,
				inst.path, inst.wp, w.type == WP_MOB ? "wave" : (w.type == WP_TURN ? "turn" : "chest"), g->GetX(), g->GetY(),
				w.x, w.y, (unsigned int)g->GetCurrentMoveDuration(), going ? "" : " (goto refused)");
		inst.walkAt = get_dword_time();
		inst.lastX = g->GetX();
		inst.lastY = g->GetY();
		inst.stuck = 0;
		inst.escort = ESCORT_WALK;
	}

	void StartPath(Instance& inst, LPCHARACTER owner, LPCHARACTER g)
	{
		inst.wp = 0;
		PurgeWave(inst);
		SpawnChest(inst);
		SetPhase(inst, owner, PHASE_ESCORT);
		if (g)
			WalkTo(inst, g);
	}

	void BeginEscort(Instance& inst, LPCHARACTER owner)
	{
		if (inst.phase != PHASE_BUFF && inst.phase != PHASE_INTRO)
			return;
		for (int i = 2; i > 0; --i)
		{
			const int j = number(0, i);
			std::swap(inst.order[i], inst.order[j]);
		}
		inst.attempted = 0;
		inst.path = inst.order[0];
		LPCHARACTER g = Goblin(inst);
		StartPath(inst, owner, g);
		Say(g, SAY_START);
		sys_log(0, "GOBLIN: %s escorts on path %d (right %d)", owner ? owner->GetName() : "?", inst.path,
				inst.correctPath);
	}

	void SpawnWave(Instance& inst, LPCHARACTER g, const Waypoint& w)
	{
		PurgeWave(inst);
		const int count = std::max(1, std::min(16, (int)w.mobs));
		for (int i = 0; i < count; ++i)
		{
			const DWORD vnum = MOB_POOL[number(0, (int)(sizeof(MOB_POOL) / sizeof(MOB_POOL[0])) - 1)];
			const long x = w.x + ((i % 4) - 1) * 150 + number(-40, 40);
			const long y = w.y + ((i / 4) - 1) * 150 + number(-40, 40);
			LPCHARACTER mob = CHARACTER_MANAGER::instance().SpawnMob(vnum, inst.map, x, y, 0, true, -1, true);
			if (!mob)
			{
				sys_err("GOBLIN: wave monster %u did not spawn at %ld,%ld on %ld", vnum, x, y, inst.map);
				continue;
			}
			mob->SetAggressive();
			if (g)
				mob->BeginFight(g);
			inst.wave.push_back(mob->GetVID());
		}
		sys_log(0, "GOBLIN: wave of %u monsters for pid %u at path %d wp %d (%ld,%ld)", (unsigned int)inst.wave.size(),
				inst.pid, inst.path, inst.wp, w.x, w.y);
	}

	// A wave's dead: a Doubloon each, as long as the purse has room.
	bool PollWave(Instance& inst, LPCHARACTER owner)
	{
		size_t alive = 0;
		for (size_t i = 0; i < inst.wave.size(); ++i)
		{
			if (!inst.wave[i])
				continue;
			LPCHARACTER mob = CHARACTER_MANAGER::instance().Find(inst.wave[i]);
			if (mob && !mob->IsDead())
			{
				++alive;
				continue;
			}
			inst.wave[i] = 0;
			if (owner)
			{
				if (Gold(owner) >= GOLD_MAX)
					Msg(owner, MSG_GOLD_FULL);
				else
				{
					SetGold(owner, Gold(owner) + 1);
					Msg(owner, MSG_GOLD_1);
				}
			}
		}
		return alive == 0;
	}

	void ArriveChest(Instance& inst, LPCHARACTER owner, LPCHARACTER g)
	{
		const Path& path = PATHS[inst.path];
		g->Stop();
		g->SetRotationToXY(path.chestX, path.chestY);
		g->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
		inst.escort = ESCORT_CHEST;
		inst.fearAt = get_dword_time();
		const bool right = inst.path == inst.correctPath;
		SetPhase(inst, owner, right ? PHASE_CHEST_SUCCESS : PHASE_CHEST_FAIL);
		if (right)
		{
			Say(g, SAY_RIGHT_CHEST);
			if (owner)
			{
				// Every fifth right chest a Goblin Key (the archive's
				// RecordChestProtection).
				const int chests = Flag(owner, F_CHESTS) + 1;
				owner->SetQuestFlag(F_CHESTS, chests);
				if (chests % KEY_EVERY_CHESTS == 0)
				{
					owner->SetQuestFlag(F_KEYS, VirtualKeys(owner) + 1);
					Msg(owner, MSG_KEY_WON, chests);
				}
			}
			LPCHARACTER chest = inst.chestVid ? CHARACTER_MANAGER::instance().Find(inst.chestVid) : NULL;
			if (chest)
				chest->SpecificEffectPacket("d:/ymir work/effect/background/metinstone_loop_yellow.mse");
		}
		else
			Say(g, SAY_WRONG_CHEST);
		if (owner)
			Msg(owner, MSG_TALK_TO_GOBLIN);
	}

	// "That was the wrong chest": everybody back to the start, the next path.
	void NextPath(Instance& inst, LPCHARACTER owner)
	{
		if (inst.phase != PHASE_CHEST_FAIL)
			return;
		inst.attempted |= (1 << inst.path);
		BYTE next = inst.correctPath;
		for (int i = 0; i < 3; ++i)
			if (!(inst.attempted & (1 << inst.order[i])))
			{
				next = inst.order[i];
				break;
			}
		inst.path = next;
		LPCHARACTER g = Goblin(inst);
		if (owner)
		{
			owner->Show(inst.map, SPAWN_X, SPAWN_Y, 0);
			owner->Stop();
		}
		if (g)
		{
			g->Stop();
			g->Show(inst.map, GOBLIN_X, GOBLIN_Y, 0);
			g->Stop();
		}
		StartPath(inst, owner, g);
		Say(g, SAY_WRONG_KEY);
		if (owner)
		{
			owner->SyncPacket();
			Msg(owner, MSG_REPLAY_ESCORT);
		}
	}

	void OpenChest(Instance& inst, LPCHARACTER owner)
	{
		if (inst.phase != PHASE_CHEST_SUCCESS || !owner)
			return;
		SetPhase(inst, owner, PHASE_REWARD);
		SetGold(owner, Gold(owner) + GOLD_CHEST);
		inst.success = true;
		Msg(owner, MSG_FOUND_BOX);
		Msg(owner, MSG_GOLD_100);
		SendInfo(owner);
		inst.leaveAt = get_dword_time() + 5000;
		sys_log(0, "GOBLIN: %s opened the chest (%d doubloons)", owner->GetName(), Gold(owner));
	}

	void Fail(Instance& inst, LPCHARACTER owner, const char* why)
	{
		if (inst.phase == PHASE_FAILED)
			return;
		SetPhase(inst, owner, PHASE_FAILED);
		PurgeWave(inst);
		if (owner)
		{
			Msg(owner, MSG_GIVEUP_1);
			Msg(owner, MSG_GIVEUP_2);
			SendDungeon(owner, NULL);
		}
		inst.leaveAt = get_dword_time() + 10000;
		sys_log(0, "GOBLIN: run of pid %u failed (%s)", inst.pid, why);
	}

	void ApplyBuffs(Instance& inst, LPCHARACTER owner, LPCHARACTER g)
	{
		const int paid = Flag(owner, F_BUFFS) & 15;
		const int fresh = paid & ~inst.buffs;
		if (!fresh || !g)
			return;
		if (fresh & 1)
		{
			// Set, not an affect: KeepGoblin holds the maximum every tick.
			g->SetMaxHP(GOBLIN_HP_BUFFED);
			g->SetHP(std::min(GOBLIN_HP_BUFFED, g->GetHP() + (GOBLIN_HP_BUFFED - GOBLIN_HP)));
		}
		if (fresh & 2)
			g->AddAffect(AFFECT_GOBLIN_DEF, POINT_DEF_GRADE, 50, 0, RUN_SECONDS + 600, 0, true);
		if (fresh & 4)
		{
			g->AddAffect(AFFECT_GOBLIN_POISON, POINT_POISON_REDUCE, 100, 0, RUN_SECONDS + 600, 0, true);
			g->RemovePoison();
		}
		if (fresh & 8)
			g->AddAffect(AFFECT_GOBLIN_SPEED, POINT_MOV_SPEED, 100, 0, RUN_SECONDS + 600, 0, true);
		inst.buffs |= fresh;
		g->UpdatePacket();
		if ((fresh & 8) && inst.phase == PHASE_ESCORT && inst.escort == ESCORT_WALK)
			WalkTo(inst, g);
		sys_log(0, "GOBLIN: %s blessed the goblin (%d)", owner->GetName(), inst.buffs);
	}

	// What the engine may have undone: the max HP and the poison immunity.
	void KeepGoblin(Instance& inst, LPCHARACTER g)
	{
		const int wantMax = (inst.buffs & 1) ? GOBLIN_HP_BUFFED : GOBLIN_HP;
		if (g->GetMaxHP() != wantMax)
		{
			g->SetMaxHP(wantMax);
			if (g->GetHP() > wantMax)
				g->SetHP(wantMax);
		}
		if (inst.buffs & 4)
		{
			g->SetImmuneFlag(g->GetMobTable().dwImmuneFlag | IMMUNE_POISON);
			if (g->IsAffectFlag(AFF_POISON) || g->FindAffect(AFFECT_POISON))
				g->RemovePoison();
		}
	}

	void Escort(Instance& inst, LPCHARACTER owner, LPCHARACTER g, DWORD now)
	{
		const Path& path = PATHS[inst.path];
		if (inst.escort == ESCORT_WALK)
		{
			if (inst.wp >= path.count)
				return;
			const Waypoint& w = path.wp[inst.wp];
			const long dx = w.x - g->GetX(), dy = w.y - g->GetY();
			const long dist2 = dx * dx + dy * dy;
			const bool moving = g->IsStateMove();
			const bool there = dist2 <= 60 * 60 || (!moving && dist2 <= 350 * 350) || (w.type == WP_TURN && dist2 <= 350 * 350);
			if (there)
			{
				if (w.type == WP_TURN)
				{
					++inst.wp;
					WalkTo(inst, g);
				}
				else if (w.type == WP_MOB)
				{
					g->Stop();
					g->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
					SpawnWave(inst, g, w);
					inst.escort = ESCORT_FIGHT;
					inst.fightAt = inst.fearAt = now;
					inst.slowSaid = false;
					g->Motion(MOTION_SPECIAL_1);
					Say(g, SAY_HELP);
				}
				else
					ArriveChest(inst, owner, g);
				return;
			}
			if (now - inst.walkAt >= 10000 && (now - inst.walkAt) % 10000 < 250)
				sys_log(0, "GOBLIN: goblin of pid %u at %ld,%ld, %ld px from wp %d, %s", inst.pid, g->GetX(), g->GetY(),
						(long)sqrt((double)dist2), inst.wp, moving ? "walking" : "standing");
			if (!moving)
			{
				if (g->GetX() == inst.lastX && g->GetY() == inst.lastY)
					++inst.stuck;
				else
				{
					inst.stuck = 0;
					inst.lastX = g->GetX();
					inst.lastY = g->GetY();
				}
				if (inst.stuck >= 4)
					WalkTo(inst, g);
			}
		}
		else if (inst.escort == ESCORT_FIGHT)
		{
			if (PollWave(inst, owner))
			{
				inst.wave.clear();
				Say(g, inst.cleared <= 0 ? SAY_THANKS : SAY_CONTINUE);
				++inst.cleared;
				++inst.wp;
				if (owner)
					SendInfo(owner);
				WalkTo(inst, g);
				return;
			}
			if (!inst.slowSaid && now - inst.fightAt >= 30000)
			{
				inst.slowSaid = true;
				Say(g, SAY_DISAPPOINTED);
			}
			if (now - inst.fearAt >= 5000)
			{
				inst.fearAt = now;
				g->Motion(MOTION_SPECIAL_1);
				Say(g, SAY_PLEAD);
			}
		}
		else if (inst.escort == ESCORT_CHEST)
		{
			if (now - inst.fearAt >= 2000)
			{
				inst.fearAt = now;
				g->Motion(MOTION_WAIT);
			}
		}
	}

	// The quest's requests (goblin_skarbow.quest).
	void Requests(Instance& inst, LPCHARACTER owner)
	{
		ApplyBuffs(inst, owner, Goblin(inst));
		const int req = owner->GetQuestFlag(F_REQ);
		if (!req)
			return;
		owner->SetQuestFlag(F_REQ, 0);
		switch (req)
		{
			case 1:
				if (inst.phase == PHASE_INTRO)
					SetPhase(inst, owner, PHASE_BUFF);
				break;
			case 2:
				BeginEscort(inst, owner);
				break;
			case 20:
				NextPath(inst, owner);
				break;
			case 22:
				OpenChest(inst, owner);
				break;
		}
		SendDungeon(owner, &inst);
	}

	// A player on the island's own map (not a copy): came from another core
	// with a ticket, or stands where nobody should.
	void OnBaseMap(LPCHARACTER ch)
	{
		if (Warping(ch) || Find(ch->GetPlayerID()))
			return;
		const long pending = ch->GetQuestFlag(F_PENDING);
		const long now = (long)get_global_time();
		ch->SetQuestFlag(F_PENDING, 0);
		if (!pending || now - pending > 180 || !IsOn())
		{
			GoHome(ch);
			return;
		}
		const long map = SECTREE_MANAGER::instance().CreatePrivateMap(MAP_INDEX);
		if (!map)
		{
			sys_err("GOBLIN: no private copy of %ld for %s", MAP_INDEX, ch->GetName());
			ch->ExitToSavedLocation();
			return;
		}
		Instance inst;
		inst.pid = ch->GetPlayerID();
		inst.map = map;
		inst.correctPath = (BYTE)number(0, 2);
		inst.createdAt = get_dword_time();
		inst.endsAt = now + RUN_SECONDS;
		s_mapInstances[inst.pid] = inst;
		// The exit the save writes: where the ticket was used, not here.
		ch->SetExitLocation(ch->GetQuestFlag(F_RET_X), ch->GetQuestFlag(F_RET_Y), ch->GetQuestFlag(F_RET_MAP));
		// Into the copy without a warp: the copy is the same map at the same
		// coordinates, so the client has nothing to load - the engine's Show
		// moves the character to the copy's sectree (as a GM goto does inside
		// a map), and the player sees one loading screen, the one to the
		// island's core. A warp here was the second loading screen (the
		// operator's test, 28 September).
		ch->Stop();
		if (!ch->Show(map, SPAWN_X, SPAWN_Y, 0))
		{
			sys_err("GOBLIN: %s could not be shown on the copy %ld", ch->GetName(), map);
			s_mapInstances.erase(inst.pid);
			QueueDestroy(map, 1000);
			GoHome(ch);
			return;
		}
		ch->Stop();
		ch->SyncPacket();
		sys_log(0, "GOBLIN: %s moved from the island's map to its copy %ld (no warp)", ch->GetName(), map);
	}

	void Arrived(Instance& inst, LPCHARACTER ch)
	{
		s_mapWarping.erase(ch->GetPlayerID());
		inst.arrived = true;
		inst.awaySince = 0;
		ch->SetQuestFlag(F_PHASE, PHASE_INTRO);
		ch->SetQuestFlag(F_REQ, 0);
		ch->SetQuestFlag(F_BUFFS, 0);
		SpawnGoblin(inst);
		SetPhase(inst, ch, PHASE_INTRO);
		inst.nextTimeNote = inst.endsAt - 25 * 60;
		Msg(ch, MSG_ESCORT_START_1);
		Msg(ch, MSG_ESCORT_START_2);
		Msg(ch, MSG_TALK_TO_GOBLIN);
		SendDungeon(ch, &inst);
		sys_log(0, "GOBLIN: %s is on the island %ld", ch->GetName(), inst.map);
	}

	void ProcessInstances(DWORD now)
	{
		const long epoch = (long)get_global_time();
		std::vector<DWORD> closing;
		for (std::map<DWORD, Instance>::iterator it = s_mapInstances.begin(); it != s_mapInstances.end(); ++it)
		{
			Instance& inst = it->second;
			LPCHARACTER owner = CHARACTER_MANAGER::instance().FindByPID(inst.pid);
			const bool here = owner && owner->GetDesc() && owner->GetMapIndex() == inst.map && owner->GetSectree();
			if (!here)
			{
				// On the way in, a minute; once in, gone for good after a few
				// seconds (a warp out, a lost connection).
				if (!inst.arrived)
				{
					if (now - inst.createdAt > 60000)
						closing.push_back(inst.pid);
				}
				else
				{
					if (!inst.awaySince)
						inst.awaySince = now;
					else if (now - inst.awaySince > 5000)
						closing.push_back(inst.pid);
				}
				continue;
			}
			if (!inst.arrived)
				Arrived(inst, owner);
			inst.awaySince = 0;
			if (inst.leaveAt)
			{
				// Again every ten seconds until the owner is gone.
				if ((int)(now - inst.leaveAt) >= 0)
				{
					inst.leaveAt = now + 10000;
					Leave(owner);
				}
				continue;
			}
			if (!IsOn())
			{
				Msg(owner, MSG_EVENT_OFF);
				Fail(inst, owner, "event over");
				continue;
			}
			if (owner->IsDead())
			{
				Fail(inst, owner, "the player died");
				continue;
			}
			LPCHARACTER g = Goblin(inst);
			if (!g)
			{
				if (inst.phase >= PHASE_INTRO && inst.phase <= PHASE_CHEST_SUCCESS && inst.goblinVid)
				{
					inst.goblinVid = 0;
					Fail(inst, owner, "the goblin died");
				}
				else if (!inst.goblinVid)
					SpawnGoblin(inst);
				continue;
			}
			if (epoch >= inst.endsAt && inst.phase != PHASE_REWARD)
			{
				Msg(owner, MSG_OVER_TIME);
				Fail(inst, owner, "time");
				continue;
			}
			if (epoch >= inst.nextTimeNote && inst.phase != PHASE_REWARD)
			{
				const long left = inst.endsAt - epoch;
				Msg(owner, MSG_REMAINING_TIME, left);
				inst.nextTimeNote = left > 5 * 60 ? epoch + 5 * 60 : (left > 60 ? inst.endsAt - 60 : inst.endsAt + 1);
			}
			KeepGoblin(inst, g);
			Requests(inst, owner);
			if (inst.phase == PHASE_ESCORT || inst.phase == PHASE_CHEST_FAIL || inst.phase == PHASE_CHEST_SUCCESS)
				Escort(inst, owner, g, now);
			if (now - inst.uiAt >= 1000)
			{
				inst.uiAt = now;
				SendDungeon(owner, &inst);
			}
		}
		for (size_t i = 0; i < closing.size(); ++i)
			Close(closing[i], "the owner left");
		// The copies nobody stands on any more.
		for (size_t i = 0; i < s_vecDestroyMaps.size(); )
		{
			if ((int)(now - s_vecDestroyMaps[i].second) < 0)
			{
				++i;
				continue;
			}
			const long map = s_vecDestroyMaps[i].first;
			bool someone = false;
			const DESC_MANAGER::DESC_SET& descs = DESC_MANAGER::instance().GetClientSet();
			for (DESC_MANAGER::DESC_SET::const_iterator d = descs.begin(); d != descs.end() && !someone; ++d)
				if ((*d)->GetCharacter() && (*d)->GetCharacter()->GetMapIndex() == map)
					someone = true;
			bool owned = false;
			for (std::map<DWORD, Instance>::iterator it = s_mapInstances.begin(); it != s_mapInstances.end(); ++it)
				if (it->second.map == map)
					owned = true;
			if (someone || owned)
			{
				s_vecDestroyMaps[i].second = now + 5000;
				++i;
				continue;
			}
			if (SECTREE_MANAGER::instance().GetMap(map))
				SECTREE_MANAGER::instance().DestroyPrivateMap(map);
			s_vecDestroyMaps.erase(s_vecDestroyMaps.begin() + i);
		}
	}

	// The ticket: this core's copy of the island, or the island's own map on
	// the core that hosts it, which sends the player on to a copy there.
	bool UseTicket(LPCHARACTER ch, LPITEM item)
	{
		if (!IsOn())
		{
			Msg(ch, MSG_EVENT_OFF);
			return true;
		}
		if (!IslandOpen())
		{
			Msg(ch, MSG_ISLAND_CLOSED);
			return true;
		}
		Sync(ch);
		if (ch->GetLevel() < ENTER_LEVEL)
		{
			Msg(ch, MSG_ENTER_LEVEL, ENTER_LEVEL);
			return true;
		}
		if (ch->GetParty())
		{
			Msg(ch, MSG_ENTER_PARTY);
			return true;
		}
		if (Gold(ch) >= GOLD_ENTER_LIMIT)
		{
			Msg(ch, MSG_ENTER_MAX_GOLD, Gold(ch));
			return true;
		}
		if (IsIslandMap(ch->GetMapIndex()) || ch->GetDungeon() || ch->GetMapIndex() >= 10000 || Find(ch->GetPlayerID()))
		{
			Msg(ch, MSG_TICKET_PLACE);
			return true;
		}
		if (!ch->CanWarp() || ch->IsDead())
			return true;
		item->SetCount(item->GetCount() - 1);
		ch->SetQuestFlag(F_PHASE, 0);
		ch->SetQuestFlag(F_REQ, 0);
		ch->SetQuestFlag(F_BUFFS, 0);
		ch->SetQuestFlag(F_RET_MAP, (int)ch->GetMapIndex());
		ch->SetQuestFlag(F_RET_X, (int)ch->GetX());
		ch->SetQuestFlag(F_RET_Y, (int)ch->GetY());
		ch->SaveExitLocation();
		MarkWarping(ch);
		if (HostsIsland())
		{
			const long map = SECTREE_MANAGER::instance().CreatePrivateMap(MAP_INDEX);
			if (!map)
			{
				ch->AutoGiveItem(TICKET_VNUM, 1);
				return true;
			}
			Instance inst;
			inst.pid = ch->GetPlayerID();
			inst.map = map;
			inst.correctPath = (BYTE)number(0, 2);
			inst.createdAt = get_dword_time();
			inst.endsAt = (long)get_global_time() + RUN_SECONDS;
			s_mapInstances[inst.pid] = inst;
			sys_log(0, "GOBLIN: %s uses a ticket, island %ld", ch->GetName(), map);
			if (!ch->WarpSet(SPAWN_X, SPAWN_Y, map))
			{
				s_mapInstances.erase(inst.pid);
				QueueDestroy(map, 1000);
				ch->AutoGiveItem(TICKET_VNUM, 1);
				s_mapWarping.erase(ch->GetPlayerID());
			}
		}
		else
		{
			ch->SetQuestFlag(F_PENDING, (int)get_global_time());
			sys_log(0, "GOBLIN: %s uses a ticket, to the island's core", ch->GetName());
			if (!ch->WarpSet(SPAWN_X, SPAWN_Y))
			{
				ch->SetQuestFlag(F_PENDING, 0);
				ch->AutoGiveItem(TICKET_VNUM, 1);
				s_mapWarping.erase(ch->GetPlayerID());
				Msg(ch, MSG_ISLAND_CLOSED);
			}
		}
		return true;
	}

	// ------------------------------------------------------------- the bots

	// The bots that take part (the events' share, playerbot_event_rules.h's
	// BotTakesPart, level 70 or more) finish a round now and then - about
	// three a day online - so the ranking has them where a real server has
	// its regulars. They play neither the island nor the board.
	void BotRounds()
	{
		if (!IsOn() || Gen() <= 0)
			return;
		const int percent = s_PlayerBotEventSettings.botsPercent;
		const DESC_MANAGER::DESC_SET& descs = DESC_MANAGER::instance().GetClientSet();
		for (DESC_MANAGER::DESC_SET::const_iterator d = descs.begin(); d != descs.end(); ++d)
		{
			LPCHARACTER ch = (*d)->GetCharacter();
			if (!ch || !(*d)->IsBot() || ch->GetLevel() < ENTER_LEVEL)
				continue;
			if (!playerbot_events::BotTakesPart(ch->GetPlayerID(), percent))
				continue;
			Sync(ch);
			if (number(1, 480) == 1)
				ch->SetQuestFlag(F_ROUND, Rounds(ch) + 1);
		}
	}

	// ------------------------------------------------------------- the clock

	// Every second on every core: a new event gets a generation of its own
	// (the leader bumps it only after five minutes without the event, so a
	// restart inside an event keeps everybody's progress), players who did
	// not hear yet are told, and the island's ticks run.
	void EverySecond(DWORD now)
	{
		const bool on = IsOn();
		quest::CQuestManager& q = quest::CQuestManager::instance();
		// The event flags come from the db core a moment after the boot; a
		// generation judged before them would start a new event on every
		// restart.
		if (!s_dwBootAt)
			s_dwBootAt = now;
		if (now - s_dwBootAt >= 60000 && IsPlayerBotEventLeader())
		{
			const int nowEpoch = (int)get_global_time();
			if (on && !s_bWasOn)
			{
				const int seen = q.GetEventFlag(E_SEEN);
				if (Gen() <= 0 || seen <= 0 || nowEpoch - seen > 300)
				{
					const int gen = std::max(Gen(), 0) + 1;
					q.RequestSetEventFlag(E_GEN, gen);
					q.RequestSetEventFlag(E_FC6, 0);
					q.RequestSetEventFlag(E_FC9, 0);
					sys_log(0, "GOBLIN: a new Treasure Hunt, generation %d", gen);
				}
			}
			if (on && (s_dwNextMinute == 0 || (int)(now - s_dwNextMinute) >= 0))
				q.RequestSetEventFlag(E_SEEN, nowEpoch);
		}
		s_bWasOn = on;
		// The state changed, or the end moved: everybody hears it again.
		if (on != s_bLastOn || EndEpoch() != s_lLastEnd)
		{
			s_bLastOn = on;
			s_lLastEnd = EndEpoch();
			s_setKnown.clear();
		}
		const DESC_MANAGER::DESC_SET& descs = DESC_MANAGER::instance().GetClientSet();
		std::set<DWORD> present;
		for (DESC_MANAGER::DESC_SET::const_iterator d = descs.begin(); d != descs.end(); ++d)
		{
			LPCHARACTER ch = (*d)->GetCharacter();
			if (!ch || (*d)->IsBot() || !(*d)->IsPhase(PHASE_GAME) || !ch->GetSectree())
				continue;
			present.insert(ch->GetPlayerID());
			if (!s_setKnown.count(ch->GetPlayerID()))
			{
				SendEvent(ch);
				if (on)
					Sync(ch);
				// Somebody on the island's copy with no run of this core's
				// (a relog into a copy that closed meanwhile is sent home by
				// the engine; a copy of another channel's) - out.
				if (IsIslandMap(ch->GetMapIndex()) && ch->GetMapIndex() >= 10000 && !Find(ch->GetPlayerID()))
					Leave(ch);
			}
			if (ch->GetMapIndex() == MAP_INDEX)
				OnBaseMap(ch);
		}
		s_setKnown.swap(present);
		if (s_dwNextMinute == 0 || (int)(now - s_dwNextMinute) >= 0)
		{
			s_dwNextMinute = now + 60000;
			BotRounds();
			for (std::map<DWORD, DWORD>::iterator it = s_mapLastCommand.begin(); it != s_mapLastCommand.end(); )
			{
				if (!CHARACTER_MANAGER::instance().FindByPID(it->first))
					s_mapLastCommand.erase(it++);
				else
					++it;
			}
			for (std::map<DWORD, DWORD>::iterator it = s_mapWarping.begin(); it != s_mapWarping.end(); )
			{
				if (now - it->second > 60000)
					s_mapWarping.erase(it++);
				else
					++it;
			}
		}
	}

	EVENTINFO(goblin_tick_info)
	{
		int dummy;
	};

	EVENTFUNC(goblin_tick)
	{
		const DWORD now = get_dword_time();
		if (!s_mapInstances.empty() || !s_vecDestroyMaps.empty())
			ProcessInstances(now);
		return PASSES_PER_SEC(1) / 4;
	}

	bool TooSoon(LPCHARACTER ch)
	{
		const DWORD now = get_dword_time();
		DWORD& last = s_mapLastCommand[ch->GetPlayerID()];
		if (last && now - last < 300)
			return true;
		last = now;
		return false;
	}
}

// Every pass of the events (playerbot_events.h), once a second at most.
void GoblinEventTick(DWORD dwNow)
{
	using namespace mt2009_goblin;
	if (!s_pkTick)
	{
		goblin_tick_info* info = AllocEventInfo<goblin_tick_info>();
		s_pkTick = event_create(goblin_tick, info, PASSES_PER_SEC(1));
	}
	if (s_dwNextSecond && (int)(dwNow - s_dwNextSecond) < 0)
		return;
	s_dwNextSecond = dwNow + 1000;
	EverySecond(dwNow);
}

// "/goblin" (server-patches/playerqol, MT2009_PLUS_GOBLIN_V1 (command)).
void GoblinCommand(LPCHARACTER ch, const char* argument)
{
	using namespace mt2009_goblin;
	if (!Eligible(ch))
		return;
	char sub[32], arg[32], arg2[32];
	const char* rest = one_argument(argument, sub, sizeof(sub));
	rest = one_argument(rest, arg, sizeof(arg));
	one_argument(rest, arg2, sizeof(arg2));
	if (strcmp(sub, "gm") && TooSoon(ch))
		return;
	sys_log(0, "GOBLIN: /goblin %s %s %s by %s (map %ld, event %d, doubloons %d, revealed %d)", sub, arg, arg2,
			ch->GetName(), ch->GetMapIndex(), IsOn() ? 1 : 0, Gold(ch), Revealed(ch) ? 1 : 0);
	if (!*sub || !strcmp(sub, "otworz"))
	{
		SendEvent(ch);
		if (!IsOn())
			return Msg(ch, MSG_EVENT_OFF);
		SendSlots(ch);
		SendInfo(ch);
		Cmd(ch, "open");
	}
	else if (!strcmp(sub, "info"))
	{
		SendEvent(ch);
		SendSlots(ch);
		SendInfo(ch);
	}
	else if (!strcmp(sub, "odkryj"))
		Reveal(ch);
	else if (!strcmp(sub, "szukaj"))
		Claim(ch);
	else if (!strcmp(sub, "reset"))
		ResetBoard(ch);
	else if (!strcmp(sub, "tury"))
	{
		Sync(ch);
		SendRounds(ch);
	}
	else if (!strcmp(sub, "tura"))
	{
		Sync(ch);
		TakeRound(ch);
	}
	else if (!strcmp(sub, "ranking"))
		SendRanking(ch);
	else if (!strcmp(sub, "wyjdz"))
	{
		Instance* inst = Find(ch->GetPlayerID());
		if (inst)
		{
			inst->leaveAt = get_dword_time();
			SetPhase(*inst, ch, PHASE_FAILED);
			PurgeWave(*inst);
		}
		else
			Leave(ch);
	}
	else if (!strcmp(sub, "gm") && ch->GetGMLevel() >= GM_HIGH_WIZARD)
	{
		int n = 0;
		str_to_number(n, arg2);
		quest::CQuestManager& q = quest::CQuestManager::instance();
		if (!strcmp(arg, "doblony"))
			SetGold(ch, n);
		else if (!strcmp(arg, "tury"))
		{
			ch->SetQuestFlag(F_ROUND, std::max(0, n));
			ch->SetQuestFlag(F_CLAIMS, 0);
			ch->SetQuestFlag(F_MASK, 0);
			ch->SetQuestFlag(F_ACC, 0);
		}
		else if (!strcmp(arg, "klucze"))
			ch->SetQuestFlag(F_KEYS, std::max(0, n));
		else if (!strcmp(arg, "reset"))
			Reset(ch);
		else if (!strcmp(arg, "wyspa"))
		{
			q.RequestSetEventFlag(E_ISLAND, n ? 0 : 1);
			ch->ChatPacket(CHAT_TYPE_INFO, "Wyspa Skarbow: %s (flaga %s).", n ? "otwarta" : "zamknieta", E_ISLAND);
		}
		else if (!strcmp(arg, "bilet"))
			ch->AutoGiveItem(TICKET_VNUM, (ITEM_COUNT)std::max(1, n));
		else if (!strcmp(arg, "wyspy"))
			ch->ChatPacket(CHAT_TYPE_INFO, "Wyspy na tym rdzeniu: %u, event %s, generacja %d, wyspa %s.",
					(unsigned int)s_mapInstances.size(), IsOn() ? "trwa" : "nie trwa", Gen(), IslandOpen() ? "otwarta" : "zamknieta");
		else
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "/goblin gm doblony|tury|klucze <n>, reset, wyspa 0|1, bilet <n>, wyspy");
			return;
		}
		SendInfo(ch);
	}
}

// MT2009_PLUS_GOBLIN_V1 (use): the ticket and the key box, before the engine's
// own use. True when the item was ours.
bool GoblinUseItem(LPCHARACTER ch, LPITEM item)
{
	using namespace mt2009_goblin;
	if (!ch || !item || !ch->IsPC())
		return false;
	const DWORD vnum = item->GetVnum();
	if (vnum == TICKET_VNUM)
	{
		if (!ch->GetDesc() || ch->GetDesc()->IsBot())
			return true;
		return UseTicket(ch, item);
	}
	if (vnum == KEY_BOX_VNUM)
	{
		const TItemTable* key = ITEM_MANAGER::instance().GetTable(KEY_VNUM);
		if (!key || ch->GetEmptyInventory(key->bSize) < 0)
		{
			Msg(ch, MSG_INVENTORY_FULL);
			return true;
		}
		item->SetCount(item->GetCount() - 1);
		ch->AutoGiveItem(KEY_VNUM, KEY_BOX_KEYS);
		Msg(ch, MSG_KEYS_FROM_BOX, KEY_BOX_KEYS);
		return true;
	}
	if (vnum == KEY_VNUM)
	{
		// The key opens the board, as its description says.
		if (ch->GetDesc() && !ch->GetDesc()->IsBot())
			GoblinCommand(ch, "");
		return true;
	}
	return false;
}

// MT2009_PLUS_GOBLIN_V1 (used): an item was used; a chest now and then gives
// a Treasure Ticket while the event runs.
void GoblinOnUse(LPCHARACTER ch, DWORD vnum)
{
	using namespace mt2009_goblin;
	if (!ch || !ch->IsPC() || !IsOn() || !IslandOpen())
		return;
	const int chance = TicketChance(vnum);
	if (chance <= 0 || number(1, 100) > chance)
		return;
	if (ch->GetDesc() && ch->GetDesc()->IsBot())
		return;
	if (ch->AutoGiveItem(TICKET_VNUM, 1))
		Msg(ch, MSG_TICKET_FOUND);
}

// MT2009_PLUS_GOBLIN_V1 (attack): the wave's monsters may strike the goblin, a
// NPC the engine keeps out of every fight.
bool GoblinAttackable(LPCHARACTER attacker, LPCHARACTER victim)
{
	return attacker && victim && !attacker->IsPC() && attacker->IsMonster() &&
			victim->GetRaceNum() == mt2009_goblin::GOBLIN_VNUM && mt2009_goblin::IsIslandMap(victim->GetMapIndex());
}

// MT2009_PLUS_GOBLIN_V1 (refine ...): the round rewards' blessings. The
// bonus in per cent for the next refine; with consume the blessing is spent.
int GoblinRefineBonus(LPCHARACTER ch, bool consume)
{
	if (!ch)
		return 0;
	CAffect* aff = ch->FindAffect(mt2009_goblin::AFFECT_REFINE_PCT);
	if (!aff)
		return 0;
	const int bonus = aff->lApplyValue > 0 ? aff->lApplyValue : 10;
	if (consume)
		ch->RemoveAffect(mt2009_goblin::AFFECT_REFINE_PCT);
	return bonus;
}

// The next refine without its materials.
bool GoblinRefineFree(LPCHARACTER ch, bool consume)
{
	if (!ch || !ch->FindAffect(mt2009_goblin::AFFECT_REFINE_FREE))
		return false;
	if (consume)
		ch->RemoveAffect(mt2009_goblin::AFFECT_REFINE_FREE);
	return true;
}
