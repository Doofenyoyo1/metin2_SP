// MT2009_PLUS_SIDEKICK_SHOP_ERRAND_V1: the companion sent for goods (the owner,
// 3 October: "Opcja wyslania towarzysza po poty czerwone, niebieskie, zielone,
// fioletowe, strzaly, skupienie podstawowych itemow z rynku - peleryny").
//
// "Kup" on the window's orders page names one kind of goods and how many
// ("/towarzysz kup <towar> <ile>"). The server answers with what it would cost
// at most (SidekickShopQuote, the window asks "Tak/Nie"; typed, the chat says
// how to confirm), and "/towarzysz kup <towar> <ile> tak" sends it:
//
//   - the OWNER pays: that most is taken from the owner's yang at once and
//     put in the companion's purse for the errand (the log says so, as the
//     window's yang transfer does), and what it did not spend is given back
//     when it settles. The companion's own yang is never touched, and nothing
//     is ever bought below a merchant's price: at a merchant it pays exactly
//     what the merchant asks (the item's price, as the bots' every purchase
//     at a merchant does, GetPlayerBotNpcPurchasePrice), and at a stand what
//     the stand asks, through the engine's own purchase (the db core's lock
//     takes the yang) - never more a piece than PLAYERBOT_SIDEKICK_SHOP_CAP_PCT
//     of the bots' price list (GetPlayerBotMaterialAskingBase).
//   - red and blue potions (M, S, D) at the General Store saleswoman, Wooden
//     Arrows at the weapon dealer - in the town it stands in, or its kingdom's
//     first village; green and purple potions and the Cape of Courage off the
//     stands of a town of this core and channel, whichever has the most of it
//     at a sane price (no merchant sells them here).
//   - it walks there (the way back to its village is the bots' own map change,
//     the walk the town legs' - put at the counter if the walk stalls), buys
//     into free cells of its bag and holds what it bought for its owner
//     (AddPlayerBotSidekickHeld: the AI never drinks, sells, wears or moves
//     it, whatever the equipment lock says), and tells the owner how it goes.
//   - back beside its owner it puts what it bought straight into the owner's
//     bag as far as there is room; the rest waits in its own bag, marked as
//     the owner's, for the bag window's quick transfer.
//
// "Przywolaj", "Wolna reka" or the owner leaving end the errand early: what was
// bought so far is handed over or held, the rest of the yang given back.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, right after playerbot_sidekick.h.

namespace
{
	enum EPlayerBotSidekickShopSource
	{
		PLAYERBOT_SIDEKICK_SHOP_MISC = 0,	// the General Store saleswoman (9003)
		PLAYERBOT_SIDEKICK_SHOP_WEAPON = 1,	// the weapon dealer (9001)
		PLAYERBOT_SIDEKICK_SHOP_MARKET = 2,	// the stands
	};

	enum EPlayerBotSidekickShopStage
	{
		PLAYERBOT_SIDEKICK_SHOP_TO_MERCHANT = 1,
		PLAYERBOT_SIDEKICK_SHOP_PICK_LINE = 2,
		PLAYERBOT_SIDEKICK_SHOP_TO_STAND = 3,
		PLAYERBOT_SIDEKICK_SHOP_WAIT_DB = 4,
	};

	struct TPlayerBotSidekickShopGood
	{
		const char* key;
		DWORD vnum;
		DWORD altVnum;	// the same thing under another vnum, off the stands
		BYTE source;
		const char* name;
	};

	// The window's buttons send these keys (uisidekick.SHOP_GOODS).
	const TPlayerBotSidekickShopGood PLAYERBOT_SIDEKICK_SHOP_GOODS[] = {
		{ "czerwona1", 27001, 0, PLAYERBOT_SIDEKICK_SHOP_MISC, "Czerwona Mikstura (M)" },
		{ "czerwona2", 27002, 0, PLAYERBOT_SIDEKICK_SHOP_MISC, "Czerwona Mikstura (S)" },
		{ "czerwona3", 27003, 0, PLAYERBOT_SIDEKICK_SHOP_MISC, "Czerwona Mikstura (D)" },
		{ "niebieska1", 27004, 0, PLAYERBOT_SIDEKICK_SHOP_MISC, "Niebieska Mikstura (M)" },
		{ "niebieska2", 27005, 0, PLAYERBOT_SIDEKICK_SHOP_MISC, "Niebieska Mikstura (S)" },
		{ "niebieska3", 27006, 0, PLAYERBOT_SIDEKICK_SHOP_MISC, "Niebieska Mikstura (D)" },
		{ "zielona1", 27100, 0, PLAYERBOT_SIDEKICK_SHOP_MARKET, "Zielona Mikstura (M)" },
		{ "zielona2", 27101, 0, PLAYERBOT_SIDEKICK_SHOP_MARKET, "Zielona Mikstura (S)" },
		{ "zielona3", 27102, 0, PLAYERBOT_SIDEKICK_SHOP_MARKET, "Zielona Mikstura (D)" },
		{ "fioletowa1", 27103, 0, PLAYERBOT_SIDEKICK_SHOP_MARKET, "Fioletowa Mikstura (M)" },
		{ "fioletowa2", 27104, 0, PLAYERBOT_SIDEKICK_SHOP_MARKET, "Fioletowa Mikstura (S)" },
		{ "fioletowa3", 27105, 0, PLAYERBOT_SIDEKICK_SHOP_MARKET, "Fioletowa Mikstura (D)" },
		{ "strzaly", PLAYERBOT_WOODEN_ARROW_VNUM, 0, PLAYERBOT_SIDEKICK_SHOP_WEAPON, "Drewniana Strzala" },
		{ "peleryna", 70038, 76007, PLAYERBOT_SIDEKICK_SHOP_MARKET, "Peleryna Mestwa" },
	};
	const size_t PLAYERBOT_SIDEKICK_SHOP_GOOD_COUNT =
			sizeof(PLAYERBOT_SIDEKICK_SHOP_GOODS) / sizeof(PLAYERBOT_SIDEKICK_SHOP_GOODS[0]);

