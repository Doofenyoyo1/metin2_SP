#ifndef __INC_PLAYERBOT_PROGRESSION_RULES_H__
#define __INC_PLAYERBOT_PROGRESSION_RULES_H__

// MT2009_PLUS_PROGRESSION_V1: a bot's road through the world as the
// operator's table, the pure half (no engine types).
//
// "Aby w panelu admina zaawansowanym byly poziomy przejscia na kolejne mapy
// ... i zeby te poziomy mozna bylo dostosowac pod siebie. Fajnie by bylo moc
// zrobic swoista check liste dla botow, co maja zrobic przed przeexpieniem
// danego etapu" (the owner, 1 October). Three things live in one file the
// Seban panel writes, /opt/m2spool/playerbot_progression.tsv:
//
//   * the map transitions - the levels the frontier draw
//     (GetPlayerBotFrontierMapForLevelRaw) and the village ceiling read, which
//     were constants in playerbot_types.h and playerbot_travel.h;
//   * the early holds - Iwakura's Grinder tiers and the Law of Advancement
//     (playerbot_persona_rules.h), which kept a fresh world's bots for hours
//     in the first and the second village;
//   * the checklist - gates at a level a bot may reach but not pass (the
//     engine's AFFECT_EXP_BLOCK, the same lock ManagePlayerBotExpLock puts on
//     a dropper) until it has what the gate asks for.
//
// The file, one line a row, tab separated, '#' starts a comment:
//
//   map   <key> <from> <to>
//   tier  <n> <band_from> <band_to> <lock_from> <lock_to> <on|off>
//   law   <from_level> <weapon+> <armour+> <shield+> <helmet+>
//   gate  <level> <req> <on|off> <a> <b> [<quest flag>]
//   set   <key> <value>
//
// A section the file does not mention keeps its defaults; "law" and "gate"
// lines replace the whole default list once there is one of them ("gates
// none" / "laws none" empty it). Every value is clamped on the way in.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>

namespace playerbot_progression
{
	// ---------------------------------------------------------------------
	// The map transitions. `from` is where the frontier draw starts sending a
	// bot there, `to` where it stops (only where the travel code has an upper
	// end at all: hasTo). `floor` is the lowest `from` the panel accepts - the
	// hubs of a map are planted for its monsters, so a map opened far below
	// them has nowhere to hunt (ChoosePlayerBotHuntingHub shifts its hub
	// levels by the difference, HubLevelShift, up to this much).
	// ---------------------------------------------------------------------
	enum EMapRow
	{
		MAP_M2 = 0,          // the second village: 20-21 partly, all from 22; ceiling 35
		MAP_ISLANDS,         // Orc Valley's Fanatic islands and the desert, 30..
		MAP_ORC_VALLEY,      // Orc Valley proper
		MAP_DESERT,          // Yongbi Desert, shared with the valley
		MAP_SOHAN,
		MAP_SPIDER1,
		MAP_HWANG,
		MAP_SPIDER2,
		MAP_DEMON_TOWER,
		MAP_FOREST,
		MAP_FIRE_LAND,
		MAP_RED_FOREST,
		MAP_GROTTO1,
		MAP_GROTTO2,
		MAP_ROW_COUNT
	};

	struct TMapRowDef
	{
		const char* key;
		uint8_t from;
		uint8_t to;
		bool hasTo;
		uint8_t floor;
	};

	// Today's built-in values (playerbot_types.h, playerbot_travel.h).
	// MT2009_PLUS_PROGRESSION_V2: the owner's upper limits of 1 October - the
	// first Spider Dungeon and the Hwang Temple to 61, the second Spider
	// Dungeon to 78, the Forest to 72; a bot past one draws higher ground
	// (PlayerBotMapUnderCeiling, playerbot_travel.h).
	const TMapRowDef MAP_DEFAULTS[MAP_ROW_COUNT] = {
		{ "m2",          20, 35, true,  15 },
		{ "islands",     30, 35, true,  25 },
		{ "orc_valley",  36, 55, true,  30 },
		{ "desert",      30, 47, true,  25 },
		{ "sohan",       48, 75, false, 40 },
		{ "spider1",     48, 61, true,  42 },
		{ "hwang",       52, 61, true,  45 },
		{ "spider2",     54, 78, true,  48 },
		{ "demon_tower", 57, 255, false, 50 },
		{ "forest",      62, 72, true,  55 },
		{ "fire_land",   66, 80, true,  60 },
		{ "red_forest",  71, 255, false, 65 },
		{ "grotto1",     78, 255, false, 72 },
		{ "grotto2",     84, 255, false, 78 },
	};

