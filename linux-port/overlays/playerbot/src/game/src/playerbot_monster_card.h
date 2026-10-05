#ifndef __INC_METIN2_PLAYERBOT_MONSTER_CARD_H__
#define __INC_METIN2_PLAYERBOT_MONSTER_CARD_H__

// Karty Potworow - the Monster Card System (MT2009_PLUS_MONSTER_CARDS_V1).
//
// Autor systemu: Digi Rasta (nowy-system v0.25.2), a port of the
// "Official-Monster-Card-System" package (Best Studio) onto our engine,
// taken into MT2009 PLUS as our own code. His package hooked it into the
// engine with zastosuj.py; here the four engine calls are
// server-patches/monstercard (edits.json), the tables and the items are
// mariadb/playerbot/apply.sh, the window is client-patches/client-2.0.30/root
// (monstercard.py, uimonstercard.py).
//
//   * A card mission: three monsters drawn from the pool of the mission level
//     (playerbot_monster_card_data.h, 15 levels, then from 1 again); killing
//     all three earns a Monster Card (50283, the monster in socket 1) and the
//     next level. One reset of the mission a day is free, the next ones take a
//     Karta Nowego Poczatku (72322); new targets take a Karta Nowego Ukladu
//     (72323).
//   * A monster of the pools drops its card at 5% (to the killer, 2 minutes).
//     Using a card adds it to the account's collection of that monster;
//     9/21/30/60/90 cards raise it a star (0..5). The stars open the monster's
//     functions: the drop list (1 star, from the drop wiki), the polymorph (3,
//     every 3 h), the teleport to it (4, 30 min), the summon (5, 12 h) and the
//     recruitment (5, 24 h); 10 collected cards trade for a tradable card
//     (50284, the monster in socket 0).
//   * The sets: 17 groups of monsters; a rank N takes N stars of every monster
//     of the set and gives its bonus as AFFECT_COLLECT. The progress is the
//     ACCOUNT's (account_id = GetAID()); the bonuses are each character's,
//     brought in line with the account at login and after every change, the
//     given amount kept in quest flags "karty_bonus.p<point>".
//   * No damage bonus from the stars - the package's formula was wrong (12x
//     at 9 cards, a division by zero at 0); the stars open functions, the
//     strength is the sets'.
//   * Bots have no cards: their kills, uses and commands are ignored. Nobody
//     can recruit a monster against a bot (the only "other players" here).
//   * M2_MONSTER_CARDS=0 (the game container's environment) switches the
//     system off: no drops, the cards do nothing, /cardmonster answers
//     "DISABLED" and a character's set bonuses come off at its next login.
//
// The tables keep the names of his package (player.nowy_karty_misja,
// nowy_karty_status, nowy_karty_osiagniecia) and the bonus flags theirs, so a
// world that ran his package keeps its progress and nothing is given twice.
//
// The client speaks through chat commands only, no packet: "/cardmonster
// <command> [args]" in, "MONSTERCARDSYSTEM <text>" out (the list:
// server-patches/monstercard/README.md). GM test commands: /cardmonster
// gm_misja | gm_gwiazdki <0-5> | gm_karta <mob> <n> | gm_reset | gm_przeladuj
// | gm_info.