	// One order: at most this many pieces, which is five stacks of two hundred.
	const int PLAYERBOT_SIDEKICK_SHOP_MAX_COUNT = 1000;
	// The most a piece off a stand may cost, in percent of the bots' price list.
	const int PLAYERBOT_SIDEKICK_SHOP_CAP_PCT = 150;
	// A walk that has not arrived by then is finished by putting it there; the
	// whole errand, and the wait for the db core's answer to one purchase.
	const DWORD PLAYERBOT_SIDEKICK_SHOP_WALK_MS = 45000;
	const DWORD PLAYERBOT_SIDEKICK_SHOP_MAX_MS = 6 * 60 * 1000;
	const DWORD PLAYERBOT_SIDEKICK_SHOP_DB_WAIT_MS = 30000;
	const DWORD PLAYERBOT_SIDEKICK_SHOP_STAND_WAIT_MS = 15000;
	// Lines looked at in one scan of the stands.
	const unsigned int PLAYERBOT_SIDEKICK_SHOP_SCAN_MAX = 4000;

	const TPlayerBotSidekickShopGood* FindPlayerBotSidekickShopGood(const char* key)
	{
		for (size_t i = 0; key && i < PLAYERBOT_SIDEKICK_SHOP_GOOD_COUNT; ++i)
			if (!strcmp(PLAYERBOT_SIDEKICK_SHOP_GOODS[i].key, key))
				return &PLAYERBOT_SIDEKICK_SHOP_GOODS[i];
		return NULL;
	}

	bool IsPlayerBotSidekickShopGoodVnum(const TPlayerBotSidekickShopGood& good, DWORD vnum)
	{
		return vnum == good.vnum || (good.altVnum && vnum == good.altVnum);
	}

	// How many pieces one cell takes.
	int GetPlayerBotSidekickShopStack(const TItemTable* proto)
	{
		return proto && IS_SET(proto->dwFlags, ITEM_FLAG_STACKABLE) ? (int)ITEM_MAX_COUNT : 1;
	}

	// What a merchant asks for count pieces, a stack at a time as they are
	// bought; 0 when it has no price on this world.
	long long GetPlayerBotSidekickShopNpcCost(const TItemTable* proto, int count)
	{
		const int stack = GetPlayerBotSidekickShopStack(proto);
		long long total = 0;
		for (int left = count; left > 0; left -= stack)
		{
			const long long price = GetPlayerBotNpcPurchasePrice(proto, std::min(left, stack));
			if (price <= 0)
				return 0;
			total += price;
		}
		return total;
	}

	// The most one piece off a stand may cost; 0 for a thing the list has no
	// price for, which is then not bought off the stands at all.
	long long GetPlayerBotSidekickShopUnitCap(const TPlayerBotSidekickShopGood& good)
	{
		const long long base = (long long)GetPlayerBotMaterialAskingBase(good.vnum);
		return base > 0 ? base * PLAYERBOT_SIDEKICK_SHOP_CAP_PCT / 100 : 0;
	}

	const char* GetPlayerBotSidekickShopPlace(BYTE source)
	{
		switch (source)
		{
			case PLAYERBOT_SIDEKICK_SHOP_MISC: return "do Handlarki";
			case PLAYERBOT_SIDEKICK_SHOP_WEAPON: return "do Handlarza Bronia";
			default: return "na targ";
		}
	}

	// Where its purse has room, and where the owner's has, for an amount.
	bool PlayerBotSidekickGoldFits(LPCHARACTER ch, long long amount)
	{
		return ch && (long long)ch->GetGold() + amount <= (long long)GOLD_MAX;
	}

	// What it spent of the owner's yang so far: what its purse lost since the
	// yang was put in, never more than it was given.
	long long GetPlayerBotSidekickShopSpent(LPCHARACTER ch, const TPlayerBotSidekickShopErrand& shop)
	{
		const long long spent = shop.llGoldStart - (long long)ch->GetGold();
		return MINMAX(0LL, spent, shop.llEscrow);
	}

	bool DescribePlayerBotSidekickShopErrand(const TPlayerBotSidekickRuntime& rt, char* out, size_t size)
	{
		if (!rt.shop.bActive || rt.shop.bGood >= PLAYERBOT_SIDEKICK_SHOP_GOOD_COUNT)
			return false;
		snprintf(out, size, "kupuje: %s (%d/%d)", PLAYERBOT_SIDEKICK_SHOP_GOODS[rt.shop.bGood].name,
				rt.shop.iGot, rt.shop.iWanted);
		return true;
	}

#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
	// The stands' channel: the bots keep their stands on one channel when the
	// channel table runs, else on every channel they play on.
	bool IsPlayerBotSidekickShopMarketChannel()
	{
		return !CPlayerBotManager::instance().IsChannelTableMode() ||
				g_bChannel == playerbot_channel_rules::SHOP_CHANNEL;
	}

	// A stand line it would buy: the thing, at a sane price a piece, no more
	// pieces than it still needs, not its owner's own stand, not one somebody
	// else walks to, on this core and channel in a town.
	template<class Shop, class Line>
	bool IsPlayerBotSidekickShopLine(LPCHARACTER sk, DWORD ownerPid, const TPlayerBotSidekickShopGood& good,
			long long unitCap, int need, const Shop& shop, DWORD id, const Line& line, DWORD dwNow)
	{
		if (!line || !shop || shop->GetDuration() == 0 || shop->GetOwnerPID() == ownerPid ||
				shop->GetOwnerPID() == sk->GetPlayerID())
			return false;
		if (!IsPlayerBotSidekickShopGoodVnum(good, line->GetInfo().vnum))
			return false;
		const int count = (int)line->GetInfo().count;
		const long long price = (long long)line->GetPrice().GetTotalYangAmount();
		if (count <= 0 || count > need || price <= 0 || price > unitCap * count)
			return false;
		if (IsPlayerBotLineClaimedByOther(id, sk->GetPlayerID(), dwNow))
			return false;
		const auto spawn = shop->GetSpawn();
		playerbot_empire_rules::TTownServices svc;
		return (int)spawn.channel == (int)g_bChannel && IsPlayerBotMapHostedHere(spawn.map) &&
				playerbot_empire_rules::GetTownServices(spawn.map, svc);
	}

