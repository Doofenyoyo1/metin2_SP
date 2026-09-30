// The words over a stall: Iwakura's names for a counter (playerbot_shop_names.h,
// rendered from his list of 14 September) chosen by his rules
// (playerbot_shop_name_rules.h). This file is the engine's half - what each
// line on a counter is, in the terms those rules read - and it comes after
// playerbot_town.h because what heads a +7..+9 piece or a soul stone is its
// price.
//
// Player-visible and, on purpose, not ASCII like the other bot strings: the
// names are his, diacritics and all, in CP1250 - the engine's encoding for an
// item name and a shop name alike (player.ikashop_offlineshop.name is
// cp1250_polish_ci). The one piece of wording that is not his is the short
// form of a bonus line below; his rule gives two examples of it.
namespace
{
	// The item's own name with its "+N" taken off - "Miecz Pelni Ksiezyca+9" is
	// "Miecz Pelni Ksiezyca" - because his rule puts the plus after a space.
	std::string GetPlayerBotSignBaseName(LPITEM item)
	{
		const TItemTable* proto = item ? item->GetProto() : NULL;
		if (!proto)
			return std::string();
		std::string name = proto->szLocaleName;
		const std::string::size_type plus = name.rfind('+');
		if (plus != std::string::npos && plus + 1 < name.size() &&
				name.find_first_not_of("0123456789", plus + 1) == std::string::npos)
			name.erase(plus);
		while (!name.empty() && name[name.size() - 1] == ' ')
			name.erase(name.size() - 1);
		return name;
	}

	// A bonus line in as few letters as a sign has room for. His rule gives
	// "1500 HP" and "10 do PZ" (x% of the damage added to HP); the rest follow
	// the same market habit, the number and then what it is. Only lines that
	// can be worth x1.5 at their top roll are here - no other line is named.
	// The last column is the same line for a title in English
	// (TranslateSignName): ours, the short forms English players trade in.
	struct TPlayerBotSignBonusLabel { BYTE bApply; const char* szLabel; bool bValue; const char* szLabelEn; };
	const TPlayerBotSignBonusLabel PLAYERBOT_SIGN_BONUS_LABELS[] = {
		{ APPLY_MAX_HP, "HP", true, "HP" },
		{ APPLY_STEAL_HP, "do P\xAF", true, "HP absorb" },
		{ APPLY_ATTBONUS_HUMAN, "PL", true, "HH" },
		{ APPLY_ATTBONUS_ANIMAL, "zwierz", true, "animals" },
		{ APPLY_ATTBONUS_DEVIL, "diably", true, "devils" },
		{ APPLY_ATTBONUS_UNDEAD, "nieumarli", true, "undead" },
		{ APPLY_ATTBONUS_ORC, "orki", true, "orcs" },
		{ APPLY_ATTBONUS_MILGYO, "mistyki", true, "esoterics" },
		{ APPLY_ATT_GRADE_BONUS, "WA", true, "AV" },
		{ APPLY_ATT_SPEED, "SA", true, "AS" },
		{ APPLY_CAST_SPEED, "SZ", true, "CS" },
		{ APPLY_MOV_SPEED, "SR", true, "MS" },
		{ APPLY_BLOCK, "blok", true, "block" },
		{ APPLY_REFLECT_MELEE, "odbicie", true, "reflect" },
		{ APPLY_STR, "Sila", true, "STR" },
		{ APPLY_INT, "Int", true, "INT" },
		{ APPLY_DEX, "Zr", true, "DEX" },
		{ APPLY_CON, "Wit", true, "VIT" },
		{ APPLY_CRITICAL_PCT, "kryt", true, "crit" },
		{ APPLY_PENETRATE_PCT, "przesz", true, "pierce" },
		{ APPLY_STUN_PCT, "omdl", true, "stun" },
		{ APPLY_POISON_PCT, "otruc", true, "poison" },
		{ APPLY_DODGE, "unik", true, "dodge" },
		{ APPLY_GOLD_DOUBLE_BONUS, "yang", true, "yang" },
		{ APPLY_MALL_EXPBONUS, "EXP", true, "EXP" },
		{ APPLY_IMMUNE_STUN, "NNO", false, "stun immune" },
		{ APPLY_RESIST_BOW, "odp strzaly", true, "arrow res" },
		{ APPLY_RESIST_DAGGER, "odp sztylety", true, "dagger res" },
		{ APPLY_RESIST_SWORD, "odp miecze", true, "sword res" },
		{ APPLY_RESIST_TWOHAND, "odp 2r", true, "2H res" },
		{ APPLY_RESIST_BELL, "odp dzwony", true, "bell res" },
		{ APPLY_RESIST_FAN, "odp wachlarze", true, "fan res" },
		{ APPLY_RESIST_MAGIC, "odp magia", true, "magic res" },
		{ APPLY_NORMAL_HIT_DAMAGE_BONUS, "sr", true, "avg dmg" },
		{ APPLY_SKILL_DAMAGE_BONUS, "UM", true, "skill dmg" },
	};

