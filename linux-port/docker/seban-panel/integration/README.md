# Integracja zarządzania serwerem — panel 1.34.0

Te pliki rozszerzają kontener gry, a nie kontener panelu. Przed przebudowaniem gry porównaj `m2-supervise` i `entrypoint.sh` z bieżącym wydaniem Tieru; dołączone wersje bazują na 1.29.10. Nie zastępuj nowszych zmian autora bez połączenia różnic.

`playerbot-quest-login.patch` jest wymagany przez narzędzie masowych nadań. Standardowa ścieżka logowania Playerbotów omija moment, w którym silnik uruchamia timery questów. Patch odkłada ten moment do czasu, aż dane questa dotrą z bazy. Bez niego kolejka będzie prawidłowo utworzona, lecz aktywne boty nie odbiorą przedmiotów. Patch nakładaj na źródła `game/src/server/game/src/` danego wydania, a następnie odbuduj wyłącznie usługę `game`.

- `m2-map-regens`: oddzielne ustawienia `regen.txt` (potwory) i `stone.txt` (Metiny). Pozostawia pliki NPC i bossów poza zakresem. Kopie `.m2orig` przechowują oryginały danego obrazu; reset odtwarza różne domyślne czasy grup, nie jeden wymyślony czas mapy.
- `m2-server-settings`: co pięć sekund publikuje sygnał gotowości `server-settings.ready`, odbiera kompletny plik `server-settings.request` ze wspólnego wolumenu `/opt/m2spool`, sprawdza wartości, stosuje zmiany przez natywne helpery i zwraca nadzorcy jedno żądanie restartu. Nie uruchamia poleceń podanych przez formularz.
- `m2-supervise`: obsługuje wspólne zlecenie, publikuje etapy oraz zapisuje ukończone restarty i automatyczne podniesienia pojedynczych rdzeni. Istniejące kolejki klasycznego panelu pozostają obsługiwane.
- `entrypoint.sh`: przed oddaniem uprawnień użytkownikowi `metin2` wywołuje `m2-map-regens prepare`. Helper sprawdza indeksy map i przygotowuje uprawnienia do dozwolonych plików.

Skrypty muszą znaleźć się w `linux-port/docker/game/bin/` kontekstu budowania gry, z końcami linii LF i uprawnieniami wykonywania. Następnie przebuduj usługę `game` i odtwórz ją z `--no-deps`, aby nie restartować bazy. Samo przebudowanie panelu nie instaluje tej integracji. Od wersji 1.38.2 panel blokuje nowe zlecenia, dopóki sygnał `server-settings.ready` nie potwierdzi obecności helpera; chroni to przed zleceniem, które nigdy nie zostałoby wykonane.

Obsługiwane mapy: ID 1 = Shinsoo M1 / Yongan, 21 = Chunjo M1 / Joan, 41 = Jinno M1, ich M2 (3/23/43), Łatwy Loch Małp (25), Góra Sohan (61), pustynia (63), Dolina Orków (64, `map_n_threeway`), Loch Pająków V1 (104), Loch Małp Normalny (108) i Loch Małp Trudny (109). Wszystkie trzy lochy małp oraz Loch Pająków V1 udostępniają wyłącznie respawn potworów, ponieważ ich pakiet map nie ma pliku `stone.txt`. Przed zmianą plików mapowanie jest sprawdzane względem pliku `map/index` z obrazu gry.

Daty restartów są zapisywane jako Unix timestamp i wyświetlane w strefie Europe/Warsaw. Zlecenie panelu ma źródło `panel`; start spoza panelu jest opisany jako źródło nieustalone, ponieważ restart kontenera może pochodzić zarówno od Dockera, jak i operatora. Automatyczna odbudowa pojedynczego rdzenia jest osobnym wpisem i nie zastępuje daty pełnego restartu.

Testy (Linux, bez dotykania danych produkcyjnych):

```bash
python3 integration/test_settings.py /ścieżka/do/game/bin/m2-rates
python3 test_manage_settings.py
```

Drugi test wymaga zależności panelu. Testy obejmują odrębny respawn, przywracanie domyślnych, walidację całego zestawu, cofnięcie błędnego zapisu, izolację starych kolejek, jeden cykl zatrzymania/uruchomienia, blokadę podwójnego kliknięcia oraz renderowanie `/manage`.
