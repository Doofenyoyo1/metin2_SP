#ifndef __INC_METIN2_PLAYERBOT_EXPLAIN_RULES_H__
#define __INC_METIN2_PLAYERBOT_EXPLAIN_RULES_H__

// The words of the bots' explanations (playerbot_explain.h): why a line went
// on a counter and how its price was reached (log.playerbot_listing), why a
// worn piece was changed (log.playerbot_equip). The core writes numbers only;
// the classic panel says them in the page's language (admin_panel.py, the
// DECISION_* tables and DECISION_CODE_NAMES, which carry the same names).
//
// A number never changes once shipped and a new one is appended. The numbers
// from 200 up are this project's own - goods, reasons and steps the design the
// panel was written for does not have (Cor Draconis and the sashes, the guild
// materials, the Dragon Stones, the Dozorca's exchange) - so a code appended
// upstream can never mean two things here.
//
// Two text encodings, both ASCII:
//   steps and terms - "code=value[:a[:b[:c]]]" joined by ";", the trailing
//     zero parameters left out, and ";~" at the end when the column ran out
//     of room (EncodePairs);
//   lines - "type:value" joined by ",", then "|" and the vnums of the seated
//     stones joined by "," (EncodeLines). A line's type is the item's own
//     attrtype, a POINT number on mt2009.
//
// Nothing here knows the engine: the rules are plain C++ so the encodings can
// be tested on their own.

#include <string>
#include <vector>
#include <cstdio>
#include <cstddef>

namespace playerbot_explain_rules
{
	// ---- the columns --------------------------------------------------------
	const size_t STEPS_COLUMN = 720;   // list_steps, last_steps
	const size_t TERMS_COLUMN = 360;   // new_terms, old_terms
	const size_t LINES_COLUMN = 96;    // new_lines, old_lines

	// A swap back to the piece taken off within this long is a flip-flop.
	const unsigned int FLIP_FLOP_WINDOW_SECONDS = 3600;

	// ---- how a line went up, changed or came down (list_event, last_event) --
	enum EEvent
	{
		EVENT_NONE = 0,
		EVENT_LIST_CREATE = 1,      // a new stand's line
		EVENT_LIST_ADD = 2,         // added on a service visit
		EVENT_LIST_CLASSIC = 3,     // a classic stall's line (r40250)
		EVENT_REPRICE = 4,
		EVENT_SLIP_FIX = 5,         // the keeper put its slip right
		EVENT_SLIP_FIX_CORE = 6,    // the core put a slip right
		EVENT_RAISE_TO_FLOOR = 7,
		EVENT_TAKE_OFF = 8,
		EVENT_RECLAIM = 9,          // taken back to wear
		EVENT_SOLD = 10,
	};

	inline bool IsChangeEvent(int e) { return e >= EVENT_REPRICE && e <= EVENT_RAISE_TO_FLOOR; }
	inline bool IsEndEvent(int e) { return e >= EVENT_TAKE_OFF && e <= EVENT_SOLD; }

