// MT2009 PLUS Yut Nori - MT2009_PLUS_YUTNORI_V1
// (the operator, 30 September: Owsap's mini games, full versions, with real
// packets in a new exe; plugged into the in-game event manager,
// playerbot_ingame_events.h, which is included before this file).
//
// Owsap's CMiniGameYutnori (minigame_yutnori.cpp, v6.2.6) with its packets
// unchanged (HEADER_CG/GC_MINI_GAME_YUTNORI = 182, packet.h,
// server-patches/yutnori), so the client's exe and python are Owsap's. The
// board walk (__GetDestPos), the computer's choice (__GetComUnitIndex), the
// throw table and the scores are his, line for line. What is ours:
//
//  - a game is a row of s_games (by pid) here, not a member of CHARACTER: the
//    engine keeps no pointer that could outlive the character. A row whose
//    character is gone (logout, warp, channel change) or whose vid changed (a
//    new login on this core) is dropped by the tick: the board and the yang
//    paid for it are gone, as in Owsap ("if you leave the map you get no
//    reward"); a reward already earned is not (see below).
//  - one expected move at a time (TGame::expect). Owsap let a client move
//    again and again after a Yut or a Mo (m_bReThrow did not stop Move), throw
//    again instead of moving (a re-roll of a bad throw), throw after the game
//    had ended and keep playing after the last throw when that throw was a
//    Back-do that could not move. Here the server takes only the move the game
//    is waiting for, and every other packet is dropped (logged once a game).
//  - the reward is earned when the game ends, not when the button is pressed:
//    the box's vnum goes to the quest flag minigame_yutnori.pending_reward at
//    once, and the score to player.minigame_yutnori. The button, the next
//    start or the next opening of the window hands the box out - once: the flag
//    is cleared before the item is given. A full inventory keeps the flag (the
//    player is told to make room) instead of dropping the box on the ground.
//    A logout between the end and the button no longer loses the reward.
//  - the computer's turn with no piece that can move passes the turn back
//    (Owsap returned and the game hung), and it wins a reward only with
//    points left, like the player.
//  - only real players play here: a playerbot has no client. Its kills grow
//    birch trunks of its own, and it "plays" a board without a table
//    (MT2009_PLUS_BOT_MINIGAMES_V1, playerbot_minigames.h); some of those
//    games go into player.minigame_yutnori like a player's
//    (MT2009_PLUS_MINIGAME_BOT_RANKING_V1), and a bot of the top ten gets
//    its place's prize in the reward window (marked with the quest's own
//    minigame_yutnori.reward_season, as a player's claim is).
//  - the season. Owsap's ranking table summed every event since the server
//    was born. The leading core writes mini_game_yutnori_season (the epoch
//    the event began) when an event starts and
//    mini_game_yutnori_season_closed = 1 when it ends; the scores, the
//    rankings and the top-10 prizes of the table NPC
//    (quest/minigame_yutnori.quest) are this season's. A game starts only
//    while the event runs and its season is known.
//
// Rewards (special_item_group.yutnori.txt): 83030 Zlote Trofeum Yutnori
// (score >= 220), 83031 Srebrne Trofeum Yutnori (150-219), 83034 Brazowy
// Pakiet Yutnori (< 150); the trophies hold the Golden/Silver bundles,
// 83032/83033 - Owsap's 50920-50922 are our Receptura items.
#include "item.h"

// MT2009_PLUS_BOT_MINIGAMES_V1: a bot's trunk (playerbot_minigames.h, later in the unit).
namespace
{
	void PlayerBotMinigameCard(LPCHARACTER ch, int game);
}

namespace mt2009_yutnori
{
	enum EYutSem
	{
		YUTSEM1,	// Do: 1 forward
		YUTSEM2,	// Ge: 2
		YUTSEM3,	// Geol: 3
		YUTSEM4,	// Yut: 4, throw again
		YUTSEM5,	// Mo: 5, throw again
		YUTSEM6,	// Back-do: 1 back
		YUTSEM_MAX
	};

	// What the client is told comes next (PUSH_NEXT_TURN's state).
	enum ETurnState
	{
		STATE_THROW,
		STATE_RE_THROW,
		STATE_MOVE,
		BEFORE_TURN_SELECT,
		AFTER_TURN_SELECT,
		STATE_END
	};

	// What the server takes next.
	enum EExpect
	{
		EXPECT_PC_THROW,
		EXPECT_COM_THROW,
		EXPECT_PC_MOVE,
		EXPECT_COM_MOVE,
		EXPECT_END
	};

