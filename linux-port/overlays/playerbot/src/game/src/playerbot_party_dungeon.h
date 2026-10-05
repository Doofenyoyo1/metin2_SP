#ifndef __INC_METIN2_PLAYERBOT_PARTY_DUNGEON_H__
#define __INC_METIN2_PLAYERBOT_PARTY_DUNGEON_H__

// MT2009_PLUS_BOT_DUNGEONS_ALL_V1 - a person's party bots in every dungeon.
//
// "Trzeba to naprawic aby boty mogly wchodzic na wszystkie dungeony ... Musza
// byc wszystkie 3 nowe dungi Arezzo + dungi z podstawowej wersji gry" (the
// owner, 4 October), and: any bot a person invites to the party the normal way
// goes into the dungeon with the person, fights there and comes out with it.
//
// The Demon Tower and the Catacomb jump the whole party into their instance
// themselves (CDungeon::JumpParty, which reaches a bot through WarpBot), and
// the Monkey and Spider Dungeons are open maps the follow pass walks to. The
// rest - Leze Smoka (208), Czysciec Ognia (351), Lodowa Kraina (352), the
// Biblioteka Wiedzy (363), the Wzgorze Wukonga (364), the Ruiny Skorpiona (365)
// and the Starozytna Dzungla (366) - open a new instance at their guard with
// d.new_jump_pids for the people of the party alone: a person pays the fee and
// the daily limit and is let in by level and cooldown, a bot is none of that.
// All seven sit on game1 now (m2-render-config), beside every bot, so:
//
//   - the way in: a bot whose party has a person standing in an instance of
//     one of them (the leader first, any person of the party else) is put
//     beside that person, in the same instance, a few seconds after the
//     person arrived - CPlayerBotManager::WarpBot, the map change a dungeon's
//     own jump makes for a bot, with the dungeon membership a reconnecting
//     player gets. Its way out is the person's: the warp location the quest
//     set at the person's login, so the dungeon's d.exit_all at the end sends
//     the bot where it sends the person;
//   - inside, this pass owns the tick: the Demon Tower's fight
//     (FightPlayerBotTowerObjective, KeepPlayerBotTowerAlive,
//     BuffPlayerBotTowerFellows) and the Arezzo cohort's boss break-off and
//     unstick, round the person instead of round a pack - what attacks the
//     bot or the person, what the person hits, the stones, the nearest
//     monster - and between the fights at the person's side. A person a quest
//     jumped into another room (Razador's hall, Nemere's floors) is caught up
//     with the same way the dungeon jumped him. No loot pass runs inside: the
//     drops are the person's;
//   - the dungeons' own things a bot can do are done: a seal or a key the bot
//     got by the last blow (the Arezzo waves' seals, Nemere's keys) is used at
//     once, a Golden Cog, a Maat stone or an Ice Crystal it dropped is picked
//     up and given to the stele or the seal it is for - the quest's own
//     handlers, as a person's use and drag would. What only the person can do
//     (the statue's tasks, the lion's start) is the person's;
//   - the way out: the dungeon's d.exit_all, or - with no person of the party
//     left in the instance for PLAYERBOT_PDG_LEAVE_GRACE_MS (the person left,
//     logged out, kicked the bot) - to the person elsewhere on this core, else
//     to the way out it was given, the quest's exit, or Orc Valley. A bot is
//     never left in an instance on its own. A dead bot stands up where it
//     fell (restart_here) and walks back to the person.
//
// The player's own companion (playerbot_sidekick.h) places itself beside its
// owner, a dungeon instance included; here it only uses and hands in the
// dungeons' items. The Arezzo dungeon test cohort (playerbot_arezzo_dungeon_bots.h)
// runs its own instances and is not this pass's; nor are the bots' own dungeon
// runs (playerbot_dungeon_runs.h, MT2009_PLUS_BOT_DUNGEON_RUNS_V1), which use
// its fight round their leader (a bot - its target is its AI's) and its items.
//
// An implementation fragment in the sense playerbot_types.h describes:
// include it once, after playerbot_arezzo_dungeon_bots.h (whose scan, boss
// break-off and unstick it borrows, and the tower's fight with them).

