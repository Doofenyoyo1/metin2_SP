#ifndef __INC_METIN2_PLAYERBOT_SASH_H__
#define __INC_METIN2_PLAYERBOT_SASH_H__

// Sashes (MT2009 Plus): a bot builds one, fills it and wears it.
//
// Until now a sash was only counter goods for a bot (GetPlayerBotRareGoodsKind):
// dropped by a boss, carried to a stall, sold. Here a share of the bots keeps
// them and does what a player does with them at Uriel (NPC 20011, in each
// first village, acce_costume_uriel.quest):
//
//   * combine two sashes of the same grade (any kind: a Szarfa Mistrza goes
//     with a Krolewska Szarfa, the engine asks only the grade) - 1+1 -> 2 (5%),
//     2+2 -> 3 (10%), 3+3 -> 4 (11-19%), 4+4 -> 4 (+1..5, at most 25%); the
//     second sash is lost whether it works or not, the yang too
//     (CHARACTER::RefineAcceMaterials, item_length.h ACCE_*);
//   * absorb a weapon or a body armour into a sash of the grade its level
//     wants: the piece is gone, the sash carries its bonuses times its
//     absorption, class restrictions stay behind with the piece;
//   * wear it (WEAR_COSTUME_ACCE).
//
// Uriel's window is a client's, so the pass calls the engine's own functions
// the window's packets call (OpenAcce, AddAcceMaterial, RefineAcceMaterials,
// CloseAcce): every check and roll is the engine's. A bot's DESC is a bot
// desc, and the packets these send go nowhere.
//
// Who: PLAYERBOT_SASH_KEEPER_PERCENT of the bots of PLAYERBOT_SASH_MIN_LEVEL
// and more, picked by player id so the choice survives a restart; the rest
// keep selling sashes, so the market does not dry up. None while the world
// has sashes switched off (M2_SASHES=0 -> event flag m2_sash_off).

#if defined(ENABLE_ACCE_COSTUME_SYSTEM)

namespace
{
	const int PLAYERBOT_SASH_MIN_LEVEL = 30;
	// Four bots in five since 28 September: 660 grade-1 sashes stood on the
	// counters and 3300 more in the bags, while the keepers were done at
	// grade 2 (the operator: "boty chetniej to kupowaly i robily wyzsze
	// szarfy").
	const int PLAYERBOT_SASH_KEEPER_PERCENT = 80;
	// Unworn sashes a keeper holds for the combining; past this many the worst
	// are goods again.
	const int PLAYERBOT_SASH_KEEP = 10;
	const DWORD PLAYERBOT_SASH_CHECK_MIN_MS = 3 * 60 * 1000;
	const DWORD PLAYERBOT_SASH_CHECK_MAX_MS = 6 * 60 * 1000;
	// A visit's work: combines and one absorption, a few seconds apart.
	const int PLAYERBOT_SASH_VISIT_MAX_STEPS = 8;
	// 4+4 only from this level and for a bot with this much to spare over its
	// reserve (operator, 26 September 2026: 90 and ten million; 28 September:
	// higher sashes - 75 and six million).
	const long long PLAYERBOT_SASH_RICH_GOLD = 6000000LL;
	const int PLAYERBOT_SASH_UNIQUE_COMBINE_LEVEL = 75;
	// What goes into a sash (GetPlayerBotSashPieceValue): "tylko przedmioty
	// bliskie poziomem bota i z bonusami" (operator, 26 September 2026), and
	// then "boty wrzucaja syfiaste przedmioty do szarf, np. zbroje na 26 lvl
	// z bonusem 6%" (27 September) - the old rule, 40% of the class-neutral
	// equipment score of what the bot wore, let a body armour of 26 with one
	// line of 6% into the sash of a bot of 47. A piece is now measured by what
	// the sash would give this bot: absorption times its base attack or
	// defence, its own lines and its rolled ones, each scored for the bot's
	// class. It goes in only when
	//   - it asks a level no more than PLAYERBOT_SASH_ABSORB_LEVEL_SPAN under
	//     the bot's (higher is fine: a sash asks no level of what it holds);
	//   - it has PLAYERBOT_SASH_MIN_LINES rolled lines, or one strong one
	//     (IsPlayerBotSashStrongLine), or is refined to
	//     PLAYERBOT_SASH_MIN_PLUS_BARE at least;
	//   - it is worth PLAYERBOT_SASH_REF_PERCENT of the weapon the bot fights
	//     with, put into the same sash - the measure of what is good at its
	//     level;
	//   - it beats the sash the bot wears by PLAYERBOT_SASH_BETTER_PERCENT.
	// A worn sash under PLAYERBOT_SASH_JUNK_PERCENT of that measure is junk: the
	// sash is not done, the bot builds another and wears the better one.
	// "Prog z 90% do 75%, przyjmowac tez przedmiot z jednym mocnym bonusem" (the
	// operator, 27 September, once the rules left ~500 keepers with nothing to
	// absorb). And a level-30 weapon has no level and no measure to meet: "bronie
	// 30 lvl bez zadnych ograniczen, jak sa dobre i duze plusy to nawet bot 75 lvl
	// moze wlozyc" - its lines (as above) and PLAYERBOT_SASH_LEVEL30_MIN_PLUS
	// are the whole test, and one in the sash is never junk.
	const int PLAYERBOT_SASH_ABSORB_LEVEL_SPAN = 10;
	const int PLAYERBOT_SASH_MIN_LINES = 2;
	const int PLAYERBOT_SASH_MIN_PLUS_BARE = 7;
	const int PLAYERBOT_SASH_REF_PERCENT = 75;
	const int PLAYERBOT_SASH_LEVEL30_MIN_PLUS = 6;
	const int PLAYERBOT_SASH_BETTER_PERCENT = 110;
	const int PLAYERBOT_SASH_JUNK_PERCENT = 60;
	// A lone sash - the only one of its grade a keeper holds, waiting for a
	// pair - is goods again after this long: 260 keepers sat on one grade-1
	// sash each, the very supply the others waited for (26 September 2026).
	const DWORD PLAYERBOT_SASH_LONE_RELEASE_MS = 3 * 60 * 60 * 1000;
	// A sash bought off a counter: at most this share of the spare purse.
	const int PLAYERBOT_SASH_MARKET_PURSE_PERCENT = 40;
	// Uriel: npc.txt cells (713,605), (655,553) and (425,716) on each first
	// village's BasePosition (409600,896000), (0,102400), (921600,204800).
	const DWORD PLAYERBOT_URIEL_VNUM = 20011;

	struct TPlayerBotSashTarget
	{
		int grade;
		int absorption;
	};

	struct TPlayerBotSashStats
	{
		unsigned combines;
		unsigned combineFails;
		unsigned absorbs;
		unsigned wears;
		unsigned trips;
		unsigned bought;
	};
	TPlayerBotSashStats s_kPlayerBotSashStats = { 0, 0, 0, 0, 0, 0 };

	bool GetPlayerBotUriel(long mapIndex, playerbot_empire_rules::TPoint& out)
	{
		switch (mapIndex)
		{
			case 1:  out.x = 480900; out.y = 956500; return true;
			case 21: out.x = 65500;  out.y = 157700; return true;
			case 41: out.x = 964100; out.y = 276400; return true;
			default: return false;
		}
	}

	bool ArePlayerBotSashesOff()
	{
		return quest::CQuestManager::instance().GetEventFlag("m2_sash_off") != 0;
	}

	bool IsPlayerBotSashItem(LPITEM item)
	{
		return item && item->GetType() == ITEM_COSTUME && item->GetSubType() == COSTUME_ACCE &&
				IsPlayerBotSashVnum(item->GetVnum());
	}

	int GetPlayerBotSashGrade(LPITEM item)
	{
		return item ? (int)item->GetValue(ACCE_GRADE_VALUE_FIELD) : 0;
	}

	int GetPlayerBotSashAbsorption(LPITEM item)
	{
		return item ? (int)item->GetSocket(ACCE_ABSORPTION_SOCKET) : 0;
	}

	bool IsPlayerBotSashAbsorbed(LPITEM item)
	{
		return item && item->GetSocket(ACCE_ABSORBED_SOCKET) > 0;
	}

	// Which bots build one: a stable roll on the player id, the share scaled by
	// the panel's SASH will (playerbot_config.h; 100 = the build's 80%).
	bool IsPlayerBotSashKeeperPID(DWORD pid)
	{
		const DWORD h = (pid * 2654435761U) ^ 0x53415348U;
		return (int)((h >> 16) % 100) < PLAYERBOT_SASH_KEEPER_PERCENT * s_iPlayerBotSashPercent / 100;
	}