	enum
	{
		PLAYER_MAX = 2,
		GOAL_AREA = 11,
		SCORE_STEP = 10,
		LOW_TOTAL_SCORE = 150,
		MID_TOTAL_SCORE = 220,
		INIT_SCORE = 250,
		INIT_REMAIN_COUNT = 20,
		START_GOLD = 30000,
		PIECE_COUNT_MAX = 28,
		BOARD_COUNT_MAX = 999,
	};

	const DWORD ITEM_YUT_PIECE = 79507;	// Pien Brzozy
	const DWORD ITEM_YUT_BOARD = 79508;	// Plansza do Yutnori
	const DWORD REWARD_HIGH = 83030;	// Zlote Trofeum Yutnori
	const DWORD REWARD_MID = 83031;		// Srebrne Trofeum Yutnori
	const DWORD REWARD_LOW = 83034;		// Brazowy Pakiet Yutnori (Owsap's 50922)

	const char* const EVENT_KEY = "yutnori";
	const char* const FLAG_SEASON = "mini_game_yutnori_season";
	const char* const FLAG_SEASON_CLOSED = "mini_game_yutnori_season_closed";
	const char* const QF_PIECE = "minigame_yutnori.piece_count";
	const char* const QF_BOARD = "minigame_yutnori.board_count";
	const char* const QF_PENDING = "minigame_yutnori.pending_reward";

	const DWORD BOOT_DELAY_MS = 30000;	// as the event manager: the flags come a moment after boot

	// Owsap's layouts, which the exe reads byte for byte.
	static_assert(sizeof(TPacketCGMiniGameYutnori) == 3, "CG Yut Nori: 3 bytes");
	static_assert(sizeof(TPacketGCMiniGameYutnori) == 4, "GC Yut Nori head: 4 bytes");
	static_assert(sizeof(TPacketGCMiniGameYutnoriMove) == 5, "Yut Nori MOVE: 5 bytes");
	static_assert(sizeof(TPacketGCMiniGameYutnoriQuestFlag) == 4, "Yut Nori flags: 4 bytes");
	static_assert(sizeof(TPacketGCMiniGameYutnoriThrow) == 2 && sizeof(TPacketGCMiniGameYutnoriPushNextTurn) == 2, "Yut Nori: 2-byte bodies");

	struct TGame
	{
		DWORD	pid;
		DWORD	vid;
		DWORD	season;
		bool	firstThrow;
		bool	reThrow;
		int		score;
		BYTE	remain;
		bool	reward;		// the game ended with a reward
		BYTE	prob;		// the throw the player favours (SET_PROB)
		BYTE	pcYut;
		BYTE	pcPos[PLAYER_MAX];
		BYTE	pcLast[PLAYER_MAX];
		BYTE	comYut;
		BYTE	comPos[PLAYER_MAX];
		BYTE	comLast[PLAYER_MAX];
		BYTE	comNext;
		EExpect	expect;
		bool	warned;		// one log line for packets out of turn
	};

	std::map<DWORD, TGame> s_games;	// pid -> the game
	DWORD s_dwBootAt = 0;
	DWORD s_dwNextSecond = 0;

	bool Eligible(LPCHARACTER ch)
	{
		return ch && ch->IsPC() && ch->GetDesc() && !ch->GetDesc()->IsBot();
	}

	bool ValidReward(DWORD vnum)
	{
		return vnum == REWARD_HIGH || vnum == REWARD_MID || vnum == REWARD_LOW;
	}

	DWORD Season()
	{
		const int s = quest::CQuestManager::instance().GetEventFlag(FLAG_SEASON);
		return s > 0 ? (DWORD)s : 0;
	}

	// ------------------------------------------------------------ packets

	void SendRaw(LPCHARACTER ch, BYTE sub, const void* data, size_t len)
	{
		if (!ch || !ch->GetDesc())
			return;
		TPacketGCMiniGameYutnori head;
		head.bHeader = HEADER_GC_MINI_GAME_YUTNORI;
		head.wSize = (WORD)(sizeof(head) + len);
		head.bSubHeader = sub;
		if (!len)
		{
			ch->GetDesc()->Packet(&head, sizeof(head));
			return;
		}
		ch->GetDesc()->BufferedPacket(&head, sizeof(head));
		ch->GetDesc()->Packet(data, (int)len);
	}

	template <class T> void Send(LPCHARACTER ch, BYTE sub, const T& data)
	{
		SendRaw(ch, sub, &data, sizeof(T));
	}

	void PushNextTurn(LPCHARACTER ch, bool pc, BYTE state)
	{
		Send(ch, YUTNORI_GC_SUBHEADER_PUSH_NEXT_TURN, TPacketGCMiniGameYutnoriPushNextTurn(pc, state));
	}

