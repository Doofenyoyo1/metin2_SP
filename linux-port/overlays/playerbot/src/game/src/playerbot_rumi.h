// MT2009 PLUS Rumi (Okey card game) - MT2009_PLUS_RUMI_V1
// (the operator, 30 September: Owsap's mini games in full, with real packets
// in a new exe; Rumi after Catch the King).
//
// Owsap's CMiniGameRumi (minigame_rumi.cpp, v6.2.6, with __OKEY_EVENT_FLAG_RENEWAL__
// and __RUMI_DEALER__) as one overlay header. The packets are Owsap's, byte for
// byte (packet.h, server-patches/rumi): CG 181 {header, sub, BOOL use, BYTE index}
// (7 B), GC 181 {header, WORD size, sub} + the sub's body. The engine calls in
// here from five places (server-patches/rumi/edits.json):
//
//   RumiPacket       input_main.cpp, CInputMain::Analyze, HEADER_CG_MINI_GAME_RUMI
//   RumiDisconnect   char.cpp, CHARACTER::Disconnect (logout, warp, channel change)
//   RumiUseItem      char_item.cpp, UseItemEx (79505 card, 79506 card set)
//   RumiOnKill       item_manager.cpp, CreateQuestDropItem (a card per kill share)
//   RumiLua*         questlua_game.cpp, game.get_minigame_rumi_score & co.
//
// and playerbot_events.h ticks RumiTick once a second, after the event manager
// (playerbot_ingame_events.h, whose InGameEventIsActive("rumi") /
// ("rumi_xmas") say whether the event runs, and whose leader spawns the table
// NPC 20417 and opens the 7-day reward window when it ends).
//
// The game: 24 cards (red, blue, yellow 1-8), a hand of five, a field of
// three. A click on the deck deals the hand full (Owsap's __RUMI_DEALER__); a
// left click on a hand card puts it on the field, the third card scores it
// (three of a number: number*10+10; a run: lowest*10, +40 one colour) or sends
// all three back; a right click discards a hand card. The game ends when the
// player says so: a chest by the score - 400+ gold, 300+ silver, less bronze
// ("normal" chests while the scheduler's "rumi" runs, Owsap's "Merry" ones for
// the Christmas flag "rumi_xmas"); every game adds to the player's season
// score (player.mt2009_rumi_score), the top ten of the season take their
// prize at the table during the reward window (minigame_rumi.quest).
// A game costs 30 000 yang and one card set (minigame_rumi.card_count, made of
// 24 cards, minigame_rumi.card_piece_count, which drop per kill while the
// event runs).
//
// What is fixed against Owsap:
//  - Owsap's server trusted its own state, but let a draw happen with cards on
//    the field: hand and field could then hold more than five, the failed
//    field went back only partly and cards were lost. A draw now needs an
//    empty field (the client's own rule: its deck only flashes then), so the
//    three always fit back.
//  - Logout, warp or channel change in a game deleted it: the yang and the
//    card set were gone, the score never counted. It is settled now - the
//    score counts, the chest waits as a quest flag and comes at the next login
//    (or the next open of the window).
//  - The chest went out before the game was forgotten (Reward() gave, then
//    reset): the game is taken out of the list first, so nothing can pay twice.
//  - The ranking never reset (one table for ever) and the top-ten prize was
//    once per 7 days, not once per event: scores are per season (an event and
//    its reward window, ids kept in event flags by the leading core) and the
//    prize once per season, checked and marked here before the item is given.
//  - Synchronous score writes on every game end: an asynchronous query now.
//  - GC 181 went to every client, also one whose exe does not know it (the
//    card counter after a kill): an old exe would stop at the unknown header.
//    A client gets the packet only after it sent a CG 181 itself (the window's
//    REQUEST_QUEST_FLAG when the game window starts); before that, chat lines.
//  - Bots never rank and get no packets; they collect cards from their kills
//    and "play" a set without a table (MT2009_PLUS_BOT_MINIGAMES_V1,
//    playerbot_minigames.h).
#include "packet.h"
#include <random>

int GetDropPerKillPct(int iMinimum, int iDefault, int iDeltaPercent, const char* c_pszFlag);
bool InGameEventIsActive(const char* key);
DWORD InGameEventRewardEndTime(const char* key);

