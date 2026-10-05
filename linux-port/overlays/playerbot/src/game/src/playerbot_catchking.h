// MT2009 PLUS Catch the King ("Zlap Krola") - MT2009_PLUS_CATCH_KING_V1
// (the operator, 30 September: Owsap's mini games in full, with real packets
// in a new exe, plugged into the in-game event manager).
//
// After Owsap's CMiniGameCatchKing (minigame_catchking.cpp, v6.2.6, with
// __CATCH_KING_EVENT_FLAG_RENEWAL__): a 5x5 field of 25 hidden cards (seven
// 1s, four 2s, five 3s, five 4s, three 5s and the King), a hand of twelve
// (five 1s, two 2s, two 3s, a 4, a 5 and the King, in that order); a hand card
// turns a field card: a higher hand card scores the field card and stays, an
// equal one scores and is spent, a lower one is spent for nothing; a 5 next to
// a hidden 5 is caught; the King scores 100 only on the King and ends the
// game; a full row or column is +10. 10+ points win a King's Loot per deck
// bet (1-5 decks, 30 000 yang each): Bronze 10-399, Silver 400-549, Gold 550+.
// The cards of a deck come from kills while the event runs (a share of every
// kill, "mini_game_catchking_drop"; 25 cards = 1 deck, 999 decks at most) -
// quest flags minigame_catchking.piece_count / pack_count, Owsap's names.
//
// The packets are Owsap's (packet.h, server-patches/catchking): CG 226
// {header, sub, arg} and GC 238 {header, WORD size, sub} + the sub's payload.
// Everything is decided here; the client only draws.
//
// What is different from Owsap (its bugs and holes):
//  - the game lives here, per player id, not in CHARACTER members; one game at
//    a time, every click checked against it (a turned card, a card out of the
//    field, a click without a hand card, a hand card while one is held, the
//    reward before the last card - all refused);
//  - a score under 10 ended nothing in Owsap: the game stayed "on", the player
//    could never start another until a relog. Here the reward always ends it;
//  - the reward was refused once the event had ended - decks and yang paid,
//    nothing back. A game begun during the event can be finished after it;
//  - a logout, a warp or a channel change mid-game lost everything. Here the
//    game is closed with the points it has (the cards left could only add
//    points, so this is never better than playing on) and its Loot waits in
//    quest flags for the next login (minigame_catchking.quest hands it out);
//  - the "5 next to a 5" rule counted a 5 already turned; the rules (and the
//    Polish description) speak of a hidden 5 - only a hidden one counts;
//  - one generator, seeded once (Owsap: std::srand(time) per game besides);
//  - scores by player id into player.minigame_catchking, per event "season",
//    with no SQL built from a name; the ranking reads one season;
//  - "no more cards" (999 decks) is said once a minute, not on every kill;
//  - the official top-10 prizes (Owsap's quest showed them but had no way to
//    collect): during the 7-day reward window after the event, at the table
//    NPC, once per player and season (an UPDATE ... claimed = 0 that only one
//    core can win);
//  - a bot is never sent a packet; it gathers cards from its kills and
//    "plays" a deck without a table (MT2009_PLUS_BOT_MINIGAMES_V1,
//    playerbot_minigames.h); some of those games are ranked like a player's
//    (MT2009_PLUS_MINIGAME_BOT_RANKING_V1, RegisterScore), and a bot of the
//    top ten gets its place's prize in the reward window (the same
//    "claimed = 0" UPDATE a player's claim wins);
//  - an unasked packet (a card from a kill) goes only to a client that has
//    sent this game a packet - an exe without the header would stop on it.
//
// Season: the event flag mini_game_catchking_season (epoch of the season's
// start), written by the leading core: set when the event starts, cleared
// once the event and its reward window are over. A player's cards, decks and
// best score belong to a season (minigame_catchking.season) and go when the
// next one starts.
//
// Engine hooks (server-patches/catchking): input_main.cpp (CatchKingProcess),
// item_manager.cpp CreateQuestDropItem (CatchKingOnKill), char.cpp
// CHARACTER::Disconnect (CatchKingOnDisconnect), questlua_game.cpp
// (game.get_catchking_score / get_catchking_myscore / catchking_claim_reward /
// catchking_use_item / catchking_deliver). playerbot_events.h ticks
// CatchKingTick every pass.
#include <random>

