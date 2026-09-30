#ifndef __INC_METIN2_PLAYERBOT_EXPLAIN_LATE_H__
#define __INC_METIN2_PLAYERBOT_EXPLAIN_LATE_H__

// The parts of the bots' explanations (playerbot_explain.h) that read the
// whole AI - the goods rule, the roles a piece has for its bot, the equipment
// score term by term - and so come after all of it in playerbot_manager.cpp.
// Declared at the end of playerbot_explain.h; the same anonymous namespace.

namespace
{
	// A line a bot would rather wear soon than sell: at most this many levels
	// ahead of the bot.
	const int PLAYERBOT_EXPLAIN_WEAR_SOON_LEVELS = 5;

	// Which of ScorePlayerBotShopStock's branches makes this item goods, with
	// its parameters, and the score it gives. Asked once for a line that goes
	// up, not for every candidate of the bag.
	bool ExplainPlayerBotGoods(LPCHARACTER ch, LPITEM item, bool merchant, TPlayerBotGoodsWhy& why, int& score)
	{
		why = TPlayerBotGoodsWhy();
		score = 0;
		if (!ch || !item || !IsPlayerBotExplainOn())
			return false;
		TPlayerBotGoodsWhy* previous = s_pPlayerBotGoodsWhy;
		s_pPlayerBotGoodsWhy = &why;
		score = ScorePlayerBotShopStock(ch, item, merchant, false);
		s_pPlayerBotGoodsWhy = previous;
		if (score <= 0)
			why = TPlayerBotGoodsWhy();
		return why.set;
	}

	// What a piece is to its bot (ROLE_*): the level-30 weapons and the
	// projects, the Stalki, the spares and backups, the prizes, the listed
	// jewels, the stones in it, a gamble's pieces, the owner's pin, the banned
	// low weapon, the scroll-only weapon.
	unsigned int GetPlayerBotExplainRoles(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return 0;
		unsigned int roles = 0;
		if (IsPlayerBotSpecialLevel30Weapon(item))
			roles |= per::ROLE_LEVEL30_FAMILY;
		if (IsPlayerBotClassLevel30Weapon(ch, item))
			roles |= per::ROLE_CLASS_LEVEL30;
		if (IsPlayerBotLevel30Project(ch, item))
			roles |= per::ROLE_LEVEL30_PROJECT;
		if (item->GetType() == ITEM_WEAPON && item == FindPlayerBotLinesProject(ch))
			roles |= per::ROLE_LINES_PROJECT;
		if (IsPlayerBotStalkiItem(item))
		{
			roles |= per::ROLE_STALKI;
			if (IsPlayerBotKeptStalki(ch, item))
				roles |= per::ROLE_STALKI_KEPT;
		}
		if (!item->IsEquipped() && IsPlayerBotHigherTierSpare(ch, item))
			roles |= per::ROLE_HIGHER_TIER_SPARE;
		if (IsPlayerBotKeptBackupWeapon(ch, item) ||
				(item->GetType() == ITEM_ARMOR && item->GetID() != 0 && GetPlayerBotBackupArmourID(ch, false) == item->GetID()))
			roles |= per::ROLE_BACKUP;
		if (IsPlayerBotScrollRulePiece(ch, item))
			roles |= per::ROLE_SCROLL_RULE_PIECE;
		if (IsPlayerBotPrizeItem(item))
			roles |= per::ROLE_PRIZE;
		if (IsPlayerBotListedJewel(ch, item))
			roles |= per::ROLE_LISTED_JEWEL;
		for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
		{
			const DWORD inSocket = (DWORD)item->GetSocket(socket);
			if (inSocket > 2 && inSocket != PLAYERBOT_BROKEN_SOUL_STONE_VNUM && IsPlayerBotSoulStoneVnum(inSocket))
			{
				roles |= per::ROLE_SOUL_STONES;
				break;
			}
		}
		if (IsPlayerBotGambleForSale(ch, item) || IsPlayerBotRareGambleHeldBase(ch, item))
			roles |= per::ROLE_GAMBLE_SET;
		if (IsPlayerBotSidekickPinned(ch, item))
			roles |= per::ROLE_OWNER_PINNED;
		if (item->GetType() == ITEM_WEAPON && IsPlayerBotBannedLowWeapon(ch, item))
			roles |= per::ROLE_BANNED_LOW_WEAPON;
		if (IsPlayerBotScrollOnlyWeapon(item))
			roles |= per::ROLE_SCROLL_ONLY_WEAPON;
		return roles;
	}