	struct TMapRow
	{
		uint8_t from;
		uint8_t to;
	};

	// ---------------------------------------------------------------------
	// The checklist.
	// ---------------------------------------------------------------------
	enum EReq
	{
		REQ_WEAPON = 0,  // a = min item level, b = min plus
		REQ_ARMOUR,
		REQ_HELMET,
		REQ_SHIELD,      // only for a bot that carries one (not a bow, not two hands)
		REQ_SHOES,
		REQ_BRACELET,
		REQ_NECKLACE,
		REQ_EARRINGS,
		REQ_ALL_WORN,    // every worn piece of the eight slots above
		REQ_HP,          // a = min MAX_HP from worn items (lines + the items' own applies)
		REQ_SKILLS,      // a = how many of the build's skills, b = at least this skill level
		REQ_HORSE,       // a = min horse level (11 = the battle horse, 21 = the military one)
		REQ_METINS,      // a = Metin stones broken (the engine's stat_stone)
		REQ_ORC_TEETH,   // the Biologist's Orc Teeth handed in (collect_quest_lv30 past its teeth)
		REQ_QUEST_FLAG,  // flag >= a
		REQ_GOLD,        // a = yang in hand
		REQ_COUNT
	};

	const char* const REQ_KEYS[REQ_COUNT] = {
		"weapon", "armour", "helmet", "shield", "shoes", "bracelet", "necklace", "earrings",
		"all_worn", "hp", "skills", "horse", "metins", "orc_teeth", "quest_flag", "gold"
	};

	// What the bot has to do about a requirement - the engine side pushes the
	// matching errand (playerbot_progression.h).
	enum ENeed
	{
		NEED_GEAR = 1,
		NEED_HP = 2,
		NEED_SKILLS = 4,
		NEED_HORSE = 8,
		NEED_METINS = 16,
		NEED_ORC_TEETH = 32,
		NEED_OTHER = 64,
	};

	inline int NeedOf(int type)
	{
		switch (type)
		{
			case REQ_HP: return NEED_HP;
			case REQ_SKILLS: return NEED_SKILLS;
			case REQ_HORSE: return NEED_HORSE;
			case REQ_METINS: return NEED_METINS;
			case REQ_ORC_TEETH: return NEED_ORC_TEETH;
			case REQ_QUEST_FLAG:
			case REQ_GOLD: return NEED_OTHER;
			default: return NEED_GEAR;
		}
	}

	struct TReq
	{
		uint8_t type;
		bool on;
		long long a;
		int b;
		std::string flag;
		TReq() : type(REQ_WEAPON), on(true), a(0), b(0) {}
		TReq(uint8_t t, long long va, int vb, bool enabled = true, const char* f = "") :
			type(t), on(enabled), a(va), b(vb), flag(f ? f : "") {}
	};

	struct TGate
	{
		uint8_t level;
		std::vector<TReq> reqs;
		TGate() : level(0) {}
	};

	// ---------------------------------------------------------------------
	// The early holds (copied into playerbot_persona's tables on a load).
	// ---------------------------------------------------------------------
	struct TTierRow
	{
		uint8_t tier, bandFrom, bandTo, lockFrom, lockTo;
		bool on;
	};
	const unsigned int TIER_ROWS = 5;
	const TTierRow TIER_DEFAULTS[TIER_ROWS] = {
		{ 1, 10, 18, 13, 19, true },
		{ 2, 19, 25, 19, 25, true },
		{ 3, 26, 35, 30, 35, true },
		{ 5, 36, 50, 40, 48, true },
		{ 7, 51, 65, 55, 62, true },
	};

	struct TLaw
	{
		uint8_t fromLevel, weapon, armour, shield, helmet;
	};
	const unsigned int LAW_MAX = 8;