int GetDropPerKillPct(int iMinimum, int iDefault, int iDeltaPercent, const char* c_pszFlag);	// item_manager.cpp
bool InGameEventIsActive(const char* key);	// playerbot_ingame_events.h
DWORD InGameEventRewardEndTime(const char* key);
// MT2009_PLUS_BOT_MINIGAMES_V1: a bot's card (playerbot_minigames.h, later in the unit).
namespace
{
	void PlayerBotMinigameCard(LPCHARACTER ch, int game);
}

namespace mt2009_catchking
{
	const char* const EVENT_KEY = "catchking";
	const char* const FLAG_DROP = "mini_game_catchking_drop";
	const char* const FLAG_SEASON = "mini_game_catchking_season";
	const char* const QF_PIECE = "minigame_catchking.piece_count";
	const char* const QF_PACK = "minigame_catchking.pack_count";
	const char* const QF_SEASON = "minigame_catchking.season";
	const char* const QF_BEST = "minigame_catchking.best_score";
	const char* const QF_PEND[3] = { "minigame_catchking.pend_gold", "minigame_catchking.pend_silver", "minigame_catchking.pend_bronze" };

	enum
	{
		PIECE_COUNT_MAX = 25,
		PACK_COUNT_MAX = 999,
		FIELD_CARDS = 25,
		HAND_CARDS = 12,
		CARD_KING = 6,
		BET_MAX = 5,
		COST_PER_DECK = 30000,
		REWARD_MIN_SCORE = 10,
		REWARD_MID_SCORE = 400,
		REWARD_HIGH_SCORE = 550,
		KING_POINTS = 100,
		LINE_POINTS = 10,
	};

	// The tokens keep Owsap's (free here) vnums; the three Loots do not:
	// Owsap's 50928-50930 are this world's "Receptura" items.
	const DWORD VNUM_PIECE = 79603;		// Karta Krolewska
	const DWORD VNUM_PACK = 79604;		// Talia Krolewska
	const DWORD VNUM_LOOT[3] = { 50968, 50969, 50970 };	// Zloty / Srebrny / Brazowy Lup Krolewski

	// The official top-10 prizes (Owsap's locale_quest 10127-10130): Golden Loots.
	const int TOP_PRIZE[10] = { 10, 5, 3, 1, 1, 1, 1, 1, 1, 1 };

	const DWORD BOOT_DELAY_MS = 30000;
	const DWORD SEASON_CLEAR_HOLD_MS = 60000;
	const long REQUEST_HOLD_SECONDS = 15;
	const DWORD NO_MORE_GAIN_INTERVAL_MS = 60000;

	// Owsap's sub-headers (packet.h).
	enum
	{
		CG_START = 0,
		CG_CLICK_HAND = 1,
		CG_CLICK_CARD = 2,
		CG_REWARD = 3,
		CG_REQUEST_QUEST_FLAG = 4,
	};
	enum
	{
		GC_START = 0,
		GC_SET_CARD = 1,
		GC_RESULT_FIELD = 2,
		GC_SET_END_CARD = 3,
		GC_REWARD = 4,
		GC_SET_CARD_PIECE_FLAG = 5,
		GC_SET_CARD_FLAG = 6,
		GC_SET_QUEST_FLAG = 7,
		GC_NO_MORE_GAIN = 8,
	};

	struct TCard
	{
		BYTE	index;		// 1-5, 6 = the King
		bool	exposed;
	};

	struct TGame
	{
		TCard	field[FIELD_CARDS];
		BYTE	hand;		// the card held, 0 none
		BYTE	handLeft;	// the deck's cards not yet drawn
		BYTE	bet;		// decks bet (= Loots won)
		DWORD	score;
		bool	over;		// the King was played: only the reward is left
	};

	std::map<DWORD, TGame> s_games;			// pid -> the game in progress
	std::set<DWORD> s_capable;				// pids whose client sent this game a packet
	std::map<DWORD, DWORD> s_noMoreGainAt;	// pid -> when it was last told
	std::mt19937 s_rng(std::random_device{}());

	DWORD	s_dwBootAt = 0;
	DWORD	s_dwNextSecond = 0;
	bool	s_bLeadInit = false;
	bool	s_bWasActive = false;
	DWORD	s_dwClearSince = 0;
	long	s_lSeasonAskedAt = 0;
	std::map<std::string, std::pair<int, long> > s_requested;

	bool Eligible(LPCHARACTER ch)
	{
		return ch && ch->IsPC() && ch->GetDesc() && !ch->GetDesc()->IsBot();
	}