	bool IsPlayerBotSashKeeper(LPCHARACTER ch)
	{
		if (!ch || !ch->IsPC() || ch->GetLevel() < PLAYERBOT_SASH_MIN_LEVEL)
			return false;
		const DWORD pid = ch->GetPlayerID();
		if (!IsPlayerBotSashKeeperPID(pid) || IsPlayerBotSidekickPID(pid) ||
				CPlayerBotManager::instance().IsMedalDropperCohortPID(pid))
			return false;
		return !ArePlayerBotSashesOff();
	}

	long long GetPlayerBotSashSpareGold(LPCHARACTER ch)
	{
		return ch ? (long long)ch->GetGold() - (long long)GetPlayerBotReservedGold(ch) -
				(long long)PLAYERBOT_SHOPPING_GOLD_FLOOR : 0;
	}

	// What a sash should be at this level before the bot absorbs a piece into
	// it: the operator's table (plan of 25 September 2026).
	TPlayerBotSashTarget GetPlayerBotSashTarget(LPCHARACTER ch)
	{
		TPlayerBotSashTarget t = { 0, 0 };
		if (!ch)
			return t;
		const int level = ch->GetLevel();
		if (level < PLAYERBOT_SASH_MIN_LEVEL)
			return t;
		// A grade higher since 28 September ("robily wyzsze szarfy", the
		// operator; the grade-1 sashes the combining eats were piling up): 10%
		// from 30, any unique from 50, and with the money 4+4 up to 21% from
		// PLAYERBOT_SASH_UNIQUE_COMBINE_LEVEL (26 September 2026: 5% from 30,
		// 10% from 50, a unique from 65, 4+4 from 90).
		if (level < 50)       { t.grade = 3; t.absorption = ACCE_GRADE_3_ABS; }
		else                  { t.grade = 4; t.absorption = ACCE_GRADE_4_ABS_MIN; }
		if (level >= PLAYERBOT_SASH_UNIQUE_COMBINE_LEVEL && GetPlayerBotSashSpareGold(ch) >= PLAYERBOT_SASH_RICH_GOLD)
			t.absorption = 21;
		return t;
	}

	bool IsPlayerBotSashAtTarget(LPITEM item, const TPlayerBotSashTarget& t)
	{
		return item && t.grade > 0 && GetPlayerBotSashGrade(item) >= t.grade &&
				GetPlayerBotSashAbsorption(item) >= t.absorption;
	}

	// ------------------------------------------------ what a sash is worth

	// One line of a piece in a sash of this absorption, as the engine gives it
	// (CItem::CalcAcceBonus: a positive line is at least 1), scored for the
	// bot's class the way its gear is (ScorePlayerBotApplyTiered).
	long long ScorePlayerBotSashLine(BYTE type, long value, int absorption, LPCHARACTER ch)
	{
		if (type == APPLY_NONE || type == APPLY_SKILL || value == 0)
			return 0;
#if defined(USE_ACCE_ABSORB_WITH_NO_NEGATIVE_BONUS)
		if (value < 0)
			return 0;
#endif
		return ScorePlayerBotApplyTiered(type, CItem::CalcAcceBonus((int32_t)value, (uint32_t)absorption), ch);
	}

	// What a sash of `absorption` holding a piece of `proto` with the rolled
	// lines of `lines` gives the bot - what CItem::ModifyPoints adds for a
	// sash: a weapon's attack (the greater of its two values, plus the refine
	// value; the physical one worth what the bot's school makes of it, the
	// magic one likewise), a body armour's defence and magic defence, the
	// piece's own lines and its rolled ones, each times the absorption.
	long long GetPlayerBotSashPieceValue(LPCHARACTER ch, const TItemTable* proto, LPITEM lines, int absorption)
	{
		if (!proto || absorption <= 0)
			return 0;
		long long value = 0;
		const bool magic = IsPlayerBotMagicSchool(ch);
		if (proto->bType == ITEM_WEAPON)
		{
			if (proto->alValues[3] + proto->alValues[4] > 0)
			{
				const long attack = std::max(proto->alValues[3], proto->alValues[4]) + proto->alValues[5];
				value += (long long)CItem::CalcAcceBonus((int32_t)attack, (uint32_t)absorption) * (magic ? 60 : 300);
			}
			if (proto->alValues[1] + proto->alValues[2] > 0)
			{
				const long attack = std::max(proto->alValues[1], proto->alValues[2]) + proto->alValues[5];
				value += (long long)CItem::CalcAcceBonus((int32_t)attack, (uint32_t)absorption) * (magic ? 300 : 60);
			}
		}
		else if (proto->bType == ITEM_ARMOR && proto->bSubType == ARMOR_BODY)
		{
			value += ScorePlayerBotApplyTiered(APPLY_DEF_GRADE_BONUS,
					CItem::CalcAcceBonus((int32_t)(proto->alValues[1] + proto->alValues[5] * 2), (uint32_t)absorption), ch);
			if (proto->alValues[0] > 0)
				value += (long long)CItem::CalcAcceBonus((int32_t)proto->alValues[0], (uint32_t)absorption) * 50;
		}
		for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
			value += ScorePlayerBotSashLine(proto->aApplies[i].bType, proto->aApplies[i].lValue, absorption, ch);
		if (lines)
			for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				value += ScorePlayerBotSashLine(lines->GetAttributeType(i), lines->GetAttributeValue(i), absorption, ch);
		return value;
	}

	// A filled sash: the piece it holds (the proto of its absorbed vnum) with
	// the lines it copied from it, at its own absorption.
	long long GetPlayerBotSashValue(LPCHARACTER ch, LPITEM sash)
	{
		if (!sash || !IsPlayerBotSashItem(sash) || !IsPlayerBotSashAbsorbed(sash))
			return 0;
		return GetPlayerBotSashPieceValue(ch,
				ITEM_MANAGER::instance().GetTable((DWORD)sash->GetSocket(ACCE_ABSORBED_SOCKET)),
				sash, GetPlayerBotSashAbsorption(sash));
	}

	// The measure of a good piece at the bot's level: the weapon it fights
	// with, put into a sash of this absorption. 0 without a weapon.
	long long GetPlayerBotSashReferenceValue(LPCHARACTER ch, int absorption)
	{
		LPITEM weapon = ch ? ch->GetWear(WEAR_WEAPON) : NULL;
		return weapon ? GetPlayerBotSashPieceValue(ch, weapon->GetProto(), weapon, absorption) : 0;
	}

	bool IsPlayerBotSashGrailVnum(LPCHARACTER ch, DWORD vnum);

	// A level-30 weapon at PLAYERBOT_SASH_LEVEL30_MIN_PLUS or more (the plus is
	// the last digit of the vnum).
	bool IsPlayerBotSashLevel30Vnum(DWORD vnum)
	{
		const TItemTable* proto = vnum ? ITEM_MANAGER::instance().GetTable(vnum) : NULL;
		if (!proto || proto->bType != ITEM_WEAPON || (int)(vnum % 10) < PLAYERBOT_SASH_LEVEL30_MIN_PLUS)
			return false;
		for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
			if (proto->aLimits[i].bType == LIMIT_LEVEL)
				return proto->aLimits[i].lValue == 30;
		return false;
	}

	// One rolled line worth carrying alone: the top two grades of what
	// item_attr rolls (a stat of 8, 1000 HP, 5% attack speed, 10% cast speed,
	// 5% critical or piercing or against half-humans, 10% against a race), or
	// an average-damage line of 20% or a skill-damage line of 10%.
	bool IsPlayerBotSashStrongLine(BYTE type, long value)
	{
		switch (type)
		{
			case APPLY_STR: case APPLY_DEX: case APPLY_INT: case APPLY_CON:
				return value >= 8;
			case APPLY_MAX_HP:
				return value >= 1000;
			case APPLY_ATT_SPEED:
				return value >= 5;
			case APPLY_CAST_SPEED:
				return value >= 10;
			case APPLY_CRITICAL_PCT: case APPLY_PENETRATE_PCT: case APPLY_ATTBONUS_HUMAN:
				return value >= 5;
			case APPLY_ATTBONUS_ANIMAL: case APPLY_ATTBONUS_ORC: case APPLY_ATTBONUS_MILGYO:
			case APPLY_ATTBONUS_UNDEAD: case APPLY_ATTBONUS_DEVIL: case APPLY_ATTBONUS_MONSTER:
				return value >= 10;
			case APPLY_NORMAL_HIT_DAMAGE_BONUS:
				return value >= 20;
			case APPLY_SKILL_DAMAGE_BONUS:
				return value >= 10;
			default:
				return false;
		}
	}

