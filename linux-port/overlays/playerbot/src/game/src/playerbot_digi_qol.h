// MT2009 PLUS - Digi Rasta's server conveniences, MT2009_PLUS_DIGI_SERVER_QOL_V1.
// Autor: Digi Rasta (package "nowy-system" v0.23.0, the "Biore" systems of
// 3 October: nowy_system.cpp, nowy_system_biore.h, zastosuj.py). Ported as our
// own code: no hooks script, the engine calls in through
// server-patches/digirasta-qol (edits.json), the client's side is ordinary
// files of the client root (digiserverqol.py, uikillbar.py, uiskillbookexchange.py,
// game.py, uichat.py, uirestart.py, uichestpreview.py).
//
// What the engine calls (each declared where it is called, as the other
// overlays are):
//  - Mt2009DigiRefineFailNoted / Mt2009DigiRefineFailReport: why a refine
//    failed. NotifyRefineFail (char_item.cpp) notes the item before it is
//    changed, CInputMain::Refine (input_main.cpp) compares the cell after the
//    refine and sends "RefineFailedType <0 lost a level | 1 destroyed or one
//    piece of a stack gone | 2 untouched>" - game.py shows the reason.
//  - Mt2009DigiBlocked: the messenger's block list stops a trade, a party
//    invitation, a guild invitation, an emote for two and a duel request,
//    both ways (I blocked him / he blocked me). A GM is not stopped by
//    someone else's block, as with whispers.
//  - Mt2009DigiLevelUp: a real player's congratulation in the chat at every
//    level and "[Awans] <name> zdobywa N. poziom!" to everybody at every tenth.
//    Bots never - two thousand of them would fill the chat.
//  - Mt2009DigiDailyGift: at login, once per 24 h from level 10 - Yang =
//    level x 500 and 20 Red + 20 Blue Potions (D) (27003, 27006). Players
//    only. The quest flag digi_qol.daily_gift keeps the time of the last one;
//    it is read only once the player's quest flags are loaded (a short event
//    waits for them), so a slow quest load never hands out a second gift.
//  - Mt2009DigiKillBar: a kill of a character by a character -
//    "KillBar <killer race> <weapon subtype> <victim race> <killer> <victim>"
//    to every real player on that map (uikillbar.py, top right). Only kills
//    with a real player on one side: the kingdoms' bots fight each other all
//    day long, and two thousand of them would bury the bar (decision of the
//    port, README.md).
//  - Mt2009DigiKillSound: a real player's kill of a character or a boss -
//    "KillSound <1..13>" to the killer; the streak grows when the next such
//    kill comes within 10 s (an ordinary monster does not count, it would
//    be every blow).
//  - Mt2009DigiDeathCooldowns: a real player's skills are ready again after
//    death (the official servers do it); "SkillCoolTimeReset" lets the client
//    clear its own timers (player.ResetSkillCoolTimes in an exe that has it).
//    Bots keep their cooldowns - their fighting stays as it was tuned.
//  - Mt2009DigiDeadTime: "DeadTime <here> <town>", the seconds before
//    /restart_here and /restart_town are let through (do_restart: 10 and 7,
//    the portal limit when IsHack) - the death window counts down on its
//    buttons (uirestart.py).
//  - do_nowy_ksiegi, "/nowy_ksiegi <10 cells>": the book exchange at
//    Seon-Hae (quest ksiegi_seonhae, 20095): ten skill books of any kind +
//    250 000 Yang = a random "Instr." book (50400 + skill) of the player's
//    own class and skill group, only the ones our item_proto has. Everything
//    is checked here again: the NPC by the vid the quest wrote (same map,
//    25 m, talked to in the last 10 minutes), no other window, the ten
//    distinct cells, each a skill book that is not locked or traded, the
//    Yang. The Yang goes through PlayerBotChangeGold (mt2009 refuses
//    PointChange(POINT_GOLD)).
//    MT2009_PLUS_BOOK_EXCHANGE_V2: "/nowy_ksiegi x<times> <cells...>" - up to
//    BOOK_MAX_CELLS cells, whole stacks counted (a stack of ten books is ten
//    books), <times> exchanges in one order (1..BOOK_MAX_TIMES; without the
//    "x" one, the old window's form). The books are taken from the cells in
//    their order, 10 x <times> pieces, never more than the cells hold; the
//    Yang is <times> x BOOK_COST. Everything is checked before anything is
//    taken, the room in the bag too (the stacks the new books join, the
//    cells the taken ones free) - nothing falls on the ground.
//    Answer: "NOWY_KSIEGI done <vnum> <count>", one per kind of book given
//    (uiskillbookexchange.py).
#include "messenger_manager.h"

