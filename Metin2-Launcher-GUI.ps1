[CmdletBinding()]
param(
    [switch]$SelfTest,
    # The window's own test (Metin2-Launcher-GUI.Layout.ps1): the window is
    # built off screen, every page is walked at three sizes, a picture of
    # each goes to -UiTestOutput, and the process ends before any timer,
    # Docker question, update check or action.
    [switch]$UiSelfTest,
    [string]$UiTestOutput,
    [ValidateSet('pl', 'en')][string]$UiLanguage = 'pl',
    # Another installation for the modules and the configuration, so the
    # window can be tried from a checkout against a working server.
    [string]$ServerRoot = ''
)

$ErrorActionPreference = 'Stop'
if (-not $ServerRoot) { $ServerRoot = $PSScriptRoot }
$root = [IO.Path]::GetFullPath($ServerRoot)
$cliLauncher = Join-Path $root 'Metin2-Launcher.ps1'
$modulePath = Join-Path $root 'launcher\Metin2Launcher.psm1'
$diagnosticsModulePath = Join-Path $root 'launcher\Metin2Launcher.Diagnostics.psm1'
$configPath = Join-Path $root '.m2launcher.json'
# The window's language, read before anything can go wrong: the refusals below
# come before the module that reads the configuration is loaded. Polish unless
# .m2launcher.json says 'en' (the test window takes -UiLanguage). The process
# environment carries it to the modules' UI-Text and to every action started
# from here; Metin2-Launcher.ps1 reads the same file to the same answer.
$script:Lang = 'pl'
try {
    if (Test-Path -LiteralPath $configPath -PathType Leaf) {
        $storedConfig = Get-Content -LiteralPath $configPath -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($storedConfig -and $storedConfig.PSObject.Properties['language'] -and [string]$storedConfig.language -eq 'en') { $script:Lang = 'en' }
    }
}
catch { }
if ($UiSelfTest) { $script:Lang = $UiLanguage }
$env:M2_LAUNCHER_LANGUAGE = $script:Lang

# A text in both languages where it is used, for everything outside the table
# below: the dialogs, the status lines, the log's own lines. The same pair the
# layout file defines for its pages (it keeps its own copy for an older GUI).
function UI-Text([string]$Pl, [string]$En) {
    if ($script:Lang -eq 'en') { return $En }
    return $Pl
}
$logDirectory = Join-Path $root 'launcher-logs'
if ($UiSelfTest) {
    if (-not $UiTestOutput) { throw 'UiTestOutput is required for UiSelfTest.' }
    $logDirectory = Join-Path ([IO.Path]::GetFullPath($UiTestOutput)) 'test-logs'
}
$supportDirectory = Join-Path $root 'support-bundles'
$composeFile = Join-Path $root 'linux-port\docker\docker-compose.yml'
$sessionLog = Join-Path $logDirectory ('launcher-{0}.log' -f (Get-Date -Format 'yyyyMMdd'))
# Protect-M2SessionLogLine's memory of a heading whose password is on the next line.
$script:sessionLogRedaction = @{}

function Write-StartupFailure {
    # Straight to the file: this runs before (or instead of) the window, so
    # Write-LocalLog and its on-screen box may not exist yet.
    param([string]$Text)
    try {
        New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
        [IO.File]::AppendAllText($sessionLog,
            ((UI-Text '{0}  BLAD LAUNCHERA: {1}' '{0}  LAUNCHER ERROR: {1}') -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $Text) + [Environment]::NewLine,
            [Text.UTF8Encoding]::new($false))
    }
    catch { }
}

trap {
    if ($UiSelfTest) { Write-Error $_ -ErrorAction Continue; exit 1 }
    # A launcher that dies before its first log line left nothing behind but a
    # dialog nobody could copy from - after the 2.0.8 restart the session log
    # ended at "Uruchamiam launcher ponownie" and the player saw an error box
    # (11 September). Whatever stops the script is written down first, then
    # shown with its text, so the next report carries the reason.
    Write-StartupFailure ($_ | Out-String)
    try {
        Add-Type -AssemblyName System.Windows.Forms
        [Windows.Forms.MessageBox]::Show(
            ((UI-Text "Launcher nie wystartowal:`r`n`r`n{0}`r`n`r`nSzczegoly sa w folderze launcher-logs." "The launcher did not start:`r`n`r`n{0}`r`n`r`nThe details are in the launcher-logs folder.") -f $_.Exception.Message),
            (UI-Text 'Blad launchera' 'Launcher error'), 'OK', 'Error') | Out-Null
    }
    catch { }
    break
}

foreach ($required in @($cliLauncher, $modulePath, $diagnosticsModulePath, $composeFile)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        # Hyper96's Metin2-Launcher.ps1 was written by the update at 01:20, run
        # at 01:22 and gone at 01:23 with the launcher still open (28 September):
        # the launcher deletes none of these, an antivirus that takes a script
        # which downloads and starts a server for a threat does. Say what to do
        # rather than only which file it was.
        $relative = $required.Substring($root.Length).TrimStart('\')
        # Which antivirus, from Windows' own Security Center, so the report
        # names it: Hyper96's folder was in his antivirus's exclusions and its
        # history was empty, and the file was gone all the same.
        $antivirus = ''
        try {
            $antivirus = (@(Get-CimInstance -Namespace 'root/SecurityCenter2' -ClassName 'AntiVirusProduct' -ErrorAction Stop |
                ForEach-Object { [string]$_.displayName } | Where-Object { $_ }) | Sort-Object -Unique) -join ', '
        }
        catch { }
        throw ((UI-Text "Brakuje pliku launchera: $required`r`n" "A launcher file is missing: $required`r`n") +
            $(if ($antivirus) { (UI-Text "Antywirus w systemie: $antivirus`r`n" "Antivirus on this system: $antivirus`r`n") } else { '' }) + "`r`n" +
            (UI-Text "Launcher sam go nie usuwa - najczesciej zabiera go antywirus (falszywy alarm na skrypt, ktory pobiera i uruchamia serwer).`r`n`r`n" "The launcher never deletes it itself - as a rule an antivirus takes it (a false alarm on a script that downloads and starts a server).`r`n`r`n") +
            (UI-Text "1. Zajrzyj do kwarantanny antywirusa. W Windows: Zabezpieczenia Windows > Ochrona przed wirusami i zagrozeniami > Historia ochrony. " "1. Look in the antivirus quarantine. In Windows: Windows Security > Virus & threat protection > Protection history. ") +
            (UI-Text "Przywroc plik i dodaj folder $root do wykluczen.`r`n" "Restore the file and add the folder $root to the exclusions.`r`n") +
            (UI-Text "2. Jesli nie da sie go przywrocic: pobierz z GitHuba (Doofenyoyo1/metin2_SP, Releases) paczke metin2-server-update-<wersja>.zip " "2. If it cannot be restored: download the package metin2-server-update-<version>.zip from GitHub (Doofenyoyo1/metin2_SP, Releases) ") +
            (UI-Text "i wyjmij z niej $relative do $root. Najpierw dodaj wykluczenie, inaczej antywirus zabierze go znowu." "and take $relative out of it into $root. Add the exclusion first, or the antivirus takes it again."))
    }
}

Import-Module $modulePath -Force
Import-Module $diagnosticsModulePath -Force
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName Microsoft.VisualBasic
# Before the first control exists: an exception thrown inside a button or
# timer handler is logged with its stack and shown with its text, instead of
# the .NET "Unhandled exception has occurred" dialog and an empty log.
[Windows.Forms.Application]::SetUnhandledExceptionMode([Windows.Forms.UnhandledExceptionMode]::CatchException)
[Windows.Forms.Application]::add_ThreadException([System.Threading.ThreadExceptionEventHandler]{
    param($sender, $eventArgs)
    Write-StartupFailure ((UI-Text 'w oknie: ' 'in the window: ') + $eventArgs.Exception.ToString())
    try {
        [Windows.Forms.MessageBox]::Show(
            ((UI-Text "Blad w oknie launchera:`r`n`r`n{0}`r`n`r`nSzczegoly sa w folderze launcher-logs. Okno dziala dalej." "An error in the launcher window:`r`n`r`n{0}`r`n`r`nThe details are in the launcher-logs folder. The window keeps working.") -f $eventArgs.Exception.Message),
            (UI-Text 'Blad launchera' 'Launcher error'), 'OK', 'Error') | Out-Null
    }
    catch { }
})

if ($SelfTest) {
    $cliErrors = $null
    $guiErrors = $null
    $diagnosticErrors = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile($cliLauncher, [ref]$null, [ref]$cliErrors)
    [void][System.Management.Automation.Language.Parser]::ParseFile($PSCommandPath, [ref]$null, [ref]$guiErrors)
    [void][System.Management.Automation.Language.Parser]::ParseFile($diagnosticsModulePath, [ref]$null, [ref]$diagnosticErrors)
    $formProbe = [Windows.Forms.Form]::new()
    $formProbe.Dispose()
    $portGuidance = Get-M2LauncherErrorGuidance -Text 'Bind for 127.0.0.1:7788 failed: port is already allocated'
    $wslGuidance = Get-M2LauncherErrorGuidance -Text 'There was a problem with WSL; wsl.exe exit status 1'
    [pscustomobject]@{
        Gui = 'OK'
        CliParserErrors = @($cliErrors).Count
        GuiParserErrors = @($guiErrors).Count
        DiagnosticsParserErrors = @($diagnosticErrors).Count
        PortErrorParser = $portGuidance.Code
        WslErrorParser = $wslGuidance.Code
        ComposePresent = Test-Path -LiteralPath $composeFile -PathType Leaf
        DockerCliPresent = $null -ne (Get-Command docker -ErrorAction SilentlyContinue)
        ConfigReadable = $null -ne (Get-M2LauncherConfig -ServerRoot $root -ConfigPath $configPath)
    } | ConvertTo-Json
    exit 0
}

[Windows.Forms.Application]::EnableVisualStyles()

# The session log is opened in Notepad and pasted into chat, and Polish letters
# do not survive either. Everything written to the file is transliterated; the
# on-screen box is left alone, because it renders them correctly and there is no
# reason to make the window worse to fix the file. A character that is neither
# ASCII nor in the table - including the replacement character left behind by an
# older, mis-encoded log - becomes a question mark rather than disappearing.
$script:M2_ASCII_MAP = @{
    [char]0x0105 = 'a'; [char]0x0107 = 'c'; [char]0x0119 = 'e'; [char]0x0142 = 'l'
    [char]0x0144 = 'n'; [char]0x00F3 = 'o'; [char]0x015B = 's'; [char]0x017A = 'z'
    [char]0x017C = 'z'
    [char]0x0104 = 'A'; [char]0x0106 = 'C'; [char]0x0118 = 'E'; [char]0x0141 = 'L'
    [char]0x0143 = 'N'; [char]0x00D3 = 'O'; [char]0x015A = 'S'; [char]0x0179 = 'Z'
    [char]0x017B = 'Z'
    [char]0x2013 = '-'; [char]0x2014 = '-'; [char]0x2026 = '...'
    [char]0x2018 = "'"; [char]0x2019 = "'"; [char]0x201C = '"'; [char]0x201D = '"'
    [char]0x00A0 = ' '
}

function ConvertTo-M2AsciiLine {
    param([string]$Text)
    if ([string]::IsNullOrEmpty($Text)) { return $Text }
    $builder = New-Object Text.StringBuilder
    foreach ($ch in $Text.ToCharArray()) {
        if ($script:M2_ASCII_MAP.ContainsKey($ch)) {
            [void]$builder.Append($script:M2_ASCII_MAP[$ch])
        }
        elseif ([int]$ch -lt 128) { [void]$builder.Append($ch) }
        else { [void]$builder.Append('?') }
    }
    return $builder.ToString()
}

function Write-LocalLog {
    # -FileOnly keeps very chatty build output (apt, unpacking) in the session
    # log without flooding the small on-screen box.
    param(
        [Parameter(Mandatory = $true)][string]$Message,
        [switch]$FileOnly
    )
    $line = '{0}  {1}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $Message
    # The file is what players post, so a password never reaches it; the box
    # keeps the line as it was, because that is where the player reads it.
    $fileLine = $line
    try { $fileLine = Protect-M2SessionLogLine -Text $line -State $script:sessionLogRedaction } catch { }
    [IO.File]::AppendAllText($sessionLog,
        (ConvertTo-M2AsciiLine $fileLine) + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))
    if (-not $FileOnly -and $script:logBox -and -not $script:logBox.IsDisposed) {
        $script:logBox.AppendText($line + [Environment]::NewLine)
        # Keep the box bounded so a long build cannot grow it without limit.
        if ($script:logBox.Lines.Count -gt 600) {
            $script:logBox.Lines = $script:logBox.Lines[-400..-1]
        }
        $script:logBox.SelectionStart = $script:logBox.TextLength
        $script:logBox.ScrollToCaret()
    }
}

function New-Button {
    param(
        [string]$Text,
        [int]$X,
        [int]$Y,
        [int]$Width = 210,
        [int]$Height = 52,
        [Drawing.Color]$Color = [Drawing.Color]::FromArgb(45, 110, 190)
    )
    $button = [Windows.Forms.Button]::new()
    $button.Text = $Text
    $button.Location = [Drawing.Point]::new($X, $Y)
    $button.Size = [Drawing.Size]::new($Width, $Height)
    $button.BackColor = $Color
    $button.ForeColor = [Drawing.Color]::White
    $button.FlatStyle = 'Flat'
    $button.FlatAppearance.BorderSize = 0
    $button.Font = [Drawing.Font]::new('Segoe UI Semibold', 10)
    $button.Cursor = [Windows.Forms.Cursors]::Hand
    return $button
}

function Get-LauncherConfig {
    Get-M2LauncherConfig -ServerRoot $root -ConfigPath $configPath
}

# Every word this window shows, in both languages.
#
# The Discord has more and more English speakers and a launcher they cannot read
# is a launcher they run wrongly - the difference between "ZATRZYMAJ I ZAPISZ"
# and "wipe everything" is not guessable. Only the interface is translated here;
# the two web panels are separate applications with their own thousand-odd
# strings and are not covered by this switch.
$script:Strings = @{
    pl = @{
        formTitle    = 'Metin2 Singleplayer Playerbots - All in One'
        title        = 'METIN2 SINGLEPLAYER - PLAYERBOTS'
        subtitle     = 'Prosty launcher: Docker, serwer, klient, aktualizacje i diagnostyka w jednym miejscu.'
        install      = '1. ZAINSTALUJ / PRZYGOTUJ'
        play         = '2. GRAJ (SERWER + KLIENT)'
        launchClient = 'Uruchom takze klienta gry'
        playNoClient = '2. GRAJ (SAM SERWER)'
        docker       = 'URUCHOM DOCKER'
        stop         = 'ZATRZYMAJ I ZAPISZ'
        panel        = 'OTWORZ PANEL WWW'
        client       = 'WYBIERZ KLIENTA'
        update       = 'SPRAWDZ AKTUALIZACJE'
        bundle       = 'ZBIERZ / WYSLIJ LOGI'
        diagnostics  = 'DIAGNOSTYKA'
        openLog      = 'OTWORZ LOG'
        logFolder    = 'FOLDER LOGOW'
        botCount     = 'LICZBA BOTOW (0-2500)'
        importDb     = 'IMPORTUJ BAZE'
        worldBackup  = 'KOPIA / NOWY SWIAT'
        backupDialog = 'Kopia swiata'
        backupInfo   = 'Kopia zapisuje caly swiat - postacie, poziomy, ekwipunek, boty i konta gry - do jednego pliku zip w folderze backups. Serwer zostanie na czas kazdej z tych operacji zatrzymany i zapisany.'
        backupMake   = 'Zapisz kopie swiata'
        backupLoad   = 'Przywroc swiat z kopii'
        backupReset  = 'Wyzeruj swiat i zacznij od nowa (swieza instalacja)'
        backupPick   = 'Wybierz plik kopii'
        backupNone   = 'W folderze backups nie ma jeszcze zadnej kopii. Zapisz najpierw kopie.'
        repairDb     = 'NAPRAW DOSTEP DO BAZY'
        dbAccess     = 'DANE DO BAZY (NAVICAT)'
        gmPanel      = 'PANEL GM F9 (TEST)'
        updateClient = 'AKTUALIZUJ KLIENTA'
        dbAccessTitle = 'Dane do polaczenia z baza'
        dbAccessHint = 'Wpisz te dane w Navicat, HeidiSQL albo DBeaver (typ MySQL/MariaDB, polaczenie TCP). Konto root widzi wszystko, konto gry tylko bazy gry. Baza slucha wylacznie na tym komputerze. Jesli baza odrzuca haslo, kliknij NAPRAW DOSTEP DO BAZY - ustawia oba konta na hasla z pliku .env. Nie wklejaj tych hasel publicznie.'
        dbAccessProtoNote = 'Na plikach 2.x przedmioty i potwory (item_proto, mob_proto) sa w bazie world; player.item_proto i player.mob_proto to tylko widoki. Zmiany w world zostaja po restarcie serwera.'
        startupUpdateTitle = 'Dostepna aktualizacja'
        startupServerUpdate = 'Znaleziono nowsza wersje serwera: {0}' + [Environment]::NewLine + '(zainstalowana: {1})' + [Environment]::NewLine + [Environment]::NewLine + 'Postacie, przedmioty i boty zostana bez zmian. Serwer zostanie przebudowany - postep w logu na dole - a launcher, jesli sie zmienil, uruchomi sie potem ponownie sam.' + [Environment]::NewLine + [Environment]::NewLine + '"Nie teraz" odklada pytanie do nastepnej wersji; przycisk AKTUALIZUJ dziala zawsze.'
        startupClientUpdate = 'Znaleziono nowsza wersje klienta: {0}' + [Environment]::NewLine + '(zainstalowana: {1})' + [Environment]::NewLine + [Environment]::NewLine + 'Podmienia pliki pack w folderze klienta; poprzednie trafiaja do backups\client. Zamknij gre przed aktualizacja.' + [Environment]::NewLine + [Environment]::NewLine + '"Nie teraz" odklada pytanie do nastepnej wersji; przycisk AKTUALIZUJ KLIENTA dziala zawsze.'
        startupUpdateBoth = 'Znaleziono nowsze wersje:' + [Environment]::NewLine + '   serwer i launcher: {0}  (zainstalowana: {1})' + [Environment]::NewLine + '   klient: {2}  (zainstalowana: {3})' + [Environment]::NewLine + [Environment]::NewLine + 'Postacie, przedmioty i boty zostana bez zmian. Serwer zostanie przebudowany - postep w logu na dole - klient dostanie nowe pliki pack (poprzednie trafiaja do backups\client), a launcher, jesli sie zmienil, uruchomi sie potem ponownie sam. Zamknij gre przed aktualizacja.' + [Environment]::NewLine + [Environment]::NewLine + '"Tylko serwer" - gdy nie grasz na kliencie z tego komputera albo aktualizujesz go inaczej. "Nie teraz" odklada pytanie do nastepnej wersji; przyciski AKTUALIZUJ i AKTUALIZUJ KLIENTA dzialaja zawsze.'
        startupUpdateAll = 'Aktualizuj wszystko'
        startupUpdateNow = 'Aktualizuj teraz'
        startupUpdateServerOnly = 'Tylko serwer'
        startupUpdateLater = 'Nie teraz'
        dbAccessOpenEnv = 'OTWORZ PLIK .ENV'
        dbAccessNoEnv = 'Brak pliku linux-port\docker\.env - uruchom najpierw serwer (GRAJ), launcher go utworzy.'
        language     = 'JEZYK / LANGUAGE: POLSKI'
        ready        = 'Gotowy.'
        footer       = '"Zatrzymaj i zapisz" nie usuwa postaci ani postepu botow. Nigdy nie uzywa docker compose down -v.'
        botDialog    = 'Liczba grajacych botow'
        difficulty   = 'POZIOM TRUDNOSCI'
        difficultyDialog = 'Poziom trudnosci swiata'
        coop         = 'COOP: GRA ZE ZNAJOMYMI'
        vps          = 'SERWER NA VPS'
        apply        = 'Zastosuj'
        cancel       = 'Anuluj'
        panelDialog  = 'Ktory panel otworzyc?'
        panelInfo    = 'Oba panele pokazuja ten sam swiat i dzialaja jednoczesnie.'
        panelClassic = "Oryginalny panel`r`nmapa i sterowanie"
        panelSeban   = "Zaawansowany panel seban latino`r`nprofile, rankingi, gospodarka, obciazenie"
        panelPw      = 'Nie moge sie zalogowac (haslo do panelu)'
        importDialog = 'Importuj baze z innej instalacji'
        importInfo   = 'Wybierz zrodlowa instalacje. Jej swiat (postacie, poziomy, ekwipunek) zostanie skopiowany do biezacej instalacji.'
        importOk     = 'Importuj'
        langSwitched = 'Jezyk zmieniony. Uruchom launcher ponownie, zeby zobaczyc zmiane.'
        clientLangTitle = 'Język gry'
        clientLangAsk = "Launcher jest po angielsku, a klient gry po polsku.`r`n`r`nPrzełączyć klienta gry na angielski? Język można potem zmienić na ekranie logowania (Ustawienia)."
        clientLangDone = 'Klient gry uruchomi się po angielsku.'
        clientPickTitle = 'Wybierz plik uruchamiający klienta Metin2'
        clientPickFilter = 'Program klienta Metin2 (*.exe)|*.exe|Wszystkie pliki (*.*)|*.*'
        clientNotChosen = 'Nie wybrano klienta. Użyj przycisku „Wybierz klienta”.'
        clientStartFailed = 'Nie udało się uruchomić klienta'
        stopAskTitle = 'Bezpieczne zatrzymanie'
        stopAsk = "Zatrzymać serwer? Postacie, baza i postęp botów zostaną zachowane.`r`n`r`nTak - serwer i Docker Desktop (Docker zwalnia wtedy pamięć RAM).`r`nNie - sam serwer; Docker zostaje włączony dla innych programów.`r`nAnuluj - nic nie zatrzymuj."
        clientBlockedTitle = 'Windows zablokował klienta'
        clientBlockedPolicy = "Windows nie pozwolił uruchomić klienta gry (metin2client.exe). Zrobiła to Inteligentna kontrola aplikacji (Smart App Control): blokuje programy bez podpisu cyfrowego, których Microsoft nie zna, a klient gry takiego podpisu nie ma.`r`n`r`nTa funkcja nie ma listy wyjątków. Klient uruchomi się dopiero po jej wyłączeniu: Ustawienia > Prywatność i zabezpieczenia > Zabezpieczenia Windows > Kontrola aplikacji i przeglądarki > Ustawienia inteligentnej kontroli aplikacji > Wyłączone. Windows może potem nie pozwolić włączyć jej z powrotem bez ponownej instalacji systemu, więc zdecyduj sam.`r`n`r`nKlienta bierz tylko z pełnej paczki gry albo z aktualizacji w launcherze."
        clientBlockedVirus = "Program antywirusowy zablokował klienta gry (metin2client.exe). Windows Defender potrafi uznać go za zagrożenie (np. Trojan:Script/Wacatac). To fałszywy alarm: plik nie ma podpisu cyfrowego, więc ocenia go heurystyka.`r`n`r`nCo zrobić: Zabezpieczenia Windows > Ochrona przed wirusami i zagrożeniami > Historia ochrony > wpis z metin2client.exe > Akcje > Przywróć. Potem dodaj folder klienta do wykluczeń: Ochrona przed wirusami i zagrożeniami > Zarządzaj ustawieniami > Wykluczenia > Dodaj wykluczenie > Folder.`r`n`r`nWyjątek dodawaj tylko dla klienta z pełnej paczki gry albo z aktualizacji w launcherze."
        clientMissing = "W folderze klienta nie ma już pliku {0}. Najczęściej zabiera go program antywirusowy: Windows Defender potrafi uznać klienta za zagrożenie (np. Trojan:Script/Wacatac). To fałszywy alarm: plik nie ma podpisu cyfrowego, więc ocenia go heurystyka.`r`n`r`nCo zrobić: Zabezpieczenia Windows > Ochrona przed wirusami i zagrożeniami > Historia ochrony > wpis z {0} > Akcje > Przywróć. Potem dodaj folder klienta do wykluczeń: Ochrona przed wirusami i zagrożeniami > Zarządzaj ustawieniami > Wykluczenia > Dodaj wykluczenie > Folder. Na koniec kliknij GRAJ jeszcze raz.`r`n`r`nJeśli przeniosłeś klienta w inne miejsce, wskaż go przyciskiem WYBIERZ KLIENTA."
    }
    en = @{
        formTitle    = 'Metin2 Singleplayer Playerbots - All in One'
        title        = 'METIN2 SINGLEPLAYER - PLAYERBOTS'
        subtitle     = 'One launcher: Docker, the server, the client, updates and diagnostics in one place.'
        install      = '1. INSTALL / PREPARE'
        play         = '2. PLAY (SERVER + CLIENT)'
        launchClient = 'Start the game client too'
        playNoClient = '2. PLAY (SERVER ONLY)'
        docker       = 'START DOCKER'
        stop         = 'STOP AND SAVE'
        panel        = 'OPEN WEB PANEL'
        client       = 'CHOOSE CLIENT'
        update       = 'CHECK FOR UPDATES'
        bundle       = 'COLLECT / SEND LOGS'
        diagnostics  = 'DIAGNOSTICS'
        openLog      = 'OPEN LOG'
        logFolder    = 'LOG FOLDER'
        botCount     = 'BOT COUNT (0-2500)'
        importDb     = 'IMPORT DATABASE'
        worldBackup  = 'BACKUP / NEW WORLD'
        backupDialog = 'World backup'
        backupInfo   = 'A backup writes the whole world - characters, levels, equipment, bots and game accounts - into one zip file in the backups folder. The server is stopped and saved for each of these operations.'
        backupMake   = 'Save a backup'
        backupLoad   = 'Restore from a backup'
        backupReset  = 'Wipe the world and start over (fresh install)'
        backupPick   = 'Choose a backup file'
        backupNone   = 'There is no backup in the backups folder yet. Save one first.'
        repairDb     = 'REPAIR DATABASE ACCESS'
        dbAccess     = 'DATABASE LOGIN (NAVICAT)'
        gmPanel      = 'GM PANEL F9 (BETA)'
        updateClient = 'UPDATE CLIENT'
        dbAccessTitle = 'Database connection details'
        dbAccessHint = 'Enter these in Navicat, HeidiSQL or DBeaver (MySQL/MariaDB, TCP connection). root sees everything, the game account only the game databases. The database listens on this computer only. If it rejects the password, click REPAIR DATABASE ACCESS - it sets both accounts to the passwords in .env. Never paste these passwords anywhere public.'
        dbAccessProtoNote = 'On the 2.x files items and monsters (item_proto, mob_proto) live in the world database; player.item_proto and player.mob_proto are views. Changes in world survive a server restart.'
        startupUpdateTitle = 'Update available'
        startupServerUpdate = 'A newer server version was found: {0}' + [Environment]::NewLine + '(installed: {1})' + [Environment]::NewLine + [Environment]::NewLine + 'Characters, items and bots stay as they are. The server is rebuilt - progress in the log below - and the launcher, if it changed, restarts by itself afterwards.' + [Environment]::NewLine + [Environment]::NewLine + '"Not now" postpones the question until the next version; the UPDATE button always works.'
        startupClientUpdate = 'A newer client version was found: {0}' + [Environment]::NewLine + '(installed: {1})' + [Environment]::NewLine + [Environment]::NewLine + 'Replaces the pack files in the client folder; the previous ones go to backups\client. Close the game before the update.' + [Environment]::NewLine + [Environment]::NewLine + '"Not now" postpones the question until the next version; the UPDATE CLIENT button always works.'
        startupUpdateBoth = 'Newer versions were found:' + [Environment]::NewLine + '   server and launcher: {0}  (installed: {1})' + [Environment]::NewLine + '   client: {2}  (installed: {3})' + [Environment]::NewLine + [Environment]::NewLine + 'Characters, items and bots stay as they are. The server is rebuilt - progress in the log below - the client gets the new pack files (the previous ones go to backups\client), and the launcher, if it changed, restarts by itself afterwards. Close the game before the update.' + [Environment]::NewLine + [Environment]::NewLine + '"Server only" - when you do not play on this computer''s client or update it another way. "Not now" postpones the question until the next version; the UPDATE and UPDATE CLIENT buttons always work.'
        startupUpdateAll = 'Update everything'
        startupUpdateNow = 'Update now'
        startupUpdateServerOnly = 'Server only'
        startupUpdateLater = 'Not now'
        dbAccessOpenEnv = 'OPEN .ENV FILE'
        dbAccessNoEnv = 'No linux-port\docker\.env yet - start the server (PLAY) once, the launcher creates it.'
        language     = 'LANGUAGE / JEZYK: ENGLISH'
        ready        = 'Ready.'
        footer       = '"Stop and save" never deletes characters or bot progress. It never uses docker compose down -v.'
        botDialog    = 'Number of playing bots'
        difficulty   = 'DIFFICULTY'
        difficultyDialog = 'World difficulty'
        coop         = 'CO-OP: PLAY WITH FRIENDS'
        vps          = 'SERVER ON A VPS'
        apply        = 'Apply'
        cancel       = 'Cancel'
        panelDialog  = 'Which panel should open?'
        panelInfo    = 'Both panels show the same world and run at the same time.'
        panelClassic = "Original panel`r`nmap and controls"
        panelSeban   = "Advanced panel by seban latino`r`nprofiles, rankings, economy, load"
        panelPw      = 'I cannot log in (panel password)'
        importDialog = 'Import a database from another installation'
        importInfo   = 'Pick the source installation. Its world - characters, levels, equipment - is copied into this one.'
        importOk     = 'Import'
        langSwitched = 'Language changed. Restart the launcher to see it.'
        clientLangTitle = 'Game language'
        clientLangAsk = "The launcher is in English, but the game client is set to Polish.`r`n`r`nSwitch the game client to English? You can change it later on the login screen (Settings)."
        clientLangDone = 'The game client will start in English.'
        clientPickTitle = 'Choose the Metin2 client program'
        clientPickFilter = 'Metin2 client program (*.exe)|*.exe|All files (*.*)|*.*'
        clientNotChosen = 'No client chosen. Use the "CHOOSE CLIENT" button.'
        clientStartFailed = 'Could not start the client'
        stopAskTitle = 'Safe stop'
        stopAsk = "Stop the server? Characters, the database and the bots' progress are kept.`r`n`r`nYes - the server and Docker Desktop (Docker then frees its RAM).`r`nNo - the server only; Docker stays on for other programs.`r`nCancel - stop nothing."
        clientBlockedTitle = 'Windows blocked the client'
        clientBlockedPolicy = "Windows would not start the game client (metin2client.exe). Smart App Control did it: it blocks programs without a digital signature that Microsoft does not know, and the game client has no such signature.`r`n`r`nIt has no list of exceptions. The client starts only once it is off: Settings > Privacy & security > Windows Security > App & browser control > Smart App Control settings > Off. Windows may not let you turn it back on without reinstalling the system, so decide for yourself.`r`n`r`nOnly take the client from the project's full package or from an update in the launcher."
        clientBlockedVirus = "An antivirus blocked the game client (metin2client.exe). Windows Defender can take it for a threat (e.g. Trojan:Script/Wacatac). It is a false alarm: the file has no digital signature, so a heuristic judges it.`r`n`r`nWhat to do: Windows Security > Virus & threat protection > Protection history > the entry for metin2client.exe > Actions > Restore. Then exclude the client folder: Virus & threat protection > Manage settings > Exclusions > Add an exclusion > Folder.`r`n`r`nOnly make that exception for the client from the project's full package or from an update in the launcher."
        clientMissing = "The client folder no longer holds {0}. As a rule an antivirus took it: Windows Defender can take the client for a threat (e.g. Trojan:Script/Wacatac). It is a false alarm: the file has no digital signature, so a heuristic judges it.`r`n`r`nWhat to do: Windows Security > Virus & threat protection > Protection history > the entry for {0} > Actions > Restore. Then exclude the client folder: Virus & threat protection > Manage settings > Exclusions > Add an exclusion > Folder. Then click PLAY again.`r`n`r`nIf you moved the client somewhere else, point to it with the CHOOSE CLIENT button."
    }
}

function T {
    param([Parameter(Mandatory = $true)][string]$Key)
    $table = $script:Strings[$script:Lang]
    if ($table -and $table.ContainsKey($Key)) { return [string]$table[$Key] }
    return [string]$script:Strings['pl'][$Key]
}

function Switch-LauncherLanguage {
    $config = Get-LauncherConfig
    $config.language = if ($script:Lang -eq 'en') { 'pl' } else { 'en' }
    Save-M2LauncherConfig -Config $config -ConfigPath $configPath
    $script:Lang = $config.language
    $env:M2_LAUNCHER_LANGUAGE = $script:Lang
    Write-LocalLog ("Language: {0}" -f $config.language)
    [Windows.Forms.MessageBox]::Show((T 'langSwitched'), (T 'formTitle'),
        [Windows.Forms.MessageBoxButtons]::OK,
        [Windows.Forms.MessageBoxIcon]::Information) | Out-Null
}

function Save-ClientExecutable {
    param([Parameter(Mandatory = $true)][string]$Executable)
    $config = Get-LauncherConfig
    $config.clientExecutable = [IO.Path]::GetFullPath($Executable)
    $config.clientRoot = [IO.Path]::GetFullPath((Split-Path -Parent $Executable))
    Save-M2LauncherConfig -Config $config -ConfigPath $configPath
    Write-LocalLog (UI-Text "Wybrano klienta: $([IO.Path]::GetFileName($Executable))" "Client chosen: $([IO.Path]::GetFileName($Executable))")
}

function Select-ClientExecutable {
    $config = Get-LauncherConfig
    $dialog = [Windows.Forms.OpenFileDialog]::new()
    $dialog.Title = T 'clientPickTitle'
    $dialog.Filter = T 'clientPickFilter'
    $dialog.CheckFileExists = $true
    if ($config.clientRoot -and (Test-Path -LiteralPath $config.clientRoot -PathType Container)) {
        $dialog.InitialDirectory = $config.clientRoot
    }
    if ($dialog.ShowDialog($script:form) -eq [Windows.Forms.DialogResult]::OK) {
        Save-ClientExecutable -Executable $dialog.FileName
        Confirm-ClientLanguageForLauncher -Executable $dialog.FileName
        return $dialog.FileName
    }
    return ''
}

