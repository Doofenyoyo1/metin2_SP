#ifndef PLAYERBOT_ITEM_LINK_RULES_H
#define PLAYERBOT_ITEM_LINK_RULES_H
// An item as the client links one in a line of the chat, and a bot's reply
// with the items it names linked. What a player's Alt-click on an item puts
// in a line (playerGetItemLink, the client's PythonPlayerModule.cpp) is
// "item:" and the vnum, the flags and the three sockets in hex, then every
// bonus as its type in hex and its value in decimal, inside |H..|h, gold
// ("|cffffc700") with a bonus among the first five slots and pale
// ("|cfff1e6c0") without; the client prints "[name]" and opens the item's own
// tooltip on a click - its grade, stones and bonuses (HyperlinkItemToolTip,
// uitooltip.py, which reads the tokens back in this order). So a bot shows
// what it has instead of spelling it out ("zamiast pisac tekstem jakie ma
// itemy zalaczal je jako podglad itema", the operator, 27 September).
//
// No engine types; unit-tested in tests/playerbot_item_link_rules_test.cpp.
// The engine half (playerbot_chat_trade.h, playerbot_chat_conversation.h)
// reads the item and the stall line and sends the whisper.
#include <cstdio>
#include <string>
#include <vector>

namespace playerbot_item_link {

const int LINK_SOCKETS = 3;
// The client colours a link gold when a bonus stands in one of these slots.
const int LINK_NORM_ATTRS = 5;

// What the client shows of a whisper is one line: RecvWhisperPacket prints
// "%s : %s", the sender and the text, into a static char line[256] with
// _snprintf (PythonNetworkStreamPhaseGame.cpp) - past 255 bytes the line is
// cut, and MSVC's _snprintf leaves a cut line without its terminator. The
// packet itself would carry CHAT_MAX_LEN; the room that counts is this.
const size_t WHISPER_LINE_MAX = 255;

// The room a whisper's text has under a sender of that many bytes.
inline size_t WhisperRoom(size_t senderNameLen)
{
	const size_t used = senderNameLen + 3; // " : "
	return used < WHISPER_LINE_MAX ? WHISPER_LINE_MAX - used : 0;
}

struct TAttr
{
	unsigned char type;
	short value;
};

// The link, or "" when it would not fit what the client parses in one piece.
inline std::string Format(unsigned int vnum, unsigned int flags, const long* sockets,
		const TAttr* attrs, int attrCount, const char* name)
{
	char link[256];
	int len = snprintf(link, sizeof(link), "item:%x:%x:%x:%x:%x", vnum, flags,
			(unsigned int)sockets[0], (unsigned int)sockets[1], (unsigned int)sockets[2]);
	bool gold = false;
	for (int i = 0; i < attrCount && len > 0 && len < (int)sizeof(link); ++i)
	{
		if (attrs[i].type == 0)
			continue;
		len += snprintf(link + len, sizeof(link) - len, ":%x:%d", (unsigned int)attrs[i].type, (int)attrs[i].value);
		if (i < LINK_NORM_ATTRS)
			gold = true;
	}
	if (len <= 0 || len >= (int)sizeof(link))
		return std::string();
	std::string out(gold ? "|cffffc700" : "|cfff1e6c0");
	out += "|H";
	out += link;
	out += "|h[";
	out += name ? name : "";
	out += "]|h|r";
	return out;
}

// A byte of a word: a letter, a digit, or the high half, where the Polish
// letters of CP1250 are.
inline bool IsWordByte(unsigned char c)
{
	return c >= 0x80 || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Whether a name that ends at `end` ends there: not in the middle of a word,
// and not before a grade - "Miecz" is not the first half of "Miecz+5".
inline bool NameEndsAt(const std::string& text, size_t end)
{
	if (end >= text.size())
		return true;
	const unsigned char c = (unsigned char)text[end];
	if (IsWordByte(c))
		return false;
	return !(c == '+' && end + 1 < text.size() && text[end + 1] >= '0' && text[end + 1] <= '9');
}

struct TEntry
{
	std::string name;
	std::string link;
};

// The text with the named items linked, left to right: at each place the
// longest name not yet used that starts a word there and ends one, each entry
// once - two swords of one name take their own links in turn. A link goes in
// only while the whole still fits maxLen, because a link cut in half prints as
// raw text; the ones past the room keep their names.
inline std::string Substitute(const std::string& text, const std::vector<TEntry>& book, size_t maxLen)
{
	if (book.empty())
		return text;
	std::vector<bool> used(book.size(), false);
	std::string out;
	out.reserve(text.size());
	size_t i = 0;
	while (i < text.size())
	{
		int best = -1;
		size_t bestLen = 0;
		if (i == 0 || !IsWordByte((unsigned char)text[i - 1]))
		{
			for (size_t k = 0; k < book.size(); ++k)
			{
				const std::string& name = book[k].name;
				if (used[k] || name.empty() || name.size() <= bestLen || book[k].link.empty() ||
						text.compare(i, name.size(), name) != 0 || !NameEndsAt(text, i + name.size()))
					continue;
				best = (int)k;
				bestLen = name.size();
			}
		}
		if (best >= 0 && out.size() + book[best].link.size() + (text.size() - i - bestLen) <= maxLen)
		{
			out += book[best].link;
			used[best] = true;
			i += bestLen;
			continue;
		}
		out += text[i];
		++i;
	}
	return out;
}

}

#endif
