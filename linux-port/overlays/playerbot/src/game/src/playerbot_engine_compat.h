#ifndef __INC_PLAYERBOT_ENGINE_COMPAT_H__
#define __INC_PLAYERBOT_ENGINE_COMPAT_H__

// The names the playerbot sources were written against are the r40250
// engine's. The mt2009 (martysama0134 r41023) engine renamed two families:
//
//   * an item apply IS a point there. TItemApply::bType and every
//     TPlayerItemAttribute::bType carry EPointTypes values (item.cpp hands
//     them straight to PointChange, world.item_attr.apply is an enum of
//     POINT_* names, and the applytype columns of world.item_proto hold the
//     POINT numbers - a Water Robe's are 19/77, MOV_SPEED and RESIST_MAGIC).
//     The EApplyTypes enum does not exist. Comparing an attribute type with
//     APPLY_MAX_HP therefore has to mean POINT_MAX_HP, and that is what these
//     aliases say. The stats keep their old spelling on the apply side
//     (CON/INT/STR/DEX) and the point side's (HT/IQ/ST/DX) here.
//
//   * the skill affect bits carry English names; the Korean ones are kept as
//     comments beside them in common/length.h, which is the table below.
//
// Built with -DPLAYERBOT_ENGINE_MT2009 (game/src/Makefile, put there by
// linux-port-mt2009/port/playerbotify.py); on r40250 this header is empty and
// the sources compile against the engine's own enums as before.

#if defined(PLAYERBOT_ENGINE_MT2009)

#define APPLY_NONE                    POINT_NONE
#define APPLY_MAX_HP                  POINT_MAX_HP
#define APPLY_MAX_SP                  POINT_MAX_SP
#define APPLY_CON                     POINT_HT
#define APPLY_INT                     POINT_IQ
#define APPLY_STR                     POINT_ST
#define APPLY_DEX                     POINT_DX
#define APPLY_ATT_SPEED               POINT_ATT_SPEED
#define APPLY_MOV_SPEED               POINT_MOV_SPEED
#define APPLY_CAST_SPEED              POINT_CASTING_SPEED
#define APPLY_HP_REGEN                POINT_HP_REGEN
#define APPLY_SP_REGEN                POINT_SP_REGEN
#define APPLY_POISON_PCT              POINT_POISON_PCT
#define APPLY_STUN_PCT                POINT_STUN_PCT
#define APPLY_SLOW_PCT                POINT_SLOW_PCT
#define APPLY_CRITICAL_PCT            POINT_CRITICAL_PCT
#define APPLY_PENETRATE_PCT           POINT_PENETRATE_PCT
#define APPLY_ATTBONUS_HUMAN          POINT_ATTBONUS_HUMAN
#define APPLY_ATTBONUS_ANIMAL         POINT_ATTBONUS_ANIMAL
#define APPLY_ATTBONUS_ORC            POINT_ATTBONUS_ORC
#define APPLY_ATTBONUS_MILGYO         POINT_ATTBONUS_MILGYO
#define APPLY_ATTBONUS_UNDEAD         POINT_ATTBONUS_UNDEAD
#define APPLY_ATTBONUS_DEVIL          POINT_ATTBONUS_DEVIL
#define APPLY_ATTBONUS_MONSTER        POINT_ATTBONUS_MONSTER
#define APPLY_STEAL_HP                POINT_STEAL_HP
#define APPLY_STEAL_SP                POINT_STEAL_SP
#define APPLY_POISON_REDUCE           POINT_POISON_REDUCE
#define APPLY_BLOCK                   POINT_BLOCK
#define APPLY_DODGE                   POINT_DODGE
#define APPLY_RESIST_SWORD            POINT_RESIST_SWORD
#define APPLY_RESIST_TWOHAND          POINT_RESIST_TWOHAND
#define APPLY_RESIST_DAGGER           POINT_RESIST_DAGGER
#define APPLY_RESIST_BELL             POINT_RESIST_BELL
#define APPLY_RESIST_FAN              POINT_RESIST_FAN
#define APPLY_RESIST_BOW              POINT_RESIST_BOW
#define APPLY_RESIST_FIRE             POINT_RESIST_FIRE
#define APPLY_RESIST_ELEC             POINT_RESIST_ELEC
#define APPLY_RESIST_MAGIC            POINT_RESIST_MAGIC
#define APPLY_RESIST_WIND             POINT_RESIST_WIND
#define APPLY_RESIST_ICE              POINT_RESIST_ICE
#define APPLY_RESIST_EARTH            POINT_RESIST_EARTH
#define APPLY_RESIST_DARK             POINT_RESIST_DARK
#define APPLY_REFLECT_MELEE           POINT_REFLECT_MELEE
#define APPLY_EXP_DOUBLE_BONUS        POINT_EXP_DOUBLE_BONUS
#define APPLY_GOLD_DOUBLE_BONUS       POINT_GOLD_DOUBLE_BONUS
#define APPLY_ITEM_DROP_BONUS         POINT_ITEM_DROP_BONUS
#define APPLY_POTION_BONUS            POINT_POTION_BONUS
#define APPLY_KILL_HP_RECOVER         POINT_KILL_HP_RECOVERY
#define APPLY_IMMUNE_STUN             POINT_IMMUNE_STUN
#define APPLY_IMMUNE_SLOW             POINT_IMMUNE_SLOW
#define APPLY_IMMUNE_FALL             POINT_IMMUNE_FALL
#define APPLY_SKILL                   POINT_SKILL
#define APPLY_ATT_GRADE_BONUS         POINT_ATT_GRADE_BONUS
#define APPLY_DEF_GRADE_BONUS         POINT_DEF_GRADE_BONUS
#define APPLY_DEF_GRADE               POINT_DEF_GRADE
#define APPLY_MAGIC_ATT_GRADE         POINT_MAGIC_ATT_GRADE
#define APPLY_MAX_STAMINA             POINT_MAX_STAMINA
#define APPLY_MAX_HP_PCT              POINT_MAX_HP_PCT
#define APPLY_SKILL_DAMAGE_BONUS      POINT_SKILL_DAMAGE_BONUS
#define APPLY_NORMAL_HIT_DAMAGE_BONUS POINT_NORMAL_HIT_DAMAGE_BONUS
#define APPLY_NORMAL_HIT_DEFEND_BONUS POINT_NORMAL_HIT_DEFEND_BONUS
// Five points Iwakura's bonus prices name (playerbot_price_tables.h). The
// first two exist on r40250 under the same APPLY names; the last three
// exist only here, and their rows sit in an #if there.
#define APPLY_MANA_BURN_PCT           POINT_MANA_BURN_PCT
#define APPLY_MALL_EXPBONUS           POINT_MALL_EXPBONUS
#define APPLY_REFLECT_ARROW           POINT_REFLECT_ARROW
#define APPLY_ST_REGEN                POINT_ST_REGEN
#define APPLY_SKILL_DURATION          POINT_SKILL_DURATION

