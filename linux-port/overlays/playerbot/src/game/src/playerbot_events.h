// The world's timed events, run by the core: a window in which the Moonlight
// chests drop (and outside which they do not drop at all), and windows of
// more experience, drop or yang over the world's own rates - on a weekly
// clock, or switched on from the panel for a number of minutes ("Aktywuj
// teraz"). The panel writes /opt/m2spool/playerbot_events.tsv; this re-reads
// it the way the weights are read, once every five seconds, judges every kind
// once a second with playerbot_events::Evaluate, and:
//
//  - gates the chest odds: g_iMoonlightChestPermille and the stone figure are
//    what playerbot_config.h read from the weights file (the sliders), kept
//    here as the wanted values and put to zero while no chest window is open.
//    Until 2.0.74 the gate shut only once a chest window was written, so a
//    world with no schedule dropped chests all the time, and the operator's
//    word was "usune domyslny drop, a wprowadze tylko jako event"
//    (18 September): the event is the only way a chest drops now. Every core
//    gates its own, because CreateDropItem rolls locally - including a core
//    that hosts no bot, which the world clock (CPlayerBotManager::
//    StartWorldClock) runs this for; before it, such a core never gated and
//    dropped at the sliders' rate whatever the schedule said ("mimo
//    harmonogramu blaskow dropia one takze poza nim", NerrVoVy).
//  - moves the rate flags (mob_exp, mob_item, mob_gold and their _buyer
//    twins) through the DB core on ONE core only - the one hosting Joan, map
//    21, which is game1 under both layouts. Three cores each adding fifty
//    percent to a flag they all read would compound it. The base is kept in
//    a flag of its own (m2_event_exp_base ...) so a core restarted inside an
//    event starts from the base and not from the boosted number, and a rate
//    the operator moved during the event is left where they put it.
//  - speaks: BroadcastNotice at the start, every fifteen minutes and at the
//    end, from the leader only, because the notice goes to every core by P2P.
//  - writes playerbot_events_status.tsv beside playerbot_status.tsv for the
//    panel's "active until / next at".
//
// Nothing here changes what the bots do; a chest event is about drops. The
// two events that put something into the world - Pirate Tanaka and Zuo's
// Metin rain - are judged here like the rest and run by
// playerbot_world_events.h, which speaks for them itself, because only it
// knows the map and what stands on it.
// MT2009_PLUS_GOBLIN_V1: the Treasure Hunt's clock (playerbot_goblin.h, which
// the manager includes after this file), asked once for every pass below.
void GoblinEventTick(DWORD dwNow);
// MT2009_PLUS_EVENT_MANAGER_V1: the in-game event manager
// (playerbot_ingame_events.h, included after this file): the mini games'
// event flags, their table NPCs and the players' event list, every pass.
void InGameEventTick(DWORD dwNow);
// MT2009_PLUS_RUMI_V1: Rumi's seasons and its games' upkeep (playerbot_rumi.h,
// included after this file), every pass after the event manager.
void RumiTick(DWORD dwNow);
// MT2009_PLUS_CATCH_KING_V1: Catch the King's season (playerbot_catchking.h,
// included after this file), every pass.
void CatchKingTick(DWORD dwNow);

namespace {
	const char* const PLAYERBOT_EVENTS_DEFAULT_PATH = "/opt/m2spool/playerbot_events.tsv";
	const char* const PLAYERBOT_EVENTS_STATUS_PATH = "playerbot_events_status.tsv";
	const DWORD PLAYERBOT_EVENTS_RELOAD_INTERVAL = 5000;
	const DWORD PLAYERBOT_EVENTS_CHECK_INTERVAL = 1000;
	const DWORD PLAYERBOT_EVENTS_REMINDER_INTERVAL = 15 * 60 * 1000;
	const DWORD PLAYERBOT_EVENTS_STATUS_INTERVAL = 60000;
	// Joan. game1 hosts it under split and under unified, so exactly one core
	// drives the flags and speaks.
	const long PLAYERBOT_EVENTS_LEADER_MAP = PLAYERBOT_MAP_CHUNJO_M1;

	std::vector<playerbot_events::Window> s_vecPlayerBotEvents;
	// The file's settings lines ("bots 50"), back to their defaults at every
	// read so a line taken out is a setting taken out.
	playerbot_events::Settings s_PlayerBotEventSettings;
	time_t s_tPlayerBotEventsMtime = 0;
	long s_lPlayerBotEventsSize = -1;
	DWORD s_dwPlayerBotEventsNextReload = 0;
	DWORD s_dwPlayerBotEventsNextCheck = 0;
	DWORD s_dwPlayerBotEventsNextStatus = 0;
	bool s_bPlayerBotEventsStatusDirty = true;
	bool s_bPlayerBotEventsFileSeen = false;