	void SendQuestFlag(LPCHARACTER ch, BYTE sub)
	{
		const int pieces = ch->GetQuestFlag(QF_PIECE);
		const int boards = ch->GetQuestFlag(QF_BOARD);
		Send(ch, sub, TPacketGCMiniGameYutnoriQuestFlag((WORD)MAX(0, MIN(pieces, 65535)), (WORD)MAX(0, MIN(boards, 65535))));
	}

	// ------------------------------------------------------------ the reward

	// The earned box, once: the flag goes to 0 before the item is made. false:
	// nothing to give, or no room (the player is told, the flag stays).
	bool DeliverPending(LPCHARACTER ch)
	{
		const DWORD vnum = (DWORD)MAX(0, ch->GetQuestFlag(QF_PENDING));
		if (!vnum)
			return false;
		if (!ValidReward(vnum))
		{
			ch->SetQuestFlag(QF_PENDING, 0);
			return false;
		}
		if (ch->GetEmptyInventory(1) < 0)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Yut Nori: zwolnij miejsce w ekwipunku, aby odebrac nagrode.");
			return false;
		}
		ch->SetQuestFlag(QF_PENDING, 0);
		ch->AutoGiveItem(vnum, 1);
		sys_log(0, "YUTNORI: %s (pid %u) receives %u", ch->GetName(), ch->GetPlayerID(), vnum);
		return true;
	}

	void Finish(LPCHARACTER ch, TGame& g, bool rewardable)
	{
		g.expect = EXPECT_END;
		g.reward = rewardable && g.score > 0;
		if (!g.reward)
			return;
		const DWORD vnum = g.score >= MID_TOTAL_SCORE ? REWARD_HIGH : (g.score >= LOW_TOTAL_SCORE ? REWARD_MID : REWARD_LOW);
		// An older box not taken yet is handed out first; without room the
		// better of the two is kept (one flag, one box).
		bool store = true;
		if (ch->GetQuestFlag(QF_PENDING) && !DeliverPending(ch))
		{
			const DWORD old = (DWORD)ch->GetQuestFlag(QF_PENDING);
			if (old == REWARD_HIGH || (old == REWARD_MID && vnum == REWARD_LOW))
			{
				sys_log(0, "YUTNORI: %s keeps the pending %u, %u not stored", ch->GetName(), old, vnum);
				store = false;
			}
		}
		if (store)
			ch->SetQuestFlag(QF_PENDING, (int)vnum);
		DBManager::instance().Query(
				"INSERT INTO player.minigame_yutnori (season, pid, best_score, total_score, games, last_play) "
				"VALUES (%u, %u, %d, %d, 1, NOW()) ON DUPLICATE KEY UPDATE "
				"total_score = total_score + %d, best_score = GREATEST(best_score, %d), games = games + 1, last_play = NOW()",
				g.season, g.pid, g.score, g.score, g.score, g.score);
		sys_log(0, "YUTNORI: %s (pid %u) ends with %d points, season %u", ch->GetName(), g.pid, g.score, g.season);
	}

	// ------------------------------------------------------------ the board (Owsap's)

	void UpdateScore(LPCHARACTER ch, TGame& g, bool increase, bool twice)
	{
		const int step = twice ? SCORE_STEP * 2 : SCORE_STEP;
		g.score = MAX(0, g.score + (increase ? step : -step));
		Send(ch, YUTNORI_GC_SUBHEADER_SET_SCORE, TPacketGCMiniGameYutnoriSetScore((WORD)g.score));
	}

	void UpdateRemainCount(LPCHARACTER ch, TGame& g)
	{
		if (g.remain > 0)
			--g.remain;
		Send(ch, YUTNORI_GC_SUBHEADER_SET_REMAIN_COUNT, TPacketGCMiniGameYutnoriSetRemainCount(g.remain));
	}

	bool IsGoalArea(const BYTE* pos)
	{
		return pos[0] == GOAL_AREA && pos[1] == GOAL_AREA;
	}

	bool IsDouble(const BYTE* pos)
	{
		return pos[0] == pos[1] && pos[0] != 0 && pos[0] != GOAL_AREA;
	}

	bool CanMoveBack(const BYTE* pos)
	{
		for (int i = 0; i < PLAYER_MAX; ++i)
			if (pos[i] == 0 || pos[i] == GOAL_AREA)
				return false;
		return true;
	}

	bool ExcludeBackDo(const BYTE* pos)
	{
		for (int i = 0; i < PLAYER_MAX; ++i)
			if (pos[i] == GOAL_AREA || pos[i] == GOAL_AREA - 1)
				return true;
		return false;
	}

	BYTE RandomYut(bool reThrow, bool excludeBackDo, BYTE favoured)
	{
		std::vector<BYTE> v;
		for (BYTE yut = YUTSEM1; yut < YUTSEM_MAX; ++yut)
		{
			if (reThrow && (yut == YUTSEM4 || yut == YUTSEM5))
				continue;
			if (excludeBackDo && yut == YUTSEM6)
				continue;
			if (yut == favoured)
				v.push_back(yut);
			v.push_back(yut);
		}
		return v[number(0, (int)v.size() - 1)];
	}

	void GetDestPos(BYTE moveCount, BYTE* start, BYTE* dest, BYTE* last)
	{
		if (*start == 0 || *dest == 0 || *last == 0)
			*start = GOAL_AREA;

		*dest = *start;
		const BYTE next = *dest;

		if (moveCount == YUTSEM6)
		{
			if (*dest == 20 || *dest == 21)
				*dest = 1;
			else if (*dest == 26 || *dest == 5)
				*dest = 6;
			else if (*dest == 28 || *dest == 24)
				*dest = 23;
			else if (*dest == 23)
				*dest = (*last == 27) ? 27 : 22;
			else if (*dest == 16)
				*dest = (*last == 29) ? 29 : 17;	// Owsap tested dest == 16 here: always 29, off the path walked
			else
				*dest = (*dest <= 20) ? *dest + 1 : *dest - 1;
			return;
		}

		for (BYTE i = 0; i < moveCount + 1; ++i)
		{
			const BYTE prev = *dest;
			if (*dest == 1)
				*dest = (next == 1) ? 21 : 20;
			else if (*dest == 6)
				*dest = (next == 6) ? 26 : 5;
			else if (*dest == 23)
				*dest = (next == 23 || *last == 22) ? 24 : 28;
			else if (*dest == 27)
				*dest = 23;
			else if (*dest == 29)
				*dest = 16;
			else if (*dest == 25 || *dest == 12)
			{
				*dest = GOAL_AREA;
				*last = prev;
				break;
			}
			else
				*dest = (*dest <= 20) ? *dest - 1 : *dest + 1;
			*last = prev;
		}
	}

	BYTE ComUnitIndex(TGame& g)
	{
		BYTE start[PLAYER_MAX] = { g.comPos[0], g.comPos[1] };
		BYTE dest[PLAYER_MAX] = { g.comPos[0], g.comPos[1] };
		BYTE last[PLAYER_MAX] = { g.comLast[0], g.comLast[1] };

		if (start[0] == GOAL_AREA && start[1] == GOAL_AREA)
			return PLAYER_MAX;
		if (start[0] == start[1])
			return 0;

		const bool exclude1 = (g.comYut == YUTSEM6 && start[0] == 0) || start[0] == GOAL_AREA;
		const bool exclude2 = (g.comYut == YUTSEM6 && start[1] == 0) || start[1] == GOAL_AREA;

		for (int i = 0; i < PLAYER_MAX; ++i)
			GetDestPos(g.comYut, &start[i], &dest[i], &last[i]);

		const bool catch1 = dest[0] == g.pcPos[0] || dest[0] == g.pcPos[1];
		const bool catch2 = dest[1] == g.pcPos[0] || dest[1] == g.pcPos[1];

		if (catch1 && !exclude1)
			return 0;
		if (catch2 && !exclude2)
			return 1;
		if (exclude1 && exclude2)
			return PLAYER_MAX;
		if (exclude1)
			return 1;
		if (exclude2)
			return 0;
		return g.comNext;
	}

	// A piece walks: catches, the double, the goal (Owsap's Move and
	// RequestComAction, one body for both sides).
	void Walk(LPCHARACTER ch, TGame& g, bool pc, BYTE unit)
	{
		BYTE* myPos = pc ? g.pcPos : g.comPos;
		BYTE* myLast = pc ? g.pcLast : g.comLast;
		BYTE* otherPos = pc ? g.comPos : g.pcPos;
		BYTE* otherLast = pc ? g.comLast : g.pcLast;
		const BYTE yut = pc ? g.pcYut : g.comYut;

		BYTE start = myPos[unit];
		BYTE dest = start;
		BYTE last = myLast[unit];
		GetDestPos(yut, &start, &dest, &last);

		bool caught = false;
		for (BYTE i = 0; i < PLAYER_MAX; ++i)
		{
			if (otherPos[i] == dest && dest != 0 && dest != GOAL_AREA)
			{
				UpdateScore(ch, g, pc, false);
				otherPos[i] = 0;
				otherLast[i] = 0;
				Send(ch, YUTNORI_GC_SUBHEADER_PUSH_CATCH_YUT, TPacketGCMiniGameYutnoriPushCatchYut(!pc, i));
				caught = true;
			}
		}

		Send(ch, YUTNORI_GC_SUBHEADER_MOVE, TPacketGCMiniGameYutnoriMove(pc, unit, caught, start, dest));

		if (IsDouble(myPos))
		{
			if (dest == GOAL_AREA)
				UpdateScore(ch, g, pc, true);
			for (int i = 0; i < PLAYER_MAX; ++i)
			{
				myPos[i] = dest;
				myLast[i] = last;
			}
		}
		else
		{
			if (dest == GOAL_AREA)
				UpdateScore(ch, g, pc, false);
			myPos[unit] = dest;
			myLast[unit] = last;
		}
	}

	// ------------------------------------------------------------ the moves

	bool Expect(LPCHARACTER ch, TGame& g, EExpect want, const char* what)
	{
		if (g.expect == want)
			return true;
		if (!g.warned)
		{
			g.warned = true;
			sys_log(0, "YUTNORI: %s sent %s out of turn (expected %d) - dropped", ch->GetName(), what, (int)g.expect);
		}
		return false;
	}

	void Throw(LPCHARACTER ch, TGame& g, bool pc)
	{
		if (!Expect(ch, g, pc ? EXPECT_PC_THROW : EXPECT_COM_THROW, "a throw"))
			return;

		if (g.firstThrow)
		{
			const BYTE yut = RandomYut(false, true, YUTSEM_MAX);
			Send(ch, YUTNORI_GC_SUBHEADER_THROW, TPacketGCMiniGameYutnoriThrow(pc, yut));
			if (pc)
			{
				g.pcYut = yut;
				g.expect = EXPECT_COM_THROW;
				PushNextTurn(ch, false, BEFORE_TURN_SELECT);
			}
			else
			{
				// The lower throw begins (Owsap; the client's notices say so).
				g.firstThrow = false;
				const bool pcFirst = g.pcYut < yut;
				g.expect = pcFirst ? EXPECT_PC_THROW : EXPECT_COM_THROW;
				PushNextTurn(ch, pcFirst, AFTER_TURN_SELECT);
			}
			return;
		}

		const bool reThrow = g.reThrow;
		g.reThrow = false;

		if (pc && !reThrow)
		{
			if (g.remain == 0)
			{
				Finish(ch, g, true);
				PushNextTurn(ch, false, STATE_END);
				return;
			}
			UpdateScore(ch, g, false, false);
			UpdateRemainCount(ch, g);
		}

		const bool excludeBackDo = ExcludeBackDo(pc ? g.pcPos : g.comPos);
		const BYTE favoured = pc ? g.prob : (BYTE)YUTSEM_MAX;
		const BYTE lastYut = pc ? g.pcYut : g.comYut;
		BYTE yut = RandomYut(reThrow, excludeBackDo, favoured);
		for (int tries = 0; yut == lastYut && tries < 32; ++tries)	// Owsap: never the same throw twice
			yut = RandomYut(reThrow, excludeBackDo, favoured);

		Send(ch, YUTNORI_GC_SUBHEADER_THROW, TPacketGCMiniGameYutnoriThrow(pc, yut));
		if (pc)
			g.pcYut = yut;
		else
			g.comYut = yut;

		if (yut == YUTSEM6 && !CanMoveBack(pc ? g.pcPos : g.comPos))
		{
			// A Back-do with no piece on the board: the turn passes. The
			// player's last throw ends the game here (Owsap played on).
			if (pc && (g.remain == 0 || g.score == 0))
			{
				Finish(ch, g, true);
				PushNextTurn(ch, false, STATE_END);
				return;
			}
			g.expect = pc ? EXPECT_COM_THROW : EXPECT_PC_THROW;
			PushNextTurn(ch, !pc, STATE_THROW);
			return;
		}

		g.expect = pc ? EXPECT_PC_MOVE : EXPECT_COM_MOVE;
		PushNextTurn(ch, pc, STATE_MOVE);
	}

	void CharClick(LPCHARACTER ch, TGame& g, BYTE unit)
	{
		if (g.expect != EXPECT_PC_MOVE || unit >= PLAYER_MAX || g.pcPos[unit] == GOAL_AREA)
			return;
		BYTE start = g.pcPos[unit];
		BYTE dest = start;
		BYTE last = g.pcLast[unit];
		GetDestPos(g.pcYut, &start, &dest, &last);
		Send(ch, YUTNORI_GC_SUBHEADER_AVAILABLE_AREA, TPacketGCMiniGameYutnoriAvailableArea(unit, dest));
	}

	void Move(LPCHARACTER ch, TGame& g, BYTE unit)
	{
		if (!Expect(ch, g, EXPECT_PC_MOVE, "a move"))
			return;
		if (unit >= PLAYER_MAX || g.pcPos[unit] == GOAL_AREA)
			return;
		// A Back-do takes a piece on the board (the throw checked both are).
		if (g.pcYut == YUTSEM6 && g.pcPos[unit] == 0)
			return;

		Walk(ch, g, true, unit);

		if (IsGoalArea(g.pcPos))
		{
			Finish(ch, g, true);
			PushNextTurn(ch, true, STATE_END);
		}
		else if (g.remain == 0 || g.score == 0)
		{
			Finish(ch, g, true);
			PushNextTurn(ch, false, STATE_END);
		}
		else if (g.pcYut == YUTSEM4 || g.pcYut == YUTSEM5)
		{
			g.reThrow = true;
			g.expect = EXPECT_PC_THROW;
			PushNextTurn(ch, true, STATE_RE_THROW);
		}
		else
		{
			g.expect = EXPECT_COM_THROW;
			PushNextTurn(ch, false, STATE_THROW);
		}
	}

	void ComAction(LPCHARACTER ch, TGame& g)
	{
		if (!Expect(ch, g, EXPECT_COM_MOVE, "the computer's move"))
			return;

		const BYTE unit = ComUnitIndex(g);
		if (unit >= PLAYER_MAX || g.comPos[unit] == GOAL_AREA)
		{
			// Nothing the computer can move: the player's turn (Owsap hung here).
			g.expect = EXPECT_PC_THROW;
			PushNextTurn(ch, true, STATE_THROW);
			return;
		}
		g.comNext = unit ? 0 : 1;

		Walk(ch, g, false, unit);

		if (IsGoalArea(g.comPos))
		{
			Finish(ch, g, true);
			PushNextTurn(ch, false, STATE_END);
		}
		else if (g.comYut == YUTSEM4 || g.comYut == YUTSEM5)
		{
			g.reThrow = true;
			g.expect = EXPECT_COM_THROW;
			PushNextTurn(ch, false, STATE_RE_THROW);
		}
		else
		{
			g.expect = EXPECT_PC_THROW;
			PushNextTurn(ch, true, STATE_THROW);
		}
	}

	// ------------------------------------------------------------ the game's life

	TGame* Find(LPCHARACTER ch)
	{
		std::map<DWORD, TGame>::iterator it = s_games.find(ch->GetPlayerID());
		if (it == s_games.end())
			return NULL;
		if (it->second.vid != (DWORD)ch->GetVID())
		{
			s_games.erase(it);	// a game of an earlier login
			return NULL;
		}
		return &it->second;
	}

	void Stop(LPCHARACTER ch)
	{
		if (s_games.erase(ch->GetPlayerID()))
			SendRaw(ch, YUTNORI_GC_SUBHEADER_STOP, NULL, 0);
	}

	void Start(LPCHARACTER ch)
	{
		if (Find(ch))
			return;
		if (!InGameEventIsActive(EVENT_KEY))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Yut Nori: event sie nie odbywa.");
			return;
		}
		const DWORD season = Season();
		if (!season || quest::CQuestManager::instance().GetEventFlag(FLAG_SEASON_CLOSED))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Yut Nori: event wlasnie sie zaczyna, sprobuj za chwile.");
			return;
		}
		// A box from an earlier game first (no room: no new game either).
		if (ch->GetQuestFlag(QF_PENDING) && !DeliverPending(ch) && ch->GetQuestFlag(QF_PENDING))
			return;
		const int boards = ch->GetQuestFlag(QF_BOARD);
		if (boards <= 0 || ch->GetGold() < START_GOLD)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Yut Nori: potrzebujesz planszy do Yut Nori i 30 000 yang.");
			return;
		}

		ch->SetQuestFlag(QF_BOARD, boards - 1);
		PlayerBotChangeGold(ch, -(long long)START_GOLD);

		TGame g;
		memset(&g, 0, sizeof(g));
		g.pid = ch->GetPlayerID();
		g.vid = (DWORD)ch->GetVID();
		g.season = season;
		g.firstThrow = true;
		g.score = INIT_SCORE;
		g.remain = INIT_REMAIN_COUNT;
		g.prob = YUTSEM1;
		g.comNext = 0;
		g.expect = EXPECT_PC_THROW;
		s_games[g.pid] = g;
		sys_log(0, "YUTNORI: %s (pid %u) starts a game, season %u, %d board(s) left", ch->GetName(), g.pid, season, boards - 1);
		SendRaw(ch, YUTNORI_GC_SUBHEADER_START, NULL, 0);
	}

	void Reward(LPCHARACTER ch, TGame& g)
	{
		if (g.expect != EXPECT_END)
			return;	// no reward during the game
		if (g.reward && ch->GetQuestFlag(QF_PENDING) && !DeliverPending(ch))
			return;	// no room: the window stays, the button works again
		Stop(ch);
	}

	// A birch branch: from a kill or the item. false: the event is off or the
	// boards are at their maximum.
	bool UpdateQuestFlag(LPCHARACTER ch)
	{
		if (!Eligible(ch) || !InGameEventIsActive(EVENT_KEY))
			return false;
		const int pieces = ch->GetQuestFlag(QF_PIECE);
		const int boards = ch->GetQuestFlag(QF_BOARD);
		if (boards >= BOARD_COUNT_MAX)
		{
			SendQuestFlag(ch, YUTNORI_GC_SUBHEADER_NO_MORE_GAIN);
			return false;
		}
		if (pieces + 1 >= PIECE_COUNT_MAX)
		{
			ch->SetQuestFlag(QF_PIECE, 0);
			ch->SetQuestFlag(QF_BOARD, boards + 1);
			SendQuestFlag(ch, YUTNORI_GC_SUBHEADER_SET_YUT_BOARD_FLAG);
		}
		else
		{
			ch->SetQuestFlag(QF_PIECE, pieces + 1);
			SendQuestFlag(ch, YUTNORI_GC_SUBHEADER_SET_YUT_PIECE_FLAG);
		}
		return true;
	}

	// ------------------------------------------------------------ the season (the leader)

	void LeadSeason(long now)
	{
		const bool active = InGameEventIsActive(EVENT_KEY);
		const int season = quest::CQuestManager::instance().GetEventFlag(FLAG_SEASON);
		const int closed = quest::CQuestManager::instance().GetEventFlag(FLAG_SEASON_CLOSED);
		if (active && (season <= 0 || closed))
		{
			// A new event: a new season, its scores start from nothing.
			mt2009_ingame_event::Request(FLAG_SEASON, (int)now, now);
			mt2009_ingame_event::Request(FLAG_SEASON_CLOSED, 0, now);
		}
		else if (!active && season > 0 && !closed)
			mt2009_ingame_event::Request(FLAG_SEASON_CLOSED, 1, now);	// the prizes are this season's until the next
	}

	void EverySecond(DWORD dwNow)
	{
		if (!s_dwBootAt)
			s_dwBootAt = dwNow;
		if (dwNow - s_dwBootAt >= BOOT_DELAY_MS && IsPlayerBotEventLeader())
			LeadSeason((long)time(NULL));

		// The games whose player left this core (or came back as a new login).
		for (std::map<DWORD, TGame>::iterator it = s_games.begin(); it != s_games.end(); )
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch || (DWORD)ch->GetVID() != it->second.vid || !ch->GetDesc())
			{
				sys_log(0, "YUTNORI: pid %u left during a game (%s)", it->first,
						it->second.expect == EXPECT_END ? "ended" : "lost");
				s_games.erase(it++);
			}
			else
				++it;
		}
	}
}

