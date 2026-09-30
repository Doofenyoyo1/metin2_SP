#ifndef __INC_METIN2_PLAYERBOT_EXPLAIN_H__
#define __INC_METIN2_PLAYERBOT_EXPLAIN_H__

// The bots' explanations: why a bot put a line on its counter and how it
// reached the price, and why it changed a worn piece - the question an
// operator wants answered and reported ("why did it swap its Miecz Pelni
// Ksiezyca +6 for a Krwawy Miecz +4"). Two InnoDB tables in `log`
// (mariadb/playerbot/log_schema.sql):
//
//   playerbot_listing - one row per counter line, keyed by the line's item
//     id: inserted when the line goes up, updated in place when it is
//     repriced, put right, raised to its floor, taken off, taken back to wear
//     or sold;
//   playerbot_equip - one row per equipment decision: the old piece against
//     the new, the score's terms of both, the rule that took it.
//
// Numbers only, in the codes of playerbot_explain_rules.h; the classic panel
// says them (the "?" of a line in a bot's shop, the gear history's Decisions
// tab, the /decisions page).
//
// Nothing here waits for the database. A row is a VALUES tuple on a queue;
// once a second the queue goes to the log database's own asynchronous
// connection (LogManager::Query, a thread of its own), consecutive rows of one
// statement shape in one INSERT up to the engine's 4 KB query buffer. The
// volume is the decisions themselves: a counter line is one row updated in
// place, an equipment change one row, and a bot that changes gear more than
// PLAYERBOT_EXPLAIN_EQUIP_ROWS_PER_HOUR times an hour has the rest counted,
// not written (suppressed, EFLAG_SUPPRESSED_BEFORE on the next row).
//
// EXPLAIN in the weights file (s_iPlayerBotExplainDays, playerbot_config.h):
// 0 records and computes nothing, 1-30 is how many days a row stays, no line
// in the file a week. Once every PLAYERBOT_EXPLAIN_CLEANUP_MS the rows older
// than that are deleted - except a line still standing on a counter, whatever
// its age. The PLAYERBOT_EXPLAIN syslog lines say what was written a minute.
//
// Included once, after playerbot_config.h; the parts that need the rest of
// the AI (the goods, the roles, the equipment rows) are playerbot_explain_late.h.

#include "playerbot_explain_rules.h"

namespace
{
	namespace per = playerbot_explain_rules;

	const DWORD PLAYERBOT_EXPLAIN_FLUSH_MS = 1000;
	const size_t PLAYERBOT_EXPLAIN_STATEMENTS_PER_FLUSH = 40;
	const size_t PLAYERBOT_EXPLAIN_QUEUE_MAX = 20000;
	const size_t PLAYERBOT_EXPLAIN_QUERY_BYTES = 3900;   // the engine's buffer is 4096
	const DWORD PLAYERBOT_EXPLAIN_CLEANUP_MS = 15 * 60 * 1000;
	const DWORD PLAYERBOT_EXPLAIN_CLEANUP_FIRST_MS = 5 * 60 * 1000;
	const int PLAYERBOT_EXPLAIN_CLEANUP_ROWS = 20000;
	const DWORD PLAYERBOT_EXPLAIN_REPORT_MS = 60000;
	const int PLAYERBOT_EXPLAIN_EQUIP_ROWS_PER_HOUR = 30;
	const DWORD PLAYERBOT_EXPLAIN_PENDING_MS = 10 * 60 * 1000;
	const size_t PLAYERBOT_EXPLAIN_ORIGINS_MAX = 20000;
	const DWORD PLAYERBOT_EXPLAIN_ORIGIN_MS = 6 * 60 * 60 * 1000;

	bool IsPlayerBotExplainOn()
	{
#if defined(PLAYERBOT_ENGINE_MT2009)
		return s_iPlayerBotExplainDays > 0;
#else
		return false;
#endif
	}

	long long PlayerBotExplainClamp(long long v, long long lo, long long hi)
	{
		return v < lo ? lo : (v > hi ? hi : v);
	}

	// ---- the price, step by step ---------------------------------------------
	// A trace is open around one pricing whose steps are wanted (a line going
	// up, a reprice); the asking price (GetPlayerBotShopAskingPrice) and the
	// helpers under it add their steps while it is open and they run at the
	// depth of that pricing - the chest's worth or a sash's price asked from
	// inside it add nothing. Everything else prices as it always did: the
	// trace is a pointer that is NULL.
	struct TPlayerBotPriceTrace
	{
		std::vector<per::TPair> steps;
		std::vector<per::TPair> pendingBonus;   // the lines, said beside their premium
		unsigned int flags;
		long long npcUnit;      // what the merchant pays for a unit
		long long sheetUnit;    // the sheet's unit, its lines' premium on it
		bool sheetSeen;
		TPlayerBotPriceTrace() : flags(0), npcUnit(0), sheetUnit(0), sheetSeen(false) {}

		void Step(int code, long long value, long long a = 0, long long b = 0, long long c = 0)
		{
			steps.push_back(per::Pair(code, value, a, b, c));
			if (per::IsSheetStep(code) && !sheetSeen)
			{
				sheetUnit = value;
				sheetSeen = true;
			}
			else if (code == per::STEP_BONUS_PREMIUM && sheetSeen)
				sheetUnit = value;
		}

		std::string Encode() const
		{
			return per::EncodePairs(steps, per::STEPS_COLUMN);
		}

