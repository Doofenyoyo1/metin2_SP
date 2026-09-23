#ifndef __INC_METIN2_PLAYERBOT_MINING_H__
#define __INC_METIN2_PLAYERBOT_MINING_H__

// Mining: the pickaxe, the vein, the ore and what it is smelted into.
//
// The engine has carried the whole mechanism since r40250 - mining.cpp holds
// the ore table, the odds and the swing event, and CHARACTER::mining(vein) is
// the one entry point - but **this world spawns none of it**. Measured across
// all 109 maps of the running server: zero vein spawns (20047-20059, 30301-30305)
// and zero alchemists in any regen.txt, npc.txt or boss.txt. So the missing
// half was never the AI; it was the world, and an AI that walked bots towards
// ore would have walked them towards nothing.
//
// The veins are therefore ours to place and ours to keep alive. Two facts from
// the engine decide the shape of that:
//
//   * CHARACTER::SetProto - a character whose race IsVeinOfOre gets a
//     kill_ore_load_event of 7 to 15 minutes, so **a vein deletes itself**.
//     On a normal server regen.txt puts it back; here nothing would, so the
//     maintenance pass below re-spawns a site whose vein has gone.
//   * CHARACTER_MANAGER::SpawnMob refuses a vein on an ATTR_BLOCK cell (and
//     only on that - an ordinary NPC is also refused on ATTR_OBJECT). The
//     sites are therefore hunting-hub coordinates, which are spawn points the
//     world itself uses and are walkable by construction; a refusal is logged
//     and the site is simply left empty rather than guessed at.
//
// Placement is deliberately on the three frontier maps and nowhere else. The
// pickaxe carries LIMIT_LEVEL 30 (world.item_proto 29101), so no bot that could
// hold one is still in a village, and putting ore where the bots already are
// means a miner walks to a vein instead of across a kingdom to one.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_activities.h - it uses the walk, the gold
// reserve and the level-limit reader - and before anything that asks whether a
// bot is mining.

namespace
{
	// The engine's own table (mining.cpp info[]), restricted to the thirteen
	// veins that carry a raw ore and a smelted form in this world's item_proto.
	// Kept here rather than reached through mining::GetRawOreFromLoad because
	// the AI needs the whole row - what to spawn, what drops, what it becomes -
	// and the engine exposes only one direction of it.
	struct TPlayerBotOreRow
	{
		DWORD dwVeinVnum;
		DWORD dwRawVnum;
		DWORD dwSmeltedVnum;
	};

	const TPlayerBotOreRow PLAYERBOT_ORE_ROWS[] = {
		{ 20047, 50601, 50621 },   // Zyla Diamentu     -> Diament
		{ 20048, 50602, 50622 },   // Zyla Bursztynu    -> Bursztyn
		{ 20049, 50603, 50623 },   // Skamien. Drzewo
		{ 20050, 50604, 50624 },   // Miedz
		{ 20051, 50605, 50625 },   // Srebro
		{ 20052, 50606, 50626 },   // Zloto
		{ 20053, 50607, 50627 },   // Jadeit
		{ 20054, 50608, 50628 },   // Zyla Ebonitu      -> Ebonit
		{ 20055, 50609, 50629 },   // Sterta Muszli     -> Perla
		{ 20056, 50610, 50630 },   // Biale Zloto
		{ 20057, 50611, 50631 },   // Krysztal
		{ 20058, 50612, 50632 },   // Ametyst
		{ 20059, 50613, 50633 }    // Niebianskie Lzy
	};
	const size_t PLAYERBOT_ORE_ROW_COUNT =
			sizeof(PLAYERBOT_ORE_ROWS) / sizeof(PLAYERBOT_ORE_ROWS[0]);

	const TPlayerBotOreRow* GetPlayerBotOreRowByVein(DWORD dwVeinVnum)
	{
		for (size_t i = 0; i < PLAYERBOT_ORE_ROW_COUNT; ++i)
			if (PLAYERBOT_ORE_ROWS[i].dwVeinVnum == dwVeinVnum)
				return &PLAYERBOT_ORE_ROWS[i];
		return NULL;
	}

	const TPlayerBotOreRow* GetPlayerBotOreRowByRaw(DWORD dwRawVnum)
	{
		for (size_t i = 0; i < PLAYERBOT_ORE_ROW_COUNT; ++i)
			if (PLAYERBOT_ORE_ROWS[i].dwRawVnum == dwRawVnum)
				return &PLAYERBOT_ORE_ROWS[i];
		return NULL;
	}