	// The town whose stands hold the most of it (up to need) at a sane price:
	// its map, and how many pieces are there. 0 for none.
	long FindPlayerBotSidekickShopMarketMap(LPCHARACTER sk, DWORD ownerPid, const TPlayerBotSidekickShopGood& good,
			long long unitCap, int need, int& outPieces, DWORD dwNow)
	{
		outPieces = 0;
		std::map<long, int> pieces;
		unsigned int looked = 0;
		for (const auto& [pid, shop] : ikashop::GetManager().GetPlayerBotOfflineShops())
		{
			if (!shop || looked > PLAYERBOT_SIDEKICK_SHOP_SCAN_MAX)
				break;
			for (const auto& [id, line] : shop->GetItems())
			{
				if (++looked > PLAYERBOT_SIDEKICK_SHOP_SCAN_MAX)
					break;
				if (IsPlayerBotSidekickShopLine(sk, ownerPid, good, unitCap, need, shop, id, line, dwNow))
					pieces[shop->GetSpawn().map] += (int)line->GetInfo().count;
			}
		}
		long best = 0;
		for (std::map<long, int>::const_iterator it = pieces.begin(); it != pieces.end(); ++it)
		{
			// The map it stands on first among equals: no walk at all.
			const int have = std::min(it->second, need);
			if (have > outPieces || (have == outPieces && it->first == sk->GetMapIndex()))
			{
				best = it->first;
				outPieces = have;
			}
		}
		return best;
	}

	// The cheapest line a piece on the map it stands on, that fits what it
	// still needs and what is left of the owner's yang.
	bool FindPlayerBotSidekickShopLine(LPCHARACTER sk, DWORD ownerPid, const TPlayerBotSidekickShopErrand& shopErrand,
			const TPlayerBotSidekickShopGood& good, long long unitCap, DWORD dwNow, DWORD& outOwner, DWORD& outItem,
			long& outX, long& outY)
	{
		outOwner = outItem = 0;
		const int need = shopErrand.iWanted - shopErrand.iGot;
		const long long left = shopErrand.llEscrow - GetPlayerBotSidekickShopSpent(sk, shopErrand);
		if (need <= 0 || left <= 0)
			return false;
		long long bestUnit = 0;
		unsigned int looked = 0;
		for (const auto& [pid, shop] : ikashop::GetManager().GetPlayerBotOfflineShops())
		{
			if (!shop || looked > PLAYERBOT_SIDEKICK_SHOP_SCAN_MAX)
				break;
			if (shop->GetSpawn().map != sk->GetMapIndex() || shop->IsEditMode())
				continue;
			for (const auto& [id, line] : shop->GetItems())
			{
				if (++looked > PLAYERBOT_SIDEKICK_SHOP_SCAN_MAX)
					break;
				if (shopErrand.setTried.count(id) ||
						!IsPlayerBotSidekickShopLine(sk, ownerPid, good, unitCap, need, shop, id, line, dwNow))
					continue;
				const long long price = (long long)line->GetPrice().GetTotalYangAmount();
				if (price > left || price > (long long)sk->GetGold())
					continue;
				const long long unit = price / std::max<long long>(1, (long long)line->GetInfo().count);
				if (outOwner && unit >= bestUnit)
					continue;
				outOwner = shop->GetOwnerPID();
				outItem = id;
				outX = shop->GetSpawn().x;
				outY = shop->GetSpawn().y;
				bestUnit = unit;
			}
		}
		return outOwner != 0;
	}
#endif

	// Where it goes for a thing a merchant sells: the merchant of the town it
	// stands in, or of its kingdom's first village. False when neither is a
	// map of this core.
	bool GetPlayerBotSidekickShopMerchant(LPCHARACTER sk, BYTE source, long& outMap, long& outX, long& outY)
	{
		long map = sk->GetMapIndex();
		playerbot_empire_rules::TTownServices svc;
		if (!playerbot_empire_rules::GetTownServices(map, svc))
		{
			long x = 0, y = 0;
			if (!GetPlayerBotVillageReturn(sk, playerbot_empire_rules::MAP_ROLE_M1, map, x, y) ||
					!playerbot_empire_rules::GetTownServices(map, svc))
				return false;
		}
		if (!IsPlayerBotMapHostedHere(map))
			return false;
		const playerbot_empire_rules::TPoint& npc = source == PLAYERBOT_SIDEKICK_SHOP_WEAPON ?
				svc.weaponMerchant : svc.miscMerchant;
		GetPlayerBotNpcApproach(sk->GetPlayerID(), npc.x, npc.y, 0x53484f50U, outX, outY);
		outMap = map;
		return true;
	}

	void SayPlayerBotSidekickShop(DWORD ownerPid, const char* text)
	{
		if (LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(ownerPid))
			SayPlayerBotSidekick(owner, text);
	}

	// ------------------------------------------------------------ the order