#include "playerbot_monster_card_data.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace mt2009_mcard
{
	const DWORD CARD_MISSION = 50283;		// from a mission or a drop: the monster in socket 1, no trade
	const DWORD CARD_TRADABLE = 50284;		// the tradable card: the monster in socket 0
	const DWORD CARD_RESET = 72322;			// Karta Nowego Poczatku (a mission reset past the free one)
	const DWORD CARD_SHUFFLE = 72323;		// Karta Nowego Ukladu (new targets)

	const int TARGETS = 3;
	const int STARS_MAX = 5;
	const int NEEDED[STARS_MAX] = { 9, 21, 30, 60, 90 };	// cards for the next star (9/30/60/120/210 in all)
	const int TRADE_COST = 10;				// collected cards for one tradable card - what the window says
	const int DROP_PERCENT = 5;

	const int WAIT_TELEPORT = 30 * 60;
	const int WAIT_POLYMORPH = 3 * 60 * 60;
	const int WAIT_SUMMON = 12 * 60 * 60;
	const int WAIT_RECRUIT = 24 * 60 * 60;
	const int RESET_WINDOW = 24 * 60 * 60;	// one free mission reset a window, the next for a Karta Nowego Poczatku
	const int DROP_LIST_GAP = 3;
	const size_t DROP_LIST_ROWS = 20;		// the rest: the drop wiki window
	const int SAVE_EVERY = 60;				// seconds: the kill counters alone are saved at most this often

#ifdef OXEVENT_MAP_INDEX
	const long OX_MAP = OXEVENT_MAP_INDEX;
#else
	const long OX_MAP = 113;				// OXEvent.h
#endif

	enum ECommand
	{
		CMD_STATE = 2, CMD_TARGETS = 3, CMD_NEW = 4, CMD_REWARD = 5, CMD_SHUFFLE = 6, CMD_RESET = 7, CMD_FUNCTION = 8,
		CMD_WARP_TO_RECRUIT = 9, CMD_SET_TOGGLE = 10, CMD_SET_REGISTER = 11
	};

	enum EFunction
	{
		FN_TELEPORT = 0, FN_POLYMORPH = 1, FN_DROP = 2, FN_SUMMON = 3, FN_RECRUIT = 4, FN_TRADE = 5, FN_PROMOTE = 6
	};

	// The players' texts, CP1250 as escapes (a hex digit never follows one -
	// the literals are split there).
	const char* const MSG_KILLED = "Pokona" "\xb3" "e" "\x9c" " potwora: %s (poziom misji %d).";
	const char* const MSG_MISSION_DONE = "Misja poziomu %d uko" "\xf1" "czona. Odbierz Kart" "\xea" " Potwora w oknie kart.";
	const char* const MSG_NO_MONSTER = "Ta karta nie ma przypisanego potwora.";
	const char* const MSG_MAX = "Ten potw" "\xf3" "r ma ju" "\xbf" " maksymaln" "\xb9" " liczb" "\xea" " gwiazdek.";
	const char* const MSG_FULL = "Masz ju" "\xbf" " komplet kart tego potwora. Awansuj go w oknie kart, zanim u" "\xbf" "yjesz kolejnej.";
	const char* const MSG_CARD_USED = "Karta potwora %s: %d/%d.";
	const char* const MSG_NO_RESET = "Kolejny reset misji w tym oknie czasu wymaga Karty Nowego Pocz" "\xb9" "tku.";
	const char* const MSG_NO_SHUFFLE = "Potrzebujesz Karty Nowego Uk" "\xb3" "adu, by wylosowa" "\xe6" " nowe cele misji.";
	const char* const MSG_NO_TELEPORT = "Brak informacji o teleporcie do tego potwora.";
	const char* const MSG_NOT_NOW = "Teraz nie mo" "\xbf" "esz tego u" "\xbf" "y" "\xe6" " (zamknij handel, sklep, magazyn; nie w lochu).";
	const char* const MSG_NO_DROP = "Ten potw" "\xf3" "r nie ma dropu.";
	const char* const MSG_EVERY_3S = "Mo" "\xbf" "esz to robi" "\xe6" " co 3 sekundy.";
	const char* const MSG_NO_SPAWN = "Nie uda" "\xb3" "o si" "\xea" " przywo" "\xb3" "a" "\xe6" " potwora.";
	const char* const MSG_NO_POLY = "Nie mo" "\xbf" "esz teraz zmieni" "\xe6" " si" "\xea" " w tego potwora.";
	const char* const MSG_NO_POLY_OX = "Na tej mapie nie mo" "\xbf" "na si" "\xea" " przemieni" "\xe6" ".";
	const char* const MSG_DISABLED = "Karty Potwor" "\xf3" "w s" "\xb9" " wy" "\xb3\xb9" "czone na tym serwerze.";

	template <typename T, size_t N> size_t CountOf(const T (&)[N]) { return N; }

	// ---------------------------------------------------------------- the switch

	// M2_MONSTER_CARDS in the game container's environment (docker-compose.yml),
	// read once: on unless "0", "off", "no" or "false" (MT2009 Classic sets 0).
	bool Enabled()
	{
		static int s_enabled = -1;
		if (s_enabled < 0)
		{
			const char* value = getenv("M2_MONSTER_CARDS");
			s_enabled = 1;
			if (value && (!strcmp(value, "0") || !strcasecmp(value, "off") || !strcasecmp(value, "no") || !strcasecmp(value, "false")))
				s_enabled = 0;
			sys_log(0, "MONSTER_CARDS: %s (M2_MONSTER_CARDS=%s)", s_enabled ? "on" : "off", value ? value : "");
		}
		return s_enabled != 0;
	}

	// ---------------------------------------------------------------- the account's state

	struct SMonster
	{
		int collected, kills, stars;
		long long teleport, polymorph, summon, recruit;
		SMonster() : collected(0), kills(0), stars(0), teleport(0), polymorph(0), summon(0), recruit(0) {}
	};

	struct SAccount
	{
		DWORD targets[TARGETS];
		DWORD deck[DECK_SIZE];
		int killed[TARGETS];
		int level;							// mission levels given (0..LEVELS); the one shown
		long long missionReset, shuffleAt, windowStart;
		int windowCount;
		std::map<DWORD, SMonster> monsters;
		std::set<DWORD> worn;				// sets whose bonus is on
		std::map<DWORD, int> rank;			// sets registered, their rank
		long long dropListAt;
		DWORD recruitVid;
		bool dirty;							// kill counters not saved yet
		long long savedAt;

		SAccount() : level(0), missionReset(0), shuffleAt(0), windowStart(0), windowCount(0), dropListAt(0), recruitVid(0),
			dirty(false), savedAt(0)
		{
			memset(targets, 0, sizeof(targets));
			memset(deck, 0, sizeof(deck));
			memset(killed, 0, sizeof(killed));
		}
	};

	std::map<DWORD, SAccount> s_accounts;		// account_id -> state, for as long as the core runs
	std::set<DWORD> s_needSync;					// player ids whose bonuses waited for their affects

	bool IsBot(LPCHARACTER ch)
	{
		return CPlayerBotManager::instance().IsManaged(ch->GetPlayerID());
	}

	bool Counts(LPCHARACTER ch)
	{
		return ch && ch->IsPC() && ch->GetDesc() && ch->GetAID() && !IsBot(ch);
	}

	long long Number(const char* text)
	{
		return text ? atoll(text) : 0;
	}

	int Needed(const SMonster& m)
	{
		return m.stars >= STARS_MAX ? 0 : NEEDED[m.stars];
	}

	bool IsCardMonster(DWORD race)
	{
		static std::set<DWORD> pool;
		if (pool.empty())
		{
			for (int l = 0; l < LEVELS; ++l)
				for (int i = 0; i < DECK_SIZE; ++i)
					if (LEVEL_POOL[l][i])
						pool.insert(LEVEL_POOL[l][i]);
		}
		return pool.find(race) != pool.end();
	}

	DWORD RandomMonster()
	{
		std::vector<DWORD> v;
		for (int l = 0; l < LEVELS; ++l)
			for (int i = 0; i < DECK_SIZE; ++i)
				if (LEVEL_POOL[l][i] && std::find(v.begin(), v.end(), LEVEL_POOL[l][i]) == v.end())
					v.push_back(LEVEL_POOL[l][i]);
		return v.empty() ? 0 : v[number(0, (int) v.size() - 1)];
	}

	const SAchievement* Achievement(DWORD vnum)
	{
		for (size_t i = 0; i < CountOf(ACHIEVEMENTS); ++i)
			if (ACHIEVEMENTS[i].dwVnum == vnum)
				return &ACHIEVEMENTS[i];
		return NULL;
	}

	int FirstRank(const SAchievement& a)
	{
		return (a.iRanks > 0 && a.iRanks < 5) ? 6 - a.iRanks : 1;
	}

	int RankBonus(const SAchievement& a, int rank)
	{
		if (rank < FirstRank(a) || rank > 5)
			return 0;
		return a.aiBonus[rank - 1];
	}

	void Shuffle(DWORD* a, int n)
	{
		for (int i = n - 1; i > 0; --i)
			std::swap(a[i], a[number(0, i)]);
	}

	std::vector<std::string> Tokens(const char* text)
	{
		std::vector<std::string> out;
		std::string cur;
		for (const char* p = text ? text : ""; ; ++p)
		{
			if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '\0')
			{
				if (!cur.empty())
					out.push_back(cur);
				cur.clear();
				if (*p == '\0')
					break;
			}
			else
				cur += *p;
		}
		return out;
	}

	// ---------------------------------------------------------------- the database

	void SaveMission(DWORD aid, const SAccount& s)
	{
		char q[2048];
		int n = snprintf(q, sizeof(q), "REPLACE INTO player.nowy_karty_misja (account_id,glowny0,glowny1,glowny2,"
				"talia0,talia1,talia2,talia3,talia4,talia5,talia6,talia7,talia8,talia9,talia10,talia11,talia12,talia13,talia14,talia15,"
				"zabity0,zabity1,zabity2,poziom,reset_misji,reset_kolejnosci,okno_start,okno_liczba) VALUES (%u", aid);
		for (int i = 0; i < TARGETS; ++i)
			n += snprintf(q + n, sizeof(q) - n, ",%u", s.targets[i]);
		for (int i = 0; i < DECK_SIZE; ++i)
			n += snprintf(q + n, sizeof(q) - n, ",%u", s.deck[i]);
		for (int i = 0; i < TARGETS; ++i)
			n += snprintf(q + n, sizeof(q) - n, ",%d", s.killed[i]);
		snprintf(q + n, sizeof(q) - n, ",%d,%lld,%lld,%lld,%d)", s.level, s.missionReset, s.shuffleAt, s.windowStart, s.windowCount);
		DBManager::instance().DirectQuery("%s", q);
	}

	void SaveMonster(DWORD aid, DWORD vnum, const SMonster& m)
	{
		DBManager::instance().DirectQuery("REPLACE INTO player.nowy_karty_status (account_id,vnum,zebrane,zabicia,gwiazdki,teleport,przemiana,przywolanie,rekrutacja) "
				"VALUES (%u,%u,%d,%d,%d,%lld,%lld,%lld,%lld)", aid, vnum, m.collected, m.kills, m.stars, m.teleport, m.polymorph, m.summon, m.recruit);
	}

	void SaveSet(DWORD aid, const SAccount& s, DWORD vnum)
	{
		std::map<DWORD, int>::const_iterator it = s.rank.find(vnum);
		DBManager::instance().DirectQuery("REPLACE INTO player.nowy_karty_osiagniecia (account_id,vnum,zalozone,ranga) VALUES (%u,%u,%d,%d)",
				aid, vnum, s.worn.count(vnum) ? 1 : 0, it == s.rank.end() ? 0 : it->second);
	}

	void SaveCounters(DWORD aid, SAccount& s)
	{
		for (std::map<DWORD, SMonster>::const_iterator it = s.monsters.begin(); it != s.monsters.end(); ++it)
			SaveMonster(aid, it->first, it->second);
		s.dirty = false;
		s.savedAt = get_global_time();
	}

	void Load(DWORD aid, SAccount& s)
	{
		{
			std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("SELECT glowny0,glowny1,glowny2,"
					"talia0,talia1,talia2,talia3,talia4,talia5,talia6,talia7,talia8,talia9,talia10,talia11,talia12,talia13,talia14,talia15,"
					"zabity0,zabity1,zabity2,poziom,reset_misji,reset_kolejnosci,okno_start,okno_liczba "
					"FROM player.nowy_karty_misja WHERE account_id=%u", aid));
			if (msg && msg->Get() && msg->Get()->uiNumRows > 0)
			{
				MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
				int i = 0;
				for (int j = 0; j < TARGETS; ++j)
					s.targets[j] = (DWORD) Number(row[i++]);
				for (int j = 0; j < DECK_SIZE; ++j)
					s.deck[j] = (DWORD) Number(row[i++]);
				for (int j = 0; j < TARGETS; ++j)
					s.killed[j] = Number(row[i++]) ? 1 : 0;
				s.level = (int) Number(row[i++]);
				s.missionReset = Number(row[i++]);
				s.shuffleAt = Number(row[i++]);
				s.windowStart = Number(row[i++]);
				s.windowCount = (int) Number(row[i++]);
				if (s.level < 0 || s.level > LEVELS)
					s.level = 0;
			}
		}
		{
			std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("SELECT vnum,zebrane,zabicia,gwiazdki,teleport,przemiana,przywolanie,rekrutacja "
					"FROM player.nowy_karty_status WHERE account_id=%u", aid));
			if (msg && msg->Get() && msg->Get()->uiNumRows > 0)
			{
				while (MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult))
				{
					SMonster m;
					m.collected = (int) Number(row[1]);
					m.kills = (int) Number(row[2]);
					m.stars = std::max(0, std::min<int>(STARS_MAX, (int) Number(row[3])));
					m.teleport = Number(row[4]);
					m.polymorph = Number(row[5]);
					m.summon = Number(row[6]);
					m.recruit = Number(row[7]);
					s.monsters[(DWORD) Number(row[0])] = m;
				}
			}
		}
		{
			std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("SELECT vnum,zalozone,ranga FROM player.nowy_karty_osiagniecia WHERE account_id=%u", aid));
			if (msg && msg->Get() && msg->Get()->uiNumRows > 0)
			{
				while (MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult))
				{
					const DWORD vnum = (DWORD) Number(row[0]);
					if (!Achievement(vnum))
						continue;
					if (Number(row[1]))
						s.worn.insert(vnum);
					if (Number(row[2]) > 0)
						s.rank[vnum] = (int) Number(row[2]);
				}
			}
		}
	}

	SAccount& State(LPCHARACTER ch)
	{
		const DWORD aid = ch->GetAID();
		std::map<DWORD, SAccount>::iterator it = s_accounts.find(aid);
		if (it != s_accounts.end())
			return it->second;
		SAccount& s = s_accounts[aid];
		Load(aid, s);
		return s;
	}

	// ---------------------------------------------------------------- the set bonuses (AFFECT_COLLECT)

	// As the horse's bonus (playerbot_horse30.h): what the sets worn give
	// against what this character was given (a quest flag a point), the
	// difference into the point's AFFECT_COLLECT. The progress is the
	// account's and the affects the character's - hence at login and after
	// every change. No account (NULL): every set bonus comes off (the system
	// switched off).
	void SyncBonuses(LPCHARACTER ch, const SAccount* s)
	{
		if (!ch->IsLoadedAffect())
		{
			s_needSync.insert(ch->GetPlayerID());
			return;
		}

		std::map<int, int> want;
		for (size_t i = 0; i < CountOf(ACHIEVEMENTS); ++i)
		{
			const SAchievement& a = ACHIEVEMENTS[i];
			for (int p = 0; p < 2; ++p)
				if (a.aiPoint[p])
					want[a.aiPoint[p]] += 0;		// every point of the table is brought in line, also back to zero
		}
		if (s)
		{
			for (std::set<DWORD>::const_iterator it = s->worn.begin(); it != s->worn.end(); ++it)
			{
				const SAchievement* a = Achievement(*it);
				std::map<DWORD, int>::const_iterator r = s->rank.find(*it);
				if (!a || r == s->rank.end())
					continue;
				const int bonus = RankBonus(*a, r->second);
				for (int p = 0; p < 2; ++p)
					if (a->aiPoint[p])
						want[a->aiPoint[p]] += bonus;
			}
		}

		for (std::map<int, int>::const_iterator it = want.begin(); it != want.end(); ++it)
		{
			char flag[48];
			snprintf(flag, sizeof(flag), "karty_bonus.p%d", it->first);
			const int given = ch->GetQuestFlag(flag);
			const int delta = it->second - given;
			if (!delta)
				continue;
			long value = delta;
			if (const CAffect* aff = ch->FindAffect(AFFECT_COLLECT, it->first))
				value += aff->lApplyValue;
			ch->AddAffect(AFFECT_COLLECT, it->first, value, 0, INFINITE_AFFECT_DURATION, 0, true, true);
			ch->SetQuestFlag(flag, it->second);
		}
		s_needSync.erase(ch->GetPlayerID());
	}

	// ---------------------------------------------------------------- to the client

	void Command(LPCHARACTER ch, const char* text)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM %s", text);
	}

	void SendMonster(LPCHARACTER ch, DWORD vnum, const SMonster& m)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM ADD_MOB_INFO/%u/%d/%d/%d/%d/%lld/%lld/%lld",
				vnum, m.collected, m.kills, Needed(m), m.stars, m.teleport, m.polymorph, m.summon);
	}

	void SendMonsters(LPCHARACTER ch, SAccount& s)
	{
		for (std::map<DWORD, SMonster>::const_iterator it = s.monsters.begin(); it != s.monsters.end(); ++it)
			SendMonster(ch, it->first, it->second);
		Command(ch, "ILLUSTRATION_READY");
	}

	int KilledCount(const SAccount& s)
	{
		int n = 0;
		for (int i = 0; i < TARGETS; ++i)
			n += s.killed[i] ? 1 : 0;
		return n;
	}

	bool DeckEmpty(const SAccount& s)
	{
		for (int i = 0; i < DECK_SIZE; ++i)
			if (s.deck[i])
				return false;
		return true;
	}

	bool HasTargets(const SAccount& s)
	{
		return s.targets[0] && s.targets[1] && s.targets[2];
	}

	// ---------------------------------------------------------------- missions

	void NewMission(LPCHARACTER ch, SAccount& s)
	{
		if (s.level >= LEVELS)
			s.level = 0;
		for (int i = 0; i < DECK_SIZE; ++i)
			s.deck[i] = LEVEL_POOL[s.level][i];
		Shuffle(s.deck, DECK_SIZE);
		memset(s.targets, 0, sizeof(s.targets));
		++s.level;

		ch->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM ADD_DATA/Level/%d", s.level);
		std::string data = "NEW_MISSION";
		for (int i = 0; i < DECK_SIZE; ++i)
		{
			char b[16];
			snprintf(b, sizeof(b), "/%u", s.deck[i]);
			data += b;
		}
		Command(ch, data.c_str());
		SaveMission(ch->GetAID(), s);
	}

	// The package drew from an empty pool for ever (level 9 and up of its
	// table had none) - here a level with fewer than three monsters left
	// deals the next level, and a pool that still has none gives up.
	void DrawTargets(LPCHARACTER ch, SAccount& s)
	{
		if (HasTargets(s))
		{
			Command(ch, "NO_NEW_MISSION");
			return;
		}
		if (KilledCount(s) >= TARGETS)
			return;

		std::vector<int> slots;
		for (int i = 0; i < DECK_SIZE; ++i)
			if (s.deck[i])
				slots.push_back(i);
		if ((int) slots.size() < TARGETS)
		{
			NewMission(ch, s);
			slots.clear();
			for (int i = 0; i < DECK_SIZE; ++i)
				if (s.deck[i])
					slots.push_back(i);
			if ((int) slots.size() < TARGETS)
				return;
		}
		for (int i = (int) slots.size() - 1; i > 0; --i)
			std::swap(slots[i], slots[number(0, i)]);

		for (int i = 0; i < TARGETS; ++i)
			s.targets[i] = s.deck[slots[i]];
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM REC_MAINCARDS/%u/%u/%u/%d/%d/%d",
				s.targets[0], s.targets[1], s.targets[2], slots[0], slots[1], slots[2]);
		for (int i = 0; i < TARGETS; ++i)
			s.deck[slots[i]] = 0;
		SaveMission(ch->GetAID(), s);
	}

	void SendState(LPCHARACTER ch, SAccount& s)
	{
		if (s.level != 0)
			ch->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM ADD_DATA/Level/%d", s.level);
		if (DeckEmpty(s) && !HasTargets(s))
			NewMission(ch, s);

		std::string data = "ADD_DATA/Cards";
		char b[16];
		for (int i = 0; i < TARGETS; ++i)
		{
			snprintf(b, sizeof(b), "/%u", s.targets[i]);
			data += b;
		}
		for (int i = 0; i < DECK_SIZE; ++i)
		{
			snprintf(b, sizeof(b), "/%u", s.deck[i]);
			data += b;
		}
		for (int i = 0; i < TARGETS; ++i)
		{
			snprintf(b, sizeof(b), "/%d", s.killed[i]);
			data += b;
		}
		Command(ch, data.c_str());
		if (KilledCount(s) == TARGETS)
			Command(ch, "SUCCES_MISSION");
		Command(ch, "OPEN");
		SendMonsters(ch, s);

		for (std::set<DWORD>::const_iterator it = s.worn.begin(); it != s.worn.end(); ++it)
			ch->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM ACHIEV_APPLY/%u/1", *it);
		for (std::map<DWORD, int>::const_iterator it = s.rank.begin(); it != s.rank.end(); ++it)
			ch->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM ACHIEV_REGIST/%u/%d", it->first, it->second);
	}

	void MissionReward(LPCHARACTER ch, SAccount& s)
	{
		if (KilledCount(s) != TARGETS)
		{
			Command(ch, "NOT_ALL_MONSTERS_KILLED");
			return;
		}
		LPITEM item = ITEM_MANAGER::instance().CreateItem(CARD_MISSION, 1, 0, true);
		if (!item)
		{
			sys_err("MONSTER_CARDS: no item %u in item_proto", CARD_MISSION);
			return;
		}
		item->SetSocket(1, s.targets[number(0, TARGETS - 1)]);
		ch->AutoGiveItem(item);

		memset(s.killed, 0, sizeof(s.killed));
		NewMission(ch, s);
	}

	void MissionReset(LPCHARACTER ch, SAccount& s)
	{
		if (!HasTargets(s))
			return;
		const long long now = get_global_time();
		if (s.windowStart && now - s.windowStart >= RESET_WINDOW)
		{
			s.windowStart = 0;
			s.windowCount = 0;
		}
		if (s.windowCount >= 1)
		{
			if (ch->CountSpecifyItem(CARD_RESET) < 1)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_NO_RESET);
				return;
			}
			ch->RemoveSpecifyItem(CARD_RESET, 1);
		}
		if (!s.windowCount)
			s.windowStart = now;
		++s.windowCount;
		s.missionReset = now;
		s.level = 0;
		memset(s.killed, 0, sizeof(s.killed));
		NewMission(ch, s);
	}

	void MissionShuffle(LPCHARACTER ch, SAccount& s)
	{
		if (!HasTargets(s) || KilledCount(s) == TARGETS)
			return;
		if (ch->CountSpecifyItem(CARD_SHUFFLE) < 1)
		{
			Command(ch, "MISSION_FAIL/0/0");
			ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_NO_SHUFFLE);
			return;
		}
		ch->RemoveSpecifyItem(CARD_SHUFFLE, 1);
		s.shuffleAt = get_global_time();
		memset(s.targets, 0, sizeof(s.targets));
		memset(s.killed, 0, sizeof(s.killed));
		SaveMission(ch->GetAID(), s);
		SendState(ch, s);
	}

	void DropCard(LPCHARACTER ch, DWORD race)
	{
		LPITEM item = ITEM_MANAGER::instance().CreateItem(CARD_MISSION, 1, 0, true);
		if (!item)
			return;
		item->SetSocket(1, race);
		PIXEL_POSITION pos;
		pos.x = ch->GetX() + number(-200, 200);
		pos.y = ch->GetY() + number(-200, 200);
		item->SetOwnership(ch, 120);
		if (!item->AddToGround(ch->GetMapIndex(), pos))
		{
			M2_DESTROY_ITEM(item);
			return;
		}
		item->StartDestroyEvent();
	}

	// ---------------------------------------------------------------- cards and stars

	bool UseCard(LPCHARACTER ch, SAccount& s, LPITEM item)
	{
		DWORD mob = 0;
		if (item->GetVnum() == CARD_TRADABLE)
		{
			mob = (DWORD) item->GetSocket(0);
			if (!mob)
				mob = (DWORD) item->GetSocket(1);
		}
		else
		{
			mob = (DWORD) item->GetSocket(1);
			if (!mob)
				mob = (DWORD) item->GetSocket(0);
		}
		if (!mob)
			mob = RandomMonster();			// a card made with /i: some monster of the pools
		if (!mob || !IsCardMonster(mob))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_NO_MONSTER);
			return false;
		}

		SMonster& m = s.monsters[mob];
		if (m.stars >= STARS_MAX)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_MAX);
			return false;
		}
		const int needed = Needed(m);
		if (m.collected >= needed)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_FULL);
			return false;
		}
		++m.collected;
		SaveMonster(ch->GetAID(), mob, m);
		SendMonster(ch, mob, m);
		const CMob* pkMob = CMobManager::instance().Get(mob);
		ch->ChatPacket(CHAT_TYPE_INFO, MSG_CARD_USED, pkMob ? pkMob->m_table.szLocaleName : "?", m.collected, needed);
		return true;
	}

	void Promote(LPCHARACTER ch, SAccount& s, DWORD mob)
	{
		SMonster& m = s.monsters[mob];
		if (m.stars >= STARS_MAX)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_MAX);
			return;
		}
		if (m.collected < Needed(m))
		{
			Command(ch, "NO_PROMOTION");
			return;
		}
		++m.stars;
		m.collected = 0;
		SaveMonster(ch->GetAID(), mob, m);
		SendMonster(ch, mob, m);
	}

	// ---------------------------------------------------------------- the star functions

	bool Waiting(LPCHARACTER ch, long long last, int wait)
	{
		if (last <= 0)
			return false;
		const long long left = last + wait - get_global_time();
		if (left <= 0)
			return false;
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM NO_NEW_ORDER/%lld", left);
		return true;
	}

	bool TooFewStars(LPCHARACTER ch, const SMonster& m, int needed)
	{
		if (m.stars >= needed)
			return false;
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM NO_NEED_STAGE/%d", needed);
		return true;
	}

	bool CanMove(LPCHARACTER ch)
	{
		return !(ch->IsDead() || ch->IsWarping() || !ch->CanWarp() || ch->GetExchange() || ch->GetMyShop() || ch->GetShopOwner() ||
				ch->IsOpenSafebox() || ch->IsCubeOpen() || ch->GetDungeon() || ch->GetMapIndex() >= 10000);
	}

	// The kingdoms' own maps: a(1,3) Shinsoo, b(21,23) Chunjo, c(41,43) Jinno; the rest is everyone's.
	bool MapForEmpire(long map, int empire)
	{
		if (map == 1 || map == 3)
			return empire == 1;
		if (map == 21 || map == 23)
			return empire == 2;
		if (map == 41 || map == 43)
			return empire == 3;
		return true;
	}

	void FnTeleport(LPCHARACTER ch, SMonster& m, DWORD mob)
	{
		std::vector<const STeleport*> candidates;
		for (size_t i = 0; i < CountOf(TELEPORTS); ++i)
			if (TELEPORTS[i].dwVnum == mob && MapForEmpire(TELEPORTS[i].lMap, ch->GetEmpire()))
				candidates.push_back(&TELEPORTS[i]);
		if (candidates.empty())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_NO_TELEPORT);
			return;
		}
		if (!CanMove(ch))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_NOT_NOW);
			return;
		}
		const STeleport* t = candidates[number(0, (int) candidates.size() - 1)];
		PIXEL_POSITION base;
		if (!SECTREE_MANAGER::instance().GetMapBasePositionByMapIndex(t->lMap, base))
		{
			sys_err("MONSTER_CARDS: %s: teleport to a map this world lacks (%ld)", ch->GetName(), t->lMap);
			return;
		}
		const long anchorX = base.x + t->lX * 100;
		const long anchorY = base.y + t->lY * 100;
		PIXEL_POSITION pos;
		bool ok;
		if (SECTREE_MANAGER::instance().GetRandomLocation(t->lMap, pos, (DWORD) anchorX, (DWORD) anchorY, 2000))
			ok = ch->WarpSet(pos.x, pos.y);
		else
			ok = ch->WarpSet(anchorX, anchorY);
		if (!ok)
			return;
		m.teleport = get_global_time();
	}

	void FnPolymorph(LPCHARACTER ch, SMonster& m, DWORD mob)
	{
		if (ch->GetMapIndex() == OX_MAP)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_NO_POLY_OX);
			return;
		}
		if (ch->IsPolymorphed() || ch->IsRiding())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_NO_POLY);
			return;
		}
		const CMob* pkMob = CMobManager::instance().Get(mob);
		if (!pkMob)
			return;
		const int limit = MAX(0, 20 - ch->GetLevel() * 3 / 10);
		if (pkMob->m_table.bLevel >= ch->GetLevel() + limit)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_NO_POLY);
			return;
		}
		const int skill = ch->GetSkillLevel(129);		// POLYMORPH_SKILL_ID
		int minutes = skill == 0 ? 5 : (5 + (5 + skill / 40 * 25));
		ch->AddAffect(AFFECT_POLYMORPH, POINT_POLYMORPH, (long) mob, AFF_POLYMORPH, minutes * 60, 0, true);
		m.polymorph = get_global_time();
	}

	void FnDropList(LPCHARACTER ch, SAccount& s, DWORD mob)
	{
		const long long now = get_global_time();
		if (now - s.dropListAt < DROP_LIST_GAP)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_EVERY_3S);
			return;
		}
		// The drop wiki's rows (MT2009_PLUS_DROP_WIKI_V1): every drop table of the monster, the likeliest first.
		std::vector<TDropWikiRow> rows;
		ITEM_MANAGER::instance().GetDropWikiRows(mob, rows);
		if (rows.empty())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_NO_DROP);
			return;
		}
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM NEW_DROPP_GUI/%u", mob);
		const size_t shown = MIN(rows.size(), DROP_LIST_ROWS);
		for (size_t i = 0; i < shown; ++i)
			ch->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM ADD_DROPP/%u/%d/%.4f/%d", rows[i].dwVnum, rows[i].iCountMin,
					rows[i].dChance * 100.0, rows[i].iCountMax);
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM OPEN_DROPP_GUI/%u", (unsigned) (rows.size() - shown));
		s.dropListAt = now;
	}

	void FnSummon(LPCHARACTER ch, SMonster& m, DWORD mob)
	{
		if (!CanMove(ch))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_NOT_NOW);
			return;
		}
		LPCHARACTER monster = CHARACTER_MANAGER::instance().SpawnMobRange(mob, ch->GetMapIndex(), ch->GetX() - number(200, 750),
				ch->GetY() - number(200, 750), ch->GetX() + number(200, 750), ch->GetY() + number(200, 750), true);
		if (!monster)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_NO_SPAWN);
			return;
		}
		m.summon = get_global_time();
	}

	// The monster goes after a player named by the caller. The bots are the
	// only other "players" of this world - never their target (open: the
	// owner's call whether the recruitment stays at all).
	void FnRecruit(LPCHARACTER ch, SAccount& s, SMonster& m, DWORD mob, const std::string& name)
	{
		LPCHARACTER victim = CHARACTER_MANAGER::instance().FindPC(name.c_str());
		if (!victim || victim == ch || !victim->IsPC() || IsBot(victim))
		{
			Command(ch, "PLAYER_DONT_EXIST");
			return;
		}
		LPCHARACTER monster = CHARACTER_MANAGER::instance().SpawnMobRange(mob, victim->GetMapIndex(), victim->GetX() - number(200, 750),
				victim->GetY() - number(200, 750), victim->GetX() + number(200, 750), victim->GetY() + number(200, 750), true);
		if (!monster)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_NO_SPAWN);
			return;
		}
		monster->Follow(victim, 0.0f);
		monster->BeginFight(victim);
		m.recruit = get_global_time();
		s.recruitVid = monster->GetVID();
	}

	// "Wymien" in the window: TRADE_COST collected cards of the monster -> one tradable card (50284).
	// (The package counted cards in the bag by socket 0, where the mission cards never have their monster.)
	void FnTrade(LPCHARACTER ch, SMonster& m, DWORD mob)
	{
		if (m.collected < TRADE_COST)
		{
			Command(ch, "NOT_ENOUGH_FOR_TRADE");
			return;
		}
		LPITEM card = ITEM_MANAGER::instance().CreateItem(CARD_TRADABLE, 1, 0, true);
		if (!card)
			return;
		card->SetSocket(0, mob);
		m.collected -= TRADE_COST;
		ch->AutoGiveItem(card);
	}

	void Function(LPCHARACTER ch, SAccount& s, const std::vector<std::string>& t)
	{
		if (t.size() < 3)
			return;
		const int fn = atoi(t[1].c_str());
		const DWORD mob = (DWORD) strtoul(t[2].c_str(), NULL, 10);

		if (fn == FN_DROP && mob == 0)		// the window asks for the collection
		{
			SendMonsters(ch, s);
			return;
		}
		std::map<DWORD, SMonster>::iterator it = s.monsters.find(mob);
		if (it == s.monsters.end())
			return;
		SMonster& m = it->second;

		bool save = true;
		switch (fn)
		{
			case FN_TELEPORT:
				if (Waiting(ch, m.teleport, WAIT_TELEPORT) || TooFewStars(ch, m, 4))
					return;
				FnTeleport(ch, m, mob);
				break;
			case FN_POLYMORPH:
				if (Waiting(ch, m.polymorph, WAIT_POLYMORPH) || TooFewStars(ch, m, 3))
					return;
				FnPolymorph(ch, m, mob);
				break;
			case FN_DROP:
				if (TooFewStars(ch, m, 1))
					return;
				FnDropList(ch, s, mob);
				save = false;
				break;
			case FN_SUMMON:
				if (Waiting(ch, m.summon, WAIT_SUMMON) || TooFewStars(ch, m, 5))
					return;
				FnSummon(ch, m, mob);
				break;
			case FN_RECRUIT:
				// The owner, 5 October: the recruitment is off - here every other
				// "player" is a bot, and bots are never its target anyway.
				ch->ChatPacket(CHAT_TYPE_INFO, "Rekrutacja potworow jest wylaczona.");
				return;
			case FN_TRADE:
				FnTrade(ch, m, mob);
				break;
			case FN_PROMOTE:
				Promote(ch, s, mob);
				save = false;
				break;
			default:
				return;
		}
		if (save)
		{
			SaveMonster(ch->GetAID(), mob, m);
			SendMonster(ch, mob, m);
		}
	}

	void WarpToRecruit(LPCHARACTER ch, SAccount& s)
	{
		LPCHARACTER monster = s.recruitVid ? CHARACTER_MANAGER::instance().Find(s.recruitVid) : NULL;
		if (!monster || monster->IsDead() || monster->GetMapIndex() != ch->GetMapIndex())
		{
			Command(ch, "MOB_IS_ALREADY_DEAD");
			return;
		}
		if (!CanMove(ch))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_NOT_NOW);
			return;
		}
		ch->WarpSet(monster->GetX() + 100, monster->GetY() + 100);
	}

	// ---------------------------------------------------------------- the sets

	int MinimumStars(SAccount& s, const SAchievement& a)
	{
		int minimum = STARS_MAX + 1;
		for (int i = 0; i < 8; ++i)
		{
			if (!a.adwMonsters[i])
				continue;
			std::map<DWORD, SMonster>::const_iterator it = s.monsters.find(a.adwMonsters[i]);
			minimum = std::min<int>(minimum, it == s.monsters.end() ? 0 : it->second.stars);
		}
		return minimum > STARS_MAX ? 0 : minimum;
	}

	// A field set at a time: wearing one takes the other field sets off.
	void TakeOffFieldSetsBut(LPCHARACTER ch, SAccount& s, DWORD keep)
	{
		const SAchievement* kept = Achievement(keep);
		for (std::set<DWORD>::iterator it = s.worn.begin(); it != s.worn.end();)
		{
			const SAchievement* a = Achievement(*it);
			if (a && a->iType == 0 && *it != keep && kept && kept->iType == 0)
			{
				const DWORD off = *it;
				s.worn.erase(it++);
				SaveSet(ch->GetAID(), s, off);
				ch->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM ACHIEV_APPLY/%u/0", off);
			}
			else
				++it;
		}
	}

	void SetToggle(LPCHARACTER ch, SAccount& s, const std::vector<std::string>& t)
	{
		if (t.size() < 2)
			return;
		const DWORD vnum = (DWORD) strtoul(t[1].c_str(), NULL, 10);
		const SAchievement* a = Achievement(vnum);
		std::map<DWORD, int>::const_iterator r = s.rank.find(vnum);
		if (!a || r == s.rank.end() || r->second <= 0)
			return;

		if (s.worn.count(vnum))
			s.worn.erase(vnum);
		else
		{
			TakeOffFieldSetsBut(ch, s, vnum);
			s.worn.insert(vnum);
		}
		SaveSet(ch->GetAID(), s, vnum);
		SyncBonuses(ch, &s);
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM ACHIEV_APPLY/%u/%d", vnum, s.worn.count(vnum) ? 1 : 0);
	}

	// Every monster of the set needs at least `rank` stars, and the registration takes them
	// (the package let a rank 5 in for one star).
	void SetRegister(LPCHARACTER ch, SAccount& s, const std::vector<std::string>& t)
	{
		if (t.size() < 3)
			return;
		const DWORD vnum = (DWORD) strtoul(t[1].c_str(), NULL, 10);
		const int rank = atoi(t[2].c_str());
		const SAchievement* a = Achievement(vnum);
		if (!a || rank < FirstRank(*a) || rank > 5)
			return;
		if (MinimumStars(s, *a) < rank)
			return;

		for (int i = 0; i < 8; ++i)
		{
			if (!a->adwMonsters[i])
				continue;
			SMonster& m = s.monsters[a->adwMonsters[i]];
			m.stars -= rank;
			m.collected = 0;
			SaveMonster(ch->GetAID(), a->adwMonsters[i], m);
			SendMonster(ch, a->adwMonsters[i], m);
		}
		s.rank[vnum] = rank;
		TakeOffFieldSetsBut(ch, s, vnum);
		s.worn.insert(vnum);
		SaveSet(ch->GetAID(), s, vnum);
		SyncBonuses(ch, &s);
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM ACHIEV_APPLY/%u/1", vnum);
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM ACHIEV_REGIST/%u/%d", vnum, rank);
	}

	// ---------------------------------------------------------------- GM commands (tests)

	void GmCommand(LPCHARACTER ch, SAccount& s, const std::vector<std::string>& t)
	{
		const DWORD aid = ch->GetAID();
		const std::string& what = t[0];
		if (what == "gm_misja")				// the mission's targets all killed
		{
			if (!HasTargets(s))
				DrawTargets(ch, s);
			if (HasTargets(s))
			{
				for (int i = 0; i < TARGETS; ++i)
					s.killed[i] = 1;
				SaveMission(aid, s);
				SendState(ch, s);
			}
		}
		else if (what == "gm_gwiazdki")		// every monster of the pools at n stars: /cardmonster gm_gwiazdki 5
		{
			const int n = t.size() > 1 ? std::max(0, std::min(STARS_MAX, atoi(t[1].c_str()))) : STARS_MAX;
			for (int l = 0; l < LEVELS; ++l)
				for (int i = 0; i < DECK_SIZE; ++i)
					if (LEVEL_POOL[l][i])
					{
						SMonster& m = s.monsters[LEVEL_POOL[l][i]];
						m.stars = n;
						m.collected = 0;
					}
			SaveCounters(aid, s);
			SendMonsters(ch, s);
		}
		else if (what == "gm_karta" && t.size() > 2)	// /cardmonster gm_karta <monster> <cards towards the next star>
		{
			const DWORD mob = (DWORD) strtoul(t[1].c_str(), NULL, 10);
			if (IsCardMonster(mob))
			{
				SMonster& m = s.monsters[mob];
				m.collected = std::max(0, std::min(Needed(m), atoi(t[2].c_str())));
				SaveMonster(aid, mob, m);
				SendMonster(ch, mob, m);
			}
		}
		else if (what == "gm_reset")			// the account's whole progress gone
		{
			DBManager::instance().DirectQuery("DELETE FROM player.nowy_karty_osiagniecia WHERE account_id=%u", aid);
			DBManager::instance().DirectQuery("DELETE FROM player.nowy_karty_status WHERE account_id=%u", aid);
			DBManager::instance().DirectQuery("DELETE FROM player.nowy_karty_misja WHERE account_id=%u", aid);
			s_accounts.erase(aid);
			SAccount& fresh = State(ch);
			SyncBonuses(ch, &fresh);
			SendState(ch, fresh);
		}
		else if (what == "gm_przeladuj")		// the account's progress read again
		{
			s_accounts.erase(aid);
			SAccount& fresh = State(ch);
			SyncBonuses(ch, &fresh);
			SendState(ch, fresh);
		}
		else if (what == "gm_info")
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "karty: konto %u, poziom %d, cele %u/%u/%u, zabite %d, potwory %d, zestawy zalozone %d",
					aid, s.level, s.targets[0], s.targets[1], s.targets[2], KilledCount(s), (int) s.monsters.size(), (int) s.worn.size());
		}
	}
}