// MT2009_PLUS_BOT_MINIGAMES_V1: a bot's card (playerbot_minigames.h, later in the unit).
namespace
{
	void PlayerBotMinigameCard(LPCHARACTER ch, int game);
}

namespace mt2009_rumi
{
	enum
	{
		POS_NONE = 0,
		POS_DECK = 1,
		POS_HAND = 2,
		POS_FIELD = 3,

		HAND_MAX = 5,
		FIELD_MAX = 3,

		COLOR_RED = 10,
		COLOR_BLUE = 20,
		COLOR_YELLOW = 30,
		NUMBER_END = 8,
		DECK_MAX = 3 * NUMBER_END,

		START_GOLD = 30000,
		CARD_PIECE_MAX = DECK_MAX,	// cards per card set
		CARD_COUNT_MAX = 999,		// card sets kept

		SCORE_LOW = 300,
		SCORE_MID = 400,

		ITEM_CARD_PIECE = 79505,	// Karta Okey
		ITEM_CARD_PACK = 79506,		// Zestaw kart Okey

		REWARD_NORMAL_HIGH = 50275,	// Zlota Skrzynia Okey
		REWARD_NORMAL_MID = 50276,
		REWARD_NORMAL_LOW = 50277,
		REWARD_XMAS_HIGH = 50267,	// Swiateczna Zlota Skrzynia Okey
		REWARD_XMAS_MID = 50268,
		REWARD_XMAS_LOW = 50269,

		TABLE_NPC = 20417,
	};

	// The top ten's prize, Owsap's RUMI_RANK*_REWARD_COUNT: gold chests.
	const int RANK_PRIZE[10] = { 10, 5, 3, 1, 1, 1, 1, 1, 1, 1 };

	const char* const F_PIECES = "minigame_rumi.card_piece_count";
	const char* const F_CARDS = "minigame_rumi.card_count";
	const char* const F_CLAIMED = "minigame_rumi.claimed";		// the season whose prize was taken
	const char* const F_PENDING = "minigame_rumi.pending_";		// + chest vnum: chests owed

	// The season, written by the leading core only (event flags).
	const char* const S_ID = "mini_game_okey_season";			// epoch the season began
	const char* const S_STATE = "mini_game_okey_season_state";	// 0 none, 1 the event runs, 2 the reward window
	const char* const S_KIND = "mini_game_okey_season_kind";	// 1 normal chests, 2 Christmas chests

	const DWORD REWARDS[6] = { REWARD_NORMAL_HIGH, REWARD_NORMAL_MID, REWARD_NORMAL_LOW,
			REWARD_XMAS_HIGH, REWARD_XMAS_MID, REWARD_XMAS_LOW };

	struct Card
	{
		BYTE color;
		BYTE number;	// 0 = no card
	};

	struct Game
	{
		std::vector<Card> deck;
		Card hand[HAND_MAX];
		Card field[FIELD_MAX];
		WORD score;
		bool normal;
		DWORD startedAt;
		int combos;
	};

	std::map<DWORD, Game> s_games;		// pid -> the game
	std::set<DWORD> s_capable;			// pids whose exe sent CG 181 on this core
	DWORD s_dwNextSweep = 0;

	std::mt19937& Rng()
	{
		static std::mt19937 rng(std::random_device{}());
		return rng;
	}

	bool Eligible(LPCHARACTER ch)
	{
		return ch && ch->IsPC() && ch->GetDesc() && !ch->GetDesc()->IsBot();
	}

	bool EventOn()
	{
		return InGameEventIsActive("rumi") || InGameEventIsActive("rumi_xmas");
	}

	bool EventNormal()
	{
		return InGameEventIsActive("rumi") || !InGameEventIsActive("rumi_xmas");
	}

	DWORD RewardEnd()
	{
		return std::max(InGameEventRewardEndTime("rumi"), InGameEventRewardEndTime("rumi_xmas"));
	}

	int EventFlag(const char* name)
	{
		return quest::CQuestManager::instance().GetEventFlag(name);
	}

	int Flag(LPCHARACTER ch, const char* name)
	{
		const int v = ch->GetQuestFlag(name);
		return v < 0 ? 0 : v;
	}

