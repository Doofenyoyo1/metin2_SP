#ifndef __INC_METIN2_PLAYERBOT_CHAT_TRADE_H__
#define __INC_METIN2_PLAYERBOT_CHAT_TRADE_H__

// Trading over the chat: what a bot shouts about its counter and its wants,
// and what it whispers back when a player shouts "Kupie ..." or "Sprzedam ...".
//
// The market already exists - counters in Joan and Bokjung, a ledger of who
// is short of what - but a player only found out by walking the ring. A
// player on a real server finds out from the shout channel, and answers a
// shout with a whisper, and that is the shape copied here: a bot that opens
// a counter with something worth crossing town for says so once, a bot that
// walked the market for a material and found none asks for it once, and a
// player's own shout is read for the two words that matter and answered by
// the one bot best placed to answer - the nearest counter that has the
// thing, or a bot that is short of it.
//
// The engine side of this is patch 0007: CInputMain::Chat hands a player's
// shout to CPlayerBotManager::OnPlayerShout after it has gone out, and
// CInputMain::Whisper hands a whisper addressed to a bot to OnPlayerWhisper
// instead of writing it to a descriptor with no client behind it. Both are
// one call each; everything they call is here. A whisper from a person on
// another core - the other channel, or a map this core does not host - comes
// by the P2P relay, and CInputP2P::Relay hands it to OnPeerWhisper the same
// way (mt2009, playerbotify apply_peer_whisper_to_bot); the answer goes back
// by the relay too (SendPlayerBotWhisperTo).
//
// Text is CP1250, which is what the Polish client sends and what the item
// names in the proto are written in. Matching folds both sides to lowercase
// ASCII so "Kupię Kość Niedźwiedzia" finds "Kość Niedźwiedzia" whether or not
// the player bothered with the diacritics. What a bot says is ASCII, as
// everywhere else; the item names it quotes are the proto's own.
//
// And in the reader's language (Jeremus-Sama, 28 September): a person whose
// client reads English, or who wrote the line in English ("WTB", "WTS",
// "buy", "sell"), is answered in English with the items' official English
// names (playerbot_language.h), and a line in English is matched against those
// names as well as the Polish ones. A bot's shout is two, a half per language
// (SendPlayerBotShoutIn), and every reader is handed its own.
//
// An implementation fragment in the sense playerbot_types.h describes:
// include it exactly once, after playerbot_market.h - it reads the counters
// the way a shopping bot does, and the market's own helpers for what a bot
// wants.

// The players' names for items (FMS, 12D, bodzio ...) - pure, playerbot_conv_aliases.h.
#include "playerbot_conv_aliases.h"

namespace
{
	// One trade shout on the world channel this often, whoever it is from,
	// and one from any single bot this often. The refine announcements run at
	// one every three minutes; with these the channel carries a line a minute
	// at the most, which reads as a market and not as a wall.
	const DWORD PLAYERBOT_TRADE_SHOUT_INTERVAL = 90000;
	const DWORD PLAYERBOT_TRADE_SHOUT_BOT_INTERVAL = 1200000;
	// A player gets one whispered answer this often, so a shout repeated
	// twice does not bring two bots to the same door.
	const DWORD PLAYERBOT_TRADE_REPLY_INTERVAL = 8000;
	// Fewer letters than this after the verb is not a thing anybody meant.
	const size_t PLAYERBOT_TRADE_QUERY_MIN = 3;
	// The skill books the proto names one skill each - "Instr. Aura Miecza",
	// value 0 the skill - which is the only place the server has a skill's
	// Polish name. skill_proto holds the Korean ones.
	const DWORD PLAYERBOT_TRADE_SKILL_BOOK_FIRST = 50401;
	const DWORD PLAYERBOT_TRADE_SKILL_BOOK_LAST = 50511;

	DWORD s_dwPlayerBotTradeShoutTime = 0;
	std::map<DWORD, DWORD> s_mapPlayerBotTradeShoutTime;
	std::map<DWORD, DWORD> s_mapPlayerBotTradeReplyTime;

	const char* GetPlayerBotTownName(long mapIndex)
	{
		// The engine's own quests name these: new_quest_lv52 for the first
		// villages, new_quest_lv7 for the second.
		switch (mapIndex)
		{
			case 1: return "Yongan";
			case 3: return "Jayang";
			case PLAYERBOT_MAP_CHUNJO_M1: return "Joan";
			case PLAYERBOT_MAP_CHUNJO_M2: return "Bokjung";
			case 41: return "Pyongmoo";
			case 43: return "Bakra";
			default: return "miescie";
		}
	}

	// The villages have one name in both languages; "in town" is the other
	// case's word.
	const char* GetPlayerBotTownNameIn(long mapIndex, bool english)
	{
		const char* name = GetPlayerBotTownName(mapIndex);
		return english && strcmp(name, "miescie") == 0 ? "town" : name;
	}

	// Lowercase ASCII from CP1250: the Polish letters go to their base, the
	// rest of the high half to '?', so a name compares the same however it
	// was typed.
	void FoldPlayerBotChatText(const char* in, char* out, size_t size)
	{
		size_t o = 0;
		for (const unsigned char* p = (const unsigned char*)(in ? in : ""); *p && o + 1 < size; ++p)
		{
			unsigned char c = *p;
			switch (c)
			{
				case 0xA5: case 0xB9: c = 'a'; break;
				case 0xC6: case 0xE6: c = 'c'; break;
				case 0xCA: case 0xEA: c = 'e'; break;
				case 0xA3: case 0xB3: c = 'l'; break;
				case 0xD1: case 0xF1: c = 'n'; break;
				case 0xD3: case 0xF3: c = 'o'; break;
				case 0x8C: case 0x9C: c = 's'; break;
				case 0x8F: case 0x9F: case 0xAF: case 0xBF: c = 'z'; break;
				default:
					if (c >= 'A' && c <= 'Z')
						c = (unsigned char)(c - 'A' + 'a');
					else if (c >= 0x80)
						c = '?';
					break;
			}
			out[o++] = (char)c;
		}
		out[o] = 0;
	}

	bool IsPlayerBotChatSeparator(char c)
	{
		return playerbot_lure_rules::IsSeparator(c);
	}

	// The whisper the client shows as one from the bot: the same packet
	// CInputMain::Whisper builds for a player, with the bot's name as sender.
	// `relayTo` names the person when `desc` is another core's P2P descriptor
	// rather than the person's own: that core hands the packet to its client
	// (CInputP2P::Relay), as it does a player's whisper to somebody the
	// sender's core does not hold.
	void SendPlayerBotWhisperPacket(LPCHARACTER bot, LPDESC desc, const char* relayTo, const char* text)
	{
		const size_t len = std::min<size_t>(strlen(text), CHAT_MAX_LEN);
		TPacketGCWhisper pack;
		pack.bHeader = HEADER_GC_WHISPER;
		pack.bType = WHISPER_TYPE_NORMAL;
		pack.wSize = (WORD)(sizeof(TPacketGCWhisper) + len);
		strlcpy(pack.szNameFrom, bot->GetName(), sizeof(pack.szNameFrom));
		TEMP_BUFFER tmpbuf;
		tmpbuf.write(&pack, sizeof(pack));
		tmpbuf.write(text, (int)len);
		if (relayTo)
			desc->SetRelay(relayTo);
		desc->Packet(tmpbuf.read_peek(), tmpbuf.size());
		// The packet clears the relay name - unless the descriptor refused it
		// (a peer closing), and then the next packet to that core would go
		// astray under this person's name. CInputMain::Whisper clears it too.
		if (relayTo)
			desc->SetRelay("");
	}

