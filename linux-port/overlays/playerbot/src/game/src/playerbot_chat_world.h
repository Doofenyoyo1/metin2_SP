#ifndef __INC_METIN2_PLAYERBOT_CHAT_WORLD_H__
#define __INC_METIN2_PLAYERBOT_CHAT_WORLD_H__

// MT2009_PLUS_BOT_CHAT_V2 - the bots on the public channels.
//
// The owner, 4 October: "Zachec boty z wszystkich krolestw do uzywania czatu
// handlowego pod '@ tekst' ... Boty maja uzywac czatu handlowego, kiedy chca
// kupic lub sprzedac", and the shout channel "wiekszy, bardziej
// zroznicowany, sytuacyjny ... reagowanie na krzyki graczy - np. gracz pyta
// 'gdzie metki na 30?' i dostaje odpowiedz".
//
// Three things live here:
//
//   - THE TRADE CHAT ('@'). What '@' does on this server: a line typed after
//     "@ " in the chat (or in TAB's trade mode) is CHAT_TYPE_TRADE - every
//     kingdom reads it, level 20 and up may write, once in 30 s
//     (CInputMain::Chat); "@nick tekst" with no space is Digi Rasta's whisper
//     shortcut, the client's own (uichat.py __SendWhisperShortcut). The bots
//     of every kingdom write there when they really have something: a line
//     of their own counter with its price ("S> ..."), a piece of their bag at
//     its asking price, a material they are short of at what the market
//     pays for it ("K> ... place .../szt"), the weapon they are saving for.
//     One line on a core every TRADECHAT seconds, one bot every
//     TRADECHAT_BOT minutes (playerbot_config.h). A person's own '@' line
//     comes here through the engine (OnPlayerTradeChat, server-patches
//     playerqol) and is answered by whisper by the bot best placed to - the
//     shout's old answer (AnswerPlayerBotTradeLine).
//
//   - THE SHOUT CHANNEL'S QUESTIONS. A person's shout read by the
//     conversation's own analyser: "gdzie metki na 30?", "gdzie expic na
//     45?", "ile stoi fms?", "siema wszystkim" - answered by a bot of the
//     kingdom a few seconds later, in the shout, by name. And the bots'
//     own questions (the chatter's "gdzie najlepiej expic na 40 lvl?") are
//     answered by another bot now and then, so the channel talks to itself.
//     SHOUT_ANSWER percent of them, at most one answer a kingdom every
//     PLAYERBOT_SHOUT_ANSWER_GAP_MS.
//
//   - THE ADVICE. Where to exp at a level (the progression table's maps,
//     the operator's own) and where the Metins of a level stand (the Battle
//     Pass bots' stone table, read off the maps' stone.txt) - for these
//     answers and for the whisper's (IConvWorld::ExpPlaceFor/MetinPlaceFor).
//
// An implementation fragment: include it once, after playerbot_bpbots.h and
// playerbot_shouters.h.

namespace
{
	const DWORD PLAYERBOT_TRADECHAT_FIRST_DELAY_MS = 40 * 1000;
	const int PLAYERBOT_TRADECHAT_PICK_TRIES = 6;
	const DWORD PLAYERBOT_SHOUT_ANSWER_GAP_MS = 6000;
	const DWORD PLAYERBOT_SHOUT_ANSWER_MIN_DELAY_MS = 3500;
	const DWORD PLAYERBOT_SHOUT_ANSWER_SPREAD_MS = 7000;
	const DWORD PLAYERBOT_SHOUT_QUESTION_TTL_MS = 30000;
	const size_t PLAYERBOT_SHOUT_QUESTIONS_MAX = 16;
	// The bots' own questions are answered this often (percent of
	// SHOUT_ANSWER), so the channel is not a quiz.
	const int PLAYERBOT_SHOUT_BOT_QUESTION_PERCENT = 45;

	// ------------------------------------------------------------ advice

	const char* PlayerBotAdvicePlace(long mapIndex)
	{
		const playerbot_conv::TMapWords& w = playerbot_conv::GetMapWords(mapIndex);
		return w.index ? w.at : "";
	}