	// ---- why an item is goods at all (why): ScorePlayerBotShopStock's branches
	enum EGoods
	{
		GOODS_UNKNOWN = 0,
		GOODS_POLICY_STALL = 1,              // a = policy
		GOODS_GAMBLE_GOODS = 2,              // a = plus
		GOODS_GM_STONE_SPARE = 3,            // a = in the bag, b = keep
		GOODS_MARBLE = 4,                    // a = mob
		GOODS_METIN_DETECTOR = 5,
		GOODS_CATACOMB_HEAD = 6,
		GOODS_BONUS_STONE_SPARE = 7,         // a = in the bag, b = keep
		GOODS_FORGET_SCROLL_MERCHANT_ONLY = 8, // a = skill
		GOODS_STALKI_OTHER = 9,              // a = plus, b = level, c = STALKI_WHY
		GOODS_LEVEL30_SALE_READY = 10,       // a = plus, b = average %, c = target plus
		GOODS_LEVEL30_NOT_KEPT = 11,         // a = plus, b = average %, c = L30_WHY
		GOODS_LOW_LEVEL_LOW_PLUS = 12,       // a = plus, b = level
		GOODS_LOW_LEVEL_REFINED = 13,        // a = plus, b = threshold, c = level
		GOODS_LPP_SURPLUS = 14,              // a = plus, b = valuable
		GOODS_VALUABLE_BONUS = 15,           // a = apply, b = value
		GOODS_MERCHANT_ONLY_GEAR = 16,       // a = plus
		GOODS_PRECIOUS_SPARE = 17,           // a = plus, b = worn vnum, c = worn plus
		GOODS_SAFE_SCROLL_OVER_KEEP = 18,    // a = held, b = keep, c = SCROLL_KEEP
		GOODS_HORSE_MEDAL = 19,              // a = in the bag, b = keep, c = MEDAL_WHY
		GOODS_MATERIAL_FLOOR = 20,           // a = in the bag, b = reserve, c = village supply
		GOODS_MATERIAL_SHORT = 21,           // a = in the bag, b = reserve, c = bots short
		GOODS_MATERIAL_OTHER = 22,           // a = in the bag, b = reserve, c = MAT_WHY
		GOODS_PICKUP_GOODS = 23,             // a = plus
		GOODS_SURPLUS_POTION = 24,
		GOODS_SURPLUS_RECIPE = 25,
		GOODS_HAIR_DYE_SHOP = 26,
		GOODS_HAIR_DYE_FISHED_KEPT = 27,
		GOODS_ISHOP_HAIRSTYLE = 28,
		GOODS_FORGET_SCROLL = 29,            // a = skill
		GOODS_MAGIC_DUST = 30,               // a = in the bag, b = keep
		GOODS_SOUL_STONE_LOW_MARKET = 31,    // a = grade, b = kind
		GOODS_SOUL_STONE = 32,               // a = grade, b = kind
		GOODS_GENERAL_BOOK = 33,             // a = ahead, b = keep, c = BOOK_USEFUL
		GOODS_SHEET_GOODS = 34,
		GOODS_SKILL_BOOK_DROPPER = 35,       // a = skill, b = keep
		GOODS_SKILL_BOOK_OWN_SPARE = 36,     // a = skill, b = keep
		GOODS_SKILL_BOOK_OTHER_CLASS = 37,   // a = skill
		GOODS_LOW_PLUS_GEAR = 38,            // a = plus
		GOODS_SCRAP_KEEPER_LOW = 39,         // a = plus
		GOODS_SURPLUS_CHEST = 40,            // a = in the bag, b = hold
		GOODS_SURPLUS_KEY = 41,              // a = in the bag, b = keep
		// This project's own.
		GOODS_RARE_GOODS = 200,              // a Cor Draconis or a sash, a player's goods
		GOODS_GUILD_MATERIAL = 201,          // a guild building material its guild does not keep
		GOODS_DRAGON_STONE_SPARE = 202,      // a Dragon Stone the bot has no use for
		// MT2009_PLUS_BOTLIFE_V1: a jewellery refine stone over what the bot's
		// own sockets take.
		GOODS_ACCESSORY_STONE_SPARE = 203,   // a = in the bag, b = keep
	};

	// ---- what shape the line was cut to (cut_shape) --------------------------
	enum EShape
	{
		SHAPE_WHOLE_STACK = 0,
		SHAPE_NATURAL_LINE = 1,
		SHAPE_POTION_PACK = 2,
		SHAPE_DUST_PACK = 3,
		SHAPE_MEDAL_PAIR = 4,
		SHAPE_COUNTED_SINGLE = 5,
		SHAPE_KEY_SINGLE = 6,
		SHAPE_CHEST_PACK = 7,
		SHAPE_CLASSIC_SPLIT_SINGLE = 8,
	};

	// ---- why a line came home (off_reason): BotOfflineUnwantedLine's words --
	enum EOff
	{
		OFF_NONE = 0,
		OFF_LPP = 1,
		OFF_SOUL_STONE_DUST = 2,
		OFF_MARBLE = 3,
		OFF_SAME_VNUM = 4,
		OFF_MISSION_BOOKS = 5,
		OFF_JUNK_WEAPON = 6,
		OFF_POTION_PACK = 7,
		OFF_LOW_ARMOUR = 8,
		OFF_LOW_JEWEL = 9,
		OFF_HAIR_DYE = 10,
		OFF_LEVEL30_ANVIL = 11,
		OFF_CHEST_PACK = 12,
		OFF_GM_STONE = 13,
		OFF_SCROLL_PACK = 14,
		OFF_MEDAL_PACK = 15,
		OFF_HEAP_PACK = 16,
		OFF_MATERIAL_PACK = 17,
		OFF_LOW_GEAR = 18,
		OFF_TACKLE = 19,
		OFF_RECLAIM_TO_WEAR = 20,
		// This project's own.
		OFF_BUFF_POTION = 200,       // the bot's own green or purple potion
		OFF_DS_LOW_GRADE = 201,      // an ordinary or brilliant Dragon Stone, material now
		OFF_SASH_KEEPER = 202,       // a sash its keeper wants for its own
		OFF_CRAFT_EXCHANGE = 203,    // refine goods unsold, for the Dozorca's exchange
		OFF_RARE_UNSOLD = 204,       // a Cor Draconis or a sash unsold through the markdown
	};

