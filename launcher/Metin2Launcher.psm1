Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

# A text in the launcher's language: .m2launcher.json's "language", which the
# window and Metin2-Launcher.ps1 put into $env:M2_LAUNCHER_LANGUAGE for every
# module and every action they start. The English one for 'en', otherwise the
# Polish one, which is word for word what the launcher always said. Each
# module keeps its own copy and none exports it.
function UI-Text {
    param([AllowEmptyString()][string]$Pl, [AllowEmptyString()][string]$En)
    if ($env:M2_LAUNCHER_LANGUAGE -eq 'en' -and $En) { return $En }
    return $Pl
}

# Fallback used when the manifest carries no support block (offline, or an old manifest).
$script:M2_DEFAULT_SUPPORT_CONTACT = 'https://github.com/Doofenyoyo1/metin2_SP/issues'

function Get-M2SiblingClientExecutable {
    # The full package (Metin2-Singleplayer-<version>.zip) unpacks as Klient\
    # beside Serwer\; a launcher that finds the client there asks nobody for
    # it. Empty when there is no such folder.
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $parent = Split-Path -Parent ([IO.Path]::GetFullPath($ServerRoot).TrimEnd('\'))
    if (-not $parent) { return '' }
    foreach ($folder in @('Klient', 'Client')) {
        $candidate = Join-Path $parent "$folder\metin2client.exe"
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return [IO.Path]::GetFullPath($candidate) }
    }
    return ''
}

function Get-M2DefaultLauncherConfig {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    # The mt2009 line has its own manifest: an update meant for one engine
    # dropped onto the other's tree would put ENGINE, world.sql and eighty
    # engine files where they do not belong, and the launcher would then
    # refuse to start the world it had.
    $manifest = 'https://raw.githubusercontent.com/Doofenyoyo1/metin2_SP/main/update-manifest.json'
    if ((Get-M2ServerEngine -ServerRoot $ServerRoot) -eq 'mt2009') {
        $manifest = 'https://raw.githubusercontent.com/Doofenyoyo1/metin2_SP/main/update-manifest-mt2009.json'
    }
    $sibling = Get-M2SiblingClientExecutable -ServerRoot $ServerRoot
    [pscustomobject]@{
        schema = 1
        manifestUrl = $manifest
        clientRoot = $(if ($sibling) { Split-Path -Parent $sibling } else { '' })
        clientExecutable = $sibling
        supportUploadUrl = ''
        # Interface language: 'pl' or 'en'. More and more of the Discord is
        # English-speaking, and a launcher nobody can read is a launcher nobody
        # runs correctly.
        language = 'pl'
        # Whether PLAY starts the game client as well as the server. On by
        # default, because that is what the button has always done and what
        # most people want; an operator who only keeps the world running for
        # other people turns it off and stops having a client open itself.
        launchClientOnPlay = $true
        serverRoot = [IO.Path]::GetFullPath($ServerRoot)
    }
}

function Get-M2LauncherConfig {
    param(
        [Parameter(Mandatory = $true)][string]$ServerRoot,
        [Parameter(Mandatory = $true)][string]$ConfigPath
    )

    $defaults = Get-M2DefaultLauncherConfig -ServerRoot $ServerRoot
    if (-not (Test-Path -LiteralPath $ConfigPath -PathType Leaf)) {
        return $defaults
    }

    $loaded = Get-Content -LiteralPath $ConfigPath -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($name in @('manifestUrl', 'clientRoot', 'clientExecutable', 'supportUploadUrl', 'language')) {
        if ($null -ne $loaded.PSObject.Properties[$name]) {
            $defaults.$name = [string]$loaded.$name
        }
    }
    # Its own line, because the loop above casts to [string] and "False" is a
    # non-empty string - every config would then read as "yes, start it".
    if ($null -ne $loaded.PSObject.Properties['launchClientOnPlay']) {
        $defaults.launchClientOnPlay = [bool]$loaded.launchClientOnPlay
    }
    # A config saved before the client was unpacked beside the server, or
    # pointing at a client that has since moved, still gets the sibling.
    if (-not [string]$defaults.clientExecutable -or
        -not (Test-Path -LiteralPath ([string]$defaults.clientExecutable) -PathType Leaf)) {
        $sibling = Get-M2SiblingClientExecutable -ServerRoot $ServerRoot
        if ($sibling) {
            $defaults.clientExecutable = $sibling
            $defaults.clientRoot = Split-Path -Parent $sibling
        }
    }
    return $defaults
}

function Save-M2LauncherConfig {
    param(
        [Parameter(Mandatory = $true)]$Config,
        [Parameter(Mandatory = $true)][string]$ConfigPath
    )

    $Config | Select-Object schema, manifestUrl, clientRoot, clientExecutable, supportUploadUrl, language, launchClientOnPlay |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $ConfigPath -Encoding UTF8
}

# The builds of metin2client.exe (SHA-256) from before the current one. A full
# package carries the exe of its day, and no client package has carried one
# since 2.0.35 (Defender took it for a trojan), so an install made from an old
# full package keeps its exe through every update, under the new root. The
# first three know two inventory pages: on a server of 2.0.74 or later such a
# client draws the server's page IV in the equipment slots, shows pages III and
# IV empty, sells nothing and loses what it puts on (601210 and kordianq1112,
# 27 September - a full 2.0.71 updated to 2.2.26). A list of the old builds
# rather than of the good ones, so a newer build, or somebody's own, is never
# touched. The same four as Metin2Launcher.Coop.psm1's.
$script:M2OldClientExeHashes = @(
    '8263F81BFACDFA4A664C1F2A8CE846AEE14F19E584DB40A43F3184FEF1A59531',  # 2.0.0 - 2.0.8
    '752623560AB54E2F3F84FF9ADB8961D73E2E289634075118D3FBEA984CFCEFB1',  # klient 2.0.6 - 2.0.12
    '6D2BCDAF8311EAD805629093404137BDEC23CDC71EFC6F684795C333F075ADF0',  # klient 2.0.13, pelna 2.0.71
    '8FD0D516DE691AC4C9CDC84E551DC1EE154881051B03177AA80F57D84F57E66E'   # klient 2.0.14 - 2.0.16
)

# The two executables the full packages up to 2.0.97 carried beside
# metin2client.exe by accident, the 2.0.13 build and a test build of 2.0.14,
# which no update ever took away (the operator, 27 September: "mamy w kliencie
# 3 exe zamiast jednego").
$script:M2StrayClientExeNames = @('metin2client-2.0.13.exe', 'metin2client-claude.exe')

function Get-M2FileSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return '' }
    try { return ([string](Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash).ToUpperInvariant() }
    catch { return '' }
}

function Test-M2ClientExeOld {
    # Whether the client folder's metin2client.exe is one of the old builds.
    param(
        [Parameter(Mandatory = $true)][string]$ClientFolder,
        [string[]]$OldHashes = $script:M2OldClientExeHashes
    )
    $hash = Get-M2FileSha256 -Path (Join-Path $ClientFolder 'metin2client.exe')
    return [bool]($hash -and ($OldHashes -contains $hash))
}

function Get-M2ClientExeComponent {
    # The manifest's "clientExe": the current metin2client.exe as a release
    # asset of its own, with its SHA-256. It stays out of the client package on
    # purpose - a package with one file the antivirus refuses is rolled back
    # whole, and this is the file Defender takes. $null when the manifest has
    # none (an older one) or a malformed one.
    param($Manifest)
    if (-not $Manifest) { return $null }
    $property = $Manifest.PSObject.Properties['clientExe']
    if (-not $property -or -not $property.Value) { return $null }
    $component = $property.Value
    foreach ($name in @('url', 'sha256')) {
        if (-not $component.PSObject.Properties[$name] -or -not [string]$component.$name) { return $null }
    }
    if (-not (Test-M2Sha256 ([string]$component.sha256))) { return $null }
    return $component
}

