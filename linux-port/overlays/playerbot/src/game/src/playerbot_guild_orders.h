#ifndef __INC_METIN2_PLAYERBOT_GUILD_ORDERS_H__
#define __INC_METIN2_PLAYERBOT_GUILD_ORDERS_H__

// A person's orders to the bots of the person's own guild - the engine side.
//
// Derpsonkowy95, 28 September: "Wyobrazmy sobie, ze potrzebujemy pomocy jako
// gracz przy zadymie/expie. Bot moglby reagowac na kluczowe slowa ... i przy
// podaniu lokalizacji (mapa + kordy) bot moglby tam przyjsc"; the operator:
// "jestem na tak, ale ... warto zrobic to w GUI jako komendy gildyjne".
// So it is three buttons in the guild window (uiguildbots.py in the client
// root) and one command, "/gildia_boty pomoc|exp|wracajcie" (playerbotify's
// apply_guild_bot_orders hands it to CPlayerBotManager::OnGuildBotOrder), and
// the place is never the client's to say: the bots come to the character the
// command came from, where it stands on the server, and follow it there.
//
//   HandlePlayerBotGuildOrder()   - the command: who may order, where, which
//                                   bots answer, and the one line that says
//                                   who is coming
//   GetPlayerBotGuildCallRefusal() - whether a bot of the guild is free to come
//                                   to a person of the guild at all
//   FightPlayerBotGuildOrderFoe() - what such a bot fights once it is there
//
// The rest is the whisper summon's ("chodz do mnie",
// playerbot_chat_conversation.h), which already walks a bot to a person and
// keeps it there, ends by itself (the stay over, the person gone or off the
// map, a walk that never arrives) and gives way to anything with a better
// right - a duel, a war, the tower, a stall. An order is that summon with its
// order on it: its stay, its fight, and for a bot on another map of this core
// the move onto the person's map. The policy - the words, who may order, the
// level window, the numbers, the ranking, the clock - is
// playerbot_guild_order_rules.h, unit-tested.
//
// Where a bot comes from. A bot stands on a map this core hosts, and so does
// the person who gave the order - the command came to this core. A bot of the
// guild on the person's map walks; one on another map of this core is moved
// onto the person's map by the one move a bot makes for every map change
// (TransitionPlayerBotMap, as the follow pass moves the bots of a person's
// party and the tower's master calls his guild). A bot on another core - the
// other kingdoms' villages on the split world, the other channels - cannot
// come at all ("One map is hosted by exactly one core" in CLAUDE.md), and the
// person hears that nobody of the guild is in this part of the world. The
// person's map must take a bot from elsewhere: not a dungeon instance, not the
// Spider Dungeons (a move there is a desert crossing, TransitionPlayerBotMap),
// a map with a navigation grid.
//
// What a called bot fights, and what it does not. Beside the person it takes
// the companion's choice of foe (FindPlayerBotSidekickFoe): the person's own
// target once it is a fight, what is at the person, what is at the bot, and
// for a hunt the nearest monster round the person - never a boss it has to go
// looking for - and fights with the tower's fight, skills and all. A Shaman
// keeps the person's buffs up first. A person the person is fighting is not
// its business, but for the enemy of a guild war, which the companion's
// choice already knows; a guild's help for a person who is attacked by bots
// is another thing and not this command's.
//
// An implementation fragment in the sense playerbot_types.h describes:
// include it exactly once, after playerbot_companions.h (IsPlayerBotHeldForCompany),
// which comes after the companion (the choice of foe) and the tower (the
// fight).

namespace
{
	// When each person last called the guild's bots, by pid
	// (playerbot_guild_order_rules::CALL_COOLDOWN_MS), kept small.
	std::map<DWORD, DWORD> s_mapPlayerBotGuildCallAt;
	const size_t PLAYERBOT_GUILD_CALL_MEMORY_MAX = 256;

	enum EPlayerBotGuildCallRefusal
	{
		PLAYERBOT_GUILD_CALL_FREE = 0,
		PLAYERBOT_GUILD_CALL_BUSY,      // something with a better right has it
		PLAYERBOT_GUILD_CALL_WITH_YOU,  // in the person's own party, or the person's own companion
		PLAYERBOT_GUILD_CALL_AWAY       // on a map no move of a bot's leads from to the person's
	};

	// Whether the person's map takes a bot from another map of this core: the
	// one move a bot makes (TransitionPlayerBotMap) refuses a map with no
	// navigation grid, turns a move onto a Spider Dungeon into a desert
	// crossing, and nothing moves a bot into a dungeon instance but the
	// dungeon itself.
	bool CanPlayerBotGuildCallCrossMaps(LPCHARACTER person)
	{
		if (!person || !person->GetSectree())
			return false;
		const long mapIndex = person->GetMapIndex();
		return mapIndex > 0 && mapIndex < PLAYERBOT_INSTANCE_MAP_INDEX_MIN && IsPlayerBotMapHostedHere(mapIndex) &&
				!IsPlayerBotSpiderMap(mapIndex) && CPlayerBotNavigation::instance(mapIndex).Init(mapIndex);
	}