# The game client keeps its language in game1.cfg beside the exe, one line
# "LANGUAGE <code>" (CPythonSystem::LoadConfig; with no file or no line it is
# Polish), and its own switch is on the login screen and closes the client. A
# player who set the launcher to English had to find it (23 September:
# "zeby nie musieli szukac zmiany jezyka gry"), so the launcher offers it
# itself, when it chooses the client and before it starts one.
function Get-ClientGameLanguage {
    param([Parameter(Mandatory = $true)][string]$ClientRoot)
    $cfg = Join-Path $ClientRoot 'game1.cfg'
    if (-not (Test-Path -LiteralPath $cfg -PathType Leaf)) { return 'pl' }
    foreach ($line in [IO.File]::ReadAllLines($cfg, [Text.Encoding]::Default)) {
        $parts = @($line.Trim() -split '\s+', 2)
        if ($parts.Count -eq 2 -and $parts[0] -ieq 'LANGUAGE') { return $parts[1].Trim().ToLowerInvariant() }
    }
    return 'pl'
}

function Set-ClientGameLanguage {
    param(
        [Parameter(Mandatory = $true)][string]$ClientRoot,
        [Parameter(Mandatory = $true)][string]$Language
    )
    $cfg = Join-Path $ClientRoot 'game1.cfg'
    $lines = New-Object 'System.Collections.Generic.List[string]'
    $written = $false
    if (Test-Path -LiteralPath $cfg -PathType Leaf) {
        foreach ($line in [IO.File]::ReadAllLines($cfg, [Text.Encoding]::Default)) {
            $parts = @($line.Trim() -split '\s+', 2)
            if ($parts[0] -ieq 'LANGUAGE') {
                if (-not $written) { $lines.Add("LANGUAGE`t`t`t`t$Language"); $written = $true }
                continue
            }
            $lines.Add($line)
        }
    }
    # A client that has never run has no file; one line is enough, the client
    # fills in the rest with its defaults and writes the whole file on exit.
    if (-not $written) { $lines.Add("LANGUAGE`t`t`t`t$Language") }
    [IO.File]::WriteAllText($cfg, (($lines -join "`r`n") + "`r`n"), [Text.Encoding]::Default)
}

# Asked only while the launcher is in English and the client in Polish: another
# language is somebody's own choice. Not while the client runs, because it
# writes game1.cfg back when it closes. A "No" is kept for that client folder
# in a file beside the launcher's settings, so it is asked once.
function Confirm-ClientLanguageForLauncher {
    param([string]$Executable)
    if ($script:Lang -ne 'en' -or -not $Executable) { return }
    try {
        $clientRoot = Split-Path -Parent $Executable
        if ((Get-ClientGameLanguage -ClientRoot $clientRoot) -ne 'pl') { return }
        $declinedFile = Join-Path $root '.m2client-language-declined'
        if ((Test-Path -LiteralPath $declinedFile -PathType Leaf) -and
            ([IO.File]::ReadAllText($declinedFile).Trim() -ieq $clientRoot)) { return }
        if ((Get-Command Get-M2FolderProcesses -ErrorAction SilentlyContinue) -and
            @(Get-M2FolderProcesses -Root $clientRoot).Count -gt 0) { return }
        $answer = [Windows.Forms.MessageBox]::Show((T 'clientLangAsk'), (T 'clientLangTitle'),
            [Windows.Forms.MessageBoxButtons]::YesNo, [Windows.Forms.MessageBoxIcon]::Question)
        if ($answer -eq [Windows.Forms.DialogResult]::Yes) {
            Set-ClientGameLanguage -ClientRoot $clientRoot -Language 'en'
            Write-LocalLog "Client language set to English: $clientRoot"
            [Windows.Forms.MessageBox]::Show((T 'clientLangDone'), (T 'clientLangTitle'),
                [Windows.Forms.MessageBoxButtons]::OK, [Windows.Forms.MessageBoxIcon]::Information) | Out-Null
        }
        else {
            [IO.File]::WriteAllText($declinedFile, $clientRoot)
            Write-LocalLog "Client language left in Polish: $clientRoot"
        }
    }
    catch {
        Write-LocalLog "Client language not changed: $($_.Exception.Message)"
    }
}

function Find-ClientExecutable {
    $config = Get-LauncherConfig
    if ($config.clientExecutable -and (Test-Path -LiteralPath $config.clientExecutable -PathType Leaf)) {
        return [string]$config.clientExecutable
    }
    if ($config.clientRoot -and (Test-Path -LiteralPath $config.clientRoot -PathType Container)) {
        foreach ($name in @('metin2client.exe', 'Metin2.exe', 'metin2.exe', 'start.exe', 'launcher.exe')) {
            $candidate = Join-Path $config.clientRoot $name
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                Save-ClientExecutable -Executable $candidate
                return $candidate
            }
        }
    }
    return ''
}

function Get-ClientStartBlock {
    # Whether Windows itself refused the client, and how. Smart App Control and
    # the other application control policies answer with
    # ERROR_SYSTEM_INTEGRITY_POLICY_VIOLATION (4551) or
    # ERROR_ACCESS_DISABLED_BY_POLICY (1260), an antivirus with
    # ERROR_VIRUS_INFECTED (225) or ERROR_VIRUS_DELETED (226). On 25 September
    # Microsoft began taking metin2client.exe for a trojan, and all a player saw
    # was Windows' one sentence about "application control policies".
    param($ErrorRecord)

    $exception = $ErrorRecord.Exception
    while ($exception) {
        if ($exception -is [ComponentModel.Win32Exception]) {
            if (@(4551, 1260) -contains $exception.NativeErrorCode) { return 'policy' }
            if (@(225, 226) -contains $exception.NativeErrorCode) { return 'virus' }
        }
        $exception = $exception.InnerException
    }
    $message = [string]$ErrorRecord.Exception.Message
    if ($message -match 'kontroli aplikacji|Application Control|zasady grupy|group policy') { return 'policy' }
    if ($message -match 'wirus|virus') { return 'virus' }
    return ''
}

function Start-ConfiguredClient {
    $executable = Find-ClientExecutable
    if (-not $executable) {
        # A client that was chosen once and has gone from its folder was, as a
        # rule, taken by an antivirus; the picker would only have asked for a
        # file that is no longer there.
        $gone = [string](Get-LauncherConfig).clientExecutable
        if ($gone -and (Test-Path -LiteralPath (Split-Path -Parent $gone) -PathType Container)) {
            Write-LocalLog (UI-Text "Brak pliku klienta: $gone" "The client file is missing: $gone")
            [Windows.Forms.MessageBox]::Show(((T 'clientMissing') -f [IO.Path]::GetFileName($gone)),
                (T 'clientBlockedTitle'), 'OK', 'Warning') | Out-Null
            return
        }
        $executable = Select-ClientExecutable
    }
    if (-not $executable) {
        [Windows.Forms.MessageBox]::Show(
            (T 'clientNotChosen'),
            'Metin2 Playerbots', 'OK', 'Information') | Out-Null
        return
    }
    # The full packages' two stray executables go, and a launcher that started
    # one starts metin2client.exe; an old metin2client.exe is replaced first,
    # by an action that starts the client when it ends (601210, kordianq1112,
    # 27 September: the exe of a full 2.0.71 under the root of 2.0.41).
    try {
        foreach ($note in @(Repair-M2ClientExecutables -ClientFolder (Split-Path -Parent $executable) -ServerRoot $root -ConfigPath $configPath)) {
            Write-LocalLog $note
        }
        $repointed = Find-ClientExecutable
        if ($repointed) { $executable = $repointed }
    }
    catch { Write-LocalLog (UI-Text "Porządki w plikach klienta nieudane: $($_.Exception.Message)" "Tidying up the client files failed: $($_.Exception.Message)") }
    if (Start-ClientExeRepairIfWanted -LaunchClient) { return }
    Confirm-ClientLanguageForLauncher -Executable $executable
    try {
        Start-Process -FilePath $executable -WorkingDirectory (Split-Path -Parent $executable)
        Write-LocalLog (UI-Text "Uruchomiono klienta: $([IO.Path]::GetFileName($executable))" "Client started: $([IO.Path]::GetFileName($executable))")
    }
    catch {
        $startError = $_
        Write-LocalLog (UI-Text "BŁĄD uruchamiania klienta: $($startError.Exception.Message)" "ERROR starting the client: $($startError.Exception.Message)")
        switch (Get-ClientStartBlock -ErrorRecord $startError) {
            'policy' { [Windows.Forms.MessageBox]::Show((T 'clientBlockedPolicy'), (T 'clientBlockedTitle'), 'OK', 'Warning') | Out-Null }
            'virus'  { [Windows.Forms.MessageBox]::Show((T 'clientBlockedVirus'), (T 'clientBlockedTitle'), 'OK', 'Warning') | Out-Null }
            default  { [Windows.Forms.MessageBox]::Show($startError.Exception.Message, (T 'clientStartFailed'), 'OK', 'Error') | Out-Null }
        }
    }
}

function Invoke-QuickProcess {
    param([string]$FileName, [string]$Arguments, [int]$TimeoutMs = 1800)
    try {
        $psi = [Diagnostics.ProcessStartInfo]::new()
        $psi.FileName = $FileName
        $psi.Arguments = $Arguments
        $psi.UseShellExecute = $false
        $psi.CreateNoWindow = $true
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $process = [Diagnostics.Process]::Start($psi)
        if (-not $process.WaitForExit($TimeoutMs)) {
            try { $process.Kill() } catch {}
            return [pscustomobject]@{ ExitCode = -1; Output = '' }
        }
        return [pscustomobject]@{
            ExitCode = $process.ExitCode
            Output = $process.StandardOutput.ReadToEnd() + $process.StandardError.ReadToEnd()
        }
    }
    catch { return [pscustomobject]@{ ExitCode = -1; Output = $_.Exception.Message } }
}

function Refresh-Status {
    $dockerInstalled = $null -ne (Get-Command docker -ErrorAction SilentlyContinue)
    $dockerProcessesRunning = @(Get-Process -Name 'Docker Desktop', 'com.docker.backend' -ErrorAction SilentlyContinue).Count -gt 0
    $dockerEngineReady = $false
    if ($dockerInstalled) {
        $engineResult = Invoke-QuickProcess -FileName 'docker.exe' -Arguments 'info --format "{{.ServerVersion}}"' -TimeoutMs 2500
        $dockerEngineReady = $engineResult.ExitCode -eq 0
    }
    $script:dockerStatus.Text = if (-not $dockerInstalled) { (UI-Text 'Docker: NIEZAINSTALOWANY' 'Docker: NOT INSTALLED') }
        elseif ($dockerEngineReady) { (UI-Text 'Docker: GOTOWY' 'Docker: READY') }
        elseif ($dockerProcessesRunning) { (UI-Text 'Docker: STARTUJE / WYMAGA NAPRAWY' 'Docker: STARTING / NEEDS REPAIR') }
        else { (UI-Text 'Docker: ZATRZYMANY' 'Docker: STOPPED') }
    $script:dockerStatus.ForeColor = if ($dockerEngineReady) { [Drawing.Color]::LightGreen }
        elseif ($dockerInstalled) { [Drawing.Color]::Gold }
        else { [Drawing.Color]::Tomato }

    $serverRunning = $false
    if ($dockerEngineReady) {
        $composeDirectory = Join-Path $root 'linux-port\docker'
        $result = Invoke-QuickProcess -FileName 'docker.exe' -Arguments (
            'compose --project-directory "{0}" -f "{1}" ps --services --status running' -f $composeDirectory, $composeFile) -TimeoutMs 1800
        $serverRunning = $result.ExitCode -eq 0 -and $result.Output -match '(?m)^game\s*$'
    }
    $script:serverStatus.Text = if ($serverRunning) { (UI-Text 'Serwer: DZIAŁA' 'Server: RUNNING') } else { (UI-Text 'Serwer: ZATRZYMANY' 'Server: STOPPED') }
    $script:serverStatus.ForeColor = if ($serverRunning) { [Drawing.Color]::LightGreen } else { [Drawing.Color]::Silver }

    if ($script:versionLabel) { Update-VersionFooter }
}

# apt/dpkg chatter from a first image build, plus Docker's note about the data
# volume it did not create itself. Both are harmless, but players read the
# word "warning" next to their database and reach for `down -v`, which is the
# one command that would actually destroy the world. Kept in the session log,
# kept out of the on-screen box so the interesting lines stay readable.
$script:M2_NOISY_BUILD = 'already exists but was not created by Docker Compose|Get:\d|Unpacking |Selecting previously|Preparing to unpack|Reading database|Setting up |Suggested packages:|Recommended packages:|The following NEW packages|The following packages will be|debconf:'

# Issue tracker the ZIP button falls back to, and the once-per-session cache of
# the support address read from the update manifest.
$script:openContactAfterAction = ''
$script:supportSettingsCache = $null

function Read-SharedText {
    # The child process still holds these files open for writing.
    param([Parameter(Mandatory = $true)][string]$Path)
    try {
        $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
        try {
            $reader = New-Object IO.StreamReader($stream, [Text.Encoding]::UTF8)
            try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
        }
        finally { $stream.Dispose() }
    }
    catch { return $null }
}

function Set-ActionPhase {
    # One place records the phase, so the clock under it always starts when the
    # phase actually changes. Without that clock a start that had stopped
    # looked exactly like a start that was working: the line read
    # "m2zip-db: Healthy" for eight minutes while the migration behind it
    # could not reach the database at all, and nothing said which it was.
    # A build step is a phase of its own for the clock: "game builder" is the
    # same name for its three steps, and the one that takes the time is the
    # compile in the middle.
    param([Parameter(Mandatory = $true)][string]$Phase, [int]$Step = 0, [int]$Total = 0, [string]$Label = '')
    if ($script:activePhase -ne $Phase -or $script:activePhaseStep -ne $Step) {
        $script:activePhase = $Phase
        $script:activePhaseSince = Get-Date
    }
    $script:activePhaseStep = $Step
    $script:activePhaseTotal = $Total
    $script:activePhaseLabel = $Label
}

function Get-BuildStepLabel {
    # What a BuildKit step is doing, read out of its RUN line. The game core's
    # compile shows as "game builder 2/3 (67%)" for as long as it runs, and
    # nothing on that line said it was a compile or how long it had been going,
    # so a slow one read as a hang ("wiecznie zatrzymuje sie na 67 procentach",
    # Drip, 24 September).
    param([Parameter(Mandatory = $true)][string]$Line)
    if ($Line -match 'make -C game/src') { return (UI-Text 'kompilacja rdzenia gry' 'compiling the game core') }
    if ($Line -match 'make -C db/src') { return (UI-Text 'kompilacja rdzenia bazy' 'compiling the database core') }
    if ($Line -match 'make -C liblua') { return (UI-Text 'kompilacja bibliotek' 'compiling the libraries') }
    return ''
}

function Format-StepClock {
    # m:ss with the whole minutes, so a step past an hour does not wrap to 00.
    param([Parameter(Mandatory = $true)][TimeSpan]$Span)
    return ('{0}:{1:00}' -f [int][Math]::Floor($Span.TotalMinutes), $Span.Seconds)
}

function Update-ActionPhase {
    # Turn BuildKit / Compose chatter into a phase name and a step count, so the
    # progress bar and the status line can show real movement during the long
    # first build instead of an endless marquee.
    param([Parameter(Mandatory = $true)][string]$Line)
    $step = [Regex]::Match($Line, '^\s*#\d+\s+\[([^\]]+?)\s+(\d+)/(\d+)\]')
    if ($step.Success) {
        Set-ActionPhase $step.Groups[1].Value ([int]$step.Groups[2].Value) ([int]$step.Groups[3].Value) (Get-BuildStepLabel -Line $Line)
        if (-not $script:activeBuildNoticed) {
            $script:activeBuildNoticed = $true
            Write-LocalLog (UI-Text 'Trwa budowanie obrazów serwera. Przy pierwszym uruchomieniu to normalnie kilkanaście–kilkadziesiąt minut — nie przerywaj.' 'Building the server images. On the first start this normally takes anything from ten minutes to an hour - do not interrupt it.')
        }
        return
    }
    # The game core's build step says this when the Docker VM has less memory
    # free than its heaviest file needs (see the game Dockerfile). Its RUN line
    # carries the same words, but that line is a step and returned above.
    # "Pamięć" is also what Polish calls disk space, and the status line's
    # "mało wolnej pamięci w Dockerze" sent players with hundreds of free
    # gigabytes to clean their disks (artur554, charliee, uxietoszef, 25-26
    # September): it says RAM now, and the log says once what it means.
    if ($Line -match 'UWAGA: w maszynie Dockera wolne jest tylko') {
        if (-not $script:activeLowMemory) {
            Write-LocalLog (UI-Text 'Maszynie Dockera brakuje teraz wolnej pamięci RAM (to nie jest miejsce na dysku - dysku nie trzeba czyścić). Zwykle dlatego, że aktualizacja kompiluje serwer, gdy stary serwer z botami wciąż działa. Kompilacja potrwa dłużej, ale skończy się - nie przerywaj. Przy następnej aktualizacji kliknij najpierw ZATRZYMAJ I ZAPISZ.' 'The Docker machine is short of free RAM right now (this is not disk space - the disk does not need cleaning). Usually because the update compiles the server while the old server with the bots is still running. The compile takes longer, but it will finish - do not interrupt it. Before the next update, click STOP AND SAVE first.')
        }
        $script:activeLowMemory = $true
        return
    }
    if ($Line -match '^\[(?:faza|phase)\]\s*(.+?)\s*(\(|$)') {
        Set-ActionPhase $Matches[1]
        return
    }
    if ($Line -match 'transferring context:\s*([\d.]+\s*[kKMG]?B)') {
        Set-ActionPhase (UI-Text "przesyłanie plików do budowy ($($Matches[1]))" "sending the files to the build ($($Matches[1]))")
        return
    }
    # The database migration is where a start sits longest, and it says plainly
    # what it is waiting for. Show that instead of the last container line from
    # a minute ago, so "still waiting" cannot be mistaken for progress.
    if ($Line -match 'still waiting for the database \((\d+)s\)') {
        Set-ActionPhase ((UI-Text 'migracja bazy czeka na bazę ({0} s)' 'database migration waiting for the database ({0} s)') -f $Matches[1])
        return
    }
    if ($Line -match 'the database is not answering yet|Unknown server host') {
        Set-ActionPhase (UI-Text 'migracja bazy: baza nie odpowiada' 'database migration: the database is not answering')
        return
    }
    if ($Line -match 'waiting for the complete mt2009 schema') {
        Set-ActionPhase (UI-Text 'migracja bazy: czekam na schemat' 'database migration: waiting for the schema')
        return
    }
    $container = [Regex]::Match($Line, 'Container\s+(\S+)\s+(Creating|Created|Starting|Started|Waiting|Healthy|Recreate|Stopping|Stopped)')
    if ($container.Success) {
        Set-ActionPhase "$($container.Groups[1].Value): $($container.Groups[2].Value)"
    }
}

function Update-ActionStatusText {
    if (-not $script:activeProcess -or -not $script:actionStatus) { return }
    $elapsed = (Get-Date) - $script:activeStarted
    $text = (UI-Text 'Trwa: {0}...  {1:mm\:ss}' 'Running: {0}...  {1:mm\:ss}') -f $script:activeAction, $elapsed
    if ($script:activePhaseTotal -gt 0) {
        $pct = [int](100 * $script:activePhaseStep / $script:activePhaseTotal)
        $pct = [Math]::Max(0, [Math]::Min(100, $pct))
        $text += '   —   {0} {1}/{2} ({3}%)' -f $script:activePhase, $script:activePhaseStep, $script:activePhaseTotal, $pct
        # The step's own clock: the action's timer includes the download and
        # every image built before this one, so it cannot say whether the
        # compile has been going for one minute or for ten.
        $inStep = if ($script:activePhaseSince) { (Get-Date) - $script:activePhaseSince } else { [TimeSpan]::Zero }
        if ($script:activePhaseLabel) { $text += ' — {0} {1}' -f $script:activePhaseLabel, (Format-StepClock -Span $inStep) }
        else { $text += ' {0}' -f (Format-StepClock -Span $inStep) }
        if ($script:activePhaseLabel -eq (UI-Text 'kompilacja rdzenia gry' 'compiling the game core')) {
            $long = $inStep.TotalSeconds -ge $script:activeCompileHintSeconds
            if ($script:activeLowMemory) { $text += (UI-Text '  ⚠ mało wolnego RAM-u w Dockerze (to nie dysk)' '  ⚠ little free RAM in Docker (not the disk)') }
            elseif ($long) { $text += (UI-Text '  ⚠ dłużej niż zwykle' '  ⚠ longer than usual') }
            if ($long -and -not $script:activeCompileHintNoticed) {
                $script:activeCompileHintNoticed = $true
                Write-LocalLog ((UI-Text 'Kompilacja rdzenia gry trwa już {0:N0} min, a zwykle zajmuje 1–5 min. Tak długo trwa najczęściej wtedy, gdy maszynie Dockera brakuje pamięci RAM (nie miejsca na dysku): aktualizacja kompiluje, kiedy stary serwer z botami wciąż działa. Nie przerywaj — restart zaczyna kompilację od nowa. Przy następnej aktualizacji kliknij najpierw ZATRZYMAJ I ZAPISZ.' 'The game core has been compiling for {0:N0} min, and it usually takes 1-5 min. It takes this long mostly when the Docker machine is short of RAM (not disk space): the update compiles while the old server with the bots is still running. Do not interrupt it - a restart begins the compile again. Before the next update, click STOP AND SAVE first.') -f $inStep.TotalMinutes)
            }
        }
        if ($script:progress.Style -ne 'Blocks') { $script:progress.Style = 'Blocks' }
        $script:progress.Value = $pct
    }
    elseif ($script:activePhase) {
        # How long THIS phase has lasted, not only the whole action: a build
        # step that takes four minutes is normal, the same container line for
        # four minutes is not, and the two used to look identical.
        $inPhase = if ($script:activePhaseSince) { (Get-Date) - $script:activePhaseSince } else { [TimeSpan]::Zero }
        $text += '   —   {0} ({1:mm\:ss})' -f $script:activePhase, $inPhase
        if ($inPhase.TotalSeconds -ge $script:activeStallSeconds) {
            $text += (UI-Text '  ⚠ bez zmian — sprawdź DIAGNOSTYKA' '  ⚠ no change - check DIAGNOSTICS')
            if (-not $script:activeStallNoticed) {
                $script:activeStallNoticed = $true
                Write-LocalLog ((UI-Text "Od {0:N0} min nic się nie zmienia na etapie: {1}. To nie musi być awaria (duży świat wstaje wolno), ale jeśli potrwa dalej, użyj DIAGNOSTYKA i ZBIERZ LOGI." "Nothing has changed for {0:N0} min at the stage: {1}. That need not be a failure (a big world starts slowly), but if it goes on, use DIAGNOSTICS and COLLECT / SEND LOGS.") -f $inPhase.TotalMinutes, $script:activePhase)
            }
        }
    }
    $script:actionStatus.Text = $text
}

function Update-ActionStream {
    # Tail the running action's output into the log while it runs, so a long
    # start does not look like a frozen window.
    if (-not $script:activeProcess) { return }
    foreach ($key in @('out', 'err')) {
        $path = if ($key -eq 'out') { $script:activeOut } else { $script:activeErr }
        if (-not $path -or -not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
        $content = Read-SharedText -Path $path
        if ($null -eq $content) { continue }
        $offset = if ($key -eq 'out') { $script:activeOutOffset } else { $script:activeErrOffset }
        if ($content.Length -le $offset) { continue }
        $fresh = $content.Substring($offset)
        $lastBreak = $fresh.LastIndexOf("`n")
        if ($lastBreak -lt 0) { continue }
        $complete = $fresh.Substring(0, $lastBreak + 1)
        if ($key -eq 'out') { $script:activeOutOffset = $offset + $complete.Length }
        else { $script:activeErrOffset = $offset + $complete.Length }
        $script:activeOutputAll += $complete
        foreach ($line in ($complete -split '\r?\n')) {
            if (-not $line.Trim()) { continue }
            Update-ActionPhase -Line $line
            if ($line -match $script:M2_NOISY_BUILD) { Write-LocalLog $line -FileOnly }
            else { Write-LocalLog $line }
        }
    }
    Update-ActionStatusText
}

function Complete-LauncherAction {
    if (-not $script:activeProcess) { return }
    try { $script:activeProcess.Refresh() } catch {}
    if (-not $script:activeProcess.HasExited) { return }

    $exitCode = $script:activeProcess.ExitCode
    # Flush whatever the action wrote between the last tick and its exit.
    Update-ActionStream
    foreach ($key in @('out', 'err')) {
        $path = if ($key -eq 'out') { $script:activeOut } else { $script:activeErr }
        if (-not $path -or -not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
        $content = Read-SharedText -Path $path
        if ($null -eq $content) { continue }
        $offset = if ($key -eq 'out') { $script:activeOutOffset } else { $script:activeErrOffset }
        if ($content.Length -le $offset) { continue }
        $tail = $content.Substring($offset)
        if ($key -eq 'out') { $script:activeOutOffset = $content.Length } else { $script:activeErrOffset = $content.Length }
        $script:activeOutputAll += $tail
        foreach ($line in ($tail -split '\r?\n')) {
            if (-not $line.Trim()) { continue }
            if ($line -match $script:M2_NOISY_BUILD) { Write-LocalLog $line -FileOnly } else { Write-LocalLog $line }
        }
    }
    $output = $script:activeOutputAll
    foreach ($path in @($script:activeOut, $script:activeErr)) {
        try { Protect-M2LogFile -Path $path } catch { }
    }

    $action = $script:activeAction
    $launchClient = $script:launchClientAfterAction
    $openSupport = $script:openSupportAfterAction
    $contactUrl = $script:openContactAfterAction
    $restartAfterUpdate = $script:restartAfterUpdate
    $script:activeProcess.Dispose()
    $script:activeProcess = $null
    $script:launchClientAfterAction = $false
    $script:openSupportAfterAction = $false
    $script:openContactAfterAction = ''
    $script:restartAfterUpdate = $false
    $script:progress.Style = 'Blocks'
    $script:progress.Value = 0
    $script:actionStatus.Text = if ($exitCode -eq 0) { (UI-Text "Gotowe: $action" "Done: $action") } else { (UI-Text "Błąd: $action (kod $exitCode)" "Error: $action (code $exitCode)") }
    $script:actionStatus.ForeColor = if ($exitCode -eq 0) { [Drawing.Color]::LightGreen } else { [Drawing.Color]::Tomato }
    Write-LocalLog (UI-Text "Zakończono akcję $action, kod $exitCode." "Action $action finished, code $exitCode.")
    Refresh-Status

    if ($exitCode -ne 0) {
        $guidance = Get-M2LauncherErrorGuidance -Text $output -ServerRoot $ServerRoot
        $message = $guidance.Message + [Environment]::NewLine + [Environment]::NewLine + (UI-Text 'Jak naprawić:' 'How to fix it:') + [Environment]::NewLine + $guidance.Remedy
        [Windows.Forms.MessageBox]::Show(
            $message,
            $guidance.Title,
            'OK',
            'Warning') | Out-Null
    }
    if ($exitCode -eq 0 -and $action -like 'Update*' -and (Get-LauncherFingerprint) -ne $script:launcherFingerprint) {
        # The startup question said the launcher restarts by itself, and a
        # second question about it was what KamCio asked to be spared.
        if ($restartAfterUpdate) {
            Write-LocalLog (UI-Text 'Launcher zaktualizowany - uruchamiam go ponownie.' 'Launcher updated - restarting it.')
            Restart-Launcher
            return
        }
        $answer = [Windows.Forms.MessageBox]::Show(
            (UI-Text "Launcher zostal zaktualizowany.`r`n`r`nTo okno dziala jeszcze na starej wersji - nowe przyciski i poprawki pojawia sie dopiero po ponownym uruchomieniu.`r`n`r`nUruchomic launcher ponownie teraz?" "The launcher was updated.`r`n`r`nThis window still runs the old version - the new buttons and fixes appear only after a restart.`r`n`r`nRestart the launcher now?"),
            (UI-Text 'Aktualizacja zainstalowana' 'Update installed'), 'YesNo', 'Information')
        if ($answer -eq [Windows.Forms.DialogResult]::Yes) {
            Restart-Launcher
            return
        }
        $script:launcherFingerprint = Get-LauncherFingerprint
    }
    if ($exitCode -eq 0 -and $launchClient) { Start-ConfiguredClient }
    if ($exitCode -eq 0 -and $openSupport -and (Test-Path $supportDirectory)) {
        Start-Process explorer.exe -ArgumentList ('"{0}"' -f $supportDirectory)
        if ($contactUrl) { Start-Process $contactUrl }
    }
}

function Confirm-DockerReady {
    # Without this the docker CLI just returns nothing and the caller reports
    # "no databases found", which reads as data loss rather than a stopped engine.
    if (Test-M2DockerRunning) { return $true }
    [Windows.Forms.MessageBox]::Show(
        (UI-Text "Silnik Dockera jest zatrzymany, wiec nie widac zadnych baz.`r`n`r`nKliknij przycisk URUCHOM DOCKER, poczekaj az status u gory zmieni sie na - Docker: GOTOWY - i sprobuj ponownie.`r`n`r`nZadne dane nie zginely: bazy sa na dysku, tylko Docker ich teraz nie pokazuje." "The Docker engine is stopped, so no database can be seen.`r`n`r`nClick the START DOCKER button, wait until the status at the top changes to - Docker: READY - and try again.`r`n`r`nNo data is lost: the databases are on the disk, Docker just does not show them now."),
        (UI-Text 'Docker jest zatrzymany' 'Docker is stopped'), 'OK', 'Warning') | Out-Null
    return $false
}

function Get-SupportSettings {
    if ($null -eq $script:supportSettingsCache) {
        try { $script:supportSettingsCache = Get-M2SupportSettings -Config (Get-LauncherConfig) }
        catch {
            Write-LocalLog (UI-Text "Nie udalo sie odczytac adresu zgloszen: $($_.Exception.Message)" "Could not read the support address: $($_.Exception.Message)") -FileOnly
            $script:supportSettingsCache = [pscustomobject]@{ UploadUrl = ''; ContactUrl = 'https://github.com/Doofenyoyo1/metin2_SP/issues'; Source = 'none' }
        }
    }
    return $script:supportSettingsCache
}

function Get-BotCountFromEnv {
    $envPath = Join-Path $root 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $match = [Regex]::Match([IO.File]::ReadAllText($envPath), '(?m)^PLAYERBOT_AUTOSPAWN_COUNT=(\d+)\s*$')
        if ($match.Success) { return [int]$match.Groups[1].Value }
    }
    return 350
}

function Get-SpawnPlanFromEnv {
    # The spawn plan as .env has it; 1 / 0 / 24 when the keys are not there yet.
    $envPath = Join-Path $root 'linux-port\docker\.env'
    $plan = @{ Minutes = 1; Late = 0; Hours = 24 }
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $content = [IO.File]::ReadAllText($envPath)
        $m = [Regex]::Match($content, '(?m)^PLAYERBOT_SPAWN_WINDOW_MINUTES=(\d+)\s*$')
        if ($m.Success) { $plan.Minutes = [int]$m.Groups[1].Value }
        $m = [Regex]::Match($content, '(?m)^PLAYERBOT_LATE_JOINERS=(\d+)\s*$')
        if ($m.Success) { $plan.Late = [int]$m.Groups[1].Value }
        $m = [Regex]::Match($content, '(?m)^PLAYERBOT_LATE_JOIN_HOURS=(\d+)\s*$')
        if ($m.Success) { $plan.Hours = [int]$m.Groups[1].Value }
    }
    return $plan
}

function Get-KingdomPlanFromEnv {
    # PLAYERBOT_AUTOSPAWN_PER_KINGDOM with the three numbers, and the second
    # channel with its share, as .env has them; off and 40% otherwise.
    #
    # The three boxes open on the operator's own numbers only while those are in
    # use, and on an even share of the bots that play otherwise - never on the
    # zeros .env.example ships: ticked and half filled, they left two kingdoms
    # with nobody ("nowe postacie tworza sie tylko w Chunjo", NerrVoVy, 19
    # September). The 2.0.83 fix reached the text launcher alone, and only for a
    # .env without the keys, which start-server.ps1 adds at 0 to every one.
    # The game channels as one choice ("Kanaly gry 1-4"): 1, 2 with the second
    # channel, or 2 and the fresh cohort's CH3 (and CH4), with its count.
    $envPath = Join-Path $root 'linux-port\docker\.env'
    $plan = @{ PerKingdom = $false; Shinsoo = 0; Chunjo = 0; Jinno = 0; Channel2 = $false; Channel2Share = 40
        GameChannels = 1; FreshCount = 200 }
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $content = [IO.File]::ReadAllText($envPath)
        $m = [Regex]::Match($content, '(?m)^PLAYERBOT_AUTOSPAWN_PER_KINGDOM=(\d+)\s*$')
        if ($m.Success) { $plan.PerKingdom = $m.Groups[1].Value -eq '1' }
        $total = 0
        $m = [Regex]::Match($content, '(?m)^PLAYERBOT_AUTOSPAWN_COUNT=(\d+)\s*$')
        if ($m.Success) { $total = [int]$m.Groups[1].Value }
        $even = [int][Math]::Floor($total / 3)
        foreach ($pair in @(@('Shinsoo', 'PLAYERBOT_AUTOSPAWN_SHINSOO'), @('Chunjo', 'PLAYERBOT_AUTOSPAWN_CHUNJO'), @('Jinno', 'PLAYERBOT_AUTOSPAWN_JINNO'))) {
            $plan[$pair[0]] = $even
            if (-not $plan.PerKingdom) { continue }
            $m = [Regex]::Match($content, '(?m)^' + $pair[1] + '=(\d+)\s*$')
            if ($m.Success) { $plan[$pair[0]] = [int]$m.Groups[1].Value }
        }
        $m = [Regex]::Match($content, '(?m)^M2_PLAYERBOT_CH2=(\d+)\s*$')
        if ($m.Success) { $plan.Channel2 = $m.Groups[1].Value -eq '1' }
        $m = [Regex]::Match($content, '(?m)^PLAYERBOT_CH2_SHARE=(\d+)\s*$')
        if ($m.Success) { $plan.Channel2Share = [int]$m.Groups[1].Value }
        $fresh = 0
        $m = [Regex]::Match($content, '(?m)^M2_PLAYERBOT_FRESH_CHANNELS=(\d+)\s*$')
        if ($m.Success -and [int]$m.Groups[1].Value -le 2) { $fresh = [int]$m.Groups[1].Value }
        $m = [Regex]::Match($content, '(?m)^PLAYERBOT_FRESH_COUNT=(\d+)\s*$')
        if ($m.Success) { $plan.FreshCount = [int]$m.Groups[1].Value }
        $plan.GameChannels = if ($fresh -gt 0) { 2 + $fresh } elseif ($plan.Channel2) { 2 } else { 1 }
    }
    return $plan
}

