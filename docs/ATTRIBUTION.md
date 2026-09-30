# Pochodzenie projektu i atrybucja / Project provenance and attribution

## Linia 2.x / The 2.x line

Linia 2.x jest przerobka **MT2009 PLUS** (ZAXEP/SIZOWSKI,
https://github.com/zaxerrrr-dot/mt2009-sp-plus), a ten jest przerobka
**Metin2 Playerbots** (Tieru, https://github.com/TieruYT/metin2-playerbots).
Oba sa na licencji CC BY-NC-SA 4.0 i ten projekt tez: podajemy autorow, lista
zmian jest w CHANGELOG.md, projekt nie sluzy celom komercyjnym.

The 2.x line is a modification of **MT2009 PLUS** (ZAXEP/SIZOWSKI), itself a
modification of **Metin2 Playerbots** (Tieru). Both are CC BY-NC-SA 4.0, and so
is this project: the authors are credited, the changes are listed in
CHANGELOG.md, and the project is non-commercial.

    Metin2 Playerbots - Tieru - https://github.com/TieruYT/metin2-playerbots - CC BY-NC-SA 4.0
    MT2009 PLUS - ZAXEP/SIZOWSKI - https://github.com/zaxerrrr-dot/mt2009-sp-plus - CC BY-NC-SA 4.0

## Linia 1.x i baza / The 1.x line and its base

## Polski

Metin2 Playerbots powstał na bazie publicznego projektu
`AzzlackSyndicate/metin2-singleplayer-serverfiles-linux`. Z tamtego projektu
pochodzą pierwotna baza portu linuksowego, instalatory, opakowanie Docker oraz
panel administracyjny. Oryginalna historia Git, autorzy commitów i licencja MIT
zostały zachowane. Dziękujemy AzzlackSyndicate za udostępnioną pracę.

Repozytorium źródłowe zostało później ustawione jako prywatne przez autora.
Szanujemy również wycofanie jego WebClienta: ten fork nie publikuje, nie pobiera
i nie wspiera komponentu przeglądarkowego ani jego danych. Projekt działa z
natywnym klientem Windows zgodnym z r40250.

Głównym wkładem tego forka jest serwerowy system Playerbots, jego AI, trwałość
danych, nawigacja, walka, ekonomia, panel Live Map oraz narzędzia deweloperskie.

To repozytorium nie zawiera źródeł serwera Metin2, plików gry, klienta ani
pakietu `[40250] Reference Serverfile`. Prawa do Metin2 należą do Ymir
Interactive/Webzen. Użytkownik musi sam dostarczyć zgodną bazę r40250 i klienta,
których ma prawo używać. Szczegóły zakresu licencji zawiera [NOTICE.md](../NOTICE.md).

## English

Metin2 Playerbots is derived from the formerly public
`AzzlackSyndicate/metin2-singleplayer-serverfiles-linux` project. Its original
Linux port foundation, installers, Docker packaging, and administration panel
came from that work. The original Git history, commit authorship, and MIT
licence have been retained. We thank AzzlackSyndicate for making that work
available.

The upstream repository was later made private by its author. We also respect
the withdrawal of its WebClient: this fork does not publish, fetch, or support
the browser component or its data. The supported play path is a compatible
native Windows r40250 client.

This fork primarily adds the server-side Playerbots system, AI behaviours,
persistence, navigation, combat, economy, Live Map panel, and development
tooling.

This repository contains no Metin2 server source, game files, client, or
`[40250] Reference Serverfile` package. Metin2 belongs to Ymir
Interactive/Webzen. Operators must supply a compatible r40250 baseline and
client they are authorised to use. See [NOTICE.md](../NOTICE.md) for the exact
licensing boundary.
