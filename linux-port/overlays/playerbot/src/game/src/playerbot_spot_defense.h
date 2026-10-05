#ifndef __INC_METIN2_PLAYERBOT_SPOT_DEFENSE_H__
#define __INC_METIN2_PLAYERBOT_SPOT_DEFENSE_H__

// MT2009_PLUS_BOT_CHAT_V2 - a bot defends its spot (the engine half).
//
// "Kiedy ktos wejdzie na spota bota i zacznie go bic albo kradnie mu moby,
// bot pisze do gracza na priv 'Spadaj', 'Wypad', 'To moj spot', 'SPIEEEEEEEE
// STAD'" (the owner, 4 October). The rules - points, steps, tempers, lines -
// are playerbot_spot_rules.h; this file sees what a person does round a
// hunting bot and says it through the conversation engine, so the
// complaint is typed in the bot's own hand and a "czemu?" or a "sorry" after
// it is answered in kind (TBotSnapshot::spotQuarrel).
//
//   - NotePlayerBotSpotStruck: a person's blow at a bot, from the Anti-PK
//     protocol's NotePlayerBotStruck (CHARACTER::Damage). The protocol still
//     fights back as it always did; this only talks.
//   - ManagePlayerBotSpotDefense: the bot's tick, a look round every
//     SCAN_MS while it hunts. A monster the bot is fighting that a person
//     has hit too (the damage map holds both, mt2009's
//     Mt2009PlusGetDamageMap) is a mob stolen; a monster near the hunting
//     bot that a person hits is its spot being farmed. A person's hit counts
//     only when it is fresh (TBattleInfo::dwLastHit, the drop share's clock).
//
// Who is never complained at: the bot's party, its guild, its companion's
// owner, whoever called it over or ordered it to lure, a person it is fond of
// (the friend ledger), and anybody while the bot is in a safe zone, in a
// dungeon, at war, in a duel, behind a counter or after it has capitulated. Shouters, companions and mercenaries complain at nobody.
//
// The gentle bots leave the spot after their second word (the persona's
// avoided spot, honoured by the targeting and the hub choice); the hot-headed
// ones call a bot of their party or guild standing near, which takes their
// side in a whisper of its own. A person gets one such whisper from all the
// bots together every PERSON_GAP_MS at most. The SPOT key of the weights file
// switches it all off (playerbot_config.h).
//
// An implementation fragment: include it once, after playerbot_anti_pk.h and
// playerbot_chat_conversation.h.

#include "playerbot_spot_rules.h"

namespace
{
	const DWORD PLAYERBOT_SPOT_SCAN_MS = 1500;
	const int PLAYERBOT_SPOT_SCAN_RANGE = 1600;       // monsters this near the bot are looked at
	const int PLAYERBOT_SPOT_AREA_RANGE = 1000;       // ... and this near count as its spot
	const int PLAYERBOT_SPOT_PERSON_RANGE = 2200;     // the person must stand this near
	const DWORD PLAYERBOT_SPOT_HIT_FRESH_MS = 8000;   // a hit older than this is history
	const DWORD PLAYERBOT_SPOT_PERSON_GAP_MS = 10000; // one complaint a person this often
	// MT2009_PLUS_SPOT_WHISPER_CAP_V1 (the owner, 5 October): a person who leaves
	// Auto Lowy running walks into the bots' spots all day and every bot it
	// struck whispered - dozens of envelopes. All the bots together send one
	// person at most this many spot complaints (the friends' lines included)
	// an hour; the quarrel itself goes on silently (a bot still gives up and
	// leaves the spot).
	const unsigned int PLAYERBOT_SPOT_PERSON_HOUR_MAX = 2;
	const DWORD PLAYERBOT_SPOT_PERSON_HOUR_MS = 60 * 60 * 1000;
	std::map<DWORD, std::deque<DWORD> > s_mapPlayerBotSpotPersonHour;	// person pid -> when its complaints went
	const DWORD PLAYERBOT_SPOT_FIGHT_RECENT_MS = 20000;
	const DWORD PLAYERBOT_SPOT_LEAVE_MS = 10 * 60 * 1000;
	const int PLAYERBOT_SPOT_FRIEND_RANGE = 3000;
	const int PLAYERBOT_SPOT_FOND_AFFINITY = 40;      // a friend this close is let be
	const DWORD PLAYERBOT_SPOT_PRUNE_MS = 60 * 1000;