	void OrderPlayerBotSidekickShopErrand(LPCHARACTER owner, TPlayerBotSidekick& rec, const char* key,
			const char* countText, const char* confirm, DWORD dwNow)
	{
		const TPlayerBotSidekickShopGood* good = FindPlayerBotSidekickShopGood(key);
		int count = 0;
		if (countText && *countText)
			str_to_number(count, countText);
		if (!good || count <= 0)
		{
			SayPlayerBotSidekick(owner, "Uzyj: /towarzysz kup <towar> <ile> - towar: czerwona1-3, niebieska1-3, "
					"zielona1-3, fioletowa1-3 (1 M, 2 S, 3 D), strzaly, peleryna.");
			return;
		}
		count = std::min(count, PLAYERBOT_SIDEKICK_SHOP_MAX_COUNT);
		TPlayerBotAIState* state = NULL;
		LPCHARACTER sk = FindPlayerBotSidekickForOrder(owner, rec, &state);
		if (!sk)
			return;
		TPlayerBotSidekickRuntime& rt = s_mapPlayerBotSidekickRuntime[rec.dwSidekickPID];
		if (rt.bErrand)
		{
			SayPlayerBotSidekick(owner, "Juz jestem na zakupach. Zawolaj mnie, jesli mam wrocic wczesniej.");
			return;
		}
		if (rt.bFishing)
		{
			SayPlayerBotSidekick(owner, "Lowie ryby - zawolaj mnie najpierw (Przywolaj).");
			return;
		}
		if (rec.bMode == PLAYERBOT_SIDEKICK_FREE)
		{
			SayPlayerBotSidekick(owner, "Gram teraz po swojemu - zawolaj mnie najpierw (Przywolaj).");
			return;
		}
		if (rt.shop.dwPendingItem || rt.bTrading || !sk->IsItemLoaded() || !sk->CanHandleItem())
		{
			SayPlayerBotSidekick(owner, "Chwila - koncze poprzednia sprawe. Sprobuj za moment.");
			return;
		}
		TItemTable* proto = ITEM_MANAGER::instance().GetTable(good->vnum);
		if (!proto)
		{
			SayPlayerBotSidekick(owner, "Tego tu nikt nie sprzedaje.");
			return;
		}
		// Room in its bag: a cell a stack at a merchant, and a cell a line at a
		// stand, which holds at most a stack as well.
		const int stack = GetPlayerBotSidekickShopStack(proto);
		const int freeCells = CountPlayerBotFreeInventoryCells(sk);
		if (freeCells <= 0)
		{
			SayPlayerBotSidekick(owner, "Mam pelna torbe - zabierz cos ode mnie (okno Ekwipunek) i sprobuj znowu.");
			return;
		}
		const bool market = good->source == PLAYERBOT_SIDEKICK_SHOP_MARKET;
		if (!market)
			count = std::min(count, freeCells * stack);
		long map = 0, x = 0, y = 0;
		long long unitCap = 0;
		long long cost = 0;
		char text[256];
		if (market)
		{
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
			unitCap = GetPlayerBotSidekickShopUnitCap(*good);
			if (unitCap <= 0)
			{
				SayPlayerBotSidekick(owner, "Nie wiem, ile to jest warte - tego nie kupie.");
				return;
			}
			if (!IsPlayerBotSidekickShopMarketChannel())
			{
				snprintf(text, sizeof(text), "Stragany stoja na kanale %d - tu nic z targu nie kupie.",
						playerbot_channel_rules::SHOP_CHANNEL);
				SayPlayerBotSidekick(owner, text);
				return;
			}
			int pieces = 0;
			map = FindPlayerBotSidekickShopMarketMap(sk, owner->GetPlayerID(), *good, unitCap, count, pieces, dwNow);
			if (!map || pieces <= 0)
			{
				snprintf(text, sizeof(text), "Nikt teraz nie sprzedaje: %s w rozsadnej cenie (do %s yang za sztuke).",
						good->name, playerbot_conv::FormatYang(unitCap).c_str());
				SayPlayerBotSidekick(owner, text);
				return;
			}
			count = std::min(count, pieces);
			playerbot_empire_rules::TPoint pitch;
			if (playerbot_empire_rules::GetTownPitch(map, pitch))
			{
				x = pitch.x;
				y = pitch.y;
			}
			else
			{
				playerbot_empire_rules::TTownServices svc;
				playerbot_empire_rules::GetTownServices(map, svc);
				x = svc.miscMerchant.x;
				y = svc.miscMerchant.y;
			}
			// What the owner has pays for that many at the cap.
			count = (int)std::min<long long>(count, (long long)owner->GetGold() / unitCap);
			cost = unitCap * count;
#else
			SayPlayerBotSidekick(owner, "Na tym serwerze nie ma straganow, z ktorych moglbym kupic.");
			return;
#endif
		}
		else
		{
			if (!GetPlayerBotSidekickShopMerchant(sk, good->source, map, x, y))
			{
				SayPlayerBotSidekick(owner, "Stad nie dojde do zadnego handlarza.");
				return;
			}
			const long long all = GetPlayerBotSidekickShopNpcCost(proto, count);
			if (all <= 0)
			{
				SayPlayerBotSidekick(owner, "Tego tu nikt nie sprzedaje.");
				return;
			}
			while (count > 0 && GetPlayerBotSidekickShopNpcCost(proto, count) > (long long)owner->GetGold())
				count = count > stack ? count - stack : count - 1;
			cost = count > 0 ? GetPlayerBotSidekickShopNpcCost(proto, count) : 0;
		}
		if (count <= 0 || cost <= 0)
		{
			SayPlayerBotSidekick(owner, "Nie masz tyle yang.");
			return;
		}
		if (!PlayerBotSidekickGoldFits(sk, cost))
		{
			SayPlayerBotSidekick(owner, "Mam za duzo yang przy sobie, zeby wziac jeszcze tyle - wez cos ode mnie.");
			return;
		}
		// The price first: the window asks the owner, the chat says how.
		if (!confirm || strcmp(confirm, "tak"))
		{
			SendPlayerBotSidekickCommand(owner, "SidekickShopQuote %s %d %lld %d", good->key, count, cost, market ? 1 : 0);
			snprintf(text, sizeof(text), "%d x %s: zaplacisz z gory %s%s yang, reszta wroci do ciebie. "
					"Potwierdz: /towarzysz kup %s %d tak", count, good->name, market ? "najwyzej " : "",
					playerbot_conv::FormatYang(cost).c_str(), good->key, count);
			SayPlayerBotSidekick(owner, text);
			return;
		}

		// Sent: it leaves the owner's side the way "Na zakupy" does.
		if (sk->GetMapIndex() != map && !IsPlayerBotMapHostedHere(map))
		{
			SayPlayerBotSidekick(owner, "Stad tam nie dojde.");
			return;
		}
		rt.bHold = false;
		HandPlayerBotSidekickFoesToOwner(sk, owner, rt, dwNow, "shop_errand");
		rt.bLureStage = 0;
		rt.dwLureVID = 0;
		if (sk->GetParty())
			LeavePlayerBotParty(sk);
		if (sk->GetMapIndex() != map &&
				(!TransitionPlayerBotMap(sk, *state, map, x, y, dwNow, "sidekick_shop_errand") || sk->GetMapIndex() != map))
		{
			// A crossing on the way (the desert out of the spiders') is not
			// this errand's road: back beside the owner, nothing paid.
			if (sk->GetMapIndex() != owner->GetMapIndex())
				PlacePlayerBotSidekick(sk, *state, owner, dwNow, "sidekick_shop_refused");
			KeepPlayerBotSidekickInParty(sk, owner, dwNow);
			SayPlayerBotSidekick(owner, "Nie udalo mi sie tam dojsc.");
			return;
		}
		// The owner pays now; what is not spent comes back when it settles.
		PlayerBotChangeGold(owner, -cost);
		PlayerBotChangeGold(sk, cost);
		LogManager::instance().CharLog(owner, cost, "PLAYERBOT_SIDEKICK_SHOP_PAY", sk->GetName());
		TPlayerBotSidekickShopErrand& shop = rt.shop;
		shop = TPlayerBotSidekickShopErrand();
		shop.bActive = true;
		shop.bGood = (BYTE)(good - PLAYERBOT_SIDEKICK_SHOP_GOODS);
		shop.iWanted = count;
		shop.llEscrow = cost;
		shop.llGoldStart = (long long)sk->GetGold();
		shop.lMap = map;
		shop.lX = x;
		shop.lY = y;
		shop.dwSince = dwNow;
		shop.dwStageSince = dwNow;
		shop.bStage = market ? PLAYERBOT_SIDEKICK_SHOP_PICK_LINE : PLAYERBOT_SIDEKICK_SHOP_TO_MERCHANT;
		// Market goods: to the stands' square first, then from stand to stand.
		if (market && DISTANCE_APPROX(sk->GetX() - x, sk->GetY() - y) > 1500)
			shop.bStage = PLAYERBOT_SIDEKICK_SHOP_TO_STAND;
		rt.bErrand = true;
		rt.bErrandVisit = true;
		rt.dwErrandSince = dwNow;
		rt.llErrandGold = (long long)sk->GetGold();
		state->bVisitingShop = false;
		state->bTownVisitPhase = BOT_TOWN_PHASE_NONE;
		state->bMarketTrip = false;
		state->dwTargetVID = 0;
		sk->SetVictim(NULL);
		ClearPlayerBotRoute(*state, true);
		++s_uPlayerBotSidekickErrands;
		snprintf(text, sizeof(text), "Ide %s %s po %d x %s. Wziales mi %s yang - reszte oddam.",
				GetPlayerBotSidekickShopPlace(good->source), playerbot_conv::GetMapWords(map).at, count, good->name,
				playerbot_conv::FormatYang(cost).c_str());
		SayPlayerBotSidekick(owner, text);
		sys_log(0, "PLAYERBOT_SIDEKICK: shop errand pid=%u owner=%u good=%s vnum=%u count=%d escrow=%lld map=%ld "
				"x=%ld y=%ld market=%d", rec.dwSidekickPID, rec.dwOwnerPID, good->key, good->vnum, count, cost, map, x, y,
				market ? 1 : 0);
	}