	// The take-off's reason word (BotOfflineUnwantedLine) as its code.
	inline int OffReasonCode(const char* why)
	{
		struct TWord { const char* word; int code; };
		static const TWord WORDS[] = {
			{ "lpp", OFF_LPP }, { "soul_stone_dust", OFF_SOUL_STONE_DUST }, { "marble", OFF_MARBLE },
			{ "same_vnum", OFF_SAME_VNUM }, { "mission_books", OFF_MISSION_BOOKS },
			{ "junk_weapon", OFF_JUNK_WEAPON }, { "potion_pack", OFF_POTION_PACK },
			{ "low_armour", OFF_LOW_ARMOUR }, { "low_jewel", OFF_LOW_JEWEL }, { "hair_dye", OFF_HAIR_DYE },
			{ "level30_anvil", OFF_LEVEL30_ANVIL }, { "chest_pack", OFF_CHEST_PACK }, { "gm_stone", OFF_GM_STONE },
			{ "scroll_pack", OFF_SCROLL_PACK }, { "medal_pack", OFF_MEDAL_PACK }, { "heap_pack", OFF_HEAP_PACK },
			{ "material_pack", OFF_MATERIAL_PACK }, { "low_gear", OFF_LOW_GEAR }, { "tackle", OFF_TACKLE },
			{ "buff_potion", OFF_BUFF_POTION }, { "ds_low_grade", OFF_DS_LOW_GRADE },
			{ "sash_keeper", OFF_SASH_KEEPER }, { "craft_exchange", OFF_CRAFT_EXCHANGE },
			{ "rare_unsold", OFF_RARE_UNSOLD },
		};
		if (!why || !*why)
			return OFF_NONE;
		for (size_t i = 0; i < sizeof(WORDS) / sizeof(WORDS[0]); ++i)
		{
			const char* a = WORDS[i].word;
			const char* b = why;
			while (*a && *a == *b)
			{
				++a;
				++b;
			}
			if (*a == 0 && *b == 0)
				return WORDS[i].code;
		}
		return OFF_NONE;
	}

