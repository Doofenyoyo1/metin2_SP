// Rendered by linux-port/overlays/playerbot/tools/generate_iwakura_prices.py from Iwakura's price
// list (data/iwakura_ceny.txt, his v1.0). DO NOT EDIT; edit the list and re-run.
//
// Every number here is his, every vnum was resolved against this world's own
// item_proto and mob_proto, and every bonus row was checked against
// world.item_attr for the slot he put it under - the generator refuses to
// write this file if one name cannot be matched or one bonus cannot roll.
//
// Every price scales with the world's yang drop rate along the curve at the top
// of his sheet (PLAYERBOT_PRICE_RATE_POINTS, see ScalePlayerBotIwakuraPrice).
#ifndef __INC_METIN2_PLAYERBOT_PRICE_TABLES_H__
#define __INC_METIN2_PLAYERBOT_PRICE_TABLES_H__

namespace
{
	// The yang drop rate in percent and the multiplier it pays, in hundredths:
	// read straight through between his points and proportionally outside them.
	struct TPlayerBotPriceRatePoint { int iRate; int iPct; };
	const TPlayerBotPriceRatePoint PLAYERBOT_PRICE_RATE_POINTS[] = {
		{   100,   100 },
		{   200,   220 },
		{   500,   500 },
		{   900,   900 },
		{  1000,  1000 },
		{  1500,  1500 },
		{ 10000, 10000 },
	};