		// The flags the final price of a line raises against its own steps:
		// under what the merchant pays, under half or over three times the
		// sheet's unit.
		unsigned int PriceFlags(long long linePrice, long long count) const
		{
			const long long units = count > 0 ? count : 1;
			const long long unit = linePrice / units;
			unsigned int f = 0;
			if (npcUnit > 0 && unit > 0 && unit < npcUnit)
				f |= per::LFLAG_UNDER_MERCHANT;
			if (sheetSeen)
				f |= per::SheetRatioFlags(unit, sheetUnit);
			return f;
		}
	};

	TPlayerBotPriceTrace* s_pPlayerBotPriceTrace = NULL;
	int s_iPlayerBotPriceDepth = 0;

	// Opens a trace for one pricing when explanations are on and none is open.
	struct TPlayerBotPriceTraceScope
	{
		TPlayerBotPriceTrace trace;
		bool bOpen;
		TPlayerBotPriceTraceScope() : bOpen(false)
		{
			if (IsPlayerBotExplainOn() && !s_pPlayerBotPriceTrace)
			{
				s_pPlayerBotPriceTrace = &trace;
				bOpen = true;
			}
		}
		~TPlayerBotPriceTraceScope()
		{
			if (bOpen)
				s_pPlayerBotPriceTrace = NULL;
		}
		bool On() const { return bOpen; }
		void Step(int code, long long value, long long a = 0, long long b = 0, long long c = 0)
		{
			if (bOpen)
				trace.Step(code, value, a, b, c);
		}
	};

	// The depth of the asking price: the steps of the one being traced are
	// the ones at depth one.
	struct TPlayerBotPriceDepth
	{
		TPlayerBotPriceDepth() { ++s_iPlayerBotPriceDepth; }
		~TPlayerBotPriceDepth() { --s_iPlayerBotPriceDepth; }
	};

	bool IsPlayerBotPriceTracing()
	{
		return s_pPlayerBotPriceTrace != NULL && s_iPlayerBotPriceDepth == 1;
	}

	void PlayerBotPriceStep(int code, long long value, long long a = 0, long long b = 0, long long c = 0)
	{
		if (IsPlayerBotPriceTracing())
			s_pPlayerBotPriceTrace->Step(code, value, a, b, c);
	}

	void PlayerBotPriceFlag(unsigned int flag)
	{
		if (IsPlayerBotPriceTracing())
			s_pPlayerBotPriceTrace->flags |= flag;
	}

	// What the listing does to the asking price - a markdown, a markup, a
	// floor - is said by the code around the asking price, outside it.
	bool IsPlayerBotListingTracing()
	{
		return s_pPlayerBotPriceTrace != NULL && s_iPlayerBotPriceDepth == 0;
	}

	void PlayerBotListingStep(int code, long long value, long long a = 0, long long b = 0, long long c = 0)
	{
		if (IsPlayerBotListingTracing())
			s_pPlayerBotPriceTrace->Step(code, value, a, b, c);
	}

	void PlayerBotListingFlag(unsigned int flag)
	{
		if (IsPlayerBotListingTracing())
			s_pPlayerBotPriceTrace->flags |= flag;
	}

	// A line's worth, said beside the premium it makes (ApplyPlayerBotBonusPremium).
	void PlayerBotPriceBonusStep(int code, long long value, long long a = 0, long long b = 0, long long c = 0)
	{
		if (IsPlayerBotPriceTracing())
			s_pPlayerBotPriceTrace->pendingBonus.push_back(per::Pair(code, value, a, b, c));
	}

	void PlayerBotPriceFlushBonusSteps()
	{
		if (!IsPlayerBotPriceTracing())
			return;
		TPlayerBotPriceTrace& t = *s_pPlayerBotPriceTrace;
		for (size_t i = 0; i < t.pendingBonus.size(); ++i)
			t.steps.push_back(t.pendingBonus[i]);
		t.pendingBonus.clear();
	}

	// ---- why an item is goods ------------------------------------------------
	// ScorePlayerBotShopStock says which branch took an item while one of
	// these is open (PlayerBotGoods at each of its positive returns). The
	// outermost return is the last to write, so it is what stays.
	struct TPlayerBotGoodsWhy
	{
		int code;
		long long a, b, c;
		bool set;
		TPlayerBotGoodsWhy() : code(0), a(0), b(0), c(0), set(false) {}
	};
	TPlayerBotGoodsWhy* s_pPlayerBotGoodsWhy = NULL;

	bool IsPlayerBotGoodsExplaining()
	{
		return s_pPlayerBotGoodsWhy != NULL;
	}

	int PlayerBotGoods(int score, int code, long long a = 0, long long b = 0, long long c = 0)
	{
		if (s_pPlayerBotGoodsWhy && score > 0)
		{
			s_pPlayerBotGoodsWhy->code = code;
			s_pPlayerBotGoodsWhy->a = a;
			s_pPlayerBotGoodsWhy->b = b;
			s_pPlayerBotGoodsWhy->c = c;
			s_pPlayerBotGoodsWhy->set = true;
		}
		return score;
	}

	// How a line was cut off its stack (BotOfflinePrepareLine, the stall's
	// split for a new stand).
	struct TPlayerBotLineCut
	{
		int shape;
		int from;
		int keep;
		TPlayerBotLineCut() : shape(per::SHAPE_WHOLE_STACK), from(0), keep(0) {}
	};

