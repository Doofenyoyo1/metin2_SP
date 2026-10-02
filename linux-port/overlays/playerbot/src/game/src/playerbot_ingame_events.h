// MT2009 PLUS in-game event manager - MT2009_PLUS_EVENT_MANAGER_V1
// (the operator, 30 September: the event manager first, then Catch the King,
// Rumi, Yut Nori and the Flower Event plug into it - full versions, with real
// packets in a new exe; and a new event - Easter, Valentine's, any later one -
// must never need the exe again).
//
// Owsap's CInGameEventManager (ingame_event_manager.cpp, v6.2.6) kept a map
// of event-flag names to event types, told every client "<flag> <value>" when
// a flag changed and at login, ended the flags whose end time had passed and
// spawned the mini games' table NPCs. Here the clock is ours: the panels'
// scheduler (playerbot_events.h) says when an event runs, and this file turns
// that into
//
//  - the players' event list. Every scheduler kind is an event under its
//    name (chest, exp, drop, yang, tanaka, zuo, bossloot, metinloot, goblin,
//    catchking, rumi, yutnori, flower, easter - and whatever kind comes
//    next), plus the flag events of s_defs that have no kind (rumi_xmas).
//    It goes to the client as HEADER_GC_INGAME_EVENT (183, packet.h,
//    server-patches/eventmanager): a key of at most 24 characters [a-z0-9_],
//    on/off, start, end, reward-window end and a figure. The exe
//    (client-patches/exe) only keeps that list for the python module
//    ingameEventSystem; what an event is called, its icon and which window
//    its button opens live in the client's python (uiingameevent.py) - a new
//    event is a kind here and a line there. An exe without the packet would
//    stop at an unknown header, so a client gets it only after it said it can
//    take it: "/ingame_event hello <caps>" from the client's python
//    (ingameevent.py), caps a bit mask -
//        1  the packet (the exe has the ingameEventSystem module)
//        2  the same list as chat lines, for a client whose exe is older:
//             IGE begin <count>
//             IGE ev <key> <enable> <start> <end> <reward end> <value>
//             IGE end
//        4  Owsap's "<flag> <value>" commands (mini_game_okey,
//           mini_game_okey_normal, mini_game_yutnori, mini_game_catchking,
//           e_flower_drop, easter_drop), which Owsap's game python listens
//           for: every non-zero one when the client says hello, a changed
//           one (0 too) when it changes.
//    The list goes again whenever anything in it changes; a warp or a
//    channel change is a new game window, which says hello again.
//  - Owsap's own event flags, so the mini game code ported next reads the
//    state it was written for, unchanged. The leading core (Joan's, the one
//    that speaks the notices) writes them through the DB core: at the start
//    the flag (mini_game_okey_normal, mini_game_yutnori, mini_game_catchking:
//    the end epoch; e_flower_drop: the drop value; easter_drop and
//    easter_rabbit: 1), the per-kill share (*_drop, 100 when unset) and a
//    closed reward window (*_reward = 0); while it runs the flag follows the
//    end; at the end the flag goes to 0 and Rumi and Yut Nori open their
//    7-day reward window (*_reward = now + 7 days). A flag a GM sets by hand
//    (Owsap's way, e.g. mini_game_okey, the Christmas Rumi, which has no panel
//    kind, or the classic panel's Easter page) runs the event too, and the
//    leader ends an end-epoch flag when its time is up, as Owsap's
//    UpdateInGameEvent did.
//  - the table NPCs on the three first villages (maps 1, 21, 41, Owsap's
//    cells, checked walkable on our server_attr): Rumi 20417, Yut Nori 20502,
//    Catch the King 20506 - while the event runs, and for Rumi and Yut Nori
//    also through the reward window. Every core spawns on the maps it hosts;
//    an NPC without a mob_proto row is skipped (said once in the log) until
//    the mini game brings it.
//
// Every core judges the state itself once a second (the scheduler's status
// and the flags), like the goblin; only the leader writes the flags.
//
// For the mini games that plug in next: InGameEventIsActive(key),
// InGameEventEndTime(key), InGameEventRewardEndTime(key) and
// InGameEventValue(key) at the end of this file.
#include "mob_manager.h"