function Repair-M2ClientExecutables {
    # Puts the client folder's executables in order: an old metin2client.exe
    # replaced by the manifest's (when $ExeComponent is given), a launcher that
    # starts one of the strays pointed back at metin2client.exe, and the strays
    # deleted - the last two only while metin2client.exe is there, so nobody is
    # left without a client. Nothing here fails the caller: an antivirus taking
    # the download, a running client holding its file, a folder that cannot be
    # written are each a line of the notes it returns, and the root refuses an
    # old exe at the login in words of its own.
    param(
        [Parameter(Mandatory = $true)][string]$ClientFolder,
        $ExeComponent = $null,
        [string]$BackupRoot = '',
        [string]$ServerRoot = '',
        [string]$ConfigPath = '',
        [string[]]$OldHashes = $script:M2OldClientExeHashes
    )
    $notes = @()
    if (-not (Test-Path -LiteralPath $ClientFolder -PathType Container)) { return $notes }
    $folder = [IO.Path]::GetFullPath($ClientFolder).TrimEnd('\')
    $main = Join-Path $folder 'metin2client.exe'

    if ($ExeComponent -and (Test-M2ClientExeOld -ClientFolder $folder -OldHashes $OldHashes)) {
        $running = @(Get-M2FolderProcesses -Root $folder)
        if ($running.Count -gt 0) {
            $notes += (UI-Text "Klient ma stary metin2client.exe, ale gra jest uruchomiona ($($running -join ', ')) - zamknij ją, a launcher podmieni plik przy następnym uruchomieniu." "The client has the old metin2client.exe, but the game is running ($($running -join ', ')) - close it, and the launcher replaces the file at its next start.")
        }
        else {
            $temp = Join-Path ([IO.Path]::GetTempPath()) ('m2-client-exe-' + [Guid]::NewGuid().ToString('N') + '.exe')
            try {
                Get-M2Download -Source ([string]$ExeComponent.url) -Destination $temp
                $hash = Get-M2FileSha256 -Path $temp
                if ($hash -ne ([string]$ExeComponent.sha256).ToUpperInvariant()) {
                    throw (UI-Text "błędna suma SHA-256 pobranego pliku ($hash)" "the downloaded file has a wrong SHA-256 ($hash)")
                }
                if ($BackupRoot) {
                    $backup = Join-Path $BackupRoot ('exe-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
                    New-Item -ItemType Directory -Path $backup -Force | Out-Null
                    Copy-Item -LiteralPath $main -Destination (Join-Path $backup 'metin2client.exe') -Force
                }
                Copy-Item -LiteralPath $temp -Destination $main -Force
                $notes += (UI-Text "Podmieniono stary metin2client.exe na aktualny (SHA-256 $($hash.Substring(0, 8))...)." "Replaced the old metin2client.exe with the current one (SHA-256 $($hash.Substring(0, 8))...).")
            }
            catch {
                if (Test-M2AntivirusBlock -ErrorRecord $_) {
                    $notes += ((UI-Text 'Antywirus nie pozwolił zapisać aktualnego metin2client.exe (Windows Defender bierze go za zagrożenie - to fałszywy alarm), więc w folderze klienta został stary, z którym ekwipunek się rozjeżdża. ' 'An antivirus did not let the current metin2client.exe be written (Windows Defender takes it for a threat - a false alarm), so the client folder keeps the old one, with which the inventory goes wrong. ') +
                        (UI-Text 'Dodaj folder klienta do wykluczeń (Zabezpieczenia Windows > Ochrona przed wirusami i zagrożeniami > Zarządzaj ustawieniami > Wykluczenia) i uruchom launcher ponownie.' 'Add the client folder to the exclusions (Windows Security > Virus & threat protection > Manage settings > Exclusions) and start the launcher again.'))
                }
                else {
                    $notes += (UI-Text "Nie udało się podmienić starego metin2client.exe: $($_.Exception.Message)" "Could not replace the old metin2client.exe: $($_.Exception.Message)")
                }
            }
            finally {
                if (Test-Path -LiteralPath $temp -PathType Leaf) {
                    try { Remove-Item -LiteralPath $temp -Force } catch { }
                }
            }
        }
    }

    if (-not (Test-Path -LiteralPath $main -PathType Leaf)) { return $notes }
    # The launcher first: a config that starts a stray is pointed back before
    # the stray goes, or the next start would ask for a file nobody has.
    if ($ServerRoot -and $ConfigPath -and (Test-Path -LiteralPath $ConfigPath -PathType Leaf)) {
        $config = Get-M2LauncherConfig -ServerRoot $ServerRoot -ConfigPath $ConfigPath
        $chosen = [string]$config.clientExecutable
        if ($chosen -and ($script:M2StrayClientExeNames -contains [IO.Path]::GetFileName($chosen)) -and
            ([IO.Path]::GetFullPath((Split-Path -Parent $chosen)).TrimEnd('\') -ieq $folder)) {
            $config.clientExecutable = $main
            $config.clientRoot = $folder
            Save-M2LauncherConfig -Config $config -ConfigPath $ConfigPath
            $notes += (UI-Text "Launcher uruchamiał $([IO.Path]::GetFileName($chosen)) - teraz uruchamia metin2client.exe." "The launcher started $([IO.Path]::GetFileName($chosen)) - now it starts metin2client.exe.")
        }
    }
    foreach ($name in $script:M2StrayClientExeNames) {
        $stray = Join-Path $folder $name
        if (-not (Test-Path -LiteralPath $stray -PathType Leaf)) { continue }
        try {
            Remove-Item -LiteralPath $stray -Force
            $notes += (UI-Text "Usunięto zbędny plik klienta: $name." "Deleted a client file nothing needs: $name.")
        }
        catch {
            $notes += (UI-Text "Nie udało się usunąć $name ($($_.Exception.Message)) - spróbuję przy następnym uruchomieniu." "Could not delete $name ($($_.Exception.Message)) - trying again at the next start.")
        }
    }
    return $notes
}

function ConvertFrom-M2ManifestText {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Text,
        [Parameter(Mandatory = $true)][string]$Origin
    )

    # A byte order mark is not JSON. ConvertFrom-Json in Windows PowerShell
    # refuses a string that starts with one ("Nieprawidlowy element pierwotny
    # JSON"), and Invoke-RestMethod hides that failure by handing back the raw
    # text instead of an object - so the caller reads a String, finds no
    # 'server' property on it and concludes there is no new version. 1.32.0
    # shipped a manifest with a BOM and every install stopped seeing updates
    # for twenty minutes, each one being told its channel had nothing new.
    # Strip the mark, parse it here, and let a real failure be a failure.
    $clean = $Text
    if ($clean.Length -gt 0 -and [int]$clean[0] -eq 0xFEFF) { $clean = $clean.Substring(1) }
    $clean = $clean.Trim()
    if (-not $clean) {
        throw (UI-Text "Kanal aktualizacji ($Origin) zwrocil pusta odpowiedz. Twoja instalacja pozostaje bez zmian." "The update channel ($Origin) answered with nothing. Your installation stays as it is.")
    }
    try {
        $parsed = $clean | ConvertFrom-Json
    }
    catch {
        throw (UI-Text "Kanal aktualizacji ($Origin) zwrocil plik, ktorego nie da sie odczytac jako JSON. To blad po stronie kanalu, nie Twojej instalacji - zglos to na GitHubie. Szczegoly: $($_.Exception.Message)" "The update channel ($Origin) answered with a file that cannot be read as JSON. That is the channel's fault, not your installation's - report it on GitHub. Details: $($_.Exception.Message)")
    }
    if ($parsed -isnot [psobject] -or $parsed -is [string]) {
        throw (UI-Text "Kanal aktualizacji ($Origin) zwrocil cos, co nie jest manifestem. Twoja instalacja pozostaje bez zmian." "The update channel ($Origin) answered with something that is not a manifest. Your installation stays as it is.")
    }
    return $parsed
}

function Get-M2UpdateManifest {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [int]$TimeoutSec = 30
    )

    if (Test-Path -LiteralPath $Source -PathType Leaf) {
        $text = Get-Content -LiteralPath $Source -Raw -Encoding UTF8
        return ConvertFrom-M2ManifestText -Text $text -Origin $Source
    }

    $uri = $null
    if (-not [Uri]::TryCreate($Source, [UriKind]::Absolute, [ref]$uri) -or $uri.Scheme -ne 'https') {
        throw (UI-Text 'Manifest musi być lokalnym plikiem albo adresem HTTPS.' 'The manifest has to be a local file or an HTTPS address.')
    }
    # raw.githubusercontent.com is a CDN with a five-minute cache
    # (Cache-Control: max-age=300), and for a while after a release it hands
    # out the previous manifest: measured five minutes after 2.0.8 was pushed,
    # "Masz juz najnowsza wersje (2.0.7)" from a launcher that had just read
    # it. The contents API answers from the repository itself (its cache is a
    # minute), so for the repository's own manifest it is asked first, with
    # the raw URL as the fallback - the API's anonymous budget is sixty
    # requests an hour per address, and a session reads the manifest once.
    $apiUri = $null
    if ($uri.Host -eq 'raw.githubusercontent.com') {
        $parts = $uri.AbsolutePath.Trim('/') -split '/', 4
        if ($parts.Count -eq 4) {
            [void][Uri]::TryCreate(('https://api.github.com/repos/{0}/{1}/contents/{3}?ref={2}' -f $parts[0], $parts[1], $parts[2], $parts[3]),
                [UriKind]::Absolute, [ref]$apiUri)
        }
    }
    if ($null -ne $apiUri) {
        try {
            $response = Invoke-WebRequest -Uri $apiUri -Method Get -UseBasicParsing -TimeoutSec $TimeoutSec `
                -Headers @{ Accept = 'application/vnd.github.raw+json'; 'User-Agent' = 'metin2-playerbots-launcher' }
            # Windows PowerShell 5.1 hands this media type back as a byte[],
            # and [string] of one is its numbers joined by spaces - so this
            # branch never matched and every launcher read the five-minute CDN
            # instead (m2zip saw 2.2.5 twice after 2.2.6 was pushed).
            $content = $response.Content
            $text = if ($content -is [byte[]]) { [Text.Encoding]::UTF8.GetString($content) } else { [string]$content }
            if ($text.TrimStart().StartsWith('{')) {
                return ConvertFrom-M2ManifestText -Text $text -Origin ([string]$apiUri)
            }
        }
        catch { }
    }
    try {
        # Invoke-WebRequest, not Invoke-RestMethod: the REST variant parses for
        # us and silently degrades to a string when it cannot, which is exactly
        # the failure that has to be visible here.
        $response = Invoke-WebRequest -Uri $uri -Method Get -UseBasicParsing -TimeoutSec $TimeoutSec
        return ConvertFrom-M2ManifestText -Text ([string]$response.Content) -Origin $Source
    }
    catch {
        $statusCode = 0
        try {
            if ($null -ne $_.Exception.Response) {
                $statusCode = [int]$_.Exception.Response.StatusCode
            }
        }
        catch { $statusCode = 0 }

        # GitHub serves raw manifests from an anonymous, per-IP budget. A player
        # who clicks the button a few times in a row spends it, and the bare
        # transport error that came back ("Operacja nie powiodla sie") told them
        # nothing about waiting an hour - or that their install was fine.
        if ($statusCode -eq 403 -or $statusCode -eq 429) {
            throw (UI-Text 'GitHub chwilowo ogranicza liczbe zapytan z Twojego adresu IP (limit anonimowy). Nie jest to blad Twojej instalacji - serwer dziala dalej. Sprobuj ponownie za kilkanascie minut.' 'GitHub is limiting the requests from your IP address for a while (the anonymous limit). That is not your installation''s fault - the server keeps running. Try again in a quarter of an hour or so.')
        }

        # The stable channel may intentionally be empty between releases. A
        # missing manifest must never make the launcher reinstall the server,
        # create another Compose project or touch the user's database.
        if ($statusCode -eq 404) {
            return [pscustomobject]@{
                schema = 1
                channel = 'unavailable'
                publishedAt = $null
                server = $null
                client = $null
                statusMessage = (UI-Text 'Kanał aktualizacji nie został jeszcze opublikowany. Obecna instalacja pozostaje bez zmian.' 'The update channel has not been published yet. The installation stays as it is.')
            }
        }

        throw (UI-Text "Nie można sprawdzić aktualizacji pod adresem $Source. Sprawdź internet, zaporę i ustawienia DNS. Szczegóły: $($_.Exception.Message)" "Cannot check for updates at $Source. Check the Internet connection, the firewall and the DNS settings. Details: $($_.Exception.Message)")
    }
}

function Test-M2Sha256 {
    param([Parameter(Mandatory = $true)][string]$Value)
    return $Value -match '^[A-Fa-f0-9]{64}$'
}

function Test-M2AntivirusBlock {
    # Windows zglasza blokade antywirusa jako zwykly blad operacji na pliku:
    # ERROR_VIRUS_INFECTED (0x800700E1) albo ERROR_VIRUS_DELETED (0x800700E2).
    # Bez tej zamiany w logu zostaje samo "plik zawiera wirusa lub potencjalnie
    # niechciane oprogramowanie" - bez nazwy pliku, a wiec bez niczego, co
    # dalo by sie sprawdzic.
    param([Parameter(Mandatory = $true)]$ErrorRecord)

    $codes = @(-2147024671, -2147024670)
    $exception = $ErrorRecord.Exception
    while ($exception) {
        if ($codes -contains $exception.HResult) { return $true }
        $exception = $exception.InnerException
    }
    return $ErrorRecord.Exception.Message -match 'wirus|virus'
}

function New-M2AntivirusError {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)]$ErrorRecord
    )

    return ((UI-Text "Antywirus zablokowal plik aktualizacji: $Path`n" "An antivirus blocked an update file: $Path`n") +
        (UI-Text "Windows zglosil: $($ErrorRecord.Exception.Message)`n" "Windows said: $($ErrorRecord.Exception.Message)`n") +
        (UI-Text 'Nic nie zostalo zainstalowane - poprzednia wersja serwera dziala dalej. ' 'Nothing was installed - the previous server version keeps running. ') +
        (UI-Text 'Dodaj katalog serwera do wykluczen w Zabezpieczeniach Windows (Ochrona przed ' 'Add the server folder to the exclusions in Windows Security (Virus & threat ') +
        (UI-Text 'wirusami > Zarzadzaj ustawieniami > Wykluczenia) albo przeslij ten log, ' 'protection > Manage settings > Exclusions) or send this log, ') +
        (UI-Text 'zebysmy zobaczyli, o ktory plik chodzi.' 'so that we can see which file it is.'))
}

function Get-M2FolderProcesses {
    # Nazwy programow uruchomionych z tego folderu (albo z jego podfolderow).
    # Windows nie pozwala nadpisac pliku .exe dzialajacego programu, a mowi
    # o tym dopiero przy kopiowaniu - po pobraniu calej paczki.
    param([Parameter(Mandatory = $true)][string]$Root)

    $names = @()
    try {
        $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
        foreach ($process in @(Get-Process -ErrorAction SilentlyContinue)) {
            $path = $null
            try { $path = $process.Path } catch { }
            if ($path -and $path.StartsWith($rootFull, [StringComparison]::OrdinalIgnoreCase)) {
                $names += $process.Name
            }
        }
    }
    catch { }
    return @($names | Select-Object -Unique)
}

function Test-M2FileInUse {
    # "Proces nie moze uzyskac dostepu do pliku, poniewaz jest on uzywany przez
    # inny proces": ERROR_SHARING_VIOLATION albo ERROR_LOCK_VIOLATION, jako
    # IOException gdzies w lancuchu wyjatkow (jak przy Test-M2AccessDenied).
    param([Parameter(Mandatory = $true)]$ErrorRecord)

    $exception = $ErrorRecord.Exception
    while ($exception) {
        if ($exception.HResult -eq -2147024864 -or $exception.HResult -eq -2147024863) { return $true }
        $exception = $exception.InnerException
    }
    return $false
}

function New-M2FileInUseError {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Root
    )

    $holders = @(Get-M2FolderProcesses -Root $Root)
    $lines = @()
    $lines += (UI-Text "Plik jest teraz uzywany przez uruchomiony program: $Path" "A running program is using the file now: $Path")
    if ($holders.Count -gt 0) {
        $lines += (UI-Text "Z tego folderu dziala: $($holders -join ', ')." "Running from this folder: $($holders -join ', ').")
    }
    $lines += (UI-Text 'Zamknij gre (sprawdz tez Menedzer zadan, czy metin2client.exe nie zostal w tle) i kliknij ZAINSTALUJ AKTUALIZACJE jeszcze raz.' 'Close the game (check Task Manager too, in case metin2client.exe stayed in the background) and click CHECK FOR UPDATES again.')
    $lines += (UI-Text 'Nic nie zostalo zmienione - poprzednia wersja dziala dalej.' 'Nothing was changed - the previous version keeps running.')
    return ($lines -join [Environment]::NewLine)
}

function Test-M2AccessDenied {
    # Nie da sie tego zlapac przez `catch [UnauthorizedAccessException]`:
    # przy $ErrorActionPreference = 'Stop' PowerShell 5.1 opakowuje blad
    # cmdletu w ActionPreferenceStopException i typowany catch go mija -
    # sprawdzone, gracz dostawal goly komunikat mimo poprawnego z pozoru
    # bloku. Lancuch wyjatkow mowi prawde, tak samo jak przy antywirusie.
    param([Parameter(Mandatory = $true)]$ErrorRecord)

    $exception = $ErrorRecord.Exception
    while ($exception) {
        if ($exception -is [UnauthorizedAccessException]) { return $true }
        # E_ACCESSDENIED, gdy przyjdzie jako zwykly Win32Exception.
        if ($exception.HResult -eq -2147024891) { return $true }
        $exception = $exception.InnerException
    }
    return $false
}

function Repair-M2WritableFile {
    # Copy-Item -Force overwrites a read-only destination, but NOT a hidden or
    # system one: Windows refuses to replace those and .NET reports it as
    # UnauthorizedAccessException - the same sentence an ACL denial produces.
    # Clearing the three attributes is the one cause we can repair ourselves, so
    # it is tried before the copy is called a failure.
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    try {
        $item = Get-Item -LiteralPath $Path -Force
        $unwanted = ([IO.FileAttributes]::ReadOnly -bor
            [IO.FileAttributes]::Hidden -bor [IO.FileAttributes]::System)
        if (([int]$item.Attributes -band [int]$unwanted) -eq 0) { return $false }
        $item.Attributes = [IO.FileAttributes]([int]$item.Attributes -band (-bnot [int]$unwanted))
        return $true
    }
    catch { return $false }
}

function New-M2AccessDeniedError {
    # "Odmowa dostepu do sciezki" is what Windows says for at least four
    # different problems, and this function exists because the launcher used to
    # pass that one sentence through untouched. Artur554 hit it nine times in a
    # day on E:\Metin2Client - every attempt the same eleven words, nothing to
    # act on, and the client never updated once. Same shape as the bait purchase
    # that reported "cannot_afford_tackle" for three unrelated causes: name each
    # refusal, or the report cannot be diagnosed from outside.
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)]$ErrorRecord
    )

    $lines = @()
    $lines += (UI-Text "Brak prawa zapisu do pliku: $Path" "No right to write the file: $Path")
    $lines += (UI-Text "Windows zglosil: $($ErrorRecord.Exception.Message)" "Windows said: $($ErrorRecord.Exception.Message)")
    $lines += (UI-Text 'Nic nie zostalo zmienione - poprzednia wersja dziala dalej.' 'Nothing was changed - the previous version keeps running.')
    $lines += ''
    $found = $false

    # 1. Cos z tego folderu dziala i trzyma plik. Windows zwykle mowi wtedy
    #    "uzywany przez inny proces", ale przy pliku otwartym na wylacznosc
    #    przez sterownik gry potrafi odpowiedziec odmowa dostepu.
    try {
        $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd('') + ''
        $holders = @(Get-Process -ErrorAction SilentlyContinue |
            Where-Object { $_.Path -and $_.Path.StartsWith($rootFull, [StringComparison]::OrdinalIgnoreCase) } |
            Select-Object -ExpandProperty Name -Unique)
        if ($holders.Count -gt 0) {
            $lines += (UI-Text "* Z tego folderu dziala teraz: $($holders -join ', ')." "* Running from this folder now: $($holders -join ', ').")
            $lines += (UI-Text '  Zamknij gre (takze launcher gry, jesli go uzywasz) i sprobuj ponownie.' '  Close the game (and the game''s launcher too, if you use one) and try again.')
            $found = $true
        }
    }
    catch { }

    # 2. Atrybuty pliku. Repair-M2WritableFile probowal je zdjac wczesniej, wiec
    #    jesli nadal tu sa, to znaczy, ze nie wolno ich bylo zmienic.
    try {
        if (Test-Path -LiteralPath $Path -PathType Leaf) {
            $attrs = (Get-Item -LiteralPath $Path -Force).Attributes
            if (([int]$attrs -band [int][IO.FileAttributes]::ReadOnly) -ne 0) {
                $lines += (UI-Text '* Plik jest tylko do odczytu i nie dalo sie tego zdjac.' '* The file is read-only and that could not be taken off.')
                $found = $true
            }
        }
    }
    catch { }

    # 3. Ochrona folderow w Windows Defenderze. Blokuje zapis do Dokumentow,
    #    Pulpitu i wszystkiego, co operator sam dopisal - i zglasza to dokladnie
    #    tak samo jak brak uprawnien.
    try {
        $cfa = (Get-MpPreference -ErrorAction SilentlyContinue).EnableControlledFolderAccess
        if ($cfa -and [int]$cfa -ne 0) {
            $lines += (UI-Text '* Wlaczona jest Ochrona folderow (Kontrolowany dostep do folderow) w Zabezpieczeniach Windows.' '* Folder protection (Controlled folder access) is on in Windows Security.')
            $lines += (UI-Text '  Zabezpieczenia Windows > Ochrona przed wirusami i zagrozeniami > Ochrona przed' '  Windows Security > Virus & threat protection > Ransomware')
            $lines += (UI-Text '  ransomware > Zezwalaj aplikacji na dostep - dodaj powershell.exe, albo wylacz ochrone na czas aktualizacji.' '  protection > Allow an app through Controlled folder access - add powershell.exe, or turn the protection off for the update.')
            $found = $true
        }
    }
    catch { }

    # 4. Zwykle uprawnienia NTFS. Test zapisu mowi wiecej niz odczytanie listy
    #    ACL, bo liczy sie wynik, a nie to, co na liscie widac.
    if (-not $found) {
        $parent = Split-Path -Parent $Path
        $probe = Join-Path $parent (".m2write-" + [Guid]::NewGuid().ToString('N') + ".tmp")
        try {
            [IO.File]::WriteAllText($probe, 'x')
            Remove-Item -LiteralPath $probe -Force -ErrorAction SilentlyContinue
            $lines += (UI-Text "* Do folderu $parent mozna pisac, ale do samego pliku nie." "* The folder $parent can be written to, but the file itself cannot.")
            $lines += (UI-Text '  Kliknij plik prawym przyciskiem > Wlasciwosci > Zabezpieczenia i sprawdz, czy Twoje konto ma Zapis.' '  Right-click the file > Properties > Security and check that your account has Write.')
        }
        catch {
            $lines += (UI-Text "* Do folderu $parent nie mozna pisac w ogole - to uprawnienia NTFS, nie sam plik." "* The folder $parent cannot be written to at all - that is the NTFS permissions, not the file itself.")
            $lines += (UI-Text '  Kliknij folder prawym przyciskiem > Wlasciwosci > Zabezpieczenia > Edytuj i daj swojemu kontu Pelna kontrole,' '  Right-click the folder > Properties > Security > Edit and give your account Full control,')
            $lines += (UI-Text '  albo przenies klienta do folderu, ktorego jestes wlascicielem (np. C:\Gry\Metin2Client).' '  or move the client to a folder you own (e.g. C:\Games\Metin2Client).')
        }
    }

    return ($lines -join [Environment]::NewLine)
}

function Get-M2Download {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    if (Test-Path -LiteralPath $Source -PathType Leaf) {
        Copy-Item -LiteralPath $Source -Destination $Destination -Force
        return
    }

    $uri = $null
    if (-not [Uri]::TryCreate($Source, [UriKind]::Absolute, [ref]$uri) -or $uri.Scheme -ne 'https') {
        throw (UI-Text 'Pakiet aktualizacji musi pochodzić z lokalnego pliku albo adresu HTTPS.' 'The update package has to come from a local file or an HTTPS address.')
    }
    # Three attempts: a release asset on GitHub answered "(500) Wewnetrzny
    # blad serwera" and "Polaczenie zostalo nieoczekiwanie zakonczone" a
    # second into the download, twice in two minutes, and served the same
    # file minutes later (Hiob, 17 September). One request, one failure was
    # the whole update. An antivirus block is raised at once - it does not
    # mend itself.
    $attempts = 3
    for ($attempt = 1; $attempt -le $attempts; $attempt++) {
        try {
            Invoke-WebRequest -Uri $uri -OutFile $Destination -UseBasicParsing -TimeoutSec 300
            return
        }
        catch {
            if (Test-M2AntivirusBlock -ErrorRecord $_) {
                throw (New-M2AntivirusError -Path $Destination -ErrorRecord $_)
            }
            if ($attempt -ge $attempts) { throw }
            Write-Warning ((UI-Text 'Pobieranie nie powiodlo sie (proba {0} z {1}): {2} - ponawiam za 5 s.' 'The download failed (attempt {0} of {1}): {2} - trying again in 5 s.') -f $attempt, $attempts, $_.Exception.Message)
            Remove-Item -LiteralPath $Destination -Force -ErrorAction SilentlyContinue
            Start-Sleep -Seconds 5
        }
    }
}

function Expand-M2SafeZip {
    param(
        [Parameter(Mandatory = $true)][string]$ArchivePath,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    $root = [IO.Path]::GetFullPath($Destination).TrimEnd('\') + '\'
    $archive = [IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        foreach ($entry in $archive.Entries) {
            $relative = $entry.FullName.Replace('/', '\').TrimStart('\')
            if (-not $relative) { continue }
            if ([IO.Path]::IsPathRooted($relative) -or $relative.Split('\') -contains '..') {
                throw (UI-Text "Niedozwolona ścieżka w ZIP: $($entry.FullName)" "A path not allowed in the ZIP: $($entry.FullName)")
            }

            $target = [IO.Path]::GetFullPath((Join-Path $root $relative))
            if (-not $target.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
                throw (UI-Text "Plik ZIP wychodzi poza katalog docelowy: $($entry.FullName)" "The ZIP file reaches outside the target folder: $($entry.FullName)")
            }

            if (-not $entry.Name) {
                New-Item -ItemType Directory -Path $target -Force | Out-Null
                continue
            }

            New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
            $input = $entry.Open()
            try {
                # Kazdy plik osobno, zeby blokada antywirusa wskazala ten jeden,
                # a nie cala paczke: skaner sprawdza plik przy zamknieciu uchwytu,
                # wiec to tutaj wychodzi na jaw.
                try {
                    $output = [IO.File]::Open($target, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
                    try { $input.CopyTo($output) } finally { $output.Dispose() }
                }
                catch {
                    if (Test-M2AntivirusBlock -ErrorRecord $_) {
                        throw (New-M2AntivirusError -Path $entry.FullName -ErrorRecord $_)
                    }
                    throw
                }
            }
            finally { $input.Dispose() }
        }
    }
    finally { $archive.Dispose() }
}

function Test-M2ProtectedPath {
    param([Parameter(Mandatory = $true)][string]$RelativePath)

    $path = $RelativePath.Replace('/', '\').TrimStart('\')
    $protectedFiles = @(
        'linux-port\docker\.env',
        '.m2launcher.json',
        '.m2launcher-state.json',
        '.m2install.json',
        # COOP: the friends' accounts and passwords, the hosting state.
        '.m2coop.json',
        # VPS: which server this world was put on, and the tunnel to it.
        '.m2vps.json'
    )
    foreach ($protectedFile in $protectedFiles) {
        if ($path.Equals($protectedFile, [StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }

    $protectedDirectories = @('.git', 'backups', 'support-bundles', 'launcher-logs')
    foreach ($protectedDirectory in $protectedDirectories) {
        if ($path.Equals($protectedDirectory, [StringComparison]::OrdinalIgnoreCase) -or
            $path.StartsWith($protectedDirectory + '\', [StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }
    return $false
}

function Invoke-M2PackageUpdate {
    param(
        [Parameter(Mandatory = $true)]$Component,
        [Parameter(Mandatory = $true)][string]$TargetRoot,
        [Parameter(Mandatory = $true)][string]$BackupRoot
    )

    $url = [string]$Component.url
    $expectedHash = ([string]$Component.sha256).ToUpperInvariant()
    if (-not $url -or -not (Test-M2Sha256 $expectedHash)) {
        throw (UI-Text 'Manifest nie zawiera poprawnego URL i SHA-256 dla tej aktualizacji.' 'The manifest has no valid URL and SHA-256 for this update.')
    }

    $target = [IO.Path]::GetFullPath($TargetRoot).TrimEnd('\')
    if (-not (Test-Path -LiteralPath $target -PathType Container)) {
        throw (UI-Text "Katalog docelowy nie istnieje: $target" "The target folder does not exist: $target")
    }

    $tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('m2-update-' + [Guid]::NewGuid().ToString('N'))
    $download = Join-Path $tempRoot 'update.zip'
    $expanded = Join-Path $tempRoot 'expanded'
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

    try {
        $downloadWatch = [Diagnostics.Stopwatch]::StartNew()
        Get-M2Download -Source $url -Destination $download
        $actualHash = (Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash.ToUpperInvariant()
        if ($actualHash -ne $expectedHash) {
            throw (UI-Text "Błędna suma SHA-256. Oczekiwano $expectedHash, otrzymano $actualHash." "Wrong SHA-256. Expected $expectedHash, got $actualHash.")
        }
        $downloadMb = [Math]::Round((Get-Item -LiteralPath $download).Length / 1MB, 1)
        Write-Host ((UI-Text "[faza] pakiet pobrany i sprawdzony: {0} MB w {1} s" "[phase] package downloaded and checked: {0} MB in {1} s") -f $downloadMb, [int]$downloadWatch.Elapsed.TotalSeconds) -ForegroundColor DarkCyan

        Expand-M2SafeZip -ArchivePath $download -Destination $expanded
        $files = @(Get-ChildItem -LiteralPath $expanded -Recurse -File -Force)
        if ($files.Count -eq 0) { throw (UI-Text 'Pakiet aktualizacji jest pusty.' 'The update package is empty.') }

        $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
        $backup = Join-Path $BackupRoot ("update-$stamp")
        New-Item -ItemType Directory -Path $backup -Force | Out-Null

        $changes = @()
        foreach ($file in $files) {
            $relative = $file.FullName.Substring($expanded.Length).TrimStart('\')
            if (Test-M2ProtectedPath -RelativePath $relative) {
                throw (UI-Text "Pakiet próbuje zmienić chroniony plik: $relative" "The package tries to change a protected file: $relative")
            }
            $destination = [IO.Path]::GetFullPath((Join-Path $target $relative))
            if (-not $destination.StartsWith($target + '\', [StringComparison]::OrdinalIgnoreCase)) {
                throw (UI-Text "Niedozwolona ścieżka aktualizacji: $relative" "An update path that is not allowed: $relative")
            }
            $changes += [pscustomobject]@{
                Relative = $relative
                Source = $file.FullName
                Destination = $destination
                Existed = Test-Path -LiteralPath $destination -PathType Leaf
            }
        }

        foreach ($change in $changes) {
            if ($change.Existed) {
                $backupFile = Join-Path $backup $change.Relative
                New-Item -ItemType Directory -Path (Split-Path -Parent $backupFile) -Force | Out-Null
                Copy-Item -LiteralPath $change.Destination -Destination $backupFile -Force
            }
        }

        # Only what was really written is rolled back. Rolling back the whole
        # list used to mean restoring a backup onto a file the update had just
        # been refused - which fails the same way and replaces the diagnosis
        # with its own error, so the player was told "Odmowa dostepu" no matter
        # how carefully the copy loop had named the cause.
        $applied = @()
        try {
            foreach ($change in $changes) {
                New-Item -ItemType Directory -Path (Split-Path -Parent $change.Destination) -Force | Out-Null
                try {
                    Copy-Item -LiteralPath $change.Source -Destination $change.Destination -Force
                }
                catch {
                    if (Test-M2AntivirusBlock -ErrorRecord $_) {
                        throw (New-M2AntivirusError -Path $change.Relative -ErrorRecord $_)
                    }
                    # The game still running from this folder: its exe cannot be
                    # replaced, and Windows says so only here (Ratorex, 18
                    # September - five tries, each after a 65 MB download).
                    if (Test-M2FileInUse -ErrorRecord $_) {
                        throw (New-M2FileInUseError -Path $change.Destination -Root $target)
                    }
                    if (-not (Test-M2AccessDenied -ErrorRecord $_)) { throw }
                    # One repair is worth trying before this is called a failure:
                    # a read-only or hidden destination costs nothing to clear.
                    # It is rarely the cause - Copy-Item -Force handles both on
                    # its own - so the message below is what usually ships, and
                    # it has to name which of the remaining causes this is.
                    if (-not (Repair-M2WritableFile -Path $change.Destination)) {
                        throw (New-M2AccessDeniedError -Path $change.Destination -Root $target -ErrorRecord $_)
                    }
                    Copy-Item -LiteralPath $change.Source -Destination $change.Destination -Force
                }
                $applied += $change
            }
        }
        catch {
            foreach ($change in $applied) {
                # A rollback that throws hides why the update failed, and that is
                # the one thing the player needs. Each restore stands alone.
                try {
                    $backupFile = Join-Path $backup $change.Relative
                    if (Test-Path -LiteralPath $backupFile -PathType Leaf) {
                        Copy-Item -LiteralPath $backupFile -Destination $change.Destination -Force
                    }
                    elseif (-not $change.Existed -and (Test-Path -LiteralPath $change.Destination -PathType Leaf)) {
                        Remove-Item -LiteralPath $change.Destination -Force
                    }
                }
                catch { }
            }
            throw
        }

        return [pscustomobject]@{
            Version = [string]$Component.version
            Files = $changes.Count
            Backup = $backup
            Sha256 = $actualHash
        }
    }
    finally {
        if (Test-Path -LiteralPath $tempRoot) {
            Remove-Item -LiteralPath $tempRoot -Recurse -Force
        }
    }
}

function Invoke-M2EnginePatches {
    <#
        The engine patches are shipped and then never applied. prepare-context.sh
        applies them, but it needs the pristine tree at /opt/m2port and never runs
        on a player's machine, and nothing in the rebuild path did it instead - so
        0004-private-shop-guard.patch sat in every installation while every server
        still ran the unpatched guard. That guard is `GetPart(PART_MAIN) > 2`, and
        PART_MAIN holds the vnum of the worn body armour, so OpenMyShop refused a
        stall to anyone wearing armour: every bot, and every player who tried it.

        patch(1) is not something a Windows machine has, but Docker is - a build
        cannot happen without it - so the patch runs in the same base image the
        server is compiled with. The directory is read whole rather than by name:
        adding a patch must not mean editing this function.
    #>
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    # The patches are r40250's. The mt2009 tree ships already ported and
    # patched (linux-port-mt2009/port/*.py did that where the engine source
    # is), and a hunk written for r40250's char.cpp has nothing to match in
    # it; the staged files themselves travel in the update instead.
    if ((Get-M2ServerEngine -ServerRoot $ServerRoot) -ne 'r40250') { return 0 }

    $patchDir = Join-Path $ServerRoot 'linux-port\overlays\playerbot\patches'
    $target = Join-Path $ServerRoot 'linux-port\docker\game\src\server'
    if (-not (Test-Path -LiteralPath $patchDir -PathType Container)) { return 0 }
    if (-not (Test-Path -LiteralPath $target -PathType Container)) { return 0 }
    $patches = @(Get-ChildItem -LiteralPath $patchDir -File -Filter '*.patch' | Sort-Object Name)
    if ($patches.Count -eq 0) { return 0 }

    # busybox, not the build image: ubuntu:24.04 has no patch(1) at all, and the
    # first version of this silently reported "nothing to apply" because a failed
    # dry run and a missing binary look identical. busybox is four megabytes and
    # its patch understands -N and --dry-run, which is all this needs.
    $image = 'busybox:latest'

    # Whether a patch is already in is decided by reading the target files, not
    # by asking patch(1). busybox's -N cannot tell an applied patch from a fresh
    # one: it re-applied the 18 KB core-integration patch onto a tree that
    # already had it and duplicated every declaration in it. So the tool is only
    # ever handed a patch this function has first established is absent, and
    # after that it cannot double-apply whatever the tool reports.
    # Whether a patch is already in is decided by reading the target files, not
    # by asking patch(1). busybox's -N cannot tell an applied patch from a fresh
    # one: it re-applied the 18 KB core-integration patch onto a tree that
    # already had it and duplicated every declaration in it. So the tool is only
    # ever handed a patch this function has first established is absent.
    #
    # The test is per hunk and uses the whole block the hunk produces - its
    # context lines together with its added lines. A single added line is not
    # enough to go on: 0004 adds "if (IsPolymorphed())", which char.cpp already
    # contains in three other functions.
    $pending = @()
    foreach ($p in $patches) {
        $blocks = @()
        $file = $null
        $current = $null
        foreach ($line in [IO.File]::ReadAllLines($p.FullName)) {
            if ($line.StartsWith('+++ b/')) { $file = $line.Substring(6).Trim(); continue }
            if ($line.StartsWith('@@')) {
                if ($null -ne $current -and $current.Lines.Count -gt 0) { $blocks += $current }
                $current = [pscustomobject]@{ File = $file; Lines = (New-Object Collections.Generic.List[string]) }
                continue
            }
            if ($null -eq $current) { continue }
            if ($line.StartsWith('-')) { continue }
            if ($line.StartsWith('+')) { $current.Lines.Add($line.Substring(1)); continue }
            if ($line.StartsWith(' ')) { $current.Lines.Add($line.Substring(1)); continue }
            if ($line.StartsWith('\')) { continue }
            # Anything else ends the hunk (a new "diff --git", for instance).
            if ($current.Lines.Count -gt 0) { $blocks += $current }
            $current = $null
        }
        if ($null -ne $current -and $current.Lines.Count -gt 0) { $blocks += $current }
        if ($blocks.Count -eq 0) { continue }

        $cache = @{}
        $found = 0
        $checked = 0
        foreach ($b in $blocks) {
            $path = Join-Path $target $b.File
            if (-not $cache.ContainsKey($path)) {
                $cache[$path] = if (Test-Path -LiteralPath $path -PathType Leaf) {
                    ([IO.File]::ReadAllText($path)) -replace "`r`n", "`n"
                } else { $null }
            }
            if ($null -eq $cache[$path]) { continue }
            $checked++
            $needle = ($b.Lines -join "`n")
            if ($needle.Trim().Length -eq 0) { $checked--; continue }
            if ($cache[$path].Contains($needle)) { $found++ }
        }
        if ($checked -eq 0) { continue }
        if ($found -eq $checked) { continue }          # every hunk already in
        if ($found -gt 0) {
            Write-Host (UI-Text "Latka $($p.Name) jest nalozona tylko czesciowo - pomijam ja, zeby nie pogorszyc." "The patch $($p.Name) is applied only in part - skipping it so as not to make it worse.") -ForegroundColor Yellow
            continue
        }
        $pending += $p
    }

    if ($pending.Count -eq 0) { return 0 }

    $lines = @(
        'command -v patch >/dev/null 2>&1 || { echo NOPATCH; exit 3; }',
        'cd /src || exit 4'
    )
    foreach ($p in $pending) {
        $lines += ('patch -N -p1 -i "/patches/' + $p.Name + '" >/dev/null 2>&1 || { echo "FAILED ' + $p.Name + '"; exit 5; }')
        $lines += ('echo "APPLIED ' + $p.Name + '"')
    }
    $scriptFile = Join-Path ([IO.Path]::GetTempPath()) ("m2patch-" + [Guid]::NewGuid().ToString('N') + ".sh")
    [IO.File]::WriteAllText($scriptFile, ($lines -join "`n") + "`n", (New-Object Text.UTF8Encoding($false)))

    $previous = $ErrorActionPreference
    $count = 0
    try {
        $ErrorActionPreference = 'Continue'
        $output = & docker run --rm `
            -v "${target}:/src" -v "${patchDir}:/patches:ro" -v "${scriptFile}:/apply.sh:ro" `
            --entrypoint sh $image /apply.sh 2>&1
        $exit = $LASTEXITCODE
        $text = ($output | Out-String)
        if ($exit -eq 0) {
            foreach ($line in @($output)) {
                if ("$line" -match '^APPLIED (.+)$') {
                    Write-Host (UI-Text "Nalozono latke silnika: $($Matches[1])" "Applied the engine patch: $($Matches[1])") -ForegroundColor DarkGray
                    $count++
                }
            }
        }
        else {
            # Never fail quietly here: the stalls were broken for weeks because a
            # patch that never arrived looked exactly like one already applied.
            if ($text -match 'NOPATCH') {
                Write-Host (UI-Text "Obraz $image nie zawiera narzedzia patch - latki silnika NIE zostaly nalozone." "The image $image has no patch tool - the engine patches were NOT applied.") -ForegroundColor Yellow
            }
            else {
                Write-Host (UI-Text 'Nie udalo sie nalozyc latek silnika - serwer zbuduje sie bez nich.' 'Could not apply the engine patches - the server builds without them.') -ForegroundColor Yellow
            }
            Write-Host (UI-Text 'Prywatne stragany botow i graczy moga przez to nie dzialac.' 'The private stalls of bots and players may not work because of that.') -ForegroundColor Yellow
            Write-Host $text -ForegroundColor DarkGray
        }
    }
    finally {
        $ErrorActionPreference = $previous
        Remove-Item -LiteralPath $scriptFile -Force -ErrorAction SilentlyContinue
    }
    return $count
}