	long PlayerBotProgressionRowMap(int row, int empire)
	{
		using namespace playerbot_progression;
		static const long kM2[4] = { 23, 3, 23, 43 };
		switch (row)
		{
			case MAP_M2: return kM2[empire >= 1 && empire <= 3 ? empire : 0];
			case MAP_ISLANDS: case MAP_ORC_VALLEY: return PLAYERBOT_MAP_ORC_VALLEY;
			case MAP_DESERT: return PLAYERBOT_MAP_DESERT;
			case MAP_SOHAN: return PLAYERBOT_MAP_SOHAN;
			case MAP_SPIDER1: return PLAYERBOT_MAP_SPIDER_V1;
			case MAP_HWANG: return PLAYERBOT_MAP_HWANG;
			case MAP_SPIDER2: return PLAYERBOT_MAP_SPIDER_V2;
			case MAP_DEMON_TOWER: return PLAYERBOT_MAP_DEMON_TOWER;
			case MAP_FOREST: return PLAYERBOT_MAP_FOREST;
			case MAP_FIRE_LAND: return PLAYERBOT_MAP_FIRE_LAND;
			case MAP_RED_FOREST: return PLAYERBOT_MAP_RED_FOREST;
			default: return 0;
		}
	}

	// "gdzie expic na 45?": the maps the operator's progression table opens
	// for that level, the two that opened last - which is where the bots of
	// that level hunt. Below the second village, the first.
	bool DescribePlayerBotExpPlace(int level, int empire, std::string& out)
	{
		using namespace playerbot_progression;
		out.clear();
		if (level <= 0)
			return false;
		if (level < MapFrom(MAP_M2))
		{
			out = level < 10 ? "na M1, przy samej wiosce" : "na M1, dalej od wioski";
			return true;
		}
		int best[2] = { -1, -1 };
		for (int row = 0; row < MAP_ROW_COUNT; ++row)
		{
			// The Demon Tower is a climb, not a spot; the grottoes have their own way in.
			if (row == MAP_DEMON_TOWER || row == MAP_GROTTO1 || row == MAP_GROTTO2)
				continue;
			const int from = MapFrom(row);
			const int to = MAP_DEFAULTS[row].hasTo ? MapTo(row) : 255;
			if (level < from || level > to)
				continue;
			if (best[0] < 0 || from > MapFrom(best[0]))
			{
				best[1] = best[0];
				best[0] = row;
			}
			else if (best[1] < 0 || from > MapFrom(best[1]))
				best[1] = row;
		}
		std::vector<std::string> names;
		for (int i = 0; i < 2; ++i)
		{
			if (best[i] < 0)
				continue;
			const long map = PlayerBotProgressionRowMap(best[i], empire);
			std::string name = best[i] == MAP_M2 ? std::string("na M2") : std::string(PlayerBotAdvicePlace(map));
			if (best[i] == MAP_ISLANDS)
				name = "na wyspach w Dolinie Orkow";
			if (!name.empty() && std::find(names.begin(), names.end(), name) == names.end())
				names.push_back(name);
		}
		if (names.empty())
			return false;
		out = names[0];
		if (names.size() > 1)
			out += " albo " + names[1];
		return true;
	}

	// "gdzie metki na 30?": the maps where a stone of that level (or the
	// nearest level that has one) stands - the Battle Pass bots' table.
	bool DescribePlayerBotMetinPlace(int level, int empire, std::string& out)
	{
		out.clear();
		if (level <= 0)
			return false;
		int bestGap = 1000;
		for (size_t i = 0; i < playerbot_bpbots::STONE_SPAWN_COUNT; ++i)
		{
			const int stone = playerbot_bpbots::StoneLevel(playerbot_bpbots::STONE_SPAWNS[i].vnum);
			if (stone <= 0)
				continue;
			const int gap = stone > level ? stone - level : level - stone;
			if (gap < bestGap)
				bestGap = gap;
		}
		if (bestGap > 10)
			return false;
		std::vector<std::string> names;
		for (size_t i = 0; i < playerbot_bpbots::STONE_SPAWN_COUNT && names.size() < 3; ++i)
		{
			const playerbot_bpbots::TStoneSpawn& s = playerbot_bpbots::STONE_SPAWNS[i];
			const int stone = playerbot_bpbots::StoneLevel(s.vnum);
			const int gap = stone > level ? stone - level : level - stone;
			if (stone <= 0 || gap != bestGap)
				continue;
			std::vector<std::string> here;
			if (s.m1)
				here.push_back("na M1");
			if (s.m2)
				here.push_back("na M2");
			for (int k = 0; k < 2; ++k)
				if (s.shared[k])
					here.push_back(PlayerBotAdvicePlace(s.shared[k]));
			for (size_t k = 0; k < here.size(); ++k)
				if (!here[k].empty() && std::find(names.begin(), names.end(), here[k]) == names.end())
					names.push_back(here[k]);
		}
		(void)empire;
		if (names.empty())
			return false;
		out = names[0];
		for (size_t i = 1; i < names.size(); ++i)
			out += (i + 1 == names.size() ? " albo " : ", ") + names[i];
		return true;
	}