	int EventFlag(const char* name)
	{
		return quest::CQuestManager::instance().GetEventFlag(name);
	}

	DWORD Season()
	{
		const int s = EventFlag(FLAG_SEASON);
		return s > 0 ? (DWORD)s : 0;
	}

	void Request(const char* name, int value)
	{
		const long now = (long)time(NULL);
		std::map<std::string, std::pair<int, long> >::iterator it = s_requested.find(name);
		if (it != s_requested.end() && it->second.first == value && now - it->second.second < REQUEST_HOLD_SECONDS)
			return;
		s_requested[name] = std::make_pair(value, now);
		quest::CQuestManager::instance().RequestSetEventFlag(name, value);
		sys_log(0, "CATCH_KING: flag %s = %d", name, value);
	}

	int QF(LPCHARACTER ch, const char* name)
	{
		const int v = ch->GetQuestFlag(name);
		return v > 0 ? v : 0;
	}

	// A player's cards, decks and best score belong to one season; the next
	// season starts them from nothing. A player who has none yet (or gathered
	// some in the moments before the leader wrote the season) keeps them.
	void SyncSeason(LPCHARACTER ch)
	{
		const DWORD season = Season();
		if (!season)
			return;
		const DWORD mine = (DWORD)QF(ch, QF_SEASON);
		if (mine == season)
			return;
		if (mine)
		{
			ch->SetQuestFlag(QF_PIECE, 0);
			ch->SetQuestFlag(QF_PACK, 0);
			ch->SetQuestFlag(QF_BEST, 0);
		}
		ch->SetQuestFlag(QF_SEASON, (int)season);
	}

	// ------------------------------------------------------------ packets

	void Send(LPCHARACTER ch, BYTE sub, const void* data, size_t len)
	{
		if (!Eligible(ch))
			return;
		TPacketGCMiniGameCatchKing head;
		head.bHeader = HEADER_GC_MINI_GAME_CATCH_KING;
		head.bSubHeader = sub;
		head.wSize = (WORD)(sizeof(head) + len);
		std::vector<BYTE> buf(head.wSize);
		memcpy(&buf[0], &head, sizeof(head));
		if (len)
			memcpy(&buf[sizeof(head)], data, len);
		ch->GetDesc()->Packet(&buf[0], (int)buf.size());
	}

	// A packet the client did not ask for: only to an exe that knows the header.
	bool MaySendUnasked(LPCHARACTER ch)
	{
		return Eligible(ch) && s_capable.count(ch->GetPlayerID());
	}

	void SendQuestFlag(LPCHARACTER ch, BYTE sub)
	{
		TPacketGCMiniGameCatchKingQuestFlag p;
		p.wPieceCount = (WORD)QF(ch, QF_PIECE);
		p.wPackCount = (WORD)QF(ch, QF_PACK);
		Send(ch, sub, &p, sizeof(p));
	}

	// ------------------------------------------------------------ cards and decks

	// Owsap's UpdateQuestFlag: one card more, 25 make a deck. False when
	// nothing was added (no event, or 999 decks).
	bool AddPiece(LPCHARACTER ch, bool fromItem)
	{
		if (!Eligible(ch) || !InGameEventIsActive(EVENT_KEY))
			return false;
		SyncSeason(ch);
		const int pieces = QF(ch, QF_PIECE);
		const int packs = QF(ch, QF_PACK);
		if (packs >= PACK_COUNT_MAX)
		{
			const DWORD now = get_dword_time();
			DWORD& last = s_noMoreGainAt[ch->GetPlayerID()];
			if (fromItem || !last || now - last >= NO_MORE_GAIN_INTERVAL_MS)
			{
				last = now;
				if (MaySendUnasked(ch))
					SendQuestFlag(ch, GC_NO_MORE_GAIN);
				else
					ch->ChatPacket(CHAT_TYPE_INFO, "[Zlap Krola] Masz juz najwiecej talii krolewskich (%d).", PACK_COUNT_MAX);
			}
			return false;
		}
		if (pieces + 1 >= PIECE_COUNT_MAX)
		{
			ch->SetQuestFlag(QF_PIECE, 0);
			ch->SetQuestFlag(QF_PACK, packs + 1);
			if (MaySendUnasked(ch))
				SendQuestFlag(ch, GC_SET_CARD_FLAG);
			else
				ch->ChatPacket(CHAT_TYPE_INFO, "[Zlap Krola] 25 kart krolewskich dalo nowa Talie Krolewska (masz %d).", packs + 1);
		}
		else
		{
			ch->SetQuestFlag(QF_PIECE, pieces + 1);
			if (MaySendUnasked(ch))
				SendQuestFlag(ch, GC_SET_CARD_PIECE_FLAG);
		}
		return true;
	}

