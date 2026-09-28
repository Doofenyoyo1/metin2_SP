// playerbot_refine_rules.h: what a bot of thirty may fight with, the scroll
// rule for its weapon and armour, and the backup weapon (Iwakura's Community
// Patch 5, points 2 and 4, and his audit's R8). Ours: upstream shipped the
// header without its test.
//
//   g++ -Wall -Wextra -o /tmp/t tests/playerbot_refine_rules_test.cpp && /tmp/t
#include <cassert>
#include <cstdio>

#include "../linux-port/overlays/playerbot/src/game/src/playerbot_refine_rules.h"

using namespace playerbot_refine_rules;

int main()
{
	// Point 2: from thirty, no weapon of level ten or under.
	assert(!IsLowWeaponFor(29, 0) && IsLowWeaponFor(30, 0) && IsLowWeaponFor(70, 10));
	assert(!IsLowWeaponFor(70, 11));
	assert(IsProperWeaponFor(20, 0));           // a young bot keeps its sword
	assert(!IsProperWeaponFor(35, 1));
	assert(IsProperWeaponFor(35, 30) && !IsProperWeaponFor(35, 36));  // and never over its level

	// The ban gives way only where nothing can answer it.
	assert(!IsLowWeaponFallback(true, 0, 100, true));
	assert(IsLowWeaponFallback(false, 1000000, 0, false));      // no merchant sells one
	assert(IsLowWeaponFallback(false, 1000000, 500, true));     // a purchase refused lately
	assert(IsLowWeaponFallback(false, 499, 500, false));
	assert(!IsLowWeaponFallback(false, 500, 500, false));

	// Point 4: three scrolls, a weapon of thirty or more under +7.
	assert(IsScrollRuleWeapon(30, 5, true, true, 3));
	assert(!IsScrollRuleWeapon(30, 5, true, true, 2));
	assert(!IsScrollRuleWeapon(29, 5, true, true, 37));
	assert(!IsScrollRuleWeapon(30, 7, true, true, 37));
	assert(!IsScrollRuleWeapon(30, 5, false, true, 37));
	assert(!IsScrollRuleWeapon(30, 5, true, false, 37));
	// The armour once the weapon stands at +8.
	assert(!IsScrollRuleArmour(34, 5, true, true, 3, 7));
	assert(IsScrollRuleArmour(34, 5, true, true, 3, 8));
	assert(!IsScrollRuleArmour(26, 5, true, true, 3, 9));
	assert(!IsScrollRuleArmour(34, 5, true, true, 3, -1));
	assert(ScrollRuleTarget(6) == 7 && ScrollRuleTarget(7) == 7 && ScrollRuleTarget(9) == 9);

	// A step in a fight: taken the moment the window opens, held for at most
	// five seconds and only above half health, dropped when a rule forbids.
	assert(DecideScrollStep(false, true, true, 0, 100) == SCROLL_STEP_NOW);
	assert(DecideScrollStep(false, false, false, 0, 10) == SCROLL_STEP_NOW);
	assert(DecideScrollStep(true, false, false, 0, 100) == SCROLL_STEP_WAIT);
	assert(DecideScrollStep(true, true, true, 1000, 50) == SCROLL_STEP_HOLD);
	assert(DecideScrollStep(true, true, true, 1000, 49) == SCROLL_STEP_DROP);
	assert(DecideScrollStep(true, true, false, 0, 100) == SCROLL_STEP_DROP);
	assert(DecideScrollStep(false, false, true, SCROLL_STEP_WAIT_MAX_MS + 1, 100) == SCROLL_STEP_DROP);
	assert(DecideScrollStep(true, false, true, SCROLL_STEP_WAIT_MAX_MS, 100) == SCROLL_STEP_WAIT);

	// R8: a backup by score, by family, or by level - the hand's, or the best
	// a merchant sells, whichever is lower.
	assert(IsBackupWeaponFor(1000, 500, 50, false, 45, 0, 0));
	assert(!IsBackupWeaponFor(1000, 499, 50, false, 45, 0, 0));
	assert(IsBackupWeaponFor(1000, 10, 50, true, 45, 0, 0));
	assert(IsBackupWeaponFor(1000, 10, 50, false, 45, 36, 36));
	assert(!IsBackupWeaponFor(1000, 10, 50, false, 45, 25, 36));
	assert(IsBackupWeaponFor(1000, 10, 50, false, 45, 45, 0));   // no merchant: the hand's level
	assert(!IsBackupWeaponFor(1000, 10, 50, false, 45, 44, 0));
	assert(IsBackupWeaponFor(1000, 10, 50, false, 20, 20, 36));  // a merchant over the hand: the hand's

	std::printf("playerbot_refine_rules: all checks passed\n");
	return 0;
}
