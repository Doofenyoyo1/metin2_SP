# Przekazanie Claude: zakupy, skille, biolog i pełne sklepy

18.09.2026. Zmiany są już zapisane w roboczym repozytorium, bez commita, zmiany VERSION, restartu świata ani release. Baza: 2.0.71, HEAD `b172752`. Przed pracą przeczytano CONTRIBUTING.md i istotne instrukcje CLAUDE.md. Zmieniono kanoniczny overlay, nie wygenerowany staging.

## Wdrożone

- Nowy `playerbot_progression_needs.h`: wspólny odczyt brakujących KU, Kamieni Duchowych i próbek otwartego etapu zbiórek biologa. Kolejka wypraw biologa nie jest mutowana przez ten odczyt. Zioła i kamienie questowe nie stają się towarem.
- KU liczone z całego plecaka, po liczbie sztuk i skill ID, z pominięciem dodatkowych komórek zajętych przez ten sam przedmiot. Pozycja oferty sprzedawcy nie wpływa na zapas kupującego.
- Popyt na 50513 przy G1..G10, zapas do 3 sztuk. Popyt na KU kończy się po G. Nadmiernie duża paczka nie jest automatycznie kupowana tylko dlatego, że brakuje jednej sztuki.
- Potrzeby rozwoju uruchamiają poszukiwanie na rynku; dodano też ograniczoną wyprawę do M1, gdy ledger wykazuje podaż. Start rozproszony 1–31 min, okno wyprawy 10 min, kolejna próba po 30–45 min. Próby konne zachowują priorytet. To bounded retry, nie nieskończony powrót do miasta.
- Zakupy rozwojowe korzystają z 30% własnego dostępnego budżetu, nie z 30% mediany portfeli. Dodatkowa granica: dwukrotność istniejącej wyceny danej oferty. Rezerwy konia/teleportów i shopping floor pozostają chronione. Zakupy strategiczne wyposażenia zachowują odrębny istniejący limit.
- Wspólna walidacja ceny/budżetu w klasycznych i offline sklepach, również bezpośrednio przed zakupem. Native manager i DB nadal rozstrzygają transakcje; brak tworzenia itemów/gotówki po stronie AI.
- Offline browser wznawia skan także po item ID wewnątrz sklepu. Nadal najwyżej 64 linie na przebieg. W badanej puli preferuje zakup rozwojowy przed opcjonalnym; nie kończy na pierwszej napotkanej nieistotnej ofercie.
- Własny sklep może oddać botowi potrzebny, mieszczący się w limicie zapas KU/50513/próbek przez istniejący remove/ACK.
- 50513 nie jest już bezwarunkowo wyłączony ze sprzedaży: dostępne oddzielne nadmiarowe stosy mogą trafić do obrotu z zachowaniem zapasu właściciela. KU i 50513 mają docelową pojedynczą jednostkę sprzedaży w istniejącym splitterze.
- Reader wybiera wykonalną księgę: cooldown głównego skilla nie blokuje gotowego drugiego skilla. BOOKS nie zużywa uprzednio zbędnego zwoju egzorcyzmu.
- Trening G pomija skill z nieosiągalnym kosztem rangi, jeśli inny G można trenować. Brak rangi dla wszystkich dostępnych G daje dodatkowego kandydata normalnego polowania, ważonego SKILL. Nie przyznaje rangi i nie pozwala zejść poniżej zera.
- Utrzymanie ofert: do 4 przeglądanych pozycji w ograniczonej sesji, z native ACK pomiędzy mutacjami. Zakończone serie przeplatają się z okazją uzupełnienia towaru. Dodawanie itemów nie blokuje stale przecen, a przeceny nie blokują stale dodawania.
- Nieznany wiek ofert po restarcie nie jest liczony jako uptime procesu. Osobno zapisywany jest początek bieżącej obserwacji; historyczny `when=0` pozostaje nieznany w statystyce sprzedaży.

## Testy rzeczywiście wykonane

PASS: `tests/playerbot_progression_needs_test.cpp`, kompilator MSVC 14.44, C++20, asercje aktywne. Pokrycie: stosy i końcowe komórki torby, niezależność od komórki oferty, limity KU/G/P, łańcuch biologa, ukończenie/faza kamienia/dropper/niedostępna mapa, ranga, ograniczone wyprawy i priorytet próby konnej.

