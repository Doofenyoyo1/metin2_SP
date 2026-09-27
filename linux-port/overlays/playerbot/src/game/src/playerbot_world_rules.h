#ifndef __INC_METIN2_PLAYERBOT_WORLD_RULES_H__
#define __INC_METIN2_PLAYERBOT_WORLD_RULES_H__

// Pure world-travel policy.  Keep decisions which do not need CHARACTER or
// server singletons here so they can be tested without booting a game core.
// The manager remains responsible for performing the chosen transition.
namespace playerbot_world_rules
{
	enum EMonkeyExitDecision
	{
		MONKEY_STAY = 0,
		MONKEY_EXIT_RESTOCK,
		MONKEY_EXIT_MEDAL_READY,
		MONKEY_EXIT_TIMEOUT,
		MONKEY_EXIT_HORSE_COMPLETE
	};

	struct TMonkeyVisitContext
	{
		bool needsEssentialSupply;
		// Running out of red potions is just as much a reason to leave as losing
		// a weapon. Every other map reaches this through BlocksPlayerBotTravel;
		// the dungeon has its own rules, so it needs its own copy of the fact.
		bool needsPotions;
		int medalCount;
		int desiredMedalCount;
		bool visitExpired;
		bool canAdvanceHorse;
	};

	inline EMonkeyExitDecision DecideMonkeyExit(const TMonkeyVisitContext& context)
	{
		if (context.needsEssentialSupply || context.needsPotions)
			return MONKEY_EXIT_RESTOCK;
		if (!context.canAdvanceHorse)
			return MONKEY_EXIT_HORSE_COMPLETE;
		if (context.medalCount >= context.desiredMedalCount)
			return MONKEY_EXIT_MEDAL_READY;
		if (context.visitExpired)
			return MONKEY_EXIT_TIMEOUT;
		return MONKEY_STAY;
	}

	// The longest travel cooldown the travel pass sets is fifteen minutes
	// (playerbot_travel.h, number(300000, 900000)); an hour leaves room, and
	// a deadline further ahead than that is no cooldown at all.
	const unsigned int TRAVEL_COOLDOWN_MAX_MS = 60u * 60u * 1000u;

	// Both are get_dword_time(), milliseconds since the core started in a
	// 32-bit DWORD that wraps after 49.7 days. `now < next` read a cooldown set
	// in the last minutes before the wrap as long over, and after the wrap
	// every deadline set before it - nothing sets one back to zero while the
	// bot is in the world - as running for as long as the core had run when it
	// was set. The difference is what survives the wrap. The bound is what a
	// signed difference alone would lose: a deadline the pass has not read for
	// over 24.8 days (a bot back in its village after a month at the frontier)
	// would read as ahead again, and for up to as long.
	inline bool IsTravelCooldownActive(unsigned int now, unsigned int nextTravelTime)
	{
		const unsigned int ahead = nextTravelTime - now;
		return nextTravelTime != 0 && ahead != 0 && ahead <= TRAVEL_COOLDOWN_MAX_MS;
	}
}

#endif