	struct TConfig
	{
		// The checklist as a whole; the maps and the early holds work whatever
		// this says.
		bool enabled;
		TMapRow maps[MAP_ROW_COUNT];
		std::vector<TGate> gates;
		// How long a bot may stand at one gate before it is let through and
		// the gate logged as given up (minutes of play at the gate).
		unsigned int timeoutMin;
		// A gate holds a bot of its level and up to this many levels above it:
		// one that was already past it when the gate appeared is held where it
		// stands, not pulled down. 0 is the gate's level only.
		unsigned int retro;
		// Fishing: may a held bot with unmet requirements fish at all, and the
		// most of a ten-level band (30-39, 40-49, ...) at the water at once.
		bool fishWhenHeld;
		unsigned int fishCapPct;
		// The veins and Baek-Go's board, for a held bot.
		bool sideWhenHeld;
		// The early holds.
		TTierRow tiers[TIER_ROWS];
		unsigned int tier1SkipPct;
		TLaw laws[LAW_MAX];
		unsigned int lawCount;
		unsigned int lawWindow;
		unsigned int lawPremiumWindow;
		unsigned int advanceChance;
		unsigned int advanceFirstMin;
		unsigned int advanceRollMin;
	};

	inline void AddDefaultGates(std::vector<TGate>& gates)
	{
		gates.clear();
		// MT2009_PLUS_PROGRESSION_V2: the owner's gates of 1 October. Two or
		// more rows of one piece at one gate are alternatives - any one of
		// them met is the piece met (MeetsAlternative).
		// 35: the owner's own example - a battle horse and fifty Metins - and
		// the gear the Law of Advancement asked of a Grinder leaving its tier
		// below 35 (weapon +7, armour +6), for a piece no more than twenty
		// levels under the gate (the law's window).
		TGate g35;
		g35.level = 35;
		// V2: a weapon of 15 at +7, or of 16-29 at +6, or of 30 at +4; an
		// armour of 16 or more at +6; thirty Metins, was fifty.
		g35.reqs.push_back(TReq(REQ_WEAPON, 15, 7));
		g35.reqs.push_back(TReq(REQ_WEAPON, 16, 6));
		g35.reqs.push_back(TReq(REQ_WEAPON, 30, 4));
		g35.reqs.push_back(TReq(REQ_ARMOUR, 16, 6));
		// MT2009_PLUS_PROGRESSION_V3: a war horse of level 5 (was 11) and ten
		// Metins (was thirty) - the owner's 2 October.
		g35.reqs.push_back(TReq(REQ_HORSE, 5, 0));
		g35.reqs.push_back(TReq(REQ_METINS, 10, 0));
		gates.push_back(g35);
		// 45: the owner's example - the Orc Teeth handed in, two skills at M4
		// (24), 2000 HP from items - and the law's window again (a level-25
		// weapon at +7; a level-30 one is what nearly every bot carries) with
		// an armour of the 26 family at +6.
		TGate g45;
		g45.level = 45;
		// V2: a weapon of 25 at +7 or of 30 at +6, two skills at M4 and a
		// battle horse past 11; the 2000 HP and the Orc Teeth are gone.
		g45.reqs.push_back(TReq(REQ_WEAPON, 25, 7));
		g45.reqs.push_back(TReq(REQ_WEAPON, 30, 6));
		g45.reqs.push_back(TReq(REQ_ARMOUR, 26, 6));
		g45.reqs.push_back(TReq(REQ_SKILLS, 2, 24));
		g45.reqs.push_back(TReq(REQ_HORSE, 12, 0));
		gates.push_back(g45);
		// 55: the law's hard row (weapon +8, shield and helmet +6) for the
		// frontier past the valley.
		TGate g55;
		g55.level = 55;
		// V2: the weapon at +7, was +8; the 2500 HP are gone.
		g55.reqs.push_back(TReq(REQ_WEAPON, 30, 7));
		g55.reqs.push_back(TReq(REQ_ARMOUR, 34, 6));
		g55.reqs.push_back(TReq(REQ_HELMET, 0, 6));
		g55.reqs.push_back(TReq(REQ_SHIELD, 0, 6));
		g55.reqs.push_back(TReq(REQ_SKILLS, 3, 24));
		gates.push_back(g55);
	}

