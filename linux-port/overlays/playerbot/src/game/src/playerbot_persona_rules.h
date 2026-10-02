#ifndef __INC_PLAYERBOT_PERSONA_RULES_H__
#define __INC_PLAYERBOT_PERSONA_RULES_H__

// Iwakura's "SYSTEM OSOBOWOSCI v2.0" (19 September) as pure policy: the moods
// of his Bot Mood System, the Grinder's tiers and their experience locks, his
// Law of Advancement, the gambler's risk and the order in which the
// personalities claim a bot. No engine types, unit-tested
// (tests/playerbot_persona_rules_test.cpp); the engine's half is
// playerbot_mood.h and playerbot_persona.h.
//
// His document says a personality is not a bot's for life: it is what the bot
// is doing about its situation right now - a full bag makes a trader, a Metin
// in sight a stone breaker, a purse too heavy for the road a perfectionist or
// a gambler. So a personality here is an answer computed again every planning
// pass from signals the engine side gathers, and the old personality drawn
// by pid at login lives on underneath only as a character that tilts the odds
// (the operator's choice, 19 September).
//
// Every clock below counts elapsed play, not wall time: the engine side
// advances it by the time between two ticks, so what a bot remembers across a
// logout is how long it still has to run, and a bot that moves to another core
// takes the same numbers with it through its quest flags.
#include <algorithm>
#include <cstdint>
#include <vector>

namespace playerbot_persona
{
	// ---------------------------------------------------------------------
	// Personalities. Appended, never inserted: the id goes into the status
	// file both panels read and into the title the client draws.
	// ---------------------------------------------------------------------
	enum EPersona
	{
		PERSONA_GRINDER = 0,
		PERSONA_ZDOBYWCA,
		PERSONA_HANDLARZ,
		PERSONA_HAZARDZISTA,
		PERSONA_PERFEKCJONISTA,
		PERSONA_POGROMCA,
		PERSONA_GORNIK,
		PERSONA_RYBAK,
		PERSONA_NAJEMNIK,
		PERSONA_TOWARZYSZ,
		// Iwakura's Patch 3, point 7: the rare ones (ERare below), shown in red.
		// Appended, so every id before them means what it meant.
		PERSONA_METINOLOG,
		PERSONA_NALOGOWIEC,
		PERSONA_NAUKOWIEC,
		PERSONA_EGZEKUTOR,
		PERSONA_WEDKARZ,
		// Iwakura's Community Patch 5, point 1: the four gamblers that took
		// the Hazardzista's place, rare too and shown in purple. Appended for
		// the same reason as the rare ones above.
		PERSONA_HAZ_MLODSZY,
		PERSONA_HAZ_STARSZY,
		PERSONA_HAZ_NACZELNY,
		PERSONA_HAZ_SZALONY,
		// MT2009_PLUS_BOTLIFE_V1: Baek-Go's herbalist, the Gornik's sibling -
		// a bot at the herbal board the way the Gornik is one at a vein.
		// Appended like the rest; its title is PERSONA_TITLE_BASE + 19.
		PERSONA_ZIELARZ,
		PERSONA_COUNT
	};

	// What the status file carries for a persona or a mood while the operator
	// has the system switched off.
	const unsigned int PERSONA_NONE = 255;

	// The title command carries the old personality as 0..10 and a persona as
	// this plus its id. A client older than the persona names refuses an id it
	// does not know and draws nothing, which is better than drawing the wrong
	// name: every persona id would otherwise land on an old personality's.
	const unsigned int PERSONA_TITLE_BASE = 100;

	// ---------------------------------------------------------------------
	// Moods (BMS). The document's three, in order, so a mood can go up or
	// down by one.
	// ---------------------------------------------------------------------
	enum EMood
	{
		MOOD_SLABY = 0,
		MOOD_NORMALNY,
		MOOD_BARDZO_DOBRY,
		MOOD_COUNT
	};

	// What holds a mood where it is. Euphoria (a refine to +8 or +9) holds
	// BARDZO DOBRY for three hours and nothing changes it; the capitulation of
	// the anti-PK protocol holds SLABY for forty-five minutes.
	enum EMoodLock
	{
		MOOD_LOCK_NONE = 0,
		MOOD_LOCK_EUPHORIA,
		MOOD_LOCK_CAPITULATION
	};

	const uint32_t MOOD_ROTATION_MS = 6u * 3600u * 1000u;      // "co rowne 6 godzin rozgrywki"
	const uint32_t MOOD_DROUGHT_MS = 30u * 60u * 1000u;        // "przez 30 minut ... pecha"
	const uint32_t MOOD_EUPHORIA_MS = 3u * 3600u * 1000u;      // "przez rowne 3 godziny"
	const uint32_t MOOD_CAPITULATION_MS = 45u * 60u * 1000u;   // "na 45 minut"

	// What Advance reports, as bits.
	const int MOOD_EVENT_ROTATED = 1;
	const int MOOD_EVENT_DROUGHT = 2;
	const int MOOD_EVENT_UNLOCKED = 4;

	struct TMood
	{
		uint8_t mood;
		uint8_t lockKind;
		// How long the lock still holds, how long the bot has played since the
		// last rotation, and how long it has hunted since its last good drop.
		uint32_t lockLeftMs;
		uint32_t playedMs;
		uint32_t droughtMs;

		TMood() : mood(MOOD_NORMALNY), lockKind(MOOD_LOCK_NONE), lockLeftMs(0),
			playedMs(0), droughtMs(0) {}
	};

	inline uint32_t SaturatingAdd(uint32_t a, uint32_t b)
	{
		return a > 0xFFFFFFFFu - b ? 0xFFFFFFFFu : a + b;
	}

	inline bool IsMoodLocked(const TMood& m)
	{
		return m.lockKind != MOOD_LOCK_NONE && m.lockLeftMs > 0;
	}

	inline uint8_t ClampMood(unsigned int value)
	{
		return value >= MOOD_COUNT ? (uint8_t)MOOD_NORMALNY : (uint8_t)value;
	}

	// A fresh mood out of a roll: the document's rotation is "calkowicie
	// losowy", so the three are equally likely.
	inline uint8_t RollMood(uint32_t roll)
	{
		return (uint8_t)(roll % MOOD_COUNT);
	}

	// Play time passes. The lock runs down first; the drought clock runs only
	// while the bot is hunting - a trader in town or an angler at the water is
	// not "unlucky" for standing there - and never under a lock, which the
	// document says nothing may worsen. A rotation due under a lock waits for
	// the lock to end rather than being lost, and then happens at once.
	inline int AdvanceMood(TMood& m, uint32_t dtMs, bool hunting, uint32_t roll)
	{
		int events = 0;
		if (m.lockKind != MOOD_LOCK_NONE)
		{
			if (m.lockLeftMs > dtMs)
				m.lockLeftMs -= dtMs;
			else
			{
				m.lockLeftMs = 0;
				m.lockKind = MOOD_LOCK_NONE;
				events |= MOOD_EVENT_UNLOCKED;
			}
		}
		m.playedMs = SaturatingAdd(m.playedMs, dtMs);
		if (!IsMoodLocked(m) && hunting)
		{
			m.droughtMs = SaturatingAdd(m.droughtMs, dtMs);
			if (m.droughtMs >= MOOD_DROUGHT_MS)
			{
				m.droughtMs = 0;
				if (m.mood > MOOD_SLABY)
				{
					--m.mood;
					events |= MOOD_EVENT_DROUGHT;
				}
			}
		}
		if (!IsMoodLocked(m) && m.playedMs >= MOOD_ROTATION_MS)
		{
			m.mood = RollMood(roll);
			m.playedMs = 0;
			m.droughtMs = 0;
			events |= MOOD_EVENT_ROTATED;
		}
		return events;
	}

	// Something from the document's list of valuable items came to hand: one
	// level up, and the drought clock starts again. Under a lock nothing moves
	// - euphoria is already at the top, and a capitulation is a lock the
	// document calls forced.
	inline bool OnValuableDrop(TMood& m)
	{
		m.droughtMs = 0;
		if (IsMoodLocked(m) || m.mood >= MOOD_BARDZO_DOBRY)
			return false;
		++m.mood;
		return true;
	}

	// A refine that landed on +8 or +9. A second one inside the three hours
	// starts the three hours again.
	inline void OnEuphoria(TMood& m)
	{
		m.mood = MOOD_BARDZO_DOBRY;
		m.lockKind = MOOD_LOCK_EUPHORIA;
		m.lockLeftMs = MOOD_EUPHORIA_MS;
		m.droughtMs = 0;
	}

	// A refine to +8 or +9 that failed - burned at the anvil, or brought down
	// a grade under a scroll, which the document calls burning as well
	// ("spalenie Zwojem ... zrzucilo przedmiot"): one level down (Iwakura, 19
	// September: "spalenie itemu na +8/+9 obniza mood o 1"). A lock holds, as
	// it holds against the drought - euphoria is three hours nothing may
	// worsen, and a capitulation is already at the bottom.
	const uint8_t MOOD_BIG_REFINE_PLUS = 8;

	inline bool OnBigRefineFailure(TMood& m, uint8_t targetPlus)
	{
		if (targetPlus < MOOD_BIG_REFINE_PLUS || IsMoodLocked(m) || m.mood <= MOOD_SLABY)
			return false;
		--m.mood;
		return true;
	}