	// ---- one step of the price (list_steps, last_steps) ----------------------
	// value is the price after the step, except where the comment names it.
	enum EStep
	{
		STEP_CONTEXT = 1,            // value = curve %, a = yang rate %, b = inflation x10000, c = steps
		STEP_BONUS_LINE = 2,         // value = x100, a = apply, b = value, c = at its top
		STEP_BONUS_MAX_LINES = 3,    // value = x100, a = lines at their top
		STEP_BONUS_PERCENT = 4,      // value = %, a = the capped product x100
		STEP_INVESTMENT = 5,         // value = yang, a = plus
		STEP_SHEET_GEAR = 6,         // a = sheet yang, b = vnum, c = plus
		STEP_SOCKET_STONES = 7,      // value = %, a = stones
		STEP_FLAT_PLUS = 8,          // a = plus
		STEP_SCRAP = 9,              // a = merchant yang, b = x100
		STEP_INVESTMENT_FLOOR = 10,  // a = the blacksmith's bill
		STEP_BONUS_PREMIUM = 11,     // a = %
		STEP_COMPETITION = 12,       // a = signed %, b = PREVIEW
		STEP_SHEET_MARBLE = 13,      // a = mob, b = MARBLE_SRC
		STEP_COUNT = 14,             // a = count
		STEP_PRIOR_MERCHANT = 15,    // a = merchant yang, b = markup %
		STEP_PRIOR_LEVEL30 = 16,
		STEP_SHEET_BOOK = 17,        // a = skill, b = sheet yang
		STEP_SHEET_GENERAL_BOOK = 18, // a = vnum
		STEP_SHEET_MATERIAL = 19,    // a = sheet yang
		STEP_CHEST_WORTH = 20,       // a = sheet yang, b = worth yang
		STEP_SHEET_FORGET_SCROLL = 21, // a = skill
		STEP_FIXED_PRIOR = 22,       // a = FIXED
		STEP_SHEET_SOUL_STONE = 23,  // a = grade, b = kind, c = GRADE_TABLE
		STEP_NO_MERCHANT_PRIOR = 24,
		STEP_WALLET = 25,            // a = median wallet, b = permille, c = worth %
		STEP_WALLET_STACK_CAP = 26,  // value = yang a unit, a = stack %
		STEP_SALE_MEMORY = 27,       // a = median paid, b = sales
		STEP_LEDGER = 28,            // a = bots short, b = units on counters, c = x100
		STEP_STEP_LIMIT = 29,        // a = wanted
		STEP_SPREAD = 30,            // a = %
		STEP_BONUS_GOODS_FLOOR = 31, // a = floor a unit
		STEP_ROUND = 32,
		STEP_POOR_DISCOUNT = 33,     // a = %
		STEP_MARKDOWN_CLOCK = 34,    // a = int, b = CLOCK_UNIT, c = CLOCK_RESTART
		STEP_MARKDOWN = 35,          // a = %
		STEP_MARKUP = 36,            // a = %, b = missing %, c = lines sold
		STEP_MARKUP_REFUSED = 37,    // a = %
		STEP_LISTING_FLOOR = 38,     // a = floor
		STEP_SLIP = 39,              // a = meant
		STEP_GENERATION = 40,        // a = GENERATION
		STEP_SLIP_PUT_RIGHT = 41,    // a = was, b = minutes, c = SLIP_BY
		STEP_RAISED_TO_FLOOR = 42,   // a = was
		// This project's own.
		STEP_OPERATOR_PRICE = 200,   // a = yang a unit: Cor Draconis, a Dragon Stone, a sash, crafting goods
	};

	// The steps whose value is the sheet's price for one unit.
	inline bool IsSheetStep(int code)
	{
		return code == STEP_SHEET_GEAR || code == STEP_SHEET_MARBLE || code == STEP_SHEET_BOOK ||
				code == STEP_SHEET_GENERAL_BOOK || code == STEP_SHEET_MATERIAL || code == STEP_CHEST_WORTH ||
				code == STEP_SHEET_FORGET_SCROLL || code == STEP_SHEET_SOUL_STONE;
	}

	// ---- the listing flags (flags) -------------------------------------------
	enum ELFlag
	{
		LFLAG_UNDER_MERCHANT = 1,
		LFLAG_FLOOR_BOUND = 2,
		LFLAG_UNDER_SHEET_HALF = 4,
		LFLAG_OVER_SHEET_3X = 8,
		LFLAG_MEMORY_PULL = 16,
		LFLAG_REGULATOR_EDGE = 32,
		LFLAG_MARKDOWN_MAX = 64,
		LFLAG_MARKUP_MAX = 128,
		LFLAG_SLIP = 256,
		LFLAG_BETTER_THAN_WORN_SOON = 512,
		LFLAG_LOW_RANK = 1024,
		LFLAG_PRICE_JUMP = 2048,
	};
	const unsigned int LISTING_UNUSUAL_MASK = LFLAG_UNDER_MERCHANT | LFLAG_UNDER_SHEET_HALF | LFLAG_OVER_SHEET_3X |
			LFLAG_MEMORY_PULL | LFLAG_BETTER_THAN_WORN_SOON | LFLAG_PRICE_JUMP;

	// ---- the equipment flags (flags) -----------------------------------------
	enum EEFlag
	{
		EFLAG_PLUS_DOWN = 1,
		EFLAG_LEVEL_DOWN = 2,
		EFLAG_LINES_WORSE = 4,
		EFLAG_SMALL_GAIN = 8,
		EFLAG_FLIP_FLOP = 16,
		EFLAG_OLD_ZEROED = 32,
		EFLAG_PROJECT_REPLACED = 64,
		EFLAG_SUPPRESSED_BEFORE = 128,
		EFLAG_AFTER_BURN = 256,
	};
	const unsigned int EQUIP_UNUSUAL_MASK = EFLAG_PLUS_DOWN | EFLAG_LEVEL_DOWN | EFLAG_LINES_WORSE |
			EFLAG_SMALL_GAIN | EFLAG_FLIP_FLOP | EFLAG_PROJECT_REPLACED | EFLAG_SUPPRESSED_BEFORE;