	// A piece's rolled lines and the stones seated in it (per::EncodeLines).
	std::string EncodePlayerBotPieceLines(LPITEM item)
	{
		std::vector<std::pair<int, long long> > lines;
		std::vector<unsigned int> stones;
		if (!item)
			return std::string();
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			if (item->GetAttributeType(i) != 0 && item->GetAttributeValue(i) != 0)
				lines.push_back(std::make_pair((int)item->GetAttributeType(i), (long long)item->GetAttributeValue(i)));
		if (item->GetType() == ITEM_WEAPON || item->GetType() == ITEM_ARMOR)
			for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
			{
				const DWORD inSocket = (DWORD)item->GetSocket(socket);
				if (inSocket > 2 && inSocket != PLAYERBOT_BROKEN_SOUL_STONE_VNUM && IsPlayerBotSoulStoneVnum(inSocket))
					stones.push_back(inSocket);
			}
		return per::EncodeLines(lines, stones, per::LINES_COLUMN);
	}

	void FillPlayerBotEquipPiece(TPlayerBotEquipPiece& piece, LPCHARACTER ch, LPITEM item)
	{
		piece = TPlayerBotEquipPiece();
		if (!item)
			return;
		TPlayerBotScoreTerms terms;
		piece.id = item->GetID();
		piece.vnum = item->GetVnum();
		piece.plus = item->GetRefineLevel();
		piece.score = GetPlayerBotEquipmentScoreTerms(item, ch, &terms);
		piece.linesTiered = GetPlayerBotItemLineScore(item, ch);
		terms.Set(per::TERM_LINES_TIERED, piece.linesTiered);
		piece.roles = GetPlayerBotExplainRoles(ch, item);
		piece.lines = EncodePlayerBotPieceLines(item);
		piece.terms = terms.Encode();
		piece.levelLimit = item->GetLevelLimit();
	}