	// ------------------------------------------------------------ the errand

	// At the merchant: as much as was ordered, a stack at a time into a free
	// cell, each paid for at the merchant's price out of the owner's yang.
	void BuyPlayerBotSidekickShopAtMerchant(LPCHARACTER sk, TPlayerBotSidekick& rec, TPlayerBotSidekickRuntime& rt)
	{
		TPlayerBotSidekickShopErrand& shop = rt.shop;
		const TPlayerBotSidekickShopGood& good = PLAYERBOT_SIDEKICK_SHOP_GOODS[shop.bGood];
		TItemTable* proto = ITEM_MANAGER::instance().GetTable(good.vnum);
		const int stack = GetPlayerBotSidekickShopStack(proto);
		shop.szWhy = "done";
		while (proto && shop.iGot < shop.iWanted)
		{
			const int n = std::min(stack, shop.iWanted - shop.iGot);
			const long long price = GetPlayerBotNpcPurchasePrice(proto, n);
			const long long left = shop.llEscrow - GetPlayerBotSidekickShopSpent(sk, shop);
			if (price <= 0 || price > left || price > (long long)sk->GetGold())
			{
				shop.szWhy = "no_yang";
				break;
			}
			const int cell = sk->GetEmptyInventory(proto->bSize);
			if (cell < 0)
			{
				shop.szWhy = "no_room";
				break;
			}
			LPITEM item = ITEM_MANAGER::instance().CreateItem(good.vnum, n);
			if (!item)
			{
				shop.szWhy = "no_item";
				break;
			}
			item->AddToCharacter(sk, TItemPos(INVENTORY, cell));
			ITEM_MANAGER::instance().FlushDelayedSave(item);
			PlayerBotChangeGold(sk, -price);
			AddPlayerBotSidekickHeld(sk->GetPlayerID(), rt, item->GetID());
			shop.vecItems.push_back(item->GetID());
			shop.iGot += n;
			LogManager::instance().ItemLog(sk, item, "PLAYERBOT_SIDEKICK_SHOP_BUY", "npc");
			sys_log(0, "PLAYERBOT_SIDEKICK: shop bought at the merchant pid=%u owner=%u vnum=%u count=%d price=%lld "
					"item=%u", sk->GetPlayerID(), rec.dwOwnerPID, good.vnum, n, price, item->GetID());
		}
	}

	// Walking to (x, y): true there. A walk that stalls is finished by putting
	// it there, as the companion is put beside its owner.
	bool WalkPlayerBotSidekickShop(LPCHARACTER sk, TPlayerBotAIState& state, TPlayerBotSidekickShopErrand& shop,
			long x, long y, int arrival, DWORD dwNow)
	{
		SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
		if (MovePlayerBotTownLeg(sk, state, dwNow, x, y, arrival))
			return true;
		if (dwNow - shop.dwStageSince < PLAYERBOT_SIDEKICK_SHOP_WALK_MS)
			return false;
		shop.dwStageSince = dwNow;
		return PlacePlayerBotSidekickAt(sk, state, sk->GetMapIndex(), x, y, dwNow, "sidekick_shop_walk") &&
				MovePlayerBotTownLeg(sk, state, dwNow, x, y, arrival);
	}

