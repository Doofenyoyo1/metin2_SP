// Rendered by linux-port/overlays/playerbot/tools/generate_guild_names.py
// from data/guild_names_iwakura.txt. DO NOT EDIT; edit the list and re-run.
//
// The names the bots found guilds under, in the order Iwakura wrote them.
// A founder walks this pool from a pid-based offset and takes the first
// name no guild in the world wears and the engine's GUILD_NAME_MAX_LEN
// admits (fourteen on mt2009, twelve on r40250), so the same header serves
// both lines. 100 names.
#ifndef __INC_METIN2_PLAYERBOT_GUILD_NAMES_H__
#define __INC_METIN2_PLAYERBOT_GUILD_NAMES_H__

namespace
{
	const char* const PLAYERBOT_GUILD_NAME_POOL[] = {
		"Shire", "UrzadPracy", "TotalneBoty", "AgressiveTeam",
		"FullOfHate", "SWAT", "Anonimowi", "NoMercy",
		"PompaTeam", "BezPodjazdu", "BRAT3RSTWO", "Brygada997",
		"PonadPrawem", "Outlaw", "CzarnyLegion", "Zenith",
		"BlueDeath", "Borderline", "LekcjaPokory", "VooDoo",
		"Przelew24", "BLIK", "SmoczaMoneta", "Heaven",
		"Paradise", "EkstraKlasa", "PODKARPACKA", "NoRespect",
		"ViceVersa", "VaeVictis", "Meksyk", "Excellence",
		"Dziekanat", "NocnaZmiana", "EverQuest", "Anarchia",
		"UNDERGROUND", "ZUS", "ERROR", "CzarneOrki",
		"produkcja", "StormCloud", "LianYu", "Hazardzisci",
		"ZimnaWodka", "ShadowBlade", "Amarena", "4Ever",
		"Templariusze", "Nevermind", "NeverEnough", "PALLADYNI",
		"Masarnia", "Bociarze", "Cloud9", "Vitality",
		"BloodLust", "Gameforge", "ABYSS", "RedBull",
		"MOONLIGHT", "Elementals", "WLOCLAWEK", "Zabka",
		"MINISTRANCI", "MONASTYR", "NeverGiveUp", "PolishWarriors",
		"Lidl", "Biedronka", "Zawodowcy", "NoExperience",
		"PATOLOGIA", "Dominate", "NieTaLiga", "BlackWolfs",
		"BIOHAZARD", "KochamCie", "NoToLecimy", "NeverForgive",
		"Tuskaffki", "GameOver", "Konfitury", "NoSexGoExp",
		"ZglosNas", "NoLimit", "Farmerzy", "BLACKREDWHITE",
		"Ashborn", "Hydra", "Winston", "ZbakaneChomiczki",
		"LudzieJezusa", "ZakonBigosu", "Blackshot", "IgnisAsinus",
		"Awangarda", "Yakuza", "Blitzkrieg", "MostWanted",
	};
	const size_t PLAYERBOT_GUILD_NAME_POOL_SIZE = sizeof(PLAYERBOT_GUILD_NAME_POOL) / sizeof(PLAYERBOT_GUILD_NAME_POOL[0]);
}

#endif