	inline void AddDefaultLaws(TConfig& c)
	{
		static const TLaw rows[] = {
			{ 0, 5, 4, 0, 0 },
			{ 19, 6, 5, 4, 0 },
			// MT2009_PLUS_PROGRESSION_V2: the weapon one plus lower from 26
			// (+6) and from 35 (+7).
			{ 26, 6, 5, 5, 0 },
			{ 35, 7, 6, 6, 6 },
		};
		c.lawCount = sizeof(rows) / sizeof(rows[0]);
		for (unsigned int i = 0; i < c.lawCount; ++i)
			c.laws[i] = rows[i];
	}

	inline TConfig Defaults()
	{
		TConfig c;
		c.enabled = true;
		for (int i = 0; i < MAP_ROW_COUNT; ++i)
		{
			c.maps[i].from = MAP_DEFAULTS[i].from;
			c.maps[i].to = MAP_DEFAULTS[i].to;
		}
		AddDefaultGates(c.gates);
		c.timeoutMin = 180;
		c.retro = 9;
		c.fishWhenHeld = false;
		c.fishCapPct = 10;
		c.sideWhenHeld = false;
		for (unsigned int i = 0; i < TIER_ROWS; ++i)
			c.tiers[i] = TIER_DEFAULTS[i];
		c.tier1SkipPct = 25;
		AddDefaultLaws(c);
		c.lawWindow = 20;
		c.lawPremiumWindow = 30;
		c.advanceChance = 60;
		c.advanceFirstMin = 2;
		c.advanceRollMin = 30;
		return c;
	}

	// The configuration in force: the defaults until the file says otherwise.
	inline TConfig& Config()
	{
		static TConfig s_config = Defaults();
		return s_config;
	}

	inline uint8_t MapFrom(int row) { return Config().maps[row].from; }
	inline uint8_t MapTo(int row) { return Config().maps[row].to; }

	// How far a map was opened below its built-in entry: its hubs, planted
	// for the built-in band, are read that many levels lower.
	inline int HubLevelShift(int row)
	{
		if (row < 0 || row >= MAP_ROW_COUNT)
			return 0;
		const int shift = (int)MAP_DEFAULTS[row].from - (int)Config().maps[row].from;
		return shift > 0 ? shift : 0;
	}

	// ---------------------------------------------------------------------
	// Parsing. Clamped, never rejected: the panel validates, a hand edit that
	// goes out of range still gives a working world.
	// ---------------------------------------------------------------------
	inline int Clamp(long v, long lo, long hi)
	{
		return (int)std::max(lo, std::min(hi, v));
	}

	inline bool ParseOn(const char* word, bool fallback)
	{
		if (!word || !*word)
			return fallback;
		if (!strcmp(word, "on") || !strcmp(word, "1") || !strcmp(word, "tak"))
			return true;
		if (!strcmp(word, "off") || !strcmp(word, "0") || !strcmp(word, "nie"))
			return false;
		return fallback;
	}

	inline int ReqTypeOf(const char* key)
	{
		for (int i = 0; i < REQ_COUNT; ++i)
			if (key && !strcmp(key, REQ_KEYS[i]))
				return i;
		return -1;
	}

	inline int MapRowOf(const char* key)
	{
		for (int i = 0; i < MAP_ROW_COUNT; ++i)
			if (key && !strcmp(key, MAP_DEFAULTS[i].key))
				return i;
		return -1;
	}

	// The state of one read: which list-sections the file has replaced.
	struct TParseState
	{
		bool gatesSeen;
		bool lawsSeen;
		int rows;
		TParseState() : gatesSeen(false), lawsSeen(false), rows(0) {}
	};

	inline void SplitTabs(char* line, std::vector<char*>& out)
	{
		out.clear();
		char* hash = strchr(line, '#');
		if (hash)
			*hash = 0;
		char* p = line;
		while (*p)
		{
			while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
				++p;
			if (!*p)
				break;
			char* start = p;
			while (*p && *p != '\t' && *p != '\r' && *p != '\n' && *p != ' ')
				++p;
			if (*p)
				*p++ = 0;
			out.push_back(start);
		}
	}

