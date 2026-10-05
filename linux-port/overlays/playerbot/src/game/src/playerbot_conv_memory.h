#ifndef __INC_METIN2_PLAYERBOT_CONV_MEMORY_H__
#define __INC_METIN2_PLAYERBOT_CONV_MEMORY_H__

// PlayerBot Conversation v6 - conversation memory and context (pure).
//
// CONTEXT RESOLUTION -> CONVERSATION MEMORY -> PLAYER RELATIONSHIP
//
// One TConvMemory per (player, bot) pair. It holds:
//   - the last few turns (intent, subject, topic, text, time),
//   - the last game subject and how many small-talk turns happened since
//     (so "a gdzie teraz expisz?" after talking about winter reads as
//     "wracajac do tego..."),
//   - what the bot last asked the player ("A ty?") so that the answer is read
//     as an answer and not as a new question,
//   - the last reply and the templates used lately (no parroting),
//   - a small relationship ledger: lines, sessions, kindness, insults,
//     one thing the player said they like.
//
// Context expires (CONV_CONTEXT_TTL_MS); the relationship ledger lives much
// longer (CONV_MEMORY_TTL_MS) and is capped in count by the engine side.

#include "playerbot_conv_intents.h"
// MT2009_PLUS_BOT_DUNGEON_LFG_V1: the dungeon finder's words (TTalk, the yes
// and the no, the dungeons' names) - pure, like this file.
#include "playerbot_dungeon_lfg_rules.h"

namespace playerbot_conv
{
	const u32 CONV_CONTEXT_TTL_MS = 3 * 60 * 1000;      // follow-ups older than this mean nothing
	const u32 CONV_RETURN_TTL_MS = 15 * 60 * 1000;      // "wracajac do tego" window
	const u32 CONV_SESSION_GAP_MS = 20 * 60 * 1000;     // a new conversation starts after this
	const u32 CONV_MEMORY_TTL_MS = 3 * 60 * 60 * 1000;  // the pair is forgotten after this
	const u32 CONV_BOT_ASK_TTL_MS = 90 * 1000;          // an answer to "a ty?" is expected this long
	// "przestan do mnie pisac": how long the bot keeps from starting anything
	// with that person. An apology ends it early.
	const u32 CONV_QUIET_MS = 2 * 60 * 60 * 1000;
	// A fact said this recently (the level, the gear line, why the weapon is
	// what it is) is not recited again in full - "Dalej to samo".
	const u32 CONV_FACT_TTL_MS = 3 * 60 * 1000;
	// A reason said this recently is left out of a reply that already says
	// something of its own: four replies in a row ending "kasa, jak mowilem"
	// read as a bot with one line.
	const u32 CONV_REASON_RECENT_MS = 60 * 1000;
	// "Jestem w Joan" is remembered this long, so a teleport since then is
	// said as one ("Bylem w Joan, teraz jestem juz w Dolinie Orkow").
	const u32 CONV_SAID_MAP_TTL_MS = 15 * 60 * 1000;
	const size_t CONV_TURNS = 4;
	const size_t CONV_RECENT_TEMPLATES = 12;

	// MT2009_PLUS_BOT_CHAT_V2 (deals): a trade talked over on the whisper -
	// the bot buying what a person sells (its own "K> ..." post answered, or
	// an offer) or selling what it has in its bag. The talk is here; the
	// exchange window that completes it is the engine's
	// (playerbot_chat_deals.h), told of the agreement by IConvWorld::DealAgreed.
	const u32 CONV_DEAL_TTL_MS = 12 * 60 * 1000;
	enum EDealState
	{
		DEAL_NONE = 0,
		DEAL_OPEN,       // talking: price and count
		DEAL_AGREED,     // both settled, waiting for the exchange window
		DEAL_DONE,
		DEAL_FAILED
	};
	enum EDealSide
	{
		DEAL_BOT_BUYS = 1,
		DEAL_BOT_SELLS = 2
	};

	// MT2009_PLUS_BOT_CHAT_V2 (deals): why the bot takes or gives fewer pieces
	// than the person named - said, never silently cut (the owner, 4 October:
	// "5" answered with "2 szt po 158k" and no word why).
	enum EDealCap
	{
		DEAL_CAP_NONE = 0,
		DEAL_CAP_NEED,    // it does not need more (its post's count, its want)
		DEAL_CAP_PURSE,   // its yang does not stretch further
		DEAL_CAP_HAVE     // it has only so many to sell
	};

	// The landmark a bot waits at for a deal's window.
	enum EDealSpot
	{
		DEAL_SPOT_NONE = 0,
		DEAL_SPOT_SMITH,  // the village's blacksmith (20016)
		DEAL_SPOT_STALL   // its own stall, which it does not leave
	};