	void Info(LPCHARACTER ch, const char* text)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "%s", text);
	}

	// ------------------------------------------------------------ the packets

	void Send(LPCHARACTER ch, BYTE sub, const void* body, size_t len)
	{
		if (!ch || !ch->GetDesc())
			return;
		TPacketGCMiniGameRumi head;
		head.bHeader = HEADER_GC_MINI_GAME_RUMI;
		head.bSubHeader = sub;
		head.wSize = (WORD)(sizeof(head) + len);
		std::vector<BYTE> buf(head.wSize);
		memcpy(&buf[0], &head, sizeof(head));
		if (len)
			memcpy(&buf[sizeof(head)], body, len);
		ch->GetDesc()->Packet(&buf[0], (int)buf.size());
	}

	void SendMove(LPCHARACTER ch, BYTE srcPos, BYTE srcIndex, const Card& src, BYTE dstPos, BYTE dstIndex, const Card& dst)
	{
		TPacketGCMiniGameRumiMoveCard p;
		memset(&p, 0, sizeof(p));
		p.bSrcPos = srcPos;
		p.bSrcIndex = srcIndex;
		p.bSrcColor = src.color;
		p.bSrcNumber = src.number;
		p.bDstPos = dstPos;
		p.bDstIndex = dstIndex;
		p.bDstColor = dst.color;
		p.bDstNumber = dst.number;
		Send(ch, RUMI_GC_SUBHEADER_MOVE_CARD, &p, sizeof(p));
	}

	void SendDeck(LPCHARACTER ch, const Game& g)
	{
		TPacketGCMiniGameRumiSetDeck p;
		p.bDeckCount = (BYTE)g.deck.size();
		Send(ch, RUMI_GC_SUBHEADER_SET_DECK, &p, sizeof(p));
	}

	// The card counters: a packet to an exe that knows it, a chat line to
	// another (only when something changed).
	void SendFlags(LPCHARACTER ch, BYTE sub)
	{
		const int pieces = Flag(ch, F_PIECES);
		const int cards = Flag(ch, F_CARDS);
		if (s_capable.count(ch->GetPlayerID()))
		{
			TPacketGCMiniGameRumiQuestFlag p;
			p.wCardPieceCount = (WORD)pieces;
			p.wCardCount = (WORD)cards;
			Send(ch, sub, &p, sizeof(p));
			return;
		}
		if (sub == RUMI_GC_SUBHEADER_SET_CARD_PIECE_FLAG)
			ch->ChatPacket(CHAT_TYPE_INFO, "Otrzymujesz kart\xea Okey (%d/%d).", pieces, (int)CARD_PIECE_MAX);
		else if (sub == RUMI_GC_SUBHEADER_SET_CARD_FLAG)
			ch->ChatPacket(CHAT_TYPE_INFO, "Otrzymujesz zestaw kart Okey (masz %d).", cards);
		else if (sub == RUMI_GC_SUBHEADER_NO_MORE_GAIN)
			Info(ch, "Nie mo\xbf" "esz otrzyma\xe6 kolejnych zestaw\xf3w kart Okey.");
	}

	// ------------------------------------------------------------ the cards

	bool AddPiece(LPCHARACTER ch)
	{
		const int pieces = Flag(ch, F_PIECES);
		const int cards = Flag(ch, F_CARDS);
		if (cards >= CARD_COUNT_MAX)
		{
			SendFlags(ch, RUMI_GC_SUBHEADER_NO_MORE_GAIN);
			return false;
		}
		if (pieces + 1 >= CARD_PIECE_MAX)
		{
			ch->SetQuestFlag(F_PIECES, 0);
			ch->SetQuestFlag(F_CARDS, cards + 1);
			SendFlags(ch, RUMI_GC_SUBHEADER_SET_CARD_FLAG);
		}
		else
		{
			ch->SetQuestFlag(F_PIECES, pieces + 1);
			SendFlags(ch, RUMI_GC_SUBHEADER_SET_CARD_PIECE_FLAG);
		}
		return true;
	}

	// ------------------------------------------------------------ the seasons

	DWORD SeasonId()
	{
		const int v = EventFlag(S_ID);
		return v > 0 ? (DWORD)v : 0;
	}

	bool SeasonNormal()
	{
		return EventFlag(S_KIND) != 2;
	}

	DWORD ChestFor(WORD score, bool normal)
	{
		if (score >= SCORE_MID)
			return normal ? REWARD_NORMAL_HIGH : REWARD_XMAS_HIGH;
		if (score >= SCORE_LOW)
			return normal ? REWARD_NORMAL_MID : REWARD_XMAS_MID;
		return normal ? REWARD_NORMAL_LOW : REWARD_XMAS_LOW;
	}

	void SaveScore(DWORD pid, WORD score)
	{
		const DWORD season = SeasonId();
		DBManager::instance().Query(
				"INSERT INTO player.mt2009_rumi_score (pid, season, best_score, total_score, games, last_play) "
				"VALUES (%u, %u, %u, %u, 1, NOW()) ON DUPLICATE KEY UPDATE "
				"total_score = total_score + %u, best_score = GREATEST(best_score, %u), games = games + 1, last_play = NOW()",
				pid, season, (unsigned int)score, (unsigned int)score, (unsigned int)score, (unsigned int)score);
	}

	// The leading core keeps the season's flags (Owsap had none): a new
	// season when an event starts after the last one's reward window began
	// (or after nothing), the reward state when it ends, none after the window.
	void LeadSeason(long now)
	{
		using namespace mt2009_ingame_event;
		const bool on = EventOn();
		const bool reward = RewardEnd() > (DWORD)now;
		const int state = EventFlag(S_STATE);
		if (on)
		{
			if (state != 1)
			{
				Request(S_ID, (int)now, now);
				Request(S_KIND, EventNormal() ? 1 : 2, now);
				Request(S_STATE, 1, now);
				sys_log(0, "RUMI: season %ld begins (%s)", now, EventNormal() ? "normal" : "xmas");
			}
		}
		else if (reward)
		{
			if (state == 1)
			{
				Request(S_STATE, 2, now);
				sys_log(0, "RUMI: season %d - reward window until %u", EventFlag(S_ID), (unsigned int)RewardEnd());
			}
		}
		else if (state != 0)
		{
			Request(S_STATE, 0, now);
			sys_log(0, "RUMI: season %d over", EventFlag(S_ID));
		}
	}

	// ------------------------------------------------------------ the game

	int EmptyHand(const Game& g)
	{
		for (int i = 0; i < HAND_MAX; ++i)
			if (!g.hand[i].number)
				return i;
		return -1;
	}

	int EmptyField(const Game& g)
	{
		for (int i = 0; i < FIELD_MAX; ++i)
			if (!g.field[i].number)
				return i;
		return -1;
	}

	int FieldCount(const Game& g)
	{
		int n = 0;
		for (int i = 0; i < FIELD_MAX; ++i)
			if (g.field[i].number)
				++n;
		return n;
	}

	// Owsap's __CheckCombination: three of a number, or three in a row
	// (+40 in one colour). 0 = no combination.
	WORD Combination(const Game& g)
	{
		BYTE n[FIELD_MAX], c[FIELD_MAX];
		for (int i = 0; i < FIELD_MAX; ++i)
		{
			if (!g.field[i].number)
				return 0;
			n[i] = g.field[i].number;
			c[i] = g.field[i].color;
		}
		if (n[0] == n[1] && n[1] == n[2])
			return (WORD)(n[0] * 10 + 10);
		std::sort(n, n + FIELD_MAX);
		if (n[1] != n[0] + 1 || n[2] != n[1] + 1)
			return 0;
		WORD score = (WORD)(n[0] * 10);
		if (c[0] == c[1] && c[1] == c[2])
			score += 40;
		return score;
	}

	void Draw(LPCHARACTER ch, Game& g)
	{
		if (g.deck.empty())
		{
			Info(ch, "Nie mo\xbf" "esz dobra\xe6 wi\xea" "cej kart.");
			return;
		}
		if (FieldCount(g))
		{
			Info(ch, "Nie mo\xbf" "esz dobiera\xe6 kart, gdy le\xbf\xb9 ju\xbf karty na stole.");
			return;
		}
		// Owsap's dealer: the hand is dealt full.
		int pos;
		while (!g.deck.empty() && (pos = EmptyHand(g)) >= 0)
		{
			const Card card = g.deck.back();
			g.deck.pop_back();
			g.hand[pos] = card;
			const Card none = Card();
			SendMove(ch, POS_DECK, 0, none, POS_HAND, (BYTE)pos, card);
		}
	}

	void FieldToHand(LPCHARACTER ch, Game& g, int index)
	{
		if (index < 0 || index >= FIELD_MAX || !g.field[index].number)
			return;
		const int pos = EmptyHand(g);
		if (pos < 0)
			return;	// cannot happen: hand and field hold five at most
		const Card card = g.field[index];
		g.hand[pos] = card;
		g.field[index] = Card();
		SendMove(ch, POS_FIELD, (BYTE)index, card, POS_HAND, (BYTE)pos, card);
	}

	void HandClick(LPCHARACTER ch, Game& g, bool use, int index)
	{
		if (index < 0 || index >= HAND_MAX || !g.hand[index].number)
			return;
		const Card card = g.hand[index];
		if (!use)
		{
			g.hand[index] = Card();
			SendMove(ch, POS_HAND, (BYTE)index, card, POS_NONE, 0, Card());
			return;
		}
		const int pos = EmptyField(g);
		if (pos < 0)
			return;
		g.field[pos] = card;
		g.hand[index] = Card();
		SendMove(ch, POS_HAND, (BYTE)index, card, POS_FIELD, (BYTE)pos, card);
		if (FieldCount(g) < FIELD_MAX)
			return;
		const WORD score = Combination(g);
		if (score)
		{
			for (int i = 0; i < FIELD_MAX; ++i)
				g.field[i] = Card();
			g.score = (WORD)std::min(65535, g.score + score);
			++g.combos;
			TPacketGCMiniGameRumiSetScore p;
			p.wScore = score;
			p.wTotalScore = g.score;
			Send(ch, RUMI_GC_SUBHEADER_SET_SCORE, &p, sizeof(p));
			return;
		}
		for (int i = 0; i < FIELD_MAX; ++i)
			FieldToHand(ch, g, i);
	}

	// ------------------------------------------------------------ owed chests

	std::string PendingFlag(DWORD vnum)
	{
		char name[64];
		snprintf(name, sizeof(name), "%s%u", F_PENDING, (unsigned int)vnum);
		return name;
	}

	// The chests a game left behind (a logout in it): as many as fit.
	int GivePending(LPCHARACTER ch)
	{
		int given = 0;
		for (int i = 0; i < 6; ++i)
		{
			const std::string flag = PendingFlag(REWARDS[i]);
			int owed = ch->GetQuestFlag(flag);
			if (owed <= 0)
				continue;
			const TItemTable* t = ITEM_MANAGER::instance().GetTable(REWARDS[i]);
			if (!t)
				continue;
			while (owed > 0 && ch->GetEmptyInventory(t->bSize) >= 0)
			{
				// The flag first: a chest can never come twice.
				ch->SetQuestFlag(flag, --owed);
				ch->AutoGiveItem(REWARDS[i], 1);
				++given;
			}
			if (owed > 0)
			{
				Info(ch, "Zr\xf3\xbf" "nij miejsce w ekwipunku, aby odebra\xe6 skrzyni\xea z gry w Okey.");
				break;
			}
		}
		if (given)
			sys_log(0, "RUMI: %s took %d owed chest(s)", ch->GetName(), given);
		return given;
	}

	// The end of a game. A chest by the score, the score into the season.
	// logout: the chest is owed (a quest flag) - no item is made while the
	// character leaves. Otherwise it needs room, or the game goes on.
	bool Settle(LPCHARACTER ch, bool logout)
	{
		std::map<DWORD, Game>::iterator it = s_games.find(ch->GetPlayerID());
		if (it == s_games.end())
			return false;
		const DWORD vnum = ChestFor(it->second.score, it->second.normal);
		const TItemTable* t = ITEM_MANAGER::instance().GetTable(vnum);
		if (!logout && t && ch->GetEmptyInventory(t->bSize) < 0)
		{
			Info(ch, "Nagrod\xea otrzymasz dopiero po zrobieniu miejsca w ekwipunku.");
			return false;
		}
		const WORD score = it->second.score;
		const int combos = it->second.combos;
		// Out of the list before anything is given.
		s_games.erase(it);
		SaveScore(ch->GetPlayerID(), score);
		if (!t)
			sys_err("RUMI: no item_proto row for the chest %u", vnum);
		else if (logout)
		{
			const std::string flag = PendingFlag(vnum);
			ch->SetQuestFlag(flag, std::max(0, ch->GetQuestFlag(flag)) + 1);
		}
		else
			ch->AutoGiveItem(vnum, 1);
		sys_log(0, "RUMI: %s ends a game%s - score %u, %d combination(s), chest %u",
				ch->GetName(), logout ? " by leaving" : "", (unsigned int)score, combos, vnum);
		if (!logout)
			Send(ch, RUMI_GC_SUBHEADER_END, NULL, 0);
		return true;
	}

	void Start(LPCHARACTER ch)
	{
		const DWORD pid = ch->GetPlayerID();
		if (!EventOn())
		{
			Info(ch, "Event Okey w\xb3" "a\x9c" "nie si\xea nie odbywa.");
			Send(ch, RUMI_GC_SUBHEADER_END, NULL, 0);
			return;
		}
		if (s_games.count(pid))
			return;
		if (ch->GetExchange() || ch->GetMyShop() || ch->GetShopOwner() || ch->IsOpenSafebox() || ch->IsCubeOpen())
		{
			Info(ch, "Nie mo\xbf" "esz zagra\xe6 w Okey, dop\xf3ki masz otwarte inne okno.");
			return;
		}
		const int cards = Flag(ch, F_CARDS);
		if (cards <= 0 || ch->GetGold() < START_GOLD)
		{
			Info(ch, "Masz za ma\xb3o przedmiot\xf3w lub pieni\xea" "dzy, aby gra\xe6.");
			return;
		}
		ch->SetQuestFlag(F_CARDS, cards - 1);
		PlayerBotChangeGold(ch, -(long long)START_GOLD);

		Game& g = s_games[pid];
		g = Game();
		g.normal = EventNormal();
		g.startedAt = get_global_time();
		for (BYTE color = COLOR_RED; color <= COLOR_YELLOW; color += 10)
			for (BYTE number = 1; number <= NUMBER_END; ++number)
			{
				Card card;
				card.color = color;
				card.number = number;
				g.deck.push_back(card);
			}
		std::shuffle(g.deck.begin(), g.deck.end(), Rng());

		Send(ch, RUMI_GC_SUBHEADER_START, NULL, 0);
		SendDeck(ch, g);
		Info(ch, "Rozpoczynasz gr\xea w Okey!");
		sys_log(0, "RUMI: %s starts a game (%s chests, %d card set(s) left)", ch->GetName(), g.normal ? "normal" : "xmas", cards - 1);
	}
}

