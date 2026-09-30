#ifndef __INC_METIN2_PLAYERBOT_GIFT_TRADE_H__
#define __INC_METIN2_PLAYERBOT_GIFT_TRADE_H__

// A player hands a bot something through the trade window (the operator, 27
// September 2026). The bot stops where it stands, drops what it was doing and
// waits for the player to accept; then it looks at what the player put in the
// window with the same eyes it has for a drop on the ground
// (IsPlayerBotWantedLootItem, playerbot_loot.h). One thing it would pick up
// is enough: it accepts, the scrap beside it included, and whispers its
// thanks. Nothing but scrap, and it closes the window with a word about it.
// Yang alone is always welcome - a bot bends down for every pile.
//
// The bot never puts anything in the window, so accepting costs it nothing
// but bag room. The companion has its own trade (playerbot_sidekick.h,
// HandlePlayerBotSidekickTrade), and a bot's trade with another bot is closed.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// relies on playerbot_loot.h and playerbot_chat_trade.h above it and reopens
// the same anonymous namespace. Include it exactly once.

namespace
{
	// How long the bot stands at an open window the player never accepts.
	const DWORD PLAYERBOT_GIFT_TRADE_MAX_WAIT_MS = 120000;
	// The pause between the player's accept and the bot's, a person's glance
	// at the window rather than the same tick.
	const DWORD PLAYERBOT_GIFT_TRADE_GLANCE_MIN_MS = 700;
	const DWORD PLAYERBOT_GIFT_TRADE_GLANCE_MAX_MS = 1800;

	struct TPlayerBotGiftTrade
	{
		DWORD dwPartnerPID;
		DWORD dwSince;
		DWORD dwDecideAt;	// 0 while the player has not accepted
	};

	std::map<DWORD, TPlayerBotGiftTrade> s_mapPlayerBotGiftTrades;

	const char* const PLAYERBOT_GIFT_TRADE_SCRAP_REPLIES[] =
	{
		"co to za zlom",
		"po cholere mi to",
		"Sprzedaj to sobie do handlarki",
		"Wywal to na glebe",
		"nie chce tego, to syf",
	};

