// MT2009 PLUS Flower Event ("Dzieci Kwiaty") - MT2009_PLUS_FLOWER_V1
// (the operator, 30 September: the Owsap mini games, full versions, with real
// packets in a new exe; the rewards must stay editable).
//
// Owsap's CFlowerEvent (flower_event.cpp, v6.2.6) with its packets and names,
// on our in-game event manager (playerbot_ingame_events.h):
//
//  - while the event "flower" runs (the panels' scheduler kind "flower", or a
//    GM's e_flower_drop flag), every monster a player kills may give a seed
//    ("envelope", a counter - quest flag flower_event.envelope, no item);
//  - in the window (uiflowerevent.py, HEADER_CG/GC_FLOWER_EVENT 187) seeds
//    become shoots of the five flowers at random (flower_event.chrysanthemum,
//    may_bell, daffodil, lily, sunflower), and shoots of one flower become
//    its reward - by default its gift box 83023-83027, whose contents are
//    special_item_group groups (the Seban panel's "Szkatulki" page);
//  - the boxes give, among others, the flowers 25121-25125: USE_AFFECT items
//    with value0 = 570 (Owsap's AFFECT_FLOWER_EVENT), value1 the point,
//    value2 the level-1 value, value3 the duration, value4 the value per
//    level. One flower buff at a time: the same flower raises its level,
//    another replaces it, each try with its own chance; a failed try uses the
//    flower up too.
//  - After the event, for the reward window the event manager opens
//    (e_flower_reward, 7 days), seeds and shoots can still be exchanged and
//    flowers used; no new seeds drop.
//
// What the operator may change without a build (the classic panel's
// "Dzieci Kwiaty" page writes it; re-read within 5 s, no restart):
// /opt/m2spool/flower_event.tsv, "key value" lines, '#' comments:
//     seed_chance 100        hundredths of a percent per kill at an equal
//                            level (100 = 1 %, Owsap's e_flower_drop 100)
//     min_level 1            the killer's lowest level for a seed
//     seeds_per_shoot 1      seeds one shoot costs
//     shoots_per_reward 10   shoots of one flower one reward costs
//     add_rate 50            % - a flower when no flower buff is on
//     change_rate 30         % - a flower replacing another flower's buff
//     upgrade_rate 15        % - the same flower one level up
//     max_level 5            the highest buff level
//     counter_max 99999      the most of each counter
//     use_after_event 1      flowers usable in the reward window too (0 no)
//     reward <1-5> <vnum> <count>   what shoots_per_reward shoots of that
//                            flower give (1 Chryzantema .. 5 Slonecznik)
// A missing key keeps its default, a wrong line is skipped (one log line).
//
// Fixed from Owsap: the client's shoot type and amount key are checked
// (Owsap indexed its tables with them - out of bounds); seeds are spent only
// for shoots actually given (Owsap took them when the counters were full and
// its "all full" test could never be true); one counter update per exchange;
// the counters change before the reward is given and only after the room is
// checked (no second claim, no lost reward); bots never use the window (but
// see below); the GC packet goes only to a client whose exe sent the CG packet in
// this session (an older exe would stop at the unknown header).
//
// Counters are kept from one event to the next (Owsap did the same): what a
// player collected is never taken away.
//
// MT2009_PLUS_BOT_FLOWER_V1 (the owner, 5 October: "Naucz boty grac w Dzieci
// Kwiaty", every bot): a bot's kill rolls the seed with a player's chance and
// the seed goes to the same counter (PlayerBotFlowerSeed); the bots' pass
// (playerbot_minigames.h) exchanges the seeds and the shoots through
// ExchangeSeeds / ExchangeShoots below - the window's own exchange - and the
// boxes are opened by the bots' chest pass like any gift box.
#include <sys/stat.h>

// MT2009_PLUS_BOT_FLOWER_V1: a bot's seed (playerbot_minigames.h, later in the unit).
namespace
{
	void PlayerBotFlowerSeed(LPCHARACTER ch);
}

