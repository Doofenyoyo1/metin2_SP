#ifndef __INC_METIN2_PLAYERBOT_ADMIN_H__
#define __INC_METIN2_PLAYERBOT_ADMIN_H__

// What the F10 bot-admin window asks the core, and where those answers come
// from.
//
// The window is OskarPWA's; three of the things it asks for did not exist on
// this side, because his fork's manager answers them and ours does not. They
// are kept here rather than in the manager for the usual reason: the manager is
// the tick, and this is a reporting surface that nothing in the AI reads.
//
// Two of them need memory the AI never kept:
//
//   * "Akcje na zywo" wants the last few things a bot did. Nothing records
//     that - PlayerBotLogThrottled deliberately collapses a line written by
//     three hundred bots into one, which is the opposite of a per-bot history.
//     So the status snapshot, which already composes one sentence per bot every
//     two seconds, drops that sentence in here when it *changes*. An unchanged
//     status is not an action, and a bot standing at a stall for twenty minutes
//     would otherwise fill its whole buffer with one line.
//
//   * "Osiagniecia" wants the first bot to reach level 30, 60 and 90. The only
//     honest way to know who was first is to watch somebody cross the line, so
//     that is what this does: the first observation of a bot only records where
//     it already is, and an award is made when a bot the core has seen below a
//     threshold is later seen at or above it. On a world whose bots are already
//     past 30 the row therefore stays empty until somebody actually levels up -
//     which is correct, and better than crowning whichever PID the map happened
//     to iterate first. Winners are kept in a file beside playerbot_status.tsv
//     so a restart does not re-open a race that was already run.
//
// Both are per core, like everything else the F10 window shows: its bot list is
// built from CHARACTER_MANAGER::FindByPID, which only ever finds the bots this
// core hosts.
//
// An implementation fragment in the sense playerbot_types.h describes: it
// defines objects, relies on the engine headers playerbot_manager.cpp includes
// above it, and reopens the same anonymous namespace. Include it exactly once,
// after playerbot_status.h - BuildPlayerBotStatusText is what feeds it.

namespace
{
	// How many lines one bot keeps. The window shows a short tail, and this is
	// three hundred bots times this many strings held for as long as the core
	// runs, so it is deliberately small.
	const size_t PLAYERBOT_ADMIN_LOG_LINES = 12;

	// The levels the client labels (PBA_ACHIEVEMENT_LABELS in
	// interfacemodule.py: "Pierwszy 30/60/90 poziom"). The id sent on the wire
	// is the index + 1; the label itself never travels, because the client's
	// command parser cannot take a space inside an argument.
	const int PLAYERBOT_ADMIN_ACHIEVEMENT_LEVELS[] = { 30, 60, 90 };
	const size_t PLAYERBOT_ADMIN_ACHIEVEMENT_COUNT =
			sizeof(PLAYERBOT_ADMIN_ACHIEVEMENT_LEVELS) /
			sizeof(PLAYERBOT_ADMIN_ACHIEVEMENT_LEVELS[0]);

	// Written next to playerbot_status.tsv, in the core's own working
	// directory: one core, one world, one set of winners.
	const char* const PLAYERBOT_ADMIN_ACHIEVEMENT_PATH = "playerbot_achievements.tsv";

	struct TPlayerBotAchievementWinner
	{
		DWORD		dwPID;
		std::string	strName;

		TPlayerBotAchievementWinner() : dwPID(0) {}
	};

	// id (1-based) -> who got there first. Absent means nobody yet.
	std::map<int, TPlayerBotAchievementWinner> s_mapPlayerBotAchievements;
	// What level each bot was last seen at, so a crossing can be noticed.
	std::map<DWORD, int> s_mapPlayerBotLastSeenLevel;
	bool s_bPlayerBotAchievementsLoaded = false;

	// pid -> its recent status lines, oldest first.
	std::map<DWORD, std::deque<std::string> > s_mapPlayerBotAdminLog;

	void LoadPlayerBotAchievements()
	{
		if (s_bPlayerBotAchievementsLoaded)
			return;
		s_bPlayerBotAchievementsLoaded = true;

		FILE* fp = fopen(PLAYERBOT_ADMIN_ACHIEVEMENT_PATH, "r");
		if (!fp)
			return;

		char line[256];
		while (fgets(line, sizeof(line), fp))
		{
			int id = 0;
			unsigned int pid = 0;
			char name[64];
			name[0] = '\0';
			// id, pid, name - the name never has a space in it, the character
			// screen refuses one, so %s is the whole field.
			if (sscanf(line, "%d\t%u\t%63s", &id, &pid, name) != 3)
				continue;
			if (id <= 0 || pid == 0)
				continue;
			TPlayerBotAchievementWinner& winner = s_mapPlayerBotAchievements[id];
			winner.dwPID = pid;
			winner.strName = name;
		}
		fclose(fp);
		sys_log(0, "PLAYERBOT_ADMIN: loaded %u achievement winner(s)",
				(unsigned int)s_mapPlayerBotAchievements.size());
	}