	// What the bag holds, by item id and count: what the trade brought is
	// what is new or larger afterwards (a potion pours into a stack it had).
	void SnapshotPlayerBotGiftBag(LPCHARACTER ch, std::map<DWORD, DWORD>& out)
	{
		out.clear();
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetCell() == cell)
				out[item->GetID()] = item->GetCount();
		}
	}

	int CountPlayerBotGiftReceived(LPCHARACTER ch, const std::map<DWORD, DWORD>& before)
	{
		int received = 0;
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell)
				continue;
			std::map<DWORD, DWORD>::const_iterator it = before.find(item->GetID());
			if (it == before.end() || item->GetCount() > it->second)
				++received;
		}
		return received;
	}

	// The player's side of the window: the engine marks every item put in it
	// (CExchange::AddItem, SetExchanging) and keeps the list to itself.
	void CollectPlayerBotGiftOffer(LPCHARACTER partner, std::vector<LPITEM>& out)
	{
		out.clear();
		for (WORD cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = partner->GetInventoryItem(cell);
			if (item && item->GetCell() == cell && item->IsExchanging())
				out.push_back(item);
		}
	}

	void EndPlayerBotGiftTrade(LPCHARACTER ch, CExchange* exchange)
	{
		s_mapPlayerBotGiftTrades.erase(ch->GetPlayerID());
		if (exchange)
			exchange->Cancel();
	}

	// true while a trade holds the bot: the rest of its tick waits.
	bool HandlePlayerBotGiftTrade(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch)
			return false;
		CExchange* exchange = ch->GetExchange();
		if (!exchange)
		{
			s_mapPlayerBotGiftTrades.erase(ch->GetPlayerID());
			return false;
		}
		if (IsPlayerBotSidekickPID(ch->GetPlayerID()))
			return false;

		CExchange* other = exchange->GetCompany();
		LPCHARACTER partner = other ? other->GetOwner() : NULL;
		if (!partner || !partner->GetDesc() || partner->GetDesc()->IsBot())
		{
			EndPlayerBotGiftTrade(ch, exchange);
			return true;
		}

		std::map<DWORD, TPlayerBotGiftTrade>::iterator rec = s_mapPlayerBotGiftTrades.find(ch->GetPlayerID());
		if (rec == s_mapPlayerBotGiftTrades.end() || rec->second.dwPartnerPID != partner->GetPlayerID())
		{
			TPlayerBotGiftTrade fresh;
			fresh.dwPartnerPID = partner->GetPlayerID();
			fresh.dwSince = dwNow;
			fresh.dwDecideAt = 0;
			rec = s_mapPlayerBotGiftTrades.insert(std::make_pair(ch->GetPlayerID(), fresh)).first;
			rec->second = fresh;
			sys_log(0, "PLAYERBOT_GIFT: trade opened pid=%u name=%s partner=%s",
					ch->GetPlayerID(), ch->GetName(), partner->GetName());
		}

		// Standing still, doing nothing else, for as long as the window is open.
		ch->SetVictim(NULL);
		if (ch->IsStateMove())
			ch->Stop();
		SetPlayerBotAction(state, BOT_ACTION_IDLE, dwNow);
		state.dwLastMeaningfulActivityTime = dwNow;

		if (dwNow - rec->second.dwSince > PLAYERBOT_GIFT_TRADE_MAX_WAIT_MS)
		{
			sys_log(0, "PLAYERBOT_GIFT: gave up waiting pid=%u name=%s partner=%s",
					ch->GetPlayerID(), ch->GetName(), partner->GetName());
			EndPlayerBotGiftTrade(ch, exchange);
			return true;
		}

		// Only after the player: accepting first would close the trade on
		// whatever was in the window at that moment.
		if (!other->GetAcceptStatus() || exchange->GetAcceptStatus())
		{
			rec->second.dwDecideAt = 0;
			return true;
		}
		if (rec->second.dwDecideAt == 0)
		{
			rec->second.dwDecideAt = dwNow + number(PLAYERBOT_GIFT_TRADE_GLANCE_MIN_MS,
					PLAYERBOT_GIFT_TRADE_GLANCE_MAX_MS);
			return true;
		}
		if (dwNow < rec->second.dwDecideAt)
			return true;

		std::vector<LPITEM> offer;
		CollectPlayerBotGiftOffer(partner, offer);
		const bool choosy = IsPlayerBotChoosyLooter(ch) && !IsPlayerBotDemonTowerInstance(ch->GetMapIndex());
		const bool medalDropper = GetPlayerBotPersonalityByPID(ch->GetPlayerID()) == BOT_PERSONALITY_MEDAL_DROPPER;
		int wanted = 0, fits = 0;
		for (size_t i = 0; i < offer.size(); ++i)
		{
			if (!IsPlayerBotWantedLootItem(ch, offer[i], choosy, medalDropper))
				continue;
			++wanted;
			if (PlayerBotBagTakesDrop(ch, offer[i]))
				++fits;
		}

		if (!offer.empty() && wanted == 0)
		{
			const char* reply = PLAYERBOT_GIFT_TRADE_SCRAP_REPLIES[number(0,
					(int)(sizeof(PLAYERBOT_GIFT_TRADE_SCRAP_REPLIES) / sizeof(PLAYERBOT_GIFT_TRADE_SCRAP_REPLIES[0])) - 1)];
			sys_log(0, "PLAYERBOT_GIFT: refused scrap pid=%u name=%s partner=%s items=%u",
					ch->GetPlayerID(), ch->GetName(), partner->GetName(), (unsigned int)offer.size());
			EndPlayerBotGiftTrade(ch, exchange);
			SendPlayerBotWhisper(ch, partner, reply);
			return true;
		}
		if (wanted > 0 && fits == 0)
		{
			sys_log(0, "PLAYERBOT_GIFT: no room pid=%u name=%s partner=%s wanted=%d",
					ch->GetPlayerID(), ch->GetName(), partner->GetName(), wanted);
			EndPlayerBotGiftTrade(ch, exchange);
			SendPlayerBotWhisper(ch, partner, "nie mam juz miejsca w torbie");
			return true;
		}

		// The trade ends inside this call when it goes through, and the two
		// exchanges with it.
		std::map<DWORD, DWORD> before;
		SnapshotPlayerBotGiftBag(ch, before);
		const long long goldBefore = (long long)ch->GetGold();
		s_mapPlayerBotGiftTrades.erase(ch->GetPlayerID());
		exchange->Accept(true);
		if (ch->GetExchange())
			return true;

		const int received = CountPlayerBotGiftReceived(ch, before);
		const long long gold = (long long)ch->GetGold() - goldBefore;
		sys_log(0, "PLAYERBOT_GIFT: accepted pid=%u name=%s partner=%s offered=%u wanted=%d received=%d gold=%lld",
				ch->GetPlayerID(), ch->GetName(), partner->GetName(), (unsigned int)offer.size(), wanted,
				received, gold);
		if (received > 0 || gold > 0)
		{
			SendPlayerBotWhisper(ch, partner, "dzieki");
			// What it was given is judged like everything else in the bag:
			// worn when it is better, sold when it is not.
			state.dwNextEquipmentCheckTime = 0;
		}
		return true;
	}
}

#endif
