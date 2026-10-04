#ifndef __INC_METIN2_PLAYERBOT_HORSE30_H__
#define __INC_METIN2_PLAYERBOT_HORSE30_H__

// MT2009_PLUS_HORSE30_V1: the horse to level 30, the bots' side.
//
// Autor systemu: Digi Rasta (nowy-system v0.16, karta-kon-30-i-juki.md);
// ported to MT2009 PLUS as our own code. The players' side is the quest
// konie.quest (paid training at the Stajenny, the Black Steed trial, the
// permanent horse bonus) and horse_inventory.quest (saddlebag rows to 30);
// the engine shows race 20119 from horse level 30 (server-patches/digirasta,
// char_horse.cpp). This file keeps the bots on the same numbers:
//
//   * training at the Stajenny instead of the old training missions (which a
//     bot never ran - it handed in one medal a level up to twenty): a level
//     costs medals, feed, Materialy Rzemieslnicze and yang exactly as
//     konie.koszt() asks - 1-9: 1 / 5 Siano / 5 / 100 000*(l+1); 11-19: 2 /
//     5 Marchewka / 10 / 2-10 mln; 21-28: 3 / 5 Czerwony Zen-szen / 20 /
//     15-50 mln. Feed the bag lacks is bought from the Stajenny's shop on the
//     spot, at its price. Level 0 -> 1 is pony_buy's one medal;
//   * 10 -> 11 and 20 -> 21 stay the bots' battle and military horse trials
//     (playerbot_battle_horse.h);
//   * 29 -> 30 is the Black Steed trial: fifty Setaou Archers (2412) in the
//     Grotto of Exile V2 (map 73). konie.quest gives a player thirty minutes;
//     a bot, as on its other trials, has no clock - but only a strong one
//     (PLAYERBOT_BLACK_STEED_MIN_LEVEL, the grotto's own band) takes it;
//   * the permanent bonus from horse level 21 (konie.bonus): the difference
//     against the quest's own flags konie.nm / konie.nb / konie.ns on
//     AFFECT_COLLECT, as konie.synchronizuj does - one bonus, whoever applies
//     it, a player's quest at login or this file after a level.
//
// Change a number here and in konie.quest together.
//
// An implementation fragment in the sense playerbot_types.h describes:
// include it once, after playerbot_battle_horse.h and playerbot_gear.h, and
// before playerbot_activities.h, which walks the bot to the stable with it.

namespace
{
	const DWORD PLAYERBOT_HORSE_FEED_HAY = 50054;      // Siano
	const DWORD PLAYERBOT_HORSE_FEED_CARROT = 50055;   // Marchewka
	const DWORD PLAYERBOT_HORSE_FEED_GINSENG = 50056;  // Czerwony Zen-szen
	const DWORD PLAYERBOT_HORSE_TRAINING_MATERIAL_VNUM = 30378;
	// The Stajenny's shop (shop 11) - read off the item's own price when the
	// proto is there, these when it is not.
	const DWORD PLAYERBOT_HORSE_FEED_PRICE_FALLBACK[3] = { 8000, 10000, 12000 };

	const BYTE PLAYERBOT_BLACK_STEED_FROM_HORSE_LEVEL = 29;
	const BYTE PLAYERBOT_BLACK_STEED_LEVEL = 30;
	// The quest asks 75; a bot goes where the Setaou Archers are only from the
	// Grotto V2's band (PLAYERBOT_GROTTO_V2_MIN_LEVEL) and one over it.
	const BYTE PLAYERBOT_BLACK_STEED_MIN_LEVEL = 85;
	const int PLAYERBOT_BLACK_STEED_KILLS = 50;
	const DWORD PLAYERBOT_BLACK_STEED_MOB = 2412;
	const char* PLAYERBOT_BLACK_STEED_KILLS_FLAG = "playerbot.black_steed_kills";

