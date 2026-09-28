#ifndef PLAYERBOT_BONUS_RULES_H
#define PLAYERBOT_BONUS_RULES_H
// Which stone a bot puts on which piece next - the add stone, the Blessing
// Marble's fifth line, the change stone, a green one or an ordinary one - as
// pure policy. No engine types; unit-tested in
// tests/playerbot_bonus_rules_test.cpp. playerbot_bonus.h is the engine's half:
// it reads the pieces, the bag and the gates, asks here, and spends the stone
// the answer names.
//
// Two points of Iwakura's Community Patch 5 are this file.
//
// Point 5: a bot whose necklace, boots and bracelet were done kept seventeen
// adds and nine changes in its bag for good. The categories of Patch 4, point
// 6 (IsPlayerBotBonusCategoryAllowed) take the armour, the helmet, the shield
// and the earrings only at +7 and a weapon only at forty-five and +7, while
// the refine aim keeps the helmet and the jewellery at +4 until the weapon,
// the armour and the shield stand at +7 - and those three stall at +6 for want
// of scrolls. So the categories still come first, and what they cannot use
// now goes on to the rest of the worn gear (PLAIN_REST, RestOpen): "po
// wybonowaniu jednych przedmiotow bot powinien automatycznie przechodzic do
// kolejnych, az do calkowitego wybonowania ekwipunku".
//
// Point 9: Zielona Sila and Zielony Czar go on a weapon or a body armour of
// level forty or less and on nothing else (char_item.cpp), and the same
// categories refused every such weapon but the level-30 family and every such
// armour under +7, the weapon under forty-five waited for two health lines on
// the jewellery that only an ordinary stone can give, and the change stone
// waited for a fourth line three green adds cannot give. They are spent first
// now, on the armour and then on the weapon ("w pierwszej kolejnosci ... w
// zbroi, a nastepnie w broni"), and a Zielony Czar mixes whatever lines are
// there once no add stone can give the piece another ("mieszac bonusy dodane
// nie tylko za pomoca Zielonej Sily, ale takze inne bonusy dodatkowe").
namespace playerbot_bonus_rules {

enum EStep { STEP_NONE, STEP_ADD, STEP_MARBLE, STEP_CHANGE };
enum EStone { STONE_NONE, STONE_GREEN, STONE_PLAIN, STONE_MARBLE };

// How the ordinary stones may reach a piece in this pass.
enum EPlain
{
	// The categories of Patch 4, point 6: the ordinary stones as ever.
	PLAIN_CATEGORY,
	// The rest of the worn gear (Patch 5, point 5): only a kind of ordinary
	// stone no category piece can take now.
	PLAIN_REST,
	// No ordinary stone: a young bot's piece before its necklace, bracelet and
	// boots are done, the weapon waiting for their health lines, a weapon the
	// ordinary stones are wasted on. A green stone may still fit it.
	PLAIN_NONE,
};

// The kinds of ordinary stone the rest may take (RestOpen).
enum { REST_ADD = 1, REST_CHANGE = 2 };

struct TStepChoice
{
	EStep step;
	EStone stone;
};

// What the bag holds, by kind.
struct TBag
{
	bool greenAdd;
	bool greenChange;
	bool plainAdd;
	bool plainChange;
	bool marble;
};

// The engine's numbers: the add stone stops at four lines (USE_ADD_ATTRIBUTE,
// the marble's USE_ADD_ATTRIBUTE2 adds the fifth), and the ordinary change
// waits for a full piece (Iwakura's QUICK FIX nr 3 and Patch 4, point 6).
// Passed in rather than repeated here: they are playerbot_types.h's
// PLAYERBOT_BONUS_MAX_LINES and PLAYERBOT_BONUS_CHANGE_MIN_LINES.
struct TLimits
{
	int stoneLines;
	int plainChangeMinLines;
};

// One piece, as the choice needs it.
struct TPiece
{
	int lines;
	// A new line can still land on it: the engine draws among the lines its
	// attribute set allows and it does not carry yet, and with none left an
	// add stone or the marble is spent for nothing.
	bool lineRolls;
	EPlain plain;
	// The engine takes a green stone on it: a weapon or a body armour of
	// level forty or less, from +4 like every stone.
	bool greenFits;
	// A worn category piece: the marble's fifth line may go on it.
	bool marbleAllowed;
	// An ordinary change, and a green one, can still move it towards its
	// finish (WantsChange).
	bool wantsPlainChange;
	bool wantsGreenChange;
	// A companion's owner put it on: lines are added to it, never mixed.
	bool pinned;
	// A bag piece kept for sale: outside the rest gate, and last.
	bool goods;
	// Its place in the green round: 0 the armour, 1 the weapon, -1 none.
	int greenRank;
};

// What one stone would do to a piece now, and which stone.
// greenStonesOnly: the caller spends green stones or nothing (the green
// round, the gambler's pieces).
inline TStepChoice StepFor(const TPiece& p, const TBag& bag, const TLimits& limits,
		unsigned restOpen, bool greenStonesOnly)
{
	const TStepChoice none = { STEP_NONE, STONE_NONE };
	// Whether the ordinary pass would put an ordinary stone on it now, asked
	// even of the green round: its add is what a green change waits for.
	const bool plainAdd = p.plain != PLAIN_NONE && bag.plainAdd &&
			(p.plain == PLAIN_CATEGORY || (restOpen & REST_ADD) != 0);
	const bool plainChange = !greenStonesOnly && p.plain != PLAIN_NONE && bag.plainChange &&
			(p.plain == PLAIN_CATEGORY || (restOpen & REST_CHANGE) != 0);

	// An empty line is free power: add before anything else, a green stone
	// first where the piece takes one, since it is good for nothing else. The
	// green round leaves an ordinary add to the ordinary pass, and mixes
	// nothing meanwhile: the piece is filled first.
	if (p.lines < limits.stoneLines && p.lineRolls)
	{
		if (p.greenFits && bag.greenAdd)
			return TStepChoice{ STEP_ADD, STONE_GREEN };
		if (plainAdd)
			return greenStonesOnly ? none : TStepChoice{ STEP_ADD, STONE_PLAIN };
	}
	else if (p.lines == limits.stoneLines && p.lineRolls && !greenStonesOnly &&
			p.plain == PLAIN_CATEGORY && p.marbleAllowed && bag.marble)
		return TStepChoice{ STEP_MARBLE, STONE_MARBLE };

	// Nothing to mix on a piece with no line, and what a companion's owner
	// put on keeps the lines the owner chose it for. A finished piece is
	// never mixed - an add above cannot lose what is there, a change can.
	if (p.lines <= 0 || p.pinned)
		return none;
	// Past here no add stone can give the piece another line now, so a
	// Zielony Czar mixes whatever lines it has. The ordinary change waits for
	// the full piece.
	if (p.greenFits && bag.greenChange && p.wantsGreenChange)
		return TStepChoice{ STEP_CHANGE, STONE_GREEN };
	if (plainChange && p.wantsPlainChange && p.lines >= limits.plainChangeMinLines)
		return TStepChoice{ STEP_CHANGE, STONE_PLAIN };
	return none;
}

// Whether a category piece could take an ordinary add, or an ordinary
// change, now. Goods are not asked: the gear a bot fights in comes before a
// weapon it will sell.
inline bool CategoryTakesPlainAdd(const TPiece& p, const TBag& bag, const TLimits& limits)
{
	return !p.goods && p.plain == PLAIN_CATEGORY && bag.plainAdd && p.lines < limits.stoneLines &&
			p.lineRolls;
}

inline bool CategoryTakesPlainChange(const TPiece& p, const TBag& bag, const TLimits& limits)
{
	return !p.goods && p.plain == PLAIN_CATEGORY && bag.plainChange && p.lines > 0 &&
			p.lines >= limits.plainChangeMinLines && !p.pinned && p.wantsPlainChange;
}

// The kinds of ordinary stone the rest of the worn gear may take: each kind
// once no category piece can use it now. By kind, because the two do not
// compete: a category weapon being mixed for its average line must not hold
// seventeen adds back from an armour with four empty lines.
inline unsigned RestOpen(const TPiece* pieces, int count, const TBag& bag, const TLimits& limits)
{
	unsigned open = REST_ADD | REST_CHANGE;
	for (int i = 0; i < count; ++i)
	{
		if (CategoryTakesPlainAdd(pieces[i], bag, limits))
			open &= ~(unsigned)REST_ADD;
		if (CategoryTakesPlainChange(pieces[i], bag, limits))
			open &= ~(unsigned)REST_CHANGE;
	}
	return open;
}

// The piece the next stones go on, and the stone. -1 when none can take one.
//
// The green stones first, the armour before the weapon (Patch 5, point 9):
// one piece of each rank, the first the list names. The green round leaves
// the ordinary pass's memory alone (greenPick), so an ordinary piece half way
// through its lines is come back to.
// Then the ordinary pass (Iwakura's QUICK FIX nr 3): the piece being worked
// keeps the stones while one fits it, and a new piece is the first in the
// list that can take a line, then the first that can take a change, so
// pieces are filled before anything is mixed.
inline int Pick(const TPiece* pieces, int count, const TBag& bag, const TLimits& limits,
		unsigned restOpen, int focus, TStepChoice& choice, bool& greenPick)
{
	choice.step = STEP_NONE;
	choice.stone = STONE_NONE;
	greenPick = false;
	for (int rank = 0; rank < 2; ++rank)
		for (int i = 0; i < count; ++i)
		{
			if (pieces[i].greenRank != rank)
				continue;
			const TStepChoice c = StepFor(pieces[i], bag, limits, restOpen, true);
			if (c.step != STEP_NONE)
			{
				choice = c;
				greenPick = true;
				return i;
			}
			break;
		}
	if (focus >= 0 && focus < count)
	{
		const TStepChoice c = StepFor(pieces[focus], bag, limits, restOpen, false);
		if (c.step != STEP_NONE)
		{
			choice = c;
			return focus;
		}
	}
	for (int round = 0; round < 2; ++round)
		for (int i = 0; i < count; ++i)
		{
			const TStepChoice c = StepFor(pieces[i], bag, limits, restOpen, false);
			const bool adding = c.step == STEP_ADD || c.step == STEP_MARBLE;
			if ((round == 0 && adding) || (round == 1 && c.step == STEP_CHANGE))
			{
				choice = c;
				return i;
			}
		}
	return -1;
}

// Whether a change stone can carry a piece to its finish at all. A weapon's
// finish is its average or skill line (HasPlayerBotFinishedBonus), and those
// come from item_addon.cpp, not from item_attr: a change rolls them again only
// on a weapon whose proto carries the damage addon (the level-30 and level-75
// families here). Every other finish is a line item_attr rolls on its slot.
inline bool ChangeReachesFinish(bool isWeapon, bool damageAddon)
{
	return !isWeapon || damageAddon;
}

// Whether a change stone would still move a piece towards its finish: never
// once it is finished, and short of that either to the finish
// (mixToFinish) or until its line score - each line weighed by Iwakura's
// PvE tier - reaches keepScore.
//
// The ordinary change mixes to the finish only what the operator always
// mixed so: the level-30 weapons and the young bot's jewellery. For the rest
// the score is the stop, and it has to be: one piece takes every stone while
// it can use one (Pick's focus), and a finish can be rare - a necklace past
// forty-five wants health of 1500 and critical of 5 at once, about one change
// in ninety by item_attr - so mixed to its finish it would hold every change
// stone the bot finds, and the pass would never go on to the next piece
// ("bot powinien automatycznie przechodzic do kolejnych", Patch 5, point 5).
// A green change mixes to the finish wherever a change can roll it
// (ChangeReachesFinish): a bot has three, and past its band they are good
// for nothing ("dazac do uzyskania jak najlepszych statystyk z tabeli
// tierow", point 9).
inline bool WantsChange(bool finished, bool mixToFinish, int lineScore, int keepScore)
{
	if (finished)
		return false;
	return mixToFinish || lineScore < keepScore;
}

// A bot past the green band: over the level the green stones stop at, and
// wearing neither a weapon nor an armour they fit. Its green stones are kept
// back from a counter no longer (Patch 5, point 9: "przetrzymuja je w
// ekwipunku az do poznych faz gry").
inline bool PastGreenBand(int botLevel, int greenMaxLevel, bool weaponTakesGreen, bool armourTakesGreen)
{
	return botLevel > greenMaxLevel && !weaponTakesGreen && !armourTakesGreen;
}

} // namespace playerbot_bonus_rules

#endif