function Sync-M2PlayerbotOverlay {
    <#
        The compiler never sees linux-port/overlays -- it builds from the staged
        copy under linux-port/docker/game/src/server/game/src. On a development
        machine prepare-context.sh keeps the two in step, but it needs the
        pristine engine tree at /opt/m2port, which the distribution deliberately
        does not contain, so on a player's machine nothing did.

        That is how 1.23.2 shipped a manager that included a header no player
        had: every build stopped at "playerbot_types.h: No such file or
        directory", and 1.22.4 had already broken the same way on a stale
        playerbot_manager.h. Copying the whole overlay directory - rather than a
        hand-maintained list of filenames - is what keeps the next new source
        file from repeating it.
    #>
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $source = Join-Path $ServerRoot 'linux-port\overlays\playerbot\src\game\src'
    $staged = Join-Path $ServerRoot 'linux-port\docker\game\src\server\game\src'
    if (-not (Test-Path -LiteralPath $source -PathType Container)) { return 0 }
    if (-not (Test-Path -LiteralPath $staged -PathType Container)) { return 0 }

    $copied = 0
    foreach ($file in Get-ChildItem -LiteralPath $source -File) {
        $destination = Join-Path $staged $file.Name
        $current = $null
        if (Test-Path -LiteralPath $destination -PathType Leaf) {
            $current = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        }
        $incoming = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        if ($current -ne $incoming) {
            Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
            $copied++
        }
    }

    # The engine Makefile in the build context is patched by prepare-context.sh,
    # which never runs on a player's machine - so the wildcard that makes a new
    # .cpp compile itself would never reach them. Repair that one line here
    # instead of shipping the whole Makefile over theirs.
    $makefile = Join-Path $ServerRoot 'linux-port\docker\game\src\server\game\src\Makefile'
    if (Test-Path -LiteralPath $makefile -PathType Leaf) {
        $text = Get-Content -LiteralPath $makefile -Raw
        if ($text -match '(?m)^CPPFILE \+= playerbot_manager\.cpp\s*$') {
            $text = $text -replace '(?m)^CPPFILE \+= playerbot_manager\.cpp\s*$',
                'CPPFILE += $(wildcard playerbot_*.cpp)'
            [IO.File]::WriteAllText($makefile, $text)
            $copied++
        }
    }

    # The two data files the Dockerfile COPYs from the build context. The
    # update builds the image straight after the files land - before
    # start-server.ps1 runs and stages them - and a COPY of a file that is not
    # there fails the whole build: "special_item_group.moonlight.txt: not
    # found", five players in the first ten minutes of 1.29.1.
    # r40250 only. The mt2009 package ships docker\game\*.txt itself, and its
    # serverfiles\ copy was the full package's, which no update refreshes: this
    # copy put it back over an operator's edit of the chest file at every start
    # and update (Piciu713, 26 September - "the original from 16.09"). An edit
    # that has to last goes into special_item_group.moonlight.custom.txt.
    $dataPairs = @()
    if ((Get-M2ServerEngine -ServerRoot $ServerRoot) -eq 'r40250') {
        $dataPairs = @(
            @{ From = 'linux-port\overlays\playerbot\serverfiles\special_item_group.moonlight.txt';
               To   = 'linux-port\docker\game\special_item_group.moonlight.txt' },
            @{ From = 'linux-port\overlays\playerbot\serverfiles\mob_drop_item.m3.append.txt';
               To   = 'linux-port\docker\game\mob_drop_item.m3.append.txt' })
    }
    foreach ($pair in $dataPairs) {
        $dataSource = Join-Path $ServerRoot $pair.From
        $dataStaged = Join-Path $ServerRoot $pair.To
        if (-not (Test-Path -LiteralPath $dataSource -PathType Leaf)) { continue }
        $dataStagedHash = $null
        if (Test-Path -LiteralPath $dataStaged -PathType Leaf) {
            $dataStagedHash = (Get-FileHash -LiteralPath $dataStaged -Algorithm SHA256).Hash
        }
        if ($dataStagedHash -ne (Get-FileHash -LiteralPath $dataSource -Algorithm SHA256).Hash) {
            Copy-Item -LiteralPath $dataSource -Destination $dataStaged -Force
            $copied++
        }
    }

    # The migrate container mounts linux-port/docker/mariadb/playerbot, so the
    # seed it actually applies is a copy of the overlay's - staged there by
    # prepare-context.sh, which never runs on a player's machine. That copy was
    # from August: the 1000-character registry and the additive seeding were in
    # every update package and reached nobody, because nothing replaced the file
    # the container reads.
    $seedSource = Join-Path $ServerRoot 'linux-port\overlays\playerbot\sql\playerbots_seed.sql'
    $seedStaged = Join-Path $ServerRoot 'linux-port\docker\mariadb\playerbot\playerbots_seed.sql'
    if ((Get-M2ServerEngine -ServerRoot $ServerRoot) -ne 'r40250') {
        # mt2009's seed is rendered from the overlay's by port/seedify.py and
        # ships where the migrate container mounts it; the overlay's own would
        # write columns this schema does not have. Nothing to copy over it.
    }
    elseif ((Test-Path -LiteralPath $seedSource -PathType Leaf) -and
        (Test-Path -LiteralPath (Split-Path -Parent $seedStaged) -PathType Container)) {
        $seedStagedHash = $null
        if (Test-Path -LiteralPath $seedStaged -PathType Leaf) {
            $seedStagedHash = (Get-FileHash -LiteralPath $seedStaged -Algorithm SHA256).Hash
        }
        if ($seedStagedHash -ne (Get-FileHash -LiteralPath $seedSource -Algorithm SHA256).Hash) {
            Copy-Item -LiteralPath $seedSource -Destination $seedStaged -Force
            $copied++
        }
    }
    elseif (-not (Test-Path -LiteralPath $seedSource -PathType Leaf)) {
        # Silent before: a missing source left the staged copy as it was, and if
        # that was missing too the start failed a second after the database came
        # up, with "exit 1" and nothing else. Say which file, and where.
        Write-Warning (UI-Text "Brak $seedSource - playerbots_seed.sql nie zostal odswiezony." "$seedSource is missing - playerbots_seed.sql was not refreshed.")
    }

    # The panel's build context, the same way. start-server.ps1 stages these
    # too, but below its -IdentityOnly return - and this function is what the
    # launcher's own build path runs, so a player who only ever clicked GRAJ or
    # AKTUALIZUJ had a panel built from whatever VERSION and CHANGELOG the
    # installer left there: ten releases later the classic panel still said
    # 1.29.0 and offered "Zobacz, co przynosi" for a version it was running.
    # Rebuilding the image could not help, because the file it bakes in was
    # the stale one.
    foreach ($pair in @(
            @{ From = 'VERSION';                    To = 'linux-port\docker\panel\app\VERSION' },
            @{ From = 'CHANGELOG.md';               To = 'linux-port\docker\panel\app\CHANGELOG.md' },
            @{ From = 'files\admin_panel.py';       To = 'linux-port\docker\panel\app\admin_panel.py' },
            @{ From = 'files\items.json';           To = 'linux-port\docker\panel\app\items.json' },
            @{ From = 'files\favicon.png';          To = 'linux-port\docker\panel\app\favicon.png' },
            @{ From = 'files\web_admin_schema.sql'; To = 'linux-port\docker\panel\schema\web_admin_schema.sql' }) +
            # The two quests from files\ on r40250 only. The mt2009 package ships
            # its own docker\game\quest\web_admin.quest - the live rates, respawn
            # speed and count, difficulty and Auto Lowy commands - and this put
            # r40250's over it at every update, so none of those ran on an
            # install made by the launcher (27 September).
            $(if ((Get-M2ServerEngine -ServerRoot $ServerRoot) -eq 'r40250') { @(
            @{ From = 'files\web_admin.quest';      To = 'linux-port\docker\game\quest\web_admin.quest' },
            @{ From = 'files\high_risk.quest';      To = 'linux-port\docker\game\quest\high_risk.quest' }) } else { @() })) {
        $panelSource = Join-Path $ServerRoot $pair.From
        $panelStaged = Join-Path $ServerRoot $pair.To
        if (-not (Test-Path -LiteralPath $panelSource -PathType Leaf)) { continue }
        $panelParent = Split-Path -Parent $panelStaged
        if (-not (Test-Path -LiteralPath $panelParent -PathType Container)) {
            New-Item -ItemType Directory -Path $panelParent -Force | Out-Null
        }
        $panelStagedHash = $null
        if (Test-Path -LiteralPath $panelStaged -PathType Leaf) {
            $panelStagedHash = (Get-FileHash -LiteralPath $panelStaged -Algorithm SHA256).Hash
        }
        if ($panelStagedHash -ne (Get-FileHash -LiteralPath $panelSource -Algorithm SHA256).Hash) {
            Copy-Item -LiteralPath $panelSource -Destination $panelStaged -Force
            $copied++
        }
    }
    $staticSource = Join-Path $ServerRoot 'files\static'
    $staticStaged = Join-Path $ServerRoot 'linux-port\docker\panel\app\static'
    if (Test-Path -LiteralPath $staticSource -PathType Container) {
        foreach ($asset in Get-ChildItem -LiteralPath $staticSource -Recurse -File) {
            $relative = $asset.FullName.Substring($staticSource.Length).TrimStart('\')
            $assetStaged = Join-Path $staticStaged $relative
            $assetParent = Split-Path -Parent $assetStaged
            if (-not (Test-Path -LiteralPath $assetParent -PathType Container)) {
                New-Item -ItemType Directory -Path $assetParent -Force | Out-Null
            }
            $assetHash = $null
            if (Test-Path -LiteralPath $assetStaged -PathType Leaf) {
                $assetHash = (Get-FileHash -LiteralPath $assetStaged -Algorithm SHA256).Hash
            }
            if ($assetHash -ne (Get-FileHash -LiteralPath $asset.FullName -Algorithm SHA256).Hash) {
                Copy-Item -LiteralPath $asset.FullName -Destination $assetStaged -Force
                $copied++
            }
        }
    }

    return $copied
}

function Set-M2PlayerbotsVersionEnvironment {
    <#
        The advanced panel reports the Playerbots release it is looking at from
        PLAYERBOTS_VERSION, which compose takes from M2_PLAYERBOTS_VERSION or a
        default written into docker-compose.yml. Nothing on a player's machine
        ever set the variable, so the panel reported whatever the default was
        when that compose file was last touched - 1.30.29 for ten releases.
        Compose reads the process environment before the .env file, so the
        launcher can say what VERSION on disk says without touching a file
        it must never rewrite. A value an operator set by hand is left alone.
    #>
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    if ($env:M2_PLAYERBOTS_VERSION) { return }
    $versionFile = Join-Path $ServerRoot 'VERSION'
    if (-not (Test-Path -LiteralPath $versionFile -PathType Leaf)) { return }
    $version = ([IO.File]::ReadAllText($versionFile)).Trim()
    if ($version -match '^\d+\.\d+\.\d+$') { $env:M2_PLAYERBOTS_VERSION = $version }
}

function Get-M2SanitizedEnv {
    param([Parameter(Mandatory = $true)][string]$EnvPath)

    if (-not (Test-Path -LiteralPath $EnvPath -PathType Leaf)) { return @() }
    return @(Get-Content -LiteralPath $EnvPath | ForEach-Object {
        if ($_ -match '^([A-Za-z_][A-Za-z0-9_]*(?:PASSWORD|SECRET|TOKEN|PRIVATE_KEY)[A-Za-z0-9_]*)=(.*)$') {
            "$($Matches[1])=<redacted>"
        }
        else { $_ }
    })
}

function Protect-M2LogContent {
    param([AllowEmptyString()][string]$Text)
    if (-not $Text) { return '' }

    $safe = $Text
    $safe = [Regex]::Replace(
        $safe,
        '(?im)(password|passwd|secret|token|private[_-]?key)(\s*[:=]\s*)([^\s,;]+)',
        '$1$2<redacted>')
    $safe = [Regex]::Replace(
        $safe,
        '(?im)(M2_[A-Z0-9_]*(?:PASSWORD|SECRET|TOKEN)[A-Z0-9_]*=).*$',
        '$1<redacted>')
    # On the first panel start the generated administrator password is printed
    # on a line of its own, below a heading. It has no "password=" prefix, so
    # the generic key/value rules above cannot recognize it. In capitals only,
    # as the panel prints it: the launcher's English "Admin panel password:"
    # line carries its password itself.
    $safe = [Regex]::Replace(
        $safe,
        '(?s)(ADMIN PANEL PASSWORD[^\r\n]*\r?\n\s*\r?\n\s*)[^\r\n]+',
        '$1<redacted>')
    # And the launcher's own lines, which are Polish and which the rules above
    # could not read: start-server.ps1 prints a freshly made panel password as
    # "Haslo do panelu administracyjnego: <it>", under a heading "HASLO DO
    # PANELU WWW" with the password alone a line or two below, and the
    # password button prints it below "Haslo do panelu WWW (...):". The GUI's
    # session log keeps every such line, and pattsito's bundle carried his
    # panel password onto a public channel on 23 September. [ \t] and not \s
    # where the value must be on the same line: \s crosses into the next
    # line's timestamp. A heading is a line that ends in ")" or ":" - the line
    # that carries the password itself ends in the password - and .NET's $
    # stands before \n alone, so a CRLF line ends at \r?$. The same headings
    # in a launcher set to English say "web panel password" (28 September),
    # and the password button's second print of it, "Gotowe. Zaloguj sie
    # haslem:" / "Done. Log in with the web panel password:", is one too.
    $safe = [Regex]::Replace(
        $safe,
        '(?im)((?:has[lł]o do panelu|zaloguj si[eę] has[lł]em|web panel password)[^\r\n]*[):][ \t]*\r?\n(?:[ \t]*\r?\n)*(?:\d{4}-\d\d-\d\d \d\d:\d\d:\d\d  )?[ \t]*)(\S+)(?=[ \t]*\r?$)',
        '$1<redacted>')
    $safe = [Regex]::Replace(
        $safe,
        '(?im)(\bhas[lł]o\b[^\r\n:]{0,80}:[ \t]*)([^\s,;]+)',
        '$1<redacted>')
    # A friend's account line from the COOP actions: "login X, haslo Y".
    $safe = [Regex]::Replace(
        $safe,
        '(?i)(\bhas[lł]o[ \t]+)([^\s,;:)]{6,})',
        '$1<redacted>')
    $safe = [Regex]::Replace(
        $safe,
        '(?i)(\b(?:mysql|mariadb)://[^:\s/]+:)[^@\s/]+(@)',
        '$1<redacted>$2')
    return $safe
}

function Protect-M2SessionLogLine {
    # The GUI's session log takes the actions' output a line at a time, and a
    # player posts that file on the Discord as it is: only the support bundle's
    # copy went through Protect-M2LogContent, and Hyper96's launcher-*.log of
    # 28 September carried the panel password start-server.ps1 had just made.
    # One line cannot show a heading with the password a line or two below it,
    # so -State remembers the heading and the next line that is not empty is
    # the password. A heading is a line that begins with "Haslo do panelu" and
    # ends in ")" or ":", or the panel's own "ADMIN PANEL PASSWORD"; the line
    # that carries a password itself ends in the password. Before either may
    # stand the session log's timestamp and a container's "panel-1  | ", which
    # is all an empty line of `docker compose logs` consists of. In English
    # the headings begin with "Web panel password", and the password button's
    # "Gotowe. Zaloguj sie haslem:" is "Done. Log in with the web panel
    # password:" - both of them headings of a password on the next line.
    param(
        [AllowEmptyString()][string]$Text,
        [hashtable]$State
    )
    if (-not $Text) { return $Text }
    $body = [Regex]::Match($Text, '^((?:\d{4}-\d\d-\d\d \d\d:\d\d:\d\d  )?[ \t]*(?:[\w.-]+[ \t]+\|[ \t]?)?[ \t]*)(.*)$')
    if ($State -and $State['redactNext']) {
        if (-not $body.Groups[2].Value.Trim()) { return $Text }
        $State['redactNext'] = $false
        return $body.Groups[1].Value + '<redacted>'
    }
    # The panel's own heading is matched in capitals only: the English
    # start-server line "Admin panel password: <it>" carries its password
    # itself, and taken for the heading it masked the line after it.
    if ($State -and ($body.Groups[2].Value -match '(?i)^(?:has[lł]o do panelu|web panel password|(?:gotowe\. )?zaloguj si[eę] has[lł]em|(?:done\. )?log in with the web panel password).*[):][ \t]*$' -or
            $body.Groups[2].Value -cmatch 'ADMIN PANEL PASSWORD')) {
        $State['redactNext'] = $true
    }
    return (Protect-M2LogContent -Text $Text)
}

function Protect-M2LogFile {
    # An action's own output file (launcher-logs\action-*.out.log) gets posted
    # too. The child writes it through a redirect, so it is masked in place once
    # the child has exited, whole, so the headings are seen with what follows
    # them. Rewritten only when something was masked.
    param([string]$Path)
    if (-not $Path -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) { return }
    try {
        $text = [IO.File]::ReadAllText($Path)
        $safe = Protect-M2LogContent -Text $text
        if ($safe -cne $text) {
            [IO.File]::WriteAllText($Path, $safe, [Text.UTF8Encoding]::new($false))
        }
    }
    catch { }
}

function Invoke-M2CapturedCommand {
    param(
        [Parameter(Mandatory = $true)][string]$OutputPath,
        [Parameter(Mandatory = $true)][scriptblock]$Command
    )

    $previousPreference = $ErrorActionPreference
    try {
        # Windows PowerShell 5.1 turns some native stderr lines into error
        # records. Diagnostics should capture those lines, not abort at them.
        $ErrorActionPreference = 'Continue'
        $text = Protect-M2LogContent -Text (& $Command 2>&1 | Out-String)
        [IO.File]::WriteAllText($OutputPath, $text, [Text.UTF8Encoding]::new($false))
    }
    catch {
        [IO.File]::WriteAllText($OutputPath, ($_ | Out-String), [Text.UTF8Encoding]::new($false))
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
}

function New-M2SupportBundle {
    param(
        [Parameter(Mandatory = $true)][string]$ServerRoot,
        # Text files the caller made, by name - what another module knows
        # (the disk report is the diagnostics module's), without this one
        # reaching into it.
        [hashtable]$ExtraFiles = @{}
    )

    $root = [IO.Path]::GetFullPath($ServerRoot).TrimEnd('\')
    $outputDir = Join-Path $root 'support-bundles'
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $work = Join-Path $outputDir ('.work-' + $stamp)
    $zip = Join-Path $outputDir ("metin2-support-$stamp.zip")
    New-Item -ItemType Directory -Path $work -Force | Out-Null

    try {
        # A bundle collected after Docker Desktop has been shut down contains
        # nothing but connection errors where the container logs should be, and
        # used to be reported as a success. Say so at the top of the file instead,
        # so nobody spends an evening reading an empty report.
        $dockerUp = Test-M2DockerRunning
        $summary = @(
            (UI-Text 'Metin2 Playerbots - pakiet diagnostyczny' 'Metin2 Playerbots - diagnostic bundle'),
            (UI-Text "Utworzono: $([DateTime]::Now.ToString('s'))" "Made: $([DateTime]::Now.ToString('s'))"),
            "PowerShell: $($PSVersionTable.PSVersion)",
            "Windows: $([Environment]::OSVersion.VersionString)",
            (UI-Text "Folder serwera: $([IO.Path]::GetFileName($root))" "Server folder: $([IO.Path]::GetFileName($root))"),
            (UI-Text "Silnik Dockera: $(if ($dockerUp) { 'dziala' } else { 'ZATRZYMANY' })" "Docker engine: $(if ($dockerUp) { 'running' } else { 'STOPPED' })")
        )
        if (-not $dockerUp) {
            $summary += @(
                '',
                (UI-Text 'PACZKA NIEPELNA. Docker byl wylaczony, wiec nie ma w niej logow' 'INCOMPLETE BUNDLE. Docker was off, so it holds no logs of the'),
                (UI-Text 'kontenerow ani stanu uslug - a to zwykle jedyne miejsce, gdzie' 'containers and no state of the services - which are usually the only'),
                (UI-Text 'widac przyczyne problemu.' 'place that shows what caused the problem.'),
                (UI-Text 'Uruchom Docker (przycisk URUCHOM DOCKER), odtworz problem' 'Start Docker (the START DOCKER button), make the problem happen again'),
                (UI-Text 'i zbierz paczke ponownie.' 'and collect the bundle again.')
            )
        }
        $summary = $summary -join [Environment]::NewLine
        [IO.File]::WriteAllText((Join-Path $work 'summary.txt'), $summary, [Text.UTF8Encoding]::new($false))
        foreach ($extra in @($ExtraFiles.Keys)) {
            $extraName = [IO.Path]::GetFileName([string]$extra)
            if (-not $extraName) { continue }
            [IO.File]::WriteAllText((Join-Path $work $extraName), [string]$ExtraFiles[$extra], [Text.UTF8Encoding]::new($false))
        }

        $versionFile = Join-Path $root 'VERSION'
        if (Test-Path -LiteralPath $versionFile -PathType Leaf) {
            Copy-Item -LiteralPath $versionFile -Destination (Join-Path $work 'server-version.txt') -Force
        }

        $envPath = Join-Path $root 'linux-port\docker\.env'
        # An empty array returned from a function arrives here as $null, and
        # WriteAllLines refuses it: a player with no .env could not even send
        # the logs that would have shown it.
        [string[]]$safeEnv = @(Get-M2SanitizedEnv -EnvPath $envPath)
        if ($safeEnv.Count -eq 0) { $safeEnv = @((UI-Text '(brak pliku .env)' '(no .env file)')) }
        [IO.File]::WriteAllLines((Join-Path $work 'environment-redacted.txt'), $safeEnv, [Text.UTF8Encoding]::new($false))

        $composeDir = Join-Path $root 'linux-port\docker'
        $composeFile = Join-Path $composeDir 'docker-compose.yml'
        Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'docker-version.txt') -Command { docker version }
        Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'docker-info.txt') -Command { docker info }
        # Every volume and when it was made. A world that "vanished with the
        # update" was a database MariaDB had initialized from nothing the
        # evening before (Piciu97, 27 September), and only the volume's age
        # says whether it was removed, and when. No space in the format: an
        # argument with one reaches docker quoted.
        Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'docker-volumes.txt') -Command {
            $volumeNames = @(docker volume ls -q)
            if ($volumeNames.Count -gt 0) { docker volume inspect --format '{{.Name}};{{.CreatedAt}}' $volumeNames }
        }
        if (Test-Path -LiteralPath $composeFile -PathType Leaf) {
            Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'compose-ps.txt') -Command {
                docker compose --project-directory $composeDir -f $composeFile ps -a
            }
            Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'compose-logs.txt') -Command {
                docker compose --project-directory $composeDir -f $composeFile logs --no-color --tail 800
            }
            # The game container on its own, with a window of its own: the
            # shared 800 lines were fifty seconds of watchdog and MariaDB
            # "Aborted connection" chatter on a world whose core was dying
            # every ninety seconds, and the one line that mattered - the
            # supervisor's CORE DIED with the backtrace under it - had
            # scrolled out before the bundle was made.
            Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'game-container-logs.txt') -Command {
                docker compose --project-directory $composeDir -f $composeFile logs --no-color --tail 6000 game
            }
            # Filtered on the PowerShell side: 2.0.11 piped this through grep,
            # which Windows PowerShell does not have, and every bundle carried
            # a CommandNotFoundException where the supervisor's lines belonged.
            Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'game-supervise.txt') -Command {
                docker compose --project-directory $composeDir -f $composeFile logs --no-color --tail 200000 game 2>&1 |
                    Select-String -Pattern 'supervise', 'CORE DIED', 'fatal signal', 'crash' -SimpleMatch |
                    Select-Object -Last 400 | ForEach-Object { $_.Line }
            }
            # The core's syslog never reaches the container log - only syserr
            # does - so a bundle sent about "the bots walk to the wrong portal"
            # carried nothing about where any bot was going. The travel,
            # portal, navigation and watchdog lines of the last hours, and the
            # goal and status lines that say what each bot wanted, filtered
            # here rather than shipped whole (a busy day's syslog is hundreds
            # of megabytes). Bots only; a player's chat is not in it.
            # No double quotes anywhere inside the sh command - not round $f,
            # not round the grep pattern: Windows PowerShell 5.1 wraps a native
            # argument in double quotes without escaping the ones it already
            # holds, so the first embedded quote ended the argument and the
            # file came back empty on the machine it was made for (1.30.40).
            # Hence -e per pattern instead of one quoted alternation.
            # Every core, not game1 alone. Since 2.0.8 Shinsoo lives on `first'
            # and Jinno on `game2', and a bundle about "the bots stand at level
            # five" carried nothing about the two cores they stood on. One file
            # per core, and the crash traces m2-supervise keeps beside the
            # syserr (crash-<stamp>.txt, 2.0.8) - the only way to see where a
            # player's core died.
            # The other channels' cores too, when the server has run them - the
            # second (M2_PLAYERBOT_CH2) and the fresh cohort's third and fourth
            # (M2_PLAYERBOT_FRESH_CHANNELS): their files are named chN-<core>.
            $coreKeys = @('first', 'game1', 'game2')
            # A command that prints nothing gives $null, and [string] of that
            # is $null too in Windows PowerShell 5.1 - so .Trim() on it threw
            # "You cannot call a method on a null-valued expression" and the
            # whole bundle failed on every server without a second channel
            # (2.0.76: archonek, Urtopy). Joined and asked, never called.
            $channelProbe = $null
            try { $channelProbe = docker compose --project-directory $composeDir -f $composeFile exec -T game sh -c 'ls /opt/metin2/var/channel2/game1/syslog /opt/metin2/var/channel3/game1/syslog /opt/metin2/var/channel4/game1/syslog 2>/dev/null' } catch { $channelProbe = $null }
            $channelsSeen = "$(@($channelProbe) -join ' ')"
            foreach ($n in 2..4) {
                if ($channelsSeen.Contains('/channel' + $n + '/game1/syslog')) {
                    $coreKeys += @(('ch' + $n + '-first'), ('ch' + $n + '-game1'), ('ch' + $n + '-game2'))
                }
            }
            foreach ($core in $coreKeys) {
                $coreDir = '/opt/metin2/var/channel1/' + $core
                if ($core -match '^ch([2-4])-(.+)$') { $coreDir = '/opt/metin2/var/channel' + $Matches[1] + '/' + $Matches[2] }
                Invoke-M2CapturedCommand -OutputPath (Join-Path $work ('playerbot-syslog-' + $core + '.txt')) -Command {
                    docker compose --project-directory $composeDir -f $composeFile exec -T game sh -c `
                        ('for f in ' + $coreDir + '/log/*/syslog.* ' + $coreDir + '/syslog; do [ -f $f ] && tail -n 400000 $f; done 2>/dev/null | grep -a -e PLAYERBOT_WORLD -e PLAYERBOT_PORTAL -e PLAYERBOT_NAV -e PLAYERBOT_WATCHDOG -e PLAYERBOT_GOAL -e PLAYERBOT_LOAD -e PLAYERBOT_SHOP -e PLAYERBOT_TOWN -e PLAYERBOT_DEPARTURE -e PLAYERBOT_HORSE -e PLAYERBOT_MONKEY -e PLAYERBOT_AUTH -e PLAYERBOT_CHANNEL -e PLAYERBOT_SERVICE -e PLAYERBOT_CONFIG -e PLAYERBOT_EVENT -e PLAYERBOT_LIFE -e PLAYERBOT_CHEST -e PLAYERBOT_COMBAT -e PLAYERBOT_STOCK -e PLAYERBOT_GUILD -e PLAYERBOT_TOWER -e PLAYERBOT_CATACOMB -e PLAYERBOT_ISHOP -e PLAYERBOT_OFFLINE -e PLAYERBOT_MARKET -e PLAYERBOT_BAG -e INVENTORY_ARRANGE -e PLAYERBOT_AI -e PLAYERBOT_ECONOMY -e PLAYERBOT_PVP -e PLAYERBOT_LOOT -e PLAYERBOT_MOOD -e PLAYERBOT_PERSONA -e PLAYERBOT_ANTIPK -e PLAYERBOT_MERC -e PLAYERBOT_LPP -e PLAYERBOT_ALCHEMIST -e PLAYERBOT_METIN:.detector -e PLAYERBOT_BONUS -e PLAYERBOT_PARTY:.accepted -e PLAYERBOT_PARTY:.asked -e PLAYERBOT_LURE:.order -e PLAYERBOT_LURE:.pack.handed -e PLAYERBOT_LURE:.waiting -e PLAYERBOT_CONV -e PLAYERBOT_CHAT -e PLAYERBOT_SUMMON -e PLAYERBOT_SIDEKICK -e CAPE_PULL -e FLEA_MARKET -e QUEST_ITEM -e GMPANEL -e GM_PROFILE -e autospawn | tail -n 40000')
                }
                Invoke-M2CapturedCommand -OutputPath (Join-Path $work ('syserr-' + $core + '.txt')) -Command {
                    docker compose --project-directory $composeDir -f $composeFile exec -T game sh -c `
                        ('tail -n 3000 ' + $coreDir + '/syserr 2>/dev/null')
                }
                Invoke-M2CapturedCommand -OutputPath (Join-Path $work ('playerbot-status-' + $core + '.tsv')) -Command {
                    docker compose --project-directory $composeDir -f $composeFile exec -T game sh -c `
                        ('cat ' + $coreDir + '/playerbot_status.tsv 2>/dev/null')
                }
                Invoke-M2CapturedCommand -OutputPath (Join-Path $work ('crash-' + $core + '.txt')) -Command {
                    docker compose --project-directory $composeDir -f $composeFile exec -T game sh -c `
                        ('ls -la ' + $coreDir + '/crash*.txt 2>/dev/null; for f in $(ls -t ' + $coreDir + '/crash*.txt 2>/dev/null | head -n 5); do echo; echo === $f; cat $f; done')
                }
                # A player's login lands on a channel core: the key it brought,
                # what the db core answered, and a FULL or ALREADY refusal are
                # all syslog lines, and none of the patterns above matched them.
                # "Wisi na ekranie logowania" (sizowski, 2.0.11) could not be read
                # from a bundle without these.
                Invoke-M2CapturedCommand -OutputPath (Join-Path $work ('login-' + $core + '.txt')) -Command {
                    docker compose --project-directory $composeDir -f $composeFile exec -T game sh -c `
                        ('for f in ' + $coreDir + '/log/*/syslog.* ' + $coreDir + '/syslog; do [ -f $f ] && tail -n 400000 $f; done 2>/dev/null | grep -a -e LOGIN -e Login -e login -e AUTH -e CHANNEL_STATUS -e P2P -e ALREADY -e FULL | grep -a -v -e PLAYERBOT_AUTH -e playerbot_ | tail -n 2000')
                }
            }
            # The auth core answers the client's first screen. Its syserr and
            # every login it handled, because "Logowanie..." that never ends
            # is decided here or in the db core, and 2.0.11 collected neither.
            Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'syserr-auth.txt') -Command {
                docker compose --project-directory $composeDir -f $composeFile exec -T game sh -c `
                    'tail -n 2000 /opt/metin2/var/auth/syserr 2>/dev/null'
            }
            Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'login-auth.txt') -Command {
                docker compose --project-directory $composeDir -f $composeFile exec -T game sh -c `
                    'for f in /opt/metin2/var/auth/log/*/syslog.* /opt/metin2/var/auth/syslog; do [ -f $f ] && tail -n 200000 $f; done 2>/dev/null | grep -a -v -e playerbot_ -e PLAYERBOT_ | tail -n 2000'
            }
            Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'login-db.txt') -Command {
                docker compose --project-directory $composeDir -f $composeFile exec -T game sh -c `
                    'for f in /opt/metin2/var/db/log/*/syslog.* /opt/metin2/var/db/syslog; do [ -f $f ] && tail -n 400000 $f; done 2>/dev/null | grep -a -e LOGIN -e Login -e login -e AUTH -e ALREADY -e KEY -e PLAYER_LOAD | grep -a -v -e playerbot_ | tail -n 2000'
            }
            # The db core writes its own syserr (a failed query, a table the
            # game asked for and the schema lacks).
            Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'syserr-db.txt') -Command {
                docker compose --project-directory $composeDir -f $composeFile exec -T game sh -c `
                    'tail -n 1000 /opt/metin2/var/db/syserr 2>/dev/null'
            }
            Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'compose-services.txt') -Command {
                docker compose --project-directory $composeDir -f $composeFile config --services
            }
        }

        $launcherLogDir = Join-Path $root 'launcher-logs'
        if (Test-Path -LiteralPath $launcherLogDir -PathType Container) {
            $logOutput = Join-Path $work 'launcher-logs'
            New-Item -ItemType Directory -Path $logOutput -Force | Out-Null
            # The day's launcher log of the last three days whatever else was
            # written since, and the five newest of the rest. Five of any kind
            # let one afternoon's action logs push out the day before - the
            # day that held what the world lost (Piciu97, 27 September).
            $allLogs = @(Get-ChildItem -LiteralPath $launcherLogDir -File -Filter '*.log' |
                Sort-Object LastWriteTime -Descending)
            $dailyLogs = @($allLogs | Where-Object { $_.Name -like 'launcher-*.log' } | Select-Object -First 3)
            $otherLogs = @($allLogs | Where-Object { $_.Name -notlike 'launcher-*.log' } | Select-Object -First 5)
            @($dailyLogs + $otherLogs) |
                ForEach-Object {
                    $safeLog = Protect-M2LogContent -Text (Get-Content -LiteralPath $_.FullName -Raw -ErrorAction SilentlyContinue)
                    [IO.File]::WriteAllText((Join-Path $logOutput $_.Name), $safeLog, [Text.UTF8Encoding]::new($false))
                }
        }

        # Konfiguracja launchera, przepuszczona przez ten sam filtr co logi.
        #
        # Bez niej nie wiadomo, z ktorego manifestu ten launcher czyta - a linie
        # 1.x i mt2009 maja osobne. Gdy SIZOWSKI zglosil, ze nie moze
        # zainstalowac aktualizacji, bundle nie niosl tego pliku i trzeba bylo
        # zgadywac miedzy dwiema przyczynami, ktore wygladaja w logu identycznie:
        # zly adres manifestu i manifest, ktorego nikt nie przestawil.
        $launcherConfig = Join-Path $root '.m2launcher.json'
        if (Test-Path -LiteralPath $launcherConfig -PathType Leaf) {
            $safeConfig = Protect-M2LogContent -Text (Get-Content -LiteralPath $launcherConfig -Raw -ErrorAction SilentlyContinue)
            [IO.File]::WriteAllText((Join-Path $work 'launcher-config-redacted.json'), $safeConfig, [Text.UTF8Encoding]::new($false))
        }

        Compress-Archive -Path (Join-Path $work '*') -DestinationPath $zip -CompressionLevel Optimal -Force
        return $zip
    }
    finally {
        if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
    }
}

function Test-M2SupportUploadUrl {
    param([Parameter(Mandatory = $true)][string]$Url)

    $uri = $null
    if (-not [Uri]::TryCreate($Url, [UriKind]::Absolute, [ref]$uri)) { return $false }
    if ($uri.Scheme -ne 'https') { return $false }
    # discord.gg links are invitations, not webhooks - posting to one always fails.
    if ($uri.Host -ieq 'discord.gg') { return $false }
    if ($uri.Host -ieq 'discord.com' -or $uri.Host -ieq 'discordapp.com') {
        return $uri.AbsolutePath.StartsWith('/api/webhooks/', [StringComparison]::OrdinalIgnoreCase)
    }
    return $true
}

function Get-M2SupportSettings {
    param(
        [Parameter(Mandatory = $true)]$Config,
        [switch]$NoRemote
    )

    $result = [pscustomobject]@{
        UploadUrl  = ''
        ContactUrl = $script:M2_DEFAULT_SUPPORT_CONTACT
        Source     = 'none'
    }

    # A local setting always wins, so a tester can redirect the button without
    # touching the manifest everybody else reads.
    $local = ''
    if ($null -ne $Config.PSObject.Properties['supportUploadUrl']) { $local = [string]$Config.supportUploadUrl }
    if ($local -and (Test-M2SupportUploadUrl -Url $local)) {
        $result.UploadUrl = $local
        $result.Source = 'config'
        return $result
    }

    if ($NoRemote) { return $result }

    # Otherwise read it from the update manifest. Keeping the address there means
    # it can be rotated by editing one file on GitHub - no new release, no
    # reinstall, and a leaked webhook can be revoked the same way.
    $manifest = $null
    try {
        $source = ''
        if ($null -ne $Config.PSObject.Properties['manifestUrl']) { $source = [string]$Config.manifestUrl }
        if (-not $source) { return $result }
        $manifest = Get-M2UpdateManifest -Source $source -TimeoutSec 10
    }
    catch { return $result }

    if ($null -eq $manifest -or $null -eq $manifest.PSObject.Properties['support']) { return $result }
    $support = $manifest.support
    if ($null -eq $support) { return $result }

    if ($null -ne $support.PSObject.Properties['contactUrl']) {
        $contact = [string]$support.contactUrl
        if ($contact) { $result.ContactUrl = $contact }
    }
    if ($null -ne $support.PSObject.Properties['uploadUrl']) {
        $upload = [string]$support.uploadUrl
        if ($upload -and (Test-M2SupportUploadUrl -Url $upload)) {
            $result.UploadUrl = $upload
            $result.Source = 'manifest'
        }
    }
    return $result
}

function Send-M2SupportBundle {
    param(
        [Parameter(Mandatory = $true)][string]$BundlePath,
        [Parameter(Mandatory = $true)][string]$UploadUrl
    )

    $uri = $null
    if (-not [Uri]::TryCreate($UploadUrl, [UriKind]::Absolute, [ref]$uri) -or $uri.Scheme -ne 'https') {
        throw (UI-Text 'Adres wysyłki logów musi używać HTTPS.' 'The address the logs are sent to has to use HTTPS.')
    }
    if ($uri.Host -ieq 'discord.gg') {
        throw (UI-Text 'To jest zaproszenie na serwer Discord, a nie webhook. Adres webhooka wygląda tak: https://discord.com/api/webhooks/...' 'That is an invitation to a Discord server, not a webhook. A webhook address looks like this: https://discord.com/api/webhooks/...')
    }
    if (-not (Test-Path -LiteralPath $BundlePath -PathType Leaf)) {
        throw (UI-Text "Nie znaleziono paczki: $BundlePath" "Bundle not found: $BundlePath")
    }

    $isDiscordWebhook =
        ($uri.Host -ieq 'discord.com' -or $uri.Host -ieq 'discordapp.com') -and
        $uri.AbsolutePath.StartsWith('/api/webhooks/', [StringComparison]::OrdinalIgnoreCase)

    if ($isDiscordWebhook) {
        # Discord rejects attachments over 10 MB on servers without boosts, and it
        # does so after the whole upload, so check before wasting the transfer.
        $size = (Get-Item -LiteralPath $BundlePath).Length
        if ($size -gt 10MB) {
            throw ((UI-Text 'Paczka ma {0:N1} MB, a Discord przyjmuje do 10 MB. Wyślij ZIP ręcznie albo usuń starsze logi z folderu i zbierz paczkę ponownie.' 'The bundle is {0:N1} MB, and Discord takes up to 10 MB. Send the ZIP yourself, or delete older logs from the folder and collect the bundle again.') -f ($size / 1MB))
        }
    }

    Add-Type -AssemblyName System.Net.Http
    $client = [Net.Http.HttpClient]::new()
    $form = [Net.Http.MultipartFormDataContent]::new()
    $stream = [IO.File]::OpenRead($BundlePath)
    try {
        $content = [Net.Http.StreamContent]::new($stream)
        $content.Headers.ContentType = [Net.Http.Headers.MediaTypeHeaderValue]::Parse('application/zip')
        if ($isDiscordWebhook) {
            $payload = [Net.Http.StringContent]::new(
                '{"content":"Paczka diagnostyczna Metin2 Playerbots (hasła automatycznie usunięte).","allowed_mentions":{"parse":[]}}',
                [Text.Encoding]::UTF8,
                'application/json')
            $form.Add($payload, 'payload_json')
            $form.Add($content, 'files[0]', [IO.Path]::GetFileName($BundlePath))
        }
        else {
            $form.Add($content, 'file', [IO.Path]::GetFileName($BundlePath))
        }
        $response = $client.PostAsync($uri, $form).GetAwaiter().GetResult()
        $body = $response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
        if (-not $response.IsSuccessStatusCode) {
            throw (UI-Text "Serwer pomocy odrzucił paczkę: HTTP $([int]$response.StatusCode) $body" "The support server refused the bundle: HTTP $([int]$response.StatusCode) $body")
        }
        return $body
    }
    finally {
        $stream.Dispose()
        $form.Dispose()
        $client.Dispose()
    }
}

# =============================================================================
#  Database import -- copy an existing world (higher-level characters) from
#  another Docker installation's db-data volume into this install. Uses a
#  throwaway MariaDB container with --skip-grant-tables so no volume password
#  needs to be known, dumps the five game databases and reloads them into the
#  target. mysql.* (the game DB user and its grants) is never touched, so the
#  game keeps authenticating with the target install's own password. The source
#  volume is only ever read; the target is backed up before it is replaced.
# =============================================================================

$script:M2_DB_IMAGE = 'mariadb:10.11'
$script:M2_DB_LIST = @('account', 'common', 'player', 'log', 'hotbackup')

function Test-M2DockerRunning {
    # docker volume ls and friends fail with an unhelpful pipe/socket error when
    # the engine is down, so callers ask this first and say something useful.
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    try {
        & docker info --format '{{.ServerVersion}}' 1>$null 2>$null
        return ($LASTEXITCODE -eq 0)
    }
    catch { return $false }
    finally { $ErrorActionPreference = $previous }
}

function Get-M2DbDataVolumes {
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    try {
        $names = New-Object System.Collections.Generic.List[string]
        $labelled = & docker volume ls --filter 'label=com.docker.compose.volume=db-data' --format '{{.Name}}' 2>$null
        if ($LASTEXITCODE -eq 0 -and $labelled) {
            foreach ($n in @($labelled -split '\r?\n' | Where-Object { $_ })) { if (-not $names.Contains($n)) { $names.Add($n) } }
        }
        $all = & docker volume ls --format '{{.Name}}' 2>$null
        if ($LASTEXITCODE -eq 0 -and $all) {
            foreach ($n in @($all -split '\r?\n' | Where-Object { $_ -match '_db-data$' })) { if (-not $names.Contains($n)) { $names.Add($n) } }
        }
        # Creation dates in one call - the list is short, but one docker
        # invocation per volume is still a visible stall on a cold engine.
        $created = @{}
        if ($names.Count -gt 0) {
            $inspected = & docker volume inspect --format '{{.Name}}|{{.CreatedAt}}' @($names) 2>$null
            if ($LASTEXITCODE -eq 0 -and $inspected) {
                foreach ($line in @($inspected -split '\r?\n' | Where-Object { $_ })) {
                    $parts = $line -split '\|', 2
                    if ($parts.Count -eq 2) {
                        # Docker prints an RFC 3339 stamp with an offset.
                        try {
                            $stamp = [DateTimeOffset]::Parse($parts[1], [Globalization.CultureInfo]::InvariantCulture)
                            $created[$parts[0]] = $stamp.LocalDateTime
                        }
                        catch { }
                    }
                }
            }
        }

        $result = New-Object System.Collections.Generic.List[object]
        foreach ($name in $names) {
            $project = if ($name -match '^(.*)_db-data$') { $Matches[1] } else { $name }
            $stamp = $null
            if ($created.ContainsKey($name)) { $stamp = $created[$name] }
            $result.Add([pscustomobject]@{ Name = $name; Project = $project; CreatedAt = $stamp })
        }
        return $result.ToArray()
    }
    finally { $ErrorActionPreference = $previous }
}

function Get-M2ServerEngine {
    <#
        Which engine the tree under linux-port\docker was built for. 'r40250'
        is the original port; the mt2009 tree (linux-port-mt2009 in the
        repository, deployed under the same linux-port name so that every
        path in the launcher stays one path) carries an ENGINE file naming
        itself. Everything engine-specific asks here: which dumps make a
        world, whether the r40250 engine patches are applied, what a complete
        build context holds.
    #>
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $marker = Join-Path $ServerRoot 'linux-port\docker\ENGINE'
    if (Test-Path -LiteralPath $marker -PathType Leaf) {
        $engine = ([IO.File]::ReadAllText($marker)).Trim().ToLowerInvariant()
        if ($engine -match '^[a-z0-9]+$') { return $engine }
    }
    return 'r40250'
}

function Get-M2ChannelMemoryWarning {
    <#
        "Kanaly gry 1-4": every channel is three more game cores, and WSL 2
        gives Docker's machine half of the computer's memory unless .wslconfig
        says otherwise. A channel is about 2.6 GB there - measured on a test
        world on 28 September at 1000 bots over four channels (unified): the
        core hosting the villages and the frontier 2.5-2.8 GB, the two others
        about 0.09 GB each, whatever the channel's bot count - and MariaDB, the
        panels and Docker itself about 1.5 GB more. It said "about 1 GB" until
        then, and a player whose machine ran out of memory at two channels was
        never warned (Piciu97). Warns from the second channel when half the
        computer's memory is under what the channels take. A warning and never
        a refusal: the operator may have given Docker more by hand. TotalBytes
        is the computer's memory, asked of Windows when not given; an empty
        answer is no warning.
    #>
    param([int]$Channels, [long]$TotalBytes = -1)
    if ($Channels -lt 2) { return '' }
    if ($TotalBytes -lt 0) {
        try { $TotalBytes = [long](Get-CimInstance -ClassName Win32_ComputerSystem -ErrorAction Stop).TotalPhysicalMemory }
        catch { return '' }
    }
    if ($TotalBytes -le 0) { return '' }
    $dockerGb = [Math]::Round($TotalBytes / 2 / 1GB, 1)
    $needGb = [Math]::Round(1.5 + 2.6 * $Channels, 1)
    if ($dockerGb -ge $needGb) { return '' }
    return ((UI-Text 'UWAGA: każdy kanał to ok. 2,5 GB RAM w maszynie Dockera, która dostaje zwykle połowę pamięci komputera - tu ok. {0} GB. {1} kanały potrzebują ok. {2} GB; bezpieczniej mniej kanałów albo więcej pamięci dla Dockera.' 'WARNING: every channel is about 2.5 GB of RAM in the Docker machine, which usually gets half of the computer memory - here about {0} GB. {1} channels need about {2} GB; fewer channels or more memory for Docker is safer.') -f $dockerGb, $Channels, $needGb)
}

function Get-M2RequiredSqlDumps {
    # The per-database dumps MariaDB imports on its first start. r40250's
    # package ships hotbackup (legitimately empty); mt2009's keeps the protos
    # in a sixth database, world, and has no hotbackup dump at all.
    param([Parameter(Mandatory = $true)][string]$Engine)
    if ($Engine -eq 'mt2009') { return @('account', 'common', 'player', 'log', 'world') }
    return @('account', 'common', 'player', 'log', 'hotbackup')
}

function Restore-M2EmptyGameContextDirs {
    # The build inputs that are empty directories on every install of both
    # lines, and are therefore the most fragile thing a package can carry:
    # git cannot track an empty directory at all, a zip holds it as a bare
    # entry (twelve of them in the whole full package) and more than one
    # extraction tool drops those. The game Dockerfile COPYs
    # src/serverfiles/share/package all the same, so without it the build dies
    # at "failed to compute cache key" - and until now the launcher refused
    # first and told the player to re-run the r40250 installer, which on the
    # 2.x line is a package they have never had (dekri, 20 September).
    # Nothing is ever in these, so make them rather than demand them.
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $made = @()
    foreach ($rel in @('linux-port\docker\game\src\serverfiles\share\package')) {
        $full = Join-Path $ServerRoot $rel
        if (Test-Path -LiteralPath $full) { continue }
        try {
            New-Item -ItemType Directory -Path $full -Force -ErrorAction Stop | Out-Null
            $made += $rel
        } catch {
            # A folder we cannot create is a folder the check below will name,
            # which is the honest outcome.
        }
    }
    return $made
}

function Get-M2RequiredGameContext {
    # What linux-port\docker\game\src has to hold for the image to build,
    # relative to it: the modules the Dockerfile COPYs and the share
    # directories. mt2009 keeps its protos in the database, so it has no
    # share\conf, and its dependency script sits one level up, in game\.
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    if ((Get-M2ServerEngine -ServerRoot $ServerRoot) -eq 'mt2009') {
        return @(
            '..\build-deps-mt2009.sh', 'extern\include', 'extern\cryptopp', 'extern-tarballs',
            'server\__REVISION__',
            'server\common', 'server\db', 'server\game', 'server\libgame',
            'server\liblua', 'server\libpoly', 'server\libsql', 'server\libthecore',
            'serverfiles\share\CMD', 'serverfiles\share\data',
            'serverfiles\share\locale', 'serverfiles\share\package',
            'serverfiles\mark-default'
        )
    }
    return @(
        'build-deps-40250.sh', 'extern',
        'server\common', 'server\db', 'server\game', 'server\libgame',
        'server\liblua', 'server\libpoly', 'server\libserverkey',
        'server\libsql', 'server\libthecore',
        'serverfiles\share\conf', 'serverfiles\share\data',
        'serverfiles\share\locale', 'serverfiles\share\package',
        'serverfiles\mark-default'
    )
}

function Get-M2MissingSqlDumps {
    # The five SQL dumps MariaDB imports on its very first start. They come out
    # of the operator's own r40250 package (Server/metin2_mysql_dump.zip) and
    # are staged by the installer into mariadb/initdb.d/dumps; an update never
    # touches them. Missing here, the database initialises empty, the migrate
    # container waits thirty minutes for a schema that cannot appear, and the
    # only honest error sits in the MariaDB log - reported by an operator who
    # found it by reading container logs by hand. Returns the missing names.
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $dumpDir = Join-Path $ServerRoot 'linux-port\docker\mariadb\initdb.d\dumps'
    $missing = @()
    $engine = Get-M2ServerEngine -ServerRoot $ServerRoot
    foreach ($db in (Get-M2RequiredSqlDumps -Engine $engine)) {
        $f = Join-Path $dumpDir "$db.sql"
        if (-not (Test-Path -LiteralPath $f -PathType Leaf)) { $missing += "$db.sql"; continue }
        # hotbackup is legitimately empty (its Readme says so); the rest carry
        # the schema and must not be zero-length copies of nothing.
        if ($db -ne 'hotbackup' -and (Get-Item -LiteralPath $f).Length -eq 0) { $missing += (UI-Text "$db.sql (pusty)" "$db.sql (empty)") }
    }
    return $missing
}

function Test-M2VolumeInitialized {
    # True only when the volume already exists AND holds an initialized MariaDB
    # data directory. Never creates anything: `docker volume inspect' does not
    # create, and the content probe mounts read-only.
    param([Parameter(Mandatory = $true)][string]$Volume)
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    try {
        & docker volume inspect $Volume 1>$null 2>$null
        if ($LASTEXITCODE -ne 0) { return $false }
        # --entrypoint sh is required: the mariadb image's own entrypoint would
        # otherwise swallow the probe command.
        & docker run --rm --entrypoint sh -v "${Volume}:/v:ro" $script:M2_DB_IMAGE -c 'test -d /v/mysql' 1>$null 2>$null
        return ($LASTEXITCODE -eq 0)
    }
    finally { $ErrorActionPreference = $previous }
}

