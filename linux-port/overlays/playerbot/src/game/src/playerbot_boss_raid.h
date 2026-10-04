#ifndef __INC_METIN2_PLAYERBOT_BOSS_RAID_H__
#define __INC_METIN2_PLAYERBOT_BOSS_RAID_H__

// Boss raids: a world boss broken by a crowd of one kingdom.
//
// A boss used to be a place to walk to (the boss hubs of
// playerbot_wandering.h), and three things kept him standing. A monster one
// bot had picked was nobody else's target (IsTargetClaimedByAnotherBot), so
// whoever came second chained onto his escort instead. The rule that gives
// up a fight going nowhere gave up a boss that heals faster than one bot
// hurts him - the Spider Queen takes back a tenth of her health every ten
// seconds, the Yellow Tiger Spectre an eighth every seven - and put him on
// the failed list for two minutes. And the walk to a boss hub was chosen only
// on a tick with nothing else to hit, and dropped for the first monster on
// the way. Measured on the test world over two and a half days (25
// September): the Orc Chief killed once, in fifty-three minutes, by
// twenty-three bots of whom exactly one was on him for eighteen of the
// thirty-four minutes he was fought, and not one casket in any bag
// ("sprawdzilem stan szkatulek - nie ma u mnie ani jednej po paru dniach
// gry", prodnathin; the operator: the Giant Turtle, the Flame King, the Spider
// Queen, Nine Tails, the Tiger Spectre - every boss has to be killed).
//
// A raid is the Demon Tower's shape on open ground. This core's world pass
// looks for every boss of PLAYERBOT_WORLD_BOSSES on the maps it hosts and
// calls a raid to one standing with none: up to bSize bots of one kingdom in
// his level window, the ones already on his map first and then the
// strongest, a Shaman among them when the kingdom has one free. Each called
// bot's tick is this pass's: it comes to its own spot outside his sight,
// buffs, and when enough have come - or he has started on one of them - they
// all go at him with the tower's fight and the tower's keeping alive. A boss
// who has not lost half a percent of his health in
// PLAYERBOT_BOSS_RAID_STALL_MS gets more bots, up to three rounds of four and
// twice the raid's size (MT2009_PLUS_BOSS_RAID_V2, 2.2.51); once nobody is
// left to call he is given up for PLAYERBOT_BOSS_RAID_OUTPACED_COOLDOWN_MS -
// unless he is under PLAYERBOT_BOSS_RAID_FINISH_PERCENT, when he is finished
// off. A bot at half its health comes (the walk is its time for the
// potions), and so does an angler or a miner, the tool put away. After his
// fall the loot pass has its window and the raid disbands.
//
// Not a party: a party's rules (the cohort, the straggler radius, the level
// gap of six) are about camps, and a raid of eight from four maps would be
// broken up by them before it met. The engine pays experience by damage and
// the drop to the best damager, which is what a crowd wants anyway.
//
// An implementation fragment in the sense playerbot_types.h describes:
// include it once, after playerbot_demon_tower.h, whose fight it borrows.

namespace
{
	struct TPlayerBotWorldBoss
	{
		WORD wRace;
		long lMap;
		BYTE bMinLevel;
		BYTE bMaxLevel;
		BYTE bSize;
		// Whether his fall is news for the whole world (a notice). The Bestial
		// Captain falls three times an hour.
		bool bAnnounce;
		// MT2009_PLUS_BOSS_RAID_V2 (2.2.51): a raid of all three kingdoms, for
		// a boss no one kingdom's bots of his window are enough for.
		bool bAnyKingdom;
	};

	// The bosses, read off the world's own tables (mob_proto, boss.txt and
	// special_spawns.txt, 25 September). The window runs from eight levels
	// under the boss to nine over him: past that the casket is no longer
	// certain (item_manager.cpp) and a bot refuses him for the experience.
	// The size aims at two and a half times what he heals, at the damage the
	// test world's bots of forty were measured dealing, scaled by level - an
	// estimate the stall rule corrects by calling more.
	const TPlayerBotWorldBoss PLAYERBOT_WORLD_BOSSES[] =
	{
		{  591,   3, 34, 51, 2, false },	// Bestial Captain (42), Jayang
		{  591,  23, 34, 51, 2, false },	// Bokjung
		{  591,  43, 34, 51, 2, false },	// Bakra
		{  691,  64, 46, 63, 4, true },	// Orc Chief (54), Orc Valley
		{ 2091, 104, 52, 69, 6, true },	// Spider Queen (60), Spider Dungeon
		{  791,  65, 52, 69, 6, true },	// Esoteric Leader (60), Hwang
		{  792,  65, 60, 77, 6, true },	// what his fall leaves standing (68)
		{ 2191,  63, 59, 76, 6, true },	// Giant Turtle (67), Yongbi Desert
		{ 1901,  61, 64, 81, 6, true },	// Nine Tails (72), Mount Sohan
		{ 2206,  62, 65, 82, 5, true },	// Flame King (73), Doyyumhwaji
		{ 1304,  65, 67, 84, 8, true },	// Yellow Tiger Spectre (75), Hwang
		// The Spider Baroness, who drops the Stalki (the operator's decision of
		// 28 September): the image stands her in V2's last room every four to
		// five hours (special_spawns.baroness.txt) with her lair's arithmetic -
		// her eggs broken, a blow on her counts ten times - or twenty percent
		// of her health back every thirty seconds would outheal any crowd. Her
		// raiders are brought past the desert (TransitionPlayerBotMap).
		// MT2009_PLUS_BOSS_RAID_V2 (2.2.51): no raid of eight broke her - the
		// test world's two (29-30 September) gave her up at 11% and 67%, and
		// from then on "nobody to call ... in_band=92 free=0/0/0" for a day,
		// the window of 67 holding one bot outside the Arezzo cohort. Sixteen
		// of every kingdom from level 60, as upstream's raid that broke her.
		{ 2092,  71, 60, 84, 16, true, true },	// Spider Baroness (75), Spider Dungeon 2
		// The Grotto of Exile (26 September). Both stand a maze's walk from
		// where a bot comes in - the Ice Witch some two hundred kilometres -
		// which is what the raid's own move to a spot is for. Yonghan's
		// Commander is no boss by rank, but his fall is what raises the
		// General, and nothing else would ever break him.
		{ 1192,  72, 81, 98, 8, true },	// Ice Witch (89), Grotto of Exile
		{ 2491,  73, 85, 102, 6, true },	// Yonghan's Commander (93), Grotto of Exile 2
		{ 2492,  73, 87, 104, 10, true },	// Yonghan's General (95), raised by his fall
		// MT2009_PLUS_OCHAO_BOTS_V1 (bosses): the Temple of Ochao, whose door is
		// level 95 - so the window opens at the door, not eight under the boss.
		// Its raids take only the bots already in the labyrinth, the nearest by
		// the walk, and walk them there (playerbot_ochao_bots.h). Not Straznik
		// En-Tai (6400): he is fought by whoever is near, as before (the owner,
		// 30 September).
		{ 6311, 209, 95, 120, 8, true },	// Ochroniarz Ochao (103), boss.txt, hourly
		{ 6390, 209, 95, 120, 10, true },	// Wladca Ochao (105), boss.txt, hourly
		// MT2009_PLUS_AREZZO_BOTS_V1 (bosses): the Arezzo maps' bosses
		// (boss.txt of each), for the bots already on the map - for now the
		// operator's test cohorts alone (playerbot_arezzo_bots.h). The windows
		// open at each map's own floor, the size is what his health asks.
		{ 9606, 360, 43, 61, 4, true },		// Arges (52), Dolina Cyklopow, 50-70 min
		{ 9607, 360, 43, 64, 6, true },		// Polifem (55), 2-3 h
		{ 9675, 361, 52, 69, 5, true },		// Bastet (60), Pustkowie Faraona, 50-70 min
		{ 9681, 361, 54, 71, 6, true },		// Anubis (62), 2-3 h
		{ 3390, 362, 95, 120, 8, true },	// Lemur Hrabia (103), Zaczarowany Las, 50-70 min
		{ 3391, 362, 95, 120, 10, true },	// Straz Przyboczna Lemur (105), 2-3 h
	};
	const size_t PLAYERBOT_WORLD_BOSS_COUNT = sizeof(PLAYERBOT_WORLD_BOSSES) / sizeof(PLAYERBOT_WORLD_BOSSES[0]);

	enum EPlayerBotBossRaidPhase
	{
		BOSS_RAID_PHASE_GATHER = 0,	// the members on their way to their spots
		BOSS_RAID_PHASE_FIGHT,		// everybody on him
		BOSS_RAID_PHASE_LOOT		// he is down; the loot pass has its window
	};

