# Klient (`linux-port/client-root/`): pliki, ktore podmieniamy w `pack/root.epk`

> Ten opis lezy obok katalogu, a nie w nim. `tools/eterpack.py repack`
> bierze *kazdy* plik z katalogu podmian, wiec README lezacy w srodku
> wjezdzal do paczki klienta jako `readme.md`. Teraz repack odmawia
> dokladania plikow, ktorych archiwum nie ma, ale opis i tak nie ma tam
> czego szukac.


Klient trzyma skrypty w jednym archiwum w `pack/`: r40250 w `root.epk` z
indeksem w `root.eix`, mt2009 w `root.data` z indeksem w `root.index`. Nic na
maszynie gracza nie dopisze pliku do tego archiwum, więc zmiana po stronie
klienta jedzie jako gotowa para plików w paczce klienta (składnik `client` w
`update-manifest.json`, nakładany przez launcher na folder klienta).

`linux-port/client-root/` to źródła tych plików, które różnią się od stockowego
klienta — dziś sześć plików panelu GM na F9, przycisków EQ/Sprawdź i zapisanych
miejsc na minimapie (autor: OskarPWA): `game.py`, `interfacemodule.py`,
`uitarget.py`, `constinfo.py`, `uiminimap.py` i `uiscript/equipmentdialog.py`.

Katalog jest **nakładką, a nie kompletem**: repack przepisuje archiwum ze
wszystkimi jego plikami i podmienia tylko te, które tu leżą. Wcześniejsze
zmiany klienta (`gamerules.py`, `intrologin.py`, `uiitemshop.py`, `uisystem.py`,
`uitooltip.py` z `clientrootify.py`) są już wypieczone w archiwum klienta
odniesienia, więc przepakowuje się **jego** root — nie stockowy z paczki
producenta, bo ten by je cofnął.

Przepakowanie (potrzebuje python-lzo; obraz `m2-eterpack:dev`, czyli
`python:3.12-slim` + `liblzo2-dev`). Na mt2009 profil jest obowiązkowy, bo
archiwum ma inne klucze i inne nazwy plików:

    python tools/eterpack.py --profile mt2009 repack <klient>/pack/root builds/clientpack/new/root linux-port/client-root

Sprawdzenie: rozpakuj archiwum sprzed i po, i porównaj — różnić się mogą
wyłącznie podmienione pliki, reszta bajt w bajt.

Potem `tools/New-M2UpdatePackage.ps1 -Type client` z listą
`launcher/client-update-files.txt` i wpis `client` w manifeście.
