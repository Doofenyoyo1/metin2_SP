#ifndef __INC_METIN2_PLAYERBOT_ALCHEMY_H__
#define __INC_METIN2_PLAYERBOT_ALCHEMY_H__

// Alchemy (Dragon Soul) for bots (MT2009 Plus, operator, 26 September 2026).
//
// Every bot of 30 and more does the Alchemist's daily errand the players do
// (dragon_soul.quest, state_farming): a monster it kills drops a Dragon Stone
// Shard one time in ten (drop_gamble_with_flag("ds_drop"), 10% unless the
// flag says otherwise), ten shards are a Cor Draconis, five Cors a day
// unless the flag ds_cor_day says otherwise. The
// quest skips bots (pc.is_bot), so the pass counts the shards itself instead
// of filling a bag with them.
//
//   * Half the bots (PLAYERBOT_ALCHEMY_PERCENT, by player id) use alchemy:
//     qualified the way a player is, they keep their Cors and buy more off
//     the counters, open them, wear the best stone of each of the seven kinds
//     (deck 0), keep the deck active outside the safe zones, refine at the
//     Alchemist (grade and step from two stones, strength with a Green
//     Dragon Bean) up to what their level asks, buy the Time Elixir when a
//     worn stone runs low, and sell what they have no use for: a spare kind,
//     a worse copy, or a stone whose bonuses are nothing to their class.
//   * The other half sells its Cors on its counters, five and more a line.
//
// Every engine step is the one the player's window calls (DSManager's
// DoRefineGrade / DoRefineStep / DoRefineStrength, EquipItem, PullOut,
// UseItem), so odds, fees and results are the players'.

#include "DragonSoul.h"

namespace
{
	// Three bots in four since 28 September: with half, 448 Cors stood on the
	// counters and 1878 more in the bags of the sellers, while the users that
	// could have opened them were a few hundred (the operator: "boty chetniej
	// to kupowaly ... i klikaly alchemie").
	const int PLAYERBOT_ALCHEMY_PERCENT = 75;
	const int PLAYERBOT_ALCHEMY_MIN_LEVEL = 30;
	const DWORD PLAYERBOT_DS_SHARD_VNUM = 30270;
	const int PLAYERBOT_DS_SHARDS_PER_COR = 10;
	// Cors a day: the players' event flag ds_cor_day (dragon_soul.quest, the
	// admin panel's alchemy page; 1-20), this when it is unset or out of range.
	const int PLAYERBOT_DS_CORS_PER_DAY = 5;
	const DWORD PLAYERBOT_COR_ROUGH_VNUM = 50255;
	const int PLAYERBOT_COR_LINE_MIN_UNITS = 5;
	const DWORD PLAYERBOT_DS_TIME_ELIXIR_VNUM = 100002;
	const DWORD PLAYERBOT_DS_GREEN_BEAN_VNUM = 100300;
	const DWORD PLAYERBOT_DS_ALCHEMIST_VNUM = 20001;
	// A worn stone with less than this left gets an elixir.
	const int PLAYERBOT_DS_ELIXIR_BELOW_SEC = 2 * 60 * 60;
	// Unworn stones of one kind kept as refine material.
	// Eight: a rare stone takes about nine ordinary ones.
	const int PLAYERBOT_DS_MATERIAL_KEEP = 8;
	// Dragon Stone lines one counter shows (playerbot_offline_shop.h).
	// Eight, not four: 43 000 unworn stones stood in the bags of 800 users on
	// the test world (26 September 2026), four lines at a time.
	const int PLAYERBOT_DS_COUNTER_LINES = 8;
	// Cors a user keeps in the bag before it stops buying them.
	const int PLAYERBOT_DS_COR_KEEP = 25;
	const DWORD PLAYERBOT_ALCHEMY_CHECK_MIN_MS = 3 * 60 * 1000;
	const DWORD PLAYERBOT_ALCHEMY_CHECK_MAX_MS = 6 * 60 * 1000;
	const DWORD PLAYERBOT_ALCHEMY_LOCAL_MS = 20000;
	const int PLAYERBOT_ALCHEMY_VISIT_MAX_STEPS = 20;
	// Cors opened a pass (every PLAYERBOT_ALCHEMY_LOCAL_MS), in a fight too:
	// three a pass out of combat left 2300 Cors in the bags of 31 users.
	const int PLAYERBOT_DS_COR_OPEN_PER_PASS = 10;
	// What a refine leaves over its fee: a million kept the poorer users, the
	// ones that opened their Cors, away from the Alchemist for good.
	const long long PLAYERBOT_DS_REFINE_SPARE = 300000LL;
	// A Cor off a counter: at most this many times the price the bots ask for
	// one (ScalePlayerBotIwakuraPrice(PLAYERBOT_COR_DRACONIS_PRICE)). The old
	// cap was twice the unscaled 100 000 - under every Cor on every counter
	// (680 000 - 1 025 000 on the test world), so no bot ever bought one.
	const int PLAYERBOT_DS_COR_BUY_MULT = 5;
	const char* PLAYERBOT_DS_SHARDS_FLAG = "playerbot.ds_shards";
	const char* PLAYERBOT_DS_DAY_FLAG = "playerbot.ds_day";
	const char* PLAYERBOT_DS_LEFT_FLAG = "playerbot.ds_left";

	// ------------------------------------------------------------ prices
	//
	// A stone's price is what it costs to make, on average, from Cors at
	// PLAYERBOT_COR_DRACONIS_PRICE, plus PLAYERBOT_DS_PRICE_MARGIN_PERCENT
	// (operator, 26 September 2026). From the engine's tables
	// (dragon_soul_table.txt, DoRefineGrade/Step/Strength):
	//   grade: two stones of a grade -> one of the next (50%) or of the same
	//     (50%), step 0: 2 tries on average, 3 stones net, fee a try;
	//   step: two of a grade and step -> the next step (70%) or the same (30%):
	//     1/0.7 tries, 2 + (1/0.7 - 1) stones net, fee a try;
	//   strength: a Green Dragon Bean a try (100/80/70/50/30/20%), a failure
	//     one strength down: E(k) = (1 + (1 - p(k)) * E(k-1)) / p(k) tries for
	//     the step from k to k+1.
	const int PLAYERBOT_DS_PRICE_MARGIN_PERCENT = 25;
	const long long PLAYERBOT_DS_GRADE_FEES[5] = { 30000, 50000, 70000, 100000, 150000 };
	const long long PLAYERBOT_DS_STEP_FEES[4] = { 20000, 30000, 40000, 50000 };
	const double PLAYERBOT_DS_STRENGTH_PCT[6] = { 1.0, 0.8, 0.7, 0.5, 0.3, 0.2 };
	const long long PLAYERBOT_DS_BEAN_COST = 1000000 + 10000;