	inline void ParseLine(TConfig& c, TParseState& st, char* line)
	{
		std::vector<char*> f;
		SplitTabs(line, f);
		if (f.empty())
			return;
		const char* kind = f[0];
		if (!strcmp(kind, "map") && f.size() >= 4)
		{
			const int row = MapRowOf(f[1]);
			if (row < 0)
				return;
			const int from = Clamp(atol(f[2]), MAP_DEFAULTS[row].floor, 250);
			const int to = MAP_DEFAULTS[row].hasTo ? Clamp(atol(f[3]), from, 255) : MAP_DEFAULTS[row].to;
			c.maps[row].from = (uint8_t)from;
			c.maps[row].to = (uint8_t)to;
			++st.rows;
		}
		else if (!strcmp(kind, "tier") && f.size() >= 6)
		{
			const int n = atoi(f[1]);
			for (unsigned int i = 0; i < TIER_ROWS; ++i)
			{
				if (c.tiers[i].tier != n)
					continue;
				TTierRow& t = c.tiers[i];
				t.bandFrom = (uint8_t)Clamp(atol(f[2]), 1, 250);
				t.bandTo = (uint8_t)Clamp(atol(f[3]), t.bandFrom, 250);
				t.lockFrom = (uint8_t)Clamp(atol(f[4]), t.bandFrom, 250);
				t.lockTo = (uint8_t)Clamp(atol(f[5]), t.lockFrom, 250);
				t.on = ParseOn(f.size() >= 7 ? f[6] : "on", true);
				++st.rows;
			}
		}
		else if (!strcmp(kind, "laws") && f.size() >= 2 && !strcmp(f[1], "none"))
		{
			st.lawsSeen = true;
			c.lawCount = 0;
		}
		else if (!strcmp(kind, "law") && f.size() >= 6)
		{
			if (!st.lawsSeen)
			{
				st.lawsSeen = true;
				c.lawCount = 0;
			}
			if (c.lawCount >= LAW_MAX)
				return;
			TLaw& l = c.laws[c.lawCount++];
			l.fromLevel = (uint8_t)Clamp(atol(f[1]), 0, 250);
			l.weapon = (uint8_t)Clamp(atol(f[2]), 0, 9);
			l.armour = (uint8_t)Clamp(atol(f[3]), 0, 9);
			l.shield = (uint8_t)Clamp(atol(f[4]), 0, 9);
			l.helmet = (uint8_t)Clamp(atol(f[5]), 0, 9);
			++st.rows;
		}
		else if (!strcmp(kind, "gates") && f.size() >= 2 && !strcmp(f[1], "none"))
		{
			st.gatesSeen = true;
			c.gates.clear();
		}
		else if (!strcmp(kind, "gate") && f.size() >= 5)
		{
			if (!st.gatesSeen)
			{
				st.gatesSeen = true;
				c.gates.clear();
			}
			const int level = Clamp(atol(f[1]), 1, 250);
			const int type = ReqTypeOf(f[2]);
			if (type < 0)
				return;
			TReq r;
			r.type = (uint8_t)type;
			r.on = ParseOn(f[3], true);
			r.a = std::max(0LL, atoll(f[4]));
			r.b = f.size() >= 6 ? Clamp(atol(f[5]), 0, 255) : 0;
			if (f.size() >= 7)
				r.flag = std::string(f[6]).substr(0, 47);
			if (type == REQ_QUEST_FLAG && r.flag.empty())
				return;
			TGate* gate = NULL;
			for (size_t i = 0; i < c.gates.size(); ++i)
				if (c.gates[i].level == level)
					gate = &c.gates[i];
			if (!gate)
			{
				TGate g;
				g.level = (uint8_t)level;
				c.gates.push_back(g);
				gate = &c.gates.back();
			}
			if (gate->reqs.size() < 24)
				gate->reqs.push_back(r);
			++st.rows;
		}
		else if (!strcmp(kind, "set") && f.size() >= 3)
		{
			const char* key = f[1];
			const long v = atol(f[2]);
			if (!strcmp(key, "enabled")) c.enabled = ParseOn(f[2], true);
			else if (!strcmp(key, "timeout_min")) c.timeoutMin = (unsigned)Clamp(v, 10, 24 * 60);
			else if (!strcmp(key, "retro")) c.retro = (unsigned)Clamp(v, 0, 255);
			else if (!strcmp(key, "fish_when_held")) c.fishWhenHeld = ParseOn(f[2], false);
			else if (!strcmp(key, "fish_cap_pct")) c.fishCapPct = (unsigned)Clamp(v, 0, 100);
			else if (!strcmp(key, "side_when_held")) c.sideWhenHeld = ParseOn(f[2], false);
			else if (!strcmp(key, "tier1_skip_pct")) c.tier1SkipPct = (unsigned)Clamp(v, 0, 100);
			else if (!strcmp(key, "law_window")) c.lawWindow = (unsigned)Clamp(v, 0, 100);
			else if (!strcmp(key, "law_premium_window")) c.lawPremiumWindow = (unsigned)Clamp(v, 0, 100);
			else if (!strcmp(key, "advance_chance")) c.advanceChance = (unsigned)Clamp(v, 1, 100);
			else if (!strcmp(key, "advance_first_min")) c.advanceFirstMin = (unsigned)Clamp(v, 0, 600);
			else if (!strcmp(key, "advance_roll_min")) c.advanceRollMin = (unsigned)Clamp(v, 1, 600);
			else
				return;
			++st.rows;
		}
	}