namespace mt2009_ingame_event
{
	enum EFlagMeaning
	{
		FLAG_NONE,		// no flag of its own (a kind the scheduler alone runs)
		FLAG_END_EPOCH,	// the flag is the event's end epoch, 0 = off
		FLAG_VALUE,		// the flag is the event's value (a drop, a switch), 0 = off
	};

	struct TEventDef
	{
		const char*	key;		// the list's key, [a-z0-9_], at most INGAME_EVENT_KEY_MAX_LEN
		int			kind;		// the playerbot_events kind that drives it, -1 none
		const char*	flag;		// its event flag (Owsap's name for the mini games)
		EFlagMeaning	meaning;
		int			onValue;	// FLAG_VALUE: what the leader writes, 0 = the drop share (100)
		const char*	companion;	// a second switch written 1/0 with it (easter_rabbit)
		const char*	dropFlag;	// Owsap's per-kill share flag
		const char*	rewardFlag;	// the reward window's end epoch
		DWORD		npc;		// the table NPC, 0 none
		bool		owsapCommand;	// "<flag> <value>" to a client with CAP_OWSAP_FLAGS
	};

	// The events with flags or NPCs. Every other scheduler kind is an event
	// too, under its name, without either (BuildEvents).
	const TEventDef s_defs[] =
	{
		{ "rumi",		playerbot_events::KIND_RUMI,		"mini_game_okey_normal",	FLAG_END_EPOCH,	0, NULL,			"mini_game_okey_drop",		"mini_game_okey_reward",	20417, true },
		// The Christmas Rumi (Owsap's default "mini_game_okey"): a GM's flag only.
		{ "rumi_xmas",	-1,								"mini_game_okey",			FLAG_END_EPOCH,	0, NULL,			"mini_game_okey_drop",		"mini_game_okey_reward",	20417, true },
		{ "yutnori",	playerbot_events::KIND_YUTNORI,	"mini_game_yutnori",		FLAG_END_EPOCH,	0, NULL,			"mini_game_yutnori_drop",	"mini_game_yutnori_reward",	20502, true },
		// MT2009_PLUS_FLOWER_V1: after the event a 7-day window in which the seeds
		// and shoots left can still be exchanged (playerbot_flower.h); Owsap had none.
		{ "flower",		playerbot_events::KIND_FLOWER,		"e_flower_drop",			FLAG_VALUE,		0, NULL,			NULL,						"e_flower_reward",			0, true },
		// MT2009_PLUS_CATCH_KING_V1: Catch the King's top ten collect their prize at
		// the table in a 7-day window too (playerbot_catchking.h).
		{ "catchking",	playerbot_events::KIND_CATCHKING,	"mini_game_catchking",		FLAG_END_EPOCH,	0, NULL,			"mini_game_catchking_drop",	"mini_game_catchking_reward",	20506, true },
		// event_easter.quest's two switches (the classic panel's Easter page).
		{ "easter",		playerbot_events::KIND_EASTER,		"easter_drop",				FLAG_VALUE,		1, "easter_rabbit",	NULL,						NULL,						0, true },
	};
	const int DEFS = sizeof(s_defs) / sizeof(s_defs[0]);

	// Owsap's RUMI_REWARD_COOLDOWN / YUTNORI_REWARD_COOLDOWN and the default
	// <drop_value> of its GM commands.
	const int REWARD_WINDOW_SECONDS = 7 * 24 * 60 * 60;
	const int DEFAULT_DROP = 100;
	// The event flags reach a core a moment after its boot; the leader writes
	// nothing before this, or a restart inside an event would look like its end.
	const DWORD BOOT_DELAY_MS = 30000;
	const DWORD NPC_INTERVAL_MS = 5000;
	// A flag asked for is not asked for again with the same value this soon:
	// the DB core's answer takes a moment to come round.
	const long REQUEST_HOLD_SECONDS = 15;