	// ---- an equipment score, term by term -------------------------------------
	// GetPlayerBotEquipmentScore and the weapon's blow fill one of these when
	// they are handed one (playerbot_gear.h); nothing else asks for it.
	struct TPlayerBotScoreTerms
	{
		long long v[per::TERM_SLOTS];
		bool set[per::TERM_SLOTS];
		TPlayerBotScoreTerms()
		{
			for (int i = 0; i < per::TERM_SLOTS; ++i)
			{
				v[i] = 0;
				set[i] = false;
			}
		}
		void Set(int term, long long value)
		{
			if (term > 0 && term < per::TERM_SLOTS)
			{
				v[term] = value;
				set[term] = true;
			}
		}
		void Add(int term, long long value)
		{
			if (term > 0 && term < per::TERM_SLOTS)
			{
				v[term] += value;
				set[term] = true;
			}
		}
		long long Get(int term) const
		{
			return term > 0 && term < per::TERM_SLOTS ? v[term] : 0;
		}
		// The score's own terms first, then the blow's, each only when set -
		// a zero term is written, a term that does not apply is not.
		std::string Encode() const
		{
			std::vector<per::TPair> pairs;
			for (int i = 1; i < per::TERM_SLOTS; ++i)
				if (set[i] && (i < per::TERM_BLOW || i >= per::TERM_LINES_TIERED))
					pairs.push_back(per::Pair(i, v[i]));
			for (int i = per::TERM_BLOW; i < per::TERM_LINES_TIERED; ++i)
				if (set[i])
					pairs.push_back(per::Pair(i, v[i]));
			return per::EncodePairs(pairs, per::TERMS_COLUMN);
		}
	};

	// ---- the rows ------------------------------------------------------------
	struct TPlayerBotListingExplain
	{
		DWORD itemId, pid, vnum;
		long long count;
		int listEvent, standReason;
		int why;
		long long whyA, whyB, whyC;
		long long score;
		int pickRank, candidates, refusedAbove;
		TPlayerBotLineCut cut;
		long long held;
		long long listPrice;
		std::string steps;
		unsigned int flags;
		DWORD at;
		TPlayerBotListingExplain()
			: itemId(0), pid(0), vnum(0), count(0), listEvent(0), standReason(0), why(0), whyA(0), whyB(0),
			  whyC(0), score(0), pickRank(0), candidates(0), refusedAbove(0), held(0), listPrice(0), flags(0), at(0)
		{
		}
	};

	struct TPlayerBotEquipPiece
	{
		DWORD id, vnum;
		int plus;
		long long score;
		unsigned int roles;
		std::string lines, terms;
		long long linesTiered;
		int levelLimit;
		TPlayerBotEquipPiece() : id(0), vnum(0), plus(0), score(0), roles(0), linesTiered(0), levelLimit(0) {}
	};

	struct TPlayerBotEquipExplain
	{
		DWORD pid;
		int level, wear, path, rule, context;
		TPlayerBotEquipPiece newPiece, oldPiece;
		int newOrigin;
		long long newOriginRef;
		unsigned int flags;
		bool ready;
		TPlayerBotEquipExplain()
			: pid(0), level(0), wear(0), path(0), rule(0), context(0), newOrigin(0), newOriginRef(0), flags(0),
			  ready(false)
		{
		}
	};

	// ---- the queue -----------------------------------------------------------
	enum EPlayerBotExplainKind
	{
		PLAYERBOT_EXPLAIN_LISTING_NEW = 0,
		PLAYERBOT_EXPLAIN_LISTING_EVENT,
		PLAYERBOT_EXPLAIN_EQUIP,
		PLAYERBOT_EXPLAIN_RAW,
	};

	struct TPlayerBotExplainQueued
	{
		int kind;
		std::string text;
	};
	std::deque<TPlayerBotExplainQueued> s_dequePlayerBotExplain;

	struct TPlayerBotExplainStats
	{
		unsigned int listings, events, equips, suppressed, statements, dropped, unusual;
		TPlayerBotExplainStats() : listings(0), events(0), equips(0), suppressed(0), statements(0), dropped(0),
			unusual(0) {}
	};
	TPlayerBotExplainStats s_kPlayerBotExplainStats;
	DWORD s_dwPlayerBotExplainFlushAt = 0;
	DWORD s_dwPlayerBotExplainReportAt = 0;
	DWORD s_dwPlayerBotExplainCleanupAt = 0;
	int s_iPlayerBotExplainDaysSeen = -1;

	void QueuePlayerBotExplain(int kind, const std::string& text)
	{
		if (s_dequePlayerBotExplain.size() >= PLAYERBOT_EXPLAIN_QUEUE_MAX)
		{
			s_dequePlayerBotExplain.pop_front();
			++s_kPlayerBotExplainStats.dropped;
		}
		TPlayerBotExplainQueued q;
		q.kind = kind;
		q.text = text;
		s_dequePlayerBotExplain.push_back(q);
	}