	// Where the two of a settled deal meet, as the engine arranged it
	// (IConvWorld::DealAgreed, IConvWorld::DealMeet). The owner, 4 October:
	// "Boty niech podaja dokladna lokalizacje ... typu jestem w m1 yongan
	// bede czekac przy kowalu" - a village, a landmark in it, and the channel
	// when it is not the person's.
	struct TDealMeetPlace
	{
		int kind;           // EDealMeet (playerbot_conv_state.h)
		long map;           // the village (or the bot's map) it waits on, 0 none
		int spot;           // EDealSpot
		int channel;        // the bot's channel
		bool otherChannel;  // the person plays on another one
		bool arrived;       // standing at the spot (or never had to go)
		TDealMeetPlace() : kind(-1), map(0), spot(DEAL_SPOT_NONE), channel(0), otherChannel(false), arrived(false) {}
	};

	// "ide", "czekaj", "zaraz bede", "chwila" - the person on the way to a
	// meeting, or asking the bot to wait for them.
	inline bool DealComingWords(const TTokens& t)
	{
		static const char* const k[] = { "ide", "idziemy", "czekaj", "poczekaj", "zaczekaj", "czekej", "chwila",
			"chwile", "chwilka", "chwileczke", "moment", "sek", "sec", "zaraz", "bede", "lece", "biegne", "jade",
			"dojde", "dochodze", "przychodze", "teleportuje", "tepam", "przelaczam", "zmieniam", "przelacze",
			"zmienie", "toba", "kowala", "kowalu", "jestem" };
		for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); ++i)
			if (t.Has(k[i]))
				return true;
		return false;
	}

	struct TDeal
	{
		unsigned char state;
		unsigned char side;
		u32 vnum;
		u32 skill;            // a skill book's skill (socket 0), 0 any other item
		std::string name;
		int count;            // pieces settled on, 0 not yet
		int maxCount;         // the most the bot takes / has
		long long offer;      // the bot's price per piece now
		long long ask;        // the person's last price per piece, 0 none
		long long limit;      // the most the bot pays / the least it takes, per piece
		long long fair;       // what the market says a piece is worth
		int rounds;           // counter-offers made
		bool priceSettled;
		bool fromPost;        // an answer to the bot's own public post
		int capWhy;           // EDealCap: what maxCount stands for
		TDealMeetPlace meet;  // once agreed: where the window is
		u32 at;
		TDeal() : state(DEAL_NONE), side(0), vnum(0), skill(0), count(0), maxCount(0), offer(0), ask(0), limit(0), fair(0),
			rounds(0), priceSettled(false), fromPost(false), capWhy(DEAL_CAP_NONE), at(0) {}
		bool Live(u32 now) const
		{
			return (state == DEAL_OPEN || state == DEAL_AGREED) && at != 0 && now - at < CONV_DEAL_TTL_MS;
		}
	};

	enum EBotAsk
	{
		ASK_NONE = 0,
		ASK_HOW_ARE_YOU,   // "A u ciebie jak?"
		ASK_ACTIVITY,      // "A ty co robisz?"
		ASK_TOPIC,         // "A ty lubisz zime?" - botAskTopic says which
		ASK_JOIN,          // "Idziesz na exp?"
		ASK_FOUND,         // "Znalazles cos ciekawego?"
		ASK_SUMMON,        // "Po co mam przyjsc?" - a stranger called the bot over
		ASK_REAL           // MT2009_PLUS_BOT_CHAT_V2: "a ty skad jestes?" after the bot's own city
	};

	enum ETier
	{
		TIER_HOSTILE = 0,
		TIER_STRANGER,
		TIER_KNOWN,
		TIER_FRIEND,
		TIER_BUDDY
	};

	inline const char* TierName(int t)
	{
		switch (t)
		{
			case TIER_HOSTILE: return "HOSTILE";
			case TIER_STRANGER: return "STRANGER";
			case TIER_KNOWN: return "KNOWN";
			case TIER_FRIEND: return "FRIEND";
			default: return "BUDDY";
		}
	}

	struct TTurn
	{
		EIntent intent;
		EIntent subject;
		ETopic topic;
		EFollow follow;
		EQType qtype;
		bool question;
		u32 at;
		std::string norm;
		std::string object;
		TTurn() : intent(I_NONE), subject(I_NONE), topic(T_NONE), follow(F_NONE), qtype(Q_STATEMENT),
			question(false), at(0) {}
	};

	struct TConvMemory
	{
		u32 playerPID;
		u32 botPID;
		u32 firstAt;
		u32 lastPlayerAt;
		u32 lastBotAt;
		u32 lastInitiativeAt;
		u32 lastCheckAt;
		u32 talks;
		u32 sessions;
		int positive;
		int negative;

		TTurn turns[CONV_TURNS]; // [0] newest
		size_t turnCount;

		EIntent lastGameIntent;
		u32 lastGameAt;
		int generalStreak;
		ETopic lastTopic;
		u32 lastTopicAt;

		EIntent lastAnswered;
		u32 lastAnsweredAt;
		std::string lastReply;
		std::string lastReason;   // the "because" of the last answer, for "dlaczego?"

		unsigned char botAsk;
		ETopic botAskTopic;
		u32 botAskAt;

		u32 recentTemplates[CONV_RECENT_TEMPLATES];
		size_t recentIndex;

		u32 moodMentionAt;
		u32 greetedAt;
		std::string playerLikes;
		std::string playerDislikes;
		int lastKnownLevel;
		u32 repeatCount;

		// "przestan do mnie pisac": no initiative before this (0 = none).
		u32 quietUntil;
		// The maps the bot last named to this person, newest first: what a
		// teleport since then is measured against.
		long lastSaidMap;
		u32 lastSaidMapAt;
		long prevSaidMap;
		u32 prevSaidMapAt;
		// Facts said lately, so they are not recited again in full.
		u32 levelSaidAt;
		int levelSaid;
		u32 gearSaidAt;
		u32 gearReasonAt;
		// Generic answers in a row ("Aha, rozumiem."): the second one steers.
		int fallbackStreak;
		// MT2009_PLUS_BOT_CHAT_V2: the bot's patience with this person, 0..100.
		// Insults, spam, the same question over and over and lines nothing
		// understood wear it down; time and kindness bring it back
		// (UpdatePatience). Low, the bot answers shortly, then rarely, then
		// not at all.
		int patience;
		u32 patienceAt;
		// How often each of these was asked lately: the answer moves on
		// ("serio, jestem botem, beep boop xd" the third time).
		int botAsked;
		int begAsked;
		int jokesTold;
		// The rate limit: lines in the current minute, and the bot ignoring
		// the person until mutedUntil after it said it would.
		u32 rateStart;
		int rateLines;
		u32 mutedUntil;
		// The trade talked over now (TDeal).
		TDeal deal;
		// MT2009_PLUS_BOT_DUNGEON_LFG_V1: what the bot asked the person about
		// a dungeon (playerbot_dungeon_lfg.h): "moge przyjsc?", "na jaki
		// dung?", or that it waits at the entrance. Set by the engine when the
		// offer goes out, read by ResolveContext, moved on by the answer's
		// generator (GenLfgAnswer) and cleared by the engine when it is over.
		// Kept through ClearContext, as the deal is: it has its own clock.
		playerbot_lfg::TTalk lfg;

		TConvMemory() : playerPID(0), botPID(0), firstAt(0), lastPlayerAt(0), lastBotAt(0),
			lastInitiativeAt(0), lastCheckAt(0), talks(0), sessions(0), positive(0), negative(0),
			turnCount(0), lastGameIntent(I_NONE), lastGameAt(0), generalStreak(0), lastTopic(T_NONE),
			lastTopicAt(0), lastAnswered(I_NONE), lastAnsweredAt(0), botAsk(ASK_NONE),
			botAskTopic(T_NONE), botAskAt(0), recentIndex(0), moodMentionAt(0), greetedAt(0),
			lastKnownLevel(0), repeatCount(0), quietUntil(0), lastSaidMap(0), lastSaidMapAt(0),
			prevSaidMap(0), prevSaidMapAt(0), levelSaidAt(0), levelSaid(0), gearSaidAt(0), gearReasonAt(0),
			fallbackStreak(0), patience(100), patienceAt(0), botAsked(0), begAsked(0), jokesTold(0),
			rateStart(0), rateLines(0), mutedUntil(0)
		{
			for (size_t i = 0; i < CONV_RECENT_TEMPLATES; ++i)
				recentTemplates[i] = 0;
		}

		const TTurn* Prev(u32 now, size_t back = 0) const
		{
			if (back >= turnCount)
				return NULL;
			const TTurn& t = turns[back];
			if (t.at == 0 || now - t.at > CONV_CONTEXT_TTL_MS)
				return NULL;
			return &t;
		}

		bool UsedTemplateRecently(u32 id) const
		{
			for (size_t i = 0; i < CONV_RECENT_TEMPLATES; ++i)
				if (recentTemplates[i] == id)
					return true;
			return false;
		}

		void NoteTemplate(u32 id)
		{
			recentTemplates[recentIndex % CONV_RECENT_TEMPLATES] = id;
			++recentIndex;
		}

		// The quiet window and the maps said are kept: they have clocks of
		// their own, longer than the context's.
		void ClearContext()
		{
			turnCount = 0;
			lastGameIntent = I_NONE;
			lastGameAt = 0;
			generalStreak = 0;
			lastTopic = T_NONE;
			botAsk = ASK_NONE;
			lastAnswered = I_NONE;
			lastReason.clear();
			levelSaidAt = 0;
			gearSaidAt = 0;
			gearReasonAt = 0;
			fallbackStreak = 0;
		}
	};

	inline bool IsQuiet(const TConvMemory& m, u32 now)
	{
		return m.quietUntil != 0 && (int)(m.quietUntil - now) > 0;
	}

	// MT2009_PLUS_BOT_CHAT_V2: patience. A point back every 20 s of quiet, up
	// to a hundred; `delta` is what this line costs (negative) or gives.
	const u32 CONV_PATIENCE_REGEN_MS = 20 * 1000;

	inline void UpdatePatience(TConvMemory& m, int delta, u32 now)
	{
		if (m.patienceAt != 0 && now - m.patienceAt >= CONV_PATIENCE_REGEN_MS)
		{
			const u32 regained = (now - m.patienceAt) / CONV_PATIENCE_REGEN_MS;
			m.patience += regained > 100 ? 100 : (int)regained;
		}
		m.patienceAt = now;
		m.patience += delta;
		if (m.patience > 100)
			m.patience = 100;
		if (m.patience < 0)
			m.patience = 0;
	}

	// A map the bot has just named in a reply.
	inline void NoteSaidMap(TConvMemory& m, long map, u32 now)
	{
		if (map == 0)
			return;
		if (m.lastSaidMap != map)
		{
			m.prevSaidMap = m.lastSaidMap;
			m.prevSaidMapAt = m.lastSaidMapAt;
			m.lastSaidMap = map;
		}
		m.lastSaidMapAt = now;
	}

	// What a resolved turn is "about" - the intent itself, or what a
	// follow-up was about.
	inline EIntent TurnSubject(const TTurn& t)
	{
		if (t.intent == I_FOLLOW_UP || t.intent == I_ANSWER_TO_BOT)
			return t.subject;
		return t.intent;
	}

	// The most recent turn that was about something (a bare "ok" or "xD" does
	// not reset what the conversation is about).
	inline const TTurn* PrevMeaningful(const TConvMemory& m, u32 now)
	{
		for (size_t i = 0; i < m.turnCount; ++i)
		{
			const TTurn* t = m.Prev(now, i);
			if (!t)
				return NULL;
			if (IsReactionIntent(t->intent))
				continue;
			if (TurnSubject(*t) == I_NONE && t->topic == T_NONE)
				continue;
			return t;
		}
		return NULL;
	}

	inline bool IsActivityish(EIntent i)
	{
		return i == I_ACTIVITY || i == I_ACTIVITY_LOCATION || i == I_LOCATION || i == I_TARGET ||
				i == I_MOB_COUNT || i == I_METIN || i == I_TIME_HERE || i == I_MAP_OPINION ||
				i == I_BIOLOGIST || i == I_MISSIONS || i == I_DROP_LUCK;
	}

	inline bool IsQuestionLine(const TAnalysis& a)
	{
		const TConceptSet& c = a.concepts;
		return a.question || c.Has(C_WHAT) || c.Has(C_WHERE) || c.Has(C_WHY) || c.Has(C_WHO) ||
				c.Has(C_WHICH) || c.Has(C_HOWMUCH) || c.Has(C_WHEN) || a.tokens.Has("czy") ||
				(c.Has(C_HOW) && !c.Has(C_HOWAREYOU));
	}

	// ------------------------------------------------------ context resolver

	inline void ResolveContext(TAnalysis& a, const TConvMemory& m, u32 now)
	{
		const TTurn* prev = PrevMeaningful(m, now);
		const EIntent base = prev ? TurnSubject(*prev) : I_NONE;
		const ETopic baseTopic = prev ? prev->topic : T_NONE;
		const bool baseGeneral = base == I_GENERAL || (base == I_NONE && baseTopic != T_NONE);

		// The same line again within a minute.
		for (size_t i = 0; i < m.turnCount && i < 3; ++i)
		{
			const TTurn* t = m.Prev(now, i);
			if (t && now - t->at < 60000 && a.tokens.norm.size() > 2 && t->norm == a.tokens.norm)
				a.repeated = true;
		}

		// MT2009_PLUS_BOT_DUNGEON_LFG_V1: the bot offered to come along to a
		// dungeon, asked which one, or waits at its entrance. A yes, a no, or
		// (to "na jaki dung?") the dungeon's name is the answer to that,
		// whatever the line would read as alone: "chodz" is not "come over to
		// me" here, "nie" not a reaction, "ok" not a nod. Anything else - a
		// question of its own, "nie wiem" - is answered as ever and the offer
		// keeps its clock.
		if (m.lfg.Live(now) && a.tokens.words.size() <= playerbot_lfg::MAX_ANSWER_WORDS)
		{
			int difficulty = 0;
			const std::string named = m.lfg.state == playerbot_lfg::TALK_ASKED
					? playerbot_lfg::FindDungeonKey(a.tokens, difficulty) : std::string();
			const int yesNo = playerbot_lfg::ParseYesNo(a.tokens);
			int answer = playerbot_lfg::ANSWER_NONE;
			if (!named.empty() && yesNo >= 0)
			{
				answer = playerbot_lfg::ANSWER_CHOOSE;
				a.lfgKey = named;
				a.lfgDifficulty = difficulty;
			}
			else if (yesNo < 0)
				answer = playerbot_lfg::ANSWER_NO;
			else if (yesNo > 0)
				answer = m.lfg.state == playerbot_lfg::TALK_ASKED ? playerbot_lfg::ANSWER_WHICH : playerbot_lfg::ANSWER_YES;
			if (answer != playerbot_lfg::ANSWER_NONE)
			{
				a.intent = I_LFG_ANSWER;
				a.subject = I_LFG_ANSWER;
				a.lfgAnswer = answer;
				return;
			}
		}

		// MT2009_PLUS_BOT_CHAT_V2 (deals): while a trade is talked over, a
		// price, a count, a yes or a no is about that trade - unless the line
		// names another item, which is a new one.
		if (m.deal.Live(now) && a.tokens.words.size() <= 12)
		{
			const TConceptSet& c = a.concepts;
			const bool otherItem = !a.object.empty() && !m.deal.name.empty() &&
					!ItemNameMatches(m.deal.name.c_str(), a.object) && (a.intent == I_BUY || a.intent == I_SELL);
			const bool dealish = a.offerYang > 0 || a.dealCount > 0 || c.Has(C_AGREE) || c.Has(C_YES) ||
					c.Has(C_NO) || c.Has(C_ACK) || c.Has(C_PRICEQ) || c.Has(C_HOWMUCH) || c.Has(C_STILL) ||
					a.intent == I_BUY || a.intent == I_SELL || a.intent == I_PRICE || a.intent == I_GOLD ||
					a.tokens.Has("drogo") || a.tokens.Has("malo") || a.tokens.Has("tanio") || a.tokens.Has("wiecej") ||
					a.tokens.Has("mniej") || a.tokens.Has("taniej") || a.tokens.Has("drozej") || a.tokens.Has("gdzie") ||
					a.tokens.Has("wymiane") || a.tokens.Has("wymiana") || a.tokens.Has("handel");
			// Agreed and waiting for the window: "czekaj ide za toba", "juz
			// ide", "zaraz bede", "chwila", "gdzie jestes?" are about the
			// meeting (the small talk answered them "hehe, moze", and "juz
			// ide" read as a purchase of "ide").
			const bool meeting = (m.deal.state == DEAL_AGREED && a.tokens.words.size() <= 8 &&
					(DealComingWords(a.tokens) || c.Has(C_WHERE) || a.tokens.Has("jestes") ||
					 a.tokens.Has("kanal") || a.tokens.Has("ch"))) ||
					// Still talking: "czekaj", "chwila" - a moment to think.
					(m.deal.state == DEAL_OPEN && a.tokens.words.size() <= 3 && DealComingWords(a.tokens));
			if ((meeting || (dealish && !otherItem)) && !IsColdIntent((EIntent)a.intent) && a.intent != I_FAREWELL &&
					a.intent != I_THANKS && a.intent != I_MATH)
			{
				a.intent = I_DEAL;
				a.subject = I_DEAL;
				return;
			}
		}
		// A trade talked about and then just the item's name ("buty ognistego
		// ptaka", "a fms?"), or "a moge ci sprzedac?" with the item a line
		// before: the same trade, about that item.
		if ((base == I_BUY || base == I_SELL || base == I_SHOP || base == I_PRICE || base == I_ITEM_OWN ||
				base == I_MARKET || base == I_DEAL) && prev)
		{
			if ((a.intent == I_SELL || a.intent == I_BUY || a.intent == I_PRICE) && a.object.empty() &&
					!prev->object.empty())
			{
				a.object = prev->object;
				a.subject = a.intent;
				return;
			}
			const bool bareItem = a.tokens.words.size() <= 6 && !a.question && !a.concepts.Has(C_YOU) &&
					(a.concepts.Has(C_ITEMWORD) || a.concepts.Has(C_GEAR) || a.intent == I_EQUIPMENT ||
					 a.intent == I_UNKNOWN_STATEMENT);
			const bool bareItemAsked = a.tokens.words.size() <= 6 && (a.concepts.Has(C_ITEMWORD) ||
					a.concepts.Has(C_GEAR)) && (a.intent == I_EQUIPMENT || a.intent == I_UNKNOWN_QUESTION ||
					a.intent == I_ITEM_OWN || a.intent == I_FOLLOW_UP);
			if (bareItem || bareItemAsked)
			{
				a.intent = base == I_SELL ? I_SELL : (base == I_PRICE ? I_PRICE : I_BUY);
				a.subject = a.intent;
				a.object = ExtractTradeObject(a.tokens, 0);
				return;
			}
		}

		// MT2009_PLUS_BOT_CHAT_V2: "jeszcze jeden", "dawaj kolejny", "jeszcze"
		// after a joke is another joke; "a na 45?" after a question about
		// where the Metins or the exp of a level are is the same question for
		// another level.
		if (base == I_JOKE && a.tokens.words.size() <= 5 && a.intent != I_JOKE &&
				(a.concepts.Has(C_MORE) || a.tokens.Has("jeszcze") || a.tokens.Has("dawaj") ||
				 a.tokens.Has("kolejny") || a.tokens.Has("nastepny")) && !IsColdIntent((EIntent)a.intent))
		{
			a.intent = I_JOKE;
			a.subject = I_JOKE;
			return;
		}
		if ((base == I_WHERE_METIN || base == I_WHERE_EXP) && a.tokens.words.size() <= 5 &&
				(a.intent == I_UNKNOWN_QUESTION || a.intent == I_UNKNOWN_STATEMENT || a.intent == I_FOLLOW_UP ||
				 a.intent == I_LEVEL))
		{
			for (size_t i = 0; i < a.tokens.words.size(); ++i)
			{
				const std::string& w = a.tokens.words[i];
				int v = 0;
				bool digits = !w.empty() && w.size() <= 3;
				for (size_t k = 0; k < w.size() && digits; ++k)
				{
					if (w[k] < '0' || w[k] > '9')
						digits = false;
					else
						v = v * 10 + (w[k] - '0');
				}
				if (digits && v >= 1 && v <= 120)
				{
					a.intent = base;
					a.subject = base;
					a.levelAsked = v;
					return;
				}
			}
		}
		// "a w real?" after the bot's empire or its map: real life.
		if ((base == I_ORIGIN || base == I_LOCATION || base == I_REAL_LIFE) && a.tokens.words.size() <= 4 &&
				(a.tokens.Has("real") || a.tokens.Has("realu") || a.tokens.Has("irl") ||
				 (a.concepts.Has(C_CITY) && IsQuestionLine(a))))
		{
			a.intent = I_REAL_LIFE;
			a.subject = I_REAL_LIFE;
			return;
		}

		// An answer to what the bot asked. Only when the line is not a new
		// question and not one of the fixed social moves. The summon and its
		// release are moves of their own too ("chodz tu" again, "mozesz isc");
		// but "pomoz mi" after "Po co mam przyjsc?" is the reason asked for,
		// which the party words would otherwise take for an invitation. And
		// the reason often sounds like a question ("pokaze ci cos", "pomozesz
		// mi z metinem?"): after that question only one about the bot itself
		// ("jaki masz lvl?") is a new question.
		const bool newQuestion = IsQuestionLine(a) &&
				(m.botAsk != ASK_SUMMON || IsGameIntent((EIntent)a.intent));
		const bool askPending = m.botAsk != ASK_NONE && now - m.botAskAt < CONV_BOT_ASK_TTL_MS;
		// "moze razem pobijemy?" met with "zawijaj stad" or "przestan do mnie
		// pisac" is not an answer to it, but it does close it: the reply knows
		// which question it closed.
		if (askPending && (IsColdIntent((EIntent)a.intent)))
			a.answeredAsk = (unsigned char)m.botAsk;
		if (askPending && !newQuestion &&
				a.intent != I_FAREWELL && a.intent != I_GREETING && !IsColdIntent((EIntent)a.intent) &&
				!IsArgumentIntent((EIntent)a.intent) && a.intent != I_MATH &&
				a.intent != I_BUY && a.intent != I_SELL &&
				(a.intent != I_PARTY_REQUEST || m.botAsk == ASK_SUMMON) &&
				a.intent != I_SUMMON && a.intent != I_DISMISS && a.intent != I_THANKS &&
				// MT2009_PLUS_BOT_CHAT_V2: requests of their own, never an answer
				a.intent != I_JOKE && a.intent != I_BEG && a.intent != I_MEET && a.intent != I_GENDER &&
				a.intent != I_WHERE_METIN && a.intent != I_WHERE_EXP && a.intent != I_IS_BOT &&
				(a.intent != I_REAL_LIFE || m.botAsk == ASK_REAL))
		{
			a.subject = (EIntent)a.intent;
			a.intent = I_ANSWER_TO_BOT;
			a.answeredAsk = (unsigned char)m.botAsk;
			if (a.topic == T_NONE && m.botAsk == ASK_TOPIC)
				a.topic = m.botAskTopic;
			return;
		}

		// "a za 3kk?" after talking about an item: an offer for that item.
		if (a.offerYang > 0 && a.object.empty() && prev && !prev->object.empty() &&
				(base == I_BUY || base == I_SHOP || base == I_PRICE || base == I_ITEM_OWN) &&
				a.intent != I_SELL && a.intent != I_GOLD && a.tokens.words.size() <= 5)
		{
			a.intent = I_BUY;
			a.subject = I_BUY;
			a.object = prev->object;
			return;
		}

		if (a.intent == I_FOLLOW_UP)
		{
			a.subject = base;
			switch (a.follow)
			{
				case F_WHERE:
					if (baseGeneral) { a.intent = I_GENERAL; a.topic = baseTopic; a.qtype = Q_OPEN; }
					else if (base == I_NEXT_PLAN || base == I_GOAL) a.intent = I_NEXT_PLAN;
					else if (base == I_TRAVEL || base == I_REST) a.intent = I_TRAVEL;
					else if (base == I_GUILD || base == I_PARTY) a.intent = I_LOCATION;
					else if (base == I_SHOP || base == I_MARKET || base == I_BUY || base == I_SELL) a.intent = I_SHOP;
					else if (base == I_FISHING || base == I_MINING) a.intent = I_LOCATION;
					else a.intent = I_ACTIVITY_LOCATION;
					break;
				case F_COUNT:
					if (IsActivityish(base) || base == I_ACTIVITY) a.intent = I_MOB_COUNT;
					else if (base == I_GOLD || base == I_SHOP || base == I_MARKET) a.intent = I_GOLD;
					else if (base == I_PARTY || base == I_PARTY_REQUEST) a.intent = I_PARTY;
					else if (base == I_GUILD || base == I_GUILD_WAR) a.intent = I_GUILD;
					else if (base == I_INVENTORY || base == I_INVENTORY_SPACE || base == I_ITEM_OWN ||
							base == I_EQUIPMENT) a.intent = I_INVENTORY_SPACE;
					else if (base == I_LEVEL || base == I_PROGRESS_TODAY) a.intent = base;
					else if (base == I_HP || base == I_FISHING || base == I_DEATH || base == I_HORSE ||
							base == I_SKILLS || base == I_BUFFS) a.intent = base;
					else if (baseGeneral) { a.intent = I_GENERAL; a.topic = baseTopic; a.qtype = Q_OPEN; }
					else a.intent = I_FOLLOW_UP; // "ale czego?"
					break;
				case F_ALONE:
				case F_WITHWHO:
					if (baseGeneral) { a.intent = I_GENERAL; a.topic = baseTopic; a.qtype = Q_OPEN; }
					else a.intent = I_PARTY;
					break;
				case F_NEXT:
					if (baseGeneral) { a.intent = I_FOLLOW_UP; }
					else a.intent = I_NEXT_PLAN;
					break;
				case F_WHAT:
					if (base == I_ACTIVITY || base == I_ACTIVITY_LOCATION || base == I_MOB_COUNT) a.intent = I_TARGET;
					else a.intent = I_FOLLOW_UP;
					break;
				case F_WHO:
					if (base == I_PARTY || base == I_GUILD || base == I_PARTY_REQUEST) a.intent = base;
					else if (base == I_TARGET || base == I_ACTIVITY) a.intent = I_TARGET;
					else a.intent = I_FOLLOW_UP;
					break;
				case F_THIS:
					if (base != I_NONE && base != I_GENERAL) a.intent = base;
					else if (baseGeneral) { a.intent = I_GENERAL; a.topic = baseTopic; a.qtype = Q_OPEN; }
					else a.intent = I_FOLLOW_UP;
					break;
				case F_MIRROR:
				{
					// "a ty?" - the bot's own take on what the player just said.
					const TTurn* last = m.Prev(now, 0);
					if (last && (last->topic != T_NONE) && last->intent != I_FOLLOW_UP)
					{
						a.intent = I_GENERAL;
						a.topic = last->topic;
						a.qtype = Q_MIRROR;
						a.object = last->object;
					}
					else if (last && IsGameIntent(TurnSubject(*last)))
						a.intent = TurnSubject(*last);
					else
						a.intent = I_HOW_ARE_YOU;
					break;
				}
				default:
					// WHY / HOW / CONFIRM / WHEN stay follow-ups about `base`.
					break;
			}
			if (a.intent != I_FOLLOW_UP && a.intent != I_GENERAL)
				a.subject = a.intent;
		}

		// Back to the game after small talk.
		// Only to the same subject: talking about winter and then asking for yang
		// is a new question, not "wracajac do tego".
		if (IsGameIntent(a.intent) && m.generalStreak >= 1 && m.lastGameIntent != I_NONE &&
				now - m.lastGameAt < CONV_RETURN_TTL_MS &&
				GameTopicGroup(a.intent) == GameTopicGroup(m.lastGameIntent))
			a.returnToTopic = true;
		// Away from the game.
		if (a.intent == I_GENERAL && prev && IsGameIntent(base))
			a.topicChange = true;
	}

	// ---------------------------------------------------------------- update

	inline void BeginPlayerLine(TConvMemory& m, u32 now)
	{
		if (m.firstAt == 0)
			m.firstAt = now;
		// A question the bot asked a moment ago survives the clearing: the
		// bot speaks first only to somebody who has been quiet a while, so
		// the answer to "moze razem pobijemy?" is exactly the line that comes
		// after a gap - and was read as a new question about the horse.
		const unsigned char ask = m.botAsk;
		const ETopic askTopic = m.botAskTopic;
		const u32 askAt = m.botAskAt;
		const bool keepAsk = ask != ASK_NONE && askAt != 0 && now - askAt < CONV_BOT_ASK_TTL_MS;
		if (m.lastPlayerAt == 0 || now - m.lastPlayerAt > CONV_SESSION_GAP_MS)
		{
			++m.sessions;
			m.ClearContext();
		}
		else if (now - m.lastPlayerAt > CONV_CONTEXT_TTL_MS)
			m.ClearContext();
		if (keepAsk)
		{
			m.botAsk = ask;
			m.botAskTopic = askTopic;
			m.botAskAt = askAt;
		}
	}

	inline void RememberPlayerLine(TConvMemory& m, const TAnalysis& a, u32 now)
	{
		for (size_t i = CONV_TURNS - 1; i > 0; --i)
			m.turns[i] = m.turns[i - 1];
		TTurn& t = m.turns[0];
		t = TTurn();
		t.intent = a.intent;
		t.subject = a.subject;
		t.topic = a.topic;
		t.follow = a.follow;
		t.qtype = a.qtype;
		t.question = a.question;
		t.at = now;
		t.norm = a.tokens.norm;
		t.object = a.object;
		if (m.turnCount < CONV_TURNS)
			++m.turnCount;

		++m.talks;
		m.lastPlayerAt = now;
		const EIntent subj = a.intent == I_FOLLOW_UP ? a.subject : a.intent;
		if (IsGameIntent(subj))
		{
			m.lastGameIntent = subj;
			m.lastGameAt = now;
			m.generalStreak = 0;
		}
		else if (a.intent == I_GENERAL || (a.intent == I_ANSWER_TO_BOT && a.topic != T_NONE) ||
				(a.intent == I_UNKNOWN_STATEMENT && a.topic != T_NONE))
			++m.generalStreak;
		if (a.topic != T_NONE)
		{
			m.lastTopic = a.topic;
			m.lastTopicAt = now;
		}
		// Abuse counts wherever it sits: "przestan do mnie pisac gold diggerze"
		// is a request to stop and an insult at once. Banter does not count.
		if (a.intent == I_INSULT || a.intent == I_THREAT || a.concepts.Has(C_INSULT))
			++m.negative;
		if (a.intent == I_THANKS || a.intent == I_PRAISE || a.thanksToo || a.intent == I_APOLOGY)
			++m.positive;
		if (a.intent == I_APOLOGY && m.negative > 0)
			--m.negative;
		if (a.intent == I_STOP_TALKING)
		{
			const u32 until = now + CONV_QUIET_MS;
			m.quietUntil = until != 0 ? until : 1;
		}
		if (a.intent == I_APOLOGY)
			m.quietUntil = 0;
		if (a.repeated)
			++m.repeatCount;
		// MT2009_PLUS_BOT_CHAT_V2: what the line does to the bot's patience.
		{
			int delta = 0;
			if (a.intent == I_INSULT || a.concepts.Has(C_INSULT))
				delta -= 18;
			else if (a.intent == I_THREAT)
				delta -= 12;
			else if (a.intent == I_MOCK)
				delta -= 5;
			if (a.repeated)
				delta -= 8;
			if (a.intent == I_UNKNOWN_QUESTION || a.intent == I_UNKNOWN_STATEMENT)
				delta -= 3;
			if (a.intent == I_BEG)
				delta -= 6;
			if (a.intent == I_THANKS || a.intent == I_PRAISE || a.thanksToo)
				delta += 10;
			if (a.intent == I_APOLOGY)
				delta += 35;
			UpdatePatience(m, delta, now);
			if (a.intent == I_IS_BOT)
				++m.botAsked;
			if (a.intent == I_BEG)
				++m.begAsked;
		}
		// "lubie zime" - one thing to remember about the person.
		const int lubie = a.tokens.Find("lubie");
		if (lubie >= 0)
		{
			const std::string obj = ExtractObjectAfter(a.tokens, lubie, 2);
			if (!obj.empty())
			{
				if (lubie > 0 && a.tokens.words[lubie - 1] == "nie")
					m.playerDislikes = obj;
				else
					m.playerLikes = obj;
			}
		}
		// An answer closes the question, and so does a jibe or a "stop"
		// instead of one; anything else lets it expire.
		if (a.intent == I_ANSWER_TO_BOT || a.answeredAsk != ASK_NONE || now - m.botAskAt > CONV_BOT_ASK_TTL_MS)
			m.botAsk = ASK_NONE;
	}

	inline void RememberBotReply(TConvMemory& m, EIntent answered, const std::string& reply, u32 now)
	{
		m.lastAnswered = answered;
		m.lastAnsweredAt = now;
		m.lastReply = reply;
		m.lastBotAt = now;
	}

	// How well the bot knows the person. `affinity` is the AI's own friend
	// ledger (party, gifts, trade - playerbot_guild.h), `sameParty` whether
	// they are in one party right now.
	inline int ComputeTier(const TConvMemory& m, int affinity, bool sameParty)
	{
		if (m.negative >= 3 && m.negative > m.positive + 1)
			return TIER_HOSTILE;
		int score = affinity;
		score += (int)(m.talks > 40 ? 40 : m.talks) / 2;
		score += (int)(m.sessions > 10 ? 10 : m.sessions) * 3;
		score += m.positive * 2 - m.negative * 6;
		if (sameParty)
			score += 15;
		if (score < 6) return TIER_STRANGER;
		if (score < 25) return TIER_KNOWN;
		if (score < 55) return TIER_FRIEND;
		return TIER_BUDDY;
	}
}

#endif
