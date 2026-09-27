#ifndef PLAYERBOT_OFFLINE_MARKET_H
#define PLAYERBOT_OFFLINE_MARKET_H
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
// Included after playerbot_market.h so the existing demand/gear rules are used.
namespace {
    // The line a walk over from a second village is for. Every bot of a
    // village read the same first village's lines and set off for the best
    // of them, and of the first 155 walks on the test world 31 found their
    // line sold on arrival - 29 of them to another bot that had walked over
    // for the same line (23 September). A walk claims its line for as long
    // as the walk and the ride to the stand may take, and every other look,
    // the browse of the stands in reach included, passes over it.
    struct TPlayerBotFarClaim { DWORD pid; DWORD until; };
    std::unordered_map<DWORD, TPlayerBotFarClaim> s_mapPlayerBotFarClaims;

    bool IsPlayerBotLineClaimedByOther(DWORD itemId, DWORD pid, DWORD now) {
        auto it = s_mapPlayerBotFarClaims.find(itemId);
        if (it == s_mapPlayerBotFarClaims.end()) return false;
        if (playerbot_offline::Due(now, it->second.until)) {
            s_mapPlayerBotFarClaims.erase(it);
            return false;
        }
        return it->second.pid != pid;
    }
    void ClaimPlayerBotLineUntil(DWORD itemId, DWORD pid, DWORD now, DWORD until) {
        if (!itemId) return;
        // A claim whose walk ended some other way than at the stand is left
        // to run out, and the ones that did are swept once the map is big.
        if (s_mapPlayerBotFarClaims.size() >= 4096)
            for (auto it = s_mapPlayerBotFarClaims.begin(); it != s_mapPlayerBotFarClaims.end(); )
                it = playerbot_offline::Due(now, it->second.until) ? s_mapPlayerBotFarClaims.erase(it) : std::next(it);
        s_mapPlayerBotFarClaims[itemId] = TPlayerBotFarClaim{ pid, until };
    }
    void ClaimPlayerBotFarLine(DWORD itemId, DWORD pid, DWORD now) {
        ClaimPlayerBotLineUntil(itemId, pid, now,
            now + PLAYERBOT_MARKET_JOAN_WALK_TIMEOUT + PLAYERBOT_MARKET_FAR_PICK_WALK_MS);
    }
    void ReleasePlayerBotFarLine(DWORD itemId, DWORD pid) {
        auto it = s_mapPlayerBotFarClaims.find(itemId);
        if (it != s_mapPlayerBotFarClaims.end() && it->second.pid == pid)
            s_mapPlayerBotFarClaims.erase(it);
    }

    // The buyer gives its pick up. A pick a walk over was made for says why,
    // once: the lines left after the claim are the ones whose reason is not
    // yet known, and "every way this function can decline looks identical
    // from outside" is how a walk over came to end nine times in ten with
    // nothing bought.
    void DropPlayerBotOfflinePick(LPCHARACTER ch, TPlayerBotAIState& state, const char* reason) {
        auto& o = state.offlineShop;
        if (o.buyOwner) {
            ReleasePlayerBotFarLine(o.buyItem, ch->GetPlayerID());
            if (o.farBuy)
                sys_log(0, "PLAYERBOT_MARKET: far pick lost pid=%u name=%s owner=%u item=%u reason=%s",
                    ch->GetPlayerID(), ch->GetName(), o.buyOwner, o.buyItem, reason);
        }
        o.farBuy = false;
        o.buyOwner = 0;
    }

