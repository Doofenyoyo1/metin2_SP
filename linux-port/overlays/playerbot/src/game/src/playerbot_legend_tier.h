#ifndef __INC_METIN2_PLAYERBOT_LEGEND_TIER_H__
#define __INC_METIN2_PLAYERBOT_LEGEND_TIER_H__

// MT2009_PLUS_LEGENDS_V1: a bot's tier in the System Legend, as every core
// knows it, and the numbers each tier changes.
//
// The tiers are the owner's (2 October): Wyrozniajacy sie (about four bots in
// a hundred), Specjalny (about two), Chodzaca Legenda (the 27 of
// PLAYERBOT_LEGEND_NAMES, MT2009_PLUS_LEGEND_NAMES_V1) and Czempion Krolestwa (one of a kingdom's Legends, the one whose
// guild leads the kingdom's ranking). A bot keeps its tier for good: it is a
// row of player.playerbot_legend, which every core reads again every
// PLAYERBOT_LEGEND_RELOAD_MS - the row is the truth, and a bot without one
// has no tier until its upkeep (playerbot_legends.h) writes the one its pid
// draws. Everything that changes what a bot of a tier does asks here: the
// engine's hooks (the blow, the experience, a death), the refine's chance
// (GoblinRefineBonus), the books and the Spirit Stones, the potions, the
// choice of a foe, the skills against a person, the gear, the parties and the
// guilds. Under the LEGENDS switch off every answer is the tierless one.
//
// An implementation fragment in the sense playerbot_types.h describes:
// included once, after playerbot_config.h. The rest of the system is
// playerbot_legends.h, late in the include order.

namespace
{
	enum EPlayerBotLegendTier
	{
		BOT_LEGEND_NONE = 0,
		BOT_LEGEND_DISTINGUISHED = 1,   // Wyrozniajacy sie
		BOT_LEGEND_SPECIAL = 2,         // Specjalny
		BOT_LEGEND_WALKING = 3,         // Chodzaca Legenda
		BOT_LEGEND_CHAMPION = 4,        // Czempion Krolestwa
	};

	// Each counted once, in the row's achievements.
	enum EPlayerBotLegendAchievement
	{
		BOT_LEGEND_ACH_WEAPON_PLUS9 = 1,
		BOT_LEGEND_ACH_LEVEL75 = 2,
		BOT_LEGEND_ACH_LEVEL99 = 4,
		BOT_LEGEND_ACH_FIRST_BOSS = 8,
		BOT_LEGEND_ACH_HUNDRED_KILLS = 16,
		BOT_LEGEND_ACH_CROWNED = 32,
	};

	// MT2009_PLUS_LEGEND_NAMES_V1: the owner's 27 Legends (3 October), the
	// most legendary first. The world always has these 27 Chodzace Legendy
	// (or Champions) and no other: each name is worn by one bot, the Legend
	// of that place (playerbot_legends.h, AssignPlayerBotLegends), and by no
	// ordinary bot - the name pool leaves them out (playerbot_names.sql) and
	// a bot that wears one when its place goes to another gives it up.
	const int PLAYERBOT_LEGEND_NAME_COUNT = 27;
	const char* const PLAYERBOT_LEGEND_NAMES[PLAYERBOT_LEGEND_NAME_COUNT] = {
		"FuBu", "Wallander4ever", "SirMamutPOL", "Zwierz", "IsAmU", "GoToSleep", "Kasanga",
		"Loth", "SQRCZYBYKU", "KAPRAL", "BumBum", "NinjaxesPL", "Xuminnek", "ShiveR",
		"Scoobany", "Schenk", "Kosikredki", "Tream", "Sabal", "Gimper", "BuenaCosta",
		"Uzurpator", "DIIIM2", "TNT", "Asfen", "LifeIsDeath", "ZAXEP",
	};

	// The place (0 = the most legendary) of a Legend's name, or -1.
	int GetPlayerBotLegendNameSlot(const char* name)
	{
		if (!name || !*name)
			return -1;
		for (int i = 0; i < PLAYERBOT_LEGEND_NAME_COUNT; ++i)
			if (strcasecmp(name, PLAYERBOT_LEGEND_NAMES[i]) == 0)
				return i;
		return -1;
	}