namespace mt2009_flower
{
	const DWORD AFFECT_FLOWER = 570;	// Owsap's AFFECT_FLOWER_EVENT (ours end at 544)
	const char* const SETTINGS_PATH = "/opt/m2spool/flower_event.tsv";
	const DWORD SETTINGS_RELOAD_MS = 5000;
	const DWORD EXCHANGE_COOLDOWN_MS = 1000;	// Owsap's FLOWER_EVENT_EXCHANGE_COOLTIME_SEC
	const char* const EFFECT_FILE = "d:/ymir work/effect/etc/buff/buff_item15_flower.mse";
	const char* const EVENT_KEY = "flower";

	// The window's amount list (uiflowerevent.py EXCHANGE_COUNT_TYPE_TEXT).
	const int EXCHANGE_KEYS[] = { 1, 10, 50, 100 };
	const int EXCHANGE_KEY_COUNT = sizeof(EXCHANGE_KEYS) / sizeof(EXCHANGE_KEYS[0]);

	const char* const COUNTER_FLAG[SHOOT_TYPE_MAX] =
	{
		"flower_event.envelope",
		"flower_event.chrysanthemum",
		"flower_event.may_bell",
		"flower_event.daffodil",
		"flower_event.lily",
		"flower_event.sunflower",
	};

	// Player-visible, so Polish and ASCII-only.
	const char* const SHOOT_NAME[SHOOT_TYPE_MAX] =
	{
		"Nasiona Kwiatow", "Chryzantema", "Konwalia", "Narcyz", "Lilia", "Slonecznik",
	};

	struct TSettings
	{
		int		seedChance;
		int		minLevel;
		int		seedsPerShoot;
		int		shootsPerReward;
		int		addRate;
		int		changeRate;
		int		upgradeRate;
		int		maxLevel;
		int		counterMax;
		bool	useAfterEvent;
		DWORD	rewardVnum[SHOOT_TYPE_MAX];
		int		rewardCount[SHOOT_TYPE_MAX];

		TSettings()
			: seedChance(100), minLevel(1), seedsPerShoot(1), shootsPerReward(10),
			addRate(50), changeRate(30), upgradeRate(15), maxLevel(5), counterMax(99999),
			useAfterEvent(true)
		{
			for (int i = 0; i < SHOOT_TYPE_MAX; ++i)
			{
				rewardVnum[i] = i == SHOOT_ENVELOPE ? 0 : 83022 + i;	// 83023 .. 83027
				rewardCount[i] = 1;
			}
		}
	};

	TSettings s_settings;
	DWORD	s_dwNextReload = 0;
	time_t	s_tLoadedMtime = (time_t)-1;
	off_t	s_lLoadedSize = -1;

	// pid -> the desc handle whose exe sent HEADER_CG_FLOWER_EVENT: only that
	// session gets HEADER_GC_FLOWER_EVENT.
	std::map<DWORD, DWORD> s_capable;
	// pid -> when the last exchange may be followed by the next (ms).
	std::map<DWORD, DWORD> s_nextExchange;

	int Clamp(int v, int lo, int hi)
	{
		return v < lo ? lo : (v > hi ? hi : v);
	}