	// Weapons, armour, boots, bracelets, necklaces, earrings and shields by
	// family and refine. The index is the refine level, 0 to 9; a bit set in the
	// mask is a refine his sheet marks "do handlarki" - that piece is the
	// merchant's and its price is zero here. A family the table does not carry
	// keeps the old flat prices.
	struct TPlayerBotGearPrice { DWORD dwBaseVnum; WORD wMerchantMask; DWORD adwPrice[10]; };
	const TPlayerBotGearPrice PLAYERBOT_GEAR_PRICES[] = {
		{    10, 0x000, { 28800, 28800, 28800, 28800, 80000, 80000, 80000, 240000, 336000, 800000 } },	// Miecz
		{    20, 0x000, { 40000, 40000, 40000, 40000, 160000, 160000, 160000, 278400, 560000, 1024000 } },	// Dlugi Miecz
		{    30, 0x000, { 57600, 57600, 57600, 57600, 96000, 96000, 96000, 316800, 768000, 1360000 } },	// Sejmitar
		{    40, 0x000, { 80000, 80000, 80000, 80000, 160000, 160000, 160000, 288000, 704000, 1264000 } },	// Stozkowy Miecz
		{    50, 0x000, { 102400, 102400, 102400, 102400, 284800, 284800, 284800, 496000, 864000, 1344000 } },	// Szeroki Miecz
		{    60, 0x000, { 96000, 96000, 96000, 96000, 336000, 336000, 336000, 604800, 896000, 1616000 } },	// Srebrny Miecz
		{    70, 0x000, { 192000, 192000, 192000, 192000, 384000, 384000, 384000, 768000, 1216000, 1760000 } },	// Miecz Storczykowy
		{    80, 0x000, { 230400, 230400, 230400, 230400, 432000, 432000, 432000, 928000, 1440000, 2080000 } },	// Miecz Poltorareczny
		{    90, 0x000, { 233600, 233600, 233600, 233600, 480000, 480000, 480000, 1120000, 1440000, 2272000 } },	// Miecz Barbarzyncy
		{   100, 0x000, { 236800, 236800, 236800, 236800, 528000, 528000, 528000, 1376000, 2240000, 3680000 } },	// Krwawy Miecz
		{   110, 0x000, { 240000, 240000, 240000, 240000, 544000, 544000, 544000, 1504000, 2816000, 4000000 } },	// Wielki Miecz
		{   120, 0x000, { 246400, 246400, 246400, 246400, 576000, 576000, 576000, 2048000, 3040000, 5120000 } },	// Miecz Lat. Maga
		{   130, 0x000, { 249600, 249600, 249600, 249600, 608000, 608000, 608000, 2496000, 4320000, 7200000 } },	// Pol-ksiezycowy Miecz
		{   140, 0x000, { 480000, 480000, 480000, 480000, 1120000, 1120000, 1120000, 3840000, 8000000, 14400000 } },	// Miecz Bojowy
		{   150, 0x000, { 416000, 416000, 416000, 416000, 896000, 896000, 896000, 3200000, 7040000, 11200000 } },	// Miecz Szponu Ducha
		{   160, 0x000, { 448000, 448000, 448000, 448000, 1024000, 1024000, 1024000, 3200000, 6080000, 11200000 } },	// Miecz Nimfy
		{   170, 0x000, { 352000, 352000, 352000, 352000, 896000, 896000, 896000, 3840000, 5760000, 8320000 } },	// Miecz Zadlo
		{   180, 0x000, { 4480000, 4480000, 4480000, 4480000, 6400000, 8000000, 10240000, 14720000, 25600000, 41600000 } },	// Zatruty miecz
		{   190, 0x000, { 2560000, 2560000, 2560000, 2560000, 3200000, 4320000, 5600000, 7680000, 10240000, 17600000 } },	// Lwi Miecz
		{   290, 0x000, { 725000, 725000, 725000, 725000, 1050000, 1375000, 1650000, 3400000, 6225000, 8125000 } },	// Miecz Pelni Ksiezyca
		{  1000, 0x000, { 21760, 21760, 21760, 21760, 60800, 60800, 60800, 128000, 283200, 512000 } },	// Sztylet
		{  1010, 0x000, { 48000, 48000, 48000, 48000, 124800, 124800, 124800, 192000, 387200, 569600 } },	// Sztylet Kobry
		{  1040, 0x000, { 76800, 76800, 76800, 76800, 230400, 230400, 230400, 649600, 1049600, 1552000 } },	// Ukaszenie Kota
		{  1050, 0x000, { 73600, 73600, 73600, 73600, 265600, 265600, 265600, 704000, 1120000, 1740800 } },	// Twarz Diabla
		{  1060, 0x000, { 144000, 144000, 144000, 144000, 299200, 299200, 299200, 816000, 1244800, 2326400 } },	// Sztylet Piesci Diab
		{  1070, 0x000, { 169600, 169600, 169600, 169600, 396800, 396800, 396800, 1052800, 1816000, 2860800 } },	// Krwawy Sztylet
		{  1080, 0x000, { 176000, 176000, 176000, 176000, 460800, 460800, 460800, 1148800, 2083200, 2982400 } },	// Zebrowy Noz
		{  1090, 0x000, { 182400, 182400, 182400, 182400, 518400, 518400, 518400, 1350400, 2729600, 4480000 } },	// Chakram
		{  1100, 0x000, { 358400, 358400, 358400, 358400, 873600, 873600, 873600, 2918400, 5920000, 11040000 } },	// Smoczy Noz
		{  1110, 0x000, { 332800, 332800, 332800, 332800, 768000, 768000, 768000, 2496000, 4608000, 8160000 } },	// Noz Blyskawicy
		{  1120, 0x000, { 278400, 278400, 278400, 278400, 672000, 672000, 672000, 2841600, 4416000, 6240000 } },	// Noz Siamese
		{  1130, 0x000, { 3360000, 3360000, 3360000, 3360000, 4992000, 5920000, 7776000, 10720000, 19712000, 31200000 } },	// Skrzydla Demona Chakr.
		{  1170, 0x000, { 357500, 357500, 357500, 357500, 850000, 1080000, 1590000, 2210000, 3455000, 5825000 } },	// Kozik Czar. Lis.
		{  2000, 0x000, { 23360, 23360, 23360, 23360, 67200, 67200, 67200, 144000, 316800, 480000 } },	// Luk
		{  2010, 0x000, { 32640, 32640, 32640, 32640, 131200, 131200, 131200, 174400, 272000, 608000 } },	// Dlugi Luk
		{  2020, 0x000, { 47360, 47360, 47360, 47360, 78400, 78400, 78400, 204800, 457600, 784000 } },	// Kompozytowy Luk
		{  2030, 0x000, { 65600, 65600, 65600, 65600, 131200, 131200, 131200, 235200, 569600, 1152000 } },	// Bojowy Luk
		{  2040, 0x000, { 64000, 64000, 64000, 64000, 230400, 230400, 230400, 400000, 704000, 1344000 } },	// Dlugi Luk Jezdzcy
		{  2050, 0x000, { 78400, 78400, 78400, 78400, 272000, 272000, 272000, 492800, 720000, 1616000 } },	// Bojowy Luk Jezdzcy
		{  2060, 0x000, { 156800, 156800, 156800, 156800, 313600, 313600, 313600, 624000, 992000, 1920000 } },	// Miedziany Luk
		{  2070, 0x000, { 182400, 182400, 182400, 182400, 352000, 352000, 352000, 752000, 1360000, 2224000 } },	// Luk Czarnych Ruin
		{  2080, 0x000, { 179200, 179200, 179200, 179200, 387200, 387200, 387200, 1072000, 1664000, 2320000 } },	// Luk Czerwonego Oka
		{  2090, 0x000, { 185600, 185600, 185600, 185600, 432000, 432000, 432000, 1120000, 1808000, 3952000 } },	// Luk Kolczastego Lis.
		{  2100, 0x000, { 195200, 195200, 195200, 195200, 441600, 441600, 441600, 1216000, 2288000, 4512000 } },	// Luk z rogu byka
		{  2110, 0x000, { 192000, 192000, 192000, 192000, 464000, 464000, 464000, 1664000, 2464000, 5120000 } },	// Luk Jednorozca
		{  2120, 0x000, { 203200, 203200, 203200, 203200, 492800, 492800, 492800, 2032000, 5088000, 8064000 } },	// Olbrz. Skrzydl. Luk
		{  2130, 0x000, { 364800, 364800, 364800, 364800, 832000, 832000, 832000, 2592000, 4928000, 10400000 } },	// Boski Luk Moreli
		{  2140, 0x000, { 390400, 390400, 390400, 390400, 912000, 912000, 912000, 3104000, 6464000, 13600000 } },	// Olbrz. Luk Zolt. Smoka
		{  2150, 0x000, { 462500, 462500, 462500, 462500, 762500, 1075000, 1300000, 2437500, 3600000, 6275000 } },	// Luk Z Rogu Jelenia
		{  2160, 0x000, { 288000, 288000, 288000, 288000, 729600, 729600, 729600, 3136000, 5952000, 9920000 } },	// Olbrz. Luk Diabla
		{  2170, 0x000, { 3648000, 3648000, 3648000, 3648000, 5184000, 6496000, 8320000, 11968000, 20736000, 33600000 } },	// Stalowy Luk Kruka
		{  2180, 0x000, { 2080000, 2080000, 2080000, 2080000, 2592000, 3488000, 4544000, 6240000, 8320000, 14240000 } },	// Luk Niebieskiego Smoka
		{  3000, 0x000, { 25600, 25600, 25600, 25600, 72000, 72000, 72000, 160000, 214400, 633600 } },	// Glewia
		{  3010, 0x000, { 35200, 35200, 35200, 35200, 140800, 140800, 140800, 188800, 390400, 768000 } },	// Wlocznia
		{  3020, 0x000, { 52800, 52800, 52800, 52800, 88000, 88000, 88000, 230400, 496000, 953600 } },	// Gilotynowe Ostrze
		{  3030, 0x000, { 70400, 70400, 70400, 70400, 142400, 142400, 142400, 256000, 880000, 1760000 } },	// Pajecza Wlocznia
		{  3040, 0x000, { 92800, 92800, 92800, 92800, 259200, 259200, 259200, 451200, 784000, 1382400 } },	// Gizarma
		{  3050, 0x000, { 83200, 83200, 83200, 83200, 291200, 291200, 291200, 524800, 777600, 1532800 } },	// Kosa Bojowa
		{  3060, 0x000, { 172800, 172800, 172800, 172800, 345600, 345600, 345600, 691200, 1094400, 1904000 } },	// Trojzab
		{  3070, 0x000, { 195200, 195200, 195200, 195200, 371200, 371200, 371200, 796800, 1238400, 2108800 } },	// Halabarda
		{  3080, 0x000, { 214400, 214400, 214400, 214400, 444800, 444800, 444800, 1040000, 1337600, 2432000 } },	// Olbrz. Topor
		{  3090, 0x000, { 208000, 208000, 208000, 208000, 464000, 464000, 464000, 1209600, 1971200, 3878400 } },	// Lodowa Iglica
		{  3100, 0x000, { 217600, 217600, 217600, 217600, 492800, 492800, 492800, 1366400, 2624000, 4918400 } },	// Miecz Dwunastu Duchow
		{  3110, 0x000, { 219200, 219200, 219200, 219200, 512000, 512000, 512000, 1820800, 3056000, 5516800 } },	// Ostrze Zbawienia
		{  3120, 0x000, { 227200, 227200, 227200, 227200, 556800, 556800, 556800, 2294400, 3974400, 7264000 } },	// Zabojca Lwow
		{  3130, 0x000, { 432000, 432000, 432000, 432000, 1008000, 1008000, 1008000, 3456000, 7200000, 16160000 } },	// Partyzana
		{  3140, 0x000, { 364800, 364800, 364800, 364800, 787200, 787200, 787200, 2816000, 6176000, 11456000 } },	// Magnetyczne Ostrze
		{  3150, 0x000, { 300800, 300800, 300800, 300800, 768000, 768000, 768000, 3296000, 4928000, 12896000 } },	// Zlodziej Dusz
		{  3160, 0x000, { 4160000, 4160000, 4160000, 4160000, 5952000, 7440000, 9504000, 13696000, 23808000, 38688000 } },	// Miecz Zalu
		{  3210, 0x000, { 700000, 700000, 700000, 700000, 895000, 1075000, 1435000, 2555000, 4287500, 6637500 } },	// Ostrze Z Czerw. Stali
		{  4000, 0x000, { 30400, 30400, 30400, 30400, 124800, 124800, 124800, 212800, 390400, 768000 } },	// Amija
		{  4010, 0x000, { 60800, 60800, 60800, 60800, 124800, 124800, 124800, 280000, 668800, 1139200 } },	// Dziewiec Ostrzy
		{  4020, 0x000, { 72000, 72000, 72000, 72000, 192000, 192000, 192000, 614400, 832000, 1308800 } },	// Krotki Noz
		{  5000, 0x000, { 20160, 20160, 20160, 20160, 56000, 56000, 56000, 123200, 200000, 364800 } },	// Miedziany Dzwon
		{  5010, 0x000, { 56000, 56000, 56000, 56000, 112000, 112000, 112000, 201600, 492800, 884800 } },	// Srebrny Dzwon
		{  5020, 0x000, { 71680, 71680, 71680, 71680, 199360, 199360, 199360, 347200, 604800, 1260800 } },	// Zloty Dzwon
		{  5030, 0x000, { 67200, 67200, 67200, 67200, 235200, 235200, 235200, 423360, 691200, 1355200 } },	// Jadeitowy Dzwon
		{  5040, 0x000, { 134400, 134400, 134400, 134400, 268800, 268800, 268800, 537600, 1011200, 1712000 } },	// Dzwon Fontanny
		{  5050, 0x000, { 161280, 161280, 161280, 161280, 302400, 302400, 302400, 649600, 1136000, 2000000 } },	// Morelowy Dzwon
		{  5060, 0x000, { 163520, 163520, 163520, 163520, 336000, 336000, 336000, 784000, 1136000, 2416000 } },	// Magiczny Dzwon
		{  5070, 0x000, { 165760, 165760, 165760, 165760, 369600, 369600, 369600, 963200, 1632000, 2960000 } },	// Zloty Robaczy
		{  5080, 0x000, { 168000, 168000, 168000, 168000, 380800, 380800, 380800, 1212800, 2291200, 3920000 } },	// Stalowy Robaczy Dzwon
		{  5090, 0x000, { 172480, 172480, 172480, 172480, 403200, 403200, 403200, 1849600, 2768000, 4544000 } },	// Dzwon Burzowego Ptaka
		{  5100, 0x000, { 291200, 291200, 291200, 291200, 627200, 627200, 627200, 2240000, 4928000, 9120000 } },	// Dzwon Nieba I Ziemi
		{  5110, 0x000, { 422500, 422500, 422500, 422500, 585000, 750000, 1130000, 1490000, 2137500, 5687500 } },	// Antyczny Dzwon
		{  5120, 0x000, { 3136000, 3136000, 3136000, 3136000, 4480000, 5600000, 7168000, 10304000, 17920000, 29120000 } },	// Bambusowy Dzwon
		{  5130, 0x000, { 246400, 246400, 246400, 246400, 627200, 627200, 627200, 2688000, 4032000, 8384000 } },	// Dzwon Smierci
		{  7000, 0x000, { 20160, 20160, 20160, 20160, 56000, 56000, 56000, 123200, 168000, 396800 } },	// Wachlarz
		{  7010, 0x000, { 27200, 27200, 27200, 27200, 108800, 108800, 108800, 145600, 318080, 668800 } },	// Zelazny Wachlarz
		{  7020, 0x000, { 41600, 41600, 41600, 41600, 68800, 68800, 68800, 278400, 387200, 793600 } },	// Wachlarz Czarn. Tygr.
		{  7030, 0x000, { 51200, 51200, 51200, 51200, 104000, 104000, 104000, 315200, 585600, 1046400 } },	// Zurawi Wachlarz
		{  7040, 0x000, { 75200, 75200, 75200, 75200, 211200, 211200, 211200, 496000, 960000, 1379200 } },	// Pawi Wachlarz
		{  7050, 0x000, { 65600, 65600, 65600, 65600, 232000, 232000, 232000, 736000, 1257600, 1785600 } },	// Wodny Wachlarz
		{  7060, 0x000, { 136000, 136000, 136000, 136000, 272000, 272000, 272000, 544000, 1184000, 2144000 } },	// Kamienny Wachlarz
		{  7070, 0x000, { 153600, 153600, 153600, 153600, 289600, 289600, 289600, 716800, 1283200, 2352000 } },	// Oceaniczny Wachlarz
		{  7080, 0x000, { 169600, 169600, 169600, 169600, 348800, 348800, 348800, 880000, 1369600, 2585600 } },	// Zadlowy Wachlarz
		{  7090, 0x000, { 156800, 156800, 156800, 156800, 348800, 348800, 348800, 1030400, 1798400, 3068800 } },	// Wachlarz Feniksa
		{  7100, 0x000, { 168000, 168000, 168000, 168000, 380800, 380800, 380800, 1372800, 2291200, 3440000 } },	// Potrojny Wachlarz
		{  7110, 0x000, { 169600, 169600, 169600, 169600, 396800, 396800, 396800, 2118400, 3056000, 4320000 } },	// Brwisty Wachlarz
		{  7120, 0x000, { 184000, 184000, 184000, 184000, 448000, 448000, 448000, 2454400, 4156800, 6608000 } },	// Czarny Slon. Wachlarz
		{  7130, 0x000, { 304000, 304000, 304000, 304000, 697600, 697600, 697600, 2816000, 5088000, 10816000 } },	// Niebian. Ptasi Wachl.
		{  7140, 0x000, { 340800, 340800, 340800, 340800, 795200, 795200, 795200, 2726400, 6000000, 13440000 } },	// Wachlarz Zbawienia
		{  7150, 0x000, { 368000, 368000, 368000, 368000, 710400, 710400, 710400, 4044800, 7040000, 11424000 } },	// Ekstazyjny Wachlarz
		{  7160, 0x000, { 300000, 300000, 300000, 300000, 457500, 612500, 897500, 1392500, 2945000, 5150000 } },	// Wachlarz Jes. Wiatru
		{  7180, 0x000, { 3046400, 3046400, 3046400, 3046400, 4352000, 5440000, 6976000, 10016000, 17408000, 28288000 } },	// Wachlarz 8 Trigramow
		{ 11200, 0x000, { 32000, 32000, 32000, 32000, 89600, 89600, 89600, 192000, 400000, 1552000 } },	// Mnisia Zbr. Plytowa
		{ 11210, 0x000, { 48000, 48000, 48000, 48000, 176000, 176000, 176000, 240000, 528000, 1600000 } },	// Zelazna Zbr. Plytowa
		{ 11220, 0x000, { 70400, 70400, 70400, 70400, 224000, 224000, 224000, 384000, 1216000, 2400000 } },	// Zbr. Plyt. Tygrysa
		{ 11230, 0x000, { 112000, 112000, 112000, 112000, 304000, 304000, 304000, 1184000, 1824000, 3840000 } },	// Lwia Zbroja Plytowa
		{ 11240, 0x000, { 208000, 208000, 208000, 208000, 528000, 528000, 528000, 1600000, 3840000, 8224000 } },	// Smiert. Zbroja Plytowa
		{ 11250, 0x000, { 272000, 272000, 272000, 272000, 624000, 624000, 624000, 1920000, 3840000, 8224000 } },	// Smocza Zbroja Plytowa
		{ 11260, 0x000, { 288000, 288000, 288000, 288000, 704000, 704000, 704000, 2240000, 4480000, 6080000 } },	// Zbroja Plytowa Z Lusek
		{ 11270, 0x000, { 304000, 304000, 304000, 304000, 768000, 768000, 768000, 2944000, 5120000, 8960000 } },	// Zlota Zbroja Plytowa
		{ 11280, 0x000, { 800000, 800000, 800000, 800000, 1120000, 1760000, 4000000, 5440000, 11200000, 23040000 } },	// Zbroja Boga Smokow
		{ 11290, 0x000, { 2880000, 2880000, 2880000, 2880000, 5120000, 7040000, 11840000, 15680000, 27200000, 41600000 } },	// Zbroja Z Czarnej Stali
		{ 11400, 0x000, { 27200, 27200, 27200, 27200, 75200, 75200, 75200, 164800, 497600, 1254400 } },	// Blekitne Ubranie
		{ 11410, 0x000, { 40000, 40000, 40000, 40000, 147200, 147200, 147200, 204800, 732800, 1721600 } },	// Kremowe Ubranie
		{ 11420, 0x000, { 59200, 59200, 59200, 59200, 190400, 190400, 190400, 608000, 1360000, 2000000 } },	// Czerwone Ubranie
		{ 11430, 0x000, { 96000, 96000, 96000, 96000, 256000, 256000, 256000, 768000, 1440000, 2256000 } },	// Czer. Ubranie Mrowki
		{ 11440, 0x000, { 176000, 176000, 176000, 176000, 361600, 361600, 361600, 1568000, 2720000, 4768000 } },	// Ubranie Lwiej Mrowki
		{ 11450, 0x000, { 232000, 232000, 232000, 232000, 524800, 524800, 524800, 1584000, 3536000, 6272000 } },	// Ubranie Zabojcy
		{ 11460, 0x000, { 244800, 244800, 244800, 244800, 592000, 592000, 592000, 1920000, 4128000, 8000000 } },	// Ubranie Mlodego Smoka
		{ 11470, 0x000, { 259200, 259200, 259200, 259200, 640000, 640000, 640000, 2496000, 6016000, 9536000 } },	// Ubranie Zaboj. Wiatru
		{ 11480, 0x000, { 678400, 678400, 678400, 678400, 940800, 1488000, 3440000, 4512000, 9520000, 19360000 } },	// Ubranie Fuksyjne
		{ 11490, 0x000, { 2768000, 2768000, 2768000, 2768000, 4288000, 5984000, 10176000, 13024000, 23120000, 29568000 } },	// Ubranie Czarn. Wiatru
		{ 11600, 0x000, { 28800, 28800, 28800, 28800, 83200, 83200, 83200, 182400, 473600, 912000 } },	// Zalobna Zbr. Plytowa
		{ 11610, 0x000, { 44800, 44800, 44800, 44800, 163200, 163200, 163200, 220800, 601600, 1232000 } },	// Burzowa Zbroj. Plytowa
		{ 11620, 0x000, { 64000, 64000, 64000, 64000, 206400, 206400, 206400, 364800, 726400, 1456000 } },	// Nieszczesna Zbr. Plytowa
		{ 11630, 0x000, { 136000, 136000, 136000, 136000, 344000, 344000, 344000, 1161600, 2662400, 3238400 } },	// Upiorna Zbroja Plytowa
		{ 11640, 0x000, { 195200, 195200, 195200, 195200, 400000, 400000, 400000, 1664000, 3040000, 5184000 } },	// Zbroja Plyt. Yin-Yang
		{ 11650, 0x000, { 252800, 252800, 252800, 252800, 585600, 585600, 585600, 1360000, 2816000, 4992000 } },	// Mistyczna Zbroja Plytowa
		{ 11660, 0x000, { 264000, 264000, 264000, 264000, 668800, 668800, 668800, 2080000, 4112000, 6672000 } },	// Mglista Zbroja Plytowa
		{ 11670, 0x000, { 281600, 281600, 281600, 281600, 720000, 720000, 720000, 2796800, 4704000, 8320000 } },	// Zbroja Twarzy Ducha
		{ 11680, 0x000, { 752000, 752000, 752000, 752000, 1030400, 1600000, 3792000, 5056000, 10528000, 21184000 } },	// Duchowa Zbroja Plytowa
		{ 11690, 0x000, { 2736000, 2736000, 2736000, 2736000, 4800000, 6544000, 10880000, 14240000, 25568000, 32384000 } },	// Zbr. Plyt. Czar. Magii
		{ 11800, 0x000, { 27200, 27200, 27200, 27200, 75200, 75200, 75200, 164800, 232000, 358400 } },	// Blekitna Szata
		{ 11810, 0x000, { 41600, 41600, 41600, 41600, 177600, 177600, 177600, 238400, 404800, 761600 } },	// Turkusowa Szata
		{ 11820, 0x000, { 59200, 59200, 59200, 59200, 190400, 190400, 190400, 329600, 579200, 1193600 } },	// Rozowa Szata
		{ 11830, 0x000, { 96000, 96000, 96000, 96000, 256000, 256000, 256000, 761600, 1248000, 2080000 } },	// Milosna Szata
		{ 11840, 0x000, { 176000, 176000, 176000, 176000, 368000, 368000, 368000, 1552000, 3040000, 4736000 } },	// Szata Zach. Nieba
		{ 11850, 0x000, { 230400, 230400, 230400, 230400, 537600, 537600, 537600, 1056000, 2304000, 4960000 } },	// Szata Slonca
		{ 11860, 0x000, { 244800, 244800, 244800, 244800, 604800, 604800, 604800, 1904000, 3808000, 8224000 } },	// Szata Moralnosci
		{ 11870, 0x000, { 260800, 260800, 260800, 260800, 652800, 652800, 652800, 2496000, 4608000, 8960000 } },	// Szata Pomaran. Kota
		{ 11880, 0x000, { 688000, 688000, 688000, 688000, 950400, 1494400, 3360000, 4608000, 9504000, 19328000 } },	// Szata Baronow
		{ 11890, 0x000, { 2448000, 2448000, 2448000, 2448000, 4352000, 5984000, 9920000, 13312000, 21920000, 29920000 } },	// Czarna Szata
		{ 13000, 0x000, { 16000, 16000, 16000, 16000, 256000, 256000, 256000, 480000, 768000, 1152000 } },	// Bojowa Tarcza
		{ 13020, 0x000, { 208000, 208000, 208000, 208000, 288000, 608000, 896000, 1440000, 2716800, 6240000 } },	// Pieciokatna Tarcza
		{ 13040, 0x000, { 288000, 288000, 288000, 288000, 400000, 672000, 1248000, 1836800, 3680000, 9424000 } },	// Czarna Okragla Tarcza
		{ 13060, 0x000, { 480000, 480000, 480000, 480000, 704000, 1136000, 1888000, 5760000, 10880000, 19200000 } },	// Sokola Tarcza
		{ 13080, 0x000, { 416000, 416000, 416000, 416000, 576000, 880000, 1440000, 4800000, 9280000, 16640000 } },	// Tarcza Tygrysa
		{ 13100, 0x000, { 416000, 416000, 416000, 416000, 576000, 880000, 1440000, 4800000, 9280000, 16640000 } },	// Lwia Tarcza
		{ 13120, 0x000, { 416000, 416000, 416000, 416000, 576000, 880000, 1440000, 4800000, 9280000, 16640000 } },	// Tarcza Smoka
		{ 14000, 0x000, { 16000, 16000, 16000, 16000, 176000, 176000, 176000, 240000, 800000, 1440000 } },	// Drewniana Bransoleta
		{ 14020, 0x000, { 48000, 48000, 48000, 48000, 259200, 259200, 259200, 300800, 896000, 1536000 } },	// Miedziana Bransoleta
		{ 14040, 0x000, { 240000, 240000, 240000, 240000, 448000, 448000, 448000, 672000, 1040000, 2560000 } },	// Srebrna Bransoleta
		{ 14060, 0x000, { 80000, 80000, 80000, 80000, 240000, 240000, 240000, 396800, 768000, 1120000 } },	// Zlota Bransoleta
		{ 14080, 0x000, { 80000, 80000, 80000, 80000, 208000, 208000, 208000, 268800, 576000, 1216000 } },	// Jadeitowa Bransoleta
		{ 14100, 0x000, { 208000, 208000, 208000, 208000, 316800, 316800, 316800, 800000, 2112000, 3040000 } },	// Ebonitowa Bransoleta
		{ 14120, 0x000, { 60800, 60800, 60800, 60800, 224000, 224000, 224000, 300800, 480000, 1120000 } },	// Perlowa Bransoleta
		{ 14140, 0x000, { 384000, 384000, 384000, 384000, 512000, 880000, 1216000, 4800000, 9120000, 13440000 } },	// Bransol. Z Bial. Zlota
		{ 14160, 0x000, { 144000, 144000, 144000, 144000, 480000, 480000, 480000, 1120000, 1696000, 2336000 } },	// Krysztalowa Bransoleta
		{ 14180, 0x000, { 147200, 147200, 147200, 147200, 384000, 384000, 384000, 672000, 1088000, 2112000 } },	// Ametystowa Bransoleta
		{ 14200, 0x000, { 480000, 480000, 480000, 480000, 608000, 880000, 1440000, 9920000, 20800000, 29440000 } },	// Bransol. Z Niebian.Lez
		{ 15000, 0x000, { 16000, 16000, 16000, 16000, 128000, 128000, 128000, 240000, 800000, 1632000 } },	// Skorzane Buty
		{ 15020, 0x000, { 38400, 38400, 38400, 38400, 160000, 160000, 160000, 268800, 896000, 1824000 } },	// Bambusowe Buty
		{ 15040, 0x000, { 48000, 48000, 48000, 48000, 198400, 198400, 198400, 278400, 704000, 1664000 } },	// Drewniane Buty
		{ 15060, 0x000, { 112000, 112000, 112000, 112000, 384000, 384000, 384000, 544000, 1216000, 1920000 } },	// Buty Wyszywane Zlotem
		{ 15080, 0x000, { 256000, 256000, 256000, 256000, 480000, 480000, 480000, 1024000, 2080000, 3840000 } },	// Skorzane Kozaki
		{ 15100, 0x000, { 96000, 96000, 96000, 96000, 240000, 240000, 240000, 896000, 1152000, 2528000 } },	// Zlote Buty
		{ 15120, 0x000, { 112000, 112000, 112000, 112000, 240000, 240000, 240000, 896000, 1344000, 2176000 } },	// Buty Z Brazu
		{ 15140, 0x000, { 208000, 208000, 208000, 208000, 384000, 384000, 384000, 1024000, 1856000, 2816000 } },	// Jadeitowe Buty
		{ 15160, 0x000, { 67200, 67200, 67200, 67200, 272000, 272000, 272000, 278400, 704000, 1664000 } },	// Ekstazyjne Buty
		{ 15180, 0x000, { 224000, 224000, 224000, 224000, 416000, 416000, 416000, 1024000, 2080000, 3840000 } },	// Deszczowe Buty
		{ 15200, 0x000, { 304000, 304000, 304000, 304000, 480000, 960000, 2080000, 5120000, 8000000, 12800000 } },	// Buty Feniksa
		{ 15220, 0x000, { 288000, 288000, 288000, 288000, 416000, 896000, 1792000, 4480000, 6720000, 11200000 } },	// Buty Ognistego Ptaka
		{ 16000, 0x000, { 16000, 16000, 16000, 16000, 128000, 128000, 128000, 240000, 400000, 768000 } },	// Drewniany Naszyjnik
		{ 16020, 0x000, { 38400, 38400, 38400, 38400, 176000, 176000, 176000, 262400, 528000, 816000 } },	// Miedziany Naszyjnik
		{ 16040, 0x000, { 64000, 64000, 64000, 64000, 214400, 214400, 214400, 294400, 723200, 1168000 } },	// Srebrny Naszyjnik
		{ 16060, 0x000, { 80000, 80000, 80000, 80000, 294400, 294400, 294400, 544000, 1011200, 1424000 } },	// Zloty Naszyjnik
		{ 16080, 0x000, { 192000, 192000, 192000, 192000, 240000, 512000, 928000, 1472000, 2304000, 3040000 } },	// Jadeitowy Naszyjnik
		{ 16100, 0x000, { 176000, 176000, 176000, 176000, 272000, 384000, 672000, 1024000, 1536000, 3168000 } },	// Ebonitowy Naszyjnik
		{ 16120, 0x000, { 176000, 176000, 176000, 176000, 272000, 384000, 672000, 1024000, 1536000, 3168000 } },	// Perlowy Naszyjnik
		{ 16140, 0x000, { 176000, 176000, 176000, 176000, 336000, 544000, 800000, 1248000, 1728000, 3520000 } },	// Naszyj. Z Bial. Zlota
		{ 16160, 0x000, { 176000, 176000, 176000, 176000, 272000, 384000, 672000, 1024000, 1536000, 3168000 } },	// Krysztalowy Naszyjnik
		{ 16180, 0x000, { 256000, 256000, 256000, 256000, 384000, 608000, 1120000, 3840000, 6720000, 13760000 } },	// Ametystowy Naszyjnik
		{ 16200, 0x000, { 288000, 288000, 288000, 288000, 448000, 928000, 1824000, 6080000, 10240000, 18880000 } },	// Naszyj. Z Niebian.Lez
		{ 17000, 0x000, { 16000, 16000, 16000, 16000, 128000, 128000, 128000, 240000, 400000, 768000 } },	// Drewniane Kolczyki
		{ 17020, 0x000, { 144000, 144000, 144000, 144000, 256000, 256000, 256000, 352000, 704000, 1184000 } },	// Miedziane Kolczyki
		{ 17040, 0x000, { 48000, 48000, 48000, 48000, 128000, 128000, 128000, 240000, 400000, 768000 } },	// Srebrne Kolczyki
		{ 17060, 0x000, { 144000, 144000, 144000, 144000, 256000, 256000, 256000, 384000, 672000, 1120000 } },	// Zlote Kolczyki
		{ 17080, 0x000, { 192000, 192000, 192000, 192000, 288000, 480000, 992000, 1536000, 3040000, 4160000 } },	// Jadeitowe Kolczyki
		{ 17100, 0x000, { 304000, 304000, 304000, 304000, 416000, 720000, 1024000, 1632000, 4640000, 8000000 } },	// Ebonitowe Kolczyki
		{ 17120, 0x000, { 272000, 272000, 272000, 272000, 352000, 640000, 992000, 1536000, 3680000, 6720000 } },	// Perlowe Kolczyki
		{ 17140, 0x000, { 176000, 176000, 176000, 176000, 272000, 448000, 896000, 1248000, 2400000, 3200000 } },	// Kolczyki Z Bial. Zlota
		{ 17160, 0x000, { 176000, 176000, 176000, 176000, 272000, 448000, 1024000, 1888000, 2848000, 4800000 } },	// Krysztalowe Kolczyki
		{ 17180, 0x000, { 153600, 153600, 153600, 153600, 272000, 448000, 1024000, 1888000, 2848000, 4800000 } },	// Ametystowe Kolczyki
		{ 17200, 0x000, { 400000, 400000, 400000, 400000, 576000, 1344000, 1920000, 5120000, 12160000, 16480000 } },	// Kolczyki Z Niebian.Lez
	};