	enum ECaps
	{
		CAP_PACKET = 1,
		CAP_TEXT = 2,
		CAP_OWSAP_FLAGS = 4,
		CAP_ALL = 7,
	};

	// Owsap's cells (minigame_rumi.cpp, minigame_yutnori.cpp,
	// minigame_catchking.cpp SpawnEventNPC), all walkable on this world's
	// server_attr (checked 30 September; our villages are Owsap's).
	struct TNpcSpot
	{
		DWORD	vnum;
		long	map;
		long	cellX, cellY;
		DWORD	vid;
	};

	TNpcSpot s_spots[] =
	{
		{ 20417,  1, 607, 619, 0 },	// Rumi - Yongan (metin2_map_a1)
		{ 20417, 21, 595, 613, 0 },	// Rumi - Joan (metin2_map_b1)
		{ 20417, 41, 353, 741, 0 },	// Rumi - Pyungmoo (metin2_map_c1)
		{ 20502,  1, 608, 614, 0 },	// Yut Nori
		{ 20502, 21, 596, 608, 0 },
		{ 20502, 41, 358, 748, 0 },
		{ 20506,  1, 608, 623, 0 },	// Catch the King
		{ 20506, 21, 596, 614, 0 },
		{ 20506, 41, 350, 738, 0 },
	};
	const int SPOTS = sizeof(s_spots) / sizeof(s_spots[0]);
	const int NPC_ROTATION = 90;

	struct TState
	{
		bool	enable;
		DWORD	start;
		DWORD	end;
		DWORD	rewardEnd;
		int		value;
		bool operator==(const TState& o) const
		{
			return enable == o.enable && start == o.start && end == o.end && rewardEnd == o.rewardEnd && value == o.value;
		}
	};

	struct TEvent
	{
		TEventDef	def;
		TState		state;
		DWORD		firstSeen;	// a flag event's start: when this core first saw it on
		bool		schedWas;	// the leader's last look at its scheduler kind
	};

	std::vector<TEvent> s_events;
	// What Owsap's client would have heard: the flag's value as the state
	// says it (a flag the leader is still writing is already its new value).
	std::map<std::string, int> s_flagNow;
	std::map<std::string, int> s_flagBefore;
	int		s_iGeneration = 1;

	struct TClient
	{
		int caps;
		int generation;	// the list last sent, 0 = none yet
	};
	std::map<DWORD, TClient> s_clients;	// pid -> what the client can take

	DWORD	s_dwBootAt = 0;
	DWORD	s_dwNextSecond = 0;
	DWORD	s_dwNextNpc = 0;
	std::map<std::string, std::pair<int, long> > s_requested;	// flag -> (value, epoch asked)
	std::set<DWORD> s_setMissingMob;

	void BuildEvents()
	{
		if (!s_events.empty())
			return;
		std::set<int> kinds;
		for (int i = 0; i < DEFS; ++i)
		{
			TEvent e = TEvent();
			e.def = s_defs[i];
			s_events.push_back(e);
			if (s_defs[i].kind >= 0)
				kinds.insert(s_defs[i].kind);
		}
		for (int kind = 0; kind < playerbot_events::KIND_MAX; ++kind)
		{
			if (kinds.count(kind))
				continue;
			TEvent e = TEvent();
			e.def.key = playerbot_events::KindName(kind);
			e.def.kind = kind;
			e.def.meaning = FLAG_NONE;
			s_events.push_back(e);
		}
		for (size_t i = 0; i < s_events.size(); ++i)
			if (!s_events[i].def.key || !*s_events[i].def.key || strlen(s_events[i].def.key) > INGAME_EVENT_KEY_MAX_LEN)
				sys_err("INGAME_EVENT: event %u has no usable key", (unsigned int)i);
	}

	TEvent* Find(const char* key)
	{
		if (!key)
			return NULL;
		for (size_t i = 0; i < s_events.size(); ++i)
			if (s_events[i].def.key && !strcmp(s_events[i].def.key, key))
				return &s_events[i];
		return NULL;
	}

