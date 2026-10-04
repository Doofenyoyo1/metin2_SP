#ifndef __INC_METIN2_PLAYERBOT_CONSUMABLES_H__
#define __INC_METIN2_PLAYERBOT_CONSUMABLES_H__

// The Moonlight Treasure Chest, and the boosters that come out of it.
//
// A chest is an item the engine opens itself (ITEM_USE on r40250, ITEM_GIFTBOX
// on mt2009) - UseItem on 50011 draws one line of the chest's special item
// group into the bag - so opening one is a matter of noticing it is there. What
// it holds the bot already knows how to spend: the change and add stones go
// through playerbot_bonus.h onto its own gear - a bot spends only the stones
// it holds, and the chest is where most of them come from, so every bot but a
// dropper opens every chest it gets (IsPlayerBotMoonlightChestSeller); the
// speed potions through UseUtilityPotions; the big potions through the
// ordinary potion lists. The two boosters, Hand of the Critic and Hand of
// Penetration, are new: a twenty-percent chance for ten minutes, worth
// drinking when a fight starts and pointless at an NPC.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_gear.h.

namespace
{
	// Boxes the engine will not open, by vnum and until when.
	//
	// "Skrzynia Eksperta III" and "Skrzynia Mistrza I" (50192, 50193) are
	// giftboxes a bot cannot use, and it asked anyway - close to six thousand
	// refusals a minute between them. Worse, a refusal ended the whole pass, so
	// every Moonlight chest sitting behind one of these in the bag was never
	// reached: that is how 587 bots came to be holding nine thousand of them.
	//
	// Keyed by bot AND vnum since 2.0.17. Keyed by vnum alone it was one map
	// for the whole population, and a refusal is mostly transient - the bag
	// had no room for the group at that moment - so the first bot in the tick
	// with a full bag switched the Moonlight chest off for everyone for ten
	// minutes, and with two thousand bots there always was one: a player's
	// table showed 165 663 unopened chests in the bots' bags (uxietoszef,
	// 12 September). What 50192 and 50193 actually were is a level limit
	// (Skrzynia Eksperta III at fifty, Skrzynia Mistrza I at sixty), and that
	// is asked before UseItem now, so no refusal has to be remembered for it.
	//
	// The passes that open a starter-chain chest by themselves - the
	// progression pass and the weapon recovery in playerbot_gear.h - ask and
	// add to the same memory through the two functions below, declared there.
	std::map<std::pair<DWORD, DWORD>, DWORD> s_mapPlayerBotChestRefused;

	bool IsPlayerBotChestRefused(DWORD dwPlayerID, DWORD dwVnum, DWORD dwNow)
	{
		std::map<std::pair<DWORD, DWORD>, DWORD>::const_iterator refused =
				s_mapPlayerBotChestRefused.find(std::make_pair(dwPlayerID, dwVnum));
		return refused != s_mapPlayerBotChestRefused.end() && dwNow < refused->second;
	}

	void NotePlayerBotChestRefused(DWORD dwPlayerID, DWORD dwVnum, DWORD dwNow)
	{
		s_mapPlayerBotChestRefused[std::make_pair(dwPlayerID, dwVnum)] = dwNow +
				(dwVnum == PLAYERBOT_MOONLIGHT_CHEST_VNUM ? PLAYERBOT_CHEST_MOONLIGHT_REFUSED_RETRY
					: PLAYERBOT_CHEST_REFUSED_RETRY);
	}

	// A box this bot has not grown into: the engine's own LIMIT_LEVEL on the
	// giftbox, which UseItem would refuse with a chat line nobody reads.
	bool IsPlayerBotChestLevelLocked(LPCHARACTER ch, LPITEM item)
	{
		return ch && item && item->GetLevelLimit() > ch->GetLevel();
	}

	// Whether this bot keeps Moonlight chests for its counter rather than
	// opening them: a dropper, whose counter is its trade, and nobody else
	// since blipu's report of 28 September - the resource trader opens its
	// chests like everybody (playerbot_moonlight_rules::KeepsChestClosed). The
	// one question for the chest pass, the counter's scorer and its take-home,
	// the hoard and the buyer, so none of them can hold a chest another opens.
	bool IsPlayerBotMoonlightChestSeller(DWORD dwPID)
	{
		return IsPlayerBotDropper(GetPlayerBotPersonalityByPID(dwPID));
	}

