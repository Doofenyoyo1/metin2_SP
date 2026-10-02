#ifndef __INC_METIN2_PLAYERBOT_HAGGLE_H__
#define __INC_METIN2_PLAYERBOT_HAGGLE_H__

// MT2009_PLUS_BOT_HAGGLE_V1: a bot haggles with a person over a line of the
// person's offline shop it finds too dear.
//
// "Boty targuja sie o cene" (Remigiusz): a bot that wants a finished piece of
// gear - a weapon or an armour piece at +6 and up that WantsPlayerBotStallItem
// takes - on a person's shop, but whose purse refuses the price the shop
// asks (CanPlayerBotPayForOffer, the bots' cap on a person's price included:
// MT2009_PLUS_PERSON_PRICE_CAP_V1, at most 1.5 to 2 times what the market
// asks for the piece), whispers the owner an offer, with the piece linked:
//
// - its offer is four fifths of the most it would pay, and the most it would
//   pay - the highest price CanPlayerBotPayForOffer still lets through, read
//   by halving - is its last price; a person's cap is never passed, since the
//   purse that sets the last price is the one that buys;
// - the owner answers "ok" (the bot's last price is agreed), "nie" (the haggle
//   ends, and the piece is not asked about again for twelve hours) or a price
//   of his own ("5kk"): one the bot would pay is agreed, one over it is
//   answered once with the last price; or he simply lowers the price on the
//   shop - the bot watches the line;
// - once the line asks the price agreed (or, with nothing agreed, the bot's
//   last price) or less, the bot goes and buys it through the ordinary
//   purchase (RunPlayerBotOfflinePick), which asks the purse again. A deal
//   waits PLAYERBOT_HAGGLE_DEAL_WAIT_MS for the shop, an offer
//   PLAYERBOT_HAGGLE_ANSWER_WAIT_MS for its answer.
//
// Only an owner in the game on the bot's core is asked; one haggle at a time
// with a person and for a bot, PLAYERBOT_HAGGLE_PERSON_HOUR offers an hour to
// one person PLAYERBOT_HAGGLE_PERSON_GAP_MS apart, one offer about the same
// line in three hours (twelve after a no), PLAYERBOT_HAGGLE_CORE_OPEN haggles
// at once on a core. The HAGGLE key of the panel's AI page switches it
// (IsPlayerBotHaggleEnabled); off, no offer is made and every deal is
// forgotten. What a bot buys at once is unchanged: the haggle starts only
// where the browse found nothing to buy.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_chat_trade.h (the whisper and the item
// link); playerbot_offline_market.h declares the two functions it calls.

#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
namespace
{
	const BYTE PLAYERBOT_HAGGLE_MIN_PLUS = 6;
	const long long PLAYERBOT_HAGGLE_OFFER_PERCENT = 80;
	// A shop asking more than this many times the bot's last price is not
	// haggled with: the two are not talking about the same piece.
	const long long PLAYERBOT_HAGGLE_MAX_GAP_PERCENT = 250;
	const DWORD PLAYERBOT_HAGGLE_ANSWER_WAIT_MS = 10 * 60 * 1000;
	const DWORD PLAYERBOT_HAGGLE_DEAL_WAIT_MS = 10 * 60 * 1000;
	const DWORD PLAYERBOT_HAGGLE_PERSON_GAP_MS = 15 * 60 * 1000;
	const unsigned int PLAYERBOT_HAGGLE_PERSON_HOUR = 3;
	const DWORD PLAYERBOT_HAGGLE_SAME_LINE_MS = 3 * 60 * 60 * 1000;
	const DWORD PLAYERBOT_HAGGLE_SAME_LINE_NO_MS = 12 * 60 * 60 * 1000;
	const DWORD PLAYERBOT_HAGGLE_BOT_GAP_MS = 30 * 60 * 1000;
	const DWORD PLAYERBOT_HAGGLE_MUTE_MS = 12 * 60 * 60 * 1000;
	const size_t PLAYERBOT_HAGGLE_CORE_OPEN = 3;
	// MT2009_PLUS_BOT_HAGGLE_V2: once the shop asks the price agreed, the bot
	// waits a moment (the owner may still be at his stand's edit window) and
	// then buys; the deal no longer runs out on the answer's clock but holds
	// PLAYERBOT_HAGGLE_MET_WAIT_MS for the bot to get to the stand, and while
	// a deal is open the bot does not leave the shop's map (the world travel
	// asks IsPlayerBotHaggleHoldingMap).
	const DWORD PLAYERBOT_HAGGLE_BUY_DELAY_MS = 3 * 1000;
	const DWORD PLAYERBOT_HAGGLE_MET_WAIT_MS = 20 * 60 * 1000;
	const DWORD PLAYERBOT_HAGGLE_CHANNEL_ASK_MS = 30 * 1000;

