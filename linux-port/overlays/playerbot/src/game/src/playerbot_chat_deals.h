#ifndef __INC_METIN2_PLAYERBOT_CHAT_DEALS_H__
#define __INC_METIN2_PLAYERBOT_CHAT_DEALS_H__

// MT2009_PLUS_BOT_CHAT_V2 (deals) - a trade talked over on the whisper, done
// in the exchange window.
//
// The owner, 4 October: a bot posts "B> Zdobycz Dzikusa x10, kto ma? pw", a
// person whispers "czesc, mam do sprzedania zdobycz dzikusa" - and the bot
// has to know it is the answer to its post, name its price, bargain within
// what it can really pay, and then buy: meet the person, take the items in
// the window, put the yang in, accept. The same for a bot selling from its
// bag.
//
// The talk is the conversation's (playerbot_conv_generator.h, TDeal in the
// pair's memory). This file is the rest:
//   - QuotePlayerBotDealItem: what a whisper names, as an item of this world
//     (the bag, the counter, the wanted materials, the item table), its fair
//     price (the sale memory, the cheapest counter, else the asking price),
//     whether the bot buys it (a material it is short of, an open "K>" post
//     of its own, the weapon it saves for, a merchant's resale) and at what
//     most, and whether it has it to sell and at what least. A bot never pays
//     more than the market's price and a tenth (its own post's price for its
//     post) nor more than 60% of its purse; it never sells under 85% of the
//     market, nor what it is short of itself, nor what it wears.
//   - RegisterPlayerBotDeal: the settled deal, held for PLAYERBOT_DEAL_TTL_MS,
//     and the meeting - near (the window now), on the same map (the bot walks
//     over, the summon's walk), on another map of this core the bot may go to
//     (it goes over, then the summon's walk), anywhere else a village's
//     blacksmith: the village the person stands in, else the person's
//     kingdom's M1, else the bot's own - the bot goes there and waits
//     PLAYERBOT_DEAL_MEET_WAIT_MS, and says exactly where ("jestem w M1
//     Yongan, bede czekac przy kowalu"; the channel too when the person
//     plays on the other one). A bot that cannot leave its stall waits there.
//   - HandlePlayerBotDealTrade: the bot's tick while a deal waits. Near the
//     person it opens the window itself; in the window it checks what the
//     person put in - only the item, no more than agreed - and pays exactly
//     for it (CExchange::AddGold), or puts its own pieces in (a stack split
//     to the count) and waits for the person's yang
//     (CExchange::Mt2009PlusGetGold, server-patches/playerqol); it accepts
//     only after the person has, and only while the window still holds what
//     was checked. Done: a thanks, the post closed, the memory told. The
//     yang goes in only for the agreed count; a count other than agreed is
//     said as it is ("umawialismy sie na 2 szt, a dales 5"), and "zmieniles
//     ilosc" only when the pieces changed after the yang went in.
//   - ManagePlayerBotDealMeeting (inside HandlePlayerBotDealTrade): the way
//     to the landmark and the wait there, the bot held for company
//     meanwhile (IsPlayerBotDealMeeting).
//
// An implementation fragment: include it once, after
// playerbot_chat_conversation.h and playerbot_gift_trade.h; the manager's
// tick asks HandlePlayerBotDealTrade before the gift trade.

namespace
{
	const DWORD PLAYERBOT_DEAL_TTL_MS = 15 * 60 * 1000;
	const int PLAYERBOT_DEAL_NEAR = 800;
	const DWORD PLAYERBOT_DEAL_OPEN_RETRY_MS = 15000;
	const DWORD PLAYERBOT_DEAL_WINDOW_MAX_MS = 3 * 60 * 1000;
	const DWORD PLAYERBOT_DEAL_GLANCE_MIN_MS = 700;
	const DWORD PLAYERBOT_DEAL_GLANCE_MAX_MS = 1600;
	// The meeting at a landmark: a moment before it sets off (a scroll read),
	// the walk from the village's square to the smith, and the wait there.
	const DWORD PLAYERBOT_DEAL_MEET_LEAVE_MIN_MS = 2500;
	const DWORD PLAYERBOT_DEAL_MEET_LEAVE_MAX_MS = 5000;
	const DWORD PLAYERBOT_DEAL_MEET_WALK_MAX_MS = 90 * 1000;
	const DWORD PLAYERBOT_DEAL_MEET_WAIT_MS = 5 * 60 * 1000;
	const DWORD PLAYERBOT_DEAL_MEET_RETRY_MS = 5000;
	const int PLAYERBOT_DEAL_MEET_TRIES = 3;
	const int PLAYERBOT_DEAL_MEET_STAY = 300;       // at the spot within this
	const int PLAYERBOT_DEAL_MEET_SPOT_OFFSET = 250; // beside the smith, not in it
	// A person on a hunting map of this core is gone over to only by a bot
	// not this many levels under them (the map is theirs to survive).
	const int PLAYERBOT_DEAL_CROSS_LEVEL_SLACK = 5;
	// The person accepted the window with fewer pieces than agreed and keeps
	// it so this long after being told: the bot pays for those.
	const DWORD PLAYERBOT_DEAL_SHORT_ACCEPT_MS = 6000;

	struct TPlayerBotDeal
	{
		BYTE side;          // playerbot_conv::EDealSide
		DWORD vnum;
		DWORD skill;        // a skill book's skill (socket 0), 0 any other item
		int count;
		long long unit;
		DWORD at;
		DWORD nextOpenAt;
		DWORD windowSince;
		DWORD decideAt;
		bool paid;          // the bot's yang is in the window
		int paidPieces;
		bool offered;       // the bot's pieces are in the window
		DWORD warnedAt;
		int lastPieces;     // the person's pieces at the last look
		DWORD shortSince;   // told "doloz", the person still accepted with fewer
		// The meeting (playerbot_conv::EDealMeet, TDealMeetPlace).
		int meet;
		long meetMap;       // the village it waits in
		BYTE spot;          // playerbot_conv::EDealSpot
		long spotX, spotY;
		bool otherChannel;
		bool crossToPerson; // going over to the person's map of this core first
		DWORD leaveAt;      // sets off then
		DWORD walkSince;
		DWORD arrivedAt;    // at the spot; the wait runs from here
		DWORD nextTryAt;
		int tries;
		bool travelled;     // it went somewhere for this meeting (said on arrival)
		TPlayerBotDeal() : side(0), vnum(0), skill(0), count(0), unit(0), at(0), nextOpenAt(0), windowSince(0), decideAt(0),
			paid(false), paidPieces(0), offered(false), warnedAt(0), lastPieces(0), shortSince(0),
			meet(playerbot_conv::DEAL_MEET_FAILED), meetMap(0), spot(playerbot_conv::DEAL_SPOT_NONE), spotX(0), spotY(0),
			otherChannel(false), crossToPerson(false), leaveAt(0), walkSince(0), arrivedAt(0), nextTryAt(0), tries(0),
			travelled(false) {}
	};
	typedef std::pair<DWORD, DWORD> TPlayerBotDealKey; // (bot pid, person pid)
	std::map<TPlayerBotDealKey, TPlayerBotDeal> s_mapPlayerBotDeals;