function Get-DifficultyFromEnv {
    # M2_DIFFICULTY and the hour counts, as .env has them; easy and zeros when
    # the keys are not there yet (an older .env, which start-server.ps1 fills in).
    $envPath = Join-Path $root 'linux-port\docker\.env'
    $level = 'easy'; $bio = '0'; $horse = '0'; $book = '0'; $botBook = '0'
    $autoHunt = $true; $sidekick = $true; $starter = $true; $autoHuntItem = $false; $flea = $true
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $content = [IO.File]::ReadAllText($envPath)
        $m = [Regex]::Match($content, '(?m)^M2_DIFFICULTY=(\S+)\s*$')
        if ($m.Success) { $level = $m.Groups[1].Value.Trim().ToLowerInvariant() }
        $m = [Regex]::Match($content, '(?m)^M2_BIOLOGIST_WAIT_HOURS=(\S+)\s*$')
        if ($m.Success) { $bio = $m.Groups[1].Value.Trim() }
        $m = [Regex]::Match($content, '(?m)^M2_HORSE_WAIT_HOURS=(\S+)\s*$')
        if ($m.Success) { $horse = $m.Groups[1].Value.Trim() }
        $m = [Regex]::Match($content, '(?m)^M2_BOOK_WAIT_HOURS=(\S+)\s*$')
        if ($m.Success) { $book = $m.Groups[1].Value.Trim() }
        $m = [Regex]::Match($content, '(?m)^M2_BOT_BOOK_WAIT_HOURS=(\S+)\s*$')
        if ($m.Success) { $botBook = $m.Groups[1].Value.Trim() }
        # Auto Lowy and the companion: on unless .env says 0.
        $m = [Regex]::Match($content, '(?m)^M2_AUTOHUNT=(\S+)\s*$')
        if ($m.Success) { $autoHunt = ($m.Groups[1].Value.Trim() -ne '0') }
        # Only with the ItemShop's ticket: off unless .env says 1.
        $m = [Regex]::Match($content, '(?m)^M2_AUTOHUNT_ITEM=(\S+)\s*$')
        if ($m.Success) { $autoHuntItem = ($m.Groups[1].Value.Trim() -eq '1') }
        $m = [Regex]::Match($content, '(?m)^M2_SIDEKICK=(\S+)\s*$')
        if ($m.Success) { $sidekick = ($m.Groups[1].Value.Trim() -ne '0') }
        $m = [Regex]::Match($content, '(?m)^M2_STARTER_CHEST=(\S+)\s*$')
        if ($m.Success) { $starter = ($m.Groups[1].Value.Trim() -ne '0') }
        $m = [Regex]::Match($content, '(?m)^M2_FLEA_MARKET=(\S+)\s*$')
        if ($m.Success) { $flea = ($m.Groups[1].Value.Trim() -ne '0') }
    }
    if ($level -notin @('easy', 'medium', 'hard', 'custom')) { $level = 'easy' }
    return @{ Level = $level; Biologist = $bio; Horse = $horse; Book = $book; BotBook = $botBook
        AutoHunt = $autoHunt; AutoHuntItem = $autoHuntItem; Sidekick = $sidekick; Starter = $starter; Flea = $flea }
}

function Show-DifficultyDialog {
    # Four presets as radio buttons and the hour counts custom reads; the
    # numbers are what the migrate service turns into the event flags at the
    # next start (quest/m2_difficulty.lua, and the engine and the bots for the
    # skill books), so the dialog says a restart is needed. Below them, whether
    # the world is played with Auto Lowy and with the companion (25
    # September: Drip's COOP without the auto hunt), and whether a player's new
    # character gets the apprentice chest - asked before only where a fresh
    # world is made, so a world already standing had no way to it (Drip looked
    # here for it the same morning). Under Auto Lowy, whether its panel is for
    # everybody or only for a character that bought "Auto Lowy (8h)" in the
    # ItemShop (the operator, 27 September). Last, the Dom Towarowy (Uxie
    # [DSO]'s flea market, the operator's switch of 27 September). Returns
    # @{ Level; Biologist; Horse; Book; BotBook; AutoHunt; AutoHuntItem; Sidekick; Starter; Flea } or $null.
    param([hashtable]$Current)
    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = (T 'difficultyDialog')
    $dialog.Size = [Drawing.Size]::new(560, 644)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false

    $info = [Windows.Forms.Label]::new()
    $info.Text = (UI-Text "Ile czeka się u Biologa, u Stajennego (kucyk, Księgi Konia, treningi medalami) i na kolejną księgę umiejętności? Biolog i Stajenny dotyczą graczy; księgi mają osobny czas dla graczy i dla botów.`r`nZmiana wymaga restartu serwera (panel WWW zmienia to samo od razu)." "How long is the wait at the Biologist, the Stable Keeper (pony, Horse Books, medal trainings) and for the next skill book? The Biologist and the Stable Keeper concern players; the books have separate waits for players and for bots.`r`nA change needs a server restart (the web panel changes the same at once).")
    $info.Location = [Drawing.Point]::new(14, 12)
    $info.Size = [Drawing.Size]::new(520, 58)
    $dialog.Controls.Add($info)

    $labels = @{
        easy   = (UI-Text 'Łatwy - bez czekania u Biologa, Stajennego i na księgi (tak jak dotąd)' 'Easy - no waiting at the Biologist, the Stable Keeper or for books (as until now)')
        medium = (UI-Text 'Średni - Biolog 8 h; kucyk i Księgi Konia 4 h; treningi 6/7 h; księgi 7 h' 'Medium - Biologist 8 h; pony and Horse Books 4 h; trainings 6/7 h; books 7 h')
        hard   = (UI-Text 'Trudny - jak w oryginale: Biolog 24 h; kucyk i Księgi 12 h; treningi 18/21 h; księgi 21 h' 'Hard - as in the original: Biologist 24 h; pony and Books 12 h; trainings 18/21 h; books 21 h')
        custom = (UI-Text 'Własny - godziny poniżej' 'Custom - the hours below')
    }
    $radios = @{}
    $y = 76
    foreach ($level in @('easy', 'medium', 'hard', 'custom')) {
        $radio = [Windows.Forms.RadioButton]::new()
        $radio.Name = "level_$level"
        $radio.Text = $labels[$level]
        $radio.Location = [Drawing.Point]::new(18, $y)
        $radio.Size = [Drawing.Size]::new(516, 26)
        $radio.Checked = ($Current.Level -eq $level)
        $dialog.Controls.Add($radio)
        $radios[$level] = $radio
        $y += 30
    }

    $bioLabel = [Windows.Forms.Label]::new()
    $bioLabel.Text = (UI-Text 'Biolog: godzin między oddaniami' 'Biologist: hours between hand-ins')
    $bioLabel.Location = [Drawing.Point]::new(40, $y + 8)
    $bioLabel.Size = [Drawing.Size]::new(260, 22)
    $dialog.Controls.Add($bioLabel)
    $bioBox = [Windows.Forms.NumericUpDown]::new()
    $bioBox.Name = 'bioHours'
    $bioBox.DecimalPlaces = 1
    $bioBox.Increment = 0.5
    $bioBox.Minimum = 0
    $bioBox.Maximum = 720
    $bioBox.Location = [Drawing.Point]::new(310, $y + 5)
    $bioBox.Size = [Drawing.Size]::new(90, 24)
    $dialog.Controls.Add($bioBox)

    $horseLabel = [Windows.Forms.Label]::new()
    $horseLabel.Text = (UI-Text 'Stajenny: godzin na kucyka, Księgę i trening' 'Stable Keeper: hours for pony, Book, training')
    $horseLabel.Location = [Drawing.Point]::new(40, $y + 38)
    $horseLabel.Size = [Drawing.Size]::new(260, 22)
    $dialog.Controls.Add($horseLabel)
    $horseBox = [Windows.Forms.NumericUpDown]::new()
    $horseBox.Name = 'horseHours'
    $horseBox.DecimalPlaces = 1
    $horseBox.Increment = 0.5
    $horseBox.Minimum = 0
    $horseBox.Maximum = 720
    $horseBox.Location = [Drawing.Point]::new(310, $y + 35)
    $horseBox.Size = [Drawing.Size]::new(90, 24)
    $dialog.Controls.Add($horseBox)

    $bookLabel = [Windows.Forms.Label]::new()
    $bookLabel.Text = (UI-Text 'Księgi umiejętności - gracze: godzin' 'Skill books - players: hours')
    $bookLabel.Location = [Drawing.Point]::new(40, $y + 68)
    $bookLabel.Size = [Drawing.Size]::new(260, 22)
    $dialog.Controls.Add($bookLabel)
    $bookBox = [Windows.Forms.NumericUpDown]::new()
    $bookBox.Name = 'bookHours'
    $bookBox.DecimalPlaces = 1
    $bookBox.Increment = 0.5
    $bookBox.Minimum = 0
    $bookBox.Maximum = 720
    $bookBox.Location = [Drawing.Point]::new(310, $y + 65)
    $bookBox.Size = [Drawing.Size]::new(90, 24)
    $dialog.Controls.Add($bookBox)

    $botBookLabel = [Windows.Forms.Label]::new()
    $botBookLabel.Text = (UI-Text 'Księgi umiejętności - boty: godzin' 'Skill books - bots: hours')
    $botBookLabel.Location = [Drawing.Point]::new(40, $y + 98)
    $botBookLabel.Size = [Drawing.Size]::new(260, 22)
    $dialog.Controls.Add($botBookLabel)
    $botBookBox = [Windows.Forms.NumericUpDown]::new()
    $botBookBox.Name = 'botBookHours'
    $botBookBox.DecimalPlaces = 1
    $botBookBox.Increment = 0.5
    $botBookBox.Minimum = 0
    $botBookBox.Maximum = 720
    $botBookBox.Location = [Drawing.Point]::new(310, $y + 95)
    $botBookBox.Size = [Drawing.Size]::new(90, 24)
    $dialog.Controls.Add($botBookBox)

    $toDecimal = {
        param([string]$Text)
        $n = 0.0
        if ([double]::TryParse("$Text".Trim().Replace(',', '.'), [Globalization.NumberStyles]::Float,
                [Globalization.CultureInfo]::InvariantCulture, [ref]$n)) {
            # [double] on both sides: with an int beside it PowerShell picks
            # Math.Min(int, int) and rounds 0.5 to 0, so a custom half hour
            # showed as 0 and Apply wrote the 0 back.
            return [decimal][Math]::Max([double]0, [Math]::Min([double]720, $n))
        }
        return [decimal]0
    }
    $bioBox.Value = & $toDecimal $Current.Biologist
    $horseBox.Value = & $toDecimal $Current.Horse
    $bookBox.Value = & $toDecimal $Current.Book
    $botBookBox.Value = & $toDecimal $Current.BotBook

    # The hour boxes belong to "custom"; the presets say their numbers themselves.
    $sync = {
        $form = $this.FindForm()
        if (-not $form) { return }
        $custom = $form.Controls['level_custom'].Checked
        $form.Controls['bioHours'].Enabled = $custom
        $form.Controls['horseHours'].Enabled = $custom
        $form.Controls['bookHours'].Enabled = $custom
        $form.Controls['botBookHours'].Enabled = $custom
    }
    foreach ($radio in $radios.Values) { $radio.Add_CheckedChanged($sync) }
    $bioBox.Enabled = $radios['custom'].Checked
    $horseBox.Enabled = $radios['custom'].Checked
    $bookBox.Enabled = $radios['custom'].Checked
    $botBookBox.Enabled = $radios['custom'].Checked

    # Whatever the level: the two features a world may be played without.
    $autoHuntCheck = [Windows.Forms.CheckBox]::new()
    $autoHuntCheck.Name = 'autoHunt'
    $autoHuntCheck.Text = (UI-Text 'Auto Łowy - automatyczne polowanie w kliencie (klawisz K)' 'Auto Hunt - automatic hunting in the client (key K)')
    $autoHuntCheck.Location = [Drawing.Point]::new(18, $y + 136)
    $autoHuntCheck.Size = [Drawing.Size]::new(516, 24)
    $autoHuntCheck.Checked = ($Current.AutoHunt -ne $false)
    $dialog.Controls.Add($autoHuntCheck)
    # Its own panel, so the two are a group apart from the level's radios.
    $autoHuntItemPanel = [Windows.Forms.Panel]::new()
    $autoHuntItemPanel.Name = 'autoHuntItem'
    $autoHuntItemPanel.Location = [Drawing.Point]::new(36, $y + 160)
    $autoHuntItemPanel.Size = [Drawing.Size]::new(498, 50)
    $autoHuntAllRadio = [Windows.Forms.RadioButton]::new()
    $autoHuntAllRadio.Text = (UI-Text 'Panel Autołowy dostępny dla każdego' 'Auto Hunt panel available to everybody')
    $autoHuntAllRadio.Location = [Drawing.Point]::new(0, 2)
    $autoHuntAllRadio.Size = [Drawing.Size]::new(496, 22)
    $autoHuntItemRadio = [Windows.Forms.RadioButton]::new()
    $autoHuntItemRadio.Text = (UI-Text 'Panel Autołowy dostępny tylko po kupnie przedmiotu z ItemShop (8 h gry)' 'Auto Hunt panel only after buying the ItemShop item (8 h of play)')
    $autoHuntItemRadio.Location = [Drawing.Point]::new(0, 26)
    $autoHuntItemRadio.Size = [Drawing.Size]::new(496, 22)
    $autoHuntItemRadio.Checked = ($Current.AutoHuntItem -eq $true)
    $autoHuntAllRadio.Checked = -not $autoHuntItemRadio.Checked
    $autoHuntItemPanel.Controls.Add($autoHuntAllRadio)
    $autoHuntItemPanel.Controls.Add($autoHuntItemRadio)
    $autoHuntItemPanel.Enabled = $autoHuntCheck.Checked
    $dialog.Controls.Add($autoHuntItemPanel)
    $autoHuntCheck.Add_CheckedChanged({
        $form = $this.FindForm()
        if ($form) { $form.Controls['autoHuntItem'].Enabled = $this.Checked }
    })
    $sidekickCheck = [Windows.Forms.CheckBox]::new()
    $sidekickCheck.Name = 'sidekick'
    $sidekickCheck.Text = (UI-Text 'Towarzysz - stały kompan gracza (list "Towarzysz" i okno P)' 'Companion - the player''s permanent companion (the "Companion" letter and window P)')
    $sidekickCheck.Location = [Drawing.Point]::new(18, $y + 214)
    $sidekickCheck.Size = [Drawing.Size]::new(516, 24)
    $sidekickCheck.Checked = ($Current.Sidekick -ne $false)
    $dialog.Controls.Add($sidekickCheck)
    $starterCheck = [Windows.Forms.CheckBox]::new()
    $starterCheck.Name = 'starterChest'
    # One switch for people and bots: off, a player's new character gets no
    # chest, a bot is made without one and loses the ones in its bag.
    $starterCheck.Text = (UI-Text 'Skrzynia Ucznia w grze - nowe postacie graczy i boty' 'Apprentice Chest in the game - players'' new characters and bots')
    $starterCheck.Location = [Drawing.Point]::new(18, $y + 240)
    $starterCheck.Size = [Drawing.Size]::new(516, 24)
    $starterCheck.Checked = ($Current.Starter -ne $false)
    $dialog.Controls.Add($starterCheck)
    $fleaCheck = [Windows.Forms.CheckBox]::new()
    $fleaCheck.Name = 'fleaMarket'
    $fleaCheck.Text = (UI-Text 'Dom Towarowy - oferty wszystkich sklepów u Handlarki Różności w M1' 'Flea Market - every shop''s offers at the General Store merchant in M1')
    $fleaCheck.Location = [Drawing.Point]::new(18, $y + 266)
    $fleaCheck.Size = [Drawing.Size]::new(516, 24)
    $fleaCheck.Checked = ($Current.Flea -ne $false)
    $dialog.Controls.Add($fleaCheck)

    $okButton = [Windows.Forms.Button]::new()
    $okButton.Text = (T 'apply')
    $okButton.Location = [Drawing.Point]::new(332, $y + 308)
    $okButton.Size = [Drawing.Size]::new(100, 32)
    $okButton.DialogResult = [Windows.Forms.DialogResult]::OK
    $dialog.Controls.Add($okButton)

    $cancelButton = [Windows.Forms.Button]::new()
    $cancelButton.Text = (T 'cancel')
    $cancelButton.Location = [Drawing.Point]::new(438, $y + 308)
    $cancelButton.Size = [Drawing.Size]::new(96, 32)
    $cancelButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($cancelButton)
    $dialog.AcceptButton = $okButton
    $dialog.CancelButton = $cancelButton

    $result = $dialog.ShowDialog()
    $chosen = 'easy'
    foreach ($level in $radios.Keys) { if ($radios[$level].Checked) { $chosen = $level } }
    $bio = $bioBox.Value.ToString([Globalization.CultureInfo]::InvariantCulture)
    $horse = $horseBox.Value.ToString([Globalization.CultureInfo]::InvariantCulture)
    $book = $bookBox.Value.ToString([Globalization.CultureInfo]::InvariantCulture)
    $botBook = $botBookBox.Value.ToString([Globalization.CultureInfo]::InvariantCulture)
    $autoHunt = $autoHuntCheck.Checked
    $autoHuntItem = $autoHuntItemRadio.Checked
    $sidekick = $sidekickCheck.Checked
    $starter = $starterCheck.Checked
    $flea = $fleaCheck.Checked
    $dialog.Dispose()
    if ($result -ne [Windows.Forms.DialogResult]::OK) { return $null }
    return @{ Level = $chosen; Biologist = $bio; Horse = $horse; Book = $book; BotBook = $botBook
        AutoHunt = $autoHunt; AutoHuntItem = $autoHuntItem; Sidekick = $sidekick; Starter = $starter; Flea = $flea }
}

function Show-FreshWorldDialog {
    # The rates a world about to be made starts on, and whether its bots wait
    # at the door. Both reach the migrator through .env and are read before the
    # cores come up, so this is the only moment they can be chosen without
    # something already happening in the world - which is what the window is
    # for (NerrVoVy, 20 September). And whether a player's new character gets
    # the apprentice chest (seban latino, 22 September). Returns
    # @{ Exp; Drop; Yang; Hold; Starter } or $null.
    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = (UI-Text 'Nowy świat - ustawienia na start' 'New world - starting settings')
    $dialog.Size = [Drawing.Size]::new(560, 426)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false

    $info = [Windows.Forms.Label]::new()
    $info.Text = ((UI-Text "Jak szybko ma iść nowy świat? 100% to tyle, ile w oryginalnej grze.`r`n" "How fast should the new world go? 100% is as much as in the original game.`r`n") +
                  (UI-Text "Te liczby wchodzą w życie, zanim pojawi się pierwszy bot - później zmienia się je w panelu." "These numbers take effect before the first bot appears - later they are changed in the panel."))
    $info.Location = [Drawing.Point]::new(14, 12)
    $info.Size = [Drawing.Size]::new(520, 44)
    $dialog.Controls.Add($info)

    $presets = @(
        @{ Key = 'normal'; Text = (UI-Text 'Normalnie - 100% doświadczenia, 100% dropu, 100% yang' 'Normal - 100% experience, 100% drop, 100% yang'); Exp = 100; Drop = 100; Yang = 100 },
        @{ Key = 'relaxed'; Text = (UI-Text 'Spokojnie - 300% / 200% / 200%' 'Relaxed - 300% / 200% / 200%'); Exp = 300; Drop = 200; Yang = 200 },
        @{ Key = 'fast'; Text = (UI-Text 'Szybko - 1000% / 500% / 500%' 'Fast - 1000% / 500% / 500%'); Exp = 1000; Drop = 500; Yang = 500 },
        @{ Key = 'custom'; Text = (UI-Text 'Własne liczby - poniżej' 'Your own numbers - below'); Exp = 0; Drop = 0; Yang = 0 }
    )
    $radios = @{}
    $y = 62
    foreach ($preset in $presets) {
        $radio = [Windows.Forms.RadioButton]::new()
        $radio.Name = ('rate_' + $preset.Key)
        $radio.Text = $preset.Text
        $radio.Location = [Drawing.Point]::new(18, $y)
        $radio.Size = [Drawing.Size]::new(516, 26)
        $radio.Checked = ($preset.Key -eq 'normal')
        $dialog.Controls.Add($radio)
        $radios[$preset.Key] = $radio
        $y += 30
    }

    $boxes = @{}
    $x = 40
    foreach ($field in @(
            @{ Name = 'exp'; Label = (UI-Text 'Doświadczenie %' 'Experience %') },
            @{ Name = 'drop'; Label = 'Drop %' },
            @{ Name = 'yang'; Label = 'Yang %' })) {
        $label = [Windows.Forms.Label]::new()
        $label.Text = $field.Label
        $label.Location = [Drawing.Point]::new($x, $y + 10)
        $label.Size = [Drawing.Size]::new(120, 22)
        $dialog.Controls.Add($label)
        $box = [Windows.Forms.NumericUpDown]::new()
        $box.Name = ('num_' + $field.Name)
        $box.Minimum = 1
        $box.Maximum = 10000
        $box.Increment = 50
        $box.Value = 100
        $box.Location = [Drawing.Point]::new($x, $y + 34)
        $box.Size = [Drawing.Size]::new(110, 24)
        $dialog.Controls.Add($box)
        $boxes[$field.Name] = $box
        $x += 160
    }
    $y += 70

    $holdBox = [Windows.Forms.CheckBox]::new()
    $holdBox.Text = (UI-Text 'Wstrzymaj boty po starcie (wpuszczę je sam, przyciskiem w panelu)' 'Hold the bots after the start (I let them in myself, with a panel button)')
    $holdBox.Location = [Drawing.Point]::new(18, $y + 6)
    $holdBox.Size = [Drawing.Size]::new(516, 26)
    $holdBox.Checked = $false
    $dialog.Controls.Add($holdBox)
    $y += 34

    # What the world had, not a fixed "on": a wipe ticked the chest back on
    # for everybody who had switched it off and clicked through the dialog -
    # one way "the apprentice chests come back" (the operator, 28 September).
    $starterBox = [Windows.Forms.CheckBox]::new()
    $starterBox.Text = (UI-Text 'Skrzynia Ucznia w grze - nowe postacie graczy i boty' 'Apprentice Chest in the game - players'' new characters and bots')
    $starterBox.Location = [Drawing.Point]::new(18, $y + 6)
    $starterBox.Size = [Drawing.Size]::new(516, 26)
    $starterBox.Checked = ((Get-DifficultyFromEnv).Starter -ne $false)
    $dialog.Controls.Add($starterBox)
    $y += 34

    # The boxes belong to "własne"; a preset says its own numbers.
    $sync = {
        $form = $this.FindForm()
        if (-not $form) { return }
        $custom = $form.Controls['rate_custom'].Checked
        foreach ($name in @('num_exp', 'num_drop', 'num_yang')) { $form.Controls[$name].Enabled = $custom }
    }
    foreach ($radio in $radios.Values) { $radio.Add_CheckedChanged($sync) }
    foreach ($box in $boxes.Values) { $box.Enabled = $false }

    $okButton = [Windows.Forms.Button]::new()
    $okButton.Text = (UI-Text 'Dalej' 'Next')
    $okButton.Location = [Drawing.Point]::new(332, $y + 16)
    $okButton.Size = [Drawing.Size]::new(100, 32)
    $okButton.DialogResult = [Windows.Forms.DialogResult]::OK
    $dialog.Controls.Add($okButton)

    $cancelButton = [Windows.Forms.Button]::new()
    $cancelButton.Text = (T 'cancel')
    $cancelButton.Location = [Drawing.Point]::new(438, $y + 16)
    $cancelButton.Size = [Drawing.Size]::new(96, 32)
    $cancelButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($cancelButton)
    $dialog.AcceptButton = $okButton
    $dialog.CancelButton = $cancelButton

    $result = $dialog.ShowDialog()
    $chosen = 'normal'
    foreach ($key in $radios.Keys) { if ($radios[$key].Checked) { $chosen = $key } }
    $values = @{ Exp = 100; Drop = 100; Yang = 100 }
    if ($chosen -eq 'custom') {
        $values = @{ Exp = [int]$boxes['exp'].Value; Drop = [int]$boxes['drop'].Value; Yang = [int]$boxes['yang'].Value }
    }
    else {
        foreach ($preset in $presets) {
            if ($preset.Key -eq $chosen) { $values = @{ Exp = $preset.Exp; Drop = $preset.Drop; Yang = $preset.Yang } }
        }
    }
    $hold = $(if ($holdBox.Checked) { 1 } else { 0 })
    $starter = $(if ($starterBox.Checked) { 1 } else { 0 })
    $dialog.Dispose()
    if ($result -ne [Windows.Forms.DialogResult]::OK) { return $null }
    return @{ Exp = $values.Exp; Drop = $values.Drop; Yang = $values.Yang; Hold = $hold; Starter = $starter }
}

function Get-LauncherFingerprint {
    # An update replaces the launcher's own files, but this process already read
    # them - the new buttons cannot appear until it restarts.
    $stamps = New-Object System.Collections.Generic.List[string]
    foreach ($file in @($PSCommandPath, $cliLauncher, $modulePath, $diagnosticsModulePath)) {
        if (Test-Path -LiteralPath $file -PathType Leaf) {
            $stamps.Add(((Get-Item -LiteralPath $file).LastWriteTimeUtc.Ticks).ToString())
        }
    }
    return ($stamps -join '|')
}