	long long GetPlayerBotDragonSoulBaseCost(BYTE grade, BYTE step, BYTE strength)
	{
		static bool s_built = false;
		static double s_cost[6][5];
		static double s_strength[7];
		if (!s_built)
		{
			s_built = true;
			double g = (double)PLAYERBOT_COR_DRACONIS_PRICE;
			for (int gi = 0; gi < 6; ++gi)
			{
				double s = g;
				s_cost[gi][0] = s;
				for (int si = 0; si < 4; ++si)
				{
					s = (2.0 + (1.0 / 0.7 - 1.0)) * s + (1.0 / 0.7) * (double)PLAYERBOT_DS_STEP_FEES[si];
					s_cost[gi][si + 1] = s;
				}
				if (gi < 5)
					g = 3.0 * g + 2.0 * (double)PLAYERBOT_DS_GRADE_FEES[gi];
			}
			double e = 1.0, sum = 0.0;
			s_strength[0] = 0.0;
			for (int k = 0; k < 6; ++k)
			{
				if (k > 0)
					e = (1.0 + (1.0 - PLAYERBOT_DS_STRENGTH_PCT[k]) * e) / PLAYERBOT_DS_STRENGTH_PCT[k];
				sum += e;
				s_strength[k + 1] = sum;
			}
		}
		grade = std::min<BYTE>(grade, 5);
		step = std::min<BYTE>(step, 4);
		strength = std::min<BYTE>(strength, 6);
		return (long long)(s_cost[grade][step] + s_strength[strength] * (double)PLAYERBOT_DS_BEAN_COST);
	}

	DWORD GetPlayerBotDragonSoulPrice(LPITEM item)
	{
		if (!item || !item->IsDragonSoul())
			return 0;
		const DWORD vnum = item->GetVnum();
		const long long cost = GetPlayerBotDragonSoulBaseCost((BYTE)((vnum / 1000) % 10),
				(BYTE)((vnum / 100) % 10), (BYTE)((vnum / 10) % 10));
		long long price = cost * (100 + PLAYERBOT_DS_PRICE_MARGIN_PERCENT) / 100;
		// On the world's yang rate and inflation, like the rest of the bots'
		// goods (ScalePlayerBotIwakuraPrice; operator, 26 September 2026): the
		// table is the price at the curve's base.
		price = (long long)ScalePlayerBotIwakuraPrice((DWORD)std::min<long long>(price, 0xFFFFFFFFLL));
		price = (price + 500) / 1000 * 1000;
		return (DWORD)std::min<long long>(std::max<long long>(price, 1000), GOLD_MAX - 1000);
	}

	// ------------------------------------------------------------ who

	bool ArePlayerBotAlchemyOff()
	{
		return quest::CQuestManager::instance().GetEventFlag("m2_alchemy_off") != 0;
	}

	// The players' daily Cor count (dragon_soul.quest reads the same flag).
	int GetPlayerBotDragonCorsPerDay()
	{
		const int n = quest::CQuestManager::instance().GetEventFlag("ds_cor_day");
		return (n >= 1 && n <= 20) ? n : PLAYERBOT_DS_CORS_PER_DAY;
	}

	// The share scaled by the panel's ALCHEMY will (playerbot_config.h; 100 =
	// the build's 75%), by player id, so the same bots come back when raised.
	bool IsPlayerBotAlchemyUserPID(DWORD pid)
	{
		const DWORD h = (pid * 3266489917U) ^ 0x414c4348U;
		return (int)((h >> 15) % 100) < PLAYERBOT_ALCHEMY_PERCENT * s_iPlayerBotAlchemyPercent / 100;
	}

	bool IsPlayerBotAlchemyUser(LPCHARACTER ch)
	{
		if (!ch || !ch->IsPC() || ch->GetLevel() < PLAYERBOT_ALCHEMY_MIN_LEVEL || ArePlayerBotAlchemyOff())
			return false;
		const DWORD pid = ch->GetPlayerID();
		return IsPlayerBotAlchemyUserPID(pid) && !IsPlayerBotSidekickPID(pid) &&
				!CPlayerBotManager::instance().IsMedalDropperCohortPID(pid);
	}

	struct TPlayerBotAlchemyStats
	{
		unsigned shards, cors, opened, equipped, refinesGrade, refinesStep, refinesStrength, refineFails,
				elixirs, beans, listed, trips;
	};
	TPlayerBotAlchemyStats s_kPlayerBotAlchemyStats = { 0 };

	// ------------------------------------------------------------ the daily Cors