	struct TPlayerBotEventState {
		bool active = false;
		int value = 0;
		long until = 0;
		DWORD nextReminder = 0;
	};
	TPlayerBotEventState s_aPlayerBotEventState[playerbot_events::KIND_MAX];
	playerbot_events::Status s_aPlayerBotEventStatus[playerbot_events::KIND_MAX];
	// Tanaka and Zuo run once for every map the file names, several at a
	// time (EvaluateWorldByMap): the active ones a map, which
	// playerbot_world_events.h begins, runs and ends one by one.
	std::vector<playerbot_events::Status> s_aPlayerBotWorldActive[playerbot_events::KIND_MAX];

	// The sliders' figures, captured whenever playerbot_config.h has just
	// written them (the weights generation moved), so the gate can put zero
	// into the engine's variables and still give them back.
	int s_iPlayerBotChestWantedPermille = -1;
	int s_iPlayerBotChestStoneWantedPermille = -1;
	DWORD s_dwPlayerBotEventsWeightsGeneration = (DWORD)-1;
	bool s_bPlayerBotChestGateClosed = false;

	int GetPlayerBotChestWantedPermille(bool stone)
	{
		return stone ? s_iPlayerBotChestStoneWantedPermille : s_iPlayerBotChestWantedPermille;
	}

	bool IsPlayerBotChestGateClosed()
	{
		return s_bPlayerBotChestGateClosed;
	}

	const char* GetPlayerBotEventsPath()
	{
		const char* override_path = getenv("PLAYERBOT_EVENTS_FILE");
		if (override_path && *override_path)
			return override_path;
		return PLAYERBOT_EVENTS_DEFAULT_PATH;
	}

	void ReadPlayerBotEventsFile(const char* szPath)
	{
		s_vecPlayerBotEvents.clear();
		s_PlayerBotEventSettings = playerbot_events::Settings();
		FILE* fp = fopen(szPath, "rb");
		if (!fp)
			return;
		char line[512];
		int ignored = 0;
		while (fgets(line, sizeof(line), fp))
		{
			playerbot_events::Window w;
			if (playerbot_events::ParseLine(line, w))
				s_vecPlayerBotEvents.push_back(w);
			else if (playerbot_events::ParseSettingLine(line, s_PlayerBotEventSettings))
				continue;
			else if (line[0] != '#' && line[0] != '\r' && line[0] != '\n')
				++ignored;
		}
		fclose(fp);
		sys_log(0, "PLAYERBOT_EVENT: %u event lines read from %s (%d ignored, bots %d%%)",
				(unsigned int)s_vecPlayerBotEvents.size(), szPath, ignored,
				s_PlayerBotEventSettings.botsPercent);
	}

	void RefreshPlayerBotEvents(DWORD dwNow)
	{
		if (dwNow < s_dwPlayerBotEventsNextReload)
			return;
		s_dwPlayerBotEventsNextReload = dwNow + PLAYERBOT_EVENTS_RELOAD_INTERVAL;
		const char* szPath = GetPlayerBotEventsPath();
		struct stat st;
		if (stat(szPath, &st) != 0)
		{
			if (s_lPlayerBotEventsSize >= 0)
			{
				sys_log(0, "PLAYERBOT_EVENT: %s is gone, no events", szPath);
				s_vecPlayerBotEvents.clear();
				s_PlayerBotEventSettings = playerbot_events::Settings();
				s_tPlayerBotEventsMtime = 0;
				s_lPlayerBotEventsSize = -1;
			}
			return;
		}
		if (st.st_mtime == s_tPlayerBotEventsMtime && (long)st.st_size == s_lPlayerBotEventsSize)
			return;
		s_tPlayerBotEventsMtime = st.st_mtime;
		s_lPlayerBotEventsSize = (long)st.st_size;
		s_bPlayerBotEventsFileSeen = true;
		ReadPlayerBotEventsFile(szPath);
	}

	bool IsPlayerBotEventLeader()
	{
		// Joan's core on the first channel: with a second channel on, its
		// game1 hosts map 21 too, and two leaders would say every notice twice.
		return g_bChannel == 1 &&
				SECTREE_MANAGER::instance().GetMap(PLAYERBOT_EVENTS_LEADER_MAP) != NULL;
	}

	const char* PlayerBotEventRateFlag(int kind, bool premium)
	{
		switch (kind)
		{
			case playerbot_events::KIND_EXP: return premium ? "mob_exp_buyer" : "mob_exp";
			case playerbot_events::KIND_DROP: return premium ? "mob_item_buyer" : "mob_item";
			case playerbot_events::KIND_YANG: return premium ? "mob_gold_buyer" : "mob_gold";
		}
		return "";
	}