#define AFF_JEONGWIHON  AFF_SKILL_BERSERK
#define AFF_GEOMGYEONG  AFF_SKILL_SWORD_AURA
#define AFF_CHEONGEUN   AFF_SKILL_STRONG_BODY
#define AFF_GYEONGGONG  AFF_SKILL_FEATHER_WALK
#define AFF_EUNHYUNG    AFF_SKILL_STEALTH
#define AFF_GWIGUM      AFF_SKILL_ENCHANTED_BLADE
#define AFF_TERROR      AFF_SKILL_TERROR
#define AFF_JUMAGAP     AFF_SKILL_ENCHANTED_ARMOR
#define AFF_HOSIN       AFF_SKILL_BLESSING
#define AFF_BOHO        AFF_SKILL_REFLECT
#define AFF_KWAESOK     AFF_SKILL_SWIFTNESS
#define AFF_MANASHIELD  AFF_SKILL_MANASHIELD
#define AFF_MUYEONG     AFF_SKILL_FLAME_SPIRIT
#define AFF_GICHEON     AFF_SKILL_DRAGON_HELP
#define AFF_JEUNGRYEOK  AFF_SKILL_ATTACK_UP

// unique_item.h for UNIQUE_ITEM_FISHING_PASS, which fishing() insists on.
#include "unique_item.h"
#include "../../common/PulseManager.h"

// fishing::Take() accepts the take 1700-3500 ms after the bite; the fishing
// pass runs every quarter second, so this band is always hit once.
const DWORD PLAYERBOT_MT2009_FISHING_REACT_MIN = 1900;
const DWORD PLAYERBOT_MT2009_FISHING_REACT_MAX = 3300;
// fishing.cpp keeps its game-state enum to itself: NONE 0, PRE_START 1,
// IN_PROGRESS 2, PAST_EVENT 3.
const int PLAYERBOT_MT2009_FISHING_GAME_IN_PROGRESS = 2;

// A private shop is a right this engine grants at level 15 and 800 kills
// (CHARACTER::CanOpenShop reads PLAYER_STATS_MONSTER_FLAG); a keeper asked
// before that is refused with a chat line and comes back after this long.
const DWORD PLAYERBOT_MT2009_SHOP_NOT_YET_RETRY = 600000;

// One page of the storeroom, as r40250's length.h named it.
#ifndef SAFEBOX_PAGE_SIZE
#define SAFEBOX_PAGE_SIZE (SAFEBOX_PAGE_WIDTH * SAFEBOX_PAGE_HEIGHT)
#endif

#endif // PLAYERBOT_ENGINE_MT2009

// Yang in and out of the purse. mt2009 refuses PointChange(POINT_GOLD, ..)
// outright ("unknown point change type 11"); ChangeGold() is its way, and
// it takes the 64-bit YANG the engine keeps gold in. r40250 has neither.
inline void PlayerBotChangeGold(LPCHARACTER ch, long long delta)
{
#if defined(PLAYERBOT_ENGINE_MT2009)
	ch->ChangeGold((YANG) delta);
#else
	ch->PointChange(POINT_GOLD, (int) delta);
#endif
}

