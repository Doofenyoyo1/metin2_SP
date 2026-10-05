#ifndef __INC_METIN2_PLAYERBOT_SPOT_RULES_H__
#define __INC_METIN2_PLAYERBOT_SPOT_RULES_H__

// MT2009_PLUS_BOT_CHAT_V2 - a bot defends its spot (the pure half).
//
// The owner, 4 October: "kiedy ktos wejdzie na spota bota i zacznie go bic
// albo kradnie mu moby, bot pisze do gracza na priv 'Spadaj', 'Wypad', 'To moj
// spot', 'SPIEEEEEEEE STAD' i inne takie".
//
// The engine half (playerbot_spot_defense.h) watches what a person does round
// a hunting bot and turns it into events here:
//   - CONTESTED: the person hit a monster the bot is fighting (the monster's
//     damage map has both of them, or the bot is its target or its victim) -
//     a mob stolen, worth the most;
//   - AREA: the person hit a monster near a bot that is hunting there - its
//     spot being farmed under its nose;
//   - STRUCK: the person hit the bot itself (CHARACTER::Damage, the Anti-PK
//     protocol's own report, which still fights back as it did).
// Points pile up while it goes on and are forgotten after a quiet two
// minutes. Each threshold crossed is a whisper one step ruder - a warning,
// then "to moj spot", then "spadaj", then the capitals - never two closer
// than STEP_GAP_MS, never more than MAX_WHISPERS a quarrel. How a bot takes it
// is its temper (playerbot_conv::TemperOf, from its persona): the gentle ones
// say one word and leave the spot to the person, the ordinary ones get ruder
// and give up at the end, the hot-headed ones shout in capitals and call
// their party in. An apology whispered to the bot calms the quarrel down.
//
// Everything here is pure and decided by numbers, so it can be tested.

#include <string>
#include <cstring>

namespace playerbot_spot
{
	typedef unsigned int u32;

	const int POINTS_CONTESTED = 3;
	const int POINTS_AREA = 1;
	const int POINTS_STRUCK = 6;
	// A step: the points a level needs (level 1 .. 4).
	const int LEVEL_POINTS[5] = { 0, 3, 7, 12, 18 };
	const u32 FORGET_MS = 2 * 60 * 1000;      // quiet this long: the points are gone
	const u32 REMEMBER_MS = 6 * 60 * 1000;    // the quarrel itself, for the conversation
	const u32 STEP_GAP_MS = 22 * 1000;        // two whispers of one quarrel at the least
	const u32 REPEAT_GAP_MS = 60 * 1000;      // the same step said again, at the least
	const int REPEAT_POINTS = 8;              // ... and only after this much more
	const int MAX_WHISPERS = 6;
	const u32 APOLOGY_CALM_MS = 3 * 60 * 1000;
	const u32 STRUCK_GAP_MS = 12 * 1000;      // blows counted at most this often
	const size_t MOB_MEMORY = 24;

	enum ETemper
	{
		TEMPER_GENTLE = 0,
		TEMPER_NORMAL = 1,
		TEMPER_HOT = 2
	};

	struct TQuarrel
	{
		u32 firstAt;
		u32 lastAt;
		u32 lastWhisperAt;
		u32 apologyAt;
		u32 struckAt;
		int points;
		int pointsAtWhisper;
		int level;          // the last step whispered, 0 none
		int whispers;
		bool gaveUp;
		bool struck;
		bool calledFriends;
		u32 mobs[MOB_MEMORY];
		size_t mobCount;
		size_t mobNext;

		TQuarrel() : firstAt(0), lastAt(0), lastWhisperAt(0), apologyAt(0), struckAt(0), points(0),
			pointsAtWhisper(0), level(0), whispers(0), gaveUp(false), struck(false), calledFriends(false),
			mobCount(0), mobNext(0)
		{
			for (size_t i = 0; i < MOB_MEMORY; ++i)
				mobs[i] = 0;
		}
	};

	// A quiet spell forgets the points; a long one the quarrel.
	inline void Age(TQuarrel& q, u32 now)
	{
		if (q.lastAt != 0 && now - q.lastAt > REMEMBER_MS)
		{
			q = TQuarrel();
			return;
		}
		if (q.lastAt != 0 && now - q.lastAt > FORGET_MS && q.points > 0)
		{
			q.points = 0;
			q.pointsAtWhisper = 0;
			q.mobCount = 0;
			q.mobNext = 0;
			q.struck = false;
			// Back after a quiet spell: warned once already, so the next
			// word is the second step - not straight to the capitals.
			if (q.level > 1)
				q.level = 1;
			if (q.whispers > 2)
				q.whispers = 2;
		}
	}