	int Flag(const char* name)
	{
		return name ? quest::CQuestManager::instance().GetEventFlag(name) : 0;
	}

	// RequestSetEventFlag through the DB core, once for a value within
	// REQUEST_HOLD_SECONDS.
	void Request(const char* name, int value, long now)
	{
		if (!name)
			return;
		std::map<std::string, std::pair<int, long> >::iterator it = s_requested.find(name);
		if (it != s_requested.end() && it->second.first == value && now - it->second.second < REQUEST_HOLD_SECONDS)
			return;
		s_requested[name] = std::make_pair(value, now);
		quest::CQuestManager::instance().RequestSetEventFlag(name, value);
		sys_log(0, "INGAME_EVENT: flag %s = %d", name, value);
	}

	// A flag the leader has just asked to be zeroed, which the DB core has not
	// answered yet: not an event ending by itself.
	bool ZeroPending(const char* name, long now)
	{
		if (!name)
			return false;
		std::map<std::string, std::pair<int, long> >::iterator it = s_requested.find(name);
		return it != s_requested.end() && it->second.first == 0 && now - it->second.second < REQUEST_HOLD_SECONDS;
	}

	// The figure an event carries: a mini game's drop share, a switch's
	// value, a rate's percent.
	int Value(const TEventDef& d)
	{
		if (d.dropFlag)
		{
			const int v = Flag(d.dropFlag);
			return v > 0 ? v : DEFAULT_DROP;
		}
		if (d.meaning == FLAG_VALUE)
		{
			const int v = Flag(d.flag);
			if (v > 0)
				return v;
			return d.onValue > 0 ? d.onValue : DEFAULT_DROP;
		}
		if (d.kind >= 0)
			return s_aPlayerBotEventStatus[d.kind].value;
		return 0;
	}

	// ------------------------------------------------------------ the state

	TState Evaluate(TEvent& e, long now)
	{
		const TEventDef& d = e.def;
		TState s = TState();
		bool sched = false;
		long schedUntil = 0, schedSince = 0;
		if (d.kind >= 0)
		{
			const playerbot_events::Status& st = s_aPlayerBotEventStatus[d.kind];
			sched = st.active;
			schedUntil = st.until;
			schedSince = st.since;
		}
		const int flag = d.meaning == FLAG_NONE ? 0 : Flag(d.flag);
		bool flagOn = false;
		if (d.meaning == FLAG_END_EPOCH)
			flagOn = flag > now && !ZeroPending(d.flag, now);
		else if (d.meaning == FLAG_VALUE)
			flagOn = flag > 0 && !ZeroPending(d.flag, now);
		s.enable = sched || flagOn;
		if (s.enable)
		{
			if (!e.firstSeen)
				e.firstSeen = (DWORD)now;
			s.end = (DWORD)(sched ? schedUntil : (d.meaning == FLAG_END_EPOCH ? flag : 0));
			s.start = (DWORD)(sched && schedSince > 0 ? schedSince : e.firstSeen);
			s.value = Value(d);
		}
		else
			e.firstSeen = 0;
		const int reward = Flag(d.rewardFlag);
		s.rewardEnd = reward > now && !ZeroPending(d.rewardFlag, now) ? (DWORD)reward : 0;
		return s;
	}

	// The flags as Owsap's client knows them, from the state.
	void FlagsFromState(std::map<std::string, int>& out)
	{
		out.clear();
		for (size_t i = 0; i < s_events.size(); ++i)
		{
			const TEventDef& d = s_events[i].def;
			if (d.meaning == FLAG_NONE || !d.owsapCommand)
				continue;
			const TState& s = s_events[i].state;
			int v = 0;
			if (s.enable)
				v = d.meaning == FLAG_END_EPOCH ? (int)(s.end ? s.end : 1) : (s.value > 0 ? s.value : 1);
			out[d.flag] = v;
		}
	}

	// ------------------------------------------------------------ the leader