	const char* PlayerBotExplainHead(int kind)
	{
		switch (kind)
		{
			case PLAYERBOT_EXPLAIN_LISTING_NEW:
				return "INSERT INTO playerbot_listing (item_id,pid,vnum,`count`,listed_at,list_event,stand_reason,why,"
						"score,why_a,why_b,why_c,pick_rank,candidates,refused_above,cut_shape,cut_from,cut_keep,held,"
						"list_price,list_steps,price,was,last_event,last_steps,last_at,changes,off_reason,sold_price,flags) VALUES ";
			case PLAYERBOT_EXPLAIN_LISTING_EVENT:
				return "INSERT INTO playerbot_listing (item_id,pid,vnum,`count`,price,was,last_event,last_steps,last_at,"
						"changes,off_reason,sold_price,flags) VALUES ";
			case PLAYERBOT_EXPLAIN_EQUIP:
				return "INSERT INTO playerbot_equip (pid,level,wear,path,rule,context,new_id,new_vnum,new_plus,new_score,"
						"new_roles,new_origin,new_origin_ref,new_lines,new_terms,old_id,old_vnum,old_plus,old_score,old_roles,"
						"old_lines,old_terms,suppressed,flags) VALUES ";
			default:
				return "";
		}
	}

	// A new listing replaces whatever the id carried before: a line that came
	// home and went up again is a new listing of the same piece.
	const char* PlayerBotExplainTail(int kind)
	{
		switch (kind)
		{
			case PLAYERBOT_EXPLAIN_LISTING_NEW:
				return " ON DUPLICATE KEY UPDATE pid=VALUES(pid),vnum=VALUES(vnum),`count`=VALUES(`count`),"
						"listed_at=VALUES(listed_at),list_event=VALUES(list_event),stand_reason=VALUES(stand_reason),"
						"why=VALUES(why),score=VALUES(score),why_a=VALUES(why_a),why_b=VALUES(why_b),why_c=VALUES(why_c),"
						"pick_rank=VALUES(pick_rank),candidates=VALUES(candidates),refused_above=VALUES(refused_above),"
						"cut_shape=VALUES(cut_shape),cut_from=VALUES(cut_from),cut_keep=VALUES(cut_keep),held=VALUES(held),"
						"list_price=VALUES(list_price),list_steps=VALUES(list_steps),price=VALUES(price),was=0,"
						"last_event=VALUES(last_event),last_steps='',last_at=VALUES(last_at),changes=0,off_reason=0,"
						"sold_price=0,flags=VALUES(flags)";
			// An event on a line: a change moves the price and says how, an end
			// says why; a line this core never saw go up gets a row of its own
			// (why 0: listed before explanations were recorded).
			case PLAYERBOT_EXPLAIN_LISTING_EVENT:
				return " ON DUPLICATE KEY UPDATE "
						"price=IF(VALUES(last_event) BETWEEN 4 AND 7,VALUES(price),price),"
						"was=IF(VALUES(last_event) BETWEEN 4 AND 7,VALUES(was),was),"
						"last_steps=IF(VALUES(last_event) BETWEEN 4 AND 7,VALUES(last_steps),last_steps),"
						"changes=LEAST(65535,changes+IF(VALUES(last_event) BETWEEN 4 AND 7,1,0)),"
						"off_reason=VALUES(off_reason),"
						"sold_price=IF(VALUES(last_event)=10,VALUES(sold_price),sold_price),"
						"last_event=VALUES(last_event),last_at=VALUES(last_at),flags=flags|VALUES(flags)";
			default:
				return "";
		}
	}

	// ---- per bot -------------------------------------------------------------
	struct TPlayerBotExplainDecision
	{
		DWORD at;
		int wear;
		DWORD newId, oldId;
	};

	struct TPlayerBotExplainBot
	{
		DWORD hourStart;
		int hourRows;
		int suppressed;
		DWORD blacksmithAt;
		DWORD burnAt[WEAR_MAX_NUM];
		DWORD wornId[WEAR_MAX_NUM];
		std::deque<TPlayerBotExplainDecision> recent;
		TPlayerBotExplainBot() : hourStart(0), hourRows(0), suppressed(0), blacksmithAt(0)
		{
			for (int i = 0; i < WEAR_MAX_NUM; ++i)
				burnAt[i] = wornId[i] = 0;
		}
	};
	std::map<DWORD, TPlayerBotExplainBot> s_mapPlayerBotExplainBots;

	TPlayerBotExplainBot& GetPlayerBotExplainBot(DWORD pid)
	{
		return s_mapPlayerBotExplainBots[pid];
	}

	// A blacksmith session put its pieces back (RestorePlayerBotEquipmentAfterRefining).
	void NotePlayerBotExplainBlacksmith(DWORD pid, DWORD now)
	{
		if (IsPlayerBotExplainOn() && pid)
			GetPlayerBotExplainBot(pid).blacksmithAt = now ? now : 1;
	}

	// The worn piece of this slot burned at the anvil.
	void NotePlayerBotExplainBurn(DWORD pid, int wear, DWORD now)
	{
		if (IsPlayerBotExplainOn() && pid && wear >= 0 && wear < WEAR_MAX_NUM)
			GetPlayerBotExplainBot(pid).burnAt[wear] = now ? now : 1;
	}

	// What each slot held when the equipment pass last looked: an equip into
	// an empty slot whose last piece now stands on the bot's own counter is
	// the worn piece gone to the counter (CONTEXT_WENT_TO_COUNTER).
	void NotePlayerBotExplainWorn(LPCHARACTER ch)
	{
		if (!ch || !IsPlayerBotExplainOn())
			return;
		TPlayerBotExplainBot& bot = GetPlayerBotExplainBot(ch->GetPlayerID());
		for (int wear = 0; wear < WEAR_MAX_NUM; ++wear)
			if (LPITEM worn = ch->GetWear((WORD)wear))
				bot.wornId[wear] = worn->GetID();
	}