	bool AddPack(LPCHARACTER ch)
	{
		if (!Eligible(ch) || !InGameEventIsActive(EVENT_KEY))
			return false;
		SyncSeason(ch);
		const int packs = QF(ch, QF_PACK);
		if (packs >= PACK_COUNT_MAX)
		{
			if (MaySendUnasked(ch))
				SendQuestFlag(ch, GC_NO_MORE_GAIN);
			else
				ch->ChatPacket(CHAT_TYPE_INFO, "[Zlap Krola] Masz juz najwiecej talii krolewskich (%d).", PACK_COUNT_MAX);
			return false;
		}
		ch->SetQuestFlag(QF_PACK, packs + 1);
		if (MaySendUnasked(ch))
			SendQuestFlag(ch, GC_SET_CARD_FLAG);
		return true;
	}

	// ------------------------------------------------------------ rewards

	int LootTier(DWORD score)
	{
		if (score >= REWARD_HIGH_SCORE)
			return 0;
		if (score >= REWARD_MID_SCORE)
			return 1;
		if (score >= REWARD_MIN_SCORE)
			return 2;
		return -1;
	}

	// The Loots kept for a player who left mid-game, given now if the bag
	// has room (one stack each).
	void DeliverPending(LPCHARACTER ch)
	{
		if (!Eligible(ch))
			return;
		for (int tier = 0; tier < 3; ++tier)
		{
			const int count = QF(ch, QF_PEND[tier]);
			if (count <= 0)
				continue;
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(VNUM_LOOT[tier]);
			if (!proto)
			{
				sys_err("CATCH_KING: no item %u for %s's pending Loot", VNUM_LOOT[tier], ch->GetName());
				continue;
			}
			if (ch->GetEmptyInventory(proto->bSize) < 0)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, "[Zlap Krola] Czeka na ciebie nagroda z przerwanej gry - zrob miejsce w ekwipunku i zaloguj sie ponownie.");
				return;
			}
			ch->SetQuestFlag(QF_PEND[tier], 0);
			ch->AutoGiveItem(VNUM_LOOT[tier], count);
			sys_log(0, "CATCH_KING: %s (pid %u) gets the pending %u x%d", ch->GetName(), ch->GetPlayerID(), VNUM_LOOT[tier], count);
		}
	}

	void RegisterScore(LPCHARACTER ch, DWORD score)
	{
		const DWORD season = Season();
		const int best = QF(ch, QF_BEST);
		if ((int)score > best)
			ch->SetQuestFlag(QF_BEST, (int)score);
		if (!season)
		{
			sys_log(0, "CATCH_KING: %s scored %u with no season yet - not ranked", ch->GetName(), score);
			return;
		}
		char name[CHARACTER_NAME_MAX_LEN * 2 + 1];
		DBManager::instance().EscapeString(name, sizeof(name), ch->GetName(), strlen(ch->GetName()));
		DBManager::instance().Query(
			"INSERT INTO player.minigame_catchking (season, pid, name, empire, max_score, total_score, games, claimed, last_play) "
			"VALUES (%u, %u, '%s', %u, %u, %u, 1, 0, NOW()) "
			"ON DUPLICATE KEY UPDATE name = VALUES(name), empire = VALUES(empire), max_score = GREATEST(max_score, VALUES(max_score)), "
			"total_score = total_score + VALUES(total_score), games = games + 1, last_play = NOW()",
			season, ch->GetPlayerID(), name, (unsigned int)ch->GetEmpire(), score, score);
	}

	// The game's end: the score is ranked, the Loots given (or kept for the
	// next login), the game gone. Returns Owsap's code: 0 a Loot, 1 too few points.
	BYTE Finish(LPCHARACTER ch, bool leaving)
	{
		std::map<DWORD, TGame>::iterator it = s_games.find(ch->GetPlayerID());
		if (it == s_games.end())
			return 1;
		const TGame game = it->second;
		s_games.erase(it);
		RegisterScore(ch, game.score);
		const int tier = LootTier(game.score);
		sys_log(0, "CATCH_KING: %s (pid %u) ends with %u points, bet %u, tier %d%s", ch->GetName(), ch->GetPlayerID(),
				game.score, (unsigned int)game.bet, tier, leaving ? " (left the game)" : "");
		if (tier < 0)
			return 1;
		if (leaving)
		{
			ch->SetQuestFlag(QF_PEND[tier], QF(ch, QF_PEND[tier]) + game.bet);
			return 0;
		}
		ch->AutoGiveItem(VNUM_LOOT[tier], game.bet);
		return 0;
	}

	// ------------------------------------------------------------ the game

	void StartGame(LPCHARACTER ch, BYTE bet)
	{
		const DWORD pid = ch->GetPlayerID();
		if (s_games.count(pid))
			return;
		if (!InGameEventIsActive(EVENT_KEY))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Zlap Krola] Event teraz nie trwa.");
			return;
		}
		if (bet < 1 || bet > BET_MAX)
			return;
		SyncSeason(ch);
		DeliverPending(ch);
		const int packs = QF(ch, QF_PACK);
		const YANG cost = (YANG)COST_PER_DECK * bet;
		if (packs < bet || ch->GetGold() < cost)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Zlap Krola] Nie masz dosc Talii Krolewskich albo yang (%d talii i %d yang za gre).",
					(int)bet, (int)cost);
			return;
		}
		ch->SetQuestFlag(QF_PACK, packs - bet);
		PlayerBotChangeGold(ch, -(long long)cost);

		TGame game = TGame();
		BYTE deck[FIELD_CARDS];
		int n = 0;
		for (int i = 0; i < 7; ++i) deck[n++] = 1;
		for (int i = 0; i < 4; ++i) deck[n++] = 2;
		for (int i = 0; i < 5; ++i) deck[n++] = 3;
		for (int i = 0; i < 5; ++i) deck[n++] = 4;
		for (int i = 0; i < 3; ++i) deck[n++] = 5;
		deck[n++] = CARD_KING;
		std::shuffle(deck, deck + FIELD_CARDS, s_rng);
		for (int i = 0; i < FIELD_CARDS; ++i)
		{
			game.field[i].index = deck[i];
			game.field[i].exposed = false;
		}
		game.hand = 0;
		game.handLeft = HAND_CARDS;
		game.bet = bet;
		game.score = 0;
		game.over = false;
		s_games[pid] = game;
		sys_log(0, "CATCH_KING: %s (pid %u) starts, bet %u", ch->GetName(), pid, (unsigned int)bet);

		const DWORD best = (DWORD)QF(ch, QF_BEST);
		Send(ch, GC_START, &best, sizeof(best));
	}

	// Owsap's DeckCardClick: the next card of the deck, smallest first.
	void DeckCardClick(LPCHARACTER ch)
	{
		std::map<DWORD, TGame>::iterator it = s_games.find(ch->GetPlayerID());
		if (it == s_games.end())
			return;
		TGame& g = it->second;
		if (g.over || g.hand || !g.handLeft)
			return;
		const BYTE left = g.handLeft;
		BYTE card;
		if (left > 7)
			card = 1;
		else if (left > 5)
			card = 2;
		else if (left > 3)
			card = 3;
		else if (left == 3)
			card = 4;
		else if (left == 2)
			card = 5;
		else
			card = CARD_KING;
		g.hand = card;
		g.handLeft = left - 1;
		Send(ch, GC_SET_CARD, &card, sizeof(card));
	}

	void FieldCardClick(LPCHARACTER ch, BYTE pos)
	{
		std::map<DWORD, TGame>::iterator it = s_games.find(ch->GetPlayerID());
		if (it == s_games.end())
			return;
		TGame& g = it->second;
		if (g.over || !g.hand || pos >= FIELD_CARDS || g.field[pos].exposed)
			return;
		const BYTE hand = g.hand;
		const BYTE value = g.field[pos].index;

		// A hidden 5 in the eight cells around (Owsap's neighbourhood, edges
		// cut); a turned 5 does not catch.
		bool fiveNear = false;
		const int row = pos / 5, col = pos % 5;
		for (int dr = -1; dr <= 1 && !fiveNear; ++dr)
			for (int dc = -1; dc <= 1; ++dc)
			{
				if (!dr && !dc)
					continue;
				const int r = row + dr, c = col + dc;
				if (r < 0 || r > 4 || c < 0 || c > 4)
					continue;
				const TCard& n = g.field[r * 5 + c];
				if (n.index == 5 && !n.exposed)
				{
					fiveNear = true;
					break;
				}
			}

		DWORD points = 0;
		bool destroy = false, keep = false;
		if (hand == CARD_KING)
		{
			keep = value == CARD_KING;
			points = keep ? KING_POINTS : 0;
			destroy = true;
		}
		else if (hand == 5 && fiveNear)
		{
			// Caught: no points, the card is spent; the field card is turned
			// if the 5 could have taken it (Owsap).
			destroy = true;
			keep = hand >= value;
		}
		else if (hand < value)
			destroy = true;
		else if (hand == value)
		{
			points = value * 10;
			destroy = keep = true;
		}
		else
		{
			points = value * 10;
			keep = true;
		}

		if (keep)
			g.field[pos].exposed = true;

		bool fullRow = keep, fullCol = keep;
		for (int c = 0; c < 5 && fullRow; ++c)
			if (!g.field[row * 5 + c].exposed)
				fullRow = false;
		for (int r = 0; r < 5 && fullCol; ++r)
			if (!g.field[r * 5 + col].exposed)
				fullCol = false;
		if (fullRow)
			points += LINE_POINTS;
		if (fullCol)
			points += LINE_POINTS;
		g.score += points;

		const bool theEnd = hand == CARD_KING;
		if (destroy)
			g.hand = 0;
		if (theEnd)
			g.over = true;

		TPacketGCMiniGameCatchKingResult r;
		r.dwPoints = g.score;
		r.bRowType = (BYTE)((fullRow ? 1 : 0) | (fullCol ? 2 : 0));
		r.bCardPos = pos;
		r.bCardValue = value;
		r.bKeepFieldCard = keep;
		r.bDestroyHandCard = destroy;
		r.bGetReward = theEnd && g.score >= REWARD_MIN_SCORE;
		r.bIsFiveNearBy = hand == 5 && fiveNear;
		Send(ch, GC_RESULT_FIELD, &r, sizeof(r));

		if (theEnd)
		{
			for (int i = 0; i < FIELD_CARDS; ++i)
			{
				if (g.field[i].exposed)
					continue;
				TPacketGCMiniGameCatchKingSetEndCard e;
				e.bCardPos = (BYTE)i;
				e.bCardValue = g.field[i].index;
				Send(ch, GC_SET_END_CARD, &e, sizeof(e));
			}
		}
	}

	void GetReward(LPCHARACTER ch)
	{
		std::map<DWORD, TGame>::iterator it = s_games.find(ch->GetPlayerID());
		if (it == s_games.end())
			return;
		const TGame& g = it->second;
		// Every card played: the King is the deck's last, and it ends the game.
		if (g.hand || g.handLeft || !g.over)
			return;
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(VNUM_LOOT[0]);
		if (LootTier(g.score) >= 0 && proto && ch->GetEmptyInventory(proto->bSize) < 0)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "[Zlap Krola] Zrob miejsce w ekwipunku, zeby odebrac nagrode.");
			return;
		}
		const BYTE code = Finish(ch, false);
		Send(ch, GC_REWARD, &code, sizeof(code));
	}

	// ------------------------------------------------------------ the leader

	// The season flag: set when the event starts, cleared a minute after the
	// event and its reward window are both over (the reward flag is written a
	// moment after the end, by the event manager).
	void Lead(DWORD dwNow)
	{
		const bool active = InGameEventIsActive(EVENT_KEY);
		const long now = (long)time(NULL);
		const bool rewardOpen = (long)InGameEventRewardEndTime(EVENT_KEY) > now;
		const DWORD season = Season();
		if (!s_bLeadInit)
		{
			// A restart inside an event keeps its season.
			s_bLeadInit = true;
			s_bWasActive = active;
		}
		if (active)
		{
			s_dwClearSince = 0;
			if (!season || !s_bWasActive)
			{
				// Asked once; the DB core's answer takes a moment to come round.
				if ((!season || (DWORD)now > season) && (!s_lSeasonAskedAt || now - s_lSeasonAskedAt >= REQUEST_HOLD_SECONDS))
				{
					sys_log(0, "CATCH_KING: a new season (was %u)", season);
					s_lSeasonAskedAt = now;
					Request(FLAG_SEASON, (int)now);
				}
			}
		}
		else if (season && !rewardOpen)
		{
			if (!s_dwClearSince)
				s_dwClearSince = dwNow;
			else if (dwNow - s_dwClearSince >= SEASON_CLEAR_HOLD_MS)
				Request(FLAG_SEASON, 0);
		}
		else
			s_dwClearSince = 0;
		s_bWasActive = active;
	}
}