namespace
{
	const DWORD PLAYERBOT_PDG_FOLLOW_RETRY_MS = 3000;
	// A person's warp between two rooms (a loading screen: the character is
	// off the map until the client is back), or a reconnect, is not the
	// person gone: the bot waits this long before it leaves.
	const DWORD PLAYERBOT_PDG_LEAVE_GRACE_MS = 20000;
	// What the bots fight: within this of the person...
	const int PLAYERBOT_PDG_HUNT_RANGE = 1800;
	// ...and whatever attacks the bot or the person within this of the bot.
	const int PLAYERBOT_PDG_THREAT_RANGE = 1500;
	// Between the fights it stands this near the person.
	const int PLAYERBOT_PDG_STAY_DISTANCE = 400;
	// Further than this from the person and no route to him, or further than
	// the second figure at all: beside the person at once.
	const int PLAYERBOT_PDG_CATCH_UP_DISTANCE = 3500;
	const int PLAYERBOT_PDG_FAR_DISTANCE = 8000;
	const DWORD PLAYERBOT_PDG_CATCH_UP_RETRY_MS = 5000;
	// The dungeons' items: looked at once a second, a use or a hand-over tried
	// again after this.
	const DWORD PLAYERBOT_PDG_ITEM_SCAN_MS = 1000;
	const DWORD PLAYERBOT_PDG_ITEM_RETRY_MS = 3000;
	const int PLAYERBOT_PDG_ITEM_RANGE = 2500;
	// CHARACTER::CanReceiveItem takes a hand-over from 2000; the bot walks closer.
	const int PLAYERBOT_PDG_GIVE_DISTANCE = 700;
	const int PLAYERBOT_PDG_PICKUP_DISTANCE = 200;
	const int PLAYERBOT_PDG_POTION_HP = 70;
	const int PLAYERBOT_PDG_POTION_SP = 40;

	// The items a bot uses as soon as it holds one inside: the seals of the
	// Biblioteka and the three Arezzo dungeons' waves (given to the last
	// killer) and Nemere's two keys (dropped for the killer).
	const DWORD PLAYERBOT_PDG_USE_ITEMS[] = { 30765, 30766, 30767, 30768, 30760, 30762 };
	// The items a bot hands to a dungeon's NPC: Razador's Golden Cog and Maat
	// stones to the Steles of Isfet, Nemere's Ice Crystals to the seals.
	struct TPlayerBotPdgGive
	{
		DWORD dwVnum;
		DWORD dwNpc;
	};
	const TPlayerBotPdgGive PLAYERBOT_PDG_GIVE_ITEMS[] = { { 30329, 20386 }, { 30330, 20386 }, { 30761, 20398 } };
	// The Blue Dragon takes no damage while one of his four stones stands
	// (server-patches/bluedragon): the stones first.
	const DWORD PLAYERBOT_PDG_BLUE_DRAGON = 2493;

	// What the dungeon exits of the quests' cfg() are (cells): the last way
	// out when the bot has none of its own.
	struct TPlayerBotPdgExit
	{
		long lMap;
		long lExitMap;
		long lCellX;
		long lCellY;
	};
	const TPlayerBotPdgExit PLAYERBOT_PDG_EXITS[] = {
		{ 208, 73, 1536 + 275, 12032 + 175 },	// blue_dragon_lair
		{ 351, 62, 5888 + 91, 6144 + 916 },	// razador_dungeon
		{ 352, 61, 3584 + 738, 1536 + 119 },	// nemere_dungeon
		{ 363, 64, 2560 + 292, 6656 + 1452 },	// biblioteka_wiedzy
		{ 364, 65, 5537, 1450 },		// wzgorze_wukonga
		{ 365, 62, 5989, 7058 },		// ruiny_skorpiona
		{ 366, 362, 3783, 3954 },		// starozytna_dzungla (the Las)
	};

	struct TPlayerBotPdgBot
	{
		long lInstance;
		DWORD dwEnteredAt;
		DWORD dwNextFollow;
		DWORD dwPersonGoneSince;
		DWORD dwNextCatchUp;
		DWORD dwNextItemScan;
		DWORD dwNextItemUse;
		DWORD dwKills;
		TPlayerBotPdgBot() : lInstance(0), dwEnteredAt(0), dwNextFollow(0), dwPersonGoneSince(0), dwNextCatchUp(0),
				dwNextItemScan(0), dwNextItemUse(0), dwKills(0) {}
	};
	std::map<DWORD, TPlayerBotPdgBot> s_mapPlayerBotPdg;
	unsigned int s_uPlayerBotPdgEntered = 0;

	// ------------------------------------------------------------ the person

	bool IsPlayerBotPdgPerson(LPCHARACTER c)
	{
		return c && c->IsPC() && c->GetDesc() && !c->GetDesc()->IsBot() && c->GetSectree();
	}

	struct FPlayerBotPartyPerson
	{
		LPCHARACTER m_me;
		long m_lMap;
		bool m_bDungeon;
		LPCHARACTER m_found;
		FPlayerBotPartyPerson(LPCHARACTER me, long map, bool dungeon) : m_me(me), m_lMap(map), m_bDungeon(dungeon), m_found(NULL) {}
		bool Fits(LPCHARACTER c) const
		{
			if (c == m_me || !IsPlayerBotPdgPerson(c))
				return false;
			if (m_bDungeon)
				return c->GetMapIndex() >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN && IsPlayerBotPartyDungeonMap(c->GetMapIndex());
			return m_lMap == 0 || c->GetMapIndex() == m_lMap;
		}
		void operator()(LPCHARACTER c)
		{
			if (!m_found && Fits(c))
				m_found = c;
		}
	};

