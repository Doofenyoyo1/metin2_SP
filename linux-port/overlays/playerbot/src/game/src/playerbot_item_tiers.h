// Rendered by linux-port/overlays/playerbot/tools/generate_iwakura_tiers.py from Iwakura's
// tier list (data/iwakura_tiery.txt, 16 September, the soul stones added on
// 19 September). Do not edit by hand.
//
// Every family of bracelets, earrings, necklaces, boots and weapons, and every
// bonus line, rated 1 (bardzo zly) to 6 (wspanialy) - once for PvE, once for
// PvP - with the "+1 dla Wojownika" notes as job masks. Body armour, helmets
// and shields are judged by level and bonuses, not by name, so they are not
// here. The PvE column steers the hunting set today; the PvP column waits for
// the second set ("na przyszlosc pod posiadanie przez boty dwoch setow").
//
// The soul stones (Kamienie Duszy, the ITEM_METIN stones a socket takes) are
// his rule rather than a nudge: a stone of a banned grade (+0, +1 and +2 on
// his list) never goes into a weapon or an armour, only the stones listed
// here may, and the four class stones only into a PvP weapon.
#ifndef __INC_METIN2_PLAYERBOT_ITEM_TIERS_H__
#define __INC_METIN2_PLAYERBOT_ITEM_TIERS_H__

namespace
{
	const int PLAYERBOT_TIER_MIN = 1;
	const int PLAYERBOT_TIER_MAX = 6;