	bool IsPlayerBotRawOre(DWORD vnum)
	{
		return GetPlayerBotOreRowByRaw(vnum) != NULL;
	}

	bool IsPlayerBotSmeltedOre(DWORD vnum)
	{
		for (size_t i = 0; i < PLAYERBOT_ORE_ROW_COUNT; ++i)
			if (PLAYERBOT_ORE_ROWS[i].dwSmeltedVnum == vnum)
				return true;
		return false;
	}

	// Where a vein stands. Every coordinate here is a hunting-hub point out of
	// playerbot_wandering.h - ground the world spawns monsters on, so walkable
	// and acceptable to SpawnMob - and the ebonite vein is repeated on all
	// three maps because ebonite is the one the operator asked for by name.
	struct TPlayerBotVeinSite
	{
		long lMapIndex;
		long x;
		long y;
		DWORD dwVeinVnum;
	};

	const TPlayerBotVeinSite PLAYERBOT_VEIN_SITES[] = {
		// Orc Valley (64)
		{ PLAYERBOT_MAP_ORC_VALLEY, 276600, 684600, 20054 },   // ebonit
		{ PLAYERBOT_MAP_ORC_VALLEY, 281700, 795300, 20050 },
		{ PLAYERBOT_MAP_ORC_VALLEY, 290300, 799400, 20051 },
		{ PLAYERBOT_MAP_ORC_VALLEY, 348300, 705800, 20047 },
		{ PLAYERBOT_MAP_ORC_VALLEY, 391100, 738100, 20052 },
		{ PLAYERBOT_MAP_ORC_VALLEY, 315800, 732600, 20055 },
		{ PLAYERBOT_MAP_ORC_VALLEY, 342600, 729800, 20053 },
		// Yongbi Desert (63)
		{ PLAYERBOT_MAP_DESERT, 291300, 515700, 20054 },       // ebonit
		{ PLAYERBOT_MAP_DESERT, 237500, 525900, 20048 },
		{ PLAYERBOT_MAP_DESERT, 264600, 526100, 20050 },
		{ PLAYERBOT_MAP_DESERT, 317900, 526100, 20056 },
		{ PLAYERBOT_MAP_DESERT, 336900, 534300, 20051 },
		{ PLAYERBOT_MAP_DESERT, 245100, 542500, 20049 },
		{ PLAYERBOT_MAP_DESERT, 264500, 552300, 20057 },
		// Mount Sohan (61)
		{ PLAYERBOT_MAP_SOHAN, 432000, 272000, 20054 },        // ebonit
		{ PLAYERBOT_MAP_SOHAN, 393600, 265600, 20057 },
		{ PLAYERBOT_MAP_SOHAN, 470400, 291200, 20058 },
		{ PLAYERBOT_MAP_SOHAN, 438400, 272000, 20059 },
		{ PLAYERBOT_MAP_SOHAN, 412800, 278400, 20052 },
		{ PLAYERBOT_MAP_SOHAN, 483200, 208000, 20056 }
	};
	const size_t PLAYERBOT_VEIN_SITE_COUNT =
			sizeof(PLAYERBOT_VEIN_SITES) / sizeof(PLAYERBOT_VEIN_SITES[0]);

	// The vein standing at each site, by site index. A vein kills itself after
	// 7-15 minutes, so this is a registry of what is alive, not of what was
	// asked for; the VID is checked against the race before it is believed,
	// because a VID is reused once its character is gone.
	std::map<size_t, DWORD> s_mapPlayerBotVeinVID;
	DWORD s_dwPlayerBotNextVeinCheck = 0;

	LPCHARACTER FindPlayerBotVeinAtSite(size_t site)
	{
		std::map<size_t, DWORD>::iterator it = s_mapPlayerBotVeinVID.find(site);
		if (it == s_mapPlayerBotVeinVID.end())
			return NULL;
		LPCHARACTER vein = CHARACTER_MANAGER::instance().Find(it->second);
		if (!vein || vein->IsDead() ||
				vein->GetRaceNum() != PLAYERBOT_VEIN_SITES[site].dwVeinVnum)
		{
			s_mapPlayerBotVeinVID.erase(it);
			return NULL;
		}
		return vein;
	}

