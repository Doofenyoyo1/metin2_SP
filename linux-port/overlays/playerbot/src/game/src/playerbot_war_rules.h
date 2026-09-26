#ifndef __INC_METIN2_PLAYERBOT_WAR_RULES_H__
#define __INC_METIN2_PLAYERBOT_WAR_RULES_H__

// How a bot guild fights a guild war, as pure policy.
//
// prodnathin's proposal of 26 September, "Zachowania poszczegolnych klas na
// wojnach", taken as he wrote it: a healing Shaman buffs its side at the
// start and after every regroup, then keeps out of reach and heals whoever
// is low - or, the offensive one, bursts the ninjas and the black-magic
// suras; a dragon Shaman buffs the side and then goes into the middle for
// its splash; the mental warrior is out of the camp first, always, and the
// body warrior and the weapon sura go in as before; the black-magic sura
// hunts the ninjas, then the healers, then its own kind; the archer keeps its
// distance and shoots the black magic, the healers and the ninjas; the dagger
// ninja goes unseen and takes the healers, the shamans and the suras. "Fajnie
// jakby takich patternow bylo nie wiem z 4-8 i gildia je losowala
// randomowo" - so a guild draws one of six patterns for each war, and the
// pattern moves those roles about. And the rest of that message: the bots
// leave the camp one by one ("x bot wyruszy za 0.5 sekundy, inny za 2
// sekundy"), and a side that has knocked out the whole of the other goes
// back to its camp while the other stands up ("system, w ktorym jesli jedna
// z gildii pokona wszystkich przeciwnikow to jest cofana z powrotem do
// miejsca startowego i daje czas na zregenerowanie sie przeciwnej gildii").
//
// No engine types: the job and the skill group come in as the engine's
// numbers (JOB_WARRIOR 0, JOB_ASSASSIN 1, JOB_SURA 2, JOB_SHAMAN 3; group 1
// or 2). The engine side is playerbot_guild_war.h. Tested in
// tests/playerbot_war_rules_test.cpp.

#include <cmath>

namespace playerbot_war_rules
{
	// What a character fights as: its class and its path.
	enum EKind
	{
		KIND_WARRIOR_BODY = 0,
		KIND_WARRIOR_MENTAL,
		KIND_NINJA_DAGGER,
		KIND_NINJA_ARCHER,
		KIND_SURA_WEAPON,
		KIND_SURA_MAGIC,
		KIND_SHAMAN_DRAGON,
		KIND_SHAMAN_HEAL,
		KIND_COUNT
	};

	// A character with no path yet fights as the first one of its class.
	inline EKind KindOf(int job, int skillGroup)
	{
		const bool second = skillGroup == 2;
		switch (job)
		{
			case 1: return second ? KIND_NINJA_ARCHER : KIND_NINJA_DAGGER;
			case 2: return second ? KIND_SURA_MAGIC : KIND_SURA_WEAPON;
			case 3: return second ? KIND_SHAMAN_HEAL : KIND_SHAMAN_DRAGON;
			default: return second ? KIND_WARRIOR_MENTAL : KIND_WARRIOR_BODY;
		}
	}

	enum ERole
	{
		ROLE_FIGHTER = 0,	// into the middle at the nearest foe
		ROLE_TANK,			// the same, and out of the camp first
		ROLE_HEALER_GUARD,	// buffs, keeps out of reach, heals the low
		ROLE_HEALER_STRIKER,	// buffs, then the ninjas and the black magic
		ROLE_DRAGON,		// buffs, then its splash in the middle
		ROLE_HUNTER,		// the ninjas, the healers, the black magic
		ROLE_ARCHER,		// keeps its distance: the black magic, the healers, the ninjas
		ROLE_ASSASSIN,		// unseen: the healers, the shamans, the suras
		ROLE_COUNT
	};

	enum EPattern
	{
		PATTERN_CLASSIC = 0,	// the proposal as it stands
		PATTERN_GUARD,			// every healer keeps back and the archers cover them
		PATTERN_HEALER_HUNT,	// every hand at the enemy's healers first
		PATTERN_BLITZ,			// all at once, at the nearest, every healer strikes
		PATTERN_AMBUSH,			// the daggers first and unseen, the rest after them
		PATTERN_WALL,			// the mental warriors first, the rest well behind
		PATTERN_COUNT
	};

	inline unsigned int Mix(unsigned int a, unsigned int b)
	{
		unsigned int h = a * 2654435761u ^ (b + 0x9e3779b9u + (a << 6) + (a >> 2));
		h ^= h >> 15;
		h *= 2246822519u;
		h ^= h >> 13;
		h *= 3266489917u;
		h ^= h >> 16;
		return h;
	}

