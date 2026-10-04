#ifndef __INC_METIN2_PLAYERBOT_REPRICE_NOW_H__
#define __INC_METIN2_PLAYERBOT_REPRICE_NOW_H__

// MT2009_PLUS_BOT_REPRICE_NOW_V1: "Przelicz ceny w sklepach botow teraz".
//
// After a change of the rates or an update the keepers' counters caught up
// at their own pace - a slice of lines a visit, the visits ten minutes to an
// hour apart (PLAYERBOT_OFFLINE_REPRICE_*) - so a counter could ask
// yesterday's prices for hours. The owner asked for a button that makes
// every bot reprice its whole counter at once ("przycisk w panelu, ktory
// wymuszalby na botach przetasowanie cen sklepu").
//
// Both panels touch /opt/m2spool/playerbot_reprice_now; every core watches
// the file's mtime, as it watches playerbot_catacomb_now, and starts a pass
// over the stands it hosts (an offline counter is this core's when it stands
// on this channel on a map hosted here - the rule the core's own slip
// correction keeps, CorrectPlayerBotStandingSlips - and a classic stall when
// its keeper is here). The pass:
//  - takes PLAYERBOT_REPRICE_NOW_BOTS_PER_SECOND keepers a second and sends
//    at most PLAYERBOT_REPRICE_NOW_EDITS_PER_SECOND edits a second, so the db
//    core sees a steady trickle, not two thousand counters at once;
//  - prices every line by BotOfflineRepriceTarget, the very rule of a
//    keeper's visit (markdown clock, markup, floors, operator prices);
//  - edits a line in place - the ikashop's SUBHEADER_GD_EDIT_ITEM, as the
//    core's slip correction does, no keeper walk, no take-down and relist; a
//    classic stall's line is edited in the CShop itself and shown to whoever
//    is looking (BroadcastUpdateItem);
//  - leaves a line alone whose price is within
//    PLAYERBOT_REPRICE_NOW_TOLERANCE_PERCENT of the new one and not under
//    its floor;
//  - waits for a keeper with its board open or a request of its own in
//    flight, a few seconds and a few times, before passing it over;
//  - stamps a keeper served here with the price generation, so its next
//    visit does not walk the counter again at the catch-up pace;
//  - logs "PLAYERBOT_REPRICE_NOW: start" and "...: done" with the keepers,
//    the lines looked at, changed and left within the tolerance, and the
//    seconds it took.
//
// The same pass starts by itself (optional, cheap):
//  - when the price key - the price table's version, the yang rate the panel
//    writes, the bonus-count pricing switch and the build of this binary -
//    differs from the one this core recorded (playerbot_reprice_key in the
//    core's own directory) PLAYERBOT_REPRICE_NOW_SETTLE_MS after the start:
//    an update (a new binary) or a rate changed while the server was down;
//  - when the key moves while the core runs (a rate moved in the panel),
//    PLAYERBOT_REPRICE_NOW_AUTO_DEBOUNCE_MS after the last move, so a few
//    values set one after another make one pass.
// The world's yang inflation is not in the key: it moves the prices in 2%
// bands all the time and the ordinary reprice follows it. A core with no
// key recorded yet only records it.

namespace
{
	const char* const PLAYERBOT_REPRICE_NOW_PATH = "/opt/m2spool/playerbot_reprice_now";
	const char* const PLAYERBOT_REPRICE_KEY_PATH = "playerbot_reprice_key";
	const int PLAYERBOT_REPRICE_NOW_BOTS_PER_SECOND = 4;
	const int PLAYERBOT_REPRICE_NOW_EDITS_PER_SECOND = 20;
	const int PLAYERBOT_REPRICE_NOW_TOLERANCE_PERCENT = 1;
	const int PLAYERBOT_REPRICE_NOW_BUSY_RETRIES = 5;
	const DWORD PLAYERBOT_REPRICE_NOW_CHECK_MS = 5000;
	const DWORD PLAYERBOT_REPRICE_NOW_SETTLE_MS = 5 * 60 * 1000;
	const DWORD PLAYERBOT_REPRICE_NOW_AUTO_DEBOUNCE_MS = 60 * 1000;

	struct TPlayerBotRepriceKeeper
	{
		DWORD pid;
		int retries;
		bool classic;
	};