namespace mt2009_digi_qol
{
	inline bool RealPlayer(LPCHARACTER ch)
	{
		return ch && ch->IsPC() && ch->GetDesc() && !ch->GetDesc()->IsBot();
	}

	// ---------------------------------------------------------- refine failure
	struct SRefineFail
	{
		DWORD vnum;
		ITEM_COUNT count;
		WORD cell;
		DWORD time;
	};
	std::map<DWORD, SRefineFail> s_mapRefineFail;

	enum
	{
		REFINE_FAIL_GRADE_DOWN = 0,	// the item lost a level
		REFINE_FAIL_DEL_ITEM = 1,	// the item (or one piece of its stack) is gone
		REFINE_FAIL_KEEP_GRADE = 2,	// the item is untouched
	};

	// ---------------------------------------------------------- level up, gift
	const int LEVEL_NOTICE_EVERY = 10;
	// The owner, 3 October: switched off for now, the code stays for later -
	// set to true to hand the daily gift out again.
	const bool GIFT_ENABLED = false;
	const int GIFT_MIN_LEVEL = 10;
	const int GIFT_YANG_PER_LEVEL = 500;
	const DWORD GIFT_POTIONS[] = { 27003, 27006 };	// Red / Blue Potion (D)
	const int GIFT_POTIONS_COUNT = 20;
	const int GIFT_EVERY_SECONDS = 24 * 60 * 60;
	const char* const GIFT_FLAG = "digi_qol.daily_gift";
	const int GIFT_WAIT_TRIES = 30;	// seconds the event waits for the quest flags

	// ---------------------------------------------------------- kill streak
	struct SKillStreak
	{
		int stage;
		int last;
	};
	std::map<DWORD, SKillStreak> s_mapKillStreak;
	const int STREAK_MAX = 13;
	const int STREAK_SECONDS = 10;

	// ---------------------------------------------------------- book exchange
	const DWORD BOOK_NPC = 20095;	// Seon-Hae
	const int BOOK_NPC_RANGE = 2500;
	const int BOOK_NPC_TALK_SECONDS = 600;
	const long long BOOK_COST = 250000;	// the owner, 3 October (was 1 000 000)
	const int BOOK_COUNT = 10;
	const DWORD BOOK_INSTR_BASE = 50400;	// "Instr." book = 50400 + skill vnum
	// MT2009_PLUS_BOOK_EXCHANGE_V2: the window's cells (5 x 4) and the most
	// exchanges one order may ask for (uiskillbookexchange.py has the same).
	const size_t BOOK_MAX_CELLS = 20;
	const int BOOK_MAX_TIMES = 100;

