# Nocny przegląd osiemnastu zgłoszeń (13 września 2026)

Lista osiemnastu rzeczy oddana do zrobienia w nocy. Ten plik mówi, co zostało
**zrobione i zweryfikowane**, co zostało **sprawdzone i okazało się już
działające**, a co **nie jest zrobione** — z dokładnymi punktami zaczepienia,
żeby następne podejście nie zaczynało od szukania.

Zasada, której się trzymałem: nie wysyłać na żywy serwer zmiany, której nie da
się skompilować i sprawdzić tej samej nocy. CLAUDE.md ma osobny akapit o tym,
ile razy nieprzetestowana zmiana silnika położyła aktualizację wszystkim.

## Kopia zapasowa

`Downloads\Metin2 Singleplayer\kopia-zapasowa-2.0.37-20260913-2235.zip` —
788 MB, Klient + Serwer, zrobiona przed pierwszą zmianą.

---

## Druga partia nocy — z dowodami na żywo

Po podniesieniu rat (exp 100×, drop i yang 30×) boty zaczęły rosnąć i część
rzeczy dało się wreszcie **zobaczyć w działaniu**, a nie tylko skompilować.

| Rzecz | Dowód z żywego serwera |
|---|---|
| Blokada expa dropperów | `exp locked for a dropper pid=1539 ZwojPowrotu level=33 personality=10` i to samo dla pid 1513 — obaj dropperzy medali, obaj zatrzymani **dokładnie na swoim progu 33** |
| PvP między botami | 4 wyzwania i 4 zgody; pierwsza para: `NoSiemaNie` wyzwał `xLowieRybkix` o 21:23:13, zgoda o 21:23:17 |
| Marmury polimorfii | pierwsza linia `polymorphed for a boss` w historii tego świata |
| Przenosiny map | `game1 MAP_ALLOW` zawiera teraz 66, 67 i 68; `game2` już ich nie ma |

Czego nadal **nie** widziałem: łowienia (wędkarz to losowanie 8% po pid, a
najwyższy bot ma dopiero 35 poziom), gildii (próg 40) i ruchu na nowe mapy
(progi 57 i 62).

### Co doszło w drugiej partii

- **PvP (zadanie 2)** — wyzwanie gracza dociera do bota tą samą drogą co
  zaproszenie do drużyny: hak w `pvp.cpp` zapisuje wyzwanie, tick bota zgadza
  się po trzech sekundach. Boty wyzywają też siebie nawzajem — rzadko (sześć na
  tysiąc, raz na minutę), tylko blisko, w podobnym poziomie i przy pełnym życiu.
  W pojedynku **nie piją potek**; silnikowego `IsFighting` nie dało się użyć, bo
  na jednym silniku siedzi pod `ENABLE_NEWSTUFF`, a na drugim nie istnieje, więc
  bot pamięta pojedynek sam.
- **Las, Czerwony Las, Wieża Demonów (zadania 3 i 12)** — przeniesione z `game2`
  na `game1`, z hubami wyliczonymi z regenów, przybyciami, trasowaniem po
  poziomie i granicami w panelu. Wieża **nie** dostaje łamania metinów, zgodnie
  z Twoją prośbą.
- **Etap 50 biologa (zadanie 7)** — odblokowany samą przenosiną mapy 66; panel
  liczy znów dziewięć etapów.
- **Koń militarny (zadanie 12)** — medale zatrzymują się na dwudziestce, a
  dwudziesty pierwszy poziom pochodzi z próby w Wieży Demonów: pięćdziesiąt
  zabić na 1001–1004, bez limitu czasowego, dokładnie jak próba na pustyni.
- **Eliksiry Słońca (zadanie 15)** — 39037–39039 dopisane do eliksirów
  doświadczenia; wcześniej bot traktował je jak zwykły łup.

## Zrobione

### 18. Łowienie od 30 poziomu (było od 50)

Trzy miejsca, bo dwa muszą się zgadzać z trzecim:

- silnik: `char.cpp` w `CHARACTER::fishing()` odmawiał poniżej 50 — zmienione na
  30 przez `playerbotify.py` (`apply_fishing_min_level`) i w zestage'owanym
  pliku, który realnie jedzie w paczce;
