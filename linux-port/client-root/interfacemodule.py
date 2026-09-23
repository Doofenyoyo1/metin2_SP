##
## Interface
##
import constInfo
import systemSetting
import wndMgr
import chat
import app
import player
import uiTaskBar
import ikashop
import net
import chr
if app.ENABLE_CONQUEROR_UI:
	import uicharacternew as uiCharacter
else:
	import uiCharacter
import uiInventory
import uiDragonSoul
import uiChat
import uiMessenger
import guild

import ui
import uiHelp
import uiWhisper
import uiPointReset
import uiShop
import uiExchange
import uiSystem
import uiRestart
import uiToolTip
import uiMiniMap
import uiParty
import uiSafebox
import uiGuild
import uiQuest
import uiCommon
import uiRefine
import uiEquipmentDialog
import uiGameButton
import uiTip
import uiCube
import miniMap
# ACCESSORY_REFINE_ADD_METIN_STONE
import uiSelectItem
# END_OF_ACCESSORY_REFINE_ADD_METIN_STONE
import uiScriptLocale
import uiMaintenance
import uiBusyAction
import uiHorseInventory
import uiReport

import event
import localeInfo
import background
import captcha

if app.ENABLE_ACCE_COSTUME_SYSTEM:
	import uiacce

if app.ENABLE_MOVE_CHANNEL:
	import uiMoveChannel

if app.ENABLE_WON_EXCHANGE_WINDOW:
	import uiWonExchange

import uiPrivateShopBuilder
import offlineShopBuilder
import offlineShopManage
import offlineShopGuest
import offlineShopHistory
import offlineShopSearch
import uiItemShop

import uiItemExchange
import uiPlayerStat
import flamewindPath

import eventManager
import uiFishing
import uiCraft
import uiAttributeList
import uiSpecialShop
import uiReputation
import uiPotionRecharge

import uiCaptcha

# F9 GM panel (2026-09-09). Merged directly into interfacemodule.py rather
# than living in its own uiGMPanel.py: EPack32 on this project only refreshes
# files the packed root.epk already knows about, it does not auto-add brand
# new filenames, so a standalone module here never made it into the archive.
# interfacemodule.py is already tracked, so its content updates fine.
GM_PANEL_LOOKUP_FIELD_ORDER = [
	"lk_name", "lk_level", "lk_job", "lk_hp", "lk_mp", "lk_exp", "lk_gold",
	"lk_st", "lk_ht", "lk_dx", "lk_iq", "lk_statpt", "lk_skillpt",
	"lk_subskillpt", "lk_map", "lk_lastplay", "lk_horselvl", "lk_horsept",
	"lk_account", "lk_ip", "lk_playtime", "lk_range",
]

GM_PANEL_LOOKUP_LAYOUT = [
	("lk_name",		"Nick:"),
	("lk_level",	"Level:"),
	("lk_job",		"Klasa:"),
	("lk_hp",		"HP:"),
	("lk_mp",		"MP:"),
	("lk_exp",		"Exp:"),
	("lk_gold",		"Yang:"),
	("lk_st",		"Sil:"),
	("lk_ht",		"Wit:"),
	("lk_dx",		"ZR:"),
	("lk_iq",		"Int:"),
	("lk_statpt",	"Punkty statusu:"),
	("lk_skillpt",	"Punkty umiej.:"),
	("lk_subskillpt","Punkty umiej. (dod.):"),
	("lk_map",		"Mapa (index):"),
	("lk_lastplay",	"Ostatnio online:"),
	("lk_horselvl",	"Level konia:"),
	("lk_horsept",	"Punkty konne:"),
	("lk_account",	"Konto:"),
	("lk_ip",		"IP:"),
	("lk_playtime",	"Czas gry (min):"),
	("lk_range",	"Punkty range:"),
]

GM_PANEL_JOB_NAMES = {
	0 : "Wojownik (M)", 1 : "Wojowniczka (K)",
	2 : "Ninja (M)", 3 : "Ninja (K)",
	4 : "Sura (M)", 5 : "Sura (K)",
	6 : "Szaman (M)", 7 : "Szamanka (K)",
}

GM_PANEL_PAGE_NAMES = [
	"page_main", "page_addgm", "page_lookup", "page_createitem",
	"page_ban", "page_spawnbots", "page_spawnmobs", "page_serverctrl",
	# Sub-pages of "Sprawdz Gracza"/"Spawn Mobow" opened by their action
	# buttons - not real tabs (no tab_* counterpart / no tab button), only
	# reachable via __SetPage from their parent tab and returning the same
	# way.
	"page_giveamount", "page_setvalue", "page_setstat", "page_skills",
	"page_gmcommands",
]
GM_PANEL_TAB_NAMES = [
	"tab_main", "tab_addgm", "tab_lookup", "tab_createitem",
	"tab_ban", "tab_spawnbots", "tab_spawnmobs", "tab_serverctrl",
]

GM_PANEL_TAB_LABELS = {
	"tab_main"			: "Glowny Panel",
	"tab_addgm"			: "Dodaj GM",
	"tab_lookup"		: "Sprawdz Gracza",
	"tab_createitem"	: "Utworz Przedmiot",
	"tab_ban"			: "Blokada Konta",
	"tab_spawnbots"		: "Spawn Botow",
	"tab_spawnmobs"		: "Spawn Mobow",
	"tab_serverctrl"	: "Sterowanie Serwerem",
}

# Nazwa (w tabeli player.web_admin_rates) -> etykieta pola na karcie
# "Sterowanie Serwerem" i atrybuty widgetow zbudowane pod ta nazwa
# (self.serverctrl<Exp/Drop/Yang>Edit/Status - patrz __BuildServerControlPage).
GM_PANEL_RATE_FIELDS = [
	("exp",  "Exp",  "Doswiadczenie:"),
	("drop", "Drop", "Drop Przedmiotow:"),
	("yang", "Yang", "Drop Yang:"),
]

# Karta "Sterowanie Serwerem" - waga/przelaczniki z playerbot_weights.tsv,
# ten sam plik (i te same opisy, kopiowane wprost stad) co strona /ai
# panelu webowego (admin_panel.py, T["aiw_*"]/T["aih_*"]/T["ai_*_help"]
# polskie teksty). Core gry czyta ten plik sam co 5 sekund - zadnego
# restartu, zadnego spool-request jak przy ratach.
#
# (key, tytul, opis, rodzaj, min, max):
#   "bool"  - przycisk ON/OFF (czerwony/zielony), nie suwak - min/max nieuzywane
#   "weight"- suwak 25-250, neutralnie 100 (dopisek rzadko/jak w grze/czesto)
#   "scrap" - suwak 0-100 (%), dopisek wylaczone/kazdy straganiarz
#   "chest" - suwak 1-100 (%) w kliencie; po drucie leci w promilach (x10),
#             bo tyle rozumie CONFIG i silnik (item_manager.cpp) - patrz
#             __OnAIWeightSliderMove/SetAIWeightsResult. -1 z serwera =
#             "nieustawione, uzyj CONFIG", suwak wtedy na 1% z etykieta "-".
GM_PANEL_AI_WEIGHT_ROWS = [
	("CHAT", "Boty pisza na czacie", "Napis nad glowa bota (poluje, idzie do "
		"kowala, lowi). Wylacz dla graczy, ktorym to przeszkadza. Wolanie na "
		"czacie swiata o ulepszeniu na +7/+8/+9 zostaje niezaleznie od tego.",
		"bool", 0, 1),
	("BOOKS", "Czytanie bez opoznienia dnia", "Gra kaze czekac okolo doby "
		"miedzy dwoma czytaniami tej samej umiejetnosci, wiec bot potrzebuje "
		"miesiaca, by przeczytac skill z M1 na G1, a ksiegi tymczasem "
		"zalegaja w plecaku. Wlaczone: bot czyta ksiege od razu, gdy ja ma - "
		"jedyny hamulec to sama gra. Wylaczone: dobowa przerwa gry bez zmian.",
		"bool", 0, 1),
	("NIGHT", "Noc/snieg po 22:00", "Miedzy 22:00 a 05:59 czasu serwera "
		"rdzen podnosi flage nocy - ta sama, ktora GM ustawia komenda "
		"/xmas_snow 1 - a rano ja opuszcza. Klient pokazuje nocne niebo i, "
		"bo to flaga swiateczna, snieg.",
		"bool", 0, 1),
	("SCRAP", "Boty zlomiarze", "Udzial straganiarzy, ktorzy wystawiaja na "
		"lade swoje slabe ulepszenia (+0 do +3) za grosze zamiast sprzedawac "
		"je NPC - zlom do palenia u kowala, jak na serwerach hard. Domyslnie "
		"wylaczone.",
		"scrap", 0, 100),
	("REST", "Odpoczynek w miescie", "Udzial botow, ktore po sprawunku w "
		"miescie zostaja na chwile przy straganach zamiast wracac od razu na "
		"polowanie. 100% to zywe miasto, 0% - wszyscy w terenie.",
		"scrap", 0, 100),
	("CHEST", "Szkatulka Ksiezycowa - z zabitego potwora", "Jak czesto "
		"wypada szkatulka: z zabitego potwora i z rozbitego Metina. "
		"Domyslnie w grze 1% i 30%; wiecej szkatulek to wiecej zwojow "
		"bonusow, mikstur szybkosci i Zwojow Blogoslawienstwa u botow. "
		"Dziala w piec sekund, dla botow i graczy tak samo.",
		"chest", 1, 100),
	("CHEST_STONE", "Szkatulka Ksiezycowa - z rozbitego Metina",
		"Druga polowa ustawienia powyzej - osobna szansa liczona przy "
		"rozbiciu kamienia Metin, zamiast przy zabiciu zwyklego potwora.",
		"chest", 1, 100),
	("RESTOCK", "Kupowanie mikstur", "Powrot do miasta, gdy tylko koncza "
		"sie czerwone mikstury.",
		"weight", 25, 250),
	("REFINE", "Kowal", "Ulepszanie broni i pancerza zamiast polowania.",
		"weight", 25, 250),
	("SKILL", "Ksiegi umiejetnosci", "Czytanie ksiag, by pchnac "
		"umiejetnosc z M w strone G.",
		"weight", 25, 250),
	("HORSE", "Kon", "Stajnia i polowanie na medale w Lochu Malp.",
		"weight", 25, 250),
	("BIOLOG", "Biolog", "Zbieranie dla Biologa zamiast bicia poziomow.",
		"weight", 25, 250),
	("METIN", "Kamienie Metin", "Polowanie na metiny zamiast na zwykle "
		"potwory.",
		"weight", 25, 250),
	("PARTY", "Grupy (PT)", "Walka razem, a nie kazdy bot na wlasna reke.",
		"weight", 25, 250),
	("HUNTING", "Misje polowania", "Polowanie na awans na mapie, ktora "
		"wskazuje misja.",
		"weight", 25, 250),
	("LEVEL", "Zwykle bicie potworow", "To, co bot robi, gdy nic innego "
		"sie nie dopomina. Podnies, a sprawunki przegraja.",
		"weight", 25, 250),
	("FISHING", "Wedkowanie", "Ilu botow w ogole lowi. Rozstrzygane raz na "
		"bota, wiec zmiana obejmuje kolejne pokolenie wedkarzy.",
		"weight", 25, 250),
	("TRADE", "Stragany", "Ilu botow trzyma otwarty stragan. Handlarze "
		"robia to zawsze, niezaleznie od tego suwaka.",
		"weight", 25, 250),
]

# Kolejnosc, w ktorej do_gmpanel_getaiweights (cmd_gm.cpp) faktycznie
# wysyla wartosci - STALA, niezalezna od kolejnosci wyswietlania powyzej
# (ktora ma byc jak na stronie /ai, nie jak w pliku tsv).
#
# Musi sie zgadzac z PLAYERBOT_PANEL_WEIGHT_ORDER w playerbot_config.h.
# Serwer dopisuje nowe klucze na koncu - REST doszedl w aktualizacji i
# przez brak go tutaj cala karta "Zachowanie botow" przestala sie
# wypelniac (SetAIWeightsResult odrzucal odpowiedz o zlej dlugosci).
GM_PANEL_AI_WEIGHT_SERVER_ORDER = [
	"RESTOCK", "REFINE", "SKILL", "HORSE", "BIOLOG", "METIN", "PARTY",
	"HUNTING", "LEVEL", "FISHING", "TRADE",
	"CHAT", "BOOKS", "NIGHT", "SCRAP", "CHEST", "CHEST_STONE", "REST",
]

# Reference-only (client never sends this to the server) - the "Lista
# komend GM" button on Spawn Mobow just renders this scrollable list as-is,
# copied from the community command list the user provided. cmd_info[] in
# cmd.cpp has no description field to enumerate at runtime, so a curated
# static list is the only way to show what each command actually does.
GM_COMMANDS_LIST = [
	"/item - dodaje przedmiot",
	"/item - dodaje kilka sztuk przedmiotu",
	"/m - przywoluje potwora lub npc",
	"/p - zamiana w potwora lub npc",
	"/set align 200000 - ranga rycerski",
	"/weak - najpierw klikamy na potwora ppm, nastepnie wpisujemy komende i potwor ma 1hp",
	"/ski 121 - Dowodzenie",
	"/ski 131 - Przywolaj Konia",
	"/ski 124 - MPCictwo",
	"/ski 125 - Kowalstwo",
	"/xmas 0 - Dzien",
	"/xmas 1 - Noc",
	"/inv - niewidzialnosc",
	"/dc - wyrzuca kogos z serwera",
	"/warp - przenosi GM'a do gracza",
	"/transfer - przenosi gracza do GM'a",
	"/go x y - przenosi do wspolrzednych x y",
	"/go s - teleport na Gore Sohan",
	"/go t - teleport do Doliny Orkow",
	"/go m - teleport do Swiatynii Hwang",
	"/go f - teleport do Piekla",
	"/go tr - teleport do Lasu Duchow",
	"/go trent2 - teleport do Czerwonego Lasu",
	"/xmas_snow 0 - wylacz snieg",
	"/xmas_snow 1 - wlacz snieg",
	"/set max_hp - zwieksza punkty zycia",
	"/set max_sp - zwieksza punkty energii",
	"/n - Wiadomosc na gorze ekranu",
	"/block_chat - blokuje chat na jakis czas",
	"/block_chat_list - lista graczy z zablokowanym pisaniem",
	"/kill - zabija gracza",
	"/stun - omdlewa gracza",
	"/slow - spowalnia gracza",
	"/horse_level - podnosi poziom konia",
	"/reset - resetuje PZ i PE",
	"/go d - teleport na pustynie",
	"/go A - Shinsoo M1",
	"/go A3 - Shinsoo M2",
	"/go B - Chunjo M1",
	"/go B3 - Chunjo M2",
	"/go C - Jinno M1",
	"/go C3 - Jinno M2",
	"/purge - usuwa potwory, npc itp z okolicy",
	"/level - zmiana lvl'u postaci",
	"/go o - teleport do Areny OX",
	"/warp 400 600 - teleport do Areny GM'ow",
	"/warp 200 100 - Wyspa ze sniegiem (Siedliszcza GM'ow)",
	"/warp 7032 5225 - teleport do V2",
	"/resp all - jezeli usuniemy komenda /purge jakiegos NPC",
	"/ - powtarza poprzednia komende",
	"/gwlist - pokazuje wojny gildii",
	"/gwcancel - konczy wojne miedzy danymi gildiami",
	"/horse_ride - przywoluje konia",
	"/setsk 121 59 - Dowodzenie P",
	"/setsk 122 59 - Combo P",
	"/setsk 124 59 - MPCictwo P",
	"/setsk 125 59 - Kowalstwo P",
	"/setsk 126 59 - Jezyk Shinsoo P",
	"/setsk 127 59 - Jezyk Chunjo P",
	"/setsk 128 59 - Jezyk Jinno P",
	"/setsk 129 59 - Polimorfia P",
	"/setsk 130 30 - 30 lvl konia",
	"/setsk 131 10 - Przywolanie konia 100%",
	"/b - ogloszenie w ramce",
	"/x - dzien",
	"/h - informacja o koniu",
	"/o - Tryb obserwowania",
	"/f - ...nie wiem...",
	"/u - ilosc osob online",
	"/w - ilosc osob online",
	"/setsk 137 59 - Ciecie z Siodla P",
	"/setsk 138 59 - Stapniecie Konia P",
	"/setsk 139 59 - Fala Mocy P",
	"/setsk 140 59 - 4 skill militara",
	"/poly 0-3 - twoja postac",
	"/poly 4 - Wojownik",
	"/poly 5 - Ninja",
	"/poly 6 - Sura Kobieta",
	"/poly 7 - Szaman",
	"/priv_empire 0 1:item_drop - Zwieksza drop itemkow o dany procent na okreslony czas",
	"/priv_empire 0 2:gold_drop - Zwieksza drop Yang o dany procent na okreslony czas",
	"/priv_empire 0 3:gold10_drop - Zwieksza drop Yang o dany procent na okreslony czas i mnozy razy 10",
	"/priv_empire 0:4exp - Zwieksza exp o dany procent na okreslony czas",
	"-- Sura BM - skille P --",
	"/setsk 76 59",
	"/setsk 77 59",
	"/setsk 78 59",
	"/setsk 79 59",
	"/setsk 80 59",
	"/setsk 81 59",
	"-- Sura WP - skille P --",
	"/setsk 61 59",
	"/setsk 62 59",
	"/setsk 63 59",
	"/setsk 64 59",
	"/setsk 65 59",
	"/setsk 66 59",
	"-- Wojownik Body - skille P --",
	"/setsk 1 59",
	"/setsk 2 59",
	"/setsk 3 59",
	"/setsk 4 59",
	"/setsk 5 59",
	"-- Wojownik Mental - skille P --",
	"/setsk 16 59",
	"/setsk 17 59",
	"/setsk 18 59",
	"/setsk 19 59",
	"/setsk 20 59",
	"-- Szaman Smok - skille P --",
	"/setsk 91 59",
	"/setsk 92 59",
	"/setsk 93 59",
	"/setsk 94 59",
	"/setsk 95 59",
	"/setsk 96 59",
	"-- Szaman Healer - skille P --",
	"/setsk 106 59",
	"/setsk 107 59",
	"/setsk 108 59",
	"/setsk 109 59",
	"/setsk 110 59",
	"/setsk 111 59",
	"-- Ninja Dagger - skille P --",
	"/setsk 31 59",
	"/setsk 32 59",
	"/setsk 33 59",
	"/setsk 34 59",
	"/setsk 35 59",
	"-- Ninja Archer - skille P --",
	"/setsk 46 59",
	"/setsk 47 59",
	"/setsk 48 59",
	"/setsk 49 59",
	"/setsk 50 59",
	"/set exp - dostajemy dana ilosc expa",
	"/set gold - dostajemy dana ilosc Yang",
	"/pkmode 0 - Tryb PVP Pokojowy",
	"/pkmode 1 - Tryb PVP Agresywny",
	"/pkmode 2 - Tryb PVP Wolny",
	"/set skill - dodaje dana ilosc pkt umiejetnosci",
	"/setskillother 59 - dajemy komus dany skill na P",
	"/r - przywraca wszystkie pkt energii i zycia",
	"/a - dajemy komus dany lvl",
	"/xmas_song 1 - Wlacza sie piosenka swiateczna",
	"/xmas_song 0 - Wylacza sie piosenka",
	"/xmas_boom 1 - Noc",
	"/xmas_boom 0 - Dzien",
	"/xmas_tree 0 - swieta OFF",
	"/xmas_tree 1 - Pierwszy etap swiat.",
	"/xmas_tree 2 - Drugi etap swiat.",
	"/xmas_tree 3 - Trzeci etap swiat.",
	"/xmas_santa 1 - Santa Claus wlaczony",
	"/xmas_santa 0 - Santa Claus wylaczony",
	"/open - start eventu OX",
	"/mspd 0 - 1000 - szybkosc chodzenia",
	"/set_maxhp - doladowanie hp",
	"/set_maxsp - doladowanie pe",
	"/a level - Zmienia lvl gracza",
	"/polyitem - dodaje marmur polimorfii z danym potworem",
	"/setsk 151 7 - Krew Boga Smokow",
	"/setsk 152 7 - Blogoslawienstwo Boga Smokow",
	"/setsk 153 7 - Swieta Zbroja",
	"/setsk 154 7 - Akceleracja",
	"/setsk 155 7 - Furia Boga Smokow",
	"/setsk 156 7 - Smocze Zyczenie",
	"/setsk 157 7 - ...nie wiem...",
	"/set_state run - ...nie wiem...",
	"/set_state start - ...nie wiem...",
	"/set_state information - ...nie wiem...",
	"/e lotto_round - ...nie wiem...",
	"/e lotto-drop - ...nie wiem...",
	"/gete - ...nie wiem...",
	"/getq - ...nie wiem...",
	"/reload q - ...nie wiem...",
]

WINDOW_WIDTH = 680
WINDOW_HEIGHT = 560
TAB_Y = 44
TAB_HEIGHT = 22
# The 8 tabs add up to more than WINDOW_WIDTH once "Sterowanie Serwerem"
# joined them (reported live: its own label ran past the panel's right
# edge). A horizontal scroll slider was tried here and worked, but was
# simpler to just give "Sterowanie Serwerem" its own second row, left-
# aligned under "Glowny Panel" - see the tab-building loop below.
TAB_Y2 = TAB_Y + TAB_HEIGHT + 2
CONTENT_Y = TAB_Y2 + TAB_HEIGHT + 8
CONTENT_HEIGHT = WINDOW_HEIGHT - CONTENT_Y - 10

# "Kategoria" dropdown on Utworz Przedmiot - codes match what
# do_gmpanel_itemlist (cmd_gm.cpp) understands.
GM_PANEL_CATEGORY_LIST = [
	("weapon",		"Bron"),
	("armor_body",	"Zbroja"),
	("armor_ear",	"Kolczyki"),
	("armor_wrist",	"Bransolety"),
	("armor_neck",	"Naszyjniki"),
	("armor_foots",	"Buty"),
	("armor_head",	"Helm"),
	("armor_shield","Tarcza"),
	("material",	"Ulepszacze"),
	("other",		"Inne przedmioty"),
]

# "Miejsce przyznania" dropdown - codes match do_gmpanel_createitem's
# location field (cmd_gm.cpp).
GM_PANEL_LOCATION_LIST = [
	("inv",		"Ekwipunek"),
	("safe",	"Magazyn"),
	("mall",	"Itemshop"),
]

# "Teleport" grid on Spawn Botow - sends the vanilla "/warp <x> <y>" GM
# command (cmd_gm.cpp, do_warp - GM_LOW_WIZARD, every GM already has it),
# which resolves the map from the coordinate itself (SECTREE_MANAGER), so
# no separate map index has to travel with these. Coordinates are meters
# (raw game units / 100, matching what do_warp itself expects and prints
# back as "You warp to (...)").
#
# Chunjo/Shinsoo/Jinno "city" entries reuse the exact same tested spot the
# web panel's own warp menu uses (admin_panel.py, WARP_LOC) - these are
# each empire's M1 field/town. Chunjo M2 (Bokjung) is that same list's
# entry too. Shinsoo M2 (a3) and Jinno M2 (c3) have no such reference (the
# playerbot AI never goes there, only Chunjo) so they are the geometric
# centre of the map instead, read out of each map's own Setting.txt
# (BasePosition + MapSize*25600) the same way PLAYERBOT_MAP_BOUNDS is - a
# few hundred units off the real town center at worst, still solid ground.
# Siatka teleportu na stronie "Spawn Botow" ma trzy kolumny: dwie z
# gotowymi celami i trzecia z wlasnymi punktami powrotu gracza. Panel ma
# 680 px szerokosci (WINDOW_WIDTH), stad te liczby.
GM_PANEL_TELE_BTN_W = 185
GM_PANEL_WP_X = 392
GM_PANEL_WP_W = 258
GM_PANEL_WP_BTN_W = 60
GM_PANEL_WAYPOINT_SLOTS = 5

GM_PANEL_TELEPORT_LIST = [
	("Chunjo M1",	659,	1556),
	("Chunjo M2",	1455,	2400),
	("Jinno M1",	9635,	2797),
	("Jinno M2",	8704,	2560),
	("Shinsoo M1",	4743,	9548),
	("Shinsoo M2",	3584,	8704),
]
GM_PANEL_TELEPORT_EXTRA_LIST = [
	("Pustynia Yongbi",	2219,	5027),
	("Dolina Orkow",	2704,	7399),
	("Gora Sohan",		3752,	1749),
	("Kraina Ognia",	5978,	6222),
]

# Dungeon/instance destinations - global meters = map's own BasePosition
# (Setting.txt, raw units /100) + its Town.txt safe-spot (already local
# meters, confirmed against metin2_map_b1's Town.txt "557 555" matching
# its known spawn exactly) rather than the geometric centre used above:
# a dungeon's interior is mostly walls, so its actual town/entry marker
# is the only coordinate guaranteed to be open ground.
GM_PANEL_TELEPORT_DUNGEON_LIST = [
	("Loch Pajakow V1",	600,	4966),
	("Loch Pajakow V2",	7040,	4625),
	("Latwy loch malp",	7752,	4477),
	("Normalny loch malp",	1352,	6525),
	("Trudny loch malp",	1352,	7293),
	("Swiatynia Hwang",	5537,	1450),
	("Las Duchow",		2901,	57),
	("Czerw. Las Duchow",	11196,	700),
]

# "Bossy" na Spawn Mobow - kazdy z 3 przedzialow ma wlasna, recznie
# dobrana pule (vnumy sprawdzone w player.mob_proto): 1/5/10 losuje z
# tego zestawu, po calej puli zanim ktos powtorzy sie drugi raz
# (gmpanel_spawnrandommobs, cmd_gm.cpp), wiec "10 bossow" nigdy nie
# znaczy 10 kopii jednego.
# 25-40 lvl: rodzina Bestii (Best. Zolnierz/Maniak/Specjalista/Kapitan,
# Bestialski Lucznik).
GM_PANEL_BOSS_TIER_1 = [531, 532, 533, 534, 591]
# 45-60: Krolowa Pajakow, Wodz Orkow, Olbrzymi Zolw (najblizszy odpowiednik
# "Pustynny zolw" w danych gry - takiej nazwy dosl. nie ma).
GM_PANEL_BOSS_TIER_2 = [2091, 691, 2191]
# 65-90: Ezot. Przywolywacz, Zjawa Zoltego Tygrysa, Krol Demonow,
# Umarly Rozpruwacz.
GM_PANEL_BOSS_TIER_3 = [791, 1304, 1091, 1093]

GM_PANEL_BOSS_TIERS = [
	("25-40 lvl", GM_PANEL_BOSS_TIER_1),
	("45-60",     GM_PANEL_BOSS_TIER_2),
	("65-90",     GM_PANEL_BOSS_TIER_3),
]

# "Metiny" na Spawn Mobow - w odroznieniu od Bossy powyzej to nie jest
# zaszyta lista: gmpanel_spawnrandommetin (cmd_gm.cpp) odpytuje
# player.mob_proto o kamienie Metin (type=STONE) w podanym przedziale
# poziomow i losuje z calego wyniku, wiec tu wystarcza same widelki.
GM_PANEL_METIN_TIERS = [
	("1-20",  5,  20),
	("25-40", 25, 40),
	("45-65", 45, 65),
	("70-90", 70, 90),
]

# "Marmur Poli" picker on Utworz Przedmiot - the curated creature list from
# https://pl-wiki.metin2.gameforge.com/index.php/Polimorfia (only mobs the
# game actually has a valid player-sized polymorph model/motion set for -
# NOT every mob_proto entry works). vnums cross-checked one by one against
# player.mob_proto.locale_name on the live DB, not guessed from the wiki's
# own (unnumbered) table.
GM_PANEL_POLY_MOB_LIST = [
	("101", "Dziki Pies"), ("171", "Glodny Zablakany Pies"),
	("102", "Wilk"), ("172", "Glodny Wilk"),
	("103", "Alfa Wilk"), ("173", "Glodny Alfa Wilk"),
	("104", "Niebieski Wilk"), ("174", "Glodny Niebieski Wilk"),
	("105", "Niebieski Alfa Wilk"), ("175", "Glodny Niebieski Alfa Wilk"),
	("106", "Szary Wilk"), ("176", "Glodny Szary Wilk"),
	("108", "Dzik"), ("178", "Glodny Dzik"),
	("110", "Niedzwiedz"), ("180", "Glodny Niedzwiedz"),
	("112", "Czarny Niedzwiedz"), ("182", "Glodny Czarny Niedzwiedz"),
	("113", "Brazowy Niedzwiedz"),
	("114", "Tygrys"), ("184", "Glodny Tygrys"),
	("5101", "Slaby Malpi Zolnierz"), ("5102", "Slaby Malpi Miotacz"),
	("5111", "Malpi Zolnierz"),
	("5121", "Silny Malpi Zolnierz"), ("5151", "Zly Silny Malpi Zolnierz"),
	("701", "Ezoteryczny Fanatyk"), ("751", "Wysoki Fanatyk"),
	("731", "Elitarny Ezoteryczny Fanatyk"),
	("771", "Bestialski Fanatyk"), ("772", "Bestialski Arahan"),
	("401", "Zolnierz Czarnego Wiatru"), ("402", "Maniak Czarnego Wiatru"),
	("451", "Zly Zolnierz Czarnej Burzy"),
	("501", "Dziki Zolnierz Piechoty"), ("551", "Silny Dziki Piechur"),
	("502", "Dziki Sluga"), ("552", "Silny Dziki Sluga"),
	("602", "Ork Zwiadowca"), ("631", "Elitarny Ork"), ("651", "Duzy Lysy Ork"),
	("2001", "Mlody Pajak"), ("2002", "Trujacy Pajak"), ("2061", "Maly Trujacy Pajak"),
	("2051", "Podly Mlody Trujacy Pajak"), ("2052", "Podly Smiertelny Trujacy Pajak"),
	("2131", "Bestialski Czlowiek Skorpion"),
	("1105", "Mrozny Lodowy Czlowiek"), ("1107", "Lodowy Golem"),
	("1136", "Podziemne Yeti"),
	("1402", "Wojownik z Toporem"), ("1403", "Tysieczny Wojownik"), ("1601", "Ogr Wojownik"),
	("2302", "Duch Pniaka"),
]

# Bonus type dropdown source: (APPLY_* numeric id, enum suffix) pairs
# matching EApplyTypes in length.h exactly. Display labels are pulled at
# runtime from localeInfo.TOOLTIP_APPLY_<suffix> (locale_game.txt) instead
# of being hand-translated here, so they always match what item tooltips
# already show.
GM_PANEL_APPLY_SUFFIXES = [
	(1, "MAX_HP"), (2, "MAX_SP"), (3, "CON"), (4, "INT"), (5, "STR"), (6, "DEX"),
	(7, "ATT_SPEED"), (8, "MOV_SPEED"), (9, "CAST_SPEED"), (10, "HP_REGEN"),
	(11, "SP_REGEN"), (12, "POISON_PCT"), (13, "STUN_PCT"), (14, "SLOW_PCT"),
	(15, "CRITICAL_PCT"), (16, "PENETRATE_PCT"), (17, "ATTBONUS_HUMAN"),
	(18, "ATTBONUS_ANIMAL"), (19, "ATTBONUS_ORC"), (20, "ATTBONUS_MILGYO"),
	(21, "ATTBONUS_UNDEAD"), (22, "ATTBONUS_DEVIL"), (23, "STEAL_HP"),
	(24, "STEAL_SP"), (25, "MANA_BURN_PCT"), (26, "DAMAGE_SP_RECOVER"),
	(27, "BLOCK"), (28, "DODGE"), (29, "RESIST_SWORD"), (30, "RESIST_TWOHAND"),
	(31, "RESIST_DAGGER"), (32, "RESIST_BELL"), (33, "RESIST_FAN"),
	(34, "RESIST_BOW"), (35, "RESIST_FIRE"), (36, "RESIST_ELEC"),
	(37, "RESIST_MAGIC"), (38, "RESIST_WIND"), (39, "REFLECT_MELEE"),
	(40, "REFLECT_CURSE"), (41, "POISON_REDUCE"), (42, "KILL_SP_RECOVER"),
	(43, "EXP_DOUBLE_BONUS"), (44, "GOLD_DOUBLE_BONUS"), (45, "ITEM_DROP_BONUS"),
	(46, "POTION_BONUS"), (47, "KILL_HP_RECOVER"), (48, "IMMUNE_STUN"),
	(49, "IMMUNE_SLOW"), (50, "IMMUNE_FALL"), (51, "SKILL"), (52, "BOW_DISTANCE"),
	(53, "ATT_GRADE_BONUS"), (54, "DEF_GRADE_BONUS"), (55, "MAGIC_ATT_GRADE"),
	(56, "MAGIC_DEF_GRADE"), (57, "CURSE_PCT"), (58, "MAX_STAMINA"),
	(59, "ATTBONUS_WARRIOR"), (60, "ATTBONUS_ASSASSIN"), (61, "ATTBONUS_SURA"),
	(62, "ATTBONUS_SHAMAN"), (63, "ATTBONUS_MONSTER"), (64, "MALL_ATTBONUS"),
	(65, "MALL_DEFBONUS"), (66, "MALL_EXPBONUS"), (67, "MALL_ITEMBONUS"),
	(68, "MALL_GOLDBONUS"), (69, "MAX_HP_PCT"), (70, "MAX_SP_PCT"),
	(71, "SKILL_DAMAGE_BONUS"), (72, "NORMAL_HIT_DAMAGE_BONUS"),
	(73, "SKILL_DEFEND_BONUS"), (74, "NORMAL_HIT_DEFEND_BONUS"),
	(75, "PC_BANG_EXP_BONUS"), (76, "PC_BANG_DROP_BONUS"), (77, "EXTRACT_HP_PCT"),
	(78, "RESIST_WARRIOR"), (79, "RESIST_ASSASSIN"), (80, "RESIST_SURA"),
	(81, "RESIST_SHAMAN"), (82, "ENERGY"), (83, "DEF_GRADE"),
	(84, "COSTUME_ATTR_BONUS"), (85, "MAGIC_ATTBONUS_PER"),
	(86, "MELEE_MAGIC_ATTBONUS_PER"), (87, "RESIST_ICE"), (88, "RESIST_EARTH"),
	(89, "RESIST_DARK"), (90, "ANTI_CRITICAL_PCT"), (91, "ANTI_PENETRATE_PCT"),
]