function Start-M2ThrowawayDb {
    param([Parameter(Mandatory = $true)][string]$Volume)
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    try {
        # Refuse to touch a volume that is missing or not an initialized
        # database. Docker silently CREATES a named volume that does not exist,
        # and MariaDB would then initialize it as an empty, password-less server
        # with no game schema. Because the volume is no longer empty afterwards,
        # the compose entrypoint never runs initdb.d again and the install is
        # permanently broken. Fail loudly instead.
        if (-not (Test-M2VolumeInitialized -Volume $Volume)) {
            throw (UI-Text "Baza '$Volume' nie istnieje albo nie jest jeszcze zainicjalizowana. Uruchom najpierw serwer (GRAJ) choć raz, aby baza powstała poprawnie, i dopiero potem użyj tej funkcji." "The database '$Volume' does not exist or is not initialised yet. Start the server (PLAY) once first so that the database is made properly, and only then use this.")
        }
        $container = 'm2dbimp-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
        # No MARIADB_ALLOW_EMPTY_ROOT_PASSWORD: on an initialized volume the
        # entrypoint skips setup entirely, and without it a surprise empty volume
        # makes the container refuse to start rather than silently create a
        # password-less database.
        $null = & docker run -d --name $container -v "${Volume}:/var/lib/mysql" $script:M2_DB_IMAGE --skip-grant-tables 2>$null
        if ($LASTEXITCODE -ne 0) { throw (UI-Text "Nie udało się uruchomić kontenera bazy dla wolumenu '$Volume' (czy jest zajęty przez działający serwer?)." "Could not start a database container for the volume '$Volume' (is a running server holding it?).") }
        $deadline = (Get-Date).AddSeconds(120)
        do {
            & docker exec $container sh -c "mariadb -uroot -e 'SELECT 1'" 1>$null 2>$null
            if ($LASTEXITCODE -eq 0) { return $container }
            Start-Sleep -Seconds 2
        } while ((Get-Date) -lt $deadline)
        & docker rm -f $container 1>$null 2>$null
        throw (UI-Text "Baza dla wolumenu '$Volume' nie wystartowała w 120 s." "The database of the volume '$Volume' did not start within 120 s.")
    }
    finally { $ErrorActionPreference = $previous }
}