PASS: `tests/playerbot_offline_scan_test.cpp`, ten sam kompilator. Testuje produkcyjny BrowseLines, a nie jego kopię: 100 pozycji w jednym sklepie, drugi sklep, limit 64, dojście do pozycji 90, usunięty item kursora i usunięty właściciel.

PASS: istniejący `tests/playerbot_offline_policy_test.cpp`: ordering, refusals, duplicate suppression, relog, timeout, clock wrap, 1118 grid cases i rezerwa portfela.

PASS: `git diff --check`.

Pierwsza wersja zmian przeszła `g++ -fsyntax-only -m32 -std=c++23` względem aktualnych nagłówków mt2009 w jednorazowym `m2mt2009-builder:dev`. Później dopisano wyprawy, politykę skanowania, obsługę rangi i korekty serwisu. **Końcowy diff NIE ma jeszcze potwierdzonej kompilacji całego modułu Linux**: po wznowieniu środowiska Docker Desktop był wyłączony (`dockerDesktopLinuxEngine: The system cannot find the file specified`). Nie uruchamiano Desktop ani stosu gry, żeby nie wznowić niezamierzenie usług świata. Testy samodzielnych polityk wykonano lokalnym MSVC.

## Przed commitem/releasem

1. Przejrzeć diff i nowe pliki; nie dodawać przez `git add .` istniejących obcych katalogów docs/codex-audits. Nowe pliki wymagające dodania: `playerbot_progression_needs.h`, dwa testy C++ i niniejszy handoff.
2. Wykonać końcową kompilację/link właściwego game mt2009 oraz testy C++ w Linuxie. Overlay współdzielony — sprawdzić też składnię konfiguracji r40250, jeśli nadal jest wydawana.
3. Na kopii świata: brak wyłącznie KU → podróż → zakup → czytanie; G bez kamienia → zakup → trening; brak rangi → polowanie. Sprawdzić BOOKS on/off oraz rzeczywiste wymaganie EXP w UseItem.
4. Sprawdzić trójkę królestw, dostępność rynku w split/unified, pełną torbę, nieaktualną cenę i konkurencyjny zakup. Udana sprzedaż oznacza ACK, nie purchase_requested.
5. Sprawdzić sesję przecen z wolnym ACK i niepowodzeniem edycji: bez stałego edit mode i bez blokowania zwykłej gry. Przy unresolved nadal obowiązuje ostrożny istniejący journal — nie ponawiać w ciemno wysłanego żądania.
6. Po pilotażu przygotować VERSION/CHANGELOG i release według procesu projektu. Nie oznaczać zmian jako przetestowanych w grze na podstawie samych testów polityki.

## Jawne ograniczenia tej implementacji

- To poprawki wykonania aktualnego AI, nie pełny nowy system uczenia maszynowego, globalny broker rynku ani trwała baza planów rozwojowych.
- Ledger ksiąg jest agregowany po 50300. Podaż informuje o możliwości wyprawy, ale nie gwarantuje właściwego skilla na danym rynku. Oferta zawsze jest walidowana szczegółowo; nieudana wyprawa ma timeout i cooldown. Indeks po skill+mapa to kolejne rozszerzenie, jeśli pomiary wykażą częste puste wyprawy.
- Plan wypraw i kursory pozostają lokalne dla procesu. Restart rozprasza ponowny start, nie odtwarza trwałej kolejki FIFO. Nie zastąpiono wcześniejszych poprawek biologa 2.0.70 nowym schedulerem.
- Limit całej paczki jest celowy. Nadmiar w jednym nierozdzielonym stosie właściciela może czekać na istniejący splitter; nie wprowadzono częściowego zakupu pozycji ani usuwania towarów. Przetestować zwłaszcza 50513 w jednej dużej paczce — konserwatywna ochrona rezerwy może ją zatrzymać zamiast sprzedać.
- Globalny limit native mutacji 1/s pozostaje; nie dodano centralnej sprawiedliwej kolejki DB. Sesje są ograniczone, ale przy dużej populacji należy zmierzyć oczekiwanie klientów.
- Zachowano dolną cenę kosztu ulepszeń. Nikt nie kupuje bezużytecznego towaru tylko po to, żeby opróżnić sklep. Nie dodano masowego wycofywania zalegających itemów ani nowych ekranów panelu.

Te ograniczenia nie są powodem, by pomijać testy przed wydaniem. Materiał bazowy z szerszym planem pozostaje w audycie `sklepy-skille-biolog-20260917/DLA_CLAUDE.md` w katalogu zadania Codex.