	LPCHARACTER FindPlayerBotPartyPersonWhere(LPCHARACTER ch, long mapIndex, bool dungeon)
	{
		if (!ch)
			return NULL;
		LPPARTY party = ch->GetParty();
		if (!party)
			return NULL;
		FPlayerBotPartyPerson finder(ch, mapIndex, dungeon);
		LPCHARACTER leader = party->GetLeaderCharacter();
		if (leader && finder.Fits(leader))
			return leader;
		party->ForEachOnlineMember(finder);
		return finder.m_found;
	}

	LPCHARACTER FindPlayerBotPartyPerson(LPCHARACTER ch, long mapIndex)
	{
		return FindPlayerBotPartyPersonWhere(ch, mapIndex, false);
	}

	// The person of the bot's party in a party dungeon's instance on this core.
	LPCHARACTER FindPlayerBotPartyPersonInDungeon(LPCHARACTER ch)
	{
		return FindPlayerBotPartyPersonWhere(ch, 0, true);
	}

	bool IsPlayerBotDungeonPartyMove(LPCHARACTER ch, long targetMap, const char* reason)
	{
		if (!ch)
			return false;
		// MT2009_PLUS_BOT_DUNGEON_RUNS_V1: the bots' own runs - to the
		// gathering, into the run's instance and home again.
		if (IsPlayerBotDungeonRunMove(ch, targetMap, reason))
			return true;
		const long fromMap = ch->GetMapIndex();
		// Out of a party dungeon's instance: always - a bot is never kept in one.
		if (fromMap >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN && IsPlayerBotPartyDungeonMap(fromMap) &&
				targetMap != fromMap && targetMap < PLAYERBOT_INSTANCE_MAP_INDEX_MIN)
			return true;
		if (!reason || !ch->GetParty())
			return false;
		const bool jump = strcmp(reason, "dungeon_jump") == 0;
		if (!jump && strcmp(reason, "follow_leader") != 0 && strncmp(reason, "party_dungeon", 13) != 0 &&
				strcmp(reason, "warpset") != 0)
			return false;
		// Onto the map a person of its party stands on.
		if (FindPlayerBotPartyPerson(ch, targetMap))
			return true;
		// A dungeon's own jump of a person's party into a party dungeon.
		return jump && targetMap >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN && IsPlayerBotPartyDungeonMap(targetMap) &&
				FindPlayerBotPartyPerson(ch, 0) != NULL;
	}

	// ------------------------------------------------------------ in and out

	const TPlayerBotPdgExit* GetPlayerBotPdgExit(long mapIndex)
	{
		const long base = mapIndex >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN ? mapIndex / 10000 : mapIndex;
		for (size_t i = 0; i < sizeof(PLAYERBOT_PDG_EXITS) / sizeof(PLAYERBOT_PDG_EXITS[0]); ++i)
			if (PLAYERBOT_PDG_EXITS[i].lMap == base)
				return &PLAYERBOT_PDG_EXITS[i];
		return NULL;
	}

	// The dungeons' items are the run's: what a bot still carries when it is
	// out goes, as the quests take them from a person at the next login.
	void DropPlayerBotPdgItems(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return;
		for (size_t i = 0; i < sizeof(PLAYERBOT_PDG_USE_ITEMS) / sizeof(PLAYERBOT_PDG_USE_ITEMS[0]); ++i)
		{
			const ITEM_COUNT n = ch->CountSpecifyItem(PLAYERBOT_PDG_USE_ITEMS[i]);
			if (n > 0)
				ch->RemoveSpecifyItem(PLAYERBOT_PDG_USE_ITEMS[i], n);
		}
		for (size_t i = 0; i < sizeof(PLAYERBOT_PDG_GIVE_ITEMS) / sizeof(PLAYERBOT_PDG_GIVE_ITEMS[0]); ++i)
		{
			const ITEM_COUNT n = ch->CountSpecifyItem(PLAYERBOT_PDG_GIVE_ITEMS[i].dwVnum);
			if (n > 0)
				ch->RemoveSpecifyItem(PLAYERBOT_PDG_GIVE_ITEMS[i].dwVnum, n);
		}
	}

	// Beside the person in the person's instance: the dungeon's own jump for
	// a bot (WarpBot), a step off the person by pid.
	bool PutPlayerBotBesidePdgPerson(LPCHARACTER ch, LPCHARACTER person)
	{
		static const int kSide[8][2] = {
			{ 150, 0 }, { 106, 106 }, { 0, 150 }, { -106, 106 }, { -150, 0 }, { -106, -106 }, { 0, -150 }, { 106, -106 } };
		const long map = person->GetMapIndex();
		const int* side = kSide[ch->GetPlayerID() % 8];
		long x = person->GetX() + side[0];
		long y = person->GetY() + side[1];
		LPSECTREE tree = SECTREE_MANAGER::instance().Get(map, x, y);
		if (!tree || tree->IsAttr(x, y, ATTR_BLOCK | ATTR_OBJECT))
		{
			x = person->GetX();
			y = person->GetY();
		}
		return CPlayerBotManager::instance().WarpBot(ch, x, y, map);
	}

