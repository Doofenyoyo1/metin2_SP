#ifndef __INC_METIN2_PLAYERBOT_BONUS_H__
#define __INC_METIN2_PLAYERBOT_BONUS_H__

// The bonus lines on a worn item, and what a bot is willing to spend to change
// them.
//
// Gear is only half a bot's power and these are the other half: a
// level-appropriate weapon rolled into five resistances is genuinely worse
// than the plain one it replaced. Two engine items do the work and neither can
// be dropped, sold, traded or shopped, so there is no market to walk to - a bot
// pays for one the way it pays for its stall.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_economy.h - it uses that file's idea of what
// a bot is short of - and before playerbot_town.h, which is where a bot decides
// to go and do this.

namespace
{
	// --- Bonus lines ---------------------------------------------------------
	// Gear is only half a bot's power; the four bonus lines are the other half. A
	// level-appropriate weapon rolled into four resistances is genuinely worse
	// than the one it replaced, and until now nothing ever looked at them.
	//
	// The scoring below is deliberately coarse. It exists to tell "worth keeping"
	// from "roll it again", not to model the damage formula: every line is scored
	// as points-per-typical-roll so that a +2000 HP line and a +15 attack line
	// can be compared at all.
	bool IsPlayerBotCaster(LPCHARACTER ch)
	{
		return ch && (ch->GetJob() == JOB_SHAMAN || ch->GetJob() == JOB_SURA);
	}

	bool IsPlayerBotOffensiveSlot(BYTE wearCell)
	{
		return wearCell == WEAR_WEAPON;
	}

	// What one line is worth to this bot, in points per typical roll.
	//
	// Three of the world's own tables decide it and none of it is taste. A
	// player sent in a spreadsheet of "which bonus is worth having on which
	// piece, on which map"; this is that spreadsheet checked against our files,
	// which disagree with it in several places worth knowing about.
	//
	//  * `player.item_attr` says what may roll where and how high. Health does
	//    not roll on a helmet or an earring, critical does not roll on a wrist
	//    or an earring, attack value rolls on a body and nowhere else, and
	//    block only on a shield. That is why the finishing rule below is per
	//    slot: the old one asked a helmet for health and attack value, so no
	//    helmet in the world could ever be finished and every one of them was
	//    rerolled for as long as its owner had gold.
	//  * `battle.cpp` says what a line does. BLOCK stops a melee hit outright
	//    and 73% of this world's monsters are melee. DODGE and RESIST_BOW only
	//    answer a ranged attacker, and ranged monsters are 0-24% of a map. An
	//    elemental resistance is applied at thirty percent of its own number,
	//    so fifteen points of it is four and a half percent. And the five
	//    weapon-type resistances never fire against a monster at all: that
	//    branch reads the attacker's WEAR_WEAPON and a monster wears none, so
	//    "odpornosc na miecze" is a line for fighting players.
	//  * `tools/analyse_map_races.py` says what each map is made of, which is
	//    what makes a race line worth having - or not, on the three maps whose
	//    monsters no item can be strong against.
	//
	// The scoring stays coarse on purpose: it tells "worth keeping" from "roll
	// it again", it does not model the damage formula.
	int ScorePlayerBotBonusLineRaw(LPCHARACTER ch, BYTE wearCell, BYTE type, short value)
	{
		// A negative roll exists (movement speed on some sets) and is worth less
		// than nothing, so it must not be able to prop up a bad item's total.
		if (value <= 0)
			return 0;

		const bool bCaster = IsPlayerBotCaster(ch);

		switch (type)
		{
			// The two damage lines are not the same line for every character,
			// and weighting them alike had one class rerolling away the only
			// bonus that does anything for it.
			//
			// Measured over every attribute on this world's items: average
			// damage rolls up to 46 and skill damage only to 18. At twelve and
			// ten a maximum average roll scored 460 against a maximum skill
			// roll's 216, so average damage won by more than two to one - for
			// everybody, a Shaman included, whose damage is very nearly all
			// skills. A caster that rolled the best skill-damage line in the
			// game would throw it away on the next pass.
			//
			// So the weights are per build, and chosen against those two
			// ceilings rather than by feel: a caster's best skill roll (18 x 30
			// = 540) beats its best average roll (46 x 6 = 276), and for
			// everyone else the order stays as it was.
			case APPLY_SKILL_DAMAGE_BONUS:
				return bCaster ? value * 30 : value * 12;
			case APPLY_NORMAL_HIT_DAMAGE_BONUS:
				return bCaster ? value * 6 : value * 10;

			// "Silny przeciwko Orkom" and its five siblings - the line the
			// player's table is really about, and the one this pass used to
			// throw away. It fell through to the default and was worth its own
			// number, so twenty percent against orcs scored twenty points and
			// lost to six points of movement speed, while the equipment pass was
			// paying twelve thousand for the same line. The bot bought the
			// shield for it and rerolled it off at the next blacksmith.
			//
			// It multiplies the whole attack - normal hits and skills alike -
			// against every monster of that race, so where the map is that race
			// it beats any other line a shield or an earring can roll. Where it
			// is not, it is worth keeping only because bots change maps.
			case APPLY_ATTBONUS_ANIMAL:
			case APPLY_ATTBONUS_UNDEAD:
			case APPLY_ATTBONUS_DEVIL:
			case APPLY_ATTBONUS_HUMAN:
			case APPLY_ATTBONUS_ORC:
			case APPLY_ATTBONUS_MILGYO:
			{
				int racePercent = 0;
				const int race = GetPlayerBotFightingRace(ch, &racePercent);
				const int onMap = (race != PLAYERBOT_RACE_NONE &&
						GetPlayerBotRaceApplyType(race) == type)
						? PLAYERBOT_BONUS_RACE_ON_MAP * racePercent / 100 : 0;
				return value * std::max(onMap, PLAYERBOT_BONUS_RACE_OFF_MAP);
			}
			// Worth having and worth nothing to chase: "Silny przeciwko
			// Potworom" raises damage against every monster and against Metin
			// stones, which is the whole of what a bot ever fights. It is not in
			// player.item_attr at all, so no reroll can produce one;
			// item_attr_rare carries it at ten, and the pieces that have it keep
			// it.
			case APPLY_ATTBONUS_MONSTER:        return value * 14;

			case APPLY_CRITICAL_PCT:            return value * 10;
			case APPLY_PENETRATE_PCT:           return value * 10;
			// Attack speed is a straight multiplier on everything a bot does and
			// it rolls only to eight, so a maximum roll is eight percent more of
			// every swing, every shot and every skill. It was worth sixty-four
			// points, less than a mediocre health roll.
			case APPLY_ATT_SPEED:               return value * 15;
			// Life stolen per hit is what keeps a grinder off the potions and
			// out of town, which is the errand that costs a bot the most time.
			case APPLY_STEAL_HP:                return value * 12;
			// Rolls on a body and nowhere else, to fifty.
			case APPLY_ATT_GRADE_BONUS:         return value * 5;
			case APPLY_CAST_SPEED:              return bCaster ? value * 8 : value;
			case APPLY_MAX_HP_PCT:              return value * 15;
			case APPLY_DEF_GRADE_BONUS:
				return IsPlayerBotOffensiveSlot(wearCell) ? value * 2 : value * 6;
			// A bot walks kilometres between hubs and the horse is not always
			// under it, but speed is not power: a real line, not a great one.
			case APPLY_MOV_SPEED:               return value * 4;

			// The four stats, which roll to twelve on a weapon and a shield. A
			// point of the school's own stat is attack; a point of vitality is
			// health no reroll can take away. They used to be worth their own
			// number, so a maximum roll of the best stat in the game scored
			// twelve and lost to two percent of anything.
			case APPLY_CON:                     return value * 20;
			case APPLY_STR:                     return bCaster ? value * 8 : value * 25;
			case APPLY_INT:                     return bCaster ? value * 25 : value * 8;
			case APPLY_DEX:                     return value * 12;

			// A blocked hit is a hit that did not happen, and it answers melee -
			// 73% of the monsters in this world. Fifteen percent of every hit is
			// the roll a player keeps a shield for, after immunity to stun.
			case APPLY_BLOCK:                   return value * 20;
			// Dodge and arrow resistance answer a ranged attacker only, and
			// ranged monsters are between nothing and a quarter of a map: real,
			// and a fraction of what block is worth.
			case APPLY_DODGE:                   return value * 6;
			case APPLY_RESIST_BOW:              return value * 4;
			// battle_hit reads the attacker's WEAR_WEAPON to choose which of
			// these applies and a monster wears no weapon, so against anything a
			// bot fights these five do nothing whatever. Left at a point a line
			// rather than zero, because a line is still a line.
			case APPLY_RESIST_SWORD:
			case APPLY_RESIST_TWOHAND:
			case APPLY_RESIST_DAGGER:
			case APPLY_RESIST_BELL:
			case APPLY_RESIST_FAN:              return value;
			// An elemental resistance is applied at thirty percent of its own
			// number and only against a monster carrying that attack flag, so
			// the fifteen of a maximum roll is four and a half percent off the
			// hits of about half of one map. The player's table wanted a
			// resistance chosen per map; measured, the whole axis is too small
			// to plan a piece of gear around.
			case APPLY_RESIST_FIRE:
			case APPLY_RESIST_ELEC:
			case APPLY_RESIST_WIND:
			case APPLY_RESIST_ICE:
			case APPLY_RESIST_EARTH:
			case APPLY_RESIST_DARK:
			case APPLY_RESIST_MAGIC:            return value * 3;
			case APPLY_REFLECT_MELEE:           return value * 6;

			// A stunned monster does not hit back, which is worth more to a bot
			// than to a player: nothing here retreats from a fight it is winning.
			case APPLY_STUN_PCT:                return value * 10;
			case APPLY_SLOW_PCT:                return value * 6;
			case APPLY_POISON_PCT:              return value * (ch && (int)ch->GetLevel() >= PLAYERBOT_POISON_BOSS_LEVEL ? 16 : 8);

			// The economy lines. A bot's drops are its gear, its refines, its
			// stall and its fares, so twenty percent more of them is a real
			// upgrade; experience is what the whole population is for.
			case APPLY_ITEM_DROP_BONUS:         return value * 8;
			case APPLY_EXP_DOUBLE_BONUS:        return value * 8;
			case APPLY_GOLD_DOUBLE_BONUS:       return value * 4;
			case APPLY_HP_REGEN:
			case APPLY_SP_REGEN:                return value * 2;

			// Big absolute numbers that have to be scaled down to compare with the
			// percentage lines above.
			case APPLY_MAX_HP:                  return value / 4;
			// The immunities roll as a 1, so they used to fall through to the
			// default and be worth one point - less than a point of movement
			// speed. Immunity to stun is the roll a player keeps a shield for
			// the rest of the game, and a bot was rerolling it away.
			case APPLY_IMMUNE_STUN:             return 400;
			case APPLY_IMMUNE_SLOW:             return 250;
			case APPLY_IMMUNE_FALL:             return 120;
			// Mana is what stops a bot keeping its buffs up - 213 of 1300 held
			// less than the 300 SP a mastered aura costs - and it rolls to
			// eighty, so this is one of the few lines that can fix that.
			case APPLY_MAX_SP:                  return bCaster ? value * 2 : value / 2;
			// Everything else - stamina, poison reduction, mana burn - is real but
			// minor for a bot that only grinds. Never zero: a line is still a line.
			default:                            return value;
		}
	}

	// The measured weight above, scaled by Iwakura's PvE tier of the line
	// (playerbot_item_tiers.h, PLAYERBOT_BONUS_TIER_PERCENT): what he calls
	// wspanialy is worth a third more, what he calls bardzo zly a quarter.
	// The equipment score scales its lines the same way
	// (ScorePlayerBotApplyTiered), so buying and rerolling agree.
	// The three slots a young bot is told to bonus first, and what it is told
	// to want on each (Community Patch 1). They are the cheap pieces - the
	// jewellery and the boots a bot of twenty wears are the weakest thing it
	// owns - and that is the point: a line on one of them is worth more early
	// than a grade of refine on any of them.
	bool IsPlayerBotEarlySlotLine(BYTE wearCell, BYTE type)
	{
		switch (wearCell)
		{
			case WEAR_FOOTS:
				// Maks. PZ, Szansa na cios krytyczny, Szybkosc ataku
				return type == APPLY_MAX_HP || type == APPLY_CRITICAL_PCT ||
						type == APPLY_ATT_SPEED;
			case WEAR_NECK:
				// Maks. PZ, Szansa na cios krytyczny, Szansa na przeszywajace uderzenie
				return type == APPLY_MAX_HP || type == APPLY_CRITICAL_PCT ||
						type == APPLY_PENETRATE_PCT;
			case WEAR_WRIST:
				// Maks. PZ, x% obrazen dodanych do PZ, przeszywajace uderzenie,
				// Silny przeciwko Zwierzetom, Silny przeciwko Orkom
				return type == APPLY_MAX_HP || type == APPLY_STEAL_HP ||
						type == APPLY_PENETRATE_PCT ||
						type == APPLY_ATTBONUS_ANIMAL || type == APPLY_ATTBONUS_ORC;
			default:
				return false;
		}
	}

	// Under PLAYERBOT_EARLY_BONUS_MAX_LEVEL the jewellery and the boots come
	// before everything else, and the change stone's refine floor does not
	// apply to them.
	bool IsPlayerBotEarlyBonusSlot(LPCHARACTER ch, BYTE wearCell)
	{
		if (!ch || ch->GetLevel() >= PLAYERBOT_EARLY_BONUS_MAX_LEVEL)
			return false;
		return wearCell == WEAR_NECK || wearCell == WEAR_WRIST || wearCell == WEAR_FOOTS;
	}

	int ScorePlayerBotBonusLine(LPCHARACTER ch, BYTE wearCell, BYTE type, short value)
	{
		const int raw = ScorePlayerBotBonusLineRaw(ch, wearCell, type, value);
		const int tier = ch ? GetPlayerBotBonusTier(type, (int)ch->GetJob(), false) : 0;
		int score = tier > 0 ? raw * PLAYERBOT_BONUS_TIER_PERCENT[tier] / 100 : raw;
		if (IsPlayerBotEarlyBonusSlot(ch, wearCell) && IsPlayerBotEarlySlotLine(wearCell, type))
			score = score * PLAYERBOT_EARLY_BONUS_PERCENT / 100;
		return score;
	}