	// schedWas starts false: after a boot what runs now "starts" again (the
	// writes are the same values), and an event that ended while the core was
	// down is ended below by its flag's time.
	void Lead(long now)
	{
		for (size_t i = 0; i < s_events.size(); ++i)
		{
			TEvent& e = s_events[i];
			const TEventDef& d = e.def;
			if (d.kind < 0 || d.meaning == FLAG_NONE)
				continue;
			const playerbot_events::Status& st = s_aPlayerBotEventStatus[d.kind];
			if (st.active)
			{
				if (!e.schedWas)
				{
					sys_log(0, "INGAME_EVENT: %s starts (scheduler, until %ld)", d.key, st.until);
					if (d.dropFlag && Flag(d.dropFlag) <= 0)
						Request(d.dropFlag, DEFAULT_DROP, now);
					if (d.rewardFlag && Flag(d.rewardFlag) != 0)
						Request(d.rewardFlag, 0, now);
					if (d.companion && Flag(d.companion) <= 0)
						Request(d.companion, 1, now);
				}
				const int want = d.meaning == FLAG_END_EPOCH ? (int)st.until : Value(d);
				if (Flag(d.flag) != want)
					Request(d.flag, want, now);
			}
			else if (e.schedWas)
			{
				sys_log(0, "INGAME_EVENT: %s ends (scheduler)", d.key);
				Request(d.flag, 0, now);
				if (d.companion)
					Request(d.companion, 0, now);
				if (d.rewardFlag)
					Request(d.rewardFlag, (int)(now + REWARD_WINDOW_SECONDS), now);
			}
			e.schedWas = st.active;
		}
		// Owsap's UpdateInGameEvent: an end-epoch flag whose end has passed
		// goes to 0 and opens the reward window; a reward window that has
		// passed (or Owsap's -1, "take the NPC away now") goes to 0. The
		// scheduler's own events were ended above.
		for (size_t i = 0; i < s_events.size(); ++i)
		{
			const TEventDef& d = s_events[i].def;
			if (d.meaning == FLAG_END_EPOCH)
			{
				const bool sched = d.kind >= 0 && s_aPlayerBotEventStatus[d.kind].active;
				const int v = Flag(d.flag);
				if (!sched && v != 0 && v <= now && !ZeroPending(d.flag, now))
				{
					sys_log(0, "INGAME_EVENT: %s ends (flag %s = %d)", d.key, d.flag, v);
					Request(d.flag, 0, now);
					if (d.rewardFlag)
						Request(d.rewardFlag, (int)(now + REWARD_WINDOW_SECONDS), now);
				}
			}
			if (d.rewardFlag)
			{
				const int r = Flag(d.rewardFlag);
				if (r != 0 && r <= now && !ZeroPending(d.rewardFlag, now))
				{
					sys_log(0, "INGAME_EVENT: %s reward window over (%s = %d)", d.key, d.rewardFlag, r);
					Request(d.rewardFlag, 0, now);
				}
			}
		}
	}

	// ------------------------------------------------------------ the NPCs

	bool NpcWanted(DWORD vnum)
	{
		for (size_t i = 0; i < s_events.size(); ++i)
		{
			const TEvent& e = s_events[i];
			if (e.def.npc == vnum && (e.state.enable || e.state.rewardEnd))
				return true;
		}
		return false;
	}

