#ifndef __INC_METIN2_PLAYERBOT_CONV_GENERAL_H__
#define __INC_METIN2_PLAYERBOT_CONV_GENERAL_H__

// PlayerBot Conversation v6 - GENERAL_CONVERSATION (pure).
//
// Everything that is not the game: weather, food, music, dreams, "gdybys
// mogl...". There is no list of questions here. There is:
//   - a TOPIC (from the lexicon: T_WEATHER, T_FOOD, T_MUSIC ...),
//   - a QUESTION TYPE (statement, "lubisz X?", "wolisz X czy Y?", "gdybys",
//     "czego sie boisz", "co myslisz o", a plain open question ...),
//   - the VOICE of the persona (grinder / wanderer / merchant / fighter / social),
//   - the MOOD and the RELATIONSHIP,
// and a generator that combines them. A new topic is one TTopicPack below and
// one lexicon concept + rule; nothing else changes.
//
// Opinions are deterministic per bot and word (OpinionRoll): asked twice
// whether it likes winter, the same bot answers the same way. The bot never
// states facts it cannot know - a factual question it cannot answer gets an
// honest "nie mam pojecia" in its own voice.
//
// Every pack has an English twin (FindTopicPackEn) with the same biases, the
// same number of texts in each field and the favourites in the same order, so
// a bot likes the same things in either language and a draw from either takes
// the same numbers; TopicPackFor() gives the reader's.

#include "playerbot_conv_say.h"

namespace playerbot_conv
{
	struct TTopicPack
	{
		unsigned char topic;
		unsigned char likeBias[V_COUNT];  // chance in % that a voice likes the topic
		const char* react[4];             // to a player's statement
		const char* like[2];              // the bot likes it
		const char* dislike[2];           // the bot does not
		const char* view[V_COUNT];        // the bot's own take ("a ty?", "co myslisz")
		const char* ask[2];               // a question back
		const char* why[2];               // a reason, for "dlaczego?"
		const char* favorites;            // "a|b|c" for "ulubiony X"
	};