    // One line of one stand as this bot would rate it: -1 for a line it would
    // not buy, else the priority a browse ranks by. The browse of the stands
    // in reach and the look at the first village's from a second village
    // (FindPlayerBotFarOfflinePick) ask this one function, so a walk over is
    // made for exactly what the buyer would take on arrival.
    template<class Shop, class Line>
    int RatePlayerBotOfflineLine(LPCHARACTER ch, const Shop& shop, const Line& line,
            long long budget, bool personFirst, long long& outPrice) {
        if (!line) return -1;
        const long long price = (long long)line->GetPrice().GetTotalYangAmount();
        if (price <= 0 || price > budget) return -1;
        auto preview = BotOfflinePreview(*line);
        if (!preview) return -1;
        const bool want = WantsPlayerBotStallItem(ch, preview) &&
            CanPlayerBotPayForOffer(ch, preview, price) && ch->GetEmptyInventory(preview->GetSize()) >= 0;
        int priority = IsPlayerBotProgressionOffer(ch, preview) ? 200 : 0;
        // The class's level-30 weapon comes first, and of those the
        // highest average line, the price only breaking a tie: "12% za
        // 300k albo 26% za 450k - wybierze drozsza" (community patch 2).
        // A finished piece the market Perfectionist's anvil waits for.
        if (want && priority < 300 && IsPlayerBotReadyGearOffer(ch, preview))
            priority = 300;
        if (want && IsPlayerBotClassLevel30Weapon(ch, preview))
            priority = 400 + (int)std::min<long>(99, std::max<long>(0,
                SumPlayerBotItemLines(preview, APPLY_NORMAL_HIT_DAMAGE_BONUS)));
        if (want && personFirst &&
                !CPlayerBotManager::instance().IsRegisteredBotPID(shop->GetOwnerPID())) {
            const long long fair = GetPlayerBotShopAskingPrice(preview);
            if (fair > 0 && price <= fair * PLAYERBOT_MARKET_PERSON_PRICE_PERCENT / 100)
                priority += 100;
        }
        M2_DELETE(preview);
        outPrice = price;
        return want ? priority : -1;
    }

