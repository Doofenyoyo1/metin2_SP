"""MT2009_PLUS_EVENTS_AUTOGEN_V1: a weekly event schedule drawn from a pool.

The events page's generator: the admin ticks the kinds, says how many events a
week, how long each runs and between which hours, and this lays them out over
the chosen days - an even share a day (or more at the weekend), the kinds taken
in turn so each comes up about as often, and never two events of one kind at
once, neither among the new ones nor against the rows the schedule already has
when the new ones are appended. The result is plain schedule rows, the same the
calendar edits and write_events() saves (kind, days, start, end, value, map,
on); identical rows of different days are merged into one row with several
days, so a week of events costs as few of the 64 rows as it can.

Pure Python, no Flask: the panel's route validates the form and calls
generate_week(); test_event_autogen.py runs it on its own.
"""
import random

DAY_MINUTES = 1440
WEEK_MINUTES = 7 * DAY_MINUTES
STEP = 30  # starts on the half hour
MAX_ROWS = 64  # MT2009_PLUS_EVENTS_64_V1
MAX_EVENTS = 64
# A schedule row is one window of at most a day (the core reads HH:MM-HH:MM,
# past midnight when the end is not after the start); longer events are the
# "activate now" lines', up to seven days (MT2009_PLUS_EVENTS_7_DAYS_V1).
LENGTHS = (30, 60, 90, 120, 180, 240, 360, 480, 720, 1440)
WEEKEND = (5, 6, 7)  # Friday, Saturday, Sunday