	// The one roll that finishes an item, and it is a different roll for every
	// slot because `player.item_attr` lets a different set of lines onto every
	// slot.
	//
	// Everything above is a score, and a score can always be beaten by another
	// score - which means a perfect item is one unlucky comparison away from
	// being rerolled. These are the rolls a player stops on.
	//
	// The old rule asked a helmet for fifteen hundred health and either attack
	// value or arrow resistance, and asked an earring and a wrist for health and
	// critical. None of those five lines can roll on those slots at all - health
	// rolls on body, wrist, foots and neck, critical on weapon, foots and neck,
	// attack value on a body and nowhere else - so a helmet, an earring and a
	// wrist could never be finished, and were rerolled for as long as their
	// owner had gold. What each of them is actually worn for is here instead:
	// attack speed or arrow dodge on a helmet, the race line and item drop on an
	// earring, penetration and stolen life on a wrist.
	//
	// An item that has one is never rerolled again. It may still have a line
	// ADDED, because that cannot lose what is already there.
	bool HasPlayerBotFinishedBonus(LPCHARACTER ch, LPITEM item, BYTE wearCell)
	{
		if (!item)
			return false;

		// Under level forty-five the boots, the necklace and the bracelet are
		// finished by the lines the patch requires of them, not by a score:
		// the health line and at least one more of the slot's list
		// (IsPlayerBotEarlySlotLine) - community patch 2, point 3.
		if (IsPlayerBotEarlyBonusSlot(ch, wearCell))
		{
			long earlyHp = 0;
			int others = 0;
			for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			{
				const BYTE type = item->GetAttributeType(i);
				const long value = item->GetAttributeValue(i);
				if (value <= 0)
					continue;
				if (type == APPLY_MAX_HP)
					earlyHp = value;
				else if (IsPlayerBotEarlySlotLine(wearCell, type))
					++others;
			}
			return earlyHp >= PLAYERBOT_BONUS_KEEP_HP && others >= 1;
		}

		long hp = 0, attGrade = 0, resistBow = 0, crit = 0, penetrate = 0;
		long average = 0, skill = 0, block = 0, dodge = 0, attSpeed = 0;
		long steal = 0, drop = 0, mov = 0, race = 0;
		bool immuneStun = false;
		// The race this bot is being paid for; a line against any other race is
		// not what a piece is kept for, however high it rolled.
		const BYTE wantedRace = GetPlayerBotRaceApplyType(GetPlayerBotFightingRace(ch));
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			const BYTE type = item->GetAttributeType(i);
			const long value = item->GetAttributeValue(i);
			if (value <= 0)
				continue;
			if (wantedRace != APPLY_NONE && type == wantedRace)
				race = value;
			switch (type)
			{
				case APPLY_IMMUNE_STUN:             immuneStun = true; break;
				case APPLY_MAX_HP:                  hp = value; break;
				case APPLY_ATT_GRADE_BONUS:         attGrade = value; break;
				case APPLY_RESIST_BOW:              resistBow = value; break;
				case APPLY_CRITICAL_PCT:            crit = value; break;
				case APPLY_PENETRATE_PCT:           penetrate = value; break;
				case APPLY_NORMAL_HIT_DAMAGE_BONUS: average = value; break;
				case APPLY_SKILL_DAMAGE_BONUS:      skill = value; break;
				case APPLY_BLOCK:                   block = value; break;
				case APPLY_DODGE:                   dodge = value; break;
				case APPLY_ATT_SPEED:               attSpeed = value; break;
				case APPLY_STEAL_HP:                steal = value; break;
				case APPLY_ITEM_DROP_BONUS:         drop = value; break;
				case APPLY_MOV_SPEED:               mov = value; break;
				default: break;
			}
		}

