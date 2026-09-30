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
		// MT2009_PLUS_SHOUTERS_V1: a shouter of the first villages whispers to
		// nobody (playerbot_shouters.h).
		if (!bot || IsPlayerBotShouterPID(bot->GetPlayerID()))
			return;
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
	struct TPlayerBotPerson
	{
		DWORD pid;
		std::string name;
		long mapIndex;
		int channel;
		LPCHARACTER local;
		TPlayerBotPerson() : pid(0), mapIndex(0), channel(0), local(NULL) {}
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

	// A line on the world channel in the bot's name, within the two throttles.
	bool ShoutPlayerBotTrade(LPCHARACTER bot, const char* text, DWORD dwNow)
	{
		if (!bot || !text || !*text)
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
		SendPlayerBotShout(msg, bot->GetEmpire());
		BattlePassOnShout(bot); // MT2009_PLUS_BP_BOTS_V1: a shout for the Battle Pass
		sys_log(0, "PLAYERBOT_TRADE: shout pid=%u name=%s text=\"%s\"",
				bot->GetPlayerID(), bot->GetName(), text);
		return true;
	}

	// The counter just opened with something worth crossing town for; the
	// keeper says so. Called from the stall code with the headline item.
	void AnnouncePlayerBotStall(LPCHARACTER ch, const char* pszItemName)
	{
		if (!ch || !pszItemName || !*pszItemName)
			return;
		char text[CHAT_MAX_LEN + 1];
		snprintf(text, sizeof(text), "Sprzedam %s - stragan w %s",
				pszItemName, GetPlayerBotTownName(ch->GetMapIndex()));
		ShoutPlayerBotTrade(ch, text, get_dword_time());
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
		ShoutPlayerBotTrade(ch, text, get_dword_time());
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
	bool PlayerBotStallLineMatches(const TPlayerBotStallLine& line, const std::vector<std::string>& candidates,
			bool book, bool forget, DWORD skillVnum)
	{
		if (book)
			return line.skill != 0 && line.skill == skillVnum && line.forget == forget;
		if (line.forget)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(line.vnum);
			return proto && playerbot_conv::ItemNameMatchesAny(playerbot_conv::FoldName(proto->szLocaleName), candidates);
		}
		return playerbot_conv::ItemNameMatchesAny(playerbot_conv::FoldName(line.name.c_str()), candidates);
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
	EPlayerBotTradeVerb ParsePlayerBotTradeText(const char* text, char* outQuery,
			size_t size, bool& outBook, bool& outForget)
	{
		outQuery[0] = 0;
		outBook = false;
		outForget = false;
		char folded[CHAT_MAX_LEN + 1];
		FoldPlayerBotChatText(text, folded, sizeof(folded));
		const char* p = folded;
		while (*p && IsPlayerBotChatSeparator(*p))
			++p;
		static const struct { const char* word; EPlayerBotTradeVerb verb; } kVerbs[] = {
			{ "kupie", PLAYERBOT_TRADE_BUY }, { "kupuje", PLAYERBOT_TRADE_BUY },
			{ "szukam", PLAYERBOT_TRADE_BUY }, { "potrzebuje", PLAYERBOT_TRADE_BUY },
			{ "sprzedam", PLAYERBOT_TRADE_SELL }, { "sprzedaje", PLAYERBOT_TRADE_SELL },
			{ "oddam", PLAYERBOT_TRADE_SELL }, { "s>", PLAYERBOT_TRADE_SELL },
			{ "k>", PLAYERBOT_TRADE_BUY },
		};
		EPlayerBotTradeVerb verb = PLAYERBOT_TRADE_NONE;
		for (size_t i = 0; i < sizeof(kVerbs) / sizeof(kVerbs[0]); ++i)
		{
			const size_t len = strlen(kVerbs[i].word);
			if (strncmp(p, kVerbs[i].word, len) == 0 &&
					(p[len] == 0 || IsPlayerBotChatSeparator(p[len])))
			{
				verb = kVerbs[i].verb;
				p += len;
				break;
			}
		}
		if (verb == PLAYERBOT_TRADE_NONE)
			return verb;
		while (*p && IsPlayerBotChatSeparator(*p))
			++p;
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
	// how much. The player's own map first, then any.
	bool AnswerPlayerBotBuyShout(const TPlayerBotPerson& player, const char* query, bool book, bool forget,
			DWORD skillVnum)
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
				if (!PlayerBotStallLineMatches(line, candidates, book, forget, skillVnum))
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
		if (bestLine.count > 1)
			snprintf(reply, sizeof(reply), "Mam %s x%u na straganie w %s, %s yang za calosc",
					bestLine.name.c_str(), bestLine.count, GetPlayerBotTownName(bestMap),
					playerbot_conv::FormatYang(bestLine.price).c_str());
		else
			snprintf(reply, sizeof(reply), "Mam %s na straganie w %s, %s yang",
					bestLine.name.c_str(), GetPlayerBotTownName(bestMap),
					playerbot_conv::FormatYang(bestLine.price).c_str());
		// The line shown as the client shows a linked item: the piece itself,
		// its grade and bonuses, on a click.
		std::vector<playerbot_item_link::TEntry> links(1);
		links[0].name = bestLine.name;
		links[0].link = bestLine.link;
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
	bool AnswerPlayerBotSellShout(const TPlayerBotPerson& player, const char* query, bool book, bool forget,
			DWORD skillVnum)
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
				if (playerbot_conv::ItemNameMatchesAny(playerbot_conv::FoldName(proto->szLocaleName), candidates))
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
				if (playerbot_conv::ItemNameMatchesAny(playerbot_conv::FoldName(proto->szLocaleName), candidates))
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
		if (book)
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

		if (order == PLAYERBOT_LURE_ORDER_STOP)
		{
			if (state.dwLurePlayerPID != player.pid)
				snprintf(reply, sizeof(reply), "Nie luruje dla ciebie");
			else
			{
				sys_log(0, "PLAYERBOT_LURE: order ended pid=%u name=%s player=%s held_ms=%u",
						bot->GetPlayerID(), bot->GetName(), player.name.c_str(),
						state.dwLurePlayerTime != 0 ? dwNow - state.dwLurePlayerTime : 0);
				state.dwLurePlayerPID = 0;
				state.dwLurePlayerTime = 0;
				snprintf(reply, sizeof(reply), "Dobra, koncze lurowanie");
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
			refuse = "Nie stoje na twojej mapie";
		else if (!bot->GetParty() || bot->GetParty() != player.local->GetParty())
			refuse = "Najpierw zapros mnie do druzyny";
		else if (bot->GetMapIndex() != player.local->GetMapIndex())
			refuse = "Nie stoje na twojej mapie";
		else if (!IsPlayerBotArcherBuild(bot))
			refuse = "Nie jestem lucznikiem - lurowanie robie z luku";
		else if (!weapon || weapon->GetType() != ITEM_WEAPON ||
				weapon->GetSubType() != WEAPON_BOW)
			refuse = "Nie mam teraz luku w rece";
		// The course refuses a safe zone anyway - there is nothing there to
		// pull - but it refuses it silently, and a person who typed "luruj" in
		// a village and heard "jasne" would be waiting for something that can
		// never happen.
		else if (IsPlayerBotSafeZone(bot->GetMapIndex(), bot->GetX(), bot->GetY()))
			refuse = "Jestesmy w strefie bezpieczenstwa - wyjdz na lowisko i powtorz";
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
			snprintf(reply, sizeof(reply), "Juz dla ciebie luruje");
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
			snprintf(reply, sizeof(reply),
					"Jasne. Stoj w miejscu, przyprowadze je na ciebie. Koniec: napisz \"przestan lurowac\"");
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
		const EPlayerBotTradeVerb verb = ParsePlayerBotTradeText(text, query, sizeof(query), book, forget);
		if (verb == PLAYERBOT_TRADE_NONE)
			return false;
		const DWORD skillVnum = book ? FindPlayerBotSkillByName(query) : 0;
		if (book && skillVnum == 0)
		{
			// "Kupie ksiege misji" names an item whose name begins with the
			// word, not a skill: it is searched as a name like any other. So
			// is a Forgetting Book with no skill after it ("Kupie KZ").
			std::string named = forget ? "ksiega zapomnienia" : "ksiega";
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
				? AnswerPlayerBotBuyShout(player, query, book, forget, skillVnum)
				: AnswerPlayerBotSellShout(player, query, book, forget, skillVnum);
		sys_log(0, "PLAYERBOT_TRADE: shout from=%s verb=%s book=%d forget=%d query=\"%s\" answered=%d",
				player.name.c_str(), verb == PLAYERBOT_TRADE_BUY ? "buy" : "sell",
				book ? 1 : 0, forget ? 1 : 0, query, answered ? 1 : 0);
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

	// A person asking a bot into their guild ("chodz do mnie do gildii",
	// "chcesz do gildii?", "dolaczysz do gildii?", "dodac cie do gildii?"):
	// a word of the guild and a word of asking, in one whisper.
	// A person asking to join the bot's guild (the operator, 27 September):
	// "dodasz mnie do gildii?", "a teraz mnie dodasz do gildii?", "przyjmiesz
	// mnie do gildii", "moge dolaczyc do twojej gildii". The words in any
	// order: "gild", "mnie" and a verb of taking someone in, or one of the
	// phrases that ask for a place. Read before the invitation the other way
	// round (IsPlayerBotGuildRecruitText), which "dodaj" and "dolacz" also
	// match; "chodz do mnie do gildii" has none of these verbs.
	bool IsPlayerBotGuildJoinText(const char* text)
	{
		char folded[CHAT_MAX_LEN + 1];
		FoldPlayerBotChatText(text, folded, sizeof(folded));
		if (!strstr(folded, "gild"))
			return false;
		static const char* const phrases[] = {
			"moge dolaczyc", "moge do", "mozna dolaczyc", "mozna do", "chce dolaczyc", "chce do",
			"chcialbym dolaczyc", "chcialabym dolaczyc", "chcialbym do", "chcialabym do",
			"do twojej", "do waszej", "jest miejsce", "macie miejsce", "masz miejsce",
		};
		for (size_t i = 0; i < sizeof(phrases) / sizeof(phrases[0]); ++i)
			if (strstr(folded, phrases[i]))
				return true;
		static const char* const verbs[] = {
			"dodasz", "dodaj", "dodac", "dodal", "dodacie", "przyjmiesz", "przyjmij", "przyjac",
			"przyjal", "przyjmiecie", "zaprosisz", "zapros", "zaprosic", "wezmiesz", "wez",
			"wziac", "wezcie", "dopiszesz", "dopisz", "wpuscisz", "wpusc",
		};
		bool me = false, verb = false;
		for (char* word = folded; *word; )
		{
			while (*word && !(*word >= 'a' && *word <= 'z'))
				++word;
			char* end = word;
			while (*end >= 'a' && *end <= 'z')
				++end;
			const size_t len = end - word;
			if (len == 4 && !strncmp(word, "mnie", 4))
				me = true;
			else if (len == 3 && !strncmp(word, "mie", 3))
				me = true;
			for (size_t i = 0; !verb && i < sizeof(verbs) / sizeof(verbs[0]); ++i)
			{
				const size_t vlen = strlen(verbs[i]);
				// The word itself or the verb with its ending ("dodalbys").
				if (len >= vlen && !strncmp(word, verbs[i], vlen) && len <= vlen + 4)
					verb = true;
			}
			word = end;
		}
		return me && verb;
	}

	// The level a person needs to join a bot guild: above the average of the
	// guild's bots (the operator: "jego poziom musi byc wyzszy niz sredni poziom
	// botow w gildii"). 0 when the guild has no bot to count or the query fails.
	int GetPlayerBotGuildJoinLevel(CGuild* guild)
	{
		if (!guild)
			return 0;
		char query[512];
		snprintf(query, sizeof(query),
				"SELECT AVG(p.level) FROM player.guild_member AS gm "
				"JOIN player.player AS p ON p.id=gm.pid "
				"JOIN account.account AS a ON a.id=p.account_id "
				"WHERE gm.guild_id=%u AND BINARY a.login LIKE BINARY 'playerbot\\_%%'", guild->GetID());
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		MYSQL_ROW row = NULL;
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult ||
				!(row = mysql_fetch_row(msg->Get()->pSQLResult)) || !row[0])
			return 0;
		const double average = atof(row[0]);
		return average > 0.0 ? (int)average + 1 : 0;
	}

	// The answer to it. A member points to the master, by name; the master of
	// a bot guild says yes and invites (the engine's own window, which the
	// person accepts or not) or says what is missing: the level above the
	// bots' average, a place, the person's own guild, the kingdom.
	bool HandlePlayerBotGuildJoinWhisper(LPCHARACTER player, LPCHARACTER bot, const char* text)
	{
		if (!player || !bot || !IsPlayerBotGuildJoinText(text))
			return false;
		if (IsPlayerBotSidekickPID(bot->GetPlayerID()))
			return false;
		CGuild* mine = bot->GetGuild();
		char reply[CHAT_MAX_LEN + 1];
		if (!mine)
		{
			SendPlayerBotWhisper(bot, player, "Nie mam gildii");
			return true;
		}
		if (mine->GetMasterPID() != bot->GetPlayerID())
		{
			TGuildMember* master = mine->GetMember(mine->GetMasterPID());
			if (master && !master->name.empty())
				snprintf(reply, sizeof(reply), "Liderem jest %s, to on dodaje", master->name.c_str());
			else
				snprintf(reply, sizeof(reply), "Nie ja tu dodaje, napisz do lidera");
			SendPlayerBotWhisper(bot, player, reply);
			return true;
		}
		// The master of a person's guild is a bot only by accident; the bot
		// guilds are the ones it speaks for.
		const TPlayerBotGuildInfo* info = GetPlayerBotGuildInfo(mine);
		const int needLevel = GetPlayerBotGuildJoinLevel(mine);
		const char* refusal = NULL;
		if (player->GetGuild() == mine)
			refusal = "Przeciez juz jestes w mojej gildii";
		else if (player->GetGuild())
			refusal = "Najpierw wyjdz ze swojej gildii";
		else if (!info)
			refusal = "Nie przyjmuje nowych";
		else if (player->GetEmpire() != bot->GetEmpire())
			refusal = "Jestes z innego krolestwa, nie moge";
		else if (mine->UnderAnyWar() != 0)
			refusal = "Mamy teraz wojne, napisz pozniej";
		else if (mine->GetMemberCount() >= GetPlayerBotGuildMemberCap(mine, info->bTier))
			refusal = "Nie mam juz miejsca w gildii";
		else if (get_global_time() - player->GetQuestFlag("guild_manage.new_withdraw_time") <
				CGuildManager::instance().GetWithdrawDelay() ||
				get_global_time() - player->GetQuestFlag("guild_manage.new_disband_time") <
				CGuildManager::instance().GetDisbandDelay())
			refusal = "Niedawno odszedles z gildii, jeszcze nie moge cie dodac";
		if (refusal)
		{
			SendPlayerBotWhisper(bot, player, refusal);
			return true;
		}
		if (needLevel > 0 && player->GetLevel() < needLevel)
		{
			sys_log(0, "PLAYERBOT_GUILD: refuses a player pid=%u name=%s guild=%s player=%s level=%u need=%d",
					bot->GetPlayerID(), bot->GetName(), mine->GetName(), player->GetName(),
					(unsigned int)player->GetLevel(), needLevel);
			snprintf(reply, sizeof(reply), "Nie, nie dodam cie, musisz miec %d lvl", needLevel);
			SendPlayerBotWhisper(bot, player, reply);
			return true;
		}
		SendPlayerBotWhisper(bot, player, "Jasne, juz cie dodaje");
		sys_log(0, "PLAYERBOT_GUILD: invites a player pid=%u name=%s guild=%s player=%s level=%u need=%d",
				bot->GetPlayerID(), bot->GetName(), mine->GetName(), player->GetName(),
				(unsigned int)player->GetLevel(), needLevel);
		mine->Invite(bot, player);
		return true;
	}

	bool IsPlayerBotGuildRecruitText(const char* text)
	{
		char folded[CHAT_MAX_LEN + 1];
		FoldPlayerBotChatText(text, folded, sizeof(folded));
		if (!strstr(folded, "gild"))
			return false;
		static const char* const asks[] = {
			"chodz", "chcesz", "dolacz", "dodac", "dodam", "dodaj", "zapros", "zaprosze",
			"wbij", "przyjdz", "wstap", "przyjm", "zostan", "zapisz", "do mnie", "do mojej",
			"do nas", "moze do", "wejdz", "przejdz",
		};
		for (size_t i = 0; i < sizeof(asks) / sizeof(asks[0]); ++i)
			if (strstr(folded, asks[i]))
				return true;
		return false;
	}

	// The answer to it (the operator, 27 September). A bot with no guild says
	// yes at once; one of a bot guild below the elite leaves it for the
	// person's and says yes; the elite's members, a guild's master and a
	// member of another person's guild say no. A yes waits for the person's
	// invitation (IsPlayerBotAwaitingGuildInvite), which AcceptPlayerBotGuildInvite
	// takes as for any bot with no guild. In the person's guild it offers its
	// experience as any member does.
	bool HandlePlayerBotGuildRecruitWhisper(LPCHARACTER player, LPCHARACTER bot, const char* text)
	{
		if (!player || !bot || !IsPlayerBotGuildRecruitText(text))
			return false;
		// A companion belongs to its owner (playerbot_sidekick.h).
		if (IsPlayerBotSidekickPID(bot->GetPlayerID()))
			return false;
		CGuild* theirs = player->GetGuild();
		CGuild* mine = bot->GetGuild();
		const char* reply = NULL;
		bool leave = false;
		if (!theirs)
			reply = "Najpierw zaloz gildie";
		else if (mine == theirs)
			reply = "Przeciez juz jestem w twojej gildii";
		else if (bot->GetEmpire() != player->GetEmpire())
			reply = "Jestem z innego krolestwa, nie moge";
		else if (!theirs->GetMember(player->GetPlayerID()) ||
				!theirs->HasGradeAuth(theirs->GetMember(player->GetPlayerID())->grade, GUILD_AUTH_ADD_MEMBER))
			reply = "Nie mozesz zapraszac do tej gildii";
		else if (theirs->GetMemberCount() >= theirs->GetMaxMemberCount())
			reply = "Twoja gildia jest pelna";
		else if (mine)
		{
			const TPlayerBotGuildInfo* info = GetPlayerBotGuildInfo(mine);
			if (mine->GetMasterPID() == bot->GetPlayerID())
				reply = "Mam swoja gildie, jestem liderem";
			else if (!info)
				reply = "Jestem juz w gildii, zostaje w niej";
			else if (info->bTier == GUILD_TIER_ELITE)
				reply = "Sorry, moja gildia jest lepsza";
			else if (mine->UnderAnyWar() != 0)
				reply = "Moja gildia jest teraz na wojnie, napisz pozniej";
			else
				leave = true;
		}
		if (!reply)
		{
			if (leave)
			{
				sys_log(0, "PLAYERBOT_GUILD: leaves for a player's guild pid=%u name=%s from=%s to=%s player=%s",
						bot->GetPlayerID(), bot->GetName(), mine->GetName(), theirs->GetName(), player->GetName());
				mine->RequestRemoveMember(bot->GetPlayerID());
			}
			NotePlayerBotAwaitingGuildInvite(bot->GetPlayerID(), player->GetPlayerID());
			reply = "Dobrze, dodawaj mnie";
		}
		SendPlayerBotWhisper(bot, player, reply);
		return true;
	}

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
		TPlayerBotStall stall;
		if (GetPlayerBotStall(bot->GetPlayerID(), bot, stall) && !stall.lines.empty())
		{
			std::string goods;
			std::vector<playerbot_item_link::TEntry> links;
			for (size_t k = 0; k < stall.lines.size() && k < 3; ++k)
			{
				if (!goods.empty())
					goods += ", ";
				goods += stall.lines[k].name;
				playerbot_item_link::TEntry entry;
				entry.name = stall.lines[k].name;
				entry.link = stall.lines[k].link;
				links.push_back(entry);
			}
			snprintf(reply, sizeof(reply), "Mam stragan w %s, na nim: %s",
					GetPlayerBotTownName(stall.mapIndex), goods.c_str());
			SendPlayerBotWhisperTo(bot, player, LinkPlayerBotTradeReply(bot, reply, links).c_str());
		}
		else if (it != s_mapPlayerBotAIStates.end() && it->second.bMarketTrip)
		{
			snprintf(reply, sizeof(reply), "Wlasnie ide na targ w %s", GetPlayerBotTownName(bot->GetMapIndex()));
			SendPlayerBotWhisperTo(bot, player, reply);
		}
		else
		{
			snprintf(reply, sizeof(reply), "Nie rozumiem. Zapytaj mnie, co robie, gdzie expie albo co mam na straganie.");
			SendPlayerBotWhisperTo(bot, player, reply);
		}
	}

	void HandlePlayerWhisperToBot(LPCHARACTER player, LPCHARACTER bot, const char* text)
	{
		// Before everything else: an invitation is not a trade or a talk, and
		// a request to join is read before an invitation. A person of this
		// core only: the guild's own calls need the character here.
		if (HandlePlayerBotGuildJoinWhisper(player, bot, text))
			return;
		if (HandlePlayerBotGuildRecruitWhisper(player, bot, text))
			return;
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