// ---------------------------------------------------------------- the hooks

// input_main.cpp, HEADER_CG_MINI_GAME_CATCH_KING (a fixed 3 bytes,
// packet_info.cpp): nothing extra to read.
int CatchKingProcess(LPCHARACTER ch, const char* data, size_t len)
{
	using namespace mt2009_catchking;
	if (len < sizeof(TPacketCGMiniGameCatchKing))
		return -1;
	if (!Eligible(ch))
		return 0;
	const TPacketCGMiniGameCatchKing* p = reinterpret_cast<const TPacketCGMiniGameCatchKing*>(data);
	s_capable.insert(ch->GetPlayerID());
	switch (p->bSubHeader)
	{
		case CG_START:
			StartGame(ch, p->bSubArgument);
			break;
		case CG_CLICK_HAND:
			DeckCardClick(ch);
			break;
		case CG_CLICK_CARD:
			FieldCardClick(ch, p->bSubArgument);
			break;
		case CG_REWARD:
			GetReward(ch);
			break;
		case CG_REQUEST_QUEST_FLAG:
			SyncSeason(ch);
			DeliverPending(ch);
			SendQuestFlag(ch, GC_SET_QUEST_FLAG);
			break;
		default:
			sys_err("CATCH_KING: unknown sub-header %u from %s", (unsigned int)p->bSubHeader, ch->GetName());
			break;
	}
	return 0;
}

