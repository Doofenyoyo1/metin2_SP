#ifndef __INC_METIN2_PLAYERBOT_GUILD_AID_RULES_H__
#define __INC_METIN2_PLAYERBOT_GUILD_AID_RULES_H__

// A person's guild answering for the person, as pure policy.
//
// A bot a person strikes has had its guild answer since Iwakura's Community
// Patch 2, point 15 (playerbot_anti_pk.h, BOT_FOE_GUILD): every bot of the
// guild within twelve kilometres goes for the person. The other side of the
// same fight had nothing: "W przypadku walki z botem z innego krolestwa na
// jakiejs mapie zlatuja sie boty z jego gildii by walczyc z nami, ale boty z
// naszej gildii kompletnie to olewaja (dwukrotnie pobliski bot z mojej gildii
// olal sytuacje, ze walcze z 5 innymi botami)" (Derpsonkowy95, 28 September;
// Iwakura: "Punkt 1 jest spoko, mozna dodac"; the operator: "jestem na tak"). People
// invite bots into their guilds, and those bots now answer for the person the
// way a bot guild answers for its bot - bounded where that one is not:
//
// - against bots only, of another kingdom and another guild than the
//   person's. A bot of the person's own kingdom fights the person only after
//   the person's own blow in free mode, which is the Anti-PK protocol's to
//   answer, and a person striking the person is the people's own quarrel: this
//   answer never turns a bot on a person;
// - never for a person under a truce (playerbot_truce_rules.h): a person who
//   has made peace with the bots has no fight for its guild to take up;
// - by the guild's bots near the person, one for each bot fighting it and
//   never more than a cap - even sides, as the wars have been since 2.2.26;
// - for as long as the fight lasts: an attacker is in it while its last blow
//   at the person is recent or while it still holds the person as its foe,
//   and a defender lets go the moment its attacker is not, the person is down
//   or gone, or the person has made peace.
//
// Who may answer is asked in the order of what outranks the answer (WhyNot):
// a raid, a war, a duel, a companion's owner, another person's claim on the
// bot, the errands that hold engine state (a town visit, a stall, an open
// trade or safebox), and then the bot itself - away from the keyboard,
// standing up after a death, hurt, too far.
//
// No engine types: pids, VIDs, empires, guild ids and the clock come in as
// numbers. The engine side is playerbot_anti_pk.h. Tested in
// tests/playerbot_guild_aid_rules_test.cpp.

namespace playerbot_guild_aid_rules
{
	// Whether a blow at a person opens the person's guild's call, or renews
	// it: a bot's, of another kingdom and another guild, at a person of a
	// guild who is at peace with nobody.
	inline bool OpensCall(bool attackerIsBot, int attackerEmpire, unsigned int attackerGuild,
			int personEmpire, unsigned int personGuild, bool personTruced)
	{
		return attackerIsBot && personGuild != 0 && !personTruced &&
				attackerEmpire != personEmpire && attackerGuild != personGuild;
	}

	// The bots striking one person, by pid: the VID the engine finds the
	// character by (a warp or a relog gives it a new one, and the next blow
	// brings it) and the clock of its last blow, in the game's milliseconds
	// that wrap - compared by difference. A person fought by more than
	// MAX_ATTACKERS at once has the stalest forgotten first.
	const int MAX_ATTACKERS = 8;

	struct TAttacker
	{
		unsigned int pid;
		unsigned int vid;
		unsigned int lastBlowAt;
		TAttacker() : pid(0), vid(0), lastBlowAt(0) {}
	};

	struct TAttackers
	{
		TAttacker list[MAX_ATTACKERS];
		int count;
		TAttackers() : count(0) {}
	};

	inline int FindAttacker(const TAttackers& t, unsigned int pid)
	{
		for (int i = 0; i < t.count; ++i)
			if (t.list[i].pid == pid)
				return i;
		return -1;
	}

	inline void NoteBlow(TAttackers& t, unsigned int pid, unsigned int vid, unsigned int now)
	{
		int slot = FindAttacker(t, pid);
		if (slot < 0)
		{
			if (t.count < MAX_ATTACKERS)
				slot = t.count++;
			else
			{
				slot = 0;
				for (int i = 1; i < t.count; ++i)
					if (now - t.list[i].lastBlowAt > now - t.list[slot].lastBlowAt)
						slot = i;
			}
		}
		t.list[slot].pid = pid;
		t.list[slot].vid = vid;
		t.list[slot].lastBlowAt = now;
	}

	// An attacker is in the fight while its last blow is younger than
	// memoryMs, or while it still holds the person as its foe - the engine's
	// word, stillOnPerson: a bot chasing a person who ran lands no blow for a
	// while and is fighting all the same.
	inline bool IsLive(const TAttacker& a, unsigned int now, unsigned int memoryMs, bool stillOnPerson)
	{
		return a.pid != 0 && (stillOnPerson || now - a.lastBlowAt < memoryMs);
	}

	// How many of the guild answer: perAttacker for every attacker in the
	// fight, never more than cap. Even sides - a lone bot that picked on a
	// person does not bring the person's whole guild down on it.
	inline int DefendersWanted(int liveAttackers, int perAttacker, int cap)
	{
		if (liveAttackers <= 0 || perAttacker <= 0 || cap <= 0)
			return 0;
		const long long n = (long long)liveAttackers * perAttacker;
		return n > cap ? cap : (int)n;
	}

	// A defender takes no attacker more than levelOver levels over itself: it
	// would only die to it. Stronger than the attacker is what help is.
	inline bool LevelAllows(int defenderLevel, int attackerLevel, int levelOver)
	{
		return attackerLevel - defenderLevel <= levelOver;
	}