	const char* PlayerBotEventBaseFlag(int kind, bool premium)
	{
		switch (kind)
		{
			case playerbot_events::KIND_EXP: return premium ? "m2_event_exp_base_buyer" : "m2_event_exp_base";
			case playerbot_events::KIND_DROP: return premium ? "m2_event_drop_base_buyer" : "m2_event_drop_base";
			case playerbot_events::KIND_YANG: return premium ? "m2_event_yang_base_buyer" : "m2_event_yang_base";
		}
		return "";
	}

	// The yang rate the bots' prices are set by: the world's own, never an
	// event's boost of it. While a yang event runs the live mob_gold is the
	// base times (100 + value) percent and the base waits in its own flag, so
	// pricing by the live flag made every counter stocked during the event ask
	// double ("Raty eventowe maja wplyw na ceny na rynku", Iwakura, 23
	// September) and made the market forget what it had learned twice - at the
	// event's start and at its end (ForgetPlayerBotPricesOnRateChange). Asked
	// for every price, so it is read once a second.
	int GetPlayerBotPriceYangRate()
	{
		static DWORD s_dwReadAt = 0;
		static int s_iRate = 0;
		const DWORD now = get_dword_time();
		if (s_iRate == 0 || now - s_dwReadAt >= 1000)
		{
			const int base = quest::CQuestManager::instance().GetEventFlag(
					PlayerBotEventBaseFlag(playerbot_events::KIND_YANG, false));
			s_iRate = base > 0 ? base : CHARACTER_MANAGER::instance().GetMobGoldAmountRate(NULL);
			s_dwReadAt = now;
		}
		return s_iRate;
	}

	// Player-visible, so Polish and ASCII-only like every bot string.
	const char* PlayerBotEventRateWord(int kind)
	{
		switch (kind)
		{
			case playerbot_events::KIND_EXP: return "doswiadczenia";
			case playerbot_events::KIND_DROP: return "szansy na drop";
			case playerbot_events::KIND_YANG: return "yang z potworow";
		}
		return "";
	}

	void FormatPlayerBotEventClock(long until, char* out, size_t len)
	{
		time_t t = (time_t)until;
		struct tm local;
		localtime_r(&t, &local);
		snprintf(out, len, "%02d:%02d", local.tm_hour, local.tm_min);
	}

	enum EPlayerBotEventPhase { EVENT_PHASE_START, EVENT_PHASE_REMINDER, EVENT_PHASE_END };

	// MT2009_PLUS_EVENT_MANAGER_V1: the mini games and Easter run for days, and
	// a notice every fifteen minutes of a week would be the chat's whole text;
	// they are reminded every two hours (their list is by the minimap anyway).
	DWORD PlayerBotEventReminderInterval(int kind)
	{
		switch (kind)
		{
			case playerbot_events::KIND_CATCHKING:
			case playerbot_events::KIND_RUMI:
			case playerbot_events::KIND_YUTNORI:
			case playerbot_events::KIND_FLOWER:
			case playerbot_events::KIND_EASTER:
				return 2 * 60 * 60 * 1000;
		}
		return PLAYERBOT_EVENTS_REMINDER_INTERVAL;
	}

	// MT2009_PLUS_EVENT_MANAGER_V1: the mini games' names in the notices, NULL
	// for every other kind. Player-visible, so Polish and ASCII-only.
	const char* PlayerBotMiniGameEventName(int kind)
	{
		switch (kind)
		{
			case playerbot_events::KIND_CATCHKING: return "Zlap Krola";
			case playerbot_events::KIND_RUMI: return "Rumi (Okey)";
			case playerbot_events::KIND_YUTNORI: return "Yut Nori";
			case playerbot_events::KIND_FLOWER: return "Dzieci Kwiaty";
			case playerbot_events::KIND_EASTER: return "Event wielkanocny (metiny wielkanocne i Wielkanocny Zajac)";
		}
		return NULL;
	}