	// The fifth death at a player's hands inside a quarter of an hour. The
	// euphoria lock outranks it: the document says nothing may change that
	// mood for its three hours, and a capitulation is a change.
	inline bool OnCapitulation(TMood& m)
	{
		if (m.lockKind == MOOD_LOCK_EUPHORIA && m.lockLeftMs > 0)
			return false;
		m.mood = MOOD_SLABY;
		m.lockKind = MOOD_LOCK_CAPITULATION;
		m.lockLeftMs = MOOD_CAPITULATION_MS;
		m.droughtMs = 0;
		return true;
	}

	// The mood the bot plays by. In a party or a dungeon it can still change
	// in the background, but the bot plays NORMALNY until it leaves.
	inline uint8_t EffectiveMood(const TMood& m, bool inGroupOrDungeon)
	{
		return inGroupOrDungeon ? (uint8_t)MOOD_NORMALNY : m.mood;
	}

	// SLABY's two habits: a pause of 2-8 seconds between one pack and the
	// next, and every 10-30 minutes a stop of 2-5 minutes - the player gone to
	// the kitchen.
	const uint32_t PAUSE_MIN_MS = 2000u;
	const uint32_t PAUSE_MAX_MS = 8000u;
	const uint32_t AFK_EVERY_MIN_MS = 10u * 60u * 1000u;
	const uint32_t AFK_EVERY_MAX_MS = 30u * 60u * 1000u;
	const uint32_t AFK_MIN_MS = 2u * 60u * 1000u;
	const uint32_t AFK_MAX_MS = 5u * 60u * 1000u;

	inline uint32_t RollBetween(uint32_t low, uint32_t high, uint32_t roll)
	{
		return high <= low ? low : low + roll % (high - low + 1u);
	}

	inline uint32_t PauseDuration(uint32_t roll) { return RollBetween(PAUSE_MIN_MS, PAUSE_MAX_MS, roll); }
	inline uint32_t AfkInterval(uint32_t roll) { return RollBetween(AFK_EVERY_MIN_MS, AFK_EVERY_MAX_MS, roll); }
	inline uint32_t AfkDuration(uint32_t roll) { return RollBetween(AFK_MIN_MS, AFK_MAX_MS, roll); }

	// ---------------------------------------------------------------------
	// The Grinder's tiers. What the document gives is a place and a band for
	// each tier and, for the first three, the level the bot holds itself at:
	// fifteen in the first village, twenty-three on the cursed ground of M3
	// (the level-30 weapons), thirty to thirty-five in the second village.
	// Past that it names only "the best level" of a map - 36 to 50 for the
	// valley and the desert - and Mount Sohan with no band at all, so those
	// locks are spread by pid over the upper part of the map's band, which is
	// where a player farming it would stand. Past the last tier a Grinder
	// holds wherever it is. The Monkey Dungeon is not a band of its own but a
	// choice of ground inside the others.
	//
	// The travel code places the villages and the frontier by level, so a bot
	// held at those tiers' locks stays on their maps without being told to.
	// M3 is not placed by level - the level sends 19 to 25 to the second
	// village - so the second tier has a road of its own
	// (IsPlayerBotM3TierGrinder in playerbot_travel.h).
	// ---------------------------------------------------------------------
	struct TGrinderTier
	{
		uint8_t tier;
		uint8_t minLevel;
		uint8_t maxLevel;
		uint8_t lockMin;
		uint8_t lockMax;
	};

	// MT2009_PLUS_PROGRESSION_V1: the table is the operator's now - the
	// panel's "Progresja botow" writes /opt/m2spool/playerbot_progression.tsv
	// and playerbot_progression.h copies its "tier" lines in here (and the
	// defaults below back on a reset). A tier switched off holds nobody.
	inline TGrinderTier GRINDER_TIERS[] = {
		// Community Patch 1 (Iwakura, 20 September) widened the first two: a
		// tier that stopped every bot on the same level made a first village
		// of bots all of fifteen, which is not what a village looks like.
		// Each level of the range is as likely as any other - the lock is
		// MixPid over it - so the band fills evenly rather than at its ends.
		// The band a tier is farmed on is unchanged - tier 1 is the first
		// village, which ends where M3 begins - and only where the bot
		// stops inside it moved. A lock of 19 therefore carries the bot to
		// the last level of the village and no further.
		{ 1, 10, 18, 13, 19 },  // first village
		{ 2, 19, 25, 19, 25 },  // M3, the cursed animals and the level-30 weapons
		{ 3, 26, 35, 30, 35 },  // second village
		{ 5, 36, 50, 40, 48 },  // Orc Valley and the Yongbi Desert
		{ 7, 51, 65, 55, 62 },  // Mount Sohan
	};
	const unsigned int GRINDER_TIER_COUNT = sizeof(GRINDER_TIERS) / sizeof(GRINDER_TIERS[0]);
	inline bool GRINDER_TIER_HOLDS[GRINDER_TIER_COUNT] = { true, true, true, true, true };
	// Under this the bot is still learning to walk: no lock at all.
	const uint8_t GRINDER_FREE_BELOW = 10;
	// "25% botow moze pominac farmienie M1 na 13. poziomie, aby od razu
	// skupic sie na Tierze 2 (M3)" - Community Patch 1. Such a bot is held
	// nowhere in the first village: it walks through tier 1 and stops for the
	// first time in the M3 band, which is where its tier 2 lock puts it. The
	// share is drawn by pid, so it is the same bot every time it logs in.
	inline uint8_t GRINDER_TIER1_SKIP_PERCENT = 25;   // MT2009_PLUS_PROGRESSION_V1: the panel's
	const uint8_t GRINDER_TIER1_SKIP_LEVEL = 13;
	// The tier number the document never names, for everything past Sohan.
	const uint8_t GRINDER_TIER_BEYOND = 8;

	inline uint32_t MixPid(uint32_t pid, uint32_t salt)
	{
		uint32_t h = pid ^ salt;
		h ^= h >> 16;
		h *= 0x85ebca6bu;
		h ^= h >> 13;
		h *= 0xc2b2ae35u;
		h ^= h >> 16;
		return h;
	}

	inline uint8_t GrinderTierFor(uint8_t level)
	{
		if (level < GRINDER_FREE_BELOW)
			return 0;
		for (unsigned int i = 0; i < GRINDER_TIER_COUNT; ++i)
			if (level >= GRINDER_TIERS[i].minLevel && level <= GRINDER_TIERS[i].maxLevel)
				return GRINDER_TIERS[i].tier;
		return GRINDER_TIER_BEYOND;
	}

	// The level a Grinder of this level holds at: the tier's lock, spread by
	// pid inside the tier's lock range, or the bot's own level when it has
	// already passed that (it holds where it stands). Zero is no lock.
	// True for the quarter of the bots that do not stop in the first village.
	inline bool SkipsFirstVillage(uint32_t pid)
	{
		return MixPid(pid, 0x534b4950u) % 100u < (uint32_t)GRINDER_TIER1_SKIP_PERCENT;
	}

	// Iwakura's community patch 2, point 2. Of all bots, GRINDER_NEVER_HOLDS
	// percent never hold at a tier's lock ("calkowicie pomija zatrzymywanie
	// sie na okreslonych poziomach"), and GRINDER_MAY_QUIT percent may give
	// grinding up: at each tier, once its gear stands (the Law of Advancement
	// met at the lock), GRINDER_QUIT_CHANCE percent of them do, for good. One
	// draw by pid for both, so the two shares are the sheet's exactly and
	// never overlap. Both kinds get their upgrades mainly off the market.
	const uint8_t GRINDER_NEVER_HOLDS_PERCENT = 13;
	const uint8_t GRINDER_MAY_QUIT_PERCENT = 10;
	const uint8_t GRINDER_QUIT_CHANCE_PERCENT = 33;

	inline uint32_t GrinderStyleRoll(uint32_t pid)
	{
		return MixPid(pid, 0x5354594cu) % 100u;
	}

	inline bool NeverHoldsAtLocks(uint32_t pid)
	{
		return GrinderStyleRoll(pid) < (uint32_t)GRINDER_NEVER_HOLDS_PERCENT;
	}

	inline bool MayQuitGrinding(uint32_t pid)
	{
		const uint32_t roll = GrinderStyleRoll(pid);
		return roll >= (uint32_t)GRINDER_NEVER_HOLDS_PERCENT &&
				roll < (uint32_t)(GRINDER_NEVER_HOLDS_PERCENT + GRINDER_MAY_QUIT_PERCENT);
	}

	inline bool RollQuitGrinding(uint32_t roll)
	{
		return roll % 100u < (uint32_t)GRINDER_QUIT_CHANCE_PERCENT;
	}

	// The lock a tier draws for this pid, spread evenly over the tier's lock
	// range (see the salt below).
	inline uint8_t GrinderDrawFor(const TGrinderTier& t, uint32_t pid)
	{
		const uint32_t salt = 0x4c4f434bu + (uint32_t)t.tier * 0x9e3779b1u;
		return (uint8_t)(t.lockMin + MixPid(pid, salt) % (uint32_t)(t.lockMax - t.lockMin + 1));
	}

