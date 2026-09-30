#ifndef __INC_METIN2_PLAYERBOT_GUILD_ORDER_RULES_H__
#define __INC_METIN2_PLAYERBOT_GUILD_ORDER_RULES_H__

// A person's orders to the bots of the person's own guild, as pure policy.
//
// Derpsonkowy95, 28 September: "poprawa komunikacji na linii gracz - bot (w
// przypadku gildii) ... potrzebujemy pomocy jako gracz przy zadymie/expie";
// the operator: yes, and as guild commands in the GUI. The guild window's
// "Boty gildii" button (uiguildbots.py in the client root) sends one of three
// words - "/gildia_boty pomoc", "exp" or "wracajcie" - and nothing else: the
// server takes where the person stands from the person's own character, never
// a coordinate from the client. playerbot_guild_orders.h is the engine side,
// and the walk and the stay are the whisper summon's ("chodz do mnie",
// playerbot_chat_conversation.h) with the order's own fight and clock.
//
// Who may order is the engine's own grade system, not a list of ours: the
// guild's master, and a member whose rank the master gave the right to use the
// guild's skills - the fourth box of the grade page (GUILD_AUTH_USE_SKILL),
// the right CGuild::UseSkill already asks before a war skill. Commanding the
// guild in a fight is that right; a master sets it for a rank in the stock
// window, and nothing new has to be shown or kept. A bot guild's master never
// changes a grade, so a person who joined a bot guild orders nobody there.
//
// Which bots come is decided per bot on the engine side (the business a bot
// is on, the map it stands on); what is decided here is the level window, the
// numbers, the ranking and the clock:
//
//   "Pomocy!" (help): the bots of at least the person's level less
//   HELP_LEVELS_BELOW, no ceiling - they fight what the person fights, and a
//   bot ten levels under the monsters round the person is a bot to rescue.
//   HELP_MAX_BOTS come, and stay HELP_STAY_MS once there.
//
//   "Chodzcie expic ze mna" (hunt): the bots within HUNT_LEVELS_BELOW under
//   and HUNT_LEVELS_ABOVE over the person - they hunt round the person, so
//   they had better hunt the same monsters, and a bot far above would take
//   every kill. HUNT_MAX_BOTS come - more is a map swept bare round one
//   person - and stay HUNT_STAY_MS.
//
//   "Wracajcie" (release): every bot this person called goes back to its own
//   life. It needs no right: it touches nothing but the person's own calls.
//
// A call is refused CALL_COOLDOWN_MS after the person's last one: a click
// twice on "Pomocy!" is one call, and the scan of the core's bots it costs is
// not paid for every click. Calling again after that renews the stay of the
// bots already there, which is what calling again means.
//
// No engine types. Tested in tests/playerbot_guild_order_rules_test.cpp.

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

namespace playerbot_guild_order_rules
{
	enum EOrder
	{
		ORDER_NONE = 0,
		ORDER_HELP,
		ORDER_HUNT,
		ORDER_RELEASE
	};

	const int HELP_LEVELS_BELOW = 10;
	const int HUNT_LEVELS_BELOW = 10;
	const int HUNT_LEVELS_ABOVE = 10;
	const int HELP_MAX_BOTS = 8;
	const int HUNT_MAX_BOTS = 4;
	const unsigned int HELP_STAY_MS = 5 * 60 * 1000;
	const unsigned int HUNT_STAY_MS = 15 * 60 * 1000;
	const unsigned int CALL_COOLDOWN_MS = 15 * 1000;