    // The buyer's pick, taken to its stand and bought: the walk, the keeper's
    // edit mode waited out, the line asked again on arrival, and the request
    // to the db core, which delivers. True while it has the tick; false once
    // the pick is spent - asked for, or lost and said why. The browse below
    // and a gambler's walk along the counters (ManagePlayerBotGambleMarket)
    // both hand it their line.
    bool RunPlayerBotOfflinePick(LPCHARACTER ch, TPlayerBotAIState& state, DWORD now) {
        using namespace playerbot_offline;
        auto& o = state.offlineShop;
        auto& manager = ikashop::GetManager();
        if (!o.buyOwner) return false;
        if (g_bChannel != playerbot_channel_rules::SHOP_CHANNEL) {
            // Something worth buying, on the shop channel: ask to be moved, at
            // most this often, and forget the pick - the purchase is made there,
            // by the browse after the move.
            DropPlayerBotOfflinePick(ch, state, "other_channel");
            if (Due(now, state.dwNextBuyChannelRequestTime)) {
                state.dwNextBuyChannelRequestTime = now + PLAYERBOT_SHOP_CHANNEL_BUY_REQUEST_GAP_MS;
                if (CPlayerBotManager::instance().RequestShopChannel(ch->GetPlayerID()))
                    PlayerBotLogThrottled("shop_channel_buy", now,
                            "PLAYERBOT_CHANNEL: pid=%u name=%s asks for the shop channel to buy (here %u)",
                            ch->GetPlayerID(), ch->GetName(), (unsigned int)g_bChannel);
            }
            return false;
        }
        auto shop = manager.GetShopByOwnerID(o.buyOwner);
        const char* lost = !shop ? "shop_gone" : shop->GetDuration() == 0 ? "shop_expired" :
            Due(now, o.buyUntil) ? "timeout" :
            (shop->GetSpawn().map != ch->GetMapIndex() || shop->GetSpawn().channel != g_bChannel) ? "elsewhere" : NULL;
        if (lost) {
            DropPlayerBotOfflinePick(ch, state, lost);
            ClearPlayerBotRoute(state, true);
            return false;
        }
        auto line = shop->GetItem(o.buyItem);
        if (!line) { DropPlayerBotOfflinePick(ch, state, "sold"); return false; }
        SetPlayerBotAction(state, BOT_ACTION_TRAVEL, now);
        if (!MovePlayerBotTownLeg(ch, state, now, shop->GetSpawn().x, shop->GetSpawn().y, 600)) return true;
        // The keeper is serving its stand, which is a few seconds of edit
        // mode: the buyer waits them out at the counter instead of giving up
        // the line it came for - which it did, whatever the walk had cost.
        if (shop->IsEditMode()) return true;
        auto price = line->GetPrice().GetTotalYangAmount();
        auto finalPreview = BotOfflinePreview(*line);
        const bool wanted = finalPreview && WantsPlayerBotStallItem(ch, finalPreview);
        const bool payable = wanted && CanPlayerBotPayForOffer(ch, finalPreview, price);
        const bool stillWanted = payable && ch->GetEmptyInventory(finalPreview->GetSize()) >= 0;
        if (finalPreview) M2_DELETE(finalPreview);
        if (!stillWanted) {
            DropPlayerBotOfflinePick(ch, state, !wanted ? "no_longer_wanted" : !payable ? "cannot_pay" : "no_room");
            ClearPlayerBotRoute(state, true);
            return false;
        }
        if (!BotOfflineBudget(now)) return true;
        // Read before the request: the log line below must not touch the shop
        // line once the purchase is in the engine's hands.
        const DWORD boughtVnum = line->GetInfo().vnum;
        if (boughtVnum == PLAYERBOT_MOONLIGHT_CHEST_VNUM)
            NotePlayerBotChestBought(ch->GetPlayerID(), now);
        if (Begin(ch->GetPlayerID(), Buy, o.buyItem, now)) {
            auto& request = requests.at(ch->GetPlayerID());
            request.vnum = line->GetInfo().vnum;
            request.count = line->GetInfo().count;
            request.unitPrice = uint32_t(price / std::max<uint32_t>(1, request.count));
            if (auto preview = BotOfflinePreview(*line)) {
                request.refine = preview->GetRefineLevel();
                request.level30 = IsPlayerBotClassLevel30Weapon(ch, preview);
                request.gambleBase = IsPlayerBotGamblerBaseOffer(ch, preview);
                request.skill = preview->GetType() == ITEM_SKILLBOOK ? GetPlayerBotSkillBookSkillVnum(preview) : 0;
                // Out of the book purse as it is asked for: a refused purchase
                // costs a window's share, which the next window gives back.
                if (preview->GetType() == ITEM_SKILLBOOK)
                    NotePlayerBotBookBought(ch, price);
                M2_DELETE(preview);
            }
            manager.RecvShopOpenClientPacket(ch, o.buyOwner);
            manager.RecvShopBuyItemClientPacket(ch, o.buyOwner, o.buyItem, false, price);
            const bool sent = EndCall(ch->GetPlayerID());
            manager.RecvCloseShopGuestClientPacket(ch);
            sys_log(0, "PLAYERBOT_OFFLINE: purchase_requested buyer=%u owner=%u item=%u vnum=%u price=%lld sent=%d",
                ch->GetPlayerID(), o.buyOwner, o.buyItem, (unsigned int)boughtVnum, (long long)price, sent);
        }
        ReleasePlayerBotFarLine(o.buyItem, ch->GetPlayerID());
        o.farBuy = false;
        o.buyOwner = 0;
        ClearPlayerBotRoute(state, true);
        return false; // DB completion owns delivery; never synthesize money/items
    }

    // Defined below, with the gambler's walk along the counters it was
    // written for.
    bool FindPlayerBotGambleMaterialPick(LPCHARACTER ch, TPlayerBotAIState& state,
            const std::map<DWORD, int>& missing, long long cap, DWORD now);