	// Gives the daily gift when it is due; false while the quest flags are not loaded yet.
	bool DailyGiftTry(LPCHARACTER ch)
	{
		quest::PC* pc = quest::CQuestManager::instance().GetPC(ch->GetPlayerID());
		if (!pc || !pc->IsLoaded())
			return false;
		if (ch->GetLevel() < GIFT_MIN_LEVEL)
			return true;

		const int now = get_global_time();
		const int last = ch->GetQuestFlag(GIFT_FLAG);
		if (last && now >= last && now - last < GIFT_EVERY_SECONDS)
			return true;
		ch->SetQuestFlag(GIFT_FLAG, now);

		const long long yang = (long long)ch->GetLevel() * GIFT_YANG_PER_LEVEL;
		PlayerBotChangeGold(ch, yang);
		for (size_t i = 0; i < sizeof(GIFT_POTIONS) / sizeof(GIFT_POTIONS[0]); ++i)
			ch->AutoGiveItem(GIFT_POTIONS[i], GIFT_POTIONS_COUNT);
		// cp1250: "Nastepny" with its e-ogonek
		ch->ChatPacket(CHAT_TYPE_INFO, "Prezent dzienny: %lld Yang i po %d mikstur (D). Nast" "\xea" "pny za 24 godziny.",
			yang, GIFT_POTIONS_COUNT);
		sys_log(0, "DIGI_QOL: daily gift %s lv %d yang %lld", ch->GetName(), ch->GetLevel(), yang);
		return true;
	}

	EVENTINFO(gift_event_info)
	{
		DWORD pid;
		int tries;

		gift_event_info() : pid(0), tries(0) {}
	};

	EVENTFUNC(gift_event)
	{
		gift_event_info* info = dynamic_cast<gift_event_info*>(event->info);
		if (!info)
			return 0;
		LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(info->pid);
		if (!RealPlayer(ch))
			return 0;
		if (DailyGiftTry(ch) || ++info->tries >= GIFT_WAIT_TRIES)
			return 0;
		return PASSES_PER_SEC(1);
	}
}

void Mt2009DigiRefineFailNoted(LPCHARACTER ch, LPITEM item)
{
	using namespace mt2009_digi_qol;
	if (!RealPlayer(ch) || !item || item->GetWindow() != INVENTORY)
		return;

	SRefineFail& f = s_mapRefineFail[ch->GetPlayerID()];
	f.vnum = item->GetVnum();
	f.count = item->GetCount();
	f.cell = item->GetCell();
	f.time = get_dword_time();
}

void Mt2009DigiRefineFailReport(LPCHARACTER ch)
{
	using namespace mt2009_digi_qol;
	if (!ch)
		return;

	std::map<DWORD, SRefineFail>::iterator it = s_mapRefineFail.find(ch->GetPlayerID());
	if (it == s_mapRefineFail.end())
		return;

	const SRefineFail f = it->second;
	s_mapRefineFail.erase(it);
	if (get_dword_time() - f.time > 10000)
		return;	// a failure from somewhere else long ago (a quest), not this refine

	LPITEM now = ch->GetInventoryItem(f.cell);
	int type = REFINE_FAIL_GRADE_DOWN;
	if (!now || (now->GetVnum() == f.vnum && now->GetCount() < f.count))
		type = REFINE_FAIL_DEL_ITEM;
	else if (now->GetVnum() == f.vnum)
		type = REFINE_FAIL_KEEP_GRADE;

	ch->ChatPacket(CHAT_TYPE_COMMAND, "RefineFailedType %d", type);
}

bool Mt2009DigiBlocked(LPCHARACTER ch, LPCHARACTER tch)
{
	if (!ch || !tch || ch == tch || !ch->IsPC() || !tch->IsPC())
		return false;

	MessengerManager& mm = MessengerManager::instance();
	// cp1250: the Polish letters of "liscie" and "zaproszen".
	if (mm.IsBlock(ch->GetPlayerID(), tch->GetName()))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "%s: ten gracz jest na Twojej li" "\x9c" "cie zablokowanych.", tch->GetName());
		return true;
	}
	if (!ch->IsGM() && mm.IsBlock(tch->GetPlayerID(), ch->GetName()))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "%s: ten gracz nie przyjmuje od Ciebie zaprosze" "\xf1" ".", tch->GetName());
		return true;
	}
	return false;
}