	void SendPlayerBotWhisper(LPCHARACTER bot, LPCHARACTER to, const char* text)
	{
		if (!bot || !to || !to->GetDesc() || !text || !*text)
			return;
		SendPlayerBotWhisperPacket(bot, to->GetDesc(), NULL, text);
		sys_log(0, "PLAYERBOT_TRADE: whisper pid=%u name=%s to=%s text=\"%s\"",
				bot->GetPlayerID(), bot->GetName(), to->GetName(), text);
	}

	// A whisper to somebody another core holds, by the P2P table's line for
	// them - the table every core keeps of every other core's characters.
	// False when there is none: the person has logged out, or has come to this
	// core in the meantime (a character here is not in the table).
	bool SendPlayerBotWhisperToPeer(LPCHARACTER bot, const char* toName, const char* text)
	{
		if (!bot || !toName || !*toName || !text || !*text)
			return false;
		CCI* peer = P2P_MANAGER::instance().Find(toName);
		if (!peer || !peer->pkDesc)
			return false;
		SendPlayerBotWhisperPacket(bot, peer->pkDesc, peer->szName, text);
		return true;
	}

	// Whoever whispered or shouted. A character of this core, or - `local`
	// NULL - one another core holds: the other channel's core, or the core of
	// a map this one does not host. That one is known here by its line in the
	// P2P table, reaches a bot here through the P2P relay (CInputP2P::Relay,
	// OnPeerWhisper) and is answered the same way (SendPlayerBotWhisperTo). A
	// bot lives on one core only, so the answer can only come from there.
	// english: whether the person's client reads English (the flag its answer
	// left, IsPlayerBotPersonEnglish). Known of a character of this core only:
	// the flag is a quest flag of the core the person plays on, so a person
	// another core holds reads Polish here unless the line itself is English
	// (AnswerPlayerBotTradeLine).
	struct TPlayerBotPerson
	{
		DWORD pid;
		std::string name;
		long mapIndex;
		int channel;
		LPCHARACTER local;
		bool english;
		TPlayerBotPerson() : pid(0), mapIndex(0), channel(0), local(NULL), english(false) {}
	};

	TPlayerBotPerson GetPlayerBotLocalPerson(LPCHARACTER ch)
	{
		TPlayerBotPerson person;
		if (!ch)
			return person;
		person.pid = ch->GetPlayerID();
		person.name = ch->GetName();
		person.mapIndex = ch->GetMapIndex();
		person.channel = g_bChannel;
		person.local = ch;
		person.english = IsPlayerBotPersonEnglish(ch);
		return person;
	}

	TPlayerBotPerson GetPlayerBotPeerPerson(const CCI* peer)
	{
		TPlayerBotPerson person;
		if (!peer)
			return person;
		person.pid = peer->dwPID;
		person.name = peer->szName;
		person.mapIndex = peer->lMapIndex;
		person.channel = peer->bChannel;
		return person;
	}

	// A bot's whisper to a person wherever the person is.
	void SendPlayerBotWhisperTo(LPCHARACTER bot, const TPlayerBotPerson& to, const char* text)
	{
		if (to.local)
		{
			SendPlayerBotWhisper(bot, to.local, text);
			return;
		}
		if (!bot || !text || !*text)
			return;
		if (SendPlayerBotWhisperToPeer(bot, to.name.c_str(), text))
			sys_log(0, "PLAYERBOT_TRADE: whisper pid=%u name=%s to=%s to_channel=%d text=\"%s\"",
					bot->GetPlayerID(), bot->GetName(), to.name.c_str(), to.channel, text);
		else
			sys_log(0, "PLAYERBOT_CHAT: reply to another core undelivered pid=%u name=%s to=%s",
					bot->GetPlayerID(), bot->GetName(), to.name.c_str());
	}