	void ParseSettings(FILE* fp)
	{
		TSettings s;
		char line[512];
		int lineNo = 0;
		while (fgets(line, sizeof(line), fp))
		{
			++lineNo;
			char* hash = strchr(line, '#');
			if (hash)
				*hash = '\0';
			char key[64] = "";
			long a = 0, b = 0, c = 0;
			const int n = sscanf(line, "%63s %ld %ld %ld", key, &a, &b, &c);
			if (n <= 0)
				continue;
			if (n < 2)
			{
				sys_err("FLOWER: %s:%d: a key without a value", SETTINGS_PATH, lineNo);
				continue;
			}
			if (!strcmp(key, "seed_chance"))
				s.seedChance = Clamp((int)a, 0, 10000);
			else if (!strcmp(key, "min_level"))
				s.minLevel = Clamp((int)a, 1, 250);
			else if (!strcmp(key, "seeds_per_shoot"))
				s.seedsPerShoot = Clamp((int)a, 1, 1000);
			else if (!strcmp(key, "shoots_per_reward"))
				s.shootsPerReward = Clamp((int)a, 1, 1000);
			else if (!strcmp(key, "add_rate"))
				s.addRate = Clamp((int)a, 0, 100);
			else if (!strcmp(key, "change_rate"))
				s.changeRate = Clamp((int)a, 0, 100);
			else if (!strcmp(key, "upgrade_rate"))
				s.upgradeRate = Clamp((int)a, 0, 100);
			else if (!strcmp(key, "max_level"))
				s.maxLevel = Clamp((int)a, 1, 20);
			else if (!strcmp(key, "counter_max"))
				s.counterMax = Clamp((int)a, 1, 999999);
			else if (!strcmp(key, "use_after_event"))
				s.useAfterEvent = a != 0;
			else if (!strcmp(key, "reward"))
			{
				if (n < 4 || a < SHOOT_CHRYSANTHEMUM || a > SHOOT_SUNFLOWER || b <= 0 || c <= 0 || c > 1000)
				{
					sys_err("FLOWER: %s:%d: reward <1-5> <vnum> <count 1-1000>", SETTINGS_PATH, lineNo);
					continue;
				}
				if (!ITEM_MANAGER::instance().GetTable((DWORD)b))
				{
					sys_err("FLOWER: %s:%d: no item %ld - the reward of flower %ld stays %u", SETTINGS_PATH, lineNo, b, a,
							s.rewardVnum[a]);
					continue;
				}
				s.rewardVnum[a] = (DWORD)b;
				s.rewardCount[a] = (int)c;
			}
			else
				sys_err("FLOWER: %s:%d: unknown key %s", SETTINGS_PATH, lineNo, key);
		}
		s_settings = s;
	}

	// The file again when it changed; at most every SETTINGS_RELOAD_MS.
	const TSettings& Settings()
	{
		const DWORD now = get_dword_time();
		if (s_dwNextReload && (int)(now - s_dwNextReload) < 0)
			return s_settings;
		s_dwNextReload = now + SETTINGS_RELOAD_MS;
		struct stat st;
		if (stat(SETTINGS_PATH, &st) != 0)
		{
			if (s_lLoadedSize != -1)
			{
				s_settings = TSettings();
				s_tLoadedMtime = (time_t)-1;
				s_lLoadedSize = -1;
				sys_log(0, "FLOWER: %s gone - the defaults", SETTINGS_PATH);
			}
			return s_settings;
		}
		if (st.st_mtime == s_tLoadedMtime && st.st_size == s_lLoadedSize)
			return s_settings;
		FILE* fp = fopen(SETTINGS_PATH, "r");
		if (!fp)
			return s_settings;
		ParseSettings(fp);
		fclose(fp);
		s_tLoadedMtime = st.st_mtime;
		s_lLoadedSize = st.st_size;
		sys_log(0, "FLOWER: settings read - seed %d/10000, %d seed(s)/shoot, %d shoots/reward, rates %d/%d/%d, level %d, cap %d, rewards %u x%d %u x%d %u x%d %u x%d %u x%d",
				s_settings.seedChance, s_settings.seedsPerShoot, s_settings.shootsPerReward,
				s_settings.addRate, s_settings.changeRate, s_settings.upgradeRate, s_settings.maxLevel, s_settings.counterMax,
				s_settings.rewardVnum[1], s_settings.rewardCount[1], s_settings.rewardVnum[2], s_settings.rewardCount[2],
				s_settings.rewardVnum[3], s_settings.rewardCount[3], s_settings.rewardVnum[4], s_settings.rewardCount[4],
				s_settings.rewardVnum[5], s_settings.rewardCount[5]);
		return s_settings;
	}

	bool Eligible(LPCHARACTER ch)
	{
		return ch && ch->IsPC() && ch->GetDesc() && !ch->GetDesc()->IsBot();
	}

	bool Running()
	{
		return InGameEventIsActive(EVENT_KEY);
	}

	bool RewardWindow()
	{
		return InGameEventRewardEndTime(EVENT_KEY) > (DWORD)time(NULL);
	}

	// The window works while the event runs and through its reward window.
	bool WindowOpen()
	{
		return Running() || RewardWindow();
	}