	// The +0 vnum of the family; the rest of the family is base + refine.
	struct TPlayerBotItemTier { DWORD dwBaseVnum; BYTE bPve; BYTE bPvp; BYTE bPveJobs; BYTE bPvpJobs; };
	const TPlayerBotItemTier PLAYERBOT_ITEM_TIERS[] = {
		{ 10, 1, 1, 0, 0 }, // Miecz
		{ 20, 1, 1, 0, 0 }, // Dlugi Miecz
		{ 30, 1, 1, 0, 0 }, // Sejmitar
		{ 40, 1, 1, 0, 0 }, // Stozkowy Miecz
		{ 50, 1, 1, 0, 0 }, // Szeroki Miecz
		{ 60, 1, 1, 0, 0 }, // Srebrny Miecz
		{ 70, 1, 1, 0, 0 }, // Miecz Storczykowy
		{ 80, 1, 1, 0, 0 }, // Miecz Poltorareczny
		{ 90, 1, 1, 0, 0 }, // Miecz Barbarzyncy
		{ 100, 1, 1, 0, 0 }, // Krwawy Miecz
		{ 110, 1, 1, 0, 0 }, // Wielki Miecz
		{ 120, 1, 1, 0, 0 }, // Miecz Lat. Maga
		{ 130, 1, 1, 0, 0 }, // Pol-ksiezycowy Miecz
		{ 140, 2, 5, 0, 0 }, // Miecz Bojowy
		{ 150, 2, 5, 0, 0 }, // Miecz Szponu Ducha
		{ 160, 2, 3, 0, 0 }, // Miecz Nimfy
		{ 170, 2, 2, 0, 0 }, // Miecz Zadlowy
		{ 180, 6, 5, 0, 0 }, // Zatruty miecz
		{ 190, 5, 4, 0, 0 }, // Lwi Miecz
		{ 200, 4, 6, 0, 0 }, // Brzegowe Ostrze
		{ 240, 2, 5, 0, 0 }, // Miecz Egzorcysty
		{ 250, 2, 2, 0, 0 }, // Demoniczne Ostrze
		{ 270, 3, 6, 0, 0 }, // Miecz Trytona
		{ 280, 2, 6, 0, 0 }, // Swiety Miecz
		{ 290, 5, 4, 0, 0 }, // Miecz Pelni Ksiezyca
		{ 1000, 1, 1, 0, 0 }, // Sztylet
		{ 1010, 1, 1, 0, 0 }, // Sztylet Kobry
		{ 1020, 1, 1, 0, 0 }, // Sztylet Nozycowy
		{ 1030, 1, 1, 0, 0 }, // Noz Szczescia
		{ 1040, 1, 1, 0, 0 }, // Ukaszenie Kota
		{ 1050, 1, 1, 0, 0 }, // Twarz Diabla
		{ 1060, 1, 1, 0, 0 }, // Sztylet Piesci Diab.
		{ 1070, 1, 1, 0, 0 }, // Krwawy Sztylet
		{ 1080, 1, 1, 0, 0 }, // Zebrowy Noz
		{ 1090, 1, 1, 0, 0 }, // Chakram
		{ 1100, 3, 5, 0, 0 }, // Smoczy Noz
		{ 1110, 3, 3, 0, 0 }, // Noz Blyskawicy
		{ 1120, 4, 2, 0, 0 }, // Noz Siamese
		{ 1130, 6, 4, 0, 0 }, // Skrzydla Demona Chakr.
		{ 1170, 5, 3, 0, 0 }, // Kozik Czar. Lis.
		{ 2000, 1, 1, 0, 0 }, // Luk
		{ 2010, 1, 1, 0, 0 }, // Dlugi Luk
		{ 2020, 1, 1, 0, 0 }, // Kompozytowy Luk
		{ 2030, 1, 1, 0, 0 }, // Bojowy Luk
		{ 2040, 1, 1, 0, 0 }, // Dlugi Luk Jezdzcy
		{ 2050, 1, 1, 0, 0 }, // Bojowy Luk Jezdzcy
		{ 2060, 1, 1, 0, 0 }, // Miedziany Luk
		{ 2070, 1, 1, 0, 0 }, // Luk Czarnych Ruin
		{ 2080, 1, 1, 0, 0 }, // Luk Czerwonego Oka
		{ 2090, 1, 1, 0, 0 }, // Luk Kolczastego Lis.
		{ 2100, 1, 1, 0, 0 }, // Luk z rogu byka
		{ 2110, 1, 1, 0, 0 }, // Luk Jednorozca
		{ 2120, 1, 1, 0, 0 }, // Olbrz. Skrzydl. Luk
		{ 2130, 2, 3, 0, 0 }, // Boski Luk Moreli
		{ 2140, 2, 5, 0, 0 }, // Olbrz. Luk Zolt. Smoka
		{ 2150, 4, 4, 0, 0 }, // Luk Z Rogu Jelenia
		{ 2160, 3, 6, 0, 0 }, // Olbrz. Luk Diabla
		{ 2170, 6, 4, 0, 0 }, // Stalowy Luk Kruka
		{ 2180, 1, 2, 0, 0 }, // Luk Niebieskiego Smoka
		{ 3000, 1, 1, 0, 0 }, // Glewia
		{ 3010, 1, 1, 0, 0 }, // Wlocznia
		{ 3020, 1, 1, 0, 0 }, // Gilotynowe Ostrze
		{ 3030, 1, 1, 0, 0 }, // Pajecza Wlocznia
		{ 3040, 1, 1, 0, 0 }, // Gizarma
		{ 3050, 1, 1, 0, 0 }, // Kosa Bojowa
		{ 3060, 1, 1, 0, 0 }, // Trojzab
		{ 3070, 1, 1, 0, 0 }, // Halabarda
		{ 3080, 1, 1, 0, 0 }, // Olbrz. Topor
		{ 3090, 1, 1, 0, 0 }, // Lodowa Iglica
		{ 3100, 1, 1, 0, 0 }, // Miecz Dwunastu Duchow
		{ 3110, 1, 1, 0, 0 }, // Ostrze Zbawienia
		{ 3120, 1, 1, 0, 0 }, // Zabojca Lwow
		{ 3130, 3, 5, 0, 0 }, // Partyzana
		{ 3140, 3, 4, 0, 0 }, // Magnetyczne Ostrze
		{ 3150, 2, 4, 0, 0 }, // Zlodziej Dusz
		{ 3160, 6, 3, 0, 0 }, // Miecz Zalu
		{ 3180, 2, 6, 0, 0 }, // Pogromca Nieb. Smoka
		{ 3210, 5, 3, 0, 0 }, // Ostrze Z Czerw. Stali
		{ 4000, 1, 1, 0, 0 }, // Amija
		{ 4010, 1, 1, 0, 0 }, // Dziewiec Ostrzy
		{ 4020, 1, 1, 0, 0 }, // Krotki Noz
		{ 4040, 3, 6, 0, 0 }, // Bezduszny Noz
		{ 5000, 1, 1, 0, 0 }, // Miedziany Dzwon
		{ 5010, 1, 1, 0, 0 }, // Srebrny Dzwon
		{ 5020, 1, 1, 0, 0 }, // Zloty Dzwon
		{ 5030, 1, 1, 0, 0 }, // Jadeitowy Dzwon
		{ 5040, 1, 1, 0, 0 }, // Dzwon Fontanny
		{ 5050, 1, 1, 0, 0 }, // Morelowy Dzwon
		{ 5060, 1, 1, 0, 0 }, // Magiczny Dzwon
		{ 5070, 1, 1, 0, 0 }, // Zloty Robaczy Dzwon
		{ 5080, 1, 1, 0, 0 }, // Stalowy Robaczy Dzwon
		{ 5090, 3, 3, 0, 0 }, // Dzwon Burzowego Ptaka
		{ 5100, 3, 5, 0, 0 }, // Dzwon Nieba I Ziemi
		{ 5110, 5, 3, 0, 0 }, // Antyczny Dzwon
		{ 5120, 6, 4, 0, 0 }, // Bambusowy Dzwon
		{ 5130, 2, 2, 0, 0 }, // Dzwon Smierci
		{ 5330, 3, 6, 0, 0 }, // Dzwon Szczeki Smoka
		{ 7000, 1, 1, 0, 0 }, // Wachlarz
		{ 7010, 1, 1, 0, 0 }, // Zelazny Wachlarz
		{ 7020, 1, 1, 0, 0 }, // Wachlarz Czarn. Tygr.
		{ 7030, 1, 1, 0, 0 }, // Zurawi Wachlarz
		{ 7040, 1, 1, 0, 0 }, // Pawi Wachlarz
		{ 7050, 1, 1, 0, 0 }, // Wodny Wachlarz
		{ 7060, 1, 1, 0, 0 }, // Kamienny Wachlarz
		{ 7070, 1, 1, 0, 0 }, // Oceaniczny Wachlarz
		{ 7080, 1, 1, 0, 0 }, // Zadlowy Wachlarz
		{ 7090, 1, 1, 0, 0 }, // Wachlarz Feniksa
		{ 7100, 1, 1, 0, 0 }, // Potrojny Wachlarz
		{ 7110, 1, 1, 0, 0 }, // Brwisty Wachlarz
		{ 7120, 1, 1, 0, 0 }, // Czarny Slon. Wachlarz
		{ 7130, 3, 2, 0, 0 }, // Niebian. Ptasi Wachl.
		{ 7140, 3, 5, 0, 0 }, // Wachlarz Zbawienia
		{ 7150, 2, 2, 0, 0 }, // Ekstazyjny Wachlarz
		{ 7160, 4, 3, 0, 0 }, // Wachlarz Jes. Wiatru
		{ 7180, 5, 4, 0, 0 }, // Wachlarz 8 Trigramow
		{ 7190, 3, 6, 0, 0 }, // Wachlarz Demona
		{ 14000, 1, 1, 0, 0 }, // Drewniana Bransoleta
		{ 14020, 1, 1, 0, 0 }, // Miedziana Bransoleta
		{ 14040, 5, 4, 0, 0 }, // Srebrna Bransoleta
		{ 14060, 4, 2, 0, 0 }, // Zlota Bransoleta
		{ 14080, 2, 2, 0, 0 }, // Jadeitowa Bransoleta
		{ 14100, 5, 2, 0, 0 }, // Ebonitowa Bransoleta
		{ 14120, 1, 1, 0, 0 }, // Perlowa Bransoleta
		{ 14140, 5, 5, 0, 0 }, // Bransol. Z Bial. Zlota
		{ 14160, 4, 3, 0, 0 }, // Krysztalowa Bransoleta
		{ 14180, 2, 1, 0, 0 }, // Ametystowa Bransoleta
		{ 14200, 6, 6, 0, 0 }, // Bransol. Z Niebian.Lez
		{ 15000, 2, 2, 0, 0 }, // Skorzane Buty
		{ 15020, 2, 1, 0, 0 }, // Bambusowe Buty
		{ 15040, 3, 3, 0, 0 }, // Drewniane Buty
		{ 15060, 3, 3, 0, 0 }, // Buty Wyszywane Zlotem
		{ 15080, 5, 5, 0, 0 }, // Skorzane Kozaki
		{ 15100, 2, 2, 0, 0 }, // Zlote Buty
		{ 15120, 4, 3, 0, 0 }, // Buty Z Brazu
		{ 15140, 5, 1, 0, 0 }, // Jadeitowe Buty
		{ 15160, 3, 5, 0, 0 }, // Ekstazyjne Buty
		{ 15180, 3, 4, 0, 0 }, // Deszczowe Buty
		{ 15200, 6, 6, 0, 0 }, // Buty Feniksa
		{ 15220, 3, 6, 0, 0 }, // Buty Ognistego Ptaka
		{ 16000, 1, 1, 0, 0 }, // Drewniany Naszyjnik
		{ 16020, 1, 1, 0, 0 }, // Miedziany Naszyjnik
		{ 16040, 3, 2, 0, 0 }, // Srebrny Naszyjnik
		{ 16060, 3, 4, 0, 0 }, // Zloty Naszyjnik
		{ 16080, 4, 1, (1 << JOB_SURA) | (1 << JOB_SHAMAN), 0 }, // Jadeitowy Naszyjnik
		{ 16100, 3, 3, 0, 0 }, // Ebonitowy Naszyjnik
		{ 16120, 4, 4, (1 << JOB_SURA) | (1 << JOB_SHAMAN), (1 << JOB_SURA) | (1 << JOB_SHAMAN) }, // Perlowy Naszyjnik
		{ 16140, 4, 4, (1 << JOB_WARRIOR), (1 << JOB_WARRIOR) }, // Naszyj. Z Bial. Zlota
		{ 16160, 4, 4, (1 << JOB_WARRIOR) | (1 << JOB_ASSASSIN), (1 << JOB_WARRIOR) | (1 << JOB_ASSASSIN) }, // Krysztalowy Naszyjnik
		{ 16180, 1, 1, 0, 0 }, // Ametystowy Naszyjnik
		{ 16200, 6, 6, 0, 0 }, // Naszyj. Z Niebian.Lez
		{ 17000, 3, 3, 0, 0 }, // Drewniane Kolczyki
		{ 17020, 3, 3, 0, 0 }, // Miedziane Kolczyki
		{ 17040, 3, 3, 0, 0 }, // Srebrne Kolczyki
		{ 17060, 3, 3, 0, 0 }, // Zlote Kolczyki
		{ 17080, 3, 4, 0, 0 }, // Jadeitowe Kolczyki
		{ 17100, 6, 6, 0, 0 }, // Ebonitowe Kolczyki
		{ 17120, 5, 4, 0, 0 }, // Perlowe Kolczyki
		{ 17140, 4, 4, 0, 0 }, // Kolczyki Z Bial. Zlota
		{ 17160, 3, 5, 0, 0 }, // Krysztalowe Kolczyki
		{ 17180, 3, 5, 0, 0 }, // Ametystowe Kolczyki
		{ 17200, 6, 6, 0, 0 }, // Kolczyki Z Niebian.Lez
	};