	// A bonus line's label in English by its Polish one, or "".
	std::string GetPlayerBotSignBonusLabelEn(const std::string& label)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_SIGN_BONUS_LABELS) / sizeof(PLAYERBOT_SIGN_BONUS_LABELS[0]); ++i)
			if (label == PLAYERBOT_SIGN_BONUS_LABELS[i].szLabel)
				return PLAYERBOT_SIGN_BONUS_LABELS[i].szLabelEn;
		return std::string();
	}

	const TPlayerBotSignBonusLabel* FindPlayerBotSignBonusLabel(BYTE bApply)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_SIGN_BONUS_LABELS) / sizeof(PLAYERBOT_SIGN_BONUS_LABELS[0]); ++i)
			if (PLAYERBOT_SIGN_BONUS_LABELS[i].bApply == bApply)
				return &PLAYERBOT_SIGN_BONUS_LABELS[i];
		return NULL;
	}

	// The one line worth naming over a +7..+9 piece. His rule: "maksymalny
	// bonus podbijajacy cene o min. 1.5x" - a line at its top roll whose
	// multiplier is at least SIGN_BONUS_MIN_PCT. A weapon's two damage lines are
	// priced by tier on his sheet, not by a top roll, so a tier worth that much
	// counts. Asked of the same rows GetPlayerBotBonusPricePercent compounds,
	// so the sign cannot praise a line the price ignored; the dearest wins.
	std::string GetPlayerBotSignBonus(LPITEM item)
	{
		const BYTE slot = GetPlayerBotPriceSlot(item);
		if (!item || slot == 0)
			return std::string();
		const int level = item->GetLevelLimit();
		int bestPct = 0;
		long bestValue = 0;
		const TPlayerBotSignBonusLabel* best = NULL;
		const int count = item->GetAttributeCount();
		for (int i = 0; i < count && i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			const BYTE type = item->GetAttributeType(i);
			const long value = item->GetAttributeValue(i);
			const TPlayerBotSignBonusLabel* label = FindPlayerBotSignBonusLabel(type);
			if (type == 0 || value <= 0 || !label)
				continue;
			// The price's own question (GetPlayerBotBonusLinePct): a damage line
			// by its tier, any other line only at its top.
			bool atTop = false;
			int pct = GetPlayerBotBonusLinePct(item, slot, level, type, value, &atTop);
			const bool damageLine = slot == PRICE_SLOT_WEAPON &&
					(type == APPLY_NORMAL_HIT_DAMAGE_BONUS || type == APPLY_SKILL_DAMAGE_BONUS);
			if (!damageLine && !atTop)
				pct = 0;
			if (pct >= playerbot_shop_names::SIGN_BONUS_MIN_PCT && pct > bestPct)
			{
				bestPct = pct;
				bestValue = value;
				best = label;
			}
		}
		if (!best)
			return std::string();
		char text[32];
		if (best->bValue)
			snprintf(text, sizeof(text), "%ld %s", bestValue, best->szLabel);
		else
			snprintf(text, sizeof(text), "%s", best->szLabel);
		return text;
	}

	// One counter line in the terms of his rules.
	playerbot_shop_names::TSignLine DescribePlayerBotSignLine(LPITEM item)
	{
		using namespace playerbot_shop_names;
		TSignLine line;
		if (!item)
			return line;
		const DWORD vnum = item->GetVnum();
		const BYTE type = item->GetType();
		line.dwVnum = vnum;
		if (type == ITEM_WEAPON || type == ITEM_ARMOR)
		{
			line.iPlus = item->GetRefineLevel();
			line.bLine = GetSignGearLine(line.iPlus);
			line.iLevel = item->GetLevelLimit();
			if (type == ITEM_WEAPON)
				line.bGearSlot = SIGN_GEAR_WEAPON;
			else if (item->GetSubType() == ARMOR_BODY)
				line.bGearSlot = SIGN_GEAR_BODY;
			else if (item->GetSubType() == ARMOR_SHIELD)
				line.bGearSlot = SIGN_GEAR_SHIELD;
			line.strName = GetPlayerBotSignBaseName(item);
			if (line.bLine == SIGN_LINE_TOP_GEAR)
			{
				line.qwValue = GetPlayerBotShopAskingPrice(item);
				line.strBonus = GetPlayerBotSignBonus(item);
				line.bStones = GetPlayerBotSocketStonePercent(item) >= SIGN_STONES_MIN_PCT;
			}
			return line;
		}
		if (type == ITEM_SKILLBOOK)
		{
			line.bLine = SIGN_LINE_BOOK;
			line.dwSkill = GetPlayerBotSkillBookSkillVnum(item);
			return line;
		}
		// Asked before the materials: a shell and the pearls are refine materials
		// too, and a fish counter is not a smith's supplier.
		if (type == ITEM_FISH || vnum == PLAYERBOT_SHELLFISH_VNUM ||
				(vnum >= PLAYERBOT_PEARL_FIRST_VNUM && vnum <= PLAYERBOT_PEARL_LAST_VNUM) ||
				(vnum >= PLAYERBOT_GRILLED_FISH_FIRST_VNUM && vnum <= PLAYERBOT_GRILLED_FISH_LAST_VNUM) ||
				IsPlayerBotHairDye(vnum))
		{
			line.bLine = SIGN_LINE_FISH;
			return line;
		}
		if (type == ITEM_METIN)
		{
			line.bLine = SIGN_LINE_STONE;
			line.strName = GetPlayerBotSignBaseName(item);
			line.iPlus = GetPlayerBotSoulStoneGrade(vnum);
			line.qwValue = GetPlayerBotSoulStoneAskingBase(vnum);
			return line;
		}
		// His [INNE]: ores, horse medals and the Blessing Scroll by name - 25040,
		// not the Magic Stone that shares its name in this locale.
		const bool ore = IsPlayerBotRawOre(vnum) || IsPlayerBotSmeltedOre(vnum);
		if (ore || vnum == PLAYERBOT_HORSE_MEDAL_VNUM || vnum == PLAYERBOT_BLESSING_SCROLL_VNUM)
		{
			line.bLine = SIGN_LINE_OTHER;
			line.bOre = ore;
			return line;
		}
		if (IsPlayerBotTradeableMaterial(item))
		{
			line.bLine = SIGN_LINE_MATERIAL;
			const TItemTable* proto = item->GetProto();
			line.strName = proto ? proto->szLocaleName : "";
		}
		return line;
	}

	// A name for a counter of these goods, the classic stall's lines or an
	// offline shop's. False only for an empty counter; how says which of his
	// rules chose it, for the log.
	bool ChoosePlayerBotShopName(LPCHARACTER ch, const std::vector<LPITEM>& goods,
			char* out, size_t outSize, const char** how)
	{
		if (!out || outSize == 0)
			return false;
		playerbot_shop_names::TSignCounter counter;
		counter.bFirstVillage = ch && IsPlayerBotM1Map(ch->GetMapIndex());
		for (size_t i = 0; i < goods.size(); ++i)
			if (goods[i])
				counter.lines.push_back(DescribePlayerBotSignLine(goods[i]));
		auto roll = [](int lo, int hi) { return number(lo, hi); };
		std::string name;
		uint8_t reason = playerbot_shop_names::SIGN_HOW_NONE;
		if (!playerbot_shop_names::ChooseSignName(counter, roll, name, reason))
			return false;
		strlcpy(out, name.c_str(), outSize);
		if (how)
			*how = playerbot_shop_names::GetSignHowName(reason);
		return true;
	}

	// A bot's shop title in English, for a person whose client reads English
	// (CPlayerBotManager::GetShopNameFor): TranslateSignName over the official
	// English item names and the labels above. False where the Polish title has
	// to stand. Remembered by the title - a town's two hundred stands share a few
	// hundred titles, and each is asked again for every viewer who walks up to
	// it - but not before the names are there to be asked.
	const size_t PLAYERBOT_SHOP_TITLE_EN_CACHE_MAX = 4096;

	bool GetPlayerBotShopNameEn(const char* sign, std::string& out)
	{
		static std::unordered_map<std::string, std::string> s_mapTitles;
		out.clear();
		if (!sign || !*sign)
			return false;
		std::unordered_map<std::string, std::string>::const_iterator it = s_mapTitles.find(sign);
		if (it != s_mapTitles.end())
		{
			out = it->second;
			return !out.empty();
		}
		const bool ok = playerbot_shop_names::TranslateSignName(std::string(sign),
				[](const std::string& polish) { return FindPlayerBotItemNameEnByPolish(polish); },
				[](const std::string& label) { return GetPlayerBotSignBonusLabelEn(label); }, out);
		if (!ok)
			out.clear();
		if (s_PlayerBotEnglishNames.bLoaded)
		{
			if (s_mapTitles.size() >= PLAYERBOT_SHOP_TITLE_EN_CACHE_MAX)
				s_mapTitles.clear();
			s_mapTitles[sign] = out;
		}
		return ok;
	}
}