	// ---- which pass, which rule, which moment --------------------------------
	enum EPath
	{
		PATH_EQUIP_PASS = 1,
		PATH_EMPTY_HAND = 2,
		PATH_EMERGENCY_BUY = 3,
		PATH_PROFESSION_OFF = 4,
		PATH_UNIQUE = 5,
	};

	enum ERule
	{
		RULE_SCORE_UPGRADE = 1,
		RULE_EMPTY_SLOT = 2,
		RULE_OWNER_PIN = 3,
		RULE_OLD_LOW_WEAPON_BANNED = 4,
		RULE_ARCHER_STONE_SWITCH = 5,
		RULE_EMPTY_HAND_BEST = 6,
		RULE_EMPTY_HAND_LOW_FALLBACK = 7,
		RULE_EMERGENCY_WEAPON = 8,
		RULE_PROFESSION_MISMATCH = 9,
		RULE_UNIQUE_NEVER_WORN = 10,
	};

	enum EContext
	{
		CONTEXT_NONE = 0,
		CONTEXT_AFTER_BLACKSMITH = 1,
		CONTEXT_AFTER_BURN = 2,
		CONTEXT_AFTER_SPAWN = 3,
		CONTEXT_RETURNED_FROM_COUNTER = 4,
		CONTEXT_WENT_TO_COUNTER = 5,
	};
	const unsigned int AFTER_SPAWN_SECONDS = 120;
	const unsigned int AFTER_BLACKSMITH_SECONDS = 300;
	const unsigned int AFTER_BURN_SECONDS = 900;
	const unsigned int AFTER_RECLAIM_SECONDS = 900;

	// ---- where the new piece came from (new_origin, new_origin_ref) ----------
	enum EOrigin
	{
		ORIGIN_UNKNOWN = 0,
		ORIGIN_NPC_LADDER = 1,        // ref = LADDER_CAT
		ORIGIN_NPC_EMERGENCY = 2,     // ref = yang
		ORIGIN_NPC_PROPER_WEAPON = 3, // ref = yang
		ORIGIN_REFINED = 4,           // ref = the item id it was refined from
		ORIGIN_RECLAIMED = 5,         // ref = the gain
		ORIGIN_SAFEBOX = 6,           // ref = SAFEBOX_WHY
		ORIGIN_GAMBLER = 7,           // ref = kind
	};

	enum ELadderCat
	{
		LADDER_WEAPON = 1, LADDER_ARMOUR = 2, LADDER_SHIELD = 3, LADDER_HELMET = 4, LADDER_BOOTS = 5,
		LADDER_BRACELET = 6, LADDER_NECKLACE = 7, LADDER_EARRINGS = 8, LADDER_BACKUP_WEAPON = 9,
		LADDER_BACKUP_ARMOUR = 10,
	};

	// The merchant visit's category word (BuyPlayerBotProgressionGear) as its code.
	inline int LadderCategoryCode(const char* category)
	{
		struct TWord { const char* word; int code; };
		static const TWord WORDS[] = {
			{ "weapon", LADDER_WEAPON }, { "stone dagger", LADDER_WEAPON }, { "armor", LADDER_ARMOUR },
			{ "shield", LADDER_SHIELD }, { "helmet", LADDER_HELMET }, { "boots", LADDER_BOOTS },
			{ "wrist", LADDER_BRACELET }, { "necklace", LADDER_NECKLACE }, { "earring", LADDER_EARRINGS },
			{ "backup weapon", LADDER_BACKUP_WEAPON }, { "backup armor", LADDER_BACKUP_ARMOUR },
		};
		if (!category)
			return 0;
		for (size_t i = 0; i < sizeof(WORDS) / sizeof(WORDS[0]); ++i)
		{
			const char* a = WORDS[i].word;
			const char* b = category;
			while (*a && *a == *b)
			{
				++a;
				++b;
			}
			if (*a == 0 && *b == 0)
				return WORDS[i].code;
		}
		return 0;
	}