	// A stand's answer: the piece in its bag, held for the owner. True when it
	// came (bought or refused).
	bool TakePlayerBotSidekickShopAnswer(LPCHARACTER sk, TPlayerBotSidekick& rec, TPlayerBotSidekickRuntime& rt,
			DWORD dwNow, bool late)
	{
		TPlayerBotSidekickShopErrand& shop = rt.shop;
		if (!shop.dwPendingItem)
			return true;
		bool done = true;
		bool success = false;
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		std::map<uint32_t, playerbot_offline::Request>::iterator req = playerbot_offline::requests.find(sk->GetPlayerID());
		if (req != playerbot_offline::requests.end() && req->second.op == playerbot_offline::Buy &&
				req->second.item == shop.dwPendingItem)
		{
			if (!req->second.done)
			{
				if (dwNow - shop.dwPendingSince < (late ? 10U * PLAYERBOT_SIDEKICK_SHOP_DB_WAIT_MS
						: PLAYERBOT_SIDEKICK_SHOP_DB_WAIT_MS))
					return false;
				// Never answered: given up, the journal's line with it, so the
				// next purchase is not refused for good (playerbot_offline_policy.h).
				sys_err("PLAYERBOT_SIDEKICK: stand purchase unanswered pid=%u item=%u", sk->GetPlayerID(),
						shop.dwPendingItem);
				playerbot_offline::requests.erase(req);
				done = false;
			}
			else
			{
				success = req->second.success;
				playerbot_offline::requests.erase(req);
			}
		}
#endif
		const TPlayerBotSidekickShopGood& good = PLAYERBOT_SIDEKICK_SHOP_GOODS[shop.bGood];
		LPITEM item = ITEM_MANAGER::instance().Find(shop.dwPendingItem);
		// The piece is in its bag whatever the answer said: that is what counts.
		if (item && item->GetOwner() == sk && item->GetWindow() == INVENTORY)
		{
			success = true;
			AddPlayerBotSidekickHeld(sk->GetPlayerID(), rt, item->GetID());
			shop.vecItems.push_back(item->GetID());
			shop.iGot += (int)item->GetCount();
		}
		else
			success = false;
		char text[192];
		if (success)
			snprintf(text, sizeof(text), "Kupilem %d x %s za %s yang (%d/%d).", shop.iPendingCount, good.name,
					playerbot_conv::FormatYang(shop.llPendingPrice).c_str(), std::min(shop.iGot, shop.iWanted),
					shop.iWanted);
		else
			snprintf(text, sizeof(text), "Ktos byl szybszy - ten towar juz zszedl.");
		if (!late)
			SayPlayerBotSidekickShop(rec.dwOwnerPID, text);
		else if (success)
		{
			snprintf(text, sizeof(text), "Dotarl jeszcze zakup z targu: %d x %s - czeka w mojej torbie.",
					shop.iPendingCount, good.name);
			SayPlayerBotSidekickShop(rec.dwOwnerPID, text);
		}
		sys_log(0, "PLAYERBOT_SIDEKICK: shop stand answer pid=%u owner=%u item=%u ok=%d answered=%d late=%d price=%lld",
				sk->GetPlayerID(), rec.dwOwnerPID, shop.dwPendingItem, success ? 1 : 0, done ? 1 : 0, late ? 1 : 0,
				shop.llPendingPrice);
		// Settled already: the yang held back for this purchase goes to the
		// owner when it was not spent.
		if (late)
		{
			const bool paid = (long long)sk->GetGold() <= shop.llGoldBeforeBuy - shop.llPendingPrice;
			LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec.dwOwnerPID);
			if (!success && !paid && owner && shop.llPendingPrice > 0 &&
					(long long)sk->GetGold() >= shop.llPendingPrice && PlayerBotSidekickGoldFits(owner, shop.llPendingPrice))
			{
				PlayerBotChangeGold(sk, -shop.llPendingPrice);
				PlayerBotChangeGold(owner, shop.llPendingPrice);
				LogManager::instance().CharLog(owner, shop.llPendingPrice, "PLAYERBOT_SIDEKICK_SHOP_REFUND", sk->GetName());
			}
		}
		shop.setTried.insert(shop.dwPendingItem);
		shop.dwPendingItem = 0;
		shop.iPendingCount = 0;
		shop.llPendingPrice = 0;
		return true;
	}

#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
	// At the stand: the line bought through the engine's own purchase, as the
	// bots buy (RunPlayerBotOfflinePick). True when the request went out.
	bool BuyPlayerBotSidekickShopLine(LPCHARACTER sk, TPlayerBotSidekick& rec, TPlayerBotSidekickRuntime& rt,
			DWORD dwNow, bool& wait)
	{
		using namespace playerbot_offline;
		wait = false;
		TPlayerBotSidekickShopErrand& shop = rt.shop;
		const TPlayerBotSidekickShopGood& good = PLAYERBOT_SIDEKICK_SHOP_GOODS[shop.bGood];
		auto& manager = ikashop::GetManager();
		auto stand = manager.GetShopByOwnerID(shop.dwShopOwner);
		if (!stand || stand->GetDuration() == 0)
			return false;
		auto line = stand->GetItem(shop.dwShopItem);
		if (!line)
			return false;
		// The keeper is at its stand for a few seconds of edit mode, or has
		// just edited it (the engine refuses a purchase for ten seconds).
		if (stand->IsEditMode() || stand->GetLastEditTime() + 10 > (DWORD)get_global_time())
		{
			wait = dwNow - shop.dwStageSince < PLAYERBOT_SIDEKICK_SHOP_STAND_WAIT_MS;
			return false;
		}
		const long long unitCap = GetPlayerBotSidekickShopUnitCap(good);
		if (!IsPlayerBotSidekickShopLine(sk, rec.dwOwnerPID, good, unitCap, shop.iWanted - shop.iGot, stand,
				shop.dwShopItem, line, dwNow))
			return false;
		const long long price = (long long)line->GetPrice().GetTotalYangAmount();
		const long long left = shop.llEscrow - GetPlayerBotSidekickShopSpent(sk, shop);
		if (price > left || price > (long long)sk->GetGold() || sk->GetEmptyInventory(line->GetTable() ?
				line->GetTable()->bSize : 1) < 0 || !sk->CanHandleItem())
			return false;
		if (!BotOfflineBudget(dwNow))
		{
			wait = true;
			return false;
		}
		if (!Begin(sk->GetPlayerID(), Buy, shop.dwShopItem, dwNow))
		{
			wait = dwNow - shop.dwStageSince < PLAYERBOT_SIDEKICK_SHOP_STAND_WAIT_MS;
			return false;
		}
		Request& request = requests.at(sk->GetPlayerID());
		request.vnum = line->GetInfo().vnum;
		request.count = line->GetInfo().count;
		request.unitPrice = uint32_t(price / std::max<uint32_t>(1, request.count));
		shop.iPendingCount = (int)line->GetInfo().count;
		shop.llPendingPrice = price;
		shop.llGoldBeforeBuy = (long long)sk->GetGold();
		manager.RecvShopOpenClientPacket(sk, shop.dwShopOwner);
		manager.RecvShopBuyItemClientPacket(sk, shop.dwShopOwner, shop.dwShopItem, false, price);
		const bool sent = EndCall(sk->GetPlayerID());
		manager.RecvCloseShopGuestClientPacket(sk);
		sys_log(0, "PLAYERBOT_SIDEKICK: shop stand purchase pid=%u owner=%u stand=%u item=%u vnum=%u count=%d "
				"price=%lld sent=%d", sk->GetPlayerID(), rec.dwOwnerPID, shop.dwShopOwner, shop.dwShopItem,
				(unsigned int)line->GetInfo().vnum, shop.iPendingCount, price, sent ? 1 : 0);
		if (!sent)
		{
			shop.iPendingCount = 0;
			shop.llPendingPrice = 0;
			return false;
		}
		shop.dwPendingItem = shop.dwShopItem;
		shop.dwPendingSince = dwNow;
		return true;
	}