// ---------------------------------------------------------------- the engine's

// HEADER_CG_MINI_GAME_RUMI (input_main.cpp). Owsap's MiniGameRumi + Analyze.
void RumiPacket(LPCHARACTER ch, const char* data)
{
	using namespace mt2009_rumi;
	if (!Eligible(ch) || !data)
		return;
	const TPacketCGMiniGameRumi* p = reinterpret_cast<const TPacketCGMiniGameRumi*>(data);
	const DWORD pid = ch->GetPlayerID();
	s_capable.insert(pid);
	switch (p->bSubHeader)
	{
		case RUMI_CG_SUBHEADER_START:
			GivePending(ch);
			Start(ch);
			return;

		case RUMI_CG_SUBHEADER_END:
			if (!Settle(ch, false) && !s_games.count(pid))
				Send(ch, RUMI_GC_SUBHEADER_END, NULL, 0);
			return;

		case RUMI_CG_SUBHEADER_REQUEST_QUEST_FLAG:
			GivePending(ch);
			SendFlags(ch, RUMI_GC_SUBHEADER_SET_QUEST_FLAG);
			return;

		default:
			break;
	}
	std::map<DWORD, Game>::iterator it = s_games.find(pid);
	if (it == s_games.end())
		return;
	Game& g = it->second;
	switch (p->bSubHeader)
	{
		case RUMI_CG_SUBHEADER_DECK_CARD_CLICK:
			Draw(ch, g);
			break;
		case RUMI_CG_SUBHEADER_HAND_CARD_CLICK:
			HandClick(ch, g, p->bUseCard != 0, p->bIndex);
			break;
		case RUMI_CG_SUBHEADER_FIELD_CARD_CLICK:
			FieldToHand(ch, g, p->bIndex);
			break;
		default:
			sys_err("RUMI: %s sent the unknown sub-header %u", ch->GetName(), (unsigned int)p->bSubHeader);
			break;
	}
}