	struct TPlayerBotRepricePass
	{
		bool running = false;
		std::string why;
		DWORD startedAt = 0;
		DWORD nextSecond = 0;
		std::deque<TPlayerBotRepriceKeeper> queue;
		// A keeper whose lines outran the second's edits goes on from here.
		DWORD cursorOwner = 0;
		DWORD cursorItem = 0;
		unsigned queued = 0, bots = 0, lines = 0, changed = 0, within = 0, busy = 0;
	};
	TPlayerBotRepricePass s_PlayerBotRepricePass;
	time_t s_tPlayerBotRepriceNowSeen = (time_t)-1;
	DWORD s_dwPlayerBotRepriceNextCheck = 0;
	DWORD s_dwPlayerBotRepriceBootAt = 0;
	bool s_bPlayerBotRepriceBootChecked = false;
	std::string s_strPlayerBotRepriceKeySeen;
	DWORD s_dwPlayerBotRepriceAutoAt = 0;

	// A classic stall's lines are the CShop's own, protected: read and written
	// through a class of ours, which may name the member (a pointer to it
	// applies to any CShop).
	struct CPlayerBotShopPriceAccess : public CShop
	{
		static std::vector<SHOP_ITEM>& Items(CShop* shop)
		{
			return shop->*(&CPlayerBotShopPriceAccess::m_itemVector);
		}
	};

	std::string GetPlayerBotRepriceKey()
	{
		char key[160];
		snprintf(key, sizeof(key), "v%u r%d b%d build %s %s",
				(unsigned int)PLAYERBOT_PRICE_TABLE_VERSION, GetPlayerBotPriceYangRate(),
				IsPlayerBotBonusCountPricingOn() ? 1 : 0, __DATE__, __TIME__);
		return key;
	}

	std::string ReadPlayerBotRepriceKey()
	{
		FILE* fp = fopen(PLAYERBOT_REPRICE_KEY_PATH, "r");
		if (!fp)
			return std::string();
		char line[256] = {0};
		if (!fgets(line, sizeof(line), fp))
			line[0] = 0;
		fclose(fp);
		std::string key(line);
		while (!key.empty() && (key.back() == '\n' || key.back() == '\r'))
			key.pop_back();
		return key;
	}

	void WritePlayerBotRepriceKey(const std::string& key)
	{
		FILE* fp = fopen(PLAYERBOT_REPRICE_KEY_PATH, "w");
		if (!fp)
			return;
		fprintf(fp, "%s\n", key.c_str());
		fclose(fp);
	}

	bool PlayerBotRepriceNowRequested()
	{
		struct stat st;
		if (stat(PLAYERBOT_REPRICE_NOW_PATH, &st) != 0)
			return false;
		// A file from before the start is no request: a touch is one press.
		if (s_tPlayerBotRepriceNowSeen == (time_t)-1)
		{
			s_tPlayerBotRepriceNowSeen = st.st_mtime;
			return false;
		}
		if (st.st_mtime <= s_tPlayerBotRepriceNowSeen)
			return false;
		s_tPlayerBotRepriceNowSeen = st.st_mtime;
		return true;
	}