def hhmm(minutes, end=False):
    """Minutes of the day to "HH:MM"; an end at midnight is "24:00" when the
    window started that same day, so it stays one plain window."""
    minutes %= DAY_MINUTES
    if end and minutes == 0:
        return "24:00"
    return "%02d:%02d" % (minutes // 60, minutes % 60)


def parse_hhmm(text):
    try:
        hour, minute = (int(part) for part in str(text).split(":"))
    except (TypeError, ValueError):
        return None
    if not (0 <= hour <= 24 and 0 <= minute <= 59) or (hour == 24 and minute):
        return None
    return hour * 60 + minute


def row_length(start, end):
    """A row's length the way the core reads it: an end not after the start
    runs past midnight, the same start and end is a whole day."""
    return end - start if end > start else DAY_MINUTES - start + end


def _overlaps(a_start, a_len, b_start, b_len):
    # Two spans of the week, either of which may run past Sunday midnight.
    for shift in (-WEEK_MINUTES, 0, WEEK_MINUTES):
        if a_start < b_start + shift + b_len and b_start + shift < a_start + a_len:
            return True
    return False


def existing_spans(rows):
    """(kind, week_start, length) of every enabled row's every day."""
    spans = []
    for row in rows or ():
        if not row.get("on", True):
            continue
        start, end = parse_hhmm(row.get("start")), parse_hhmm(row.get("end"))
        if start is None or end is None or start >= DAY_MINUTES:
            continue
        length = row_length(start, end)
        for day in row.get("days") or ():
            if 1 <= int(day) <= 7:
                spans.append((row.get("kind", ""), (int(day) - 1) * DAY_MINUTES + start, length))
    return spans


def day_quotas(count, days, weekend_bias=False):
    """How many events each day gets: an even spread, a running total rounded
    (10 over a week is 1,2,1,2,1,2,1), the weekend days counted twice when
    asked."""
    weights = [2 if weekend_bias and day in WEEKEND else 1 for day in days]
    total = float(sum(weights))
    quotas, running, given = [], 0, 0
    for weight in weights:
        running += weight
        target = int(count * running / total + 0.5)
        quotas.append(target - given)
        given = target
    return quotas


def merge_rows(events):
    """One row per kind, hours and value, with the days of every event that
    shares them, in week order."""
    merged, order = {}, []
    for item in events:
        key = (item["kind"], item["start"], item["end"], item["value"], item["map"])
        if key not in merged:
            merged[key] = {"kind": item["kind"], "days": [], "start": item["start"], "end": item["end"],
                           "value": item["value"], "map": item["map"], "on": True}
            order.append(key)
        if item["day"] not in merged[key]["days"]:
            merged[key]["days"].append(item["day"])
    rows = [merged[key] for key in order]
    for row in rows:
        row["days"].sort()
    rows.sort(key=lambda row: (row["days"][0], row["start"], row["kind"]))
    return rows


def generate_week(kinds, count, length, hour_from, hour_to, days=(1, 2, 3, 4, 5, 6, 7),
                  values=None, weekend_bias=False, exclusive=False, existing=(),
                  max_rows=MAX_ROWS, seed=None):
    """Lay `count` events of `kinds` over the week.

    hour_from/hour_to: the hours the events must fit in (0-24); a "to" not
    after the "from" runs past midnight (18-2), the same hour is the whole
    day. values: kind -> the row's value (the rate bonus, the world event's
    count, 0 for the switches). exclusive: no two events of any kinds at
    once, not only of one kind. existing: the rows already on the schedule
    (append) - the new events keep clear of their kinds' hours and the rows
    they leave free are the limit.

    Returns {"rows": merged schedule rows, "events": one item per event,
    "warnings": [Polish texts], "placed": n}; raises ValueError with a Polish
    text when the request cannot work at all.
    """
    kinds = [kind for kind in dict.fromkeys(kinds or ())]
    days = sorted({int(day) for day in days or () if 1 <= int(day) <= 7})
    values = values or {}
    if not kinds:
        raise ValueError("Zaznacz co najmniej jeden rodzaj eventu.")
    if not days:
        raise ValueError("Zaznacz co najmniej jeden dzień tygodnia.")
    if not 1 <= count <= MAX_EVENTS:
        raise ValueError("Liczba eventów w tygodniu musi być od 1 do %d." % MAX_EVENTS)
    if length not in LENGTHS:
        raise ValueError("Nieprawidłowa długość eventu.")
    if not (0 <= hour_from <= 23 and 0 <= hour_to <= 24):
        raise ValueError("Nieprawidłowy zakres godzin.")
    window_start = hour_from * 60
    span = (hour_to - hour_from) * 60 if hour_to > hour_from else (hour_to + 24 - hour_from) * 60
    if length > span:
        raise ValueError("Event (%s) nie mieści się w wybranych godzinach (%s)."
                         % (_duration(length), _duration(span)))
    free_rows = max_rows - len(existing or ())
    if free_rows <= 0:
        raise ValueError("Harmonogram ma już %d wierszy - nie ma miejsca na nowe eventy." % max_rows)

    rng = random.Random(seed)
    spans = existing_spans(existing)
    by_kind = {}
    for span_item in spans:
        by_kind.setdefault(span_item[0], []).append(span_item)
    warnings = []
    pool = list(kinds)
    rng.shuffle(pool)
    used = {kind: 0 for kind in kinds}

    # The offsets from the window's start an event may begin at.
    offsets = list(range(0, span - length + 1, STEP))

    def conflicts(kind, week_start):
        for _other, other_start, other_length in (spans if exclusive else by_kind.get(kind, ())):
            if _overlaps(week_start, length, other_start, other_length):
                return True
        return False

    def week_start_of(day, offset):
        return ((day - 1) * DAY_MINUTES + window_start + offset) % WEEK_MINUTES

    def candidates(ideal):
        # Nearest the ideal first; between two as near, the full hour.
        return sorted(offsets, key=lambda offset: (abs(offset - ideal), (window_start + offset) % 60 != 0, offset))

    room = span - length

    def ideals(quota):
        """Where a day's events would best start: spaced evenly with room
        around them when they fit one after another (and a little jitter, so
        another draw is another week, never enough to make them touch), cut
        evenly over the window when they cannot help overlapping. On the
        full hour where that keeps them apart, else on the half hour."""
        if quota * length <= span:
            free = span - quota * length
            result, previous_end = [], 0
            for index in range(quota):
                exact = index * length + free * (index + 0.5) / quota
                exact += rng.uniform(-0.25, 0.25) * free / quota
                # The latest start that still leaves the rest their hours.
                limit = span - (quota - index) * length
                hour = int(round((window_start + exact) / 60.0)) * 60 - window_start
                if previous_end <= hour <= limit:
                    offset = hour
                else:
                    offset = min(limit, max(previous_end, int(exact) // STEP * STEP))
                result.append(offset)
                previous_end = offset + length
            return result
        result = []
        for index in range(quota):
            exact = room * (index + 0.5) / quota + rng.uniform(-0.25, 0.25) * room / quota
            result.append(max(0, min(room, int(round(exact / 60.0)) * 60)))
        return result

    def place(kind_order, day_order, ideal):
        for try_day in day_order:
            for kind in kind_order:
                for offset in candidates(ideal):
                    start = week_start_of(try_day, offset)
                    if not conflicts(kind, start):
                        return kind, start
        return None

    def kinds_for(wanted):
        # The kind in turn first, then the least used others. With no two
        # events at once of any kinds a slot is free or not for every kind
        # alike: the one in turn is enough to try.
        if exclusive:
            return [wanted]
        return [wanted] + sorted((kind for kind in pool if kind != wanted), key=lambda kind: used[kind])

    events = []
    leftovers = []
    sequence = 0

    def record(kind, start):
        spans.append((kind, start, length))
        by_kind.setdefault(kind, []).append((kind, start, length))
        used[kind] += 1
        start_day, start_minute = start // DAY_MINUTES + 1, start % DAY_MINUTES
        events.append({"kind": kind, "day": start_day, "start": hhmm(start_minute),
                       "end": hhmm(start_minute + length, end=start_minute + length == DAY_MINUTES),
                       "value": int(values.get(kind, 0)), "map": 0, "minutes": length})

    # First every day its own share, so an event that does not fit its day
    # cannot take the hours a later day's own event wanted...
    for day, quota in zip(days, day_quotas(count, days, weekend_bias)):
        for ideal in ideals(quota) if quota else ():
            wanted = pool[sequence % len(pool)]
            sequence += 1
            placed = place(kinds_for(wanted), [day], ideal)
            if placed:
                record(*placed)
            else:
                leftovers.append((day, wanted, ideal))
    # ...then what did not fit, on the nearest other chosen day with room.
    unplaced = 0
    for day, wanted, ideal in leftovers:
        day_order = sorted((other for other in days if other != day),
                           key=lambda other: (min((other - day) % 7, (day - other) % 7), (other - day) % 7))
        placed = place(kinds_for(wanted), day_order, ideal)
        if placed:
            record(*placed)
        else:
            unplaced += 1
    if unplaced:
        warnings.append("Nie zmieściło się %d z %d eventów - za mało wolnych godzin (%s). Wydłuż zakres "
                        "godzin, dodaj dni lub rodzaje eventów albo skróć eventy."
                        % (unplaced, count, "eventy nie mogą się nakładać" if exclusive
                           else "eventy tego samego rodzaju nie mogą się nakładać"))
    events.sort(key=lambda item: (item["day"], item["start"], item["kind"]))
    rows = merge_rows(events)
    if len(rows) > free_rows:
        dropped = rows[free_rows:]
        rows = rows[:free_rows]
        lost = sum(len(row["days"]) for row in dropped)
        kept = {(row["kind"], row["start"], row["end"], day) for row in rows for day in row["days"]}
        events = [item for item in events if (item["kind"], item["start"], item["end"], item["day"]) in kept]
        warnings.append("Limit %d wierszy harmonogramu: pominięto %d eventów (%d wierszy)."
                        % (max_rows, lost, len(dropped)))
    missing = [kind for kind in kinds if not any(item["kind"] == kind for item in events)]
    if missing and len(events) >= len(kinds):
        warnings.append("Nie udało się zaplanować: %s." % ", ".join(missing))
    return {"rows": rows, "events": events, "warnings": warnings, "placed": len(events)}


def _duration(minutes):
    if minutes % 60:
        return "%d min" % minutes if minutes < 60 else "%d h %d min" % (minutes // 60, minutes % 60)
    return "%d h" % (minutes // 60)
