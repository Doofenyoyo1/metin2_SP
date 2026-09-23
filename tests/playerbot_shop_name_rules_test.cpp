// Iwakura's rules for naming a bot's counter (playerbot_shop_name_rules.h) over
// the names rendered from his list (playerbot_shop_names.h).
// Build: g++ -std=c++17 -Wall -Wextra -I linux-port/overlays/playerbot/src/game/src tests/playerbot_shop_name_rules_test.cpp
#include "playerbot_shop_name_rules.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace playerbot_shop_names;

// The dice, scripted: every roll must be one the test expected, in order.
struct TScript
{
	std::vector<int> values;
	size_t next = 0;
	int operator()(int lo, int hi)
	{
		assert(next < values.size());
		const int value = values[next++];
		assert(value >= lo && value <= hi);
		return value;
	}
	bool Spent() const { return next == values.size(); }
};

static const TSignName* Find(const std::string& text)
{
	for (size_t i = 0; i < SIGN_NAME_COUNT; ++i)
		if (text == SIGN_NAMES[i].szName)
			return &SIGN_NAMES[i];
	return nullptr;
}

static bool NamesVnum(const TSignName& name, uint32_t vnum)
{
	if (name.bNeed != SIGN_NEED_ALL_GOODS && name.bNeed != SIGN_NEED_ANY_GOODS)
		return false;
	for (uint8_t g = 0; g < name.bGoods; ++g)
		for (uint32_t v : name.aGoods[g].adwVnum)
			if (v == vnum)
				return true;
	return false;
}

static TSignLine Line(uint8_t kind, uint32_t vnum)
{
	TSignLine line;
	line.bLine = kind;
	line.dwVnum = vnum;
	return line;
}

static TSignLine Gear(int plus, uint8_t slot, int level, const char* name, uint64_t value)
{
	TSignLine line;
	line.bLine = GetSignGearLine(plus);
	line.dwVnum = 10 + plus;
	line.bGearSlot = slot;
	line.iLevel = level;
	line.strName = name;
	line.iPlus = plus;
	line.qwValue = value;
	return line;
}

static TSignLine Book(uint32_t skill)
{
	TSignLine line = Line(SIGN_LINE_BOOK, 50300);
	line.dwSkill = skill;
	return line;
}

static std::string Choose(const TSignCounter& counter, std::vector<int> values, uint8_t& how)
{
	TScript roll{ values };
	std::string out;
	const bool chosen = ChooseSignName(counter, roll, out, how);
	assert(chosen);
	assert(roll.Spent());
	return out;
}

