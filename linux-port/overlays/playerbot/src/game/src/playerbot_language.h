#ifndef __INC_METIN2_PLAYERBOT_LANGUAGE_H__
#define __INC_METIN2_PLAYERBOT_LANGUAGE_H__

// A line said to one person. MT2009 PLUS speaks Polish only: the base's
// per-person language (the login question "PlayerBotLanguage", the quest flag
// playerbot_lang.en, the English halves of the bots' notices and the English
// name tables) is not taken - this file keeps only the helper the base's
// later code calls, under the base's name, so the next merge finds it here.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, early - anything may speak to a person.

namespace
{
	// One line in one person's chat, printf-style.
	void TellPlayerBotPerson(LPCHARACTER person, const char* format, ...) __attribute__((format(printf, 2, 3)));
	void TellPlayerBotPerson(LPCHARACTER person, const char* format, ...)
	{
		if (!person || !person->GetDesc())
			return;
		char text[CHAT_MAX_LEN + 1];
		va_list args;
		va_start(args, format);
		vsnprintf(text, sizeof(text), format, args);
		va_end(args);
		person->ChatPacket(CHAT_TYPE_INFO, "%s", text);
	}
}

#endif