	struct TPlayerBotSpotQuarrel
	{
		playerbot_spot::TQuarrel q;
		DWORD dwPersonVID;
		std::string personName;
		TPlayerBotSpotQuarrel() : dwPersonVID(0) {}
	};

	typedef std::pair<DWORD, DWORD> TPlayerBotSpotKey; // (bot pid, person pid)
	std::map<TPlayerBotSpotKey, TPlayerBotSpotQuarrel> s_mapPlayerBotSpotQuarrels;
	std::map<DWORD, DWORD> s_mapPlayerBotSpotScanAt;      // by bot pid
	std::map<DWORD, DWORD> s_mapPlayerBotSpotPersonSaid;  // by person pid: the last complaint
	std::map<DWORD, DWORD> s_mapPlayerBotSpotFoughtAt;    // by bot pid: the last fight seen
	DWORD s_dwPlayerBotSpotPruneAt = 0;
	unsigned int s_uPlayerBotSpotWhispers = 0;

	// What the conversation asks (TBotSnapshot::spotQuarrel): the step the
	// complaints to this person reached, while the quarrel is remembered.
	int GetPlayerBotSpotQuarrel(DWORD botPID, DWORD personPID, bool& gaveUp)
	{
		gaveUp = false;
		std::map<TPlayerBotSpotKey, TPlayerBotSpotQuarrel>::const_iterator it =
				s_mapPlayerBotSpotQuarrels.find(std::make_pair(botPID, personPID));
		if (it == s_mapPlayerBotSpotQuarrels.end() || playerbot_spot::Forgotten(it->second.q, get_dword_time()))
			return 0;
		gaveUp = it->second.q.gaveUp;
		return it->second.q.level;
	}

	// The bots that never quarrel, and the moments no bot does.
	bool IsPlayerBotSpotQuiet(LPCHARACTER ch, const TPlayerBotAIState& state, DWORD dwNow)
	{
		if (!ch || ch->IsDead() || !IsPlayerBotSpotDefenseEnabled())
			return true;
		const DWORD pid = ch->GetPlayerID();
		if (IsPlayerBotShouterPID(pid) || IsPlayerBotSidekickPID(pid) || IsPlayerBotOnMercContract(pid))
			return true;
		// The village maps are hunting grounds too (M1, M2): only their
		// safe zones are quiet.
		const long mapIndex = ch->GetMapIndex();
		if (mapIndex >= PLAYERBOT_INSTANCE_MAP_INDEX_MIN || IsPlayerBotSafeZone(mapIndex, ch->GetX(), ch->GetY()))
			return true;
		if (ch->GetMyShop() || state.dwGuildWarEnemyGID != 0 || playerbot_pvp::IsInDuel(pid, dwNow))
			return true;
		if (state.persona.dwCapitulatedUntil != 0 && dwNow < state.persona.dwCapitulatedUntil)
			return true;
		return false;
	}

	// A person the bot does not quarrel with.
	bool IsPlayerBotSpotExempt(LPCHARACTER bot, const TPlayerBotAIState& state, LPCHARACTER person)
	{
		if (!person || !IsPlayerBotPersonCharacter(person) || person->IsDead())
			return true;
		if (bot->GetParty() && bot->GetParty() == person->GetParty())
			return true;
		if (bot->GetGuild() && bot->GetGuild() == person->GetGuild())
			return true;
		const DWORD personPID = person->GetPlayerID();
		if (state.dwLurePlayerPID == personPID || GetPlayerBotSummonerPID(bot->GetPlayerID()) == personPID)
			return true;
		if (GetPlayerBotAffinity(state, personPID) >= PLAYERBOT_SPOT_FOND_AFFINITY)
			return true;
		return false;
	}

	// A bot of the same party or guild near the complaining one: the friend
	// a hot-headed bot calls in.
	struct FPlayerBotSpotFriend
	{
		LPCHARACTER m_bot;
		LPCHARACTER m_found;
		FPlayerBotSpotFriend(LPCHARACTER bot) : m_bot(bot), m_found(NULL) {}
		void operator () (LPENTITY ent)
		{
			if (m_found || !ent || !ent->IsType(ENTITY_CHARACTER))
				return;
			LPCHARACTER c = static_cast<LPCHARACTER>(ent);
			if (c == m_bot || !c->IsPC() || c->IsDead() || !c->GetDesc() || !c->GetDesc()->IsBot())
				return;
			const bool sameParty = m_bot->GetParty() && c->GetParty() == m_bot->GetParty();
			const bool sameGuild = m_bot->GetGuild() && c->GetGuild() == m_bot->GetGuild();
			if (!sameParty && !sameGuild)
				return;
			if (IsPlayerBotShouterPID(c->GetPlayerID()) || IsPlayerBotSidekickPID(c->GetPlayerID()))
				return;
			if (DISTANCE_APPROX(c->GetX() - m_bot->GetX(), c->GetY() - m_bot->GetY()) > PLAYERBOT_SPOT_FRIEND_RANGE)
				return;
			m_found = c;
		}
	};