	// Whether a bot of the person's guild is free to come to the person now,
	// whatever the call: its own life's business with a better right, the
	// person's own company, a map it cannot come from. The level window is the
	// order's (playerbot_guild_order_rules::LevelFits), asked apart. The one
	// question every call of a guild's bots to one of its people asks of each
	// bot, so it stands on its own: a guild's help that comes unasked would ask
	// it too. crossMaps is CanPlayerBotGuildCallCrossMaps(person), asked once
	// per call and not per bot.
	int GetPlayerBotGuildCallRefusal(LPCHARACTER bot, const TPlayerBotAIState& state, LPCHARACTER person,
			bool crossMaps, DWORD dwNow)
	{
		if (!bot || !person || bot->IsDead())
			return PLAYERBOT_GUILD_CALL_BUSY;
		const DWORD pid = bot->GetPlayerID();
		// "Towarzysz" is its owner's, whoever else calls it.
		if (IsPlayerBotSidekickPID(pid))
		{
			const TPlayerBotSidekick* rec = FindPlayerBotSidekickOf(pid);
			return rec && rec->dwOwnerPID == person->GetPlayerID()
					? PLAYERBOT_GUILD_CALL_WITH_YOU : PLAYERBOT_GUILD_CALL_BUSY;
		}
		LPPARTY party = bot->GetParty();
		if (party && party == person->GetParty())
			return PLAYERBOT_GUILD_CALL_WITH_YOU;
		// Called by somebody else; called by this person is a call renewed.
		const DWORD summoner = GetPlayerBotSummonerPID(pid);
		if (summoner != 0 && summoner != person->GetPlayerID())
			return PLAYERBOT_GUILD_CALL_BUSY;
		// A contract on either side, a companion with a person in its party.
		if (summoner == 0 && IsPlayerBotHeldForCompany(bot))
			return PLAYERBOT_GUILD_CALL_BUSY;
		// The tower, a world boss's raid, the Catacomb, any dungeon instance,
		// Tanaka and Zuo: each owns the bot until it lets go.
		if (IsPlayerBotInDungeonBusiness(bot, state) || state.bWorldEventKind != 0)
			return PLAYERBOT_GUILD_CALL_BUSY;
		// Away from its keyboard, as a weak mood's pause plays it: a call waits
		// for its return, as a person's would.
		if (state.persona.dwAfkUntil != 0 && dwNow < state.persona.dwAfkUntil)
			return PLAYERBOT_GUILD_CALL_BUSY;
		if (bot->GetMapIndex() != person->GetMapIndex() &&
				(!crossMaps || IsPlayerBotSpiderMap(bot->GetMapIndex())))
			return PLAYERBOT_GUILD_CALL_AWAY;
		// And everything a whisper's call gives way to: a stall, the water, the
		// vein, a duel, a war, a person's party or order.
		if (GetPlayerBotSummonBlock(bot, state, person, dwNow, true) != playerbot_conv::SB_NONE)
			return PLAYERBOT_GUILD_CALL_BUSY;
		return PLAYERBOT_GUILD_CALL_FREE;
	}

	// The fight of a bot beside the person whose order it answers
	// (ManagePlayerBotSummon asks it once the bot is there). A Shaman keeps the
	// person's buffs up first, between blows, as a companion does. With nothing
	// to fight, a target that is gone is let go, so the light tick does not
	// swing at it.
	bool FightPlayerBotGuildOrderFoe(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER person, BYTE order,
			DWORD dwNow)
	{
		if (!ch || !person || person->GetMapIndex() != ch->GetMapIndex())
			return false;
		if (ManagePlayerBotBuffPerson(ch, state, person, dwNow, false, true))
			return true;
		int why = 0;
		bool personFighting = false;
		const BYTE stance = order == playerbot_guild_order_rules::ORDER_HUNT
				? PLAYERBOT_SIDEKICK_STANCE_ATTACK : PLAYERBOT_SIDEKICK_STANCE_DEFEND;
		LPCHARACTER foe = FindPlayerBotSidekickFoe(ch, person, stance, why, personFighting);
		if (!foe)
		{
			if (state.dwTargetVID != 0)
			{
				LPCHARACTER held = CHARACTER_MANAGER::instance().Find(state.dwTargetVID);
				if (!held || held->IsDead())
				{
					state.dwTargetVID = 0;
					ch->SetVictim(NULL);
				}
			}
			return false;
		}
		state.dwLastMeaningfulActivityTime = dwNow;
		return FightPlayerBotTowerObjective(ch, state, foe, dwNow);
	}