- AI: `playerbot_activities.h` i `playerbot_travel.h` pytały o 50 wprost —
  teraz obie pytają o `PLAYERBOT_FISHING_MIN_LEVEL`, żeby bot poniżej progu nie
  szedł nad wodę, która i tak by go odprawiła.

Reszta bramek łowienia na tej linii zostaje bez zmian: mapa 1/21/41,
przepustka (27620, kupowana przez `EnsurePlayerBotFishingPass`) i przynęta.

### 13. Dropperzy blokują sobie doświadczenie

Silnik ma to od zawsze: `PointChange` przy `POINT_EXP` robi
`if (FindAffect(AFFECT_EXP_BLOCK)) return;`. Nie trzeba było łatać silnika —
bot sam nakłada sobie ten efekt, gdy dojdzie do poziomu swojego łowiska:

| osobowość | poziom | dlaczego tam |
|---|---|---|
| dropper medali | 33 | medal to grupa „kill", a `aiPercentByDeltaLev` zbija ją do 1 przy piętnastu poziomach nad potworem; małpy w łatwym lochu mają 22–30 |
| dropper M2 | 36 | żołnierze drugiej wioski to 18–36 |
| dropper M3 | 30 | mapa gildii to 8–24, a farma broni na 30 chodzi do czterdziestki |
| dropper metinów | 40 | dorzucanie księgi z metina umiera piętnaście poziomów nad kamieniem |

Na r40250 `AFFECT_EXP_BLOCK` **nie istnieje** (grep po całym drzewie) — kod jest
tam zabezpieczony `#if defined(PLAYERBOT_ENGINE_MT2009)` i nic nie robi.

### 9. Marmury polimorfii przeciw bossom

Wcześniej żaden bot nigdy nie użył marmuru — były wyłącznie towarem na stragan.
Teraz bot zakłada marmur, gdy: bije bossa (`GetMobRank() >= MOB_RANK_BOSS`),
boss ma jeszcze ≥90% życia, bot nie jest już przemieniony i nie siedzi na koniu
(silnik odmawia w siodle). Marmury to 70104–70107 i 71093 — tylko te wchodzą w
gałąź, która daje bonus do obrażeń.

„Na marmurach nie używa się skilli" **wymusza silnik** (`char_skill.cpp` odmawia
każdej umiejętności pod polimorfią) — nic nie trzeba było dodawać.

### 1. Boty przyjmują zaproszenie do drużyny (częściowo)

Przyczyna, dla której to nigdy nie działało: `CHARACTER::PartyInvite` kończy się
wysłaniem `HEADER_GC_PARTY_INVITE` na deskryptor zapraszanego. Bot ma deskryptor,
ale nie ma za nim klienta — pakiet szedł w próżnię, nikt nie klikał „Akceptuj" i
po dziesięciu sekundach zaproszenie wygasało. **Zapraszanie bota nie robiło
dosłownie nic i nie zostawiało śladu w żadnym logu.**

Rozwiązanie w trzech częściach:

- `playerbot_party_policy.h` — nowy, inline, bez zależności od silnika (ten sam
  kształt co `playerbot_offline_policy.h`), dziennik zaproszeń;
- hak w `char.cpp::PartyInvite` (przez `playerbotify.py`): gdy zapraszany jest
  botem, zaproszenie ląduje w dzienniku zamiast w pakiecie;
- `AcceptPlayerBotPartyInvite` w ticku bota — woła `leader->PartyInviteAccept(bot)`,
  czyli dokładnie to, co zrobiłby klient. Uruchamiane **co tick**, bo zaproszenie
  żyje tylko dziesięć sekund, a pas drużyny chodzi co dziesięć sekund do trzech
  minut.

Do tego dwie rzeczy, bez których to by nie miało sensu:

- drużyna prowadzona przez **gracza** jest wyjęta ze wszystkich reguł rotacji
  botów (kohorta, wygasanie po 5–15 minutach, promień maruderów) — inaczej bot
  dołączyłby i wyszedł w ciągu minuty;
- `ManagePlayerBotFollowHumanLeader` — bot biegnie za graczem, gdy oddali się
  ponad 1500 jednostek, z koniem, i zabiera tick, żeby pas wędrowania nie wysłał
  go na własne łowisko.