	void SyncNpcs()
	{
		for (int i = 0; i < SPOTS; ++i)
		{
			TNpcSpot& spot = s_spots[i];
			if (!map_allow_find(spot.map))
				continue;
			LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(spot.map);
			if (!map)
				continue;
			LPCHARACTER ch = spot.vid ? CHARACTER_MANAGER::instance().Find(spot.vid) : NULL;
			if (ch && (ch->GetRaceNum() != spot.vnum || ch->GetMapIndex() != spot.map))
				ch = NULL;
			if (!NpcWanted(spot.vnum))
			{
				if (ch)
				{
					M2_DESTROY_CHARACTER(ch);
					sys_log(0, "INGAME_EVENT: NPC %u taken from map %ld", spot.vnum, spot.map);
				}
				spot.vid = 0;
				continue;
			}
			if (ch)
				continue;
			spot.vid = 0;
			if (!CMobManager::instance().Get(spot.vnum))
			{
				if (s_setMissingMob.insert(spot.vnum).second)
					sys_log(0, "INGAME_EVENT: NPC %u has no mob_proto row yet - not spawned", spot.vnum);
				continue;
			}
			const long x = map->m_setting.iBaseX + spot.cellX * 100;
			const long y = map->m_setting.iBaseY + spot.cellY * 100;
			ch = CHARACTER_MANAGER::instance().SpawnMob(spot.vnum, spot.map, x, y, 0, false, NPC_ROTATION, true);
			if (!ch)
			{
				sys_err("INGAME_EVENT: cannot spawn NPC %u on map %ld at %ld %ld", spot.vnum, spot.map, x, y);
				continue;
			}
			spot.vid = ch->GetVID();
			sys_log(0, "INGAME_EVENT: NPC %u on map %ld (cell %ld %ld), vid %u", spot.vnum, spot.map, spot.cellX, spot.cellY, spot.vid);
		}
	}

	// ------------------------------------------------------------ the clients

	bool Eligible(LPCHARACTER ch)
	{
		return ch && ch->IsPC() && ch->GetDesc() && !ch->GetDesc()->IsBot();
	}

	bool Listed(const TEvent& e)
	{
		return e.def.key && *e.def.key && strlen(e.def.key) <= INGAME_EVENT_KEY_MAX_LEN;
	}

	void SendPacket(LPCHARACTER ch)
	{
		std::vector<TPacketGCInGameEventInfo> infos;
		for (size_t i = 0; i < s_events.size() && infos.size() < 255; ++i)
		{
			const TEvent& e = s_events[i];
			if (!Listed(e))
				continue;
			TPacketGCInGameEventInfo info;
			memset(&info, 0, sizeof(info));
			strlcpy(info.key, e.def.key, sizeof(info.key));
			info.enable = e.state.enable ? 1 : 0;
			info.start_time = e.state.start;
			info.end_time = e.state.end;
			info.reward_end_time = e.state.rewardEnd;
			info.value = e.state.value;
			infos.push_back(info);
		}
		TPacketGCInGameEvent head;
		head.header = HEADER_GC_INGAME_EVENT;
		head.subheader = INGAME_EVENT_SUBHEADER_GC_LIST;
		head.count = (BYTE)infos.size();
		head.size = (WORD)(sizeof(head) + infos.size() * sizeof(TPacketGCInGameEventInfo));
		std::vector<BYTE> buf(head.size);
		memcpy(&buf[0], &head, sizeof(head));
		if (!infos.empty())
			memcpy(&buf[sizeof(head)], &infos[0], infos.size() * sizeof(TPacketGCInGameEventInfo));
		ch->GetDesc()->Packet(&buf[0], (int)buf.size());
	}

	void SendText(LPCHARACTER ch)
	{
		int count = 0;
		for (size_t i = 0; i < s_events.size(); ++i)
			if (Listed(s_events[i]))
				++count;
		ch->ChatPacket(CHAT_TYPE_COMMAND, "IGE begin %d", count);
		for (size_t i = 0; i < s_events.size(); ++i)
		{
			const TEvent& e = s_events[i];
			if (!Listed(e))
				continue;
			ch->ChatPacket(CHAT_TYPE_COMMAND, "IGE ev %s %d %u %u %u %d", e.def.key, e.state.enable ? 1 : 0,
					(unsigned int)e.state.start, (unsigned int)e.state.end, (unsigned int)e.state.rewardEnd, e.state.value);
		}
		ch->ChatPacket(CHAT_TYPE_COMMAND, "IGE end");
	}

