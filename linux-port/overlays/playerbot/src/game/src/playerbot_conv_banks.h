#ifndef __INC_METIN2_PLAYERBOT_CONV_BANKS_H__
#define __INC_METIN2_PLAYERBOT_CONV_BANKS_H__

// MT2009_PLUS_BOT_CHAT_V2 - the bigger bank of things a bot says (pure).
//
// The owner, 4 October: "chce zeby rozumialy wiecej ... wieksza baza
// wiadomosci". What is here is words only - jokes, the cities a bot comes
// from, the answers to "jestes botem?", to begging, to an insult in each of
// the five voices, and the places a bot names when asked where the Metins of
// a level stand or where to exp at one. The generators that choose among them
// are in playerbot_conv_generator.h; the facts they are grounded in (the
// bot's level, its map, its gold, the quarrel over its spot) come from the
// snapshot, never from here.
//
// The lines are written the way the bots write on the shout channel: casual,
// mostly without capitals (the typing hand, playerbot_conv_style.h, lowers
// the rest), Polish without its diacritics, the game's slang (exp, metki, pt,
// dropi, M1, wojek, sura, k, SM).

#include "playerbot_conv_say.h"

namespace playerbot_conv
{
	// ------------------------------------------------------------- jokes

	inline const char* const* JokeBank(size_t& count)
	{
		static const char* const k[] = {
			"przychodzi gracz do kowala: +9 prosze. kowal: a mial pan kiedys +9? gracz: nie. kowal: no i nie bedzie xd",
			"czemu metin nie chodzi na imprezy? bo i tak wszyscy go tam bija :D",
			"ilu ninja trzeba, zeby wkrecic zarowke? nie wiadomo, nikt ich nigdy nie widzial :P",
			"kowal do gracza: mam dobra i zla wiadomosc. dobra - bron ci zelzala. zla - bo jej juz nie ma xd",
			"jak sie nazywa wojownik bez potek? swietej pamieci wojownik :D",
			"spotyka sie dwoch ninja. jeden: widziales mnie wczoraj? drugi: nie. pierwszy: no i o to chodzi xd",
			"przychodzi gracz do sklepu: poprosze mikstury. ile? wszystkie, ide na pustynie xd",
			"co mowi zero do osemki? fajny pasek :D",
			"pacjent do lekarza: panie doktorze, wszyscy mnie ignoruja. lekarz: nastepny prosze! :D",
			"wchodzi kon do baru. barman: czemu taka dluga mina? :D",
			"ile graczy trzeba, zeby zbic metina? dwoch. jeden bije, drugi ksuje xd",
			"zona do meza: albo ja, albo metin. maz: kochanie, a ktory metin? :P",
			"najkrotszy kawal swiata: kowal dal +9 za pierwszym razem xd",
			"skad wiesz, ze ktos gra wojem? spokojnie, sam ci powie :D",
			"sprzedam miecz +0, prawie nowy, nigdy nie byl u kowala, wiec jeszcze istnieje xd",
			"pajak do pajaka w lochu: ale dzis tlok, nawet nie ma gdzie pajeczyny powiesic xd",
			"czemu szaman ma tylu znajomych? bo kazdego zbuffuje za darmo :D",
			"idzie dwoch mysliwych przez las. jeden: o, slady niedzwiedzia. drugi: to ty zobacz dokad poszedl, a ja sprawdze skad przyszedl xd",
			"mama pyta jasia: czemu masz jedynke z matmy? bo pani pytala ile to 2+2, a ja ze +9 to i tak sie spali :D",
			"przychodzi sura do lekarza. lekarz: co panu dolega? sura: nic, wpadlem tylko troche hp pozyczyc :P",
			"jak sie nazywa kon, ktory nie umie ustac? kon-sternacja :D",
			"czemu orki w dolinie sa takie zle? bo od lat nikt im nie powiedzial ze maja ladne kly xd",
			"wiesz czemu biolog jest zawsze spokojny? bo wie, ze i tak dropnie dopiero za 50 razem xd",
			"przychodzi baba do lekarza, a lekarz tez baba. no i obie stoja :D",
			"co robi kowal na wakacjach? odpoczywa od twoich lez :P",
			"czemu ninja nie placi podatkow? bo urzad skarbowy go nie widzi xd",
			"gracz pyta wrozke: czy wydropie dzis cos dobrego? wrozka: tak. gracz: co? wrozka: zmeczenie :D",
			"co ma wspolnego metin i moj portfel? oba sie rozpadaja jak sie w nie mocno uderzy xd",
			"ojciec do syna: jak bedziesz duzy, to bedziesz lekarzem? syn: nie tato, bede expil po nocach. ojciec: no to tez bez snu, gratuluje :P",
			"czemu szkielety w lochu nie walcza ze soba? bo nie maja do tego serca :D",
			"jak sie nazywa mob, ktory ucieka? lur w odwrotna strone xd",
			"dlaczego szaman nie gra w chowanego? bo zawsze leczy sie w krzakach i swieci na pol mapy :D",
		};
		count = sizeof(k) / sizeof(k[0]);
		return k;
	}