// CHARACTER::Disconnect: a game left behind is settled (the chest owed).
void RumiDisconnect(LPCHARACTER ch)
{
	using namespace mt2009_rumi;
	if (!ch || !ch->IsPC())
		return;
	if (s_games.count(ch->GetPlayerID()))
		Settle(ch, true);
	s_capable.erase(ch->GetPlayerID());
}

// UseItemEx: the card (a card towards a set, while the event runs) and the
// card set (+1 set). true = handled here.
bool RumiUseItem(LPCHARACTER ch, LPITEM item)
{
	using namespace mt2009_rumi;
	if (!ch || !item || !ch->IsPC())
		return false;
	const DWORD vnum = item->GetVnum();
	if (vnum != ITEM_CARD_PIECE && vnum != ITEM_CARD_PACK)
		return false;
	if (!Eligible(ch))
		return true;
	if (vnum == ITEM_CARD_PIECE)
	{
		if (!EventOn())
		{
			Info(ch, "Karty Okey mo\xbf" "na u\xbf" "y\xe6 tylko w czasie eventu Okey.");
			return true;
		}
		if (AddPiece(ch))
			item->SetCount(item->GetCount() - 1);
		return true;
	}
	const int cards = Flag(ch, F_CARDS);
	if (cards >= CARD_COUNT_MAX)
	{
		SendFlags(ch, RUMI_GC_SUBHEADER_NO_MORE_GAIN);
		return true;
	}
	ch->SetQuestFlag(F_CARDS, cards + 1);
	item->SetCount(item->GetCount() - 1);
	SendFlags(ch, RUMI_GC_SUBHEADER_SET_CARD_FLAG);
	return true;
}