	bool HasPlayerBotSashStrongLine(LPITEM item)
	{
		for (int i = 0; item && i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			if (IsPlayerBotSashStrongLine(item->GetAttributeType(i), item->GetAttributeValue(i)))
				return true;
		return false;
	}

	// A filled sash not worth keeping on: under PLAYERBOT_SASH_JUNK_PERCENT of
	// the measure at its own absorption.
	bool IsPlayerBotSashJunk(LPCHARACTER ch, LPITEM sash)
	{
		if (!sash || !IsPlayerBotSashAbsorbed(sash))
			return false;
		if (IsPlayerBotSashGrailVnum(ch, (DWORD)sash->GetSocket(ACCE_ABSORBED_SOCKET)) ||
				IsPlayerBotSashLevel30Vnum((DWORD)sash->GetSocket(ACCE_ABSORBED_SOCKET)))
			return false;
		const long long reference = GetPlayerBotSashReferenceValue(ch, GetPlayerBotSashAbsorption(sash));
		return reference > 0 && GetPlayerBotSashValue(ch, sash) * 100 < reference * PLAYERBOT_SASH_JUNK_PERCENT;
	}

	// ------------------------------------------------------ the sash's grail
	//
	// "Najbardziej pozadana bronia do wkladania do szarf jest 30 lvl luk dla
	// klas atakujacych fizycznie ... a dla klas atakujacych magicznie wachlarz
	// 30 lvl, jak najwiekszy plus i jak najwiekszy procent" (the operator, 27
	// September): the Luk z Rogu Jelenia (2150-2159, the highest physical
	// attack of its level, rolling the average-damage line) for a warrior, a
	// weapon sura and a ninja; the Wachlarz Jesiennego Wiatru (7160-7169,
	// rolling the skill-damage line) for a shaman and a magic sura. A keeper
	// with PLAYERBOT_SASH_GRAIL_MIN_GOLD to spare buys one off a counter, takes
	// it to PLAYERBOT_SASH_GRAIL_PLUS at the blacksmith - the anvil's table and
	// scrolls decide how, as for any level-30 weapon, and one the anvil burns
	// is bought again - and absorbs it ahead of any other piece. The better of
	// two is the higher plus, then the higher line.
	const long long PLAYERBOT_SASH_GRAIL_MIN_GOLD = 5000000LL;
	const DWORD PLAYERBOT_SASH_GRAIL_BOW = 2150;
	const DWORD PLAYERBOT_SASH_GRAIL_FAN = 7160;

	DWORD GetPlayerBotSashGrailFamily(LPCHARACTER ch)
	{
		return IsPlayerBotMagicSchool(ch) ? PLAYERBOT_SASH_GRAIL_FAN : PLAYERBOT_SASH_GRAIL_BOW;
	}

	BYTE GetPlayerBotSashGrailLine(LPCHARACTER ch)
	{
		return IsPlayerBotMagicSchool(ch) ? APPLY_SKILL_DAMAGE_BONUS : APPLY_NORMAL_HIT_DAMAGE_BONUS;
	}

	bool IsPlayerBotSashGrailVnum(LPCHARACTER ch, DWORD vnum)
	{
		const DWORD family = GetPlayerBotSashGrailFamily(ch);
		return ch && vnum >= family && vnum <= family + 9;
	}

	bool IsPlayerBotSashGrailItem(LPCHARACTER ch, LPITEM item)
	{
		return ch && item && item->GetType() == ITEM_WEAPON && IsPlayerBotSashGrailVnum(ch, item->GetVnum());
	}

	long long RankPlayerBotSashGrail(LPCHARACTER ch, LPITEM item)
	{
		return (long long)item->GetRefineLevel() * 1000 + SumPlayerBotItemLines(item, GetPlayerBotSashGrailLine(ch));
	}

	// The plus the grail is absorbed at: PLAYERBOT_SASH_GRAIL_PLUS, or the
	// anvil's ceiling for its roll when that is lower, or the plus it has when
	// only a scroll may raise it - a scroll the bag holds still does, first.
	BYTE GetPlayerBotSashGrailReadyPlus(LPITEM item)
	{
		if (IsPlayerBotScrollOnlyWeapon(item))
			return 0;
		return (BYTE)std::min<int>(PLAYERBOT_SASH_GRAIL_PLUS,
				GetPlayerBotLevel30AnvilCeiling(SumPlayerBotItemLines(item, APPLY_NORMAL_HIT_DAMAGE_BONUS)));
	}

	bool IsPlayerBotSashGrailReady(LPITEM item)
	{
		return item && item->GetRefineLevel() >= GetPlayerBotSashGrailReadyPlus(item);
	}

	// The keeper's grail in the bag: its best copy, a few seconds at a time
	// (the counters, the merchant and the blacksmith ask for every item).
	struct TPlayerBotSashGrailMemo { DWORD dwUntil; DWORD dwItemID; };
	std::map<DWORD, TPlayerBotSashGrailMemo> s_mapPlayerBotSashGrail;

	LPITEM FindPlayerBotSashGrailProject(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded() || !IsPlayerBotSashKeeper(ch) || ArePlayerBotSashesOff())
			return NULL;
		const DWORD now = get_dword_time();
		TPlayerBotSashGrailMemo& memo = s_mapPlayerBotSashGrail[ch->GetPlayerID()];
		if (memo.dwUntil != 0 && (int)(now - memo.dwUntil) < 0)
		{
			if (!memo.dwItemID)
				return NULL;
			LPITEM held = ITEM_MANAGER::instance().Find(memo.dwItemID);
			if (held && held->GetOwner() == ch && held->GetWindow() == INVENTORY && !held->IsEquipped())
				return held;
		}
		memo.dwUntil = now + 3000;
		memo.dwItemID = 0;
		LPITEM best = NULL;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->IsEquipped() || !IsPlayerBotSashGrailItem(ch, item))
				continue;
			if (!best || RankPlayerBotSashGrail(ch, item) > RankPlayerBotSashGrail(ch, best))
				best = item;
		}
		memo.dwItemID = best ? best->GetID() : 0;
		return best;
	}

	bool IsPlayerBotSashGrailProject(LPCHARACTER ch, LPITEM item)
	{
		return ch && item && IsPlayerBotSashGrailItem(ch, item) && FindPlayerBotSashGrailProject(ch) == item;
	}

	// The worn sash already holds the grail of its school.
	bool PlayerBotWornSashHoldsGrail(LPCHARACTER ch)
	{
		LPITEM worn = ch ? ch->GetWear(WEAR_COSTUME_ACCE) : NULL;
		return worn && IsPlayerBotSashItem(worn) && IsPlayerBotSashAbsorbed(worn) &&
				IsPlayerBotSashGrailVnum(ch, (DWORD)worn->GetSocket(ACCE_ABSORBED_SOCKET));
	}

	// A rich keeper without one - neither in the bag nor in its sash.
	bool PlayerBotNeedsSashGrail(LPCHARACTER ch)
	{
		if (!ch || !IsPlayerBotSashKeeper(ch) || ArePlayerBotSashesOff() ||
				GetPlayerBotSashTarget(ch).grade <= 0)
			return false;
		const long long spare = GetPlayerBotSashSpareGold(ch);
		const bool needs = spare >= PLAYERBOT_SASH_GRAIL_MIN_GOLD &&
				!PlayerBotWornSashHoldsGrail(ch) && FindPlayerBotSashGrailProject(ch) == NULL;
		if (needs)
			PlayerBotLogThrottled("sash_grail_needs", get_dword_time(),
					"PLAYERBOT_SASH: grail wanted pid=%u name=%s lv=%d spare=%lld",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), spare);
		return needs;
	}

	// A counter's grail worth the money: the school's family with its line
	// rolled up, not down.
	bool WantsPlayerBotSashGrailOffer(LPCHARACTER ch, LPITEM offer)
	{
		return IsPlayerBotSashGrailItem(ch, offer) && PlayerBotNeedsSashGrail(ch) &&
				SumPlayerBotItemLines(offer, GetPlayerBotSashGrailLine(ch)) > 0;
	}

	// Which sash to wear: a filled one by what it gives this bot, over an
	// empty one by its absorption.
	long long RankPlayerBotSashFor(LPCHARACTER ch, LPITEM item)
	{
		if (!item)
			return -1;
		if (IsPlayerBotSashAbsorbed(item))
			return (1LL << 40) + GetPlayerBotSashValue(ch, item);
		return GetPlayerBotSashAbsorption(item);
	}

	// Which sash is the better one to wear: a filled one over an empty one,
	// then the absorption.
	int RankPlayerBotSash(LPITEM item)
	{
		if (!item)
			return -1;
		return (IsPlayerBotSashAbsorbed(item) ? 1000 : 0) + GetPlayerBotSashAbsorption(item);
	}

	bool IsPlayerBotSashDone(LPCHARACTER ch, const TPlayerBotSashTarget& t)
	{
		LPITEM worn = ch ? ch->GetWear(WEAR_COSTUME_ACCE) : NULL;
		return worn && IsPlayerBotSashItem(worn) && IsPlayerBotSashAbsorbed(worn) &&
				IsPlayerBotSashAtTarget(worn, t) && !IsPlayerBotSashJunk(ch, worn);
	}

	bool IsPlayerBotSashUsable(LPITEM item)
	{
		return IsPlayerBotSashItem(item) && !item->IsEquipped() && !item->isLocked() &&
				!item->IsExchanging();
	}

	void CollectPlayerBotBagSashes(LPCHARACTER ch, std::vector<LPITEM>& out)
	{
		out.clear();
		if (!ch || !ch->IsItemLoaded())
			return;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetCell() == cell && IsPlayerBotSashUsable(item))
				out.push_back(item);
		}
	}

	// A lone sash: empty, under the target grade, and the only one of its
	// grade in the bag - it waits for a pair. After
	// PLAYERBOT_SASH_LONE_RELEASE_MS alone it is released: goods again, for
	// the keeper that has the other one (IsPlayerBotKeptSash), and never taken
	// back off the counter (FindPlayerBotShopUnwantedLine).
	std::map<DWORD, DWORD> s_mapPlayerBotLoneSashSince;
	std::set<DWORD> s_setPlayerBotReleasedSash;

	bool IsPlayerBotSashLone(LPCHARACTER ch, LPITEM item, const TPlayerBotSashTarget& t)
	{
		if (!IsPlayerBotSashUsable(item) || IsPlayerBotSashAbsorbed(item))
			return false;
		const int grade = GetPlayerBotSashGrade(item);
		if (grade >= t.grade)
			return false;
		std::vector<LPITEM> bag;
		CollectPlayerBotBagSashes(ch, bag);
		for (size_t i = 0; i < bag.size(); ++i)
			if (bag[i] != item && !IsPlayerBotSashAbsorbed(bag[i]) && GetPlayerBotSashGrade(bag[i]) == grade)
				return false;
		return true;
	}

	// The clocks, on the keeper's sash look (ManagePlayerBotSash).
	void NotePlayerBotLoneSashes(LPCHARACTER ch, const TPlayerBotSashTarget& t, DWORD dwNow)
	{
		std::vector<LPITEM> bag;
		CollectPlayerBotBagSashes(ch, bag);
		for (size_t i = 0; i < bag.size(); ++i)
		{
			if (IsPlayerBotSashLone(ch, bag[i], t))
				s_mapPlayerBotLoneSashSince.insert(std::make_pair(bag[i]->GetID(), dwNow));
			else
				s_mapPlayerBotLoneSashSince.erase(bag[i]->GetID());
		}
	}

	bool IsPlayerBotSashReleased(DWORD itemId)
	{
		return s_setPlayerBotReleasedSash.count(itemId) != 0;
	}

	// Kept, not goods: a keeper's sashes while its sash is not done, the best
	// PLAYERBOT_SASH_KEEP of those at or under its target grade; once done,
	// only one that would beat what it wears. The worn one is never goods.
	bool IsPlayerBotSashPieceKind(LPITEM item);
	DWORD GetPlayerBotChosenSashPieceID(LPCHARACTER ch);
	bool IsPlayerBotSashGrailProject(LPCHARACTER ch, LPITEM item);

	bool IsPlayerBotKeptSash(LPCHARACTER ch, LPITEM item)
	{
		// The piece chosen for the sash to fill waits for the Uriel visit, and
		// the grail for the blacksmith and then Uriel.
		if (ch && IsPlayerBotSashPieceKind(item) && !item->IsEquipped() && IsPlayerBotSashKeeper(ch))
			return IsPlayerBotSashGrailProject(ch, item) || GetPlayerBotChosenSashPieceID(ch) == item->GetID();
		if (!ch || !IsPlayerBotSashItem(item))
			return false;
		if (item->IsEquipped())
			return true;
		if (!IsPlayerBotSashKeeper(ch))
			return false;
		const TPlayerBotSashTarget t = GetPlayerBotSashTarget(ch);
		if (t.grade <= 0)
			return false;
		if (IsPlayerBotSashDone(ch, t))
			return RankPlayerBotSashFor(ch, item) > RankPlayerBotSashFor(ch, ch->GetWear(WEAR_COSTUME_ACCE));
		if (IsPlayerBotSashAbsorbed(item))
			return RankPlayerBotSashFor(ch, item) > RankPlayerBotSashFor(ch, ch->GetWear(WEAR_COSTUME_ACCE));
		if (GetPlayerBotSashGrade(item) > t.grade)
			return true;
		if (IsPlayerBotSashReleased(item->GetID()))
			return false;
		{
			std::map<DWORD, DWORD>::const_iterator lone = s_mapPlayerBotLoneSashSince.find(item->GetID());
			if (lone != s_mapPlayerBotLoneSashSince.end() &&
					get_dword_time() - lone->second >= PLAYERBOT_SASH_LONE_RELEASE_MS && IsPlayerBotSashLone(ch, item, t))
			{
				s_setPlayerBotReleasedSash.insert(item->GetID());
				s_mapPlayerBotLoneSashSince.erase(lone);
				sys_log(0, "PLAYERBOT_SASH: lone released pid=%u name=%s lv=%d vnum=%u grade=%d",
						ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), item->GetVnum(), GetPlayerBotSashGrade(item));
				return false;
			}
		}
		const int grade = GetPlayerBotSashGrade(item);
		const int absorption = GetPlayerBotSashAbsorption(item);
		int better = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM other = ch->GetInventoryItem(cell);
			if (!other || other == item || other->GetCell() != cell || !IsPlayerBotSashItem(other) ||
					IsPlayerBotSashAbsorbed(other))
				continue;
			const int g = GetPlayerBotSashGrade(other);
			if (g > grade || (g == grade && GetPlayerBotSashAbsorption(other) > absorption) ||
					(g == grade && GetPlayerBotSashAbsorption(other) == absorption && other->GetID() < item->GetID()))
				++better;
		}
		return better < PLAYERBOT_SASH_KEEP;
	}

	// A counter's sash worth a keeper's money: an empty one of a grade it still
	// combines, or one already at its target grade and absorption.
	bool WantsPlayerBotSashOffer(LPCHARACTER ch, LPITEM offer)
	{
		if (!IsPlayerBotSashItem(offer) || IsPlayerBotSashAbsorbed(offer) || !IsPlayerBotSashKeeper(ch))
			return false;
		const TPlayerBotSashTarget t = GetPlayerBotSashTarget(ch);
		if (t.grade <= 0 || IsPlayerBotSashDone(ch, t))
			return false;
		std::vector<LPITEM> bag;
		CollectPlayerBotBagSashes(ch, bag);
		if ((int)bag.size() >= PLAYERBOT_SASH_KEEP)
			return false;
		// A finished-grade sash in the bag waits only for a piece to absorb.
		for (size_t i = 0; i < bag.size(); ++i)
			if (!IsPlayerBotSashAbsorbed(bag[i]) && IsPlayerBotSashAtTarget(bag[i], t))
				return false;
		const int grade = GetPlayerBotSashGrade(offer);
		return grade < t.grade || IsPlayerBotSashAtTarget(offer, t);
	}

	// ------------------------------------------------------------ prices
	//
	// A sash costs what it takes to make, on average, plus
	// PLAYERBOT_SASH_PRICE_MARGIN_PERCENT - the way the Dragon Stones are
	// priced ("tak jak cory, ze 25% drozej niz koszt wyrobienia", operator,
	// 26 September 2026). A grade-1 sash, the boss's drop, stands at
	// PLAYERBOT_SASH_PRICE; each grade above is made at Uriel from two of the
	// grade under it: on a failure the second one and the fee are gone and the
	// first stays, so one of grade g+1 costs
	//   cost(g) + (cost(g) + fee(g)) / chance(g)
	// with the engine's fees and chances (item_length.h ACCE_GRADE_*_PRICE,
	// ACCE_COMBINE_GRADE_*): 0.7M, 1.7M, 4.41M, 13.84M. A unique's is by its
	// absorption: 3+3 gives 12-19% (15.5% on average) for cost(4), and past
	// 19% every point is a third of a 4+4 success, (cost(4) + fee(4)) / 30%.
	// On the yang rate and inflation like the bots' other goods, and not
	// marked down (FindPlayerBotShopUnwantedLine's reprice). One flat 700 000
	// for every grade put a unique at a grade-1's price and let the sale
	// memory drive a grade 1 to 91 million.
	const int PLAYERBOT_SASH_PRICE_MARGIN_PERCENT = 25;

	double GetPlayerBotSashGradeCost(int grade)
	{
		static double s_cost[5];
		static bool s_built = false;
		if (!s_built)
		{
			const double fee[5] = { 0.0, (double)ACCE_GRADE_1_PRICE, (double)ACCE_GRADE_2_PRICE,
					(double)ACCE_GRADE_3_PRICE, (double)ACCE_GRADE_4_PRICE };
			const double chance[5] = { 1.0, ACCE_COMBINE_GRADE_1 / 100.0, ACCE_COMBINE_GRADE_2 / 100.0,
					ACCE_COMBINE_GRADE_3 / 100.0, ACCE_COMBINE_GRADE_4 / 100.0 };
			s_cost[0] = 0.0;
			s_cost[1] = (double)PLAYERBOT_SASH_PRICE;
			for (int g = 1; g < 4; ++g)
				s_cost[g + 1] = s_cost[g] + (s_cost[g] + fee[g]) / chance[g];
			s_built = true;
		}
		return s_cost[std::max(1, std::min(grade, 4))];
	}

	DWORD GetPlayerBotSashPrice(LPITEM item)
	{
		if (!IsPlayerBotSashItem(item))
			return 0;
		const int grade = std::max(1, std::min(GetPlayerBotSashGrade(item), 4));
		double cost = GetPlayerBotSashGradeCost(grade);
		if (grade == 4)
		{
			const double average = (ACCE_GRADE_4_ABS_MIN + 1 + ACCE_GRADE_4_ABS_MAX_COMB) / 2.0;
			const int abs = std::max(GetPlayerBotSashAbsorption(item), (int)ACCE_GRADE_4_ABS_MIN);
			const double perPoint = (cost + (double)ACCE_GRADE_4_PRICE) / (ACCE_COMBINE_GRADE_4 / 100.0) /
					((1 + ACCE_GRADE_4_ABS_RANGE) / 2.0);
			if (abs <= ACCE_GRADE_4_ABS_MAX_COMB)
				cost = cost * abs / average;
			else
				cost = cost * ACCE_GRADE_4_ABS_MAX_COMB / average + (abs - ACCE_GRADE_4_ABS_MAX_COMB) * perPoint;
		}
		long long price = (long long)(cost * (100 + PLAYERBOT_SASH_PRICE_MARGIN_PERCENT) / 100.0);
		price = (long long)ScalePlayerBotIwakuraPrice((DWORD)std::min<long long>(price, 0xFFFFFFFFLL));
		price = (price + 500) / 1000 * 1000;
		return (DWORD)std::min<long long>(std::max<long long>(price, 1000), GOLD_MAX - 1000);
	}

	// Within a keeper's purse share, and never over a fifth above the price
	// (a counter's 91 million for a grade-1 was bought at that).
	bool CanPlayerBotPayForSashOffer(LPCHARACTER ch, LPITEM offer, long long price)
	{
		const long long spare = GetPlayerBotSashSpareGold(ch);
		const long long fair = (long long)GetPlayerBotSashPrice(offer);
		return price > 0 && price <= spare * PLAYERBOT_SASH_MARKET_PURSE_PERCENT / 100 &&
				(fair <= 0 || price <= fair * 12 / 10);
	}

	// Asked before a market walk, without reading a counter: a keeper short of
	// sashes while some counter carries one.
	bool PlayerBotWantsSashFromMarket(LPCHARACTER ch)
	{
		if (!IsPlayerBotSashKeeper(ch))
			return false;
		const TPlayerBotSashTarget t = GetPlayerBotSashTarget(ch);
		if (t.grade <= 0 || IsPlayerBotSashDone(ch, t))
			return false;
		std::vector<LPITEM> bag;
		CollectPlayerBotBagSashes(ch, bag);
		if ((int)bag.size() >= PLAYERBOT_SASH_KEEP || GetPlayerBotSashSpareGold(ch) < 2500000LL)
			return false;
		for (size_t i = 0; i < bag.size(); ++i)
			if (!IsPlayerBotSashAbsorbed(bag[i]) && IsPlayerBotSashAtTarget(bag[i], t))
				return false;
		for (DWORD vnum = 85001; vnum <= 85104; ++vnum)
		{
			if (vnum == 85025)
				vnum = 85101;
			const TPlayerBotMarketLedgerEntry* e = GetPlayerBotMarketLedgerEntry(vnum);
			if (e && e->dwSupplyUnits > 0)
				return true;
		}
		return false;
	}

	void NotePlayerBotSashBought(LPCHARACTER ch, DWORD vnum, long long price)
	{
		++s_kPlayerBotSashStats.bought;
		sys_log(0, "PLAYERBOT_SASH: bought pid=%u name=%s vnum=%u price=%lld",
				ch ? ch->GetPlayerID() : 0, ch ? ch->GetName() : "", vnum, price);
	}

	// ------------------------------------------------------------ the work

	// A sash to absorb into: empty and at the target - from the bag, or the
	// empty one the bot already wears.
	LPITEM FindPlayerBotSashToFill(LPCHARACTER ch, const TPlayerBotSashTarget& t)
	{
		LPITEM best = NULL;
		std::vector<LPITEM> bag;
		CollectPlayerBotBagSashes(ch, bag);
		for (size_t i = 0; i < bag.size(); ++i)
			if (!IsPlayerBotSashAbsorbed(bag[i]) && IsPlayerBotSashAtTarget(bag[i], t) &&
					(!best || RankPlayerBotSash(bag[i]) > RankPlayerBotSash(best)))
				best = bag[i];
		LPITEM worn = ch->GetWear(WEAR_COSTUME_ACCE);
		if (worn && IsPlayerBotSashItem(worn) && !IsPlayerBotSashAbsorbed(worn) &&
				IsPlayerBotSashAtTarget(worn, t) && (!best || RankPlayerBotSash(worn) > RankPlayerBotSash(best)))
			best = worn;
		return best;
	}

	// Two sashes to combine: the lowest grade under the target with a pair,
	// else (rich, level 75+) two uniques under the target absorption. First
	// material the better one - it is the one a failure leaves.
	bool FindPlayerBotSashPair(LPCHARACTER ch, const TPlayerBotSashTarget& t,
			LPITEM& first, LPITEM& second)
	{
		first = second = NULL;
		// Nothing more to combine once the sash is done, or while one at the
		// target waits for a piece to absorb: more would be money for goods.
		if (IsPlayerBotSashDone(ch, t) || FindPlayerBotSashToFill(ch, t))
			return false;
		std::vector<LPITEM> bag;
		CollectPlayerBotBagSashes(ch, bag);
		const long long spare = GetPlayerBotSashSpareGold(ch);
		for (int grade = 1; grade <= 4; ++grade)
		{
			if (grade >= t.grade && !(grade == 4 && t.grade == 4))
				break;
			LPITEM best = NULL, worst = NULL;
			int count = 0;
			for (size_t i = 0; i < bag.size(); ++i)
			{
				LPITEM s = bag[i];
				if (GetPlayerBotSashGrade(s) != grade || GetPlayerBotSashAbsorption(s) >= ACCE_GRADE_4_ABS_MAX)
					continue;
				++count;
				if (!best || RankPlayerBotSash(s) > RankPlayerBotSash(best))
					best = s;
			}
			if (count < 2)
				continue;
			for (size_t i = 0; i < bag.size(); ++i)
			{
				LPITEM s = bag[i];
				if (s == best || GetPlayerBotSashGrade(s) != grade || IsPlayerBotSashAbsorbed(s) ||
						GetPlayerBotSashAbsorption(s) >= ACCE_GRADE_4_ABS_MAX)
					continue;
				if (!worst || RankPlayerBotSash(s) < RankPlayerBotSash(worst))
					worst = s;
			}
			if (!worst)
				continue;
			if (grade == 4)
			{
				// Uniques: only from level 90, with money to lose at 30%, and
				// while the better one is under the target.
				if (ch->GetLevel() < PLAYERBOT_SASH_UNIQUE_COMBINE_LEVEL || spare < PLAYERBOT_SASH_RICH_GOLD ||
						GetPlayerBotSashAbsorption(best) >= t.absorption)
					continue;
			}
			if (spare < (long long)ch->GetAcceCombinePrice(grade))
				return false;
			first = best;
			second = worst;
			return true;
		}
		return false;
	}

	bool IsPlayerBotSashPieceKind(LPITEM item)
	{
		if (!item || !item->GetProto())
			return false;
		if (item->GetType() == ITEM_WEAPON)
			return item->GetSubType() != WEAPON_ARROW
#if defined(ENABLE_QUIVER_SYSTEM)
					&& item->GetSubType() != WEAPON_QUIVER
#endif
					;
		return item->GetType() == ITEM_ARMOR && item->GetSubType() == ARMOR_BODY;
	}

	// What `item` would be worth in a sash of `absorption` under the rules
	// above (PLAYERBOT_SASH_*), or -1 when it does not pass them. `reference`
	// and `wornValue` are the bot's measure and its worn sash's worth.
	long long GetPlayerBotSashPieceWorth(LPCHARACTER ch, LPITEM item, int absorption,
			long long reference, long long wornValue)
	{
		if (!IsPlayerBotSashPieceKind(item))
			return -1;
		// Lines to carry, or a refine that makes the bare piece worth it.
		const bool lines = item->GetAttributeCount() >= PLAYERBOT_SASH_MIN_LINES ||
				HasPlayerBotSashStrongLine(item);
		if (!lines && (int)(item->GetVnum() % 10) < PLAYERBOT_SASH_MIN_PLUS_BARE)
			return -1;
		// A good level-30 weapon at its plus goes in at any level.
		const bool level30 = lines && IsPlayerBotSashLevel30Vnum(item->GetVnum());
		if (!level30 && (int)item->GetLevelLimit() < ch->GetLevel() - PLAYERBOT_SASH_ABSORB_LEVEL_SPAN)
			return -1;
		const long long value = GetPlayerBotSashPieceValue(ch, item->GetProto(), item, absorption);
		if (!level30 && value * 100 < reference * PLAYERBOT_SASH_REF_PERCENT)
			return -1;
		if (wornValue > 0 && value * 100 < wornValue * PLAYERBOT_SASH_BETTER_PERCENT)
			return -1;
		return value;
	}

	// The worn sash's worth for the rules, unless it is the one being filled.
	long long GetPlayerBotWornSashValue(LPCHARACTER ch, LPITEM sash)
	{
		LPITEM worn = ch ? ch->GetWear(WEAR_COSTUME_ACCE) : NULL;
		return (worn && worn != sash) ? GetPlayerBotSashValue(ch, worn) : 0;
	}

	// One the bot would put on is not spare: the gear pass has it.
	bool IsPlayerBotSashPieceWorn(LPCHARACTER ch, LPITEM item)
	{
		if (!IsPlayerBotEquipmentCandidate(ch, item) || item->GetLevelLimit() > ch->GetLevel())
			return false;
		LPITEM worn = item->GetType() == ITEM_WEAPON ? ch->GetWear(WEAR_WEAPON) : ch->GetWear(WEAR_BODY);
		return !worn || GetPlayerBotEquipmentScore(item, ch) > GetPlayerBotEquipmentScore(worn, ch);
	}

	// The piece to absorb into `sash`: a weapon or a body armour in the bag
	// the bot will not wear and does not keep for anything else, of any
	// class's (the sash does not ask), that passes the rules above - the one
	// the sash would make most of.
	LPITEM FindPlayerBotSashAbsorbPiece(LPCHARACTER ch, LPITEM sash)
	{
		if (!ch || !ch->IsItemLoaded() || !sash)
			return NULL;
		const int absorption = GetPlayerBotSashAbsorption(sash);
		if (absorption <= 0)
			return NULL;
		const long long reference = GetPlayerBotSashReferenceValue(ch, absorption);
		const long long wornValue = GetPlayerBotWornSashValue(ch, sash);
		// The grail first, once it is at its plus (and not over a grail it
		// already wears); a grail still at the blacksmith keeps the sash waiting.
		LPITEM grail = FindPlayerBotSashGrailProject(ch);
		if (grail && !grail->isLocked() && !grail->IsExchanging())
		{
			if (!IsPlayerBotSashGrailReady(grail))
				return NULL;
			if (!PlayerBotWornSashHoldsGrail(ch) || GetPlayerBotSashPieceValue(ch, grail->GetProto(), grail, absorption) * 100 >=
					wornValue * PLAYERBOT_SASH_BETTER_PERCENT)
			{
				PlayerBotLogThrottled("sash_grail", get_dword_time(),
						"PLAYERBOT_SASH: grail chosen pid=%u name=%s lv=%d piece=%u line=%ld abs=%d",
						ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), grail->GetVnum(),
						SumPlayerBotItemLines(grail, GetPlayerBotSashGrailLine(ch)), absorption);
				return grail;
			}
		}
		LPITEM best = NULL;
		long long bestValue = 0;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->IsEquipped() || item->isLocked() ||
					item->IsExchanging() || !IsPlayerBotSashPieceKind(item) || item == grail)
				continue;
			if (IsPlayerBotLppKeptItem(ch, item) || IsPlayerBotKeptBackupArmour(ch, item) ||
					IsPlayerBotSidekickGift(ch, item) || IsPlayerBotSidekickPinned(ch, item) ||
					IsPlayerBotSashPieceWorn(ch, item))
				continue;
			const long long value = GetPlayerBotSashPieceWorth(ch, item, absorption, reference, wornValue);
			if (value < 0)
				continue;
			if (!best || value > bestValue)
			{
				best = item;
				bestValue = value;
			}
		}
		if (best)
			PlayerBotLogThrottled("sash_piece", get_dword_time(),
					"PLAYERBOT_SASH: piece chosen pid=%u name=%s lv=%d piece=%u lines=%d value=%lld reference=%lld worn=%lld abs=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), best->GetVnum(), best->GetAttributeCount(),
					bestValue, reference, wornValue, absorption);
		return best;
	}

	// The bag's chosen piece, a few seconds at a time: the counters and the
	// merchant ask for every item of the bag (IsPlayerBotKeptSash).
	struct TPlayerBotSashPieceMemo { DWORD dwUntil; DWORD dwItemID; };
	std::map<DWORD, TPlayerBotSashPieceMemo> s_mapPlayerBotSashPiece;

	DWORD GetPlayerBotChosenSashPieceID(LPCHARACTER ch)
	{
		const DWORD now = get_dword_time();
		TPlayerBotSashPieceMemo& memo = s_mapPlayerBotSashPiece[ch->GetPlayerID()];
		if (memo.dwUntil != 0 && (int)(now - memo.dwUntil) < 0)
			return memo.dwItemID;
		memo.dwUntil = now + 3000;
		memo.dwItemID = 0;
		const TPlayerBotSashTarget t = GetPlayerBotSashTarget(ch);
		LPITEM sash = t.grade > 0 ? FindPlayerBotSashToFill(ch, t) : NULL;
		LPITEM piece = sash ? FindPlayerBotSashAbsorbPiece(ch, sash) : NULL;
		memo.dwItemID = piece ? piece->GetID() : 0;
		return memo.dwItemID;
	}

	// ------------------------------------------------ a piece off a counter

	// A keeper with an empty sash at its target and nothing in the bag for it
	// buys the piece on the market, the way players do: a weapon or a body
	// armour passing the same rules, from the sash's share of the purse and
	// at no more than half again its asking price.
	bool WantsPlayerBotSashPieceOffer(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !offer || !IsPlayerBotSashPieceKind(offer) || !IsPlayerBotSashKeeper(ch) ||
				ArePlayerBotSashesOff())
			return false;
		if (WantsPlayerBotSashGrailOffer(ch, offer))
		{
			PlayerBotLogThrottled("sash_grail_offer", get_dword_time(),
					"PLAYERBOT_SASH: wants the grail off a counter pid=%u name=%s lv=%d piece=%u line=%ld",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), offer->GetVnum(),
					SumPlayerBotItemLines(offer, GetPlayerBotSashGrailLine(ch)));
			return true;
		}
		const TPlayerBotSashTarget t = GetPlayerBotSashTarget(ch);
		if (t.grade <= 0)
			return false;
		LPITEM sash = FindPlayerBotSashToFill(ch, t);
		if (!sash || GetPlayerBotChosenSashPieceID(ch) != 0 || IsPlayerBotSashPieceWorn(ch, offer))
			return false;
		const int absorption = GetPlayerBotSashAbsorption(sash);
		const long long worth = GetPlayerBotSashPieceWorth(ch, offer, absorption,
				GetPlayerBotSashReferenceValue(ch, absorption), GetPlayerBotWornSashValue(ch, sash));
		if (worth >= 0)
			PlayerBotLogThrottled("sash_piece_offer", get_dword_time(),
					"PLAYERBOT_SASH: wants a piece off a counter pid=%u name=%s lv=%d piece=%u lines=%d value=%lld abs=%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), offer->GetVnum(), offer->GetAttributeCount(),
					worth, absorption);
		return worth >= 0;
	}

	bool CanPlayerBotPayForSashPiece(LPCHARACTER ch, LPITEM offer, long long price)
	{
		const long long spare = GetPlayerBotSashSpareGold(ch);
		const long long fair = (long long)GetPlayerBotShopAskingPrice(offer);
		return price > 0 && price <= spare * PLAYERBOT_SASH_MARKET_PURSE_PERCENT / 100 &&
				(fair <= 0 || price <= fair * 15 / 10);
	}

	// Asked before a market walk: a keeper with a sash to fill and no piece.
	bool PlayerBotWantsSashPieceFromMarket(LPCHARACTER ch)
	{
		if (PlayerBotNeedsSashGrail(ch))
			return true;
		if (!ch || !IsPlayerBotSashKeeper(ch) || ArePlayerBotSashesOff() ||
				GetPlayerBotSashSpareGold(ch) < 2000000LL)
			return false;
		const TPlayerBotSashTarget t = GetPlayerBotSashTarget(ch);
		return t.grade > 0 && FindPlayerBotSashToFill(ch, t) && GetPlayerBotChosenSashPieceID(ch) == 0;
	}

	bool HasPlayerBotSashWork(LPCHARACTER ch, const TPlayerBotSashTarget& t)
	{
		LPITEM a = NULL, b = NULL;
		if (FindPlayerBotSashPair(ch, t, a, b))
			return true;
		LPITEM sash = FindPlayerBotSashToFill(ch, t);
		return sash && FindPlayerBotSashAbsorbPiece(ch, sash);
	}

	// Takes the worn sash off into the bag, for the window: it refuses an
	// equipped one.
	bool TakeOffPlayerBotSash(LPCHARACTER ch, LPITEM sash)
	{
		if (!sash || !sash->IsEquipped())
			return true;
		if (ch->GetEmptyInventory(sash->GetSize()) < 0)
			return false;
		return ch->UnequipItem(sash) && !sash->IsEquipped();
	}

	bool CombinePlayerBotSashes(LPCHARACTER ch, LPITEM first, LPITEM second)
	{
		const DWORD firstId = first->GetID();
		const DWORD secondId = second->GetID();
		const WORD firstCell = first->GetCell();
		const int grade = GetPlayerBotSashGrade(first);
		const int oldAbs = GetPlayerBotSashAbsorption(first);
		const DWORD firstVnum = first->GetVnum();
		const long long goldBefore = ch->GetGold();

		ch->OpenAcce(true);
		ch->AddAcceMaterial(TItemPos(INVENTORY, first->GetCell()), 0);
		ch->AddAcceMaterial(TItemPos(INVENTORY, second->GetCell()), 1);
		std::vector<LPITEM> mats = ch->GetAcceMaterials();
		if (mats.size() < 2 || mats[0] != first || mats[1] != second)
		{
			ch->CloseAcce();
			sys_err("PLAYERBOT_SASH: combine refused pid=%u name=%s first=%u second=%u grade=%d",
					ch->GetPlayerID(), ch->GetName(), firstVnum, second->GetVnum(), grade);
			return false;
		}
		ch->RefineAcceMaterials();
		ch->CloseAcce();

		// Neither pointer is to be trusted now: read the bag.
		LPITEM result = ch->GetInventoryItem(firstCell);
		const bool secondGone = ITEM_MANAGER::instance().Find(secondId) == NULL;
		const long long paid = goldBefore - (long long)ch->GetGold();
		if (result && result->GetID() != firstId && IsPlayerBotSashItem(result))
		{
			++s_kPlayerBotSashStats.combines;
			sys_log(0, "PLAYERBOT_SASH: combine ok pid=%u name=%s lv=%d grade=%d->%d vnum=%u->%u abs=%d->%d paid=%lld",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), grade, GetPlayerBotSashGrade(result),
					firstVnum, result->GetVnum(), oldAbs, GetPlayerBotSashAbsorption(result), paid);
			return true;
		}
		if (secondGone && paid > 0)
		{
			++s_kPlayerBotSashStats.combineFails;
			sys_log(0, "PLAYERBOT_SASH: combine fail pid=%u name=%s lv=%d grade=%d vnum=%u paid=%lld",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), grade, firstVnum, paid);
			return true;
		}
		sys_err("PLAYERBOT_SASH: combine did nothing pid=%u name=%s grade=%d gold=%lld",
				ch->GetPlayerID(), ch->GetName(), grade, (long long)ch->GetGold());
		return false;
	}

	bool AbsorbIntoPlayerBotSash(LPCHARACTER ch, LPITEM sash, LPITEM piece)
	{
		if (!TakeOffPlayerBotSash(ch, sash))
			return false;
		const DWORD sashId = sash->GetID();
		const DWORD pieceId = piece->GetID();
		const DWORD pieceVnum = piece->GetVnum();
		const int pieceRefine = piece->GetRefineLevel();

		ch->OpenAcce(false);
		ch->AddAcceMaterial(TItemPos(INVENTORY, sash->GetCell()), 0);
		ch->AddAcceMaterial(TItemPos(INVENTORY, piece->GetCell()), 1);
		std::vector<LPITEM> mats = ch->GetAcceMaterials();
		if (mats.size() < 2 || mats[0] != sash || mats[1] != piece)
		{
			ch->CloseAcce();
			sys_err("PLAYERBOT_SASH: absorb refused pid=%u name=%s sash=%u piece=%u",
					ch->GetPlayerID(), ch->GetName(), sash->GetVnum(), pieceVnum);
			return false;
		}
		ch->RefineAcceMaterials();
		ch->CloseAcce();

		LPITEM filled = ITEM_MANAGER::instance().Find(sashId);
		if (filled && IsPlayerBotSashAbsorbed(filled) && ITEM_MANAGER::instance().Find(pieceId) == NULL)
		{
			++s_kPlayerBotSashStats.absorbs;
			sys_log(0, "PLAYERBOT_SASH: absorb pid=%u name=%s lv=%d sash=%u grade=%d abs=%d piece=%u+%d",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), filled->GetVnum(),
					GetPlayerBotSashGrade(filled), GetPlayerBotSashAbsorption(filled), pieceVnum, pieceRefine);
			return true;
		}
		sys_err("PLAYERBOT_SASH: absorb did nothing pid=%u name=%s sash=%u piece=%u",
				ch->GetPlayerID(), ch->GetName(), filled ? filled->GetVnum() : 0, pieceVnum);
		return false;
	}

	// Puts on the best sash the bot has: a filled one, or the empty one at its
	// target that waits for a piece (a look until then). An empty one under
	// the target stays in the bag for the combining.
	bool WearPlayerBotBestSash(LPCHARACTER ch, const TPlayerBotSashTarget& t)
	{
		if (!ch || ch->IsDead() || ch->GetExchange() || ch->GetMyShop() || ch->IsAcceOpened())
			return false;
		LPITEM worn = ch->GetWear(WEAR_COSTUME_ACCE);
		std::vector<LPITEM> bag;
		CollectPlayerBotBagSashes(ch, bag);
		LPITEM best = NULL;
		for (size_t i = 0; i < bag.size(); ++i)
		{
			LPITEM s = bag[i];
			if (!IsPlayerBotSashAbsorbed(s) && !IsPlayerBotSashAtTarget(s, t))
				continue;
			if (!best || RankPlayerBotSashFor(ch, s) > RankPlayerBotSashFor(ch, best))
				best = s;
		}
		if (!best || (worn && RankPlayerBotSashFor(ch, best) <= RankPlayerBotSashFor(ch, worn)))
			return false;
		const DWORD oldVnum = worn ? worn->GetVnum() : 0;
		if (worn && !TakeOffPlayerBotSash(ch, worn))
			return false;
		if (!ch->EquipItem(best))
		{
			if (worn && !worn->IsEquipped())
				ch->EquipItem(worn);
			return false;
		}
		++s_kPlayerBotSashStats.wears;
		sys_log(0, "PLAYERBOT_SASH: wear pid=%u name=%s lv=%d vnum=%u grade=%d abs=%d absorbed=%u old=%u",
				ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), best->GetVnum(), GetPlayerBotSashGrade(best),
				GetPlayerBotSashAbsorption(best), (DWORD)best->GetSocket(ACCE_ABSORBED_SOCKET), oldVnum);
		return true;
	}

	void EndPlayerBotUrielVisit(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		state.bVisitingUriel = false;
		state.dwNextSashActionTime = 0;
		state.dwNextSashCheckTime = dwNow + number(PLAYERBOT_SASH_CHECK_MIN_MS, PLAYERBOT_SASH_CHECK_MAX_MS);
		ClearPlayerBotRoute(state, true);
		if (ch && ch->IsAcceOpened())
		{
			if (ch->IsAcceOpened(true))
				ch->CloseAcce();
			if (ch->IsAcceOpened(false))
				ch->CloseAcce();
		}
	}

	// The tick: now and then, look at the sashes; with work for Uriel, go to
	// him (a first village of its kingdom, by the road the medal stand takes)
	// and do it there, a step every few seconds.
	bool ManagePlayerBotSash(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || state.bVisitingShop || state.bVisitingBiologist || state.bVisitingHerbalist ||
				state.bVisitingStable || state.bVisitingAlchemist || state.bFishingSession ||
				state.bMarketTrip)
			return false;
		if (!state.bVisitingUriel && dwNow < state.dwNextSashCheckTime)
			return false;
		if (!IsPlayerBotSashKeeper(ch) ||
				(ch->GetParty() && IsPlayerBotHumanLedParty(ch->GetParty())) ||
				IsPlayerBotHeldForCompany(ch) || ch->GetMyShop() != NULL)
		{
			if (state.bVisitingUriel)
				EndPlayerBotUrielVisit(ch, state, dwNow);
			state.dwNextSashCheckTime = dwNow + number(PLAYERBOT_SASH_CHECK_MIN_MS, PLAYERBOT_SASH_CHECK_MAX_MS);
			return false;
		}
		const TPlayerBotSashTarget t = GetPlayerBotSashTarget(ch);

		if (!state.bVisitingUriel)
		{
			state.dwNextSashCheckTime = dwNow + number(PLAYERBOT_SASH_CHECK_MIN_MS, PLAYERBOT_SASH_CHECK_MAX_MS);
			WearPlayerBotBestSash(ch, t);
			NotePlayerBotLoneSashes(ch, t, dwNow);
			if (!HasPlayerBotSashWork(ch, t))
				return false;

			playerbot_empire_rules::TPoint uriel;
			if (!GetPlayerBotUriel(ch->GetMapIndex(), uriel))
			{
				long homeMap = 0, homeX = 0, homeY = 0;
				if (!GetPlayerBotVillageReturn(ch, playerbot_empire_rules::MAP_ROLE_M1, homeMap, homeX, homeY) ||
						!IsPlayerBotMapHostedHere(homeMap) || !GetPlayerBotUriel(homeMap, uriel))
					return false;
				// Not from a fight: the next look, then.
				if (ch->GetVictim() != NULL || state.dwTargetVID != 0)
				{
					state.dwNextSashCheckTime = dwNow + number(20000, 60000);
					return false;
				}
				SetPlayerBotAction(state, BOT_ACTION_TRAVEL, dwNow);
				if (!TransitionPlayerBotMap(ch, state, homeMap, homeX, homeY, dwNow, "sash_to_uriel"))
					return false;
				++s_kPlayerBotSashStats.trips;
				// Straight to Uriel once it stands in the village.
				state.dwNextSashCheckTime = 0;
				return true;
			}
			state.bVisitingUriel = true;
			state.dwNextSashActionTime = 0;
			state.dwTargetVID = 0;
			ch->SetVictim(NULL);
			ch->Stop();
			ClearPlayerBotRoute(state, true);
			sys_log(0, "PLAYERBOT_SASH: going to Uriel pid=%u name=%s lv=%d target=%d/%d gold=%lld",
					ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), t.grade, t.absorption,
					(long long)ch->GetGold());
		}

		playerbot_empire_rules::TPoint uriel;
		if (!GetPlayerBotUriel(ch->GetMapIndex(), uriel))
		{
			EndPlayerBotUrielVisit(ch, state, dwNow);
			return false;
		}
		SetPlayerBotAction(state, BOT_ACTION_SHOP, dwNow);
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);

		long approachX = 0, approachY = 0;
		GetPlayerBotNpcApproach(ch->GetPlayerID(), uriel.x, uriel.y, 0x55524945U, approachX, approachY);
		if (DISTANCE_APPROX(ch->GetX() - approachX, ch->GetY() - approachY) > 650)
		{
			if (!MovePlayerBot(ch, approachX, approachY, dwNow, 20, true, true, false, true) &&
					state.bStuckCounter >= 6)
			{
				sys_err("PLAYERBOT_SASH: route to Uriel failed pid=%u name=%s map=%ld from=(%ld,%ld)",
						ch->GetPlayerID(), ch->GetName(), ch->GetMapIndex(), ch->GetX(), ch->GetY());
				EndPlayerBotUrielVisit(ch, state, dwNow);
				return false;
			}
			return true;
		}

		ch->Stop();
		ch->SetPosition(POS_STANDING);
		if (state.dwNextSashActionTime == 0)
		{
			state.dwNextSashActionTime = dwNow + number(3000, 7000);
			state.bSashVisitSteps = 0;
			return true;
		}
		if (dwNow < state.dwNextSashActionTime)
			return true;

		// One step: a combine, else the absorption; then the next in a few
		// seconds, until nothing is left or the visit has done enough.
		bool did = false;
		LPITEM first = NULL, second = NULL;
		if (state.bSashVisitSteps < PLAYERBOT_SASH_VISIT_MAX_STEPS && FindPlayerBotSashPair(ch, t, first, second))
			did = CombinePlayerBotSashes(ch, first, second);
		else if (state.bSashVisitSteps < PLAYERBOT_SASH_VISIT_MAX_STEPS)
		{
			LPITEM sash = FindPlayerBotSashToFill(ch, t);
			LPITEM piece = sash ? FindPlayerBotSashAbsorbPiece(ch, sash) : NULL;
			if (sash && piece)
			{
				did = AbsorbIntoPlayerBotSash(ch, sash, piece);
				if (did)
					WearPlayerBotBestSash(ch, t);
			}
		}
		if (did)
		{
			++state.bSashVisitSteps;
			state.dwNextSashActionTime = dwNow + number(2500, 5000);
			return true;
		}
		WearPlayerBotBestSash(ch, t);
		sys_log(0, "PLAYERBOT_SASH: visit over pid=%u name=%s lv=%d steps=%d worn=%u grade=%d abs=%d absorbed=%d gold=%lld",
				ch->GetPlayerID(), ch->GetName(), ch->GetLevel(), (int)state.bSashVisitSteps,
				ch->GetWear(WEAR_COSTUME_ACCE) ? ch->GetWear(WEAR_COSTUME_ACCE)->GetVnum() : 0,
				GetPlayerBotSashGrade(ch->GetWear(WEAR_COSTUME_ACCE)),
				GetPlayerBotSashAbsorption(ch->GetWear(WEAR_COSTUME_ACCE)),
				IsPlayerBotSashAbsorbed(ch->GetWear(WEAR_COSTUME_ACCE)) ? 1 : 0, (long long)ch->GetGold());
		EndPlayerBotUrielVisit(ch, state, dwNow);
		return false;
	}

	void LogPlayerBotSashCensus()
	{
		sys_log(0, "PLAYERBOT_SASH: census combines=%u fails=%u absorbs=%u wears=%u trips=%u bought=%u",
				s_kPlayerBotSashStats.combines, s_kPlayerBotSashStats.combineFails,
				s_kPlayerBotSashStats.absorbs, s_kPlayerBotSashStats.wears,
				s_kPlayerBotSashStats.trips, s_kPlayerBotSashStats.bought);
	}
}

#else

namespace
{
	bool IsPlayerBotKeptSash(LPCHARACTER, LPITEM) { return false; }
	bool IsPlayerBotSashReleased(DWORD) { return false; }
	DWORD GetPlayerBotSashPrice(LPITEM) { return 0; }
	bool WantsPlayerBotSashOffer(LPCHARACTER, LPITEM) { return false; }
	bool CanPlayerBotPayForSashOffer(LPCHARACTER, LPITEM, long long) { return false; }
	bool PlayerBotWantsSashFromMarket(LPCHARACTER) { return false; }
	bool WantsPlayerBotSashPieceOffer(LPCHARACTER, LPITEM) { return false; }
	bool CanPlayerBotPayForSashPiece(LPCHARACTER, LPITEM, long long) { return false; }
	bool PlayerBotWantsSashPieceFromMarket(LPCHARACTER) { return false; }
	bool IsPlayerBotSashGrailProject(LPCHARACTER, LPITEM) { return false; }
	void NotePlayerBotSashBought(LPCHARACTER, DWORD, long long) {}
	bool ManagePlayerBotSash(LPCHARACTER, TPlayerBotAIState&, DWORD) { return false; }
	void LogPlayerBotSashCensus() {}
}

#endif

#endif
