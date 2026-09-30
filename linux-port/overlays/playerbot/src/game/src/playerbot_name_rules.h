#ifndef __INC_METIN2_PLAYERBOT_NAME_RULES_H__
#define __INC_METIN2_PLAYERBOT_NAME_RULES_H__

// The official English names of this world's items and monsters, as the core
// reads them - pure, with no engine types, so tests/playerbot_name_rules_test.cpp
// can pin every rule; playerbot_language.h is the engine side that loads them.
//
// The server knows an item or a monster by its Polish proto name alone, and a
// chat line, a shout or a shop's title is text the client prints as it comes.
// tools/generate_english_names.py renders
// linux-port-mt2009/docker/game/playerbot_names_en.tsv from Gameforge's own
// English (the published client's locale pack, r40250's tables for what the
// pack lacks), one line a vnum:
//
//     i<TAB>10<TAB>Sword+0<TAB>6c1d2f3a
//
// "i" an item, "m" a monster or an NPC, and last the FNV-1a hash of the Polish
// proto name the generator matched the English one to. The core takes a line
// only while its world still calls the vnum that: an item an update renamed
// after the file was rendered keeps its Polish name instead of wearing another
// item's English one.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace playerbot_names
{
	enum ENameKind : uint8_t
	{
		NAME_NONE,
		NAME_ITEM,
		NAME_MOB
	};

	// Longer than any official name (the longest in the file has 25), shorter
	// than anything a stray line of another file would be.
	const size_t NAME_MAX_LEN = 48;

	struct TNameLine
	{
		uint8_t kind = NAME_NONE;
		uint32_t vnum = 0;
		std::string name;
		uint32_t polishHash = 0;
	};

	// FNV-1a over the bytes of a proto name as the engine holds it (CP1250),
	// the generator's hash of the same name.
	inline uint32_t HashProtoName(const char* name)
	{
		uint32_t hash = 2166136261u;
		for (const unsigned char* p = (const unsigned char*)(name ? name : ""); *p; ++p)
		{
			hash ^= *p;
			hash *= 16777619u;
		}
		return hash;
	}

	inline bool ParseHex32(const char* p, const char* end, uint32_t& out)
	{
		if (end - p != 8)
			return false;
		uint32_t value = 0;
		for (; p < end; ++p)
		{
			const char c = *p;
			uint32_t digit;
			if (c >= '0' && c <= '9')
				digit = (uint32_t)(c - '0');
			else if (c >= 'a' && c <= 'f')
				digit = (uint32_t)(c - 'a' + 10);
			else
				return false;
			value = value * 16u + digit;
		}
		out = value;
		return true;
	}

	// One line of the file. False for a comment, a blank line and for anything
	// that is not exactly a kind, a vnum, a printable ASCII name and a hash -
	// the file is ours, and a line it would not have written is no name.
	inline bool ParseNameLine(const char* line, TNameLine& out)
	{
		out = TNameLine();
		if (!line || (line[0] != 'i' && line[0] != 'm') || line[1] != '\t')
			return false;
		const uint8_t kind = line[0] == 'i' ? NAME_ITEM : NAME_MOB;
		const char* p = line + 2;
		uint32_t vnum = 0;
		const char* digits = p;
		while (*p >= '0' && *p <= '9')
		{
			vnum = vnum * 10u + (uint32_t)(*p - '0');
			if (vnum > 100000000u)
				return false;
			++p;
		}
		if (p == digits || vnum == 0 || *p != '\t')
			return false;
		const char* name = ++p;
		while (*p && *p != '\t' && *p != '\r' && *p != '\n')
		{
			if ((unsigned char)*p < 0x20 || (unsigned char)*p > 0x7E)
				return false;
			++p;
		}
		const size_t len = (size_t)(p - name);
		if (len == 0 || len > NAME_MAX_LEN || *p != '\t' || name[0] == ' ' || name[len - 1] == ' ')
			return false;
		const char* hash = ++p;
		while (*p && *p != '\r' && *p != '\n')
			++p;
		uint32_t polishHash = 0;
		if (!ParseHex32(hash, p, polishHash))
			return false;
		out.kind = kind;
		out.vnum = vnum;
		out.name.assign(name, len);
		out.polishHash = polishHash;
		return true;
	}

	// A name without the grade it ends in: "Sword+5" is "Sword", as a line that
	// says the grade apart names the piece ("Full Moon Sword +9"). A '+' no
	// digit follows is part of the name (White Hairband+, a refined material),
	// and a name that is nothing but a grade stays whole.
	inline std::string StripGrade(const std::string& name)
	{
		const std::string::size_type plus = name.rfind('+');
		if (plus == std::string::npos || plus == 0 || plus + 1 >= name.size() ||
				name.find_first_not_of("0123456789", plus + 1) != std::string::npos)
			return name;
		std::string base = name.substr(0, plus);
		while (!base.empty() && base[base.size() - 1] == ' ')
			base.erase(base.size() - 1);
		return base.empty() ? name : base;
	}
}

#endif