	// Once a minute for the whole world, not per bot: the sites are twenty and
	// the check is a map lookup each.
	void MaintainPlayerBotOreVeins(DWORD dwNow)
	{
		if (dwNow < s_dwPlayerBotNextVeinCheck)
			return;
		s_dwPlayerBotNextVeinCheck = dwNow + PLAYERBOT_ORE_VEIN_CHECK_INTERVAL;

		int spawned = 0, standing = 0, refused = 0;
		for (size_t site = 0; site < PLAYERBOT_VEIN_SITE_COUNT; ++site)
		{
			const TPlayerBotVeinSite& s = PLAYERBOT_VEIN_SITES[site];
			// A map this core does not host is another core's business; asking
			// it to spawn there would fail on a NULL sectree every minute.
			if (SECTREE_MANAGER::instance().GetMap(s.lMapIndex) == NULL)
				continue;
			if (FindPlayerBotVeinAtSite(site) != NULL)
			{
				++standing;
				continue;
			}
			LPCHARACTER vein = CHARACTER_MANAGER::instance().SpawnMob(
					s.dwVeinVnum, s.lMapIndex, s.x, s.y, 0, false, -1, true);
			if (!vein)
			{
				++refused;
				continue;
			}
			s_mapPlayerBotVeinVID[site] = (DWORD)vein->GetVID();
			++spawned;
		}
		if (spawned > 0 || refused > 0)
			sys_log(0, "PLAYERBOT_MINING: veins standing=%d spawned=%d refused=%d sites=%u",
					standing, spawned, refused, (unsigned int)PLAYERBOT_VEIN_SITE_COUNT);
	}

	// The nearest vein to this bot on its own map, and how far it is.
	LPCHARACTER FindPlayerBotNearestVein(LPCHARACTER ch, long* pDistance)
	{
		if (!ch)
			return NULL;
		LPCHARACTER best = NULL;
		long bestDistance = 0;
		for (size_t site = 0; site < PLAYERBOT_VEIN_SITE_COUNT; ++site)
		{
			if (PLAYERBOT_VEIN_SITES[site].lMapIndex != ch->GetMapIndex())
				continue;
			LPCHARACTER vein = FindPlayerBotVeinAtSite(site);
			if (!vein)
				continue;
			const long distance = DISTANCE_APPROX(ch->GetX() - vein->GetX(),
					ch->GetY() - vein->GetY());
			if (!best || distance < bestDistance)
			{
				best = vein;
				bestDistance = distance;
			}
		}
		if (best && pDistance)
			*pDistance = bestDistance;
		return best;
	}

	bool PlayerBotMapHasOreVeins(long mapIndex)
	{
		for (size_t site = 0; site < PLAYERBOT_VEIN_SITE_COUNT; ++site)
			if (PLAYERBOT_VEIN_SITES[site].lMapIndex == mapIndex)
				return true;
		return false;
	}

	// Who mines. A share by pid, as the anglers are drawn, plus the collector
	// personality which is already the one that keeps things rather than
	// selling them. Deliberately small: a vein pays one roll every half minute,
	// so a crowd at one is a crowd doing nothing.
	bool IsPlayerBotMiner(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		// Nor is a dropper a miner: a session at a vein is time away from the
		// one thing it farms (IsPlayerBotAngler).
		if (!ch || IsPlayerBotDropper(state.bPersonality) || ch->GetLevel() < PLAYERBOT_MINING_MIN_LEVEL)
			return false;
		const DWORD roll = PlayerBotNavHash(ch->GetPlayerID() ^ 0x4D494E45U) % 100U;
		const int chance = state.bPersonality == BOT_PERSONALITY_CAREFUL_COLLECTOR
				? PLAYERBOT_MINING_COLLECTOR_PERCENT : PLAYERBOT_MINING_PERCENT;
		return roll < (DWORD)chance;
	}

	bool IsPlayerBotHoldingPickaxe(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		LPITEM pick = ch->GetWear(WEAR_WEAPON);
		return pick && pick->GetType() == ITEM_PICK;
	}