	struct TPlayerBotBonusTier { BYTE bApply; BYTE bPve; BYTE bPvp; BYTE bPveJobs; BYTE bPvpJobs; };
	const TPlayerBotBonusTier PLAYERBOT_BONUS_TIERS[] = {
		{ APPLY_ATTBONUS_ANIMAL, 3, 1, 0, 0 }, // Silny na zwierzeta
		{ APPLY_ATTBONUS_DEVIL, 4, 1, 0, 0 }, // Silny przeciwko diablom
		{ APPLY_ATTBONUS_HUMAN, 1, 6, 0, 0 }, // Silny przeciwko ludziom
		{ APPLY_ATTBONUS_MILGYO, 3, 1, 0, 0 }, // Silny przeciwko mistykom
		{ APPLY_ATTBONUS_ORC, 4, 1, 0, 0 }, // Silny na orki
		{ APPLY_ATTBONUS_UNDEAD, 5, 1, 0, 0 }, // Silny na nieumarle
		{ APPLY_ATT_GRADE_BONUS, 6, 6, 0, 0 }, // Wartosc ataku
		{ APPLY_ATT_SPEED, 5, 3, 0, 0 }, // Szybkosc ataku
		{ APPLY_BLOCK, 6, 4, 0, 0 }, // Szansa na blok ciosu
		{ APPLY_CAST_SPEED, 3, 6, 0, 0 }, // Szybkosc zaklecia
		{ APPLY_CON, 3, 3, 0, 0 }, // Witalnosc
		{ APPLY_CRITICAL_PCT, 6, 6, 0, 0 }, // Szansa na cios krytyczny
		{ APPLY_DEX, 5, 5, (1 << JOB_WARRIOR) | (1 << JOB_ASSASSIN), (1 << JOB_WARRIOR) | (1 << JOB_ASSASSIN) }, // Zrecznosc
		{ APPLY_DODGE, 5, 4, 0, 0 }, // Szansa na unikniecie strzaly
		{ APPLY_GOLD_DOUBLE_BONUS, 6, 1, 0, 0 }, // Szansa na podwojna ilosc yang
		{ APPLY_HP_REGEN, 3, 2, 0, 0 }, // Regeneracja Mikstur PZ
		{ APPLY_IMMUNE_SLOW, 2, 2, 0, 0 }, // Niewrazliwy na spowolnienie
		{ APPLY_IMMUNE_STUN, 6, 6, 0, 0 }, // Niewrazliwy na omdlenie
		{ APPLY_INT, 5, 5, (1 << JOB_SURA) | (1 << JOB_SHAMAN), (1 << JOB_SURA) | (1 << JOB_SHAMAN) }, // Inteligencja
		{ APPLY_MALL_EXPBONUS, 4, 1, 0, 0 }, // Punkty doswiadczenia +%
		{ APPLY_MANA_BURN_PCT, 2, 1, 0, 0 }, // Szansa na kradziez PE
		{ APPLY_MAX_HP, 6, 6, 0, 0 }, // Maks. PZ
		{ APPLY_MAX_SP, 2, 2, 0, 0 }, // Maks. PE
		{ APPLY_MAX_STAMINA, 1, 1, 0, 0 }, // Maks. Stamina
		{ APPLY_MOV_SPEED, 4, 4, 0, 0 }, // Szybkosc ruchu
		{ APPLY_PENETRATE_PCT, 4, 6, 0, 0 }, // Szansa na przeszywajace uderzenie
		{ APPLY_POISON_PCT, 6, 5, 0, 0 }, // Szansa na otrucie
		{ APPLY_POISON_REDUCE, 2, 2, 0, 0 }, // Odpornosc na trucizny
		{ APPLY_REFLECT_MELEE, 3, 3, 0, 0 }, // Szansa na dobicie ciosu
		{ APPLY_RESIST_BELL, 1, 5, 0, 0 }, // Odpornosc na dzwony
		{ APPLY_RESIST_BOW, 4, 6, 0, 0 }, // Odpornosc na strzaly
		{ APPLY_RESIST_DAGGER, 1, 6, 0, 0 }, // Odpornosc na sztylety
		{ APPLY_RESIST_FAN, 1, 5, 0, 0 }, // Odpornosc na wachlarze
		{ APPLY_RESIST_MAGIC, 3, 6, 0, 0 }, // Odpornosc na magie
		{ APPLY_RESIST_SWORD, 1, 6, 0, 0 }, // Odpornosc na miecze
		{ APPLY_RESIST_TWOHAND, 1, 6, 0, 0 }, // Odpornosc na bron dwureczna
		{ APPLY_SLOW_PCT, 1, 2, 0, 0 }, // Szansa na spowolnienie
		{ APPLY_SP_REGEN, 2, 1, 0, 0 }, // Regeneracja Mikstur PE
		{ APPLY_STEAL_HP, 6, 2, 0, 0 }, // x% obrazen dodanych do PZ
		{ APPLY_STEAL_SP, 2, 1, 0, 0 }, // x% obrazen dodanych do PE
		{ APPLY_STR, 5, 5, (1 << JOB_WARRIOR), (1 << JOB_WARRIOR) }, // Sila
		{ APPLY_STUN_PCT, 5, 5, 0, 0 }, // Szansa na omdlenie
#if defined(PLAYERBOT_ENGINE_MT2009)
		{ APPLY_REFLECT_ARROW, 3, 3, 0, 0 }, // Szansa na odbicie pocisku
#endif
#if defined(PLAYERBOT_ENGINE_MT2009)
		{ APPLY_SKILL_DURATION, 3, 3, 0, 0 }, // Czas trwania umiejetnosci
#endif
#if defined(PLAYERBOT_ENGINE_MT2009)
		{ APPLY_ST_REGEN, 1, 1, 0, 0 }, // Regeneracja ST (staminy)
#endif
	};