	inline uint8_t GrinderLockFor(uint8_t level, uint32_t pid)
	{
		if (level < GRINDER_FREE_BELOW)
			return 0;
		// A tier whose lock range reaches past its band holds a bot that drew
		// a lock past it, up to that lock. Tier 1 draws 13-19 and its band ends
		// at 18: a bot that drew 19 came to 19 in tier 2's band, where tier 2's
		// draw decided, and went on - 2.3% of the bots stopped at 19 where the
		// draws say about 14% (B17 of Iwakura's audit of 26 September).
		for (unsigned int i = 0; i < GRINDER_TIER_COUNT; ++i)
		{
			const TGrinderTier& t = GRINDER_TIERS[i];
			if (level <= t.maxLevel || level > t.lockMax || !GRINDER_TIER_HOLDS[i])
				continue;
			if (t.tier == 1 && SkipsFirstVillage(pid))
				continue;
			const uint8_t drawn = GrinderDrawFor(t, pid);
			if (drawn >= level)
				return drawn;
		}
		for (unsigned int i = 0; i < GRINDER_TIER_COUNT; ++i)
		{
			const TGrinderTier& t = GRINDER_TIERS[i];
			if (level < t.minLevel || level > t.maxLevel)
				continue;
			// MT2009_PLUS_PROGRESSION_V1: a tier the operator switched off.
			if (!GRINDER_TIER_HOLDS[i])
				return 0;
			// The quarter that walks through the first village: no lock here
			// at all from the level the document names, so the bot carries on
			// to M3 and is held there for the first time.
			if (t.tier == 1 && level >= GRINDER_TIER1_SKIP_LEVEL && SkipsFirstVillage(pid))
				return 0;
			// The draw is salted with the tier, and that is not decoration.
			// With one salt for every tier the *offset* into the range is the
			// same number in each of them, and tier 1 (13-19) and tier 2
			// (19-25) are both seven levels wide - so a bot that drew offset
			// six drew 19 in the first village, which its band (10-18) can
			// never reach, walked out of the village unheld, entered tier 2 at
			// 19 and drew offset six again: 25, the top of that tier. Every
			// seventh bot in the world therefore stopped on exactly the same
			// level. Counted over the seeded cohort (pid 4..2503, the pure
			// functions run outside the engine): 15.6% of the bots stopped at
			// 25 against 3.4% at each of 19..24, where Community Patch 1 says
			// the M3 lock is "losowana rowno". Iwakura saw it as bots massing
			// on a level his document never names (20 September). With a salt
			// per tier the same count reads 4.8-5.4% across 19..25.
			const uint8_t lock = GrinderDrawFor(t, pid);
			return lock < level ? level : lock;
		}
		// Past the last tier the table has nothing to say, and answering with
		// the bot's own level made every level a lock: a bot that arrived at 71
		// was frozen at 71, on a number no tier names. Measured in the whole of
		// m2zip's history - locks at 63, 64, 65 ... up to 112 of them at 71,
		// where the last tier ends at 62. The document's tiers stop at Mount
		// Sohan, so above it a Grinder is not held at all (20 September).
		return 0;
	}

	// ---------------------------------------------------------------------
	// Where a Grinder's written lock stands against today's draw: the one
	// answer GetPlayerBotPersonaLockLevel acts on and logs. `written` is the
	// lock the bot carries (zero for none); `pinned` says the Law of
	// Advancement wrote it - a Conqueror dying to monsters three times in half
	// an hour goes back to grinding, held where it fell, until its gear meets
	// the law for that level; `quit` is a Grinder that gave grinding up
	// (community patch 2, point 2).
	//
	// A pinned lock is the law's and not the draw's, so no redraw raises it and
	// no rule lifts it. Without the pin the demotion held nobody: at 26-29 and
	// 36-39 the redraw raised the lock at once to the tier's draw above the
	// bot (30-35, 40-48), and past the last tier the lift took it off (B04 of
	// Iwakura's audit of 26 September).
	// ---------------------------------------------------------------------
	enum EGrinderLockChange
	{
		GRINDER_LOCK_UNCHANGED,
		GRINDER_LOCK_LIFTED_QUIT,
		GRINDER_LOCK_LIFTED_NEVER_HOLDS,
		GRINDER_LOCK_LIFTED_NO_TIER,
		GRINDER_LOCK_REDRAWN,
		GRINDER_LOCK_REACHED,
	};

	struct TGrinderLockAnswer
	{
		// The level the bot is held at once it reaches it, zero for none.
		uint8_t hold;
		// The lock it carries from now on.
		uint8_t written;
		// EGrinderLockChange: what happened to `written`, for the log.
		uint8_t change;
	};

	inline TGrinderLockAnswer ResolveGrinderLock(uint8_t written, bool pinned, bool quit,
			uint8_t level, uint32_t pid)
	{
		TGrinderLockAnswer a = { 0, written, GRINDER_LOCK_UNCHANGED };
		if (pinned && written != 0)
		{
			a.hold = written;
			return a;
		}
		// The Grinders who never hold, and the ones who gave grinding up: no
		// lock, and one written before goes.
		if (NeverHoldsAtLocks(pid) || quit)
		{
			if (written != 0)
			{
				a.written = 0;
				a.change = quit ? GRINDER_LOCK_LIFTED_QUIT : GRINDER_LOCK_LIFTED_NEVER_HOLDS;
			}
			return a;
		}
		const uint8_t lock = GrinderLockFor(level, pid);
		// MT2009_PLUS_PROGRESSION_V1: a lock the operator's table no longer
		// allows - its tier switched off, or the lock outside the tier's range
		// after an edit - is drawn again from today's table (or lifted).
		if (written != 0)
		{
			const uint8_t wt = GrinderTierFor(written);
			bool allowed = false;
			for (unsigned int i = 0; i < GRINDER_TIER_COUNT; ++i)
			{
				const TGrinderTier& t = GRINDER_TIERS[i];
				if (t.tier != wt && !(written > t.maxLevel && written <= t.lockMax))
					continue;
				if (GRINDER_TIER_HOLDS[i] && written >= t.lockMin && written <= t.lockMax)
					allowed = true;
			}
			if (!allowed)
			{
				a.written = lock != 0 && level >= lock ? lock : 0;
				a.change = a.written != 0 ? GRINDER_LOCK_REDRAWN : GRINDER_LOCK_LIFTED_NO_TIER;
				a.hold = lock;
				return a;
			}
		}
		// Today's draw holds this bot nowhere - past the last tier, or one of
		// the quarter that walks through the first village - so a lock written
		// under an older rule goes.
		if (written != 0 && lock == 0)
		{
			a.written = 0;
			a.change = GRINDER_LOCK_LIFTED_NO_TIER;
			return a;
		}
		if (written != 0)
		{
			// A lock written under an older rule is raised to today's draw,
			// and only raised, and only inside the tier the bot is in: lowering
			// it would hand a bot a level it has already passed.
			if (lock > written && GrinderTierFor(written) == GrinderTierFor(level))
			{
				a.written = lock;
				a.change = GRINDER_LOCK_REDRAWN;
			}
			a.hold = a.written;
			return a;
		}
		if (lock != 0 && level >= lock)
		{
			a.written = lock;
			a.change = GRINDER_LOCK_REACHED;
		}
		a.hold = lock;
		return a;
	}

	// ---------------------------------------------------------------------
	// The Law of Advancement ("Prawo Awansu"): a Grinder never moves on to a
	// harder tier without a weapon at +7, an armour at +6 and a shield at +6,
	// and "zgodnie z obowiazujaca Tierlista" - each of them a piece for about
	// the bot's own level, not a +9 bought at level ten. A piece counts while
	// its level limit is within AWANS_LEVEL_WINDOW of the bot; the special
	// weapons of level thirty (his PVE 4-5 list) are good for longer, which is
	// what every player of this world does with them.
	// ---------------------------------------------------------------------
	const uint8_t AWANS_WEAPON_PLUS = 7;
	const uint8_t AWANS_ARMOUR_PLUS = 6;
	const uint8_t AWANS_SHIELD_PLUS = 6;
	// "Dla botow od 35. poziomu wzwyz optymalnym rozwiazaniem jest posiadanie
	// broni ulepszonej przynajmniej na +8 oraz zbroi, tarczy i maski/helmu na
	// minimum +6. Pozniejsze etapy gry wymagaja znacznie wiekszej defensywy"
	// (Community Patch 1). The helmet joins the law there and not before: the
	// merchants of the first villages sell none a bot of ten could wear, and
	// asking for one would hold every young Grinder at its tier for ever.
	const uint8_t AWANS_HARD_FROM_LEVEL = 35;
	const uint8_t AWANS_WEAPON_PLUS_HARD = 8;
	const uint8_t AWANS_HELMET_PLUS = 6;
	inline uint8_t AWANS_LEVEL_WINDOW = 20;           // MT2009_PLUS_PROGRESSION_V1: the panel's
	inline uint8_t AWANS_PREMIUM_LEVEL_WINDOW = 30;   // MT2009_PLUS_PROGRESSION_V1: the panel's
	// MT2009_PLUS_PROGRESSION_V1: the law by level, the operator's table
	// ("law" lines of playerbot_progression.tsv): from a level on, the plus a
	// weapon, an armour, a shield and a helmet need - zero asks nothing of
	// that piece. The row with the highest `fromLevel` at or under the bot's
	// level applies. The constants above were one row up to 35 and another
	// from 35 (7/6/6/- and 8/6/6/6); the defaults are milder in the villages,
	// where a +7 held a bot of fifteen for hours (a 50% refine that eats the
	// weapon and wants a drop), and the same as before from 35.
	struct TLawRow
	{
		uint8_t fromLevel;
		uint8_t weapon;
		uint8_t armour;
		uint8_t shield;
		uint8_t helmet;
	};
	const unsigned int AWANS_LAW_MAX = 8;
	inline TLawRow AWANS_LAW[AWANS_LAW_MAX] = {
		{ 0, 5, 4, 0, 0 },
		{ 19, 6, 5, 4, 0 },
		{ 26, 7, 5, 5, 0 },
		{ 35, 8, 6, 6, 6 },
	};
	inline unsigned int AWANS_LAW_COUNT = 4;

