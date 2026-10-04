#ifndef __INC_METIN2_PLAYERBOT_MINIGAMES_H__
#define __INC_METIN2_PLAYERBOT_MINIGAMES_H__

// MT2009_PLUS_BOT_MINIGAMES_V1 (the owner, 2 October): the bots take part in
// the three mini game events - Catch the King, Rumi (Okey) and Yut Nori - the
// simulated way. No bot walks to a table, sends a packet or ranks; it is all
// in here.
//
//  - The drop. While an event runs a bot's kill rolls the event's card with
//    the players' own chance (the kill hooks CatchKingOnKill, RumiOnKill and
//    YutnoriKillRoll call PlayerBotMinigameCard for a bot). A player's cards
//    are counters (quest flags) that make a counter of decks; a bot's cards
//    are a counter too (mt2009_botmg.*_cards, per event season like the
//    players'), but what they make is the REAL item: 25 King Cards a Talia
//    Krolewska (79604), 24 Okey cards a Zestaw kart Okey (79506), 28 birch
//    trunks a Plansza do Yutnori (79508) - the counts the games use
//    (PIECE_COUNT_MAX, CARD_PIECE_MAX, PIECE_COUNT_MAX). A bot holds at most
//    PLAYERBOT_MG_TOKEN_HOLD of one; past that its cards stop, as a player's
//    stop at 999 decks.
//  - The game. A deck, a set or a board in the bag is "played" by the next
//    pass while its event runs: the token and the players' 30 000 yang go,
//    and ONE chest comes by a roll - bronze 60%, silver 30%, gold 10%:
//      Catch the King  50970 / 50969 / 50968 (the Lupy Krolewskie)
//      Rumi            50277 / 50276 / 50275, or the Christmas 50269 / 50268 /
//                      50267 while only the GM's "rumi_xmas" flag runs - the
//                      game's own choice (mt2009_rumi::EventNormal)
//      Yut Nori        83034 Brazowy Pakiet / 83031 Srebrne Trofeum /
//                      83030 Zlote Trofeum - what the game itself gives for
//                      under 150, 150-219 and 220+ points; the trophies hold
//                      the Srebrny/Zloty Pakiet (83033/83032)
//  - The chest. 30% go on a counter, 70% are opened by the chest pass
//    (playerbot_consumables.h, the special_item_group like a player's use).
//    Only a bot that keeps a counter for a living lists one: a dropper, the
//    merchant or a resource trader, with a counter at all. Those are some
//    two fifths of the bots, so each lists its chest at the share that makes
//    the world's 30% (PLAYERBOT_MG_LIST_PERCENT over the fitting bots' share,
//    worked out every ten minutes) and opens the rest. A kept chest is
//    counted in mt2009_botmg.hold_<vnum>: the chest pass leaves that many
//    shut, IsPlayerBotSurplusChest makes them counter goods at the sheet's
//    price (GetPlayerBotMaterialAskingBase - the owner's MT2009_PLUS_OWNER_
//    PRICES_V2 numbers); one not on a counter after PLAYERBOT_MG_HOLD_MAX_SEC
//    is opened after all.
//  - The buyers. A gambler (the addict and the four rare gamblers,
//    IsPlayerBotSessionGambler) buys chests off the counters to open them, up
//    to the greater of half again their worth and the sheet's price plus a
//    tenth, out of a quarter of its spare yang, PLAYERBOT_MG_BUY_DAY_GAMBLER a
//    day. Of the other bots a fixed share (PLAYERBOT_MG_FAN_PERCENT, by pid)
//    likes a chest's drop: it buys one no dearer than what it holds (the
//    group's expected value at the sheet's prices), out of a twelfth of its
//    spare yang, PLAYERBOT_MG_BUY_DAY a day. Never a bot that keeps that kind
//    for its own counter.
//  - Chests are opened, listed and bought any time; cards drop and games are
//    played only while the event runs.
//  - "PLAYERBOT_MINIGAME:" lines: every token made, every game, every chest
//    kept or bought; a census per game every ten minutes (cards, tokens,
//    games by tier, chests kept for counters, opened, bought).
//
// An implementation fragment in the sense playerbot_types.h describes:
// included once, after playerbot_offline_market.h.