	// ------------------------------------------------------------ pricing

	// MT2009_PLUS_BOT_CHAT_V2 (deals): every skill book is one vnum with the
	// skill in socket 0 (GetPlayerBotSkillBookSkillVnum) - a deal's piece is
	// the item of the vnum and, for a book, of the deal's skill.
	const DWORD PLAYERBOT_DEAL_SKILL_BOOK_VNUM = 50300;

	bool IsPlayerBotDealPiece(LPITEM item, DWORD vnum, DWORD skill)
	{
		if (!item || !item->GetProto())
			return false;
		if (skill)
			return item->GetType() == ITEM_SKILLBOOK && GetPlayerBotSkillBookSkillVnum(item) == skill;
		return item->GetVnum() == vnum;
	}

	// How a deal names its item: "KU Aura Miecza" for a book.
	std::string GetPlayerBotDealItemName(DWORD vnum, DWORD skill)
	{
		if (skill)
		{
			const char* name = GetPlayerBotSkillName(skill);
			if (name && strcmp(name, "?") != 0)
				return std::string("KU ") + name;
		}
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		return proto ? std::string(proto->szLocaleName) : std::string("to");
	}

	// The market's price of one piece: what was paid lately, the cheapest
	// counter, else what a keeper would ask for it. A book by its skill:
	// the sale memory's, a counter's line of that skill, else Iwakura's
	// sheet for the skill (PLAYERBOT_BOOK_PRICES).
	long long GetPlayerBotDealFairPrice(DWORD vnum, DWORD skill = 0)
	{
		size_t samples = 0;
		long long unit = GetPlayerBotSaleUnitPrice(vnum, 0, get_dword_time(), &samples, skill);
		if (unit > 0)
			return unit;
		TPlayerBotStall stall;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin(); it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER keeper = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!keeper || !keeper->GetMyShop() || !GetPlayerBotStall(it->first, keeper, stall))
				continue;
			for (size_t i = 0; i < stall.lines.size(); ++i)
			{
				if (stall.lines[i].vnum != vnum || stall.lines[i].price <= 0 ||
						(skill && (stall.lines[i].skill != skill || stall.lines[i].forget)))
					continue;
				const long long per = stall.lines[i].price / (stall.lines[i].count ? stall.lines[i].count : 1);
				if (unit == 0 || per < unit)
					unit = per;
			}
		}
		if (unit > 0)
			return unit;
		if (skill)
			return (long long)GetPlayerBotBookAskingBase(skill);
		LPITEM probe = ITEM_MANAGER::instance().CreateItem(vnum, 1, 0, false);
		if (!probe)
			return 0;
		unit = (long long)GetPlayerBotShopAskingPrice(probe);
		M2_DESTROY_ITEM(probe);
		return unit;
	}

	// The item a whisper names: the bot's bag, its counter, what it is short
	// of, then the whole item table - the exact name first, else the
	// shortest that matches (the base of a weapon rather than its +9).
	DWORD FindPlayerBotDealVnum(LPCHARACTER bot, const std::string& folded)
	{
		if (folded.empty())
			return 0;
		std::vector<std::string> candidates;
		playerbot_conv::ExpandItemQuery(folded, candidates);
		if (bot->IsItemLoaded())
		{
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = bot->GetInventoryItem(cell);
				if (item && item->GetProto() &&
						playerbot_conv::ItemNameMatchesAny(playerbot_conv::FoldName(item->GetProto()->szLocaleName), candidates))
					return item->GetVnum();
			}
		}
		std::set<DWORD> wanted;
		CollectPlayerBotWantedMaterials(bot, wanted);
		for (std::set<DWORD>::const_iterator it = wanted.begin(); it != wanted.end(); ++it)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(*it);
			if (proto && playerbot_conv::ItemNameMatchesAny(playerbot_conv::FoldName(proto->szLocaleName), candidates))
				return *it;
		}
		DWORD best = 0;
		size_t bestLen = 0;
		const std::vector<TItemTable>& protos = ITEM_MANAGER::instance().GetTable();
		for (size_t i = 0; i < protos.size(); ++i)
		{
			const TItemTable& t = protos[i];
			if (t.bType == ITEM_NONE || !*t.szLocaleName)
				continue;
			const std::string name = playerbot_conv::FoldName(t.szLocaleName);
			if (name == folded)
				return t.dwVnum;
			if (!playerbot_conv::ItemNameMatchesAny(name, candidates))
				continue;
			if (best == 0 || name.size() < bestLen)
			{
				best = t.dwVnum;
				bestLen = name.size();
			}
		}
		return best;
	}

	// The bot's own open "K>" post of this item, if any.
	const TPlayerBotPublicLine* FindPlayerBotOpenBuyPost(DWORD botPID, DWORD vnum, DWORD skill = 0)
	{
		std::map<DWORD, std::deque<TPlayerBotPublicLine> >::const_iterator it = s_mapPlayerBotPublicLines.find(botPID);
		if (it == s_mapPlayerBotPublicLines.end())
			return NULL;
		const DWORD now = get_dword_time();
		for (size_t i = 0; i < it->second.size(); ++i)
		{
			const TPlayerBotPublicLine& l = it->second[i];
			if (l.vnum == vnum && l.skill == skill && l.kind == playerbot_conv::PL_BUY && l.open &&
					now - l.at < PLAYERBOT_PUBLIC_LINE_TTL_MS)
				return &l;
		}
		return NULL;
	}

	bool IsPlayerBotDealSellable(LPITEM item)
	{
		return item && item->GetProto() && !item->IsEquipped() && !item->IsExchanging() && !item->isLocked() &&
				!IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE) && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_MYSHOP);
	}

	// The skill books in the bot's bag it would sell, "KU Aura Miecza, KU
	// Berserk", for "ktora?".
	std::string ListPlayerBotDealBooks(LPCHARACTER bot)
	{
		std::string out;
		std::set<DWORD> seen;
		if (!bot || !bot->IsItemLoaded())
			return out;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && seen.size() < 4; ++cell)
		{
			LPITEM item = bot->GetInventoryItem(cell);
			const DWORD skill = item && item->GetCell() == cell && IsPlayerBotDealSellable(item) ?
					GetPlayerBotSkillBookSkillVnum(item) : 0;
			if (!skill || !seen.insert(skill).second)
				continue;
			if (!out.empty())
				out += ", ";
			out += GetPlayerBotDealItemName(item->GetVnum(), skill);
		}
		return out;
	}

	bool QuotePlayerBotDealItem(LPCHARACTER bot, const std::string& query, DWORD vnumHint, DWORD skillHint,
			playerbot_conv::TDealQuote& out)
	{
		out = playerbot_conv::TDealQuote();
		if (!bot)
			return false;
		const std::string folded = playerbot_conv::FoldName(query.c_str());
		// MT2009_PLUS_BOT_CHAT_V2 (deals): a skill book by its skill - the
		// post's, or the query's "ku aura miecza" / "ksiega aura miecza" /
		// "instr. aura miecza". Matched by name, "ku aura miecza" found the
		// per-skill proto (50401..) that no bag holds, and a bare "ku" any
		// book at all: every book is 50300 with the skill in socket 0.
		DWORD skill = skillHint;
		if (!skill && !vnumHint && playerbot_conv::IsBareBookObject(folded))
		{
			out.needSkill = true;
			out.booksHad = ListPlayerBotDealBooks(bot);
			return false;
		}
		if (!skill)
		{
			std::string rest;
			bool forget = false;
			const DWORD named = GetPlayerBotStallBookQuery(folded, rest, forget);
			if (!forget)
				skill = named;
		}
		DWORD vnum = vnumHint;
		if (!skill && !vnumHint)
		{
			vnum = FindPlayerBotDealVnum(bot, folded);
			const TItemTable* named = vnum ? ITEM_MANAGER::instance().GetTable(vnum) : NULL;
			if (named && named->bType == ITEM_SKILLBOOK)
			{
				// A per-skill book's name ("Instr. Aura Miecza") is its skill;
				// the general one's ("Ksiega Umiejetnosci", "ku") is none.
				skill = vnum != PLAYERBOT_DEAL_SKILL_BOOK_VNUM ? (DWORD)named->alValues[0] : 0;
				if (!skill)
				{
					out.needSkill = true;
					out.booksHad = ListPlayerBotDealBooks(bot);
					return false;
				}
			}
		}
		if (skill)
			vnum = PLAYERBOT_DEAL_SKILL_BOOK_VNUM;
		const TItemTable* proto = vnum ? ITEM_MANAGER::instance().GetTable(vnum) : NULL;
		if (!proto)
			return false;
		TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(bot->GetPlayerID());
		out.found = true;
		out.vnum = vnum;
		out.skill = skill;
		out.name = skill ? GetPlayerBotDealItemName(vnum, skill) : std::string(proto->szLocaleName);
		out.stackable = IS_SET(proto->dwFlags, ITEM_FLAG_STACKABLE);
		out.fair = GetPlayerBotDealFairPrice(vnum, skill);
		out.botGold = (long long)bot->GetGold();

		// Would it buy it, and at what most.
		std::set<DWORD> wanted;
		CollectPlayerBotWantedMaterials(bot, wanted);
		const TPlayerBotPublicLine* post = FindPlayerBotOpenBuyPost(bot->GetPlayerID(), vnum, skill);
		// A book of its own path's skill, while it still reads them (Master,
		// 20..29): the one book a bot reliably buys off anybody.
		const bool bookNeeded = skill && IsPlayerBotOwnSkill(bot, skill) &&
				bot->GetSkillMasterType(skill) == SKILL_MASTER && bot->GetSkillLevel(skill) >= 20 &&
				bot->GetSkillLevel(skill) < 30;
		if (post)
		{
			out.botWants = true;
			out.wantCount = post->count > 0 ? post->count : (out.stackable ? 10 : 1);
			out.maxBuyUnit = std::max<long long>(post->unit, out.fair) * 110 / 100;
		}
		else if (bookNeeded)
		{
			out.botWants = out.fair > 0;
			out.wantCount = 1;
			out.maxBuyUnit = out.fair * 105 / 100;
		}
		else if (skill)
		{
			// Another skill's book: a merchant only, for its counter.
			if (st != s_mapPlayerBotAIStates.end() && MapPlayerBotConvStyle(st->second) == playerbot_conv::S_MERCHANT &&
					out.fair > 0)
			{
				out.botWants = true;
				out.wantCount = 1;
				out.maxBuyUnit = out.fair * 70 / 100;
			}
		}
		else if (wanted.count(vnum) || (GetPlayerBotRefineMaterialVnums().count(vnum) && PlayerBotNeedsRefineMaterial(bot, vnum)))
		{
			out.botWants = out.fair > 0;
			out.wantCount = out.stackable ? 10 : 1;
			out.maxBuyUnit = out.fair * 105 / 100;
		}
		else
		{
			std::map<DWORD, TPlayerBotWeaponGoal>::const_iterator goal = s_mapPlayerBotWeaponGoals.find(bot->GetPlayerID());
			if (goal != s_mapPlayerBotWeaponGoals.end() && goal->second.family &&
					proto->bType == ITEM_WEAPON && vnum - (vnum % 10) == goal->second.family->dwBaseVnum)
			{
				const long long goalPrice = (long long)GetPlayerBotWeaponGoalPrice(goal->second.family);
				out.botWants = goalPrice > 0;
				out.wantCount = 1;
				out.maxBuyUnit = std::max(goalPrice, out.fair) * 110 / 100;
			}
			else if (st != s_mapPlayerBotAIStates.end() && MapPlayerBotConvStyle(st->second) == playerbot_conv::S_MERCHANT &&
					out.fair > 0)
			{
				// A merchant buys anything worth something, for its counter.
				out.botWants = true;
				out.wantCount = out.stackable ? 20 : 1;
				out.maxBuyUnit = out.fair * 70 / 100;
			}
		}

		// Would it sell it, and at what least: its bag, never what it wears,
		// what it is short of, or what its counter holds.
		if (!wanted.count(vnum) && !bookNeeded && out.fair > 0 && bot->IsItemLoaded())
		{
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = bot->GetInventoryItem(cell);
				if (item && IsPlayerBotDealPiece(item, vnum, skill) && item->GetCell() == cell && IsPlayerBotDealSellable(item))
					out.botHas += (int)item->GetCount();
			}
			out.minSellUnit = out.fair * 85 / 100;
			out.sellUnit = out.fair * 110 / 100;
		}
		TPlayerBotStall stall;
		if (GetPlayerBotStall(bot->GetPlayerID(), bot, stall))
		{
			for (size_t i = 0; i < stall.lines.size(); ++i)
			{
				if (stall.lines[i].vnum != vnum || (skill && (stall.lines[i].skill != skill || stall.lines[i].forget)))
					continue;
				out.onStall = true;
				out.stallUnit = stall.lines[i].price / (stall.lines[i].count ? stall.lines[i].count : 1);
				out.stallWhere = GetPlayerBotTownName(stall.mapIndex >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN
						? stall.mapIndex / 10000 : stall.mapIndex);
				break;
			}
		}
		return true;
	}

	// --------------------------------------------------------- the meeting

	// The bot's own place beside a village's blacksmith (20016, the town
	// services' table): each bot a side of its own, so two waiting there do
	// not stand in one another.
	bool GetPlayerBotDealSmithSpot(long mapIndex, DWORD botPID, long& x, long& y)
	{
		playerbot_empire_rules::TTownServices services;
		if (!playerbot_empire_rules::GetTownServices(mapIndex, services))
			return false;
		static const int kSide[8][2] = {
			{ 200, 0 }, { 141, 141 }, { 0, 200 }, { -141, 141 }, { -200, 0 }, { -141, -141 }, { 0, -200 }, { 141, -141 } };
		const int* side = kSide[botPID % 8];
		x = services.blacksmith.x + side[0] * PLAYERBOT_DEAL_MEET_SPOT_OFFSET / 200;
		y = services.blacksmith.y + side[1] * PLAYERBOT_DEAL_MEET_SPOT_OFFSET / 200;
		return true;
	}

	bool IsPlayerBotDealVillage(long mapIndex)
	{
		playerbot_empire_rules::TTownServices services;
		return mapIndex > 0 && *playerbot_conv::VillageTag(mapIndex) && map_allow_find(mapIndex) &&
				playerbot_empire_rules::GetTownServices(mapIndex, services);
	}

	// The village of a meeting: the one the person stands in (on either
	// channel - the same village on the other is a channel switch away), the
	// bot's own when it already stands in one of the person's kingdom, the
	// person's kingdom's M1 (a person goes into another kingdom's village
	// only with trouble), then the bot's village or its own M1. Only a
	// village this core hosts; 0 none.
	long PickPlayerBotDealVillage(LPCHARACTER bot, int personEmpire, long personMap)
	{
		using namespace playerbot_empire_rules;
		const long botMap = bot->GetMapIndex();
		if (IsPlayerBotDealVillage(personMap))
			return personMap;
		if (IsPlayerBotDealVillage(botMap) && GetMapOwnerEmpire(botMap) == personEmpire)
			return botMap;
		const long personM1 = GetHomeMap(personEmpire, MAP_ROLE_M1);
		if (IsPlayerBotDealVillage(personM1))
			return personM1;
		if (IsPlayerBotDealVillage(botMap))
			return botMap;
		const long botM1 = GetHomeMap((int)bot->GetEmpire(), MAP_ROLE_M1);
		return IsPlayerBotDealVillage(botM1) ? botM1 : 0;
	}

	// A person on another map of this core the bot may simply go over to: a
	// plain hunting map the bot can stand (not a village - the smith there
	// is the place - nor the tower, an instance or a place no bot is taken).
	bool CanPlayerBotDealCrossTo(LPCHARACTER bot, LPCHARACTER person)
	{
		const long map = person->GetMapIndex();
		return map > 0 && map < PLAYERBOT_INSTANCE_MAP_INDEX_MIN && map != bot->GetMapIndex() &&
				playerbot_conv::IsKnownMap(map) && !playerbot_conv::GetMapWords(map).town &&
				!IsPlayerBotDemonTowerInstance(map) && !IsPlayerBotOffLimitsMap(map) && map_allow_find(map) &&
				(int)bot->GetLevel() + PLAYERBOT_DEAL_CROSS_LEVEL_SLACK >= (int)person->GetLevel() &&
				!person->IsWarping() && person->GetSectree();
	}

	void FillPlayerBotDealMeetPlace(const TPlayerBotDeal& d, LPCHARACTER bot, playerbot_conv::TDealMeetPlace& out)
	{
		out = playerbot_conv::TDealMeetPlace();
		out.kind = d.meet;
		out.map = d.meetMap ? d.meetMap : bot->GetMapIndex();
		out.spot = d.spot;
		out.channel = g_bChannel;
		out.otherChannel = d.otherChannel;
		out.arrived = d.arrivedAt != 0;
	}

	int RegisterPlayerBotDeal(LPCHARACTER bot, DWORD personPID, LPCHARACTER person, BYTE side, DWORD vnum, DWORD skill,
			int count, long long unit, playerbot_conv::TDealMeetPlace& place)
	{
		using namespace playerbot_conv;
		place = TDealMeetPlace();
		if (!bot || !personPID || count <= 0 || unit <= 0)
			return DEAL_MEET_FAILED;
		if (side == DEAL_BOT_BUYS && (long long)bot->GetGold() < unit * count)
			return DEAL_MEET_FAILED;
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(bot->GetPlayerID());
		if (st == s_mapPlayerBotAIStates.end())
			return DEAL_MEET_FAILED;
		const DWORD now = get_dword_time();
		TPlayerBotDeal& d = s_mapPlayerBotDeals[std::make_pair(bot->GetPlayerID(), personPID)];
		d = TPlayerBotDeal();
		d.side = side;
		d.vnum = vnum;
		d.skill = skill;
		d.count = count;
		d.unit = unit;
		d.at = now;
		// The person: a character of this core, or the P2P table's line of
		// one on another core - its channel, kingdom and map.
		const CCI* peer = person ? NULL : P2P_MANAGER::instance().FindByPID(personPID);
		const int personChannel = person ? (int)g_bChannel : peer ? (int)peer->bChannel : 0;
		const int personEmpire = person ? (int)person->GetEmpire() : peer ? (int)peer->bEmpire : (int)bot->GetEmpire();
		const long personMap = person ? person->GetMapIndex() : peer ? peer->lMapIndex : 0;
		d.otherChannel = personChannel > 0 && personChannel != (int)g_bChannel;

		// Near, or on the same map: the window now, or the summon's walk over.
		if (person && person->GetMapIndex() == bot->GetMapIndex())
		{
			if (DISTANCE_APPROX(person->GetX() - bot->GetX(), person->GetY() - bot->GetY()) <= PLAYERBOT_DEAL_NEAR)
				d.meet = DEAL_MEET_NEAR;
			else
			{
				const int code = StartPlayerBotSummon(bot, person, now);
				if (code == SUMMON_START_OK || code == SUMMON_START_RENEWED)
					d.meet = DEAL_MEET_COMING;
			}
		}
		if (d.meet == DEAL_MEET_FAILED)
		{
			// What keeps it where it stands (GetPlayerBotSummonBlock, any map;
			// the bot itself stands in for a person of another core).
			const int block = GetPlayerBotSummonBlock(bot, st->second, person ? person : bot, now, true);
			if (block == SB_STALL && IsPlayerBotDealVillage(bot->GetMapIndex()))
			{
				// Behind its counter it waits for them there.
				d.meet = DEAL_MEET_AT_SPOT;
				d.meetMap = bot->GetMapIndex();
				d.spot = DEAL_SPOT_STALL;
				d.spotX = bot->GetX();
				d.spotY = bot->GetY();
				d.arrivedAt = now;
			}
			else if (block != SB_NONE)
				d.meet = DEAL_MEET_COME_TO_ME;
			else if (person && CanPlayerBotDealCrossTo(bot, person))
			{
				d.meet = DEAL_MEET_COMING;
				d.crossToPerson = true;
				d.leaveAt = now + number(PLAYERBOT_DEAL_MEET_LEAVE_MIN_MS, PLAYERBOT_DEAL_MEET_LEAVE_MAX_MS);
			}
			else
			{
				const long village = PickPlayerBotDealVillage(bot, personEmpire, personMap);
				long x = 0, y = 0;
				if (village && GetPlayerBotDealSmithSpot(village, bot->GetPlayerID(), x, y))
				{
					d.meet = DEAL_MEET_AT_SPOT;
					d.meetMap = village;
					d.spot = DEAL_SPOT_SMITH;
					d.spotX = x;
					d.spotY = y;
					if (bot->GetMapIndex() == village &&
							DISTANCE_APPROX(bot->GetX() - x, bot->GetY() - y) <= PLAYERBOT_DEAL_MEET_STAY)
						d.arrivedAt = now;
					else
						d.leaveAt = now + number(PLAYERBOT_DEAL_MEET_LEAVE_MIN_MS, PLAYERBOT_DEAL_MEET_LEAVE_MAX_MS);
				}
				else
					d.meet = DEAL_MEET_COME_TO_ME;
			}
		}
		FillPlayerBotDealMeetPlace(d, bot, place);
		sys_log(0, "PLAYERBOT_DEAL: agreed pid=%u name=%s person_pid=%u side=%s vnum=%u skill=%u count=%d unit=%lld meet=%d "
				"map=%ld spot=%d channel=%d person_channel=%d cross=%d",
				bot->GetPlayerID(), bot->GetName(), personPID, side == DEAL_BOT_BUYS ? "buys" : "sells", vnum, skill, count, unit,
				d.meet, place.map, (int)d.spot, (int)g_bChannel, personChannel, d.crossToPerson ? 1 : 0);
		return d.meet;
	}

	// The meeting as it stands, for "gdzie jestes?".
	bool GetPlayerBotDealMeet(LPCHARACTER bot, DWORD personPID, playerbot_conv::TDealMeetPlace& place)
	{
		if (!bot)
			return false;
		std::map<TPlayerBotDealKey, TPlayerBotDeal>::const_iterator it =
				s_mapPlayerBotDeals.find(std::make_pair(bot->GetPlayerID(), personPID));
		if (it == s_mapPlayerBotDeals.end())
			return false;
		FillPlayerBotDealMeetPlace(it->second, bot, place);
		return true;
	}

	// On its way to, or waiting at, a deal's meeting: held for company, as a
	// summoned bot is (IsPlayerBotHeldForCompany) - the market, the world
	// travel and the events leave it alone.
	bool IsPlayerBotDealMeeting(DWORD botPID)
	{
		std::map<TPlayerBotDealKey, TPlayerBotDeal>::const_iterator it =
				s_mapPlayerBotDeals.lower_bound(std::make_pair(botPID, (DWORD)0));
		for (; it != s_mapPlayerBotDeals.end() && it->first.first == botPID; ++it)
			if ((it->second.meet == playerbot_conv::DEAL_MEET_AT_SPOT && it->second.spot == playerbot_conv::DEAL_SPOT_SMITH) ||
					it->second.crossToPerson)
				return true;
		return false;
	}

	void EndPlayerBotDeal(LPCHARACTER bot, DWORD personPID, unsigned char state, const char* why)
	{
		std::map<TPlayerBotDealKey, TPlayerBotDeal>::iterator it =
				s_mapPlayerBotDeals.find(std::make_pair(bot->GetPlayerID(), personPID));
		if (it != s_mapPlayerBotDeals.end())
		{
			sys_log(0, "PLAYERBOT_DEAL: %s pid=%u name=%s person_pid=%u vnum=%u skill=%u count=%d unit=%lld", why,
					bot->GetPlayerID(), bot->GetName(), personPID, it->second.vnum, it->second.skill, it->second.count,
					it->second.unit);
			if (state == playerbot_conv::DEAL_DONE)
				ClosePlayerBotPublicPost(bot->GetPlayerID(), it->second.vnum, it->second.skill);
			s_mapPlayerBotDeals.erase(it);
		}
		if (playerbot_conv::TConvPair* pair = s_PlayerBotConvEngine.FindPair(personPID, bot->GetPlayerID()))
			pair->mem.deal.state = state;
	}

	// --------------------------------------------------------- the window

	// A line of the deal to the person by pid - a character of this core or
	// of another (the conversation's queue answers either, by the relay).
	void SayPlayerBotDealLineTo(LPCHARACTER bot, DWORD personPID, const char* text)
	{
		playerbot_conv::TBotSnapshot snap;
		if (!bot || !personPID || !s_PlayerBotConvHost.BuildSnapshot(personPID, bot->GetPlayerID(), snap))
		{
			LPCHARACTER person = bot ? CHARACTER_MANAGER::instance().FindByPID(personPID) : NULL;
			if (person)
				SendPlayerBotWhisper(bot, person, text);
			return;
		}
		s_PlayerBotConvEngine.QueueBotLine(s_PlayerBotConvHost, personPID, bot->GetPlayerID(), text,
				std::string(), playerbot_conv::I_DEAL, snap, get_dword_time(), 200);
		EnsurePlayerBotConvTimer();
	}

	void SayPlayerBotDealLine(LPCHARACTER bot, LPCHARACTER person, const char* text)
	{
		if (bot && person)
			SayPlayerBotDealLineTo(bot, person->GetPlayerID(), text);
	}

	// One of a few ways to say a line with the deal's numbers in it.
	void SayPlayerBotDealVariant(LPCHARACTER bot, LPCHARACTER person, const char* const* variants, int n, ...)
	{
		char line[CHAT_MAX_LEN + 1];
		va_list args;
		va_start(args, n);
		vsnprintf(line, sizeof(line), variants[number(0, n - 1)], args);
		va_end(args);
		SayPlayerBotDealLine(bot, person, line);
	}

	// The bot's pieces of the deal into its side of the window, a stack split
	// to the count when it has to be. False when it has not got them.
	// A book only of the deal's skill (socket 0): every book is one vnum,
	// and "KU Aura Miecza" agreed must not hand over a KU Berserk.
	bool OfferPlayerBotDealPieces(LPCHARACTER ch, CExchange* exchange, DWORD vnum, DWORD skill, int count)
	{
		std::vector<LPITEM> chosen;
		int pieces = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS && pieces < count; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || !IsPlayerBotDealPiece(item, vnum, skill) || item->GetCell() != cell || !IsPlayerBotDealSellable(item))
				continue;
			const int left = count - pieces;
			if ((int)item->GetCount() > left)
			{
				// Split: the rest stays, the deal's part goes to a cell of its own
				// - a book's with its skill.
				LPITEM part = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), left, 0, false);
				if (!part)
					return false;
				if (skill)
					part->SetSocket(0, item->GetSocket(0));
				const int free = ch->GetEmptyInventory(part->GetSize());
				if (free < 0)
				{
					M2_DESTROY_ITEM(part);
					return false;
				}
				item->SetCount(item->GetCount() - left);
				part->AddToCharacter(ch, TItemPos(INVENTORY, (WORD)free));
				chosen.push_back(part);
				pieces += left;
				break;
			}
			chosen.push_back(item);
			pieces += (int)item->GetCount();
		}
		if (pieces != count)
			return false;
		BYTE display = 0;
		for (size_t i = 0; i < chosen.size(); ++i)
		{
			bool added = false;
			for (; display < EXCHANGE_ITEM_MAX_NUM && !added; ++display)
				added = exchange->AddItem(TItemPos(INVENTORY, chosen[i]->GetCell()), display);
			if (!added)
				return false;
		}
		return true;
	}

	// The way to a deal's meeting and the wait there, the bot's tick while
	// no window is open. A meeting at the smith: a moment, then the village
	// (TransitionPlayerBotMap onto its square, as every trip of a bot ends),
	// the walk to the smith, and PLAYERBOT_DEAL_MEET_WAIT_MS standing there,
	// off the horse; arrived from elsewhere, it says so. A person on a
	// hunting map of this core: the bot goes over and the summon's walk
	// takes it the rest of the way. True while it claims the tick.
	bool ManagePlayerBotDealMeeting(LPCHARACTER ch, TPlayerBotAIState& state, DWORD personPID, DWORD dwNow)
	{
		using namespace playerbot_conv;
		std::map<TPlayerBotDealKey, TPlayerBotDeal>::iterator it =
				s_mapPlayerBotDeals.find(std::make_pair(ch->GetPlayerID(), personPID));
		if (it == s_mapPlayerBotDeals.end() || ch->IsDead())
			return false;
		TPlayerBotDeal& d = it->second;
		const char* village = VillageTag(d.meetMap);
		char line[CHAT_MAX_LEN + 1];

		// Over to the person's map, then their summon.
		if (d.crossToPerson)
		{
			if (dwNow < d.leaveAt || dwNow < d.nextTryAt)
			{
				ch->SetVictim(NULL);
				if (ch->IsStateMove())
					ch->Stop();
				SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
				state.dwLastMeaningfulActivityTime = dwNow;
				return true;
			}
			LPCHARACTER person = CHARACTER_MANAGER::instance().FindByPID(personPID);
			if (!person || !person->GetDesc() || !CanPlayerBotDealCrossTo(ch, person) || ++d.tries > PLAYERBOT_DEAL_MEET_TRIES)
			{
				d.crossToPerson = false;
				if (person && person->GetDesc() && person->GetMapIndex() == ch->GetMapIndex())
				{
					// The person came onto the bot's map meanwhile: the walk
					// over, and the near check opens the window.
					StartPlayerBotSummon(ch, person, dwNow);
					return false;
				}
				SayPlayerBotDealLineTo(ch, personPID, "Nie moge cie znalezc. Napisz gdzie jestes, to sie umowimy jeszcze raz.");
				EndPlayerBotDeal(ch, personPID, DEAL_FAILED, "cross_failed");
				return false;
			}
			d.nextTryAt = dwNow + PLAYERBOT_DEAL_MEET_RETRY_MS;
			static const int kSide[4][2] = { { 300, 0 }, { 0, 300 }, { -300, 0 }, { 0, -300 } };
			const int* side = kSide[ch->GetPlayerID() % 4];
			if (!TransitionPlayerBotMap(ch, state, person->GetMapIndex(), person->GetX() + side[0], person->GetY() + side[1],
					dwNow, "deal_meet"))
				return true;
			d.crossToPerson = false;
			const int code = StartPlayerBotSummon(ch, person, dwNow);
			sys_log(0, "PLAYERBOT_DEAL: crossed pid=%u name=%s person=%s map=%ld summon=%d", ch->GetPlayerID(), ch->GetName(),
					person->GetName(), person->GetMapIndex(), code);
			return true;
		}
		if (d.meet != DEAL_MEET_AT_SPOT || d.spot != DEAL_SPOT_SMITH)
			return false;

		// The wait is over.
		if (d.arrivedAt != 0 && dwNow - d.arrivedAt > PLAYERBOT_DEAL_MEET_WAIT_MS)
		{
			static const char* const kGone[] = {
				"Nie doczekalem sie, wracam do swoich spraw. Jak bedziesz chcial handlowac, napisz.",
				"Stalem przy kowalu %d minut i nic. Ide dalej, napisz jak bedziesz mial czas.",
				"Dluzej nie czekam, sorki. Odezwij sie jak bedziesz gotowy." };
			snprintf(line, sizeof(line), kGone[number(0, 2)], (int)(PLAYERBOT_DEAL_MEET_WAIT_MS / 60000));
			SayPlayerBotDealLineTo(ch, personPID, line);
			EndPlayerBotDeal(ch, personPID, DEAL_FAILED, "meet_timeout");
			return false;
		}
		state.dwLastMeaningfulActivityTime = dwNow;
		state.lLastX = ch->GetX();
		state.lLastY = ch->GetY();
		ch->SetVictim(NULL);
		state.dwTargetVID = 0;

		// A moment before it sets off.
		if (d.arrivedAt == 0 && dwNow < d.leaveAt)
		{
			if (ch->IsStateMove())
				ch->Stop();
			SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
			return true;
		}

		// Into the village.
		if (ch->GetMapIndex() != d.meetMap)
		{
			if (d.walkSince != 0 || d.arrivedAt != 0)
			{
				// Taken off the village by something with the better claim.
				SayPlayerBotDealLineTo(ch, personPID, "Musialem odejsc, sorki. Napisz jak bedziesz chcial jeszcze handlowac.");
				EndPlayerBotDeal(ch, personPID, DEAL_FAILED, "meet_moved_away");
				return false;
			}
			if (dwNow < d.nextTryAt)
				return true;
			d.nextTryAt = dwNow + PLAYERBOT_DEAL_MEET_RETRY_MS;
			if (++d.tries > PLAYERBOT_DEAL_MEET_TRIES)
			{
				SayPlayerBotDealLineTo(ch, personPID, "Cos mnie zatrzymalo, nie dojde teraz. Sorki, sprobujmy pozniej.");
				EndPlayerBotDeal(ch, personPID, DEAL_FAILED, "meet_unreachable");
				return false;
			}
			playerbot_empire_rules::TPoint square;
			if (!playerbot_empire_rules::GetTownPitch(d.meetMap, square))
			{
				square.x = d.spotX;
				square.y = d.spotY;
			}
			if (TransitionPlayerBotMap(ch, state, d.meetMap, square.x, square.y, dwNow, "deal_meet"))
			{
				d.travelled = true;
				d.walkSince = dwNow;
				sys_log(0, "PLAYERBOT_DEAL: to the meeting pid=%u name=%s map=%ld", ch->GetPlayerID(), ch->GetName(), d.meetMap);
			}
			return true;
		}

		// To the smith.
		const int distance = DISTANCE_APPROX(ch->GetX() - d.spotX, ch->GetY() - d.spotY);
		if (d.arrivedAt == 0)
		{
			if (d.walkSince == 0)
				d.walkSince = dwNow;
			if (distance > PLAYERBOT_DEAL_MEET_STAY && dwNow - d.walkSince < PLAYERBOT_DEAL_MEET_WALK_MAX_MS)
			{
				d.travelled = true;
				MovePlayerBot(ch, d.spotX, d.spotY, dwNow, 6, true, true, false, false);
				SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
				return true;
			}
			// There - or as near as the walk got in its time, which is in
			// sight of the smith all the same.
			d.arrivedAt = dwNow;
			sys_log(0, "PLAYERBOT_DEAL: at the meeting pid=%u name=%s map=%ld distance=%d walk_s=%u", ch->GetPlayerID(),
					ch->GetName(), d.meetMap, distance, (dwNow - d.walkSince) / 1000);
			if (d.travelled)
			{
				if (d.otherChannel)
				{
					static const char* const kThere[] = { "Jestem juz przy kowalu w %s na CH%d, czekam.",
						"Stoje przy kowalu w %s (CH%d). Czekam na ciebie." };
					snprintf(line, sizeof(line), kThere[number(0, 1)], village, (int)g_bChannel);
				}
				else
				{
					static const char* const kThere[] = { "Jestem juz przy kowalu w %s, czekam.", "Stoje przy kowalu w %s, czekam na ciebie.",
						"Doszedlem, jestem przy kowalu w %s." };
					snprintf(line, sizeof(line), kThere[number(0, 2)], village);
				}
				SayPlayerBotDealLineTo(ch, personPID, line);
			}
		}

		// Waiting: back to its place when something pushed it off, else
		// standing there off the horse.
		if (distance > PLAYERBOT_DEAL_MEET_STAY * 3)
		{
			MovePlayerBot(ch, d.spotX, d.spotY, dwNow, 6, true, true, false, false);
			SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
			return true;
		}
		if (!state.vecRoute.empty())
			ClearPlayerBotRoute(state, true);
		if (ch->IsStateMove())
			ch->Stop();
		if (ch->IsRiding())
			SetPlayerBotRidingForTravel(ch, state, false, dwNow, "deal_wait");
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		return true;
	}

	// true while a deal holds the bot in an exchange (or it just opened one).
	bool HandlePlayerBotDealTrade(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		using namespace playerbot_conv;
		if (!ch || s_mapPlayerBotDeals.empty())
			return false;
		const DWORD pid = ch->GetPlayerID();
		CExchange* exchange = ch->GetExchange();
		if (!exchange)
		{
			// The bot's deals: the stale ones go; a person standing near gets
			// the window opened.
			DWORD meetingWith = 0;
			std::map<TPlayerBotDealKey, TPlayerBotDeal>::iterator it = s_mapPlayerBotDeals.lower_bound(std::make_pair(pid, (DWORD)0));
			while (it != s_mapPlayerBotDeals.end() && it->first.first == pid)
			{
				TPlayerBotDeal& d = it->second;
				const DWORD personPID = it->first.second;
				if (dwNow - d.at > PLAYERBOT_DEAL_TTL_MS)
				{
					++it;
					EndPlayerBotDeal(ch, personPID, DEAL_FAILED, "expired");
					return false;
				}
				if (meetingWith == 0 && ((d.meet == DEAL_MEET_AT_SPOT && d.spot == DEAL_SPOT_SMITH) || d.crossToPerson))
					meetingWith = personPID;
				LPCHARACTER person = CHARACTER_MANAGER::instance().FindByPID(personPID);
				if (person && person->GetDesc() && person->GetMapIndex() == ch->GetMapIndex() && !person->GetExchange() &&
						!person->IsDead() && dwNow >= d.nextOpenAt &&
						DISTANCE_APPROX(person->GetX() - ch->GetX(), person->GetY() - ch->GetY()) <= PLAYERBOT_DEAL_NEAR)
				{
					d.nextOpenAt = dwNow + PLAYERBOT_DEAL_OPEN_RETRY_MS;
					if (ch->IsStateMove())
						ch->Stop();
					if (ch->ExchangeStart(person))
					{
						sys_log(0, "PLAYERBOT_DEAL: window opened pid=%u name=%s person=%s", pid, ch->GetName(), person->GetName());
						return true;
					}
				}
				++it;
			}
			return meetingWith != 0 && ManagePlayerBotDealMeeting(ch, state, meetingWith, dwNow);
		}
		CExchange* other = exchange->GetCompany();
		LPCHARACTER partner = other ? other->GetOwner() : NULL;
		if (!partner)
			return false;
		std::map<TPlayerBotDealKey, TPlayerBotDeal>::iterator found =
				s_mapPlayerBotDeals.find(std::make_pair(pid, partner->GetPlayerID()));
		if (found == s_mapPlayerBotDeals.end())
			return false; // not a deal: the gift trade's
		TPlayerBotDeal& d = found->second;
		const std::string dealName = GetPlayerBotDealItemName(d.vnum, d.skill);
		const char* name = dealName.c_str();

		// Standing still at the window, as the gift trade does.
		ch->SetVictim(NULL);
		if (ch->IsStateMove())
			ch->Stop();
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		state.dwLastMeaningfulActivityTime = dwNow;
		if (d.windowSince == 0)
			d.windowSince = dwNow;
		if (dwNow - d.windowSince > PLAYERBOT_DEAL_WINDOW_MAX_MS)
		{
			exchange->Cancel();
			d.windowSince = 0;
			d.paid = false;
			d.offered = false;
			SayPlayerBotDealLine(ch, partner, "Za dlugo to trwa, zamykam. Daj wymiane jeszcze raz jak bedziesz gotowy.");
			return true;
		}

		std::vector<LPITEM> offer;
		CollectPlayerBotGiftOffer(partner, offer);
		if (d.side == DEAL_BOT_BUYS)
		{
			int pieces = 0, others = 0;
			// A book of another skill is not the deal's piece: it is told to
			// go as anything else is ("Daj tylko KU Aura Miecza").
			for (size_t i = 0; i < offer.size(); ++i)
			{
				if (IsPlayerBotDealPiece(offer[i], d.vnum, d.skill))
					pieces += (int)offer[i]->GetCount();
				else
					++others;
			}
			if (pieces != d.lastPieces)
			{
				d.lastPieces = pieces;
				d.shortSince = 0;
			}
			// Paid for what lay there: a change since is a change after the
			// yang went in, and only then is it said so.
			if (d.paid && pieces != d.paidPieces)
			{
				exchange->Cancel();
				d.paid = false;
				d.windowSince = 0;
				d.shortSince = 0;
				static const char* const kChanged[] = {
					"Zmieniles ilosc po tym jak dalem yang (bylo %d szt, jest %d). Daj wymiane jeszcze raz.",
					"Po moim yang zmienila sie ilosc - %d szt, teraz %d. Zamykam, daj wymiane od nowa." };
				SayPlayerBotDealVariant(ch, partner, kChanged, 2, d.paidPieces, pieces);
				return true;
			}
			if (others > 0)
			{
				if (dwNow - d.warnedAt > 8000)
				{
					d.warnedAt = dwNow;
					static const char* const kOthers[] = { "Daj tylko %s, reszte zabierz.", "Kupuje tylko %s - inne rzeczy zabierz z okna." };
					SayPlayerBotDealVariant(ch, partner, kOthers, 2, name);
				}
				return true;
			}
			if (!d.paid)
			{
				if (pieces > d.count)
				{
					if (dwNow - d.warnedAt > 8000)
					{
						d.warnedAt = dwNow;
						static const char* const kMore[] = {
							"Umawialismy sie na %d szt, a dales %d - daj dokladnie %d.",
							"Mialo byc %d szt, w oknie jest %d. Zostaw dokladnie %d, reszte zabierz.",
							"Biore %d szt, nie %d. Podziel stack i daj %d." };
						SayPlayerBotDealVariant(ch, partner, kMore, 3, d.count, pieces, d.count);
					}
					return true;
				}
				if (pieces == 0)
					return true;
				if (pieces < d.count)
				{
					// Maybe more is coming. Accepted with fewer: told once; kept
					// accepted after that, the bot pays for what lies there.
					if (!other->GetAcceptStatus())
						return true;
					if (d.shortSince == 0)
					{
						d.shortSince = dwNow;
						static const char* const kFewer[] = {
							"Umawialismy sie na %d szt, a dales %d - doloz jeszcze %d. Jak wiecej nie masz, zostaw tak, zaplace za %d.",
							"Mialo byc %d szt, dales %d. Doloz %d albo zostaw zaakceptowane, to wezme %d." };
						SayPlayerBotDealVariant(ch, partner, kFewer, 2, d.count, pieces, d.count - pieces, pieces);
						return true;
					}
					if (dwNow - d.shortSince < PLAYERBOT_DEAL_SHORT_ACCEPT_MS)
						return true;
				}
				const long long pay = d.unit * pieces;
				if ((long long)ch->GetGold() < pay || !exchange->AddGold(pay))
				{
					exchange->Cancel();
					SayPlayerBotDealLine(ch, partner, "Sorki, nie mam teraz tyle yang.");
					EndPlayerBotDeal(ch, partner->GetPlayerID(), DEAL_FAILED, "no_gold");
					return true;
				}
				d.paid = true;
				d.paidPieces = pieces;
				d.decideAt = 0;
				return true;
			}
		}
		else
		{
			if (!d.offered)
			{
				if (!OfferPlayerBotDealPieces(ch, exchange, d.vnum, d.skill, d.count))
				{
					exchange->Cancel();
					SayPlayerBotDealLine(ch, partner, "Ups, juz tego nie mam. Sorki.");
					EndPlayerBotDeal(ch, partner->GetPlayerID(), DEAL_FAILED, "no_pieces");
					return true;
				}
				d.offered = true;
				d.decideAt = 0;
				return true;
			}
			const long long due = d.unit * d.count;
			if (other->GetAcceptStatus() && other->Mt2009PlusGetGold() < due && dwNow - d.warnedAt > 8000)
			{
				d.warnedAt = dwNow;
				char line[CHAT_MAX_LEN + 1];
				snprintf(line, sizeof(line), "Mialo byc %s yang, doloz brakujace.", playerbot_conv::FormatYang(due).c_str());
				SayPlayerBotDealLine(ch, partner, line);
			}
			if (other->Mt2009PlusGetGold() > due && dwNow - d.warnedAt > 8000)
			{
				// More than agreed is not taken either.
				d.warnedAt = dwNow;
				char line[CHAT_MAX_LEN + 1];
				snprintf(line, sizeof(line), "Dales za duzo yang - mialo byc %s. Popraw kwote.",
						playerbot_conv::FormatYang(due).c_str());
				SayPlayerBotDealLine(ch, partner, line);
			}
			if (other->Mt2009PlusGetGold() != due || !offer.empty())
			{
				// Yang only from the person's side: an item in it is not the deal.
				if (!offer.empty() && dwNow - d.warnedAt > 8000)
				{
					d.warnedAt = dwNow;
					SayPlayerBotDealLine(ch, partner, "Daj tylko yang, przedmioty zabierz.");
				}
				return true;
			}
		}

		// The person's accept first, then a glance, then the bot's.
		if (!other->GetAcceptStatus() || exchange->GetAcceptStatus())
		{
			d.decideAt = 0;
			return true;
		}
		if (d.decideAt == 0)
		{
			d.decideAt = dwNow + number(PLAYERBOT_DEAL_GLANCE_MIN_MS, PLAYERBOT_DEAL_GLANCE_MAX_MS);
			return true;
		}
		if (dwNow < d.decideAt)
			return true;
		const long long goldBefore = (long long)ch->GetGold();
		const DWORD personPID = partner->GetPlayerID();
		exchange->Accept(true);
		if (ch->GetExchange())
			return true; // not done yet
		const long long goldNow = (long long)ch->GetGold();
		const bool done = d.side == DEAL_BOT_BUYS ? goldNow < goldBefore : goldNow > goldBefore;
		static const char* const kThanks[] = { "Dzieki, dobry handel!", "Dzieki, polecam sie na przyszlosc :)", "Gitara, dzieki!",
			"Dzieki, milo sie handlowalo." };
		if (done)
		{
			SayPlayerBotDealLine(ch, partner, kThanks[number(0, 3)]);
			state.dwNextEquipmentCheckTime = 0;
		}
		EndPlayerBotDeal(ch, personPID, done ? DEAL_DONE : DEAL_FAILED, done ? "done" : "window_closed");
		return true;
	}
}

#endif