	// Which attacker a defender takes: the one fewest of the guild are on,
	// the nearest among those, a defender already on one costing
	// crowdPenalty units of distance - so the guild spreads over the
	// attackers, and a much nearer one still wins. -1 for none.
	struct TCandidate
	{
		int distance;
		int defenders;
	};

	inline int PickAttacker(const TCandidate* candidates, int n, int crowdPenalty)
	{
		int best = -1;
		long long bestCost = 0;
		for (int i = 0; i < n; ++i)
		{
			const long long cost = (long long)candidates[i].distance +
					(long long)candidates[i].defenders * crowdPenalty;
			if (best < 0 || cost < bestCost)
			{
				best = i;
				bestCost = cost;
			}
		}
		return best;
	}

	// Why a bot of the guild does not answer, the first reason in the order
	// of the promise: what outranks the answer, then the bot itself, then the
	// fight (as many already answer, no attacker it can take).
	enum ERefusal
	{
		REFUSE_NONE = 0,
		REFUSE_RAID,          // the Demon Tower, a world boss's raid, the Catacomb
		REFUSE_WAR,           // its guild's war
		REFUSE_DUEL,          // a duel it agreed to
		REFUSE_COMPANION,     // a player's own companion (Towarzysz): its owner's
		REFUSE_SERVING,       // a mercenary's contract, another person's call or company
		REFUSE_PERSON_PARTY,  // a party a person leads: that person's to lead
		REFUSE_TOWN,          // a town visit, a stall or a stand's service, a trade or the safebox open
		REFUSE_AFK,           // away from the keyboard (SLABY's stop): it has not seen the fight
		REFUSE_RECOVERING,    // standing up after a death, or breaking off a losing fight
		REFUSE_HURT,          // under the health it takes a fight on with
		REFUSE_FAR,           // further from the person than the call reaches
		REFUSE_ENOUGH,        // as many of the guild already answer as fight the person
		REFUSE_LEVEL,         // every attacker it could reach too far over it
		REFUSE_NO_FOE,        // no attacker it can reach and strike
		REFUSE_COUNT
	};

	struct TAnswerer
	{
		bool onRaid;
		bool atWar;
		bool inDuel;
		bool companion;
		bool serving;
		bool inPersonParty;
		bool onErrand;
		bool afk;
		bool recovering;
		int hpPercent;
		int distanceToPerson;
		TAnswerer() : onRaid(false), atWar(false), inDuel(false), companion(false), serving(false),
			inPersonParty(false), onErrand(false), afk(false), recovering(false), hpPercent(100),
			distanceToPerson(0) {}
	};

	// Whether this bot may answer: REFUSE_NONE, or why not. The fight itself
	// (REFUSE_ENOUGH, _LEVEL, _NO_FOE) is the caller's to add, because only
	// the engine can say who is in reach.
	inline ERefusal WhyNot(const TAnswerer& a, int minHpPercent, int range)
	{
		if (a.onRaid)
			return REFUSE_RAID;
		if (a.atWar)
			return REFUSE_WAR;
		if (a.inDuel)
			return REFUSE_DUEL;
		if (a.companion)
			return REFUSE_COMPANION;
		if (a.serving)
			return REFUSE_SERVING;
		if (a.inPersonParty)
			return REFUSE_PERSON_PARTY;
		if (a.onErrand)
			return REFUSE_TOWN;
		if (a.afk)
			return REFUSE_AFK;
		if (a.recovering)
			return REFUSE_RECOVERING;
		if (a.hpPercent < minHpPercent)
			return REFUSE_HURT;
		if (a.distanceToPerson > range)
			return REFUSE_FAR;
		return REFUSE_NONE;
	}

	inline const char* RefusalName(int r)
	{
		switch (r)
		{
			case REFUSE_NONE: return "none";
			case REFUSE_RAID: return "raid";
			case REFUSE_WAR: return "war";
			case REFUSE_DUEL: return "duel";
			case REFUSE_COMPANION: return "companion";
			case REFUSE_SERVING: return "serving";
			case REFUSE_PERSON_PARTY: return "person_party";
			case REFUSE_TOWN: return "town";
			case REFUSE_AFK: return "afk";
			case REFUSE_RECOVERING: return "recovering";
			case REFUSE_HURT: return "hurt";
			case REFUSE_FAR: return "far";
			case REFUSE_ENOUGH: return "enough";
			case REFUSE_LEVEL: return "level";
			case REFUSE_NO_FOE: return "no_foe";
			default: return "?";
		}
	}

	// Whether a defender holds on to the attacker it took: while the person
	// stands on its map, alive and under no truce, and the attacker is still
	// in the fight - the first reason to let go otherwise.
	enum EHold
	{
		HOLD_ON = 0,
		LET_GO_PERSON_LEFT,   // logged out, warped, or on another map
		LET_GO_PERSON_DOWN,
		LET_GO_TRUCE,         // the person made peace with the bots
		LET_GO_FIGHT_OVER     // the attacker has stopped fighting the person
	};

	inline EHold Hold(bool personHere, bool personAlive, bool personTruced, bool attackerLive)
	{
		if (!personHere)
			return LET_GO_PERSON_LEFT;
		if (!personAlive)
			return LET_GO_PERSON_DOWN;
		if (personTruced)
			return LET_GO_TRUCE;
		if (!attackerLive)
			return LET_GO_FIGHT_OVER;
		return HOLD_ON;
	}

	inline const char* HoldName(EHold h)
	{
		switch (h)
		{
			case HOLD_ON: return "";
			case LET_GO_PERSON_LEFT: return "person_left";
			case LET_GO_PERSON_DOWN: return "person_down";
			case LET_GO_TRUCE: return "truce";
			case LET_GO_FIGHT_OVER: return "fight_over";
			default: return "?";
		}
	}
}

#endif