function Restart-Launcher {
    # The new launcher is a console program (the .bat's cmd.exe, or
    # powershell.exe), and a console program whose parent exits in the same
    # breath can fail to initialise: on Windows 11, where the new terminal
    # takes over every console window, that is cmd.exe's "Aplikacja nie zostala
    # wlasciwie uruchomiona (0xc0000142)" after an update (Urtopy, 19 September,
    # the launcher started from its desktop shortcut). So this window stays up
    # until the new one has run for a few seconds, and a start that died on the
    # way is tried once more straight through powershell.exe.
    $batch = Join-Path $root 'Metin2-Launcher-GUI.bat'
    $attempts = @()
    if (Test-Path -LiteralPath $batch -PathType Leaf) {
        $attempts += ,@($batch)
    }
    # -STA matters: WinForms will not start without it.
    $attempts += ,@('powershell.exe', '-NoProfile', '-STA', '-ExecutionPolicy', 'Bypass', '-File', ('"{0}"' -f $PSCommandPath))
    $lastError = ''
    foreach ($attempt in $attempts) {
        try {
            if ($attempt.Count -gt 1) {
                $started = Start-Process -FilePath $attempt[0] -WorkingDirectory $root -PassThru `
                    -ArgumentList ($attempt[1..($attempt.Count - 1)])
            }
            else {
                $started = Start-Process -FilePath $attempt[0] -WorkingDirectory $root -PassThru
            }
        }
        catch {
            $lastError = $_.Exception.Message
            Write-LocalLog ((UI-Text "Restart launchera przez {0} nieudany: {1}" "Restarting the launcher through {0} failed: {1}") -f $attempt[0], $lastError)
            continue
        }
        $deadline = [DateTime]::UtcNow.AddSeconds(4)
        while ($started -and -not $started.HasExited -and [DateTime]::UtcNow -lt $deadline) {
            [Windows.Forms.Application]::DoEvents()
            Start-Sleep -Milliseconds 200
        }
        if (-not $started -or -not $started.HasExited -or $started.ExitCode -eq 0) {
            Write-LocalLog (UI-Text 'Uruchamiam launcher ponownie po aktualizacji.' 'Restarting the launcher after the update.')
            $script:form.Close()
            return
        }
        $lastError = (UI-Text 'kod 0x{0:X8}' 'code 0x{0:X8}') -f $started.ExitCode
        Write-LocalLog ((UI-Text "Nowy launcher ({0}) zakonczyl sie od razu, {1}." "The new launcher ({0}) ended at once, {1}.") -f $attempt[0], $lastError)
    }
    [Windows.Forms.MessageBox]::Show(
        ((UI-Text "Nie udalo sie uruchomic launchera ponownie ({0}).`r`n`r`nZamknij to okno i uruchom launcher skrotem z pulpitu." "Could not restart the launcher ({0}).`r`n`r`nClose this window and start the launcher from its desktop shortcut.") -f $lastError),
        (UI-Text 'Restart launchera' 'Launcher restart'), 'OK', 'Warning') | Out-Null
}

function Get-InstalledServerVersion {
    # Same reasoning as Read-State in the text launcher: while a rebuild is
    # outstanding the files on disk are ahead of the containers, so the version
    # they claim must not be used to decide that nothing needs doing.
    if (Test-Path -LiteralPath (Join-Path $root '.m2launcher-rebuild-pending') -PathType Leaf) {
        return 'unknown'
    }
    $statePath = Join-Path $root '.m2launcher-state.json'
    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        try {
            $state = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
            if ([string]$state.server) { return ([string]$state.server).Trim() }
        }
        catch { }
    }
    $versionFile = Join-Path $root 'VERSION'
    if (Test-Path -LiteralPath $versionFile -PathType Leaf) {
        return (Get-Content -LiteralPath $versionFile -Raw).Trim()
    }
    return 'unknown'
}

function Get-InstalledClientVersion {
    # What a client update recorded, else what the full package shipped
    # (CLIENT_VERSION beside VERSION, put there by New-M2DeployTree.ps1).
    $statePath = Join-Path $root '.m2launcher-state.json'
    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        try {
            $state = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
            if ([string]$state.client -and [string]$state.client -ne 'unknown') { return ([string]$state.client).Trim() }
        }
        catch { }
    }
    $marker = Join-Path $root 'CLIENT_VERSION'
    if (Test-Path -LiteralPath $marker -PathType Leaf) {
        return (Get-Content -LiteralPath $marker -Raw).Trim()
    }
    return 'unknown'
}

# The versions the player said NO to at startup, so the same question is not
# asked at every start - a newer version asks again. Its own file: Save-State
# in the text launcher rewrites .m2launcher-state.json with three fields only.
$script:offersPath = Join-Path $root '.m2launcher-offers.json'
function Read-DeclinedOffers {
    $declined = @{ server = ''; client = '' }
    if (Test-Path -LiteralPath $script:offersPath -PathType Leaf) {
        try {
            $saved = Get-Content -LiteralPath $script:offersPath -Raw -Encoding UTF8 | ConvertFrom-Json
            if ([string]$saved.server) { $declined.server = [string]$saved.server }
            if ([string]$saved.client) { $declined.client = [string]$saved.client }
        }
        catch { }
    }
    return $declined
}
function Save-DeclinedOffer {
    param([string]$Component, [string]$Version)
    $declined = Read-DeclinedOffers
    $declined[$Component] = $Version
    try {
        [pscustomobject]$declined | ConvertTo-Json | Set-Content -LiteralPath $script:offersPath -Encoding UTF8
    }
    catch { }
}

function Test-VersionNewer {
    param([string]$Installed, [string]$Available)
    if (-not $Available) { return $false }
    if (-not $Installed -or $Installed -eq 'unknown') { return $true }
    return -not $Installed.Trim().Equals($Available.Trim(), [StringComparison]::OrdinalIgnoreCase)
}

$script:latestManifest = $null
$script:startupOfferDone = $false
# The replacement of an old metin2client.exe is a download and an action of
# its own; once a session, because an antivirus that refused it refuses it
# again, and the root says the rest at the login.
$script:clientExeRepairTried = $false
# An update the startup question started: its answer covers restarting the
# launcher too. An update from the AKTUALIZUJ button still asks about that.
$script:restartAfterUpdate = $false

function Get-ServerUpdateOffer {
    # The server version to offer at startup, or $null: newer than the one
    # installed and not put off with "Nie teraz".
    $installed = Get-InstalledServerVersion
    $available = $script:latestServerVersion
    if (-not $available -or -not (Test-VersionNewer -Installed $installed -Available $available)) { return $null }
    if ((Read-DeclinedOffers).server -eq $available) { return $null }
    return @{ Available = $available; Installed = $installed }
}

function Get-ClientUpdateOffer {
    # Only on the 2.x line: there the manifest's client component is the
    # ordinary client package. On r40250 it is the experimental GM panel,
    # which nobody should be nagged into at startup.
    if (-not $script:clientUpdateIsPlain -or -not $script:latestManifest) { return $null }
    $clientProperty = $script:latestManifest.PSObject.Properties['client']
    if (-not $clientProperty -or -not $clientProperty.Value -or -not [string]$clientProperty.Value.version) { return $null }
    $available = ([string]$clientProperty.Value.version).Trim()
    $installed = Get-InstalledClientVersion
    if (-not (Test-VersionNewer -Installed $installed -Available $available)) { return $null }
    if ((Read-DeclinedOffers).client -eq $available) { return $null }
    $config = Get-LauncherConfig
    if (-not [string]$config.clientRoot) {
        Write-LocalLog (UI-Text "Dostepna wersja klienta $available, ale folder klienta nie jest ustawiony - pomijam pytanie." "Client version $available is available, but no client folder is set - skipping the question.")
        return $null
    }
    return @{ Available = $available; Installed = $installed }
}

function Show-StartupUpdateDialog {
    # One question for everything the start found. It used to be two - the
    # server's, then the client's once the server was done, and a third about
    # restarting the launcher between them ("dwa monity o potwierdzeniu
    # aktualizacji launchera i clienta", KamCio, 25 September) - and the server
    # alone is an answer of its own for whoever does not play on this
    # computer's client (bruce_willis: a server on a VPS). Returns 'all',
    # 'server', 'later' (put off until the next version, as NO always did), or
    # '' when the window was closed: then it asks again at the next start.
    param($Server, $Client)

    if ($Server -and $Client) {
        $text = (T 'startupUpdateBoth') -f $Server.Available, $Server.Installed, $Client.Available, $Client.Installed
    }
    elseif ($Server) { $text = (T 'startupServerUpdate') -f $Server.Available, $Server.Installed }
    else { $text = (T 'startupClientUpdate') -f $Client.Available, $Client.Installed }

    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = (T 'startupUpdateTitle')
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false
    $dialog.ShowInTaskbar = $false

    $info = [Windows.Forms.Label]::new()
    $info.Text = $text
    $info.Location = [Drawing.Point]::new(16, 14)
    $infoHeight = $info.GetPreferredSize([Drawing.Size]::new(500, 0)).Height
    $info.Size = [Drawing.Size]::new(500, $infoHeight)
    $dialog.Controls.Add($info)

    $buttons = @()
    if ($Server -and $Client) {
        $buttons += ,@((T 'startupUpdateAll'), [Windows.Forms.DialogResult]::Yes, 180)
        $buttons += ,@((T 'startupUpdateServerOnly'), [Windows.Forms.DialogResult]::No, 150)
    }
    else {
        $buttons += ,@((T 'startupUpdateNow'), [Windows.Forms.DialogResult]::Yes, 180)
    }
    $buttons += ,@((T 'startupUpdateLater'), [Windows.Forms.DialogResult]::Ignore, 130)
    $y = 14 + $infoHeight + 16
    $x = 16 + 500
    $accept = $null
    for ($i = $buttons.Count - 1; $i -ge 0; $i--) {
        $spec = $buttons[$i]
        $button = [Windows.Forms.Button]::new()
        $button.Text = $spec[0]
        $button.DialogResult = $spec[1]
        $x -= $spec[2]
        $button.Location = [Drawing.Point]::new($x, $y)
        $button.Size = [Drawing.Size]::new($spec[2] - 8, 30)
        # Laid out from the right, tabbed from the left: Enter must mean the
        # update, not the "Nie teraz" that was added first.
        $button.TabIndex = $i
        $dialog.Controls.Add($button)
        if ($i -eq 0) { $accept = $button }
    }
    $dialog.AcceptButton = $accept
    $dialog.ActiveControl = $accept
    $dialog.ClientSize = [Drawing.Size]::new(532, $y + 30 + 14)

    if ($script:form -and $script:form.Visible) { $answer = $dialog.ShowDialog($script:form) }
    else { $answer = $dialog.ShowDialog() }
    $dialog.Dispose()
    if ($answer -eq [Windows.Forms.DialogResult]::Yes) { return 'all' }
    if ($answer -eq [Windows.Forms.DialogResult]::No) { return 'server' }
    if ($answer -eq [Windows.Forms.DialogResult]::Ignore) { return 'later' }
    return ''
}

function Get-ClientFolderForRepair {
    $config = Get-LauncherConfig
    $folder = [string]$config.clientRoot
    if (-not $folder -and [string]$config.clientExecutable) { $folder = Split-Path -Parent ([string]$config.clientExecutable) }
    if ($folder -and (Test-Path -LiteralPath $folder -PathType Container)) { return $folder }
    return ''
}

function Test-ClientExeRepairWanted {
    # The client folder's metin2client.exe is one of the old builds (from a
    # full package of its day: no client package has carried the exe since
    # 2.0.35) and the manifest names the current one.
    if ($script:clientExeRepairTried) { return $false }
    if (-not (Get-M2ClientExeComponent -Manifest $script:latestManifest)) { return $false }
    $folder = Get-ClientFolderForRepair
    return [bool]($folder -and (Test-M2ClientExeOld -ClientFolder $folder))
}

function Invoke-ClientStrayCleanup {
    # The full packages' two stray executables go, and a launcher that starts
    # one starts metin2client.exe, at every start of this window: an update is
    # run by the launcher that was there before it, which has no such step, and
    # a player whose PLAY starts only the server never reaches
    # Start-ConfiguredClient (the operator, 27 September: "zaktualizowalem u
    # siebie i nie usunelo plikow exe starych"). No download - the old exe's
    # replacement is Start-ClientExeRepairIfWanted's.
    try {
        $folder = Get-ClientFolderForRepair
        if (-not $folder) { return }
        foreach ($note in @(Repair-M2ClientExecutables -ClientFolder $folder -ServerRoot $root -ConfigPath $configPath)) {
            Write-LocalLog $note
        }
    }
    catch { Write-LocalLog (UI-Text "Porządki w plikach klienta nieudane: $($_.Exception.Message)" "Tidying up the client files failed: $($_.Exception.Message)") }
}

function Start-ClientExeRepairIfWanted {
    param([switch]$LaunchClient)
    if (-not (Test-ClientExeRepairWanted)) { return $false }
    $script:clientExeRepairTried = $true
    Write-LocalLog (UI-Text 'Klient ma stary metin2client.exe (sprzed czterech stron ekwipunku) - podmieniam go na aktualny.' 'The client has an old metin2client.exe (from before the four inventory pages) - replacing it with the current one.')
    $null = Start-LauncherAction -Action 'RepairClientExe' -Yes -LaunchClient:$LaunchClient
    return $true
}

function Offer-StartupUpdates {
    # Once per session, on the first manifest read. Everything in one action:
    # UpdateAll does the server and then the client in one run, where the
    # client used to wait for the server's action to end and ask again.
    if ($script:startupOfferDone -or -not $script:latestManifest) { return }
    $script:startupOfferDone = $true
    if ($script:activeProcess -and -not $script:activeProcess.HasExited) { return }
    $server = Get-ServerUpdateOffer
    $client = Get-ClientUpdateOffer
    if (-not $server -and -not $client) {
        [void](Start-ClientExeRepairIfWanted)
        return
    }
    $choice = Show-StartupUpdateDialog -Server $server -Client $client
    if ($choice -eq 'all') {
        if ($server) {
            $script:restartAfterUpdate = $true
            if ($client) { Start-LauncherAction -Action 'UpdateAll' -Yes }
            else { Start-LauncherAction -Action 'UpdateServer' -Yes }
        }
        else { Start-LauncherAction -Action 'UpdateClient' -Yes }
        return
    }
    if ($choice -eq 'server') {
        Save-DeclinedOffer -Component 'client' -Version $client.Available
        Write-LocalLog (UI-Text "Aktualizacja klienta $($client.Available) odlozona - wybrano tylko serwer." "Client update $($client.Available) put off - only the server was chosen.")
        $script:restartAfterUpdate = $true
        Start-LauncherAction -Action 'UpdateServer' -Yes
        return
    }
    if ($choice -eq 'later') {
        if ($server) {
            Save-DeclinedOffer -Component 'server' -Version $server.Available
            Write-LocalLog (UI-Text "Aktualizacja serwera $($server.Available) odlozona." "Server update $($server.Available) put off.")
        }
        if ($client) {
            Save-DeclinedOffer -Component 'client' -Version $client.Available
            Write-LocalLog (UI-Text "Aktualizacja klienta $($client.Available) odlozona." "Client update $($client.Available) put off.")
        }
        [void](Start-ClientExeRepairIfWanted)
    }
}

function Update-BotDialogValueLabel {
    # The heading of the bot dialog says what the core will start: the one
    # number, or the sum of the three while each kingdom has its own.
    param($Form)
    if (-not $Form) { return }
    $label = $Form.Controls['valueLabel']
    $check = $Form.Controls['kingdomCheck']
    if (-not $label -or -not $check) { return }
    if ($check.Checked) {
        $sum = [int]$Form.Controls['shinsooBox'].Value + [int]$Form.Controls['chunjoBox'].Value + [int]$Form.Controls['jinnoBox'].Value
        $label.Text = (UI-Text "Boty: $sum (osobno dla królestw)" "Bots: $sum (per kingdom)")
    }
    else { $label.Text = (UI-Text "Boty: $([int]$Form.Controls['botBar'].Value)" "Bots: $([int]$Form.Controls['botBar'].Value)") }
}

function Update-BotDialogChannels {
    # The channel choice of the bot dialog: the share box while there is a
    # second channel, the fresh count while there is a third, and a line on
    # what the choice runs - its ports, its RAM, and a warning when Docker's
    # machine is likely too small for a third or a fourth channel
    # (Get-M2ChannelMemoryWarning: half of the computer's memory under what
    # the channels take, about 2.6 GB each and 1.5 GB for the rest).
    param($Form)
    if (-not $Form) { return }
    $box = $Form.Controls['channelsBox']
    $info = $Form.Controls['channelInfo']
    if (-not $box -or -not $info) { return }
    $channels = [int]$box.SelectedIndex + 1
    $Form.Controls['channelShareBox'].Enabled = $channels -ge 2
    $Form.Controls['freshCountBox'].Enabled = $channels -ge 3
    $text = switch ($channels) {
        1 { (UI-Text 'Jeden kanał (CH1). Porty 13000-13002.' 'One channel (CH1). Ports 13000-13002.') }
        2 { (UI-Text "Dwa kanały: boty rozkładają się na CH1 i CH2, sklepy stoją tylko na CH1.`r`nOk. 2,5 GB RAM więcej. Porty 13000-13012." "Two channels: the bots spread over CH1 and CH2, shops stand on CH1 only.`r`nAbout 2.5 GB more RAM. Ports 13000-13012.") }
        3 { (UI-Text "Trzy kanały: boty świata na CH1 i CH2 (sklepy tylko na CH1), na CH3 świeże`r`nboty od 1 poziomu (bez sklepów, gildii i wojen). Ok. 5 GB RAM więcej.`r`nPorty 13000-13022." "Three channels: the world's bots on CH1 and CH2 (shops on CH1 only), fresh bots`r`nfrom level 1 on CH3 (no shops, guilds or wars). About 5 GB more RAM.`r`nPorts 13000-13022.") }
        default { (UI-Text "Cztery kanały: boty świata na CH1 i CH2 (sklepy tylko na CH1), na CH3 i CH4`r`nświeże boty od 1 poziomu (bez sklepów, gildii i wojen). Ok. 7,5 GB RAM więcej.`r`nPorty 13000-13032." "Four channels: the world's bots on CH1 and CH2 (shops on CH1 only), fresh bots`r`nfrom level 1 on CH3 and CH4 (no shops, guilds or wars). About 7.5 GB more RAM.`r`nPorts 13000-13032.") }
    }
    # The computer's memory is asked of Windows once a session, not at every
    # change of the list; 0 when it would not say, which warns about nothing.
    if ($null -eq $script:botDialogTotalMemory) {
        try { $script:botDialogTotalMemory = [long](Get-CimInstance -ClassName Win32_ComputerSystem -ErrorAction Stop).TotalPhysicalMemory }
        catch { $script:botDialogTotalMemory = 0L }
    }
    $warning = ''
    try { $warning = [string](Get-M2ChannelMemoryWarning -Channels $channels -TotalBytes $script:botDialogTotalMemory) } catch { $warning = '' }
    if ($warning) {
        $text = $text + "`r`n" + $warning
        $info.ForeColor = [Drawing.Color]::FromArgb(170, 60, 0)
    }
    else { $info.ForeColor = [Drawing.SystemColors]::ControlText }
    $info.Text = $text
}

function Add-BotDialogHelp {
    # A "?" beside a setting of the bot dialog: a hover shows what the setting
    # does, a click opens the same text in a box that stays until it is read
    # ("tego nie za bardzo rozumiem, mozesz tam dodac jakies opisy albo znaki
    # zapytania co znaczy kazda regula", 23 September). The text goes on
    # the setting's own label and box too, so a hover anywhere on the row
    # explains it. The tooltip does not wrap, so the texts carry their breaks.
    param(
        [Parameter(Mandatory = $true)][Windows.Forms.Form]$Dialog,
        [Parameter(Mandatory = $true)][Windows.Forms.ToolTip]$Tip,
        [Parameter(Mandatory = $true)][string]$Title,
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][int]$X,
        [Parameter(Mandatory = $true)][int]$Y,
        [object[]]$Also = @()
    )
    $help = [Windows.Forms.Button]::new()
    $help.Text = '?'
    $help.Font = [Drawing.Font]::new('Segoe UI', 9, [Drawing.FontStyle]::Bold)
    $help.FlatStyle = 'Flat'
    $help.Size = [Drawing.Size]::new(24, 24)
    $help.Location = [Drawing.Point]::new($X, $Y)
    $help.Cursor = [Windows.Forms.Cursors]::Hand
    $help.TabStop = $false
    $help.Tag = @{ Title = $Title; Text = $Text }
    $help.Add_Click({
            $info = $this.Tag
            [void][Windows.Forms.MessageBox]::Show($this.FindForm(), [string]$info.Text, [string]$info.Title,
                [Windows.Forms.MessageBoxButtons]::OK, [Windows.Forms.MessageBoxIcon]::Information)
        })
    $Dialog.Controls.Add($help)
    $Tip.SetToolTip($help, $Text)
    foreach ($control in @($Also)) {
        if ($control -is [Windows.Forms.Control]) { $Tip.SetToolTip($control, $Text) }
    }
}

function Show-BotCountDialog {
    # Slider instead of a typed number: the range is a property of the world, and
    # dragging is far friendlier than guessing a value. The maximum is the
    # core's own ceiling (2500, input_db.cpp). The seed holds 1500 identities a
    # kingdom since 2.2.1 (PID 4..4503; 1500 with Chunjo alone), and the one
    # number is split equally between the kingdoms, so 2500 is 834/833/833.
    # A kingdom's own number (the boxes below) goes to 1500, what it holds.
    # Asking for more than a world holds is safe and always was: the core
    # spawns what its registry has and logs requested/registered/started.
    # Under the slider, the spawn plan: the window the cohort arrives over and
    # the second cohort with its hours - "1000 w 15 minut, a dodatkowe 500 w
    # ciagu 24 godzin". Below that, the operator's own number per kingdom
    # (Greess's "Indywidualne wartosci dla krolestw") and the game channels as
    # one choice, "Kanaly gry 1-4" (the second channel's share, and on the 2.x
    # line the fresh cohort's CH3/CH4 with its count).
    # Returns @{ Count; Minutes; Late; Hours; PerKingdom; Shinsoo; Chunjo;
    # Jinno; Channel2; Channel2Share; GameChannels; FreshCount } or $null.
    param([int]$Current = 350, [hashtable]$Plan = @{ Minutes = 1; Late = 0; Hours = 24 },
        [hashtable]$Kingdoms = @{ PerKingdom = $false; Shinsoo = 0; Chunjo = 0; Jinno = 0; Channel2 = $false; Channel2Share = 40
            GameChannels = 1; FreshCount = 200 })
    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = (T 'botDialog')
    $dialog.Size = [Drawing.Size]::new(480, 690)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false

    # What each "?" says (Add-BotDialogHelp). Written for a player, with an
    # example each: the numbers are the core's own rules (SplitPopulation,
    # SetSpawnWindow, ScheduleLateJoiners, the second channel's share).
    $tip = [Windows.Forms.ToolTip]::new()
    $tip.AutoPopDelay = 30000
    $tip.InitialDelay = 250
    $tip.ReshowDelay = 100
    $tip.ShowAlways = $true
    $helpCount = (UI-Text "Ile botów gra na serwerze jednocześnie.`r`n`r`nLiczba dzieli się po równo między trzy królestwa:`r`nnp. 900 to po 300 botów w Shinsoo, Chunjo i Jinno.`r`nKażde królestwo ma 1500 postaci botów, więcej się nie da.`r`n`r`nWięcej botów to więcej pracy dla komputera.`r`nZmiana działa po restarcie serwera." "How many bots play on the server at the same time.`r`n`r`nThe number is split equally between the three kingdoms:`r`ne.g. 900 is 300 bots each in Shinsoo, Chunjo and Jinno.`r`nEach kingdom has 1500 bot characters, more is not possible.`r`n`r`nMore bots means more work for the computer.`r`nA change works after a server restart.")
    $helpMinutes = (UI-Text "W ile minut od startu serwera wchodzą do gry`r`nboty z suwaka.`r`n`r`n1 = prawie wszystkie naraz: szybko, ale przez pierwszą`r`nminutę serwer mocno pracuje.`r`n15 = boty schodzą się przez kwadrans, jak gracze`r`npo otwarciu serwera, a start jest lżejszy dla komputera." "Within how many minutes of the server start the slider's`r`nbots enter the game.`r`n`r`n1 = almost all at once: fast, but for the first minute`r`nthe server works hard.`r`n15 = the bots gather over a quarter of an hour, like players`r`nafter a server opens, and the start is lighter on the computer.")
    $helpLate = (UI-Text "Ilu botów dołączy PÓŹNIEJ, ponad liczbę z suwaka.`r`nWchodzą pojedynczo, równo rozłożone na liczbę godzin`r`nz pola poniżej.`r`n`r`n0 = żadnych, grają tylko boty z suwaka.`r`nNp. suwak 1000 i tu 500: po starcie wchodzi 1000 botów,`r`na przez kolejne godziny dochodzi jeszcze 500, po jednym.`r`n`r`nKrólestwo nie da więcej botów, niż ma postaci (1500)." "How many bots join LATER, on top of the slider's number.`r`nThey enter one at a time, spread evenly over the hours`r`nin the field below.`r`n`r`n0 = none, only the slider's bots play.`r`nE.g. slider 1000 and 500 here: 1000 bots enter after the start,`r`nand over the next hours 500 more arrive, one at a time.`r`n`r`nA kingdom gives no more bots than it has characters (1500).")
    $helpHours = (UI-Text "W ciągu ilu godzin dochodzą dodatkowe boty z pola`r`nwyżej. Rozkładają się równo na ten czas.`r`n`r`nNp. 500 botów w ciągu 24 h to mniej więcej jeden bot`r`nco 3 minuty. Liczy się od startu serwera, więc restart`r`nzaczyna ten plan od nowa.`r`n`r`nNie ma znaczenia, gdy dodatkowych botów jest 0." "Over how many hours the extra bots from the field`r`nabove arrive. They are spread evenly over that time.`r`n`r`nE.g. 500 bots over 24 h is roughly one bot`r`nevery 3 minutes. It counts from the server start, so a restart`r`nbegins this plan again.`r`n`r`nIt does not matter when there are 0 extra bots.")
    $helpKingdoms = (UI-Text "Zamiast jednej liczby z suwaka ustawiasz osobno, ile`r`nbotów gra w każdym królestwie (0-1500). Suwak jest wtedy`r`nwyłączony, a u góry widać sumę.`r`n`r`nNp. Chunjo 1000, Shinsoo 0, Jinno 0 = boty grają tylko`r`nw żółtym królestwie. Królestwo z 0 nie ma żadnego bota." "Instead of one number from the slider you set, for each`r`nkingdom, how many bots play in it (0-1500). The slider is then`r`noff, and the sum shows at the top.`r`n`r`nE.g. Chunjo 1000, Shinsoo 0, Jinno 0 = bots play only`r`nin the yellow kingdom. A kingdom with 0 has no bot at all.")
    $helpChannel = (UI-Text "Ile kanałów gry uruchamia serwer. Przy logowaniu`r`nwybierasz kanał.`r`n`r`n1 = jeden kanał.`r`n2 = drugi kanał (CH2): część botów gra na nim, serwer rozkłada`r`nje na dwa rdzenie procesora. Sklepy (botów i graczy) stoją`r`ntylko na CH1: bot z CH2, który chce handlować, na chwilę`r`nprzechodzi na CH1.`r`n3 i 4 = do tego CH3 (i CH4) ze świeżymi botami od 1 poziomu,`r`nosobnymi od botów z CH1 i CH2. Tam nie ma sklepów, gildii`r`nbotów ani wojen - świeże boty sprzedają u handlarza.`r`n`r`nKażdy kanał to ok. 2,5 GB RAM. Otwiera porty 13000-13012`r`n(2 kanały), 13022 (3) albo 13032 (4). Zmiana działa po`r`nrestarcie serwera. Kanały 3 i 4 są tylko na linii 2.x." "How many game channels the server runs. At the login`r`nyou choose a channel.`r`n`r`n1 = one channel.`r`n2 = a second channel (CH2): some of the bots play on it, and the server`r`nspreads them over two processor cores. Shops (the bots' and the players')`r`nstand on CH1 only: a bot on CH2 that wants to trade moves`r`nto CH1 for a moment.`r`n3 and 4 = and CH3 (and CH4) with fresh bots from level 1,`r`napart from the bots of CH1 and CH2. There are no shops, bot`r`nguilds or wars there - the fresh bots sell to the merchant.`r`n`r`nEvery channel is about 2.5 GB of RAM. Opens ports 13000-13012`r`n(2 channels), 13022 (3) or 13032 (4). A change works after`r`na server restart. Channels 3 and 4 are on the 2.x line only.")
    $helpShare = (UI-Text "Jaka część botów z CH1 i CH2 gra na CH2.`r`nNp. 40 = mniej więcej 4 boty na 10 grają na CH2,`r`nreszta na CH1.`r`n`r`nDziała przy 2 kanałach i więcej." "What share of the bots of CH1 and CH2 plays on CH2.`r`nE.g. 40 = roughly 4 bots in 10 play on CH2,`r`nthe rest on CH1.`r`n`r`nWorks with 2 channels or more.")
    $helpFresh = (UI-Text "Ilu świeżych botów gra naraz na CH3 i CH4 - osobne`r`npostacie od 1 poziomu, po 500 na królestwo, tworzone przy`r`npierwszym starcie z 3 albo 4 kanałami. Liczba dzieli się`r`npo równo na królestwa i pół na pół między CH3 i CH4.`r`n`r`nTo osobna liczba od suwaka: boty z CH1 i CH2 zostają`r`njak były. Działa przy 3 i 4 kanałach." "How many fresh bots play at once on CH3 and CH4 - characters`r`nof their own from level 1, 500 a kingdom, made at the`r`nfirst start with 3 or 4 channels. The number is split equally`r`nbetween the kingdoms and half and half between CH3 and CH4.`r`n`r`nIt is a number apart from the slider: the bots of CH1 and CH2`r`nstay as they were. Works with 3 and 4 channels.")

    $info = [Windows.Forms.Label]::new()
    $info.Text = (UI-Text "Ilu botów ma grać jednocześnie?`r`nKażde królestwo ma 1500 postaci botów. Liczba z suwaka dzieli się po równo`r`nmiędzy królestwa, najwyżej 2500 naraz. Zmiana wymaga restartu serwera." "How many bots should play at the same time?`r`nEach kingdom has 1500 bot characters. The slider's number is split equally`r`nbetween the kingdoms, at most 2500 at once. A change needs a server restart.")
    $info.Location = [Drawing.Point]::new(14, 12)
    $info.Size = [Drawing.Size]::new(440, 54)
    $dialog.Controls.Add($info)

    $valueLabel = [Windows.Forms.Label]::new()
    $valueLabel.Name = 'valueLabel'
    $valueLabel.Font = [Drawing.Font]::new('Segoe UI Semibold', 15)
    $valueLabel.Location = [Drawing.Point]::new(14, 70)
    $valueLabel.Size = [Drawing.Size]::new(400, 32)
    $dialog.Controls.Add($valueLabel)

    $bar = [Windows.Forms.TrackBar]::new()
    $bar.Name = 'botBar'
    $bar.Minimum = 0
    $bar.Maximum = 2500
    $bar.TickFrequency = 50
    $bar.SmallChange = 1
    $bar.LargeChange = 25
    $bar.Location = [Drawing.Point]::new(12, 104)
    $bar.Size = [Drawing.Size]::new(442, 45)
    $bar.Value = [Math]::Max(0, [Math]::Min(2500, $Current))
    $dialog.Controls.Add($bar)
    Add-BotDialogHelp -Dialog $dialog -Tip $tip -Title (UI-Text 'Liczba botów' 'Bot count') -Text $helpCount -X 420 -Y 74 -Also @($valueLabel, $bar)
    $valueLabel.Text = (UI-Text "Boty: $($bar.Value)" "Bots: $($bar.Value)")
    # $this/FindForm keeps the handler independent of captured locals.
    $bar.Add_ValueChanged({
            $form = $this.FindForm()
            if ($form) {
                $label = $form.Controls['valueLabel']
                if ($label) { $label.Text = (UI-Text "Boty: $($this.Value)" "Bots: $($this.Value)") }
            }
        })

    $planInfo = [Windows.Forms.Label]::new()
    $planInfo.Text = (UI-Text "Jak boty wchodzą do gry po starcie serwera. Przy każdym polu jest przycisk ?`r`n- najedź na niego myszką albo kliknij, żeby zobaczyć, co to pole robi." "How the bots enter the game after the server start. Each field has a ? button`r`n- hover over it with the mouse or click it to see what the field does.")
    $planInfo.Location = [Drawing.Point]::new(14, 150)
    $planInfo.Size = [Drawing.Size]::new(440, 34)
    $dialog.Controls.Add($planInfo)

    $rows = @(
        @{ Name = 'minutesBox'; Text = (UI-Text 'Boty z suwaka wchodzą w ciągu (min, 1-180):' 'The slider''s bots enter within (min, 1-180):'); Min = 1; Max = 180; Value = [int]$Plan.Minutes; Y = 188; Title = (UI-Text 'Wejście botów po starcie' 'Bots entering after the start'); Help = $helpMinutes },
        @{ Name = 'lateBox';    Text = (UI-Text 'Dodatkowe boty później (0-2500):' 'Extra bots later (0-2500):'); Min = 0; Max = 2500; Value = [int]$Plan.Late; Y = 218; Title = (UI-Text 'Dodatkowe boty później' 'Extra bots later'); Help = $helpLate },
        @{ Name = 'hoursBox';   Text = (UI-Text 'Dodatkowe boty dochodzą przez (h, 1-168):' 'Extra bots arrive over (h, 1-168):'); Min = 1; Max = 168; Value = [int]$Plan.Hours; Y = 248; Title = (UI-Text 'Czas dochodzenia dodatkowych botów' 'Time over which the extra bots arrive'); Help = $helpHours }
    )
    foreach ($row in $rows) {
        $label = [Windows.Forms.Label]::new()
        $label.Text = $row.Text
        $label.Location = [Drawing.Point]::new(14, $row.Y + 3)
        $label.Size = [Drawing.Size]::new(280, 22)
        $dialog.Controls.Add($label)
        $box = [Windows.Forms.NumericUpDown]::new()
        $box.Name = $row.Name
        $box.Minimum = $row.Min
        $box.Maximum = $row.Max
        $box.Value = [Math]::Max($row.Min, [Math]::Min($row.Max, $row.Value))
        $box.Location = [Drawing.Point]::new(300, $row.Y)
        $box.Size = [Drawing.Size]::new(90, 24)
        $dialog.Controls.Add($box)
        Add-BotDialogHelp -Dialog $dialog -Tip $tip -Title $row.Title -Text $row.Help -X 400 -Y $row.Y -Also @($label, $box)
    }

    # Each kingdom its own number instead of a share of the one above. The
    # core cuts each to the identities that kingdom has - 1500 since 2.2.1,
    # so a box goes no further (it said 2500 while Shinsoo and Jinno held 500,
    # and 729 asked for came out as 500 with no word why, kavvaski).
    $kingdomCheck = [Windows.Forms.CheckBox]::new()
    $kingdomCheck.Name = 'kingdomCheck'
    $kingdomCheck.Text = (UI-Text 'Indywidualne wartości dla królestw' 'Individual numbers for the kingdoms')
    $kingdomCheck.Location = [Drawing.Point]::new(14, 282)
    $kingdomCheck.Size = [Drawing.Size]::new(380, 24)
    $kingdomCheck.Checked = [bool]$Kingdoms.PerKingdom
    $dialog.Controls.Add($kingdomCheck)
    Add-BotDialogHelp -Dialog $dialog -Tip $tip -Title (UI-Text 'Osobno dla królestw' 'Per kingdom') -Text $helpKingdoms -X 400 -Y 282 -Also @($kingdomCheck)
    $kingdomRows = @(
        @{ Name = 'shinsooBox'; Text = (UI-Text 'Shinsoo (czerwone, 0-1500):' 'Shinsoo (red, 0-1500):'); Color = [Drawing.Color]::FromArgb(220, 40, 40); Value = [int]$Kingdoms.Shinsoo; Y = 310 },
        @{ Name = 'chunjoBox';  Text = (UI-Text 'Chunjo (żółte, 0-1500):' 'Chunjo (yellow, 0-1500):');     Color = [Drawing.Color]::FromArgb(235, 200, 30); Value = [int]$Kingdoms.Chunjo; Y = 340 },
        @{ Name = 'jinnoBox';   Text = (UI-Text 'Jinno (niebieskie, 0-1500):' 'Jinno (blue, 0-1500):'); Color = [Drawing.Color]::FromArgb(40, 110, 220); Value = [int]$Kingdoms.Jinno; Y = 370 }
    )
    foreach ($row in $kingdomRows) {
        $swatch = [Windows.Forms.Panel]::new()
        $swatch.BackColor = $row.Color
        $swatch.Location = [Drawing.Point]::new(34, $row.Y + 3)
        $swatch.Size = [Drawing.Size]::new(18, 18)
        $dialog.Controls.Add($swatch)
        $label = [Windows.Forms.Label]::new()
        $label.Text = $row.Text
        $label.Location = [Drawing.Point]::new(58, $row.Y + 3)
        $label.Size = [Drawing.Size]::new(236, 22)
        $dialog.Controls.Add($label)
        $box = [Windows.Forms.NumericUpDown]::new()
        $box.Name = $row.Name
        $box.Minimum = 0
        $box.Maximum = 1500
        $box.Value = [Math]::Max(0, [Math]::Min(1500, $row.Value))
        $box.Location = [Drawing.Point]::new(300, $row.Y)
        $box.Size = [Drawing.Size]::new(90, 24)
        $box.Enabled = $kingdomCheck.Checked
        $dialog.Controls.Add($box)
        $tip.SetToolTip($label, $helpKingdoms)
        $tip.SetToolTip($box, $helpKingdoms)
    }
    $kingdomCheck.Add_CheckedChanged({
            $form = $this.FindForm()
            if (-not $form) { return }
            foreach ($name in @('shinsooBox', 'chunjoBox', 'jinnoBox')) { $form.Controls[$name].Enabled = $this.Checked }
            $form.Controls['botBar'].Enabled = -not $this.Checked
            Update-BotDialogValueLabel $form
        })
    foreach ($name in @('shinsooBox', 'chunjoBox', 'jinnoBox')) {
        $dialog.Controls[$name].Add_ValueChanged({ Update-BotDialogValueLabel $this.FindForm() })
    }
    $bar.Enabled = -not $kingdomCheck.Checked
    Update-BotDialogValueLabel $dialog

    # The game channels, one choice (the operator's "Kanaly gry 1-4", 28
    # September): 1, 2 with the second channel for the world's bots and
    # players (shops on the first only), 3 and 4 that and the fresh cohort's
    # CH3 (and CH4) - bots of their own from level one. Only the 2.x line has
    # the fresh cohort, so r40250's list stops at two.
    $maxChannels = if ((Get-M2ServerEngine -ServerRoot $root) -eq 'r40250') { 2 } else { 4 }
    $channelsLabel = [Windows.Forms.Label]::new()
    $channelsLabel.Text = (UI-Text 'Kanały gry (1-4):' 'Game channels (1-4):')
    $channelsLabel.Location = [Drawing.Point]::new(14, 409)
    $channelsLabel.Size = [Drawing.Size]::new(280, 22)
    $dialog.Controls.Add($channelsLabel)
    $channelsBox = [Windows.Forms.ComboBox]::new()
    $channelsBox.Name = 'channelsBox'
    $channelsBox.DropDownStyle = 'DropDownList'
    foreach ($n in 1..$maxChannels) { [void]$channelsBox.Items.Add([string]$n) }
    $channelsBox.SelectedIndex = [Math]::Max(1, [Math]::Min($maxChannels, [int]$Kingdoms.GameChannels)) - 1
    $channelsBox.Location = [Drawing.Point]::new(300, 406)
    $channelsBox.Size = [Drawing.Size]::new(90, 24)
    $dialog.Controls.Add($channelsBox)
    Add-BotDialogHelp -Dialog $dialog -Tip $tip -Title (UI-Text 'Kanały gry' 'Game channels') -Text $helpChannel -X 400 -Y 406 -Also @($channelsLabel, $channelsBox)
    $shareLabel = [Windows.Forms.Label]::new()
    $shareLabel.Text = (UI-Text 'Ile procent botów gra na CH2 (10-90):' 'Percent of the bots on CH2 (10-90):')
    $shareLabel.Location = [Drawing.Point]::new(34, 437)
    $shareLabel.Size = [Drawing.Size]::new(260, 22)
    $dialog.Controls.Add($shareLabel)
    $shareBox = [Windows.Forms.NumericUpDown]::new()
    $shareBox.Name = 'channelShareBox'
    $shareBox.Minimum = 10
    $shareBox.Maximum = 90
    $shareBox.Value = [Math]::Max(10, [Math]::Min(90, [int]$Kingdoms.Channel2Share))
    $shareBox.Location = [Drawing.Point]::new(300, 434)
    $shareBox.Size = [Drawing.Size]::new(90, 24)
    $dialog.Controls.Add($shareBox)
    Add-BotDialogHelp -Dialog $dialog -Tip $tip -Title (UI-Text 'Boty na CH2' 'Bots on CH2') -Text $helpShare -X 400 -Y 434 -Also @($shareLabel, $shareBox)
    $freshLabel = [Windows.Forms.Label]::new()
    $freshLabel.Text = (UI-Text 'Świeże boty od 1 poz. na CH3/CH4 (0-1500):' 'Fresh bots from level 1 on CH3/CH4 (0-1500):')
    $freshLabel.Location = [Drawing.Point]::new(34, 465)
    $freshLabel.Size = [Drawing.Size]::new(264, 22)
    $dialog.Controls.Add($freshLabel)
    $freshBox = [Windows.Forms.NumericUpDown]::new()
    $freshBox.Name = 'freshCountBox'
    $freshBox.Minimum = 0
    $freshBox.Maximum = 1500
    $freshBox.Value = [Math]::Max(0, [Math]::Min(1500, [int]$Kingdoms.FreshCount))
    $freshBox.Location = [Drawing.Point]::new(300, 462)
    $freshBox.Size = [Drawing.Size]::new(90, 24)
    $dialog.Controls.Add($freshBox)
    Add-BotDialogHelp -Dialog $dialog -Tip $tip -Title (UI-Text 'Świeże boty na CH3 i CH4' 'Fresh bots on CH3 and CH4') -Text $helpFresh -X 400 -Y 462 -Also @($freshLabel, $freshBox)
    $channelInfo = [Windows.Forms.Label]::new()
    $channelInfo.Name = 'channelInfo'
    $channelInfo.Location = [Drawing.Point]::new(14, 494)
    $channelInfo.Size = [Drawing.Size]::new(440, 98)
    $dialog.Controls.Add($channelInfo)
    Update-BotDialogChannels $dialog
    $channelsBox.Add_SelectedIndexChanged({ Update-BotDialogChannels $this.FindForm() })

    $okButton = [Windows.Forms.Button]::new()
    $okButton.Text = (T 'apply')
    $okButton.Location = [Drawing.Point]::new(252, 598)
    $okButton.Size = [Drawing.Size]::new(100, 32)
    $okButton.DialogResult = [Windows.Forms.DialogResult]::OK
    $dialog.Controls.Add($okButton)

    $cancelButton = [Windows.Forms.Button]::new()
    $cancelButton.Text = (T 'cancel')
    $cancelButton.Location = [Drawing.Point]::new(358, 598)
    $cancelButton.Size = [Drawing.Size]::new(96, 32)
    $cancelButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($cancelButton)
    $dialog.AcceptButton = $okButton
    $dialog.CancelButton = $cancelButton

    $result = $dialog.ShowDialog()
    $chosen = @{
        Count         = [int]$bar.Value
        Minutes       = [int]$dialog.Controls['minutesBox'].Value
        Late          = [int]$dialog.Controls['lateBox'].Value
        Hours         = [int]$dialog.Controls['hoursBox'].Value
        PerKingdom    = [bool]$dialog.Controls['kingdomCheck'].Checked
        Shinsoo       = [int]$dialog.Controls['shinsooBox'].Value
        Chunjo        = [int]$dialog.Controls['chunjoBox'].Value
        Jinno         = [int]$dialog.Controls['jinnoBox'].Value
        GameChannels  = [int]$dialog.Controls['channelsBox'].SelectedIndex + 1
        Channel2      = ([int]$dialog.Controls['channelsBox'].SelectedIndex + 1) -ge 2
        Channel2Share = [int]$dialog.Controls['channelShareBox'].Value
        FreshCount    = [int]$dialog.Controls['freshCountBox'].Value
    }
    $tip.Dispose()
    $dialog.Dispose()
    if ($result -ne [Windows.Forms.DialogResult]::OK) { return $null }
    return $chosen
}

function Get-GuiTargetVolume {
    $statePath = Join-Path $root '.m2install.json'
    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        try {
            $state = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
            $vp = $state.PSObject.Properties['databaseVolume']
            if ($vp -and [string]$vp.Value) { return [string]$vp.Value }
            if ([string]$state.projectName) { return "$([string]$state.projectName)_db-data" }
        }
        catch { }
    }
    $envPath = Join-Path $root 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $match = [Regex]::Match([IO.File]::ReadAllText($envPath), '(?m)^M2_COMPOSE_PROJECT_NAME=([a-z0-9][a-z0-9_-]+)\s*$')
        if ($match.Success) { return "$($match.Groups[1].Value)_db-data" }
    }
    return ''
}

function Start-LauncherAction {
    param(
        [Parameter(Mandatory = $true)][string]$Action,
        [switch]$Yes,
        [switch]$LaunchClient,
        [switch]$OpenSupport,
        [string[]]$ExtraArgs = @()
    )
    if ($script:activeProcess -and -not $script:activeProcess.HasExited) {
        [Windows.Forms.MessageBox]::Show((UI-Text 'Poczekaj na zakończenie bieżącej operacji.' 'Wait for the current operation to finish.'), (UI-Text 'Launcher pracuje' 'The launcher is busy'), 'OK', 'Information') | Out-Null
        return
    }

    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $script:activeOut = Join-Path $logDirectory ("action-$stamp.out.log")
    $script:activeErr = Join-Path $logDirectory ("action-$stamp.err.log")
    $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ('"{0}"' -f $cliLauncher), '-Action', $Action)
    if ($Yes) { $arguments += '-Yes' }
    # Start-Process joins ArgumentList with spaces and quotes nothing, so a value
    # containing a space arrives at the CLI as two arguments. The default install
    # folder is "Metin2 Singleplayer", so restoring a backup sent
    # "C:\...\Metin2" to -RestoreSource and the tail
    # "Singleplayer\Serwer\backups\db-backup-....zip" to the next positional
    # parameter - which is [int]$BotCount - and every restore died with "Cannot
    # convert value ... to type System.Int32" (NieBijOddam, 13 September).
    # Parameter names pass through untouched; every value is quoted.
    foreach ($extra in @($ExtraArgs)) {
        $text = [string]$extra
        if ($text -match '^-[A-Za-z]') { $arguments += $text }
        else { $arguments += ('"{0}"' -f ($text -replace '"', '\"')) }
    }
    Write-LocalLog (UI-Text "Rozpoczęto akcję $Action." "Action $Action started.")
    $script:activeAction = $Action
    $script:launchClientAfterAction = [bool]$LaunchClient
    $script:openSupportAfterAction = [bool]$OpenSupport
    # Live-progress state for this run.
    $script:activeOutOffset = 0
    $script:activeErrOffset = 0
    $script:activeOutputAll = ''
    $script:activeStarted = Get-Date
    $script:activePhase = ''
    $script:activePhaseSince = Get-Date
    $script:activePhaseStep = 0
    $script:activePhaseTotal = 0
    $script:activeBuildNoticed = $false
    # After this long on one phase the status line says so. Four minutes is
    # past every normal compose step and well inside a first build's stages,
    # which carry their own step counter anyway.
    $script:activeStallSeconds = 240
    $script:activeStallNoticed = $false
    # The game core's compile: its label, the minute a slow one gets a word in
    # the log, and whether the build said the Docker VM is short of memory.
    $script:activePhaseLabel = ''
    $script:activeCompileHintSeconds = 600
    $script:activeCompileHintNoticed = $false
    $script:activeLowMemory = $false
    $script:actionStatus.Text = (UI-Text "Trwa: $Action..." "Running: $Action...")
    $script:actionStatus.ForeColor = [Drawing.Color]::Gold
    $script:progress.Style = 'Marquee'
    $script:activeProcess = Start-Process -FilePath 'powershell.exe' `
        -ArgumentList $arguments -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $script:activeOut -RedirectStandardError $script:activeErr
    # Start-Process -PassThru with redirected stdout/stderr does not keep the OS
    # process handle, so a later $activeProcess.ExitCode reads $null even after
    # the process has exited cleanly. That made every action -- including a fully
    # successful start -- report "Błąd: ... (kod )" with an empty code. Touching
    # .Handle now caches it so ExitCode is readable in Complete-LauncherAction.
    try { $null = $script:activeProcess.Handle } catch {}
}

function Install-Or-Prepare {
    # @(...) round the whole pipeline, not only its input: Where-Object hands
    # back a bare string when one file is missing, and under Set-StrictMode a
    # string has no .Count - the button threw "The property 'Count' cannot be
    # found" at exactly the player whose package was incomplete.
    $missing = @(@($cliLauncher, $composeFile, $modulePath) | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
    if ($missing.Count -gt 0) {
        [Windows.Forms.MessageBox]::Show((UI-Text 'Paczka jest niekompletna. Rozpakuj ponownie całe archiwum RAR.' 'The package is incomplete. Unpack the whole RAR archive again.'), (UI-Text 'Brak plików' 'Files missing'), 'OK', 'Error') | Out-Null
        return
    }

    if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
        $answer = [Windows.Forms.MessageBox]::Show(
            (UI-Text 'Docker Desktop nie jest zainstalowany. Czy otworzyć oficjalną stronę pobierania?' 'Docker Desktop is not installed. Open the official download page?'),
            (UI-Text 'Wymagany Docker Desktop' 'Docker Desktop required'), 'YesNo', 'Question')
        if ($answer -eq [Windows.Forms.DialogResult]::Yes) {
            Start-Process 'https://www.docker.com/products/docker-desktop/'
        }
        return
    }

    $preflight = Get-M2DockerPreflight -ServerRoot $root
    Write-LocalLog (Format-M2DockerPreflightReport -Report $preflight)
    if (-not $preflight.CanStart) {
        [Windows.Forms.MessageBox]::Show(
            ((@($preflight.BlockingIssues) -join [Environment]::NewLine) + [Environment]::NewLine + [Environment]::NewLine + (UI-Text 'Po naprawie uruchom launcher ponownie.' 'After the repair, start the launcher again.')),
            (UI-Text 'Komputer nie jest jeszcze gotowy' 'The computer is not ready yet'),
            'OK',
            'Warning') | Out-Null
        return
    }

    # This button prepares a package whose game sources are already on disk;
    # it never fetches them, because they are the operator's own r40250 files
    # and only installer\install.ps1 knows how to take them from that package.
    # Until now it said "Paczka jest gotowa" regardless - and a player whose
    # sources were missing pressed it, was told the package was ready, pressed
    # GRAJ and got fifteen Docker errors. Reported from the Discord: "re-running
    # the installer through Install in GUI did not restore the sources" - it
    # could not have, this is not the installer. Say which it is.
    $gameContext = Join-Path $root 'linux-port\docker\game\src'
    foreach ($made in @(Restore-M2EmptyGameContextDirs -ServerRoot $root)) {
        Write-LocalLog ((UI-Text "Odtworzono pusty katalog budowy: " "Recreated an empty build folder: ") + $made)
    }
    $requiredContext = @(Get-M2RequiredGameContext -ServerRoot $root)
    $missingContext = @($requiredContext | Where-Object { -not (Test-Path -LiteralPath (Join-Path $gameContext $_)) })
    # The database dumps are the other half of what the installer takes out
    # of the package, and the half nobody saw missing until MariaDB came up
    # empty: name them here with the sources.
    $missingContext += @(Get-M2MissingSqlDumps -ServerRoot $root | ForEach-Object { 'mariadb\initdb.d\dumps\' + $_ })
    if ($missingContext.Count -gt 0) {
        $installerPath = Join-Path $root 'installer\install.ps1'
        Write-LocalLog ((UI-Text "Brak zrodel gry w " "Game sources missing in ") + $gameContext + ": " + ($missingContext -join ', '))
        [Windows.Forms.MessageBox]::Show(
            ((UI-Text "Ten przycisk przygotowuje paczke, ktora ma juz na dysku zrodla gry - a tu ich nie ma." "This button prepares a package that already has the game sources on disk - and here they are not.") + [Environment]::NewLine +
             (UI-Text "Brakuje: " "Missing: ") + ($missingContext -join ', ') + [Environment]::NewLine + [Environment]::NewLine +
             (UI-Text "Zrodla pochodza z Twojej wlasnej paczki serwera r40250 i zaden przycisk launchera ani zadna aktualizacja ich nie pobiera - " "The sources come from your own r40250 server package, and no launcher button and no update downloads them - ") +
             (UI-Text "robi to wylacznie instalator, ktory wyciaga z tej paczki to, czego trzeba." "only the installer does, taking what it needs out of that package.") + [Environment]::NewLine + [Environment]::NewLine +
             (UI-Text "Uruchom w PowerShell jako administrator:" "Run in PowerShell as administrator:") + [Environment]::NewLine +
             (UI-Text "  `$env:M2_SRC_ARCHIVE = 'C:\sciezka\do\paczki-r40250.zip'" "  `$env:M2_SRC_ARCHIVE = 'C:\path\to\package-r40250.zip'") + [Environment]::NewLine +
             "  & '" + $installerPath + "'" + [Environment]::NewLine + [Environment]::NewLine +
             (UI-Text "Baza, postacie i ustawienia zostaja nietkniete." "The database, characters and settings stay untouched.")),
            (UI-Text 'Brak zrodel gry - potrzebny instalator' 'Game sources missing - the installer is needed'), 'OK', 'Warning') | Out-Null
        return
    }

    if (-not (Find-ClientExecutable)) { [void](Select-ClientExecutable) }
    try {
        $desktop = [Environment]::GetFolderPath('Desktop')
        $shortcutPath = Join-Path $desktop 'Metin2 Playerbots.lnk'
        $shell = New-Object -ComObject WScript.Shell
        $shortcut = $shell.CreateShortcut($shortcutPath)
        $shortcut.TargetPath = Join-Path $root 'Metin2-Launcher-GUI.bat'
        $shortcut.WorkingDirectory = $root
        $shortcut.Description = 'Metin2 Singleplayer Playerbots'
        $shortcut.Save()
        Write-LocalLog (UI-Text 'Sprawdzono paczkę i utworzono skrót na pulpicie.' 'Package checked and a desktop shortcut created.')
    }
    catch { Write-LocalLog (UI-Text "Nie udało się utworzyć skrótu: $($_.Exception.Message)" "Could not create the shortcut: $($_.Exception.Message)") }
    [Windows.Forms.MessageBox]::Show(
        (UI-Text 'Paczka jest gotowa. Utworzono skrót na pulpicie. Kliknij GRAJ, aby uruchomić Docker, serwer i klienta.' 'The package is ready. A desktop shortcut was created. Click PLAY to start Docker, the server and the client.'),
        (UI-Text 'Gotowe' 'Done'), 'OK', 'Information') | Out-Null
}

$script:launcherFingerprint = Get-LauncherFingerprint

$script:form = [Windows.Forms.Form]::new()
$script:form.Text = (T 'formTitle')
$script:form.Size = [Drawing.Size]::new(780, 806)
$script:form.MinimumSize = [Drawing.Size]::new(780, 764)
$script:form.StartPosition = 'CenterScreen'
$script:form.BackColor = [Drawing.Color]::FromArgb(24, 25, 29)
$script:form.ForeColor = [Drawing.Color]::White
$script:form.Font = [Drawing.Font]::new('Segoe UI', 9)

$title = [Windows.Forms.Label]::new()
$title.Text = (T 'title')
$title.Font = [Drawing.Font]::new('Segoe UI Semibold', 19)
$title.ForeColor = [Drawing.Color]::FromArgb(247, 194, 66)
$title.Location = [Drawing.Point]::new(24, 18)
$title.Size = [Drawing.Size]::new(710, 38)
$script:form.Controls.Add($title)

$subtitle = [Windows.Forms.Label]::new()
$subtitle.Text = (T 'subtitle')
$subtitle.Location = [Drawing.Point]::new(27, 58)
$subtitle.Size = [Drawing.Size]::new(700, 24)
$subtitle.ForeColor = [Drawing.Color]::Silver
$script:form.Controls.Add($subtitle)

$script:dockerStatus = [Windows.Forms.Label]::new()
$script:dockerStatus.Location = [Drawing.Point]::new(28, 92)
$script:dockerStatus.Size = [Drawing.Size]::new(310, 25)
$script:dockerStatus.Font = [Drawing.Font]::new('Segoe UI Semibold', 10)
$script:form.Controls.Add($script:dockerStatus)

$script:serverStatus = [Windows.Forms.Label]::new()
$script:serverStatus.Location = [Drawing.Point]::new(375, 92)
$script:serverStatus.Size = [Drawing.Size]::new(300, 25)
$script:serverStatus.Font = [Drawing.Font]::new('Segoe UI Semibold', 10)
$script:form.Controls.Add($script:serverStatus)

$installButton = New-Button (T 'install') 28 128 338 58 ([Drawing.Color]::FromArgb(88, 82, 160))
$playButton = New-Button (T 'play') 388 128 338 58 ([Drawing.Color]::FromArgb(27, 150, 88))
# Whether PLAY opens the client as well. It sits in the gap under the PLAY
# button, so no other control moves; the choice is kept in
# launcher.config.json, because somebody who runs the world for other
# people wants it off every time, not once.
$script:launchClientCheck = [Windows.Forms.CheckBox]::new()
$script:launchClientCheck.Text = (T 'launchClient')
$script:launchClientCheck.Location = [Drawing.Point]::new(390, 187)
$script:launchClientCheck.Size = [Drawing.Size]::new(336, 17)
$script:launchClientCheck.Font = [Drawing.Font]::new('Segoe UI', 8)
$script:launchClientCheck.ForeColor = [Drawing.Color]::Silver
function Update-PlayButtonLabel {
    $playButton.Text = if ($script:launchClientCheck.Checked) { (T 'play') } else { (T 'playNoClient') }
}
# Ustawiane przed podpieciem obslugi zmiany, zeby pierwsze przypisanie nie
# zapisywalo pliku konfiguracji przy samym otwarciu okna.
$script:launchClientCheck.Checked = [bool](Get-LauncherConfig).launchClientOnPlay
Update-PlayButtonLabel
$script:launchClientCheck.Add_CheckedChanged({
    $config = Get-LauncherConfig
    $config.launchClientOnPlay = $script:launchClientCheck.Checked
    Save-M2LauncherConfig -Config $config -ConfigPath $configPath
    Update-PlayButtonLabel
    Write-LocalLog $(if ($script:launchClientCheck.Checked) {
        (UI-Text 'GRAJ bedzie uruchamiac takze klienta gry.' 'PLAY will start the game client too.') } else {
        (UI-Text 'GRAJ bedzie uruchamiac sam serwer - klient zostaje wylaczony.' 'PLAY will start the server only - the client stays off.') })
})
$script:form.Controls.Add($script:launchClientCheck)

$dockerButton = New-Button (T 'docker') 28 202 218 50
$stopButton = New-Button (T 'stop') 268 202 218 50 ([Drawing.Color]::FromArgb(180, 75, 55))
$panelButton = New-Button (T 'panel') 508 202 218 50 ([Drawing.Color]::FromArgb(180, 125, 35))
$clientButton = New-Button (T 'client') 28 266 218 48 ([Drawing.Color]::FromArgb(75, 90, 120))
$updateButton = New-Button (T 'update') 268 266 218 48 ([Drawing.Color]::FromArgb(75, 90, 120))
$bundleButton = New-Button (T 'bundle') 508 266 218 48 ([Drawing.Color]::FromArgb(75, 90, 120))
$diagnosticsButton = New-Button (T 'diagnostics') 28 328 218 45 ([Drawing.Color]::FromArgb(45, 110, 190))
$openLogButton = New-Button (T 'openLog') 262 328 218 45 ([Drawing.Color]::FromArgb(58, 62, 72))
$folderButton = New-Button (T 'logFolder') 496 328 230 45 ([Drawing.Color]::FromArgb(58, 62, 72))
$botCountButton = New-Button (T 'botCount') 28 380 218 32 ([Drawing.Color]::FromArgb(120, 95, 40))
$importDbButton = New-Button (T 'importDb') 262 380 218 32 ([Drawing.Color]::FromArgb(70, 120, 90))
$repairDbButton = New-Button (T 'repairDb') 496 380 230 32 ([Drawing.Color]::FromArgb(150, 90, 55))
$dbAccessButton = New-Button (T 'dbAccess') 28 418 218 32 ([Drawing.Color]::FromArgb(70, 100, 130))
# The optional, experimental client half of the GM panel (F9): the server half
# rides in every update, this button fetches the client package from the
# manifest's `client` component and swaps pack/root.eix + root.epk.
$gmPanelButton = New-Button (T 'gmPanel') 262 418 218 32 ([Drawing.Color]::FromArgb(120, 70, 130))
# On the mt2009 line the client update is the ordinary one - the packs the
# server's root points at - and not the experimental GM panel.
$script:clientUpdateIsPlain = ((Get-M2ServerEngine -ServerRoot $root) -ne 'r40250')
if ($script:clientUpdateIsPlain) { $gmPanelButton.Text = (T 'updateClient') }
# Backup, restore and "start over" behind one button: reported from the
# Discord as "the launcher can import a database but nothing says how to
# export one", together with a wish to get back to a fresh install.
$worldBackupButton = New-Button (T 'worldBackup') 496 418 230 32 ([Drawing.Color]::FromArgb(70, 120, 90))
# The world's difficulty - the waits at the Biologist and the stable keeper -
# chosen here and applied at the next start (M2_DIFFICULTY in .env).
$difficultyButton = New-Button (T 'difficulty') 28 456 218 32 ([Drawing.Color]::FromArgb(120, 95, 40))
# COOP (experimental): this world played with friends over the Internet
# (Show-CoopDialog). The button exists only when the optional module does.
$coopModulePath = Join-Path $root 'launcher\Metin2Launcher.Coop.psm1'
$coopButton = $null
if (Test-Path -LiteralPath $coopModulePath -PathType Leaf) {
    Import-Module $coopModulePath -Force
    $coopButton = New-Button (T 'coop') 262 456 218 32 ([Drawing.Color]::FromArgb(40, 120, 150))
}
# This world on a rented Linux VPS (Show-VpsDialog): for everybody, the invite
# codes included. The button exists only when the
# optional module does - the 1.x line ships the window without it.
$vpsModulePath = Join-Path $root 'launcher\Metin2Launcher.Vps.psm1'
$vpsButton = $null
if (Test-Path -LiteralPath $vpsModulePath -PathType Leaf) {
    Import-Module $vpsModulePath -Force
    $vpsButton = New-Button (T 'vps') 496 456 230 32 ([Drawing.Color]::FromArgb(60, 110, 90))
}

# The language switch sits with the other small buttons rather than in a menu:
# somebody who cannot read the window needs to find it without reading anything.
$languageButton = New-Button (T 'language') 508 702 218 28 ([Drawing.Color]::FromArgb(60, 70, 95))
$languageButton.Add_Click({ Switch-LauncherLanguage })

foreach ($button in @($installButton, $playButton, $dockerButton, $stopButton, $panelButton, $clientButton, $updateButton, $bundleButton, $diagnosticsButton, $openLogButton, $folderButton, $botCountButton, $importDbButton, $repairDbButton, $dbAccessButton, $gmPanelButton, $worldBackupButton, $difficultyButton, $languageButton)) {
    $script:form.Controls.Add($button)
}
if ($coopButton) { $script:form.Controls.Add($coopButton) }
if ($vpsButton) { $script:form.Controls.Add($vpsButton) }

$script:actionStatus = [Windows.Forms.Label]::new()
$script:actionStatus.Text = (T 'ready')
$script:actionStatus.Location = [Drawing.Point]::new(28, 500)
$script:actionStatus.Size = [Drawing.Size]::new(690, 24)
$script:actionStatus.Font = [Drawing.Font]::new('Segoe UI Semibold', 9)
$script:form.Controls.Add($script:actionStatus)

$script:progress = [Windows.Forms.ProgressBar]::new()
$script:progress.Location = [Drawing.Point]::new(28, 528)
$script:progress.Size = [Drawing.Size]::new(698, 12)
$script:form.Controls.Add($script:progress)

$script:logBox = [Windows.Forms.TextBox]::new()
$script:logBox.Location = [Drawing.Point]::new(28, 550)
$script:logBox.Size = [Drawing.Size]::new(698, 104)
$script:logBox.Multiline = $true
$script:logBox.ReadOnly = $true
$script:logBox.ScrollBars = 'Vertical'
$script:logBox.BackColor = [Drawing.Color]::FromArgb(12, 13, 16)
$script:logBox.ForeColor = [Drawing.Color]::Gainsboro
$script:logBox.Font = [Drawing.Font]::new('Consolas', 8.5)
$script:form.Controls.Add($script:logBox)

$footer = [Windows.Forms.Label]::new()
$footer.Text = (T 'footer')
$footer.Location = [Drawing.Point]::new(28, 660)
$footer.Size = [Drawing.Size]::new(700, 25)
$footer.ForeColor = [Drawing.Color]::DarkGray
$script:form.Controls.Add($footer)


$script:versionLabel = [Windows.Forms.Label]::new()
$script:versionLabel.Location = [Drawing.Point]::new(28, 666)
# Four lines when an update is waiting: the "!! NOWA WERSJA" notice goes above
# the three version lines, and at 54 pixels the client line was cut off.
$script:versionLabel.Size = [Drawing.Size]::new(700, 74)
$script:versionLabel.ForeColor = [Drawing.Color]::Silver
$script:versionLabel.Font = [Drawing.Font]::new('Segoe UI Semibold', 9)
$script:form.Controls.Add($script:versionLabel)

$script:latestServerVersion = $null
$script:latestClientVersion = $null
$script:latestVersionChecked = $false

function Get-LauncherVersionOnDisk {
    # The launcher ships inside the server package, so the VERSION file beside
    # it is its version. Read at startup for what this window runs, and again
    # for the footer: after an update applied in this session the file is
    # ahead of the process, and the footer says so.
    $path = Join-Path $root 'VERSION'
    try {
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $text = (Get-Content -LiteralPath $path -Raw -ErrorAction Stop).Trim()
            if ($text) { return $text }
        }
    }
    catch { }
    return (UI-Text 'nieznana' 'unknown')
}
$script:launcherVersion = Get-LauncherVersionOnDisk

function Set-LatestVersionsFromManifest {
    param($Manifest)
    if (-not $Manifest) { return }
    $serverProperty = $Manifest.PSObject.Properties['server']
    if ($serverProperty -and $serverProperty.Value -and [string]$serverProperty.Value.version) {
        $script:latestServerVersion = ([string]$serverProperty.Value.version).Trim()
    }
    $clientProperty = $Manifest.PSObject.Properties['client']
    if ($clientProperty -and $clientProperty.Value -and [string]$clientProperty.Value.version) {
        $script:latestClientVersion = ([string]$clientProperty.Value.version).Trim()
    }
}

function Update-VersionFooter {
    # The manifest lives behind GitHub's anonymous per-IP budget, so it is read
    # once per session and whenever the player asks for a check - never on the
    # 8-second status timer, which would spend that budget for nothing.
    $installed = Get-InstalledServerVersion
    $installedText = if ($installed -and $installed -ne 'unknown') { $installed } else { (UI-Text 'nieznana (przebudowa w toku)' 'unknown (rebuild in progress)') }
    $latestText = if ($script:latestServerVersion) { $script:latestServerVersion }
        elseif ($script:latestVersionChecked) { (UI-Text 'nie udalo sie sprawdzic' 'could not check') }
        else { (UI-Text 'sprawdzanie...' 'checking...') }
    $latestClientText = if ($script:latestClientVersion) { $script:latestClientVersion }
        elseif ($script:latestVersionChecked) { (UI-Text 'nie udalo sie sprawdzic' 'could not check') }
        else { (UI-Text 'sprawdzanie...' 'checking...') }
    # Three lines, asked for on the Discord: the server, the launcher itself
    # (its newest version is the server package's) and the client.
    $onDisk = Get-LauncherVersionOnDisk
    $launcherText = $script:launcherVersion
    if ($onDisk -ne $script:launcherVersion) {
        $launcherText = (UI-Text '{0} (na dysku {1} - uruchom launcher ponownie)' '{0} (on disk {1} - restart the launcher)') -f $script:launcherVersion, $onDisk
    }
    $clientInstalled = Get-InstalledClientVersion
    $clientText = if ($clientInstalled -and $clientInstalled -ne 'unknown') { $clientInstalled } else { (UI-Text 'nieznana' 'unknown') }
    $script:versionLabel.Text = ((UI-Text "Serwer: {0}   |   najnowszy: {1}`r`nLauncher: {2}   |   najnowszy: {3}`r`nKlient: {4}   |   najnowszy: {5}" "Server: {0}   |   latest: {1}`r`nLauncher: {2}   |   latest: {3}`r`nClient: {4}   |   latest: {5}") -f
        $installedText, $latestText, $launcherText, $latestText, $clientText, $latestClientText)
    $upToDate = $script:latestServerVersion -and $installed -and $installed -ne 'unknown' -and
        $installed.Equals($script:latestServerVersion, [StringComparison]::OrdinalIgnoreCase)
    # The same question for the client, which has its own version and its own
    # button. A player who has the newest server and an old client saw nothing
    # but a green footer, because only the server was ever compared.
    $clientBehind = $false
    if ($script:latestClientVersion -and $clientInstalled -and $clientInstalled -ne 'unknown') {
        $clientBehind = -not $clientInstalled.Equals(
            $script:latestClientVersion, [StringComparison]::OrdinalIgnoreCase)
    }
    $serverBehind = $script:latestServerVersion -and $installed -and $installed -ne 'unknown' -and -not $upToDate
    # What the blink timer below reads. A colour alone is easy to miss on a
    # window nobody is looking at, and "nie wiedzialem ze jest nowa wersja" is
    # what this is for: the line says so in words as well.
    $script:updateAvailable = [bool]($serverBehind -or $clientBehind)
    if ($script:updateAvailable) {
        $what = if ($serverBehind -and $clientBehind) { (UI-Text 'SERWERA I KLIENTA' 'SERVER AND CLIENT') }
            elseif ($serverBehind) { (UI-Text 'SERWERA' 'SERVER') }
            else { (UI-Text 'KLIENTA' 'CLIENT') }
        # The button's own words (T 'update'): the line used to send players
        # to a "ZAINSTALUJ AKTUALIZACJE" nobody could find (Artur554, 25 September).
        $script:versionLabel.Text = ((UI-Text "!! NOWA WERSJA {0} - kliknij {1}`r`n{2}" "!! NEW {0} VERSION - click {1}`r`n{2}") -f
            $what, (T 'update'), $script:versionLabel.Text)
    }
    $script:versionBaseColor = if ($upToDate -and -not $clientBehind) { [Drawing.Color]::LightGreen }
        elseif ($script:latestServerVersion) { [Drawing.Color]::Gold }
        else { [Drawing.Color]::Silver }
    $script:versionLabel.ForeColor = $script:versionBaseColor
}

function Read-LatestServerVersion {
    param([switch]$Force)
    if ($script:latestVersionChecked -and -not $Force) { return }
    $script:latestVersionChecked = $true
    try {
        $config = Get-M2LauncherConfig -ServerRoot $root -ConfigPath $configPath
        $manifest = Get-M2UpdateManifest -Source ([string]$config.manifestUrl) -TimeoutSec 8
        $script:latestManifest = $manifest
        Set-LatestVersionsFromManifest -Manifest $manifest
    }
    catch { }
    Update-VersionFooter
    Offer-StartupUpdates
}

$installButton.Add_Click({ Install-Or-Prepare })
$playButton.Add_Click({
    $withClient = $script:launchClientCheck.Checked
    if ($withClient) {
        if (-not (Find-ClientExecutable)) {
            if (-not (Select-ClientExecutable)) { return }
        }
    }
    Start-LauncherAction -Action 'Start' -LaunchClient:$withClient
})
$dockerButton.Add_Click({ Start-LauncherAction -Action 'StartDocker' })
$stopButton.Add_Click({
    # The button used to stop Docker Desktop with the server, and with it
    # whatever else the player runs in Docker ("nie powinna mi go wylaczac,
    # zamykajac tym samym inne projekty", Producent Hip Hopu, 28 September).
    # Yes is the old stop - the engine's machine holds gigabytes of RAM, which
    # is why it went at all - and No stops the server alone.
    $answer = [Windows.Forms.MessageBox]::Show((T 'stopAsk'), (T 'stopAskTitle'), 'YesNoCancel', 'Question')
    if ($answer -eq [Windows.Forms.DialogResult]::Yes) { Start-LauncherAction -Action 'StopAll' }
    elseif ($answer -eq [Windows.Forms.DialogResult]::No) { Start-LauncherAction -Action 'Stop' }
})
function Get-M2PanelAddresses {
    # Both web panels of the same world, at whatever ports this installation
    # actually publishes them on. The defaults match the compose file; a world
    # whose .env moves a port is followed rather than guessed at.
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $classic = 7788
    $seban = 7790
    $envPath = Join-Path $ServerRoot 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $match = Select-String -LiteralPath $envPath -Pattern '^M2_PANEL_PUBLIC_PORT=(\d+)$' | Select-Object -First 1
        if ($match) { $classic = [int]$match.Matches[0].Groups[1].Value }
        $match = Select-String -LiteralPath $envPath -Pattern '^M2_SEBAN_PANEL_PORT=(\d+)$' | Select-Object -First 1
        if ($match) { $seban = [int]$match.Matches[0].Groups[1].Value }
    }
    return [pscustomobject]@{
        ClassicUrl = "http://127.0.0.1:$classic/map"
        SebanUrl   = "http://127.0.0.1:$seban/"
        ClassicPort = $classic
        SebanPort   = $seban
    }
}

function Show-PanelPasswordDialog {
    # In-process and read-only: an action would print the passphrase through the
    # launcher log, and the launcher log travels in support bundles that get
    # posted on the Discord. Same rule as the database credentials dialog.
    $envPath = Join-Path $root 'linux-port\docker\.env'
    $passphrase = ''
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $match = [Regex]::Match([IO.File]::ReadAllText($envPath), '(?m)^M2_PANEL_PASSWORD=(.+?)\s*$')
        if ($match.Success) { $passphrase = $match.Groups[1].Value }
    }
    if (-not $passphrase) {
        [Windows.Forms.MessageBox]::Show(
            (UI-Text "W pliku .env nie ma jeszcze hasla do panelu.`r`n`r`nKliknij GRAJ raz - launcher je uzupelni i pokaze." "The .env file has no panel password yet.`r`n`r`nClick PLAY once - the launcher fills it in and shows it."),
            (UI-Text 'Haslo do panelu' 'Panel password'), 'OK', 'Information') | Out-Null
        return
    }

    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = (UI-Text 'Haslo do panelu WWW' 'Web panel password')
    $dialog.Size = [Drawing.Size]::new(520, 250)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false

    $info = [Windows.Forms.Label]::new()
    $info.Text = (UI-Text 'Panel ma jedno haslo i nie ma loginu. Zaznacz je i skopiuj.' 'The panel has one password and no login. Select it and copy it.')
    $info.Location = [Drawing.Point]::new(16, 14)
    $info.Size = [Drawing.Size]::new(480, 20)
    $dialog.Controls.Add($info)

    $box = [Windows.Forms.TextBox]::new()
    $box.Text = $passphrase
    $box.ReadOnly = $true
    $box.Location = [Drawing.Point]::new(16, 40)
    $box.Size = [Drawing.Size]::new(480, 26)
    $box.Font = [Drawing.Font]::new('Consolas', 12)
    $dialog.Controls.Add($box)

    $hint = [Windows.Forms.Label]::new()
    $hint.Text = ((UI-Text 'Jesli panel go nie przyjmuje, zapamietal starsze haslo z pierwszego' 'If the panel rejects it, it kept an older password from its first') + [Environment]::NewLine +
        (UI-Text 'uruchomienia. Reset kasuje jeden plik konfiguracyjny panelu i ustawia' 'start. The reset deletes one configuration file of the panel and sets') + [Environment]::NewLine +
        (UI-Text 'haslo powyzej. Swiat, postacie i boty sa w bazie i nie sa tym ruszane.' 'the password above. The world, characters and bots are in the database, untouched.'))
    $hint.Location = [Drawing.Point]::new(16, 76)
    $hint.Size = [Drawing.Size]::new(480, 60)
    $dialog.Controls.Add($hint)

    $resetButton = [Windows.Forms.Button]::new()
    $resetButton.Text = (UI-Text 'Zresetuj haslo panelu' 'Reset the panel password')
    $resetButton.Location = [Drawing.Point]::new(16, 148)
    $resetButton.Size = [Drawing.Size]::new(230, 34)
    $resetButton.DialogResult = [Windows.Forms.DialogResult]::Yes
    $dialog.Controls.Add($resetButton)

    $closeButton = [Windows.Forms.Button]::new()
    $closeButton.Text = (UI-Text 'Zamknij' 'Close')
    $closeButton.Location = [Drawing.Point]::new(396, 148)
    $closeButton.Size = [Drawing.Size]::new(100, 34)
    $closeButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($closeButton)
    $dialog.CancelButton = $closeButton

    $answer = $dialog.ShowDialog()
    $dialog.Dispose()
    if ($answer -ne [Windows.Forms.DialogResult]::Yes) { return }
    Start-LauncherAction -Action 'PanelPassword' -Yes
}

function Show-CoopSecretDialog {
    # Passwords are shown here and nowhere else: an action's output is a file
    # under launcher-logs, and support bundles carry that folder.
    param([string]$Title, [string]$Intro, [string]$Secret)
    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = $Title
    $dialog.Size = [Drawing.Size]::new(560, 290)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false
    $info = [Windows.Forms.Label]::new()
    $info.Text = $Intro
    $info.Location = [Drawing.Point]::new(14, 12)
    $info.Size = [Drawing.Size]::new(520, 44)
    $dialog.Controls.Add($info)
    $box = [Windows.Forms.TextBox]::new()
    $box.Text = $Secret
    $box.ReadOnly = $true
    $box.Multiline = $true
    $box.ScrollBars = 'Vertical'
    $box.WordWrap = $true
    $box.Location = [Drawing.Point]::new(14, 60)
    $box.Size = [Drawing.Size]::new(520, 132)
    $box.Font = [Drawing.Font]::new('Consolas', 10)
    $dialog.Controls.Add($box)
    $copy = [Windows.Forms.Button]::new()
    $copy.Text = (UI-Text 'Kopiuj do schowka' 'Copy to clipboard')
    $copy.Location = [Drawing.Point]::new(14, 202)
    $copy.Size = [Drawing.Size]::new(170, 32)
    $copy.Add_Click({ try { [Windows.Forms.Clipboard]::SetText($box.Text) } catch { } })
    $dialog.Controls.Add($copy)
    $close = [Windows.Forms.Button]::new()
    $close.Text = (UI-Text 'Zamknij' 'Close')
    $close.Location = [Drawing.Point]::new(434, 202)
    $close.Size = [Drawing.Size]::new(100, 32)
    $close.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($close)
    $dialog.CancelButton = $close
    [void]$dialog.ShowDialog()
    $dialog.Dispose()
}

function Get-CoopClientFolder {
    $config = Get-LauncherConfig
    $folder = [string]$config.clientRoot
    if (-not $folder -and [string]$config.clientExecutable) { $folder = Split-Path -Parent ([string]$config.clientExecutable) }
    if ($folder -and (Test-Path -LiteralPath $folder -PathType Container)) { return $folder }
    return ''
}

function Show-CoopDialog {
    # Co-op over the Internet (experimental). Hosting and its end restart the
    # game container and so run as actions in the main window; everything else
    # here is quick and in-process, and nothing that shows a password is
    # written to any log.
    if (-not (Get-Command Get-M2CoopNetworkReport -ErrorAction SilentlyContinue)) {
        [Windows.Forms.MessageBox]::Show((UI-Text 'Ta paczka nie ma modułu COOP.' 'This package has no COOP module.'), 'COOP', 'OK', 'Information') | Out-Null
        return
    }
    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = (UI-Text 'COOP - gra ze znajomymi przez internet (eksperymentalne)' 'COOP - playing with friends over the Internet (experimental)')
    $dialog.Size = [Drawing.Size]::new(660, 600)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false

    $tabs = [Windows.Forms.TabControl]::new()
    $tabs.Location = [Drawing.Point]::new(10, 10)
    $tabs.Size = [Drawing.Size]::new(626, 500)
    $dialog.Controls.Add($tabs)
    $hostTab = [Windows.Forms.TabPage]::new()
    $hostTab.Text = (UI-Text 'Hostuję swój świat' 'I host my world')
    $joinTab = [Windows.Forms.TabPage]::new()
    $joinTab.Text = (UI-Text 'Dołączam do znajomego' 'I join a friend')
    $tabs.TabPages.Add($hostTab)
    $tabs.TabPages.Add($joinTab)

    # ------------------------------------------------------------ host tab
    $status = [Windows.Forms.Label]::new()
    $status.Location = [Drawing.Point]::new(12, 10)
    $status.Size = [Drawing.Size]::new(594, 74)
    $status.Font = [Drawing.Font]::new('Segoe UI', 9.5)
    $hostTab.Controls.Add($status)

    $friendsLabel = [Windows.Forms.Label]::new()
    $friendsLabel.Text = (UI-Text 'Znajomi (każdy ma własne konto w Twoim świecie):' 'Friends (each has their own account in your world):')
    $friendsLabel.Location = [Drawing.Point]::new(12, 90)
    $friendsLabel.Size = [Drawing.Size]::new(400, 20)
    $hostTab.Controls.Add($friendsLabel)

    $list = [Windows.Forms.ListView]::new()
    $list.View = 'Details'
    $list.FullRowSelect = $true
    $list.HideSelection = $false
    $list.MultiSelect = $false
    $list.Location = [Drawing.Point]::new(12, 112)
    $list.Size = [Drawing.Size]::new(430, 150)
    [void]$list.Columns.Add((UI-Text 'Znajomy' 'Friend'), 170)
    [void]$list.Columns.Add('Login', 130)
    [void]$list.Columns.Add((UI-Text 'Stan' 'Status'), 110)
    $hostTab.Controls.Add($list)

    # How the world is offered. A host the Internet cannot reach (CGNAT, a
    # second router) is offered through a VPN both players are in; the VPNs
    # are read off this machine's adapters when the window opens.
    $viaLabel = [Windows.Forms.Label]::new()
    $viaLabel.Text = (UI-Text 'Połączenie:' 'Connection:')
    $viaLabel.Location = [Drawing.Point]::new(12, 276)
    $viaLabel.Size = [Drawing.Size]::new(80, 20)
    $hostTab.Controls.Add($viaLabel)
    $viaBox = [Windows.Forms.ComboBox]::new()
    $viaBox.DropDownStyle = 'DropDownList'
    $viaBox.Location = [Drawing.Point]::new(96, 272)
    $viaBox.Size = [Drawing.Size]::new(346, 24)
    $hostTab.Controls.Add($viaBox)
    $viaValues = New-Object System.Collections.Generic.List[string]
    # Auto takes the VPN also when the router opens no port at all
    # (Resolve-M2CoopRouterFallback), which the label has to say.
    [void]$viaBox.Items.Add((UI-Text 'Automatycznie (internet, a gdy router nie da rady - VPN)' 'Automatic (Internet, and a VPN when the router cannot)'))
    $viaValues.Add('auto')
    [void]$viaBox.Items.Add((UI-Text 'Internet (porty w routerze, UPnP)' 'Internet (router ports, UPnP)'))
    $viaValues.Add('internet')
    $viaFound = @()
    try { $viaFound = @(Get-M2CoopVpnAdapters) } catch { $viaFound = @() }
    foreach ($vpn in $viaFound) {
        [void]$viaBox.Items.Add(((UI-Text '{0} - adres {1}' '{0} - address {1}') -f $vpn.Name, $vpn.Address))
        $viaValues.Add([string]$vpn.Kind)
    }
    $viaBox.SelectedIndex = 0
    # The way the world was last hosted, while that VPN is still here.
    try {
        $lastHosting = (Read-M2CoopState -ServerRoot $root).hosting
        if ($lastHosting -and (@($lastHosting.PSObject.Properties.Name) -contains 'vpn')) {
            $lastIndex = $viaValues.IndexOf([string]$lastHosting.vpn)
            if ($lastIndex -ge 2) { $viaBox.SelectedIndex = $lastIndex }
        }
    }
    catch { }

    $addButton = [Windows.Forms.Button]::new()
    $addButton.Text = (UI-Text 'Dodaj znajomego' 'Add a friend')
    $addButton.Location = [Drawing.Point]::new(452, 112)
    $addButton.Size = [Drawing.Size]::new(154, 32)
    $hostTab.Controls.Add($addButton)
    $inviteButton = [Windows.Forms.Button]::new()
    $inviteButton.Text = (UI-Text 'Kod zaproszenia' 'Invite code')
    $inviteButton.Location = [Drawing.Point]::new(452, 150)
    $inviteButton.Size = [Drawing.Size]::new(154, 32)
    $hostTab.Controls.Add($inviteButton)
    $blockButton = [Windows.Forms.Button]::new()
    $blockButton.Text = (UI-Text 'Zablokuj / odblokuj' 'Block / unblock')
    $blockButton.Location = [Drawing.Point]::new(452, 188)
    $blockButton.Size = [Drawing.Size]::new(154, 32)
    $hostTab.Controls.Add($blockButton)
    $secureButton = [Windows.Forms.Button]::new()
    $secureButton.Text = (UI-Text 'Zabezpiecz konta' 'Secure the accounts')
    $secureButton.Location = [Drawing.Point]::new(452, 232)
    $secureButton.Size = [Drawing.Size]::new(154, 32)
    $hostTab.Controls.Add($secureButton)
    $passwordsButton = [Windows.Forms.Button]::new()
    $passwordsButton.Text = (UI-Text 'Moje hasła' 'My passwords')
    $passwordsButton.Location = [Drawing.Point]::new(452, 270)
    $passwordsButton.Size = [Drawing.Size]::new(154, 32)
    $hostTab.Controls.Add($passwordsButton)

    $hostButton = [Windows.Forms.Button]::new()
    $hostButton.Text = (UI-Text 'HOSTUJ ŚWIAT' 'HOST THE WORLD')
    $hostButton.Location = [Drawing.Point]::new(12, 316)
    $hostButton.Size = [Drawing.Size]::new(200, 42)
    $hostButton.BackColor = [Drawing.Color]::FromArgb(27, 150, 88)
    $hostButton.ForeColor = [Drawing.Color]::White
    $hostButton.FlatStyle = 'Flat'
    $hostButton.Font = [Drawing.Font]::new('Segoe UI Semibold', 10)
    $hostTab.Controls.Add($hostButton)
    $stopButtonCoop = [Windows.Forms.Button]::new()
    $stopButtonCoop.Text = (UI-Text 'ZAKOŃCZ HOSTOWANIE' 'STOP HOSTING')
    $stopButtonCoop.Location = [Drawing.Point]::new(222, 316)
    $stopButtonCoop.Size = [Drawing.Size]::new(200, 42)
    $stopButtonCoop.BackColor = [Drawing.Color]::FromArgb(180, 75, 55)
    $stopButtonCoop.ForeColor = [Drawing.Color]::White
    $stopButtonCoop.FlatStyle = 'Flat'
    $stopButtonCoop.Font = [Drawing.Font]::new('Segoe UI Semibold', 10)
    $hostTab.Controls.Add($stopButtonCoop)
    $checkButton = [Windows.Forms.Button]::new()
    $checkButton.Text = (UI-Text 'Sprawdź sieć' 'Check the network')
    $checkButton.Location = [Drawing.Point]::new(452, 316)
    $checkButton.Size = [Drawing.Size]::new(154, 42)
    $hostTab.Controls.Add($checkButton)

    $hostHelp = [Windows.Forms.Label]::new()
    $hostHelp.Text = ((UI-Text 'Kolejność: Zabezpiecz konta, Dodaj znajomego, HOSTUJ ŚWIAT, a potem Kod zaproszenia - ' 'Order: Secure the accounts, Add a friend, HOST THE WORLD, then Invite code - ') +
        (UI-Text 'skopiuj go i wyślij znajomemu w prywatnej wiadomości (zawiera hasło). Hostowanie uruchamia ponownie ' 'copy it and send it to your friend in a private message (it holds the password). Hosting restarts ') +
        (UI-Text 'serwer gry (około minuty) i prosi Windows o zgodę na regułę zapory dla portów 11000 i 13000-13002; ' 'the game server (about a minute) and asks Windows to allow a firewall rule for ports 11000 and 13000-13002; ') +
        (UI-Text 'przez internet otwiera je w routerze (UPnP). Gdy operator nie daje publicznego adresu (CGNAT - częste ' 'over the Internet it opens them in the router (UPnP). When your provider gives no public address (CGNAT - common ') +
        (UI-Text 'w internecie komórkowym), zainstalujcie Radmin VPN albo Tailscale i połączcie się w jednej sieci: ' 'on mobile Internet), both of you install Radmin VPN or Tailscale and join one network: ') +
        (UI-Text 'launcher ją wykryje i hostuje przez nią, bez routera. Ty grasz dalej na serwerze 1, znajomy na serwerze Online.' 'the launcher detects it and hosts through it, without the router. You go on playing on server 1, your friend on the Online server.'))
    $hostHelp.Location = [Drawing.Point]::new(12, 368)
    $hostHelp.Size = [Drawing.Size]::new(594, 96)
    $hostHelp.ForeColor = [Drawing.Color]::DimGray
    $hostTab.Controls.Add($hostHelp)

    # ------------------------------------------------------------ join tab
    $joinInfo = [Windows.Forms.Label]::new()
    $joinInfo.Text = ((UI-Text 'Wklej kod zaproszenia, który dostałeś od znajomego (zaczyna się od M2COOP1:). ' 'Paste the invite code you got from your friend (it begins with M2COOP1:). ') +
        (UI-Text 'Launcher dopisze jego świat do Twojego klienta jako drugi serwer na liście.' 'The launcher adds their world to your client as the second server on the list.'))
    $joinInfo.Location = [Drawing.Point]::new(12, 12)
    $joinInfo.Size = [Drawing.Size]::new(594, 40)
    $joinTab.Controls.Add($joinInfo)
    $codeBox = [Windows.Forms.TextBox]::new()
    $codeBox.Multiline = $true
    $codeBox.WordWrap = $true
    $codeBox.ScrollBars = 'Vertical'
    $codeBox.Location = [Drawing.Point]::new(12, 56)
    $codeBox.Size = [Drawing.Size]::new(594, 110)
    $codeBox.Font = [Drawing.Font]::new('Consolas', 9)
    $joinTab.Controls.Add($codeBox)
    $joinButton = [Windows.Forms.Button]::new()
    $joinButton.Text = (UI-Text 'Zapisz w kliencie' 'Save in the client')
    $joinButton.Location = [Drawing.Point]::new(12, 176)
    $joinButton.Size = [Drawing.Size]::new(200, 36)
    $joinTab.Controls.Add($joinButton)
    $forgetButton = [Windows.Forms.Button]::new()
    $forgetButton.Text = (UI-Text 'Usuń świat znajomego z listy' 'Remove the friend''s world from the list')
    $forgetButton.Location = [Drawing.Point]::new(222, 176)
    $forgetButton.Size = [Drawing.Size]::new(220, 36)
    $joinTab.Controls.Add($forgetButton)
    $joinStatus = [Windows.Forms.Label]::new()
    $joinStatus.Location = [Drawing.Point]::new(12, 224)
    $joinStatus.Size = [Drawing.Size]::new(594, 120)
    $joinStatus.Font = [Drawing.Font]::new('Segoe UI', 9.5)
    $joinTab.Controls.Add($joinStatus)

    $closeButton = [Windows.Forms.Button]::new()
    $closeButton.Text = (UI-Text 'Zamknij' 'Close')
    $closeButton.Location = [Drawing.Point]::new(536, 518)
    $closeButton.Size = [Drawing.Size]::new(100, 32)
    $closeButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($closeButton)
    $dialog.CancelButton = $closeButton

    $refresh = {
        $state = Read-M2CoopState -ServerRoot $root
        $bindings = Get-M2CoopGameBindings -ServerRoot $root
        $lines = @()
        $ruleText = $(if (Test-M2CoopFirewallRule) { (UI-Text 'reguła zapory jest' 'the firewall rule is there') } else { (UI-Text 'BRAK reguły zapory - HOSTUJ ŚWIAT o nią poprosi' 'NO firewall rule - HOST THE WORLD will ask for it') })
        if (-not $bindings.Running) { $lines += (UI-Text 'Serwer gry: nie działa - najpierw GRAJ.' 'Game server: not running - PLAY first.') }
        elseif ($bindings.Public) { $lines += ((UI-Text 'Hostowanie: WŁĄCZONE - porty gry są otwarte dla sieci ({0}).' 'Hosting: ON - the game ports are open to the network ({0}).') -f $ruleText) }
        else { $lines += (UI-Text 'Hostowanie: wyłączone - porty gry słuchają tylko na tym komputerze.' 'Hosting: off - the game ports listen on this computer only.') }
        $public = ''; $viaName = ''
        if ($state.hosting) {
            $fields = @($state.hosting.PSObject.Properties.Name)
            if (($fields -contains 'friendAddress') -and [string]$state.hosting.friendAddress) { $public = [string]$state.hosting.friendAddress }
            elseif (($fields -contains 'publicAddress') -and [string]$state.hosting.publicAddress) { $public = [string]$state.hosting.publicAddress }
            if (($fields -contains 'mode') -and [string]$state.hosting.mode -eq 'vpn' -and ($fields -contains 'vpnName')) { $viaName = [string]$state.hosting.vpnName }
        }
        if ($public -and $viaName) { $lines += ((UI-Text 'Adres dla znajomych (ostatnio): {0}, przez {1}' 'Address for friends (last time): {0}, through {1}') -f $public, $viaName) }
        elseif ($public) { $lines += ((UI-Text 'Adres dla znajomych (ostatnio): {0}' 'Address for friends (last time): {0}') -f $public) }
        $defaults = @()
        try { $defaults = @(Get-M2CoopDefaultPasswordAccounts -ServerRoot $root) }
        catch { $lines += (UI-Text 'Baza nie odpowiada - uruchom serwer (GRAJ).' 'The database is not answering - start the server (PLAY).') }
        if ($defaults.Count -gt 0) { $lines += ((UI-Text 'UWAGA: konta {0} mają hasła z paczki - kliknij Zabezpiecz konta.' 'WARNING: the accounts {0} have the package''s passwords - click Secure the accounts.') -f ($defaults -join ', ')) }
        else { $lines += (UI-Text 'Konta admin i test: hasła zmienione.' 'The admin and test accounts: passwords changed.') }
        $status.Text = ($lines -join [Environment]::NewLine)
        $status.ForeColor = $(if ($defaults.Count -gt 0) { [Drawing.Color]::DarkRed } else { [Drawing.Color]::Black })
        $list.Items.Clear()
        foreach ($f in @($state.friends)) {
            $item = [Windows.Forms.ListViewItem]::new([string]$f.name)
            [void]$item.SubItems.Add([string]$f.login)
            [void]$item.SubItems.Add($(if ($f.blocked) { (UI-Text 'zablokowany' 'blocked') } else { (UI-Text 'aktywny' 'active') }))
            $item.Tag = [string]$f.login
            [void]$list.Items.Add($item)
        }
        $client = Get-CoopClientFolder
        $cfg = $(if ($client) { Join-Path $client 'coop.cfg' } else { '' })
        if ($cfg -and (Test-Path -LiteralPath $cfg -PathType Leaf)) {
            $text = [IO.File]::ReadAllText($cfg)
            $name = ''; $hostName = ''
            if ($text -match '(?m)^name=(.*)$') { $name = $Matches[1].Trim() }
            if ($text -match '(?m)^host=(.*)$') { $hostName = $Matches[1].Trim() }
            $joinStatus.Text = ((UI-Text "W Twoim kliencie jest świat znajomego: {0} ({1}).`r`nW kliencie wybierz serwer 'Online: {0}'." "Your client has a friend's world: {0} ({1}).`r`nIn the client choose the server 'Online: {0}'.") -f $name, $hostName)
            $joinVpn = Get-M2CoopVpnProduct -Kind (Get-M2CoopVpnKindForAddress $hostName)
            if ($joinVpn) { $joinStatus.Text += ((UI-Text "`r`nTen adres jest w sieci {0} - gra połączy się, gdy {0} jest włączony i jesteś w sieci znajomego." "`r`nThis address is in the {0} network - the game connects when {0} is on and you are in your friend's network.") -f $joinVpn.Name) }
        }
        elseif ($client) { $joinStatus.Text = (UI-Text 'W Twoim kliencie nie ma jeszcze świata znajomego.' 'Your client has no friend''s world yet.') }
        else { $joinStatus.Text = (UI-Text 'Nie wiem, gdzie jest klient - wskaż go przyciskiem WYBIERZ KLIENTA w oknie launchera.' 'I do not know where the client is - point to it with the CHOOSE CLIENT button in the launcher window.') }
    }

    $selectedFriend = {
        if ($list.SelectedItems.Count -eq 0) {
            [Windows.Forms.MessageBox]::Show((UI-Text 'Zaznacz znajomego na liście.' 'Select a friend on the list.'), 'COOP', 'OK', 'Information') | Out-Null
            return $null
        }
        $login = [string]$list.SelectedItems[0].Tag
        foreach ($f in @((Read-M2CoopState -ServerRoot $root).friends)) { if ([string]$f.login -eq $login) { return $f } }
        return $null
    }

    $addButton.Add_Click({
        $name = [Microsoft.VisualBasic.Interaction]::InputBox((UI-Text 'Imię albo nick znajomego (z niego powstanie login):' 'Your friend''s name or nick (the login is made from it):'), (UI-Text 'Dodaj znajomego' 'Add a friend'), '')
        if (-not $name) { return }
        try {
            $friend = New-M2CoopFriend -ServerRoot $root -Name $name
            Write-LocalLog ((UI-Text "COOP: dodano konto znajomego, login {0}." "COOP: a friend's account added, login {0}.") -f $friend.login)
            & $refresh
            Show-CoopSecretDialog -Title (UI-Text 'Nowy znajomy' 'New friend') `
                -Intro ((UI-Text "Konto gotowe. Login i hasło są też w kodzie zaproszenia - wyślij znajomemu kod (przycisk Kod zaproszenia) po włączeniu hostowania." "The account is ready. The login and password are also in the invite code - send your friend the code (the Invite code button) once hosting is on.")) `
                -Secret ((UI-Text "Login: {0}`r`nHasło: {1}`r`nKod usuwania postaci: {2}" "Login: {0}`r`nPassword: {1}`r`nCharacter deletion code: {2}") -f $friend.login, $friend.password, $friend.socialId)
        }
        catch { [Windows.Forms.MessageBox]::Show($_.Exception.Message, 'COOP', 'OK', 'Error') | Out-Null }
    })

    $inviteButton.Add_Click({
        $friend = & $selectedFriend
        if (-not $friend) { return }
        if ($friend.blocked) { [Windows.Forms.MessageBox]::Show((UI-Text 'To konto jest zablokowane - najpierw je odblokuj.' 'This account is blocked - unblock it first.'), 'COOP', 'OK', 'Information') | Out-Null; return }
        $dialog.Cursor = [Windows.Forms.Cursors]::WaitCursor
        $target = Get-M2CoopInviteTarget -ServerRoot $root
        $dialog.Cursor = [Windows.Forms.Cursors]::Default
        if (-not $target.Address) {
            $why = $(if ($target.Vpn) { ((UI-Text 'Nie udało się odczytać Twojego adresu w {0} - uruchom go i spróbuj jeszcze raz.' 'Could not read your address in {0} - start it and try again.') -f $target.VpnName) } else { (UI-Text 'Nie udało się odczytać Twojego adresu w internecie.' 'Could not read your address on the Internet.') })
            [Windows.Forms.MessageBox]::Show($why, 'COOP', 'OK', 'Warning') | Out-Null
            return
        }
        $code = Get-M2CoopFriendInvite -ServerRoot $root -Friend $friend -HostAddress $target.Address -Vpn $target.Vpn -Lan $target.Lan
        try { [Windows.Forms.Clipboard]::SetText($code) } catch { }
        Write-LocalLog ((UI-Text "COOP: skopiowano kod zaproszenia dla loginu {0}." "COOP: the invite code copied for the login {0}.") -f $friend.login)
        $intro = (UI-Text 'Kod jest już w schowku. Wyślij go znajomemu w prywatnej wiadomości - zawiera jego hasło. Znajomy wkleja go w swoim launcherze (przycisk COOP) albo w pliku Dolacz.bat w folderze klienta.' 'The code is already in the clipboard. Send it to your friend in a private message - it holds their password. Your friend pastes it in their launcher (the COOP button) or into Dolacz.bat in the client folder.')
        if ($target.Vpn) { $intro = ((UI-Text 'Kod jest już w schowku i prowadzi na Twój adres w {0}: znajomy musi najpierw dołączyć do Twojej sieci {0}. Wyślij mu kod w prywatnej wiadomości - zawiera jego hasło.' 'The code is already in the clipboard and leads to your address in {0}: your friend must first join your {0} network. Send them the code in a private message - it holds their password.') -f $target.VpnName) }
        Show-CoopSecretDialog -Title ((UI-Text 'Kod zaproszenia - ' 'Invite code - ') + [string]$friend.name) -Intro $intro -Secret $code
    })

    $blockButton.Add_Click({
        $friend = & $selectedFriend
        if (-not $friend) { return }
        try {
            Set-M2CoopFriendBlocked -ServerRoot $root -Login ([string]$friend.login) -Blocked (-not [bool]$friend.blocked)
            Write-LocalLog ((UI-Text "COOP: konto {0} {1}." "COOP: account {0} {1}.") -f $friend.login, $(if ($friend.blocked) { (UI-Text 'odblokowane' 'unblocked') } else { (UI-Text 'zablokowane' 'blocked') }))
            & $refresh
        }
        catch { [Windows.Forms.MessageBox]::Show($_.Exception.Message, 'COOP', 'OK', 'Error') | Out-Null }
    })

    $secureButton.Add_Click({
        try {
            $changed = Protect-M2CoopAccounts -ServerRoot $root
            $names = @($changed.PSObject.Properties | ForEach-Object { $_.Name })
            & $refresh
            if ($names.Count -eq 0) {
                [Windows.Forms.MessageBox]::Show((UI-Text 'Konta admin i test nie mają już haseł z paczki.' 'The admin and test accounts no longer have the package''s passwords.'), 'COOP', 'OK', 'Information') | Out-Null
                return
            }
            Write-LocalLog ((UI-Text "COOP: zmieniono hasła kont {0}." "COOP: the passwords of the accounts {0} changed.") -f ($names -join ', '))
            $text = ($names | ForEach-Object { (UI-Text "Konto {0}: hasło {1}" "Account {0}, password: {1}") -f $_, $changed.$_ }) -join "`r`n"
            Show-CoopSecretDialog -Title (UI-Text 'Nowe hasła' 'New passwords') `
                -Intro (UI-Text 'Od teraz logujesz się na te konta tymi hasłami (w kliencie wpisz je zamiast starych). Launcher je pamięta - przycisk Moje hasła.' 'From now on you log in to these accounts with these passwords (type them in the client instead of the old ones). The launcher remembers them - the My passwords button.') `
                -Secret $text
        }
        catch { [Windows.Forms.MessageBox]::Show($_.Exception.Message, 'COOP', 'OK', 'Error') | Out-Null }
    })

    $passwordsButton.Add_Click({
        $state = Read-M2CoopState -ServerRoot $root
        $lines = @()
        if ($state.PSObject.Properties.Name -contains 'accounts' -and $state.accounts) {
            foreach ($p in $state.accounts.PSObject.Properties) { $lines += ((UI-Text "Konto {0}: hasło {1}" "Account {0}, password: {1}") -f $p.Name, $p.Value) }
        }
        foreach ($f in @($state.friends)) { $lines += ((UI-Text "Znajomy {0}: login {1}, hasło {2}" "Friend {0}: login {1}, password: {2}") -f $f.name, $f.login, $f.password) }
        if ($lines.Count -eq 0) { $lines += (UI-Text 'Launcher nie zmieniał jeszcze żadnego hasła.' 'The launcher has not changed any password yet.') }
        Show-CoopSecretDialog -Title (UI-Text 'Hasła COOP' 'COOP passwords') -Intro (UI-Text 'Hasła kont zmienionych i założonych przez okno COOP.' 'Passwords of the accounts changed and created in the COOP window.') -Secret ($lines -join "`r`n")
    })

    $hostButton.Add_Click({
        try { $defaults = @(Get-M2CoopDefaultPasswordAccounts -ServerRoot $root) }
        catch { [Windows.Forms.MessageBox]::Show((UI-Text 'Baza nie odpowiada - uruchom najpierw serwer (GRAJ).' 'The database is not answering - start the server first (PLAY).'), 'COOP', 'OK', 'Warning') | Out-Null; return }
        if ($defaults.Count -gt 0) {
            [Windows.Forms.MessageBox]::Show(((UI-Text 'Konta {0} mają hasła z paczki - każdy w internecie mógłby się na nie zalogować. Najpierw kliknij Zabezpiecz konta.' 'The accounts {0} have the package''s passwords - anybody on the Internet could log in to them. Click Secure the accounts first.') -f ($defaults -join ', ')), 'COOP', 'OK', 'Warning') | Out-Null
            return
        }
        $via = $viaValues[[Math]::Max(0, $viaBox.SelectedIndex)]
        $routerLine = (UI-Text "- otworzy te porty w routerze (UPnP), a gdy operator nie daje publicznego adresu (CGNAT), użyje VPN, jeśli go wykryje." "- open these ports in the router (UPnP), and when your provider gives no public address (CGNAT), use a VPN if it finds one.")
        if ($via -eq 'internet') { $routerLine = (UI-Text '- otworzy te porty w routerze (UPnP).' '- open these ports in the router (UPnP).') }
        elseif ($via -ne 'auto') {
            $product = Get-M2CoopVpnProduct -Kind $via
            $routerLine = ((UI-Text "- niczego nie otwiera w routerze: znajomi łączą się przez {0}, w Twojej sieci." "- open nothing in the router: friends connect through {0}, in your network.") -f $(if ($product) { $product.Name } else { 'VPN' }))
        }
        $answer = [Windows.Forms.MessageBox]::Show(
            ((UI-Text "Hostowanie:`r`n- uruchomi ponownie serwer gry (około minuty) - wyloguj się z gry,`r`n" "Hosting will:`r`n- restart the game server (about a minute) - log out of the game,`r`n") +
             (UI-Text "- poprosi Windows o zgodę na regułę zapory dla portów gry (wybierz Tak),`r`n" "- ask Windows to allow a firewall rule for the game ports (choose Yes),`r`n") +
             $routerLine + (UI-Text "`r`n`r`nKontynuować?" "`r`n`r`nContinue?")), (UI-Text 'Hostuj świat' 'Host the world'), 'YesNo', 'Question')
        if ($answer -ne [Windows.Forms.DialogResult]::Yes) { return }
        # The rule is asked for here, by the window the player just clicked:
        # Windows puts the question of a process in front on the screen, and
        # the question of the hidden action only on the taskbar, where
        # xXxDaronxXx's went unanswered twice (24 September).
        if (-not (Test-M2CoopFirewallRule)) {
            $dialog.Cursor = [Windows.Forms.Cursors]::WaitCursor
            $ruleAdded = $false
            try { $ruleAdded = [bool](Add-M2CoopFirewallRule -Ports (Get-M2CoopGamePorts -ServerRoot $root)) } catch { $ruleAdded = $false }
            $dialog.Cursor = [Windows.Forms.Cursors]::Default
            Write-LocalLog ((UI-Text "COOP: regula zapory {0}." "COOP: firewall rule {0}.") -f $(if ($ruleAdded) { (UI-Text 'dodana z okna' 'added from the window') } else { (UI-Text 'nie dodana (odmowa zgody)' 'not added (permission refused)') }))
        }
        $dialog.Close()
        Start-LauncherAction -Action 'CoopHost' -Yes -ExtraArgs @('-CoopVia', $via, '-CoopFirewallAsked')
    })

    $stopButtonCoop.Add_Click({
        $answer = [Windows.Forms.MessageBox]::Show(
            (UI-Text "Zakończenie hostowania zamknie porty w routerze i uruchomi ponownie serwer gry (około minuty). Znajomi zostaną rozłączeni.`r`n`r`nKontynuować?" "Stopping hosting closes the ports in the router and restarts the game server (about a minute). Your friends will be disconnected.`r`n`r`nContinue?"),
            (UI-Text 'Zakończ hostowanie' 'Stop hosting'), 'YesNo', 'Question')
        if ($answer -ne [Windows.Forms.DialogResult]::Yes) { return }
        $dialog.Close()
        Start-LauncherAction -Action 'CoopStop' -Yes
    })

    $checkButton.Add_Click({
        $dialog.Close()
        Start-LauncherAction -Action 'CoopCheck'
    })

    $joinButton.Add_Click({
        try {
            $invite = Read-M2CoopInvite -Code $codeBox.Text
            $client = Get-CoopClientFolder
            if (-not $client) { throw (UI-Text 'Nie wiem, gdzie jest klient - wskaż go przyciskiem WYBIERZ KLIENTA w oknie launchera.' 'I do not know where the client is - point to it with the CHOOSE CLIENT button in the launcher window.') }
            # What this machine can tell before the client is started: the
            # host's home address answers here (the same house), a VPN world
            # needs that VPN here, and the world's auth either answers from
            # here or it does not (not hosting right now, or no path).
            $dialog.Cursor = [Windows.Forms.Cursors]::WaitCursor
            $choice = Resolve-M2CoopJoinHost -Invite $invite
            $advice = $(if ($choice.Lan) { '' } else { Get-M2CoopJoinAdvice -Invite $invite })
            $dialog.Cursor = [Windows.Forms.Cursors]::Default
            $path = Write-M2CoopClientConfig -ClientFolder $client -Invite $invite -HostAddress $choice.Host
            Write-LocalLog ((UI-Text "COOP: zapisano swiat znajomego w {0} (adres {1}{2})." "COOP: the friend's world saved in {0} (address {1}{2}).") -f $path, $choice.Host, $(if ($choice.Lan) { (UI-Text ', siec domowa' ', home network') } else { '' }))
            try { [Windows.Forms.Clipboard]::SetText([string]$invite.password) } catch { }
            $codeBox.Text = ''
            & $refresh
            Write-LocalLog ((UI-Text "COOP: serwer znajomego {0}." "COOP: the friend's server {0}.") -f $(if ($choice.Answers) { (UI-Text 'odpowiada' 'answers') } else { (UI-Text 'nie odpowiada' 'does not answer') }))
            $intro = ((UI-Text "Uruchom klienta i wybierz serwer 'Online: {0}'. Hasło jest w schowku." "Start the client and choose the server 'Online: {0}'. The password is in the clipboard.") -f $invite.name)
            if ($advice) { $intro = $advice + ' ' + $intro }
            else { $intro += ' ' + (@(Get-M2CoopJoinNotes -Choice $choice) -join ' ') }
            # A client exe from before 2.0.17 cannot enter a friend's world at
            # all (Test-M2CoopClientExeOld): said first, because nothing else
            # here helps until it is replaced.
            if (Test-M2CoopClientExeOld -ClientFolder $client) {
                Write-LocalLog (UI-Text 'COOP: klient ma stary metin2client.exe (sprzed 2.0.17).' 'COOP: the client has an old metin2client.exe (from before 2.0.17).')
                $intro = (Get-M2CoopOldClientNote) + ' ' + $intro
            }
            Show-CoopSecretDialog -Title (UI-Text 'Świat znajomego dodany' 'Friend''s world added') -Intro $intro `
                -Secret ((UI-Text "Login: {0}`r`nHasło: {1}" "Login: {0}`r`nPassword: {1}") -f $invite.login, $invite.password)
        }
        catch { [Windows.Forms.MessageBox]::Show($_.Exception.Message, 'COOP', 'OK', 'Error') | Out-Null }
    })

    $forgetButton.Add_Click({
        $client = Get-CoopClientFolder
        if (-not $client) { return }
        $cfg = Join-Path $client 'coop.cfg'
        if (Test-Path -LiteralPath $cfg -PathType Leaf) {
            [IO.File]::Delete($cfg)
            Write-LocalLog (UI-Text 'COOP: usunieto swiat znajomego z klienta.' 'COOP: the friend''s world removed from the client.')
        }
        & $refresh
    })

    try { & $refresh } catch { $status.Text = (UI-Text "Nie udało się odczytać stanu: $($_.Exception.Message)" "Could not read the state: $($_.Exception.Message)") }
    [void]$dialog.ShowDialog()
    $dialog.Dispose()
}

function Show-VpsDialog {
    # This world on a rented Linux VPS (launcher\Metin2Launcher.Vps.psm1).
    # The key, the check, the tunnel, the client entry and anything that shows
    # a password are quick and in-process; the install, the update, the status
    # and the logs run as actions in the main window, whose log shows their
    # progress. No password is written to any log. Installing, updating, the
    # panels and the invite codes are for everybody, like every COOP invite.
    if (-not (Get-Command Install-M2Vps -ErrorAction SilentlyContinue)) {
        [Windows.Forms.MessageBox]::Show((UI-Text 'Ta paczka nie ma modułu VPS.' 'This package has no VPS module.'), 'VPS', 'OK', 'Information') | Out-Null
        return
    }
    $state = Get-M2VpsState -ServerRoot $root
    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = (UI-Text 'Serwer na VPS - ten świat na wynajętym serwerze Linux' 'Server on a VPS - this world on a rented Linux server')
    $dialog.Size = [Drawing.Size]::new(700, 640)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false

    $intro = [Windows.Forms.Label]::new()
    $intro.Text = ((UI-Text 'Launcher wyśle folder serwera na VPS przez SSH i zainstaluje go tam: Docker, plik wymiany, .env z losowymi hasłami, ' 'The launcher sends the server folder to the VPS over SSH and installs it there: Docker, a swap file, an .env with random passwords, ') +
        (UI-Text 'budowa w tle (pierwszy raz 15-40 minut) i nowe hasła kont admin i test. Potrzebny VPS z Debian 12/13 albo Ubuntu 22.04/24.04, ' 'a build in the background (15-40 minutes the first time) and new passwords for the admin and test accounts. It needs a VPS with Debian 12/13 or Ubuntu 22.04/24.04, ') +
        (UI-Text 'procesor x86_64 (Intel/AMD - nie ARM), 8 GB RAM (minimum 4 GB, wtedy mniej botów) i 60-80 GB dysku. ' 'an x86_64 processor (Intel/AMD - not ARM), 8 GB RAM (at least 4 GB, then fewer bots) and 60-80 GB of disk. ') +
        (UI-Text 'Hasło do VPS wpisujesz raz, w oknie ssh - potem launcher łączy się kluczem.' 'You type the VPS password once, in the ssh window - then the launcher connects with a key.'))
    $intro.Location = [Drawing.Point]::new(14, 10)
    $intro.Size = [Drawing.Size]::new(660, 76)
    $dialog.Controls.Add($intro)

    $fields = @{}
    $row = 94
    foreach ($field in @(
            @('host', (UI-Text 'Adres VPS (IPv4 albo domena):' 'VPS address (IPv4 or domain):'), [string]$state.host, 250),
            @('user', (UI-Text 'Użytkownik (root, debian, ubuntu...):' 'User (root, debian, ubuntu...):'), [string]$state.user, 160),
            @('port', (UI-Text 'Port SSH:' 'SSH port:'), [string]$state.port, 70),
            @('remoteDir', (UI-Text 'Folder na VPS:' 'Folder on the VPS:'), [string]$state.remoteDir, 250))) {
        $label = [Windows.Forms.Label]::new()
        $label.Text = $field[1]
        $label.Location = [Drawing.Point]::new(14, $row + 3)
        $label.Size = [Drawing.Size]::new(230, 20)
        $dialog.Controls.Add($label)
        $box = [Windows.Forms.TextBox]::new()
        $box.Text = $field[2]
        $box.Location = [Drawing.Point]::new(250, $row)
        $box.Size = [Drawing.Size]::new([int]$field[3], 24)
        $box.Font = [Drawing.Font]::new('Consolas', 10)
        $dialog.Controls.Add($box)
        $fields[$field[0]] = $box
        $row += 30
    }

    $buttons = @{}
    $grid = @(
        @('connect', (UI-Text '1. POŁĄCZ (KLUCZ SSH)' '1. CONNECT (SSH KEY)'), 0, 0, '#2D6EBE'), @('check', (UI-Text 'SPRAWDŹ VPS' 'CHECK VPS'), 1, 0, ''), @('install', (UI-Text '2. ZAINSTALUJ NA VPS' '2. INSTALL ON VPS'), 2, 0, '#1B9658'),
        @('update', (UI-Text 'AKTUALIZUJ VPS' 'UPDATE VPS'), 0, 1, ''), @('status', (UI-Text 'STAN VPS' 'VPS STATUS'), 1, 1, ''), @('logs', (UI-Text 'LOGI VPS' 'VPS LOGS'), 2, 1, ''),
        @('panel', (UI-Text 'OTWÓRZ PANEL (TUNEL SSH)' 'OPEN PANEL (SSH TUNNEL)'), 0, 2, '#B47D23'), @('tunnelClose', (UI-Text 'ZAMKNIJ TUNEL' 'CLOSE TUNNEL'), 1, 2, ''), @('passwords', (UI-Text 'HASŁA KONT' 'ACCOUNT PASSWORDS'), 2, 2, ''),
        @('client', (UI-Text 'DOPISZ DO KLIENTA GRY' 'ADD TO GAME CLIENT'), 0, 3, ''), @('invite', (UI-Text 'KOD DLA ZNAJOMEGO (COOP)' 'CODE FOR A FRIEND (CO-OP)'), 1, 3, '#28788F'))
    foreach ($spec in $grid) {
        $button = [Windows.Forms.Button]::new()
        $button.Text = $spec[1]
        $button.Location = [Drawing.Point]::new(14 + 222 * [int]$spec[2], 222 + 46 * [int]$spec[3])
        $button.Size = [Drawing.Size]::new(212, 38)
        $button.Font = [Drawing.Font]::new('Segoe UI Semibold', 9)
        if ($spec[4]) {
            $button.BackColor = [Drawing.ColorTranslator]::FromHtml($spec[4])
            $button.ForeColor = [Drawing.Color]::White
            $button.FlatStyle = 'Flat'
        }
        $dialog.Controls.Add($button)
        $buttons[$spec[0]] = $button
    }

    $help = [Windows.Forms.Label]::new()
    $help.Text = ((UI-Text 'Kolejność: POŁĄCZ, SPRAWDŹ VPS, ZAINSTALUJ. Gracze łączą się z adresem VPS (porty 11000 i 13000-13002 muszą być otwarte ' 'Order: CONNECT, CHECK VPS, INSTALL. Players connect to the VPS address (ports 11000 and 13000-13002 must be open ') +
        (UI-Text 'w zaporze dostawcy VPS, jeśli ją ma). Panele WWW słuchają tylko na VPS - otwiera je tunel SSH (przycisk OTWÓRZ PANEL). ' 'in the VPS provider''s firewall, if it has one). The web panels listen on the VPS only - an SSH tunnel opens them (the OPEN PANEL button). ') +
        (UI-Text 'DOPISZ DO KLIENTA dodaje VPS jako drugi serwer na liście w Twoim kliencie. KOD DLA ZNAJOMEGO robi zaproszenie do świata na VPS.' 'ADD TO GAME CLIENT adds the VPS as the second server on the list in your client. CODE FOR A FRIEND makes an invite to the world on the VPS.'))
    $help.Location = [Drawing.Point]::new(14, 412)
    $help.Size = [Drawing.Size]::new(660, 62)
    $help.ForeColor = [Drawing.Color]::DimGray
    $dialog.Controls.Add($help)

    $status = [Windows.Forms.Label]::new()
    $status.Location = [Drawing.Point]::new(14, 480)
    $status.Size = [Drawing.Size]::new(660, 64)
    $status.Font = [Drawing.Font]::new('Segoe UI', 9.5)
    $dialog.Controls.Add($status)

    $closeButton = [Windows.Forms.Button]::new()
    $closeButton.Text = (UI-Text 'Zamknij' 'Close')
    $closeButton.Location = [Drawing.Point]::new(574, 556)
    $closeButton.Size = [Drawing.Size]::new(100, 32)
    $closeButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($closeButton)
    $dialog.CancelButton = $closeButton

    $showStatus = {
        $lines = @()
        $current = Get-M2VpsState -ServerRoot $root
        if ($current.host) { $lines += ('VPS: {0}@{1}, folder {2}.' -f $current.user, $current.host, $current.remoteDir) }
        else { $lines += (UI-Text 'Wpisz adres VPS i użytkownika, potem POŁĄCZ.' 'Enter the VPS address and the user, then CONNECT.') }
        if ($current.lastInstall) { $lines += ((UI-Text 'Ostatnia instalacja z tego launchera: {0} (wersja {1}).' 'Last install from this launcher: {0} (version {1}).') -f $current.lastInstall, $current.lastVersion) }
        $tunnel = Get-M2VpsTunnelProcess -State $current
        if ($tunnel -and @($current.tunnelPorts).Count -gt 0) {
            $lines += ((UI-Text 'Tunel do paneli otwarty: http://127.0.0.1:{0}/map' 'Tunnel to the panels open: http://127.0.0.1:{0}/map') -f @($current.tunnelPorts)[0].local)
        }
        $status.Text = ($lines -join [Environment]::NewLine)
    }

    $saveFields = {
        # What the fields say, checked and saved; $null and a message when
        # something is wrong. Only here does the window change the state.
        $next = Get-M2VpsState -ServerRoot $root
        $next.host = $fields['host'].Text.Trim()
        $next.user = $fields['user'].Text.Trim()
        $next.remoteDir = $fields['remoteDir'].Text.Trim()
        $portNumber = 0
        if ([int]::TryParse($fields['port'].Text.Trim(), [ref]$portNumber)) { $next.port = $portNumber } else { $next.port = 0 }
        try { Assert-M2VpsState -State $next }
        catch {
            [Windows.Forms.MessageBox]::Show($_.Exception.Message, 'VPS', 'OK', 'Warning') | Out-Null
            return $null
        }
        Save-M2VpsState -ServerRoot $root -State $next
        return $next
    }

    $runQuick = {
        # A quick in-process call with the wait cursor; its error in a box.
        param([scriptblock]$Work)
        $dialog.Cursor = [Windows.Forms.Cursors]::WaitCursor
        try { & $Work }
        catch { [Windows.Forms.MessageBox]::Show($_.Exception.Message, 'VPS', 'OK', 'Error') | Out-Null }
        finally { $dialog.Cursor = [Windows.Forms.Cursors]::Default; try { & $showStatus } catch { } }
    }

    $buttons['connect'].Add_Click({
        $vps = & $saveFields
        if (-not $vps) { return }
        [Windows.Forms.MessageBox]::Show(((UI-Text "Otworzy się czarne okno ssh. Wpisz w nim hasło do VPS ({0}@{1}) i naciśnij Enter - znaków nie widać.`r`n`r`nTo jedyny raz: ssh doda klucz tego launchera na VPS i potem launcher łączy się bez hasła. Launcher tego hasła nie widzi i nigdzie go nie zapisuje." "A black ssh window opens. Type the VPS password ({0}@{1}) in it and press Enter - the characters do not show.`r`n`r`nThis is the only time: ssh adds this launcher's key on the VPS, and then the launcher connects without a password. The launcher does not see this password and saves it nowhere.") -f $vps.user, $vps.host),
            (UI-Text 'Połączenie z VPS' 'Connecting to the VPS'), 'OK', 'Information') | Out-Null
        & $runQuick {
            $works = Install-M2VpsKey -State $vps
            Write-LocalLog ((UI-Text 'VPS: klucz SSH dla {0}@{1} {2}.' 'VPS: SSH key for {0}@{1} {2}.') -f $vps.user, $vps.host, $(if ($works) { (UI-Text 'dziala' 'works') } else { (UI-Text 'NIE dziala' 'does NOT work') }))
            if ($works) { [Windows.Forms.MessageBox]::Show((UI-Text 'Klucz działa - launcher łączy się z VPS bez hasła. Teraz SPRAWDŹ VPS.' 'The key works - the launcher connects to the VPS without a password. Now CHECK VPS.'), 'VPS', 'OK', 'Information') | Out-Null }
            else { [Windows.Forms.MessageBox]::Show((UI-Text 'Klucz nie działa - sprawdź adres, użytkownika i hasło, i spróbuj jeszcze raz.' 'The key does not work - check the address, the user and the password, and try again.'), 'VPS', 'OK', 'Warning') | Out-Null }
        }
    })

    $buttons['check'].Add_Click({
        $vps = & $saveFields
        if (-not $vps) { return }
        & $runQuick {
            $machine = Test-M2VpsMachine -State $vps
            $report = (Format-M2VpsMachineReport -Machine $machine) -join "`r`n"
            Write-LocalLog ((UI-Text 'VPS: sprawdzenie {0} - {1}.' 'VPS: check of {0} - {1}.') -f $vps.host, $(if ($machine.Verdict.Ok) { (UI-Text 'mozna instalowac' 'ready to install') } else { (UI-Text 'nie spelnia wymagan' 'does not meet the requirements') }))
            [Windows.Forms.MessageBox]::Show($report, (UI-Text 'Sprawdzenie VPS' 'VPS check'), 'OK', $(if ($machine.Verdict.Ok) { 'Information' } else { 'Warning' })) | Out-Null
        }
    })

    $buttons['install'].Add_Click({
        $vps = & $saveFields
        if (-not $vps) { return }
        $answer = [Windows.Forms.MessageBox]::Show(
            ((UI-Text "Zainstalować ten świat na VPS {0}?`r`n`r`n- folder serwera pójdzie na VPS (około 100 MB, bez .env, kopii, logów i klienta),`r`n- VPS dostanie Dockera, plik wymiany (przy małej pamięci) i swój .env z losowymi hasłami,`r`n- pierwsza budowa trwa tam 15-40 minut - postęp w logu launchera; zamknięcie launchera jej nie przerywa,`r`n- konta admin i test dostaną nowe hasła (przycisk HASŁA KONT).`r`n`r`nKontynuować?" "Install this world on the VPS {0}?`r`n`r`n- the server folder goes to the VPS (about 100 MB, without .env, backups, logs and the client),`r`n- the VPS gets Docker, a swap file (with little memory) and its own .env with random passwords,`r`n- the first build there takes 15-40 minutes - progress in the launcher log; closing the launcher does not stop it,`r`n- the admin and test accounts get new passwords (the ACCOUNT PASSWORDS button).`r`n`r`nContinue?") -f $vps.host),
            (UI-Text 'Zainstaluj na VPS' 'Install on the VPS'), 'YesNo', 'Question')
        if ($answer -ne [Windows.Forms.DialogResult]::Yes) { return }
        $dialog.Close()
        Start-LauncherAction -Action 'VpsInstall' -Yes
    })

    $buttons['update'].Add_Click({
        $vps = & $saveFields
        if (-not $vps) { return }
        $answer = [Windows.Forms.MessageBox]::Show(
            ((UI-Text "Zaktualizować serwer na VPS {0} do najnowszej wersji z GitHuba?`r`n`r`nAktualizacja pobiera paczkę na VPS i przebudowuje serwer w tle; postacie i boty zostają. Postęp w logu launchera." "Update the server on the VPS {0} to the newest version from GitHub?`r`n`r`nThe update downloads the package onto the VPS and rebuilds the server in the background; characters and bots stay. Progress in the launcher log.") -f $vps.host),
            (UI-Text 'Aktualizuj VPS' 'Update the VPS'), 'YesNo', 'Question')
        if ($answer -ne [Windows.Forms.DialogResult]::Yes) { return }
        $dialog.Close()
        Start-LauncherAction -Action 'VpsUpdate' -Yes
    })

    $buttons['status'].Add_Click({
        if (-not (& $saveFields)) { return }
        $dialog.Close()
        Start-LauncherAction -Action 'VpsStatus'
    })

    $buttons['logs'].Add_Click({
        if (-not (& $saveFields)) { return }
        $dialog.Close()
        Start-LauncherAction -Action 'VpsLogs'
    })

    $buttons['panel'].Add_Click({
        $vps = & $saveFields
        if (-not $vps) { return }
        $addresses = $null
        & $runQuick { $script:vpsPanelAddresses = Open-M2VpsPanel -State $vps -ServerRoot $root }
        $addresses = $script:vpsPanelAddresses
        $script:vpsPanelAddresses = $null
        if (-not $addresses) { return }
        Write-LocalLog ((UI-Text 'VPS: tunel do paneli otwarty ({0}).' 'VPS: tunnel to the panels open ({0}).') -f $addresses.ClassicUrl)
        $url = Show-PanelChoiceDialog -Addresses $addresses
        if ($url -eq 'panel-password') {
            [Windows.Forms.MessageBox]::Show((UI-Text 'Panele na VPS słuchają tylko na samym VPS i otwierają się bez hasła - dojść do nich można wyłącznie przez ten tunel SSH.' 'The panels on the VPS listen on the VPS itself only and open without a password - they can be reached only through this SSH tunnel.'), 'VPS', 'OK', 'Information') | Out-Null
            $url = $addresses.ClassicUrl
        }
        if ($url) { Start-Process $url }
    })

    $buttons['tunnelClose'].Add_Click({
        $closed = Close-M2VpsPanel -ServerRoot $root
        Write-LocalLog ((UI-Text 'VPS: tunel do paneli {0}.' 'VPS: tunnel to the panels {0}.') -f $(if ($closed) { (UI-Text 'zamkniety' 'closed') } else { (UI-Text 'nie byl otwarty' 'was not open') }))
        & $showStatus
    })

    $buttons['passwords'].Add_Click({
        $vps = & $saveFields
        if (-not $vps) { return }
        & $runQuick {
            $accounts = @(Get-M2VpsAccounts -State $vps)
            if ($accounts.Count -eq 0) {
                [Windows.Forms.MessageBox]::Show((UI-Text 'Na VPS nie ma jeszcze pliku z hasłami - powstaje, gdy po instalacji wstanie baza.' 'The VPS has no password file yet - it appears when the database comes up after the install.'), 'VPS', 'OK', 'Information') | Out-Null
                return
            }
            $text = ($accounts | ForEach-Object { (UI-Text 'login {0}   hasło {1}{2}' 'login {0}   password: {1}{2}') -f $_.Login, $_.Password, $(if ($_.Note) { '   (' + $_.Note + ')' } else { '' }) }) -join "`r`n"
            Show-CoopSecretDialog -Title (UI-Text 'Hasła kont gry na VPS' 'Game account passwords on the VPS') -Intro (UI-Text 'Tymi loginami i hasłami logujesz się na serwer na VPS. Leżą też na VPS w /root/metin2-accounts.txt (tylko dla roota).' 'You log in to the server on the VPS with these logins and passwords. They are also on the VPS in /root/metin2-accounts.txt (for root only).') -Secret $text
        }
    })

    $buttons['client'].Add_Click({
        $vps = & $saveFields
        if (-not $vps) { return }
        & $runQuick {
            $result = Write-M2VpsClientEntry -State $vps -ServerRoot $root -ClientFolder (Get-CoopClientFolder)
            Write-LocalLog ((UI-Text 'VPS: zapisano serwer VPS w kliencie ({0}).' 'VPS: the VPS server saved in the client ({0}).') -f $result.Path)
            $text = ((UI-Text "W kliencie wybierz serwer 'Online: {0}' ({1})." "In the client choose the server 'Online: {0}' ({1}).") -f $result.Name, $result.Host)
            if ($result.Replaced) { $text += ((UI-Text "`r`n`r`nZastąpił świat znajomego '{0}' - klient ma jedno takie miejsce, a kod zaproszenia wpisze go z powrotem." "`r`n`r`nIt replaced the friend's world '{0}' - the client has one such place, and the invite code puts it back.") -f $result.Replaced) }
            [Windows.Forms.MessageBox]::Show($text, (UI-Text 'VPS w kliencie' 'VPS in the client'), 'OK', 'Information') | Out-Null
        }
    })

    $buttons['invite'].Add_Click({
        $vps = & $saveFields
        if (-not $vps) { return }
        if (-not (Get-Command Read-M2CoopInvite -ErrorAction SilentlyContinue)) {
            [Windows.Forms.MessageBox]::Show((UI-Text 'Ta paczka nie ma modułu COOP, a kody zaproszeń idą przez niego.' 'This package has no COOP module, and the invite codes go through it.'), 'VPS', 'OK', 'Information') | Out-Null
            return
        }
        $name = [Microsoft.VisualBasic.Interaction]::InputBox((UI-Text "Imię albo nick znajomego - z niego powstanie jego login na VPS.`r`n`r`nPuste pole pokaże kody dla znajomych, którzy już mają konta." "Your friend's name or nick - their login on the VPS is made from it.`r`n`r`nAn empty field shows the codes for friends who already have accounts."), (UI-Text 'Kod dla znajomego (VPS)' 'Code for a friend (VPS)'), '')
        # InputBox answers "" for Cancel and for an empty OK alike, and what
        # the empty one shows is every friend's password: ask which it was.
        if (-not $name) {
            $showAll = [Windows.Forms.MessageBox]::Show((UI-Text 'Pokazać kody dla znajomych, którzy już mają konta na VPS?' 'Show the codes for friends who already have accounts on the VPS?'), (UI-Text 'Kod dla znajomego (VPS)' 'Code for a friend (VPS)'), 'YesNo', 'Question')
            if ($showAll -ne [Windows.Forms.DialogResult]::Yes) { return }
        }
        & $runQuick {
            $vpsStatus = Get-M2VpsStatus -State $vps
            $codes = @()
            if ($name) {
                $friend = New-M2VpsFriend -State $vps -ServerRoot $root -Name $name
                Write-LocalLog ((UI-Text 'VPS: konto znajomego na VPS, login {0}.' 'VPS: a friend''s account on the VPS, login {0}.') -f $friend.login)
                $codes += ((UI-Text '{0} (login {1}, hasło {2}):' '{0} (login {1}, password: {2}):') -f $friend.name, $friend.login, $friend.password)
                $codes += (Get-M2VpsFriendInvite -State $vps -ServerRoot $root -Account $friend -Status $vpsStatus)
            }
            else {
                foreach ($account in @(Get-M2VpsAccounts -State $vps | Where-Object { $_.Note -like 'znajomy*' })) {
                    $codes += ((UI-Text '{0} (login {1}, hasło {2}):' '{0} (login {1}, password: {2}):') -f $account.Note, $account.Login, $account.Password)
                    $codes += (Get-M2VpsFriendInvite -State $vps -ServerRoot $root -Account $account -Status $vpsStatus)
                    $codes += ''
                }
            }
            if ($codes.Count -eq 0) {
                [Windows.Forms.MessageBox]::Show((UI-Text 'Na VPS nie ma jeszcze kont znajomych - wpisz imię, żeby założyć pierwsze.' 'The VPS has no friends'' accounts yet - enter a name to create the first one.'), 'VPS', 'OK', 'Information') | Out-Null
                return
            }
            Show-CoopSecretDialog -Title (UI-Text 'Kod zaproszenia na VPS' 'Invite code for the VPS') `
                -Intro (UI-Text 'Wyślij kod znajomemu w prywatnej wiadomości - zawiera jego hasło. Znajomy wkleja go w swoim launcherze (COOP > Dołączam do znajomego) albo w Dolacz.bat w folderze klienta.' 'Send the code to your friend in a private message - it holds their password. Your friend pastes it in their launcher (COOP > I join a friend) or into Dolacz.bat in the client folder.') `
                -Secret ($codes -join "`r`n")
        }
    })

    try { & $showStatus } catch { $status.Text = (UI-Text "Nie udało się odczytać stanu: $($_.Exception.Message)" "Could not read the state: $($_.Exception.Message)") }
    [void]$dialog.ShowDialog()
    $dialog.Dispose()
}

function Show-PanelChoiceDialog {
    # Two panels look at the same world and neither replaces the other, so the
    # button asks instead of deciding: the classic one is the map and the
    # controls this launcher has always opened, seban latino's is the wider
    # view - profiles, rankings, the economy's history, the host's load.
    param([Parameter(Mandatory = $true)]$Addresses)

    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = (T 'panelDialog')
    $dialog.Size = [Drawing.Size]::new(520, 250)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false

    $info = [Windows.Forms.Label]::new()
    $info.Text = (T 'panelInfo')
    $info.Location = [Drawing.Point]::new(16, 14)
    $info.Size = [Drawing.Size]::new(480, 22)
    $dialog.Controls.Add($info)

    $classicButton = [Windows.Forms.Button]::new()
    $classicButton.Text = ("{0}  -  port {1}" -f (T 'panelClassic'), $Addresses.ClassicPort)
    $classicButton.Location = [Drawing.Point]::new(16, 46)
    $classicButton.Size = [Drawing.Size]::new(480, 56)
    $classicButton.DialogResult = [Windows.Forms.DialogResult]::Yes
    $dialog.Controls.Add($classicButton)

    $sebanButton = [Windows.Forms.Button]::new()
    $sebanButton.Text = ("{0}  -  port {1}" -f (T 'panelSeban'), $Addresses.SebanPort)
    $sebanButton.Location = [Drawing.Point]::new(16, 110)
    $sebanButton.Size = [Drawing.Size]::new(480, 56)
    $sebanButton.DialogResult = [Windows.Forms.DialogResult]::No
    $dialog.Controls.Add($sebanButton)

    # The third thing somebody pressing this button may actually want.
    # "podajcie te kody do gm bo ja nie moge na www wejsc", "ja nie mam zadnego
    # hasla nawet w panelu" - it is one line in .env and nothing showed it.
    $passwordButton = [Windows.Forms.Button]::new()
    $passwordButton.Text = (T 'panelPw')
    $passwordButton.Location = [Drawing.Point]::new(16, 174)
    $passwordButton.Size = [Drawing.Size]::new(370, 30)
    $passwordButton.DialogResult = [Windows.Forms.DialogResult]::Retry
    $dialog.Controls.Add($passwordButton)

    $cancelButton = [Windows.Forms.Button]::new()
    $cancelButton.Text = (T 'cancel')
    $cancelButton.Location = [Drawing.Point]::new(396, 174)
    $cancelButton.Size = [Drawing.Size]::new(100, 30)
    $cancelButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($cancelButton)
    $dialog.AcceptButton = $classicButton
    $dialog.CancelButton = $cancelButton

    $answer = $dialog.ShowDialog()
    $dialog.Dispose()
    switch ($answer) {
        ([Windows.Forms.DialogResult]::Yes) { return $Addresses.ClassicUrl }
        ([Windows.Forms.DialogResult]::No)  { return $Addresses.SebanUrl }
        ([Windows.Forms.DialogResult]::Retry) { return 'panel-password' }
        default { return $null }
    }
}

$panelButton.Add_Click({
    $addresses = Get-M2PanelAddresses -ServerRoot $root
    $url = Show-PanelChoiceDialog -Addresses $addresses
    if ($url -eq 'panel-password') {
        Show-PanelPasswordDialog
        return
    }
    if ($url) {
        Write-LocalLog (UI-Text "Otwieram panel: $url" "Opening the panel: $url")
        Start-Process $url
    }
})
$clientButton.Add_Click({ [void](Select-ClientExecutable) })
$updateButton.Add_Click({
    # One button for the whole flow: check in-process, and only offer to install
    # when there really is something newer.
    $installed = Get-InstalledServerVersion
    Write-LocalLog (UI-Text "Sprawdzam aktualizacje (zainstalowana wersja: $installed)..." "Checking for updates (installed version: $installed)...")
    $manifest = $null
    try {
        $config = Get-M2LauncherConfig -ServerRoot $root -ConfigPath $configPath
        $manifest = Get-M2UpdateManifest -Source ([string]$config.manifestUrl)
    }
    catch {
        Write-LocalLog (UI-Text "Nie udało się sprawdzić aktualizacji: $($_.Exception.Message)" "Could not check for updates: $($_.Exception.Message)")
        [Windows.Forms.MessageBox]::Show(
            (UI-Text "Nie udało się sprawdzić aktualizacji.`r`n`r`n$($_.Exception.Message)" "Could not check for updates.`r`n`r`n$($_.Exception.Message)"),
            (UI-Text 'Sprawdzanie aktualizacji' 'Checking for updates'), 'OK', 'Warning') | Out-Null
        return
    }
    $server = $null
    $serverProperty = $manifest.PSObject.Properties['server']
    if ($serverProperty -and $serverProperty.Value) { $server = $serverProperty.Value }
    if (-not $server -or -not [string]$server.version) {
        $message = (UI-Text 'Kanał aktualizacji nie podał wersji serwera. Twoja instalacja pozostaje bez zmian.' 'The update channel gave no server version. Your installation stays as it is.')
        $statusProperty = $manifest.PSObject.Properties['statusMessage']
        if ($statusProperty -and [string]$statusProperty.Value) { $message = [string]$statusProperty.Value }
        Write-LocalLog $message
        [Windows.Forms.MessageBox]::Show($message, (UI-Text 'Brak aktualizacji' 'No update'), 'OK', 'Information') | Out-Null
        return
    }
    $available = ([string]$server.version).Trim()
    $script:latestManifest = $manifest
    $script:latestServerVersion = $available
    Set-LatestVersionsFromManifest -Manifest $manifest
    $script:latestVersionChecked = $true
    Update-VersionFooter
    Write-LocalLog (UI-Text "Dostępna wersja serwera: $available" "Available server version: $available")
    if ($installed -and $installed -ne 'unknown' -and $installed.Equals($available, [StringComparison]::OrdinalIgnoreCase)) {
        [Windows.Forms.MessageBox]::Show((UI-Text "Masz już najnowszą wersję ($available)." "You already have the newest version ($available)."), (UI-Text 'Aktualizacje' 'Updates'), 'OK', 'Information') | Out-Null
        return
    }
    $answer = [Windows.Forms.MessageBox]::Show(
        (UI-Text "Znaleziono nową wersję serwera: $available`r`n(zainstalowana: $installed)`r`n`r`nZainstalować teraz?`r`n`r`nTwoje postacie, przedmioty i boty pozostaną bez zmian. Aktualizacja przebudowuje serwer lokalnie — przy pierwszym razie może to potrwać kilkanaście–kilkadziesiąt minut. Postęp zobaczysz w logu poniżej." "A new server version was found: $available`r`n(installed: $installed)`r`n`r`nInstall it now?`r`n`r`nYour characters, items and bots stay as they are. The update rebuilds the server locally - the first time this can take anything from ten minutes to an hour. You will see the progress in the log below."),
        (UI-Text 'Dostępna aktualizacja' 'Update available'), 'YesNo', 'Question')
    if ($answer -ne [Windows.Forms.DialogResult]::Yes) {
        Write-LocalLog (UI-Text 'Aktualizacja odłożona na później.' 'Update put off until later.')
        return
    }
    # The server only. The client half of the GM panel is experimental and
    # goes in through its own button below, never with the ordinary update.
    Start-LauncherAction -Action 'UpdateServer' -Yes
})
$gmPanelButton.Add_Click({
    $config = Get-M2LauncherConfig -ServerRoot $root -ConfigPath $configPath
    if (-not [string]$config.clientRoot) {
        [Windows.Forms.MessageBox]::Show((UI-Text 'Najpierw wskaż folder klienta przyciskiem WYBIERZ KLIENTA.' 'First point to the client folder with the CHOOSE CLIENT button.'), (UI-Text 'Brak klienta' 'No client'), 'OK', 'Information') | Out-Null
        return
    }
    if ($script:clientUpdateIsPlain) {
        $answer = [Windows.Forms.MessageBox]::Show(
            (UI-Text "Zaktualizować klienta w $($config.clientRoot)?`r`n`r`nPodmienia pack\root.index i pack\root.data (skrypty gry). Poprzednie wersje trafiają do backups\client w folderze serwera." "Update the client in $($config.clientRoot)?`r`n`r`nReplaces pack\root.index and pack\root.data (the game scripts). The previous versions go to backups\client in the server folder."),
            (UI-Text 'Aktualizacja klienta' 'Client update'), 'YesNo', 'Question')
        if ($answer -ne [Windows.Forms.DialogResult]::Yes) { return }
        Start-LauncherAction -Action 'UpdateClient' -Yes
        return
    }
    $answer = [Windows.Forms.MessageBox]::Show(
        (UI-Text "Panel GM na F9 (autor: OskarPWA) to funkcja MOCNO EKSPERYMENTALNA.`r`n`r`nInstalacja podmienia w kliencie dwa pliki: pack\root.eix i pack\root.epk (skrypty gry). Poprzednie wersje trafiają do kopii zapasowej w folderze serwera (backups\client), więc da się wrócić.`r`n`r`nPanel otwiera tylko postać z uprawnieniami GM klawiszem F9. Jeśli po instalacji gra nie wczytuje się do końca, przywróć pliki z kopii i zgłoś to na GitHubie.`r`n`r`nZainstalować teraz?" "The GM panel on F9 (by OskarPWA) is a HIGHLY EXPERIMENTAL feature.`r`n`r`nThe install replaces two files in the client: pack\root.eix and pack\root.epk (the game scripts). The previous versions go to a backup in the server folder (backups\client), so you can go back.`r`n`r`nThe panel opens only for a character with GM rights, with the F9 key. If the game does not load fully after the install, restore the files from the backup and report it on GitHub.`r`n`r`nInstall now?"),
        (UI-Text 'Panel GM F9 - wersja testowa' 'GM panel F9 - test version'), 'YesNo', 'Warning')
    if ($answer -ne [Windows.Forms.DialogResult]::Yes) { return }
    Start-LauncherAction -Action 'UpdateClient' -Yes
})
$diagnosticsButton.Add_Click({ Start-LauncherAction -Action 'Diagnose' })
$bundleButton.Add_Click({
    # The support address comes from the update manifest, so it is looked up
    # once per session - a slow or missing network just falls back to the ZIP.
    $support = Get-SupportSettings
    if ($support.UploadUrl) {
        $answer = [Windows.Forms.MessageBox]::Show(
            (UI-Text "Spakowac logi i wyslac je od razu do autora projektu?`r`n`r`nPaczka zawiera logi Dockera i konfiguracje z usunietymi haslami.`r`n`r`nNIE = tylko zapisz ZIP na dysku." "Pack the logs and send them straight to the project's author?`r`n`r`nThe package holds the Docker logs and the configuration with the passwords removed.`r`n`r`nNO = only save the ZIP on disk."),
            (UI-Text 'Wyslij logi' 'Send logs'), 'YesNoCancel', 'Question')
        if ($answer -eq [Windows.Forms.DialogResult]::Cancel) { return }
        if ($answer -eq [Windows.Forms.DialogResult]::Yes) {
            Start-LauncherAction -Action 'SendLogs' -Yes
            return
        }
    }
    else {
        [Windows.Forms.MessageBox]::Show(
            (UI-Text "Zapisze paczke ZIP z logami i otworze jej folder - dolacz ja do zgloszenia na GitHubie.`r`n`r`nOtworze tez strone zgloszen." "I will save a ZIP package with the logs and open its folder - attach it to a report on GitHub.`r`n`r`nI will also open the reports page."),
            (UI-Text 'Logi' 'Logs'), 'OK', 'Information') | Out-Null
        $script:openContactAfterAction = $support.ContactUrl
    }
    Start-LauncherAction -Action 'Logs' -OpenSupport
})
$openLogButton.Add_Click({
    if (-not (Test-Path -LiteralPath $sessionLog -PathType Leaf)) { Write-LocalLog (UI-Text 'Utworzono dziennik launchera.' 'Launcher log created.') }
    Start-Process notepad.exe -ArgumentList ('"{0}"' -f $sessionLog)
})
$folderButton.Add_Click({
    New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
    Start-Process explorer.exe -ArgumentList ('"{0}"' -f $logDirectory)
})
$botCountButton.Add_Click({
    $current = Get-BotCountFromEnv
    $plan = Get-SpawnPlanFromEnv
    $kingdoms = Get-KingdomPlanFromEnv
    $chosen = Show-BotCountDialog -Current $current -Plan $plan -Kingdoms $kingdoms
    if ($null -eq $chosen) { return }
    $count = [int]$chosen.Count
    $extra = @('-BotCount', "$count", '-SpawnMinutes', "$($chosen.Minutes)", '-LateJoiners', "$($chosen.Late)", '-LateHours', "$($chosen.Hours)",
        '-PerKingdom', $(if ($chosen.PerKingdom) { '1' } else { '0' }),
        '-ShinsooBots', "$($chosen.Shinsoo)", '-ChunjoBots', "$($chosen.Chunjo)", '-JinnoBots', "$($chosen.Jinno)",
        '-GameChannels', "$($chosen.GameChannels)", '-Channel2Share', "$($chosen.Channel2Share)", '-FreshCount', "$($chosen.FreshCount)")
    $what = if ($chosen.PerKingdom) {
        (UI-Text "osobno dla królestw: Shinsoo $($chosen.Shinsoo), Chunjo $($chosen.Chunjo), Jinno $($chosen.Jinno)" "per kingdom: Shinsoo $($chosen.Shinsoo), Chunjo $($chosen.Chunjo), Jinno $($chosen.Jinno)")
    }
    else { (UI-Text "$count grających botów" "$count playing bots") }
    $channelWhat = switch ([int]$chosen.GameChannels) {
        1 { '' }
        2 { (UI-Text ", dwa kanały ($($chosen.Channel2Share)% botów na CH2)" ", two channels ($($chosen.Channel2Share)% of the bots on CH2)") }
        3 { (UI-Text ", trzy kanały ($($chosen.Channel2Share)% botów na CH2, $($chosen.FreshCount) świeżych botów na CH3)" ", three channels ($($chosen.Channel2Share)% of the bots on CH2, $($chosen.FreshCount) fresh bots on CH3)") }
        default { (UI-Text ", cztery kanały ($($chosen.Channel2Share)% botów na CH2, $($chosen.FreshCount) świeżych botów na CH3 i CH4)" ", four channels ($($chosen.Channel2Share)% of the bots on CH2, $($chosen.FreshCount) fresh bots on CH3 and CH4)") }
    }
    # A kingdom at zero starts nobody, and said so only in the text launcher.
    $emptyKingdoms = @()
    if ($chosen.PerKingdom) {
        foreach ($k in @('Shinsoo', 'Chunjo', 'Jinno')) { if ([int]$chosen.$k -le 0) { $emptyKingdoms += $k } }
    }
    $emptyWhat = if ($emptyKingdoms.Count -gt 0) { (UI-Text "`n`nUWAGA: " "`n`nWARNING: ") + ($emptyKingdoms -join (UI-Text ' i ' ' and ')) + (UI-Text ' nie wystartuje żadnego bota.' ' will start no bot.') } else { '' }
    $answer = [Windows.Forms.MessageBox]::Show(
        (UI-Text "Ustawić $what (wejście w $($chosen.Minutes) min, $($chosen.Late) dodatkowych w ciągu $($chosen.Hours) h)$channelWhat i zrestartować serwer teraz, aby zastosować? Baza i postęp botów pozostaną bez zmian.$emptyWhat" "Set $what (entry within $($chosen.Minutes) min, $($chosen.Late) extra over $($chosen.Hours) h)$channelWhat and restart the server now to apply it? The database and the bots' progress stay as they are.$emptyWhat"),
        (UI-Text 'Liczba botów' 'Bot count'), 'YesNoCancel', 'Question')
    if ($answer -eq [Windows.Forms.DialogResult]::Cancel) { return }
    if ($answer -eq [Windows.Forms.DialogResult]::Yes) {
        Start-LauncherAction -Action 'SetBots' -Yes -ExtraArgs $extra
    }
    else {
        Start-LauncherAction -Action 'SetBots' -ExtraArgs $extra
    }
})
$difficultyButton.Add_Click({
    $current = Get-DifficultyFromEnv
    $chosen = Show-DifficultyDialog -Current $current
    if ($null -eq $chosen) { return }
    $what = switch ($chosen.Level) {
        'easy' { (UI-Text 'łatwy (bez czekania)' 'easy (no waiting)') }
        'medium' { (UI-Text 'średni (Biolog 8 h, koń 4-7 h, księgi 7 h)' 'medium (Biologist 8 h, horse 4-7 h, books 7 h)') }
        'hard' { (UI-Text 'trudny (Biolog 24 h, koń 12-21 h, księgi 21 h)' 'hard (Biologist 24 h, horse 12-21 h, books 21 h)') }
        default { (UI-Text "własny (Biolog $($chosen.Biologist) h, Stajenny $($chosen.Horse) h, księgi: gracze $($chosen.Book) h, boty $($chosen.BotBook) h)" "custom (Biologist $($chosen.Biologist) h, Stable Keeper $($chosen.Horse) h, books: players $($chosen.Book) h, bots $($chosen.BotBook) h)") }
    }
    $features = (UI-Text "Auto Łowy $(if (-not $chosen.AutoHunt) { 'wyłączone' } elseif ($chosen.AutoHuntItem) { 'włączone (tylko po kupnie z ItemShop)' } else { 'włączone (dla każdego)' }), Towarzysz $(if ($chosen.Sidekick) { 'włączony' } else { 'wyłączony' }), Skrzynia Ucznia $(if ($chosen.Starter) { 'tak' } else { 'nie' }), Dom Towarowy $(if ($chosen.Flea) { 'włączony' } else { 'wyłączony' })" "Auto Hunt $(if (-not $chosen.AutoHunt) { 'off' } elseif ($chosen.AutoHuntItem) { 'on (only after an ItemShop purchase)' } else { 'on (for everybody)' }), Companion $(if ($chosen.Sidekick) { 'on' } else { 'off' }), Apprentice Chest $(if ($chosen.Starter) { 'yes' } else { 'no' }), Flea Market $(if ($chosen.Flea) { 'on' } else { 'off' })")
    $answer = [Windows.Forms.MessageBox]::Show(
        (UI-Text "Ustawić poziom trudności: $what; $features - i zrestartować serwer teraz, aby zastosować? Baza i postęp botów pozostaną bez zmian." "Set the difficulty: $what; $features - and restart the server now to apply it? The database and the bots' progress stay as they are."),
        (UI-Text 'Poziom trudności' 'Difficulty'), 'YesNoCancel', 'Question')
    if ($answer -eq [Windows.Forms.DialogResult]::Cancel) { return }
    $extra = @('-Difficulty', $chosen.Level, '-BiologistHours', "$($chosen.Biologist)", '-HorseHours', "$($chosen.Horse)",
        '-BookHours', "$($chosen.Book)", '-BotBookHours', "$($chosen.BotBook)",
        '-AutoHunt', $(if ($chosen.AutoHunt) { '1' } else { '0' }), '-AutoHuntItem', $(if ($chosen.AutoHuntItem) { '1' } else { '0' }),
        '-Sidekick', $(if ($chosen.Sidekick) { '1' } else { '0' }),
        '-StarterChest', $(if ($chosen.Starter) { '1' } else { '0' }),
        '-FleaMarket', $(if ($chosen.Flea) { '1' } else { '0' }))
    if ($answer -eq [Windows.Forms.DialogResult]::Yes) {
        Start-LauncherAction -Action 'SetDifficulty' -Yes -ExtraArgs $extra
    }
    else {
        Start-LauncherAction -Action 'SetDifficulty' -ExtraArgs $extra
    }
})
if ($coopButton) { $coopButton.Add_Click({ Show-CoopDialog }) }
if ($vpsButton) { $vpsButton.Add_Click({ Show-VpsDialog }) }
$importDbButton.Add_Click({
    if (-not (Confirm-DockerReady)) { return }
    $target = Get-GuiTargetVolume
    $sources = @()
    try { $sources = @(Get-M2DbDataVolumes | Where-Object { $_.Name -ne $target }) } catch { $sources = @() }
    if (-not $sources -or $sources.Count -eq 0) {
        [Windows.Forms.MessageBox]::Show((UI-Text 'Nie znaleziono innej bazy Docker do importu na tym komputerze.' 'No other Docker database to import was found on this computer.'), (UI-Text 'Import bazy' 'Database import'), 'OK', 'Information') | Out-Null
        return
    }
    $dlg = [Windows.Forms.Form]::new()
    $dlg.Text = (T 'importDialog')
    $dlg.Size = [Drawing.Size]::new(470, 320)
    $dlg.StartPosition = 'CenterParent'
    $dlg.FormBorderStyle = 'FixedDialog'
    $dlg.MaximizeBox = $false
    $dlg.MinimizeBox = $false
    $lbl = [Windows.Forms.Label]::new()
    $lbl.Text = (T 'importInfo')
    $lbl.Location = [Drawing.Point]::new(12, 10)
    $lbl.Size = [Drawing.Size]::new(430, 44)
    $dlg.Controls.Add($lbl)
    $list = [Windows.Forms.ListBox]::new()
    $list.Location = [Drawing.Point]::new(12, 58)
    $list.Size = [Drawing.Size]::new(430, 170)
    # The project names are opaque hashes, so the creation date is the only thing
    # that tells one install from another.
    foreach ($item in $sources) {
        $label = $item.Project
        if ($item.CreatedAt) { $label = (UI-Text '{0}   (utworzona {1:yyyy-MM-dd HH:mm})' '{0}   (created {1:yyyy-MM-dd HH:mm})') -f $item.Project, $item.CreatedAt }
        [void]$list.Items.Add($label)
    }
    $list.SelectedIndex = 0
    $dlg.Controls.Add($list)
    $okButton = [Windows.Forms.Button]::new()
    $okButton.Text = (T 'importOk')
    $okButton.Location = [Drawing.Point]::new(246, 240)
    $okButton.Size = [Drawing.Size]::new(95, 32)
    $okButton.DialogResult = [Windows.Forms.DialogResult]::OK
    $dlg.Controls.Add($okButton)
    $cancelButton = [Windows.Forms.Button]::new()
    $cancelButton.Text = (T 'cancel')
    $cancelButton.Location = [Drawing.Point]::new(347, 240)
    $cancelButton.Size = [Drawing.Size]::new(95, 32)
    $cancelButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dlg.Controls.Add($cancelButton)
    $dlg.AcceptButton = $okButton
    $dlg.CancelButton = $cancelButton
    $result = $dlg.ShowDialog()
    $picked = ''
    if ($list.SelectedIndex -ge 0) { $picked = [string]$sources[$list.SelectedIndex].Project }
    $dlg.Dispose()
    if ($result -ne [Windows.Forms.DialogResult]::OK -or -not $picked) { return }
    $confirm = [Windows.Forms.MessageBox]::Show(
        (UI-Text "Zaimportować świat z '$picked' do bieżącej instalacji?`r`n`r`nObecny świat zostanie ZASTĄPIONY (kopia trafi do folderu 'backups'), a serwer zostanie zatrzymany na czas importu. Źródło pozostanie nietknięte." "Import the world from '$picked' into this installation?`r`n`r`nThe current world will be REPLACED (a backup goes to the 'backups' folder), and the server is stopped for the import. The source stays untouched."),
        (UI-Text 'Potwierdź import bazy' 'Confirm the database import'), 'YesNo', 'Warning')
    if ($confirm -ne [Windows.Forms.DialogResult]::Yes) { return }
    Start-LauncherAction -Action 'ImportDb' -Yes -ExtraArgs @('-ImportSource', "$picked")
})
$worldBackupButton.Add_Click({
    # One button rather than three, because the main window has no room for
    # three and the report was that the backup could not be FOUND, not that it
    # was too many clicks away. The dialog says what each of them does before
    # anything is stopped or deleted.
    if (-not (Confirm-DockerReady)) { return }
    $dlg = [Windows.Forms.Form]::new()
    $dlg.Text = (T 'backupDialog')
    $dlg.Size = [Drawing.Size]::new(470, 300)
    $dlg.StartPosition = 'CenterParent'
    $dlg.FormBorderStyle = 'FixedDialog'
    $dlg.MaximizeBox = $false
    $dlg.MinimizeBox = $false
    $lbl = [Windows.Forms.Label]::new()
    $lbl.Text = (T 'backupInfo')
    $lbl.Location = [Drawing.Point]::new(12, 10)
    $lbl.Size = [Drawing.Size]::new(430, 60)
    $dlg.Controls.Add($lbl)
    $choice = ''
    $makeButton = [Windows.Forms.Button]::new()
    $makeButton.Text = (T 'backupMake')
    $makeButton.Location = [Drawing.Point]::new(12, 80)
    $makeButton.Size = [Drawing.Size]::new(430, 40)
    $makeButton.Add_Click({ $script:guiBackupChoice = 'make'; $dlg.DialogResult = [Windows.Forms.DialogResult]::OK })
    $dlg.Controls.Add($makeButton)
    $loadButton = [Windows.Forms.Button]::new()
    $loadButton.Text = (T 'backupLoad')
    $loadButton.Location = [Drawing.Point]::new(12, 126)
    $loadButton.Size = [Drawing.Size]::new(430, 40)
    $loadButton.Add_Click({ $script:guiBackupChoice = 'load'; $dlg.DialogResult = [Windows.Forms.DialogResult]::OK })
    $dlg.Controls.Add($loadButton)
    $resetButton = [Windows.Forms.Button]::new()
    $resetButton.Text = (T 'backupReset')
    $resetButton.Location = [Drawing.Point]::new(12, 172)
    $resetButton.Size = [Drawing.Size]::new(430, 40)
    $resetButton.BackColor = [Drawing.Color]::FromArgb(180, 75, 55)
    $resetButton.ForeColor = [Drawing.Color]::White
    $resetButton.Add_Click({ $script:guiBackupChoice = 'reset'; $dlg.DialogResult = [Windows.Forms.DialogResult]::OK })
    $dlg.Controls.Add($resetButton)
    $cancelButton = [Windows.Forms.Button]::new()
    $cancelButton.Text = (T 'cancel')
    $cancelButton.Location = [Drawing.Point]::new(347, 222)
    $cancelButton.Size = [Drawing.Size]::new(95, 30)
    $cancelButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dlg.Controls.Add($cancelButton)
    $dlg.CancelButton = $cancelButton
    $script:guiBackupChoice = ''
    $result = $dlg.ShowDialog()
    $choice = $script:guiBackupChoice
    $dlg.Dispose()
    if ($result -ne [Windows.Forms.DialogResult]::OK -or -not $choice) { return }

    if ($choice -eq 'make') {
        Start-LauncherAction -Action 'BackupDb' -Yes
        return
    }
    if ($choice -eq 'load') {
        $backupRoot = Join-Path $root 'backups'
        if (-not (Test-Path -LiteralPath $backupRoot -PathType Container) -or
            -not (Get-ChildItem -LiteralPath $backupRoot -Filter 'db-backup-*.zip' -File -ErrorAction SilentlyContinue)) {
            [Windows.Forms.MessageBox]::Show((T 'backupNone'), (T 'backupDialog'), 'OK', 'Information') | Out-Null
            return
        }
        $picker = [Windows.Forms.OpenFileDialog]::new()
        $picker.Title = (T 'backupPick')
        $picker.InitialDirectory = $backupRoot
        $picker.Filter = (UI-Text 'Kopia swiata (db-backup-*.zip)|db-backup-*.zip|ZIP|*.zip' 'World backup (db-backup-*.zip)|db-backup-*.zip|ZIP|*.zip')
        if ($picker.ShowDialog() -ne [Windows.Forms.DialogResult]::OK) { $picker.Dispose(); return }
        $file = $picker.FileName
        $picker.Dispose()
        $confirm = [Windows.Forms.MessageBox]::Show(
            (UI-Text "Przywrócić świat z '$([IO.Path]::GetFileName($file))'?`r`n`r`nObecny świat zostanie ZASTĄPIONY. Zanim to nastąpi, launcher zapisze go do własnej kopii w folderze 'backups', więc da się cofnąć." "Restore the world from '$([IO.Path]::GetFileName($file))'?`r`n`r`nThe current world will be REPLACED. Before that, the launcher saves it to a backup of its own in the 'backups' folder, so it can be undone."),
            (UI-Text 'Potwierdź przywrócenie kopii' 'Confirm restoring the backup'), 'YesNo', 'Warning')
        if ($confirm -ne [Windows.Forms.DialogResult]::Yes) { return }
        Start-LauncherAction -Action 'RestoreDb' -Yes -ExtraArgs @('-RestoreSource', "$file")
        return
    }
    # reset: the world is wiped and the server comes straight back up on the
    # fresh one (-ThenStart), so "wyzeruj i zacznij od nowa" is one decision.
    $confirm = [Windows.Forms.MessageBox]::Show(
        (UI-Text "Wyzerować świat i zacząć od nowa?`r`n`r`nZniknie CAŁY obecny świat: postacie, poziomy, ekwipunek, boty i konta gry. Launcher najpierw zapisze go do kopii zip w folderze 'backups', więc da się do niego wrócić przyciskiem KOPIA SWIATA -> Przywroc swiat z kopii.`r`n`r`nPo wyzerowaniu serwer uruchomi się sam na nowym świecie. Ten start potrwa dłużej - baza powstaje od nowa i boty są zasiewane." "Wipe the world and start over?`r`n`r`nThe WHOLE current world goes: characters, levels, equipment, bots and game accounts. The launcher first saves it to a zip backup in the 'backups' folder, so you can go back to it with BACKUP / NEW WORLD -> Restore from a backup.`r`n`r`nAfter the wipe the server starts by itself on the new world. This start takes longer - the database is made anew and the bots are seeded."),
        (UI-Text 'Potwierdź wyzerowanie świata' 'Confirm wiping the world'), 'YesNo', 'Warning')
    if ($confirm -ne [Windows.Forms.DialogResult]::Yes) { return }
    $again = [Windows.Forms.MessageBox]::Show(
        (UI-Text "Na pewno? To ostatnie pytanie.`r`n`r`nPo kliknięciu TAK obecny świat przestaje być światem tego serwera." "Are you sure? This is the last question.`r`n`r`nOnce you click YES, the current world is no longer this server's world."),
        (UI-Text 'Wyzerowanie świata' 'Wiping the world'), 'YesNo', 'Warning')
    if ($again -ne [Windows.Forms.DialogResult]::Yes) { return }
    # The new world's rates and whether its bots wait, asked before the old
    # one goes: this is the last moment they can be set with nothing yet
    # happening in the world.
    $fresh = Show-FreshWorldDialog
    if (-not $fresh) { return }
    Start-LauncherAction -Action 'ResetWorld' -Yes -ExtraArgs @(
        '-ThenStart',
        '-RateExp', "$($fresh.Exp)",
        '-RateDrop', "$($fresh.Drop)",
        '-RateYang', "$($fresh.Yang)",
        '-HoldBots', "$($fresh.Hold)",
        '-StarterChest', "$($fresh.Starter)")
})
$dbAccessButton.Add_Click({
    # In-process on purpose: an action would print through the log box and the
    # launcher log, and the launcher log travels in support bundles. Read-only
    # text boxes so the values can be selected and copied.
    $envPath = Join-Path $root 'linux-port\docker\.env'
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) {
        [Windows.Forms.MessageBox]::Show((T 'dbAccessNoEnv'), (T 'dbAccessTitle'), 'OK', 'Warning') | Out-Null
        return
    }
    $envText = [IO.File]::ReadAllText($envPath)
    $read = {
        param($name, $default)
        $m = [Regex]::Match($envText, "(?m)^$name=(.*?)\s*$")
        if ($m.Success -and $m.Groups[1].Value) { $m.Groups[1].Value } else { $default }
    }
    $rows = @(
        @('Host', '127.0.0.1'),
        @('Port', (& $read 'M2_DB_PUBLISH_PORT' '3306')),
        @('root', (& $read 'M2_DB_ROOT_PASSWORD' '')),
        @((& $read 'M2_DB_USER' 'metin2'), (& $read 'M2_DB_PASSWORD' ''))
    )
    $dlg = [Windows.Forms.Form]::new()
    $dlg.Text = (T 'dbAccessTitle')
    $dlg.Size = [Drawing.Size]::new(560, 372)
    $dlg.StartPosition = 'CenterParent'
    $dlg.FormBorderStyle = 'FixedDialog'
    $dlg.MaximizeBox = $false
    $dlg.MinimizeBox = $false
    $y = 18
    foreach ($row in $rows) {
        $label = [Windows.Forms.Label]::new()
        $label.Text = $row[0]
        $label.Location = [Drawing.Point]::new(18, $y + 4)
        $label.Size = [Drawing.Size]::new(110, 22)
        $dlg.Controls.Add($label)
        $box = [Windows.Forms.TextBox]::new()
        $box.Text = $row[1]
        $box.ReadOnly = $true
        $box.Location = [Drawing.Point]::new(132, $y)
        $box.Size = [Drawing.Size]::new(396, 24)
        $box.Font = [Drawing.Font]::new('Consolas', 9)
        $dlg.Controls.Add($box)
        $y += 34
    }
    $hint = [Windows.Forms.Label]::new()
    $hint.Text = (T 'dbAccessHint')
    if ($script:clientUpdateIsPlain) { $hint.Text = (T 'dbAccessHint') + [Environment]::NewLine + [Environment]::NewLine + (T 'dbAccessProtoNote') }
    $hint.Location = [Drawing.Point]::new(18, $y + 8)
    $hint.Size = [Drawing.Size]::new(510, 160)
    $dlg.Controls.Add($hint)
    $dlg.Size = [Drawing.Size]::new(560, 412)
    $openButton = [Windows.Forms.Button]::new()
    $openButton.Text = (T 'dbAccessOpenEnv')
    $openButton.Location = [Drawing.Point]::new(18, 330)
    $openButton.Size = [Drawing.Size]::new(170, 32)
    $openButton.Add_Click({ Start-Process notepad.exe -ArgumentList ('"' + $envPath + '"') }.GetNewClosure())
    $dlg.Controls.Add($openButton)
    $okButton = [Windows.Forms.Button]::new()
    $okButton.Text = 'OK'
    $okButton.Location = [Drawing.Point]::new(433, 330)
    $okButton.Size = [Drawing.Size]::new(95, 32)
    $okButton.DialogResult = [Windows.Forms.DialogResult]::OK
    $dlg.Controls.Add($okButton)
    $dlg.AcceptButton = $okButton
    $dlg.ShowDialog() | Out-Null
    $dlg.Dispose()
})
$repairDbButton.Add_Click({
    if (-not (Confirm-DockerReady)) { return }
    $answer = [Windows.Forms.MessageBox]::Show(
        (UI-Text "Naprawić dostęp do bazy?`r`n`r`nUżyj tego, gdy po imporcie serwer nie startuje (playerbot-migrate kończy się błędem) albo gdy Navicat/HeidiSQL odrzuca hasło z pliku .env. Odtwarza tylko techniczne konta bazy (gry i root) — postacie, przedmioty i boty pozostają BEZ ZMIAN. Serwer zostanie zatrzymany na czas naprawy." "Repair the database access?`r`n`r`nUse this when the server does not start after an import (playerbot-migrate ends with an error) or when Navicat/HeidiSQL rejects the password from the .env file. It restores only the database's technical accounts (the game's and root) - characters, items and bots stay UNCHANGED. The server is stopped for the repair."),
        (UI-Text 'Napraw dostęp do bazy' 'Repair database access'), 'YesNo', 'Question')
    if ($answer -ne [Windows.Forms.DialogResult]::Yes) { return }
    Start-LauncherAction -Action 'RepairDb'
})

# The window's layout - the menu of five pages, the cards, the painted
# background (22 September) - is its own file and only moves the controls
# built above into place: their Click handlers and the one-action-at-a-time
# guard stay as they are. Without the file (an installation part-way through
# an update) the window keeps its plain layout. A layout that failed once on
# this computer leaves .m2launcher-classic-layout behind, and the plain
# layout is what opens from then on; deleting the file tries again.
$layoutPath = Join-Path $PSScriptRoot 'Metin2-Launcher-GUI.Layout.ps1'
$classicLayoutFlag = Join-Path $root '.m2launcher-classic-layout'
if ((Test-Path -LiteralPath $layoutPath -PathType Leaf) -and
        ($UiSelfTest -or -not (Test-Path -LiteralPath $classicLayoutFlag -PathType Leaf))) {
    try {
        . $layoutPath
    }
    catch {
        if ($UiSelfTest) { throw }
        Write-StartupFailure ((UI-Text 'Nowy wyglad launchera: ' 'The launcher''s new layout: ') + ($_ | Out-String))
        if ($script:ui -and $script:ui.Cleared) {
            # The controls are half moved and there is no way back inside
            # this process; the next start opens the plain window.
            try { [IO.File]::WriteAllText($classicLayoutFlag, ($_ | Out-String)) } catch { }
            throw ((UI-Text 'Nowy wyglad launchera nie uruchomil sie na tym komputerze ({0}). Uruchom launcher jeszcze raz - otworzy sie w poprzednim wygladzie.' 'The launcher''s new layout did not start on this computer ({0}). Start the launcher again - it opens in the previous layout.') -f $_.Exception.Message)
        }
    }
    if ($UiSelfTest) {
        Invoke-LayoutSelfTest -OutputDirectory $UiTestOutput
        $script:form.Dispose()
        exit 0
    }
}
elseif ($UiSelfTest) { throw 'Metin2-Launcher-GUI.Layout.ps1 is missing.' }

$timer = [Windows.Forms.Timer]::new()
$timer.Interval = 1200
$timer.Add_Tick({ Update-ActionStream; Complete-LauncherAction })
$timer.Start()

$statusTimer = [Windows.Forms.Timer]::new()
$statusTimer.Interval = 8000
$statusTimer.Add_Tick({
    if ($script:activeProcess) { return }
    Refresh-Status
    # One manifest read per session: on the first quiet tick rather than during
    # form startup, so the window is already usable while it happens.
    Read-LatestServerVersion
})
$statusTimer.Start()

# A new version people can actually notice. The footer has always changed
# colour when the server was behind, and that is easy to miss on a window
# sitting in the background - so while an update is waiting the line blinks
# red and says so in words ("zrob migotanie na czerwono ze jest wydana
# nowa wersja klienta lub serwera, aby ludzie to widzieli").
#
# The timer owns nothing but the colour: Update-VersionFooter decides whether
# there is an update at all and what the resting colour is, so a check that
# comes back "already newest" stops the blinking on its own.
$script:versionBlinkOn = $false
$blinkTimer = [Windows.Forms.Timer]::new()
$blinkTimer.Interval = 700
$blinkTimer.Add_Tick({
    if (-not $script:versionLabel) { return }
    if (-not $script:updateAvailable) {
        if ($script:versionBlinkOn) {
            $script:versionBlinkOn = $false
            if ($script:versionBaseColor) { $script:versionLabel.ForeColor = $script:versionBaseColor }
        }
        return
    }
    $script:versionBlinkOn = -not $script:versionBlinkOn
    $script:versionLabel.ForeColor = if ($script:versionBlinkOn) { [Drawing.Color]::Red }
        elseif ($script:versionBaseColor) { $script:versionBaseColor }
        else { [Drawing.Color]::Gold }
})
$blinkTimer.Start()

# COOP: the router's mappings are leased for four hours. While hosting is on
# and this window is open they are renewed every hour by a quiet process of
# their own, outside the action runner, so no button is ever refused for it.
if ($coopButton) {
    $script:coopRenewTimer = [Windows.Forms.Timer]::new()
    $script:coopRenewTimer.Interval = 3600000
    $script:coopRenewTimer.Add_Tick({
        try {
            $coopState = Read-M2CoopState -ServerRoot $root
            if ($coopState.hosting -and $coopState.hosting.active) {
                Start-Process -FilePath 'powershell.exe' -WindowStyle Hidden -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass',
                    '-File', ('"{0}"' -f $cliLauncher), '-Action', 'CoopRenew') | Out-Null
            }
        }
        catch { }
    })
    $script:coopRenewTimer.Start()
}

$script:form.Add_FormClosing({
    param($sender, $eventArgs)
    if ($script:activeProcess -and -not $script:activeProcess.HasExited) {
        $answer = [Windows.Forms.MessageBox]::Show(
            (UI-Text 'Launcher nadal wykonuje operację. Czy na pewno zamknąć okno?' 'The launcher is still running an operation. Close the window anyway?'),
            (UI-Text 'Operacja w toku' 'Operation in progress'), 'YesNo', 'Warning')
        if ($answer -ne [Windows.Forms.DialogResult]::Yes) { $eventArgs.Cancel = $true }
    }
})

if (Test-Path -LiteralPath $sessionLog -PathType Leaf) {
    $tail = Get-Content -LiteralPath $sessionLog -Tail 12 -ErrorAction SilentlyContinue
    if ($tail) { $script:logBox.Text = ($tail -join [Environment]::NewLine) + [Environment]::NewLine }
}
Write-LocalLog (UI-Text 'Uruchomiono GUI launchera.' 'Launcher window started.')
Invoke-ClientStrayCleanup
Update-VersionFooter
Refresh-Status
[void]$script:form.ShowDialog()
