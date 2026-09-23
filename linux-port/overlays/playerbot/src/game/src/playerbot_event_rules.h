#ifndef PLAYERBOT_EVENT_RULES_H
#define PLAYERBOT_EVENT_RULES_H
// The world's timed events as pure policy: a Moonlight chest window, or a
// share of experience, drop or yang over the world's own rate, open between
// two clock times on chosen weekdays, or switched on from the panel for a
// number of minutes. No engine types; unit-tested in
// tests/playerbot_event_rules_test.cpp. playerbot_events.h is the engine half:
// it reads the panel's file, asks Evaluate once a second, gates the chest
// odds, moves the rate flags and speaks on the chat.
//
// The file the panel writes (/opt/m2spool/playerbot_events.tsv), one event a
// line, tab-separated:
//
//   chest	*	20:00	21:00	0		every day, 20:00-21:00
//   exp	6,7	18:00	20:00	50		Saturday and Sunday, +50% experience
//   now	drop	1758045600	30		+30% drop until that epoch second
//   #off	yang	*	12:00	13:00	25	a row the operator switched off
//
// Days are 1 (Monday) to 7 (Sunday) or "*"; a window whose end is not after
// its start runs past midnight (22:00-02:00). The core skips a comment line,
// which is how a row is switched off without being forgotten.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace playerbot_events {

enum Kind { KIND_CHEST = 0, KIND_EXP = 1, KIND_DROP = 2, KIND_YANG = 3, KIND_MAX = 4 };

inline const char* KindName(int kind)
{
	switch (kind)
	{
		case KIND_CHEST: return "chest";
		case KIND_EXP: return "exp";
		case KIND_DROP: return "drop";
		case KIND_YANG: return "yang";
	}
	return "";
}

inline int KindFromName(const char* name)
{
	if (!name)
		return -1;
	for (int kind = 0; kind < KIND_MAX; ++kind)
		if (strcmp(name, KindName(kind)) == 0)
			return kind;
	return -1;
}

struct Window {
	int kind = KIND_CHEST;
	int startMin = 0;         // minutes since midnight
	int endMin = 0;           // exclusive; not after startMin means "past midnight"
	int value = 0;            // percent over the base for a rate; the chests ignore it
	unsigned char days = 127; // bit 0 = Monday ... bit 6 = Sunday
	bool now = false;         // an "activate now" line: until is an epoch second
	long until = 0;
};

// "HH:MM" to minutes since midnight; 24:00 is admitted as an end.
inline bool ParseHHMM(const char* text, int& minutes)
{
	if (!text)
		return false;
	int h = 0, m = 0;
	if (sscanf(text, "%d:%d", &h, &m) != 2)
		return false;
	if (h < 0 || h > 24 || m < 0 || m > 59 || (h == 24 && m != 0))
		return false;
	minutes = h * 60 + m;
	return true;
}

// "*" is every day; "1,3,7" the listed ones. Nothing listed is no day: a row
// the panel saved with every box unticked never opens.
inline unsigned char ParseDays(const char* text)
{
	if (!text || !*text || strcmp(text, "*") == 0)
		return 127;
	unsigned char bits = 0;
	for (const char* p = text; *p; ++p)
		if (*p >= '1' && *p <= '7')
			bits |= (unsigned char)(1u << (*p - '1'));
	return bits;
}

inline int SplitTabs(char* line, char** fields, int max)
{
	int count = 0;
	char* p = line;
	while (count < max)
	{
		fields[count++] = p;
		char* tab = strchr(p, '\t');
		if (!tab)
			break;
		*tab = '\0';
		p = tab + 1;
	}
	return count;
}