void Mt2009DigiLevelUp(LPCHARACTER ch, int oldLevel)
{
	using namespace mt2009_digi_qol;
	if (!RealPlayer(ch))
		return;

	const int level = ch->GetLevel();
	if (level <= oldLevel)
		return;

	ch->ChatPacket(CHAT_TYPE_INFO, "Gratulacje - %d. poziom!", level);
	// Several levels at once (a GM's /level) announce the last tenth they passed.
	const int tenth = level / LEVEL_NOTICE_EVERY * LEVEL_NOTICE_EVERY;
	if (tenth > oldLevel && tenth > 0)
	{
		char buf[256];
		snprintf(buf, sizeof(buf), "[Awans] %s zdobywa %d. poziom!", ch->GetName(), tenth);
		BroadcastNotice(buf);
	}
}

void Mt2009DigiDailyGift(LPCHARACTER ch)
{
	// MT2009_PLUS_DBDATA_STAMP_V1: this login hook also tells a person's client
	// which client files (database editor) the server has (playerbot_dbdata_stamp.h).
	Mt2009DbDataStampOnLogin(ch);
	if (!mt2009_digi_qol::GIFT_ENABLED)
		return;
	using namespace mt2009_digi_qol;
	if (!RealPlayer(ch))
		return;
	if (DailyGiftTry(ch))
		return;
	gift_event_info* info = AllocEventInfo<gift_event_info>();
	info->pid = ch->GetPlayerID();
	event_create(gift_event, info, PASSES_PER_SEC(1));
}

void Mt2009DigiKillBar(LPCHARACTER killer, LPCHARACTER victim)
{
	using namespace mt2009_digi_qol;
	if (!killer || !victim || !killer->IsPC() || !victim->IsPC())
		return;
	if (!RealPlayer(killer) && !RealPlayer(victim))
		return;	// bot against bot: never shown

	LPITEM weapon = killer->GetWear(WEAR_WEAPON);
	char buf[160];
	snprintf(buf, sizeof(buf), "KillBar %u %d %u %s %s", (unsigned)killer->GetRaceNum(), weapon ? (int)weapon->GetSubType() : 255,
		(unsigned)victim->GetRaceNum(), killer->GetName(), victim->GetName());

	const long mapIndex = killer->GetMapIndex();
	const DESC_MANAGER::DESC_SET& descs = DESC_MANAGER::instance().GetClientSet();
	for (DESC_MANAGER::DESC_SET::const_iterator it = descs.begin(); it != descs.end(); ++it)
	{
		if ((*it)->IsBot())
			continue;
		LPCHARACTER ch = (*it)->GetCharacter();
		if (ch && ch->GetMapIndex() == mapIndex)
			ch->ChatPacket(CHAT_TYPE_COMMAND, "%s", buf);
	}
}

void Mt2009DigiKillSound(LPCHARACTER killer, LPCHARACTER victim)
{
	using namespace mt2009_digi_qol;
	if (!RealPlayer(killer) || !victim)
		return;
	if (!victim->IsPC() && victim->GetMobRank() < MOB_RANK_BOSS)
		return;

	const int now = get_global_time();
	SKillStreak& s = s_mapKillStreak[killer->GetPlayerID()];
	if (s.stage && now - s.last >= STREAK_SECONDS)
		s.stage = 0;
	if (s.stage < STREAK_MAX)
		++s.stage;
	s.last = now;
	killer->ChatPacket(CHAT_TYPE_COMMAND, "KillSound %d", s.stage);
}

void Mt2009DigiDeadTime(LPCHARACTER ch)
{
	using namespace mt2009_digi_qol;
	if (!RealPlayer(ch))
		return;
	// do_restart: here after 10 s (180 - 170), the town after 7 s (180 - 173),
	// both not before the portal limit while IsHack (a fight, a trade...).
	const int hack = ch->IsHack(false) ? g_nPortalLimitTime : 0;
	ch->ChatPacket(CHAT_TYPE_COMMAND, "DeadTime %d %d", MAX(10, hack), MAX(7, hack));
}