namespace
{
	enum
	{
		PLAYERBOT_MG_GAMES = 3,
		PLAYERBOT_MG_FEE = 30000,				// a game, as a player pays
		PLAYERBOT_MG_TOKEN_HOLD = 5,			// decks/sets/boards a bot holds at most
		PLAYERBOT_MG_LIST_PERCENT = 30,			// the world's chests that go on a counter
		PLAYERBOT_MG_TIER_GOLD = 10,			// percent
		PLAYERBOT_MG_TIER_SILVER = 30,			// percent
		PLAYERBOT_MG_FAN_PERCENT = 35,			// non-gamblers that buy a chest at all
		PLAYERBOT_MG_BUY_DAY = 2,
		PLAYERBOT_MG_BUY_DAY_GAMBLER = 6,
		PLAYERBOT_MG_BUY_BUDGET_PERCENT = 8,
		PLAYERBOT_MG_BUY_BUDGET_PERCENT_GAMBLER = 25,
		PLAYERBOT_MG_BUY_MIN_LEVEL = 20,
		PLAYERBOT_MG_BUY_MIN_FREE_CELLS = 8,
		PLAYERBOT_MG_HOLD_MAX_SEC = 12 * 3600,
	};
	const DWORD PLAYERBOT_MG_PASS_MS = 45000;
	const DWORD PLAYERBOT_MG_CENSUS_MS = 600000;

	struct TPlayerBotMinigame
	{
		const char*	name;
		DWORD		token;			// the deck / set / board
		int			piecesPerToken;	// the game's own count
		const char*	seasonFlag;		// the event flag of the players' season
		const char*	qfCards;
		const char*	qfSeason;
		DWORD		chest[3];		// bronze, silver, gold
		DWORD		xmasChest[3];	// Rumi's Christmas chests, 0 elsewhere
	};

	const TPlayerBotMinigame PLAYERBOT_MINIGAMES[PLAYERBOT_MG_GAMES] = {
		{ "catchking", mt2009_catchking::VNUM_PACK, mt2009_catchking::PIECE_COUNT_MAX, mt2009_catchking::FLAG_SEASON,
			"mt2009_botmg.ck_cards", "mt2009_botmg.ck_season",
			{ mt2009_catchking::VNUM_LOOT[2], mt2009_catchking::VNUM_LOOT[1], mt2009_catchking::VNUM_LOOT[0] }, { 0, 0, 0 } },
		{ "rumi", mt2009_rumi::ITEM_CARD_PACK, mt2009_rumi::CARD_PIECE_MAX, mt2009_rumi::S_ID,
			"mt2009_botmg.okey_cards", "mt2009_botmg.okey_season",
			{ mt2009_rumi::REWARD_NORMAL_LOW, mt2009_rumi::REWARD_NORMAL_MID, mt2009_rumi::REWARD_NORMAL_HIGH },
			{ mt2009_rumi::REWARD_XMAS_LOW, mt2009_rumi::REWARD_XMAS_MID, mt2009_rumi::REWARD_XMAS_HIGH } },
		{ "yutnori", mt2009_yutnori::ITEM_YUT_BOARD, mt2009_yutnori::PIECE_COUNT_MAX, mt2009_yutnori::FLAG_SEASON,
			"mt2009_botmg.yut_pieces", "mt2009_botmg.yut_season",
			{ mt2009_yutnori::REWARD_LOW, mt2009_yutnori::REWARD_MID, mt2009_yutnori::REWARD_HIGH }, { 0, 0, 0 } },
	};

	// What the buyers may take off a counter: every chest the three games
	// hand out, and the two Yut Nori bundles inside the trophies.
	const DWORD PLAYERBOT_MG_BUYABLE[] = {
		50968, 50969, 50970,
		50275, 50276, 50277, 50267, 50268, 50269,
		83030, 83031, 83032, 83033, 83034,
	};

	struct TPlayerBotMinigameCensus
	{
		DWORD cards, tokens, games, tier[3], held, opened, bought;
		long long feesPaid, boughtYang;
	};
	TPlayerBotMinigameCensus s_aPlayerBotMgCensus[PLAYERBOT_MG_GAMES];
	DWORD s_dwPlayerBotMgNextCensus = 0;
	int s_iPlayerBotMgFitListPercent = 75;	// until the first census works it out
	std::map<DWORD, DWORD> s_mapPlayerBotMgNextPass;
	std::map<DWORD, std::pair<long, int> > s_mapPlayerBotMgBoughtDay;	// pid -> (day, chests)
	std::map<DWORD, std::pair<DWORD, long long> > s_mapPlayerBotMgWorth;	// vnum -> (price generation, worth)