	void AnnouncePlayerBotEvent(int kind, int value, long until, EPlayerBotEventPhase phase)
	{
		char body[128];
		if (kind == playerbot_events::KIND_CHEST)
			snprintf(body, sizeof(body), "Szkatulki Blasku Ksiezyca dropia z potworow i metinow");
		else if (kind == playerbot_events::KIND_BOSS_LOOT)
			snprintf(body, sizeof(body), "podwojny loot z bossow");
		else if (kind == playerbot_events::KIND_METIN_LOOT)
			snprintf(body, sizeof(body), "podwojny loot z Metinow");
		else if (kind == playerbot_events::KIND_GOBLIN)
			snprintf(body, sizeof(body), "Poszukiwanie skarbow z Goblinem Skarbow - Bilety Skarbow w skrzyniach");
		else if (PlayerBotMiniGameEventName(kind))
			snprintf(body, sizeof(body), "%s (lista eventow: przycisk przy minimapie)", PlayerBotMiniGameEventName(kind));
		else
			snprintf(body, sizeof(body), "+%d%% %s", value, PlayerBotEventRateWord(kind));
		char text[256];
		if (phase == EVENT_PHASE_END)
		{
			if (kind == playerbot_events::KIND_CHEST)
				snprintf(text, sizeof(text), "Event zakonczony: Szkatulki Blasku Ksiezyca juz nie dropia.");
			else if (kind == playerbot_events::KIND_BOSS_LOOT)
				snprintf(text, sizeof(text), "Event zakonczony: podwojny loot z bossow.");
			else if (kind == playerbot_events::KIND_METIN_LOOT)
				snprintf(text, sizeof(text), "Event zakonczony: podwojny loot z Metinow.");
			else if (kind == playerbot_events::KIND_GOBLIN)
				snprintf(text, sizeof(text), "Event zakonczony: Poszukiwanie skarbow z Goblinem Skarbow.");
			// MT2009_PLUS_CATCH_KING_V1: Catch the King's top ten collect too.
			else if (kind == playerbot_events::KIND_RUMI || kind == playerbot_events::KIND_YUTNORI || kind == playerbot_events::KIND_CATCHKING)
				snprintf(text, sizeof(text), "Event zakonczony: %s. Nagrody za ranking mozna odebrac przy stole przez 7 dni.",
						PlayerBotMiniGameEventName(kind));
			else if (PlayerBotMiniGameEventName(kind))
				snprintf(text, sizeof(text), "Event zakonczony: %s.", PlayerBotMiniGameEventName(kind));
			else
				snprintf(text, sizeof(text), "Event zakonczony: %s wraca do normy.", PlayerBotEventRateWord(kind));
		}
		else
		{
			char when[16];
			FormatPlayerBotEventClock(until, when, sizeof(when));
			snprintf(text, sizeof(text), "%s: %s do %s!",
					phase == EVENT_PHASE_START ? "Event" : "Trwa event", body, when);
		}
		BroadcastNotice(text);
		sys_log(0, "PLAYERBOT_EVENT: notice \"%s\"", text);
	}

	// The world's rate times (100 + value) percent, from a base remembered in
	// a flag of its own; asked again after a restart it finds that base and
	// arrives at the same boosted number, which is what makes this safe to
	// repeat.
	void BeginPlayerBotRateEvent(int kind, int value)
	{
		quest::CQuestManager& q = quest::CQuestManager::instance();
		for (int premium = 0; premium < 2; ++premium)
		{
			const std::string flag = PlayerBotEventRateFlag(kind, premium != 0);
			const std::string baseFlag = PlayerBotEventBaseFlag(kind, premium != 0);
			int base = q.GetEventFlag(baseFlag);
			if (base <= 0)
			{
				base = q.GetEventFlag(flag);
				if (base <= 0)
					base = 100;
				q.RequestSetEventFlag(baseFlag, base);
			}
			const int boosted = (int)((long long)base * (100 + value) / 100);
			q.RequestSetEventFlag(flag, boosted);
			sys_log(0, "PLAYERBOT_EVENT: %s begins: %s %d -> %d (+%d%%)",
					playerbot_events::KindName(kind), flag.c_str(), base, boosted, value);
		}
	}

	// The end gives the live flag the base back, whatever it holds: while an
	// event runs the base IS the operator's setting - both panels write it
	// there during an event (persist_rates_mt2009) and HoldPlayerBotRateEvent
	// keeps the live flag at the boost of it - so there is no other number to
	// return to. Until 2.0.95 a live flag that had moved was "left where the
	// operator put it" and the base kept, and the page read the base: a rate
	// saved during an event looked as if it had not saved, and did not change
	// after the event either ("gdy mamy odpalony event ... zmiana rat nie
	// dziala ... teraz nie mozna ich zmienic mimo ze zaden event nie jest
	// wlaczony", Monek, CarloMontana, 20 September).
	void EndPlayerBotRateEvent(int kind, int value)
	{
		quest::CQuestManager& q = quest::CQuestManager::instance();
		for (int premium = 0; premium < 2; ++premium)
		{
			const std::string flag = PlayerBotEventRateFlag(kind, premium != 0);
			const std::string baseFlag = PlayerBotEventBaseFlag(kind, premium != 0);
			const int base = q.GetEventFlag(baseFlag);
			if (base <= 0)
				continue;
			const int current = q.GetEventFlag(flag);
			q.RequestSetEventFlag(flag, base);
			q.RequestSetEventFlag(baseFlag, 0);
			sys_log(0, "PLAYERBOT_EVENT: %s ends: %s %d -> %d (+%d%% over)",
					playerbot_events::KindName(kind), flag.c_str(), current, base, value);
		}
	}