// The events' tick (playerbot_ingame_events.h, InGameEventTick).
void YutnoriTick(DWORD dwNow)
{
	using namespace mt2009_yutnori;
	if (s_dwNextSecond && (int)(dwNow - s_dwNextSecond) < 0)
		return;
	s_dwNextSecond = dwNow + 1000;
	EverySecond(dwNow);
}

// HEADER_CG_MINI_GAME_YUTNORI (input_main.cpp, MT2009_PLUS_YUTNORI_V1 (input)).
void YutnoriPacket(LPCHARACTER ch, const char* data)
{
	using namespace mt2009_yutnori;
	if (!Eligible(ch) || !data)
		return;
	const TPacketCGMiniGameYutnori* p = reinterpret_cast<const TPacketCGMiniGameYutnori*>(data);
	switch (p->bSubHeader)
	{
		case YUTNORI_CG_SUBHEADER_START:
			Start(ch);
			return;
		case YUTNORI_CG_SUBHEADER_REQUEST_QUEST_FLAG:
			if (!Find(ch) && ch->GetQuestFlag(QF_PENDING))
				DeliverPending(ch);
			SendQuestFlag(ch, YUTNORI_GC_SUBHEADER_SET_QUEST_FLAG);
			return;
	}

	TGame* g = Find(ch);
	if (!g)
	{
		// No game here (it was lost with a warp): the client's window goes back.
		if (p->bSubHeader == YUTNORI_CG_SUBHEADER_GIVEUP || p->bSubHeader == YUTNORI_CG_SUBHEADER_REWARD)
			SendRaw(ch, YUTNORI_GC_SUBHEADER_STOP, NULL, 0);
		return;
	}

	switch (p->bSubHeader)
	{
		case YUTNORI_CG_SUBHEADER_GIVEUP:
			Stop(ch);	// an earned box stays in the flag
			break;
		case YUTNORI_CG_SUBHEADER_SET_PROB:
			if (p->bArgument < YUTSEM_MAX && g->expect != EXPECT_END)
			{
				g->prob = p->bArgument;
				Send(ch, YUTNORI_GC_SUBHEADER_SET_PROB, TPacketGCMiniGameYutnoriSetProb(p->bArgument));
			}
			break;
		case YUTNORI_CG_SUBHEADER_CLICK_CHAR:
			CharClick(ch, *g, p->bArgument);
			break;
		case YUTNORI_CG_SUBHEADER_THROW:
			Throw(ch, *g, p->bArgument != 0);
			break;
		case YUTNORI_CG_SUBHEADER_MOVE:
			Move(ch, *g, p->bArgument);
			break;
		case YUTNORI_CG_SUBHEADER_REQUEST_COM_ACTION:
			ComAction(ch, *g);
			break;
		case YUTNORI_CG_SUBHEADER_REWARD:
			Reward(ch, *g);
			break;
		default:
			sys_err("YUTNORI: %s sent an unknown sub header %u", ch->GetName(), (unsigned int)p->bSubHeader);
			break;
	}
}