	// ---- what a piece is to its bot (new_roles, old_roles) -------------------
	enum ERole
	{
		ROLE_LEVEL30_FAMILY = 1,
		ROLE_CLASS_LEVEL30 = 2,
		ROLE_LEVEL30_PROJECT = 4,
		ROLE_LINES_PROJECT = 8,
		ROLE_STALKI = 16,
		ROLE_STALKI_KEPT = 32,
		ROLE_HIGHER_TIER_SPARE = 64,
		ROLE_BACKUP = 128,
		ROLE_SCROLL_RULE_PIECE = 256,
		ROLE_PRIZE = 512,
		ROLE_LISTED_JEWEL = 1024,
		ROLE_SOUL_STONES = 2048,
		ROLE_GAMBLE_SET = 4096,
		ROLE_OWNER_PINNED = 8192,
		ROLE_BANNED_LOW_WEAPON = 16384,
		ROLE_SCROLL_ONLY_WEAPON = 32768,
	};
	const unsigned int ROLE_PROJECT_OR_PRIZE = ROLE_LEVEL30_PROJECT | ROLE_LINES_PROJECT | ROLE_PRIZE;

	// ---- the score's terms and the weapon blow's (new_terms, old_terms) ------
	enum ETerm
	{
		TERM_TOTAL = 1,
		TERM_BLOW_X1000 = 2,
		TERM_CLASS_PREF = 3,
		TERM_DEFENCE_X1000 = 4,
		TERM_LEVEL_TIE = 5,
		TERM_PROTO_APPLIES = 6,
		TERM_LINES = 7,
		TERM_SOUL_STONES = 8,
		TERM_IMMUNE = 9,
		TERM_RACE_LINES = 10,
		TERM_TIER_PCT = 11,
		TERM_LISTED_JEWEL_PCT = 12,
		TERM_BOOTS_FAMILY = 13,
		TERM_LEVEL_LIMIT = 14,
		TERM_PLUS = 15,
		TERM_BLOW = 20,
		TERM_ROLL = 21,
		TERM_MAGIC_ROLL = 22,
		TERM_PLUS_ATTACK = 23,
		TERM_GRADE = 24,
		TERM_ATT_PCT = 25,
		TERM_RACE_PCT = 26,
		TERM_AVG_PCT = 27,
		TERM_SKILL_PCT = 28,
		TERM_CRIT_PCT = 29,
		TERM_PEN_PCT = 30,
		TERM_LEVEL_BONUS_PCT = 31,
		TERM_ATTACK = 32,
		TERM_MOB_DEFENCE = 33,
		TERM_HIT = 34,
		TERM_SKILL_HIT = 35,
		TERM_STYLE = 36,
		TERM_HIT_SHARE = 37,
		TERM_SKILL_SHARE = 38,
		TERM_LINES_TIERED = 40,
		TERM_SLOTS = 41,             // the size of the table below
	};

	// ---- the small vocabularies a parameter can be one of ---------------------
	enum EStalkiWhy { STALKI_OTHER_CLASS = 1, STALKI_TOO_FAR = 2, STALKI_SECOND_COPY = 3 };
	enum EL30Why { L30_PROJECT = 1, L30_OWN_CLASS = 2, L30_OTHER_CLASS = 3, L30_DRAW_COUNTER = 4, L30_BAG_FULL = 5,
		L30_DRAW_KEPT = 6 };
	enum EScrollKeep { SCROLL_KEEP_NONE = 0, SCROLL_KEEP_WORN = 1, SCROLL_KEEP_TRADER = 2, SCROLL_KEEP_RULE = 3 };
	enum EMedalWhy { MEDAL_MERCHANT = 1, MEDAL_DROPPER = 2, MEDAL_OVER_KEEP = 3, MEDAL_HORSE_TOP = 4 };
	enum EMatWhy { MAT_PROBE = 1, MAT_NO_DEMAND = 2, MAT_OVERSTOCK = 3 };
	enum EFixed { FIXED_PEARL_WHITE = 1, FIXED_PEARL_BLUE = 2, FIXED_PEARL_RED = 3, FIXED_SHELLFISH = 4,
		FIXED_HORSE_MEDAL = 5, FIXED_CATACOMB_HEAD = 6, FIXED_HAIRSTYLE = 7, FIXED_MAGIC_DUST = 8 };
	enum EClockUnit { CLOCK_STANDS = 0, CLOCK_MINUTES = 1 };
	enum ESlipBy { SLIP_BY_KEEPER = 0, SLIP_BY_CORE_AWAY = 1, SLIP_BY_CORE_HELD = 2, SLIP_BY_CORE_LATE = 3 };
	enum ESafeboxWhy { SAFEBOX_STALKI = 1, SAFEBOX_LPP = 2, SAFEBOX_GAMBLE = 3, SAFEBOX_KEY = 4, SAFEBOX_OTHER = 9 };