	// The complaint, through the conversation engine: the bot's hand, its
	// pace, and a memory of having said it.
	void SayPlayerBotSpotLine(LPCHARACTER bot, LPCHARACTER person, const char* text, const char* reason, DWORD delayMs)
	{
		if (!bot || !person || !text || !*text)
			return;
		// MT2009_PLUS_SPOT_WHISPER_CAP_V1: two complaints a person an hour.
		{
			const DWORD now = get_dword_time();
			std::deque<DWORD>& sent = s_mapPlayerBotSpotPersonHour[person->GetPlayerID()];
			while (!sent.empty() && now - sent.front() >= PLAYERBOT_SPOT_PERSON_HOUR_MS)
				sent.pop_front();
			if (sent.size() >= PLAYERBOT_SPOT_PERSON_HOUR_MAX)
				return;
			sent.push_back(now);
		}
		playerbot_conv::TBotSnapshot snap;
		if (!s_PlayerBotConvHost.BuildSnapshot(person->GetPlayerID(), bot->GetPlayerID(), snap))
			return;
		const DWORD dwNow = get_dword_time();
		s_PlayerBotConvEngine.QueueBotLine(s_PlayerBotConvHost, person->GetPlayerID(), bot->GetPlayerID(), text,
				reason ? reason : "", playerbot_conv::I_KS, snap, dwNow, delayMs);
		EnsurePlayerBotConvTimer();
	}

	// The gentle bot's last word, made true: the spot is the person's for a
	// while (the persona's avoided spot, which the targeting and the hub
	// choice honour), and the bot walks on.
	void LeavePlayerBotSpot(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		TPlayerBotPersona& p = state.persona;
		if (p.dwAvoidSpotUntil == 0 || dwNow >= p.dwAvoidSpotUntil)
		{
			p.lAvoidSpotMap = ch->GetMapIndex();
			p.lAvoidSpotX = ch->GetX();
			p.lAvoidSpotY = ch->GetY();
			p.dwAvoidSpotUntil = dwNow + PLAYERBOT_SPOT_LEAVE_MS;
		}
		state.dwTargetVID = 0;
		state.dwNextWanderTime = dwNow;
		state.dwHubChosenTime = 0;
		ClearPlayerBotRoute(state, true);
	}