	int CountPlayerBotPickaxes(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		int count = IsPlayerBotHoldingPickaxe(ch) ? 1 : 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetType() == ITEM_PICK)
				++count;
		}
		return count;
	}

	// Bought the way the fishing pass and the Forgetting Scroll are bought.
	// Deokbae's pick_shop (world.shop 10) stands on three maps this world never
	// sends a bot to, and a bot that cannot reach a counter cannot buy from it;
	// the price paid here is the shop's own.
	bool EnsurePlayerBotPickaxe(LPCHARACTER ch, DWORD dwNow)
	{
		if (!ch)
			return false;
		if (CountPlayerBotPickaxes(ch) > 0)
			return true;
		if (ch->GetGold() < (long long)(PLAYERBOT_PICKAXE_PRICE + GetPlayerBotReservedGold(ch)))
			return false;
		// AutoGiveItem drops what the bag cannot take at the bot's feet, so the
		// room is checked before the money is spent.
		if (ch->GetEmptyInventory(1) < 0)
			return false;
		LPITEM pick = ch->AutoGiveItem(PLAYERBOT_PICKAXE_VNUM, 1, -1, false);
		if (!pick)
			return false;
		PlayerBotChangeGold(ch, -(int)PLAYERBOT_PICKAXE_PRICE);
		sys_log(0, "PLAYERBOT_MINING: pickaxe bought pid=%u name=%s price=%u gold=%lld",
				ch->GetPlayerID(), ch->GetName(), PLAYERBOT_PICKAXE_PRICE,
				(long long)ch->GetGold());
		(void)dwNow;
		return true;
	}

	// The pickaxe goes where the rod goes: the weapon slot. mining_event asks
	// GetWear(WEAR_WEAPON) for an ITEM_PICK on the tick it fires, so a bot that
	// swapped back to its sword mid-swing is told it cannot dig.
	bool EquipPlayerBotPickaxe(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		if (IsPlayerBotHoldingPickaxe(ch))
			return true;
		LPITEM best = NULL;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			// The grades are consecutive vnums and a higher grade digs better
			// (PickGradeAddPct), so the highest vnum is the best pickaxe.
			if (item && item->GetType() == ITEM_PICK &&
					(!best || item->GetVnum() > best->GetVnum()))
				best = item;
		}
		if (!best)
			return false;
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		// The engine's UnequipItem takes a bag cell without asking whether there
		// is one; the room is asked here, and the weapon has to be off after.
		if (worn && (ch->GetEmptyInventory(worn->GetSize()) < 0 || !ch->UnequipItem(worn) ||
				worn->IsEquipped()))
			return false;
		if (!PlayerBotEquipItem(ch, best))
			return false;
		sys_log(0, "PLAYERBOT_MINING: pickaxe equipped pid=%u name=%s vnum=%u",
				ch->GetPlayerID(), ch->GetName(), best->GetVnum());
		return true;
	}

	// Smelting, without an alchemist - because this world has none.
	//
	// The engine's mining::OreRefine is a quest call: pc.ore_refine(cost, pct)
	// from an alchemist's dialog, taking ORE_COUNT_FOR_REFINE (100) of the raw
	// ore and rolling pct for one smelted piece. No alchemist is spawned on any
	// of the 109 maps here and no quest calls it, so the same arithmetic is
	// done directly - exactly as CollectPlayerBotBattleHorse does the stable
	// keeper's quest rather than pretending to talk to him.
	//
	// The roll is the one thing deliberately not copied. A bot cannot judge a
	// gamble, and losing a hundred ore it spent an hour on would read as the
	// mining being broken; the fee stands and the result is certain.
	bool SmeltPlayerBotOre(LPCHARACTER ch, DWORD dwNow)
	{
		if (!ch)
			return false;
		for (size_t i = 0; i < PLAYERBOT_ORE_ROW_COUNT; ++i)
		{
			const TPlayerBotOreRow& row = PLAYERBOT_ORE_ROWS[i];
			if (ch->CountSpecifyItem(row.dwRawVnum) < PLAYERBOT_ORE_SMELT_COUNT)
				continue;
			if (ch->GetGold() < (long long)(PLAYERBOT_ORE_SMELT_FEE + GetPlayerBotReservedGold(ch)))
				return false;
			if (ch->GetEmptyInventory(1) < 0)
				return false;
			ch->RemoveSpecifyItem(row.dwRawVnum, PLAYERBOT_ORE_SMELT_COUNT);
			PlayerBotChangeGold(ch, -(int)PLAYERBOT_ORE_SMELT_FEE);
			ch->AutoGiveItem(row.dwSmeltedVnum, 1, -1, false);
			sys_log(0, "PLAYERBOT_MINING: smelted pid=%u name=%s raw=%u -> %u fee=%u gold=%lld",
					ch->GetPlayerID(), ch->GetName(), row.dwRawVnum, row.dwSmeltedVnum,
					PLAYERBOT_ORE_SMELT_FEE, (long long)ch->GetGold());
			(void)dwNow;
			return true;
		}
		return false;
	}

	// Iwakura's Gornik and the Perfectionist together: a smelt that fits the
	// bot's own jewellery goes into it before anything thinks of selling it
	// ("najpierw uzywa Diamentu, by wytworzyc wolne gniazda w swoim
	// ekwipunku, a nastepnie umieszcza w nich przetopy"). The engine does both
	// through UseItemEx onto the piece - USE_ADD_ACCESSORY_SOCKET opens a
	// socket (up to ITEM_ACCESSORY_SOCKET_MAX_NUM), USE_PUT_INTO_ACCESSORY_SOCKET
	// fills one when CItem::CanPutInto says the stone is this piece's - and
	// refuses both on a worn piece, so the piece comes off for the one use and
	// goes back on. One use a call, on a clock of its own.
	std::map<DWORD, DWORD> s_mapPlayerBotSocketWorkNext;

	bool ManagePlayerBotAccessorySockets(LPCHARACTER ch, DWORD dwNow)
	{
		if (!ch || !IsPlayerBotPersonaEnabled() || !ch->IsItemLoaded() || ch->IsDead())
			return false;
		const DWORD pid = ch->GetPlayerID();
		std::map<DWORD, DWORD>::const_iterator next = s_mapPlayerBotSocketWorkNext.find(pid);
		if (next != s_mapPlayerBotSocketWorkNext.end() && dwNow < next->second)
			return false;
		s_mapPlayerBotSocketWorkNext[pid] = dwNow + PLAYERBOT_GORNIK_SOCKET_WORK_MS;
		static const BYTE slots[] = { WEAR_EAR, WEAR_WRIST, WEAR_NECK };
		for (size_t s = 0; s < sizeof(slots) / sizeof(slots[0]); ++s)
		{
			LPITEM piece = ch->GetWear(slots[s]);
			if (!piece || !piece->IsAccessoryForSocket() || piece->isLocked() || piece->IsExchanging())
				continue;
			int smeltCell = -1, diamondCell = -1;
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetCell() != cell || item->isLocked() || item->GetType() != ITEM_USE)
					continue;
				if (item->GetSubType() == USE_PUT_INTO_ACCESSORY_SOCKET && smeltCell < 0 &&
						item->CanPutInto(piece))
					smeltCell = cell;
				else if (item->GetSubType() == USE_ADD_ACCESSORY_SOCKET && diamondCell < 0)
					diamondCell = cell;
			}
			if (smeltCell < 0)
				continue;
			const int grade = piece->GetAccessorySocketGrade();
			const int maxGrade = piece->GetAccessorySocketMaxGrade();
			const bool freeSocket = grade < maxGrade;
			if (!freeSocket && (maxGrade >= ITEM_ACCESSORY_SOCKET_MAX_NUM || diamondCell < 0))
				continue;
			// UnequipItem takes a bag cell without asking for one.
			if (ch->GetEmptyInventory(piece->GetSize()) < 0)
				return false;
			const DWORD pieceVnum = piece->GetVnum();
			if (!ch->UnequipItem(piece) || piece->IsEquipped() || piece->GetWindow() != INVENTORY)
				return false;
			const WORD pieceCell = piece->GetCell();
			const int useCell = freeSocket ? smeltCell : diamondCell;
			LPITEM use = ch->GetInventoryItem(useCell);
			const DWORD useVnum = use ? use->GetVnum() : 0;
			const bool used = use && ch->UseItem(TItemPos(INVENTORY, (WORD)useCell),
					TItemPos(INVENTORY, pieceCell));
			LPITEM after = ch->GetInventoryItem(pieceCell);
			const int gradeAfter = after ? after->GetAccessorySocketGrade() : grade;
			const int maxAfter = after ? after->GetAccessorySocketMaxGrade() : maxGrade;
			if (after)
				PlayerBotEquipItem(ch, after);
			sys_log(0, "PLAYERBOT_PERSONA: gornik jewel %s pid=%u name=%s piece=%u used=%u grade=%d->%d sockets=%d->%d ok=%d",
					freeSocket ? "gem" : "socket", pid, ch->GetName(), pieceVnum, useVnum,
					grade, gradeAfter, maxGrade, maxAfter, used ? 1 : 0);
			return used;
		}
		return false;
	}

	// The session's three clocks, kept out of TPlayerBotAIState on purpose.
	// Adding a field there means matching its position in the declaration list
	// *and* in the constructor's initialiser list, and -Wreorder is the first
	// trap CLAUDE.md lists; a map keyed by pid says the same thing and cannot
	// be got wrong.
	std::map<DWORD, DWORD> s_mapPlayerBotMiningNext;     // when to consider digging again
	std::map<DWORD, DWORD> s_mapPlayerBotMiningUntil;    // session end, and "is mining now"
	std::map<DWORD, DWORD> s_mapPlayerBotMiningSwingAt;  // when the current swing resolves
	// Iwakura's Gornik digs one vein until it is gone: which one, by pid.
	std::map<DWORD, DWORD> s_mapPlayerBotMiningVein;

	// What the tick needs to know about a miner standing still on purpose. A
	// swing is up to thirty seconds of not moving and the inactivity watchdog
	// fires at ninety, so without this a miner is reset in the middle of its
	// third swing - the same exemption an angler has, for the same reason.
	bool IsPlayerBotMiningNow(DWORD pid, DWORD dwNow)
	{
		std::map<DWORD, DWORD>::const_iterator it = s_mapPlayerBotMiningUntil.find(pid);
		return it != s_mapPlayerBotMiningUntil.end() && dwNow < it->second;
	}

	// Health at the session's last look, so a blow landed between two looks
	// is noticed (ManagePlayerBotMining).
	std::map<DWORD, int> s_mapPlayerBotMiningHP;

	// dwRetry, when not zero, replaces the rest: a session a fight or a death
	// broke off is picked up again soon after, not half an hour later.
	void EndPlayerBotMiningSession(LPCHARACTER ch, TPlayerBotAIState& state,
			DWORD dwNow, const char* szReason, DWORD dwRetry = 0)
	{
		if (!ch)
			return;
		const DWORD pid = ch->GetPlayerID();
		s_mapPlayerBotMiningUntil.erase(pid);
		s_mapPlayerBotMiningSwingAt.erase(pid);
		s_mapPlayerBotMiningHP.erase(pid);
		s_mapPlayerBotMiningVein.erase(pid);
		// The pickaxe must not travel in the weapon slot: every combat path
		// judges by the weapon in the hand, and a bot that walked away holding
		// one would swing a digging tool at an orc until the gear pass noticed.
		LPITEM worn = ch->GetWear(WEAR_WEAPON);
		if (worn && worn->GetType() == ITEM_PICK)
			ch->UnequipItem(worn);
		s_mapPlayerBotMiningNext[pid] = dwNow + (dwRetry != 0 ? dwRetry :
				(DWORD)number(PLAYERBOT_MINING_REST_MIN, PLAYERBOT_MINING_REST_MAX));
		ClearPlayerBotRoute(state, true);
		sys_log(0, "PLAYERBOT_MINING: session end pid=%u name=%s reason=%s",
				pid, ch->GetName(), szReason ? szReason : "done");
	}

	// A session, in the shape the fishing one has: it owns the whole tick, so
	// combat and the gear pass below it never run while a pickaxe is in the
	// weapon slot.
	bool ManagePlayerBotMining(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead())
			return false;
		const DWORD pid = ch->GetPlayerID();
		const bool inSession = IsPlayerBotMiningNow(pid, dwNow);

		if (!PlayerBotMapHasOreVeins(ch->GetMapIndex()))
		{
			if (inSession)
				EndPlayerBotMiningSession(ch, state, dwNow, "left_map");
			return false;
		}
		// Anything the bot is actually doing outranks digging. Standing up after
		// a death is not a new errand, though: it used to end the session with
		// the full rest, so a miner killed at its vein walked off to fight
		// monsters and did not dig again for up to three quarters of an hour
		// ("zawsze po odrodzeniu porzuca rude", Mat, 14 September).
		if (state.bVisitingShop || state.bVisitingBiologist || state.bVisitingStable ||
				state.bRecoveringAfterDeath || state.bTacticalRetreat ||
				state.bMultiPullActive || state.bFishingSession || ch->GetMyShop())
		{
			if (inSession)
				EndPlayerBotMiningSession(ch, state, dwNow,
						state.bRecoveringAfterDeath ? "recovering" : "busy",
						state.bRecoveringAfterDeath ? PLAYERBOT_MINING_RESUME_AFTER_FIGHT : 0);
			return false;
		}

		// Iwakura's Gornik (PLAYERBOT_GORNIK_*): a bot with a pickaxe digs a
		// vein in sight until it is gone, and comes straight back to it after a
		// fight; without the switch, the old share digs anywhere on the map for
		// a session's length.
		const bool gornik = IsPlayerBotPersonaEnabled();
		if (!inSession)
		{
			std::map<DWORD, DWORD>::const_iterator next = s_mapPlayerBotMiningNext.find(pid);
			if (next != s_mapPlayerBotMiningNext.end() && dwNow < next->second)
				return false;
			if (!IsPlayerBotMiner(ch, state) && !(gornik && CountPlayerBotPickaxes(ch) > 0))
				return false;
			// Never walk off mid-fight; finish what is already hitting back.
			LPCHARACTER victim = state.dwTargetVID != 0
					? CHARACTER_MANAGER::instance().Find(state.dwTargetVID) : NULL;
			if (victim && !victim->IsDead())
				return false;
			long veinDistance = 0;
			LPCHARACTER nearest = FindPlayerBotNearestVein(ch, &veinDistance);
			if (nearest == NULL)
			{
				// Every vein on this map has expired at once; the maintenance
				// pass puts them back inside the minute.
				s_mapPlayerBotMiningNext[pid] = dwNow + PLAYERBOT_ORE_VEIN_CHECK_INTERVAL;
				return false;
			}
			// "W zasiegu jego radaru (wzroku)": a vein in sight, not one across
			// the map.
			if (gornik && veinDistance > PLAYERBOT_SEARCH_RANGE)
			{
				s_mapPlayerBotMiningNext[pid] = dwNow + PLAYERBOT_GORNIK_PROBE_MS;
				return false;
			}
			if (!EnsurePlayerBotPickaxe(ch, dwNow))
			{
				s_mapPlayerBotMiningNext[pid] = dwNow + PLAYERBOT_MINING_NO_PICK_RETRY;
				return false;
			}
			s_mapPlayerBotMiningUntil[pid] = dwNow + (gornik ? PLAYERBOT_GORNIK_SESSION_CAP :
					(DWORD)number(PLAYERBOT_MINING_SESSION_MIN, PLAYERBOT_MINING_SESSION_MAX));
			if (gornik)
				s_mapPlayerBotMiningVein[pid] = (DWORD)nearest->GetVID();
			s_mapPlayerBotMiningSwingAt.erase(pid);
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_MINING: session start pid=%u name=%s map=%ld level=%d",
					pid, ch->GetName(), ch->GetMapIndex(), (int)ch->GetLevel());
		}

		// A session owns the tick, so nothing below it runs while the bot digs -
		// not the fight, and not the emergency recovery that would have taken
		// it away at low health. A bot of another kingdom could be killed at its
		// vein without lifting a hand (Mat, 14 September). A blow now ends the
		// session on the tick it is noticed, and the fight below takes over.
		// Health at its maximum is not a blow: a maximum that drops - a buff
		// running out, the pickaxe taking a weapon's vitality with it - pulls
		// health down to meet it, and that is all it does.
		const int hp = ch->GetHP();
		std::map<DWORD, int>::iterator lastHP = s_mapPlayerBotMiningHP.find(pid);
		if (lastHP != s_mapPlayerBotMiningHP.end() && hp < lastHP->second &&
				hp < ch->GetMaxHP())
		{
			// The Gornik kills what came and is back at the vein the moment
			// nothing is on it - the start waits for the fight to end by itself.
			EndPlayerBotMiningSession(ch, state, dwNow, "attacked",
					gornik ? PLAYERBOT_GORNIK_RESUME_MS : PLAYERBOT_MINING_RESUME_AFTER_FIGHT);
			return false;
		}
		s_mapPlayerBotMiningHP[pid] = hp;

		state.bCurrentAction = BOT_ACTION_MINING;

		// A full bag ends it. OreDrop puts the ore on the grass and the loot
		// pass has to have somewhere to put it; a bot digging into a full bag
		// is a bot feeding the floor.
		if (ch->GetEmptyInventory(1) < 0)
		{
			EndPlayerBotMiningSession(ch, state, dwNow, "bag_full");
			return false;
		}
		if (dwNow >= s_mapPlayerBotMiningUntil[pid])
		{
			EndPlayerBotMiningSession(ch, state, dwNow, "session_over");
			return false;
		}

		long distance = 0;
		LPCHARACTER vein = NULL;
		if (gornik)
		{
			// The Gornik's own vein, until it is gone ("Wyczerpanie zloza:
			// Ruda znika z mapy") - then back to what it was doing.
			std::map<DWORD, DWORD>::const_iterator own = s_mapPlayerBotMiningVein.find(pid);
			vein = own != s_mapPlayerBotMiningVein.end()
					? CHARACTER_MANAGER::instance().Find(own->second) : NULL;
			if (!vein || vein->IsDead() || !GetPlayerBotOreRowByVein(vein->GetRaceNum()))
			{
				EndPlayerBotMiningSession(ch, state, dwNow, "vein_gone",
						(DWORD)number(PLAYERBOT_GORNIK_REST_MIN, PLAYERBOT_GORNIK_REST_MAX));
				ManagePlayerBotAccessorySockets(ch, dwNow);
				return false;
			}
			distance = DISTANCE_APPROX(ch->GetX() - vein->GetX(), ch->GetY() - vein->GetY());
		}
		else
			vein = FindPlayerBotNearestVein(ch, &distance);
		if (!vein)
			// The one being dug has just expired. The session has time left, so
			// wait for the maintenance pass rather than ending it.
			return true;

		if (distance > PLAYERBOT_MINING_ARRIVE)
		{
			// The horse is asked for, as a wander leg does: a vein can be a long
			// way from wherever the bot happened to be fighting.
			MovePlayerBot(ch, vein->GetX(), vein->GetY(), dwNow, 4, false, true, false, false);
			return true;
		}

		// The swing is made on foot. mining() asks nothing about a horse and
		// EquipItem lets a pickaxe on from the saddle, so a rider dug from
		// horseback (Remigiusz's screenshot, 17 September: TheBlady2 at a
		// Sterta Muszli on a white horse). As at the water: StopRiding leaves
		// the horse standing beside the miner, so it is sent away and
		// summoned again for the ride.
		if (SetPlayerBotRidingForTravel(ch, state, false, dwNow, "mining"))
			ch->HorseSummon(false);
		ch->Stop();
		std::map<DWORD, DWORD>::const_iterator swing = s_mapPlayerBotMiningSwingAt.find(pid);
		if (swing != s_mapPlayerBotMiningSwingAt.end() && dwNow < swing->second)
			return true;
		// CHARACTER::mining() is a toggle: called while its event is live it
		// *cancels* the swing. A bot that cancelled every swing would dig for
		// ever and never drop an ore, so the event is asked for directly and
		// the clock above is the belt to that braces.
		if (ch->m_pkMiningEvent != NULL)
			return true;
		if (!EquipPlayerBotPickaxe(ch))
		{
			EndPlayerBotMiningSession(ch, state, dwNow, "no_pickaxe");
			return false;
		}
		ch->SetRotationToXY(vein->GetX(), vein->GetY());
		ch->mining(vein);
		if (ch->m_pkMiningEvent == NULL)
		{
			// mining() explains a refusal to the client only, and a bot has
			// none; both of its gates are asked again here so the log says
			// which one it was.
			LPITEM pick = ch->GetWear(WEAR_WEAPON);
			PlayerBotLogThrottled("mining_refused", dwNow,
					"PLAYERBOT_MINING: mining() refused pid=%u name=%s vein=%u pick=%d dist=%ld",
					pid, ch->GetName(), (unsigned int)vein->GetRaceNum(),
					(pick && pick->GetType() == ITEM_PICK) ? 1 : 0, distance);
			s_mapPlayerBotMiningSwingAt[pid] = dwNow + PLAYERBOT_MINING_SWING_RETRY;
			return true;
		}
		s_mapPlayerBotMiningSwingAt[pid] = dwNow + PLAYERBOT_MINING_SWING_WAIT;
		// A hundred raw ore is a smelt, and the bot is standing where it earned
		// them. There is no alchemist in this world to walk to. And a smelt that
		// fits the bot's own jewellery goes into it first (the Gornik's synergy
		// with the Perfectionist).
		SmeltPlayerBotOre(ch, dwNow);
		ManagePlayerBotAccessorySockets(ch, dwNow);
		return true;
	}

	// The same rule the fishing arrival learned the hard way: a radius smaller
	// than the one the walk itself stops at leaves the bot standing in the gap
	// for ever, reporting success.
	static_assert(PLAYERBOT_MINING_ARRIVE >= PLAYERBOT_NAV_ARRIVAL_DISTANCE,
			"a mining arrival inside the walk's own stopping distance strands the bot");
}

#endif