	// Owsap's BroadcastInGameEventOnLogin (full: every non-zero flag) and
	// SetInGameEvent's broadcast (a changed flag, 0 included).
	void SendOwsapFlags(LPCHARACTER ch, bool full)
	{
		for (std::map<std::string, int>::const_iterator it = s_flagNow.begin(); it != s_flagNow.end(); ++it)
		{
			if (full)
			{
				if (it->second != 0)
					ch->ChatPacket(CHAT_TYPE_COMMAND, "%s %d", it->first.c_str(), it->second);
				continue;
			}
			std::map<std::string, int>::const_iterator before = s_flagBefore.find(it->first);
			if (before == s_flagBefore.end() || before->second != it->second)
				ch->ChatPacket(CHAT_TYPE_COMMAND, "%s %d", it->first.c_str(), it->second);
		}
	}

	void Send(LPCHARACTER ch, TClient& client)
	{
		if (!Eligible(ch))
			return;
		// One step behind: the Owsap flags that changed; anything else (a new
		// client, one that missed a list): every flag that is on.
		const bool full = client.generation == 0 || client.generation != s_iGeneration - 1;
		if (client.caps & CAP_PACKET)
			SendPacket(ch);
		else if (client.caps & CAP_TEXT)
			SendText(ch);
		if (client.caps & CAP_OWSAP_FLAGS)
			SendOwsapFlags(ch, full);
		client.generation = s_iGeneration;
	}

	void EverySecond(DWORD dwNow)
	{
		BuildEvents();
		const long now = (long)time(NULL);
		if (!s_dwBootAt)
		{
			s_dwBootAt = dwNow;
			FlagsFromState(s_flagNow);
		}
		const bool booted = dwNow - s_dwBootAt >= BOOT_DELAY_MS;
		if (booted && IsPlayerBotEventLeader())
			Lead(now);

		bool changed = false;
		for (size_t i = 0; i < s_events.size(); ++i)
		{
			TEvent& e = s_events[i];
			const TState fresh = Evaluate(e, now);
			if (fresh == e.state)
				continue;
			sys_log(0, "INGAME_EVENT: %s %s start %u end %u reward %u value %d (was %s until %u)",
					e.def.key, fresh.enable ? "on" : "off", (unsigned int)fresh.start, (unsigned int)fresh.end,
					(unsigned int)fresh.rewardEnd, fresh.value, e.state.enable ? "on" : "off", (unsigned int)e.state.end);
			e.state = fresh;
			changed = true;
		}
		if (changed)
		{
			s_flagBefore = s_flagNow;
			FlagsFromState(s_flagNow);
			++s_iGeneration;
		}

		// The clients that said hello, while they stand in this core's world.
		std::set<DWORD> present;
		const DESC_MANAGER::DESC_SET& descs = DESC_MANAGER::instance().GetClientSet();
		for (DESC_MANAGER::DESC_SET::const_iterator it = descs.begin(); it != descs.end(); ++it)
		{
			LPDESC desc = *it;
			LPCHARACTER ch = desc ? desc->GetCharacter() : NULL;
			if (!ch || desc->IsBot() || !desc->IsPhase(PHASE_GAME))
				continue;
			present.insert(ch->GetPlayerID());
			std::map<DWORD, TClient>::iterator c = s_clients.find(ch->GetPlayerID());
			if (c != s_clients.end() && c->second.generation != s_iGeneration)
				Send(ch, c->second);
		}
		for (std::map<DWORD, TClient>::iterator c = s_clients.begin(); c != s_clients.end(); )
		{
			if (present.count(c->first))
				++c;
			else
				s_clients.erase(c++);
		}

		if (booted && (s_dwNextNpc == 0 || (int)(dwNow - s_dwNextNpc) >= 0))
		{
			s_dwNextNpc = dwNow + NPC_INTERVAL_MS;
			SyncNpcs();
		}
	}
}

// The mini games' own seconds (their files are included after this one).
void YutnoriTick(DWORD dwNow);	// playerbot_yutnori.h (MT2009_PLUS_YUTNORI_V1)
void YutnoriGmInfo(LPCHARACTER ch);