	// Upgrade materials ("ULEPSZACZE"), and then everything else he prices by
	// name - the Moonlight chest, the Blessing Scroll, the horse medal, herbs,
	// guild materials, ores and smelted ores - in the second table.
	struct TPlayerBotMaterialPrice { DWORD dwVnum; DWORD dwPrice; };
	const TPlayerBotMaterialPrice PLAYERBOT_MATERIAL_PRICES[] = {
		{ 27799,   166665 },	// Rybia Osc
		{ 27987,   362576 },	// Malz
		{ 27992,  1981530 },	// Biala Perla
		{ 27993,  1746376 },	// Niebieska Perla
		{ 27994,  1275850 },	// Krwawa Perla
		{ 30003,    38539 },	// Nos Swini
		{ 30004,    42507 },	// Zab Dzika
		{ 30005,    65766 },	// Kawalek Zepsutej Zbroi
		{ 30006,   232441 },	// Zab Orka
		{ 30007,   102051 },	// Amulet Orka
		{ 30008,   192763 },	// Ezoteryczny Przewodnik
		{ 30009,   104302 },	// Nieznane Lekarstwo
		{ 30010,    78231 },	// Zolc Niedzwiedzia
		{ 30011,    20410 },	// Klab
		{ 30014,    79373 },	// Futro Yeti
		{ 30015,   187085 },	// Pamiatka Po Demonie
		{ 30016,    90712 },	// Klejnot Demona
		{ 30017,    48749 },	// Ozdobna Spinka Do Wlosow
		{ 30018,   113390 },	// Czerwona Wstega
		{ 30019,   102051 },	// Plonaca Grzywa
		{ 30021,   140604 },	// Kawalek Klejnotu
		{ 30022,   107712 },	// Ogon Weza
		{ 30023,    18142 },	// Futro Bialego Tygrysa
		{ 30025,    90695 },	// Worek Z Pajecza Trucizna
		{ 30027,    21536 },	// Futro Wilka
		{ 30028,    43639 },	// Szpon Wilka
		{ 30030,    70285 },	// Zardzewiale Ostrze
		{ 30031,    69159 },	// Ornament
		{ 30032,   102051 },	// Czarny Uniform
		{ 30033,    59517 },	// Stluczona Porcelana
		{ 30034,    56695 },	// Biala Wstega
		{ 30035,    46481 },	// Krem Do Twarzy
		{ 30037,    43075 },	// Szpon Tygrysa
		{ 30038,    20410 },	// Skora Tygrysa
		{ 30039,    40820 },	// Kawalek Plotna
		{ 30040,    39678 },	// Lisc
		{ 30041,    67463 },	// Shuriken
		{ 30042,    51017 },	// Pazur Tygrysa
		{ 30045,    45356 },	// Igla Skorpiona
		{ 30046,    73695 },	// Ogon Skorpiona
		{ 30047,   190495 },	// Ksiega Klatw
		{ 30048,    68017 },	// Kawalek Lodu
		{ 30049,    44214 },	// Lodowy Rog Wieloryba
		{ 30050,   205217 },	// Matowy Lod
		{ 30051,   116783 },	// Nieznany Talizman
		{ 30052,    50456 },	// Flaga
		{ 30053,    64624 },	// Niedzwiedzia Skora
		{ 30055,   156478 },	// Szpon Skorpiona
		{ 30056,    60088 },	// Pajecza Siec
		{ 30057,   109980 },	// Oczy Pajaka
		{ 30058,   115641 },	// Worek Z Pajeczymi Jajami
		{ 30059,    72553 },	// Nogi Pajaka
		{ 30060,    79373 },	// Jezyk Zaby
		{ 30061,    91837 },	// Zabie Udka
		{ 30067,    44214 },	// Skora Weza
		{ 30069,    43639 },	// Szpon Wilka +
		{ 30070,    20407 },	// Futro Wilka +
		{ 30071,    49314 },	// Zolc Niedzwiedzia +
		{ 30072,    65749 },	// Niedzwiedzia Skora +
		{ 30073,    95248 },	// Biala Wstega +
		{ 30074,    48756 },	// Czarny Uniform +
		{ 30075,   293638 },	// Shuriken +
		{ 30076,    56695 },	// Amulet Orka +
		{ 30077,    70285 },	// Zab Orka +
		{ 30078,    77098 },	// Ezoteryczny Przewodnik +
		{ 30079,   128122 },	// Nieznany Talizman +
		{ 30080,    57820 },	// Ksiega Klatw +
		{ 30081,    46481 },	// Ogon Skorpiona +
		{ 30082,    62356 },	// Ogon Weza +
		{ 30083,   113373 },	// Nieznane Lekarstwo +
		{ 30084,   128122 },	// Nieznany Talizman +
		{ 30085,    45356 },	// Kawalek Plotna +
		{ 30086,    68034 },	// Pamiatka Po Demonie +
		{ 30087,    57820 },	// Klejnot Demona +
		{ 30088,    79373 },	// Kawalek Lodu +
		{ 30089,   124729 },	// Futro Yeti +
		{ 30090,    51017 },	// Matowy Lod +
		{ 30091,   126997 },	// Symbol Wojownika
		{ 30092,    53278 },	// Zdobycz Dzikusa
		{ 30116,    91837 },	// Zabie Udka
		{ 30271,    48749 },	// Ozdobna Spinka Do Wlosow
		{ 30343,   110500 },	// Mieso demona
		{ 30344,   127500 },	// Wysuszone Oczy
		{ 30345,   102000 },	// Ezoteryczne drewno
		{ 30346,    49300 },	// Niedzwiedzina
		{ 30347,    68000 },	// Gourou
		{ 30348,   161500 },	// Lodowata Maz
		{ 30349,   132600 },	// Zywe Drzewo
		{ 30350,    93500 },	// Pazury Malpy
		{ 30351,    93500 },	// Ogon Malpy
		{ 30352,    85000 },	// Orkowe Jadra
		{ 30353,    51000 },	// Wieprzowina
		{ 30354,   136000 },	// Czerwone nasiono
		{ 30355,   110500 },	// Zgnile Mieso
		{ 30356,    68000 },	// Waleczna dusza
		{ 30357,    59500 },	// Amulet Wojownika
		{ 30358,    91800 },	// Serce Wojownika
		{ 30359,    71400 },	// Piasek Pustyni
		{ 30367,   151300 },	// Luski Smoka
		{ 35002,   104302 },	// Nieznane Lekarstwo
	};
	const TPlayerBotMaterialPrice PLAYERBOT_EXTRA_MATERIAL_PRICES[] = {
		{ 25040,   550000 },	// Zwoj Blogoslawienstwa
		{ 25043,  1500000 },	// Podrecznik Kowala
		{ 25044,   400000 },	// Zwoj Wojny
		{ 25045,  1000000 },	// Zwoj Boga Smokow
		{ 25100,    40000 },	// Zwoj Kamienia Duszy
		{ 27100,      400 },	// Zielona Mikstura(M)
		{ 27101,     1200 },	// Zielona Mikstura(S)
		{ 27102,     2000 },	// Zielona Mikstura(D)
		{ 27103,      500 },	// Fioletowa Mikstura(M)
		{ 27104,     1500 },	// Fioletowa Mikstura(S)
		{ 27105,     3000 },	// Fioletowa Mikstura(D)
		{ 27110,      400 },	// Zielona Mikstura(M)
		{ 27111,     1200 },	// Zielona Mikstura(S)
		{ 27112,     2000 },	// Zielona Mikstura(D)
		{ 27113,      500 },	// Fioletowa Mikstura(M)
		{ 27114,     1500 },	// Fioletowa Mikstura(S)
		{ 27115,     3000 },	// Fioletowa Mikstura(D)
		{ 27798,    15000 },	// Skamieniala Krewetka
		{ 30378,    40000 },	// Materialy Rzemieslnicze
		{ 39002,   500000 },	// Pierscien Doswiadczenia
		{ 39006,    55000 },	// Peleryna Mestwa
		{ 39028,  2000000 },	// Zaczarowanie Przedmiotu
		{ 39029,  1900000 },	// Wzmocnienie Przedmiotu
		{ 50006,    55000 },	// Zlota Szkatulka
		{ 50007,    35000 },	// Srebrna Szkatulka
		{ 50008,    90000 },	// Zloty klucz
		{ 50009,    72000 },	// Srebrny klucz
		{ 50011,   100000 },	// Szkat. Blasku Ksiezyca
		{ 50012,    85000 },	// Zlota Szkatulka	+
		{ 50024,    65000 },	// Roza
		{ 50025,    65000 },	// Czekolada
		{ 50031,    65000 },	// Roza
		{ 50037,    75000 },	// Heksagonalna Szkatulka
		{ 50050,   120000 },	// Medal konny
		{ 50054,     5000 },	// Siano
		{ 50055,    65000 },	// Marchewka
		{ 50056,    75000 },	// Czerwony Zen-szen
		{ 50060,    95000 },	// Instr. Jazdy Konnej
		{ 50061,   135500 },	// Instr. Oswajania Konia
		{ 50062,   125000 },	// Instr. Walki Konno
		{ 50301,    60000 },	// Sztuka Wojny Sun Zi
		{ 50302,    60000 },	// Sztuka Wojny Wu Zi
		{ 50303,    60000 },	// WeiLiao Zi
		{ 50304,    95000 },	// Sztuka Combo
		{ 50305,   125000 },	// Zaaw. Sztuka Combo
		{ 50306,   150000 },	// Mistrz. Sztuka Combo
		{ 50307,    70000 },	// Ksiega Misji (Latwa)
		{ 50308,   120000 },	// Ksiega Misji (Normalna)
		{ 50309,   250000 },	// Ksiega Misji (Trudna)
		{ 50310,   500000 },	// Ksiega Misji (ekspert)
		{ 50314,    75000 },	// Ksiega Polimorfii
		{ 50315,    95000 },	// Zaaw. Ks. Polimorfii
		{ 50316,   125000 },	// Mistrz. Ks. Polimorfii
		{ 50318,    70000 },	// Ksiega Misji (Latwa)
		{ 50319,   120000 },	// Ksiega Misji (Normalna)
		{ 50320,   250000 },	// Ksiega Misji (Trudna)
		{ 50321,   500000 },	// Ksiega Misji (ekspert)
		{ 50513,   750000 },	// Kamien duchowy
		{ 50600,    90000 },	// Przewodnik do zbieractwa
		{ 50601,     3184 },	// Diamentowy Kamien
		{ 50603,      290 },	// Skamienialy Pien
		{ 50604,      298 },	// Ruda Miedzi
		{ 50605,     1160 },	// Ruda Srebra
		{ 50606,      436 },	// Ruda Zlota
		{ 50607,      290 },	// Ruda Jadeitu
		{ 50608,     2320 },	// Ruda Ebonitu
		{ 50609,     1202 },	// Kawalek Perly
		{ 50610,     2195 },	// Ruda Bialego Zlota
		{ 50611,     1016 },	// Ruda Krysztalu
		{ 50612,     1043 },	// Ruda Ametystu
		{ 50613,     2466 },	// Ruda Niebianskich Lez
		{ 50621,   707560 },	// Diament
		{ 50623,   160970 },	// Skamieniale Drewno
		{ 50624,   166277 },	// Miedz
		{ 50625,   240570 },	// Srebro
		{ 50626,    90568 },	// Zloto
		{ 50627,   158847 },	// Jadeit
		{ 50628,   638219 },	// Ebonit
		{ 50629,   389158 },	// Perla
		{ 50630,   488216 },	// Biale Zloto
		{ 50631,   336091 },	// Krysztal
		{ 50632,   371469 },	// Ametyst
		{ 50633,   667229 },	// Niebianskie Lzy
		{ 50701,     3800 },	// Kwiat Brzoskwini
		{ 50702,     1710 },	// Pokrzywa
		{ 50703,     1805 },	// Kwiat Kaki
		{ 50704,     1425 },	// Korzen Gango
		{ 50705,     2280 },	// Bez
		{ 50706,     1900 },	// Grzyb Tue
		{ 50707,     2375 },	// Roza Alpejska
		{ 50708,     2280 },	// Morwa
		{ 50709,     7600 },	// Mniszek Lekarski
		{ 50710,     2850 },	// Oset
		{ 50721,     3800 },	// Kwiat Brzoskwini
		{ 50722,     1710 },	// Pokrzywa
		{ 50723,     1805 },	// Kwiat Kaki
		{ 50724,     1425 },	// Korzen Gango
		{ 50725,     2280 },	// Bez
		{ 50726,     1900 },	// Grzyb Tue
		{ 50727,     2375 },	// Roza Alpejska
		{ 50728,     2280 },	// Morwa
		{ 50729,     7600 },	// Mniszek Lekarski
		{ 50730,     2850 },	// Oset
		{ 50731,    19000 },	// Gwiazda Nocy
		{ 50732,    19000 },	// Sniezny Kwiat
		{ 50733,    19000 },	// Bursztynowy Platek
		{ 50734,    19000 },	// Perlowe Wiechy
		{ 50735,    19000 },	// Korzen Weza
		{ 50909,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50910,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50911,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50912,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50913,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50914,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50915,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50916,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50917,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50918,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50919,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50920,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50921,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50922,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50923,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50924,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50925,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50926,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50927,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50928,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50929,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50930,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50931,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50932,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50933,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50934,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50935,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50936,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50937,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50938,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50939,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50940,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50941,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50942,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50943,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50944,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50945,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50946,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 50947,   450000 },	// WSZYSTKIE RECEPTURY np. Zielony Wywar, Platynowy Wywar, Szary wywar itd.
		{ 70005,   500000 },	// Pierscien Doswiadczenia
		{ 70007,    75000 },	// Pierscien Niejawnosci
		{ 70008,    50000 },	// Biala Flaga
		{ 70014,    25000 },	// Pigulka Krwi
		{ 70038,    55000 },	// Peleryna Mestwa
		{ 70039,  1500000 },	// Podrecznik Kowala
		{ 70042,   300000 },	// Zaczarowana Rekawica
		{ 70043,   750000 },	// Rekawica Zlodzieja
		{ 70048,    70000 },	// Plaszcz Uciekiniera
		{ 70049,    82000 },	// Pierscien Lucy
		{ 70050,    65000 },	// Symb. Krola Przepowiedni
		{ 70051,    65000 },	// Rekawica Krola Przepow.
		{ 70057,    55000 },	// Peleryna Mestwa
		{ 70102,    65500 },	// Fasolka zen
		{ 70138,    55000 },	// Peleryna Mestwa
		{ 70201,    20000 },	// Wybielacz
		{ 70202,    25000 },	// Biala Farba Do Wlosow
		{ 70203,    12000 },	// Blond Farba Do Wlosow
		{ 70204,    25000 },	// Czerwona Farba Do Wlosow
		{ 70205,    11000 },	// Brazowa Farba Do Wlosow
		{ 70207,    20000 },	// Wybielacz
		{ 71015,   500000 },	// Pierscien Doswiadczenia
		{ 71016,   750000 },	// Rekawica Zlodzieja
		{ 71021,   400000 },	// Zwoj Wojny
		{ 71032,  1000000 },	// Zwoj Boga Smokow
		{ 71084,  2000000 },	// Zaczarowanie Przedmiotu
		{ 71085,  1900000 },	// Wzmocnienie Przedmiotu
		{ 71088,    70000 },	// Ksiega Misji (Latwa)
		{ 71089,   120000 },	// Ksiega Misji (Normalna)
		{ 71090,   250000 },	// Ksiega Misji (Trudna)
		{ 71092,    75000 },	// Ksiega Polimorfii
		{ 71284,  2000000 },	// Zaczarowanie Przedmiotu
		{ 71285,  1900000 },	// Wzmocnienie Przedmiotu
		{ 72001,   500000 },	// Pierscien Doswiadczenia
		{ 72002,   500000 },	// Pierscien Doswiadczenia
		{ 72003,   500000 },	// Pierscien Doswiadczenia
		{ 72004,   750000 },	// Rekawica Zlodzieja
		{ 72005,   750000 },	// Rekawica Zlodzieja
		{ 72006,   750000 },	// Rekawica Zlodzieja
		{ 72049,   500000 },	// Pierscien Doswiadczenia
		{ 72050,   500000 },	// Pierscien Doswiadczenia
		{ 72101,   500000 },	// Pierscien Doswiadczenia
		{ 72102,   500000 },	// Pierscien Doswiadczenia
		{ 72111,   750000 },	// Rekawica Zlodzieja
		{ 72112,   750000 },	// Rekawica Zlodzieja
		{ 76007,    55000 },	// Peleryna Mestwa
		{ 90010,    12000 },	// Kamien Wegielny
		{ 90011,     7000 },	// Pien
		{ 90012,    20000 },	// Dykta
	};