	struct TPlayerBotBossRaid
	{
		size_t row;
		BYTE bEmpire;
		BYTE bPhase;
		DWORD dwCalledAt;
		DWORD dwPhaseSince;
		DWORD dwBossVID;
		long lBossX;
		long lBossY;
		int iBestHP;
		DWORD dwLastProgress;
		// MT2009_PLUS_BOSS_RAID_V2: the rounds of reinforcements called
		// (PLAYERBOT_BOSS_RAID_REINFORCE_ROUNDS at most).
		BYTE bReinforced;
		// A stall that found nobody to call and him over the finish line: the
		// next such stall gives him up (one minute's grace, as before).
		bool bStalledEmpty;
		std::set<DWORD> members;
		// MT2009_PLUS_OCHAO_BOTS_V1 (muster): in the temple the raid meets here,
		// out of his group's reach, and goes in together; zero elsewhere.
		long lMusterX;
		long lMusterY;
		long lMusterForX;	// where he stood when the muster was chosen
		long lMusterForY;
		DWORD dwNextTopUp;

		TPlayerBotBossRaid() :
			row(0), bEmpire(0), bPhase(BOSS_RAID_PHASE_GATHER), dwCalledAt(0), dwPhaseSince(0),
			dwBossVID(0), lBossX(0), lBossY(0), iBestHP(0), dwLastProgress(0), bReinforced(0), bStalledEmpty(false),
			lMusterX(0), lMusterY(0), lMusterForX(0), lMusterForY(0), dwNextTopUp(0) {}
	};

	typedef std::pair<long, WORD> TPlayerBotBossKey;
	typedef std::map<TPlayerBotBossKey, TPlayerBotBossRaid> TPlayerBotBossRaidMap;
	TPlayerBotBossRaidMap s_mapPlayerBotBossRaids;
	// When each boss may next be looked at: a raid over, a boss down, or
	// nobody free to call.
	std::map<TPlayerBotBossKey, DWORD> s_mapPlayerBotBossRaidNext;
	DWORD s_dwNextPlayerBotBossRaidCheck = 0;
	DWORD s_dwPlayerBotBossRaidsStartedAt = 0;
	DWORD s_dwNextPlayerBotBossRaidCensus = 0;
	unsigned int s_uPlayerBotBossRaidsFormed = 0;
	unsigned int s_uPlayerBotBossRaidsKilled = 0;
	unsigned int s_uPlayerBotBossRaidsOutpaced = 0;
	unsigned int s_uPlayerBotBossRaidsTooFew = 0;

	const char* GetPlayerBotWorldBossName(WORD wRace)
	{
		const CMob* mob = CMobManager::instance().Get(wRace);
		return mob ? mob->m_table.szLocaleName : "Boss";
	}

	// The boss on this map, or NULL. The whole map is asked, as
	// IsPlayerBotBossAlive asks it: a boss chases what hits him a long way
	// from where he stood.
	LPCHARACTER FindPlayerBotWorldBoss(long lMap, WORD wRace)
	{
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(lMap);
		if (!pMap)
			return NULL;
		FPlayerBotFindBoss finder(wRace);
		pMap->for_each(finder);
		return finder.m_found;
	}

	// The raid's boss by the VID it was called to, or NULL once he is dead or
	// gone - or the VID is somebody else's by now.
	LPCHARACTER GetPlayerBotBossRaidBoss(const TPlayerBotBossRaid& raid)
	{
		if (raid.dwBossVID == 0)
			return NULL;
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[raid.row];
		LPCHARACTER boss = CHARACTER_MANAGER::instance().Find(raid.dwBossVID);
		if (!boss || boss->IsDead() || !boss->IsMonster() || boss->GetRaceNum() != row.wRace ||
				boss->GetMapIndex() != row.lMap)
			return NULL;
		return boss;
	}

	TPlayerBotBossRaid* FindPlayerBotBossRaid(long lMap, WORD wRace)
	{
		TPlayerBotBossRaidMap::iterator it = s_mapPlayerBotBossRaids.find(TPlayerBotBossKey(lMap, wRace));
		return it == s_mapPlayerBotBossRaids.end() ? NULL : &it->second;
	}

	void ClearPlayerBotBossRaidState(TPlayerBotAIState& state, LPCHARACTER ch, DWORD dwBossVID)
	{
		state.wBossRaidRace = 0;
		state.lBossRaidMap = 0;
		state.dwNextBossRaidMoveTime = 0;
		if (ch)
			ForgetPlayerBotOchaoWalk(ch->GetPlayerID()); // MT2009_PLUS_OCHAO_BOTS_V1 (walk)
		if (ch)
			ForgetPlayerBotArezzoWalk(ch->GetPlayerID()); // MT2009_PLUS_AREZZO_BOTS_V1 (walk)
		if (dwBossVID != 0 && state.dwTargetVID == dwBossVID)
			state.dwTargetVID = 0;
		if (ch && dwBossVID != 0 && ch->GetVictim() && (DWORD)ch->GetVictim()->GetVID() == dwBossVID)
			ch->SetVictim(NULL);
	}

	// A bot's own spot by the boss: outside his sight (2000 for every boss of
	// the table), by pid round him, on open ground his own ground joins - a
	// spot across a river from him is a spot the bot stands on for good.
	void GetPlayerBotBossRaidRally(DWORD pid, const TPlayerBotBossRaid& raid, long& outX, long& outY)
	{
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[raid.row];
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(row.lMap);
		const bool nav = navigation.Init(row.lMap);
		const DWORD bossGround = nav ? navigation.GetComponentAtWorld(raid.lBossX, raid.lBossY, 12) : 0;
		const long radius = PLAYERBOT_BOSS_RAID_RALLY_MIN +
				(long)(PlayerBotNavHash(pid ^ 0x42524C5AU) % (DWORD)PLAYERBOT_BOSS_RAID_RALLY_SPREAD);
		// MT2009_PLUS_OCHAO_BOTS_V1 (rally): the labyrinth is one piece of
		// ground, so "joined" says nothing - a spot has to be in his corridor.
		if (row.lMap == PLAYERBOT_MAP_OCHAO && nav && raid.lMusterX != 0)
		{
			// Round the muster, in its corridor.
			static const long spread[2] = { 450, 250 };
			const DWORD angle0 = PlayerBotNavHash(pid ^ 0x42524C59U) % 360U;
			for (int r = 0; r < 2; ++r)
				for (int attempt = 0; attempt < 8; ++attempt)
				{
					const double rad = (double)((angle0 + (DWORD)attempt * 45U) % 360U) * 3.14159265 / 180.0;
					const long x = raid.lMusterX + (long)(cos(rad) * spread[r]);
					const long y = raid.lMusterY + (long)(sin(rad) * spread[r]);
					if (!IsPlayerBotPositionBlocked(row.lMap, x, y) &&
							navigation.SegmentClearWorld(raid.lMusterX, raid.lMusterY, x, y))
					{
						outX = x;
						outY = y;
						return;
					}
				}
			outX = raid.lMusterX;
			outY = raid.lMusterY;
			return;
		}
		if (row.lMap == PLAYERBOT_MAP_OCHAO && nav)
		{
			static const long radii[3] = { PLAYERBOT_BOSS_RAID_RALLY_MIN, 1500, 900 };
			const DWORD angle0 = PlayerBotNavHash(pid ^ 0x42524C59U) % 360U;
			for (int r = 0; r < 3; ++r)
				for (int attempt = 0; attempt < 8; ++attempt)
				{
					const double rad = (double)((angle0 + (DWORD)attempt * 45U) % 360U) * 3.14159265 / 180.0;
					const long x = raid.lBossX + (long)(cos(rad) * radii[r]);
					const long y = raid.lBossY + (long)(sin(rad) * radii[r]);
					if (!IsPlayerBotPositionBlocked(row.lMap, x, y) &&
							navigation.SegmentClearWorld(raid.lBossX, raid.lBossY, x, y))
					{
						outX = x;
						outY = y;
						return;
					}
				}
			outX = raid.lBossX;
			outY = raid.lBossY;
			return;
		}
		const DWORD firstAngle = PlayerBotNavHash(pid ^ 0x42524C59U) % 360U;
		for (int attempt = 0; attempt < 8; ++attempt)
		{
			const double rad = (double)((firstAngle + (DWORD)attempt * 45U) % 360U) * 3.14159265 / 180.0;
			long x = raid.lBossX + (long)(cos(rad) * radius);
			long y = raid.lBossY + (long)(sin(rad) * radius);
			long openX = 0, openY = 0;
			if (FindPlayerBotWarGround(row.lMap, x, y, 800, openX, openY))
			{
				x = openX;
				y = openY;
			}
			if (!nav || bossGround == 0 || navigation.GetComponentAtWorld(x, y, 4) == bossGround)
			{
				outX = x;
				outY = y;
				return;
			}
		}
		outX = raid.lBossX;
		outY = raid.lBossY;
	}

