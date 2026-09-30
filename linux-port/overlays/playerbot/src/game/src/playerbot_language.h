#ifndef __INC_METIN2_PLAYERBOT_LANGUAGE_H__
#define __INC_METIN2_PLAYERBOT_LANGUAGE_H__

// Which of a text's two languages a person reads, and the lines said to one
// person or announced to everybody in it.
//
// Everything this project shows a player exists in Polish and in English, and
// the client's own switch decides which: Polish for a client set to Polish,
// English for every other language, English being what a Romanian or a German
// reads before Polish (playerbot_lang.py on the client; the bots' status lines
// have followed the same rule since 2.2.6). The server cannot see that switch,
// so it asks. A login quest (playerbot_lang.quest) sends "PlayerBotLanguage"
// at every entry into the game and a client that knows the question answers
// "/playerbot_lang en" or "pl" (OnPersonLanguage). The answer is a quest flag
// of the character's own, PLAYERBOT_PERSON_ENGLISH_FLAG: it is there at the
// next login before the question is asked again, it goes with the character to
// another core, and a quest reads it as pc.getf("playerbot_lang", "en"). A
// client that never answers - one from before the question, every client of
// the r40250 line - reads Polish, as it always did.
//
// What the AI says to one person is PBT(en, pl, english) with
// en = IsPlayerBotPersonEnglish(person), said through TellPlayerBotPerson; what
// it announces to everybody is BroadcastPlayerBotNotice(pl, english), and what
// a bot shouts SendPlayerBotShoutIn(pl, english, empire) - on every core each
// player is handed the half its client asked for.
//
// An item or a monster a line names is named in the reader's language too:
// GetPlayerBotItemNameIn / GetPlayerBotMobNameIn give the official English
// name (playerbot_names_en.tsv, Gameforge's own, see playerbot_name_rules.h)
// and the proto's Polish one where there is none. An NPC over its head and a
// bot's shop title go through the same names (playerbotify.py,
// apply_person_language_names).
//
// A bot's name, a guild's and a Polish map name are what they are.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, early - anything may speak to a person.

// A macro and not a function so that GCC checks both formats of a pair: the
// format of a printf-style call that is a conditional of two literals has
// each of them checked against the arguments (-Wformat), while a function's
// return value is a runtime string nobody checks. The two texts of a pair take
// the same conversions, and a call that formats one should be a printf-style
// one (snprintf, TellPlayerBotPerson) - ChatPacket declares no format.
#define PBT(en, pl, english) ((en) ? (english) : (pl))

namespace
{
	// A quest flag of the character's own: 1 English, anything else Polish.
	const char* const PLAYERBOT_PERSON_ENGLISH_FLAG = "playerbot_lang.en";
	// What each half of a bots' notice begins with (BroadcastPlayerBotNotice).
	const char PLAYERBOT_NOTICE_POLISH_MARK = '\x01';
	const char PLAYERBOT_NOTICE_ENGLISH_MARK = '\x02';

