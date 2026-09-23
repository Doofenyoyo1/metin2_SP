// Rendered by linux-port/overlays/playerbot/tools/generate_iwakura_persona.py from
// Iwakura's personality document (data/iwakura_osobowosci.txt, "SYSTEM OSOBOWOSCI
// v2.0", 19 September) and world.item_proto. Do not edit by hand.
//
// Two lists of his, as data: what lifts a bot's mood when it drops (the Bot
// Mood System's "predefiniowana lista wartosciowych przedmiotow"), and the
// weapons, armours and shields his LPP keeps in the storekeeper's box by the
// band of the bot's level. The skill books and the Forgetting books are one
// vnum each with the skill in socket 0, so they are lists of skills; a family
// of gear is its +0 vnum and the family is +0..+9.
#ifndef __INC_METIN2_PLAYERBOT_PERSONA_TABLES_H__
#define __INC_METIN2_PLAYERBOT_PERSONA_TABLES_H__

namespace
{
	const DWORD PLAYERBOT_MOOD_SKILL_BOOK_VNUM = 50300;
	const DWORD PLAYERBOT_MOOD_FORGET_BOOK_VNUM = 70037;
	// "Marmur Polimorfi (kazdy rodzaj)": every ITEM_POLYMORPH counts.
	const bool PLAYERBOT_MOOD_ANY_POLYMORPH = true;

	// Single items, sorted: materials, pearls, scrolls, stones, medals.
	const DWORD PLAYERBOT_MOOD_VALUABLE_VNUMS[] = {
		25040, // Zwoj Blogoslawienstwa
		27799, // Rybia Osc
		27987, // Malz
		27992, // Biala Perla
		27993, // Niebieska Perla
		27994, // Krwawa Perla
		28431, // Kamien Duszy Smierci +4
		28437, // Kamien Duszy Potwora +4
		28443, // Kamien Duszy Przyspieszenia +4
		30006, // Zab Orka
		30008, // Ezoteryczny Przewodnik
		30010, // Zolc Niedzwiedzia
		30015, // Pamiatka Po Demonie
		30018, // Czerwona Wstega
		30021, // Kawalek Klejnotu
		30042, // Pazur Tygrysa
		30047, // Ksiega Klatw
		30050, // Matowy Lod
		30051, // Nieznany Talizman
		30053, // Niedzwiedzia Skora
		30055, // Szpon Skorpiona
		30058, // Worek Z Pajeczymi Jajami
		30071, // Zolc Niedzwiedzia +
		30072, // Niedzwiedzia Skora +
		30073, // Biala Wstega +
		30075, // Shuriken +
		30079, // Nieznany Talizman +
		30084, // Nieznany Talizman +
		30346, // Niedzwiedzina
		30348, // Lodowata Maz
		30350, // Pazury Malpy
		30351, // Ogon Malpy
		30353, // Wieprzowina
		30354, // Czerwone nasiono
		30355, // Zgnile Mieso
		30356, // Waleczna Dusza Zaprzys
		30358, // Serce Wojownika
		30367, // Luski Smoka
		50050, // Medal konny
		50513, // Kamien duchowy
		70102, // Fasolka zen
	};

	// Families of gear, by the +0 vnum.
	const DWORD PLAYERBOT_MOOD_VALUABLE_FAMILIES[] = {
		290, // Miecz Pelni Ksiezyca
		2150, // Luk z Rogu Jelenia
		3210, // Ostrze Z Czerw. Stali
		5110, // Antyczny Dzwon
		7160, // Wachlarz Jes. Wiatru
		11290, // Zbroja Z Czarnej Stali
		11490, // Ubranie Czarn. Wiatru
		11690, // Zbr. Plyt. Czar. Magii
		11890, // Czarna Szata
		14200, // Bransol. Z Niebian.Lez
		15200, // Buty Feniksa
		16200, // Naszyj. Z Niebian.Lez.
		17100, // Ebonitowe Kolczyki
		17200, // Kolczyki Z Niebian.Lez
	};