	// Why this bot cannot be called to this boss now, or NULL when it can.
	const char* GetPlayerBotBossRaidRefusal(LPCHARACTER c, const TPlayerBotAIState& st,
			const TPlayerBotWorldBoss& row, DWORD dwNow)
	{
		if (!c || c->IsDead() || !c->GetDesc() || !c->GetDesc()->IsBot())
			return "gone";
		const int level = (int)c->GetLevel();
		if (level < (int)row.bMinLevel || level > (int)row.bMaxLevel)
			return "level";
		if (st.wBossRaidRace != 0)
			return "raiding";
		const DWORD pid = c->GetPlayerID();
		if (IsPlayerBotSidekickPID(pid) || IsPlayerBotSummoned(pid) || IsPlayerBotOnMercContract(pid) ||
				IsPlayerBotHeldForCompany(c))
			return "company";
		if (c->GetParty() && IsPlayerBotHumanLedParty(c->GetParty()))
			return "person";
		if (IsPlayerBotDropper(st.bPersonality))
			return "dropper";
		// MT2009_PLUS_BOSS_RAID_V2 (2.2.51): an angler or a miner comes too -
		// it puts the rod or the pickaxe away when called
		// (EnlistPlayerBotBossRaider); it used to be refused ("tool").
		if (IsPlayerBotOnTowerBusiness(c, st) || st.dwGuildWarEnemyGID != 0 ||
				playerbot_pvp::GetDuelOpponent(pid, dwNow) != 0)
			return "busy";
		if (IsPlayerBotOnAnyHorseTrial(c) /* MT2009_PLUS_HORSE30_V1 */)
			return "trial";
		if (st.bTownVisitPhase != BOT_TOWN_PHASE_NONE || c->GetMyShop())
			return "town";
		if (c->GetSkillGroup() == 0)
			return "no_skills";
		const long map = c->GetMapIndex();
		if (map >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN || map == PLAYERBOT_MAP_DEMON_TOWER ||
				IsPlayerBotDemonTowerInstance(map))
			return "dungeon";
		// The Spider Dungeon is reached across the desert on foot
		// (TransitionPlayerBotMap), which takes longer than a gathering lasts:
		// its queen is for the bots already down there.
		if (row.lMap == PLAYERBOT_MAP_SPIDER_V1 && map != row.lMap)
			return "far";
		if (st.bRecoveringAfterDeath || c->GetMaxHP() <= 0 ||
				(long long)c->GetHP() * 100 < (long long)c->GetMaxHP() * PLAYERBOT_BOSS_RAID_MIN_HP_PERCENT)
			return "health";
		size_t red = 0, blue = 0;
		CountPlayerBotPotions(c, red, blue);
		const bool caster = c->GetJob() == JOB_SHAMAN || (c->GetJob() == JOB_SURA && c->GetSkillGroup() == 2);
		if (red < PLAYERBOT_BOSS_RAID_MIN_RED_POTIONS || (caster && blue < PLAYERBOT_BOSS_RAID_MIN_BLUE_POTIONS))
			return "potions";
		return NULL;
	}

	struct TPlayerBotBossRecruit
	{
		DWORD pid;
		BYTE empire;
		bool onMap;
		bool shaman;
		int strength;
	};

	bool PlayerBotBossRecruitOrder(const TPlayerBotBossRecruit& a, const TPlayerBotBossRecruit& b)
	{
		if (a.onMap != b.onMap)
			return a.onMap;
		if (a.strength != b.strength)
			return a.strength > b.strength;
		return a.pid < b.pid;
	}