#endif

	bool ManagePlayerBotSidekickShopErrand(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt, DWORD dwNow)
	{
		TPlayerBotSidekickShopErrand& shop = rt.shop;
		// Lying down: the bots' own way back up runs, the errand waits.
		if (ch->IsDead())
			return false;
		const DWORD elapsed = dwNow >= shop.dwSince ? dwNow - shop.dwSince : 0;
		if (elapsed >= PLAYERBOT_SIDEKICK_SHOP_MAX_MS)
		{
			SettlePlayerBotSidekickShopErrand(ch, state, rec, rt, dwNow, "timeout", true);
			return true;
		}
		// Somewhere else than where it went (a death's way back to town).
		if (ch->GetMapIndex() != shop.lMap)
		{
			SettlePlayerBotSidekickShopErrand(ch, state, rec, rt, dwNow, "elsewhere", true);
			return true;
		}
		ch->SetVictim(NULL);
		state.dwTargetVID = 0;
		state.dwLastMeaningfulActivityTime = dwNow;
		switch (shop.bStage)
		{
			case PLAYERBOT_SIDEKICK_SHOP_TO_MERCHANT:
				if (!WalkPlayerBotSidekickShop(ch, state, shop, shop.lX, shop.lY, 300, dwNow))
					return true;
				SetPlayerBotAction(state, BOT_ACTION_SHOP, dwNow);
				BuyPlayerBotSidekickShopAtMerchant(ch, rec, rt);
				SettlePlayerBotSidekickShopErrand(ch, state, rec, rt, dwNow, shop.szWhy, true);
				return true;
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
			case PLAYERBOT_SIDEKICK_SHOP_TO_STAND:
			{
				// To the square, or to the stand picked.
				const bool square = shop.dwShopOwner == 0;
				if (!WalkPlayerBotSidekickShop(ch, state, shop, shop.lX, shop.lY, square ? 800 : 300, dwNow))
					return true;
				if (square)
				{
					shop.bStage = PLAYERBOT_SIDEKICK_SHOP_PICK_LINE;
					return true;
				}
				SetPlayerBotAction(state, BOT_ACTION_SHOP, dwNow);
				bool wait = false;
				if (BuyPlayerBotSidekickShopLine(ch, rec, rt, dwNow, wait))
				{
					shop.bStage = PLAYERBOT_SIDEKICK_SHOP_WAIT_DB;
					shop.dwStageSince = dwNow;
				}
				else if (!wait)
				{
					ReleasePlayerBotFarLine(shop.dwShopItem, ch->GetPlayerID());
					shop.setTried.insert(shop.dwShopItem);
					shop.dwShopOwner = 0;
					shop.bStage = PLAYERBOT_SIDEKICK_SHOP_PICK_LINE;
				}
				return true;
			}
			case PLAYERBOT_SIDEKICK_SHOP_WAIT_DB:
				ch->Stop();
				if (!TakePlayerBotSidekickShopAnswer(ch, rec, rt, dwNow, false))
					return true;
				ReleasePlayerBotFarLine(shop.dwShopItem, ch->GetPlayerID());
				shop.dwShopOwner = 0;
				if (shop.iGot >= shop.iWanted)
				{
					SettlePlayerBotSidekickShopErrand(ch, state, rec, rt, dwNow, "done", true);
					return true;
				}
				shop.bStage = PLAYERBOT_SIDEKICK_SHOP_PICK_LINE;
				return true;
			case PLAYERBOT_SIDEKICK_SHOP_PICK_LINE:
			{
				if (CountPlayerBotFreeInventoryCells(ch) <= 0)
				{
					SettlePlayerBotSidekickShopErrand(ch, state, rec, rt, dwNow, "no_room", true);
					return true;
				}
				const TPlayerBotSidekickShopGood& good = PLAYERBOT_SIDEKICK_SHOP_GOODS[shop.bGood];
				DWORD standOwner = 0, item = 0;
				long x = 0, y = 0;
				if (!FindPlayerBotSidekickShopLine(ch, rec.dwOwnerPID, shop, good, GetPlayerBotSidekickShopUnitCap(good),
						dwNow, standOwner, item, x, y))
				{
					SettlePlayerBotSidekickShopErrand(ch, state, rec, rt, dwNow,
							shop.iGot > 0 ? "market_short" : "market_empty", true);
					return true;
				}
				shop.dwShopOwner = standOwner;
				shop.dwShopItem = item;
				shop.lX = x;
				shop.lY = y;
				shop.bStage = PLAYERBOT_SIDEKICK_SHOP_TO_STAND;
				shop.dwStageSince = dwNow;
				ClaimPlayerBotLineUntil(item, ch->GetPlayerID(), dwNow, dwNow + PLAYERBOT_SIDEKICK_SHOP_WALK_MS * 2);
				return true;
			}
#endif
			default:
				SettlePlayerBotSidekickShopErrand(ch, state, rec, rt, dwNow, "stage", true);
				return true;
		}
	}

	// Back with the owner: the goods into the owner's bag as far as there is
	// room, the owner's yang it did not spend given back, and a word on it.
	void SettlePlayerBotSidekickShopErrand(LPCHARACTER ch, TPlayerBotAIState& state, TPlayerBotSidekick& rec,
			TPlayerBotSidekickRuntime& rt, DWORD dwNow, const char* why, bool comeBack)
	{
		TPlayerBotSidekickShopErrand& shop = rt.shop;
		if (!shop.bActive)
			return;
		shop.bActive = false;
		rt.bErrand = false;
		rt.bErrandVisit = false;
		state.bVisitingShop = false;
		state.bTownVisitPhase = BOT_TOWN_PHASE_NONE;
		state.bMarketTrip = false;
		ClearPlayerBotRoute(state, true);
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		if (shop.dwShopItem)
			ReleasePlayerBotFarLine(shop.dwShopItem, ch->GetPlayerID());
#endif
		const TPlayerBotSidekickShopGood& good = PLAYERBOT_SIDEKICK_SHOP_GOODS[shop.bGood];
		LPCHARACTER owner = GetPlayerBotSidekickOwnerChar(rec.dwOwnerPID);
		if (owner && comeBack && owner->GetSectree() && !ch->IsDead() && rec.bMode == PLAYERBOT_SIDEKICK_FOLLOW)
		{
			PlacePlayerBotSidekick(ch, state, owner, dwNow, "sidekick_shop_back");
			KeepPlayerBotSidekickInParty(ch, owner, dwNow);
		}
		// The goods: straight into the owner's bag when it stands by.
		int handed = 0, kept = 0;
		const bool near = owner && owner->GetMapIndex() == ch->GetMapIndex() &&
				DISTANCE_APPROX(owner->GetX() - ch->GetX(), owner->GetY() - ch->GetY()) <= PLAYERBOT_SIDEKICK_TELEPORT_DISTANCE &&
				owner->CanHandleItem() && ch->CanHandleItem();
		for (size_t i = 0; i < shop.vecItems.size(); ++i)
		{
			LPITEM item = ITEM_MANAGER::instance().Find(shop.vecItems[i]);
			if (!item || item->GetOwner() != ch || item->GetWindow() != INVENTORY)
				continue;
			std::string answer;
			if (near && TakePlayerBotSidekickItem(owner, ch, rt, item->GetCell(), -1, answer) == 0)
				++handed;
			else
				++kept;
		}
		// The yang: what it was given and did not spend. A purchase from a stand
		// still in the db core's hands keeps its price back until it is answered.
		const long long spent = GetPlayerBotSidekickShopSpent(ch, shop);
		long long refund = shop.llEscrow - spent;
		if (shop.dwPendingItem && (long long)ch->GetGold() > shop.llGoldBeforeBuy - shop.llPendingPrice)
			refund -= shop.llPendingPrice;
		refund = std::min<long long>(std::max<long long>(refund, 0), (long long)ch->GetGold());
		bool refunded = false;
		if (refund > 0 && owner && PlayerBotSidekickGoldFits(owner, refund))
		{
			PlayerBotChangeGold(ch, -refund);
			PlayerBotChangeGold(owner, refund);
			LogManager::instance().CharLog(owner, refund, "PLAYERBOT_SIDEKICK_SHOP_REFUND", ch->GetName());
			refunded = true;
			// The late answer reads whether the stand was paid against this.
			shop.llGoldBeforeBuy -= refund;
		}
		char text[320];
		const char* reason = "";
		if (!strcmp(why, "market_empty") || !strcmp(why, "market_short"))
			reason = " Na targu nie bylo wiecej w rozsadnej cenie.";
		else if (!strcmp(why, "no_room"))
			reason = " Zabraklo mi miejsca w torbie.";
		else if (!strcmp(why, "no_yang"))
			reason = " Zabraklo yang.";
		else if (!strcmp(why, "called"))
			reason = " Zawolales mnie wczesniej.";
		else if (!strcmp(why, "timeout") || !strcmp(why, "elsewhere"))
			reason = " Nie udalo mi sie dokonczyc.";
		int n = snprintf(text, sizeof(text), "Wrocilem: kupilem %d/%d x %s.%s", shop.iGot, shop.iWanted, good.name,
				reason);
		if (n > 0 && n < (int)sizeof(text) && handed > 0)
			n += snprintf(text + n, sizeof(text) - n, " Wlozylem ci do torby.");
		if (n > 0 && n < (int)sizeof(text) && kept > 0)
			n += snprintf(text + n, sizeof(text) - n, " %s czeka w mojej torbie (okno Ekwipunek).",
					handed > 0 ? "Reszta" : "Wszystko");
		if (n > 0 && n < (int)sizeof(text) && refund > 0)
			snprintf(text + n, sizeof(text) - n, refunded ? " Oddaje ci %s yang." : " Mam przy sobie twoje %s yang.",
					playerbot_conv::FormatYang(refund).c_str());
		if (owner)
			SayPlayerBotSidekick(owner, text);
		sys_log(0, "PLAYERBOT_SIDEKICK: shop errand over pid=%u owner=%u why=%s good=%s got=%d wanted=%d escrow=%lld "
				"spent=%lld refund=%lld refunded=%d handed=%d kept=%d pending=%u", ch->GetPlayerID(), rec.dwOwnerPID, why,
				good.key, shop.iGot, shop.iWanted, shop.llEscrow, spent, refund, refunded ? 1 : 0, handed, kept,
				shop.dwPendingItem);
		shop.vecItems.clear();
	}

	// After the errand: a stand's answer that came late; and a journal line
	// of a purchase whose errand a logout forgot, which would refuse the next.
	void PollPlayerBotSidekickShopPurchase(LPCHARACTER ch, TPlayerBotSidekick& rec, TPlayerBotSidekickRuntime& rt,
			DWORD dwNow)
	{
		if (rt.shop.bActive)
			return;
		if (rt.shop.dwPendingItem)
		{
			TakePlayerBotSidekickShopAnswer(ch, rec, rt, dwNow, true);
			return;
		}
		// Off the leash it may shop for itself like any bot, whose own pass
		// reads its journal line.
		if (rec.bMode != PLAYERBOT_SIDEKICK_FOLLOW)
			return;
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		std::map<uint32_t, playerbot_offline::Request>::iterator req = playerbot_offline::requests.find(ch->GetPlayerID());
		if (req != playerbot_offline::requests.end() && req->second.op == playerbot_offline::Buy &&
				(req->second.done || dwNow - req->second.started >= 10U * PLAYERBOT_SIDEKICK_SHOP_DB_WAIT_MS))
			playerbot_offline::requests.erase(req);
#endif
	}
}