	inline const TTopicPack* FindTopicPack(int topic)
	{
		static const TTopicPack kPacks[] = {
			{ T_WEATHER, { 40, 60, 45, 40, 55 },
				{ "No, pogoda dzis jakas dziwna.", "Tez to zauwazylem.", "Pogoda robi swoje, a my swoje.", "Mhm, dzien jak dzien." },
				{ "Lubie, jak jest spokojnie i sucho.", "Taka pogoda mi pasuje." },
				{ "Nie przepadam za taka pogoda.", "Wolalbym cos cieplejszego." },
				{ "Byle nie padalo na expie.", "Lubie, jak jest ladnie i mozna sie powloczyc.", "Kazda pogoda dobra, jak idzie handel.", "Pogoda mi nie przeszkadza, gorzej z mobami.", "Najlepiej jak jest ladnie i ludzie wychodza razem." },
				{ "A u ciebie jak pogoda?", "U ciebie tez tak?" },
				{ "Po prostu wtedy lepiej sie gra.", "Jakos tak mam od zawsze." },
				"slonce, ale bez upalu|lekki deszcz|chlodny wieczor|mroz i snieg" },
			{ T_SEASON, { 50, 60, 50, 45, 55 },
				{ "Kazda pora roku ma cos w sobie.", "No, pory roku lataja szybko." },
				{ "Lubie, ma swoj klimat.", "Tak, calkiem lubie." },
				{ "Raczej nie, wole cieplejsze miesiace.", "Niezbyt, za zimno jak na moj gust." },
				{ "Wole lato, dluzej jasno.", "Jesien jest fajna, wszystko inaczej wyglada.", "Latem wiecej ludzi na targu.", "Zima jest dobra, mniej ludzi na spotach.", "Lato, bo wtedy wszyscy maja czas grac." },
				{ "A ty jaka pore roku lubisz?", "A ty wolisz zime czy lato?" },
				{ "Jakos lepiej sie wtedy czuje.", "Po prostu mniej narzekam wtedy na pogode." },
				"lato|wiosna|jesien|zima" },
			{ T_DAYTIME, { 50, 50, 50, 50, 50 },
				{ "Czas leci jak szalony.", "No, pora jak kazda inna na granie." },
				{ "Lubie te pore.", "Ta pora jest spoko." },
				{ "Nie przepadam za ta pora.", "Wolalbym, zeby bylo inaczej." },
				{ "Najlepiej gra mi sie wieczorem.", "Noca jest spokojniej, lubie to.", "Rano ruch na targu jest najmniejszy.", "Noca moby jakby wredniejsze.", "Wieczorem jest najwiecej ludzi, wiec lubie." },
				{ "A ty dlugo jeszcze siedzisz?", "U ciebie ktora godzina?" },
				{ "Bo wtedy jest spokoj.", "Tak mi pasuje." },
				"wieczor|noc|poranek|popoludnie" },
			{ T_SLEEP, { 60, 50, 50, 40, 55 },
				{ "Sen to podstawa, trzeba sie wyspac.", "No, spanie to wazna sprawa.", "Mnie tez by sie przydalo sie przespac." },
				{ "Lubie sie wyspac, jak tylko moge.", "Spac to ja lubie." },
				{ "Szkoda mi czasu na spanie.", "Jakos malo spie." },
				{ "Spanie to strata czasu na expa.", "Lubie sie wyspac, potem lepiej sie gra.", "Jak sie wyspie, lepiej mi sie liczy yang.", "Po dobrej walce spi sie najlepiej.", "Najlepiej sie spi, jak wie sie, ze jutro znowu razem gramy." },
				{ "Ty sie wysypiasz?", "Dlugo spisz?" },
				{ "Bez snu nic sie nie chce.", "Po prostu tak mam." }, NULL },
			{ T_TIRED, { 30, 30, 30, 30, 30 },
				{ "To odpocznij troche, gra nie ucieknie.", "Znam to. Przerwa dobrze robi.", "Moze pora na chwile przerwy?" },
				{ "", "" }, { "", "" },
				{ "Troche tak, ale jeszcze pociagne.", "Jestem troche zmeczony, ale da sie zyc.", "Zmeczony? Troche, ale interes sie kreci.", "Troche jestem, ale walka mnie budzi.", "Troche tak, ale w dobrym towarzystwie mniej czuc." },
				{ "Dlugo juz grasz?", "Ty tez juz padasz?" },
				{ "Bo siedze juz dluzsza chwile.", "Za duzo biegania dzisiaj." }, NULL },
			{ T_BORED, { 30, 30, 30, 30, 30 },
				{ "Nuda to najgorsze. Trzeba cos wymyslic.", "To moze pobijemy cos razem?", "Znam to uczucie." },
				{ "", "" }, { "", "" },
				{ "Troche, ale exp sam sie nie zrobi.", "Czasem sie nudze, wtedy ide gdzies bez celu.", "Stanie przy straganie potrafi nudzic.", "Jak nie ma z kim walczyc, to tak.", "Samemu sie czasem nudze, w grupie nigdy." },
				{ "A ty sie nudzisz?", "Masz jakis pomysl, co porobic?" },
				{ "Bo to w kolko to samo.", "Jak nic sie nie dzieje, to tak jest." }, NULL },
			{ T_HOBBY, { 50, 70, 50, 50, 60 },
				{ "Fajnie miec jakies zajecie poza gra.", "O, ciekawe hobby.", "Kazdy powinien miec cos swojego." },
				{ "Lubie, jasne.", "Tak, to fajne zajecie." },
				{ "To raczej nie dla mnie.", "Nie bardzo, nie mam do tego cierpliwosci." },
				{ "Jak mam chwile, to po prostu expie. Takie hobby.", "Jak mam spokoj, lubie pochodzic gdzies bez celu i pozwiedzac.", "Lubie liczyc zyski i szukac okazji na targu.", "Lubie dobra walke. Reszta to dodatki.", "Najbardziej lubie grac z kims, samemu to nie to samo." },
				{ "A ty co lubisz robic?", "A ty masz jakies hobby?" },
				{ "Bo to mnie odpreza.", "Tak jakos zawsze mnie ciagnelo." },
				"chodzenie po okolicy|lowienie ryb|gotowanie|czytanie|zbieranie roznych dziwnych rzeczy" },
			{ T_FOOD, { 60, 65, 55, 60, 65 },
				{ "Ale teraz zglodnialem.", "O, smacznego.", "Jedzenie to podstawa.", "Brzmi dobrze." },
				{ "Lubie, jasne.", "Tak, to jest dobre." },
				{ "Niezbyt, nie moj smak.", "Raczej nie przepadam." },
				{ "Zjem cokolwiek, byle szybko i z powrotem na exp.", "Lubie probowac nowych rzeczy.", "Zjem to, co tanie i dobre.", "Po walce zjadlbym konia z kopytami.", "Najlepiej je sie w towarzystwie." },
				{ "A ty co lubisz jesc?", "Jadles juz cos dzisiaj?" },
				{ "Bo jest smaczne i sycace.", "Tak mnie wychowali." },
				"pierogi|pizza|schabowy z ziemniakami|pomidorowa|ryba z ogniska|nalesniki|kebab" },
			{ T_DRINK, { 60, 60, 55, 55, 60 },
				{ "Cos do picia zawsze sie przyda.", "Na zdrowie." },
				{ "Lubie, czemu nie.", "Tak, dobre to jest." },
				{ "Nie bardzo za tym przepadam.", "Raczej nie." },
				{ "Kawa, zeby dalej expic.", "Herbata i spokojny wieczor.", "Pije to, co tanie.", "Cos mocnego po wygranej walce.", "Cokolwiek, byle w dobrym gronie." },
				{ "A ty co pijesz?", "Kawa czy herbata?" },
				{ "Bo stawia na nogi.", "Tak juz mam." },
				"kawa|herbata|kompot|woda|sok jablkowy" },
			{ T_TRAVEL, { 40, 90, 60, 55, 65 },
				{ "Podroze to fajna sprawa.", "O, zazdroszcze.", "Chcialbym kiedys gdzies tak pojechac." },
				{ "Lubie, bardzo.", "Tak, to jest cos." },
				{ "Nie za bardzo, wole znane katy.", "Raczej nie, szkoda mi czasu na droge." },
				{ "Podrozuje glownie miedzy spotami.", "Lubie odkrywac nowe miejsca, nawet jak nic tam nie ma.", "Najbardziej ciekawia mnie miasta i ich targi.", "Pojechalbym tam, gdzie sa mocne potwory.", "Z dobra ekipa pojechalbym wszedzie." },
				{ "A ty gdzie bys pojechal?", "Byles gdzies ciekawym?" },
				{ "Bo lubie zobaczyc cos nowego.", "Bo w jednym miejscu sie nudze." },
				"gory|morze|jakies male miasteczko|daleka pustynia|las gdzies na uboczu" },
			{ T_MUSIC, { 55, 65, 55, 60, 65 },
				{ "Muzyka to dobra sprawa.", "O, niezle.", "Bez muzyki ciezko sie gra." },
				{ "Lubie, czasem slucham.", "Tak, to mi siedzi." },
				{ "Nie moja bajka.", "Raczej nie przepadam." },
				{ "Slucham czegokolwiek, co nie przeszkadza w expie.", "Lubie spokojna muzyke, jak gdzies wedruje.", "Lubie muzyke z tawerny, kojarzy mi sie z targiem.", "Cos ciezszego, pod walke.", "To, co akurat puszcza ekipa." },
				{ "A ty czego sluchasz?", "Masz jakis ulubiony kawalek?" },
				{ "Bo dodaje energii.", "Bo mnie uspokaja." },
				"rock|cos spokojnego|rap|metal|muzyka z tawerny|stare przeboje" },
			{ T_MOVIES, { 50, 60, 50, 55, 60 },
				{ "Dobry film to podstawa wieczoru.", "O, slyszalem cos o tym. Chyba.", "Filmy to dobra odskocznia." },
				{ "Lubie, jak jest ciekawa fabula.", "Tak, czasem cos obejrze." },
				{ "Nie bardzo, szybko sie nudze.", "Raczej nie, wole grac." },
				{ "Rzadko ogladam, szkoda czasu.", "Lubie filmy o podrozach i przygodach.", "Lubie filmy o ludziach, co doszli do fortuny.", "Cos z walka, zeby sie dzialo.", "Najlepiej oglada sie z kims." },
				{ "A ty co ostatnio ogladales?", "Jaki film polecasz?" },
				{ "Bo lubie, jak sie cos dzieje.", "Bo mozna sie oderwac." },
				"filmy przygodowe|fantasy|komedie|filmy akcji|stare bajki" },
			{ T_GAMES, { 60, 60, 55, 60, 60 },
				{ "Gry to jest to.", "Kazda gra ma cos w sobie." },
				{ "Lubie, jasne.", "Tak, czasem pogram." },
				{ "Nie bardzo, to nie dla mnie.", "Raczej nie przepadam." },
				{ "Wole jedna gre i rozwijac postac.", "Lubie gry, gdzie mozna duzo zwiedzac.", "Lubie gry z handlem i ekonomia.", "Cos, gdzie trzeba sie bic.", "Lubie gry, gdzie gra sie z innymi." },
				{ "A ty w co jeszcze grasz?", "Grasz w cos poza tym?" },
				{ "Bo lubie rozwijac postac.", "Bo mozna sie odprezyc." },
				"strategie|RPG-i|karcianki|stare gry z dziecinstwa" },
			{ T_HUMOR, { 60, 60, 60, 60, 70 },
				{ "Haha, dobre.", "Niezle, usmialem sie.", "Hehe, ale z ciebie zartownis." },
				{ "Lubie sie posmiac.", "Dobry zart zawsze na plus." },
				{ "Nie jestem dzis w nastroju do zartow.", "Nie bardzo mnie to bawi." },
				{ "Najsmieszniej jest, jak ktos ginie na mobie o 10 lvl nizej.", "Smieje sie z roznych dziwnych sytuacji w podrozy.", "Najlepszy zart to ceny niektorych straganow.", "Smieszy mnie, jak ktos ucieka przed Metinem.", "Najlepiej smiac sie w grupie." },
				{ "Znasz jakis dobry kawal?", "Masz cos smiesznego?" },
				{ "Bo trzeba miec dystans.", "Bez smiechu byloby smutno." }, NULL },
			{ T_LUCK, { 50, 50, 50, 50, 50 },
				{ "Szczescie sprzyja odwaznym.", "Raz lepiej, raz gorzej.", "Trzeba miec troche farta." },
				{ "", "" }, { "", "" },
				{ "Szczescie trzeba sobie wyexpic.", "Wierze, ze szczescie sie odwraca.", "Szczescie to dobra cena w odpowiednim momencie.", "Szczescie to jak Metin dropi cos dobrego.", "Szczescie to dobra ekipa." },
				{ "A ty masz dzis farta?", "Tobie dzis dopisuje szczescie?" },
				{ "Bo tak to juz jest.", "Taka jest gra." }, NULL },
			{ T_FRIENDSHIP, { 60, 60, 55, 55, 90 },
				{ "Przyjaciele to podstawa.", "Dobrych ludzi warto trzymac blisko.", "Zgadzam sie." },
				{ "", "" }, { "", "" },
				{ "Mam kilku znajomych, z ktorymi czasem expie.", "Poznaje ludzi po drodze, niektorzy zostaja.", "Mam znajomych, ale interesy to interesy.", "Ufam tym, z ktorymi walczylem.", "Dla mnie znajomi to najwazniejsze w tej grze." },
				{ "A ty masz tu znajomych?", "Grasz z kims na stale?" },
				{ "Bo samemu jest trudniej.", "Bo razem jest weselej." }, NULL },
			{ T_TEAMWORK, { 50, 55, 55, 60, 90 },
				{ "Razem zawsze lepiej.", "Wspolpraca to podstawa.", "Dobrze zgrana ekipa robi robote." },
				{ "", "" }, { "", "" },
				{ "Z dobra ekipa exp leci szybciej.", "Lubie grac z innymi, choc czasem wole sam.", "Wspolpraca sie oplaca, doslownie.", "W grupie mozna bic mocniejsze rzeczy.", "Najbardziej lubie grac w grupie." },
				{ "Wolisz grac sam czy z kims?", "Masz stala ekipe?" },
				{ "Bo razem idzie szybciej.", "Bo samemu nie wszystko sie da." }, NULL },
			{ T_LONELY, { 30, 30, 30, 30, 20 },
				{ "Hej, nie jestes sam. Mozemy pogadac.", "Znam to uczucie. Trzeba wyjsc do ludzi.", "Czasem kazdy sie tak czuje." },
				{ "", "" }, { "", "" },
				{ "Czasem gram sam, ale mi to nie przeszkadza.", "Czasem czuje sie samotnie w podrozy, ale mija.", "Samotnosc? Na targu zawsze ktos jest.", "Samemu tez da sie walczyc.", "Nie lubie byc sam, dlatego szukam ekipy." },
				{ "Grasz sam czesto?", "Chcesz pogadac?" },
				{ "Bo tak czasem wychodzi.", "Nie zawsze jest z kim grac." }, NULL },
			{ T_RISK, { 35, 50, 45, 85, 40 },
				{ "Ryzyko to czesc zabawy.", "Czasem trzeba zaryzykowac.", "Ostroznie z tym." },
				{ "Lubie troche ryzyka.", "Jak nie ma ryzyka, to nudno." },
				{ "Wole nie ryzykowac bez potrzeby.", "Raczej gram ostroznie." },
				{ "Ryzykuje tylko, jak sie oplaca w expie.", "Czasem ryzykuje, zeby zobaczyc cos nowego.", "Ryzykuje tylko wtedy, kiedy sie to zwroci.", "Lubie ryzyko. Bez niego nie ma dobrej walki.", "W grupie ryzyko jest mniejsze." },
				{ "A ty lubisz ryzyko?", "Ryzykujesz czasem?" },
				{ "Bo bez ryzyka nie ma nagrody.", "Bo raz juz sie przejechalem." }, NULL },
			{ T_MONEY, { 55, 45, 95, 55, 50 },
				{ "Pieniadze szczescia nie daja, ale pomagaja.", "Kasa zawsze sie przyda.", "No, bez kasy ciezko." },
				{ "", "" }, { "", "" },
				{ "Kasa jest potrzebna, ale wazniejszy jest exp.", "Pieniadze to nie wszystko.", "Pieniadz robi pieniadz, jak sie wie jak.", "Kasa na lepszy sprzet i tyle.", "Kasa jest spoko, ale ludzie wazniejsi." },
				{ "A ty oszczedzasz czy wydajesz?", "Duzo wydajesz na sprzet?" },
				{ "Bo za wszystko trzeba placic.", "Bo tak dziala swiat." }, NULL },
			{ T_WORK, { 45, 45, 55, 45, 50 },
				{ "Praca to praca, trzeba jakos zyc.", "Oj, znam to.", "Wspolczuje, jak meczaca." },
				{ "", "" }, { "", "" },
				{ "Moja praca to expienie.", "Nie wyobrazam sobie siedziec w jednym miejscu caly dzien.", "Handel to moja praca.", "Walka to moja robota.", "Najlepiej pracuje sie z fajnymi ludzmi." },
				{ "A ty pracujesz?", "Meczaca ta twoja robota?" },
				{ "Bo trzeba z czegos zyc.", "Tak wyszlo." }, NULL },
			{ T_SCHOOL, { 40, 55, 45, 40, 50 },
				{ "Nauka sie przydaje, nawet jak nie chce sie wierzyc.", "Powodzenia z nauka.", "Oj, szkola to temat." },
				{ "Lubilem sie uczyc nowych rzeczy.", "Nauka jest spoko, jak jest ciekawa." },
				{ "Nigdy nie lubilem siedziec w lawce.", "Szkola to nie moj temat." },
				{ "Ucze sie glownie nowych skilli.", "Lubie sie uczyc nowych rzeczy po drodze.", "Najwiecej nauczyl mnie targ.", "Najlepsza nauka to walka.", "Najwiecej nauczylem sie od innych graczy." },
				{ "Uczysz sie jeszcze?", "Jak ci idzie nauka?" },
				{ "Bo wiedza sie przydaje.", "Bo to sie oplaca." }, NULL },
			{ T_LIFE, { 50, 60, 50, 50, 60 },
				{ "Zycie to ciekawa sprawa.", "Gleboka mysl jak na szept.", "Kazdy ma swoja droge." },
				{ "", "" }, { "", "" },
				{ "Dla mnie sens to isc do przodu, poziom po poziomie.", "Zycie to podroz, liczy sie droga.", "Zycie to dobre inwestycje i spokoj na starosc.", "Zycie to walka, trzeba byc gotowym.", "Zycie to ludzie, z ktorymi je dzielisz." },
				{ "A ty jak myslisz?", "A dla ciebie co jest wazne?" },
				{ "Tak to czuje.", "Tak mnie nauczylo zycie." }, NULL },
			{ T_DREAMS, { 50, 50, 50, 50, 50 }, { "Marzenia sa wazne.", "Oby sie spelnilo.", "Ladne marzenie." },
				{ "", "" }, { "", "" }, { NULL, NULL, NULL, NULL, NULL },
				{ "A ty o czym marzysz?", "A ty masz jakies marzenie?" },
				{ "Bo tak czuje.", "Bo to by duzo zmienilo." }, NULL },
			{ T_FEAR, { 50, 50, 50, 50, 50 }, { "Kazdy sie czegos boi.", "Rozumiem, to straszne.", "Nie ma sie czego wstydzic." },
				{ "", "" }, { "", "" }, { NULL, NULL, NULL, NULL, NULL },
				{ "A ty czego sie boisz?", "A ciebie co przeraza?" },
				{ "Bo tak juz mam.", "Raz sie przejechalem." }, NULL },
			{ T_ANNOY, { 50, 50, 50, 50, 50 }, { "Tez mnie to wkurza.", "Rozumiem, to potrafi zdenerwowac.", "Oj, znam to." },
				{ "", "" }, { "", "" }, { NULL, NULL, NULL, NULL, NULL },
				{ "A ciebie co denerwuje?", "Ciebie tez to wkurza?" },
				{ "Bo to strata czasu.", "Bo tak nie powinno byc." }, NULL },
			{ T_JOY, { 50, 50, 50, 50, 50 }, { "To fajnie, ciesze sie.", "Super, tak trzymaj.", "Dobrze to slyszec." },
				{ "", "" }, { "", "" }, { NULL, NULL, NULL, NULL, NULL },
				{ "A ciebie co cieszy?", "Co ci dzis poprawilo humor?" },
				{ "Bo wtedy wiem, ze bylo warto.", "Bo takie chwile sie pamieta." }, NULL },
			{ T_FEELINGS, { 50, 50, 50, 50, 50 }, { "Rozumiem.", "Trzymaj sie.", "Bywa i tak." },
				{ "", "" }, { "", "" },
				{ "U mnie bez wiekszych emocji, robie swoje.", "Czasem jestem wesoly, czasem mniej, jak kazdy.", "Humor mi sie zmienia z cenami.", "Emocje zostawiam na walke.", "Lepiej sie czuje, jak mam z kim pogadac." },
				{ "A ty jak sie dzis czujesz?", "Co u ciebie tak naprawde?" },
				{ "Tak po prostu jest.", "Bywa roznie." }, NULL },
			{ T_ANIMALS, { 55, 70, 50, 55, 70 },
				{ "Zwierzaki sa super.", "O, fajnie.", "Lubie zwierzeta." },
				{ "Lubie, jasne.", "Tak, sa fajne." },
				{ "Nie przepadam.", "Raczej nie, wole z daleka." },
				{ "Najbardziej lubie swojego konia, bo szybko biega.", "Lubie obserwowac zwierzaki w podrozy.", "Zwierzaki sa fajne, byle nie drogie w utrzymaniu.", "Szanuje zwierzeta, ktore potrafia walczyc.", "Zwierzaki to najlepsze towarzystwo." },
				{ "Masz jakiegos zwierzaka?", "Psy czy koty?" },
				{ "Bo sa wierne.", "Bo sa szczere." },
				"psy|koty|konie|lisy|sowy" },
			{ T_SPORT, { 55, 55, 45, 70, 55 },
				{ "Sport to zdrowie.", "O, szacun.", "Ruch jest wazny." },
				{ "Lubie, czasem cos porobie.", "Tak, sport jest spoko." },
				{ "Niezbyt, wole grac.", "Raczej nie jestem sportowcem." },
				{ "Moj sport to bieganie od moba do moba.", "Lubie dlugie spacery.", "Sport? Tylko jak mozna na nim zarobic.", "Walka to moj sport.", "Lubie sporty druzynowe." },
				{ "Uprawiasz cos?", "Ogladasz mecze?" },
				{ "Bo trzeba sie ruszac.", "Bo daje energie." },
				"pilka nozna|bieganie|plywanie|rower|sztuki walki" },
			{ T_LOVE, { 50, 50, 50, 50, 60 },
				{ "Milosc to piekna sprawa.", "Oj, sprawy sercowe.", "Powodzenia w tych sprawach." },
				{ "", "" }, { "", "" },
				{ "Na razie mam czas tylko na expa.", "Moze kiedys kogos spotkam w podrozy.", "Na razie zakochany jestem w dobrych cenach.", "Na razie moja milosc to moj miecz.", "Kto wie, moze kiedys." },
				{ "A ty masz kogos?", "Spotykasz sie z kims?" },
				{ "Bo tak wyszlo.", "Na razie inne rzeczy sa wazniejsze." }, NULL },
			{ T_BOOKS, { 45, 65, 50, 40, 55 },
				{ "Czytanie to dobra rzecz.", "O, ciekawie brzmi." },
				{ "Lubie, jak mam czas.", "Tak, czasem cos poczytam." },
				{ "Nie bardzo, szybko zasypiam.", "Raczej nie czytam." },
				{ "Czytam glownie ksiegi umiejetnosci.", "Lubie historie o podrozach.", "Czytam tylko cenniki.", "Czytam o slawnych bitwach.", "Wole jak ktos mi opowie." },
				{ "A ty czytasz cos?", "Polecisz cos?" },
				{ "Bo mozna sie czegos nauczyc.", "Bo odpreza." },
				"fantastyka|ksiazki przygodowe|historie o podrozach|kryminaly" },
			{ T_NATURE, { 50, 85, 45, 50, 60 },
				{ "Natura jest piekna.", "Tez lubie takie miejsca.", "Brzmi spokojnie." },
				{ "Lubie, spokoj i cisza.", "Tak, bardzo." },
				{ "Nie bardzo, wole miasto.", "Raczej wole miejsca z ludzmi." },
				{ "Las jest spoko, byle byly moby.", "Kocham lasy i gory, moglbym tam siedziec godzinami.", "Natura jest ladna, ale na targu wiecej sie dzieje.", "W lesie jest najwiecej zwierzyny do bicia.", "Najlepiej na lonie natury z kims." },
				{ "Lubisz chodzic po lesie?", "Wolisz gory czy morze?" },
				{ "Bo tam jest spokoj.", "Bo mozna odpoczac od halasu." },
				"las|jezioro|gory|laka" },
			{ T_ADVENTURE, { 55, 90, 50, 80, 65 },
				{ "Przygoda to jest to!", "Brzmi jak niezla przygoda." },
				{ "Lubie przygody.", "Tak, zawsze." },
				{ "Wole spokoj.", "Raczej nie szukam przygod." },
				{ "Przygoda jest fajna, jak daje expa.", "Zyje dla przygod.", "Przygoda to dobra okazja na zysk.", "Kazda walka to przygoda.", "Przygody najlepiej przezywac z kims." },
				{ "Miales ostatnio jakas przygode?", "Szukasz przygod?" },
				{ "Bo zycie bez nich jest nudne.", "Bo zawsze cos sie dzieje." }, NULL },
			{ T_COLOR, { 50, 50, 50, 50, 50 },
				{ "Ladny kolor.", "Gust to gust." },
				{ "Lubie ten kolor.", "Tak, ladny." },
				{ "Nie moj kolor.", "Raczej nie." },
				{ "Kolor mi obojetny, byle item mial dobre bonusy.", "Lubie kolory natury.", "Zloty, jak yang.", "Czerwony, jak krew na polu bitwy.", "Kazdy kolor jest ok." },
				{ "A ty jaki kolor lubisz?", "Masz ulubiony kolor?" },
				{ "Tak mi sie podoba.", "Kojarzy mi sie dobrze." },
				"niebieski|czerwony|czarny|zielony|zloty" },
		};
		for (size_t i = 0; i < sizeof(kPacks) / sizeof(kPacks[0]); ++i)
			if (kPacks[i].topic == topic)
				return &kPacks[i];
		return NULL;
	}