// ---------------------------------------------------------------- the engine's calls (server-patches/monstercard)

// char_battle.cpp, after KillLog: a monster of the pools - its card at 5% and the mission.
void MonsterCardOnKill(LPCHARACTER killer, LPCHARACTER victim)
{
	using namespace mt2009_mcard;
	if (!Counts(killer) || !victim || victim->IsPC())
		return;
	if (!Enabled())
	{
		if (s_needSync.erase(killer->GetPlayerID()))
			SyncBonuses(killer, NULL);
		return;
	}
	if (!IsCardMonster(victim->GetRaceNum()))
		return;

	SAccount& s = State(killer);
	if (s_needSync.erase(killer->GetPlayerID()))
		SyncBonuses(killer, &s);

	const DWORD race = victim->GetRaceNum();
	if (number(1, 100) <= DROP_PERCENT)
		DropCard(killer, race);

	SMonster& m = s.monsters[race];
	++m.kills;

	bool save = false;
	for (int i = 0; i < TARGETS; ++i)
	{
		if (s.targets[i] != race || s.killed[i])
			continue;
		s.killed[i] = 1;
		save = true;
		killer->ChatPacket(CHAT_TYPE_COMMAND, "MONSTERCARDSYSTEM SUCCES_KILL/%d", i);
		const CMob* pkMob = CMobManager::instance().Get(race);
		killer->ChatPacket(CHAT_TYPE_INFO, MSG_KILLED, pkMob ? pkMob->m_table.szLocaleName : "?", s.level);
		if (KilledCount(s) == TARGETS)
		{
			Command(killer, "SUCCES_MISSION");
			killer->ChatPacket(CHAT_TYPE_INFO, MSG_MISSION_DONE, s.level);
		}
		break;
	}

	if (save)
	{
		SaveMission(killer->GetAID(), s);
		SaveMonster(killer->GetAID(), race, m);
		SendMonster(killer, race, m);
		s.dirty = false;
		s.savedAt = get_global_time();
	}
	else
	{
		s.dirty = true;
		if (get_global_time() - s.savedAt >= SAVE_EVERY)
			SaveCounters(killer->GetAID(), s);
	}
}