	bool IsPlayerBotMinigameEventOn(int game)
	{
		switch (game)
		{
			case 0: return InGameEventIsActive(mt2009_catchking::EVENT_KEY);
			case 1: return mt2009_rumi::EventOn();
			case 2: return InGameEventIsActive(mt2009_yutnori::EVENT_KEY);
		}
		return false;
	}

	// The game whose chest this is, or -1.
	int GetPlayerBotMinigameOfChest(DWORD dwVnum)
	{
		for (int g = 0; g < PLAYERBOT_MG_GAMES; ++g)
			for (int t = 0; t < 3; ++t)
				if (PLAYERBOT_MINIGAMES[g].chest[t] == dwVnum ||
						(PLAYERBOT_MINIGAMES[g].xmasChest[t] && PLAYERBOT_MINIGAMES[g].xmasChest[t] == dwVnum))
					return g;
		if (dwVnum == 83032 || dwVnum == 83033)	// the two Yut Nori bundles inside the trophies
			return 2;
		return -1;
	}

	bool IsPlayerBotMinigameChestVnum(DWORD dwVnum)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_MG_BUYABLE) / sizeof(PLAYERBOT_MG_BUYABLE[0]); ++i)
			if (PLAYERBOT_MG_BUYABLE[i] == dwVnum)
				return true;
		return false;
	}

	int GetPlayerBotMgFlag(LPCHARACTER ch, const char* name)
	{
		const int v = ch->GetQuestFlag(name);
		return v > 0 ? v : 0;
	}

	void GetPlayerBotMgHoldFlags(DWORD dwVnum, char* count, size_t countSize, char* at, size_t atSize)
	{
		snprintf(count, countSize, "mt2009_botmg.hold_%u", dwVnum);
		snprintf(at, atSize, "mt2009_botmg.hold_at_%u", dwVnum);
	}

	// How many of this bot's chests of a kind are kept for its counter: never
	// more than it holds (a line that went up or sold takes its count with
	// it), and nothing once PLAYERBOT_MG_HOLD_MAX_SEC have passed without
	// them reaching a counter - the chest pass opens them then.
	int GetPlayerBotMinigameChestsHeld(LPCHARACTER ch, DWORD dwVnum)
	{
		if (!ch || !IsPlayerBotMinigameChestVnum(dwVnum))
			return 0;
		char qfCount[48], qfAt[48];
		GetPlayerBotMgHoldFlags(dwVnum, qfCount, sizeof(qfCount), qfAt, sizeof(qfAt));
		const int held = GetPlayerBotMgFlag(ch, qfCount);
		if (held <= 0)
			return 0;
		const int inBag = (int)ch->CountSpecifyItem(dwVnum);
		const long now = (long)time(NULL);
		if (now - (long)GetPlayerBotMgFlag(ch, qfAt) > PLAYERBOT_MG_HOLD_MAX_SEC)
		{
			ch->SetQuestFlag(qfCount, 0);
			if (inBag > 0)
				sys_log(0, "PLAYERBOT_MINIGAME: hold over pid=%u name=%s vnum=%u kept=%d in_bag=%d - opened instead",
						ch->GetPlayerID(), ch->GetName(), dwVnum, held, inBag);
			return 0;
		}
		if (held > inBag)
		{
			ch->SetQuestFlag(qfCount, inBag);
			return inBag;
		}
		return held;
	}

	bool IsPlayerBotMinigameFitSeller(DWORD dwPID, BYTE personality)
	{
		return IsPlayerBotDropper(personality) || personality == BOT_PERSONALITY_MERCHANT ||
				IsPlayerBotResourceTrader(dwPID);
	}

	// ------------------------------------------------------------ the cards

	// A kill's card (the kill hooks, for a bot only): the players' chance was
	// rolled there. The season's partial count goes as a player's does.
	void PlayerBotMinigameCard(LPCHARACTER ch, int game)
	{
		if (!ch || game < 0 || game >= PLAYERBOT_MG_GAMES || !ch->IsItemLoaded() ||
				IsPlayerBotSidekickPID(ch->GetPlayerID()))
			return;
		const TPlayerBotMinigame& mg = PLAYERBOT_MINIGAMES[game];
		const int season = quest::CQuestManager::instance().GetEventFlag(mg.seasonFlag);
		if (season > 0 && GetPlayerBotMgFlag(ch, mg.qfSeason) != season)
		{
			ch->SetQuestFlag(mg.qfCards, 0);
			ch->SetQuestFlag(mg.qfSeason, season);
		}
		if ((int)ch->CountSpecifyItem(mg.token) >= PLAYERBOT_MG_TOKEN_HOLD)
			return;
		const int cards = GetPlayerBotMgFlag(ch, mg.qfCards) + 1;
		++s_aPlayerBotMgCensus[game].cards;
		if (cards < mg.piecesPerToken)
		{
			ch->SetQuestFlag(mg.qfCards, cards);
			return;
		}
		// The last card waits for a free cell, as a player's 999th deck waits.
		if (ch->GetEmptyInventory(1) < 0)
		{
			ch->SetQuestFlag(mg.qfCards, mg.piecesPerToken - 1);
			PlayerBotLogThrottled("minigame_token_room", get_dword_time(),
					"PLAYERBOT_MINIGAME: no room for a token pid=%u name=%s game=%s", ch->GetPlayerID(), ch->GetName(), mg.name);
			return;
		}
		ch->SetQuestFlag(mg.qfCards, 0);
		LPITEM token = ch->AutoGiveItem(mg.token, 1, -1, false);
		if (!token)
		{
			sys_err("PLAYERBOT_MINIGAME: token %u not made for %s (pid %u)", mg.token, ch->GetName(), ch->GetPlayerID());
			return;
		}
		++s_aPlayerBotMgCensus[game].tokens;
		sys_log(0, "PLAYERBOT_MINIGAME: token pid=%u name=%s game=%s vnum=%u cards=%d held=%u",
				ch->GetPlayerID(), ch->GetName(), mg.name, mg.token, mg.piecesPerToken,
				(unsigned int)ch->CountSpecifyItem(mg.token));
	}

	// ------------------------------------------------------------ the census

	void UpdatePlayerBotMinigameCensus(DWORD dwNow)
	{
		if (s_dwPlayerBotMgNextCensus && (int)(dwNow - s_dwPlayerBotMgNextCensus) < 0)
			return;
		const bool first = s_dwPlayerBotMgNextCensus == 0;
		s_dwPlayerBotMgNextCensus = dwNow + PLAYERBOT_MG_CENSUS_MS;
		// The share of the bots that list: PLAYERBOT_MG_LIST_PERCENT of the
		// world's chests over the fitting sellers' share of the bots.
		int total = 0, fit = 0;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin(); it != s_mapPlayerBotAIStates.end(); ++it)
		{
			if (IsPlayerBotSidekickPID(it->first))
				continue;
			++total;
			if (IsPlayerBotMinigameFitSeller(it->first, it->second.bPersonality))
				++fit;
		}
		if (fit > 0)
			s_iPlayerBotMgFitListPercent = (int)std::min<long long>(100,
					((long long)PLAYERBOT_MG_LIST_PERCENT * total + fit - 1) / fit);
		if (first)
			return;
		for (int g = 0; g < PLAYERBOT_MG_GAMES; ++g)
		{
			const TPlayerBotMinigameCensus& c = s_aPlayerBotMgCensus[g];
			if (!IsPlayerBotMinigameEventOn(g) && !c.cards && !c.games && !c.opened && !c.bought && !c.held)
				continue;
			sys_log(0, "PLAYERBOT_MINIGAME: census game=%s event=%d cards=%u tokens=%u games=%u bronze=%u silver=%u gold=%u "
					"kept_for_counter=%u opened=%u bought=%u fees=%lld bought_yang=%lld bots=%d fit_sellers=%d fit_list_pct=%d",
					PLAYERBOT_MINIGAMES[g].name, IsPlayerBotMinigameEventOn(g) ? 1 : 0, c.cards, c.tokens, c.games,
					c.tier[0], c.tier[1], c.tier[2], c.held, c.opened, c.bought, c.feesPaid, c.boughtYang,
					total, fit, s_iPlayerBotMgFitListPercent);
		}
	}

	void NotePlayerBotMinigameChestOpened(LPCHARACTER ch, DWORD dwVnum)
	{
		const int game = GetPlayerBotMinigameOfChest(dwVnum);
		if (game < 0)
			return;
		++s_aPlayerBotMgCensus[game].opened;
		sys_log(0, "PLAYERBOT_MINIGAME: opened pid=%u name=%s game=%s vnum=%u",
				ch ? ch->GetPlayerID() : 0, ch ? ch->GetName() : "", PLAYERBOT_MINIGAMES[game].name, dwVnum);
	}

	// ------------------------------------------------------------ the game

	// The token to play: an unlocked one (not on a stall, not in a trade).
	LPITEM FindPlayerBotMinigameToken(LPCHARACTER ch, DWORD dwVnum)
	{
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetCell() == cell && item->GetVnum() == dwVnum && !item->isLocked() &&
					!item->IsExchanging() && item->GetCount() > 0)
				return item;
		}
		return NULL;
	}

	// One game: the token and the fee go, one chest comes by the roll, and
	// the chest is kept for the counter or left for the chest pass.
	bool PlayPlayerBotMinigame(LPCHARACTER ch, int game)
	{
		const TPlayerBotMinigame& mg = PLAYERBOT_MINIGAMES[game];
		LPITEM token = FindPlayerBotMinigameToken(ch, mg.token);
		if (!token)
			return false;
		if ((long long)ch->GetGold() < (long long)PLAYERBOT_MG_FEE * 2)
		{
			PlayerBotLogThrottled("minigame_no_fee", get_dword_time(),
					"PLAYERBOT_MINIGAME: no fee pid=%u name=%s game=%s gold=%lld", ch->GetPlayerID(), ch->GetName(),
					mg.name, (long long)ch->GetGold());
			return false;
		}
		if (ch->GetEmptyInventory(1) < 0)
			return false;
		const int roll = number(1, 100);
		const int tier = roll <= PLAYERBOT_MG_TIER_GOLD ? 2 : (roll <= PLAYERBOT_MG_TIER_GOLD + PLAYERBOT_MG_TIER_SILVER ? 1 : 0);
		const bool xmas = game == 1 && !mt2009_rumi::EventNormal();
		const DWORD chest = xmas ? mg.xmasChest[tier] : mg.chest[tier];
		if (!ITEM_MANAGER::instance().GetTable(chest))
		{
			PlayerBotLogThrottled("minigame_no_chest", get_dword_time(),
					"PLAYERBOT_MINIGAME: no item %u for game=%s", chest, mg.name);
			return false;
		}
		token->SetCount(token->GetCount() - 1);
		PlayerBotChangeGold(ch, -(long long)PLAYERBOT_MG_FEE);
		LPITEM given = ch->AutoGiveItem(chest, 1, -1, false);
		TPlayerBotMinigameCensus& c = s_aPlayerBotMgCensus[game];
		++c.games;
		++c.tier[tier];
		c.feesPaid += PLAYERBOT_MG_FEE;
		// The 30%: only a bot that keeps a counter for a living lists, at the
		// share that makes the world's 30% (UpdatePlayerBotMinigameCensus).
		const DWORD pid = ch->GetPlayerID();
		const bool fit = IsPlayerBotMinigameFitSeller(pid, GetPlayerBotPersonalityByPID(pid)) && PlayerBotHasCounter(ch);
		const bool list = given && fit && number(1, 100) <= s_iPlayerBotMgFitListPercent;
		if (list)
		{
			char qfCount[48], qfAt[48];
			GetPlayerBotMgHoldFlags(chest, qfCount, sizeof(qfCount), qfAt, sizeof(qfAt));
			ch->SetQuestFlag(qfCount, GetPlayerBotMinigameChestsHeld(ch, chest) + 1);
			ch->SetQuestFlag(qfAt, (int)time(NULL));
			++c.held;
		}
		static const char* const TIER_NAME[3] = { "bronze", "silver", "gold" };
		sys_log(0, "PLAYERBOT_MINIGAME: played pid=%u name=%s game=%s token=%u fee=%d tier=%s chest=%u given=%d fate=%s gold=%lld",
				pid, ch->GetName(), mg.name, mg.token, (int)PLAYERBOT_MG_FEE, TIER_NAME[tier], chest, given ? 1 : 0,
				!given ? "none" : (list ? "counter" : "open"), (long long)ch->GetGold());
		return true;
	}

	// Every PLAYERBOT_MG_PASS_MS a bot plays one game of an event that runs,
	// if it holds that game's token. Nothing walks anywhere.
	bool ManagePlayerBotMinigames(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		(void)state;
		UpdatePlayerBotMinigameCensus(dwNow);
		if (!ch || !ch->IsItemLoaded() || ch->GetExchange())
			return false;
		const DWORD pid = ch->GetPlayerID();
		DWORD& next = s_mapPlayerBotMgNextPass[pid];
		if (next && (int)(dwNow - next) < 0)
			return false;
		next = dwNow + PLAYERBOT_MG_PASS_MS + (pid % 30U) * 1000U;
		if (IsPlayerBotSidekickPID(pid))
			return false;
		for (int g = 0; g < PLAYERBOT_MG_GAMES; ++g)
			if (IsPlayerBotMinigameEventOn(g) && PlayPlayerBotMinigame(ch, g))
				return true;
		return false;
	}

	// ------------------------------------------------------------ the buyers

	// What a chest holds, at the sheet's prices: its group's lines weighed by
	// their chances (playerbot_moonlight_rules::ExpectedOpeningValue), a
	// Moonlight chest inside at its own worth, a box inside that the sheet
	// does not price at what it holds (one level down). Kept per price
	// generation.
	long long GetPlayerBotMinigameGroupWorth(DWORD dwVnum, int depth)
	{
		const CSpecialItemGroup* group = ITEM_MANAGER::instance().GetSpecialItemGroup(dwVnum);
		if (!group)
			return 0;
		std::vector<playerbot_moonlight_rules::TGroupLine> lines;
		for (size_t i = 0; i < group->m_vecItems.size() && i < group->m_vecProbs.size(); ++i)
		{
			const DWORD v = group->m_vecItems[i].vnum;
			playerbot_moonlight_rules::TGroupLine line;
			line.cumulative = group->m_vecProbs[i];
			line.count = group->m_vecItems[i].count;
			line.unit = v == PLAYERBOT_MOONLIGHT_CHEST_VNUM ? (long long)GetPlayerBotMoonlightChestWorth()
					: GetPlayerBotGroupUnitWorth(v);
			if (line.unit == 0 && depth < 1 && v > (DWORD)CSpecialItemGroup::MOB_GROUP)
			{
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(v);
				if (proto && proto->bType == ITEM_GIFTBOX)
					line.unit = GetPlayerBotMinigameGroupWorth(v, depth + 1);
			}
#if defined(PLAYERBOT_ENGINE_MT2009)
			if (group->m_vecItems[i].isSpecial)
				line.unit = 0;
#endif
			lines.push_back(line);
		}
		return lines.empty() ? 0 : playerbot_moonlight_rules::ExpectedOpeningValue(
				&lines[0], (int)lines.size(), group->m_bType == CSpecialItemGroup::PCT);
	}

	long long GetPlayerBotMinigameChestWorth(DWORD dwVnum)
	{
		const DWORD generation = GetPlayerBotPriceGeneration();
		std::map<DWORD, std::pair<DWORD, long long> >::const_iterator it = s_mapPlayerBotMgWorth.find(dwVnum);
		if (it != s_mapPlayerBotMgWorth.end() && it->second.first == generation)
			return it->second.second;
		const long long worth = std::max(0LL, GetPlayerBotMinigameGroupWorth(dwVnum, 0));
		s_mapPlayerBotMgWorth[dwVnum] = std::make_pair(generation, worth);
		sys_log(0, "PLAYERBOT_MINIGAME: chest worth vnum=%u worth=%lld sheet=%u", dwVnum, worth,
				GetPlayerBotMaterialAskingBase(dwVnum));
		return worth;
	}

	bool IsPlayerBotMinigameGambler(LPCHARACTER ch)
	{
		if (!ch || !IsPlayerBotPersonaEnabled())
			return false;
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(ch->GetPlayerID());
		return it != s_mapPlayerBotAIStates.end() && IsPlayerBotSessionGambler(it->second.persona, get_dword_time());
	}

	int GetPlayerBotMinigameBoughtToday(DWORD dwPID)
	{
		const long day = (long)(time(NULL) / 86400);
		std::map<DWORD, std::pair<long, int> >::const_iterator it = s_mapPlayerBotMgBoughtDay.find(dwPID);
		return it != s_mapPlayerBotMgBoughtDay.end() && it->second.first == day ? it->second.second : 0;
	}

	bool WantsPlayerBotMinigameChest(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !offer || !ch->IsItemLoaded() || (int)ch->GetLevel() < PLAYERBOT_MG_BUY_MIN_LEVEL ||
				IsPlayerBotSidekickPID(ch->GetPlayerID()))
			return false;
		const DWORD vnum = offer->GetVnum();
		// A bot keeping this kind for its own counter does not buy it back.
		if (GetPlayerBotMinigameChestsHeld(ch, vnum) > 0)
			return false;
		const bool gambler = IsPlayerBotMinigameGambler(ch);
		if (!gambler && (int)(PlayerBotNavHash(ch->GetPlayerID() ^ 0x4d47424eU) % 100U) >= PLAYERBOT_MG_FAN_PERCENT)
			return false;
		if (GetPlayerBotMinigameBoughtToday(ch->GetPlayerID()) >=
				(gambler ? PLAYERBOT_MG_BUY_DAY_GAMBLER : PLAYERBOT_MG_BUY_DAY))
			return false;
		if (CountPlayerBotFreeInventoryCells(ch) < PLAYERBOT_MG_BUY_MIN_FREE_CELLS || ch->GetEmptyInventory(3) < 0)
			return false;
		int cellsNeeded = 0;
		return PlayerBotBagTakesGroup(ch, vnum, cellsNeeded);
	}

	bool CanPlayerBotPayForMinigameChest(LPCHARACTER ch, LPITEM item, long long price)
	{
		if (!ch || !item || price <= 0)
			return false;
		const long long units = std::max<long long>(1, (long long)item->GetCount());
		const long long unit = price / units;
		const long long worth = GetPlayerBotMinigameChestWorth(item->GetVnum());
		const long long sheet = (long long)GetPlayerBotMaterialAskingBase(item->GetVnum());
		const long long spare = (long long)ch->GetGold() - GetPlayerBotReservedGold(ch) - PLAYERBOT_SHOPPING_GOLD_FLOOR;
		if (spare <= 0)
			return false;
		const bool gambler = IsPlayerBotMinigameGambler(ch);
		const long long unitCap = gambler ? std::max(worth * 150 / 100, sheet * 110 / 100) : worth;
		const long long budget = spare * (gambler ? PLAYERBOT_MG_BUY_BUDGET_PERCENT_GAMBLER : PLAYERBOT_MG_BUY_BUDGET_PERCENT) / 100;
		return unitCap > 0 && unit <= unitCap && price <= budget;
	}

	void NotePlayerBotMinigameChestBought(LPCHARACTER ch, DWORD dwVnum, long long price, DWORD count)
	{
		const int game = GetPlayerBotMinigameOfChest(dwVnum);
		if (!ch || game < 0)
			return;
		const long day = (long)(time(NULL) / 86400);
		std::pair<long, int>& bought = s_mapPlayerBotMgBoughtDay[ch->GetPlayerID()];
		if (bought.first != day)
			bought = std::make_pair(day, 0);
		++bought.second;
		s_aPlayerBotMgCensus[game].bought += std::max<DWORD>(1, count);
		s_aPlayerBotMgCensus[game].boughtYang += price;
		sys_log(0, "PLAYERBOT_MINIGAME: bought pid=%u name=%s game=%s vnum=%u count=%u price=%lld worth=%lld gambler=%d today=%d",
				ch->GetPlayerID(), ch->GetName(), PLAYERBOT_MINIGAMES[game].name, dwVnum, (unsigned int)count, price,
				GetPlayerBotMinigameChestWorth(dwVnum), IsPlayerBotMinigameGambler(ch) ? 1 : 0, bought.second);
	}
}

#endif