	// Whether this person reads our texts in English. A bot has no client, and
	// reads Polish. Asked through GetPCForce, which - unlike GetPC, and so
	// unlike CHARACTER::GetQuestFlag - leaves the quest manager's current
	// character where it is: a notice may go out from inside a quest that is
	// running for somebody else.
	bool IsPlayerBotPersonEnglish(LPCHARACTER ch)
	{
		if (!ch || !ch->IsPC() || !ch->GetDesc() || ch->GetDesc()->IsBot())
			return false;
		quest::PC* pc = quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID());
		return pc && pc->GetFlag(PLAYERBOT_PERSON_ENGLISH_FLAG) > 0;
	}

	// A guild's chat is one line for everybody in it, so a line the bots say
	// there is said in its master's language: a person's guild with bots in it
	// is its master's, and a bot guild's master reads Polish. A master on
	// another core reads Polish here too, as for the war's answer.
	bool IsPlayerBotGuildMasterEnglish(CGuild* guild)
	{
		return guild && IsPlayerBotPersonEnglish(CHARACTER_MANAGER::instance().FindByPID(guild->GetMasterPID()));
	}

	// One line in one person's chat. printf-style, so a PBT pair as its format
	// is checked on both sides.
	void TellPlayerBotPerson(LPCHARACTER person, const char* format, ...) __attribute__((format(printf, 2, 3)));
	void TellPlayerBotPerson(LPCHARACTER person, const char* format, ...)
	{
		if (!person || !person->GetDesc())
			return;
		char text[CHAT_MAX_LEN + 1];
		va_list args;
		va_start(args, format);
		vsnprintf(text, sizeof(text), format, args);
		va_end(args);
		person->ChatPacket(CHAT_TYPE_INFO, "%s", text);
	}

	// ----------------------------------------------------- the English names
	//
	// The official English names of this world's items and monsters
	// (linux-port-mt2009/docker/game/playerbot_names_en.tsv, rendered by
	// tools/generate_english_names.py from Gameforge's own tables; the rules
	// are playerbot_name_rules.h). The image copies the file into its share
	// (the Dockerfile's runtime stage); on a core without it - an older image,
	// the r40250 line - every name stays Polish, which is what a Polish reader
	// gets anyway.
	const char* const PLAYERBOT_NAMES_EN_FILE = "playerbot_names_en.tsv";
	const char* const PLAYERBOT_NAMES_EN_DEFAULT_DIR = "/opt/metin2/share";
	// An item every world has: until the protos are there, a line cannot be
	// checked against them, and the file is read at the first use after.
	const DWORD PLAYERBOT_NAMES_EN_PROBE_ITEM = 1;

	struct TPlayerBotEnglishNames
	{
		bool bLoaded = false;
		std::unordered_map<DWORD, std::string> items;
		std::unordered_map<DWORD, std::string> mobs;
		// A Polish item name as the proto has it -> its English one, "" for a
		// name two items share with two English names. For words that only
		// carry the name: a shop's title (playerbot_shop_name_rules.h).
		bool bItemsByPolishBuilt = false;
		std::unordered_map<std::string, std::string> itemsByPolish;
	};
	TPlayerBotEnglishNames s_PlayerBotEnglishNames;

	void LoadPlayerBotEnglishNames()
	{
		TPlayerBotEnglishNames& names = s_PlayerBotEnglishNames;
		if (names.bLoaded || !ITEM_MANAGER::instance().GetTable(PLAYERBOT_NAMES_EN_PROBE_ITEM))
			return;
		names.bLoaded = true;
		const char* dir = getenv("M2_SHARE_DIR");
		std::string path = (dir && *dir) ? dir : PLAYERBOT_NAMES_EN_DEFAULT_DIR;
		path += '/';
		path += PLAYERBOT_NAMES_EN_FILE;
		FILE* fp = fopen(path.c_str(), "r");
		if (!fp)
		{
			sys_log(0, "PLAYERBOT_LANG: no english names at %s - items and monsters keep their Polish names",
					path.c_str());
			return;
		}
		char line[256];
		unsigned int stale = 0, unknown = 0, malformed = 0;
		while (fgets(line, sizeof(line), fp))
		{
			if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == 0)
				continue;
			playerbot_names::TNameLine entry;
			if (!playerbot_names::ParseNameLine(line, entry))
			{
				++malformed;
				continue;
			}
			// The name the generator matched the English one to, as this world
			// has it: a rename since then is another item.
			const char* polish = NULL;
			if (entry.kind == playerbot_names::NAME_ITEM)
			{
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(entry.vnum);
				polish = proto ? proto->szLocaleName : NULL;
			}
			else
			{
				const CMob* mob = CMobManager::instance().Get(entry.vnum);
				polish = mob ? mob->m_table.szLocaleName : NULL;
			}
			if (!polish)
			{
				++unknown;
				continue;
			}
			if (playerbot_names::HashProtoName(polish) != entry.polishHash)
			{
				++stale;
				continue;
			}
			(entry.kind == playerbot_names::NAME_ITEM ? names.items : names.mobs)[entry.vnum] = entry.name;
		}
		fclose(fp);
		sys_log(0, "PLAYERBOT_LANG: english names from %s items=%u mobs=%u renamed_since=%u not_in_world=%u malformed=%u",
				path.c_str(), (unsigned int)names.items.size(), (unsigned int)names.mobs.size(), stale, unknown, malformed);
	}

	// The official English name, or NULL where there is none.
	const char* FindPlayerBotItemNameEn(DWORD vnum)
	{
		LoadPlayerBotEnglishNames();
		std::unordered_map<DWORD, std::string>::const_iterator it = s_PlayerBotEnglishNames.items.find(vnum);
		return it == s_PlayerBotEnglishNames.items.end() ? NULL : it->second.c_str();
	}

	const char* FindPlayerBotMobNameEn(DWORD vnum)
	{
		LoadPlayerBotEnglishNames();
		std::unordered_map<DWORD, std::string>::const_iterator it = s_PlayerBotEnglishNames.mobs.find(vnum);
		return it == s_PlayerBotEnglishNames.mobs.end() ? NULL : it->second.c_str();
	}

	// An item's name for a line in one of the two languages: the official
	// English one for an English reader where there is one, the proto's Polish
	// one otherwise - "" for a vnum the world does not have.
	std::string GetPlayerBotItemNameIn(DWORD dwVnum, bool bEnglish)
	{
		if (bEnglish)
			if (const char* en = FindPlayerBotItemNameEn(dwVnum))
				return en;
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(dwVnum);
		return proto ? std::string(proto->szLocaleName) : std::string();
	}

	// The same for a monster or an NPC.
	std::string GetPlayerBotMobNameIn(DWORD dwVnum, bool bEnglish)
	{
		if (bEnglish)
			if (const char* en = FindPlayerBotMobNameEn(dwVnum))
				return en;
		const CMob* mob = CMobManager::instance().Get(dwVnum);
		return mob ? std::string(mob->m_table.szLocaleName) : std::string();
	}

	// A Polish item name, as the proto spells it, in English: for text that
	// carries an item's name and not its vnum - a bot's shop title. "" where
	// the name is no item's, or where two items of that name have two English
	// names.
	std::string FindPlayerBotItemNameEnByPolish(const std::string& polish)
	{
		LoadPlayerBotEnglishNames();
		TPlayerBotEnglishNames& names = s_PlayerBotEnglishNames;
		if (!names.bItemsByPolishBuilt && names.bLoaded)
		{
			names.bItemsByPolishBuilt = true;
			for (std::unordered_map<DWORD, std::string>::const_iterator it = names.items.begin();
					it != names.items.end(); ++it)
			{
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(it->first);
				if (!proto)
					continue;
				std::pair<std::unordered_map<std::string, std::string>::iterator, bool> put =
						names.itemsByPolish.insert(std::make_pair(std::string(proto->szLocaleName), it->second));
				if (!put.second && put.first->second != it->second)
					put.first->second.clear();
			}
		}
		std::unordered_map<std::string, std::string>::const_iterator it = names.itemsByPolish.find(polish);
		return it == names.itemsByPolish.end() ? std::string() : it->second;
	}

	// A monster's English name for a notice. The server knows its monsters by
	// the Polish mob_proto only, and a notice is text the client prints as it
	// comes - there is no "{m<vnum>}" to fill in as the status line has - so a
	// notice names a monster by its official English name (the names file
	// above), or for the bosses it names, on a core without that file, by the
	// client's own (locale/en/mob_names.txt of the 2.0.35 locale pack, the same
	// words). Anything else keeps the name it has.
	const char* GetPlayerBotMobNameEn(DWORD vnum, const char* fallback)
	{
		if (const char* en = FindPlayerBotMobNameEn(vnum))
			return en;
		static const struct { DWORD vnum; const char* name; } kNames[] =
		{
			{ 191, "Lykos" }, { 192, "Scrofa" }, { 193, "Bera" }, { 194, "Tigris" },
			{ 491, "Mahon" }, { 492, "Bo" }, { 493, "Goo-Pae" }, { 494, "Chuong" },
			{ 591, "Bestial Captain" }, { 691, "Chief Orc" }, { 692, "Chief Elite Orc" },
			{ 791, "Dark Leader" }, { 792, "Dark-Ghost Leader" }, { 794, "Elite Dark-Ghost Leader" },
			{ 1093, "Death Reaper" }, { 1191, "Ice Witch" }, { 1192, "Mighty Ice Witch" },
			{ 1304, "Yellow Tiger Ghost" }, { 1901, "Nine Tails" }, { 1902, "Elite Nine Tails" },
			{ 2091, "Queen Spider" }, { 2191, "Giant Tortoise" }, { 2206, "Flame King" },
			{ 2207, "Dark Flame King" }, { 2306, "Giant Ghost Tree" }, { 2491, "Captain Yonghan" },
			{ 2492, "General Yonghan" }, { 2598, "Azrael" }, { 5001, "Pirate Tanaka" },
		};
		for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); ++i)
			if (kNames[i].vnum == vnum)
				return kNames[i].name;
		return fallback;
	}

	// A path skill's English name, for a line said in English: the server names
	// a skill after its skill book's Polish proto name (GetPlayerBotSkillName),
	// and a chat line has nothing the client could fill in. The names are the
	// English client's (locale/en/skilldesc.txt of the 2.0.35 locale pack), the
	// six skills of each of the eight paths; anything else keeps its name. The
	// trade layer reads a person's English line for a book by them too
	// (FindPlayerBotSkillByEnglishName).
	struct TPlayerBotSkillNameEn { DWORD vnum; const char* name; };
	const TPlayerBotSkillNameEn PLAYERBOT_SKILL_NAMES_EN[] =
	{
		{ 1, "Three-Way Cut" }, { 2, "Sword Spin" }, { 3, "Berserk" }, { 4, "Aura of the Sword" }, { 5, "Dash" },
		{ 16, "Spirit Strike (W)" }, { 17, "Bash" }, { 18, "Stump" }, { 19, "Strong Body" },
		{ 20, "Sword Strike" }, { 31, "Ambush" }, { 32, "Fast Attack" }, { 33, "Rolling Dagger" },
		{ 34, "Stealth" }, { 35, "Poisonous Cloud" }, { 46, "Repetitive Shot" }, { 47, "Arrow Shower" },
		{ 48, "Fire Arrow" }, { 49, "Feather Walk" }, { 50, "Poison Arrow" }, { 61, "Finger Strike" },
		{ 62, "Dragon Swirl" }, { 63, "Enchanted Blade" }, { 64, "Fear" }, { 65, "Enchanted Armour" },
		{ 66, "Dispel" }, { 76, "Dark Strike" }, { 77, "Flame Strike" }, { 78, "Flame Spirit" },
		{ 79, "Dark Protection" }, { 80, "Spirit Strike (S)" }, { 81, "Dark Orb" }, { 91, "Flying Talisman" },
		{ 92, "Shooting Dragon" }, { 93, "Dragon's Roar" }, { 94, "Blessing" }, { 95, "Reflect" },
		{ 96, "Dragon's Strength" }, { 106, "Lightning Throw" }, { 107, "Summon Lightning" },
		{ 108, "Lightning Claw" }, { 109, "Cure" }, { 110, "Swiftness" }, { 111, "Attack Up" },
	};

	const char* GetPlayerBotSkillNameEn(DWORD vnum, const char* fallback)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_SKILL_NAMES_EN) / sizeof(PLAYERBOT_SKILL_NAMES_EN[0]); ++i)
			if (PLAYERBOT_SKILL_NAMES_EN[i].vnum == vnum)
				return PLAYERBOT_SKILL_NAMES_EN[i].name;
		return fallback;
	}

	// A map's English name, bare, for the maps the conversation layer names in
	// Polish (playerbot_conv::GetMapWords, the same list) and in the words of
	// the bots' English status line (GetPlayerBotMapDestinationEn); "" for any
	// other, as the Polish table has.
	const char* GetPlayerBotMapNameEn(long mapIndex)
	{
		switch (mapIndex)
		{
			case 1: return "Yongan";
			case 2: return "Waryong";
			case 3: return "Jayang";
			case 4:
			case 24:
			case 44: return "Guild Land";
			case 5:
			case 25:
			case 45: return "Monkey Dungeon";
			case 21: return "Joan";
			case 23: return "Bokjung";
			case 41: return "Pyongmoo";
			case 43: return "Bakra";
			case 61: return "Mount Sohan";
			case 62: return "Doyyumhwaji";
			case 63: return "Yongbi Desert";
			case 64: return "Orc Valley";
			case 65: return "Hwang Temple";
			case 66: return "Demon Tower";
			case 67: return "Ghost Wood";
			case 68: return "Red Wood";
			case 71: return "Spider Dungeon 2";
			case 104: return "Spider Dungeon";
			case 108: return "Monkey Dungeon II";
			case 109: return "Monkey Dungeon III";
			default: return "";
		}
	}

	// A notice to every player on every core, each in its own language. On
	// mt2009 it goes out as two notices, a half per language each behind its
	// mark, and the engine's notice_packet_func hands every player its own half
	// (CPlayerBotManager::ShowsNoticeTo, playerbotify apply_person_language):
	// two notices and not one, because a core receiving a notice keeps 256
	// bytes of it (CInputP2P::Notice), and one line of both languages would not
	// fit. The r40250 line has no such filter and hears the Polish one.
	void BroadcastPlayerBotNotice(const char* pl, const char* english)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		std::string half;
		half.reserve(strlen(pl) + 1);
		half += PLAYERBOT_NOTICE_POLISH_MARK;
		half += pl;
		BroadcastNotice(half.c_str());
		half.clear();
		half += PLAYERBOT_NOTICE_ENGLISH_MARK;
		half += english;
		BroadcastNotice(half.c_str());
#else
		(void)english;
		BroadcastNotice(pl);
#endif
	}

	// A bot's shout, each reader's half: the same two halves behind the same
	// marks as a notice, and every core's FuncShout (input_p2p.cpp) hands a
	// player the half it reads and drops the other (playerbotify
	// apply_person_language_names). A shout is sent whole to the other cores
	// (TPacketGGShout, CHAT_MAX_LEN) and to this one's players, so each half
	// is a shout of its own, the bot's name in front as a player's has it. The
	// r40250 line has no such filter and hears the Polish one.
	void SendPlayerBotShoutIn(const char* pl, const char* english, BYTE bEmpire)
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		std::string half;
		half.reserve(strlen(pl) + 1);
		half += PLAYERBOT_NOTICE_POLISH_MARK;
		half += pl;
		SendPlayerBotShout(half.c_str(), bEmpire);
		half.clear();
		half += PLAYERBOT_NOTICE_ENGLISH_MARK;
		half += english;
		SendPlayerBotShout(half.c_str(), bEmpire);
#else
		(void)english;
		SendPlayerBotShout(pl, bEmpire);
#endif
	}
}

#endif