	// MT2009_PLUS_BOT_MINIGAMES_V1 (playerbot_minigames.h, later in the unit):
	// a mini game's chest, and how many of this bot's are kept for its counter.
	bool IsPlayerBotMinigameChestVnum(DWORD dwVnum);
	int GetPlayerBotMinigameChestsHeld(LPCHARACTER ch, DWORD dwVnum);
	void NotePlayerBotMinigameChestOpened(LPCHARACTER ch, DWORD dwVnum);

	// The bonus items in the bag, in units (IsPlayerBotBonusStoneItem, defined
	// with the economy further down the include order): what a chest has just
	// handed out is the count after its use against the count before.
	bool IsPlayerBotBonusStoneItem(LPITEM item);

	int CountPlayerBotBonusItemUnits(LPCHARACTER ch)
	{
		int units = 0;
		for (WORD cell = 0; ch && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetCell() == cell && IsPlayerBotBonusStoneItem(item))
				units += std::max<int>(1, (int)item->GetCount());
		}
		return units;
	}

	// Opens one chest per pass. UseItem refuses when the bag has no room, and
	// says so in the engine's own log; the bot's next town visit makes room.
	// A box that belongs on a counter rather than in the bot's own hands.
	//
	// Two kinds qualify. One this bot cannot open - a level-locked giftbox, or
	// one the engine refused it - which is goods to whoever holds it. And the
	// surplus of a stack big enough that selling it costs the bot nothing: the
	// chest pass keeps eating the stack meanwhile, so most of what drops is
	// still opened and only what piles up is sold. A stack goes up whole
	// because a private shop line is a whole stack; splitting one is its own
	// change and not this one.
	bool IsPlayerBotSurplusChest(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || (item->GetVnum() != PLAYERBOT_MOONLIGHT_CHEST_VNUM &&
				item->GetType() != ITEM_GIFTBOX))
			return false;
		// A Cor Draconis is counter goods of its own (ScorePlayerBotShopStock),
		// never a box.
		if (GetPlayerBotRareGoodsKind(item->GetVnum()) != PLAYERBOT_RARE_GOODS_NONE)
			return false;
		// MT2009_PLUS_BOT_MINIGAMES_V1: a mini game's chest is goods only on the
		// counter of the bot that kept it for one (its share of the 30%); every
		// other is opened, whatever the stack.
		if (IsPlayerBotMinigameChestVnum(item->GetVnum()))
			return GetPlayerBotMinigameChestsHeld(ch, item->GetVnum()) > 0;
		if (IsPlayerBotChestLevelLocked(ch, item))
			return true;
		// A dropper's Moonlight chests are all goods: it keeps them for the
		// counter (ManagePlayerBotChests), so a stack of one is a line.
		// Everybody else's are opened and never listed: a stack of five went up
		// whole, a line nobody's cap reached stood unsold, and the bot that
		// listed it had nothing left to open ("za malo z nich je otwiera, wiecej
		// wystawiaja na sklepy", AkhiGubernator, 15 September). The resource
		// trader's were goods too until blipu's report of 28 September: a player
		// bought them up at a fifth of what they hold and bonused a whole set.
		if (item->GetVnum() == PLAYERBOT_MOONLIGHT_CHEST_VNUM)
			return IsPlayerBotMoonlightChestSeller(ch->GetPlayerID());
		// A box the engine refused this bot stays goods; the retry clock is not
		// consulted here - it exists to stop the asking, not to make the box
		// valuable again.
		// Not a Moonlight chest, though: one refusal put the bot's whole stack on
		// its counter for the life of the core, long after the retry clock had
		// let it open them again (PLAYERBOT_CHEST_MOONLIGHT_REFUSED_RETRY).
		if (item->GetVnum() != PLAYERBOT_MOONLIGHT_CHEST_VNUM &&
				s_mapPlayerBotChestRefused.find(std::make_pair(ch->GetPlayerID(), item->GetVnum())) !=
				s_mapPlayerBotChestRefused.end())
			return true;
		// A boss's casket is opened, whatever the stack: the bot that broke the
		// boss is the one it is for, and a trader's two of them went up as a
		// line the chest pass would have opened a few seconds later.
		if (IsPlayerBotBossCasketVnum(item->GetVnum()))
			return false;
		// A trader puts a box up from a much smaller stack, so unopened chests
		// reach the market without the population stopping opening them: the
		// chest pass keeps eating the stack either way.
		const DWORD minStack = IsPlayerBotResourceTrader(ch->GetPlayerID())
				? PLAYERBOT_CHEST_TRADER_MIN_STACK : PLAYERBOT_CHEST_STALL_MIN_STACK;
		return item->GetCount() >= minStack;
	}

	// Ile pol plecaka jest naprawde puste.
	//
	// GetEmptyInventory(height) odpowiada na inne pytanie - "gdzie zmiesci sie
	// jeden przedmiot tej wysokosci" - i nie da sie z niego zbudowac rezerwacji
	// na kilka nagrod naraz.
	//
	// Asked of the item grid, not of the item pointers: SetItem puts the
	// pointer in the top cell only and marks bItemGrid for every cell the
	// piece covers, so a weapon of three cells looked like one occupied and
	// two free from here. Every rule on this count was off by the height of
	// the gear in the bag - and the stall pass looped on it: the split kept
	// "three free cells" that were the bottoms of swords, the bundle had no
	// cell, the merge freed one, the split took it again, every three
	// seconds (6066 splits and 4054 merges in a quarter of an hour on one
	// core; sizowski, 12 September: "stan chunjo m1: 2 sklepy").
	int CountPlayerBotFreeInventoryCells(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		int free = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			if (ch->IsEmptyItemGrid(TItemPos(INVENTORY, cell), 1))
				++free;
		return free;
	}

	// The free cells of the saddlebag page (playerbot_saddlebag.h) while it is
	// open: the rows unlocked, with the horse out or ridden. Only the bag's
	// fullness asks it (IsPlayerBotBagFull, IsPlayerBotBagUnderPressure, the
	// loot): the engine puts a drop there once the four pages are full, and
	// the saddlebag pass moves it back down as a cell frees - every other rule
	// counts the four pages it can see.
	int CountPlayerBotSaddlebagFreeCells(LPCHARACTER ch)
	{
		if (!ch || !ch->CanUseHorseInventory() || ch->GetHorseInventoryUnlock() == 0)
			return 0;
		const int end = std::min<int>(INVENTORY_MAX_NUM,
				INVENTORY_DEFAULT_MAX_NUM + INVENTORY_PAGE_COLUMN * ch->GetHorseInventoryUnlock());
		int free = 0;
		for (int cell = INVENTORY_DEFAULT_MAX_NUM; cell < end; ++cell)
			if (ch->IsEmptyItemGrid(TItemPos(INVENTORY, cell), 1))
				++free;
		return free;
	}

	// A giftbox opens only into a free column of three - UseItemEx asks
	// GetEmptyInventory(3) - and a bag of seventy to ninety cells has free cells
	// without one: the bots holding the most Moonlight chests on the test world
	// had 69 to 90 of 90 cells taken, and their chests stood unopened for good
	// (15 September). This moves at most two single-cell items out of the column
	// nearest to free into cells outside it, with MoveItem, which only moves: the
	// worst case is a column that stays shut.
	bool FreePlayerBotGiftboxColumn(LPCHARACTER ch)
	{
		if (!ch || !ch->CanHandleItem() || ch->GetExchange() || ch->GetMyShop() ||
				CountPlayerBotFreeInventoryCells(ch) < 3)
			return false;
		const int columns = PLAYERBOT_BAG_PAGE_COLUMNS;
		const int pageSize = PLAYERBOT_BAG_PAGE_COLUMNS * PLAYERBOT_BAG_PAGE_ROWS;
		int bestTop = -1;
		int bestBlockers = 3;
		for (int top = 0; top + 2 * columns < PLAYERBOT_BAG_CELLS; ++top)
		{
			if ((top % pageSize) / columns > PLAYERBOT_BAG_PAGE_ROWS - 3)
				continue;
			int blockers = 0;
			bool movable = true;
			for (int k = 0; k < 3 && movable; ++k)
			{
				const WORD cell = (WORD)(top + k * columns);
				if (ch->IsEmptyItemGrid(TItemPos(INVENTORY, cell), 1))
					continue;
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetCell() != cell || item->GetSize() != 1 || item->IsEquipped() ||
						item->isLocked() || item->IsExchanging())
					movable = false;
				else
					++blockers;
			}
			if (movable && blockers < bestBlockers)
			{
				bestTop = top;
				bestBlockers = blockers;
			}
		}
		if (bestTop < 0)
			return false;
		int moved = 0;
		for (int k = 0; k < 3; ++k)
		{
			const WORD cell = (WORD)(bestTop + k * columns);
			if (ch->IsEmptyItemGrid(TItemPos(INVENTORY, cell), 1))
				continue;
			int to = -1;
			for (int dest = PLAYERBOT_BAG_CELLS - 1; dest >= 0 && to < 0; --dest)
			{
				const bool inColumn = dest >= bestTop && dest <= bestTop + 2 * columns &&
						(dest - bestTop) % columns == 0;
				if (!inColumn && ch->IsEmptyItemGrid(TItemPos(INVENTORY, (WORD)dest), 1))
					to = dest;
			}
			if (to < 0 || !ch->MoveItem(TItemPos(INVENTORY, cell), TItemPos(INVENTORY, (WORD)to), 0))
				return false;
			++moved;
		}
		if (moved > 0)
			PlayerBotLogThrottled("chest_column", get_dword_time(),
					"PLAYERBOT_CHEST: freed a column pid=%u name=%s moved=%d free=%d",
					ch->GetPlayerID(), ch->GetName(), moved, CountPlayerBotFreeInventoryCells(ch));
		return ch->GetEmptyInventory(3) >= 0;
	}

	bool ManagePlayerBotChests(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || dwNow < state.dwNextChestTime)
			return false;
		state.dwNextChestTime = dwNow + PLAYERBOT_CHEST_INTERVAL;
		// A companion whose owner keeps its chests closed ("Skrzynki: nie").
		if (IsPlayerBotSidekickKeepingChests(ch))
			return false;
		// A treasure chest (the silver and gold ones) opens with a key, not by
		// itself: the engine's path is "use the key on the chest", which removes
		// both and hands out the chest's group. Any key whose lock value matches.
		for (WORD boxCell = 0; boxCell < PLAYERBOT_BAG_CELLS; ++boxCell)
		{
			LPITEM box = ch->GetInventoryItem(boxCell);
			if (!box || box->GetType() != ITEM_TREASURE_BOX || box->isLocked() ||
					IsPlayerBotSidekickHeld(ch, box) ||	// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1
					GetPlayerBotRareGoodsKind(box->GetVnum()) != PLAYERBOT_RARE_GOODS_NONE ||
					IsPlayerBotChestRefused(ch->GetPlayerID(), box->GetVnum(), dwNow))
				continue;
			for (WORD keyCell = 0; keyCell < PLAYERBOT_BAG_CELLS; ++keyCell)
			{
				LPITEM key = ch->GetInventoryItem(keyCell);
				if (!key || key->GetType() != ITEM_TREASURE_KEY || key->GetValue(0) != box->GetValue(0) ||
						key->isLocked() || IsPlayerBotSidekickHeld(ch, key))	// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1
					continue;
				// Miejsce na caly zestaw, a nie na jeden przedmiot: patrz
				// PLAYERBOT_CHEST_FREE_CELLS. Wysokie przedmioty potrzebuja
				// dodatkowo ciaglych trzech pol w jednej kolumnie, o co
				// GetEmptyInventory(3) pyta wprost.
				// The whole set or nothing (PlayerBotBagTakesGroup, playerbot_gear.h):
				// the key's use hands out the box's own group. A box whose set
				// does not fit waits and the pass goes on: it used to end here,
				// and every boss casket behind it waited with it.
				int cellsNeeded = 0;
				if (!PlayerBotBagTakesGroup(ch, box->GetVnum(), cellsNeeded))
					break;
				const DWORD boxVnum = box->GetVnum(), keyVnum = key->GetVnum();
				const int before = ch->GetEmptyInventory(1);
				if (ch->UseItem(TItemPos(INVENTORY, keyCell), TItemPos(INVENTORY, boxCell)))
				{
					sys_log(0, "PLAYERBOT_CHEST: treasure pid=%u name=%s box=%u key=%u free_before=%d free_after=%d",
							ch->GetPlayerID(), ch->GetName(), boxVnum, keyVnum, before, ch->GetEmptyInventory(1));
					return true;
				}
				// Remembered like a giftbox's refusal, or the pass asked the
				// same box every eight seconds for as long as it had the key.
				NotePlayerBotChestRefused(ch->GetPlayerID(), boxVnum, dwNow);
				PlayerBotLogThrottled("chest_refused", dwNow,
						"PLAYERBOT_CHEST: treasure refused pid=%u name=%s box=%u key=%u free=%d",
						ch->GetPlayerID(), ch->GetName(), boxVnum, keyVnum, ch->GetEmptyInventory(1));
				break;
			}
		}
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			// The Moonlight chest, and every boss casket (ITEM_GIFTBOX: the Orc
			// Chief's, the Spider Queen's) - the engine opens both the same way.
			if (!item || (item->GetVnum() != PLAYERBOT_MOONLIGHT_CHEST_VNUM &&
					item->GetType() != ITEM_GIFTBOX))
				continue;
			// A Cor Draconis is never opened (MT2009 Plus): it is a player's
			// goods, and goes on the counter or to the merchant.
			if (GetPlayerBotRareGoodsKind(item->GetVnum()) != PLAYERBOT_RARE_GOODS_NONE)
				continue;
			// A box already on this bot's own counter. UseItem refuses a locked
			// item, and that refusal is remembered by vnum for every bot in the
			// world - so opening one that is for sale would stop the whole
			// population opening that kind of box for the next few minutes.
			if (item->isLocked())
				continue;
			// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: the owner's, held for it.
			if (IsPlayerBotSidekickHeld(ch, item))
				continue;
			// MT2009_PLUS_BOT_MINIGAMES_V1: the mini game chests kept for the
			// counter stay shut; one past them is opened.
			if (IsPlayerBotMinigameChestVnum(item->GetVnum()) &&
					(int)ch->CountSpecifyItem(item->GetVnum()) <= GetPlayerBotMinigameChestsHeld(ch, item->GetVnum()))
				continue;
			// The apprentice chain is giftboxes too, and with the world's
			// apprentice chest off none of it is opened: the progression pass
			// (playerbot_gear.h) takes it out of the bag instead, and it runs
			// later in the tick than this one.
			if (IsPlayerBotApprenticeChestVnum(item->GetVnum()) && IsPlayerBotApprenticeChestOff())
				continue;
			// A dropper keeps its Moonlight chests for its counter, up to
			// PLAYERBOT_CHEST_DROPPER_HOLD, and opens what is past that. The
			// resource trader kept six for its counter as well (2.0.31, after
			// "nadal ... stan sklepow ze szkatami blasku: 0", sizowski) until
			// blipu's report of 28 September: its chests stood on the counters
			// at a fifth of what they hold, and a player bought a whole set's
			// bonuses out of them after half an hour of a chest window. It opens
			// every one now, and spends the stones on its own gear.
			if (item->GetVnum() == PLAYERBOT_MOONLIGHT_CHEST_VNUM &&
					playerbot_moonlight_rules::KeepsChestClosed(
						IsPlayerBotMoonlightChestSeller(ch->GetPlayerID()),
						(int)ch->CountSpecifyItem(PLAYERBOT_MOONLIGHT_CHEST_VNUM), PLAYERBOT_CHEST_DROPPER_HOLD))
				continue;
			// A box above the bot's level is not asked for: the engine would
			// refuse it, and remembering that refusal is what used to switch the
			// chest off for everybody.
			if (IsPlayerBotChestLevelLocked(ch, item))
				continue;
			if (IsPlayerBotChestRefused(ch->GetPlayerID(), item->GetVnum(), dwNow))
				continue;
			// The same test as for the treasure box: room for the whole set the
			// group can hand out, placed the way the engine places it. A box
			// whose set does not fit waits, and the next is asked: the
			// Moonlight chest's group is every line of it, a boss casket's one,
			// so a bag too full for the first still takes the second.
			int cellsNeeded = 0;
			if (!PlayerBotBagTakesGroup(ch, item->GetVnum(), cellsNeeded))
				continue;
			// The engine's own two refusals, asked first so neither is remembered
			// as the box's: a giftbox wants a free column of three (UseItemEx,
			// ITEM_GIFTBOX), and nothing is used with a window open (CanHandleItem -
			// the safebox of a town visit).
			if (ch->GetEmptyInventory(3) < 0 && !FreePlayerBotGiftboxColumn(ch))
				return false;
			if (ch->GetEmptyInventory(3) < 0 || !ch->CanHandleItem())
				return false;
			const int before = ch->GetEmptyInventory(1);
			const DWORD chestVnum = item->GetVnum();
			const DWORD chestCount = item->GetCount();
			// What the chest hands out goes straight into the bag, one line of
			// its group at a time, and nothing else names it: the bag's
			// valuables before and after are the Bot Mood System's news.
			const int valuablesBefore = IsPlayerBotPersonaEnabled() ? CountPlayerBotMoodValuables(ch) : 0;
			const int bonusBefore = CountPlayerBotBonusItemUnits(ch);
			// By the chest's own cell: FreePlayerBotGiftboxColumn may have moved
			// this very chest out of the column it made.
			if (ch->UseItem(TItemPos(INVENTORY, item->GetCell())))
			{
				if (IsPlayerBotPersonaEnabled())
					NotePlayerBotMoodValuableCount(ch,
							CountPlayerBotMoodValuables(ch) - valuablesBefore, "chest");
				// A change or an add stone out of the chest is for the bot's own
				// gear, so the bonus pass looks at it now rather than at the end
				// of its five minutes (blipu: "zeby boty faktycznie to
				// uzywaly") - only when one came out, or a bot opening a chest
				// every eight seconds would weigh its whole gear as often.
				NotePlayerBotMinigameChestOpened(ch, chestVnum); // MT2009_PLUS_BOT_MINIGAMES_V1
				const int bonusGained = CountPlayerBotBonusItemUnits(ch) - bonusBefore;
				if (bonusGained > 0 && state.dwNextBonusCheckTime > dwNow)
					state.dwNextBonusCheckTime = dwNow;
				sys_log(0, "PLAYERBOT_CHEST: opened pid=%u name=%s level=%u map=%ld vnum=%u boss=%d free_before=%d free_after=%d bonus=%d",
						ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), ch->GetMapIndex(), chestVnum,
						IsPlayerBotBossCasketVnum(chestVnum) ? 1 : 0, before, ch->GetEmptyInventory(1),
						bonusGained);
				return true;
			}
			// Not the end of the pass: the next box in the bag may well open,
			// and giving up here is what kept the Moonlight chests behind these
			// two out of reach. The refusal is remembered - for this bot and
			// this box - so the bot stops asking every eight seconds.
			NotePlayerBotChestRefused(ch->GetPlayerID(), chestVnum, dwNow);
			PlayerBotLogThrottled("chest_refused", dwNow,
					"PLAYERBOT_CHEST: refused pid=%u name=%s vnum=%u count=%u free=%d",
					ch->GetPlayerID(), ch->GetName(), chestVnum,
					(unsigned int)chestCount, ch->GetEmptyInventory(1));
			continue;
		}
		return false;
	}

	// A booster at the start of a fight. The engine keeps one of each running
	// at a time and refuses a second, so a failed use is the usual case and
	// nothing to log; a minute between attempts is enough.
	// A timed buff the way the engine sees one (see PLAYERBOT_USE_AFFECT_TIMED_BUFF).
	bool IsPlayerBotBoosterItem(LPITEM item)
	{
		if (!item || item->GetType() != ITEM_USE)
			return false;
		if (item->GetSubType() == USE_ABILITY_UP)
			return true;
		return item->GetSubType() == USE_AFFECT &&
				item->GetValue(0) == PLAYERBOT_USE_AFFECT_TIMED_BUFF;
	}

	// Rada Pustelnika (AFFECT_SKILL_BOOK_BONUS) and the Exorcism Scroll
	// (AFFECT_SKILL_NO_BOOK_DELAY): read by the book pass, never merchant scrap.
	// Most of them carry ANTI_SELL; the item shop's copies (71201, 71294) do
	// not, and the junk rule's default sold them.
	bool IsPlayerBotBookAffectItem(LPITEM item)
	{
		return item && item->GetType() == ITEM_USE && item->GetSubType() == USE_AFFECT &&
				((DWORD)item->GetValue(0) == AFFECT_SKILL_BOOK_BONUS ||
				 (DWORD)item->GetValue(0) == AFFECT_SKILL_NO_BOOK_DELAY);
	}

	// The recovery affect an auto potion keeps up, or zero for anything else.
	DWORD GetPlayerBotAutoPotionAffect(DWORD vnum)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_AUTO_HP_POTION_VNUMS) / sizeof(PLAYERBOT_AUTO_HP_POTION_VNUMS[0]); ++i)
			if (PLAYERBOT_AUTO_HP_POTION_VNUMS[i] == vnum)
				return AFFECT_AUTO_HP_RECOVERY;
		for (size_t i = 0; i < sizeof(PLAYERBOT_AUTO_SP_POTION_VNUMS) / sizeof(PLAYERBOT_AUTO_SP_POTION_VNUMS[0]); ++i)
			if (PLAYERBOT_AUTO_SP_POTION_VNUMS[i] == vnum)
				return AFFECT_AUTO_SP_RECOVERY;
		return 0;
	}

	// The engine's own test in the use path: nothing left to give.
	bool IsPlayerBotAutoPotionEmpty(LPITEM item)
	{
		return item && item->GetSocket(1) == item->GetSocket(2);
	}

	// Sztuka Combo and the three Leadership books: ITEM_USE, USE_SPECIAL, read
	// by char_item.cpp's own cases (50301-50306), so neither the skill-book
	// pass nor the junk rule knew them and the merchant got them for a
	// thousand yang ("Mistrz. Sztuka Combo" in a gear history, sizowski,
	// 16 September).
	bool IsPlayerBotGeneralSkillBook(DWORD vnum)
	{
		return vnum >= 50301 && vnum <= 50306;
	}

	DWORD GetPlayerBotGeneralSkillBookSkill(DWORD vnum)
	{
		return vnum >= 50304 ? PLAYERBOT_SKILL_COMBO_VNUM : PLAYERBOT_SKILL_LEADERSHIP_VNUM;
	}

	// The engine's own tests, from char_item.cpp: Combo 0 reads from level 30,
	// Combo 1 from 50, Combo 2 reads no more; a Leadership book covers twenty
	// levels of the skill (value0 to value1 of the proto: 0-20, 20-30, 30-40).
	bool CanPlayerBotReadGeneralSkillBookNow(LPCHARACTER ch, DWORD vnum)
	{
		if (!ch || !IsPlayerBotGeneralSkillBook(vnum))
			return false;
		if (vnum >= 50304)
		{
			const int combo = ch->GetSkillLevel(PLAYERBOT_SKILL_COMBO_VNUM);
			if (combo >= 2)
				return false;
			return (int)ch->GetLevel() >= (combo == 0 ? 30 : 50);
		}
		const int lead = ch->GetSkillLevel(PLAYERBOT_SKILL_LEADERSHIP_VNUM);
		if (vnum == 50301)
			return lead < 20;
		if (vnum == 50302)
			return lead >= 20 && lead < 30;
		return lead >= 30 && lead < 40;
	}

	// Worth keeping: readable now, or a Combo book a few levels ahead of the
	// level that reads it. A Leadership book for a range the skill has passed
	// or not reached is goods.
	bool IsPlayerBotGeneralSkillBookUseful(LPCHARACTER ch, DWORD vnum)
	{
		if (!ch || !IsPlayerBotGeneralSkillBook(vnum))
			return false;
		if (CanPlayerBotReadGeneralSkillBookNow(ch, vnum))
			return true;
		if (vnum >= 50304)
		{
			const int combo = ch->GetSkillLevel(PLAYERBOT_SKILL_COMBO_VNUM);
			if (combo >= 2)
				return false;
			return (int)ch->GetLevel() + PLAYERBOT_GENERAL_BOOK_LEVEL_AHEAD >= (combo == 0 ? 30 : 50);
		}
		return false;
	}

	// The polymorph books (50314-50316) and the Mining Guide (50600, the
	// engine's ITEM_MINING_SKILL_TRAIN_BOOK), read the way char_item.cpp
	// reads them. They were goods for the counter and nothing else, so no bot
	// ever raised either skill. The horse-taming book stays the players':
	// a bot has that skill at 10 from the horse training alone.
	bool IsPlayerBotExtraSkillBook(DWORD vnum)
	{
		return (vnum >= 50314 && vnum <= 50316) || vnum == 50600;
	}

	DWORD GetPlayerBotExtraSkillBookSkill(DWORD vnum)
	{
		return vnum == 50600 ? (DWORD)SKILL_MINING : (DWORD)SKILL_POLYMORPH;
	}

	// The engine's own tests: under 40 and the skill's own cap; a polymorph
	// book for its range of the skill (value0..value1) from its level
	// (value3).
	bool CanPlayerBotReadExtraSkillBookNow(LPCHARACTER ch, DWORD vnum)
	{
		if (!ch || !IsPlayerBotExtraSkillBook(vnum))
			return false;
		const DWORD skill = GetPlayerBotExtraSkillBookSkill(vnum);
		const int level = ch->GetSkillLevel(skill);
		const CSkillProto* sk = CSkillManager::instance().Get(skill);
		if (!sk || level >= 40 || level >= (int)sk->bMaxLevel)
			return false;
		if (vnum == 50600)
			return true;
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		return proto && (int)ch->GetLevel() >= proto->alValues[3] &&
				level >= proto->alValues[0] && level < proto->alValues[1];
	}

	bool IsPlayerBotMetinDetector(DWORD vnum)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_METIN_DETECTOR_VNUMS) / sizeof(PLAYERBOT_METIN_DETECTOR_VNUMS[0]); ++i)
			if (PLAYERBOT_METIN_DETECTOR_VNUMS[i] == vnum)
				return true;
		return false;
	}

	// An auto potion is switched on once and left alone: a use with its affect
	// already running would switch it off again, and a use of an empty one does
	// nothing but report success - see PLAYERBOT_AUTO_HP_POTION_VNUMS for what
	// treating these as elixirs to drink on every tick cost. The engine admits
	// one auto-potion use a second, so an SP potion found in the same pass as an
	// HP one simply waits for the next minute.
	bool ManagePlayerBotAutoPotions(LPCHARACTER ch, DWORD dwNow)
	{
		static std::map<DWORD, DWORD> s_mapPlayerBotAutoPotionNext;
		if (!ch || !ch->IsItemLoaded() || ch->IsDead() || ch->GetShop() || ch->GetExchange())
			return false;
		// Not in a duel: SwitchOffPlayerBotAutoPotionsForDuel takes them off for
		// it, and this pass puts them back on after it.
		if (playerbot_pvp::IsInDuel(ch->GetPlayerID(), dwNow))
			return false;
		DWORD& next = s_mapPlayerBotAutoPotionNext[ch->GetPlayerID()];
		if (dwNow < next)
			return false;
		next = dwNow + PLAYERBOT_AUTO_POTION_INTERVAL;
		bool used = false;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || IsPlayerBotSidekickHeld(ch, item))	// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1
				continue;
			const DWORD affect = GetPlayerBotAutoPotionAffect(item->GetVnum());
			if (affect == 0 || ch->FindAffect(affect) || IsPlayerBotAutoPotionEmpty(item))
				continue;
			const DWORD vnum = item->GetVnum();
			if (ch->UseItem(TItemPos(INVENTORY, cell)) && ch->FindAffect(affect))
			{
				sys_log(0, "PLAYERBOT_CHEST: auto potion on pid=%u name=%s vnum=%u affect=%u",
						ch->GetPlayerID(), ch->GetName(), vnum, (unsigned int)affect);
				used = true;
			}
		}
		return used;
	}

	// A duel is fought on the health the bot walks into it with. The potion pass
	// drinks nothing during one, but an auto potion switched on before the
	// challenge heals by itself inside the engine (AutoRecoveryItemProcess), and
	// a bot topping itself up mid-duel was what "boty w PvP uzywaja potki
	// czerwonej, moze maja wlaczona autopote?" was (15 September). The
	// engine's own switch is a second use of the item that runs the affect,
	// found by the item id the affect carries; ManagePlayerBotAutoPotions puts it
	// back on within a minute of the duel's end. One use a call: the engine
	// admits one auto-potion use a second.
	void SwitchOffPlayerBotAutoPotionsForDuel(LPCHARACTER ch, DWORD dwNow)
	{
		static std::map<DWORD, DWORD> s_mapPlayerBotDuelPotionNext;
		if (!ch || !ch->IsItemLoaded() || ch->IsDead())
			return;
		DWORD& next = s_mapPlayerBotDuelPotionNext[ch->GetPlayerID()];
		if (dwNow < next)
			return;
		next = dwNow + 1100;
		const DWORD affects[2] = { AFFECT_AUTO_HP_RECOVERY, AFFECT_AUTO_SP_RECOVERY };
		for (int i = 0; i < 2; ++i)
		{
			const CAffect* running = ch->FindAffect(affects[i]);
			if (!running)
				continue;
			for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
			{
				LPITEM item = ch->GetInventoryItem(cell);
				if (!item || item->GetID() != running->dwFlag)
					continue;
				const DWORD vnum = item->GetVnum();
				ch->UseItem(TItemPos(INVENTORY, cell));
				sys_log(0, "PLAYERBOT_PVP: auto potion off for the duel pid=%u name=%s vnum=%u affect=%u off=%d",
						ch->GetPlayerID(), ch->GetName(), vnum, (unsigned int)affects[i],
						ch->FindAffect(affects[i]) ? 0 : 1);
				return;
			}
		}
	}

	bool UsePlayerBotBoosters(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || dwNow < state.dwNextBoosterTime)
			return false;
		ManagePlayerBotAutoPotions(ch, dwNow);
		if (state.bCurrentAction != BOT_ACTION_FIGHT || state.bVisitingShop ||
				state.bRecoveringAfterDeath || state.bTacticalRetreat)
			return false;
		state.dwNextBoosterTime = dwNow + PLAYERBOT_BOOSTER_INTERVAL;
		bool used = false;
		// Anything the engine treats as a timed buff, one attempt per buff line
		// per pass: the engine itself refuses a second Mikstura Ataku while the
		// first runs ("This effect is already activated"), so a refusal is the
		// bot being told the buff is up, not an error. The vnum list is only the
		// order the chest boosters come in.
		std::set<long> triedLines;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || !IsPlayerBotBoosterItem(item) ||
					IsPlayerBotSidekickHeld(ch, item))	// MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1
				continue;
			const long line = item->GetSubType() * 1000L + item->GetValue(1);
			if (!triedLines.insert(line).second)
				continue;
			const DWORD vnum = item->GetVnum();
			if (ch->UseItem(TItemPos(INVENTORY, cell)))
			{
				sys_log(0, "PLAYERBOT_CHEST: booster pid=%u name=%s vnum=%u",
						ch->GetPlayerID(), ch->GetName(), vnum);
				used = true;
			}
		}
		return used;
	}
}

#endif