ACMD(do_nowy_ksiegi)
{
	using namespace mt2009_digi_qol;
	if (!RealPlayer(ch))
		return;

	// Seon-Hae: the vid the quest wrote, on this map, close enough, talked to lately.
	const DWORD vid = (DWORD)ch->GetQuestFlag("ksiegi_seonhae.npc_vid");
	const int talked = ch->GetQuestFlag("ksiegi_seonhae.npc_time");
	const int now = get_global_time();
	LPCHARACTER npc = vid ? CHARACTER_MANAGER::instance().Find(vid) : NULL;
	if (!npc || !npc->IsNPC() || npc->GetRaceNum() != BOOK_NPC || npc->GetMapIndex() != ch->GetMapIndex() ||
		!talked || now < talked || now - talked > BOOK_NPC_TALK_SECONDS ||
		DISTANCE_APPROX(ch->GetX() - npc->GetX(), ch->GetY() - npc->GetY()) > BOOK_NPC_RANGE)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Podejd" "\x9f" " do Seon-Hae.");
		return;
	}
	if (ch->IsDead() || !ch->CanHandleItem() || ch->GetExchange() || ch->GetShop() || ch->GetMyShop() ||
		ch->IsOpenSafebox() || ch->IsCubeOpen())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Zamknij inne okna (handel, sklep, magazyn).");
		return;
	}
	if (ch->GetSkillGroup() == 0 || ch->GetJob() > JOB_SHAMAN)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Najpierw wybierz drog" "\xea" " umiej" "\xea" "tno" "\x9c" "ci.");
		return;
	}

	// MT2009_PLUS_BOOK_EXCHANGE_V2: "x<times>" first (none: one exchange), then
	// the cells; whole stacks count, the books are taken in the cells' order.
	char arg[64];
	const char* rest = argument;
	int times = 1;
	{
		const char* next = one_argument(rest, arg, sizeof(arg));
		if (arg[0] == 'x' || arg[0] == 'X')
		{
			times = 0;
			str_to_number(times, arg + 1);
			rest = next;
			if (times < 1 || times > BOOK_MAX_TIMES)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, "Od 1 do %d wymian naraz.", BOOK_MAX_TIMES);
				return;
			}
		}
	}

	std::vector<LPITEM> books;
	long long have = 0;
	while (books.size() < BOOK_MAX_CELLS)
	{
		rest = one_argument(rest, arg, sizeof(arg));
		if (!*arg)
			break;
		int cell = -1;
		str_to_number(cell, arg);
		LPITEM book = (cell >= 0 && cell < (int)ch->GetInventoryMaxCount() && cell < INVENTORY_MAX_NUM) ? ch->GetInventoryItem(cell) : NULL;
		if (!book || book->GetType() != ITEM_SKILLBOOK || book->isLocked() || book->IsExchanging() || book->IsEquipped() ||
			book->GetCount() < 1 || std::find(books.begin(), books.end(), book) != books.end())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Ksi" "\xea" "gi si" "\xea" " zmieni" "\xb3" "y - u" "\xb3" "\xf3" "\xbf" " je od nowa.");
			return;
		}
		books.push_back(book);
		have += (long long)book->GetCount();
	}
	const long long need = (long long)times * BOOK_COUNT;
	if (have < need)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Potrzeba %lld ksi" "\xb9" "g (w oknie: %lld).", need, have);
		return;
	}
	const long long cost = (long long)times * BOOK_COST;
	if ((long long)ch->GetGold() < cost)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Za ma" "\xb3" "o Yang (potrzeba %lld).", cost);
		return;
	}

	// The class's and group's "Instr." books that exist here (some are missing in this world:
	// the Warrior has five a group, an Assassin's / Sura's / Shaman's group six).
	static const DWORD c_adwFirstSkill[4][2] = { { 1, 16 }, { 31, 46 }, { 61, 76 }, { 91, 106 } };
	const DWORD dwFirst = c_adwFirstSkill[ch->GetJob()][ch->GetSkillGroup() - 1];
	std::vector<DWORD> rewards;
	for (DWORD i = 0; i < 6; ++i)
		if (ITEM_MANAGER::instance().GetTable(BOOK_INSTR_BASE + dwFirst + i))
			rewards.push_back(BOOK_INSTR_BASE + dwFirst + i);
	if (rewards.empty())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Seon-Hae nie ma ksi" "\xb9" "g dla Twojej drogi.");
		return;
	}

	// MT2009_PLUS_BOOK_EXCHANGE_V2: how many pieces each cell gives (in order,
	// 10 x times in all), and the cells a whole stack taken frees.
	std::map<LPITEM, long long> taken;
	long long left = need;
	int freed = 0;
	for (size_t i = 0; i < books.size() && left > 0; ++i)
	{
		const long long count = (long long)books[i]->GetCount();
		const long long take = std::min(count, left);
		taken[books[i]] = take;
		left -= take;
		if (take == count)
			freed += books[i]->GetSize();
	}

	// The rewards, rolled first, then the room they need: the stacks of the
	// same book (as they will be after the taking) and empty cells.
	std::map<DWORD, int> rolled;
	for (int t = 0; t < times; ++t)
		++rolled[rewards[number(0, (int)rewards.size() - 1)]];
	int cellsNeeded = 0;
	for (std::map<DWORD, int>::const_iterator it = rolled.begin(); it != rolled.end(); ++it)
	{
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(it->first);
		const long long maxStack = std::max<long long>(1, proto ? (long long)proto->dwMaxStack : 1);
		long long room = 0;
		if (proto && (proto->dwFlags & ITEM_FLAG_STACKABLE) && proto->bType != ITEM_BLEND)
		{
			for (int i = 0; i < ch->GetInventoryMaxCount(); ++i)
			{
				LPITEM held = ch->GetInventoryItem(i);
				if (!held || held->GetVnum() != it->first)
					continue;
				bool plain = true;	// AutoStackItemProto joins only a stack with the proto's sockets
				for (int j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
					if (held->GetSocket(j) != proto->alSockets[j])
						plain = false;
				if (!plain)
					continue;
				std::map<LPITEM, long long>::const_iterator tk = taken.find(held);
				const long long after = (long long)held->GetCount() - (tk != taken.end() ? tk->second : 0);
				if (after > 0 && after < maxStack)
					room += maxStack - after;
			}
		}
		const long long over = (long long)it->second - room;
		if (over > 0)
			cellsNeeded += (int)((over + maxStack - 1) / maxStack) * std::max<int>(1, proto ? proto->bSize : 1);
	}
	if (cellsNeeded > ch->CountEmptyInventory() + freed)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Zr" "\xf3" "b miejsce w ekwipunku (potrzeba %d wolnych p" "\xf3" "l).", cellsNeeded);
		return;
	}

	for (size_t i = 0; i < books.size(); ++i)
	{
		std::map<LPITEM, long long>::const_iterator tk = taken.find(books[i]);
		if (tk == taken.end() || tk->second <= 0)
			continue;
		LPITEM book = books[i];
		if ((long long)book->GetCount() > tk->second)
			book->SetCount((ITEM_COUNT)((long long)book->GetCount() - tk->second));
		else
			ITEM_MANAGER::instance().RemoveItem(book, "DIGI_QOL_BOOKS");
	}
	PlayerBotChangeGold(ch, -cost);
	for (std::map<DWORD, int>::const_iterator it = rolled.begin(); it != rolled.end(); ++it)
	{
		// One at a time: each joins a stack or takes an empty cell (CreateItem
		// cuts a count over the stack's limit).
		for (int n = 0; n < it->second; ++n)
			ch->AutoGiveItem(it->first, 1, -1, false);
		char hint[32];
		snprintf(hint, sizeof(hint), "x%d", it->second);
		LogManager::instance().CharLog(ch, it->first, "DIGI_QOL_BOOKS", hint);
		ch->ChatPacket(CHAT_TYPE_COMMAND, "NOWY_KSIEGI done %u %d", it->first, it->second);
	}
}