	// "Wracajcie": every bot this person called by an order goes back to its
	// own life now. No right is asked: it touches the person's own calls only.
	void ReleasePlayerBotGuildOrder(LPCHARACTER person, DWORD dwNow)
	{
		std::vector<DWORD> called;
		for (TPlayerBotSummonMap::const_iterator it = s_mapPlayerBotSummons.begin(); it != s_mapPlayerBotSummons.end(); ++it)
			if (it->second.dwPlayerPID == person->GetPlayerID() &&
					it->second.bOrder != playerbot_guild_order_rules::ORDER_NONE)
				called.push_back(it->first);
		std::string names;
		for (size_t i = 0; i < called.size(); ++i)
		{
			LPCHARACTER bot = CHARACTER_MANAGER::instance().FindByPID(called[i]);
			if (bot)
			{
				if (!names.empty())
					names += ", ";
				names += bot->GetName();
			}
			EndPlayerBotSummon(called[i], playerbot_conv::SUMMON_END_DISMISSED, dwNow);
		}
		sys_log(0, "PLAYERBOT_GUILD_ORDER: released pid=%u name=%s bots=%u",
				person->GetPlayerID(), person->GetName(), (unsigned int)called.size());
		if (called.empty())
			TellPlayerBotPerson(person, "%s", "[Gildia] Zaden bot gildii nie szedl do ciebie.");
		else
			TellPlayerBotPerson(person, "[Gildia] Wracaja do swoich spraw (%u): %s.", (unsigned int)called.size(), names.c_str());
	}