	// Skill books by the skill in socket 0, before the per-listing jitter.
	struct TPlayerBotBookPrice { DWORD dwSkill; DWORD dwPrice; };
	const TPlayerBotBookPrice PLAYERBOT_BOOK_PRICES[] = {
		{   1,   52372 },	// Trzystronne Ciecie
		{   2,  162101 },	// Wir Miecza
		{   3,  224448 },	// Berserk
		{   4,  550568 },	// Aura Miecza
		{   5,   67334 },	// Szarza
		{  16,  162101 },	// Duchowe Uderzenie
		{  17,   37407 },	// Tapniecie
		{  18,   37407 },	// Uderzenie Miecza
		{  19,  289672 },	// Silne Cialo
		{  20,   69827 },	// Walniecie
		{  31,  112224 },	// Zasadzka
		{  32,   39902 },	// Szybki Atak
		{  33,  112224 },	// Wirujacy Sztylet
		{  34,   62347 },	// Krycie Sie
		{  35,   39902 },	// Trujaca Chmura
		{  46,   39902 },	// Powtarzalny Strzal
		{  47,   32420 },	// Deszcz Strzal
		{  48,  137162 },	// Ognista Strzala
		{  49,   24940 },	// Bezszelestny Chod
		{  50,  112224 },	// Trujaca Strzala
		{  61,   22445 },	// Uderzenie Palcem
		{  62,   39902 },	// Smoczy Wir
		{  63,  364487 },	// Czarowane Ostrze
		{  64,  237875 },	// Strach
		{  65,  112224 },	// Czarowana Zbroja
		{  66,   62347 },	// Rozproszenie Magii
		{  76,   54865 },	// Mroczne Uderzenie
		{  77,   64840 },	// Ogniste Uderzenie
		{  78,  112224 },	// Ognisty Duch
		{  79,   99754 },	// Mroczna Ochrona
		{  80,   29927 },	// Duchowy Cios
		{  81,   24940 },	// Mroczny Kamien
		{  91,   22445 },	// Latajacy Talizman
		{  92,   29927 },	// Strzelajacy Smok
		{  93,   77309 },	// Smoczy Skowyt
		{  94,  174571 },	// Blogoslawienstwo
		{  95,   49877 },	// Odbicie
		{  96,  187039 },	// Pomoc Smoka
		{ 106,   37407 },	// Blyskawiczny Rzut
		{ 107,   62347 },	// Przywolanie Blyskawicy
		{ 108,   44890 },	// Szpon Blyskawicy
		{ 109,  112224 },	// Leczenie
		{ 110,   44890 },	// Zwinnosc
		{ 111,   27432 },	// Burza
	};

