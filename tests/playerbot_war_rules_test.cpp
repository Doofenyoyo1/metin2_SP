// How a bot guild fights a guild war (playerbot_war_rules.h): the class roles,
// the six patterns a guild draws, the order the bots leave their camp in, the
// round and the field.
//
// Upstream shipped the header without its test; this is ours, written against
// what the header says it does.
//
// Build: g++ -std=c++17 -Wall -Wextra -I linux-port/overlays/playerbot/src/game/src tests/playerbot_war_rules_test.cpp
#include "playerbot_war_rules.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
using namespace playerbot_war_rules;

namespace {

int g_checks = 0;

#define CHECK(c) do { ++g_checks; if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)

void TestKinds()
{
	CHECK(KindOf(0, 1) == KIND_WARRIOR_BODY);
	CHECK(KindOf(0, 2) == KIND_WARRIOR_MENTAL);
	CHECK(KindOf(1, 1) == KIND_NINJA_DAGGER);
	CHECK(KindOf(1, 2) == KIND_NINJA_ARCHER);
	CHECK(KindOf(2, 1) == KIND_SURA_WEAPON);
	CHECK(KindOf(2, 2) == KIND_SURA_MAGIC);
	CHECK(KindOf(3, 1) == KIND_SHAMAN_DRAGON);
	CHECK(KindOf(3, 2) == KIND_SHAMAN_HEAL);
	// No path yet: the first one of the class.
	for (int job = 0; job < 4; ++job)
		CHECK(KindOf(job, 0) == KindOf(job, 1));
}

void TestPatterns()
{
	// Deterministic, in range, and every pattern is drawn by some war.
	bool seen[PATTERN_COUNT] = {};
	for (unsigned int g = 1; g < 50; ++g)
		for (unsigned int t = 1000; t < 1100; ++t)
		{
			EPattern p = PickPattern(g, t);
			CHECK(p >= 0 && p < PATTERN_COUNT);
			CHECK(PickPattern(g, t) == p);
			seen[p] = true;
		}
	for (int p = 0; p < PATTERN_COUNT; ++p)
	{
		CHECK(seen[p]);
		CHECK(PatternName((EPattern)p)[0] != '?');
	}
	CHECK(PatternName(PATTERN_COUNT)[0] == '?');
}

void TestRoles()
{
	for (int p = 0; p < PATTERN_COUNT; ++p)
	{
		const EPattern pat = (EPattern)p;
		CHECK(RoleOf(KIND_WARRIOR_BODY, pat, 7) == ROLE_FIGHTER);
		CHECK(RoleOf(KIND_SURA_WEAPON, pat, 7) == ROLE_FIGHTER);
		CHECK(RoleOf(KIND_WARRIOR_MENTAL, pat, 7) == ROLE_TANK);
		CHECK(RoleOf(KIND_NINJA_DAGGER, pat, 7) == ROLE_ASSASSIN);
		CHECK(RoleOf(KIND_NINJA_ARCHER, pat, 7) == ROLE_ARCHER);
		CHECK(RoleOf(KIND_SURA_MAGIC, pat, 7) == ROLE_HUNTER);
		CHECK(RoleOf(KIND_SHAMAN_DRAGON, pat, 7) == ROLE_DRAGON);
	}
	// The healer's draw: every one keeps back under the guard and the wall,
	// every one strikes in a healer hunt and a blitz, about three in ten else.
	int strikers = 0;
	for (unsigned int pid = 1; pid <= 10000; ++pid)
	{
		CHECK(RoleOf(KIND_SHAMAN_HEAL, PATTERN_GUARD, pid) == ROLE_HEALER_GUARD);
		CHECK(RoleOf(KIND_SHAMAN_HEAL, PATTERN_WALL, pid) == ROLE_HEALER_GUARD);
		CHECK(RoleOf(KIND_SHAMAN_HEAL, PATTERN_HEALER_HUNT, pid) == ROLE_HEALER_STRIKER);
		CHECK(RoleOf(KIND_SHAMAN_HEAL, PATTERN_BLITZ, pid) == ROLE_HEALER_STRIKER);
		if (RoleOf(KIND_SHAMAN_HEAL, PATTERN_CLASSIC, pid) == ROLE_HEALER_STRIKER)
			++strikers;
	}
	CHECK(strikers > 2500 && strikers < 3500);
	for (int r = 0; r < ROLE_COUNT; ++r)
	{
		CHECK(RoleName((ERole)r, false)[0] != '?');
		CHECK(RoleName((ERole)r, true)[0] != '?');
	}
	CHECK(BuffsSide(ROLE_HEALER_GUARD) && BuffsSide(ROLE_HEALER_STRIKER) && BuffsSide(ROLE_DRAGON));
	CHECK(!BuffsSide(ROLE_TANK) && !BuffsSide(ROLE_ARCHER) && !BuffsSide(ROLE_FIGHTER));
}

void TestFocus()
{
	// The black-magic sura hunts the ninjas, then the healers, then its own kind.
	CHECK(FocusBonus(ROLE_HUNTER, PATTERN_CLASSIC, KIND_NINJA_DAGGER) == FOCUS_BONUS[0]);
	CHECK(FocusBonus(ROLE_HUNTER, PATTERN_CLASSIC, KIND_NINJA_ARCHER) == FOCUS_BONUS[0]);
	CHECK(FocusBonus(ROLE_HUNTER, PATTERN_CLASSIC, KIND_SHAMAN_HEAL) == FOCUS_BONUS[1]);
	CHECK(FocusBonus(ROLE_HUNTER, PATTERN_CLASSIC, KIND_SURA_MAGIC) == FOCUS_BONUS[2]);
	CHECK(FocusBonus(ROLE_HUNTER, PATTERN_CLASSIC, KIND_WARRIOR_BODY) == 0);
	// The archer: the black magic, the healers, the ninjas - and under the
	// guard the daggers first, which are what comes for its healers.
	CHECK(FocusBonus(ROLE_ARCHER, PATTERN_CLASSIC, KIND_SURA_MAGIC) == FOCUS_BONUS[0]);
	CHECK(FocusBonus(ROLE_ARCHER, PATTERN_GUARD, KIND_NINJA_DAGGER) == FOCUS_BONUS[0]);
	CHECK(FocusBonus(ROLE_ARCHER, PATTERN_GUARD, KIND_NINJA_ARCHER) == 0);
	// The assassin: the healers, the shamans, the suras.
	CHECK(FocusBonus(ROLE_ASSASSIN, PATTERN_CLASSIC, KIND_SHAMAN_HEAL) == FOCUS_BONUS[0]);
	CHECK(FocusBonus(ROLE_ASSASSIN, PATTERN_CLASSIC, KIND_SHAMAN_DRAGON) == FOCUS_BONUS[1]);
	CHECK(FocusBonus(ROLE_ASSASSIN, PATTERN_CLASSIC, KIND_SURA_WEAPON) == FOCUS_BONUS[2]);
	// The offensive healer: the ninjas and the black magic.
	CHECK(FocusBonus(ROLE_HEALER_STRIKER, PATTERN_CLASSIC, KIND_NINJA_ARCHER) == FOCUS_BONUS[0]);
	CHECK(FocusBonus(ROLE_HEALER_STRIKER, PATTERN_CLASSIC, KIND_SURA_MAGIC) == FOCUS_BONUS[1]);
	// The fighters, the tank, the dragon and the defensive healer take the nearest.
	for (int k = 0; k < KIND_COUNT; ++k)
	{
		CHECK(FocusBonus(ROLE_FIGHTER, PATTERN_CLASSIC, (EKind)k) == 0);
		CHECK(FocusBonus(ROLE_TANK, PATTERN_CLASSIC, (EKind)k) == 0);
		CHECK(FocusBonus(ROLE_DRAGON, PATTERN_CLASSIC, (EKind)k) == 0);
		CHECK(FocusBonus(ROLE_HEALER_GUARD, PATTERN_CLASSIC, (EKind)k) == 0);
		// A blitz goes at the nearest, every role.
		for (int r = 0; r < ROLE_COUNT; ++r)
			CHECK(FocusBonus((ERole)r, PATTERN_BLITZ, (EKind)k) == 0);
	}
	// A healer hunt puts the healers first for everybody but the healers,
	// and keeps the rest of each list after them without a duplicate.
	CHECK(FocusBonus(ROLE_FIGHTER, PATTERN_HEALER_HUNT, KIND_SHAMAN_HEAL) == FOCUS_BONUS[0]);
	CHECK(FocusBonus(ROLE_HUNTER, PATTERN_HEALER_HUNT, KIND_SHAMAN_HEAL) == FOCUS_BONUS[0]);
	CHECK(FocusBonus(ROLE_HUNTER, PATTERN_HEALER_HUNT, KIND_NINJA_DAGGER) == FOCUS_BONUS[1]);
	CHECK(FocusBonus(ROLE_HUNTER, PATTERN_HEALER_HUNT, KIND_SURA_MAGIC) == FOCUS_BONUS[2]);
	CHECK(FocusBonus(ROLE_HEALER_GUARD, PATTERN_HEALER_HUNT, KIND_SHAMAN_HEAL) == 0);
	CHECK(FocusBonus(ROLE_HEALER_STRIKER, PATTERN_HEALER_HUNT, KIND_NINJA_DAGGER) == FOCUS_BONUS[0]);
	EFocus list[3];
	FocusList(ROLE_ASSASSIN, PATTERN_HEALER_HUNT, list);
	CHECK(list[0] == FOCUS_HEALER && list[1] == FOCUS_SHAMAN && list[2] == FOCUS_SURA);
}

void TestRunOut()
{
	for (unsigned int pid = 1; pid < 3000; ++pid)
	{
		// The tank is out first; everybody else between half a second and two.
		CHECK(RunOutDelayMs(ROLE_TANK, PATTERN_CLASSIC, pid) < 300);
		const unsigned int d = RunOutDelayMs(ROLE_FIGHTER, PATTERN_CLASSIC, pid);
		CHECK(d >= 500 && d < 2000);
		CHECK(RunOutDelayMs(ROLE_FIGHTER, PATTERN_BLITZ, pid) < 300);
		// The ambush: the daggers ahead of everybody.
		CHECK(RunOutDelayMs(ROLE_ASSASSIN, PATTERN_AMBUSH, pid) < RunOutDelayMs(ROLE_FIGHTER, PATTERN_AMBUSH, pid + 1));
		// The wall: the tanks ahead of everybody.
		CHECK(RunOutDelayMs(ROLE_TANK, PATTERN_WALL, pid) < RunOutDelayMs(ROLE_ARCHER, PATTERN_WALL, pid + 1));
		CHECK(RunOutDelayMs(ROLE_FIGHTER, PATTERN_CLASSIC, pid) == d);
	}
}

void TestRounds()
{
	CHECK(RoundWinner(0, 5, 3, 5) == 1);
	CHECK(RoundWinner(2, 5, 0, 5) == 0);
	CHECK(RoundWinner(2, 5, 3, 5) == -1);
	CHECK(RoundWinner(0, 5, 0, 5) == -1);
	// A side that never came wins nothing and loses nothing.
	CHECK(RoundWinner(0, 0, 3, 5) == -1);
	CHECK(RoundWinner(3, 5, 0, 0) == -1);
}

void TestGeometry()
{
	CHECK(DistanceToSegment(0, 0, 0, 0, 100, 0) == 0);
	CHECK(DistanceToSegment(50, 30, 0, 0, 100, 0) == 30);
	CHECK(DistanceToSegment(-40, 30, 0, 0, 100, 0) == 50);	// past a end: the end
	CHECK(DistanceToSegment(140, 30, 0, 0, 100, 0) == 50);	// past b end
	CHECK(DistanceToSegment(3, 4, 0, 0, 0, 0) == 5);	// a point segment
	long x = 0, y = 0;
	StepAway(100, 0, 0, 0, 0, 0, 50, x, y);
	CHECK(x == 150 && y == 0);
	// Standing on what it steps away from: towards `to`.
	StepAway(100, 100, 100, 100, 100, 300, 50, x, y);
	CHECK(x == 100 && y == 150);
	// Nowhere to go: stays.
	StepAway(10, 10, 10, 10, 10, 10, 50, x, y);
	CHECK(x == 10 && y == 10);
}

}

int main()
{
	TestKinds();
	TestPatterns();
	TestRoles();
	TestFocus();
	TestRunOut();
	TestRounds();
	TestGeometry();
	std::printf("playerbot_war_rules_test: %d checks passed\n", g_checks);
	return 0;
}
