#ifndef __INC_METIN2_PLAYERBOT_SHOUTER_LINES_H__
#define __INC_METIN2_PLAYERBOT_SHOUTER_LINES_H__

// MT2009_PLUS_SHOUTERS_V1: what the three shouters of the first villages
// (playerbot_shouters.h) call out on the shout channel ("wolaj").
//
// JAK DOPISAC LINIE: jedna linia = jeden napis w cudzyslowie zakonczony
// przecinkiem, w dowolnym miejscu listy ponizej. Plik jest w UTF-8, polskie
// litery piszemy normalnie (rdzen sam zamienia je na CP1250 klienta);
// cudzyslow wewnatrz napisu to \" , a ukosnik to \\ . Dlugosc do ok. 200
// znakow. Po zmianie: przebudowac obraz gry (docker compose build game).
// Bot nie powtarza tej samej linii dwa razy pod rzad.
//
// HOW TO ADD A LINE: one quoted string with a comma after it, anywhere in the
// list below. UTF-8, Polish letters as they are (converted to the client's
// CP1250 at runtime). Rebuild the game image afterwards.

namespace
{
	const char* const PLAYERBOT_SHOUTER_LINES[] =
	{
		"mam dojebana aplikację za 700zl w tokenach",
		"Zniszczę Cię",
		"Dojadę Cię, frajerskie zachowanie",
		"Moja appka to robi, nie musisz tego robić",
		"wydałem 700 zł na tokeny i dalej nie wiem co robię",
		"700 zł w tokenach poszło, appka nadal nie odpala",
		"kto pożyczy tokeny? moja appka zjadła wszystkie",
		"appka kosztowała mnie więcej niż komputer, na którym nie działa",
		"700 zł w tokenach, a jedyne co działa to przycisk zamknij",
		"subskrypcja się skończyła, appka też",
		"zbieram na tokeny, rzucajcie yang",
		"zrobiłem appkę vibe codingiem, vibe jest, appki nie ma",
		"napisałem do AI „zrób żeby działało” i nie zrobiło",
		"nie umiem programować, ale mam appkę za 700 zł",
		"AI napisało 5000 linii, ja przeczytałem 2",
		"ktoś wie jak przeczytać error? appka pisze po chińsku",
		"naprawiłem jeden błąd, wyskoczyło siedem nowych",
		"mój kod jest na licencji „nie dotykać, bo się rozsypie”",
		"moja appka robi wszystko, tylko nie to co trzeba",
		"appka działa, ale tylko u mnie i tylko we wtorki",
		"wersja 2.0 appki: teraz nie działa szybciej",
		"appka ma nowy update, dalej nic nie robi, ale ładniej",
		"zgłoście buga, appka go zignoruje",
		"appka zrobiła 346 fryzur, a miała zrobić jedną rzecz",
		"moja appka zniszczy wasz serwer (jak się uruchomi)",
		"dojadę was wszystkich, jak tylko appka się skompiluje",
		"frajerskie zachowanie grać bez appki za 700 zł",
		"nie muszę grać, moja appka gra za mnie (nie gra)",
		"mam appkę, ale nie mam pomysłu co ma robić",
	};
	const int PLAYERBOT_SHOUTER_LINE_COUNT =
			(int)(sizeof(PLAYERBOT_SHOUTER_LINES) / sizeof(PLAYERBOT_SHOUTER_LINES[0]));

	// MT2009_PLUS_MEDAL_SHOUTERS_V1: what Tieru, Tiieru and Tiiieru - the
	// medal droppers among the krzykacze (playerbot_shouters.h) - call out on
	// the shout channel. Dopisuje sie tak samo jak wyzej.
	const char* const PLAYERBOT_MEDAL_SHOUTER_LINES[] =
	{
		"Zakaz wchodzenia do lochu małp, wszystkie medale moje, loch małp nie jest już open source!!",
		"Nie kraść moich medali! Wszystkie yang dla mnie i tylko dla mnie!",
		"Każdy inny dropek medali to niemoralna suka.",
		"Oddawaj moje złoto złodzieju!",
		"Medale są chronione prawem autorskim, drop bez mojej licencji zgłaszam do GM-a",
		"Małpy pracują teraz dla mnie, płacę im bananami, a medale zostają u mnie",
		"Ceny medali ustalam ja i tylko na moim straganie, reszta to podróbki",
		"Widziałem cię w lochu małp. Policzyłem medale. Brakuje trzech.",
		"Loch małp zamknięty na inwentaryzację medali, otwarte tylko dla mnie",
		"Król małp to mój pracownik miesiąca, premia w bananach",
		"Każde yang z lochu małp ma mój podpis, oddawać do właściciela",
		"Wpuściłem cię raz do lochu małp i do dziś liczę straty",
		"Pożyczyłem ci raz medal i do dziś nie oddałeś, złodzieju",
		"Każdy medal sprzedany poza moim straganem to medal ukradziony mnie",
		"Inni dropkowie medali to banda oszustów, kupujcie tylko u oryginału",
		"Kto kupuje medale u konkurencji, ten wspiera przestępczość zorganizowaną",
		"Medal z mojego straganu jest prawdziwy, reszta to chińska podróbka",
		"Pytałeś o rabat na medale? Rabat dostała tylko moja małpa",
		"Twój koń jeździ na moich medalach i nawet nie podziękował",
		"Raz wszedłem z kimś do lochu małp. Wyszedłem sam. Z medalami.",
		"Konkurencja sprzedaje medale taniej, bo kradnie je z mojego lochu",
		"Nie ufajcie innym dropkom, ja przynajmniej oszukuję was uczciwie",
	};
	const int PLAYERBOT_MEDAL_SHOUTER_LINE_COUNT =
			(int)(sizeof(PLAYERBOT_MEDAL_SHOUTER_LINES) / sizeof(PLAYERBOT_MEDAL_SHOUTER_LINES[0]));
}

#endif