	// The same packs for an English reader. Field by field the twin of the
	// Polish one: an empty text where it has one, a NULL where it has one.
	inline const TTopicPack* FindTopicPackEn(int topic)
	{
		static const TTopicPack kPacks[] = {
			{ T_WEATHER, { 40, 60, 45, 40, 55 },
				{ "Yeah, the weather's kinda weird today.", "Noticed that too.", "Weather does its thing, we do ours.", "Mhm, just another day." },
				{ "I like it calm and dry.", "That kind of weather works for me." },
				{ "Not a fan of that kind of weather.", "I'd prefer something warmer." },
				{ "As long as it doesn't rain while I grind.", "I like it when it's nice out and I can wander around.", "Any weather's fine if trading goes well.", "Weather doesn't bother me, the mobs do.", "Best when it's nice and people go out together." },
				{ "How's the weather where you are?", "Same where you are?" },
				{ "The game just feels better then.", "Always been like that for me." },
				"sun, but not too hot|light rain|a cool evening|frost and snow" },
			{ T_SEASON, { 50, 60, 50, 45, 55 },
				{ "Every season has something to it.", "Yeah, the seasons fly by." },
				{ "I like it, it has its own vibe.", "Yeah, I like it quite a bit." },
				{ "Not really, I prefer the warmer months.", "Not much, too cold for my taste." },
				{ "I prefer summer, longer days.", "Autumn's nice, everything looks different.", "More people at the market in summer.", "Winter's good, fewer people at the spots.", "Summer, 'cause everyone has time to play then." },
				{ "What season do you like?", "Do you prefer winter or summer?" },
				{ "I just feel better then.", "I complain less about the weather then." },
				"summer|spring|autumn|winter" },
			{ T_DAYTIME, { 50, 50, 50, 50, 50 },
				{ "Time flies like crazy.", "Yeah, as good a time as any to play." },
				{ "I like this time of day.", "This time's alright." },
				{ "Not a fan of this time of day.", "I'd rather it was different." },
				{ "I play best in the evening.", "It's quieter at night, I like that.", "The market's emptiest in the morning.", "Mobs feel meaner at night.", "Evenings have the most people, so I like them." },
				{ "You staying on much longer?", "What time is it for you?" },
				{ "It's quiet then.", "Just suits me." },
				"evening|night|morning|afternoon" },
			{ T_SLEEP, { 60, 50, 50, 40, 55 },
				{ "Sleep is important, gotta get your rest.", "Yeah, sleep's a big deal.", "I could use some sleep too." },
				{ "I like a good sleep whenever I can.", "I do love sleeping." },
				{ "Sleeping feels like wasted time to me.", "I don't sleep much somehow." },
				{ "Sleep is time I could spend grinding.", "I like a good night's sleep, I play better after.", "After a good sleep I count my yang better.", "You sleep best after a good fight.", "Sleep's best when you know we're playing together tomorrow." },
				{ "Do you get enough sleep?", "Do you sleep long?" },
				{ "Without sleep nothing feels worth doing.", "Just how I am." }, NULL },
			{ T_TIRED, { 30, 30, 30, 30, 30 },
				{ "Take a break then, the game won't run away.", "I know that feeling. A break helps.", "Maybe time for a short break?" },
				{ "", "" }, { "", "" },
				{ "A bit, but I can keep going.", "I'm a bit tired, but I'll live.", "Tired? A bit, but business is running.", "A bit, but fighting wakes me up.", "A bit, but good company helps." },
				{ "Been playing long?", "You falling asleep too?" },
				{ "I've been at it for a while.", "Too much running around today." }, NULL },
			{ T_BORED, { 30, 30, 30, 30, 30 },
				{ "Boredom's the worst. Gotta think of something.", "Wanna go kill something together?", "I know that feeling." },
				{ "", "" }, { "", "" },
				{ "A bit, but the exp won't farm itself.", "Sometimes I get bored, then I just wander somewhere.", "Standing at the shop can get boring.", "When there's nothing to fight, yeah.", "Solo I get bored sometimes, in a group never." },
				{ "Are you bored?", "Got any idea what to do?" },
				{ "It's the same thing over and over.", "When nothing's happening, that's how it is." }, NULL },
			{ T_HOBBY, { 50, 70, 50, 50, 60 },
				{ "Nice to have something outside the game.", "Oh, interesting hobby.", "Everyone should have something of their own." },
				{ "I like it, sure.", "Yeah, that's a fun thing to do." },
				{ "Not really my thing.", "Not really, I don't have the patience for it." },
				{ "When I have time, I just grind. That's my hobby.", "When it's quiet I like to wander around and explore.", "I like counting profits and looking for deals at the market.", "I like a good fight. Everything else is extra.", "I like playing with others most, solo just isn't the same." },
				{ "What do you like doing?", "Got any hobbies?" },
				{ "It relaxes me.", "Always been drawn to it." },
				"walking around|fishing|cooking|reading|collecting weird stuff" },
			{ T_FOOD, { 60, 65, 55, 60, 65 },
				{ "Now I'm hungry.", "Oh, enjoy your meal.", "Food comes first.", "Sounds good." },
				{ "I like it, sure.", "Yeah, that's good stuff." },
				{ "Not really, not my taste.", "Not a big fan." },
				{ "I'll eat anything, as long as it's quick and I'm back to grinding.", "I like trying new things.", "I eat whatever's cheap and good.", "After a fight I could eat a horse.", "Food's best with company." },
				{ "What do you like to eat?", "Have you eaten anything today?" },
				{ "It's tasty and filling.", "That's how I was raised." },
				"pierogi|pizza|pork chop with potatoes|tomato soup|fish from the campfire|pancakes|kebab" },
			{ T_DRINK, { 60, 60, 55, 55, 60 },
				{ "Something to drink always helps.", "Cheers." },
				{ "I like it, why not.", "Yeah, that's good." },
				{ "Not really into it.", "Not really." },
				{ "Coffee, so I can keep grinding.", "Tea and a quiet evening.", "I drink whatever's cheap.", "Something strong after a won fight.", "Anything, as long as the company's good." },
				{ "What do you drink?", "Coffee or tea?" },
				{ "It keeps me going.", "Just how I am." },
				"coffee|tea|fruit compote|water|apple juice" },
			{ T_TRAVEL, { 40, 90, 60, 55, 65 },
				{ "Traveling is great.", "Oh, I'm jealous.", "I'd love to go somewhere like that one day." },
				{ "I like it a lot.", "Yeah, that's something." },
				{ "Not really, I prefer places I know.", "Not really, I don't wanna waste time on the road." },
				{ "I mostly travel between spots.", "I like finding new places, even if there's nothing there.", "Towns and their markets interest me most.", "I'd go where the strong monsters are.", "With a good team I'd go anywhere." },
				{ "Where would you go?", "Been anywhere interesting?" },
				{ "I like seeing something new.", "I get bored in one place." },
				"the mountains|the sea|some small town|a faraway desert|a forest somewhere off the beaten path" },
			{ T_MUSIC, { 55, 65, 55, 60, 65 },
				{ "Music's a good thing.", "Oh, not bad.", "Hard to play without music." },
				{ "I like it, listen to it sometimes.", "Yeah, that's my thing." },
				{ "Not my thing.", "Not a big fan." },
				{ "I listen to anything that doesn't get in the way of grinding.", "I like calm music when I'm wandering.", "I like tavern music, reminds me of the market.", "Something heavier, for fighting.", "Whatever the team's playing." },
				{ "What do you listen to?", "Got a favorite song?" },
				{ "It gives me energy.", "It calms me down." },
				"rock|something calm|rap|metal|tavern music|old hits" },
			{ T_MOVIES, { 50, 60, 50, 55, 60 },
				{ "A good movie makes the evening.", "Oh, heard something about that. I think.", "Movies are a nice escape." },
				{ "I like them when the story's good.", "Yeah, I watch something now and then." },
				{ "Not really, I get bored fast.", "Not really, I'd rather play." },
				{ "I rarely watch, feels like a waste of time.", "I like travel and adventure movies.", "I like movies about people who made a fortune.", "Something with fighting, so stuff happens.", "Movies are best watched with someone." },
				{ "What did you watch lately?", "Any movie you'd recommend?" },
				{ "I like it when stuff happens.", "You can switch off for a while." },
				"adventure movies|fantasy|comedies|action movies|old cartoons" },
			{ T_GAMES, { 60, 60, 55, 60, 60 },
				{ "Games are where it's at.", "Every game has something to it." },
				{ "I like them, sure.", "Yeah, I play sometimes." },
				{ "Not really, not for me.", "Not a big fan." },
				{ "I prefer one game and building up my character.", "I like games where you can explore a lot.", "I like games with trading and an economy.", "Something where you have to fight.", "I like games where you play with others." },
				{ "What else do you play?", "Do you play anything besides this?" },
				{ "I like building up a character.", "It's relaxing." },
				"strategy games|RPGs|card games|old games from my childhood" },
			{ T_HUMOR, { 60, 60, 60, 60, 70 },
				{ "Haha, good one.", "Not bad, that made me laugh.", "Hehe, you're a funny one." },
				{ "I like a good laugh.", "A good joke is always a plus." },
				{ "Not in the mood for jokes today.", "Doesn't really amuse me." },
				{ "Funniest is when someone dies to a mob 10 levels lower.", "I laugh at all kinds of weird stuff on the road.", "Best joke is the prices at some shops.", "It cracks me up when someone runs from a Metin.", "Laughing's best in a group." },
				{ "Know any good jokes?", "Got anything funny?" },
				{ "Gotta not take things too seriously.", "It'd be sad without laughing." }, NULL },
			{ T_LUCK, { 50, 50, 50, 50, 50 },
				{ "Fortune favors the bold.", "Some days better, some worse.", "You need a bit of luck." },
				{ "", "" }, { "", "" },
				{ "You gotta grind your own luck.", "I believe luck turns around.", "Luck is a good price at the right moment.", "Luck is when a Metin drops something good.", "Luck is a good team." },
				{ "Are you lucky today?", "Is luck on your side today?" },
				{ "That's just how it is.", "That's the game." }, NULL },
			{ T_FRIENDSHIP, { 60, 60, 55, 55, 90 },
				{ "Friends are everything.", "Good people are worth keeping close.", "Agreed." },
				{ "", "" }, { "", "" },
				{ "I've got a few friends I grind with sometimes.", "I meet people along the way, some stick around.", "I've got friends, but business is business.", "I trust the ones I've fought with.", "For me friends are the most important thing in this game." },
				{ "Got any friends here?", "Do you play with someone regularly?" },
				{ "It's harder alone.", "It's more fun together." }, NULL },
			{ T_TEAMWORK, { 50, 55, 55, 60, 90 },
				{ "Always better together.", "Teamwork is key.", "A team that plays well together gets things done." },
				{ "", "" }, { "", "" },
				{ "With a good team the exp flies.", "I like playing with others, though sometimes I prefer solo.", "Teamwork pays off, literally.", "In a group you can take on tougher stuff.", "I like playing in a group most." },
				{ "Do you prefer playing solo or with someone?", "Got a regular team?" },
				{ "It goes faster together.", "Not everything can be done solo." }, NULL },
			{ T_LONELY, { 30, 30, 30, 30, 20 },
				{ "Hey, you're not alone. We can talk.", "I know that feeling. Gotta get out there.", "Everyone feels like that sometimes." },
				{ "", "" }, { "", "" },
				{ "I play solo sometimes, but it doesn't bother me.", "Sometimes I feel lonely on the road, but it passes.", "Lonely? There's always someone at the market.", "You can fight solo too.", "I don't like being alone, that's why I look for a team." },
				{ "Do you play solo a lot?", "Wanna talk?" },
				{ "Just turns out that way sometimes.", "There isn't always someone to play with." }, NULL },
			{ T_RISK, { 35, 50, 45, 85, 40 },
				{ "Risk is part of the fun.", "Sometimes you gotta take a risk.", "Careful with that." },
				{ "I like a bit of risk.", "No risk, no fun." },
				{ "I'd rather not take risks for nothing.", "I usually play it safe." },
				{ "I only take risks when it pays in exp.", "Sometimes I take a risk just to see something new.", "I only take a risk when it'll pay off.", "I like risk. There's no good fight without it.", "Risk is smaller in a group." },
				{ "Do you like taking risks?", "Do you take risks sometimes?" },
				{ "No risk, no reward.", "Because I got burned once." }, NULL },
			{ T_MONEY, { 55, 45, 95, 55, 50 },
				{ "Money can't buy happiness, but it helps.", "Cash always comes in handy.", "Yeah, it's hard without money." },
				{ "", "" }, { "", "" },
				{ "Money's needed, but exp matters more.", "Money isn't everything.", "Money makes money, if you know how.", "Money for better gear, that's it.", "Money's fine, but people matter more." },
				{ "Do you save or spend?", "Spend a lot on gear?" },
				{ "Everything costs something.", "That's how the world works." }, NULL },
			{ T_WORK, { 45, 45, 55, 45, 50 },
				{ "Work is work, gotta make a living.", "Oh, I know that.", "Sorry, sounds exhausting." },
				{ "", "" }, { "", "" },
				{ "My job is grinding.", "Can't imagine sitting in one place all day.", "Trading is my job.", "Fighting is my job.", "Work's best with nice people." },
				{ "Do you work?", "Is your job tiring?" },
				{ "Gotta make a living somehow.", "It just turned out that way." }, NULL },
			{ T_SCHOOL, { 40, 55, 45, 40, 50 },
				{ "Learning pays off, even if it's hard to believe.", "Good luck with your studies.", "Oh, school's a whole topic." },
				{ "I liked learning new things.", "Learning's fine when it's interesting." },
				{ "Never liked sitting at a desk.", "School's not my thing." },
				{ "I mostly learn new skills.", "I like picking up new things along the way.", "The market taught me the most.", "Fighting is the best teacher.", "I learned the most from other players." },
				{ "Are you still in school?", "How's school going?" },
				{ "Knowledge comes in handy.", "Because it pays off." }, NULL },
			{ T_LIFE, { 50, 60, 50, 50, 60 },
				{ "Life is an interesting thing.", "Deep thought for a whisper.", "Everyone has their own path." },
				{ "", "" }, { "", "" },
				{ "For me the point is to keep moving forward, level by level.", "Life's a journey, it's the road that counts.", "Life is good investments and a quiet old age.", "Life is a fight, you gotta be ready.", "Life is the people you share it with." },
				{ "What do you think?", "What's important to you?" },
				{ "That's how I feel it.", "That's what life taught me." }, NULL },
			{ T_DREAMS, { 50, 50, 50, 50, 50 }, { "Dreams matter.", "Hope it comes true.", "Nice dream." },
				{ "", "" }, { "", "" }, { NULL, NULL, NULL, NULL, NULL },
				{ "What do you dream about?", "Got any dreams?" },
				{ "That's how I feel.", "It'd change a lot." }, NULL },
			{ T_FEAR, { 50, 50, 50, 50, 50 }, { "Everyone's afraid of something.", "I get it, that's scary.", "Nothing to be ashamed of." },
				{ "", "" }, { "", "" }, { NULL, NULL, NULL, NULL, NULL },
				{ "What are you afraid of?", "What scares you?" },
				{ "Just how I am.", "I got burned once." }, NULL },
			{ T_ANNOY, { 50, 50, 50, 50, 50 }, { "That annoys me too.", "I get it, that can drive you mad.", "Oh, I know that." },
				{ "", "" }, { "", "" }, { NULL, NULL, NULL, NULL, NULL },
				{ "What annoys you?", "Does that annoy you too?" },
				{ "It's a waste of time.", "It shouldn't be like that." }, NULL },
			{ T_JOY, { 50, 50, 50, 50, 50 }, { "That's great, happy for you.", "Awesome, keep it up.", "Good to hear." },
				{ "", "" }, { "", "" }, { NULL, NULL, NULL, NULL, NULL },
				{ "What makes you happy?", "What made your day today?" },
				{ "Then I know it was worth it.", "You remember moments like that." }, NULL },
			{ T_FEELINGS, { 50, 50, 50, 50, 50 }, { "I get it.", "Hang in there.", "It happens." },
				{ "", "" }, { "", "" },
				{ "Nothing special for me, just doing my thing.", "Sometimes I'm happy, sometimes less, like everyone.", "My mood goes up and down with the prices.", "I save my emotions for the fight.", "I feel better when I've got someone to talk to." },
				{ "How are you feeling today?", "How are you really doing?" },
				{ "It just is.", "It varies." }, NULL },
			{ T_ANIMALS, { 55, 70, 50, 55, 70 },
				{ "Animals are awesome.", "Oh, nice.", "I like animals." },
				{ "I like them, sure.", "Yeah, they're cool." },
				{ "Not a fan.", "Not really, I prefer them at a distance." },
				{ "I like my horse best, it runs fast.", "I like watching animals on the road.", "Animals are nice, as long as they're not expensive to keep.", "I respect animals that can fight.", "Animals are the best company." },
				{ "Do you have a pet?", "Dogs or cats?" },
				{ "They're loyal.", "They're honest." },
				"dogs|cats|horses|foxes|owls" },
			{ T_SPORT, { 55, 55, 45, 70, 55 },
				{ "Sport keeps you healthy.", "Oh, respect.", "Moving is important." },
				{ "I like it, do something now and then.", "Yeah, sports are cool." },
				{ "Not really, I'd rather play.", "I'm not much of an athlete." },
				{ "My sport is running from mob to mob.", "I like long walks.", "Sport? Only if I can make money on it.", "Fighting is my sport.", "I like team sports." },
				{ "Do you do any sports?", "Do you watch matches?" },
				{ "Gotta keep moving.", "It gives you energy." },
				"football|running|swimming|cycling|martial arts" },
			{ T_LOVE, { 50, 50, 50, 50, 60 },
				{ "Love is a beautiful thing.", "Oh, matters of the heart.", "Good luck with that." },
				{ "", "" }, { "", "" },
				{ "For now I only have time for exp.", "Maybe I'll meet someone on the road one day.", "For now I'm in love with good prices.", "For now my love is my sword.", "Who knows, maybe one day." },
				{ "Do you have someone?", "Are you seeing someone?" },
				{ "Just turned out that way.", "Other things matter more for now." }, NULL },
			{ T_BOOKS, { 45, 65, 50, 40, 55 },
				{ "Reading is a good thing.", "Oh, sounds interesting." },
				{ "I like it when I have time.", "Yeah, I read something now and then." },
				{ "Not really, I fall asleep fast.", "I don't really read." },
				{ "I mostly read skill books.", "I like stories about travels.", "I only read price lists.", "I read about famous battles.", "I prefer when someone tells me." },
				{ "Do you read anything?", "Anything you'd recommend?" },
				{ "You can learn something.", "It's relaxing." },
				"fantasy|adventure books|travel stories|crime novels" },
			{ T_NATURE, { 50, 85, 45, 50, 60 },
				{ "Nature is beautiful.", "I like places like that too.", "Sounds peaceful." },
				{ "I like it, peace and quiet.", "Yeah, a lot." },
				{ "Not really, I prefer the city.", "I prefer places with people." },
				{ "The forest's fine, as long as there are mobs.", "I love forests and mountains, could stay there for hours.", "Nature's pretty, but more happens at the market.", "The forest has the most game to hunt.", "Best out in nature with someone." },
				{ "Do you like walking in the woods?", "Mountains or the sea?" },
				{ "It's peaceful there.", "You can get away from the noise." },
				"the forest|a lake|the mountains|a meadow" },
			{ T_ADVENTURE, { 55, 90, 50, 80, 65 },
				{ "Adventure is where it's at!", "Sounds like quite an adventure." },
				{ "I like adventures.", "Yeah, always." },
				{ "I prefer peace and quiet.", "I don't really go looking for adventure." },
				{ "Adventure's great when it gives exp.", "I live for adventure.", "Adventure is a good chance for profit.", "Every fight is an adventure.", "Adventures are best shared with someone." },
				{ "Had any adventures lately?", "Looking for adventure?" },
				{ "Life without them is boring.", "There's always something happening." }, NULL },
			{ T_COLOR, { 50, 50, 50, 50, 50 },
				{ "Nice color.", "Taste is taste." },
				{ "I like that color.", "Yeah, it's nice." },
				{ "Not my color.", "Not really." },
				{ "I don't care about color, as long as the item has good bonuses.", "I like nature's colors.", "Gold, like yang.", "Red, like blood on the battlefield.", "Every color's fine." },
				{ "What color do you like?", "Got a favorite color?" },
				{ "I just like it.", "It reminds me of good things." },
				"blue|red|black|green|gold" },
		};
		for (size_t i = 0; i < sizeof(kPacks) / sizeof(kPacks[0]); ++i)
			if (kPacks[i].topic == topic)
				return &kPacks[i];
		return NULL;
	}