	// Iwakura's soul stones, by vnum (28000 + grade * 100 + kind). A stone of a
	// grade under PLAYERBOT_SOUL_STONE_MIN_GRADE, or one not on this list, is
	// never put into a weapon or an armour; a PvP-only stone never into the
	// hunting set.
	const int PLAYERBOT_SOUL_STONE_MIN_GRADE = 3;
	struct TPlayerBotSoulStoneTier { DWORD dwVnum; BYTE bPve; BYTE bPvp; bool bPvpOnly; };
	const TPlayerBotSoulStoneTier PLAYERBOT_SOUL_STONE_TIERS[] = {
		{ 28331, 4, 3, false }, // Kamien Duszy Smierci +3
		{ 28337, 5, 1, false }, // Kamien Duszy Potwora +3
		{ 28338, 3, 3, false }, // Kamien Duszy Uchylenia +3
		{ 28341, 4, 3, false }, // Kamien Duszy Witalnosci +3
		{ 28342, 5, 1, false }, // Kamien Duszy Obrony +3
		{ 28343, 4, 4, false }, // Kamien Duszy Przyspieszenia +3
		{ 28430, 3, 6, false }, // Kamien Duszy Penetracji +4
		{ 28431, 6, 3, false }, // Kamien Duszy Smierci +4
		{ 28432, 2, 4, false }, // Kamien Duszy Powtorki +4
		{ 28433, 1, 6, true }, // Kamien Duszy Wojownika +4
		{ 28434, 1, 6, true }, // Kamien Duszy Ninja +4
		{ 28435, 1, 6, true }, // Kamien Duszy Sury +4
		{ 28436, 1, 5, true }, // Kamien Duszy Szamana +4
		{ 28437, 6, 1, false }, // Kamien Duszy Potwora +4
		{ 28438, 4, 4, false }, // Kamien Duszy Uchylenia +4
		{ 28439, 6, 6, false }, // Kamien Duszy Uniku +4
		{ 28440, 1, 1, false }, // Kamien Duszy Magii +4
		{ 28441, 6, 6, false }, // Kamien Duszy Witalnosci +4
		{ 28442, 5, 1, false }, // Kamien Duszy Obrony +4
		{ 28443, 6, 4, false }, // Kamien Duszy Przyspieszenia +4
	};

