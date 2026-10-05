#ifndef __INC_METIN2_PLAYERBOT_DBDATA_STAMP_H__
#define __INC_METIN2_PLAYERBOT_DBDATA_STAMP_H__

// MT2009_PLUS_DBDATA_STAMP_V1: "your client's item/skill files are not this
// server's".
//
// The Seban panel's database editor changes items and skills on the server;
// the client shows names, bonuses and descriptions from its own pack/dbdata,
// which the player replaces with the zip the panel hands out. A client
// update that puts the release's pack back (or a zip of an older client
// base) shows the original data again while the server runs the edits, and
// the player has no way to know (the owner, 5 October).
//
// The panel keeps a stamp of what the zip carries in /opt/m2spool/
// dbdata_stamp.txt (dbeditor/clientdata.py): the client base's version
// alone with nothing client-visible edited, else "<version>-<12 hex>". The
// zip has the same stamp in the client root's dbdata_stamp.txt. At every
// entry into the game a person (not a bot) gets "DbDataStamp <stamp>"; the
// client (dbdatastamp.py) compares it with its own file and, when they
// differ and the server has edits, asks once a session to download the zip.
// An old client has no handler for the command and passes it over.
//
// The file: '#' comments and a line "stamp <value>"; read again when its
// mtime or size changes (a stat at each login, nothing else). No file, or
// no valid stamp in it: nothing is sent. Called from Mt2009DigiDailyGift,
// the login hook of server-patches/digirasta-qol (input_login.cpp
// Entergame), so no engine change of its own.

#include <sys/stat.h>
#include <cctype>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <string>

namespace mt2009_dbstamp
{
	const char* const STAMP_PATH = "/opt/m2spool/dbdata_stamp.txt";
	const size_t STAMP_MAX = 64;

	struct SCache
	{
		bool known = false;
		time_t mtime = 0;
		off_t size = -1;
		std::string stamp;
	};

	inline SCache& Cache()
	{
		static SCache s;
		return s;
	}

	// A stamp goes into a chat command: letters, digits, '.', '-' and '_' only.
	inline bool Valid(const std::string& s)
	{
		if (s.empty() || s.size() > STAMP_MAX)
			return false;
		for (size_t i = 0; i < s.size(); ++i)
		{
			const unsigned char c = (unsigned char) s[i];
			if (!(isalnum(c) || c == '.' || c == '-' || c == '_'))
				return false;
		}
		return true;
	}

	inline std::string ParseStamp(FILE* f)
	{
		char line[256];
		while (fgets(line, sizeof(line), f))
		{
			char key[32], value[128];
			if (line[0] == '#')
				continue;
			if (sscanf(line, "%31s %127s", key, value) == 2 && !strcmp(key, "stamp"))
			{
				const std::string s(value);
				return Valid(s) ? s : std::string();
			}
		}
		return std::string();
	}

	// The current stamp ("" when there is none).
	inline const std::string& Current()
	{
		SCache& c = Cache();
		struct stat st;
		if (stat(STAMP_PATH, &st) != 0)
		{
			c.known = false;
			c.stamp.clear();
			return c.stamp;
		}
		if (c.known && c.mtime == st.st_mtime && c.size == st.st_size)
			return c.stamp;
		c.known = true;
		c.mtime = st.st_mtime;
		c.size = st.st_size;
		c.stamp.clear();
		if (FILE* f = fopen(STAMP_PATH, "r"))
		{
			c.stamp = ParseStamp(f);
			fclose(f);
		}
		return c.stamp;
	}
}

// A character enters the game (each map change too: the client keeps the
// notice to once a session itself).
inline void Mt2009DbDataStampOnLogin(LPCHARACTER ch)
{
	if (!ch || !ch->IsPC() || !ch->GetDesc() || ch->GetDesc()->IsBot())
		return;
	const std::string& stamp = mt2009_dbstamp::Current();
	if (!stamp.empty())
		ch->ChatPacket(CHAT_TYPE_COMMAND, "DbDataStamp %s", stamp.c_str());
}

#endif