// char_item.cpp, UseItemEx: -1 = not a card of ours, 0 = ours and not used, 1 = used up.
int MonsterCardUseItem(LPCHARACTER ch, LPITEM item)
{
	using namespace mt2009_mcard;
	if (!item || (item->GetVnum() != CARD_MISSION && item->GetVnum() != CARD_TRADABLE))
		return -1;
	if (!Counts(ch))
		return 0;
	if (!Enabled())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "%s", MSG_DISABLED);
		return 0;
	}
	if (!UseCard(ch, State(ch), item))
		return 0;
	item->SetCount(item->GetCount() - 1);
	return 1;
}

// input_login.cpp, entering the game: the account's set bonuses on this character
// (with the system off: taken off).
void MonsterCardOnLogin(LPCHARACTER ch)
{
	using namespace mt2009_mcard;
	if (!Counts(ch))
		return;
	SyncBonuses(ch, Enabled() ? &State(ch) : NULL);
}

// "/cardmonster <command> [args]": the card window (client) and the GM's "gm_*".
ACMD(do_cardmonster)
{
	using namespace mt2009_mcard;
	if (!Counts(ch))
		return;
	if (!Enabled())
	{
		Command(ch, "DISABLED");
		return;
	}
	const std::vector<std::string> t = Tokens(argument);
	if (t.empty())
		return;

	SAccount& s = State(ch);
	if (!isdigit((unsigned char) t[0][0]))
	{
		if (ch->GetGMLevel() >= GM_HIGH_WIZARD)
		{
			GmCommand(ch, s, t);
			SyncBonuses(ch, &State(ch));
		}
		return;
	}

	switch (atoi(t[0].c_str()))
	{
		case CMD_STATE:
			SendState(ch, s);
			break;
		case CMD_TARGETS:
			if (DeckEmpty(s) && !HasTargets(s))
				NewMission(ch, s);
			DrawTargets(ch, s);
			break;
		case CMD_NEW:
			if (DeckEmpty(s) && !HasTargets(s))		// the next level comes with a reward or a reset only
				NewMission(ch, s);
			break;
		case CMD_REWARD:
			MissionReward(ch, s);
			break;
		case CMD_SHUFFLE:
			MissionShuffle(ch, s);
			break;
		case CMD_RESET:
			MissionReset(ch, s);
			break;
		case CMD_FUNCTION:
			Function(ch, s, t);
			break;
		case CMD_WARP_TO_RECRUIT:
			WarpToRecruit(ch, s);
			break;
		case CMD_SET_TOGGLE:
			SetToggle(ch, s, t);
			break;
		case CMD_SET_REGISTER:
			SetRegister(ch, s, t);
			break;
		default:
			break;
	}
}

#endif