	bool Capable(LPCHARACTER ch)
	{
		std::map<DWORD, DWORD>::const_iterator it = s_capable.find(ch->GetPlayerID());
		return it != s_capable.end() && it->second == ch->GetDesc()->GetHandle();
	}

	int Counter(LPCHARACTER ch, int type)
	{
		const int v = ch->GetQuestFlag(COUNTER_FLAG[type]);
		return v > 0 ? v : 0;
	}

	void SetCounter(LPCHARACTER ch, int type, int value)
	{
		ch->SetQuestFlag(COUNTER_FLAG[type], value > 0 ? value : 0);
	}

	// Owsap's CFlowerEvent::Process.
	void Send(LPCHARACTER ch, BYTE sub, BYTE chatType = FLOWER_EVENT_CHAT_TYPE_MAX, BYTE shootType = SHOOT_TYPE_MAX, int count = 0)
	{
		if (!Eligible(ch) || !Capable(ch))
			return;
		TPacketGCFlowerEvent p;
		memset(&p, 0, sizeof(p));
		p.bHeader = HEADER_GC_FLOWER_EVENT;
		p.bSubHeader = sub;
		p.bChatType = chatType;
		if (sub == FLOWER_EVENT_SUBHEADER_GC_INFO_ALL)
		{
			p.bShootType = SHOOT_TYPE_MAX;
			for (int i = 0; i < SHOOT_TYPE_MAX; ++i)
				p.aiShootCount[i] = Counter(ch, i);
		}
		else
		{
			p.bShootType = shootType <= SHOOT_TYPE_MAX ? shootType : (BYTE)SHOOT_TYPE_MAX;
			p.aiShootCount[p.bShootType] = count;
		}
		ch->GetDesc()->Packet(&p, sizeof(p));
	}

	void Update(LPCHARACTER ch, int type)
	{
		Send(ch, FLOWER_EVENT_SUBHEADER_GC_UPDATE_INFO, FLOWER_EVENT_CHAT_TYPE_MAX, (BYTE)type, Counter(ch, type));
	}

	void Message(LPCHARACTER ch, BYTE chatType, BYTE shootType = SHOOT_TYPE_MAX, int count = 0)
	{
		Send(ch, FLOWER_EVENT_SUBHEADER_GC_GET_INFO, chatType, shootType, count);
	}

	// Owsap's CFlowerEvent::Reward: one seed.
	void GiveSeed(LPCHARACTER ch, const TSettings& s)
	{
		const int have = Counter(ch, SHOOT_ENVELOPE);
		if (have >= s.counterMax)
		{
			if (Capable(ch))
				Message(ch, FLOWER_EVENT_CHAT_TYPE_ENVELOPE_MAX);
			return;
		}
		SetCounter(ch, SHOOT_ENVELOPE, have + 1);
		if (Capable(ch))
		{
			Update(ch, SHOOT_ENVELOPE);
			Message(ch, FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_ENVELOPE, SHOOT_ENVELOPE);
		}
		else
			ch->ChatPacket(CHAT_TYPE_INFO, "Dzieci Kwiaty: zdobywasz Nasiona Kwiatow (masz %d). Okno eventu otworzysz po aktualizacji klienta.", have + 1);
	}

	const BYTE SHOOT_GET_ONE[SHOOT_TYPE_MAX] =
	{
		FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_ENVELOPE,
		FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_CHRYSANTHEMUM,
		FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_MAY_BELL,
		FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_DAFFODIL,
		FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_LILY,
		FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_SUNFLOWER,
	};
	const BYTE SHOOT_GET_MANY[SHOOT_TYPE_MAX] =
	{
		FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_ENVELOPE,
		FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_CHRYSANTHEMUM_COUNT,
		FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_MAY_BELL_COUNT,
		FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_DAFFODIL_COUNT,
		FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_LILY_COUNT,
		FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_SUNFLOWER_COUNT,
	};