	// The first word of the command, folded by the caller to lowercase ASCII.
	// The window sends "pomoc", "exp" and "wracajcie"; the rest are what a
	// person typing the command by hand would write, in either language.
	// Whole words only: this is a command's argument, not a line of chat, and
	// "exp" inside a longer word is somebody else's.
	inline EOrder ParseOrder(const char* word)
	{
		if (!word)
			return ORDER_NONE;
		while (*word && !(*word >= 'a' && *word <= 'z'))
			++word;
		size_t end = strlen(word);
		while (end > 0 && !(word[end - 1] >= 'a' && word[end - 1] <= 'z'))
			--end;
		if (end == 0)
			return ORDER_NONE;
		struct TWord { const char* text; EOrder order; };
		static const TWord kWords[] = {
			{ "pomoc", ORDER_HELP }, { "pomocy", ORDER_HELP }, { "ratunku", ORDER_HELP },
			{ "help", ORDER_HELP },
			{ "exp", ORDER_HUNT }, { "expimy", ORDER_HUNT }, { "expic", ORDER_HUNT },
			{ "poluj", ORDER_HUNT }, { "polujemy", ORDER_HUNT }, { "hunt", ORDER_HUNT },
			{ "wracajcie", ORDER_RELEASE }, { "wracaj", ORDER_RELEASE }, { "odwolaj", ORDER_RELEASE },
			{ "release", ORDER_RELEASE }, { "back", ORDER_RELEASE },
		};
		for (size_t i = 0; i < sizeof(kWords) / sizeof(kWords[0]); ++i)
			if (strlen(kWords[i].text) == end && strncmp(word, kWords[i].text, end) == 0)
				return kWords[i].order;
		return ORDER_NONE;
	}

	// For the log lines.
	inline const char* OrderName(EOrder order)
	{
		switch (order)
		{
			case ORDER_HELP: return "help";
			case ORDER_HUNT: return "hunt";
			case ORDER_RELEASE: return "release";
			default: return "none";
		}
	}

	// The master, or a rank with the right to use the guild's skills. The
	// master's rank holds every right from the founding (CGuild's
	// constructor) and the grade page greys its boxes out, but the master is
	// asked as the master all the same: leading the guild is the rule, and a
	// rank's rights are only data.
	inline bool MayOrder(bool isMaster, bool rankUsesGuildSkills)
	{
		return isMaster || rankUsesGuildSkills;
	}

	// Whether a bot of this level answers this order from a person of that one.
	inline bool LevelFits(EOrder order, int personLevel, int botLevel)
	{
		switch (order)
		{
			case ORDER_HELP:
				return botLevel >= personLevel - HELP_LEVELS_BELOW;
			case ORDER_HUNT:
				return botLevel >= personLevel - HUNT_LEVELS_BELOW &&
						botLevel <= personLevel + HUNT_LEVELS_ABOVE;
			default:
				return false;
		}
	}

	inline int MaxBots(EOrder order)
	{
		switch (order)
		{
			case ORDER_HELP: return HELP_MAX_BOTS;
			case ORDER_HUNT: return HUNT_MAX_BOTS;
			default: return 0;
		}
	}

	// How long a bot stays beside the person once it has got there.
	inline unsigned int StayMs(EOrder order)
	{
		switch (order)
		{
			case ORDER_HELP: return HELP_STAY_MS;
			case ORDER_HUNT: return HUNT_STAY_MS;
			default: return 0;
		}
	}

	// Whether a person may call again: never called, or the cooldown over. By
	// difference, so the core's millisecond clock may wrap in between.
	inline bool CallAllowed(bool calledBefore, unsigned int lastCallAt, unsigned int now)
	{
		return !calledBefore || now - lastCallAt >= CALL_COOLDOWN_MS;
	}

	// A bot that may come, as the ranking sees it.
	struct TCandidate
	{
		unsigned int pid;
		bool sameMap;     // on the person's map: it walks; elsewhere on the core it is moved
		int distance;     // to the person, on the same map
		int levelGap;     // |bot - person|
	};

	// Who comes first: the bots already on the person's map, nearest first -
	// what a person sees coming is the guild close by, and a walk of a few
	// seconds - then the bots from the core's other maps, the nearest in level
	// first, since a bot put down beside the person has no distance to rank
	// by. The pid last, so the same call picks the same bots.
	inline bool ComesBefore(const TCandidate& a, const TCandidate& b)
	{
		if (a.sameMap != b.sameMap)
			return a.sameMap;
		if (a.sameMap && a.distance != b.distance)
			return a.distance < b.distance;
		if (a.levelGap != b.levelGap)
			return a.levelGap < b.levelGap;
		return a.pid < b.pid;
	}

	// The ranking cut to the order's number.
	inline void Pick(std::vector<TCandidate>& candidates, EOrder order)
	{
		std::sort(candidates.begin(), candidates.end(), ComesBefore);
		const size_t cap = (size_t)std::max(0, MaxBots(order));
		if (candidates.size() > cap)
			candidates.resize(cap);
	}
}

#endif