function Stop-M2ThrowawayDb {
    param([string]$Container)
    if ($Container) {
        $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
        try {
            # Graceful stop so mysqld flushes and shuts down cleanly. A hard
            # `docker rm -f' (SIGKILL) leaves the volume needing InnoDB crash
            # recovery on the next start, which could bring the post-import
            # MariaDB up in a state where the game DB user failed to authenticate
            # ("unauthenticated"), stalling playerbot-migrate and blocking the
            # game and panel.
            & docker stop -t 40 $Container 1>$null 2>$null
            & docker rm -f $Container 1>$null 2>$null
        }
        finally { $ErrorActionPreference = $previous }
    }
}

function Get-M2VolumeWorldStats {
    param([Parameter(Mandatory = $true)][string]$Volume)
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $container = $null
    $created = ''
    try {
        # The volume's own creation time answers "when was this world made?"
        # without touching the data or starting a container.
        $created = [string](& docker volume inspect $Volume --format '{{.CreatedAt}}' 2>$null | Select-Object -First 1)
        $container = Start-M2ThrowawayDb -Volume $Volume
        # -B keeps the columns tab-separated so a last_play datetime (which has a
        # space) is not split apart.
        $out = & docker exec $container sh -c "mariadb -uroot -N -B -e 'SELECT COUNT(*), IFNULL(MAX(level),0), IFNULL(MAX(last_play),0) FROM player.player'" 2>$null
        if ($LASTEXITCODE -eq 0 -and $out) {
            $parts = ($out.ToString().Trim() -split "`t")
            return [pscustomobject]@{
                Players  = [int]$parts[0]
                MaxLevel = [int]$parts[1]
                LastPlay = [string]$parts[2]
                Created  = $created
                Ok       = $true
            }
        }
        return [pscustomobject]@{ Players = 0; MaxLevel = 0; LastPlay = ''; Created = $created; Ok = $false }
    }
    catch { return [pscustomobject]@{ Players = 0; MaxLevel = 0; LastPlay = ''; Created = $created; Ok = $false } }
    finally { Stop-M2ThrowawayDb -Container $container; $ErrorActionPreference = $previous }
}