	// One pattern a guild for each war: the guild and the war's start.
	inline EPattern PickPattern(unsigned int guildId, unsigned int warStartedAt)
	{
		return (EPattern)(Mix(guildId, warStartedAt) % (unsigned int)PATTERN_COUNT);
	}

	inline const char* PatternName(EPattern p)
	{
		static const char* const names[PATTERN_COUNT] = {
			"klasyczny", "oslona uzdrowicieli", "polowanie na uzdrowicieli",
			"szturm", "zasadzka", "mur"
		};
		return p < PATTERN_COUNT ? names[p] : "?";
	}

	// The share of healing Shamans that strike rather than keep back, per
	// mille, by pattern: the proposal's two healers as a draw by pid.
	inline unsigned int HealerStrikerPerMille(EPattern p)
	{
		static const unsigned int shares[PATTERN_COUNT] = { 300, 0, 1000, 1000, 300, 0 };
		return p < PATTERN_COUNT ? shares[p] : 0;
	}

	inline ERole RoleOf(EKind kind, EPattern p, unsigned int pid)
	{
		switch (kind)
		{
			case KIND_WARRIOR_MENTAL: return ROLE_TANK;
			case KIND_NINJA_DAGGER: return ROLE_ASSASSIN;
			case KIND_NINJA_ARCHER: return ROLE_ARCHER;
			case KIND_SURA_MAGIC: return ROLE_HUNTER;
			case KIND_SHAMAN_DRAGON: return ROLE_DRAGON;
			case KIND_SHAMAN_HEAL:
				return Mix(pid, 0x4845414cu) % 1000u < HealerStrikerPerMille(p)
						? ROLE_HEALER_STRIKER : ROLE_HEALER_GUARD;
			default: return ROLE_FIGHTER;
		}
	}

	inline const char* RoleName(ERole r, bool en)
	{
		static const char* const pl[ROLE_COUNT] = {
			"wojownik", "tank", "uzdrowiciel w obronie", "uzdrowiciel w ataku",
			"smok", "lowca", "lucznik", "skrytobojca"
		};
		static const char* const english[ROLE_COUNT] = {
			"fighter", "tank", "defensive healer", "offensive healer",
			"dragon", "hunter", "archer", "assassin"
		};
		if (r >= ROLE_COUNT)
			return "?";
		return en ? english[r] : pl[r];
	}

	// What a priority list names a foe by.
	enum EFocus
	{
		FOCUS_NONE = 0,
		FOCUS_NINJA,	// either path
		FOCUS_DAGGER,
		FOCUS_HEALER,	// the healing Shaman
		FOCUS_SHAMAN,	// either path
		FOCUS_MAGIC,	// the black-magic sura
		FOCUS_SURA		// either path
	};

	inline bool FocusMatches(EFocus f, EKind foe)
	{
		switch (f)
		{
			case FOCUS_NINJA: return foe == KIND_NINJA_DAGGER || foe == KIND_NINJA_ARCHER;
			case FOCUS_DAGGER: return foe == KIND_NINJA_DAGGER;
			case FOCUS_HEALER: return foe == KIND_SHAMAN_HEAL;
			case FOCUS_SHAMAN: return foe == KIND_SHAMAN_DRAGON || foe == KIND_SHAMAN_HEAL;
			case FOCUS_MAGIC: return foe == KIND_SURA_MAGIC;
			case FOCUS_SURA: return foe == KIND_SURA_WEAPON || foe == KIND_SURA_MAGIC;
			default: return false;
		}
	}

	// A role's first, second and third choice under a pattern. The fighters,
	// the tank, the dragon and the defensive healer take the nearest.
	inline void FocusList(ERole role, EPattern p, EFocus out[3])
	{
		out[0] = out[1] = out[2] = FOCUS_NONE;
		if (p == PATTERN_BLITZ)
			return;
		switch (role)
		{
			case ROLE_HUNTER:
				out[0] = FOCUS_NINJA; out[1] = FOCUS_HEALER; out[2] = FOCUS_MAGIC;
				break;
			case ROLE_ARCHER:
				if (p == PATTERN_GUARD)
				{
					// The daggers are what comes for the healers it covers.
					out[0] = FOCUS_DAGGER; out[1] = FOCUS_MAGIC; out[2] = FOCUS_HEALER;
				}
				else
				{
					out[0] = FOCUS_MAGIC; out[1] = FOCUS_HEALER; out[2] = FOCUS_NINJA;
				}
				break;
			case ROLE_ASSASSIN:
				out[0] = FOCUS_HEALER; out[1] = FOCUS_SHAMAN; out[2] = FOCUS_SURA;
				break;
			case ROLE_HEALER_STRIKER:
				out[0] = FOCUS_NINJA; out[1] = FOCUS_MAGIC;
				break;
			default:
				break;
		}
		// Every hand at the healers first; the rest of each list after them.
		if (p == PATTERN_HEALER_HUNT && role != ROLE_HEALER_GUARD && role != ROLE_HEALER_STRIKER)
		{
			EFocus rest[3] = { out[0], out[1], out[2] };
			out[0] = FOCUS_HEALER;
			int n = 1;
			for (int i = 0; i < 3 && n < 3; ++i)
				if (rest[i] != FOCUS_NONE && rest[i] != FOCUS_HEALER)
					out[n++] = rest[i];
			while (n < 3)
				out[n++] = FOCUS_NONE;
		}
	}