	// The pack in the reader's language.
	inline const TTopicPack* TopicPackFor(const TGen& g, int topic)
	{
		return g.en ? FindTopicPackEn(topic) : FindTopicPack(topic);
	}

	// Picks from a TTopicPack field (skipping empty and NULL entries).
	inline std::string PickField(TGen& g, const char* const* arr, size_t n)
	{
		const char* tmp[8];
		size_t k = 0;
		for (size_t i = 0; i < n && k < 8; ++i)
			if (arr[i] && *arr[i])
				tmp[k++] = arr[i];
		if (!k)
			return std::string();
		return Fill(g, Pick(g, tmp, k));
	}

	inline std::string PickFavorite(TGen& g, const TTopicPack* pack)
	{
		if (!pack || !pack->favorites)
			return std::string();
		std::vector<std::string> items;
		std::string cur;
		for (const char* p = pack->favorites; ; ++p)
		{
			if (*p == '|' || *p == 0)
			{
				if (!cur.empty())
					items.push_back(cur);
				cur.clear();
				if (!*p)
					break;
			}
			else
				cur += *p;
		}
		if (items.empty())
			return std::string();
		// Stable per bot: the favourite does not change between two questions.
		const u32 h = HashStr(TopicName((ETopic)pack->topic), g.m.botPID * 40503u + (u32)g.s.style);
		return items[h % items.size()];
	}

