# Metin2 Singleplayer — audyt zgłoszeń Discorda dla Claude’a

**10 września 2026; migawka wiadomości do 21:35:53 czasu polskiego.** Zadanie: sprawdzić najnowsze błędy, powiązać je z kodem i przygotować naprawy do wdrożenia przez Claude’a. Ten dokument jest przekazaniem pracy, nie potwierdzeniem wdrożenia.

Najpilniejsze obszary to poprawne wydawanie przedmiotów, rozbieżność planowania i wykonywania ulepszeń oraz instalacja na Linuksie. Potwierdzone są też zamienione osobowości i bonusy w panelach. Część starszych problemów ma już poprawki w źródłach 1.32.5; nie należy ponownie implementować ich na podstawie samego starego zgłoszenia.

## 1. Stan projektu i wiarygodność ustaleń

**Audytowana kopia robocza:**

`C:\Users\dawio\Documents\Codex\2026-08-14\https-github-com-azzlacksyndicate-metin2-singleplayer\work\metin2-suite`

`VERSION = 1.32.5`, odczytany HEAD: `20b3d19f46b970e5fb82aa516d039bc85cf444df`. Wszystkie ścieżki kodu niżej są względem tej kopii. HEAD nie opisuje samodzielnie plików zmienianych równolegle przez Claude’a; ich SHA-256 i czasy modyfikacji są w `DISCORD_DOWODY_I_WERSJA.json`. Przed edycją odszukać symbole i porównać aktualne pliki.

Instalacja w `Downloads\Metin2_Singleplayer_Server_r40250_FIXED\Metin2_Singleplayer_Server_r40250_FIXED` nadal deklarowała 1.31.3. Nie utożsamiać jej pliku VERSION, bieżącego checkoutu, źródeł stagingu i faktycznie uruchomionego obrazu/binarium. To cztery osobne informacje. Nie ustalono tutaj hasha działającego na serwerze zgłaszającego binarium.

Odczytano **37 aktywnych wątków forum, 134 wiadomości**, a także po 100 ostatnich wiadomości devlogu i kanału ogólnego. Publiczne archiwum forum zwróciło pustą listę. Szczegółowy indeks wszystkich wątków i dat: `DISCORD_INDEKS_ZGLOSZEN.md`. Nie jest to kompletny eksport wszystkich kanałów ani prywatnych archiwów. Odczyt przez API bota; bez wysyłania wiadomości. Nie analizowano wizualnie filmów i wszystkich załączonych screenshotów.

Stosowane statusy:

- **Potwierdzone w kodzie:** odczyt implementacji pokazuje konkretną niezgodność. Nie oznacza to odtworzenia objawu w działającej grze.
- **Częściowa poprawka:** nowa implementacja istnieje, ale nie zamyka wskazanego przypadku.
- **Do reprodukcji:** objaw zgłoszony przez gracza, bez wystarczających danych do przypisania jednej przyczyny.
- **Poprawka obecna / zamknięte:** zachować i sprawdzić regresję; nie przedstawiać jako nowej niezałatwionej usterki.

Nie zmieniano kodu, bazy ani stanu gry. Nie wykonywano restartów, rebuildów, migracji ani testów na świecie użytkownika. Wykonano analizę źródeł, ekstrakcję stałych paneli przez AST i kontrolę kontraktów między Pythonem, questem i C++. Planowane testy poniżej pozostają do wykonania przez Claude’a na izolowanym środowisku.

Skróty ścieżek: **AI** = `linux-port/overlays/playerbot/src/game/src/`; **CORE** = `linux-port/docker/game/src/server/`; **SEBAN** = `linux-port/docker/seban-panel/`; **DB** = `linux-port/docker/mariadb/`.

## 2. Kolejność realizacji

| ID | Priorytet | Problem | Status i zakres naprawy |
|---|---|---|---|
| D01 | P1 | Nagrody ze skrzyń wypadają na ziemię | Częściowa poprawka; rezerwacja miejsca na całą paczkę |
| D02 | P1 | Masowe przydzielanie zatrzymuje się po 333 | Objaw do reprodukcji; dodatkowo potwierdzony błędny zakres ilości |
| D03 | P1 | Pełne +9, a bot wciąż planuje kowala | Potwierdzona różnica predykatów planera i wykonawcy; związek z konkretnym botem do sprawdzenia |
| D04 | P1 | Świeża instalacja MariaDB pomija import | Zgłoszone prawa plików + brak trwałej gwarancji czytelności w ścieżce importu |
| D05 | P1 | ItemShop 403 / unhealthy | Zgłoszone prawa katalogów + brak normalizacji w Dockerfile |
| D06 | P2 | Migrator czeka mimo Access denied | Potwierdzone; przerwać szybko trwały błąd uwierzytelnienia |
| D07 | P2 | `log.log.hint`: big5 kontra latin1 | Zgłoszenie z obejściem; potrzebna kontrolowana migracja po sprawdzeniu danych |
| D08 | P2 | Zamienione ŚR / UM | Potwierdzone w słowniku i SQL rankingu, także w 1.32.5 |
| D09 | P2 | Zamienione Handlarz / Wędrowiec | Potwierdzone w etykietach PL i EN |
| D10 | P2 | Niepełne rankingi +9 i skilli | Potwierdzone ograniczenia zapytań i agregacji |
| D11 | P2 | Minimalny suwak, a większość botów handluje | Częściowa poprawka; sprawdzić wyjątki oraz istniejące sklepy |
| D12 | P2 | Bot nie opuszcza M1 mimo celu Pustynia | Do reprodukcji; zarejestrować przejmowanie celu i postęp podróży |
| D13 | P2 | Zastygające / znikające boty | Do reprodukcji; oddzielić AI od widoczności klienta |
| D14 | P2 | Plecak pełen wartościowych rzeczy innych klas | Nie usuwać ochrony kosztowności; sprawdzić przepływ do handlu |

P1 oznacza ryzyko utraty/nieprawidłowego wydania przedmiotów albo blokadę ważnej funkcji. P2 oznacza błąd zachowania, prezentacji lub diagnostyki. Zrealizować niezależne drobne poprawki D08–D09 przy okazji pierwszego pakietu; nie wymagają przebudowy AI.