function Export-M2Database {
    # -Force dumps past a table mariadb-dump cannot read (a crashed MyISAM
    # table of the log database, typically) and still says so in its exit
    # code; the caller decides what that is worth. What the dump said on
    # stderr goes into the error, because "the dump failed" alone is what a
    # player sent us on 20 September and nobody could tell which table.
    param([string]$Container, [string]$Database, [string]$OutFile, [switch]$Force)
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $errFile = "$OutFile.err"
    try {
        $extra = if ($Force) { ' --force' } else { '' }
        $line = "docker exec $Container mariadb-dump -uroot --single-transaction --no-tablespaces --skip-lock-tables$extra $Database > `"$OutFile`" 2> `"$errFile`""
        & cmd.exe /c $line
        $code = $LASTEXITCODE
        if ($code -ne 0 -or -not (Test-Path -LiteralPath $OutFile)) {
            $why = ''
            if (Test-Path -LiteralPath $errFile) {
                $why = ((Get-Content -LiteralPath $errFile -ErrorAction SilentlyContinue | Select-Object -First 3) -join ' ').Trim()
            }
            if ($why) { throw (UI-Text "Zrzut bazy '$Database' nie powiódł się: $why" "The dump of the database '$Database' failed: $why") }
            throw (UI-Text "Zrzut bazy '$Database' nie powiódł się." "The dump of the database '$Database' failed.")
        }
    }
    finally {
        if (Test-Path -LiteralPath $errFile) { Remove-Item -LiteralPath $errFile -Force -ErrorAction SilentlyContinue }
        $ErrorActionPreference = $previous
    }
}