	// Into the person's instance.
	bool FollowPlayerBotPersonIntoDungeon(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER person, DWORD dwNow)
	{
		TPlayerBotPdgBot& b = s_mapPlayerBotPdg[ch->GetPlayerID()];
		if (dwNow < b.dwNextFollow || person->IsWarping() || person->IsDead())
			return false;
		const long target = person->GetMapIndex();
		if (!IsPlayerBotMapHostedHere(target) || !CDungeonManager::instance().FindByMapIndex(target))
			return false;
		b.dwNextFollow = dwNow + PLAYERBOT_PDG_FOLLOW_RETRY_MS;
		const long from = ch->GetMapIndex();
		const long fromX = ch->GetX(), fromY = ch->GetY();
		// Its way out is the person's: the warp location the quest set at the
		// person's login (d.exit_all reads it), else where the bot stood.
		const PIXEL_POSITION& w = person->GetWarpPosition();
		const long exitMap = w.x > 0 && w.y > 0 ? SECTREE_MANAGER::instance().GetMapIndex(w.x, w.y) : 0;
		if (exitMap > 0 && exitMap < PLAYERBOT_INSTANCE_MAP_INDEX_MIN && !IsPlayerBotPartyDungeonMap(exitMap))
			ch->SetWarpLocation(exitMap, w.x / 100, w.y / 100);
		else if (from < PLAYERBOT_INSTANCE_MAP_INDEX_MIN)
			ch->SetWarpLocation(from, fromX / 100, fromY / 100);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		if (!PutPlayerBotBesidePdgPerson(ch, person))
		{
			PlayerBotLogThrottled("pdg_enter_refused", dwNow,
					"PLAYERBOT_DUNGEON: party bot not let in pid=%u name=%s person=%s from=%ld instance=%ld",
					ch->GetPlayerID(), ch->GetName(), person->GetName(), from, target);
			return false;
		}
		b.lInstance = target;
		b.dwEnteredAt = dwNow;
		b.dwPersonGoneSince = 0;
		b.dwNextCatchUp = dwNow + PLAYERBOT_PDG_CATCH_UP_RETRY_MS;
		++s_uPlayerBotPdgEntered;
		sys_log(0, "PLAYERBOT_DUNGEON: party bot in pid=%u name=%s level=%u person=%s from=%ld instance=%ld exit=%ld entered_total=%u",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)ch->GetLevel(), person->GetName(), from, target,
				exitMap, s_uPlayerBotPdgEntered);
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		return true;
	}

	LPCHARACTER PickPlayerBotPdgFoe(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER person, DWORD dwNow);

	// No person of the party in the instance any more: out, to the person or
	// by the way out.
	bool LeavePlayerBotPartyDungeon(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotPdgBot& b, DWORD dwNow)
	{
		if (b.dwPersonGoneSince == 0)
			b.dwPersonGoneSince = dwNow;
		if (dwNow - b.dwPersonGoneSince < PLAYERBOT_PDG_LEAVE_GRACE_MS)
		{
			// The person's warp between two rooms: standing, drinking, and
			// fighting back what attacks it.
			if (KeepPlayerBotTowerAlive(ch, state, dwNow, PLAYERBOT_PDG_POTION_HP, PLAYERBOT_PDG_POTION_SP))
				return true;
			LPCHARACTER foe = PickPlayerBotPdgFoe(ch, state, ch, dwNow);
			if (foe && foe->GetVictim() == ch)
				return FightPlayerBotTowerObjective(ch, state, foe, dwNow);
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			if (ch->IsStateMove())
				ch->Stop();
			SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
			return true;
		}
		if (dwNow < b.dwNextFollow)
			return true;
		const long map = ch->GetMapIndex();
		const char* how = NULL;
		long to = 0;
		// The person in another party dungeon's instance (a new run): there.
		LPCHARACTER person = FindPlayerBotPartyPersonInDungeon(ch);
		if (person && person->GetMapIndex() != map && !person->IsWarping() && FollowPlayerBotPersonIntoDungeon(ch, state, person, dwNow))
			return true;
		b.dwNextFollow = dwNow + PLAYERBOT_PDG_FOLLOW_RETRY_MS;
		// The person elsewhere on this core: to the person.
		person = FindPlayerBotPartyPerson(ch, 0);
		if (person && !person->IsWarping() && person->GetMapIndex() < PLAYERBOT_INSTANCE_MAP_INDEX_MIN &&
				IsPlayerBotMapHostedHere(person->GetMapIndex()) &&
				TransitionPlayerBotMap(ch, state, person->GetMapIndex(), person->GetX(), person->GetY(), dwNow, "party_dungeon_out"))
		{
			how = "to_person";
			to = person->GetMapIndex();
		}
		// The way out it was given (the person's, at the way in).
		if (!how)
		{
			const PIXEL_POSITION& w = ch->GetWarpPosition();
			const long exitMap = w.x > 0 && w.y > 0 ? SECTREE_MANAGER::instance().GetMapIndex(w.x, w.y) : 0;
			if (exitMap > 0 && exitMap < PLAYERBOT_INSTANCE_MAP_INDEX_MIN && !IsPlayerBotPartyDungeonMap(exitMap) &&
					IsPlayerBotMapHostedHere(exitMap) &&
					TransitionPlayerBotMap(ch, state, exitMap, w.x, w.y, dwNow, "party_dungeon_exit"))
			{
				how = "saved_exit";
				to = exitMap;
			}
		}
		// The quest's exit.
		if (!how)
		{
			const TPlayerBotPdgExit* e = GetPlayerBotPdgExit(map);
			if (e && IsPlayerBotMapHostedHere(e->lExitMap) &&
					TransitionPlayerBotMap(ch, state, e->lExitMap, e->lCellX * 100, e->lCellY * 100, dwNow, "party_dungeon_exit"))
			{
				how = "quest_exit";
				to = e->lExitMap;
			}
		}
		// Orc Valley, the ground every party dungeon's way lies across.
		if (!how)
		{
			long x = 0, y = 0;
			GetPlayerBotFrontierArrivalFor(ch, PLAYERBOT_MAP_ORC_VALLEY, x, y);
			if (IsPlayerBotMapHostedHere(PLAYERBOT_MAP_ORC_VALLEY) &&
					TransitionPlayerBotMap(ch, state, PLAYERBOT_MAP_ORC_VALLEY, x, y, dwNow, "party_dungeon_exit"))
			{
				how = "orc_valley";
				to = PLAYERBOT_MAP_ORC_VALLEY;
			}
		}
		if (!how)
		{
			PlayerBotLogThrottled("pdg_leave_failed", dwNow,
					"PLAYERBOT_DUNGEON: party bot cannot leave pid=%u name=%s instance=%ld", ch->GetPlayerID(), ch->GetName(), map);
			return true;
		}
		sys_log(0, "PLAYERBOT_DUNGEON: party bot out pid=%u name=%s instance=%ld to=%ld via=%s inside_s=%u kills=%u",
				ch->GetPlayerID(), ch->GetName(), map, to, how, b.dwEnteredAt ? (dwNow - b.dwEnteredAt) / 1000 : 0, b.dwKills);
		return true;
	}

	// ------------------------------------------------------------ the dungeons' items

	struct FPlayerBotPdgNpc
	{
		LPCHARACTER m_me;
		DWORD m_dwRace;
		LPCHARACTER m_found;
		int m_iBest;
		FPlayerBotPdgNpc(LPCHARACTER me, DWORD race) : m_me(me), m_dwRace(race), m_found(NULL), m_iBest(INT_MAX) {}
		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER c = (LPCHARACTER)ent;
			if (c->IsPC() || c->IsDead() || c->GetRaceNum() != m_dwRace || c->GetMapIndex() != m_me->GetMapIndex())
				return;
			const int d = DISTANCE_APPROX(c->GetX() - m_me->GetX(), c->GetY() - m_me->GetY());
			if (d < m_iBest)
			{
				m_iBest = d;
				m_found = c;
			}
		}
	};

	bool IsPlayerBotPdgItemVnum(DWORD vnum)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_PDG_USE_ITEMS) / sizeof(PLAYERBOT_PDG_USE_ITEMS[0]); ++i)
			if (PLAYERBOT_PDG_USE_ITEMS[i] == vnum)
				return true;
		for (size_t i = 0; i < sizeof(PLAYERBOT_PDG_GIVE_ITEMS) / sizeof(PLAYERBOT_PDG_GIVE_ITEMS[0]); ++i)
			if (PLAYERBOT_PDG_GIVE_ITEMS[i].dwVnum == vnum)
				return true;
		return false;
	}

	// A dungeon's item on the ground the bot may take: its own drop (the
	// quests drop them for the killer), or one nobody owns any more. A
	// person's is the person's.
	struct FPlayerBotPdgGroundItem
	{
		LPCHARACTER m_me;
		LPITEM m_found;
		int m_iBest;
		explicit FPlayerBotPdgGroundItem(LPCHARACTER me) : m_me(me), m_found(NULL), m_iBest(PLAYERBOT_PDG_ITEM_RANGE + 1) {}
		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_ITEM))
				return;
			LPITEM item = (LPITEM)ent;
			if (!item->GetSectree() || item->GetMapIndex() != m_me->GetMapIndex() || !IsPlayerBotPdgItemVnum(item->GetVnum()) ||
					!item->IsOwnership(m_me))
				return;
			const int d = DISTANCE_APPROX(item->GetX() - m_me->GetX(), item->GetY() - m_me->GetY());
			if (d < m_iBest)
			{
				m_iBest = d;
				m_found = item;
			}
		}
	};

	// Used, handed in or picked up: true when it took the tick.
	bool ManagePlayerBotPdgItems(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotPdgBot& b, DWORD dwNow)
	{
		if (dwNow < b.dwNextItemScan || ch->IsDead() || state.bRecoveringAfterDeath || !ch->IsItemLoaded() || !ch->GetSectree())
			return false;
		b.dwNextItemScan = dwNow + PLAYERBOT_PDG_ITEM_SCAN_MS;
		const long map = ch->GetMapIndex();
		// A seal or a key: its quest's use handler, which says itself whether
		// now is its time.
		if (dwNow >= b.dwNextItemUse)
			for (size_t i = 0; i < sizeof(PLAYERBOT_PDG_USE_ITEMS) / sizeof(PLAYERBOT_PDG_USE_ITEMS[0]); ++i)
			{
				const int cell = FindPlayerBotTowerItemCell(ch, PLAYERBOT_PDG_USE_ITEMS[i]);
				if (cell < 0)
					continue;
				b.dwNextItemUse = dwNow + PLAYERBOT_PDG_ITEM_RETRY_MS;
				if (ch->IsStateMove())
					ch->Stop();
				const bool ok = ch->UseItem(TItemPos(INVENTORY, (WORD)cell));
				sys_log(0, "PLAYERBOT_DUNGEON: party bot used pid=%u name=%s vnum=%u instance=%ld ok=%d",
						ch->GetPlayerID(), ch->GetName(), (unsigned int)PLAYERBOT_PDG_USE_ITEMS[i], map, ok ? 1 : 0);
				return true;
			}
		// A cog, a stone, a crystal: to the nearest NPC that takes it.
		for (size_t i = 0; i < sizeof(PLAYERBOT_PDG_GIVE_ITEMS) / sizeof(PLAYERBOT_PDG_GIVE_ITEMS[0]); ++i)
		{
			const int cell = FindPlayerBotTowerItemCell(ch, PLAYERBOT_PDG_GIVE_ITEMS[i].dwVnum);
			if (cell < 0)
				continue;
			LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(map);
			if (!pMap)
				break;
			FPlayerBotPdgNpc finder(ch, PLAYERBOT_PDG_GIVE_ITEMS[i].dwNpc);
			pMap->for_each(finder);
			if (!finder.m_found)
				continue;
			LPCHARACTER npc = finder.m_found;
			if (finder.m_iBest > PLAYERBOT_PDG_GIVE_DISTANCE)
			{
				state.dwTargetVID = 0;
				ch->SetVictim(NULL);
				b.dwNextItemScan = dwNow + 500;
				SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
				MovePlayerBot(ch, npc->GetX(), npc->GetY(), dwNow, 8, true, false);
				return true;
			}
			if (dwNow < b.dwNextItemUse)
				return false;
			b.dwNextItemUse = dwNow + PLAYERBOT_PDG_ITEM_RETRY_MS;
			if (ch->IsStateMove())
				ch->Stop();
			const bool ok = ch->GiveItem(npc, TItemPos(INVENTORY, (WORD)cell));
			sys_log(0, "PLAYERBOT_DUNGEON: party bot handed in pid=%u name=%s vnum=%u npc=%u instance=%ld ok=%d",
					ch->GetPlayerID(), ch->GetName(), (unsigned int)PLAYERBOT_PDG_GIVE_ITEMS[i].dwVnum,
					(unsigned int)npc->GetRaceNum(), map, ok ? 1 : 0);
			return true;
		}
		// Its own drop of one on the ground.
		FPlayerBotPdgGroundItem ground(ch);
		ch->GetSectree()->ForEachAround(ground);
		if (!ground.m_found || ch->GetEmptyInventory(1) < 0)
			return false;
		LPITEM item = ground.m_found;
		if (ground.m_iBest > PLAYERBOT_PDG_PICKUP_DISTANCE)
		{
			// Not away from what attacks it.
			if (ch->GetVictim() && !ch->GetVictim()->IsDead() && ch->GetVictim()->GetVictim() == ch)
				return false;
			b.dwNextItemScan = dwNow + 400;
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			MovePlayerBot(ch, item->GetX(), item->GetY(), dwNow, 2, true, false);
			return true;
		}
		const DWORD vnum = item->GetVnum();
		if (ch->IsStateMove())
			ch->Stop();
		const bool ok = ch->PickupItem(item->GetVID());
		sys_log(0, "PLAYERBOT_DUNGEON: party bot picked up pid=%u name=%s vnum=%u instance=%ld ok=%d",
				ch->GetPlayerID(), ch->GetName(), (unsigned int)vnum, map, ok ? 1 : 0);
		return true;
	}

	// ------------------------------------------------------------ the fight

	bool IsPlayerBotPdgFoe(LPCHARACTER ch, LPCHARACTER c)
	{
		return c && !c->IsPC() && !c->IsDead() && (c->IsMonster() || c->IsStone()) &&
				c->GetMapIndex() == ch->GetMapIndex() && !IsPlayerBotEventStone(c->GetRaceNum());
	}

	// What attacks the bot or the person, what the person hits, the stones,
	// the nearest monster round the person - the target in hand kept while
	// nothing with the better claim comes.
	LPCHARACTER PickPlayerBotPdgFoe(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER person, DWORD dwNow)
	{
		const long map = ch->GetMapIndex();
		const TPlayerBotArzDgScan& scan = ScanPlayerBotArzDg(map, dwNow);
		bool dragonShielded = false;
		if (map / 10000 == 208)
			for (size_t i = 0; i < scan.foes.size() && !dragonShielded; ++i)
				dragonShielded = scan.foes[i].dwRace >= 8031 && scan.foes[i].dwRace <= 8034;
		LPCHARACTER threat = NULL, stone = NULL, any = NULL;
		int dThreat = INT_MAX, dStone = INT_MAX, dAny = INT_MAX;
		for (size_t i = 0; i < scan.foes.size(); ++i)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().Find(scan.foes[i].dwVID);
			if (!IsPlayerBotPdgFoe(ch, c))
				continue;
			if (dragonShielded && c->GetRaceNum() == PLAYERBOT_PDG_BLUE_DRAGON)
				continue;
			const int dBot = DISTANCE_APPROX(ch->GetX() - c->GetX(), ch->GetY() - c->GetY());
			const int dPerson = DISTANCE_APPROX(person->GetX() - c->GetX(), person->GetY() - c->GetY());
			LPCHARACTER v = c->GetVictim();
			if ((v == ch || v == person) && dBot <= PLAYERBOT_PDG_THREAT_RANGE && dBot < dThreat)
			{
				threat = c;
				dThreat = dBot;
			}
			if (dPerson > PLAYERBOT_PDG_HUNT_RANGE)
				continue;
			if (c->IsStone() && dPerson < dStone)
			{
				stone = c;
				dStone = dPerson;
			}
			if (dPerson < dAny)
			{
				any = c;
				dAny = dPerson;
			}
		}
		LPCHARACTER aimed = person->GetTarget();
		// MT2009_PLUS_BOT_DUNGEON_RUNS_V1: a bot leads the bots' own runs, and
		// a bot has no client's target - what it fights is its AI's.
		if (!aimed && person->GetDesc() && person->GetDesc()->IsBot())
		{
			TPlayerBotAIStateMap::const_iterator ps = s_mapPlayerBotAIStates.find(person->GetPlayerID());
			if (ps != s_mapPlayerBotAIStates.end() && ps->second.dwTargetVID != 0)
				aimed = CHARACTER_MANAGER::instance().Find(ps->second.dwTargetVID);
		}
		if (!IsPlayerBotPdgFoe(ch, aimed) || (dragonShielded && aimed->GetRaceNum() == PLAYERBOT_PDG_BLUE_DRAGON) ||
				DISTANCE_APPROX(person->GetX() - aimed->GetX(), person->GetY() - aimed->GetY()) > PLAYERBOT_PDG_HUNT_RANGE)
			aimed = NULL;
		LPCHARACTER cur = state.dwTargetVID ? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
		if (!IsPlayerBotPdgFoe(ch, cur) || (dragonShielded && cur->GetRaceNum() == PLAYERBOT_PDG_BLUE_DRAGON))
			cur = NULL;
		if (threat)
			return cur && (cur->GetVictim() == ch || cur->GetVictim() == person) ? cur : threat;
		if (aimed)
			return aimed;
		if (cur && DISTANCE_APPROX(person->GetX() - cur->GetX(), person->GetY() - cur->GetY()) <= PLAYERBOT_PDG_HUNT_RANGE &&
				(!stone || cur->IsStone()))
			return cur;
		return stone ? stone : any;
	}

	bool FightPlayerBotPartyDungeon(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotPdgBot& b, LPCHARACTER person,
			DWORD dwNow)
	{
		const long map = ch->GetMapIndex();
		if (KeepPlayerBotTowerAlive(ch, state, dwNow, PLAYERBOT_PDG_POTION_HP, PLAYERBOT_PDG_POTION_SP))
			return true;
		if (BreakOffPlayerBotArzDgBoss(ch, state, map, dwNow))
			return true;
		if (BuffPlayerBotTowerFellows(ch, state, dwNow))
			return true;
		// Caught up with a person a quest jumped to another room, or one far
		// beyond a wall: the way the dungeon jumped the person.
		const int toPerson = DISTANCE_APPROX(ch->GetX() - person->GetX(), ch->GetY() - person->GetY());
		if (toPerson > PLAYERBOT_PDG_CATCH_UP_DISTANCE && dwNow >= b.dwNextCatchUp && !person->IsWarping() &&
				!person->IsDead())
		{
			b.dwNextCatchUp = dwNow + PLAYERBOT_PDG_CATCH_UP_RETRY_MS;
			if ((toPerson > PLAYERBOT_PDG_FAR_DISTANCE ||
					!IsPlayerBotReachable(map, ch->GetX(), ch->GetY(), person->GetX(), person->GetY())) &&
					PutPlayerBotBesidePdgPerson(ch, person))
			{
				sys_log(0, "PLAYERBOT_DUNGEON: party bot caught up pid=%u name=%s person=%s instance=%ld distance=%d",
						ch->GetPlayerID(), ch->GetName(), person->GetName(), map, toPerson);
				return true;
			}
		}
		LPCHARACTER foe = PickPlayerBotPdgFoe(ch, state, person, dwNow);
		if (foe)
		{
			const DWORD before = state.dwTargetVID;
			state.bLastNavOutcome = PLAYERBOT_NAV_OUT_NONE;
			const bool fought = FightPlayerBotTowerObjective(ch, state, foe, dwNow);
			if (before != state.dwTargetVID && before != 0)
			{
				LPCHARACTER old = CHARACTER_MANAGER::instance().Find(before);
				if (!old || old->IsDead())
					++b.dwKills;
			}
			if (state.bLastNavOutcome == PLAYERBOT_NAV_OUT_UNREACHABLE)
			{
				// A big monster on cells the routes do not end on: any cell
				// near it, and if that fails too, set down by the person.
				state.bLastNavOutcome = PLAYERBOT_NAV_OUT_NONE;
				ClearPlayerBotRoute(state, false);
				state.dwNextNavPlanTime = 0;
				MovePlayerBot(ch, foe->GetX(), foe->GetY(), dwNow, 24, true, false);
				if (state.bLastNavOutcome == PLAYERBOT_NAV_OUT_UNREACHABLE)
					UnstickPlayerBotArzDgFoe(ch, foe, person->GetX(), person->GetY(), dwNow);
			}
			return fought;
		}
		// Nothing to fight: at the person's side.
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		const int stay = PLAYERBOT_PDG_STAY_DISTANCE - (int)(ch->GetPlayerID() % 4) * 50;
		if (toPerson > stay)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			MovePlayerBot(ch, person->GetX(), person->GetY(), dwNow, 6, true, false);
			return true;
		}
		if (ch->IsStateMove())
			ch->Stop();
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		return true;
	}

	// ------------------------------------------------------------ the tick

	// From the top of the bot's tick, right after the Arezzo dungeon cohort's
	// (playerbot_manager.cpp), ahead of every errand: a bot whose person is in
	// a party dungeon goes in after the person, and a bot inside belongs to
	// this pass alone. False for everybody else.
	bool ManagePlayerBotPartyDungeon(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead())
			return false;
		const DWORD pid = ch->GetPlayerID();
		const long map = ch->GetMapIndex();
		const bool inside = map >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN && IsPlayerBotPartyDungeonMap(map);
		if (!inside)
		{
			std::map<DWORD, TPlayerBotPdgBot>::iterator it = s_mapPlayerBotPdg.find(pid);
			if (it != s_mapPlayerBotPdg.end() && it->second.lInstance != 0)
			{
				// Out: the run's items go, and the record with them (the
				// follow's clock stays while it waits to go in).
				DropPlayerBotPdgItems(ch);
				it->second.lInstance = 0;
				it->second.dwEnteredAt = 0;
				it->second.dwKills = 0;
			}
			if (!ch->GetParty())
			{
				if (it != s_mapPlayerBotPdg.end())
					s_mapPlayerBotPdg.erase(it);
				return false;
			}
			// The companion places itself beside its owner (playerbot_sidekick.h).
			if (IsPlayerBotSidekickPID(pid))
				return false;
			LPCHARACTER person = FindPlayerBotPartyPersonInDungeon(ch);
			if (!person)
				return false;
			return FollowPlayerBotPersonIntoDungeon(ch, state, person, dwNow);
		}
		// The Arezzo dungeon cohort runs its own instances, and so do the
		// bots' own dungeon runs (MT2009_PLUS_BOT_DUNGEON_RUNS_V1).
		if ((IsPlayerBotArezzoDungeonCohortPID(pid) && s_bPlayerBotArzDgHosting) || IsPlayerBotOnDungeonRun(pid))
			return false;
		TPlayerBotPdgBot& b = s_mapPlayerBotPdg[pid];
		if (b.lInstance != map)
		{
			// In by a way of its own (the dungeon's jump, the companion's
			// placing): this run's from now.
			b.lInstance = map;
			b.dwEnteredAt = dwNow;
			b.dwKills = 0;
			b.dwPersonGoneSince = 0;
		}
		state.dwLastMeaningfulActivityTime = dwNow;
		state.lLastX = ch->GetX();
		state.lLastY = ch->GetY();
		if (ManagePlayerBotPdgItems(ch, state, b, dwNow))
			return true;
		if (IsPlayerBotSidekickPID(pid))
			return false;
		LPCHARACTER person = FindPlayerBotPartyPerson(ch, map);
		if (!person)
			return LeavePlayerBotPartyDungeon(ch, state, b, dwNow);
		b.dwPersonGoneSince = 0;
		return FightPlayerBotPartyDungeon(ch, state, b, person, dwNow);
	}
}

#endif