	// Does this bot like it? Deterministic per bot + word, leaning on the voice.
	inline int LikeLevel(TGen& g, const TTopicPack* pack, const std::string& what)
	{
		int bias = pack ? pack->likeBias[g.voice] : 50;
		const int roll = OpinionRoll(g, what.empty() ? std::string(TopicName(pack ? (ETopic)pack->topic : T_NONE)) : what, 17);
		// The cold is disliked by most - the requirement example, and people.
		if (g.a && g.a->concepts.Has(C_COLD))
			bias -= 20;
		if (g.a && g.a->concepts.Has(C_WARM))
			bias += 10;
		if (roll < bias - 10) return 2;  // likes
		if (roll > bias + 20) return 0;  // does not
		return 1;                         // so-so
	}

	inline std::string WithEcho(TGen& g, const std::string& body, u32 chance)
	{
		if (HasEchoObject(g) && g.rng.Chance(chance))
			return EchoObject(g) + "? " + body;
		return body;
	}

	// ---------------------------------------------------------- per question

	inline std::string GenDream(TGen& g)
	{
		static const char* const kDream[V_COUNT][3] = {
			{ "Wbic maksymalny poziom. Proste.", "Zeby kiedys byc najmocniejszy na serwerze.", "Miec full set +9. To jest marzenie." },
			{ "Moze kiedys znalezc miejsce, gdzie nie trzeba caly czas walczyc.", "Zobaczyc kazdy zakatek tego swiata.", "Miec wlasny domek gdzies w gorach." },
			{ "Miec najwiekszy stragan w miescie.", "Zarobic tyle, zeby juz nie liczyc.", "Kupic cos, na co wszyscy patrza z zazdroscia." },
			{ "Pokonac cos, czego nikt jeszcze nie pokonal.", "Rozwalic tysiac Metinow.", "Wygrac wielka wojne gildii." },
			{ "Miec stala ekipe, na ktora zawsze mozna liczyc.", "Zeby wszyscy znajomi byli razem online.", "Zalozyc gildie z fajnymi ludzmi." },
		};
		static const char* const kDreamEn[V_COUNT][3] = {
			{ "Hitting max level. Simple.", "Being the strongest on the server one day.", "Having a full +9 set. That's the dream." },
			{ "Maybe finding a place one day where you don't have to fight all the time.", "Seeing every corner of this world.", "Having my own little house somewhere in the mountains." },
			{ "Having the biggest shop in town.", "Earning so much I stop counting.", "Buying something everyone looks at with envy." },
			{ "Beating something nobody's beaten yet.", "Smashing a thousand Metins.", "Winning a huge guild war." },
			{ "Having a regular team I can always count on.", "All my friends online together.", "Starting a guild with cool people." },
		};
		std::string out = Pick(g, g.en ? kDreamEn[g.voice] : kDream[g.voice], 3);
		if (g.Bad() && g.rng.Chance(40))
			out = Txt(g, "Teraz to marze glownie o odpoczynku. ", "Right now I mostly dream about a rest. ") + out;
		return out;
	}