	enum EPlayerBotHaggleStep
	{
		PLAYERBOT_HAGGLE_OFFERED,
		PLAYERBOT_HAGGLE_COUNTERED,
		PLAYERBOT_HAGGLE_AGREED,
	};

	struct TPlayerBotHaggle
	{
		DWORD dwOwner;
		std::string strOwnerName;
		DWORD dwItem;
		DWORD dwVnum;
		long long llListed;
		long long llOffer;      // the last price the bot said
		long long llCeiling;    // the most it pays
		long long llAgreed;     // 0 until a price is agreed
		BYTE bStep;
		bool bUnsureSaid;
		DWORD dwStartedAt;
		DWORD dwSaidAt;
		DWORD dwMetAt;          // MT2009_PLUS_BOT_HAGGLE_V2: when the shop first asked the price, 0 while it does not
		DWORD dwChannelAskAt;
	};
	std::map<DWORD, TPlayerBotHaggle> s_mapPlayerBotHaggles;   // by the bot's pid
	std::map<DWORD, std::deque<DWORD> > s_mapPlayerBotHaggleOffersTo; // by the person's pid
	std::map<std::pair<DWORD, DWORD>, DWORD> s_mapPlayerBotHaggleLineUntil; // (owner, item)
	std::map<std::pair<DWORD, DWORD>, DWORD> s_mapPlayerBotHaggleMuted; // (person, bot)
	std::map<DWORD, DWORD> s_mapPlayerBotHaggleBotNext;
	bool s_bPlayerBotHaggleWasOn = true;

	struct TPlayerBotHaggleCensus
	{
		unsigned int offers, deals, bought, declined, gaveUp, lapsed, gone;
		TPlayerBotHaggleCensus() : offers(0), deals(0), bought(0), declined(0), gaveUp(0), lapsed(0), gone(0) {}
	};
	TPlayerBotHaggleCensus s_kPlayerBotHaggleCensus;

	// A price the way a person says it, rounded down to two significant
	// figures ("1.4kk", "850k"): a bot's offer of 1 487 312 reads as a machine.
	long long RoundPlayerBotHagglePrice(long long v)
	{
		if (v <= 0)
			return 0;
		long long unit = 1;
		while (v / unit >= 100)
			unit *= 10;
		return std::max<long long>(1, v / unit * unit);
	}

	std::string SayPlayerBotHagglePrice(long long v)
	{
		return playerbot_conv::FormatYang(v);
	}

	void EndPlayerBotHaggle(DWORD botPid, const char* why, DWORD lineFor)
	{
		std::map<DWORD, TPlayerBotHaggle>::iterator it = s_mapPlayerBotHaggles.find(botPid);
		if (it == s_mapPlayerBotHaggles.end())
			return;
		const DWORD now = get_dword_time();
		if (lineFor != 0)
			s_mapPlayerBotHaggleLineUntil[std::make_pair(it->second.dwOwner, it->second.dwItem)] = now + lineFor;
		s_mapPlayerBotHaggleBotNext[botPid] = now + PLAYERBOT_HAGGLE_BOT_GAP_MS;
		sys_log(0, "PLAYERBOT_HAGGLE: over pid=%u owner=%u to=%s item=%u vnum=%u why=%s listed=%lld offer=%lld ceiling=%lld agreed=%lld seconds=%u",
				botPid, it->second.dwOwner, it->second.strOwnerName.c_str(), it->second.dwItem, it->second.dwVnum, why,
				it->second.llListed, it->second.llOffer, it->second.llCeiling, it->second.llAgreed,
				(unsigned int)((now - it->second.dwStartedAt) / 1000));
		s_mapPlayerBotHaggles.erase(it);
	}

	// The switch went off: every deal is forgotten, said once.
	bool IsPlayerBotHaggleOn()
	{
		const bool on = IsPlayerBotHaggleEnabled();
		if (!on && s_bPlayerBotHaggleWasOn)
		{
			const size_t open = s_mapPlayerBotHaggles.size();
			s_mapPlayerBotHaggles.clear();
			sys_log(0, "PLAYERBOT_HAGGLE: haggling off, deals forgotten=%u", (unsigned int)open);
		}
		s_bPlayerBotHaggleWasOn = on;
		return on;
	}