	// Seeds -> shoots: up to `wanted' shoots, each of a random flower whose
	// counter has room; seeds are spent only for the shoots given.
	void ExchangeSeeds(LPCHARACTER ch, int wanted, const TSettings& s)
	{
		const int seeds = Counter(ch, SHOOT_ENVELOPE);
		if (seeds < s.seedsPerShoot * wanted)
		{
			Message(ch, FLOWER_EVENT_CHAT_TYPE_NOT_ENOUGH_SHOOT_ENVELOPE);
			return;
		}
		int room[SHOOT_TYPE_MAX] = { 0 };
		int totalRoom = 0;
		for (int t = SHOOT_CHRYSANTHEMUM; t <= SHOOT_SUNFLOWER; ++t)
		{
			room[t] = std::max(0, s.counterMax - Counter(ch, t));
			totalRoom += room[t];
		}
		if (totalRoom <= 0)
		{
			Message(ch, FLOWER_EVENT_CHAT_TYPE_ITEM_FULL_AND_NOT_USE);
			return;
		}
		int got[SHOOT_TYPE_MAX] = { 0 };
		int given = 0;
		for (int i = 0; i < wanted; ++i)
		{
			int open[SHOOT_TYPE_MAX];
			int opens = 0;
			for (int t = SHOOT_CHRYSANTHEMUM; t <= SHOOT_SUNFLOWER; ++t)
				if (got[t] < room[t])
					open[opens++] = t;
			if (!opens)
				break;
			++got[open[number(0, opens - 1)]];
			++given;
		}
		SetCounter(ch, SHOOT_ENVELOPE, seeds - given * s.seedsPerShoot);
		for (int t = SHOOT_CHRYSANTHEMUM; t <= SHOOT_SUNFLOWER; ++t)
		{
			if (!got[t])
				continue;
			SetCounter(ch, t, Counter(ch, t) + got[t]);
			Update(ch, t);
			if (got[t] == 1)
				Message(ch, SHOOT_GET_ONE[t], (BYTE)t, 1);
			else
				Message(ch, SHOOT_GET_MANY[t], (BYTE)t, got[t]);
		}
		Update(ch, SHOOT_ENVELOPE);
		sys_log(0, "FLOWER: %s %d seed(s) -> %d shoot(s) (%d %d %d %d %d)", ch->GetName(), given * s.seedsPerShoot, given,
				got[1], got[2], got[3], got[4], got[5]);
	}

	// Shoots of one flower -> its reward.
	void ExchangeShoots(LPCHARACTER ch, int type, int multiplier, const TSettings& s)
	{
		const int have = Counter(ch, type);
		const int need = s.shootsPerReward * multiplier;
		if (have < need)
		{
			Message(ch, FLOWER_EVENT_CHAT_TYPE_NOT_ENOUGH_SHOOT_COUNT);
			return;
		}
		const DWORD vnum = s.rewardVnum[type];
		const TItemTable* table = ITEM_MANAGER::instance().GetTable(vnum);
		if (!table)
		{
			sys_err("FLOWER: the reward of flower %d is item %u, which does not exist", type, vnum);
			ch->ChatPacket(CHAT_TYPE_INFO, "Dzieci Kwiaty: nagroda jest chwilowo niedostepna - zglos to administracji.");
			return;
		}
		const long long total = (long long)s.rewardCount[type] * multiplier;
		const bool stackable = (table->dwFlags & ITEM_FLAG_STACKABLE) && !(table->dwAntiFlags & ITEM_ANTIFLAG_STACK);
		const long long stacks = stackable ? (total + ITEM_MAX_COUNT - 1) / ITEM_MAX_COUNT : total;
		const int size = std::max(1, (int)table->bSize);
		if (ch->GetEmptyInventory((BYTE)size) == -1 || stacks * size > ch->CountEmptyInventory())
		{
			Message(ch, FLOWER_EVENT_CHAT_TYPE_NOT_ENOUGH_EVENTORY_SPACE);
			return;
		}
		// The counter first: whatever happens below, the shoots pay once.
		SetCounter(ch, type, have - need);
		Update(ch, type);
		long long left = total;
		while (left > 0)
		{
			const ITEM_COUNT count = (ITEM_COUNT)(stackable ? std::min<long long>(left, ITEM_MAX_COUNT) : 1);
			if (!ch->AutoGiveItem(vnum, count))
			{
				sys_err("FLOWER: %s - item %u x%u not given (%lld left)", ch->GetName(), vnum, (unsigned int)count, left);
				break;
			}
			left -= count;
		}
		sys_log(0, "FLOWER: %s %d shoot(s) of %s -> item %u x%lld", ch->GetName(), need, SHOOT_NAME[type], vnum, total - left);
	}