int main()
{
	// Every rendered name is what ParseShopName leaves unchanged.
	static const unsigned char s_abPolish[] = {
		0xA5, 0xB9, 0xC6, 0xE6, 0xCA, 0xEA, 0xA3, 0xB3, 0xD1, 0xF1, 0xD3, 0xF3, 0x8C, 0x9C, 0x8F, 0x9F, 0xAF, 0xBF };
	int perKind[SIGN_KIND_COUNT] = { 0 };
	for (size_t i = 0; i < SIGN_NAME_COUNT; ++i)
	{
		const TSignName& name = SIGN_NAMES[i];
		const size_t len = std::strlen(name.szName);
		assert(len > 0 && len <= SIGN_MAX_LEN);
		std::string escaped;
		for (size_t c = 0; c < len; ++c)
		{
			const unsigned char b = (unsigned char)name.szName[c];
			bool proper = b >= 0x20 && b <= 0x7E;
			for (unsigned char polish : s_abPolish)
				proper = proper || b == polish;
			assert(proper);
			if (b == '\\' || b == '\'' || b == '"')
				escaped += '\\';
			escaped += (char)b;
		}
		assert(escaped.substr(0, SIGN_MAX_LEN) == name.szName);
		assert(name.bKind < SIGN_KIND_COUNT && name.bNeed < SIGN_NEED_COUNT && name.bGoods <= 3);
		if (name.bNeed == SIGN_NEED_ALL_GOODS || name.bNeed == SIGN_NEED_ANY_GOODS)
			assert(name.bGoods > 0 && name.aGoods[0].adwVnum[0] != 0);
		++perKind[name.bKind];
	}
	for (int kind = 0; kind < SIGN_KIND_COUNT; ++kind)
		assert(perKind[kind] > 0);

	uint8_t how = 0;

	// A third of the time, whatever the goods, a neutral name.
	{
		TSignCounter counter;
		counter.lines.push_back(Book(4));
		const std::string out = Choose(counter, { SIGN_NEUTRAL_OVERRIDE_PERCENT, 0 }, how);
		assert(how == SIGN_HOW_NEUTRAL_ROLL && Find(out) && Find(out)->bKind == SIGN_KIND_NEUTRAL);
	}

	// +7..+9 gear names the counter: suffix, KD, then the bonus go until it fits.
	{
		TSignCounter counter;
		TSignLine sword = Gear(9, SIGN_GEAR_WEAPON, 30, "Miecz Pe\xB3ni Ksi\xEA\xBFyca", 5000000);
		sword.strBonus = "1500 HP";
		sword.bStones = true;
		counter.lines.push_back(sword);
		counter.lines.push_back(Line(SIGN_LINE_MATERIAL, 30006));
		assert(Choose(counter, { 100, 0 }, how) == "Miecz Pe\xB3ni Ksi\xEA\xBFyca +9 1500 HP");
		assert(how == SIGN_HOW_TOP_GEAR);
		counter.lines[0].strName = "He\xB3m";
		assert(Choose(counter, { 100, 0 }, how) == "He\xB3m +9 1500 HP KD TANIO");
		counter.lines[0].strBonus.clear();
		counter.lines[0].bStones = false;
		assert(Choose(counter, { 100, 1 }, how) == "He\xB3m +9 OKAZJA");
		// Down to the name and its plus, never shorter.
		counter.lines[0].strName = "Bardzo D\xB3uga Nazwa Przedmiot";
		counter.lines[0].strBonus = "10 do P\xAF";
		assert(Choose(counter, { 100, 0 }, how) == "Bardzo D\xB3uga Nazwa Przedmiot +9");
		// A name that cannot fit even alone leaves the counter to its other goods.
		counter.lines[0].strName = "Bardzo D\xB3uga Nazwa Przedmiotu XYZ";
		const std::string rest = Choose(counter, { 100, 0, 1, 0 }, how);
		assert(how == SIGN_HOW_LIST && Find(rest) && Find(rest)->bKind == SIGN_KIND_MATERIALS);
	}

	// Two top pieces: the dearer one leads, and one time in three the list's
	// own name for such a counter.
	{
		TSignCounter counter;
		counter.lines.push_back(Gear(7, SIGN_GEAR_BODY, 34, "Zbroja", 400000));
		counter.lines.push_back(Gear(8, SIGN_GEAR_WEAPON, 30, "Miecz", 900000));
		assert(Choose(counter, { 100, 2, 0 }, how) == "Miecz +8 TANIO" && how == SIGN_HOW_TOP_GEAR);
		const std::string out = Choose(counter, { 100, 1, 0 }, how);
		assert(how == SIGN_HOW_LIST && Find(out) && Find(out)->bNeed == SIGN_NEED_TOP_GEAR);
	}

	// Soul stones lead: the best stone and its grade, with a suffix that fits.
	{
		TSignCounter counter;
		TSignLine a = Line(SIGN_LINE_STONE, 28337);
		a.strName = "Kamie\xF1 Duszy Potwora";
		a.iPlus = 3;
		a.qwValue = 80000;
		TSignLine b = a;
		b.dwVnum = 28437;
		b.iPlus = 4;
		b.qwValue = 350000;
		counter.lines = { a, b, Line(SIGN_LINE_MATERIAL, 30006) };
		assert(Choose(counter, { 100, 2 }, how) == std::string("Kamie\xF1 Duszy Potwora +4 ") + SIGN_GOODS_SUFFIXES[2]);
		assert(how == SIGN_HOW_STONE);
		counter.lines[1].strName = "Kamie\xF1 Duszy Przyspieszenia";
		assert(Choose(counter, { 100, 3 }, how) == "Kamie\xF1 Duszy Przyspieszenia +4");
		// A tie between stones and books goes to the stones.
		TSignCounter tie;
		tie.lines = { b, Book(4) };
		Choose(tie, { 100, 0 }, how);
		assert(how == SIGN_HOW_STONE);
	}

	// One material on half the material lines may name the counter.
	{
		TSignCounter counter;
		for (int i = 0; i < 3; ++i)
		{
			TSignLine tooth = Line(SIGN_LINE_MATERIAL, 30006);
			tooth.strName = "Z\xB9" "b Orka";
			counter.lines.push_back(tooth);
		}
		counter.lines.push_back(Line(SIGN_LINE_MATERIAL, 30010));
		counter.lines.push_back(Line(SIGN_LINE_NONE, 70037));
		assert(Choose(counter, { 100, 0, 4 }, how) == std::string("Z\xB9" "b Orka ") + SIGN_GOODS_SUFFIXES[3]);
		assert(how == SIGN_HOW_MATERIAL);
		assert(Choose(counter, { 100, 0, 0 }, how) == "Z\xB9" "b Orka");
		// Or a name from the list - one about teeth, never one about curse books.
		std::vector<const TSignName*> fits;
		CollectSignListNames(counter, SIGN_KIND_MATERIALS, SIGN_NEED_COUNT, fits);
		bool teeth = false, curse = false, generic = false;
		for (const TSignName* name : fits)
		{
			teeth = teeth || NamesVnum(*name, 30006);
			curse = curse || NamesVnum(*name, 30047);
			generic = generic || name->bNeed == SIGN_NEED_NONE;
		}
		assert(teeth && !curse && generic);
		const std::string out = Choose(counter, { 100, 1, 0 }, how);
		assert(how == SIGN_HOW_LIST && Find(out)->bKind == SIGN_KIND_MATERIALS);
		// Three materials a line each: no single one leads, straight to the list.
		TSignCounter mixed;
		mixed.lines = { Line(SIGN_LINE_MATERIAL, 30006), Line(SIGN_LINE_MATERIAL, 30010),
				Line(SIGN_LINE_MATERIAL, 30015) };
		const std::string listed = Choose(mixed, { 100, 0 }, how);
		assert(how == SIGN_HOW_LIST && Find(listed)->bKind == SIGN_KIND_MATERIALS);
		// A name that names two goods wants both.
		CollectSignListNames(mixed, SIGN_KIND_MATERIALS, SIGN_NEED_ALL_GOODS, fits);
		for (const TSignName* name : fits)
			assert(!NamesVnum(*name, 30016));
	}

	// Books: a name about every class wants every class.
	{
		TSignCounter counter;
		counter.lines = { Book(4), Book(19) };
		std::vector<const TSignName*> fits;
		CollectSignListNames(counter, SIGN_KIND_BOOKS, SIGN_NEED_BOOK_CLASSES, fits);
		assert(fits.empty());
		counter.lines.push_back(Book(31));
		counter.lines.push_back(Book(63));
		CollectSignListNames(counter, SIGN_KIND_BOOKS, SIGN_NEED_BOOK_CLASSES, fits);
		assert(fits.size() == 1);
		counter.lines.push_back(Book(94));
		CollectSignListNames(counter, SIGN_KIND_BOOKS, SIGN_NEED_BOOK_CLASSES, fits);
		assert(fits.size() == 2);
		CollectSignListNames(counter, SIGN_KIND_BOOKS, SIGN_NEED_FIRST_VILLAGE, fits);
		assert(fits.empty());
		counter.bFirstVillage = true;
		CollectSignListNames(counter, SIGN_KIND_BOOKS, SIGN_NEED_FIRST_VILLAGE, fits);
		assert(fits.size() == 1);
		const std::string out = Choose(counter, { 100, 0 }, how);
		assert(how == SIGN_HOW_LIST && Find(out)->bKind == SIGN_KIND_BOOKS);
	}

	// Gear: the level and slot names follow what is on the counter.
	{
		TSignCounter counter;
		counter.lines = { Gear(5, SIGN_GEAR_WEAPON, 50, "Miecz", 1), Gear(4, SIGN_GEAR_OTHER, 55, "Buty", 1) };
		std::vector<const TSignName*> fits;
		CollectSignListNames(counter, SIGN_KIND_GEAR, SIGN_NEED_COUNT, fits);
		for (const TSignName* name : fits)
			assert(name->bNeed != SIGN_NEED_LOW_GEAR && name->bNeed != SIGN_NEED_BODY_ARMOUR &&
					name->bNeed != SIGN_NEED_GEAR_KINDS && name->bNeed != SIGN_NEED_TOP_GEAR);
		CollectSignListNames(counter, SIGN_KIND_GEAR, SIGN_NEED_HIGH_GEAR, fits);
		assert(fits.size() == 1);
		counter.lines.push_back(Gear(6, SIGN_GEAR_BODY, 26, "Zbroja", 1));
		counter.lines.push_back(Gear(4, SIGN_GEAR_SHIELD, 41, "Tarcza", 1));
		CollectSignListNames(counter, SIGN_KIND_GEAR, SIGN_NEED_GEAR_KINDS, fits);
		assert(fits.size() == 1);
		CollectSignListNames(counter, SIGN_KIND_GEAR, SIGN_NEED_LOW_GEAR, fits);
		assert(fits.size() == 1);
		const std::string out = Choose(counter, { 100, 0 }, how);
		assert(how == SIGN_HOW_LIST && Find(out)->bKind == SIGN_KIND_GEAR);
	}

	// +0..+3 is his list for the blacksmith's fodder.
	{
		TSignCounter counter;
		counter.lines = { Gear(2, SIGN_GEAR_WEAPON, 30, "Luk", 1), Gear(0, SIGN_GEAR_BODY, 34, "Zbroja", 1),
				Line(SIGN_LINE_NONE, 70037) };
		const std::string out = Choose(counter, { 100, 0 }, how);
		assert(how == SIGN_HOW_LIST && Find(out)->bKind == SIGN_KIND_SCRAP);
	}

	// [INNE]: every name there is about its goods.
	{
		TSignCounter counter;
		counter.lines = { Line(SIGN_LINE_OTHER, 50050) };
		const std::string medal = Choose(counter, { 100, 0 }, how);
		assert(how == SIGN_HOW_LIST && Find(medal)->bKind == SIGN_KIND_OTHER && NamesVnum(*Find(medal), 50050));
		TSignLine ore = Line(SIGN_LINE_OTHER, 50608);
		ore.bOre = true;
		counter.lines = { ore };
		const std::string ores = Choose(counter, { 100, 0 }, how);
		assert(Find(ores)->bNeed == SIGN_NEED_ORES);
	}

	// Nothing from his lists: a neutral name. An empty counter: no name.
	{
		TSignCounter counter;
		counter.lines = { Line(SIGN_LINE_NONE, 70037), Line(SIGN_LINE_NONE, 50011) };
		const std::string out = Choose(counter, { 100, 0 }, how);
		assert(how == SIGN_HOW_NEUTRAL && Find(out)->bKind == SIGN_KIND_NEUTRAL);
		TSignCounter empty;
		TScript roll{ {} };
		std::string none;
		assert(!ChooseSignName(empty, roll, none, how) && none.empty());
	}

	std::cout << "PASS: " << SIGN_NAME_COUNT << " names fit the engine; neutral roll, top gear with the drop order,"
			<< " stones, a leading material, goods-bound names, book classes, gear levels, scrap, [INNE] and the neutral fallback\n";
}