	bool IsPlayerBotHagglingWith(DWORD personPid)
	{
		for (std::map<DWORD, TPlayerBotHaggle>::const_iterator it = s_mapPlayerBotHaggles.begin();
				it != s_mapPlayerBotHaggles.end(); ++it)
			if (it->second.dwOwner == personPid)
				return true;
		return false;
	}

	// The person's limits: no haggle open with him, the last offer a quarter
	// of an hour back and no more than three in the hour.
	bool MayPlayerBotHaggleWith(DWORD personPid, DWORD botPid, DWORD now)
	{
		if (IsPlayerBotHagglingWith(personPid))
			return false;
		std::map<std::pair<DWORD, DWORD>, DWORD>::iterator muted =
				s_mapPlayerBotHaggleMuted.find(std::make_pair(personPid, botPid));
		if (muted != s_mapPlayerBotHaggleMuted.end())
		{
			if ((long)(now - muted->second) < 0)
				return false;
			s_mapPlayerBotHaggleMuted.erase(muted);
		}
		std::deque<DWORD>& offers = s_mapPlayerBotHaggleOffersTo[personPid];
		while (!offers.empty() && now - offers.front() >= 60U * 60U * 1000U)
			offers.pop_front();
		if (offers.size() >= PLAYERBOT_HAGGLE_PERSON_HOUR)
			return false;
		return offers.empty() || now - offers.back() >= PLAYERBOT_HAGGLE_PERSON_GAP_MS;
	}

	// The most the bot pays for the piece: the highest price its purse still
	// lets through, by halving between nothing and the price asked.
	long long GetPlayerBotHaggleCeiling(LPCHARACTER ch, LPITEM preview, long long listed, DWORD owner)
	{
		long long lo = 0, hi = listed;
		for (int i = 0; i < 40 && hi - lo > 1; ++i)
		{
			const long long mid = lo + (hi - lo) / 2;
			if (CanPlayerBotPayForOffer(ch, preview, mid, owner))
				lo = mid;
			else
				hi = mid;
		}
		return lo;
	}

	void SayPlayerBotHaggle(LPCHARACTER bot, LPCHARACTER person, const char* text)
	{
		SendPlayerBotWhisper(bot, person, text);
	}