	inline void SortGates(TConfig& c)
	{
		std::sort(c.gates.begin(), c.gates.end(),
				[](const TGate& x, const TGate& y) { return x.level < y.level; });
	}

	// ---------------------------------------------------------------------
	// The checklist against what a bot has. The engine side measures; this
	// only compares and words the answer (ASCII Polish, the panel's and the
	// log's).
	// ---------------------------------------------------------------------
	struct TPiece
	{
		bool present;
		uint8_t level;
		uint8_t plus;
		TPiece() : present(false), level(0), plus(0) {}
	};

	// The eight slots REQ_WEAPON..REQ_EARRINGS name, in that order.
	const int SLOT_COUNT = 8;

	struct TSnapshot
	{
		uint8_t level;
		TPiece slot[SLOT_COUNT];
		bool wantsShield;
		long long hp;
		// The build's skills, their levels.
		uint8_t skills[16];
		int skillCount;
		int horse;
		long long metins;
		bool orcTeeth;
		long long gold;
		TSnapshot() : level(1), wantsShield(true), hp(0), skillCount(0), horse(0), metins(0),
			orcTeeth(false), gold(0)
		{
			memset(skills, 0, sizeof(skills));
		}
	};

	inline const char* SlotName(int slot)
	{
		static const char* const names[SLOT_COUNT] = {
			"bron", "zbroja", "helm", "tarcza", "buty", "bransoleta", "naszyjnik", "kolczyki"
		};
		return slot >= 0 && slot < SLOT_COUNT ? names[slot] : "?";
	}

	inline bool PieceMeets(const TPiece& p, long long minLevel, int minPlus)
	{
		return p.present && (long long)p.level >= minLevel && (int)p.plus >= minPlus;
	}