	// Skill books (50300) by the skill in socket 0.
	const BYTE PLAYERBOT_MOOD_VALUABLE_BOOK_SKILLS[] = {
		4, // Ksiega Umiejetnosci Aura Miecza
		19, // Ksiega Umiejetnosci Silne Cialo
		63, // Ksiega Umiejetnosci Czarowane Ostrze
		64, // Ksiega Umiejetnosci Strach
		94, // Ksiega Umiejetnosci Blogoslawienstwo
		96, // Ksiega Umiejetnosci Pomoc Smoka
	};

	// Forgetting books (70037) - his "Opaska Zapomnienia" - by the skill in socket 0.
	const BYTE PLAYERBOT_MOOD_VALUABLE_FORGET_SKILLS[] = {
		3, // Opaska Zapomnienia Berserk
		16, // Opaska Zapomnienia Duchowe Uderzenie
		35, // Opaska Zapomnienia Trujaca Chmura
		64, // Opaska Zapomnienia Strach
		79, // Opaska Zapomnienia Mroczna Ochrona
		94, // Opaska Zapomnienia Blogoslawienstwo
	};

	// LPP weapons. bBand: 1 = his "5-29 lvl", 2 = "30 lvl+", 3 = "65 lvl+";
	// bLevel is the family's level limit in this world; bOnlyOne marks the
	// weapons of level 15 and 20, of which a box keeps one.
	struct TPlayerBotLppWeapon { DWORD dwBaseVnum; BYTE bBand; BYTE bLevel; BYTE bOnlyOne; };
	const TPlayerBotLppWeapon PLAYERBOT_LPP_WEAPONS[] = {
		{ 40, 1, 15, 1 }, // Stozkowy Miecz
		{ 50, 1, 20, 1 }, // Szeroki Miecz
		{ 4010, 1, 15, 1 }, // Dziewiec Ostrzy
		{ 4020, 1, 25, 0 }, // Krotki Noz
		{ 2030, 1, 15, 1 }, // Bojowy Luk
		{ 2040, 1, 20, 1 }, // Dlugi Luk Jezdzcy
		{ 3030, 1, 15, 1 }, // Pajecza Wlocznia
		{ 3040, 1, 20, 1 }, // Gizarma
		{ 5030, 1, 32, 0 }, // Jadeitowy Dzwon
		{ 5040, 1, 36, 0 }, // Dzwon Fontanny
		{ 7030, 1, 15, 1 }, // Zurawi Wachlarz
		{ 7040, 1, 20, 1 }, // Pawi Wachlarz.
		{ 290, 2, 30, 0 }, // Miecz Pelni Ksiezyca
		{ 3210, 2, 30, 0 }, // Ostrze Z Czerw. Stali
		{ 2150, 2, 30, 0 }, // Luk Z Rogu Jelenia
		{ 1170, 2, 30, 0 }, // Kozik Czar. Lis.
		{ 5110, 2, 30, 0 }, // Antyczny Dzwon
		{ 7160, 2, 30, 0 }, // Wachlarz Jes. Wiatru
		{ 180, 3, 75, 0 }, // Zatruty miecz
		{ 190, 3, 75, 0 }, // Lwi Miecz
		{ 1130, 3, 75, 0 }, // Skrzydla Demona Chakr.
		{ 2170, 3, 75, 0 }, // Stalowy Luk Kruka
		{ 3160, 3, 75, 0 }, // Miecz Zalu
		{ 5120, 3, 75, 0 }, // Bambusowy Dzwon
		{ 7180, 3, 75, 0 }, // Wachlarz 8 Trigramow
	};

	// The shields of level 61 with resistances, and the armours his "70 lvl" names
	// (level 66 in this world): what a box keeps an armour or a shield for.
	const DWORD PLAYERBOT_LPP_TARGET_SHIELDS[] = {
		13060, // Sokola Tarcza
		13080, // Tarcza Tygrysa
		13100, // Lwia Tarcza
		13120, // Tarcza Smoka
	};
	const DWORD PLAYERBOT_LPP_TARGET_ARMOURS[] = {
		11290, // Zbroje z Czarnej Stali
		11690, // Zbroja Plyt. Czarnej Magii
		11490, // Ubranie Czarnego Wiatru
		11890, // Czarna Szata
	};
}

#endif