	// One quarrel's next step, if it has one now.
	void StepPlayerBotSpotQuarrel(LPCHARACTER ch, TPlayerBotAIState& state, LPCHARACTER person,
			TPlayerBotSpotQuarrel& entry, DWORD dwNow)
	{
		playerbot_spot::TQuarrel& q = entry.q;
		// A "sorry" whispered to the bot lately calms it.
		const playerbot_conv::TConvMemory* mem =
				s_PlayerBotConvEngine.FindMemory(person->GetPlayerID(), ch->GetPlayerID());
		if (mem && mem->turnCount > 0 && mem->turns[0].intent == playerbot_conv::I_APOLOGY &&
				dwNow - mem->turns[0].at < playerbot_spot::APOLOGY_CALM_MS &&
				(q.apologyAt == 0 || mem->turns[0].at != q.apologyAt))
			playerbot_spot::NoteApology(q, mem->turns[0].at);
		std::map<DWORD, DWORD>::const_iterator said = s_mapPlayerBotSpotPersonSaid.find(person->GetPlayerID());
		if (said != s_mapPlayerBotSpotPersonSaid.end() && dwNow - said->second < PLAYERBOT_SPOT_PERSON_GAP_MS)
			return;
		const int temper = playerbot_conv::TemperOf(MapPlayerBotConvStyle(state));
		const int step = playerbot_spot::Decide(q, temper, dwNow);
		if (step == 0)
			return;
		const unsigned int roll = (unsigned int)number(0, 1 << 20);
		const char* line = playerbot_spot::LineFor(step, temper, q.struck, q.gaveUp, roll);
		s_mapPlayerBotSpotPersonSaid[person->GetPlayerID()] = dwNow;
		++s_uPlayerBotSpotWhispers;
		SayPlayerBotSpotLine(ch, person, line, playerbot_spot::ReasonFor(q.struck), 300 + number(0, 900));
		sys_log(0, "PLAYERBOT_SPOT: pid=%u name=%s to=%s step=%d temper=%d points=%d struck=%d gave_up=%d map=%ld text=\"%s\"",
				ch->GetPlayerID(), ch->GetName(), person->GetName(), step, temper, q.points, q.struck ? 1 : 0,
				q.gaveUp ? 1 : 0, ch->GetMapIndex(), line);
		if (q.gaveUp)
			LeavePlayerBotSpot(ch, state, dwNow);
		if (playerbot_spot::CallsFriends(q, temper, step) && ch->GetSectree())
		{
			q.calledFriends = true;
			FPlayerBotSpotFriend finder(ch);
			ch->GetSectree()->ForEachAround(finder);
			TPlayerBotAIStateMap::const_iterator friendState = finder.m_found
					? s_mapPlayerBotAIStates.find(finder.m_found->GetPlayerID()) : s_mapPlayerBotAIStates.end();
			if (friendState != s_mapPlayerBotAIStates.end() &&
					!IsPlayerBotSpotExempt(finder.m_found, friendState->second, person))
			{
				SayPlayerBotSpotLine(finder.m_found, person, playerbot_spot::FriendLine((unsigned int)number(0, 1 << 20)),
						"Bo to nasz spot, a ty kradniesz moby mojemu kumplowi.", 4000 + number(0, 4000));
				sys_log(0, "PLAYERBOT_SPOT: friend pid=%u name=%s backs pid=%u name=%s against=%s",
						finder.m_found->GetPlayerID(), finder.m_found->GetName(), ch->GetPlayerID(), ch->GetName(),
						person->GetName());
			}
		}
	}

	TPlayerBotSpotQuarrel& GetPlayerBotSpotEntry(LPCHARACTER bot, LPCHARACTER person)
	{
		TPlayerBotSpotQuarrel& entry =
				s_mapPlayerBotSpotQuarrels[std::make_pair(bot->GetPlayerID(), person->GetPlayerID())];
		entry.dwPersonVID = (DWORD)person->GetVID();
		entry.personName = person->GetName();
		return entry;
	}

	// A person's blow at a bot (the Anti-PK protocol's report).
	void NotePlayerBotSpotStruck(LPCHARACTER bot, LPCHARACTER person, DWORD dwNow)
	{
		if (!bot || !person || !IsPlayerBotSpotDefenseEnabled())
			return;
		TPlayerBotAIStateMap::iterator st = s_mapPlayerBotAIStates.find(bot->GetPlayerID());
		if (st == s_mapPlayerBotAIStates.end() || IsPlayerBotSpotQuiet(bot, st->second, dwNow) ||
				IsPlayerBotSpotExempt(bot, st->second, person))
			return;
		TPlayerBotSpotQuarrel& entry = GetPlayerBotSpotEntry(bot, person);
		if (playerbot_spot::NoteStruck(entry.q, dwNow))
			StepPlayerBotSpotQuarrel(bot, st->second, person, entry, dwNow);
	}

#if defined(PLAYERBOT_ENGINE_MT2009)
	// The look round: monsters near the bot with a person's fresh hit on them.
	struct FPlayerBotSpotScan
	{
		LPCHARACTER m_bot;
		const TPlayerBotAIState& m_state;
		DWORD m_now;
		bool m_hunting;
		struct THit
		{
			LPCHARACTER person;
			DWORD mobVid;
			bool contested;
		};
		std::vector<THit> m_hits;

		FPlayerBotSpotScan(LPCHARACTER bot, const TPlayerBotAIState& state, DWORD now, bool hunting)
			: m_bot(bot), m_state(state), m_now(now), m_hunting(hunting) {}