	// Called on every monster a bot kills (NotePlayerBotBattleHorseKill).
	void NotePlayerBotDragonShardKill(LPCHARACTER ch, LPCHARACTER victim)
	{
		if (!ch || ch->GetLevel() < PLAYERBOT_ALCHEMY_MIN_LEVEL || ArePlayerBotAlchemyOff())
			return;
		// The player's rule (dragon_soul.quest): only a monster at most 15
		// levels under the killer, or any higher (operator, 27 September 2026).
		if (!victim || (int)victim->GetLevel() < (int)ch->GetLevel() - 15)
			return;
		const int today = (int)(get_global_time() / 86400);
		if (ch->GetQuestFlag(PLAYERBOT_DS_DAY_FLAG) != today)
		{
			ch->SetQuestFlag(PLAYERBOT_DS_DAY_FLAG, today);
			ch->SetQuestFlag(PLAYERBOT_DS_LEFT_FLAG, GetPlayerBotDragonCorsPerDay());
		}
		const int left = ch->GetQuestFlag(PLAYERBOT_DS_LEFT_FLAG);
		if (left <= 0)
			return;
		int chance = quest::CQuestManager::instance().GetEventFlag("ds_drop");
		if (chance < 1 || chance > 100)
			chance = 10;
		if (number(1, 100) > chance)
			return;
		++s_kPlayerBotAlchemyStats.shards;
		const int shards = ch->GetQuestFlag(PLAYERBOT_DS_SHARDS_FLAG) + 1;
		if (shards < PLAYERBOT_DS_SHARDS_PER_COR)
		{
			ch->SetQuestFlag(PLAYERBOT_DS_SHARDS_FLAG, shards);
			return;
		}
		if (ch->GetEmptyInventory(1) < 0 || !ch->AutoGiveItem(PLAYERBOT_COR_ROUGH_VNUM, 1, -1, false))
		{
			// No room: the shards wait, as a player's would in the bag.
			ch->SetQuestFlag(PLAYERBOT_DS_SHARDS_FLAG, shards);
			return;
		}
		ch->SetQuestFlag(PLAYERBOT_DS_SHARDS_FLAG, 0);
		ch->SetQuestFlag(PLAYERBOT_DS_LEFT_FLAG, left - 1);
		++s_kPlayerBotAlchemyStats.cors;
		sys_log(0, "PLAYERBOT_ALCHEMY: daily cor pid=%u name=%s left_today=%d user=%d",
				ch->GetPlayerID(), ch->GetName(), left - 1, IsPlayerBotAlchemyUser(ch) ? 1 : 0);
	}

	bool IsPlayerBotCorVnum(DWORD vnum)
	{
		return vnum == PLAYERBOT_COR_ROUGH_VNUM || vnum == 50260;
	}

	// A user's Cors are its own; everybody else's are goods, a line of at
	// least PLAYERBOT_COR_LINE_MIN_UNITS.
	bool IsPlayerBotKeptCor(LPCHARACTER ch, LPITEM item)
	{
		return ch && item && IsPlayerBotCorVnum(item->GetVnum()) && IsPlayerBotAlchemyUser(ch);
	}

	bool IsPlayerBotCorStackShort(LPCHARACTER ch, LPITEM item)
	{
		return ch && item && IsPlayerBotCorVnum(item->GetVnum()) &&
				(int)ch->CountSpecifyItem(item->GetVnum()) < PLAYERBOT_COR_LINE_MIN_UNITS;
	}

	// ------------------------------------------------------------ the stones

	struct TPlayerBotDsTarget
	{
		BYTE grade, step, strength;
	};

	// What a user's stones should come to at its level: the refining stops
	// there, and a stone past it is only worn.
	//
	// The grade first - the operator's table (26 September 2026):
	//   Zwykly, Blyszczacy  - never worn, only material;
	//   Rzadki              - worn at low levels (30-54);
	//   Starozytny          - at middle levels (55-74);
	//   Legendarny          - gladly (75-89), and stepped up after it;
	//   Mityczny            - worn by anybody who has one, even unstepped;
	//                         only the end game (90+) makes and steps it.
	// "Wszystkie kamienie alchemii przed wlozeniem powinny byc ulepszane do
	// maksymalnego dostepnego poziomu": a kind is refined as far as the bag
	// allows before its stone goes on (EquipPlayerBotBestDragonSouls), and the
	// stones of the target grade left over are stepped to the highest step. A
	// grade refine gives step 0 and strength 0 (DSManager::DoRefineGrade), so
	// the steps come after the grade.
	const BYTE PLAYERBOT_DS_MIN_WORN_GRADE = 2;
	const BYTE PLAYERBOT_DS_MYTH_GRADE = 5;
	const int PLAYERBOT_DS_END_GAME_LEVEL = 90;

	TPlayerBotDsTarget GetPlayerBotDsTarget(LPCHARACTER ch)
	{
		TPlayerBotDsTarget t = { 0, 0, 0 };
		const int level = ch ? ch->GetLevel() : 0;
		if (level >= PLAYERBOT_DS_END_GAME_LEVEL) { t.grade = 5; t.step = 4; t.strength = 4; }
		else if (level >= 75) { t.grade = 4; t.step = 4; t.strength = 2; }
		else if (level >= 55) { t.grade = 3; t.step = 4; t.strength = 1; }
		else if (level >= PLAYERBOT_ALCHEMY_MIN_LEVEL) { t.grade = 2; t.step = 4; t.strength = 0; }
		return t;
	}

	BYTE GetPlayerBotDsGrade(LPITEM item) { return item ? (BYTE)((item->GetVnum() / 1000) % 10) : 0; }
	BYTE GetPlayerBotDsStep(LPITEM item) { return item ? (BYTE)((item->GetVnum() / 100) % 10) : 0; }
	BYTE GetPlayerBotDsStrength(LPITEM item) { return item ? (BYTE)((item->GetVnum() / 10) % 10) : 0; }