	// While a rate event runs, the live flag is the boost of the base and
	// nothing else. A panel that saved a rate during the event wrote the base
	// (and the plain figure to the live flag, which is what the in-game helper
	// sets too): this puts the boost back on top of the new base within a
	// second. A request goes out only when the flag disagrees, and again ten
	// seconds on if it still does - the flag moves only when the db core's
	// broadcast comes back.
	DWORD s_adwPlayerBotRateHoldAt[playerbot_events::KIND_MAX][2];

	void HoldPlayerBotRateEvent(int kind, int value, DWORD dwNow)
	{
		quest::CQuestManager& q = quest::CQuestManager::instance();
		for (int premium = 0; premium < 2; ++premium)
		{
			const std::string flag = PlayerBotEventRateFlag(kind, premium != 0);
			const int base = q.GetEventFlag(PlayerBotEventBaseFlag(kind, premium != 0));
			if (base <= 0)
				continue;
			const int boosted = (int)((long long)base * (100 + value) / 100);
			const int current = q.GetEventFlag(flag);
			DWORD& at = s_adwPlayerBotRateHoldAt[kind][premium];
			if (current == boosted || (at != 0 && (int)(dwNow - at) < 10000))
				continue;
			at = dwNow;
			q.RequestSetEventFlag(flag, boosted);
			sys_log(0, "PLAYERBOT_EVENT: %s holds %s at %d (base %d +%d%%, was %d)",
					playerbot_events::KindName(kind), flag.c_str(), boosted, base, value, current);
		}
	}

	// Once, a little after a start, for a kind no event runs on: a base left
	// behind by an event that ended while the core was down, or by a version
	// before 2.0.95. The live flag is the boost of it for one of the event
	// figures on the schedule - then the event's end never happened and the
	// base is the setting - or it is not, and the operator's number stands.
	// Either way the base goes, which is what lets both pages show the real
	// rate again: they read the base first.
	bool s_bPlayerBotRateBasesReconciled = false;
	DWORD s_dwPlayerBotRateReconcileAt = 0;

	void ReconcilePlayerBotRateBases(const bool active[playerbot_events::KIND_MAX])
	{
		quest::CQuestManager& q = quest::CQuestManager::instance();
		for (int kind = playerbot_events::KIND_EXP; kind < playerbot_events::KIND_MAX; ++kind)
		{
			if (!playerbot_events::IsRateKind(kind) || active[kind])
				continue;
			for (int premium = 0; premium < 2; ++premium)
			{
				const std::string flag = PlayerBotEventRateFlag(kind, premium != 0);
				const std::string baseFlag = PlayerBotEventBaseFlag(kind, premium != 0);
				const int base = q.GetEventFlag(baseFlag);
				if (base <= 0)
					continue;
				const int current = q.GetEventFlag(flag);
				bool boosted = false;
				for (size_t i = 0; i < s_vecPlayerBotEvents.size() && !boosted; ++i)
					if (s_vecPlayerBotEvents[i].kind == kind &&
							current == (int)((long long)base * (100 + s_vecPlayerBotEvents[i].value) / 100))
						boosted = true;
				if (boosted)
					q.RequestSetEventFlag(flag, base);
				q.RequestSetEventFlag(baseFlag, 0);
				sys_log(0, "PLAYERBOT_EVENT: stale base cleared %s base=%d live=%d -> %d",
						baseFlag.c_str(), base, current, boosted ? base : current);
			}
		}
	}

	void ApplyPlayerBotChestGate(bool closed)
	{
		const DWORD generation = GetPlayerBotWeightsGeneration();
		if (generation != s_dwPlayerBotEventsWeightsGeneration)
		{
			s_dwPlayerBotEventsWeightsGeneration = generation;
			// The sliders' figure as parsed (playerbot_config.h), never the
			// engine's variable: the parse no longer writes that while this
			// gate is shut, so reading it back here would capture the zero.
			s_iPlayerBotChestWantedPermille = GetPlayerBotChestConfigPermille(false);
			s_iPlayerBotChestStoneWantedPermille = GetPlayerBotChestConfigPermille(true);
		}
		if (closed != s_bPlayerBotChestGateClosed)
		{
			s_bPlayerBotChestGateClosed = closed;
			sys_log(0, "PLAYERBOT_EVENT: moonlight chests %s (kill %d, stone %d permille)",
					closed ? "wait for a chest event" : "drop",
					s_iPlayerBotChestWantedPermille, s_iPlayerBotChestStoneWantedPermille);
		}
		g_iMoonlightChestPermille = closed ? 0 : s_iPlayerBotChestWantedPermille;
		g_iMoonlightChestStonePermille = closed ? 0 : s_iPlayerBotChestStoneWantedPermille;
	}