// Every pass of the events (playerbot_events.h), once a second at most.
void InGameEventTick(DWORD dwNow)
{
	using namespace mt2009_ingame_event;
	if (s_dwNextSecond && (int)(dwNow - s_dwNextSecond) < 0)
		return;
	s_dwNextSecond = dwNow + 1000;
	EverySecond(dwNow);
	YutnoriTick(dwNow);
}

// "/ingame_event hello <caps>" from the client's python, "/ingame_event info"
// for the list again, "/ingame_event gm" for a GM's look
// (server-patches/eventmanager, MT2009_PLUS_EVENT_MANAGER_V1 (command)).
void InGameEventCommand(LPCHARACTER ch, const char* argument)
{
	using namespace mt2009_ingame_event;
	if (!Eligible(ch))
		return;
	BuildEvents();
	char sub[32], arg[32];
	const char* rest = one_argument(argument, sub, sizeof(sub));
	one_argument(rest, arg, sizeof(arg));
	if (!strcmp(sub, "hello") || !strcmp(sub, "info"))
	{
		int caps = CAP_PACKET | CAP_OWSAP_FLAGS;
		std::map<DWORD, TClient>::iterator c = s_clients.find(ch->GetPlayerID());
		if (!strcmp(sub, "hello"))
		{
			if (*arg)
				str_to_number(caps, arg);
		}
		else if (c != s_clients.end())
			caps = c->second.caps;
		else
			return;
		caps &= CAP_ALL;
		// The packet or the lines, never both.
		if ((caps & CAP_PACKET) && (caps & CAP_TEXT))
			caps &= ~CAP_TEXT;
		if (!caps)
		{
			s_clients.erase(ch->GetPlayerID());
			return;
		}
		TClient& client = s_clients[ch->GetPlayerID()];
		client.caps = caps;
		client.generation = 0;
		Send(ch, client);
		sys_log(0, "INGAME_EVENT: %s %s caps %d", ch->GetName(), sub, caps);
	}
	else if (!strcmp(sub, "gm") && ch->GetGMLevel() >= GM_HIGH_WIZARD)
	{
		const long now = (long)time(NULL);
		for (size_t i = 0; i < s_events.size(); ++i)
		{
			const TEvent& e = s_events[i];
			if (!e.state.enable && !e.state.rewardEnd && e.def.meaning == FLAG_NONE)
				continue;
			ch->ChatPacket(CHAT_TYPE_INFO, "%s: %s, koniec za %ld min, nagrody jeszcze %ld min, wartosc %d, flaga %s = %d",
					e.def.key, e.state.enable ? "trwa" : "nie trwa",
					e.state.end > (DWORD)now ? ((long)e.state.end - now) / 60 : 0L,
					e.state.rewardEnd > (DWORD)now ? ((long)e.state.rewardEnd - now) / 60 : 0L, e.state.value,
					e.def.flag ? e.def.flag : "-", Flag(e.def.flag));
		}
		ch->ChatPacket(CHAT_TYPE_INFO, "Klienci z lista eventow na tym rdzeniu: %u, lider: %s.",
				(unsigned int)s_clients.size(), IsPlayerBotEventLeader() ? "tak" : "nie");
		YutnoriGmInfo(ch);
	}
}

// For the mini games that plug in next (declare them where they are used, as
// the engine's hooks declare GoblinCommand). The key is the list's
// ("catchking", "rumi", "rumi_xmas", "yutnori", "flower", "easter" ...).
bool InGameEventIsActive(const char* key)
{
	const mt2009_ingame_event::TEvent* e = mt2009_ingame_event::Find(key);
	return e && e->state.enable;
}

DWORD InGameEventEndTime(const char* key)
{
	const mt2009_ingame_event::TEvent* e = mt2009_ingame_event::Find(key);
	return e ? e->state.end : 0;
}

DWORD InGameEventRewardEndTime(const char* key)
{
	const mt2009_ingame_event::TEvent* e = mt2009_ingame_event::Find(key);
	return e ? e->state.rewardEnd : 0;
}

int InGameEventValue(const char* key)
{
	const mt2009_ingame_event::TEvent* e = mt2009_ingame_event::Find(key);
	return e ? e->state.value : 0;
}