	// A bot picked for a Legend's place and logged out to be renamed: out of
	// the world until the rename (or until the hold runs out).
	std::map<DWORD, DWORD> s_mapPlayerBotLegendNameHold;

	bool IsPlayerBotLegendNameHold(DWORD pid)
	{
		std::map<DWORD, DWORD>::iterator it = s_mapPlayerBotLegendNameHold.find(pid);
		if (it == s_mapPlayerBotLegendNameHold.end())
			return false;
		if (get_dword_time() >= it->second)
		{
			s_mapPlayerBotLegendNameHold.erase(it);
			return false;
		}
		return true;
	}

	struct TPlayerBotLegendRow
	{
		BYTE bTier;
		BYTE bEmpire;
		int iReputation;
		DWORD dwPlayerKills;
		DWORD dwPlayerDeaths;
		DWORD dwWarsWon;
		DWORD dwWarsLost;
		DWORD dwBossKills;
		DWORD dwAchievements;
		DWORD dwChampionCount;

		TPlayerBotLegendRow() : bTier(0), bEmpire(0), iReputation(0), dwPlayerKills(0), dwPlayerDeaths(0),
			dwWarsWon(0), dwWarsLost(0), dwBossKills(0), dwAchievements(0), dwChampionCount(0) {}
	};

	std::map<DWORD, TPlayerBotLegendRow> s_mapPlayerBotLegendRows;
	// The Legends and Champions of the world, for the rivals' checks.
	std::vector<DWORD> s_vecPlayerBotLegendPids;

	void RebuildPlayerBotLegendIndex()
	{
		s_vecPlayerBotLegendPids.clear();
		for (std::map<DWORD, TPlayerBotLegendRow>::const_iterator it = s_mapPlayerBotLegendRows.begin();
				it != s_mapPlayerBotLegendRows.end(); ++it)
			if (it->second.bTier >= BOT_LEGEND_WALKING)
				s_vecPlayerBotLegendPids.push_back(it->first);
	}

	// The draw a pid makes once: forty in a thousand Wyrozniajacy sie, twenty
	// Specjalni, by the pid's own hash - the same answer on every core and at
	// every start, which is what "random, and for good" needs.
	DWORD PlayerBotLegendHash(DWORD pid)
	{
		DWORD x = pid ^ 0x4c454745U;
		x ^= x >> 16;
		x *= 0x7feb352dU;
		x ^= x >> 15;
		x *= 0x846ca68bU;
		x ^= x >> 16;
		return x;
	}

	BYTE GetPlayerBotLegendDrawnTier(DWORD pid)
	{
		const int roll = (int)(PlayerBotLegendHash(pid) % 1000U);
		if (roll < PLAYERBOT_LEGEND_SPECIAL_PERMILLE)
			return BOT_LEGEND_SPECIAL;
		if (roll < PLAYERBOT_LEGEND_SPECIAL_PERMILLE + PLAYERBOT_LEGEND_DISTINGUISHED_PERMILLE)
			return BOT_LEGEND_DISTINGUISHED;
		return BOT_LEGEND_NONE;
	}

	// The tier the table holds, whatever the switch says.
	BYTE GetPlayerBotLegendTierRaw(DWORD pid)
	{
		std::map<DWORD, TPlayerBotLegendRow>::const_iterator it = s_mapPlayerBotLegendRows.find(pid);
		return it != s_mapPlayerBotLegendRows.end() ? it->second.bTier : (BYTE)BOT_LEGEND_NONE;
	}

	BYTE GetPlayerBotLegendTier(DWORD pid)
	{
		if (!IsPlayerBotLegendsEnabled())
			return BOT_LEGEND_NONE;
		return GetPlayerBotLegendTierRaw(pid);
	}

	bool IsPlayerBotLegendBot(LPCHARACTER ch)
	{
		return ch && ch->IsPC() && ch->GetDesc() && ch->GetDesc()->IsBot();
	}

	// A bot's tier; a person (or anything not a bot) has none.
	BYTE GetPlayerBotLegendTierOf(LPCHARACTER ch)
	{
		if (!IsPlayerBotLegendBot(ch))
			return BOT_LEGEND_NONE;
		return GetPlayerBotLegendTier(ch->GetPlayerID());
	}

