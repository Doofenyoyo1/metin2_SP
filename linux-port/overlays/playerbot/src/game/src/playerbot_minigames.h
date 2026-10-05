#ifndef __INC_METIN2_PLAYERBOT_MINIGAMES_H__
#define __INC_METIN2_PLAYERBOT_MINIGAMES_H__

// MT2009_PLUS_BOT_MINIGAMES_V1 (the owner, 2 October): the bots take part in
// the three mini game events - Catch the King, Rumi (Okey) and Yut Nori - the
// simulated way. No bot walks to a table or sends a packet; it is all in here.
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
//  - The ranking (MT2009_PLUS_MINIGAME_BOT_RANKING_V1, the owner, 5 October:
//    "Boty niech wyswietlaja sie w rankingach mini gier eventowych"). A game
//    a bot plays may also go into the season's ranking table the NPCs read -
//    the very statement a player's game end runs: Catch the King
//    mt2009_catchking::RegisterScore (player.minigame_catchking), Rumi
//    mt2009_rumi::SaveScore (player.mt2009_rumi_score), Yut Nori the INSERT of
//    mt2009_yutnori::Finish (player.minigame_yutnori). The score fits the chest
//    the roll gave (the game's own bands: Catch the King 10-399 / 400-549 /
//    550+, Rumi <300 / 300-399 / 400+, Yut Nori <150 / 150-219 / 220+), a
//    multiple of ten as every score of the three games is, a gold one leaning
//    to the band's low end and kept well under each game's best possible one.
//    Not every bot ranks: PLAYERBOT_MG_RANK_PERCENT of them, drawn by pid
//    afresh each season and game, and each of those counts at most its own
//    3-8 games a day (PLAYERBOT_MG_RANK_DAY_MIN/MAX, an active player's day;
//    further games are played and paid as before, unranked). Only while the
//    event runs and its season is known and open - the seasons' own reset
//    (a new season id per event) starts the bots from nothing too.
//    The top-ten prizes of the three tables (10/5/3/1... gold chests) go by
//    the real place in the full ranking, bots included (the owner, 5
//    October: "boty niech normalnie dostaja nagrody"). A player claims his
//    own at the table as before; a bot never visits a table, so a bot of the
//    top ten is handed its place's prize by its next pass while the reward
//    window is open (GivePlayerBotMinigamePrizes): the same items and counts
//    the claim gives, marked claimed the claim's own way first (Rumi
//    minigame_rumi.claimed, Catch the King the "claimed = 0" UPDATE, Yut Nori
//    minigame_yutnori.reward_season), so neither the bot nor a second core
//    can take it twice. AutoGiveItem puts it in the bag; a full bag gets it
//    on the ground with the bot's ownership (the engine's overflow), where the
//    bot's loot pass picks it up. From there it is ordinary bot loot - the
//    chest pass and the counters, as for any of these chests.
//  - Dzieci Kwiaty (MT2009_PLUS_BOT_FLOWER_V1, the owner, 5 October: "Naucz
//    boty grac w Dzieci Kwiaty", every bot): a bot's kill rolls the seed with
//    a player's chance (FlowerEventOnKill) onto the same counter
//    (flower_event.envelope); while the event window is open (the event and
//    its reward window) each pass exchanges the seeds for shoots and every
//    ten shoots of one flower for its box, through the window's own
//    ExchangeSeeds / ExchangeShoots (counters, limits and the room check as a
//    player's); the boxes are gift boxes the chest pass opens. Seeds and
//    shoots are counters, never items, so nothing of them can be sold or
//    dropped; what is left after the window waits for the next event, as a
//    player's does.
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
		DWORD ranked;			// MT2009_PLUS_MINIGAME_BOT_RANKING_V1: games put into the ranking
		long long rankedScore;
	};
	TPlayerBotMinigameCensus s_aPlayerBotMgCensus[PLAYERBOT_MG_GAMES];
	// MT2009_PLUS_BOT_FLOWER_V1
	struct TPlayerBotFlowerCensus
	{
		DWORD seeds, seedsExchanged, shoots, boxes, opened, noRoom;
	};
	TPlayerBotFlowerCensus s_kPlayerBotFlowerCensus;
	// MT2009_PLUS_MINIGAME_BOT_RANKING_V1: the bots' top-ten prizes handed out.
	DWORD s_adwPlayerBotMgPrizes[PLAYERBOT_MG_GAMES];
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

	// ------------------------------------------------------------ Dzieci Kwiaty
	// MT2009_PLUS_BOT_FLOWER_V1 (see the head of the file).

	// A bot's kill gave a seed (FlowerEventOnKill rolled the players' chance).
	void PlayerBotFlowerSeed(LPCHARACTER ch)
	{
		using namespace mt2009_flower;
		if (!ch || IsPlayerBotSidekickPID(ch->GetPlayerID()))
			return;
		const TSettings& s = Settings();
		const int have = Counter(ch, SHOOT_ENVELOPE);
		if (have >= s.counterMax)
			return;
		SetCounter(ch, SHOOT_ENVELOPE, have + 1);
		++s_kPlayerBotFlowerCensus.seeds;
	}

	bool IsPlayerBotFlowerBoxVnum(DWORD dwVnum)
	{
		const mt2009_flower::TSettings& s = mt2009_flower::Settings();
		for (int t = SHOOT_CHRYSANTHEMUM; t <= SHOOT_SUNFLOWER; ++t)
			if (dwVnum && s.rewardVnum[t] == dwVnum)
				return true;
		return false;
	}

	// The window's exchange, as a player clicks it: the seeds into shoots (the
	// biggest of the window's amounts the seeds cover), then every flower with
	// enough shoots into its box, one box a flower a pass.
	bool ManagePlayerBotFlower(LPCHARACTER ch)
	{
		using namespace mt2009_flower;
		if (!WindowOpen() || ch->GetExchange() || ch->GetMyShop() || ch->GetShop() || ch->IsOpenSafebox() ||
				ch->IsCubeOpen() || ch->IsWarping() || ch->IsDead())
			return false;
		const TSettings& s = Settings();
		const int seeds = Counter(ch, SHOOT_ENVELOPE);
		int seedsUsed = 0, shootsMade = 0;
		if (seeds >= s.seedsPerShoot)
		{
			int wanted = 0;
			for (int k = 0; k < EXCHANGE_KEY_COUNT; ++k)
				if ((long long)EXCHANGE_KEYS[k] * s.seedsPerShoot <= seeds)
					wanted = EXCHANGE_KEYS[k];
			int shootsBefore = 0;
			for (int t = SHOOT_CHRYSANTHEMUM; t <= SHOOT_SUNFLOWER; ++t)
				shootsBefore += Counter(ch, t);
			if (wanted > 0)
				ExchangeSeeds(ch, wanted, s);
			seedsUsed = seeds - Counter(ch, SHOOT_ENVELOPE);
			for (int t = SHOOT_CHRYSANTHEMUM; t <= SHOOT_SUNFLOWER; ++t)
				shootsMade += Counter(ch, t);
			shootsMade -= shootsBefore;
		}
		int boxes = 0;
		bool noRoom = false;
		for (int t = SHOOT_CHRYSANTHEMUM; t <= SHOOT_SUNFLOWER; ++t)
		{
			const int have = Counter(ch, t);
			if (have < s.shootsPerReward)
				continue;
			ExchangeShoots(ch, t, 1, s);
			if (Counter(ch, t) >= have)
			{
				noRoom = true;	// the bag had no room: the next pass asks again
				break;
			}
			++boxes;
			sys_log(0, "PLAYERBOT_MINIGAME: flower box pid=%u name=%s flower=%s vnum=%u count=%d shoots_left=%d",
					ch->GetPlayerID(), ch->GetName(), SHOOT_NAME[t], s.rewardVnum[t], s.rewardCount[t], Counter(ch, t));
		}
		TPlayerBotFlowerCensus& c = s_kPlayerBotFlowerCensus;
		c.seedsExchanged += seedsUsed;
		c.shoots += shootsMade;
		c.boxes += boxes;
		if (noRoom)
		{
			++c.noRoom;
			PlayerBotLogThrottled("minigame_flower_room", get_dword_time(),
					"PLAYERBOT_MINIGAME: flower no room pid=%u name=%s", ch->GetPlayerID(), ch->GetName());
		}
		if (seedsUsed > 0)
			sys_log(0, "PLAYERBOT_MINIGAME: flower seeds pid=%u name=%s seeds=%d shoots=%d seeds_left=%d",
					ch->GetPlayerID(), ch->GetName(), seedsUsed, shootsMade, Counter(ch, SHOOT_ENVELOPE));
		return seedsUsed > 0 || boxes > 0;
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
					"kept_for_counter=%u opened=%u bought=%u fees=%lld bought_yang=%lld bots=%d fit_sellers=%d fit_list_pct=%d "
					"ranked=%u ranked_score=%lld",
					PLAYERBOT_MINIGAMES[g].name, IsPlayerBotMinigameEventOn(g) ? 1 : 0, c.cards, c.tokens, c.games,
					c.tier[0], c.tier[1], c.tier[2], c.held, c.opened, c.bought, c.feesPaid, c.boughtYang,
					total, fit, s_iPlayerBotMgFitListPercent, c.ranked, c.rankedScore);
		}
		for (int g = 0; g < PLAYERBOT_MG_GAMES; ++g)
			if (s_adwPlayerBotMgPrizes[g])
				sys_log(0, "PLAYERBOT_MINIGAME: census prizes game=%s bots_paid=%u", PLAYERBOT_MINIGAMES[g].name,
						s_adwPlayerBotMgPrizes[g]);
		const TPlayerBotFlowerCensus& f = s_kPlayerBotFlowerCensus;
		if (mt2009_flower::WindowOpen() || f.seeds || f.boxes || f.opened)
			sys_log(0, "PLAYERBOT_MINIGAME: census game=flower event=%d window=%d seeds=%u seeds_exchanged=%u shoots=%u "
					"boxes=%u opened=%u no_room=%u",
					mt2009_flower::Running() ? 1 : 0, mt2009_flower::WindowOpen() ? 1 : 0, f.seeds, f.seedsExchanged,
					f.shoots, f.boxes, f.opened, f.noRoom);
	}

	void NotePlayerBotMinigameChestOpened(LPCHARACTER ch, DWORD dwVnum)
	{
		if (IsPlayerBotFlowerBoxVnum(dwVnum))	// MT2009_PLUS_BOT_FLOWER_V1
		{
			++s_kPlayerBotFlowerCensus.opened;
			return;
		}
		const int game = GetPlayerBotMinigameOfChest(dwVnum);
		if (game < 0)
			return;
		++s_aPlayerBotMgCensus[game].opened;
		sys_log(0, "PLAYERBOT_MINIGAME: opened pid=%u name=%s game=%s vnum=%u",
				ch ? ch->GetPlayerID() : 0, ch ? ch->GetName() : "", PLAYERBOT_MINIGAMES[game].name, dwVnum);
	}

	// ------------------------------------------------------------ the ranking
	// MT2009_PLUS_MINIGAME_BOT_RANKING_V1 (see the head of the file).

	enum
	{
		PLAYERBOT_MG_RANK_PERCENT = 40,		// bots of a season that rank at all
		PLAYERBOT_MG_RANK_DAY_MIN = 3,		// ranked games a day, per bot: 3..8
		PLAYERBOT_MG_RANK_DAY_MAX = 8,
	};

	// A game's score by the chest's tier (bronze, silver, gold): the game's own
	// bands, the top ones well below the best a game can make (Catch the King
	// ~850, Rumi 560, Yut Nori ~300).
	const int PLAYERBOT_MG_SCORE_LO[PLAYERBOT_MG_GAMES][3] = { { 150, 400, 550 }, { 150, 300, 400 }, { 60, 150, 220 } };
	const int PLAYERBOT_MG_SCORE_HI[PLAYERBOT_MG_GAMES][3] = { { 390, 540, 680 }, { 290, 390, 490 }, { 140, 210, 270 } };

	// The day's count of ranked games (UTC day, games), per game.
	const char* const PLAYERBOT_MG_RANK_QF_DAY[PLAYERBOT_MG_GAMES] = {
		"mt2009_botmg.ck_rank_day", "mt2009_botmg.okey_rank_day", "mt2009_botmg.yut_rank_day" };
	const char* const PLAYERBOT_MG_RANK_QF_GAMES[PLAYERBOT_MG_GAMES] = {
		"mt2009_botmg.ck_rank_games", "mt2009_botmg.okey_rank_games", "mt2009_botmg.yut_rank_games" };

	// The season a game's score goes into now, 0 when none takes one: the
	// conditions under which a player's game counts.
	DWORD GetPlayerBotMinigameRankSeason(int game)
	{
		switch (game)
		{
			case 0:
				return InGameEventIsActive(mt2009_catchking::EVENT_KEY) ? mt2009_catchking::Season() : 0;
			case 1:
				return mt2009_rumi::EventOn() && mt2009_rumi::EventFlag(mt2009_rumi::S_STATE) == 1 ? mt2009_rumi::SeasonId() : 0;
			case 2:
				if (!InGameEventIsActive(mt2009_yutnori::EVENT_KEY) ||
						quest::CQuestManager::instance().GetEventFlag(mt2009_yutnori::FLAG_SEASON_CLOSED))
					return 0;
				return mt2009_yutnori::Season();
		}
		return 0;
	}

	int RollPlayerBotMinigameScore(int game, int tier)
	{
		const int lo = PLAYERBOT_MG_SCORE_LO[game][tier];
		const int steps = (PLAYERBOT_MG_SCORE_HI[game][tier] - lo) / 10;
		int step = number(0, steps);
		if (tier == 2)	// a gold game leans to the band's low end
			step = std::min(step, number(0, steps));
		return lo + step * 10;
	}

	// One played game into the season's ranking, as the game's own end writes
	// it. Returns the score, 0 when this game is not ranked.
	int RecordPlayerBotMinigameScore(LPCHARACTER ch, int game, int tier)
	{
		if (!ch || game < 0 || game >= PLAYERBOT_MG_GAMES || tier < 0 || tier > 2)
			return 0;
		const DWORD season = GetPlayerBotMinigameRankSeason(game);
		if (!season)
			return 0;
		const DWORD pid = ch->GetPlayerID();
		const DWORD hash = PlayerBotNavHash(pid ^ season ^ (0x524B4D47U + (DWORD)game));
		if ((int)(hash % 100U) >= PLAYERBOT_MG_RANK_PERCENT)
			return 0;
		const int dayCap = PLAYERBOT_MG_RANK_DAY_MIN +
				(int)((hash / 100U) % (DWORD)(PLAYERBOT_MG_RANK_DAY_MAX - PLAYERBOT_MG_RANK_DAY_MIN + 1));
		const int day = (int)(time(NULL) / 86400);
		int games = GetPlayerBotMgFlag(ch, PLAYERBOT_MG_RANK_QF_GAMES[game]);
		if (GetPlayerBotMgFlag(ch, PLAYERBOT_MG_RANK_QF_DAY[game]) != day)
		{
			games = 0;
			ch->SetQuestFlag(PLAYERBOT_MG_RANK_QF_DAY[game], day);
		}
		if (games >= dayCap)
			return 0;
		ch->SetQuestFlag(PLAYERBOT_MG_RANK_QF_GAMES[game], games + 1);
		const int score = RollPlayerBotMinigameScore(game, tier);
		switch (game)
		{
			case 0:
				mt2009_catchking::RegisterScore(ch, (DWORD)score);
				break;
			case 1:
				mt2009_rumi::SaveScore(pid, (WORD)score);
				break;
			case 2:
				DBManager::instance().Query(
						"INSERT INTO player.minigame_yutnori (season, pid, best_score, total_score, games, last_play) "
						"VALUES (%u, %u, %d, %d, 1, NOW()) ON DUPLICATE KEY UPDATE "
						"total_score = total_score + %d, best_score = GREATEST(best_score, %d), games = games + 1, last_play = NOW()",
						season, pid, score, score, score, score);
				break;
		}
		++s_aPlayerBotMgCensus[game].ranked;
		s_aPlayerBotMgCensus[game].rankedScore += score;
		return score;
	}

	// ------------------------------------------------------------ the prizes
	// MT2009_PLUS_MINIGAME_BOT_RANKING_V1 (see the head of the file).

	const DWORD PLAYERBOT_MG_TOP_READ_MS = 300000;
	// Yut Nori's prize counts (quest/minigame_yutnori.quest, prize_count).
	const int PLAYERBOT_MG_YUT_PRIZE[10] = { 10, 5, 3, 1, 1, 1, 1, 1, 1, 1 };
	const char* const PLAYERBOT_MG_CK_PRIZE_QF = "mt2009_botmg.ck_prize_season";
	const char* const PLAYERBOT_MG_YUT_PRIZE_QF = "minigame_yutnori.reward_season";	// the quest's pc.setqf("reward_season")

	struct TPlayerBotMgTop
	{
		DWORD season;
		DWORD nextRead;
		std::vector<DWORD> pids;	// the top ten, in place order
	};
	TPlayerBotMgTop s_aPlayerBotMgTop[PLAYERBOT_MG_GAMES];

	// The season whose top-ten prize can be taken now (the claim's own
	// conditions), 0 when none.
	DWORD GetPlayerBotMinigamePrizeSeason(int game)
	{
		const long now = (long)time(NULL);
		switch (game)
		{
			case 0:
			{
				const DWORD season = mt2009_catchking::Season();
				return season && !InGameEventIsActive(mt2009_catchking::EVENT_KEY) &&
						(long)InGameEventRewardEndTime(mt2009_catchking::EVENT_KEY) > now ? season : 0;
			}
			case 1:
			{
				const DWORD season = mt2009_rumi::SeasonId();
				return season && mt2009_rumi::EventFlag(mt2009_rumi::S_STATE) == 2 &&
						mt2009_rumi::RewardEnd() > (DWORD)get_global_time() ? season : 0;
			}
			case 2:
			{
				const DWORD season = mt2009_yutnori::Season();
				return season && quest::CQuestManager::instance().GetEventFlag(mt2009_yutnori::FLAG_SEASON_CLOSED) &&
						quest::CQuestManager::instance().GetEventFlag("mini_game_yutnori_reward") > now ? season : 0;
			}
		}
		return 0;
	}

	// The top ten the claim reads (the same order), read again every
	// PLAYERBOT_MG_TOP_READ_MS.
	const std::vector<DWORD>& GetPlayerBotMinigameTopTen(int game, DWORD season, DWORD dwNow)
	{
		TPlayerBotMgTop& top = s_aPlayerBotMgTop[game];
		if (top.season == season && top.nextRead && (int)(dwNow - top.nextRead) < 0)
			return top.pids;
		top.season = season;
		top.nextRead = dwNow + PLAYERBOT_MG_TOP_READ_MS;
		top.pids.clear();
		std::unique_ptr<SQLMsg> msg;
		if (game == 0)
			msg = DBManager::instance().DirectQuery(
					"SELECT pid FROM player.minigame_catchking WHERE season = %u AND games > 0 AND total_score >= %d "
					"ORDER BY total_score DESC, max_score DESC, pid ASC LIMIT 10", season, (int)mt2009_catchking::REWARD_MIN_SCORE);
		else if (game == 1)
			msg = DBManager::instance().DirectQuery(
					"SELECT s.pid FROM player.mt2009_rumi_score s JOIN player.player p ON p.id = s.pid "
					"WHERE s.season = %u AND s.total_score > 0 AND p.name NOT LIKE '[%%' ORDER BY s.total_score DESC, s.last_play ASC LIMIT 10",
					season);
		else
			msg = DBManager::instance().DirectQuery(
					"SELECT y.pid FROM player.minigame_yutnori y JOIN player.player p ON p.id = y.pid "
					"WHERE y.season = %u AND y.total_score > 0 AND p.name NOT LIKE '[%%' ORDER BY y.total_score DESC, y.last_play ASC LIMIT 10",
					season);
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return top.pids;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			DWORD pid = 0;
			if (row[0])
				str_to_number(pid, row[0]);
			top.pids.push_back(pid);
		}
		return top.pids;
	}

	// A bot of a top ten gets its place's prize, as its claim would give it.
	bool GivePlayerBotMinigamePrizes(LPCHARACTER ch, DWORD dwNow)
	{
		bool given = false;
		const DWORD pid = ch->GetPlayerID();
		for (int g = 0; g < PLAYERBOT_MG_GAMES; ++g)
		{
			const DWORD season = GetPlayerBotMinigamePrizeSeason(g);
			if (!season)
				continue;
			const std::vector<DWORD>& top = GetPlayerBotMinigameTopTen(g, season, dwNow);
			int rank = -1;
			for (size_t i = 0; i < top.size() && i < 10; ++i)
				if (top[i] == pid)
				{
					rank = (int)i;
					break;
				}
			if (rank < 0)
				continue;
			DWORD vnum = 0;
			int count = 0;
			switch (g)
			{
				case 0:
				{
					if ((DWORD)ch->GetQuestFlag(PLAYERBOT_MG_CK_PRIZE_QF) == season)
						continue;
					vnum = mt2009_catchking::VNUM_LOOT[0];
					count = mt2009_catchking::TOP_PRIZE[rank];
					if (!ITEM_MANAGER::instance().GetTable(vnum))
						continue;
					// The claim's own mark: only one core, once.
					std::unique_ptr<SQLMsg> upd(DBManager::instance().DirectQuery(
							"UPDATE player.minigame_catchking SET claimed = 1 WHERE season = %u AND pid = %u AND claimed = 0",
							season, pid));
					if (!upd.get() || upd->uiSQLErrno != 0 || !upd->Get())
						continue;	// no answer: asked again next pass
					// Taken now, or before (the row says so): asked no more.
					ch->SetQuestFlag(PLAYERBOT_MG_CK_PRIZE_QF, (int)season);
					if (upd->Get()->uiAffectedRows != 1)
						continue;
					break;
				}
				case 1:
					if ((DWORD)ch->GetQuestFlag(mt2009_rumi::F_CLAIMED) == season)
						continue;
					vnum = mt2009_rumi::SeasonNormal() ? mt2009_rumi::REWARD_NORMAL_HIGH : mt2009_rumi::REWARD_XMAS_HIGH;
					count = mt2009_rumi::RANK_PRIZE[rank];
					if (!ITEM_MANAGER::instance().GetTable(vnum))
						continue;
					ch->SetQuestFlag(mt2009_rumi::F_CLAIMED, (int)season);
					break;
				case 2:
					if ((DWORD)ch->GetQuestFlag(PLAYERBOT_MG_YUT_PRIZE_QF) == season)
						continue;
					vnum = mt2009_yutnori::REWARD_HIGH;
					count = PLAYERBOT_MG_YUT_PRIZE[rank];
					if (!ITEM_MANAGER::instance().GetTable(vnum))
						continue;
					ch->SetQuestFlag(PLAYERBOT_MG_YUT_PRIZE_QF, (int)season);
					break;
			}
			LPITEM item = ch->AutoGiveItem(vnum, (ITEM_COUNT)count, -1, false);
			++s_adwPlayerBotMgPrizes[g];
			given = true;
			sys_log(0, "PLAYERBOT_MINIGAME: prize pid=%u name=%s game=%s season=%u rank=%d vnum=%u count=%d where=%s",
					pid, ch->GetName(), PLAYERBOT_MINIGAMES[g].name, season, rank + 1, vnum, count,
					!item ? "none" : (item->GetOwner() == ch ? "bag" : "ground"));
		}
		return given;
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
		// MT2009_PLUS_MINIGAME_BOT_RANKING_V1: the game's score into the season's ranking.
		const int ranked = RecordPlayerBotMinigameScore(ch, game, tier);
		static const char* const TIER_NAME[3] = { "bronze", "silver", "gold" };
		sys_log(0, "PLAYERBOT_MINIGAME: played pid=%u name=%s game=%s token=%u fee=%d tier=%s chest=%u given=%d fate=%s gold=%lld ranked_score=%d",
				pid, ch->GetName(), mg.name, mg.token, (int)PLAYERBOT_MG_FEE, TIER_NAME[tier], chest, given ? 1 : 0,
				!given ? "none" : (list ? "counter" : "open"), (long long)ch->GetGold(), ranked);
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
		// MT2009_PLUS_MINIGAME_BOT_RANKING_V1: a top ten's prize in the reward window.
		bool acted = GivePlayerBotMinigamePrizes(ch, dwNow);
		// MT2009_PLUS_BOT_FLOWER_V1: the Dzieci Kwiaty window's exchange.
		if (ManagePlayerBotFlower(ch))
			acted = true;
		for (int g = 0; g < PLAYERBOT_MG_GAMES; ++g)
			if (IsPlayerBotMinigameEventOn(g) && PlayPlayerBotMinigame(ch, g))
				return true;
		return acted;
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
