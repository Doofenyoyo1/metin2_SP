// The player's own companion - "Towarzysz" (24 September): a bot that
// belongs to one player for good, the way a follower does in Diablo. The
// player picks its class, its sex, its path and its name in the Towarzysz
// quest; from then on it is in the player's party with the experience split
// evenly, walks at the player's side, fights what the player fights and what
// attacks the player, buffs the player, picks the player's drops up into the
// player's own bag, accepts every trade the player offers, and refines and
// shops when the player stands at the blacksmith or a merchant. It logs in and
// out with the player. "Wolna reka" lets it play like any other bot while the
// player is online; "Przywolaj" puts it back at the player's side. "Gra beze
// mnie", off unless the player sets it in the window, keeps it in the world
// when the player leaves the game, playing like any other bot and earning
// experience up to thirty levels over the player's. It never takes a duel,
// and nobody's duel is its fight.
//
// A companion is an ordinary registered bot identity - a seeded
// playerbot_NNN account, with every guard LoadRegisteredBots keeps - picked
// from the player's kingdom by the race asked for, renamed, raised to the
// player's level at its first spawn, and written into
// player.playerbot_sidekick. Nothing of the population's machinery may start,
// move or log out such an identity (CPlayerBotManager::Spawn refuses it
// unless SpawnSidekick asks, the top-up, the late joiners, the life schedule
// and the channel coordinator step over it): whether it is in the world is its
// owner's presence on this core alone.
//
// A player who fights a player is defended by the Anti-PK protocol, which
// already answers for a person in a party with bots in it
// (playerbot_anti_pk.h, BOT_FOE_PARTY) and runs above this pass in the tick.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_demon_tower.h (the fight is that file's)
// and before playerbot_companions.h, whose IsPlayerBotHeldForCompany asks it.

namespace
{
	enum EPlayerBotSidekickMode
	{
		PLAYERBOT_SIDEKICK_FOLLOW = 0,
		PLAYERBOT_SIDEKICK_FREE = 1,
	};

	// How it fights at the owner's side: the letter's "Jak ma walczyc?" and the
	// whispers "atakuj", "nie atakuj pierwszy" and "nie walcz" (25
	// September: "typu nie atakuj pierwszy").
	enum EPlayerBotSidekickStance
	{
		// Everything near the owner: the owner's target, what hits either of
		// them, and the nearest monster within PLAYERBOT_SIDEKICK_HUNT_RANGE.
		PLAYERBOT_SIDEKICK_STANCE_ATTACK = 0,
		// It starts nothing: what hits the owner or itself, and the owner's own
		// target once that is a fight already - a monster at one of the owner's
		// party, a stone somebody has begun, a guild war's foe.
		PLAYERBOT_SIDEKICK_STANCE_DEFEND = 1,
		// It hits back at what hits it and at nothing else, and takes no
		// monster off a losing owner.
		PLAYERBOT_SIDEKICK_STANCE_PASSIVE = 2,
	};

	// Why a foe was taken up, in the order FindPlayerBotSidekickFoe asks.
	enum EPlayerBotSidekickFoeWhy
	{
		PLAYERBOT_SIDEKICK_FOE_OWNER_TARGET = 0,
		PLAYERBOT_SIDEKICK_FOE_AT_OWNER = 1,
		PLAYERBOT_SIDEKICK_FOE_AT_SELF = 2,
		PLAYERBOT_SIDEKICK_FOE_NEARBY = 3,
	};

	// The table is read again this often, so a companion made on another core
	// - the owner on the second channel, or on a map another core hosts - is
	// known here by the time the owner arrives.
	const DWORD PLAYERBOT_SIDEKICK_RELOAD_MS = 30000;
	// A player who is not in this core's world for this long has logged out or
	// gone to another core, and the companion leaves with it. A warp between
	// two maps of one core is a logout and a login seconds apart, which is why
	// it is not zero.
	const DWORD PLAYERBOT_SIDEKICK_OWNER_GONE_MS = 20000;
	// "Gra beze mnie" (Burdavsky, 27 September; off unless its owner sets it
	// in the window): with its owner out of the game it stays and plays as a
	// bot of its own, and stops earning experience this many levels over its
	// owner - the engine's own party boundary (__party_can_join_by_level), so
	// the two can always hunt in one party again.
	const int PLAYERBOT_SIDEKICK_SOLO_LEVEL_LEAD = 30;
	const DWORD PLAYERBOT_SIDEKICK_SPAWN_RETRY_MS = 10000;
	// MT2009_PLUS_SIDEKICK_POOL_V1 (rename): out for longer than the db core
	// keeps a logged-out character in its cache (g_iLogoutSeconds, ten
	// minutes), so the next load reads the row with the name the owner chose.
	const DWORD PLAYERBOT_SIDEKICK_RENAME_HOLD_MS = 11 * 60 * 1000;
	// At the owner's side: it walks up past the first distance, is put beside
	// the owner past the second (a horse outran it, or a wall is in the way),
	// and hunts on its own only within the third of the owner.
	const int PLAYERBOT_SIDEKICK_FOLLOW_DISTANCE = 450;
	const int PLAYERBOT_SIDEKICK_TELEPORT_DISTANCE = 3000;
	const int PLAYERBOT_SIDEKICK_HUNT_RANGE = 1200;
	// What attacks the owner is looked for this far round the owner, and the
	// owner's own target is fought this far from the owner.
	const int PLAYERBOT_SIDEKICK_GUARD_RANGE = 1800;
	const int PLAYERBOT_SIDEKICK_ASSIST_RANGE = 2500;
	// A companion's kill counts for its owner's quests while the owner stands
	// this near the corpse (CPlayerBotManager::GetSidekickKillCredit): the reach
	// of a party's shared experience.
	const int PLAYERBOT_SIDEKICK_KILL_CREDIT_RANGE = 5000;
	// The step at which it takes a drop (how far it goes for one is
	// PLAYERBOT_SIDEKICK_LOOT_LEASH). The server hands a pick-up over from 600
	// (@fixme173), so the step stays inside it.
	const int PLAYERBOT_SIDEKICK_PICKUP_RANGE = 250;
	const DWORD PLAYERBOT_SIDEKICK_LOOT_INTERVAL_MS = 400;
	// A drop it could not take (the owner's bag full, somebody else's by then)
	// is left alone this long, or the companion stands over it for good.
	const DWORD PLAYERBOT_SIDEKICK_LOOT_FAILED_MS = 30000;
	const DWORD PLAYERBOT_SIDEKICK_LOOT_GIVE_UP_MS = 8000;
	// MT2009_PLUS_SIDEKICK_FOLLOW_LOOT_V1: at its owner's side ("Przywolaj")
	// and at a spot it keeps, the drops it goes for lie this near the owner
	// (the spot) - fifteen metres, its own kills' within the hunt range - and
	// it walks back after the pick-up.
	const int PLAYERBOT_SIDEKICK_LOOT_LEASH = 1500;
	// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: a drop held for the owner whose
	// bag is full is told at most this often.
	const DWORD PLAYERBOT_SIDEKICK_HELD_TELL_MS = 60 * 1000;
	// The blacksmith and the three merchants, when the owner stands this near
	// one of them, and how often the companion does its business there.
	const int PLAYERBOT_SIDEKICK_NPC_RANGE = 900;
	const DWORD PLAYERBOT_SIDEKICK_SERVICE_INTERVAL_MS = 15000;
	const DWORD PLAYERBOT_SIDEKICK_PARTY_CHECK_MS = 3000;
	const DWORD PLAYERBOT_SIDEKICK_CATCH_UP_MS = 3000;
	// The owner losing a fight: under this share of its health, what is hitting
	// the owner is turned onto the companion - the lure's handover of a monster
	// (playerbot_lure.h), pointed at itself - a few at a time, while the
	// companion's own health holds.
	const int PLAYERBOT_SIDEKICK_PROTECT_HP_PERCENT = 40;
	const int PLAYERBOT_SIDEKICK_PROTECT_SELF_HP_PERCENT = 30;
	const int PLAYERBOT_SIDEKICK_PROTECT_MAX_MONSTERS = 3;
	const DWORD PLAYERBOT_SIDEKICK_PROTECT_INTERVAL_MS = 1500;
	// A name: letters and digits, as a player's is on the character screen.
	const size_t PLAYERBOT_SIDEKICK_NAME_MIN = 3;
	const size_t PLAYERBOT_SIDEKICK_NAME_MAX = 16;
	// An identity saved this recently may still sit in the db core's player
	// cache (g_iPlayerCacheFlushSeconds, seven minutes), and a load from the
	// cache carries the name it had - the rename would not show until the cache
	// let it go, and a SetName after the load would leave the engine's name
	// maps (whispers, P2P) on the old one. A bot in the world saves every few
	// minutes, so this also keeps the pick away from the population's live
	// bots; one that has never played (playtime 0) was never cached at all.
	const int PLAYERBOT_SIDEKICK_IDENTITY_IDLE_MINUTES = 15;
	// A self-test switch, a file in the core's working directory: with it a bot
	// may own a companion, and the core makes one for a bot it picks, so the
	// whole machinery runs on a world with no person in it. Nothing creates the
	// file; off unless an operator does.
	const char* const PLAYERBOT_SIDEKICK_SELFTEST_FILE = "playerbot_sidekick_selftest";
	// How long the self-test waits after a start for the owner of a pair an
	// earlier run made - a bot, which may come in late in the spawn window -
	// before it gives up and makes a new pair. Every new pair renames one more
	// of the population's bots.
	const DWORD PLAYERBOT_SIDEKICK_SELFTEST_OWNER_WAIT_MS = 10 * 60 * 1000;
	// The companion's window in the client (uisidekick.py, the P key;
	// 25 September: "GUI do sterowania towarzyszem ... rozne akcje, ktore
	// mozna mu zlecac"): the version of what it is sent.
	const int PLAYERBOT_SIDEKICK_WINDOW_PROTOCOL = 1;
	// "Czekaj tutaj": how far from its spot a waiting companion follows what
	// it fights before it walks back.
	const int PLAYERBOT_SIDEKICK_HOLD_LEASH = 300;
	// "Idz na zakupy": a town visit in its own first village and back to the
	// owner. A visit not begun this long after the arrival is a town with
	// nothing to do, and one still going after the second is called off.
	const DWORD PLAYERBOT_SIDEKICK_ERRAND_START_MS = 15000;
	const DWORD PLAYERBOT_SIDEKICK_ERRAND_MAX_MS = 8 * 60 * 1000;
	// The window of its bag and skills (uisidekickinventory.py): the version of
	// what it is sent, where a worn position starts (1000 + WEAR_*), the cells
	// of one bag page, the mark of a piece its owner took off, how long the
	// fight holds off for a piece its owner put on, and how many lines one
	// answer may carry - more than a full bag and every slot, so a full picture
	// is one answer and a runaway is still bounded.
	const int PLAYERBOT_SIDEKICK_EQ_PROTOCOL = 1;
	const int PLAYERBOT_SIDEKICK_EQ_WEAR_BASE = 1000;
	// MT2009_PLUS_SIDEKICK_PANELS_V1: the companion's Alchemy window - a cell of
	// its Dragon Soul inventory is 2000 + the engine's cell, a worn stone
	// 1000 + WEAR_MAX_NUM + deck * DS_SLOT_MAX + kind (the engine's own wear
	// index past the gear), and -2 in an order is "into its Alchemy, wherever
	// the stone goes".
	const int PLAYERBOT_SIDEKICK_EQ_DS_BASE = 2000;
	const int PLAYERBOT_SIDEKICK_EQ_DS_AUTO = -2;
	const int PLAYERBOT_SIDEKICK_EQ_PAGE_CELLS = 45;
	const BYTE PLAYERBOT_SIDEKICK_PIN_UNWANTED = 255;
	const DWORD PLAYERBOT_SIDEKICK_EQUIP_WAIT_MS = 4000;
	const size_t PLAYERBOT_SIDEKICK_EQ_MAX_LINES = 260;
	// A companion that falls, is sent away or leaves on an errand in the
	// middle of a fight hands what was fighting it to its owner (the operator, 25
	// September: "Boty zaczepione przez towarzysza niech lapia tez aggro na nas
	// (np. jak nasz towarzysz zginie)"). The engine gives a monster whose
	// victim died a new one only when the monster is aggressive
	// (CHARACTER::StateBattle, FindVictim), so a pack the companion was holding
	// walked home the moment it fell, and the owner's fight ended with the
	// monsters back at their spawn. What fought it is remembered this long - by
	// the time the tick sees the companion dead some have let it go already -
	// and a monster is handed over within this distance of the owner.
	const DWORD PLAYERBOT_SIDEKICK_FOE_MEMORY_MS = 10000;
	const DWORD PLAYERBOT_SIDEKICK_FOE_MEMORY_EVERY_MS = 1000;
	const size_t PLAYERBOT_SIDEKICK_FOE_MEMORY_MAX = 32;
	const int PLAYERBOT_SIDEKICK_HANDOFF_RANGE = 2500;
	// The owner spends the stat points (the window's status page; Kiciamol,
	// 25 September: "Dodasz jeszcze mozliwosc dodawania statystyk przez
	// gracza?" - the operator: "Tak"). One order adds at most this many points: the
	// page's "+" is the player's own, whose Ctrl + click asks for a number of
	// two digits, and ninety is what one stat can hold.
	const int PLAYERBOT_SIDEKICK_STAT_ORDER_MAX = 90;
	// "Lurowanie" in the window (the operator, 25 September: "towarzysz zbiera 3
	// grupki mobow - moby zwykle sa w trojke/czworke, wiec niech jakos to
	// odroznia i zbiera spoty"). A course wakes up to this many packs round
	// the owner and walks back with them. A pack is one group as the regen
	// spawned it - a "g" or "r" line makes one CParty of three or four, and a
	// blow at one wakes the rest (CParty's PM_AGGRO_INCREASE) - and a monster
	// with no group is a pack of one, taken only when no group is in reach.
	const int PLAYERBOT_SIDEKICK_LURE_PACKS = 3;
	// Looked for this far round the owner and not nearer - what stands at the
	// owner is the owner's fight already - and one pack this far at least from
	// the spots woken before it in the course, or it is the same spot.
	const int PLAYERBOT_SIDEKICK_LURE_MIN_RANGE = 600;
	const int PLAYERBOT_SIDEKICK_LURE_MAX_RANGE = 2600;
	const int PLAYERBOT_SIDEKICK_LURE_SEPARATION = 700;
	// A pack further over the owner's level than this is left alone. The lure
	// is the owner's order, so only a pull nobody survives is refused: at 5 an
	// Archer companion lured 21% of the desert's spawns for an owner of 38 and
	// "nagle dobilismy level to zaczal lurowac" (prodnathin, 26 September); at
	// 15 an owner of 35 has 74% of it. Bosses stay out, and the 60/35% health
	// gates still turn it home.
	const int PLAYERBOT_SIDEKICK_LURE_LEVEL_OVER = 15;
	// A course begins only while this few monsters are on the owner and the
	// companion - the last pull is as good as dealt with - and while both
	// have this much of their health; it turns back under the second share.
	const int PLAYERBOT_SIDEKICK_LURE_BUSY_MONSTERS = 2;
	const int PLAYERBOT_SIDEKICK_LURE_START_HP_PERCENT = 60;
	const int PLAYERBOT_SIDEKICK_LURE_ABORT_HP_PERCENT = 35;
	// Close enough to strike, the walk to one pack, the whole course, the
	// owner walking off from where it began it, back at the owner, and the
	// wait between two courses and after a look that found nothing.
	const int PLAYERBOT_SIDEKICK_LURE_STRIKE_RANGE = 900;
	const DWORD PLAYERBOT_SIDEKICK_LURE_APPROACH_MS = 12000;
	const DWORD PLAYERBOT_SIDEKICK_LURE_COURSE_MS = 45000;
	const int PLAYERBOT_SIDEKICK_LURE_ANCHOR_LEASH = 1200;
	const int PLAYERBOT_SIDEKICK_LURE_BACK_RANGE = 350;
	const DWORD PLAYERBOT_SIDEKICK_LURE_PAUSE_MS = 3000;
	const DWORD PLAYERBOT_SIDEKICK_LURE_NOTHING_MS = 6000;
	// MT2009_PLUS_SIDEKICK_LURE_V1: how long after a course the companion
	// spends its skills on what it brought, and how many of them have to be on
	// it for that.
	const DWORD PLAYERBOT_SIDEKICK_LURE_GATHERED_MS = 25000;
	const int PLAYERBOT_SIDEKICK_LURE_GATHERED_MIN = 2;
	// MT2009_PLUS_SIDEKICK_SHAMAN_REBUFF_V1: a Shaman companion fighting from
	// a saddle (RebuffPlayerBotSidekickShaman) climbs down when a buff - its
	// own or its owner's - is gone or has this little left, looks for one
	// this often, stays on foot this long at most, waits for a cooldown this
	// short rather than ride off with the buff undone, and rides on for this
	// long before it climbs down again. A cast the engine refused is not
	// tried again for the last.
	const long PLAYERBOT_SIDEKICK_REBUFF_DUE_SECONDS = 10;
	const DWORD PLAYERBOT_SIDEKICK_REBUFF_CHECK_MS = 1000;
	const DWORD PLAYERBOT_SIDEKICK_REBUFF_MAX_MS = 15000;
	const DWORD PLAYERBOT_SIDEKICK_REBUFF_COOL_WAIT_MS = 3000;
	const DWORD PLAYERBOT_SIDEKICK_REBUFF_GAP_MS = 20000;
	const DWORD PLAYERBOT_SIDEKICK_REBUFF_FAIL_MS = 60000;
	// A skill of its path standing at seventeen without Master: a Forgetting
	// Book in its bag naming that skill is read within the first, and its
	// owner is asked for one at most once per the second, per skill
	// (ReadPlayerBotSidekickForgetBook).
	const DWORD PLAYERBOT_SIDEKICK_FORGET_CHECK_MS = 3000;
	const DWORD PLAYERBOT_SIDEKICK_FORGET_ASK_MS = 60 * 60 * 1000;
	// A bag near full (IsPlayerBotBagFull) is told to the owner at most this
	// often: at its owner's side the companion never goes to town by itself.
	const DWORD PLAYERBOT_SIDEKICK_BAG_FULL_TELL_MS = 30 * 60 * 1000;
	// Its owner's client is told which character the companion is at every
	// change and again this often, for a command a loading screen swallowed
	// (SendPlayerBotSidekickBody).
	const DWORD PLAYERBOT_SIDEKICK_BODY_RESEND_MS = 60 * 1000;
	// MT2009_PLUS_SIDEKICK_REDRESS_V1: how long after its bag has come the
	// companion is shown to its viewers anew (ResendPlayerBotSidekickView) -
	// time for the setup and the equipment pass to dress it.
	const DWORD PLAYERBOT_SIDEKICK_VIEW_RESEND_MS = 3000;
#if defined(PLAYERBOT_ENGINE_MT2009)
	// The engine's refusal names two ways past seventeen - "Uzyj Zwoju Powrotu
	// Um. lub Ksiegi Zapomnienia" - and the scroll is the ItemShop's:
	// reset_scroll.quest's 71003 takes one skill back to nothing with its
	// points (ResetOneSkill, seventeen at most) and the next skill to reach
	// seventeen turns Master for certain
	// (reset_status_items.force_to_master_skill, read by SkillLevelUp). The
	// quest asks which skill in a dialog a companion cannot answer; it takes
	// the one that is stuck.
	const DWORD PLAYERBOT_SIDEKICK_SKILL_RESET_SCROLL_VNUM = 71003;
#endif

	// What it picks up at the owner's side.
	enum EPlayerBotSidekickLoot
	{
		PLAYERBOT_SIDEKICK_LOOT_NONE = 0,
		// Only what the owner may take, which the party hands to the owner.
		PLAYERBOT_SIDEKICK_LOOT_OWNERS = 1,
		PLAYERBOT_SIDEKICK_LOOT_ALL = 2,
	};

	struct TPlayerBotSidekick
	{
		DWORD dwOwnerPID;
		DWORD dwSidekickPID;
		BYTE bMode;
		BYTE bStance;
		BYTE bLoot;
		bool bProtect;	// takes what hits a losing owner
		bool bBuffs;	// a Shaman's buffs on the owner
		bool bManualSkills;	// the owner spends its skill points, the AI none
		bool bManualStats;	// the owner spends its stat points, the AI none
		bool bStatResetUsed;	// the one reset of its stats for nothing is gone
		bool bLure;	// wakes packs round the owner and brings them over
		bool bSolo;	// "Gra beze mnie": plays on while the owner is out of the game
		bool bChests;	// "Skrzynki": opens the chests and caskets in its bag
		// "Lider grupy" (Mkls on the Discord, the operator, 28 September): the
		// companion makes the party and invites its owner, so its Leadership
		// (Dowodzenie) gives the owner the role bonus in bRole - one of
		// PARTY_ROLE_ATTACKER..PARTY_ROLE_DEFENDER, PARTY_ROLE_NORMAL for none.
		bool bLead;
		BYTE bRole;
		bool bParty;	// "Grupa": joins its owner's party whoever leads it
		BYTE bGroup;
		BYTE bLevel;
		// The owner's level: live while the owner is in this world, the
		// table's otherwise (the cap of "Gra beze mnie").
		BYTE bOwnerLevel;
		bool bSetupDone;
		// This core's clocks.
		DWORD dwOwnerSeenAt;
		DWORD dwNextSpawnTry;
		TPlayerBotSidekick()
			: dwOwnerPID(0), dwSidekickPID(0), bMode(PLAYERBOT_SIDEKICK_FOLLOW),
			  bStance(PLAYERBOT_SIDEKICK_STANCE_ATTACK), bLoot(PLAYERBOT_SIDEKICK_LOOT_ALL), bProtect(true),
			  bBuffs(true), bManualSkills(false), bManualStats(false), bStatResetUsed(false), bLure(false),
			  bSolo(false), bChests(true), bLead(false), bRole(PARTY_ROLE_NORMAL), bParty(true), bGroup(0), bLevel(1), bOwnerLevel(0), bSetupDone(true), dwOwnerSeenAt(0),
			  dwNextSpawnTry(0)
		{
		}
	};
	typedef std::map<DWORD, TPlayerBotSidekick> TPlayerBotSidekickMap;
	TPlayerBotSidekickMap s_mapPlayerBotSidekicks;			// by owner pid
	std::map<DWORD, DWORD> s_mapPlayerBotSidekickOwner;	// companion pid -> owner pid
	DWORD s_dwPlayerBotSidekickNextLoad = 0;
	DWORD s_dwPlayerBotSidekickNextManage = 0;
	bool s_bPlayerBotSidekickTable = false;
	bool s_bPlayerBotSidekickSettingsColumns = false;
	bool s_bPlayerBotSidekickSelfTest = false;
	DWORD s_dwPlayerBotSidekickNextSelfTestCheck = 0;
	DWORD s_dwPlayerBotSidekickSelfTestOwner = 0;
	DWORD s_dwPlayerBotSidekickSelfTestAt = 0;
	int s_iPlayerBotSidekickSelfTestStep = 0;
	DWORD s_dwPlayerBotSidekickSelfTestSince = 0;
	// Whether this run's town visit has begun: the order is asked again while
	// something refuses it (the companion lying down, a map it cannot leave).
	bool s_bPlayerBotSidekickSelfTestErrand = false;
	// Every town visit an owner's order began, so the self-test can tell a
	// begun one from a refused one.
	unsigned int s_uPlayerBotSidekickErrands = 0;
	// The self-test of the window alone (the file's first line "eq"), and the
	// piece it hands over and takes back.
	bool s_bPlayerBotSidekickSelfTestEq = false;
	DWORD s_dwPlayerBotSidekickSelfTestGiftID = 0;
	// The piece the window's self-test took off onto a chosen cell, and the
	// slot a gift was dropped straight onto.
	DWORD s_dwPlayerBotSidekickSelfTestPieceID = 0;
	int s_iPlayerBotSidekickSelfTestGiftWear = -1;
	const int PLAYERBOT_SIDEKICK_EQ_SELFTEST_STEPS = 15;
	// The self-test of the stats, the lure and the handover (the file's first
	// line "lure"): its owner - a bot - is put on a hunting spot of its village
	// and stands there as a player standing on a spot does, until the step
	// that lets the companion go.
	bool s_bPlayerBotSidekickSelfTestLure = false;
	const int PLAYERBOT_SIDEKICK_LURE_SELFTEST_STEPS = 17;
	const int PLAYERBOT_SIDEKICK_LURE_SELFTEST_FREE_STEP = 14;
	bool s_bPlayerBotSidekickPinTable = false;
	// MT2009_PLUS_SIDEKICK_POOL_V1: the companions' own pool of identities
	// (player.playerbot_sidekick_pool) - never played, so never in the db
	// core's player cache, and kept out of the population's world the way a
	// companion is (IsPlayerBotSidekickPID). "Nie ma teraz wolnej postaci tej
	// klasy" (Note, 1 October) was every identity of the race in the world or
	// saved within the last quarter of an hour - on a world of a thousand bots,
	// all of them; and an identity taken from the population came in under the
	// name the db core's cache still held for it.
	std::set<DWORD> s_setPlayerBotSidekickPool;
	bool s_bPlayerBotSidekickPoolTable = false;
	DWORD s_dwPlayerBotSidekickNextPoolFill = 0;
	// MT2009_PLUS_SIDEKICK_POOL_V1 (rename): a companion that came in under
	// another name than its row's - the db core's cache of the population's
	// bot it was - logs out once and stays out until the cache has let it go
	// (g_iLogoutSeconds, ten minutes), and comes back under its own. Asked of
	// a companion once per core run, so a cache that would not let go cannot
	// make it come and go for ever.
	std::set<DWORD> s_setPlayerBotSidekickRenameRelog;
	std::set<DWORD> s_setPlayerBotSidekickRenameTried;
	// MT2009_PLUS_SIDEKICK_COINS_V1: "Smocze Monety: wydaje / nie wydaje" in
	// the window's Options page, by companion pid; on unless the owner set it
	// off. Kept in player.playerbot_sidekick.coins.
	std::map<DWORD, bool> s_mapPlayerBotSidekickCoins;
	bool s_bPlayerBotSidekickCoinsColumn = false;
	// MT2009_PLUS_SIDEKICK_EQUIP_LOCK_V1: "Zablokuj ekwipunek" in the window's
	// Options page - the companions whose owner locked their gear, by pid (a
	// companion is in only while its lock is on, so the passes of every bot
	// ask an empty map). Kept in player.playerbot_sidekick.equipment_lock.
	std::set<DWORD> s_setPlayerBotSidekickEquipLock;
	bool s_bPlayerBotSidekickEquipLockColumn = false;
	// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: "Pelne EQ" in the window's
	// Options page (on by default) - with its owner's bag too full for a drop
	// of the owner's, the companion picks that drop up into its own bag and
	// holds it there for the owner. The companions whose owner switched it off,
	// by pid; kept in player.playerbot_sidekick.keep_loot. What it holds so is
	// marked in player.playerbot_sidekick_gift.held.
	std::set<DWORD> s_setPlayerBotSidekickNoKeep;
	bool s_bPlayerBotSidekickKeepColumn = false;
	bool s_bPlayerBotSidekickHeldColumn = false;

	// MT2009_PLUS_SIDEKICK_SHOP_ERRAND_V1: "Kup" in the window - the owner's
	// errand for one kind of goods (playerbot_sidekick_shop.h). Kept in the
	// runtime: an errand cut off by a logout leaves what it bought held in the
	// bag (playerbot_sidekick_gift.held) and the owner's yang in its purse.
	struct TPlayerBotSidekickShopErrand
	{
		bool bActive = false;
		BYTE bStage = 0;
		BYTE bGood = 0;
		int iWanted = 0;
		int iGot = 0;
		// The owner's yang it was handed for this, and its purse right after.
		long long llEscrow = 0;
		long long llGoldStart = 0;
		// Where it buys: the merchant's counter, or the stand it walks to.
		long lMap = 0;
		long lX = 0;
		long lY = 0;
		DWORD dwSince = 0;
		DWORD dwStageSince = 0;
		DWORD dwShopOwner = 0;
		DWORD dwShopItem = 0;
		// A purchase from a stand the db core has not answered yet: the line,
		// its count and price, and the purse just before it was asked for.
		DWORD dwPendingItem = 0;
		int iPendingCount = 0;
		long long llPendingPrice = 0;
		long long llGoldBeforeBuy = 0;
		DWORD dwPendingSince = 0;
		std::set<DWORD> setTried;
		std::vector<DWORD> vecItems;
		const char* szWhy = "";
	};

	// What a companion carries between ticks that nobody else needs.
	struct TPlayerBotSidekickRuntime
	{
		DWORD dwNextPartyCheck;
		DWORD dwNextService;
		DWORD dwNextLoot;
		DWORD dwNextCatchUp;
		DWORD dwLootVID;
		DWORD dwLootSince;
		DWORD dwNextProtect;
		bool bTrading;
		std::set<DWORD> setBagBeforeTrade;
		// What the owner handed it: kept, never sold (IsPlayerBotSidekickGift).
		std::set<DWORD> setGifts;
		// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: the owner's drops it picked up
		// while the owner's bag was full - gifts as well, and more: the AI does
		// not put them on, refine, rework, open or spend them
		// (IsPlayerBotSidekickHeld).
		std::set<DWORD> setHeld;
		DWORD dwHeldToldAt;	// when the owner last heard of a drop held for it
		std::map<DWORD, DWORD> mapLootFailed;	// item vid -> until
		// The foes it took up, by why (EPlayerBotSidekickFoeWhy): the self-test
		// reads them to see a stance hold.
		DWORD dwLastFoeVID;
		DWORD adwFoes[4];
		// "Czekaj tutaj": the spot it keeps until it is called. Not kept over a
		// logout - it comes back at its owner's side.
		bool bHold;
		long lHoldMap;
		long lHoldX;
		long lHoldY;
		// "Idz na zakupy": the town visit it was sent on, whether one began,
		// and the purse it left with.
		bool bErrand;
		bool bErrandVisit;
		DWORD dwErrandSince;
		long long llErrandGold;
		// What the window last got of its gear.
		DWORD dwGearSent;
		// The owner's hand on its gear (the window): item id -> the wear slot
		// the owner put it on, or PLAYERBOT_SIDEKICK_PIN_UNWANTED for a piece
		// the owner took off.
		std::map<DWORD, BYTE> mapPins;
		// What the bag window was last sent, by position, the batch number and
		// the yang beside it.
		std::map<int, DWORD> mapEqSent;
		DWORD dwEqGen;
		long long llEqGoldSent;
		// A piece the owner put on while a blow was fresh: the fight holds off
		// until it is on or this passes.
		DWORD dwEquipWaitUntil;
		// When it last saw its owner in a fight (IsPlayerBotSidekickOwnerFighting).
		DWORD dwOwnerFightSeenAt;
		// What fought it lately (monster vid -> when), handed to the owner when
		// it falls or leaves (HandPlayerBotSidekickFoesToOwner).
		std::map<DWORD, DWORD> mapFoesOnSelf;
		DWORD dwNextFoeMemory;
		// A lure course (ManagePlayerBotSidekickLure): 0 none, 1 walking out
		// to a pack, 2 walking back with what it woke; the pack it walks to,
		// how many it woke, where the owner stood when it began, the spots it
		// woke, and its clocks.
		BYTE bLureStage;
		DWORD dwLureVID;
		int iLurePacks;
		int iLureMonsters;
		long lLureAnchorX;
		long lLureAnchorY;
		std::vector<std::pair<long, long> > vecLureSpots;
		DWORD dwLureCourseSince;
		DWORD dwLureStageSince;
		DWORD dwNextLure;
		unsigned int uLureCourses;
		// MT2009_PLUS_SIDEKICK_LURE_V1: the pack brought home is fought with
		// the skills until this, while two or more of it are on the companion.
		DWORD dwLureGatheredUntil = 0;
		// The Forgetting Book (ReadPlayerBotSidekickForgetBook): its clock, when
		// the owner was last asked for one, by skill, and the books it holds for
		// a skill that is not stuck, told once.
		DWORD dwNextForgetCheck;
		std::map<DWORD, DWORD> mapForgetAskedAt;
		std::set<DWORD> setForgetToldItems;
		// When it last told its owner its bag was near full.
		DWORD dwBagFullToldAt;
		// Playing on its own while its owner is out of the game ("Gra beze
		// mnie"; StartPlayerBotSidekickAlone).
		bool bAlone;
		// Sent fishing ("Na ryby"; SendPlayerBotSidekickFishing): at the water
		// for as long as its Fishing Card lasts, and since when.
		bool bFishing;
		DWORD dwFishingSince;
		// MT2009_PLUS_SIDEKICK_REDRESS_V1: when it is shown to its viewers again
		// after its first dressing (ResendPlayerBotSidekickView), and whether it
		// has been this time in the world.
		DWORD dwViewResendAt;
		bool bViewResent;
		// MT2009_PLUS_SIDEKICK_SERVICE_LOG_V1: what the last service at the
		// npcs did, so an owner idling at the blacksmith is not logged every
		// fifteen seconds (13 lines in 3 minutes with the purse unchanged,
		// 1 October).
		long long llLastServiceGold = -1;
		DWORD dwLastServiceLog = 0;
		// MT2009_PLUS_SIDEKICK_SHOP_ERRAND_V1: the owner's "Kup" errand.
		TPlayerBotSidekickShopErrand shop;
		// MT2009_PLUS_SIDEKICK_SHAMAN_REBUFF_V1: a Shaman off its saddle for
		// the buffs until this (0 when it is not), its next cast, its next look
		// from the saddle, and the casts the engine refused (vnum * 2, + 1 for
		// its own) -> until when they are left alone.
		DWORD dwRebuffUntil = 0;
		DWORD dwRebuffNextCast = 0;
		DWORD dwNextRebuffCheck = 0;
		std::map<DWORD, DWORD> mapRebuffFailedUntil;
		// MT2009_PLUS_SIDEKICK_KEEP_VALUABLES_V1: the owner's valuables in its
		// bag (WatchPlayerBotSidekickValuables) - its next look, when the owner
		// last heard of them, and which pieces the owner has heard of.
		DWORD dwNextValuablesCheck = 0;
		DWORD dwValuablesToldAt = 0;
		std::set<DWORD> setValuablesTold;
		TPlayerBotSidekickRuntime()
			: dwNextPartyCheck(0), dwNextService(0), dwNextLoot(0), dwNextCatchUp(0), dwLootVID(0),
			  dwLootSince(0), dwNextProtect(0), bTrading(false), dwLastFoeVID(0), bHold(false), lHoldMap(0),
			  lHoldX(0), lHoldY(0), bErrand(false), bErrandVisit(false), dwErrandSince(0), llErrandGold(0),
			  dwGearSent(0), dwEqGen(0), llEqGoldSent(-1), dwEquipWaitUntil(0), dwOwnerFightSeenAt(0),
			  dwNextFoeMemory(0), bLureStage(0), dwLureVID(0), iLurePacks(0), iLureMonsters(0), lLureAnchorX(0),
			  lLureAnchorY(0), dwLureCourseSince(0), dwLureStageSince(0), dwNextLure(0), uLureCourses(0),
			  dwNextForgetCheck(0), dwBagFullToldAt(0), bAlone(false), bFishing(false), dwFishingSince(0),
			  dwViewResendAt(0), bViewResent(false)
		{
			memset(adwFoes, 0, sizeof(adwFoes));
			dwHeldToldAt = 0;	// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1
		}
	};
	std::map<DWORD, TPlayerBotSidekickRuntime> s_mapPlayerBotSidekickRuntime;

	// MT2009_PLUS_SIDEKICK_SHOP_ERRAND_V1: the owner's "Kup" errand, defined in
	// playerbot_sidekick_shop.h (included right after this file).
	void OrderPlayerBotSidekickShopErrand(LPCHARACTER owner, TPlayerBotSidekick& rec, const char* key,
			const char* countText, const char* confirm, DWORD dwNow);
	bool ManagePlayerBotSidekickShopErrand(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt, DWORD dwNow);
	void SettlePlayerBotSidekickShopErrand(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt, DWORD dwNow, const char* why, bool comeBack);
	void PollPlayerBotSidekickShopPurchase(LPCHARACTER ch, TPlayerBotSidekick& rec, TPlayerBotSidekickRuntime& rt,
			DWORD dwNow);
	bool DescribePlayerBotSidekickShopErrand(const TPlayerBotSidekickRuntime& rt, char* out, size_t size);

	// The world's switch: M2_SIDEKICK=0 in .env (the launcher's difficulty
	// window) is the event flag m2_sidekick_off, which the migrator writes at a
	// start. Off, nobody's companion comes into the world and one already in
	// it logs out, the letter is not sent (towarzysz.quest) and the command
	// says why. Asked of the event flag every time, a map lookup.
	bool IsPlayerBotSidekickSwitchedOff()
	{
		return quest::CQuestManager::instance().GetEventFlag("m2_sidekick_off") != 0;
	}

	// ------------------------------------------------------------ the record

	bool EnsurePlayerBotSidekickTable()
	{
		if (s_bPlayerBotSidekickTable)
			return true;
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(
				"CREATE TABLE IF NOT EXISTS player.playerbot_sidekick ("
				"owner_pid INT UNSIGNED NOT NULL PRIMARY KEY, "
				"sidekick_pid INT UNSIGNED NOT NULL, "
				"mode TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"stance TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"loot TINYINT UNSIGNED NOT NULL DEFAULT 2, "
				"protect TINYINT UNSIGNED NOT NULL DEFAULT 1, "
				"buffs TINYINT UNSIGNED NOT NULL DEFAULT 1, "
				"skill_group TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"start_level TINYINT UNSIGNED NOT NULL DEFAULT 1, "
				"setup_done TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"created_at DATETIME NOT NULL, "
				"UNIQUE KEY sidekick (sidekick_pid)) ENGINE=InnoDB"));
		std::unique_ptr<SQLMsg> gifts(AccountDB::instance().DirectQuery(
				"CREATE TABLE IF NOT EXISTS player.playerbot_sidekick_gift ("
				"item_id INT UNSIGNED NOT NULL PRIMARY KEY, "
				"sidekick_pid INT UNSIGNED NOT NULL, "
				"given_at DATETIME NOT NULL, "
				"KEY sidekick (sidekick_pid)) ENGINE=InnoDB"));
		s_bPlayerBotSidekickTable = msg.get() && msg->uiSQLErrno == 0 && gifts.get() && gifts->uiSQLErrno == 0;
		if (!s_bPlayerBotSidekickTable)
		{
			sys_err("PLAYERBOT_SIDEKICK: the companion tables cannot be created");
			return false;
		}
		// The stance and the window's settings came a day after the table, which
		// the test world already held; without the columns the companions still
		// load, with the defaults, and keep what they are told while the core
		// runs. The stat points, the stat reset and the lure came the day
		// after that, in the same statement, "Gra beze mnie" (solo) and
		// "Skrzynki" (chests) on 27 September, and "Grupa" (party) on the 28th.
		std::unique_ptr<SQLMsg> settings(AccountDB::instance().DirectQuery(
				"ALTER TABLE player.playerbot_sidekick "
				"ADD COLUMN IF NOT EXISTS stance TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER mode, "
				"ADD COLUMN IF NOT EXISTS loot TINYINT UNSIGNED NOT NULL DEFAULT 2 AFTER stance, "
				"ADD COLUMN IF NOT EXISTS protect TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER loot, "
				"ADD COLUMN IF NOT EXISTS buffs TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER protect, "
				"ADD COLUMN IF NOT EXISTS manual_skills TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER buffs, "
				"ADD COLUMN IF NOT EXISTS manual_stats TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER manual_skills, "
				"ADD COLUMN IF NOT EXISTS stat_reset TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER manual_stats, "
				"ADD COLUMN IF NOT EXISTS lure TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER stat_reset, "
				"ADD COLUMN IF NOT EXISTS solo TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER lure, "
				"ADD COLUMN IF NOT EXISTS chests TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER solo, "
				"ADD COLUMN IF NOT EXISTS lead TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER chests, "
				"ADD COLUMN IF NOT EXISTS role TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER lead, "
				"ADD COLUMN IF NOT EXISTS party TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER role"));
		s_bPlayerBotSidekickSettingsColumns = settings.get() && settings->uiSQLErrno == 0;
		if (!s_bPlayerBotSidekickSettingsColumns)
			sys_err("PLAYERBOT_SIDEKICK: no settings columns errno=%u", settings.get() ? settings->uiSQLErrno : 0U);
		// What the owner put on or took off in the window. Without the table
		// the marks hold while the core runs.
		std::unique_ptr<SQLMsg> pins(AccountDB::instance().DirectQuery(
				"CREATE TABLE IF NOT EXISTS player.playerbot_sidekick_pin ("
				"item_id INT UNSIGNED NOT NULL PRIMARY KEY, "
				"sidekick_pid INT UNSIGNED NOT NULL, "
				"wear TINYINT UNSIGNED NOT NULL, "
				"pinned_at DATETIME NOT NULL, "
				"KEY sidekick (sidekick_pid)) ENGINE=InnoDB"));
		s_bPlayerBotSidekickPinTable = pins.get() && pins->uiSQLErrno == 0;
		if (!s_bPlayerBotSidekickPinTable)
			sys_err("PLAYERBOT_SIDEKICK: no pin table errno=%u", pins.get() ? pins->uiSQLErrno : 0U);
		// MT2009_PLUS_SIDEKICK_POOL_V1: the companions' pool. Without it a
		// companion is picked out of the population as before.
		std::unique_ptr<SQLMsg> pool(AccountDB::instance().DirectQuery(
				"CREATE TABLE IF NOT EXISTS player.playerbot_sidekick_pool ("
				"pid INT UNSIGNED NOT NULL PRIMARY KEY, "
				"race TINYINT UNSIGNED NOT NULL, "
				"reserved_at DATETIME NOT NULL, "
				"KEY race (race)) ENGINE=InnoDB"));
		s_bPlayerBotSidekickPoolTable = pool.get() && pool->uiSQLErrno == 0;
		if (!s_bPlayerBotSidekickPoolTable)
			sys_err("PLAYERBOT_SIDEKICK: no companion pool table errno=%u", pool.get() ? pool->uiSQLErrno : 0U);
		// MT2009_PLUS_SIDEKICK_COINS_V1: "Smocze Monety: wydaje / nie wydaje".
		std::unique_ptr<SQLMsg> coins(AccountDB::instance().DirectQuery(
				"ALTER TABLE player.playerbot_sidekick "
				"ADD COLUMN IF NOT EXISTS coins TINYINT UNSIGNED NOT NULL DEFAULT 1"));
		s_bPlayerBotSidekickCoinsColumn = coins.get() && coins->uiSQLErrno == 0;
		if (!s_bPlayerBotSidekickCoinsColumn)
			sys_err("PLAYERBOT_SIDEKICK: no coins column errno=%u", coins.get() ? coins->uiSQLErrno : 0U);
		// MT2009_PLUS_SIDEKICK_EQUIP_LOCK_V1: "Zablokuj ekwipunek", off by default.
		std::unique_ptr<SQLMsg> equipLock(AccountDB::instance().DirectQuery(
				"ALTER TABLE player.playerbot_sidekick "
				"ADD COLUMN IF NOT EXISTS equipment_lock TINYINT UNSIGNED NOT NULL DEFAULT 0"));
		s_bPlayerBotSidekickEquipLockColumn = equipLock.get() && equipLock->uiSQLErrno == 0;
		if (!s_bPlayerBotSidekickEquipLockColumn)
			sys_err("PLAYERBOT_SIDEKICK: no equipment_lock column errno=%u",
					equipLock.get() ? equipLock->uiSQLErrno : 0U);
		// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: "Pelne EQ", on by default, and
		// the mark of what it holds for its owner.
		std::unique_ptr<SQLMsg> keepLoot(AccountDB::instance().DirectQuery(
				"ALTER TABLE player.playerbot_sidekick "
				"ADD COLUMN IF NOT EXISTS keep_loot TINYINT UNSIGNED NOT NULL DEFAULT 1"));
		s_bPlayerBotSidekickKeepColumn = keepLoot.get() && keepLoot->uiSQLErrno == 0;
		if (!s_bPlayerBotSidekickKeepColumn)
			sys_err("PLAYERBOT_SIDEKICK: no keep_loot column errno=%u", keepLoot.get() ? keepLoot->uiSQLErrno : 0U);
		std::unique_ptr<SQLMsg> held(AccountDB::instance().DirectQuery(
				"ALTER TABLE player.playerbot_sidekick_gift "
				"ADD COLUMN IF NOT EXISTS held TINYINT UNSIGNED NOT NULL DEFAULT 0"));
		s_bPlayerBotSidekickHeldColumn = held.get() && held->uiSQLErrno == 0;
		if (!s_bPlayerBotSidekickHeldColumn)
			sys_err("PLAYERBOT_SIDEKICK: no held column errno=%u", held.get() ? held->uiSQLErrno : 0U);
		// A companion whose owner's character was deleted would be kept out of
		// the population for good: its record goes, and the identity plays on
		// as the bot it was, under the name it was given.
		std::unique_ptr<SQLMsg> orphans(AccountDB::instance().DirectQuery(
				"DELETE s FROM player.playerbot_sidekick AS s LEFT JOIN player.player AS p ON p.id=s.owner_pid "
				"WHERE p.id IS NULL"));
		if (orphans.get() && orphans->uiSQLErrno == 0 && orphans->Get() && orphans->Get()->uiAffectedRows > 0)
			sys_log(0, "PLAYERBOT_SIDEKICK: dropped %u companions of deleted characters",
					(unsigned int)orphans->Get()->uiAffectedRows);
		return true;
	}

	void LoadPlayerBotSidekicks()
	{
		if (!EnsurePlayerBotSidekickTable())
			return;
		// The owner's level from its row, which is where an owner out of the
		// game is (the cap of "Gra beze mnie"), and the chest and party switches
		// after it.
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(s_bPlayerBotSidekickSettingsColumns
				? "SELECT s.owner_pid, s.sidekick_pid, s.mode, s.skill_group, s.start_level, s.setup_done, s.stance, "
				  "s.loot, s.protect, s.buffs, s.manual_skills, s.manual_stats, s.stat_reset, s.lure, s.solo, "
				  "(SELECT p.level FROM player.player AS p WHERE p.id=s.owner_pid), s.chests, s.lead, s.role, s.party "
				  "FROM player.playerbot_sidekick AS s"
				: "SELECT s.owner_pid, s.sidekick_pid, s.mode, s.skill_group, s.start_level, s.setup_done, "
				  "0, 2, 1, 1, 0, 0, 0, 0, 0, (SELECT p.level FROM player.player AS p WHERE p.id=s.owner_pid), 1, 0, 0, 1 "
				  "FROM player.playerbot_sidekick AS s"));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return;
		TPlayerBotSidekickMap fresh;
		std::map<DWORD, DWORD> owners;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			TPlayerBotSidekick rec;
			unsigned int mode = 0, group = 0, level = 1, done = 0, stance = 0, loot = 2, protect = 1, buffs = 1,
					manual = 0, manualStats = 0, statReset = 0, lure = 0, solo = 0, ownerLevel = 0, chests = 1,
					lead = 0, role = 0, party = 1;
			if (row[0]) str_to_number(rec.dwOwnerPID, row[0]);
			if (row[1]) str_to_number(rec.dwSidekickPID, row[1]);
			if (row[2]) str_to_number(mode, row[2]);
			if (row[3]) str_to_number(group, row[3]);
			if (row[4]) str_to_number(level, row[4]);
			if (row[5]) str_to_number(done, row[5]);
			if (row[6]) str_to_number(stance, row[6]);
			if (row[7]) str_to_number(loot, row[7]);
			if (row[8]) str_to_number(protect, row[8]);
			if (row[9]) str_to_number(buffs, row[9]);
			if (row[10]) str_to_number(manual, row[10]);
			if (row[11]) str_to_number(manualStats, row[11]);
			if (row[12]) str_to_number(statReset, row[12]);
			if (row[13]) str_to_number(lure, row[13]);
			if (row[14]) str_to_number(solo, row[14]);
			if (row[15]) str_to_number(ownerLevel, row[15]);
			if (row[16]) str_to_number(chests, row[16]);
			if (row[17]) str_to_number(lead, row[17]);
			if (row[18]) str_to_number(role, row[18]);
			if (row[19]) str_to_number(party, row[19]);
			if (rec.dwOwnerPID == 0 || rec.dwSidekickPID == 0)
				continue;
			rec.bMode = mode == PLAYERBOT_SIDEKICK_FREE ? PLAYERBOT_SIDEKICK_FREE : PLAYERBOT_SIDEKICK_FOLLOW;
			rec.bStance = (BYTE)std::min<unsigned int>(stance, PLAYERBOT_SIDEKICK_STANCE_PASSIVE);
			rec.bLoot = (BYTE)std::min<unsigned int>(loot, PLAYERBOT_SIDEKICK_LOOT_ALL);
			rec.bProtect = protect != 0;
			rec.bBuffs = buffs != 0;
			rec.bManualSkills = manual != 0;
			rec.bManualStats = manualStats != 0;
			rec.bStatResetUsed = statReset != 0;
			rec.bLure = lure != 0;
			rec.bSolo = solo != 0;
			rec.bChests = chests != 0;
			rec.bLead = lead != 0;
			rec.bRole = (role >= PARTY_ROLE_ATTACKER && role <= PARTY_ROLE_DEFENDER) ? (BYTE)role : (BYTE)PARTY_ROLE_NORMAL;
			rec.bParty = party != 0;
			rec.bGroup = (BYTE)std::min<unsigned int>(group, 2);
			rec.bLevel = (BYTE)std::max<unsigned int>(1, std::min<unsigned int>(level, 255));
			rec.bOwnerLevel = (BYTE)std::min<unsigned int>(ownerLevel, 255);
			rec.bSetupDone = done != 0;
			TPlayerBotSidekickMap::const_iterator old = s_mapPlayerBotSidekicks.find(rec.dwOwnerPID);
			if (old != s_mapPlayerBotSidekicks.end() && old->second.dwSidekickPID == rec.dwSidekickPID)
			{
				rec.dwOwnerSeenAt = old->second.dwOwnerSeenAt;
				rec.dwNextSpawnTry = old->second.dwNextSpawnTry;
				// The setup this core did is newer than the row it wrote.
				rec.bSetupDone = rec.bSetupDone || old->second.bSetupDone;
				// The level this core saw is newer than the row: the db core
				// writes a character's row out of its cache minutes after the
				// fact, and a level never goes down.
				rec.bOwnerLevel = std::max(rec.bOwnerLevel, old->second.bOwnerLevel);
				// With no columns to keep them in, the settings live as long as
				// the core.
				if (!s_bPlayerBotSidekickSettingsColumns)
				{
					rec.bStance = old->second.bStance;
					rec.bLoot = old->second.bLoot;
					rec.bProtect = old->second.bProtect;
					rec.bBuffs = old->second.bBuffs;
					rec.bManualSkills = old->second.bManualSkills;
					rec.bManualStats = old->second.bManualStats;
					rec.bStatResetUsed = old->second.bStatResetUsed;
					rec.bLure = old->second.bLure;
					rec.bSolo = old->second.bSolo;
					rec.bChests = old->second.bChests;
					rec.bLead = old->second.bLead;
					rec.bRole = old->second.bRole;
					rec.bParty = old->second.bParty;
				}
			}
			fresh[rec.dwOwnerPID] = rec;
			owners[rec.dwSidekickPID] = rec.dwOwnerPID;
		}
		s_mapPlayerBotSidekicks.swap(fresh);
		s_mapPlayerBotSidekickOwner.swap(owners);
		// MT2009_PLUS_SIDEKICK_POOL_V1: the pool another core may have filled
		// or drawn from since the last read.
		if (s_bPlayerBotSidekickPoolTable)
		{
			std::unique_ptr<SQLMsg> pool(AccountDB::instance().DirectQuery(
					"SELECT pid FROM player.playerbot_sidekick_pool"));
			if (pool.get() && pool->uiSQLErrno == 0 && pool->Get() && pool->Get()->pSQLResult)
			{
				std::set<DWORD> pids;
				MYSQL_ROW poolRow;
				while (NULL != (poolRow = mysql_fetch_row(pool->Get()->pSQLResult)))
				{
					DWORD pid = 0;
					if (poolRow[0])
						str_to_number(pid, poolRow[0]);
					if (pid != 0)
						pids.insert(pid);
				}
				s_setPlayerBotSidekickPool.swap(pids);
			}
		}
		// MT2009_PLUS_SIDEKICK_COINS_V1: the coins switch, by companion.
		if (s_bPlayerBotSidekickCoinsColumn)
		{
			std::unique_ptr<SQLMsg> coins(AccountDB::instance().DirectQuery(
					"SELECT sidekick_pid, coins FROM player.playerbot_sidekick"));
			if (coins.get() && coins->uiSQLErrno == 0 && coins->Get() && coins->Get()->pSQLResult)
			{
				MYSQL_ROW coinRow;
				while (NULL != (coinRow = mysql_fetch_row(coins->Get()->pSQLResult)))
				{
					DWORD pid = 0;
					unsigned int on = 1;
					if (coinRow[0])
						str_to_number(pid, coinRow[0]);
					if (coinRow[1])
						str_to_number(on, coinRow[1]);
					if (pid != 0)
						s_mapPlayerBotSidekickCoins[pid] = on != 0;
				}
			}
		}
		// MT2009_PLUS_SIDEKICK_EQUIP_LOCK_V1: the locked companions.
		if (s_bPlayerBotSidekickEquipLockColumn)
		{
			std::unique_ptr<SQLMsg> locks(AccountDB::instance().DirectQuery(
					"SELECT sidekick_pid FROM player.playerbot_sidekick WHERE equipment_lock<>0"));
			if (locks.get() && locks->uiSQLErrno == 0 && locks->Get() && locks->Get()->pSQLResult)
			{
				std::set<DWORD> locked;
				MYSQL_ROW lockRow;
				while (NULL != (lockRow = mysql_fetch_row(locks->Get()->pSQLResult)))
				{
					DWORD pid = 0;
					if (lockRow[0])
						str_to_number(pid, lockRow[0]);
					if (pid != 0)
						locked.insert(pid);
				}
				s_setPlayerBotSidekickEquipLock.swap(locked);
			}
		}
		// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: the companions with "Pelne EQ" off.
		if (s_bPlayerBotSidekickKeepColumn)
		{
			std::unique_ptr<SQLMsg> keeps(AccountDB::instance().DirectQuery(
					"SELECT sidekick_pid FROM player.playerbot_sidekick WHERE keep_loot=0"));
			if (keeps.get() && keeps->uiSQLErrno == 0 && keeps->Get() && keeps->Get()->pSQLResult)
			{
				std::set<DWORD> off;
				MYSQL_ROW keepRow;
				while (NULL != (keepRow = mysql_fetch_row(keeps->Get()->pSQLResult)))
				{
					DWORD pid = 0;
					if (keepRow[0])
						str_to_number(pid, keepRow[0]);
					if (pid != 0)
						off.insert(pid);
				}
				s_setPlayerBotSidekickNoKeep.swap(off);
			}
		}
	}

	bool IsPlayerBotSidekickPID(DWORD pid)
	{
		// MT2009_PLUS_SIDEKICK_POOL_V1: an identity of the companions' pool is
		// nobody's companion yet and nobody's bot either - Spawn refuses it,
		// and the population's queues step over it, by this one question.
		return s_mapPlayerBotSidekickOwner.find(pid) != s_mapPlayerBotSidekickOwner.end() ||
				s_setPlayerBotSidekickPool.find(pid) != s_setPlayerBotSidekickPool.end();
	}

	// A bot that owns a companion - only under the self-test, since a person's
	// party is none of the party pass's business anyway: its party is the
	// companion's, and the cohort rule must not break it up every minute.
	bool IsPlayerBotSidekickOwnerPID(DWORD pid)
	{
		return s_bPlayerBotSidekickSelfTest && s_mapPlayerBotSidekicks.find(pid) != s_mapPlayerBotSidekicks.end();
	}

	TPlayerBotSidekick* FindPlayerBotSidekickOf(DWORD sidekickPid)
	{
		std::map<DWORD, DWORD>::const_iterator owner = s_mapPlayerBotSidekickOwner.find(sidekickPid);
		if (owner == s_mapPlayerBotSidekickOwner.end())
			return NULL;
		TPlayerBotSidekickMap::iterator rec = s_mapPlayerBotSidekicks.find(owner->second);
		return rec == s_mapPlayerBotSidekicks.end() ? NULL : &rec->second;
	}

	// "Skrzynki: nie" in the window: the chest pass (ManagePlayerBotChests)
	// leaves this companion's chests and caskets closed, for its owner to take
	// out of its bag and open (xxkld., 27 September: the Moonlight chests it
	// opened put stones in its bag its owner could not use).
	bool IsPlayerBotSidekickKeepingChests(LPCHARACTER ch)
	{
		if (!ch || s_mapPlayerBotSidekickOwner.empty())
			return false;
		const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		return rec && !rec->bChests;
	}

	// MT2009_PLUS_PICKUP_FILTER_V1 (bots): the owner whose pick-up filter
	// (Ctrl+Z) this companion keeps, on this core; NULL for any other bot.
	LPCHARACTER GetPlayerBotSidekickFilterOwner(LPCHARACTER ch)
	{
		if (!ch || s_mapPlayerBotSidekickOwner.empty())
			return NULL;
		const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		return rec ? CHARACTER_MANAGER::instance().FindByPID(rec->dwOwnerPID) : NULL;
	}

	// MT2009_PLUS_SIDEKICK_LOOT_OFF_V1: "Nic" means nothing (upstream 2.2.44,
	// kuszaa and Tyrion). The window's setting held only at the owner's side,
	// where this file's own pass picks the drops up; let off the leash ("Wolna
	// reka") the ordinary loot pass ran for the companion as for any bot - and,
	// still in its owner's party, took the owner's drops through the engine's
	// party branch of PickupItem - and so it did in the moments after a
	// summons. HandleLoot, the combat pick-up and the loot count ask this first.
	bool IsPlayerBotSidekickLootOff(LPCHARACTER ch)
	{
		if (!ch || s_mapPlayerBotSidekickOwner.empty())
			return false;
		const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		return rec && rec->bLoot == PLAYERBOT_SIDEKICK_LOOT_NONE;
	}

	// The owner, when it is a person in this core's world (or a bot, under the
	// self-test).
	LPCHARACTER GetPlayerBotSidekickOwnerChar(DWORD ownerPid)
	{
		LPCHARACTER owner = CHARACTER_MANAGER::instance().FindByPID(ownerPid);
		if (!owner || !owner->IsPC() || !owner->GetDesc())
			return NULL;
		if (owner->GetDesc()->IsBot() && !s_bPlayerBotSidekickSelfTest)
			return NULL;
		return owner;
	}

	// Its owner out of the game altogether: in no core's world - P2P_MANAGER
	// knows every character the other cores hold - and gone from this one for
	// longer than a warp between two of its maps takes.
	bool IsPlayerBotSidekickOwnerOut(const TPlayerBotSidekick& rec, DWORD dwNow)
	{
		if (GetPlayerBotSidekickOwnerChar(rec.dwOwnerPID))
			return false;
		// A record made in this very pass carries a clock later than dwNow.
		if (rec.dwOwnerSeenAt != 0 &&
				(dwNow < rec.dwOwnerSeenAt || dwNow - rec.dwOwnerSeenAt < PLAYERBOT_SIDEKICK_OWNER_GONE_MS))
			return false;
		return P2P_MANAGER::instance().FindByPID(rec.dwOwnerPID) == NULL;
	}

	// "Gra beze mnie" in force: its owner out of the game, it plays on as a
	// bot of its own (ManagePlayerBotSidekick hands it to the population's
	// passes, ManagePlayerBotSidekicks keeps it in the world).
	bool IsPlayerBotSidekickPlayingAlone(const TPlayerBotSidekick& rec, DWORD dwNow)
	{
		return rec.bSolo && IsPlayerBotSidekickOwnerOut(rec, dwNow);
	}

	// The level it stops earning experience at while it plays alone
	// (ManagePlayerBotExpLock), or 0 when it does not: its owner's plus the
	// party boundary. An owner whose level is not known holds it where it is.
	BYTE GetPlayerBotSidekickSoloLockLevel(LPCHARACTER ch, DWORD dwNow)
	{
		const TPlayerBotSidekick* rec = ch ? FindPlayerBotSidekickOf(ch->GetPlayerID()) : NULL;
		if (!rec || !IsPlayerBotSidekickPlayingAlone(*rec, dwNow))
			return 0;
		if (rec->bOwnerLevel == 0)
			return 1;
		return (BYTE)std::min<int>(255, (int)rec->bOwnerLevel + PLAYERBOT_SIDEKICK_SOLO_LEVEL_LEAD);
	}

	const TPlayerBotSidekickRuntime* FindPlayerBotSidekickRuntime(DWORD sidekickPid)
	{
		std::map<DWORD, TPlayerBotSidekickRuntime>::const_iterator rt = s_mapPlayerBotSidekickRuntime.find(sidekickPid);
		return rt == s_mapPlayerBotSidekickRuntime.end() ? NULL : &rt->second;
	}

	// Sent to town by its owner ("Idz na zakupy"): the ordinary town visit
	// runs it, so for that while it is nobody's to hold back.
	bool IsPlayerBotSidekickOnErrand(DWORD sidekickPid)
	{
		const TPlayerBotSidekickRuntime* rt = FindPlayerBotSidekickRuntime(sidekickPid);
		return rt && rt->bErrand;
	}

	// Told to wait where it stands ("Czekaj tutaj").
	bool IsPlayerBotSidekickHolding(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		const TPlayerBotSidekickRuntime* rt = FindPlayerBotSidekickRuntime(ch->GetPlayerID());
		return rt && rt->bHold;
	}

	// A companion at its owner's side: in follow mode, the owner in this core's
	// world. Everything that asks "is this bot somebody's" asks this too
	// (IsPlayerBotHeldForCompany), so none of the errands take it away - but
	// the one its owner sent it on.
	bool IsPlayerBotSidekickLeashed(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		return rec && rec->bMode == PLAYERBOT_SIDEKICK_FOLLOW && GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID) != NULL &&
				!IsPlayerBotSidekickOnErrand(ch->GetPlayerID());
	}

	// Whose companion this is, for the line over its head
	// (BuildPlayerBotStatusText): the owner's name while it is at the owner's
	// side, nothing otherwise - let off the leash it plays, and says so, like
	// any bot.
	// Its owner in this core's world, if there: SpawnSidekick logs the
	// companion in with the owner's kingdom.
	LPCHARACTER GetPlayerBotSidekickOwnerHere(DWORD sidekickPid)
	{
		const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(sidekickPid);
		return rec ? GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID) : NULL;
	}

	const char* GetPlayerBotSidekickOwnerName(LPCHARACTER ch)
	{
		if (!ch || s_mapPlayerBotSidekickOwner.empty())
			return NULL;
		const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		if (!rec || rec->bMode != PLAYERBOT_SIDEKICK_FOLLOW || IsPlayerBotSidekickOnErrand(ch->GetPlayerID()))
			return NULL;
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID);
		return owner ? owner->GetName() : NULL;
	}

	// Standing at its owner's side, which the inactivity watchdog reads as
	// stillness on purpose: an owner who stands about, stands about with it -
	// and one told to wait at a spot waits there.
	bool IsPlayerBotSidekickBesideOwner(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		if (!rec || rec->bMode != PLAYERBOT_SIDEKICK_FOLLOW || IsPlayerBotSidekickOnErrand(ch->GetPlayerID()))
			return false;
		if (IsPlayerBotSidekickHolding(ch))
			return true;
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID);
		return owner && owner->GetMapIndex() == ch->GetMapIndex() &&
				DISTANCE_APPROX(owner->GetX() - ch->GetX(), owner->GetY() - ch->GetY()) <=
						PLAYERBOT_SIDEKICK_TELEPORT_DISTANCE;
	}

	// Whose companion this is, when the asker is the owner or the leader of
	// the owner's party: the only invitations a companion takes.
	bool IsPlayerBotSidekickInviteFromOwner(LPCHARACTER ch, LPCHARACTER leader)
	{
		const TPlayerBotSidekick* rec = ch ? FindPlayerBotSidekickOf(ch->GetPlayerID()) : NULL;
		if (!rec || !leader)
			return false;
		if (leader->GetPlayerID() == rec->dwOwnerPID)
			return true;
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID);
		return owner && owner->GetParty() && owner->GetParty()->GetLeaderPID() == leader->GetPlayerID();
	}

	bool IsPlayerBotSidekickGift(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || s_mapPlayerBotSidekickRuntime.empty())
			return false;
		std::map<DWORD, TPlayerBotSidekickRuntime>::const_iterator rt =
				s_mapPlayerBotSidekickRuntime.find(ch->GetPlayerID());
		return rt != s_mapPlayerBotSidekickRuntime.end() &&
				rt->second.setGifts.find(item->GetID()) != rt->second.setGifts.end();
	}

	// The owner's mark on a piece of its companion's (the window, below): the
	// wear slot the owner put it on, PLAYERBOT_SIDEKICK_PIN_UNWANTED for one
	// the owner took off, -1 for none. Asked by the equipment, refine, bonus
	// and junk passes of every bot, so the empty case is one test.
	// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: the owner's drop it picked up
	// for the owner whose bag was full.
	bool IsPlayerBotSidekickHeld(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || s_mapPlayerBotSidekickRuntime.empty())
			return false;
		std::map<DWORD, TPlayerBotSidekickRuntime>::const_iterator rt = s_mapPlayerBotSidekickRuntime.find(ch->GetPlayerID());
		return rt != s_mapPlayerBotSidekickRuntime.end() &&
				rt->second.setHeld.find(item->GetID()) != rt->second.setHeld.end();
	}

	// "Pelne EQ": on unless its owner switched it off, and only while it is
	// somebody's companion.
	bool IsPlayerBotSidekickKeepingLoot(DWORD pid)
	{
		return s_mapPlayerBotSidekickOwner.find(pid) != s_mapPlayerBotSidekickOwner.end() &&
				s_setPlayerBotSidekickNoKeep.find(pid) == s_setPlayerBotSidekickNoKeep.end();
	}

	int GetPlayerBotSidekickPinOf(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || s_mapPlayerBotSidekickRuntime.empty())
			return -1;
		std::map<DWORD, TPlayerBotSidekickRuntime>::const_iterator rt =
				s_mapPlayerBotSidekickRuntime.find(ch->GetPlayerID());
		if (rt == s_mapPlayerBotSidekickRuntime.end() || rt->second.mapPins.empty())
			return -1;
		std::map<DWORD, BYTE>::const_iterator pin = rt->second.mapPins.find(item->GetID());
		return pin == rt->second.mapPins.end() ? -1 : (int)pin->second;
	}

	// MT2009_PLUS_SIDEKICK_EQUIP_LOCK_V1: "Zablokuj ekwipunek" (blipu's,
	// 2 October). With the lock on, what the companion wears and what its
	// owner gave it are the owner's: no refine, no stone, no bonus change, no
	// crafting or alchemy, not taken off by the AI for something better, never
	// sold or thrown away. The owner's own hand in the bag window still moves
	// anything (those orders do not ask this).
	bool IsPlayerBotSidekickEquipLocked(DWORD pid)
	{
		// Only while it is somebody's companion: a dismissed one plays on as
		// an ordinary bot.
		return !s_setPlayerBotSidekickEquipLock.empty() &&
				s_setPlayerBotSidekickEquipLock.find(pid) != s_setPlayerBotSidekickEquipLock.end() &&
				s_mapPlayerBotSidekickOwner.find(pid) != s_mapPlayerBotSidekickOwner.end();
	}

	bool IsPlayerBotSidekickLockedItem(LPCHARACTER ch, LPITEM item)
	{
		// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: what it holds for its owner is
		// the owner's, lock or no lock.
		if (IsPlayerBotSidekickHeld(ch, item))
			return true;
		if (!ch || !item || !IsPlayerBotSidekickEquipLocked(ch->GetPlayerID()))
			return false;
		return item->IsEquipped() || IsPlayerBotSidekickGift(ch, item);
	}

	// Put on by its owner: kept on, never refined, its lines never changed -
	// and, under the equipment lock, everything it wears or was given.
	bool IsPlayerBotSidekickPinned(LPCHARACTER ch, LPITEM item)
	{
		const int pin = GetPlayerBotSidekickPinOf(ch, item);
		if (pin >= 0 && pin != PLAYERBOT_SIDEKICK_PIN_UNWANTED)
			return true;
		return IsPlayerBotSidekickLockedItem(ch, item);
	}

	// Taken off by its owner: the AI never puts it back on.
	bool IsPlayerBotSidekickUnwanted(LPCHARACTER ch, LPITEM item)
	{
		// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: nor what it holds for its owner.
		if (IsPlayerBotSidekickHeld(ch, item))
			return true;
		return GetPlayerBotSidekickPinOf(ch, item) == PLAYERBOT_SIDEKICK_PIN_UNWANTED;
	}

	// MT2009_PLUS_SIDEKICK_KEEP_VALUABLES_V1 ("Dropnalem KD+4, polecialo na
	// towarzysza. Poszedl sprzedac smieci, sprzedal wszystkie KD+4. Zostawil
	// w eq KD+1/2/3", the owner, 5 October). The owner's kill drops one item
	// in turn to each party member near it (CParty::GetNextOwnership), so the
	// companion picks up the owner's drops as its own loot, not only the ones
	// it holds for a full bag (IsPlayerBotSidekickHeld) - and every rule of a
	// bot's bag ran on them: the storekeeper's pass of its shopping errand put
	// the soul stones +4 the LPP list keeps (and the materials, books and keys
	// of a bag under pressure, a companion having no counter) in its own box,
	// where the owner never sees them; the Alchemist made dust of its stones
	// +0..+2. A companion's bag is its owner's now: only plain scrap worth
	// next to nothing ever leaves it by the AI's hand.
	const DWORD PLAYERBOT_SIDEKICK_JUNK_MAX_VALUE = 50000;	// yang a piece, the market's price
	// Plain gear priced as scrap (twice the merchant's pay, GetPlayerBotShopAskingPrice)
	// is scrap whatever its level; past this many times the merchant's pay the
	// market prices it for something else.
	const DWORD PLAYERBOT_SIDEKICK_JUNK_SCRAP_MULT = 3;
	const DWORD PLAYERBOT_SIDEKICK_VALUABLES_CHECK_MS = 20 * 1000;
	const DWORD PLAYERBOT_SIDEKICK_VALUABLES_TELL_MS = 3 * 60 * 1000;

	std::set<DWORD> s_setPlayerBotSidekickBoxSwept;
	void SayPlayerBotSidekick(LPCHARACTER owner, const char* text);

	bool IsPlayerBotSidekickServing(LPCHARACTER ch)
	{
		return ch && !s_mapPlayerBotSidekickOwner.empty() &&
				s_mapPlayerBotSidekickOwner.find(ch->GetPlayerID()) != s_mapPlayerBotSidekickOwner.end();
	}

	// What a companion may let go of: plain gear (no lines, under +4, no
	// stone in a socket, none of the kinds kept whatever their plus), the
	// water's catch and a spare tool - and only at the market's price of
	// scrap. Never what its owner handed it, holds for it or put on it; never
	// a soul stone, a material, a book, a scroll, a chest, a key, a Cor, a
	// quest item, a costume, a potion or anything else.
	bool IsPlayerBotSidekickSellableJunk(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !item->GetProto() || item->IsEquipped() || item->isLocked())
			return false;
		if (IsPlayerBotSidekickGift(ch, item) || IsPlayerBotSidekickHeld(ch, item) ||
				IsPlayerBotSidekickPinned(ch, item))
			return false;
		const BYTE type = item->GetType();
		const DWORD count = std::max<DWORD>(1, (DWORD)item->GetCount());
		if (type == ITEM_WEAPON || type == ITEM_ARMOR)
		{
			if (item->GetAttributeCount() > 0 || item->GetRefineLevel() >= PLAYERBOT_PRECIOUS_REFINE ||
					IsPlayerBotSpecialLevel30Weapon(item) || IsPlayerBotStalkiItem(item) ||
					IsPlayerBotAwakeningGoods(item->GetVnum()))
				return false;
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				if (IsPlayerBotSoulStoneVnum((DWORD)item->GetSocket(i)))
					return false;
			const DWORD unit = GetPlayerBotShopAskingPrice(item) / count;
			const DWORD npc = GetPlayerBotNpcSellUnitPrice(item);
			return unit <= std::max<DWORD>(PLAYERBOT_SIDEKICK_JUNK_MAX_VALUE, npc * PLAYERBOT_SIDEKICK_JUNK_SCRAP_MULT);
		}
		if (type == ITEM_FISH || type == ITEM_ROD || type == ITEM_PICK)
			return GetPlayerBotShopAskingPrice(item) / count <= PLAYERBOT_SIDEKICK_JUNK_MAX_VALUE;
		return false;
	}

	// What the AI of a companion does not spend - a soul stone seated, a Cor
	// opened: what it holds for its owner, what the lock keeps, and whatever
	// did not come from its owner's hand (the owner's kill drops among it).
	bool IsPlayerBotSidekickKeptForOwner(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return false;
		if (IsPlayerBotSidekickLockedItem(ch, item))
			return true;
		return IsPlayerBotSidekickServing(ch) && !IsPlayerBotSidekickGift(ch, item);
	}

	// What its owner would want to know it carries: the kinds of value a
	// player keeps, and gear with lines, a plus from +4 or a stone in it -
	// not what its owner handed it or put on it, nor what it wears.
	bool IsPlayerBotSidekickOwnerValuable(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !item->GetProto() || item->IsEquipped() || IsPlayerBotSidekickGift(ch, item) ||
				IsPlayerBotSidekickPinned(ch, item))
			return false;
		if (IsPlayerBotSidekickHeld(ch, item))
			return true;
		const DWORD vnum = item->GetVnum();
		switch (item->GetType())
		{
			case ITEM_METIN:
			case ITEM_MATERIAL:
			case ITEM_SKILLBOOK:
			case ITEM_SKILLFORGET:
			case ITEM_TREASURE_BOX:
			case ITEM_TREASURE_KEY:
			case ITEM_GIFTBOX:
			case ITEM_QUEST:
			case ITEM_POLYMORPH:
				return true;
			case ITEM_WEAPON:
			case ITEM_ARMOR:
				if (item->GetSubType() == WEAPON_ARROW && item->GetType() == ITEM_WEAPON)
					return false;
				return !IsPlayerBotSidekickSellableJunk(ch, item) &&
						(item->GetAttributeCount() > 0 || item->GetRefineLevel() >= PLAYERBOT_PRECIOUS_REFINE ||
						 IsPlayerBotSpecialLevel30Weapon(item) || IsPlayerBotAwakeningGoods(vnum));
			case ITEM_COSTUME:
				return IsPlayerBotSashVnum(vnum);
			case ITEM_USE:
				return IsPlayerBotRefineScroll(vnum) || IsPlayerBotGeneralSkillBook(vnum) ||
						IsPlayerBotExtraSkillBook(vnum) || item->GetSubType() == USE_ADD_ATTRIBUTE ||
						item->GetSubType() == USE_CHANGE_ATTRIBUTE || item->GetSubType() == USE_ADD_ATTRIBUTE2;
			default:
				break;
		}
		return IsPlayerBotCorDraconisVnum(vnum) || IsPlayerBotAwakeningGoods(vnum) || item->IsDragonSoul();
	}

	bool PlayerBotSidekickWantsBoxSweep(LPCHARACTER ch)
	{
		return IsPlayerBotSidekickServing(ch) &&
				s_setPlayerBotSidekickBoxSwept.find(ch->GetPlayerID()) == s_setPlayerBotSidekickBoxSwept.end();
	}

	// After the withdrawal of its errand's storekeeper visit: an empty box is
	// not looked into again this start; what the bag had no room for is told
	// to the owner and waits for the next errand.
	void NotePlayerBotSidekickBoxSwept(LPCHARACTER ch, CSafebox* box)
	{
		if (!IsPlayerBotSidekickServing(ch) || !box)
			return;
		std::set<LPITEM> left;
		for (DWORD pos = 0; pos < SAFEBOX_MAX_NUM; ++pos)
			if (box->IsValidPosition(pos) && box->Get(pos))
				left.insert(box->Get(pos));
		const DWORD ownerPid = s_mapPlayerBotSidekickOwner[ch->GetPlayerID()];
		sys_log(0, "PLAYERBOT_SIDEKICK: box swept pid=%u owner=%u left=%u free=%d", ch->GetPlayerID(), ownerPid,
				(unsigned int)left.size(), CountPlayerBotFreeInventoryCells(ch));
		if (left.empty())
		{
			s_setPlayerBotSidekickBoxSwept.insert(ch->GetPlayerID());
			return;
		}
		char text[192];
		snprintf(text, sizeof(text), "W moim magazynie zostalo jeszcze %u rzeczy - wyjme je przy nastepnych zakupach. "
				"Zabieraj ode mnie, co twoje (okno Towarzysza), zebym mial na nie miejsce.", (unsigned int)left.size());
		SayPlayerBotSidekick(GetPlayerBotSidekickOwnerChar(ownerPid), text);
	}

	// A piece its owner put on that waits in the bag - refused for the moment
	// (a blow in the last second and a half), or taken off by something since
	// (a fishing session with the leash let go) - with the slot it goes to.
	// askEngine: only one the engine would let on now.
	LPITEM FindPlayerBotSidekickPinnedInBag(LPCHARACTER ch, int& wearCell, bool askEngine)
	{
		wearCell = -1;
		if (!ch || s_mapPlayerBotSidekickRuntime.empty())
			return NULL;
		std::map<DWORD, TPlayerBotSidekickRuntime>::const_iterator rt =
				s_mapPlayerBotSidekickRuntime.find(ch->GetPlayerID());
		if (rt == s_mapPlayerBotSidekickRuntime.end() || rt->second.mapPins.empty())
			return NULL;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell)
				continue;
			std::map<DWORD, BYTE>::const_iterator pin = rt->second.mapPins.find(item->GetID());
			if (pin == rt->second.mapPins.end() || pin->second == PLAYERBOT_SIDEKICK_PIN_UNWANTED ||
					pin->second >= WEAR_MAX_NUM)
				continue;
			LPITEM worn = ch->GetWear(pin->second);
			if (worn && IS_SET(worn->GetFlag(), ITEM_FLAG_IRREMOVABLE))
				continue;
			if (askEngine && (item->isLocked() || item->IsExchanging() ||
					!PlayerBotCanEquipNow(ch, item, TItemPos(INVENTORY, cell))))
				continue;
			wearCell = pin->second;
			return item;
		}
		return NULL;
	}

	// The owner spends its skill points (the window's "reczne"), and the skill
	// pass leaves them alone.
	bool IsPlayerBotSidekickManualSkills(LPCHARACTER ch)
	{
		if (!ch || s_mapPlayerBotSidekickOwner.empty())
			return false;
		const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		return rec && rec->bManualSkills;
	}

	// The owner spends its stat points (the window's "Statystyki rozdaje sam"),
	// and the stat pass (ManagePlayerBotStats) leaves them alone.
	bool IsPlayerBotSidekickManualStats(LPCHARACTER ch)
	{
		if (!ch || s_mapPlayerBotSidekickOwner.empty())
			return false;
		const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		return rec && rec->bManualStats;
	}

	void SetPlayerBotSidekickFlag(DWORD ownerPid, const char* flag, int value)
	{
		quest::PC* pc = quest::CQuestManager::instance().GetPC(ownerPid);
		if (pc)
			pc->SetFlag(flag, value);
	}

	// MT2009_PLUS_SIDEKICK_WHISPER_V1: the companion's word to its owner is a
	// whisper from it, as a player's would be - its name on the whisper
	// window, and the owner's answer goes straight back to it (its orders are
	// whispered anyway) - not an info line in the chat ("Wszystkie
	// powiadomienia towarzysza ... niech beda wysylane szeptem", the owner,
	// 5 October). The name: the companion in this world, else another core's
	// line for it, else its row (it may not be in the game yet - "za chwile
	// bede w grze"); kept per companion.
	const DWORD PLAYERBOT_SIDEKICK_WHISPER_REPEAT_MS = 20000;
	std::map<DWORD, std::string> s_mapPlayerBotSidekickWhisperName;	// companion pid -> name
	std::map<DWORD, std::map<std::string, DWORD> > s_mapPlayerBotSidekickWhisperSent;	// owner pid -> text -> when

	const char* GetPlayerBotSidekickWhisperName(DWORD ownerPid)
	{
		TPlayerBotSidekickMap::const_iterator rec = s_mapPlayerBotSidekicks.find(ownerPid);
		if (rec == s_mapPlayerBotSidekicks.end() || rec->second.dwSidekickPID == 0)
			return NULL;
		const DWORD pid = rec->second.dwSidekickPID;
		std::string& name = s_mapPlayerBotSidekickWhisperName[pid];
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(pid);
		CCI* peer = sk ? NULL : P2P_MANAGER::instance().FindByPID(pid);
		if (sk)
			name = sk->GetName();
		else if (peer)
			name = peer->szName;
		else if (name.empty())
		{
			char query[128];
			snprintf(query, sizeof(query), "SELECT name FROM player.player WHERE id=%u", pid);
			std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
			MYSQL_ROW row = NULL;
			if (msg.get() && msg->uiSQLErrno == 0 && msg->Get() && msg->Get()->pSQLResult &&
					(row = mysql_fetch_row(msg->Get()->pSQLResult)) && row[0])
				name = row[0];
		}
		return name.empty() ? NULL : name.c_str();
	}

	// The same text again within PLAYERBOT_SIDEKICK_WHISPER_REPEAT_MS is not
	// whispered twice: a refusal asked for every second, an order clicked
	// twice.
	bool IsPlayerBotSidekickWhisperRepeat(DWORD ownerPid, const char* text, DWORD dwNow)
	{
		std::map<std::string, DWORD>& sent = s_mapPlayerBotSidekickWhisperSent[ownerPid];
		if (sent.size() > 32)
			for (std::map<std::string, DWORD>::iterator it = sent.begin(); it != sent.end();)
			{
				if (dwNow - it->second >= PLAYERBOT_SIDEKICK_WHISPER_REPEAT_MS)
					sent.erase(it++);
				else
					++it;
			}
		std::map<std::string, DWORD>::iterator it = sent.find(text);
		if (it != sent.end() && dwNow - it->second < PLAYERBOT_SIDEKICK_WHISPER_REPEAT_MS)
			return true;
		sent[text] = dwNow;
		return false;
	}

	// The packet CInputMain::Whisper gives a player's whisper, under the
	// companion's name (SendPlayerBotWhisperPacket, which wants the companion
	// in this world).
	void SendPlayerBotSidekickWhisperPacket(LPCHARACTER owner, const char* name, const char* text)
	{
		const size_t len = std::min<size_t>(strlen(text), CHAT_MAX_LEN);
		TPacketGCWhisper pack;
		pack.bHeader = HEADER_GC_WHISPER;
		pack.bType = WHISPER_TYPE_NORMAL;
		pack.wSize = (WORD)(sizeof(TPacketGCWhisper) + len);
		strlcpy(pack.szNameFrom, name, sizeof(pack.szNameFrom));
		TEMP_BUFFER tmpbuf;
		tmpbuf.write(&pack, sizeof(pack));
		tmpbuf.write(text, (int)len);
		owner->GetDesc()->Packet(tmpbuf.read_peek(), tmpbuf.size());
	}

	void SayPlayerBotSidekick(LPCHARACTER owner, const char* text)
	{
		if (!text || !*text)
			return;
		if (owner && owner->GetDesc() && !owner->GetDesc()->IsBot())
		{
			const DWORD dwNow = get_dword_time();
			if (IsPlayerBotSidekickWhisperRepeat(owner->GetPlayerID(), text, dwNow))
				return;
			const char* name = GetPlayerBotSidekickWhisperName(owner->GetPlayerID());
			if (name)
				SendPlayerBotSidekickWhisperPacket(owner, name, text);
			else
				owner->ChatPacket(CHAT_TYPE_INFO, "[Towarzysz] %s", text);	// no companion of record to speak
		}
		// The self-test's owner is a bot with no client, and an order it gave
		// that was refused would otherwise leave no trace at all.
		else if (owner && s_bPlayerBotSidekickSelfTest)
			sys_log(0, "PLAYERBOT_SIDEKICK: says owner=%u \"%s\"", owner->GetPlayerID(), text);
	}

	const char* GetPlayerBotSidekickStanceName(BYTE stance)
	{
		switch (stance)
		{
			case PLAYERBOT_SIDEKICK_STANCE_DEFEND: return "nie atakuje pierwszy";
			case PLAYERBOT_SIDEKICK_STANCE_PASSIVE: return "nie walczy, tylko sie broni";
			default: return "atakuje wszystko w poblizu";
		}
	}

	// ------------------------------------------------ its fight, when it goes

	// What is fighting the companion now: the monsters whose victim it is,
	// round it.
	struct FPlayerBotSidekickAttackers
	{
		LPCHARACTER self;
		std::vector<DWORD> vids;

		explicit FPlayerBotSidekickAttackers(LPCHARACTER s) : self(s)
		{
		}

		void operator()(LPENTITY ent)
		{
			if (vids.size() >= PLAYERBOT_SIDEKICK_FOE_MEMORY_MAX || !ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER c = (LPCHARACTER)ent;
			if (c != self && !c->IsDead() && c->IsMonster() && c->GetVictim() == self)
				vids.push_back((DWORD)c->GetVID());
		}
	};

	void NotePlayerBotSidekickAttackers(LPCHARACTER ch, TPlayerBotSidekickRuntime& rt, DWORD dwNow)
	{
		if (!ch->GetSectree())
			return;
		FPlayerBotSidekickAttackers attackers(ch);
		ch->GetSectree()->ForEachAround(attackers);
		for (size_t i = 0; i < attackers.vids.size(); ++i)
			rt.mapFoesOnSelf[attackers.vids[i]] = dwNow;
	}

	// Once a second at its owner's side: what fights it now joins what fought
	// it lately, and what fought it long ago is forgotten. A stamp later than
	// the pass's clock (a command in the same pass) is not old.
	void RememberPlayerBotSidekickAttackers(LPCHARACTER ch, TPlayerBotSidekickRuntime& rt, DWORD dwNow)
	{
		if (dwNow < rt.dwNextFoeMemory)
			return;
		rt.dwNextFoeMemory = dwNow + PLAYERBOT_SIDEKICK_FOE_MEMORY_EVERY_MS;
		for (std::map<DWORD, DWORD>::iterator it = rt.mapFoesOnSelf.begin(); it != rt.mapFoesOnSelf.end();)
		{
			if (dwNow > it->second && dwNow - it->second > PLAYERBOT_SIDEKICK_FOE_MEMORY_MS)
				rt.mapFoesOnSelf.erase(it++);
			else
				++it;
		}
		if (rt.mapFoesOnSelf.size() < PLAYERBOT_SIDEKICK_FOE_MEMORY_MAX * 2)
			NotePlayerBotSidekickAttackers(ch, rt, dwNow);
	}

	// The companion leaves its fight - it fell, was sent away or off on an
	// errand, or let go to play on its own - and what was fighting it turns on
	// its owner, the way a monster that is hit turns on who hit it: a place in
	// its aggro and its victim (CHARACTER::BeginFight). The aggro is a single
	// point, so the fight after it is the engine's to decide - the companion,
	// back on its feet and hitting it again, has the larger share and takes it
	// back (ChangeVictimByAggro). A monster fighting somebody else still
	// standing is left to that fight, and one the owner cannot be hit by (a
	// safe zone, a monster too far) stays where it is.
	int HandPlayerBotSidekickFoesToOwner(LPCHARACTER ch, LPCHARACTER owner, TPlayerBotSidekickRuntime& rt,
			DWORD dwNow, const char* why)
	{
		if (!owner || owner->IsDead() || !owner->GetSectree() || owner->GetMapIndex() != ch->GetMapIndex())
		{
			rt.mapFoesOnSelf.clear();
			return 0;
		}
		NotePlayerBotSidekickAttackers(ch, rt, dwNow);
		int handed = 0;
		int tooFar = 0;
		int refused = 0;
		for (std::map<DWORD, DWORD>::const_iterator it = rt.mapFoesOnSelf.begin(); it != rt.mapFoesOnSelf.end(); ++it)
		{
			LPCHARACTER monster = CHARACTER_MANAGER::instance().Find(it->first);
			if (!monster || monster->IsDead() || !monster->IsMonster() ||
					monster->GetMapIndex() != owner->GetMapIndex())
				continue;
			LPCHARACTER victim = monster->GetVictim();
			if (victim == owner || (victim && victim != ch && !victim->IsDead()))
				continue;
			if (DISTANCE_APPROX(monster->GetX() - owner->GetX(), monster->GetY() - owner->GetY()) >
					PLAYERBOT_SIDEKICK_HANDOFF_RANGE)
			{
				++tooFar;
				continue;
			}
			if (!battle_is_attackable(monster, owner))
			{
				++refused;
				continue;
			}
			monster->UpdateAggrPoint(owner, DAMAGE_TYPE_SPECIAL, 1);
			monster->BeginFight(owner);
			++handed;
		}
		rt.mapFoesOnSelf.clear();
		if (handed > 0 || tooFar > 0 || refused > 0)
			sys_log(0, "PLAYERBOT_SIDEKICK: foes turned on the owner pid=%u name=%s owner=%u why=%s handed=%d "
					"too_far=%d refused=%d", ch->GetPlayerID(), ch->GetName(), owner->GetPlayerID(), why, handed, tooFar,
					refused);
		return handed;
	}

	// The companion is seen dead (HandleDeath, playerbot_survival.h).
	void NotePlayerBotSidekickDown(LPCHARACTER ch, DWORD dwNow)
	{
		if (!ch || s_mapPlayerBotSidekickOwner.empty())
			return;
		const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		if (!rec || rec->bMode != PLAYERBOT_SIDEKICK_FOLLOW)
			return;
		std::map<DWORD, TPlayerBotSidekickRuntime>::iterator rt = s_mapPlayerBotSidekickRuntime.find(ch->GetPlayerID());
		if (rt == s_mapPlayerBotSidekickRuntime.end())
			return;
		// A course cut short by the fall: what it woke is in the memory.
		rt->second.bLureStage = 0;
		rt->second.dwLureVID = 0;
		HandPlayerBotSidekickFoesToOwner(ch, GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID), rt->second, dwNow, "down");
	}

	// Before an order takes the companion out of the owner's fight.
	void HandPlayerBotSidekickFoesBeforeLeaving(DWORD sidekickPid, LPCHARACTER owner, const char* why)
	{
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(sidekickPid);
		std::map<DWORD, TPlayerBotSidekickRuntime>::iterator rt = s_mapPlayerBotSidekickRuntime.find(sidekickPid);
		if (!sk || sk->IsDead() || rt == s_mapPlayerBotSidekickRuntime.end())
			return;
		rt->second.bLureStage = 0;
		rt->second.dwLureVID = 0;
		HandPlayerBotSidekickFoesToOwner(sk, owner, rt->second, get_dword_time(), why);
	}

	// --------------------------------------------------------- being born

	const char* GetPlayerBotSidekickClassName(BYTE race)
	{
		switch (race % 4)
		{
			case 0: return "Wojownik";
			case 1: return "Ninja";
			case 2: return "Sura";
			default: return "Szaman";
		}
	}

	// The owner's level on the companion, the way pc.set_level makes a level
	// (questlua_pc.cpp): the points the levels bring, the random health and
	// mana, full bars.
	void RaisePlayerBotSidekickLevel(LPCHARACTER ch, int target)
	{
		target = MINMAX(1, target, gPlayerMaxLevel);
		const int oldLevel = ch->GetLevel();
		if (target <= oldLevel)
			return;
		ch->PointChange(POINT_SKILL, target - oldLevel);
		ch->PointChange(POINT_SUB_SKILL, target < 10 ? 0 : target - MAX(oldLevel, 9));
		ch->PointChange(POINT_STAT, (target - oldLevel) * 3 + ch->GetPoint(POINT_LEVEL_STEP));
		ch->PointChange(POINT_LEVEL, target - oldLevel);
		ch->SetRandomHP((target - 1) * number(JobInitialPoints[ch->GetJob()].hp_per_lv_begin,
				JobInitialPoints[ch->GetJob()].hp_per_lv_end));
		ch->SetRandomSP((target - 1) * number(JobInitialPoints[ch->GetJob()].sp_per_lv_begin,
				JobInitialPoints[ch->GetJob()].sp_per_lv_end));
		ch->ComputePoints();
		ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
		ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());
		ch->PointsPacket();
		ch->SkillLevelPacket();
	}

	// A free identity of the race asked for in the owner's kingdom: every
	// guard of the registry (LoadRegisteredBots), nobody's companion yet, out
	// of every world and out of the db core's cache, not one of the operator's
	// medal droppers, with no offline shop of its own. The level nearest the
	// owner's from below first, so the companion is raised rather than stands
	// over the owner.
	DWORD PickPlayerBotSidekickIdentity(BYTE empire, BYTE race, int ownerLevel)
	{
		char query[1600];
		snprintf(query, sizeof(query),
				"SELECT l.pid FROM common.playerbot_seed_state AS l "
				"JOIN player.player AS p ON p.id=l.pid "
				"JOIN account.account AS a ON a.id=p.account_id "
				"JOIN player.player_index AS pi ON pi.id=a.id "
				"WHERE l.seed_version=1 AND l.state IN ('complete','adopted') "
				"AND BINARY a.login=BINARY CONCAT('playerbot_',LPAD(l.pid-3,GREATEST(3,LENGTH(l.pid-3)),'0')) "
				"AND BINARY a.social_id=BINARY CONCAT('9',LPAD(l.pid-3,12,'0')) "
				"AND pi.pid1=l.pid AND pi.pid2=0 AND pi.pid3=0 AND pi.pid4=0 "
				"AND pi.empire=%u AND p.job=%u "
				// Never played (playtime 0: never in any world, so never in the
				// cache) or saved long enough ago. last_play alone refused every
				// identity of a world seeded minutes ago, which is exactly when a
				// new player meets the letter: the column defaults to the moment
				// the seed wrote the row.
				"AND (p.playtime = 0 OR p.last_play < NOW() - INTERVAL %d MINUTE) "
				"AND l.pid NOT IN (SELECT sidekick_pid FROM player.playerbot_sidekick) "
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
				"AND l.pid NOT IN (SELECT owner FROM player.ikashop_offlineshop) "
#endif
				// A never-played identity first: a played one is one of the
				// population's bots, taken out with everything it earned.
				"ORDER BY (p.playtime > 0), (p.level > %d), ABS(CAST(p.level AS SIGNED) - %d), l.pid DESC LIMIT 120",
				(unsigned int)empire, (unsigned int)race, PLAYERBOT_SIDEKICK_IDENTITY_IDLE_MINUTES,
				ownerLevel, ownerLevel);
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
		{
			sys_err("PLAYERBOT_SIDEKICK: identity query failed errno=%u", msg.get() ? msg->uiSQLErrno : 0U);
			return 0;
		}
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			DWORD pid = 0;
			if (row[0])
				str_to_number(pid, row[0]);
			if (pid == 0 || !CPlayerBotManager::instance().IsRegisteredBotPID(pid) ||
					CPlayerBotManager::instance().IsManaged(pid) ||
					CPlayerBotManager::instance().IsMedalDropperCohortPID(pid) ||
					IsPlayerBotShouterPID(pid) || // MT2009_PLUS_SHOUTERS_V1
					CHARACTER_MANAGER::instance().FindByPID(pid) || P2P_MANAGER::instance().FindByPID(pid))
				continue;
			return pid;
		}
		return 0;
	}

	// MT2009_PLUS_SIDEKICK_POOL_V1: the pool kept at PLAYERBOT_SIDEKICK_POOL_PER_RACE
	// identities of each of the eight races, any kingdom - SpawnSidekick makes
	// a companion its owner's kingdom's at the load. Only an identity that has
	// never played: never in a world, never in the db core's cache, so the name
	// it is given is the name it comes in under. The registry's every guard,
	// as PickPlayerBotSidekickIdentity asks them. A world whose identities have
	// all played keeps an empty pool, and a companion is picked as before.
	const int PLAYERBOT_SIDEKICK_POOL_PER_RACE = 2;
	const DWORD PLAYERBOT_SIDEKICK_POOL_FILL_MS = 5 * 60 * 1000;

	void FillPlayerBotSidekickPool(DWORD dwNow)
	{
		if (!s_bPlayerBotSidekickPoolTable || IsPlayerBotSidekickSwitchedOff() ||
				(s_dwPlayerBotSidekickNextPoolFill != 0 && dwNow < s_dwPlayerBotSidekickNextPoolFill))
			return;
		s_dwPlayerBotSidekickNextPoolFill = dwNow + PLAYERBOT_SIDEKICK_POOL_FILL_MS;
		int have[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		{
			std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(
					"SELECT race, COUNT(*) FROM player.playerbot_sidekick_pool GROUP BY race"));
			if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
				return;
			MYSQL_ROW row;
			while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
			{
				unsigned int race = 0, count = 0;
				if (row[0]) str_to_number(race, row[0]);
				if (row[1]) str_to_number(count, row[1]);
				if (race < 8)
					have[race] = (int)count;
			}
		}
		int added = 0;
		for (int race = 0; race < 8; ++race)
		{
			if (have[race] >= PLAYERBOT_SIDEKICK_POOL_PER_RACE)
				continue;
			char query[1400];
			snprintf(query, sizeof(query),
					"SELECT l.pid FROM common.playerbot_seed_state AS l "
					"JOIN player.player AS p ON p.id=l.pid "
					"JOIN account.account AS a ON a.id=p.account_id "
					"JOIN player.player_index AS pi ON pi.id=a.id "
					"WHERE l.seed_version=1 AND l.state IN ('complete','adopted') "
					"AND BINARY a.login=BINARY CONCAT('playerbot_',LPAD(l.pid-3,GREATEST(3,LENGTH(l.pid-3)),'0')) "
					"AND BINARY a.social_id=BINARY CONCAT('9',LPAD(l.pid-3,12,'0')) "
					"AND pi.pid1=l.pid AND pi.pid2=0 AND pi.pid3=0 AND pi.pid4=0 AND pi.empire IN (1,2,3) "
					"AND p.job=%d AND p.playtime=0 "
					"AND l.pid NOT IN (SELECT sidekick_pid FROM player.playerbot_sidekick) "
					"AND l.pid NOT IN (SELECT pid FROM player.playerbot_sidekick_pool) "
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
					"AND l.pid NOT IN (SELECT owner FROM player.ikashop_offlineshop) "
#endif
					"ORDER BY l.pid DESC LIMIT 40", race);
			std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
			if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			{
				sys_err("PLAYERBOT_SIDEKICK: companion pool query failed race=%d errno=%u", race,
						msg.get() ? msg->uiSQLErrno : 0U);
				return;
			}
			MYSQL_ROW row;
			while (have[race] < PLAYERBOT_SIDEKICK_POOL_PER_RACE &&
					NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
			{
				DWORD pid = 0;
				if (row[0])
					str_to_number(pid, row[0]);
				if (pid == 0 || !CPlayerBotManager::instance().IsRegisteredBotPID(pid) ||
						CPlayerBotManager::instance().IsManaged(pid) ||
						CPlayerBotManager::instance().IsMedalDropperCohortPID(pid) ||
						IsPlayerBotShouterPID(pid) ||
						// The test cohorts, the takeovers and the retirements keep
						// their identities.
						IsPlayerBotArezzoCohortPID(pid) || IsPlayerBotArezzoDungeonCohortPID(pid) ||
						IsPlayerBotArezzoDungeonReservedPID(pid) ||
						IsPlayerBotTakeoverHold(pid) || IsPlayerBotRetirementHold(pid) ||
						CHARACTER_MANAGER::instance().FindByPID(pid) || P2P_MANAGER::instance().FindByPID(pid))
					continue;
				char insert[192];
				snprintf(insert, sizeof(insert),
						"INSERT IGNORE INTO player.playerbot_sidekick_pool (pid, race, reserved_at) VALUES (%u, %d, NOW())",
						pid, race);
				std::unique_ptr<SQLMsg> put(AccountDB::instance().DirectQuery(insert));
				if (!put.get() || put->uiSQLErrno != 0 || !put->Get() || put->Get()->uiAffectedRows == 0)
					continue;
				s_setPlayerBotSidekickPool.insert(pid);
				++have[race];
				++added;
			}
		}
		if (added > 0)
			sys_log(0, "PLAYERBOT_SIDEKICK: companion pool reserved %d identities, %u in the pool",
					added, (unsigned int)s_setPlayerBotSidekickPool.size());
	}

	// An identity of the race out of the pool, taken for good: the DELETE
	// decides between two cores claiming one at once. 0 when the pool has none
	// of the race.
	DWORD ClaimPlayerBotSidekickPoolIdentity(BYTE race, DWORD ownerPid)
	{
		if (!s_bPlayerBotSidekickPoolTable)
			return 0;
		char query[192];
		snprintf(query, sizeof(query),
				"SELECT pid FROM player.playerbot_sidekick_pool WHERE race=%u ORDER BY reserved_at, pid LIMIT 8",
				(unsigned int)race);
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
		{
			sys_err("PLAYERBOT_SIDEKICK: companion pool query failed owner=%u errno=%u", ownerPid,
					msg.get() ? msg->uiSQLErrno : 0U);
			return 0;
		}
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			DWORD pid = 0;
			if (row[0])
				str_to_number(pid, row[0]);
			if (pid == 0 || CHARACTER_MANAGER::instance().FindByPID(pid) || P2P_MANAGER::instance().FindByPID(pid))
				continue;
			snprintf(query, sizeof(query), "DELETE FROM player.playerbot_sidekick_pool WHERE pid=%u", pid);
			std::unique_ptr<SQLMsg> take(AccountDB::instance().DirectQuery(query));
			if (!take.get() || take->uiSQLErrno != 0 || !take->Get() || take->Get()->uiAffectedRows == 0)
			{
				sys_log(0, "PLAYERBOT_SIDEKICK: companion pool claim failed owner=%u pid=%u errno=%u", ownerPid, pid,
						take.get() ? take->uiSQLErrno : 0U);
				continue;
			}
			s_setPlayerBotSidekickPool.erase(pid);
			return pid;
		}
		return 0;
	}

	// A claim whose companion was never written (the name refused, the record
	// not written) goes back to the pool.
	void GiveBackPlayerBotSidekickPoolIdentity(DWORD pid, BYTE race, DWORD ownerPid)
	{
		if (!pid || !s_bPlayerBotSidekickPoolTable)
			return;
		char query[192];
		snprintf(query, sizeof(query),
				"INSERT IGNORE INTO player.playerbot_sidekick_pool (pid, race, reserved_at) VALUES (%u, %u, NOW())",
				pid, (unsigned int)race);
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		const bool done = msg.get() && msg->uiSQLErrno == 0;
		if (done)
			s_setPlayerBotSidekickPool.insert(pid);
		sys_log(0, "PLAYERBOT_SIDEKICK: companion pool claim given back pid=%u owner=%u done=%d", pid, ownerPid,
				done ? 1 : 0);
	}

	bool IsPlayerBotSidekickNameAllowed(const char* name)
	{
		const size_t length = name ? strlen(name) : 0;
		if (length < PLAYERBOT_SIDEKICK_NAME_MIN || length > PLAYERBOT_SIDEKICK_NAME_MAX)
			return false;
		for (size_t i = 0; i < length; ++i)
		{
			const char c = name[i];
			if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
				return false;
		}
		// The engine's own rules for a character's name, and the ban list.
		return check_name && check_name(name) && !CBanwordManager::instance().CheckString(name, length);
	}

	bool CreatePlayerBotSidekick(LPCHARACTER owner, int race, int group, const char* name)
	{
		if (!owner || !EnsurePlayerBotSidekickTable())
			return false;
		const DWORD ownerPid = owner->GetPlayerID();
		// Another core may have made one a moment ago; the table decides.
		LoadPlayerBotSidekicks();
		if (s_mapPlayerBotSidekicks.find(ownerPid) != s_mapPlayerBotSidekicks.end())
		{
			SetPlayerBotSidekickFlag(ownerPid, "towarzysz.created", 1);
			SayPlayerBotSidekick(owner, "Masz juz towarzysza.");
			return false;
		}
		if (race < 0 || race > 7 || group < 1 || group > 2)
		{
			SayPlayerBotSidekick(owner, "Nie ma takiej klasy albo sciezki.");
			return false;
		}
		if (!IsPlayerBotSidekickNameAllowed(name))
		{
			SayPlayerBotSidekick(owner, "Ten nick nie pasuje: od 3 do 16 liter i cyfr, bez polskich znakow i spacji.");
			return false;
		}
		{
			char query[256];
			snprintf(query, sizeof(query), "SELECT COUNT(*) FROM player.player WHERE name='%s'", name);
			std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
			MYSQL_ROW row = NULL;
			if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult ||
					!(row = mysql_fetch_row(msg->Get()->pSQLResult)))
			{
				SayPlayerBotSidekick(owner, "Nie udalo sie sprawdzic nicku, sprobuj za chwile.");
				return false;
			}
			unsigned int taken = 0;
			if (row[0])
				str_to_number(taken, row[0]);
			if (taken != 0)
			{
				SayPlayerBotSidekick(owner, "Ten nick jest juz zajety - wybierz inny.");
				return false;
			}
		}
		// MT2009_PLUS_SIDEKICK_POOL_V1: the companions' own pool first, and the
		// population only when the pool has none of the race - a world whose
		// identities have all played, or a pool drawn empty in the last five
		// minutes.
		DWORD pid = ClaimPlayerBotSidekickPoolIdentity((BYTE)race, ownerPid);
		const bool fromPool = pid != 0;
		if (pid == 0)
			pid = PickPlayerBotSidekickIdentity(owner->GetEmpire(), (BYTE)race, owner->GetLevel());
		if (pid == 0)
		{
			SayPlayerBotSidekick(owner, "Nie ma teraz wolnej postaci tej klasy w twoim krolestwie - wybierz inna albo sprobuj pozniej.");
			sys_log(0, "PLAYERBOT_SIDEKICK: no identity for owner=%u race=%d empire=%u (pool empty)",
					ownerPid, race, (unsigned int)owner->GetEmpire());
			s_dwPlayerBotSidekickNextPoolFill = 0;
			return false;
		}
		char query[512];
		// The seed's name stays in the name history as this identity's own, so
		// the name pool reads the new one as a person's choice and keeps it at
		// every later start, 'restore' included (generate_seed.py).
		snprintf(query, sizeof(query),
				"INSERT IGNORE INTO common.playerbot_name_history (pid, seed_name, human_name, pool_version, renamed_at) "
				"SELECT id, name, name, 'sidekick', NOW() FROM player.player WHERE id=%u", pid);
		std::unique_ptr<SQLMsg> history(AccountDB::instance().DirectQuery(query));
		snprintf(query, sizeof(query), "UPDATE player.player SET name='%s' WHERE id=%u", name, pid);
		std::unique_ptr<SQLMsg> rename(AccountDB::instance().DirectQuery(query));
		if (!rename.get() || rename->uiSQLErrno != 0)
		{
			SayPlayerBotSidekick(owner, "Nie udalo sie nadac nicku - sprobuj innego.");
			sys_log(0, "PLAYERBOT_SIDEKICK: name not given owner=%u owner_name=%s pid=%u name=%s errno=%u", ownerPid,
					owner->GetName(), pid, name, rename.get() ? rename->uiSQLErrno : 0U);
			if (fromPool)
				GiveBackPlayerBotSidekickPoolIdentity(pid, (BYTE)race, ownerPid);
			return false;
		}
		// And the storekeeper's box of the account it was: a companion on a town
		// errand takes books and materials back out of it (WithdrawPlayerBotSafebox),
		// and the window then hands them to the owner. The db core writes the
		// SAFEBOX window straight to the table and reads it from there, so the
		// rows are the box; the bag is emptied on the first load
		// (FinishPlayerBotSidekickSetup).
		snprintf(query, sizeof(query),
				"DELETE i FROM player.item AS i JOIN player.player AS p ON p.id=%u "
				"WHERE i.window='SAFEBOX' AND i.owner_id=p.account_id", pid);
		std::unique_ptr<SQLMsg> safebox(AccountDB::instance().DirectQuery(query));
		const int level = MINMAX(1, owner->GetLevel(), 255);
		// A new companion's skill points are its owner's from the start: the
		// AI spent them all at the first summon, before anybody had opened the
		// window ("Lepiej byloby, gdyby domyslnie wlaczona byla opcja
		// samodzielnego rozdawania skilli", blasty, 28 September). The window's
		// switch hands them to the AI. The stat points stay the AI's.
		if (s_bPlayerBotSidekickSettingsColumns)
			snprintf(query, sizeof(query),
					"INSERT INTO player.playerbot_sidekick (owner_pid, sidekick_pid, mode, skill_group, start_level, setup_done, "
					"created_at, manual_skills) VALUES (%u, %u, 0, %d, %d, 0, NOW(), 1)", ownerPid, pid, group, level);
		else
			snprintf(query, sizeof(query),
					"INSERT INTO player.playerbot_sidekick (owner_pid, sidekick_pid, mode, skill_group, start_level, setup_done, created_at) "
					"VALUES (%u, %u, 0, %d, %d, 0, NOW())", ownerPid, pid, group, level);
		std::unique_ptr<SQLMsg> insert(AccountDB::instance().DirectQuery(query));
		if (!insert.get() || insert->uiSQLErrno != 0)
		{
			SayPlayerBotSidekick(owner, "Nie udalo sie zapisac towarzysza - sprobuj za chwile.");
			sys_log(0, "PLAYERBOT_SIDEKICK: record not written owner=%u owner_name=%s pid=%u name=%s errno=%u", ownerPid,
					owner->GetName(), pid, name, insert.get() ? insert->uiSQLErrno : 0U);
			if (fromPool)
				GiveBackPlayerBotSidekickPoolIdentity(pid, (BYTE)race, ownerPid);
			return false;
		}
		TPlayerBotSidekick rec;
		rec.dwOwnerPID = ownerPid;
		rec.dwSidekickPID = pid;
		rec.bMode = PLAYERBOT_SIDEKICK_FOLLOW;
		rec.bGroup = (BYTE)group;
		rec.bLevel = (BYTE)level;
		rec.bSetupDone = false;
		rec.bManualSkills = true;
		rec.dwOwnerSeenAt = get_dword_time();
		s_mapPlayerBotSidekicks[ownerPid] = rec;
		s_mapPlayerBotSidekickOwner[pid] = ownerPid;
		SetPlayerBotSidekickFlag(ownerPid, "towarzysz.created", 1);
		char text[192];
		snprintf(text, sizeof(text), "%s (%s) dolacza do ciebie - chwila i bedzie przy tobie.",
				name, GetPlayerBotSidekickClassName((BYTE)race));
		SayPlayerBotSidekick(owner, text);
		SayPlayerBotSidekick(owner, "Punkty umiejetnosci rozdajesz ty: okno towarzysza (P), Umiejetnosci. "
				"Wolisz, zeby robil to sam? Ustaw tam \"Punkty rozdaje sam: nie\".");
		sys_log(0, "PLAYERBOT_SIDEKICK: created owner=%u owner_name=%s pid=%u name=%s race=%d group=%d level=%d pool=%d",
				ownerPid, owner->GetName(), pid, name, race, group, level, fromPool ? 1 : 0);
		CPlayerBotManager::instance().SpawnSidekick(pid);
		return true;
	}

	// ---------------------------------------------------------- the party

	// "Grupa" of this companion (TPlayerBotSidekick::bParty), by its pid.
	bool IsPlayerBotSidekickJoiningAnyParty(DWORD sidekickPid)
	{
		std::map<DWORD, DWORD>::const_iterator owner = s_mapPlayerBotSidekickOwner.find(sidekickPid);
		if (owner == s_mapPlayerBotSidekickOwner.end())
			return false;
		TPlayerBotSidekickMap::const_iterator rec = s_mapPlayerBotSidekicks.find(owner->second);
		return rec != s_mapPlayerBotSidekicks.end() && rec->second.bParty;
	}

	// In the owner's party, "exp leci nam po rowno". A party the owner leads,
	// or none, which is made for the owner the way the engine makes one when
	// its first invitation is accepted (CHARACTER::PartyInviteAccept); the
	// split is set once, when this makes the party - after that it is the
	// leader's. Another person's party the companion joins with "Grupa" on
	// (the default) while a place stays free after it for one more person:
	// three friends in one party, and only the leader's companion with them,
	// was "tylko jeden towarzysz" (xXxDaronxXx, 28 September). With "Grupa"
	// off it takes an invitation from that leader (AcceptPlayerBotPartyInvite)
	// and otherwise follows its owner outside the party. And not while the
	// owner is in a dungeon, where the engine refuses a new member too
	// (PERR_DUNGEON).
	void ApplyPlayerBotSidekickRole(LPCHARACTER ch, LPCHARACTER owner, const TPlayerBotSidekick& rec, LPPARTY party);

	void KeepPlayerBotSidekickInParty(LPCHARACTER ch, LPCHARACTER owner, DWORD dwNow)
	{
		LPPARTY party = owner->GetParty();
		const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		// "Lider grupy": the companion's own party with its owner in it.
		if (rec && rec->bLead)
		{
			if (party && ch->GetParty() == party && party->GetLeaderPID() == ch->GetPlayerID())
			{
				ApplyPlayerBotSidekickRole(ch, owner, *rec, party);
				return;
			}
			if (owner->GetDungeon())
				return;
			// The two alone in the owner's party: it is handed over. With
			// anybody else in it the owner's party stays as it is.
			if (party)
			{
				if (party->GetLeaderPID() == owner->GetPlayerID() && ch->GetParty() == party &&
						party->GetMemberCount() <= 2)
					LeavePlayerBotParty(ch);
				else
				{
					PlayerBotLogThrottled("sidekick_lead_busy", dwNow,
							"PLAYERBOT_SIDEKICK: owner is in a party of others, cannot lead pid=%u name=%s owner=%u",
							ch->GetPlayerID(), ch->GetName(), owner->GetPlayerID());
					return;
				}
			}
			if (owner->GetParty())
				return;
			if (ch->GetParty())
				LeavePlayerBotParty(ch);
			// The invitation's own conditions (IsPartyJoinableMutableCondition,
			// protected in CHARACTER): thirty levels either way, one kingdom.
			if (std::abs((int)ch->GetLevel() - (int)owner->GetLevel()) > 30 || owner->IsObserverMode() ||
					ch->GetEmpire() != owner->GetEmpire())
			{
				PlayerBotLogThrottled("sidekick_lead_refused", dwNow,
						"PLAYERBOT_SIDEKICK: cannot invite its owner pid=%u name=%s owner=%u",
						ch->GetPlayerID(), ch->GetName(), owner->GetPlayerID());
				return;
			}
			LPPARTY own = CPartyManager::instance().CreateParty(ch);
			if (!own)
				return;
			own->SetParameter(PARTY_EXP_DISTRIBUTION_PARITY);
			// What CHARACTER::PartyJoin does (protected as well).
			own->Join(owner->GetPlayerID());
			own->Link(owner);
			// MT2009_PLUS_SIDEKICK_WHISPER_V1: in its whisper, as the rest of its words.
			SayPlayerBotSidekick(owner, "[Grupa] Zapraszam cie do mojej grupy - jestem jej liderem.");
			sys_log(0, "PLAYERBOT_SIDEKICK: leads its owner's party pid=%u name=%s owner=%u leadership=%d",
					ch->GetPlayerID(), ch->GetName(), owner->GetPlayerID(), ch->GetLeadershipSkillLevel());
			return;
		}
		// Back from leading: its party goes, and the owner's is made below.
		if (party && ch->GetParty() == party && party->GetLeaderPID() == ch->GetPlayerID())
		{
			LeavePlayerBotParty(ch);
			party = owner->GetParty();
		}
		if (party && ch->GetParty() == party)
			return;
		if (owner->GetDungeon())
			return;
		const bool joinsAny = IsPlayerBotSidekickJoiningAnyParty(ch->GetPlayerID());
		if (party && party->GetLeaderPID() != owner->GetPlayerID() && joinsAny &&
				party->GetMemberCount() + 2 > PARTY_MAX_MEMBER)
		{
			if (ch->GetParty())
				LeavePlayerBotParty(ch);
			PlayerBotLogThrottled("sidekick_party_room", dwNow,
					"PLAYERBOT_SIDEKICK: no place left for a person after it pid=%u name=%s owner=%u members=%d",
					ch->GetPlayerID(), ch->GetName(), owner->GetPlayerID(), (int)party->GetMemberCount());
			return;
		}
		if (party && party->GetLeaderPID() != owner->GetPlayerID() && !joinsAny)
		{
			if (ch->GetParty())
				LeavePlayerBotParty(ch);
			PlayerBotLogThrottled("sidekick_party_other", dwNow,
					"PLAYERBOT_SIDEKICK: owner is in another leader's party pid=%u name=%s owner=%u leader=%u",
					ch->GetPlayerID(), ch->GetName(), owner->GetPlayerID(), party->GetLeaderPID());
			return;
		}
		if (party && party->GetMemberCount() >= PARTY_MAX_MEMBER)
		{
			PlayerBotLogThrottled("sidekick_party_full", dwNow,
					"PLAYERBOT_SIDEKICK: owner's party is full pid=%u name=%s owner=%u",
					ch->GetPlayerID(), ch->GetName(), owner->GetPlayerID());
			return;
		}
		if (ch->GetParty())
			LeavePlayerBotParty(ch);
		if (!party)
		{
			party = CPartyManager::instance().CreateParty(owner);
			if (!party)
				return;
			party->SetParameter(PARTY_EXP_DISTRIBUTION_PARITY);
			sys_log(0, "PLAYERBOT_SIDEKICK: made a party for owner=%u name=%s",
					owner->GetPlayerID(), owner->GetName());
		}
		party->Join(ch->GetPlayerID());
		party->Link(ch);
		sys_log(0, "PLAYERBOT_SIDEKICK: joined the owner's party pid=%u name=%s owner=%u members=%d",
				ch->GetPlayerID(), ch->GetName(), owner->GetPlayerID(), (int)party->GetMemberCount());
	}

	// ------------------------------------------------------ coming and going

	// A point beside the owner, a little to one side by pid, on ground a bot
	// may stand on.
	void GetPlayerBotSidekickSpot(LPCHARACTER ch, LPCHARACTER owner, long& x, long& y)
	{
		const DWORD h = PlayerBotNavHash(ch->GetPlayerID() ^ 0x534b5053U);
		x = owner->GetX() + (long)(h % 301) - 150;
		y = owner->GetY() + (long)((h >> 9) % 301) - 150;
		LPSECTREE tree = SECTREE_MANAGER::instance().Get(owner->GetMapIndex(), x, y);
		if (!tree || tree->IsAttr(x, y, ATTR_BLOCK | ATTR_OBJECT))
		{
			x = owner->GetX();
			y = owner->GetY();
		}
	}

	// Put the companion beside its owner, on the owner's map. Not
	// TransitionPlayerBotMap: that one refuses a map with no navigation grid
	// and turns a warp into the Spider Dungeon into a desert crossing, and a
	// companion goes wherever its owner went - a dungeon instance included,
	// with the membership Entergame would give a reconnecting player (as
	// CPlayerBotManager::WarpBot does).
	bool PlacePlayerBotSidekickAt(LPCHARACTER ch, TPlayerBotAIState& state, long targetMap, long x, long y,
			DWORD dwNow, const char* reason);

	bool PlacePlayerBotSidekick(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER owner, DWORD dwNow,
			const char* reason)
	{
		long x = 0, y = 0;
		GetPlayerBotSidekickSpot(ch, owner, x, y);
		return PlacePlayerBotSidekickAt(ch, state, owner->GetMapIndex(), x, y, dwNow, reason);
	}

	// The same move to a point of this core's maps: beside the owner, or the
	// fishing bank of its kingdom's first village (SendPlayerBotSidekickFishing).
	bool PlacePlayerBotSidekickAt(LPCHARACTER ch, TPlayerBotAIState& state, long targetMap, long x, long y,
			DWORD dwNow, const char* reason)
	{
		// MT2009_PLUS_AREZZO_BOTS_V1 (off limits): not into the new dungeons, the
		// Blue Dragon's lair or the Arezzo maps - no bot goes there for now (the
		// owner, 30 September). It waits where it is for its owner.
		// MT2009_PLUS_BOT_DUNGEONS_ALL_V1: except beside its owner, who stands
		// there - the companion goes into every dungeon with its owner (the
		// owner, 4 October), and through the Las to the Jungle's guard. On its
		// own errands it keeps out as ever.
		// MT2009_PLUS_SIDEKICK_AREZZO_V1: the Arezzo maps (360-362) included -
		// "towarzysz nie wchodzi na doline cyklopow / pustkowie faraona" (the
		// owner, 5 October) was the rule above, in force to 2.21: this check
		// refused them and ManagePlayerBotSidekick held the companion where it
		// stood ("czeka na Ciebie tutaj - do tego miejsca towarzysz nie
		// wchodzi"). With the module off (M2_AREZZO=0) nobody but a GM stands
		// there, and the module's send-off takes the owner out.
		LPCHARACTER placeOwner = IsPlayerBotOffLimitsMap(targetMap) ? GetPlayerBotSidekickOwnerHere(ch->GetPlayerID()) : NULL;
		if (IsPlayerBotOffLimitsMap(targetMap) && !(placeOwner && placeOwner->GetMapIndex() == targetMap))
		{
			PlayerBotLogThrottled("sidekick_off_limits", dwNow,
					"PLAYERBOT_SIDEKICK: does not follow onto map=%ld pid=%u name=%s reason=%s",
					targetMap, ch->GetPlayerID(), ch->GetName(), reason);
			return false;
		}
		const long oldMap = ch->GetMapIndex();
		const bool wasRiding = ch->IsRiding();
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		ch->Stop();
		if (wasRiding)
			ch->StopRiding();
		ch->HorseSummon(false);
		ClearPlayerBotRoute(state, true);
		state.bVisitingShop = false;
		state.bVisitingBiologist = false;
		state.bVisitingStable = false;
		state.bTacticalRetreat = false;
		state.lDesertCrossingTo = 0;
		LPDUNGEON before = ch->GetDungeon();
		if (!ch->Show(targetMap, x, y, 0))
		{
			PlayerBotLogThrottled("sidekick_place_failed", dwNow,
					"PLAYERBOT_SIDEKICK: cannot stand beside the owner pid=%u name=%s from=%ld to=%ld reason=%s",
					ch->GetPlayerID(), ch->GetName(), oldMap, targetMap, reason);
			return false;
		}
		ch->Stop();
		ch->SendMovePacket(FUNC_MOVE, 0, x, y, 0, dwNow);
		LPDUNGEON after = targetMap >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN
				? CDungeonManager::instance().FindByMapIndex(targetMap) : NULL;
		if (before != after)
		{
			if (before)
				ch->SetDungeon(NULL);
			if (after)
				ch->SetDungeon(after);
		}
		CPlayerBotNavigation::instance(targetMap).Init(targetMap);
		state.lLastX = x;
		state.lLastY = y;
		state.dwLastMeaningfulActivityTime = dwNow;
		if (oldMap != targetMap)
		{
			ch->Save();
			sys_log(0, "PLAYERBOT_SIDEKICK: beside the owner pid=%u name=%s from=%ld to=%ld reason=%s",
					ch->GetPlayerID(), ch->GetName(), oldMap, targetMap, reason);
		}
		return true;
	}

	// A walk on a map with no navigation grid (a dungeon the bots never go to):
	// the server moves a bot in a straight line, with nothing in the way.
	void WalkPlayerBotSidekickDirect(LPCHARACTER ch, long x, long y, DWORD dwNow)
	{
		if (ch->Goto(x, y))
			ch->SendMovePacket(FUNC_MOVE, 0, x, y, 0, dwNow);
	}

	void WalkPlayerBotSidekick(LPCHARACTER ch, long x, long y, DWORD dwNow, bool allowHorse)
	{
		if (CPlayerBotNavigation::instance(ch->GetMapIndex()).Init(ch->GetMapIndex()) &&
				MovePlayerBot(ch, x, y, dwNow, 4, true, allowHorse, false, allowHorse))
			return;
		WalkPlayerBotSidekickDirect(ch, x, y, dwNow);
	}

	void LoadPlayerBotSidekickGifts(DWORD sidekickPid, TPlayerBotSidekickRuntime& rt)
	{
		char query[512];
		// Forget what is gone for a day: sold by the owner's hand, burnt, or
		// taken back. A day, because the db core writes an item's new owner
		// minutes after the trade.
		snprintf(query, sizeof(query),
				"DELETE g FROM player.playerbot_sidekick_gift AS g LEFT JOIN player.item AS i ON i.id=g.item_id "
				"WHERE g.sidekick_pid=%u AND i.id IS NULL AND g.given_at < NOW() - INTERVAL 1 DAY", sidekickPid);
		std::unique_ptr<SQLMsg> prune(AccountDB::instance().DirectQuery(query));
		// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: and which of them it holds for
		// its owner.
		snprintf(query, sizeof(query), "SELECT item_id, %s FROM player.playerbot_sidekick_gift WHERE sidekick_pid=%u",
				s_bPlayerBotSidekickHeldColumn ? "held" : "0", sidekickPid);
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			DWORD id = 0;
			unsigned int held = 0;
			if (row[0])
				str_to_number(id, row[0]);
			if (row[1])
				str_to_number(held, row[1]);
			if (id != 0)
			{
				rt.setGifts.insert(id);
				if (held != 0)
					rt.setHeld.insert(id);
			}
		}
		if (!s_bPlayerBotSidekickPinTable)
			return;
		// The window's marks, forgotten the same way: a day after the piece is
		// nobody's or somebody else's (the owner takes a piece back through the
		// window, which clears its mark at once).
		snprintf(query, sizeof(query),
				"DELETE p FROM player.playerbot_sidekick_pin AS p LEFT JOIN player.item AS i "
				"ON i.id=p.item_id AND i.owner_id=p.sidekick_pid "
				"WHERE p.sidekick_pid=%u AND i.id IS NULL AND p.pinned_at < NOW() - INTERVAL 1 DAY", sidekickPid);
		std::unique_ptr<SQLMsg> prunePins(AccountDB::instance().DirectQuery(query));
		snprintf(query, sizeof(query), "SELECT item_id, wear FROM player.playerbot_sidekick_pin WHERE sidekick_pid=%u",
				sidekickPid);
		std::unique_ptr<SQLMsg> pins(AccountDB::instance().DirectQuery(query));
		if (!pins.get() || pins->uiSQLErrno != 0 || !pins->Get() || !pins->Get()->pSQLResult)
			return;
		while (NULL != (row = mysql_fetch_row(pins->Get()->pSQLResult)))
		{
			DWORD id = 0;
			unsigned int wear = PLAYERBOT_SIDEKICK_PIN_UNWANTED;
			if (row[0])
				str_to_number(id, row[0]);
			if (row[1])
				str_to_number(wear, row[1]);
			if (id != 0 && (wear < WEAR_MAX_NUM || wear == PLAYERBOT_SIDEKICK_PIN_UNWANTED))
				rt.mapPins[id] = (BYTE)wear;
		}
	}

	// A companion starts with nothing of the bot it was (Vipper, 26 September:
	// "dostaje caly jego ekwipunek oraz yang"). Its identity is one of the
	// population's, often played, and the window hands the owner anything it
	// holds ("wez") - a bot's bag, its worn gear and its yang, and after a
	// dismissal the next bot's; a seeded identity raised to the owner's level
	// would open its starter chest and hand over the whole chain the same way.
	// So the first load empties it and gives the class's starter weapon and
	// armour and the two potions, as the seed gives a new bot. Only at
	// creation: what the pair gathers afterwards is the pair's.
	void ClearPlayerBotSidekickBelongings(LPCHARACTER ch)
	{
		int removed = 0;
		for (int wear = 0; wear < WEAR_MAX_NUM; ++wear)
		{
			LPITEM worn = ch->GetWear(wear);
			if (!worn)
				continue;
			ITEM_MANAGER::instance().RemoveItem(worn, "PLAYERBOT_SIDEKICK_CLEAR");
			++removed;
		}
		for (int cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell)
				continue;
			ITEM_MANAGER::instance().RemoveItem(item, "PLAYERBOT_SIDEKICK_CLEAR");
			++removed;
		}
		const long long gold = (long long)ch->GetGold();
		if (gold > 0)
			PlayerBotChangeGold(ch, -gold);
		// The seed's own starter set (playerbots_seed.sql): the class's first
		// weapon and armour, red and blue potions.
		static const DWORD weapons[4] = { 10, 1000, 10, 7000 };
		static const DWORD armours[4] = { 11200, 11400, 11600, 11800 };
		const int job = MINMAX(0, (int)ch->GetJob(), 3);
		const DWORD starter[2] = { weapons[job], armours[job] };
		for (int i = 0; i < 2; ++i)
		{
			LPITEM piece = ch->AutoGiveItem(starter[i], 1, -1, false);
			if (piece && piece->GetOwner() == ch && piece->GetWindow() == INVENTORY)
				PlayerBotEquipItem(ch, piece);
		}
		ch->AutoGiveItem(27001, 200, -1, false);
		ch->AutoGiveItem(27004, 200, -1, false);
		sys_log(0, "PLAYERBOT_SIDEKICK: belongings cleared pid=%u name=%s items=%d gold=%lld",
				ch->GetPlayerID(), ch->GetName(), removed, gold);
	}

	// The setup's second half, on the companion's first tick with its bag
	// loaded: what the bot it was carried goes, and the setup is written down.
	void FinishPlayerBotSidekickSetup(LPCHARACTER ch, TPlayerBotSidekick& rec)
	{
		ClearPlayerBotSidekickBelongings(ch);
		ch->Save();
		rec.bSetupDone = true;
		DBManager::instance().Query("UPDATE player.playerbot_sidekick SET setup_done=1 WHERE sidekick_pid=%u",
				ch->GetPlayerID());
		sys_log(0, "PLAYERBOT_SIDEKICK: set up pid=%u name=%s level=%d group=%u",
				ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), (unsigned int)ch->GetSkillGroup());
	}

	void OnPlayerBotSidekickLoaded(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		if (!rec)
			return;
		// The player's, not the population's: no stone role, no party role, and
		// the character of a steady adventurer when it is let off the leash.
		state.bBotRole = BOT_ROLE_MOB_GRINDER;
		state.bPersonality = BOT_PERSONALITY_STEADY_ADVENTURER;
		state.persona.bDrawnPersonality = BOT_PERSONALITY_STEADY_ADVENTURER;
		TPlayerBotSidekickRuntime& rt = s_mapPlayerBotSidekickRuntime[ch->GetPlayerID()];
		rt = TPlayerBotSidekickRuntime();
		LoadPlayerBotSidekickGifts(ch->GetPlayerID(), rt);
		if (!rec->bSetupDone)
		{
			// The owner's level at its creation, then the path chosen - which a
			// companion under level five takes at five
			// (KeepPlayerBotSidekickPath). Nothing of the bot it was goes once
			// its bag has come (FinishPlayerBotSidekickSetup): this runs as the
			// character loads, before the db core sends a single item, and the
			// first cut emptied a bag that was not there yet - the bot's items
			// arrived a moment later and stayed (the self-test, 26 September:
			// "belongings cleared items=0", then its seed chest opened). Both
			// steps are safe to repeat until the setup is written down.
			RaisePlayerBotSidekickLevel(ch, rec->bLevel);
			if (rec->bGroup != 0 && ch->GetLevel() >= 5 && ch->GetSkillGroup() != rec->bGroup)
			{
				ch->ClearSkill();
				ch->SetSkillGroup(rec->bGroup);
			}
		}
		// MT2009_PLUS_SIDEKICK_POOL_V1 (rename): the name its row holds. The db
		// core answers a load out of its cache when it still holds the
		// character - a population's bot taken while its cache stood, the
		// companions made before the pool - and the cache carries the name it
		// had; the save leaves the name column alone, so the row keeps the
		// chosen one, and a load after the cache has gone brings it.
		{
			char query[128];
			snprintf(query, sizeof(query), "SELECT name FROM player.player WHERE id=%u", ch->GetPlayerID());
			std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
			MYSQL_ROW row = NULL;
			if (msg.get() && msg->uiSQLErrno == 0 && msg->Get() && msg->Get()->pSQLResult &&
					(row = mysql_fetch_row(msg->Get()->pSQLResult)) && row[0] && *row[0] &&
					strcmp(row[0], ch->GetName()) != 0)
			{
				if (s_setPlayerBotSidekickRenameTried.insert(ch->GetPlayerID()).second)
				{
					s_setPlayerBotSidekickRenameRelog.insert(ch->GetPlayerID());
					sys_log(0, "PLAYERBOT_SIDEKICK: loaded under an old name pid=%u name=%s row_name=%s owner=%u, "
							"logging out once to come back under it", ch->GetPlayerID(), ch->GetName(), row[0],
							rec->dwOwnerPID);
				}
				else
					sys_log(0, "PLAYERBOT_SIDEKICK: still under its old name after a relog pid=%u name=%s row_name=%s owner=%u",
							ch->GetPlayerID(), ch->GetName(), row[0], rec->dwOwnerPID);
			}
		}
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID);
		if (owner && owner->GetSectree() && rec->bMode == PLAYERBOT_SIDEKICK_FOLLOW)
		{
			PlacePlayerBotSidekick(ch, state, owner, dwNow, "sidekick_arrives");
			KeepPlayerBotSidekickInParty(ch, owner, dwNow);
			char text[128];
			snprintf(text, sizeof(text), "%s jest przy tobie.", ch->GetName());
			SayPlayerBotSidekick(owner, text);
		}
		sys_log(0, "PLAYERBOT_SIDEKICK: entered pid=%u name=%s owner=%u mode=%u level=%d gifts=%u",
				ch->GetPlayerID(), ch->GetName(), rec->dwOwnerPID, (unsigned int)rec->bMode, ch->GetLevel(),
				(unsigned int)rt.setGifts.size());
	}

	// The orders, below.
	void ReportPlayerBotSidekick(LPCHARACTER owner, const TPlayerBotSidekick& rec);
	void SummonPlayerBotSidekick(LPCHARACTER owner, TPlayerBotSidekick& rec, DWORD dwNow);
	void FreePlayerBotSidekick(LPCHARACTER owner, TPlayerBotSidekick& rec);
	void DismissPlayerBotSidekick(LPCHARACTER owner, TPlayerBotSidekick rec);
	bool HandlePlayerBotSidekickWhisper(LPCHARACTER from, LPCHARACTER bot, const char* text);
	void HandlePlayerBotSidekickCommand(LPCHARACTER ch, const char* argument);
	void SendPlayerBotSidekickWindow(LPCHARACTER owner, bool fullGear);
	DWORD GetPlayerBotSidekickSkillBase(LPCHARACTER sk);
	std::string DescribePlayerBotSidekickGrandMaster(LPCHARACTER sk);
	std::string GetPlayerBotSidekickWearRefusal(LPCHARACTER sk, LPITEM item, LPITEM replacing);
	std::string GetPlayerBotSidekickHandOverRefusal(LPITEM item);
	int CountPlayerBotSidekickChasers(LPCHARACTER ch);
	int GetPlayerBotSidekickHealthPercent(LPCHARACTER ch);

	// Where a piece of the companion's stands now: a bag cell, 1000 + the
	// wear slot, or -1 when it has left both.
	int FindPlayerBotSidekickSelfTestPiece(LPCHARACTER ch, DWORD id)
	{
		if (!id)
			return -1;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetCell() == cell && item->GetID() == id)
				return cell;
		}
		for (int wear = 0; wear < WEAR_MAX_NUM; ++wear)
		{
			LPITEM item = ch->GetWear(wear);
			if (item && item->GetID() == id)
				return PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + wear;
		}
		return -1;
	}

	// The bag window's self-test (the self-test file's first line "eq"): one
	// of the window's orders every pass of the self-test (half a minute), on
	// the pair it makes or takes up, each answered in the log - where
	// SendPlayerBotSidekickCommand writes every command while a self-test
	// runs. A move in the bag, a piece handed over and taken back, the body
	// armour taken off (and left off by the AI) and put on again (pinned), the
	// skills, the pin undone; then the paths that move items the other way
	// round - two pieces trading places, a stack poured on another, a piece
	// taken off onto a chosen cell, a piece the companion can wear taken to
	// the owner, handed back straight onto its slot, taken back off the slot
	// and given back to the bag - and a skill point spent, each measured
	// before and after (the units of a kind, where a piece ended up). The
	// owner in a self-test is a bot, so the piece that goes round is the
	// companion's own, and each round trip is made in one pass: given half a
	// minute, the owner's own equipment pass put a necklace on.
	void StepPlayerBotSidekickEqSelfTest(LPCHARACTER owner, LPCHARACTER sk, int step)
	{
		char order[96];
		int from = -1;
		int to = -1;
		switch (step)
		{
			case 1:
				HandlePlayerBotSidekickCommand(owner, "eq 1");
				break;
			case 2:
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && from < 0; ++cell)
				{
					LPITEM item = sk->GetInventoryItem(cell);
					if (item && item->GetCell() == cell && item->GetSize() == 1 && !item->isLocked())
						from = cell;
				}
				for (int cell = PLAYERBOT_BAG_CELLS - 1; cell >= 0 && to < 0; --cell)
					if (!sk->GetInventoryItem(cell) && sk->IsEmptyItemGrid(TItemPos(INVENTORY, cell), 1))
						to = cell;
				snprintf(order, sizeof(order), "eq ruch %d %d", from, to);
				HandlePlayerBotSidekickCommand(owner, order);
				break;
			case 3:
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && from < 0; ++cell)
				{
					LPITEM item = owner->GetInventoryItem(cell);
					if (item && item->GetCell() == cell && item->GetSize() == 1 && !item->isLocked() &&
							!item->IsExchanging() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE))
					{
						from = cell;
						s_dwPlayerBotSidekickSelfTestGiftID = item->GetID();
					}
				}
				snprintf(order, sizeof(order), "eq daj %d -1", from);
				HandlePlayerBotSidekickCommand(owner, order);
				break;
			case 4:
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && from < 0; ++cell)
				{
					LPITEM item = sk->GetInventoryItem(cell);
					if (item && item->GetCell() == cell && item->GetID() == s_dwPlayerBotSidekickSelfTestGiftID)
						from = cell;
				}
				snprintf(order, sizeof(order), "eq wez %d -1", from);
				HandlePlayerBotSidekickCommand(owner, order);
				break;
			case 5:
				snprintf(order, sizeof(order), "eq ruch %d -1", PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + WEAR_BODY);
				HandlePlayerBotSidekickCommand(owner, order);
				break;
			case 6:
			{
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && from < 0; ++cell)
				{
					LPITEM item = sk->GetInventoryItem(cell);
					if (item && item->GetCell() == cell && IsPlayerBotSidekickUnwanted(sk, item))
						from = cell;
				}
				LPITEM body = sk->GetWear(WEAR_BODY);
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test body_now=%u unwanted_cell=%d", body ? body->GetVnum() : 0U,
						from);
				snprintf(order, sizeof(order), "eq ruch %d %d", from, PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + WEAR_BODY);
				HandlePlayerBotSidekickCommand(owner, order);
				break;
			}
			case 7:
			{
				LPITEM body = sk->GetWear(WEAR_BODY);
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test body=%u pinned=%d points=%d", body ? body->GetVnum() : 0U,
						body && IsPlayerBotSidekickPinned(sk, body) ? 1 : 0, (int)sk->GetPoint(POINT_SKILL));
				HandlePlayerBotSidekickCommand(owner, "umiejetnosci");
				const DWORD base = GetPlayerBotSidekickSkillBase(sk);
				if (base != 0 && sk->GetPoint(POINT_SKILL) > 0)
				{
					snprintf(order, sizeof(order), "umiejetnosci dodaj %u", base);
					HandlePlayerBotSidekickCommand(owner, order);
				}
				HandlePlayerBotSidekickCommand(owner, "umiejetnosci reczne 0");
				break;
			}
			case 8:
				snprintf(order, sizeof(order), "eq odepnij %d", PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + WEAR_BODY);
				HandlePlayerBotSidekickCommand(owner, order);
				HandlePlayerBotSidekickCommand(owner, "eq");
				break;
			case 9:
			{
				// Two pieces of one size and two kinds trade places.
				int a = -1;
				int b = -1;
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && b < 0; ++cell)
				{
					LPITEM item = sk->GetInventoryItem(cell);
					if (!item || item->GetCell() != cell || item->isLocked())
						continue;
					LPITEM first = a >= 0 ? sk->GetInventoryItem(a) : NULL;
					if (!first)
						a = cell;
					else if (first->GetSize() == item->GetSize() && first->GetVnum() != item->GetVnum())
						b = cell;
				}
				if (a < 0 || b < 0)
				{
					sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test swap: no pair");
					break;
				}
				const DWORD idA = sk->GetInventoryItem(a)->GetID();
				const DWORD idB = sk->GetInventoryItem(b)->GetID();
				snprintf(order, sizeof(order), "eq ruch %d %d", a, b);
				HandlePlayerBotSidekickCommand(owner, order);
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test swap a=%d b=%d a_now=%d b_now=%d", a, b,
						FindPlayerBotSidekickSelfTestPiece(sk, idA), FindPlayerBotSidekickSelfTestPiece(sk, idB));
				break;
			}
			case 10:
			{
				// A stack poured on another of its kind with room to spare; the
				// units of the kind are the same afterwards.
				int into = -1;
				for (WORD i = 0; i < PLAYERBOT_BAG_CELLS && into < 0; ++i)
				{
					LPITEM x = sk->GetInventoryItem(i);
					if (!x || x->GetCell() != i || !x->IsStackable() || x->isLocked() ||
							(int)x->GetCount() >= PlayerBotMaxStack(x))
						continue;
					for (WORD j = 0; j < PLAYERBOT_BAG_CELLS; ++j)
					{
						LPITEM y = sk->GetInventoryItem(j);
						if (j != i && y && y->GetCell() == j && y->GetVnum() == x->GetVnum() && !y->isLocked())
						{
							from = j;
							into = i;
							break;
						}
					}
				}
				if (into < 0)
				{
					sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test pour: no two stacks of a kind with room");
					break;
				}
				const DWORD vnum = sk->GetInventoryItem(into)->GetVnum();
				const int unitsBefore = sk->CountSpecifyItem(vnum);
				const int intoBefore = (int)sk->GetInventoryItem(into)->GetCount();
				const int fromBefore = (int)sk->GetInventoryItem(from)->GetCount();
				snprintf(order, sizeof(order), "eq ruch %d %d", from, into);
				HandlePlayerBotSidekickCommand(owner, order);
				LPITEM intoNow = sk->GetInventoryItem(into);
				LPITEM fromNow = sk->GetInventoryItem(from);
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test pour vnum=%u from=%d(%d->%d) into=%d(%d->%d) units=%d->%d",
						vnum, from, fromBefore, fromNow && fromNow->GetVnum() == vnum ? (int)fromNow->GetCount() : 0,
						into, intoBefore, intoNow ? (int)intoNow->GetCount() : -1, unitsBefore,
						sk->CountSpecifyItem(vnum));
				break;
			}
			case 11:
			{
				// A worn piece taken off onto a cell chosen for it.
				static const int kinds[] = { WEAR_HEAD, WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR, WEAR_SHIELD };
				LPITEM piece = NULL;
				int wear = -1;
				for (size_t k = 0; k < sizeof(kinds) / sizeof(kinds[0]) && !piece; ++k)
					if ((piece = sk->GetWear(kinds[k])) != NULL)
						wear = kinds[k];
				if (!piece)
				{
					sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test take-off: nothing worn to take off");
					break;
				}
				for (int cell = PLAYERBOT_BAG_CELLS - 1; cell >= 0 && to < 0; --cell)
					if (!sk->GetInventoryItem(cell) && sk->IsEmptyItemGrid(TItemPos(INVENTORY, cell), piece->GetSize()))
						to = cell;
				s_dwPlayerBotSidekickSelfTestPieceID = piece->GetID();
				snprintf(order, sizeof(order), "eq ruch %d %d", PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + wear, to);
				HandlePlayerBotSidekickCommand(owner, order);
				LPITEM landed = to >= 0 ? sk->GetInventoryItem(to) : NULL;
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test take-off wear=%d vnum=%u to=%d now=%d unwanted=%d", wear,
						piece->GetVnum(), to, FindPlayerBotSidekickSelfTestPiece(sk, s_dwPlayerBotSidekickSelfTestPieceID),
						landed && landed->GetID() == s_dwPlayerBotSidekickSelfTestPieceID &&
						IsPlayerBotSidekickUnwanted(sk, landed) ? 1 : 0);
				break;
			}
			case 12:
			{
				// A piece the companion can wear, from its bag to the owner's and
				// straight back onto its slot in the same pass - a bot owner's own
				// equipment pass would put it on otherwise: given, worn, pinned.
				int wear = -1;
				for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && from < 0; ++cell)
				{
					LPITEM item = sk->GetInventoryItem(cell);
					if (!item || item->GetCell() != cell || !item->IsEquipable() || item->isLocked() ||
							item->IsExchanging() || !GetPlayerBotSidekickHandOverRefusal(item).empty())
						continue;
					const int cellWear = item->FindEquipCell(sk);
					if (cellWear < 0 || cellWear >= WEAR_MAX_NUM ||
							!GetPlayerBotSidekickWearRefusal(sk, item, sk->GetWear(cellWear)).empty())
						continue;
					from = cell;
					wear = cellWear;
					s_dwPlayerBotSidekickSelfTestGiftID = item->GetID();
				}
				if (from < 0)
				{
					sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test gift to a slot: nothing in the bag it can wear");
					break;
				}
				s_iPlayerBotSidekickSelfTestGiftWear = wear;
				const DWORD id = s_dwPlayerBotSidekickSelfTestGiftID;
				snprintf(order, sizeof(order), "eq wez %d -1", from);
				HandlePlayerBotSidekickCommand(owner, order);
				const int ownerAt = FindPlayerBotSidekickSelfTestPiece(owner, id);
				LPITEM before = sk->GetWear(wear);
				const DWORD beforeID = before ? before->GetID() : 0;
				if (ownerAt >= 0 && ownerAt < PLAYERBOT_SIDEKICK_EQ_WEAR_BASE)
				{
					snprintf(order, sizeof(order), "eq daj %d %d", ownerAt, PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + wear);
					HandlePlayerBotSidekickCommand(owner, order);
				}
				LPITEM worn = sk->GetWear(wear);
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test gift to a slot from=%d owner_had=%d wear=%d gift_now=%d worn_is_gift=%d pinned=%d replaced_now=%d owner_now=%d",
						from, ownerAt, wear, FindPlayerBotSidekickSelfTestPiece(sk, id),
						worn && worn->GetID() == id ? 1 : 0, worn && IsPlayerBotSidekickPinned(sk, worn) ? 1 : 0,
						FindPlayerBotSidekickSelfTestPiece(sk, beforeID), FindPlayerBotSidekickSelfTestPiece(owner, id));
				break;
			}
			case 13:
			{
				// Taken back off the slot and given back to the bag, in one pass.
				const DWORD id = s_dwPlayerBotSidekickSelfTestGiftID;
				const int at = FindPlayerBotSidekickSelfTestPiece(sk, id);
				if (at < 0)
				{
					sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test take off the slot: the companion has not got the piece");
					break;
				}
				snprintf(order, sizeof(order), "eq wez %d -1", at);
				HandlePlayerBotSidekickCommand(owner, order);
				const int ownerAt = FindPlayerBotSidekickSelfTestPiece(owner, id);
				const int companionAfterTake = FindPlayerBotSidekickSelfTestPiece(sk, id);
				if (ownerAt >= 0 && ownerAt < PLAYERBOT_SIDEKICK_EQ_WEAR_BASE)
				{
					snprintf(order, sizeof(order), "eq daj %d -1", ownerAt);
					HandlePlayerBotSidekickCommand(owner, order);
				}
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test took the piece off the slot from=%d companion_after_take=%d owner_had=%d companion_now=%d owner_now=%d",
						at, companionAfterTake, ownerAt, FindPlayerBotSidekickSelfTestPiece(sk, id),
						FindPlayerBotSidekickSelfTestPiece(owner, id));
				break;
			}
			case 14:
			{
				// A point spent by the owner - the first makes the points the
				// owner's. A companion with none is given one for the test.
				const DWORD base = GetPlayerBotSidekickSkillBase(sk);
				DWORD pick = 0;
				int low = 99;
				for (DWORD vnum = base; base != 0 && vnum < base + 6; ++vnum)
					if (CSkillManager::instance().Get(vnum) && sk->GetSkillMasterType(vnum) == SKILL_NORMAL &&
							sk->GetSkillLevel(vnum) < 17 && sk->GetSkillLevel(vnum) < low)
					{
						pick = vnum;
						low = sk->GetSkillLevel(vnum);
					}
				if (!pick)
				{
					sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test skill point: no skill of the path takes a point");
					break;
				}
				if (sk->GetPoint(POINT_SKILL) <= 0)
				{
					sk->PointChange(POINT_SKILL, 1);
					sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test skill point: one point given for the test");
				}
				const int pointsBefore = sk->GetPoint(POINT_SKILL);
				snprintf(order, sizeof(order), "umiejetnosci dodaj %u", pick);
				HandlePlayerBotSidekickCommand(owner, order);
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test skill point vnum=%u level=%d->%d points=%d->%d manual=%d", pick,
						low, (int)sk->GetSkillLevel(pick), pointsBefore, (int)sk->GetPoint(POINT_SKILL),
						IsPlayerBotSidekickManualSkills(sk) ? 1 : 0);
				HandlePlayerBotSidekickCommand(owner, "umiejetnosci reczne 0");
				break;
			}
			case 15:
			{
				// The piece left in the bag at step eleven is the AI's again.
				const int at = FindPlayerBotSidekickSelfTestPiece(sk, s_dwPlayerBotSidekickSelfTestPieceID);
				if (at >= 0)
				{
					snprintf(order, sizeof(order), "eq odepnij %d", at);
					HandlePlayerBotSidekickCommand(owner, order);
				}
				HandlePlayerBotSidekickCommand(owner, "eq");
				sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test done");
				break;
			}
		}
	}

	// The self-test's orders, one every ninety seconds after the companion is
	// made: a trade from the owner with one item in it, the leash let go, the
	// call back, a report, the whispers, the three stances, then the window's
	// orders - the spot kept, the snapshot, the switches, a town visit - and,
	// after an hour, the dismissal: every path a person's letter and window
	// take, on a world with no person in it.
	void LogPlayerBotSidekickSelfTestStats(LPCHARACTER sk, const TPlayerBotSidekick& rec, const char* when)
	{
		sys_log(0, "PLAYERBOT_SIDEKICK: lure self-test stats %s points=%d ht=%d iq=%d st=%d dx=%d manual=%d reset_used=%d",
				when, (int)sk->GetPoint(POINT_STAT), (int)sk->GetRealPoint(POINT_HT), (int)sk->GetRealPoint(POINT_IQ),
				(int)sk->GetRealPoint(POINT_ST), (int)sk->GetRealPoint(POINT_DX), rec.bManualStats ? 1 : 0,
				rec.bStatResetUsed ? 1 : 0);
	}

	// The owner of the lure self-test stands on its spot (the tick asks it,
	// playerbot_manager.cpp).
	bool IsPlayerBotSidekickSelfTestFrozenOwner(LPCHARACTER ch)
	{
		return s_bPlayerBotSidekickSelfTest && s_bPlayerBotSidekickSelfTestLure && ch &&
				ch->GetPlayerID() == s_dwPlayerBotSidekickSelfTestOwner && s_iPlayerBotSidekickSelfTestStep >= 1 &&
				s_iPlayerBotSidekickSelfTestStep < PLAYERBOT_SIDEKICK_LURE_SELFTEST_FREE_STEP;
	}

	void StepPlayerBotSidekickLureSelfTest(LPCHARACTER owner, LPCHARACTER sk, TPlayerBotSidekick& rec, int step)
	{
		const DWORD dwNow = get_dword_time();
		const TPlayerBotSidekickRuntime* rt = FindPlayerBotSidekickRuntime(sk->GetPlayerID());
		sys_log(0, "PLAYERBOT_SIDEKICK: lure self-test step=%d owner=%u sidekick=%u lure=%d stage=%d packs=%d courses=%u "
				"chasers=%d dist=%d owner_hp=%d sk_hp=%d", step, owner->GetPlayerID(), sk->GetPlayerID(),
				rec.bLure ? 1 : 0, rt ? (int)rt->bLureStage : -1, rt ? rt->iLurePacks : -1, rt ? rt->uLureCourses : 0U,
				CountPlayerBotSidekickChasers(sk), DISTANCE_APPROX(sk->GetX() - owner->GetX(), sk->GetY() - owner->GetY()),
				GetPlayerBotSidekickHealthPercent(owner), GetPlayerBotSidekickHealthPercent(sk));
		switch (step)
		{
			case 1:
			{
				// A hunting spot of the owner's village, the one whose monsters are
				// nearest the owner's level from below.
				const TPlayerBotVillageGround* ground = GetPlayerBotVillageGround(owner->GetMapIndex());
				const TPlayerBotVillageHub* best = NULL;
				for (size_t i = 0; ground && i < ground->hubCount; ++i)
				{
					const TPlayerBotVillageHub& hub = ground->hubs[i];
					if (hub.mobLevel <= owner->GetLevel() && (!best || hub.mobLevel > best->mobLevel))
						best = &hub;
				}
				if (best && owner->Show(owner->GetMapIndex(), best->x, best->y, 0))
				{
					owner->Stop();
					sys_log(0, "PLAYERBOT_SIDEKICK: lure self-test owner on a spot map=%ld x=%ld y=%ld mob_level=%d",
							owner->GetMapIndex(), best->x, best->y, (int)best->mobLevel);
				}
				LogPlayerBotSidekickSelfTestStats(sk, rec, "before");
				HandlePlayerBotSidekickCommand(owner, "statystyki odnow");
				LogPlayerBotSidekickSelfTestStats(sk, rec, "after_reset");
				// The second reset is refused.
				HandlePlayerBotSidekickCommand(owner, "statystyki odnow");
				break;
			}
			case 2:
				HandlePlayerBotSidekickCommand(owner, "statystyki dodaj st 5");
				HandlePlayerBotSidekickCommand(owner, "statystyki dodaj ht 3");
				LogPlayerBotSidekickSelfTestStats(sk, rec, "after_adds");
				break;
			case 3:
				// The AI has spent none of what is left in the thirty seconds since.
				LogPlayerBotSidekickSelfTestStats(sk, rec, "manual_holds");
				HandlePlayerBotSidekickCommand(owner, "luruj 1");
				SendPlayerBotSidekickWindow(owner, false);
				break;
			case 10:
				sys_log(0, "PLAYERBOT_SIDEKICK: lure self-test whisper \"przestan lurowac\" handled=%d lure=%d",
						HandlePlayerBotSidekickWhisper(owner, sk, "przestan lurowac") ? 1 : 0, rec.bLure ? 1 : 0);
				break;
			case 11:
				sys_log(0, "PLAYERBOT_SIDEKICK: lure self-test whisper \"luruj\" handled=%d lure=%d",
						HandlePlayerBotSidekickWhisper(owner, sk, "luruj") ? 1 : 0, rec.bLure ? 1 : 0);
				break;
			case PLAYERBOT_SIDEKICK_LURE_SELFTEST_FREE_STEP:
				HandlePlayerBotSidekickCommand(owner, "wolny");
				break;
			case 15:
				HandlePlayerBotSidekickCommand(owner, "przywolaj");
				HandlePlayerBotSidekickCommand(owner, "luruj 0");
				HandlePlayerBotSidekickCommand(owner, "statystyki reczne 0");
				break;
			case 16:
				LogPlayerBotSidekickSelfTestStats(sk, rec, "ai_again");
				break;
			case PLAYERBOT_SIDEKICK_LURE_SELFTEST_STEPS:
				sys_log(0, "PLAYERBOT_SIDEKICK: lure self-test done owner=%u sidekick=%u courses=%u",
						owner->GetPlayerID(), sk->GetPlayerID(), rt ? rt->uLureCourses : 0U);
				break;
			default:
				break;
		}
		(void)dwNow;
	}

	void StepPlayerBotSidekickSelfTest(DWORD dwNow)
	{
		TPlayerBotSidekickMap::iterator rec = s_mapPlayerBotSidekicks.find(s_dwPlayerBotSidekickSelfTestOwner);
		LPCHARACTER owner = CHARACTER_MANAGER::instance().FindByPID(s_dwPlayerBotSidekickSelfTestOwner);
		if (rec == s_mapPlayerBotSidekicks.end() || !owner)
			return;
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec->second.dwSidekickPID);
		const DWORD age = dwNow - s_dwPlayerBotSidekickSelfTestAt;
		// The window's test goes one order a pass, once the companion is in
		// the world with its bag loaded.
		if (s_bPlayerBotSidekickSelfTestEq)
		{
			if (!sk || !sk->IsItemLoaded() || !CPlayerBotManager::instance().IsManaged(sk->GetPlayerID()))
				return;
			if (s_iPlayerBotSidekickSelfTestStep >= PLAYERBOT_SIDEKICK_EQ_SELFTEST_STEPS)
				return;
			const int eqStep = ++s_iPlayerBotSidekickSelfTestStep;
			sys_log(0, "PLAYERBOT_SIDEKICK: eq self-test step=%d owner=%u sidekick=%u", eqStep, owner->GetPlayerID(),
					sk->GetPlayerID());
			StepPlayerBotSidekickEqSelfTest(owner, sk, eqStep);
			return;
		}
		// The stats', the lure's and the handover's, one step a pass too.
		if (s_bPlayerBotSidekickSelfTestLure)
		{
			if (!sk || sk->IsDead() || !CPlayerBotManager::instance().IsManaged(sk->GetPlayerID()) ||
					s_iPlayerBotSidekickSelfTestStep >= PLAYERBOT_SIDEKICK_LURE_SELFTEST_STEPS)
				return;
			StepPlayerBotSidekickLureSelfTest(owner, sk, rec->second, ++s_iPlayerBotSidekickSelfTestStep);
			return;
		}
		const int step = (int)(age / 90000U);
		if (step <= s_iPlayerBotSidekickSelfTestStep)
			return;
		s_iPlayerBotSidekickSelfTestStep = step;
		const TPlayerBotSidekickRuntime* rtNow = FindPlayerBotSidekickRuntime(rec->second.dwSidekickPID);
		sys_log(0, "PLAYERBOT_SIDEKICK: self-test step=%d owner=%u sidekick=%u here=%d map=%ld/%ld dist=%d party=%d hold=%d spot=%d errand=%d visit=%d",
				step, owner->GetPlayerID(), rec->second.dwSidekickPID, sk ? 1 : 0, owner->GetMapIndex(),
				sk ? sk->GetMapIndex() : 0L,
				sk ? DISTANCE_APPROX(sk->GetX() - owner->GetX(), sk->GetY() - owner->GetY()) : -1,
				sk && sk->GetParty() && sk->GetParty() == owner->GetParty() ? 1 : 0,
				rtNow && rtNow->bHold ? 1 : 0,
				sk && rtNow && rtNow->bHold ? DISTANCE_APPROX(sk->GetX() - rtNow->lHoldX, sk->GetY() - rtNow->lHoldY) : -1,
				rtNow && rtNow->bErrand ? 1 : 0, rtNow && rtNow->bErrandVisit ? 1 : 0);
		if (step == 1 && sk && !owner->GetExchange() && !sk->GetExchange() &&
				owner->GetMapIndex() == sk->GetMapIndex() && owner->ExchangeStart(sk) && owner->GetExchange())
		{
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = owner->GetInventoryItem(cell);
				if (!item || item->GetCell() != cell || item->GetSize() != 1 || item->isLocked() ||
						IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE))
					continue;
				const bool added = owner->GetExchange()->AddItem(TItemPos(INVENTORY, cell), 0);
				sys_log(0, "PLAYERBOT_SIDEKICK: self-test trade item=%u vnum=%u added=%d", item->GetID(),
						item->GetVnum(), added ? 1 : 0);
				break;
			}
			if (owner->GetExchange())
				owner->GetExchange()->Accept(true);
		}
		else if (step == 2)
			FreePlayerBotSidekick(owner, rec->second);
		else if (step == 4)
			SummonPlayerBotSidekick(owner, rec->second, dwNow);
		else if (step == 5)
			ReportPlayerBotSidekick(owner, rec->second);
		// The same three orders by whisper, as an owner would write them.
		else if (step == 6 && sk)
			sys_log(0, "PLAYERBOT_SIDEKICK: self-test whisper \"graj sam\" handled=%d",
					HandlePlayerBotSidekickWhisper(owner, sk, "graj sam") ? 1 : 0);
		else if (step == 8 && sk)
			sys_log(0, "PLAYERBOT_SIDEKICK: self-test whisper \"chodz do mnie\" handled=%d",
					HandlePlayerBotSidekickWhisper(owner, sk, "chodz do mnie") ? 1 : 0);
		else if (step == 9 && sk)
			sys_log(0, "PLAYERBOT_SIDEKICK: self-test whisper \"ile masz lvl\" handled=%d (expected 0)",
					HandlePlayerBotSidekickWhisper(owner, sk, "ile masz lvl") ? 1 : 0);
		// The three stances, each held for four and a half minutes, with what it
		// took up under the one before: nothing "nearby" under the second and
		// the third, nothing but what hit it under the third.
		else if ((step == 10 || step == 13 || step == 16 || step == 18) && sk)
		{
			TPlayerBotSidekickRuntime& rt = s_mapPlayerBotSidekickRuntime[sk->GetPlayerID()];
			sys_log(0, "PLAYERBOT_SIDEKICK: self-test foes stance=%u owner_target=%u at_owner=%u at_self=%u nearby=%u",
					(unsigned int)rec->second.bStance, rt.adwFoes[PLAYERBOT_SIDEKICK_FOE_OWNER_TARGET],
					rt.adwFoes[PLAYERBOT_SIDEKICK_FOE_AT_OWNER], rt.adwFoes[PLAYERBOT_SIDEKICK_FOE_AT_SELF],
					rt.adwFoes[PLAYERBOT_SIDEKICK_FOE_NEARBY]);
			memset(rt.adwFoes, 0, sizeof(rt.adwFoes));
			const char* order = step == 10 ? "nie atakuj pierwszy" : step == 13 ? "nie walcz" : step == 16 ? "atakuj" : NULL;
			if (order)
			{
				const bool handled = HandlePlayerBotSidekickWhisper(owner, sk, order);
				sys_log(0, "PLAYERBOT_SIDEKICK: self-test whisper \"%s\" handled=%d stance=%u", order, handled ? 1 : 0,
						(unsigned int)rec->second.bStance);
			}
		}
		// The window's orders: waiting at a spot while the owner goes on,
		// the snapshot the window is sent, its switches, the call back, and a
		// town visit there and back.
		else if (step == 19)
			HandlePlayerBotSidekickCommand(owner, "czekaj");
		else if (step == 20 || step == 22)
			SendPlayerBotSidekickWindow(owner, true);
		else if (step == 21)
		{
			HandlePlayerBotSidekickCommand(owner, "zbieraj 1");
			HandlePlayerBotSidekickCommand(owner, "ochrona 0");
			HandlePlayerBotSidekickCommand(owner, "buffy 0");
		}
		else if (step == 23)
		{
			HandlePlayerBotSidekickCommand(owner, "zbieraj 2");
			HandlePlayerBotSidekickCommand(owner, "ochrona 1");
			HandlePlayerBotSidekickCommand(owner, "buffy 1");
			if (sk)
				sys_log(0, "PLAYERBOT_SIDEKICK: self-test whisper \"chodz\" handled=%d",
						HandlePlayerBotSidekickWhisper(owner, sk, "chodz") ? 1 : 0);
		}
		// Asked again at every step until it begins: a companion lying down
		// at the moment of the order says "Najpierw musze wstac" and does
		// nothing, which on 25 September was the whole of the first try.
		else if (step >= 24 && step <= 29 && !s_bPlayerBotSidekickSelfTestErrand)
		{
			const unsigned int before = s_uPlayerBotSidekickErrands;
			HandlePlayerBotSidekickCommand(owner, "zakupy");
			s_bPlayerBotSidekickSelfTestErrand = s_uPlayerBotSidekickErrands != before;
		}
		else if (step == 40)
			DismissPlayerBotSidekick(owner, rec->second);
	}

	// The self-test: a bot the core picks owns a companion made for it.
	void RunPlayerBotSidekickSelfTest(DWORD dwNow)
	{
		if (dwNow < s_dwPlayerBotSidekickNextSelfTestCheck)
			return;
		s_dwPlayerBotSidekickNextSelfTestCheck = dwNow + 30000;
		struct stat st;
		const bool on = stat(PLAYERBOT_SIDEKICK_SELFTEST_FILE, &st) == 0;
		if (on != s_bPlayerBotSidekickSelfTest)
		{
			sys_log(0, "PLAYERBOT_SIDEKICK: self-test %s", on ? "on" : "off");
			s_dwPlayerBotSidekickSelfTestSince = dwNow;
		}
		s_bPlayerBotSidekickSelfTest = on;
		if (!on)
			return;
		// The file's first line "eq": the bag window's orders alone.
		{
			char line[16] = "";
			FILE* file = fopen(PLAYERBOT_SIDEKICK_SELFTEST_FILE, "r");
			if (file)
			{
				if (!fgets(line, sizeof(line), file))
					line[0] = 0;
				fclose(file);
			}
			const bool eq = !strncmp(line, "eq", 2);
			if (eq != s_bPlayerBotSidekickSelfTestEq)
				sys_log(0, "PLAYERBOT_SIDEKICK: self-test of the bag window %s", eq ? "on" : "off");
			s_bPlayerBotSidekickSelfTestEq = eq;
			const bool lure = !strncmp(line, "lure", 4);
			if (lure != s_bPlayerBotSidekickSelfTestLure)
				sys_log(0, "PLAYERBOT_SIDEKICK: self-test of the stats and the lure %s", lure ? "on" : "off");
			s_bPlayerBotSidekickSelfTestLure = lure;
		}
		if (s_dwPlayerBotSidekickSelfTestOwner != 0)
		{
			StepPlayerBotSidekickSelfTest(dwNow);
			return;
		}
		// A pair an earlier run made - its owner a bot - is taken up again
		// after a restart rather than a second one made, its owner waited for
		// while the spawn window may still bring it in.
		bool ownerComing = false;
		for (TPlayerBotSidekickMap::const_iterator it = s_mapPlayerBotSidekicks.begin();
				it != s_mapPlayerBotSidekicks.end(); ++it)
		{
			if (!CPlayerBotManager::instance().IsRegisteredBotPID(it->first))
				continue;
			if (!CHARACTER_MANAGER::instance().FindByPID(it->first))
			{
				ownerComing = true;
				continue;
			}
			s_dwPlayerBotSidekickSelfTestOwner = it->first;
			s_dwPlayerBotSidekickSelfTestAt = dwNow;
			s_iPlayerBotSidekickSelfTestStep = 0;
			s_bPlayerBotSidekickSelfTestErrand = false;
			sys_log(0, "PLAYERBOT_SIDEKICK: self-test takes up owner=%u sidekick=%u", it->first,
					it->second.dwSidekickPID);
			return;
		}
		if (ownerComing && dwNow - s_dwPlayerBotSidekickSelfTestSince < PLAYERBOT_SIDEKICK_SELFTEST_OWNER_WAIT_MS)
			return;
		// An owner that is a bot of ten or more with no companion and not one
		// itself, on a village map; the companion of the other class.
		const CHARACTER_MANAGER::NAME_MAP& pcs = CHARACTER_MANAGER::instance().GetPCMap();
		for (CHARACTER_MANAGER::NAME_MAP::const_iterator it = pcs.begin(); it != pcs.end(); ++it)
		{
			LPCHARACTER c = it->second;
			if (!c || !c->GetDesc() || !c->GetDesc()->IsBot() || c->GetLevel() < 10 || c->IsDead() ||
					IsPlayerBotSidekickPID(c->GetPlayerID()) ||
					s_mapPlayerBotSidekicks.find(c->GetPlayerID()) != s_mapPlayerBotSidekicks.end() ||
					!IsPlayerBotVillageMap(c->GetMapIndex()))
				continue;
			char name[32];
			snprintf(name, sizeof(name), "TestKompan%u", (unsigned int)(dwNow % 1000));
			const int race = c->GetJob() == JOB_SHAMAN ? 0 : 7;
			sys_log(0, "PLAYERBOT_SIDEKICK: self-test owner=%u name=%s level=%d", c->GetPlayerID(), c->GetName(),
					c->GetLevel());
			s_dwPlayerBotSidekickSelfTestOwner = c->GetPlayerID();
			s_dwPlayerBotSidekickSelfTestAt = dwNow;
			s_iPlayerBotSidekickSelfTestStep = 0;
			s_bPlayerBotSidekickSelfTestErrand = false;
			CreatePlayerBotSidekick(c, race, 1 + (int)(c->GetPlayerID() % 2), name);
			return;
		}
	}

#if defined(PLAYERBOT_ENGINE_MT2009)
	// Walking through one's own companion ("wylacz kolizje player-towarzysz",
	// the operator, 26 September: at its owner's side in a fight it stood in the way).
	// The collision is the client's alone - CInstanceBase::CheckAdvancing tests
	// the main instance against every other one, and
	// CActorInstance::TestActorCollision skips a victim whose actor type is NPC
	// (ENABLE_NPC_WITHOUT_COLLISIONS) - so the owner's client is told which
	// character is its companion, "SidekickVid <vid>" and 0 once it has gone,
	// and client-root/sidekickcollision.py types that instance as an NPC every
	// time the client makes it anew. At every change of either VID - a warp is
	// a new login, a new VID for the owner - and again after
	// PLAYERBOT_SIDEKICK_BODY_RESEND_MS. A root from before 2.0.41 answers the
	// command with one "Unknown Server Command" line in its syserr.txt.
	struct TPlayerBotSidekickBodySent
	{
		DWORD dwVid;
		DWORD dwOwnerVid;
		DWORD dwAt;
		TPlayerBotSidekickBodySent() : dwVid(0), dwOwnerVid(0), dwAt(0) {}
	};
	std::map<DWORD, TPlayerBotSidekickBodySent> s_mapPlayerBotSidekickBodySent;	// by owner pid

	void SendPlayerBotSidekickBody(LPCHARACTER owner, const TPlayerBotSidekick& rec, DWORD dwNow)
	{
		if (!owner || !owner->GetDesc() || owner->GetDesc()->IsBot())
			return;
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
		const DWORD vid = sk && sk->GetSectree() ? (DWORD)sk->GetVID() : 0;
		const DWORD ownerVid = (DWORD)owner->GetVID();
		TPlayerBotSidekickBodySent& sent = s_mapPlayerBotSidekickBodySent[rec.dwOwnerPID];
		// Nothing to take back from a client that was never told.
		if (vid == 0 && sent.dwVid == 0)
			return;
		if (vid == sent.dwVid && ownerVid == sent.dwOwnerVid &&
				dwNow - sent.dwAt < PLAYERBOT_SIDEKICK_BODY_RESEND_MS)
			return;
		owner->ChatPacket(CHAT_TYPE_COMMAND, "SidekickVid %u", (unsigned int)vid);
		sent.dwVid = vid;
		sent.dwOwnerVid = ownerVid;
		sent.dwAt = dwNow;
	}
#endif

	// MT2009_PLUS_SIDEKICK_FREE_CORE_V1: a companion let off the leash ("wolna
	// reka", fishing) plays in its own world, not its owner's. It used to be
	// logged out and in behind every warp of its owner's across cores, and on
	// a core that does not host its map the engine put it down wherever it
	// could: "cannot find valid location 56973 x 167174", then the owner's
	// Arezzo dungeon instance 3630000, from which no warp of a bot leads
	// (warpset refused map=64, ~240 "target navigation unavailable" lines a
	// minute, 30 September 14:09-14:19). Its saved map, read from the table
	// at most every ten seconds: the core that hosts it - on the owner's
	// channel - keeps it and spawns it, while the owner is anywhere in the game.
	long GetPlayerBotSidekickSavedMap(DWORD sidekickPid, DWORD dwNow)
	{
		static std::map<DWORD, std::pair<long, DWORD> > s_cache;
		std::pair<long, DWORD>& c = s_cache[sidekickPid];
		if (c.second != 0 && dwNow - c.second < 10000)
			return c.first;
		c.second = dwNow | 1;
		char query[128];
		snprintf(query, sizeof(query), "SELECT map_index FROM player.player WHERE id=%u", sidekickPid);
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		MYSQL_ROW row = NULL;
		if (msg.get() && msg->uiSQLErrno == 0 && msg->Get() && msg->Get()->pSQLResult &&
				(row = mysql_fetch_row(msg->Get()->pSQLResult)) && row[0])
		{
			long map = 0;
			str_to_number(map, row[0]);
			c.first = map;
		}
		return c.first;
	}

	// Let off the leash and its world is this core's: its saved map is an
	// ordinary one hosted here, and its owner plays on this channel.
	bool IsPlayerBotSidekickFreeWorldHere(const TPlayerBotSidekick& rec, LPCHARACTER ownerHere, DWORD dwNow)
	{
		if (rec.bMode != PLAYERBOT_SIDEKICK_FREE)
			return false;
		if (!ownerHere)
		{
			CCI* cci = P2P_MANAGER::instance().FindByPID(rec.dwOwnerPID);
			if (!cci || cci->bChannel != g_bChannel)
				return false;
		}
		const long saved = GetPlayerBotSidekickSavedMap(rec.dwSidekickPID, dwNow);
		return saved > 0 && saved < PLAYERBOT_INSTANCE_MAP_INDEX_MIN && !IsPlayerBotOffLimitsMap(saved) &&
				IsPlayerBotMapHostedHere(saved);
	}

	// Once a second for the whole core: the table, and every companion in or
	// out of the world by its owner's presence here.
	void ManagePlayerBotSidekicks(DWORD dwNow)
	{
		if (dwNow < s_dwPlayerBotSidekickNextManage)
			return;
		s_dwPlayerBotSidekickNextManage = dwNow + 1000;
		if (dwNow >= s_dwPlayerBotSidekickNextLoad)
		{
			s_dwPlayerBotSidekickNextLoad = dwNow + PLAYERBOT_SIDEKICK_RELOAD_MS;
			LoadPlayerBotSidekicks();
			FillPlayerBotSidekickPool(dwNow); // MT2009_PLUS_SIDEKICK_POOL_V1
		}
		RunPlayerBotSidekickSelfTest(dwNow);
		const bool off = IsPlayerBotSidekickSwitchedOff();
		for (TPlayerBotSidekickMap::iterator it = s_mapPlayerBotSidekicks.begin(); it != s_mapPlayerBotSidekicks.end(); ++it)
		{
			TPlayerBotSidekick& rec = it->second;
			LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec.dwOwnerPID);
			const bool here = CPlayerBotManager::instance().IsManaged(rec.dwSidekickPID);
			// Switched off for the world: the companion leaves as it leaves with
			// a gone owner, and nobody's is spawned. The record stays, so it
			// comes back as it was when the switch is on again.
			if (off)
			{
				if (here)
				{
					LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
					if (sk)
					{
						if (sk->GetParty())
							LeavePlayerBotParty(sk);
						sk->Save();
					}
					sys_log(0, "PLAYERBOT_SIDEKICK: companions switched off, logging out pid=%u owner=%u",
							rec.dwSidekickPID, rec.dwOwnerPID);
					CPlayerBotManager::instance().Despawn(rec.dwSidekickPID, playerbot_session_rules::OUT_SIDEKICK); // MT2009_PLUS_BOT_SESSIONS_V1
					s_mapPlayerBotSidekickRuntime.erase(rec.dwSidekickPID);
				}
#if defined(PLAYERBOT_ENGINE_MT2009)
				if (owner && owner->GetSectree())
					SendPlayerBotSidekickBody(owner, rec, dwNow);
#endif
				continue;
			}
			if (owner && owner->GetSectree())
			{
				rec.dwOwnerSeenAt = dwNow;
				rec.bOwnerLevel = (BYTE)MINMAX(1, owner->GetLevel(), 255);
				// Its owner came back in another kingdom (an Olejek Wygnania is
				// pc.change_empire, which rewrites the owner's account and takes
				// a relog) while the companion still stood in the world: it logs
				// out, and the next try brings it in with the owner's kingdom
				// (SpawnSidekick).
				// MT2009_PLUS_SIDEKICK_POOL_V1 (rename): in under an old name, it
				// logs out once and comes back under its own once the db core
				// has let the old one go (OnPlayerBotSidekickLoaded).
				if (here && s_setPlayerBotSidekickRenameRelog.count(rec.dwSidekickPID))
				{
					s_setPlayerBotSidekickRenameRelog.erase(rec.dwSidekickPID);
					LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
					if (sk)
					{
						if (sk->GetParty())
							LeavePlayerBotParty(sk);
						sk->Save();
					}
					sys_log(0, "PLAYERBOT_SIDEKICK: logging out to come back under its name pid=%u name=%s owner=%u",
							rec.dwSidekickPID, sk ? sk->GetName() : "?", rec.dwOwnerPID);
					SayPlayerBotSidekick(owner, "Gra pamieta mnie jeszcze pod starym nickiem - wyloguje sie i za okolo "
							"dziesiec minut wroce juz pod wlasciwym.");
					CPlayerBotManager::instance().Despawn(rec.dwSidekickPID, playerbot_session_rules::OUT_SIDEKICK);
					s_mapPlayerBotSidekickRuntime.erase(rec.dwSidekickPID);
					rec.dwNextSpawnTry = dwNow + PLAYERBOT_SIDEKICK_RENAME_HOLD_MS;
					continue;
				}
				if (here)
				{
					LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
					if (sk && sk->GetEmpire() != owner->GetEmpire() && owner->GetEmpire() >= 1 &&
							owner->GetEmpire() <= 3)
					{
						if (sk->GetParty())
							LeavePlayerBotParty(sk);
						sk->Save();
						sys_log(0, "PLAYERBOT_SIDEKICK: owner changed kingdom, logging out to follow pid=%u owner=%u empire=%u->%u",
								rec.dwSidekickPID, rec.dwOwnerPID, (unsigned int)sk->GetEmpire(),
								(unsigned int)owner->GetEmpire());
						SayPlayerBotSidekick(owner, "Zmieniles krolestwo - ide za toba, zaraz bede.");
						CPlayerBotManager::instance().Despawn(rec.dwSidekickPID, playerbot_session_rules::OUT_SIDEKICK); // MT2009_PLUS_BOT_SESSIONS_V1
						s_mapPlayerBotSidekickRuntime.erase(rec.dwSidekickPID);
						rec.dwNextSpawnTry = dwNow + PLAYERBOT_SIDEKICK_SPAWN_RETRY_MS;
						continue;
					}
				}
				// MT2009_PLUS_SIDEKICK_FREE_CORE_V1: off the leash, the core that
				// hosts its saved map brings it in - this one only when that is
				// here, or when nobody has for a minute.
				bool spawnHere = true;
				if (!here && rec.bMode == PLAYERBOT_SIDEKICK_FREE &&
						!IsPlayerBotSidekickFreeWorldHere(rec, owner, dwNow))
				{
					const long saved = GetPlayerBotSidekickSavedMap(rec.dwSidekickPID, dwNow);
					static std::map<DWORD, DWORD> s_mapWaitingSince;
					DWORD& since = s_mapWaitingSince[rec.dwSidekickPID];
					if (since == 0 || dwNow - since > 120000)
						since = dwNow;
					if (saved > 0 && saved < PLAYERBOT_INSTANCE_MAP_INDEX_MIN && !IsPlayerBotOffLimitsMap(saved) &&
							dwNow - since < 60000)
						spawnHere = false;
				}
				if (!here && spawnHere && dwNow >= rec.dwNextSpawnTry)
				{
					rec.dwNextSpawnTry = dwNow + PLAYERBOT_SIDEKICK_SPAWN_RETRY_MS;
					// Still in another core's world: that core lets it go once it
					// sees its owner gone, and a later try here spawns it.
					if (!P2P_MANAGER::instance().FindByPID(rec.dwSidekickPID) &&
							!CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID))
						CPlayerBotManager::instance().SpawnSidekick(rec.dwSidekickPID);
				}
#if defined(PLAYERBOT_ENGINE_MT2009)
				SendPlayerBotSidekickBody(owner, rec, dwNow);
#endif
			}
			// MT2009_PLUS_SIDEKICK_FREE_CORE_V1: off the leash with its owner on
			// another core of the game, it stays in this world - or comes into
			// it, when its saved map is hosted here.
			else if (rec.bMode == PLAYERBOT_SIDEKICK_FREE && P2P_MANAGER::instance().FindByPID(rec.dwOwnerPID) &&
					(here || IsPlayerBotSidekickFreeWorldHere(rec, NULL, dwNow)))
			{
				if (!here && dwNow >= rec.dwNextSpawnTry)
				{
					rec.dwNextSpawnTry = dwNow + PLAYERBOT_SIDEKICK_SPAWN_RETRY_MS;
					if (!P2P_MANAGER::instance().FindByPID(rec.dwSidekickPID) &&
							!CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID))
					{
						sys_log(0, "PLAYERBOT_SIDEKICK: free, spawned in its own world pid=%u owner=%u map=%ld",
								rec.dwSidekickPID, rec.dwOwnerPID, GetPlayerBotSidekickSavedMap(rec.dwSidekickPID, dwNow));
						CPlayerBotManager::instance().SpawnSidekick(rec.dwSidekickPID);
					}
				}
			}
			// A record made in this very pass carries get_dword_time(), later
			// than dwNow: seen just now, not four billion milliseconds ago.
			else if (here && (rec.dwOwnerSeenAt == 0 ||
					(dwNow >= rec.dwOwnerSeenAt && dwNow - rec.dwOwnerSeenAt >= PLAYERBOT_SIDEKICK_OWNER_GONE_MS)))
			{
				// "Gra beze mnie": an owner out of the game leaves it playing. An
				// owner on another core still takes it along - this core lets it
				// go and that one spawns it at the owner's side.
				if (IsPlayerBotSidekickPlayingAlone(rec, dwNow))
				{
#if defined(PLAYERBOT_ENGINE_MT2009)
					continue;
#else
					// r40250 has no AFFECT_EXP_BLOCK to hold it at the lead
					// (ManagePlayerBotExpLock), so there it leaves at the lead.
					LPCHARACTER alone = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
					if (!alone || alone->GetLevel() < (int)GetPlayerBotSidekickSoloLockLevel(alone, dwNow))
						continue;
					sys_log(0, "PLAYERBOT_SIDEKICK: playing alone reached its owner's level + %d pid=%u name=%s level=%d "
							"owner=%u owner_level=%u", PLAYERBOT_SIDEKICK_SOLO_LEVEL_LEAD, rec.dwSidekickPID,
							alone->GetName(), alone->GetLevel(), rec.dwOwnerPID, (unsigned int)rec.bOwnerLevel);
#endif
				}
				LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
				if (sk)
				{
					if (sk->GetParty())
						LeavePlayerBotParty(sk);
					sk->Save();
				}
				sys_log(0, "PLAYERBOT_SIDEKICK: owner gone, logging out pid=%u owner=%u", rec.dwSidekickPID,
						rec.dwOwnerPID);
				CPlayerBotManager::instance().Despawn(rec.dwSidekickPID, playerbot_session_rules::OUT_SIDEKICK); // MT2009_PLUS_BOT_SESSIONS_V1
				s_mapPlayerBotSidekickRuntime.erase(rec.dwSidekickPID);
			}
		}
	}

	// ------------------------------------------------------------ the orders

#if defined(PLAYERBOT_ENGINE_MT2009)
	std::string DescribePlayerBotSidekickCoins(LPCHARACTER sk, TPlayerBotAIState& state, const TPlayerBotSidekick& rec);
#endif
	bool IsPlayerBotSidekickCoinsOn(DWORD pid);

	void ReportPlayerBotSidekick(LPCHARACTER owner, const TPlayerBotSidekick& rec)
	{
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
		const char* mode = rec.bMode == PLAYERBOT_SIDEKICK_FREE ? "wolna reka" : "przy tobie";
		char text[320];
		if (!sk)
			snprintf(text, sizeof(text), "Twoj towarzysz za chwile bedzie w grze (tryb: %s, walka: %s).", mode,
					GetPlayerBotSidekickStanceName(rec.bStance));
		else
			snprintf(text, sizeof(text), "%s - poziom %d, zycie %d%%, %s, tryb: %s, walka: %s.",
					sk->GetName(), sk->GetLevel(),
					sk->GetMaxHP() > 0 ? (int)((long long)sk->GetHP() * 100 / sk->GetMaxHP()) : 0,
					sk->GetMapIndex() == owner->GetMapIndex() ? "na twojej mapie" : "na innej mapie", mode,
					GetPlayerBotSidekickStanceName(rec.bStance));
		SayPlayerBotSidekick(owner, text);
		// MT2009_PLUS_SIDEKICK_EQUIP_LOCK_V1
		if (IsPlayerBotSidekickEquipLocked(rec.dwSidekickPID))
			SayPlayerBotSidekick(owner, "Ekwipunek zablokowany: nie ruszam tego, co mam na sobie i co dostalem od ciebie.");
#if defined(PLAYERBOT_ENGINE_MT2009)
		// MT2009_PLUS_SIDEKICK_COINS_V1: what it does with its Dragon Coins.
		if (sk)
		{
			TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(rec.dwSidekickPID);
			if (st != s_mapPlayerBotAIStates.end())
				SayPlayerBotSidekick(owner, DescribePlayerBotSidekickCoins(sk, st->second, rec).c_str());
		}
#endif
		// MT2009_PLUS_SIDEKICK_GRAND_MASTER_V1: its Grand Master training, and
		// what holds it.
		if (sk)
		{
			const std::string gm = DescribePlayerBotSidekickGrandMaster(sk);
			if (!gm.empty())
				SayPlayerBotSidekick(owner, gm.c_str());
		}
	}

	// The stance, kept in the record at once: the table is read again every
	// thirty seconds, and an order written behind that read would be undone by
	// it. Answers with the companion's words for it.
	const char* SetPlayerBotSidekickStance(TPlayerBotSidekick& rec, BYTE stance)
	{
		stance = std::min<BYTE>(stance, PLAYERBOT_SIDEKICK_STANCE_PASSIVE);
		if (rec.bStance != stance)
		{
			rec.bStance = stance;
			if (s_bPlayerBotSidekickSettingsColumns)
			{
				char query[160];
				snprintf(query, sizeof(query), "UPDATE player.playerbot_sidekick SET stance=%u WHERE owner_pid=%u",
						(unsigned int)stance, rec.dwOwnerPID);
				std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
			}
			sys_log(0, "PLAYERBOT_SIDEKICK: stance owner=%u pid=%u stance=%u", rec.dwOwnerPID, rec.dwSidekickPID,
					(unsigned int)stance);
		}
		switch (stance)
		{
			case PLAYERBOT_SIDEKICK_STANCE_DEFEND:
				return "Dobra, nie zaczynam walki. Bronie ciebie i siebie, a pomagam, kiedy ty juz walczysz.";
			case PLAYERBOT_SIDEKICK_STANCE_PASSIVE:
				return "Dobra, nie walcze. Oddam tylko temu, kto mnie uderzy.";
			default:
				return "Dobra, bije wszystko, co sie do nas zblizy.";
		}
	}

	bool PlayerBotSidekickHeard(const char* folded, const char* phrase);

	// The stance a whisper asks for, or -1. The longer phrases first: "nie
	// atakuj pierwszy" holds "nie atakuj" and "atakuj pierwszy" both.
	int GetPlayerBotSidekickStanceHeard(const char* folded)
	{
		static const char* const defendWords[] = { "nie atakuj pierwszy", "nie bij pierwszy", "nie zaczynaj",
				"nie zaczepiaj", "bron mnie", "obronnie", "defensywnie" };
		static const char* const passiveWords[] = { "nie walcz", "nie atakuj", "nie bij", "tylko sie bron",
				"pasywnie", "biernie" };
		static const char* const attackWords[] = { "atakuj wszystko", "bij wszystko", "atakuj pierwszy", "atakuj",
				"agresywnie", "walcz" };
		for (size_t i = 0; i < sizeof(defendWords) / sizeof(defendWords[0]); ++i)
			if (PlayerBotSidekickHeard(folded, defendWords[i]))
				return PLAYERBOT_SIDEKICK_STANCE_DEFEND;
		for (size_t i = 0; i < sizeof(passiveWords) / sizeof(passiveWords[0]); ++i)
			if (PlayerBotSidekickHeard(folded, passiveWords[i]))
				return PLAYERBOT_SIDEKICK_STANCE_PASSIVE;
		for (size_t i = 0; i < sizeof(attackWords) / sizeof(attackWords[0]); ++i)
			if (PlayerBotSidekickHeard(folded, attackWords[i]))
				return PLAYERBOT_SIDEKICK_STANCE_ATTACK;
		return -1;
	}

	void SummonPlayerBotSidekick(LPCHARACTER owner, TPlayerBotSidekick& rec, DWORD dwNow)
	{
		if (rec.bMode != PLAYERBOT_SIDEKICK_FOLLOW)
		{
			rec.bMode = PLAYERBOT_SIDEKICK_FOLLOW;
			DBManager::instance().Query("UPDATE player.playerbot_sidekick SET mode=0 WHERE owner_pid=%u", rec.dwOwnerPID);
			sys_log(0, "PLAYERBOT_SIDEKICK: summoned back owner=%u pid=%u", rec.dwOwnerPID, rec.dwSidekickPID);
		}
		// Waiting at a spot, or sent to town: the call ends either.
		std::map<DWORD, TPlayerBotSidekickRuntime>::iterator rtIt = s_mapPlayerBotSidekickRuntime.find(rec.dwSidekickPID);
		const bool wasOnErrand = rtIt != s_mapPlayerBotSidekickRuntime.end() && rtIt->second.bErrand;
		if (rtIt != s_mapPlayerBotSidekickRuntime.end())
		{
			rtIt->second.bHold = false;
			rtIt->second.bErrand = false;
			rtIt->second.bFishing = false;
		}
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(rec.dwSidekickPID);
		if (!sk || st == s_mapPlayerBotAIStates.end() || !owner->GetSectree())
		{
			SayPlayerBotSidekick(owner, "Juz ide - pojawie sie przy tobie za chwile.");
			return;
		}
		if (sk->IsDead())
		{
			SayPlayerBotSidekick(owner, "Najpierw musze wstac - zaraz bede.");
			return;
		}
		// Whatever it was doing on its own ends here.
		TPlayerBotAIState& state = st->second;
		state.bTownVisitPhase = BOT_TOWN_PHASE_NONE;
		state.bMarketTrip = false;
		state.bFishingSession = false;
		if (wasOnErrand)
			state.bVisitingShop = false;
		PlacePlayerBotSidekick(sk, state, owner, dwNow, "sidekick_summoned");
		KeepPlayerBotSidekickInParty(sk, owner, dwNow);
		char text[128];
		snprintf(text, sizeof(text), "%s: jestem!", sk->GetName());
		SayPlayerBotSidekick(owner, text);
	}

	void FreePlayerBotSidekick(LPCHARACTER owner, TPlayerBotSidekick& rec)
	{
		// Let off the leash in town, it goes on with the visit as its own.
		std::map<DWORD, TPlayerBotSidekickRuntime>::iterator rtIt = s_mapPlayerBotSidekickRuntime.find(rec.dwSidekickPID);
		if (rtIt != s_mapPlayerBotSidekickRuntime.end())
		{
			rtIt->second.bHold = false;
			rtIt->second.bErrand = false;
			rtIt->second.bFishing = false;
		}
		// What it was fighting at the owner's side stays with the owner.
		if (rec.bMode == PLAYERBOT_SIDEKICK_FOLLOW)
			HandPlayerBotSidekickFoesBeforeLeaving(rec.dwSidekickPID, owner, "free");
		rec.bMode = PLAYERBOT_SIDEKICK_FREE;
		DBManager::instance().Query("UPDATE player.playerbot_sidekick SET mode=1 WHERE owner_pid=%u", rec.dwOwnerPID);
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
		if (sk && sk->GetParty() && owner->GetParty() == sk->GetParty())
			LeavePlayerBotParty(sk);
		SayPlayerBotSidekick(owner, "Ide expic po swojemu. Zawolaj mnie z listu Towarzysz, kiedy bede potrzebny.");
		sys_log(0, "PLAYERBOT_SIDEKICK: let off the leash owner=%u pid=%u", rec.dwOwnerPID, rec.dwSidekickPID);
	}

	void DismissPlayerBotSidekick(LPCHARACTER owner, TPlayerBotSidekick rec)
	{
		if (rec.bMode == PLAYERBOT_SIDEKICK_FOLLOW)
			HandPlayerBotSidekickFoesBeforeLeaving(rec.dwSidekickPID, owner, "dismissed");
		DBManager::instance().Query("DELETE FROM player.playerbot_sidekick WHERE owner_pid=%u", rec.dwOwnerPID);
		DBManager::instance().Query("DELETE FROM player.playerbot_sidekick_gift WHERE sidekick_pid=%u", rec.dwSidekickPID);
		if (s_bPlayerBotSidekickPinTable)
			DBManager::instance().Query("DELETE FROM player.playerbot_sidekick_pin WHERE sidekick_pid=%u",
					rec.dwSidekickPID);
		s_mapPlayerBotSidekicks.erase(rec.dwOwnerPID);
		s_mapPlayerBotSidekickOwner.erase(rec.dwSidekickPID);
		s_mapPlayerBotSidekickRuntime.erase(rec.dwSidekickPID);
		s_setPlayerBotSidekickEquipLock.erase(rec.dwSidekickPID);	// MT2009_PLUS_SIDEKICK_EQUIP_LOCK_V1
		s_setPlayerBotSidekickNoKeep.erase(rec.dwSidekickPID);	// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1
		SetPlayerBotSidekickFlag(rec.dwOwnerPID, "towarzysz.created", 0);
		if (CPlayerBotManager::instance().IsManaged(rec.dwSidekickPID))
		{
			LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
			if (sk && sk->GetParty())
				LeavePlayerBotParty(sk);
			CPlayerBotManager::instance().Despawn(rec.dwSidekickPID, playerbot_session_rules::OUT_SIDEKICK); // MT2009_PLUS_BOT_SESSIONS_V1
		}
#if defined(PLAYERBOT_ENGINE_MT2009)
		// The record goes with this call, so the owner's client hears of the
		// empty place here or nowhere (SendPlayerBotSidekickBody).
		std::map<DWORD, TPlayerBotSidekickBodySent>::iterator body =
				s_mapPlayerBotSidekickBodySent.find(rec.dwOwnerPID);
		if (body != s_mapPlayerBotSidekickBodySent.end())
		{
			if (body->second.dwVid != 0 && owner && owner->GetDesc() && !owner->GetDesc()->IsBot())
				owner->ChatPacket(CHAT_TYPE_COMMAND, "SidekickVid 0");
			s_mapPlayerBotSidekickBodySent.erase(body);
		}
#endif
		SayPlayerBotSidekick(owner, "Towarzysz odszedl. Nowego mozesz wybrac w liscie Towarzysz.");
		sys_log(0, "PLAYERBOT_SIDEKICK: dismissed owner=%u pid=%u", rec.dwOwnerPID, rec.dwSidekickPID);
	}

	// One of the window's switches, kept in the record at once and in the table
	// when it has the column (see SetPlayerBotSidekickStance for why at once).
	void SetPlayerBotSidekickSetting(const TPlayerBotSidekick& rec, const char* column, unsigned int value)
	{
		if (s_bPlayerBotSidekickSettingsColumns)
		{
			char query[192];
			snprintf(query, sizeof(query), "UPDATE player.playerbot_sidekick SET %s=%u WHERE owner_pid=%u", column, value,
					rec.dwOwnerPID);
			std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		}
		sys_log(0, "PLAYERBOT_SIDEKICK: setting owner=%u pid=%u %s=%u", rec.dwOwnerPID, rec.dwSidekickPID, column, value);
	}

	// The lure switch, kept in the record at once (see the stance). Answers
	// with the companion's words for it.
	const char* SetPlayerBotSidekickLure(TPlayerBotSidekick& rec, bool lure)
	{
		if (rec.bLure != lure)
		{
			rec.bLure = lure;
			SetPlayerBotSidekickSetting(rec, "lure", lure ? 1U : 0U);
		}
		if (!lure)
			return "Dobra, nie luruje - walcze przy tobie.";
		if (rec.bStance == PLAYERBOT_SIDEKICK_STANCE_PASSIVE)
			return "Lurowanie wlaczone, ale teraz nie walcze - zmien walke na \"Atakuj\" albo \"Nie 1. atak\", a zaczne.";
		return "Dobra, luruje: kiedy stoisz na spocie, sciagam do 3 grup potworow z okolicy i przyprowadzam je do ciebie.";
	}

	// "Gra beze mnie", kept in the record at once (see the stance). Answers
	// with the companion's words for it.
	const char* SetPlayerBotSidekickSolo(TPlayerBotSidekick& rec, bool solo)
	{
		if (rec.bSolo != solo)
		{
			rec.bSolo = solo;
			SetPlayerBotSidekickSetting(rec, "solo", solo ? 1U : 0U);
		}
		if (!solo)
			return "Dobra, kiedy wyjdziesz z gry, wyjde razem z toba.";
		return "Dobra, kiedy wyjdziesz z gry, gram dalej sam - najwyzej 30 poziomow ponad twoj, zebysmy dalej mogli "
				"expic razem w druzynie.";
	}

	// "Skrzynki", kept in the record at once (see the stance). Answers with the
	// companion's words for it.
	const char* SetPlayerBotSidekickChests(TPlayerBotSidekick& rec, bool open)
	{
		if (rec.bChests != open)
		{
			rec.bChests = open;
			SetPlayerBotSidekickSetting(rec, "chests", open ? 1U : 0U);
		}
		if (open)
			return "Dobra, sam otwieram skrzynie i szkatulki z torby.";
		return "Dobra, nie otwieram skrzyn ani szkatulek - zostaja w mojej torbie, mozesz je wziac w oknie Towarzysza.";
	}

	// ------------------------------------------- MT2009_PLUS_SIDEKICK_COINS_V1
	//
	// "Smocze Monety: wydaje / nie wydaje" (Derpsonkowy95's five thousand
	// coins that bought nothing; the operator, 1 October). A companion went
	// through the shop pass as any bot, behind the world's ISHOP switch, and
	// bought what every bot does - a hairstyle first. Now its owner's switch
	// decides (on by default): on, it cashes every Kupon SM in its bag at once
	// and buys in the Item Shop only what it uses (CollectPlayerBotItemShopNeeds:
	// the Kamien Duchowy for a Grand Master skill, the change stone for its
	// weapon, the Blessing Scroll, the Exorcism Scroll and the Rada for its
	// books), never a look, and says every purchase and its price; off, the
	// vouchers stay in its bag for its owner to take in the window and the
	// coins stay on its account.
	bool IsPlayerBotSidekickCoinsOn(DWORD pid)
	{
		std::map<DWORD, bool>::const_iterator it = s_mapPlayerBotSidekickCoins.find(pid);
		return it == s_mapPlayerBotSidekickCoins.end() || it->second;
	}

	const char* SetPlayerBotSidekickCoins(TPlayerBotSidekick& rec, bool spend)
	{
		if (IsPlayerBotSidekickCoinsOn(rec.dwSidekickPID) != spend)
		{
			s_mapPlayerBotSidekickCoins[rec.dwSidekickPID] = spend;
			if (s_bPlayerBotSidekickCoinsColumn)
				SetPlayerBotSidekickSetting(rec, "coins", spend ? 1U : 0U);
		}
		else
			s_mapPlayerBotSidekickCoins[rec.dwSidekickPID] = spend;
		if (spend)
			return "Dobra, wydaje Smocze Monety w Item Shopie na to, czego potrzebuje, i wymieniam kupony SM z plecaka. "
					"Kazdy zakup ci zglosze.";
		return "Dobra, nie wydaje Smoczych Monet. Kupony SM zostaja w moim plecaku - mozesz je wziac w oknie Towarzysza.";
	}

	// MT2009_PLUS_SIDEKICK_EQUIP_LOCK_V1: "Zablokuj ekwipunek", kept at once.
	const char* SetPlayerBotSidekickEquipLock(TPlayerBotSidekick& rec, bool locked)
	{
		if (IsPlayerBotSidekickEquipLocked(rec.dwSidekickPID) != locked)
		{
			if (locked)
				s_setPlayerBotSidekickEquipLock.insert(rec.dwSidekickPID);
			else
				s_setPlayerBotSidekickEquipLock.erase(rec.dwSidekickPID);
			if (s_bPlayerBotSidekickEquipLockColumn)
				SetPlayerBotSidekickSetting(rec, "equipment_lock", locked ? 1U : 0U);
		}
		if (locked)
			return "Dobra, ekwipunek zablokowany: nie ulepszam, nie zmieniam bonusow, nie zdejmuje, nie sprzedaje "
					"i nie wyrzucam tego, co mam na sobie i co dostalem od ciebie. Ty mozesz przekladac moje rzeczy.";
		return "Dobra, ekwipunek odblokowany - sam dbam o swoj sprzet.";
	}

	// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: "Pelne EQ", kept at once.
	const char* SetPlayerBotSidekickKeepLoot(TPlayerBotSidekick& rec, bool keep)
	{
		if (IsPlayerBotSidekickKeepingLoot(rec.dwSidekickPID) != keep)
		{
			if (keep)
				s_setPlayerBotSidekickNoKeep.erase(rec.dwSidekickPID);
			else
				s_setPlayerBotSidekickNoKeep.insert(rec.dwSidekickPID);
			if (s_bPlayerBotSidekickKeepColumn)
				SetPlayerBotSidekickSetting(rec, "keep_loot", keep ? 1U : 0U);
		}
		if (keep)
			return "Dobra, gdy nie zmiescisz swojego dropu, podnosze go do swojego plecaka i trzymam dla ciebie. "
					"Prawy klik na nim w moim plecaku oddaje ci go.";
		return "Dobra, twojego dropu przy pelnym ekwipunku nie podnosze - zostaje na ziemi.";
	}

#if defined(PLAYERBOT_ENGINE_MT2009)
	// What a wish of the shop pass is for, in the companion's words.
	const char* GetPlayerBotSidekickWishWords(const char* reason)
	{
		if (!reason)
			return "na zakupy";
		if (!strcmp(reason, "grand_master_stone"))
			return "do treningu Wielkiego Mistrza";
		if (!strcmp(reason, "change_stone"))
			return "do zmiany bonusow u kowala";
		if (!strcmp(reason, "blessing_scroll"))
			return "do ulepszania u kowala";
		if (!strcmp(reason, "exorcism_scroll") || !strcmp(reason, "rada_pustelnika"))
			return "do czytania ksiag";
		if (!strcmp(reason, "metin_detector"))
			return "do szukania Metinow";
		if (!strcmp(reason, "teleport_ring"))
			return "do teleportow";
		if (!strcmp(reason, "raid_booster"))
			return "na walke";
		return "na to, czego uzywam";
	}

	void NotePlayerBotSidekickVouchers(LPCHARACTER sk, long long coins, int balance)
	{
		TPlayerBotSidekick* rec = sk ? FindPlayerBotSidekickOf(sk->GetPlayerID()) : NULL;
		if (!rec)
			return;
		char text[200];
		snprintf(text, sizeof(text), "Wymienilem kupony SM na %lld Smoczych Monet - mam ich teraz %d. "
				"Kupie za nie, czego potrzebuje.", coins, balance);
		SayPlayerBotSidekick(GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID), text);
	}

	void NotePlayerBotSidekickItemShopBuy(LPCHARACTER sk, DWORD vnum, DWORD count, DWORD price, bool marks,
			const char* reason, int left)
	{
		TPlayerBotSidekick* rec = sk ? FindPlayerBotSidekickOf(sk->GetPlayerID()) : NULL;
		if (!rec)
			return;
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		char text[240];
		snprintf(text, sizeof(text), "Kupilem w Item Shopie: %s x%u za %u %s, %s (zostalo %d).",
				proto ? proto->szLocaleName : "przedmiot", (unsigned int)count, (unsigned int)price,
				marks ? "Smoczych Znakow" : "Smoczych Monet", GetPlayerBotSidekickWishWords(reason), left);
		SayPlayerBotSidekick(GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID), text);
		sys_log(0, "PLAYERBOT_SIDEKICK: itemshop bought pid=%u name=%s owner=%u vnum=%u x%u price=%u %s reason=%s left=%d",
				sk->GetPlayerID(), sk->GetName(), rec->dwOwnerPID, vnum, (unsigned int)count, (unsigned int)price,
				marks ? "marks" : "coins", reason ? reason : "-", left);
	}

	// The "Raport"'s line of the coins: the balance, and what it saves for,
	// buys next or does not need.
	std::string DescribePlayerBotSidekickCoins(LPCHARACTER sk, TPlayerBotAIState& state, const TPlayerBotSidekick& rec)
	{
		char text[320];
		if (!IsPlayerBotSidekickCoinsOn(rec.dwSidekickPID))
		{
			snprintf(text, sizeof(text), "Smocze Monety: nie wydaje (mam %d). Kupony SM zostaja w moim plecaku.",
					state.bDragonBalanceKnown ? state.iDragonCoins : 0);
			return text;
		}
		RefreshPlayerBotItemShopCatalogue(get_dword_time());
		if (!state.bDragonBalanceKnown)
			RefreshPlayerBotDragonBalance(sk, state, get_dword_time());
		TPlayerBotItemShopWish wishes[PLAYERBOT_ISHOP_MAX_WISHES];
		const int n = CollectPlayerBotItemShopNeeds(sk, state, wishes);
		char what[200] = "";
		for (int i = 0; i < n; ++i)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(wishes[i].dwVnum);
			const DWORD price = GetPlayerBotItemShopCheapest(sk, wishes[i].dwVnum, wishes[i].bMarks);
			if (!proto || price == 0)
				continue;
			const int have = wishes[i].bMarks ? state.iDragonMarks : state.iDragonCoins;
			if (have >= (int)price)
				snprintf(what, sizeof(what), "Kupie %s (%u %s) %s przy nastepnym zajrzeniu do Item Shopu.",
						proto->szLocaleName, (unsigned int)price, wishes[i].bMarks ? "Smoczych Znakow" : "Smoczych Monet",
						GetPlayerBotSidekickWishWords(wishes[i].szReason));
			else
				snprintf(what, sizeof(what), "Odkladam Smocze Monety na %s (%u), %s - mam %d, brakuje %d.",
						proto->szLocaleName, (unsigned int)price, GetPlayerBotSidekickWishWords(wishes[i].szReason),
						have, (int)price - have);
			break;
		}
		if (!*what)
			snprintf(what, sizeof(what), "Wydaje je w Item Shopie na to, czego uzywam - Kamienie Duchowe, zmiane bonusow, "
					"zwoje do ulepszania i do ksiag - ale teraz nic z tego mi nie trzeba.");
		snprintf(text, sizeof(text), "Smocze Monety: %d, Smocze Znaki: %d. %s", state.iDragonCoins, state.iDragonMarks, what);
		return text;
	}
#endif

	// The roles a leader's Leadership opens (CParty::Update's
	// m_anMaxRole): the level each wants and the client's own name for it.
	int GetPlayerBotSidekickRoleLeadership(BYTE role)
	{
		switch (role)
		{
			case PARTY_ROLE_DEFENDER: return 1;
			case PARTY_ROLE_ATTACKER: return 10;
			case PARTY_ROLE_BUFFER: return 10;
			case PARTY_ROLE_HASTE: return 15;
			case PARTY_ROLE_TANKER: return 20;
			case PARTY_ROLE_SKILL_MASTER: return 20;
			default: return 0;
		}
	}

	const char* GetPlayerBotSidekickRoleName(BYTE role)
	{
		switch (role)
		{
			case PARTY_ROLE_ATTACKER: return "Atakujacy (wartosc ataku)";
			case PARTY_ROLE_TANKER: return "Walczacy w zwarciu (maks. PZ)";
			case PARTY_ROLE_BUFFER: return "Blokujacy (czas trwania)";
			case PARTY_ROLE_SKILL_MASTER: return "Mistrz umiejetnosci";
			case PARTY_ROLE_HASTE: return "Berserker (szybkosc ataku)";
			case PARTY_ROLE_DEFENDER: return "Obronca (obrona)";
			default: return "bez bonusu";
		}
	}

	// "Lider grupy", kept in the record at once (see the stance); the party
	// changes hands on the next look (KeepPlayerBotSidekickInParty).
	const char* SetPlayerBotSidekickLead(TPlayerBotSidekick& rec, bool lead)
	{
		if (rec.bLead != lead)
		{
			rec.bLead = lead;
			SetPlayerBotSidekickSetting(rec, "lead", lead ? 1U : 0U);
		}
		if (lead)
			return "Dobra, to ja zakladam grupe i ciebie zapraszam. Wybierz, jaki bonus z mojego Dowodzenia ci dac.";
		return "Dobra, ty prowadzisz grupe - dolacze do twojej.";
	}

	// The owner's role in its companion's party.
	std::string SetPlayerBotSidekickRole(TPlayerBotSidekick& rec, BYTE role, int leadership)
	{
		if (role < PARTY_ROLE_ATTACKER || role > PARTY_ROLE_DEFENDER)
			role = PARTY_ROLE_NORMAL;
		if (rec.bRole != role)
		{
			rec.bRole = role;
			SetPlayerBotSidekickSetting(rec, "role", role);
		}
		char text[192];
		const int need = GetPlayerBotSidekickRoleLeadership(role);
		if (role == PARTY_ROLE_NORMAL)
			snprintf(text, sizeof(text), "Dobra, nie daje ci bonusu z Dowodzenia.");
		else if (!rec.bLead)
			snprintf(text, sizeof(text), "Bonus: %s. Dam ci go, kiedy bede liderem grupy (Lider grupy: Towarzysz).",
					GetPlayerBotSidekickRoleName(role));
		else if (leadership < need)
			snprintf(text, sizeof(text), "Bonus: %s - potrzebuje do niego Dowodzenia %d, mam %d. Daj mi Ksiege Dowodzenia.",
					GetPlayerBotSidekickRoleName(role), need, leadership);
		else
			snprintf(text, sizeof(text), "Dobra, daje ci bonus: %s.", GetPlayerBotSidekickRoleName(role));
		return text;
	}

	// The owner's role, as the leader's click on it would set it
	// (CInputMain::PartySetState): the party's own SetRole and the state
	// change for the db core, which tells the other cores.
	void SetPlayerBotSidekickPartyRole(LPPARTY party, DWORD leaderPid, DWORD pid, BYTE role, bool on)
	{
		if (!party->SetRole(pid, role, on))
			return;
		TPacketPartyStateChange pack;
		pack.dwLeaderPID = leaderPid;
		pack.dwPID = pid;
		pack.bRole = role;
		pack.bFlag = on ? 1 : 0;
		if (db_clientdesc)
			db_clientdesc->DBPacket(HEADER_GD_PARTY_STATE_CHANGE, 0, &pack, sizeof(pack));
	}

	void ApplyPlayerBotSidekickRole(LPCHARACTER ch, LPCHARACTER owner, const TPlayerBotSidekick& rec, LPPARTY party)
	{
		const DWORD ownerPid = owner->GetPlayerID();
		const BYTE current = party->GetRole(ownerPid);
		const BYTE want = rec.bRole;
		if (current == want)
			return;
		if (current != PARTY_ROLE_NORMAL && current != PARTY_ROLE_LEADER)
			SetPlayerBotSidekickPartyRole(party, ch->GetPlayerID(), ownerPid, current, false);
		// Refused while the Leadership is short of it (m_anMaxRole): asked again
		// on the next look, the level may have come with a book.
		if (want != PARTY_ROLE_NORMAL)
			SetPlayerBotSidekickPartyRole(party, ch->GetPlayerID(), ownerPid, want, true);
	}

	// "Grupa", kept in the record at once (see the stance): whether the
	// companion follows its owner into a party somebody else leads. Answers
	// with the companion's words for it.
	const char* SetPlayerBotSidekickParty(TPlayerBotSidekick& rec, bool join)
	{
		if (rec.bParty != join)
		{
			rec.bParty = join;
			SetPlayerBotSidekickSetting(rec, "party", join ? 1U : 0U);
		}
		if (join)
			return "Dobra, dolaczam do twojej grupy, nawet gdy prowadzi ja ktos inny - jesli zostanie w niej miejsce "
					"jeszcze dla jednej osoby.";
		return "Dobra, do grupy, ktora prowadzi ktos inny, dolacze tylko na zaproszenie jej lidera.";
	}

	// The companion in this core's world with its state, or NULL with the owner
	// told why not.
	LPCHARACTER FindPlayerBotSidekickForOrder(LPCHARACTER owner, const TPlayerBotSidekick& rec,
			TPlayerBotAIState** state)
	{
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(rec.dwSidekickPID);
		if (!sk || st == s_mapPlayerBotAIStates.end() || !CPlayerBotManager::instance().IsManaged(rec.dwSidekickPID))
		{
			SayPlayerBotSidekick(owner, "Jeszcze mnie nie ma w grze - chwila.");
			return NULL;
		}
		if (sk->IsDead())
		{
			SayPlayerBotSidekick(owner, "Najpierw musze wstac.");
			return NULL;
		}
		*state = &st->second;
		return sk;
	}

	bool PlayerBotSidekickHasFishingPass(LPCHARACTER sk)
	{
		if (!sk)
			return false;
		if (sk->IsEquipUniqueItem(UNIQUE_ITEM_FISHING_PASS))
			return true;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = sk->GetInventoryItem(cell);
			if (item && item->GetVnum() == UNIQUE_ITEM_FISHING_PASS)
				return true;
		}
		return false;
	}

	bool IsPlayerBotSidekickFishing(DWORD pid)
	{
		std::map<DWORD, TPlayerBotSidekickRuntime>::const_iterator it = s_mapPlayerBotSidekickRuntime.find(pid);
		return it != s_mapPlayerBotSidekickRuntime.end() && it->second.bFishing;
	}

	// "Na ryby" (the operator, 28 September: "towarzysz ... na lowienie ryb i
	// lowi wtedy ryby przez jeden karnet rybacki"). It goes to the water with
	// the Fishing Card it carries - the owner hands one over through its bag
	// window; it buys none for this - and fishes as an angler bot does
	// (ManagePlayerBotFishing) until the card runs out, then comes back.
	// Called back earlier with "Przywolaj". The bank is the one of the map it
	// stands on, or of its kingdom's first village when this core hosts it.
	void SendPlayerBotSidekickFishing(LPCHARACTER owner, TPlayerBotSidekick& rec, DWORD dwNow)
	{
		TPlayerBotAIState* state = NULL;
		LPCHARACTER sk = FindPlayerBotSidekickForOrder(owner, rec, &state);
		if (!sk)
			return;
		TPlayerBotSidekickRuntime& rt = s_mapPlayerBotSidekickRuntime[rec.dwSidekickPID];
		if (rt.bFishing)
		{
			SayPlayerBotSidekick(owner, "Juz lowie ryby. Zawolaj mnie (Przywolaj), jesli mam wrocic wczesniej.");
			return;
		}
		if (rt.bErrand)
		{
			SayPlayerBotSidekick(owner, "Jestem na zakupach - najpierw mnie zawolaj.");
			return;
		}
		if ((int)sk->GetLevel() < PLAYERBOT_FISHING_MIN_LEVEL)
		{
			char text[128];
			snprintf(text, sizeof(text), "Na ryby potrzebuje %d poziomu.", PLAYERBOT_FISHING_MIN_LEVEL);
			SayPlayerBotSidekick(owner, text);
			return;
		}
		if (!PlayerBotSidekickHasFishingPass(sk))
		{
			SayPlayerBotSidekick(owner, "Daj mi Karte Wedkarska (w oknie mojej torby) - bez niej nie moge lowic.");
			return;
		}
		if (!GetPlayerBotFishingBank(sk->GetMapIndex()))
		{
			const long village = playerbot_empire_rules::GetHomeMap((int)sk->GetEmpire(),
					playerbot_empire_rules::MAP_ROLE_M1);
			const TPlayerBotFishingBank* bank = village ? GetPlayerBotFishingBank(village) : NULL;
			if (!bank || !IsPlayerBotMapHostedHere(village) ||
					!PlacePlayerBotSidekickAt(sk, *state, village, bank->centre.x, bank->centre.y, dwNow,
							"sidekick_fishing"))
			{
				SayPlayerBotSidekick(owner, "Stad nie dojde nad wode - zabierz mnie do pierwszej wioski, tam jest lowisko.");
				return;
			}
		}
		rt.bHold = false;
		rt.bFishing = true;
		rt.dwFishingSince = dwNow;
		if (rec.bMode == PLAYERBOT_SIDEKICK_FOLLOW)
			HandPlayerBotSidekickFoesBeforeLeaving(rec.dwSidekickPID, owner, "fishing");
		rec.bMode = PLAYERBOT_SIDEKICK_FREE;
		DBManager::instance().Query("UPDATE player.playerbot_sidekick SET mode=1 WHERE owner_pid=%u", rec.dwOwnerPID);
		if (sk->GetParty() && owner->GetParty() == sk->GetParty())
			LeavePlayerBotParty(sk);
		state->dwTargetVID = 0;
		sk->SetVictim(NULL);
		state->bFishingSession = false;
		state->dwNextFishingCheckTime = 0;
		SayPlayerBotSidekick(owner, "Ide na ryby. Lowie, dopoki Karta Wedkarska nie wygasnie - zawolaj mnie (Przywolaj), jesli bede potrzebny.");
		sys_log(0, "PLAYERBOT_SIDEKICK: sent fishing owner=%u pid=%u map=%ld", rec.dwOwnerPID, rec.dwSidekickPID,
				sk->GetMapIndex());
	}

	// At the water: the next session straight after the last, and home when
	// the card is gone. True when it came back this tick.
	bool KeepPlayerBotSidekickFishing(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt, DWORD dwNow)
	{
		if (!rt.bFishing)
			return false;
		if (rec.bMode == PLAYERBOT_SIDEKICK_FOLLOW)
		{
			rt.bFishing = false;
			return false;
		}
		if (!state.bFishingSession)
			state.dwNextFishingCheckTime = 0;
		// A card that expired is taken out of the bag by the engine; the first
		// half minute is the walk to the bank with the card still in the bag.
		if (PlayerBotSidekickHasFishingPass(ch) || dwNow - rt.dwFishingSince < 30000)
			return false;
		rt.bFishing = false;
		sys_log(0, "PLAYERBOT_SIDEKICK: fishing over, the card ran out owner=%u pid=%u", rec.dwOwnerPID,
				rec.dwSidekickPID);
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec.dwOwnerPID);
		if (owner)
		{
			SayPlayerBotSidekick(owner, "Karta Wedkarska sie skonczyla - wracam do ciebie.");
			SummonPlayerBotSidekick(owner, rec, dwNow);
			return true;
		}
		rec.bMode = PLAYERBOT_SIDEKICK_FOLLOW;
		DBManager::instance().Query("UPDATE player.playerbot_sidekick SET mode=0 WHERE owner_pid=%u", rec.dwOwnerPID);
		state.bFishingSession = false;
		return true;
	}

	// "Czekaj tutaj": it stays where its owner stands now - put there first
	// when it is anywhere else - and keeps the spot until it is called.
	void HoldPlayerBotSidekick(LPCHARACTER owner, TPlayerBotSidekick& rec, DWORD dwNow)
	{
		TPlayerBotAIState* state = NULL;
		LPCHARACTER sk = FindPlayerBotSidekickForOrder(owner, rec, &state);
		if (!sk)
			return;
		TPlayerBotSidekickRuntime& rt = s_mapPlayerBotSidekickRuntime[rec.dwSidekickPID];
		if (rt.bErrand)
		{
			SayPlayerBotSidekick(owner, "Jestem na zakupach. Zawolaj mnie, jesli mam wrocic wczesniej.");
			return;
		}
		if (rec.bMode != PLAYERBOT_SIDEKICK_FOLLOW)
		{
			rec.bMode = PLAYERBOT_SIDEKICK_FOLLOW;
			DBManager::instance().Query("UPDATE player.playerbot_sidekick SET mode=0 WHERE owner_pid=%u", rec.dwOwnerPID);
		}
		if (!owner->GetSectree())
			return;
		if (sk->GetMapIndex() != owner->GetMapIndex() ||
				DISTANCE_APPROX(sk->GetX() - owner->GetX(), sk->GetY() - owner->GetY()) > PLAYERBOT_SIDEKICK_TELEPORT_DISTANCE)
		{
			PlacePlayerBotSidekick(sk, *state, owner, dwNow, "sidekick_hold");
			KeepPlayerBotSidekickInParty(sk, owner, dwNow);
		}
		rt.bHold = true;
		rt.lHoldMap = sk->GetMapIndex();
		rt.lHoldX = sk->GetX();
		rt.lHoldY = sk->GetY();
		SayPlayerBotSidekick(owner, "Czekam tutaj. Zawolaj mnie, kiedy bede potrzebny.");
		sys_log(0, "PLAYERBOT_SIDEKICK: holds pid=%u owner=%u map=%ld x=%ld y=%ld", rec.dwSidekickPID, rec.dwOwnerPID,
				rt.lHoldMap, rt.lHoldX, rt.lHoldY);
	}

	// Back from town - done, called off, or with nothing to do - beside the
	// owner, with what it spent and what it carries.
	void EndPlayerBotSidekickErrand(LPCHARACTER sk, TPlayerBotAIState& state, TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt, DWORD dwNow, const char* why)
	{
		const long long spent = rt.llErrandGold - (long long)sk->GetGold();
		size_t red = 0, blue = 0;
		CountPlayerBotPotions(sk, red, blue);
		const bool visited = rt.bErrandVisit;
		rt.bErrand = false;
		rt.bErrandVisit = false;
		state.bVisitingShop = false;
		state.bTownVisitPhase = BOT_TOWN_PHASE_NONE;
		state.bMarketTrip = false;
		state.bFishingSession = false;
		char text[192];
		if (!visited)
			snprintf(text, sizeof(text), "Nie mialem nic do zalatwienia w miescie - wracam.");
		else
			snprintf(text, sizeof(text), "Wracam z zakupow: wydalem %lld yang, mam %u czerwonych i %u niebieskich mikstur.",
					spent > 0 ? spent : 0LL, (unsigned int)red, (unsigned int)blue);
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec.dwOwnerPID);
		if (owner)
		{
			SayPlayerBotSidekick(owner, text);
			if (owner->GetSectree() && !sk->IsDead())
			{
				PlacePlayerBotSidekick(sk, state, owner, dwNow, "sidekick_errand_back");
				KeepPlayerBotSidekickInParty(sk, owner, dwNow);
			}
		}
		sys_log(0, "PLAYERBOT_SIDEKICK: errand over pid=%u owner=%u why=%s visited=%d spent=%lld red=%u blue=%u",
				rec.dwSidekickPID, rec.dwOwnerPID, why, visited ? 1 : 0, spent, (unsigned int)red, (unsigned int)blue);
	}

	// "Idz na zakupy": at its owner's side it never goes to town by itself, so
	// its potions run out and its junk fills the bag. This takes it to its own
	// kingdom's first village, where every service stands, and runs the town
	// visit any bot runs - the merchant, the potions, the blacksmith, the
	// storekeeper, whatever it needs - then brings it back
	// (ManagePlayerBotSidekickErrand).
	void SendPlayerBotSidekickShopping(LPCHARACTER owner, TPlayerBotSidekick& rec, DWORD dwNow)
	{
		TPlayerBotAIState* state = NULL;
		LPCHARACTER sk = FindPlayerBotSidekickForOrder(owner, rec, &state);
		if (!sk)
			return;
		TPlayerBotSidekickRuntime& rt = s_mapPlayerBotSidekickRuntime[rec.dwSidekickPID];
		if (rt.bErrand)
		{
			SayPlayerBotSidekick(owner, "Juz jestem na zakupach.");
			return;
		}
		if (rec.bMode == PLAYERBOT_SIDEKICK_FREE)
		{
			SayPlayerBotSidekick(owner, "Gram teraz po swojemu, zakupy robie sam. Zawolaj mnie najpierw.");
			return;
		}
		long map = 0, x = 0, y = 0;
		if (!GetPlayerBotVillageReturn(sk, playerbot_empire_rules::MAP_ROLE_M1, map, x, y) ||
				!IsPlayerBotMapHostedHere(map))
		{
			SayPlayerBotSidekick(owner, "Stad nie dojde do swojego miasta.");
			return;
		}
		rt.bHold = false;
		// The monsters it held stay with the owner rather than follow it into
		// town or walk home.
		HandPlayerBotSidekickFoesToOwner(sk, owner, rt, dwNow, "errand");
		rt.bLureStage = 0;
		rt.dwLureVID = 0;
		if (sk->GetParty())
			LeavePlayerBotParty(sk);
		if (sk->GetMapIndex() != map && !TransitionPlayerBotMap(sk, *state, map, x, y, dwNow, "sidekick_errand"))
		{
			KeepPlayerBotSidekickInParty(sk, owner, dwNow);
			SayPlayerBotSidekick(owner, "Nie udalo mi sie dojsc do miasta.");
			return;
		}
		rt.bErrand = true;
		rt.bErrandVisit = false;
		rt.dwErrandSince = dwNow;
		rt.llErrandGold = (long long)sk->GetGold();
		state->bVisitingShop = false;
		state->dwNextShopCheckTime = 0;
		StartPlayerBotTownVisit(sk, *state, dwNow);
		rt.bErrandVisit = state->bVisitingShop;
		++s_uPlayerBotSidekickErrands;
		sys_log(0, "PLAYERBOT_SIDEKICK: errand pid=%u owner=%u map=%ld visit=%d gold=%lld", rec.dwSidekickPID,
				rec.dwOwnerPID, map, rt.bErrandVisit ? 1 : 0, rt.llErrandGold);
		if (!rt.bErrandVisit)
		{
			EndPlayerBotSidekickErrand(sk, *state, rec, rt, dwNow, "nothing");
			return;
		}
		char text[160];
		snprintf(text, sizeof(text), "Ide na zakupy %s. Wroce, jak skoncze.", playerbot_conv::GetMapWords(map).to);
		SayPlayerBotSidekick(owner, text);
	}

	// ------------------------------------------------------------ the window

	// A text for the window: hex of its bytes, "-" for none. The client splits
	// a command on its spaces, and names have them.
	std::string EncodePlayerBotSidekickText(const char* text, size_t maxBytes)
	{
		static const char kDigits[] = "0123456789abcdef";
		std::string out;
		size_t n = 0;
		for (const unsigned char* p = (const unsigned char*)(text ? text : ""); *p && n < maxBytes; ++p, ++n)
		{
			const unsigned char c = (*p < 32 || *p == 127) ? '?' : *p;
			out += kDigits[c >> 4];
			out += kDigits[c & 15];
		}
		return out.empty() ? std::string("-") : out;
	}

	// Every command to the window goes through here: refused rather than cut
	// when it would not fit CHARACTER::ChatPacket's buffer, and written to the
	// log under the self-test, whose owner is a bot with no client to see it.
	void SendPlayerBotSidekickCommand(LPCHARACTER owner, const char* format, ...)
	{
		char text[CHAT_MAX_LEN + 1];
		va_list args;
		va_start(args, format);
		const int len = vsnprintf(text, sizeof(text), format, args);
		va_end(args);
		if (len < 0 || len >= (int)sizeof(text))
		{
			sys_err("PLAYERBOT_SIDEKICK: window command too long (%d) for %s", len, owner->GetName());
			return;
		}
		owner->ChatPacket(CHAT_TYPE_COMMAND, "%s", text);
		if (s_bPlayerBotSidekickSelfTest)
			sys_log(0, "PLAYERBOT_SIDEKICK: window %s", text);
	}

	// What the window says it is doing.
	void DescribePlayerBotSidekickDoing(LPCHARACTER sk, const TPlayerBotAIState& state, const TPlayerBotSidekick& rec,
			const TPlayerBotSidekickRuntime* rt, char* out, size_t size)
	{
		if (sk->IsDead())
		{
			snprintf(out, size, "lezy - zaraz wstanie");
			return;
		}
		// MT2009_PLUS_SIDEKICK_SHOP_ERRAND_V1: what it was sent for, and how many so far.
		if (rt && rt->bErrand && DescribePlayerBotSidekickShopErrand(*rt, out, size))
			return;
		if (rt && rt->bErrand)
		{
			snprintf(out, size, "robi zakupy %s", playerbot_conv::GetMapWords(sk->GetMapIndex()).at);
			return;
		}
		if (rec.bMode == PLAYERBOT_SIDEKICK_FREE)
		{
			char status[128] = "";
			BuildPlayerBotStatusText(sk, state, status, sizeof(status), false);
			const char* text = status;
			if (!strncmp(text, "[PT] ", 5))
				text += 5;
			snprintf(out, size, "%s", *text ? text : "gra po swojemu");
			return;
		}
		if (rt && rt->bLureStage == 1)
		{
			snprintf(out, size, "luruje potwory (%d/%d grup)", rt->iLurePacks, PLAYERBOT_SIDEKICK_LURE_PACKS);
			return;
		}
		if (rt && rt->bLureStage == 2)
		{
			snprintf(out, size, "wraca z potworami (%d grup)", rt->iLurePacks);
			return;
		}
		const char* what = "stoi przy tobie";
		if (state.bCurrentAction == BOT_ACTION_FIGHT)
			what = "walczy";
		else if (state.bCurrentAction == BOT_ACTION_LOOT)
			what = "zbiera drop";
		else if (state.bCurrentAction == BOT_ACTION_REFINE)
			what = "ulepsza u kowala";
		else if (state.bCurrentAction == BOT_ACTION_SHOP)
			what = "u handlarza";
		else if (state.bRecoveringAfterDeath)
			what = "wraca do sil";
		else if (rt && rt->bHold)
			what = "czeka w miejscu";
		else if (state.bCurrentAction == BOT_ACTION_TRAVEL)
			what = "idzie do ciebie";
		snprintf(out, size, "%s", what);
	}

	// The window's snapshot, in three kinds of command, each far under the
	// 512 bytes CHARACTER::ChatPacket formats into (and past which it would
	// read beyond its buffer): the numbers, the three texts, and the worn
	// gear piece by piece, only when it changed unless the window asks for all
	// of it (it does when it opens).
	void SendPlayerBotSidekickWindow(LPCHARACTER owner, bool fullGear)
	{
		if (!owner || !owner->GetDesc() || (owner->GetDesc()->IsBot() && !s_bPlayerBotSidekickSelfTest))
			return;
		TPlayerBotSidekickMap::iterator it = s_mapPlayerBotSidekicks.find(owner->GetPlayerID());
		if (it == s_mapPlayerBotSidekicks.end())
		{
			SendPlayerBotSidekickCommand(owner, "SidekickInfo %d 0", PLAYERBOT_SIDEKICK_WINDOW_PROTOCOL);
			return;
		}
		const TPlayerBotSidekick& rec = it->second;
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec.dwSidekickPID);
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(rec.dwSidekickPID);
		std::map<DWORD, TPlayerBotSidekickRuntime>::iterator rtIt = s_mapPlayerBotSidekickRuntime.find(rec.dwSidekickPID);
		TPlayerBotSidekickRuntime* rt = rtIt == s_mapPlayerBotSidekickRuntime.end() ? NULL : &rtIt->second;
		const bool inWorld = sk && st != s_mapPlayerBotAIStates.end() &&
				CPlayerBotManager::instance().IsManaged(rec.dwSidekickPID);
		// MT2009_PLUS_SIDEKICK_FREE_CORE_V1: off the leash it may play in
		// another core's world - in the game, elsewhere, not "about to come in".
		const bool elsewhere = !inWorld && P2P_MANAGER::instance().FindByPID(rec.dwSidekickPID) != NULL;
		int where = elsewhere ? 2 : 0;
		long dist = 0;
		int expPercent = 0;
		size_t red = 0, blue = 0;
		if (inWorld)
		{
			where = sk->GetMapIndex() == owner->GetMapIndex() ? 1 : 2;
			if (where == 1)
				dist = DISTANCE_APPROX(sk->GetX() - owner->GetX(), sk->GetY() - owner->GetY());
			const long long next = (long long)sk->GetNextExp();
			if (next > 0)
				expPercent = (int)MINMAX(0LL, (long long)sk->GetExp() * 100 / next, 100LL);
			CountPlayerBotPotions(sk, red, blue);
		}
		int mode = rec.bMode == PLAYERBOT_SIDEKICK_FREE ? 1 : 0;
		if (rt && rt->bErrand)
			mode = 3;
		else if (rt && rt->bHold && mode == 0)
			mode = 2;
		// MT2009_PLUS_SIDEKICK_COINS_V1: the coins switch and what its account
		// holds (-1 before the server has read it) after the rank: the window's
		// Options page shows them on "Smocze Monety".
		int coinBalance = -1;
#if defined(PLAYERBOT_ENGINE_MT2009)
		if (inWorld)
		{
			if (!st->second.bDragonBalanceKnown)
				RefreshPlayerBotDragonBalance(sk, st->second, get_dword_time());
			coinBalance = st->second.iDragonCoins;
		}
#endif
		// "Gra beze mnie", "Skrzynki" and "Grupa" last: a window older than they
		// are reads the words it knows and leaves the rest (uisidekick.ParseInfo).
		// MT2009_PLUS_SIDEKICK_RANK_V1: the companion's rank points after them
		// (the alignment over ten, as the character packet carries it), for the
		// rank title on its name in the window, as the player's own shows his.
		// MT2009_PLUS_SIDEKICK_EQUIP_LOCK_V1: "Zablokuj ekwipunek" after the
		// coins' balance, and MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1's "Pelne
		// EQ" after it.
		SendPlayerBotSidekickCommand(owner,
				"SidekickInfo %d 1 %d %d %d %d %d %d %d %d %d %ld %d %u %u %d %d %lld %u %u %d %d %d %d %d %d %u %d %d %d %d %d %d %d",
				PLAYERBOT_SIDEKICK_WINDOW_PROTOCOL,
				inWorld ? (int)sk->GetRaceNum() : -1, inWorld ? (int)sk->GetSkillGroup() : 0,
				inWorld ? sk->GetLevel() : 0, expPercent,
				inWorld ? (int)sk->GetHP() : 0, inWorld ? (int)sk->GetMaxHP() : 0,
				inWorld ? (int)sk->GetSP() : 0, inWorld ? (int)sk->GetMaxSP() : 0,
				where, dist, mode, (unsigned int)rec.bStance, (unsigned int)rec.bLoot, rec.bProtect ? 1 : 0,
				rec.bBuffs ? 1 : 0, inWorld ? (long long)sk->GetGold() : 0LL, (unsigned int)red, (unsigned int)blue,
				inWorld && sk->IsDead() ? 1 : 0, rec.bLure ? 1 : 0, rt ? (int)rt->bLureStage : 0, rec.bSolo ? 1 : 0,
				rec.bChests ? 1 : 0, rec.bLead ? 1 : 0, (unsigned int)rec.bRole,
				inWorld ? sk->GetLeadershipSkillLevel() : 0, rec.bParty ? 1 : 0,
				inWorld ? sk->GetAlignment() / 10 : 0,
				IsPlayerBotSidekickCoinsOn(rec.dwSidekickPID) ? 1 : 0, coinBalance,
				IsPlayerBotSidekickEquipLocked(rec.dwSidekickPID) ? 1 : 0,
				IsPlayerBotSidekickKeepingLoot(rec.dwSidekickPID) ? 1 : 0);
		char doing[96] = "";
		char place[64] = "";
		if (inWorld)
		{
			DescribePlayerBotSidekickDoing(sk, st->second, rec, rt, doing, sizeof(doing));
			long map = sk->GetMapIndex();
			if (map >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN)
				map /= 10000;
			const playerbot_conv::TMapWords& words = playerbot_conv::GetMapWords(map);
			if (*words.name)
				snprintf(place, sizeof(place), "%s", words.name);
			else
				snprintf(place, sizeof(place), "mapa %ld", map);
		}
		else if (elsewhere)
		{
			snprintf(doing, sizeof(doing), "%s", rec.bMode == PLAYERBOT_SIDEKICK_FREE ? "gra po swojemu" : "idzie do ciebie");
			const playerbot_conv::TMapWords& words =
					playerbot_conv::GetMapWords(GetPlayerBotSidekickSavedMap(rec.dwSidekickPID, get_dword_time()));
			snprintf(place, sizeof(place), "%s", *words.name ? words.name : "inna mapa");
		}
		else
			snprintf(doing, sizeof(doing), "za chwile bedzie w grze");
		SendPlayerBotSidekickCommand(owner, "SidekickNames %s %s %s",
				EncodePlayerBotSidekickText(inWorld ? sk->GetName() : "", 24).c_str(),
				EncodePlayerBotSidekickText(place, 40).c_str(),
				EncodePlayerBotSidekickText(doing, 60).c_str());
		if (!inWorld)
			return;
		static const BYTE kWear[8] = { WEAR_WEAPON, WEAR_BODY, WEAR_HEAD, WEAR_SHIELD, WEAR_FOOTS, WEAR_WRIST,
				WEAR_NECK, WEAR_EAR };
		DWORD hash = 2166136261U;
		for (int i = 0; i < 8; ++i)
		{
			LPITEM item = sk->GetWear(kWear[i]);
			hash = (hash ^ (item ? item->GetID() : 0U)) * 16777619U;
			hash = (hash ^ (item ? item->GetVnum() : 0U)) * 16777619U;
		}
		if (!fullGear && rt && rt->dwGearSent == hash)
			return;
		for (int i = 0; i < 8; ++i)
		{
			LPITEM item = sk->GetWear(kWear[i]);
			SendPlayerBotSidekickCommand(owner, "SidekickGear %d %s", i,
					EncodePlayerBotSidekickText(item ? item->GetName() : "", 40).c_str());
		}
		if (rt)
			rt->dwGearSent = hash;
	}

	// ------------------------------------------------ its bag and skills, in a window
	//
	// The companion's bag and worn gear in a window of the owner's client
	// (uisidekickinventory.py; the operator, 25 September: "widoczny jej ekwipunek, ze
	// mozesz itemy przenosic normalnie dowoli"), and its skills beside it. What
	// the owner does there is the owner's word over the AI's:
	// - a piece the owner puts on is pinned (IsPlayerBotSidekickPinned): the
	//   equipment pass never takes it off or ranks anything against it, puts it
	//   back first when something else took it off, the blacksmith does not
	//   refine it and no change stone touches its lines (an added line loses
	//   nothing, so the add stone still may) - the bell with +12 INT that the
	//   equipment pass kept in the bag for a heavier one (GoracyDelfin, 25
	//   September);
	// - a piece the owner takes off is never put back on by the AI
	//   (IsPlayerBotSidekickUnwanted), or the next equipment pass would undo
	//   the owner's hand;
	// - what crosses between the two bags crosses the way a trade moves it
	//   (CExchange::Done), with a trade's refusals but ITEM_ANTIFLAG_GIVE
	//   (GetPlayerBotSidekickHandOverRefusal): nothing locked or in a trade,
	//   and a window that is busy says so.
	// The positions are the protocol's: a bag cell, or 1000 + WEAR_* for a worn
	// piece; -1 is "wherever it fits".

	bool IsPlayerBotSidekickEqBagPos(int pos)
	{
		return pos >= 0 && pos < PLAYERBOT_BAG_CELLS;
	}

	bool IsPlayerBotSidekickEqWearPos(int pos)
	{
		return pos >= PLAYERBOT_SIDEKICK_EQ_WEAR_BASE && pos < PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + WEAR_MAX_NUM;
	}

	// MT2009_PLUS_SIDEKICK_PANELS_V1: a worn Dragon Stone's place, either deck.
	bool IsPlayerBotSidekickEqDsWearPos(int pos)
	{
		const int first = PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + WEAR_MAX_NUM;
		return pos >= first && pos < first + (int)DS_SLOT_MAX * (int)DRAGON_SOUL_DECK_MAX_NUM;
	}

	// A cell of its Dragon Soul inventory.
	bool IsPlayerBotSidekickEqDsCellPos(int pos)
	{
		return pos >= PLAYERBOT_SIDEKICK_EQ_DS_BASE && pos < PLAYERBOT_SIDEKICK_EQ_DS_BASE + DRAGON_SOUL_INVENTORY_MAX_NUM;
	}

	bool IsPlayerBotSidekickEqDsPos(int pos)
	{
		return IsPlayerBotSidekickEqDsWearPos(pos) || IsPlayerBotSidekickEqDsCellPos(pos);
	}

	// What stands at one of those places of the companion's, or NULL.
	LPITEM GetPlayerBotSidekickDsAt(LPCHARACTER sk, int pos)
	{
		if (IsPlayerBotSidekickEqDsWearPos(pos))
			return sk->GetWear((BYTE)(pos - PLAYERBOT_SIDEKICK_EQ_WEAR_BASE));
		if (!IsPlayerBotSidekickEqDsCellPos(pos))
			return NULL;
		const WORD cell = (WORD)(pos - PLAYERBOT_SIDEKICK_EQ_DS_BASE);
		LPITEM item = sk->GetItem(TItemPos(DRAGON_SOUL_INVENTORY, cell));
		return item && item->GetWindow() == DRAGON_SOUL_INVENTORY && item->GetCell() == cell ? item : NULL;
	}

	// A position as the window writes it; INT_MIN for nothing a window writes.
	int ParsePlayerBotSidekickEqPos(const char* text)
	{
		if (!text || !*text)
			return INT_MIN;
		for (const char* p = text + (*text == '-' ? 1 : 0); *p; ++p)
			if (!isdigit((unsigned char)*p))
				return INT_MIN;
		int pos = INT_MIN;
		str_to_number(pos, text);
		if (pos == -1 || IsPlayerBotSidekickEqBagPos(pos) || IsPlayerBotSidekickEqWearPos(pos))
			return pos;
		// MT2009_PLUS_SIDEKICK_PANELS_V1: the Alchemy window's places.
		if (pos == PLAYERBOT_SIDEKICK_EQ_DS_AUTO || IsPlayerBotSidekickEqDsPos(pos))
			return pos;
		return INT_MIN;
	}

	void SetPlayerBotSidekickPin(DWORD sidekickPid, TPlayerBotSidekickRuntime& rt, DWORD itemId, BYTE wear)
	{
		// One piece a slot: a new one there frees the one pinned before.
		if (wear != PLAYERBOT_SIDEKICK_PIN_UNWANTED)
			for (std::map<DWORD, BYTE>::iterator it = rt.mapPins.begin(); it != rt.mapPins.end();)
			{
				if (it->first != itemId && it->second == wear)
				{
					if (s_bPlayerBotSidekickPinTable)
						DBManager::instance().Query("DELETE FROM player.playerbot_sidekick_pin WHERE item_id=%u", it->first);
					rt.mapPins.erase(it++);
				}
				else
					++it;
			}
		rt.mapPins[itemId] = wear;
		if (s_bPlayerBotSidekickPinTable)
			DBManager::instance().Query("REPLACE INTO player.playerbot_sidekick_pin (item_id, sidekick_pid, wear, pinned_at) "
					"VALUES (%u, %u, %u, NOW())", itemId, sidekickPid, (unsigned int)wear);
	}

	void ClearPlayerBotSidekickPin(TPlayerBotSidekickRuntime& rt, DWORD itemId)
	{
		if (rt.mapPins.erase(itemId) && s_bPlayerBotSidekickPinTable)
			DBManager::instance().Query("DELETE FROM player.playerbot_sidekick_pin WHERE item_id=%u", itemId);
	}

	void AddPlayerBotSidekickGift(DWORD sidekickPid, TPlayerBotSidekickRuntime& rt, DWORD itemId)
	{
		if (rt.setGifts.insert(itemId).second)
			DBManager::instance().Query("INSERT IGNORE INTO player.playerbot_sidekick_gift (item_id, sidekick_pid, given_at) "
					"VALUES (%u, %u, NOW())", itemId, sidekickPid);
	}

	// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: an owner's drop it picked up for
	// the owner, a gift that the AI leaves alone altogether.
	void AddPlayerBotSidekickHeld(DWORD sidekickPid, TPlayerBotSidekickRuntime& rt, DWORD itemId)
	{
		rt.setGifts.insert(itemId);
		rt.setHeld.insert(itemId);
		if (s_bPlayerBotSidekickHeldColumn)
			DBManager::instance().Query("INSERT INTO player.playerbot_sidekick_gift (item_id, sidekick_pid, given_at, held) "
					"VALUES (%u, %u, NOW(), 1) ON DUPLICATE KEY UPDATE sidekick_pid=%u, held=1", itemId, sidekickPid,
					sidekickPid);
		else
			DBManager::instance().Query("INSERT IGNORE INTO player.playerbot_sidekick_gift (item_id, sidekick_pid, given_at) "
					"VALUES (%u, %u, NOW())", itemId, sidekickPid);
	}

	void ClearPlayerBotSidekickGift(DWORD sidekickPid, TPlayerBotSidekickRuntime& rt, DWORD itemId)
	{
		rt.setHeld.erase(itemId);	// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1
		if (rt.setGifts.erase(itemId))
			DBManager::instance().Query("DELETE FROM player.playerbot_sidekick_gift WHERE item_id=%u AND sidekick_pid=%u",
					itemId, sidekickPid);
	}

	// The window's marks on a piece: 1 put on by the owner, 2 the owner's gift,
	// 4 taken off by the owner, 8 the owner's drop it holds for the owner
	// (MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1).
	int GetPlayerBotSidekickEqFlags(const TPlayerBotSidekickRuntime& rt, LPITEM item)
	{
		int flags = 0;
		std::map<DWORD, BYTE>::const_iterator pin = rt.mapPins.find(item->GetID());
		if (pin != rt.mapPins.end())
			flags |= pin->second == PLAYERBOT_SIDEKICK_PIN_UNWANTED ? 4 : 1;
		if (rt.setGifts.find(item->GetID()) != rt.setGifts.end())
			flags |= 2;
		if (rt.setHeld.find(item->GetID()) != rt.setHeld.end())
			flags |= 8;
		return flags;
	}

	DWORD HashPlayerBotSidekickEqItem(LPITEM item, int flags)
	{
		DWORD hash = 2166136261U;
		const DWORD parts[4] = { item->GetID(), item->GetVnum(), (DWORD)item->GetCount(), (DWORD)flags };
		for (int i = 0; i < 4; ++i)
			hash = (hash ^ parts[i]) * 16777619U;
		for (int i = 0; i < 3 && i < ITEM_SOCKET_MAX_NUM; ++i)
			hash = (hash ^ (DWORD)item->GetSocket(i)) * 16777619U;
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			hash = (hash ^ (DWORD)item->GetAttributeType(i)) * 16777619U;
			hash = (hash ^ (DWORD)item->GetAttributeValue(i)) * 16777619U;
		}
		return hash == 0 ? 1U : hash;
	}

	void SendPlayerBotSidekickEqItem(LPCHARACTER owner, DWORD gen, int pos, LPITEM item, int flags)
	{
		char attrs[192] = "-";
		bool any = false;
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			if (item->GetAttributeType(i) != 0)
				any = true;
		if (any)
		{
			size_t n = 0;
			for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM && n + 24 < sizeof(attrs); ++i)
				n += snprintf(attrs + n, sizeof(attrs) - n, "%s%u:%d", i ? "," : "",
						(unsigned int)item->GetAttributeType(i), (int)item->GetAttributeValue(i));
		}
		long sockets[3] = { 0, 0, 0 };
		for (int i = 0; i < 3 && i < ITEM_SOCKET_MAX_NUM; ++i)
			sockets[i] = (long)item->GetSocket(i);
		SendPlayerBotSidekickCommand(owner, "SidekickEqItem %u %d %u %u %d %ld %ld %ld %s", gen, pos, item->GetVnum(),
				(unsigned int)item->GetCount(), flags, sockets[0], sockets[1], sockets[2], attrs);
	}

	struct TPlayerBotSidekickEqEntry
	{
		LPITEM item;
		int flags;
		DWORD hash;
	};

	// The bag and the worn gear as the window is sent them: the whole picture
	// when it opens (or after the companion came back into the world), and
	// after that only what changed, nothing at all when nothing did. What the
	// window was sent is remembered by position (mapEqSent).
	void SendPlayerBotSidekickEq(LPCHARACTER owner, bool full)
	{
		if (!owner || !owner->GetDesc() || (owner->GetDesc()->IsBot() && !s_bPlayerBotSidekickSelfTest))
			return;
		if (IsPlayerBotSidekickSwitchedOff())
		{
			SendPlayerBotSidekickCommand(owner, "SidekickEqNone %d 2", PLAYERBOT_SIDEKICK_EQ_PROTOCOL);
			return;
		}
		TPlayerBotSidekickMap::iterator it = s_mapPlayerBotSidekicks.find(owner->GetPlayerID());
		if (it == s_mapPlayerBotSidekicks.end())
		{
			SendPlayerBotSidekickCommand(owner, "SidekickEqNone %d 0", PLAYERBOT_SIDEKICK_EQ_PROTOCOL);
			return;
		}
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(it->second.dwSidekickPID);
		if (!sk || !sk->IsItemLoaded() || !CPlayerBotManager::instance().IsManaged(it->second.dwSidekickPID))
		{
			SendPlayerBotSidekickCommand(owner, "SidekickEqNone %d 1", PLAYERBOT_SIDEKICK_EQ_PROTOCOL);
			return;
		}
		TPlayerBotSidekickRuntime& rt = s_mapPlayerBotSidekickRuntime[sk->GetPlayerID()];
		std::map<int, TPlayerBotSidekickEqEntry> now;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = sk->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell)
				continue;
			TPlayerBotSidekickEqEntry& e = now[(int)cell];
			e.item = item;
			e.flags = GetPlayerBotSidekickEqFlags(rt, item);
			e.hash = HashPlayerBotSidekickEqItem(item, e.flags);
		}
		for (int wear = 0; wear < WEAR_MAX_NUM; ++wear)
		{
			LPITEM item = sk->GetWear(wear);
			if (!item)
				continue;
			TPlayerBotSidekickEqEntry& e = now[PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + wear];
			e.item = item;
			e.flags = GetPlayerBotSidekickEqFlags(rt, item);
			e.hash = HashPlayerBotSidekickEqItem(item, e.flags);
		}
		// MT2009_PLUS_SIDEKICK_PANELS_V1: its Dragon Stones, worn and in its
		// Alchemy, for the Alchemy window - while the Alchemy is in the game.
		const bool alchemy = !ArePlayerBotAlchemyOff();
		if (alchemy)
		{
			for (int i = 0; i < (int)DS_SLOT_MAX * (int)DRAGON_SOUL_DECK_MAX_NUM; ++i)
			{
				LPITEM item = sk->GetWear((BYTE)(WEAR_MAX_NUM + i));
				if (!item)
					continue;
				TPlayerBotSidekickEqEntry& e = now[PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + WEAR_MAX_NUM + i];
				e.item = item;
				e.flags = GetPlayerBotSidekickEqFlags(rt, item);
				e.hash = HashPlayerBotSidekickEqItem(item, e.flags);
			}
			for (int cell = 0; cell < DRAGON_SOUL_INVENTORY_MAX_NUM; ++cell)
			{
				LPITEM item = sk->GetItem(TItemPos(DRAGON_SOUL_INVENTORY, (WORD)cell));
				if (!item || item->GetWindow() != DRAGON_SOUL_INVENTORY || item->GetCell() != cell)
					continue;
				TPlayerBotSidekickEqEntry& e = now[PLAYERBOT_SIDEKICK_EQ_DS_BASE + cell];
				e.item = item;
				e.flags = GetPlayerBotSidekickEqFlags(rt, item);
				e.hash = HashPlayerBotSidekickEqItem(item, e.flags);
			}
		}
		const long long gold = (long long)sk->GetGold();
		const bool begin = full || rt.dwEqGen == 0;
		if (!begin)
		{
			bool changed = gold != rt.llEqGoldSent || now.size() != rt.mapEqSent.size();
			for (std::map<int, TPlayerBotSidekickEqEntry>::const_iterator e = now.begin(); !changed && e != now.end(); ++e)
			{
				std::map<int, DWORD>::const_iterator sent = rt.mapEqSent.find(e->first);
				changed = sent == rt.mapEqSent.end() || sent->second != e->second.hash;
			}
			if (!changed)
				return;
		}
		++rt.dwEqGen;
		if (begin)
		{
			rt.mapEqSent.clear();
			// MT2009_PLUS_SIDEKICK_PANELS_V1: and the cells of its Alchemy and
			// the stones of one deck (0 0 with the Alchemy out of the game); a
			// window that does not know them reads the first four words.
			SendPlayerBotSidekickCommand(owner, "SidekickEqBegin %d %u %d %d %d %d", PLAYERBOT_SIDEKICK_EQ_PROTOCOL,
					rt.dwEqGen, PLAYERBOT_BAG_CELLS, PLAYERBOT_SIDEKICK_EQ_PAGE_CELLS,
					alchemy ? (int)DRAGON_SOUL_INVENTORY_MAX_NUM : 0, alchemy ? (int)DS_SLOT_MAX : 0);
		}
		size_t lines = 0;
		for (std::map<int, DWORD>::iterator sent = rt.mapEqSent.begin(); sent != rt.mapEqSent.end();)
		{
			if (now.find(sent->first) != now.end())
			{
				++sent;
				continue;
			}
			if (lines >= PLAYERBOT_SIDEKICK_EQ_MAX_LINES)
				break;
			SendPlayerBotSidekickCommand(owner, "SidekickEqEmpty %u %d", rt.dwEqGen, sent->first);
			++lines;
			rt.mapEqSent.erase(sent++);
		}
		for (std::map<int, TPlayerBotSidekickEqEntry>::const_iterator e = now.begin(); e != now.end(); ++e)
		{
			std::map<int, DWORD>::iterator sent = rt.mapEqSent.find(e->first);
			if (sent != rt.mapEqSent.end() && sent->second == e->second.hash)
				continue;
			// What does not fit this answer goes with the next, which the window
			// asks for in a second and a half.
			if (lines >= PLAYERBOT_SIDEKICK_EQ_MAX_LINES)
				break;
			SendPlayerBotSidekickEqItem(owner, rt.dwEqGen, e->first, e->second.item, e->second.flags);
			++lines;
			rt.mapEqSent[e->first] = e->second.hash;
		}
		SendPlayerBotSidekickCommand(owner, "SidekickEqEnd %u %lld", rt.dwEqGen, gold);
		rt.llEqGoldSent = gold;
	}

	void AnswerPlayerBotSidekickEq(LPCHARACTER owner, int code, const char* text)
	{
		SendPlayerBotSidekickCommand(owner, "SidekickEqResult %d %s", code,
				EncodePlayerBotSidekickText(text, 150).c_str());
	}

	// Why the companion cannot wear a piece at all - its type, a class, a sex,
	// a level or a stat the item asks for, a unique of a group it already
	// wears - in the owner's words; empty when it can, whatever the moment
	// says (a blow a second ago). The engine says it to the companion's chat,
	// which nobody reads. replacing: the piece a unique would take the place
	// of, which is no obstacle.
	std::string GetPlayerBotSidekickWearRefusal(LPCHARACTER sk, LPITEM item, LPITEM replacing)
	{
		if (!item->IsEquipable() || item->IsDragonSoul() || item->FindEquipCell(sk) < 0)
			return "Tego nie da sie zalozyc.";
		// Every costume goes on: the engine takes them on this server now
		// (char_item.cpp), and "Kostiumy sa na tym serwerze wylaczone" was a
		// refusal left from when it did not (26 September 2026).
		if (!item->CanUsedBy(sk))
			return "To nie jest dla klasy towarzysza.";
		if ((IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_MALE) && GET_SEX(sk) == SEX_MALE) ||
				(IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_FEMALE) && GET_SEX(sk) == SEX_FEMALE))
			return "To nie jest dla plci towarzysza.";
		char text[128];
		const TItemTable* proto = item->GetProto();
		for (int i = 0; proto && i < ITEM_LIMIT_MAX_NUM; ++i)
		{
			const long limit = proto->aLimits[i].lValue;
			const char* what = NULL;
			switch (proto->aLimits[i].bType)
			{
				case LIMIT_LEVEL:
					if (sk->GetLevel() < limit)
					{
						snprintf(text, sizeof(text), "Towarzysz ma za niski poziom (trzeba %ld, ma %d).", limit,
								sk->GetLevel());
						return text;
					}
					break;
				case LIMIT_STR: if (sk->GetPoint(POINT_ST) < limit) what = "sily"; break;
				case LIMIT_INT: if (sk->GetPoint(POINT_IQ) < limit) what = "inteligencji"; break;
				case LIMIT_DEX: if (sk->GetPoint(POINT_DX) < limit) what = "zrecznosci"; break;
				case LIMIT_CON: if (sk->GetPoint(POINT_HT) < limit) what = "witalnosci"; break;
			}
			if (what)
			{
				snprintf(text, sizeof(text), "Towarzysz ma za malo %s (trzeba %ld).", what, limit);
				return text;
			}
		}
		if (item->GetWearFlag() & WEARABLE_UNIQUE)
			for (int wear = WEAR_UNIQUE1; wear <= WEAR_UNIQUE2; ++wear)
			{
				LPITEM worn = sk->GetWear(wear);
				if (worn && worn != item && worn != replacing && worn->IsSameSpecialGroup(item))
					return "Towarzysz nosi juz cos z tej samej grupy.";
			}
		return std::string();
	}

	// MT2009_PLUS_SIDEKICK_HAIR_V1: the companion's hairstyle (upstream
	// 2.2.44, busz30_04484). A hairstyle comes off with a Bleach (Wybielacz,
	// 70201) or a hair dye (70202-70206) - the engine's own tools for it
	// (UseItem_Common's NEW_HAIR_STYLE_ADD, which takes the costume off with
	// the Bleach as its key). The owner lays one in the companion's bag and
	// right-clicks it there (or drops it on the hair slot): the hairstyle goes
	// into the bag, the tool is used up, and another hairstyle can be put on.
	// With no hairstyle worn the tool does to the companion's own hair what it
	// does to a player's. A hairstyle the engine would not let go said "sprobuj
	// za chwile", which no wait changed; it says what the tool is now.
	const char* const PLAYERBOT_SIDEKICK_NEEDS_BLEACH_TEXT =
			"Fryzure zdejmuje Wybielacz (albo farba do wlosow): poloz go w torbie towarzysza i kliknij prawym przyciskiem.";

	bool IsPlayerBotSidekickHairTool(LPITEM item)
	{
		// The engine's range (PLAYERBOT_HAIR_DYE_FIRST_VNUM is the Bleach).
		return item && IsPlayerBotFishedHairDye(item->GetVnum());
	}

	bool IsPlayerBotSidekickHairstyle(LPITEM item)
	{
		return item && item->GetType() == ITEM_COSTUME && item->GetSubType() == COSTUME_HAIR;
	}

	// The Bleach or a dye in the companion's bag, used for its owner.
	int UsePlayerBotSidekickHairTool(LPCHARACTER owner, LPCHARACTER sk, TPlayerBotSidekickRuntime& rt, LPITEM tool,
			std::string& answer)
	{
		if (tool->isLocked() || tool->IsExchanging())
		{
			answer = "Ten przedmiot jest teraz zajety.";
			return 2;
		}
		const DWORD vnum = tool->GetVnum();
		const bool bleach = vnum == PLAYERBOT_HAIR_DYE_FIRST_VNUM;
		LPITEM hair = sk->GetWear(WEAR_COSTUME_HAIR);
		if (hair)
		{
			if (IS_SET(hair->GetFlag(), ITEM_FLAG_IRREMOVABLE))
			{
				answer = "Tej fryzury nie da sie zdjac.";
				return 2;
			}
			if (sk->GetEmptyInventory(hair->GetSize()) < 0)
			{
				answer = "Towarzysz nie ma miejsca w torbie na fryzure - wez najpierw cos z jego torby.";
				return 2;
			}
			// The engine's own unequip, the tool as its key. What refuses it now
			// is a stun, a rod or a pickaxe at work (CanUnequipNow), which pass.
			if (!sk->UnequipItem(hair, tool) || hair->IsEquipped())
			{
				answer = "Towarzysz jest teraz zajety (ogluszony, lowi albo kopie) - kliknij Wybielacz jeszcze raz.";
				return 2;
			}
			const DWORD hairVnum = hair->GetVnum();
			std::string hairName = hair->GetName() ? hair->GetName() : "";
			// Off by its owner's hand: the AI does not put it back on.
			SetPlayerBotSidekickPin(sk->GetPlayerID(), rt, hair->GetID(), PLAYERBOT_SIDEKICK_PIN_UNWANTED);
			LogManager::instance().ItemLog(sk, hair, "PLAYERBOT_SIDEKICK_HAIR_OFF", owner->GetName());
			FlushPlayerBotItemRow(hair);
			LogManager::instance().ItemLog(sk, tool, "PLAYERBOT_SIDEKICK_HAIR_TOOL", owner->GetName());
			tool->SetCount(tool->GetCount() - 1);
			sys_log(0, "PLAYERBOT_SIDEKICK: hairstyle off pid=%u name=%s owner=%u hair=%u tool=%u", sk->GetPlayerID(),
					sk->GetName(), owner->GetPlayerID(), hairVnum, vnum);
			answer = std::string("Fryzura zdjeta: ") + hairName + " jest w torbie towarzysza. Mozesz zalozyc inna.";
			return 0;
		}
		// No hairstyle: the companion's own hair, by the engine's rule for a
		// player's - a colour once in three levels, the Bleach at any time.
		const int lastDyeLevel = sk->GetQuestFlag("dyeing_hair.last_dye_level");
		if (!bleach && lastDyeLevel != 0 && lastDyeLevel + 3 > sk->GetLevel())
		{
			char text[128];
			snprintf(text, sizeof(text), "Towarzysz moze znowu farbowac wlosy od %d poziomu.", lastDyeLevel + 3);
			answer = text;
			return 2;
		}
		if (bleach && sk->GetPart(PART_HAIR) == 0)
		{
			answer = "Towarzysz nie ma fryzury ani farbowanych wlosow.";
			return 2;
		}
		sk->SetPart(PART_HAIR, vnum - PLAYERBOT_HAIR_DYE_FIRST_VNUM);
		sk->SetQuestFlag("dyeing_hair.last_dye_level", bleach ? 0 : sk->GetLevel());
		LogManager::instance().ItemLog(sk, tool, "PLAYERBOT_SIDEKICK_HAIR_TOOL", owner->GetName());
		tool->SetCount(tool->GetCount() - 1);
		sk->UpdatePacket();
		sys_log(0, "PLAYERBOT_SIDEKICK: hair %s pid=%u name=%s owner=%u tool=%u part=%d", bleach ? "bleached" : "dyed",
				sk->GetPlayerID(), sk->GetName(), owner->GetPlayerID(), vnum, sk->GetPart(PART_HAIR));
		answer = bleach ? "Wlosy towarzysza wrocily do naturalnego koloru." : "Wlosy towarzysza ufarbowane.";
		return 0;
	}

	// Puts a piece of the companion's bag on for its owner, and pins it there.
	// A piece the engine will not let on for the moment - a blow or a skill in
	// the last second and a half, which it asks of every equip - is pinned all
	// the same and goes on at the equipment pass's first chance, the fight held
	// off for it (dwEquipWaitUntil).
	int EquipPlayerBotSidekickForOwner(LPCHARACTER owner, LPCHARACTER sk, TPlayerBotAIState& state,
			TPlayerBotSidekickRuntime& rt, LPITEM item, int wantWear, std::string& answer)
	{
		if (item->IsEquipped())
		{
			answer = "Towarzysz juz to nosi.";
			return 0;
		}
		const int wear = item->FindEquipCell(sk);
		const bool unique = wear == WEAR_UNIQUE1 || wear == WEAR_UNIQUE2;
		const bool uniqueSlot = wantWear == WEAR_UNIQUE1 || wantWear == WEAR_UNIQUE2;
		if (wantWear >= 0 && wear >= 0 && wear != wantWear && !(unique && uniqueSlot))
		{
			answer = "To nie pasuje w to miejsce.";
			return 2;
		}
		const int slot = unique && uniqueSlot ? wantWear : wear;
		LPITEM old = slot >= 0 ? sk->GetWear(slot) : NULL;
		const std::string refusal = GetPlayerBotSidekickWearRefusal(sk, item, old);
		if (!refusal.empty())
		{
			answer = refusal;
			return 2;
		}
		if (old && IS_SET(old->GetFlag(), ITEM_FLAG_IRREMOVABLE))
		{
			answer = "Tego, co tam nosi, nie da sie zdjac.";
			return 2;
		}
		// MT2009_PLUS_SIDEKICK_HAIR_V1: the worn piece's mark, which the pin below
		// takes off it, for a hairstyle that stays on after all.
		const int oldPin = GetPlayerBotSidekickPinOf(sk, old);
		SetPlayerBotSidekickPin(sk->GetPlayerID(), rt, item->GetID(), (BYTE)slot);
		const DWORD now = get_dword_time();
		const bool blowFresh = IsPlayerBotEquipWindowShut(sk, state);
		bool worn = false;
		if (!blowFresh && !item->isLocked() && !item->IsExchanging())
		{
			// A ring or a glove goes where the owner put it: the engine takes the
			// first free unique slot whatever it is asked (CItem::FindEquipCell),
			// so the slot asked for is emptied first.
			if (unique && old && old->IsEquipped() && sk->GetEmptyInventory(old->GetSize()) >= 0)
				sk->UnequipItem(old);
			// Anything else: the engine swaps a worn piece into the new one's
			// cell when it fits there; one that does not goes to a free cell.
			worn = PlayerBotEquipItem(sk, item) && item->IsEquipped();
			if (!worn && old && old->IsEquipped() && sk->GetEmptyInventory(old->GetSize()) >= 0 &&
					sk->UnequipItem(old) && !old->IsEquipped())
				worn = PlayerBotEquipItem(sk, item) && item->IsEquipped();
		}
		if (worn)
		{
			// Pinned where it went: a unique finds its own slot.
			const int wentTo = (int)item->GetCell() - INVENTORY_MAX_NUM;
			if (wentTo >= 0 && wentTo < WEAR_MAX_NUM && wentTo != slot)
				SetPlayerBotSidekickPin(sk->GetPlayerID(), rt, item->GetID(), (BYTE)wentTo);
			LogManager::instance().ItemLog(sk, item, "PLAYERBOT_SIDEKICK_WEAR", owner->GetName());
			FlushPlayerBotItemRow(item);
			FlushPlayerBotItemRow(old);
			answer = "Zalozone. Tego towarzysz sam nie zdejmie.";
			return 0;
		}
		rt.dwEquipWaitUntil = now + PLAYERBOT_SIDEKICK_EQUIP_WAIT_MS;
		state.bEquipPending = true;
		state.dwNextEquipmentCheckTime = 0;
		// The reason the owner can act on: a blow still landing, or the bag
		// with no cell for what would come off - the equipment pass tries the
		// engine's swap in place, which needs none, and keeps trying.
		if (blowFresh)
			answer = "Zalozy to, jak tylko skonczy cios.";
		else if (old && sk->GetEmptyInventory(old->GetSize()) < 0)
			answer = "Nie mam miejsca w plecaku na to, co zdejme. Zaloze, gdy tylko sie zwolni.";
		else if (IsPlayerBotSidekickHairstyle(old) && IsPlayerBotSidekickHairstyle(item))
		{
			// MT2009_PLUS_SIDEKICK_HAIR_V1: the hairstyle worn stays on until a
			// Bleach takes it off - no wait mends that, so nothing waits.
			ClearPlayerBotSidekickPin(rt, item->GetID());
			if (oldPin >= 0)
				SetPlayerBotSidekickPin(sk->GetPlayerID(), rt, old->GetID(), (BYTE)oldPin);
			rt.dwEquipWaitUntil = 0;
			state.bEquipPending = false;
			answer = PLAYERBOT_SIDEKICK_NEEDS_BLEACH_TEXT;
			return 2;
		}
		else
			answer = "Nie moge tego teraz zalozyc - sprobuje za chwile.";
		sys_log(0, "PLAYERBOT_SIDEKICK: equip waits pid=%u name=%s vnum=%u slot=%d blow=%d bag_room=%d",
				sk->GetPlayerID(), sk->GetName(), item->GetVnum(), slot, blowFresh ? 1 : 0,
				old && sk->GetEmptyInventory(old->GetSize()) < 0 ? 0 : 1);
		return 0;
	}

	// Takes a worn piece off for its owner, to a cell or to the first that
	// fits, and marks it: the AI does not put it back on.
	// MT2009_PLUS_SIDEKICK_MOUNT_FIX_V1: a mount seal its owner takes off a
	// companion that rides it - the companion is set down first, as Ctrl+G
	// sets a player down (the unequip then sends the mount away).
	void StopPlayerBotSidekickMountFor(LPCHARACTER sk, LPITEM worn)
	{
#if defined(ENABLE_MOUNT_COSTUME_SYSTEM)
		if (!sk || !worn || !worn->IsNewMountItem() || !sk->IsRiding())
			return;
		sk->Stop();
		StopPlayerBotRiding(sk);
		sys_log(0, "PLAYERBOT_SIDEKICK: off the mount for its owner pid=%u name=%s seal=%u riding=%d", sk->GetPlayerID(),
				sk->GetName(), worn->GetVnum(), sk->IsRiding() ? 1 : 0);
#endif
	}

	int UnequipPlayerBotSidekickForOwner(LPCHARACTER owner, LPCHARACTER sk, TPlayerBotSidekickRuntime& rt, int wear,
			int toCell, std::string& answer)
	{
		LPITEM worn = sk->GetWear(wear);
		if (!worn)
		{
			answer = "Tam nic nie ma.";
			return 3;
		}
		if (IS_SET(worn->GetFlag(), ITEM_FLAG_IRREMOVABLE) || worn->isLocked())
		{
			answer = "Tego nie da sie zdjac.";
			return 2;
		}
		// MT2009_PLUS_SIDEKICK_MOUNT_FIX_V1: down from the seal's mount first,
		// the way Ctrl+G gets a player off before the seal comes off.
		StopPlayerBotSidekickMountFor(sk, worn);
		bool done = false;
		if (toCell >= 0)
			done = sk->MoveItem(TItemPos(INVENTORY, INVENTORY_MAX_NUM + wear), TItemPos(INVENTORY, toCell),
					worn->GetCount()) && !worn->IsEquipped();
		else
			done = sk->GetEmptyInventory(worn->GetSize()) >= 0 && sk->UnequipItem(worn) && !worn->IsEquipped();
		if (!done)
		{
			// MT2009_PLUS_SIDEKICK_HAIR_V1: a hairstyle comes off with the Bleach.
			answer = sk->GetEmptyInventory(worn->GetSize()) < 0 ? "Towarzysz nie ma miejsca w torbie." :
					IsPlayerBotSidekickHairstyle(worn) ? PLAYERBOT_SIDEKICK_NEEDS_BLEACH_TEXT :
					"Nie da sie tego teraz zdjac - sprobuj za chwile.";
			return 2;
		}
		SetPlayerBotSidekickPin(sk->GetPlayerID(), rt, worn->GetID(), PLAYERBOT_SIDEKICK_PIN_UNWANTED);
		LogManager::instance().ItemLog(sk, worn, "PLAYERBOT_SIDEKICK_UNWEAR", owner->GetName());
		FlushPlayerBotItemRow(worn);
		answer = "Zdjete. Towarzysz sam tego nie zalozy (odepnij, zeby znow mogl).";
		return 0;
	}

	// A move inside its bag, as the owner's own bag moves: onto an empty place,
	// into a stack of the same thing, or - which the engine's own move does
	// not do - swapped with a piece of the same size lying there.
	int MovePlayerBotSidekickBagItem(LPCHARACTER sk, int from, int to, std::string& answer)
	{
		LPITEM item = sk->GetInventoryItem(from);
		if (!item || item->GetCell() != from)
		{
			answer = "Tam nic nie ma.";
			return 3;
		}
		if (from == to)
			return 0;
		if (item->isLocked() || item->IsExchanging())
		{
			answer = "Ten przedmiot jest teraz zajety.";
			return 2;
		}
		LPITEM other = sk->GetInventoryItem(to);
		const bool sameStack = other && other != item && other->GetVnum() == item->GetVnum() && item->IsStackable();
		if (other && other != item && other->GetCell() == to && !sameStack && other->GetSize() == item->GetSize())
		{
			if (other->isLocked() || other->IsExchanging())
			{
				answer = "Ten przedmiot jest teraz zajety.";
				return 2;
			}
			item->RemoveFromCharacter();
			other->RemoveFromCharacter();
			item->AddToCharacter(sk, TItemPos(INVENTORY, to));
			other->AddToCharacter(sk, TItemPos(INVENTORY, from));
			answer = "Zamienione miejscami.";
			return 0;
		}
		const DWORD before = sameStack ? (DWORD)other->GetCount() : 0;
		if (!sk->MoveItem(TItemPos(INVENTORY, from), TItemPos(INVENTORY, to), item->GetCount()))
		{
			answer = "Tam nie ma miejsca.";
			return 2;
		}
		if (sameStack && (DWORD)other->GetCount() == before)
		{
			answer = "Ten stos jest juz pelny.";
			return 2;
		}
		answer = sameStack ? "Polaczone." : "Przeniesione.";
		return 0;
	}

	// A stack dropped on a stack of the same thing, poured the way the
	// engine's own move pours one: the same vnum and sockets, up to the item's
	// own ceiling. The units moved; -1 when the two are not one thing. A
	// source emptied is removed, with its quickslot.
	int PourPlayerBotSidekickStack(LPCHARACTER from, LPITEM item, LPITEM into, const char* why)
	{
		if (!into || into == item || into->GetVnum() != item->GetVnum() || !into->IsStackable() ||
				IS_SET(into->GetAntiFlag(), ITEM_ANTIFLAG_STACK) || into->isLocked() || into->IsExchanging())
			return -1;
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			if (into->GetSocket(i) != item->GetSocket(i))
				return -1;
		const int room = PlayerBotMaxStack(into) - (int)into->GetCount();
		const int moved = std::min(room, (int)item->GetCount());
		if (moved <= 0)
			return 0;
		into->SetCount(into->GetCount() + moved);
		if (moved >= (int)item->GetCount())
		{
			from->SyncQuickslot(QUICKSLOT_TYPE_ITEM, item->GetCell(), 255);
			ITEM_MANAGER::instance().RemoveItem(item, why);
		}
		else
			item->SetCount(item->GetCount() - moved);
		return moved;
	}

	// Why a piece may not cross between the two bags, in the owner's words;
	// empty when it may. Not ITEM_ANTIFLAG_GIVE: that flag keeps a piece from
	// changing hands between two players, and the companion's bag is its
	// owner's. The Moonlight chest's add and change stones (71084, 71085) carry
	// it, and a companion that opened the chests kept them where its owner
	// could not reach them (xxkld., 27 September; the operator: no block
	// between a player and the companion). A trade still refuses them - the
	// window is the way.
	std::string GetPlayerBotSidekickHandOverRefusal(LPITEM item)
	{
		if (item->isLocked() || item->IsExchanging())
			return "Ten przedmiot jest teraz zajety.";
		if (item->IsDragonSoul())
			return "Kamieni smoka nie da sie tu przekazac.";
		return std::string();
	}

	// Moves a piece from one character's bag to a cell of the other's, as
	// CExchange::Done does.
	void HandPlayerBotSidekickItemOver(LPCHARACTER from, LPCHARACTER to, LPITEM item, int cell)
	{
		from->SyncQuickslot(QUICKSLOT_TYPE_ITEM, item->GetCell(), 255);
		item->RemoveFromCharacter();
		item->AddToCharacter(to, TItemPos(INVENTORY, cell));
		ITEM_MANAGER::instance().FlushDelayedSave(item);
	}

	// Where a piece of this size goes in a character's bag: the cell asked
	// for when it is free, the first free one for -1. -1 when there is none.
	int FindPlayerBotSidekickHandOverCell(LPCHARACTER ch, LPITEM item, int cell)
	{
		if (cell < 0)
			return ch->GetEmptyInventory(item->GetSize());
		return ch->IsEmptyItemGrid(TItemPos(INVENTORY, cell), item->GetSize()) ? cell : -1;
	}

	// From the owner's bag to its companion: onto a cell, a stack or a slot.
	// What it gives is its gift (never sold, IsPlayerBotSidekickGift); what it
	// gives straight onto a slot is put on and pinned.
	int GivePlayerBotSidekickItem(LPCHARACTER owner, LPCHARACTER sk, TPlayerBotAIState& state,
			TPlayerBotSidekickRuntime& rt, int fromCell, int to, std::string& answer)
	{
		LPITEM item = IsPlayerBotSidekickEqBagPos(fromCell) ? owner->GetInventoryItem(fromCell) : NULL;
		if (!item || item->GetCell() != fromCell || item->GetWindow() != INVENTORY || item->IsEquipped())
		{
			answer = "Nie ma tego w twojej torbie.";
			return 3;
		}
		answer = GetPlayerBotSidekickHandOverRefusal(item);
		if (!answer.empty())
			return 2;
		const bool toWear = IsPlayerBotSidekickEqWearPos(to);
		const int wantWear = toWear ? to - PLAYERBOT_SIDEKICK_EQ_WEAR_BASE : -1;
		// MT2009_PLUS_SIDEKICK_HAIR_V1: the Bleach or a dye dropped from the
		// owner's bag on the hair slot goes into the companion's bag and is used.
		const bool hairTool = wantWear == WEAR_COSTUME_HAIR && IsPlayerBotSidekickHairTool(item);
		if (toWear && !hairTool)
		{
			// What the companion could never wear stays with the owner.
			const int wear = item->FindEquipCell(sk);
			const bool unique = (wear == WEAR_UNIQUE1 || wear == WEAR_UNIQUE2) &&
					(wantWear == WEAR_UNIQUE1 || wantWear == WEAR_UNIQUE2);
			if (wear >= 0 && wear != wantWear && !unique)
			{
				answer = "To nie pasuje w to miejsce.";
				return 2;
			}
			answer = GetPlayerBotSidekickWearRefusal(sk, item, sk->GetWear(wantWear));
			if (!answer.empty())
				return 2;
		}
		const char* name = item->GetName();
		std::string pieceName = name ? name : "";
		const DWORD vnum = item->GetVnum();
		// Onto a stack of the same thing, as far as it takes.
		LPITEM stack = IsPlayerBotSidekickEqBagPos(to) ? sk->GetInventoryItem(to) : NULL;
		if (stack && stack->GetCell() == to)
		{
			const DWORD count = (DWORD)item->GetCount();
			const int poured = PourPlayerBotSidekickStack(owner, item, stack, "PLAYERBOT_SIDEKICK_GIVE");
			if (poured > 0)
			{
				AddPlayerBotSidekickGift(sk->GetPlayerID(), rt, stack->GetID());
				LogManager::instance().ItemLog(sk, stack, "PLAYERBOT_GIFT_IN", owner->GetName());
				char text[160];
				snprintf(text, sizeof(text), "Dolozone do stosu towarzysza: %d z %u.", poured, (unsigned int)count);
				answer = text;
				return 0;
			}
			if (poured == 0)
			{
				answer = "Ten stos jest juz pelny.";
				return 2;
			}
		}
		const int cell = FindPlayerBotSidekickHandOverCell(sk, item, IsPlayerBotSidekickEqBagPos(to) ? to : -1);
		if (cell < 0)
		{
			answer = IsPlayerBotSidekickEqBagPos(to) ? "Tam nie ma miejsca." : "Towarzysz nie ma miejsca w torbie.";
			return 2;
		}
		HandPlayerBotSidekickItemOver(owner, sk, item, cell);
		AddPlayerBotSidekickGift(sk->GetPlayerID(), rt, item->GetID());
		ClearPlayerBotSidekickPin(rt, item->GetID());
		LogManager::instance().ItemLog(sk, item, "PLAYERBOT_GIFT_IN", owner->GetName());
		sys_log(0, "PLAYERBOT_SIDEKICK: given pid=%u owner=%u item=%u vnum=%u cell=%d", sk->GetPlayerID(),
				owner->GetPlayerID(), item->GetID(), vnum, cell);
		if (hairTool)
		{
			std::string used;
			const int code = UsePlayerBotSidekickHairTool(owner, sk, rt, item, used);
			answer = std::string("Dane: ") + pieceName + ". " + used;
			return code;
		}
		if (toWear)
		{
			std::string worn;
			const int code = EquipPlayerBotSidekickForOwner(owner, sk, state, rt, item, wantWear, worn);
			answer = std::string("Dane: ") + pieceName + ". " + worn;
			return code;
		}
		state.dwNextEquipmentCheckTime = 0;
		answer = std::string("Dane towarzyszowi: ") + pieceName + ".";
		return 0;
	}

	// From the companion to its owner's bag: a bag piece or a worn one (taken
	// off first). The owner's mark and gift go with it.
	int TakePlayerBotSidekickItem(LPCHARACTER owner, LPCHARACTER sk, TPlayerBotSidekickRuntime& rt, int from,
			int toCell, std::string& answer)
	{
		const bool fromWear = IsPlayerBotSidekickEqWearPos(from);
		LPITEM item = fromWear ? sk->GetWear(from - PLAYERBOT_SIDEKICK_EQ_WEAR_BASE)
				: (IsPlayerBotSidekickEqBagPos(from) ? sk->GetInventoryItem(from) : NULL);
		if (!item || (!fromWear && item->GetCell() != from))
		{
			answer = "Tam nic nie ma.";
			return 3;
		}
		answer = GetPlayerBotSidekickHandOverRefusal(item);
		if (!answer.empty())
			return 2;
		if (fromWear && IS_SET(item->GetFlag(), ITEM_FLAG_IRREMOVABLE))
		{
			answer = "Tego nie da sie zdjac.";
			return 2;
		}
		std::string pieceName = item->GetName() ? item->GetName() : "";
		// Onto a stack of the owner's of the same thing, as far as it takes.
		LPITEM stack = IsPlayerBotSidekickEqBagPos(toCell) ? owner->GetInventoryItem(toCell) : NULL;
		if (!fromWear && stack && stack->GetCell() == toCell)
		{
			const DWORD id = item->GetID();
			const DWORD count = (DWORD)item->GetCount();
			const int poured = PourPlayerBotSidekickStack(sk, item, stack, "PLAYERBOT_SIDEKICK_TAKE");
			if (poured > 0)
			{
				if (poured >= (int)count)
				{
					ClearPlayerBotSidekickGift(sk->GetPlayerID(), rt, id);
					ClearPlayerBotSidekickPin(rt, id);
				}
				LogManager::instance().ItemLog(owner, stack, "PLAYERBOT_SIDEKICK_TAKE", sk->GetName());
				char text[160];
				snprintf(text, sizeof(text), "Dolozone do twojego stosu: %d z %u.", poured, (unsigned int)count);
				answer = text;
				return 0;
			}
			if (poured == 0)
			{
				answer = "Twoj stos jest juz pelny.";
				return 2;
			}
		}
		const int cell = FindPlayerBotSidekickHandOverCell(owner, item, IsPlayerBotSidekickEqBagPos(toCell) ? toCell : -1);
		if (cell < 0)
		{
			answer = IsPlayerBotSidekickEqBagPos(toCell) ? "Tam nie ma miejsca." : "Nie masz miejsca w torbie.";
			return 2;
		}
		// A worn piece comes off into the companion's bag first: the engine
		// unequips a piece only there (CItem::RemoveFromCharacter would leave
		// it with two owners).
		if (fromWear)
			StopPlayerBotSidekickMountFor(sk, item);	// MT2009_PLUS_SIDEKICK_MOUNT_FIX_V1
		if (fromWear && (sk->GetEmptyInventory(item->GetSize()) < 0 || !sk->UnequipItem(item) || item->IsEquipped()))
		{
			answer = sk->GetEmptyInventory(item->GetSize()) < 0 ?
					"Towarzysz nie ma miejsca w torbie, zeby to zdjac - wez najpierw cos z jego torby." :
					IsPlayerBotSidekickHairstyle(item) ? PLAYERBOT_SIDEKICK_NEEDS_BLEACH_TEXT : // MT2009_PLUS_SIDEKICK_HAIR_V1
					"Nie da sie tego teraz zdjac - sprobuj za chwile.";
			return 2;
		}
		const DWORD id = item->GetID();
		HandPlayerBotSidekickItemOver(sk, owner, item, cell);
		ClearPlayerBotSidekickGift(sk->GetPlayerID(), rt, id);
		ClearPlayerBotSidekickPin(rt, id);
		LogManager::instance().ItemLog(owner, item, "PLAYERBOT_SIDEKICK_TAKE", sk->GetName());
		sys_log(0, "PLAYERBOT_SIDEKICK: taken pid=%u owner=%u item=%u vnum=%u worn=%d cell=%d", sk->GetPlayerID(),
				owner->GetPlayerID(), id, item->GetVnum(), fromWear ? 1 : 0, cell);
		answer = std::string("Wziete od towarzysza: ") + pieceName + ".";
		return 0;
	}

	// ------------------------------------------- MT2009_PLUS_SIDEKICK_PANELS_V1
	//
	// The companion's Alchemy window (uisidekickinventory.py, AlchemyWindow,
	// opened from the Options page of its window): its two decks and the
	// stones of its Dragon Soul inventory, which its owner gives it from the
	// owner's own Alchemy, puts on, takes off and takes back. The companion
	// never buys, opens, refines or swaps a stone by itself
	// (IsPlayerBotAlchemyUser leaves it out); its deck goes on for its fights
	// (ManagePlayerBotDsDeckTick). A stone comes off a deck the way the
	// owner's own does - DSManager::PullOut, the Alchemy's odds of keeping it,
	// better with a Dragon Soul extractor, which it takes from its own bag or,
	// with none there, from its owner's.

	LPITEM FindPlayerBotSidekickDsExtractor(LPCHARACTER ch)
	{
		for (WORD cell = 0; ch && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetCell() == cell && item->GetType() == ITEM_EXTRACT &&
					item->GetSubType() == EXTRACT_DRAGON_SOUL && !item->isLocked() && !item->IsExchanging())
				return item;
		}
		return NULL;
	}

	// Off a deck into its own Alchemy: 0 off, 2 refused (the answer says why),
	// 4 lost to the Alchemy's odds - item is NULL then.
	int PullOutPlayerBotSidekickDs(LPCHARACTER owner, LPCHARACTER sk, LPITEM& item, std::string& answer)
	{
		if (IS_SET(item->GetFlag(), ITEM_FLAG_IRREMOVABLE) || item->isLocked() || item->IsExchanging())
		{
			answer = "Tego kamienia nie da sie teraz zdjac.";
			return 2;
		}
		if (!sk->DragonSoul_IsQualified())
			sk->DragonSoul_GiveQualification();
		if (sk->GetEmptyDragonSoulInventory(item) < 0)
		{
			answer = "W alchemii towarzysza nie ma miejsca na ten kamien.";
			return 2;
		}
		LPITEM extractor = FindPlayerBotSidekickDsExtractor(sk);
		if (!extractor)
			extractor = FindPlayerBotSidekickDsExtractor(owner);
		const DWORD vnum = item->GetVnum();
		const bool ok = DSManager::instance().PullOut(sk, NPOS, item, extractor);
		if (!item)
		{
			sys_log(0, "PLAYERBOT_SIDEKICK: ds pull-out lost pid=%u owner=%u vnum=%u extractor=%d", sk->GetPlayerID(),
					owner->GetPlayerID(), vnum, extractor ? 1 : 0);
			answer = "Kamien pekl przy zdejmowaniu - szansa alchemii, jak przy twoim (Szczypce Smoka ja podnosza).";
			return 4;
		}
		if (!ok || item->IsEquipped())
		{
			answer = "Nie da sie go teraz zdjac - sprobuj za chwile.";
			return 2;
		}
		return 0;
	}

	// Onto a deck: wantWear the engine's wear index of a deck's place, or -1
	// for the first deck whose place for the kind is free.
	int EquipPlayerBotSidekickDs(LPCHARACTER sk, LPITEM item, int wantWear, std::string& answer)
	{
		if (item->isLocked() || item->IsExchanging())
		{
			answer = "Ten przedmiot jest teraz zajety.";
			return 2;
		}
		if (item->GetSubType() >= DS_SLOT_MAX)
		{
			answer = "Tego kamienia nie da sie zalozyc.";
			return 2;
		}
		int wear = -1;
		if (wantWear >= 0)
		{
			if (item->FindEquipCell(sk, wantWear) != wantWear)
			{
				answer = "Ten kamien nie pasuje w to miejsce.";
				return 2;
			}
			if (sk->GetWear((BYTE)wantWear))
			{
				answer = "Tu juz jest kamien - zdejmij go najpierw.";
				return 2;
			}
			wear = wantWear;
		}
		else
			for (int deck = 0; deck < DRAGON_SOUL_DECK_MAX_NUM && wear < 0; ++deck)
			{
				const int w = WEAR_MAX_NUM + deck * DS_SLOT_MAX + item->GetSubType();
				if (item->FindEquipCell(sk, w) == w && !sk->GetWear((BYTE)w))
					wear = w;
			}
		if (wear < 0)
		{
			answer = "Oba zestawy maja juz kamien tego rodzaju - zdejmij jeden najpierw.";
			return 2;
		}
		if (!sk->DragonSoul_IsQualified())
			sk->DragonSoul_GiveQualification();
		if (!PlayerBotEquipItem(sk, item, wear) || !item->IsEquipped())
		{
			answer = "Nie da sie go teraz zalozyc - sprobuj za chwile.";
			return 2;
		}
		char text[96];
		snprintf(text, sizeof(text), "Zalozony w zestawie %d.", (wear - WEAR_MAX_NUM) / DS_SLOT_MAX + 1);
		answer = text;
		return 0;
	}

	// From the owner's Alchemy (its cell) into the companion's, and onto a
	// deck when it was dropped on one.
	int GivePlayerBotSidekickDs(LPCHARACTER owner, LPCHARACTER sk, TPlayerBotSidekickRuntime& rt, int ownerCell, int to,
			std::string& answer)
	{
		if (ArePlayerBotAlchemyOff())
		{
			answer = "Alchemia jest wylaczona na tym serwerze.";
			return 2;
		}
		LPITEM item = ownerCell >= 0 && ownerCell < DRAGON_SOUL_INVENTORY_MAX_NUM
				? owner->GetItem(TItemPos(DRAGON_SOUL_INVENTORY, (WORD)ownerCell)) : NULL;
		if (!item || item->GetWindow() != DRAGON_SOUL_INVENTORY || item->GetCell() != ownerCell || !item->IsDragonSoul())
		{
			answer = "Nie ma tego w twojej alchemii.";
			return 3;
		}
		if (item->isLocked() || item->IsExchanging())
		{
			answer = "Ten przedmiot jest teraz zajety.";
			return 2;
		}
		const bool toWear = IsPlayerBotSidekickEqDsWearPos(to);
		const int wantWear = toWear ? to - PLAYERBOT_SIDEKICK_EQ_WEAR_BASE : -1;
		if (toWear && item->FindEquipCell(sk, wantWear) != wantWear)
		{
			answer = "Ten kamien nie pasuje w to miejsce.";
			return 2;
		}
		if (toWear && sk->GetWear((BYTE)wantWear))
		{
			answer = "Tu juz jest kamien - zdejmij go najpierw.";
			return 2;
		}
		if (!sk->DragonSoul_IsQualified())
			sk->DragonSoul_GiveQualification();
		const int cell = sk->GetEmptyDragonSoulInventory(item);
		if (cell < 0)
		{
			answer = "W alchemii towarzysza nie ma miejsca na ten kamien.";
			return 2;
		}
		const std::string pieceName = item->GetName() ? item->GetName() : "";
		item->RemoveFromCharacter();
		item->AddToCharacter(sk, TItemPos(DRAGON_SOUL_INVENTORY, (WORD)cell));
		ITEM_MANAGER::instance().FlushDelayedSave(item);
		AddPlayerBotSidekickGift(sk->GetPlayerID(), rt, item->GetID());
		LogManager::instance().ItemLog(sk, item, "PLAYERBOT_GIFT_IN", owner->GetName());
		sys_log(0, "PLAYERBOT_SIDEKICK: ds given pid=%u owner=%u item=%u vnum=%u cell=%d wear=%d", sk->GetPlayerID(),
				owner->GetPlayerID(), item->GetID(), item->GetVnum(), cell, wantWear);
		if (toWear)
		{
			std::string worn;
			const int code = EquipPlayerBotSidekickDs(sk, item, wantWear, worn);
			answer = std::string("Dany: ") + pieceName + ". " + worn;
			return code;
		}
		answer = std::string("Dany do alchemii towarzysza: ") + pieceName + ".";
		return 0;
	}

	// From the companion's Alchemy or deck into the owner's Alchemy.
	int TakePlayerBotSidekickDs(LPCHARACTER owner, LPCHARACTER sk, TPlayerBotSidekickRuntime& rt, int from,
			std::string& answer)
	{
		LPITEM item = GetPlayerBotSidekickDsAt(sk, from);
		if (!item)
		{
			answer = "Tam nic nie ma.";
			return 3;
		}
		if (item->isLocked() || item->IsExchanging())
		{
			answer = "Ten przedmiot jest teraz zajety.";
			return 2;
		}
		if (!owner->DragonSoul_IsQualified())
		{
			answer = "Twoja alchemia nie jest jeszcze otwarta (list od Alchemika).";
			return 2;
		}
		if (owner->GetEmptyDragonSoulInventory(item) < 0)
		{
			answer = "W twojej alchemii nie ma miejsca na ten kamien.";
			return 2;
		}
		const std::string pieceName = item->GetName() ? item->GetName() : "";
		const DWORD id = item->GetID();
		if (item->IsEquipped())
		{
			const int code = PullOutPlayerBotSidekickDs(owner, sk, item, answer);
			if (code == 4)
			{
				ClearPlayerBotSidekickGift(sk->GetPlayerID(), rt, id);
				ClearPlayerBotSidekickPin(rt, id);
				return 2;
			}
			if (code != 0)
				return code;
		}
		const int cell = owner->GetEmptyDragonSoulInventory(item);
		if (cell < 0)
		{
			answer = "W twojej alchemii nie ma miejsca - kamien zostal w alchemii towarzysza.";
			return 2;
		}
		item->RemoveFromCharacter();
		item->AddToCharacter(owner, TItemPos(DRAGON_SOUL_INVENTORY, (WORD)cell));
		ITEM_MANAGER::instance().FlushDelayedSave(item);
		ClearPlayerBotSidekickGift(sk->GetPlayerID(), rt, id);
		ClearPlayerBotSidekickPin(rt, id);
		LogManager::instance().ItemLog(owner, item, "PLAYERBOT_SIDEKICK_TAKE", sk->GetName());
		sys_log(0, "PLAYERBOT_SIDEKICK: ds taken pid=%u owner=%u item=%u vnum=%u cell=%d", sk->GetPlayerID(),
				owner->GetPlayerID(), id, item->GetVnum(), cell);
		answer = std::string("Wziety od towarzysza: ") + pieceName + ".";
		return 0;
	}

	// Within the companion: from its Alchemy onto a deck (-1: the first deck
	// with the place free), and off a deck into its Alchemy.
	int MovePlayerBotSidekickDs(LPCHARACTER owner, LPCHARACTER sk, TPlayerBotSidekickRuntime& rt, int from, int to,
			std::string& answer)
	{
		LPITEM item = GetPlayerBotSidekickDsAt(sk, from);
		if (!item)
		{
			answer = "Tam nic nie ma.";
			return 3;
		}
		if (IsPlayerBotSidekickEqDsCellPos(from))
		{
			if (to == -1 || IsPlayerBotSidekickEqDsWearPos(to))
			{
				if (ArePlayerBotAlchemyOff())
				{
					answer = "Alchemia jest wylaczona na tym serwerze.";
					return 2;
				}
				return EquipPlayerBotSidekickDs(sk, item, to == -1 ? -1 : to - PLAYERBOT_SIDEKICK_EQ_WEAR_BASE, answer);
			}
			if (to == PLAYERBOT_SIDEKICK_EQ_DS_AUTO || IsPlayerBotSidekickEqDsCellPos(to))
			{
				answer = "Kamien juz lezy w alchemii towarzysza.";
				return 0;
			}
			answer = "Kamien idzie do zestawu albo do twojej torby (trafi do twojej alchemii).";
			return 2;
		}
		if (to == -1 || to == PLAYERBOT_SIDEKICK_EQ_DS_AUTO || IsPlayerBotSidekickEqDsCellPos(to))
		{
			const DWORD id = item->GetID();
			const int code = PullOutPlayerBotSidekickDs(owner, sk, item, answer);
			if (code == 4)
			{
				ClearPlayerBotSidekickGift(sk->GetPlayerID(), rt, id);
				ClearPlayerBotSidekickPin(rt, id);
				return 2;
			}
			if (code == 0)
				answer = "Zdjety do alchemii towarzysza.";
			return code;
		}
		answer = "Zdejmij go najpierw do alchemii towarzysza.";
		return 2;
	}

	// "1500000", "1.5kk", "500k", "2kkk": the amount the window's yang dialog
	// or a typed order names, 0 when it names none.
	long long ParsePlayerBotSidekickYang(const char* text)
	{
		if (!text || !*text)
			return 0;
		long long whole = 0;
		long long fraction = 0;
		long long fractionScale = 1;
		bool dot = false;
		const char* p = text;
		for (; *p; ++p)
		{
			if (*p >= '0' && *p <= '9')
			{
				if (dot)
				{
					if (fractionScale < 1000)
					{
						fraction = fraction * 10 + (*p - '0');
						fractionScale *= 10;
					}
				}
				else if (whole < 100000000000LL)
					whole = whole * 10 + (*p - '0');
				continue;
			}
			if ((*p == '.' || *p == ',') && !dot)
			{
				dot = true;
				continue;
			}
			break;
		}
		long long unit = 1;
		for (; *p == 'k' || *p == 'K'; ++p)
			unit *= 1000;
		if (*p || unit > 1000000000LL)
			return 0;
		// More than any purse holds is refused as that, not wrapped round.
		if (whole > (long long)GOLD_MAX / unit + 1)
			return (long long)GOLD_MAX + 1;
		return whole * unit + fraction * unit / fractionScale;
	}

	// Yang between the owner and the companion, either way. A trade could
	// only give it some ("Yang daje sie przez handel"), and what it gathered -
	// its drops' yang, what the merchant paid it - stayed with it: "bedzie w
	// koncu mozliwosc pobrania hajsu od towarzysza?" (Hiob, 27 September; the
	// operator: both ways). The window's two buttons send "eq yang daj
	// <kwota>" and "eq yang wez <kwota>". A purse holds GOLD_MAX either way,
	// and the game's log says who moved what.
	int MovePlayerBotSidekickYang(LPCHARACTER owner, LPCHARACTER sk, bool give, const char* amountText,
			std::string& answer)
	{
		const long long amount = ParsePlayerBotSidekickYang(amountText);
		if (amount <= 0)
		{
			answer = "Podaj kwote, np. 500000 albo 1.5kk.";
			return 9;
		}
		LPCHARACTER from = give ? owner : sk;
		LPCHARACTER to = give ? sk : owner;
		if ((long long)from->GetGold() < amount)
		{
			answer = give ? "Nie masz tyle yang." : "Towarzysz nie ma tyle yang.";
			return 2;
		}
		if ((long long)to->GetGold() + amount > (long long)GOLD_MAX)
		{
			answer = give ? "Towarzysz nie zmiesci tyle yang." : "Nie zmiescisz tyle yang.";
			return 2;
		}
		PlayerBotChangeGold(from, -amount);
		PlayerBotChangeGold(to, amount);
		LogManager::instance().CharLog(owner, (DWORD)amount,
				give ? "PLAYERBOT_SIDEKICK_YANG_GIVE" : "PLAYERBOT_SIDEKICK_YANG_TAKE", sk->GetName());
		sys_log(0, "PLAYERBOT_SIDEKICK: yang %s pid=%u name=%s owner=%u amount=%lld owner_gold=%lld sidekick_gold=%lld",
				give ? "given" : "taken", sk->GetPlayerID(), sk->GetName(), owner->GetPlayerID(), amount,
				(long long)owner->GetGold(), (long long)sk->GetGold());
		answer = std::string(give ? "Dano towarzyszowi " : "Wziete od towarzysza: ") +
				playerbot_conv::FormatYang(amount) + " yang.";
		return 0;
	}

	// The window's orders on the bag: "eq" (what changed), "eq 1" (all of
	// it), "eq ruch <z> <na>", "eq daj <twoja komorka> <na>", "eq wez <z>
	// <twoja komorka>", "eq odepnij <pozycja>", "eq yang daj|wez <kwota>".
	// Each is answered with one SidekickEqResult and then what changed.
	void HandlePlayerBotSidekickEqCommand(LPCHARACTER owner, const char* op, const char* a, const char* b)
	{
		if (!*op || !strcmp(op, "1"))
		{
			SendPlayerBotSidekickEq(owner, *op != 0);
			return;
		}
		TPlayerBotSidekickMap::iterator rec = s_mapPlayerBotSidekicks.find(owner->GetPlayerID());
		if (rec == s_mapPlayerBotSidekicks.end())
		{
			SendPlayerBotSidekickCommand(owner, "SidekickEqNone %d 0", PLAYERBOT_SIDEKICK_EQ_PROTOCOL);
			return;
		}
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec->second.dwSidekickPID);
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(rec->second.dwSidekickPID);
		if (!sk || !sk->IsItemLoaded() || st == s_mapPlayerBotAIStates.end() ||
				!CPlayerBotManager::instance().IsManaged(rec->second.dwSidekickPID))
		{
			AnswerPlayerBotSidekickEq(owner, 1, "Towarzysza nie ma teraz w grze.");
			SendPlayerBotSidekickCommand(owner, "SidekickEqNone %d 1", PLAYERBOT_SIDEKICK_EQ_PROTOCOL);
			return;
		}
		// Not before the bot it was has left its bag (FinishPlayerBotSidekickSetup):
		// an order sent blind in that moment would take out what the setup is
		// about to remove.
		if (!rec->second.bSetupDone)
		{
			AnswerPlayerBotSidekickEq(owner, 1, "Towarzysz jeszcze sie przygotowuje - sprobuj za chwile.");
			return;
		}
		TPlayerBotSidekickRuntime& rt = s_mapPlayerBotSidekickRuntime[sk->GetPlayerID()];
		TPlayerBotAIState& state = st->second;
		const int from = ParsePlayerBotSidekickEqPos(a);
		const int to = ParsePlayerBotSidekickEqPos(b);
		std::string answer;
		int code = 9;
		const bool twoBags = !strcmp(op, "daj") || !strcmp(op, "wez");
		if (!strcmp(op, "yang"))
		{
			// A trade, a counter, the safebox open on either side: the
			// engine's "busy", as for an item.
			if (strcmp(a, "daj") && strcmp(a, "wez"))
				answer = "Nieznane polecenie okna.";
			else if (!sk->CanHandleItem())
				answer = "Towarzysz jest teraz zajety (handel, magazyn albo kowal) - sprobuj za chwile.";
			else if (!owner->CanHandleItem())
				answer = "Zamknij najpierw handel, sklep albo magazyn.";
			else
				code = MovePlayerBotSidekickYang(owner, sk, !strcmp(a, "daj"), b, answer);
		}
		else if (!strcmp(op, "ruch") || twoBags || !strcmp(op, "odepnij"))
		{
			if (from == INT_MIN || (strcmp(op, "odepnij") && to == INT_MIN))
				answer = "Zle miejsce.";
			// A trade, a counter, the safebox, the anvil: the engine's own
			// "busy", for both of them.
			else if (!sk->CanHandleItem())
				answer = "Towarzysz jest teraz zajety (handel, magazyn albo kowal) - sprobuj za chwile.";
			// A worn piece taken off a transformed companion stays off until the
			// transformation ends (IsPlayerBotGearFrozen).
			else if (strcmp(op, "odepnij") && IsPlayerBotGearFrozen(sk) &&
					(IsPlayerBotSidekickEqWearPos(from) || IsPlayerBotSidekickEqWearPos(to) ||
					(!strcmp(op, "ruch") && to == -1)))
				answer = "Towarzysz jest teraz przemieniony - zmiana sprzetu poczeka do konca przemiany.";
			else if (twoBags && !owner->CanHandleItem())
				answer = "Zamknij najpierw handel, sklep albo magazyn.";
			// MT2009_PLUS_SIDEKICK_PANELS_V1: the stones of the Alchemy window.
			else if (!strcmp(op, "daj") && IsPlayerBotSidekickEqDsCellPos(from))
				code = GivePlayerBotSidekickDs(owner, sk, rt, from - PLAYERBOT_SIDEKICK_EQ_DS_BASE, to, answer);
			else if (!strcmp(op, "wez") && IsPlayerBotSidekickEqDsPos(from))
				code = TakePlayerBotSidekickDs(owner, sk, rt, from, answer);
			else if (!strcmp(op, "ruch") && IsPlayerBotSidekickEqDsPos(from))
				code = MovePlayerBotSidekickDs(owner, sk, rt, from, to, answer);
			else if (!strcmp(op, "ruch"))
			{
				if (IsPlayerBotSidekickEqBagPos(from) && IsPlayerBotSidekickEqBagPos(to))
					code = MovePlayerBotSidekickBagItem(sk, from, to, answer);
				else if (IsPlayerBotSidekickEqBagPos(from) && (to == -1 || IsPlayerBotSidekickEqWearPos(to)))
				{
					LPITEM item = sk->GetInventoryItem(from);
					if (!item || item->GetCell() != from)
					{
						answer = "Tam nic nie ma.";
						code = 3;
					}
					// MT2009_PLUS_SIDEKICK_HAIR_V1: the Bleach or a dye right-clicked
					// in its bag, or dropped on the hair slot, is used.
					else if (IsPlayerBotSidekickHairTool(item) &&
							(to == -1 || to == PLAYERBOT_SIDEKICK_EQ_WEAR_BASE + WEAR_COSTUME_HAIR))
						code = UsePlayerBotSidekickHairTool(owner, sk, rt, item, answer);
					else
						code = EquipPlayerBotSidekickForOwner(owner, sk, state, rt, item,
								to == -1 ? -1 : to - PLAYERBOT_SIDEKICK_EQ_WEAR_BASE, answer);
				}
				else if (IsPlayerBotSidekickEqWearPos(from) && (to == -1 || IsPlayerBotSidekickEqBagPos(to)))
					code = UnequipPlayerBotSidekickForOwner(owner, sk, rt, from - PLAYERBOT_SIDEKICK_EQ_WEAR_BASE, to,
							answer);
				else
					answer = "Przeciagnij to do torby towarzysza.";
			}
			else if (!strcmp(op, "daj"))
			{
				if (!IsPlayerBotSidekickEqBagPos(from))
					answer = "Zle miejsce.";
				else
					code = GivePlayerBotSidekickItem(owner, sk, state, rt, from, to, answer);
			}
			else if (!strcmp(op, "wez"))
			{
				if (from == -1 || !(to == -1 || IsPlayerBotSidekickEqBagPos(to)))
					answer = "Zle miejsce.";
				else
					code = TakePlayerBotSidekickItem(owner, sk, rt, from, to, answer);
			}
			else
			{
				LPITEM item = IsPlayerBotSidekickEqWearPos(from) ? sk->GetWear(from - PLAYERBOT_SIDEKICK_EQ_WEAR_BASE)
						: IsPlayerBotSidekickEqDsPos(from) ? GetPlayerBotSidekickDsAt(sk, from)	// MT2009_PLUS_SIDEKICK_PANELS_V1
						: (IsPlayerBotSidekickEqBagPos(from) ? sk->GetInventoryItem(from) : NULL);
				if (!item || (IsPlayerBotSidekickEqBagPos(from) && item->GetCell() != from))
				{
					answer = "Tam nic nie ma.";
					code = 3;
				}
				else
				{
					const bool unwanted = IsPlayerBotSidekickUnwanted(sk, item);
					ClearPlayerBotSidekickPin(rt, item->GetID());
					state.dwNextEquipmentCheckTime = 0;
					answer = unwanted ? "Towarzysz moze to znow zalozyc sam." :
							"Odpiete. Towarzysz znow sam wybiera, co tam nosi.";
					code = 0;
				}
			}
		}
		else
			answer = "Nieznane polecenie okna.";
		sys_log(0, "PLAYERBOT_SIDEKICK: eq %s owner=%u pid=%u from=%s to=%s code=%d", op, owner->GetPlayerID(),
				sk->GetPlayerID(), a, b, code);
		AnswerPlayerBotSidekickEq(owner, code, answer.c_str());
		SendPlayerBotSidekickEq(owner, false);
	}

	// The six skills of the path it walks: from one, thirty-one, sixty-one and
	// ninety-one by class, the second path fifteen further on - the rule of
	// skill_proto and of every client's skill window. 0 with no path.
	DWORD GetPlayerBotSidekickSkillBase(LPCHARACTER sk)
	{
		static const DWORD bases[4] = { 1, 31, 61, 91 };
		const int job = sk->GetJob();
		const int group = sk->GetSkillGroup();
		if (job < 0 || job > 3 || group < 1 || group > 2)
			return 0;
		return bases[job] + (DWORD)(group - 1) * 15;
	}

	void SendPlayerBotSidekickSkills(LPCHARACTER owner)
	{
		if (!owner || !owner->GetDesc() || (owner->GetDesc()->IsBot() && !s_bPlayerBotSidekickSelfTest))
			return;
		TPlayerBotSidekickMap::iterator rec = s_mapPlayerBotSidekicks.find(owner->GetPlayerID());
		if (rec == s_mapPlayerBotSidekicks.end())
		{
			SendPlayerBotSidekickCommand(owner, "SidekickEqNone %d 0", PLAYERBOT_SIDEKICK_EQ_PROTOCOL);
			return;
		}
		LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec->second.dwSidekickPID);
		if (!sk || !CPlayerBotManager::instance().IsManaged(rec->second.dwSidekickPID))
		{
			SendPlayerBotSidekickCommand(owner, "SidekickEqNone %d 1", PLAYERBOT_SIDEKICK_EQ_PROTOCOL);
			return;
		}
		// After the five the window has always read, what the client's skill
		// tooltip computes its numbers from - the companion's, because the
		// client's own tooltip reads the player's (CPythonSkill's
		// ProcessFormula asks CPythonPlayer::GetStatus). An older client reads
		// the five and nothing more. The skill power by level and the battle
		// points (hit rate, the attack from the weapon) the client works out
		// itself, as it does for the player, from the level, the stats and
		// the weapon's vnum.
		LPITEM weapon = sk->GetWear(WEAR_WEAPON);
#if defined(PLAYERBOT_ENGINE_MT2009)
		const int magicAtt = (int)sk->GetPoint(POINT_MAGIC_ATT);
		const int skillDuration = (int)sk->GetPoint(POINT_SKILL_DURATION);
#else
		const int magicAtt = 0;
		const int skillDuration = 0;
#endif
		// And after those, the stat points of the window's status page
		// (uisidekick.py): the points left, who spends them, whether the one
		// free reset is still there, and the four stats as spent - the real
		// points, without what the gear adds - in the order the character window
		// shows them (vitality, intelligence, strength, dexterity).
		//
		// And last, what that page shows as the player's own character window
		// shows it and the client cannot work out for another character
		// (Piciu713, 28 September: "jaki zakres ataku ma moj towarzysz i ile ma
		// obrony"): the experience and what the level needs, the attack the
		// gear, the party and the monster grades add over the weapon's own
		// (uicharacter.py's atkBonus and attackerBonus - the weapon's the
		// client reads from its item table, as it does for the player), the
		// defence boost in percent, and the moving speed. An older client reads
		// none of it.
		int attackBonus = (int)sk->GetPoint(POINT_ATT_GRADE_BONUS) + (int)sk->GetPoint(POINT_PARTY_ATTACKER_BONUS);
#if defined(PLAYERBOT_ENGINE_MT2009)
		attackBonus += (int)sk->GetPoint(POINT_DAGGER_ATT_GRADE_MONSTER) + (int)sk->GetPoint(POINT_ATT_GRADE_MONSTER);
#endif
		SendPlayerBotSidekickCommand(owner,
				"SidekickSkillBegin %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %u %d %d %d %d %d %d %d "
				"%u %u %d %d %d",
				PLAYERBOT_SIDEKICK_EQ_PROTOCOL, (int)sk->GetPoint(POINT_SKILL), (int)sk->GetJob(),
				(int)sk->GetSkillGroup(), rec->second.bManualSkills ? 1 : 0, (int)sk->GetLevel(),
				(int)sk->GetPoint(POINT_ST), (int)sk->GetPoint(POINT_DX), (int)sk->GetPoint(POINT_HT),
				(int)sk->GetPoint(POINT_IQ), (int)sk->GetMaxHP(), (int)sk->GetMaxSP(),
				(int)sk->GetPoint(POINT_DEF_GRADE), (int)sk->GetPoint(POINT_MAGIC_ATT_GRADE),
				(int)sk->GetPoint(POINT_ATT_SPEED), magicAtt, skillDuration,
				(int)sk->GetPoint(POINT_PARTY_BUFFER_BONUS), (int)sk->GetPoint(POINT_CASTING_SPEED),
				weapon ? (unsigned int)weapon->GetVnum() : 0U,
				(int)sk->GetPoint(POINT_STAT), rec->second.bManualStats ? 1 : 0, rec->second.bStatResetUsed ? 0 : 1,
				(int)sk->GetRealPoint(POINT_HT), (int)sk->GetRealPoint(POINT_IQ), (int)sk->GetRealPoint(POINT_ST),
				(int)sk->GetRealPoint(POINT_DX),
				(unsigned int)sk->GetExp(), (unsigned int)sk->GetNextExp(), attackBonus,
				(int)sk->GetPoint(POINT_DEF_BONUS), (int)sk->GetPoint(POINT_MOV_SPEED));
		const DWORD base = GetPlayerBotSidekickSkillBase(sk);
		for (DWORD vnum = base; base != 0 && vnum < base + 6; ++vnum)
			if (CSkillManager::instance().Get(vnum))
				SendPlayerBotSidekickCommand(owner, "SidekickSkill %u %d %d", vnum, (int)sk->GetSkillLevel(vnum),
						(int)sk->GetSkillMasterType(vnum));
		SendPlayerBotSidekickCommand(owner, "SidekickSkillEnd");
	}

	void SetPlayerBotSidekickManualSkills(TPlayerBotSidekick& rec, bool manual)
	{
		rec.bManualSkills = manual;
		SetPlayerBotSidekickSetting(rec, "manual_skills", manual ? 1U : 0U);
	}

	// "umiejetnosci zeruj <vnum>": the owner takes one of the companion's skills
	// back to nothing, with what a player would use, out of the companion's own
	// bag - "mozliwosc resetowania jego umiejetnosci do zera za pomoca KZ lub
	// zwoju powrotu umiejetnosci" (blasty, 28 September). The Forgetting Book
	// takes one level with its point (SkillLevelDown) and never a Master's; the
	// skill reset scroll takes the whole skill, Master and all, and does what
	// its quest does (ResetOneSkill, the next Master forced). Books when the
	// bag holds enough to reach zero, because the scroll is the ItemShop's;
	// the scroll when they are too few, or for a Master; and nothing half-way:
	// the owner asked for zero. The points wait in the window, the owner's to
	// spend from then on, as after a "+".
	bool ResetPlayerBotSidekickSkill(LPCHARACTER sk, TPlayerBotSidekick& rec, DWORD vnum, std::string& answer)
	{
		char text[192];
		const int before = (int)sk->GetSkillLevel(vnum);
		const bool normal = sk->GetSkillMasterType(vnum) == SKILL_NORMAL;
		std::vector<WORD> bookCells;
		int books = 0;
#if defined(PLAYERBOT_ENGINE_MT2009)
		LPITEM scroll = NULL;
#endif
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = sk->GetInventoryItem(cell);
			if (!item || item->isLocked())
				continue;
			if (item->GetType() == ITEM_SKILLFORGET && (DWORD)item->GetSocket(0) == vnum)
			{
				bookCells.push_back(cell);
				books += (int)item->GetCount();
			}
#if defined(PLAYERBOT_ENGINE_MT2009)
			else if (!scroll && item->GetVnum() == PLAYERBOT_SIDEKICK_SKILL_RESET_SCROLL_VNUM)
				scroll = item;
#endif
		}
		const char* way = "none";
		if (normal && books >= before)
		{
			way = "books";
			int read = 0;
			for (size_t i = 0; i < bookCells.size() && sk->GetSkillLevel(vnum) > 0; ++i)
			{
				for (int guard = 0; guard < 40 && sk->GetSkillLevel(vnum) > 0; ++guard)
				{
					LPITEM item = sk->GetInventoryItem(bookCells[i]);
					if (!item || item->GetType() != ITEM_SKILLFORGET || (DWORD)item->GetSocket(0) != vnum)
						break;
					const int was = (int)sk->GetSkillLevel(vnum);
					sk->UseItem(TItemPos(INVENTORY, bookCells[i]));
					if ((int)sk->GetSkillLevel(vnum) >= was)
						break;
					++read;
				}
			}
			if (sk->GetSkillLevel(vnum) > 0)
				snprintf(text, sizeof(text), "Przeczytane Ksiegi Zapomnienia: %d, %s stoi na %d - reszty silnik nie przyjal.",
						read, GetPlayerBotSkillName(vnum), (int)sk->GetSkillLevel(vnum));
			else
				snprintf(text, sizeof(text), "Przeczytane Ksiegi Zapomnienia: %d - %s od zera, punkty czekaja w oknie.",
						read, GetPlayerBotSkillName(vnum));
		}
#if defined(PLAYERBOT_ENGINE_MT2009)
		else if (scroll)
		{
			way = "scroll";
			if (!sk->ResetOneSkill(vnum))
				snprintf(text, sizeof(text), "Zwoj Powrotu Umiejetnosci nie zadzialal.");
			else
			{
				sk->SetQuestFlag("reset_status_items.force_to_master_skill",
						sk->GetQuestFlag("reset_status_items.force_to_master_skill") + 1);
				scroll->SetCount(scroll->GetCount() - 1);
				sk->Save();
				snprintf(text, sizeof(text), "Zwoj Powrotu Umiejetnosci uzyty: %s od zera, punkty czekaja w oknie. "
						"Kolejna umiejetnosc na 17 zostanie mistrzem.", GetPlayerBotSkillName(vnum));
			}
		}
#endif
		else if (!normal)
			snprintf(text, sizeof(text), "Mistrza nie cofnie Ksiega Zapomnienia - wloz do plecaka towarzysza "
					"Zwoj Powrotu Umiejetnosci.");
		else
			snprintf(text, sizeof(text), "Do zera trzeba %d Ksiag Zapomnienia tej umiejetnosci, w plecaku towarzysza "
					"jest %d - albo wloz mu Zwoj Powrotu Umiejetnosci.", before, books);
		answer = text;
		const bool done = sk->GetSkillLevel(vnum) < before;
		if (done && !rec.bManualSkills)
			SetPlayerBotSidekickManualSkills(rec, true);
		sys_log(0, "PLAYERBOT_SIDEKICK: skill reset owner=%u pid=%u vnum=%u way=%s master=%d level=%d->%d books=%d points=%d",
				rec.dwOwnerPID, sk->GetPlayerID(), vnum, way, normal ? 0 : 1, before, (int)sk->GetSkillLevel(vnum), books,
				(int)sk->GetPoint(POINT_SKILL));
		return done;
	}

	// "umiejetnosci" (the list), "umiejetnosci dodaj <vnum>" (one point there -
	// and from then on the owner spends them, or the skill pass would move the
	// point to its own build), "umiejetnosci reczne <0|1>", "umiejetnosci zeruj
	// <vnum>" (ResetPlayerBotSidekickSkill).
	void HandlePlayerBotSidekickSkillCommand(LPCHARACTER owner, const char* op, const char* a)
	{
		if (!*op)
		{
			SendPlayerBotSidekickSkills(owner);
			return;
		}
		TPlayerBotSidekickMap::iterator rec = s_mapPlayerBotSidekicks.find(owner->GetPlayerID());
		LPCHARACTER sk = rec == s_mapPlayerBotSidekicks.end() ? NULL :
				CHARACTER_MANAGER::instance().FindByPID(rec->second.dwSidekickPID);
		if (!sk || !CPlayerBotManager::instance().IsManaged(rec->second.dwSidekickPID))
		{
			AnswerPlayerBotSidekickEq(owner, 1, "Towarzysza nie ma teraz w grze.");
			SendPlayerBotSidekickSkills(owner);
			return;
		}
		std::string answer;
		int code = 2;
		char text[160];
		if (!strcmp(op, "reczne") && (!strcmp(a, "0") || !strcmp(a, "1")))
		{
			SetPlayerBotSidekickManualSkills(rec->second, !strcmp(a, "1"));
			answer = rec->second.bManualSkills ? "Punkty umiejetnosci rozdajesz teraz ty." :
					"Punkty umiejetnosci rozdaje znow towarzysz.";
			code = 0;
		}
		else if (!strcmp(op, "dodaj"))
		{
			DWORD vnum = 0;
			str_to_number(vnum, a);
			const DWORD base = GetPlayerBotSidekickSkillBase(sk);
			if (base == 0)
				answer = "Towarzysz nie ma jeszcze sciezki (dostanie ja na 5 poziomie).";
			else if (vnum < base || vnum >= base + 6 || !CSkillManager::instance().Get(vnum))
				answer = "To nie jest umiejetnosc towarzysza.";
			else if (sk->GetPoint(POINT_SKILL) <= 0)
				answer = "Towarzysz nie ma wolnych punktow umiejetnosci.";
			else if (sk->GetSkillMasterType(vnum) != SKILL_NORMAL)
				answer = "Te umiejetnosc rozwijaja juz tylko ksiegi i kamienie duchowe.";
			else if (sk->GetSkillLevel(vnum) >= 17)
				answer = "Na 17 poziomie umiejetnosc czeka na mistrza - dalej tylko ksiegi albo reset u Starszej Pani.";
			else
			{
				const int before = sk->GetSkillLevel(vnum);
				const int typeBefore = sk->GetSkillMasterType(vnum);
				sk->SkillLevelUp(vnum);
				const int after = sk->GetSkillLevel(vnum);
				if (after > before || sk->GetSkillMasterType(vnum) != typeBefore)
				{
					if (!rec->second.bManualSkills)
						SetPlayerBotSidekickManualSkills(rec->second, true);
					snprintf(text, sizeof(text), "Umiejetnosc na poziomie %d%s. Punkty rozdajesz teraz ty.", after,
							sk->GetSkillMasterType(vnum) != typeBefore ? " - mistrz!" : "");
					answer = text;
					code = 0;
				}
				else
					answer = "Nie udalo sie - poziom towarzysza jest za niski na te umiejetnosc.";
			}
			sys_log(0, "PLAYERBOT_SIDEKICK: skill up owner=%u pid=%u vnum=%u code=%d level=%d points=%d",
					owner->GetPlayerID(), sk->GetPlayerID(), vnum, code, (int)sk->GetSkillLevel(vnum),
					(int)sk->GetPoint(POINT_SKILL));
		}
		else if (!strcmp(op, "zeruj"))
		{
			DWORD vnum = 0;
			str_to_number(vnum, a);
			const DWORD base = GetPlayerBotSidekickSkillBase(sk);
			if (base == 0)
				answer = "Towarzysz nie ma jeszcze sciezki (dostanie ja na 5 poziomie).";
			else if (vnum < base || vnum >= base + 6 || !CSkillManager::instance().Get(vnum))
				answer = "To nie jest umiejetnosc towarzysza.";
			else if (sk->GetSkillLevel(vnum) <= 0)
				answer = "Ta umiejetnosc jest juz na zerze.";
			else if (sk->IsPolymorphed() || sk->IsDead() || sk->GetExchange())
				answer = "Nie teraz - sprobuj, gdy towarzysz nie walczy przemieniony, nie lezy i nie handluje.";
			else if (ResetPlayerBotSidekickSkill(sk, rec->second, vnum, answer))
				code = 0;
		}
		else
			answer = "Nieznane polecenie okna.";
		AnswerPlayerBotSidekickEq(owner, code, answer.c_str());
		SendPlayerBotSidekickSkills(owner);
	}

	void SetPlayerBotSidekickManualStats(TPlayerBotSidekick& rec, bool manual)
	{
		rec.bManualStats = manual;
		SetPlayerBotSidekickSetting(rec, "manual_stats", manual ? 1U : 0U);
	}

	// "statystyki" (the numbers come with the skills, SidekickSkillBegin),
	// "statystyki dodaj <ht|iq|st|dx> [ile]", "statystyki reczne <0|1>" and
	// "statystyki odnow": Kiciamol's question of 25 September, "Dodasz
	// jeszcze mozliwosc dodawania statystyk przez gracza?" - the operator: "Tak". The
	// first point the owner spends makes the points the owner's
	// (ManagePlayerBotStats spends none after it), as with the skills. The AI
	// spends a point within a second of the level that brings it, so by the
	// time an owner opens the window every point is spent: the owner gets one
	// reset of them for nothing - CHARACTER::ResetPoint, what a stat reset
	// does to a player - once per companion, and spends them all anew.
	void HandlePlayerBotSidekickStatCommand(LPCHARACTER owner, const char* op, const char* a, const char* b)
	{
		if (!*op)
		{
			SendPlayerBotSidekickSkills(owner);
			return;
		}
		TPlayerBotSidekickMap::iterator rec = s_mapPlayerBotSidekicks.find(owner->GetPlayerID());
		LPCHARACTER sk = rec == s_mapPlayerBotSidekicks.end() ? NULL :
				CHARACTER_MANAGER::instance().FindByPID(rec->second.dwSidekickPID);
		if (!sk || !CPlayerBotManager::instance().IsManaged(rec->second.dwSidekickPID))
		{
			AnswerPlayerBotSidekickEq(owner, 1, "Towarzysza nie ma teraz w grze.");
			SendPlayerBotSidekickSkills(owner);
			return;
		}
		std::string answer;
		int code = 2;
		char text[192];
		if (!strcmp(op, "reczne") && (!strcmp(a, "0") || !strcmp(a, "1")))
		{
			SetPlayerBotSidekickManualStats(rec->second, !strcmp(a, "1"));
			answer = rec->second.bManualStats ? "Punkty statystyk rozdajesz teraz ty." :
					"Punkty statystyk rozdaje znow towarzysz.";
			code = 0;
		}
		else if (!strcmp(op, "dodaj"))
		{
			BYTE point = 0;
			const char* name = "";
			if (!strcmp(a, "ht") || !strcmp(a, "wit"))
			{
				point = POINT_HT;
				name = "witalnosc";
			}
			else if (!strcmp(a, "iq") || !strcmp(a, "int"))
			{
				point = POINT_IQ;
				name = "inteligencja";
			}
			else if (!strcmp(a, "st") || !strcmp(a, "sil"))
			{
				point = POINT_ST;
				name = "sila";
			}
			else if (!strcmp(a, "dx") || !strcmp(a, "zr"))
			{
				point = POINT_DX;
				name = "zrecznosc";
			}
			int wanted = 1;
			if (*b)
				str_to_number(wanted, b);
			wanted = MINMAX(1, wanted, PLAYERBOT_SIDEKICK_STAT_ORDER_MAX);
			if (point == 0)
				answer = "Nieznana statystyka.";
			else if (sk->IsPolymorphed())
				answer = "Towarzysz jest teraz przemieniony - statystyki poczekaja.";
			else if (sk->GetPoint(POINT_STAT) <= 0)
				answer = "Towarzysz nie ma wolnych punktow statystyk.";
			else
			{
				int added = 0;
				while (added < wanted && AllocatePlayerBotStat(sk, point))
					++added;
				if (added > 0)
				{
					if (!rec->second.bManualStats)
						SetPlayerBotSidekickManualStats(rec->second, true);
					snprintf(text, sizeof(text), "%s: +%d (teraz %d). Wolne punkty: %d. Rozdajesz je teraz ty.",
							name, added, (int)sk->GetRealPoint(point), (int)sk->GetPoint(POINT_STAT));
					answer = text;
					code = 0;
				}
				else
					answer = "Tej statystyki nie da sie juz podniesc (90 to najwiecej).";
			}
			sys_log(0, "PLAYERBOT_SIDEKICK: stat up owner=%u pid=%u point=%u code=%d value=%d points=%d",
					owner->GetPlayerID(), sk->GetPlayerID(), (unsigned int)point, code,
					point ? (int)sk->GetRealPoint(point) : 0, (int)sk->GetPoint(POINT_STAT));
		}
		else if (!strcmp(op, "odnow"))
		{
			if (rec->second.bStatResetUsed)
				answer = "Darmowy reset statystyk towarzysz juz wykorzystal.";
			else if (sk->IsDead())
				answer = "Najpierw musi wstac.";
			else if (sk->IsPolymorphed())
				answer = "Towarzysz jest teraz przemieniony - reset poczeka.";
			else
			{
				const int before = (int)sk->GetPoint(POINT_STAT);
				sk->ResetPoint(sk->GetLevel());
				rec->second.bStatResetUsed = true;
				SetPlayerBotSidekickSetting(rec->second, "stat_reset", 1U);
				if (!rec->second.bManualStats)
					SetPlayerBotSidekickManualStats(rec->second, true);
				snprintf(text, sizeof(text), "Statystyki wrocily do poczatkowych. Masz %d punktow do rozdania.",
						(int)sk->GetPoint(POINT_STAT));
				answer = text;
				code = 0;
				sys_log(0, "PLAYERBOT_SIDEKICK: stats reset owner=%u pid=%u level=%d points=%d->%d",
						owner->GetPlayerID(), sk->GetPlayerID(), (int)sk->GetLevel(), before,
						(int)sk->GetPoint(POINT_STAT));
			}
		}
		else
			answer = "Nieznane polecenie okna.";
		AnswerPlayerBotSidekickEq(owner, code, answer.c_str());
		SendPlayerBotSidekickSkills(owner);
	}

	// Whether the folded whisper holds the phrase as whole words.
	bool PlayerBotSidekickHeard(const char* folded, const char* phrase)
	{
		const size_t n = strlen(phrase);
		for (const char* p = strstr(folded, phrase); p; p = strstr(p + 1, phrase))
		{
			const bool startOk = p == folded || !isalnum((unsigned char)p[-1]);
			const bool endOk = !isalnum((unsigned char)p[n]);
			if (startOk && endOk)
				return true;
		}
		return false;
	}

	// The owner's whisper to its own companion: the letter's menu without the
	// letter - "chodz", "graj sam", "czekaj", "zakupy", "stan", and how it
	// fights ("atakuj", "nie atakuj pierwszy", "nie walcz"; one whisper may
	// carry a stance and an order). Anything else is a conversation, and the
	// conversation layer answers it as it answers anybody
	// (playerbot_chat_conversation.h).
	bool HandlePlayerBotSidekickWhisper(LPCHARACTER from, LPCHARACTER bot, const char* text)
	{
		if (!from || !bot || !text || s_mapPlayerBotSidekickOwner.empty())
			return false;
		TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(bot->GetPlayerID());
		if (!rec || rec->dwOwnerPID != from->GetPlayerID())
			return false;
		char folded[160];
		FoldPlayerBotChatText(text, folded, sizeof(folded));
		bool handled = false;
		const int stance = GetPlayerBotSidekickStanceHeard(folded);
		if (stance >= 0)
		{
			SendPlayerBotWhisper(bot, from, SetPlayerBotSidekickStance(*rec, (BYTE)stance));
			handled = true;
		}
		// The lure, in the words any bot is asked to lure with; a bare "stop"
		// is about the lure only while the companion lures.
		const playerbot_lure_rules::EOrder lureOrder = playerbot_lure_rules::ParseOrder(folded);
		if (lureOrder == playerbot_lure_rules::ORDER_START ||
				(lureOrder == playerbot_lure_rules::ORDER_STOP && (rec->bLure || strstr(folded, "lur"))))
		{
			SendPlayerBotWhisper(bot, from, SetPlayerBotSidekickLure(*rec, lureOrder == playerbot_lure_rules::ORDER_START));
			return true;
		}
		static const char* const summonWords[] = { "chodz", "do mnie", "wracaj", "wroc", "za mna", "przywolaj",
				"tutaj", "come", "follow" };
		static const char* const freeWords[] = { "graj sam", "graj po swojemu", "wolna reka", "idz expic",
				"expij sam", "idz sam", "go play" };
		// Before the call, whose "tutaj" is in "czekaj tutaj".
		static const char* const holdWords[] = { "czekaj", "zaczekaj", "poczekaj", "zostan", "stoj", "wait" };
		// "Zrob miejsce w eq" is what a player writes when the bag is full
		// (a player's screenshot of 25 September: the companion answered it with
		// talk, "Zero, EQ pelne"): the errand is where the merchant takes the junk.
		static const char* const errandWords[] = { "zakupy", "na zakupy", "do miasta", "idz do miasta",
				"zrob miejsce", "oproznij", "wyczysc eq", "sprzedaj smieci" };
		for (size_t i = 0; i < sizeof(freeWords) / sizeof(freeWords[0]); ++i)
			if (PlayerBotSidekickHeard(folded, freeWords[i]))
			{
				SendPlayerBotWhisper(bot, from, rec->bMode == PLAYERBOT_SIDEKICK_FREE ?
						"Juz gram po swojemu. Napisz \"chodz\", kiedy bede potrzebny." :
						"Dobra, ide expic po swojemu. Napisz \"chodz\", kiedy bede potrzebny.");
				if (rec->bMode != PLAYERBOT_SIDEKICK_FREE)
					FreePlayerBotSidekick(from, *rec);
				return true;
			}
		for (size_t i = 0; i < sizeof(errandWords) / sizeof(errandWords[0]); ++i)
			if (PlayerBotSidekickHeard(folded, errandWords[i]))
			{
				SendPlayerBotWhisper(bot, from, "Dobra, zaraz zobacze, co trzeba kupic.");
				SendPlayerBotSidekickShopping(from, *rec, get_dword_time());
				return true;
			}
		for (size_t i = 0; i < sizeof(holdWords) / sizeof(holdWords[0]); ++i)
			if (PlayerBotSidekickHeard(folded, holdWords[i]))
			{
				SendPlayerBotWhisper(bot, from, "Dobra, czekam.");
				HoldPlayerBotSidekick(from, *rec, get_dword_time());
				return true;
			}
		for (size_t i = 0; i < sizeof(summonWords) / sizeof(summonWords[0]); ++i)
			if (PlayerBotSidekickHeard(folded, summonWords[i]))
			{
				SendPlayerBotWhisper(bot, from, "Juz ide!");
				SummonPlayerBotSidekick(from, *rec, get_dword_time());
				return true;
			}
		if (PlayerBotSidekickHeard(folded, "stan") || PlayerBotSidekickHeard(folded, "status"))
		{
			ReportPlayerBotSidekick(from, *rec);
			return true;
		}
		return handled;
	}

	// /towarzysz stworz <rasa 0-7> <sciezka 1-2> <nick> | przywolaj | wolny | czekaj | zakupy | stan
	//            | walka <0 atakuj, 1 nie atakuj pierwszy, 2 nie walcz> | zbieraj <0 nic, 1 twoj, 2 wszystko>
	//            | ochrona <0|1> | buffy <0|1> | okno [1] | odprawa tak
	//            | eq [1 | ruch <z> <na> | daj <z> <na> | wez <z> <na> | odepnij <pozycja>]
	//            | umiejetnosci [dodaj <vnum> | reczne <0|1>]
	//            | statystyki [dodaj <ht|iq|st|dx> [ile] | reczne <0|1> | odnow]
	//            | luruj <0|1> | kup <towar> <ile> [tak]
	void HandlePlayerBotSidekickCommand(LPCHARACTER ch, const char* argument)
	{
		if (!ch || !ch->GetDesc() || (ch->GetDesc()->IsBot() && !s_bPlayerBotSidekickSelfTest))
			return;
		if (!EnsurePlayerBotSidekickTable())
		{
			SayPlayerBotSidekick(ch, "Towarzysze sa teraz niedostepni - baza nie odpowiada.");
			return;
		}
		char sub[32] = "";
		char a1[32] = "";
		char a2[32] = "";
		char a3[64] = "";
		const char* rest = one_argument(argument ? argument : "", sub, sizeof(sub));
		rest = one_argument(rest, a1, sizeof(a1));
		rest = one_argument(rest, a2, sizeof(a2));
		one_argument(rest, a3, sizeof(a3));
		const DWORD dwNow = get_dword_time();
		// The world's switch: the window is told (a third answer, "2"), and
		// everything else - a new companion included - is refused.
		if (IsPlayerBotSidekickSwitchedOff())
		{
			if (!strcmp(sub, "okno"))
				SendPlayerBotSidekickCommand(ch, "SidekickInfo %d 2", PLAYERBOT_SIDEKICK_WINDOW_PROTOCOL);
			else if (!strcmp(sub, "eq") || !strcmp(sub, "umiejetnosci") || !strcmp(sub, "statystyki"))
				SendPlayerBotSidekickCommand(ch, "SidekickEqNone %d 2", PLAYERBOT_SIDEKICK_EQ_PROTOCOL);
			else
				SayPlayerBotSidekick(ch, "Towarzysze sa wylaczeni na tym serwerze (wlacza je wlasciciel serwera w launcherze).");
			return;
		}
		if (!strcmp(sub, "stworz"))
		{
			int race = -1, group = 0;
			str_to_number(race, a1);
			str_to_number(group, a2);
			CreatePlayerBotSidekick(ch, race, group, a3);
			return;
		}
		// The window asks every second and a half while it is open, and for the
		// whole gear when it opens ("okno 1"); with no companion it is told so.
		if (!strcmp(sub, "okno"))
		{
			SendPlayerBotSidekickWindow(ch, !strcmp(a1, "1"));
			return;
		}
		// The bag window (uisidekickinventory.py) and the status and skill pages
		// of the companion's window (uisidekick.py): each answer says for
		// itself that there is no companion.
		if (!strcmp(sub, "eq"))
		{
			HandlePlayerBotSidekickEqCommand(ch, a1, a2, a3);
			return;
		}
		if (!strcmp(sub, "umiejetnosci"))
		{
			HandlePlayerBotSidekickSkillCommand(ch, a1, a2);
			return;
		}
		if (!strcmp(sub, "statystyki"))
		{
			HandlePlayerBotSidekickStatCommand(ch, a1, a2, a3);
			return;
		}
		TPlayerBotSidekickMap::iterator rec = s_mapPlayerBotSidekicks.find(ch->GetPlayerID());
		if (rec == s_mapPlayerBotSidekicks.end())
		{
			SetPlayerBotSidekickFlag(ch->GetPlayerID(), "towarzysz.created", 0);
			SayPlayerBotSidekick(ch, "Nie masz jeszcze towarzysza - wybierz go w liscie Towarzysz.");
			return;
		}
		if (!strcmp(sub, "przywolaj"))
			SummonPlayerBotSidekick(ch, rec->second, dwNow);
		else if (!strcmp(sub, "wolny"))
			FreePlayerBotSidekick(ch, rec->second);
		else if (!strcmp(sub, "czekaj"))
			HoldPlayerBotSidekick(ch, rec->second, dwNow);
		else if (!strcmp(sub, "zakupy"))
			SendPlayerBotSidekickShopping(ch, rec->second, dwNow);
		// MT2009_PLUS_SIDEKICK_SHOP_ERRAND_V1: "kup <towar> <ile> [tak]".
		else if (!strcmp(sub, "kup"))
			OrderPlayerBotSidekickShopErrand(ch, rec->second, a1, a2, a3, dwNow);
		else if (!strcmp(sub, "ryby"))
			SendPlayerBotSidekickFishing(ch, rec->second, dwNow);
		else if (!strcmp(sub, "luruj"))
		{
			if (!strcmp(a1, "0") || !strcmp(a1, "1"))
				SayPlayerBotSidekick(ch, SetPlayerBotSidekickLure(rec->second, !strcmp(a1, "1")));
			else
				SayPlayerBotSidekick(ch, "Uzyj: /towarzysz luruj 1 (lurowanie wlaczone) albo /towarzysz luruj 0");
		}
		else if (!strcmp(sub, "sam"))
		{
			if (!strcmp(a1, "0") || !strcmp(a1, "1"))
				SayPlayerBotSidekick(ch, SetPlayerBotSidekickSolo(rec->second, !strcmp(a1, "1")));
			else
				SayPlayerBotSidekick(ch, "Uzyj: /towarzysz sam 1 (gram dalej, kiedy wyjdziesz z gry) albo /towarzysz sam 0");
		}
		else if (!strcmp(sub, "lider"))
		{
			if (!strcmp(a1, "0") || !strcmp(a1, "1"))
			{
				SayPlayerBotSidekick(ch, SetPlayerBotSidekickLead(rec->second, !strcmp(a1, "1")));
				if (LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec->second.dwSidekickPID))
					if (rec->second.bMode == PLAYERBOT_SIDEKICK_FOLLOW)
						KeepPlayerBotSidekickInParty(sk, ch, dwNow);
			}
			else
				SayPlayerBotSidekick(ch, "Uzyj: /towarzysz lider 1 (ja zakladam grupe) albo /towarzysz lider 0 (ty prowadzisz)");
		}
		else if (!strcmp(sub, "rola"))
		{
			int role = -1;
			if (*a1)
				str_to_number(role, a1);
			if (role < PARTY_ROLE_NORMAL || role > PARTY_ROLE_DEFENDER || role == PARTY_ROLE_LEADER)
				SayPlayerBotSidekick(ch, "Uzyj: /towarzysz rola 0 (bez) 2 atak 3 maks. PZ 4 czas trwania 5 mistrz 6 szybkosc ataku 7 obrona");
			else
			{
				LPCHARACTER sk = CHARACTER_MANAGER::instance().FindByPID(rec->second.dwSidekickPID);
				const std::string text = SetPlayerBotSidekickRole(rec->second, (BYTE)role,
						sk ? sk->GetLeadershipSkillLevel() : 0);
				SayPlayerBotSidekick(ch, text.c_str());
				if (sk && rec->second.bLead && sk->GetParty() && sk->GetParty()->GetLeaderPID() == sk->GetPlayerID() &&
						ch->GetParty() == sk->GetParty())
					ApplyPlayerBotSidekickRole(sk, ch, rec->second, sk->GetParty());
			}
		}
		else if (!strcmp(sub, "skrzynki"))
		{
			if (!strcmp(a1, "0") || !strcmp(a1, "1"))
				SayPlayerBotSidekick(ch, SetPlayerBotSidekickChests(rec->second, !strcmp(a1, "1")));
			else
				SayPlayerBotSidekick(ch, "Uzyj: /towarzysz skrzynki 1 (sam otwieram skrzynie) albo /towarzysz skrzynki 0");
		}
		// MT2009_PLUS_SIDEKICK_COINS_V1: "Smocze Monety: wydaje / nie wydaje".
		else if (!strcmp(sub, "monety") || !strcmp(sub, "coins"))
		{
			if (!strcmp(a1, "0") || !strcmp(a1, "1") || !strcmp(a1, "on") || !strcmp(a1, "off"))
				SayPlayerBotSidekick(ch, SetPlayerBotSidekickCoins(rec->second, !strcmp(a1, "1") || !strcmp(a1, "on")));
			else
				SayPlayerBotSidekick(ch, "Uzyj: /towarzysz monety 1 (wydaje Smocze Monety w Item Shopie) albo "
						"/towarzysz monety 0 (nie wydaje)");
		}
		// MT2009_PLUS_SIDEKICK_EQUIP_LOCK_V1: "Zablokuj ekwipunek".
		else if (!strcmp(sub, "blokada"))
		{
			if (!strcmp(a1, "0") || !strcmp(a1, "1"))
				SayPlayerBotSidekick(ch, SetPlayerBotSidekickEquipLock(rec->second, !strcmp(a1, "1")));
			else
				SayPlayerBotSidekick(ch, "Uzyj: /towarzysz blokada 1 (nie ruszam ekwipunku) albo /towarzysz blokada 0");
		}
		// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: "Pelne EQ".
		else if (!strcmp(sub, "przechowuj"))
		{
			if (!strcmp(a1, "0") || !strcmp(a1, "1"))
				SayPlayerBotSidekick(ch, SetPlayerBotSidekickKeepLoot(rec->second, !strcmp(a1, "1")));
			else
				SayPlayerBotSidekick(ch, "Uzyj: /towarzysz przechowuj 1 (zbieram twoj drop, gdy masz pelny ekwipunek) "
						"albo /towarzysz przechowuj 0");
		}
		else if (!strcmp(sub, "grupa"))
		{
			if (!strcmp(a1, "0") || !strcmp(a1, "1"))
				SayPlayerBotSidekick(ch, SetPlayerBotSidekickParty(rec->second, !strcmp(a1, "1")));
			else
				SayPlayerBotSidekick(ch, "Uzyj: /towarzysz grupa 1 (dolaczam do twojej grupy, kto by jej nie prowadzil) "
						"albo /towarzysz grupa 0");
		}
		else if (!strcmp(sub, "zbieraj") || !strcmp(sub, "ochrona") || !strcmp(sub, "buffy"))
		{
			int value = -1;
			if (*a1)
				str_to_number(value, a1);
			TPlayerBotSidekick& r = rec->second;
			if (!strcmp(sub, "zbieraj") && value >= PLAYERBOT_SIDEKICK_LOOT_NONE && value <= PLAYERBOT_SIDEKICK_LOOT_ALL)
			{
				r.bLoot = (BYTE)value;
				SetPlayerBotSidekickSetting(r, "loot", (unsigned int)value);
				SayPlayerBotSidekick(ch, value == PLAYERBOT_SIDEKICK_LOOT_ALL ? "Zbieram caly drop: twoj i swoj." :
						value == PLAYERBOT_SIDEKICK_LOOT_OWNERS ? "Zbieram tylko twoj drop." : "Nie zbieram dropu.");
			}
			else if (!strcmp(sub, "ochrona") && (value == 0 || value == 1))
			{
				r.bProtect = value == 1;
				SetPlayerBotSidekickSetting(r, "protect", (unsigned int)value);
				SayPlayerBotSidekick(ch, r.bProtect ? "Gdy bedziesz ginac, sciagne na siebie potwory." :
						"Nie bede sciagac z ciebie potworow.");
			}
			else if (!strcmp(sub, "buffy") && (value == 0 || value == 1))
			{
				r.bBuffs = value == 1;
				SetPlayerBotSidekickSetting(r, "buffs", (unsigned int)value);
				SayPlayerBotSidekick(ch, r.bBuffs ? "Bede cie buffowac (jesli umiem)." : "Nie bede cie buffowac.");
			}
			else
				SayPlayerBotSidekick(ch, "Uzyj: /towarzysz zbieraj 0|1|2, /towarzysz ochrona 0|1, /towarzysz buffy 0|1");
		}
		else if (!strcmp(sub, "walka"))
		{
			int stance = -1;
			if (!strcmp(a1, "atakuj") || !strcmp(a1, "atak"))
				stance = PLAYERBOT_SIDEKICK_STANCE_ATTACK;
			else if (!strcmp(a1, "obrona") || !strcmp(a1, "bron"))
				stance = PLAYERBOT_SIDEKICK_STANCE_DEFEND;
			else if (!strcmp(a1, "spokoj") || !strcmp(a1, "nie"))
				stance = PLAYERBOT_SIDEKICK_STANCE_PASSIVE;
			else if (*a1)
				str_to_number(stance, a1);
			if (stance < PLAYERBOT_SIDEKICK_STANCE_ATTACK || stance > PLAYERBOT_SIDEKICK_STANCE_PASSIVE)
			{
				char text[192];
				snprintf(text, sizeof(text), "Teraz: %s. Zmien: /towarzysz walka atakuj | obrona | spokoj",
						GetPlayerBotSidekickStanceName(rec->second.bStance));
				SayPlayerBotSidekick(ch, text);
			}
			else
				SayPlayerBotSidekick(ch, SetPlayerBotSidekickStance(rec->second, (BYTE)stance));
		}
		else if (!strcmp(sub, "odprawa"))
		{
			if (!strcmp(a1, "tak"))
				DismissPlayerBotSidekick(ch, rec->second);
			else
				SayPlayerBotSidekick(ch, "Na pewno? Wpisz: /towarzysz odprawa tak");
		}
		else
			ReportPlayerBotSidekick(ch, rec->second);
	}

	// ------------------------------------------------------ at the owner's side

	// Every trade the owner opens is accepted once the owner has accepted it,
	// and anybody else's is closed at once. What the owner put in is the
	// owner's gift: never sold, never on a counter, never scrap
	// (IsPlayerBotJunkItem), and worn when the equipment pass finds it better -
	// the same judgement it makes of everything, so the two never take turns.
	bool HandlePlayerBotSidekickTrade(LPCHARACTER ch, TPlayerBotAIState& state, const TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt, DWORD dwNow)
	{
		CExchange* exchange = ch->GetExchange();
		if (!exchange)
		{
			if (!rt.bTrading)
				return false;
			rt.bTrading = false;
			int gifts = 0;
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetCell() != cell ||
						rt.setBagBeforeTrade.find(item->GetID()) != rt.setBagBeforeTrade.end())
					continue;
				if (rt.setGifts.insert(item->GetID()).second)
				{
					DBManager::instance().Query(
							"INSERT IGNORE INTO player.playerbot_sidekick_gift (item_id, sidekick_pid, given_at) "
							"VALUES (%u, %u, NOW())", item->GetID(), ch->GetPlayerID());
					++gifts;
				}
			}
			rt.setBagBeforeTrade.clear();
			if (gifts > 0)
			{
				SayPlayerBotSidekick(GetPlayerBotSidekickOwnerChar(rec.dwOwnerPID),
						"Dzieki! Zachowam to, a co lepsze od mojego, zaraz zaloze.");
				state.dwNextEquipmentCheckTime = 0;
				sys_log(0, "PLAYERBOT_SIDEKICK: gifts pid=%u name=%s items=%d", ch->GetPlayerID(), ch->GetName(),
						gifts);
			}
			return false;
		}
		CExchange* other = exchange->GetCompany();
		LPCHARACTER partner = other ? other->GetOwner() : NULL;
		if (!partner || partner->GetPlayerID() != rec.dwOwnerPID)
		{
			exchange->Cancel();
			return true;
		}
		if (!rt.bTrading)
		{
			rt.bTrading = true;
			rt.setBagBeforeTrade.clear();
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (item && item->GetCell() == cell)
					rt.setBagBeforeTrade.insert(item->GetID());
			}
		}
		if (ch->IsStateMove())
			ch->Stop();
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		state.dwLastMeaningfulActivityTime = dwNow;
		// Only after the owner: accepting first would close the trade on
		// whatever was in the window at that moment. The trade may end inside
		// this call, and the two exchanges with it.
		if (other->GetAcceptStatus() && !exchange->GetAcceptStatus())
			exchange->Accept(true);
		return true;
	}

	// What the companion fights: the owner's own target, then what is hitting
	// the owner, then what is hitting itself, then - with nothing of that - the
	// nearest monster within PLAYERBOT_SIDEKICK_HUNT_RANGE of the owner. A
	// monster only, and a stone only when the owner hits it: a person is the
	// owner's business, a duel above all - save one of another kingdom who
	// strikes the owner or the companion, answered before all of it
	// (MT2009_PLUS_SIDEKICK_DEFEND_V1, FindPlayerBotSidekickDefendFoe). A boss
	// is fought when it fights one of the two, never looked for.
	// The ranges are measured from a centre: the owner at its side, the spot
	// when it was told to wait ("Czekaj tutaj"), where the owner - when it
	// is on that map at all - may be far away.
	struct FPlayerBotSidekickFoes
	{
		LPCHARACTER self;
		LPCHARACTER owner;
		long centreX;
		long centreY;
		long mapIndex;
		LPCHARACTER onOwner;
		int onOwnerDist;
		LPCHARACTER onSelf;
		int onSelfDist;
		LPCHARACTER idle;
		int idleDist;

		FPlayerBotSidekickFoes(LPCHARACTER s, LPCHARACTER o, long x, long y, long map)
			: self(s), owner(o), centreX(x), centreY(y), mapIndex(map), onOwner(NULL), onOwnerDist(INT_MAX),
			  onSelf(NULL), onSelfDist(INT_MAX), idle(NULL), idleDist(INT_MAX)
		{
		}

		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER c = (LPCHARACTER)ent;
			if (c == self || c == owner || c->IsDead() || !c->IsMonster() || c->GetMapIndex() != mapIndex)
				return;
			const int fromOwner = DISTANCE_APPROX(c->GetX() - centreX, c->GetY() - centreY);
			if (fromOwner > PLAYERBOT_SIDEKICK_GUARD_RANGE)
				return;
			const int fromSelf = DISTANCE_APPROX(c->GetX() - self->GetX(), c->GetY() - self->GetY());
			if (owner && c->GetVictim() == owner)
			{
				if (fromOwner < onOwnerDist && battle_is_attackable(self, c))
				{
					onOwner = c;
					onOwnerDist = fromOwner;
				}
				return;
			}
			if (c->GetVictim() == self)
			{
				if (fromSelf < onSelfDist && battle_is_attackable(self, c))
				{
					onSelf = c;
					onSelfDist = fromSelf;
				}
				return;
			}
			if (fromOwner > PLAYERBOT_SIDEKICK_HUNT_RANGE || c->GetMobRank() >= MOB_RANK_BOSS)
				return;
			if (fromSelf < idleDist && battle_is_attackable(self, c))
			{
				idle = c;
				idleDist = fromSelf;
			}
		}
	};

	// A character the owner is at war with: its guild and the owner's are in a
	// guild war. The one kind of person the companion strikes on its own
	// initiative, and only the one its owner has in hand.
	bool IsPlayerBotSidekickWarFoe(LPCHARACTER owner, LPCHARACTER target)
	{
		if (!owner || !target || !target->IsPC() || !owner->GetGuild() || !target->GetGuild())
			return false;
		return owner->GetGuild() != target->GetGuild() && owner->GetGuild()->UnderWar(target->GetGuild()->GetID());
	}

	// ------------------------------------------- MT2009_PLUS_SIDEKICK_DEFEND_V1
	//
	// "Gdy gracza atakuja boty z krolestwa przeciwnego, towarzysz pomaga w
	// obronie i ich atakuje rowniez" (the owner, 3 October). A character of
	// another kingdom - a bot or a person, one rule for both - that struck the
	// owner or the companion lately, or a bot of another kingdom in a fight
	// with either of them now, is the companion's foe before any monster: it
	// fights it with its usual fight (skills, potions, buffs) while the threat
	// lasts, and goes back to its owner and the monsters after it.
	//
	// An answer only, never a first blow; never the owner's own kingdom, party
	// or guild, whoever struck first; never one in a duel or a guild war with
	// the owner (that is the owner's business, and the war's foe the owner has
	// in hand is fought above); never a person under a truce with the bots;
	// and only where the engine lets the blow land (battle_is_attackable: the
	// safe zones, PK protection on a kingdom's own ground). A blow at another
	// kingdom puts nobody in killer mode (CPVPManager::CanAttack) and a kill
	// there costs no alignment (CHARACTER::Dead's empire branch).
	//
	// The blows come from CHARACTER::Damage through the manager
	// (CPlayerBotManager::OnPlayerStruck, NotePlayerBotStruck - a bot's blow
	// only when it was meant): at the companion always, at the owner while the
	// owner is in a party or a guild, which at its side the owner is (the
	// companion's party). The aim of another kingdom's bots (their AI state)
	// covers the rest.
	const DWORD PLAYERBOT_SIDEKICK_DEFEND_MEMORY_MS = 12000;
	const size_t PLAYERBOT_SIDEKICK_DEFEND_MAX = 8;
	const DWORD PLAYERBOT_SIDEKICK_DEFEND_LOG_MS = 15000;
	const DWORD PLAYERBOT_SIDEKICK_DEFEND_LOG_SWITCH_MS = 3000;

	struct TPlayerBotSidekickDefendBlow
	{
		DWORD dwVID;
		DWORD dwAt;
	};
	// The struck (an owner or a companion) by pid -> the attacker's pid -> its
	// last blow.
	std::map<DWORD, std::map<DWORD, TPlayerBotSidekickDefendBlow> > s_mapPlayerBotSidekickDefendBlows;
	// A companion's pid -> the attacker it last said it fought, and when.
	std::map<DWORD, std::pair<DWORD, DWORD> > s_mapPlayerBotSidekickDefendLogged;

	bool IsPlayerBotSidekickDefendBlowFresh(const TPlayerBotSidekickDefendBlow& blow, DWORD dwNow)
	{
		return blow.dwAt >= dwNow || dwNow - blow.dwAt <= PLAYERBOT_SIDEKICK_DEFEND_MEMORY_MS;
	}

	void PrunePlayerBotSidekickDefendBlows(std::map<DWORD, TPlayerBotSidekickDefendBlow>& blows, DWORD dwNow)
	{
		for (std::map<DWORD, TPlayerBotSidekickDefendBlow>::iterator it = blows.begin(); it != blows.end();)
		{
			if (!IsPlayerBotSidekickDefendBlowFresh(it->second, dwNow))
				blows.erase(it++);
			else
				++it;
		}
	}

	// From NotePlayerBotStruck (playerbot_anti_pk.h), for a blow that counts:
	// kept when it lands on an owner or a companion and comes from another
	// kingdom.
	void NotePlayerBotSidekickDefendBlow(LPCHARACTER victim, LPCHARACTER attacker, DWORD dwNow)
	{
		if (!victim || !attacker || victim == attacker || s_mapPlayerBotSidekicks.empty() || !attacker->IsPC() ||
				attacker->GetEmpire() == victim->GetEmpire())
			return;
		const DWORD pid = victim->GetPlayerID();
		if (s_mapPlayerBotSidekicks.find(pid) == s_mapPlayerBotSidekicks.end() &&
				s_mapPlayerBotSidekickOwner.find(pid) == s_mapPlayerBotSidekickOwner.end())
			return;
		if (s_mapPlayerBotSidekickDefendBlows.size() >= 256)
			for (std::map<DWORD, std::map<DWORD, TPlayerBotSidekickDefendBlow> >::iterator it =
					s_mapPlayerBotSidekickDefendBlows.begin(); it != s_mapPlayerBotSidekickDefendBlows.end();)
			{
				PrunePlayerBotSidekickDefendBlows(it->second, dwNow);
				if (it->second.empty())
					s_mapPlayerBotSidekickDefendBlows.erase(it++);
				else
					++it;
			}
		std::map<DWORD, TPlayerBotSidekickDefendBlow>& blows = s_mapPlayerBotSidekickDefendBlows[pid];
		PrunePlayerBotSidekickDefendBlows(blows, dwNow);
		if (blows.size() >= PLAYERBOT_SIDEKICK_DEFEND_MAX && blows.find(attacker->GetPlayerID()) == blows.end())
			return;
		TPlayerBotSidekickDefendBlow& blow = blows[attacker->GetPlayerID()];
		blow.dwVID = (DWORD)attacker->GetVID();
		blow.dwAt = dwNow;
	}

	// Who struck this character lately, still in the world under the same vid.
	void CollectPlayerBotSidekickDefendBlows(DWORD pid, DWORD dwNow, std::vector<LPCHARACTER>& out)
	{
		std::map<DWORD, std::map<DWORD, TPlayerBotSidekickDefendBlow> >::iterator v =
				s_mapPlayerBotSidekickDefendBlows.find(pid);
		if (v == s_mapPlayerBotSidekickDefendBlows.end())
			return;
		PrunePlayerBotSidekickDefendBlows(v->second, dwNow);
		if (v->second.empty())
		{
			s_mapPlayerBotSidekickDefendBlows.erase(v);
			return;
		}
		for (std::map<DWORD, TPlayerBotSidekickDefendBlow>::const_iterator it = v->second.begin();
				it != v->second.end(); ++it)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().Find(it->second.dwVID);
			if (c && c->IsPC() && c->GetPlayerID() == it->first)
				out.push_back(c);
		}
	}

	// Another kingdom's bots round the companion in a fight with the owner or
	// with the companion now: their own foe (the Anti-PK protocol's), or the
	// target of a fight under way.
	struct FPlayerBotSidekickDefendScan
	{
		LPCHARACTER self;
		LPCHARACTER owner;
		std::vector<LPCHARACTER> onOwner;
		std::vector<LPCHARACTER> onSelf;

		FPlayerBotSidekickDefendScan(LPCHARACTER s, LPCHARACTER o) : self(s), owner(o)
		{
		}

		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER) ||
					onOwner.size() + onSelf.size() >= PLAYERBOT_SIDEKICK_DEFEND_MAX * 2)
				return;
			LPCHARACTER c = (LPCHARACTER)ent;
			if (c == self || c == owner || !c->IsPC() || c->IsDead() || !c->GetDesc() || !c->GetDesc()->IsBot() ||
					c->GetEmpire() == self->GetEmpire())
				return;
			TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(c->GetPlayerID());
			if (it == s_mapPlayerBotAIStates.end())
				return;
			const TPlayerBotAIState& st = it->second;
			const bool fighting = st.bCurrentAction == BOT_ACTION_FIGHT;
			if (owner && (st.persona.dwFoeVID == (DWORD)owner->GetVID() ||
					(fighting && st.dwTargetVID == (DWORD)owner->GetVID())))
				onOwner.push_back(c);
			else if (st.persona.dwFoeVID == (DWORD)self->GetVID() ||
					(fighting && st.dwTargetVID == (DWORD)self->GetVID()))
				onSelf.push_back(c);
		}
	};

	// One the companion may answer now. centreX/Y: the owner at its side, the
	// spot it keeps ("Czekaj tutaj").
	bool IsPlayerBotSidekickDefendFoe(LPCHARACTER ch, LPCHARACTER owner, LPCHARACTER foe, long centreX, long centreY)
	{
		if (!foe || foe == ch || foe == owner || !foe->IsPC() || foe->IsDead() || foe->IsObserverMode() ||
				foe->GetMapIndex() != ch->GetMapIndex() || !foe->GetSectree())
			return false;
		// Another kingdom only: the owner's own - the companion's - never.
		if (foe->GetEmpire() == ch->GetEmpire() || (owner && foe->GetEmpire() == owner->GetEmpire()))
			return false;
		if (owner)
		{
			if ((owner->GetParty() && foe->GetParty() == owner->GetParty()) ||
					(owner->GetGuild() && foe->GetGuild() == owner->GetGuild()) ||
					IsPlayerBotBlowConsensual(owner, foe))
				return false;
		}
		if ((ch->GetParty() && foe->GetParty() == ch->GetParty()) ||
				(ch->GetGuild() && foe->GetGuild() == ch->GetGuild()) || IsPlayerBotBlowConsensual(ch, foe))
			return false;
		if (IsPlayerBotWarFoeRecovering(foe) || IsPlayerBotPersonTruced(foe, get_dword_time()))
			return false;
		if (DISTANCE_APPROX(foe->GetX() - centreX, foe->GetY() - centreY) > PLAYERBOT_SIDEKICK_ASSIST_RANGE)
			return false;
		if (IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()) ||
				IsPlayerBotSafeZone(foe->GetMapIndex(), foe->GetX(), foe->GetY()))
			return false;
		// The engine's own word on the blow. A companion on a mount is refused
		// every blow by it (CPVPManager::CanAttack) until the fight takes it
		// down - a character is fought on foot (CanPlayerBotFightOnHorse) - so
		// on one the foe's blow at it is asked: between two kingdoms the rules
		// are the same both ways.
		if (battle_is_attackable(ch, foe))
			return true;
		return ch->IsRiding() && battle_is_attackable(foe, ch);
	}

	// The one the companion keeps fighting (its victim) while it still may,
	// else the nearest of the owner's attackers, else the nearest of its own.
	// Passive ("nie walcz") answers only its own. why: AT_OWNER or AT_SELF.
	LPCHARACTER FindPlayerBotSidekickDefendFoe(LPCHARACTER ch, LPCHARACTER owner, BYTE stance, long centreX,
			long centreY, int& why)
	{
		if (!ch || !ch->GetSectree())
			return NULL;
		const DWORD dwNow = get_dword_time();
		const bool guardOwner = owner && stance != PLAYERBOT_SIDEKICK_STANCE_PASSIVE && !owner->IsDead() &&
				owner->GetMapIndex() == ch->GetMapIndex() &&
				DISTANCE_APPROX(owner->GetX() - centreX, owner->GetY() - centreY) <= PLAYERBOT_SIDEKICK_GUARD_RANGE;
		FPlayerBotSidekickDefendScan scan(ch, guardOwner ? owner : NULL);
		ch->GetSectree()->ForEachAround(scan);
		if (guardOwner)
			CollectPlayerBotSidekickDefendBlows(owner->GetPlayerID(), dwNow, scan.onOwner);
		CollectPlayerBotSidekickDefendBlows(ch->GetPlayerID(), dwNow, scan.onSelf);
		if (scan.onOwner.empty() && scan.onSelf.empty())
			return NULL;
		LPCHARACTER current = ch->GetVictim();
		const std::vector<LPCHARACTER>* lists[2] = { &scan.onOwner, &scan.onSelf };
		const int whys[2] = { PLAYERBOT_SIDEKICK_FOE_AT_OWNER, PLAYERBOT_SIDEKICK_FOE_AT_SELF };
		if (current)
			for (int l = 0; l < 2; ++l)
				for (size_t i = 0; i < lists[l]->size(); ++i)
					if ((*lists[l])[i] == current && IsPlayerBotSidekickDefendFoe(ch, owner, current, centreX, centreY))
					{
						why = whys[l];
						return current;
					}
		for (int l = 0; l < 2; ++l)
		{
			LPCHARACTER best = NULL;
			int bestDist = INT_MAX;
			for (size_t i = 0; i < lists[l]->size(); ++i)
			{
				LPCHARACTER c = (*lists[l])[i];
				const int d = DISTANCE_APPROX(c->GetX() - ch->GetX(), c->GetY() - ch->GetY());
				if (d < bestDist && IsPlayerBotSidekickDefendFoe(ch, owner, c, centreX, centreY))
				{
					best = c;
					bestDist = d;
				}
			}
			if (best)
			{
				why = whys[l];
				return best;
			}
		}
		return NULL;
	}

	// Said once per attacker, again only after PLAYERBOT_SIDEKICK_DEFEND_LOG_MS.
	void LogPlayerBotSidekickDefend(LPCHARACTER ch, LPCHARACTER owner, LPCHARACTER foe, int why, DWORD dwNow)
	{
		std::pair<DWORD, DWORD>& last = s_mapPlayerBotSidekickDefendLogged[ch->GetPlayerID()];
		const DWORD since = dwNow - last.second;
		if (last.second != 0 && (last.first == foe->GetPlayerID() ? since < PLAYERBOT_SIDEKICK_DEFEND_LOG_MS
				: since < PLAYERBOT_SIDEKICK_DEFEND_LOG_SWITCH_MS))
			return;
		last.first = foe->GetPlayerID();
		last.second = dwNow;
		sys_log(0, "PLAYERBOT_SIDEKICK: defends %s pid=%u name=%s owner=%u attacker_pid=%u attacker=%s "
				"attacker_empire=%u person=%d map=%ld hp=%d/%d",
				why == PLAYERBOT_SIDEKICK_FOE_AT_OWNER ? "owner" : "itself", ch->GetPlayerID(), ch->GetName(),
				owner ? owner->GetPlayerID() : 0, foe->GetPlayerID(), foe->GetName(), (unsigned int)foe->GetEmpire(),
				foe->GetDesc() && !foe->GetDesc()->IsBot() ? 1 : 0, ch->GetMapIndex(), ch->GetHP(), ch->GetMaxHP());
	}

	// What is hitting a losing owner turns on the companion.
	struct FPlayerBotSidekickTaunt
	{
		LPCHARACTER self;
		LPCHARACTER owner;
		int taken;

		FPlayerBotSidekickTaunt(LPCHARACTER s, LPCHARACTER o) : self(s), owner(o), taken(0)
		{
		}

		void operator()(LPENTITY ent)
		{
			if (taken >= PLAYERBOT_SIDEKICK_PROTECT_MAX_MONSTERS || !ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER monster = (LPCHARACTER)ent;
			if (monster->IsDead() || !monster->IsMonster() || monster->GetVictim() != owner ||
					monster->GetMapIndex() != owner->GetMapIndex() ||
					DISTANCE_APPROX(monster->GetX() - owner->GetX(), monster->GetY() - owner->GetY()) >
							PLAYERBOT_SIDEKICK_GUARD_RANGE ||
					!battle_is_attackable(monster, self))
				return;
			monster->UpdateAggrPoint(self, DAMAGE_TYPE_SPECIAL, monster->GetMaxHP());
			monster->SetVictim(self);
			++taken;
		}
	};

	void ProtectPlayerBotSidekickOwner(LPCHARACTER ch, LPCHARACTER owner, TPlayerBotSidekickRuntime& rt, DWORD dwNow)
	{
		if (dwNow < rt.dwNextProtect || owner->IsDead() || owner->GetMaxHP() <= 0 || ch->GetMaxHP() <= 0 ||
				(long long)owner->GetHP() * 100 >= (long long)owner->GetMaxHP() * PLAYERBOT_SIDEKICK_PROTECT_HP_PERCENT ||
				(long long)ch->GetHP() * 100 < (long long)ch->GetMaxHP() * PLAYERBOT_SIDEKICK_PROTECT_SELF_HP_PERCENT ||
				!owner->GetSectree())
			return;
		rt.dwNextProtect = dwNow + PLAYERBOT_SIDEKICK_PROTECT_INTERVAL_MS;
		FPlayerBotSidekickTaunt taunt(ch, owner);
		owner->GetSectree()->ForEachAround(taunt);
		if (taunt.taken > 0)
			PlayerBotLogThrottled("sidekick_protect", dwNow,
					"PLAYERBOT_SIDEKICK: took the owner's attackers pid=%u name=%s owner=%u owner_hp=%d/%d taken=%d",
					ch->GetPlayerID(), ch->GetName(), owner->GetPlayerID(), owner->GetHP(), owner->GetMaxHP(),
					taunt.taken);
	}

	// The owner's target is a fight already: a monster at the owner, at the
	// companion or at somebody of the owner's party, a stone somebody has begun
	// (a stone takes no victim), a guild war's foe. Merely clicked on, it is
	// not - which is what "nie atakuj pierwszy" asks.
	bool IsPlayerBotSidekickFightUnderWay(LPCHARACTER ch, LPCHARACTER owner, LPCHARACTER target)
	{
		if (IsPlayerBotSidekickWarFoe(owner, target))
			return true;
		if (target->IsStone())
			return target->GetHP() < target->GetMaxHP();
		LPCHARACTER victim = target->GetVictim();
		return victim && (victim == owner || victim == ch ||
				(owner->GetParty() && victim->GetParty() == owner->GetParty()));
	}

	// The owner's target is a fight under way, whatever the stance says the
	// companion may do about it.
	bool IsPlayerBotSidekickOwnerTargetInFight(LPCHARACTER ch, LPCHARACTER owner)
	{
		LPCHARACTER target = owner->GetTarget();
		return target && target != ch && !target->IsDead() && target->GetMapIndex() == owner->GetMapIndex() &&
				(target->IsMonster() || target->IsStone() || IsPlayerBotSidekickWarFoe(owner, target)) &&
				IsPlayerBotSidekickFightUnderWay(ch, owner, target);
	}

	// The owner was seen in a fight within PLAYERBOT_BUFF_COMBAT_WINDOW - the
	// window a bot keeps its own fight buffs up for after its last blow. Its
	// buffs then come before the loot and the walk after it.
	bool IsPlayerBotSidekickOwnerFighting(const TPlayerBotSidekickRuntime& rt, DWORD dwNow)
	{
		return rt.dwOwnerFightSeenAt != 0 && dwNow >= rt.dwOwnerFightSeenAt &&
				dwNow - rt.dwOwnerFightSeenAt < PLAYERBOT_BUFF_COMBAT_WINDOW;
	}

	// ownerFighting: the owner's target is a fight under way or a monster is
	// at the owner - told in every stance, "nie walcz" included, because beside
	// the owner's fight the owner's buffs come first.
	LPCHARACTER FindPlayerBotSidekickFoe(LPCHARACTER ch, LPCHARACTER owner, BYTE stance, int& why,
			bool& ownerFighting)
	{
		ownerFighting = IsPlayerBotSidekickOwnerTargetInFight(ch, owner);
		// MT2009_PLUS_SIDEKICK_DEFEND_V1: another kingdom's attackers come
		// before the owner's target and every monster.
		LPCHARACTER defend = FindPlayerBotSidekickDefendFoe(ch, owner, stance, owner->GetX(), owner->GetY(), why);
		if (defend)
		{
			if (why == PLAYERBOT_SIDEKICK_FOE_AT_OWNER)
				ownerFighting = true;
			LogPlayerBotSidekickDefend(ch, owner, defend, why, get_dword_time());
			return defend;
		}
		LPCHARACTER target = stance == PLAYERBOT_SIDEKICK_STANCE_PASSIVE ? NULL : owner->GetTarget();
		// MT2009_PLUS_AREZZO_BOTS_V1 (events): the owner's Easter metin is the owner's.
		if (target && target->IsStone() && IsPlayerBotEventStone(target->GetRaceNum()))
			target = NULL;
		if (target && target != ch && !target->IsDead() &&
				(target->IsMonster() || target->IsStone() || IsPlayerBotSidekickWarFoe(owner, target)) &&
				target->GetMapIndex() == owner->GetMapIndex() &&
				DISTANCE_APPROX(target->GetX() - owner->GetX(), target->GetY() - owner->GetY()) <=
						PLAYERBOT_SIDEKICK_ASSIST_RANGE &&
				(stance == PLAYERBOT_SIDEKICK_STANCE_ATTACK || IsPlayerBotSidekickFightUnderWay(ch, owner, target)) &&
				battle_is_attackable(ch, target))
		{
			why = PLAYERBOT_SIDEKICK_FOE_OWNER_TARGET;
			return target;
		}
		if (!owner->GetSectree())
			return NULL;
		FPlayerBotSidekickFoes foes(ch, owner, owner->GetX(), owner->GetY(), owner->GetMapIndex());
		owner->GetSectree()->ForEachAround(foes);
		if (foes.onOwner)
			ownerFighting = true;
		if (foes.onOwner && stance != PLAYERBOT_SIDEKICK_STANCE_PASSIVE)
		{
			why = PLAYERBOT_SIDEKICK_FOE_AT_OWNER;
			return foes.onOwner;
		}
		if (foes.onSelf)
		{
			why = PLAYERBOT_SIDEKICK_FOE_AT_SELF;
			return foes.onSelf;
		}
		if (foes.idle && stance == PLAYERBOT_SIDEKICK_STANCE_ATTACK)
		{
			why = PLAYERBOT_SIDEKICK_FOE_NEARBY;
			return foes.idle;
		}
		return NULL;
	}

	// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: whether the owner's own pick-up
	// would find this drop a place - the engine's party branch of PickupItem
	// pours it onto a stack of the same thing first (AutoStackItem: the same
	// vnum and sockets, room left) and puts the rest on an empty cell
	// (GetEmptyInventoryEx, which knows the special pages).
	bool PlayerBotSidekickOwnerTakesDrop(LPCHARACTER owner, LPITEM item)
	{
		if (!owner || !item)
			return false;
		if (IsPlayerBotMoneyDrop(item))
			return true;
#if defined(PLAYERBOT_ENGINE_MT2009)
		if (owner->GetEmptyInventoryEx(item) != -1)
			return true;
		if (!item->IsStackable() || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
			return false;
		for (int i = 0; i < owner->GetInventoryMaxCount(); ++i)
		{
			LPITEM stack = owner->GetInventoryItem(i);
			if (!stack || stack->GetVnum() != item->GetVnum() || stack->GetCount() >= stack->GetMaxStack())
				continue;
			int j = 0;
			for (; j < ITEM_SOCKET_MAX_NUM; ++j)
				if (stack->GetSocket(j) != item->GetSocket(j))
					break;
			if (j == ITEM_SOCKET_MAX_NUM)
				return true;
		}
		return false;
#else
		return PlayerBotBagTakesDrop(owner, item);
#endif
	}

	// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: "jak u nas sie zapelni EQ" - an
	// owner's drop the owner's bag has no place for, which the companion
	// picks up into its own bag and holds for the owner (AddPlayerBotSidekickHeld)
	// until the owner takes it in the window (a right click in its bag). Not
	// what the AI spends by the item's number, which no mark of a piece can
	// stop: a Cor Draconis (the alchemy opens every one), a skill book (read by
	// its number); nor a dragon stone (the window cannot hand one back) or a
	// quest item (the owner's quest hears of its pick-up only in the owner's
	// own bag). Yang never needs a place.
	bool CanPlayerBotSidekickHoldOwnerDrop(LPCHARACTER sk, LPITEM item)
	{
		if (!sk || !item || IsPlayerBotMoneyDrop(item) || item->IsDragonSoul() || item->GetType() == ITEM_QUEST ||
				item->GetType() == ITEM_SKILLBOOK || item->GetType() == ITEM_SKILLFORGET ||
				IsPlayerBotGeneralSkillBook(item->GetVnum()) || IsPlayerBotExtraSkillBook(item->GetVnum()) ||
				IsPlayerBotCorDraconisVnum(item->GetVnum()))
			return false;
		return sk->GetEmptyInventory(item->GetSize()) >= 0;
	}

	// The drops worth going for: the owner's, which the engine hands to the
	// owner when a party member picks them up (CHARACTER::PickupItem, the party
	// branch), and what is the companion's own to take.
	struct FPlayerBotSidekickLoot
	{
		LPCHARACTER self;
		LPCHARACTER owner;
		const TPlayerBotSidekickRuntime& rt;
		DWORD now;
		BYTE mode;	// EPlayerBotSidekickLoot
		LPITEM best;
		int bestDist;
		// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: "Pelne EQ" on, and whether
		// the drop chosen is one to hold for the owner.
		bool keep;
		bool bestHeld;
		// MT2009_PLUS_SIDEKICK_FOLLOW_LOOT_V1: the leash's centre - the owner,
		// or the spot it keeps.
		long centreX;
		long centreY;

		FPlayerBotSidekickLoot(LPCHARACTER s, LPCHARACTER o, const TPlayerBotSidekickRuntime& r, DWORD n, BYTE m,
				long cx, long cy)
			: self(s), owner(o), rt(r), now(n), mode(m), best(NULL), bestDist(INT_MAX),
			  keep(s && IsPlayerBotSidekickKeepingLoot(s->GetPlayerID())), bestHeld(false), centreX(cx), centreY(cy)
		{
		}

		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_ITEM))
				return;
			LPITEM item = (LPITEM)ent;
			if (!item->GetSectree() || item->GetOwner())
				return;
			const int d = DISTANCE_APPROX(item->GetX() - self->GetX(), item->GetY() - self->GetY());
			if (d >= bestDist)
				return;
			// MT2009_PLUS_SIDEKICK_FOLLOW_LOOT_V1: within the leash round the
			// owner rather than twelve metres round itself - a drop of its own
			// kill on the far side of the owner was out of its reach.
			if (DISTANCE_APPROX(item->GetX() - centreX, item->GetY() - centreY) > PLAYERBOT_SIDEKICK_LOOT_LEASH)
				return;
			std::map<DWORD, DWORD>::const_iterator failed = rt.mapLootFailed.find(item->GetVID());
			if (failed != rt.mapLootFailed.end() && (int)(now - failed->second) < 0)
				return;
			// MT2009_PLUS_PICKUP_FILTER_V1 (bots): what the owner does not pick
			// up (Ctrl+Z), its companion leaves too - the owner's drops and its
			// own alike.
			if (!PlayerBotRecipientWantsDrop(owner ? owner : GetPlayerBotSidekickFilterOwner(self), item))
				return;
			const bool ownersOnly = owner && item->IsOwnership(owner) && !item->IsOwnership(self);
			bool held = false;
			// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: the owner's drop the
			// owner's bag cannot take is held for the owner ("Pelne EQ"), or
			// left alone - the party branch would refuse it anyway, and the
			// companion ran to it and back for eight seconds.
			if (ownersOnly && !PlayerBotSidekickOwnerTakesDrop(owner, item))
			{
				if (!keep || !CanPlayerBotSidekickHoldOwnerDrop(self, item))
					return;
				held = true;
			}
			else if (ownersOnly)
			{
				// The party branch hands over only what may change hands - and
				// not the owner's yang, which it would put into the owner's bag
				// as an item worth nothing (IsPlayerBotPartyLoot).
				if (IsPlayerBotMoneyDrop(item) ||
						IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_DROP) ||
						self->GetParty() == NULL || self->GetParty() != owner->GetParty())
					return;
			}
			// "Tylko moj" in the window: the owner's drops, none of its own.
			else if (mode != PLAYERBOT_SIDEKICK_LOOT_ALL || !item->IsOwnership(self) ||
					IsPlayerBotLootBeneathBot(self, item) || !PlayerBotBagTakesDrop(self, item))
				return;
			best = item;
			bestDist = d;
			bestHeld = held;
		}
	};

	// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: the owner's drop off the ground
	// and into the companion's bag, held for the owner; the owner is told once
	// a minute at most.
	bool HoldPlayerBotSidekickOwnerDrop(LPCHARACTER ch, LPCHARACTER owner, TPlayerBotSidekickRuntime& rt, LPITEM item,
			DWORD dwNow)
	{
		if (!item->GetSectree() || !CanPlayerBotSidekickHoldOwnerDrop(ch, item) || !ch->CanHandleItem())
			return false;
		const int cell = ch->GetEmptyInventory(item->GetSize());
		if (cell < 0)
			return false;
		item->RemoveFromGround();
		item->AddToCharacter(ch, TItemPos(INVENTORY, cell));
		ITEM_MANAGER::instance().FlushDelayedSave(item);
		AddPlayerBotSidekickHeld(ch->GetPlayerID(), rt, item->GetID());
		char hint[64];
		snprintf(hint, sizeof(hint), "%s %u %u", item->GetName(), (unsigned int)item->GetCount(), item->GetOriginalVnum());
		LogManager::instance().ItemLog(ch, item, "PLAYERBOT_SIDEKICK_HOLD", owner ? owner->GetName() : hint);
		sys_log(0, "PLAYERBOT_SIDEKICK: held for the owner pid=%u owner=%u item=%u vnum=%u count=%u cell=%d",
				ch->GetPlayerID(), owner ? owner->GetPlayerID() : 0, item->GetID(), item->GetVnum(),
				(unsigned int)item->GetCount(), cell);
		if (owner && (rt.dwHeldToldAt == 0 || dwNow - rt.dwHeldToldAt >= PLAYERBOT_SIDEKICK_HELD_TELL_MS))
		{
			rt.dwHeldToldAt = dwNow;
			char text[192];
			snprintf(text, sizeof(text), "Masz pelny ekwipunek - %s trzymam dla ciebie w swoim plecaku "
					"(okno Towarzysza, prawy klik oddaje).", item->GetName());
			SayPlayerBotSidekick(owner, text);
		}
		return true;
	}

	// MT2009_PLUS_SIDEKICK_FOLLOW_LOOT_V1: walk false is the Z of a fight -
	// what lies in reach is taken where it stands, nothing is walked to, and
	// the walk under way (rt.dwLootVID) is left as it was.
	bool PickUpPlayerBotSidekickLoot(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER owner,
			TPlayerBotSidekickRuntime& rt, BYTE lootMode, long centreX, long centreY, bool walk, DWORD dwNow)
	{
		if (!ch->GetSectree() || lootMode == PLAYERBOT_SIDEKICK_LOOT_NONE)
			return false;
		if (rt.mapLootFailed.size() > 64)
		{
			for (std::map<DWORD, DWORD>::iterator it = rt.mapLootFailed.begin(); it != rt.mapLootFailed.end(); )
			{
				if ((int)(dwNow - it->second) >= 0)
					rt.mapLootFailed.erase(it++);
				else
					++it;
			}
		}
		FPlayerBotSidekickLoot loot(ch, owner, rt, dwNow, lootMode, centreX, centreY);
		ch->GetSectree()->ForEachAround(loot);
		if (!walk)
		{
			if (!loot.best || loot.bestDist > PLAYERBOT_SIDEKICK_PICKUP_RANGE)
				return false;
			const DWORD zvid = loot.best->GetVID();
			if (loot.bestHeld)
			{
				if (!HoldPlayerBotSidekickOwnerDrop(ch, owner, rt, loot.best, dwNow))
					rt.mapLootFailed[zvid] = dwNow + PLAYERBOT_SIDEKICK_LOOT_FAILED_MS;
				return true;
			}
			const DWORD zvnum = loot.best->GetVnum();
			if (!ch->PickupItem(zvid))
				return false;
			sys_log(0, "PLAYERBOT_SIDEKICK: picked up pid=%u vid=%u vnum=%u how=fight", ch->GetPlayerID(), zvid, zvnum);
			return true;
		}
		if (!loot.best)
		{
			rt.dwLootVID = 0;
			return false;
		}
		const DWORD vid = loot.best->GetVID();
		// The same drop for longer than a walk to it takes: something refuses
		// it (the owner's bag, a pick-up clock), and it is left alone a while.
		if (rt.dwLootVID != vid)
		{
			rt.dwLootVID = vid;
			rt.dwLootSince = dwNow;
		}
		else if (dwNow - rt.dwLootSince > PLAYERBOT_SIDEKICK_LOOT_GIVE_UP_MS)
		{
			rt.mapLootFailed[vid] = dwNow + PLAYERBOT_SIDEKICK_LOOT_FAILED_MS;
			rt.dwLootVID = 0;
			return false;
		}
		SetPlayerBotAction(state, BOT_ACTION_LOOT, dwNow);
		state.dwLastMeaningfulActivityTime = dwNow;
		if (loot.bestDist > PLAYERBOT_SIDEKICK_PICKUP_RANGE)
		{
			WalkPlayerBotSidekick(ch, loot.best->GetX(), loot.best->GetY(), dwNow, false);
			return true;
		}
		if (ch->IsStateMove())
			ch->Stop();
		// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: the owner's drop the owner's
		// bag cannot take goes into the companion's.
		if (loot.bestHeld)
		{
			if (!HoldPlayerBotSidekickOwnerDrop(ch, owner, rt, loot.best, dwNow))
			{
				rt.mapLootFailed[vid] = dwNow + PLAYERBOT_SIDEKICK_LOOT_FAILED_MS;
				rt.dwLootVID = 0;
			}
			return true;
		}
		const DWORD vnum = loot.best->GetVnum();
		if (ch->PickupItem(vid))
			sys_log(0, "PLAYERBOT_SIDEKICK: picked up pid=%u vid=%u vnum=%u how=walk from_centre=%d", ch->GetPlayerID(),
					vid, vnum, DISTANCE_APPROX(ch->GetX() - centreX, ch->GetY() - centreY));
		return true;
	}

	// MT2009_PLUS_SIDEKICK_FOLLOW_LOOT_V1: the loot pass at the owner's side
	// and at a spot it keeps. It looked every 400 ms, and the ticks between
	// went on to the follow below it - a walk back to the owner past four and
	// a half metres, a stop under two - so the companion took a step to a drop
	// and a step back, gave the drop up after eight seconds and left it for
	// thirty ("Przywolaj" did not pick up, "Wolna reka", the ordinary loot pass
	// of every tick, did). A walk to a drop now goes on on every tick until it
	// is picked up or given up; only the search for a new one keeps the clock.
	bool RunPlayerBotSidekickLootPass(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER owner,
			TPlayerBotSidekickRuntime& rt, BYTE lootMode, long centreX, long centreY, DWORD dwNow)
	{
		if (rt.dwLootVID == 0 && dwNow < rt.dwNextLoot)
			return false;
		rt.dwNextLoot = dwNow + PLAYERBOT_SIDEKICK_LOOT_INTERVAL_MS;
		return PickUpPlayerBotSidekickLoot(ch, state, owner, rt, lootMode, centreX, centreY, true, dwNow);
	}

	// MT2009_PLUS_SIDEKICK_FOLLOW_LOOT_V1: in a fight, what lies at its feet
	// (the Z a player presses between blows); the walk to a drop it had begun
	// starts afresh after the fight, or the eight seconds ran out in the fight
	// and the drop was given up the moment it was looked at again.
	void TakePlayerBotSidekickLootInFight(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER owner,
			TPlayerBotSidekickRuntime& rt, BYTE lootMode, long centreX, long centreY, DWORD dwNow)
	{
		rt.dwLootVID = 0;
		if (dwNow < rt.dwNextLoot)
			return;
		rt.dwNextLoot = dwNow + PLAYERBOT_SIDEKICK_LOOT_INTERVAL_MS;
		PickUpPlayerBotSidekickLoot(ch, state, owner, rt, lootMode, centreX, centreY, false, dwNow);
	}

	// The blacksmith and the merchants where the owner stands: the refine pass,
	// the junk sold and the merchants' shopping, as a town visit would do them -
	// "najlepiej wtedy, kiedy stoimy przy kowalu".
	struct FPlayerBotSidekickNpcs
	{
		LPCHARACTER owner;
		bool blacksmith;
		bool weapons;
		bool armour;
		bool misc;

		explicit FPlayerBotSidekickNpcs(LPCHARACTER o)
			: owner(o), blacksmith(false), weapons(false), armour(false), misc(false)
		{
		}

		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER c = (LPCHARACTER)ent;
			if (!c->IsNPC() || DISTANCE_APPROX(c->GetX() - owner->GetX(), c->GetY() - owner->GetY()) >
					PLAYERBOT_SIDEKICK_NPC_RANGE)
				return;
			switch (c->GetRaceNum())
			{
				case 20016: blacksmith = true; break;
				case 9001: weapons = true; break;
				case 9002: armour = true; break;
				case 9003: misc = true; break;
				default: break;
			}
		}
	};

	bool ServePlayerBotSidekickAtNpcs(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER owner, DWORD dwNow,
			TPlayerBotSidekickRuntime& rt)
	{
		if (!owner->GetSectree() || ch->GetExchange())
			return false;
		FPlayerBotSidekickNpcs npcs(owner);
		owner->GetSectree()->ForEachAround(npcs);
		if (!npcs.blacksmith && !npcs.weapons && !npcs.armour && !npcs.misc)
			return false;
		bool did = false;
		if (npcs.weapons)
			did = ManagePlayerBotWeaponMerchant(ch) || did;
		if (npcs.armour)
			did = ManagePlayerBotArmorMerchant(ch) || did;
		if (npcs.misc)
			did = ManagePlayerBotMiscMerchant(ch) || did;
		if (npcs.blacksmith)
		{
			state.dwNextRefineCheckTime = 0;
			const bool hadWeapon = ch->GetWear(WEAR_WEAPON) != NULL;
			const bool hadBody = ch->GetWear(WEAR_BODY) != NULL;
			const bool hadShield = ch->GetWear(WEAR_SHIELD) != NULL;
			did = ManagePlayerBotRefining(ch, state, dwNow) || did;
			// MT2009_PLUS_SIDEKICK_BONUS_V1: and its bonuses, as a bot's town
			// visit does them at the anvil (ManagePlayerBotBonusReroll): the
			// change and add stones and the marbles in its bag - what it found
			// and what its owner handed it - on its worn gear, the green ones
			// first, a piece being worked until its lines are good. It refined
			// at its owner's blacksmith and carried its stones for good.
			state.dwNextBonusCheckTime = 0;
			if (ManagePlayerBotBonusReroll(ch, state, dwNow))
			{
				did = true;
				sys_log(0, "PLAYERBOT_SIDEKICK: bonuses at the blacksmith pid=%u name=%s owner=%u",
						ch->GetPlayerID(), ch->GetName(), owner->GetPlayerID());
			}
			// MT2009_PLUS_BOSS_RAID_V2 (2.2.52, burn): a piece burnt at the
			// anvil is replaced on the same look when the owner stands by the
			// merchant too, not on the next service visit.
			// (A piece the anvil only took off is still in the bag.)
			if (npcs.weapons && hadWeapon && !PlayerBotHasPieceForSlot(ch, WEAR_WEAPON))
				ManagePlayerBotWeaponMerchant(ch);
			if (npcs.armour && ((hadBody && !PlayerBotHasPieceForSlot(ch, WEAR_BODY)) ||
					(hadShield && !PlayerBotHasPieceForSlot(ch, WEAR_SHIELD))))
				ManagePlayerBotArmorMerchant(ch);
		}
		if (did)
		{
			SetPlayerBotAction(state, npcs.blacksmith ? BOT_ACTION_REFINE : BOT_ACTION_SHOP, dwNow);
			state.dwLastMeaningfulActivityTime = dwNow;
			// MT2009_PLUS_SIDEKICK_SERVICE_LOG_V1: a line when the purse moved, or
			// every five minutes of the same.
			const long long gold = (long long)ch->GetGold();
			if (gold != rt.llLastServiceGold || dwNow - rt.dwLastServiceLog >= 300000)
			{
				rt.llLastServiceGold = gold;
				rt.dwLastServiceLog = dwNow;
				sys_log(0, "PLAYERBOT_SIDEKICK: served at the npcs pid=%u name=%s smith=%d weapons=%d armour=%d misc=%d gold=%lld",
						ch->GetPlayerID(), ch->GetName(), npcs.blacksmith ? 1 : 0, npcs.weapons ? 1 : 0,
						npcs.armour ? 1 : 0, npcs.misc ? 1 : 0, gold);
			}
		}
		return did;
	}

	// Staying alive at the owner's side: the potions, and after a death the
	// recovery the tick gives every bot - but standing up and healing beside
	// the owner rather than walking off from where it fell
	// (HandlePostDeathRecovery walks away from lDeathX/lDeathY). No breaking
	// off at a fifth of its health either: the owner is the one who retreats,
	// and the companion goes with the owner.
	bool KeepPlayerBotSidekickAlive(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER owner, DWORD dwNow)
	{
		UseHealthPotion(ch, state, dwNow);
		UseManaPotion(ch, state, dwNow);
		state.bTacticalRetreat = false;
		if (!state.bRecoveringAfterDeath)
			return false;
		state.lDeathX = 0;
		state.lDeathY = 0;
		if (!HandlePostDeathRecovery(ch, state, dwNow))
			return false;
		// One told to wait heals where it fell and walks back to its spot after.
		if (!owner || IsPlayerBotSidekickHolding(ch))
			return true;
		const int dist = DISTANCE_APPROX(ch->GetX() - owner->GetX(), ch->GetY() - owner->GetY());
		if (owner->GetMapIndex() == ch->GetMapIndex() && dist > PLAYERBOT_SIDEKICK_FOLLOW_DISTANCE)
		{
			long x = 0, y = 0;
			GetPlayerBotSidekickSpot(ch, owner, x, y);
			WalkPlayerBotSidekick(ch, x, y, dwNow, false);
		}
		return true;
	}

	// Defined in playerbot_manager.cpp, after the fragments: a Shaman's buffs
	// cast on a person, the owner here (ManagePlayerBotBuffHumanLeader's pass).
	bool ManagePlayerBotBuffPerson(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER person, DWORD dwNow,
			bool mayWalk, bool fightBuffsDue);

	// The owner's buffs come before the companion's own and before the next
	// blow. They were cast only when nothing was left to fight, and in the
	// default stance a companion hunting beside its owner always has a
	// monster: the fight put its buffs on itself (FightPlayerBotTowerObjective)
	// and the owner went without them for as long as the hunt lasted - "zaczela
	// stawiac wylacznie na siebie" (Teivos, 25 September). A buff out of reach
	// waits for the fight's end rather than walking the companion off its foe.
	// And the owner's are kept up the way its own are: all of them, wherever it
	// stands. The companion's own go up as a duellist's do
	// (ManagePlayerBotCombatBuffs with duel), while the owner's fight buffs
	// waited for a fight of the companion's - which in "nie walcz" never comes,
	// so a Dragon Shaman, whose Blessing, Reflect and Dragon's Aid are all
	// fight buffs, buffed its owner with nothing (teivos), and after the
	// owner's death buffed itself back up and left the owner bare ("jak sie
	// zginie to ten buff nie chce buffac ... sam siebie buffa", iceBeeg, the
	// same evening). The buffs' cooldown is two seconds, so its own never
	// keep the owner's waiting.
	bool BuffPlayerBotSidekickOwner(LPCHARACTER ch, TPlayerBotAIState& state, const TPlayerBotSidekick& rec,
			LPCHARACTER owner, DWORD dwNow, bool mayWalk)
	{
		if (!rec.bBuffs || !owner || owner->IsDead() || owner->GetMapIndex() != ch->GetMapIndex())
			return false;
		// MT2009_PLUS_SIDEKICK_POLYMORPH_V1: under a marble the engine refuses
		// every buff (IsPlayerBotFightingAsMonster), so it neither walks up to
		// the owner for one nor asks; the buffs come back with its own shape.
		if (IsPlayerBotFightingAsMonster(ch))
			return false;
		return ManagePlayerBotBuffPerson(ch, state, owner, dwNow, mayWalk, true);
	}

	// MT2009_PLUS_SIDEKICK_SHAMAN_REBUFF_V1: a Shaman companion in the saddle.
	//
	// "Szamanka na koniu ma bic zwyklym atakiem, zsiadac aby dac sobie i
	// swojemu towarzyszowi buffy jak sie skoncza i znowu wejsc i bic" (the
	// owner, 5 October). From a battle horse or a seal's mount that is not a
	// standing one it breaks a Metin with the swing alone
	// (CanPlayerBotFightOnHorse) - no skill of a class is cast from that
	// saddle - and the buffs came down one at a time: the owner's pass
	// (ManagePlayerBotBuffPerson) climbed down for one the owner had already
	// lost and renewed nothing that was about to go, the companion's own pass
	// (ManagePlayerBotCombatBuffs) climbed down for its own copy two seconds
	// later, once the skill had cooled from the owner's cast, and a buff
	// running out a moment after the saddle took it back took it down again.
	//
	// So once a second it looks from the saddle: a buff of its path gone or
	// with PLAYERBOT_SIDEKICK_REBUFF_DUE_SECONDS left on the owner (in the
	// skill's reach, its "buffy" on) or on itself, off its cooldown and paid
	// for, takes it down - with no mana for it, a blue potion first and the
	// saddle kept. On foot it puts up everything of the owner's and its own
	// with PLAYERBOT_SADDLE_BUFF_REFRESH_SECONDS or less left, one cast a pass,
	// waits out a cooldown of PLAYERBOT_SIDEKICK_REBUFF_COOL_WAIT_MS (the
	// owner's copy and its own are one skill), and with nothing left lets the
	// fight put it back in the saddle at once (FightPlayerBotTowerObjective).
	// Then PLAYERBOT_SIDEKICK_REBUFF_GAP_MS before it looks again, and a cast
	// the engine refused is left alone for PLAYERBOT_SIDEKICK_REBUFF_FAIL_MS,
	// so no buff takes it up and down in a loop. An owner out of the skill's
	// reach is not waited for: its own buffs only, the owner's when it is
	// back in reach. Its own Swiftness is a walking buff, which the horse
	// outruns: renewed on foot, never a reason to climb down. Cure is a heal,
	// for one of the two under its health share.
	struct TPlayerBotSidekickRebuffPick
	{
		LPCHARACTER target;
		DWORD vnum;
		bool cooling;		// a buff that is due waits for its cooldown
		DWORD coolWaitMs;	// the shortest such wait
		bool shortOfMana;	// a buff that is due is off cooldown but not paid for
	};

	bool IsPlayerBotSidekickRebuffDue(LPCHARACTER ch, const TPlayerBotAIState& state, LPCHARACTER target,
			DWORD vnum, long marginSeconds, DWORD dwNow)
	{
		if (vnum == 109) // Cure / Heal
		{
			const long long limit = target == ch ? 60 : PLAYERBOT_PARTY_LEADER_CURE_HP_PERCENT;
			return target->GetMaxHP() > 0 && (long long)target->GetHP() * 100 / target->GetMaxHP() <= limit;
		}
		const bool up = target == ch ? IsPlayerBotBuffActive(ch, vnum, dwNow, state)
				: IsPlayerBotBuffAffectOn(target, vnum);
		if (!up)
			return true;
		// A toggle is never renewed: UseSkill takes an active one off.
		const CSkillProto* proto = CSkillManager::instance().Get(vnum);
		if (!proto || (proto->dwFlag & SKILL_FLAG_TOGGLE))
			return false;
		const CAffect* affect = target->FindAffect(vnum);
		return affect && affect->lDuration > 0 && affect->lDuration <= marginSeconds;
	}

	// The first buff due, the owner's copy before its own, in its build's
	// order; climbingDown leaves out what is no reason to leave the saddle.
	void PickPlayerBotSidekickRebuff(LPCHARACTER ch, const TPlayerBotAIState& state, LPCHARACTER owner,
			const TPlayerBotSidekickRuntime& rt, long marginSeconds, bool climbingDown, DWORD dwNow,
			TPlayerBotSidekickRebuffPick& pick)
	{
		pick.target = NULL;
		pick.vnum = 0;
		pick.cooling = false;
		pick.coolWaitMs = 0;
		pick.shortOfMana = false;
		const TJobSkillBuild build = GetPlayerBotSkillBuild(ch->GetJob(), ch->GetSkillGroup(), ch->GetPlayerID());
		for (size_t i = 0; i < sizeof(build.dwBuffSkills) / sizeof(build.dwBuffSkills[0]); ++i)
		{
			const DWORD vnum = build.dwBuffSkills[i];
			if (vnum == 0 || ch->GetSkillLevel(vnum) == 0)
				continue;
			CSkillProto* proto = CSkillManager::instance().Get(vnum);
			if (!proto)
				continue;
			LPCHARACTER targets[2] = { NULL, ch };
			if (owner && !IS_SET(proto->dwFlag, SKILL_FLAG_SELFONLY) &&
					(proto->dwTargetRange == 0 ||
					 DISTANCE_APPROX(ch->GetX() - owner->GetX(), ch->GetY() - owner->GetY()) <=
							(int)proto->dwTargetRange))
				targets[0] = owner;
			for (int t = 0; t < 2; ++t)
			{
				LPCHARACTER target = targets[t];
				if (!target)
					continue;
				if (climbingDown && target == ch && vnum != 109 && IsPlayerBotOutOfCombatBuff(vnum))
					continue;
				std::map<DWORD, DWORD>::const_iterator failed =
						rt.mapRebuffFailedUntil.find(vnum * 2 + (target == ch ? 1 : 0));
				if (failed != rt.mapRebuffFailedUntil.end() && dwNow < failed->second)
					continue;
				if (!IsPlayerBotSidekickRebuffDue(ch, state, target, vnum, marginSeconds, dwNow))
					continue;
				// Not while it cools, and not without the mana: the engine
				// would take the mana and refuse the cast (PlayerBotUseSkill).
				if (!IsPlayerBotSkillReady(state, vnum, dwNow))
				{
					std::map<DWORD, DWORD>::const_iterator ready = state.mapSkillReadyAt.find(vnum);
					const DWORD wait = ready != state.mapSkillReadyAt.end() ? ready->second - dwNow : 0;
					if (!pick.cooling || wait < pick.coolWaitMs)
						pick.coolWaitMs = wait;
					pick.cooling = true;
					continue;
				}
				if (ch->GetSP() < GetPlayerBotSkillSPCost(ch, vnum))
				{
					pick.shortOfMana = true;
					continue;
				}
				pick.target = target;
				pick.vnum = vnum;
				return;
			}
		}
	}

	void EndPlayerBotSidekickRebuff(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotSidekickRuntime& rt,
			DWORD dwNow, const char* why)
	{
		rt.dwRebuffUntil = 0;
		rt.dwNextRebuffCheck = dwNow + PLAYERBOT_SIDEKICK_REBUFF_GAP_MS;
		// Back in the saddle on the fight's next look, not after the flip hold.
		if (!ch->IsRiding())
			state.dwNextHorseRideCheckTime = dwNow;
		sys_log(0, "PLAYERBOT_SIDEKICK: shaman rebuff done pid=%u name=%s why=%s", ch->GetPlayerID(),
				ch->GetName(), why);
	}

	// True while it claims the tick: the climb-down, a cast, or standing for
	// the next one.
	bool RebuffPlayerBotSidekickShaman(LPCHARACTER ch, TPlayerBotAIState& state, const TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt, LPCHARACTER owner, LPCHARACTER foe, DWORD dwNow)
	{
		// Under a marble the engine refuses every buff (IsPlayerBotFightingAsMonster).
		if (ch->GetJob() != JOB_SHAMAN || ch->GetSkillGroup() == 0 || ch->IsDead() ||
				IsPlayerBotFightingAsMonster(ch))
		{
			if (rt.dwRebuffUntil != 0)
				EndPlayerBotSidekickRebuff(ch, state, rt, dwNow, "cannot_cast");
			return false;
		}
		LPCHARACTER person = (rec.bBuffs && owner && owner != ch && !owner->IsDead() &&
				owner->GetMapIndex() == ch->GetMapIndex()) ? owner : NULL;
		TPlayerBotSidekickRebuffPick pick;
		if (rt.dwRebuffUntil == 0)
		{
			// Only in a fight from a saddle that casts nothing; a standing
			// mount casts from where it stands, and on foot the ordinary
			// passes buff as they always did.
			if (!foe || !ch->IsRiding() || IsPlayerBotOnStandingMount(ch) || dwNow < rt.dwNextRebuffCheck)
				return false;
			rt.dwNextRebuffCheck = dwNow + PLAYERBOT_SIDEKICK_REBUFF_CHECK_MS;
			PickPlayerBotSidekickRebuff(ch, state, person, rt, PLAYERBOT_SIDEKICK_REBUFF_DUE_SECONDS, true, dwNow,
					pick);
			if (!pick.target)
			{
				if (pick.shortOfMana)
					UseManaPotion(ch, state, dwNow, 100);
				return false;
			}
			if (!SetPlayerBotRidingForTravel(ch, state, false, dwNow, "buff"))
			{
				rt.dwNextRebuffCheck = dwNow + PLAYERBOT_SIDEKICK_REBUFF_GAP_MS;
				return false;
			}
			rt.dwRebuffUntil = dwNow + PLAYERBOT_SIDEKICK_REBUFF_MAX_MS;
			rt.dwRebuffNextCast = dwNow + PLAYERBOT_BUFF_RECHECK_FAST;
			// On foot until the set is up; EndPlayerBotSidekickRebuff lets the
			// saddle back the moment it is.
			if (state.dwNextHorseRideCheckTime < rt.dwRebuffUntil)
				state.dwNextHorseRideCheckTime = rt.dwRebuffUntil;
			state.dwLastMeaningfulActivityTime = dwNow;
			sys_log(0, "PLAYERBOT_SIDEKICK: shaman rebuff climbs down pid=%u name=%s vnum=%u for=%s",
					ch->GetPlayerID(), ch->GetName(), pick.vnum, pick.target == ch ? "self" : "owner");
			return true;
		}
		// Something else put it back in the saddle: the next look decides again.
		if (ch->IsRiding() && !IsPlayerBotOnStandingMount(ch))
		{
			EndPlayerBotSidekickRebuff(ch, state, rt, dwNow, "remounted");
			return false;
		}
		if (dwNow >= rt.dwRebuffUntil)
		{
			EndPlayerBotSidekickRebuff(ch, state, rt, dwNow, "time");
			return false;
		}
		if (ch->IsStateMove())
			ch->Stop();
		state.dwLastMeaningfulActivityTime = dwNow;
		if (dwNow < rt.dwRebuffNextCast)
			return true;
		PickPlayerBotSidekickRebuff(ch, state, person, rt, PLAYERBOT_SADDLE_BUFF_REFRESH_SECONDS, false, dwNow, pick);
		if (pick.target)
		{
			if (PlayerBotUseSkill(ch, state, pick.vnum, pick.target, dwNow))
			{
				SendPlayerBotSkillPacket(ch, pick.vnum);
				state.dwLastBotSkillTime = dwNow;
				state.dwNextAttackTime = dwNow + PLAYERBOT_SKILL_ANIMATION_LOCK;
				// The self pass's fallback clock (IsPlayerBotBuffActive).
				if (pick.target == ch)
					state.mapBuffActiveUntil[pick.vnum] = dwNow +
							(pick.vnum == 109 ? 10000 : PLAYERBOT_BUFF_FALLBACK_DURATION);
				sys_log(0, "PLAYERBOT_SIDEKICK: shaman rebuff cast pid=%u name=%s vnum=%u on=%s",
						ch->GetPlayerID(), ch->GetName(), pick.vnum, pick.target == ch ? "self" : "owner");
			}
			else
				rt.mapRebuffFailedUntil[pick.vnum * 2 + (pick.target == ch ? 1 : 0)] =
						dwNow + PLAYERBOT_SIDEKICK_REBUFF_FAIL_MS;
			rt.dwRebuffNextCast = dwNow + PLAYERBOT_BUFF_RECHECK_FAST;
			return true;
		}
		if (pick.shortOfMana && UseManaPotion(ch, state, dwNow, 100))
		{
			rt.dwRebuffNextCast = dwNow + PLAYERBOT_BUFF_RECHECK_FAST;
			return true;
		}
		if (pick.cooling && pick.coolWaitMs <= PLAYERBOT_SIDEKICK_REBUFF_COOL_WAIT_MS)
		{
			rt.dwRebuffNextCast = dwNow + std::max<DWORD>(pick.coolWaitMs, 200);
			return true;
		}
		EndPlayerBotSidekickRebuff(ch, state, rt, dwNow, pick.shortOfMana ? "no_mana" : "all_up");
		return false;
	}

	// The path the owner chose, the moment the engine allows one:
	// CHARACTER::SetSkillGroup refuses a character under level five
	// (char_skill.cpp), so a companion made at the start of the game takes it at
	// five, wherever it stands. Never the trainer's, which draws a path by pid
	// (playerbot_skills.h), and back again after the old woman's reset.
	// ClearSkill goes first every time, the first path included: it is what
	// hands a player the 4 + (level - 5) points at the trainer, and SetSkillGroup
	// hands none - a companion made under five stood on the points of its levels
	// after five alone, two at level seven where a player has six (Piciu713,
	// 25 September). No skill has a level before the first path.
	void KeepPlayerBotSidekickPath(LPCHARACTER ch, const TPlayerBotSidekick& rec)
	{
		if (rec.bGroup == 0 || ch->GetLevel() < 5 || ch->GetSkillGroup() == rec.bGroup)
			return;
		const BYTE before = ch->GetSkillGroup();
		ch->ClearSkill();
		ch->SetSkillGroup(rec.bGroup);
		sys_log(0, "PLAYERBOT_SIDEKICK: path pid=%u name=%s level=%d group=%u->%u points=%d", ch->GetPlayerID(),
				ch->GetName(), ch->GetLevel(), (unsigned int)before, (unsigned int)ch->GetSkillGroup(),
				(int)ch->GetPoint(POINT_SKILL));
	}

	// The companions made under level five before that: their points are
	// topped up to a player's count once - the trainer's 4 + (level - 5) against
	// what the path's skills already took (a level under Master took that many
	// points, a Master seventeen and the roll at seventeen, the books the rest)
	// - and never past it, so a companion whose points are right is left alone.
	void TopUpPlayerBotSidekickSkillPoints(LPCHARACTER ch)
	{
		const DWORD base = GetPlayerBotSidekickSkillBase(ch);
		if (base == 0 || ch->GetLevel() < 5)
			return;
		const int budget = 4 + ((int)ch->GetLevel() - 5);
		int spent = 0;
		for (DWORD vnum = base; vnum < base + 6; ++vnum)
		{
			const int level = (int)ch->GetSkillLevel(vnum);
			spent += level >= 20 ? 17 : level;
		}
		const int unspent = (int)ch->GetPoint(POINT_SKILL);
		if (unspent + spent >= budget)
			return;
		ch->PointChange(POINT_SKILL, budget - spent - unspent);
		ch->SkillLevelPacket();
		sys_log(0, "PLAYERBOT_SIDEKICK: skill points topped up pid=%u name=%s level=%d spent=%d free=%d->%d",
				ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), spent, unspent, (int)ch->GetPoint(POINT_SKILL));
	}

	// What the next point at seventeen is worth: SkillLevelUp's own roll.
	int GetPlayerBotSidekickMasterChance(LPCHARACTER ch)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		int chance = MIN(100, 25 + 25 * ch->GetQuestFlag("skill_reset2.reset_count"));
		if (ch->GetLevel() < 35)
			chance += 20;
		return MIN(100, chance);
#else
		return 25;	// 1 in 21 - 17
#endif
	}

	// ------------------------------------ MT2009_PLUS_SIDEKICK_GRAND_MASTER_V1
	//
	// Kamien Duchowy in a companion's bag. Its skills are the ones its owner
	// put points into in the window (base..base+5 of its path), so the stone
	// trains the one of those at G1..G10 that comes first: the build's primary
	// skill, then the highest grade. The rules are training_grandmaster_skill
	// .quest's, as a player meets them: the players' wait between two stones
	// (m2_book_wait, at most twelve hours - server-patches/soulstonewait), an
	// Exorcism Scroll's affect waving it once, and the rank, 1000 + 500 a grade
	// over G1, taken in full on a success and a third to a half of it on a
	// failure. Like every bot it never trains its rank below zero (the rank
	// that lets a player hunt it for its gear). Whatever holds it - no stone,
	// the wait, the rank - its owner hears once, again when it changes or
	// every half hour while a stone waits, and in its report ("raport").
	enum EPlayerBotSidekickGrandMaster
	{
		PLAYERBOT_SIDEKICK_GM_NOTHING,	// no skill at G1..G10
		PLAYERBOT_SIDEKICK_GM_NO_STONE,
		PLAYERBOT_SIDEKICK_GM_POLYMORPHED,
		PLAYERBOT_SIDEKICK_GM_BOOK,	// a Rada on, for a class book it reads now
		PLAYERBOT_SIDEKICK_GM_WAIT,
		PLAYERBOT_SIDEKICK_GM_RANK,
		PLAYERBOT_SIDEKICK_GM_READY,
	};

	struct TPlayerBotSidekickGrandMasterView
	{
		BYTE bStatus;
		DWORD dwSkill;
		int iLevel;
		int iRank;		// shown, as the quest reads it
		int iNeed;		// shown
		int iReadyAt;
		int iStones;
		LPITEM pStone;
	};

	struct TPlayerBotSidekickGrandMasterTold
	{
		BYTE bStatus;
		DWORD dwSkill;
		DWORD dwToldAt;
	};
	std::map<DWORD, TPlayerBotSidekickGrandMasterTold> s_mapPlayerBotSidekickGrandMasterTold;
	const DWORD PLAYERBOT_SIDEKICK_GM_TELL_MS = 30 * 60 * 1000;

	// The players' wait between two stones (questlua_pc's M2SoulStoneQuestFlag).
	int GetPlayerBotSidekickSoulStoneWaitSeconds()
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		const int wait = quest::CQuestManager::instance().GetEventFlag("m2_book_wait");
		return wait <= 0 ? 0 : (wait > 12 * 3600 ? 12 * 3600 : wait);
#else
		return PLAYERBOT_GRAND_MASTER_TRAIN_SECONDS;
#endif
	}

	const char* const PLAYERBOT_SIDEKICK_GM_FLAG = "training_grandmaster_skill.next_time";

	TPlayerBotSidekickGrandMasterView AssessPlayerBotSidekickGrandMaster(LPCHARACTER sk)
	{
		TPlayerBotSidekickGrandMasterView v;
		memset(&v, 0, sizeof(v));
		v.bStatus = PLAYERBOT_SIDEKICK_GM_NOTHING;
		const DWORD base = sk ? GetPlayerBotSidekickSkillBase(sk) : 0;
		if (base == 0 || !sk->IsItemLoaded())
			return v;
		const TJobSkillBuild build = GetPlayerBotSkillBuild(sk->GetJob(), sk->GetSkillGroup(), sk->GetPlayerID());
		int best = INT_MIN;
		for (DWORD vnum = base; vnum < base + 6; ++vnum)
		{
			if (!CSkillManager::instance().Get(vnum) || sk->GetSkillMasterType(vnum) != SKILL_GRAND_MASTER)
				continue;
			const int level = sk->GetSkillLevel(vnum);
			if (level < 30 || level >= 40 || !sk->IsLearnableSkill(vnum))
				continue;
			const int priority = (vnum == build.dwPrimaryMaxSkill ? 100000 : 0) + level * 10 - (int)(vnum - base);
			if (priority > best)
			{
				best = priority;
				v.dwSkill = vnum;
			}
		}
		if (v.dwSkill == 0)
			return v;
		v.iLevel = sk->GetSkillLevel(v.dwSkill);
		v.iRank = sk->GetRealAlignment() / 10;
		v.iNeed = GetPlayerBotGrandMasterRankCost(v.iLevel) / 10;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = sk->GetInventoryItem(cell);
			if (!item || item->GetVnum() != PLAYERBOT_GRAND_MASTER_STONE_VNUM)
				continue;
			v.iStones += item->GetCount();
			if (!v.pStone && !item->isLocked())
				v.pStone = item;
		}
		if (!v.pStone)
		{
			v.bStatus = PLAYERBOT_SIDEKICK_GM_NO_STONE;
			return v;
		}
		if (sk->IsPolymorphed())
		{
			v.bStatus = PLAYERBOT_SIDEKICK_GM_POLYMORPHED;
			return v;
		}
		// The quest's wait, shortened to the world's as the engine does it.
		const int now = get_global_time();
		const int wait = GetPlayerBotSidekickSoulStoneWaitSeconds();
		v.iReadyAt = sk->GetQuestFlag(PLAYERBOT_SIDEKICK_GM_FLAG);
		if (v.iReadyAt > now + wait)
			v.iReadyAt = now + wait;
		if (now < v.iReadyAt && !sk->FindAffect(AFFECT_SKILL_NO_BOOK_DELAY))
		{
			v.bStatus = PLAYERBOT_SIDEKICK_GM_WAIT;
			return v;
		}
		if (sk->GetRealAlignment() < GetPlayerBotGrandMasterRankCost(v.iLevel))
		{
			v.bStatus = PLAYERBOT_SIDEKICK_GM_RANK;
			return v;
		}
		// A Rada's affect waits for the class book it was taken for, when that
		// book is read now: the stone's read would take it off for nothing.
		if (sk->FindAffect(AFFECT_SKILL_BOOK_BONUS))
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = sk->GetInventoryItem(cell);
				if (!item || item->GetType() != ITEM_SKILLBOOK)
					continue;
				const DWORD skill = GetPlayerBotSkillBookSkillVnum(item);
				if (!IsPlayerBotOwnSkill(sk, skill) || sk->GetSkillMasterType(skill) != SKILL_MASTER ||
						sk->GetSkillLevel(skill) < 20 || sk->GetSkillLevel(skill) >= 30)
					continue;
				if (IsPlayerBotFastBooksEnabled() || now >= sk->GetSkillNextReadTime(skill))
				{
					v.bStatus = PLAYERBOT_SIDEKICK_GM_BOOK;
					return v;
				}
			}
		v.bStatus = PLAYERBOT_SIDEKICK_GM_READY;
		return v;
	}

	std::string GetPlayerBotSidekickGradeName(int level)
	{
		char grade[8];
		if (level >= 40)
			snprintf(grade, sizeof(grade), "P");
		else
			snprintf(grade, sizeof(grade), "G%d", level - 29);
		return grade;
	}

	std::string GetPlayerBotSidekickWaitWords(int seconds)
	{
		char text[32];
		if (seconds < 60)
			snprintf(text, sizeof(text), "mniej niz minute");
		else if (seconds < 3600)
			snprintf(text, sizeof(text), "%d min", seconds / 60);
		else
			snprintf(text, sizeof(text), "%d h %d min", seconds / 3600, seconds % 3600 / 60);
		return text;
	}

	std::string DescribePlayerBotSidekickGrandMasterView(const TPlayerBotSidekickGrandMasterView& v)
	{
		char text[320];
		const std::string grade = GetPlayerBotSidekickGradeName(v.iLevel);
		const char* skill = v.dwSkill ? GetPlayerBotSkillName(v.dwSkill) : "";
		switch (v.bStatus)
		{
			case PLAYERBOT_SIDEKICK_GM_NO_STONE:
				snprintf(text, sizeof(text), "Trening Wielkiego Mistrza: %s (%s) czeka na Kamien Duchowy - "
						"nie mam zadnego w plecaku. Daj mi go w handlu albo w oknie Towarzysza.", skill, grade.c_str());
				break;
			case PLAYERBOT_SIDEKICK_GM_POLYMORPHED:
				snprintf(text, sizeof(text), "Trening Wielkiego Mistrza: %s (%s) czeka, az skonczy sie przemiana.",
						skill, grade.c_str());
				break;
			case PLAYERBOT_SIDEKICK_GM_WAIT:
				snprintf(text, sizeof(text), "Trening Wielkiego Mistrza: %s (%s) - Kamien Duchowy mam (%d), "
						"ale nastepny moge uzyc dopiero za %s.", skill, grade.c_str(), v.iStones,
						GetPlayerBotSidekickWaitWords(v.iReadyAt - get_global_time()).c_str());
				break;
			case PLAYERBOT_SIDEKICK_GM_RANK:
				snprintf(text, sizeof(text), "Trening Wielkiego Mistrza: %s (%s) - Kamien Duchowy mam (%d), "
						"ale ranga za niska: mam %d, trening kosztuje %d. Ranga rosnie za zabijanie potworow.",
						skill, grade.c_str(), v.iStones, v.iRank, v.iNeed);
				break;
			case PLAYERBOT_SIDEKICK_GM_BOOK:
				snprintf(text, sizeof(text), "Trening Wielkiego Mistrza: %s (%s) - najpierw czytam ksiege pod "
						"Rade Pustelnika, potem Kamien Duchowy.", skill, grade.c_str());
				break;
			case PLAYERBOT_SIDEKICK_GM_READY:
				snprintf(text, sizeof(text), "Trening Wielkiego Mistrza: %s (%s) - zaraz uzywam Kamienia Duchowego "
						"(mam %d).", skill, grade.c_str(), v.iStones);
				break;
			default:
				return std::string();
		}
		return text;
	}

	std::string DescribePlayerBotSidekickGrandMaster(LPCHARACTER sk)
	{
		return DescribePlayerBotSidekickGrandMasterView(AssessPlayerBotSidekickGrandMaster(sk));
	}

	// Called by ManagePlayerBotGrandMasterTraining for a companion, every
	// PLAYERBOT_GRAND_MASTER_CHECK_INTERVAL, wherever it is.
	void TrainPlayerBotSidekickGrandMaster(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		if (!rec || !rec->bSetupDone)
			return;
		TPlayerBotSidekickGrandMasterView v = AssessPlayerBotSidekickGrandMaster(ch);
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID);
		if (v.bStatus != PLAYERBOT_SIDEKICK_GM_READY)
		{
			// Nothing to say without a Grand Master skill, nor while it reads
			// the book first; no stone is told once, when that changes, and the
			// rest again every half hour while a stone waits in its bag.
			if (v.bStatus == PLAYERBOT_SIDEKICK_GM_NOTHING || v.bStatus == PLAYERBOT_SIDEKICK_GM_BOOK)
				return;
			TPlayerBotSidekickGrandMasterTold& told = s_mapPlayerBotSidekickGrandMasterTold[ch->GetPlayerID()];
			const bool changed = told.bStatus != v.bStatus || told.dwSkill != v.dwSkill;
			const bool again = v.bStatus != PLAYERBOT_SIDEKICK_GM_NO_STONE &&
					dwNow - told.dwToldAt >= PLAYERBOT_SIDEKICK_GM_TELL_MS;
			if (!owner || (!changed && !again))
				return;
			told.bStatus = v.bStatus;
			told.dwSkill = v.dwSkill;
			told.dwToldAt = dwNow;
			const std::string text = DescribePlayerBotSidekickGrandMasterView(v);
			SayPlayerBotSidekick(owner, text.c_str());
			sys_log(0, "PLAYERBOT_SIDEKICK: grand master waits pid=%u name=%s owner=%u skill=%u level=%d "
					"status=%u rank=%d need=%d stones=%d ready_in=%d",
					ch->GetPlayerID(), ch->GetName(), rec->dwOwnerPID, v.dwSkill, v.iLevel,
					(unsigned int)v.bStatus, v.iRank, v.iNeed, v.iStones,
					v.iReadyAt > get_global_time() ? v.iReadyAt - get_global_time() : 0);
			return;
		}
		if (ch->IsDead() || ch->GetExchange() || ch->GetMyShop())
			return;

		// The quest, in its order: an Exorcism Scroll's affect waves a wait
		// still running and goes, the next wait is set, the stone is spent,
		// then the roll and the rank.
		const int now = get_global_time();
		const int wait = GetPlayerBotSidekickSoulStoneWaitSeconds();
		if (now < v.iReadyAt)
			ch->RemoveAffect(AFFECT_SKILL_NO_BOOK_DELAY);
		ch->SetQuestFlag(PLAYERBOT_SIDEKICK_GM_FLAG, now + wait);
		if (v.pStone->GetCount() > 1)
			v.pStone->SetCount(v.pStone->GetCount() - 1);
		else
			ITEM_MANAGER::instance().RemoveItem(v.pStone, "PLAYERBOT_SIDEKICK_GRAND_MASTER_READ");
		const int rankBefore = ch->GetRealAlignment() / 10;
		const bool learned = ch->LearnGrandMasterSkill(v.dwSkill);
		const int paid = learned ? v.iNeed : number(v.iNeed / 3, v.iNeed / 2);
		ch->UpdateAlignment(-paid * 10);
		SetPlayerBotAction(state, BOT_ACTION_READ_BOOK, dwNow);
		const int levelNow = ch->GetSkillLevel(v.dwSkill);
		sys_log(0, "PLAYERBOT_SIDEKICK: grand master training %s pid=%u name=%s owner=%u skill=%u level=%d->%d "
				"rank=%d->%d stones_left=%d",
				learned ? "SUCCESS" : "FAILED", ch->GetPlayerID(), ch->GetName(), rec->dwOwnerPID, v.dwSkill,
				v.iLevel, levelNow, rankBefore, ch->GetRealAlignment() / 10, v.iStones - 1);
		s_mapPlayerBotSidekickGrandMasterTold.erase(ch->GetPlayerID());
		if (!owner)
			return;
		char text[320];
		const std::string waitWords = wait > 0 ? GetPlayerBotSidekickWaitWords(wait) : std::string("od razu");
		if (learned)
			snprintf(text, sizeof(text), "Kamien Duchowy uzyty: %s - udalo sie, teraz %s (ranga -%d). "
					"Kamieni zostalo: %d, nastepny trening: %s.", GetPlayerBotSkillName(v.dwSkill),
					GetPlayerBotSidekickGradeName(levelNow).c_str(), paid, v.iStones - 1, waitWords.c_str());
		else
			snprintf(text, sizeof(text), "Kamien Duchowy uzyty: %s - nie udalo sie, zostaje %s (ranga -%d). "
					"Kamieni zostalo: %d, nastepna proba: %s.", GetPlayerBotSkillName(v.dwSkill),
					GetPlayerBotSidekickGradeName(levelNow).c_str(), paid, v.iStones - 1, waitWords.c_str());
		SayPlayerBotSidekick(owner, text);
	}

	// A skill at seventeen that did not turn Master, and the Forgetting Book
	// to roll again (Hiob's screenshot of 26 September: Smoczy Skowyt and
	// Pomoc Smoka at seventeen, nothing free - "ona zacznie uzywac OZ???";
	// the operator: "bysmy mogli uzyc za nia ksiegi zapomnienia lub ona sama bedzie
	// uzywac"). The engine rolls once, as a point takes a skill to seventeen
	// (SkillLevelUp), and refuses every point after it; the book
	// (ITEM_SKILLFORGET, the skill in its first socket, SkillLevelDown) takes
	// the level back to sixteen with its point, and that point spent again is
	// the next roll. The ordinary bots buy theirs out of their purse on a pass
	// with a point in hand (ManagePlayerBotSkills); a companion has no purse
	// and spends a point the moment it has one, so its skills at seventeen
	// stood there for good. The book is its owner's to give - the trade or
	// the bag window - and it reads it here, by itself: a book naming one of
	// its skills that stands at seventeen, as the book would for a player.
	// Its points spent by itself, the point goes straight back into that
	// skill; spent by the owner, the point waits in the window for the
	// owner's "+". A book for a skill that is not stuck stays in the bag - it
	// would take a level for nothing - and a stuck skill with no book in the
	// bag has the owner asked for one.
	void ReadPlayerBotSidekickForgetBook(LPCHARACTER ch, const TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt, DWORD dwNow)
	{
		if (dwNow < rt.dwNextForgetCheck)
			return;
		rt.dwNextForgetCheck = dwNow + PLAYERBOT_SIDEKICK_FORGET_CHECK_MS;
		const DWORD base = GetPlayerBotSidekickSkillBase(ch);
		if (base == 0 || ch->IsDead() || ch->IsPolymorphed() || !ch->IsItemLoaded() || ch->GetExchange())
			return;
		std::set<DWORD> stuck;
		for (DWORD vnum = base; vnum < base + 6; ++vnum)
			if (CSkillManager::instance().Get(vnum) && ch->GetSkillMasterType(vnum) == SKILL_NORMAL &&
					ch->GetSkillLevel(vnum) >= PLAYERBOT_SKILL_MASTER_TRY_LEVEL && ch->GetSkillLevel(vnum) < 20)
				stuck.insert(vnum);
		if (stuck.empty())
			return;
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec.dwOwnerPID);
		char text[256];
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetType() != ITEM_SKILLFORGET || item->isLocked())
				continue;
			const DWORD skill = (DWORD)item->GetSocket(0);
			if (stuck.find(skill) == stuck.end())
			{
				if (rt.setForgetToldItems.insert(item->GetID()).second)
				{
					const bool mine = skill >= base && skill < base + 6;
					snprintf(text, sizeof(text), "Ta Ksiega Zapomnienia jest do umiejetnosci %s, %s - zostaje w plecaku.%s",
							skill ? GetPlayerBotSkillName(skill) : "(zadnej)",
							mine ? "a ta nie stoi na 17" : "ktorej nie mam",
							mine ? " Zeruj w oknie umiejetnosci ja zuzyje." : "");
					SayPlayerBotSidekick(owner, text);
				}
				continue;
			}
			const int before = (int)ch->GetSkillLevel(skill);
			const DWORD itemId = item->GetID();
			ch->UseItem(TItemPos(INVENTORY, cell));
			const int after = (int)ch->GetSkillLevel(skill);
			if (after >= before)
			{
				sys_log(0, "PLAYERBOT_SIDEKICK: forget book refused pid=%u name=%s skill=%u level=%d item=%u",
						ch->GetPlayerID(), ch->GetName(), skill, before, itemId);
				return;
			}
			int rolled = after;
			bool master = false;
			if (!rec.bManualSkills && ch->GetPoint(POINT_SKILL) > 0)
			{
				ch->SkillLevelUp(skill);
				rolled = (int)ch->GetSkillLevel(skill);
				master = ch->GetSkillMasterType(skill) != SKILL_NORMAL;
			}
			sys_log(0, "PLAYERBOT_SIDEKICK: forget book pid=%u name=%s owner=%u skill=%u level=%d->%d->%d master=%d manual=%d chance=%d",
					ch->GetPlayerID(), ch->GetName(), rec.dwOwnerPID, skill, before, after, rolled, master ? 1 : 0,
					rec.bManualSkills ? 1 : 0, GetPlayerBotSidekickMasterChance(ch));
			if (rec.bManualSkills)
				snprintf(text, sizeof(text), "Ksiega Zapomnienia przeczytana: %s na %d. Dodaj punkt w oknie "
						"umiejetnosci - to proba na mistrza (szansa %d%%).", GetPlayerBotSkillName(skill), after,
						GetPlayerBotSidekickMasterChance(ch));
			else if (master)
				snprintf(text, sizeof(text), "Ksiega Zapomnienia przeczytana: %s - mistrz!", GetPlayerBotSkillName(skill));
			else
				snprintf(text, sizeof(text), "Ksiega Zapomnienia przeczytana: %s znow na %d, bez mistrza. "
						"Kolejna ksiega to kolejna proba (szansa %d%%).", GetPlayerBotSkillName(skill), rolled,
						GetPlayerBotSidekickMasterChance(ch));
			SayPlayerBotSidekick(owner, text);
			rt.mapForgetAskedAt[skill] = dwNow;
			return;
		}
#if defined(PLAYERBOT_ENGINE_MT2009)
		// No book: the skill reset scroll, for the stuck skill of the lowest
		// vnum - after the book, which is a roll where the scroll is a Master
		// bought outright.
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetVnum() != PLAYERBOT_SIDEKICK_SKILL_RESET_SCROLL_VNUM || item->isLocked())
				continue;
			const DWORD skill = *stuck.begin();
			const int before = (int)ch->GetSkillLevel(skill);
			if (!ch->ResetOneSkill(skill))
				return;
			ch->SetQuestFlag("reset_status_items.force_to_master_skill",
					ch->GetQuestFlag("reset_status_items.force_to_master_skill") + 1);
			item->SetCount(item->GetCount() - 1);
			bool master = false;
			if (!rec.bManualSkills)
			{
				for (int guard = 0; guard < 20 && ch->GetPoint(POINT_SKILL) > 0 &&
						ch->GetSkillMasterType(skill) == SKILL_NORMAL && ch->GetSkillLevel(skill) < 17; ++guard)
				{
					const int was = (int)ch->GetSkillLevel(skill);
					ch->SkillLevelUp(skill);
					if ((int)ch->GetSkillLevel(skill) == was && ch->GetSkillMasterType(skill) == SKILL_NORMAL)
						break;
				}
				master = ch->GetSkillMasterType(skill) != SKILL_NORMAL;
			}
			ch->Save();
			sys_log(0, "PLAYERBOT_SIDEKICK: skill reset scroll pid=%u name=%s owner=%u skill=%u level=%d->%d master=%d manual=%d points=%d",
					ch->GetPlayerID(), ch->GetName(), rec.dwOwnerPID, skill, before, (int)ch->GetSkillLevel(skill),
					master ? 1 : 0, rec.bManualSkills ? 1 : 0, (int)ch->GetPoint(POINT_SKILL));
			if (rec.bManualSkills)
				snprintf(text, sizeof(text), "Zwoj Powrotu Umiejetnosci uzyty: %s od zera, punkty czekaja w oknie "
						"umiejetnosci. Pierwsza umiejetnosc, ktora dojdzie do 17, zostanie mistrzem.",
						GetPlayerBotSkillName(skill));
			else if (master)
				snprintf(text, sizeof(text), "Zwoj Powrotu Umiejetnosci uzyty: %s - mistrz!", GetPlayerBotSkillName(skill));
			else
				snprintf(text, sizeof(text), "Zwoj Powrotu Umiejetnosci uzyty: %s na %d. Mistrz przy 17.",
						GetPlayerBotSkillName(skill), (int)ch->GetSkillLevel(skill));
			SayPlayerBotSidekick(owner, text);
			rt.mapForgetAskedAt[skill] = dwNow;
			return;
		}
#endif
		// Nothing to read: its owner is asked, once in a while, in one line
		// for every skill whose clock has run out.
		if (!owner || rec.bMode != PLAYERBOT_SIDEKICK_FOLLOW)
			return;
		std::string names;
		for (std::set<DWORD>::const_iterator it = stuck.begin(); it != stuck.end(); ++it)
		{
			std::map<DWORD, DWORD>::iterator asked = rt.mapForgetAskedAt.find(*it);
			if (asked != rt.mapForgetAskedAt.end() && dwNow - asked->second < PLAYERBOT_SIDEKICK_FORGET_ASK_MS)
				continue;
			rt.mapForgetAskedAt[*it] = dwNow;
			if (!names.empty())
				names += ", ";
			names += GetPlayerBotSkillName(*it);
		}
		if (names.empty())
			return;
		snprintf(text, sizeof(text), "Na 17 bez mistrza: %s. Daj mi Ksiege Zapomnienia tej umiejetnosci"
#if defined(PLAYERBOT_ENGINE_MT2009)
				" (albo Zwoj Powrotu Umiejetnosci)"
#endif
				" - uzyje jej i sprobuje mistrza (szansa %d%%).", names.c_str(),
				GetPlayerBotSidekickMasterChance(ch));
		SayPlayerBotSidekick(owner, text);
	}

	// A fight where it stands - at its owner's side, or at the spot it keeps -
	// with the straight walk a map with no navigation grid needs.
	bool FightPlayerBotSidekickFoe(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotSidekickRuntime& rt,
			LPCHARACTER foe, int why, DWORD dwNow)
	{
		if (foe->GetVID() != rt.dwLastFoeVID)
		{
			rt.dwLastFoeVID = foe->GetVID();
			++rt.adwFoes[MINMAX(0, why, 3)];
		}
		state.dwLastMeaningfulActivityTime = dwNow;
		// MT2009_PLUS_SIDEKICK_LURE_V1: the packs a course brought stand round
		// it now - the skills on them as soon as one is due, the area ones
		// taking the whole gathering, rather than on the rotation's clock of
		// a hunt (a Shaman's six seconds).
		if (rt.dwLureGatheredUntil != 0)
		{
			if (dwNow >= rt.dwLureGatheredUntil)
				rt.dwLureGatheredUntil = 0;
			else if (CountPlayerBotSidekickChasers(ch) >= PLAYERBOT_SIDEKICK_LURE_GATHERED_MIN &&
					state.dwNextSkillCastTime > dwNow && dwNow >= state.dwNextAttackTime)
				state.dwNextSkillCastTime = dwNow;
		}
		const int foeDist = DISTANCE_APPROX(ch->GetX() - foe->GetX(), ch->GetY() - foe->GetY());
		if (foeDist > PLAYERBOT_DUEL_MELEE_RANGE &&
				!CPlayerBotNavigation::instance(ch->GetMapIndex()).Init(ch->GetMapIndex()) &&
				dwNow >= state.dwNextTowerMoveTime)
		{
			state.dwNextTowerMoveTime = dwNow + 1000;
			WalkPlayerBotSidekickDirect(ch, foe->GetX(), foe->GetY(), dwNow);
		}
		return FightPlayerBotTowerObjective(ch, state, foe, dwNow);
	}

	// ------------------------------------------------------------- the lure
	//
	// "Lurowanie" (the window's switch, "/towarzysz luruj 1", the whisper
	// "luruj"): beside an owner standing on a spot, the companion walks out to
	// the packs round it nobody is fighting, wakes each with a blow - the
	// group wakes with it through the engine's party aggro - and walks back
	// with what it woke, up to PLAYERBOT_SIDEKICK_LURE_PACKS packs a course.
	// What it brings keeps fighting it beside the owner, whose area skills
	// then have them all in reach; should it fall, they turn on the owner
	// (HandPlayerBotSidekickFoesToOwner). Not an Archer's party role
	// (playerbot_lure.h, which needs a bow and three in a party): any class,
	// its own blow, the owner's spot.

	// What stands round the owner in a fight with either of them, and round
	// the companion in a fight with it.
	struct FPlayerBotSidekickEngaged
	{
		LPCHARACTER self;
		LPCHARACTER owner;
		int onBoth;
		int onSelf;

		FPlayerBotSidekickEngaged(LPCHARACTER s, LPCHARACTER o) : self(s), owner(o), onBoth(0), onSelf(0)
		{
		}

		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER c = (LPCHARACTER)ent;
			if (c->IsDead() || !c->IsMonster())
				return;
			LPCHARACTER victim = c->GetVictim();
			if (!victim)
				return;
			if (victim == self)
			{
				++onSelf;
				++onBoth;
			}
			else if (owner && victim == owner)
				++onBoth;
		}
	};

	int CountPlayerBotSidekickChasers(LPCHARACTER ch)
	{
		if (!ch->GetSectree())
			return 0;
		FPlayerBotSidekickEngaged engaged(ch, NULL);
		ch->GetSectree()->ForEachAround(engaged);
		return engaged.onSelf;
	}

	// The packs round the anchor - where the owner stood when the course began
	// - one entry per group: the CParty the regen spawned it in, or the monster
	// itself when it stands alone. A pack with a member already in a fight, one
	// too strong for the owner, one in a safe zone and one at a spot this
	// course woke already are no packs to go for.
	struct FPlayerBotSidekickLurePacks
	{
		struct TPack
		{
			DWORD dwVID;
			int iDist;
			int iMembers;
			bool bRefused;
			long x;
			long y;
			TPack() : dwVID(0), iDist(INT_MAX), iMembers(0), bRefused(false), x(0), y(0)
			{
			}
		};
		LPCHARACTER self;
		LPCHARACTER owner;
		long anchorX;
		long anchorY;
		int maxLevel;
		const std::vector<std::pair<long, long> >& spots;
		std::map<const void*, TPack> packs;
		int seen;

		FPlayerBotSidekickLurePacks(LPCHARACTER s, LPCHARACTER o, long x, long y, int level,
				const std::vector<std::pair<long, long> >& taken)
			: self(s), owner(o), anchorX(x), anchorY(y), maxLevel(level), spots(taken), seen(0)
		{
		}

		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER c = (LPCHARACTER)ent;
			if (c == self || c == owner || c->IsDead() || !c->IsMonster() || c->GetMobRank() >= MOB_RANK_BOSS ||
					c->GetMapIndex() != self->GetMapIndex())
				return;
			const int fromAnchor = DISTANCE_APPROX(c->GetX() - anchorX, c->GetY() - anchorY);
			if (fromAnchor < PLAYERBOT_SIDEKICK_LURE_MIN_RANGE || fromAnchor > PLAYERBOT_SIDEKICK_LURE_MAX_RANGE)
				return;
			++seen;
			const void* key = c->GetParty() ? (const void*)c->GetParty() : (const void*)c;
			TPack& pack = packs[key];
			if (pack.bRefused)
				return;
			bool refuse = c->GetVictim() != NULL || (int)c->GetLevel() > maxLevel ||
					IsPlayerBotSafeZone(c->GetMapIndex(), c->GetX(), c->GetY());
			for (size_t i = 0; i < spots.size() && !refuse; ++i)
				refuse = DISTANCE_APPROX(c->GetX() - spots[i].first, c->GetY() - spots[i].second) <
						PLAYERBOT_SIDEKICK_LURE_SEPARATION;
			if (refuse)
			{
				pack.bRefused = true;
				return;
			}
			++pack.iMembers;
			const int fromSelf = DISTANCE_APPROX(c->GetX() - self->GetX(), c->GetY() - self->GetY());
			if (fromSelf < pack.iDist)
			{
				pack.iDist = fromSelf;
				pack.dwVID = (DWORD)c->GetVID();
				pack.x = c->GetX();
				pack.y = c->GetY();
			}
		}
	};

	// The pack to go for next: a group before a monster on its own, the nearer
	// of two, one another bot is on or the companion cannot walk to passed
	// over. The member it answers with is the one to strike.
	LPCHARACTER PickPlayerBotSidekickLurePack(LPCHARACTER ch, LPCHARACTER owner, const TPlayerBotSidekickRuntime& rt,
			int& members, int& seen)
	{
		members = 0;
		seen = 0;
		if (!owner->GetSectree())
			return NULL;
		FPlayerBotSidekickLurePacks finder(ch, owner, rt.lLureAnchorX, rt.lLureAnchorY,
				(int)owner->GetLevel() + PLAYERBOT_SIDEKICK_LURE_LEVEL_OVER, rt.vecLureSpots);
		owner->GetSectree()->ForEachAround(finder);
		seen = finder.seen;
		std::vector<std::pair<int, const FPlayerBotSidekickLurePacks::TPack*> > order;
		for (std::map<const void*, FPlayerBotSidekickLurePacks::TPack>::const_iterator it = finder.packs.begin();
				it != finder.packs.end(); ++it)
		{
			const FPlayerBotSidekickLurePacks::TPack& pack = it->second;
			if (pack.bRefused || pack.dwVID == 0)
				continue;
			const int size = std::min(pack.iMembers, 4);
			order.push_back(std::make_pair(pack.iDist - 400 * size + (pack.iMembers == 1 ? 800 : 0), &pack));
		}
		std::sort(order.begin(), order.end(),
				[](const std::pair<int, const FPlayerBotSidekickLurePacks::TPack*>& a,
						const std::pair<int, const FPlayerBotSidekickLurePacks::TPack*>& b) { return a.first < b.first; });
		for (size_t i = 0; i < order.size() && i < 6; ++i)
		{
			const FPlayerBotSidekickLurePacks::TPack& pack = *order[i].second;
			if (IsTargetClaimedByAnotherBot(ch, pack.dwVID) ||
					!IsPlayerBotReachable(ch->GetMapIndex(), ch->GetX(), ch->GetY(), pack.x, pack.y))
				continue;
			LPCHARACTER target = CHARACTER_MANAGER::instance().Find(pack.dwVID);
			if (!target)
				continue;
			members = pack.iMembers;
			return target;
		}
		return NULL;
	}

	int GetPlayerBotSidekickHealthPercent(LPCHARACTER ch)
	{
		return ch->GetMaxHP() > 0 ? (int)((long long)ch->GetHP() * 100 / ch->GetMaxHP()) : 0;
	}

	// MT2009_PLUS_SIDEKICK_LURE_V1: a pack is woken with a plain blow - the
	// basic hit, an Archer's arrow at its bow's reach - never a skill (prodnathin,
	// 1 October: "towarzysz zbiera grupki skillami i nic nie zostaje do
	// zebrania"). The tower's fight cast the rotation at the first monster, and
	// a splash skill killed the pack it was meant to bring, or woke one and
	// spent the cooldowns the gathered packs were for.
	void StrikePlayerBotSidekickLurePack(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER target, DWORD dwNow)
	{
		ReadyPlayerBotHandForFight(ch, state, dwNow, "sidekick_lure");
		LPITEM weapon = ch->GetWear(WEAR_WEAPON);
		const bool isBow = !IsPlayerBotFightingAsMonster(ch) && weapon && weapon->GetType() == ITEM_WEAPON &&
				weapon->GetSubType() == WEAPON_BOW;
		const int reach = isBow ? GetPlayerBotBowRange(ch->GetMapIndex()) - PLAYERBOT_BOW_APPROACH_SLACK
				: PLAYERBOT_DUEL_MELEE_RANGE;
		const int distance = DISTANCE_APPROX(ch->GetX() - target->GetX(), ch->GetY() - target->GetY());
		state.dwTargetVID = (DWORD)target->GetVID();
		ch->SetVictim(target);
		ch->SetRotationToXY(target->GetX(), target->GetY());
		SetPlayerBotAction(state, BOT_ACTION_FIGHT, dwNow);
		if (distance > reach)
		{
			WalkPlayerBotSidekick(ch, target->GetX(), target->GetY(), dwNow, false);
			return;
		}
		if (ch->IsStateMove())
			ch->Stop();
		ch->SetPosition(POS_FIGHTING);
		ExecutePlayerBotBasicAttack(ch, target, state, dwNow);
	}

	void EndPlayerBotSidekickLure(LPCHARACTER ch, TPlayerBotSidekickRuntime& rt, DWORD dwNow, const char* why)
	{
		if (rt.bLureStage != 0)
			sys_log(0, "PLAYERBOT_SIDEKICK: lure over pid=%u name=%s why=%s packs=%d chasers=%d course_s=%u",
					ch->GetPlayerID(), ch->GetName(), why, rt.iLurePacks, CountPlayerBotSidekickChasers(ch),
					dwNow >= rt.dwLureCourseSince ? (dwNow - rt.dwLureCourseSince) / 1000 : 0U);
		rt.bLureStage = 0;
		rt.dwLureVID = 0;
		rt.iLurePacks = 0;
		rt.iLureMonsters = 0;
		rt.vecLureSpots.clear();
		rt.dwNextLure = dwNow + PLAYERBOT_SIDEKICK_LURE_PAUSE_MS;
	}

	// Aims the course at the next pack, or turns it home when there is none
	// (or the course has its packs already).
	void AimPlayerBotSidekickLure(LPCHARACTER ch, LPCHARACTER owner, TPlayerBotSidekickRuntime& rt, DWORD dwNow)
	{
		rt.dwLureStageSince = dwNow;
		if (rt.iLurePacks >= PLAYERBOT_SIDEKICK_LURE_PACKS)
		{
			rt.bLureStage = 2;
			rt.dwLureVID = 0;
			return;
		}
		int members = 0, seen = 0;
		LPCHARACTER target = PickPlayerBotSidekickLurePack(ch, owner, rt, members, seen);
		if (!target)
		{
			rt.bLureStage = rt.iLurePacks > 0 ? 2 : 0;
			rt.dwLureVID = 0;
			return;
		}
		rt.bLureStage = 1;
		rt.dwLureVID = (DWORD)target->GetVID();
	}

	// The course, one step a tick; true while it has the tick. With nothing to
	// do it looks for a course once a second, and after a look that found no
	// pack PLAYERBOT_SIDEKICK_LURE_NOTHING_MS later.
	bool ManagePlayerBotSidekickLure(LPCHARACTER ch, TPlayerBotAIState& state, const TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt, LPCHARACTER owner, DWORD dwNow)
	{
		if (!rec.bLure || rec.bStance == PLAYERBOT_SIDEKICK_STANCE_PASSIVE || rt.bHold || rt.bErrand)
		{
			if (rt.bLureStage != 0)
				EndPlayerBotSidekickLure(ch, rt, dwNow, "switched_off");
			return false;
		}
		if (rt.bLureStage == 0)
		{
			if (dwNow < rt.dwNextLure)
				return false;
			rt.dwNextLure = dwNow + 1000;
			if (owner->IsDead() || owner->GetMapIndex() != ch->GetMapIndex() || !owner->GetSectree() ||
					owner->IsStateMove() ||
					IsPlayerBotSafeZone(owner->GetMapIndex(), owner->GetX(), owner->GetY()) ||
					GetPlayerBotSidekickHealthPercent(owner) < PLAYERBOT_SIDEKICK_LURE_START_HP_PERCENT ||
					GetPlayerBotSidekickHealthPercent(ch) < PLAYERBOT_SIDEKICK_LURE_START_HP_PERCENT)
				return false;
			FPlayerBotSidekickEngaged engaged(ch, owner);
			owner->GetSectree()->ForEachAround(engaged);
			if (engaged.onBoth > PLAYERBOT_SIDEKICK_LURE_BUSY_MONSTERS)
				return false;
			rt.lLureAnchorX = owner->GetX();
			rt.lLureAnchorY = owner->GetY();
			rt.vecLureSpots.clear();
			rt.iLurePacks = 0;
			rt.iLureMonsters = engaged.onSelf;
			int members = 0, seen = 0;
			LPCHARACTER target = PickPlayerBotSidekickLurePack(ch, owner, rt, members, seen);
			if (!target)
			{
				rt.dwNextLure = dwNow + PLAYERBOT_SIDEKICK_LURE_NOTHING_MS;
				PlayerBotLogThrottled("sidekick_lure_none", dwNow,
						"PLAYERBOT_SIDEKICK: nothing to lure pid=%u name=%s map=%ld seen=%d",
						ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), seen);
				return false;
			}
			rt.bLureStage = 1;
			rt.dwLureVID = (DWORD)target->GetVID();
			rt.dwLureCourseSince = dwNow;
			rt.dwLureStageSince = dwNow;
			++rt.uLureCourses;
			sys_log(0, "PLAYERBOT_SIDEKICK: lure begins pid=%u name=%s owner=%u map=%ld first=%s members=%d dist=%d",
					ch->GetPlayerID(), ch->GetName(), owner->GetPlayerID(), ch->GetMapIndex(), target->GetName(),
					members, DISTANCE_APPROX(target->GetX() - ch->GetX(), target->GetY() - ch->GetY()));
		}
		state.dwLastMeaningfulActivityTime = dwNow;
		// The owner gone from the spot, somebody's health low, the course too
		// long: what is woken comes home now, and with nothing woken it ends.
		const bool ownerLeft = owner->IsDead() || owner->GetMapIndex() != ch->GetMapIndex() ||
				DISTANCE_APPROX(owner->GetX() - rt.lLureAnchorX, owner->GetY() - rt.lLureAnchorY) >
						PLAYERBOT_SIDEKICK_LURE_ANCHOR_LEASH;
		if (owner->GetMapIndex() != ch->GetMapIndex())
		{
			EndPlayerBotSidekickLure(ch, rt, dwNow, "owner_left_map");
			return false;
		}
		const bool hurt = GetPlayerBotSidekickHealthPercent(owner) < PLAYERBOT_SIDEKICK_LURE_ABORT_HP_PERCENT ||
				GetPlayerBotSidekickHealthPercent(ch) < PLAYERBOT_SIDEKICK_LURE_ABORT_HP_PERCENT;
		const bool late = dwNow >= rt.dwLureCourseSince && dwNow - rt.dwLureCourseSince > PLAYERBOT_SIDEKICK_LURE_COURSE_MS;
		if (rt.bLureStage == 1 && (ownerLeft || hurt || late))
		{
			if (rt.iLurePacks == 0)
			{
				EndPlayerBotSidekickLure(ch, rt, dwNow, ownerLeft ? "owner_left" : hurt ? "hurt" : "late");
				return false;
			}
			rt.bLureStage = 2;
			rt.dwLureVID = 0;
			rt.dwLureStageSince = dwNow;
		}
		if (rt.bLureStage == 1)
		{
			LPCHARACTER target = CHARACTER_MANAGER::instance().Find(rt.dwLureVID);
			const int chasers = CountPlayerBotSidekickChasers(ch);
			LPCHARACTER victim = target && !target->IsDead() ? target->GetVictim() : NULL;
			// A pack is woken when it chases the companion - the one it struck,
			// or more of them than before the blow (the struck one may have died
			// of it and its group come on all the same).
			if (victim == ch || chasers > rt.iLureMonsters)
			{
				++rt.iLurePacks;
				rt.vecLureSpots.push_back(target ? std::make_pair((long)target->GetX(), (long)target->GetY())
						: std::make_pair(ch->GetX(), ch->GetY()));
				rt.iLureMonsters = std::max(chasers, rt.iLureMonsters + 1);
				AimPlayerBotSidekickLure(ch, owner, rt, dwNow);
				return true;
			}
			// Gone, fighting somebody else, or out of reach for too long: the
			// next one.
			if (!target || target->IsDead() || victim ||
					(dwNow >= rt.dwLureStageSince && dwNow - rt.dwLureStageSince > PLAYERBOT_SIDEKICK_LURE_APPROACH_MS))
			{
				if (target)
					rt.vecLureSpots.push_back(std::make_pair((long)target->GetX(), (long)target->GetY()));
				AimPlayerBotSidekickLure(ch, owner, rt, dwNow);
				if (rt.bLureStage == 0)
				{
					EndPlayerBotSidekickLure(ch, rt, dwNow, "no_pack");
					return false;
				}
				return true;
			}
			const int dist = DISTANCE_APPROX(target->GetX() - ch->GetX(), target->GetY() - ch->GetY());
			if (dist > PLAYERBOT_SIDEKICK_LURE_STRIKE_RANGE)
			{
				WalkPlayerBotSidekick(ch, target->GetX(), target->GetY(), dwNow, false);
				SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
				return true;
			}
			StrikePlayerBotSidekickLurePack(ch, state, target, dwNow);
			return true;
		}
		// Home with what it woke: beside the owner the course is over and the
		// fight is the ordinary one, the woken packs on the companion.
		long x = 0, y = 0;
		GetPlayerBotSidekickSpot(ch, owner, x, y);
		if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) > PLAYERBOT_SIDEKICK_LURE_BACK_RANGE &&
				!(dwNow >= rt.dwLureStageSince && dwNow - rt.dwLureStageSince > PLAYERBOT_SIDEKICK_LURE_APPROACH_MS))
		{
			if (state.dwTargetVID != 0)
			{
				state.dwTargetVID = 0;
				ch->SetVictim(NULL);
			}
			WalkPlayerBotSidekick(ch, x, y, dwNow, false);
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			return true;
		}
		// MT2009_PLUS_SIDEKICK_LURE_V1: what it brought is fought with the
		// skills now (FightPlayerBotSidekickFoe), the packs gathered round it.
		rt.dwLureGatheredUntil = dwNow + PLAYERBOT_SIDEKICK_LURE_GATHERED_MS;
		EndPlayerBotSidekickLure(ch, rt, dwNow, "brought");
		return false;
	}

	// "Czekaj tutaj": it keeps its spot. What comes at it is fought, and - in
	// the default stance - what stands near the spot; the owner, when it is
	// on this map, is defended there as at its side. The drops within reach
	// are picked up, and it walks back to the spot when a fight took it off.
	bool ManagePlayerBotSidekickHold(LPCHARACTER ch, TPlayerBotAIState& state, const TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt, LPCHARACTER owner, DWORD dwNow)
	{
		if (ch->GetMapIndex() != rt.lHoldMap || !ch->GetSectree())
		{
			// Something took it off the map (a warp, a dungeon's end): its spot
			// is gone with it, and it goes back to its owner.
			rt.bHold = false;
			return true;
		}
		LPCHARACTER sameMapOwner = owner && owner->GetMapIndex() == ch->GetMapIndex() ? owner : NULL;
		RememberPlayerBotSidekickAttackers(ch, rt, dwNow);
		if (sameMapOwner && dwNow >= rt.dwNextPartyCheck)
		{
			rt.dwNextPartyCheck = dwNow + PLAYERBOT_SIDEKICK_PARTY_CHECK_MS;
			KeepPlayerBotSidekickInParty(ch, sameMapOwner, dwNow);
		}
		FPlayerBotSidekickFoes foes(ch, sameMapOwner, rt.lHoldX, rt.lHoldY, rt.lHoldMap);
		ch->GetSectree()->ForEachAround(foes);
		if (sameMapOwner && (foes.onOwner || IsPlayerBotSidekickOwnerTargetInFight(ch, sameMapOwner)))
			rt.dwOwnerFightSeenAt = dwNow;
		LPCHARACTER foe = NULL;
		int why = PLAYERBOT_SIDEKICK_FOE_NEARBY;
		if (foes.onOwner && rec.bStance != PLAYERBOT_SIDEKICK_STANCE_PASSIVE)
		{
			foe = foes.onOwner;
			why = PLAYERBOT_SIDEKICK_FOE_AT_OWNER;
		}
		else if (foes.onSelf)
		{
			foe = foes.onSelf;
			why = PLAYERBOT_SIDEKICK_FOE_AT_SELF;
		}
		else if (foes.idle && rec.bStance == PLAYERBOT_SIDEKICK_STANCE_ATTACK)
			foe = foes.idle;
		// MT2009_PLUS_SIDEKICK_DEFEND_V1: another kingdom's attackers of the
		// owner near the spot, or of itself, before any monster.
		{
			int defendWhy = PLAYERBOT_SIDEKICK_FOE_AT_SELF;
			LPCHARACTER defend = FindPlayerBotSidekickDefendFoe(ch, sameMapOwner, rec.bStance, rt.lHoldX, rt.lHoldY,
					defendWhy);
			if (defend)
			{
				foe = defend;
				why = defendWhy;
				if (why == PLAYERBOT_SIDEKICK_FOE_AT_OWNER)
					rt.dwOwnerFightSeenAt = dwNow;
				LogPlayerBotSidekickDefend(ch, sameMapOwner, defend, why, dwNow);
			}
		}
		// The owner near the spot is buffed as at its side, but a waiting
		// companion never walks to it for that.
		if (sameMapOwner && BuffPlayerBotSidekickOwner(ch, state, rec, sameMapOwner, dwNow, false))
			return true;
		// MT2009_PLUS_SIDEKICK_FOLLOW_LOOT_V1: its drops before a new monster
		// nobody fights yet, as a bot's own loot pass does.
		if (foe && why == PLAYERBOT_SIDEKICK_FOE_NEARBY &&
				RunPlayerBotSidekickLootPass(ch, state, sameMapOwner, rt, rec.bLoot, rt.lHoldX, rt.lHoldY, dwNow))
			return true;
		if (foe)
		{
			TakePlayerBotSidekickLootInFight(ch, state, sameMapOwner, rt, rec.bLoot, rt.lHoldX, rt.lHoldY, dwNow);
			return FightPlayerBotSidekickFoe(ch, state, rt, foe, why, dwNow);
		}
		if (state.dwTargetVID != 0)
		{
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
		}
		if (RunPlayerBotSidekickLootPass(ch, state, sameMapOwner, rt, rec.bLoot, rt.lHoldX, rt.lHoldY, dwNow))
			return true;
		const int away = DISTANCE_APPROX(ch->GetX() - rt.lHoldX, ch->GetY() - rt.lHoldY);
		if (away > PLAYERBOT_SIDEKICK_HOLD_LEASH)
		{
			WalkPlayerBotSidekick(ch, rt.lHoldX, rt.lHoldY, dwNow, false);
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			state.dwLastMeaningfulActivityTime = dwNow;
			return true;
		}
		if (ch->IsStateMove())
			ch->Stop();
		state.dwLastMeaningfulActivityTime = dwNow;
		ManagePlayerBotCombatBuffs(ch, state, dwNow, true);
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		return true;
	}

	// "Idz na zakupy" under way: the ordinary town visit runs in the rest of
	// the tick, and this only says when it is over. A visit that never began
	// (nothing to do) and one that has run too long end it too.
	bool ManagePlayerBotSidekickErrand(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt, DWORD dwNow)
	{
		if (!rt.bErrandVisit && state.bVisitingShop)
			rt.bErrandVisit = true;
		// The order may be newer than this tick's clock: a command handled in
		// the same pass (the self-test's) stamps get_dword_time(), later than
		// the dwNow the pass began with, and the unsigned difference wrapped
		// round to four billion - "errand over why=timeout" in the second it
		// began.
		const DWORD elapsed = dwNow >= rt.dwErrandSince ? dwNow - rt.dwErrandSince : 0;
		if (rt.bErrandVisit)
		{
			if (state.bVisitingShop && elapsed < PLAYERBOT_SIDEKICK_ERRAND_MAX_MS)
				return false;
			EndPlayerBotSidekickErrand(ch, state, rec, rt, dwNow, state.bVisitingShop ? "timeout" : "done");
			return true;
		}
		if (elapsed < PLAYERBOT_SIDEKICK_ERRAND_START_MS)
			return false;
		EndPlayerBotSidekickErrand(ch, state, rec, rt, dwNow, "nothing");
		return true;
	}

	// The companion's part of the tick. The trade in either mode; at its
	// owner's side everything else too, and then it owns the tick, so neither
	// the wander nor the target search takes it anywhere the owner is not.
	// Above it in the tick run the duel (it never takes one), the guild war,
	// the Anti-PK protocol, and the upkeep - stats, skills, books, the gear
	// pass, chests - exactly as for any bot.
	// Its owner out of the game with "Gra beze mnie" set: what it held at the
	// owner's side is let go - a spot, an errand, a lure course, and the
	// owner's party, which the population's passes would read as a person's
	// and hold it to - and those passes take it from here, as they take a
	// companion let off the leash.
	void StartPlayerBotSidekickAlone(LPCHARACTER ch, const TPlayerBotSidekick& rec, TPlayerBotSidekickRuntime& rt)
	{
		rt.bAlone = true;
		rt.bHold = false;
		rt.bErrand = false;
		rt.bLureStage = 0;
		rt.dwLureVID = 0;
		if (ch->GetParty())
			LeavePlayerBotParty(ch);
		sys_log(0, "PLAYERBOT_SIDEKICK: owner out of the game, plays alone pid=%u name=%s level=%d owner=%u "
				"owner_level=%u", ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), rec.dwOwnerPID,
				(unsigned int)rec.bOwnerLevel);
	}

	// Its owner back: what it began on its own ends, as a summons ends it
	// (SummonPlayerBotSidekick), and the follow below puts it at the owner's
	// side and back in the party.
	void EndPlayerBotSidekickAlone(LPCHARACTER ch, TPlayerBotAIState& state, const TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt)
	{
		rt.bAlone = false;
		state.bTownVisitPhase = BOT_TOWN_PHASE_NONE;
		state.bMarketTrip = false;
		state.bFishingSession = false;
		sys_log(0, "PLAYERBOT_SIDEKICK: owner back, stops playing alone pid=%u name=%s level=%d owner=%u",
				ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), rec.dwOwnerPID);
	}

	// MT2009_PLUS_SIDEKICK_TRIP_V1: a companion let off the leash ("wolna
	// reka", or "Gra beze mnie" with its owner out) plays as a bot, and a bot of
	// ninety wants the Grotto of Exile 2. The owner's (1 October) said "Ide do
	// Groty Wygnancow 2" in Joan for an hour: the first village's hold kept it
	// there - a companion made at ninety starts the Biologist at his first
	// herb, whose wolves live round Joan (goal=9, make_herb_lv15/lv20, 30
	// September) - and every relog behind its owner's warps started the trip's
	// clocks again. The watch is kept here, outside the runtime a relog drops:
	// held in the village past the first mark, the hold is lifted; still not
	// there at the second, that ground is given up for an hour, the next one
	// down is taken (GetPlayerBotFrontierFallback), and the log and the owner
	// are told why.
	const DWORD PLAYERBOT_SIDEKICK_TRIP_CHECK_MS = 5000;
	const DWORD PLAYERBOT_SIDEKICK_TRIP_UNHOLD_MS = 8 * 60 * 1000;
	const DWORD PLAYERBOT_SIDEKICK_TRIP_GIVE_UP_MS = 20 * 60 * 1000;
	const DWORD PLAYERBOT_SIDEKICK_TRIP_BLOCK_MS = 60 * 60 * 1000;

	struct TPlayerBotSidekickTrip
	{
		long lMap = 0;
		long lFromMap = 0;
		DWORD dwSince = 0;
		DWORD dwNextCheck = 0;
		bool bUnheld = false;
		std::map<long, DWORD> mapBlockedUntil;
	};
	std::map<DWORD, TPlayerBotSidekickTrip> s_mapPlayerBotSidekickTrips;

	bool IsPlayerBotSidekickTripBlocked(LPCHARACTER ch, long mapIndex)
	{
		if (!ch || s_mapPlayerBotSidekickTrips.empty())
			return false;
		std::map<DWORD, TPlayerBotSidekickTrip>::iterator trip = s_mapPlayerBotSidekickTrips.find(ch->GetPlayerID());
		if (trip == s_mapPlayerBotSidekickTrips.end())
			return false;
		std::map<long, DWORD>::iterator blocked = trip->second.mapBlockedUntil.find(mapIndex);
		if (blocked == trip->second.mapBlockedUntil.end())
			return false;
		const DWORD left = blocked->second - get_dword_time();
		if (left == 0 || left > PLAYERBOT_SIDEKICK_TRIP_BLOCK_MS)
		{
			trip->second.mapBlockedUntil.erase(blocked);
			return false;
		}
		return true;
	}

	void WatchPlayerBotSidekickTrip(LPCHARACTER ch, TPlayerBotAIState& state, const TPlayerBotSidekick& rec, DWORD dwNow)
	{
		if (!ch || ch->IsDead())
			return;
		TPlayerBotSidekickTrip& trip = s_mapPlayerBotSidekickTrips[ch->GetPlayerID()];
		if (trip.dwNextCheck != 0 && dwNow - trip.dwNextCheck > 0x80000000U)
			return;
		trip.dwNextCheck = dwNow + PLAYERBOT_SIDEKICK_TRIP_CHECK_MS;
		long here = ch->GetMapIndex();
		if (here >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN)
			here /= 10000;
		const long want = ShouldPlayerBotLeaveForFrontier(ch) ? GetPlayerBotFrontierMapForLevel(ch) : 0;
		if (want == 0 || want == here)
		{
			if (trip.lMap != 0 && trip.lMap == here)
				sys_log(0, "PLAYERBOT_SIDEKICK: trip done pid=%u name=%s to=%ld from=%ld took_s=%u",
						ch->GetPlayerID(), ch->GetName(), trip.lMap, trip.lFromMap, (dwNow - trip.dwSince) / 1000U);
			trip.lMap = 0;
			trip.bUnheld = false;
			return;
		}
		if (want != trip.lMap)
		{
			trip.lMap = want;
			trip.lFromMap = here;
			trip.dwSince = dwNow;
			trip.bUnheld = false;
			sys_log(0, "PLAYERBOT_SIDEKICK: trip set pid=%u name=%s to=%ld from=%ld level=%u",
					ch->GetPlayerID(), ch->GetName(), want, here, (unsigned int)ch->GetLevel());
			return;
		}
		const DWORD waited = dwNow - trip.dwSince;
		const int heldBy = IsPlayerBotM1Map(here) ? GetPlayerBotM1HoldWhy(ch->GetPlayerID(), dwNow) : -1;
		if (!trip.bUnheld && heldBy >= 0 && waited >= PLAYERBOT_SIDEKICK_TRIP_UNHOLD_MS)
		{
			ReleasePlayerBotM1Hold(ch->GetPlayerID());
			state.dwNextWorldTravelTime = dwNow;
			trip.bUnheld = true;
			sys_log(0, "PLAYERBOT_SIDEKICK: trip unheld pid=%u name=%s to=%ld map=%ld waited_s=%u held_by=%s",
					ch->GetPlayerID(), ch->GetName(), want, here, waited / 1000U, PLAYERBOT_M1_HOLD_WHY_KEY[heldBy]);
			return;
		}
		if (waited < PLAYERBOT_SIDEKICK_TRIP_GIVE_UP_MS)
			return;
		trip.mapBlockedUntil[want] = dwNow + PLAYERBOT_SIDEKICK_TRIP_BLOCK_MS;
		trip.lMap = 0;
		trip.bUnheld = false;
		const long next = ShouldPlayerBotLeaveForFrontier(ch) ? GetPlayerBotFrontierMapForLevel(ch) : 0;
		sys_log(0, "PLAYERBOT_SIDEKICK: trip given up pid=%u name=%s to=%ld from=%ld map=%ld pos=(%ld,%ld) waited_s=%u "
				"held_by=%s hosted=%d gold=%lld fee=%d action=%u goal=%u shop=%d next=%ld blocked_min=%u",
				ch->GetPlayerID(), ch->GetName(), want, trip.lFromMap, here, ch->GetX(), ch->GetY(), waited / 1000U,
				heldBy >= 0 ? PLAYERBOT_M1_HOLD_WHY_KEY[heldBy] : "none", IsPlayerBotMapHostedHere(want) ? 1 : 0,
				(long long)ch->GetGold(), GetPlayerBotTeleporterFee(ch), (unsigned int)state.bCurrentAction,
				(unsigned int)state.bLongTermGoal, state.bVisitingShop ? 1 : 0, next,
				PLAYERBOT_SIDEKICK_TRIP_BLOCK_MS / 60000U);
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec.dwOwnerPID);
		if (owner)
		{
			char text[200];
			const char* nextWords = next != 0 ? GetPlayerBotMapDestinationPl(next) : "";
			if (nextWords[0])
				snprintf(text, sizeof(text), "Od %u minut nie moge dojsc %s - na razie odpuszczam i ide %s.",
						waited / 60000U, GetPlayerBotMapDestinationPl(want), nextWords);
			else
				snprintf(text, sizeof(text), "Od %u minut nie moge dojsc %s - na razie odpuszczam i expie tutaj.",
						waited / 60000U, GetPlayerBotMapDestinationPl(want));
			SayPlayerBotSidekick(owner, text);
		}
	}

	// ------------------------------------------- the owner's transformation
	//
	// MT2009_PLUS_SIDEKICK_POLYMORPH_V1: "towarzysz razem z nami uzywa marmura
	// polimorfii jesli ma go w eq" (the owner, 3 October). When its owner
	// transforms - a Polymorph Marble, the Polymorph book or a quest's
	// transformation, whatever puts the owner under AFFECT_POLYMORPH - a
	// companion at the owner's side ("Przywolaj", not let off the leash, not on
	// an errand or at the water) takes a marble of its own from its bag and
	// transforms with it, the way a player's use does: CHARACTER::UseItem, one
	// marble spent, five minutes and the engine's damage bonus (more with its
	// own Polymorph skill).
	//
	// Which marble: one of the owner's own monster if it carries that, or else
	// the highest monster the engine lets it become (ItemProcess_Polymorph
	// refuses a monster at or above its level plus MAX(0, 20 - level*3/10) and
	// throws away a marble whose monster does not exist, so both are asked here
	// first). Never one it holds for its owner (IsPlayerBotSidekickHeld). The
	// "Zablokuj ekwipunek" lock does not stop it: the lock keeps what it wears
	// and was given from the blacksmith, the merchant and the ground, and a
	// marble handed over is there to be used - at the owner's own move.
	//
	// One marble for one transformation of the owner's: should its own run out
	// first (its five minutes against an owner's longer skill, or it fell) it
	// does not spend another. And it ends with the owner's: when the owner is
	// back in its own shape the companion's transformation is taken off at
	// once, as long as it was the one taken for the owner. A transformation of
	// its own for the Reaper or a raid's boss (ManagePlayerBotPolymorph) runs
	// its own course - unless the owner transforms during it, when it is
	// counted as the owner's company and ends with the owner's too. While the
	// owner warps it changes nothing.
	//
	// A transformed companion fights as the bots under a marble already do
	// (IsPlayerBotFightingAsMonster): hand to hand, no skill and no buff, which
	// the engine refuses (char_skill.cpp); its gear is frozen
	// (IsPlayerBotGearFrozen) and it does not mount (StartRiding refuses).
	struct TPlayerBotSidekickPolymorph
	{
		// This transformation of the owner's has had its marble.
		bool bOwnerSeen;
		// The companion's own transformation keeps the owner company.
		bool bWithOwner;
		// The owner heard once that the only marbles it has are too high.
		bool bToldTooHigh;
		DWORD dwNextTry;
		TPlayerBotSidekickPolymorph() : bOwnerSeen(false), bWithOwner(false), bToldTooHigh(false), dwNextTry(0)
		{
		}
	};
	// By companion pid. Outside the runtime, which a logout drops: a
	// companion that left the world for a moment comes back to the same answer.
	std::map<DWORD, TPlayerBotSidekickPolymorph> s_mapPlayerBotSidekickPolymorph;
	const DWORD PLAYERBOT_SIDEKICK_POLYMORPH_RETRY_MS = 5000;
	const DWORD PLAYERBOT_SIDEKICK_POLYMORPH_DISMOUNT_MS = 600;

	// The cell of the marble it takes, or -1. tooHigh: it carries one whose
	// monster its level does not allow yet.
	int FindPlayerBotSidekickPolymorphMarble(LPCHARACTER ch, DWORD ownerMob, bool& tooHigh)
	{
		tooHigh = false;
		const int limit = ch->GetLevel() + MAX(0, 20 - ch->GetLevel() * 3 / 10);
		int best = -1;
		int bestLevel = -1;
		for (int cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetType() != ITEM_POLYMORPH || item->isLocked() || IsPlayerBotSidekickHeld(ch, item))
				continue;
			bool known = false;
			for (size_t i = 0; i < sizeof(PLAYERBOT_POLYMORPH_MARBLE_VNUMS) /
					sizeof(PLAYERBOT_POLYMORPH_MARBLE_VNUMS[0]); ++i)
				if (PLAYERBOT_POLYMORPH_MARBLE_VNUMS[i] == item->GetVnum())
					known = true;
			if (!known)
				continue;
			const DWORD mob = (DWORD)item->GetSocket(0);
			const CMob* pMob = mob != 0 ? CMobManager::instance().Get(mob) : NULL;
			if (!pMob)
				continue;
			if ((int)pMob->m_table.bLevel >= limit)
			{
				tooHigh = true;
				continue;
			}
			if (ownerMob != 0 && mob == ownerMob)
				return cell;
			if ((int)pMob->m_table.bLevel > bestLevel)
			{
				bestLevel = pMob->m_table.bLevel;
				best = cell;
			}
		}
		return best;
	}

	void FollowPlayerBotSidekickOwnerPolymorph(LPCHARACTER ch, TPlayerBotAIState& state, const TPlayerBotSidekick& rec,
			const TPlayerBotSidekickRuntime& rt, DWORD dwNow)
	{
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec.dwOwnerPID);
		std::map<DWORD, TPlayerBotSidekickPolymorph>::iterator it = s_mapPlayerBotSidekickPolymorph.find(ch->GetPlayerID());
		// Nothing under way, and the owner in its own shape: the whole of it for
		// nearly every tick.
		if (it == s_mapPlayerBotSidekickPolymorph.end() && (!owner || !owner->IsPolymorphed()))
			return;
		// A warp, or the owner gone: nothing changes until the owner is back.
		if (!owner || !owner->GetSectree())
			return;
		if (!owner->IsPolymorphed())
		{
			if (it->second.bWithOwner && ch->IsPolymorphed())
			{
				ch->RemoveAffect(AFFECT_POLYMORPH);
				sys_log(0, "PLAYERBOT_SIDEKICK: polymorph ended with the owner's pid=%u name=%s owner=%u still=%d",
						ch->GetPlayerID(), ch->GetName(), rec.dwOwnerPID, ch->IsPolymorphed() ? 1 : 0);
			}
			s_mapPlayerBotSidekickPolymorph.erase(it);
			return;
		}
		TPlayerBotSidekickPolymorph& poly = s_mapPlayerBotSidekickPolymorph[ch->GetPlayerID()];
		// Its own ran out, or it fell: the owner's goes on without it.
		if (poly.bWithOwner && !ch->IsPolymorphed())
		{
			poly.bWithOwner = false;
			sys_log(0, "PLAYERBOT_SIDEKICK: polymorph over before the owner's pid=%u name=%s owner=%u",
					ch->GetPlayerID(), ch->GetName(), rec.dwOwnerPID);
		}
		if (poly.bOwnerSeen)
			return;
		// Already transformed (the Reaper, or back from a logout under the
		// owner's transformation): that one keeps the owner company.
		if (ch->IsPolymorphed())
		{
			poly.bOwnerSeen = true;
			poly.bWithOwner = true;
			return;
		}
		// At the owner's side, and free to use an item.
		if (rec.bMode != PLAYERBOT_SIDEKICK_FOLLOW || IsPlayerBotSidekickPlayingAlone(rec, dwNow) ||
				rt.bErrand || rt.bFishing || rt.bTrading)
			return;
		if (ch->IsDead() || ch->GetExchange() || ch->GetMyShop() || !ch->IsItemLoaded() ||
				owner->GetMapIndex() != ch->GetMapIndex() ||
				DISTANCE_APPROX(ch->GetX() - owner->GetX(), ch->GetY() - owner->GetY()) > PLAYERBOT_SIDEKICK_TELEPORT_DISTANCE)
			return;
		if (dwNow < poly.dwNextTry)
			return;
		poly.dwNextTry = dwNow + PLAYERBOT_SIDEKICK_POLYMORPH_RETRY_MS;
		bool tooHigh = false;
		const int cell = FindPlayerBotSidekickPolymorphMarble(ch, owner->GetPolymorphVnum(), tooHigh);
		if (cell < 0)
		{
			if (tooHigh && !poly.bToldTooHigh)
			{
				poly.bToldTooHigh = true;
				SayPlayerBotSidekick(owner, "Mam marmur polimorfii, ale w tego potwora nie moge sie jeszcze "
						"zmienic - mam za niski poziom.");
			}
			return;
		}
		// ItemProcess_Polymorph refuses a rider: down from the horse or the
		// mount first, the marble a moment later.
		if (ch->IsRiding())
		{
			SetPlayerBotRidingForTravel(ch, state, false, dwNow, "sidekick_polymorph");
			poly.dwNextTry = dwNow + PLAYERBOT_SIDEKICK_POLYMORPH_DISMOUNT_MS;
			return;
		}
		LPITEM item = ch->GetInventoryItem(cell);
		const DWORD vnum = item->GetVnum();
		const DWORD mob = (DWORD)item->GetSocket(0);
		// The marble may be its last of the stack: nothing of it is read after.
		if (!ch->UseItem(TItemPos(INVENTORY, cell)) || !ch->IsPolymorphed())
		{
			sys_log(0, "PLAYERBOT_SIDEKICK: polymorph refused pid=%u name=%s marble=%u mob=%u",
					ch->GetPlayerID(), ch->GetName(), vnum, mob);
			return;
		}
		poly.bOwnerSeen = true;
		poly.bWithOwner = true;
		sys_log(0, "PLAYERBOT_SIDEKICK: polymorphed with the owner pid=%u name=%s owner=%u marble=%u mob=%u owner_mob=%u",
				ch->GetPlayerID(), ch->GetName(), rec.dwOwnerPID, vnum, mob, (unsigned)owner->GetPolymorphVnum());
	}

	// MT2009_PLUS_SIDEKICK_REDRESS_V1: bald after its first summons (upstream
	// 2.2.44, urtopy). A companion comes into the world, and is shown to the
	// players round it, as the character loads - before the db core has sent a
	// single item - and is dressed a moment later: the setup's starter set, the
	// equipment pass, a hairstyle. The client took the parts that followed as
	// updates to a character it had already built and showed it bald. So once
	// it is dressed it is sent to its viewers again, removed and inserted
	// (CEntity::ViewReencode), as it is now - once each time in the world.
	void ResendPlayerBotSidekickView(LPCHARACTER ch, TPlayerBotSidekickRuntime& rt, DWORD dwNow)
	{
		if (rt.bViewResent || !ch->IsItemLoaded() || !ch->GetSectree() || ch->IsDead())
			return;
		if (rt.dwViewResendAt == 0)
		{
			rt.dwViewResendAt = dwNow + PLAYERBOT_SIDEKICK_VIEW_RESEND_MS;
			return;
		}
		if ((int)(dwNow - rt.dwViewResendAt) < 0)
			return;
		rt.bViewResent = true;
		ch->ViewReencode();
		sys_log(0, "PLAYERBOT_SIDEKICK: shown anew once dressed pid=%u name=%s body=%d hair=%d", ch->GetPlayerID(),
				ch->GetName(), ch->GetPart(PART_MAIN), ch->GetPart(PART_HAIR));
	}

	// MT2009_PLUS_SIDEKICK_KEEP_VALUABLES_V1: the owner hears, in a whisper,
	// when the bag holds valuables of the owner's it has not heard of yet (at
	// most every three minutes), so it takes them in the bag window. And a
	// companion never walks with a rank under zero, the only ranks whose death
	// drops what it carries (CHARACTER::ItemDropPenalty, aItemDropPenalty_kor).
	void WatchPlayerBotSidekickValuables(LPCHARACTER ch, const TPlayerBotSidekick& rec, TPlayerBotSidekickRuntime& rt,
			DWORD dwNow)
	{
		if (dwNow < rt.dwNextValuablesCheck || !ch->IsItemLoaded())
			return;
		rt.dwNextValuablesCheck = dwNow + PLAYERBOT_SIDEKICK_VALUABLES_CHECK_MS;
		if (ch->GetRealAlignment() < 0)
		{
			sys_log(0, "PLAYERBOT_SIDEKICK: rank raised to zero pid=%u name=%s rank=%d", ch->GetPlayerID(),
					ch->GetName(), ch->GetRealAlignment());
			ch->UpdateAlignment(-ch->GetRealAlignment());
		}
		std::set<DWORD> present;
		std::vector<LPITEM> fresh;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || !IsPlayerBotSidekickOwnerValuable(ch, item))
				continue;
			present.insert(item->GetID());
			if (rt.setValuablesTold.find(item->GetID()) == rt.setValuablesTold.end())
				fresh.push_back(item);
		}
		for (std::set<DWORD>::iterator it = rt.setValuablesTold.begin(); it != rt.setValuablesTold.end(); )
		{
			if (present.find(*it) == present.end())
				rt.setValuablesTold.erase(it++);
			else
				++it;
		}
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec.dwOwnerPID);
		if (fresh.empty() || !owner || !owner->GetDesc() ||
				(rt.dwValuablesToldAt != 0 && dwNow - rt.dwValuablesToldAt < PLAYERBOT_SIDEKICK_VALUABLES_TELL_MS))
			return;
		rt.dwValuablesToldAt = dwNow;
		std::string names;
		for (size_t i = 0; i < fresh.size() && i < 3; ++i)
		{
			char one[96];
			if (fresh[i]->GetCount() > 1)
				snprintf(one, sizeof(one), "%s%s x%u", i ? ", " : "", fresh[i]->GetName() ? fresh[i]->GetName() : "?",
						(unsigned int)fresh[i]->GetCount());
			else
				snprintf(one, sizeof(one), "%s%s", i ? ", " : "", fresh[i]->GetName() ? fresh[i]->GetName() : "?");
			names += one;
		}
		if (fresh.size() > 3)
		{
			char more[48];
			snprintf(more, sizeof(more), " i %u innych", (unsigned int)(fresh.size() - 3));
			names += more;
		}
		char text[400];
		snprintf(text, sizeof(text), "Mam w plecaku twoje cenne rzeczy: %s. Nie sprzedam ich ani nie odloze do "
				"magazynu - wez je ode mnie (okno Towarzysza, prawy klik oddaje).", names.c_str());
		SayPlayerBotSidekick(owner, text);
		rt.setValuablesTold.insert(present.begin(), present.end());
		sys_log(0, "PLAYERBOT_SIDEKICK: valuables told pid=%u owner=%u new=%u all=%u", ch->GetPlayerID(),
				rec.dwOwnerPID, (unsigned int)fresh.size(), (unsigned int)present.size());
	}

	bool ManagePlayerBotSidekick(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(ch->GetPlayerID());
		if (!rec)
			return false;
		// A new companion does nothing until the bot it was is gone from its bag
		// (OnPlayerBotSidekickLoaded, FinishPlayerBotSidekickSetup).
		if (!rec->bSetupDone)
		{
			if (!ch->IsItemLoaded())
				return true;
			FinishPlayerBotSidekickSetup(ch, *rec);
		}
		KeepPlayerBotSidekickPath(ch, *rec);
		TopUpPlayerBotSidekickSkillPoints(ch);
		TPlayerBotSidekickRuntime& rt = s_mapPlayerBotSidekickRuntime[ch->GetPlayerID()];
		ResendPlayerBotSidekickView(ch, rt, dwNow);	// MT2009_PLUS_SIDEKICK_REDRESS_V1
		FollowPlayerBotSidekickOwnerPolymorph(ch, state, *rec, rt, dwNow);	// MT2009_PLUS_SIDEKICK_POLYMORPH_V1
		ReadPlayerBotSidekickForgetBook(ch, *rec, rt, dwNow);
		// MT2009_PLUS_SIDEKICK_SHOP_ERRAND_V1: a "Kup" errand the call, the leash
		// or the owner's logout ended (they clear bErrand) is settled here - the
		// goods handed over, the rest of the owner's yang given back - and a
		// purchase from a stand answered late is taken in.
		if (rt.shop.bActive && !rt.bErrand)
			SettlePlayerBotSidekickShopErrand(ch, state, *rec, rt, dwNow, "called", false);
		PollPlayerBotSidekickShopPurchase(ch, *rec, rt, dwNow);
		WatchPlayerBotSidekickValuables(ch, *rec, rt, dwNow);	// MT2009_PLUS_SIDEKICK_KEEP_VALUABLES_V1
		if (HandlePlayerBotSidekickTrade(ch, state, *rec, rt, dwNow))
			return true;
		if (KeepPlayerBotSidekickFishing(ch, state, *rec, rt, dwNow))
			return true;
		if (rec->bMode != PLAYERBOT_SIDEKICK_FOLLOW)
		{
			WatchPlayerBotSidekickTrip(ch, state, *rec, dwNow);	// MT2009_PLUS_SIDEKICK_TRIP_V1
			return false;
		}
		if (IsPlayerBotSidekickPlayingAlone(*rec, dwNow))
		{
			if (!rt.bAlone)
				StartPlayerBotSidekickAlone(ch, *rec, rt);
			WatchPlayerBotSidekickTrip(ch, state, *rec, dwNow);	// MT2009_PLUS_SIDEKICK_TRIP_V1
			return false;
		}
		// MT2009_PLUS_SIDEKICK_TRIP_V1: at its owner's side no trip is under
		// way; the next one let off the leash starts its clocks afresh.
		{
			std::map<DWORD, TPlayerBotSidekickTrip>::iterator trip = s_mapPlayerBotSidekickTrips.find(ch->GetPlayerID());
			if (trip != s_mapPlayerBotSidekickTrips.end())
				trip->second.lMap = 0;
		}
		if (rt.bAlone)
			EndPlayerBotSidekickAlone(ch, state, *rec, rt);
		if (rt.bErrand && rt.shop.bActive)
			return ManagePlayerBotSidekickShopErrand(ch, state, *rec, rt, dwNow);
		if (rt.bErrand)
			return ManagePlayerBotSidekickErrand(ch, state, *rec, rt, dwNow);
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec->dwOwnerPID);
		if (!owner || !owner->GetSectree())
		{
			// Keeping a spot, it keeps it while its owner warps.
			if (rt.bHold)
			{
				if (KeepPlayerBotSidekickAlive(ch, state, NULL, dwNow))
					return true;
				return ManagePlayerBotSidekickHold(ch, state, *rec, rt, NULL, dwNow);
			}
			// A warp: it waits where it is for the owner to come back into this
			// world (ManagePlayerBotSidekicks lets it go if the owner does not).
			if (ch->IsStateMove())
				ch->Stop();
			ch->SetVictim(NULL);
			state.dwTargetVID = 0;
			state.dwLastMeaningfulActivityTime = dwNow;
			SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
			return true;
		}
		// The personality pass sits below this one in the tick, so at the
		// owner's side it runs here or never - and the title over the
		// companion's head read "Grinder", decided before it joined the party,
		// where it should read Towarzysz (the operator's screenshots of 25 September).
		ManagePlayerBotPersona(ch, state, dwNow);
		if (KeepPlayerBotSidekickAlive(ch, state, owner, dwNow))
			return true;
		// A piece its owner put on waits out the second and a half after a blow
		// that the engine asks of every equip: the fight holds off that long, and
		// the equipment pass above puts it on.
		if (rt.dwEquipWaitUntil != 0)
		{
			int pinnedWear = -1;
			if (dwNow < rt.dwEquipWaitUntil && FindPlayerBotSidekickPinnedInBag(ch, pinnedWear, false))
			{
				if (ch->IsStateMove())
					ch->Stop();
				state.bEquipPending = true;
				state.dwLastMeaningfulActivityTime = dwNow;
				return true;
			}
			rt.dwEquipWaitUntil = 0;
		}
		if (rt.bHold)
			return ManagePlayerBotSidekickHold(ch, state, *rec, rt, owner, dwNow);
		// Another map of this core, or the owner outran it: beside the owner.
		// A dungeon instance too - the owner is its guide, which a bot on its
		// own never has.
		const bool otherMap = owner->GetMapIndex() != ch->GetMapIndex();
		// MT2009_PLUS_AREZZO_BOTS_V1 (off limits) waited here for an owner in a
		// new dungeon, the Blue Dragon's lair or on an Arezzo map.
		// MT2009_PLUS_BOT_DUNGEONS_ALL_V1: no more - the companion goes with its
		// owner into every dungeon and across the Las to the Jungle's guard (the
		// owner, 4 October); PlacePlayerBotSidekickAt keeps it out of those
		// places only without its owner there.
		const int dist = otherMap ? INT_MAX
				: DISTANCE_APPROX(ch->GetX() - owner->GetX(), ch->GetY() - owner->GetY());
		if (otherMap || dist > PLAYERBOT_SIDEKICK_TELEPORT_DISTANCE)
		{
			if (owner->IsWarping() || owner->IsDead() || dwNow < rt.dwNextCatchUp)
				return true;
			rt.dwNextCatchUp = dwNow + PLAYERBOT_SIDEKICK_CATCH_UP_MS;
			PlacePlayerBotSidekick(ch, state, owner, dwNow, otherMap ? "sidekick_follow" : "sidekick_catch_up");
			return true;
		}
		if (dwNow >= rt.dwNextPartyCheck)
		{
			rt.dwNextPartyCheck = dwNow + PLAYERBOT_SIDEKICK_PARTY_CHECK_MS;
			KeepPlayerBotSidekickInParty(ch, owner, dwNow);
		}
		if (rec->bStance != PLAYERBOT_SIDEKICK_STANCE_PASSIVE && rec->bProtect)
			ProtectPlayerBotSidekickOwner(ch, owner, rt, dwNow);
		RememberPlayerBotSidekickAttackers(ch, rt, dwNow);
		if (ManagePlayerBotSidekickLure(ch, state, *rec, rt, owner, dwNow))
			return true;
		int why = PLAYERBOT_SIDEKICK_FOE_NEARBY;
		bool ownerFighting = false;
		// MT2009_PLUS_SIDEKICK_DEFEND_V1: over a fallen owner it still answers
		// another kingdom's blows at itself.
		LPCHARACTER foe = owner->IsDead()
				? FindPlayerBotSidekickDefendFoe(ch, NULL, rec->bStance, ch->GetX(), ch->GetY(), why)
				: FindPlayerBotSidekickFoe(ch, owner, rec->bStance, why, ownerFighting);
		if (foe && owner->IsDead())
			LogPlayerBotSidekickDefend(ch, owner, foe, why, dwNow);
		if (ownerFighting)
			rt.dwOwnerFightSeenAt = dwNow;
		// MT2009_PLUS_SIDEKICK_SHAMAN_REBUFF_V1: a Shaman fighting from the
		// saddle climbs down for its own and its owner's buffs, puts them all
		// up and rides on - before the loot, the owner's pass and the blow.
		if (RebuffPlayerBotSidekickShaman(ch, state, *rec, rt, owner, foe, dwNow))
			return true;
		// MT2009_PLUS_SIDEKICK_FOLLOW_LOOT_V1: its drops before a monster
		// nobody fights yet - in "atakuj" it went from one to the next and the
		// drops lay till their time ran out; "Wolna reka" (HandleLoot) finishes
		// its drop before it picks a new monster. What is on the owner or on
		// itself, and the owner's own target, still come first.
		if (foe && why == PLAYERBOT_SIDEKICK_FOE_NEARBY &&
				RunPlayerBotSidekickLootPass(ch, state, owner, rt, rec->bLoot, owner->GetX(), owner->GetY(), dwNow))
			return true;
		if (foe)
		{
			if (BuffPlayerBotSidekickOwner(ch, state, *rec, owner, dwNow, false))
				return true;
			TakePlayerBotSidekickLootInFight(ch, state, owner, rt, rec->bLoot, owner->GetX(), owner->GetY(), dwNow);
			return FightPlayerBotSidekickFoe(ch, state, rt, foe, why, dwNow);
		}
		if (state.dwTargetVID != 0)
		{
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
		}
		// Beside a fight it is not to take part in, the owner's buffs are all it
		// does there, and they come before the loot and the walk after the owner.
		if (IsPlayerBotSidekickOwnerFighting(rt, dwNow) &&
				BuffPlayerBotSidekickOwner(ch, state, *rec, owner, dwNow, true))
			return true;
		if (dist <= PLAYERBOT_SIDEKICK_NPC_RANGE + PLAYERBOT_SIDEKICK_FOLLOW_DISTANCE && dwNow >= rt.dwNextService)
		{
			rt.dwNextService = dwNow + PLAYERBOT_SIDEKICK_SERVICE_INTERVAL_MS;
			if (ServePlayerBotSidekickAtNpcs(ch, state, owner, dwNow, rt))
				return true;
		}
		if (RunPlayerBotSidekickLootPass(ch, state, owner, rt, rec->bLoot, owner->GetX(), owner->GetY(), dwNow))
			return true;
		// A bag near full is its owner's to know: the merchant takes the scrap
		// and, from a bag under pressure, the goods only when the owner stands
		// at one or sends it on an errand, and the owner cannot see the bag
		// without the window.
		if ((rt.dwBagFullToldAt == 0 || dwNow - rt.dwBagFullToldAt >= PLAYERBOT_SIDEKICK_BAG_FULL_TELL_MS) &&
				IsPlayerBotBagFull(ch))
		{
			rt.dwBagFullToldAt = dwNow;
			// MT2009_PLUS_SIDEKICK_KEEP_VALUABLES_V1: only the scrap goes.
			SayPlayerBotSidekick(owner, "Mam prawie pelny plecak. Stan przy handlarzu albo szepnij \"zakupy\" - "
					"sprzedam tylko zlom, cenne rzeczy trzymam dla ciebie. Wez je z mojego plecaka (okno Towarzysza).");
			sys_log(0, "PLAYERBOT_SIDEKICK: bag near full, owner told pid=%u name=%s free=%d", ch->GetPlayerID(),
					ch->GetName(), CountPlayerBotFreeInventoryCells(ch));
		}
		if (dist > PLAYERBOT_SIDEKICK_FOLLOW_DISTANCE)
		{
			long x = 0, y = 0;
			GetPlayerBotSidekickSpot(ch, owner, x, y);
			WalkPlayerBotSidekick(ch, x, y, dwNow, dist > 1500);
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			state.dwLastMeaningfulActivityTime = dwNow;
			return true;
		}
		// Beside the owner with nothing to fight: the owner's buffs, its own,
		// and standing where it is.
		if (ch->IsStateMove() && dist <= PLAYERBOT_SIDEKICK_FOLLOW_DISTANCE / 2)
			ch->Stop();
		state.dwLastMeaningfulActivityTime = dwNow;
		if (BuffPlayerBotSidekickOwner(ch, state, *rec, owner, dwNow, true))
			return true;
		ManagePlayerBotCombatBuffs(ch, state, dwNow, true);
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		return true;
	}
}