# Plain ui.ComboBox with no way to hook "I'm opening now" - subclassed just
# to close every other combo/search-combo on the panel first, otherwise
# each one only knows how to close itself and clicking a different field
# leaves the previous dropdown stuck open on top of everything (this is
# what "tabele na siebie nachodza" turned out to be: Kategoria, Miejsce
# przyznania and a search-combo's results all left open at once).
class GMComboBox(ui.ComboBox):
	def __init__(self):
		ui.ComboBox.__init__(self)
		self.closeOthers = None

	def OnMouseLeftButtonUp(self):
		if not self.isListOpened and self.closeOthers:
			self.closeOthers(self)
		ui.ComboBox.OnMouseLeftButtonUp(self)

# Type-to-filter combo for lists too long for a plain ui.ComboBox (which has
# no scrolling - ComboBox.ArrangeItem sizes the popup to fit every item, so
# a 200+ entry weapon list rendered as one giant unclipped column covering
# the rest of the window). Always shows at most MAX_RESULTS matches, so the
# popup never needs to scroll. Typing filters live via OnUpdate polling -
# EditLine has no per-keystroke "changed" event in this engine, but OnUpdate
# is called every frame on any visible window (confirmed by ui.ComboBox's
# own use of it for hover state), so polling GetText() each frame is the
# supported way to detect this.
class GMSearchCombo(ui.Window):
	MAX_RESULTS = 10

	def __init__(self):
		ui.Window.__init__(self)
		self.items = []
		self.selectedValue = "0"
		self.onChange = None
		self.closeOthers = None
		self.openUpward = False
		self.lastText = None
		self.lastFocused = False
		self.edit = None
		self.slot = None
		self.listBox = None
		self.owner = None
		self.panelX = 0
		self.panelY = 0
		self._matches = []

	def __del__(self):
		ui.Window.__del__(self)

	# owner: the top-level GMPanelWindow. Its own popup gets parented
	# directly to it (not to this combo, which sits inside a page ui.Window
	# sized to CONTENT_HEIGHT) - a page clips its children to its own rect,
	# which is exactly why the popup was disappearing under the panel's
	# own board texture whenever it needed to extend past the page's
	# bounds. panelX/panelY is this combo's position in the PANEL's own
	# coordinate space (all pages sit at a fixed (10, CONTENT_Y) offset),
	# used to place the reparented popup directly under/above this combo.
	#
	# Split from __init__ like GMPanelWindow's pages: a child Show()n while
	# its parent is still hidden can end up permanently invisible even
	# after the parent is shown later in this engine, so the caller shows
	# this widget itself before calling Create().
	def Create(self, owner, panelX, panelY):
		self.owner = owner
		self.panelX = panelX
		self.panelY = panelY

		width = self.GetWidth()
		height = self.GetHeight()

		self.slot = ui.SlotBar()
		self.slot.SetParent(self)
		self.slot.SetSize(width, height)
		self.slot.SetPosition(0, 0)
		self.slot.AddFlag("not_pick")	# else it swallows clicks meant for self.edit below it
		self.slot.Show()

		self.edit = ui.EditLine()
		self.edit.SetParent(self)
		self.edit.SetPosition(3, 3)
		self.edit.SetSize(width - 6, height - 4)
		self.edit.SetMax(40)
		self.edit.Show()

		self.listBox = ui.ListBox()
		self.listBox.SetParent(owner)
		self.listBox.SetWidth(width)
		self.listBox.SetPickAlways()
		self.listBox.SetEvent(self.__OnSelectItem)
		self.listBox.Hide()

	def SetItems(self, items):
		self.items = items
		if items:
			self.__Select(items[0][0], items[0][1])
		else:
			self.__Select("0", "(brak)")

	def Close(self):
		if self.listBox:
			self.listBox.Hide()

	def __Select(self, value, label):
		self.selectedValue = value
		self.edit.SetText(label)
		self.lastText = label
		self.Close()

	def __OnSelectItem(self, index, name):
		if 0 <= index < len(self._matches):
			value, label = self._matches[index]
			self.__Select(value, label)
			if self.onChange:
				self.onChange(value)

	def OnUpdate(self):
		if not self.edit:
			return
		text = self.edit.GetText()
		focused = self.edit.IsFocus()
		if text == self.lastText and focused == self.lastFocused:
			return
		self.lastText = text
		self.lastFocused = focused
		if focused:
			self.__Refresh(text)
		else:
			self.Close()

	def __Refresh(self, text):
		query = text.strip().lower()
		self._matches = []
		# No query yet (box just clicked/focused) - show the first
		# MAX_RESULTS items as a starting point, same as a normal dropdown.
		for value, label in self.items:
			if not query or query in label.lower():
				self._matches.append((value, label))
				if len(self._matches) >= self.MAX_RESULTS:
					break

		self.listBox.ClearItem()
		if not self._matches:
			self.listBox.Hide()
			return

		if self.closeOthers:
			self.closeOthers(self)

		for i, (value, label) in enumerate(self._matches):
			self.listBox.InsertItem(i, label)
		self.listBox.ArrangeItem()
		popupHeight = self.listBox.GetHeight()
		if self.openUpward:
			self.listBox.SetPosition(self.panelX, self.panelY - popupHeight - 2)
		else:
			self.listBox.SetPosition(self.panelX, self.panelY + self.GetHeight() + 2)
		self.SetTop()
		self.listBox.Show()
		self.listBox.SetTop()