	inline bool Forgotten(const TQuarrel& q, u32 now)
	{
		return q.lastAt == 0 || now - q.lastAt > REMEMBER_MS;
	}

	// A monster the person hit: counted once per monster.
	inline bool NoteMob(TQuarrel& q, u32 mobVid, bool contested, u32 now)
	{
		Age(q, now);
		for (size_t i = 0; i < q.mobCount; ++i)
			if (q.mobs[i] == mobVid)
				return false;
		q.mobs[q.mobNext] = mobVid;
		q.mobNext = (q.mobNext + 1) % MOB_MEMORY;
		if (q.mobCount < MOB_MEMORY)
			++q.mobCount;
		if (q.firstAt == 0)
			q.firstAt = now;
		q.lastAt = now;
		q.points += contested ? POINTS_CONTESTED : POINTS_AREA;
		return true;
	}

	inline bool NoteStruck(TQuarrel& q, u32 now)
	{
		Age(q, now);
		if (q.struckAt != 0 && now - q.struckAt < STRUCK_GAP_MS)
			return false;
		q.struckAt = now;
		q.struck = true;
		if (q.firstAt == 0)
			q.firstAt = now;
		q.lastAt = now;
		q.points += POINTS_STRUCK;
		return true;
	}

	inline void NoteApology(TQuarrel& q, u32 now)
	{
		q.apologyAt = now;
		q.points = q.points / 3;
		q.pointsAtWhisper = q.points;
	}

	inline int LevelOf(int points)
	{
		int level = 0;
		for (int i = 1; i <= 4; ++i)
			if (points >= LEVEL_POINTS[i])
				level = i;
		return level;
	}

	// The step to whisper now, 0 for none. Called after an event; `now` is
	// the clock. The gentle give the spot up at the second step, the
	// ordinary at the last; a bot that gave up says nothing more.
	inline int Decide(TQuarrel& q, int temper, u32 now)
	{
		if (q.gaveUp || q.whispers >= MAX_WHISPERS)
			return 0;
		if (q.apologyAt != 0 && now - q.apologyAt < APOLOGY_CALM_MS)
			return 0;
		const int wanted = LevelOf(q.points);
		if (wanted == 0)
			return 0;
		int step = 0;
		if (wanted > q.level)
		{
			if (q.lastWhisperAt != 0 && now - q.lastWhisperAt < STEP_GAP_MS)
				return 0;
			// One step at a time: "ej, to moj mob" before "SPIEEEEE".
			step = q.level + 1;
		}
		else if (q.points - q.pointsAtWhisper >= REPEAT_POINTS && q.lastWhisperAt != 0 &&
				now - q.lastWhisperAt >= REPEAT_GAP_MS)
			step = q.level < 4 ? q.level + 1 : 4;
		if (step == 0)
			return 0;
		q.level = step;
		q.lastWhisperAt = now;
		q.pointsAtWhisper = q.points;
		++q.whispers;
		if ((temper == TEMPER_GENTLE && step >= 2) || (temper == TEMPER_NORMAL && step >= 4))
			q.gaveUp = true;
		return step;
	}

	// Whether this step is the one a hot-headed bot calls its party with.
	inline bool CallsFriends(const TQuarrel& q, int temper, int step)
	{
		return temper == TEMPER_HOT && step >= 3 && !q.calledFriends;
	}

	template <size_t N>
	inline const char* PickOf(const char* const (&pool)[N], u32 roll)
	{
		return pool[roll % N];
	}