	// How much nearer, in world units, a foe counts for being first, second or
	// third on the list: the foe finder weighs distance, the crowd already on a
	// foe and a draw, and this comes off the sum.
	const int FOCUS_BONUS[3] = { 1500, 1000, 500 };

	inline int FocusBonus(ERole role, EPattern p, EKind foe)
	{
		EFocus list[3];
		FocusList(role, p, list);
		for (int i = 0; i < 3; ++i)
			if (list[i] != FOCUS_NONE && FocusMatches(list[i], foe))
				return FOCUS_BONUS[i];
		return 0;
	}

	// When a bot leaves the camp after the muster or a regroup, in ms: one by
	// one, half a second to two, the tank always first; all at once in a
	// blitz; the daggers ahead of everybody in an ambush, the tanks ahead of
	// everybody behind a wall.
	inline unsigned int RunOutDelayMs(ERole role, EPattern p, unsigned int pid)
	{
		const unsigned int r = Mix(pid, 0x52554e4fu);
		switch (p)
		{
			case PATTERN_BLITZ:
				return r % 300u;
			case PATTERN_AMBUSH:
				return role == ROLE_ASSASSIN ? r % 300u : 2000u + r % 2000u;
			case PATTERN_WALL:
				return role == ROLE_TANK ? r % 300u : 2500u + r % 1500u;
			default:
				return role == ROLE_TANK ? r % 300u : 500u + r % 1500u;
		}
	}

	// The side's buffs from its Shamans at the camp: both paths buff.
	inline bool BuffsSide(ERole role)
	{
		return role == ROLE_HEALER_GUARD || role == ROLE_HEALER_STRIKER || role == ROLE_DRAGON;
	}

	// A round: one side has nobody up on the field while the other has
	// somebody, and both came. The side that won, 0 or 1, or -1.
	inline int RoundWinner(int up0, int present0, int up1, int present1)
	{
		if (present0 <= 0 || present1 <= 0)
			return -1;
		if (up0 == 0 && up1 > 0)
			return 1;
		if (up1 == 0 && up0 > 0)
			return 0;
		return -1;
	}

	// The distance from a point to the segment a-b: the field along the axis
	// of the two camps is a capsule round it.
	inline long DistanceToSegment(long px, long py, long ax, long ay, long bx, long by)
	{
		const double dx = (double)(bx - ax);
		const double dy = (double)(by - ay);
		const double len2 = dx * dx + dy * dy;
		double t = 0.0;
		if (len2 > 0.0)
		{
			t = ((double)(px - ax) * dx + (double)(py - ay) * dy) / len2;
			if (t < 0.0)
				t = 0.0;
			else if (t > 1.0)
				t = 1.0;
		}
		const double cx = (double)ax + t * dx - (double)px;
		const double cy = (double)ay + t * dy - (double)py;
		return (long)std::sqrt(cx * cx + cy * cy);
	}

	// Where a point `back` units behind `from` stands, away from `away`
	// (the flight of a defensive healer or an archer's step back), or towards
	// `to` when `away` is where it stands.
	inline void StepAway(long fromX, long fromY, long awayX, long awayY, long toX, long toY,
			long back, long& outX, long& outY)
	{
		double dx = (double)(fromX - awayX);
		double dy = (double)(fromY - awayY);
		double len = std::sqrt(dx * dx + dy * dy);
		if (len < 1.0)
		{
			dx = (double)(toX - fromX);
			dy = (double)(toY - fromY);
			len = std::sqrt(dx * dx + dy * dy);
		}
		if (len < 1.0)
		{
			outX = fromX;
			outY = fromY;
			return;
		}
		outX = fromX + (long)(dx / len * (double)back);
		outY = fromY + (long)(dy / len * (double)back);
	}
}

#endif