// item_manager.cpp, CreateQuestDropItem: a card for a share of every kill.
void CatchKingOnKill(LPCHARACTER victim, LPCHARACTER killer, int iDeltaPercent, int iRandRange)
{
	using namespace mt2009_catchking;
	if (!victim || !(victim->IsMonster() || victim->IsStone()) || !killer || !killer->IsPC() || !killer->GetDesc())
		return;
	// MT2009_PLUS_BOT_MINIGAMES_V1: a bot's kill rolls the same card, which
	// goes to the bot's own count and makes a real Talia Krolewska
	// (playerbot_minigames.h); a player's goes to the quest flags.
	const bool bot = killer->GetDesc()->IsBot();
	if (!bot && !mt2009_catchking::Eligible(killer))
		return;
	if (!InGameEventIsActive(EVENT_KEY))
		return;
	if (GetDropPerKillPct(50, 100, iDeltaPercent, FLAG_DROP) >= number(1, iRandRange))
	{
		if (bot)
			PlayerBotMinigameCard(killer, 0);
		else
			AddPiece(killer, false);
	}
}

// char.cpp, CHARACTER::Disconnect (logout, warp, channel change): a game in
// progress ends with its points; its Loot waits for the next login.
void CatchKingOnDisconnect(LPCHARACTER ch)
{
	using namespace mt2009_catchking;
	if (!ch || !ch->IsPC())
		return;
	const DWORD pid = ch->GetPlayerID();
	if (s_games.count(pid))
		Finish(ch, true);
	s_capable.erase(pid);
	s_noMoreGainAt.erase(pid);
}