	inline TLawRow AwansLawFor(uint8_t level)
	{
		TLawRow best = { 0, 0, 0, 0, 0 };
		for (unsigned int i = 0; i < AWANS_LAW_COUNT && i < AWANS_LAW_MAX; ++i)
			if (AWANS_LAW[i].fromLevel <= level && AWANS_LAW[i].fromLevel >= best.fromLevel)
				best = AWANS_LAW[i];
		return best;
	}
	// A shield is asked for its +6 and not for its level. The document wants
	// one "jak najbardziej zblizona do aktualnego poziomu", and this world
	// cannot supply that: the merchants sell the level-0 Bojowa Tarcza and
	// nothing else, and measured on m2zip on 19 September the whole world held
	// 1353 shields of level 0, 68 of 21, 95 of 41, two of 61 - so a window
	// would have held every bot past forty at its lock for good. The
	// equipment pass still wears the best shield the bot finds.
	const bool AWANS_SHIELD_ANY_LEVEL = true;
	// M3's entry for a Grinder: a weapon at +6 and an armour at +5. The Mental
	// Warrior's Strong Body stands in for the armour, so it goes in with the
	// weapon alone.
	const uint8_t M3_WEAPON_PLUS = 6;
	const uint8_t M3_ARMOUR_PLUS = 5;

	struct TGearPiece
	{
		bool present;
		uint8_t plus;
		uint8_t levelLimit;
		// A piece that stays good past the ordinary window.
		bool premium;
		TGearPiece() : present(false), plus(0), levelLimit(0), premium(false) {}
		TGearPiece(uint8_t p, uint8_t limit, bool prem = false) :
			present(true), plus(p), levelLimit(limit), premium(prem) {}
	};

	struct TAdvanceGear
	{
		uint8_t level;
		TGearPiece weapon;
		TGearPiece armour;
		TGearPiece shield;
		// Asked for from AWANS_HARD_FROM_LEVEL, and ignored below it.
		TGearPiece helmet;
		// A bow and a two-handed weapon leave the shield slot empty for good.
		bool wantsShield;
		TAdvanceGear() : level(1), wantsShield(true) {}
	};

	// What is missing, as bits: zero is the law met.
	const int AWANS_GAP_WEAPON = 1;
	const int AWANS_GAP_ARMOUR = 2;
	const int AWANS_GAP_SHIELD = 4;
	const int AWANS_GAP_HELMET = 8;

	inline bool IsPieceCurrent(const TGearPiece& p, uint8_t level)
	{
		const unsigned int window = p.premium ? AWANS_PREMIUM_LEVEL_WINDOW : AWANS_LEVEL_WINDOW;
		return p.present && (unsigned int)p.levelLimit + window >= (unsigned int)level;
	}

	inline int AwansGaps(const TAdvanceGear& g)
	{
		// MT2009_PLUS_PROGRESSION_V1: the thresholds are the law's row for
		// this level (AwansLawFor); a zero asks nothing of that piece.
		int gaps = 0;
		const TLawRow law = AwansLawFor(g.level);
		if (law.weapon != 0 && (!IsPieceCurrent(g.weapon, g.level) || g.weapon.plus < law.weapon))
			gaps |= AWANS_GAP_WEAPON;
		if (law.helmet != 0 && (!IsPieceCurrent(g.helmet, g.level) || g.helmet.plus < law.helmet))
			gaps |= AWANS_GAP_HELMET;
		if (law.armour != 0 && (!IsPieceCurrent(g.armour, g.level) || g.armour.plus < law.armour))
			gaps |= AWANS_GAP_ARMOUR;
		const bool shieldCurrent = g.shield.present &&
				(AWANS_SHIELD_ANY_LEVEL || IsPieceCurrent(g.shield, g.level));
		if (law.shield != 0 && g.wantsShield && (!shieldCurrent || g.shield.plus < law.shield))
			gaps |= AWANS_GAP_SHIELD;
		return gaps;
	}

	inline bool MeetsM3Survival(const TAdvanceGear& g, bool mentalWarrior)
	{
		if (!g.weapon.present || g.weapon.plus < M3_WEAPON_PLUS)
			return false;
		return mentalWarrior || (g.armour.present && g.armour.plus >= M3_ARMOUR_PLUS);
	}

	// A Conqueror that dies this often to monsters has outgrown its gear:
	// "bot za czesto ginie i traci expa". It goes back to grinding, held at
	// the level it has reached.
	const uint8_t WEAK_DEATHS = 3;
	const uint32_t WEAK_WINDOW_MS = 30u * 60u * 1000u;

	// The last WEAK_DEATHS deaths to monsters, as ages in the same elapsed-play
	// clock the moods use. NoteDeath returns true when this death makes the
	// bot weak.
	struct TDeathWindow
	{
		uint32_t agoMs[WEAK_DEATHS];
		uint8_t count;
		TDeathWindow() : count(0)
		{
			for (unsigned int i = 0; i < WEAK_DEATHS; ++i)
				agoMs[i] = 0;
		}
	};

	inline void AdvanceDeathWindow(TDeathWindow& w, uint32_t dtMs)
	{
		uint8_t kept = 0;
		for (uint8_t i = 0; i < w.count; ++i)
		{
			const uint32_t age = SaturatingAdd(w.agoMs[i], dtMs);
			if (age < WEAK_WINDOW_MS)
				w.agoMs[kept++] = age;
		}
		w.count = kept;
	}

	inline bool NoteDeath(TDeathWindow& w)
	{
		if (w.count == WEAK_DEATHS)
		{
			for (unsigned int i = 1; i < WEAK_DEATHS; ++i)
				w.agoMs[i - 1] = w.agoMs[i];
			w.count = WEAK_DEATHS - 1;
		}
		w.agoMs[w.count++] = 0;
		return w.count >= WEAK_DEATHS;
	}

	// ---------------------------------------------------------------------
	// The gambler (Hazardzista). Each item it takes to the anvil gets its own
	// ambition - +7 six times in ten, +8 three, +9 one - and is worked the
	// document's way: the plain blacksmith up to +7, the Blessing Scroll for
	// +8 and +9 and nothing else. A scroll that fails hands the item back a
	// grade down; after that the item is taken back to +7 at the anvil and
	// sold as it is, whatever it was meant to reach.
	// ---------------------------------------------------------------------
	const int GAMBLE_BUDGET_PERCENT = 40;
	// The Perfectionist's share of the purse for its gear: 80 ("max 80% yang")
	// until Iwakura's Patch 4, point 2 - "50% zgromadzonych Yang przeznaczy na
	// ulepszanie ekwipunku, a 50% na rozwoj postaci poprzez zakup Ksiag
	// Umiejetnosci" (PLAYERBOT_BOOK_VISIT_BUDGET_PERCENT is the other half).
	const int PERFECT_BUDGET_PERCENT = 50;
	const uint8_t GAMBLE_SAFE_PLUS = 7;

	inline uint8_t RollGambleTarget(uint32_t roll)
	{
		const uint32_t r = roll % 100u;
		return r < 60u ? 7 : (r < 90u ? 8 : 9);
	}

	enum EGambleStep
	{
		GAMBLE_STEP_PLAIN = 0,   // the ordinary blacksmith
		GAMBLE_STEP_SCROLL,      // under a Blessing Scroll
		GAMBLE_STEP_DONE         // for sale as it stands
	};

	inline EGambleStep NextGambleStep(uint8_t plus, uint8_t target, bool scrollFailed)
	{
		if (scrollFailed)
			return plus < GAMBLE_SAFE_PLUS ? GAMBLE_STEP_PLAIN : GAMBLE_STEP_DONE;
		if (plus < GAMBLE_SAFE_PLUS)
			return GAMBLE_STEP_PLAIN;
		if (plus < target)
			return GAMBLE_STEP_SCROLL;
		return GAMBLE_STEP_DONE;
	}

	// What a gambler or a perfectionist may still spend: its share of the
	// purse it came in with, less what it has already spent. Never negative.
	inline long long BudgetLeft(long long goldAtStart, int sharePercent, long long spent)
	{
		const long long budget = goldAtStart > 0 ? goldAtStart * sharePercent / 100 : 0;
		return spent >= budget ? 0 : budget - spent;
	}

	// ---------------------------------------------------------------------
	// The stone hunter (Pogromca metinow). A Metin in sight within ten levels
	// either way claims the bot; three bots of its own kingdom on one already
	// are enough and it goes back to what it was doing; below 35% health it
	// turns on the stone's monsters and then comes back; and a stone that has
	// killed it more than six times is given up.
	// ---------------------------------------------------------------------
	const int POGROMCA_LEVEL_BAND = 10;
	const int POGROMCA_KINGDOM_CROWD = 3;
	const int POGROMCA_RETREAT_HP_PERCENT = 35;
	const unsigned int POGROMCA_MAX_DEATHS = 6;