## D01. Skrzynie ucznia: dwie próby znalezienia miejsca nie rezerwują paczki

[Zgłoszenie z 10.09, 17:38](https://discord.com/channels/1543720413035499650/1547632250944757911/1547632250944757911). O 20:28 pojawia się odpowiedź, że problem podobno naprawiono. Jest to informacja o deklarowanej poprawce, nie dowód pokrycia wszystkich układów plecaka.

**Kod:** `AI/playerbot_consumables.h:75` i `:106` sprawdzają `GetEmptyInventory(1)` oraz `GetEmptyInventory(3)` przed otwieraniem kluczem/skrzyni prezentowej. Te wywołania mogą wskazać to samo miejsce. Nie rezerwują osobnych pól dla kilku nagród i nie uwzględniają całej zawartości paczki.

`CORE/game/src/char_item.cpp:7018`, `GiveItemFromSpecialItemGroup`, losuje zestaw przez `GetMultiIndex` i wydaje nagrody kolejno. Wywołanie `AutoGiveItem` w linii 7108 traktuje niezerowy wskaźnik jako sukces. `AutoGiveItem` w linii 6669 umieszcza przedmiot na ziemi, gdy brakuje miejsca; to nadal utworzony przedmiot, więc wskaźnik nie jest dowodem dostarczenia do plecaka. Obecne zabezpieczenie ogranicza objaw, ale go nie eliminuje.

**Naprawa docelowa:** przygotować plan nagród, sprawdzić i zarezerwować ich rozmieszczenie, dopiero potem zużyć skrzynię/klucz i zatwierdzić wydanie. Losować raz na próbę i stosować dokładnie ten sam zestaw w walidacji i wykonaniu. Uwzględnić wysokość przedmiotów, granice stron, dopełnianie zgodnych stosów, limit stosu, specjalne okna ekwipunku oraz ewentualne miejsce zwalniane przez zużywaną skrzynię. Nie zmieniać globalnie zachowania wszystkich nagród graczy tylko po to, by zabezpieczyć boty: dodać jawny tryb wydania do ekwipunku bez zrzucania na ziemię, wykorzystany w tej ścieżce.

W przypadku braku miejsca odroczyć otwarcie do zwolnienia plecaka i pokazać konkretny powód w statusie. Jeśli pełna przebudowa wydawania paczek wymaga większego wydania, doraźna blokada otwierania musi być zachowawcza i oparta na maksymalnej rzeczywistej paczce; nie kolejny arbitralny próg liczby wolnych pól. Najpierw potwierdzić VNUM skrzyni ucznia i jej ścieżkę: special item group czy quest. Poprawka wyłącznie `ITEM_GIFTBOX` nie zabezpiecza automatycznie `pc.give_item2` w questach.

**Testy odbioru:** brak miejsca; jeden wspólny otwór mieszczący 3 pola i paczka dwóch przedmiotów po 3; rozdrobnione wolne pola; stos prawie pełny; nagroda niestackowalna; zwolnienie miejsca po zużyciu ostatniej skrzyni; błąd tworzenia kolejnej nagrody. Przy odroczeniu brak ubytku skrzyni/klucza i częściowej nagrody. Przy sukcesie cała paczka w poprawnym oknie, zero `SYSTEM_DROP`. Nie uznawać samego `UseItem()==true` za zaliczenie.

## D02. Masowe wydawanie: zatrzymanie po 333 oraz osobny błąd ilości

[Najnowsze zgłoszenie, 10.09, 21:35:53](https://discord.com/channels/1543720413035499650/1543721657821302899/1547692110046822551). Wcześniejszy devlog 1.32.2 opisywał poprawkę związanej z tym funkcji. Nie zamyka to nowszego zgłoszenia.

**Co jest pewne:** `SEBAN/item_grants.py:8` dopuszcza 65 535 sztuk; `files/web_admin.quest:252` przyjmuje ten sam zakres i w linii 261 przekazuje go do `pc.give_item2`. `CORE/game/src/questlua_pc.cpp:508` odczytuje `int`, a w linii 526 przekazuje go do `AutoGiveItem(DWORD, BYTE, ...)` z `CORE/game/src/char_item.cpp:6560`. Dochodzi do zawężenia licznika. Dla 256 argument BYTE wynosi 0, dla 300 wynosi 44, dla 65 535 wynosi 255. To wartości argumentu funkcji, **nie zmierzona liczba końcowo nadanych sztuk**; dalszy kod może jeszcze ograniczać stos. Dodatkowo quest sprawdza miejsce tylko przez `pc.enough_inventory(vnum)`, bez ilości. `item_id != 0` nie dowodzi zgodnej ilości ani dostarczenia wszystkiego do plecaka.

**Czego nie potwierdzono:** liczba 333 to zgłoszony licznik rozdanych przedmiotów/odbiorców. Nie wynika z powyższego zawężenia. Nie ma danych z konkretnej paczki, jej statusów, logu workera ani hashy questów na wszystkich core’ach. Nie przedstawiać tego jako udowodnionej przyczyny zatrzymania.

**Ścieżka do prześledzenia:**

- `SEBAN/item_grants.py:175` — worker pobiera blokadę `GET_LOCK`, uzgadnia `queued` z `web_admin_queue`, liczy wszystkie globalne `pending`, a wolne miejsca dopełnia maksymalnie do 10.
- `:193` — `player_offline` wraca do `waiting` po dwóch minutach; `:190` — stare przejęte zadanie przechodzi do `review`, co rozsądnie chroni przed ślepą powtórką niepewnego wydania.
- `files/web_admin.quest:197` — każdy timer gracza widzi tylko 10 najstarszych globalnych `pending`, a dopiero później porównuje nazwę z własną. Duża kolejka innych poleceń może opóźniać dostarczenie. To ograniczenie architektury, nie dowód trwałego zakleszczenia: są mechanizmy wygaszania.
- `:403` i `:427` — sweep bierze 5 starszych niż 30 sekund i oznacza je `player_offline`, bez bezpośredniego sprawdzenia online. Przeciążony/timerowo nieobsłużony bot może więc dostać mylący powód i kolejne odroczenie.
- Worker działa w pętli co 3 sekundy, ale samo istnienie skryptu nie potwierdza, że proces działa i że wszystkie core’y mają aktualny skompilowany quest.

**Diagnostyka przed naprawą zatrzymania** — odczyty na kopii bazy lub uprawnionym połączeniu, bez resetowania kolejki:

```sql
SELECT batch,status,COUNT(*) AS n,MIN(next_try) AS earliest_retry,
       MAX(updated) AS last_update
FROM player.web_seban_grants
GROUP BY batch,status ORDER BY last_update DESC;

SELECT status,cmd,COUNT(*) AS n,MIN(created) AS oldest
FROM player.web_admin_queue
GROUP BY status,cmd ORDER BY oldest;

SELECT g.id,g.batch,g.player_id,g.status,g.quantity,g.queue_id,
       g.next_try,g.updated,q.status AS queue_status,q.cmd,q.created
FROM player.web_seban_grants g
LEFT JOIN player.web_admin_queue q ON q.id=g.queue_id
WHERE g.status IN ('waiting','queued','review')
ORDER BY g.updated,g.id LIMIT 100;
```

Zebrać identyfikator paczki i wyjaśnić każdy niedostarczony rekord: offline, pełny plecak, brak workera, nieaktualny quest, globalna kolejka, przejęte zadanie bez wyniku, warunek filtra już niespełniony. Dodać heartbeat workera i statystyki najstarszego oczekującego zadania. Najpierw rozstrzygnąć, czy 333 to stan terminalny, czy kolejni odbiorcy są offline. Logowania/wylogowania botów są częścią reprodukcji.

**Naprawa wydawania:** wprowadzić usługę/binding dostarczającą żądaną ilość w legalnych stosach z kontrolą przestrzeni i jednoznacznym wynikiem (`requested`, `delivered`, `reason`). Do czasu jej wdrożenia UI i serwer muszą zgodnie odrzucać nieobsługiwane ilości; samo zwiększenie typu BYTE nie rozwiązuje stosów i częściowych wydań. Zadanie identyfikować trwałym `grant_id`. Używać dziennika wydania lub równoważnego protokołu odzyskiwania po awarii. Transakcja SQL w panelu nie obejmuje automatycznie pamięci i zapisu przedmiotów serwera gry. Nie resetować `review` masowo do `waiting`: mogłoby to powielić przedmioty wydane przed utratą potwierdzenia.

**Naprawa obsługi kolejki, jeśli potwierdzi ją reprodukcja:** kierować zadania po numerycznym PID i wybierać tylko zadania własnej postaci, z indeksem `(player_id,status,id)` i atomowym claimem. Wymaga to migracji kolejki, producentów i questa, nie prostego dopisania kolumny do SELECT. Nie interpolować nazwy gracza w SQL. Zachować uczciwe ponawianie dla offline oraz rozdzielić timeout obsługi od rzeczywistego stanu offline. Sprawdzić też wyścig między odczytem `pending`, claimem w grze i nieudaną próbą anulowania w workerze: uzgadniać aktualny status, zamiast opierać decyzję na starym odczycie.

**Testy:** paczka >333, np. 500 botów z mieszanką online/offline/pełny plecak; kilkadziesiąt minut i ponowne logowanie; równoległe zwykłe komendy panelu; restart workera; przerwanie po claimie i po wydaniu przed wynikiem; ilości 1, 200, 255, 256, 300, 65 535; dwie paczki dla tego samego PID; dwa workery. Każdy rekord ma wyjaśniony stan, brak zawężenia ilości, duplikatów i cichego kończenia po stałej liczbie odbiorców. Uzupełnić `SEBAN/test_item_grants.py` o zachowanie i awarie, nie same porównania fragmentów SQL.

## D03. Kowal mimo pełnego +9: planer obiecuje coś, czego wykonawca nie wykona

[Zgłoszenie, 10.09, 19:51](https://discord.com/channels/1543720413035499650/1547665939309404223/1547665939309404223), powiązane [nieużywanie bodzi](https://discord.com/channels/1543720413035499650/1547572360746369034/1547572360746369034).

**Potwierdzona rozbieżność:** `AI/playerbot_economy.h:1369`, `HasPlayerBotRefineOpportunity`, sprawdza również kandydatów w plecaku i korzysta z `CanPlayerBotAttemptRefineItem` (`:1349`). Sprawdza receptę, Yang, materiały i docelowy plus. Tymczasem wykonawca zbierający kandydatów w `:909` dodatkowo odrzuca `IsPlayerBotJunkItem` w `:920`. Planer nie ma tej samej kontroli. `AI/playerbot_planner.h:28` uznaje rozpoczętą wizytę z `bTownNeedBlacksmith` za zobowiązanie, które ma pierwszeństwo przed zwykłym wyborem celu.

W efekcie istnieje ścieżka: przedmiot w plecaku dopuszczony przez ogólny selektor, ale przeznaczony przez ekonomię na złom → planer widzi okazję → wykonawca odrzuca. Trzeba jeszcze odtworzyć to na konkretnym przedmiocie i stanie bota ze zgłoszenia. Samo założone pełne +9 nie wystarcza do zakazu wizyty: bot może zasadnie ulepszać wartościowy zapas lub lepszy przedmiot na później.

**Naprawa:** wydzielić wspólną funkcję budującą wykonalny `RefinePlan` dla konkretnego ID przedmiotu: cel, metoda, zwój, materiały, koszt po rezerwie Yang, powód braku planu. Użyć jej w plannerze i wykonawcy. Uwzględnić ochronę wartościowego przedmiotu, reguły ryzyka, zablokowanie/wystawienie/wymianę, cooldown oraz `IsPlayerBotJunkItem`. Nie duplikować luźno dwóch list warunków. Przy dotarciu do NPC ponownie zweryfikować plan; gdy stał się nieważny, zakończyć zobowiązanie/wyczyścić właściwe flagi i wrócić do wcześniejszego celu. Unieważniać tylko własną wizytę, nie prawidłową operację stajennego.

Dodać status np. `brak materiału: vnum..., potrzeba..., mam...`, `brak zwoju dla wybranej metody`, `brak opłacalnego kandydata`. Samo posiadanie bodzi nie oznacza spełnienia kosztów recepty. W 1.32.4 jest już rozszerzenie używania zwoju poniżej +6 — zachować je.

**Testy:** pełne założone +9 + złom w plecaku → brak nowej wyprawy do kowala; pełne +9 + rzeczywisty wartościowy upgrade → legalny plan; brak materiału/zwoju/środków → odpowiedni powód i brak pętli; utrata/zamiana kandydata po drodze → poprawne zakończenie wizyty; trwająca podróż na exp wznawiana po rzeczywistym załatwieniu potrzeby.

## D04. MariaDB: prawa importu muszą być poprawne przed pierwszym startem

[Zgłoszenie z Debian/Compose 1.32.3, 10.09, 20:40](https://discord.com/channels/1543720413035499650/1547678087662149813/1547678087662149813): skrypt `770 root:root`, dumpy `660 root:root`, `Permission denied`; według zgłaszającego świeży wolumen powstał bez schematów i użytkownika. Nie sprawdzano praw na jego hoście.

**Miejsca:** `DB/initdb.d/10-import-dumps.sh`; mount `./mariadb/initdb.d:/docker-entrypoint-initdb.d:ro` w Compose. Skrypt importu nie może naprawić własnej niedostępności, jeśli entrypoint nie może go nawet odczytać. Rootowy test istnienia plików nie gwarantuje dostępu dla mysql.

**Naprawa:** normalizować tryby katalogów na 755, SQL na 644, skryptu na 755 w przygotowaniu paczki i instalatorze Linuksa, przed startem usługi. Alternatywnie osadzić kontrolowane pliki importu w obrazie z prawami ustawionymi przy buildzie; nie uzależniać ich od trybów źródłowego ZIP-a. Sprawdzić cały łańcuch katalogów. Dodać preflight czytelności jako docelowy użytkownik mysql, test obecności i niepustej treści wymaganych dumpów oraz jednoznaczny błąd przed inicjalizacją wolumenu. Sprawdzać wszystkie wejścia przed rozpoczęciem pierwszego importu.

Proces gotowości powinien rozróżniać bazę odpowiadającą na ping od kompletnego importu: sprawdzić wymagane tabele i niepuste proto oraz zapisać znacznik ukończenia dopiero po pełnym sukcesie. Zrzuty SQL nie są jedną atomową transakcją dla wszystkich tabel/silników; znacznik nie daje sam w sobie rollbacku częściowego importu.

**Istniejące światy:** nie dodawać automatycznego kasowania wolumenu ani ponownego importu dumpów na bazę użytkownika. Naprawa pustego, nieudanego pierwszego startu musi najpierw dowieść, że nie ma danych do zachowania. Dla istniejącego świata stosować wersjonowane migracje i kopię bezpieczeństwa. Instrukcja z Discorda o odtworzeniu wolumenu jest opisem jednego świeżego świata, nie uniwersalnym krokiem aktualizacji.

**Testy:** świeża instalacja na Linux przy `umask 077`, pliki wejściowe 770/660, zwykłe rozpakowanie ZIP; mysql odczytuje wszystko przed inicjalizacją, pięć wymaganych baz importuje się poprawnie, ponowny start nie duplikuje danych. Celowo uszkodzony/brakujący dump daje konkretny błąd i nie udaje gotowości. Aktualizacja istniejącego świata zachowuje postacie i przedmioty.

## D05. ItemShop: naprawić obraz, nie działający kontener

[Zgłoszenie, 10.09, 20:40](https://discord.com/channels/1543720413035499650/1547678161095888896/1547678161095888896): `/var/www/html/itemshop` ma 770 root:root, Apache jako www-data nie czyta `.htaccess`, HTTP 403 i `AH00529`. Ręczna zmiana na 755 pomaga do odtworzenia kontenera.

**Kod:** `linux-port/docker/itemshop/Dockerfile:16` wykonuje `COPY app/ /var/www/html/`, ale nie normalizuje praw. Healthcheck `:31` odpytuje `/itemshop/`.

**Naprawa:** po COPY ustawić kontrolowane prawa katalogów i publicznych plików aplikacji w obrazie (katalogi 755, zwykłe pliki 644, w tym `.htaccess`). Przyznać zapis użytkownikowi webowemu wyłącznie tam, gdzie aplikacja rzeczywiście zapisuje. Nie potrzeba nadawania 777 ani uruchamiania Apache jako root. Jeśli katalog może być przykrywany bind mountem, poprawić również źródło tego mountu; zmiana obrazu nie naprawi przykrywającego go katalogu.

**Testy:** zbudować obraz ze źródłem o prawach 770/660; odczyt jako www-data; HTTP `/itemshop/` i `/ishop`; healthcheck po czystym utworzeniu i odtworzeniu kontenera. Zachować istniejącą regułę `/ishop` używaną przez klienta gry. Brak ręcznego chmod po starcie.

## D06. Migrator: rozdzielić zły login od wolnego importu

[Zgłoszenie, 10.09, 20:41](https://discord.com/channels/1543720413035499650/1547678439883014234/1547678439883014234).

**Potwierdzenie:** `DB/playerbot/apply.sh:65` przechwytuje błąd przez `|| true`. W trzeciej próbie `:69` rozpoznaje Access denied i drukuje trafną diagnozę, ale nie kończy pracy. Dopiero próba 900 (`:84`) przerywa z komunikatem o około 30 minutach i sugeruje recovery świata. To błędna klasyfikacja przyczyny.

**Naprawa:** zachować kod wyjścia klienta i rozróżnić trwałe błędy uwierzytelnienia/uprawnień (np. 1045, 1044), przejściowe błędy połączenia oraz brak gotowego schematu. Po krótkim, ograniczonym potwierdzeniu błędu logowania zakończyć z niezerowym kodem i komunikatem o konfiguracji dostępu. Zachować dłuższy budżet dla faktycznego recovery/importu; nie skracać wszystkich retry do kilku sekund. Diagnostyka nie może wypisywać haseł ani pełnych connection stringów. W przypadku braku wszystkich tabel dać odrębny błąd nieukończonego importu i odniesienie do D04, bez automatycznego zalecania usunięcia świata.

**Testy:** błędne hasło → zakończenie w kilka prób; brak uprawnienia → właściwy komunikat; start MariaDB opóźniony → skuteczne retry; działająca baza, niekompletny import → stan importu; kompletne tabele i proto → normalny start. Nie sprawdzać tylko obecności nowego napisu w logu.

## D07. `log.log.hint`: naprawa zgodna z rzeczywistym kodowaniem danych

[Zgłoszenie, 10.09, 20:40](https://discord.com/channels/1543720413035499650/1547678219254374521/1547678219254374521): domyślne latin1, tabela logów z dumpa big5, błędy polskich nazw. Zgłaszający podaje, że konwersja tabeli do latin1 usuwa problem.

**Stan audytu:** skrypt pierwszego importu tworzy bazy z domyślnym latin1, lecz deklaracja tabeli w dumpie może je nadpisać. W sprawdzonym migratorze nie znaleziono dedykowanej migracji tej tabeli. Nie wykonano odczytu bajtów ani metadanych z bazy zgłaszającego; nie ma podstaw do bezwarunkowej konwersji wszystkich tabel.

**Naprawa:** sprawdzić `SHOW CREATE TABLE log.log`, charset/collation kolumn, ustawienia sesji zapisującego game i reprezentację bajtową kilku poprawnych oraz błędnych nazw (`HEX(hint)`). Ustalić, czy rzeczywiste bajty zgadzają się z deklarowanym charsetem. Zmiana deklaracji i transkodowanie to różne operacje; niewłaściwa konwersja może utrwalić błędne bajty. Po potwierdzeniu kontraktu dodać normalizację świeżego schematu oraz osobną, wersjonowaną i powtarzalną migrację istniejącej tabeli, z kopią i pomiarem czasu/rozmiaru. Nie robić globalnego utf8mb4 „na wszelki wypadek” w starszym serwerze.

**Testy:** zapis i odczyt nazw z polskimi znakami przez rzeczywisty writer gry, zachowanie istniejących rekordów i bajtów po migracji, ponowne uruchomienie migracji bez kolejnego transkodowania. Ustalić, czy błąd powodował wyłącznie utratę wpisu logu, czy wpływał na operację gry; nie założyć drugiego bez reprodukcji.

## D08. Ranking broni 30: komentarz poprawny, implementacja nadal odwrócona

[Nowy wątek, 10.09, 18:50](https://discord.com/channels/1543720413035499650/1547650362247487719/1547650362247487719), także [wznowione starsze zgłoszenie](https://discord.com/channels/1543720413035499650/1547099077404262411/1547655002191826944).

**Źródło prawdy:** `CORE/common/length.h:411` — `APPLY_SKILL_DAMAGE_BONUS` = 71; następna pozycja `APPLY_NORMAL_HIT_DAMAGE_BONUS` = 72. Potwierdzone w lokalnym serwerze, nie założone na podstawie ogólnej wiedzy o Metin2.

**Błędne miejsca:** `SEBAN/app.py:153`, `APPLY_LABELS`, opisuje 71 jako średnie, a 72 jako umiejętności. `:963` liczy `avg_damage` z attrtype=71, a `:964` `skill_damage` z attrtype=72. Komentarze poniżej opisują poprawne enumy, ale nie zmieniają wartości SQL ani słownika. Ekstrakcja AST potwierdziła rzeczywiste etykiety. To nadal błąd audytowanej wersji 1.32.5.

**Naprawa:** wspólne nazwane stałe 71=UM, 72=ŚR; poprawić tooltipy, oba aliasy zapytania, kolejność SQL, filtry i sortowanie UI. Nie zamieniać fizycznie atrybutów przedmiotów w bazie — dane mają prawidłowe identyfikatory, panel błędnie je interpretuje. Poprawka wyłącznie w Pythonowym `sorted` po SQL `LIMIT 100` jest niewystarczająca: do wyniku już trafiło potencjalnie niewłaściwe 100 rekordów.

**Testy:** przedmiot ze ŚR=40, UM=-10 w dwóch różnych slotach atrybutów; wartości ujemne i brak bonusu; pozycje 0–6; ponad 100 broni z rozbieżnymi rankingami ŚR/UM; sortowanie po ŚR, UM i ulepszeniu. Tooltip i ranking pokazują te same liczby co dane i klient gry, a pierwsza setka jest wybrana po właściwym kryterium.

## D09. Handlarz i Wędrowiec: zmienić prezentację, zachować ID

[Zgłoszenie, 10.09, 21:17](https://discord.com/channels/1543720413035499650/1547687485918679142/1547687485918679142), botcobra2/PID 129: personality_id=5, ambition_id=6.

**Kod:** enum w `AI/playerbot_types.h:2893` definiuje MERCHANT=5 i WANDERER=6. `files/admin_panel.py:200` w `BOT_PERSONALITY_LABELS` zamienia te nazwy w obu językach. Kopia stagingu `linux-port/docker/panel/app/admin_panel.py` również wymaga wygenerowania z poprawnego źródła.

**Naprawa:** PL 5=Handlarz, 6=Wędrowiec; EN 5=Merchant, 6=Wanderer. Nie zmieniać kolejności enumu, zapisanej osobowości ani ambicji istniejących botów. Wspólny kontrakt eksportowanych ID między core’em i panelami zapobiegnie kolejnemu rozjazdowi.

**Testy:** fixture statusu personality=5/ambition=6 → Handlarz/Handel; personality=6 → Wędrowiec; obie lokalizacje; nieznane ID daje bezpieczny fallback, nie losową nazwę. Sprawdzić rzeczywisty pakowany panel.

## D10. Zakładki +9 i skille nie pokazują całego oczekiwanego zbioru

[Wątek +9 z kolejnymi uwagami o skillach, 10.09](https://discord.com/channels/1543720413035499650/1547581021858430996/1547581021858430996). Użytkownik wskazuje limit 1000, tylko kilkanaście pozycji, brak tarczy i pojedynczy skill przy kilku P.

**Kod:** `files/admin_panel.py:10186` i `SEBAN/app.py:1012` pobierają ograniczoną próbkę 400 botów dla skilli; wariant Sebana wybiera `max(parse_skills(...))`, czyli jeden najlepszy skill na bota. Dla +9 `files/admin_panel.py:10195` oraz `SEBAN/app.py:1019` używają `i.vnum < 12000` i ostatniej cyfry VNUM=9. Wyklucza to m.in. tarcze z zakresu 13xxx; liczba wyników nie jest równa liczbie wszystkich +9. Seban dodatkowo ma stałe `LIMIT 100`. Limit UI nie może przywrócić rekordów odciętych wcześniej.

**Naprawa:** określić jednostkę rankingu: przedmiot lub bot. Dla zakładki przedmiotów preferować wiersz per ID przedmiotu, uwzględnić obsługiwane okna i kategorie z proto zamiast arbitralnego `vnum < 12000`. Poziom ulepszenia ustalać zgodnie z rzeczywistym proto/łańcuchem refine projektu; sama ostatnia cyfra wymaga jawnego ograniczenia do rodzin, dla których kontrakt jest prawdziwy. Dla skilli albo wyświetlić wszystkie kwalifikujące skille danego bota, albo wprost nazwać zestawienie „najlepszy skill bota” i dać pełną listę w szczegółach. Oczekiwanie z wątku przemawia za pokazaniem wszystkich P/G/M. Sortować właściwy zbiór przed paginacją, liczyć total z tego samego filtra; nie brać top 400 po levelu jako zastępstwa rankingu umiejętności.

**Testy:** broń, zbroja, tarcza, hełm, biżuteria +9; dwa +9 jednego bota; przedmiot niebędący sprzętem z VNUM kończącym się 9; bot z dwoma P; bot spoza pierwszych 400 levelowo, ale z lepszym skillem; ponad 100 i ponad 1000 rekordów. Licznik, paginacja i eksport mają zgodną semantykę.

## D11. Suwak Stragany: wyjątki i życie istniejącego sklepu

[Zgłoszenie, 10.09, 19:47](https://discord.com/channels/1543720413035499650/1547664896227934208/1547664896227934208): minimalny suwak, a około 180 z 280 botów handluje; zgłaszający potwierdza aktualizację. [Devlog 1.32.5](https://discord.com/channels/1543720413035499650/1543721634924593362/1547664688605823007) opublikowano o 19:46:55, zaledwie minutę wcześniej.

**Kod:** `AI/playerbot_town.h:574`, `ShouldPlayerBotKeepShop`: gałąź nadmiaru książek przechodzi już przez ważone losowanie; to obecna poprawka. Nadal są bezpośrednie `true` dla biedy/pełnego plecaka, osobowości handlarza i presji plecaka droppera. W `:1581` ustawiany jest termin zamknięcia sklepu, a `:1634` istnieje ścieżka pozostawienia go przed tym terminem. Zmiana wagi otwierania nie musi natychmiast zamknąć wszystkich sklepów.

Nie dowiedziono, że suwak w ogóle nie dociera do core’a. Najpierw zebrać wersję konfiguracji w każdym core, surową i efektywną wagę, liczbę otwartych sklepów według przyczyny oraz pozostały czas do zamknięcia. Minimalna wartość nie musi znaczyć „wyłącz”. To rozstrzygnąć zgodnie z zakresem UI.

**Naprawa:** określić jawny kontrakt suwaka. Jeżeli ma ograniczać także handlerów i wyprzedaż plecaka, zastąpić bezwarunkowe wyjątki kontrolowaną polityką: krótka, uzasadniona sprzedaż, limit czasu i ponowne sprawdzenie po zmianie ustawienia. Jeśli wyjątki mają pozostać, UI powinno wyjaśniać ich wpływ i pokazywać rozkład powodów. Po zmianie konfiguracji przeliczać istniejące sklepy w kontrolowanym, rozłożonym czasie. Nie zerwać atomowej transakcji handlowej. Nie dodawać sztywnego globalnego limitu sklepów jako rzekomej naprawy bez ustalenia oczekiwanej polityki.

**Testy:** te same kohorty i stany plecaka dla min/default/max; duża populacja przez co najmniej pełny cykl otwarcia–zamknięcia–cooldown; min ustawione, gdy sklepy już stoją; każda gałąź wyjątku. Mierzyć odsetek czasu handlu i liczbę sklepów, a nie pojedynczy screenshot minutę po aktualizacji.

## D12. Bot krąży w M1 mimo komunikatu o Pustyni

[Zgłoszenie, 10.09, 20:50](https://discord.com/channels/1543720413035499650/1547680772520153088/1547680772520153088). Brak PID, dokładnej wersji core’a, mapy królestwa i logu przejść. Nie przypisywać automatycznie nowego objawu do starego błędu portalu: w [starszym wątku](https://discord.com/channels/1543720413035499650/1547219965223108692/1547560977665368085) jest już potwierdzenie poprawy z 12:54.

**Do sprawdzenia:** `AI/playerbot_travel.h`, `playerbot_planner.h` i `playerbot_town.h`. Rozbieżność D03 może wielokrotnie przerywać podróż potrzebą kowala; jest to hipoteza dla tego zgłoszenia. Inne możliwości: nieosiągalny waypoint, niewłaściwy portal dla królestwa, brak obsługi mapy docelowej w core, cooldown/odrzucenie warpu, nieaktualny napis celu.

**Minimalny zapis diagnostyczny:** PID, empire, core/channel, rzeczywista mapa i x/y, cel strategiczny, aktualna akcja i jej właściciel, docelowa mapa i waypoint, powód przerwania, flagi wizyty, wiek celu, czas ostatniego postępu, próby portalu/warpu i wynik. Logować zmiany i timeouty, nie każdą klatkę wszystkich botów. Status gracza ma pokazywać bieżącą czynność np. „uzupełnia zapasy przed Pustynią”, jeśli to prawda.

**Proponowana naprawa po reprodukcji:** podróż jako trwałe zadanie z bieżącym odcinkiem i warunkami ukończenia. Dopuszczalne przerwanie przez rzeczywiste potrzeby przeżycia/zaopatrzenia, potem powrót do zadania. Ograniczyć powtarzane identyczne wizyty bez efektu; po braku postępu unieważnić konkretny odcinek, spróbować innej legalnej drogi i podać powód. Bez teleportowania bota „na skróty” jako domyślnej naprawy i bez losowej zmiany wszystkich waypointów.

**Testy:** M1→M2→mapa pośrednia/Pustynia zgodnie z katalogiem świata dla każdego z trzech królestw; wizyta po poty; pełne EQ+9; zablokowany portal; transfer między core’ami; relog w podróży. Sukces oznacza realną zmianę mapy i wznowienie aktywności, nie sam komunikat celu. Zgrać to z wcześniejszym audytem Shinsoo/Jinno, unikając przywrócenia hardcodów Chunjo.

## D13. Boty stoją i znikają po podejściu: nie mylić AI z synchronizacją widoku

[Zgłoszenie z 10.09, 14:40](https://discord.com/channels/1543720413035499650/1547587580172836965/1547587580172836965), po aktualizacji 1.31.8; odpowiedź wspomina, że drugi restart pomógł i poza miastem nadal ginęły moby. Film nie był oglądany w tym audycie.

**Diagnostyka:** porównać w tym samym czasie pozycję serwerową i przyrost akcji/damage z obrazem klienta. Czy widoczny VID nadal istnieje? Czy po respawnie ma nowy VID? Czy serwer wysłał remove/insert/move do obserwatora po zmianie sectree? Czy problem dotyczy konia, transferu mapy lub usuwania bota? Sprawdzić jeden core i dwa klienty przed restartem, żeby nie utracić dowodów. Znalezionych komentarzy o wcześniejszej poprawce koni nie traktować jako dowodu, że każdy obecny przypadek ma tę przyczynę.

**Naprawa zależna od wyniku:** jeśli serwer rusza botem, naprawić replikację/widoczność i cykl VID. Jeśli brak postępu także na serwerze, zdiagnozować kolejkę/tick, strzały, cel, trasę i flagi blokujące AI. Reset całego świata lub stały okresowy restart nie jest poprawką przyczyny.

**Testy:** wielokrotne wejście/wyjście obserwatora z zasięgu; koń pieszo/jazda; śmierć/respawn; wylogowanie i ponowny spawn; transfer mapy; zgodność pozycji na dwóch klientach oraz brak duchów starych VID.

## D14. Przedmioty innych klas: odetkać sprzedaż, nie złomować kosztowności

[Zgłoszenie, 10.09, 16:09](https://discord.com/channels/1543720413035499650/1547610075093532693/1547610075093532693): szaman trzyma m.in. stal wojownika +9, sura przedmioty innych klas.

**Kod:** `AI/playerbot_economy.h`, `IsPlayerBotJunkItem`, celowo chroni wartościowe ulepszone rzeczy i broń 30. To pozwala na handel między klasami. Samo posiadanie cennego przedmiotu innej klasy nie dowodzi błędu. Problemem jest brak końcowej drogi zagospodarowania przy pełnym plecaku.

**Naprawa:** jawne przeznaczenie przedmiotu: własny upgrade, uzasadniona rezerwa, handel, konsumpcja/materiały, złom. Dla handlu rejestrować pierwszą próbę wystawienia, przyczynę niewystawienia (brak miejsca w sklepie, filtr, cena, konflikt rezerwacji) i wiek zapasu. Uzgodnić `IsPlayerBotJunkItem`, ocenę ekwipunku i kwalifikację oferty. Stare niesprzedające się dobra mogą mieć stopniową korektę ceny lub trafić do wspieranego magazynu, ale nie automatycznie za grosze do NPC tylko dlatego, że klasa się nie zgadza. Nie dodawać nowego magazynu bez uwzględnienia trwałości i limitów istniejącej gry.

**Testy:** przedmiot innej klasy +9 trafia do handlu, nie jest zakładany i nie znika; tani złom może zostać sprzedany; materiały i zwoje są chronione; zapełniony sklep/plecak ma wyjaśniony, kończący się stan; ograniczenie Straganów z D11 nie blokuje trwale gospodarowania plecakiem.

## 3. Zgłoszenia już obsłużone lub wymagające wyłącznie potwierdzenia wydania

| Zgłoszenie | Co ustalono | Działanie Claude’a |
|---|---|---|
| [Linux/VPS — patch 0009](https://discord.com/channels/1543720413035499650/1547559014185963540) | `linux-port/docker/prepare-context.sh:218` tworzy drzewo próbne i nakłada serię kumulatywnie; to poprawia dawny niezależny dry-run | Zachować. Test świeżego stagingu i zależności 0001→0009; nie wracać do samodzielnego dry-run każdego patcha |
| [Kupowanie broni 75 i stalek u NPC](https://discord.com/channels/1543720413035499650/1547582092257271938) | `AI/playerbot_gear.h:1389`, `FindPlayerBotMerchantOffer`, sprawdza stock NPC; `BuyPlayerBotProgressionGear:1419` wymaga oferty | Poprawka obecna. Test braku oferty i poprawnej ceny; upewnić się, że każda ścieżka zakupu podlega regule. Nie kasować już posiadanych przedmiotów bez ustalenia pochodzenia |
| [Sprzedaż bodzi do NPC](https://discord.com/channels/1543720413035499650/1547552011593973760) | Jest ochrona przez `IsPlayerBotRefineScroll` w `IsPlayerBotJunkItem` (`AI/playerbot_economy.h:583`); maintainer wskazuje 1.31.8 | Zachować i testować wszystkie wspierane warianty, także przed pełnym plecakiem. Oddzielić od problemu nieużycia przy refine (D03) |
| [Bodzie nieużywane do ulepszeń](https://discord.com/channels/1543720413035499650/1547572360746369034) | W 1.32.4 uwzględniono użycie także poniżej +6 | Sprawdzić receptę, materiały, koszt i plan konkretnego bota; nie ponawiać identycznej poprawki w ciemno |
| [Strona nie działa](https://discord.com/channels/1543720413035499650/1547557640928755712) | Brak startera w pobraniu; maintainer rozpoznaje uszkodzoną MyISAM `player.quest`. Obecny migrator ma `mariadb-check --auto-repair --fast --silent` | Nie utożsamiać z ItemShop 403. Zweryfikować wykrycie i raportowanie nieudanej naprawy; dla napraw danych zapewnić backup i zakres konkretnej tabeli, nie reset świata |
| [doker](https://discord.com/channels/1543720413035499650/1547601737731407922) | Autor oznaczył temat rozwiązany o 17:33 | Zamknięte; brak podstaw do wymyślania nowej diagnozy z samego tytułu |
| [Boty nie wchodzą w portal](https://discord.com/channels/1543720413035499650/1547219965223108692) | Potwierdzenie poprawy o 12:54 | Zachować regresję trasy; nowego zgłoszenia 20:50 nie zamykać automatycznie jako duplikatu |

Pozostałe starsze wątki (lochy małp, strzały/łucznik, KU, kupowanie wędek, M2/M3/Pustynia, Safe Zone) są ujęte w indeksie. Przejrzano wiadomości, ale nie przeprowadzono dla każdego oddzielnego testu najnowszego runtime. Nie oznacza to zbiorczego potwierdzenia, że wszystko zostało naprawione. Użyć ich jako zestawu regresji przy dotykaniu wspólnych modułów, szczególnie podróży i ekonomii.

### Uwaga o liczbie botów i trzech królestwach

W wątku migratora pada także pytanie o 2500 kont: 500 Shinsoo, 1500 Chunjo, 500 Jinno. Ścieżka `DB/playerbot/apply.sh:252` przekazuje `@playerbot_seed_kingdoms` do `playerbots_seed.sql`. Rozszerzenie istniejącej kohorty Chunjo o nowe kohorty jest zgodne z celem zachowania starego świata, a nie z automatycznym przenoszeniem istniejących postaci. **Nie „naprawiać” liczby przez usuwanie starych botów albo losową zmianę ich empire.** Odrębnie mierzyć liczbę kont/postaci w bazie i aktywną populację na mapach. Równowagę aktywnych królestw realizować limitem/schedulerem spawnu i konfiguracją, z zachowaniem danych. Szczegóły świata i PvP są w wcześniejszym `AUDYT_SHINSOO_JINNO_DLA_CLAUDE.md`; ten dokument uzupełnia go o problemy z Discorda.

## 4. Wdrożenie do obu ścieżek dystrybucji

1. **Ustalić bazę zmian.** Porównać bieżące źródła z manifestem. Claude może już mieć część poprawek; zachować jego nowe zmiany i nie przywracać starszego stagingu z Downloads.
2. **Naprawiać źródło dystrybuowane.** AI w overlay; silnik przez właściwą ścieżkę patchy/źródeł upstream projektu; panel klasyczny w `files/admin_panel.py`; Seban w swoim katalogu. Sam hotfix w `linux-port/docker/game/src` lub kopii panelu może zniknąć przy następnym prepare-context.
3. **Małe, przeglądalne pakiety.** Oddzielić: etykiety/rankingi; uprawnienia i diagnostykę instalacji; refine/town; bezpieczne nagrody; kolejkę masowych nadań. Dopiero po reprodukcji osobne poprawki nawigacji i widoczności. Wspólne zmiany nie mogą przypadkiem zmienić reguł królestw.
4. **Migracje istniejącego świata.** Zmiany charsetu i kolejki wersjonowane, z rozpoznaniem starego schematu, backupem i testem powtórnego uruchomienia. Jawne zachowanie starszego questa/panelu podczas aktualizacji; nie publikować producenta nowych komend, zanim działają ich konsumenci.
5. **Staging i build.** Wykonać czyste przygotowanie kontekstu na Linuksie oraz ścieżkę Windows/launchera. Zweryfikować, że poprawione overlaye/panele trafiają do obrazu, a quest jest faktycznie skompilowany i załadowany na każdym obsługującym boty core. Porównać SHA-256 źródła/stagingu, wersję obrazu i znacznik zgodności questa.
6. **Testy izolowane.** Najpierw małe deterministyczne scenariusze z sekcji D01–D14, potem świeży świat Linux oraz aktualizacja kopii istniejącego świata. Test masowej dystrybucji musi przekroczyć 333 odbiorców. Test handlu trwa pełny cykl sklepów. Test podróży obejmuje trzy królestwa i łatwe lochy małp zgodnie z katalogiem projektu.
7. **Odbiór działania.** Raportować wersję i hash rzeczywistego core’a/panelu/questa, liczbę sukcesów/odroczeń/błędów, wyniki reprodukcji i pozostałe niewiadome. Ogłoszenie w devlogu i komentarz „naprawiono” są wskaźnikiem do weryfikacji, nie substytutem testu.

Nie przeprowadzać restartu uruchomionej gry w ramach samego przygotowania tego audytu. Zmiany wdraża Claude w koordynacji z użytkownikiem i swoim aktualnym zadaniem.

## 5. Gotowa instrukcja startowa dla Claude’a

> Przeczytaj `AUDYT_DISCORD_DLA_CLAUDE_2026-09-10.md` i porównaj źródła z `DISCORD_DOWODY_I_WERSJA.json`. Bazą audytu był checkout 1.32.5, a nie stara instalacja w Downloads. Najpierw napraw potwierdzone D08–D09 i D06 oraz przygotuj D04–D05. Następnie ujednolić plan i wykonanie refine (D03), zabezpieczyć paczki nagród (D01) i wyjaśnić stan konkretnej paczki masowego wydawania (D02), osobno naprawiając błąd zakresu ilości. Przy D11–D14 najpierw uzyskać minimalną reprodukcję i diagnostykę. Zachowaj istniejące poprawki i prace nad Shinsoo/Jinno. Każdą zmianę doprowadź do źródła pakowanego na Linux i Windows, wykonaj opisane testy i podaj rzeczywisty wynik. Nie kasuj świata, nie zmieniaj ID osobowości ani atrybutów przedmiotów w bazie w ramach poprawiania etykiet.

## Załączniki

- `DISCORD_INDEKS_ZGLOSZEN.md` — komplet 37 przejrzanych wątków, daty, odnośniki do devlogu.
- `DISCORD_DOWODY_I_WERSJA.json` — hashe audytowanych źródeł, indeks wiadomości i wyniki ekstrakcji stałych.
- Skrypty `build_discord_evidence.py` i `verify_discord_audit.py` pozostają w katalogu roboczym audytu w Documents/Codex/2026-09-10. Odtwarzają indeks i sprawdzają odnośniki, hashe oraz pliki bez importowania aplikacji. Wymagają lokalnych surowych pobrań; nie są częścią trzyplikowego pakietu dla Claude’a.

Żaden plik przekazywanego pakietu nie zawiera tokenu Discorda. Surowe wiadomości nie są wymagane do wdrożenia; odnośniki umożliwiają sprawdzenie kontekstu na serwerze.
