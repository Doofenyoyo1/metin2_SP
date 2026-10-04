#ifndef __INC_METIN2_PLAYERBOT_GUILD_LAND_H__
#define __INC_METIN2_PLAYERBOT_GUILD_LAND_H__

// A bot guild's land and what it builds on it (the operator, 27 September).
//
// The master of a bot guild buys a land the way a player does - at the land's
// own price (world.land), from guild level guild_level_limit, one land a
// guild, in its own kingdom (guild_building.quest) - and builds on it in the
// order the engine allows and the operator asked for: the headquarters, then
// a blacksmith, then an alchemist. Each building costs what object_proto says,
// yang and the three building materials (Kamien Wegielny 90010, Pien 90011,
// Dykta 90012), taken from the master's bag the way /build takes them
// (cmd_gm.cpp do_build).
//
// Which blacksmith: the weapon smith on a map that has none, then the armour
// smith, then the jeweller, and at random once a map has all three. Which
// alchemist: the ebony one first (14050), then one of the others a map lacks,
// and at random once it has them all.
//
// The materials are goods now. No bot sells one to a merchant: they go on the
// counters at PLAYERBOT_GUILD_MATERIAL_BASE_PRICE a piece before the rate
// curve and the inflation (GetPlayerBotMaterialAskingBase), and a master
// whose next building lacks some buys them there like any other purchase.
//
// The money. The master pays first, from what it can spare. What it cannot,
// the guild collects ("zrzutka") - and fairly:
//   - only from the guild's bots in this world, never from a person;
//   - every member keeps its reserve untouched - its own reserved gold and
//     max(PLAYERBOT_GUILD_DONOR_RESERVE_MIN, level^2 * ..._PER_LEVEL_SQ) -
//     and of what is above it gives at most PLAYERBOT_GUILD_DONOR_SHARE_PERCENT;
//   - the sum is split in proportion to what each can spare, so a rich member
//     gives more and a poor one little or nothing;
//   - a member gives to every stage while it has something above its
//     reserve - the land, then each building - and never below it;
//   - nothing is taken unless the whole sum can be raised: a collection that
//     would fall short takes nobody's money and is tried again later.
// Every payment is a row of player.playerbot_guild_contribution.
//
// A material nobody has on a counter is fetched: members of the level for it
// are sent to hunt where it drops (PLAYERBOT_GUILD_MATERIAL_GROUNDS,
// GetPlayerBotGuildErrandMap), and every member of the guild in this world
// hands the pieces the next building lacks to the master.
//
// The materials' money is collected ahead of the purchases and kept in the
// master's purse as the guild's fund (playerbot_guild.build_fund): the fund
// is added to the master's reserved gold, so no other purchase of its spends
// it, and each material bought for the building comes out of it.
//
// On the maps with a guild blacksmith the bots refine their gear of level
// thirty and up there (the engine's +10 percentage points and its fee, which
// pays the owning guild its share; ChoosePlayerBotRefineGuildSmith). The
// alchemists are built and stand; the bots do not smelt at them yet.
//
// An implementation fragment in the sense playerbot_types.h describes: the
// master's pass is called from ManagePlayerBotGuild (first channel), the
// hooks from the market, the counters, the merchant and the blacksmith.

#include "building.h"

namespace
{
	const DWORD PLAYERBOT_GUILD_MATERIAL_VNUMS[3] = { 90010, 90011, 90012 };
	// The operator's price of a piece, before the rate curve and the inflation.
	const DWORD PLAYERBOT_GUILD_MATERIAL_BASE_PRICE = 40000;
	// object_proto groups (dwGroupVnum): one building of a group a land.
	const DWORD PLAYERBOT_GUILD_GROUP_HQ = 1;
	const DWORD PLAYERBOT_GUILD_GROUP_SMITH = 2;
	const DWORD PLAYERBOT_GUILD_GROUP_ALCHEMIST = 3;
	const DWORD PLAYERBOT_GUILD_HQ_VNUMS[3] = { 14100, 14110, 14120 };
	// Weapon, armour, jewellery - the order a map gets them in.
	const DWORD PLAYERBOT_GUILD_SMITH_VNUMS[3] = { 14013, 14014, 14015 };
	// The ebony alchemist first, then the others (there is no 14044).
	const DWORD PLAYERBOT_GUILD_ALCHEMIST_VNUMS[] = {
		14050, 14043, 14045, 14046, 14047, 14048, 14049, 14051, 14052, 14053, 14054, 14055 };
	// The guild smiths' NPCs (refine.h BLACKSMITH_*_MOB).
	const DWORD PLAYERBOT_GUILD_SMITH_NPC_WEAPON = 20044;
	const DWORD PLAYERBOT_GUILD_SMITH_NPC_ARMOUR = 20045;
	const DWORD PLAYERBOT_GUILD_SMITH_NPC_JEWEL = 20046;
	// Gear from this level up goes to a guild smith where a map has one.
	const int PLAYERBOT_GUILD_SMITH_MIN_ITEM_LEVEL = 30;

	// The collection's rules (see the head of the file).
	const long long PLAYERBOT_GUILD_DONOR_RESERVE_MIN = 1000000;
	const long long PLAYERBOT_GUILD_DONOR_RESERVE_PER_LEVEL_SQ = 300;
	const int PLAYERBOT_GUILD_DONOR_SHARE_PERCENT = 25;
	// The master keeps a reserve of its own too and pays this share of the rest.
	const long long PLAYERBOT_GUILD_MASTER_RESERVE = 500000;
	const int PLAYERBOT_GUILD_MASTER_SHARE_PERCENT = 90;
	// A collection that fell short, or a land nobody's could be bought, is
	// tried again after this.
	const DWORD PLAYERBOT_GUILD_LAND_RETRY_MS = 3 * 60 * 60 * 1000;
	// The materials' fund: the missing pieces at the asking price, and this
	// much over it for a counter that asks more.
	const int PLAYERBOT_GUILD_MATERIAL_FUND_PERCENT = 125;
	// A counter's line of materials is bought up to this multiple of what the
	// bots ask for it.
	const int PLAYERBOT_GUILD_MATERIAL_FAIR_PERCENT = 150;
	// Where on the land a building may stand: this far in from every edge
	// (the largest footprint is 525), on a grid of this step.
	const long PLAYERBOT_GUILD_BUILD_MARGIN = 600;
	const long PLAYERBOT_GUILD_BUILD_STEP = 300;