	bool IsPlayerBotLegendTier(BYTE tier)
	{
		return tier >= BOT_LEGEND_WALKING;
	}

	// Two Legends (or a Legend and a Champion): rivals, never in one party
	// or one guild.
	bool ArePlayerBotLegendRivalPids(DWORD a, DWORD b)
	{
		return a != b && IsPlayerBotLegendTier(GetPlayerBotLegendTier(a)) &&
				IsPlayerBotLegendTier(GetPlayerBotLegendTier(b));
	}

	// Whether a party holds a rival of this bot's.
	bool PlayerBotPartyHasLegendRival(LPCHARACTER me, LPPARTY party)
	{
		if (!me || !party || !IsPlayerBotLegendTier(GetPlayerBotLegendTierOf(me)))
			return false;
		for (size_t i = 0; i < s_vecPlayerBotLegendPids.size(); ++i)
		{
			const DWORD pid = s_vecPlayerBotLegendPids[i];
			if (pid != me->GetPlayerID() && party->IsMember(pid) &&
					IsPlayerBotLegendTier(GetPlayerBotLegendTier(pid)))
				return true;
		}
		return false;
	}

	BYTE ClampPlayerBotLegendTier(BYTE tier)
	{
		return tier < PLAYERBOT_LEGEND_TIER_COUNT ? tier : (BYTE)(PLAYERBOT_LEGEND_TIER_COUNT - 1);
	}

	// A guild whose master is a Legend or a Champion.
	bool IsPlayerBotLegendGuild(CGuild* guild)
	{
		return guild && IsPlayerBotLegendTier(GetPlayerBotLegendTier(guild->GetMasterPID()));
	}

	// The kingdom's pick for a war: the two Legends' guilds, when both are
	// ready, were not its last pair, and the draw says so.
	bool PickPlayerBotLegendRivalWarPair(const std::vector<CGuild*>& ready, std::pair<DWORD, DWORD> last,
			CGuild*& out1, CGuild*& out2)
	{
		if (!IsPlayerBotLegendsEnabled() || number(1, 100) > PLAYERBOT_LEGEND_RIVAL_WAR_PERCENT)
			return false;
		CGuild* a = NULL;
		CGuild* b = NULL;
		for (size_t i = 0; i < ready.size(); ++i)
		{
			if (!IsPlayerBotLegendGuild(ready[i]))
				continue;
			if (!a)
				a = ready[i];
			else if (!b)
				b = ready[i];
		}
		if (!a || !b)
			return false;
		if ((a->GetID() == last.first && b->GetID() == last.second) ||
				(a->GetID() == last.second && b->GetID() == last.first))
			return false;
		out1 = a;
		out2 = b;
		return true;
	}

	// A bot of a tier in a fight with a person, short of health and of red
	// potions while the foe has more of its own left: it breaks off and runs
	// (the tactical retreat), and keeps out of a fight for
	// PLAYERBOT_LEGEND_PVP_RETREAT_LOCK_MS.
	bool ShouldPlayerBotLegendBreakOff(LPCHARACTER ch, LPCHARACTER foe)
	{
		const BYTE tier = ClampPlayerBotLegendTier(GetPlayerBotLegendTierOf(ch));
		const int at = PLAYERBOT_LEGEND_PVP_RETREAT_HP_PERCENT[tier];
		if (at <= 0 || !foe || ch->GetMaxHP() <= 0 || foe->GetMaxHP() <= 0)
			return false;
		const int mine = (int)((long long)ch->GetHP() * 100 / ch->GetMaxHP());
		const int theirs = (int)((long long)foe->GetHP() * 100 / foe->GetMaxHP());
		if (mine >= at || theirs <= mine)
			return false;
		const DWORD reds[] = { 27051, 27001, 27002, 27003, 71018, 71020, 27863, 27865, 27875 };
		for (size_t i = 0; i < sizeof(reds) / sizeof(reds[0]); ++i)
			if (ch->CountSpecifyItem(reds[i]) > 0)
				return false;
		return true;
	}

