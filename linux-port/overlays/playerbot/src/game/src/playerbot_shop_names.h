// Rendered by linux-port/overlays/playerbot/tools/generate_shop_names.py from Iwakura's list
// of shop names (data/iwakura_nazwy_sklepow.txt). DO NOT EDIT; edit the list and re-run.
//
// Every name is his, byte for byte in CP1250 - the engine's own encoding for a shop
// name (player.ikashop_offlineshop.name is cp1250_polish_ci) - and every one is what
// ikashop::CShopManager::ParseShopName leaves of it unchanged: at most SHOP_SIGN_MAX_LEN
// characters after EscapeString, nothing has_proper_characters refuses, no word from
// world.banword. A name that failed was trimmed of the decoration on both of its ends
// when that was enough and left out when it was not; both are listed below, so the list
// can be fixed where it is written. Which name a counter gets is decided in
// playerbot_shop_name_rules.h.
//
// szNameEn is its English twin, for a person whose client reads English, from
// data/iwakura_nazwy_sklepow_en.txt: ours, because his list has no English, with
// any item it names by the official English name (playerbot_names_en.tsv). It never
// goes through ParseShopName - the core puts it straight into that person's packet -
// so it is only held to SHOP_SIGN_MAX_LEN, plain ASCII and world.banword.
//
// Trimmed to fit a sign:
//   [NEUTRALNE] "-> Prawdziwa kobieta ZaPrAsZa :) <-" -> "Prawdziwa kobieta ZaPrAsZa :)"
//   [RYBY] "@@@@@@@@@@@@@@ RYBY! @@@@@@@@@@@@@@" -> "@@@@@@@@@@@@ RYBY! @@@@@@@@@@@@"
//   [EKWIPUNEK] "@@@@@ Kute w bolach u kowala @@@@@" -> "@@@@ Kute w bolach u kowala @@@@"
//   [EKWIPUNEK] "@@@@@ Gotowe do walki itemy @@@@@" -> "@@@@ Gotowe do walki itemy @@@@"
//   [EKWIPUNEK] ">>> Itemy za ktore sprzedasz nerke <<<" -> "Itemy za ktore sprzedasz nerke"
//
// Left out - the engine would refuse or cut them:
//   [NEUTRALNE] "Okradli mnie, pomoz stanac na nogi :(" (silnik pokazalby 32 z 37 znakow)
//   [NEUTRALNE] "T A N I E J N I E Z N A J D Z I E S Z" (silnik pokazalby 32 z 37 znakow)
//   [KU] "Nauka czytania dla opornych" (zakazane slowo "porn")
//   [KU] "AURA MIECZA CZAROWANE SILNE CIALO" (silnik pokazalby 32 z 33 znakow)
//   [RYBY] "TeRybieOsciIKowalNieZrobiPoZlosci" (silnik pokazalby 32 z 33 znakow)
//   [ULEPSZACZE] "Kawalek klejnotu i zardzewiale ostrze" (silnik pokazalby 32 z 37 znakow)
//   [ULEPSZACZE] "ZARDZEWIALE OSTRZE | CZARNY UNIFORM" (silnik pokazalby 32 z 35 znakow)
//   [ULEPSZACZE] "Nie biegaj po mapach - kup tutaj!" (silnik pokazalby 32 z 33 znakow)
//   [EKWIPUNEK] "Rzeczy ktore kowal cudem oszczedzil!" (silnik pokazalby 32 z 36 znakow)
#ifndef PLAYERBOT_SHOP_NAMES_H
#define PLAYERBOT_SHOP_NAMES_H
#include <cstddef>
#include <cstdint>

namespace playerbot_shop_names
{
	// His seven lists.
	enum ESignKind : uint8_t
	{
		SIGN_KIND_NEUTRAL,	// [NEUTRALNE]
		SIGN_KIND_BOOKS,	// [KU]
		SIGN_KIND_FISH,	// [RYBY]
		SIGN_KIND_SCRAP,	// [DO SPALENIA]
		SIGN_KIND_MATERIALS,	// [ULEPSZACZE]
		SIGN_KIND_OTHER,	// [INNE]
		SIGN_KIND_GEAR,	// [EKWIPUNEK]
		SIGN_KIND_COUNT
	};

	// What a name says about the goods, so that it is drawn only over a counter that
	// has them. The one number some of them need is aGoods[0].adwVnum[0].
	enum ESignNeed : uint8_t
	{
		SIGN_NEED_NONE,	// anything of its list
		SIGN_NEED_ALL_GOODS,	// a line of every group in aGoods
		SIGN_NEED_ANY_GOODS,	// a line of one of them
		SIGN_NEED_ORES,	// ore, raw or smelted
		SIGN_NEED_BOOK_SKILLS,	// a book of one of the skills in aGoods[0]
		SIGN_NEED_BOOK_CLASSES,	// books of every class in the mask (warrior 1, ninja 2, sura 4, shaman 8)
		SIGN_NEED_TOP_GEAR,	// a weapon or armour at +7 to +9
		SIGN_NEED_LOW_GEAR,	// a weapon or armour under that level
		SIGN_NEED_HIGH_GEAR,	// a weapon or armour from that level
		SIGN_NEED_BODY_ARMOUR,	// a body armour
		SIGN_NEED_GEAR_KINDS,	// a weapon, a body armour and a shield
		SIGN_NEED_FIRST_VILLAGE,	// the counter stands in a first village
		SIGN_NEED_COUNT
	};