	// ------------------------------------------------------- the speakers

	// A bot that may speak on a public channel now.
	bool IsPlayerBotPublicSpeaker(LPCHARACTER c, int minLevel)
	{
		if (!c || c->IsDead() || !c->GetDesc() || !c->GetDesc()->IsBot() || (int)c->GetLevel() < minLevel)
			return false;
		const DWORD pid = c->GetPlayerID();
		return !IsPlayerBotShouterPID(pid) && !IsPlayerBotMedalShouterPID(pid) && !IsPlayerBotSidekickPID(pid) &&
				!IsPlayerBotOnMercContract(pid);
	}

	// ------------------------------------------------------- the trade chat

	// What a trade line means, for the bot's memory of its own posts.
	struct TPlayerBotTradeMeaning
	{
		BYTE kind;
		DWORD vnum;
		int count;
		DWORD unit;
		std::string name;
		DWORD skill;        // a skill book's skill (socket 0)
		// The second item of a two-item "S> A - x, B - y" line, noted as a
		// post of its own: "kupie ku czarowane ostrze" answers it too.
		DWORD vnum2;
		int count2;
		DWORD unit2;
		std::string name2;
		DWORD skill2;
		TPlayerBotTradeMeaning() : kind(0), vnum(0), count(0), unit(0), skill(0), vnum2(0), count2(0), unit2(0), skill2(0) {}
	};
	TPlayerBotTradeMeaning s_PlayerBotTradeMeaning;

	DWORD s_dwPlayerBotTradeChatNext = 0;
	std::map<DWORD, DWORD> s_mapPlayerBotTradeChatLast; // by bot pid
	unsigned int s_uPlayerBotTradeChatLines = 0;

	template <size_t N>
	const char* PickPlayerBotChatLine(const char* const (&pool)[N])
	{
		return pool[number(0, (int)N - 1)];
	}