	// Whether the bot is in a fight with a person: its target, the foe the
	// Anti-PK protocol holds, or a guild war.
	bool IsPlayerBotFightingPerson(LPCHARACTER ch, const TPlayerBotAIState& state)
	{
		if (!ch)
			return false;
		if (state.persona.dwFoeVID != 0 || state.dwGuildWarEnemyGID != 0)
			return true;
		if (state.dwTargetVID == 0)
			return false;
		LPCHARACTER target = CHARACTER_MANAGER::instance().Find(state.dwTargetVID);
		return target && target != ch && target->IsPC() && !target->IsDead();
	}

	// playerbot_legends.h, after the wars: a field war's end, for the
	// reputation and the notices (winner NULL for a draw).
	void NotePlayerBotLegendGuildWarOver(CGuild* g1, CGuild* g2, CGuild* winner, bool playerWar);

	// ------------------------------------------------------------ the numbers

	int GetPlayerBotLegendRefineBonus(LPCHARACTER ch)
	{
		return PLAYERBOT_LEGEND_REFINE_POINTS[ClampPlayerBotLegendTier(GetPlayerBotLegendTierOf(ch))];
	}

	// How many reads one book or Spirit Stone counts as (one for a tierless
	// bot).
	int GetPlayerBotLegendBookReads(LPCHARACTER ch)
	{
		return PLAYERBOT_LEGEND_BOOK_READS[ClampPlayerBotLegendTier(GetPlayerBotLegendTierOf(ch))];
	}

	// The health a red potion is drunk from in a fight with a person: the
	// caller's, or the tier's when that is higher.
	int GetPlayerBotLegendPvpPotionPercent(LPCHARACTER ch, int callerPercent)
	{
		const int tierPercent = PLAYERBOT_LEGEND_PVP_POTION_HP_PERCENT[ClampPlayerBotLegendTier(GetPlayerBotLegendTierOf(ch))];
		return std::max(callerPercent, tierPercent);
	}

	// A foe's cost for a bot of a tier choosing whom to strike: lower for the
	// one already hurt and the one of fewer levels - the weaker first.
	long GetPlayerBotLegendFoeCostAdjust(LPCHARACTER ch, LPCHARACTER foe)
	{
		const BYTE tier = ClampPlayerBotLegendTier(GetPlayerBotLegendTierOf(ch));
		if (tier < BOT_LEGEND_SPECIAL || !foe || foe->GetMaxHP() <= 0)
			return 0;
		const int lostPercent = 100 - (int)((long long)foe->GetHP() * 100 / foe->GetMaxHP());
		const int levelOver = (int)foe->GetLevel() - (int)ch->GetLevel();
		return -(long)lostPercent * PLAYERBOT_LEGEND_WEAK_FOE_HP_WEIGHT[tier] +
				(long)levelOver * PLAYERBOT_LEGEND_WEAK_FOE_LEVEL_WEIGHT[tier];
	}

	// The gap between two skills against a person, cut for a Legend and a
	// Champion.
	DWORD GetPlayerBotLegendPvpSkillGap(LPCHARACTER ch, DWORD gap)
	{
		const BYTE tier = ClampPlayerBotLegendTier(GetPlayerBotLegendTierOf(ch));
		return gap * (DWORD)PLAYERBOT_LEGEND_PVP_SKILL_GAP_PERCENT[tier] / 100U;
	}

	// A piece's score for a Specjalny and up: its plus and its bonuses weigh
	// more, so it aims for the better-bonused, higher piece.
	long long AdjustPlayerBotLegendGearScore(LPITEM item, LPCHARACTER ch, long long score)
	{
		if (!item || !ch || score <= 0)
			return score;
		const BYTE tier = ClampPlayerBotLegendTier(GetPlayerBotLegendTierOf(ch));
		if (tier < BOT_LEGEND_SPECIAL)
			return score;
		const long long weight = (long long)item->GetRefineLevel() * PLAYERBOT_LEGEND_GEAR_PLUS_PERMILLE[tier] +
				(long long)item->GetAttributeCount() * PLAYERBOT_LEGEND_GEAR_BONUS_PERMILLE[tier];
		if (weight <= 0)
			return score;
		return score + score / 1000 * weight;
	}
}

#endif