		switch (wearCell)
		{
			case WEAR_SHIELD:
				// Immunity to stun first, as it always was; then the two lines a
				// shield is otherwise kept for, and both of them roll here and
				// nowhere else worth speaking of.
				return immuneStun || block >= PLAYERBOT_BONUS_KEEP_BLOCK ||
						race >= PLAYERBOT_BONUS_KEEP_RACE;
			case WEAR_WEAPON:
			{
				// A level-30 or level-75 weapon a player hand-tuned is finished
				// the moment it lands an average-damage or average-skill line
				// over the lock, so the mixer leaves it alone (Ciapek).
				const int lvl = item->GetLevelLimit();
				if ((lvl == 30 || lvl == 75) &&
						(average >= PLAYERBOT_BONUS_WEAPON_LOCK_PCT ||
						 skill >= PLAYERBOT_BONUS_WEAPON_LOCK_PCT))
					return true;
				// Any weapon, not only the level-30 family: with the vnum test
				// here a bow of forty-five with a 40% average was "unfinished"
				// and rerolled towards the line score until the average was
				// gone ("boty zmixowaly wysokie srednie 35+ na duzo mniejsze").
				//
				// The caster's clause is not generosity: item_addon.cpp draws
				// the skill line and then sets the average to minus twice it, so
				// a weapon cannot carry both and a Shaman that only ever stopped
				// on the average line never stopped at all.
				//
				// And a big skill line is finished for every class, not only a
				// caster: it is a PvP prize this world will use later, and mixing
				// it off would waste it ("szkoda tracic takiego ladnego bonusu do
				// PvP"). PvE still wears the average weapon - this only
				// stops the reroll from destroying the skill one.
				// Thirty since community patch 2, point 3 ("tak, aby wynosily one
				// 30%+ SR"); it was PLAYERBOT_BONUS_KEEP_AVERAGE, twenty.
				return average >= PLAYERBOT_BONUS_WEAPON_TARGET_AVERAGE ||
						skill > PLAYERBOT_BONUS_SKILL_PVP_PCT ||
						(IsPlayerBotCaster(ch) && skill >= PLAYERBOT_BONUS_KEEP_SKILL);
			}
			case WEAR_BODY:
				return hp >= PLAYERBOT_BONUS_KEEP_HP &&
						(attGrade > 0 || resistBow > 0 ||
						 steal >= PLAYERBOT_BONUS_KEEP_STEAL);
			case WEAR_HEAD:
				// No health, no attack value, no arrow resistance rolls here.
				return attSpeed >= PLAYERBOT_BONUS_KEEP_ATT_SPEED ||
						dodge >= PLAYERBOT_BONUS_KEEP_DODGE ||
						race >= PLAYERBOT_BONUS_KEEP_RACE;
			case WEAR_FOOTS:
				return hp >= PLAYERBOT_BONUS_KEEP_HP &&
						(attSpeed >= PLAYERBOT_BONUS_KEEP_ATT_SPEED ||
						 crit >= PLAYERBOT_BONUS_KEEP_CRIT ||
						 dodge >= PLAYERBOT_BONUS_KEEP_DODGE ||
						 mov >= PLAYERBOT_BONUS_KEEP_MOV);
			case WEAR_WRIST:
				// Critical does not roll on a wrist; penetration does.
				return hp >= PLAYERBOT_BONUS_KEEP_HP &&
						(penetrate >= PLAYERBOT_BONUS_KEEP_CRIT ||
						 steal >= PLAYERBOT_BONUS_KEEP_STEAL ||
						 drop >= PLAYERBOT_BONUS_KEEP_DROP ||
						 race >= PLAYERBOT_BONUS_KEEP_RACE);
			case WEAR_NECK:
				return hp >= PLAYERBOT_BONUS_KEEP_HP &&
						crit >= PLAYERBOT_BONUS_KEEP_CRIT;
			case WEAR_EAR:
				// Neither health nor critical rolls on an earring. What does is
				// the race line, item drop and movement speed.
				return race >= PLAYERBOT_BONUS_KEEP_RACE ||
						drop >= PLAYERBOT_BONUS_KEEP_DROP ||
						mov >= PLAYERBOT_BONUS_KEEP_MOV;
			default:
				return false;
		}
	}

	// The rolls a piece is priced up for: the top of what a line can be, or near
	// enough that a player keeps the item for it. A deliberately shorter list
	// than ScorePlayerBotBonusLine - the question here is not "is this line
	// good" but "is this the line somebody pays extra for" - and every number
	// is the lv5 column of `player.item_attr`, so it is the top of what this
	// world can actually roll rather than the top of what the wiki lists.
	//
	// Two entries used to name lines that cannot roll here at all: health as a
	// percentage is in no attribute table, and "strong against monsters" is only
	// in the rare one, where it is ten and not twenty.
	bool IsPlayerBotTopBonusLine(BYTE type, long value)
	{
		switch (type)
		{
			case APPLY_MAX_HP:                  return value >= 2000;
			case APPLY_MAX_SP:                  return value >= 80;
			case APPLY_CON:
			case APPLY_INT:
			case APPLY_STR:
			case APPLY_DEX:                     return value >= 12;
			case APPLY_CRITICAL_PCT:            return value >= 10;
			case APPLY_PENETRATE_PCT:           return value >= 10;
			case APPLY_SKILL_DAMAGE_BONUS:      return value >= 15;
			case APPLY_NORMAL_HIT_DAMAGE_BONUS: return value >= 20;
			// Twenty on the five ordinary races, ten on human, which rolls half
			// as high and half as often.
			case APPLY_ATTBONUS_ANIMAL:
			case APPLY_ATTBONUS_ORC:
			case APPLY_ATTBONUS_MILGYO:
			case APPLY_ATTBONUS_UNDEAD:
			case APPLY_ATTBONUS_DEVIL:          return value >= 20;
			case APPLY_ATTBONUS_HUMAN:          return value >= 10;
			case APPLY_ATTBONUS_MONSTER:        return value >= 10;
			case APPLY_ATT_GRADE_BONUS:         return value >= 50;
			case APPLY_ATT_SPEED:               return value >= 8;
			case APPLY_MOV_SPEED:               return value >= 20;
			case APPLY_STEAL_HP:                return value >= 10;
			case APPLY_BLOCK:
			case APPLY_DODGE:                   return value >= 15;
			case APPLY_ITEM_DROP_BONUS:
			case APPLY_EXP_DOUBLE_BONUS:        return value >= 20;
			case APPLY_IMMUNE_STUN:
			case APPLY_IMMUNE_SLOW:             return true;
			default:                            return false;
		}
	}

	// Iwakura's bonus multipliers - the rows per slot and line, and the tiers
	// of a weapon's two damage lines - are generated into
	// playerbot_price_tables.h from his sheet, each row checked against
	// world.item_attr for the slot he put it under.
	// The whole product is capped here - hundredths, so ten thousand is a
	// hundredfold; a weapon of sixty average and thirty skill would be
	// 2800 times its base otherwise.
	const long long PLAYERBOT_BONUS_PRICE_MAX_PCT = 10000;

	BYTE GetPlayerBotPriceSlot(LPITEM item)
	{
		if (!item)
			return 0;
		if (item->GetType() == ITEM_WEAPON)
			return PRICE_SLOT_WEAPON;
		if (item->GetType() != ITEM_ARMOR)
			return 0;
		switch (item->GetSubType())
		{
			case ARMOR_BODY:   return PRICE_SLOT_BODY;
			case ARMOR_HEAD:   return PRICE_SLOT_HEAD;
			case ARMOR_SHIELD: return PRICE_SLOT_SHIELD;
			case ARMOR_FOOTS:  return PRICE_SLOT_FOOTS;
			case ARMOR_WRIST:  return PRICE_SLOT_WRIST;
			case ARMOR_NECK:   return PRICE_SLOT_NECK;
			case ARMOR_EAR:    return PRICE_SLOT_EAR;
			default:           return 0;
		}
	}

	// The top roll of an apply on this item's attribute set, from the
	// engine's own table; zero when the table has no such line.
	long GetPlayerBotBonusMaxRoll(LPITEM item, BYTE bApply)
	{
		TItemAttrMap::const_iterator it = g_map_itemAttr.find(bApply);
		if (it == g_map_itemAttr.end())
			return 0;
		const TItemAttrTable& row = it->second;
		const int set = item ? item->GetAttributeSetIndex() : -1;
		int level = (set >= 0 && set < ATTRIBUTE_SET_MAX_NUM) ? row.bMaxLevelBySet[set] : 0;
		if (level <= 0 || level > ITEM_ATTRIBUTE_MAX_LEVEL)
			level = ITEM_ATTRIBUTE_MAX_LEVEL;
		return row.lValues[level - 1];
	}

	// A weapon damage line's multiplier on Iwakura's sheet, read between his
	// bands. The sheet gives one number to a band - average 10-19 x1.2, 20-29
	// x1.5, skill 1-10 x1.2 - and read as steps, a 19% average asked what a 10%
	// one did, and exactly what a weapon of 1% average and 3% skill did: two
	// Ostrza z Czerwonej Stali +0 at 15 150 000 each ("czy nie pracowalismy nad
	// tym, aby premiowana bardziej byla z wyzszymi srednimi?",
	// 15 September). His number is taken as what a roll in the middle of its
	// band is worth, and the multiplier runs in a straight line from one band's
	// middle to the next: a better roll asks more, a worse one less, and a
	// band's rolls average his price. It starts from no premium one point under
	// the first band. The last band is a single value and ends the line.
	WORD GetPlayerBotDamageTierPct(const TPlayerBotDamageTier* tiers, size_t count, long value)
	{
		if (!tiers || count == 0)
			return 100;
		// Doubled, so the middle of a band is a whole number.
		const long v2 = 2L * value;
		long prevX = 2L * ((long)tiers[0].bFrom - 1);
		long prevPct = 100;
		if (v2 <= prevX)
			return 100;
		for (size_t i = 0; i < count; ++i)
		{
			const long from = tiers[i].bFrom;
			const long end = i + 1 < count ? (long)tiers[i + 1].bFrom - 1 : from;
			const long midX = from + end;
			const long pct = tiers[i].wPct;
			if (v2 <= midX)
				return (WORD)(prevPct + (pct - prevPct) * (v2 - prevX) / std::max(1L, midX - prevX));
			prevX = midX;
			prevPct = pct;
		}
		return (WORD)prevPct;
	}

	// What one line on a piece asks on Iwakura's sheet, in hundredths, and
	// whether it is at its top (Community Patch 5, point 11). A weapon's two
	// damage lines by his tiers - the average counts as a line at its top from
	// forty, his own rule, the skill damage at the top of its tiers - and
	// every other line by his rows: the slot's row against this world's table,
	// and the point-13 row of a line only upstream's 2.2.31 table rolled, against the
	// top it names (playerbot_price_rules.h says which asks what). A line at
	// its top that his sheet prices at x1.0 still counts as one. The shop sign
	// asks the same question, so it cannot praise a line the price ignored.
	WORD GetPlayerBotBonusLinePct(LPITEM item, BYTE slot, int level, BYTE type, long value, bool* atTop)
	{
		if (atTop)
			*atTop = false;
		if (!item || type == 0 || value <= 0)
			return 100;
		if (slot == PRICE_SLOT_WEAPON && type == APPLY_NORMAL_HIT_DAMAGE_BONUS)
		{
			if (atTop)
				*atTop = value >= playerbot_price_rules::AVERAGE_DAMAGE_MAX_FROM;
			return GetPlayerBotDamageTierPct(PLAYERBOT_AVERAGE_DAMAGE_TIERS,
					sizeof(PLAYERBOT_AVERAGE_DAMAGE_TIERS) / sizeof(PLAYERBOT_AVERAGE_DAMAGE_TIERS[0]), value);
		}
		if (slot == PRICE_SLOT_WEAPON && type == APPLY_SKILL_DAMAGE_BONUS)
		{
			const size_t tiers = sizeof(PLAYERBOT_SKILL_DAMAGE_TIERS) / sizeof(PLAYERBOT_SKILL_DAMAGE_TIERS[0]);
			if (atTop)
				*atTop = value >= (long)PLAYERBOT_SKILL_DAMAGE_TIERS[tiers - 1].bFrom;
			return GetPlayerBotDamageTierPct(PLAYERBOT_SKILL_DAMAGE_TIERS, tiers, value);
		}
		const TPlayerBotBonusPriceRow* slotRow = NULL;
		const TPlayerBotBonusPriceRow* topRow = NULL;
		for (size_t r = 0; r < sizeof(PLAYERBOT_BONUS_PRICE_ROWS) / sizeof(PLAYERBOT_BONUS_PRICE_ROWS[0]); ++r)
		{
			const TPlayerBotBonusPriceRow& row = PLAYERBOT_BONUS_PRICE_ROWS[r];
			if (row.bApply != type || (row.bSlots & slot) == 0 ||
					level < row.bMinLevel || level > row.bMaxLevel)
				continue;
			if (row.wTop == 0 && !slotRow)
				slotRow = &row;
			else if (row.wTop != 0 && !topRow)
				topRow = &row;
		}
		const long tableMax = GetPlayerBotBonusMaxRoll(item, type);
		if (atTop)
			*atTop = playerbot_price_rules::IsMaxLine(value, tableMax, topRow ? (long)topRow->wTop : 0L);
		playerbot_price_rules::BonusRow slotRule = { 100, 100, 0 };
		playerbot_price_rules::BonusRow topRule = { 100, 100, 0 };
		if (slotRow)
		{
			slotRule.maxPct = slotRow->wMaxPct;
			slotRule.otherPct = slotRow->wOtherPct;
		}
		if (topRow)
		{
			topRule.maxPct = topRow->wMaxPct;
			topRule.otherPct = topRow->wOtherPct;
			topRule.top = topRow->wTop;
		}
		return (WORD)playerbot_price_rules::LinePercent(value, tableMax,
				slotRow ? &slotRule : NULL, topRow ? &topRule : NULL);
	}

	// What the lines on an item add to its asking price, as a percentage:
	// Iwakura's multipliers compounded, less the one the base already is, and
	// then his multiplier for two to four lines at their top over the whole
	// (Community Patch 5, point 11). Every exit of the asking price applies
	// this once (ApplyPlayerBotBonusPremium), so the plus, the stones and the
	// lines are all under it, as "cena koncowa" says.
	//
	// No character is asked for, on purpose: this is what any buyer pays, not
	// what one bot would wear, so the caster and weapon-slot weightings of
	// ScorePlayerBotItemBonuses are left out of it.
	int GetPlayerBotBonusPricePercent(LPITEM item)
	{
		if (!item)
			return 0;
		const BYTE slot = GetPlayerBotPriceSlot(item);
		if (slot == 0)
			return 0;
		const int level = item->GetLevelLimit();
		long long product = 100; // hundredths
		int atTopLines = 0;
		// The five ordinary lines: GetAttributeCount counts nothing else, so
		// the two rare ones are neither priced nor counted.
		const int count = item->GetAttributeCount();
		for (int i = 0; i < count && i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			bool atTop = false;
			const WORD pct = GetPlayerBotBonusLinePct(item, slot, level, item->GetAttributeType(i),
					item->GetAttributeValue(i), &atTop);
			if (atTop)
				++atTopLines;
			// Said beside the premium they make (playerbot_explain.h).
			PlayerBotPriceBonusStep(per::STEP_BONUS_LINE, pct, item->GetAttributeType(i),
					item->GetAttributeValue(i), atTop ? 1 : 0);
			// Every line is walked to the end: the ceiling stops the product,
			// not the count of lines at their top.
			product = playerbot_price_rules::CompoundLinePercent(product, pct, PLAYERBOT_BONUS_PRICE_MAX_PCT);
		}
		const int premium = (int)playerbot_price_rules::PiecePremiumPercent(product, atTopLines);
		if (IsPlayerBotPriceTracing() && count > 0)
		{
			if (atTopLines > 0)
				PlayerBotPriceBonusStep(per::STEP_BONUS_MAX_LINES, playerbot_price_rules::MaxLinesPercent(atTopLines),
						atTopLines);
			PlayerBotPriceBonusStep(per::STEP_BONUS_PERCENT, premium, product);
		}
		return premium;
	}

	int ScorePlayerBotItemBonuses(LPCHARACTER ch, LPITEM item, BYTE wearCell)
	{
		if (!ch || !item)
			return 0;
		int score = 0;
		const int count = item->GetAttributeCount();
		for (int i = 0; i < count && i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			score += ScorePlayerBotBonusLine(ch, wearCell,
					item->GetAttributeType(i), item->GetAttributeValue(i));
		}
		return score;
	}

	// A Marmur Blogoslawienstwa in the bag: the one item that adds a fifth
	// line (USE_ADD_ATTRIBUTE2, vnums 39004/70024/70124/76015 on these files;
	// asked by subtype so a renamed one still counts). Nothing sells it, so
	// it comes from drops and chests, and a bot without one stops at four
	// like a player without one.
	int FindPlayerBotBlessingMarbleCell(LPCHARACTER ch)
	{
		if (!ch)
			return -1;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetType() == ITEM_USE && item->GetSubType() == USE_ADD_ATTRIBUTE2 &&
					item->GetCount() > 0 && !item->isLocked())
				return (int)cell;
		}
		return -1;
	}

	// Iwakura's Patch 4, point 6: the only gear an add or a change stone goes
	// on - "doswiadczeni gracze nie bonuja losowego przedmiotu tylko dlatego, ze
	// osiagnal on poziom +4 ... bot ma od teraz calkowity zakaz marnowania ich
	// na ekwipunek niskiej jakosci (np. Srebrny Miecz)": the level-30
	// average-damage weapons, every weapon from level 45 at +7, shields - and
	// with them body armour and helmets, the operator's choice - from level 21
	// at +7, bracelets, necklaces and boots from +4, earrings from +7. Nothing
	// under +4 at all (point 1: "dodawanie bonusow do itemow od +0 do +3 jest
	// od teraz niemozliwe").
	bool IsPlayerBotBonusCategoryAllowed(LPITEM item)
	{
		if (!item)
			return false;
		const int plus = (int)item->GetRefineLevel();
		if (plus < PLAYERBOT_BONUS_JEWEL_MIN_PLUS)
			return false;
		if (item->GetType() == ITEM_WEAPON)
			return IsPlayerBotSpecialLevel30Weapon(item) ||
					(item->GetLevelLimit() >= PLAYERBOT_BONUS_WEAPON_MIN_LEVEL &&
						plus >= PLAYERBOT_BONUS_WEAPON_MIN_PLUS);
		if (item->GetType() != ITEM_ARMOR)
			return false;
		switch (item->GetSubType())
		{
			case ARMOR_BODY:
				return item->GetLevelLimit() >= PLAYERBOT_BONUS_BODY_MIN_LEVEL &&
						plus >= PLAYERBOT_BONUS_ARMOUR_MIN_PLUS;
			case ARMOR_SHIELD:
			case ARMOR_HEAD:
				return item->GetLevelLimit() >= PLAYERBOT_BONUS_ARMOUR_MIN_LEVEL &&
						plus >= PLAYERBOT_BONUS_ARMOUR_MIN_PLUS;
			case ARMOR_WRIST:
			case ARMOR_NECK:
			case ARMOR_FOOTS:
				return true;
			case ARMOR_EAR:
				return plus >= PLAYERBOT_BONUS_EAR_MIN_PLUS;
			default:
				return false;
		}
	}

	// An item the engine will actually accept a stone on, whatever the bot's
	// own rules say. UseItemEx refuses a costume, anything without an
	// attribute set (an arrow, a unique, a ring) and a piece in a trade, and
	// on mt2009 the wedding clothes and rings too; a locked piece is on a
	// counter. It refuses an equipped item outright as well ("if
	// (item2->IsEquipped()) return false"), so a worn piece has to come off
	// first - exactly as a player's does (ApplyPlayerBotBonusSteps).
	bool CanPlayerBotTakeBonusStone(LPITEM item)
	{
		if (!item || item->GetType() == ITEM_COSTUME || item->isLocked() || item->IsExchanging() ||
				item->GetAttributeSetIndex() == -1)
			return false;
		const DWORD vnum = item->GetVnum();
		return !((vnum >= 11901 && vnum <= 11904) || vnum == 50201 || vnum == 50202);
	}

	bool CanPlayerBotRerollItem(LPITEM item)
	{
		return CanPlayerBotTakeBonusStone(item) && IsPlayerBotBonusCategoryAllowed(item);
	}

	// The same, for a piece in its slot. The young bot's boots, necklace and
	// bracelet were bonused at any refine (community patch 2, point 3) until
	// Iwakura's Patch 4 put every piece under the categories above, +4 first.
	bool CanPlayerBotRerollItemFor(LPCHARACTER ch, LPITEM item, BYTE wearCell)
	{
		return CanPlayerBotRerollItem(item);
	}

	// Iwakura's Community Patch 5, point 5: "Zauwazono boty posiadajace
	// wybonowany ekwipunek (naszyjnik, buty, bransoleta), ktore mimo
	// posiadania np. 17 dodan i 9 zmianek w ogole z nich nie korzystaja. Bot
	// powinien w takiej sytuacji wykorzystac je na reszcie ekwipunku." The
	// rest of what a bot fights in is what the categories refuse for its
	// refine alone - an armour, a helmet, a shield or earrings under +7, a
	// weapon of forty-five under +7 - and the refine aim keeps the helmet and
	// the jewellery at +4 until the weapon, the armour and the shield stand at
	// +7, which stall at +6 for want of scrolls: on such a bot the categories
	// took no stone past the three pieces, whatever its bag held. Such a piece
	// takes the ordinary stones the categories cannot use now
	// (playerbot_bonus_rules.h, RestOpen). Nothing under +4 still (point 1 of
	// Patch 4), the categories' level floors stay, and so does the waste Patch
	// 4 names: a weapon under forty-five outside the level-30 family ("np.
	// Srebrny Miecz") takes no ordinary stone - a green one it may.
	bool IsPlayerBotBonusRestPiece(LPITEM item)
	{
		if (!CanPlayerBotTakeBonusStone(item) || IsPlayerBotBonusCategoryAllowed(item) ||
				(int)item->GetRefineLevel() < PLAYERBOT_BONUS_JEWEL_MIN_PLUS)
			return false;
		if (item->GetType() == ITEM_WEAPON)
			return IsPlayerBotSpecialLevel30Weapon(item) ||
					item->GetLevelLimit() >= PLAYERBOT_BONUS_WEAPON_MIN_LEVEL;
		if (item->GetType() != ITEM_ARMOR)
			return false;
		switch (item->GetSubType())
		{
			case ARMOR_BODY:
				return item->GetLevelLimit() >= PLAYERBOT_BONUS_BODY_MIN_LEVEL;
			case ARMOR_SHIELD:
			case ARMOR_HEAD:
				return item->GetLevelLimit() >= PLAYERBOT_BONUS_ARMOUR_MIN_LEVEL;
			case ARMOR_EAR:
				return true;
			default:
				// The bracelet, the necklace and the boots: the categories take
				// them from +4.
				return false;
		}
	}

	// How many of the boots, the necklace and the bracelet worn carry a
	// health line - the weapon's turn comes at two (community patch 2).
	int CountPlayerBotEarlyHpPieces(LPCHARACTER ch)
	{
		static const BYTE slots[] = { WEAR_FOOTS, WEAR_NECK, WEAR_WRIST };
		int pieces = 0;
		for (size_t s = 0; ch && s < sizeof(slots) / sizeof(slots[0]); ++s)
		{
			LPITEM worn = ch->GetWear(slots[s]);
			for (int i = 0; worn && i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				if (worn->GetAttributeType(i) == APPLY_MAX_HP && worn->GetAttributeValue(i) > 0)
				{
					++pieces;
					break;
				}
		}
		return pieces;
	}

	// Zielony Czar and Zielona Sila (71151/76023, 71152/76024) are the change
	// and add stones of the same kind as 71084/71085, and the engine lets a
	// player spend one only on a weapon or a body armour of level forty or less
	// (char_item.cpp, USE_CHANGE_ATTRIBUTE and USE_ADD_ATTRIBUTE). The bots never
	// spent one: under PLAYERBOT_BONUS_MIN_LEVEL no stone was spent at all, and
	// those are the bots whose gear the green stones are for ("Boty nie uzywaja
	// zielonego czaru i zielonego wzmocnienia", Sammy Suricate, 18 September).
	// The pass calls AddAttribute itself, so it has to keep the engine's rule
	// on its own: a green stone never goes on a helmet or a level-70 armour.
	bool IsPlayerBotGreenBonusStone(DWORD vnum)
	{
		return vnum == 71151 || vnum == 71152 || vnum == 76023 || vnum == 76024;
	}

	bool CanPlayerBotSpendGreenBonusStoneOn(LPITEM target)
	{
		if (!target)
			return false;
		if (target->GetType() != ITEM_WEAPON &&
				!(target->GetType() == ITEM_ARMOR && target->GetSubType() == ARMOR_BODY))
			return false;
		for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
			if (target->GetLimitType(i) == LIMIT_LEVEL &&
					target->GetLimitValue(i) > PLAYERBOT_GREEN_BONUS_MAX_LEVEL)
				return false;
		return true;
	}

	// Iwakura's Community Patch 5, point 9: "Obecnie boty bardzo slabo
	// wykorzystuja Zielony Czar i Zielona Sile. Czesto przetrzymuja je w
	// ekwipunku az do poznych faz gry." A piece a green stone goes on is the
	// engine's weapon or body armour of level forty or less, from +4 like
	// every stone (point 1 of Patch 4) - and none of the Patch 4 categories:
	// those took no weapon under forty-five but the level-30 family and no
	// armour under +7, the young bot's weapon waited for health lines on the
	// jewellery, and a green stone fits nothing else, so the level-20 chest's
	// three and three lay in the bag for the rest of the game.
	bool CanPlayerBotTakeGreenBonusStone(LPITEM item)
	{
		return CanPlayerBotTakeBonusStone(item) && CanPlayerBotSpendGreenBonusStoneOn(item) &&
				(int)item->GetRefineLevel() >= PLAYERBOT_BONUS_JEWEL_MIN_PLUS;
	}

	// A weapon whose proto carries the damage addon: CItem::ChangeAttribute
	// applies the addon again before it rolls the rest, so a change stone
	// rolls its average and skill lines anew - the level-30 and level-75
	// families on these files. On any other weapon those two lines never roll.
	bool HasPlayerBotDamageAddon(LPITEM item)
	{
		return item && item->GetProto() && item->GetProto()->sAddonType != 0;
	}

	// Whether an add stone or the marble can still land a line on this piece.
	// PutAttributeWithLevel draws among the lines the piece's attribute set
	// allows and the piece does not carry yet, and with none left it adds
	// nothing while the stone is spent all the same - so such a piece takes no
	// add, and a pass that came back to it every thirty seconds would empty
	// the bag into it.
	bool CanPlayerBotItemRollNewLine(LPITEM item)
	{
		const int set = item ? item->GetAttributeSetIndex() : -1;
		if (set < 0 || set >= ATTRIBUTE_SET_MAX_NUM)
			return false;
		for (TItemAttrMap::const_iterator it = g_map_itemAttr.begin(); it != g_map_itemAttr.end(); ++it)
			if (it->second.bMaxLevelBySet[set] != 0 && it->second.dwProb != 0 &&
					!item->HasAttr((BYTE)it->first))
				return true;
		return false;
	}

	// The bag stone of the kind a vnum names: the change stone is
	// USE_CHANGE_ATTRIBUTE and the add stone USE_ADD_ATTRIBUTE, and on these
	// files each comes in three vnums (71084/71151/76023, 71085/71152/76024) -
	// a bot counting only its own vnum vendored the others. For a target, the
	// stone that may go on it: a green one first where the target takes one -
	// it is good for nothing else - and a plain one otherwise, unless the bot
	// is young enough to spend green ones only (greenOnly). Without a target,
	// any stone of the kind: the "is there anything to spend" of the pass.
	int FindPlayerBotBonusStoneCellLike(LPCHARACTER ch, DWORD vnum, LPITEM target = NULL, bool greenOnly = false)
	{
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
		if (!ch || !proto)
			return -1;
		int plain = -1;
		int green = -1;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM stone = ch->GetInventoryItem(cell);
			if (!stone || stone->GetType() != proto->bType || stone->GetSubType() != proto->bSubType)
				continue;
			if (IsPlayerBotGreenBonusStone(stone->GetVnum()))
			{
				if (green < 0)
					green = cell;
			}
			else if (plain < 0)
				plain = cell;
		}
		if (!target)
			return greenOnly ? green : (plain >= 0 ? plain : green);
		if (green >= 0 && CanPlayerBotSpendGreenBonusStoneOn(target))
			return green;
		return greenOnly ? -1 : plain;
	}

	// A bot spends the stones it holds and no others. It used to make one out
	// of nothing whenever the bag had none - AutoGiveItem for a price in yang,
	// on the grounds that the stones could not be dropped, traded or shopped.
	// That was never true of mt2009: both drop from monsters
	// (mob_drop_item.txt) and come out of chests and the Moonlight chest, and on
	// a world with the chests switched off the gear history showed bots
	// spending Wzmocnienie Przedmiotu that no bag had ever received and the
	// economy charts had none of ("boty zmieniaja oraz dodaja bonusy bez
	// przedmiotu", seban latino and Drip, 15 September). The operator's rule is
	// the marble's: a bot without a stone does without, the way a player does.
	bool HasPlayerBotBonusStone(LPCHARACTER ch, DWORD vnum, bool greenOnly = false)
	{
		return ch && FindPlayerBotBonusStoneCellLike(ch, vnum, NULL, greenOnly) >= 0;
	}

	// The stone FindPlayerBotBonusStoneCellLike chose for a piece, by its cell.
	bool ConsumePlayerBotBonusStoneAt(LPCHARACTER ch, int cell)
	{
		if (!ch || cell < 0 || cell >= PLAYERBOT_BAG_CELLS)
			return false;
		LPITEM stone = ch->GetInventoryItem((WORD)cell);
		if (!stone)
			return false;
		if (stone->GetCount() > 1)
			stone->SetCount(stone->GetCount() - 1);
		else
			ITEM_MANAGER::instance().RemoveItem(stone, "PLAYERBOT_BONUS");
		return true;
	}

	// The stones in the bag by kind, the first cell of each: the add stone is
	// USE_ADD_ATTRIBUTE and the change stone USE_CHANGE_ATTRIBUTE, each in an
	// ordinary flavour (71085/71084 and the ItemShop's copies) and a green one
	// (IsPlayerBotGreenBonusStone), and the marble USE_ADD_ATTRIBUTE2. Read
	// once for a choice and again after every stone spent.
	struct TPlayerBotBonusBag
	{
		int greenAdd;
		int greenChange;
		int plainAdd;
		int plainChange;
		int marble;
	};

	void ReadPlayerBotBonusBag(LPCHARACTER ch, TPlayerBotBonusBag& bag)
	{
		bag.greenAdd = bag.greenChange = bag.plainAdd = bag.plainChange = bag.marble = -1;
		for (WORD cell = 0; ch && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM stone = ch->GetInventoryItem(cell);
			if (!stone || stone->GetType() != ITEM_USE || stone->GetCount() == 0 || stone->isLocked())
				continue;
			const bool green = IsPlayerBotGreenBonusStone(stone->GetVnum());
			int* first = NULL;
			switch (stone->GetSubType())
			{
				case USE_ADD_ATTRIBUTE:
					first = green ? &bag.greenAdd : &bag.plainAdd;
					break;
				case USE_CHANGE_ATTRIBUTE:
					first = green ? &bag.greenChange : &bag.plainChange;
					break;
				case USE_ADD_ATTRIBUTE2:
					first = &bag.marble;
					break;
				default:
					break;
			}
			if (first && *first < 0)
				*first = (int)cell;
		}
	}

	bool HasPlayerBotAnyBonusStone(const TPlayerBotBonusBag& bag)
	{
		return bag.greenAdd >= 0 || bag.greenChange >= 0 || bag.plainAdd >= 0 ||
				bag.plainChange >= 0 || bag.marble >= 0;
	}

	playerbot_bonus_rules::TBag GetPlayerBotBonusBagKinds(const TPlayerBotBonusBag& bag)
	{
		const playerbot_bonus_rules::TBag kinds = {
			bag.greenAdd >= 0, bag.greenChange >= 0, bag.plainAdd >= 0, bag.plainChange >= 0, bag.marble >= 0
		};
		return kinds;
	}

	playerbot_bonus_rules::TLimits GetPlayerBotBonusLimits()
	{
		const playerbot_bonus_rules::TLimits limits = {
			PLAYERBOT_BONUS_MAX_LINES, PLAYERBOT_BONUS_CHANGE_MIN_LINES
		};
		return limits;
	}

	// What one stone would do to a piece now, and where the pass found it.
	//
	// Iwakura's QUICK FIX nr 3 (23 September) is both halves of this. "Boty
	// musza skupic sie na bonowaniu jednego przedmiotu na raz": the pass walked
	// the slots and gave each piece one stone, three a visit, so a bot added a
	// line to the boots, one to the necklace and one to the bracelet and
	// finished none of them. And "zmianki wylacznie na przedmiotach, ktore
	// posiadaja juz co najmniej 3 dodane bonusy (z priorytetem dobicia do
	// pelnych 4)": PLAYERBOT_BONUS_CHANGE_MIN_LINES. One function answers for
	// every place a piece can be (worn, held for a slot, kept for sale), so the
	// pass, the choice of the next piece and the swap rule's wait
	// (PlayerBotHoldsBonusStoneFor) cannot disagree about a piece. The
	// choice itself is playerbot_bonus_rules.h, pure and unit-tested.
	enum EPlayerBotBonusStep
	{
		PLAYERBOT_BONUS_STEP_NONE,
		PLAYERBOT_BONUS_STEP_ADD,
		PLAYERBOT_BONUS_STEP_MARBLE,
		PLAYERBOT_BONUS_STEP_CHANGE,
	};

	enum EPlayerBotBonusTargetKind
	{
		PLAYERBOT_BONUS_TARGET_WORN,
		// The new piece the swap rule holds in the bag for a slot
		// (IsPlayerBotSwapHeldForBonus), bonused where it lies.
		PLAYERBOT_BONUS_TARGET_HELD,
		// A piece in the bag kept for sale, bonused where it lies: a level-30
		// weapon, or the piece a caller of ApplyPlayerBotGreenBonusToItem names.
		PLAYERBOT_BONUS_TARGET_GOODS,
	};

	// plain is how the ordinary stones may reach the piece
	// (playerbot_bonus_rules::EPlain): the categories, the rest of the worn
	// gear, or green stones only. The young bot's rule is read when the step
	// is (IsPlayerBotGreenOnlyFor), not stored here.
	struct TPlayerBotBonusTarget
	{
		LPITEM item;
		BYTE wearCell;
		BYTE kind;
		BYTE plain;
	};

	const char* GetPlayerBotBonusStepName(EPlayerBotBonusStep step)
	{
		switch (step)
		{
			case PLAYERBOT_BONUS_STEP_ADD: return "add";
			case PLAYERBOT_BONUS_STEP_MARBLE: return "marble";
			case PLAYERBOT_BONUS_STEP_CHANGE: return "change";
			default: return "none";
		}
	}

	// Community Patch 1 sends a young bot's ordinary stones to the necklace,
	// the bracelet and the boots first. Once none of the three it wears can
	// take a line from the bag, the rest of the gear is next (Iwakura, 26
	// September: "po wybonowaniu butow, naszyjnika i bransolety, bot powinien
	// automatycznie przechodzic do kolejnych przedmiotow") - until then the
	// stones of 327 of 346 bots holding them on m2zip had no piece they were
	// allowed on, and 17 adds and 9 changes rode in one bag for good.
	bool IsPlayerBotEarlyBonusDone(LPCHARACTER ch)
	{
		static const BYTE slots[] = { WEAR_NECK, WEAR_WRIST, WEAR_FOOTS };
		for (size_t s = 0; ch && s < sizeof(slots) / sizeof(slots[0]); ++s)
		{
			LPITEM worn = ch->GetWear(slots[s]);
			if (worn && worn->GetAttributeCount() < PLAYERBOT_BONUS_MAX_LINES &&
					CanPlayerBotRerollItemFor(ch, worn, slots[s]) &&
					FindPlayerBotBonusStoneCellLike(ch, PLAYERBOT_BONUS_ADD_VNUM, worn, false) >= 0)
				return false;
		}
		return true;
	}

	// Whether a young bot may spend only the green stones on this piece: yes
	// on goods, never on the three early pieces, and on the rest only until
	// those three have taken what the bag can give them.
	bool IsPlayerBotGreenOnlyFor(LPCHARACTER ch, const TPlayerBotBonusTarget& target)
	{
		if (!ch || ch->GetLevel() >= PLAYERBOT_BONUS_MIN_LEVEL)
			return false;
		if (target.kind == PLAYERBOT_BONUS_TARGET_GOODS)
			return true;
		return !IsPlayerBotEarlyBonusSlot(ch, target.wearCell) && !IsPlayerBotEarlyBonusDone(ch);
	}

	// Whether a change stone - an ordinary one, or a green one - would still
	// move this piece towards its finish: the one answer for the pass and for
	// the ItemShop's purchase of a change stone (PlayerBotWantsChangeStone).
	// An item that has landed the roll its slot is bought for is finished: it
	// can still gain a line - that cannot lose what is already there - but it
	// is never rerolled, whatever the score says. Short of that an ordinary
	// change stops at the line score (PLAYERBOT_BONUS_KEEP_SCORE, every line
	// weighed by Iwakura's tier), except where the operator mixes to the
	// finish: a level-30 weapon is rerolled until it lands its average line,
	// whatever the score says - the score is a sum of good lines, and a weapon
	// full of them at twelve percent average was "good enough" to the score
	// and not to anybody who looked at it; one kept for sale sells for that
	// line (PLAYERBOT_PRIZE_AVERAGE_DAMAGE) at a stone's fortieth of what the
	// finished piece asks - and the young bot's jewellery and boots are
	// changed until they carry the lines community patch 2, point 3 requires.
	// Why the rest keeps the score's stop is playerbot_bonus_rules::WantsChange:
	// one piece takes every stone while it can use one. A green change mixes
	// to the finish wherever a change can roll it (ChangeReachesFinish).
	bool PlayerBotWantsBonusChange(LPCHARACTER ch, const TPlayerBotBonusTarget& target, bool green)
	{
		LPITEM item = target.item;
		if (!ch || !item)
			return false;
		// Rerolled until its lines beat the worn piece's, which is what ends
		// the hold (IsPlayerBotSwapHeldForBonus).
		if (target.kind == PLAYERBOT_BONUS_TARGET_HELD)
			return true;
		// MT2009_PLUS_PROGRESSION_V1: a bot the checklist holds for health from
		// its items changes a worn piece that can carry a health line and has
		// none (item_attr: body, boots, bracelet, necklace).
		if (!green && target.kind == PLAYERBOT_BONUS_TARGET_WORN &&
				(GetPlayerBotProgressionNeeds(ch->GetPlayerID()) & playerbot_progression::NEED_HP) != 0 &&
				(target.wearCell == WEAR_BODY || target.wearCell == WEAR_FOOTS ||
				 target.wearCell == WEAR_WRIST || target.wearCell == WEAR_NECK))
		{
			bool hasHp = false;
			for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				if (item->GetAttributeType(i) == APPLY_MAX_HP && item->GetAttributeValue(i) > 0)
					hasHp = true;
			if (!hasHp)
				return true;
		}
		const bool mixToFinish = green
				? playerbot_bonus_rules::ChangeReachesFinish(item->GetType() == ITEM_WEAPON,
					HasPlayerBotDamageAddon(item))
				: (IsPlayerBotSpecialLevel30WeaponVnum(item->GetVnum()) ||
					(target.kind != PLAYERBOT_BONUS_TARGET_GOODS && IsPlayerBotEarlyBonusSlot(ch, target.wearCell)));
		return playerbot_bonus_rules::WantsChange(HasPlayerBotFinishedBonus(ch, item, target.wearCell),
				mixToFinish, ScorePlayerBotItemBonuses(ch, item, target.wearCell), PLAYERBOT_BONUS_KEEP_SCORE);
	}

	// A piece as playerbot_bonus_rules.h weighs it.
	playerbot_bonus_rules::TPiece BuildPlayerBotBonusPiece(LPCHARACTER ch, const TPlayerBotBonusTarget& target)
	{
		playerbot_bonus_rules::TPiece piece;
		LPITEM item = target.item;
		piece.lines = item ? item->GetAttributeCount() : 0;
		piece.lineRolls = piece.lines < PLAYERBOT_BONUS_MARBLE_LINES && CanPlayerBotItemRollNewLine(item);
		// A young bot spends the green stones only - except on the necklace,
		// the bracelet and the boots, which a green stone cannot touch at all,
		// so those three take an ordinary one even under the level, and on the
		// rest once those three are done (IsPlayerBotGreenOnlyFor).
		piece.plain = (playerbot_bonus_rules::EPlain)target.plain;
		if (piece.plain != playerbot_bonus_rules::PLAIN_NONE && IsPlayerBotGreenOnlyFor(ch, target))
			piece.plain = playerbot_bonus_rules::PLAIN_NONE;
		piece.greenFits = CanPlayerBotTakeGreenBonusStone(item);
		// The fifth line is the marble's, on a worn piece.
		piece.marbleAllowed = target.kind == PLAYERBOT_BONUS_TARGET_WORN;
		piece.wantsPlainChange = PlayerBotWantsBonusChange(ch, target, false);
		piece.wantsGreenChange = piece.greenFits && PlayerBotWantsBonusChange(ch, target, true);
		// What a companion's owner put on keeps the lines the owner chose it
		// for (playerbot_sidekick.h); a line added above loses nothing.
		piece.pinned = IsPlayerBotSidekickPinned(ch, item);
		piece.goods = target.kind == PLAYERBOT_BONUS_TARGET_GOODS;
		// The green round's order: the armour, then the weapon - worn, or the
		// piece the swap rule holds for the slot.
		piece.greenRank = piece.goods ? -1
				: (target.wearCell == WEAR_BODY ? 0 : (target.wearCell == WEAR_WEAPON ? 1 : -1));
		return piece;
	}

	// The stone a choice names, by its cell in the bag.
	EPlayerBotBonusStep ResolvePlayerBotBonusChoice(const playerbot_bonus_rules::TStepChoice& choice,
			const TPlayerBotBonusBag& bag, int& stoneCell)
	{
		const bool green = choice.stone == playerbot_bonus_rules::STONE_GREEN;
		EPlayerBotBonusStep step = PLAYERBOT_BONUS_STEP_NONE;
		stoneCell = -1;
		switch (choice.step)
		{
			case playerbot_bonus_rules::STEP_ADD:
				stoneCell = green ? bag.greenAdd : bag.plainAdd;
				step = PLAYERBOT_BONUS_STEP_ADD;
				break;
			case playerbot_bonus_rules::STEP_MARBLE:
				stoneCell = bag.marble;
				step = PLAYERBOT_BONUS_STEP_MARBLE;
				break;
			case playerbot_bonus_rules::STEP_CHANGE:
				stoneCell = green ? bag.greenChange : bag.plainChange;
				step = PLAYERBOT_BONUS_STEP_CHANGE;
				break;
			default:
				break;
		}
		return stoneCell >= 0 ? step : PLAYERBOT_BONUS_STEP_NONE;
	}

	// restOpen: the kinds of ordinary stone the rest of the worn gear may take
	// in this pass (playerbot_bonus_rules::RestOpen). greenStonesOnly: a green
	// stone or nothing.
	EPlayerBotBonusStep GetPlayerBotBonusStep(LPCHARACTER ch, const TPlayerBotBonusTarget& target, int& stoneCell,
			unsigned restOpen = 0, bool greenStonesOnly = false)
	{
		stoneCell = -1;
		if (!ch || !target.item)
			return PLAYERBOT_BONUS_STEP_NONE;
		TPlayerBotBonusBag bag;
		ReadPlayerBotBonusBag(ch, bag);
		if (!HasPlayerBotAnyBonusStone(bag))
			return PLAYERBOT_BONUS_STEP_NONE;
		const playerbot_bonus_rules::TStepChoice choice = playerbot_bonus_rules::StepFor(
				BuildPlayerBotBonusPiece(ch, target), GetPlayerBotBonusBagKinds(bag),
				GetPlayerBotBonusLimits(), restOpen, greenStonesOnly);
		return ResolvePlayerBotBonusChoice(choice, bag, stoneCell);
	}

	// Whether the bag holds a stone this piece could take now. The swap rule
	// waits only while the waiting can end (IsPlayerBotSwapHeldForBonus), so
	// this asks exactly what the bonus pass asks of a held piece: an armour
	// under PLAYERBOT_BONUS_MIN_REFINE, or a piece that takes no lines at all,
	// is never worked on there, and a stone in the bag for it would have held
	// a bought +0 armour back from its owner for good.
	bool PlayerBotHoldsBonusStoneFor(LPCHARACTER ch, LPITEM item)
	{
		if (!ch || !item)
			return false;
		const int slot = item->FindEquipCell(ch);
		if (slot < 0 || slot >= WEAR_MAX_NUM || !CanPlayerBotRerollItemFor(ch, item, (BYTE)slot))
			return false;
		const TPlayerBotBonusTarget target = { item, (BYTE)slot, (BYTE)PLAYERBOT_BONUS_TARGET_HELD,
				(BYTE)playerbot_bonus_rules::PLAIN_CATEGORY };
		int stoneCell = -1;
		return GetPlayerBotBonusStep(ch, target, stoneCell) != PLAYERBOT_BONUS_STEP_NONE;
	}

	// The pieces the pass may spend a stone on, in the order they matter. Past
	// the early band it is the order the gear matters in; under it, Community
	// Patch 1 puts the necklace, the wrist and the boots first, because that is
	// where a line is worth more than a grade of refine. The piece the swap
	// rule holds for a slot stands in for the worn one - a stone on the piece
	// about to come off is a stone thrown away - and the level-30 weapons kept
	// for sale come last. Other spares in the bag are sold or put in a stall
	// long before they are worth polishing.
	// Each worn piece is listed with how the ordinary stones may reach it: the
	// categories of Patch 4, the rest of the gear (IsPlayerBotBonusRestPiece,
	// Patch 5, point 5), or none at all where only a green stone fits it
	// (Patch 5, point 9).
	void CollectPlayerBotBonusTargets(LPCHARACTER ch, std::vector<TPlayerBotBonusTarget>& out)
	{
		out.clear();
		if (!ch)
			return;
		static const BYTE wearSlotsLate[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_HEAD, WEAR_SHIELD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		// After the three early pieces: the armour and the earrings, then the
		// rest (Iwakura, 26 September).
		static const BYTE wearSlotsEarly[] = {
			WEAR_NECK, WEAR_WRIST, WEAR_FOOTS,
			WEAR_WEAPON, WEAR_BODY, WEAR_EAR, WEAR_HEAD, WEAR_SHIELD
		};
		const bool early = ch->GetLevel() < PLAYERBOT_EARLY_BONUS_MAX_LEVEL;
		const BYTE* wearSlots = early ? wearSlotsEarly : wearSlotsLate;
		const size_t wearSlotCount = early ? sizeof(wearSlotsEarly) / sizeof(wearSlotsEarly[0])
				: sizeof(wearSlotsLate) / sizeof(wearSlotsLate[0]);

		// The piece held for a slot is the one the equipment pass would put on
		// but for its lines: better than the worn one by that pass's own test
		// (IsPlayerBotWearableUpgrade) and the best such piece for the slot.
		// The first bag piece with weaker lines than the worn one's used to
		// do - the piece just taken off, a gambler's stock - and the stones
		// went into what would never be worn (B02 of Iwakura's audit).
		LPITEM held[WEAR_MAX_NUM] = {};
		long long heldScore[WEAR_MAX_NUM] = {};
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->IsEquipped() ||
					!IsPlayerBotEquipmentCandidate(ch, item))
				continue;
			const int slot = item->FindEquipCell(ch);
			if (slot < 0 || slot >= WEAR_MAX_NUM)
				continue;
			// Nor for a slot whose worn piece a companion's owner put on: the
			// equipment pass never takes it off, so a piece held behind it is
			// one the companion will never wear (Iwakura's audit, B02).
			LPITEM worn = ch->GetWear((BYTE)slot);
			if (!worn || IsPlayerBotSidekickPinned(ch, worn) || !IsPlayerBotWearableUpgrade(ch, item, cell) ||
					!IsPlayerBotSwapHeldForBonus(ch, item, worn) ||
					!CanPlayerBotRerollItemFor(ch, item, (BYTE)slot))
				continue;
			const long long score = GetPlayerBotEquipmentScore(item, ch);
			if (!held[slot] || score > heldScore[slot])
			{
				held[slot] = item;
				heldScore[slot] = score;
			}
		}

		// The weapon waits for the health lines (community patch 2, point 3):
		// two of the boots, the necklace and the bracelet first - for the
		// ordinary stones. A green one goes on none of those three, and Patch
		// 5, point 9 sends it to the armour and then to the weapon.
		const bool weaponWaits = early &&
				CountPlayerBotEarlyHpPieces(ch) < PLAYERBOT_EARLY_HP_PIECES_FOR_WEAPON;
		bool listed[WEAR_MAX_NUM] = {};
		for (size_t i = 0; i < wearSlotCount; ++i)
		{
			const BYTE wearCell = wearSlots[i];
			listed[wearCell] = true;
			const bool plainWaits = weaponWaits && wearCell == WEAR_WEAPON;
			if (held[wearCell])
			{
				const TPlayerBotBonusTarget target = { held[wearCell], wearCell, (BYTE)PLAYERBOT_BONUS_TARGET_HELD,
						(BYTE)(plainWaits ? playerbot_bonus_rules::PLAIN_NONE : playerbot_bonus_rules::PLAIN_CATEGORY) };
				out.push_back(target);
				continue;
			}
			// A worn piece comes off for its stones, so only one the engine
			// really wears in that slot and lets come off.
			LPITEM item = ch->GetWear(wearCell);
			if (!item || !IsPlayerBotWornItemSound(ch, item, wearCell) ||
					IS_SET(item->GetFlag(), ITEM_FLAG_IRREMOVABLE))
				continue;
			playerbot_bonus_rules::EPlain plain;
			if (CanPlayerBotRerollItemFor(ch, item, wearCell))
				plain = playerbot_bonus_rules::PLAIN_CATEGORY;
			else if (IsPlayerBotBonusRestPiece(item))
				plain = playerbot_bonus_rules::PLAIN_REST;
			else if (CanPlayerBotTakeGreenBonusStone(item))
				plain = playerbot_bonus_rules::PLAIN_NONE;
			else
				continue;
			if (plainWaits)
				plain = playerbot_bonus_rules::PLAIN_NONE;
			const TPlayerBotBonusTarget target = { item, wearCell, (BYTE)PLAYERBOT_BONUS_TARGET_WORN, (BYTE)plain };
			out.push_back(target);
		}
		// A piece held for a slot the order above does not name.
		for (int slot = 0; slot < WEAR_MAX_NUM; ++slot)
			if (held[slot] && !listed[slot])
			{
				const TPlayerBotBonusTarget target = { held[slot], (BYTE)slot, (BYTE)PLAYERBOT_BONUS_TARGET_HELD,
						(BYTE)playerbot_bonus_rules::PLAIN_CATEGORY };
				out.push_back(target);
			}

		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->IsEquipped() ||
					!IsPlayerBotSpecialLevel30Weapon(item) || !CanPlayerBotRerollItem(item))
				continue;
			bool already = false;
			for (size_t i = 0; i < out.size() && !already; ++i)
				already = out[i].item == item;
			if (already)
				continue;
			const TPlayerBotBonusTarget target = { item, (BYTE)WEAR_WEAPON, (BYTE)PLAYERBOT_BONUS_TARGET_GOODS,
					(BYTE)playerbot_bonus_rules::PLAIN_CATEGORY };
			out.push_back(target);
		}
	}

	// One stone, the one GetPlayerBotBonusStep chose, on a piece the engine can
	// touch now: in the bag, or taken off for it (ApplyPlayerBotBonusSteps).
	void SpendPlayerBotBonusStone(LPCHARACTER ch, const TPlayerBotBonusTarget& target,
			EPlayerBotBonusStep step, int stoneCell)
	{
		LPITEM item = target.item;
		LPITEM stone = ch->GetInventoryItem((WORD)stoneCell);
		const DWORD stoneVnum = stone ? stone->GetVnum() : 0;
		const int count = item->GetAttributeCount();
		const int scoreBefore = ScorePlayerBotItemBonuses(ch, item, target.wearCell);
		const long long valueBefore = GetPlayerBotItemLineScore(item, ch);
		// The engine's own odds for a line, aiItemAttributeAddPercent by the
		// count already there (100/80/60/50, and 30 for the marble's fifth);
		// the stone or the marble is spent whether the roll lands or not, as at
		// the counter.
		if (step == PLAYERBOT_BONUS_STEP_CHANGE)
			item->ChangeAttribute();
		else if (number(1, 100) <= aiItemAttributeAddPercent[count])
			item->AddAttribute();
		ConsumePlayerBotBonusStoneAt(ch, stoneCell);

		// The gear history shows the stone spent (PLAYERBOT_BONUS); this names
		// the piece it was spent on, which is what a player asks - "na jaki
		// przedmiot" (13 September).
		LogManager::instance().ItemLog(ch, item,
				step == PLAYERBOT_BONUS_STEP_MARBLE ? "PLAYERBOT_BONUS_MARBLE"
					: (step == PLAYERBOT_BONUS_STEP_ADD ? "PLAYERBOT_BONUS_ADD" : "PLAYERBOT_BONUS_CHANGE"),
				item->GetName());
		const char* what = step == PLAYERBOT_BONUS_STEP_CHANGE ? "rerolled"
				: (step == PLAYERBOT_BONUS_STEP_MARBLE ? "marbled" : "added");
		// stone= names the stone spent, so the green ones can be told from the
		// ordinary (Patch 5, point 9), and rest=1 a piece of the rest of the
		// gear (point 5).
		if (target.kind == PLAYERBOT_BONUS_TARGET_HELD)
		{
			LPITEM wornNow = ch->GetWear(target.wearCell);
			sys_log(0, "PLAYERBOT_BONUS: %s held piece pid=%u name=%s vnum=%u slot=%u lines=%d->%d value=%lld->%lld worn_value=%lld stone=%u",
					what, ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)target.wearCell,
					count, item->GetAttributeCount(), valueBefore, GetPlayerBotItemLineScore(item, ch),
					wornNow ? GetPlayerBotItemLineScore(wornNow, ch) : 0LL, (unsigned int)stoneVnum);
		}
		else
		{
			sys_log(0, "PLAYERBOT_BONUS: %s%s pid=%u name=%s vnum=%u slot=%u lines=%d->%d score=%d->%d gold=%lld stone=%u rest=%d",
					what, target.kind == PLAYERBOT_BONUS_TARGET_GOODS ? " goods" : "",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum(), (unsigned int)target.wearCell,
					count, item->GetAttributeCount(), scoreBefore,
					ScorePlayerBotItemBonuses(ch, item, target.wearCell),
					(long long)(ch->GetGold() / 1000), (unsigned int)stoneVnum,
					target.plain == playerbot_bonus_rules::PLAIN_REST ? 1 : 0);
		}
	}

	// Up to maxStones on one piece, the step asked again after each; the
	// stones spent. A worn piece has to come off for the engine to touch it -
	// UseItemEx refuses an equipped item outright - and it has to go back on
	// afterwards: a bot walking about with its weapon in the bag would be
	// worse than any line it could win. It comes off once for all of them and
	// goes back on once. It came off and went back on for every stone, three
	// times a pass and a pass every thirty seconds while a piece was worked
	// (Iwakura's audit, R3), each time the equip window's to refuse. The
	// caller asks that window first (IsPlayerBotEquipWindowShut), as it asks
	// for a stone the piece can take: nothing comes off that no stone would go
	// on. A piece in the bag is worked on where it lies.
	int ApplyPlayerBotBonusSteps(LPCHARACTER ch, const TPlayerBotBonusTarget& target, EPlayerBotBonusStep step,
			int stoneCell, int maxStones, unsigned restOpen, bool greenStonesOnly)
	{
		LPITEM item = target.item;
		if (!ch || !item || step == PLAYERBOT_BONUS_STEP_NONE || stoneCell < 0 || maxStones <= 0 ||
				IsPlayerBotGearFrozen(ch))
			return 0;
		const bool worn = target.kind == PLAYERBOT_BONUS_TARGET_WORN;
		if (worn && !ch->UnequipItem(item))
			return 0;
		int used = 0;
		while (used < maxStones && step != PLAYERBOT_BONUS_STEP_NONE && stoneCell >= 0)
		{
			SpendPlayerBotBonusStone(ch, target, step, stoneCell);
			++used;
			step = GetPlayerBotBonusStep(ch, target, stoneCell, restOpen, greenStonesOnly);
		}
		if (worn && !PlayerBotEquipItem(ch, item))
			sys_err("PLAYERBOT_BONUS: could not re-equip pid=%u name=%s vnum=%u slot=%u stones=%d",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum(),
					(unsigned int)target.wearCell, used);
		return used;
	}

	// One piece at a time (QUICK FIX nr 3, above). A visit spends its stones on
	// one piece, and the next visit comes back to it: the remembered piece
	// (dwBonusFocusItem) keeps the stones for as long as one in the bag fits
	// it, whatever it is doing - filled to four lines, the marble's fifth,
	// then changed until it is finished. Only when it is done, or nothing in
	// the bag fits it, is another piece taken up, and then the first in the
	// order that can take a line before the first that can take a change, so
	// pieces are filled before anything is mixed. No stone waits in the bag
	// for a piece that cannot take it: a bot with change stones and no add
	// stone mixes a full piece rather than hold them for the one still short.
	// Iwakura's Patch 4, point 11: a worn piece of four lines that the
	// marble's fifth would go on, and no marble in the bag - what a hundred
	// of the Alchemist's dust are turned into a marble for, and kept back for
	// meanwhile (GetPlayerBotStallBaseKeep).
	bool PlayerBotWantsBlessingMarble(LPCHARACTER ch)
	{
		if (!ch || !ch->IsItemLoaded() || FindPlayerBotBlessingMarbleCell(ch) >= 0)
			return false;
		static const BYTE slots[] = {
			WEAR_WEAPON, WEAR_BODY, WEAR_SHIELD, WEAR_HEAD,
			WEAR_FOOTS, WEAR_WRIST, WEAR_NECK, WEAR_EAR
		};
		for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i)
		{
			// The marble goes where GetPlayerBotBonusStep would put it: a worn
			// piece, not one a young bot may give green stones only.
			LPITEM worn = ch->GetWear(slots[i]);
			const TPlayerBotBonusTarget target = { worn, slots[i], (BYTE)PLAYERBOT_BONUS_TARGET_WORN,
					(BYTE)playerbot_bonus_rules::PLAIN_CATEGORY };
			if (IsPlayerBotGreenOnlyFor(ch, target))
				continue;
			if (worn && worn->GetAttributeCount() == PLAYERBOT_BONUS_MAX_LINES &&
					CanPlayerBotRerollItemFor(ch, worn, slots[i]))
				return true;
		}
		return false;
	}

	// And the turning, "magicznie" - no NPC does it on these files. The marble
	// rolls at its own odds when it is used, as a player's does.
	bool ManagePlayerBotDustMarble(LPCHARACTER ch)
	{
		if (!PlayerBotWantsBlessingMarble(ch) ||
				(int)ch->CountSpecifyItem(PLAYERBOT_MAGIC_DUST_VNUM) < PLAYERBOT_DUST_PER_MARBLE ||
				ch->GetEmptyInventory(1) < 0)
			return false;
		ch->RemoveSpecifyItem(PLAYERBOT_MAGIC_DUST_VNUM, PLAYERBOT_DUST_PER_MARBLE);
		LPITEM marble = ch->AutoGiveItem(PLAYERBOT_BLESSING_MARBLE_VNUM, 1, -1, false);
		if (marble)
			LogManager::instance().ItemLog(ch, marble, "PLAYERBOT_DUST_MARBLE", marble->GetName());
		sys_log(0, "PLAYERBOT_BONUS: dust into a marble pid=%u name=%s dust_left=%d ok=%d",
				ch->GetPlayerID(), ch->GetName(), (int)ch->CountSpecifyItem(PLAYERBOT_MAGIC_DUST_VNUM),
				marble ? 1 : 0);
		return marble != NULL;
	}

	// The refiners' exchange (Iwakura): with the market flooded by a refine
	// material - over PLAYERBOT_EXCHANGE_FLOOD_UNITS on the stands at no more
	// than PLAYERBOT_EXCHANGE_UNIT_PRICE_MAX a piece - a bot with a surplus of
	// it trades PLAYERBOT_EXCHANGE_STONE_UNITS for a Zaczarowanie or a
	// Wzmocnienie Przedmiotu (the one it has fewer of), or
	// PLAYERBOT_EXCHANGE_MARBLE_UNITS for a Marmur Blogoslawienstwa when it
	// wears PLAYERBOT_EXCHANGE_MARBLE_PIECES pieces of four lines or more.
	// The fee is PLAYERBOT_EXCHANGE_FEE on the price sheet's yang scale; one
	// time in PLAYERBOT_EXCHANGE_SUCCESS_PERCENT it works, and a failure keeps
	// the materials and the fee. "Magicznie", as the dust's marble: no NPC.
	// While the flood lasts a bot short of the stones may buy the material
	// off the stands for it (IsPlayerBotExchangeBuyOffer).
	const int PLAYERBOT_EXCHANGE_STONE_UNITS = 20;
	const int PLAYERBOT_EXCHANGE_MARBLE_UNITS = 75;
	const int PLAYERBOT_EXCHANGE_MARBLE_PIECES = 4;
	const int PLAYERBOT_EXCHANGE_MARBLE_LINES = 4;
	const DWORD PLAYERBOT_EXCHANGE_FLOOD_UNITS = 200;
	const DWORD PLAYERBOT_EXCHANGE_UNIT_PRICE_MAX = 70000;
	const DWORD PLAYERBOT_EXCHANGE_FEE = 500000;
	const int PLAYERBOT_EXCHANGE_SUCCESS_PERCENT = 60;
	const int PLAYERBOT_EXCHANGE_STONE_KEEP = 5;
	const int PLAYERBOT_EXCHANGE_BUY_PERCENT = 10;

	DWORD GetPlayerBotShopAskingPrice(LPITEM item);
	int GetPlayerBotRefineMaterialReserve(LPCHARACTER ch, DWORD materialVnum);

	// A material the market is flooded with, judged by one piece of it.
	bool IsPlayerBotExchangeFlooded(LPITEM item)
	{
		if (!item || !IsPlayerBotTradeableMaterial(item) || IsPlayerBotSafeRefineScroll(item->GetVnum()))
			return false;
		const TPlayerBotMarketLedgerEntry* supply = GetPlayerBotMarketLedgerEntry(item->GetVnum());
		if (!supply || supply->dwSupplyUnits <= PLAYERBOT_EXCHANGE_FLOOD_UNITS)
			return false;
		const DWORD unit = GetPlayerBotShopAskingPrice(item) / std::max<DWORD>(1, (DWORD)item->GetCount());
		return unit > 0 && unit <= ScalePlayerBotIwakuraPrice(PLAYERBOT_EXCHANGE_UNIT_PRICE_MAX);
	}

	bool PlayerBotWantsExchangeMarble(LPCHARACTER ch)
	{
		if (FindPlayerBotBlessingMarbleCell(ch) >= 0)
			return false;
		int pieces = 0;
		for (int wear = 0; wear < WEAR_MAX_NUM; ++wear)
			if (LPITEM worn = ch->GetWear(wear))
				if (worn->GetAttributeCount() >= PLAYERBOT_EXCHANGE_MARBLE_LINES)
					++pieces;
		return pieces >= PLAYERBOT_EXCHANGE_MARBLE_PIECES;
	}

	// The stone it has fewer of, or 0 with both at the keep.
	DWORD GetPlayerBotExchangeStoneWanted(LPCHARACTER ch)
	{
		const int change = (int)ch->CountSpecifyItem(PLAYERBOT_BONUS_CHANGE_VNUM);
		const int add = (int)ch->CountSpecifyItem(PLAYERBOT_BONUS_ADD_VNUM);
		if (std::min(change, add) >= PLAYERBOT_EXCHANGE_STONE_KEEP)
			return 0;
		return add <= change ? PLAYERBOT_BONUS_ADD_VNUM : PLAYERBOT_BONUS_CHANGE_VNUM;
	}

	long long GetPlayerBotExchangeFee()
	{
		return (long long)ScalePlayerBotIwakuraPrice(PLAYERBOT_EXCHANGE_FEE);
	}

	int GetPlayerBotExchangeSurplus(LPCHARACTER ch, DWORD vnum)
	{
		return (int)ch->CountSpecifyItem(vnum) - GetPlayerBotRefineMaterialReserve(ch, vnum) -
				GetPlayerBotBiologistReserve(ch, vnum);
	}

	// One exchange a bot every PLAYERBOT_EXCHANGE_GAP_MS.
	const DWORD PLAYERBOT_EXCHANGE_GAP_MS = 30 * 60 * 1000;
	std::map<DWORD, DWORD> s_mapPlayerBotExchangeNext;

	bool ManagePlayerBotRefinerExchange(LPCHARACTER ch)
	{
		if (!ch || ch->GetLevel() < PLAYERBOT_BONUS_MIN_LEVEL || ch->GetEmptyInventory(1) < 0)
			return false;
		const DWORD now = get_dword_time();
		std::map<DWORD, DWORD>::const_iterator next = s_mapPlayerBotExchangeNext.find(ch->GetPlayerID());
		if (next != s_mapPlayerBotExchangeNext.end() && (int)(now - next->second) < 0)
			return false;
		const long long fee = GetPlayerBotExchangeFee();
		if ((long long)ch->GetGold() - fee < GetPlayerBotReservedGold(ch) + PLAYERBOT_SHOPPING_GOLD_FLOOR)
			return false;
		const bool marble = PlayerBotWantsExchangeMarble(ch);
		const DWORD stone = GetPlayerBotExchangeStoneWanted(ch);
		if (!marble && !stone)
			return false;
		std::set<DWORD> seen;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->GetCell() != cell || item->isLocked() || !seen.insert(item->GetVnum()).second ||
					!IsPlayerBotExchangeFlooded(item))
				continue;
			const DWORD material = item->GetVnum();
			const int surplus = GetPlayerBotExchangeSurplus(ch, material);
			const bool forMarble = marble && surplus >= PLAYERBOT_EXCHANGE_MARBLE_UNITS;
			if (!forMarble && !(stone && surplus >= PLAYERBOT_EXCHANGE_STONE_UNITS))
				continue;
			const int units = forMarble ? PLAYERBOT_EXCHANGE_MARBLE_UNITS : PLAYERBOT_EXCHANGE_STONE_UNITS;
			const DWORD reward = forMarble ? PLAYERBOT_BLESSING_MARBLE_VNUM : stone;
			ch->RemoveSpecifyItem(material, units);
			PlayerBotChangeGold(ch, -fee);
			s_mapPlayerBotExchangeNext[ch->GetPlayerID()] = now + PLAYERBOT_EXCHANGE_GAP_MS;
			const bool success = number(1, 100) <= PLAYERBOT_EXCHANGE_SUCCESS_PERCENT;
			LPITEM made = success ? ch->AutoGiveItem(reward, 1, -1, false) : NULL;
			if (made)
				LogManager::instance().ItemLog(ch, made, "PLAYERBOT_REFINER_EXCHANGE", made->GetName());
			sys_log(0, "PLAYERBOT_BONUS: refiner exchange pid=%u name=%s material=%u units=%d fee=%lld reward=%u ok=%d gold=%lld",
					ch->GetPlayerID(), ch->GetName(), material, units, fee, reward, made ? 1 : 0,
					(long long)ch->GetGold());
			return true;
		}
		return false;
	}

	// A line of such a material a bot short of the stones would take off a
	// stand for the exchange, up to what one exchange wants.
	bool IsPlayerBotExchangeBuyOffer(LPCHARACTER ch, LPITEM offer)
	{
		if (!ch || !offer || ch->GetLevel() < PLAYERBOT_BONUS_MIN_LEVEL || !IsPlayerBotExchangeFlooded(offer))
			return false;
		const bool marble = PlayerBotWantsExchangeMarble(ch);
		if (!marble && !GetPlayerBotExchangeStoneWanted(ch))
			return false;
		const int want = marble ? PLAYERBOT_EXCHANGE_MARBLE_UNITS : PLAYERBOT_EXCHANGE_STONE_UNITS;
		const int surplus = std::max(0, GetPlayerBotExchangeSurplus(ch, offer->GetVnum()));
		return surplus < want && surplus + (int)offer->GetCount() <= want &&
				(long long)ch->GetGold() > GetPlayerBotExchangeFee() * 2;
	}

	bool ManagePlayerBotBonusReroll(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || dwNow < state.dwNextBonusCheckTime)
			return false;
		state.dwNextBonusCheckTime = dwNow + PLAYERBOT_BONUS_INTERVAL;
		ManagePlayerBotDustMarble(ch);
		ManagePlayerBotRefinerExchange(ch);
		// Nothing to spend, nothing to weigh: the pass below scores every line
		// of eight worn pieces, and a bag with no stone and no marble ends here.
		// Any stone at any level: under PLAYERBOT_BONUS_MIN_LEVEL the green
		// ones go on the weapon and the armour and an ordinary one on the
		// necklace, the wrist and the boots Community Patch 1 asks a young bot
		// to bonus first (IsPlayerBotGreenOnlyFor), and a marble is something to
		// spend too - ending the pass here for a young bot left the marble its
		// dust made in the bag for good (B19 of Iwakura's audit of 26 September).
		TPlayerBotBonusBag bag;
		ReadPlayerBotBonusBag(ch, bag);
		if (!HasPlayerBotAnyBonusStone(bag))
			return false;

		std::vector<TPlayerBotBonusTarget> targets;
		CollectPlayerBotBonusTargets(ch, targets);
		if (targets.empty())
			return false;

		// The choice is playerbot_bonus_rules.h's: the green stones first, on
		// the armour and then the weapon (Patch 5, point 9); then the piece
		// being worked while a stone fits it; then a new piece, the first that
		// can take a line before the first that can take a change. The rest of
		// the worn gear takes the kinds of ordinary stone the categories cannot
		// use now (RestOpen, Patch 5, point 5), read once for the pass.
		std::vector<playerbot_bonus_rules::TPiece> pieces;
		pieces.reserve(targets.size());
		int focus = -1;
		for (size_t i = 0; i < targets.size(); ++i)
		{
			pieces.push_back(BuildPlayerBotBonusPiece(ch, targets[i]));
			if (state.dwBonusFocusItem != 0 && targets[i].item->GetID() == state.dwBonusFocusItem)
				focus = (int)i;
		}
		const playerbot_bonus_rules::TBag kinds = GetPlayerBotBonusBagKinds(bag);
		const playerbot_bonus_rules::TLimits limits = GetPlayerBotBonusLimits();
		const unsigned restOpen = playerbot_bonus_rules::RestOpen(&pieces[0], (int)pieces.size(), kinds, limits);
		playerbot_bonus_rules::TStepChoice choice;
		bool greenPick = false;
		const int pick = playerbot_bonus_rules::Pick(&pieces[0], (int)pieces.size(), kinds, limits,
				restOpen, focus, choice, greenPick);
		int stoneCell = -1;
		const EPlayerBotBonusStep step = pick >= 0
				? ResolvePlayerBotBonusChoice(choice, bag, stoneCell) : PLAYERBOT_BONUS_STEP_NONE;
		if (step == PLAYERBOT_BONUS_STEP_NONE)
		{
			PlayerBotLogThrottled("bonus_idle", dwNow,
					"PLAYERBOT_BONUS: idle pid=%u name=%s level=%d early_done=%d rest_open=%u",
					ch->GetPlayerID(), ch->GetName(), (int)ch->GetLevel(),
					IsPlayerBotEarlyBonusDone(ch) ? 1 : 0, restOpen);
			return false;
		}

		const TPlayerBotBonusTarget target = targets[pick];
		// A worn piece comes off for its stones and goes straight back on, so
		// only while the engine would let it back on - the blacksmith's path
		// too, where a buff cast on the walk in shut the window as surely as a
		// blow does. The pass comes back in a moment, not in five minutes.
		if (target.kind == PLAYERBOT_BONUS_TARGET_WORN && IsPlayerBotEquipWindowShut(ch, state))
		{
			state.dwNextBonusCheckTime = dwNow + PLAYERBOT_EQUIPMENT_COMBAT_DELAY;
			return false;
		}
		static const char* const targetKinds[] = { "worn", "held", "goods" };
		static const char* const plainKinds[] = { "category", "rest", "green" };
		const char* const on = targetKinds[target.kind < 3 ? target.kind : 0];
		const char* const reach = plainKinds[target.plain < 3 ? target.plain : 0];
		if (greenPick)
		{
			// The green round leaves the ordinary pass's piece where it was.
			sys_log(0, "PLAYERBOT_BONUS: green pid=%u name=%s vnum=%u slot=%u on=%s lines=%d step=%s level=%d",
					ch->GetPlayerID(), ch->GetName(), target.item->GetVnum(),
					(unsigned int)target.wearCell, on, target.item->GetAttributeCount(),
					GetPlayerBotBonusStepName(step), (int)ch->GetLevel());
		}
		else if (target.item->GetID() != state.dwBonusFocusItem)
		{
			sys_log(0, "PLAYERBOT_BONUS: focus pid=%u name=%s vnum=%u slot=%u on=%s lines=%d step=%s previous=%u plain=%s",
					ch->GetPlayerID(), ch->GetName(), target.item->GetVnum(),
					(unsigned int)target.wearCell, on, target.item->GetAttributeCount(),
					GetPlayerBotBonusStepName(step), state.dwBonusFocusItem, reach);
			state.dwBonusFocusItem = target.item->GetID();
		}

		const int stonesUsed = ApplyPlayerBotBonusSteps(ch, target, step, stoneCell,
				PLAYERBOT_BONUS_STONES_PER_VISIT, restOpen, greenPick);
		// A piece being worked is come back to soon, not in five minutes
		// (Iwakura's Patch 4, point 6: "zmienia bonusy tak dlugo, az wylosuje
		// statystyki o jak najwyzszym Tierze").
		if (stonesUsed > 0)
			state.dwNextBonusCheckTime = dwNow + PLAYERBOT_BONUS_WORKING_INTERVAL;
		return stonesUsed > 0;
	}

	// --- The ItemShop look's bonuses ---------------------------------------
	//
	// The operator's rule (24 September 2026): a bot with the yang gives the
	// costume, the hairstyle and the weapon skin it wears their lines the way
	// a player does at Handlarka Roznosci - 70063 "Transformuj kostium" until
	// the piece has two lines (three when the bot is rich), then 70064
	// "Zaczaruj kostium" until the lines are ones it wants, the same scoring
	// as its armour and weapons. One piece at a time, a stack of each at a
	// time, and never with yang it needs (PLAYERBOT_COSTUME_BONUS_* in
	// playerbot_types.h for the prices and odds behind the numbers).

	BYTE GetPlayerBotCostumeScoreCell(LPITEM item)
	{
		// The slot whose weights a costume's lines are scored with: the body
		// costume's lines are the armour's kind, the weapon skin's the
		// weapon's, the hairstyle's the helmet's.
		if (!item || item->GetType() != ITEM_COSTUME)
			return WEAR_BODY;
		switch (item->GetSubType())
		{
			case COSTUME_HAIR: return WEAR_HEAD;
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
			case COSTUME_WEAPON: return WEAR_WEAPON;
#endif
			default: return WEAR_BODY;
		}
	}

	int ScorePlayerBotCostumeLine(LPCHARACTER ch, LPITEM item, BYTE type, long value)
	{
		// Three costume lines the armour's scoring has no case for, and whose
		// raw numbers (stamina to 400) would pass for the best line there is.
		switch (type)
		{
			case POINT_MAX_STAMINA:   return (int)(value / 40);
			case POINT_ST_REGEN:      return (int)(value * 2);
			case POINT_SKILL_DURATION:return (int)(value * 4);
			case POINT_STEAL_SP:      return (int)(value * 3);
			default:
				return ScorePlayerBotBonusLine(ch, GetPlayerBotCostumeScoreCell(item), type, (short)value);
		}
	}

	int CountPlayerBotGoodCostumeLines(LPCHARACTER ch, LPITEM item)
	{
		int good = 0;
		for (int i = 0; item && i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
			if (item->GetAttributeValue(i) > 0 &&
					ScorePlayerBotCostumeLine(ch, item, item->GetAttributeType(i), item->GetAttributeValue(i)) >=
					PLAYERBOT_COSTUME_GOOD_LINE_SCORE)
				++good;
		return good;
	}

	// Seconds a costume has left: its REAL_TIME limit counts down in socket 0.
	long GetPlayerBotCostumeSecondsLeft(LPITEM item)
	{
		if (!item)
			return 0;
		for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
		{
			const BYTE type = item->GetProto()->aLimits[i].bType;
			if (type == LIMIT_REAL_TIME || type == LIMIT_REAL_TIME_START_FIRST_USE)
			{
				const long end = item->GetSocket(0);
				// Not yet started (first use): the whole of its time is ahead.
				if (end <= 0)
					return item->GetProto()->aLimits[i].lValue;
				return end - (long)get_global_time();
			}
		}
		return LONG_MAX;
	}

	bool IsPlayerBotCostumeBonusReagent(LPITEM item)
	{
		return item && item->GetType() == ITEM_USE &&
				(item->GetSubType() == USE_CHANGE_COSTUME_ATTR || item->GetSubType() == USE_RESET_COSTUME_ATTR);
	}

	int FindPlayerBotCostumeReagentCell(LPCHARACTER ch, DWORD vnum)
	{
		for (WORD cell = 0; ch && cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (item && item->GetVnum() == vnum && item->GetCount() > 0 && !item->isLocked() && !item->IsExchanging())
				return (int)cell;
		}
		return -1;
	}

	bool IsPlayerBotCostumeBonusDone(const TPlayerBotAIState& state, DWORD id)
	{
		return std::find(state.vecCostumeBonusDone.begin(), state.vecCostumeBonusDone.end(), id) !=
				state.vecCostumeBonusDone.end();
	}

	// What the piece needs next: the reset vnum, the change vnum, or 0 when it
	// is finished or not worth the yang.
	DWORD GetPlayerBotCostumeBonusNeed(LPCHARACTER ch, const TPlayerBotAIState& state, LPITEM item)
	{
		if (!ch || !item || item->GetAttributeSetIndex() == -1 || IsPlayerBotCostumeBonusDone(state, item->GetID()) ||
				GetPlayerBotCostumeSecondsLeft(item) < PLAYERBOT_COSTUME_BONUS_MIN_SECONDS_LEFT)
			return 0;
		const int count = item->GetAttributeCount();
		const int good = CountPlayerBotGoodCostumeLines(ch, item);
		// A hairstyle is finished with one good line ("dla fryzury wystarczy 1
		// dobra linia", operator, 26 September 2026): a quarter of its rolls
		// are HP and SP regeneration, which never score as good, and 47 of 250
		// hairstyles came to two good lines at some 72 million yang each.
		const bool hair = item->GetSubType() == COSTUME_HAIR;
		// Finished: two lines worth keeping (all of them on a two-line piece).
		if (hair ? (count >= 1 && good >= 1) : (count >= 2 && good >= 2))
			return 0;
		const int wanted = hair ? 1 : ch->GetGold() >= PLAYERBOT_COSTUME_BONUS_THREE_LINES_GOLD ? 3 : 2;
		return count < wanted ? PLAYERBOT_COSTUME_RESET_VNUM : PLAYERBOT_COSTUME_CHANGE_VNUM;
	}

	// The worn piece the pass works on: the one it was on while that still
	// needs work, else the weapon skin, the costume, the hairstyle.
	LPITEM PickPlayerBotCostumeBonusTarget(LPCHARACTER ch, TPlayerBotAIState& state, DWORD* pNeed)
	{
		*pNeed = 0;
		if (!ch || ch->GetLevel() < PLAYERBOT_COSTUME_BONUS_MIN_LEVEL)
			return NULL;
		static const BYTE s_abCells[] = {
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
			WEAR_COSTUME_WEAPON,
#endif
			WEAR_COSTUME_BODY, WEAR_COSTUME_HAIR };
		LPITEM first = NULL;
		DWORD firstNeed = 0;
		for (size_t i = 0; i < sizeof(s_abCells) / sizeof(s_abCells[0]); ++i)
		{
			LPITEM item = ch->GetWear(s_abCells[i]);
			const DWORD need = GetPlayerBotCostumeBonusNeed(ch, state, item);
			if (!need)
				continue;
			if (item->GetID() == state.dwCostumeBonusFocusItem)
			{
				*pNeed = need;
				return item;
			}
			if (!first)
			{
				first = item;
				firstNeed = need;
			}
		}
		*pNeed = firstNeed;
		return first;
	}

	// At Handlarka: one stack of what the piece needs next, when the bag has
	// none and the purse can spare it. Paid like the potions
	// (ManagePlayerBotMiscMerchant): the proto's price, times the stack.
	bool BuyPlayerBotCostumeReagent(LPCHARACTER ch, TPlayerBotAIState& state)
	{
		DWORD need = 0;
		if (!ch || ch->GetGold() < PLAYERBOT_COSTUME_BONUS_START_GOLD ||
				!PickPlayerBotCostumeBonusTarget(ch, state, &need) || !need ||
				FindPlayerBotCostumeReagentCell(ch, need) >= 0 || ch->GetEmptyInventory(1) < 0)
			return false;
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(need);
		if (!proto || proto->dwGold == 0)
			return false;
		const long long price = (long long)proto->dwGold * PLAYERBOT_COSTUME_REAGENT_STACK;
		if ((long long)ch->GetGold() - price < PLAYERBOT_COSTUME_BONUS_RESERVE_GOLD)
			return false;
		PlayerBotChangeGold(ch, -(int)price);
		ch->AutoGiveItem(need, PLAYERBOT_COSTUME_REAGENT_STACK);
		sys_log(0, "PLAYERBOT_COSTUME_BONUS: bought pid=%u name=%s vnum=%u x%u price=%lld gold_left=%lld",
				ch->GetPlayerID(), ch->GetName(), need, PLAYERBOT_COSTUME_REAGENT_STACK, price,
				(long long)ch->GetGold());
		return true;
	}

	// A few rolls on the piece, off and back on as the engine wants it
	// (UseItemEx refuses a worn costume), each checked the way the engine's
	// own handler checks it (char_item.cpp, USE_RESET/CHANGE_COSTUME_ATTR):
	// a reset must leave one to three lines, a change the same count, or the
	// old lines go back and the reagent is kept. True when a reagent was spent.
	bool ManagePlayerBotCostumeBonus(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || !ch->IsItemLoaded() || ch->IsDead() || dwNow < state.dwNextCostumeBonusTime)
			return false;
		state.dwNextCostumeBonusTime = dwNow + PLAYERBOT_COSTUME_BONUS_STEP_MS;
		DWORD need = 0;
		LPITEM item = PickPlayerBotCostumeBonusTarget(ch, state, &need);
		if (!item || !need)
			return false;
		int cell = FindPlayerBotCostumeReagentCell(ch, need);
		if (cell < 0)
			return false;
		if (item->GetID() != state.dwCostumeBonusFocusItem)
		{
			state.dwCostumeBonusFocusItem = item->GetID();
			state.iCostumeChangesSpent = 0;
		}
		if (ch->GetEmptyInventory(item->GetSize()) < 0 || !ch->UnequipItem(item) || item->IsEquipped())
			return false;

		int spent = 0;
		while (spent < PLAYERBOT_COSTUME_ROLLS_PER_PASS && need && cell >= 0)
		{
			const int countBefore = item->GetAttributeCount();
			TPlayerItemAttribute aBefore[ITEM_ATTRIBUTE_MAX_NUM];
			memcpy(aBefore, item->GetAttributes(), sizeof(aBefore));
			const bool reset = need == PLAYERBOT_COSTUME_RESET_VNUM;
			if (reset)
			{
				item->ClearAttribute();
				item->AlterToMagicItem();
			}
			else
				item->ChangeAttribute();
			const int countAfter = item->GetAttributeCount();
			if (reset ? (countAfter < 1 || countAfter > 3) : countAfter != countBefore)
			{
				item->SetAttributes(aBefore);
				sys_err("PLAYERBOT_COSTUME_BONUS: %s left %d line(s) (had %d) pid=%u vnum=%u - restored",
						reset ? "reset" : "change", countAfter, countBefore, ch->GetPlayerID(), item->GetVnum());
				break;
			}
			ConsumePlayerBotBonusStoneAt(ch, cell);
			++spent;
			if (!reset && ++state.iCostumeChangesSpent >= PLAYERBOT_COSTUME_MAX_CHANGES)
				state.vecCostumeBonusDone.push_back(item->GetID());
			LogManager::instance().ItemLog(ch, item, reset ? "PLAYERBOT_COSTUME_RESET" : "PLAYERBOT_COSTUME_CHANGE",
					item->GetName());
			sys_log(0, "PLAYERBOT_COSTUME_BONUS: %s pid=%u name=%s vnum=%u lines=%d->%d good=%d changes=%d gold=%lld",
					reset ? "reset" : "change", ch->GetPlayerID(), ch->GetName(), item->GetVnum(),
					countBefore, countAfter, CountPlayerBotGoodCostumeLines(ch, item),
					state.iCostumeChangesSpent, (long long)(ch->GetGold() / 1000));
			need = GetPlayerBotCostumeBonusNeed(ch, state, item);
			cell = need ? FindPlayerBotCostumeReagentCell(ch, need) : -1;
		}
		if (!need)
			sys_log(0, "PLAYERBOT_COSTUME_BONUS: finished pid=%u name=%s vnum=%u lines=%d good=%d changes=%d",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum(), item->GetAttributeCount(),
					CountPlayerBotGoodCostumeLines(ch, item), state.iCostumeChangesSpent);
		item->UpdatePacket();
		if (!PlayerBotEquipItem(ch, item))
			sys_err("PLAYERBOT_COSTUME_BONUS: could not re-equip pid=%u name=%s vnum=%u",
					ch->GetPlayerID(), ch->GetName(), item->GetVnum());
		return spent > 0;
	}

	// Iwakura's Patch 4, point 6, "obowiazek natychmiastowego bonowania": a
	// bot with a stone that fits a piece of its own does not wait for the next
	// blacksmith - "ma on bezwzgledny obowiazek od razu przystapic do
	// dzialania". Out of a fight only, like the scroll's field pass: a worn
	// piece comes off for the engine to touch it and goes straight back on,
	// and EquipItem refuses within a second and a half of a blow.
	bool ManagePlayerBotFieldBonus(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead() || state.bCurrentAction == BOT_ACTION_FIGHT || state.bVisitingShop ||
				state.bRecoveringAfterDeath || state.bTacticalRetreat || state.dwTargetVID != 0 ||
				ch->GetMyShop())
			return false;
		// The equipment pass's own wait (IsPlayerBotEquipWindowShut): a piece
		// taken off inside the engine's second and a half after a blow or a
		// cast cannot go back on, and it waited in the bag for the next pass
		// (PLAYERBOT_BONUS: could not re-equip, five a night on m2zip).
		if (IsPlayerBotEquipWindowShut(ch, state))
			return false;
		return ManagePlayerBotBonusReroll(ch, state, dwNow);
	}

	// Iwakura's Community Patch 5, point 9, the gambler's half: "boty z
	// osobowoscia Hazardzisty powinny wykorzystywac posiadane w ekwipunku
	// Zielone Sily i Czary do bonowania przedmiotow, ktore wlasnie ulepszyly
	// (jesli przedmioty te spelniaja wymog poziomu). Pozwoli to znacznie
	// zwiekszyc wartosc takiego sprzetu przed wystawieniem go na sklep
	// offline." For a gambler's session to call on what came off its anvil,
	// and for anything else that has one piece and green stones in mind:
	//
	//   bool ApplyPlayerBotGreenBonusToItem(LPCHARACTER ch, TPlayerBotAIState& state,
	//           LPITEM item, DWORD dwNow)
	//
	// The bot's own green stones on that one piece, worn or in its bag: a
	// Zielona Sila while the piece has fewer than four lines, then a Zielony
	// Czar - which mixes every line, not only the green ones - until the piece
	// is finished for its slot (PlayerBotWantsBonusChange: HasPlayerBotFinishedBonus,
	// and the line score for a weapon without the damage addon, whose finish no
	// change can roll). Up to PLAYERBOT_BONUS_STONES_PER_VISIT a call, the step
	// asked again after each; true when a stone was spent. Nothing on a piece
	// the engine refuses a green stone (a weapon or a body armour of level
	// forty or less, from +4), nothing but a line on a piece a companion's
	// owner put on, nothing while the bot's stall is open or it is transformed,
	// and a worn piece comes off once and goes back on once, only while the
	// engine would let it back on (IsPlayerBotEquipWindowShut). With no green
	// stone, or nothing a stone would do, it returns false having touched
	// nothing, so it is safe to call on every piece and every tick. The
	// ordinary stones and the ordinary pass's piece (dwBonusFocusItem) are
	// left alone.
	bool ApplyPlayerBotGreenBonusToItem(LPCHARACTER ch, TPlayerBotAIState& state, LPITEM item, DWORD dwNow)
	{
		if (!ch || !item || !ch->IsItemLoaded() || ch->IsDead() || ch->GetMyShop() || IsPlayerBotGearFrozen(ch) ||
				item->GetOwner() != ch || !CanPlayerBotTakeGreenBonusStone(item))
			return false;
		TPlayerBotBonusTarget target = { item, (BYTE)WEAR_WEAPON, (BYTE)PLAYERBOT_BONUS_TARGET_GOODS,
				(BYTE)playerbot_bonus_rules::PLAIN_NONE };
		if (item->IsEquipped())
		{
			const int wearCell = (int)item->GetCell() - (int)INVENTORY_MAX_NUM;
			if (wearCell < 0 || wearCell >= WEAR_MAX_NUM || !IsPlayerBotWornItemSound(ch, item, wearCell) ||
					IS_SET(item->GetFlag(), ITEM_FLAG_IRREMOVABLE) || IsPlayerBotEquipWindowShut(ch, state))
				return false;
			target.wearCell = (BYTE)wearCell;
			target.kind = PLAYERBOT_BONUS_TARGET_WORN;
		}
		else
		{
			if (item->GetWindow() != INVENTORY || item->GetCell() >= PLAYERBOT_BAG_CELLS ||
					ch->GetInventoryItem(item->GetCell()) != item)
				return false;
			// Finished by the measure of the slot it is for.
			target.wearCell = item->GetType() == ITEM_WEAPON ? (BYTE)WEAR_WEAPON : (BYTE)WEAR_BODY;
		}
		int stoneCell = -1;
		const EPlayerBotBonusStep step = GetPlayerBotBonusStep(ch, target, stoneCell, 0, true);
		if (step == PLAYERBOT_BONUS_STEP_NONE)
			return false;
		const int linesBefore = item->GetAttributeCount();
		const int used = ApplyPlayerBotBonusSteps(ch, target, step, stoneCell,
				PLAYERBOT_BONUS_STONES_PER_VISIT, 0, true);
		// Every stone has its own line above; this says whose call it was.
		PlayerBotLogThrottled("bonus_green_item", dwNow,
				"PLAYERBOT_BONUS: green on a named piece pid=%u name=%s vnum=%u id=%u worn=%d stones=%d lines=%d->%d",
				ch->GetPlayerID(), ch->GetName(), item->GetVnum(), item->GetID(),
				target.kind == PLAYERBOT_BONUS_TARGET_WORN ? 1 : 0, used, linesBefore, item->GetAttributeCount());
		return used > 0;
	}

	// Patch 5, point 9: "Czesto przetrzymuja je w ekwipunku az do poznych faz
	// gry i wysokich poziomow postaci." A bot past the green band - over forty
	// and wearing neither a weapon nor an armour a green stone fits
	// (playerbot_bonus_rules::PastGreenBand) - keeps none of them back from a
	// counter. What goes up is what the engine lets a counter carry: 71151 and
	// 71152. The level-20 chest's 76023 and 76024, the only green stones these
	// worlds hand out, carry ITEM_ANTIFLAG_MYSHOP and ITEM_ANTIFLAG_GIVE, so no
	// counter or trade takes them, and the merchant would pay nothing for them
	// (shop_buy_price 0) - they stay for a piece of forty or less that passes
	// through the bag: a level-30 weapon kept for sale
	// (PLAYERBOT_BONUS_TARGET_GOODS) or a gambler's (ApplyPlayerBotGreenBonusToItem).
	bool PlayerBotKeepsGreenBonusStones(LPCHARACTER ch)
	{
		if (!ch)
			return true;
		return !playerbot_bonus_rules::PastGreenBand((int)ch->GetLevel(), PLAYERBOT_GREEN_BONUS_MAX_LEVEL,
				CanPlayerBotSpendGreenBonusStoneOn(ch->GetWear(WEAR_WEAPON)),
				CanPlayerBotSpendGreenBonusStoneOn(ch->GetWear(WEAR_BODY)));
	}

	// Whether a piece of this bot's own could take a stone of this one's kind
	// now: the pass's own question (playerbot_bonus_rules::StepFor over the
	// pieces CollectPlayerBotBonusTargets names, the young bot's rule, the
	// categories and the rest of the gear included), asked with a bag that
	// holds this kind alone. "Zeby boty faktycznie to uzywaly zamiast wystawiac
	// za grosze na rynek" (blipu, 28 September): what the bot would spend
	// itself is not goods.
	bool PlayerBotCanSpendBonusStoneKind(LPCHARACTER ch, LPITEM stone)
	{
		if (!ch || !stone || stone->GetType() != ITEM_USE)
			return false;
		playerbot_bonus_rules::TBag kind = { false, false, false, false, false };
		const bool green = IsPlayerBotGreenBonusStone(stone->GetVnum());
		switch (stone->GetSubType())
		{
			case USE_ADD_ATTRIBUTE:
				if (green)
					kind.greenAdd = true;
				else
					kind.plainAdd = true;
				break;
			case USE_CHANGE_ATTRIBUTE:
				if (green)
					kind.greenChange = true;
				else
					kind.plainChange = true;
				break;
			case USE_ADD_ATTRIBUTE2:
				kind.marble = true;
				break;
			default:
				return false;
		}
		std::vector<TPlayerBotBonusTarget> targets;
		CollectPlayerBotBonusTargets(ch, targets);
		if (targets.empty())
			return false;
		std::vector<playerbot_bonus_rules::TPiece> pieces;
		pieces.reserve(targets.size());
		for (size_t i = 0; i < targets.size(); ++i)
			pieces.push_back(BuildPlayerBotBonusPiece(ch, targets[i]));
		const playerbot_bonus_rules::TLimits limits = GetPlayerBotBonusLimits();
		const unsigned restOpen = playerbot_bonus_rules::RestOpen(&pieces[0], (int)pieces.size(), kind, limits);
		for (size_t i = 0; i < pieces.size(); ++i)
			if (playerbot_bonus_rules::StepFor(pieces[i], kind, limits, restOpen, false).step !=
					playerbot_bonus_rules::STEP_NONE)
				return true;
		return false;
	}

	// What a bag keeps of a bonus stone kind back from a counter: the
	// counter's keep and the cut asks it (playerbot_economy.h). Every one
	// while a piece of the bot's own could take one now, none of a green one
	// past its band, PLAYERBOT_BONUS_STONE_KEEP otherwise
	// (playerbot_moonlight_rules::BonusGoodsKeep).
	int GetPlayerBotBonusStoneKeep(LPCHARACTER ch, LPITEM item)
	{
		const bool greenPastBand = item && IsPlayerBotGreenBonusStone(item->GetVnum()) &&
				!PlayerBotKeepsGreenBonusStones(ch);
		return playerbot_moonlight_rules::BonusGoodsKeep(PlayerBotCanSpendBonusStoneKind(ch, item),
				greenPastBand, PLAYERBOT_BONUS_STONE_KEEP);
	}
}

#endif
