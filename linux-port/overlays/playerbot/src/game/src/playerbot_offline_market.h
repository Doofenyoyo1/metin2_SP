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
        // A Stalki project (playerbot_stalki.h) under the level-30 weapon, the
        // higher plus first.
        if (want && IsPlayerBotStalkiProjectOffer(ch, preview))
            priority = 350 + std::min<int>(9, (int)preview->GetRefineLevel());
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

    // The people's stands among `shops`, every line up to
    // PLAYERBOT_MARKET_PERSON_LOOK_LINES, for a browse that looks at them
    // first: the window below reads sixty-four lines from a cursor, and a
    // first village holds some ten thousand.
    template<class Shops, class Visitor>
    void BrowsePlayerBotPersonLines(const Shops& shops, Visitor visit) {
        unsigned checked = 0;
        for (const auto& entry : shops) {
            const auto& shop = entry.second;
            if (CPlayerBotManager::instance().IsRegisteredBotPID(shop->GetOwnerPID()))
                continue;
            for (const auto& [id, line] : shop->GetItems()) {
                if (++checked > PLAYERBOT_MARKET_PERSON_LOOK_LINES)
                    return;
                visit(shop, id, line);
            }
        }
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
        if (IsPlayerBotSashVnum(boughtVnum))
            NotePlayerBotSashBought(ch, boughtVnum, (long long)price);
        NotePlayerBotSaddlebagBought(ch, boughtVnum, (long long)price);
        NotePlayerBotGuildMaterialBought(ch, boughtVnum, (long long)price);
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
    bool FindPlayerBotRareGamblerBasePick(LPCHARACTER ch, TPlayerBotAIState& state, long long cap, DWORD now);
    bool FindPlayerBotStalkiPick(LPCHARACTER ch, TPlayerBotAIState& state, long long cap, DWORD now);
    bool FindPlayerBotBookPick(LPCHARACTER ch, TPlayerBotAIState& state, long long cap, DWORD now);
    bool FindPlayerBotOutdatedGearPick(LPCHARACTER ch, TPlayerBotAIState& state, long long cap, DWORD now);

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
            // A guild master whose next building lacks materials looks for
            // them on every stand of the map, the gambler's way, paying out of
            // the guild's fund - which the budget below leaves out, being
            // reserved (playerbot_guild_land.h).
            {
                std::map<DWORD, int> missing;
                const long long guildCap = CollectPlayerBotGuildMaterialMissing(ch, missing);
                if (!missing.empty() && guildCap > 0 && FindPlayerBotGambleMaterialPick(ch, state, missing, guildCap, now)) {
                    sys_log(0, "PLAYERBOT_GUILD_LAND: master goes for materials pid=%u name=%s owner=%u item=%u lacking=%u",
                        ch->GetPlayerID(), ch->GetName(), o.buyOwner, o.buyItem, (unsigned int)missing.size());
                    return RunPlayerBotOfflinePick(ch, state, now);
                }
            }
            const long long budget = Affordable(ch->GetGold(), GetPlayerBotReservedGold(ch), PLAYERBOT_SHOPPING_GOLD_FLOOR);
            if (budget <= 0) return false;
            // What the piece under Iwakura's scroll rule lacks for its next
            // step - the weapon, or the armour once the weapon is at +8 - is
            // looked for on every stand of the map, the gambler's way, before
            // the browse of sixty-four lines: a first village holds some ten
            // thousand of them, and the rule's bots come in from the frontier
            // for a service visit of a minute or two.
            {
                std::map<DWORD, int> missing;
                CollectPlayerBotScrollRuleMissing(ch, missing);
                if (!missing.empty() && FindPlayerBotGambleMaterialPick(ch, state, missing, budget, now)) {
                    sys_log(0, "PLAYERBOT_MARKET: scroll-rule piece goes for materials pid=%u name=%s owner=%u item=%u lacking=%u",
                        ch->GetPlayerID(), ch->GetName(), o.buyOwner, o.buyItem, (unsigned int)missing.size());
                    return RunPlayerBotOfflinePick(ch, state, now);
                }
            }
            // And one of Community Patch 5's four gamblers buying its bases
            // looks on every stand of the map for one of its category.
            if (FindPlayerBotRareGamblerBasePick(ch, state, budget, now)) {
                sys_log(0, "PLAYERBOT_MARKET: gambler goes for a base pid=%u name=%s owner=%u item=%u category=%u bought=%u/%u",
                    ch->GetPlayerID(), ch->GetName(), o.buyOwner, o.buyItem,
                    (unsigned int)state.persona.bGambleBuyCategory, (unsigned int)state.persona.bRareBought,
                    (unsigned int)state.persona.bRareBuyWant);
                return RunPlayerBotOfflinePick(ch, state, now);
            }
            // The books of its own skills at Master: every stand of the map,
            // not the browse's sixty-four lines a look. A bot on another
            // channel finds its line on the shop channel's stands and asks to
            // be moved there (RunPlayerBotOfflinePick).
            if (FindPlayerBotBookPick(ch, state, budget, now)) {
                sys_log(0, "PLAYERBOT_MARKET: goes for a skill book pid=%u name=%s owner=%u item=%u gold=%lld channel=%u",
                    ch->GetPlayerID(), ch->GetName(), o.buyOwner, o.buyItem, (long long)ch->GetGold(),
                    (unsigned int)g_bChannel);
                return RunPlayerBotOfflinePick(ch, state, now);
            }
            // An outdated shield, helmet or body armour from level fifty: the
            // piece that replaces it, on every stand of the map.
            if (FindPlayerBotOutdatedGearPick(ch, state, budget, now)) {
                sys_log(0, "PLAYERBOT_MARKET: goes for a piece over outdated gear pid=%u name=%s owner=%u item=%u level=%d",
                    ch->GetPlayerID(), ch->GetName(), o.buyOwner, o.buyItem, (int)ch->GetLevel());
                return RunPlayerBotOfflinePick(ch, state, now);
            }
            // And a bot in the market for a Stalki (playerbot_stalki.h) looks on
            // every stand of the map for one: the few lines of seven families
            // stand among some ten thousand, where the browse reads sixty-four.
            if (FindPlayerBotStalkiPick(ch, state, budget, now)) {
                sys_log(0, "PLAYERBOT_MARKET: stalki goes for a piece pid=%u name=%s owner=%u item=%u level=%d gold=%lld",
                    ch->GetPlayerID(), ch->GetName(), o.buyOwner, o.buyItem, (int)ch->GetLevel(),
                    (long long)ch->GetGold());
                return RunPlayerBotOfflinePick(ch, state, now);
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
            auto rate = [&](auto shop, auto id, const auto& line) {
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
            };
            if (personFirst)
                BrowsePlayerBotPersonLines(shops, rate);
            if (!o.buyOwner)
                BrowseLines(shops, o, 64, rate);
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
        auto rate = [&](auto shop, auto id, const auto& line) {
            if (IsPlayerBotLineClaimedByOther(id, ch->GetPlayerID(), now)) return;
            long long price = 0;
            const int priority = RatePlayerBotOfflineLine(ch, shop, line, budget, personFirst, price);
            if (priority < 0 || priority < bestPriority ||
                    (priority == bestPriority && price >= bestPrice)) return;
            bestPriority = priority;
            bestPrice = price;
            o.farPickOwner = shop->GetOwnerPID();
            o.farPickItem = id;
        };
        if (personFirst)
            BrowsePlayerBotPersonLines(shops, rate);
        if (!o.farPickOwner) {
            std::swap(o.browseOwner, o.farBrowseOwner);
            std::swap(o.browseItem, o.farBrowseItem);
            BrowseLines(shops, o, PLAYERBOT_MARKET_FAR_LOOK_LINES, rate);
            std::swap(o.browseOwner, o.farBrowseOwner);
            std::swap(o.browseItem, o.farBrowseItem);
        }
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

    // One of Community Patch 5's four gamblers buying its bases
    // (IsPlayerBotRareGamblerBuying): the cheapest line of its category on a
    // stand of its map it can walk to, the higher level first for the share
    // that wants weapons over thirty, that the buyer would take on arrival -
    // the purchase's own tests - for no more than `cap`. The browse reads
    // sixty-four lines a look, and a first village holds some ten thousand:
    // a gambler with three hours would have spent most of them looking.
    bool FindPlayerBotRareGamblerBasePick(LPCHARACTER ch, TPlayerBotAIState& state, long long cap, DWORD now) {
        using namespace playerbot_offline;
        if (!ch || cap <= 0 || !IsPlayerBotRareGamblerBuying(state.persona, now)) return false;
        const int shopChannel = CPlayerBotManager::instance().IsChannelTableMode()
                ? playerbot_channel_rules::SHOP_CHANNEL : (int)g_bChannel;
        CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(ch->GetMapIndex());
        const bool haveNav = navigation.Init(ch->GetMapIndex());
        DWORD bestOwner = 0, bestItem = 0;
        long long bestPrice = 0;
        int bestLevel = -1;
        for (const auto& [pid, shop] : ikashop::GetManager().GetPlayerBotOfflineShops()) {
            if (!shop || pid == ch->GetPlayerID() || shop->GetDuration() == 0 || shop->IsEditMode()) continue;
            const auto spawn = shop->GetSpawn();
            if (spawn.map != ch->GetMapIndex() || (int)spawn.channel != shopChannel) continue;
            int reach = -1;
            for (const auto& [id, line] : shop->GetItems()) {
                if (!line || line->GetInfo().count != 1 || IsPlayerBotLineClaimedByOther(id, ch->GetPlayerID(), now))
                    continue;
                const TItemTable* proto = line->GetTable();
                if (!proto || (proto->bType != ITEM_WEAPON && proto->bType != ITEM_ARMOR)) continue;
                const long long price = (long long)line->GetPrice().GetTotalYangAmount();
                if (price <= 0 || price > cap) continue;
                LPITEM preview = BotOfflinePreview(*line);
                if (!preview) continue;
                const int level = (int)GetPlayerBotPersonaLevelLimit(preview);
                const bool better = !bestOwner || (state.persona.bGambleWeaponHigh && level != bestLevel
                        ? level > bestLevel : price < bestPrice);
                bool buyable = false;
                if (better && IsPlayerBotRareGamblerBaseOffer(ch, preview)) {
                    if (reach < 0)
                        reach = !haveNav || navigation.CanReach(ch->GetX(), ch->GetY(), spawn.x, spawn.y) ? 1 : 0;
                    buyable = reach == 1 && WantsPlayerBotStallItem(ch, preview) &&
                            CanPlayerBotPayForOffer(ch, preview, price) && ch->GetEmptyInventory(preview->GetSize()) >= 0;
                }
                M2_DELETE(preview);
                if (reach == 0) break;
                if (!buyable) continue;
                bestOwner = shop->GetOwnerPID();
                bestItem = id;
                bestPrice = price;
                bestLevel = level;
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

    // A bot in the market for a Stalki (IsPlayerBotStalkiShopper): the best
    // line of its own on a stand of its map it can walk to - the equipment
    // pass's score, the cheaper on a tie - that the buyer would take on
    // arrival, the purchase's own tests asked of the line, for no more than
    // `cap`. Only a line whose vnum is one of the families is built into an
    // item, so a look over the whole map costs a vnum test a line.
    bool FindPlayerBotStalkiPick(LPCHARACTER ch, TPlayerBotAIState& state, long long cap, DWORD now) {
        using namespace playerbot_offline;
        if (!ch || cap <= 0 ||
                (!IsPlayerBotStalkiShopper(ch, playerbot_stalki_rules::KIND_ARMOUR) &&
                 !IsPlayerBotStalkiShopper(ch, playerbot_stalki_rules::KIND_WEAPON)))
            return false;
        const int shopChannel = CPlayerBotManager::instance().IsChannelTableMode()
                ? playerbot_channel_rules::SHOP_CHANNEL : (int)g_bChannel;
        CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(ch->GetMapIndex());
        const bool haveNav = navigation.Init(ch->GetMapIndex());
        DWORD bestOwner = 0, bestItem = 0;
        long long bestPrice = 0, bestScore = 0;
        for (const auto& [pid, shop] : ikashop::GetManager().GetPlayerBotOfflineShops()) {
            if (!shop || pid == ch->GetPlayerID() || shop->GetDuration() == 0 || shop->IsEditMode()) continue;
            const auto spawn = shop->GetSpawn();
            if (spawn.map != ch->GetMapIndex() || (int)spawn.channel != shopChannel) continue;
            int reach = -1;
            for (const auto& [id, line] : shop->GetItems()) {
                if (!line || line->GetInfo().count != 1 || !playerbot_stalki_rules::IsStalki(line->GetInfo().vnum) ||
                        IsPlayerBotLineClaimedByOther(id, ch->GetPlayerID(), now))
                    continue;
                const long long price = (long long)line->GetPrice().GetTotalYangAmount();
                if (price <= 0 || price > cap) continue;
                LPITEM preview = BotOfflinePreview(*line);
                if (!preview) continue;
                const long long score = GetPlayerBotEquipmentScore(preview, ch);
                const bool better = !bestOwner || score > bestScore || (score == bestScore && price < bestPrice);
                bool buyable = false;
                if (better && IsPlayerBotStalkiProjectOffer(ch, preview)) {
                    if (reach < 0)
                        reach = !haveNav || navigation.CanReach(ch->GetX(), ch->GetY(), spawn.x, spawn.y) ? 1 : 0;
                    buyable = reach == 1 && WantsPlayerBotStallItem(ch, preview) &&
                            CanPlayerBotPayForOffer(ch, preview, price) && ch->GetEmptyInventory(preview->GetSize()) >= 0;
                }
                M2_DELETE(preview);
                if (reach == 0) break;
                if (!buyable) continue;
                bestOwner = shop->GetOwnerPID();
                bestItem = id;
                bestPrice = price;
                bestScore = score;
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

    // The books of its own skills at Master (PlayerBotWantsOwnBooks): the
    // cheapest book a unit on a stand of its map the buyer would take on
    // arrival, the purchase's own tests asked of the line, for no more than
    // `cap`. Only a line whose proto is a skill book is built into an item.
    bool FindPlayerBotBookPick(LPCHARACTER ch, TPlayerBotAIState& state, long long cap, DWORD now) {
        using namespace playerbot_offline;
        if (!ch || cap <= 0 || !PlayerBotWantsOwnBooks(ch)) return false;
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
            int reach = -1;
            for (const auto& [id, line] : shop->GetItems()) {
                if (!line || !line->GetTable() || line->GetTable()->bType != ITEM_SKILLBOOK ||
                        IsPlayerBotLineClaimedByOther(id, ch->GetPlayerID(), now))
                    continue;
                const auto& info = line->GetInfo();
                const DWORD skill = info.vnum == 50300 ? (DWORD)info.alSockets[0] : (DWORD)line->GetTable()->alValues[0];
                if (!IsPlayerBotOwnSkill(ch, skill) || ch->GetSkillMasterType(skill) != SKILL_MASTER) continue;
                const long long price = (long long)line->GetPrice().GetTotalYangAmount();
                if (price <= 0 || price > cap) continue;
                const long long unit = price / std::max<long long>(1, (long long)info.count);
                if (bestOwner && unit >= bestUnit) continue;
                if (reach < 0)
                    reach = !haveNav || navigation.CanReach(ch->GetX(), ch->GetY(), spawn.x, spawn.y) ? 1 : 0;
                if (reach == 0) break;
                LPITEM preview = BotOfflinePreview(*line);
                if (!preview) continue;
                const bool buyable = IsPlayerBotProgressionOffer(ch, preview) && WantsPlayerBotStallItem(ch, preview) &&
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

    // The piece that replaces an outdated shield, helmet or body armour
    // (IsPlayerBotOutdatedGearOffer): a finished one at +6 or more first, then
    // the highest level, then the cheapest - on a stand of the map the bot
    // can walk to, for no more than `cap`.
    bool FindPlayerBotOutdatedGearPick(LPCHARACTER ch, TPlayerBotAIState& state, long long cap, DWORD now) {
        using namespace playerbot_offline;
        if (!ch || cap <= 0 || (int)ch->GetLevel() < PLAYERBOT_OUTDATED_GEAR_MIN_LEVEL) return false;
        bool outdated[3] = {
            IsPlayerBotOutdatedGear(ch, ch->GetWear(WEAR_BODY)),
            IsPlayerBotOutdatedGear(ch, ch->GetWear(WEAR_HEAD)),
            IsPlayerBotOutdatedGear(ch, ch->GetWear(WEAR_SHIELD)) };
        if (!outdated[0] && !outdated[1] && !outdated[2]) return false;
        const int shopChannel = CPlayerBotManager::instance().IsChannelTableMode()
                ? playerbot_channel_rules::SHOP_CHANNEL : (int)g_bChannel;
        CPlayerBotNavigation& navigation = CPlayerBotNavigation::instance(ch->GetMapIndex());
        const bool haveNav = navigation.Init(ch->GetMapIndex());
        DWORD bestOwner = 0, bestItem = 0;
        long long bestRank = -1, bestPrice = 0;
        for (const auto& [pid, shop] : ikashop::GetManager().GetPlayerBotOfflineShops()) {
            if (!shop || pid == ch->GetPlayerID() || shop->GetDuration() == 0 || shop->IsEditMode()) continue;
            const auto spawn = shop->GetSpawn();
            if (spawn.map != ch->GetMapIndex() || (int)spawn.channel != shopChannel) continue;
            int reach = -1;
            for (const auto& [id, line] : shop->GetItems()) {
                const TItemTable* table = line ? line->GetTable() : NULL;
                if (!table || table->bType != ITEM_ARMOR || line->GetInfo().count != 1 ||
                        IsPlayerBotLineClaimedByOther(id, ch->GetPlayerID(), now))
                    continue;
                const int slot = table->bSubType == ARMOR_BODY ? 0 : table->bSubType == ARMOR_HEAD ? 1 :
                        table->bSubType == ARMOR_SHIELD ? 2 : -1;
                if (slot < 0 || !outdated[slot]) continue;
                const int level = GetPlayerBotProtoLevelLimit(table);
                if (level > (int)ch->GetLevel() || level + PLAYERBOT_OUTDATED_GEAR_LEVELS <= (int)ch->GetLevel()) continue;
                const long long price = (long long)line->GetPrice().GetTotalYangAmount();
                if (price <= 0 || price > cap) continue;
                const int plus = (int)(line->GetInfo().vnum % 10);
                const long long rank = (plus >= 6 ? 1000000LL : 0LL) + (long long)level * 100 + plus;
                if (rank < bestRank || (rank == bestRank && price >= bestPrice)) continue;
                if (reach < 0)
                    reach = !haveNav || navigation.CanReach(ch->GetX(), ch->GetY(), spawn.x, spawn.y) ? 1 : 0;
                if (reach == 0) break;
                LPITEM preview = BotOfflinePreview(*line);
                if (!preview) continue;
                const bool buyable = IsPlayerBotOutdatedGearOffer(ch, preview) && WantsPlayerBotStallItem(ch, preview) &&
                        CanPlayerBotPayForOffer(ch, preview, price) && ch->GetEmptyInventory(preview->GetSize()) >= 0;
                M2_DELETE(preview);
                if (!buyable) continue;
                bestOwner = shop->GetOwnerPID();
                bestItem = id;
                bestRank = rank;
                bestPrice = price;
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

    // The slips standing on the running stands of this channel as the last
    // ledger pass found them (IsPlayerBotStandingPriceSlip), and when this
    // core first saw each: what CorrectPlayerBotStandingSlips works from.
    struct TPlayerBotStandingSlip { DWORD owner, item; };
    std::vector<TPlayerBotStandingSlip> s_vecPlayerBotStandingSlips;
    std::map<DWORD, DWORD> s_mapPlayerBotSlipSeenAt;

    void AddPlayerBotOfflineLedger(DWORD& stalls, DWORD& lines) {
        s_vecPlayerBotStandingSlips.clear();
        for (const auto& [pid, shop] : ikashop::GetManager().GetPlayerBotOfflineShops()) {
            if (!shop || shop->GetDuration() == 0 || shop->GetSpawn().channel != g_bChannel) continue;
            ++stalls;
            ++s_mapPlayerBotStallsByMap[shop->GetSpawn().map];
            if (IsPlayerBotM2Map(shop->GetSpawn().map)) ++s_iPlayerBotStallsInM2;
            const bool botShop = CPlayerBotManager::instance().IsRegisteredBotPID(shop->GetOwnerPID());
            bool rareKinds[PLAYERBOT_RARE_GOODS_KINDS] = { false };
            for (const auto& [id, item] : shop->GetItems()) {
                if (!item) continue;
                // A slipped price is no bot's supply: no bot buys it (Community
                // Patch 5, point 6), so a trip made for it, or a listing held
                // back by it, would be made for nothing. Only a drawn line of one
                // unit is looked at closer, one in a thousand of them.
                bool slip = false;
                if (item->GetInfo().count == 1 && IsPlayerBotPriceSlipDrawn(id)) {
                    if (LPITEM preview = BotOfflinePreview(*item)) {
                        slip = IsPlayerBotStandingPriceSlip(preview, (long long)item->GetPrice().GetTotalYangAmount());
                        M2_DELETE(preview);
                    }
                    if (slip)
                        s_vecPlayerBotStandingSlips.push_back(TPlayerBotStandingSlip{ shop->GetOwnerPID(), id });
                }
                if (!slip)
                    AddPlayerBotMarketSupply(item->GetVnum(), item->GetInfo().count, shop->GetSpawn().map);
                if (botShop) {
                    NotePlayerBotCappedLineOnCounter(item->GetVnum(), item->GetInfo().count);
                    NotePlayerBotMissionBooksOnCounter(shop->GetSpawn().map, item->GetVnum(), item->GetInfo().count);
                }
                rareKinds[GetPlayerBotRareGoodsKind(item->GetVnum())] = true;
                ++lines;
            }
            // The bots' counters, and which of them carry a Cor Draconis or a
            // sash (IsPlayerBotRareGoodsShopQuotaFull).
            if (botShop) {
                ++s_iPlayerBotRareGoodsBotShops;
                for (int kind = PLAYERBOT_RARE_GOODS_NONE + 1; kind < PLAYERBOT_RARE_GOODS_KINDS; ++kind)
                    if (rareKinds[kind]) NotePlayerBotShopWithRareGoods(kind);
            }
        }
    }

    // A slip stands four hours at most (Community Patch 5, point 6). Its
    // keeper puts it right at its stand, called a little early
    // (BotOfflineDueSlipLine); this is the core putting it right where the
    // keeper cannot, PLAYERBOT_OFFLINE_SLIP_CORE_FIXES lines a ledger pass: a
    // bot's stand this core hosts whose keeper is not in the world here -
    // logged out, resting, moved to the other channel, where it would wait to
    // be moved back before it served anything - at once; one whose keeper
    // something held away from its stand past the four hours (a person's
    // party, a dungeon, the tower) at the four hours; and one whose age its
    // keeper here does not know, once the keeper has had
    // PLAYERBOT_OFFLINE_SLIP_KEEPER_LEAD_MS since this core first saw it. The
    // price goes through the db core's own edit, the packet a keeper's edit at
    // its board ends in, and nothing is done while the keeper is at its board
    // or has a request of its own under way. A person's stand is left alone.
    void CorrectPlayerBotStandingSlips(DWORD now) {
        using namespace playerbot_offline;
        {
            std::map<DWORD, DWORD> seen;
            for (const auto& slip : s_vecPlayerBotStandingSlips) {
                const auto was = s_mapPlayerBotSlipSeenAt.find(slip.item);
                seen[slip.item] = was != s_mapPlayerBotSlipSeenAt.end() ? was->second : now;
            }
            s_mapPlayerBotSlipSeenAt.swap(seen);
        }
        if (!db_clientdesc || !db_clientdesc->IsPhase(PHASE_DBCLIENT)) return;
        auto& manager = ikashop::GetManager();
        int corrected = 0;
        for (const auto& slip : s_vecPlayerBotStandingSlips) {
            if (corrected >= PLAYERBOT_OFFLINE_SLIP_CORE_FIXES) break;
            if (!CPlayerBotManager::instance().IsRegisteredBotPID(slip.owner) || requests.count(slip.owner)) continue;
            auto shop = manager.GetShopByOwnerID(slip.owner);
            if (!shop || shop->GetDuration() == 0 || shop->IsEditMode() ||
                    shop->GetSpawn().channel != g_bChannel || !IsPlayerBotMapHostedHere(shop->GetSpawn().map))
                continue;
            auto line = shop->GetItem(slip.item);
            if (!line) continue;
            auto keeper = s_mapPlayerBotAIStates.find(slip.owner);
            const char* why = "keeper_away";
            if (keeper != s_mapPlayerBotAIStates.end()) {
                const auto& listed = keeper->second.offlineShop.listed;
                const auto known = listed.find(slip.item);
                const uint32_t slippedAt = known != listed.end() ? known->second.slippedAt : 0;
                if (slippedAt ? !playerbot_stall_rules::SlipDue(slippedAt, now, 0)
                        : !Due(now, s_mapPlayerBotSlipSeenAt[slip.item] + PLAYERBOT_OFFLINE_SLIP_KEEPER_LEAD_MS))
                    continue;
                why = slippedAt ? "keeper_held" : "keeper_late";
            }
            LPITEM preview = BotOfflinePreview(*line);
            if (!preview) continue;
            const long long price = (long long)line->GetPrice().GetTotalYangAmount();
            const bool standing = IsPlayerBotStandingPriceSlip(preview, price);
            const long long normal = std::max<long long>(GetPlayerBotShopAskingPrice(preview),
                    GetPlayerBotRefineInvestment(preview));
            const DWORD vnum = preview->GetVnum();
            M2_DELETE(preview);
            if (!standing || normal <= 0 || normal >= price || normal >= GOLD_MAX) continue;
            if (!BotOfflineBudget(now)) break;
            ikashop::TPriceInfo meant{};
            meant.yang = normal;
            manager.SendShopEditItemDBPacket(slip.owner, slip.item, meant);
            if (keeper != s_mapPlayerBotAIStates.end()) {
                auto known = keeper->second.offlineShop.listed.find(slip.item);
                if (known != keeper->second.offlineShop.listed.end()) known->second.slippedAt = 0;
            }
            ++corrected;
            sys_log(0, "PLAYERBOT_OFFLINE: price slip put right by the core pid=%u name=%s item=%u vnum=%u from=%lld to=%lld why=%s",
                slip.owner, shop->GetOwnerName(), slip.item, vnum, price, normal, why);
            // The line's row (playerbot_explain.h): how long this core saw it
            // stand, and why the core and not its keeper.
            if (IsPlayerBotExplainOn()) {
                const auto seenAt = s_mapPlayerBotSlipSeenAt.find(slip.item);
                const long long minutes = seenAt != s_mapPlayerBotSlipSeenAt.end() ? (long long)((now - seenAt->second) / 60000U) : 0;
                const int by = strcmp(why, "keeper_held") == 0 ? per::SLIP_BY_CORE_HELD
                        : (strcmp(why, "keeper_late") == 0 ? per::SLIP_BY_CORE_LATE : per::SLIP_BY_CORE_AWAY);
                std::vector<per::TPair> steps(1, per::Pair(per::STEP_SLIP_PUT_RIGHT, normal, price, minutes, by));
                QueuePlayerBotListingEvent(slip.item, slip.owner, vnum, line->GetInfo().count, per::EVENT_SLIP_FIX_CORE,
                    normal, price, per::EncodePairs(steps, per::STEPS_COLUMN), 0, 0,
                    per::LFLAG_SLIP);
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