	// What a bot pays for a material, as players write a price: what the
	// market has been paying (the sale memory) or the cheapest counter, a
	// little under. 0 when nobody knows.
	DWORD GetPlayerBotWantedUnitPrice(DWORD vnum, DWORD dwNow)
	{
		size_t samples = 0;
		DWORD unit = GetPlayerBotSaleUnitPrice(vnum, 0, dwNow, &samples);
		if (unit == 0)
		{
			TPlayerBotStall stall;
			for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
					it != s_mapPlayerBotAIStates.end(); ++it)
			{
				LPCHARACTER keeper = CHARACTER_MANAGER::instance().FindByPID(it->first);
				if (!keeper || !GetPlayerBotStall(it->first, keeper, stall))
					continue;
				for (size_t i = 0; i < stall.lines.size(); ++i)
				{
					if (stall.lines[i].vnum != vnum || stall.lines[i].price <= 0)
						continue;
					const DWORD per = (DWORD)(stall.lines[i].price / (stall.lines[i].count ? stall.lines[i].count : 1));
					if (unit == 0 || per < unit)
						unit = per;
				}
			}
		}
		if (unit == 0)
			return 0;
		unit = (DWORD)((unsigned long long)unit * (85 + number(0, 10)) / 100);
		return HumanizePlayerBotPrice(unit, false);
	}

	// "S> ..." from the bot's own counter: one or two of its lines, the
	// dearest first, with their prices and where the counter stands.
	bool BuildPlayerBotTradeSellFromStall(LPCHARACTER ch, std::string& out)
	{
		TPlayerBotStall stall;
		if (!GetPlayerBotStall(ch->GetPlayerID(), ch, stall) || stall.lines.empty())
			return false;
		std::vector<size_t> order;
		for (size_t i = 0; i < stall.lines.size(); ++i)
			if (stall.lines[i].price > 0)
				order.push_back(i);
		if (order.empty())
			return false;
		// Two of the dearest four, in a random pair: not the same headline every time.
		std::sort(order.begin(), order.end(), [&stall](size_t a, size_t b) { return stall.lines[a].price > stall.lines[b].price; });
		if (order.size() > 4)
			order.resize(4);
		for (size_t i = order.size(); i > 1; --i)
			std::swap(order[i - 1], order[number(0, (int)i - 1)]);
		const TPlayerBotStallLine& a = stall.lines[order[0]];
		s_PlayerBotTradeMeaning = TPlayerBotTradeMeaning();
		s_PlayerBotTradeMeaning.kind = playerbot_conv::PL_SELL;
		s_PlayerBotTradeMeaning.vnum = a.vnum;
		s_PlayerBotTradeMeaning.count = (int)a.count;
		s_PlayerBotTradeMeaning.unit = (DWORD)(a.price / (a.count ? a.count : 1));
		s_PlayerBotTradeMeaning.name = a.name;
		s_PlayerBotTradeMeaning.skill = a.forget ? 0 : a.skill;
		const char* town = GetPlayerBotTownName(stall.mapIndex >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN
				? stall.mapIndex / 10000 : stall.mapIndex);
		char buf[CHAT_MAX_LEN + 1];
		std::string first = a.name;
		if (a.count > 1)
			first += " x" + playerbot_conv::ToString((long long)a.count);
		if (order.size() > 1 && number(0, 1) == 0)
		{
			const TPlayerBotStallLine& b = stall.lines[order[1]];
			s_PlayerBotTradeMeaning.vnum2 = b.vnum;
			s_PlayerBotTradeMeaning.count2 = (int)b.count;
			s_PlayerBotTradeMeaning.unit2 = (DWORD)(b.price / (b.count ? b.count : 1));
			s_PlayerBotTradeMeaning.name2 = b.name;
			s_PlayerBotTradeMeaning.skill2 = b.forget ? 0 : b.skill;
			std::string second = b.name;
			if (b.count > 1)
				second += " x" + playerbot_conv::ToString((long long)b.count);
			snprintf(buf, sizeof(buf), "S> %s - %s, %s - %s | stragan %s ch%d", first.c_str(),
					playerbot_conv::FormatYang(a.price).c_str(), second.c_str(), playerbot_conv::FormatYang(b.price).c_str(),
					town, stall.channel ? stall.channel : (int)g_bChannel);
		}
		else
		{
			static const char* const kOne[] = {
				"S> %s - %s, stragan %s ch%d", "Sprzedam %s za %s, stoi w %s na ch%d", "S> %s %s, wpadaj na stragan %s ch%d",
				"S> %s, cena %s, stragan w %s (ch%d) albo pw" };
			snprintf(buf, sizeof(buf), PickPlayerBotChatLine(kOne), first.c_str(), playerbot_conv::FormatYang(a.price).c_str(),
					town, stall.channel ? stall.channel : (int)g_bChannel);
		}
		out = buf;
		// The headline item shown as a player's Alt-click shows it - a link
		// with its grade and bonuses on a click - while the client's line
		// has room for it (the name in the trade chat's own colour frame).
		if (!a.link.empty())
		{
			std::vector<playerbot_item_link::TEntry> links(1);
			links[0].name = a.name;
			links[0].link = a.link;
			const size_t frame = 32 + 2 * strlen(ch->GetName());
			const size_t room = playerbot_item_link::WHISPER_LINE_MAX > frame ? playerbot_item_link::WHISPER_LINE_MAX - frame : 0;
			out = playerbot_item_link::Substitute(out, links, room);
		}
		return true;
	}

	// "S> ..." from the bag, at the asking price, when there is no counter.
	bool BuildPlayerBotTradeSellFromBag(LPCHARACTER ch, std::string& out)
	{
		if (!ch->IsItemLoaded())
			return false;
		std::vector<LPITEM> goods;
		for (WORD cell = 0; cell < PLAYERBOT_BAG_CELLS; ++cell)
		{
			LPITEM item = ch->GetInventoryItem(cell);
			if (!item || item->IsEquipped() || !item->GetProto() || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE) ||
					IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_MYSHOP))
				continue;
			const BYTE type = item->GetType();
			if (((type == ITEM_WEAPON || type == ITEM_ARMOR) && item->GetRefineLevel() >= 4) || type == ITEM_SKILLBOOK ||
					type == ITEM_METIN || (type == ITEM_MATERIAL && item->GetCount() >= 3))
				goods.push_back(item);
		}
		if (goods.empty())
			return false;
		LPITEM item = goods[number(0, (int)goods.size() - 1)];
		const DWORD price = GetPlayerBotShopAskingPrice(item);
		if (price == 0)
			return false;
		s_PlayerBotTradeMeaning = TPlayerBotTradeMeaning();
		s_PlayerBotTradeMeaning.kind = playerbot_conv::PL_SELL;
		s_PlayerBotTradeMeaning.vnum = item->GetVnum();
		s_PlayerBotTradeMeaning.count = (int)item->GetCount();
		s_PlayerBotTradeMeaning.unit = price / (item->GetCount() ? item->GetCount() : 1);
		// A skill book says which skill it teaches: "Ksiega Umiejetnosci" alone
		// told nobody what was for sale (the owner, 4 October).
		std::string name = item->GetProto()->szLocaleName;
		if (item->GetType() == ITEM_SKILLBOOK && GetPlayerBotSkillBookSkillVnum(item) > 0)
		{
			const char* skill = GetPlayerBotSkillName(GetPlayerBotSkillBookSkillVnum(item));
			if (skill && strcmp(skill, "?") != 0)
			{
				name = std::string("KU ") + skill;
				s_PlayerBotTradeMeaning.skill = GetPlayerBotSkillBookSkillVnum(item);
			}
		}
		s_PlayerBotTradeMeaning.name = name;
		if (item->GetCount() > 1)
			name += " x" + playerbot_conv::ToString((long long)item->GetCount());
		static const char* const k[] = { "S> %s - %s, pw", "Sprzedam %s, %s, pisz pw", "S> %s %s do negocjacji, pw",
			"Mam na sprzedaz %s, %s, jestem w %s" };
		char buf[CHAT_MAX_LEN + 1];
		const int pick = number(0, 3);
		if (pick == 3)
			snprintf(buf, sizeof(buf), k[3], name.c_str(), playerbot_conv::FormatYang(price).c_str(),
					playerbot_bpbots::PlaceName(ch->GetMapIndex()));
		else
			snprintf(buf, sizeof(buf), k[pick], name.c_str(), playerbot_conv::FormatYang(price).c_str());
		out = buf;
		return true;
	}

	// "K> ..." for a material the bot is short of, at what the market pays.
	bool BuildPlayerBotTradeBuy(LPCHARACTER ch, DWORD dwNow, std::string& out)
	{
		std::set<DWORD> wanted;
		CollectPlayerBotWantedMaterials(ch, wanted);
		if (wanted.empty())
			return false;
		std::set<DWORD>::const_iterator it = wanted.begin();
		std::advance(it, number(0, (int)wanted.size() - 1));
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(*it);
		if (!proto)
			return false;
		const DWORD unit = GetPlayerBotWantedUnitPrice(*it, dwNow);
		const bool stack = IS_SET(proto->dwFlags, ITEM_FLAG_STACKABLE);
		const int count = stack ? (number(0, 2) == 0 ? 5 : (number(0, 1) ? 10 : 20)) : 1;
		s_PlayerBotTradeMeaning = TPlayerBotTradeMeaning();
		s_PlayerBotTradeMeaning.kind = playerbot_conv::PL_BUY;
		s_PlayerBotTradeMeaning.vnum = *it;
		s_PlayerBotTradeMeaning.count = count;
		s_PlayerBotTradeMeaning.unit = unit;
		s_PlayerBotTradeMeaning.name = proto->szLocaleName;
		char buf[CHAT_MAX_LEN + 1];
		if (unit > 0)
		{
			static const char* const k[] = { "K> %s%s, place %s/szt, pw", "B> %s%s, daje %s za sztuke", "Kupie %s%s po %s, pw",
				"K> %s%s - %s/szt, kto ma?" };
			snprintf(buf, sizeof(buf), PickPlayerBotChatLine(k), proto->szLocaleName,
					count > 1 ? (" x" + playerbot_conv::ToString((long long)count)).c_str() : "",
					playerbot_conv::FormatYang(unit).c_str());
		}
		else
		{
			static const char* const k[] = { "K> %s%s, oferty pw", "Kupie %s%s, dobrze place, pw", "B> %s%s, kto ma? pw" };
			snprintf(buf, sizeof(buf), PickPlayerBotChatLine(k), proto->szLocaleName,
					count > 1 ? (" x" + playerbot_conv::ToString((long long)count)).c_str() : "");
		}
		out = buf;
		return true;
	}

	// "K> ..." for the weapon the bot is saving for (playerbot_weapon_goal.h).
	bool BuildPlayerBotTradeWeaponWant(LPCHARACTER ch, std::string& out)
	{
		std::map<DWORD, TPlayerBotWeaponGoal>::const_iterator known = s_mapPlayerBotWeaponGoals.find(ch->GetPlayerID());
		if (known == s_mapPlayerBotWeaponGoals.end() || !known->second.family || known->second.when == 0)
			return false;
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(known->second.family->dwBaseVnum);
		if (!proto)
			return false;
		const long long price = (long long)GetPlayerBotWeaponGoalPrice(known->second.family);
		if (price <= 0 || (long long)ch->GetGold() < price / 2)
			return false;
		const std::string name = playerbot_conv::GearName(proto->szLocaleName, 0);
		s_PlayerBotTradeMeaning = TPlayerBotTradeMeaning();
		s_PlayerBotTradeMeaning.kind = playerbot_conv::PL_BUY;
		s_PlayerBotTradeMeaning.vnum = proto->dwVnum;
		s_PlayerBotTradeMeaning.count = 1;
		s_PlayerBotTradeMeaning.unit = (DWORD)price;
		s_PlayerBotTradeMeaning.name = proto->szLocaleName;
		static const char* const k[] = { "K> %s, dam do %s, pw", "Kupie %s, mam %s, kto sprzeda?", "B> %s za %s, moze byc z plusem" };
		char buf[CHAT_MAX_LEN + 1];
		snprintf(buf, sizeof(buf), PickPlayerBotChatLine(k), name.c_str(), playerbot_conv::FormatYang(price).c_str());
		out = buf;
		return true;
	}

	// One bot's trade line, of what it really has or wants.
	bool BuildPlayerBotTradeChatLine(LPCHARACTER ch, DWORD dwNow, std::string& out)
	{
		const int roll = number(1, 100);
		if (roll <= 45 && BuildPlayerBotTradeSellFromStall(ch, out))
			return true;
		if (roll <= 75 && BuildPlayerBotTradeBuy(ch, dwNow, out))
			return true;
		if (roll <= 88 && BuildPlayerBotTradeWeaponWant(ch, out))
			return true;
		return BuildPlayerBotTradeSellFromStall(ch, out) || BuildPlayerBotTradeSellFromBag(ch, out) ||
				BuildPlayerBotTradeBuy(ch, dwNow, out);
	}

	void ManagePlayerBotTradeChat(DWORD dwNow)
	{
		if (!IsPlayerBotTradeChatOn())
			return;
		const DWORD interval = (DWORD)GetPlayerBotTradeChatSeconds() * 1000;
		if (s_dwPlayerBotTradeChatNext == 0)
		{
			s_dwPlayerBotTradeChatNext = dwNow + PLAYERBOT_TRADECHAT_FIRST_DELAY_MS + number(0, (int)interval);
			return;
		}
		if ((int)(dwNow - s_dwPlayerBotTradeChatNext) < 0)
			return;
		// A little uneven, as people are: 70..130% of the interval.
		s_dwPlayerBotTradeChatNext = dwNow + interval * (70 + number(0, 60)) / 100;
		const DWORD botGap = (DWORD)GetPlayerBotTradeChatBotMinutes() * 60 * 1000;
		for (int attempt = 0; attempt < PLAYERBOT_TRADECHAT_PICK_TRIES; ++attempt)
		{
			// A bot drawn from all of this core's, every kingdom alike.
			LPCHARACTER pick = NULL;
			int seen = 0;
			for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
					it != s_mapPlayerBotAIStates.end(); ++it)
			{
				std::map<DWORD, DWORD>::const_iterator last = s_mapPlayerBotTradeChatLast.find(it->first);
				if (last != s_mapPlayerBotTradeChatLast.end() && dwNow - last->second < botGap)
					continue;
				LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(it->first);
				if (!IsPlayerBotPublicSpeaker(c, PLAYERBOT_TRADECHAT_MIN_LEVEL))
					continue;
				if (number(1, ++seen) == 1)
					pick = c;
			}
			if (!pick)
				return;
			std::string line;
			s_mapPlayerBotTradeChatLast[pick->GetPlayerID()] = dwNow;
			if (!BuildPlayerBotTradeChatLine(pick, dwNow, line) || line.empty())
				continue;
			SendPlayerBotTradeChat(pick, line.c_str());
			// The second item first, so the headline is the newest post - the
			// one a bare "to" / "ten" refers to.
			const TPlayerBotTradeMeaning& m = s_PlayerBotTradeMeaning;
			if (m.vnum2)
				NotePlayerBotPublicLine(pick, m.kind, true, line.c_str(), m.vnum2, m.count2, m.unit2, 0, 0, m.name2.c_str(), m.skill2);
			NotePlayerBotPublicLine(pick, m.kind, true, line.c_str(), m.vnum, m.count, m.unit, 0, 0, m.name.c_str(), m.skill);
			// The kingdom's trade shout keeps its distance from it.
			s_dwPlayerBotTradeShoutTime = dwNow;
			++s_uPlayerBotTradeChatLines;
			return;
		}
	}

	// ----------------------------------------- the shout channel's questions

	enum EPlayerBotShoutQuestion
	{
		SHOUT_Q_NONE = 0,
		SHOUT_Q_METIN,
		SHOUT_Q_EXP,
		SHOUT_Q_PRICE,
		SHOUT_Q_HELLO,
		SHOUT_Q_PARTY,
	};

	struct TPlayerBotShoutQuestion
	{
		BYTE bEmpire;
		BYTE bKind;
		int iLevel;
		DWORD dwAskerPID;
		std::string asker;
		std::string object;
		DWORD dwDue;
		DWORD dwAskedAt;
		TPlayerBotShoutQuestion() : bEmpire(0), bKind(SHOUT_Q_NONE), iLevel(0), dwAskerPID(0), dwDue(0), dwAskedAt(0) {}
	};

	std::vector<TPlayerBotShoutQuestion> s_vecPlayerBotShoutQuestions;
	DWORD s_adwPlayerBotShoutAnswerAt[4] = { 0, 0, 0, 0 };
	unsigned int s_uPlayerBotShoutAnswers = 0;

	// A question for a bot of the kingdom to answer on the shout channel.
	// `byBot`: one of the bots' own (answered less often).
	void EnqueuePlayerBotShoutQuestion(BYTE empire, BYTE kind, int level, DWORD askerPID, const char* asker,
			const std::string& object, bool byBot)
	{
		if (empire < 1 || empire > 3 || kind == SHOUT_Q_NONE || !asker)
			return;
		const int percent = GetPlayerBotShoutAnswerPercent();
		if (percent <= 0 || number(1, 100) > (byBot ? percent * PLAYERBOT_SHOUT_BOT_QUESTION_PERCENT / 100 : percent))
			return;
		if (s_vecPlayerBotShoutQuestions.size() >= PLAYERBOT_SHOUT_QUESTIONS_MAX)
			return;
		const DWORD dwNow = get_dword_time();
		TPlayerBotShoutQuestion q;
		q.bEmpire = empire;
		q.bKind = kind;
		q.iLevel = level;
		q.dwAskerPID = askerPID;
		q.asker = asker;
		q.object = object;
		q.dwAskedAt = dwNow;
		q.dwDue = dwNow + PLAYERBOT_SHOUT_ANSWER_MIN_DELAY_MS + number(0, (int)PLAYERBOT_SHOUT_ANSWER_SPREAD_MS);
		s_vecPlayerBotShoutQuestions.push_back(q);
	}

	// A person's shout, read for a question a bot can answer.
	void ReadPlayerShoutForQuestion(LPCHARACTER ch, const char* text)
	{
		if (!ch || !text || !*text)
			return;
		playerbot_conv::TAnalysis a;
		playerbot_conv::AnalyzeLine(text, a, get_dword_time());
		BYTE kind = SHOUT_Q_NONE;
		int level = a.levelAsked > 0 ? a.levelAsked : (int)ch->GetLevel();
		std::string object;
		switch (a.intent)
		{
			case playerbot_conv::I_WHERE_METIN: kind = SHOUT_Q_METIN; break;
			case playerbot_conv::I_WHERE_EXP: kind = SHOUT_Q_EXP; break;
			case playerbot_conv::I_PRICE:
				if (!a.object.empty())
				{
					kind = SHOUT_Q_PRICE;
					object = a.object;
				}
				break;
			case playerbot_conv::I_GREETING:
				// "siema wszystkim" - now and then a bot says it back.
				if (a.tokens.words.size() <= 4 && number(1, 100) <= 35)
					kind = SHOUT_Q_HELLO;
				break;
			default:
				break;
		}
		if (kind == SHOUT_Q_NONE)
			return;
		EnqueuePlayerBotShoutQuestion(ch->GetEmpire(), kind, level, ch->GetPlayerID(), ch->GetName(), object, false);
	}

	// The answer's words. `who` is the asker, named the way the channel does.
	bool ComposePlayerBotShoutAnswer(LPCHARACTER bot, const TPlayerBotShoutQuestion& q, std::string& out)
	{
		char buf[CHAT_MAX_LEN + 1];
		switch (q.bKind)
		{
			case SHOUT_Q_METIN:
			{
				std::string place;
				if (!DescribePlayerBotMetinPlace(q.iLevel, q.bEmpire, place))
					return false;
				static const char* const k[] = { "%s metki na %d masz %s", "%s, na %d lvl metiny stoja %s",
					"%s %s, tam sie respia na %d" };
				const int pick = number(0, 2);
				if (pick == 2)
					snprintf(buf, sizeof(buf), k[2], q.asker.c_str(), place.c_str(), q.iLevel);
				else
					snprintf(buf, sizeof(buf), k[pick], q.asker.c_str(), q.iLevel, place.c_str());
				break;
			}
			case SHOUT_Q_EXP:
			{
				std::string place;
				if (!DescribePlayerBotExpPlace(q.iLevel, q.bEmpire, place))
					return false;
				static const char* const k[] = { "%s na %d najlepiej %s", "%s, ja na %d expilem %s", "%s %d lvl? %s" };
				snprintf(buf, sizeof(buf), PickPlayerBotChatLine(k), q.asker.c_str(), q.iLevel, place.c_str());
				break;
			}
			case SHOUT_Q_PRICE:
			{
				std::string name;
				long long price = 0;
				unsigned int sellers = 0;
				CPlayerBotConvWorld world;
				world.Bind(bot, NULL, g_bChannel);
				if (!world.FindMarketPrice(q.object, name, price, sellers) || price <= 0)
					return false;
				static const char* const k[] = { "%s %s chodzi ok %s", "%s, %s stoi teraz po %s", "%s na straganach %s jest po %s" };
				snprintf(buf, sizeof(buf), PickPlayerBotChatLine(k), q.asker.c_str(), name.c_str(),
						playerbot_conv::FormatYang(price).c_str());
				break;
			}
			case SHOUT_Q_HELLO:
			{
				static const char* const k[] = { "siema %s", "elo %s", "hej %s :)", "siemka", "witam witam" };
				const char* line = PickPlayerBotChatLine(k);
				if (strstr(line, "%s"))
					snprintf(buf, sizeof(buf), line, q.asker.c_str());
				else
					snprintf(buf, sizeof(buf), "%s", line);
				break;
			}
			default:
				return false;
		}
		out = buf;
		return true;
	}

	// The due questions answered, one a kingdom at a time.
	void ManagePlayerBotShoutAnswers(DWORD dwNow)
	{
		for (size_t i = 0; i < s_vecPlayerBotShoutQuestions.size(); )
		{
			TPlayerBotShoutQuestion& q = s_vecPlayerBotShoutQuestions[i];
			if (dwNow - q.dwAskedAt > PLAYERBOT_SHOUT_QUESTION_TTL_MS)
			{
				s_vecPlayerBotShoutQuestions.erase(s_vecPlayerBotShoutQuestions.begin() + i);
				continue;
			}
			if ((int)(dwNow - q.dwDue) < 0 || (s_adwPlayerBotShoutAnswerAt[q.bEmpire] != 0 &&
					dwNow - s_adwPlayerBotShoutAnswerAt[q.bEmpire] < PLAYERBOT_SHOUT_ANSWER_GAP_MS))
			{
				++i;
				continue;
			}
			// A bot of the asker's kingdom on this core, any but the asker.
			LPCHARACTER pick = NULL;
			int seen = 0;
			for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
					it != s_mapPlayerBotAIStates.end(); ++it)
			{
				if (it->first == q.dwAskerPID)
					continue;
				LPCHARACTER c = CHARACTER_MANAGER::instance().FindByPID(it->first);
				if (!c || c->GetEmpire() != q.bEmpire || !IsPlayerBotPublicSpeaker(c, g_iShoutLimitLevel))
					continue;
				// Advice about a level is given by somebody who has been there.
				if ((q.bKind == SHOUT_Q_METIN || q.bKind == SHOUT_Q_EXP) && (int)c->GetLevel() < q.iLevel)
					continue;
				if (number(1, ++seen) == 1)
					pick = c;
			}
			std::string text;
			if (pick && ComposePlayerBotShoutAnswer(pick, q, text) && !text.empty())
			{
				char msg[CHAT_MAX_LEN + 1];
				snprintf(msg, sizeof(msg), "%s : %s", pick->GetName(), text.c_str());
				SendPlayerBotShout(msg, q.bEmpire);
				BattlePassOnShout(pick);
				NotePlayerBotPublicLine(pick, playerbot_conv::PL_TALK, false, text.c_str());
				s_adwPlayerBotShoutAnswerAt[q.bEmpire] = dwNow;
				++s_uPlayerBotShoutAnswers;
				sys_log(0, "PLAYERBOT_SHOUT_ANSWER: pid=%u name=%s empire=%u to=%s kind=%u text=\"%s\"",
						pick->GetPlayerID(), pick->GetName(), (unsigned int)q.bEmpire, q.asker.c_str(),
						(unsigned int)q.bKind, text.c_str());
			}
			s_vecPlayerBotShoutQuestions.erase(s_vecPlayerBotShoutQuestions.begin() + i);
		}
	}

	// The whole of it, once a manager tick.
	void ManagePlayerBotChatWorld(DWORD dwNow)
	{
		ManagePlayerBotTradeChat(dwNow);
		ManagePlayerBotShoutAnswers(dwNow);
	}
}

namespace playerbot_bpbots
{
	// The chatter's own questions (LineTalk), for another bot to answer.
	void EnqueueBotShoutQuestion(BYTE empire, BYTE kind, int level, DWORD askerPID, const char* asker,
			const std::string& object)
	{
		EnqueuePlayerBotShoutQuestion(empire, kind, level, askerPID, asker, object, true);
	}
}

#endif
