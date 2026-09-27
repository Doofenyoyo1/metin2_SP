// playerbot_item_link_rules.h: an item as the client links one in a chat line,
// and a bot's reply with the items it names linked (upstream 2.2.28, our
// 2.2.30). Pure, no engine types.
//
//   g++ -std=c++23 -Wall -Wextra -Ilinux-port/overlays/playerbot/src/game/src -o /tmp/t tests/playerbot_item_link_rules_test.cpp && /tmp/t
#include "playerbot_item_link_rules.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace playerbot_item_link;

static int s_checks = 0;
static int s_failures = 0;

static void Check(bool ok, const char* what)
{
	++s_checks;
	if (!ok)
	{
		++s_failures;
		std::printf("FAIL: %s\n", what);
	}
}

static void CheckEq(const std::string& got, const std::string& want, const char* what)
{
	++s_checks;
	if (got != want)
	{
		++s_failures;
		std::printf("FAIL: %s\n  got:  %s\n  want: %s\n", what, got.c_str(), want.c_str());
	}
}

int main()
{
	const long noSockets[LINK_SOCKETS] = { 0, 0, 0 };
	const long stones[LINK_SOCKETS] = { 28030, 1, 0 };

	// What the client's Alt-click writes: hex vnum, flags and sockets, a pale
	// link without bonuses.
	CheckEq(Format(19, 0, noSockets, 0, 0, "Miecz+9"),
		"|cfff1e6c0|Hitem:13:0:0:0:0|h[Miecz+9]|h|r", "a plain item is a pale link");

	// A bonus: its type in hex, its value in decimal, and gold.
	TAttr attrs[7] = {};
	attrs[0].type = 72; attrs[0].value = 25;
	attrs[1].type = 1; attrs[1].value = -5;
	CheckEq(Format(299, 4, stones, attrs, 7, "Pelnia Ksiezyca"),
		"|cffffc700|Hitem:12b:4:6d7e:1:0:48:25:1:-5|h[Pelnia Ksiezyca]|h|r",
		"bonuses make a gold link, types in hex, values in decimal");

	// Only a bonus among the first five slots is gold; an empty slot is left out.
	TAttr rare[7] = {};
	rare[5].type = 53; rare[5].value = 10;
	CheckEq(Format(19, 0, noSockets, rare, 7, "X"),
		"|cfff1e6c0|Hitem:13:0:0:0:0:35:10|h[X]|h|r", "a sixth-slot bonus alone stays pale");

	// A null name still makes a link.
	Check(Format(19, 0, noSockets, 0, 0, 0).find("|h[]|h|r") != std::string::npos, "a null name is empty brackets");

	// The room of one whisper line.
	Check(WhisperRoom(10) == 242, "255 less the sender and \" : \"");
	Check(WhisperRoom(300) == 0, "a sender past the line leaves no room");

	// Word boundaries and grades.
	Check(NameEndsAt("Miecz", 5), "a name at the end of the text ends there");
	Check(!NameEndsAt("Mieczyk", 5), "not in the middle of a word");
	Check(!NameEndsAt("Miecz+5", 5), "not before a grade");
	Check(NameEndsAt("Miecz+ ok", 5), "a plus without a digit is no grade");
	Check(NameEndsAt("Miecz, i", 5), "a comma ends a name");
	Check(!NameEndsAt("Zbroja\xb3", 6), "a CP1250 letter continues a word");

	std::vector<TEntry> book;
	TEntry sword; sword.name = "Miecz+9"; sword.link = "<L1>";
	TEntry sword2; sword2.name = "Miecz+9"; sword2.link = "<L2>";
	TEntry longer; longer.name = "Miecz+9 Smoka"; longer.link = "<L3>";
	TEntry bare; bare.name = "Miecz"; bare.link = "<L4>";

	// Nothing to link, or no names: the text as it is.
	CheckEq(Substitute("mam Miecz+9", std::vector<TEntry>(), 255), "mam Miecz+9", "an empty book changes nothing");

	book.push_back(sword);
	CheckEq(Substitute("mam Miecz+9 i Miecz+9", book, 255), "mam <L1> i Miecz+9",
		"each entry is used once");
	book.push_back(sword2);
	CheckEq(Substitute("mam Miecz+9 i Miecz+9", book, 255), "mam <L1> i <L2>",
		"two swords of one name take their links in turn");

	book.clear();
	book.push_back(sword);
	book.push_back(longer);
	CheckEq(Substitute("mam Miecz+9 Smoka", book, 255), "mam <L3>", "the longest name wins");

	book.clear();
	book.push_back(bare);
	CheckEq(Substitute("mam Miecz+5", book, 255), "mam Miecz+5", "a name before a grade is not the item");
	CheckEq(Substitute("mam Mieczyk", book, 255), "mam Mieczyk", "a name inside a word is not the item");
	CheckEq(Substitute("aMiecz", book, 255), "aMiecz", "a name must start a word");
	CheckEq(Substitute("Miecz.", book, 255), "<L4>.", "at the start of the text");

	// A link that would not fit leaves the name; a later shorter one may.
	TEntry big; big.name = "Topor"; big.link = std::string(40, 'x');
	TEntry small; small.name = "Luk"; small.link = "<S>";
	book.clear();
	book.push_back(big);
	book.push_back(small);
	const std::string text = "Topor i Luk";
	CheckEq(Substitute(text, book, 20), "Topor i <S>", "a link past the room keeps its name");
	Check(Substitute(text, book, 20).size() <= 20, "the result fits the room");

	// An entry with no link (Format refused it) is never used.
	TEntry nolink; nolink.name = "Luk"; nolink.link = "";
	book.clear();
	book.push_back(nolink);
	CheckEq(Substitute("Luk", book, 255), "Luk", "no link, no substitution");

	std::printf("playerbot_item_link_rules: %d checks, %d failures\n", s_checks, s_failures);
	return s_failures ? 1 : 0;
}