	// konie.bonus(): { monsters, bosses, metins } in percent, horse 21..30.
	const int PLAYERBOT_HORSE_BONUS[10][3] = {
		{ 1, 1, 1 }, { 2, 1, 1 }, { 2, 2, 2 }, { 2, 3, 3 }, { 3, 3, 3 },
		{ 4, 3, 3 }, { 4, 4, 4 }, { 5, 4, 4 }, { 5, 5, 4 }, { 5, 5, 5 },
	};
	const BYTE PLAYERBOT_HORSE_BONUS_POINTS[3] = { POINT_ATTBONUS_MONSTER, POINT_ATTBONUS_BOSS, POINT_ATTBONUS_STONE };
	const char* PLAYERBOT_HORSE_BONUS_FLAGS[3] = { "konie.nm", "konie.nb", "konie.ns" };

	struct TPlayerBotHorseTraining
	{
		int medals;
		DWORD feedVnum;
		int feedCount;
		int materials;
		long long yang;
	};

	// konie.koszt(l), and pony_buy's one medal for the first horse.
	bool GetPlayerBotHorseTrainingCost(int level, TPlayerBotHorseTraining& out)
	{
		out.medals = 0;
		out.feedVnum = 0;
		out.feedCount = 0;
		out.materials = 0;
		out.yang = 0;
		if (level == 0)
		{
			out.medals = 1;
			return true;
		}
		if (level >= 1 && level <= 9)
		{
			out.medals = 1;
			out.feedVnum = PLAYERBOT_HORSE_FEED_HAY;
			out.feedCount = 0;	// MT2009_PLUS_HORSE_NO_FEED_V1: no feed since 3 October (the owner)
			out.materials = 5;
			out.yang = 100000LL * (level + 1);
			return true;
		}
		if (level >= 11 && level <= 19)
		{
			out.medals = 2;
			out.feedVnum = PLAYERBOT_HORSE_FEED_CARROT;
			out.feedCount = 0;	// MT2009_PLUS_HORSE_NO_FEED_V1: no feed since 3 October (the owner)
			out.materials = 10;
			out.yang = 2000000LL + (long long)(level - 11) * 1000000LL;
			return true;
		}
		if (level >= 21 && level <= 28)
		{
			out.medals = 3;
			out.feedVnum = PLAYERBOT_HORSE_FEED_GINSENG;
			out.feedCount = 0;	// MT2009_PLUS_HORSE_NO_FEED_V1: no feed since 3 October (the owner)
			out.materials = 20;
			out.yang = 15000000LL + (long long)(level - 21) * 5000000LL;
			return true;
		}
		return false;
	}

	long long GetPlayerBotHorseFeedPrice(DWORD vnum)
	{
		const int idx = vnum == PLAYERBOT_HORSE_FEED_HAY ? 0 : (vnum == PLAYERBOT_HORSE_FEED_CARROT ? 1 : 2);
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		if (proto && proto->dwGold > 0 && !IS_SET(proto->dwFlags, ITEM_FLAG_COUNT_PER_1GOLD))
			return (long long)proto->dwGold;
		return (long long)PLAYERBOT_HORSE_FEED_PRICE_FALLBACK[idx];
	}

	// ------------------------------------------------------- the Black Steed

	int GetPlayerBotBlackSteedKills(LPCHARACTER ch)
	{
		return ch ? std::max(0, ch->GetQuestFlag(PLAYERBOT_BLACK_STEED_KILLS_FLAG)) : 0;
	}

	bool IsPlayerBotBlackSteedCandidate(LPCHARACTER ch)
	{
		return ch &&
				ch->GetLevel() >= PLAYERBOT_BLACK_STEED_MIN_LEVEL &&
				ch->GetHorseLevel() == PLAYERBOT_BLACK_STEED_FROM_HORSE_LEVEL &&
				ch->GetHorseHealth() > 0;
	}

