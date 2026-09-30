// How a bot names its counter: Iwakura's rules (the head of
// data/iwakura_nazwy_sklepow.txt) over his names in playerbot_shop_names.h.
// Pure, like the other *_rules.h and *_policy.h headers - no engine types - so
// tests/playerbot_shop_name_rules_test.cpp can pin every branch;
// playerbot_shop_signs.h describes a real counter in these terms and brings
// the engine's dice.
//
// In the order he wrote them:
//  - a third of the time, whatever is on the counter, a neutral name;
//  - a weapon or armour at +7 to +9 names the counter after itself: the item
//    and its plus, the one bonus line worth x1.5 at its top roll, "KD" when
//    its soul stones are worth x1.4, and TANIO or OKAZJA - dropped from the
//    end until the whole fits a sign, down to the item and its plus;
//  - otherwise the kind most of its lines are decides: soul stones name the
//    best stone and its grade, one upgrade material filling half of the
//    material lines may name the counter itself, and every other kind draws
//    from its own list - where a name that mentions goods is drawn only over
//    a counter that carries them;
//  - a counter with nothing from his lists takes a neutral name.
#ifndef PLAYERBOT_SHOP_NAME_RULES_H
#define PLAYERBOT_SHOP_NAME_RULES_H
#include "playerbot_shop_names.h"
#include "playerbot_name_rules.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace playerbot_shop_names
{
	// What one counter line is, as far as the name goes.
	enum ESignLine : uint8_t
	{
		SIGN_LINE_NONE,       // nothing his lists name: a marble, a chest, a potion
		SIGN_LINE_BOOK,
		SIGN_LINE_FISH,       // fish, shells, pearls, hair dye
		SIGN_LINE_STONE,      // a soul stone
		SIGN_LINE_OTHER,      // his [INNE]: ores, horse medals, blessing scrolls
		SIGN_LINE_MATERIAL,   // an upgrade material
		SIGN_LINE_SCRAP,      // a weapon or armour at +0 to +3
		SIGN_LINE_GEAR,       // at +4 to +6
		SIGN_LINE_TOP_GEAR,   // at +7 to +9
		SIGN_LINE_COUNT
	};

	enum ESignGearSlot : uint8_t { SIGN_GEAR_OTHER, SIGN_GEAR_WEAPON, SIGN_GEAR_BODY, SIGN_GEAR_SHIELD };

	struct TSignLine
	{
		uint8_t bLine = SIGN_LINE_NONE;
		uint32_t dwVnum = 0;
		uint32_t dwSkill = 0;            // a book's skill
		uint8_t bGearSlot = SIGN_GEAR_OTHER;
		int iLevel = 0;                  // a weapon or armour's level limit
		bool bOre = false;
		uint64_t qwValue = 0;            // what one unit asks: which top piece or stone leads
		std::string strName;             // gear and stones without their "+N"; the rest whole
		int iPlus = 0;                   // gear's refine, a stone's grade
		std::string strBonus;            // top gear: the one line worth naming, "1500 HP"
		bool bStones = false;            // top gear: its soul stones are worth "KD"
	};

	struct TSignCounter
	{
		std::vector<TSignLine> lines;
		bool bFirstVillage = false;
	};

	// Why a counter got its name, for the log.
	enum ESignHow : uint8_t
	{
		SIGN_HOW_NONE, SIGN_HOW_NEUTRAL_ROLL, SIGN_HOW_TOP_GEAR, SIGN_HOW_STONE,
		SIGN_HOW_MATERIAL, SIGN_HOW_LIST, SIGN_HOW_NEUTRAL
	};

	inline const char* GetSignHowName(uint8_t how)
	{
		switch (how)
		{
			case SIGN_HOW_NEUTRAL_ROLL: return "neutral_roll";
			case SIGN_HOW_TOP_GEAR: return "top_gear";
			case SIGN_HOW_STONE: return "soul_stone";
			case SIGN_HOW_MATERIAL: return "material";
			case SIGN_HOW_LIST: return "list";
			case SIGN_HOW_NEUTRAL: return "neutral";
			default: return "none";
		}
	}

	// A skill's class as the bit SIGN_NEED_BOOK_CLASSES reads, from the skill
	// numbers of each job's two groups.
	inline uint32_t GetSignSkillClassBit(uint32_t skill)
	{
		if ((skill >= 1 && skill <= 5) || (skill >= 16 && skill <= 20))
			return 1;
		if ((skill >= 31 && skill <= 35) || (skill >= 46 && skill <= 50))
			return 2;
		if ((skill >= 61 && skill <= 66) || (skill >= 76 && skill <= 81))
			return 4;
		if ((skill >= 91 && skill <= 96) || (skill >= 106 && skill <= 111))
			return 8;
		return 0;
	}

	// Scrap, gear or top gear, by the plus - his two thresholds.
	inline uint8_t GetSignGearLine(int plus)
	{
		if (plus >= SIGN_TOP_GEAR_MIN_PLUS)
			return SIGN_LINE_TOP_GEAR;
		return plus <= SIGN_SCRAP_MAX_PLUS ? SIGN_LINE_SCRAP : SIGN_LINE_GEAR;
	}

	inline bool IsSignGearLine(uint8_t line)
	{
		return line == SIGN_LINE_SCRAP || line == SIGN_LINE_GEAR || line == SIGN_LINE_TOP_GEAR;
	}

	inline bool HasSignGoods(const TSignCounter& counter, const TSignGoods& goods)
	{
		for (const TSignLine& line : counter.lines)
			for (uint32_t vnum : goods.adwVnum)
				if (vnum != 0 && line.dwVnum == vnum)
					return true;
		return false;
	}

	inline bool IsSignNeedMet(const TSignCounter& counter, const TSignName& name)
	{
		const uint32_t param = name.aGoods[0].adwVnum[0];
		switch (name.bNeed)
		{
			case SIGN_NEED_NONE:
				return true;
			case SIGN_NEED_ALL_GOODS:
				if (name.bGoods == 0)
					return false;
				for (uint8_t g = 0; g < name.bGoods && g < 3; ++g)
					if (!HasSignGoods(counter, name.aGoods[g]))
						return false;
				return true;
			case SIGN_NEED_ANY_GOODS:
				for (uint8_t g = 0; g < name.bGoods && g < 3; ++g)
					if (HasSignGoods(counter, name.aGoods[g]))
						return true;
				return false;
			case SIGN_NEED_ORES:
				for (const TSignLine& line : counter.lines)
					if (line.bOre)
						return true;
				return false;
			case SIGN_NEED_BOOK_SKILLS:
				for (const TSignLine& line : counter.lines)
					if (line.bLine == SIGN_LINE_BOOK)
						for (uint32_t skill : name.aGoods[0].adwVnum)
							if (skill != 0 && line.dwSkill == skill)
								return true;
				return false;
			case SIGN_NEED_BOOK_CLASSES:
			{
				uint32_t classes = 0;
				for (const TSignLine& line : counter.lines)
					if (line.bLine == SIGN_LINE_BOOK)
						classes |= GetSignSkillClassBit(line.dwSkill);
				return param != 0 && (classes & param) == param;
			}
			case SIGN_NEED_TOP_GEAR:
				for (const TSignLine& line : counter.lines)
					if (line.bLine == SIGN_LINE_TOP_GEAR)
						return true;
				return false;
			case SIGN_NEED_LOW_GEAR:
				for (const TSignLine& line : counter.lines)
					if (IsSignGearLine(line.bLine) && line.iLevel < (int)param)
						return true;
				return false;
			case SIGN_NEED_HIGH_GEAR:
				for (const TSignLine& line : counter.lines)
					if (IsSignGearLine(line.bLine) && line.iLevel >= (int)param)
						return true;
				return false;
			case SIGN_NEED_BODY_ARMOUR:
				for (const TSignLine& line : counter.lines)
					if (IsSignGearLine(line.bLine) && line.bGearSlot == SIGN_GEAR_BODY)
						return true;
				return false;
			case SIGN_NEED_GEAR_KINDS:
			{
				bool weapon = false, body = false, shield = false;
				for (const TSignLine& line : counter.lines)
				{
					if (!IsSignGearLine(line.bLine))
						continue;
					weapon = weapon || line.bGearSlot == SIGN_GEAR_WEAPON;
					body = body || line.bGearSlot == SIGN_GEAR_BODY;
					shield = shield || line.bGearSlot == SIGN_GEAR_SHIELD;
				}
				return weapon && body && shield;
			}
			case SIGN_NEED_FIRST_VILLAGE:
				return counter.bFirstVillage;
			default:
				return false;
		}
	}

	// The names of one list this counter qualifies for. onlyNeed narrows it to
	// the names with that one need; SIGN_NEED_COUNT takes them all.
	inline void CollectSignListNames(const TSignCounter& counter, uint8_t kind, uint8_t onlyNeed,
			std::vector<const TSignName*>& out)
	{
		out.clear();
		for (size_t i = 0; i < SIGN_NAME_COUNT; ++i)
		{
			const TSignName& name = SIGN_NAMES[i];
			if (name.bKind != kind || (onlyNeed != SIGN_NEED_COUNT && name.bNeed != onlyNeed))
				continue;
			if (IsSignNeedMet(counter, name))
				out.push_back(&name);
		}
	}

	template <typename TRoll>
	inline bool PickSignListName(const TSignCounter& counter, uint8_t kind, uint8_t onlyNeed,
			TRoll& roll, std::string& out)
	{
		std::vector<const TSignName*> fits;
		CollectSignListNames(counter, kind, onlyNeed, fits);
		if (fits.empty())
			return false;
		out = fits[(size_t)roll(0, (int)fits.size() - 1)]->szName;
		return true;
	}

	// Pieces joined by a space, the last one dropped for as long as the whole
	// is too long for a sign. The first always stays - "w ostatecznosci sama
	// nazwa itemu z plusem" - so this fails only when it alone does not fit.
	inline bool FitSignPieces(std::vector<std::string> pieces, std::string& out)
	{
		while (!pieces.empty())
		{
			std::string joined;
			for (const std::string& piece : pieces)
			{
				if (piece.empty())
					continue;
				if (!joined.empty())
					joined += ' ';
				joined += piece;
			}
			if (!joined.empty() && joined.size() <= SIGN_MAX_LEN)
			{
				out = joined;
				return true;
			}
			if (pieces.size() == 1)
				return false;
			pieces.pop_back();
		}
		return false;
	}

	// roll(lo, hi) is inclusive at both ends, the engine's number().
	template <typename TRoll>
	inline bool ChooseSignName(const TSignCounter& counter, TRoll& roll, std::string& out, uint8_t& how)
	{
		out.clear();
		how = SIGN_HOW_NONE;
		if (counter.lines.empty())
			return false;

		if (roll(1, 100) <= SIGN_NEUTRAL_OVERRIDE_PERCENT &&
				PickSignListName(counter, SIGN_KIND_NEUTRAL, SIGN_NEED_COUNT, roll, out))
		{
			how = SIGN_HOW_NEUTRAL_ROLL;
			return true;
		}

		int counts[SIGN_LINE_COUNT] = { 0 };
		const TSignLine* top = nullptr;
		const TSignLine* stone = nullptr;
		for (const TSignLine& line : counter.lines)
		{
			if (line.bLine >= SIGN_LINE_COUNT)
				continue;
			++counts[line.bLine];
			if (line.bLine == SIGN_LINE_TOP_GEAR && (!top || line.qwValue > top->qwValue ||
					(line.qwValue == top->qwValue && line.iPlus > top->iPlus)))
				top = &line;
			if (line.bLine == SIGN_LINE_STONE && (!stone || line.qwValue > stone->qwValue ||
					(line.qwValue == stone->qwValue && line.iPlus > stone->iPlus)))
				stone = &line;
		}

		if (top)
		{
			// More than one such piece: now and then the name his list has for
			// exactly that counter, "EQ +7/+8/+9".
			if (counts[SIGN_LINE_TOP_GEAR] >= 2 && roll(1, 3) == 1 &&
					PickSignListName(counter, SIGN_KIND_GEAR, SIGN_NEED_TOP_GEAR, roll, out))
			{
				how = SIGN_HOW_LIST;
				return true;
			}
			const std::string suffix = SIGN_GEAR_SUFFIXES[roll(0, 1)];
			if (FitSignPieces({ top->strName + " +" + std::to_string(top->iPlus), top->strBonus,
					top->bStones ? "KD" : "", suffix }, out))
			{
				how = SIGN_HOW_TOP_GEAR;
				return true;
			}
		}

		// The kind most lines are; a tie goes to what a buyer crosses a market for.
		static const uint8_t s_abOrder[] = {
			SIGN_LINE_STONE, SIGN_LINE_BOOK, SIGN_LINE_MATERIAL, SIGN_LINE_OTHER,
			SIGN_LINE_FISH, SIGN_LINE_GEAR, SIGN_LINE_SCRAP };
		uint8_t lead = SIGN_LINE_NONE;
		int most = 0;
		for (uint8_t line : s_abOrder)
			if (counts[line] > most)
			{
				most = counts[line];
				lead = line;
			}

		uint8_t kind = SIGN_KIND_COUNT;
		switch (lead)
		{
			case SIGN_LINE_STONE:
				if (stone && FitSignPieces({ stone->strName + " +" + std::to_string(stone->iPlus),
						SIGN_GOODS_SUFFIXES[roll(0, 5)] }, out))
				{
					how = SIGN_HOW_STONE;
					return true;
				}
				break;
			case SIGN_LINE_MATERIAL:
			{
				// "Znaczna ilosc jednego przedmiotu": one material on at least half
				// of the material lines may name the counter itself.
				const TSignLine* material = nullptr;
				int materialLines = 0;
				for (const TSignLine& line : counter.lines)
				{
					if (line.bLine != SIGN_LINE_MATERIAL)
						continue;
					int lines = 0;
					for (const TSignLine& other : counter.lines)
						if (other.bLine == SIGN_LINE_MATERIAL && other.dwVnum == line.dwVnum)
							++lines;
					if (lines > materialLines)
					{
						material = &line;
						materialLines = lines;
					}
				}
				if (material && materialLines * 2 >= counts[SIGN_LINE_MATERIAL] && roll(0, 1) == 0)
				{
					const int suffix = roll(0, 6);
					if (FitSignPieces({ material->strName, suffix > 0 ? SIGN_GOODS_SUFFIXES[suffix - 1] : "" }, out))
					{
						how = SIGN_HOW_MATERIAL;
						return true;
					}
				}
				kind = SIGN_KIND_MATERIALS;
				break;
			}
			case SIGN_LINE_BOOK: kind = SIGN_KIND_BOOKS; break;
			case SIGN_LINE_OTHER: kind = SIGN_KIND_OTHER; break;
			case SIGN_LINE_FISH: kind = SIGN_KIND_FISH; break;
			case SIGN_LINE_GEAR: kind = SIGN_KIND_GEAR; break;
			case SIGN_LINE_SCRAP: kind = SIGN_KIND_SCRAP; break;
			default: break;
		}
		if (kind != SIGN_KIND_COUNT && PickSignListName(counter, kind, SIGN_NEED_COUNT, roll, out))
		{
			how = SIGN_HOW_LIST;
			return true;
		}

		if (PickSignListName(counter, SIGN_KIND_NEUTRAL, SIGN_NEED_COUNT, roll, out))
		{
			how = SIGN_HOW_NEUTRAL;
			return true;
		}
		return false;
	}

	// ------------------------------------------------------------ in English
	//
	// A bot's counter is titled for each viewer in the viewer's language
	// (playerbotify apply_person_language_names, CPlayerBotManager::
	// GetShopNameFor): the title stays what the engine and the database keep,
	// Polish, and a person whose client reads English is sent its twin. Worked
	// out from the Polish title itself, so a stand that stood through a restart
	// is read like one opened a minute ago.

	// The words of a title ChooseSignName put together that are ours rather
	// than an item's: his TANIO and OKAZJA in the forms he allows, and "KD"
	// after a piece whose soul stones are worth its price.
	struct TSignWord { const char* szPl; const char* szEn; };
	const TSignWord SIGN_SUFFIX_WORDS_EN[] = {
		{ "TANIO", "CHEAP" }, { "OKAZJA", "BARGAIN" }, { "Tanio", "Cheap" }, { "tanio", "cheap" },
		{ "Okazja", "Bargain" }, { "okazja", "bargain" },
	};
	const char* const SIGN_STONES_WORD = "KD";
	const char* const SIGN_STONES_WORD_EN = "stones";

	inline const char* GetSignSuffixEn(const std::string& word)
	{
		for (size_t i = 0; i < sizeof(SIGN_SUFFIX_WORDS_EN) / sizeof(SIGN_SUFFIX_WORDS_EN[0]); ++i)
			if (word == SIGN_SUFFIX_WORDS_EN[i].szPl)
				return SIGN_SUFFIX_WORDS_EN[i].szEn;
		return nullptr;
	}

	// A title in English: his name's twin (szNameEn), or for a title made of
	// pieces the same pieces with the item's official English name - a
	// +7..+9 piece and its plus with its bonus line, "KD" and TANIO or OKAZJA
	// after it, a soul stone and its grade, a material - fitted to a sign the
	// same way, dropping from the end. itemEn(the proto's Polish name) gives the
	// item's official English name or ""; labelEn(a bonus line's Polish label)
	// our English one or "". False, and the Polish title stands, for a title
	// that cannot be put into English whole.
	template <typename TItemEn, typename TLabelEn>
	inline bool TranslateSignName(const std::string& sign, TItemEn&& itemEn, TLabelEn&& labelEn, std::string& out)
	{
		out.clear();
		if (sign.empty())
			return false;
		for (size_t i = 0; i < SIGN_NAME_COUNT; ++i)
			if (sign == SIGN_NAMES[i].szName)
			{
				out = SIGN_NAMES[i].szNameEn ? SIGN_NAMES[i].szNameEn : "";
				return !out.empty();
			}

		// A piece and its plus: the last " +<digits>" that ends a word and,
		// with the words before it, is an item's name.
		for (size_t p = sign.rfind(" +"); p != std::string::npos; p = p > 0 ? sign.rfind(" +", p - 1) : std::string::npos)
		{
			size_t q = p + 2;
			while (q < sign.size() && sign[q] >= '0' && sign[q] <= '9')
				++q;
			if (q == p + 2 || (q < sign.size() && sign[q] != ' '))
				continue;
			const std::string base = sign.substr(0, p);
			const std::string plus = sign.substr(p + 2, q - p - 2);
			std::string en = itemEn(base + "+" + plus);
			if (en.empty())
				en = itemEn(base + " +" + plus);
			if (en.empty())
				continue;
			std::vector<std::string> words;
			for (size_t w = q; w < sign.size();)
			{
				while (w < sign.size() && sign[w] == ' ')
					++w;
				const size_t end = sign.find(' ', w);
				if (w < sign.size())
					words.push_back(sign.substr(w, end == std::string::npos ? std::string::npos : end - w));
				w = end;
			}
			std::string suffix, stones, bonus;
			if (!words.empty())
				if (const char* s = GetSignSuffixEn(words.back()))
				{
					suffix = s;
					words.pop_back();
				}
			if (!words.empty() && words.back() == SIGN_STONES_WORD)
			{
				stones = SIGN_STONES_WORD_EN;
				words.pop_back();
			}
			if (!words.empty())
			{
				size_t first = 0;
				std::string value;
				if (words[0].find_first_not_of("0123456789") == std::string::npos)
				{
					value = words[0];
					first = 1;
				}
				std::string label;
				for (size_t w = first; w < words.size(); ++w)
				{
					if (!label.empty())
						label += ' ';
					label += words[w];
				}
				const std::string labelEnglish = label.empty() ? std::string() : std::string(labelEn(label));
				if (labelEnglish.empty())
					return false;
				bonus = value.empty() ? labelEnglish : value + " " + labelEnglish;
			}
			return FitSignPieces({ playerbot_names::StripGrade(en) + " +" + plus, bonus, stones, suffix }, out);
		}

		// A material, whole, perhaps with TANIO or OKAZJA after it.
		std::string name = sign, suffix;
		const size_t space = sign.rfind(' ');
		if (space != std::string::npos)
			if (const char* s = GetSignSuffixEn(sign.substr(space + 1)))
			{
				name = sign.substr(0, space);
				suffix = s;
			}
		const std::string en = itemEn(name);
		if (en.empty())
			return false;
		return FitSignPieces({ en, suffix }, out);
	}
}

#endif