// One page of the bag: an item taller than one cell must stay on its page,
// which is how GetEmptyInventory judges room. r40250 never named the number.
#if defined(PLAYERBOT_ENGINE_MT2009)
const int PLAYERBOT_INVENTORY_PAGE_SIZE = INVENTORY_PAGE_SIZE;
#else
const int PLAYERBOT_INVENTORY_PAGE_SIZE = 45;
#endif

// The cells a bot's bag actually has. mt2009's INVENTORY_MAX_NUM is 135:
// the two pages plus the horse inventory page (ENABLE_EXTEND_INVEN_SYSTEM),
// which GetEmptyInventory places into only for a character who unlocked it
// (GetInventoryMaxCount) - a bot never does. Every count the bots made over
// INVENTORY_MAX_NUM saw forty-five phantom free cells, so a chest judged to
// fit spilled on the ground and every bag-pressure rule was off by half a
// bag. The fragments count over this instead; the engine's arrays stay the
// engine's size.
#if defined(PLAYERBOT_ENGINE_MT2009)
const int PLAYERBOT_BAG_CELLS = INVENTORY_DEFAULT_MAX_NUM;
#else
const int PLAYERBOT_BAG_CELLS = INVENTORY_MAX_NUM;
#endif

// The private shop's grid as the engine indexes it. r40250 lays a counter
// out five cells wide (SHOP_HOST_ITEM_MAX_NUM, forty); mt2009 doubles the
// width to SHOP_PLAYER_WIDTH (ten) and keeps the right half for slots a
// player unlocks or pays premium for (SHOP_PLAYER_LOCKED_*,
// SHOP_PLAYER_PREMIUM_*). The bots place their lines on the free left page,
// five wide, and hand the engine display_pos = row * this + column - with
// the r40250 width every line past the fifth landed in the locked half,
// CanPlaceOnShopSlot said SHOP_LOCKED_SLOTS to nobody, and OpenMyShop
// refused the whole counter: "2500 botow, 0 sklepow".
#if defined(PLAYERBOT_ENGINE_MT2009)
const int PLAYERBOT_SHOP_ENGINE_COLUMNS = SHOP_PLAYER_WIDTH;
#else
const int PLAYERBOT_SHOP_ENGINE_COLUMNS = 5;
#endif

// Whether this character may open a private shop at all. mt2009 grants the
// counter at level 15 and 800 kills (CHARACTER::CanOpenShop) - to a player;
// since 2.0.12 CanOpenShop answers yes to a bot's descriptor, because a
// world of two thousand bots opened no counter for hours after every
// restart while each one killed its eight hundred. r40250 grants it to
// anybody. The junk rule asks, because a bag that cannot be sold from a
// counter has only the merchant left. Shops are the first channel's alone
// (playerbot_channel_rules.h), so a bot on the second one keeps nothing for
// a counter it can never open - its goods take the no-counter path instead
// of filling the bag for good.
inline bool PlayerBotCanOpenShop(LPCHARACTER ch)
{
	if (g_bChannel != 1)
		return false;
#if defined(PLAYERBOT_ENGINE_MT2009)
	return ch && ch->CanOpenShop();
#else
	return ch != NULL;
#endif
}

// Putting a piece on. mt2009 answers CanEquipNow() six times per half
// second per pid and then says no (PulseManager, ePulse::ItemEquip - an
// anti-flood for a client, and a bot has none). The gear pass asks once per
// bag piece, so on a full bag EquipItem's own check was the seventh answer:
// 63 000 "failed to equip upgrade" in an hour, none of them for a reason
// the item or the character had. The clock is cleared before every ask.
inline void PlayerBotResetEquipPulse(LPCHARACTER ch)
{
#if defined(PLAYERBOT_ENGINE_MT2009)
	PulseManager::Instance().ClearClock(ch->GetPlayerID(), ePulse::ItemEquip);
#else
	(void) ch;
#endif
}

inline bool PlayerBotCanEquipNow(LPCHARACTER ch, LPITEM item, const TItemPos& srcCell = TItemPos())
{
	// EquipItem refuses every slot but the quiver to a transformed character
	// ("You cannot change the equipped item while you are transformed"), and
	// CanEquipNow does not ask - so a bot on a polymorph marble for a boss took
	// its weapon off (UnequipItem does not ask either), was refused the new one
	// and fought the boss bare-handed, "failed to equip upgrade" every ten
	// seconds until the marble ran out (26 September).
	if (ch->IsPolymorphed() && !(item && item->GetType() == ITEM_WEAPON && item->GetSubType() == WEAPON_ARROW))
		return false;
	PlayerBotResetEquipPulse(ch);
	return ch->CanEquipNow(item, srcCell);
}

inline bool PlayerBotEquipItem(LPCHARACTER ch, LPITEM item, int iCandidateCell = -1)
{
	PlayerBotResetEquipPulse(ch);
	return ch->EquipItem(item, iCandidateCell);
}

#endif // __INC_PLAYERBOT_ENGINE_COMPAT_H__