	// Whether a requirement is met; `why` gets a short line when it is not.
	// A quest flag is the engine's to read: `flagValue` is what it found.
	inline bool Meets(const TReq& r, const TSnapshot& s, long long flagValue, std::string& why)
	{
		char buf[96];
		buf[0] = 0;
		bool ok = true;
		switch (r.type)
		{
			case REQ_WEAPON: case REQ_ARMOUR: case REQ_HELMET: case REQ_SHIELD:
			case REQ_SHOES: case REQ_BRACELET: case REQ_NECKLACE: case REQ_EARRINGS:
			{
				const int slot = r.type - REQ_WEAPON;
				if (r.type == REQ_SHIELD && !s.wantsShield)
					return true;
				const TPiece& p = s.slot[slot];
				ok = PieceMeets(p, r.a, r.b);
				if (!ok)
				{
					if (p.present)
						snprintf(buf, sizeof(buf), "%s lv%lld+%d (ma lv%u+%u)", SlotName(slot), r.a, r.b,
								(unsigned)p.level, (unsigned)p.plus);
					else
						snprintf(buf, sizeof(buf), "%s lv%lld+%d (brak)", SlotName(slot), r.a, r.b);
				}
				break;
			}
			case REQ_ALL_WORN:
				for (int slot = 0; slot < SLOT_COUNT && ok; ++slot)
				{
					const TPiece& p = s.slot[slot];
					if (p.present && !PieceMeets(p, r.a, r.b))
					{
						ok = false;
						snprintf(buf, sizeof(buf), "caly ekwipunek lv%lld+%d (%s lv%u+%u)", r.a, r.b,
								SlotName(slot), (unsigned)p.level, (unsigned)p.plus);
					}
				}
				break;
			case REQ_HP:
				ok = s.hp >= r.a;
				if (!ok)
					snprintf(buf, sizeof(buf), "PZ z przedmiotow %lld (ma %lld)", r.a, s.hp);
				break;
			case REQ_SKILLS:
			{
				int have = 0;
				for (int i = 0; i < s.skillCount && i < 16; ++i)
					if ((int)s.skills[i] >= r.b)
						++have;
				ok = have >= r.a;
				if (!ok)
					snprintf(buf, sizeof(buf), "%lld umiej. na %d+ (ma %d)", r.a, r.b, have);
				break;
			}
			case REQ_HORSE:
				ok = s.horse >= r.a;
				if (!ok)
					snprintf(buf, sizeof(buf), "kon poz. %lld (ma %d)", r.a, s.horse);
				break;
			case REQ_METINS:
				ok = s.metins >= r.a;
				if (!ok)
					snprintf(buf, sizeof(buf), "metiny %lld (ma %lld)", r.a, s.metins);
				break;
			case REQ_ORC_TEETH:
				ok = r.a <= 0 || s.orcTeeth;
				if (!ok)
					snprintf(buf, sizeof(buf), "zeby orka u Biologa");
				break;
			case REQ_QUEST_FLAG:
				ok = flagValue >= r.a;
				if (!ok)
					snprintf(buf, sizeof(buf), "%s >= %lld (ma %lld)", r.flag.c_str(), r.a, flagValue);
				break;
			case REQ_GOLD:
				ok = s.gold >= r.a;
				if (!ok)
					snprintf(buf, sizeof(buf), "yang %lld (ma %lld)", r.a, s.gold);
				break;
			default:
				return true;
		}
		if (!ok)
			why = buf;
		return ok;
	}

	// MT2009_PLUS_PROGRESSION_V2: another enabled row of the same piece at the
	// same gate that is met - the gate's rows of one piece are alternatives.
	inline bool MeetsAlternative(const TGate& g, size_t index, const TSnapshot& s)
	{
		const TReq& r = g.reqs[index];
		if (r.type > REQ_EARRINGS)
			return false;
		for (size_t i = 0; i < g.reqs.size(); ++i)
		{
			if (i == index || !g.reqs[i].on || g.reqs[i].type != r.type)
				continue;
			std::string unused;
			if (Meets(g.reqs[i], s, 0, unused))
				return true;
		}
		return false;
	}

	// Of the rows of one piece, the first enabled one speaks for the rest when
	// none is met: one line in the log, not three.
	inline bool IsFirstOfPiece(const TGate& g, size_t index)
	{
		const TReq& r = g.reqs[index];
		if (r.type > REQ_EARRINGS)
			return true;
		for (size_t i = 0; i < index; ++i)
			if (g.reqs[i].on && g.reqs[i].type == r.type)
				return false;
		return true;
	}

	// The gate that holds a bot of this level, or NULL: the lowest gate at or
	// under its level, within `retro` of it, not given up (waivedUpTo), with
	// an enabled requirement - the caller then evaluates it.
	inline const TGate* GateFor(const TConfig& c, uint8_t level, unsigned int waivedUpTo, size_t start = 0)
	{
		for (size_t i = start; i < c.gates.size(); ++i)
		{
			const TGate& g = c.gates[i];
			if (g.level > level || (unsigned)level > (unsigned)g.level + c.retro ||
					(unsigned)g.level <= waivedUpTo)
				continue;
			for (size_t j = 0; j < g.reqs.size(); ++j)
				if (g.reqs[j].on)
					return &g;
		}
		return NULL;
	}
}

#endif