	// An item as the client links one in a line of the chat, what a player's
	// Alt-click puts there: the format is playerbot_item_link_rules.h's, the
	// item this one's - a live item or an offline shop's record of one.
	std::string FormatPlayerBotItemLink(DWORD vnum, DWORD flags, const long* sockets,
			const TPlayerItemAttribute* attrs, const char* name)
	{
		long socketsOf[playerbot_item_link::LINK_SOCKETS] = { 0, 0, 0 };
		for (int i = 0; i < playerbot_item_link::LINK_SOCKETS && i < ITEM_SOCKET_MAX_NUM; ++i)
			socketsOf[i] = sockets[i];
		playerbot_item_link::TAttr attrsOf[ITEM_ATTRIBUTE_MAX_NUM];
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			attrsOf[i].type = attrs[i].bType;
			attrsOf[i].value = attrs[i].sValue;
		}
		return playerbot_item_link::Format(vnum, flags, socketsOf, attrsOf, ITEM_ATTRIBUTE_MAX_NUM, name);
	}

	// A live item's link, printed under `name` (its proto's by default).
	std::string MakePlayerBotItemLink(LPITEM item, const char* name = NULL)
	{
		if (!item || !item->GetProto())
			return std::string();
		long sockets[ITEM_SOCKET_MAX_NUM];
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			sockets[i] = item->GetSocket(i);
		TPlayerItemAttribute attrs[ITEM_ATTRIBUTE_MAX_NUM];
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			attrs[i].bType = item->GetAttributeType(i);
			attrs[i].sValue = item->GetAttributeValue(i);
		}
		return FormatPlayerBotItemLink(item->GetVnum(), (DWORD)item->GetFlag(), sockets, attrs,
				name && *name ? name : item->GetProto()->szLocaleName);
	}

	// A reply with the stall lines it names linked while the client can show
	// the whole line (playerbot_item_link::Substitute, WhisperRoom): the ones
	// past the room keep their names, because a cut link prints as raw text.
	std::string LinkPlayerBotTradeReply(LPCHARACTER sender, const char* text,
			const std::vector<playerbot_item_link::TEntry>& links)
	{
		return playerbot_item_link::Substitute(text ? text : "", links,
				playerbot_item_link::WhisperRoom(strlen(sender->GetName())));
	}

	// A line on the world channel in the bot's name, within the two throttles:
	// the Polish and the English of it, each reader handed its own
	// (SendPlayerBotShoutIn).
	bool ShoutPlayerBotTrade(LPCHARACTER bot, const char* text, const char* textEn, DWORD dwNow)
	{
		if (!bot || !text || !*text || !textEn || !*textEn)
			return false;
		if (s_dwPlayerBotTradeShoutTime != 0 &&
				dwNow - s_dwPlayerBotTradeShoutTime < PLAYERBOT_TRADE_SHOUT_INTERVAL)
			return false;
		DWORD& last = s_mapPlayerBotTradeShoutTime[bot->GetPlayerID()];
		if (last != 0 && dwNow - last < PLAYERBOT_TRADE_SHOUT_BOT_INTERVAL)
			return false;
		s_dwPlayerBotTradeShoutTime = last = dwNow;
		char msg[CHAT_MAX_LEN + 1];
		snprintf(msg, sizeof(msg), "%s : %s", bot->GetName(), text);
		char msgEn[CHAT_MAX_LEN + 1];
		snprintf(msgEn, sizeof(msgEn), "%s : %s", bot->GetName(), textEn);
		SendPlayerBotShoutIn(msg, msgEn, bot->GetEmpire());
		sys_log(0, "PLAYERBOT_TRADE: shout pid=%u name=%s text=\"%s\"",
				bot->GetPlayerID(), bot->GetName(), text);
		return true;
	}

	// The counter just opened with something worth crossing town for; the
	// keeper says so. Called from the stall code with the headline item.
	void AnnouncePlayerBotStall(LPCHARACTER ch, DWORD dwItemVnum)
	{
		const std::string name = GetPlayerBotItemNameIn(dwItemVnum, false);
		if (!ch || name.empty())
			return;
		char text[CHAT_MAX_LEN + 1];
		snprintf(text, sizeof(text), "Sprzedam %s - stragan w %s",
				name.c_str(), GetPlayerBotTownName(ch->GetMapIndex()));
		char textEn[CHAT_MAX_LEN + 1];
		snprintf(textEn, sizeof(textEn), "Selling %s - stall in %s",
				GetPlayerBotItemNameIn(dwItemVnum, true).c_str(), GetPlayerBotTownNameIn(ch->GetMapIndex(), true));
		ShoutPlayerBotTrade(ch, text, textEn, get_dword_time());
	}

	// The bot walked the market for a material and found none: it asks. Called
	// from the market code when a trip ends with nothing on offer.
	void AnnouncePlayerBotNeed(LPCHARACTER ch)
	{
		if (!ch)
			return;
		// On the 2.x line this is asked after every empty look at a first
		// village's stands, hundreds of times a minute, and only one shout in
		// PLAYERBOT_TRADE_SHOUT_INTERVAL goes out: the world-wide throttle is
		// asked before the bag is, which is the costly half.
		if (s_dwPlayerBotTradeShoutTime != 0 &&
				get_dword_time() - s_dwPlayerBotTradeShoutTime < PLAYERBOT_TRADE_SHOUT_INTERVAL)
			return;
		std::set<DWORD> wanted;
		CollectPlayerBotWantedMaterials(ch, wanted);
		if (wanted.empty())
			return;
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(*wanted.begin());
		if (!proto)
			return;
		char text[CHAT_MAX_LEN + 1];
		snprintf(text, sizeof(text), "Kupie %s - kto ma, niech wystawi w %s",
				proto->szLocaleName, GetPlayerBotTownName(ch->GetMapIndex()));
		char textEn[CHAT_MAX_LEN + 1];
		snprintf(textEn, sizeof(textEn), "Buying %s - whoever has it, put it up in %s",
				GetPlayerBotItemNameIn(proto->dwVnum, true).c_str(), GetPlayerBotTownNameIn(ch->GetMapIndex(), true));
		ShoutPlayerBotTrade(ch, text, textEn, get_dword_time());
	}

	// The skill a folded name means, from the per-skill books' names.
	DWORD FindPlayerBotSkillByName(const char* foldedQuery)
	{
		if (!foldedQuery || strlen(foldedQuery) < PLAYERBOT_TRADE_QUERY_MIN)
			return 0;
		for (DWORD vnum = PLAYERBOT_TRADE_SKILL_BOOK_FIRST; vnum <= PLAYERBOT_TRADE_SKILL_BOOK_LAST; ++vnum)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
			if (!proto || proto->bType != ITEM_SKILLBOOK)
				continue;
			char name[64];
			FoldPlayerBotChatText(proto->szLocaleName, name, sizeof(name));
			const char* p = name;
			if (strncmp(p, "instr. ", 7) == 0)
				p += 7;
			if (strstr(p, foldedQuery) || strstr(foldedQuery, p))
				return (DWORD)proto->alValues[0];
		}
		return 0;
	}

	// The skill a folded English line means, by the English client's skill
	// names (PLAYERBOT_SKILL_NAMES_EN): "wtb book aura of the sword".
	DWORD FindPlayerBotSkillByEnglishName(const char* foldedQuery)
	{
		if (!foldedQuery || strlen(foldedQuery) < PLAYERBOT_TRADE_QUERY_MIN)
			return 0;
		for (size_t i = 0; i < sizeof(PLAYERBOT_SKILL_NAMES_EN) / sizeof(PLAYERBOT_SKILL_NAMES_EN[0]); ++i)
		{
			char name[64];
			FoldPlayerBotChatText(PLAYERBOT_SKILL_NAMES_EN[i].name, name, sizeof(name));
			if (strstr(name, foldedQuery) || strstr(foldedQuery, name))
				return PLAYERBOT_SKILL_NAMES_EN[i].vnum;
		}
		return 0;
	}

	// The skill a book query names: an English line by the English names
	// first, and by the Polish ones as every line always was.
	DWORD FindPlayerBotSkillByNameIn(const char* foldedQuery, bool english)
	{
		const DWORD skill = english ? FindPlayerBotSkillByEnglishName(foldedQuery) : 0;
		return skill ? skill : FindPlayerBotSkillByName(foldedQuery);
	}

	// The Polish name of a skill, for a bot's own line about it.
	const char* GetPlayerBotSkillName(DWORD skillVnum)
	{
		for (DWORD vnum = PLAYERBOT_TRADE_SKILL_BOOK_FIRST; vnum <= PLAYERBOT_TRADE_SKILL_BOOK_LAST; ++vnum)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
			if (proto && proto->bType == ITEM_SKILLBOOK && (DWORD)proto->alValues[0] == skillVnum)
				return strncmp(proto->szLocaleName, "Instr. ", 7) == 0
						? proto->szLocaleName + 7 : proto->szLocaleName;
		}
		return "?";
	}

	// A skill's book by its official English name ("Aura of the Sword
	// Manual"), the name the English client gives the book the Polish one
	// calls "Instr. Aura Miecza"; "" where there is none. Asked of every book
	// line of every counter when an English line is matched, so each skill's
	// answer is kept once the names are there.
	std::string GetPlayerBotSkillBookNameEn(DWORD skillVnum)
	{
		static std::map<DWORD, std::string> s_mapBookNames;
		std::map<DWORD, std::string>::const_iterator known = s_mapBookNames.find(skillVnum);
		if (known != s_mapBookNames.end())
			return known->second;
		std::string name;
		for (DWORD vnum = PLAYERBOT_TRADE_SKILL_BOOK_FIRST; vnum <= PLAYERBOT_TRADE_SKILL_BOOK_LAST; ++vnum)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
			if (proto && proto->bType == ITEM_SKILLBOOK && (DWORD)proto->alValues[0] == skillVnum)
			{
				const char* en = FindPlayerBotItemNameEn(vnum);
				if (en)
					name = en;
				break;
			}
		}
		if (s_PlayerBotEnglishNames.bLoaded)
			s_mapBookNames[skillVnum] = name;
		return name;
	}

	bool PlayerBotItemNameMatches(LPITEM item, const char* foldedQuery)
	{
		if (!item || !item->GetProto())
			return false;
		char name[64];
		FoldPlayerBotChatText(item->GetProto()->szLocaleName, name, sizeof(name));
		return strstr(name, foldedQuery) != NULL;
	}

	// ------------------------------------------------------------ the stall
	//
	// What a bot has up for sale, whichever engine holds it: the classic stall
	// (CHARACTER::GetMyShop, the lines in vecShopOffers) or, on mt2009 with
	// ENABLE_IKASHOP_RENEWAL, the Ikarus offline shop the classic one is moved
	// to the moment it opens (ManagePlayerBotShopLifetime, "migrate_offline").
	// Asking only GetMyShop() on that engine says "no stall" for every keeper -
	// which is what the conversation and the shout answers did.
	struct TPlayerBotStallLine
	{
		std::string name;
		// The item's chat link (MakePlayerBotItemLink), printed as the name.
		std::string link;
		DWORD vnum;
		DWORD skill;
		// A Forgetting Book's line (ITEM_SKILLFORGET, the skill in socket 0),
		// not a skill book's: "KZ", which players buy by the skill too.
		bool forget;
		long long price;
		unsigned int count;
		TPlayerBotStallLine() : vnum(0), skill(0), forget(false), price(0), count(1) {}
	};

	struct TPlayerBotStall
	{
		bool open;
		bool offline;
		long mapIndex;
		long x;
		long y;
		int channel;
		std::vector<TPlayerBotStallLine> lines;
		TPlayerBotStall() : open(false), offline(false), mapIndex(0), x(0), y(0), channel(0) {}
	};

	std::string GetPlayerBotStallLineName(const TItemTable* proto, DWORD skill, bool forget)
	{
		if (skill)
		{
			const char* skillName = GetPlayerBotSkillName(skill);
			if (skillName && strcmp(skillName, "?") != 0)
				return forget && proto ? std::string(proto->szLocaleName) + " (" + skillName + ")"
						: std::string("Instr. ") + skillName;
		}
		return proto ? std::string(proto->szLocaleName) : std::string();
	}

	// A stall line's name for a reader of English: the book's official English
	// name for a skill book, the item's with its skill's after it for a
	// Forgetting Book, the item's otherwise - and the Polish line's name where
	// the official English one is missing, never one of our own making.
	std::string GetPlayerBotStallLineNameEn(const TPlayerBotStallLine& line)
	{
		if (line.skill && !line.forget)
		{
			const std::string book = GetPlayerBotSkillBookNameEn(line.skill);
			return book.empty() ? line.name : book;
		}
		const char* en = FindPlayerBotItemNameEn(line.vnum);
		if (!en)
			return line.name;
		if (line.forget && line.skill)
		{
			const char* skill = GetPlayerBotSkillNameEn(line.skill, NULL);
			return skill ? std::string(en) + " (" + skill + ")" : line.name;
		}
		return en;
	}

	std::string GetPlayerBotStallLineNameIn(const TPlayerBotStallLine& line, bool english)
	{
		return english ? GetPlayerBotStallLineNameEn(line) : line.name;
	}

	// The line's link under that name, so a reply says it the way it links it.
	std::string GetPlayerBotStallLineLinkIn(const TPlayerBotStallLine& line, bool english)
	{
		if (!english || line.link.empty())
			return line.link;
		const std::string name = GetPlayerBotStallLineNameEn(line);
		return name == line.name ? line.link : playerbot_item_link::Rename(line.link, name);
	}

	// `keeper` may be NULL (the offline shop stands whether or not its owner
	// is in the world).
	bool GetPlayerBotStall(DWORD pid, LPCHARACTER keeper, TPlayerBotStall& out)
	{
		out = TPlayerBotStall();
		if (keeper && keeper->GetMyShop())
		{
			out.open = true;
			out.mapIndex = keeper->GetMapIndex();
			out.x = keeper->GetX();
			out.y = keeper->GetY();
			out.channel = g_bChannel;
			TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(pid);
			if (it != s_mapPlayerBotAIStates.end())
			{
				for (size_t i = 0; i < it->second.vecShopOffers.size(); ++i)
				{
					const TPlayerBotShopOffer& offer = it->second.vecShopOffers[i];
					LPITEM item = FindPlayerBotOfferItem(keeper, offer);
					if (!item || !item->GetProto())
						continue;
					TPlayerBotStallLine line;
					line.vnum = item->GetVnum();
					line.skill = GetPlayerBotSkillBookSkillVnum(item);
					if (item->GetType() == ITEM_SKILLFORGET)
					{
						line.skill = (DWORD)item->GetSocket(0);
						line.forget = true;
					}
					line.name = GetPlayerBotStallLineName(item->GetProto(), line.skill, line.forget);
					line.link = MakePlayerBotItemLink(item, line.name.c_str());
					line.price = (long long)offer.dwPrice;
					line.count = offer.wCount ? offer.wCount : 1;
					out.lines.push_back(line);
				}
			}
			return true;
		}
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		auto shop = ikashop::GetManager().GetShopByOwnerID(pid);
		if (shop && shop->GetDuration() > 0)
		{
			out.open = true;
			out.offline = true;
			out.mapIndex = shop->GetSpawn().map;
			out.x = shop->GetSpawn().x;
			out.y = shop->GetSpawn().y;
			out.channel = shop->GetSpawn().channel;
			for (const auto& entry : shop->GetItems())
			{
				const auto& shopItem = entry.second;
				if (!shopItem)
					continue;
				const TItemTable* proto = shopItem->GetTable();
				if (!proto)
					continue;
				TPlayerBotStallLine line;
				line.vnum = shopItem->GetInfo().vnum;
				if (proto->bType == ITEM_SKILLBOOK)
					line.skill = line.vnum == 50300 ? (DWORD)shopItem->GetInfo().alSockets[0] : (DWORD)proto->alValues[0];
				else if (proto->bType == ITEM_SKILLFORGET)
				{
					line.skill = (DWORD)shopItem->GetInfo().alSockets[0];
					line.forget = true;
				}
				line.name = GetPlayerBotStallLineName(proto, line.skill, line.forget);
				line.link = FormatPlayerBotItemLink(line.vnum, proto->dwFlags, shopItem->GetInfo().alSockets,
						shopItem->GetInfo().aAttr, line.name.c_str());
				line.price = (long long)shopItem->GetPrice().yang;
				line.count = (unsigned int)shopItem->GetInfo().count;
				out.lines.push_back(line);
			}
			return true;
		}
#endif
		return false;
	}

	// A folded query against a stall line: the skill of a book ("ku aura") -
	// a skill book's or a Forgetting Book's (forget, "kz aura"), never the one
	// for the other - or the name with the players' aliases ("fms", "12d",
	// "bodzio"). A Forgetting Book answers to its own name only: its line's
	// name carries the skill, and "kupie smoczy skowyt" means the skill book.
	// A line in English (english) is matched against the official English
	// name too - "wtb full moon sword" - and a Polish one as it always was.
	bool PlayerBotStallLineMatches(const TPlayerBotStallLine& line, const std::vector<std::string>& candidates,
			bool book, bool forget, DWORD skillVnum, bool english = false)
	{
		if (book)
			return line.skill != 0 && line.skill == skillVnum && line.forget == forget;
		if (english)
		{
			const char* en = FindPlayerBotItemNameEn(line.vnum);
			if (!line.forget && line.skill)
			{
				const std::string bookEn = GetPlayerBotSkillBookNameEn(line.skill);
				if (!bookEn.empty() && playerbot_conv::ItemNameMatchesAny(playerbot_conv::FoldName(bookEn.c_str()), candidates))
					return true;
			}
			else if (en && playerbot_conv::ItemNameMatchesAny(playerbot_conv::FoldName(en), candidates))
				return true;
		}
		if (line.forget)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(line.vnum);
			return proto && playerbot_conv::ItemNameMatchesAny(playerbot_conv::FoldName(proto->szLocaleName), candidates);
		}
		return playerbot_conv::ItemNameMatchesAny(playerbot_conv::FoldName(line.name.c_str()), candidates);
	}

	// An item against a prepared query: by its official English name too for a
	// line in English, by its proto's name as ever.
	bool PlayerBotItemNameMatchesQuery(const TItemTable* proto, const std::vector<std::string>& candidates,
			bool english)
	{
		if (!proto)
			return false;
		if (english)
			if (const char* en = FindPlayerBotItemNameEn(proto->dwVnum))
				if (playerbot_conv::ItemNameMatchesAny(playerbot_conv::FoldName(en), candidates))
					return true;
		return playerbot_conv::ItemNameMatchesAny(playerbot_conv::FoldName(proto->szLocaleName), candidates);
	}

	// "ku aura miecza" / "ksiege aura miecza": the skill a book query names, or 0.
	// "kz aura miecza" / "ksiega zapomnienia aura miecza": the same for the
	// skill's Forgetting Book (forget). They are read first, or "ksiege" takes
	// "zapomnienia smoczy skowyt" for a skill book's query and finds the skill
	// inside it - "kupie ksiege zapomnienia smoczy skowyt" was answered with
	// Instr. Smoczy Skowyt (prodnathin, 25 September).
	DWORD GetPlayerBotStallBookQuery(const std::string& folded, std::string& rest, bool& forget)
	{
		rest = folded;
		forget = false;
		static const char* const kForget[] = { "kz ", "ksiega zapomnienia ", "ksiege zapomnienia ", "ksiegi zapomnienia " };
		for (size_t i = 0; i < sizeof(kForget) / sizeof(kForget[0]); ++i)
		{
			const size_t n = strlen(kForget[i]);
			if (folded.compare(0, n, kForget[i]) == 0)
			{
				forget = true;
				rest = folded.substr(n);
				return FindPlayerBotSkillByName(rest.c_str());
			}
		}
		static const char* const kBook[] = { "ku ", "ksiega ", "ksiege ", "ksiegi ", "instr " };
		for (size_t i = 0; i < sizeof(kBook) / sizeof(kBook[0]); ++i)
		{
			const size_t n = strlen(kBook[i]);
			if (folded.compare(0, n, kBook[i]) == 0)
			{
				rest = folded.substr(n);
				return FindPlayerBotSkillByName(rest.c_str());
			}
		}
		return 0;
	}

	enum EPlayerBotTradeVerb
	{
		PLAYERBOT_TRADE_NONE,
		PLAYERBOT_TRADE_BUY,
		PLAYERBOT_TRADE_SELL
	};

	// Whether the folded text at p opens with this whole word.
	bool PlayerBotTextOpensWithWord(const char* p, const char* word)
	{
		const size_t n = strlen(word);
		return strncmp(p, word, n) == 0 && (p[n] == 0 || IsPlayerBotChatSeparator(p[n]));
	}

	// "Kupię KU Aura", "sprzedam kosc niedzwiedzia", "Szukam Amuletu Orka":
	// the verb, whether a skill book is meant - or a Forgetting Book, "KZ
	// Aura" and "ksiege zapomnienia Aura" (outForget) - and the rest folded.
	// And the same in English, the words people trade in on an English
	// server: "WTB"/"buy"/"buying"/"B>", "WTS"/"sell"/"selling", a "book" or
	// "skill book" before a skill's English name, "book of forgetfulness"
	// before a Forgetting Book's (outEnglish, which the answer is said in).
	EPlayerBotTradeVerb ParsePlayerBotTradeText(const char* text, char* outQuery,
			size_t size, bool& outBook, bool& outForget, bool* outEnglish = NULL)
	{
		outQuery[0] = 0;
		outBook = false;
		outForget = false;
		if (outEnglish)
			*outEnglish = false;
		char folded[CHAT_MAX_LEN + 1];
		FoldPlayerBotChatText(text, folded, sizeof(folded));
		const char* p = folded;
		while (*p && IsPlayerBotChatSeparator(*p))
			++p;
		static const struct { const char* word; EPlayerBotTradeVerb verb; bool english; } kVerbs[] = {
			{ "kupie", PLAYERBOT_TRADE_BUY, false }, { "kupuje", PLAYERBOT_TRADE_BUY, false },
			{ "szukam", PLAYERBOT_TRADE_BUY, false }, { "potrzebuje", PLAYERBOT_TRADE_BUY, false },
			{ "sprzedam", PLAYERBOT_TRADE_SELL, false }, { "sprzedaje", PLAYERBOT_TRADE_SELL, false },
			{ "oddam", PLAYERBOT_TRADE_SELL, false }, { "s>", PLAYERBOT_TRADE_SELL, false },
			{ "k>", PLAYERBOT_TRADE_BUY, false },
			{ "wtb", PLAYERBOT_TRADE_BUY, true }, { "buy", PLAYERBOT_TRADE_BUY, true },
			{ "buying", PLAYERBOT_TRADE_BUY, true }, { "b>", PLAYERBOT_TRADE_BUY, true },
			{ "wts", PLAYERBOT_TRADE_SELL, true }, { "sell", PLAYERBOT_TRADE_SELL, true },
			{ "selling", PLAYERBOT_TRADE_SELL, true },
		};
		EPlayerBotTradeVerb verb = PLAYERBOT_TRADE_NONE;
		bool english = false;
		for (size_t i = 0; i < sizeof(kVerbs) / sizeof(kVerbs[0]); ++i)
		{
			const size_t len = strlen(kVerbs[i].word);
			if (strncmp(p, kVerbs[i].word, len) == 0 &&
					(p[len] == 0 || IsPlayerBotChatSeparator(p[len])))
			{
				verb = kVerbs[i].verb;
				english = kVerbs[i].english;
				p += len;
				break;
			}
		}
		if (verb == PLAYERBOT_TRADE_NONE)
			return verb;
		if (outEnglish)
			*outEnglish = english;
		while (*p && IsPlayerBotChatSeparator(*p))
			++p;
		// The longer words first: "book of forgetfulness" is no skill book.
		static const struct { const char* words; bool forget; } kBooksEn[] = {
			{ "book of forgetfulness", true }, { "forgetting book", true }, { "forget book", true },
			{ "skill book", false }, { "skillbook", false }, { "book", false },
		};
		bool englishBook = false;
		for (size_t i = 0; english && i < sizeof(kBooksEn) / sizeof(kBooksEn[0]) && !englishBook; ++i)
			if (PlayerBotTextOpensWithWord(p, kBooksEn[i].words))
			{
				outBook = englishBook = true;
				outForget = kBooksEn[i].forget;
				p += strlen(kBooksEn[i].words);
			}
		if (!englishBook)
		{
			if (PlayerBotTextOpensWithWord(p, "kz"))
			{
				outBook = true;
				outForget = true;
				p += 2;
			}
			else if (strncmp(p, "ku ", 3) == 0)
			{
				outBook = true;
				p += 3;
			}
			else if (strncmp(p, "ksiege ", 7) == 0 || strncmp(p, "ksiega ", 7) == 0 ||
					strncmp(p, "ksiegi ", 7) == 0)
			{
				outBook = true;
				p += 7;
				while (*p && IsPlayerBotChatSeparator(*p))
					++p;
				if (PlayerBotTextOpensWithWord(p, "zapomnienia"))
				{
					outForget = true;
					p += 11;
				}
			}
		}
		while (*p && IsPlayerBotChatSeparator(*p))
			++p;
		strlcpy(outQuery, p, size);
		size_t n = strlen(outQuery);
		while (n > 0 && IsPlayerBotChatSeparator(outQuery[n - 1]))
			outQuery[--n] = 0;
		// "Kupie KK", "Sprzedam KD": two letters are too few to search names
		// with, but a word of the players' dictionary names the item exactly.
		// "Kupie KZ" names the Forgetting Book with nothing after it.
		return n >= PLAYERBOT_TRADE_QUERY_MIN || (n > 0 && playerbot_conv::IsItemAliasWord(outQuery)) || outForget
				? verb : PLAYERBOT_TRADE_NONE;
	}

	// "Kupie X": the nearest open counter with X on it answers with where and
	// how much. The player's own map first, then any. english: the answer and
	// the item in it in English, and the query matched against the English
	// names as well.
	bool AnswerPlayerBotBuyShout(const TPlayerBotPerson& player, const char* query, bool book, bool forget,
			DWORD skillVnum, bool english)
	{
		std::vector<std::string> candidates;
		playerbot_conv::ExpandItemQuery(query ? query : "", candidates);
		LPCHARACTER bestKeeper = NULL;
		TPlayerBotStallLine bestLine;
		long bestMap = 0;
		long long bestDistance = -1;
		TPlayerBotStall stall;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER keeper = CHARACTER_MANAGER::instance().FindByPID(it->first);
			// A counter on the channel the person plays on: this core's for a
			// person here, the person's own for one who whispered from the
			// other channel (a bot here may keep its stand on the first).
			if (!keeper || !GetPlayerBotStall(it->first, keeper, stall) || stall.channel != player.channel)
				continue;
			for (size_t k = 0; k < stall.lines.size(); ++k)
			{
				const TPlayerBotStallLine& line = stall.lines[k];
				if (!PlayerBotStallLineMatches(line, candidates, book, forget, skillVnum, english))
					continue;
				// Where on the map is known of a person here only; another
				// core's person has its map, and a counter there comes first.
				long long distance = 1000000LL + (long long)stall.mapIndex;
				if (stall.mapIndex == player.mapIndex)
					distance = player.local
							? (long long)DISTANCE_APPROX(player.local->GetX() - stall.x, player.local->GetY() - stall.y)
							: 500000LL;
				if (bestDistance < 0 || distance < bestDistance)
				{
					bestDistance = distance;
					bestKeeper = keeper;
					bestLine = line;
					bestMap = stall.mapIndex;
				}
				break;
			}
		}
		if (!bestKeeper)
			return false;
		char reply[CHAT_MAX_LEN + 1];
		const std::string name = GetPlayerBotStallLineNameIn(bestLine, english);
		if (bestLine.count > 1)
			snprintf(reply, sizeof(reply), PBT(english, "Mam %s x%u na straganie w %s, %s yang za calosc",
					"I have %s x%u on my stall in %s, %s yang for all of it"),
					name.c_str(), bestLine.count, GetPlayerBotTownNameIn(bestMap, english),
					playerbot_conv::FormatYang(bestLine.price).c_str());
		else
			snprintf(reply, sizeof(reply), PBT(english, "Mam %s na straganie w %s, %s yang",
					"I have %s on my stall in %s, %s yang"),
					name.c_str(), GetPlayerBotTownNameIn(bestMap, english),
					playerbot_conv::FormatYang(bestLine.price).c_str());
		// The line shown as the client shows a linked item: the piece itself,
		// its grade and bonuses, on a click - under the name the reply says.
		std::vector<playerbot_item_link::TEntry> links(1);
		links[0].name = name;
		links[0].link = GetPlayerBotStallLineLinkIn(bestLine, english);
		SendPlayerBotWhisperTo(bestKeeper, player, LinkPlayerBotTradeReply(bestKeeper, reply, links).c_str());
		return true;
	}

	// The anti-flag that keeps a class off an item, for a weapon offered by
	// name: the proto says who may not carry it.
	DWORD GetPlayerBotJobAntiFlag(BYTE bJob)
	{
		switch (bJob)
		{
			case JOB_WARRIOR: return ITEM_ANTIFLAG_WARRIOR;
			case JOB_ASSASSIN: return ITEM_ANTIFLAG_ASSASSIN;
			case JOB_SURA: return ITEM_ANTIFLAG_SURA;
			case JOB_SHAMAN: return ITEM_ANTIFLAG_SHAMAN;
			default: return 0;
		}
	}

	// "Sprzedam X": a bot that is short of X says it will buy, and where. The
	// bot can: playerbot_market.h reads a player's counter like any other.
	// english as for the buying answer.
	bool AnswerPlayerBotSellShout(const TPlayerBotPerson& player, const char* query, bool book, bool forget,
			DWORD skillVnum, bool english)
	{
		// No bot buys a Forgetting Book off anybody (the few it reads it makes,
		// BuyPlayerBotForgetScroll), so "Sprzedam KZ Aura" has nobody to answer
		// it - where a Master of Aura used to say it would buy the skill book.
		if (forget)
			return false;
		DWORD wantedVnum = 0;
		const char* pszName = NULL;
		if (!book)
		{
			// "Sprzedam FMS" is the players' dictionary as much as "Kupie FMS".
			std::vector<std::string> candidates;
			playerbot_conv::ExpandItemQuery(query ? query : "", candidates);
			// A refine material, or a level-30 weapon: the two things a bot
			// reliably wants from anybody.
			const std::set<DWORD>& materials = GetPlayerBotRefineMaterialVnums();
			for (std::set<DWORD>::const_iterator m = materials.begin();
					m != materials.end() && wantedVnum == 0; ++m)
			{
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(*m);
				if (!proto)
					continue;
				if (PlayerBotItemNameMatchesQuery(proto, candidates, english))
				{
					wantedVnum = *m;
					pszName = proto->szLocaleName;
				}
			}
			for (DWORD vnum = 290; vnum <= 7169 && wantedVnum == 0; ++vnum)
			{
				if (!IsPlayerBotSpecialLevel30WeaponVnum(vnum))
					continue;
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
				if (!proto)
					continue;
				if (PlayerBotItemNameMatchesQuery(proto, candidates, english))
				{
					wantedVnum = vnum;
					pszName = proto->szLocaleName;
				}
			}
			if (wantedVnum == 0)
				return false;
		}

		LPCHARACTER buyer = NULL;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end() && !buyer; ++it)
		{
			LPCHARACTER bot = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!bot || !bot->IsItemLoaded() || bot->GetMyShop() || !CanPlayerBotAffordMarket(bot))
				continue;
			if (book)
			{
				if (IsPlayerBotOwnSkill(bot, skillVnum) &&
						bot->GetSkillMasterType(skillVnum) == SKILL_MASTER &&
						bot->GetSkillLevel(skillVnum) >= 20 && bot->GetSkillLevel(skillVnum) < 30)
					buyer = bot;
			}
			else if (IsPlayerBotSpecialLevel30WeaponVnum(wantedVnum))
			{
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(wantedVnum);
				if (proto && bot->GetLevel() >= 30 && !HasPlayerBotSpecialLevel30Weapon(bot, false) &&
						!IS_SET(proto->dwAntiFlags, GetPlayerBotJobAntiFlag(bot->GetJob())))
					buyer = bot;
			}
			else if (PlayerBotNeedsRefineMaterial(bot, wantedVnum))
				buyer = bot;
		}
		if (!buyer)
			return false;
		char reply[CHAT_MAX_LEN + 1];
		if (english)
		{
			// The book by its official English name, and by the Polish one's
			// words where the English client's table has none.
			std::string what = book ? GetPlayerBotSkillBookNameEn(skillVnum)
					: (wantedVnum ? GetPlayerBotItemNameIn(wantedVnum, true) : std::string(query ? query : ""));
			if (what.empty())
				what = std::string("KU ") + GetPlayerBotSkillName(skillVnum);
			snprintf(reply, sizeof(reply), "I'll buy %s - put it on a stall in Joan or Bokjung, bots buy there",
					what.c_str());
		}
		else if (book)
			snprintf(reply, sizeof(reply), "Kupie KU %s - wystaw na straganie w Joan albo Bokjung, boty tam kupuja",
					GetPlayerBotSkillName(skillVnum));
		else
			snprintf(reply, sizeof(reply), "Kupie %s - wystaw na straganie w Joan albo Bokjung, boty tam kupuja",
					pszName ? pszName : query);
		SendPlayerBotWhisperTo(buyer, player, reply);
		return true;
	}

	// ------------------------------------------------------------------
	// "Luruj" / "przestan lurowac": a person's standing order to a bot in
	// their own party.
	//
	// The Archer's luring course (playerbot_lure.h) has been a bot's own role
	// since it was written - it decides for itself when a party is worth
	// pulling for. That decision is not one the AI can make on a person's
	// behalf, so the person asks: one whisper starts the order, another ends
	// it, and while it stands the course is run for that person and the pack
	// is handed to them rather than left on the bot.
	//
	// The order itself is two fields of the bot's state, set here and read
	// there - chat_trade.h is included before lure.h, so nothing in this file
	// may call into the course, and nothing needs to: the course notices the
	// order on its next tick.
	// ------------------------------------------------------------------
	enum EPlayerBotLureOrder
	{
		PLAYERBOT_LURE_ORDER_NONE = playerbot_lure_rules::ORDER_NONE,
		PLAYERBOT_LURE_ORDER_START = playerbot_lure_rules::ORDER_START,
		PLAYERBOT_LURE_ORDER_STOP = playerbot_lure_rules::ORDER_STOP
	};

	// The words themselves are playerbot_lure_order_rules.h, which is pure and
	// unit-tested; this half is the fold from CP1250 that it expects.
	EPlayerBotLureOrder ParsePlayerBotLureOrder(const char* text)
	{
		char folded[CHAT_MAX_LEN + 1];
		FoldPlayerBotChatText(text, folded, sizeof(folded));
		switch (playerbot_lure_rules::ParseOrder(folded))
		{
			case playerbot_lure_rules::ORDER_START: return PLAYERBOT_LURE_ORDER_START;
			case playerbot_lure_rules::ORDER_STOP:  return PLAYERBOT_LURE_ORDER_STOP;
			default:                                return PLAYERBOT_LURE_ORDER_NONE;
		}
	}

	bool HandlePlayerBotLureOrder(const TPlayerBotPerson& player, LPCHARACTER bot,
			const char* text, DWORD dwNow)
	{
		const EPlayerBotLureOrder order = ParsePlayerBotLureOrder(text);
		if (order == PLAYERBOT_LURE_ORDER_NONE)
			return false;
		TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.find(bot->GetPlayerID());
		if (it == s_mapPlayerBotAIStates.end())
			return false;
		TPlayerBotAIState& state = it->second;
		char reply[CHAT_MAX_LEN + 1];
		// In the person's language, or in English for an order given in it
		// ("lure", "pull"): a person another core holds has no flag here.
		char folded[CHAT_MAX_LEN + 1];
		FoldPlayerBotChatText(text, folded, sizeof(folded));
		const bool en = player.english || strstr(folded, "lure") || strstr(folded, "luring") ||
				strstr(folded, "pull");

		if (order == PLAYERBOT_LURE_ORDER_STOP)
		{
			if (state.dwLurePlayerPID != player.pid)
				snprintf(reply, sizeof(reply), "%s", PBT(en, "Nie luruje dla ciebie", "I'm not luring for you"));
			else
			{
				sys_log(0, "PLAYERBOT_LURE: order ended pid=%u name=%s player=%s held_ms=%u",
						bot->GetPlayerID(), bot->GetName(), player.name.c_str(),
						state.dwLurePlayerTime != 0 ? dwNow - state.dwLurePlayerTime : 0);
				state.dwLurePlayerPID = 0;
				state.dwLurePlayerTime = 0;
				snprintf(reply, sizeof(reply), "%s", PBT(en, "Dobra, koncze lurowanie", "All right, I'll stop luring"));
			}
			SendPlayerBotWhisperTo(bot, player, reply);
			return true;
		}

		// Why a bot cannot take the order, in its own words. Every one of these
		// is something the person can put right in a few seconds, which is why
		// each has a sentence of its own instead of one "nie moge".
		const char* refuse = NULL;
		LPITEM weapon = bot->GetWear(WEAR_WEAPON);
		// A person on another core is on another map, or on the other channel's
		// copy of this one, and no party brings the bot across.
		if (!player.local)
			refuse = PBT(en, "Nie stoje na twojej mapie", "I'm not on your map");
		else if (!bot->GetParty() || bot->GetParty() != player.local->GetParty())
			refuse = PBT(en, "Najpierw zapros mnie do druzyny", "Invite me to your party first");
		else if (bot->GetMapIndex() != player.local->GetMapIndex())
			refuse = PBT(en, "Nie stoje na twojej mapie", "I'm not on your map");
		else if (!IsPlayerBotArcherBuild(bot))
			refuse = PBT(en, "Nie jestem lucznikiem - lurowanie robie z luku",
					"I'm no archer - I lure with a bow");
		else if (!weapon || weapon->GetType() != ITEM_WEAPON ||
				weapon->GetSubType() != WEAPON_BOW)
			refuse = PBT(en, "Nie mam teraz luku w rece", "I have no bow in my hand right now");
		// The course refuses a safe zone anyway - there is nothing there to
		// pull - but it refuses it silently, and a person who typed "luruj" in
		// a village and heard "jasne" would be waiting for something that can
		// never happen.
		else if (IsPlayerBotSafeZone(bot->GetMapIndex(), bot->GetX(), bot->GetY()))
			refuse = PBT(en, "Jestesmy w strefie bezpieczenstwa - wyjdz na lowisko i powtorz",
					"We're in a safe zone - go out to a hunting ground and ask again");
		if (refuse)
		{
			SendPlayerBotWhisperTo(bot, player, refuse);
			return true;
		}

		if (state.dwLurePlayerPID == player.pid)
		{
			// Asking again renews the order rather than restarting it: a person
			// who types it twice does not want the course in progress dropped.
			state.dwLurePlayerTime = dwNow;
			snprintf(reply, sizeof(reply), "%s", PBT(en, "Juz dla ciebie luruje", "I'm already luring for you"));
		}
		else
		{
			state.dwLurePlayerPID = player.pid;
			state.dwLurePlayerTime = dwNow;
			// Whatever the role was waiting out is not this person's wait.
			state.dwLureNextTime = 0;
			sys_log(0, "PLAYERBOT_LURE: order taken pid=%u name=%s level=%u player=%s map=%ld",
					bot->GetPlayerID(), bot->GetName(), bot->GetLevel(),
					player.name.c_str(), bot->GetMapIndex());
			snprintf(reply, sizeof(reply), "%s", PBT(en,
					"Jasne. Stoj w miejscu, przyprowadze je na ciebie. Koniec: napisz \"przestan lurowac\"",
					"Sure. Stand still, I'll bring them to you. To end it write \"stop luring\""));
		}
		SendPlayerBotWhisperTo(bot, player, reply);
		return true;
	}

	bool PlayerBotTradeReplyAllowed(DWORD personPID, DWORD dwNow)
	{
		DWORD& last = s_mapPlayerBotTradeReplyTime[personPID];
		if (last != 0 && dwNow - last < PLAYERBOT_TRADE_REPLY_INTERVAL)
			return false;
		last = dwNow;
		return true;
	}

	// A trade line, shouted or whispered: answered by the bot best placed to
	// answer it, as a shout is. True when one did.
	bool AnswerPlayerBotTradeLine(const TPlayerBotPerson& player, const char* text)
	{
		if (!player.pid || !text)
			return false;
		char query[128];
		bool book = false;
		bool forget = false;
		bool lineEnglish = false;
		const EPlayerBotTradeVerb verb = ParsePlayerBotTradeText(text, query, sizeof(query), book, forget, &lineEnglish);
		if (verb == PLAYERBOT_TRADE_NONE)
			return false;
		// The person's language, or English for a line written in it: a person
		// another core holds has no flag here, and "WTB" says enough.
		const bool english = player.english || lineEnglish;
		const DWORD skillVnum = book ? FindPlayerBotSkillByNameIn(query, lineEnglish) : 0;
		if (book && skillVnum == 0)
		{
			// "Kupie ksiege misji" names an item whose name begins with the
			// word, not a skill: it is searched as a name like any other. So
			// is a Forgetting Book with no skill after it ("Kupie KZ"). An
			// English line's words are English ones.
			std::string named = lineEnglish ? (forget ? "book of forgetfulness" : "book")
					: (forget ? "ksiega zapomnienia" : "ksiega");
			if (query[0])
			{
				named += ' ';
				named += query;
			}
			strlcpy(query, named.c_str(), sizeof(query));
			book = false;
			forget = false;
		}
		const DWORD dwNow = get_dword_time();
		if (!PlayerBotTradeReplyAllowed(player.pid, dwNow))
			return false;
		const bool answered = verb == PLAYERBOT_TRADE_BUY
				? AnswerPlayerBotBuyShout(player, query, book, forget, skillVnum, english)
				: AnswerPlayerBotSellShout(player, query, book, forget, skillVnum, english);
		sys_log(0, "PLAYERBOT_TRADE: shout from=%s verb=%s book=%d forget=%d en=%d query=\"%s\" answered=%d",
				player.name.c_str(), verb == PLAYERBOT_TRADE_BUY ? "buy" : "sell",
				book ? 1 : 0, forget ? 1 : 0, english ? 1 : 0, query, answered ? 1 : 0);
		return answered;
	}

	// A player's shout, after it has gone out on the channel.
	void HandlePlayerShoutForTrade(LPCHARACTER player, const char* text)
	{
		if (player)
			AnswerPlayerBotTradeLine(GetPlayerBotLocalPerson(player), text);
	}

	// A player's whisper to a bot. A trade line is answered like a shout, by
	// whichever bot is best placed; anything else gets the bot's own state -
	// what its counter holds, or that it is out hunting.
	// Defined in playerbot_anti_pk.h, which comes after this file.
	bool HandlePlayerBotSurrenderWhisper(LPCHARACTER player, LPCHARACTER bot, const char* text, DWORD dwNow);

	// A whisper to a bot here, from a person here or on another core.
	void AnswerPlayerBotWhisper(const TPlayerBotPerson& player, LPCHARACTER bot, const char* text)
	{
		if (!player.pid || !bot || !text)
			return;
		const DWORD dwNow = get_dword_time();
		// "Poddaje sie" first (the truce, playerbot_anti_pk.h): the lure
		// order's bare stop words are a surrender's too, and from a person the
		// bots are fighting "dosc" answered "Nie luruje dla ciebie". The truce
		// is this core's, for the bots at a person here: one on another core is
		// fought by that core's bots, and surrenders to them.
		if (player.local && HandlePlayerBotSurrenderWhisper(player.local, bot, text, dwNow))
			return;
		// Before the trade line, because an order is answered whatever the
		// reply clock says: a person who asked a bot to pull for them is owed
		// an answer, and "luruj" is nobody's idea of a trade.
		if (HandlePlayerBotLureOrder(player, bot, text, dwNow))
			return;
		char query[128];
		bool book = false;
		bool forget = false;
		const EPlayerBotTradeVerb verb = ParsePlayerBotTradeText(text, query, sizeof(query), book, forget);
		if (verb != PLAYERBOT_TRADE_NONE)
		{
			// The 8-second trade clock is for shouts - one bot to one door. A
			// whisper inside it used to vanish without a word; now this bot
			// answers it itself from its own counter and needs (conversation
			// layer, I_BUY / I_SELL), so nothing a person writes is lost.
			std::map<DWORD, DWORD>::const_iterator last =
					s_mapPlayerBotTradeReplyTime.find(player.pid);
			if (last != s_mapPlayerBotTradeReplyTime.end() && last->second != 0 &&
					dwNow - last->second < PLAYERBOT_TRADE_REPLY_INTERVAL &&
					HandlePlayerBotConversationWith(player.pid, player.name.c_str(), bot, text))
				return;
			if (AnswerPlayerBotTradeLine(player, text))
				return;
			// Nobody on this core has the thing on a counter or wants it. The
			// line used to be left without a word, as a shout nobody can answer
			// is; whispered, it is this bot's to answer from its own counter and
			// needs. A person on the other channel meets that most: that
			// channel's counters are mostly the other core's bots'.
			HandlePlayerBotConversationWith(player.pid, player.name.c_str(), bot, text);
			return;
		}
		// Ordinary conversation: analysed at once, answered from the
		// conversation queue 0.7-1.5 s later, merged when several lines come
		// together. No limiter drops anything (playerbot_conv_engine.h).
		char reply[CHAT_MAX_LEN + 1];
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(bot->GetPlayerID());
		if (HandlePlayerBotConversationWith(player.pid, player.name.c_str(), bot, text))
			return;
		const bool en = player.english;
		TPlayerBotStall stall;
		if (GetPlayerBotStall(bot->GetPlayerID(), bot, stall) && !stall.lines.empty())
		{
			std::string goods;
			std::vector<playerbot_item_link::TEntry> links;
			for (size_t k = 0; k < stall.lines.size() && k < 3; ++k)
			{
				if (!goods.empty())
					goods += ", ";
				playerbot_item_link::TEntry entry;
				entry.name = GetPlayerBotStallLineNameIn(stall.lines[k], en);
				entry.link = GetPlayerBotStallLineLinkIn(stall.lines[k], en);
				goods += entry.name;
				links.push_back(entry);
			}
			snprintf(reply, sizeof(reply), PBT(en, "Mam stragan w %s, na nim: %s", "I have a stall in %s, on it: %s"),
					GetPlayerBotTownNameIn(stall.mapIndex, en), goods.c_str());
			SendPlayerBotWhisperTo(bot, player, LinkPlayerBotTradeReply(bot, reply, links).c_str());
		}
		else if (it != s_mapPlayerBotAIStates.end() && it->second.bMarketTrip)
		{
			snprintf(reply, sizeof(reply), PBT(en, "Wlasnie ide na targ w %s", "I'm just off to the market in %s"),
					GetPlayerBotTownNameIn(bot->GetMapIndex(), en));
			SendPlayerBotWhisperTo(bot, player, reply);
		}
		else
		{
			snprintf(reply, sizeof(reply), "%s", PBT(en,
					"Nie rozumiem. Zapytaj mnie, co robie, gdzie expie albo co mam na straganie.",
					"I don't understand. Ask me what I'm doing, where I hunt or what I have on my stall."));
			SendPlayerBotWhisperTo(bot, player, reply);
		}
	}

	void HandlePlayerWhisperToBot(LPCHARACTER player, LPCHARACTER bot, const char* text)
	{
		if (player)
			AnswerPlayerBotWhisper(GetPlayerBotLocalPerson(player), bot, text);
	}

	// A whisper to a bot of this core from a person another core holds - the
	// other channel's, or the core of a map this one does not host. The
	// person's core sends it here by the P2P relay, as any whisper to somebody
	// it does not hold, and this core used to hand it to the bot's descriptor,
	// which has no client and drops every packet: "boty na innym CH nie
	// odpisuja na priv" (Derpsonkowy95, 28 September). CInputP2P::Relay hands
	// it here now (mt2009, playerbotify apply_peer_whisper_to_bot), and it is
	// answered as any whisper is - here, because the bot lives on this core
	// alone - and the answer goes back by the same relay.
	void HandlePlayerWhisperFromPeer(const char* fromName, LPCHARACTER bot, const char* text)
	{
		if (!fromName || !*fromName || !bot || !text || !*text)
			return;
		const CCI* peer = P2P_MANAGER::instance().Find(fromName);
		const char* dropped = NULL;
		if (!peer)
			dropped = "sender_gone";
		// A person's whisper only: a bot's line answered would be answered
		// back, core to core.
		else if (CPlayerBotManager::instance().IsRegisteredBotPID(peer->dwPID))
			dropped = "sender_is_bot";
		if (dropped)
		{
			sys_log(0, "PLAYERBOT_CHAT: whisper from another core dropped pid=%u name=%s from=%s reason=%s",
					bot->GetPlayerID(), bot->GetName(), fromName, dropped);
			return;
		}
		const TPlayerBotPerson person = GetPlayerBotPeerPerson(peer);
		sys_log(0, "PLAYERBOT_CHAT: whisper from another core pid=%u name=%s channel=%d map=%ld from=%s "
				"from_pid=%u from_channel=%d from_map=%ld",
				bot->GetPlayerID(), bot->GetName(), (int)g_bChannel, bot->GetMapIndex(),
				person.name.c_str(), person.pid, person.channel, person.mapIndex);
		AnswerPlayerBotWhisper(person, bot, text);
	}
}

#endif