		void operator () (LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_CHARACTER) || m_hits.size() >= 16)
				return;
			LPCHARACTER mob = static_cast<LPCHARACTER>(ent);
			const bool stone = mob->IsStone();
			if (!stone && !mob->IsMonster())
				return;
			// The bosses are everybody's fight.
			if (!stone && mob->GetMobRank() >= MOB_RANK_BOSS)
				return;
			const int toBot = DISTANCE_APPROX(mob->GetX() - m_bot->GetX(), mob->GetY() - m_bot->GetY());
			if (toBot > PLAYERBOT_SPOT_SCAN_RANGE)
				return;
			const CHARACTER::TDamageMap& dm = mob->Mt2009PlusGetDamageMap();
			if (dm.empty())
				return;
			const DWORD botVid = (DWORD)m_bot->GetVID();
			const DWORD mobVid = (DWORD)mob->GetVID();
			const bool botOnIt = dm.find(m_bot->GetVID()) != dm.end() || m_state.dwTargetVID == mobVid ||
					mob->GetVictim() == m_bot;
			// A stone is only "mine" when the bot is breaking it; a monster
			// near a hunting bot is its spot.
			if (!botOnIt && (stone || !m_hunting || toBot > PLAYERBOT_SPOT_AREA_RANGE))
				return;
			for (CHARACTER::TDamageMap::const_iterator it = dm.begin(); it != dm.end(); ++it)
			{
				if ((DWORD)it->first == botVid)
					continue;
				if (it->second.dwLastHit == 0 || m_now - it->second.dwLastHit > PLAYERBOT_SPOT_HIT_FRESH_MS)
					continue;
				LPCHARACTER person = CHARACTER_MANAGER::instance().Find(it->first);
				if (!person || !IsPlayerBotPersonCharacter(person))
					continue;
				if (DISTANCE_APPROX(person->GetX() - m_bot->GetX(), person->GetY() - m_bot->GetY()) >
						PLAYERBOT_SPOT_PERSON_RANGE)
					continue;
				THit hit;
				hit.person = person;
				hit.mobVid = mobVid;
				hit.contested = botOnIt;
				m_hits.push_back(hit);
			}
		}
	};