	inline bool InPogromcaBand(int botLevel, int stoneLevel)
	{
		const int delta = stoneLevel - botLevel;
		return delta <= POGROMCA_LEVEL_BAND && delta >= -POGROMCA_LEVEL_BAND;
	}

	inline bool IsPogromcaCrowded(int sameKingdomBots)
	{
		return sameKingdomBots >= POGROMCA_KINGDOM_CROWD;
	}

	inline bool IsPogromcaRetreat(int hp, int maxHp)
	{
		return maxHp > 0 && (long long)hp * 100 < (long long)maxHp * POGROMCA_RETREAT_HP_PERCENT;
	}

	inline bool PogromcaGivesUp(unsigned int deathsAtStone)
	{
		return deathsAtStone > POGROMCA_MAX_DEATHS;
	}

	// ---------------------------------------------------------------------
	// The Anti-PK protocol. One death at a player's hands is an incident; the
	// fifth inside fifteen minutes on the same ground is harassment, and the
	// bot capitulates: SLABY for forty-five minutes (OnCapitulation), the
	// ground given up for as long, and - outside a party, from level thirty -
	// an eight percent chance of an hour at the water instead.
	// ---------------------------------------------------------------------
	const unsigned int PK_DEATHS = 5;
	// Deaths remembered, more than the five that count: a death somewhere
	// else in between must not push one of the five out of the window.
	const unsigned int PK_MEMORY = 10;
	const uint32_t PK_WINDOW_MS = 15u * 60u * 1000u;
	const long PK_SPOT_RADIUS = 5000;
	const uint32_t PK_CAPITULATION_MS = MOOD_CAPITULATION_MS;
	const uint32_t PK_FISHING_PERMILLE = 80u;
	const int PK_FISHING_MIN_LEVEL = 30;
	const uint32_t PK_FISHING_MS = 60u * 60u * 1000u;

	struct TPkDeaths
	{
		unsigned int count;
		uint32_t atMs[PK_MEMORY];
		long map[PK_MEMORY];
		long x[PK_MEMORY];
		long y[PK_MEMORY];
		TPkDeaths() : count(0)
		{
			for (unsigned int i = 0; i < PK_MEMORY; ++i)
			{
				atMs[i] = 0;
				map[i] = 0;
				x[i] = 0;
				y[i] = 0;
			}
		}
	};

	inline bool IsPkSameSpot(long mapA, long xA, long yA, long mapB, long xB, long yB)
	{
		if (mapA != mapB)
			return false;
		const long long dx = (long long)xA - xB;
		const long long dy = (long long)yA - yB;
		return dx * dx + dy * dy <= (long long)PK_SPOT_RADIUS * PK_SPOT_RADIUS;
	}

	// A death at a player's hands at nowMs (a millisecond clock that may wrap):
	// deaths older than the window are forgotten, this one is kept, and the
	// answer is whether it makes the fifth inside the window on the same ground
	// - in which case the window starts afresh, so the next capitulation needs
	// five more.
	inline bool NotePkDeath(TPkDeaths& w, uint32_t nowMs, long mapIndex, long x, long y)
	{
		unsigned int kept = 0;
		for (unsigned int i = 0; i < w.count; ++i)
		{
			if ((uint32_t)(nowMs - w.atMs[i]) >= PK_WINDOW_MS)
				continue;
			w.atMs[kept] = w.atMs[i];
			w.map[kept] = w.map[i];
			w.x[kept] = w.x[i];
			w.y[kept] = w.y[i];
			++kept;
		}
		w.count = kept;
		if (w.count == PK_MEMORY)
		{
			for (unsigned int i = 1; i < PK_MEMORY; ++i)
			{
				w.atMs[i - 1] = w.atMs[i];
				w.map[i - 1] = w.map[i];
				w.x[i - 1] = w.x[i];
				w.y[i - 1] = w.y[i];
			}
			w.count = PK_MEMORY - 1;
		}
		w.atMs[w.count] = nowMs;
		w.map[w.count] = mapIndex;
		w.x[w.count] = x;
		w.y[w.count] = y;
		++w.count;
		unsigned int near = 0;
		for (unsigned int i = 0; i < w.count; ++i)
			if (IsPkSameSpot(w.map[i], w.x[i], w.y[i], mapIndex, x, y))
				++near;
		if (near < PK_DEATHS)
			return false;
		w = TPkDeaths();
		return true;
	}

	// ---------------------------------------------------------------------
	// Iwakura's Rybak by mood (Community Patch 2, URGENT 2): the chance, in
	// per mille, that a bot of this mood takes up the rod in its window, at a
	// FISHING weight in percent of the neutral one. At the neutral weight and
	// under it each mood keeps its own odds, scaled - SLABY 750, NORMALNY 40,
	// BARDZO DOBRY none, the document's "bardzo duza szansa" and
	// "sporadycznie". Over it the population's share is the neutral share
	// times the weight, and what a mood's hundred percent cannot take goes to
	// the next mood up. Scaled mood by mood, SLABY was full at 134% and the
	// slider's 200% gave 1.37 times the anglers - 26% of the bots at 100%, 36%
	// at 200% (B11 of Iwakura's audit of 26 September). `mix` is the last
	// census of the moods; with none yet, every mood is only scaled.
	// ---------------------------------------------------------------------
	const uint32_t RYBAK_BASE_PERMILLE[MOOD_COUNT] = { 750, 40, 0 };
	// Never every bot of a core at the water, whatever the slider says: what
	// spills over from a full mood stops here.
	const uint32_t RYBAK_SHARE_CAP_PERMILLE = 900;

	inline uint32_t RybakChancePermille(uint8_t mood, uint32_t weightPercent,
			const uint32_t mix[MOOD_COUNT])
	{
		if (mood >= MOOD_COUNT)
			return 0;
		uint64_t natural[MOOD_COUNT];
		uint64_t total = 0, base = 0, sumNatural = 0;
		for (int m = 0; m < MOOD_COUNT; ++m)
		{
			natural[m] = std::min<uint64_t>(1000,
					(uint64_t)RYBAK_BASE_PERMILLE[m] * weightPercent / 100);
			total += mix[m];
			base += (uint64_t)RYBAK_BASE_PERMILLE[m] * mix[m];
			sumNatural += natural[m] * mix[m];
		}
		if (weightPercent <= 100 || total == 0)
			return (uint32_t)natural[mood];
		// In per mille of a bot, summed over the census: what the weight asks
		// for, and what the scaled odds alone give.
		const uint64_t target = std::min<uint64_t>(base * weightPercent / 100,
				total * RYBAK_SHARE_CAP_PERMILLE);
		uint64_t extra = target > sumNatural ? target - sumNatural : 0;
		uint64_t chance[MOOD_COUNT];
		for (int m = 0; m < MOOD_COUNT; ++m)
		{
			chance[m] = natural[m];
			if (extra == 0 || mix[m] == 0)
				continue;
			const uint64_t room = (1000 - natural[m]) * mix[m];
			const uint64_t give = std::min<uint64_t>(extra, room);
			chance[m] += give / mix[m];
			extra -= give;
		}
		return (uint32_t)chance[mood];
	}

	// The eight percent: from level thirty, never in a party ("przez pobyt w PT
	// nie ma 8% szansy").
	inline bool RollCapitulationFishing(uint32_t roll, int level, bool inParty)
	{
		return !inParty && level >= PK_FISHING_MIN_LEVEL && roll % 1000u < PK_FISHING_PERMILLE;
	}

	// ---------------------------------------------------------------------
	// The companion (Towarzysz). The PARTY slider sets the share of the
	// population that plays in a party, as it always has; under Iwakura's
	// system that share is drawn afresh at every phase ("im wyzsza wartosc na
	// tym suwaku, tym czesciej boty decyduja sie na zmiane osobowosci na
	// Towarzysza"), so every bot is a companion some of the time rather than
	// one bot in five for life. A draw is 0..999 and admits the bot while it
	// is under the slider's share in thousandths. A Shaman's draw is cut to a
	// third ("znacznie wyzsza wrodzona szansa"), and so, less, are the drawn
	// companion character's and the party fighter's. A phase holds 45 to 90
	// minutes while the bot is solo and stands still while it is in a party,
	// which ends by the document's own conditions; the few minutes after a
	// party are played alone.
	// ---------------------------------------------------------------------
	const uint32_t COMPANION_PHASE_MIN_MS = 45u * 60u * 1000u;
	const uint32_t COMPANION_PHASE_MAX_MS = 90u * 60u * 1000u;
	const uint32_t COMPANION_BREAK_MIN_MS = 3u * 60u * 1000u;
	const uint32_t COMPANION_BREAK_MAX_MS = 8u * 60u * 1000u;
	const unsigned int COMPANION_DRAW_RANGE = 1000u;
	// A draw nobody has rolled yet admits nobody.
	const uint16_t COMPANION_DRAW_NONE = 1000u;
	const unsigned int COMPANION_SHAMAN_DRAW_PERCENT = 35u;
	const unsigned int COMPANION_CHARACTER_DRAW_PERCENT = 60u;
	const unsigned int COMPANION_FIGHTER_DRAW_PERCENT = 25u;