	// ------------------------------------------------------- real life

	// Where a bot "comes from": fixed for the bot (its name hashed), so it
	// never says Krakow today and Gdansk tomorrow.
	inline const char* CityOf(const std::string& botName)
	{
		static const char* const k[] = {
			"z Krakowa", "z Warszawy", "spod Poznania", "z Wroclawia", "z Gdanska", "z Lodzi",
			"z malej wiochy pod Lublinem", "z Katowic", "ze Szczecina", "z Bialegostoku", "z Rzeszowa",
			"z Torunia", "z Opola", "z okolic Kielc", "z Bydgoszczy", "z Olsztyna", "z Radomia",
			"z Czestochowy", "ze Slaska, z Gliwic", "z Gdyni", "z Plocka", "z Zielonej Gory", "z Koszalina",
			"z Tarnowa", "z Legnicy", "z Kalisza", "z Nowego Sacza", "spod Zakopanego",
			"z Anglii, ale jestem polakiem", "z Niemiec, ale z polski pochodze"
		};
		const u32 h = HashStr(botName.c_str(), 0xC17Eu);
		return k[h % (sizeof(k) / sizeof(k[0]))];
	}

	inline int AgeOf(const std::string& botName)
	{
		return 15 + (int)(HashStr(botName.c_str(), 0xA6E5u) % 20);
	}

	// Most of the people who play are boys; so are most of the bots.
	inline bool IsGirl(const std::string& botName)
	{
		return HashStr(botName.c_str(), 0x6E2Du) % 100 < 18;
	}

	// ---------------------------------------------------------- places

	// Where the Metins of a level stand, when the world does not say
	// (IConvWorld::MetinPlaceFor). The stones' spawns of the base maps.
	inline const char* MetinPlaceFallback(int level)
	{
		if (level <= 15) return "na M1, wokol wioski";
		if (level <= 22) return "na M1 i M2, przy drogach";
		if (level <= 30) return "na M2, troche dalej od wioski";
		if (level <= 39) return "w Dolinie Orkow i na Pustyni";
		if (level <= 49) return "na Pustyni Yongbi i w Dolinie";
		if (level <= 59) return "na Sohanie i w Hwangu";
		if (level <= 69) return "na Sohanie i na Ognistej Ziemi";
		if (level <= 79) return "w Lesie Duchow";
		return "w Czerwonym Lesie";
	}

	// Where to exp at a level, the same way.
	inline const char* ExpPlaceFallback(int level)
	{
		if (level < 10) return "na M1, wilki i dziki obok wioski";
		if (level < 20) return "na M1, dalej od wioski, a potem M2";
		if (level < 30) return "na M2";
		if (level < 36) return "na wyspach w Dolinie Orkow albo na Pustyni";
		if (level < 48) return "w Dolinie Orkow albo na Pustyni";
		if (level < 55) return "w Lochu Pajakow albo na Sohanie";
		if (level < 62) return "w Hwangu albo w Lochu Pajakow II";
		if (level < 72) return "w Lesie Duchow";
		if (level < 80) return "na Ognistej Ziemi albo w Czerwonym Lesie";
		return "w Czerwonym Lesie albo w Grocie";
	}
}

#endif
