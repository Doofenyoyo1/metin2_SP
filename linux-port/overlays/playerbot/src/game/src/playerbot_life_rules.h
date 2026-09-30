#ifndef __INC_PLAYERBOT_LIFE_RULES_H__
#define __INC_PLAYERBOT_LIFE_RULES_H__

// MT2009_PLUS_BOTLIFE_V1: "Boty graja jak zywi ludzie" by the hour - the
// LIFE_HOURS key of the weights file, the panels' 0-24 slider under the LIFE
// switch. Pure policy, no engine types; the engine's half is
// CPlayerBotManager::ManageLifeSchedule.
//
// The hours are how long a bot plays in a day. A session is four hours, or
// the whole of a shorter day, and the rest after it is what makes the day
// add up: rest = session * (24 - hours) / hours. Twelve hours is three
// sessions of four with four hours off, six is one session of four and
// twelve hours off (one and a half sessions a day), two is one session of
// two and twenty-two off. Each session and each rest is drawn a quarter
// either way, so a cohort that started together drifts apart. Zero is the
// key unset - the free-running 3-6 hour sessions and 3-9 hour rests of
// before, which the engine side keeps - and twenty-four is no rest at all.
//
// The share of the cohort allowed to rest at once follows the day: the
// day's resting share and a tenth over it, so the draws have room and the
// world never empties further than the hours say.
#include <cstdint>

namespace playerbot_life
{
	const int HOURS_MAX = 24;
	const uint32_t HOUR_MS = 60u * 60u * 1000u;
	const uint32_t SESSION_MS = 4u * HOUR_MS;
	// A quarter either way.
	const uint32_t SPREAD_PERCENT = 25;
	// The resting share's headroom over the day's own share, in percent.
	const uint32_t RESTING_HEADROOM_PERCENT = 10;

	inline int ClampHours(long hours)
	{
		return hours < 0 ? 0 : (hours > HOURS_MAX ? HOURS_MAX : (int)hours);
	}

	// Whether the hours ask for sessions at all: set, and short of the day.
	inline bool Scheduled(int hours)
	{
		return hours > 0 && hours < HOURS_MAX;
	}

	// Whether the hours say "no rests" (the whole day).
	inline bool AllDay(int hours)
	{
		return hours >= HOURS_MAX;
	}

	// A session's nominal length: four hours, or the whole of a shorter day.
	inline uint32_t SessionMs(int hours)
	{
		if (!Scheduled(hours))
			return 0;
		const uint32_t day = (uint32_t)hours * HOUR_MS;
		return day < SESSION_MS ? day : SESSION_MS;
	}

	// The rest that makes the day add up after one session.
	inline uint32_t RestMs(int hours)
	{
		if (!Scheduled(hours))
			return 0;
		return (uint32_t)((uint64_t)SessionMs(hours) * (uint64_t)(HOURS_MAX - hours) / (uint64_t)hours);
	}

	// A nominal length drawn a quarter either way: `roll` is any 32-bit draw.
	inline uint32_t Spread(uint32_t nominal, uint32_t roll)
	{
		if (nominal == 0)
			return 0;
		const uint64_t low = (uint64_t)nominal * (100u - SPREAD_PERCENT) / 100u;
		const uint64_t width = (uint64_t)nominal * (2u * SPREAD_PERCENT) / 100u;
		return (uint32_t)(low + (width ? (uint64_t)roll % (width + 1u) : 0u));
	}

	// The most of the cohort that may rest at once, in percent.
	inline uint32_t MaxRestingPercent(int hours)
	{
		if (!Scheduled(hours))
			return 0;
		const uint32_t share = (uint32_t)(HOURS_MAX - hours) * 100u / (uint32_t)HOURS_MAX +
				RESTING_HEADROOM_PERCENT;
		return share > 100u ? 100u : share;
	}
}

#endif