	// MT2009_PLUS_BONUS_COUNT_PRICE_V1: when the world last had a Moonlight
	// chest event, for the owner's bonus-count prices
	// (playerbot_price_rules::BonusCountPricingOn): on only while no chest
	// event runs and none ran in the last fourteen days. The last second a
	// chest event was seen running is kept in an event flag of its own, so a
	// restart or another core knows it - written by every core that sees the
	// event (all of them judge the same file), at most every ten minutes and
	// once more as it ends. An "activate now" chest line still in the file
	// counts by its end, so an event from before this flag existed is not
	// forgotten. For the first minute of a core's life the rule stays off:
	// the event flags have not come from the db core yet, and a zero then is
	// not "never".
	const char* const PLAYERBOT_MOONLIGHT_LAST_FLAG = "mt2009_moonlight_last";
	const long PLAYERBOT_MOONLIGHT_LAST_WRITE_SECONDS = 600;
	const DWORD PLAYERBOT_BONUS_COUNT_WARMUP_MS = 60000;
	long s_lPlayerBotMoonlightSeen = 0;
	long s_lPlayerBotMoonlightWritten = 0;
	bool s_bPlayerBotMoonlightWasRunning = false;
	DWORD s_dwPlayerBotBonusCountSince = 0;
	bool s_bPlayerBotBonusCountReady = false;
	bool s_bPlayerBotBonusCountOn = false;

	bool IsPlayerBotBonusCountPricingOn()
	{
		return s_bPlayerBotBonusCountOn;
	}

	// The latest end of an "activate now" chest line of the file that has
	// ended, zero for none.
	long GetPlayerBotMoonlightFileLast(long now)
	{
		long last = 0;
		for (size_t i = 0; i < s_vecPlayerBotEvents.size(); ++i)
		{
			const playerbot_events::Window& w = s_vecPlayerBotEvents[i];
			if (w.kind == playerbot_events::KIND_CHEST && w.now && w.until > 0 && w.until <= now)
				last = std::max(last, w.until);
		}
		return last;
	}

	void UpdatePlayerBotBonusCountPricing(DWORD dwNow, bool chestNow)
	{
		const long now = (long)time(NULL);
		if (s_dwPlayerBotBonusCountSince == 0)
			s_dwPlayerBotBonusCountSince = dwNow ? dwNow : 1;
		quest::CQuestManager& q = quest::CQuestManager::instance();
		if (chestNow)
			s_lPlayerBotMoonlightSeen = now;
		const bool ended = !chestNow && s_bPlayerBotMoonlightWasRunning;
		s_bPlayerBotMoonlightWasRunning = chestNow;
		if (!s_bPlayerBotBonusCountReady)
		{
			if ((int)(dwNow - s_dwPlayerBotBonusCountSince) < (int)PLAYERBOT_BONUS_COUNT_WARMUP_MS)
				return;
			s_bPlayerBotBonusCountReady = true;
		}
		const long flag = (long)q.GetEventFlag(PLAYERBOT_MOONLIGHT_LAST_FLAG);
		const long last = std::max(std::max(flag, s_lPlayerBotMoonlightSeen), GetPlayerBotMoonlightFileLast(now));
		// Kept for the world: while the event runs, as it ends, and when this
		// core knows of a later one than the flag does.
		if (last > flag && last != s_lPlayerBotMoonlightWritten &&
				(ended || !chestNow || now - s_lPlayerBotMoonlightWritten >= PLAYERBOT_MOONLIGHT_LAST_WRITE_SECONDS))
		{
			s_lPlayerBotMoonlightWritten = last;
			q.RequestSetEventFlag(PLAYERBOT_MOONLIGHT_LAST_FLAG, (int)last);
		}
		const bool on = playerbot_price_rules::BonusCountPricingOn(chestNow, last, now,
				playerbot_price_rules::MOONLIGHT_QUIET_SECONDS);
		static bool s_bLogged = false;
		if (on != s_bPlayerBotBonusCountOn || !s_bLogged)
		{
			s_bLogged = true;
			s_bPlayerBotBonusCountOn = on;
			sys_log(0, "PLAYERBOT_MARKET: bonus-count prices %s (moonlight event %s, last seen %ld, %ld s ago)",
					on ? "ON" : "OFF", chestNow ? "running" : "not running", last,
					last > 0 ? now - last : -1L);
		}
	}