#endif

	// Where the people of this core stand, once a second: a bot with nobody
	// near skips its look round, which is the costly part (a few people
	// against a thousand bots).
	struct TPlayerBotSpotPerson
	{
		long map;
		long x;
		long y;
	};
	std::vector<TPlayerBotSpotPerson> s_vecPlayerBotSpotPersons;
	DWORD s_dwPlayerBotSpotPersonsAt = 0;

	bool IsPlayerBotSpotPersonNear(LPCHARACTER ch, DWORD dwNow)
	{
		if (s_dwPlayerBotSpotPersonsAt == 0 || dwNow - s_dwPlayerBotSpotPersonsAt >= 1000)
		{
			s_dwPlayerBotSpotPersonsAt = dwNow;
			s_vecPlayerBotSpotPersons.clear();
			const DESC_MANAGER::DESC_SET& descs = DESC_MANAGER::instance().GetClientSet();
			for (DESC_MANAGER::DESC_SET::const_iterator d = descs.begin(); d != descs.end(); ++d)
			{
				LPCHARACTER c = (*d)->GetCharacter();
				if (!c || !IsPlayerBotPersonCharacter(c) || c->IsDead())
					continue;
				TPlayerBotSpotPerson p;
				p.map = c->GetMapIndex();
				p.x = c->GetX();
				p.y = c->GetY();
				s_vecPlayerBotSpotPersons.push_back(p);
			}
		}
		const long map = ch->GetMapIndex();
		for (size_t i = 0; i < s_vecPlayerBotSpotPersons.size(); ++i)
		{
			const TPlayerBotSpotPerson& p = s_vecPlayerBotSpotPersons[i];
			if (p.map == map && DISTANCE_APPROX(p.x - ch->GetX(), p.y - ch->GetY()) <= PLAYERBOT_SPOT_PERSON_RANGE + 600)
				return true;
		}
		return false;
	}

	// The forgotten quarrels and the bots no longer here.
	void PrunePlayerBotSpotQuarrels(DWORD dwNow)
	{
		if (s_dwPlayerBotSpotPruneAt != 0 && dwNow - s_dwPlayerBotSpotPruneAt < PLAYERBOT_SPOT_PRUNE_MS)
			return;
		s_dwPlayerBotSpotPruneAt = dwNow;
		// MT2009_PLUS_SPOT_WHISPER_CAP_V1: a person with no complaint this hour is forgotten.
		for (std::map<DWORD, std::deque<DWORD> >::iterator h = s_mapPlayerBotSpotPersonHour.begin();
				h != s_mapPlayerBotSpotPersonHour.end(); )
		{
			if (h->second.empty() || dwNow - h->second.back() >= PLAYERBOT_SPOT_PERSON_HOUR_MS)
				s_mapPlayerBotSpotPersonHour.erase(h++);
			else
				++h;
		}
		for (std::map<TPlayerBotSpotKey, TPlayerBotSpotQuarrel>::iterator it = s_mapPlayerBotSpotQuarrels.begin();
				it != s_mapPlayerBotSpotQuarrels.end(); )
		{
			if (playerbot_spot::Forgotten(it->second.q, dwNow))
				s_mapPlayerBotSpotQuarrels.erase(it++);
			else
				++it;
		}
		for (std::map<DWORD, DWORD>::iterator it = s_mapPlayerBotSpotScanAt.begin(); it != s_mapPlayerBotSpotScanAt.end(); )
		{
			if (s_mapPlayerBotAIStates.find(it->first) == s_mapPlayerBotAIStates.end())
			{
				s_mapPlayerBotSpotFoughtAt.erase(it->first);
				s_mapPlayerBotSpotScanAt.erase(it++);
			}
			else
				++it;
		}
		for (std::map<DWORD, DWORD>::iterator it = s_mapPlayerBotSpotPersonSaid.begin();
				it != s_mapPlayerBotSpotPersonSaid.end(); )
		{
			if (dwNow - it->second > playerbot_spot::REMEMBER_MS)
				s_mapPlayerBotSpotPersonSaid.erase(it++);
			else
				++it;
		}
	}

	// The bot's tick: never claims it, only watches and talks.
	void ManagePlayerBotSpotDefense(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		PrunePlayerBotSpotQuarrels(dwNow);
		if (!ch)
			return;
		const DWORD pid = ch->GetPlayerID();
		if (state.bCurrentAction == BOT_ACTION_FIGHT || state.bCurrentAction == BOT_ACTION_LOOT)
			s_mapPlayerBotSpotFoughtAt[pid] = dwNow;
		DWORD& scanAt = s_mapPlayerBotSpotScanAt[pid];
		if (scanAt != 0 && dwNow - scanAt < PLAYERBOT_SPOT_SCAN_MS)
			return;
		scanAt = dwNow;
		if (IsPlayerBotSpotQuiet(ch, state, dwNow) || !ch->GetSectree())
			return;
#if defined(PLAYERBOT_ENGINE_MT2009)
		const std::map<DWORD, DWORD>::const_iterator fought = s_mapPlayerBotSpotFoughtAt.find(pid);
		const bool hunting = fought != s_mapPlayerBotSpotFoughtAt.end() &&
				dwNow - fought->second < PLAYERBOT_SPOT_FIGHT_RECENT_MS;
		// A bot that has not fought for a while has no spot and no monster of
		// its own to lose: no look round (most of the population, most of the
		// time - the scan is the costly part).
		if (!hunting && state.dwTargetVID == 0)
			return;
		if (!IsPlayerBotSpotPersonNear(ch, dwNow))
			return;
		FPlayerBotSpotScan scan(ch, state, dwNow, hunting);
		ch->GetSectree()->ForEachAround(scan);
		std::set<DWORD> stepped;
		for (size_t i = 0; i < scan.m_hits.size(); ++i)
		{
			LPCHARACTER person = scan.m_hits[i].person;
			if (IsPlayerBotSpotExempt(ch, state, person))
				continue;
			TPlayerBotSpotQuarrel& entry = GetPlayerBotSpotEntry(ch, person);
			// The step is taken once the whole look round is counted.
			if (playerbot_spot::NoteMob(entry.q, scan.m_hits[i].mobVid, scan.m_hits[i].contested, dwNow))
				stepped.insert(person->GetPlayerID());
		}
		for (std::set<DWORD>::const_iterator it = stepped.begin(); it != stepped.end(); ++it)
		{
			LPCHARACTER person = CHARACTER_MANAGER::instance().FindByPID(*it);
			if (!person)
				continue;
			std::map<TPlayerBotSpotKey, TPlayerBotSpotQuarrel>::iterator e =
					s_mapPlayerBotSpotQuarrels.find(std::make_pair(pid, *it));
			if (e != s_mapPlayerBotSpotQuarrels.end())
				StepPlayerBotSpotQuarrel(ch, state, person, e->second, dwNow);
		}
#endif
	}
}

#endif