	struct TSignGoods { uint32_t adwVnum[4]; };
	struct TSignName
	{
		uint8_t bKind;
		uint8_t bNeed;
		uint8_t bGoods;		// groups used in aGoods
		const char* szName;	// CP1250
		TSignGoods aGoods[3];
		const char* szNameEn;	// ASCII, the English twin
	};

	// The numbers in the rules at the head of his list.
	const size_t SIGN_MAX_LEN = 32;
	const int SIGN_NEUTRAL_OVERRIDE_PERCENT = 33;	// "istnieje 33% szans"
	const int SIGN_BONUS_MIN_PCT = 150;	// a bonus line named over +7..+9 gear
	const int SIGN_STONES_MIN_PCT = 140;	// "KD" named over it
	const int SIGN_TOP_GEAR_MIN_PLUS = 7;
	const int SIGN_SCRAP_MAX_PLUS = 3;
	const char* const SIGN_GEAR_SUFFIXES[] = { "TANIO", "OKAZJA" };
	const char* const SIGN_GOODS_SUFFIXES[] = { "Tanio", "TANIO", "tanio", "Okazja", "OKAZJA", "okazja" };

	const TSignName SIGN_NAMES[] = {
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "TANIEJ JU\xAF NIE B\xCA" "DZIE", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "IT WON'T GET CHEAPER" },	// "TANIEJ JUZ NIE BEDZIE"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, ">> TANIO TANIEJ NAJTANIEJ <<", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, ">> CHEAP CHEAPER CHEAPEST <<" },	// ">> TANIO TANIEJ NAJTANIEJ <<"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "TANIEJ NI\xAF OBOK >>>>>", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "CHEAPER THAN NEXT DOOR >>>>>" },	// "TANIEJ NIZ OBOK >>>>>"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Brakuje mi kilku yang\xF3w... :(", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Short a few yang... :(" },	// "Brakuje mi kilku yangow... :("
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Bieda a\xBF piszczy, kup co\x9C :(", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Dirt poor, buy something :(" },	// "Bieda az piszczy, kup cos :("
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Nie uwierzysz w te ceny :O", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Unbelievable prices :O" },	// "Nie uwierzysz w te ceny :O"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "<<<<<< TANIEJ NI\xAF OBOK", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "<<<<<< CHEAPER THAN NEXT DOOR" },	// "<<<<<< TANIEJ NIZ OBOK"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "koncze gre kup szypko!!!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "quitting the game buy fast!!!" },	// "koncze gre kup szypko!!!"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "@@@ Wybocilem to wszystko @@@", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@@@ Botted all of this @@@" },	// "@@@ Wybocilem to wszystko @@@"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "\x8Cmiecioszki", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Bits and junk" },	// "Smiecioszki"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Wszystko i nic", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Everything and nothing" },	// "Wszystko i nic"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "@@@@@@@@@@@@@@@@@@@@", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@@@@@@@@@@@@@@@@@@@@" },	// "@@@@@@@@@@@@@@@@@@@@"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "@@@@@ ZBIERAM NA \x8CLUB @@@@@", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@@@@@ SAVING FOR A WEDDING @@@@@" },	// "@@@@@ ZBIERAM NA SLUB @@@@@"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Zajrzyj z ciekawo\x9C" "ci ;)", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Take a look, just curious ;)" },	// "Zajrzyj z ciekawosci ;)"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Sklep z r\xF3\xBFno\x9C" "ciami", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Odds and ends shop" },	// "Sklep z roznosciami"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Do wyboru Do koloru", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Pick Any Colour" },	// "Do wyboru Do koloru"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Z0B4CZ S4M C0 TU M4M", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "C0M3 533 WH4T 1 H4V3" },	// "Z0B4CZ S4M C0 TU M4M"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "SPADEK CEN ULTRA PROMOCJA!!!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "PRICES DOWN ULTRA SALE!!!" },	// "SPADEK CEN ULTRA PROMOCJA!!!"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Taniej ni\xBF w saturnie!!<<<", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Cheaper than the mall!!<<<" },	// "Taniej niz w saturnie!!<<<"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Z A P R A S Z A M", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "W E L C O M E" },	// "Z A P R A S Z A M"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "GG 52871243 ALLEGRO", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "GG 52871243 ALLEGRO" },	// "GG 52871243 ALLEGRO"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Itemy na miar\xEA twoich yang\xF3w", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Items to fit your yang" },	// "Itemy na miare twoich yangow"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "TANIO :)", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "CHEAP :)" },	// "TANIO :)"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "T A N I O S Z K A", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "C H E A P I E S" },	// "T A N I O S Z K A"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "CUDA I NIEWIDY!!!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "WONDERS AND MARVELS!!!" },	// "CUDA I NIEWIDY!!!"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "TANIOOOOOOOOOOOOOOOOOOO", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "CHEEEEEEEEEEEEEEEEEEEAP" },	// "TANIOOOOOOOOOOOOOOOOOOO"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "WSPOMOZ CHOREGO WOJTKA :(", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "HELP POOR SICK WOJTEK :(" },	// "WSPOMOZ CHOREGO WOJTKA :("
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "ODDAM TANIO", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "GOING CHEAP" },	// "ODDAM TANIO"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "ZBIERAM YANGI NA DOZO", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "SAVING UP YANG" },	// "ZBIERAM YANGI NA DOZO"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Zbieractwo to moja pasja", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Collecting is my passion" },	// "Zbieractwo to moja pasja"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Wybuduj sobie pa\xB3" "ac", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Build yourself a palace" },	// "Wybuduj sobie palac"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Zbierane na nielegalu", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Farmed off the books" },	// "Zbierane na nielegalu"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Bot dzia\xB3" "a\xB3 ca\xB3\xB9 noc", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "The bot ran all night" },	// "Bot dzialal cala noc"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Prawdziwa kobieta ZaPrAsZa :)", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "A ReAl WoMaN iNvItEs :)" },	// "Prawdziwa kobieta ZaPrAsZa :)"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "WSZYSTKO CZEGO POTRZEBUJESZ", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "EVERYTHING YOU NEED" },	// "WSZYSTKO CZEGO POTRZEBUJESZ"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Dobry Sklep 420 Tanio", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Good Shop 420 Cheap" },	// "Dobry Sklep 420 Tanio"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, ".........ZAPRASZAM.........", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, ".........WELCOME........." },	// ".........ZAPRASZAM........."
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "ZBIERAM NA BIA\xA3\xA5 PER\xA3\xCA!!!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "SAVING FOR A White Pearl!!!" },	// "ZBIERAM NA BIALA PERLE!!!"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "@@@ CHORY BRAT PROSI O KUPNO @@@", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@@@ SICK BRO ASKS YOU TO BUY @@@" },	// "@@@ CHORY BRAT PROSI O KUPNO @@@"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "MOJA C\xD3RKA STUDIUJE PRAWO", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "MY DAUGHTER STUDIES LAW" },	// "MOJA CORKA STUDIUJE PRAWO"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "WYPRZEDA\xAF MAGAZYNU <<<<", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "WAREHOUSE CLEARANCE <<<<" },	// "WYPRZEDAZ MAGAZYNU <<<<"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Nie patrz na ceny, kupuj!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Don't look at prices, buy!" },	// "Nie patrz na ceny, kupuj!"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Promocja u Janusza", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Janusz's special offer" },	// "Promocja u Janusza"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Czyszczenie plecaka", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Cleaning out my bag" },	// "Czyszczenie plecaka"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Na robaki do rybaka...", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "For the fisherman's worms..." },	// "Na robaki do rybaka..."
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Sprzedam Opla (tanio)", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Selling my Opel (cheap)" },	// "Sprzedam Opla (tanio)"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "ZBIERAM NA BOJA!!!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "SAVING FOR A BOJA!!!" },	// "ZBIERAM NA BOJA!!!"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Brakuje mi na FMS", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Saving for an FMS" },	// "Brakuje mi na FMS"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "xXx T A N I O xXx", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "xXx C H E A P xXx" },	// "xXx T A N I O xXx"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "$ $ $ $ $ $ $ $ $ $ $", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "$ $ $ $ $ $ $ $ $ $ $" },	// "$ $ $ $ $ $ $ $ $ $ $"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "~~~ Z A P R A S Z A M ~~~", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "~~~ W E L C O M E ~~~" },	// "~~~ Z A P R A S Z A M ~~~"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "!!! NIE KLIKAJ TUTAJ !!!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "!!! DON'T CLICK HERE !!!" },	// "!!! NIE KLIKAJ TUTAJ !!!"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "B\xB3\xB9" "d cenowy! Wbija\xE6!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Price error! Get in!" },	// "Blad cenowy! Wbijac!"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Szybki sell i wracam expi\xE6", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Quick sell and back to exp" },	// "Szybki sell i wracam expic"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Wszystko za bezcen", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Everything for a song" },	// "Wszystko za bezcen"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "@@@@ Wyprzeda\xBF gara\xBFowa @@@@", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@@@@ Garage sale @@@@" },	// "@@@@ Wyprzedaz garazowa @@@@"
		{ SIGN_KIND_NEUTRAL, SIGN_NEED_NONE, 0, "Resztki z dropka", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Drop leftovers" },	// "Resztki z dropka"
		{ SIGN_KIND_BOOKS, SIGN_NEED_NONE, 0, "Ksi\xB9\xBFki do poczytania przed snem", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Bedtime reading" },	// "Ksiazki do poczytania przed snem"
		{ SIGN_KIND_BOOKS, SIGN_NEED_NONE, 0, "Wyprzedaz KU!!!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Skill book sale!!!" },	// "Wyprzedaz KU!!!"
		{ SIGN_KIND_BOOKS, SIGN_NEED_NONE, 0, "@@@ KSIEGI UMIEJETNOSCI @@@", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@@@ SKILL BOOKS @@@" },	// "@@@ KSIEGI UMIEJETNOSCI @@@"
		{ SIGN_KIND_BOOKS, SIGN_NEED_NONE, 0, "Czytaj i wbijaj na G <3", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Read and level to G <3" },	// "Czytaj i wbijaj na G <3"
		{ SIGN_KIND_BOOKS, SIGN_NEED_NONE, 0, "Wszystkie KU taniej niz obok>>>", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "All skill books cheaper>>>" },	// "Wszystkie KU taniej niz obok>>>"
		{ SIGN_KIND_BOOKS, SIGN_NEED_BOOK_CLASSES, 1, "@@@@@@ KU Woj Sura Ninja @@@@@", { { { 7, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@@ Books: Warrior Sura Ninja @@" },	// "@@@@@@ KU Woj Sura Ninja @@@@@"
		{ SIGN_KIND_BOOKS, SIGN_NEED_NONE, 0, "Ksi\xEAgarnia u Mirka :)", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Mirek's bookshop :)" },	// "Ksiegarnia u Mirka :)"
		{ SIGN_KIND_BOOKS, SIGN_NEED_NONE, 0, "Tylko dobre KU bez smieci!!!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Only good books, no junk!!!" },	// "Tylko dobre KU bez smieci!!!"
		{ SIGN_KIND_BOOKS, SIGN_NEED_NONE, 0, "@@@@@@@ Hurtownia KU @@@@@@@", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@@@@@@@ Book wholesale @@@@@@@" },	// "@@@@@@@ Hurtownia KU @@@@@@@"
		{ SIGN_KIND_BOOKS, SIGN_NEED_NONE, 0, "Tajna wiedza z przeceny ;-;", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Secret knowledge, discounted ;-;" },	// "Tajna wiedza z przeceny ;-;"
		{ SIGN_KIND_BOOKS, SIGN_NEED_NONE, 0, "!!! KU Z DROPKA !!!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "!!! BOOKS FROM DROPS !!!" },	// "!!! KU Z DROPKA !!!"
		{ SIGN_KIND_BOOKS, SIGN_NEED_NONE, 0, "dla t\xEAgich g\xB3\xF3w", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "for great minds" },	// "dla tegich glow"
		{ SIGN_KIND_BOOKS, SIGN_NEED_NONE, 0, "Czytelnia u Micha\xB3" "a", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Michal's reading room" },	// "Czytelnia u Michala"
		{ SIGN_KIND_BOOKS, SIGN_NEED_NONE, 0, "Ksiazki m\xB9" "drzejsze od Ciebie ;]", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Books smarter than you ;]" },	// "Ksiazki madrzejsze od Ciebie ;]"
		{ SIGN_KIND_BOOKS, SIGN_NEED_FIRST_VILLAGE, 0, "Biblioteka Publiczna M1", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "M1 Public Library" },	// "Biblioteka Publiczna M1"
		{ SIGN_KIND_BOOKS, SIGN_NEED_BOOK_CLASSES, 1, "KU dla ka\xBF" "dej klasy postaci", { { { 15, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Books for every class" },	// "KU dla kazdej klasy postaci"
		{ SIGN_KIND_BOOKS, SIGN_NEED_NONE, 0, "@@@ Zestaw do wbicia skilli @@@", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@@@ Skill levelling kit @@@" },	// "@@@ Zestaw do wbicia skilli @@@"
		{ SIGN_KIND_BOOKS, SIGN_NEED_NONE, 0, "$$$ Skup i Sprzeda\xBF KU $$$", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "$$$ Books bought and sold $$$" },	// "$$$ Skup i Sprzedaz KU $$$"
		{ SIGN_KIND_BOOKS, SIGN_NEED_NONE, 0, "TANIE | KSI\xCAGI | UMIEJ\xCATNO\x8C" "CI", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "CHEAP | SKILL | BOOKS" },	// "TANIE | KSIEGI | UMIEJETNOSCI"
		{ SIGN_KIND_FISH, SIGN_NEED_NONE, 0, "<<< MALZE PERLY RYBKI >>>", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "<<< Clam, pearls, fish >>>" },	// "<<< MALZE PERLY RYBKI >>>"
		{ SIGN_KIND_FISH, SIGN_NEED_ANY_GOODS, 1, "Malze taniej ni\xBF obok >>>>>", { { { 27987, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Clam cheaper than next door >>>" },	// "Malze taniej niz obok >>>>>"
		{ SIGN_KIND_FISH, SIGN_NEED_NONE, 0, "\x8Cmierdzi ryb\xB9, ale tanio", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Smells of fish, but cheap" },	// "Smierdzi ryba, ale tanio"
		{ SIGN_KIND_FISH, SIGN_NEED_NONE, 0, "@@@@@@@@@@@@ RYBY! @@@@@@@@@@@@", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@@@@@@@@@@@@ FISH! @@@@@@@@@@@@" },	// "@@@@@@@@@@@@ RYBY! @@@@@@@@@@@@"
		{ SIGN_KIND_FISH, SIGN_NEED_NONE, 0, "Sklep Rybny u Kamila ;]", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Kamil's fish shop ;]" },	// "Sklep Rybny u Kamila ;]"
		{ SIGN_KIND_FISH, SIGN_NEED_NONE, 0, "<3 Sklep Rybny u Wiktori ;p", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "<3 Wiktoria's fish shop ;p" },	// "<3 Sklep Rybny u Wiktori ;p"
		{ SIGN_KIND_FISH, SIGN_NEED_NONE, 0, "~~~~~~ Dary morza ~~~~~~", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "~~~~~~ Gifts of the sea ~~~~~~" },	// "~~~~~~ Dary morza ~~~~~~"
		{ SIGN_KIND_FISH, SIGN_NEED_NONE, 0, "/// Rybak zaprasza na zakupy \\\\\\", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "/// Fisherman welcomes you \\\\\\" },	// "/// Rybak zaprasza na zakupy \\\"
		{ SIGN_KIND_FISH, SIGN_NEED_NONE, 0, "Z\xB3\xF3w to sam albo kup tutaj!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Catch it yourself or buy here!" },	// "Zlow to sam albo kup tutaj!"
		{ SIGN_KIND_FISH, SIGN_NEED_NONE, 0, "Drop z w\xEA" "dki +20 :O !!!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Drop from Fishing Pole+20 :O!!!" },	// "Drop z wedki +20 :O !!!"
		{ SIGN_KIND_FISH, SIGN_NEED_NONE, 0, "RYBY | FARBY | MA\xA3ZE", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "FISH | DYES | SHELLS" },	// "RYBY | FARBY | MALZE"
		{ SIGN_KIND_FISH, SIGN_NEED_NONE, 0, "@ TANIE @ OWOCE @ MORZA @", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@ CHEAP @ SEA @ FOOD @" },	// "@ TANIE @ OWOCE @ MORZA @"
		{ SIGN_KIND_FISH, SIGN_NEED_NONE, 0, "@@@ RYBY TANIO @@@", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@@@ FISH CHEAP @@@" },	// "@@@ RYBY TANIO @@@"
		{ SIGN_KIND_FISH, SIGN_NEED_ANY_GOODS, 2, "Traf bia\xB3\xB9 per\xB3\xEA!", { { { 27987, 0, 0, 0 } }, { { 27992, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Hit a White Pearl!" },	// "Traf biala perle!"
		{ SIGN_KIND_FISH, SIGN_NEED_ANY_GOODS, 1, "MA\xA3\xAF" "E Z NOCNEGO BOC---- \xA3OWIENIA", { { { 27987, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Clam from night b---- fishing" },	// "MALZE Z NOCNEGO BOC---- LOWIENIA"
		{ SIGN_KIND_FISH, SIGN_NEED_ANY_GOODS, 2, "@ @ @ PER\xA3Y B\xCA" "D\xA5 TWOJE @ @ @", { { { 27987, 0, 0, 0 } }, { { 27992, 27993, 27994, 0 } }, { { 0, 0, 0, 0 } } }, "@ @ @ White Pearl is yours @ @ @" },	// "@ @ @ PERLY BEDA TWOJE @ @ @"
		{ SIGN_KIND_FISH, SIGN_NEED_ANY_GOODS, 1, "\x8CWIE\xAFY PO\xA3\xD3W KARPIA", { { { 27806, 27822, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Fresh Carp catch" },	// "SWIEZY POLOW KARPIA"
		{ SIGN_KIND_FISH, SIGN_NEED_NONE, 0, "| | Wszystko | z | wody | |", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "| | All | from | the | water | |" },	// "| | Wszystko | z | wody | |"
		{ SIGN_KIND_SCRAP, SIGN_NEED_NONE, 0, "\x8Cmieci dla Kowala", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Junk for the Blacksmith" },	// "Smieci dla Kowala"
		{ SIGN_KIND_SCRAP, SIGN_NEED_NONE, 0, "Kowal \xB3ysol to lubi xD", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "The bald Blacksmith loves it xD" },	// "Kowal lysol to lubi xD"
		{ SIGN_KIND_SCRAP, SIGN_NEED_NONE, 0, "<<< TANI SZROT POD KOWALA >>>", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "<<< CHEAP SCRAP FOR SMITH >>>" },	// "<<< TANI SZROT POD KOWALA >>>"
		{ SIGN_KIND_SCRAP, SIGN_NEED_NONE, 0, "Nakarm \xB3ysego", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Feed the bald guy" },	// "Nakarm lysego"
		{ SIGN_KIND_SCRAP, SIGN_NEED_NONE, 0, "Tanie itemki na spalenie", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Cheap items to burn" },	// "Tanie itemki na spalenie"
		{ SIGN_KIND_SCRAP, SIGN_NEED_NONE, 0, "420 Jaranie u kowala", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "420 Burning at the smith" },	// "420 Jaranie u kowala"
		{ SIGN_KIND_SCRAP, SIGN_NEED_NONE, 0, "@ @ @ KOWAL JU\xAF CZEKA @ @ @", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@ @ @ THE SMITH IS WAITING @ @ @" },	// "@ @ @ KOWAL JUZ CZEKA @ @ @"
		{ SIGN_KIND_SCRAP, SIGN_NEED_NONE, 0, "Tani z\xB3om +0 +1 +2 +3", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Cheap scrap +0 +1 +2 +3" },	// "Tani zlom +0 +1 +2 +3"
		{ SIGN_KIND_SCRAP, SIGN_NEED_NONE, 0, "Wszystko do pieca >>>>>", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "All of it to the furnace >>>>>" },	// "Wszystko do pieca >>>>>"
		{ SIGN_KIND_SCRAP, SIGN_NEED_NONE, 0, "KUP Z\xA3OM ZBIERAM NA OPLA", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "BUY SCRAP, SAVING FOR AN OPEL" },	// "KUP ZLOM ZBIERAM NA OPLA"
		{ SIGN_KIND_SCRAP, SIGN_NEED_NONE, 0, "Testuj szcz\xEA\x9C" "cie u Kowala", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Try your luck at the Blacksmith" },	// "Testuj szczescie u Kowala"
		{ SIGN_KIND_SCRAP, SIGN_NEED_NONE, 0, "JAK POCIERA CZO\xA3O TO KUJ!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "IF HE RUBS HIS HEAD, REFINE!" },	// "JAK POCIERA CZOLO TO KUJ!"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, "KAWALKI KLEJONU | ULEPY Z M2", { { { 30021, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Piece of Gem | M2 UPGRADES" },	// "KAWALKI KLEJONU | ULEPY Z M2"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, "KAWALKI KLEJNOTU NAJTANIEJ", { { { 30021, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Piece of Gem, CHEAPEST" },	// "KAWALKI KLEJNOTU NAJTANIEJ"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, "@@@@ KSI\xCAGI KL\xA5TW @@@@", { { { 30047, 30080, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@@@@ Curse Book @@@@" },	// "@@@@ KSIEGI KLATW @@@@"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, "$$$ KSIEGI KLATW DO BIOLOGA $$$", { { { 30047, 30080, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "$$$ Curse Book for Biologist $$$" },	// "$$$ KSIEGI KLATW DO BIOLOGA $$$"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, "Z\xCA" "BY ORKA Z\xCA" "BY ORKA Z\xCA" "BY ORKA", { { { 30006, 30077, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Orc Tooth Orc Tooth Orc Tooth" },	// "ZEBY ORKA ZEBY ORKA ZEBY ORKA"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, "Z\xCA" "BY ORKA TANIO OKAZJA!!!", { { { 30006, 30077, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Orc Tooth CHEAP BARGAIN!!!" },	// "ZEBY ORKA TANIO OKAZJA!!!"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_NONE, 0, "Ulepki z m2 i doliny", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Upgrades from M2 and the valley" },	// "Ulepki z m2 i doliny"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_NONE, 0, ">>>Ulepki prosto z dropka<<<", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, ">>>Upgrades fresh from drops<<<" },	// ">>>Ulepki prosto z dropka<<<"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_NONE, 0, "Tanie ulepy niebocone!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Cheap upgrades, not botted!" },	// "Tanie ulepy niebocone!"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_NONE, 0, "ZR\xD3" "B SOBIE EQ +9", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "MAKE YOURSELF +9 GEAR" },	// "ZROB SOBIE EQ +9"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ALL_GOODS, 2, "Pajecze sieci i oczy pajaka", { { { 30104, 30109, 0, 0 } }, { { 30057, 30162, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Desert Spider Web + Spider Eyes" },	// "Pajecze sieci i oczy pajaka"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, "Oczy paj\xB9ka tanio!", { { { 30057, 30162, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Spider Eyes cheap!" },	// "Oczy pajaka tanio!"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, "@ @ @ Amulety Orka @ @ @", { { { 30007, 30076, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@ @ @ Orc Amulet @ @ @" },	// "@ @ @ Amulety Orka @ @ @"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 2, "Bia\xB3" "a Wst\xEAga | K\xB3\xB9" "b itp.", { { { 30034, 30073, 0, 0 } }, { { 30011, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "White Hairband | Ball etc." },	// "Biala Wstega | Klab itp."
		{ SIGN_KIND_MATERIALS, SIGN_NEED_NONE, 0, "! ! Oby Kowal Nie Palil ! !", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "! ! May the Smith Not Burn ! !" },	// "! ! Oby Kowal Nie Palil ! !"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, ">>> \xAF\xF3\xB3\xE6 Nied\x9Fwiedzia <<<", { { { 30010, 30071, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, ">>> Bear Gall <<<" },	// ">>> Zolc Niedzwiedzia <<<"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, "Matowe lody dla och\xB3ody", { { { 30050, 30090, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Ice Marble to cool down" },	// "Matowe lody dla ochlody"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, "@ @ @ Shurikeny @ @ @", { { { 30041, 30075, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@ @ @ Shiriken @ @ @" },	// "@ @ @ Shurikeny @ @ @"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_NONE, 0, "Sklepik z ulepkami z m1/m2", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Little upgrades shop m1/m2" },	// "Sklepik z ulepkami z m1/m2"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_NONE, 0, "Wszystkie ulepszacze!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "All the upgrade items!" },	// "Wszystkie ulepszacze!"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_NONE, 0, "ULEPSZACZE TANIEJ NIZ OBOK >>>>", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "UPGRADES CHEAPER THAN NEXT DOOR" },	// "ULEPSZACZE TANIEJ NIZ OBOK >>>>"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_NONE, 0, "<<< ULEPY TANIEJ NI\xAF OBOK", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "<<< UPGRADES CHEAPER HERE" },	// "<<< ULEPY TANIEJ NIZ OBOK"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, "OGON W\xCA\xAF" "A Z PLUSEM !!!", { { { 30082, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Snake Tail+ !!!" },	// "OGON WEZA Z PLUSEM !!!"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, "Worek z paj\xEA" "cz\xB9 trucizn\xB9", { { { 30025, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Spider's Poison Sack" },	// "Worek z pajecza trucizna"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 3, "Oczy, Sieci, Worki - Paj\xB9ki", { { { 30057, 30162, 0, 0 } }, { { 30104, 30109, 0, 0 } }, { { 30025, 30058, 0, 0 } } }, "Spider Eyes, webs, sacks" },	// "Oczy, Sieci, Worki - Pajaki"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, "PAMI\xA5TKI PO DEMONIE", { { { 30015, 30086, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Demon's Keepsake" },	// "PAMIATKI PO DEMONIE"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, "@@@@@ PAMI\xA5TKI PO DEMONIE @@@@@", { { { 30015, 30086, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@@@@@ Demon's Keepsake @@@@@" },	// "@@@@@ PAMIATKI PO DEMONIE @@@@@"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, "Nieznane Leki z + i bez", { { { 30009, 30083, 35002, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Unknown Medicine, + or not" },	// "Nieznane Leki z + i bez"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ALL_GOODS, 2, "Klejnoty Demona i pami\xB9tki", { { { 30016, 30087, 0, 0 } }, { { 30015, 30086, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Demon's Gem & Demon's Keepsake" },	// "Klejnoty Demona i pamiatki"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 2, "KLEJNOTY PAMI\xA5TKI DT ULEPY", { { { 30016, 30087, 0, 0 } }, { { 30015, 30086, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Demon's Gem, DT upgrades" },	// "KLEJNOTY PAMIATKI DT ULEPY"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_NONE, 0, "Tanie ulepy", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Cheap upgrades" },	// "Tanie ulepy"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_NONE, 0, "T a n i e u l e p s z a c z e", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "C h e a p u p g r a d e s" },	// "T a n i e u l e p s z a c z e"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_NONE, 0, "Wszystko co potrzebne na +9", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "All you need for +9" },	// "Wszystko co potrzebne na +9"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ANY_GOODS, 1, "KREM DO TWARZY", { { { 30035, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Face Cream" },	// "KREM DO TWARZY"
		{ SIGN_KIND_MATERIALS, SIGN_NEED_ALL_GOODS, 2, "LI\x8C" "CIE I J\xCAZYKI \xAF" "AB", { { { 30040, 0, 0, 0 } }, { { 30060, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Leaf and Frog Tongue" },	// "LISCIE I JEZYKI ZAB"
		{ SIGN_KIND_OTHER, SIGN_NEED_ORES, 0, "Rudy i przetopy", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Ores and smelted ores" },	// "Rudy i przetopy"
		{ SIGN_KIND_OTHER, SIGN_NEED_ANY_GOODS, 1, "@ @ @ MEDALE KONNE @ @ @", { { { 50050, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@ @ @ Horse Medal @ @ @" },	// "@ @ @ MEDALE KONNE @ @ @"
		{ SIGN_KIND_OTHER, SIGN_NEED_ANY_GOODS, 1, "M e d a l e k o n n e", { { { 50050, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "~ Horse Medal ~" },	// "M e d a l e k o n n e"
		{ SIGN_KIND_OTHER, SIGN_NEED_ANY_GOODS, 1, "ZWOJE BLOGOSLAWIENSTWA", { { { 25040, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Blessing Scroll" },	// "ZWOJE BLOGOSLAWIENSTWA"
		{ SIGN_KIND_OTHER, SIGN_NEED_ANY_GOODS, 1, "ZWOJE BLOGOSLAWIENSTWA TANIO!", { { { 25040, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Blessing Scroll CHEAP!" },	// "ZWOJE BLOGOSLAWIENSTWA TANIO!"
		{ SIGN_KIND_OTHER, SIGN_NEED_ANY_GOODS, 1, "Z w o j e B\xB3ogos\xB3" "awie\xF1stwa :)", { { { 25040, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Blessing Scroll :)" },	// "Z w o j e Blogoslawienstwa :)"
		{ SIGN_KIND_OTHER, SIGN_NEED_ANY_GOODS, 1, "B\xB3ogos\xB3" "awie\xF1stwo od Alicji ;p", { { { 25040, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Blessing Scroll from Alicja ;p" },	// "Blogoslawienstwo od Alicji ;p"
		{ SIGN_KIND_GEAR, SIGN_NEED_NONE, 0, "EQ NAJTANIEJ w MIESCIE!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "CHEAPEST GEAR in TOWN!" },	// "EQ NAJTANIEJ w MIESCIE!"
		{ SIGN_KIND_GEAR, SIGN_NEED_NONE, 0, "SELL EQ ALLEGRO GG 1295551", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "SELL GEAR ALLEGRO GG 1295551" },	// "SELL EQ ALLEGRO GG 1295551"
		{ SIGN_KIND_GEAR, SIGN_NEED_NONE, 0, ">>>>>> Wyprzeda\xBF szafy <<<<<<", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, ">>>>>> Wardrobe sale <<<<<<" },	// ">>>>>> Wyprzedaz szafy <<<<<<"
		{ SIGN_KIND_GEAR, SIGN_NEED_NONE, 0, "W tym juz nikt cie nie wysmieje", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Nobody will laugh at you in it" },	// "W tym juz nikt cie nie wysmieje"
		{ SIGN_KIND_GEAR, SIGN_NEED_GEAR_KINDS, 0, "TARCZE ZBROJE BRONIE I INNE", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "SHIELDS ARMOUR WEAPONS & MORE" },	// "TARCZE ZBROJE BRONIE I INNE"
		{ SIGN_KIND_GEAR, SIGN_NEED_HIGH_GEAR, 1, "----- NIE MARZNIJ NA SOHAN -----", { { { 45, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "---- DON'T FREEZE ON SOHAN ----" },	// "----- NIE MARZNIJ NA SOHAN -----"
		{ SIGN_KIND_GEAR, SIGN_NEED_NONE, 0, "@@@@ Kute w b\xF3lach u kowala @@@@", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@@@@ Forged in pain @@@@" },	// "@@@@ Kute w bolach u kowala @@@@"
		{ SIGN_KIND_GEAR, SIGN_NEED_LOW_GEAR, 1, "Zestaw przetrwania na dzikie psy", { { { 30, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Survival kit vs Wild Dog" },	// "Zestaw przetrwania na dzikie psy"
		{ SIGN_KIND_GEAR, SIGN_NEED_TOP_GEAR, 0, "EQ +7/+8/+9", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "GEAR +7/+8/+9" },	// "EQ +7/+8/+9"
		{ SIGN_KIND_GEAR, SIGN_NEED_NONE, 0, "EQ TANIEJ NIZ OBOK >>>>", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "GEAR CHEAPER THAN NEXT DOOR >>>>" },	// "EQ TANIEJ NIZ OBOK >>>>"
		{ SIGN_KIND_GEAR, SIGN_NEED_NONE, 0, "<<<< ITEMY TANIEJ NI\xAF OBOK!!!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "<<<< ITEMS CHEAPER HERE!!!" },	// "<<<< ITEMY TANIEJ NIZ OBOK!!!"
		{ SIGN_KIND_GEAR, SIGN_NEED_NONE, 0, "B\xB3yszczy si\xEA jak psu...", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Shiny like a dog's..." },	// "Blyszczy sie jak psu..."
		{ SIGN_KIND_GEAR, SIGN_NEED_NONE, 0, "@@@@ Gotowe do walki itemy @@@@", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "@@@@ Battle-ready items @@@@" },	// "@@@@ Gotowe do walki itemy @@@@"
		{ SIGN_KIND_GEAR, SIGN_NEED_BODY_ARMOUR, 0, "Zmie\xF1 szmaty na zbroje!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Swap rags for armour!" },	// "Zmien szmaty na zbroje!"
		{ SIGN_KIND_GEAR, SIGN_NEED_NONE, 0, "NAJLEPSZE EQ NA SERWIE", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "BEST GEAR ON THE SERVER" },	// "NAJLEPSZE EQ NA SERWIE"
		{ SIGN_KIND_GEAR, SIGN_NEED_NONE, 0, "Zajrzyj i zr\xF3" "b z siebie koxa", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Look in and become a beast" },	// "Zajrzyj i zrob z siebie koxa"
		{ SIGN_KIND_GEAR, SIGN_NEED_NONE, 0, "Itemy za kt\xF3re sprzedasz nerk\xEA", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Items worth selling a kidney for" },	// "Itemy za ktore sprzedasz nerke"
		{ SIGN_KIND_GEAR, SIGN_NEED_NONE, 0, "Kasuj\xEA posta\xE6 - bra\xE6 wszystko!", { { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 } } }, "Deleting my char - take it all!" },	// "Kasuje postac - brac wszystko!"
	};
	const size_t SIGN_NAME_COUNT = sizeof(SIGN_NAMES) / sizeof(SIGN_NAMES[0]);
}

#endif