	// ---- where a piece came from ---------------------------------------------
	struct TPlayerBotExplainOrigin
	{
		int origin;
		long long ref;
		DWORD at;
	};
	std::map<DWORD, TPlayerBotExplainOrigin> s_mapPlayerBotExplainOrigins;

	void NotePlayerBotExplainOrigin(DWORD itemId, int origin, long long ref)
	{
		if (!itemId || !IsPlayerBotExplainOn())
			return;
		const DWORD now = get_dword_time();
		if (s_mapPlayerBotExplainOrigins.size() >= PLAYERBOT_EXPLAIN_ORIGINS_MAX)
		{
			for (std::map<DWORD, TPlayerBotExplainOrigin>::iterator it = s_mapPlayerBotExplainOrigins.begin();
					it != s_mapPlayerBotExplainOrigins.end(); )
			{
				if (now - it->second.at >= PLAYERBOT_EXPLAIN_ORIGIN_MS / 4)
					s_mapPlayerBotExplainOrigins.erase(it++);
				else
					++it;
			}
			if (s_mapPlayerBotExplainOrigins.size() >= PLAYERBOT_EXPLAIN_ORIGINS_MAX)
				s_mapPlayerBotExplainOrigins.clear();
		}
		TPlayerBotExplainOrigin& o = s_mapPlayerBotExplainOrigins[itemId];
		o.origin = origin;
		o.ref = ref;
		o.at = now;
	}

	void NotePlayerBotExplainOrigin(LPITEM item, int origin, long long ref)
	{
		if (item)
			NotePlayerBotExplainOrigin(item->GetID(), origin, ref);
	}

	bool GetPlayerBotExplainOrigin(DWORD itemId, int& origin, long long& ref)
	{
		origin = per::ORIGIN_UNKNOWN;
		ref = 0;
		std::map<DWORD, TPlayerBotExplainOrigin>::const_iterator it = s_mapPlayerBotExplainOrigins.find(itemId);
		if (it == s_mapPlayerBotExplainOrigins.end() || get_dword_time() - it->second.at >= PLAYERBOT_EXPLAIN_ORIGIN_MS)
			return false;
		origin = it->second.origin;
		ref = it->second.ref;
		return true;
	}

	// ---- the cuts a new stand's split made, and the lines it will list -------
	std::map<DWORD, std::pair<TPlayerBotLineCut, DWORD> > s_mapPlayerBotExplainCuts;
	std::map<DWORD, TPlayerBotListingExplain> s_mapPlayerBotListingPending;

	void NotePlayerBotExplainCut(DWORD itemId, const TPlayerBotLineCut& cut)
	{
		if (!itemId || !IsPlayerBotExplainOn())
			return;
		if (s_mapPlayerBotExplainCuts.size() >= 8192)
			s_mapPlayerBotExplainCuts.clear();
		s_mapPlayerBotExplainCuts[itemId] = std::make_pair(cut, get_dword_time());
	}

	bool FindPlayerBotExplainCut(DWORD itemId, TPlayerBotLineCut& cut)
	{
		std::map<DWORD, std::pair<TPlayerBotLineCut, DWORD> >::const_iterator it = s_mapPlayerBotExplainCuts.find(itemId);
		if (it == s_mapPlayerBotExplainCuts.end() || get_dword_time() - it->second.second >= PLAYERBOT_EXPLAIN_PENDING_MS)
			return false;
		cut = it->second.first;
		return true;
	}

	void SetPlayerBotListingPending(const TPlayerBotListingExplain& row)
	{
		if (!row.itemId || !IsPlayerBotExplainOn())
			return;
		if (s_mapPlayerBotListingPending.size() >= 8192)
			s_mapPlayerBotListingPending.clear();
		TPlayerBotListingExplain& kept = s_mapPlayerBotListingPending[row.itemId];
		kept = row;
		kept.at = get_dword_time();
	}

	bool TakePlayerBotListingPending(DWORD itemId, TPlayerBotListingExplain& row)
	{
		std::map<DWORD, TPlayerBotListingExplain>::iterator it = s_mapPlayerBotListingPending.find(itemId);
		if (it == s_mapPlayerBotListingPending.end())
			return false;
		const bool fresh = get_dword_time() - it->second.at < PLAYERBOT_EXPLAIN_PENDING_MS;
		if (fresh)
			row = it->second;
		s_mapPlayerBotListingPending.erase(it);
		return fresh;
	}

	// ---- writing rows ----------------------------------------------------------
	// A column's text is ASCII digits and ";:=~,|" by construction; anything
	// else is dropped rather than escaped.
	std::string PlayerBotExplainText(const std::string& text, size_t cap)
	{
		std::string out;
		out.reserve(std::min(text.size(), cap));
		for (size_t i = 0; i < text.size() && out.size() < cap; ++i)
		{
			const char c = text[i];
			if ((c >= '0' && c <= '9') || c == ';' || c == ':' || c == '=' || c == '~' || c == ',' ||
					c == '|' || c == '-')
				out += c;
		}
		return out;
	}