	// What the bot whispers at a step. `struck`: the person hit the bot itself.
	inline const char* LineFor(int step, int temper, bool struck, bool gaveUp, u32 roll)
	{
		if (struck && !gaveUp)
		{
			if (step <= 2)
			{
				static const char* const kGentle[] = { "ej, czemu mnie bijesz?!", "auc, co ja ci zrobilem?",
					"hej, nie bij mnie :(" };
				static const char* const kNormal[] = { "ej, co ty robisz?!", "chcesz sie bic? serio?", "halo, czemu mnie atakujesz?",
					"zostaw mnie, expie tu sobie spokojnie" };
				static const char* const kHot[] = { "chcesz sie bic? dawaj", "zaraz pozalujesz", "no chodz, chodz",
					"bijesz mnie na moim spocie? odwazny jestes" };
				if (temper == TEMPER_GENTLE)
					return PickOf(kGentle, roll);
				return temper == TEMPER_HOT ? PickOf(kHot, roll) : PickOf(kNormal, roll);
			}
			static const char* const kNormal[] = { "przestan mnie bic albo zawolam gildie", "serio, odczep sie ode mnie",
				"SPADAJ, nie bede sie z toba bil caly dzien", "dobra, starczy tego, wypad" };
			static const char* const kHot[] = { "chodz, dokoncz to jak taki mocny", "zaraz cie polozymy, zobaczysz",
				"SPIEEEEEEEE STAD", "myslisz ze sie boje? dawaj", "spierdalaj bo przyjde wojem" };
			return temper == TEMPER_HOT ? PickOf(kHot, roll) : PickOf(kNormal, roll);
		}
		if (gaveUp)
		{
			static const char* const kLeave[] = { "dobra, to ja ide gdzie indziej", "eh, zostawiam ci ten spot",
				"ok, nie bede sie klocil, ide dalej", "dobra, masz ten spot, i tak malo mobow", "ehh, ide expic gdzie indziej" };
			static const char* const kLeaveAngry[] = { "dobra, mam dosc, ide gdzie indziej. ksiarz", "eh, z toba sie nie da. ide stad",
				"dobra, wygrales, ide. ale to i tak byl moj spot" };
			return temper == TEMPER_GENTLE ? PickOf(kLeave, roll) : PickOf(kLeaveAngry, roll);
		}
		switch (step)
		{
			case 1:
			{
				static const char* const kGentle[] = { "hej, bije tu, mozesz poszukac innego spota?", "ej, to byly moje moby :(",
					"sorki, ale ja tu expie, moglbys troche obok?" };
				static const char* const kNormal[] = { "ej, to moj mob", "hej, nie kradnij mobow", "zostaw mi te moby co?",
					"ej, bije tu, poszukaj se innego spota", "halo, to moje moby", "ej, zostaw mi nastepnego, co?",
					"ej, daj mi dokonczyc mojego moba", "ej, ja tu expie od godziny" };
				static const char* const kHot[] = { "ej, to moj spot", "wypad z moich mobow", "nie kradnij mobow",
					"ej ty, to moj mob!", "ej, nie ksuj" };
				if (temper == TEMPER_GENTLE)
					return PickOf(kGentle, roll);
				return temper == TEMPER_HOT ? PickOf(kHot, roll) : PickOf(kNormal, roll);
			}
			case 2:
			{
				static const char* const kNormal[] = { "serio? to moj spot", "idz sobie gdzie indziej", "nie kradnij mobow!!",
					"bylem tu pierwszy", "ej no, ile mozna", "to moj spot, poszukaj innego" };
				static const char* const kHot[] = { "spadaj", "wypad", "TO MOJ SPOT", "idz expic gdzie indziej",
					"jeszcze raz i zobaczysz" };
				return temper == TEMPER_HOT ? PickOf(kHot, roll) : PickOf(kNormal, roll);
			}
			case 3:
			{
				static const char* const kNormal[] = { "spadaj", "wypad stad", "TO MOJ SPOT", "idz sobie gdzie indziej, serio",
					"nie kradnij mobow ksiarzu", "spadaj z mojego spota" };
				static const char* const kHot[] = { "SPIEEEEEEEE STAD", "WYPAD Z MOJEGO SPOTA", "spadaj ksiarzu",
					"zaraz zawolam ekipe, to zobaczysz", "SPADAJ STAD", "spierdalaj bo przyjde wojem" };
				return temper == TEMPER_HOT ? PickOf(kHot, roll) : PickOf(kNormal, roll);
			}
			default:
			{
				static const char* const kNormal[] = { "SPADAJ", "dobra, mam dosc, zglaszam cie za ks", "WYPAD", "spierdalaj bo przyjde wojem" };
				static const char* const kHot[] = { "SPIEEEEEEEEEEEE", "ZJEZDZAJ Z MOJEGO SPOTA",
					"nie rozumiesz po polsku? WYPAD", "SPIEEEEEEEE STAD, KSIARZU", "spierdalaj bo przyjde wojem" };
				return temper == TEMPER_HOT ? PickOf(kHot, roll) : PickOf(kNormal, roll);
			}
		}
	}

	// Another bot of the party or the guild, taking its friend's side.
	inline const char* FriendLine(u32 roll)
	{
		static const char* const k[] = { "no wlasnie, spadaj z naszego spota", "slyszales kolege? wypad",
			"nie kradnij mobow moim ludziom", "ej, zostaw go, to nasz spot", "jeszcze raz go ruszysz i bedziesz mial nas na glowie" };
		return PickOf(k, roll);
	}

	// What "czemu?" is answered with after a complaint.
	inline const char* ReasonFor(bool struck)
	{
		return struck ? "Bo mnie bijesz bez powodu." : "Bo to moj spot, bylem tu pierwszy, a ty bierzesz moje moby.";
	}
}

#endif