	// Declared in playerbot_offline_market.h: the browse found nothing to buy
	// at once; a person's line it would take at a lower price is offered for.
	bool TryStartPlayerBotHaggle(LPCHARACTER ch, TPlayerBotAIState& state,
			const std::vector<std::pair<int, NativeShop> >& shops, DWORD now)
	{
		if (!ch || !IsPlayerBotHaggleOn() || IsPlayerBotSidekickPID(ch->GetPlayerID()) ||
				s_mapPlayerBotHaggles.count(ch->GetPlayerID()) ||
				s_mapPlayerBotHaggles.size() >= PLAYERBOT_HAGGLE_CORE_OPEN)
			return false;
		{
			std::map<DWORD, DWORD>::const_iterator next = s_mapPlayerBotHaggleBotNext.find(ch->GetPlayerID());
			if (next != s_mapPlayerBotHaggleBotNext.end() && (long)(now - next->second) < 0)
				return false;
		}
		LPCHARACTER bestOwner = NULL;
		DWORD bestItem = 0, bestVnum = 0;
		long long bestListed = 0, bestCeiling = 0;
		std::string bestLink;
		for (size_t s = 0; s < shops.size(); ++s)
		{
			const NativeShop& shop = shops[s].second;
			if (!shop)
				continue;
			const DWORD owner = shop->GetOwnerPID();
			if (CPlayerBotManager::instance().IsRegisteredBotPID(owner))
				continue;
			LPCHARACTER person = CHARACTER_MANAGER::instance().FindByPID(owner);
			if (!person || !person->IsPC() || !person->GetDesc() || person->GetDesc()->IsBot() ||
					!MayPlayerBotHaggleWith(owner, ch->GetPlayerID(), now))
				continue;
			for (const auto& [id, line] : shop->GetItems())
			{
				const TItemTable* table = line ? line->GetTable() : NULL;
				if (!table || (table->bType != ITEM_WEAPON && table->bType != ITEM_ARMOR) ||
						line->GetInfo().count != 1 || (int)(line->GetInfo().vnum % 10) < PLAYERBOT_HAGGLE_MIN_PLUS)
					continue;
				std::map<std::pair<DWORD, DWORD>, DWORD>::iterator held =
						s_mapPlayerBotHaggleLineUntil.find(std::make_pair(owner, (DWORD)id));
				if (held != s_mapPlayerBotHaggleLineUntil.end())
				{
					if ((long)(now - held->second) < 0)
						continue;
					s_mapPlayerBotHaggleLineUntil.erase(held);
				}
				const long long listed = (long long)line->GetPrice().GetTotalYangAmount();
				if (listed <= 0)
					continue;
				LPITEM preview = BotOfflinePreview(*line);
				if (!preview)
					continue;
				long long ceiling = 0;
				const bool candidate = preview->GetRefineLevel() >= PLAYERBOT_HAGGLE_MIN_PLUS &&
						WantsPlayerBotStallItem(ch, preview) &&
						!CanPlayerBotPayForOffer(ch, preview, listed, owner) &&
						ch->GetEmptyInventory(preview->GetSize()) >= 0;
				if (candidate)
					ceiling = GetPlayerBotHaggleCeiling(ch, preview, listed, owner);
				std::string link;
				if (candidate && ceiling > 0 && listed * 100 <= ceiling * PLAYERBOT_HAGGLE_MAX_GAP_PERCENT &&
						ceiling > bestCeiling)
					link = MakePlayerBotItemLink(preview);
				M2_DELETE(preview);
				if (link.empty())
					continue;
				bestOwner = person;
				bestItem = id;
				bestVnum = line->GetInfo().vnum;
				bestListed = listed;
				bestCeiling = ceiling;
				bestLink = link;
			}
		}
		if (!bestOwner)
			return false;
		TPlayerBotHaggle h;
		h.dwOwner = bestOwner->GetPlayerID();
		h.strOwnerName = bestOwner->GetName();
		h.dwItem = bestItem;
		h.dwVnum = bestVnum;
		h.llListed = bestListed;
		h.llCeiling = bestCeiling;
		h.llOffer = std::min(bestCeiling, std::max<long long>(1,
				RoundPlayerBotHagglePrice(bestCeiling * PLAYERBOT_HAGGLE_OFFER_PERCENT / 100)));
		h.llAgreed = 0;
		h.bStep = PLAYERBOT_HAGGLE_OFFERED;
		h.bUnsureSaid = false;
		h.dwStartedAt = now;
		h.dwSaidAt = now;
		h.dwMetAt = 0;
		h.dwChannelAskAt = 0;
		s_mapPlayerBotHaggles[ch->GetPlayerID()] = h;
		s_mapPlayerBotHaggleOffersTo[h.dwOwner].push_back(now);
		++s_kPlayerBotHaggleCensus.offers;
		char text[CHAT_MAX_LEN + 1];
		snprintf(text, sizeof(text), "Hej, masz na sklepie %s za %s. Dam %s - pasuje? Odpisz ok, nie albo swoja cene (np. %s).",
				bestLink.c_str(), SayPlayerBotHagglePrice(bestListed).c_str(),
				SayPlayerBotHagglePrice(h.llOffer).c_str(), SayPlayerBotHagglePrice(h.llCeiling).c_str());
		// A link the line cannot hold whole goes out as the plain name.
		if (strlen(text) >= CHAT_MAX_LEN)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(bestVnum);
			snprintf(text, sizeof(text), "Hej, masz na sklepie %s za %s. Dam %s - pasuje? Odpisz ok, nie albo swoja cene.",
					proto ? proto->szLocaleName : "przedmiot", SayPlayerBotHagglePrice(bestListed).c_str(),
					SayPlayerBotHagglePrice(h.llOffer).c_str());
		}
		SayPlayerBotHaggle(ch, bestOwner, text);
		sys_log(0, "PLAYERBOT_HAGGLE: offer pid=%u name=%s level=%d gold=%lld owner=%u to=%s item=%u vnum=%u listed=%lld offer=%lld ceiling=%lld",
				ch->GetPlayerID(), ch->GetName(), (int)ch->GetLevel(), (long long)ch->GetGold(), h.dwOwner,
				h.strOwnerName.c_str(), h.dwItem, h.dwVnum, h.llListed, h.llOffer, h.llCeiling);
		(void)state;
		return true;
	}

	// The owner's answer, read the way the conversation reads a whisper.
	bool IsPlayerBotHaggleWord(const playerbot_conv::TTokens& t, const char* const* words, size_t n)
	{
		for (size_t i = 0; i < n; ++i)
			if (t.Has(words[i]))
				return true;
		return false;
	}

	// The whisper of the person a bot haggles with, to that bot. True when it
	// was the haggle's - everything else goes on to the conversation.
	bool HandlePlayerBotHaggleWhisper(LPCHARACTER from, LPCHARACTER bot, const char* text)
	{
		if (!from || !bot || !text)
			return false;
		std::map<DWORD, TPlayerBotHaggle>::iterator it = s_mapPlayerBotHaggles.find(bot->GetPlayerID());
		if (it == s_mapPlayerBotHaggles.end() || it->second.dwOwner != from->GetPlayerID())
			return false;
		if (!IsPlayerBotHaggleOn())
			return false;
		TPlayerBotHaggle& h = it->second;
		const DWORD now = get_dword_time();
		playerbot_conv::TTokens t;
		playerbot_conv::Normalize(text, t);
		static const char* const kDecline[] = { "nie", "no", "nope", "nah", "spadaj", "odpusc", "zapomnij", "niet", "nara" };
		static const char* const kMute[] = { "spam", "spamuj", "spamujesz", "pisz" };
		static const char* const kAccept[] = { "ok", "tak", "dobra", "dobrze", "pasuje", "zgoda", "git", "biore", "bierz",
				"yes", "deal", "sure", "sprzedam", "sprzedaje", "jasne", "spoko" };
		const long long amount = playerbot_conv::ParseYangAmount(t.words);
		char reply[CHAT_MAX_LEN + 1];
		reply[0] = '\0';
		const bool decline = IsPlayerBotHaggleWord(t, kDecline, sizeof(kDecline) / sizeof(kDecline[0]));
		if (decline && amount == 0)
		{
			if (IsPlayerBotHaggleWord(t, kMute, sizeof(kMute) / sizeof(kMute[0])))
				s_mapPlayerBotHaggleMuted[std::make_pair(from->GetPlayerID(), bot->GetPlayerID())] = now + PLAYERBOT_HAGGLE_MUTE_MS;
			++s_kPlayerBotHaggleCensus.declined;
			SayPlayerBotHaggle(bot, from, "Ok, rozumiem. Nie ma sprawy, powodzenia!");
			sys_log(0, "PLAYERBOT_HAGGLE: answer pid=%u name=%s from=%s owner=%u item=%u accept=0 decline=1 ask=0 amount=0 step=%u",
					bot->GetPlayerID(), bot->GetName(), from->GetName(), h.dwOwner, h.dwItem, (unsigned int)h.bStep);
			EndPlayerBotHaggle(bot->GetPlayerID(), "declined", PLAYERBOT_HAGGLE_SAME_LINE_NO_MS);
			return true;
		}
		if (amount > 0)
		{
			if (amount <= h.llCeiling)
			{
				h.llAgreed = std::max<long long>(1, amount);
				h.bStep = PLAYERBOT_HAGGLE_AGREED;
				h.dwSaidAt = now;
				++s_kPlayerBotHaggleCensus.deals;
				snprintf(reply, sizeof(reply), "Za %s biore! Ustaw taka cene na sklepie, to przyjde i kupie.",
						SayPlayerBotHagglePrice(h.llAgreed).c_str());
			}
			else if (h.bStep == PLAYERBOT_HAGGLE_OFFERED)
			{
				h.llOffer = h.llCeiling;
				h.bStep = PLAYERBOT_HAGGLE_COUNTERED;
				h.dwSaidAt = now;
				snprintf(reply, sizeof(reply), "Za %s za drogo. Moge dac najwyzej %s - to moja ostatnia cena.",
						SayPlayerBotHagglePrice(amount).c_str(), SayPlayerBotHagglePrice(h.llCeiling).c_str());
			}
			else
			{
				++s_kPlayerBotHaggleCensus.gaveUp;
				snprintf(reply, sizeof(reply), "Niestety, wiecej niz %s nie dam. Jakbys zmienil zdanie, opusc cene na sklepie.",
						SayPlayerBotHagglePrice(h.llCeiling).c_str());
				h.dwSaidAt = now;
				// The line is still watched: a price lowered to the last offer
				// is bought until the answer's clock runs out.
			}
		}
		else if (IsPlayerBotHaggleWord(t, kAccept, sizeof(kAccept) / sizeof(kAccept[0])))
		{
			h.llAgreed = h.llOffer;
			h.bStep = PLAYERBOT_HAGGLE_AGREED;
			h.dwSaidAt = now;
			++s_kPlayerBotHaggleCensus.deals;
			snprintf(reply, sizeof(reply), "Super! Ustaw na sklepie cene %s, to przyjde i kupie (poczekam do 10 minut).",
					SayPlayerBotHagglePrice(h.llAgreed).c_str());
		}
		else if (!h.bUnsureSaid)
		{
			h.bUnsureSaid = true;
			snprintf(reply, sizeof(reply), "Nie zrozumialem - napisz ok, nie albo swoja cene (np. %s).",
					SayPlayerBotHagglePrice(h.llOffer).c_str());
		}
		else
			return false;
		sys_log(0, "PLAYERBOT_HAGGLE: answer pid=%u name=%s from=%s owner=%u item=%u accept=%d decline=0 ask=%d amount=%lld step=%u price=%lld",
				bot->GetPlayerID(), bot->GetName(), from->GetName(), h.dwOwner, h.dwItem,
				h.bStep == PLAYERBOT_HAGGLE_AGREED ? 1 : 0, amount > 0 ? 1 : 0, amount, (unsigned int)h.bStep,
				h.llAgreed ? h.llAgreed : h.llOffer);
		if (reply[0])
			SayPlayerBotHaggle(bot, from, reply);
		return true;
	}

	// Declared in playerbot_offline_market.h: the bot's haggle moved along -
	// the line watched for the price agreed, the clocks run out. True when the
	// line is the buyer's pick now (o.buyOwner, o.buyItem).
	bool WatchPlayerBotHaggle(LPCHARACTER ch, TPlayerBotAIState& state, DWORD now)
	{
		if (!ch)
			return false;
		std::map<DWORD, TPlayerBotHaggle>::iterator it = s_mapPlayerBotHaggles.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotHaggles.end())
			return false;
		if (!IsPlayerBotHaggleOn())
			return false;
		TPlayerBotHaggle& h = it->second;
		auto shop = ikashop::GetManager().GetShopByOwnerID(h.dwOwner);
		decltype(shop->GetItem(h.dwItem)) line;
		if (shop)
			line = shop->GetItem(h.dwItem);
		if (!shop || !line || shop->GetDuration() == 0)
		{
			++s_kPlayerBotHaggleCensus.gone;
			LPCHARACTER person = CHARACTER_MANAGER::instance().FindByPID(h.dwOwner);
			if (person && person->GetDesc() && h.bStep == PLAYERBOT_HAGGLE_AGREED)
				SayPlayerBotHaggle(ch, person, "Widze, ze juz tego nie ma na sklepie. Nic straconego!");
			EndPlayerBotHaggle(ch->GetPlayerID(), "gone", 0);
			return false;
		}
		const DWORD wait = h.bStep == PLAYERBOT_HAGGLE_AGREED ? PLAYERBOT_HAGGLE_DEAL_WAIT_MS : PLAYERBOT_HAGGLE_ANSWER_WAIT_MS;
		const long long price = (long long)line->GetPrice().GetTotalYangAmount();
		const long long buyAt = h.llAgreed > 0 ? h.llAgreed : h.llOffer;
		const bool met = price > 0 && price <= buyAt;
		// MT2009_PLUS_BOT_HAGGLE_V2: the moment the shop first asks the price.
		if (met && h.dwMetAt == 0)
		{
			h.dwMetAt = now;
			sys_log(0, "PLAYERBOT_HAGGLE: price met pid=%u name=%s owner=%u item=%u price=%lld agreed=%lld edit=%d map=%ld shop_map=%ld",
					ch->GetPlayerID(), ch->GetName(), h.dwOwner, h.dwItem, price, h.llAgreed,
					shop->IsEditMode() ? 1 : 0, ch->GetMapIndex(), (long)shop->GetSpawn().map);
		}
		else if (!met)
			h.dwMetAt = 0;
		if (met && now - h.dwMetAt >= PLAYERBOT_HAGGLE_BUY_DELAY_MS && !shop->IsEditMode())
		{
			const auto spawn = shop->GetSpawn();
			const int shopChannel = CPlayerBotManager::instance().IsChannelTableMode()
					? playerbot_channel_rules::SHOP_CHANNEL : (int)g_bChannel;
			// A bot on another channel asks to be moved to the stands' and
			// keeps the deal: the purchase is made there, by this same watch.
			if ((int)g_bChannel != shopChannel && spawn.map == ch->GetMapIndex() &&
					(h.dwChannelAskAt == 0 || now - h.dwChannelAskAt >= PLAYERBOT_HAGGLE_CHANNEL_ASK_MS))
			{
				h.dwChannelAskAt = now;
				CPlayerBotManager::instance().RequestShopChannel(ch->GetPlayerID());
			}
			if (spawn.map == ch->GetMapIndex() && (int)spawn.channel == shopChannel && (int)g_bChannel == shopChannel)
			{
				auto& o = state.offlineShop;
				o.buyOwner = h.dwOwner;
				o.buyItem = h.dwItem;
				o.buyUntil = now + PLAYERBOT_MARKET_FAR_PICK_WALK_MS;
				o.farBuy = false;
				o.haggleItem = h.dwItem;
				o.hagglePrice = buyAt;
				ClaimPlayerBotLineUntil(h.dwItem, ch->GetPlayerID(), now, o.buyUntil);
				++s_kPlayerBotHaggleCensus.bought;
				sys_log(0, "PLAYERBOT_HAGGLE: buys pid=%u name=%s owner=%u to=%s item=%u vnum=%u price=%lld agreed=%lld waited_ms=%u",
						ch->GetPlayerID(), ch->GetName(), h.dwOwner, h.strOwnerName.c_str(), h.dwItem, h.dwVnum,
						price, h.llAgreed, (unsigned int)(now - h.dwMetAt));
				LPCHARACTER person = CHARACTER_MANAGER::instance().FindByPID(h.dwOwner);
				if (person && person->GetDesc())
					SayPlayerBotHaggle(ch, person, "Widze nowa cene - ide kupic, dzieki!");
				EndPlayerBotHaggle(ch->GetPlayerID(), "bought", PLAYERBOT_HAGGLE_SAME_LINE_MS);
				return true;
			}
		}
		// The clocks. A deal whose price the shop already asks waits for the
		// bot up to PLAYERBOT_HAGGLE_MET_WAIT_MS from that moment; the bot
		// never says the price did not change when it did.
		const bool lapsed = met ? now - h.dwMetAt >= PLAYERBOT_HAGGLE_MET_WAIT_MS
				: now - h.dwSaidAt >= wait;
		if (lapsed)
		{
			++s_kPlayerBotHaggleCensus.lapsed;
			LPCHARACTER person = CHARACTER_MANAGER::instance().FindByPID(h.dwOwner);
			if (person && person->GetDesc() && h.bStep == PLAYERBOT_HAGGLE_AGREED)
				SayPlayerBotHaggle(ch, person, met
						? "Nie dam rady dojsc do Twojego sklepu, wiec odpuszczam. Sorki i moze innym razem!"
						: "Cena na sklepie sie nie zmienila, wiec odpuszczam. Moze innym razem!");
			EndPlayerBotHaggle(ch->GetPlayerID(), met ? "met_unreached" :
					h.bStep == PLAYERBOT_HAGGLE_AGREED ? "deal_lapsed" : "no_answer",
					PLAYERBOT_HAGGLE_SAME_LINE_MS);
		}
		return false;
	}

	// MT2009_PLUS_BOT_HAGGLE_V2: declared in playerbot_travel.h - a bot with a
	// deal agreed (or its price already on the shop) stays on the shop's map
	// until it has bought or the deal is over.
	bool IsPlayerBotHaggleHoldingMap(LPCHARACTER ch)
	{
		if (!ch || s_mapPlayerBotHaggles.empty())
			return false;
		std::map<DWORD, TPlayerBotHaggle>::const_iterator it = s_mapPlayerBotHaggles.find(ch->GetPlayerID());
		if (it == s_mapPlayerBotHaggles.end() ||
				(it->second.bStep != PLAYERBOT_HAGGLE_AGREED && it->second.dwMetAt == 0))
			return false;
		auto shop = ikashop::GetManager().GetShopByOwnerID(it->second.dwOwner);
		return shop && shop->GetSpawn().map == ch->GetMapIndex();
	}
}
#endif

#endif