// A kill's roll for a birch branch (item_manager.cpp CreateQuestDropItem,
// MT2009_PLUS_YUTNORI_V1 (drop)): Owsap's GetDropPerKillPct(50, 100, delta,
// "mini_game_yutnori_drop"), for a real player only.
void YutnoriKillRoll(LPCHARACTER killer, int iDeltaPercent, int iRandRange)
{
	int GetDropPerKillPct(int iMinimum, int iDefault, int iDeltaPercent, const char* c_pszFlag);
	// MT2009_PLUS_BOT_MINIGAMES_V1: a bot rolls the same trunk, for its own
	// count and a real Plansza do Yutnori (playerbot_minigames.h).
	const bool bot = killer && killer->IsPC() && killer->GetDesc() && killer->GetDesc()->IsBot();
	if ((!bot && !mt2009_yutnori::Eligible(killer)) || !InGameEventIsActive(mt2009_yutnori::EVENT_KEY))
		return;
	if (GetDropPerKillPct(50, 100, iDeltaPercent, "mini_game_yutnori_drop") >= number(1, iRandRange))
	{
		if (bot)
			PlayerBotMinigameCard(killer, 2);
		else
			mt2009_yutnori::UpdateQuestFlag(killer);
	}
}

// The two tokens (char_item.cpp USE_SPECIAL, MT2009_PLUS_YUTNORI_V1 (use)).
// true: the item was used (one taken off).
bool YutnoriUseItem(LPCHARACTER ch, LPITEM item)
{
	using namespace mt2009_yutnori;
	if (!Eligible(ch) || !item)
		return false;
	if (item->GetVnum() == ITEM_YUT_PIECE)
	{
		if (!UpdateQuestFlag(ch))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Yut Nori: pnie brzozy mozna dodac tylko w czasie eventu.");
			return false;
		}
		item->SetCount(item->GetCount() - 1);
		return true;
	}
	if (item->GetVnum() == ITEM_YUT_BOARD)
	{
		const int boards = ch->GetQuestFlag(QF_BOARD);
		if (boards >= BOARD_COUNT_MAX)
		{
			SendQuestFlag(ch, YUTNORI_GC_SUBHEADER_NO_MORE_GAIN);
			return false;
		}
		ch->SetQuestFlag(QF_BOARD, boards + 1);
		SendQuestFlag(ch, YUTNORI_GC_SUBHEADER_SET_YUT_BOARD_FLAG);
		item->SetCount(item->GetCount() - 1);
		return true;
	}
	return false;
}

// "/yutnori_gm" for a game master's look (the season, the games on this core).
void YutnoriGmInfo(LPCHARACTER ch)
{
	using namespace mt2009_yutnori;
	if (!ch || ch->GetGMLevel() < GM_HIGH_WIZARD)
		return;
	ch->ChatPacket(CHAT_TYPE_INFO, "Yut Nori: event %s, sezon %u (zamkniety %d), gry na tym rdzeniu %u, plansze %d, pnie %d, nagroda czeka %d",
			InGameEventIsActive(EVENT_KEY) ? "trwa" : "nie trwa", Season(),
			quest::CQuestManager::instance().GetEventFlag(FLAG_SEASON_CLOSED), (unsigned int)s_games.size(),
			ch->GetQuestFlag(QF_BOARD), ch->GetQuestFlag(QF_PIECE), ch->GetQuestFlag(QF_PENDING));
}