	enum EPlayerBotGuildLandStage
	{
		GUILD_LAND_STAGE_LAND,
		GUILD_LAND_STAGE_HQ,
		GUILD_LAND_STAGE_SMITH,
		GUILD_LAND_STAGE_ALCHEMIST,
		GUILD_LAND_STAGE_DONE,
	};

	struct TPlayerBotGuildLandState
	{
		DWORD dwNextTry;
		// The fund and whose purse holds it.
		long long llFund;
		DWORD dwFundHolder;
		// What the next building still needs of each material, as of the last
		// pass (the market asks it between the passes).
		int aiNeed[3];
		DWORD dwBuildVnum;
		// The stage as of the last pass (EPlayerBotGuildLandStage).
		BYTE bStage;
		TPlayerBotGuildLandState() : dwNextTry(0), llFund(0), dwFundHolder(0), dwBuildVnum(0), bStage(0)
		{
			aiNeed[0] = aiNeed[1] = aiNeed[2] = 0;
		}
	};
	std::map<DWORD, TPlayerBotGuildLandState> s_mapPlayerBotGuildLand;
	bool s_bPlayerBotGuildFundLoaded = false;
	// Where each material drops, for whom, and on which map - the monsters of
	// mob_drop_item.txt that carry it (Mount Sohan's ice for the log, the
	// Fireland and the Orc Chief for the stone, the Hwang Temple's frogs and
	// bosses for the plywood).
	struct TPlayerBotGuildMaterialGround { DWORD vnum; long map; int minLevel; };
	const TPlayerBotGuildMaterialGround PLAYERBOT_GUILD_MATERIAL_GROUNDS[] = {
		{ 90011, PLAYERBOT_MAP_SOHAN, 60 },
		{ 90010, PLAYERBOT_MAP_FIRE_LAND, 68 },
		{ 90010, PLAYERBOT_MAP_ORC_VALLEY, 50 },
		{ 90012, PLAYERBOT_MAP_HWANG, 57 },
	};
	// How many members a guild sends at once, and for how long an errand
	// stands before it is looked at again.
	const size_t PLAYERBOT_GUILD_ERRANDS_MAX = 4;
	const DWORD PLAYERBOT_GUILD_ERRAND_MS = 2 * 60 * 60 * 1000;
	struct TPlayerBotGuildErrand { DWORD dwGuild; DWORD dwVnum; long lMap; DWORD dwUntil; };
	std::map<DWORD, TPlayerBotGuildErrand> s_mapPlayerBotGuildErrand;

	DWORD GetPlayerBotGuildMaterialBasePrice()
	{
		return PLAYERBOT_GUILD_MATERIAL_BASE_PRICE;
	}

	bool IsPlayerBotGuildBuildMaterial(DWORD vnum)
	{
		return vnum == PLAYERBOT_GUILD_MATERIAL_VNUMS[0] || vnum == PLAYERBOT_GUILD_MATERIAL_VNUMS[1] ||
				vnum == PLAYERBOT_GUILD_MATERIAL_VNUMS[2];
	}

	int GetPlayerBotGuildMaterialIndex(DWORD vnum)
	{
		for (int i = 0; i < 3; ++i)
			if (PLAYERBOT_GUILD_MATERIAL_VNUMS[i] == vnum)
				return i;
		return -1;
	}

	// The kingdom a land's map belongs to: the first village and the guild map
	// of each (the lands stand on maps 1/4, 21/24 and 41/44).
	BYTE GetPlayerBotLandMapEmpire(long mapIndex)
	{
		switch (mapIndex)
		{
			case 1: case 3: case 4: return 1;
			case 21: case 23: case 24: return 2;
			case 41: case 43: case 44: return 3;
		}
		return 0;
	}

	bool IsPlayerBotVillageLandMap(long mapIndex)
	{
		return mapIndex == 1 || mapIndex == 21 || mapIndex == 41;
	}

	// Every land's id, once: the lands come with the boot and never change.
	const std::vector<DWORD>& GetPlayerBotLandIDs()
	{
		static std::vector<DWORD> s_ids;
		static bool s_done = false;
		if (!s_done)
		{
			for (DWORD id = 1; id <= 2048; ++id)
				if (building::CManager::instance().FindLand(id))
					s_ids.push_back(id);
			// Asked before the boot delivered them: ask again next time.
			s_done = !s_ids.empty();
		}
		return s_ids;
	}

	// ------------------------------------------------------------------ fund