	void QueuePlayerBotListing(const TPlayerBotListingExplain& r)
	{
		if (!IsPlayerBotExplainOn() || !r.itemId)
			return;
		const long long INT_MAX_ = 2147483647LL, INT_MIN_ = -2147483647LL - 1;
		char buf[2048];
		const std::string steps = PlayerBotExplainText(r.steps, per::STEPS_COLUMN);
		snprintf(buf, sizeof(buf),
				"(%u,%u,%u,%lld,NOW(),%d,%d,%d,%lld,%lld,%lld,%lld,%d,%d,%d,%d,%d,%d,%lld,%lld,'%s',%lld,0,%d,'',NOW(),0,0,0,%u)",
				r.itemId, r.pid, r.vnum, PlayerBotExplainClamp(r.count, 0, 65535),
				(int)PlayerBotExplainClamp(r.listEvent, 0, 255), (int)PlayerBotExplainClamp(r.standReason, 0, 255),
				(int)PlayerBotExplainClamp(r.why, 0, 65535),
				PlayerBotExplainClamp(r.score, INT_MIN_, INT_MAX_),
				PlayerBotExplainClamp(r.whyA, INT_MIN_, INT_MAX_), PlayerBotExplainClamp(r.whyB, INT_MIN_, INT_MAX_),
				PlayerBotExplainClamp(r.whyC, INT_MIN_, INT_MAX_),
				(int)PlayerBotExplainClamp(r.pickRank, 0, 255), (int)PlayerBotExplainClamp(r.candidates, 0, 65535),
				(int)PlayerBotExplainClamp(r.refusedAbove, 0, 255), (int)PlayerBotExplainClamp(r.cut.shape, 0, 255),
				(int)PlayerBotExplainClamp(r.cut.from, 0, 65535), (int)PlayerBotExplainClamp(r.cut.keep, 0, 65535),
				PlayerBotExplainClamp(r.held, 0, 4294967295LL),
				PlayerBotExplainClamp(r.listPrice, 0, 9223372036854775807LL), steps.c_str(),
				PlayerBotExplainClamp(r.listPrice, 0, 9223372036854775807LL),
				(int)PlayerBotExplainClamp(r.listEvent, 0, 255), r.flags);
		QueuePlayerBotExplain(PLAYERBOT_EXPLAIN_LISTING_NEW, buf);
		++s_kPlayerBotExplainStats.listings;
		if (r.flags & per::LISTING_UNUSUAL_MASK)
		{
			++s_kPlayerBotExplainStats.unusual;
			PlayerBotLogThrottled("explain_unusual_listing", get_dword_time(),
					"PLAYERBOT_EXPLAIN: unusual listing pid=%u item=%u vnum=%u count=%lld why=%d price=%lld flags=%u steps=%s",
					r.pid, r.itemId, r.vnum, r.count, r.why, r.listPrice, r.flags, steps.c_str());
		}
	}

	void QueuePlayerBotListingEvent(DWORD itemId, DWORD pid, DWORD vnum, long long count, int event,
			long long price, long long was, const std::string& steps, int offReason, long long soldPrice,
			unsigned int flags)
	{
		if (!IsPlayerBotExplainOn() || !itemId)
			return;
		char buf[1024];
		const std::string text = PlayerBotExplainText(steps, per::STEPS_COLUMN);
		snprintf(buf, sizeof(buf), "(%u,%u,%u,%lld,%lld,%lld,%d,'%s',NOW(),%d,%d,%lld,%u)",
				itemId, pid, vnum, PlayerBotExplainClamp(count, 0, 65535),
				PlayerBotExplainClamp(price, 0, 9223372036854775807LL),
				PlayerBotExplainClamp(was, 0, 9223372036854775807LL), (int)PlayerBotExplainClamp(event, 0, 255),
				text.c_str(), per::IsChangeEvent(event) ? 1 : 0, (int)PlayerBotExplainClamp(offReason, 0, 255),
				PlayerBotExplainClamp(soldPrice, 0, 9223372036854775807LL), flags);
		QueuePlayerBotExplain(PLAYERBOT_EXPLAIN_LISTING_EVENT, buf);
		++s_kPlayerBotExplainStats.events;
		if (flags & per::LISTING_UNUSUAL_MASK)
		{
			++s_kPlayerBotExplainStats.unusual;
			PlayerBotLogThrottled("explain_unusual_event", get_dword_time(),
					"PLAYERBOT_EXPLAIN: unusual %s pid=%u item=%u vnum=%u price=%lld was=%lld flags=%u steps=%s",
					event == per::EVENT_REPRICE ? "reprice" : "change", pid, itemId, vnum, price, was, flags,
					text.c_str());
		}
	}