	inline uint16_t CompanionDraw(uint32_t roll, bool shaman, bool companionCharacter, bool partyFighter)
	{
		uint32_t draw = roll % COMPANION_DRAW_RANGE;
		if (shaman)
			draw = draw * COMPANION_SHAMAN_DRAW_PERCENT / 100u;
		if (companionCharacter)
			draw = draw * COMPANION_CHARACTER_DRAW_PERCENT / 100u;
		if (partyFighter)
			draw = draw * COMPANION_FIGHTER_DRAW_PERCENT / 100u;
		return (uint16_t)draw;
	}

	inline bool IsCompanionDraw(uint16_t draw, int cohortPerMille)
	{
		return draw < COMPANION_DRAW_NONE && (int)draw < cohortPerMille;
	}

	// ---------------------------------------------------------------------
	// The mercenary (Najemnik). A bot that keeps dying to monsters - three
	// deaths in half an hour, the same count that tells a Conqueror it has
	// outgrown its gear - is somebody a stronger bot of its kingdom on the
	// same map may carry for money: a higher level ("wyzszy poziom") and much
	// better gear ("znacznie lepszy ekwipunek - wyzsze Tiery/plusy"), within
	// the thirty levels the engine lets a party span. An hour costs 250 000
	// through the yang curve, paid up front; the client keeps a quarter of its
	// purse for its potions (the first cut kept seven tenths, and at a yang
	// rate of 3000% - 7.5 million an hour - no bot under forty could hire
	// anybody: the seven in distress on m2zip found no offer between them).
	// At the hour it pays again unless it has had
	// what it came for - three levels, a full bag, or its own gear raised -
	// or can no longer pay, or the mercenary no longer outclasses it.
	// ---------------------------------------------------------------------
	const uint32_t MERC_CONTRACT_MS = 60u * 60u * 1000u;
	const uint32_t MERC_BASE_PRICE = 250000u;
	const int MERC_LEVEL_LEAD = 3;
	const int MERC_PARTY_LEVEL_GAP = 30;
	const int MERC_GEAR_LEAD_PERCENT = 25;
	const int MERC_GEAR_LEAD_MIN = 30;
	// A piece's weight: its level limit, three points a plus, six a step of
	// Iwakura's tier away from the neutral three (a family his list does not
	// carry counts as neutral).
	const int MERC_POWER_PER_PLUS = 3;
	const int MERC_POWER_PER_TIER = 6;
	const int MERC_NEUTRAL_TIER = 3;
	const int MERC_CLIENT_PURSE_PERCENT = 75;
	const int MERC_CLIENT_GOAL_LEVELS = 3;
	const int MERC_CLIENT_GEAR_GAIN = 12;
	const unsigned int MERC_CLIENT_DEATHS = WEAK_DEATHS;

	struct TMercPiece
	{
		bool present;
		uint8_t plus;
		uint8_t levelLimit;
		uint8_t tier;
		TMercPiece() : present(false), plus(0), levelLimit(0), tier(0) {}
		TMercPiece(uint8_t p, uint8_t limit, uint8_t t) : present(true), plus(p), levelLimit(limit), tier(t) {}
	};

	// Weapon, body, helmet, shield, shoes, bracelet, necklace, earrings.
	const unsigned int MERC_GEAR_SLOTS = 8u;
	struct TMercGear
	{
		TMercPiece piece[MERC_GEAR_SLOTS];
	};

	inline int MercGearPower(const TMercGear& g)
	{
		int power = 0;
		for (unsigned int i = 0; i < MERC_GEAR_SLOTS; ++i)
		{
			const TMercPiece& p = g.piece[i];
			if (!p.present)
				continue;
			power += (int)p.levelLimit + MERC_POWER_PER_PLUS * (int)p.plus;
			if (p.tier != 0)
				power += MERC_POWER_PER_TIER * ((int)p.tier - MERC_NEUTRAL_TIER);
		}
		return power < 0 ? 0 : power;
	}

	inline bool MercOutclasses(int mercLevel, int mercPower, int clientLevel, int clientPower)
	{
		if (mercLevel < clientLevel + MERC_LEVEL_LEAD || mercLevel - clientLevel > MERC_PARTY_LEVEL_GAP)
			return false;
		return mercPower >= clientPower + MERC_GEAR_LEAD_MIN &&
				(long long)mercPower * 100 >= (long long)clientPower * (100 + MERC_GEAR_LEAD_PERCENT);
	}

	inline bool MercClientCanPay(long long gold, long long reserve, long long price)
	{
		return price > 0 && gold - reserve >= price &&
				price * 100 <= gold * MERC_CLIENT_PURSE_PERCENT;
	}

	inline bool MercClientGoalReached(int levelsGained, bool bagFull, int gearGain)
	{
		return levelsGained >= MERC_CLIENT_GOAL_LEVELS || bagFull || gearGain >= MERC_CLIENT_GEAR_GAIN;
	}

	inline bool MercClientRenews(bool goalReached, bool canPay, bool stillOutclassed)
	{
		return !goalReached && canPay && stillOutclassed;
	}

	// ---------------------------------------------------------------------
	// The Useful Items List (LPP): what a bot keeps at the storekeeper rather
	// than sells. Jewellery and boots of tier 3 to 6; the weapons his level
	// bands name, of tier 3 at least; the level-61 shields with resistances
	// and the armours of his "70 lvl"; and any piece with a line of tier 5 or 6
	// rolled at least half-way up ("Wysoka Wartosc" - a line of that class at
	// the bottom of its roll is not what he means by one). Of each family
	// the bot keeps two for its own class, one for another class (the
	// gambler's trade), and one of his level-15 and level-20 weapons. His
	// first document let a piece of jewellery or a shield have three; his
	// correction of 23 September makes two the most of any kind, counted over
	// the bag and the box together ("maksymalnie 2 sztuki danego typu ...
	// bo w ekwipunku i magazynie"). And the list is the gambler's alone
	// (GamblerByNature below). A family
	// the bot already wears at +9 needs no backups ("Zasada Osiagnietej
	// Perfekcji"): its plain copies go to the market, the ones worth keeping
	// for their lines stay. And what the bot has outgrown goes to the market
	// too ("Dezaktualizacja sprzetu"): a weapon of a band the bot has passed,
	// anything else twenty levels under it; the target shields and armours
	// and the last band's weapons never.
	// ---------------------------------------------------------------------
	enum ELppKind
	{
		LPP_NONE = 0,
		LPP_JEWEL,
		LPP_WEAPON,
		LPP_SHIELD,
		LPP_ARMOUR,
		LPP_VALUE
	};
	const int LPP_JEWEL_MIN_TIER = 3;
	const int LPP_WEAPON_MIN_TIER = 3;
	const int LPP_VALUE_MIN_BONUS_TIER = 5;
	const int LPP_VALUE_MIN_ROLL_PERCENT = 50;
	const int LPP_OWN_GEAR_LIMIT = 2;
	const int LPP_OWN_SMALL_LIMIT = 2;
	const int LPP_OTHER_CLASS_LIMIT = 1;
	const int LPP_ONLY_ONE_LIMIT = 1;
	const int LPP_OUTGROWN_LEVELS = AWANS_LEVEL_WINDOW;
	// Iwakura's Patch 3, point 3: the whole stock, not a family, has a
	// ceiling too - "lacznie maksymalnie 18 sztuk".
	const int LPP_TOTAL_LIMIT = 18;
	const uint8_t LPP_PERFECT_PLUS = 9;

	struct TLppPiece
	{
		uint8_t kind;
		uint8_t level;
		// A weapon's band on his list: 1 for "5-29 lvl", 2 for "30 lvl+", 3
		// for "65 lvl+"; and the level of the next band's weapons, at which a
		// band is passed (0 for the last).
		uint8_t band;
		uint8_t nextBandLevel;
		bool onlyOne;
		// Its class may wear it (the anti-flags); jewellery and a shield are
		// the small pieces with the larger keep.
		bool ownClass;
		bool small;
		// A target piece is never outgrown: the shields and armours he names
		// are what a bot keeps an armour or a shield for.
		bool target;
		TLppPiece() : kind(LPP_NONE), level(0), band(0), nextBandLevel(0), onlyOne(false),
			ownClass(false), small(false), target(false) {}
	};

	inline int LppLimit(const TLppPiece& p, bool wornAtNine)
	{
		if (p.kind == LPP_NONE)
			return 0;
		if (wornAtNine && p.kind != LPP_VALUE)
			return 0;
		if (!p.ownClass)
			return LPP_OTHER_CLASS_LIMIT;
		if (p.onlyOne)
			return LPP_ONLY_ONE_LIMIT;
		return p.small ? LPP_OWN_SMALL_LIMIT : LPP_OWN_GEAR_LIMIT;
	}

	inline bool LppObsolete(const TLppPiece& p, int botLevel)
	{
		if (p.kind == LPP_NONE)
			return true;
		if (p.target)
			return false;
		if (p.kind == LPP_WEAPON)
		{
			// A band is passed when the next band's weapons can be worn - and
			// not before the piece itself is twenty levels behind: his first
			// band names two bells of level 32 and 36 in this world.
			return p.nextBandLevel != 0 && botLevel >= (int)p.nextBandLevel &&
					botLevel > (int)p.level + LPP_OUTGROWN_LEVELS;
		}
		return botLevel > (int)p.level + LPP_OUTGROWN_LEVELS;
	}