	// "/gildia_boty <order>" (CPlayerBotManager::OnGuildBotOrder).
	void HandlePlayerBotGuildOrder(LPCHARACTER person, const char* argument)
	{
		using namespace playerbot_guild_order_rules;
		if (!person || !person->IsPC() || !person->GetDesc() || person->GetDesc()->IsBot() ||
				CPlayerBotManager::instance().IsRegisteredBotPID(person->GetPlayerID()))
			return;
		const DWORD dwNow = get_dword_time();
		char word[32];
		one_argument(argument ? argument : "", word, sizeof(word));
		char folded[32];
		FoldPlayerBotChatText(word, folded, sizeof(folded));
		const EOrder order = ParseOrder(folded);
		if (order == ORDER_NONE)
		{
			TellPlayerBotPerson(person, "%s", "[Gildia] Rozkazy dla botow gildii: /gildia_boty pomoc, /gildia_boty exp, /gildia_boty wracajcie.");
			return;
		}
		if (order == ORDER_RELEASE)
		{
			ReleasePlayerBotGuildOrder(person, dwNow);
			return;
		}

		CGuild* guild = person->GetGuild();
		if (!guild)
		{
			TellPlayerBotPerson(person, "%s", "[Gildia] Nie nalezysz do zadnej gildii.");
			return;
		}
		// The engine's own ranks: the master, or a rank the master gave the
		// right to use the guild's skills (the grade page's fourth box).
		const TGuildMember* member = guild->GetMember(person->GetPlayerID());
		const bool isMaster = guild->GetMasterPID() == person->GetPlayerID();
		const bool rankRight = member && member->grade >= 1 && member->grade <= GUILD_GRADE_COUNT &&
				guild->HasGradeAuth(member->grade, GUILD_AUTH_USE_SKILL);
		if (!MayOrder(isMaster, rankRight))
		{
			TellPlayerBotPerson(person, "%s", "[Gildia] Botom gildii rozkazuje mistrz i rangi, ktorym dal prawo do umiejetnosci gildii (zakladka Rangi).");
			return;
		}
		const long mapIndex = person->GetMapIndex();
		if (mapIndex >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN)
		{
			TellPlayerBotPerson(person, "%s", "[Gildia] Do lochu z wlasna instancja boty nie dojda.");
			return;
		}
		// The tower's ground floor: the stone's jump takes everybody standing on
		// it up to the floors, a bot under the tower's level too - the tower
		// calls a master's guild itself, from its level up.
		if (mapIndex == PLAYERBOT_MAP_DEMON_TOWER)
		{
			TellPlayerBotPerson(person, "%s", "[Gildia] Na parter Wiezy Demonow boty nie przyjda na rozkaz - kamien zabralby je na pietra.");
			return;
		}
		std::map<DWORD, DWORD>::const_iterator last = s_mapPlayerBotGuildCallAt.find(person->GetPlayerID());
		if (!CallAllowed(last != s_mapPlayerBotGuildCallAt.end(),
				last != s_mapPlayerBotGuildCallAt.end() ? last->second : 0, dwNow))
		{
			const unsigned int wait = (CALL_COOLDOWN_MS - (dwNow - last->second) + 999) / 1000;
			TellPlayerBotPerson(person, "[Gildia] Rozkaz juz poszedl - nastepny za %u s.", wait);
			return;
		}
		if (s_mapPlayerBotGuildCallAt.size() >= PLAYERBOT_GUILD_CALL_MEMORY_MAX)
		{
			for (std::map<DWORD, DWORD>::iterator it = s_mapPlayerBotGuildCallAt.begin();
					it != s_mapPlayerBotGuildCallAt.end(); )
			{
				if (dwNow - it->second >= CALL_COOLDOWN_MS)
					s_mapPlayerBotGuildCallAt.erase(it++);
				else
					++it;
			}
		}
		s_mapPlayerBotGuildCallAt[person->GetPlayerID()] = dwNow;

		// Every bot of the guild on this core, sorted into who may come and why
		// the rest may not.
		const bool crossMaps = CanPlayerBotGuildCallCrossMaps(person);
		const int personLevel = person->GetLevel();
		std::vector<TCandidate> candidates;
		unsigned int here = 0, busy = 0, outOfLevel = 0, away = 0, withYou = 0;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin(); it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER bot = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!bot || bot->GetGuild() != guild || bot == person)
				continue;
			++here;
			switch (GetPlayerBotGuildCallRefusal(bot, it->second, person, crossMaps, dwNow))
			{
				case PLAYERBOT_GUILD_CALL_BUSY: ++busy; continue;
				case PLAYERBOT_GUILD_CALL_WITH_YOU: ++withYou; continue;
				case PLAYERBOT_GUILD_CALL_AWAY: ++away; continue;
				default: break;
			}
			if (!LevelFits(order, personLevel, bot->GetLevel()))
			{
				++outOfLevel;
				continue;
			}
			TCandidate c;
			c.pid = it->first;
			c.sameMap = bot->GetMapIndex() == mapIndex;
			c.distance = c.sameMap ? DISTANCE_APPROX(bot->GetX() - person->GetX(), bot->GetY() - person->GetY()) : 0;
			c.levelGap = std::abs((int)bot->GetLevel() - personLevel);
			candidates.push_back(c);
		}
		Pick(candidates, order);

		std::string names;
		unsigned int coming = 0, renewed = 0;
		for (size_t i = 0; i < candidates.size(); ++i)
		{
			LPCHARACTER bot = CHARACTER_MANAGER::instance().FindByPID(candidates[i].pid);
			const int started = StartPlayerBotSummon(bot, person, dwNow, (BYTE)order);
			if (started == playerbot_conv::SUMMON_START_OK)
				++coming;
			else if (started == playerbot_conv::SUMMON_START_RENEWED)
				++renewed;
			else
			{
				++busy;
				continue;
			}
			if (!names.empty())
				names += ", ";
			names += bot->GetName();
		}
		sys_log(0, "PLAYERBOT_GUILD_ORDER: order pid=%u name=%s guild=%s order=%s map=%ld level=%d here=%u coming=%u renewed=%u busy=%u level_out=%u away=%u with_you=%u cross_maps=%d",
				person->GetPlayerID(), person->GetName(), guild->GetName(), OrderName(order), mapIndex, personLevel,
				here, coming, renewed, busy, outOfLevel, away, withYou, crossMaps ? 1 : 0);

		const unsigned int answering = coming + renewed;
		if (answering > 0)
		{
			if (order == ORDER_HELP)
				TellPlayerBotPerson(person, "[Gildia] Na pomoc ida (%u): %s.",
						answering, names.c_str());
			else
				TellPlayerBotPerson(person, "[Gildia] Expic z toba ida (%u): %s.", answering, names.c_str());
			if (busy + outOfLevel + away > 0)
				TellPlayerBotPerson(person, "[Gildia] Nie przyjda: zajete %u, poza poziomem %u, bez drogi tutaj %u.",
						busy, outOfLevel, away);
			return;
		}
		if (here == 0)
			TellPlayerBotPerson(person, "%s", "[Gildia] W tej czesci swiata nie ma teraz botow twojej gildii.");
		else if (busy + outOfLevel + away == 0)
			TellPlayerBotPerson(person, "%s", "[Gildia] Boty twojej gildii juz sa przy tobie.");
		else
			TellPlayerBotPerson(person, "[Gildia] Zaden bot gildii nie moze teraz przyjsc: zajete %u, poza poziomem %u, bez drogi tutaj %u.",
					busy, outOfLevel, away);
	}
}

#endif