	// Soul stones by kind and grade. The kinds are the last two digits of the
	// vnum (30-37 weapon, 38-43 armour) and the grade its hundreds digit.
	struct TPlayerBotSoulStonePrice { int iKind; int iGrade; DWORD dwPrice; };
	const TPlayerBotSoulStonePrice PLAYERBOT_SOUL_STONE_PRICES[] = {
		{ 30, 4,   539019 },
		{ 31, 3,   251875 },
		{ 31, 4,  1293938 },
		{ 32, 4,   255990 },
		{ 33, 4,   354713 },
		{ 34, 4,   323325 },
		{ 35, 4,   348435 },
		{ 36, 4,   323325 },
		{ 37, 3,   252000 },
		{ 37, 4,  1707813 },
		{ 38, 4,   719713 },
		{ 39, 4,   738545 },
		{ 40, 4,   143438 },
		{ 41, 4,   366794 },
		{ 42, 4,   491464 },
		{ 43, 2,   141750 },
		{ 43, 3,   302500 },
		{ 43, 4,   964975 },
	};
	// What a stone of that grade is worth when its kind is not named above.
	// Grade four has no general price in his table: every +4 is listed by name.
	const DWORD PLAYERBOT_SOUL_STONE_GRADE_PRICES[5] = { 45313, 62525, 98725, 170125, 0 };