	inline std::string GenFear(TGen& g)
	{
		static const char* const kFear[V_COUNT][3] = {
			{ "Straty expa po smierci. Serio.", "Tego, ze utkne na jednym poziomie.", "Niczego szczegolnego. Moze tylko spalenia broni." },
			{ "Chyba tego, ze kiedys zobacze juz wszystko.", "Ciemnych lochow bez wyjscia.", "Samotnosci w dalekiej podrozy." },
			{ "Krachu cen na targu.", "Ze ktos mnie oszuka na handlu.", "Pustego straganu." },
			{ "Niczego. No, moze nudy.", "Tego, ze trafie na kogos mocniejszego.", "Zeby nie zginac glupio na slabym mobie." },
			{ "Ze zostane sam.", "Ze znajomi przestana grac.", "Ze zawiode ekipe w waznej chwili." },
		};
		static const char* const kFearEn[V_COUNT][3] = {
			{ "Losing exp when I die. Seriously.", "Getting stuck at one level.", "Nothing special. Maybe burning my weapon." },
			{ "Maybe that one day I'll have seen everything.", "Dark dungeons with no way out.", "Being lonely on a long journey." },
			{ "A market crash.", "Getting scammed in a trade.", "An empty shop." },
			{ "Nothing. Well, maybe boredom.", "Running into someone stronger.", "Dying stupidly to a weak mob." },
			{ "Ending up alone.", "My friends quitting the game.", "Letting the team down when it matters." },
		};
		std::string out = Pick(g, g.en ? kFearEn[g.voice] : kFear[g.voice], 3);
		if (g.LowHp())
			out = Txt(g, "Teraz to boje sie glownie tego moba obok, mam malo HP. ",
					"Right now I'm mostly scared of the mob next to me, my HP is low. ") + out;
		return out;
	}

	inline std::string GenAnnoy(TGen& g)
	{
		static const char* const kAnnoy[V_COUNT][3] = {
			{ "Jak ktos kradnie mi moby.", "Jak exp stoi w miejscu.", "Jak bron sie spali przy ulepszaniu." },
			{ "Jak ktos sie spieszy i nie ma czasu pogadac.", "Zgubienie drogi.", "Halas w miastach." },
			{ "Ludzie, co zbijaja ceny.", "Jak ktos targuje sie o grosze.", "Pusty rynek." },
			{ "Uciekajacy przeciwnicy.", "Jak Metin znika mi sprzed nosa.", "Jak nie ma z kim sie zmierzyc." },
			{ "Jak ktos znika z PT bez slowa.", "Klotnie w grupie.", "Jak ktos jest niemily bez powodu." },
		};
		static const char* const kAnnoyEn[V_COUNT][3] = {
			{ "When someone steals my mobs.", "When the exp won't move.", "When my weapon burns during an upgrade." },
			{ "When someone's in a hurry and has no time to talk.", "Getting lost.", "The noise in towns." },
			{ "People who undercut prices.", "When someone haggles over pennies.", "An empty market." },
			{ "Enemies that run away.", "When a Metin disappears right in front of me.", "When there's no one to fight." },
			{ "When someone leaves the party without a word.", "Arguing in the group.", "When someone's rude for no reason." },
		};
		std::string out = Pick(g, g.en ? kAnnoyEn[g.voice] : kAnnoy[g.voice], 3);
		if (g.s.unlucky && g.rng.Chance(50))
			Append(out, Txt(g, "I pech w dropie, jak dzisiaj.", "And bad luck with drops, like today."));
		return out;
	}

	inline std::string GenJoy(TGen& g)
	{
		static const char* const kJoy[V_COUNT][3] = {
			{ "Nowy poziom. Zawsze.", "Jak exp leci szybko.", "Udane ulepszenie." },
			{ "Nowe miejsca i ladne widoki.", "Spokojny wieczor w podrozy.", "Jak trafie na cos, czego nie znalem." },
			{ "Dobra transakcja.", "Pelna sakiewka.", "Jak towar schodzi od reki." },
			{ "Wygrana walka.", "Rozbity Metin.", "Dobry przeciwnik." },
			{ "Dobra ekipa i rozmowa.", "Jak ktos napisze, tak jak ty teraz.", "Wspolny exp ze znajomymi." },
		};
		static const char* const kJoyEn[V_COUNT][3] = {
			{ "A new level. Always.", "When the exp flies.", "A successful upgrade." },
			{ "New places and nice views.", "A quiet evening on the road.", "Finding something I didn't know." },
			{ "A good deal.", "A full purse.", "When stuff sells right away." },
			{ "A won fight.", "A broken Metin.", "A good opponent." },
			{ "A good team and a chat.", "When someone messages me, like you just did.", "Grinding together with friends." },
		};
		std::string out = Pick(g, g.en ? kJoyEn[g.voice] : kJoy[g.voice], 3);
		if (g.s.euphoria && g.rng.Chance(60))
			Append(out, Txt(g, "A dzis wyjatkowo, bo ulepszenie weszlo.", "And today especially, 'cause an upgrade went through."));
		return out;
	}

	inline std::string GenHypo(TGen& g)
	{
		const ETopic t = g.a ? g.a->topic : T_NONE;
		if (t == T_TRAVEL || t == T_NATURE || (g.a && g.a->concepts.Has(C_WHERE)))
		{
			static const char* const kTravel[V_COUNT][3] = {
				{ "Nie wiem. Pewnie tam, gdzie mozna cos osiagnac.", "Tam, gdzie sa najlepsze spoty.", "Gdzies, gdzie szybko rosnie poziom." },
				{ "Chyba gdzies daleko od miast. Lubie spokojne miejsca.", "W gory, na sam szczyt, i posiedzial.", "Gdzies, gdzie nikt jeszcze nie byl." },
				{ "Moze do jakiegos duzego miasta. Ciekawi mnie, jak wygladaja tamtejsze rynki.", "Tam, gdzie mozna dobrze zarobic.", "Na wielki targ gdzies za morzem." },
				{ "Tam, gdzie sa najmocniejsi przeciwnicy.", "Na jakas dzika pustynie, pelna potworow.", "Tam, gdzie jest jakies wyzwanie." },
				{ "Gdziekolwiek, byle ze znajomymi.", "Nad morze z cala ekipa.", "Tam, gdzie sa fajni ludzie." },
			};
			static const char* const kTravelEn[V_COUNT][3] = {
				{ "Dunno. Probably somewhere I can achieve something.", "Wherever the best spots are.", "Somewhere I level fast." },
				{ "Probably somewhere far from towns. I like quiet places.", "To the mountains, right to the top, and just sit there.", "Somewhere no one's been yet." },
				{ "Maybe to some big city. I'm curious what their markets look like.", "Wherever I can make good money.", "To a big market somewhere across the sea." },
				{ "Wherever the strongest enemies are.", "Some wild desert full of monsters.", "Wherever there's a challenge." },
				{ "Anywhere, as long as it's with friends.", "To the sea with the whole team.", "Wherever the cool people are." },
			};
			return Pick(g, g.en ? kTravelEn[g.voice] : kTravel[g.voice], 3);
		}
		static const char* const kHypo[V_COUNT][3] = {
			{ "Pewnie dalej bym expil, ale szybciej.", "Wzialbym to, co daje najwiecej expa.", "Nie wiem, pewnie cos, co mnie wzmocni." },
			{ "Chyba ruszylbym w droge i zobaczyl, co z tego wyjdzie.", "Zrobilbym cos zupelnie nowego, dla samej ciekawosci.", "Pewnie bym sie rozejrzal i zdecydowal na miejscu." },
			{ "Najpierw policzylbym, czy sie oplaca.", "Zainwestowalbym to w cos pewnego.", "Kupilbym tanio, sprzedal drogo. Jak zawsze." },
			{ "Zmierzylbym sie z czyms mocnym.", "Poszedlbym na najtrudniejszy loch.", "Zaryzykowalbym. Bez ryzyka nudno." },
			{ "Zebralbym ekipe i zrobil to razem.", "Zapytalbym znajomych, co o tym mysla.", "Zrobilbym to z kims, samemu to nie to samo." },
		};
		static const char* const kHypoEn[V_COUNT][3] = {
			{ "Probably keep grinding, just faster.", "I'd take whatever gives the most exp.", "Dunno, probably something that makes me stronger." },
			{ "I'd probably hit the road and see what happens.", "I'd do something totally new, just out of curiosity.", "I'd probably look around and decide on the spot." },
			{ "First I'd work out if it pays off.", "I'd invest it in something safe.", "Buy low, sell high. As always." },
			{ "I'd take on something strong.", "I'd go to the hardest dungeon.", "I'd take the risk. Without risk it's boring." },
			{ "I'd gather a team and do it together.", "I'd ask my friends what they think.", "I'd do it with someone, solo isn't the same." },
		};
		std::string out = Pick(g, g.en ? kHypoEn[g.voice] : kHypo[g.voice], 3);
		if (g.rng.Chance(30))
			out = Txt(g, "Hmm, ciekawe pytanie. ", "Hmm, interesting question. ") + out;
		return out;
	}