	const TPlayerBotSoulStoneTier* FindPlayerBotSoulStoneTier(DWORD vnum)
	{
		if ((int)((vnum / 100) % 10) < PLAYERBOT_SOUL_STONE_MIN_GRADE)
			return NULL;
		for (size_t i = 0; i < sizeof(PLAYERBOT_SOUL_STONE_TIERS) / sizeof(PLAYERBOT_SOUL_STONE_TIERS[0]); ++i)
			if (PLAYERBOT_SOUL_STONE_TIERS[i].dwVnum == vnum)
				return &PLAYERBOT_SOUL_STONE_TIERS[i];
		return NULL;
	}

	// The stone's tier for the set it would go into, or 0 when it may not go
	// into that set at all: a banned grade, a stone off his list, or a PvP-only
	// stone asked about for the hunting set.
	int GetPlayerBotSoulStoneTier(DWORD vnum, bool pvp)
	{
		const TPlayerBotSoulStoneTier* row = FindPlayerBotSoulStoneTier(vnum);
		if (!row || (row->bPvpOnly && !pvp))
			return 0;
		return pvp ? row->bPvp : row->bPve;
	}

	int PlayerBotTierWithJob(int tier, BYTE jobs, int job)
	{
		if (tier <= 0)
			return 0;
		if (job >= 0 && job < 4 && (jobs & (1 << job)) != 0)
			++tier;
		return tier > PLAYERBOT_TIER_MAX ? PLAYERBOT_TIER_MAX : tier;
	}