	void StartPlayerBotRepricePass(DWORD dwNow, const char* why)
	{
		TPlayerBotRepricePass& pass = s_PlayerBotRepricePass;
		if (pass.running)
			sys_log(0, "PLAYERBOT_REPRICE_NOW: restarted why=%s (was %s, %u of %u keepers done)",
					why, pass.why.c_str(), pass.bots, pass.queued);
		pass = TPlayerBotRepricePass();
		pass.running = true;
		pass.why = why;
		pass.startedAt = dwNow;
		pass.nextSecond = dwNow;
		std::set<DWORD> seen;
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		for (const auto& [owner, shop] : ikashop::GetManager().GetPlayerBotOfflineShops())
		{
			if (!shop || shop->GetItems().empty() || !CPlayerBotManager::instance().IsRegisteredBotPID(owner))
				continue;
			const auto spawn = shop->GetSpawn();
			if (spawn.channel != g_bChannel || !IsPlayerBotMapHostedHere(spawn.map))
				continue;
			if (seen.insert(owner).second)
				pass.queue.push_back(TPlayerBotRepriceKeeper{ owner, 0, false });
		}
#endif
		for (TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.begin(); it != s_mapPlayerBotAIStates.end(); ++it)
		{
			if (it->second.vecShopOffers.empty() || seen.count(it->first))
				continue;
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!ch || !ch->GetMyShop())
				continue;
			pass.queue.push_back(TPlayerBotRepriceKeeper{ it->first, 0, true });
		}
		pass.queued = (unsigned int)pass.queue.size();
		sys_log(0, "PLAYERBOT_REPRICE_NOW: start why=%s keepers=%u channel=%u pace=%d/s edits=%d/s tolerance=%d%%",
				why, pass.queued, (unsigned int)g_bChannel, PLAYERBOT_REPRICE_NOW_BOTS_PER_SECOND,
				PLAYERBOT_REPRICE_NOW_EDITS_PER_SECOND, PLAYERBOT_REPRICE_NOW_TOLERANCE_PERCENT);
	}

	// Whether a line asking `asked` is to be edited to `price`.
	bool IsPlayerBotRepriceWorthEdit(long long asked, long long price, long long floor)
	{
		if (price <= 0 || price >= GOLD_MAX || price == asked)
			return false;
		if (asked < floor)
			return true;
		const long long moved = price > asked ? price - asked : asked - price;
		return moved * 100 > asked * PLAYERBOT_REPRICE_NOW_TOLERANCE_PERCENT;
	}

	enum EPlayerBotRepriceResult
	{
		REPRICE_KEEPER_DONE,
		REPRICE_KEEPER_BUSY,
		REPRICE_KEEPER_PAUSED,
	};

#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
	EPlayerBotRepriceResult RepricePlayerBotCounterNow(DWORD pid, DWORD dwNow, int& editsLeft)
	{
		TPlayerBotRepricePass& pass = s_PlayerBotRepricePass;
		auto& manager = ikashop::GetManager();
		auto shop = manager.GetShopByOwnerID(pid);
		if (!shop || shop->GetItems().empty())
			return REPRICE_KEEPER_DONE;
		const auto spawn = shop->GetSpawn();
		if (spawn.channel != g_bChannel || !IsPlayerBotMapHostedHere(spawn.map))
			return REPRICE_KEEPER_DONE;
		if (shop->IsEditMode() || playerbot_offline::requests.count(pid))
			return REPRICE_KEEPER_BUSY;
		TPlayerBotAIStateMap::iterator keeper = s_mapPlayerBotAIStates.find(pid);
		playerbot_offline::State* o = keeper != s_mapPlayerBotAIStates.end() ? &keeper->second.offlineShop : NULL;
		const DWORD generation = GetPlayerBotPriceGeneration();
		const bool generationMoved = !o || o->priceGeneration != generation;
		auto it = pass.cursorOwner == pid ? shop->GetItems().upper_bound(pass.cursorItem) : shop->GetItems().begin();
		for (; it != shop->GetItems().end(); ++it)
		{
			if (editsLeft <= 0)
			{
				pass.cursorOwner = pid;
				return REPRICE_KEEPER_PAUSED;
			}
			pass.cursorOwner = pid;
			pass.cursorItem = it->first;
			if (!it->second)
				continue;
			LPITEM preview = BotOfflinePreview(*it->second);
			if (!preview)
				continue;
			++pass.lines;
			TPlayerBotPricingKeeper pricing(pid);
			TPlayerBotPriceTraceScope priceTrace;
			const TBotOfflineRepriceTarget target = BotOfflineRepriceTarget(preview, it->first,
					o ? &o->listed : NULL, dwNow, priceTrace, generationMoved);
			const unsigned int flags = priceTrace.On()
					? priceTrace.trace.flags | priceTrace.trace.PriceFlags(target.price, preview->GetCount()) : 0;
			const std::string steps = priceTrace.On() ? priceTrace.trace.Encode() : std::string();
			const DWORD vnum = preview->GetVnum();
			const long long count = preview->GetCount();
			M2_DELETE(preview);
			const long long asked = (long long)it->second->GetPrice().yang;
			if (!IsPlayerBotRepriceWorthEdit(asked, target.price, target.floor))
			{
				++pass.within;
				continue;
			}
			ikashop::TPriceInfo price{};
			price.yang = target.price;
			manager.SendShopEditItemDBPacket(pid, it->first, price);
			--editsLeft;
			++pass.changed;
			QueuePlayerBotListingEvent(it->first, pid, vnum, count, per::EVENT_REPRICE, target.price, asked, steps, 0, 0,
					flags | (per::IsPriceJump(asked, target.price) ? per::LFLAG_PRICE_JUMP : 0));
		}
		pass.cursorOwner = 0;
		pass.cursorItem = 0;
		// Priced under this generation: the keeper's next visit reprices at
		// the ordinary pace, not the catch-up one, and not at once.
		if (o)
		{
			o->priceGeneration = generation;
			if (o->repriceSteps == 0)
				o->nextReprice = dwNow + PLAYERBOT_OFFLINE_REPRICE_MS;
		}
		return REPRICE_KEEPER_DONE;
	}