	// What a world event has put into the world and who answered it, for the
	// status columns: "host alive killed bots phase" - host 1 on the core
	// that runs it (playerbot_world_events.h, which comes after this file).
	void FormatPlayerBotWorldEventColumns(int kind, char* out, size_t size);
	// And a row for every event this core runs, "<kind>@<map>", after them.
	void WritePlayerBotWorldEventRows(FILE* fp, long written);

	// The first eight columns are what every panel has read since 2.0.74; the
	// rest came with Tanaka and Zuo, and an older panel stops at the eighth.
	void WritePlayerBotEventsStatus()
	{
		const char* tempPath = "playerbot_events_status.tsv.tmp";
		FILE* fp = fopen(tempPath, "wb");
		if (!fp)
			return;
		fprintf(fp, "kind\tscheduled\tactive\tvalue\tuntil\tnext_start\tnext_value\twritten"
				"\tmap\tsince\tnext_map\thost\talive\tkilled\tbots\tphase\n");
		const long now = (long)time(NULL);
		for (int kind = 0; kind < playerbot_events::KIND_MAX; ++kind)
		{
			const playerbot_events::Status& st = s_aPlayerBotEventStatus[kind];
			char world[96];
			if (playerbot_events::IsWorldKind(kind))
				FormatPlayerBotWorldEventColumns(kind, world, sizeof(world));
			else
				snprintf(world, sizeof(world), "0\t0\t0\t0\t-");
			fprintf(fp, "%s\t%d\t%d\t%d\t%ld\t%ld\t%d\t%ld\t%ld\t%ld\t%ld\t%s\n",
					playerbot_events::KindName(kind),
					st.scheduled ? 1 : 0, st.active ? 1 : 0, st.value, st.until, st.nextStart,
					st.nextValue, now, st.map, st.since, st.nextMap, world);
		}
		WritePlayerBotWorldEventRows(fp, now);
		fclose(fp);
		rename(tempPath, PLAYERBOT_EVENTS_STATUS_PATH);
	}