	// The family's tier for this job, or 0 when his list does not carry it.
	// `job` is CHARACTER::GetJob(), 0..3; -1 asks without the job notes.
	int GetPlayerBotItemTier(DWORD baseVnum, int job, bool pvp)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_ITEM_TIERS) / sizeof(PLAYERBOT_ITEM_TIERS[0]); ++i)
			if (PLAYERBOT_ITEM_TIERS[i].dwBaseVnum == baseVnum)
				return pvp ? PlayerBotTierWithJob(PLAYERBOT_ITEM_TIERS[i].bPvp, PLAYERBOT_ITEM_TIERS[i].bPvpJobs, job)
						: PlayerBotTierWithJob(PLAYERBOT_ITEM_TIERS[i].bPve, PLAYERBOT_ITEM_TIERS[i].bPveJobs, job);
		return 0;
	}

	// The bonus line's tier for this job, or 0 when his list does not name it.
	int GetPlayerBotBonusTier(BYTE apply, int job, bool pvp)
	{
		for (size_t i = 0; i < sizeof(PLAYERBOT_BONUS_TIERS) / sizeof(PLAYERBOT_BONUS_TIERS[0]); ++i)
			if (PLAYERBOT_BONUS_TIERS[i].bApply == apply)
				return pvp ? PlayerBotTierWithJob(PLAYERBOT_BONUS_TIERS[i].bPvp, PLAYERBOT_BONUS_TIERS[i].bPvpJobs, job)
						: PlayerBotTierWithJob(PLAYERBOT_BONUS_TIERS[i].bPve, PLAYERBOT_BONUS_TIERS[i].bPveJobs, job);
		return 0;
	}
}

#endif