	// A stone seated in a weapon or armour raises what the piece is worth.
	// First by how many are in it, then by which ones (percent, 100 = x1.0).
	const int PLAYERBOT_SOCKET_COUNT_PERCENT[4] = { 100, 120, 130, 150 };
	struct TPlayerBotSocketStoneMultiplier { int iKind; int iGrade; int iPercent; };
	const TPlayerBotSocketStoneMultiplier PLAYERBOT_SOCKET_STONE_PERCENT[] = {
		{ 30, 4, 140 },
		{ 31, 3, 140 },
		{ 31, 4, 160 },
		{ 32, 4, 120 },
		{ 33, 4, 130 },
		{ 34, 4, 120 },
		{ 35, 4, 130 },
		{ 36, 4, 110 },
		{ 37, 3, 140 },
		{ 37, 4, 180 },
		{ 38, 4, 160 },
		{ 39, 4, 140 },
		{ 40, 4, 120 },
		{ 41, 4, 130 },
		{ 42, 4, 130 },
		{ 43, 3, 120 },
		{ 43, 4, 140 },
	};

	// Polymorph marbles, by the monster in socket 0. Anything not named here
	// is drawn from the band below, stable per marble.
	struct TPlayerBotMarblePrice { DWORD dwMob; DWORD dwPrice; };
	const TPlayerBotMarblePrice PLAYERBOT_MARBLE_PRICES[] = {
		{   502,  162500 },	// Dziki Sluga
		{   701,  165000 },	// Ezoteryczny Fanatyk
		{   731,  187500 },	// Elit. Ezot. Fanatyk
		{   751,  165000 },	// Wysoki Fanatyk
		{   771,  187500 },	// Best. Fanatyk
		{  1402,  100000 },	// Wojownik Z Toporem
		{  1403,  125000 },	// Tysieczny Wojownik
		{  1601,  112500 },	// Ogr Wojownik
		{  2001,  162500 },	// Mlody Pajak
		{  2002,  175000 },	// Trujacy Pajak
		{  2051,  162500 },	// Podly Mlody Truj. Pajak
		{  2052,  162500 },	// Podly Smier. Truj. Pajak
		{  2061,  187500 },	// Maly Trujacy Pajak v2
	};
	const DWORD PLAYERBOT_MARBLE_PRICE_MIN = 65000;
	const DWORD PLAYERBOT_MARBLE_PRICE_MAX = 85000;