	// An equipment decision, read before anything moves: the two pieces with
	// their scores, terms, roles and lines, where the new one came from, the
	// moment it came at, and the flags its numbers raise. Queued by the caller
	// once the decision has happened (QueuePlayerBotEquip).
	void PreparePlayerBotEquipExplain(TPlayerBotEquipExplain& row, LPCHARACTER ch, int wear, int path, int rule,
			LPITEM newItem, LPITEM oldItem, bool oldZeroed)
	{
		row = TPlayerBotEquipExplain();
		if (!ch || !IsPlayerBotExplainOn() || (!newItem && !oldItem))
			return;
		const DWORD now = get_dword_time();
		row.pid = ch->GetPlayerID();
		row.level = ch->GetLevel();
		row.wear = wear;
		row.path = path;
		row.rule = rule;
		FillPlayerBotEquipPiece(row.newPiece, ch, newItem);
		FillPlayerBotEquipPiece(row.oldPiece, ch, oldItem);
		// The score the pass compared against: nothing, when a rule set it so.
		if (oldItem && oldZeroed)
		{
			row.oldPiece.score = 0;
			row.flags |= per::EFLAG_OLD_ZEROED;
		}
		if (newItem)
			GetPlayerBotExplainOrigin(newItem->GetID(), row.newOrigin, row.newOriginRef);

		// The moment.
		TPlayerBotExplainBot& bot = GetPlayerBotExplainBot(row.pid);
		const TPlayerBotAIState* state = NULL;
		TPlayerBotAIStateMap::const_iterator st = s_mapPlayerBotAIStates.find(row.pid);
		if (st != s_mapPlayerBotAIStates.end())
			state = &st->second;
		bool onCounter = false;
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		if (!oldItem && wear >= 0 && wear < WEAR_MAX_NUM && bot.wornId[wear] != 0 &&
				(!newItem || newItem->GetID() != bot.wornId[wear]))
		{
			if (state && state->offlineShop.listed.count(bot.wornId[wear]))
				onCounter = true;
			else if (auto shop = ikashop::GetManager().GetShopByOwnerID(row.pid))
				onCounter = shop->GetItems().find(bot.wornId[wear]) != shop->GetItems().end();
		}
		const bool reclaimed = state && newItem && state->offlineShop.lastReclaimItem == newItem->GetID() &&
				now - state->offlineShop.lastReclaimAt < per::AFTER_RECLAIM_SECONDS * 1000U;
#else
		const bool reclaimed = false;
#endif
		if (wear >= 0 && wear < WEAR_MAX_NUM && bot.burnAt[wear] != 0 &&
				now - bot.burnAt[wear] < per::AFTER_BURN_SECONDS * 1000U)
			row.context = per::CONTEXT_AFTER_BURN;
		else if (onCounter)
			row.context = per::CONTEXT_WENT_TO_COUNTER;
		else if (reclaimed || row.newOrigin == per::ORIGIN_RECLAIMED)
			row.context = per::CONTEXT_RETURNED_FROM_COUNTER;
		else if (bot.blacksmithAt != 0 && now - bot.blacksmithAt < per::AFTER_BLACKSMITH_SECONDS * 1000U)
			row.context = per::CONTEXT_AFTER_BLACKSMITH;
		else if (state && state->dwSpawnTime != 0 && now - state->dwSpawnTime <= per::AFTER_SPAWN_SECONDS * 1000U)
			row.context = per::CONTEXT_AFTER_SPAWN;
		if (row.context == per::CONTEXT_AFTER_BURN)
			row.flags |= per::EFLAG_AFTER_BURN;

		// What the numbers say - of two pieces of one kind the pass weighed
		// against each other: a rod or a pickaxe in the weapon slot, or a piece
		// a rule set to nothing, was never in the running.
		if (newItem && oldItem && !oldZeroed && newItem->GetType() == oldItem->GetType())
		{
			if (row.oldPiece.plus - row.newPiece.plus >= 2)
				row.flags |= per::EFLAG_PLUS_DOWN;
			if (row.newPiece.levelLimit < row.oldPiece.levelLimit)
				row.flags |= per::EFLAG_LEVEL_DOWN;
			if (row.oldPiece.linesTiered > 0 && row.newPiece.linesTiered < row.oldPiece.linesTiered)
				row.flags |= per::EFLAG_LINES_WORSE;
			if (rule == per::RULE_SCORE_UPGRADE && per::IsSmallGain(row.oldPiece.score, row.newPiece.score))
				row.flags |= per::EFLAG_SMALL_GAIN;
			if (row.oldPiece.roles & per::ROLE_PROJECT_OR_PRIZE)
				row.flags |= per::EFLAG_PROJECT_REPLACED;
		}
		if (wear >= 0 && wear < WEAR_MAX_NUM && newItem)
			bot.wornId[wear] = newItem->GetID();
		row.ready = true;
	}

	// LFLAG_BETTER_THAN_WORN_SOON for a line going up: a piece of the bot's
	// class it could wear within PLAYERBOT_EXPLAIN_WEAR_SOON_LEVELS levels that
	// scores over what it wears in that slot (or goes to an empty one).
	unsigned int GetPlayerBotListingWornFlags(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !IsPlayerBotExplainOn() ||
				(item->GetType() != ITEM_WEAPON && item->GetType() != ITEM_ARMOR) ||
				!item->CanUsedBy(ch) || (item->GetType() == ITEM_WEAPON && !IsPlayerBotWeapon(ch, item)) ||
				(int)item->GetLevelLimit() > (int)ch->GetLevel() + PLAYERBOT_EXPLAIN_WEAR_SOON_LEVELS)
			return 0;
		const int wearCell = item->FindEquipCell(ch);
		if (wearCell < 0 || wearCell >= WEAR_MAX_NUM)
			return 0;
		LPITEM worn = ch->GetWear((WORD)wearCell);
		if (worn && worn->GetType() != item->GetType())
			return 0;
		const long long score = GetPlayerBotEquipmentScore(item, ch);
		return (!worn || score > GetPlayerBotEquipmentScore(worn, ch)) ? per::LFLAG_BETTER_THAN_WORN_SOON : 0;
	}
}

#endif