	// Every bot of this core that could be called to this boss now - of one
	// kingdom, or of any when empire is zero. inBand counts the bots of his
	// level window before the rest of the refusals, for the log.
	void CollectPlayerBotBossRecruits(const TPlayerBotWorldBoss& row, LPCHARACTER boss, BYTE empire,
			DWORD dwNow, std::vector<TPlayerBotBossRecruit>& out, int& inBand)
	{
		out.clear();
		inBand = 0;
		CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(row.lMap);
		const bool nav = navigation.Init(row.lMap);
		const DWORD bossGround = (nav && boss) ? navigation.GetComponentAtWorld(boss->GetX(), boss->GetY(), 12) : 0;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!c || (empire != 0 && c->GetEmpire() != empire))
				continue;
			if ((int)c->GetLevel() >= (int)row.bMinLevel && (int)c->GetLevel() <= (int)row.bMaxLevel)
				++inBand;
			if (GetPlayerBotBossRaidRefusal(c, it->second, row, dwNow))
				continue;
			// MT2009_PLUS_OCHAO_BOTS_V1 (raid): a bot in the Temple of Ochao's
			// labyrinth is minutes from its way out; the raid calls somebody nearer.
			if (c->GetMapIndex() == PLAYERBOT_MAP_OCHAO && row.lMap != PLAYERBOT_MAP_OCHAO)
				continue;
			// MT2009_PLUS_AREZZO_BOTS_V1 (raid): the Arezzo test's bots hunt
			// their map's bosses and nobody else's; and nobody else hunts those.
			int arezzoWalk = 0;
			if (IsPlayerBotArezzoMap(row.lMap))
			{
				if (c->GetMapIndex() != row.lMap || !boss || IsPlayerBotArezzoLeaving(c))
					continue;
				arezzoWalk = GetPlayerBotArezzoWalk(c, boss->GetX(), boss->GetY());
				if (arezzoWalk > (row.lMap == PLAYERBOT_MAP_AREZZO_FOREST
						? PLAYERBOT_AREZZO_FOREST_RAID_WALK_MAX : PLAYERBOT_AREZZO_RAID_WALK_MAX))
					continue;
				// A long walk weighs a tenth: the nearest first, but a strong
				// bot across the forest still comes.
				arezzoWalk /= 10;
			}
			else if (IsPlayerBotArezzoMap(c->GetMapIndex()) || IsPlayerBotArezzoBound(c))
				continue;
			// And the other way round: the temple's own bosses are for the bots
			// inside, within a walk that ends before the gathering does.
			int ochaoWalk = 0;
			if (row.lMap == PLAYERBOT_MAP_OCHAO)
			{
				if (c->GetMapIndex() != PLAYERBOT_MAP_OCHAO || !boss)
					continue;
				ochaoWalk = GetPlayerBotOchaoWalk(c, boss->GetX(), boss->GetY());
				if (ochaoWalk > PLAYERBOT_OCHAO_RAID_WALK_MAX || IsPlayerBotOchaoLeaving(c))
					continue;
			}
			TPlayerBotBossRecruit r;
			r.pid = it->first;
			r.empire = c->GetEmpire();
			r.onMap = c->GetMapIndex() == row.lMap;
			// On his map, but on ground his own ground does not join: an island
			// the bot cannot leave on foot.
			if (r.onMap && bossGround != 0 && navigation.GetComponentAtWorld(c->GetX(), c->GetY()) != bossGround)
				continue;
			r.shaman = c->GetJob() == JOB_SHAMAN;
			r.strength = GetPlayerBotStrengthCached(it->first);
			if (r.strength <= 0)
				r.strength = (int)c->GetLevel() * 1000;
			// MT2009_PLUS_BP_BOTS_V1: a bot with a Battle Pass boss mission first.
			r.strength += playerbot_bpbots::BossRecruitBonus(it->first, row.wRace);
			// In the labyrinth the nearest by the walk first (a unit of walk
			// weighs what a unit of strength does).
			r.strength -= ochaoWalk;
			r.strength -= arezzoWalk; // MT2009_PLUS_AREZZO_BOTS_V1
			out.push_back(r);
		}
		std::sort(out.begin(), out.end(), PlayerBotBossRecruitOrder);
	}

	void EnlistPlayerBotBossRaider(TPlayerBotBossRaid& raid, DWORD pid, DWORD dwNow)
	{
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[raid.row];
		raid.members.insert(pid);
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(pid);
		if (st == s_mapPlayerBotAIStates.end())
			return;
		// MT2009_PLUS_BOSS_RAID_V2 (2.2.51): an angler or a miner ends its
		// session and goes with its weapon in the hand; a refused equip is
		// tried again by the fight's own ReadyPlayerBotHandForFight.
		LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(pid);
		if (c && (st->second.bFishingSession || IsPlayerBotMiningNow(pid, dwNow)))
			ReadyPlayerBotHandForFight(c, st->second, dwNow, "boss_raid_called");
		st->second.wBossRaidRace = row.wRace;
		st->second.lBossRaidMap = row.lMap;
		// The departures spread over the first twenty seconds by pid.
		st->second.dwNextBossRaidMoveTime = dwNow + PlayerBotNavHash(pid ^ 0x42535452U) % 20000U;
	}

	// MT2009_PLUS_OCHAO_BOTS_V1 (watch): the members of a temple raid at him.
	int CountPlayerBotBossRaidAt(const TPlayerBotBossRaid& raid, LPCHARACTER boss)
	{
		int at = 0;
		for (std::set<DWORD>::const_iterator m = raid.members.begin(); m != raid.members.end(); ++m)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(*m);
			if (c && !c->IsDead() && c->GetMapIndex() == boss->GetMapIndex() &&
					DISTANCE_APPROX(c->GetX() - boss->GetX(), c->GetY() - boss->GetY()) <= PLAYERBOT_BOSS_RAID_ARRIVED_RANGE)
				++at;
		}
		return at;
	}

	// MT2009_PLUS_OCHAO_BOTS_V1 (muster): the members of a temple raid at its
	// muster, alive.
	int CountPlayerBotOchaoRaidMustered(const TPlayerBotBossRaid& raid)
	{
		if (raid.lMusterX == 0)
			return 0;
		int at = 0;
		for (std::set<DWORD>::const_iterator m = raid.members.begin(); m != raid.members.end(); ++m)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(*m);
			if (c && !c->IsDead() && c->GetMapIndex() == PLAYERBOT_MAP_OCHAO &&
					DISTANCE_APPROX(c->GetX() - raid.lMusterX, c->GetY() - raid.lMusterY) <= PLAYERBOT_OCHAO_RAID_MUSTER_ARRIVED)
				++at;
		}
		return at;
	}

	// MT2009_PLUS_OCHAO_BOTS_V1 (watch): a temple raid's members, each with
	// its walk to him and the straight line (pid:walk:line:hp%, d = dead).
	void LogPlayerBotOchaoRaidMembers(const char* what, const TPlayerBotBossRaid& raid, LPCHARACTER boss)
	{
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[raid.row];
		std::string list;
		for (std::set<DWORD>::const_iterator m = raid.members.begin(); m != raid.members.end(); ++m)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(*m);
			char one[64];
			if (!c || c->GetMapIndex() != row.lMap || !boss)
				snprintf(one, sizeof(one), " %u:away", *m);
			else
				snprintf(one, sizeof(one), " %u:%d:%d:%d%s", *m, GetPlayerBotOchaoWalk(c, boss->GetX(), boss->GetY()),
						DISTANCE_APPROX(c->GetX() - boss->GetX(), c->GetY() - boss->GetY()),
						c->GetMaxHP() > 0 ? (int)(c->GetHP() * 100LL / c->GetMaxHP()) : 0, c->IsDead() ? "d" : "");
			list += one;
		}
		sys_log(0, "OCHAO_BOT: raid %s race=%u empire=%u after_s=%u members=%u at_him=%d at_muster=%d boss=(%ld,%ld) muster=(%ld,%ld) list=%s",
				what, (unsigned int)row.wRace, (unsigned int)raid.bEmpire, (get_dword_time() - raid.dwCalledAt) / 1000U,
				(unsigned int)raid.members.size(), boss ? CountPlayerBotBossRaidAt(raid, boss) : 0,
				CountPlayerBotOchaoRaidMustered(raid), raid.lBossX, raid.lBossY, raid.lMusterX, raid.lMusterY, list.c_str());
	}

	// MT2009_PLUS_OCHAO_BOTS_V1 (watch): why the bots in the temple were not
	// called, per kingdom.
	void LogPlayerBotOchaoRaidRefusals(const TPlayerBotWorldBoss& row, LPCHARACTER boss, DWORD dwNow)
	{
		std::map<std::string, int> reasons[4];
		int onMap[4] = { 0, 0, 0, 0 };
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!c || c->GetMapIndex() != PLAYERBOT_MAP_OCHAO || c->GetEmpire() < 1 || c->GetEmpire() > 3)
				continue;
			const int e = c->GetEmpire();
			++onMap[e];
			const char* why = GetPlayerBotBossRaidRefusal(c, it->second, row, dwNow);
			if (!why && IsPlayerBotOchaoLeaving(c))
				why = "leaving";
			if (!why && boss && GetPlayerBotOchaoWalk(c, boss->GetX(), boss->GetY()) > PLAYERBOT_OCHAO_RAID_WALK_MAX)
				why = "walk";
			++reasons[e][why ? why : "free"];
		}
		for (int e = 1; e <= 3; ++e)
		{
			std::string list;
			for (std::map<std::string, int>::const_iterator r = reasons[e].begin(); r != reasons[e].end(); ++r)
			{
				char one[48];
				snprintf(one, sizeof(one), " %s=%d", r->first.c_str(), r->second);
				list += one;
			}
			sys_log(0, "OCHAO_BOT: raid refusals race=%u empire=%d on_map=%d%s",
					(unsigned int)row.wRace, e, onMap[e], list.c_str());
		}
	}

	// MT2009_PLUS_AREZZO_BOTS_V1 (watch): why the bots on an Arezzo map were
	// not called to its boss, counted by reason.
	void LogPlayerBotArezzoRaidRefusals(const TPlayerBotWorldBoss& row, LPCHARACTER boss, DWORD dwNow)
	{
		std::map<std::string, int> why;
		int onMap = 0;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin(); it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!c || c->GetMapIndex() != row.lMap)
				continue;
			++onMap;
			const char* w = GetPlayerBotBossRaidRefusal(c, it->second, row, dwNow);
			if (!w && IsPlayerBotArezzoLeaving(c))
				w = "leaving";
			if (!w && boss && GetPlayerBotArezzoWalk(c, boss->GetX(), boss->GetY()) >
					(row.lMap == PLAYERBOT_MAP_AREZZO_FOREST ? PLAYERBOT_AREZZO_FOREST_RAID_WALK_MAX : PLAYERBOT_AREZZO_RAID_WALK_MAX))
				w = "walk";
			++why[w ? w : "free"];
		}
		std::string list;
		for (std::map<std::string, int>::const_iterator w = why.begin(); w != why.end(); ++w)
		{
			char one[48];
			snprintf(one, sizeof(one), " %s=%d", w->first.c_str(), w->second);
			list += one;
		}
		sys_log(0, "ARZ_BOT: raid refusals race=%u map=%ld on_map=%d%s", (unsigned int)row.wRace, row.lMap, onMap, list.c_str());
	}

	// MT2009_PLUS_BOSS_RAID_V2 (watch): why the bots of his level window were
	// not called to a world boss, counted by reason - "in_band=92 free=0"
	// said nothing of why (the Baroness, 30 September - 1 October).
	void LogPlayerBotBossRaidRefusals(const TPlayerBotWorldBoss& row, DWORD dwNow)
	{
		std::map<std::string, int> why;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin(); it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!c || (int)c->GetLevel() < (int)row.bMinLevel || (int)c->GetLevel() > (int)row.bMaxLevel)
				continue;
			const char* w = GetPlayerBotBossRaidRefusal(c, it->second, row, dwNow);
			if (!w && (IsPlayerBotArezzoMap(c->GetMapIndex()) || IsPlayerBotArezzoBound(c)))
				w = "arezzo";
			if (!w && c->GetMapIndex() == PLAYERBOT_MAP_OCHAO)
				w = "ochao";
			++why[w ? w : "free"];
		}
		std::string list;
		for (std::map<std::string, int>::const_iterator w = why.begin(); w != why.end(); ++w)
		{
			char one[48];
			snprintf(one, sizeof(one), " %s=%d", w->first.c_str(), w->second);
			list += one;
		}
		sys_log(0, "PLAYERBOT_RAID: refusals race=%u map=%ld%s", (unsigned int)row.wRace, row.lMap, list.c_str());
	}

	// MT2009_PLUS_OCHAO_BOTS_V1 (top-up): while a temple raid gathers, a
	// member that is no longer within the walk (sent back to the gate after
	// its deaths, or dead where it stands) is let go, and the raid is filled
	// up again from the nearest free bots of its kingdom.
	void TopUpPlayerBotOchaoRaid(TPlayerBotBossRaid& raid, LPCHARACTER boss, DWORD dwNow)
	{
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[raid.row];
		int dropped = 0, added = 0;
		for (std::set<DWORD>::iterator m = raid.members.begin(); m != raid.members.end();)
		{
			LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(*m);
			TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(*m);
			if (c && st != s_mapPlayerBotAIStates.end() && c->GetMapIndex() == row.lMap && !c->IsDead() &&
					GetPlayerBotOchaoWalk(c, boss->GetX(), boss->GetY()) > PLAYERBOT_OCHAO_RAID_DROP_WALK)
			{
				ClearPlayerBotBossRaidState(st->second, c, raid.dwBossVID);
				raid.members.erase(m++);
				++dropped;
			}
			else
				++m;
		}
		if (raid.members.size() < (size_t)row.bSize)
		{
			std::vector<TPlayerBotBossRecruit> pool;
			int inBand = 0;
			CollectPlayerBotBossRecruits(row, boss, raid.bEmpire, dwNow, pool, inBand);
			for (size_t i = 0; i < pool.size() && raid.members.size() < (size_t)row.bSize; ++i)
			{
				EnlistPlayerBotBossRaider(raid, pool[i].pid, dwNow);
				++added;
			}
		}
		if (dropped || added)
			sys_log(0, "OCHAO_BOT: raid top-up race=%u empire=%u dropped=%d added=%d members=%u",
					(unsigned int)row.wRace, (unsigned int)raid.bEmpire, dropped, added, (unsigned int)raid.members.size());
	}

	// A boss standing with no raid on him: the kingdom with the most bots free
	// for him (his own map's, for the Captain of a second village), its bots
	// on his map first and then the strongest, a Shaman in when there is one.
	bool CallPlayerBotBossRaid(size_t rowIndex, LPCHARACTER boss, DWORD dwNow)
	{
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[rowIndex];
		const int owner = playerbot_empire_rules::GetMapOwnerEmpire(row.lMap);
		std::vector<TPlayerBotBossRecruit> pool;
		int inBand = 0;
		CollectPlayerBotBossRecruits(row, boss, (BYTE)std::max(0, owner), dwNow, pool, inBand);
		int perEmpire[4] = { 0, 0, 0, 0 };
		for (size_t i = 0; i < pool.size(); ++i)
			if (pool[i].empire >= 1 && pool[i].empire <= 3)
				++perEmpire[pool[i].empire];
		BYTE empire = (BYTE)std::max(0, owner);
		// MT2009_PLUS_BOSS_RAID_V2: the strongest of every kingdom (empire 0).
		if (row.bAnyKingdom)
			empire = 0;
		else if (empire == 0)
		{
			// Ties go round by the hour, so one kingdom does not take every
			// boss it ties for.
			int best = 0;
			const DWORD turn = (DWORD)row.wRace + dwNow / 3600000U;
			for (DWORD k = 0; k < 3; ++k)
			{
				const int e = 1 + (int)((turn + k) % 3U);
				if (perEmpire[e] > best)
				{
					best = perEmpire[e];
					empire = (BYTE)e;
				}
			}
		}
		std::vector<TPlayerBotBossRecruit> chosen;
		const TPlayerBotBossRecruit* shaman = NULL;
		for (size_t i = 0; i < pool.size(); ++i)
		{
			if (empire != 0 && pool[i].empire != empire)
				continue;
			if (chosen.size() < (size_t)row.bSize)
				chosen.push_back(pool[i]);
			else if (!shaman && pool[i].shaman)
				shaman = &pool[i];
		}
		// A crowd with no Shaman takes the first one left over in place of its
		// last member: its buffs are worth more than one more blade.
		if (shaman && chosen.size() > 1)
		{
			bool has = false;
			for (size_t i = 0; i < chosen.size(); ++i)
				if (chosen[i].shaman)
					has = true;
			if (!has)
				chosen.back() = *shaman;
		}
		// MT2009_PLUS_AREZZO_BOTS_V1 (raid): on an Arezzo map two are enough to
		// begin; the gathering calls more (the stall rule reinforces).
		const int need = IsPlayerBotArezzoMap(row.lMap) ? 2 : std::max(2, ((int)row.bSize + 1) / 2);
		if ((int)chosen.size() < need)
		{
			// Once in ten minutes a boss, each boss on its own: one throttle for
			// all of them showed the first boss of the minute and hid the rest.
			static std::map<size_t, DWORD> s_mapLogged;
			std::map<size_t, DWORD>::const_iterator logged = s_mapLogged.find(rowIndex);
			if (logged == s_mapLogged.end() || dwNow - logged->second >= PLAYERBOT_BOSS_RAID_CENSUS_MS)
			{
				s_mapLogged[rowIndex] = dwNow;
				sys_log(0, "PLAYERBOT_RAID: nobody to call boss=%s race=%u map=%ld in_band=%d free=%d/%d/%d need=%d",
						GetPlayerBotWorldBossName(row.wRace), (unsigned int)row.wRace, row.lMap, inBand,
						perEmpire[1], perEmpire[2], perEmpire[3], need);
				if (row.lMap == PLAYERBOT_MAP_OCHAO)
					LogPlayerBotOchaoRaidRefusals(row, boss, dwNow);
				if (IsPlayerBotArezzoMap(row.lMap))
					LogPlayerBotArezzoRaidRefusals(row, boss, dwNow); // MT2009_PLUS_AREZZO_BOTS_V1
				else if (row.lMap != PLAYERBOT_MAP_OCHAO)
					LogPlayerBotBossRaidRefusals(row, dwNow); // MT2009_PLUS_BOSS_RAID_V2
			}
			return false;
		}
		TPlayerBotBossRaid& raid = s_mapPlayerBotBossRaids[TPlayerBotBossKey(row.lMap, row.wRace)];
		raid = TPlayerBotBossRaid();
		raid.row = rowIndex;
		raid.bEmpire = empire;
		raid.bPhase = BOSS_RAID_PHASE_GATHER;
		raid.dwCalledAt = dwNow;
		raid.dwPhaseSince = dwNow;
		raid.dwBossVID = (DWORD)boss->GetVID();
		raid.lBossX = boss->GetX();
		raid.lBossY = boss->GetY();
		raid.iBestHP = boss->GetHP();
		raid.dwLastProgress = dwNow;
		if (row.lMap == PLAYERBOT_MAP_OCHAO)
		{
			if (!GetPlayerBotOchaoMuster(raid.lBossX, raid.lBossY, raid.lMusterX, raid.lMusterY))
				raid.lMusterX = raid.lMusterY = 0;
			raid.lMusterForX = raid.lBossX;
			raid.lMusterForY = raid.lBossY;
			raid.dwNextTopUp = dwNow + PLAYERBOT_OCHAO_RAID_TOPUP_MS;
		}
		int onMap = 0;
		bool withShaman = false;
		for (size_t i = 0; i < chosen.size(); ++i)
		{
			EnlistPlayerBotBossRaider(raid, chosen[i].pid, dwNow);
			if (chosen[i].onMap)
				++onMap;
			if (chosen[i].shaman)
				withShaman = true;
		}
		++s_uPlayerBotBossRaidsFormed;
		if (row.lMap == PLAYERBOT_MAP_OCHAO)
			LogPlayerBotOchaoRaidMembers("formed", raid, boss);
		sys_log(0, "PLAYERBOT_RAID: formed boss=%s race=%u map=%ld pos=(%ld,%ld) empire=%u members=%u on_map=%d shaman=%d need=%d in_band=%d",
				GetPlayerBotWorldBossName(row.wRace), (unsigned int)row.wRace, row.lMap, raid.lBossX, raid.lBossY,
				(unsigned int)empire, (unsigned int)raid.members.size(), onMap, withShaman ? 1 : 0, need, inBand);
		return true;
	}

	// A raid the boss is outpacing takes a few more of its kingdom (of every
	// kingdom for a raid of all three) - MT2009_PLUS_BOSS_RAID_V2: a round of
	// PLAYERBOT_BOSS_RAID_REINFORCEMENTS, never past twice the raid's size.
	int ReinforcePlayerBotBossRaid(TPlayerBotBossRaid& raid, LPCHARACTER boss, DWORD dwNow)
	{
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[raid.row];
		const int room = std::min(PLAYERBOT_BOSS_RAID_REINFORCEMENTS,
				2 * (int)row.bSize - (int)raid.members.size());
		if (room <= 0)
			return 0;
		std::vector<TPlayerBotBossRecruit> pool;
		int inBand = 0;
		CollectPlayerBotBossRecruits(row, boss, raid.bEmpire, dwNow, pool, inBand);
		int added = 0;
		for (size_t i = 0; i < pool.size() && added < room; ++i)
		{
			EnlistPlayerBotBossRaider(raid, pool[i].pid, dwNow);
			++added;
		}
		return added;
	}

	void EndPlayerBotBossRaid(TPlayerBotBossRaidMap::iterator it, DWORD dwNow, const char* why, DWORD cooldown)
	{
		TPlayerBotBossRaid& raid = it->second;
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[raid.row];
		for (std::set<DWORD>::const_iterator m = raid.members.begin(); m != raid.members.end(); ++m)
		{
			TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(*m);
			if (st == s_mapPlayerBotAIStates.end() || st->second.wBossRaidRace != row.wRace ||
					st->second.lBossRaidMap != row.lMap)
				continue;
			ClearPlayerBotBossRaidState(st->second, CHARACTER_MANAGER::instance().FindByPID(*m), raid.dwBossVID);
		}
		s_mapPlayerBotBossRaidNext[it->first] = dwNow + cooldown;
		sys_log(0, "PLAYERBOT_RAID: over boss=%s race=%u map=%ld empire=%u phase=%d members=%u after_s=%u why=%s",
				GetPlayerBotWorldBossName(row.wRace), (unsigned int)row.wRace, row.lMap, (unsigned int)raid.bEmpire,
				(int)raid.bPhase, (unsigned int)raid.members.size(), (dwNow - raid.dwCalledAt) / 1000U, why);
		s_mapPlayerBotBossRaids.erase(it);
	}

	// One raid moved along; false when it has ended (and been erased).
	bool AdvancePlayerBotBossRaid(TPlayerBotBossRaidMap::iterator it, DWORD dwNow)
	{
		TPlayerBotBossRaid& raid = it->second;
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[raid.row];
		// Members gone from the world, or taken off the raid by their own pass.
		for (std::set<DWORD>::iterator m = raid.members.begin(); m != raid.members.end();)
		{
			TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(*m);
			LPCHARACTER member = CHARACTER_MANAGER::instance().FindByPID(*m);
			if (!member || st == s_mapPlayerBotAIStates.end() ||
					st->second.wBossRaidRace != row.wRace || st->second.lBossRaidMap != row.lMap)
				raid.members.erase(m++);
			// MT2009_PLUS_OCHAO_BOTS_V1 (leaving): a member another pass has sent
			// out of the temple walks out; it is no longer the raid's.
			else if ((row.lMap == PLAYERBOT_MAP_OCHAO && IsPlayerBotOchaoLeaving(member)) ||
					(IsPlayerBotArezzoMap(row.lMap) && IsPlayerBotArezzoLeaving(member))) // MT2009_PLUS_AREZZO_BOTS_V1
			{
				sys_log(0, "PLAYERBOT_RAID: member leaves the temple pid=%u name=%s race=%u",
						*m, member->GetName(), (unsigned int)row.wRace);
				ClearPlayerBotBossRaidState(st->second, member, raid.dwBossVID);
				raid.members.erase(m++);
			}
			else
				++m;
		}
		LPCHARACTER boss = GetPlayerBotBossRaidBoss(raid);
		if (!boss)
		{
			// MT2009_PLUS_OCHAO_BOTS_V1 (kill): a member's kill between two looks at
			// the gathering is the raid's kill, not somebody else's.
			if (raid.bPhase == BOSS_RAID_PHASE_GATHER && row.lMap == PLAYERBOT_MAP_OCHAO &&
					raid.members.count(GetPlayerBotOchaoBossKiller(raid.dwBossVID)) != 0)
				raid.bPhase = BOSS_RAID_PHASE_FIGHT;
			if (raid.bPhase == BOSS_RAID_PHASE_GATHER)
			{
				// Down before the raid got to him: somebody else's kill.
				EndPlayerBotBossRaid(it, dwNow, "boss_gone", PLAYERBOT_BOSS_RAID_KILLED_COOLDOWN_MS);
				return false;
			}
			if (raid.bPhase == BOSS_RAID_PHASE_FIGHT)
			{
				raid.bPhase = BOSS_RAID_PHASE_LOOT;
				raid.dwPhaseSince = dwNow;
				++s_uPlayerBotBossRaidsKilled;
				const unsigned int minutes = (dwNow - raid.dwCalledAt) / 60000U;
				sys_log(0, "PLAYERBOT_RAID: killed boss=%s race=%u map=%ld empire=%u members=%u after_s=%u reinforced=%d",
						GetPlayerBotWorldBossName(row.wRace), (unsigned int)row.wRace, row.lMap,
						(unsigned int)raid.bEmpire, (unsigned int)raid.members.size(),
						(dwNow - raid.dwCalledAt) / 1000U, (int)raid.bReinforced);
				if (row.bAnnounce)
				{
					char notice[200];
					if (raid.bEmpire == 0) // MT2009_PLUS_BOSS_RAID_V2: every kingdom's raid
						snprintf(notice, sizeof(notice), "Boty trzech krolestw pokonaly: %s (%u min).",
								GetPlayerBotWorldBossName(row.wRace), std::max(1U, minutes));
					else
						snprintf(notice, sizeof(notice), "Boty z krolestwa %s pokonaly: %s (%u min).",
								GetPlayerBotKingdomName(raid.bEmpire), GetPlayerBotWorldBossName(row.wRace),
								std::max(1U, minutes));
					BroadcastNotice(notice);
				}
				return true;
			}
			if (dwNow - raid.dwPhaseSince >= PLAYERBOT_BOSS_RAID_LOOT_MS)
			{
				EndPlayerBotBossRaid(it, dwNow, "killed", PLAYERBOT_BOSS_RAID_KILLED_COOLDOWN_MS);
				return false;
			}
			return true;
		}
		raid.lBossX = boss->GetX();
		raid.lBossY = boss->GetY();
		if (raid.members.empty())
		{
			EndPlayerBotBossRaid(it, dwNow, "no_members", PLAYERBOT_BOSS_RAID_TOO_FEW_COOLDOWN_MS);
			return false;
		}
		if (raid.bPhase == BOSS_RAID_PHASE_GATHER)
		{
			// MT2009_PLUS_OCHAO_BOTS_V1 (top-up): the temple's raid let go of the
			// members sent back to the gate and filled up again.
			// MT2009_PLUS_OCHAO_BOTS_V1 (muster): he has walked off (a boss
			// chases, a lone one wanders) - the muster goes with him.
			if (row.lMap == PLAYERBOT_MAP_OCHAO &&
					DISTANCE_APPROX(raid.lBossX - raid.lMusterForX, raid.lBossY - raid.lMusterForY) > PLAYERBOT_OCHAO_RAID_MUSTER_MOVE)
			{
				long mx = 0, my = 0;
				if (GetPlayerBotOchaoMuster(raid.lBossX, raid.lBossY, mx, my))
				{
					sys_log(0, "OCHAO_BOT: raid muster moved race=%u boss=(%ld,%ld) muster=(%ld,%ld) -> (%ld,%ld)",
							(unsigned int)row.wRace, raid.lBossX, raid.lBossY, raid.lMusterX, raid.lMusterY, mx, my);
					raid.lMusterX = mx;
					raid.lMusterY = my;
				}
				raid.lMusterForX = raid.lBossX;
				raid.lMusterForY = raid.lBossY;
			}
			if (row.lMap == PLAYERBOT_MAP_OCHAO && dwNow >= raid.dwNextTopUp)
			{
				raid.dwNextTopUp = dwNow + PLAYERBOT_OCHAO_RAID_TOPUP_MS;
				TopUpPlayerBotOchaoRaid(raid, boss, dwNow);
				if (raid.members.empty())
				{
					EndPlayerBotBossRaid(it, dwNow, "no_members", PLAYERBOT_BOSS_RAID_TOO_FEW_COOLDOWN_MS);
					return false;
				}
			}
			int arrived = 0;
			bool started = false;
			LPCHARACTER victim = boss->GetVictim();
			for (std::set<DWORD>::const_iterator m = raid.members.begin(); m != raid.members.end(); ++m)
			{
				LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(*m);
				if (!c || c->IsDead() || c->GetMapIndex() != row.lMap)
					continue;
				if (c == victim)
					started = true;
				if (DISTANCE_APPROX(c->GetX() - raid.lBossX, c->GetY() - raid.lBossY) <= PLAYERBOT_BOSS_RAID_ARRIVED_RANGE)
					++arrived;
				// MT2009_PLUS_OCHAO_BOTS_V1 (muster): or at the muster.
				else if (raid.lMusterX != 0 &&
						DISTANCE_APPROX(c->GetX() - raid.lMusterX, c->GetY() - raid.lMusterY) <= PLAYERBOT_OCHAO_RAID_MUSTER_ARRIVED)
					++arrived;
			}
			int need = std::max(2, ((int)row.bSize + 1) / 2);
			const bool timeUp = dwNow - raid.dwPhaseSince >= (row.lMap == PLAYERBOT_MAP_OCHAO
					? PLAYERBOT_OCHAO_RAID_GATHER_MS : (row.lMap == PLAYERBOT_MAP_AREZZO_FOREST // MT2009_PLUS_AREZZO_BOTS_V1
					? PLAYERBOT_AREZZO_FOREST_RAID_GATHER_MS : (IsPlayerBotArezzoMap(row.lMap)
					? PLAYERBOT_AREZZO_RAID_GATHER_MS : PLAYERBOT_BOSS_RAID_GATHER_MS))); // MT2009_PLUS_OCHAO_BOTS_V1
			// MT2009_PLUS_OCHAO_BOTS_V1 (gather): in the labyrinth the walk is
			// long and the packs on it many - at the end of the gathering two
			// who have come go in, and the rest follow them to him.
			if ((row.lMap == PLAYERBOT_MAP_OCHAO || IsPlayerBotArezzoMap(row.lMap)) && timeUp) // MT2009_PLUS_AREZZO_BOTS_V1
				need = 2;
			if (arrived >= (int)raid.members.size() || arrived >= (int)row.bSize || started ||
					(timeUp && arrived >= need))
			{
				raid.bPhase = BOSS_RAID_PHASE_FIGHT;
				raid.dwPhaseSince = dwNow;
				raid.iBestHP = boss->GetHP();
				raid.dwLastProgress = dwNow;
				if (row.lMap == PLAYERBOT_MAP_OCHAO)
					LogPlayerBotOchaoRaidMembers("engaged", raid, boss);
				sys_log(0, "PLAYERBOT_RAID: engaged boss=%s race=%u map=%ld arrived=%d of %u started_by_him=%d after_s=%u",
						GetPlayerBotWorldBossName(row.wRace), (unsigned int)row.wRace, row.lMap, arrived,
						(unsigned int)raid.members.size(), started ? 1 : 0, (dwNow - raid.dwCalledAt) / 1000U);
			}
			else if (timeUp)
			{
				if (row.lMap == PLAYERBOT_MAP_OCHAO)
					LogPlayerBotOchaoRaidMembers("too_few_came", raid, boss);
				++s_uPlayerBotBossRaidsTooFew;
				EndPlayerBotBossRaid(it, dwNow, "too_few_came", PLAYERBOT_BOSS_RAID_TOO_FEW_COOLDOWN_MS);
				return false;
			}
			return true;
		}
		if (raid.bPhase == BOSS_RAID_PHASE_FIGHT)
		{
			const int meaningful = std::max(1, boss->GetMaxHP() / 200);
			if (boss->GetHP() + meaningful <= raid.iBestHP)
			{
				raid.iBestHP = boss->GetHP();
				raid.dwLastProgress = dwNow;
				raid.bStalledEmpty = false;
			}
			// MT2009_PLUS_OCHAO_BOTS_V1 (stall): in the labyrinth a fight he
			// started on the first to come is a fight the rest are still
			// walking to - the clock runs once one of them stands at him.
			else if (row.lMap == PLAYERBOT_MAP_OCHAO && CountPlayerBotBossRaidAt(raid, boss) == 0)
				raid.dwLastProgress = dwNow;
			if (dwNow - raid.dwLastProgress >= PLAYERBOT_BOSS_RAID_STALL_MS)
			{
				// MT2009_PLUS_BOSS_RAID_V2 (2.2.51): up to three rounds, each
				// counted only when somebody came; a round with nobody to call
				// falls through to the finish rule below.
				int added = 0;
				if ((int)raid.bReinforced < PLAYERBOT_BOSS_RAID_REINFORCE_ROUNDS)
				{
					added = ReinforcePlayerBotBossRaid(raid, boss, dwNow);
					sys_log(0, "PLAYERBOT_RAID: outpaced, calling more boss=%s race=%u map=%ld hp=%d/%d added=%d members=%u round=%d",
							GetPlayerBotWorldBossName(row.wRace), (unsigned int)row.wRace, row.lMap,
							boss->GetHP(), boss->GetMaxHP(), added, (unsigned int)raid.members.size(),
							(int)raid.bReinforced + (added > 0 ? 1 : 0));
					if (added > 0)
						++raid.bReinforced;
				}
				const bool finishing = boss->GetMaxHP() > 0 &&
						(long long)boss->GetHP() * 100 < (long long)boss->GetMaxHP() * PLAYERBOT_BOSS_RAID_FINISH_PERCENT;
				if (added > 0)
				{
					raid.dwLastProgress = dwNow;
					raid.bStalledEmpty = false;
				}
				// MT2009_PLUS_BOSS_RAID_V2 (2.2.51): a boss under thirty percent
				// is finished off - the raid gave the Baroness up at 11%
				// (29 September). FIGHT_MAX_MS still ends it.
				else if (finishing)
				{
					raid.dwLastProgress = dwNow;
					sys_log(0, "PLAYERBOT_RAID: outpaced, finishing him boss=%s race=%u map=%ld hp=%d/%d members=%u",
							GetPlayerBotWorldBossName(row.wRace), (unsigned int)row.wRace, row.lMap,
							boss->GetHP(), boss->GetMaxHP(), (unsigned int)raid.members.size());
				}
				else if (!raid.bStalledEmpty)
				{
					raid.bStalledEmpty = true;
					raid.dwLastProgress = dwNow;
				}
				else
				{
					++s_uPlayerBotBossRaidsOutpaced;
					sys_log(0, "PLAYERBOT_RAID: outpaced, giving up boss=%s race=%u map=%ld hp=%d/%d best_hp=%d members=%u",
							GetPlayerBotWorldBossName(row.wRace), (unsigned int)row.wRace, row.lMap,
							boss->GetHP(), boss->GetMaxHP(), raid.iBestHP, (unsigned int)raid.members.size());
					EndPlayerBotBossRaid(it, dwNow, "outpaced", PLAYERBOT_BOSS_RAID_OUTPACED_COOLDOWN_MS);
					return false;
				}
			}
			else if (dwNow - raid.dwPhaseSince >= PLAYERBOT_BOSS_RAID_FIGHT_MAX_MS)
			{
				EndPlayerBotBossRaid(it, dwNow, "fight_too_long", PLAYERBOT_BOSS_RAID_OUTPACED_COOLDOWN_MS);
				return false;
			}
		}
		return true;
	}

	// The world's pass: the raids under way moved along, and a raid called to
	// every boss standing with none.
	void ManagePlayerBotBossRaids(DWORD dwNow)
	{
		if (s_dwPlayerBotBossRaidsStartedAt == 0)
			s_dwPlayerBotBossRaidsStartedAt = dwNow;
		if (dwNow < s_dwNextPlayerBotBossRaidCheck)
			return;
		s_dwNextPlayerBotBossRaidCheck = dwNow + PLAYERBOT_BOSS_RAID_CHECK_MS;
		// The cohort spawns over the first minutes, and a raid called then
		// takes whoever happened to be first.
		if (dwNow - s_dwPlayerBotBossRaidsStartedAt < PLAYERBOT_BOSS_RAID_FIRST_DELAY_MS)
			return;

		for (TPlayerBotBossRaidMap::iterator it = s_mapPlayerBotBossRaids.begin(); it != s_mapPlayerBotBossRaids.end();)
		{
			TPlayerBotBossRaidMap::iterator current = it++;
			AdvancePlayerBotBossRaid(current, dwNow);
		}

		for (size_t i = 0; i < PLAYERBOT_WORLD_BOSS_COUNT; ++i)
		{
			const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[i];
			const TPlayerBotBossKey key(row.lMap, row.wRace);
			if (s_mapPlayerBotBossRaids.find(key) != s_mapPlayerBotBossRaids.end())
				continue;
			std::map<TPlayerBotBossKey, DWORD>::const_iterator next = s_mapPlayerBotBossRaidNext.find(key);
			if (next != s_mapPlayerBotBossRaidNext.end() && dwNow < next->second)
				continue;
			if (!IsPlayerBotMapHostedHere(row.lMap))
			{
				s_mapPlayerBotBossRaidNext[key] = dwNow + PLAYERBOT_BOSS_RAID_CENSUS_MS;
				continue;
			}
			LPCHARACTER boss = FindPlayerBotWorldBoss(row.lMap, row.wRace);
			if (!boss)
			{
				s_mapPlayerBotBossRaidNext[key] = dwNow + PLAYERBOT_BOSS_RAID_DOWN_RECHECK_MS;
				continue;
			}
			if (!CallPlayerBotBossRaid(i, boss, dwNow))
				s_mapPlayerBotBossRaidNext[key] = dwNow + PLAYERBOT_BOSS_RAID_CALL_RETRY_MS;
		}

		if (s_dwNextPlayerBotBossRaidCensus == 0 || dwNow >= s_dwNextPlayerBotBossRaidCensus)
		{
			s_dwNextPlayerBotBossRaidCensus = dwNow + PLAYERBOT_BOSS_RAID_CENSUS_MS;
			sys_log(0, "PLAYERBOT_RAID: census formed=%u killed=%u outpaced=%u too_few=%u active=%u",
					s_uPlayerBotBossRaidsFormed, s_uPlayerBotBossRaidsKilled, s_uPlayerBotBossRaidsOutpaced,
					s_uPlayerBotBossRaidsTooFew, (unsigned int)s_mapPlayerBotBossRaids.size());
		}
	}

	// The per-bot pass: a bot called to a boss owns its tick until the raid
	// is over. After the loot pass, so the fall's drops are picked up, and
	// beside the tower's hook, whose fight and keeping alive it borrows.
	bool ManagePlayerBotBossRaid(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (state.wBossRaidRace == 0 || !ch || ch->IsDead())
			return false;
		const DWORD pid = ch->GetPlayerID();
		TPlayerBotBossRaid* raid = FindPlayerBotBossRaid(state.lBossRaidMap, state.wBossRaidRace);
		if (!raid || raid->members.find(pid) == raid->members.end())
		{
			ClearPlayerBotBossRaidState(state, ch, 0);
			return false;
		}
		// A person's party is the person's: the bot leaves the raid for it.
		if (ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty()))
		{
			raid->members.erase(pid);
			ClearPlayerBotBossRaidState(state, ch, raid->dwBossVID);
			sys_log(0, "PLAYERBOT_RAID: member left for a person's party pid=%u name=%s", pid, ch->GetName());
			return false;
		}
		// His fall: the loot pass above this hook has its window, and the
		// rest of the tick is the bot's own again until the raid disbands.
		if (raid->bPhase == BOSS_RAID_PHASE_LOOT)
			return false;
		const TPlayerBotWorldBoss& row = PLAYERBOT_WORLD_BOSSES[raid->row];
		if (ch->GetMapIndex() != row.lMap)
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			if (dwNow < state.dwNextBossRaidMoveTime)
				return true;
			state.dwNextBossRaidMoveTime = dwNow + 5000;
			long rallyX = 0, rallyY = 0;
			GetPlayerBotBossRaidRally(pid, *raid, rallyX, rallyY);
			TransitionPlayerBotMap(ch, state, row.lMap, rallyX, rallyY, dwNow, "boss_raid");
			return true;
		}
		if (KeepPlayerBotTowerAlive(ch, state, dwNow))
			return true;
		LPCHARACTER boss = GetPlayerBotBossRaidBoss(*raid);
		if (!boss)
			return false;
		const int distance = DISTANCE_APPROX(ch->GetX() - boss->GetX(), ch->GetY() - boss->GetY());
		// From the far end of his own map the walk would outlast the gathering
		// and come after the fight: brought to its spot, as a member from
		// another map is.
		// MT2009_PLUS_OCHAO_BOTS_V1 (walk): never inside the labyrinth - there
		// the walk is the way, planned round its walls.
		// MT2009_PLUS_AREZZO_BOTS_V1 (walk): nor on an Arezzo map - there the
		// walk is its routes.
		if (distance > PLAYERBOT_BOSS_RAID_WALK_MAX && row.lMap != PLAYERBOT_MAP_OCHAO && !IsPlayerBotArezzoMap(row.lMap))
		{
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			if (dwNow < state.dwNextBossRaidMoveTime)
				return true;
			state.dwNextBossRaidMoveTime = dwNow + 5000;
			long rallyX = 0, rallyY = 0;
			GetPlayerBotBossRaidRally(pid, *raid, rallyX, rallyY);
			TransitionPlayerBotMap(ch, state, row.lMap, rallyX, rallyY, dwNow, "boss_raid_far");
			return true;
		}
		LPCHARACTER bossVictim = boss->GetVictim();
		const bool bossStarted = bossVictim && bossVictim->IsPC() &&
				raid->members.find(bossVictim->GetPlayerID()) != raid->members.end();
		if (raid->bPhase == BOSS_RAID_PHASE_FIGHT || bossStarted)
		{
			if (BuffPlayerBotTowerFellows(ch, state, dwNow))
				return true;
			// What hits the bot on its way to him is answered on the way; at
			// him, he is the fight.
			if (distance > PLAYERBOT_BOSS_RAID_RALLY_MIN)
			{
				LPCHARACTER engaged = FindPlayerBotEngagedTarget(ch, &state, dwNow);
				if (engaged && engaged != boss)
					return FightPlayerBotTowerObjective(ch, state, engaged, dwNow);
			}
			// MT2009_PLUS_OCHAO_BOTS_V1 (walk): still far from him in the
			// labyrinth - by its corners, not at him through the walls.
			if (row.lMap == PLAYERBOT_MAP_OCHAO && distance > PLAYERBOT_OCHAO_RAID_FIGHT_WALK &&
					WalkPlayerBotInOchao(ch, state, boss->GetX(), boss->GetY(), dwNow))
				return true;
			// MT2009_PLUS_AREZZO_BOTS_V1 (walk): on an Arezzo map by its routes.
			if (IsPlayerBotArezzoMap(row.lMap) && distance > PLAYERBOT_AREZZO_RAID_FIGHT_WALK &&
					WalkPlayerBotInArezzo(ch, state, boss->GetX(), boss->GetY(), dwNow))
				return true;
			return FightPlayerBotTowerObjective(ch, state, boss, dwNow);
		}
		// Gathering: what attacks the bot is fought where it comes, the
		// Shaman buffs whoever has come, and the rest is the walk to its spot
		// and its own buffs there.
		LPCHARACTER engaged = FindPlayerBotEngagedTarget(ch, &state, dwNow);
		if (engaged && engaged != boss)
			return FightPlayerBotTowerObjective(ch, state, engaged, dwNow);
		if (BuffPlayerBotTowerFellows(ch, state, dwNow))
			return true;
		long rallyX = 0, rallyY = 0;
		GetPlayerBotBossRaidRally(pid, *raid, rallyX, rallyY);
		const int toRally = DISTANCE_APPROX(ch->GetX() - rallyX, ch->GetY() - rallyY);
		if (toRally <= 600)
		{
			// Buffs are cast on foot; a battle horse's rider is taken down by
			// the buff pass itself when one is missing.
			if (ch->IsRiding() && !HasPlayerBotBattleHorse(ch))
			{
				SetPlayerBotRidingForTravel(ch, state, false, dwNow, "boss_raid");
				return true;
			}
			if (ManagePlayerBotCombatBuffs(ch, state, dwNow, true))
				return true;
		}
		state.dwTargetVID = 0;
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		// MT2009_PLUS_OCHAO_BOTS_V1 (walk): in the labyrinth by its corners.
		if (row.lMap == PLAYERBOT_MAP_OCHAO)
		{
			if (toRally > 400)
				WalkPlayerBotInOchao(ch, state, rallyX, rallyY, dwNow);
			return true;
		}
		// MT2009_PLUS_AREZZO_BOTS_V1 (walk): on an Arezzo map by its routes.
		if (IsPlayerBotArezzoMap(row.lMap))
		{
			if (toRally > 400 && dwNow >= state.dwNextBossRaidMoveTime)
			{
				state.dwNextBossRaidMoveTime = dwNow + 1000;
				WalkPlayerBotInArezzo(ch, state, rallyX, rallyY, dwNow);
			}
			return true;
		}
		if (toRally > 400 && dwNow >= state.dwNextBossRaidMoveTime)
		{
			state.dwNextBossRaidMoveTime = dwNow + 2000;
			MovePlayerBot(ch, rallyX, rallyY, dwNow, 8, true, toRally > 4000);
		}
		return true;
	}
}

#endif