	// What the stone's lines are worth to this character (the gear's own
	// scale); nothing at all for a stone of a kind it gets nothing from.
	long long ScorePlayerBotDsLines(LPCHARACTER ch, LPITEM item)
	{
		long long score = 0;
		for (int i = 0; item && i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			if (item->GetAttributeType(i) != 0)
				score += ScorePlayerBotApplyTiered(item->GetAttributeType(i), item->GetAttributeValue(i), ch);
		return score;
	}

	// Which of two stones of a kind to wear: grade, then step, then strength,
	// then the lines.
	long long RankPlayerBotDs(LPCHARACTER ch, LPITEM item)
	{
		if (!item)
			return -1;
		const long long lines = std::min<long long>(std::max<long long>(ScorePlayerBotDsLines(ch, item), 0), 99999);
		return (long long)GetPlayerBotDsGrade(item) * 10000000000LL + (long long)GetPlayerBotDsStep(item) * 1000000000LL +
				(long long)GetPlayerBotDsStrength(item) * 100000000LL + lines;
	}

	bool HasPlayerBotDsTime(LPITEM item)
	{
		return item && DSManager::instance().IsTimeLeftDragonSoul(item);
	}

	void CollectPlayerBotDragonSouls(LPCHARACTER ch, std::vector<LPITEM>& out, int kind = -1)
	{
		out.clear();
		if (!ch || !ch->IsItemLoaded())
			return;
		for (int cell = 0; cell < DRAGON_SOUL_INVENTORY_MAX_NUM; ++cell)
		{
			LPITEM item = ch->GetItem(TItemPos(DRAGON_SOUL_INVENTORY, cell));
			if (!item || item->GetCell() != cell || !item->IsDragonSoul() || item->isLocked() || item->IsExchanging())
				continue;
			if (kind >= 0 && item->GetSubType() != kind)
				continue;
			out.push_back(item);
		}
	}

	LPITEM GetPlayerBotWornDs(LPCHARACTER ch, int kind)
	{
		if (!ch || kind < 0 || kind >= DS_SLOT_MAX)
			return NULL;
		return ch->GetItem(TItemPos(INVENTORY, DRAGON_SOUL_EQUIP_SLOT_START + kind));
	}

	// Surplus - the counter's: every unworn stone of a bot that does not use
	// alchemy; for a user, a stone its kind's best already beats and the
	// refining cannot use (over its target, or past the material it keeps),
	// or one whose lines give its class nothing and that is no material.
	bool IsPlayerBotSurplusDragonSoul(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item || !item->IsDragonSoul() || item->IsEquipped() || item->isLocked())
			return false;
		// An ordinary or brilliant stone is never goods: nobody buys one, and
		// it is the material of the rare grade ("boty niech nie wystawiaja
		// surowej smoczej alchemii ... dopiero od 3 poziomu", operator, 26
		// September 2026).
		if (GetPlayerBotDsGrade(item) < PLAYERBOT_DS_MIN_WORN_GRADE)
			return false;
		if (!IsPlayerBotAlchemyUser(ch))
			return true;
		const int kind = item->GetSubType();
		LPITEM worn = GetPlayerBotWornDs(ch, kind);
		const TPlayerBotDsTarget t = GetPlayerBotDsTarget(ch);
		const BYTE grade = GetPlayerBotDsGrade(item);
		// Material: under the target grade, one of the first
		// PLAYERBOT_DS_MATERIAL_KEEP of its kind and grade.
		if (grade < t.grade || (grade == t.grade && GetPlayerBotDsStep(item) < t.step))
		{
			std::vector<LPITEM> same;
			CollectPlayerBotDragonSouls(ch, same, kind);
			int ahead = 0;
			for (size_t i = 0; i < same.size(); ++i)
				if (same[i] != item && GetPlayerBotDsGrade(same[i]) == grade && same[i]->GetCell() < item->GetCell())
					++ahead;
			return ahead >= PLAYERBOT_DS_MATERIAL_KEEP;
		}
		if (ScorePlayerBotDsLines(ch, item) <= 0)
			return true;
		return worn && RankPlayerBotDs(ch, worn) >= RankPlayerBotDs(ch, item);
	}

	// ------------------------------------------------------------ the market