	void Exchange(LPCHARACTER ch, BYTE type, BYTE key)
	{
		// Owsap indexed its tables with both, unchecked.
		if (type >= SHOOT_TYPE_MAX || key >= EXCHANGE_KEY_COUNT)
		{
			sys_err("FLOWER: %s sent shoot type %u, amount key %u", ch->GetName(), type, key);
			return;
		}
		if (!WindowOpen())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Dzieci Kwiaty: event i czas na wymiane juz sie skonczyly.");
			return;
		}
		if (ch->GetExchange() || ch->GetMyShop() || ch->GetShop() || ch->IsOpenSafebox() || ch->IsCubeOpen() || ch->IsWarping() || ch->IsDead())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Dzieci Kwiaty: zamknij inne okna (handel, sklep, magazyn), zanim wymienisz.");
			return;
		}
		const DWORD now = get_dword_time();
		std::map<DWORD, DWORD>::iterator cd = s_nextExchange.find(ch->GetPlayerID());
		if (cd != s_nextExchange.end() && (int)(now - cd->second) < 0)
			return;
		s_nextExchange[ch->GetPlayerID()] = now + EXCHANGE_COOLDOWN_MS;

		const TSettings& s = Settings();
		if (type == SHOOT_ENVELOPE)
			ExchangeSeeds(ch, EXCHANGE_KEYS[key], s);
		else
			ExchangeShoots(ch, type, EXCHANGE_KEYS[key], s);
	}

	// ------------------------------------------------------------ the flowers

	void Effect(LPCHARACTER ch)
	{
		char file[MAX_EFFECT_FILE_NAME];
		memset(file, 0, sizeof(file));
		strlcpy(file, EFFECT_FILE, sizeof(file));
		ch->SpecificEffectPacket(file);
	}

	// Owsap's CFlowerEvent::UseFlower. true: the item was used up.
	bool UseFlower(LPCHARACTER ch, LPITEM item)
	{
		const TSettings& s = Settings();
		if (!Running() && !(s.useAfterEvent && RewardWindow()))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "Event Dzieci Kwiaty sie skonczyl - tego kwiatu nie mozna juz uzyc.");
			return false;
		}
		const BYTE point = (BYTE)item->GetValue(1);
		const long base = item->GetValue(2);
		const long duration = item->GetValue(3) > 0 ? item->GetValue(3) : 43200;
		const long perLevel = item->GetValue(4) > 0 ? item->GetValue(4) : base;
		if (point == POINT_NONE || base <= 0 || perLevel <= 0)
		{
			sys_err("FLOWER: item %u has no usable values (point %u, %ld, %ld)", item->GetVnum(), point, base, perLevel);
			return false;
		}
		const char* name = item->GetName() ? item->GetName() : "kwiat";
		bool success = false;
		CAffect* same = ch->FindAffect(AFFECT_FLOWER, point);
		if (same)
		{
			// The level from the value (Owsap's GetFlowerAffectLevel, exact).
			int level = std::max(1, (int)(same->lApplyValue / perLevel));
			if (level >= s.maxLevel)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, "%s: efekt ma juz najwyzszy poziom (%d).", name, s.maxLevel);
				return false;
			}
			++level;
			if (number(1, 100) <= s.upgradeRate)
			{
				ch->AddAffect(AFFECT_FLOWER, point, perLevel * level, 0, duration, 0, true, false, item->GetVnum());
				ch->ChatPacket(CHAT_TYPE_INFO, "Sukces! Efekt %s wzrosl do poziomu %d.", name, level);
				success = true;
			}
			else
				ch->ChatPacket(CHAT_TYPE_INFO, "Porazka! Nie udalo sie podniesc efektu %s do poziomu %d - kwiat zwiedl.", name, level);
		}
		else
		{
			const bool change = ch->FindAffect(AFFECT_FLOWER) != NULL;
			if (number(1, 100) <= (change ? s.changeRate : s.addRate))
			{
				// bOverride: the other flower's buff (one affect 570 at a time) gives way.
				ch->AddAffect(AFFECT_FLOWER, point, base, 0, duration, 0, true, false, item->GetVnum());
				ch->ChatPacket(CHAT_TYPE_INFO, "Sukces! Efekt %s jest aktywny (poziom 1).", name);
				success = true;
			}
			else if (change)
				ch->ChatPacket(CHAT_TYPE_INFO, "Porazka! Nie udalo sie zamienic efektu na %s - kwiat zwiedl, poprzedni efekt zostaje.", name);
			else
				ch->ChatPacket(CHAT_TYPE_INFO, "Porazka! Efekt %s sie nie wlaczyl - kwiat zwiedl.", name);
		}
		if (success)
			Effect(ch);
		sys_log(0, "FLOWER: %s used %u (point %u): %s", ch->GetName(), item->GetVnum(), point, success ? "success" : "failure");
		item->SetCount(item->GetCount() - 1);
		return true;
	}
}