	inline std::string GenFact(TGen& g)
	{
		static const char* const kUnknown[V_COUNT][3] = {
			{ "Nie wiem, nie znam sie na tym.", "Nie mam pojecia.", "Nie wiem. Ja sie znam glownie na expie." },
			{ "Nie wiem, nigdy sie nad tym nie zastanawialem.", "Nie mam pojecia, ale brzmi ciekawie.", "Hmm, nie wiem. Ciekawe pytanie." },
			{ "Nie wiem. Ale jak da sie na tym zarobic, daj znac.", "Nie mam pojecia, to nie moja dzialka.", "Nie wiem, szczerze." },
			{ "Nie wiem, nie zaprzatam sobie tym glowy.", "Nie mam pojecia.", "Nie wiem. Zapytaj kogos madrzejszego." },
			{ "Nie wiem, ale moze ktos z ekipy bedzie wiedzial.", "Nie mam pojecia. A ty wiesz?", "Hmm, nie wiem. Powiesz mi?" },
		};
		static const char* const kUnknownEn[V_COUNT][3] = {
			{ "Dunno, I don't know much about that.", "No idea.", "Dunno. I mostly know about grinding." },
			{ "Dunno, never really thought about it.", "No idea, but it sounds interesting.", "Hmm, dunno. Interesting question." },
			{ "Dunno. But if there's money in it, let me know.", "No idea, not my area.", "Dunno, honestly." },
			{ "Dunno, I don't bother my head with that.", "No idea.", "Dunno. Ask someone smarter." },
			{ "Dunno, but maybe someone from the team knows.", "No idea. Do you know?", "Hmm, dunno. Will you tell me?" },
		};
		return Pick(g, g.en ? kUnknownEn[g.voice] : kUnknown[g.voice], 3);
	}

	inline std::string GenLikeAnswer(TGen& g, const TTopicPack* pack)
	{
		const std::string what = g.a ? g.a->object : std::string();
		const int level = LikeLevel(g, pack, what);
		static const char* const kLike[] = { "Lubie, czemu nie.", "Tak, calkiem lubie.", "Pewnie, ze tak.", "Lubie, choc bez przesady." };
		static const char* const kMeh[] = { "Tak sobie. Ani mnie to grzeje, ani ziebi.", "Bywa roznie. Nie mam zdania.", "Czasem tak, czasem nie." };
		static const char* const kDislike[] = { "Niezbyt, szczerze mowiac.", "Raczej nie, to nie dla mnie.", "Nie bardzo." };
		static const char* const kLikeEn[] = { "I like it, why not.", "Yeah, I like it quite a bit.", "Sure I do.", "I like it, but nothing crazy." };
		static const char* const kMehEn[] = { "So-so. Doesn't do much for me either way.", "Depends. I don't have an opinion.", "Sometimes yes, sometimes no." };
		static const char* const kDislikeEn[] = { "Not really, to be honest.", "Not really, it's not for me.", "Not much." };
		std::string body;
		if (level == 2)
			body = pack && pack->like[0] && *pack->like[0] && g.rng.Chance(50) ? PickField(g, pack->like, 2) : PBC_SAY2(g, kLike, kLikeEn);
		else if (level == 0)
			body = pack && pack->dislike[0] && *pack->dislike[0] && g.rng.Chance(50) ? PickField(g, pack->dislike, 2) :
					PBC_SAY2(g, kDislike, kDislikeEn);
		else
			body = PBC_SAY2(g, kMeh, kMehEn);
		if (g.a && g.a->concepts.Has(C_COLD) && level == 0)
			body = g.rng.Chance(50) ? Txt(g, "Raczej wole cieplejsza pogode.", "I'd rather have warmer weather.") :
					Txt(g, "Niezbyt, wole jak jest cieplej.", "Not really, I prefer it warmer.");
		g.reason = pack ? PickField(g, pack->why, 2) : std::string();
		return WithEcho(g, body, 55);
	}

	inline std::string GenChoice(TGen& g)
	{
		if (!g.a || g.a->object.empty() || g.a->objectB.empty())
			return GenFact(g);
		const u32 h = HashStr((g.a->object + "|" + g.a->objectB).c_str(), g.m.botPID * 97u + 13u);
		const std::string& pick = (h & 1) ? g.a->objectB : g.a->object;
		static const char* const kPick[] = {
			"Chyba $X.", "Zdecydowanie $X.", "Hmm... $X.", "$X, bez dwoch zdan.", "Raczej $X, ale to trudny wybor."
		};
		static const char* const kPickEn[] = {
			"Probably $X.", "Definitely $X.", "Hmm... $X.", "$X, no doubt.", "I'd say $X, but it's a tough choice."
		};
		std::string out = Pick(g, g.en ? kPickEn : kPick, 5);
		ReplaceAll(out, "$X", pick);
		CapitalizeFirst(out);
		g.reason = Txt(g, "Po prostu bardziej mi pasuje.", "It just suits me better.");
		return out;
	}

	inline std::string GenWeatherStatement(TGen& g)
	{
		const TConceptSet& c = g.a->concepts;
		if (c.Has(C_COLD))
		{
			static const char* const k[] = {
				"Tez mam takie wrazenie. Jakos ponuro dzisiaj.", "No, zimno. Az sie nie chce wychodzic z miasta.",
				"Brr, prawda. Ja bym juz siedzial przy ognisku.", "Tez czuje. Dobry dzien na cieply kocyk." };
			static const char* const kEn[] = {
				"I feel the same. Kinda gloomy today.", "Yeah, it's cold. Don't even wanna leave town.",
				"Brr, true. I'd be sitting by a campfire already.", "I feel it too. Good day for a warm blanket." };
			return PBC_SAY2(g, k, kEn);
		}
		if (c.Has(C_WARM))
		{
			static const char* const k[] = {
				"No, cieplo. Az chce sie gdzies pochodzic.", "Prawda, ladnie dzisiaj.",
				"Oby tak zostalo. Lubie jak jest slonecznie.", "Byle nie za goraco, bo w zbroi ciezko." };
			static const char* const kEn[] = {
				"Yeah, it's warm. Makes you wanna go for a walk.", "True, it's nice today.",
				"Hope it stays like this. I like it sunny.", "As long as it's not too hot, armor gets heavy." };
			return PBC_SAY2(g, k, kEn);
		}
		if (c.Has(C_RAIN))
		{
			static const char* const k[] = {
				"No, leje. Dobry dzien, zeby posiedziec w grze.", "Deszcz to idealna pogoda na granie.",
				"Oj, to nie wychodz nigdzie, lepiej pograjmy.", "Szaro i mokro. Klasyka." };
			static const char* const kEn[] = {
				"Yeah, it's pouring. Good day to stay in and play.", "Rain is perfect gaming weather.",
				"Oh, then don't go anywhere, let's play instead.", "Grey and wet. Classic." };
			return PBC_SAY2(g, k, kEn);
		}
		const TTopicPack* pack = TopicPackFor(g, T_WEATHER);
		return PickField(g, pack->react, 4);
	}

	// Ask back once in a while - the social voice more, the grinder less, a
	// bad mood never.
	inline void MaybeAskBack(TGen& g, const TTopicPack* pack, u32 chance)
	{
		if (!pack || g.Bad() || !g.askBack.empty())
			return;
		if (g.voice == V_SOCIAL) chance += 20;
		if (g.voice == V_WANDERER) chance += 10;
		if (g.voice == V_GRINDER) chance = chance > 15 ? chance - 15 : 0;
		if (g.tier == TIER_HOSTILE) return;
		if (!g.rng.Chance(chance))
			return;
		const std::string q = PickField(g, pack->ask, 2);
		if (q.empty())
			return;
		g.askBack = q;
		g.askBackKind = ASK_TOPIC;
		g.askBackTopic = (ETopic)pack->topic;
	}