	// A user buys Cors while it holds fewer than PLAYERBOT_DS_COR_KEEP, and a
	// stone of a kind it wears nothing of or a worse one of, up to its target
	// grade, with lines worth something to it.
	bool WantsPlayerBotAlchemyOffer(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !offer || !IsPlayerBotAlchemyUser(ch))
			return false;
		if (IsPlayerBotCorVnum(offer->GetVnum()))
			return (int)ch->CountSpecifyItem(offer->GetVnum()) < PLAYERBOT_DS_COR_KEEP;
		if (!offer->IsDragonSoul())
			return false;
		// A stone it would wear: of the rare grade and over, up to its target
		// (a mythic one whatever the target).
		const TPlayerBotDsTarget t = GetPlayerBotDsTarget(ch);
		const BYTE grade = GetPlayerBotDsGrade(offer);
		if (grade < PLAYERBOT_DS_MIN_WORN_GRADE || (grade > t.grade && grade < PLAYERBOT_DS_MYTH_GRADE) ||
				ScorePlayerBotDsLines(ch, offer) <= 0)
			return false;
		LPITEM worn = GetPlayerBotWornDs(ch, offer->GetSubType());
		return !worn || RankPlayerBotDs(ch, offer) > RankPlayerBotDs(ch, worn);
	}

	bool CanPlayerBotPayForAlchemyOffer(LPCHARACTER ch, LPITEM item, long long price)
	{
		if (!ch || !item || price <= 0)
			return false;
		const long long spare = (long long)ch->GetGold() - GetPlayerBotReservedGold(ch) -
				(long long)PLAYERBOT_SHOPPING_GOLD_FLOOR;
		if (IsPlayerBotCorVnum(item->GetVnum()))
		{
			const long long unit = price / std::max<long long>(1, (long long)item->GetCount());
			return unit <= (long long)ScalePlayerBotIwakuraPrice(PLAYERBOT_COR_DRACONIS_PRICE) * PLAYERBOT_DS_COR_BUY_MULT &&
					price <= spare * 40 / 100;
		}
		return price <= (long long)GetPlayerBotDragonSoulPrice(item) * 12 / 10 && price <= spare * 25 / 100;
	}

	bool PlayerBotWantsAlchemyFromMarket(LPCHARACTER ch)
	{
		if (!IsPlayerBotAlchemyUser(ch) || (int)ch->CountSpecifyItem(PLAYERBOT_COR_ROUGH_VNUM) >= PLAYERBOT_DS_COR_KEEP)
			return false;
		const TPlayerBotMarketLedgerEntry* cors = GetPlayerBotMarketLedgerEntry(PLAYERBOT_COR_ROUGH_VNUM);
		return cors && cors->dwSupplyUnits > 0 &&
				(long long)ch->GetGold() - GetPlayerBotReservedGold(ch) > 1000000LL;
	}

	// ------------------------------------------------------------ in the field

	// Qualified the way the Alchemist's first errand qualifies a player.
	void EnsurePlayerBotAlchemyQualified(LPCHARACTER ch)
	{
		if (ch && !ch->DragonSoul_IsQualified())
		{
			ch->DragonSoul_GiveQualification();
			sys_log(0, "PLAYERBOT_ALCHEMY: qualified pid=%u name=%s", ch->GetPlayerID(), ch->GetName());
		}
	}

	// Its Cors opened while the alchemy bag has room: the Cor's group is one
	// rough stone of a random kind (special_item_group.txt, 50255).
	int OpenPlayerBotCors(LPCHARACTER ch)
	{
		int opened = 0;
		for (int cell = 0; cell < PLAYERBOT_BAG_CELLS && opened < PLAYERBOT_DS_COR_OPEN_PER_PASS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem((WORD)cell);
			if (!item || item->GetCell() != cell || !IsPlayerBotCorVnum(item->GetVnum()) || item->isLocked())
				continue;
			std::vector<LPITEM> stones;
			CollectPlayerBotDragonSouls(ch, stones);
			if ((int)stones.size() >= DRAGON_SOUL_INVENTORY_MAX_NUM - 8)
				break;
			if (!ch->UseItem(TItemPos(INVENTORY, (WORD)cell)))
				break;
			++opened;
			++s_kPlayerBotAlchemyStats.opened;
			--cell;	// the stack may still be there
		}
		if (opened)
			sys_log(0, "PLAYERBOT_ALCHEMY: opened pid=%u name=%s cors=%d left=%d", ch->GetPlayerID(), ch->GetName(),
					opened, (int)ch->CountSpecifyItem(PLAYERBOT_COR_ROUGH_VNUM));
		return opened;
	}

	bool PullOutPlayerBotDs(LPCHARACTER ch, LPITEM item)
	{
		if (!item || !item->IsEquipped())
			return true;
		const int cell = ch->GetEmptyDragonSoulInventory(item);
		if (cell < 0)
			return false;
		LPITEM moved = item;
		return DSManager::instance().PullOut(ch, TItemPos(DRAGON_SOUL_INVENTORY, (WORD)cell), moved) && !moved->IsEquipped();
	}

	int FindPlayerBotDsRefine(LPCHARACTER ch, int kind, LPITEM& a, LPITEM& b);

	// The best stone with time left of every kind into deck 0: of the rare
	// grade and over, and only once the kind has no refine left to do (the
	// Alchemist does it first) - but a mythic stone goes on as it is. A worn
	// ordinary or brilliant stone comes off, material again.
	int EquipPlayerBotBestDragonSouls(LPCHARACTER ch)
	{
		int changed = 0;
		for (int kind = 0; kind < DS_SLOT_MAX; ++kind)
		{
			LPITEM worn = GetPlayerBotWornDs(ch, kind);
			if (worn && GetPlayerBotDsGrade(worn) < PLAYERBOT_DS_MIN_WORN_GRADE && PullOutPlayerBotDs(ch, worn))
			{
				sys_log(0, "PLAYERBOT_ALCHEMY: took off pid=%u name=%s vnum=%u kind=%d (under the rare grade)",
						ch->GetPlayerID(), ch->GetName(), worn->GetVnum(), kind);
				worn = NULL;
			}
			std::vector<LPITEM> stones;
			CollectPlayerBotDragonSouls(ch, stones, kind);
			LPITEM best = NULL;
			for (size_t i = 0; i < stones.size(); ++i)
				if (HasPlayerBotDsTime(stones[i]) && ScorePlayerBotDsLines(ch, stones[i]) > 0 &&
						GetPlayerBotDsGrade(stones[i]) >= PLAYERBOT_DS_MIN_WORN_GRADE &&
						(!best || RankPlayerBotDs(ch, stones[i]) > RankPlayerBotDs(ch, best)))
					best = stones[i];
			if (best && GetPlayerBotDsGrade(best) < PLAYERBOT_DS_MYTH_GRADE)
			{
				LPITEM a = NULL, b = NULL;
				if (FindPlayerBotDsRefine(ch, kind, a, b) != 0)	// PLAYERBOT_DS_WORK_NONE
					continue;
			}
			if (!best || (worn && HasPlayerBotDsTime(worn) && RankPlayerBotDs(ch, worn) >= RankPlayerBotDs(ch, best)))
				continue;
			if (worn && !PullOutPlayerBotDs(ch, worn))
				continue;
			if (PlayerBotEquipItem(ch, best))
			{
				++changed;
				++s_kPlayerBotAlchemyStats.equipped;
				sys_log(0, "PLAYERBOT_ALCHEMY: wear pid=%u name=%s vnum=%u kind=%d old=%u", ch->GetPlayerID(),
						ch->GetName(), best->GetVnum(), kind, worn ? worn->GetVnum() : 0);
			}
			else if (worn && !worn->IsEquipped())
				PlayerBotEquipItem(ch, worn);
		}
		return changed;
	}

	int CountPlayerBotWornDs(LPCHARACTER ch, bool withTime)
	{
		int n = 0;
		for (int kind = 0; kind < DS_SLOT_MAX; ++kind)
		{
			LPITEM worn = GetPlayerBotWornDs(ch, kind);
			if (worn && (!withTime || HasPlayerBotDsTime(worn)))
				++n;
		}
		return n;
	}

	// The deck is on outside the safe zones and off inside them, where a stone
	// only spends its time.
	void ManagePlayerBotDsDeck(LPCHARACTER ch)
	{
		const bool safe = IsPlayerBotSafeZone(ch->GetMapIndex(), ch->GetX(), ch->GetY()) || ch->GetMyShop();
		const bool active = ch->DragonSoul_GetActiveDeck() >= 0;
		if (!safe && !active && CountPlayerBotWornDs(ch, true) > 0)
			ch->DragonSoul_ActivateDeck(DRAGON_SOUL_DECK_0);
		else if (safe && active)
			ch->DragonSoul_DeactivateAll();
	}

	// ------------------------------------------------------------ at the Alchemist

	bool PlayerBotDsRefineGrid(TItemPos (&grid)[DRAGON_SOUL_REFINE_GRID_SIZE], LPITEM a, LPITEM b)
	{
		for (int i = 0; i < DRAGON_SOUL_REFINE_GRID_SIZE; ++i)
			grid[i] = NPOS;
		if (!a || !b)
			return false;
		grid[0] = TItemPos(a->GetWindow(), a->GetCell());
		grid[1] = TItemPos(b->GetWindow(), b->GetCell());
		return true;
	}

	enum { PLAYERBOT_DS_WORK_NONE, PLAYERBOT_DS_WORK_GRADE, PLAYERBOT_DS_WORK_STEP, PLAYERBOT_DS_WORK_STRENGTH };

	// The next refine for one kind: two stones of a grade under the target;
	// else two of the target grade and a step under the target; else the
	// best one's strength with a bean. The two cheapest go; the worn one is
	// used only when nothing else can.
	int FindPlayerBotDsRefine(LPCHARACTER ch, int kind, LPITEM& a, LPITEM& b)
	{
		a = b = NULL;
		const TPlayerBotDsTarget t = GetPlayerBotDsTarget(ch);
		std::vector<LPITEM> stones;
		CollectPlayerBotDragonSouls(ch, stones, kind);
		LPITEM worn = GetPlayerBotWornDs(ch, kind);
		if (worn)
			stones.push_back(worn);
		const long long spare = (long long)ch->GetGold() - GetPlayerBotReservedGold(ch);
		for (BYTE grade = 0; grade < t.grade; ++grade)
		{
			std::vector<LPITEM> pick;
			for (size_t i = 0; i < stones.size(); ++i)
				if (GetPlayerBotDsGrade(stones[i]) == grade)
					pick.push_back(stones[i]);
			if (pick.size() < 2 || spare < PLAYERBOT_DS_GRADE_FEES[grade] + PLAYERBOT_DS_REFINE_SPARE)
				continue;
			std::sort(pick.begin(), pick.end(), [&](LPITEM x, LPITEM y) {
				return (x == worn ? 1 : 0) < (y == worn ? 1 : 0) ||
						((x == worn) == (y == worn) && RankPlayerBotDs(ch, x) < RankPlayerBotDs(ch, y)); });
			a = pick[0];
			b = pick[1];
			return PLAYERBOT_DS_WORK_GRADE;
		}
		// The steps: at the highest grade it has a pair of, the target's or
		// over it (a stone bought or made before).
		for (int grade = 5; grade >= (int)t.grade; --grade)
		for (BYTE step = 0; step < t.step; ++step)
		{
			// A mythic stone is stepped only by the end game.
			if (grade >= PLAYERBOT_DS_MYTH_GRADE && ch->GetLevel() < PLAYERBOT_DS_END_GAME_LEVEL)
				break;
			std::vector<LPITEM> pick;
			for (size_t i = 0; i < stones.size(); ++i)
				if (GetPlayerBotDsGrade(stones[i]) == grade && GetPlayerBotDsStep(stones[i]) == step)
					pick.push_back(stones[i]);
			if (pick.size() < 2 || spare < PLAYERBOT_DS_STEP_FEES[step] + PLAYERBOT_DS_REFINE_SPARE)
				continue;
			std::sort(pick.begin(), pick.end(), [&](LPITEM x, LPITEM y) {
				return (x == worn ? 1 : 0) < (y == worn ? 1 : 0) ||
						((x == worn) == (y == worn) && RankPlayerBotDs(ch, x) < RankPlayerBotDs(ch, y)); });
			a = pick[0];
			b = pick[1];
			return PLAYERBOT_DS_WORK_STEP;
		}
		LPITEM best = NULL;
		for (size_t i = 0; i < stones.size(); ++i)
			if (GetPlayerBotDsGrade(stones[i]) >= t.grade && GetPlayerBotDsStep(stones[i]) >= t.step &&
					(!best || RankPlayerBotDs(ch, stones[i]) > RankPlayerBotDs(ch, best)))
				best = stones[i];
		if (best && GetPlayerBotDsStrength(best) < t.strength && ScorePlayerBotDsLines(ch, best) > 0 &&
				spare >= PLAYERBOT_DS_BEAN_COST * 3)
		{
			a = best;
			return PLAYERBOT_DS_WORK_STRENGTH;
		}
		return PLAYERBOT_DS_WORK_NONE;
	}

	bool PlayerBotNeedsDsElixir(LPCHARACTER ch)
	{
		const long long spare = (long long)ch->GetGold() - GetPlayerBotReservedGold(ch);
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(PLAYERBOT_DS_TIME_ELIXIR_VNUM);
		if (!proto || spare < (long long)proto->dwGold * 3)
			return false;
		for (int kind = 0; kind < DS_SLOT_MAX; ++kind)
		{
			LPITEM worn = GetPlayerBotWornDs(ch, kind);
			if (worn && worn->GetSocket(ITEM_SOCKET_REMAIN_SEC) < PLAYERBOT_DS_ELIXIR_BELOW_SEC)
				return true;
		}
		return false;
	}

	bool HasPlayerBotAlchemistWork(LPCHARACTER ch)
	{
		if (PlayerBotNeedsDsElixir(ch))
			return true;
		for (int kind = 0; kind < DS_SLOT_MAX; ++kind)
		{
			LPITEM a = NULL, b = NULL;
			if (FindPlayerBotDsRefine(ch, kind, a, b) != PLAYERBOT_DS_WORK_NONE)
				return true;
		}
		return false;
	}

	// The shop's own price (item_proto gold), the way a player pays it.
	LPITEM BuyPlayerBotAlchemistItem(LPCHARACTER ch, DWORD vnum)
	{
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		if (!proto || proto->dwGold == 0 || (long long)ch->GetGold() - GetPlayerBotReservedGold(ch) < (long long)proto->dwGold ||
				ch->GetEmptyInventory(1) < 0)
			return NULL;
		PlayerBotChangeGold(ch, -(long long)proto->dwGold);
		LPITEM item = ch->AutoGiveItem(vnum, 1, -1, false);
		if (vnum == PLAYERBOT_DS_TIME_ELIXIR_VNUM)
			++s_kPlayerBotAlchemyStats.elixirs;
		else
			++s_kPlayerBotAlchemyStats.beans;
		return item;
	}

	int FindPlayerBotBagItem(LPCHARACTER ch, DWORD vnum)
	{
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetCell() == cell && item->GetVnum() == vnum && !item->isLocked())
				return cell;
		}
		return -1;
	}

	// One step at the Alchemist: an elixir for a stone running out, else one
	// refine. True when something was done.
	bool DoPlayerBotAlchemistStep(LPCHARACTER ch)
	{
		for (int kind = 0; kind < DS_SLOT_MAX; ++kind)
		{
			LPITEM worn = GetPlayerBotWornDs(ch, kind);
			if (!worn || worn->GetSocket(ITEM_SOCKET_REMAIN_SEC) >= PLAYERBOT_DS_ELIXIR_BELOW_SEC || !PlayerBotNeedsDsElixir(ch))
				continue;
			int cell = FindPlayerBotBagItem(ch, PLAYERBOT_DS_TIME_ELIXIR_VNUM);
			if (cell < 0 && BuyPlayerBotAlchemistItem(ch, PLAYERBOT_DS_TIME_ELIXIR_VNUM))
				cell = FindPlayerBotBagItem(ch, PLAYERBOT_DS_TIME_ELIXIR_VNUM);
			if (cell < 0)
				return false;
			const long before = worn->GetSocket(ITEM_SOCKET_REMAIN_SEC);
			const bool ok = ch->UseItem(TItemPos(INVENTORY, (WORD)cell), TItemPos(worn->GetWindow(), worn->GetCell()));
			sys_log(0, "PLAYERBOT_ALCHEMY: elixir pid=%u name=%s vnum=%u ok=%d sec=%ld->%ld gold=%lld", ch->GetPlayerID(),
					ch->GetName(), worn->GetVnum(), ok ? 1 : 0, before, (long)worn->GetSocket(ITEM_SOCKET_REMAIN_SEC),
					(long long)ch->GetGold());
			return ok;
		}
		for (int kind = 0; kind < DS_SLOT_MAX; ++kind)
		{
			LPITEM a = NULL, b = NULL;
			const int work = FindPlayerBotDsRefine(ch, kind, a, b);
			if (work == PLAYERBOT_DS_WORK_NONE)
				continue;
			if (!PullOutPlayerBotDs(ch, a) || (b && !PullOutPlayerBotDs(ch, b)))
				return false;
			TItemPos grid[DRAGON_SOUL_REFINE_GRID_SIZE];
			const DWORD vnumA = a->GetVnum();
			const long long goldBefore = ch->GetGold();
			if (work == PLAYERBOT_DS_WORK_STRENGTH)
			{
				int bean = FindPlayerBotBagItem(ch, PLAYERBOT_DS_GREEN_BEAN_VNUM);
				if (bean < 0 && BuyPlayerBotAlchemistItem(ch, PLAYERBOT_DS_GREEN_BEAN_VNUM))
					bean = FindPlayerBotBagItem(ch, PLAYERBOT_DS_GREEN_BEAN_VNUM);
				if (bean < 0)
					return false;
				PlayerBotDsRefineGrid(grid, a, ch->GetInventoryItem((WORD)bean));
			}
			else
				PlayerBotDsRefineGrid(grid, a, b);
			ch->DragonSoul_RefineWindow_Open(ch);
			bool ok = false;
			if (work == PLAYERBOT_DS_WORK_GRADE)
				ok = DSManager::instance().DoRefineGrade(ch, grid);
			else if (work == PLAYERBOT_DS_WORK_STEP)
				ok = DSManager::instance().DoRefineStep(ch, grid);
			else
				ok = DSManager::instance().DoRefineStrength(ch, grid);
			ch->DragonSoul_RefineWindow_Close();
			const char* what = work == PLAYERBOT_DS_WORK_GRADE ? "grade" : work == PLAYERBOT_DS_WORK_STEP ? "step" : "strength";
			if (work == PLAYERBOT_DS_WORK_GRADE) ++s_kPlayerBotAlchemyStats.refinesGrade;
			else if (work == PLAYERBOT_DS_WORK_STEP) ++s_kPlayerBotAlchemyStats.refinesStep;
			else ++s_kPlayerBotAlchemyStats.refinesStrength;
			if (!ok)
				++s_kPlayerBotAlchemyStats.refineFails;
			sys_log(0, "PLAYERBOT_ALCHEMY: refine %s pid=%u name=%s lv=%d kind=%d vnum=%u done=%d paid=%lld", what,
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), kind, vnumA, ok ? 1 : 0,
					goldBefore - (long long)ch->GetGold());
			return ok || goldBefore != (long long)ch->GetGold();
		}
		return false;
	}

	void EndPlayerBotAlchemistVisit(TPlayerBotAIState& state, DWORD dwNow)
	{
		state.bVisitingDsAlchemist = false;
		state.dwNextDsActionTime = 0;
		state.dwNextDsCheckTime = dwNow + number(PLAYERBOT_ALCHEMY_CHECK_MIN_MS, PLAYERBOT_ALCHEMY_CHECK_MAX_MS);
		ClearPlayerBotRoute(state, true);
	}

	// The tick: the local work (qualification, Cors, stones, deck) on a short
	// clock anywhere; the Alchemist's (elixir, refine) walked to in a first
	// village, travelled to when there is any.
	bool ManagePlayerBotAlchemy(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !IsPlayerBotAlchemyUser(ch))
		{
			if (ch && state.bVisitingDsAlchemist)
				EndPlayerBotAlchemistVisit(state, dwNow);
			return false;
		}
		if (dwNow >= state.dwNextDsLocalTime && !ch->IsDead() && !ch->GetExchange())
		{
			state.dwNextDsLocalTime = dwNow + PLAYERBOT_ALCHEMY_LOCAL_MS;
			EnsurePlayerBotAlchemyQualified(ch);
			// A Cor is a use of an item, which a fight does not refuse: opened
			// on every pass, not only between the blows.
			OpenPlayerBotCors(ch);
			// EquipItem refuses within a second and a half of a blow or a cast
			// ("You have to stand still"): the gear pass's own wait.
			if (ch->GetVictim() == NULL &&
					dwNow - ch->GetLastAttackTime() > PLAYERBOT_EQUIPMENT_COMBAT_DELAY &&
					(state.dwLastBotSkillTime == 0 || dwNow - state.dwLastBotSkillTime > PLAYERBOT_EQUIPMENT_COMBAT_DELAY))
				EquipPlayerBotBestDragonSouls(ch);
			ManagePlayerBotDsDeck(ch);
		}
		if (state.bVisitingShop || state.bVisitingBiologist || state.bVisitingHerbalist || state.bVisitingStable ||
				state.bVisitingAlchemist || state.bVisitingUriel || state.bSaddlebagErrand != 0 ||
				state.bFishingSession || state.bMarketTrip)
			return false;
		if (!state.bVisitingDsAlchemist && dwNow < state.dwNextDsCheckTime)
			return false;
		if ((ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty())) || IsPlayerBotHeldForCompany(ch) ||
				ch->GetMyShop() != NULL)
		{
			if (state.bVisitingDsAlchemist)
				EndPlayerBotAlchemistVisit(state, dwNow);
			else
				state.dwNextDsCheckTime = dwNow + number(PLAYERBOT_ALCHEMY_CHECK_MIN_MS, PLAYERBOT_ALCHEMY_CHECK_MAX_MS);
			return false;
		}

		playerbot_empire_rules::TPoint alchemist;
		if (!state.bVisitingDsAlchemist)
		{
			state.dwNextDsCheckTime = dwNow + number(PLAYERBOT_ALCHEMY_CHECK_MIN_MS, PLAYERBOT_ALCHEMY_CHECK_MAX_MS);
			if (ch->GetVictim() != NULL || state.dwTargetVID != 0 || !HasPlayerBotAlchemistWork(ch))
				return false;
			if (!playerbot_empire_rules::GetAlchemist(ch->GetMapIndex(), alchemist))
			{
				long homeMap = 0, homeX = 0, homeY = 0;
				if (!GetPlayerBotVillageReturn(ch, playerbot_empire_rules::MAP_ROLE_M1, homeMap, homeX, homeY) ||
						!IsPlayerBotMapHostedHere(homeMap) || !playerbot_empire_rules::GetAlchemist(homeMap, alchemist))
					return false;
				SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
				if (!TransitionPlayerBotMap(ch, state, homeMap, homeX, homeY, dwNow, "alchemy_to_alchemist"))
					return false;
				++s_kPlayerBotAlchemyStats.trips;
				state.dwNextDsCheckTime = 0;
				return true;
			}
			state.bVisitingDsAlchemist = true;
			state.dwNextDsActionTime = 0;
			state.bDsVisitSteps = 0;
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ch->Stop();
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_ALCHEMY: going to the Alchemist pid=%u name=%s lv=%d gold=%lld",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), (long long)ch->GetGold());
		}
		if (!playerbot_empire_rules::GetAlchemist(ch->GetMapIndex(), alchemist))
		{
			EndPlayerBotAlchemistVisit(state, dwNow);
			return false;
		}
		SetPlayerBotAction(state, BOT_ACTION_SHOP, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		long approachX = 0, approachY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), alchemist.x, alchemist.y, 0x44534f55U, approachX, approachY);
		if (DISTANCE_APPROX(ch->GetX() - approachX, ch->GetY() - approachY) > PLAYERBOT_STABLE_ARRIVE_DISTANCE)
		{
			if (!MovePlayerBotTownLeg(ch, state, dwNow, approachX, approachY, PLAYERBOT_STABLE_ARRIVE_DISTANCE) &&
					state.bStuckCounter >= 6)
			{
				sys_err("PLAYERBOT_ALCHEMY: route failed pid=%u name=%s map=%ld from=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), ch->GetX(), ch->GetY());
				EndPlayerBotAlchemistVisit(state, dwNow);
				return false;
			}
			return true;
		}
		ch->Stop();
		ch->SetPosition(POS_STANDING);
		if (state.dwNextDsActionTime == 0)
		{
			state.dwNextDsActionTime = dwNow + number(3000, 7000);
			return true;
		}
		if (dwNow < state.dwNextDsActionTime)
			return true;
		if (ch->DragonSoul_GetActiveDeck() >= 0)
			ch->DragonSoul_DeactivateAll();
		if (state.bDsVisitSteps < PLAYERBOT_ALCHEMY_VISIT_MAX_STEPS && DoPlayerBotAlchemistStep(ch))
		{
			++state.bDsVisitSteps;
			state.dwNextDsActionTime = dwNow + number(2500, 5000);
			return true;
		}
		EquipPlayerBotBestDragonSouls(ch);
		sys_log(0, "PLAYERBOT_ALCHEMY: visit over pid=%u name=%s lv=%d steps=%d worn=%d gold=%lld", ch->GetPlayerID(),
				ch->GetName(), ch->GetLevel(), (int)state.bDsVisitSteps, CountPlayerBotWornDs(ch, false),
				(long long)ch->GetGold());
		EndPlayerBotAlchemistVisit(state, dwNow);
		return false;
	}

	void LogPlayerBotAlchemyCensus()
	{
		const TPlayerBotAlchemyStats& s = s_kPlayerBotAlchemyStats;
		sys_log(0, "PLAYERBOT_ALCHEMY: census shards=%u daily_cors=%u opened=%u worn_changes=%u grade=%u step=%u strength=%u refine_fails=%u elixirs=%u beans=%u listed=%u trips=%u",
				s.shards, s.cors, s.opened, s.equipped, s.refinesGrade, s.refinesStep, s.refinesStrength,
				s.refineFails, s.elixirs, s.beans, s.listed, s.trips);
	}
}

#endif