// CreateQuestDropItem: Owsap's kill roll, a card instead of an item on the
// ground (__OKEY_EVENT_FLAG_RENEWAL__). Players only.
void RumiOnKill(LPCHARACTER killer, LPCHARACTER victim, int iDeltaPercent, int iRandRange)
{
	using namespace mt2009_rumi;
	if (!killer || !killer->IsPC() || !killer->GetDesc() || !victim || victim->IsPC() ||
			(!victim->IsMonster() && !victim->IsStone()))
		return;
	// MT2009_PLUS_BOT_MINIGAMES_V1: a bot rolls the same card, for its own
	// count and a real Zestaw kart Okey (playerbot_minigames.h).
	const bool bot = killer->GetDesc()->IsBot();
	if (!bot && !Eligible(killer))
		return;
	if (!EventOn())
		return;
	if (GetDropPerKillPct(50, 100, iDeltaPercent, "mini_game_okey_drop") < number(1, iRandRange))
		return;
	if (bot)
		PlayerBotMinigameCard(killer, 1);
	else
		AddPiece(killer);
}

// playerbot_events.h, once a second (after InGameEventTick).
void RumiTick(DWORD dwNow)
{
	using namespace mt2009_rumi;
	if (mt2009_ingame_event::s_dwBootAt && dwNow - mt2009_ingame_event::s_dwBootAt >= mt2009_ingame_event::BOOT_DELAY_MS
			&& IsPlayerBotEventLeader())
		LeadSeason((long)time(NULL));
	// A game whose player is gone without a Disconnect (should not happen).
	if (s_dwNextSweep && (int)(dwNow - s_dwNextSweep) < 0)
		return;
	s_dwNextSweep = dwNow + 60000;
	for (std::map<DWORD, Game>::iterator it = s_games.begin(); it != s_games.end(); )
	{
		LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
		if (ch && ch->GetDesc())
		{
			++it;
			continue;
		}
		sys_err("RUMI: the game of pid %u had no player any more (score %u) - dropped", it->first, (unsigned int)it->second.score);
		s_games.erase(it++);
	}
}