function Invoke-M2SqlFile {
    param([string]$Container, [string]$Database, [string]$InFile)
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    try {
        $target = if ($Database) { " $Database" } else { '' }
        $line = "docker exec -i $Container mariadb -uroot$target < `"$InFile`""
        & cmd.exe /c $line 2>$null
        if ($LASTEXITCODE -ne 0) { throw (UI-Text "Wczytanie SQL nie powiodło się." "Loading the SQL failed.") }
    }
    finally { $ErrorActionPreference = $previous }
}

function Repair-M2GameDbUser {
    # Recreate the game DB user and its grants on an existing db-data volume.
    # For installs that swapped the world (import) before the graceful-shutdown
    # fix and were left with a MariaDB the migrator could not authenticate to.
    # Only mysql.* (the technical DB account) is touched; player data is not.
    #
    # root@'%' is put back on the .env password too when one is given. That
    # account is what a database client on the host (Navicat, HeidiSQL) logs
    # in with over the published port, and "Access denied for user
    # 'root'@'172.18.0.1'" - the compose gateway - is the report when the
    # volume was initialised under one password and .env carries another.
    # root@'localhost' is left alone: nothing of ours uses it.
    param(
        [Parameter(Mandatory = $true)][string]$Volume,
        [Parameter(Mandatory = $true)][string]$DbUser,
        [Parameter(Mandatory = $true)][string]$DbPassword,
        [string]$RootPassword = ''
    )
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $work = Join-Path ([IO.Path]::GetTempPath()) ('m2repair-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path $work -Force | Out-Null
    $container = $null
    try {
        $container = Start-M2ThrowawayDb -Volume $Volume
        $safeUser = ($DbUser -replace '[^A-Za-z0-9_]', '')
        if (-not $safeUser) { $safeUser = 'metin2' }
        $pwEsc = $DbPassword.Replace('\', '\\').Replace("'", "''")
        $gb = New-Object System.Text.StringBuilder
        [void]$gb.AppendLine('FLUSH PRIVILEGES;')
        [void]$gb.AppendLine("CREATE USER IF NOT EXISTS '$safeUser'@'%' IDENTIFIED BY '$pwEsc';")
        [void]$gb.AppendLine("ALTER USER '$safeUser'@'%' IDENTIFIED BY '$pwEsc';")
        foreach ($db in $script:M2_DB_LIST) {
            [void]$gb.AppendLine("GRANT ALL PRIVILEGES ON $db.* TO '$safeUser'@'%';")
        }
        if ($RootPassword) {
            $rootEsc = $RootPassword.Replace('\', '\\').Replace("'", "''")
            [void]$gb.AppendLine("CREATE USER IF NOT EXISTS 'root'@'%' IDENTIFIED BY '$rootEsc';")
            [void]$gb.AppendLine("ALTER USER 'root'@'%' IDENTIFIED BY '$rootEsc';")
            [void]$gb.AppendLine("GRANT ALL PRIVILEGES ON *.* TO 'root'@'%' WITH GRANT OPTION;")
        }
        [void]$gb.AppendLine('FLUSH PRIVILEGES;')
        $repairFile = Join-Path $work 'repair.sql'
        [IO.File]::WriteAllText($repairFile, $gb.ToString(), [Text.UTF8Encoding]::new($false))
        Invoke-M2SqlFile -Container $container -Database '' -InFile $repairFile
        return $true
    }
    finally {
        if ($container) { Stop-M2ThrowawayDb -Container $container }
        if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue }
        $ErrorActionPreference = $previous
    }
}

function Invoke-M2DatabaseImport {
    param(
        [Parameter(Mandatory = $true)][string]$SourceVolume,
        [Parameter(Mandatory = $true)][string]$TargetVolume,
        [Parameter(Mandatory = $true)][string]$BackupRoot,
        [string]$DbUser = 'metin2',
        [string]$DbPassword = ''
    )
    if ($SourceVolume -eq $TargetVolume) { throw (UI-Text 'Źródło i cel to ten sam wolumen.' 'The source and the target are the same volume.') }
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $work = Join-Path ([IO.Path]::GetTempPath()) ('m2dbimp-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
    $backupDir = Join-Path $BackupRoot "db-import-$stamp"
    New-Item -ItemType Directory -Path (Join-Path $work 'source') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $backupDir 'target-before-import') -Force | Out-Null

    $srcC = $null; $tgtC = $null
    try {
        # 1. Reversible backup of the current target world.
        $tgtC = Start-M2ThrowawayDb -Volume $TargetVolume
        foreach ($db in $script:M2_DB_LIST) {
            # No double quote may reach docker from PowerShell 5.1: it wraps a
            # native command's argument in double quotes without escaping the ones
            # inside it, so the first `" ends the argument and sh gets a broken
            # script. This probe carried one, always came back empty, and the
            # "reversible backup of the current target world" the operator is
            # promised in the confirmation dialog was an empty folder every time.
            # mariadb is invoked directly, with the query as its own argument.
            $exists = & docker exec $tgtC mariadb -uroot -N -B -e "SHOW DATABASES LIKE '$db'" 2>$null
            if ($exists) { Export-M2Database -Container $tgtC -Database $db -OutFile (Join-Path $backupDir "target-before-import\$db.sql") }
        }
        Stop-M2ThrowawayDb -Container $tgtC; $tgtC = $null

        # 2. Dump the source world (read only; the source volume is untouched).
        $srcC = Start-M2ThrowawayDb -Volume $SourceVolume
        foreach ($db in $script:M2_DB_LIST) {
            Export-M2Database -Container $srcC -Database $db -OutFile (Join-Path $work "source\$db.sql")
        }
        Stop-M2ThrowawayDb -Container $srcC; $srcC = $null

        # 3. Replace the five game databases in the target.
        $tgtC = Start-M2ThrowawayDb -Volume $TargetVolume
        $sb = New-Object System.Text.StringBuilder
        [void]$sb.AppendLine('SET FOREIGN_KEY_CHECKS=0;')
        foreach ($db in $script:M2_DB_LIST) {
            [void]$sb.AppendLine("DROP DATABASE IF EXISTS $db;")
            [void]$sb.AppendLine("CREATE DATABASE $db DEFAULT CHARACTER SET latin1 COLLATE latin1_swedish_ci;")
        }
        $createFile = Join-Path $work 'create.sql'
        [IO.File]::WriteAllText($createFile, $sb.ToString(), [Text.UTF8Encoding]::new($false))
        Invoke-M2SqlFile -Container $tgtC -Database '' -InFile $createFile
        foreach ($db in $script:M2_DB_LIST) {
            Invoke-M2SqlFile -Container $tgtC -Database $db -InFile (Join-Path $work "source\$db.sql")
        }

        $stats = & docker exec $tgtC sh -c "mariadb -uroot -N -B -e 'SELECT COUNT(*), IFNULL(MAX(level),0) FROM player.player'" 2>$null

        # Re-establish the game DB user and its grants so the game core and the
        # playerbot migrator can always authenticate after an import, regardless
        # of what the imported schema left behind. Done LAST, because FLUSH
        # PRIVILEGES turns the privilege system back on inside the
        # --skip-grant-tables container -- the -uroot socket queries above rely on
        # privileges being off. The standard initdb grants are (re)applied;
        # mysql.* is otherwise untouched, so player logins, characters, items and
        # bots are not altered.
        if ($DbPassword) {
            $safeUser = ($DbUser -replace '[^A-Za-z0-9_]', '')
            if (-not $safeUser) { $safeUser = 'metin2' }
            $pwEsc = $DbPassword.Replace('\', '\\').Replace("'", "''")
            $gb = New-Object System.Text.StringBuilder
            [void]$gb.AppendLine('FLUSH PRIVILEGES;')
            [void]$gb.AppendLine("CREATE USER IF NOT EXISTS '$safeUser'@'%' IDENTIFIED BY '$pwEsc';")
            [void]$gb.AppendLine("ALTER USER '$safeUser'@'%' IDENTIFIED BY '$pwEsc';")
            foreach ($db in $script:M2_DB_LIST) {
                [void]$gb.AppendLine("GRANT ALL PRIVILEGES ON $db.* TO '$safeUser'@'%';")
            }
            [void]$gb.AppendLine('FLUSH PRIVILEGES;')
            $grantFile = Join-Path $work 'grant.sql'
            [IO.File]::WriteAllText($grantFile, $gb.ToString(), [Text.UTF8Encoding]::new($false))
            Invoke-M2SqlFile -Container $tgtC -Database '' -InFile $grantFile
        }

        Stop-M2ThrowawayDb -Container $tgtC; $tgtC = $null

        $players = 0; $maxLevel = 0
        if ($stats) { $p = ($stats.ToString().Trim() -split '\s+'); $players = [int]$p[0]; $maxLevel = [int]$p[1] }
        return [pscustomobject]@{ Backup = $backupDir; Players = $players; MaxLevel = $maxLevel }
    }
    finally {
        if ($srcC) { Stop-M2ThrowawayDb -Container $srcC }
        if ($tgtC) { Stop-M2ThrowawayDb -Container $tgtC }
        if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue }
        $ErrorActionPreference = $previous
    }
}