// ITEM_MANAGER::CreateQuestDropItem, every kill (server-patches/flower,
// MT2009_PLUS_FLOWER_V1 (kill)).
void FlowerEventOnKill(LPCHARACTER victim, LPCHARACTER killer, int iDeltaPercent, int iRandRange)
{
	using namespace mt2009_flower;
	// MT2009_PLUS_BOT_FLOWER_V1: a bot's kill rolls the same seed.
	const bool bot = killer && killer->IsPC() && killer->GetDesc() && killer->GetDesc()->IsBot();
	if (!victim || victim->IsPC() || (!bot && !Eligible(killer)) || iRandRange <= 0 || !Running())
		return;
	const TSettings& s = Settings();
	if (s.seedChance <= 0 || killer->GetLevel() < s.minLevel)
		return;
	// Owsap: GetDropPerKillPct(50, 100, iDeltaPercent, "e_flower_drop") -
	// 40000 * delta / 100 of 4 000 000, 1 % at an equal level; here the same
	// with seed_chance hundredths of a percent.
	const long long pct = (long long)iDeltaPercent * s.seedChance * iRandRange / 1000000LL;
	if (pct >= number(1, iRandRange))
	{
		if (bot)
			PlayerBotFlowerSeed(killer);
		else
			GiveSeed(killer, s);
	}
}

// CInputMain::Analyze, HEADER_CG_FLOWER_EVENT (MT2009_PLUS_FLOWER_V1 (input)).
void FlowerEventPacket(LPCHARACTER ch, const char* data)
{
	using namespace mt2009_flower;
	if (!Eligible(ch) || !data)
		return;
	// This session's exe knows the packets.
	s_capable[ch->GetPlayerID()] = ch->GetDesc()->GetHandle();
	const TPacketCGFlowerEvent* p = reinterpret_cast<const TPacketCGFlowerEvent*>(data);
	switch (p->bSubHeader)
	{
		case FLOWER_EVENT_SUBHEADER_CG_INFO_ALL:
			Send(ch, FLOWER_EVENT_SUBHEADER_GC_INFO_ALL);
			break;
		case FLOWER_EVENT_SUBHEADER_CG_EXCHANGE:
			Exchange(ch, p->bShootType, p->bExchangeKey);
			break;
		default:
			sys_err("FLOWER: %s sent subheader %u", ch->GetName(), p->bSubHeader);
			break;
	}
}

// CHARACTER::UseItemEx, USE_AFFECT (MT2009_PLUS_FLOWER_V1 (use)): false - not
// a flower, the engine goes on; true - a flower, `result' is UseItemEx's.
bool FlowerEventUseItem(LPCHARACTER ch, LPITEM item, bool& result)
{
	using namespace mt2009_flower;
	if (!ch || !item || item->GetValue(0) != (long)AFFECT_FLOWER)
		return false;
	result = Eligible(ch) ? UseFlower(ch, item) : false;
	return true;
}