	// An equipment row, through the bot's hourly budget, with the flip-flop
	// read off the bot's last decisions.
	void QueuePlayerBotEquip(TPlayerBotEquipExplain& r)
	{
		if (!IsPlayerBotExplainOn() || !r.ready || !r.pid)
			return;
		const DWORD now = get_dword_time();
		TPlayerBotExplainBot& bot = GetPlayerBotExplainBot(r.pid);
		// Swapped back: this decision puts on what one within the hour took off
		// the same slot, for the piece that one put on.
		// The Archer's bow and stone weapon take turns by design (the stone
		// switch), and are no flip-flop.
		for (std::deque<TPlayerBotExplainDecision>::const_reverse_iterator it = bot.recent.rbegin();
				r.rule != per::RULE_ARCHER_STONE_SWITCH && it != bot.recent.rend(); ++it)
		{
			if (now - it->at >= per::FLIP_FLOP_WINDOW_SECONDS * 1000U)
				break;
			if (it->wear == r.wear && r.newPiece.id && it->oldId == r.newPiece.id &&
					(!r.oldPiece.id || it->newId == r.oldPiece.id))
			{
				r.flags |= per::EFLAG_FLIP_FLOP;
				break;
			}
		}
		TPlayerBotExplainDecision d;
		d.at = now;
		d.wear = r.wear;
		d.newId = r.newPiece.id;
		d.oldId = r.oldPiece.id;
		bot.recent.push_back(d);
		while (bot.recent.size() > 16)
			bot.recent.pop_front();
		// The budget.
		if (bot.hourStart == 0 || now - bot.hourStart >= 3600000U)
		{
			bot.hourStart = now ? now : 1;
			bot.hourRows = 0;
		}
		if (bot.hourRows >= PLAYERBOT_EXPLAIN_EQUIP_ROWS_PER_HOUR)
		{
			++bot.suppressed;
			++s_kPlayerBotExplainStats.suppressed;
			return;
		}
		++bot.hourRows;
		const int suppressed = bot.suppressed;
		bot.suppressed = 0;
		if (suppressed > 0)
			r.flags |= per::EFLAG_SUPPRESSED_BEFORE;
		const long long LL_MAX_ = 9223372036854775807LL;
		char buf[2048];
		const std::string nl = PlayerBotExplainText(r.newPiece.lines, per::LINES_COLUMN);
		const std::string nt = PlayerBotExplainText(r.newPiece.terms, per::TERMS_COLUMN);
		const std::string ol = PlayerBotExplainText(r.oldPiece.lines, per::LINES_COLUMN);
		const std::string ot = PlayerBotExplainText(r.oldPiece.terms, per::TERMS_COLUMN);
		snprintf(buf, sizeof(buf),
				"(%u,%d,%d,%d,%d,%d,%u,%u,%d,%lld,%u,%d,%lld,'%s','%s',%u,%u,%d,%lld,%u,'%s','%s',%d,%u)",
				r.pid, (int)PlayerBotExplainClamp(r.level, 0, 255), (int)PlayerBotExplainClamp(r.wear, 0, 255),
				(int)PlayerBotExplainClamp(r.path, 0, 255), (int)PlayerBotExplainClamp(r.rule, 0, 65535),
				(int)PlayerBotExplainClamp(r.context, 0, 255),
				r.newPiece.id, r.newPiece.vnum, (int)PlayerBotExplainClamp(r.newPiece.plus, 0, 255),
				PlayerBotExplainClamp(r.newPiece.score, -LL_MAX_, LL_MAX_), r.newPiece.roles,
				(int)PlayerBotExplainClamp(r.newOrigin, 0, 255), PlayerBotExplainClamp(r.newOriginRef, 0, 4294967295LL),
				nl.c_str(), nt.c_str(),
				r.oldPiece.id, r.oldPiece.vnum, (int)PlayerBotExplainClamp(r.oldPiece.plus, 0, 255),
				PlayerBotExplainClamp(r.oldPiece.score, -LL_MAX_, LL_MAX_), r.oldPiece.roles, ol.c_str(), ot.c_str(),
				(int)PlayerBotExplainClamp(suppressed, 0, 65535), r.flags);
		QueuePlayerBotExplain(PLAYERBOT_EXPLAIN_EQUIP, buf);
		++s_kPlayerBotExplainStats.equips;
		if (r.flags & per::EQUIP_UNUSUAL_MASK)
		{
			++s_kPlayerBotExplainStats.unusual;
			PlayerBotLogThrottled("explain_unusual_equip", now,
					"PLAYERBOT_EXPLAIN: unusual equip pid=%u wear=%d path=%d rule=%d new=%u+%d old=%u+%d score=%lld/%lld flags=%u",
					r.pid, r.wear, r.path, r.rule, r.newPiece.vnum, r.newPiece.plus, r.oldPiece.vnum, r.oldPiece.plus,
					r.newPiece.score, r.oldPiece.score, r.flags);
		}
	}

	// ---- the flush, the cleanup, the minute's line ----------------------------
	void FlushPlayerBotExplain(DWORD now)
	{
		if (s_dequePlayerBotExplain.empty())
			return;
		if (s_dwPlayerBotExplainFlushAt != 0 && now - s_dwPlayerBotExplainFlushAt < PLAYERBOT_EXPLAIN_FLUSH_MS)
			return;
		s_dwPlayerBotExplainFlushAt = now ? now : 1;
		size_t statements = 0;
		std::string sql;
		while (!s_dequePlayerBotExplain.empty() && statements < PLAYERBOT_EXPLAIN_STATEMENTS_PER_FLUSH)
		{
			const int kind = s_dequePlayerBotExplain.front().kind;
			if (kind == PLAYERBOT_EXPLAIN_RAW)
			{
				sql = s_dequePlayerBotExplain.front().text;
				s_dequePlayerBotExplain.pop_front();
			}
			else
			{
				const std::string head = PlayerBotExplainHead(kind);
				const std::string tail = PlayerBotExplainTail(kind);
				sql = head;
				size_t rows = 0;
				while (!s_dequePlayerBotExplain.empty() && s_dequePlayerBotExplain.front().kind == kind)
				{
					const std::string& values = s_dequePlayerBotExplain.front().text;
					if (rows > 0 && sql.size() + 1 + values.size() + tail.size() > PLAYERBOT_EXPLAIN_QUERY_BYTES)
						break;
					if (rows > 0)
						sql += ',';
					sql += values;
					++rows;
					s_dequePlayerBotExplain.pop_front();
				}
				sql += tail;
			}
			if (sql.size() >= 4090)
			{
				// One row over the engine's buffer cannot happen by the columns'
				// sizes; if it ever does it is dropped, not cut into bad SQL.
				++s_kPlayerBotExplainStats.dropped;
				continue;
			}
			LogManager::instance().Query("%s", sql.c_str());
			++statements;
		}
		s_kPlayerBotExplainStats.statements += (unsigned int)statements;
	}