	void ManagePlayerBotEvents(DWORD dwNow)
	{
		RefreshPlayerBotEvents(dwNow);
		if (dwNow >= s_dwPlayerBotEventsNextCheck)
		{
			s_dwPlayerBotEventsNextCheck = dwNow + PLAYERBOT_EVENTS_CHECK_INTERVAL;
			const time_t now = time(NULL);
			struct tm local;
			localtime_r(&now, &local);
			const int dayIndex = (local.tm_wday + 6) % 7;
			const int minute = local.tm_hour * 60 + local.tm_min;
			const bool leader = IsPlayerBotEventLeader();
			for (int kind = 0; kind < playerbot_events::KIND_MAX; ++kind)
			{
				const playerbot_events::Status st = playerbot_events::Evaluate(
						s_vecPlayerBotEvents, kind, (long)now, dayIndex, minute);
				playerbot_events::Status& shown = s_aPlayerBotEventStatus[kind];
				if (st.active != shown.active || st.until != shown.until ||
						st.nextStart != shown.nextStart || st.scheduled != shown.scheduled ||
						st.value != shown.value)
					s_bPlayerBotEventsStatusDirty = true;
				shown = st;
				TPlayerBotEventState& state = s_aPlayerBotEventState[kind];
				// Tanaka and Zuo speak for themselves, with the map in the
				// sentence (playerbot_world_events.h); here they are only judged,
				// once for the kind and once for every map.
				const bool world = playerbot_events::IsWorldKind(kind);
				if (world)
				{
					std::vector<playerbot_events::Status> byMap;
					playerbot_events::EvaluateWorldByMap(s_vecPlayerBotEvents, kind, (long)now, dayIndex, minute, byMap);
					std::vector<playerbot_events::Status>& kept = s_aPlayerBotWorldActive[kind];
					bool changed = byMap.size() != kept.size();
					for (size_t i = 0; i < byMap.size() && !changed; ++i)
						changed = byMap[i].map != kept[i].map || byMap[i].until != kept[i].until;
					if (changed)
						s_bPlayerBotEventsStatusDirty = true;
					kept.swap(byMap);
				}
				if (st.active && !state.active)
				{
					state.active = true;
					state.value = st.value;
					state.until = st.until;
					state.nextReminder = dwNow + PlayerBotEventReminderInterval(kind);
					sys_log(0, "PLAYERBOT_EVENT: %s starts value=%d until=%ld map=%ld leader=%d",
							playerbot_events::KindName(kind), st.value, st.until, st.map, leader ? 1 : 0);
					if (leader && !world)
					{
						if (playerbot_events::IsRateKind(kind))
							BeginPlayerBotRateEvent(kind, st.value);
						AnnouncePlayerBotEvent(kind, st.value, st.until, EVENT_PHASE_START);
					}
				}
				else if (!st.active && state.active)
				{
					sys_log(0, "PLAYERBOT_EVENT: %s ends leader=%d", playerbot_events::KindName(kind), leader ? 1 : 0);
					if (leader && !world)
					{
						if (playerbot_events::IsRateKind(kind))
							EndPlayerBotRateEvent(kind, state.value);
						AnnouncePlayerBotEvent(kind, state.value, 0, EVENT_PHASE_END);
					}
					state.active = false;
				}
				else if (st.active)
				{
					// The live flag held at the boost of the base, which also
					// carries a second window or an "activate now" with another
					// figure onto the same base. Ending and beginning again, as
					// this did, asked for the base to be cleared and read it back
					// before the clearing had come round - the live flag stayed
					// boosted after the event.
					if (playerbot_events::IsRateKind(kind) && leader)
						HoldPlayerBotRateEvent(kind, st.value, dwNow);
					state.value = st.value;
					state.until = st.until;
					if (leader && !world && dwNow >= state.nextReminder)
					{
						state.nextReminder = dwNow + PlayerBotEventReminderInterval(kind);
						AnnouncePlayerBotEvent(kind, st.value, st.until, EVENT_PHASE_REMINDER);
					}
				}
			}
			// The bases an ended event left behind, half a minute into the
			// leader's life so the event flags have arrived from the db core.
			if (leader && !s_bPlayerBotRateBasesReconciled)
			{
				if (s_dwPlayerBotRateReconcileAt == 0)
					s_dwPlayerBotRateReconcileAt = dwNow + 30000;
				else if ((int)(dwNow - s_dwPlayerBotRateReconcileAt) >= 0)
				{
					s_bPlayerBotRateBasesReconciled = true;
					bool active[playerbot_events::KIND_MAX];
					for (int kind = 0; kind < playerbot_events::KIND_MAX; ++kind)
						active[kind] = s_aPlayerBotEventState[kind].active;
					ReconcilePlayerBotRateBases(active);
				}
			}
		}
		// Shut whenever no chest event runs, schedule or no schedule.
		const playerbot_events::Status& chest = s_aPlayerBotEventStatus[playerbot_events::KIND_CHEST];
		ApplyPlayerBotChestGate(!chest.active);
		// MT2009_PLUS_BONUS_COUNT_PRICE_V1: and the bonus-count prices' clock.
		UpdatePlayerBotBonusCountPricing(dwNow, chest.active);
		if (s_bPlayerBotEventsStatusDirty || dwNow >= s_dwPlayerBotEventsNextStatus)
		{
			s_bPlayerBotEventsStatusDirty = false;
			s_dwPlayerBotEventsNextStatus = dwNow + PLAYERBOT_EVENTS_STATUS_INTERVAL;
			WritePlayerBotEventsStatus();
		}
		// MT2009_PLUS_GOBLIN_V1: the Treasure Hunt follows its kind's status
		// (playerbot_goblin.h) - every core, like the double-loot events.
		GoblinEventTick(dwNow);
		// MT2009_PLUS_EVENT_MANAGER_V1: the in-game event manager follows the
		// mini games' kinds (playerbot_ingame_events.h) - every core.
		InGameEventTick(dwNow);
		// MT2009_PLUS_RUMI_V1: Rumi's season flags (the leader) and games (playerbot_rumi.h).
		RumiTick(dwNow);
		// MT2009_PLUS_CATCH_KING_V1: Catch the King's season flag (playerbot_catchking.h).
		CatchKingTick(dwNow);
	}
}

// MT2009_PLUS_LOOT_EVENTS_V1: the double-loot events, asked by the engine's
// CreateDropItem (item_manager.cpp) for every kill. Each core judges the
// panel's file itself every second, so this is the core's own answer.
bool Mt2009PlusDoubleLoot(LPCHARACTER victim)
{
	if (!victim || victim->IsPC())
		return false;
	if (victim->IsStone())
		return s_aPlayerBotEventStatus[playerbot_events::KIND_METIN_LOOT].active;
	if (victim->GetMobRank() >= MOB_RANK_BOSS)
		return s_aPlayerBotEventStatus[playerbot_events::KIND_BOSS_LOOT].active;
	return false;
}