	bool IsPlayerBotOnBlackSteedTrial(LPCHARACTER ch)
	{
		return IsPlayerBotHorseTrialOpenHere(PLAYERBOT_MAP_GROTTO_V2) &&
				IsPlayerBotBlackSteedCandidate(ch) &&
				!IsPlayerBotTrialExempt(ch) &&
				GetPlayerBotBlackSteedKills(ch) < PLAYERBOT_BLACK_STEED_KILLS;
	}

	bool IsPlayerBotBlackSteedEarned(LPCHARACTER ch)
	{
		return IsPlayerBotBlackSteedCandidate(ch) &&
				GetPlayerBotBlackSteedKills(ch) >= PLAYERBOT_BLACK_STEED_KILLS;
	}

	bool IsPlayerBotBlackSteedTrialMob(DWORD vnum)
	{
		return vnum == PLAYERBOT_BLACK_STEED_MOB;
	}

	// Any of the three horse trials open for this bot - the exclusion lists
	// of the other errands ask this.
	bool IsPlayerBotOnAnyHorseTrial(LPCHARACTER ch)
	{
		return IsPlayerBotOnBattleHorseTrial(ch) || IsPlayerBotOnMilitaryHorseTrial(ch) ||
				IsPlayerBotOnBlackSteedTrial(ch);
	}

	// From NotePlayerBotBattleHorseKill, under its VID guard. True when the
	// kill was the trial's.
	bool NotePlayerBotBlackSteedKill(LPCHARACTER ch, LPCHARACTER target)
	{
		if (!ch || !target || !IsPlayerBotOnBlackSteedTrial(ch) ||
				!IsPlayerBotBlackSteedTrialMob(target->GetRaceNum()))
			return false;
		const int kills = GetPlayerBotBlackSteedKills(ch) + 1;
		ch->SetQuestFlag(PLAYERBOT_BLACK_STEED_KILLS_FLAG, kills);
		if (kills >= PLAYERBOT_BLACK_STEED_KILLS)
			sys_log(0, "PLAYERBOT_HORSE: black steed trial complete pid=%u name=%s kills=%d",
					ch->GetPlayerID(), ch->GetName(), kills);
		else if (kills % 10 == 0)
			sys_log(0, "PLAYERBOT_HORSE: black steed trial pid=%u name=%s kills=%d/%d",
					ch->GetPlayerID(), ch->GetName(), kills, PLAYERBOT_BLACK_STEED_KILLS);
		return true;
	}

	// ------------------------------------------------------- the bonus

	// konie.synchronizuj() in C++: the bonus of this horse level against what
	// the flags say was given, the difference added to the AFFECT_COLLECT of
	// each point (affect_add_collect, questlua_affect.cpp). Idempotent.
	void SyncPlayerBotHorseBonus(LPCHARACTER ch)
	{
		if (!ch || !ch->IsPC())
			return;
		const int level = std::min<int>(30, ch->GetHorseLevel());
		int changed = 0;
		int want[3] = { 0, 0, 0 };
		for (int i = 0; i < 3; ++i)
		{
			want[i] = level >= 21 ? PLAYERBOT_HORSE_BONUS[level - 21][i] : 0;
			const int given = ch->GetQuestFlag(PLAYERBOT_HORSE_BONUS_FLAGS[i]);
			const int delta = want[i] - given;
			if (delta == 0)
				continue;
			long value = delta;
			if (const CAffect* aff = ch->FindAffect(AFFECT_COLLECT, PLAYERBOT_HORSE_BONUS_POINTS[i]))
				value += aff->lApplyValue;
			ch->AddAffect(AFFECT_COLLECT, PLAYERBOT_HORSE_BONUS_POINTS[i], value, 0,
					INFINITE_AFFECT_DURATION, 0, true, true);
			ch->SetQuestFlag(PLAYERBOT_HORSE_BONUS_FLAGS[i], want[i]);
			++changed;
		}
		if (changed)
			sys_log(0, "PLAYERBOT_HORSE: bonus pid=%u name=%s horse=%d monsters=%d bosses=%d metins=%d",
					ch->GetPlayerID(), ch->GetName(), level, want[0], want[1], want[2]);
	}