	// The whole GENERAL_CONVERSATION answer.
	inline std::string GenGeneral(TGen& g)
	{
		const TAnalysis& a = *g.a;
		const TTopicPack* pack = TopicPackFor(g, a.topic);
		std::string out;

		switch (a.qtype)
		{
			case Q_DREAM: out = GenDream(g); MaybeAskBack(g, TopicPackFor(g, T_DREAMS), 40); return out;
			case Q_FEAR: out = GenFear(g); MaybeAskBack(g, TopicPackFor(g, T_FEAR), 35); return out;
			case Q_ANNOY: out = GenAnnoy(g); MaybeAskBack(g, TopicPackFor(g, T_ANNOY), 35); return out;
			case Q_JOY: out = GenJoy(g); MaybeAskBack(g, TopicPackFor(g, T_JOY), 35); return out;
			case Q_HYPO: out = GenHypo(g); MaybeAskBack(g, pack, 30); return out;
			case Q_CHOICE: out = GenChoice(g); MaybeAskBack(g, pack, 30); return out;
			default: break;
		}
		if (a.topic == T_DREAMS) { out = GenDream(g); MaybeAskBack(g, pack, 40); return out; }
		if (a.topic == T_FEAR) { out = GenFear(g); MaybeAskBack(g, pack, 35); return out; }
		if (a.topic == T_ANNOY) { out = GenAnnoy(g); MaybeAskBack(g, pack, 35); return out; }
		if (a.topic == T_JOY && a.qtype != Q_STATEMENT) { out = GenJoy(g); MaybeAskBack(g, pack, 35); return out; }

		// Topics where the game state says something true about the bot.
		if (a.topic == T_TIRED && (a.qtype != Q_STATEMENT || a.concepts.Has(C_YOU)))
		{
			if (g.LowHp())
				out = Txt(g, "Troche, i jeszcze HP mi siada. Zaraz odpoczne.", "A bit, and my HP's dropping too. Gonna rest soon.");
			else if (g.s.onlineMinutes > 180)
				out = g.rng.Chance(50) ? Txt(g, "Troche tak, siedze tu juz dobrych kilka godzin.", "A bit, been here a good few hours now.") :
						Txt(g, "No troche, dlugo juz dzis gram.", "Yeah a bit, been playing a long time today.");
			else if (g.Bad())
				out = Txt(g, "Troche. Jakos ciezki dzien.", "A bit. Kinda rough day.");
			else
				out = PickField(g, pack->view + g.voice, 1);
			return out;
		}
		if (a.topic == T_BORED && (a.qtype != Q_STATEMENT || a.concepts.Has(C_YOU)))
		{
			if (g.s.shopStanding)
				out = Txt(g, "Troche. Stanie przy straganie to nie jest najciekawsze zajecie.",
						"A bit. Standing at the shop isn't the most exciting thing.");
			else if (g.s.fishing)
				out = Txt(g, "Przy wedce? Troche, ale to taki przyjemny rodzaj nudy.", "With a rod? A bit, but it's the nice kind of boring.");
			else if (g.s.action == A_FIGHT)
				out = Txt(g, "Nie, akurat sie cos dzieje, walcze.", "No, something's actually happening, I'm fighting.");
			else
				out = PickField(g, pack->view + g.voice, 1);
			MaybeAskBack(g, pack, 30);
			return out;
		}
		if (a.topic == T_LONELY && a.qtype != Q_STATEMENT)
		{
			out = g.s.inParty ? std::string(Txt(g, "Nie, teraz akurat jestem z ekipa.", "No, I'm with a team right now.")) :
					PickField(g, pack->view + g.voice, 1);
			return out;
		}
		if (a.topic == T_DAYTIME && a.qtype != Q_LIKE && a.qtype != Q_FAVORITE)
		{
			if (g.s.hour >= 23 || g.s.hour < 5)
				out = g.rng.Chance(50) ? Txt(g, "No, pozno juz. A ja dalej gram.", "Yeah, it's late. And I'm still playing.") :
						Txt(g, "Noc juz, ale jakos nie chce mi sie konczyc.", "It's night already, but I don't feel like stopping.");
			else if (g.s.hour < 10)
				out = Txt(g, "Wczesnie jeszcze. Dobry moment, zeby spokojnie poexpic.", "Still early. Good time to grind in peace.");
			else if (g.s.hour >= 18)
				out = Txt(g, "Wieczor to najlepsza pora na granie.", "Evening's the best time to play.");
			else
				out = PickField(g, pack->react, 4);
			return out;
		}
		if (a.topic == T_LUCK && a.qtype != Q_STATEMENT)
		{
			if (g.s.unlucky)
				out = Txt(g, "Dzis raczej pech. Dawno nic dobrego nie wypadlo.", "Pretty unlucky today. Nothing good has dropped in a while.");
			else if (g.s.euphoria || g.Good())
				out = Txt(g, "Dzis akurat mam farta!", "I'm lucky today!");
			else
				out = PickField(g, pack->view + g.voice, 1);
			return out;
		}
		if (a.topic == T_FRIENDSHIP && a.concepts.Has(C_YOU) && a.qtype != Q_STATEMENT)
		{
			out = g.tier >= TIER_FRIEND ? std::string(Txt(g, "Mam. Ty tez sie do nich zaliczasz.", "I do. You're one of them.")) :
					PickField(g, pack->view + g.voice, 1);
			return out;
		}

		if (!pack)
		{
			// A preference question about something with no topic of its own:
			// "lubisz kaktusy?", "co myslisz o polityce?".
			if (a.qtype == Q_LIKE || a.qtype == Q_WANT)
				return GenLikeAnswer(g, NULL);
			if (a.qtype == Q_DISLIKE)
			{
				static const char* const k[] = { "Nie lubie, jak ktos kradnie moby.", "Nie znosze czekania.", "Nie lubie pospiechu." };
				static const char* const kEn[] = { "I don't like it when someone steals mobs.", "I can't stand waiting.", "I don't like being rushed." };
				return PBC_SAY2(g, k, kEn);
			}
			if (a.qtype == Q_OPINION)
			{
				static const char* const k[] = {
					"Nie mam wyrobionego zdania, ale brzmi ciekawie.", "Szczerze? Nie zastanawialem sie nad tym.",
					"Ciezko powiedziec. Kazdy ma swoje zdanie." };
				static const char* const kEn[] = {
					"I don't have a firm opinion, but it sounds interesting.", "Honestly? Never thought about it.",
					"Hard to say. Everyone has their own opinion." };
				return WithEcho(g, PBC_SAY2(g, k, kEn), 50);
			}
			if (a.qtype == Q_CAN)
			{
				const int r = OpinionRoll(g, a.object, 5);
				static const char* const kYes[] = { "Troche umiem, ale bez szalu.", "Cos tam umiem." };
				static const char* const kNo[] = { "Nie, raczej nie umiem.", "Chyba nie. Nigdy nie probowalem." };
				static const char* const kYesEn[] = { "I can a bit, nothing special.", "I know a thing or two." };
				static const char* const kNoEn[] = { "No, I can't really.", "Probably not. Never tried." };
				return WithEcho(g, r < 40 ? PBC_SAY2(g, kYes, kYesEn) : PBC_SAY2(g, kNo, kNoEn), 50);
			}
			if (a.qtype == Q_FAVORITE)
			{
				static const char* const k[] = { "Nie mam jednego ulubionego.", "Ciezko wybrac jedno.", "Chyba nie mam ulubionego." };
				static const char* const kEn[] = { "I don't have one favorite.", "Hard to pick just one.", "I don't think I have a favorite." };
				return PBC_SAY2(g, k, kEn);
			}
			if (a.qtype == Q_WHAT_LIKE)
				return PickField(g, TopicPackFor(g, T_HOBBY)->view + g.voice, 1);
			return GenFact(g);
		}

		switch (a.qtype)
		{
			case Q_STATEMENT:
				if (a.topic == T_WEATHER || a.topic == T_SEASON)
					out = a.topic == T_WEATHER ? GenWeatherStatement(g) : PickField(g, pack->react, 4);
				else if (a.topic == T_FEELINGS && a.concepts.Has(C_SAD))
				{
					static const char* const k[] = { "Oj, przykro mi. Chcesz pogadac?", "Trzymaj sie. Bedzie lepiej.", "Kiepsko... Moze troche gry poprawi humor?" };
					static const char* const kEn[] = { "Aw, sorry. Wanna talk?", "Hang in there. It'll get better.", "That sucks... Maybe some playing will cheer you up?" };
					out = PBC_SAY2(g, k, kEn);
				}
				else if (a.topic == T_FEELINGS && a.concepts.Has(C_HAPPY))
				{
					static const char* const k[] = { "To super! Ciesze sie.", "Oby tak dalej!", "Fajnie to slyszec." };
					static const char* const kEn[] = { "That's awesome! Happy for you.", "Keep it up!", "Nice to hear." };
					out = PBC_SAY2(g, k, kEn);
				}
				else
					out = PickField(g, pack->react, 4);
				MaybeAskBack(g, pack, 30);
				return out;
			case Q_LIKE:
			case Q_WANT:
				out = GenLikeAnswer(g, pack);
				MaybeAskBack(g, pack, 30);
				return out;
			case Q_DISLIKE:
			{
				const std::string d = PickField(g, pack->dislike, 2);
				out = d.empty() ? GenAnnoy(g) : d;
				return out;
			}
			case Q_FAVORITE:
			{
				const std::string fav = PickFavorite(g, pack);
				if (fav.empty())
					out = PickField(g, pack->view + g.voice, 1);
				else
				{
					static const char* const k[] = { "Chyba $X.", "$X, zdecydowanie.", "Hmm... $X.", "Lubie $X." };
					static const char* const kEn[] = { "Probably $X.", "$X, definitely.", "Hmm... $X.", "I like $X." };
					out = Pick(g, g.en ? kEn : k, 4);
					ReplaceAll(out, "$X", fav);
					CapitalizeFirst(out);
				}
				MaybeAskBack(g, pack, 35);
				return out;
			}
			case Q_WHAT_LIKE:
			case Q_OPINION:
			case Q_MIRROR:
			case Q_OPEN:
				out = PickField(g, pack->view + g.voice, 1);
				if (out.empty())
					out = PickField(g, pack->react, 4);
				if (a.qtype == Q_MIRROR && g.rng.Chance(40))
					out = Txt(g, "Ja? ", "Me? ") + out;
				g.reason = PickField(g, pack->why, 2);
				MaybeAskBack(g, pack, a.qtype == Q_MIRROR ? 10 : 30);
				return out;
			case Q_CAN:
			{
				const int r = OpinionRoll(g, a.object, 5);
				out = r < 40 ? Txt(g, "Troche umiem, ale bez szalu.", "I can a bit, nothing special.") :
						Txt(g, "Nie, raczej nie. Nigdy nie mialem do tego glowy.", "No, not really. Never had a head for it.");
				return WithEcho(g, out, 50);
			}
			case Q_EVER:
			{
				static const char* const k[] = { "Kiedys moze, ale nie pamietam juz dokladnie.", "Chyba nie. Ale chcialbym.", "Hmm, nie przypominam sobie." };
				static const char* const kEn[] = { "Maybe once, but I don't remember exactly.", "Don't think so. But I'd like to.", "Hmm, I don't remember that." };
				out = PBC_SAY2(g, k, kEn);
				MaybeAskBack(g, pack, 40);
				return out;
			}
			case Q_KNOW:
			{
				static const char* const k[] = { "Znam cos tam, ale nie pamietam nazw.", "Kilka by sie znalazlo, ale z glowy nie powiem.", "Nie za bardzo sie znam, szczerze." };
				static const char* const kEn[] = { "I know some, but I don't remember the names.", "I could name a few, but not off the top of my head.", "I don't really know much about it, honestly." };
				out = PBC_SAY2(g, k, kEn);
				MaybeAskBack(g, pack, 40);
				return out;
			}
			case Q_FACT:
				return GenFact(g);
			default:
				return PickField(g, pack->react, 4);
		}
	}
}

#endif