// ---------------------------------------------------------------- the quest's

// questlua_game.cpp declares these inside its namespace quest.
namespace quest
{
// game.get_minigame_rumi_score(total): the season's top ten (name, score).
void RumiLuaScoreTable(bool total, std::vector<std::pair<std::string, DWORD> >& out)
{
	using namespace mt2009_rumi;
	out.clear();
	const char* column = total ? "total_score" : "best_score";
	std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
			"SELECT p.name, s.%s FROM player.mt2009_rumi_score s JOIN player.player p ON p.id = s.pid "
			"WHERE s.season = %u AND s.%s > 0 AND p.name NOT LIKE '[%%' ORDER BY s.%s DESC, s.last_play ASC LIMIT 10",
			column, (unsigned int)SeasonId(), column, column));
	if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
		return;
	MYSQL_ROW row;
	while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
	{
		if (!row[0] || !row[1])
			continue;
		DWORD score = 0;
		str_to_number(score, row[1]);
		out.push_back(std::make_pair(std::string(row[0]), score));
	}
}

// game.get_minigame_rumi_my_score(total)
DWORD RumiLuaMyScore(LPCHARACTER ch, bool total)
{
	using namespace mt2009_rumi;
	if (!ch)
		return 0;
	std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
			"SELECT %s FROM player.mt2009_rumi_score WHERE pid = %u AND season = %u",
			total ? "total_score" : "best_score", ch->GetPlayerID(), (unsigned int)SeasonId()));
	if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
		return 0;
	MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
	DWORD score = 0;
	if (row && row[0])
		str_to_number(score, row[0]);
	return score;
}

