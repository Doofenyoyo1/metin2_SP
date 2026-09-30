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
// That made a war rounds, and DUDU's report of 28 September (2.2.35) what a
// round is now: the fallen take no part in it until the next one, a round
// cannot hang - one side down, both, or its clock - and a break at the
// camps comes between two, about thirty seconds for the bots to buff, as he
// asked. The phases, a bot's stance in each, who is in the fight, how a
// round ends and the break's end are below, with the rounds.
//
// No engine types: the job and the skill group come in as the engine's
// numbers (JOB_WARRIOR 0, JOB_ASSASSIN 1, JOB_SURA 2, JOB_SHAMAN 3; group 1
// or 2). The engine side is playerbot_guild_war.h. Tested in
// tests/playerbot_war_rules_test.cpp.

#include <algorithm>
#include <cmath>
#include <vector>

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

	// ------------------------------------------------------------ the rounds
	//
	// After its muster a war is rounds: each fought until one side has nobody
	// left in the fight, the fallen of both waiting at their own camps, then a
	// break with everybody back at the camps before the next. DUDU on 2.2.35:
	// after a round the two sides stood apart for fifteen minutes before the
	// fight went on, and paused again in the next kingdom's war; and the fight
	// that did go on moved onto one guild's camp, where the bots that had
	// fallen joined it again and again.

	enum EPhase
	{
		PHASE_MUSTER = 0,	// the war's first seconds, each side at its camp
		PHASE_BREAK,		// between two rounds, each side at its camp
		PHASE_ROUND			// the fight
	};

	// What one bot does in a phase.
	enum EStance
	{
		STANCE_FIGHT = 0,	// in the round, at any foe the field offers
		STANCE_CAMP,		// at its camp: the muster, the break, its own run-out delay
		STANCE_OUT			// fell in this round: at its camp until the round is over
	};

	inline EStance StanceOf(EPhase phase, bool outOfRound, bool runOutPending)
	{
		if (phase != PHASE_ROUND)
			return STANCE_CAMP;
		if (outOfRound)
			return STANCE_OUT;
		return runOutPending ? STANCE_CAMP : STANCE_FIGHT;
	}

	// How far from the centre of its camp a bot at the camp answers anybody,
	// 0 for the fight's no limit: the muster's reach while the camp holds a
	// side, and the camp's own circle for the fallen.
	inline long CampReach(EStance stance, long defendRange, long campRadius)
	{
		switch (stance)
		{
			case STANCE_CAMP: return defendRange;
			case STANCE_OUT: return campRadius;
			default: return 0;
		}
	}

	// Whether a bot in its stance may take this foe on. In the round anybody
	// the field offers. At the camp a person who has come within its reach,
	// and never a bot: the other side is at its own camp then, or walking back
	// to it past this one, and a fight with it was how the regroup after a
	// round turned into a brawl at the losers' camp. Out of the round the
	// same within the camp's own circle: the fallen stood "at the camp" in the
	// muster's sense and took on every foe within 900 of themselves, while
	// nobody may pick one out of the round - untouchable, they joined a fight
	// that drifted near their camp again and again. No bot is a foe to them
	// now, so a blow of the other side's lands on nobody who answers; a person
	// is never held, and one who walks into a camp of the waiting is answered
	// there and nowhere else.
	inline bool MayTakeFoe(EStance stance, bool foeIsPerson, long foeFromCamp, long reach)
	{
		if (stance == STANCE_FIGHT)
			return true;
		return foeIsPerson && foeFromCamp <= reach;
	}

	// Whether a bot standing up now sits the round out: it fell in this round.
	// One that fell before the round began - in the break, the muster, an
	// earlier round, or out hunting before the war drafted it - stands up at
	// its camp and plays. Both ages count back from now, so the wrap of the
	// core's clock does not matter.
	inline bool FellThisRound(EPhase phase, unsigned int sinceDeathMs, unsigned int sinceRoundStartMs)
	{
		return phase == PHASE_ROUND && sinceDeathMs <= sinceRoundStartMs;
	}

	// Who is in the fight: the one test for a round's count and for the foes a
	// bot may pick, which used to be two. One standing where no blow lands (the
	// safe zone) or off the field counted as up - its side had not lost the
	// round - while no foe would pick it.
	struct TFighter
	{
		bool dead;
		bool recovering;	// standing up, invisible, or out of the round
		bool safeZone;
		bool offField;
	};

	inline bool InTheFight(const TFighter& f)
	{
		return !f.dead && !f.recovering && !f.safeZone && !f.offField;
	}

	enum ERoundEnd
	{
		ROUND_GOES_ON = 0,
		ROUND_WON,			// one side has nobody left in the fight
		ROUND_DRAWN,		// the last of both went down together
		ROUND_STALLED,		// nobody went down for the stall time
		ROUND_TIMED_OUT,	// the round's longest
		ROUND_END_COUNT
	};

	inline const char* RoundEndName(ERoundEnd e)
	{
		static const char* const names[ROUND_END_COUNT] = { "on", "won", "drawn", "stalled", "timed_out" };
		return e < ROUND_END_COUNT ? names[e] : "?";
	}

	// How a round stands. There is none while a side has nobody on the
	// battlefield. It is won when one side has nobody left in the fight and
	// drawn when neither has, and it cannot hang: after stallMs with nobody
	// going down, or maxMs in all, it ends - to the side with more in the
	// fight, drawn on a tie. A round had no end but a whole side down, so one
	// bot nobody could reach (thrown into the rocks, off the field) held it
	// open while everybody else stood: the fifteen minutes of Shinsoo's first
	// war. winner is 0 or 1, and -1 for a draw and for a round that goes on.
	inline ERoundEnd DecideRound(int up0, int present0, int up1, int present1, unsigned int roundMs,
			unsigned int sinceFallMs, unsigned int maxMs, unsigned int stallMs, int& winner)
	{
		winner = -1;
		if (present0 <= 0 || present1 <= 0)
			return ROUND_GOES_ON;
		if (up0 <= 0 && up1 <= 0)
			return ROUND_DRAWN;
		winner = RoundWinner(up0, present0, up1, present1);
		if (winner >= 0)
			return ROUND_WON;
		const bool timedOut = maxMs > 0 && roundMs >= maxMs;
		const bool stalled = stallMs > 0 && sinceFallMs >= stallMs;
		if (!timedOut && !stalled)
			return ROUND_GOES_ON;
		winner = up0 > up1 ? 0 : (up1 > up0 ? 1 : -1);
		return timedOut ? ROUND_TIMED_OUT : ROUND_STALLED;
	}

	// Whether the break between two rounds is over: its length, once every bot
	// of the war stands up at its camp, and the extra time on top at most,
	// wherever anybody stands.
	inline bool BreakOver(unsigned int breakMs, int ready, int present, unsigned int lengthMs, unsigned int extraMs)
	{
		if (breakMs < lengthMs)
			return false;
		return ready >= present || breakMs - lengthMs >= extraMs;
	}

	// Where a bot in the fight may step back to: never into its own camp, which
	// is the fallen's while the round lasts - a defensive healer or an archer
	// stepped back towards it, its foe came after it, and the fight settled
	// among the waiting. A point inside the circle goes out onto it along the
	// line from the centre, the centre itself towards `to` (the middle); a
	// radius of 0 keeps every point.
	inline void KeepOutOfCircle(long x, long y, long cx, long cy, long radius, long toX, long toY,
			long& outX, long& outY)
	{
		double dx = (double)(x - cx);
		double dy = (double)(y - cy);
		double len = std::sqrt(dx * dx + dy * dy);
		if (radius <= 0 || len >= (double)radius)
		{
			outX = x;
			outY = y;
			return;
		}
		if (len < 1.0)
		{
			dx = (double)(toX - cx);
			dy = (double)(toY - cy);
			len = std::sqrt(dx * dx + dy * dy);
		}
		if (len < 1.0)
		{
			outX = x;
			outY = y;
			return;
		}
		outX = cx + std::lround(dx / len * (double)radius);
		outY = cy + std::lround(dy / len * (double)radius);
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

	// How many each side sends: the smaller roster, and no more than the cap
	// (0 = none). The strong guild's forty against the elite's twenty-four won
	// almost every war on a world with one of each a kingdom (DUDU, 26
	// September); "po 20v20, niekoniecznie od najwyzszego levela - losowi
	// zawodnicy" (prodnathin).
	inline int SideSize(int roster0, int roster1, int cap)
	{
		int n = roster0 < roster1 ? roster0 : roster1;
		if (cap > 0 && n > cap)
			n = cap;
		return n > 0 ? n : 0;
	}

	inline unsigned int DrawKey(unsigned int pid, unsigned int guildId, unsigned int warStartedAt)
	{
		return Mix(Mix(pid, 0x44524157u), guildId ^ warStartedAt);
	}

	// The first n of the war's draw, by a key the pid, the guild and the
	// war's start fix: the same n all war, a different n the next war, and one
	// who leaves lets the next of the draw in while nobody else moves.
	inline void CallSide(std::vector<unsigned int>& roster, unsigned int guildId, unsigned int warStartedAt, int n)
	{
		std::sort(roster.begin(), roster.end(), [&](unsigned int a, unsigned int b) {
			const unsigned int ka = DrawKey(a, guildId, warStartedAt), kb = DrawKey(b, guildId, warStartedAt);
			return ka != kb ? ka < kb : a < b;
		});
		if (n >= 0 && roster.size() > (size_t)n)
			roster.resize((size_t)n);
	}
}

#endif