function New-M2DatabaseBackup {
    # A backup an operator can keep, move to another machine, and read.
    #
    # The import path already dumped the target world before overwriting it, but
    # only as a side effect of an import, into a folder nobody was told about.
    # This is the same dump asked for on purpose: the five game databases as
    # plain SQL, a manifest naming what is inside, and a zip so that what lands
    # in a cloud folder is one file.
    #
    # The server has to be stopped first - the caller does that - because a
    # throwaway container cannot open a volume MariaDB is holding.
    param(
        [Parameter(Mandatory = $true)][string]$Volume,
        [Parameter(Mandatory = $true)][string]$BackupRoot,
        [string]$Label = ''
    )
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $safeLabel = ($Label -replace '[^A-Za-z0-9_\-]', '')
    $name = if ($safeLabel) { "db-backup-$stamp-$safeLabel" } else { "db-backup-$stamp" }
    $dir = Join-Path $BackupRoot $name
    $container = $null
    try {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        $container = Start-M2ThrowawayDb -Volume $Volume
        $sizes = @()
        $skipped = @()
        foreach ($db in $script:M2_DB_LIST) {
            # See the note in Invoke-M2DatabaseImport: no double quote inside a
            # command handed to docker from PowerShell.
            $exists = & docker exec $container mariadb -uroot -N -B -e "SHOW DATABASES LIKE '$db'" 2>$null
            if (-not $exists) { continue }
            $out = Join-Path $dir "$db.sql"
            try {
                Export-M2Database -Container $container -Database $db -OutFile $out
            }
            catch {
                # The log database is history only - nothing in the game reads
                # it - and it is where a crashed MyISAM table lives: 22 million
                # rows of log.log, killed with the engine. Its dump failing used
                # to fail the whole backup, and the world reset behind it
                # ("Zrzut bazy 'log' nie powiodl sie", uxietoszef, 20 September).
                # It is dumped again past the unreadable tables, and if even that
                # fails the backup goes on without it and says so. Any other
                # database is the world itself and still stops the backup.
                if ($db -ne 'log') { throw }
                $first = $_.Exception.Message
                try {
                    Export-M2Database -Container $container -Database $db -OutFile $out -Force
                    Write-Host (UI-Text "UWAGA: zrzut bazy 'log' wymagal pominiecia uszkodzonych tabel ($first)." "WARNING: the dump of the 'log' database had to skip damaged tables ($first).")
                    $skipped += (UI-Text "log (czesciowo: $first)" "log (in part: $first)")
                }
                catch {
                    if (Test-Path -LiteralPath $out) { Remove-Item -LiteralPath $out -Force -ErrorAction SilentlyContinue }
                    Write-Host (UI-Text "UWAGA: pomijam baze 'log' w kopii - to tylko historia, gra jej nie czyta ($first)." "WARNING: leaving the 'log' database out of the backup - it is history only, the game does not read it ($first).")
                    $skipped += (UI-Text "log (pominieta: $first)" "log (left out: $first)")
                    continue
                }
            }
            $sizes += [pscustomobject]@{ Name = $db; Bytes = (Get-Item -LiteralPath $out).Length }
        }
        if ($sizes.Count -eq 0) { throw (UI-Text 'Nie znaleziono zadnej bazy gry do zapisania.' 'No game database to save was found.') }
        $stat = & docker exec $container sh -c "mariadb -uroot -N -B -e 'SELECT COUNT(*), IFNULL(MAX(level),0) FROM player.player'" 2>$null
        $players = 0; $maxLevel = 0
        if ($stat) {
            $parts = ($stat.ToString().Trim() -split "`t")
            if ($parts.Count -ge 2) { $players = [int]$parts[0]; $maxLevel = [int]$parts[1] }
        }
        Stop-M2ThrowawayDb -Container $container; $container = $null

        # A backup that cannot be identified six months later is not a backup.
        $readme = New-Object System.Text.StringBuilder
        [void]$readme.AppendLine((UI-Text 'Kopia zapasowa swiata Metin2 Singleplayer' 'Metin2 Singleplayer world backup'))
        [void]$readme.AppendLine('')
        [void]$readme.AppendLine((UI-Text "Wykonana:      $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" "Made:          $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"))
        [void]$readme.AppendLine((UI-Text "Wolumen:       $Volume" "Volume:        $Volume"))
        [void]$readme.AppendLine((UI-Text "Postaci:       $players" "Characters:    $players"))
        [void]$readme.AppendLine((UI-Text "Najwyzszy lvl: $maxLevel" "Highest level: $maxLevel"))
        [void]$readme.AppendLine('')
        [void]$readme.AppendLine((UI-Text 'Zawartosc (zrzuty mariadb-dump, latin1 jak w grze):' 'Contents (mariadb-dump dumps, latin1 as in the game):'))
        foreach ($s in $sizes) {
            [void]$readme.AppendLine(('  {0,-12} {1,12:N0} B' -f ($s.Name + '.sql'), $s.Bytes))
        }
        foreach ($k in $skipped) {
            [void]$readme.AppendLine((UI-Text "  UWAGA: $k" "  WARNING: $k"))
        }
        [void]$readme.AppendLine('')
        [void]$readme.AppendLine((UI-Text 'Przywrocenie: launcher -> PRZYWROC KOPIE, i wskaz ten folder albo zip.' 'To restore it: launcher -> BACKUP / NEW WORLD -> Restore from a backup, and point to this folder or the zip.'))
        [IO.File]::WriteAllText((Join-Path $dir 'README.txt'), $readme.ToString(),
            [Text.UTF8Encoding]::new($false))

        $zip = Join-Path $BackupRoot ($name + '.zip')
        if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
        # Not Compress-Archive: in Windows PowerShell 5.1 it holds every entry
        # in a MemoryStream, which stops at 2 GB, and a world whose log.sql had
        # grown past that failed its backup with "Stream was too long". The
        # backup comes before anything is deleted, so every reset of such a
        # world was refused - three tries in a day for uxietoszef (18
        # September), the world untouched each time. CreateFromDirectory
        # writes each file straight into the zip, the same layout as before.
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [System.IO.Compression.ZipFile]::CreateFromDirectory($dir, $zip,
            [System.IO.Compression.CompressionLevel]::Optimal, $false)
        return [pscustomobject]@{
            Folder   = $dir
            Zip      = $zip
            Players  = $players
            MaxLevel = $maxLevel
            Files    = $sizes
            ZipBytes = (Get-Item -LiteralPath $zip).Length
        }
    }
    finally { Stop-M2ThrowawayDb -Container $container; $ErrorActionPreference = $previous }
}

function Restore-M2DatabaseBackup {
    # The other half of New-M2DatabaseBackup, and the same swap Invoke-M2DatabaseImport
    # performs - only the source is a folder of SQL files instead of another
    # volume. A zip is accepted and unpacked to a temporary folder first, so an
    # operator can point at exactly what the backup produced.
    param(
        [Parameter(Mandatory = $true)][string]$BackupPath,
        [Parameter(Mandatory = $true)][string]$TargetVolume,
        [Parameter(Mandatory = $true)][string]$BackupRoot,
        [string]$DbUser = 'metin2',
        [string]$DbPassword = ''
    )
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $work = Join-Path ([IO.Path]::GetTempPath()) ('m2dbres-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
    $tgtC = $null
    try {
        New-Item -ItemType Directory -Path $work -Force | Out-Null
        $source = $BackupPath
        if ((Test-Path -LiteralPath $BackupPath -PathType Leaf) -and
                ([IO.Path]::GetExtension($BackupPath) -eq '.zip')) {
            $source = Join-Path $work 'unpacked'
            New-Item -ItemType Directory -Path $source -Force | Out-Null
            Expand-Archive -LiteralPath $BackupPath -DestinationPath $source -Force
        }
        if (-not (Test-Path -LiteralPath $source -PathType Container)) {
            throw (UI-Text "Nie znaleziono kopii: $BackupPath" "Backup not found: $BackupPath")
        }
        # A backup without player.sql is not this server's backup, and loading it
        # would leave the install with no world at all.
        $found = @()
        foreach ($db in $script:M2_DB_LIST) {
            if (Test-Path -LiteralPath (Join-Path $source "$db.sql") -PathType Leaf) { $found += $db }
        }
        if ($found -notcontains 'player') {
            throw (UI-Text "W kopii '$source' nie ma pliku player.sql - to nie jest kopia swiata tego serwera." "The backup '$source' has no player.sql - it is not a backup of this server's world.")
        }

        # The world about to be replaced goes into its own backup first. The
        # operator asked to restore, not to lose what is there now.
        $safety = New-M2DatabaseBackup -Volume $TargetVolume -BackupRoot $BackupRoot -Label 'przed-przywroceniem'

        $tgtC = Start-M2ThrowawayDb -Volume $TargetVolume
        $sb = New-Object System.Text.StringBuilder
        [void]$sb.AppendLine('SET FOREIGN_KEY_CHECKS=0;')
        foreach ($db in $found) {
            [void]$sb.AppendLine("DROP DATABASE IF EXISTS $db;")
            [void]$sb.AppendLine("CREATE DATABASE $db DEFAULT CHARACTER SET latin1 COLLATE latin1_swedish_ci;")
        }
        $createFile = Join-Path $work 'create.sql'
        [IO.File]::WriteAllText($createFile, $sb.ToString(), [Text.UTF8Encoding]::new($false))
        Invoke-M2SqlFile -Container $tgtC -Database '' -InFile $createFile
        foreach ($db in $found) {
            Invoke-M2SqlFile -Container $tgtC -Database $db -InFile (Join-Path $source "$db.sql")
        }
        $stats = & docker exec $tgtC sh -c "mariadb -uroot -N -B -e 'SELECT COUNT(*), IFNULL(MAX(level),0) FROM player.player'" 2>$null

        # Same tail as the import: the game user and its grants last, because
        # FLUSH PRIVILEGES turns the privilege system back on.
        if ($DbPassword) {
            $safeUser = ($DbUser -replace '[^A-Za-z0-9_]', '')
            if (-not $safeUser) { $safeUser = 'metin2' }
            $pwEsc = $DbPassword.Replace('\', '\\').Replace("'", "''")
            $gb = New-Object System.Text.StringBuilder
            [void]$gb.AppendLine('FLUSH PRIVILEGES;')
            [void]$gb.AppendLine("CREATE USER IF NOT EXISTS '$safeUser'@'%' IDENTIFIED BY '$pwEsc';")
            [void]$gb.AppendLine("ALTER USER '$safeUser'@'%' IDENTIFIED BY '$pwEsc';")
            foreach ($db in $script:M2_DB_LIST) {
                [void]$gb.AppendLine("GRANT ALL PRIVILEGES ON $db.* TO '$safeUser'@'%';")
            }
            [void]$gb.AppendLine('FLUSH PRIVILEGES;')
            $grantFile = Join-Path $work 'grant.sql'
            [IO.File]::WriteAllText($grantFile, $gb.ToString(), [Text.UTF8Encoding]::new($false))
            Invoke-M2SqlFile -Container $tgtC -Database '' -InFile $grantFile
        }
        Stop-M2ThrowawayDb -Container $tgtC; $tgtC = $null

        $players = 0; $maxLevel = 0
        if ($stats) {
            $parts = ($stats.ToString().Trim() -split "`t")
            if ($parts.Count -ge 2) { $players = [int]$parts[0]; $maxLevel = [int]$parts[1] }
        }
        return [pscustomobject]@{
            Players  = $players
            MaxLevel = $maxLevel
            Source   = $source
            Safety   = $safety.Zip
            Restored = $found
        }
    }
    finally {
        Stop-M2ThrowawayDb -Container $tgtC
        Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
        $ErrorActionPreference = $previous
    }
}

function Reset-M2WorldToFreshInstall {
    # Back to the world a fresh install starts with: the r40250 dumps, the
    # playerbot schema and a freshly seeded cohort.
    #
    # It is done by deleting the database volume, because that is the only thing
    # that makes MariaDB run initdb.d again - the entrypoint skips it entirely on
    # a volume that is not empty, which is why "just drop the databases" would
    # leave an install with no schema and no way to get one back.
    #
    # And it refuses to delete anything until the five dumps that rebuild it are
    # on disk. An install assembled from an update package can be missing them,
    # and a reset that discovers this after the volume is gone leaves an operator
    # with neither the old world nor a new one.
    param(
        [Parameter(Mandatory = $true)][string]$Volume,
        [Parameter(Mandatory = $true)][string]$ServerRoot,
        [Parameter(Mandatory = $true)][string]$BackupRoot
    )
    # @(): an empty result unrolls to $null on the way out, and $null.Count
    # throws under StrictMode - which is how the first run of this refused to
    # reset a world it was perfectly able to reset. Same idiom as every other
    # caller of this function.
    $missing = @(Get-M2MissingSqlDumps -ServerRoot $ServerRoot)
    if ($missing.Count -gt 0) {
        throw ((UI-Text "Nie moge zresetowac swiata: brakuje zrzutow, z ktorych powstaje nowa baza (" "Cannot reset the world: the dumps a new database is made from are missing (") +
               ($missing -join ', ') + (UI-Text "). Znajduja sie w linux-port\docker\mariadb\initdb.d\dumps." "). They belong in linux-port\docker\mariadb\initdb.d\dumps."))
    }
    $backup = $null
    if (Test-M2VolumeInitialized -Volume $Volume) {
        $backup = New-M2DatabaseBackup -Volume $Volume -BackupRoot $BackupRoot -Label 'przed-resetem'
    }
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    try {
        # A stopped container still holds its volume. The launcher stops the
        # stack with `compose stop', which leaves every container in place, so
        # `volume rm' answered "volume is in use" on a server that was
        # perfectly stopped - four times in a row for one player, each time
        # "czy serwer na pewno jest zatrzymany?" (NieBijOddam, 13 September).
        # Remove whatever references the volume first; `compose up' recreates
        # the containers on the next start, the way it does after an update.
        $holders = @(& docker ps -a -q --filter "volume=$Volume" 2>$null | Where-Object { $_ })
        foreach ($id in $holders) {
            & docker rm -f $id 1>$null 2>$null
        }
        & docker volume rm -f $Volume 1>$null 2>$null
        if ($LASTEXITCODE -ne 0) {
            $still = @(& docker ps -a --filter "volume=$Volume" --format '{{.Names}} ({{.Status}})' 2>$null | Where-Object { $_ })
            $who = if ($still.Count -gt 0) { (UI-Text ' Wciaz uzywaja go: ' ' Still using it: ') + ($still -join ', ') + '.' } else { '' }
            throw ((UI-Text "Nie udalo sie usunac wolumenu '$Volume'.$who Zatrzymaj Docker Desktop, uruchom go ponownie i sprobuj jeszcze raz." "Could not delete the volume '$Volume'.$who Stop Docker Desktop, start it again and try once more."))
        }
    }
    finally { $ErrorActionPreference = $previous }
    return [pscustomobject]@{
        Volume = $Volume
        Backup = if ($backup) { $backup.Zip } else { '' }
        Players = if ($backup) { $backup.Players } else { 0 }
    }
}

Export-ModuleMember -Function @(
    'Get-M2DefaultLauncherConfig',
    'Get-M2LauncherConfig',
    'Save-M2LauncherConfig',
    'Get-M2UpdateManifest',
    'Invoke-M2PackageUpdate',
    'New-M2SupportBundle',
    'Send-M2SupportBundle',
    'Get-M2SupportSettings',
    'Test-M2SupportUploadUrl',
    'Get-M2DbDataVolumes',
    'Get-M2VolumeWorldStats',
    'Invoke-M2DatabaseImport',
    'New-M2DatabaseBackup',
    'Restore-M2DatabaseBackup',
    'Reset-M2WorldToFreshInstall',
    'Repair-M2GameDbUser',
    'Test-M2VolumeInitialized',
    'Get-M2MissingSqlDumps',
    'Get-M2ServerEngine',
    'Get-M2ChannelMemoryWarning',
    'Get-M2SiblingClientExecutable',
    'Get-M2RequiredSqlDumps',
    'Get-M2RequiredGameContext',
    'Restore-M2EmptyGameContextDirs',
    'Test-M2DockerRunning',
    'Sync-M2PlayerbotOverlay',
    'Set-M2PlayerbotsVersionEnvironment',
    'Invoke-M2EnginePatches',
    'Get-M2FolderProcesses',
    'Test-M2ClientExeOld',
    'Get-M2ClientExeComponent',
    'Repair-M2ClientExecutables',
    'Protect-M2SessionLogLine',
    'Protect-M2LogFile'
)