	void LoadPlayerBotGuildFunds()
	{
		s_bPlayerBotGuildFundLoaded = true;
		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
				"SELECT guild_id, build_fund, fund_holder FROM player.playerbot_guild WHERE build_fund > 0"));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			DWORD gid = 0, holder = 0;
			long long fund = 0;
			if (row[0]) str_to_number(gid, row[0]);
			if (row[1]) str_to_number(fund, row[1]);
			if (row[2]) str_to_number(holder, row[2]);
			if (gid == 0 || fund <= 0)
				continue;
			s_mapPlayerBotGuildLand[gid].llFund = fund;
			s_mapPlayerBotGuildLand[gid].dwFundHolder = holder;
		}
	}

	void SavePlayerBotGuildFund(DWORD gid, const TPlayerBotGuildLandState& land)
	{
		DBManager::instance().Query(
				"UPDATE player.playerbot_guild SET build_fund = %lld, fund_holder = %u WHERE guild_id = %u",
				land.llFund > 0 ? land.llFund : 0LL, land.dwFundHolder, gid);
	}

	// The part of this bot's purse that is its guild's (see the head of the
	// file); GetPlayerBotReservedGold keeps it out of every other purchase.
	long long GetPlayerBotGuildFundReserve(DWORD pid)
	{
		if (!s_bPlayerBotGuildFundLoaded)
			LoadPlayerBotGuildFunds();
		for (std::map<DWORD, TPlayerBotGuildLandState>::const_iterator it = s_mapPlayerBotGuildLand.begin();
				it != s_mapPlayerBotGuildLand.end(); ++it)
			if (it->second.dwFundHolder == pid && it->second.llFund > 0)
				return it->second.llFund;
		return 0;
	}

	void LogPlayerBotGuildContribution(DWORD gid, DWORD pid, long long amount, const char* purpose)
	{
		DBManager::instance().Query(
				"INSERT INTO player.playerbot_guild_contribution (guild_id, pid, amount, purpose, at) "
				"VALUES (%u, %u, %lld, '%s', NOW())", gid, pid, amount, purpose);
	}

	// ------------------------------------------------------------ collection

	long long GetPlayerBotGuildDonorReserve(LPCHARACTER ch)
	{
		const long long level = ch ? (long long)ch->GetLevel() : 0;
		return std::max(PLAYERBOT_GUILD_DONOR_RESERVE_MIN, level * level * PLAYERBOT_GUILD_DONOR_RESERVE_PER_LEVEL_SQ) +
				(ch ? (long long)GetPlayerBotReservedGold(ch) : 0);
	}

	// Makes `need` spendable for the master (above its reserve and the
	// guild's fund): the master's own share of it first - most of what it can
	// spare - and the rest from the members. False, and nobody's money moved,
	// when the whole sum cannot be raised. Nothing is collected when the
	// master can pay it all by itself.
	bool CollectPlayerBotGuildMoney(LPCHARACTER master, CGuild* guild, long long need, const char* purpose)
	{
		if (!master || !guild || need <= 0)
			return need <= 0;
		const DWORD gid = guild->GetID();
		const long long masterSpare = (long long)master->GetGold() - (long long)GetPlayerBotReservedGold(master) -
				PLAYERBOT_GUILD_MASTER_RESERVE;
		if (masterSpare >= need)
			return true;
		const long long masterPart = std::min(need, std::max(0LL, masterSpare * PLAYERBOT_GUILD_MASTER_SHARE_PERCENT / 100));
		const long long rest = need - masterPart;

		struct TDonor { LPCHARACTER ch; long long cap; long long give; };
		std::vector<TDonor> donors;
		long long capSum = 0;
		if (rest > 0)
		{
			for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
					it != s_mapPlayerBotAIStates.end(); ++it)
			{
				LPCHARACTER member = CHARACTER_MANAGER::instance().FindByPID(it->first);
				if (!member || member == master || member->GetGuild() != guild || member->IsDead() ||
						IsPlayerBotSidekickPID(it->first))
					continue;
				const long long spare = (long long)member->GetGold() - GetPlayerBotGuildDonorReserve(member);
				const long long cap = spare > 0 ? spare * PLAYERBOT_GUILD_DONOR_SHARE_PERCENT / 100 : 0;
				if (cap <= 0)
					continue;
				TDonor d = { member, cap, 0 };
				donors.push_back(d);
				capSum += cap;
			}
			if (capSum < rest)
			{
				sys_log(0, "PLAYERBOT_GUILD_LAND: collection short guild=%s id=%u purpose=%s need=%lld master_part=%lld members_can=%lld donors=%u",
						guild->GetName(), gid, purpose, need, masterPart, capSum, (unsigned int)donors.size());
				return false;
			}
			// In proportion to what each can spare; the rounding's remainder
			// goes to whoever still has room under its cap.
			long long assigned = 0;
			for (size_t i = 0; i < donors.size(); ++i)
			{
				donors[i].give = std::min(donors[i].cap, (long long)((long double)rest * donors[i].cap / capSum));
				assigned += donors[i].give;
			}
			for (size_t i = 0; i < donors.size() && assigned < rest; ++i)
			{
				const long long more = std::min(rest - assigned, donors[i].cap - donors[i].give);
				donors[i].give += more;
				assigned += more;
			}
		}

		if (masterPart > 0)
			LogPlayerBotGuildContribution(gid, master->GetPlayerID(), masterPart, purpose);
		for (size_t i = 0; i < donors.size(); ++i)
		{
			if (donors[i].give <= 0)
				continue;
			PlayerBotChangeGold(donors[i].ch, -donors[i].give);
			PlayerBotChangeGold(master, donors[i].give);
			LogPlayerBotGuildContribution(gid, donors[i].ch->GetPlayerID(), donors[i].give, purpose);
			sys_log(0, "PLAYERBOT_GUILD_LAND: gave pid=%u name=%s guild=%s amount=%lld cap=%lld purpose=%s",
					donors[i].ch->GetPlayerID(), donors[i].ch->GetName(), guild->GetName(),
					donors[i].give, donors[i].cap, purpose);
		}
		sys_log(0, "PLAYERBOT_GUILD_LAND: collected guild=%s id=%u purpose=%s need=%lld master_part=%lld members=%lld",
				guild->GetName(), gid, purpose, need, masterPart, rest > 0 ? rest : 0LL);
		return true;
	}

	// --------------------------------------------------------------- the land

	// The cheapest free land of the master's kingdom the guild's level allows:
	// its village first, where its members come to refine, then its guild map.
	building::CLand* PickPlayerBotGuildLand(LPCHARACTER master, CGuild* guild)
	{
		building::CLand* best = NULL;
		bool bestVillage = false;
		const std::vector<DWORD>& ids = GetPlayerBotLandIDs();
		for (size_t i = 0; i < ids.size(); ++i)
		{
			building::CLand* land = building::CManager::instance().FindLand(ids[i]);
			if (!land)
				continue;
			const building::TLand& data = land->GetData();
			if (data.dwGuildID != 0 || GetPlayerBotLandMapEmpire(data.lMapIndex) != master->GetEmpire() ||
					guild->GetLevel() < data.bGuildLevelLimit)
				continue;
			const bool village = IsPlayerBotVillageLandMap(data.lMapIndex);
			if (!best || (village && !bestVillage) ||
					(village == bestVillage && data.dwPrice < best->GetData().dwPrice))
			{
				best = land;
				bestVillage = village;
			}
		}
		return best;
	}

	bool BuyPlayerBotGuildLand(LPCHARACTER master, CGuild* guild, TPlayerBotGuildLandState& land, DWORD dwNow)
	{
		building::CLand* pick = PickPlayerBotGuildLand(master, guild);
		if (!pick)
		{
			land.dwNextTry = dwNow + PLAYERBOT_GUILD_LAND_RETRY_MS;
			return false;
		}
		const building::TLand data = pick->GetData();
		const long long price = (long long)data.dwPrice;
		if (!CollectPlayerBotGuildMoney(master, guild, price, "land"))
		{
			land.dwNextTry = dwNow + PLAYERBOT_GUILD_LAND_RETRY_MS;
			return false;
		}
		if ((long long)master->GetGold() - (long long)GetPlayerBotReservedGold(master) < price)
			return false;
		PlayerBotChangeGold(master, -price);
		pick->SetOwner(guild->GetID());
		sys_log(0, "PLAYERBOT_GUILD_LAND: bought land=%u map=%ld price=%lld guild=%s id=%u level=%u master=%s gold_left=%lld",
				data.dwID, data.lMapIndex, price, guild->GetName(), guild->GetID(), (unsigned int)guild->GetLevel(),
				master->GetName(), (long long)master->GetGold());
		return true;
	}

	// ------------------------------------------------ errands and handover

	// What the guild's next building still lacks of a material: its recipe,
	// less what the master holds when the master is in this world.
	int GetPlayerBotGuildMaterialNeed(CGuild* guild, int idx)
	{
		if (!guild || idx < 0 || idx > 2)
			return 0;
		std::map<DWORD, TPlayerBotGuildLandState>::const_iterator it = s_mapPlayerBotGuildLand.find(guild->GetID());
		if (it == s_mapPlayerBotGuildLand.end() || !it->second.dwBuildVnum)
			return 0;
		int need = it->second.aiNeed[idx];
		LPCHARACTER master = CHARACTER_MANAGER::instance().FindByPID(guild->GetMasterPID());
		if (master)
			need -= (int)master->CountSpecifyItem(PLAYERBOT_GUILD_MATERIAL_VNUMS[idx]);
		return std::max(0, need);
	}

	// Every member in this world hands the master the pieces the next
	// building lacks - a gift to the guild like the collection's yang, logged
	// the same way at what a counter would ask for them.
	void CollectPlayerBotGuildMaterials(LPCHARACTER master, CGuild* guild)
	{
		for (int idx = 0; idx < 3; ++idx)
		{
			const DWORD vnum = PLAYERBOT_GUILD_MATERIAL_VNUMS[idx];
			int need = GetPlayerBotGuildMaterialNeed(guild, idx);
			for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
					need > 0 && it != s_mapPlayerBotAIStates.end(); ++it)
			{
				LPCHARACTER member = CHARACTER_MANAGER::instance().FindByPID(it->first);
				if (!member || member == master || member->GetGuild() != guild || !member->IsItemLoaded())
					continue;
				const int have = (int)member->CountSpecifyItem(vnum);
				if (have <= 0)
					continue;
				if (master->CountSpecifyItem(vnum) == 0 && master->GetEmptyInventory(1) < 0)
					return;
				const int give = std::min(have, need);
				member->RemoveSpecifyItem(vnum, give);
				master->AutoGiveItem(vnum, give);
				need -= give;
				const long long value = (long long)give * GetPlayerBotMaterialAskingBase(vnum);
				LogPlayerBotGuildContribution(guild->GetID(), member->GetPlayerID(), value, "material");
				sys_log(0, "PLAYERBOT_GUILD_LAND: handed pid=%u name=%s to=%s guild=%s vnum=%u count=%d",
						member->GetPlayerID(), member->GetName(), master->GetName(), guild->GetName(), vnum, give);
			}
		}
	}

	// The members sent to fetch what no counter has: up to
	// PLAYERBOT_GUILD_ERRANDS_MAX of a guild at once, two a material, the
	// strongest of the level for its ground first.
	void SendPlayerBotGuildErrands(CGuild* guild, DWORD dwNow)
	{
		size_t guildErrands = 0;
		std::set<DWORD> errandVnums;
		for (std::map<DWORD, TPlayerBotGuildErrand>::iterator it = s_mapPlayerBotGuildErrand.begin();
				it != s_mapPlayerBotGuildErrand.end(); )
		{
			if (it->second.dwGuild != guild->GetID()) { ++it; continue; }
			const int idx = GetPlayerBotGuildMaterialIndex(it->second.dwVnum);
			if (dwNow >= it->second.dwUntil || GetPlayerBotGuildMaterialNeed(guild, idx) <= 0)
			{
				s_mapPlayerBotGuildErrand.erase(it++);
				continue;
			}
			++guildErrands;
			errandVnums.insert(it->second.dwVnum);
			++it;
		}
		for (int idx = 0; idx < 3 && guildErrands < PLAYERBOT_GUILD_ERRANDS_MAX; ++idx)
		{
			const DWORD vnum = PLAYERBOT_GUILD_MATERIAL_VNUMS[idx];
			const int need = GetPlayerBotGuildMaterialNeed(guild, idx);
			if (need <= 0 || errandVnums.count(vnum))
				continue;
			// On the counters already: the master buys it there.
			const TPlayerBotMarketLedgerEntry* supply = GetPlayerBotMarketLedgerEntry(vnum);
			if (supply && (int)supply->dwSupplyUnits >= need)
				continue;
			for (size_t g = 0; g < sizeof(PLAYERBOT_GUILD_MATERIAL_GROUNDS) / sizeof(PLAYERBOT_GUILD_MATERIAL_GROUNDS[0]); ++g)
			{
				const TPlayerBotGuildMaterialGround& ground = PLAYERBOT_GUILD_MATERIAL_GROUNDS[g];
				if (ground.vnum != vnum)
					continue;
				std::vector<std::pair<int, LPCHARACTER> > fit;
				for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
						it != s_mapPlayerBotAIStates.end(); ++it)
				{
					LPCHARACTER member = CHARACTER_MANAGER::instance().FindByPID(it->first);
					if (!member || member->GetGuild() != guild || member->GetPlayerID() == guild->GetMasterPID() ||
							(int)member->GetLevel() < ground.minLevel || IsPlayerBotSidekickPID(it->first) ||
							IsPlayerBotDropper(it->second.bPersonality) ||
							s_mapPlayerBotGuildErrand.count(it->first))
						continue;
					fit.push_back(std::make_pair(-(int)member->GetLevel(), member));
				}
				if (fit.empty())
					continue;
				std::sort(fit.begin(), fit.end());
				for (size_t f = 0; f < fit.size() && f < 2 && guildErrands < PLAYERBOT_GUILD_ERRANDS_MAX; ++f)
				{
					TPlayerBotGuildErrand errand = { guild->GetID(), vnum, ground.map, dwNow + PLAYERBOT_GUILD_ERRAND_MS };
					s_mapPlayerBotGuildErrand[fit[f].second->GetPlayerID()] = errand;
					++guildErrands;
					sys_log(0, "PLAYERBOT_GUILD_LAND: errand pid=%u name=%s level=%u guild=%s vnum=%u map=%ld need=%d",
							fit[f].second->GetPlayerID(), fit[f].second->GetName(), (unsigned int)fit[f].second->GetLevel(),
							guild->GetName(), vnum, ground.map, need);
				}
				break;
			}
		}
	}

	// MT2009_PLUS_GUILD_DUTY_V1 (errand): defined in playerbot_guildduty.h.
	long GetPlayerBotGuildDutyMap(LPCHARACTER ch);
	bool IsPlayerBotGuildDutyKeptItem(LPCHARACTER ch, DWORD vnum);

	// The map a member on a guild errand hunts on (playerbot_travel.h asks it
	// before its level's own), or zero.
	long GetPlayerBotGuildErrandMap(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		// MT2009_PLUS_GUILD_DUTY_V1 (errand): a player's guild's item mission
		// sends its workers to the material's ground (playerbot_guildduty.h).
		{
			const long dutyMap = GetPlayerBotGuildDutyMap(ch);
			if (dutyMap != 0)
				return dutyMap;
		}
		std::map<DWORD, TPlayerBotGuildErrand>::iterator it = s_mapPlayerBotGuildErrand.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotGuildErrand.end())
			return 0;
		CGuild* guild = ch->GetGuild();
		if (!guild || guild->GetID() != it->second.dwGuild || get_dword_time() >= it->second.dwUntil ||
				GetPlayerBotGuildMaterialNeed(guild, GetPlayerBotGuildMaterialIndex(it->second.dwVnum)) <= 0)
		{
			s_mapPlayerBotGuildErrand.erase(it);
			return 0;
		}
		return it->second.lMap;
	}

	// --------------------------------------------------------- the buildings

	// The buildings are known to the core that holds their map only
	// (CManager::LoadObject), the lands to every core: what stands where is
	// read from player.object, so a master anywhere knows its land's stage.
	void QueryPlayerBotObjectVnums(const char* where, std::vector<DWORD>& out)
	{
		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
				"SELECT o.vnum FROM player.object o JOIN player.guild_land gl ON gl.land_id = o.land_id WHERE %s", where));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			DWORD vnum = 0;
			if (row[0]) str_to_number(vnum, row[0]);
			if (vnum)
				out.push_back(vnum);
		}
	}

	DWORD GetPlayerBotObjectGroup(DWORD vnum)
	{
		const building::TObjectProto* proto = building::CManager::instance().GetObjectProto(vnum);
		return proto ? proto->dwGroupVnum : 0;
	}

	// What stands on the map already: a count of each vnum of a group.
	void CountPlayerBotGuildBuildings(long mapIndex, DWORD group, std::map<DWORD, int>& out)
	{
		char where[64];
		snprintf(where, sizeof(where), "o.map_index = %ld", mapIndex);
		std::vector<DWORD> vnums;
		QueryPlayerBotObjectVnums(where, vnums);
		for (size_t i = 0; i < vnums.size(); ++i)
			if (GetPlayerBotObjectGroup(vnums[i]) == group)
				++out[vnums[i]];
	}

	bool PlayerBotLandHasGroup(const std::vector<DWORD>& vnums, DWORD group)
	{
		for (size_t i = 0; i < vnums.size(); ++i)
			if (GetPlayerBotObjectGroup(vnums[i]) == group)
				return true;
		return false;
	}

	DWORD ChoosePlayerBotGuildBuilding(building::CLand* land, CGuild* guild, EPlayerBotGuildLandStage stage)
	{
		const long mapIndex = land->GetData().lMapIndex;
		if (stage == GUILD_LAND_STAGE_HQ)
			return PLAYERBOT_GUILD_HQ_VNUMS[guild->GetID() % 3];
		const DWORD* list = stage == GUILD_LAND_STAGE_SMITH ? PLAYERBOT_GUILD_SMITH_VNUMS : PLAYERBOT_GUILD_ALCHEMIST_VNUMS;
		const size_t count = stage == GUILD_LAND_STAGE_SMITH
				? sizeof(PLAYERBOT_GUILD_SMITH_VNUMS) / sizeof(DWORD)
				: sizeof(PLAYERBOT_GUILD_ALCHEMIST_VNUMS) / sizeof(DWORD);
		std::map<DWORD, int> standing;
		CountPlayerBotGuildBuildings(mapIndex, stage == GUILD_LAND_STAGE_SMITH
				? PLAYERBOT_GUILD_GROUP_SMITH : PLAYERBOT_GUILD_GROUP_ALCHEMIST, standing);
		// The first of the list the map lacks - for an alchemist the ebony one,
		// then one of the missing at random - and at random once it has all.
		std::vector<DWORD> missing;
		for (size_t i = 0; i < count; ++i)
			if (standing.find(list[i]) == standing.end())
				missing.push_back(list[i]);
		if (missing.empty())
			return list[number(0, (int)count - 1)];
		if (stage == GUILD_LAND_STAGE_SMITH || missing[0] == PLAYERBOT_GUILD_ALCHEMIST_VNUMS[0])
			return missing[0];
		return missing[number(0, (int)missing.size() - 1)];
	}

	// Where on the land the building goes: the engine's own test
	// (RequestCreateObject: inside the land, clear ground, nobody standing
	// there) on a grid from the middle out. True when the request went out.
	bool PlacePlayerBotGuildBuilding(building::CLand* land, DWORD vnum)
	{
		const building::TLand& data = land->GetData();
		const long cx = data.x + data.width / 2;
		const long cy = data.y + data.height / 2;
		const long reach = std::min(data.width, data.height) / 2 - PLAYERBOT_GUILD_BUILD_MARGIN;
		for (long ring = 0; ring * PLAYERBOT_GUILD_BUILD_STEP <= reach; ++ring)
		{
			const long r = ring * PLAYERBOT_GUILD_BUILD_STEP;
			for (long dx = -r; dx <= r; dx += PLAYERBOT_GUILD_BUILD_STEP)
				for (long dy = -r; dy <= r; dy += PLAYERBOT_GUILD_BUILD_STEP)
				{
					if (ring > 0 && labs(dx) != r && labs(dy) != r)
						continue;
					if (land->RequestCreateObject(vnum, data.lMapIndex, cx + dx, cy + dy, 0.0f, 0.0f, 0.0f, true))
						return true;
				}
		}
		return false;
	}

	EPlayerBotGuildLandStage GetPlayerBotGuildLandStage(building::CLand* land)
	{
		if (!land)
			return GUILD_LAND_STAGE_LAND;
		char where[64];
		snprintf(where, sizeof(where), "o.land_id = %u", land->GetID());
		std::vector<DWORD> vnums;
		QueryPlayerBotObjectVnums(where, vnums);
		if (!PlayerBotLandHasGroup(vnums, PLAYERBOT_GUILD_GROUP_HQ))
			return GUILD_LAND_STAGE_HQ;
		if (!PlayerBotLandHasGroup(vnums, PLAYERBOT_GUILD_GROUP_SMITH))
			return GUILD_LAND_STAGE_SMITH;
		if (!PlayerBotLandHasGroup(vnums, PLAYERBOT_GUILD_GROUP_ALCHEMIST))
			return GUILD_LAND_STAGE_ALCHEMIST;
		return GUILD_LAND_STAGE_DONE;
	}

	// The next building: its materials (the missing ones bought at the
	// counters out of the fund), then its yang, then the building itself -
	// the last only with the master on the land's map, whose core holds it.
	void BuildPlayerBotGuildBuilding(LPCHARACTER master, CGuild* guild, building::CLand* pkLand,
			EPlayerBotGuildLandStage stage, TPlayerBotGuildLandState& land, DWORD dwNow)
	{
		if (!land.dwBuildVnum)
			land.dwBuildVnum = ChoosePlayerBotGuildBuilding(pkLand, guild, stage);
		const building::TObjectProto* proto = building::CManager::instance().GetObjectProto(land.dwBuildVnum);
		if (!proto || proto->dwGroupVnum != (stage == GUILD_LAND_STAGE_HQ ? PLAYERBOT_GUILD_GROUP_HQ :
				stage == GUILD_LAND_STAGE_SMITH ? PLAYERBOT_GUILD_GROUP_SMITH : PLAYERBOT_GUILD_GROUP_ALCHEMIST))
		{
			land.dwBuildVnum = 0;
			return;
		}

		for (int m = 0; m < 3; ++m)
			land.aiNeed[m] = 0;
		for (int i = 0; i < building::OBJECT_MATERIAL_MAX_NUM && proto->kMaterials[i].dwItemVnum; ++i)
		{
			const int idx = GetPlayerBotGuildMaterialIndex(proto->kMaterials[i].dwItemVnum);
			if (idx >= 0)
				land.aiNeed[idx] = (int)proto->kMaterials[i].dwCount;
		}
		// What the members carry comes to the master first.
		CollectPlayerBotGuildMaterials(master, guild);
		long long missingValue = 0;
		bool missingAny = false;
		for (int i = 0; i < building::OBJECT_MATERIAL_MAX_NUM && proto->kMaterials[i].dwItemVnum; ++i)
		{
			const int have = (int)master->CountSpecifyItem(proto->kMaterials[i].dwItemVnum);
			const int need = (int)proto->kMaterials[i].dwCount;
			if (have < need)
			{
				missingAny = true;
				missingValue += (long long)(need - have) *
						GetPlayerBotMaterialAskingBase(proto->kMaterials[i].dwItemVnum);
			}
		}

		if (missingAny)
		{
			// What no counter carries, members go and fetch.
			SendPlayerBotGuildErrands(guild, dwNow);
			// The fund for the missing pieces, raised once; the market spends it.
			const long long want = missingValue * PLAYERBOT_GUILD_MATERIAL_FUND_PERCENT / 100;
			if (land.dwFundHolder != master->GetPlayerID())
			{
				land.llFund = 0;
				land.dwFundHolder = master->GetPlayerID();
			}
			if (land.llFund < want)
			{
				const long long top = want - land.llFund;
				if (!CollectPlayerBotGuildMoney(master, guild, top, "materials"))
				{
					land.dwNextTry = dwNow + PLAYERBOT_GUILD_LAND_RETRY_MS;
					return;
				}
				land.llFund = want;
				SavePlayerBotGuildFund(guild->GetID(), land);
				sys_log(0, "PLAYERBOT_GUILD_LAND: material fund guild=%s building=%u fund=%lld need=%d/%d/%d have=%d/%d/%d",
						guild->GetName(), land.dwBuildVnum, land.llFund,
						land.aiNeed[0], land.aiNeed[1], land.aiNeed[2],
						(int)master->CountSpecifyItem(90010), (int)master->CountSpecifyItem(90011), (int)master->CountSpecifyItem(90012));
			}
			return;
		}

		// Every piece is in the bag: what is left of the fund goes back to
		// the master's purse as its own, and the building is paid.
		if (land.llFund > 0)
		{
			land.llFund = 0;
			SavePlayerBotGuildFund(guild->GetID(), land);
		}
		const building::TLand& data = pkLand->GetData();
		if (master->GetMapIndex() != data.lMapIndex)
			return;
		const long long price = (long long)proto->dwPrice;
		if (!CollectPlayerBotGuildMoney(master, guild, price, "building"))
		{
			land.dwNextTry = dwNow + PLAYERBOT_GUILD_LAND_RETRY_MS;
			return;
		}
		if ((long long)master->GetGold() - (long long)GetPlayerBotReservedGold(master) < price)
			return;
		if (!PlacePlayerBotGuildBuilding(pkLand, land.dwBuildVnum))
		{
			sys_log(0, "PLAYERBOT_GUILD_LAND: no room for building=%u land=%u guild=%s",
					land.dwBuildVnum, data.dwID, guild->GetName());
			land.dwNextTry = dwNow + PLAYERBOT_GUILD_LAND_RETRY_MS;
			return;
		}
		PlayerBotChangeGold(master, -price);
		for (int i = 0; i < building::OBJECT_MATERIAL_MAX_NUM && proto->kMaterials[i].dwItemVnum; ++i)
			master->RemoveSpecifyItem(proto->kMaterials[i].dwItemVnum, proto->kMaterials[i].dwCount);
		sys_log(0, "PLAYERBOT_GUILD_LAND: built building=%u land=%u map=%ld price=%lld guild=%s master=%s gold_left=%lld",
				land.dwBuildVnum, data.dwID, data.lMapIndex, price, guild->GetName(), master->GetName(),
				(long long)master->GetGold());
		land.dwBuildVnum = 0;
		for (int m = 0; m < 3; ++m)
			land.aiNeed[m] = 0;
	}

	// The master's pass, from ManagePlayerBotGuild on the first channel.
	void ManagePlayerBotGuildLand(LPCHARACTER master, CGuild* guild, DWORD dwNow)
	{
		if (!master || !guild || guild->GetMasterPID() != master->GetPlayerID())
			return;
		if (!s_bPlayerBotGuildFundLoaded)
			LoadPlayerBotGuildFunds();
		TPlayerBotGuildLandState& land = s_mapPlayerBotGuildLand[guild->GetID()];
		if (dwNow < land.dwNextTry)
			return;
		building::CLand* pkLand = building::CManager::instance().FindLandByGuild(guild->GetID());
		if (!pkLand)
		{
			BuyPlayerBotGuildLand(master, guild, land, dwNow);
			return;
		}
		const EPlayerBotGuildLandStage stage = GetPlayerBotGuildLandStage(pkLand);
		land.bStage = (BYTE)stage;
		if (stage == GUILD_LAND_STAGE_DONE)
		{
			land.dwBuildVnum = 0;
			return;
		}
		// A building chosen for another stage (it went up meanwhile): choose again.
		if (land.dwBuildVnum)
		{
			const building::TObjectProto* proto = building::CManager::instance().GetObjectProto(land.dwBuildVnum);
			if (!proto || proto->dwGroupVnum != (stage == GUILD_LAND_STAGE_HQ ? PLAYERBOT_GUILD_GROUP_HQ :
					stage == GUILD_LAND_STAGE_SMITH ? PLAYERBOT_GUILD_GROUP_SMITH : PLAYERBOT_GUILD_GROUP_ALCHEMIST))
				land.dwBuildVnum = 0;
		}
		BuildPlayerBotGuildBuilding(master, guild, pkLand, stage, land, dwNow);
	}

	// ------------------------------------------------------------ the market

	// How many more pieces of this material the master's next building needs.
	int GetPlayerBotGuildMaterialWant(LPCHARACTER ch, DWORD vnum)
	{
		const int idx = GetPlayerBotGuildMaterialIndex(vnum);
		CGuild* guild = ch ? ch->GetGuild() : NULL;
		if (idx < 0 || !guild || guild->GetMasterPID() != ch->GetPlayerID())
			return 0;
		std::map<DWORD, TPlayerBotGuildLandState>::const_iterator it = s_mapPlayerBotGuildLand.find(guild->GetID());
		if (it == s_mapPlayerBotGuildLand.end() || !it->second.dwBuildVnum)
			return 0;
		return std::max(0, it->second.aiNeed[idx] - (int)ch->CountSpecifyItem(vnum));
	}

	// What a master's next building lacks, by vnum, and what it may spend on
	// it at the counters: the guild's fund, or its own spare gold without one.
	long long CollectPlayerBotGuildMaterialMissing(LPCHARACTER ch, std::map<DWORD, int>& out)
	{
		for (int idx = 0; idx < 3; ++idx)
		{
			const int want = GetPlayerBotGuildMaterialWant(ch, PLAYERBOT_GUILD_MATERIAL_VNUMS[idx]);
			if (want > 0)
				out[PLAYERBOT_GUILD_MATERIAL_VNUMS[idx]] = want;
		}
		if (out.empty())
			return 0;
		const long long fund = GetPlayerBotGuildFundReserve(ch->GetPlayerID());
		if (fund > 0)
			return std::min(fund, (long long)ch->GetGold());
		return std::max(0LL, (long long)ch->GetGold() - (long long)GetPlayerBotReservedGold(ch));
	}

	// A counter's line of materials for the master's building: at a fair price,
	// out of the fund (or the master's own spare gold when there is none).
	bool CanPlayerBotPayForGuildMaterial(LPCHARACTER ch, LPITEM item, long long price)
	{
		if (!ch || !item || price <= 0 || GetPlayerBotGuildMaterialWant(ch, item->GetVnum()) <= 0)
			return false;
		const long long fair = (long long)GetPlayerBotShopAskingPrice(item);
		if (fair <= 0 || price > fair * PLAYERBOT_GUILD_MATERIAL_FAIR_PERCENT / 100 || price > (long long)ch->GetGold())
			return false;
		const long long fund = GetPlayerBotGuildFundReserve(ch->GetPlayerID());
		if (fund > 0)
			return price <= fund;
		return price <= (long long)ch->GetGold() - (long long)GetPlayerBotReservedGold(ch);
	}

	// A purchase of materials by a master: out of its guild's fund.
	void NotePlayerBotGuildMaterialBought(LPCHARACTER ch, DWORD vnum, long long price)
	{
		if (!ch || !IsPlayerBotGuildBuildMaterial(vnum) || price <= 0)
			return;
		for (std::map<DWORD, TPlayerBotGuildLandState>::iterator it = s_mapPlayerBotGuildLand.begin();
				it != s_mapPlayerBotGuildLand.end(); ++it)
		{
			if (it->second.dwFundHolder != ch->GetPlayerID() || it->second.llFund <= 0)
				continue;
			it->second.llFund = std::max(0LL, it->second.llFund - price);
			SavePlayerBotGuildFund(it->first, it->second);
			sys_log(0, "PLAYERBOT_GUILD_LAND: material bought pid=%u name=%s vnum=%u price=%lld fund_left=%lld",
					ch->GetPlayerID(), ch->GetName(), vnum, price, it->second.llFund);
			return;
		}
	}

	// A master keeps the pieces its next building needs off its counter.
	bool IsPlayerBotKeptGuildMaterial(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !IsPlayerBotGuildBuildMaterial(item->GetVnum()))
			return false;
		// MT2009_PLUS_GUILD_DUTY_V1 (keep): a worker of an item mission keeps
		// the material for the guild's bank (playerbot_guildduty.h).
		if (IsPlayerBotGuildDutyKeptItem(ch, item->GetVnum()))
			return true;
		CGuild* guild = ch->GetGuild();
		if (!guild)
			return false;
		// A member keeps what its own guild's next building lacks, for the master.
		if (guild->GetMasterPID() != ch->GetPlayerID())
			return GetPlayerBotGuildMaterialNeed(guild, GetPlayerBotGuildMaterialIndex(item->GetVnum())) > 0;
		std::map<DWORD, TPlayerBotGuildLandState>::const_iterator it = s_mapPlayerBotGuildLand.find(guild->GetID());
		// Before its first pass the master keeps them all: its guild may need
		// them; once its land has every building, they are goods again.
		return it == s_mapPlayerBotGuildLand.end() || it->second.bStage != GUILD_LAND_STAGE_DONE;
	}

	// ------------------------------------------------------- the guild smith

	int GetPlayerBotItemLevelLimit(LPITEM item)
	{
		if (!item)
			return 0;
		for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
			if (item->GetLimitType(i) == LIMIT_LEVEL)
				return (int)item->GetLimitValue(i);
		return 0;
	}

	// What a guild smith of this race takes (CHARACTER::CanReceiveItem, less
	// the distance).
	bool IsPlayerBotAwakenedWeaponVnum(DWORD vnum); // playerbot_awakening.h

	bool PlayerBotGuildSmithTakes(DWORD race, LPITEM item)
	{
		if (!item || !item->GetRefinedVnum())
			return false;
		// MT2009_PLUS_AWAKENING_V1: an awakened weapon is refined only by the
		// plain blacksmith - the engine refuses it at a guild smith.
		if (IsPlayerBotAwakenedWeaponVnum(item->GetVnum()))
			return false;
		const bool heavy = item->GetType() == ITEM_ARMOR && (item->GetSubType() == ARMOR_BODY ||
				item->GetSubType() == ARMOR_SHIELD || item->GetSubType() == ARMOR_HEAD);
		if (race == PLAYERBOT_GUILD_SMITH_NPC_WEAPON)
			return item->GetType() == ITEM_WEAPON;
		if (race == PLAYERBOT_GUILD_SMITH_NPC_ARMOUR)
			return heavy;
		if (race == PLAYERBOT_GUILD_SMITH_NPC_JEWEL)
			return item->GetType() == ITEM_ARMOR && !heavy;
		return false;
	}

	// The guild smith on this bot's map that takes this piece, the nearest.
	LPCHARACTER FindPlayerBotGuildSmithFor(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || GetPlayerBotItemLevelLimit(item) < PLAYERBOT_GUILD_SMITH_MIN_ITEM_LEVEL)
			return NULL;
		LPCHARACTER best = NULL;
		int bestDistance = INT_MAX;
		const std::vector<DWORD>& ids = GetPlayerBotLandIDs();
		for (size_t i = 0; i < ids.size(); ++i)
		{
			building::CLand* land = building::CManager::instance().FindLand(ids[i]);
			if (!land || land->GetOwner() == 0 || land->GetData().lMapIndex != ch->GetMapIndex())
				continue;
			building::LPOBJECT obj = land->FindObjectByGroup(PLAYERBOT_GUILD_GROUP_SMITH);
			LPCHARACTER npc = obj ? obj->GetNPC() : NULL;
			if (!npc || npc->IsDead() || !PlayerBotGuildSmithTakes(npc->GetRaceNum(), item))
				continue;
			const int distance = DISTANCE_APPROX(ch->GetX() - npc->GetX(), ch->GetY() - npc->GetY());
			if (distance < bestDistance)
			{
				best = npc;
				bestDistance = distance;
			}
		}
		return best;
	}

	// The smith this blacksmith visit goes to: a guild smith when a piece of
	// level thirty or more that the bot would refine now has one on this map,
	// decided once a visit (dwRefineGuildSmithVID; ~0 = the plain one).
	LPCHARACTER ChoosePlayerBotRefineGuildSmith(LPCHARACTER ch, TPlayerBotAIState& state)
	{
		if (!ch)
			return NULL;
		if (state.dwRefineGuildSmithVID == 0)
		{
			state.dwRefineGuildSmithVID = ~0U;
			LPCHARACTER chosen = NULL;
			const BYTE wearSlots[] = { WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
					WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR };
			for (size_t i = 0; !chosen && i < sizeof(wearSlots) / sizeof(wearSlots[0]); ++i)
			{
				LPITEM item = ch->GetWear(wearSlots[i]);
				if (CanPlayerBotAttemptRefineItem(ch, item))
					chosen = FindPlayerBotGuildSmithFor(ch, item);
			}
			for (WORD cell = 0; !chosen && cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (IsPlayerBotRefineBagCandidate(ch, item) && CanPlayerBotAttemptRefineItem(ch, item))
					chosen = FindPlayerBotGuildSmithFor(ch, item);
			}
			if (chosen)
			{
				state.dwRefineGuildSmithVID = chosen->GetVID();
				sys_log(0, "PLAYERBOT_GUILD_LAND: refines at the guild smith pid=%u name=%s smith=%u guild=%s map=%ld",
						ch->GetPlayerID(), ch->GetName(), chosen->GetRaceNum(),
						chosen->GetGuild() ? chosen->GetGuild()->GetName() : "-", ch->GetMapIndex());
			}
		}
		if (state.dwRefineGuildSmithVID == ~0U)
			return NULL;
		LPCHARACTER smith = CHARACTER_MANAGER::instance().Find(state.dwRefineGuildSmithVID);
		if (!smith || smith->IsDead() || smith->GetMapIndex() != ch->GetMapIndex())
		{
			state.dwRefineGuildSmithVID = ~0U;
			return NULL;
		}
		return smith;
	}

	// Whether this anvil takes this piece: a guild smith its own kind at
	// level thirty and up; the plain one everything but the pieces a guild
	// smith on this map would take (they wait for a visit there).
	bool PlayerBotRefineAnvilTakes(LPCHARACTER ch, const TPlayerBotAIState& state, LPITEM item, LPCHARACTER* pSmith)
	{
		if (pSmith)
			*pSmith = NULL;
		if (!ch || !item)
			return false;
		if (state.dwRefineGuildSmithVID != 0 && state.dwRefineGuildSmithVID != ~0U)
		{
			LPCHARACTER smith = CHARACTER_MANAGER::instance().Find(state.dwRefineGuildSmithVID);
			if (!smith || smith->IsDead() ||
					GetPlayerBotItemLevelLimit(item) < PLAYERBOT_GUILD_SMITH_MIN_ITEM_LEVEL ||
					!PlayerBotGuildSmithTakes(smith->GetRaceNum(), item) ||
					DISTANCE_APPROX(ch->GetX() - smith->GetX(), ch->GetY() - smith->GetY()) > 2000)
				return false;
			if (pSmith)
				*pSmith = smith;
			return true;
		}
		// The companion refines where its owner takes it (playerbot_sidekick.h),
		// never held back for a guild smith.
		if (IsPlayerBotSidekickPID(ch->GetPlayerID()))
			return true;
		return FindPlayerBotGuildSmithFor(ch, item) == NULL;
	}
}

#endif