	// Once a core run for a bot whose horse is already past twenty (the bots
	// of before this system rode military horses of 21), and after that only
	// when a level changes.
	std::set<DWORD> s_setPlayerBotHorseBonusSynced;

	void EnsurePlayerBotHorseBonus(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded())
			return;
		if (!s_setPlayerBotHorseBonusSynced.insert(ch->GetPlayerID()).second)
			return;
		SyncPlayerBotHorseBonus(ch);
	}

	// ------------------------------------------------------- the costs a bot plans for

	// activities.h, included after this file.
	bool CanPlayerBotAdvanceHorse(LPCHARACTER ch);

	// The training the bot's horse is waiting for now, when a stable visit
	// would raise it: none at 10, 20 and 29 (the trials), none at 30, none
	// under the level the horse's next band asks.
	bool GetPlayerBotNextHorseTraining(LPCHARACTER ch, TPlayerBotHorseTraining& out)
	{
		if (!ch || !CanPlayerBotAdvanceHorse(ch))
			return false;
		return GetPlayerBotHorseTrainingCost(ch->GetHorseLevel(), out);
	}

	// Medals for the next `levels` trainings of the horse, counted from where
	// it stands and stopping at a trial.
	int GetPlayerBotHorseTrainingMedals(LPCHARACTER ch, int levels)
	{
		if (!ch || !CanPlayerBotAdvanceHorse(ch))
			return 0;
		int medals = 0;
		for (int l = ch->GetHorseLevel(), n = 0; n < levels; ++l, ++n)
		{
			TPlayerBotHorseTraining cost;
			if (!GetPlayerBotHorseTrainingCost(l, cost))
				break;
			medals += cost.medals;
		}
		return medals;
	}

	int GetPlayerBotHorseTrainingMaterialsWanted(LPCHARACTER ch)
	{
		TPlayerBotHorseTraining cost;
		return GetPlayerBotNextHorseTraining(ch, cost) ? cost.materials : 0;
	}

	// MT2009_PLUS_HORSE_GOODS_MARKET_V1 (the owner, 3 October): what a bot keeps
	// of the Medale Konne and Materialy Rzemieslnicze, by its own level -
	//   * under 25: what its horse still needs to reach level 1;
	//   * 25 to 35: what its horse still needs to reach level 11;
	//   * past 35: the next training (and the due saddlebag row), the rest
	//     bought on the market as the horse goes (one level, not two - the
	//     owner, 3 October, after the first count on the test world).
	// Everything over that is goods. On the test world 15 956 materials and
	// 1 437 medals stood in the bags and none on the counters: every bot kept
	// two saddlebag rows and two trainings ahead, from its first level.
	const BYTE PLAYERBOT_HORSE_GOODS_YOUNG_BELOW = 25;
	const BYTE PLAYERBOT_HORSE_GOODS_MID_UNTIL = 35;
	const int PLAYERBOT_HORSE_GOODS_YOUNG_HORSE = 1;
	const int PLAYERBOT_HORSE_GOODS_MID_HORSE = 11;

	// The horse level a bot of 35 or less keeps towards; 0 past 35.
	int GetPlayerBotHorseGoodsTargetLevel(LPCHARACTER ch)
	{
		if (!ch)
			return 0;
		if (ch->GetLevel() < PLAYERBOT_HORSE_GOODS_YOUNG_BELOW)
			return PLAYERBOT_HORSE_GOODS_YOUNG_HORSE;
		if (ch->GetLevel() <= PLAYERBOT_HORSE_GOODS_MID_UNTIL)
			return PLAYERBOT_HORSE_GOODS_MID_HORSE;
		return 0;
	}

	// The paid trainings from the horse's level up to `target` (the trials
	// cost no medal and no material).
	void SumPlayerBotHorseTrainingTo(LPCHARACTER ch, int target, int& medals, int& materials)
	{
		medals = 0;
		materials = 0;
		if (!ch)
			return;
		for (int l = ch->GetHorseLevel(); l < target; ++l)
		{
			TPlayerBotHorseTraining cost;
			if (!GetPlayerBotHorseTrainingCost(l, cost))
				continue;
			medals += cost.medals;
			materials += cost.materials;
		}
	}

	// Materials of the next `levels` trainings, stopping at a trial like the
	// medals above.
	int GetPlayerBotHorseTrainingMaterials(LPCHARACTER ch, int levels)
	{
		if (!ch || !CanPlayerBotAdvanceHorse(ch))
			return 0;
		int materials = 0;
		for (int l = ch->GetHorseLevel(), n = 0; n < levels; ++l, ++n)
		{
			TPlayerBotHorseTraining cost;
			if (!GetPlayerBotHorseTrainingCost(l, cost))
				break;
			materials += cost.materials;
		}
		return materials;
	}

	// The materials the horse keeps (and buys towards) by the band above.
	int GetPlayerBotHorseGoodsMaterialsWanted(LPCHARACTER ch, bool nextOnly)
	{
		const int target = GetPlayerBotHorseGoodsTargetLevel(ch);
		if (target > 0)
		{
			int medals = 0, materials = 0;
			SumPlayerBotHorseTrainingTo(ch, target, medals, materials);
			return materials;
		}
		// Past 35 one training ahead (the owner, 3 October: "zapas na jeden
		// poziom konia zamiast dwoch"); short of it, the market.
		(void)nextOnly;
		return GetPlayerBotHorseTrainingMaterialsWanted(ch);
	}

	// A medal off a counter for the horse: the next training's, over the due
	// saddlebag row's.
	int GetPlayerBotHorseMedalKeep(LPCHARACTER ch);

	bool PlayerBotHorseWantsMedal(LPCHARACTER ch)
	{
		TPlayerBotHorseTraining cost;
		if (!GetPlayerBotNextHorseTraining(ch, cost))
			return false;
		// MT2009_PLUS_HORSE_GOODS_MARKET_V1: never past its band's keep - what it
		// bought over that would go straight back on its counter.
		const int have = (int)ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM);
		return have < cost.medals + GetPlayerBotSaddlebagMedalReserve(ch) &&
				have < GetPlayerBotHorseMedalKeep(ch);
	}

	// The medals a bot never puts up. MT2009_PLUS_HORSE_GOODS_MARKET_V1: up to
	// 35 what the horse needs to reach its band's level (1 under 25, 11 to 35);
	// past 35 the next training and the due row - no floor.
	int GetPlayerBotHorseMedalKeep(LPCHARACTER ch)
	{
		const int target = GetPlayerBotHorseGoodsTargetLevel(ch);
		if (target > 0)
		{
			int medals = 0, materials = 0;
			SumPlayerBotHorseTrainingTo(ch, target, medals, materials);
			return medals;
		}
		return GetPlayerBotHorseTrainingMedals(ch, 1) + GetPlayerBotSaddlebagMedalReserve(ch);
	}

	// The feed of this kind the next training eats; the bag keeps that much.
	int GetPlayerBotHorseFeedKeep(LPCHARACTER ch, DWORD vnum)
	{
		TPlayerBotHorseTraining cost;
		if (!GetPlayerBotNextHorseTraining(ch, cost) || cost.feedVnum != vnum)
			return 0;
		return cost.feedCount;
	}

	// The yang the training costs this bot now: the fee and the feed the bag
	// lacks at the shop's price.
	long long GetPlayerBotHorseTrainingGold(LPCHARACTER ch, const TPlayerBotHorseTraining& cost)
	{
		long long gold = cost.yang;
		if (ch && cost.feedVnum != 0)
		{
			const int lack = cost.feedCount - (int)ch->CountSpecifyItem(cost.feedVnum);
			if (lack > 0)
				gold += GetPlayerBotHorseFeedPrice(cost.feedVnum) * lack;
		}
		return gold;
	}

	// Everything the training asks is in the bag and the purse: the medals
	// over the due saddlebag row's, the materials, the yang over what the bot
	// holds back (GetPlayerBotReservedGold).
	bool CanPlayerBotPayHorseTraining(LPCHARACTER ch)
	{
		TPlayerBotHorseTraining cost;
		if (!GetPlayerBotNextHorseTraining(ch, cost))
			return false;
		if ((int)ch->CountSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM) <
				cost.medals + GetPlayerBotSaddlebagMedalReserve(ch))
			return false;
		if ((int)ch->CountSpecifyItem(PLAYERBOT_HORSE_TRAINING_MATERIAL_VNUM) < cost.materials)
			return false;
		return (long long)ch->GetGold() - (long long)GetPlayerBotReservedGold(ch) >=
				GetPlayerBotHorseTrainingGold(ch, cost);
	}

	// Something for the stable this minute: a trial to collect (the battle
	// horse with its fee in the purse) or a training paid in full. What used
	// to be "a medal in the bag" - the village holds a bot back for it.
	bool PlayerBotHasStableBusiness(LPCHARACTER ch)
	{
		if (!ch)
			return false;
		if (IsPlayerBotBattleHorseEarned(ch) && ch->GetGold() >= (int)PLAYERBOT_BATTLE_HORSE_FEE)
			return true;
		return IsPlayerBotMilitaryHorseEarned(ch) || IsPlayerBotBlackSteedEarned(ch) ||
				CanPlayerBotPayHorseTraining(ch);
	}

	// The Stajenny's side of "Szkolenie konia": the feed the bag lacks bought
	// from the shop, the cost taken, the level up. The level itself is the
	// caller's to set (SetPlayerBotHorseLevelInSaddle, playerbot_activities.h):
	// this answers the new level, or 0 when nothing was done.
	int PayPlayerBotHorseTraining(LPCHARACTER ch)
	{
		TPlayerBotHorseTraining cost;
		if (!CanPlayerBotPayHorseTraining(ch) || !GetPlayerBotNextHorseTraining(ch, cost))
			return 0;
		const int level = ch->GetHorseLevel();
		int bought = 0;
		long long feedGold = 0;
		if (cost.feedVnum != 0)
		{
			const int lack = cost.feedCount - (int)ch->CountSpecifyItem(cost.feedVnum);
			if (lack > 0)
			{
				// Bought and eaten on the spot: the shop's price, no cell.
				bought = lack;
				feedGold = GetPlayerBotHorseFeedPrice(cost.feedVnum) * lack;
				PlayerBotChangeGold(ch, -feedGold);
			}
			const int fromBag = cost.feedCount - bought;
			if (fromBag > 0)
				ch->RemoveSpecifyItem(cost.feedVnum, fromBag);
		}
		if (cost.medals > 0)
			ch->RemoveSpecifyItem(PLAYERBOT_HORSE_MEDAL_VNUM, cost.medals);
		if (cost.materials > 0)
			ch->RemoveSpecifyItem(PLAYERBOT_HORSE_TRAINING_MATERIAL_VNUM, cost.materials);
		if (cost.yang > 0)
			PlayerBotChangeGold(ch, -cost.yang);
		sys_log(0, "PLAYERBOT_HORSE: trained pid=%u name=%s horse=%d->%d medals=%d feed=%u:%d bought=%d feed_gold=%lld materials=%d yang=%lld gold_left=%lld",
				ch->GetPlayerID(), ch->GetName(), level, level + 1, cost.medals, (unsigned int)cost.feedVnum,
				cost.feedCount, bought, feedGold, cost.materials, cost.yang, (long long)ch->GetGold());
		return level + 1;
	}
}

#endif