Warunki dołączenia zostają **silnikowe** i o nich trzeba wiedzieć: to samo
królestwo (`PERR_DIFFEMPIRE`), różnica do 30 poziomów, wolne miejsce w drużynie
ośmioosobowej. Bot nigdy nie odmawia sam z siebie.

**Czego tu nie ma:** zapraszania kolejnych botów przez bota-lidera i „znajomych"
z punktu 1. Gracz zaprasza każdego bota osobno.

### 14. Przycisk języka w launcherze

Przełącznik PL/EN **już był** (`Switch-LauncherLanguage`, zapamiętywany w
konfiguracji). Problem był w etykiecie: napis „JEZYK: POLSKI" nie mówi
anglojęzycznemu, w co kliknąć. Teraz przycisk zawsze pokazuje oba języki:
„JEZYK / LANGUAGE: POLSKI" i „LANGUAGE / JEZYK: ENGLISH".

Kompletności tłumaczenia panelu klasycznego **nie przejrzałem** — 309 wywołań
`t()` przy ok. 530 napisach pisanych wprost po polsku, więc angielska wersja
nadal jest dziurawa (to ta sama sprawa, co notatka „Half a translation reads
worse than either language" w CLAUDE.md).

---

## Sprawdzone — działa, nic nie trzeba było zmieniać

### 17. Ulepszanie u kowala zużywa ulepszacze

Zarzut ze zgłoszenia („boty ulepszają na +9 bez ulepszaczy") **jest nieprawdziwy**.

Boty wołają `ch->DoRefine(item, false)` — drugi argument to `bMoneyOnly`. W
`char_item.cpp` (linia ~900) jest `if (!(bMoneyOnly))`, a w środku najpierw
sprawdzenie `CountSpecifyItem(prt->materials[i].vnum) < ...count` z odmową
„Not the right material for an upgrade.", a potem
`RemoveSpecifyItem(...)`. Czyli materiały są wymagane i kasowane przez sam
silnik, tą samą ścieżką co u gracza, z tą samą szansą z `refine_proto`.

Bez materiałów idą tylko **zwoje** (`DoRefineWithScroll`) — u gracza dokładnie
tak samo. Na żywym świecie: 8997 linii ulepszania, z czego 3202 bez zwoju i 79
ze zwojem.

### 16. Szaman w drużynie buffuje i leczy

`playerbot_combat.h` ma pas, który dla szamana w drużynie chodzi po członkach
na mapie (`ForEachOnMapMember`, zasięg 2000) i:

- nakłada buff, gdy `member->FindAffect(buffVnum) == NULL` — czyli faktycznie
  czeka, aż danemu członkowi zejdzie, i wtedy odnawia;
- leczy (vnum 109) członka, który spadł do ≤60% życia — to pokrywa przypadek
  „boss złapał aggro na kolegę z PT".

Jedno zastrzeżenie: pas nakłada **jeden** czar na przebieg (`m_bApplied`), więc
obłożenie pełnej drużyny zajmuje kilka przebiegów.

### 4. Osobowości działają

Przydzielane raz, stabilnie po pid (`GetPlayerBotStablePersonality`, hash z pid),
dwanaście rodzajów. Każda ma skutek: albo własną bramkę (dropperzy — `travel.h`,
kupiec — stragan, ostrożny zbieracz — szansa na podniesienie łupu), albo przez
ambicję (`GetPlayerBotStableAmbition` mapuje każdą osobowość na ambicję, a
ambicja steruje plannerem).

### 10. Gildie: zero gildii to nie błąd

W bazie jest 0 gildii i 0 członków, ale próg założenia to **poziom 40**
(`PLAYERBOT_GUILD_MIN_LEVEL`) plus 200 000 opłaty, a najwyższy bot w tym świecie
ma **31 poziom**. Mechanizm jest na miejscu (`FoundPlayerBotGuild`, zapraszanie
przez `RequestAddMember`, jeden na dwunastu jako założyciel) — po prostu nikt
jeszcze nie dorósł.

### 15. Boosty: dłonie i eliksiry są objęte

`IsPlayerBotBoosterItem` łapie po typie, nie po vnumie: `USE_ABILITY_UP` albo
`USE_AFFECT` z `value0 == 510`. Dłoń Krytyka (39024/71044/72026) i Dłoń Przebicia
(39025/71045/72025) to dokładnie `USE_AFFECT` z value0 510 — **są objęte**, także
kopie z ItemShopu.

Eliksiry Księżyca (39040–39042) są na liście eliksirów doświadczenia; **Eliksiry
Słońca (39037–39039) nie są** — to jedyna luka, jaką znalazłem w tym punkcie.

### 6. Lurowanie istnieje i jest wpięte

`playerbot_lure.h` to pełna maszyna stanów (PLAN → APPROACH → TAG → CONFIRM →
RETURN → HANDOFF → RECOVER), wołana z ticku przez `HandlePlayerBotLureCourse`
(`playerbot_manager.cpp:2247`). Wymaga łucznika w drużynie, lidera na tej samej
mapie i liczy, ile potworów **naprawdę** goni bota. Nie jest to więc rzecz do
zbudowania, tylko do dostrojenia — i tego dostrojenia nie robiłem.

---

## Nie zrobione — z punktami zaczepienia

### 2. PvP

Wszystko rozpoznane, nic nie napisane. Wyzwanie to komenda `do_pvp`
(`cmd_general.cpp:753`) wołająca `CPVPManager::instance().Insert(ch, ofiara)`.
`Insert` działa na zgodę obustronną: pierwsze wywołanie tworzy `CPVP` i wysyła
„challenged you to a battle", drugie (z drugiej strony) woła `Agree` i walka
rusza. Czyli akceptacja bota = `Insert(bot, wyzywający)` po trzech sekundach —
tą samą drogą co przy party: hak w `pvp.cpp`, dziennik, odpowiedź w ticku.
Uwaga: `pvp.cpp` **nie jest** w `server-update-files.mt2009.txt` — trzeba dodać.

Zakaz potek w pojedynku silnik już ma: `g_NoPotionsOnPVP` +
`IsLimitedPotionOnPVP`/`IsAllowedPotionOnPVP` (`char_item.cpp`).

### 3. Las i Czerwony Las

Mapy **istnieją**: 67 `metin2_map_trent`, 68 `metin2_map_trent02`. Zmierzone z
plików serwera (pewniejsze niż wiki):

| mapa | potwory | poziomy | punkty odrodzenia |
|---|---|---|---|
| Las (67) | 2301–2305 + boss 2381 (Naara, 79) | 65–71 | 912 |
| Czerwony Las (68) | 2311–2315 + boss 2382 (Dae-Ho, 86) | 74–82 | 1456 |

Obie hostuje dziś `game2`, a boty żyją na `game1` — więc trzeba je przenieść w
`m2-render-config`, plus lista dozwolonych map w `apply.sh`, whitelist nawigacji,
tabela hubów w `playerbot_wandering.h`, wiersze w tabelach frontieru i nazwy/
granice/kafelki w obu panelach. **Uwaga na kolejność:** przy botach do 31 poziomu
te mapy i tak stałyby puste; to zmiana dla świata z wysokimi botami.

### 12. Wieża Demonów i koń militarny

Wieża Demonów to mapa **66** (`metin2_map_deviltower1`), też na `game2`: 1001–1004
(57–60), 1031/1032 (67/69), jeden metin 8015. Koń dziś kończy się na 11
(`PLAYERBOT_BATTLE_HORSE_LEVEL`), docelowo 20 (medale) i 21 (misja w Wieży).

Przeniesienie mapy 66 na `game1` **odblokowałoby przy okazji etap 50 biologa**
(punkt 7) — to jedna zmiana załatwiająca dwie rzeczy.

### 7. Biolog po Zębie Orka

Etap 40 (Księga Klątw) wszedł w 2.0.37 i działa. Etap 50 (Pamiątka Po Demonie)
jest w tabeli, ale celowo pomijany, bo jego potwory stoją wyłącznie w Wieży
Demonów — patrz punkt 12. Dalszych etapów (Matowe Lody itd.) nie sprawdzałem.

### 5. Kilof, kopanie rud, przetapianie — ZROBIONE (trzecia partia nocy)

Pierwsza diagnoza brzmiała „najdroższa pozycja z listy" i była trafna, ale z
innego powodu, niż myślałem. Cała mechanika jest w silniku od r40250:
`mining.cpp` ma tabelę osiemnastu rud, `CHARACTER::mining(żyła)` jest punktem
wejścia, kilof musi siedzieć w slocie broni, a uderzenie to event na 2×(5..15)
sekund z szansą 20% plus bonus za klasę kilofa.

Czego nie ma: **tego świata**. Przeskanowałem wszystkie 109 map — zero spawnów
żył (20047–20059, 30301–30305) i zero alchemików w `regen.txt`, `npc.txt` i
`boss.txt`. AI prowadzące boty do rudy prowadziłoby je donikąd.

Dlatego żyły stawia i utrzymuje sam rdzeń playerbotów (`playerbot_mining.h`), a
nie pliki świata: `src/serverfiles/` jest w `.gitignore`, więc zmiany map i tak
nie pojechałyby do nikogo, a `playerbot_*` jedzie w każdej paczce. Dwadzieścia
żył na mapach 61/63/64, każda na współrzędnej istniejącego huba łowieckiego —
bo `SpawnMob` odmawia postawienia żyły na `ATTR_BLOCK`, a hub to punkt, na
którym świat sam odradza potwory.

Dowód na żywo, pierwsze minuty po wdrożeniu:
`veins standing=0 spawned=20 refused=0 sites=20` — **wszystkie dwadzieścia
stanęły, żadna nie odrzucona** — a bot `ToyotaSupra` (32 lvl, Dolina) kupił
kilof za 80 000, zaczął sesję i go założył.

Pierwszy przebieg ujawnił też błąd, którego nie dało się przewidzieć z kodu:
`pickaxe equipped` wracało dla tego samego bota **co równo 32 sekundy**, czyli
co moje uderzenie. Przebieg ekwipunku biegnie w grupie upkeep *nad* górnictwem i
zdejmował kilof jako „nie-broń", a `mining_event` sprawdza slot broni w chwili,
gdy odpala — więc każde uderzenie było odrzucane i ruda nie padała ani razu.
Wędka ma w tym miejscu wyjątek `!state.bFishingSession`; kilof dostał swój.

Po poprawce, na żywo: **15 sesji kopania, zero odmów, 39 sztuk rudy w torbach i
pierwsze wytopienie** (Ruda Miedzi → Miedź, opłata 5000) — przed poprawką ruda
stała na zerze przy dwóch pomiarach. Tick przy 350 botach: 1,8–2,2 s z 60,
`watchdog=0`, czyli oba nowe podsystemy nie kosztowały nic mierzalnego.

### 18. Łowienie od 30 — i błąd, który odsłoniło

Obniżenie limitu wędki obudziło drugą rzecz, śpiącą od zawsze: `log.fish_log`
miała osiem kolumn z r40250, a mt2009 wpisuje sześć
(`FishLog(playerId, itemVnum, count, rodLevel, baitVnum)`). Każdy połów kończył
się `errno 1136` — 364 linie w dziesięć minut — i nie zapisywał się nigdzie.
Winny był `logschemify.py`, który trzymał `fish_log` na liście „bierz z
r40250". Tabela ma teraz własną definicję, a `apply.sh` przebudowuje starą
tylko wtedy, gdy rozpozna w niej kolumnę, której ten silnik nigdy nie pisze.

### 8. Bossowie na wszystkich mapach

Dziś boty znają bossów tylko przez `wBossRace` w tabelach hubów (Dolina, Hwang,
kilka innych) plus `IsPlayerBotBossAlive` i wołanie gildii. Plik `boss.txt` ma
**20 map** — w tym Las i Czerwony Las. Rozszerzenie to tabela bossów per mapa
plus reguła „idziemy gildią".

### 11. Wrogość między królestwami — ZROBIONE, za przełącznikiem

W `playerbot_targeting.h` i w polityce wartości walki nie było **ani jednego**
odwołania do królestwa. Rozważałem rozluźnienie filtra kolektora celów, żeby
wpuszczał postacie z innych królestw — i odrzuciłem to świadomie. Ten filtr
występuje w trzech miejscach, a za nim stoją polityka wartości walki, reguła
trzymanego celu, multi-pull i wspólny cel drużyny: wszystkie napisane o
potworach. Przepuszczanie przez nie postaci graczy to duża zmiana w najgorętszej
ścieżce, dla funkcji, która i tak ma być domyślnie wyłączona.

Zamiast tego oparłem rzecz na pojedynku, który boty już umieją toczyć (z tej
samej nocy): agresywny bot wyzywa napotkanego bota innego królestwa, a dalej
robotę robi silnik. Pojedynek kończy się sam, gdy ktoś padnie — to „bez pętli";
przegrany schodzi poniżej progu życia i przestaje być celem, dopóki nie odpocznie
— to „ginący odpuszcza i bierze inny spot".

Ograniczenia: tylko mapy frontieru, nigdy w wiosce, nigdy na graczu, różnica
poziomów do ośmiu, jedno losowanie na dwie minuty. Udział agresywnych jest
przypisany po pid, nie losowany — żeby królestwo miało charakter, a nie humor.

Suwak `KINGDOMPVP` w panelu, **0% domyślnie**, czyli świat bez zmian dopóki
Tieru sam go nie podniesie. Musiał trafić też do zapisu pliku wag w panelu: ten
przepisuje plik w całości, więc klucz, którego panel nie zna, zostałby skasowany
przy pierwszym zapisie strony.

---

## Czego nie dotknąłem w ogóle

Punkt 1 w części „bot-lider zaprasza kolejne boty", punkt 8 (bossowie na
wszystkich mapach — zmierzony, nienapisany) oraz kompletność angielskiego w
panelu klasycznym (punkt 14, druga połowa).

Punkty 5 i 11 zostały domknięte w trzeciej partii nocy — opisy wyżej. Punkt 18
(łowienie od 30) miał trzecią bramkę, której nie widziałem za pierwszym razem:
limit poziomu na samej wędce w `world.item_proto`. Zdjęty dla wszystkich
dwudziestu wędek, w bazie na żywo i w bootstrapie dla świeżych instalacji.

### 6. Wabienie w drużynie — znaleziona przyczyna, poprawka wąska

Wabienie **działa** — w logu jest pełny przebieg: `planned ... party=6 ready=5`,
czyli łucznik znalazł drużynę i pięciu odbiorców. Zaraz potem: `no pack ...
map=1 seen=0` i koniec kursu po 957 ms.

Przyczyna: `HandlePlayerBotLureCourse` nie miał **żadnej bramki mapy**. Wystarczy
drużyna, łucznik i wyjście ze strefy bezpiecznej — więc kurs planował się w
Yongan, w pierwszej wiosce, gdzie nie ma watahy do przyciągnięcia. Dołożyłem
warunek „tylko mapy frontieru", bo o „duży spot" prosiłeś, i zszedłem z
`PLAYERBOT_LURE_MAX_GROUPS` z 4 na 3, żeby trzymać się „aggro 1–3".

**Czego to nie naprawi i trzeba o tym wiedzieć rano:** na mapach frontieru stoi
teraz 33 boty i **zero drużyn** — wszystkie 24 drużyny świata siedzą w wioskach
i na M2, bo populacja jest za słaba na frontier (najwyższy poziom to 36). Po tej
zmianie wabienie będzie więc milczeć, dopóki boty nie dorosną. To nie jest
regres: przedtem odpalało tam, gdzie i tak nie miało co złapać.

### 14 (druga połowa). Angielski w panelu klasycznym — zmierzone

Liczba „530 polskich napisów na sztywno" z pierwszej partii nocy jest myląca i
prostuję ją: z 531 literałów z ogonkami zdecydowana większość to **dane gry**, a
nie interfejs — „Korzeń Gango", „Ząb Orka", „Pamiątka Po Demonie" — i tych się
nie tłumaczy.

Realna dziura jest mała i policzalna: **20 napisów w szablonach HTML** (m.in.
„Mapa na żywo", „Broń 30 Lv", „Pęknięty Kamień", etykiety w JS podpowiedzi) oraz
**15 komunikatów `flash()`** wpisanych po polsku zamiast przez `t()`. Do tego
`🗺️ Map {{p.map_index}}` po angielsku w polskim widoku. To robota na jedno
posiedzenie z listą w ręku, nie przepisywanie panelu.