	// ---- the encodings -------------------------------------------------------
	struct TPair
	{
		int code;
		long long value, a, b, c;
	};

	inline TPair Pair(int code, long long value, long long a = 0, long long b = 0, long long c = 0)
	{
		TPair p = { code, value, a, b, c };
		return p;
	}

	// "code=value[:a[:b[:c]]]" joined by ";" - trailing zero parameters left
	// out, and ";~" when what is left does not fit in `cap` characters. A part
	// is never cut in half: the text ends on a whole one.
	inline std::string EncodePairs(const std::vector<TPair>& pairs, size_t cap)
	{
		std::string out;
		for (size_t i = 0; i < pairs.size(); ++i)
		{
			const TPair& p = pairs[i];
			char part[128];
			int n = snprintf(part, sizeof(part), "%d=%lld", p.code, p.value);
			const long long params[3] = { p.a, p.b, p.c };
			int last = -1;
			for (int k = 0; k < 3; ++k)
				if (params[k] != 0)
					last = k;
			for (int k = 0; k <= last && n > 0 && n < (int)sizeof(part); ++k)
				n += snprintf(part + n, sizeof(part) - (size_t)n, ":%lld", params[k]);
			if (n <= 0 || n >= (int)sizeof(part))
				continue;
			const size_t need = (out.empty() ? 0 : 1) + (size_t)n;
			// Room for the ";~" of a later cut, unless this is the last part.
			const size_t reserve = i + 1 < pairs.size() ? 2 : 0;
			if (out.size() + need + reserve > cap)
			{
				if (out.size() + 2 <= cap)
					out += out.empty() ? "~" : ";~";
				return out;
			}
			if (!out.empty())
				out += ';';
			out.append(part, (size_t)n);
		}
		return out;
	}

	// "type:value" joined by ",", then "|" and the stones joined by ",".
	// Whole elements only; the lines go first and the stones after them.
	inline std::string EncodeLines(const std::vector<std::pair<int, long long> >& lines,
			const std::vector<unsigned int>& stones, size_t cap)
	{
		std::string out;
		for (size_t i = 0; i < lines.size(); ++i)
		{
			char part[48];
			const int n = snprintf(part, sizeof(part), "%s%d:%lld", out.empty() ? "" : ",",
					lines[i].first, lines[i].second);
			if (n <= 0 || out.size() + (size_t)n > cap)
				return out;
			out.append(part, (size_t)n);
		}
		if (stones.empty())
			return out;
		if (out.size() + 1 > cap)
			return out;
		out += '|';
		bool first = true;
		for (size_t i = 0; i < stones.size(); ++i)
		{
			char part[24];
			const int n = snprintf(part, sizeof(part), "%s%u", first ? "" : ",", stones[i]);
			if (n <= 0 || out.size() + (size_t)n > cap)
				break;
			out.append(part, (size_t)n);
			first = false;
		}
		return out;
	}

	// ---- the listing flags a price's own steps raise --------------------------
	// A unit against the sheet's price for one: under half, or over three times.
	inline unsigned int SheetRatioFlags(long long unitPrice, long long sheetUnit)
	{
		if (unitPrice <= 0 || sheetUnit <= 0)
			return 0;
		if (unitPrice * 2 < sheetUnit)
			return LFLAG_UNDER_SHEET_HALF;
		if (unitPrice > sheetUnit * 3)
			return LFLAG_OVER_SHEET_3X;
		return 0;
	}

	// The sale memory moved a unit by more than half of it, either way.
	inline bool IsMemoryPull(long long before, long long after)
	{
		if (before <= 0 || after <= 0)
			return false;
		const long long moved = after > before ? after - before : before - after;
		return moved * 2 > before;
	}

	// A reprice that moved a line by more than half of what it asked.
	inline bool IsPriceJump(long long was, long long now)
	{
		if (was <= 0 || now <= 0)
			return false;
		const long long moved = now > was ? now - was : was - now;
		return moved * 2 > was;
	}

	// A gain under two percent of the old score.
	inline bool IsSmallGain(long long oldScore, long long newScore)
	{
		return oldScore > 0 && newScore > oldScore && (newScore - oldScore) * 100 < oldScore * 2;
	}
}

#endif
