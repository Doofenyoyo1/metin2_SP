// g++ -Wall -Wextra -o /tmp/t tests/playerbot_event_rules_test.cpp && /tmp/t
#include "../linux-port/overlays/playerbot/src/game/src/playerbot_event_rules.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

using namespace playerbot_events;

static int g_failed = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++g_failed; } } while (0)

int main()
{
	Window w;
	// A plain window, every day.
	CHECK(ParseLine("chest\t*\t20:00\t21:00\t0", w));
	CHECK(w.kind == KIND_CHEST && w.days == 127 && w.startMin == 1200 && w.endMin == 1260 && !w.now);
	CHECK(WindowActiveAt(w, 0, 1200));
	CHECK(WindowActiveAt(w, 6, 1259));
	CHECK(!WindowActiveAt(w, 3, 1260));
	CHECK(!WindowActiveAt(w, 3, 1199));
	CHECK(MinutesToEnd(w, 1230) == 30);
	CHECK(MinutesToStart(w, 0, 600) == 600);
	CHECK(MinutesToStart(w, 0, 1300) == 1440 - 1300 + 1200);

	// Weekend only, +50% experience.
	CHECK(ParseLine("exp\t6,7\t18:00\t20:00\t50\r\n", w));
	CHECK(w.kind == KIND_EXP && w.value == 50 && w.days == (32 | 64));
	CHECK(!WindowActiveAt(w, 0, 1100));   // Monday
	CHECK(WindowActiveAt(w, 5, 1100));    // Saturday
	CHECK(WindowActiveAt(w, 6, 1199));    // Sunday
	CHECK(MinutesToStart(w, 0, 0) == 5 * 1440 + 1080);

	// Past midnight: Friday 22:00 to Saturday 02:00.
	CHECK(ParseLine("drop\t5\t22:00\t02:00\t30", w));
	CHECK(WindowActiveAt(w, 4, 1320));    // Friday 22:00
	CHECK(WindowActiveAt(w, 5, 60));      // Saturday 01:00
	CHECK(!WindowActiveAt(w, 5, 120));    // Saturday 02:00
	CHECK(!WindowActiveAt(w, 4, 60));     // Friday 01:00
	CHECK(MinutesToEnd(w, 1320) == 240);
	CHECK(MinutesToEnd(w, 60) == 60);

	// A row switched off, a comment, junk.
	CHECK(!ParseLine("#off\tyang\t*\t12:00\t13:00\t25", w));
	CHECK(!ParseLine("# comment", w));
	CHECK(!ParseLine("", w));
	CHECK(!ParseLine("tea\t*\t12:00\t13:00\t25", w));
	CHECK(!ParseLine("exp\t*\t25:00\t13:00\t25", w));
	CHECK(!ParseLine("exp\t*\t12:00", w));
	// No day ticked never opens.
	CHECK(ParseLine("exp\t\t12:00\t13:00\t25", w) && w.days == 127);
	CHECK(ParseLine("exp\t-\t12:00\t13:00\t25", w) && w.days == 0);
	CHECK(!WindowActiveAt(w, 2, 750));
	CHECK(MinutesToStart(w, 2, 0) == -1);

	// An "activate now" line.
	CHECK(ParseLine("now\texp\t1758045600\t50", w));
	CHECK(w.now && w.kind == KIND_EXP && w.until == 1758045600 && w.value == 50);
	CHECK(!WindowActiveAt(w, 0, 0));
	CHECK(!ParseLine("now\texp\t0\t50", w));

	// Evaluate: a window and a now-line together, the larger value and the
	// later end win; a window still to come gives the next start.
	std::vector<Window> all;
	Window a, b, c;
	CHECK(ParseLine("exp\t*\t18:00\t20:00\t50", a));
	CHECK(ParseLine("now\texp\t1000000\t80", b));
	CHECK(ParseLine("chest\t*\t21:00\t22:00\t0", c));
	all.push_back(a);
	all.push_back(b);
	all.push_back(c);
	// 19:30 on some Wednesday; nowEpoch chosen so the minute starts at 999000.
	const long nowEpoch = 999030;
	Status st = Evaluate(all, KIND_EXP, nowEpoch, 2, 19 * 60 + 30);
	CHECK(st.scheduled && st.active && st.value == 80);
	// The window closes at 20:00, thirty minutes on from 999000: later than the now-line.
	CHECK(st.until == 999000 + 30 * 60);
	Status chest = Evaluate(all, KIND_CHEST, nowEpoch, 2, 19 * 60 + 30);
	CHECK(chest.scheduled && !chest.active);
	CHECK(chest.nextStart == 999000 + 90 * 60);
	Status yang = Evaluate(all, KIND_YANG, nowEpoch, 2, 19 * 60 + 30);
	CHECK(!yang.scheduled && !yang.active && yang.nextStart == 0);
	// The now-line alone once the window has closed: 20:30.
	st = Evaluate(all, KIND_EXP, nowEpoch, 2, 20 * 60 + 30);
	CHECK(st.active && st.value == 80 && st.until == 1000000);
	// And once the now-line has expired too.
	st = Evaluate(all, KIND_EXP, 1000001, 2, 20 * 60 + 30);
	// 1000001 sits 41 seconds into its minute (999960); Thursday 18:00 is 1290 minutes on.
	CHECK(st.scheduled && !st.active && st.nextStart == 999960 + 1290 * 60);

	// The two world events (upstream 2.2.22, whose test it did not ship):
	// Tanaka and Zuo carry a map, a rate kind does not; a now-line may carry
	// its first second; the counts are held to their ceilings.
	CHECK(ParseLine("tanaka\t*\t19:00\t20:00\t3\t64", w));
	CHECK(w.kind == KIND_TANAKA && w.value == 3 && w.map == 64);
	CHECK(IsWorldKind(KIND_TANAKA) && IsWorldKind(KIND_ZUO) && !IsWorldKind(KIND_EXP));
	CHECK(IsRateKind(KIND_YANG) && !IsRateKind(KIND_CHEST) && !IsRateKind(KIND_ZUO));
	CHECK(ParseLine("exp\t*\t19:00\t20:00\t50\t64", w) && w.map == 0);
	CHECK(ParseLine("zuo\t5\t21:00\t22:00\t8\t0", w) && w.kind == KIND_ZUO && w.map == 0);
	CHECK(ParseLine("now\tzuo\t1758045600\t8\t63\t1758042000", w));
	CHECK(w.now && w.map == 63 && w.since == 1758042000);
	CHECK(ParseLine("now\tzuo\t1758045600\t8\t63\t1758049999", w) && w.since == 0);
	CHECK(WorldEventCount(KIND_TANAKA, 0) == 3 && WorldEventCount(KIND_TANAKA, 50) == 20);
	CHECK(WorldEventCount(KIND_ZUO, 0) == 8 && WorldEventCount(KIND_ZUO, 50) == 30);
	CHECK(WorldEventCount(KIND_EXP, 50) == 50);
	CHECK(std::strcmp(KindName(KIND_TANAKA), "tanaka") == 0 && KindFromName("zuo") == KIND_ZUO);
	// A settings line is no event, and an event line no setting.
	Settings set;
	CHECK(!ParseLine("bots\t50", w));
	CHECK(ParseSettingLine("bots\t75\r\n", set) && set.botsPercent == 75);
	CHECK(ParseSettingLine("bots\t250", set) && set.botsPercent == 100);
	CHECK(!ParseSettingLine("exp\t*\t19:00\t20:00\t50", set));
	CHECK(!BotTakesPart(7, 0) && BotTakesPart(7, 100));
	int taking = 0;
	for (unsigned int pid = 1; pid <= 1000; ++pid)
		taking += BotTakesPart(pid, 50) ? 1 : 0;
	CHECK(taking > 400 && taking < 600);
	for (unsigned int pid = 1; pid <= 1000; ++pid)
		CHECK(!BotTakesPart(pid, 30) || BotTakesPart(pid, 60));  // a raised slider keeps who came
	// Zuo's bosses come in the second half; an unknown first second is all rain.
	CHECK(!IsZuoBossHalf(1000, 4600, 2000) && IsZuoBossHalf(1000, 4600, 2800));
	CHECK(!IsZuoBossHalf(0, 4600, 4000));
	// A window's map and first second come with the line whose value is taken.
	std::vector<Window> ev;
	Window t1, t2;
	CHECK(ParseLine("tanaka\t*\t19:00\t21:00\t3\t64", t1));
	CHECK(ParseLine("tanaka\t*\t19:30\t20:30\t5\t63", t2));
	ev.push_back(t1);
	ev.push_back(t2);
	Status tk = Evaluate(ev, KIND_TANAKA, 999030, 2, 20 * 60);
	CHECK(tk.active && tk.value == 5 && tk.map == 63);
	CHECK(tk.since == 999000 - 30 * 60);
	tk = Evaluate(ev, KIND_TANAKA, 999030, 2, 18 * 60 + 50);
	CHECK(!tk.active && tk.nextMap == 64 && tk.nextStart == 999000 + 10 * 60);

	if (g_failed)
	{
		std::printf("%d check(s) failed\n", g_failed);
		return 1;
	}
	std::printf("playerbot_event_rules: all checks passed\n");
	return 0;
}