// game.minigame_rumi_claim(): the top ten's prize, once a season.
// 0 given (vnum, count), 1 no reward window, 2 taken already, 3 not in the
// top ten, 4 no room, 5 no chest row.
int RumiLuaClaim(LPCHARACTER ch, DWORD& vnum, int& count)
{
	using namespace mt2009_rumi;
	vnum = 0;
	count = 0;
	if (!Eligible(ch))
		return 1;
	const DWORD season = SeasonId();
	if (!season || EventFlag(S_STATE) != 2 || RewardEnd() <= (DWORD)get_global_time())
		return 1;
	if ((DWORD)ch->GetQuestFlag(F_CLAIMED) == season)
		return 2;
	std::vector<std::pair<std::string, DWORD> > top;
	RumiLuaScoreTable(true, top);
	int rank = -1;
	for (size_t i = 0; i < top.size(); ++i)
		if (top[i].first == ch->GetName())
		{
			rank = (int)i;
			break;
		}
	if (rank < 0)
		return 3;
	vnum = SeasonNormal() ? REWARD_NORMAL_HIGH : REWARD_XMAS_HIGH;
	count = RANK_PRIZE[rank];
	const TItemTable* t = ITEM_MANAGER::instance().GetTable(vnum);
	if (!t)
		return 5;
	if (ch->GetEmptyInventory(t->bSize) < 0)
		return 4;
	// Marked before it is given.
	ch->SetQuestFlag(F_CLAIMED, (int)season);
	ch->AutoGiveItem(vnum, count);
	sys_log(0, "RUMI: %s takes the season %u prize, rank %d: %d x %u", ch->GetName(), (unsigned int)season, rank + 1, count, vnum);
	return 0;
}

// game.minigame_rumi_pending(): the chests a left game owes (at login).
int RumiLuaPending(LPCHARACTER ch)
{
	if (!mt2009_rumi::Eligible(ch))
		return 0;
	return mt2009_rumi::GivePending(ch);
}

// game.minigame_rumi_prize(): the gold chest of the season (or of the event
// that runs) - what the top ten take.
DWORD RumiLuaPrize()
{
	using namespace mt2009_rumi;
	if (EventOn())
		return EventNormal() ? REWARD_NORMAL_HIGH : REWARD_XMAS_HIGH;
	return SeasonNormal() ? REWARD_NORMAL_HIGH : REWARD_XMAS_HIGH;
}
} // namespace quest