	// Whether this piece keeps its place, with `keptAhead` better copies of its
	// family kept already (in the box, and ahead of it in the bag).
	inline bool LppKeeps(const TLppPiece& p, int botLevel, bool wornAtNine, int keptAhead)
	{
		return !LppObsolete(p, botLevel) && keptAhead < LppLimit(p, wornAtNine);
	}

	// A line that makes a piece "Wysoka Wartosc": tier 5 or 6 on his list and
	// rolled at least LPP_VALUE_MIN_ROLL_PERCENT of the way to its top.
	inline bool LppValueLine(int bonusTier, long value, long maxRoll)
	{
		return bonusTier >= LPP_VALUE_MIN_BONUS_TIER && maxRoll > 0 && value > 0 &&
				(long long)value * 100 >= (long long)maxRoll * LPP_VALUE_MIN_ROLL_PERCENT;
	}

	// Whose list it is. "Dalem to tylko na HAZARDZISTE" (Iwakura, 23
	// September): what the list gathers is the gambler's stock, pieces to
	// raise and sell, so only a bot that is a gambler by nature keeps it.
	// His gambler is a turn a bot's evening takes, not a bot, and a stock is
	// gathered over days, so the nature is drawn by pid (`hash`) in the very
	// share its character already turns gambler at when a visit ends with a
	// heavy purse (`chance`, in percent): half the traders, a third of the
	// wanderers, one careful collector in twenty. Every other bot sells what
	// the list names by the ordinary rules, and a plain armour goes to the
	// merchant ("te zbroje nadaja sie do handlarza").
	inline bool GamblerByNature(uint32_t hash, int chance)
	{
		return chance > 0 && (int)(hash % 100U) < chance;
	}

	// What the box lets go of. Every piece of gear in it comes with its
	// family, its rank (the plus first, then the lines), how many of its
	// family the bot may hold, bag and box together, and whether the bot has
	// outgrown it. An outgrown piece goes; of the rest each family keeps its
	// best `limit` - a tie to the earlier position - and the others go. A
	// limit of zero lets a whole family go, which is what every piece of the
	// list is to a bot that is no gambler.
	struct TLppBoxPiece
	{
		uint32_t id;
		uint32_t family;
		int rank;
		int limit;
		bool obsolete;
		// What the piece is worth as stock (Iwakura's sheet at the gambler's
		// +7): which pieces the total keeps when the families keep more.
		long long value;
	};

	// `release` gets the ids to take out, and `kept` (when asked) how many
	// stay. `pieces` is taken by value because it is sorted; it comes in box
	// order, which is what breaks a tie. The families choose first - each
	// keeps its best `limit` - and then the whole box keeps no more than
	// `totalLimit` of what they chose, the most valuable first.
	inline void PlanLppBoxRelease(std::vector<TLppBoxPiece> pieces, std::vector<uint32_t>& release,
			int totalLimit = LPP_TOTAL_LIMIT, int* kept = NULL)
	{
		release.clear();
		std::vector<size_t> order(pieces.size());
		for (size_t i = 0; i < order.size(); ++i)
			order[i] = i;
		std::stable_sort(order.begin(), order.end(),
				[&pieces](size_t a, size_t b)
				{
					return pieces[a].family != pieces[b].family ? pieces[a].family < pieces[b].family
							: pieces[a].rank > pieces[b].rank;
				});
		std::vector<size_t> survivors;
		uint32_t family = 0;
		int keptOfFamily = 0;
		for (size_t n = 0; n < order.size(); ++n)
		{
			const TLppBoxPiece& piece = pieces[order[n]];
			if (n == 0 || piece.family != family)
			{
				family = piece.family;
				keptOfFamily = 0;
			}
			if (!piece.obsolete && keptOfFamily < piece.limit)
			{
				++keptOfFamily;
				survivors.push_back(order[n]);
				continue;
			}
			release.push_back(piece.id);
		}
		std::stable_sort(survivors.begin(), survivors.end(),
				[&pieces](size_t a, size_t b)
				{
					return pieces[a].value != pieces[b].value ? pieces[a].value > pieces[b].value : a < b;
				});
		const size_t total = totalLimit < 0 ? 0 : (size_t)totalLimit;
		for (size_t n = total; n < survivors.size(); ++n)
			release.push_back(pieces[survivors[n]].id);
		if (kept)
			*kept = (int)std::min(total, survivors.size());
	}

	// The soul stones he keeps: +3 of PvE tier 3 at least, for the early gear
	// before the scrolls, and every +4 ("najcenniejsze zasoby"), for the final
	// set - a class stone's +4 too, which waits for a PvP weapon; a few of each.
	const int LPP_STONE_MIN_GRADE = 3;
	const int LPP_STONE_TOP_GRADE = 4;
	const int LPP_STONE_MIN_PVE_TIER = 3;
	const int LPP_STONE_KEEP = 5;

	inline bool LppKeepsStone(int grade, int pveTier, int heldAhead)
	{
		if (heldAhead >= LPP_STONE_KEEP)
			return false;
		return grade >= LPP_STONE_TOP_GRADE ||
				(grade >= LPP_STONE_MIN_GRADE && pveTier >= LPP_STONE_MIN_PVE_TIER);
	}

	// ---------------------------------------------------------------------
	// Iwakura's Patch 3, point 7: the rare personalities. Each is a state of
	// its own length that outranks the bot's situation, drawn one bot at a
	// time: at every draw each bot that qualifies wins it one time in
	// `oneIn`, no more than RareCap of a kind run at once, and after one
	// begins none of its kind begins for `worldPauseMin` minutes - his world
	// cooldown, which the Metinolog has none of.
	// ---------------------------------------------------------------------
	enum ERare
	{
		RARE_NONE = 0,
		RARE_METINOLOG,
		RARE_NALOGOWIEC,
		RARE_NAUKOWIEC,
		RARE_EGZEKUTOR,
		RARE_WEDKARZ,
		// Community Patch 5, point 1 (TGamblerTerms below).
		RARE_HAZ_MLODSZY,
		RARE_HAZ_STARSZY,
		RARE_HAZ_NACZELNY,
		RARE_HAZ_SZALONY,
		RARE_COUNT
	};

	struct TRareRule
	{
		uint8_t persona;
		uint32_t oneIn;
		uint32_t worldPauseMin;
		uint32_t minMinutes;
		uint32_t maxMinutes;
		// The Metinolog's "maksymalnie 1 bot na 300 spelniajacych warunki":
		// its cap grows with the bots that qualify; so do the four gamblers',
		// in the same words. Every other kind runs one at a time, which its
		// world pause spaces.
		bool capByEligible;
	};

	// His numbers. The addict's and the gamblers' sessions and the
	// scientist's trip end the state themselves; their minutes are only its
	// ceiling. Community Patch 5 moved the Egzekutor to one in 400 and five
	// hours (point 12) and gave the four gamblers theirs (point 1): one in
	// 250, 300, 350 and 1000, four, six, eight and sixteen hours - his
	// "cooldown" is the world pause, as the Egzekutor's always was.
	inline TRareRule GetRareRule(uint8_t rare)
	{
		switch (rare)
		{
			case RARE_METINOLOG:    return TRareRule{ PERSONA_METINOLOG, 300, 0, 120, 250, true };
			case RARE_NALOGOWIEC:   return TRareRule{ PERSONA_NALOGOWIEC, 1000, 240, 180, 180, false };
			case RARE_NAUKOWIEC:    return TRareRule{ PERSONA_NAUKOWIEC, 500, 480, 90, 90, false };
			case RARE_EGZEKUTOR:    return TRareRule{ PERSONA_EGZEKUTOR, 400, 300, 120, 120, false };
			case RARE_WEDKARZ:      return TRareRule{ PERSONA_WEDKARZ, 600, 720, 360, 360, false };
			// Iwakura: the gamblers more often - one in 200, 250,
			// 300 and 400 of the bots that qualify, pauses of two, four,
			// eight and eight hours.
			case RARE_HAZ_MLODSZY:  return TRareRule{ PERSONA_HAZ_MLODSZY, 200, 120, 180, 180, true };
			case RARE_HAZ_STARSZY:  return TRareRule{ PERSONA_HAZ_STARSZY, 250, 240, 180, 180, true };
			case RARE_HAZ_NACZELNY: return TRareRule{ PERSONA_HAZ_NACZELNY, 300, 480, 180, 180, true };
			case RARE_HAZ_SZALONY:  return TRareRule{ PERSONA_HAZ_SZALONY, 400, 480, 240, 240, true };
			default:                return TRareRule{ PERSONA_GRINDER, 0, 0, 0, 0, false };
		}
	}

	// How many of a kind may run at once: one for every `oneIn` of the bots
	// that qualify for a Metinolog - and one while any does, or a world of a
	// hundred would never meet him - and one of every other kind.
	inline uint32_t RareCap(const TRareRule& r, uint32_t eligible)
	{
		if (eligible == 0 || r.oneIn == 0)
			return 0;
		return r.capByEligible ? std::max<uint32_t>(1, eligible / r.oneIn) : 1;
	}