    bool ManagePlayerBotOfflineShopping(LPCHARACTER ch, TPlayerBotAIState& state, DWORD now) {
        using namespace playerbot_offline;
        auto& o = state.offlineShop;
        auto& manager = ikashop::GetManager();
        // A gambler between the storekeeper and the anvil walks its own pick
        // (ManagePlayerBotGambleMarket, from the town visit, which runs after
        // this pass). The visit is what makes the bot busy below, and dropping
        // the pick there handed the gambler the same line again every tick:
        // eight "goes for materials" for one line in four seconds, and a
        // purchase only where the stand was in reach at once (m2zip,
        // 26 September: 4 lines bought of 167 picks).
        if (state.bTownVisitPhase == BOT_TOWN_PHASE_GAMBLE_MARKET)
            return false;
        const bool busy = BotOfflineBusy(ch, state);
        if (busy || requests.count(ch->GetPlayerID()) || o.visiting || state.bMarketTrip) {
            if (o.buyOwner)
                DropPlayerBotOfflinePick(ch, state, busy ? "busy" : o.visiting ? "service_visit" :
                    requests.count(ch->GetPlayerID()) ? "request_in_flight" : "market_trip");
            return false;
        }
        long pitchX = 0, pitchY = 0;
        if (!GetPlayerBotShopCentre(ch->GetMapIndex(), pitchX, pitchY)) return false;
        // The finished piece the anvil is waiting for, found line by line while
        // the bot stood at the blacksmith, is this buyer's pick before any
        // browse: it may stand anywhere on the map's ring, where a browse of
        // sixty-four lines a look does not reach inside the wait.
        if (!o.buyOwner && o.readyPickOwner) {
            if (!Due(now, o.readyPickUntil)) {
                o.buyOwner = o.readyPickOwner;
                o.buyItem = o.readyPickItem;
                o.buyUntil = now + PLAYERBOT_MARKET_FAR_PICK_WALK_MS;
            }
            o.readyPickOwner = o.readyPickItem = o.readyPickUntil = 0;
        }
        if (!o.buyOwner) {
            if (!Due(now, o.nextBrowse)) return false;
            o.nextBrowse = now + number(120000, 240000);
            const long long budget = Affordable(ch->GetGold(), GetPlayerBotReservedGold(ch), PLAYERBOT_SHOPPING_GOLD_FLOOR);
            if (budget <= 0) return false;
            // What the weapon under Iwakura's scroll rule lacks for its next
            // step is looked for on every stand of the map, the gambler's way,
            // before the browse of sixty-four lines: a first village holds
            // some ten thousand of them, and the rule's bots come in from the
            // frontier for a service visit of a minute or two.
            {
                std::map<DWORD, int> missing;
                CollectPlayerBotScrollRuleMissing(ch, missing);
                if (!missing.empty() && FindPlayerBotGambleMaterialPick(ch, state, missing, budget, now)) {
                    sys_log(0, "PLAYERBOT_MARKET: scroll-rule weapon goes for materials pid=%u name=%s owner=%u item=%u lacking=%u",
                        ch->GetPlayerID(), ch->GetName(), o.buyOwner, o.buyItem, (unsigned int)missing.size());
                    return RunPlayerBotOfflinePick(ch, state, now);
                }
            }
            std::vector<std::pair<int, NativeShop> > shops;
            // Every stand is on the shop channel. With the assignment table a
            // bot elsewhere still reads the stands of its own map and asks to
            // be moved there when one holds something worth buying.
            const int shopChannel = CPlayerBotManager::instance().IsChannelTableMode()
                    ? playerbot_channel_rules::SHOP_CHANNEL : (int)g_bChannel;
            for (const auto& [pid, shop] : manager.GetPlayerBotOfflineShops()) {
                if (!shop || pid == ch->GetPlayerID() || shop->GetDuration() == 0 || shop->IsEditMode()) continue;
                const auto spawn = shop->GetSpawn();
                if (spawn.map != ch->GetMapIndex() || (int)spawn.channel != shopChannel) continue;
                const int distance = DISTANCE_APPROX(spawn.x-ch->GetX(), spawn.y-ch->GetY());
                if (distance <= PLAYERBOT_MARKET_TRIP_RANGE) shops.emplace_back(distance, shop);
            }
            std::sort(shops.begin(), shops.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            // Resume within the shop too: a full counter must not hide item 65.
            int bestPriority = -1;
            long long bestPrice = 0;
            const bool personFirst = number(1, 100) <= PLAYERBOT_MARKET_PERSON_FIRST_PERCENT;
            BrowseLines(shops, o, 64, [&](auto shop, auto id, const auto& line) {
                if (IsPlayerBotLineClaimedByOther(id, ch->GetPlayerID(), now)) return;
                long long price = 0;
                const int priority = RatePlayerBotOfflineLine(ch, shop, line, budget, personFirst, price);
                if (priority < 0 || priority < bestPriority ||
                        (priority == bestPriority && price >= bestPrice)) return;
                bestPriority = priority;
                bestPrice = price;
                o.buyOwner = shop->GetOwnerPID();
                o.buyItem = id;
                o.buyUntil = now + 45000;
            });
            // A first village's stands with nothing on them this bot came for:
            // it asks on the trade channel, as a trip that found the market
            // empty always did. The classic trip was that trip's only caller,
            // and on this engine it no longer sets off (ManagePlayerBotShopping).
            // The shout is throttled world-wide and says nothing for a bag
            // that wants no material.
            if (!o.buyOwner && IsPlayerBotM1Map(ch->GetMapIndex()))
                AnnouncePlayerBotNeed(ch);
        }
        if (!o.buyOwner) return false;
        return RunPlayerBotOfflinePick(ch, state, now);
    }
    // From a second village the buyer above cannot see its first village's
    // stands: they stand on another map. StartPlayerBotFarMarketWalk asks this
    // before the walk over - PLAYERBOT_MARKET_FAR_LOOK_LINES lines on a cursor
    // of their own, the next look going on where this one stopped - and the
    // line found is kept for the buyer, who takes it on arrival
    // (HandPlayerBotFarPickToBuyer). The walk used to be made for whatever
    // the bot might want, and nine times in ten it bought nothing.
    bool FindPlayerBotFarOfflinePick(LPCHARACTER ch, TPlayerBotAIState& state, long mapIndex) {
        using namespace playerbot_offline;
        auto& o = state.offlineShop;
        o.farPickOwner = o.farPickItem = 0;
        const long long budget = Affordable(ch->GetGold(), GetPlayerBotReservedGold(ch), PLAYERBOT_SHOPPING_GOLD_FLOOR);
        if (budget <= 0) return false;
        // In the order of their distance from the market's middle, which does
        // not move, so the cursor means the same thing at the next look.
        long centreX = 0, centreY = 0;
        GetPlayerBotShopCentre(mapIndex, centreX, centreY);
        const int shopChannel = CPlayerBotManager::instance().IsChannelTableMode()
                ? playerbot_channel_rules::SHOP_CHANNEL : (int)g_bChannel;
        std::vector<std::pair<int, NativeShop> > shops;
        for (const auto& [pid, shop] : ikashop::GetManager().GetPlayerBotOfflineShops()) {
            if (!shop || pid == ch->GetPlayerID() || shop->GetDuration() == 0 || shop->IsEditMode()) continue;
            const auto spawn = shop->GetSpawn();
            if (spawn.map != mapIndex || (int)spawn.channel != shopChannel) continue;
            shops.emplace_back(DISTANCE_APPROX(spawn.x - centreX, spawn.y - centreY), shop);
        }
        if (shops.empty()) return false;
        std::sort(shops.begin(), shops.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        // A first look starts at a stand drawn by pid. From the middle, every
        // bot of a village read the same sixty-four lines and set off for the
        // same one - three walks in the first minute on the test world for
        // owner 1863's line, which one of them could buy.
        if (!o.farBrowseOwner) {
            o.farBrowseOwner = shops[ch->GetPlayerID() % shops.size()].second->GetOwnerPID();
            o.farBrowseItem = 0;
        }
        int bestPriority = -1;
        long long bestPrice = 0;
        const bool personFirst = number(1, 100) <= PLAYERBOT_MARKET_PERSON_FIRST_PERCENT;
        const DWORD now = get_dword_time();
        std::swap(o.browseOwner, o.farBrowseOwner);
        std::swap(o.browseItem, o.farBrowseItem);
        BrowseLines(shops, o, PLAYERBOT_MARKET_FAR_LOOK_LINES, [&](auto shop, auto id, const auto& line) {
            if (IsPlayerBotLineClaimedByOther(id, ch->GetPlayerID(), now)) return;
            long long price = 0;
            const int priority = RatePlayerBotOfflineLine(ch, shop, line, budget, personFirst, price);
            if (priority < 0 || priority < bestPriority ||
                    (priority == bestPriority && price >= bestPrice)) return;
            bestPriority = priority;
            bestPrice = price;
            o.farPickOwner = shop->GetOwnerPID();
            o.farPickItem = id;
        });
        std::swap(o.browseOwner, o.farBrowseOwner);
        std::swap(o.browseItem, o.farBrowseItem);
        return o.farPickOwner != 0;
    }

    // Across: the line the walk was made for is the buyer's pick now, with
    // the time to ride from the gate to the stand, and the stands in reach
    // are read at once should it have sold meanwhile. Called on the tick of
    // arrival, and it claims that tick while the bot walks to the stand, or
    // the travel pass further down the tick would take it straight back.
    bool HandPlayerBotFarPickToBuyer(LPCHARACTER ch, TPlayerBotAIState& state, DWORD now) {
        auto& o = state.offlineShop;
        if (o.farPickOwner) {
            o.buyOwner = o.farPickOwner;
            o.buyItem = o.farPickItem;
            o.buyUntil = now + PLAYERBOT_MARKET_FAR_PICK_WALK_MS;
            o.farBuy = true;
        }
        o.farPickOwner = o.farPickItem = 0;
        o.nextBrowse = 0;
        return ManagePlayerBotOfflineShopping(ch, state, now);
    }

    // Declared in playerbot_economy.h: whether a stand of this bot's map holds
    // a finished piece for the slot that the buyer would take - the purchase's
    // own tests (WantsPlayerBotStallItem, the Perfectionist's purse) asked of
    // the line - and, when one does, the line handed to the buyer and claimed
    // for the wait. The anvil asks it once in PLAYERBOT_READY_GEAR_RECHECK_MS
    // a slot, so a read over every line of the map costs nothing to speak of;
    // only a line of the right kind, grade and class is built into an item.
    bool PlayerBotFindReadyGearToBuy(LPCHARACTER ch, TPlayerBotAIState& state, int wearCell) {
        using namespace playerbot_offline;
        if (!ch || !IsPlayerBotReadyGearSlot(wearCell)) return false;
        const long long budget = Affordable(ch->GetGold(), GetPlayerBotReservedGold(ch), PLAYERBOT_SHOPPING_GOLD_FLOOR);
        if (budget <= 0) return false;
        const int shopChannel = CPlayerBotManager::instance().IsChannelTableMode()
                ? playerbot_channel_rules::SHOP_CHANNEL : (int)g_bChannel;
        const DWORD now = get_dword_time();
        for (const auto& [pid, shop] : ikashop::GetManager().GetPlayerBotOfflineShops()) {
            if (!shop || pid == ch->GetPlayerID() || shop->GetDuration() == 0) continue;
            const auto spawn = shop->GetSpawn();
            if (spawn.map != ch->GetMapIndex() || (int)spawn.channel != shopChannel) continue;
            for (const auto& [id, line] : shop->GetItems()) {
                if (!line) continue;
                const DWORD vnum = line->GetInfo().vnum;
                const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
                if (!IsPlayerBotReadyGearProto(ch, proto, vnum, wearCell) ||
                        !IsPlayerBotReadyGearOverWorn(ch, proto, vnum, wearCell) ||
                        IsPlayerBotLineClaimedByOther(id, ch->GetPlayerID(), now))
                    continue;
                const long long price = (long long)line->GetPrice().GetTotalYangAmount();
                if (price <= 0 || price > budget) continue;
                LPITEM preview = BotOfflinePreview(*line);
                if (!preview) continue;
                const bool buyable = preview->FindEquipCell(ch) == wearCell &&
                        IsPlayerBotReadyGearOffer(ch, preview) && WantsPlayerBotStallItem(ch, preview) &&
                        CanPlayerBotPayForOffer(ch, preview, price);
                M2_DELETE(preview);
                if (!buyable) continue;
                auto& o = state.offlineShop;
                o.readyPickOwner = shop->GetOwnerPID();
                o.readyPickItem = id;
                o.readyPickUntil = now + PLAYERBOT_READY_GEAR_WAIT_MS;
                ClaimPlayerBotLineUntil(id, ch->GetPlayerID(), now, now + PLAYERBOT_READY_GEAR_WAIT_MS);
                return true;
            }
        }
        return false;
    }

    // The cheapest line a unit of a material the gambler lacks
    // (CollectPlayerBotGambleMissingMaterials), on a stand of its map it can
    // walk to, that the buyer would take on arrival - the purchase's own tests
    // asked of the line - for no more than `cap`. Handed to the buyer and
    // claimed for the walk, as the Perfectionist's finished piece is.
    bool FindPlayerBotGambleMaterialPick(LPCHARACTER ch, TPlayerBotAIState& state,
            const std::map<DWORD, int>& missing, long long cap, DWORD now) {
        using namespace playerbot_offline;
        if (!ch || missing.empty() || cap <= 0) return false;
        const int shopChannel = CPlayerBotManager::instance().IsChannelTableMode()
                ? playerbot_channel_rules::SHOP_CHANNEL : (int)g_bChannel;
        CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(ch->GetMapIndex());
        const bool haveNav = navigation.Init(ch->GetMapIndex());
        DWORD bestOwner = 0, bestItem = 0;
        long long bestUnit = 0;
        for (const auto& [pid, shop] : ikashop::GetManager().GetPlayerBotOfflineShops()) {
            if (!shop || pid == ch->GetPlayerID() || shop->GetDuration() == 0 || shop->IsEditMode()) continue;
            const auto spawn = shop->GetSpawn();
            if (spawn.map != ch->GetMapIndex() || (int)spawn.channel != shopChannel) continue;
            // Asked once a stand has a line worth walking to: Joan's wall
            // keeps some of its stands from the storekeeper's side.
            int reach = -1;
            for (const auto& [id, line] : shop->GetItems()) {
                if (!line || missing.find(line->GetInfo().vnum) == missing.end() ||
                        IsPlayerBotLineClaimedByOther(id, ch->GetPlayerID(), now))
                    continue;
                const long long price = (long long)line->GetPrice().GetTotalYangAmount();
                const long long unit = price / std::max<long long>(1, (long long)line->GetInfo().count);
                if (price <= 0 || price > cap || (bestOwner && unit >= bestUnit)) continue;
                if (reach < 0)
                    reach = !haveNav || navigation.CanReach(ch->GetX(), ch->GetY(), spawn.x, spawn.y) ? 1 : 0;
                if (reach == 0) break;
                LPITEM preview = BotOfflinePreview(*line);
                if (!preview) continue;
                const bool buyable = WantsPlayerBotStallItem(ch, preview) &&
                        CanPlayerBotPayForOffer(ch, preview, price) && ch->GetEmptyInventory(preview->GetSize()) >= 0;
                M2_DELETE(preview);
                if (!buyable) continue;
                bestOwner = shop->GetOwnerPID();
                bestItem = id;
                bestUnit = unit;
            }
        }
        if (!bestOwner) return false;
        auto& o = state.offlineShop;
        o.buyOwner = bestOwner;
        o.buyItem = bestItem;
        o.buyUntil = now + PLAYERBOT_MARKET_FAR_PICK_WALK_MS;
        o.farBuy = false;
        ClaimPlayerBotLineUntil(bestItem, ch->GetPlayerID(), now, o.buyUntil);
        return true;
    }

    // Declared in playerbot_town.h: a gambler's session between the
    // storekeeper and the anvil (BOT_TOWN_PHASE_GAMBLE_MARKET), by Iwakura's
    // answer of 26 September - "Zabiera baze z magazynu do ekwipunku ... i
    // kupuje ulepszacze na rynku i ulepsza, moze wiecej niz 1 Item, poki
    // budzet pozwala". It buys what its pieces lack for their plain steps to
    // +7, a line at a time, the cheapest first, each within
    // PLAYERBOT_GAMBLE_MARKET_BUDGET_PERCENT of what is left of the session's
    // budget, and waits for the db core to deliver each before it looks
    // again. True while it has the tick; false when the step is over - nothing
    // lacking, nothing on the counters it could take, PLAYERBOT_GAMBLE_MARKET_BUYS
    // lines bought, or PLAYERBOT_GAMBLE_MARKET_MS gone.
    bool ManagePlayerBotGambleMarket(LPCHARACTER ch, TPlayerBotAIState& state, DWORD now) {
        using namespace playerbot_offline;
        if (!ch) return false;
        auto& p = state.persona;
        auto& o = state.offlineShop;
        if (!p.bGambleMarketPending || !IsPlayerBotGambling(state, now)) {
            if (o.buyOwner) DropPlayerBotOfflinePick(ch, state, "gamble_over");
            return false;
        }
        if (p.dwGambleMarketUntil == 0) p.dwGambleMarketUntil = now + PLAYERBOT_GAMBLE_MARKET_MS;
        const bool timeUp = Due(now, p.dwGambleMarketUntil);
        // A purchase the db core has not answered yet: its goods are not in
        // the bag, so what is lacking cannot be asked again until they are.
        if (requests.count(ch->GetPlayerID())) {
            if (timeUp) return false;
            SetPlayerBotAction(state, BOT_ACTION_SHOP, now);
            return true;
        }
        if (o.buyOwner) {
            if (timeUp) {
                DropPlayerBotOfflinePick(ch, state, "gamble_time");
                return false;
            }
            RunPlayerBotOfflinePick(ch, state, now);
            return true;
        }
        if (timeUp || p.bGambleMarketBuys >= PLAYERBOT_GAMBLE_MARKET_BUYS ||
                g_bChannel != playerbot_channel_rules::SHOP_CHANNEL)
            return false;
        std::map<DWORD, int> missing;
        CollectPlayerBotGambleMissingMaterials(ch, missing);
        if (missing.empty())
            return false;
        const long long left = playerbot_persona::BudgetLeft(p.llGambleGoldStart,
                GetPlayerBotGambleBudgetPercent(p, now), p.llGambleSpent);
        const long long cap = std::min<long long>(left * PLAYERBOT_GAMBLE_MARKET_BUDGET_PERCENT / 100,
                Affordable(ch->GetGold(), GetPlayerBotReservedGold(ch), PLAYERBOT_SHOPPING_GOLD_FLOOR));
        if (!FindPlayerBotGambleMaterialPick(ch, state, missing, cap, now)) {
            PlayerBotLogThrottled("gamble_market_empty", now,
                    "PLAYERBOT_PERSONA: gambler finds none of its materials on the counters pid=%u name=%s lacking=%u cap=%lld bought=%u",
                    ch->GetPlayerID(), ch->GetName(), (unsigned int)missing.size(), cap, (unsigned int)p.bGambleMarketBuys);
            return false;
        }
        ++p.bGambleMarketBuys;
        sys_log(0, "PLAYERBOT_PERSONA: gambler goes for materials pid=%u name=%s owner=%u item=%u lacking=%u line=%u cap=%lld",
                ch->GetPlayerID(), ch->GetName(), o.buyOwner, o.buyItem, (unsigned int)missing.size(),
                (unsigned int)p.bGambleMarketBuys, cap);
        RunPlayerBotOfflinePick(ch, state, now);
        return true;
    }

    void AddPlayerBotOfflineLedger(DWORD& stalls, DWORD& lines) {
        for (const auto& [pid, shop] : ikashop::GetManager().GetPlayerBotOfflineShops()) {
            if (!shop || shop->GetDuration() == 0 || shop->GetSpawn().channel != g_bChannel) continue;
            ++stalls;
            ++s_mapPlayerBotStallsByMap[shop->GetSpawn().map];
            if (IsPlayerBotM2Map(shop->GetSpawn().map)) ++s_iPlayerBotStallsInM2;
            const bool botShop = CPlayerBotManager::instance().IsRegisteredBotPID(shop->GetOwnerPID());
            for (const auto& [id, item] : shop->GetItems()) {
                if (!item) continue;
                AddPlayerBotMarketSupply(item->GetVnum(), item->GetInfo().count, shop->GetSpawn().map);
                if (botShop) {
                    NotePlayerBotCappedLineOnCounter(item->GetVnum(), item->GetInfo().count);
                    NotePlayerBotMissionBooksOnCounter(shop->GetSpawn().map, item->GetVnum(), item->GetInfo().count);
                }
                ++lines;
            }
        }
    }
}
#else
namespace {
    // The classic stalls of r40250 are the keepers' online counters, bought
    // from where the buyer stands (FindPlayerBotStallPick): a gambler there
    // goes from the storekeeper to the anvil as it always did.
    bool ManagePlayerBotGambleMarket(LPCHARACTER, TPlayerBotAIState&, DWORD) { return false; }
}
#endif
#endif