	// The rows older than EXPLAIN's days go, a line still on a counter
	// excepted however old it is. Every core asks it (they share the tables and
	// the statement is the same), a few thousand rows a run.
	void CleanPlayerBotExplain(DWORD now)
	{
		const int days = s_iPlayerBotExplainDays;
		if (s_dwPlayerBotExplainCleanupAt == 0)
		{
			// The first run a few minutes after the start, spread by the core's
			// port so the cores of one world do not all ask in one second.
			s_dwPlayerBotExplainCleanupAt = now + PLAYERBOT_EXPLAIN_CLEANUP_FIRST_MS +
					(DWORD)(mother_port % 60) * 1000U;
			return;
		}
		if ((int)(now - s_dwPlayerBotExplainCleanupAt) < 0)
			return;
		s_dwPlayerBotExplainCleanupAt = now + PLAYERBOT_EXPLAIN_CLEANUP_MS;
		if (days <= 0)
			return;
		char sql[512];
		snprintf(sql, sizeof(sql),
				"DELETE FROM playerbot_equip WHERE `time` < NOW() - INTERVAL %d DAY LIMIT %d",
				days, PLAYERBOT_EXPLAIN_CLEANUP_ROWS);
		QueuePlayerBotExplain(PLAYERBOT_EXPLAIN_RAW, sql);
		snprintf(sql, sizeof(sql),
				"DELETE FROM playerbot_listing WHERE last_at < NOW() - INTERVAL %d DAY AND item_id NOT IN "
				"(SELECT id FROM player.item WHERE `window` = 'IKASHOP_OFFLINESHOP') LIMIT %d",
				days, PLAYERBOT_EXPLAIN_CLEANUP_ROWS);
		QueuePlayerBotExplain(PLAYERBOT_EXPLAIN_RAW, sql);
		sys_log(0, "PLAYERBOT_EXPLAIN: cleanup asked, rows older than %d days (lines still on a counter kept)", days);
	}

	// Once a manager tick: the queue to the database, the cleanup's clock, and
	// a line a minute of what was written.
	void ManagePlayerBotExplain(DWORD now)
	{
		const int days = s_iPlayerBotExplainDays;
		if (days != s_iPlayerBotExplainDaysSeen)
		{
			sys_log(0, "PLAYERBOT_EXPLAIN: %s (EXPLAIN %d)",
					days > 0 ? "recording the bots' decisions" : "off, nothing recorded", days);
			if (days <= 0)
			{
				s_mapPlayerBotListingPending.clear();
				s_mapPlayerBotExplainCuts.clear();
				s_mapPlayerBotExplainOrigins.clear();
				s_mapPlayerBotExplainBots.clear();
			}
			s_iPlayerBotExplainDaysSeen = days;
		}
		CleanPlayerBotExplain(now);
		FlushPlayerBotExplain(now);
		if (s_dwPlayerBotExplainReportAt == 0)
			s_dwPlayerBotExplainReportAt = now;
		else if (now - s_dwPlayerBotExplainReportAt >= PLAYERBOT_EXPLAIN_REPORT_MS)
		{
			const TPlayerBotExplainStats& st = s_kPlayerBotExplainStats;
			if (st.listings || st.events || st.equips || st.suppressed || st.dropped || !s_dequePlayerBotExplain.empty())
				sys_log(0, "PLAYERBOT_EXPLAIN: minute listings=%u events=%u equips=%u unusual=%u suppressed=%u statements=%u queued=%u dropped=%u days=%d",
						st.listings, st.events, st.equips, st.unusual, st.suppressed, st.statements,
						(unsigned int)s_dequePlayerBotExplain.size(), st.dropped, days);
			s_kPlayerBotExplainStats = TPlayerBotExplainStats();
			s_dwPlayerBotExplainReportAt = now;
			// The notes of bots long gone.
			for (std::map<DWORD, TPlayerBotListingExplain>::iterator it = s_mapPlayerBotListingPending.begin();
					it != s_mapPlayerBotListingPending.end(); )
			{
				if (now - it->second.at >= PLAYERBOT_EXPLAIN_PENDING_MS)
					s_mapPlayerBotListingPending.erase(it++);
				else
					++it;
			}
		}
	}

	// Defined in playerbot_explain_late.h, after the whole AI.
	bool ExplainPlayerBotGoods(LPCHARACTER ch, LPITEM item, bool merchant, TPlayerBotGoodsWhy& why, int& score);
	void PreparePlayerBotEquipExplain(TPlayerBotEquipExplain& row, LPCHARACTER ch, int wear, int path, int rule,
			LPITEM newItem, LPITEM oldItem, bool oldZeroed);
	unsigned int GetPlayerBotListingWornFlags(LPCHARACTER ch, LPITEM item);
}

#endif