# Built entirely from ui.py widget classes in Python (no uiscript file to
# execfile()/pack) - see the comment above GM_PANEL_LOOKUP_FIELD_ORDER for why.
class GMPanelWindow(ui.BoardWithTitleBar):
	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.pages = {}
		self.tabs = {}
		self.lookupValues = {}
		# Widgets built by pure Python code (no uiscript "children" tree)
		# have no other Python reference once the local variable that
		# created them goes out of scope - without this, refcounting GC
		# destroys the native widget (via __del__) right after Show(),
		# which is why the panel rendered its tabs but no page content.
		self._widgets = []
		# Every GMComboBox/GMSearchCombo on the panel, so opening one can
		# close all the others (see GMComboBox above).
		self._allCombos = []
		# Shared by every "..." picker field across every tab (Utworz
		# Przedmiot, Blokada Konta, Dodaj GM, ...) - this must exist before
		# ANY of those pages build their fields. It used to be initialized
		# inside __BuildCreateItemPage itself, which happened to work only
		# because that page was built before the others that also use
		# __MakePickerField - adding Dodaj GM ahead of it in the page-build
		# order (page_addgm comes before page_createitem) turned that
		# ordering assumption into a hard crash for every single login,
		# GM or not, since wndGMPanel is built unconditionally for everyone.
		self.pickerFields = {}

		# do_gmpanel_itemlist (cmd_gm.cpp) answers over several chat packets
		# (CHAT_MAX_LEN=512 caps one message) - these serialize requests so
		# two in-flight fetches never interleave their chunks.
		self._itemListQueue = []
		self._itemListBusy = False
		self._itemListTarget = None
		self._itemListBuffer = ""
		# If a response never arrives (dropped packet, server-side error
		# with no reply, etc.) _itemListBusy would otherwise stay stuck
		# forever and silently swallow every later fetch put behind it in
		# the queue - which is exactly what "czasem sie nie pobiera" turned
		# out to be. OnUpdate below counts frames spent busy and force-
		# advances the queue past whatever never answered.
		self._itemListBusyFrames = 0

		self.SetSize(WINDOW_WIDTH, WINDOW_HEIGHT)
		self.SetPosition(200, 100)
		self.AddFlag("movable")
		self.AddFlag("float")
		self.SetTitleName("Panel GM by OskarPWA")
		self.SetCloseEvent(self.Hide)

		# "Sterowanie Serwerem" gets its own second row, left-aligned under
		# "Glowny Panel" (x=0), instead of joining the first row's x-walk -
		# that row alone doesn't fit all 8 tabs within WINDOW_WIDTH.
		x = 0
		for tabName in GM_PANEL_TAB_NAMES:
			if tabName == "tab_serverctrl":
				width = 160
			elif tabName in ("tab_spawnbots", "tab_spawnmobs"):
				width = 110
			else:
				width = 82
			button = ui.Button()
			button.SetParent(self)
			if tabName == "tab_serverctrl":
				button.SetPosition(0, TAB_Y2)
			else:
				button.SetPosition(x, TAB_Y)
				x += width + 2
			button.SetSize(width, TAB_HEIGHT)
			button.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
			button.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
			button.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
			button.SetText(GM_PANEL_TAB_LABELS[tabName])
			button.SetEvent(self.__MakeSetPageHandler(tabName[len("tab_"):]))
			button.Show()
			self.tabs[tabName] = button

		for pageName in GM_PANEL_PAGE_NAMES:
			page = ui.Window()
			page.SetParent(self)
			page.SetPosition(10, CONTENT_Y)
			page.SetSize(WINDOW_WIDTH - 20, CONTENT_HEIGHT)
			# Show before adding children - children added while a parent is
			# still hidden can end up permanently invisible even after the
			# parent is shown later, in this engine.
			page.Show()
			self.pages[pageName] = page

		self.__BuildMainPage(self.pages["page_main"])
		self.__BuildAddGMPage(self.pages["page_addgm"])
		self.__BuildLookupPage(self.pages["page_lookup"])
		self.__BuildCreateItemPage(self.pages["page_createitem"])
		self.__BuildBanPage(self.pages["page_ban"])
		self.__BuildSpawnBotsPage(self.pages["page_spawnbots"])
		self.__BuildSpawnMobsPage(self.pages["page_spawnmobs"])
		self.__BuildServerControlPage(self.pages["page_serverctrl"])
		self.__BuildGiveAmountPage(self.pages["page_giveamount"])
		self.__BuildSetValuePage(self.pages["page_setvalue"])
		self.__BuildSetStatPage(self.pages["page_setstat"])
		self.__BuildSkillsPage(self.pages["page_skills"])
		self.__BuildGMCommandsPage(self.pages["page_gmcommands"])

		# Full-page item/stone/bonus picker, not one of the 6 tabs - opened
		# on top of page_createitem by a field's "..." button and closed
		# back to it. Floating dropdown popups (ui.ComboBox and the earlier
		# GMSearchCombo attempt) kept rendering underneath this panel's own
		# board texture no matter what layer/z-order trick was tried, since
		# this panel is a "float" window; showing/hiding a normal sibling
		# page instead reuses the one part of this UI already proven to
		# render correctly (the tab pages themselves).
		pickerPage = ui.Window()
		pickerPage.SetParent(self)
		pickerPage.SetPosition(10, CONTENT_Y)
		pickerPage.SetSize(WINDOW_WIDTH - 20, CONTENT_HEIGHT)
		pickerPage.Show()
		self.pages["page_picker"] = pickerPage
		self.__BuildPickerPage(pickerPage)
		pickerPage.Hide()

		self.__SetPage("page_main")

	def __del__(self):
		ui.BoardWithTitleBar.__del__(self)

	# Polls the picker page's search box the same way GMSearchCombo did -
	# EditLine has no per-keystroke event in this engine, but OnUpdate is
	# called every frame on any visible window.
	def OnUpdate(self):
		if self._itemListBusy:
			self._itemListBusyFrames += 1
			if self._itemListBusyFrames > 180:
				self._itemListBusy = False
				self._itemListTarget = None
				self._itemListBuffer = ""
				self._itemListBusyFrames = 0
				self.__ProcessItemListQueue()
		else:
			self._itemListBusyFrames = 0

		if self._ratesFetchFrames > 0:
			self._ratesFetchFrames += 1
			if self._ratesFetchFrames > 180:
				self._ratesFetchFrames = 0
				for status in self._serverctrlStatus.values():
					status.SetText("Brak odpowiedzi serwera, sprobuj ponownie.")

		if self._aiWeightsFetchFrames > 0:
			self._aiWeightsFetchFrames += 1
			if self._aiWeightsFetchFrames > 180:
				self._aiWeightsFetchFrames = 0
				self.aiWeightsStatus.SetText("Brak odpowiedzi serwera - otworz karte ponownie.")

		picker = self.pages.get("page_picker")
		if not picker or not picker.IsShow():
			return
		text = self.pickerSearchEdit.GetText()
		if text != self._pickerLastQuery:
			self._pickerLastQuery = text
			self._pickerPageOffset = 0
			self.__RefreshPickerRows(text)

	def __MakeText(self, parent, x, y, text=""):
		textLine = ui.TextLine()
		textLine.SetParent(parent)
		textLine.SetPosition(x, y)
		textLine.SetText(text)
		textLine.Show()
		self._widgets.append(textLine)
		return textLine

	def __MakeEdit(self, parent, x, y, width, maxlen=8):
		slot = ui.SlotBar()
		slot.SetParent(parent)
		slot.SetSize(width, 20)
		slot.SetPosition(x, y)
		slot.Show()
		self._widgets.append(slot)

		editLine = ui.EditLine()
		editLine.SetParent(slot)
		editLine.SetPosition(3, 3)
		editLine.SetSize(width - 6, 17)
		editLine.SetMax(maxlen)
		editLine.Show()
		self._widgets.append(editLine)
		return editLine

	def __MakeCombo(self, parent, x, y, width, height=20):
		combo = GMComboBox()
		combo.SetParent(parent)
		combo.SetPosition(x, y)
		combo.SetSize(width, height)
		combo.Show()
		combo.closeOthers = self.__CloseOtherCombos
		self._widgets.append(combo)
		self._allCombos.append(combo)
		return combo

	# For lists too long for a plain ComboBox (see GMSearchCombo above) -
	# vnum/stone/bonus-type pickers all use this instead. y is this widget's
	# position within its page, used to decide whether its results popup
	# needs to open upward to stay inside the page (see GMSearchCombo).
	def __MakeSearchCombo(self, parent, x, y, width, items=None, onChange=None):
		combo = GMSearchCombo()
		combo.SetParent(parent)
		combo.SetPosition(x, y)
		combo.SetSize(width, 20)
		combo.Show()
		# All pages sit at a fixed (10, CONTENT_Y) inside this panel - see
		# GMSearchCombo.Create for why the popup needs panel-relative coords.
		combo.Create(self, 10 + x, CONTENT_Y + y)
		combo.onChange = onChange
		combo.closeOthers = self.__CloseOtherCombos
		combo.openUpward = y > CONTENT_HEIGHT - 210
		combo.SetItems(items or [])
		self._widgets.append(combo)
		self._allCombos.append(combo)
		return combo

	def __CloseOtherCombos(self, keep):
		for combo in self._allCombos:
			if combo is keep:
				continue
			if hasattr(combo, "CloseListBox"):
				combo.CloseListBox()
			else:
				combo.Close()

	# items: list of (value, label). Stored on the combo itself (_gmItems)
	# so the SetEvent callback - which only ever receives an index - can
	# resolve back to both the value and the label to display.
	def __FillCombo(self, combo, items):
		combo.ClearItem()
		combo._gmItems = items
		for i, (value, label) in enumerate(items):
			combo.InsertItem(i, label)
		if items:
			combo.SetCurrentItem(items[0][1])
			combo._gmSelected = items[0][0]
		else:
			combo.SetCurrentItem("(brak)")
			combo._gmSelected = "0"

	def __MakeComboSelectHandler(self, combo, onChange=None):
		def handler(index):
			items = getattr(combo, "_gmItems", [])
			if 0 <= index < len(items):
				value, label = items[index]
				combo._gmSelected = value
				combo.SetCurrentItem(label)
				if onChange:
					onChange(value)
		return handler

	def __ApplyLabel(self, suffix):
		# Basic stats (STR/DEX/CON/INT/MAX_HP/MAX_SP/speeds/regen/SKILL) are
		# localized as plain TOOLTIP_<name> in this client, not
		# TOOLTIP_APPLY_<name> - only the less common apply types use the
		# _APPLY_ form. Without this most bonus types fell back to their
		# raw English enum name instead of a Polish label.
		for prefix in ("TOOLTIP_APPLY_", "TOOLTIP_"):
			getter = getattr(localeInfo, prefix + suffix, None)
			if getter is None:
				continue
			try:
				return getter(0)
			except:
				pass
		return suffix

	# do_gmpanel_itemlist (cmd_gm.cpp) answers in several CHAT_MAX_LEN-capped
	# chat packets ("GMPanelItemListChunk <isLast>|<data>") - queued so two
	# fetches (e.g. category change + stone list) never interleave.
	def __FetchItemList(self, category, target):
		if target == "botlist":
			command = "gmpanel_botlist"
		elif target == "availbots":
			command = "gmpanel_available_bots"
		elif target == "moblist":
			command = "gmpanel_moblist"
		elif target == "metinlist":
			command = "gmpanel_metinlist"
		else:
			command = "gmpanel_itemlist"
		self._itemListQueue.append((command, category, target))
		self.__ProcessItemListQueue()

	def __ProcessItemListQueue(self):
		if self._itemListBusy or not self._itemListQueue:
			return
		command, category, target = self._itemListQueue.pop(0)
		self._itemListBusy = True
		self._itemListTarget = target
		self._itemListBuffer = ""
		net.SendChatPacket("/%s %s" % (command, category))

	# Called from game.py's server-command dispatcher with each
	# "GMPanelItemListChunk <isLast>|<data>" payload.
	def SetItemListChunk(self, data):
		parts = data.split("|", 1)
		if len(parts) != 2:
			return
		isLast, chunk = parts
		self._itemListBuffer += chunk

		if isLast != "1":
			return

		raw = self._itemListBuffer
		self._itemListBuffer = ""
		target = self._itemListTarget
		self._itemListTarget = None
		self._itemListBusy = False

		items = []
		for entry in raw.split(";"):
			if not entry:
				continue
			bits = entry.split(":", 1)
			if len(bits) != 2:
				continue
			vnum, name = bits
			items.append((vnum, name.replace("_", " ")))

		if target == "botlist":
			# Already ordered by the server (level descending) - keep it,
			# and don't alphabetize the way item lists are below.
			self._botListItems = items
			if self._pickerKey == "__botlist__" and self.pages["page_picker"].IsShow():
				self.__RefreshPickerRows(self.pickerSearchEdit.GetText())
			self.__ProcessItemListQueue()
			return

		if target == "availbots":
			# Already ordered by the server (PID ascending) - keep it.
			self._availBotsItems = items
			if self._pickerKey == "__availbots__" and self.pages["page_picker"].IsShow():
				self.__RefreshPickerRows(self.pickerSearchEdit.GetText())
			self.__ProcessItemListQueue()
			return

		if target == "moblist":
			# Already ordered by the server (vnum ascending, ORDER BY vnum
			# in do_gmpanel_moblist) - keep it, don't alphabetize like the
			# vnum/stone item lists below (user asked for smallest-to-
			# largest ID order, not alphabetical).
			self._mobListItems = items
			if self._pickerKey == "mob_picker" and self.pages["page_picker"].IsShow():
				self.__RefreshPickerRows(self.pickerSearchEdit.GetText())
			self.__ProcessItemListQueue()
			return

		if target == "metinlist":
			self._metinListItems = items
			if self._pickerKey == "metin_picker" and self.pages["page_picker"].IsShow():
				self.__RefreshPickerRows(self.pickerSearchEdit.GetText())
			self.__ProcessItemListQueue()
			return

		items.sort(key=lambda pair: pair[1])

		if target == "vnum":
			self._vnumItems = items
		elif target == "stone":
			self._stoneItems = [("0", "(puste)")] + items

		self.__ProcessItemListQueue()

	def __OnCategoryChanged(self, catCode):
		self.__FetchItemList(catCode, "vnum")

	def __BuildMainPage(self, page):
		self.__MakeText(page, 10, 10, "Panel Administracyjny GM")
		self.__MakeText(page, 10, 32, "Wybierz zakladke powyzej. Kazda akcja jest")
		self.__MakeText(page, 10, 46, "dodatkowo zweryfikowana po stronie serwera.")
		self.__MakeText(page, 10, 62, "Panel jest w wersji BETA. Jak zauwazysz bugi")
		self.__MakeText(page, 10, 76, "napisz na Discord do OskarPWA.")

		# Logo autora (m2sp_logo.tga) lezy w jego paczce ETC, ktorej ten pakiet
		# nie wysyla i ktorej pliku nikt tu nie ma. ImageBox wczytuje obrazek
		# dopiero przy rysowaniu, wiec zaden try/except wokol LoadImage tego nie
		# lapal - wyjatek wychodzil pozniej i zabieral cale okno panelu, kazdemu.
		# Widget zostal usuniety w calosci: nie ma obrazka, nie ma czego wczytac.

	def __BuildPlaceholderPage(self, page):
		self.__MakeText(page, 10, 10, "W przygotowaniu.")

	def __BuildLookupPage(self, page):
		self.__MakeText(page, 10, 6, "Nick:")

		slot = ui.SlotBar()
		slot.SetParent(page)
		slot.SetSize(140, 20)
		slot.SetPosition(55, 2)
		slot.Show()
		self._widgets.append(slot)

		editLine = ui.EditLine()
		editLine.SetParent(slot)
		editLine.SetPosition(3, 3)
		editLine.SetSize(134, 17)
		editLine.SetMax(24)
		editLine.Show()
		self.lookupNickEdit = editLine

		searchButton = ui.Button()
		searchButton.SetParent(page)
		searchButton.SetPosition(210, 2)
		searchButton.SetSize(80, 20)
		searchButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		searchButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		searchButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		searchButton.SetText("Sprawdz")
		searchButton.SetEvent(self.__OnClickSearch)
		searchButton.Show()
		self._widgets.append(searchButton)

		self.lookupStatus = self.__MakeText(page, 300, 6, "")

		colX = [10, 300]
		rowH = 18
		for i, (fieldName, label) in enumerate(GM_PANEL_LOOKUP_LAYOUT):
			col = i % 2
			row = i // 2
			x = colX[col]
			y = 30 + row * rowH
			self.__MakeText(page, x, y, label)
			self.lookupValues[fieldName] = self.__MakeText(page, x + 130, y, "-")

		# Skill summary (read-only - "Zmien skille" below opens the actual
		# editor, page_skills) - names/levels only for skills the character
		# actually has (GetSkillLevel(vnum) > 0), online-only (see
		# do_gmpanel_skilllist comment: no trusted way to read skill_level's
		# raw BLOB for an offline character).
		skillsY = 30 + 11 * rowH + 10
		self.__MakeText(page, 10, skillsY, "Umiejetnosci:")
		self.lookupSkillLines = []
		for i in range(8):
			line = self.__MakeText(page, 10, skillsY + 18 + i * 14, "")
			self.lookupSkillLines.append(line)

		# Akcje GM na sprawdzanej postaci - dwa rzedy po 3 przyciski, pod
		# lista umiejetnosci. Kazdy zapamietuje self._lookupNick (ustawiane
		# w __OnClickSearch) i przechodzi na wlasciwa pod-strone.
		actionsY = skillsY + 18 + 8 * 14 + 10
		actionW = 175
		actionGap = 5
		actions = [
			("Daj Yang", lambda: self.__OpenGiveAmount("gold")),
			("Daj Smocze Monety", lambda: self.__OpenGiveAmount("cash")),
			("Zmien poziom konia", lambda: self.__OpenSetValue("horse")),
			("Zmien range", lambda: self.__OpenSetValue("range")),
			("Dodaj statystyki", self.__OpenSetStat),
			("Zmien skille", self.__OpenSkills),
		]
		for i, (label, handler) in enumerate(actions):
			col = i % 3
			row = i // 3
			btn = ui.Button()
			btn.SetParent(page)
			btn.SetPosition(10 + col * (actionW + actionGap), actionsY + row * 26)
			btn.SetSize(actionW, 22)
			btn.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
			btn.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
			btn.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
			btn.SetText(label)
			btn.SetEvent(handler)
			btn.Show()
			self._widgets.append(btn)

		self._lookupNick = ""
		self._skillListBuffer = ""
		self._lookupSkills = []

	# One field on page_createitem: a read-only display + a "..." button
	# that opens the shared full-page picker (see __BuildPickerPage). kind
	# says which item list the picker should search: "static" uses the
	# items passed in here directly, "vnum"/"stone"/"apply" pull from the
	# matching self._xxxItems list that SetItemListChunk/__init__ fill in.
	def __MakePickerField(self, page, x, y, width, key, kind,
			staticItems=None, defaultValue="0", defaultLabel="(wybierz)"):
		button = ui.Button()
		button.SetParent(page)
		button.SetPosition(x, y)
		button.SetSize(width, 20)
		button.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		button.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		button.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		button.SetText(defaultLabel)
		button.SetEvent(self.__MakeOpenPickerHandler(key))
		button.Show()
		self._widgets.append(button)

		self.pickerFields[key] = {
			"kind": kind, "value": defaultValue, "widget": button, "items": staticItems,
		}

	def __MakeOpenPickerHandler(self, key):
		return lambda: self.__OpenPicker(key)

	def __GetPickerSourceItems(self, key):
		field = self.pickerFields[key]
		if field["kind"] == "static":
			return field["items"]
		if field["kind"] == "vnum":
			return self._vnumItems
		if field["kind"] == "stone":
			return self._stoneItems
		if field["kind"] == "apply":
			return self._applyItems
		if field["kind"] == "botlist":
			return self._botListItems
		if field["kind"] == "availbots":
			return self._availBotsItems
		if field["kind"] == "moblist":
			return self._mobListItems
		if field["kind"] == "metinlist":
			return self._metinListItems
		return []

	def __SetPickerFieldValue(self, key, value, label):
		field = self.pickerFields[key]
		field["value"] = value
		# The bot list "field" is read-only display, not a real form field -
		# it has no widget of its own to write the picked label into.
		if field["widget"] is not None:
			field["widget"].SetText(label)
		if key == "category":
			self.__FetchItemList(value, "vnum")
		# The name-only picker button shows the picked mob/metin's name (no
		# ID, per "Dodaj same nazwy w zakladce bez ID") - the actual vnum
		# used to Spawn still has to land somewhere typable, so it's mirrored
		# into the plain "ID" edit line next to it. Typing an ID directly
		# there works exactly the same way, without ever touching the picker.
		elif key == "mob_picker":
			self.spawnMobIdEdit.SetText(value)
		elif key == "metin_picker":
			self.spawnMetinIdEdit.SetText(value)

	def __OpenPicker(self, key):
		self._pickerKey = key
		# Whichever tab page was actually open when the "..." field was
		# clicked - hardcoding "page_createitem" here hid the WRONG page
		# whenever a field on a different tab (e.g. Blokada Konta) opened
		# a picker, leaving that tab's own content visible underneath.
		self._pickerReturnPage = self._currentPageName
		self.pickerSearchEdit.SetText("")
		self._pickerLastQuery = ""
		self._pickerPageOffset = 0
		if key == "mob_picker" and not self._mobListItems:
			self.__FetchItemList("0", "moblist")
		elif key == "metin_picker" and not self._metinListItems:
			self.__FetchItemList("0", "metinlist")
		self.__RefreshPickerRows("")
		self.pages[self._pickerReturnPage].Hide()
		self.pages["page_picker"].Show()

	def __ClosePicker(self):
		self.pages["page_picker"].Hide()
		self.pages[self._pickerReturnPage].Show()

	# Strips anything outside plain ASCII instead of trying to match exact
	# bytes - the typed search text and the item names (round-tripped
	# through the server) aren't guaranteed to use the same encoding for
	# Polish letters, so comparing "kamie" against "kamie" (both stripped
	# of a/e/l/etc-with-diacritics) matches reliably where comparing the
	# raw accented bytes against each other did not.
	def __SearchKey(self, text):
		return "".join(ch for ch in text.lower() if ord(ch) < 128)

	def __RefreshPickerRows(self, text):
		items = self.__GetPickerSourceItems(self._pickerKey)
		query = self.__SearchKey(text.strip())
		filtered = [(value, label) for value, label in items
				if not query or query in self.__SearchKey(label)]

		pageSize = len(self.pickerRows)
		offset = self._pickerPageOffset
		matches = filtered[offset:offset + pageSize]
		self._pickerMatches = matches
		for i, button in enumerate(self.pickerRows):
			if i < len(matches):
				button.SetText(matches[i][1])
				button.Show()
			else:
				button.Hide()

		# "Dalej" only makes sense once a list is actually longer than one
		# page (e.g. "Lista botow gotowych do spawnu" easily has more PIDs
		# than the 42-row grid fits) - hidden otherwise, same as "Wroc" is
		# always shown regardless.
		self.pickerNextButton.Show() if offset + len(matches) < len(filtered) else self.pickerNextButton.Hide()

	def __MakePickerRowHandler(self, index):
		return lambda: self.__OnPickerRowClick(index)

	def __OnClickPickerNextPage(self):
		self._pickerPageOffset += len(self.pickerRows)
		self.__RefreshPickerRows(self.pickerSearchEdit.GetText())

	def __OnPickerRowClick(self, index):
		if index >= len(self._pickerMatches):
			return
		value, label = self._pickerMatches[index]
		self.__SetPickerFieldValue(self._pickerKey, value, label)
		self.__ClosePicker()

	def __BuildPickerPage(self, page):
		slot = ui.SlotBar()
		slot.SetParent(page)
		slot.SetSize(300, 20)
		slot.SetPosition(10, 6)
		slot.AddFlag("not_pick")
		slot.Show()
		self._widgets.append(slot)

		self.pickerSearchEdit = ui.EditLine()
		self.pickerSearchEdit.SetParent(page)
		self.pickerSearchEdit.SetPosition(13, 9)
		self.pickerSearchEdit.SetSize(294, 17)
		self.pickerSearchEdit.SetMax(40)
		self.pickerSearchEdit.Show()
		self._widgets.append(self.pickerSearchEdit)
		self._pickerLastQuery = ""
		self._pickerMatches = []
		self._pickerKey = None
		self._pickerPageOffset = 0

		# Grid instead of a single column - a single column only ever showed
		# 14 of a possibly much longer list at a time while leaving most of
		# the page empty.
		COLS = 3
		ROWS = 14
		colWidth = 175
		colGap = 5
		rowHeight = 20
		gridTop = 34

		self.pickerRows = []
		for i in range(COLS * ROWS):
			col = i % COLS
			row = i // COLS
			rowButton = ui.Button()
			rowButton.SetParent(page)
			rowButton.SetPosition(10 + col * (colWidth + colGap), gridTop + row * rowHeight)
			rowButton.SetSize(colWidth, 19)
			rowButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
			rowButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
			rowButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
			rowButton.SetEvent(self.__MakePickerRowHandler(i))
			rowButton.Hide()
			self._widgets.append(rowButton)
			self.pickerRows.append(rowButton)

		backButton = ui.Button()
		backButton.SetParent(page)
		backButton.SetPosition(10, gridTop + ROWS * rowHeight + 4)
		backButton.SetSize(80, 22)
		backButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		backButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		backButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		backButton.SetText("Wroc")
		backButton.SetEvent(self.__ClosePicker)
		backButton.Show()
		self._widgets.append(backButton)

		nextButton = ui.Button()
		nextButton.SetParent(page)
		nextButton.SetPosition(95, gridTop + ROWS * rowHeight + 4)
		nextButton.SetSize(80, 22)
		nextButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		nextButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		nextButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		nextButton.SetText("Dalej")
		nextButton.SetEvent(self.__OnClickPickerNextPage)
		nextButton.Hide()
		self._widgets.append(nextButton)
		self.pickerNextButton = nextButton

	def __BuildCreateItemPage(self, page):
		self._vnumItems = []
		self._stoneItems = [("0", "(puste)")]
		self._applyItems = [("0", "(brak)")]
		for typeId, suffix in GM_PANEL_APPLY_SUFFIXES:
			self._applyItems.append((str(typeId), self.__ApplyLabel(suffix)))

		self.__MakeText(page, 10, 6, "Wlasciciel:")
		self.createOwnerEdit = self.__MakeEdit(page, 90, 2, 140, 24)

		self.__MakeText(page, 10, 30, "Kategoria:")
		self.__MakePickerField(page, 90, 26, 130, "category", "static",
				staticItems=GM_PANEL_CATEGORY_LIST,
				defaultValue=GM_PANEL_CATEGORY_LIST[0][0], defaultLabel=GM_PANEL_CATEGORY_LIST[0][1])

		self.__MakeText(page, 230, 30, "Przedmiot:")
		self.__MakePickerField(page, 300, 26, 140, "vnum", "vnum")

		self.__MakeText(page, 450, 30, "Ilosc:")
		self.createCountEdit = self.__MakeEdit(page, 485, 26, 50, 5)

		self.__MakeText(page, 10, 54, "Miejsce przyznania:")
		self.__MakePickerField(page, 140, 50, 150, "location", "static",
				staticItems=GM_PANEL_LOCATION_LIST,
				defaultValue=GM_PANEL_LOCATION_LIST[0][0], defaultLabel=GM_PANEL_LOCATION_LIST[0][1])

		self.__MakeText(page, 10, 80, "Kamienie Duszy (0 = puste):")
		sx = 10
		for i in range(6):
			self.__MakePickerField(page, sx, 98, 84, "socket%d" % i, "stone",
					defaultValue="0", defaultLabel="(puste)")
			sx += 87

		self.__MakeText(page, 10, 124, "Bonusy (typ 0 = puste):")
		self.createAttrValueEdits = []
		ay = 142
		for i in range(7):
			self.__MakeText(page, 10, ay + 3, "Bonus %d:" % (i + 1))
			self.__MakePickerField(page, 90, ay, 200, "attr%d" % i, "apply",
					defaultValue="0", defaultLabel="(brak)")
			self.__MakeText(page, 300, ay + 3, "Wartosc:")
			valueEdit = self.__MakeEdit(page, 360, ay, 60, 6)
			self.createAttrValueEdits.append(valueEdit)
			ay += 22

		createButton = ui.Button()
		createButton.SetParent(page)
		createButton.SetPosition(10, ay + 8)
		createButton.SetSize(120, 24)
		createButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		createButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		createButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		createButton.SetText("Utworz")
		createButton.SetEvent(self.__OnClickCreateItem)
		createButton.Show()
		self._widgets.append(createButton)

		self.createItemStatus = self.__MakeText(page, 140, ay + 14, "")

		# "Marmur Poli" - a second, independent creation flow on the same
		# tab: pick a creature from the curated Wiki list (GM_PANEL_POLY_
		# MOB_LIST) and it goes straight into the GM's OWN inventory, not a
		# named owner's - this is a tool for the GM testing/using it, not
		# for handing something to a player like the form above it.
		polyY = ay + 40
		self.__MakeText(page, 10, polyY + 4, "Marmur Poli:")
		self.__MakePickerField(page, 100, polyY, 230, "poly_mob", "static",
				staticItems=GM_PANEL_POLY_MOB_LIST,
				defaultValue="0", defaultLabel="(wybierz stworzenie)")

		polyCreateButton = ui.Button()
		polyCreateButton.SetParent(page)
		polyCreateButton.SetPosition(340, polyY)
		polyCreateButton.SetSize(90, 22)
		polyCreateButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		polyCreateButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		polyCreateButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		polyCreateButton.SetText("Utworz")
		polyCreateButton.SetEvent(self.__OnClickCreatePolyItem)
		polyCreateButton.Show()
		self._widgets.append(polyCreateButton)

		self.createPolyItemStatus = self.__MakeText(page, 440, polyY + 4, "")

		# Item lists are NOT fetched here - wndGMPanel is built for every
		# login regardless of GM status (F9 just gates showing it), so
		# firing gmpanel_itemlist automatically at construction time meant
		# a burst of a dozen+ chat-command response packets landing right
		# as every character enters the game world. That's exactly the kind
		# of "too early / too much at once" traffic that broke the login
		# handshake before (see the SetGMFlag comment in char.cpp) - lazy
		# load these the first time the tab is actually opened instead
		# (see __SetPage below).
		self._createItemListsLoaded = False

	def __OnClickCreateItem(self):
		owner = self.createOwnerEdit.GetText().strip()
		vnum = self.pickerFields["vnum"]["value"]
		count = self.createCountEdit.GetText().strip() or "1"
		location = self.pickerFields["location"]["value"]

		if not owner or not vnum or vnum == "0":
			self.createItemStatus.SetText("Podaj wlasciciela i wybierz przedmiot.")
			return

		fields = [owner, vnum, count, location]
		for i in range(6):
			fields.append(self.pickerFields["socket%d" % i]["value"])
		for i in range(7):
			fields.append(self.pickerFields["attr%d" % i]["value"])
			fields.append(self.createAttrValueEdits[i].GetText().strip() or "0")

		# "|" is our own field delimiter and a space would get the whole
		# chat command line split apart by the client<->server parser (see
		# the comment on gmpanel_createitem, cmd_gm.cpp) - refuse both.
		for value in fields:
			if "|" in value or " " in value:
				self.createItemStatus.SetText("Niedozwolony znak w polu.")
				return

		self.createItemStatus.SetText("Tworze...")
		net.SendChatPacket("/gmpanel_createitem %s" % "|".join(fields))

	# Called from game.py's server-command dispatcher with the raw payload
	# do_gmpanel_createitem (cmd_gm.cpp) sent back.
	def SetCreateItemResult(self, data):
		if data == "OK":
			self.createItemStatus.SetText("Przedmiot utworzony.")
		elif data == "ERR_OWNER_OFFLINE":
			self.createItemStatus.SetText("Gracz nie jest online.")
		elif data == "ERR_BADVNUM":
			self.createItemStatus.SetText("Zly vnum przedmiotu.")
		elif data == "ERR_NOSPACE":
			self.createItemStatus.SetText("Brak miejsca docelowego.")
		elif data == "ERR_SAFEBOX_CLOSED":
			self.createItemStatus.SetText("Gracz nie otwieral Magazynu w tej sesji.")
		elif data == "ERR_MALL_CLOSED":
			self.createItemStatus.SetText("Gracz nie otwieral Itemshopu w tej sesji.")
		elif data == "ERR_BADDATA":
			self.createItemStatus.SetText("Blad danych.")
		else:
			self.createItemStatus.SetText("Blad: %s" % data)

	def __OnClickCreatePolyItem(self):
		vnum = self.pickerFields["poly_mob"]["value"]
		if not vnum or vnum == "0":
			self.createPolyItemStatus.SetText("Wybierz stworzenie.")
			return
		self.createPolyItemStatus.SetText("Tworze...")
		net.SendChatPacket("/gmpanel_polyitem %s" % vnum)

	# Called from game.py's server-command dispatcher with do_gmpanel_polyitem's
	# response - goes straight into the GM's own inventory, no owner field.
	def SetPolyItemResult(self, data):
		if data == "OK":
			self.createPolyItemStatus.SetText("Marmur dodany do EQ.")
		elif data == "ERR_BADVNUM":
			self.createPolyItemStatus.SetText("Nieznane stworzenie.")
		elif data == "ERR_NOSPACE":
			self.createPolyItemStatus.SetText("Brak miejsca w EQ.")
		elif data == "ERR_NOITEM":
			self.createPolyItemStatus.SetText("Blad: nie mozna utworzyc marmuru.")
		else:
			self.createPolyItemStatus.SetText("Blad: %s" % data)

	def __BuildBanPage(self, page):
		self.__MakeText(page, 10, 6, "Nick:")
		self.banAccountEdit = self.__MakeEdit(page, 90, 2, 160, 24)

		self.__MakeText(page, 10, 30, "Akcja:")
		banActionItems = [
			("check",	"Sprawdz status"),
			("temp",	"Blokada tymczasowa"),
			("perm",	"Blokada permanentna"),
			("unlock",	"Odblokuj"),
		]
		self.__MakePickerField(page, 90, 26, 190, "ban_action", "static",
				staticItems=banActionItems, defaultValue="check", defaultLabel="Sprawdz status")

		self.__MakeText(page, 10, 54, "Dni (tylko blokada tymczasowa):")
		self.banDaysEdit = self.__MakeEdit(page, 210, 50, 60, 4)

		execButton = ui.Button()
		execButton.SetParent(page)
		execButton.SetPosition(10, 82)
		execButton.SetSize(120, 24)
		execButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		execButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		execButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		execButton.SetText("Wykonaj")
		execButton.SetEvent(self.__OnClickBanAccount)
		execButton.Show()
		self._widgets.append(execButton)

		kickButton = ui.Button()
		kickButton.SetParent(page)
		kickButton.SetPosition(140, 82)
		kickButton.SetSize(200, 24)
		kickButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		kickButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		kickButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		kickButton.SetText("Wyrzuc z serwera")
		kickButton.SetEvent(self.__OnClickKickAccount)
		kickButton.Show()
		self._widgets.append(kickButton)

		self.banStatus = self.__MakeText(page, 10, 112, "")

		self.__MakeText(page, 10, 132,
				"Blokada dziala od NASTEPNEGO logowania na to konto - nie")
		self.__MakeText(page, 10, 146,
				"wylogowuje juz zalogowanej sesji. Wyrzuc z serwera dziala")
		self.__MakeText(page, 10, 160,
				"tylko gdy gracz jest AKTUALNIE online.")

	def __OnClickKickAccount(self):
		nick = self.banAccountEdit.GetText().strip()
		if not nick or " " in nick or "|" in nick:
			self.banStatus.SetText("Podaj nick.")
			return
		self.banStatus.SetText("Wyrzucam...")
		net.SendChatPacket("/gmpanel_account %s" % "|".join([nick, "kick", "1", "-"]))

	def __OnClickBanAccount(self):
		nick = self.banAccountEdit.GetText().strip()
		action = self.pickerFields["ban_action"]["value"]
		days = self.banDaysEdit.GetText().strip() or "1"

		if not nick:
			self.banStatus.SetText("Podaj nick.")
			return

		for value in (nick, days):
			if "|" in value or " " in value:
				self.banStatus.SetText("Niedozwolony znak w polu.")
				return

		self.banStatus.SetText("Wysylam...")
		net.SendChatPacket("/gmpanel_account %s" % "|".join([nick, action, days, "-"]))

	# Called from game.py's server-command dispatcher with the raw payload
	# do_gmpanel_account (cmd_gm.cpp) sent back.
	def SetAccountResult(self, data):
		if data.startswith("STATUS|"):
			parts = data.split("|")
			if len(parts) == 3:
				self.banStatus.SetText("Status konta: %s, dostep: %s" % (parts[1], parts[2]))
			else:
				self.banStatus.SetText("Blad odpowiedzi serwera.")
		elif data == "OK":
			self.banStatus.SetText("Wykonano.")
		elif data == "ERR_NOTFOUND":
			self.banStatus.SetText("Gracz nie istnieje.")
		elif data == "ERR_BADNAME":
			self.banStatus.SetText("Niedozwolony znak w nazwie konta.")
		elif data == "ERR_BADDATA":
			self.banStatus.SetText("Blad danych.")
		elif data == "ERR_QUERY":
			self.banStatus.SetText("Blad zapytania do bazy.")
		elif data == "ERR_NOTONLINE":
			self.banStatus.SetText("Gracz nie jest online.")
		elif data == "ERR_SELF":
			self.banStatus.SetText("Nie mozesz wyrzucic samego siebie.")
		else:
			self.banStatus.SetText("Blad: %s" % data)

	def __BuildAddGMPage(self, page):
		self.__MakeText(page, 10, 6, "Nick:")
		self.addGmNickEdit = self.__MakeEdit(page, 90, 2, 160, 24)

		self.__MakeText(page, 10, 30, "Ranga:")
		gmRankItems = [
			("HIGH_WIZARD",	"High Wizard (GM)"),
			("GOD",			"God (Admin)"),
			("LOW_WIZARD",	"Low Wizard (Moderator)"),
			("IMPLEMENTOR",	"Implementor (Wlasciciel)"),
		]
		self.__MakePickerField(page, 90, 26, 200, "gm_rank", "static",
				staticItems=gmRankItems, defaultValue="HIGH_WIZARD", defaultLabel="High Wizard (GM)")

		addButton = ui.Button()
		addButton.SetParent(page)
		addButton.SetPosition(10, 54)
		addButton.SetSize(120, 24)
		addButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		addButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		addButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		addButton.SetText("Dodaj GM")
		addButton.SetEvent(self.__OnClickAddGM)
		addButton.Show()
		self._widgets.append(addButton)

		removeButton = ui.Button()
		removeButton.SetParent(page)
		removeButton.SetPosition(140, 54)
		removeButton.SetSize(120, 24)
		removeButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		removeButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		removeButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		removeButton.SetText("Usun GM")
		removeButton.SetEvent(self.__OnClickRemoveGM)
		removeButton.Show()
		self._widgets.append(removeButton)

		self.addGmStatus = self.__MakeText(page, 10, 84, "")

		self.__MakeText(page, 10, 106,
				"Dziala natychmiast (bez restartu serwera) - odswieza liste GM")
		self.__MakeText(page, 10, 120,
				"na wszystkich juz zalogowanych postaciach.")

	def __SendAddGM(self, action):
		nick = self.addGmNickEdit.GetText().strip()
		rank = self.pickerFields["gm_rank"]["value"]

		if not nick:
			self.addGmStatus.SetText("Podaj nick.")
			return
		if "|" in nick or " " in nick:
			self.addGmStatus.SetText("Niedozwolony znak w nicku.")
			return

		self.addGmStatus.SetText("Wysylam...")
		net.SendChatPacket("/gmpanel_addgm %s" % "|".join([nick, rank, action]))

	def __OnClickAddGM(self):
		self.__SendAddGM("add")

	def __OnClickRemoveGM(self):
		self.__SendAddGM("remove")

	# Called from game.py's server-command dispatcher with the raw payload
	# do_gmpanel_addgm (cmd_gm.cpp) sent back.
	def SetAddGMResult(self, data):
		if data == "OK":
			self.addGmStatus.SetText("Wykonano.")
		elif data == "ERR_NOTFOUND":
			self.addGmStatus.SetText("Gracz nie istnieje.")
		elif data == "ERR_BADNAME":
			self.addGmStatus.SetText("Niedozwolony znak w nicku.")
		elif data == "ERR_BADRANK":
			self.addGmStatus.SetText("Niepoprawna ranga.")
		elif data == "ERR_BADDATA":
			self.addGmStatus.SetText("Blad danych.")
		elif data == "ERR_QUERY":
			self.addGmStatus.SetText("Blad zapytania do bazy.")
		else:
			self.addGmStatus.SetText("Blad: %s" % data)

	def __BuildSpawnBotsPage(self, page):
		self.__MakeText(page, 10, 6, "ID Bota:")
		self.spawnPidEdit = self.__MakeEdit(page, 100, 2, 80, 6)

		self.__MakeText(page, 200, 6, "Ilosc:")
		self.spawnCountEdit = self.__MakeEdit(page, 250, 2, 60, 4)

		self.__MakeText(page, 10, 30, "Imperium:")
		empireItems = [
			("1", "1 - Shinsoo"),
			("2", "2 - Chunjo"),
			("3", "3 - Jinno"),
		]
		self.__MakePickerField(page, 100, 26, 150, "spawn_empire", "static",
				staticItems=empireItems, defaultValue="1", defaultLabel="1 - Shinsoo")

		spawnButton = ui.Button()
		spawnButton.SetParent(page)
		spawnButton.SetPosition(10, 54)
		spawnButton.SetSize(100, 24)
		spawnButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		spawnButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		spawnButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		spawnButton.SetText("Spawn")
		spawnButton.SetEvent(self.__OnClickSpawnBots)
		spawnButton.Show()
		self._widgets.append(spawnButton)

		despawnButton = ui.Button()
		despawnButton.SetParent(page)
		despawnButton.SetPosition(120, 54)
		despawnButton.SetSize(100, 24)
		despawnButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		despawnButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		despawnButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		despawnButton.SetText("Despawn")
		despawnButton.SetEvent(self.__OnClickDespawnBots)
		despawnButton.Show()
		self._widgets.append(despawnButton)

		self.spawnStatus = self.__MakeText(page, 10, 84, "")

		refreshButton = ui.Button()
		refreshButton.SetParent(page)
		refreshButton.SetPosition(10, 104)
		refreshButton.SetSize(220, 22)
		refreshButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		refreshButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		refreshButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		refreshButton.SetText("Odswiez liste")
		refreshButton.SetEvent(self.__OnClickRefreshBotList)
		refreshButton.Show()
		self._widgets.append(refreshButton)

		availButton = ui.Button()
		availButton.SetParent(page)
		availButton.SetPosition(240, 104)
		availButton.SetSize(260, 22)
		availButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		availButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		availButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		availButton.SetText("Boty do spawnu")
		availButton.SetEvent(self.__OnClickShowAvailableBots)
		availButton.Show()
		self._widgets.append(availButton)

		# UI_DEF_FONT_LARGE turned out to be a glyph-incomplete resource in
		# this build (showed border/dingbat placeholders instead of Latin
		# letters, reported live) - reverted to the default font. SetOutline
		# is the only safe "make it stand out" left without a confirmed
		# larger font name; two overlapped copies offset by one pixel fake a
		# bold weight on top of that, matching how outlined titles read
		# heavier elsewhere in this engine's own UI.
		teleportTitle = self.__MakeText(page, 169, 134, "Teleport")
		teleportTitle.SetOutline(True)
		teleportTitleBold = self.__MakeText(page, 168, 134, "Teleport")
		teleportTitleBold.SetOutline(True)

		# Trzecia kolumna ("Zapisane miejsca") musiala sie zmiescic obok, wiec
		# dwie kolumny teleportu zwezaja sie z 310 do GM_PANEL_TELE_BTN_W.
		# Najdluzsza etykieta ("Normalny loch malp") nadal miesci sie z zapasem.
		waypointTitle = self.__MakeText(page, GM_PANEL_WP_X + 79, 134, "Zapisane miejsca")
		waypointTitle.SetOutline(True)
		waypointTitleBold = self.__MakeText(page, GM_PANEL_WP_X + 78, 134, "Zapisane miejsca")
		waypointTitleBold.SetOutline(True)

		teleY = 160
		teleAll = GM_PANEL_TELEPORT_LIST + GM_PANEL_TELEPORT_EXTRA_LIST + GM_PANEL_TELEPORT_DUNGEON_LIST
		for i in range(0, len(teleAll), 2):
			left = teleAll[i]
			self.__MakeTeleportButton(page, 10, teleY, *left)
			if i + 1 < len(teleAll):
				right = teleAll[i + 1]
				self.__MakeTeleportButton(page, 10 + GM_PANEL_TELE_BTN_W + 5, teleY, *right)
			teleY += 26

		# Wlasne punkty powrotu: [Zapisz] [nazwa] [Wczytaj] w jednym wierszu.
		# Nazwe sklada klient z mapy, na ktorej stoi (MINIMAP_ZONE_NAME_DICT),
		# ale zapamietywana pozycja pochodzi zawsze z serwera - patrz
		# do_gmpanel_waypoint w cmd_gm.cpp.
		self.waypointLabels = {}
		wpY = 160
		for slot in range(1, GM_PANEL_WAYPOINT_SLOTS + 1):
			self.__MakeWaypointButton(page, GM_PANEL_WP_X, wpY, "Zapisz",
					self.__MakeWaypointHandler(slot, "save"))
			self.waypointLabels[slot] = self.__MakeText(page,
					GM_PANEL_WP_X + GM_PANEL_WP_BTN_W + 8, wpY + 4, "(puste)")
			self.__MakeWaypointButton(page, GM_PANEL_WP_X + GM_PANEL_WP_W - GM_PANEL_WP_BTN_W,
					wpY, "Wczytaj", self.__MakeWaypointHandler(slot, "load"))
			wpY += 26

		self.teleportStatus = self.__MakeText(page, 10, teleY + 6, "")

		# Read-only "field" for the bot list - no widget of its own, it just
		# reuses the same full-page picker every other list already uses
		# instead of a small on-page list (which is also what made this
		# reliably visible - see the comment on GMSearchCombo/page_picker).
		self.pickerFields["__botlist__"] = {"kind": "botlist", "value": "0", "widget": None, "items": None}
		self._botListItems = []

		# Available-to-spawn list DOES have a widget: spawnPidEdit itself -
		# clicking a PID in the picker fills it in directly, ready to press
		# "Spawn" (same generic __SetPickerFieldValue path every other
		# picker field already uses, just pointed at this edit line).
		self.pickerFields["__availbots__"] = {"kind": "availbots", "value": "0", "widget": self.spawnPidEdit, "items": None}
		self._availBotsItems = []

	def __MakeTeleportButton(self, page, x, y, name, xMeters, yMeters):
		button = ui.Button()
		button.SetParent(page)
		button.SetPosition(x, y)
		button.SetSize(GM_PANEL_TELE_BTN_W, 22)
		button.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		button.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		button.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		button.SetText(name)
		button.SetEvent(self.__MakeTeleportHandler(name, xMeters, yMeters))
		button.Show()
		self._widgets.append(button)
		return button

	def __MakeTeleportHandler(self, name, xMeters, yMeters):
		return lambda: self.__OnClickTeleport(name, xMeters, yMeters)

	def __MakeWaypointButton(self, page, x, y, text, event):
		button = ui.Button()
		button.SetParent(page)
		button.SetPosition(x, y)
		button.SetSize(GM_PANEL_WP_BTN_W, 22)
		button.SetUpVisual("d:/ymir work/ui/public/small_button_01.sub")
		button.SetOverVisual("d:/ymir work/ui/public/small_button_02.sub")
		button.SetDownVisual("d:/ymir work/ui/public/small_button_03.sub")
		button.SetText(text)
		button.SetEvent(event)
		button.Show()
		self._widgets.append(button)
		return button

	def __MakeWaypointHandler(self, slot, action):
		return lambda: self.__OnClickWaypoint(slot, action)

	# Nazwe miejsca sklada klient, bo tylko on zna polskie nazwy map
	# (localeInfo.MINIMAP_ZONE_NAME_DICT). Spacje zamieniamy na '_', bo
	# komendy czatu serwer dzieli po bialych znakach. To jedyna rzecz, jaka
	# stad leci - zapamietywana pozycja pochodzi z postaci po stronie
	# serwera, wiec etykieta niczego nie decyduje.
	def __CurrentPlaceName(self):
		try:
			mapName = background.GetCurrentMapName()
		except:
			mapName = ""
		name = localeInfo.MINIMAP_ZONE_NAME_DICT.get(mapName, "")
		if not name:
			name = mapName or "Nieznane"
		try:
			x, y, z = player.GetMainCharacterPosition()
		except:
			x, y = 0, 0
		return ("%s %d,%d" % (name, int(x) / 100, int(y) / 100)).replace(" ", "_")

	def __OnClickWaypoint(self, slot, action):
		if action == "save":
			self.teleportStatus.SetText("Zapisuje miejsce %d..." % slot)
			net.SendChatPacket("/gmpanel_waypoint save %d %s" % (slot, self.__CurrentPlaceName()))
		else:
			self.teleportStatus.SetText("Wczytuje miejsce %d..." % slot)
			net.SendChatPacket("/gmpanel_waypoint load %d" % slot)

	# "GMPanelWaypoint <slot>|<etykieta>" - odpowiedz na zapis oraz po jednej
	# na kazde zajete miejsce przy pierwszym wejsciu na strone.
	def SetWaypoint(self, data):
		labels = getattr(self, "waypointLabels", None)
		if not labels:
			return
		parts = data.split("|")
		if len(parts) < 2:
			return
		try:
			slot = int(parts[0])
		except:
			return
		if slot not in labels:
			return
		label = parts[1].replace("_", " ").strip()
		labels[slot].SetText(label or "(puste)")

	# /warp is the vanilla GM command (cmd_gm.cpp, do_warp) - it already
	# answers on its own with "You warp to ( x, y )" in the normal info
	# chat, so this just gives immediate feedback on the panel itself
	# without waiting for (or needing) a GMPanel-style server round-trip.
	def __OnClickTeleport(self, name, xMeters, yMeters):
		self.teleportStatus.SetText("Teleportuje do: %s" % name)
		net.SendChatPacket("/warp %d %d" % (xMeters, yMeters))

	def __OnClickSpawnBots(self):
		self.__SendSpawn("spawn")

	def __OnClickDespawnBots(self):
		self.__SendSpawn("despawn")

	def __SendSpawn(self, action):
		pid = self.spawnPidEdit.GetText().strip()
		count = self.spawnCountEdit.GetText().strip()
		empire = self.pickerFields["spawn_empire"]["value"]

		if not pid or not count:
			self.spawnStatus.SetText("Podaj PID i ilosc.")
			return

		self.spawnStatus.SetText("Wysylam...")
		net.SendChatPacket("/gmpanel_spawn %s" % "|".join([pid, count, empire, action]))

	def __OnClickRefreshBotList(self):
		self._botListItems = []
		self.__OpenPicker("__botlist__")
		self.__FetchItemList("-", "botlist")

	def __OnClickShowAvailableBots(self):
		self._availBotsItems = []
		self.__OpenPicker("__availbots__")
		self.__FetchItemList("-", "availbots")

	# Called from game.py's server-command dispatcher with the raw payload
	# do_gmpanel_spawn (cmd_gm.cpp) sent back.
	def SetSpawnResult(self, data):
		if data.startswith("OK|"):
			parts = data.split("|")
			if len(parts) == 4:
				self.spawnStatus.SetText("Wykonano %s/%s. Aktywnych ogolem: %s" % (parts[1], parts[2], parts[3]))
			else:
				self.spawnStatus.SetText("Blad odpowiedzi serwera.")
		elif data == "ERR_BADDATA":
			self.spawnStatus.SetText("Blad danych - sprawdz PID i ilosc.")
		elif data == "ERR_BADEMPIRE":
			self.spawnStatus.SetText("Niepoprawne imperium.")
		else:
			self.spawnStatus.SetText("Blad: %s" % data)

	def __MakeSetPageHandler(self, shortName):
		return lambda: self.__SetPage("page_" + shortName)

	# Called from Interface.OpenGMLookupFor (interfacemodule.py), itself
	# called from uitarget.TargetBoard's new "Sprawdz" button - the target
	# menu already knows the clicked player's name, so this jumps straight
	# to the lookup tab pre-filled and already searching instead of making
	# the GM retype a nick they just clicked on.
	def OpenLookupFor(self, name):
		self.__SetPage("page_lookup")
		self.lookupNickEdit.SetText(name)
		self.__OnClickSearch()

	def Destroy(self):
		self.ClearDictionary()
		self.pages = {}
		self.tabs = {}
		self.lookupValues = {}
		self._widgets = []
		self._allCombos = []

	def OnPressEscapeKey(self):
		self.Hide()
		return True

	def __SetPage(self, pageName):
		self._currentPageName = pageName
		for name, page in self.pages.items():
			if name == pageName:
				page.Show()
			else:
				page.Hide()
		for name, tab in self.tabs.items():
			pageOfTab = "page_" + name[len("tab_"):]
			if pageOfTab == pageName:
				tab.Down()
			else:
				tab.SetUp()

		if pageName == "page_createitem" and not self._createItemListsLoaded:
			self._createItemListsLoaded = True
			self.__FetchItemList(GM_PANEL_CATEGORY_LIST[0][0], "vnum")
			self.__FetchItemList("stone", "stone")

		if pageName == "page_spawnbots" and not self._waypointsLoaded:
			self._waypointsLoaded = True
			net.SendChatPacket("/gmpanel_waypoint list")

		if pageName == "page_serverctrl" and not self._serverctrlRatesLoaded:
			self._serverctrlRatesLoaded = True
			self._ratesFetchFrames = 1
			self._aiWeightsFetchFrames = 1
			net.SendChatPacket("/gmpanel_getrates")
			net.SendChatPacket("/gmpanel_getaiweights")

	def __OnClickSearch(self):
		nick = self.lookupNickEdit.GetText().strip()
		if not nick:
			self.lookupStatus.SetText("Podaj nick.")
			return
		self._lookupNick = nick
		self.lookupStatus.SetText("Szukam...")
		for line in self.lookupSkillLines:
			line.SetText("")
		net.SendChatPacket("/gmpanel_lookup %s" % nick)
		net.SendChatPacket("/gmpanel_skilllist %s" % nick)

	# Called from game.py's server-command dispatcher (BINARY_ServerCommand_Run
	# -> "GMPanelLookupResult" -> here) with the raw pipe-delimited payload
	# do_gmpanel_lookup (cmd_gm.cpp) sent back.
	def SetLookupResult(self, data):
		if data.startswith("ERR_NOTFOUND"):
			self.lookupStatus.SetText("Nie znaleziono gracza.")
			for fieldName in GM_PANEL_LOOKUP_FIELD_ORDER:
				self.lookupValues[fieldName].SetText("-")
			return
		if data.startswith("ERR_BADNAME"):
			self.lookupStatus.SetText("Niedozwolony znak w nicku.")
			return

		parts = data.split("|")
		if len(parts) != len(GM_PANEL_LOOKUP_FIELD_ORDER):
			self.lookupStatus.SetText("Blad odpowiedzi serwera.")
			return

		values = dict(zip(GM_PANEL_LOOKUP_FIELD_ORDER, parts))
		try:
			values["lk_job"] = GM_PANEL_JOB_NAMES.get(int(values["lk_job"]), values["lk_job"])
		except ValueError:
			pass

		for fieldName in GM_PANEL_LOOKUP_FIELD_ORDER:
			self.lookupValues[fieldName].SetText(values.get(fieldName, "-"))

		self.lookupStatus.SetText("OK")

	# Called from game.py's dispatcher with each "GMPanelSkillListResult
	# <isLast>|<data>" payload from do_gmpanel_skilllist (cmd_gm.cpp) -
	# entries are "<vnum>:<name>:<level>:<maxlevel>;". Same chunk-buffer
	# idea as SetItemListChunk, but its own small buffer - this fetch runs
	# alongside (not through) the picker-oriented item-list queue.
	def SetSkillListResult(self, data):
		if data in ("ERR_OFFLINE", "ERR_BADDATA"):
			self._skillListBuffer = ""
			self._lookupSkills = []
			for i, line in enumerate(self.lookupSkillLines):
				if i == 0 and data == "ERR_OFFLINE":
					line.SetText("(postac offline - brak podgladu skilli)")
				else:
					line.SetText("")
			return

		parts = data.split("|", 1)
		if len(parts) != 2:
			return
		isLast, chunk = parts
		self._skillListBuffer += chunk

		if isLast != "1":
			return

		raw = self._skillListBuffer
		self._skillListBuffer = ""

		skills = []
		for entry in raw.split(";"):
			if not entry:
				continue
			bits = entry.split(":")
			if len(bits) != 4:
				continue
			vnum, name, level, maxLevel = bits
			skills.append((vnum, name.replace("_", " "), level, maxLevel))

		self._lookupSkills = skills
		for i, line in enumerate(self.lookupSkillLines):
			if i < len(skills):
				vnum, name, level, maxLevel = skills[i]
				line.SetText("%s: %s/%s" % (name, level, maxLevel))
			else:
				line.SetText("")

	######################################################################
	## "Spawn Mobow" - mob i metin spawn are the same layout twice over: a
	## name-only picker (do_gmpanel_moblist/metinlist, filtered server-side
	## by mob_proto.type so metins can never show up in the mob list or vice
	## versa - see CHAR_TYPE_STONE=2 in common/length.h) next to a plain
	## "ID" edit line that either one fills in with the vnum (see
	## __SetPickerFieldValue's mob_picker/metin_picker branch), or that can
	## just be typed into directly - both end up calling the exact same
	## do_gmpanel_spawnmob. "Lista komend GM" opens a separate scrollable
	## text page (page_gmcommands), not the click-to-pick grid every other
	## picker uses, since its entries are full sentences that don't fit the
	## picker's 175px-wide row buttons.
	######################################################################

	def __BuildSpawnMobsPage(self, page):
		self._mobListItems = []
		self._metinListItems = []

		self.__MakeText(page, 10, 6, "Moby:")
		self.__MakePickerField(page, 60, 2, 180, "mob_picker", "moblist",
				defaultValue="0", defaultLabel="(wybierz z listy)")

		self.__MakeText(page, 260, 6, "ID:")
		self.spawnMobIdEdit = self.__MakeEdit(page, 280, 2, 70, 6)

		self.__MakeText(page, 360, 6, "Ilosc:")
		self.spawnMobCountEdit = self.__MakeEdit(page, 410, 2, 60, 4)

		mobSpawnButton = ui.Button()
		mobSpawnButton.SetParent(page)
		mobSpawnButton.SetPosition(480, 2)
		mobSpawnButton.SetSize(90, 20)
		mobSpawnButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		mobSpawnButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		mobSpawnButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		mobSpawnButton.SetText("Spawn")
		mobSpawnButton.SetEvent(self.__OnClickSpawnMob)
		mobSpawnButton.Show()
		self._widgets.append(mobSpawnButton)

		self.spawnMobStatus = self.__MakeText(page, 10, 30, "")

		self.__MakeText(page, 10, 70, "Metiny:")
		self.__MakePickerField(page, 60, 66, 180, "metin_picker", "metinlist",
				defaultValue="0", defaultLabel="(wybierz z listy)")

		self.__MakeText(page, 260, 70, "ID:")
		self.spawnMetinIdEdit = self.__MakeEdit(page, 280, 66, 70, 6)

		self.__MakeText(page, 360, 70, "Ilosc:")
		self.spawnMetinCountEdit = self.__MakeEdit(page, 410, 66, 60, 4)

		metinSpawnButton = ui.Button()
		metinSpawnButton.SetParent(page)
		metinSpawnButton.SetPosition(480, 66)
		metinSpawnButton.SetSize(90, 20)
		metinSpawnButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		metinSpawnButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		metinSpawnButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		metinSpawnButton.SetText("Spawn")
		metinSpawnButton.SetEvent(self.__OnClickSpawnMetin)
		metinSpawnButton.Show()
		self._widgets.append(metinSpawnButton)

		self.spawnMetinStatus = self.__MakeText(page, 10, 94, "")

		gmCmdButton = ui.Button()
		gmCmdButton.SetParent(page)
		gmCmdButton.SetPosition(10, 130)
		gmCmdButton.SetSize(190, 24)
		gmCmdButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		gmCmdButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		gmCmdButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		gmCmdButton.SetText("Lista komend GM")
		gmCmdButton.SetEvent(self.__OnClickShowGMCommands)
		gmCmdButton.Show()
		self._widgets.append(gmCmdButton)

		purgeButton = ui.Button()
		purgeButton.SetParent(page)
		purgeButton.SetPosition(210, 130)
		purgeButton.SetSize(150, 24)
		purgeButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		purgeButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		purgeButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		purgeButton.SetText("Usun Moby")
		purgeButton.SetEvent(lambda: net.SendChatPacket("/purge"))
		purgeButton.Show()
		self._widgets.append(purgeButton)

		# The reference list calls this "/weak", but the real ACMD is
		# "/weaken" (cmd_gm.cpp, do_weaken) - drops every mob currently
		# around the GM to 1 HP, no target needed first.
		weakenButton = ui.Button()
		weakenButton.SetParent(page)
		weakenButton.SetPosition(370, 130)
		weakenButton.SetSize(150, 24)
		weakenButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		weakenButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		weakenButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		weakenButton.SetText("Moby 1 hit")
		weakenButton.SetEvent(lambda: net.SendChatPacket("/weaken"))
		weakenButton.Show()
		self._widgets.append(weakenButton)

		self.__BuildBossMetinSection(page, 160)

	# "Bossy" (pule stale, wybrane na karcie - patrz GM_PANEL_BOSS_TIERS) i
	# "Metiny" (przedzial poziomow, serwer sam dobiera z bazy - patrz
	# GM_PANEL_METIN_TIERS) ponizej "Lista komend GM". Kazdy przedzial to
	# napis na srodku + 3 przyciski (1/5/10); klikniecie prosi serwer o tyle
	# losowych z puli/przedzialu, nigdy same kopie jednego (gmpanel_spawnrandom
	# mobs/metin w cmd_gm.cpp robi to porzadnie, klient tylko podaje pule i
	# ilosc).
	def __BuildBossMetinSection(self, page, top):
		y = top
		self.__MakeCenteredText(page, y, "ZUO"); y += 12
		self.__MakeCenteredText(page, y, "Bossy"); y += 12
		for label, pool in GM_PANEL_BOSS_TIERS:
			self.__MakeCenteredText(page, y, label); y += 12
			self.__MakeSpawnTripleButtons(page, y, "Boss", "Boss",
					self.__MakeBossSpawnHandler(pool))
			y += 23

		y += 4
		self.__MakeCenteredText(page, y, "Metiny"); y += 12
		for label, lo, hi in GM_PANEL_METIN_TIERS:
			self.__MakeCenteredText(page, y, label); y += 12
			self.__MakeSpawnTripleButtons(page, y, "Metin", "Metin",
					self.__MakeMetinSpawnHandler(lo, hi))
			y += 23

		self.spawnBossStatus = self.__MakeText(page, 10, y + 4, "")

	def __MakeCenteredText(self, page, y, text):
		textLine = self.__MakeText(page, 330, y, text)
		textLine.SetHorizontalAlignCenter()
		return textLine

	# singularLabel "1 <singularLabel>" (e.g. "1 Boss"), pluralLabel
	# "5/10 <pluralLabel>" (e.g. "5 Bossow") - onClick(count) sends the
	# request for whichever button was pressed.
	def __MakeSpawnTripleButtons(self, page, y, singularLabel, pluralLabel, onClick):
		specs = [(1, 10, "1 %s" % singularLabel), (5, 220, "5 %s" % pluralLabel),
				(10, 430, "10 %s" % pluralLabel)]
		for count, x, text in specs:
			button = ui.Button()
			button.SetParent(page)
			button.SetPosition(x, y)
			button.SetSize(190, 22)
			button.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
			button.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
			button.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
			button.SetText(text)
			button.SetEvent(lambda c=count: onClick(c))
			button.Show()
			self._widgets.append(button)

	def __MakeBossSpawnHandler(self, pool):
		vnumStr = ",".join(str(v) for v in pool)
		def handler(count):
			self.spawnBossStatus.SetText("Spawnuje...")
			net.SendChatPacket("/gmpanel_spawnrandommobs %s|%d" % (vnumStr, count))
		return handler

	def __MakeMetinSpawnHandler(self, lo, hi):
		def handler(count):
			self.spawnBossStatus.SetText("Spawnuje...")
			net.SendChatPacket("/gmpanel_spawnrandommetin %d|%d|%d" % (lo, hi, count))
		return handler

	# Called from game.py's dispatcher with do_gmpanel_spawnrandommobs's
	# response - "OK|<spawned>|<requested>" or an ERR_* code.
	def SetSpawnBossResult(self, data):
		if data.startswith("OK|"):
			parts = data.split("|")
			if len(parts) == 3:
				self.spawnBossStatus.SetText("Zespawnowano bossow: %s/%s." % (parts[1], parts[2]))
				return
		self.spawnBossStatus.SetText("Blad: %s" % data)

	# Called from game.py's dispatcher with do_gmpanel_spawnrandommetin's
	# response - same shape as SetSpawnBossResult.
	def SetSpawnMetinResult(self, data):
		if data.startswith("OK|"):
			parts = data.split("|")
			if len(parts) == 3:
				self.spawnBossStatus.SetText("Zespawnowano metinow: %s/%s." % (parts[1], parts[2]))
				return
		self.spawnBossStatus.SetText("Blad: %s" % data)

	def __OnClickSpawnMob(self):
		self.__SendSpawnMob(self.spawnMobIdEdit, self.spawnMobCountEdit, self.spawnMobStatus, "mob")

	def __OnClickSpawnMetin(self):
		self.__SendSpawnMob(self.spawnMetinIdEdit, self.spawnMetinCountEdit, self.spawnMetinStatus, "metin")

	def __SendSpawnMob(self, idEdit, countEdit, statusText, kind):
		vnum = idEdit.GetText().strip()
		count = countEdit.GetText().strip()

		if not vnum or not count:
			statusText.SetText("Podaj ID i ilosc.")
			return

		statusText.SetText("Wysylam...")
		net.SendChatPacket("/gmpanel_spawnmob %s" % "|".join([vnum, count, kind]))

	# Called from game.py's server-command dispatcher with do_gmpanel_spawnmob's
	# response - "<kind>|OK|<done>|<count>" or "<kind>|ERR_<reason>".
	def SetSpawnMobResult(self, data):
		parts = data.split("|")
		if len(parts) < 2:
			return
		kind = parts[0]
		statusText = self.spawnMobStatus if kind == "mob" else self.spawnMetinStatus

		if parts[1] == "OK" and len(parts) == 4:
			statusText.SetText("Zespawnowano %s/%s." % (parts[2], parts[3]))
		elif parts[1] == "ERR_BADDATA":
			statusText.SetText("Blad danych - sprawdz ID i ilosc.")
		elif parts[1] == "ERR_NOTFOUND":
			statusText.SetText("Nie znaleziono moba/metina o takim ID.")
		else:
			statusText.SetText("Blad: %s" % parts[1])

	# "Sterowanie Serwerem" - 3 mnozniki serwera (exp/drop przedmiotow/drop
	# yang) w player.web_admin_rates, ta sama tabela ktora czyta i pisze
	# panel webowy na stronie /rates. "Zapisz" przy kazdym polu zapisuje
	# TYLKO ta jedna wartosc do bazy - nic to jeszcze nie zmienia na
	# serwerze, bo exp/drop/gold w mob_proto i tabelach dropu sa przeliczane
	# raz, przy starcie core'ow. "Zrestartuj serwer" zglasza restart tym
	# samym mechanizmem co apply_rates.sh (spool w kontenerze gry), ktory
	# przelicza te tabele z ich *.m2orig baseline i sam restartuje core'y w
	# bezpiecznej kolejnosci - stad "Nic sie nie usunie" w dopisku.
	def __BuildServerControlPage(self, page):
		self._serverctrlEdits = {}
		self._serverctrlStatus = {}
		self._serverctrlRatesLoaded = False
		self._waypointsLoaded = False
		self._serverctrlLastSaved = None
		# >0 while a gmpanel_getrates/getaiweights reply is outstanding -
		# OnUpdate below times this out instead of leaving "Wczytuje..." on
		# screen forever when a reply never lands (reported live - "czasem
		# jest napis Wczytuje... i nie wczytuje").
		self._ratesFetchFrames = 0
		self._aiWeightsFetchFrames = 0

		y = 10
		for name, attrSuffix, label in GM_PANEL_RATE_FIELDS:
			self.__MakeText(page, 10, y + 4, label)
			edit = self.__MakeEdit(page, 180, y, 60, 5)
			self._serverctrlEdits[name] = edit
			setattr(self, "serverctrl%sEdit" % attrSuffix, edit)

			saveButton = ui.Button()
			saveButton.SetParent(page)
			saveButton.SetPosition(250, y - 2)
			saveButton.SetSize(80, 22)
			saveButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
			saveButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
			saveButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
			saveButton.SetText("Zapisz")
			saveButton.SetEvent(self.__MakeSaveRateHandler(name))
			saveButton.Show()
			self._widgets.append(saveButton)

			status = self.__MakeText(page, 340, y + 4, "")
			self._serverctrlStatus[name] = status
			setattr(self, "serverctrl%sStatus" % attrSuffix, status)

			y += 30

		self.__MakeText(page, 10, y + 10,
				"(Aby zmiany weszly w zycie trzeba zrestartowac serwer (Nic sie nie usunie))")

		# Both buttons now start at x=10 like everything else on this page,
		# not x=500 - that pushed them past the right edge the moment the
		# panel got any narrower (reported live: "sterowanie serwerem
		# wychodzi poza panel"). Stacked side by side from the left margin
		# instead of a horizontal scrollbar - simpler, and nothing here
		# needs more width than the page already has.
		fetchButton = ui.Button()
		fetchButton.SetParent(page)
		fetchButton.SetPosition(10, y + 28)
		fetchButton.SetSize(210, 22)
		fetchButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		fetchButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		fetchButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		fetchButton.SetText("Pobierz aktualne raty")
		fetchButton.SetEvent(self.__OnClickFetchRates)
		fetchButton.Show()
		self._widgets.append(fetchButton)

		restartButton = ui.Button()
		restartButton.SetParent(page)
		restartButton.SetPosition(230, y + 28)
		restartButton.SetSize(210, 22)
		restartButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		restartButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		restartButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		restartButton.SetText("Zrestartuj serwer")
		restartButton.SetEvent(self.__OnClickRestartServer)
		restartButton.Show()
		self._widgets.append(restartButton)

		self.serverctrlRestartStatus = self.__MakeText(page, 10, y + 56, "")

		self.__BuildAIWeightsSection(page, y + 82)

	# Ustawienia zachowania botow (playerbot_weights.tsv - te same teksty co
	# strona /ai panelu webowego) w scrollowalnej liscie - 17 wierszy z
	# tytulem+opisem+suwakiem (albo przyciskiem ON/OFF dla CHAT/BOOKS/NIGHT)
	# nie miesci sie na karcie na raz. Ten sam wzorzec "scrollbar nad
	# slot-boardem" co Lista komend GM (__BuildGMCommandsPage) - wszystkie
	# wiersze istnieja od razu jako widgety, tylko Show/Hide + przepozycjo-
	# nowanie zaleznie od przewiniecia (17 wierszy to nadal tanie do
	# utrzymania w pamieci na raz).
	def __BuildAIWeightsSection(self, page, top):
		header = self.__MakeText(page, 250, top, "Zachowanie botow")
		header.SetOutline(True)
		headerBold = self.__MakeText(page, 249, top, "Zachowanie botow")
		headerBold.SetOutline(True)

		listWidth = WINDOW_WIDTH - 20 - 30
		listHeight = CONTENT_HEIGHT - top - 54

		listBoard = ui.Window()
		listBoard.SetParent(page)
		listBoard.SetPosition(10, top + 20)
		listBoard.SetSize(listWidth + 20, listHeight)
		listBoard.Show()
		self._widgets.append(listBoard)

		self.aiWeightsScrollBar = ui.ScrollBar()
		self.aiWeightsScrollBar.SetParent(listBoard)
		self.aiWeightsScrollBar.SetPosition(listWidth, 0)
		self.aiWeightsScrollBar.SetScrollBarSize(listHeight)
		self.aiWeightsScrollBar.SetScrollEvent(ui.__mem_func__(self.__OnScrollAIWeights))
		self.aiWeightsScrollBar.Show()
		self._widgets.append(self.aiWeightsScrollBar)

		self.aiWeightsSlotBoard = ui.Window()
		self.aiWeightsSlotBoard.SetParent(listBoard)
		self.aiWeightsSlotBoard.SetPosition(0, 0)
		self.aiWeightsSlotBoard.SetSize(listWidth, listHeight)
		self.aiWeightsSlotBoard.Show()
		self._widgets.append(self.aiWeightsSlotBoard)

		# Local y-offsets within one row block - a dashed rule opens every
		# row so a glance shows where one setting ends and the next begins
		# (reported live: rows ran into each other, "nie wiadomo ktore
		# ustawienie i ktory opis do czego"). Row height is figured PER ROW
		# from its own help text length (~74 chars/wrapped line, matched
		# against BOOKS - 292 chars - actually wrapping to 4 lines live) so
		# the eleven one-line weights stay compact instead of every row
		# paying for the two or three long paragraphs (CHAT/BOOKS/NIGHT/
		# SCRAP/CHEST) - reported live as "za duze odstepy... niech pojawiaja
		# sie po 3". Scrolling is therefore by PIXEL offset, not row index -
		# __OnScrollAIWeights/__LayoutAIWeightsRows work off
		# self._aiRowOffsetY (each row's top, in the virtual column) and
		# self._aiScrollPixels, showing whichever rows actually fall inside
		# the viewport instead of a fixed count of them.
		ROW_DIVIDER_Y = 0
		ROW_TITLE_Y = 12
		ROW_HELP_Y = 30
		HELP_CHARS_PER_LINE = 74
		HELP_LINE_H = 13
		GAP_HELP_CONTROL = 8
		CONTROL_H = 22
		GAP_CONTROL_CAPTION = 4
		CAPTION_H = 12
		BOTTOM_MARGIN = 14

		self._aiStartIndex = 0
		self._aiRows = []       # list of per-row widget dicts
		self._aiRowHeights = []
		self._aiLastSent = [None] * len(GM_PANEL_AI_WEIGHT_ROWS)
		self._aiKeyToIndex = {key: i for i, (key, _t, _h, _k, _mn, _mx)
				in enumerate(GM_PANEL_AI_WEIGHT_ROWS)}

		dividerText = "-" * max(10, int((listWidth - 10) / 6.2))
		cumulativeY = 0

		for i, (key, title, help, kind, minV, maxV) in enumerate(GM_PANEL_AI_WEIGHT_ROWS):
			row = {}
			helpLines = max(1, (len(help) + HELP_CHARS_PER_LINE - 1) // HELP_CHARS_PER_LINE)
			controlY = ROW_HELP_Y + helpLines * HELP_LINE_H + GAP_HELP_CONTROL
			row["controlY"] = controlY

			divider = ui.TextLine()
			divider.SetParent(self.aiWeightsSlotBoard)
			divider.SetText(dividerText)
			divider.Hide()
			self._widgets.append(divider)
			row["divider"] = divider
			row["dividerY"] = ROW_DIVIDER_Y

			titleWidget = ui.TextLine()
			titleWidget.SetParent(self.aiWeightsSlotBoard)
			titleWidget.SetText(title)
			titleWidget.Hide()
			self._widgets.append(titleWidget)
			row["title"] = titleWidget
			row["titleY"] = ROW_TITLE_Y

			valueWidget = ui.TextLine()
			valueWidget.SetParent(self.aiWeightsSlotBoard)
			valueWidget.SetText("")
			valueWidget.Hide()
			self._widgets.append(valueWidget)
			row["value"] = valueWidget

			helpWidget = ui.TextLine()
			helpWidget.SetParent(self.aiWeightsSlotBoard)
			helpWidget.SetMultiLine()
			helpWidget.SetLimitWidth(listWidth - 10)
			helpWidget.SetText(help)
			helpWidget.Hide()
			self._widgets.append(helpWidget)
			row["help"] = helpWidget
			row["helpY"] = ROW_HELP_Y

			if kind == "bool":
				toggle = ui.Button()
				toggle.SetParent(self.aiWeightsSlotBoard)
				toggle.SetSize(80, 22)
				toggle.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
				toggle.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
				toggle.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
				toggle.SetEvent(self.__MakeAIToggleHandler(i))
				toggle.Hide()
				self._widgets.append(toggle)
				row["toggle"] = toggle
				row["slider"] = None
				row["caption"] = None
				self.__SetAIToggleVisual(toggle, 1)
				# Matches the visual default set above - without this a click
				# before the first fetch reply landed (or if it never does)
				# would think it was toggling OFF, since None-or-0 reads as
				# already off, and send ON again instead.
				self._aiLastSent[i] = 1
				rowHeight = controlY + CONTROL_H + BOTTOM_MARGIN
			else:
				slider = ui.SliderBar()
				slider.SetParent(self.aiWeightsSlotBoard)
				slider.SetEvent(self.__MakeAIWeightSliderHandler(i))
				slider.Hide()
				self._widgets.append(slider)
				row["slider"] = slider
				row["toggle"] = None

				captionY = controlY + CONTROL_H + GAP_CONTROL_CAPTION
				row["captionY"] = captionY
				caption = ui.TextLine()
				caption.SetParent(self.aiWeightsSlotBoard)
				if kind == "weight":
					caption.SetText("%d - rzadko      100 - jak w grze      %d - czesto" % (minV, maxV))
				elif kind == "scrap":
					caption.SetText("%d - wylaczone      %d - kazdy straganiarz" % (minV, maxV))
				else:  # chest
					caption.SetText("%d%%      %d%%" % (minV, maxV))
				caption.Hide()
				self._widgets.append(caption)
				row["caption"] = caption
				rowHeight = captionY + CAPTION_H + BOTTOM_MARGIN

			self._aiRows.append(row)
			self._aiRowHeights.append(rowHeight)
			cumulativeY += rowHeight

		totalHeight = cumulativeY
		if totalHeight <= listHeight:
			self.aiWeightsScrollBar.Hide()
		else:
			self.aiWeightsScrollBar.SetMiddleBarSize(max(0.05, float(listHeight) / float(totalHeight)))
			self.aiWeightsScrollBar.Show()
		self._aiViewportHeight = listHeight

		self.aiWeightsStatus = self.__MakeText(page, 10, top + 20 + listHeight + 6, "")

		self.__LayoutAIWeightsRows()

	# Whole rows only, snapped to row boundaries, never a row sliced by the
	# viewport edge. Pixel-precise scrolling (an earlier version of this)
	# let a row straddle the bottom edge and render past it - this window
	# does not clip its children to its own rectangle, so that half-row hung
	# outside the list, over whatever sits below it on the page (reported
	# live: "wychodzi z tabeli", "napisy sa poza panelem"). Starting exactly
	# at self._aiStartIndex and stacking rows from y=0 until the next one
	# would no longer fully fit guarantees every visible row sits entirely
	# inside [0, viewport height].
	def __LayoutAIWeightsRows(self):
		viewportH = self._aiViewportHeight
		yPos = 0
		for i, row in enumerate(self._aiRows):
			height = self._aiRowHeights[i]
			show = (i >= self._aiStartIndex and
					(i == self._aiStartIndex or yPos + height <= viewportH))
			if not show:
				for key in ("divider", "title", "value", "help", "slider", "toggle", "caption"):
					w = row.get(key)
					if w is not None:
						w.Hide()
				continue
			row["divider"].SetPosition(2, yPos + row["dividerY"])
			row["title"].SetPosition(2, yPos + row["titleY"])
			row["value"].SetPosition(self.aiWeightsSlotBoard.GetWidth() - 90, yPos + row["titleY"])
			row["help"].SetPosition(2, yPos + row["helpY"])
			if row["toggle"] is not None:
				row["toggle"].SetPosition(2, yPos + row["controlY"])
				row["toggle"].Show()
			else:
				row["slider"].SetPosition(2, yPos + row["controlY"])
				row["slider"].Show()
				row["caption"].SetPosition(2, yPos + row["captionY"])
				row["caption"].Show()
			row["divider"].Show()
			row["title"].Show()
			row["value"].Show()
			row["help"].Show()
			yPos += height

	def __OnScrollAIWeights(self):
		rowCount = len(self._aiRows)
		scrollableRows = max(0, rowCount - 1)
		startIndex = int(round(scrollableRows * self.aiWeightsScrollBar.GetPos()))
		if startIndex != self._aiStartIndex:
			self._aiStartIndex = startIndex
			self.__LayoutAIWeightsRows()

	def __MakeAIWeightSliderHandler(self, index):
		return lambda: self.__OnAIWeightSliderMove(index)

	def __MakeAIToggleHandler(self, index):
		return lambda: self.__OnAIToggleClick(index)

	def __SetAIToggleVisual(self, toggle, value):
		if value:
			toggle.SetText("ON")
			toggle.SetTextColor(0xff00ff00)
		else:
			toggle.SetText("OFF")
			toggle.SetTextColor(0xffff0000)

	def __OnAIToggleClick(self, index):
		key, title, help, kind, minV, maxV = GM_PANEL_AI_WEIGHT_ROWS[index]
		current = self._aiLastSent[index] or 0
		newValue = 0 if current else 1
		self._aiLastSent[index] = newValue
		self.__SetAIToggleVisual(self._aiRows[index]["toggle"], newValue)
		net.SendChatPacket("/gmpanel_setaiweight %s|%d" % (key, newValue))

	# Suwak wysyla tylko gdy przesuniecie faktycznie zmienilo cala liczbe -
	# jeden suwak od konca do konca to najwyzej ~maxV-minV wywolan, nie jedno
	# na kazdy piksel przeciagniecia. CHEST/CHEST_STONE: suwak w % (1-100),
	# ale silnik (CONFIG, item_manager.cpp) rozumie promile 0-1000, wiec w
	# druta leci value*10.
	def __OnAIWeightSliderMove(self, index):
		key, title, help, kind, minV, maxV = GM_PANEL_AI_WEIGHT_ROWS[index]
		slider = self._aiRows[index]["slider"]
		pos = slider.GetSliderPos()
		value = int(round(minV + pos * (maxV - minV)))
		value = max(minV, min(maxV, value))

		if kind == "scrap":
			self._aiRows[index]["value"].SetText("%d%%" % value)
		elif kind == "chest":
			self._aiRows[index]["value"].SetText("%d%%" % value)
		else:
			self._aiRows[index]["value"].SetText(str(value))

		wireValue = value * 10 if kind == "chest" else value
		if self._aiLastSent[index] == wireValue:
			return
		self._aiLastSent[index] = wireValue
		net.SendChatPacket("/gmpanel_setaiweight %s|%d" % (key, wireValue))

	# Called from game.py's dispatcher with do_gmpanel_getaiweights's
	# response - 17 wartosci w KOLEJNOSCI SERWERA (GM_PANEL_AI_WEIGHT_
	# SERVER_ORDER), niezaleznie od kolejnosci wyswietlania na karcie.
	# -1 na CHEST/CHEST_STONE = "nieustawione, uzyj CONFIG": suwak na 1%,
	# etykieta "-" zamiast liczby, dopoki GM go nie ruszy.
	def SetAIWeightsResult(self, data):
		self._aiWeightsFetchFrames = 0
		parts = data.split("|")
		if not parts or not parts[0]:
			return
		# Klucze serwer dopisuje na koncu listy, wiec nadmiarowe pola po prostu
		# pomijamy zamiast odrzucac cala odpowiedz. Wczesniej wymagana byla
		# rowna dlugosc i jeden dopisany klucz (REST) uciszyl cala karte.
		for key, part in zip(GM_PANEL_AI_WEIGHT_SERVER_ORDER, parts):
			i = self._aiKeyToIndex.get(key)
			if i is None:
				continue
			try:
				raw = int(part)
			except ValueError:
				continue
			_key, title, help, kind, minV, maxV = GM_PANEL_AI_WEIGHT_ROWS[i]
			row = self._aiRows[i]

			if kind == "bool":
				value = 1 if raw else 0
				self._aiLastSent[i] = value
				self.__SetAIToggleVisual(row["toggle"], value)
				continue

			if raw < 0:
				row["slider"].SetSliderPos(0.0)
				row["value"].SetText("-")
				self._aiLastSent[i] = None
				continue

			value = max(1, int(round(raw / 10.0))) if kind == "chest" else raw
			value = max(minV, min(maxV, value))
			pos = 0.0 if maxV == minV else float(value - minV) / float(maxV - minV)
			row["slider"].SetSliderPos(pos)
			if kind in ("scrap", "chest"):
				row["value"].SetText("%d%%" % value)
			else:
				row["value"].SetText(str(value))
			self._aiLastSent[i] = raw

	# Called from game.py's dispatcher with do_gmpanel_setaiweight's response.
	# Quiet on OK (the slider/toggle's own live state is the feedback) - only
	# surfaces the rare error, so dragging does not spam this line.
	def SetAIWeightResult(self, data):
		if data.startswith("OK"):
			self.aiWeightsStatus.SetText("")
			return
		self.aiWeightsStatus.SetText("Blad: %s" % data)

	def __MakeSaveRateHandler(self, rateName):
		return lambda: self.__OnClickSaveRate(rateName)

	def __OnClickSaveRate(self, rateName):
		edit = self._serverctrlEdits[rateName]
		status = self._serverctrlStatus[rateName]
		value = edit.GetText().strip()

		if not value.isdigit() or not (1 <= int(value) <= 10000):
			status.SetText("Podaj liczbe 1-10000.")
			return

		self._serverctrlLastSaved = rateName
		status.SetText("Zapisuje...")
		net.SendChatPacket("/gmpanel_setrate %s|%s" % (rateName, value))

	# Called from game.py's server-command dispatcher with do_gmpanel_setrate's
	# response - "OK|<name>|<value>" on success. An error code alone does not
	# say which of the 3 fields it came from (the server doesn't echo a name
	# it never got to validate), so that case falls back to whichever field
	# was saved most recently.
	def SetRateSaveResult(self, data):
		parts = data.split("|")
		if parts[0] == "OK" and len(parts) == 3 and parts[1] in self._serverctrlStatus:
			self._serverctrlStatus[parts[1]].SetText("Zapisano (%s%%)." % parts[2])
			return

		status = self._serverctrlStatus.get(self._serverctrlLastSaved)
		if not status:
			return
		if data == "ERR_RANGE":
			status.SetText("Serwer odrzucil wartosc (1-10000).")
		elif data == "ERR_QUERY":
			status.SetText("Blad zapytania do bazy.")
		else:
			status.SetText("Blad: %s" % data)

	def __OnClickFetchRates(self):
		for status in self._serverctrlStatus.values():
			status.SetText("Wczytuje...")
		self._ratesFetchFrames = 1
		net.SendChatPacket("/gmpanel_getrates")

	def __OnClickRestartServer(self):
		self.serverctrlRestartStatus.SetText("Zglaszam restart...")
		net.SendChatPacket("/gmpanel_restartserver")

	# Called from game.py's server-command dispatcher with
	# do_gmpanel_restartserver's response.
	def SetRestartServerResult(self, data):
		if data == "OK":
			self.serverctrlRestartStatus.SetText(
					"Zgloszono - serwer sam zrestartuje core'y w kilka-kilkanascie sekund.")
		elif data == "ERR_SPOOL":
			self.serverctrlRestartStatus.SetText(
					"Blad: nie udalo sie zapisac zgloszenia restartu.")
		else:
			self.serverctrlRestartStatus.SetText("Blad: %s" % data)

	# Called from game.py's dispatcher with do_gmpanel_getrates's response -
	# "<exp>|<drop>|<yang>". Fired once automatically the first time this tab
	# is opened, and again on demand from the "Pobierz aktualne raty" button.
	def SetRatesResult(self, data):
		self._ratesFetchFrames = 0
		parts = data.split("|")
		if len(parts) != 3:
			return
		for value, (name, _attrSuffix, _label) in zip(parts, GM_PANEL_RATE_FIELDS):
			if name in self._serverctrlEdits:
				self._serverctrlEdits[name].SetText(value)
			if name in self._serverctrlStatus:
				self._serverctrlStatus[name].SetText("Wczytano.")

	def __OnClickShowGMCommands(self):
		self.__SetPage("page_gmcommands")

	def __OnClickBackFromGMCommands(self):
		self.__SetPage("page_spawnmobs")

	# Plain scrollable text list, not the click-to-pick grid every other
	# picker uses - GM_COMMANDS_LIST entries are full sentences (command +
	# description), which don't fit the picker's 175px-wide row buttons.
	# Same scrollbar-over-a-slot-board approach as PlayerbotAdminWindow's bot
	# list (__PBAMakeScrollBar/__PBARebuildBotListUI below) - a real
	# up/down slider, matching "tabelka z suwakiem gora/dol" literally,
	# since this list is informational only and never needs search/paging.
	def __BuildGMCommandsPage(self, page):
		self.__MakeText(page, 10, 6, "Lista komend GM:")

		listWidth = WINDOW_WIDTH - 20 - 30
		listHeight = CONTENT_HEIGHT - 60

		listBoard = ui.Window()
		listBoard.SetParent(page)
		listBoard.SetPosition(10, 28)
		listBoard.SetSize(listWidth + 20, listHeight)
		listBoard.Show()
		self._widgets.append(listBoard)

		self.gmCommandsScrollBar = ui.ScrollBar()
		self.gmCommandsScrollBar.SetParent(listBoard)
		self.gmCommandsScrollBar.SetPosition(listWidth, 0)
		self.gmCommandsScrollBar.SetScrollBarSize(listHeight)
		self.gmCommandsScrollBar.SetScrollEvent(ui.__mem_func__(self.__OnScrollGMCommands))
		self.gmCommandsScrollBar.Show()
		self._widgets.append(self.gmCommandsScrollBar)

		self.gmCommandsSlotBoard = ui.Window()
		self.gmCommandsSlotBoard.SetParent(listBoard)
		self.gmCommandsSlotBoard.SetPosition(0, 0)
		self.gmCommandsSlotBoard.SetSize(listWidth, listHeight)
		self.gmCommandsSlotBoard.Show()
		self._widgets.append(self.gmCommandsSlotBoard)

		self._gmCommandsRowHeight = 14
		self._gmCommandsStartLine = 0
		self._gmCommandsVisibleRows = max(1, listHeight // self._gmCommandsRowHeight)
		self._gmCommandsLines = []
		for text in GM_COMMANDS_LIST:
			line = ui.TextLine()
			line.SetParent(self.gmCommandsSlotBoard)
			line.SetPosition(2, 0)
			line.SetText(text)
			line.Hide()
			self._widgets.append(line)
			self._gmCommandsLines.append(line)

		rowCount = len(self._gmCommandsLines)
		if rowCount <= self._gmCommandsVisibleRows:
			self.gmCommandsScrollBar.Hide()
		else:
			self.gmCommandsScrollBar.SetMiddleBarSize(float(self._gmCommandsVisibleRows) / float(rowCount))
			self.gmCommandsScrollBar.Show()

		backButton = ui.Button()
		backButton.SetParent(page)
		backButton.SetPosition(10, listHeight + 34)
		backButton.SetSize(80, 22)
		backButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		backButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		backButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		backButton.SetText("Wroc")
		backButton.SetEvent(self.__OnClickBackFromGMCommands)
		backButton.Show()
		self._widgets.append(backButton)

		self.__LayoutGMCommandsRows()

	def __LayoutGMCommandsRows(self):
		yPos = 0
		for i, line in enumerate(self._gmCommandsLines):
			if i < self._gmCommandsStartLine or i >= self._gmCommandsStartLine + self._gmCommandsVisibleRows:
				line.Hide()
				continue
			line.SetPosition(2, yPos)
			line.Show()
			yPos += self._gmCommandsRowHeight

	def __OnScrollGMCommands(self):
		rowCount = len(self._gmCommandsLines)
		scrollableRows = max(0, rowCount - self._gmCommandsVisibleRows)
		startLine = int(scrollableRows * self.gmCommandsScrollBar.GetPos())
		if startLine != self._gmCommandsStartLine:
			self._gmCommandsStartLine = startLine
			self.__LayoutGMCommandsRows()

	######################################################################
	## "Sprawdz Gracza" - akcje GM (Daj Yang/Smocze Monety, Zmien punkty
	## konne/range, Dodaj statystyki, Zmien skille). Wspolny wzorzec: kazdy
	## przycisk na page_lookup zapamietuje tryb (self._giveAmountMode /
	## self._setValueMode) i przechodzi na jedna z dwoch reuzywanych
	## pod-stron (page_giveamount, page_setvalue) zamiast budowac 4 prawie
	## identyczne strony osobno.
	######################################################################

	def __BuildGiveAmountPage(self, page):
		self.__MakeText(page, 10, 6, "Nick:")
		self.giveAmountNickText = self.__MakeText(page, 60, 6, "-")

		self.giveAmountTitleText = self.__MakeText(page, 10, 34, "Kwota:")
		self.giveAmountEdit = self.__MakeEdit(page, 110, 30, 120, 10)

		giveButton = ui.Button()
		giveButton.SetParent(page)
		giveButton.SetPosition(240, 28)
		giveButton.SetSize(80, 22)
		giveButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		giveButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		giveButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		giveButton.SetText("Daj")
		giveButton.SetEvent(self.__OnClickGiveAmount)
		giveButton.Show()
		self._widgets.append(giveButton)

		cancelButton = ui.Button()
		cancelButton.SetParent(page)
		cancelButton.SetPosition(330, 28)
		cancelButton.SetSize(80, 22)
		cancelButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		cancelButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		cancelButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		cancelButton.SetText("Anuluj")
		cancelButton.SetEvent(lambda: self.__SetPage("page_lookup"))
		cancelButton.Show()
		self._widgets.append(cancelButton)

		self.giveAmountStatus = self.__MakeText(page, 10, 64, "")
		self._giveAmountMode = "gold"

	def __OpenGiveAmount(self, mode):
		if not self._lookupNick:
			return
		self._giveAmountMode = mode
		self.giveAmountNickText.SetText(self._lookupNick)
		self.giveAmountTitleText.SetText("Kwota Yang:" if mode == "gold" else "Kwota SM:")
		self.giveAmountEdit.SetText("")
		self.giveAmountStatus.SetText("")
		self.__SetPage("page_giveamount")

	def __OnClickGiveAmount(self):
		amount = self.giveAmountEdit.GetText().strip()
		if not self._lookupNick or not amount:
			self.giveAmountStatus.SetText("Podaj kwote.")
			return
		self.giveAmountStatus.SetText("Wysylam...")
		cmd = "gmpanel_give_gold" if self._giveAmountMode == "gold" else "gmpanel_give_cash"
		net.SendChatPacket("/%s %s" % (cmd, "|".join([self._lookupNick, amount])))

	# Called from game.py's dispatcher with do_gmpanel_give_gold's response.
	def SetGiveGoldResult(self, data):
		if data.startswith("OK_OFFLINE"):
			self.giveAmountStatus.SetText("OK (gracz offline, zapisano w bazie).")
		elif data.startswith("OK|"):
			newGold = data.split("|")[1]
			self.giveAmountStatus.SetText("OK. Nowy stan Yang: %s" % newGold)
			if self.lookupValues["lk_name"].GetText() == self._lookupNick:
				self.lookupValues["lk_gold"].SetText(newGold)
		elif data == "ERR_BADDATA":
			self.giveAmountStatus.SetText("Blad danych - sprawdz kwote.")
		elif data == "ERR_NOTFOUND":
			self.giveAmountStatus.SetText("Nie znaleziono gracza.")
		else:
			self.giveAmountStatus.SetText("Blad: %s" % data)

	# Called from game.py's dispatcher with do_gmpanel_give_cash's response.
	def SetGiveCashResult(self, data):
		if data == "OK":
			self.giveAmountStatus.SetText("OK. Smocze Monety dodane.")
		elif data == "ERR_BADDATA":
			self.giveAmountStatus.SetText("Blad danych - sprawdz kwote.")
		elif data == "ERR_NOTFOUND":
			self.giveAmountStatus.SetText("Nie znaleziono gracza.")
		else:
			self.giveAmountStatus.SetText("Blad: %s" % data)

	def __BuildSetValuePage(self, page):
		self.__MakeText(page, 10, 6, "Nick:")
		self.setValueNickText = self.__MakeText(page, 60, 6, "-")

		self.setValueTitleText = self.__MakeText(page, 10, 34, "Nowa wartosc:")
		self.setValueEdit = self.__MakeEdit(page, 130, 30, 100, 10)

		saveButton = ui.Button()
		saveButton.SetParent(page)
		saveButton.SetPosition(240, 28)
		saveButton.SetSize(80, 22)
		saveButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		saveButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		saveButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		saveButton.SetText("Zapisz")
		saveButton.SetEvent(self.__OnClickSetValue)
		saveButton.Show()
		self._widgets.append(saveButton)

		cancelButton = ui.Button()
		cancelButton.SetParent(page)
		cancelButton.SetPosition(330, 28)
		cancelButton.SetSize(80, 22)
		cancelButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		cancelButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		cancelButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		cancelButton.SetText("Anuluj")
		cancelButton.SetEvent(lambda: self.__SetPage("page_lookup"))
		cancelButton.Show()
		self._widgets.append(cancelButton)

		self.setValueStatus = self.__MakeText(page, 10, 64, "")
		self._setValueMode = "horse"

	def __OpenSetValue(self, mode):
		if not self._lookupNick:
			return
		self._setValueMode = mode
		self.setValueNickText.SetText(self._lookupNick)
		if mode == "horse":
			self.setValueTitleText.SetText("Nowy poziom konia (0-30):")
			current = self.lookupValues["lk_horselvl"].GetText()
		else:
			self.setValueTitleText.SetText("Nowe punkty range:")
			current = self.lookupValues["lk_range"].GetText()
		self.setValueEdit.SetText(current if current != "-" else "")
		self.setValueStatus.SetText("")
		self.__SetPage("page_setvalue")

	def __OnClickSetValue(self):
		value = self.setValueEdit.GetText().strip()
		if not self._lookupNick or not value:
			self.setValueStatus.SetText("Podaj wartosc.")
			return
		self.setValueStatus.SetText("Wysylam...")
		cmd = "gmpanel_set_horse_points" if self._setValueMode == "horse" else "gmpanel_set_range"
		net.SendChatPacket("/%s %s" % (cmd, "|".join([self._lookupNick, value])))

	# Called from game.py's dispatcher with do_gmpanel_set_horse_points's response.
	def SetHorsePointsResult(self, data):
		if data.startswith("OK_OFFLINE|") or data.startswith("OK|"):
			value = data.split("|")[1]
			self.setValueStatus.SetText("OK. Poziom konia: %s" % value)
			if self.lookupValues["lk_name"].GetText() == self._lookupNick:
				self.lookupValues["lk_horselvl"].SetText(value)
		elif data == "ERR_BADDATA":
			self.setValueStatus.SetText("Blad danych - sprawdz wartosc.")
		elif data == "ERR_NOTFOUND":
			self.setValueStatus.SetText("Nie znaleziono gracza.")
		else:
			self.setValueStatus.SetText("Blad: %s" % data)

	# Called from game.py's dispatcher with do_gmpanel_set_range's response.
	def SetRangeResult(self, data):
		if data.startswith("OK_OFFLINE|") or data.startswith("OK|"):
			value = data.split("|")[1]
			self.setValueStatus.SetText("OK. Punkty range: %s" % value)
			if self.lookupValues["lk_name"].GetText() == self._lookupNick:
				self.lookupValues["lk_range"].SetText(value)
		elif data == "ERR_BADDATA":
			self.setValueStatus.SetText("Blad danych - sprawdz wartosc.")
		elif data == "ERR_NOTFOUND":
			self.setValueStatus.SetText("Nie znaleziono gracza.")
		else:
			self.setValueStatus.SetText("Blad: %s" % data)

	def __BuildSetStatPage(self, page):
		self.__MakeText(page, 10, 6, "Nick:")
		self.setStatNickText = self.__MakeText(page, 60, 6, "-")

		self.__MakeText(page, 10, 34, "Statystyka:")
		statItems = [
			("st", "Sila"),
			("ht", "Wytrzymalosc"),
			("dx", "Zrecznosc"),
			("iq", "Inteligencja"),
		]
		self.__MakePickerField(page, 130, 30, 150, "setstat_stat", "static",
				staticItems=statItems, defaultValue="st", defaultLabel="Sila")

		self.__MakeText(page, 10, 62, "Nowa wartosc:")
		self.setStatEdit = self.__MakeEdit(page, 130, 58, 100, 6)
		self.__MakeText(page, 240, 62, "(Max 30.000)")

		saveButton = ui.Button()
		saveButton.SetParent(page)
		saveButton.SetPosition(10, 90)
		saveButton.SetSize(100, 22)
		saveButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		saveButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		saveButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		saveButton.SetText("Zapisz")
		saveButton.SetEvent(self.__OnClickSetStat)
		saveButton.Show()
		self._widgets.append(saveButton)

		cancelButton = ui.Button()
		cancelButton.SetParent(page)
		cancelButton.SetPosition(120, 90)
		cancelButton.SetSize(100, 22)
		cancelButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		cancelButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		cancelButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		cancelButton.SetText("Anuluj")
		cancelButton.SetEvent(lambda: self.__SetPage("page_lookup"))
		cancelButton.Show()
		self._widgets.append(cancelButton)

		self.setStatStatus = self.__MakeText(page, 10, 120, "")

	def __OpenSetStat(self):
		if not self._lookupNick:
			return
		self.setStatNickText.SetText(self._lookupNick)
		self.setStatEdit.SetText("")
		self.setStatStatus.SetText("")
		self.__SetPage("page_setstat")

	def __OnClickSetStat(self):
		stat = self.pickerFields["setstat_stat"]["value"]
		value = self.setStatEdit.GetText().strip()
		if not self._lookupNick or not value:
			self.setStatStatus.SetText("Podaj wartosc.")
			return
		self.setStatStatus.SetText("Wysylam...")
		net.SendChatPacket("/gmpanel_set_stat %s" % "|".join([self._lookupNick, stat, value]))

	# Called from game.py's dispatcher with do_gmpanel_set_stat's response.
	def SetStatResult(self, data):
		if data.startswith("OK_OFFLINE|") or data.startswith("OK|"):
			parts = data.split("|")
			stat, value = parts[1], parts[2]
			self.setStatStatus.SetText("OK. %s = %s" % (stat, value))
			fieldName = {"st": "lk_st", "ht": "lk_ht", "dx": "lk_dx", "iq": "lk_iq"}.get(stat)
			if fieldName and self.lookupValues["lk_name"].GetText() == self._lookupNick:
				self.lookupValues[fieldName].SetText(value)
		elif data == "ERR_BADSTAT":
			self.setStatStatus.SetText("Nieznana statystyka.")
		elif data == "ERR_BADDATA":
			self.setStatStatus.SetText("Blad danych - sprawdz wartosc.")
		elif data == "ERR_NOTFOUND":
			self.setStatStatus.SetText("Nie znaleziono gracza.")
		else:
			self.setStatStatus.SetText("Blad: %s" % data)

	def __BuildSkillsPage(self, page):
		self.__MakeText(page, 10, 6, "Nick:")
		self.skillsNickText = self.__MakeText(page, 60, 6, "-")

		backButton = ui.Button()
		backButton.SetParent(page)
		backButton.SetPosition(WINDOW_WIDTH - 20 - 100, 2)
		backButton.SetSize(100, 20)
		backButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		backButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		backButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		backButton.SetText("Powrot")
		backButton.SetEvent(lambda: self.__SetPage("page_lookup"))
		backButton.Show()
		self._widgets.append(backButton)

		self.skillRows = []
		rowY0 = 34
		rowH = 26
		for i in range(10):
			y = rowY0 + i * rowH
			nameText = self.__MakeText(page, 10, y, "")
			editLine = self.__MakeEdit(page, 300, y - 3, 60, 3)

			saveBtn = ui.Button()
			saveBtn.SetParent(page)
			saveBtn.SetPosition(370, y - 4)
			saveBtn.SetSize(70, 20)
			saveBtn.SetUpVisual("d:/ymir work/ui/public/small_button_01.sub")
			saveBtn.SetOverVisual("d:/ymir work/ui/public/small_button_02.sub")
			saveBtn.SetDownVisual("d:/ymir work/ui/public/small_button_03.sub")
			saveBtn.SetText("Zapisz")
			saveBtn.SetEvent(self.__MakeSaveSkillHandler(i))
			saveBtn.Hide()
			self._widgets.append(saveBtn)

			rowStatus = self.__MakeText(page, 445, y, "")

			self.skillRows.append({
				"vnum": None, "nameText": nameText, "edit": editLine,
				"button": saveBtn, "status": rowStatus,
			})

		self.skillsStatus = self.__MakeText(page, 10, rowY0 + 10 * rowH + 6, "")

	def __MakeSaveSkillHandler(self, index):
		return lambda: self.__OnClickSaveSkill(index)

	def __OpenSkills(self):
		if not self._lookupNick:
			return
		self.skillsNickText.SetText(self._lookupNick)
		self.__PopulateSkillRows()
		self.__SetPage("page_skills")

	# Poziom skilla <-> notacja: 1-19 zwykle liczby, 20-29 = M1-M10 (n=level-19),
	# 30-39 = G1-G10 (n=level-29), 40 = P. Podane wprost przez uzytkownika
	# (M5=24, G1=30, P=40) - nie zgadywane.
	def __LevelToGrade(self, level):
		try:
			level = int(level)
		except (TypeError, ValueError):
			return str(level)
		if level <= 19:
			return str(level)
		if level <= 29:
			return "M%d" % (level - 19)
		if level <= 39:
			return "G%d" % (level - 29)
		if level == 40:
			return "P"
		return str(level)

	def __GradeToLevel(self, text):
		text = text.strip().upper()
		if not text:
			return None
		if text == "P":
			return 40
		if text[0] in ("M", "G") and text[1:].isdigit():
			n = int(text[1:])
			if 1 <= n <= 10:
				return (19 if text[0] == "M" else 29) + n
			return None
		if text.isdigit():
			return int(text)
		return None

	def __PopulateSkillRows(self):
		skills = self._lookupSkills
		for i, row in enumerate(self.skillRows):
			if i < len(skills):
				vnum, name, level, maxLevel = skills[i]
				row["vnum"] = vnum
				row["nameText"].SetText("%s (obecnie %s/%s):" % (
						name, self.__LevelToGrade(level), self.__LevelToGrade(maxLevel)))
				row["edit"].SetText(self.__LevelToGrade(level))
				row["status"].SetText("")
				row["button"].Show()
			else:
				row["vnum"] = None
				row["nameText"].SetText("")
				row["edit"].SetText("")
				row["status"].SetText("")
				row["button"].Hide()
		self.skillsStatus.SetText("")

	def __OnClickSaveSkill(self, index):
		row = self.skillRows[index]
		if row["vnum"] is None or not self._lookupNick:
			return
		text = row["edit"].GetText().strip()
		if not text:
			row["status"].SetText("Podaj poziom.")
			return
		level = self.__GradeToLevel(text)
		if level is None:
			row["status"].SetText("Zly format (np. 18, M5, G1, P).")
			return
		row["status"].SetText("Wysylam...")
		self.skillsStatus.SetText("")
		net.SendChatPacket("/gmpanel_setskill %s" % "|".join([self._lookupNick, row["vnum"], str(level)]))

	# Called from game.py's dispatcher with do_gmpanel_setskill's response.
	def SetSetSkillResult(self, data):
		if data.startswith("OK|"):
			parts = data.split("|")
			vnum, level = parts[1], parts[2]
			for row in self.skillRows:
				if row["vnum"] == vnum:
					row["status"].SetText("OK (%s)" % self.__LevelToGrade(level))
					row["edit"].SetText(self.__LevelToGrade(level))
					break
			self.skillsStatus.SetText("Zapisano.")
		elif data == "ERR_OFFLINE":
			self.skillsStatus.SetText("Gracz musi byc online.")
		elif data == "ERR_BADSKILL":
			self.skillsStatus.SetText("Nieznany skill.")
		elif data == "ERR_BADDATA":
			self.skillsStatus.SetText("Blad danych.")
		else:
			self.skillsStatus.SetText("Blad: %s" % data)

## ============================================================================
## PlayerbotAdminWindow / BotOverheadTail - dawniej osobny plik
## root/uiPlayerbotAdmin.py, teraz scalone bezposrednio tutaj z tego samego
## powodu co GMPanelWindow nigdy nie uzywa pliku uiscript: EPack32 "Add file"
## na uiscript.epk (dodanie playerbotadminwindow.py) nadpisalo cale archiwum
## PUSTYM, kasujac wszystkie inne pliki (PopupDialog.py i reszta) - klient nie
## startowal w ogole. Po przywroceniu uiscript.epk z backupu i przepisaniu
## PlayerbotAdminWindow na czysty Python (bez uiscript), okazalo sie ze sam
## NOWY plik root/uiPlayerbotAdmin.py tez nigdy nie trafil do root.epk (EPack32
## "Save" odswieza tylko already-tracked pliki, nigdy nie dodaje nowych same z
## siebie) - "ImportError: No module named uiPlayerbotAdmin" przy wyborze
## postaci. Scalajac ten kod do interfacemodule.py (ktory JUZ jest w root.epk
## i regularnie edytowany caly ten sesje) znika potrzeba jakiegokolwiek "Add
## file" - zwykly Save wystarczy, tak jak GMPanelWindow od poczatku.
##
## Stale ponizej maja prefiks PBA_ zeby nie kolidowaly z WINDOW_WIDTH/TAB_Y/
## CONTENT_Y/CONTENT_HEIGHT itp. juz zdefiniowanymi wyzej dla GMPanelWindow
## (inny rozmiar okna - 940x680 zamiast 560x460).
## ============================================================================

PBA_WINDOW_WIDTH = 940
PBA_WINDOW_HEIGHT = 680

PBA_TAB_Y = 40
PBA_TAB_HEIGHT = 20
PBA_CONTENT_X = 15
PBA_CONTENT_Y = 75
PBA_CONTENT_HEIGHT = PBA_WINDOW_HEIGHT - PBA_CONTENT_Y - 45

PBA_LIST_WIDTH = 230
PBA_RIGHT_X = PBA_CONTENT_X + PBA_LIST_WIDTH + 15
PBA_RIGHT_WIDTH = PBA_WINDOW_WIDTH - PBA_RIGHT_X - 15

PBA_ROW_HEIGHT = 20
PBA_LOG_LINE_HEIGHT = 16
PBA_LIST_ROW_WIDTH = 190
PBA_LIST_ROW_HEIGHT = 18
PBA_LIST_ROW_NAME_MAXLEN = 11

# "Czat ogolny" nie renderuje tekstu w silniku gry w ogole - laduje prawdziwa
# strone panelu w ten sam wbudowany silnik przegladarki co Item Mall
# (app.ShowWebPage, patrz root/uiweb.py). 127.0.0.1 dziala bo to serwer
# singleplayer, klient i panel siedza na tej samej maszynie.
PBA_BOTCHAT_URL = "http://127.0.0.1:7788/botchat"

PBA_GLOBAL_CHAT_RECT_X = PBA_CONTENT_X
PBA_GLOBAL_CHAT_RECT_Y = PBA_CONTENT_Y
PBA_GLOBAL_CHAT_RECT_WIDTH = PBA_WINDOW_WIDTH - PBA_CONTENT_X * 2 - 35
PBA_GLOBAL_CHAT_RECT_HEIGHT = PBA_CONTENT_HEIGHT - 45

PBA_LIVE_LOG_BOARD_HEIGHT = PBA_CONTENT_HEIGHT - 80

# Etykiety osiagniec PO POLSKU - musza miec te same id co
# s_playerBotAchievementDefs w playerbot_manager.cpp. Nazwy NIGDY nie leca
# przez siec (prosty parser komend klienta nie znosi spacji w argumentach),
# stad ta tablica zamiast czytania nazwy z pakietu.
PBA_ACHIEVEMENT_LABELS = {
	1 : "Pierwszy 30 poziom",
	2 : "Pierwszy 60 poziom",
	3 : "Pierwszy 90 poziom",
}
PBA_ACHIEVEMENT_ORDER = [1, 2, 3]

# Ile klatek bez aktualizacji zanim uznajemy ze bot zniknal/wyszedl z zasiegu
# i chowamy jego dymek na stale - patrz BotOverheadTail.OnUpdate.
PBA_OVERHEAD_TIMEOUT_TICKS = 400

# Wysokosc (w jednostkach swiata, nad stopami postaci) na ktorej
# chr.GetProjectPosition rzutuje punkt na ekran - 220 to ta sama wartosc co
# juz uzywa uiprivateshopbuilder.py (PrivateShopAdvertisementBoard) dla szyldu
# straganu nad glowa DOWOLNEGO gracza.
PBA_OVERHEAD_HEAD_HEIGHT = 220


class BotOverheadTail(ui.ThinBoard):
	"""Maly panel nad glowa bota (do 5 linii), pozycjonowany co klatke przez
	chr.GetProjectPosition(vid, PBA_OVERHEAD_HEAD_HEIGHT) - ten sam mechanizm
	i baza klasy (ui.ThinBoard) co PrivateShopAdvertisementBoard w
	uiprivateshopbuilder.py, ktora robi dokladnie to samo (szyld nad glowa
	DOWOLNEGO gracza z otwartym straganem)."""

	MAX_LINES = 5
	LINE_HEIGHT = 14

	def __init__(self):
		ui.ThinBoard.__init__(self, "UI_BOTTOM")
		self.vid = None
		self.aliveTicks = 0
		self.textLines = []
		for i in xrange(BotOverheadTail.MAX_LINES):
			line = ui.TextLine()
			line.SetParent(self)
			line.SetHorizontalAlignLeft()
			line.SetPosition(6, 4 + i * BotOverheadTail.LINE_HEIGHT)
			self.textLines.append(line)

	def __del__(self):
		ui.ThinBoard.__del__(self)

	def Open(self, vid, lines):
		self.vid = vid
		self.SetLines(lines)
		self.Show()

	def SetLines(self, lines):
		count = min(len(lines), BotOverheadTail.MAX_LINES)
		maxLen = 1
		for i in xrange(BotOverheadTail.MAX_LINES):
			if i < count:
				self.textLines[i].SetText(lines[i])
				self.textLines[i].Show()
				maxLen = max(maxLen, len(lines[i]))
			else:
				self.textLines[i].Hide()
		self.SetSize(maxLen * 6 + 20, 8 + count * BotOverheadTail.LINE_HEIGHT)
		self.aliveTicks = 0

	def OnUpdate(self):
		if self.vid is None:
			return
		self.aliveTicks += 1
		if self.aliveTicks > PBA_OVERHEAD_TIMEOUT_TICKS:
			self.vid = None
			self.Hide()
			return
		try:
			x, y = chr.GetProjectPosition(self.vid, PBA_OVERHEAD_HEAD_HEIGHT)
		except:
			self.vid = None
			self.Hide()
			return
		self.SetPosition(int(x - self.GetWidth() / 2), int(y - self.GetHeight()))

	def Destroy(self):
		self.vid = None
		self.Hide()


TOP1_BADGE_HEAD_HEIGHT = 220
# Server resends every 5s (top1_badge_event, playerbot_manager.cpp) - stay
# hidden-after well past that so a single dropped packet doesn't blink it.
TOP1_BADGE_TIMEOUT_TICKS = 450

class Top1Badge(ui.ThinBoard):
	"""Maly, zawsze widoczny (nie GM-only) szyld 'Top1' nad glowa postaci z
	najwyzszym poziomem na serwerze w tej chwili - ten sam mechanizm co
	BotOverheadTail powyzej i PrivateShopAdvertisementBoard
	(uiprivateshopbuilder.py): pozycjonowanie co klatke przez
	chr.GetProjectPosition. Serwer przelicza mistrza co 5 sekund i rozsyla
	zwykly chat-command "Top1Badge <vid>" do wszystkich w poblizu - kazdy
	klient go odbiera, nie tylko GM.

	Byla tu proba prawdziwego efektu czastkowego (TOP1.mse przez
	chrmgr.RegisterEffect/SetAffect, ten sam mechanizm co znak GM) - jedyne
	uzycie chrmgr.SetAffect w calym kliencie (konsola deweloperska) zawsze
	woa -1 (wlasna postac), zaden kod nigdzie nie uzywa go na cudzym vid.
	Prawdopodobnie celowo ograniczone do wlasnej postaci (zabezpieczenie
	przed fal­szowaniem stanu innych graczy lokalnie) - efekty widoczne na
	INNYCH graczach normalnie pochodza z synchronizacji przez serwer, nie z
	lokalnego wywolania. Zrobienie tego "na prawdziwych zasadach" wymagaloby
	zmian w protokole sieciowym serwera.

	__init__ wraca tu do sprawdzonego tekstu (biegnie bezwarunkowo dla kazdego
	gracza - nic ryzykownego tu nie ma prawa byc). Ikona (TOP1.tga) jest
	proba numer trzy, ale doczepiona leniwie w Refresh() - pierwszy raz, gdy
	odezwie sie serwer, nie przy starcie gry - i owinieta w try/except, zeby
	nieudane wczytanie nie ubilo calego OnTop1Badge (a co za tym idzie -
	obsluge kolejnych komend). To NIE chroni przed crashem silnika (poza
	Pythonem), tylko przed bledem Pythona - jesli obrazek znow padnie, moze
	wywalic tylko tego klienta, ktory akurat dostal odswiezenie, a nie kazdego
	przy logowaniu jak poprzednio."""

	def __init__(self):
		ui.ThinBoard.__init__(self, "UI_BOTTOM")
		self.vid = None
		self.aliveTicks = 0
		self.icon = None
		self._iconLoadAttempted = False
		self.textLine = ui.TextLine()
		self.textLine.SetParent(self)
		self.textLine.SetWindowHorizontalAlignCenter()
		self.textLine.SetWindowVerticalAlignCenter()
		self.textLine.SetHorizontalAlignCenter()
		self.textLine.SetVerticalAlignCenter()
		self.textLine.SetText("Top1")
		self.textLine.SetOutline(True)
		self.textLine.SetFontColor(1.0, 0.84, 0.0)
		self.textLine.Show()
		self.SetSize(60, 20)

	def __del__(self):
		ui.ThinBoard.__del__(self)

	def __TryLoadIcon(self):
		self._iconLoadAttempted = True
		# ImageBox.LoadImage does not resolve locale/... paths (only
		# chrmgr.RegisterEffect does, for .mse) - confirmed live, w=0 h=0,
		# no exception. Every working ImageBox.LoadImage call in this client
		# uses a "d:/ymir work/..." path instead (e.g. the m2sp_logo.tga
		# above, physically under pack/ETC/ymir work/ui/public/) - TOP1.tga
		# needs to live there too, not under locale_pl.
		path = "d:/ymir work/ui/public/TOP1.tga"
		try:
			icon = ui.ImageBox()
			icon.SetParent(self)
			icon.LoadImage(path)
			icon.SetPosition(0, 0)
			width = icon.GetWidth()
			height = icon.GetHeight()
			if width <= 0 or height <= 0:
				return
			icon.Show()
			self.icon = icon
			self.textLine.Hide()
			self.SetSize(width, height)
		except Exception, e:
			self.icon = None

	def Refresh(self, vid):
		self.vid = vid
		self.aliveTicks = 0
		if not self._iconLoadAttempted:
			self.__TryLoadIcon()
		self.Show()

	def OnUpdate(self):
		if self.vid is None:
			return
		self.aliveTicks += 1
		if self.aliveTicks > TOP1_BADGE_TIMEOUT_TICKS:
			self.vid = None
			self.Hide()
			return
		try:
			x, y = chr.GetProjectPosition(int(self.vid), TOP1_BADGE_HEAD_HEIGHT)
		except:
			self.vid = None
			self.Hide()
			return
		self.SetPosition(int(x - self.GetWidth() / 2), int(y - self.GetHeight()))

	def Destroy(self):
		self.vid = None
		self.Hide()


class PlayerbotAdminWindow(ui.BoardWithTitleBar):

	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)

		self.activeTab = "general"
		self.selectedPid = 0
		self.selectedEmpire = 0

		# Widgets built by pure Python code have no other Python reference
		# once the local variable that created them goes out of scope -
		# without this, refcounting GC destroys the native widget right
		# after Show() (see the same comment on GMPanelWindow._widgets above).
		self._widgets = []

		self.botRows = []          # [(pid, level, empire, x, y, name), ...]
		self.botRowButtons = []    # ui.Button widgets, jeden na wiersz listy
		self.botListStartLine = 0

		self.logPid = 0
		self.logLines = []
		self.logTextLines = []     # ui.TextLine widgets
		self.logStartLine = 0
		self.logScrollableCount = 0

		self.globalChatWebOpen = False
		self.globalChatWebLastPos = None

		self.overheadBoards = {}  # vid -> BotOverheadTail

		self.achievementRows = {}  # id -> (pid, name)
		self.achievementTextLines = []

		# LoadWindow() is called explicitly by __MakePlayerbotAdminWindow
		# right after construction - NOT called here too, or every widget
		# below would be built twice.

	def __del__(self):
		ui.BoardWithTitleBar.__del__(self)

	######################################################################
	## Budowa okna - w calosci w Pythonie (patrz komentarz nad ta sekcja).
	######################################################################

	def __PBAMakeBoard(self, parent, x, y, w, h):
		board = ui.Window()
		board.SetParent(parent)
		board.SetPosition(x, y)
		board.SetSize(w, h)
		board.Show()
		self._widgets.append(board)
		return board

	def __PBAMakeText(self, parent, x, y, text=""):
		line = ui.TextLine()
		line.SetParent(parent)
		line.SetPosition(x, y)
		line.SetText(text)
		line.Show()
		self._widgets.append(line)
		return line

	def __PBAMakeButton(self, parent, x, y, w, h, text, size="middle"):
		prefix = "middle_button" if size == "middle" else "small_button"
		button = ui.Button()
		button.SetParent(parent)
		button.SetPosition(x, y)
		button.SetSize(w, h)
		button.SetUpVisual("d:/ymir work/ui/public/%s_01.sub" % prefix)
		button.SetOverVisual("d:/ymir work/ui/public/%s_02.sub" % prefix)
		button.SetDownVisual("d:/ymir work/ui/public/%s_03.sub" % prefix)
		button.SetText(text)
		button.Show()
		self._widgets.append(button)
		return button

	def __PBAMakeEdit(self, parent, x, y, width, maxlen=8):
		slot = ui.SlotBar()
		slot.SetParent(parent)
		slot.SetSize(width, 20)
		slot.SetPosition(x, y)
		slot.Show()
		self._widgets.append(slot)

		editLine = ui.EditLine()
		editLine.SetParent(slot)
		editLine.SetPosition(3, 3)
		editLine.SetSize(width - 6, 17)
		editLine.SetMax(maxlen)
		editLine.Show()
		self._widgets.append(editLine)
		return editLine

	def __PBAMakeScrollBar(self, parent, x, y, height):
		scrollBar = ui.ScrollBar()
		scrollBar.SetParent(parent)
		scrollBar.SetPosition(x, y)
		scrollBar.SetScrollBarSize(height)
		scrollBar.Show()
		self._widgets.append(scrollBar)
		return scrollBar

	def LoadWindow(self):
		self.SetSize(PBA_WINDOW_WIDTH, PBA_WINDOW_HEIGHT)
		self.SetCenterPosition()
		self.AddFlag("movable")
		self.AddFlag("float")
		self.SetTitleName("Zarzadzanie Botami [GM]")
		self.SetCloseEvent(self.Close)

		tabDefs = (
			("general",       15,  110, "Ogolne"),
			# "Czat ogolny" wycieta: jej jedyna trescia byla strona /botchat z panelu
			# autora, ktorej ten panel nie serwuje - zakladka mogla pokazac wylacznie
			# pustke, a wbudowana przegladarka zamykala klientowi gre.
			("live",          260, 125, "Akcje botow"),
			("manage",        390, 125, "Zarzadzanie"),
			("achievements",  520, 110, "Osiagniecia"),
		)
		tabButtons = {}
		for tabName, x, w, text in tabDefs:
			button = self.__PBAMakeButton(self, x, PBA_TAB_Y, w, PBA_TAB_HEIGHT, text)
			button.SetEvent(ui.__mem_func__(self.OnClickTab), tabName)
			tabButtons[tabName] = button
		self.tabGeneral = tabButtons["general"]
		self.tabLive = tabButtons["live"]
		self.tabManage = tabButtons["manage"]
		self.tabAchievements = tabButtons["achievements"]

		## Lista botow (wspolna dla zakladek "Akcje na zywo" i "Zarzadzanie")
		self.botListPanel = self.__PBAMakeBoard(self, PBA_CONTENT_X, PBA_CONTENT_Y, PBA_LIST_WIDTH, PBA_CONTENT_HEIGHT)
		self.__PBAMakeText(self.botListPanel, 5, 5, "Aktywne boty")
		self.botListScrollBar = self.__PBAMakeScrollBar(self.botListPanel, PBA_LIST_WIDTH - 25, 25, PBA_CONTENT_HEIGHT - 30)
		self.botListScrollBar.SetScrollEvent(ui.__mem_func__(self.OnScrollBotList))
		self.botListSlotBoard = self.__PBAMakeBoard(self.botListPanel, 5, 25, PBA_LIST_WIDTH - 30, PBA_CONTENT_HEIGHT - 30)

		## Zakladka: Ogolne
		self.generalPage = self.__PBAMakeBoard(self, PBA_CONTENT_X, PBA_CONTENT_Y, PBA_WINDOW_WIDTH - PBA_CONTENT_X * 2, PBA_CONTENT_HEIGHT)
		self.generalTotalText = self.__PBAMakeText(self.generalPage, 10, 15, "Aktywne boty: -")
		self.generalInPartyText = self.__PBAMakeText(self.generalPage, 10, 40, "W party: -")
		self.generalStallsText = self.__PBAMakeText(self.generalPage, 10, 65, "Prowadza stragan: -")
		self.generalRefreshButton = self.__PBAMakeButton(self.generalPage, 10, 100, 90, 20, "Odswiez", "small")
		self.generalRefreshButton.SetEvent(ui.__mem_func__(self.RequestStats))

		## Zakladka: Czat ogolny (polaczony strumien wszystkich botow, pelna szerokosc)
		self.globalChatPage = self.__PBAMakeBoard(self, PBA_CONTENT_X, PBA_CONTENT_Y, PBA_WINDOW_WIDTH - PBA_CONTENT_X * 2, PBA_CONTENT_HEIGHT)
		self.globalChatScrollBar = self.__PBAMakeScrollBar(self.globalChatPage, PBA_WINDOW_WIDTH - PBA_CONTENT_X * 2 - 20, 0, PBA_CONTENT_HEIGHT - 45)
		## Prawdziwa strona ma wlasne przewijanie - ten pasek jest tu tylko
		## dlatego, ze bylo prosciej zostawic go zbudowany niz go stad wywalac.
		## Nigdy pokazywany.
		self.globalChatScrollBar.Hide()
		self.globalChatRefreshButton = self.__PBAMakeButton(self.globalChatPage, 10, PBA_CONTENT_HEIGHT - 35, 90, 20, "Odswiez", "small")
		## Strona i tak odswieza sie sama co 2s (patrz BOTCHAT_TEMPLATE w
		## admin_panel.py) - ten przycisk po prostu wymusza to natychmiast.
		self.globalChatRefreshButton.SetEvent(ui.__mem_func__(self.__PBAOpenGlobalChatWeb))

		## Zakladka: Akcje na zywo (prawa kolumna)
		self.liveRightPanel = self.__PBAMakeBoard(self, PBA_RIGHT_X, PBA_CONTENT_Y, PBA_RIGHT_WIDTH, PBA_CONTENT_HEIGHT)
		self.liveSelectedBotText = self.__PBAMakeText(self.liveRightPanel, 10, 10, "Wybierz bota z listy po lewej")
		self.liveLogScrollBar = self.__PBAMakeScrollBar(self.liveRightPanel, PBA_RIGHT_WIDTH - 25, 35, PBA_CONTENT_HEIGHT - 80)
		self.liveLogScrollBar.SetScrollEvent(ui.__mem_func__(self.OnScrollLiveLog))
		self.liveLogBoard = self.__PBAMakeBoard(self.liveRightPanel, 10, 35, PBA_RIGHT_WIDTH - 45, PBA_CONTENT_HEIGHT - 80)
		self.liveRefreshButton = self.__PBAMakeButton(self.liveRightPanel, 10, PBA_CONTENT_HEIGHT - 35, 90, 20, "Odswiez log", "small")
		self.liveRefreshButton.SetEvent(ui.__mem_func__(self.RequestBotLog))

		## Zakladka: Zarzadzanie (prawa kolumna)
		self.manageRightPanel = self.__PBAMakeBoard(self, PBA_RIGHT_X, PBA_CONTENT_Y, PBA_RIGHT_WIDTH, PBA_CONTENT_HEIGHT)
		self.manageSelectedBotText = self.__PBAMakeText(self.manageRightPanel, 10, 10, "Wybierz bota z listy po lewej")

		self.manageKickButton = self.__PBAMakeButton(self.manageRightPanel, 10, 45, 130, 20, "Wyrzuc bota")
		self.manageKickButton.SetEvent(ui.__mem_func__(self.OnClickKick))
		self.manageRespawnButton = self.__PBAMakeButton(self.manageRightPanel, 145, 45, 130, 20, "Zrespawnuj")
		self.manageRespawnButton.SetEvent(ui.__mem_func__(self.OnClickRespawn))

		self.__PBAMakeText(self.manageRightPanel, 10, 90, "Daj przedmiot (vnum / ilosc):")
		self.manageVnumEdit = self.__PBAMakeEdit(self.manageRightPanel, 10, 110, 80, 6)
		self.manageCountEdit = self.__PBAMakeEdit(self.manageRightPanel, 100, 110, 60, 4)
		self.manageGiveButton = self.__PBAMakeButton(self.manageRightPanel, 195, 108, 70, 20, "Wyslij", "small")
		self.manageGiveButton.SetEvent(ui.__mem_func__(self.OnClickGive))

		self.manageRefreshButton = self.__PBAMakeButton(self.manageRightPanel, 10, PBA_CONTENT_HEIGHT - 35, 100, 20, "Odswiez liste", "small")
		self.manageRefreshButton.SetEvent(ui.__mem_func__(self.RequestBotList))

		## Zakladka: Osiagniecia
		self.achievementsPage = self.__PBAMakeBoard(self, PBA_CONTENT_X, PBA_CONTENT_Y, PBA_WINDOW_WIDTH - PBA_CONTENT_X * 2, PBA_CONTENT_HEIGHT)
		self.achievementsBoard = self.__PBAMakeBoard(self.achievementsPage, 0, 0, PBA_WINDOW_WIDTH - PBA_CONTENT_X * 2, PBA_CONTENT_HEIGHT - 45)
		self.achievementsRefreshButton = self.__PBAMakeButton(self.achievementsPage, 10, PBA_CONTENT_HEIGHT - 35, 90, 20, "Odswiez", "small")
		self.achievementsRefreshButton.SetEvent(ui.__mem_func__(self.RequestAchievements))

		self.__PBAShowTab("general")

	def Destroy(self):
		self.__PBACloseGlobalChatWeb()
		self.__ClearOverheadBoards()
		self.__ClearBotListUI()
		self.__ClearLogUI()
		self.__ClearAchievementsUI()
		self._widgets = []
		self.Hide()

	def Open(self):
		self.__PBAShowTab("general")
		self.Show()
		self.SetCenterPosition()
		self.RequestStats()
		self.RequestBotList()
		self.RequestAchievements()

	def Close(self):
		self.__PBACloseGlobalChatWeb()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OnUpdate(self):
		## Wolane co klatke przez silnik (ten sam mechanizm co uiweb.WebWindow),
		## dopoki to okno jest widoczne.
		if self.globalChatWebOpen:
			newPos = self.GetGlobalPosition()
			if newPos != self.globalChatWebLastPos:
				self.globalChatWebLastPos = newPos
				try:
					app.MoveWebPage(self.__PBAGlobalChatRect())
				except:
					self.globalChatWebOpen = False

	######################################################################
	## Dymki nad glowami botow - BotOverheadTail (na gorze pliku), pozycja
	## z chr.GetProjectPosition(vid, ...), ten sam sprawdzony mechanizm co
	## szyld "stragan" w uiprivateshopbuilder.py. Dzialaja SAME, bez pomocy
	## z zewnatrz - ui.ThinBoard dostaje OnUpdate() co klatke od silnika
	## niezaleznie od stanu jakiegokolwiek innego okna - stad brak potrzeby
	## wolania czegokolwiek z game.py poza samym dostarczeniem danych.
	######################################################################

	def OnOverheadTail(self, vid, wire):
		vid = int(vid)
		lines = [l.replace("~", " ") for l in wire.split("|") if l]
		if not lines:
			return

		board = self.overheadBoards.get(vid)
		if board is None:
			board = BotOverheadTail()
			self.overheadBoards[vid] = board
			board.Open(vid, lines)
		else:
			board.SetLines(lines)

	def __ClearOverheadBoards(self):
		for board in self.overheadBoards.values():
			board.Destroy()
		self.overheadBoards = {}

	######################################################################
	## Zakladki
	######################################################################

	def OnClickTab(self, tabName):
		self.__PBAShowTab(tabName)

	def __PBAShowTab(self, tabName):
		self.activeTab = tabName

		if tabName != "globalchat":
			self.__PBACloseGlobalChatWeb()

		self.generalPage.Hide()
		self.globalChatPage.Hide()
		self.liveRightPanel.Hide()
		self.manageRightPanel.Hide()
		self.achievementsPage.Hide()
		self.botListPanel.Hide()

		if tabName == "general":
			self.generalPage.Show()
		elif tabName == "globalchat":
			self.globalChatPage.Show()
			self.__PBAOpenGlobalChatWeb()
		elif tabName == "live":
			self.botListPanel.Show()
			self.liveRightPanel.Show()
			if self.selectedPid:
				self.RequestBotLog()
		elif tabName == "manage":
			self.botListPanel.Show()
			self.manageRightPanel.Show()
		elif tabName == "achievements":
			self.achievementsPage.Show()

	######################################################################
	## Zakladka: Ogolne
	######################################################################

	def RequestStats(self):
		net.SendChatPacket("/botadmin_stats")

	def OnStats(self, total, inParty, stalls):
		self.generalTotalText.SetText("Aktywne boty: %s" % total)
		self.generalInPartyText.SetText("W party: %s" % inParty)
		self.generalStallsText.SetText("Prowadza stragan: %s" % stalls)

	######################################################################
	## Przewijalna lista tekstu - wspolne dla "Czat ogolny" i "Akcje botow".
	## WSZYSTKIE linie dostaja swoj TextLine raz, scroll tylko chowa/pokazuje
	## i przesuwa - nic sie nie tworzy/niszczy w trakcie przeciagania paska.
	######################################################################

	def __PBABuildLineWidgets(self, board, lines):
		widgets = []
		for text in lines:
			line = ui.MakeTextLine(board)
			line.SetText(text)
			widgets.append(line)
		return widgets

	def __PBASetupLineScrollBar(self, scrollBar, panelHeight, itemCount):
		visibleCount = max(1, panelHeight / PBA_LOG_LINE_HEIGHT)
		if itemCount <= visibleCount:
			scrollBar.Hide()
			return 0
		scrollBar.SetScrollBarSize(panelHeight)
		scrollBar.SetMiddleBarSize(float(visibleCount) / float(itemCount))
		scrollBar.Show()
		return itemCount - visibleCount

	def __PBALocateLineWidgets(self, widgets, startLine, panelHeight):
		yPos = 0
		for i in xrange(len(widgets)):
			widget = widgets[i]
			if i < startLine or yPos + PBA_LOG_LINE_HEIGHT > panelHeight:
				widget.Hide()
				continue
			widget.SetPosition(0, yPos)
			widget.Show()
			yPos += PBA_LOG_LINE_HEIGHT

	######################################################################
	## Zakladka: Czat ogolny - prawdziwa strona panelu w wbudowanej
	## przegladarce, nie tekst renderowany przez silnik gry. Patrz
	## PBA_BOTCHAT_URL wyzej i /botchat w admin_panel.py.
	######################################################################

	def __PBAGlobalChatRect(self):
		wx, wy = self.GetGlobalPosition()
		sx = wx + PBA_GLOBAL_CHAT_RECT_X
		sy = wy + PBA_GLOBAL_CHAT_RECT_Y
		return (sx, sy, sx + PBA_GLOBAL_CHAT_RECT_WIDTH, sy + PBA_GLOBAL_CHAT_RECT_HEIGHT)

	def __PBAOpenGlobalChatWeb(self):
		self.globalChatWebLastPos = self.GetGlobalPosition()
		# The flag drives MoveWebPage every frame from OnUpdate, so it may
		# only go up once the browser really started: the embedded control
		# refuses on some machines (CREATE_WEBBROWSER_ERROR 1407) and moving
		# a page that was never created is the same crash by another door.
		try:
			app.ShowWebPage(PBA_BOTCHAT_URL, self.__PBAGlobalChatRect())
			self.globalChatWebOpen = True
		except:
			self.globalChatWebOpen = False

	def __PBACloseGlobalChatWeb(self):
		if not self.globalChatWebOpen:
			return
		self.globalChatWebOpen = False
		try:
			app.HideWebPage()
		except:
			# The embedded browser can refuse to start (CREATE_WEBBROWSER_ERROR
			# 1407 on every machine that reported this), and it took the whole
			# client down with it. A tab that cannot draw must not close the game.
			pass

	######################################################################
	## Lista botow (wspolna: Akcje na zywo + Zarzadzanie)
	######################################################################

	def RequestBotList(self):
		net.SendChatPacket("/botadmin_list")

	def OnBotRow(self, pid, level, empire, x, y, name):
		self.botRows.append((int(pid), int(level), int(empire), int(x), int(y), name))

	def OnBotListEnd(self):
		self.__PBARebuildBotListUI()

	def __ClearBotListUI(self):
		for btn in self.botRowButtons:
			btn.Hide()
		self.botRowButtons = []

	def __PBARebuildBotListUI(self):
		self.__ClearBotListUI()

		pageSize = self.botListPanel.GetHeight() - 30
		self.botListScrollBar.SetScrollBarSize(pageSize)

		rowCount = len(self.botRows)
		if rowCount <= pageSize / PBA_ROW_HEIGHT:
			self.botListScrollBar.Hide()
			self.botListStartLine = 0
		else:
			self.botListScrollBar.SetMiddleBarSize(float(pageSize / PBA_ROW_HEIGHT) / float(rowCount))
			self.botListScrollBar.Show()

		for pid, level, empire, x, y, name in self.botRows:
			shortName = name[:PBA_LIST_ROW_NAME_MAXLEN]
			button = ui.MakeButton(self.botListSlotBoard, 0, 0, "Lv%d %s" % (level, shortName),
					"d:/ymir work/ui/public/", "small_button_01.sub", "small_button_02.sub", "small_button_03.sub")
			## Bez tego przycisk przyjmuje natywny rozmiar grafiki .sub, ktory
			## bywa wiekszy/inny niz miejsce w liscie i wychodzi poza okno.
			button.SetSize(PBA_LIST_ROW_WIDTH, PBA_LIST_ROW_HEIGHT)
			button.SetEvent(ui.__mem_func__(self.OnSelectBot), pid)
			self.botRowButtons.append(button)

		self.__PBALocateBotListRows()

	def OnScrollBotList(self):
		pageSize = self.botListPanel.GetHeight() - 30
		visibleRows = max(1, pageSize / PBA_ROW_HEIGHT)
		scrollableRows = max(0, len(self.botRows) - visibleRows)
		startLine = int(scrollableRows * self.botListScrollBar.GetPos())

		if startLine != self.botListStartLine:
			self.botListStartLine = startLine
			self.__PBALocateBotListRows()

	def __PBALocateBotListRows(self):
		yPos = 0
		for i in xrange(len(self.botRowButtons)):
			button = self.botRowButtons[i]
			if i < self.botListStartLine:
				button.Hide()
				continue
			button.SetPosition(0, yPos)
			button.Show()
			yPos += PBA_ROW_HEIGHT

	def OnSelectBot(self, pid):
		self.selectedPid = int(pid)

		row = None
		for r in self.botRows:
			if r[0] == self.selectedPid:
				row = r
				break

		if row:
			pid, level, empire, x, y, name = row
			self.selectedEmpire = empire
			self.liveSelectedBotText.SetText("%s (Lv %d) - pid %d" % (name, level, pid))
			self.manageSelectedBotText.SetText("%s (Lv %d) - pid %d, poz (%d, %d), cesarstwo %d" %
					(name, level, pid, x, y, empire))

		if self.activeTab == "live":
			self.RequestBotLog()

	######################################################################
	## Zakladka: Akcje na zywo
	######################################################################

	def RequestBotLog(self):
		if not self.selectedPid:
			return
		self.logPid = self.selectedPid
		self.logLines = []
		net.SendChatPacket("/botadmin_botlog %d" % self.selectedPid)

	def OnBotLogLine(self, pid, text):
		if int(pid) != self.logPid:
			return
		self.logLines.append(text.replace("~", " "))

	def OnBotLogEnd(self, pid):
		if int(pid) != self.logPid:
			return
		self.__PBARebuildLogUI()

	def __ClearLogUI(self):
		for line in self.logTextLines:
			line.Hide()
		self.logTextLines = []

	def __PBARebuildLogUI(self):
		self.__ClearLogUI()
		self.logStartLine = 0

		## Najnowsze na gorze.
		if self.logLines:
			displayLines = list(reversed(self.logLines))
		else:
			displayLines = ["(ten bot nic jeszcze nie powiedzial)"]
		self.logTextLines = self.__PBABuildLineWidgets(self.liveLogBoard, displayLines)

		self.logScrollableCount = self.__PBASetupLineScrollBar(
				self.liveLogScrollBar, PBA_LIVE_LOG_BOARD_HEIGHT, len(self.logTextLines))
		self.__PBALocateLineWidgets(self.logTextLines, self.logStartLine, PBA_LIVE_LOG_BOARD_HEIGHT)

	def OnScrollLiveLog(self):
		startLine = int(self.logScrollableCount * self.liveLogScrollBar.GetPos())
		if startLine != self.logStartLine:
			self.logStartLine = startLine
			self.__PBALocateLineWidgets(self.logTextLines, self.logStartLine, PBA_LIVE_LOG_BOARD_HEIGHT)

	######################################################################
	## Zakladka: Zarzadzanie
	######################################################################

	def OnClickKick(self):
		if not self.selectedPid:
			return
		net.SendChatPacket("/bot_despawn %d" % self.selectedPid)
		self.selectedPid = 0
		self.RequestBotList()

	def OnClickRespawn(self):
		if not self.selectedPid or not self.selectedEmpire:
			return
		net.SendChatPacket("/bot_spawn %d %d" % (self.selectedPid, self.selectedEmpire))
		self.RequestBotList()

	def OnClickGive(self):
		if not self.selectedPid:
			return

		vnumText = self.manageVnumEdit.GetText()
		countText = self.manageCountEdit.GetText()

		if not vnumText:
			return
		if not countText:
			countText = "1"

		net.SendChatPacket("/botadmin_give %d %s %s" % (self.selectedPid, vnumText, countText))

	######################################################################
	## Zakladka: Osiagniecia
	######################################################################

	def RequestAchievements(self):
		self.achievementRows = {}
		net.SendChatPacket("/botadmin_achievements")

	def OnAchievementRow(self, id, pid, name):
		self.achievementRows[int(id)] = (int(pid), name)

	def OnAchievementsEnd(self):
		self.__PBARebuildAchievementsUI()

	def __ClearAchievementsUI(self):
		for line in self.achievementTextLines:
			line.Hide()
		self.achievementTextLines = []

	def __PBARebuildAchievementsUI(self):
		self.__ClearAchievementsUI()

		yPos = 0
		for achievementId in PBA_ACHIEVEMENT_ORDER:
			label = PBA_ACHIEVEMENT_LABELS.get(achievementId, "Osiagniecie #%d" % achievementId)
			pid, name = self.achievementRows.get(achievementId, (0, "-"))

			nameLine = ui.MakeTextLine(self.achievementsBoard)
			nameLine.SetPosition(0, yPos)
			nameLine.SetText(label)
			self.achievementTextLines.append(nameLine)

			winnerLine = ui.MakeTextLine(self.achievementsBoard)
			winnerLine.SetPosition(320, yPos)
			if pid:
				winnerLine.SetText("Zdobyl: %s" % name)
			else:
				winnerLine.SetText("Jeszcze nikt")
			self.achievementTextLines.append(winnerLine)

			yPos += PBA_ROW_HEIGHT


IsQBHide = 0
class Interface(object):
	CHARACTER_STATUS_TAB = 1
	CHARACTER_SKILL_TAB = 2

	def __init__(self):
		systemSetting.SetInterfaceHandler(self)
		self.windowOpenPosition = 0
		self.dlgWhisperWithoutTarget = None
		self.inputDialog = None
		self.tipBoard = None
		self.bigBoard = None
		self.fancyBoard = None

		# ITEM_MALL
		self.mallPageDlg = None
		# END_OF_ITEM_MALL

		self.wndWeb = None
		self.wndTaskBar = None
		self.wndCharacter = None
		self.wndInventory = None
		self.wndGMPanel = None
		self.wndTop1Badge = None
		self.wndPlayerbotAdmin = None
		self.wndItemShop = None
		self.wndExpandedTaskBar = None
		self.wndDragonSoul = None
		self.wndDragonSoulRefine = None
		self.wndChat = None
		self.wndMessenger = None
		self.wndMiniMap = None
		self.wndGuild = None
		self.wndGuildBuilding = None

		self.wndPopupDialog = None

		self.listGMName = {}
		self.wndQuestWindow = {}
		self.wndQuestWindowNewKey = 0
		self.privateShopAdvertisementBoardDict = {"player":{}, "offline":{}}
		self.reputationBarDict = {}
		self.guildScoreBoardDict = {}
		self.equipmentDialogDict = {}
		# Panel GM: "EQ" na menu celu karmi to samo EquipmentDialog, ale z
		# tekstowej odpowiedzi serwera (GMEquipChunk), vid -> bufor kawalkow.
		self._gmEquipBuffers = {}
		if app.ENABLE_MOVE_CHANNEL:
			self.wndMoveChannel = None
		if app.ENABLE_WON_EXCHANGE_WINDOW:
			self.wndWonExchange = None
		event.SetInterfaceWindow(self)

		self.interfaceWindowList = {}

		self.RegisterEvents()
		self.popupManager = uiCommon.PopupManager()

	def __del__(self):
		systemSetting.DestroyInterfaceHandler()
		event.SetInterfaceWindow(None)

	def RegisterEvents(self):
		eventMgr = eventManager.EventManager()
		eventMgr.add_observer(eventManager.OPEN_WHISPER_EVENT, self.OpenWhisperDialog)
		eventMgr.add_observer(eventManager.EVENT_MARK_SHOP_VIEWED, self.MarkPrivateShopAsViewed)
		eventMgr.add_observer(eventManager.EVENT_MARK_SHOP_CURRENT, self.MarkPrivateShopAsCurrent)
		eventMgr.add_observer(uiShop.EVENT_CLICK_PRIVATE_SHOP, self.ClickPrivateShop)
		eventMgr.add_observer(uiMessenger.EVENT_UPDATE_BLOCK_STATE, self.__UpdateUserBlockState)

	################################
	## Make Windows & Dialogs
	def __MakeUICurtain(self):
		wndUICurtain = ui.Bar("TOP_MOST")
		wndUICurtain.SetSize(wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight())
		wndUICurtain.SetColor(0x77000000)
		wndUICurtain.Hide()
		self.wndUICurtain = wndUICurtain

	def __MakeMessengerWindow(self):
		self.wndMessenger = uiMessenger.MessengerWindow()

		from _weakref import proxy
		self.wndMessenger.SetWhisperButtonEvent(lambda n,i=proxy(self):i.OpenWhisperDialog(n))
		self.wndMessenger.SetGuildButtonEvent(ui.__mem_func__(self.ToggleGuildWindow))

	def __MakeGuildWindow(self):
		self.wndGuild = uiGuild.GuildWindow()

	def __MakeChatWindow(self):

		wndChat = uiChat.ChatWindow()

		wndChat.SetSize(wndChat.CHAT_WINDOW_WIDTH, 0)
		wndChat.SetPosition(wndMgr.GetScreenWidth()/2 - wndChat.CHAT_WINDOW_WIDTH/2, wndMgr.GetScreenHeight() - wndChat.EDIT_LINE_HEIGHT - 37)
		wndChat.SetHeight(200)
		wndChat.Refresh()
		wndChat.Show()

		self.wndChat = wndChat
		self.wndChat.BindInterface(self)
		self.wndChat.SetSendWhisperEvent(ui.__mem_func__(self.OpenWhisperDialogWithoutTarget))
		self.wndChat.SetOpenChatLogEvent(ui.__mem_func__(self.ToggleChatLogWindow))

	def __MakeTaskBar(self):
		wndTaskBar = uiTaskBar.TaskBar()
		wndTaskBar.LoadWindow()
		self.wndTaskBar = wndTaskBar
		self.wndTaskBar.SetToggleButtonEvent(uiTaskBar.TaskBar.BUTTON_CHARACTER, ui.__mem_func__(self.ToggleCharacterWindowStatusPage))
		self.wndTaskBar.SetToggleButtonEvent(uiTaskBar.TaskBar.BUTTON_INVENTORY, ui.__mem_func__(self.ToggleInventoryWindow))
		self.wndTaskBar.SetToggleButtonEvent(uiTaskBar.TaskBar.BUTTON_MESSENGER, ui.__mem_func__(self.ToggleMessenger))
		self.wndTaskBar.SetToggleButtonEvent(uiTaskBar.TaskBar.BUTTON_SYSTEM, ui.__mem_func__(self.ToggleSystemDialog))
		# if uiTaskBar.TaskBar.IS_EXPANDED:
		# 	self.wndTaskBar.SetToggleButtonEvent(uiTaskBar.TaskBar.BUTTON_EXPAND, ui.__mem_func__(self.ToggleExpandedButton))
		# 	self.wndExpandedTaskBar = uiTaskBar.ExpandedTaskBar()
		# 	self.wndExpandedTaskBar.LoadWindow()
		# 	self.wndExpandedTaskBar.SetToggleButtonEvent(uiTaskBar.ExpandedTaskBar.BUTTON_DRAGON_SOUL, ui.__mem_func__(self.ToggleDragonSoulWindow))
		#
		# else:
		# 	self.wndTaskBar.SetToggleButtonEvent(uiTaskBar.TaskBar.BUTTON_CHAT, ui.__mem_func__(self.ToggleChat))

		self.wndEnergyBar = None
		import app
		if app.ENABLE_ENERGY_SYSTEM:
			wndEnergyBar = uiTaskBar.EnergyBar()
			wndEnergyBar.LoadWindow()
			self.wndEnergyBar = wndEnergyBar

	def __MakeParty(self):
		wndParty = uiParty.PartyWindow()
		wndParty.Hide()
		self.wndParty = wndParty

	def __MakeGameButtonWindow(self):
		wndGameButton = uiGameButton.GameButtonWindow()
		wndGameButton.SetTop()
		wndGameButton.Show()
		wndGameButton.SetButtonEvent("STATUS", ui.__mem_func__(self.__OnClickStatusPlusButton))
		wndGameButton.SetButtonEvent("SKILL", ui.__mem_func__(self.__OnClickSkillPlusButton))
		wndGameButton.SetButtonEvent("QUEST", ui.__mem_func__(self.__OnClickQuestButton))
		wndGameButton.SetButtonEvent("HELP", ui.__mem_func__(self.__OnClickHelpButton))
		wndGameButton.SetButtonEvent("BUILD", ui.__mem_func__(self.__OnClickBuildButton))

		self.wndGameButton = wndGameButton

	def __IsChatOpen(self):
		return True

	def __MakeWindows(self):
		wndCharacter = uiCharacter.CharacterWindow()
		wndInventory = uiInventory.InventoryWindow()
		wndInventory.BindInterfaceClass(self)
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			wndDragonSoul = uiDragonSoul.DragonSoulWindow()
			wndDragonSoulRefine = uiDragonSoul.DragonSoulRefineWindow()
		else:
			wndDragonSoul = None
			wndDragonSoulRefine = None

		wndMiniMap = uiMiniMap.MiniMap()
		wndSafebox = uiSafebox.SafeboxWindow()

		# ITEM_MALL
		wndMall = uiSafebox.MallWindow()
		self.wndMall = wndMall
		# END_OF_ITEM_MALL

		wndChatLog = uiChat.ChatLogWindow()
		wndChatLog.BindInterface(self)

		# Panel GM jest dodatkiem i nie moze byc powodem, dla ktorego gra sie
		# nie wczytuje. Okno powstaje w srodku budowy interfejsu, wiec kazdy
		# wyjatek - widget, ktorego ten klient nie ma, brakujacy klucz locale -
		# przerywal cala budowe i pasek ladowania stawal na 100% z pustym
		# ekranem (Dixdros, jaroszv2, .unright, ligivanastrea, 10-11 wrzesnia;
		# stockowy root wstawal na tych samych maszynach). Wiec: zbuduj, a gdy
		# sie nie da - napisz do syserr.txt i graj dalej bez niego. Kazdy
		# wolajacy nizej traktuje wndGMPanel jako mogace nie istniec.
		self.wndGMPanel = None
		try:
			wndGMPanel = GMPanelWindow()
			wndGMPanel.Hide()
			self.wndGMPanel = wndGMPanel
		except:
			# The reason, not just the fact: a fail-safe that hides why it fired
			# turns every player report into one nobody can act on.
			import dbg, traceback
			dbg.TraceError("GM panel (F9) could not be built - the game loads without it")
			for line in traceback.format_exc().splitlines():
				dbg.TraceError("    " + line)

		# To samo dla plakietki Top1: jej rejestracja w game.py siedzi juz w
		# try/except, ale samo okno powstawalo tutaj bez oslony, a pliku
		# top1.mse nie ma w tym kliencie w ogole.
		self.wndTop1Badge = None
		try:
			wndTop1Badge = Top1Badge()
			wndTop1Badge.Hide()
			self.wndTop1Badge = wndTop1Badge
		except:
			# The reason, not just the fact: a fail-safe that hides why it fired
			# turns every player report into one nobody can act on.
			import dbg, traceback
			dbg.TraceError("Top1Badge could not be built - the game loads without it")
			for line in traceback.format_exc().splitlines():
				dbg.TraceError("    " + line)

		self.wndCharacter = wndCharacter
		self.wndInventory = wndInventory
		self.wndDragonSoul = wndDragonSoul
		self.wndDragonSoulRefine = wndDragonSoulRefine
		self.wndMiniMap = wndMiniMap
		self.wndSafebox = wndSafebox
		self.wndChatLog = wndChatLog

		self.wndItemShop = uiItemShop.ItemShopWindow()
		self.wndItemShop.Hide()

		self.wndCrafting = uiCraft.CraftingWindow()
		self.wndCrafting.Hide()

		self.wndHorseInventory = uiHorseInventory.HorseInventoryWindow()
		self.wndHorseInventory.Hide()
		self.wndInventory.SetHorseInventory(self.wndHorseInventory)

		self.wndAttributeList = uiAttributeList.AttributeListWindow()
		self.wndAttributeList.Hide()

		self.wndSpecialShop = uiSpecialShop.SpecialShopWindow()
		self.wndSpecialShop.Hide()

		self.wndPlayerStat = uiPlayerStat.PlayerStatsWindow()
		self.wndPlayerStat.Hide()

		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoul.SetDragonSoulRefineWindow(self.wndDragonSoulRefine)
			self.wndDragonSoulRefine.SetInventoryWindows(self.wndInventory, self.wndDragonSoul)
			self.wndInventory.SetDragonSoulRefineWindow(self.wndDragonSoulRefine)

		if app.ENABLE_MOVE_CHANNEL:
			self.wndMoveChannel = uiMoveChannel.MoveChannelWindow()

		if app.ENABLE_WON_EXCHANGE_WINDOW:
			self.wndWonExchange = uiWonExchange.WonExchangeWindow()
			self.wndWonExchange.BindInterface(self)

	def __MakeDialogs(self):
		self.tooltip = uiToolTip.ToolTip()
		self.tooltip.Hide()

		self.tooltipItem = uiToolTip.ItemToolTip()
		self.tooltipItem.Hide()

		self.tooltipSkill = uiToolTip.SkillToolTip()
		self.tooltipSkill.Hide()

		self.dlgExchange = uiExchange.ExchangeDialog()
		self.dlgExchange.LoadDialog()
		self.dlgExchange.SetCenterPosition()
		self.dlgExchange.Hide()

		self.dlgPointReset = uiPointReset.PointResetDialog()
		self.dlgPointReset.LoadDialog()
		self.dlgPointReset.Hide()

		self.dlgShop = uiShop.ShopDialog()
		self.dlgShop.LoadDialog()
		self.dlgShop.Hide()

		self.dlgRestart = uiRestart.RestartDialog()
		self.dlgRestart.LoadDialog()
		self.dlgRestart.Hide()

		self.dlgSystem = uiSystem.SystemDialog()
		self.dlgSystem.LoadDialog()
		self.dlgSystem.SetToolTip(self.tooltip)
		self.dlgSystem.SetOpenHelpWindowEvent(ui.__mem_func__(self.OpenHelpWindow))
		self.dlgSystem.BindInterface(self)
		self.dlgSystem.Hide()

		self.dlgPassword = uiSafebox.PasswordDialog()
		self.dlgPassword.Hide()

		self.itemExchangeDialog = uiItemExchange.ExchangeItemDialog()
		self.itemExchangeDialog.SetItemToolTip(self.tooltipItem)
		self.itemExchangeDialog.Hide()

		self.offlineShopBuilder = offlineShopBuilder.OfflineShopBuilder()
		self.offlineShopBuilder.Hide()

		self.offlineShopManage = offlineShopManage.OfflineShopManage()
		self.offlineShopManage.Hide()

		self.offlineShopGuest = offlineShopGuest.OfflineShopGuest()
		self.offlineShopGuest.Hide()

		self.offlineShopHistory = offlineShopHistory.OfflineShopHistory()
		self.offlineShopHistory.Hide()

		self.offlineShopSearch = offlineShopSearch.ShopSearchWindow()
		self.offlineShopSearch.SetToolTip(self.tooltipItem)
		self.offlineShopSearch.Hide()

		self.hyperlinkItemTooltip = uiToolTip.HyperlinkItemToolTip()
		self.hyperlinkItemTooltip.Hide()

		self.dlgRefineNew = uiRefine.RefineDialogNew()
		self.dlgRefineNew.Hide()

		self.fishingGameDialog = uiFishing.FishingGameDialog()
		self.fishingGameDialog.Hide()

		self.potionRechargeDialog = uiPotionRecharge.PotionRechargeDialog()
		self.potionRechargeDialog.SetItemToolTip(self.tooltipItem)
		self.potionRechargeDialog.Hide()

		self.busyActionDialog = uiBusyAction.BusyActionDialog()
		self.busyActionDialog.Hide()

		self.maintenanceDialog = uiMaintenance.MaintenanceDialog()
		self.maintenanceDialog.Hide()

		self.reportPlayerDialog = uiReport.ReportPlayerDialog()
		self.reportPlayerDialog.Hide()

		self.captchaDialog = uiCaptcha.CaptchaDialog()
		self.captchaDialog.Hide()

		self.gameMasterTargetDialog = None

	def __MakeHelpWindow(self):
		self.wndHelp = uiHelp.HelpWindow()
		self.wndHelp.LoadDialog()
		self.wndHelp.SetCloseEvent(ui.__mem_func__(self.CloseHelpWindow))
		self.wndHelp.Hide()

	def __MakeTipBoard(self):
		self.tipBoard = uiTip.TipBoard()
		self.tipBoard.Hide()

		self.bigBoard = uiTip.BigBoard()
		self.bigBoard.Hide()

		self.fancyBoard = uiTip.FancyBoard()
		self.fancyBoard.Hide()

	def __MakeWebWindow(self):
		if constInfo.IN_GAME_SHOP_ENABLE:
			import uiWeb
			self.wndWeb = uiWeb.WebWindow()
			self.wndWeb.LoadWindow()
			self.wndWeb.Hide()

	def __MakeCubeWindow(self):
		self.wndCube = uiCube.CubeWindow()
		self.wndCube.LoadWindow()
		self.wndCube.Hide()

	def __MakeCubeResultWindow(self):
		self.wndCubeResult = uiCube.CubeResultWindow()
		self.wndCubeResult.LoadWindow()
		self.wndCubeResult.Hide()

	if app.ENABLE_ACCE_COSTUME_SYSTEM:
		def __MakeAcceWindow(self):
			self.wndAcceCombine = uiacce.CombineWindow()
			self.wndAcceCombine.LoadWindow()
			self.wndAcceCombine.Hide()

			self.wndAcceAbsorption = uiacce.AbsorbWindow()
			self.wndAcceAbsorption.LoadWindow()
			self.wndAcceAbsorption.Hide()

			if self.wndInventory:
				self.wndInventory.SetAcceWindow(self.wndAcceCombine, self.wndAcceAbsorption)

	# ACCESSORY_REFINE_ADD_METIN_STONE
	def __MakeItemSelectWindow(self):
		self.wndItemSelect = uiSelectItem.SelectItemWindow()
		self.wndItemSelect.Hide()
	# END_OF_ACCESSORY_REFINE_ADD_METIN_STONE

	def __MakePlayerbotAdminWindow(self):
		# Ta sama zasada co przy panelu GM: okno admina botow nie moze
		# przerwac budowy calego interfejsu.
		self.wndPlayerbotAdmin = None
		try:
			wndPlayerbotAdmin = PlayerbotAdminWindow()
			wndPlayerbotAdmin.LoadWindow()
			wndPlayerbotAdmin.Hide()
			self.wndPlayerbotAdmin = wndPlayerbotAdmin
		except:
			# The reason, not just the fact: a fail-safe that hides why it fired
			# turns every player report into one nobody can act on.
			import dbg, traceback
			dbg.TraceError("Playerbot admin window (F10) could not be built - the game loads without it")
			for line in traceback.format_exc().splitlines():
				dbg.TraceError("    " + line)

	def MakeInterface(self):
		self.__MakeMessengerWindow()
		self.__MakeGuildWindow()
		self.__MakeChatWindow()
		self.__MakeParty()
		self.__MakeWindows()
		self.__MakeDialogs()

		self.__MakeUICurtain()
		self.__MakeTaskBar()
		self.__MakeGameButtonWindow()
		self.__MakeHelpWindow()
		self.__MakeTipBoard()
		self.__MakeWebWindow()
		self.__MakeCubeWindow()
		self.__MakeCubeResultWindow()
		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			self.__MakeAcceWindow()

		# ACCESSORY_REFINE_ADD_METIN_STONE
		self.__MakeItemSelectWindow()
		# END_OF_ACCESSORY_REFINE_ADD_METIN_STONE

		self.__MakePlayerbotAdminWindow()

		#gamemater
		self.gameMaster_CaptchaDialogs = {}

		self.questButtonList = []
		self.whisperButtonList = []
		self.whisperDialogDict = {}
		self.privateShopAdvertisementBoardDict = {"player":{}, "offline":{}}
		self.reputationBarDict = {}

		self.wndInventory.SetItemToolTip(self.tooltipItem)
		self.wndItemShop.SetItemToolTip(self.tooltipItem)
		self.wndItemShop.SetToolTip(self.tooltip)

		self.dlgRefineNew.SetItemToolTip(self.tooltipItem)

		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoul.SetItemToolTip(self.tooltipItem)
			self.wndDragonSoulRefine.SetItemToolTip(self.tooltipItem)
		self.wndSafebox.SetItemToolTip(self.tooltipItem)
		self.wndCube.SetItemToolTip(self.tooltipItem)
		self.wndCubeResult.SetItemToolTip(self.tooltipItem)

		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			self.wndAcceCombine.SetItemToolTip(self.tooltipItem)
			self.wndAcceAbsorption.SetItemToolTip(self.tooltipItem)

		# ITEM_MALL
		self.wndMall.SetItemToolTip(self.tooltipItem)
		# END_OF_ITEM_MALL

		self.wndCharacter.SetSkillToolTip(self.tooltipSkill)
		self.wndTaskBar.SetItemToolTip(self.tooltipItem)
		self.wndTaskBar.SetSkillToolTip(self.tooltipSkill)
		self.wndGuild.SetSkillToolTip(self.tooltipSkill)

		# ACCESSORY_REFINE_ADD_METIN_STONE
		self.wndItemSelect.SetItemToolTip(self.tooltipItem)
		# END_OF_ACCESSORY_REFINE_ADD_METIN_STONE

		self.dlgShop.SetItemToolTip(self.tooltipItem)
		self.dlgExchange.SetItemToolTip(self.tooltipItem)
		self.offlineShopBuilder.SetToolTip(self.tooltip, self.tooltipItem)
		self.offlineShopManage.SetToolTip(self.tooltip, self.tooltipItem)
		self.offlineShopGuest.SetToolTip(self.tooltip, self.tooltipItem)
		self.wndHorseInventory.SetToolTip(self.tooltip)
		self.wndCrafting.SetItemToolTip(self.tooltipItem)

		self.__InitWhisper()
		self.DRAGON_SOUL_IS_QUALIFIED = True if app.ENABLE_NO_DSS_QUALIFICATION else False

		self.__InitializeWindows()

	def __InitializeWindows(self):
		pass

	def _AppendInterfaceWindow(self, name, window):
		self.interfaceWindowList[name] = window

	def _GetInterfaceWindow(self, name):
		return self.interfaceWindowList.get(name, None)

	GetInterfaceWindow = _GetInterfaceWindow

	def MakeHyperlinkTooltip(self, hyperlink):
		tokens = hyperlink.split(":")
		if tokens and len(tokens):
			type = tokens[0]
			if "item" == type:
				self.hyperlinkItemTooltip.SetHyperlinkItem(tokens)
			elif "msg" == type:
				data = tokens[1].split(",")
				name = data[0]
				empire = int(data[1])
				self.OpenWhisperDialog(name, empire)

	## Make Windows & Dialogs
	################################

	def Close(self):
		if self.popupManager:
			self.popupManager.Destroy()
			del self.popupManager

		if self.dlgWhisperWithoutTarget:
			self.dlgWhisperWithoutTarget.Destroy()
			self.dlgWhisperWithoutTarget = None

		player_name = player.GetMainCharacterName()
		if constInfo.WHISPER_DICT.has_key(player_name):
			for target_name, whisper_data in constInfo.WHISPER_DICT[player_name].items():
				if whisper_data["dialog"]:
					whisper_data["dialog"].Destroy()
					whisper_data["dialog"] = None

		if uiQuest.QuestDialog.__dict__.has_key("QuestCurtain"):
			uiQuest.QuestDialog.QuestCurtain.Close()
			del uiQuest.QuestDialog.QuestCurtain #@fixme016 it's recreated only if it's deleted from scope

		if self.wndQuestWindow:
			for key, eachQuestWindow in self.wndQuestWindow.items():
				eachQuestWindow.nextCurtainMode = -1
				eachQuestWindow.CloseSelf()
				eachQuestWindow = None
		self.wndQuestWindow = {}

		chat.DestroyWhisperMap()

		if self.wndChat:
			self.wndChat.Destroy()

		if self.wndTaskBar:
			self.wndTaskBar.Destroy()

		if self.wndExpandedTaskBar:
			self.wndExpandedTaskBar.Destroy()

		if self.wndEnergyBar:
			self.wndEnergyBar.Destroy()

		if self.wndCharacter:
			self.wndCharacter.Destroy()

		if self.wndInventory:
			self.wndInventory.Destroy()

		if self.wndItemShop:
			self.wndItemShop.Destroy()

		if self.wndCrafting:
			self.wndCrafting.Destroy()

		if self.wndHorseInventory:
			self.wndHorseInventory.Destroy()

		if self.wndSpecialShop:
			self.wndSpecialShop.Destroy()

		if self.wndAttributeList:
			self.wndAttributeList.Destroy()

		if self.wndPlayerStat:
			self.wndPlayerStat.Destroy()

		if self.wndDragonSoul:
			self.wndDragonSoul.Destroy()

		if self.wndDragonSoulRefine:
			self.wndDragonSoulRefine.Destroy()

		if self.dlgExchange:
			self.dlgExchange.Destroy()

		if self.dlgPointReset:
			self.dlgPointReset.Destroy()

		if self.dlgShop:
			self.dlgShop.Destroy()

		if self.dlgRestart:
			self.dlgRestart.Destroy()

		if self.dlgSystem:
			self.dlgSystem.Destroy()

		if self.dlgPassword:
			self.dlgPassword.Destroy()

		if self.wndMiniMap:
			self.wndMiniMap.Destroy()

		if self.wndSafebox:
			self.wndSafebox.Destroy()

		if self.wndWeb:
			self.wndWeb.Destroy()
			self.wndWeb = None

		if self.wndMall:
			self.wndMall.Destroy()

		if self.wndParty:
			self.wndParty.Destroy()

		if self.wndHelp:
			self.wndHelp.Destroy()

		if self.wndCube:
			self.wndCube.Destroy()

		if app.ENABLE_ACCE_COSTUME_SYSTEM and self.wndAcceCombine:
			self.wndAcceCombine.Destroy()

		if app.ENABLE_ACCE_COSTUME_SYSTEM and self.wndAcceAbsorption:
			self.wndAcceAbsorption.Destroy()

		if app.ENABLE_MOVE_CHANNEL and self.wndMoveChannel:
			self.wndMoveChannel.Destroy()
			self.wndMoveChannel = None

		if app.ENABLE_WON_EXCHANGE_WINDOW:
			self.wndWonExchange.Destroy()
			self.wndWonExchange = None

		if self.wndCubeResult:
			self.wndCubeResult.Destroy()

		if self.wndPlayerbotAdmin:
			self.wndPlayerbotAdmin.Destroy()

		if self.wndMessenger:
			self.wndMessenger.Destroy()

		if self.wndGuild:
			self.wndGuild.Destroy()

		if self.offlineShopBuilder:
			self.offlineShopBuilder.Destroy()

		if self.offlineShopManage:
			self.offlineShopManage.Destroy()

		if self.offlineShopGuest:
			self.offlineShopGuest.Destroy()

		if self.offlineShopHistory:
			self.offlineShopHistory.Destroy()

		if self.offlineShopSearch:
			self.offlineShopSearch.Destroy()

		if self.dlgRefineNew:
			self.dlgRefineNew.Destroy()

		if self.wndGuildBuilding:
			self.wndGuildBuilding.Destroy()

		if self.wndGameButton:
			self.wndGameButton.Destroy()

		if self.fishingGameDialog:
			self.fishingGameDialog.Destroy()
			self.fishingGameDialog.Hide()

		if self.potionRechargeDialog:
			self.potionRechargeDialog.Destroy()
			self.potionRechargeDialog.Hide()

		if self.busyActionDialog:
			self.busyActionDialog.Destroy()
			self.busyActionDialog.Hide()

		if self.maintenanceDialog:
			self.maintenanceDialog.Destroy()
			self.maintenanceDialog.Hide()

		if self.reportPlayerDialog:
			self.reportPlayerDialog.Destroy()
			self.reportPlayerDialog.Hide()

		if self.itemExchangeDialog:
			self.itemExchangeDialog.Destroy()
			self.itemExchangeDialog.Hide()

		if self.captchaDialog:
			self.captchaDialog.Destroy()
			self.captchaDialog.Hide()

		if self.gameMasterTargetDialog:
			self.gameMasterTargetDialog.Destroy()
			self.gameMasterTargetDialog.Hide()

		if self.gameMaster_CaptchaDialogs:
			for dialog in self.gameMaster_CaptchaDialogs.values():
				if dialog:
					dialog.Destroy()
					dialog.Hide()

		# ITEM_MALL
		if self.mallPageDlg:
			self.mallPageDlg.Destroy()
		# END_OF_ITEM_MALL

		# ACCESSORY_REFINE_ADD_METIN_STONE
		if self.wndItemSelect:
			self.wndItemSelect.Destroy()
		# END_OF_ACCESSORY_REFINE_ADD_METIN_STONE

		self.wndChatLog.Destroy()
		for btn in self.questButtonList:
			btn.SetEvent(0)
		for btn in self.whisperButtonList:
			btn.SetEvent(0)
		for dlg in self.whisperDialogDict.itervalues():
			dlg.Destroy()
		for brd in self.guildScoreBoardDict.itervalues():
			brd.Destroy()
		for dlg in self.equipmentDialogDict.itervalues():
			dlg.Destroy()

		# ITEM_MALL
		del self.mallPageDlg
		# END_OF_ITEM_MALL

		del self.wndGuild
		del self.wndMessenger
		del self.wndUICurtain
		del self.wndChat
		del self.wndTaskBar
		if self.wndExpandedTaskBar:
			del self.wndExpandedTaskBar
		del self.wndEnergyBar
		del self.wndCharacter
		del self.wndInventory
		del self.wndItemShop
		del self.wndCrafting
		del self.wndHorseInventory
		del self.wndSpecialShop
		del self.wndAttributeList
		del self.wndPlayerStat
		if self.wndDragonSoul:
			del self.wndDragonSoul
		if self.wndDragonSoulRefine:
			del self.wndDragonSoulRefine
		del self.dlgExchange
		del self.dlgPointReset
		del self.dlgShop
		del self.dlgRestart
		del self.dlgSystem
		del self.dlgPassword
		del self.hyperlinkItemTooltip
		del self.tooltipItem
		del self.tooltipSkill
		del self.wndMiniMap
		del self.wndSafebox
		del self.wndMall
		del self.wndParty
		del self.wndHelp
		del self.wndCube
		del self.wndCubeResult
		del self.wndPlayerbotAdmin
		del self.offlineShopBuilder
		del self.offlineShopManage
		del self.offlineShopGuest
		del self.offlineShopHistory
		del self.offlineShopSearch
		del self.inputDialog
		del self.wndChatLog
		del self.dlgRefineNew
		del self.wndGuildBuilding
		del self.wndGameButton
		del self.tipBoard
		del self.bigBoard
		del self.fancyBoard
		del self.wndItemSelect
		del self.fishingGameDialog
		del self.potionRechargeDialog
		del self.busyActionDialog
		del self.maintenanceDialog
		del self.reportPlayerDialog
		del self.itemExchangeDialog
		del self.captchaDialog
		del self.gameMasterTargetDialog

		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			del self.wndAcceCombine
			del self.wndAcceAbsorption

		self.questButtonList = []
		self.whisperButtonList = []
		self.whisperDialogDict = {}
		self.privateShopAdvertisementBoardDict ={"player":{}, "offline":{}}
		self.reputationBarDict = {}
		self.guildScoreBoardDict = {}
		self.equipmentDialogDict = {}
		self.gameMaster_CaptchaDialogs = {}

		map(lambda wnd : wnd[1].Destroy(), self.interfaceWindowList.iteritems())
		self.interfaceWindowList = {}

		captcha.ClearAllCaptchaImage()
		uiChat.DestroyChatInputSetWindow()

	## Skill
	def OnUseSkill(self, slotIndex, coolTime):
		self.wndCharacter.OnUseSkill(slotIndex, coolTime)
		self.wndTaskBar.OnUseSkill(slotIndex, coolTime)
		self.wndGuild.OnUseSkill(slotIndex, coolTime)

	def OnActivateSkill(self, slotIndex):
		self.wndCharacter.OnActivateSkill(slotIndex)
		self.wndTaskBar.OnActivateSkill(slotIndex)

	def OnDeactivateSkill(self, slotIndex):
		self.wndCharacter.OnDeactivateSkill(slotIndex)
		self.wndTaskBar.OnDeactivateSkill(slotIndex)

	def OnChangeCurrentSkill(self, skillSlotNumber):
		self.wndTaskBar.OnChangeCurrentSkill(skillSlotNumber)

	def SelectMouseButtonEvent(self, dir, event):
		self.wndTaskBar.SelectMouseButtonEvent(dir, event)

	## Refresh
	def RefreshAlignment(self):
		self.wndCharacter.RefreshAlignment()

	def RefreshStatus(self):
		self.wndTaskBar.RefreshStatus()
		self.wndCharacter.RefreshStatus()
		self.wndInventory.RefreshGold()
		self.wndAttributeList.RefreshStatus()
		if self.wndEnergyBar:
			self.wndEnergyBar.RefreshStatus()
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoul.RefreshStatus()

	def RefreshStamina(self):
		self.wndTaskBar.RefreshStamina()

	def RefreshSkill(self):
		self.wndCharacter.RefreshSkill()
		self.wndTaskBar.RefreshSkill()

	def RefreshInventory(self):
		self.wndTaskBar.RefreshQuickSlot()
		if self.wndInventory:
			self.wndInventory.RefreshItemSlot()
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			if self.wndDragonSoul and self.wndDragonSoul.IsShow():
				self.wndDragonSoul.RefreshItemSlot()

	def RefreshCharacter(self):
		self.wndCharacter.RefreshCharacter()
		self.wndTaskBar.RefreshQuickSlot()

	def RefreshQuest(self):
		self.wndCharacter.RefreshQuest()

	def RefreshSafebox(self):
		self.wndSafebox.RefreshSafebox()

	# ITEM_MALL
	def RefreshMall(self):
		self.wndMall.RefreshMall()

	def OpenItemMall(self):
		if not self.mallPageDlg:
			self.mallPageDlg = uiShop.MallPageDialog()

		self.mallPageDlg.Open()
	# END_OF_ITEM_MALL

	def RefreshMessenger(self):
		self.wndMessenger.RefreshMessenger()

	def RefreshGuildInfoPage(self):
		self.wndGuild.RefreshGuildInfoPage()

	def RefreshGuildBoardPage(self):
		self.wndGuild.RefreshGuildBoardPage()

	def RefreshGuildMemberPage(self):
		self.wndGuild.RefreshGuildMemberPage()

	def RefreshGuildMemberPageGradeComboBox(self):
		self.wndGuild.RefreshGuildMemberPageGradeComboBox()

	def RefreshGuildSkillPage(self):
		self.wndGuild.RefreshGuildSkillPage()

	def RefreshGuildGradePage(self):
		self.wndGuild.RefreshGuildGradePage()

	def DeleteGuild(self):
		self.wndMessenger.ClearGuildMember()
		self.wndGuild.DeleteGuild()

	def OnBlockMode(self, mode):
		self.dlgSystem.OnBlockMode(mode)

	## Calling Functions
	# PointReset
	def OpenPointResetDialog(self):
		self.dlgPointReset.Show()
		self.dlgPointReset.SetTop()

	def ClosePointResetDialog(self):
		self.dlgPointReset.Close()

	# Shop
	def OpenShopDialog(self, vid):
		self.wndInventory.Show()
		self.wndInventory.SetTop()
		self.dlgShop.Open(vid)
		self.dlgShop.SetTop()

		eventManager.EventManager().send_event(eventManager.EVENT_MARK_SHOP_CURRENT, vid, False)

	def CloseShopDialog(self):
		self.dlgShop.Close()
		self.offlineShopGuest.CloseNormal()

	def RefreshShopDialog(self):
		self.dlgShop.Refresh()
		self.offlineShopGuest.RefreshNormal()

	## Quest
	def OpenCharacterWindowQuestPage(self):
		self.wndCharacter.Show()
		self.wndCharacter.SetState("QUEST")

	def OpenQuestWindow(self, skin, idx):

		wnds = ()

		q = uiQuest.QuestDialog(skin, idx)
		q.SetToolTip(self.tooltipItem)
		q.SetWindowName("QuestWindow" + str(idx))
		q.Show()
		if skin:
			q.Lock()
			wnds = self.__HideWindows()

			# UNKNOWN_UPDATE
			q.AddOnDoneEvent(lambda tmp_self, args=wnds: self.__ShowWindows(args))
			# END_OF_UNKNOWN_UPDATE

		if skin:
			q.AddOnCloseEvent(q.Unlock)
		q.AddOnCloseEvent(lambda key = self.wndQuestWindowNewKey:ui.__mem_func__(self.RemoveQuestDialog)(key))
		self.wndQuestWindow[self.wndQuestWindowNewKey] = q

		self.wndQuestWindowNewKey = self.wndQuestWindowNewKey + 1

		# END_OF_UNKNOWN_UPDATE

	def RemoveQuestDialog(self, key):
		del self.wndQuestWindow[key]

	## Exchange
	def StartExchange(self):
		self.dlgExchange.OpenDialog()
		self.dlgExchange.Refresh()

	def EndExchange(self):
		self.dlgExchange.CloseDialog()

	def RefreshExchange(self):
		self.dlgExchange.Refresh()

	## Party
	def AddPartyMember(self, pid, name):
		self.wndParty.AddPartyMember(pid, name)

		self.__ArrangeQuestButton()

	def UpdatePartyMemberInfo(self, pid):
		self.wndParty.UpdatePartyMemberInfo(pid)

	def RemovePartyMember(self, pid):
		self.wndParty.RemovePartyMember(pid)

		self.__ArrangeQuestButton()

	def LinkPartyMember(self, pid, vid):
		self.wndParty.LinkPartyMember(pid, vid)

	def UnlinkPartyMember(self, pid):
		self.wndParty.UnlinkPartyMember(pid)

	def UnlinkAllPartyMember(self):
		self.wndParty.UnlinkAllPartyMember()

	def ExitParty(self):
		self.wndParty.ExitParty()

		self.__ArrangeQuestButton()

	def PartyHealReady(self):
		self.wndParty.PartyHealReady()

	def ChangePartyParameter(self, distributionMode):
		self.wndParty.ChangePartyParameter(distributionMode)

	## Safebox
	def AskSafeboxPassword(self):
		if self.wndSafebox.IsShow():
			return

		# SAFEBOX_PASSWORD
		self.dlgPassword.SetTitle(localeInfo.PASSWORD_TITLE)
		self.dlgPassword.SetSendMessage("/safebox_password ")
		# END_OF_SAFEBOX_PASSWORD

		self.dlgPassword.ShowDialog()

	def OpenSafeboxWindow(self, size):
		self.dlgPassword.CloseDialog()
		self.wndSafebox.ShowWindow(size)

	def RefreshSafeboxMoney(self):
		self.wndSafebox.RefreshSafeboxMoney()

	def CommandCloseSafebox(self):
		self.wndSafebox.CommandCloseSafebox()

	# ITEM_MALL
	def AskMallPassword(self):
		if self.wndMall.IsShow():
			return
		self.dlgPassword.SetTitle(localeInfo.MALL_PASSWORD_TITLE)
		self.dlgPassword.SetSendMessage("/mall_password ")
		self.dlgPassword.ShowDialog()

	def OpenMallWindow(self, size):
		self.dlgPassword.CloseDialog()
		self.wndMall.ShowWindow(size)

	def CommandCloseMall(self):
		self.wndMall.CommandCloseMall()
	# END_OF_ITEM_MALL

	## Guild
	def OnStartGuildWar(self, guildSelf, guildOpp):
		self.wndGuild.OnStartGuildWar(guildSelf, guildOpp)

		guildWarScoreBoard = uiGuild.GuildWarScoreBoard()
		guildWarScoreBoard.Open(guildSelf, guildOpp)
		guildWarScoreBoard.Show()
		self.guildScoreBoardDict[uiGuild.GetGVGKey(guildSelf, guildOpp)] = guildWarScoreBoard

	def OnEndGuildWar(self, guildSelf, guildOpp):
		self.wndGuild.OnEndGuildWar(guildSelf, guildOpp)

		key = uiGuild.GetGVGKey(guildSelf, guildOpp)

		if not self.guildScoreBoardDict.has_key(key):
			return

		self.guildScoreBoardDict[key].Destroy()
		del self.guildScoreBoardDict[key]

	# GUILDWAR_MEMBER_COUNT
	def UpdateMemberCount(self, gulidID1, memberCount1, guildID2, memberCount2):
		key = uiGuild.GetGVGKey(gulidID1, guildID2)

		if not self.guildScoreBoardDict.has_key(key):
			return

		self.guildScoreBoardDict[key].UpdateMemberCount(gulidID1, memberCount1, guildID2, memberCount2)
	# END_OF_GUILDWAR_MEMBER_COUNT

	def OnRecvGuildWarPoint(self, gainGuildID, opponentGuildID, point):
		key = uiGuild.GetGVGKey(gainGuildID, opponentGuildID)
		if not self.guildScoreBoardDict.has_key(key):
			return

		guildBoard = self.guildScoreBoardDict[key]
		guildBoard.SetScore(gainGuildID, opponentGuildID, point)

	## PK Mode
	def OnChangePKMode(self):
		self.wndCharacter.RefreshAlignment()
		self.dlgSystem.OnChangePKMode()

	## Refine
	def OpenRefineDialog(self, targetItemPos, nextGradeItemVnum, cost, prob, type):
		self.dlgRefineNew.Open(targetItemPos, nextGradeItemVnum, cost, prob, type)

	def AppendMaterialToRefineDialog(self, vnum, count):
		self.dlgRefineNew.AppendMaterial(vnum, count)

	## Show & Hide
	def ShowDefaultWindows(self):
		self.wndTaskBar.Show()
		self.wndMiniMap.Show()
		self.wndMiniMap.ShowMiniMap()
		if self.wndEnergyBar:
			self.wndEnergyBar.Show()

	def ShowAllWindows(self):
		self.wndTaskBar.Show()
		self.wndCharacter.Show()
		self.wndInventory.Show()
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoul.Show()
			self.wndDragonSoulRefine.Show()
		self.wndChat.Show()
		self.wndMiniMap.Show()
		if self.wndEnergyBar:
			self.wndEnergyBar.Show()
		if self.wndExpandedTaskBar:
			self.wndExpandedTaskBar.Show()
			self.wndExpandedTaskBar.SetTop()

	def HideAllWindows(self):
		if self.wndTaskBar:
			self.wndTaskBar.Hide()

		if self.wndEnergyBar:
			self.wndEnergyBar.Hide()

		if self.wndCharacter:
			self.wndCharacter.Hide()

		if self.wndInventory:
			self.wndInventory.Hide()

		if self.wndItemShop:
			self.wndItemShop.Hide()

		if self.wndCrafting:
			self.wndCrafting.Hide()

		if self.wndSpecialShop:
			self.wndSpecialShop.Hide()

		if self.wndAttributeList:
			self.wndAttributeList.Hide()

		if self.wndPlayerStat:
			self.wndPlayerStat.Hide()

		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoul.Hide()
			self.wndDragonSoulRefine.Hide()

		if self.wndChat:
			self.wndChat.Hide()

		if self.wndMiniMap:
			self.wndMiniMap.Hide()

		if self.wndMessenger:
			self.wndMessenger.Hide()

		if self.wndGuild:
			self.wndGuild.Hide()

		if self.wndExpandedTaskBar:
			self.wndExpandedTaskBar.Hide()

		if app.ENABLE_MOVE_CHANNEL and self.wndMoveChannel:
			self.wndMoveChannel.Hide()

		if app.ENABLE_WON_EXCHANGE_WINDOW:
			self.wndWonExchange.Hide()

		map(lambda wnd : wnd[1].Close(), self.interfaceWindowList.iteritems())

	def ShowMouseImage(self):
		self.wndTaskBar.ShowMouseImage()

	def HideMouseImage(self):
		self.wndTaskBar.HideMouseImage()

	def ToggleChat(self):
		if True == self.wndChat.IsEditMode():
			self.wndChat.CloseChat()
		else:
			if self.wndWeb and self.wndWeb.IsShow():
				pass
			else:
				self.wndChat.OpenChat()

	def IsOpenChat(self):
		return self.wndChat.IsEditMode()

	def SetChatFocus(self):
		self.wndChat.SetChatFocus()

	def OpenRestartDialog(self):
		self.dlgRestart.OpenDialog()
		self.dlgRestart.SetTop()

	def CloseRestartDialog(self):
		self.dlgRestart.Close()

	def ToggleSystemDialog(self):
		if False == self.dlgSystem.IsShow():
			self.dlgSystem.OpenDialog()
			self.dlgSystem.SetTop()
		else:
			self.dlgSystem.Close()

	def OpenSystemDialog(self):
		self.dlgSystem.OpenDialog()
		self.dlgSystem.SetTop()

	def ToggleMessenger(self):
		if self.wndMessenger.IsShow():
			self.wndMessenger.Hide()
		else:
			self.wndMessenger.SetTop()
			self.wndMessenger.Show()

	def ToggleTaskbarVisibility(self):
		if self.wndTaskBar.IsShow():
			self.wndTaskBar.Hide()
		else:
			self.wndTaskBar.Show()

	def ToggleMinimapVisibility(self):
		if self.wndMiniMap.IsShow():
			self.wndMiniMap.Hide()
		else:
			self.wndMiniMap.Show()

	def ToggleGameButtonVisibility(self):
		if self.wndGameButton.IsShow():
			self.wndGameButton.Hide()
		else:
			self.wndGameButton.Show()

	def ToggleMiniMap(self):
		if app.IsPressed(app.DIK_LSHIFT) or app.IsPressed(app.DIK_RSHIFT):
			if False == self.wndMiniMap.isShowMiniMap():
				self.wndMiniMap.ShowMiniMap()
				self.wndMiniMap.SetTop()
			else:
				self.wndMiniMap.HideMiniMap()

		else:
			self.wndMiniMap.ToggleAtlasWindow()

	def PressMKey(self):
		if app.IsPressed(app.DIK_LALT) or app.IsPressed(app.DIK_RALT):
			self.ToggleMessenger()

		else:
			self.ToggleMiniMap()

	def SetMapName(self, mapName):
		self.wndMiniMap.SetMapName(mapName)

	def MiniMapScaleUp(self):
		self.wndMiniMap.ScaleUp()

	def MiniMapScaleDown(self):
		self.wndMiniMap.ScaleDown()

	def ToggleCharacterWindow(self, state):
		if False == player.IsObserverMode():
			if False == self.wndCharacter.IsShow():
				self.OpenCharacterWindowWithState(state)
			else:
				if state == self.wndCharacter.GetState():
					self.wndCharacter.OverOutItem()
					self.wndCharacter.Hide()
				else:
					self.wndCharacter.SetState(state)

	def OpenCharacterWindowWithState(self, state):
		if False == player.IsObserverMode():
			self.wndCharacter.SetState(state)
			self.wndCharacter.Show()
			self.wndCharacter.SetTop()

	def ToggleCharacterWindowStatusPage(self):
		self.ToggleCharacterWindow("STATUS")

	def ToggleInventoryWindow(self):
		if False == player.IsObserverMode():
			if False == self.wndInventory.IsShow():
				self.wndInventory.Show()
				self.wndInventory.SetTop()
			else:
				self.wndInventory.OverOutItem()
				self.wndInventory.Close()

	# ---- Panel GM (F9) / admin botow (F10) --------------------------------
	def ToggleGMPanelWindow(self):
		# Wolane WYLACZNIE po odpowiedzi serwera na /gmpanel_open (game.py
		# __GMPanel_Open) - serwer sprawdza gm_level przy kazdym nacisnieciu,
		# wiec nie ma tu zadnej bramki client-side.
		if not self.wndGMPanel:
			import chat
			chat.AppendChat(chat.CHAT_TYPE_INFO, "Panel GM nie zaladowal sie w tym kliencie - szczegoly w syserr.txt")
			return
		if False == self.wndGMPanel.IsShow():
			self.wndGMPanel.Show()
			self.wndGMPanel.SetTop()
		else:
			self.wndGMPanel.Hide()

	def OpenPlayerbotAdminWindow(self):
		if not self.wndPlayerbotAdmin:
			import chat
			chat.AppendChat(chat.CHAT_TYPE_INFO, "Okno admina botow nie zaladowalo sie w tym kliencie - szczegoly w syserr.txt")
			return
		self.wndPlayerbotAdmin.Open()

	# uitarget.TargetBoard "Sprawdz" (GM-only) - zakladka "Sprawdz Gracza"
	# z tym celem juz wpisanym.
	def OpenGMLookupFor(self, name):
		if not self.wndGMPanel:
			return
		self.wndGMPanel.Show()
		self.wndGMPanel.SetTop()
		self.wndGMPanel.OpenLookupFor(name)

	# uitarget.TargetBoard "EQ" (GM-only): to samo natywne EquipmentDialog co
	# /view_equip, ale wypelnione z tekstowej odpowiedzi do_gmpanel_view_equip
	# (GMEquipChunk) zamiast z binarnego pakietu.
	def OpenGMEquipFor(self, vid, name):
		vid = int(vid)
		self.OpenEquipmentDialog(vid)
		self._gmEquipBuffers[vid] = ""
		net.SendChatPacket("/gmpanel_view_equip %d" % vid)

	# "GMEquipChunk <vid> <isLast> <data>" - wpisy
	# "<slot>:<vnum>:<count>:<s0>:<s1>:<s2>:<t0>:<v0>:...:<t6>:<v6>;"
	# (3 gniazda, 7 par typ/wartosc bonusu). SetEquipmentDialogItem musi byc
	# PRZED gniazdami/bonusami tego slotu - resetuje itemDataDict[slot].
	def SetGMEquipChunk(self, vid, isLast, data):
		vid = int(vid)
		if vid not in self._gmEquipBuffers:
			return
		self._gmEquipBuffers[vid] += data

		if isLast != "1":
			return

		raw = self._gmEquipBuffers.pop(vid)
		for entry in raw.split(";"):
			if not entry:
				continue
			bits = entry.split(":")
			if len(bits) != 20:
				continue
			try:
				nums = [int(b) for b in bits]
			except ValueError:
				continue

			slotIndex, vnum, count = nums[0], nums[1], nums[2]
			sockets = nums[3:6]
			attrPairs = nums[6:20]

			self.SetEquipmentDialogItem(vid, slotIndex, vnum, count)
			for socketIndex, value in enumerate(sockets):
				self.SetEquipmentDialogSocket(vid, slotIndex, socketIndex, value)
			for attrIndex in range(7):
				aType = attrPairs[attrIndex * 2]
				aValue = attrPairs[attrIndex * 2 + 1]
				self.SetEquipmentDialogAttr(vid, slotIndex, attrIndex, aType, aValue)

	def ToggleExpandedButton(self):
		if False == player.IsObserverMode():
			if False == self.wndExpandedTaskBar.IsShow():
				self.wndExpandedTaskBar.Show()
				self.wndExpandedTaskBar.SetTop()
			else:
				self.wndExpandedTaskBar.Close()

	def DragonSoulActivate(self, deck):
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoul.ActivateDragonSoulByExtern(deck)

	def DragonSoulDeactivate(self):
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoul.DeactivateDragonSoul()

	def Highligt_Item(self, inven_type, inven_pos):
		if player.DRAGON_SOUL_INVENTORY == inven_type:
			if app.ENABLE_DRAGON_SOUL_SYSTEM:
				self.wndDragonSoul.HighlightSlot(inven_pos)

		elif app.ENABLE_HIGHLIGHT_NEW_ITEM and player.SLOT_TYPE_INVENTORY == inven_type:
			self.wndInventory.HighlightSlot(inven_pos)


	def DragonSoulGiveQuilification(self):
		self.DRAGON_SOUL_IS_QUALIFIED = True
		if self.wndExpandedTaskBar:
			self.wndExpandedTaskBar.SetToolTipText(uiTaskBar.ExpandedTaskBar.BUTTON_DRAGON_SOUL, uiScriptLocale.TASKBAR_DRAGON_SOUL)

	def ToggleDragonSoulWindow(self):
		if False == player.IsObserverMode():
			if app.ENABLE_DRAGON_SOUL_SYSTEM:
				if False == self.wndDragonSoul.IsShow():
					if self.DRAGON_SOUL_IS_QUALIFIED:
						self.wndDragonSoul.Show()
					else:
						try:
							self.wndPopupDialog.SetText(localeInfo.DRAGON_SOUL_UNQUALIFIED)
							self.wndPopupDialog.Open()
						except:
							self.wndPopupDialog = uiCommon.PopupDialog()
							self.wndPopupDialog.SetText(localeInfo.DRAGON_SOUL_UNQUALIFIED)
							self.wndPopupDialog.Open()
				else:
					self.wndDragonSoul.Close()

	def ToggleDragonSoulWindowWithNoInfo(self):
		if False == player.IsObserverMode():
			if app.ENABLE_DRAGON_SOUL_SYSTEM:
				if False == self.wndDragonSoul.IsShow():
					if self.DRAGON_SOUL_IS_QUALIFIED:
						self.wndDragonSoul.Show()
				else:
					self.wndDragonSoul.Close()

	def FailDragonSoulRefine(self, reason, inven_type, inven_pos):
		if False == player.IsObserverMode():
			if app.ENABLE_DRAGON_SOUL_SYSTEM:
				if True == self.wndDragonSoulRefine.IsShow():
					self.wndDragonSoulRefine.RefineFail(reason, inven_type, inven_pos)

	def SucceedDragonSoulRefine(self, inven_type, inven_pos):
		if False == player.IsObserverMode():
			if app.ENABLE_DRAGON_SOUL_SYSTEM:
				if True == self.wndDragonSoulRefine.IsShow():
					self.wndDragonSoulRefine.RefineSucceed(inven_type, inven_pos)

	def OpenDragonSoulRefineWindow(self):
		if False == player.IsObserverMode():
			if app.ENABLE_DRAGON_SOUL_SYSTEM:
				if False == self.wndDragonSoulRefine.IsShow():
					self.wndDragonSoulRefine.Show()
					if None != self.wndDragonSoul:
						if False == self.wndDragonSoul.IsShow():
							self.wndDragonSoul.Show()

	def CloseDragonSoulRefineWindow(self):
		if False == player.IsObserverMode():
			if app.ENABLE_DRAGON_SOUL_SYSTEM:
				if True == self.wndDragonSoulRefine.IsShow():
					self.wndDragonSoulRefine.Close()


	def ToggleGuildWindow(self):
		if not self.wndGuild.IsShow():
			if self.wndGuild.CanOpen():
				self.wndGuild.Open()
			else:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.GUILD_YOU_DO_NOT_JOIN)
		else:
			self.wndGuild.OverOutItem()
			self.wndGuild.Hide()

	def ToggleChatLogWindow(self):
		if self.wndChatLog.IsShow():
			self.wndChatLog.Hide()
		else:
			self.wndChatLog.Show()

	def CheckGameButton(self):
		if self.wndGameButton:
			self.wndGameButton.CheckGameButton()

	def __OnClickStatusPlusButton(self):
		self.ToggleCharacterWindow("STATUS")

	def __OnClickSkillPlusButton(self):
		self.ToggleCharacterWindow("SKILL")

	def __OnClickQuestButton(self):
		self.ToggleCharacterWindow("QUEST")

	def __OnClickHelpButton(self):
		player.SetPlayTime(1)
		self.CheckGameButton()
		self.OpenHelpWindow()

	def __OnClickBuildButton(self):
		self.BUILD_OpenWindow()

	def OpenHelpWindow(self):
		self.wndUICurtain.Show()
		self.wndHelp.Open()

	def CloseHelpWindow(self):
		self.wndUICurtain.Hide()
		self.wndHelp.Close()

	def OpenWebWindow(self, url):
		self.wndWeb.Open(url)

		self.wndChat.CloseChat()

	# show GIFT
	def ShowGift(self):
		self.wndTaskBar.ShowGift()

	def CloseWbWindow(self):
		self.wndWeb.Close()

	def OpenCubeWindow(self):
		self.wndCube.Open()

		if False == self.wndInventory.IsShow():
			self.wndInventory.Show()

	def UpdateCubeInfo(self, gold, itemVnum, count):
		self.wndCube.UpdateInfo(gold, itemVnum, count)

	def CloseCubeWindow(self):
		self.wndCube.Close()

	def FailedCubeWork(self):
		self.wndCube.Refresh()

	def SucceedCubeWork(self, itemVnum, count):
		self.wndCube.Clear()

		if 0:
			self.wndCubeResult.SetPosition(*self.wndCube.GetGlobalPosition())
			self.wndCubeResult.SetCubeResultItem(itemVnum, count)
			self.wndCubeResult.Open()
			self.wndCubeResult.SetTop()

	if app.ENABLE_MOVE_CHANNEL:
		def ToggleMoveChannelWindow(self):
			if not player.IsObserverMode():
				if not self.wndMoveChannel.IsShow():
					self.wndMoveChannel.Open()
				else:
					self.wndMoveChannel.Hide()

	if app.ENABLE_WON_EXCHANGE_WINDOW:
		def ToggleWonExchangeWindow(self):
			if player.IsObserverMode():
				return

			if not self.wndWonExchange.IsShow():
				self.wndWonExchange.Open()
				self.wndWonExchange.SetTop()
			else:
				self.wndWonExchange.Close()

	if app.ENABLE_ACCE_COSTUME_SYSTEM:
		def ActAcce(self, iAct, bWindow):
			board = (self.wndAcceAbsorption,self.wndAcceCombine)[int(bWindow)]
			if iAct == 1:
				self.ActAcceOpen(board)
			elif iAct == 2:
				self.ActAcceClose(board)
			elif iAct == 3 or iAct == 4:
				self.ActAcceRefresh(board, iAct)

		def ActAcceOpen(self,board):
			if not board.IsOpened():
				board.Open()
			if not self.wndInventory.IsShow():
				self.wndInventory.Show()
			self.wndInventory.RefreshBagSlotWindow()

		def ActAcceClose(self,board):
			if board.IsOpened():
				board.Close()
			self.wndInventory.RefreshBagSlotWindow()

		def ActAcceRefresh(self,board,iAct):
			if board.IsOpened():
				board.Refresh(iAct)
			self.wndInventory.RefreshBagSlotWindow()

	def __HideWindows(self):
		hideWindows = self.wndTaskBar,\
						self.wndCharacter,\
						self.wndInventory,\
						self.wndMiniMap,\
						self.wndGuild,\
						self.wndMessenger,\
						self.wndChat,\
						self.wndParty,\
						self.wndGameButton, \
					  	self.wndItemShop

		if self.wndEnergyBar:
			hideWindows += self.wndEnergyBar,

		if self.wndExpandedTaskBar:
			hideWindows += self.wndExpandedTaskBar,

		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			hideWindows += self.wndDragonSoul,\
						self.wndDragonSoulRefine,

		if app.ENABLE_MOVE_CHANNEL and self.wndMoveChannel:
			hideWindows += self.wndMoveChannel,
		
		for window in self.interfaceWindowList.values():
			hideWindows += window,

		hideWindows = filter(lambda x: x != None and x.IsShow(), hideWindows)
		map(lambda x: x.Hide(), hideWindows)

		self.HideAllQuestButton()
		self.HideAllWhisperButton()

		if self.wndChat.IsEditMode():
			self.wndChat.CloseChat()

		return hideWindows

	def __ShowWindows(self, wnds):
		map(lambda x:x.Show(), wnds)
		global IsQBHide
		if not IsQBHide:
			self.ShowAllQuestButton()
		else:
			self.HideAllQuestButton()

		self.ShowAllWhisperButton()

	def BINARY_OpenAtlasWindow(self):
		if self.wndMiniMap:
			self.wndMiniMap.ShowAtlas()

	def BINARY_SetObserverMode(self, flag):
		self.wndGameButton.SetObserverMode(flag)

	# ACCESSORY_REFINE_ADD_METIN_STONE
	def BINARY_OpenSelectItemWindow(self):
		self.wndItemSelect.Open()
	# END_OF_ACCESSORY_REFINE_ADD_METIN_STONE

	def AppearReputationBar(self, vid, fraction):
		if self.reputationBarDict.has_key(vid):
			self.reputationBarDict[vid].Open(vid, fraction)
			return

		self.reputationBarDict[vid] = uiReputation.ReputationBar()
		self.reputationBarDict[vid].Open(vid, fraction)

	def DisappearReputationBar(self, vid):
		if self.reputationBarDict.has_key(vid):
			self.reputationBarDict[vid].Hide()

	#####################################################################################
	### Private Shop ###

	def OpenPrivateShopInputNameDialog(self):
		#if player.IsInSafeArea():
		#	chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.CANNOT_OPEN_PRIVATE_SHOP_IN_SAFE_AREA)
		#	return

		inputDialog = uiCommon.InputDialog()
		inputDialog.SetTitle(localeInfo.PRIVATE_SHOP_INPUT_NAME_DIALOG_TITLE)
		inputDialog.SetMaxLength(32)
		inputDialog.SetAcceptEvent(ui.__mem_func__(self.OpenPrivateShopBuilder))
		inputDialog.SetCancelEvent(ui.__mem_func__(self.ClosePrivateShopInputNameDialog))
		inputDialog.Open()
		self.inputDialog = inputDialog

	def ClosePrivateShopInputNameDialog(self):
		self.inputDialog = None
		return True

	def OpenPrivateShopBuilder(self):

		if not self.inputDialog:
			return True

		if not len(self.inputDialog.GetText()):
			return True

		self.offlineShopBuilder.Open(self.inputDialog.GetText())
		return True

	def OpenPrivateShopManage(self):
		self.offlineShopManage.Toggle()
		return True

	def ClickPrivateShop(self, vid, is_offline):
		if self.offlineShopGuest.IsShowNormal():
			self.offlineShopGuest.Close()
			eventManager.EventManager().send_delayed_event(uiShop.EVENT_CLICK_PRIVATE_SHOP, 0.15, vid, is_offline)
			return
		elif self.offlineShopGuest.IsShow():
			self.offlineShopGuest.Close()

		if is_offline:
			ikashop.SendOnClickPacket(vid)
		else:
			net.SendOnClickPacket(vid)

	def MarkPrivateShopAsViewed(self, vid, is_offline=False):
		shop_type = "offline" if is_offline else "player"
		if self.privateShopAdvertisementBoardDict[shop_type].has_key(vid):
			self.privateShopAdvertisementBoardDict[shop_type][vid].MarkAsViewed()

	def MarkPrivateShopAsCurrent(self, vid, is_offline=False):
		shop_type = "offline" if is_offline else "player"
		if self.privateShopAdvertisementBoardDict[shop_type].has_key(vid):
			self.privateShopAdvertisementBoardDict[shop_type][vid].MarkAsCurrent()

	def AppearPrivateShop(self, vid, text, is_offline_shop=False):

		board = uiPrivateShopBuilder.PrivateShopAdvertisementBoard()
		board.Open(vid, text, is_offline_shop)

		shop_type = "offline" if is_offline_shop else "player"
		self.privateShopAdvertisementBoardDict[shop_type][vid] = board

	def DisappearPrivateShop(self, vid, is_offline_shop=False):
		shop_type = "offline" if is_offline_shop else "player"
		if not self.privateShopAdvertisementBoardDict[shop_type].has_key(vid):
			return
		del self.privateShopAdvertisementBoardDict[shop_type][vid]

		uiPrivateShopBuilder.DeleteADBoard(vid, is_offline_shop)

	def ToggleOfflineShopVisibility(self, vid, state):
		if not self.privateShopAdvertisementBoardDict["offline"].has_key(vid):
			return

		self.privateShopAdvertisementBoardDict["offline"][vid].SetVisible(state)

	#####################################################################################
	### Equipment ###

	def OpenEquipmentDialog(self, vid):
		dlg = uiEquipmentDialog.EquipmentDialog()
		dlg.SetItemToolTip(self.tooltipItem)
		dlg.SetCloseEvent(ui.__mem_func__(self.CloseEquipmentDialog))
		dlg.Open(vid)

		self.equipmentDialogDict[vid] = dlg

	def SetEquipmentDialogItem(self, vid, slotIndex, vnum, count):
		if not vid in self.equipmentDialogDict:
			return
		self.equipmentDialogDict[vid].SetEquipmentDialogItem(slotIndex, vnum, count)

	def SetEquipmentDialogSocket(self, vid, slotIndex, socketIndex, value):
		if not vid in self.equipmentDialogDict:
			return
		self.equipmentDialogDict[vid].SetEquipmentDialogSocket(slotIndex, socketIndex, value)

	def SetEquipmentDialogAttr(self, vid, slotIndex, attrIndex, type, value):
		if not vid in self.equipmentDialogDict:
			return
		self.equipmentDialogDict[vid].SetEquipmentDialogAttr(slotIndex, attrIndex, type, value)

	def CloseEquipmentDialog(self, vid):
		if not vid in self.equipmentDialogDict:
			return
		del self.equipmentDialogDict[vid]

	#####################################################################################

	#####################################################################################
	### Quest ###
	def BINARY_ClearQuest(self, index):
		btn = self.__FindQuestButton(index)
		if 0 != btn:
			self.__DestroyQuestButton(btn)

	def RecvQuest(self, index, name):
		# QUEST_LETTER_IMAGE
		self.BINARY_RecvQuest(index, name, "file", localeInfo.GetLetterImageName())
		# END_OF_QUEST_LETTER_IMAGE

	def BINARY_RecvQuest(self, index, name, iconType, iconName):

		btn = self.__FindQuestButton(index)
		if 0 != btn:
			self.__DestroyQuestButton(btn)

		btn = uiWhisper.WhisperButton()

		# QUEST_LETTER_IMAGE
		import item
		if "item"==iconType:
			item.SelectItem(int(iconName))
			buttonImageFileName=item.GetIconImageFileName()
		else:
			buttonImageFileName=iconName

		if iconName and (iconType not in ("item", "file")):  # type "ex" implied
			btn.SetUpVisual("d:/ymir work/ui/game/quest/questicon/%s.tga" % (iconName.replace("open", "close")))
			btn.SetOverVisual("d:/ymir work/ui/game/quest/questicon/%s.tga" % (iconName))
			btn.SetDownVisual("d:/ymir work/ui/game/quest/questicon/%s.tga" % (iconName))
		else:
			btn.SetUpVisual("d:/ymir work/ui/game/quest/questicon/scroll_close.tga")
			btn.SetDownVisual("d:/ymir work/ui/game/quest/questicon/scroll_open.tga")
			btn.SetOverVisual("d:/ymir work/ui/game/quest/questicon/scroll_open.tga")
		# END_OF_QUEST_LETTER_IMAGE

		btn.SetToolTipText(name, -20, 35)
		btn.ToolTipText.SetHorizontalAlignLeft()

		listOfTypes = iconType.split(",")
		if "blink" in listOfTypes:
			btn.Flash()

		listOfColors = {
			"golden":	0xFFffa200,
			"green":	0xFF00e600,
			"blue":		0xFF0099ff,
			"purple":	0xFFcc33ff,

			"fucsia":	0xFFcc0099,
			"aqua":		0xFF00ffff,
		}
		for k,v in listOfColors.iteritems():
			if k in listOfTypes:
				btn.ToolTipText.SetPackedFontColor(v)

		btn.SetEvent(ui.__mem_func__(self.__StartQuest), btn)
		btn.Show()

		btn.index = index
		btn.name = name

		self.questButtonList.insert(0, btn)
		self.__ArrangeQuestButton()

	def __ArrangeQuestButton(self):

		screenWidth = wndMgr.GetScreenWidth()
		screenHeight = wndMgr.GetScreenHeight()

		if self.wndParty.IsShow():
			xPos = 100 + 30
		else:
			xPos = 20

		if localeInfo.IsARABIC():
			xPos = xPos + 15

		yPos = 170 * screenHeight / 600
		yCount = (screenHeight - 330) / 63

		count = 0
		for btn in self.questButtonList:

			btn.SetPosition(xPos + (int(count/yCount) * 100), yPos + (count%yCount * 63))
			count += 1
			global IsQBHide
			if IsQBHide:
				btn.Hide()
			else:
				btn.Show()

	def __StartQuest(self, btn):
		event.QuestButtonClick(btn.index)
		self.__DestroyQuestButton(btn)

	def __FindQuestButton(self, index):
		for btn in self.questButtonList:
			if btn.index == index:
				return btn

		return 0

	def __DestroyQuestButton(self, btn):
		btn.SetEvent(0)
		self.questButtonList.remove(btn)
		self.__ArrangeQuestButton()

	def HideAllQuestButton(self):
		for btn in self.questButtonList:
			btn.Hide()

	def ShowAllQuestButton(self):
		for btn in self.questButtonList:
			btn.Show()
	#####################################################################################

	#####################################################################################
	### Whisper ###

	def __UpdateUserBlockState(self, name, isBlock):
		whisper = self.__GetWhisper(name)
		if not whisper or not whisper["dialog"]:
			return

		whisper["dialog"].RefreshIgnoreButton(isBlock)

	def __InitWhisper(self):
		#chat.InitWhisper(self)

		player_name = player.GetMainCharacterName()
		if constInfo.WHISPER_DICT.has_key(player_name):
			for target_name, whisper_data in constInfo.WHISPER_DICT[player_name].items():
				if self.__FindWhisperButton(target_name) == 0:
					self.__MakeWhisperButton(target_name, whisper_data["mode"])

					for msg_data in whisper_data["history"]:
						mode = msg_data[0]
						msg = msg_data[1]
						chat.AppendWhisper(mode, target_name, msg)

	def OpenWhisperDialogWithoutTarget(self):
		if not self.dlgWhisperWithoutTarget:
			dlgWhisper = uiWhisper.WhisperDialog(self.MinimizeWhisperDialog, self.CloseWhisperDialog)
			dlgWhisper.BindInterface(self)
			dlgWhisper.LoadDialog()
			dlgWhisper.OpenWithoutTarget(self.RegisterTemporaryWhisperDialog)
			dlgWhisper.SetPosition(self.windowOpenPosition*30,self.windowOpenPosition*30)
			dlgWhisper.Show()
			self.dlgWhisperWithoutTarget = dlgWhisper

			self.windowOpenPosition = (self.windowOpenPosition+1) % 5

		else:
			self.dlgWhisperWithoutTarget.SetTop()
			self.dlgWhisperWithoutTarget.OpenWithoutTarget(self.RegisterTemporaryWhisperDialog)

	def RegisterTemporaryWhisperDialog(self, name):
		if not self.dlgWhisperWithoutTarget:
			return

		btn = self.__FindWhisperButton(name)
		if 0 != btn:
			self.__DestroyWhisperButton(btn)

		self.__DestroyWhisper(name)
		self.__AddNewWhisper(name, self.dlgWhisperWithoutTarget)
		self.dlgWhisperWithoutTarget.OpenWithTarget(name)
		self.dlgWhisperWithoutTarget = None
		self.__CheckGameMaster(name)

	def OpenWhisperDialog(self, name, targetEmpire=0):
		whisper = self.__GetWhisper(name)
		if not whisper or not whisper["dialog"]:
			dlg = self.__MakeWhisperDialog(name)
			dlg.OpenWithTarget(name, targetEmpire)
			dlg.chatLine.SetFocus()
			dlg.Show()

			self.__CheckGameMaster(name)
			btn = self.__FindWhisperButton(name)
			if 0 != btn:
				self.__DestroyWhisperButton(btn)

	def PushWhisperMessageStack(self, name, mode, text):
		if len(text) <= 0:
			return

		data = self.__GetWhisper(name)
		if not data:
			return

		sentence_stack = data["history"]

		LAST_SENTENCE_STACK_SIZE = 12
		if len(sentence_stack) > LAST_SENTENCE_STACK_SIZE:
			sentence_stack.pop(0)

		sentence_stack.append([mode, text])
		constInfo.WHISPER_DICT[player.GetMainCharacterName()][name] = data

	def RecvWhisper(self, name, mode=chat.WHISPER_TYPE_NORMAL):
		whisper = self.__GetWhisper(name)
		if whisper and whisper["dialog"]:
			if self.IsGameMasterName(name):
				whisper["dialog"].SetGameMasterLook()
		else:
			btn = self.__FindWhisperButton(name)
			if 0 == btn:
				btn = self.__MakeWhisperButton(name, mode)
				chat.AppendChat(chat.CHAT_TYPE_NOTICE, localeInfo.RECEIVE_MESSAGE % (name))

			btn.Flash()

	def ShowWhisperDialog(self, btn):
		try:
			dlgWhisper = self.__MakeWhisperDialog(btn.name)
			dlgWhisper.OpenWithTarget(btn.name)
			dlgWhisper.Show()
			self.__CheckGameMaster(btn.name)
		except:
			import dbg
			dbg.TraceError("interface.ShowWhisperDialog - Failed to find key")

		self.__DestroyWhisperButton(btn)

	def MinimizeWhisperDialog(self, name):
		if 0 != name:
			whisper = self.__GetWhisper(name)
			mode = chat.WHISPER_TYPE_NORMAL
			if whisper:
				mode = whisper["mode"]
			self.__MakeWhisperButton(name, mode)

		return self.CloseWhisperDialog(name, False)

	def CloseWhisperDialog(self, name, destroy_data=True):
		if 0 == name:
			if self.dlgWhisperWithoutTarget:
				self.dlgWhisperWithoutTarget.Destroy()
				self.dlgWhisperWithoutTarget = None
			return True

		try:
			self.__DestroyWhisper(name, destroy_data)
			return True
		except:
			import dbg
			dbg.TraceError("interface.CloseWhisperDialog - Failed to find key")

		return False

	def __ArrangeWhisperButton(self):

		screenWidth = wndMgr.GetScreenWidth()
		screenHeight = wndMgr.GetScreenHeight()

		xPos = screenWidth - 70
		yPos = 170 * screenHeight / 600
		yCount = (screenHeight - 330) / 63
		#yCount = (screenHeight - 285) / 63

		count = 0
		for button in self.whisperButtonList:

			button.SetPosition(xPos + (int(count/yCount) * -50), yPos + (count%yCount * 63))
			count += 1

	def __FindWhisperButton(self, name):
		for button in self.whisperButtonList:
			if button.name == name:
				return button

		return 0

	def __GetWhisper(self, target_name):
		player_name = player.GetMainCharacterName()
		if not constInfo.WHISPER_DICT.has_key(player_name):
			return None

		if not constInfo.WHISPER_DICT[player_name].has_key(target_name):
			return None

		return constInfo.WHISPER_DICT[player_name][target_name]

	def __DestroyWhisper(self, target_name, destroy_data=True):
		whisper = self.__GetWhisper(target_name)
		if whisper and whisper["dialog"]:
			player_name = player.GetMainCharacterName()
			constInfo.WHISPER_DICT[player_name][target_name]["dialog"].Destroy()
			if destroy_data:
				constInfo.WHISPER_DICT[player_name].pop(target_name)

	def __AddNewWhisper(self, target_name, dialog, mode=0):
		player_name = player.GetMainCharacterName()
		if not constInfo.WHISPER_DICT.has_key(player_name):
			constInfo.WHISPER_DICT[player_name] = {}

		data = constInfo.WHISPER_DICT[player_name]
		if not data.has_key(target_name):
			data[target_name] = {
				"dialog": dialog,
				"message_stack": [],
				"history": [],
				"mode": mode,
			}
		else:
			data[target_name]["dialog"] = dialog

		constInfo.WHISPER_DICT[player_name] = data

	def __MakeWhisperDialog(self, name):
		dlgWhisper = uiWhisper.WhisperDialog(self.MinimizeWhisperDialog, self.CloseWhisperDialog)
		dlgWhisper.BindInterface(self)
		dlgWhisper.LoadDialog()
		dlgWhisper.SetPosition(self.windowOpenPosition * 30, self.windowOpenPosition * 30)
		self.__AddNewWhisper(name, dlgWhisper)

		self.windowOpenPosition = (self.windowOpenPosition + 1) % 5

		return dlgWhisper

	def __MakeWhisperButton(self, name, mode=chat.WHISPER_TYPE_NORMAL):
		whisperButton = uiWhisper.WhisperButton()

		img_by_type_dict = {
			chat.WHISPER_TYPE_NORMAL: (None, "d:/ymir work/ui/game/windows/btn_mail_up.sub"),
			chat.WHISPER_TYPE_GM: (0xffffa200, flamewindPath.GetPublic("whisper_gm")),
			chat.WHISPER_TYPE_MYSHOP: (0xffffc2d2, flamewindPath.GetPublic("whisper_shop")),
		}

		if not img_by_type_dict.has_key(mode):
			mode = chat.WHISPER_TYPE_NORMAL
		(color, imgPath) = img_by_type_dict[mode]
		whisperButton.SetUpVisual(imgPath)
		whisperButton.SetOverVisual(imgPath)
		whisperButton.SetDownVisual(imgPath)
		if color:
			whisperButton.SetToolTipTextWithColor(name, color)
		else:
			whisperButton.SetToolTipText(name)

		self.__AddNewWhisper(name, None, mode)

		whisperButton.ToolTipText.SetHorizontalAlignCenter()
		whisperButton.SetEvent(ui.__mem_func__(self.ShowWhisperDialog), whisperButton)
		whisperButton.Show()
		whisperButton.name = name

		self.whisperButtonList.insert(0, whisperButton)
		self.__ArrangeWhisperButton()

		return whisperButton

	def __DestroyWhisperButton(self, button):
		button.SetEvent(0)
		self.whisperButtonList.remove(button)
		self.__ArrangeWhisperButton()

	def HideAllWhisperButton(self):
		for btn in self.whisperButtonList:
			btn.Hide()

	def ShowAllWhisperButton(self):
		for btn in self.whisperButtonList:
			btn.Show()

	def __CheckGameMaster(self, name):
		if not self.listGMName.has_key(name):
			return
		whisper = self.__GetWhisper(name)
		if whisper:
			whisper["dialog"].SetGameMasterLook()

	def RegisterGameMasterName(self, name):
		if self.listGMName.has_key(name):
			return

		print "RegisterGameMasterName", name
		self.listGMName[name] = "GM"

	def IsGameMasterName(self, name):
		if self.listGMName.has_key(name):
			return True
		else:
			return False

	#####################################################################################

	#####################################################################################
	### Guild Building ###

	def BUILD_OpenWindow(self):
		self.wndGuildBuilding = uiGuild.BuildGuildBuildingWindow()
		self.wndGuildBuilding.Open()
		self.wndGuildBuilding.wnds = self.__HideWindows()
		self.wndGuildBuilding.SetCloseEvent(ui.__mem_func__(self.BUILD_CloseWindow))

	def BUILD_CloseWindow(self):
		self.__ShowWindows(self.wndGuildBuilding.wnds)
		self.wndGuildBuilding = None

	def BUILD_OnUpdate(self):
		if not self.wndGuildBuilding:
			return

		if self.wndGuildBuilding.IsPositioningMode():
			import background
			x, y, z = background.GetPickingPoint()
			self.wndGuildBuilding.SetBuildingPosition(x, y, z)

	def BUILD_OnMouseLeftButtonDown(self):
		if not self.wndGuildBuilding:
			return

		# GUILD_BUILDING
		if self.wndGuildBuilding.IsPositioningMode():
			self.wndGuildBuilding.SettleCurrentPosition()
			return True
		elif self.wndGuildBuilding.IsPreviewMode():
			pass
		else:
			return True
		# END_OF_GUILD_BUILDING
		return False

	def BUILD_OnMouseLeftButtonUp(self):
		if not self.wndGuildBuilding:
			return

		if not self.wndGuildBuilding.IsPreviewMode():
			return True

		return False

	def BULID_EnterGuildArea(self, areaID):
		# GUILD_BUILDING
		mainCharacterName = player.GetMainCharacterName()
		masterName = guild.GetGuildMasterName()

		if mainCharacterName != masterName:
			return

		if areaID != player.GetGuildID():
			return
		# END_OF_GUILD_BUILDING

		self.wndGameButton.ShowBuildButton()

	def BULID_ExitGuildArea(self, areaID):
		self.wndGameButton.HideBuildButton()

	#####################################################################################

	def IsEditLineFocus(self):
		if self.ChatWindow.chatLine.IsFocus():
			return 1

		if self.ChatWindow.chatToLine.IsFocus():
			return 1

		return 0

	def EmptyFunction(self):
		pass

	## GAME MASTER CAPTCHA ##
	def GetGameMasterCaptchaDialog(self, pid):
		if self.gameMaster_CaptchaDialogs.has_key(pid):
			return self.gameMaster_CaptchaDialogs[pid]
		return None

	def CreateGameMasterCaptchaDialog(self, pid):
		if self.gameMaster_CaptchaDialogs.has_key(pid):
			oldDlg = self.gameMaster_CaptchaDialogs[pid]
			if oldDlg:
				oldDlg.Destroy()
				oldDlg.Hide()

		dlg = uiCaptcha.GameMasterCaptchaDialog()
		self.gameMaster_CaptchaDialogs[pid] = dlg
		return dlg