// One line of the file. False for a comment, a blank line and anything the
// core cannot use - the caller counts those and says so once.
inline bool ParseLine(const char* text, Window& out)
{
	if (!text)
		return false;
	char buf[256];
	strncpy(buf, text, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	for (size_t len = strlen(buf); len > 0 &&
			(buf[len - 1] == '\r' || buf[len - 1] == '\n' || buf[len - 1] == ' '); --len)
		buf[len - 1] = '\0';
	if (!buf[0] || buf[0] == '#')
		return false;
	char* f[6] = { 0, 0, 0, 0, 0, 0 };
	const int n = SplitTabs(buf, f, 6);
	Window w;
	if (strcmp(f[0], "now") == 0)
	{
		if (n < 4)
			return false;
		w.kind = KindFromName(f[1]);
		if (w.kind < 0)
			return false;
		w.now = true;
		w.until = atol(f[2]);
		w.value = atoi(f[3]);
		if (w.until <= 0)
			return false;
		out = w;
		return true;
	}
	if (n < 5)
		return false;
	w.kind = KindFromName(f[0]);
	if (w.kind < 0)
		return false;
	w.days = ParseDays(f[1]);
	if (!ParseHHMM(f[2], w.startMin) || !ParseHHMM(f[3], w.endMin) || w.startMin >= 1440)
		return false;
	w.value = atoi(f[4]);
	out = w;
	return true;
}

// dayIndex: 0 = Monday ... 6 = Sunday; minute: since midnight. A window past
// midnight is open on its own evening and on the morning of the day after.
inline bool WindowActiveAt(const Window& w, int dayIndex, int minute)
{
	if (w.now)
		return false;
	const unsigned int today = 1u << dayIndex;
	if (w.endMin > w.startMin)
		return (w.days & today) && minute >= w.startMin && minute < w.endMin;
	if ((w.days & today) && minute >= w.startMin)
		return true;
	const unsigned int yesterday = 1u << ((dayIndex + 6) % 7);
	return (w.days & yesterday) && minute < w.endMin;
}

// For an active window: minutes until it closes.
inline int MinutesToEnd(const Window& w, int minute)
{
	if (w.endMin > w.startMin)
		return w.endMin - minute;
	if (minute >= w.startMin)
		return (1440 - minute) + w.endMin;
	return w.endMin - minute;
}

// Minutes until the window next opens, within the coming week; -1 when it
// never does (no day chosen).
inline int MinutesToStart(const Window& w, int dayIndex, int minute)
{
	if (w.now)
		return -1;
	for (int d = 0; d <= 7; ++d)
	{
		const int day = (dayIndex + d) % 7;
		if (!(w.days & (1u << day)))
			continue;
		const int delta = d * 1440 + w.startMin - minute;
		if (delta > 0)
			return delta;
	}
	return -1;
}

struct Status {
	bool scheduled = false; // any line of this kind at all: what closes the chest gate
	bool active = false;
	int value = 0;          // the largest of the active lines' values
	long until = 0;         // epoch second the last active line closes at
	long nextStart = 0;     // epoch second the next window opens at, 0 for none
	int nextValue = 0;
};

// nowEpoch is the wall clock; dayIndex and minute are its local reading.
inline Status Evaluate(const std::vector<Window>& windows, int kind, long nowEpoch,
		int dayIndex, int minute)
{
	Status st;
	const long minuteStart = nowEpoch - (nowEpoch % 60);
	for (size_t i = 0; i < windows.size(); ++i)
	{
		const Window& w = windows[i];
		if (w.kind != kind)
			continue;
		st.scheduled = true;
		if (w.now)
		{
			if (w.until > nowEpoch)
			{
				st.active = true;
				if (w.until > st.until)
					st.until = w.until;
				if (w.value > st.value)
					st.value = w.value;
			}
			continue;
		}
		if (WindowActiveAt(w, dayIndex, minute))
		{
			st.active = true;
			const long until = minuteStart + (long)MinutesToEnd(w, minute) * 60;
			if (until > st.until)
				st.until = until;
			if (w.value > st.value)
				st.value = w.value;
			continue;
		}
		const int toStart = MinutesToStart(w, dayIndex, minute);
		if (toStart > 0)
		{
			const long start = minuteStart + (long)toStart * 60;
			if (st.nextStart == 0 || start < st.nextStart)
			{
				st.nextStart = start;
				st.nextValue = w.value;
			}
		}
	}
	return st;
}

}

#endif