	// Forgetting Scrolls by the skill in the socket. A price of zero is his
	// "do sprzedazy u handlarki": that one is the merchant's, not a counter's.
	struct TPlayerBotForgetScrollPrice { DWORD dwSkill; DWORD dwPrice; };
	const TPlayerBotForgetScrollPrice PLAYERBOT_FORGET_SCROLL_PRICES[] = {
		{   1,  131198 },	// OZ Trzystronne Ciecie
		{   2,  155564 },	// OZ Wir Miecza
		{   3,  341240 },	// OZ Berserk
		{   4,   21000 },	// OZ Aura Miecza
		{   5,  146192 },	// OZ Szarza
		{  16,  199296 },	// OZ Duchowe Uderzenie
		{  17,  130574 },	// OZ Tapniecie
		{  18,  113705 },	// OZ Uderzenie Miecza
		{  19,   21000 },	// OZ Silne Cialo
		{  20,  145442 },	// OZ Walniecie
		{  31,  150940 },	// OZ Zasadzka
		{  32,  148067 },	// OZ Szybki Atak
		{  33,  150940 },	// OZ Wirujacy Sztylet
		{  34,  140570 },	// OZ Krycie Sie
		{  35,  198047 },	// OZ Trujaca Chmura
		{  46,  100960 },	// OZ Powtarzalny Strzal
		{  47,   98087 },	// OZ Deszcz Strzal
		{  48,   21000 },	// OZ Ognista Strzala
		{  49,   89340 },	// OZ Bezszelestny Chod
		{  50,  106084 },	// OZ Trujaca Strzala
		{  61,  162435 },	// OZ Uderzenie Palcem
		{  62,  129948 },	// OZ Smoczy Wir
		{  63,   21000 },	// OZ Czarowane Ostrze
		{  64,  262395 },	// OZ Strach
		{  65,  165560 },	// OZ Czarowana Zbroja
		{  66,  148067 },	// OZ Rozproszenie Magii
		{  76,  117203 },	// OZ Mroczne Uderzenie
		{  77,  110706 },	// OZ Ogniste Uderzenie
		{  78,   21000 },	// OZ Ognisty Duch
		{  79,  191923 },	// OZ Mroczna Ochrona
		{  80,   89964 },	// OZ Duchowy Cios
		{  81,  123077 },	// OZ Mroczny Kamien
		{  91,  121827 },	// OZ Latajacy Talizman
		{  92,  113705 },	// OZ Strzelajacy Smok
		{  93,   87465 },	// OZ Smoczy Skowyt
		{  94,  231907 },	// OZ Blogoslawienstwo
		{  95,  175306 },	// OZ Odbicie
		{  96,   21000 },	// OZ Pomoc Smoka
		{ 106,   87841 },	// OZ Blyskawiczny Rzut
		{ 107,  104208 },	// OZ Przywolanie Blyskawicy
		{ 108,  123077 },	// OZ Szpon Blyskawicy
		{ 109,   21000 },	// OZ Leczenie
		{ 110,  140570 },	// OZ Zwinnosc
		{ 111,   79344 },	// OZ Burza
	};