// playerbot_events.h, every pass.
void CatchKingTick(DWORD dwNow)
{
	using namespace mt2009_catchking;
	if (s_dwNextSecond && (int)(dwNow - s_dwNextSecond) < 0)
		return;
	s_dwNextSecond = dwNow + 1000;
	if (!s_dwBootAt)
		s_dwBootAt = dwNow;
	if (dwNow - s_dwBootAt >= BOOT_DELAY_MS && IsPlayerBotEventLeader())
		Lead(dwNow);
}

// ---------------------------------------------------------------- quest API
// (questlua_game.cpp, MT2009_PLUS_CATCH_KING_V1 (lua); minigame_catchking.quest)

// game.get_catchking_score(total): the season's top ten, by total or best score.
int CatchKingRanking(bool total, std::vector<std::string>& names, std::vector<int>& empires, std::vector<DWORD>& scores)
{
	using namespace mt2009_catchking;
	names.clear();
	empires.clear();
	scores.clear();
	const DWORD season = Season();
	if (!season)
		return 0;
	std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
			"SELECT name, empire, %s FROM player.minigame_catchking WHERE season = %u AND games > 0 "
			"ORDER BY %s DESC, max_score DESC, pid ASC LIMIT 10",
			total ? "total_score" : "max_score", season, total ? "total_score" : "max_score"));
	SQLResult* res = msg.get() ? msg->Get() : NULL;
	if (!res || !res->pSQLResult)
		return 0;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(res->pSQLResult)))
	{
		int empire = 0;
		DWORD score = 0;
		str_to_number(empire, row[1]);
		str_to_number(score, row[2]);
		names.push_back(row[0] ? row[0] : "");
		empires.push_back(empire);
		scores.push_back(score);
	}
	return (int)names.size();
}