	// Whether one more of a kind may begin: under its cap, and past its world
	// pause since the last one began (`started` is whether one ever has).
	inline bool RareMayStart(const TRareRule& r, uint32_t running, uint32_t eligible,
			uint32_t nowMin, uint32_t lastStartMin, bool started)
	{
		if (running >= RareCap(r, eligible))
			return false;
		return r.worldPauseMin == 0 || !started || nowMin - lastStartMin >= r.worldPauseMin;
	}

	// One qualifying bot's draw: one time in `oneIn`.
	inline bool RareDrawWins(uint32_t roll, const TRareRule& r)
	{
		return r.oneIn > 0 && roll % r.oneIn == 0;
	}

	inline uint32_t RareMinutes(const TRareRule& r, uint32_t roll)
	{
		if (r.maxMinutes <= r.minMinutes)
			return r.minMinutes;
		return r.minMinutes + roll % (r.maxMinutes - r.minMinutes + 1);
	}

	// ---------------------------------------------------------------------
	// Iwakura's Community Patch 5, point 1: the Hazardzista is gone ("dzialala
	// slabo lub wcale i byla trudna do zbalansowania"), and four rare gamblers
	// work the anvil in its place. Each is drawn only among the richest share
	// of the world's characters (`topPercent`), stakes `budgetPercent` of the
	// purse it began with on the bases it buys and on the anvil, and buys
	// `buyMin` to `buyMax` bases at +0..+5 when its bag holds none; the
	// Szalony works every category at once (`everything`).
	// ---------------------------------------------------------------------
	struct TGamblerTerms
	{
		uint8_t topPercent;
		uint8_t budgetPercent;
		uint8_t buyMin;
		uint8_t buyMax;
		bool everything;
	};

	inline bool IsRareGambler(uint8_t rare)
	{
		return rare >= RARE_HAZ_MLODSZY && rare <= RARE_HAZ_SZALONY;
	}

	inline TGamblerTerms GetGamblerTerms(uint8_t rare)
	{
		switch (rare)
		{
			case RARE_HAZ_MLODSZY:  return TGamblerTerms{ 60, 80, 1, 3, false };
			case RARE_HAZ_STARSZY:  return TGamblerTerms{ 40, 70, 2, 5, false };
			case RARE_HAZ_NACZELNY: return TGamblerTerms{ 25, 60, 3, 6, false };
			// "Jesli nie posiada zadnych przedmiotow spelniajacych te warunki,
			// zachowuje sie jak Naczelny Hazardzista" - three to six.
			case RARE_HAZ_SZALONY:  return TGamblerTerms{ 15, 90, 3, 6, true };
			default:                return TGamblerTerms{ 0, 0, 0, 0, false };
		}
	}

	// How many bases a gambler whose bag holds none buys, out of a roll.
	inline uint8_t GamblerBuyCount(const TGamblerTerms& t, uint32_t roll)
	{
		if (t.buyMax <= t.buyMin)
			return t.buyMin;
		return (uint8_t)(t.buyMin + roll % (uint32_t)(t.buyMax - t.buyMin + 1));
	}

	// The four categories of his list, each from its own level: weapons from
	// thirty, body armour from twenty-six, shields and helmets from
	// twenty-one, and the jewellery and boots from twenty-two - except the
	// pieces his list names, which count whatever their level.
	enum EGambleCategory
	{
		GAMBLE_CAT_WEAPON = 0,
		GAMBLE_CAT_ARMOUR,
		GAMBLE_CAT_SHIELD_HELMET,
		GAMBLE_CAT_JEWEL,
		GAMBLE_CAT_COUNT
	};
	const uint8_t GAMBLE_CAT_ALL = (1u << GAMBLE_CAT_COUNT) - 1u;

	inline int GambleCategoryMinLevel(int category)
	{
		switch (category)
		{
			case GAMBLE_CAT_WEAPON: return 30;
			case GAMBLE_CAT_ARMOUR: return 26;
			case GAMBLE_CAT_SHIELD_HELMET: return 21;
			case GAMBLE_CAT_JEWEL: return 22;
			default: return 255;
		}
	}

	// Which categories a session works, as a mask, out of how many pieces fit
	// for the anvil the bag holds of each: the category it holds most of,
	// and every other category it holds as many of ("jezeli w ekwipunku bota
	// znajduja sie np. 2 bronie i 2 zbroje ... moze polaczyc te dwie
	// kategorie"); every category that holds any for the Szalony. None when
	// the bag holds nothing to work - the gambler buys first.
	inline uint8_t ChooseGambleCategories(const unsigned int counts[GAMBLE_CAT_COUNT], bool everything)
	{
		unsigned int best = 0;
		for (int i = 0; i < GAMBLE_CAT_COUNT; ++i)
			best = std::max(best, counts[i]);
		if (best == 0)
			return 0;
		uint8_t mask = 0;
		for (int i = 0; i < GAMBLE_CAT_COUNT; ++i)
			if (counts[i] > 0 && (everything || counts[i] == best))
				mask |= (uint8_t)(1u << i);
		return mask;
	}

	// Each piece's ambition: +7 seven times in ten, +8 and +9 fifteen each.
	inline uint8_t RollRareGambleTarget(uint32_t roll)
	{
		const uint32_t r = roll % 100u;
		return r < 70u ? 7 : (r < 85u ? 8 : 9);
	}

	// From which grade the gamblers take a Blessing Scroll: the steps to +7,
	// +8 and +9 ("domyslnie korzystaja z nich dopiero przy ulepszaniu na
	// poziomy od +7 do +9").
	const uint8_t RARE_GAMBLE_SCROLL_TO_PLUS = 7;

	// A step of the gamblers: the plain anvil up to +6, and each step to +7,
	// +8 and +9 under a Blessing Scroll while the bag holds one and at the
	// plain anvil when it does not - "moga uzywac Zwojow Blogoslawienstwa,
	// lecz nie musza". A scroll that fails hands the piece back a grade down
	// and the piece is worked on; the engine side stops a piece after its
	// failures (PLAYERBOT_RARE_GAMBLE_SCROLL_FAILS).
	inline EGambleStep NextRareGambleStep(uint8_t plus, uint8_t target, bool scrollInBag)
	{
		if (plus >= target || plus >= 9)
			return GAMBLE_STEP_DONE;
		if (plus + 1u >= RARE_GAMBLE_SCROLL_TO_PLUS && scrollInBag)
			return GAMBLE_STEP_SCROLL;
		return GAMBLE_STEP_PLAIN;
	}

	// Whether a purse ranks among the richest `topPercent` of the world's
	// characters: `sortedDesc` is every character's gold, richest first; the
	// bar is the purse at the edge of the share, so a tie at the edge is in.
	inline long long WealthBar(const std::vector<long long>& sortedDesc, unsigned int topPercent)
	{
		if (sortedDesc.empty() || topPercent == 0)
			return -1;
		size_t within = (sortedDesc.size() * std::min(topPercent, 100u) + 99u) / 100u;
		if (within == 0)
			within = 1;
		return sortedDesc[within - 1];
	}

	// ---------------------------------------------------------------------
	// Which personality claims the bot. A contract is the strongest claim, a
	// party the next - both are commitments to somebody else - then the two
	// sessions that hold a tool in the weapon hand, the stone under the
	// hammer, the two errands at the anvil, the full bag, and last what the
	// bot does when nothing asks: grind, or level if it has earned it.
	// ---------------------------------------------------------------------
	struct TPersonaSignals
	{
		bool mercenary;   // hired and paid for, as the strong side
		bool inParty;     // a party of any kind, the mercenary's excepted
		// The weaker side of a contract: in the mercenary's party, and not a
		// companion - it paid for that party and plays by its own lights.
		bool hired;
		bool fishing;
		bool mining;
		// MT2009_PLUS_BOTLIFE_V1: on its way to Baek-Go's board or at it.
		bool herbalism;
		bool stoneFight;
		bool gambling;
		bool perfecting;
		bool trading;
		bool advanced;
		// A rare personality running now (its EPersona id), or 0.
		uint8_t rare;
		TPersonaSignals() : mercenary(false), inParty(false), hired(false), fishing(false), mining(false),
			herbalism(false), stoneFight(false), gambling(false), perfecting(false), trading(false), advanced(false), rare(0) {}
	};

	inline uint8_t DecidePersona(const TPersonaSignals& s)
	{
		if (s.mercenary)
			return PERSONA_NAJEMNIK;
		if (s.inParty && !s.hired)
			return PERSONA_TOWARZYSZ;
		// A rare personality is what the bot is for its whole length, the rod
		// or the stone in front of it included; only a contract and a party,
		// both somebody else's claim, come before it.
		if (s.rare != 0)
			return s.rare;
		if (s.fishing)
			return PERSONA_RYBAK;
		if (s.mining)
			return PERSONA_GORNIK;
		if (s.herbalism)
			return PERSONA_ZIELARZ;
		if (s.stoneFight)
			return PERSONA_POGROMCA;
		if (s.gambling)
			return PERSONA_HAZARDZISTA;
		if (s.perfecting)
			return PERSONA_PERFEKCJONISTA;
		if (s.trading)
			return PERSONA_HANDLARZ;
		return s.advanced ? (uint8_t)PERSONA_ZDOBYWCA : (uint8_t)PERSONA_GRINDER;
	}
}

#endif