#endif

	// A classic stall: priced as when it opened (ManagePlayerBotPrivateShop) -
	// the clearance of a poor keeper, the markdown of the stands it came home
	// unsold, the markup, the floor - and edited in the shop itself.
	EPlayerBotRepriceResult RepricePlayerBotStallNow(DWORD pid, DWORD dwNow, int& editsLeft)
	{
		TPlayerBotRepricePass& pass = s_PlayerBotRepricePass;
		LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pid);
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(pid);
		if (!ch || !ch->GetMyShop() || st == s_mapPlayerBotAIStates.end())
			return REPRICE_KEEPER_DONE;
		TPlayerBotAIState& state = st->second;
		std::vector<CShop::SHOP_ITEM>& lines = CPlayerBotShopPriceAccess::Items(ch->GetMyShop());
		const bool poor = IsPlayerBotPoorKeeper(ch);
		for (size_t i = 0; i < state.vecShopOffers.size(); ++i)
		{
			TPlayerBotShopOffer& offer = state.vecShopOffers[i];
			if (offer.bSoldLogged || offer.bSlot >= lines.size())
				continue;
			CShop::SHOP_ITEM& line = lines[offer.bSlot];
			LPITEM item = FindPlayerBotOfferItem(ch, offer);
			if (!item || !line.pkItem || line.pkItem != item)
				continue;
			++pass.lines;
			TPlayerBotPricingKeeper pricing(pid);
			DWORD price = GetPlayerBotShopAskingPrice(item);
			if (poor)
				price = std::max<DWORD>(1, (DWORD)((unsigned long long)price *
						PLAYERBOT_SHOP_POOR_DISCOUNT_PERCENT / 100ULL));
			int cut = 0;
			std::map<DWORD, BYTE>::const_iterator unsold = state.mapStallUnsold.find(item->GetID());
			if (unsold != state.mapStallUnsold.end() && unsold->second > 0)
				cut = playerbot_price_rules::SteppedPercent(
						std::min<int>(unsold->second, PLAYERBOT_SHOP_UNSOLD_DISCOUNT_MAX_STANDS),
						PLAYERBOT_SHOP_UNSOLD_DISCOUNT_PERCENT, PLAYERBOT_SHOP_UNSOLD_DISCOUNT_MAX_TOTAL);
			price = GetPlayerBotListingPrice(item, price, cut, NULL);
			const DWORD floor = GetPlayerBotListingFloor(item);
			price = std::max(price, floor);
			const long long asked = (long long)line.price;
			if (!IsPlayerBotRepriceWorthEdit(asked, (long long)price, (long long)floor))
			{
				++pass.within;
				continue;
			}
			line.price = price;
			offer.dwPrice = price;
			ch->GetMyShop()->BroadcastUpdateItem(offer.bSlot);
			--editsLeft;
			++pass.changed;
		}
		return REPRICE_KEEPER_DONE;
	}

	void RunPlayerBotRepricePass(DWORD dwNow)
	{
		TPlayerBotRepricePass& pass = s_PlayerBotRepricePass;
		if (!pass.running || (int)(dwNow - pass.nextSecond) < 0)
			return;
		pass.nextSecond = dwNow + 1000;
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		if (!db_clientdesc || !db_clientdesc->IsPhase(PHASE_DBCLIENT))
			return;
#endif
		int botsLeft = PLAYERBOT_REPRICE_NOW_BOTS_PER_SECOND;
		int editsLeft = PLAYERBOT_REPRICE_NOW_EDITS_PER_SECOND;
		// A busy keeper goes to the back and is asked again; the rotation
		// stops once every keeper left this second has been asked.
		size_t asked = 0;
		const size_t inQueue = pass.queue.size();
		while (!pass.queue.empty() && botsLeft > 0 && editsLeft > 0 && asked < inQueue)
		{
			TPlayerBotRepriceKeeper keeper = pass.queue.front();
			pass.queue.pop_front();
			++asked;
			EPlayerBotRepriceResult result = REPRICE_KEEPER_DONE;
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
			if (!keeper.classic)
				result = RepricePlayerBotCounterNow(keeper.pid, dwNow, editsLeft);
			else
#endif
				result = RepricePlayerBotStallNow(keeper.pid, dwNow, editsLeft);
			if (result == REPRICE_KEEPER_PAUSED)
			{
				pass.queue.push_front(keeper);
				break;
			}
			if (result == REPRICE_KEEPER_BUSY)
			{
				if (++keeper.retries <= PLAYERBOT_REPRICE_NOW_BUSY_RETRIES)
					pass.queue.push_back(keeper);
				else
					++pass.busy;
				continue;
			}
			++pass.bots;
			--botsLeft;
		}
		if (!pass.queue.empty())
			return;
		pass.running = false;
		sys_log(0, "PLAYERBOT_REPRICE_NOW: done why=%s keepers=%u of %u lines=%u changed=%u within_tolerance=%u busy_skipped=%u duration_s=%u",
				pass.why.c_str(), pass.bots, pass.queued, pass.lines, pass.changed, pass.within, pass.busy,
				(unsigned int)((dwNow - pass.startedAt) / 1000));
		const std::string key = GetPlayerBotRepriceKey();
		s_strPlayerBotRepriceKeySeen = key;
		WritePlayerBotRepriceKey(key);
	}

	// The world pass: the panel's request, the automatic one, and the pass
	// under way moved on by a second's worth.
	void ManagePlayerBotRepriceNow(DWORD dwNow)
	{
		if (s_dwPlayerBotRepriceBootAt == 0)
			s_dwPlayerBotRepriceBootAt = dwNow ? dwNow : 1;
		if (s_dwPlayerBotRepriceNextCheck == 0 || (int)(dwNow - s_dwPlayerBotRepriceNextCheck) >= 0)
		{
			s_dwPlayerBotRepriceNextCheck = dwNow + PLAYERBOT_REPRICE_NOW_CHECK_MS;
			if (PlayerBotRepriceNowRequested())
				StartPlayerBotRepricePass(dwNow, "panel");
			// Nothing automatic before the stands are loaded and the event
			// flags (the yang rate) have come from the db.
			if (dwNow - s_dwPlayerBotRepriceBootAt >= PLAYERBOT_REPRICE_NOW_SETTLE_MS)
			{
				const std::string key = GetPlayerBotRepriceKey();
				if (!s_bPlayerBotRepriceBootChecked)
				{
					s_bPlayerBotRepriceBootChecked = true;
					const std::string stored = ReadPlayerBotRepriceKey();
					s_strPlayerBotRepriceKeySeen = key;
					if (stored.empty())
						WritePlayerBotRepriceKey(key);
					else if (stored != key)
					{
						sys_log(0, "PLAYERBOT_REPRICE_NOW: price key changed since the last run (\"%s\" -> \"%s\")",
								stored.c_str(), key.c_str());
						if (!s_PlayerBotRepricePass.running)
							StartPlayerBotRepricePass(dwNow, "update");
					}
				}
				else if (key != s_strPlayerBotRepriceKeySeen)
				{
					sys_log(0, "PLAYERBOT_REPRICE_NOW: price key moved (\"%s\" -> \"%s\"), a pass in %u s",
							s_strPlayerBotRepriceKeySeen.c_str(), key.c_str(),
							(unsigned int)(PLAYERBOT_REPRICE_NOW_AUTO_DEBOUNCE_MS / 1000));
					s_strPlayerBotRepriceKeySeen = key;
					s_dwPlayerBotRepriceAutoAt = dwNow + PLAYERBOT_REPRICE_NOW_AUTO_DEBOUNCE_MS;
					if (!s_dwPlayerBotRepriceAutoAt)
						s_dwPlayerBotRepriceAutoAt = 1;
				}
				if (s_dwPlayerBotRepriceAutoAt && (int)(dwNow - s_dwPlayerBotRepriceAutoAt) >= 0)
				{
					s_dwPlayerBotRepriceAutoAt = 0;
					StartPlayerBotRepricePass(dwNow, "rates");
				}
			}
		}
		RunPlayerBotRepricePass(dwNow);
	}
}

#endif