// game.get_catchking_myscore(total)
DWORD CatchKingMyScore(LPCHARACTER ch, bool total)
{
	using namespace mt2009_catchking;
	const DWORD season = Season();
	if (!ch || !season)
		return 0;
	std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
			"SELECT %s FROM player.minigame_catchking WHERE season = %u AND pid = %u",
			total ? "total_score" : "max_score", season, ch->GetPlayerID()));
	SQLResult* res = msg.get() ? msg->Get() : NULL;
	if (!res || !res->pSQLResult || !res->uiNumRows)
		return 0;
	MYSQL_ROW row = mysql_fetch_row(res->pSQLResult);
	DWORD score = 0;
	if (row && row[0])
		str_to_number(score, row[0]);
	return score;
}

// game.catchking_claim_reward(): the top-10 prize of the season that ended.
//   > 0  the Golden Loots given
//     0  not in the top ten
//    -1  no reward window now
//    -2  already taken
//    -3  the event still runs
//    -4  no room in the bag
int CatchKingClaimReward(LPCHARACTER ch)
{
	using namespace mt2009_catchking;
	if (!Eligible(ch))
		return -1;
	if (InGameEventIsActive(EVENT_KEY))
		return -3;
	const DWORD season = Season();
	if (!season || (long)InGameEventRewardEndTime(EVENT_KEY) <= (long)time(NULL))
		return -1;
	std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
			"SELECT pid, claimed FROM player.minigame_catchking WHERE season = %u AND games > 0 AND total_score >= %d "
			"ORDER BY total_score DESC, max_score DESC, pid ASC LIMIT 10", season, (int)REWARD_MIN_SCORE));
	SQLResult* res = msg.get() ? msg->Get() : NULL;
	if (!res || !res->pSQLResult)
		return -1;
	int rank = -1, i = 0;
	bool claimed = false;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(res->pSQLResult)))
	{
		DWORD pid = 0;
		int c = 0;
		str_to_number(pid, row[0]);
		str_to_number(c, row[1]);
		if (pid == ch->GetPlayerID())
		{
			rank = i;
			claimed = c != 0;
		}
		++i;
	}
	if (rank < 0)
		return 0;
	if (claimed)
		return -2;
	const TItemTable* proto = ITEM_MANAGER::instance().GetTable(VNUM_LOOT[0]);
	if (!proto)
		return -1;
	if (ch->GetEmptyInventory(proto->bSize) < 0)
		return -4;
	std::unique_ptr<SQLMsg> upd(DBManager::instance().DirectQuery(
			"UPDATE player.minigame_catchking SET claimed = 1 WHERE season = %u AND pid = %u AND claimed = 0",
			season, ch->GetPlayerID()));
	if (!upd.get() || !upd->Get() || upd->Get()->uiAffectedRows != 1)
		return -2;
	const int count = TOP_PRIZE[rank];
	ch->AutoGiveItem(VNUM_LOOT[0], count);
	sys_log(0, "CATCH_KING: %s (pid %u) takes the top-10 prize: rank %d, %u x%d (season %u)",
			ch->GetName(), ch->GetPlayerID(), rank + 1, VNUM_LOOT[0], count, season);
	return count;
}

// game.catchking_use_item(vnum): the King Card (+1 card) or the King Deck
// (+1 deck) used from the bag; true when it counted (the quest takes the item).
bool CatchKingUseItem(LPCHARACTER ch, DWORD vnum)
{
	using namespace mt2009_catchking;
	if (!Eligible(ch))
		return false;
	if (!InGameEventIsActive(EVENT_KEY))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "[Zlap Krola] Tego mozna uzyc tylko w czasie eventu.");
		return false;
	}
	if (vnum == VNUM_PIECE)
		return AddPiece(ch, true);
	if (vnum == VNUM_PACK)
		return AddPack(ch);
	return false;
}

// game.catchking_deliver(): at login, the Loots of a game left mid-way.
void CatchKingDeliver(LPCHARACTER ch)
{
	using namespace mt2009_catchking;
	if (!Eligible(ch))
		return;
	SyncSeason(ch);
	DeliverPending(ch);
}