	// His bonus multipliers: per slot, per line, one multiplier for the maximum
	// roll and one for any other value, the races split at level 33; a weapon's
	// two damage lines by tiers of their value. They compound into the asking
	// price (GetPlayerBotBonusPricePercent). "Maximum" is the engine's own:
	// g_map_itemAttr's top value for the apply on the item's attribute set.
	// Lines his sheet does not name, or names at x1.0, multiply by nothing. The
	// percent points are hundredths: 250 is x2.5.
	enum EPlayerBotPriceSlot
	{
		PRICE_SLOT_HEAD = 1, PRICE_SLOT_BODY = 2, PRICE_SLOT_SHIELD = 4, PRICE_SLOT_FOOTS = 8,
		PRICE_SLOT_WRIST = 16, PRICE_SLOT_NECK = 32, PRICE_SLOT_EAR = 64, PRICE_SLOT_WEAPON = 128
	};
	struct TPlayerBotBonusPriceRow
	{
		BYTE bSlots;      // EPlayerBotPriceSlot mask
		BYTE bApply;      // APPLY_*
		WORD wMaxPct;     // the maximum roll, hundredths
		WORD wOtherPct;   // any other value, hundredths
		BYTE bMinLevel;   // the item's level limit band, inclusive
		BYTE bMaxLevel;
	};
	const TPlayerBotBonusPriceRow PLAYERBOT_BONUS_PRICE_ROWS[] = {
		{ PRICE_SLOT_HEAD, APPLY_ATTBONUS_ANIMAL, 230, 130, 0, 32 },	// silny przeciwko zwierzetom
		{ PRICE_SLOT_SHIELD | PRICE_SLOT_WRIST | PRICE_SLOT_EAR | PRICE_SLOT_WEAPON, APPLY_ATTBONUS_ANIMAL, 250, 130, 0, 32 },	// silny przeciwko zwierzetom
		{ PRICE_SLOT_HEAD | PRICE_SLOT_SHIELD | PRICE_SLOT_WRIST | PRICE_SLOT_EAR | PRICE_SLOT_WEAPON, APPLY_ATTBONUS_ANIMAL, 150, 115, 33, 255 },	// silny przeciwko zwierzetom
		{ PRICE_SLOT_HEAD, APPLY_ATTBONUS_DEVIL, 230, 150, 0, 255 },	// silny przeciwko diablom
		{ PRICE_SLOT_SHIELD | PRICE_SLOT_WRIST | PRICE_SLOT_EAR | PRICE_SLOT_WEAPON, APPLY_ATTBONUS_DEVIL, 240, 150, 0, 255 },	// silny przeciwko diablom
		{ PRICE_SLOT_HEAD, APPLY_ATTBONUS_HUMAN, 200, 115, 0, 255 },	// silny przeciwko ludziom
		{ PRICE_SLOT_SHIELD, APPLY_ATTBONUS_HUMAN, 170, 140, 0, 255 },	// silny przeciwko ludziom
		{ PRICE_SLOT_WRIST, APPLY_ATTBONUS_HUMAN, 190, 140, 0, 255 },	// silny przeciwko ludziom
		{ PRICE_SLOT_EAR, APPLY_ATTBONUS_HUMAN, 250, 140, 0, 255 },	// silny przeciwko ludziom
		{ PRICE_SLOT_WEAPON, APPLY_ATTBONUS_HUMAN, 160, 120, 0, 255 },	// silny przeciwko ludziom
		{ PRICE_SLOT_HEAD | PRICE_SLOT_SHIELD | PRICE_SLOT_WRIST | PRICE_SLOT_EAR | PRICE_SLOT_WEAPON, APPLY_ATTBONUS_MILGYO, 150, 110, 0, 255 },	// silny przeciwko mistykom
		{ PRICE_SLOT_HEAD | PRICE_SLOT_SHIELD | PRICE_SLOT_WRIST | PRICE_SLOT_EAR, APPLY_ATTBONUS_ORC, 180, 110, 0, 32 },	// silny przeciwko orkom
		{ PRICE_SLOT_WEAPON, APPLY_ATTBONUS_ORC, 150, 110, 0, 32 },	// silny przeciwko orkom
		{ PRICE_SLOT_HEAD | PRICE_SLOT_SHIELD | PRICE_SLOT_WRIST | PRICE_SLOT_EAR | PRICE_SLOT_WEAPON, APPLY_ATTBONUS_ORC, 220, 130, 33, 255 },	// silny przeciwko orkom
		{ PRICE_SLOT_HEAD | PRICE_SLOT_SHIELD | PRICE_SLOT_WRIST | PRICE_SLOT_EAR | PRICE_SLOT_WEAPON, APPLY_ATTBONUS_UNDEAD, 200, 110, 0, 32 },	// silny przeciwko nieumarlym
		{ PRICE_SLOT_HEAD, APPLY_ATTBONUS_UNDEAD, 230, 150, 33, 255 },	// silny przeciwko nieumarlym
		{ PRICE_SLOT_SHIELD | PRICE_SLOT_WRIST | PRICE_SLOT_EAR | PRICE_SLOT_WEAPON, APPLY_ATTBONUS_UNDEAD, 250, 150, 33, 255 },	// silny przeciwko nieumarlym
		{ PRICE_SLOT_BODY, APPLY_ATT_GRADE_BONUS, 210, 170, 0, 255 },	// wartosc ataku
		{ PRICE_SLOT_HEAD, APPLY_ATT_SPEED, 200, 120, 0, 255 },	// szybkosc ataku
		{ PRICE_SLOT_FOOTS, APPLY_ATT_SPEED, 170, 130, 0, 255 },	// szbykosc ataku
		{ PRICE_SLOT_SHIELD, APPLY_BLOCK, 200, 150, 0, 255 },	// szansa na blok ciosu
		{ PRICE_SLOT_BODY, APPLY_CAST_SPEED, 160, 115, 0, 255 },	// szybkosc zaklecia
		{ PRICE_SLOT_WEAPON, APPLY_CAST_SPEED, 130, 105, 0, 255 },	// szybkosc zaklecia
		{ PRICE_SLOT_SHIELD, APPLY_CON, 160, 140, 0, 255 },	// witalnosc
		{ PRICE_SLOT_WEAPON, APPLY_CON, 140, 110, 0, 255 },	// witalnosc
		{ PRICE_SLOT_FOOTS | PRICE_SLOT_NECK, APPLY_CRITICAL_PCT, 200, 160, 0, 255 },	// szansa na cios krytyczny
		{ PRICE_SLOT_WEAPON, APPLY_CRITICAL_PCT, 170, 140, 0, 255 },	// szansa na cios krytyczny
		{ PRICE_SLOT_SHIELD, APPLY_DEX, 170, 140, 0, 255 },	// zrecznosc
		{ PRICE_SLOT_WEAPON, APPLY_DEX, 150, 110, 0, 255 },	// zrecznosc
		{ PRICE_SLOT_HEAD, APPLY_DODGE, 200, 140, 0, 255 },	// szansa na unik. strzaly
		{ PRICE_SLOT_FOOTS, APPLY_DODGE, 160, 120, 0, 255 },	// szansa na unik. strzaly
		{ PRICE_SLOT_SHIELD, APPLY_GOLD_DOUBLE_BONUS, 250, 170, 0, 255 },	// szansa na podwojna ilosc yang
		{ PRICE_SLOT_FOOTS, APPLY_GOLD_DOUBLE_BONUS, 200, 150, 0, 255 },	// szansa na podwojna ilosc yang
		{ PRICE_SLOT_NECK, APPLY_GOLD_DOUBLE_BONUS, 220, 150, 0, 255 },	// szansa na podwojna ilosc yang
		{ PRICE_SLOT_HEAD | PRICE_SLOT_NECK, APPLY_HP_REGEN, 130, 110, 0, 255 },	// regeneracja mikstur pz
		{ PRICE_SLOT_SHIELD, APPLY_IMMUNE_SLOW, 120, 120, 0, 255 },	// niewrazliwy na spowolnienie
		{ PRICE_SLOT_SHIELD, APPLY_IMMUNE_STUN, 250, 250, 0, 255 },	// niewrazliwy na omdlenie
		{ PRICE_SLOT_SHIELD, APPLY_INT, 160, 140, 0, 255 },	// inteligencja
		{ PRICE_SLOT_WEAPON, APPLY_INT, 140, 110, 0, 255 },	// inteligencja
		{ PRICE_SLOT_FOOTS | PRICE_SLOT_NECK, APPLY_MALL_EXPBONUS, 160, 125, 0, 255 },	// punkty doswiadczenia +%
		{ PRICE_SLOT_WRIST | PRICE_SLOT_EAR, APPLY_MANA_BURN_PCT, 120, 100, 0, 255 },	// szansa na kradziez pe
		{ PRICE_SLOT_BODY, APPLY_MAX_HP, 210, 170, 0, 255 },	// maks. pz
		{ PRICE_SLOT_FOOTS | PRICE_SLOT_WRIST | PRICE_SLOT_NECK, APPLY_MAX_HP, 190, 180, 0, 255 },	// maks. pz
		{ PRICE_SLOT_FOOTS | PRICE_SLOT_WRIST | PRICE_SLOT_NECK, APPLY_MAX_SP, 130, 110, 0, 255 },	// maks. pe
		{ PRICE_SLOT_HEAD, APPLY_MAX_STAMINA, 140, 110, 0, 255 },	// maks. stamina
		{ PRICE_SLOT_BODY, APPLY_MAX_STAMINA, 130, 110, 0, 255 },	// maks. stamina
		{ PRICE_SLOT_EAR, APPLY_MOV_SPEED, 250, 160, 0, 255 },	// szybkosc ruchu
		{ PRICE_SLOT_WRIST, APPLY_PENETRATE_PCT, 160, 130, 0, 255 },	// szansa na przeszywajace uderzenie
		{ PRICE_SLOT_NECK, APPLY_PENETRATE_PCT, 170, 130, 0, 255 },	// szansa na przeszywajace uderzenie
		{ PRICE_SLOT_WEAPON, APPLY_PENETRATE_PCT, 140, 115, 0, 255 },	// szansa na przeszywajace uderzenie
		{ PRICE_SLOT_HEAD, APPLY_POISON_PCT, 220, 150, 0, 255 },	// szansa na otrucie
		{ PRICE_SLOT_WEAPON, APPLY_POISON_PCT, 140, 120, 0, 255 },	// szansa na otrucie
		{ PRICE_SLOT_EAR, APPLY_POISON_REDUCE, 110, 100, 0, 255 },	// odpornosc na trucizny
		{ PRICE_SLOT_BODY, APPLY_REFLECT_MELEE, 130, 115, 0, 255 },	// szansa na dobicie ciosu
		{ PRICE_SLOT_SHIELD, APPLY_REFLECT_MELEE, 160, 110, 0, 255 },	// szansa na odbicie ciosu
		{ PRICE_SLOT_BODY | PRICE_SLOT_FOOTS | PRICE_SLOT_NECK | PRICE_SLOT_EAR, APPLY_RESIST_BELL, 150, 105, 0, 255 },	// odpornosc na dzwony
		{ PRICE_SLOT_BODY | PRICE_SLOT_NECK, APPLY_RESIST_BOW, 240, 130, 0, 255 },	// odpornosc na strzaly
		{ PRICE_SLOT_FOOTS, APPLY_RESIST_BOW, 160, 130, 0, 255 },	// odpornosc na strzaly
		{ PRICE_SLOT_EAR, APPLY_RESIST_BOW, 180, 115, 0, 255 },	// odpornosc na strzaly
		{ PRICE_SLOT_BODY | PRICE_SLOT_FOOTS, APPLY_RESIST_DAGGER, 160, 120, 0, 255 },	// odpornosc na sztylety
		{ PRICE_SLOT_NECK | PRICE_SLOT_EAR, APPLY_RESIST_DAGGER, 180, 120, 0, 255 },	// odpornosc na sztylety
		{ PRICE_SLOT_BODY | PRICE_SLOT_FOOTS | PRICE_SLOT_NECK | PRICE_SLOT_EAR, APPLY_RESIST_FAN, 150, 105, 0, 255 },	// odpornosc na wachlarze
		{ PRICE_SLOT_HEAD, APPLY_RESIST_MAGIC, 170, 115, 0, 255 },	// odpornosc na magie
		{ PRICE_SLOT_BODY, APPLY_RESIST_MAGIC, 150, 120, 0, 255 },	// odpornosc na magie
		{ PRICE_SLOT_WRIST, APPLY_RESIST_MAGIC, 160, 120, 0, 255 },	// odpornosc na magie
		{ PRICE_SLOT_BODY | PRICE_SLOT_FOOTS, APPLY_RESIST_SWORD, 160, 120, 0, 255 },	// odpornosc na miecze
		{ PRICE_SLOT_NECK | PRICE_SLOT_EAR, APPLY_RESIST_SWORD, 180, 120, 0, 255 },	// odpornosc na miecze
		{ PRICE_SLOT_BODY | PRICE_SLOT_FOOTS, APPLY_RESIST_TWOHAND, 160, 120, 0, 255 },	// odrponosc na bron dwureczna
		{ PRICE_SLOT_NECK | PRICE_SLOT_EAR, APPLY_RESIST_TWOHAND, 180, 120, 0, 255 },	// odrponosc na bron dwureczna
		{ PRICE_SLOT_BODY | PRICE_SLOT_WRIST, APPLY_STEAL_HP, 170, 160, 0, 255 },	// x% obrazen dodanych do pz
		{ PRICE_SLOT_BODY, APPLY_STEAL_SP, 120, 105, 0, 255 },	// x% obrazen dodanych do pe
		{ PRICE_SLOT_SHIELD, APPLY_STR, 170, 140, 0, 255 },	// sila
		{ PRICE_SLOT_WEAPON, APPLY_STR, 150, 110, 0, 255 },	// sila
		{ PRICE_SLOT_FOOTS | PRICE_SLOT_NECK, APPLY_STUN_PCT, 180, 150, 0, 255 },	// szansa na omdlenie
		{ PRICE_SLOT_WEAPON, APPLY_STUN_PCT, 170, 150, 0, 255 },	// szansa na omdlenie
#if defined(PLAYERBOT_ENGINE_MT2009)
		{ PRICE_SLOT_WRIST | PRICE_SLOT_EAR, APPLY_REFLECT_ARROW, 140, 110, 0, 255 },	// szansa na odbicie pocisku
		{ PRICE_SLOT_HEAD | PRICE_SLOT_BODY | PRICE_SLOT_WRIST, APPLY_SKILL_DURATION, 130, 110, 0, 255 },	// czas trwania umiejetnosci
		{ PRICE_SLOT_HEAD | PRICE_SLOT_BODY | PRICE_SLOT_WRIST, APPLY_ST_REGEN, 130, 110, 0, 255 },	// regeneracja st (staminy)
#endif
	};
	// A weapon's average and skill damage, by tier of the value.
	struct TPlayerBotDamageTier { BYTE bFrom; WORD wPct; };
	const TPlayerBotDamageTier PLAYERBOT_AVERAGE_DAMAGE_TIERS[] = {
		{ 0, 100 }, { 10, 120 }, { 20, 150 }, { 30, 250 }, { 40, 450 }, { 46, 800 }, { 51, 1200 }, { 56, 2400 }, { 60, 5000 }
	};
	const TPlayerBotDamageTier PLAYERBOT_SKILL_DAMAGE_TIERS[] = {
		{ 1, 120 }, { 11, 200 }, { 20, 400 }, { 25, 1400 }, { 30, 3000 }
	};
}

#endif