	void SavePlayerBotAchievements()
	{
		// Same temp-and-rename as the weights writer: a reader must never see
		// half a file.
		char szTemp[256];
		snprintf(szTemp, sizeof(szTemp), "%s.tmp", PLAYERBOT_ADMIN_ACHIEVEMENT_PATH);
		FILE* out = fopen(szTemp, "w");
		if (!out)
		{
			sys_err("PLAYERBOT_ADMIN: cannot write %s", szTemp);
			return;
		}
		for (std::map<int, TPlayerBotAchievementWinner>::const_iterator it =
					s_mapPlayerBotAchievements.begin();
				it != s_mapPlayerBotAchievements.end(); ++it)
		{
			fprintf(out, "%d\t%u\t%s\n", it->first,
					(unsigned int)it->second.dwPID, it->second.strName.c_str());
		}
		if (fclose(out) != 0 ||
				rename(szTemp, PLAYERBOT_ADMIN_ACHIEVEMENT_PATH) != 0)
		{
			unlink(szTemp);
			sys_err("PLAYERBOT_ADMIN: cannot replace %s",
					PLAYERBOT_ADMIN_ACHIEVEMENT_PATH);
		}
	}

	// One bot's sentence, as the status snapshot just composed it. Only a
	// changed sentence is an action worth keeping.
	void NotePlayerBotAdminStatus(DWORD dwPID, const char* szStatus)
	{
		if (!szStatus || !*szStatus)
			return;

		std::deque<std::string>& log = s_mapPlayerBotAdminLog[dwPID];
		if (!log.empty() && log.back() == szStatus)
			return;

		log.push_back(szStatus);
		while (log.size() > PLAYERBOT_ADMIN_LOG_LINES)
			log.pop_front();
	}

	// Called with every bot the snapshot walks. The first sighting only records
	// where the bot already is - crossing a line is what earns the row.
	void NotePlayerBotAdminLevel(LPCHARACTER ch)
	{
		if (!ch)
			return;

		LoadPlayerBotAchievements();

		const DWORD dwPID = ch->GetPlayerID();
		const int level = ch->GetLevel();
		std::map<DWORD, int>::iterator seen = s_mapPlayerBotLastSeenLevel.find(dwPID);
		const bool bFirstSighting = (seen == s_mapPlayerBotLastSeenLevel.end());
		const int previous = bFirstSighting ? level : seen->second;
		s_mapPlayerBotLastSeenLevel[dwPID] = level;

		if (bFirstSighting || level <= previous)
			return;

		bool bChanged = false;
		for (size_t i = 0; i < PLAYERBOT_ADMIN_ACHIEVEMENT_COUNT; ++i)
		{
			const int threshold = PLAYERBOT_ADMIN_ACHIEVEMENT_LEVELS[i];
			if (previous >= threshold || level < threshold)
				continue;

			const int id = (int)i + 1;
			if (s_mapPlayerBotAchievements.find(id) != s_mapPlayerBotAchievements.end())
				continue;

			TPlayerBotAchievementWinner& winner = s_mapPlayerBotAchievements[id];
			winner.dwPID = dwPID;
			winner.strName = ch->GetName() ? ch->GetName() : "";
			bChanged = true;
			sys_log(0, "PLAYERBOT_ADMIN: achievement %d (level %d) won by pid=%u name=%s",
					id, threshold, (unsigned int)dwPID, winner.strName.c_str());
		}
		if (bChanged)
			SavePlayerBotAchievements();
	}

	bool GetPlayerBotAdminAchievement(int id, DWORD& dwPID, std::string& strName)
	{
		LoadPlayerBotAchievements();

		std::map<int, TPlayerBotAchievementWinner>::const_iterator it =
				s_mapPlayerBotAchievements.find(id);
		if (it == s_mapPlayerBotAchievements.end() || it->second.dwPID == 0)
			return false;

		dwPID = it->second.dwPID;
		strName = it->second.strName;
		return true;
	}

	// The live sentence first, then what came before it, newest last. A bot the
	// snapshot has not reached yet has no history and gets one line.
	void GetPlayerBotAdminLines(DWORD dwPID, std::vector<std::string>& out)
	{
		out.clear();

		std::map<DWORD, std::deque<std::string> >::const_iterator it =
				s_mapPlayerBotAdminLog.find(dwPID);
		if (it == s_mapPlayerBotAdminLog.end())
			return;

		for (std::deque<std::string>::const_iterator line = it->second.begin();
				line != it->second.end(); ++line)
			out.push_back(*line);
	}

	// A bot that is gone keeps neither a history nor a remembered level.
	void ForgetPlayerBotAdminState(DWORD dwPID)
	{
		s_mapPlayerBotAdminLog.erase(dwPID);
		s_mapPlayerBotLastSeenLevel.erase(dwPID);
	}
}

#endif
