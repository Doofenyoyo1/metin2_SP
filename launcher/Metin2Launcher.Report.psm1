Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

# ZGLOS / REPORT (29 September): a player's report - a bug, a suggestion or
# anything else - sent from the launcher with the versions and, unless the
# the player unticks it, the support bundle (New-M2SupportBundle), to an address
# the MT2009 PLUS owner controls. Two kinds of address are understood:
#
#   a Discord webhook (https://discord.com/api/webhooks/<id>/<token>) - the
#             simplest receiver: a private channel of the owner's server. The
#             report goes as one message (kind, versions, contact, the start
#             of the description) with the ZIP attached (report.txt inside it
#             holds the whole description). Discord takes 10 MB per message
#             on a server without boosts, so the ZIP is cut to 9.5 MB;
#   a report web app (any other https address) - a JSON endpoint of the
#             owner's, as described below.
#
# What travels to a report web app:
#
#   request   the form: kind, description, contact, whether logs go along;
#   package   support-bundles\report-<stamp>.zip - report.txt and report.json
#             (the form and the versions) and the bundle's files, every file
#             masked again by the secrets this installation actually holds
#             (.env and its copies, COOP's accounts, the launcher's own
#             addresses, the client's saved logins) and cut to the size cap,
#             the logs' oldest lines first;
#   body      one JSON POST: the form, the versions and the zip in base64.
#             doPost reads a body only as text (e.postData.contents) and
#             parses no multipart upload, so a zip sent as a file part would
#             reach it mangled; and it sees no request header at all, so the
#             endpoint's optional key rides in the address (?k=...);
#   answer    {"ok":true,"id":...} or {"ok":false,"error":...}. Apps Script
#             answers 200 whatever happened, so the launcher reads the answer,
#             never the status, and a page that is not that JSON - Google's
#             sign-in, an error page - is a failure that says what it saw.
#
# The address is empty by default: reportUrl in .m2launcher.json (a tester's,
# or the owner's before he publishes it), else the update manifest's
# support.reportUrl, which reaches every launcher and can be changed or
# blanked without a release; and when neither names one, the log button's
# Discord webhook (supportUploadUrl in .m2launcher.json, else the manifest's
# support.uploadUrl) takes reports too. No credential ships in the launcher.
# Without an address the report is saved as the same ZIP for the player to
# send by hand.
#
# Everything a report leaves on disk lives in support-bundles, which every
# update, VPS upload and full package already leaves out (Invoke-M2PackageUpdate,
# Send-M2VpsServer, New-M2FullPackage.ps1): the ZIPs, an unsent draft, the
# window's request and the action's answer, and the installation's report id.

# A text in the launcher's language: .m2launcher.json's "language", which the
# window and Metin2-Launcher.ps1 put into $env:M2_LAUNCHER_LANGUAGE for every
# module and every action they start. The English one for 'en', otherwise the
# Polish one. Each module keeps its own copy and none exports it.
function UI-Text {
    param([AllowEmptyString()][string]$Pl, [AllowEmptyString()][string]$En)
    if ($env:M2_LAUNCHER_LANGUAGE -eq 'en' -and $En) { return $En }
    return $Pl
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

# The ZIP's ceiling. Base64 makes it a third larger on the wire, and the web
# app decodes it in memory under Apps Script's 50 MB blob limit; the endpoint
# may ask for less (its GET answers maxZipBytes), never for more.
$script:M2ReportMaxZipBytes = 15MB
$script:M2ReportMinZipBytes = 256KB
$script:M2ReportMinDescription = 10
$script:M2ReportMaxDescription = 20000
$script:M2ReportMaxContact = 200
$script:M2ReportDefaultContact = 'https://github.com/Doofenyoyo1/metin2_SP/issues'
# A Discord webhook's message: 10 MB with the attachment on a server without
# boosts, 2000 characters of text.
$script:M2ReportDiscordMaxZipBytes = [long](9.5 * 1MB)
$script:M2ReportDiscordMaxContent = 1900
# Byte for character, both ways: the logs are UTF-8, CP1250 from the game
# and whatever else, and a mask written through this changes no other byte.
$script:M2ReportLatin1 = [Text.Encoding]::GetEncoding(28591)
# The form itself and the bundle's summary are never shortened.
$script:M2ReportNeverTrimmed = @('report.txt', 'report.json', 'summary.txt')
# Shapes that are secret whatever their value: a COOP invite carries a
# friend's host, login and password; a Discord webhook's token is all of its
# authority; the report address's own key.
$script:M2ReportPatterns = @(
    @('(M2COOP1:)[A-Za-z0-9_+/=-]{8,}', '$1<redacted>'),
    @('(?i)(https?://(?:[a-z0-9-]+\.)?discord(?:app)?\.com/api/webhooks/\d+/)[A-Za-z0-9_.-]+', '$1<redacted>'),
    @('(?i)(script\.google(?:usercontent)?\.com/[^\s"''<>]*?[?&]k=)[^\s&"''<>]+', '$1<redacted>')
)

# ---------------------------------------------------------------- files

function Get-M2ReportFolder {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $folder = Join-Path ([IO.Path]::GetFullPath($ServerRoot).TrimEnd('\')) 'support-bundles'
    New-Item -ItemType Directory -Path $folder -Force | Out-Null
    return $folder
}

function Read-M2ReportTextFile {
    param([string]$Path)
    if (-not $Path -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) { return '' }
    try { return ([IO.File]::ReadAllText($Path)).Trim() } catch { return '' }
}

function Get-M2ReportState {
    # The installation's report id - random, so a report says which ones came
    # from one place and the endpoint can count them, and nothing about the
    # machine - and the contact typed last time, offered again.
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $path = Join-Path (Get-M2ReportFolder -ServerRoot $ServerRoot) '.report-state.json'
    $state = [pscustomobject]@{ installId = ''; lastContact = ''; lastSent = '' }
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        try {
            $saved = [IO.File]::ReadAllText($path, [Text.Encoding]::UTF8) | ConvertFrom-Json
            foreach ($name in @('installId', 'lastContact', 'lastSent')) {
                if ($saved -and $saved.PSObject.Properties[$name]) { $state.$name = [string]$saved.$name }
            }
        }
        catch { }
    }
    if ($state.installId -notmatch '^[0-9a-f]{32}$') {
        $state.installId = [Guid]::NewGuid().ToString('N')
        Save-M2ReportState -ServerRoot $ServerRoot -State $state
    }
    return $state
}

function Save-M2ReportState {
    param([Parameter(Mandatory = $true)][string]$ServerRoot, [Parameter(Mandatory = $true)]$State)
    $path = Join-Path (Get-M2ReportFolder -ServerRoot $ServerRoot) '.report-state.json'
    try { [IO.File]::WriteAllText($path, ($State | ConvertTo-Json), [Text.UTF8Encoding]::new($false)) } catch { }
}

# ---------------------------------------------------------------- the form

function ConvertTo-M2ReportCategory {
    # The three kinds as the endpoint names them. The console takes a number,
    # either language's word or nothing (a bug).
    param([AllowEmptyString()][string]$Value)
    $word = ([string]$Value).Trim().ToLowerInvariant()
    if (-not $word -or @('1', 'b', 'bug', 'blad', 'błąd', 'error') -contains $word) { return 'bug' }
    if (@('2', 'p', 's', 'suggestion', 'propozycja', 'pomysl', 'pomysł', 'idea') -contains $word) { return 'suggestion' }
    if (@('3', 'o', 'i', 'other', 'inne', 'inny', 'pytanie', 'question') -contains $word) { return 'other' }
    throw ((UI-Text 'Nieznany rodzaj zgłoszenia: {0} (błąd, propozycja albo inne).' 'Unknown kind of report: {0} (bug, suggestion or other).') -f $Value)
}

function Get-M2ReportCategoryText {
    param([AllowEmptyString()][string]$Category)
    switch ($Category) {
        'suggestion' { return (UI-Text 'Propozycja' 'Suggestion') }
        'other' { return (UI-Text 'Inne' 'Other') }
        default { return (UI-Text 'Błąd' 'Bug') }
    }
}

function Test-M2ReportRequest {
    # What is wrong with a form, in the player's words; nothing when it may go.
    param([Parameter(Mandatory = $true)]$Request)
    $problems = @()
    $text = [string]$Request.description
    if ($text.Trim().Length -lt $script:M2ReportMinDescription) {
        $problems += ((UI-Text 'Opisz problem albo pomysł w kilku słowach (co najmniej {0} znaków).' 'Describe the problem or the idea in a few words (at least {0} characters).') -f $script:M2ReportMinDescription)
    }
    if ($text.Length -gt $script:M2ReportMaxDescription) {
        $problems += ((UI-Text 'Opis jest za długi (najwyżej {0} znaków) - resztę dopisz na Discordzie.' 'The description is too long (at most {0} characters) - add the rest on Discord.') -f $script:M2ReportMaxDescription)
    }
    if (([string]$Request.contact).Length -gt $script:M2ReportMaxContact) {
        $problems += ((UI-Text 'Kontakt jest za długi (najwyżej {0} znaków).' 'The contact is too long (at most {0} characters).') -f $script:M2ReportMaxContact)
    }
    return $problems
}

function New-M2ReportRequest {
    param(
        [AllowEmptyString()][string]$Category = '',
        [AllowEmptyString()][string]$Description = '',
        [AllowEmptyString()][string]$Contact = '',
        [bool]$AttachLogs = $true,
        [AllowEmptyString()][string]$Id = '',
        [AllowEmptyString()][string]$Created = ''
    )
    $request = [pscustomobject]@{
        schema      = 1
        id          = $(if ($Id -match '^[0-9a-f]{32}$') { $Id } else { [Guid]::NewGuid().ToString('N') })
        created     = $(if ($Created) { $Created } else { (Get-Date).ToString('yyyy-MM-ddTHH:mm:sszzz') })
        category    = (ConvertTo-M2ReportCategory -Value $Category)
        description = (([string]$Description) -replace "`r`n", "`n").Trim()
        contact     = (([string]$Contact) -replace '[\r\n\t]+', ' ').Trim()
        attachLogs  = [bool]$AttachLogs
        language    = $(if ($env:M2_LAUNCHER_LANGUAGE -eq 'en') { 'en' } else { 'pl' })
    }
    $problems = @(Test-M2ReportRequest -Request $request)
    if ($problems.Count -gt 0) { throw ($problems -join ' ') }
    return $request
}

function Save-M2ReportRequest {
    # The window's form for the action it starts (a file: a description of a
    # page on a command line is quotes and backslashes PowerShell 5.1 passes
    # on broken), or, with -Draft, what a cancelled window held.
    param([Parameter(Mandatory = $true)][string]$ServerRoot, [Parameter(Mandatory = $true)]$Request, [switch]$Draft)
    $folder = Get-M2ReportFolder -ServerRoot $ServerRoot
    $name = if ($Draft) { '.report-draft.json' } else { '.report-request-{0}.json' -f (Get-Date -Format 'yyyyMMdd-HHmmss-fff') }
    $path = Join-Path $folder $name
    [IO.File]::WriteAllText($path, ($Request | ConvertTo-Json -Depth 3), [Text.UTF8Encoding]::new($false))
    return $path
}

function Read-M2ReportRequest {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw ((UI-Text 'Nie ma pliku zgłoszenia: {0}' 'The report file is not there: {0}') -f $Path)
    }
    $saved = [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8) | ConvertFrom-Json
    $values = @{ category = ''; description = ''; contact = ''; attachLogs = $true; id = ''; created = '' }
    foreach ($name in @($values.Keys)) {
        if ($saved -and $saved.PSObject.Properties[$name] -and $null -ne $saved.$name) { $values[$name] = $saved.$name }
    }
    return (New-M2ReportRequest -Category ([string]$values.category) -Description ([string]$values.description) `
            -Contact ([string]$values.contact) -AttachLogs ([bool]$values.attachLogs) -Id ([string]$values.id) -Created ([string]$values.created))
}

function Get-M2ReportDraft {
    # What the window last held and did not send - the text it was cancelled
    # with, or a request no action finished (a crash, a refused start) -
    # newest first, so a player who typed a page is not asked to type it again.
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $folder = Get-M2ReportFolder -ServerRoot $ServerRoot
    $files = @(Get-ChildItem -LiteralPath $folder -Force -File -ErrorAction SilentlyContinue |
            Where-Object { ($_.Name -eq '.report-draft.json' -or $_.Name -like '.report-request-*.json') -and $_.LastWriteTime -gt (Get-Date).AddDays(-14) } |
            Sort-Object LastWriteTime -Descending)
    foreach ($file in $files) {
        try {
            $saved = [IO.File]::ReadAllText($file.FullName, [Text.Encoding]::UTF8) | ConvertFrom-Json
            $draft = [pscustomobject]@{ category = 'bug'; description = ''; contact = ''; attachLogs = $true }
            foreach ($name in @('category', 'description', 'contact', 'attachLogs')) {
                if ($saved -and $saved.PSObject.Properties[$name] -and $null -ne $saved.$name) { $draft.$name = $saved.$name }
            }
            if (([string]$draft.description).Trim()) { return $draft }
        }
        catch { }
    }
    return $null
}

function Clear-M2ReportDrafts {
    param([Parameter(Mandatory = $true)][string]$ServerRoot, [AllowEmptyString()][string]$Except = '')
    $folder = Get-M2ReportFolder -ServerRoot $ServerRoot
    foreach ($file in @(Get-ChildItem -LiteralPath $folder -Force -File -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -eq '.report-draft.json' -or $_.Name -like '.report-request-*.json' })) {
        if ($Except -and $file.FullName -eq [IO.Path]::GetFullPath($Except)) { continue }
        Remove-Item -LiteralPath $file.FullName -Force -ErrorAction SilentlyContinue
    }
}

function Clear-M2ReportLeftovers {
    # What a report killed half-way leaves: its work folder - the unpacked
    # bundle, up to a hundred megabytes of logs - and an answer no window came
    # back for.
    param([Parameter(Mandatory = $true)][string]$Folder)
    foreach ($item in @(Get-ChildItem -LiteralPath $Folder -Force -ErrorAction SilentlyContinue)) {
        $stale = ($item.PSIsContainer -and $item.Name -like '.report-work-*' -and $item.LastWriteTime -lt (Get-Date).AddHours(-1)) -or
            (-not $item.PSIsContainer -and $item.Name -like '.report-result-*' -and $item.LastWriteTime -lt (Get-Date).AddDays(-1))
        if ($stale) { Remove-Item -LiteralPath $item.FullName -Recurse -Force -ErrorAction SilentlyContinue }
    }
}

function Get-M2ReportResultPath {
    # Where the action leaves its answer for the window that started it.
    param([Parameter(Mandatory = $true)][string]$RequestPath)
    $name = [IO.Path]::GetFileName($RequestPath)
    if ($name -like '.report-request-*') { $name = '.report-result-' + $name.Substring('.report-request-'.Length) }
    else { $name = $name + '.result.json' }
    return (Join-Path (Split-Path -Parent $RequestPath) $name)
}

function Write-M2ReportResult {
    param([Parameter(Mandatory = $true)][string]$Path, [Parameter(Mandatory = $true)]$Result)
    # Written beside it and moved over, so the window never reads half a file.
    $temp = $Path + '.tmp'
    $json = [ordered]@{
        outcome    = [string]$Result.Outcome
        ok         = [bool]$Result.Ok
        id         = [string]$Result.Id
        path       = [string]$Result.Path
        message    = [string]$Result.Message
        contactUrl = [string]$Result.ContactUrl
    } | ConvertTo-Json
    [IO.File]::WriteAllText($temp, $json, [Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temp -Destination $Path -Force
}

function Read-M2ReportResult {
    # The action's answer, once: read and removed. Nothing until it is there.
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    try { $result = [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8) | ConvertFrom-Json }
    catch { return $null }
    Remove-Item -LiteralPath $Path -Force -ErrorAction SilentlyContinue
    return $result
}

# ---------------------------------------------------------------- the address

function Test-M2ReportDiscordUrl {
    # A Discord webhook: https, discord.com (or discordapp.com), /api/webhooks/.
    # An invitation (discord.gg) is not one - a post to it always fails.
    param([AllowEmptyString()][string]$Url)
    $uri = $null
    if (-not $Url -or -not [Uri]::TryCreate($Url.Trim(), [UriKind]::Absolute, [ref]$uri)) { return $false }
    if ($uri.Scheme -ne 'https') { return $false }
    if ($uri.Host -notmatch '^(?:[a-z0-9-]+\.)?discord(?:app)?\.com$') { return $false }
    return $uri.AbsolutePath.StartsWith('/api/webhooks/', [StringComparison]::OrdinalIgnoreCase)
}

function Test-M2ReportUrl {
    # A report address: a Discord webhook, or the https address of a report
    # web app (plain http only on this computer, for tests).
    param([AllowEmptyString()][string]$Url)
    $uri = $null
    if (-not $Url -or -not [Uri]::TryCreate($Url.Trim(), [UriKind]::Absolute, [ref]$uri)) { return $false }
    if ($uri.Host -match '(^|\.)discord(app)?\.(com|gg)$') { return (Test-M2ReportDiscordUrl -Url $Url) }
    if ($uri.Scheme -eq 'https') { return $true }
    # Plain HTTP only on this computer, where nothing crosses a network: the
    # tests' own listener.
    return ($uri.Scheme -eq 'http' -and $uri.IsLoopback)
}

function Get-M2ReportSettings {
    # Where a report goes: reportUrl in .m2launcher.json wins, so a tester or
    # the operator can point one launcher somewhere without touching what
    # everybody reads; otherwise the manifest's support.reportUrl. -Manifest
    # is one already read; -NoRemote reads none.
    param([Parameter(Mandatory = $true)]$Config, $Manifest = $null, [switch]$NoRemote)
    $result = [pscustomobject]@{ Url = ''; Source = 'none'; ContactUrl = $script:M2ReportDefaultContact; Problem = '' }
    $local = ''
    if ($null -ne $Config.PSObject.Properties['reportUrl']) { $local = ([string]$Config.reportUrl).Trim() }
    if ($local) {
        if (Test-M2ReportUrl -Url $local) {
            $result.Url = $local
            $result.Source = 'config'
            return $result
        }
        $result.Problem = (UI-Text 'reportUrl w .m2launcher.json nie jest ani webhookiem Discorda, ani adresem https aplikacji zgłoszeń - pomijam go.' 'reportUrl in .m2launcher.json is neither a Discord webhook nor an https address of a report web app - it is skipped.')
    }
    # The log button's own webhook (supportUploadUrl) is the next choice, below
    # the manifest's reportUrl: one webhook can take both.
    $localUpload = ''
    if ($null -ne $Config.PSObject.Properties['supportUploadUrl']) { $localUpload = ([string]$Config.supportUploadUrl).Trim() }
    if ($null -eq $Manifest -and -not $NoRemote) {
        try {
            $source = ''
            if ($null -ne $Config.PSObject.Properties['manifestUrl']) { $source = [string]$Config.manifestUrl }
            if ($source) { $Manifest = Get-M2UpdateManifest -Source $source -TimeoutSec 10 }
        }
        catch { $Manifest = $null }
    }
    $support = $null
    if ($null -ne $Manifest -and $null -ne $Manifest.PSObject.Properties['support']) { $support = $Manifest.support }
    if ($null -ne $support) {
        if ($null -ne $support.PSObject.Properties['contactUrl'] -and [string]$support.contactUrl) { $result.ContactUrl = [string]$support.contactUrl }
        if ($null -ne $support.PSObject.Properties['reportUrl']) {
            $remote = ([string]$support.reportUrl).Trim()
            if ($remote -and (Test-M2ReportUrl -Url $remote)) {
                $result.Url = $remote
                $result.Source = 'manifest'
                return $result
            }
        }
    }
    if ($localUpload -and (Test-M2ReportDiscordUrl -Url $localUpload)) {
        $result.Url = $localUpload
        $result.Source = 'config-upload'
        return $result
    }
    if ($null -ne $support -and $null -ne $support.PSObject.Properties['uploadUrl']) {
        $remoteUpload = ([string]$support.uploadUrl).Trim()
        if ($remoteUpload -and (Test-M2ReportDiscordUrl -Url $remoteUpload)) {
            $result.Url = $remoteUpload
            $result.Source = 'manifest-upload'
        }
    }
    return $result
}

# ---------------------------------------------------------------- what is sent

function Get-M2ReportVersionInfo {
    # What a report is read against, whether or not the logs went along: the
    # server's and the client's versions, the engine, the kind of
    # installation, Windows and the few settings that change what a bot
    # does. Settings by name only - never a key .env keeps a secret under.
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $root = [IO.Path]::GetFullPath($ServerRoot).TrimEnd('\')
    $info = [ordered]@{}
    $version = Read-M2ReportTextFile (Join-Path $root 'VERSION')
    $modVersion = Read-M2ReportTextFile (Join-Path $root 'MOD_VERSION')
    $clientPackage = Read-M2ReportTextFile (Join-Path $root 'CLIENT_VERSION')
    $recordedServer = ''
    $recordedClient = ''
    $statePath = Join-Path $root '.m2launcher-state.json'
    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        try {
            $saved = [IO.File]::ReadAllText($statePath) | ConvertFrom-Json
            if ($saved -and $saved.PSObject.Properties['server']) { $recordedServer = ([string]$saved.server).Trim() }
            if ($saved -and $saved.PSObject.Properties['client']) { $recordedClient = ([string]$saved.client).Trim() }
        }
        catch { }
    }
    $info['server'] = $(if ($version) { $version } else { 'unknown' })
    if ($modVersion) { $info['mod'] = $modVersion }
    $info['serverRecorded'] = $(if ($recordedServer) { $recordedServer } else { 'unknown' })
    $info['rebuildPending'] = [bool](Test-Path -LiteralPath (Join-Path $root '.m2launcher-rebuild-pending') -PathType Leaf)
    $info['client'] = $(if ($recordedClient -and $recordedClient -ne 'unknown') { $recordedClient } elseif ($clientPackage) { $clientPackage } else { 'unknown' })
    $info['clientPackage'] = $(if ($clientPackage) { $clientPackage } else { 'unknown' })
    # The launcher ships in the server package: its version is VERSION's.
    $info['launcher'] = $info['server']
    $engine = 'unknown'
    try { $engine = Get-M2ServerEngine -ServerRoot $root } catch { }
    $info['engine'] = $engine
    $info['launcherLanguage'] = $(if ($env:M2_LAUNCHER_LANGUAGE -eq 'en') { 'en' } else { 'pl' })

    $windows = [Environment]::OSVersion.VersionString
    try {
        $current = Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion' -ErrorAction Stop
        $read = { param($name) if ($current.PSObject.Properties[$name]) { return [string]$current.$name }; return '' }
        $product = & $read 'ProductName'
        $build = & $read 'CurrentBuild'
        # Windows 11 still calls itself "Windows 10" there; the build says which.
        if ($build -match '^\d+$' -and [int]$build -ge 22000) { $product = $product -replace 'Windows 10', 'Windows 11' }
        $windows = ('{0} {1} (build {2}.{3}, {4})' -f $product, (& $read 'DisplayVersion'), $build, (& $read 'UBR'),
            $(if ([Environment]::Is64BitOperatingSystem) { '64-bit' } else { '32-bit' })).Replace('  ', ' ')
    }
    catch { }
    $info['windows'] = $windows
    $info['powershell'] = [string]$PSVersionTable.PSVersion
    $memory = 'unknown'
    try {
        $bytes = [double](Get-CimInstance -ClassName Win32_ComputerSystem -ErrorAction Stop).TotalPhysicalMemory
        if ($bytes -gt 0) { $memory = ('{0:N1} GB' -f ($bytes / 1GB)).Replace(',', '.') }
    }
    catch { }
    $info['memory'] = $memory
    $info['cpus'] = [Environment]::ProcessorCount
    $docker = 'unknown'
    try { $docker = $(if (Test-M2DockerRunning) { 'running' } else { 'stopped' }) } catch { }
    $info['docker'] = $docker

    $envPath = Join-Path $root 'linux-port\docker\.env'
    $wanted = [ordered]@{
        PLAYERBOT_AUTOSPAWN_COUNT   = 'bots'
        M2_PLAYERBOT_WORLD_LAYOUT   = 'worldLayout'
        M2_PLAYERBOT_KINGDOMS       = 'kingdoms'
        M2_PLAYERBOT_CH2            = 'channel2'
        M2_CHANNELS                 = 'channels'
        M2_GAME_PORT_BASE           = 'gamePortBase'
        M2_DIFFICULTY               = 'difficulty'
        M2_MONSTER_HP               = 'monsterHp'
    }
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $lines = @()
        try { $lines = [IO.File]::ReadAllLines($envPath) } catch { }
        foreach ($key in @($wanted.Keys)) {
            foreach ($line in $lines) {
                if ($line -match ('^\s*' + [Regex]::Escape($key) + '\s*=(.*)$')) { $info[$wanted[$key]] = $Matches[1].Trim().Trim('"') }
            }
        }
    }
    return $info
}

function Add-M2ReportSecretsFromJson {
    # Every string under a name that says it is secret, and everything under
    # "accounts" - COOP keeps login = password there.
    param($Node, [Parameter(Mandatory = $true)]$Found, [bool]$AllSecret = $false)
    if ($null -eq $Node) { return }
    if ($Node -is [string]) {
        if ($AllSecret) { [void]$Found.Add($Node) }
        return
    }
    if ($Node -is [Array]) {
        foreach ($item in $Node) { Add-M2ReportSecretsFromJson -Node $item -Found $Found -AllSecret $AllSecret }
        return
    }
    if ($Node -is [Management.Automation.PSCustomObject]) {
        foreach ($property in $Node.PSObject.Properties) {
            $secret = $AllSecret -or ($property.Name -match '(?i)pass|secret|token|private|credential|^accounts$')
            Add-M2ReportSecretsFromJson -Node $property.Value -Found $Found -AllSecret $secret
        }
    }
}

function Get-M2ReportSecretValues {
    # The secrets this installation actually holds, by value, so they are
    # masked wherever they stand in a log and whatever the line around them
    # looks like: .env and every copy of it the launcher leaves, COOP's
    # friends and accounts, the launcher's own upload addresses, and the
    # client's saved logins. Nothing here leaves the computer - it is the list
    # of what may not. Six characters at least and not a bare number, or a
    # mask would eat ports and ordinary words.
    param([Parameter(Mandatory = $true)][string]$ServerRoot, $Config = $null)
    $root = [IO.Path]::GetFullPath($ServerRoot).TrimEnd('\')
    $raw = New-Object 'System.Collections.Generic.List[string]'
    $docker = Join-Path $root 'linux-port\docker'
    if (Test-Path -LiteralPath $docker -PathType Container) {
        foreach ($file in @(Get-ChildItem -LiteralPath $docker -Force -File -ErrorAction SilentlyContinue |
                    Where-Object { $_.Name -like '.env*' -and $_.Name -ne '.env.example' })) {
            $lines = @()
            try { $lines = [IO.File]::ReadAllLines($file.FullName) } catch { continue }
            foreach ($line in $lines) {
                if ($line -notmatch '^\s*(?:export\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*=(.*)$') { continue }
                $key = $Matches[1]
                $value = $Matches[2]
                if ($key -match '(?i)PASS|SECRET|TOKEN|PRIVATE|CREDENTIAL|API_?KEY|_KEY$|SALT') { $raw.Add($value) }
            }
        }
    }
    foreach ($name in @('.m2coop.json', '.m2vps.json')) {
        $path = Join-Path $root $name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
        try { Add-M2ReportSecretsFromJson -Node ([IO.File]::ReadAllText($path, [Text.Encoding]::UTF8) | ConvertFrom-Json) -Found $raw } catch { }
    }
    $configPath = Join-Path $root '.m2launcher.json'
    $launcherConfig = $null
    if (Test-Path -LiteralPath $configPath -PathType Leaf) {
        try { $launcherConfig = [IO.File]::ReadAllText($configPath, [Text.Encoding]::UTF8) | ConvertFrom-Json } catch { }
    }
    if ($launcherConfig) {
        Add-M2ReportSecretsFromJson -Node $launcherConfig -Found $raw
        foreach ($name in @('supportUploadUrl', 'reportUrl')) {
            if (-not $launcherConfig.PSObject.Properties[$name]) { continue }
            $url = ([string]$launcherConfig.$name).Trim()
            if (-not $url) { continue }
            $raw.Add($url)
            if ($url -match '[?&]k=([^&#]+)') { $raw.Add([Uri]::UnescapeDataString($Matches[1])) }
            if ($url -match '/api/webhooks/\d+/([^/?#]+)') { $raw.Add($Matches[1]) }
        }
    }
    # The client's saved logins (cache\credentials.json beside the exe): the
    # report never reads the client's folder, and these are masked should a
    # log ever carry one.
    $clientRoot = ''
    foreach ($source in @($Config, $launcherConfig)) {
        if ($clientRoot -or $null -eq $source) { continue }
        if ($source.PSObject.Properties['clientRoot'] -and [string]$source.clientRoot) { $clientRoot = [string]$source.clientRoot }
        elseif ($source.PSObject.Properties['clientExecutable'] -and [string]$source.clientExecutable) {
            try { $clientRoot = Split-Path -Parent ([string]$source.clientExecutable) } catch { }
        }
    }
    if ($clientRoot) {
        $credentials = Join-Path $clientRoot 'cache\credentials.json'
        if (Test-Path -LiteralPath $credentials -PathType Leaf) {
            try { Add-M2ReportSecretsFromJson -Node ([IO.File]::ReadAllText($credentials, [Text.Encoding]::UTF8) | ConvertFrom-Json) -Found $raw } catch { }
        }
    }
    $values = New-Object 'System.Collections.Generic.List[string]'
    foreach ($item in $raw) {
        $text = ([string]$item).Trim().Trim('"').Trim("'").Trim()
        if ($text.Length -lt 6 -or $text -match '^\d+$' -or $text -eq '<redacted>' -or $values.Contains($text)) { continue }
        $values.Add($text)
    }
    return @($values | Sort-Object Length -Descending)
}

function Get-M2ReportScrubRegex {
    # One pass over a file for every known value, whole words only. A value
    # is looked for as it is and as its UTF-8 bytes read one byte a
    # character, which is how Protect-M2ReportFile reads a log.
    param([string[]]$Secrets = @())
    $forms = New-Object 'System.Collections.Generic.List[string]'
    foreach ($secret in @($Secrets)) {
        if (-not $secret) { continue }
        foreach ($form in @($secret, $script:M2ReportLatin1.GetString([Text.Encoding]::UTF8.GetBytes($secret)))) {
            $escaped = [Regex]::Escape($form)
            if (-not $forms.Contains($escaped)) { $forms.Add($escaped) }
        }
    }
    if ($forms.Count -eq 0) { return $null }
    $ordered = @($forms | Sort-Object Length -Descending)
    return [Regex]::new('(?<![A-Za-z0-9])(?:' + ($ordered -join '|') + ')(?![A-Za-z0-9])', [Text.RegularExpressions.RegexOptions]::CultureInvariant)
}

function Protect-M2ReportText {
    # The player's own words go through the same masks as the logs: a
    # password pasted into a description is not the author's to read.
    param([AllowEmptyString()][string]$Text, $Regex = $null)
    if (-not $Text) { return $Text }
    $safe = $Text
    if ($Regex) { $safe = $Regex.Replace($safe, '<redacted>') }
    foreach ($rule in $script:M2ReportPatterns) { $safe = [Regex]::Replace($safe, $rule[0], $rule[1]) }
    # And the launcher module's own rules - the Polish and English password
    # lines, the panel's headings, database URLs - which it does not export.
    $launcher = @(Get-Module -Name Metin2Launcher)
    if ($launcher.Count -gt 0) {
        try { $safe = [string](& $launcher[0] { param($t) Protect-M2LogContent -Text $t } $safe) } catch { }
    }
    return $safe
}

function Protect-M2ReportFile {
    # A file of the bundle masked again by value. Read and written a byte a
    # character, so a CP1250 line from the game keeps every byte but the
    # masked ones; rewritten only when something was masked.
    param([Parameter(Mandatory = $true)][string]$Path, $Regex = $null)
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -eq 0) { return $false }
    $text = $script:M2ReportLatin1.GetString($bytes)
    $safe = $text
    if ($Regex) { $safe = $Regex.Replace($safe, '<redacted>') }
    foreach ($rule in $script:M2ReportPatterns) { $safe = [Regex]::Replace($safe, $rule[0], $rule[1]) }
    if ($safe -ceq $text) { return $false }
    [IO.File]::WriteAllBytes($Path, $script:M2ReportLatin1.GetBytes($safe))
    return $true
}

function Limit-M2ReportFile {
    # A log cut to its last KeepBytes, from the start of a line, under a line
    # that says so: a report is about what happened last.
    param([Parameter(Mandatory = $true)][string]$Path, [Parameter(Mandatory = $true)][long]$KeepBytes)
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -le $KeepBytes) { return $false }
    $start = [int]($bytes.Length - [Math]::Max([long]0, $KeepBytes))
    if ($start -lt $bytes.Length) {
        $newline = [Array]::IndexOf($bytes, [byte]10, $start)
        if ($newline -ge 0 -and $newline + 1 -lt $bytes.Length) { $start = $newline + 1 }
    }
    $kept = $bytes.Length - $start
    $note = (UI-Text '[... zgloszenie: plik skrocony do limitu rozmiaru - zostalo ostatnie {0} z {1} bajtow ...]' '[... report: the file was trimmed to the size limit - kept the last {0} of {1} bytes ...]') -f $kept, $bytes.Length
    $header = [Text.Encoding]::ASCII.GetBytes($note + "`n")
    $stream = [IO.File]::Create($Path)
    try {
        $stream.Write($header, 0, $header.Length)
        $stream.Write($bytes, $start, $kept)
    }
    finally { $stream.Dispose() }
    return $true
}

function Write-M2ReportZip {
    # The form first, the bundle after it, and "/" in every name.
    param([Parameter(Mandatory = $true)][string]$Directory, [Parameter(Mandatory = $true)][string]$Path)
    if (Test-Path -LiteralPath $Path) { Remove-Item -LiteralPath $Path -Force }
    $base = [IO.Path]::GetFullPath($Directory).TrimEnd('\') + '\'
    $files = @(Get-ChildItem -LiteralPath $Directory -Recurse -File -Force | Sort-Object @{
            Expression = {
                $rank = [Array]::IndexOf($script:M2ReportNeverTrimmed, $_.Name)
                if ($rank -ge 0 -and ($_.DirectoryName.TrimEnd('\') + '\') -eq $base) { $rank } else { 99 }
            }
        }, FullName)
    $archive = [IO.Compression.ZipFile]::Open($Path, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in $files) {
            $name = $file.FullName.Substring($base.Length).Replace('\', '/')
            [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $name, [IO.Compression.CompressionLevel]::Optimal)
        }
    }
    finally { $archive.Dispose() }
}

function Get-M2ReportZipEntries {
    param([Parameter(Mandatory = $true)][string]$Path)
    $archive = [IO.Compression.ZipFile]::OpenRead($Path)
    try {
        return @($archive.Entries | ForEach-Object {
                [pscustomobject]@{ Name = $_.FullName; Compressed = [long]$_.CompressedLength; Length = [long]$_.Length }
            })
    }
    finally { $archive.Dispose() }
}

function Write-M2ReportForm {
    # report.txt for a person, report.json for a program: the form, the
    # versions, and what was cut to fit.
    param(
        [Parameter(Mandatory = $true)][string]$Directory,
        [Parameter(Mandatory = $true)]$Request,
        [AllowEmptyString()][string]$Description = '',
        [AllowEmptyString()][string]$Contact = '',
        [Parameter(Mandatory = $true)]$Versions,
        [bool]$HasLogs = $false,
        [string[]]$Trimmed = @(),
        [string[]]$Dropped = @(),
        [AllowEmptyString()][string]$InstallId = ''
    )
    $id = [string]$Request.id
    $lines = New-Object 'System.Collections.Generic.List[string]'
    $lines.Add((UI-Text 'ZGŁOSZENIE Z LAUNCHERA METIN2 SINGLEPLAYER' 'A REPORT FROM THE METIN2 SINGLEPLAYER LAUNCHER'))
    $lines.Add(((UI-Text 'Numer: {0}' 'Number: {0}') -f $id.Substring(0, [Math]::Min(8, $id.Length))))
    $lines.Add(((UI-Text 'Rodzaj: {0}' 'Kind: {0}') -f (Get-M2ReportCategoryText -Category ([string]$Request.category))))
    $lines.Add(((UI-Text 'Utworzono: {0}' 'Made: {0}') -f [string]$Request.created))
    $lines.Add(((UI-Text 'Kontakt: {0}' 'Contact: {0}') -f $(if ($Contact) { $Contact } else { (UI-Text '(nie podano)' '(none given)') })))
    $lines.Add(((UI-Text 'Logi: {0}' 'Logs: {0}') -f $(if ($HasLogs) { (UI-Text 'dołączone' 'attached') } else { (UI-Text 'bez logów' 'none') })))
    if ($InstallId) { $lines.Add(((UI-Text 'Instalacja: {0}' 'Installation: {0}') -f $InstallId.Substring(0, [Math]::Min(8, $InstallId.Length)))) }
    if (@($Trimmed).Count -gt 0) { $lines.Add(((UI-Text 'Skrócone do limitu rozmiaru: {0}' 'Trimmed to the size limit: {0}') -f (@($Trimmed) -join ', '))) }
    if (@($Dropped).Count -gt 0) { $lines.Add(((UI-Text 'Pominięte (za duże): {0}' 'Left out (too large): {0}') -f (@($Dropped) -join ', '))) }
    $lines.Add('')
    $lines.Add((UI-Text 'Opis:' 'Description:'))
    $lines.Add($Description)
    $lines.Add('')
    $lines.Add((UI-Text 'Wersje i system:' 'Versions and system:'))
    foreach ($key in @($Versions.Keys)) { $lines.Add(('  {0}: {1}' -f $key, $Versions[$key])) }
    $text = (($lines -join "`n") -replace "`r`n", "`n") -replace "`n", "`r`n"
    [IO.File]::WriteAllText((Join-Path $Directory 'report.txt'), $text + "`r`n", [Text.UTF8Encoding]::new($false))
    $json = [ordered]@{
        schema      = 1
        kind        = 'metin2-report'
        id          = $id
        installId   = $InstallId
        created     = [string]$Request.created
        category    = [string]$Request.category
        description = $Description
        contact     = $Contact
        language    = [string]$Request.language
        logs        = $HasLogs
        trimmed     = @($Trimmed)
        dropped     = @($Dropped)
        versions    = $Versions
    }
    [IO.File]::WriteAllText((Join-Path $Directory 'report.json'), (ConvertTo-Json -InputObject $json -Depth 5), [Text.UTF8Encoding]::new($false))
}

function New-M2ReportPackage {
    # support-bundles\report-<stamp>.zip: the form, the versions and the
    # bundle's files, masked by value and cut to MaxBytes - the largest logs
    # first, each to its newest lines, and a file whole only when nothing
    # large is left to shorten. Returns the path, the size and what was cut.
    param(
        [Parameter(Mandatory = $true)][string]$ServerRoot,
        [Parameter(Mandatory = $true)]$Request,
        [AllowEmptyString()][string]$BundlePath = '',
        [long]$MaxBytes = 0,
        $Versions = $null,
        $Config = $null,
        [AllowEmptyString()][string]$InstallId = ''
    )
    if ($MaxBytes -le 0) { $MaxBytes = $script:M2ReportMaxZipBytes }
    $root = [IO.Path]::GetFullPath($ServerRoot).TrimEnd('\')
    $folder = Get-M2ReportFolder -ServerRoot $root
    Clear-M2ReportLeftovers -Folder $folder
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $zipPath = Join-Path $folder "report-$stamp.zip"
    for ($n = 2; Test-Path -LiteralPath $zipPath; $n++) { $zipPath = Join-Path $folder "report-$stamp-$n.zip" }
    $work = Join-Path $folder ('.report-work-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path $work -Force | Out-Null
    try {
        $hasLogs = $false
        if ($BundlePath) {
            [IO.Compression.ZipFile]::ExtractToDirectory($BundlePath, $work)
            $hasLogs = $true
        }
        if ($null -eq $Versions) { $Versions = Get-M2ReportVersionInfo -ServerRoot $root }
        $regex = Get-M2ReportScrubRegex -Secrets @(Get-M2ReportSecretValues -ServerRoot $root -Config $Config)
        $description = Protect-M2ReportText -Text ([string]$Request.description) -Regex $regex
        $contact = Protect-M2ReportText -Text ([string]$Request.contact) -Regex $regex
        foreach ($file in @(Get-ChildItem -LiteralPath $work -Recurse -File -Force)) { [void](Protect-M2ReportFile -Path $file.FullName -Regex $regex) }
        $trimmed = New-Object 'System.Collections.Generic.List[string]'
        $dropped = New-Object 'System.Collections.Generic.List[string]'
        Write-M2ReportForm -Directory $work -Request $Request -Description $description -Contact $contact -Versions $Versions -HasLogs $hasLogs -InstallId $InstallId
        $size = [long]0
        for ($pass = 0; $pass -lt 16; $pass++) {
            Write-M2ReportZip -Directory $work -Path $zipPath
            $size = (Get-Item -LiteralPath $zipPath).Length
            if ($size -le $MaxBytes) { break }
            # Aim a tenth under the cap: a file cut by its share of the
            # compressed size is only roughly that much smaller.
            $entries = @(Get-M2ReportZipEntries -Path $zipPath | Where-Object { $script:M2ReportNeverTrimmed -notcontains $_.Name } |
                    Sort-Object Compressed -Descending)
            $excess = [double]($size - [long]($MaxBytes * 0.9))
            $cutAny = $false
            foreach ($entry in $entries) {
                if ($excess -le 0) { break }
                if ($entry.Compressed -lt 4KB -or $entry.Length -lt 8KB -or $dropped.Contains($entry.Name)) { continue }
                $cut = [Math]::Min([double]$entry.Compressed * 0.85, $excess)
                $keep = [long][Math]::Floor($entry.Length * (1.0 - $cut / $entry.Compressed))
                $file = Join-Path $work ($entry.Name.Replace('/', '\'))
                if (Limit-M2ReportFile -Path $file -KeepBytes $keep) {
                    $cutAny = $true
                    if (-not $trimmed.Contains($entry.Name)) { $trimmed.Add($entry.Name) }
                }
                $excess -= $cut
            }
            if (-not $cutAny) {
                $victims = @($entries | Where-Object { -not $dropped.Contains($_.Name) })
                if ($victims.Count -eq 0) { break }
                $file = Join-Path $work ($victims[0].Name.Replace('/', '\'))
                [IO.File]::WriteAllText($file, (UI-Text '[... zgloszenie: plik pominiety, bo nie miescil sie w limicie rozmiaru ...]' '[... report: the file was left out, it did not fit the size limit ...]'))
                $dropped.Add($victims[0].Name)
            }
            Write-M2ReportForm -Directory $work -Request $Request -Description $description -Contact $contact -Versions $Versions -HasLogs $hasLogs `
                -Trimmed @($trimmed) -Dropped @($dropped) -InstallId $InstallId
        }
        if ($size -gt $MaxBytes) {
            Remove-Item -LiteralPath $zipPath -Force -ErrorAction SilentlyContinue
            throw ((UI-Text 'Zgłoszenie nie mieści się w {0:N1} MB nawet po skróceniu logów.' 'The report does not fit in {0:N1} MB even with the logs trimmed.') -f ($MaxBytes / 1MB))
        }
        return [pscustomobject]@{
            Path        = $zipPath
            Bytes       = $size
            HasLogs     = $hasLogs
            Trimmed     = @($trimmed)
            Dropped     = @($dropped)
            Description = $description
            Contact     = $contact
            Versions    = $Versions
        }
    }
    finally {
        if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue }
    }
}

function ConvertTo-M2ReportBody {
    # The one POST body, as UTF-8 bytes: the form and the versions through
    # ConvertTo-Json, the ZIP spliced in as base64 - a 20 MB string is a
    # string JSON never has to escape, and no serializer has to hold it.
    param([Parameter(Mandatory = $true)]$Request, [Parameter(Mandatory = $true)]$Package, [AllowEmptyString()][string]$InstallId = '')
    $zipBytes = [IO.File]::ReadAllBytes($Package.Path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $hash = -join ($sha.ComputeHash($zipBytes) | ForEach-Object { $_.ToString('x2') }) }
    finally { $sha.Dispose() }
    $marker = '@@M2ZIP' + [Guid]::NewGuid().ToString('N') + '@@'
    $meta = [ordered]@{
        schema      = 1
        kind        = 'metin2-report'
        id          = [string]$Request.id
        installId   = $InstallId
        created     = [string]$Request.created
        category    = [string]$Request.category
        description = [string]$Package.Description
        contact     = [string]$Package.Contact
        language    = [string]$Request.language
        logs        = [bool]$Package.HasLogs
        trimmed     = @($Package.Trimmed)
        versions    = $Package.Versions
        zip         = [ordered]@{ name = [IO.Path]::GetFileName($Package.Path); size = $zipBytes.Length; sha256 = $hash; base64 = $marker }
    }
    $json = ConvertTo-Json -InputObject $meta -Depth 6 -Compress
    $quoted = '"' + $marker + '"'
    $at = $json.IndexOf($quoted)
    if ($at -lt 0) { throw 'ConvertTo-M2ReportBody: the zip marker is missing' }
    $head = [Text.Encoding]::UTF8.GetBytes($json.Substring(0, $at) + '"')
    $b64 = [Text.Encoding]::ASCII.GetBytes([Convert]::ToBase64String($zipBytes))
    $tail = [Text.Encoding]::UTF8.GetBytes('"' + $json.Substring($at + $quoted.Length))
    $body = New-Object byte[] ($head.Length + $b64.Length + $tail.Length)
    [Array]::Copy($head, 0, $body, 0, $head.Length)
    [Array]::Copy($b64, 0, $body, $head.Length, $b64.Length)
    [Array]::Copy($tail, 0, $body, $head.Length + $b64.Length, $tail.Length)
    return , $body
}

# ---------------------------------------------------------------- transport

function Invoke-M2ReportHttp {
    param(
        [Parameter(Mandatory = $true)][ValidateSet('GET', 'POST')][string]$Method,
        [Parameter(Mandatory = $true)][string]$Url,
        [byte[]]$Body = $null,
        [int]$TimeoutSec = 600
    )
    Add-Type -AssemblyName System.Net.Http
    # Google speaks TLS 1.2, and Windows PowerShell 5.1's .NET may not offer
    # it unless asked.
    try { [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12 } catch { }
    $handler = [Net.Http.HttpClientHandler]::new()
    # An Apps Script web app answers a POST with a 302 to
    # script.googleusercontent.com, where the answer waits for a GET; .NET
    # Framework's client follows it just so (tests\launcher_report_test.ps1).
    $handler.AllowAutoRedirect = $true
    $client = [Net.Http.HttpClient]::new($handler)
    try {
        $client.Timeout = [TimeSpan]::FromSeconds([Math]::Max(5, $TimeoutSec))
        $client.DefaultRequestHeaders.ExpectContinue = $false
        [void]$client.DefaultRequestHeaders.UserAgent.TryParseAdd('mt2009-plus-launcher/report')
        if ($Method -eq 'POST') {
            $content = [Net.Http.ByteArrayContent]::new($Body)
            $content.Headers.ContentType = [Net.Http.Headers.MediaTypeHeaderValue]::Parse('application/json; charset=utf-8')
            $response = $client.PostAsync($Url, $content).GetAwaiter().GetResult()
        }
        else { $response = $client.GetAsync($Url).GetAwaiter().GetResult() }
        $text = $response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
        return [pscustomobject]@{ Status = [int]$response.StatusCode; Text = [string]$text; Error = '' }
    }
    catch {
        $inner = $_.Exception
        while ($inner.InnerException) { $inner = $inner.InnerException }
        $message = [string]$inner.Message
        if ($inner -is [Threading.Tasks.TaskCanceledException]) { $message = (UI-Text 'brak odpowiedzi w ciągu {0} s' 'no answer within {0} s') -f $TimeoutSec }
        return [pscustomobject]@{ Status = 0; Text = ''; Error = $message }
    }
    finally { $client.Dispose() }
}

function Get-M2ReportRefusalText {
    param([AllowEmptyString()][string]$Code, [AllowEmptyString()][string]$Detail = '')
    $text = switch ($Code) {
        'too_large' { (UI-Text 'zgłoszenie jest za duże dla odbiorcy' 'the report is too large for the receiving side') }
        'rate_limited' { (UI-Text 'odbiorca przyjął już za dużo zgłoszeń w tej godzinie - spróbuj później' 'the receiving side has taken too many reports this hour - try again later') }
        'forbidden' { (UI-Text 'odbiorca nie przyjął klucza z adresu (k=) - sprawdź adres zgłoszeń' 'the receiving side did not take the key in the address (k=) - check the report address') }
        'bad_zip' { (UI-Text 'odbiorca uznał paczkę z logami za uszkodzoną' 'the receiving side found the log package damaged') }
        'bad_json' { (UI-Text 'odbiorca nie odczytał zgłoszenia' 'the receiving side could not read the report') }
        'bad_request' { (UI-Text 'odbiorca uznał zgłoszenie za niepełne' 'the receiving side found the report incomplete') }
        'internal' { (UI-Text 'błąd po stronie odbiorcy zgłoszeń' 'an error on the receiving side') }
        default { (UI-Text 'odbiorca odrzucił zgłoszenie' 'the receiving side refused the report') }
    }
    if ($Code) { $text += ' [' + $Code + ']' }
    if ($Detail) { $text += ': ' + $Detail }
    return $text
}

function ConvertFrom-M2ReportReply {
    # The web app's answer read for what it says: Apps Script answers 200 to
    # everything, a Google sign-in page and an error page included.
    param([Parameter(Mandatory = $true)]$Http)
    $reply = [pscustomobject]@{ Ok = $false; Id = ''; Error = ''; Message = ''; MaxZipBytes = [long]0; Service = ''; Status = [int]$Http.Status }
    if ($Http.Error) {
        $reply.Error = 'network'
        $reply.Message = (UI-Text 'brak połączenia z adresem zgłoszeń ({0})' 'no connection to the report address ({0})') -f $Http.Error
        return $reply
    }
    $json = $null
    try { if (([string]$Http.Text).Trim()) { $json = [string]$Http.Text | ConvertFrom-Json } } catch { $json = $null }
    if ($json -and $json -isnot [string] -and $json.PSObject.Properties['ok']) {
        $reply.Ok = ($json.ok -eq $true)
        if ($json.PSObject.Properties['id'] -and $null -ne $json.id) { $reply.Id = [string]$json.id }
        if ($json.PSObject.Properties['error'] -and $null -ne $json.error) { $reply.Error = [string]$json.error }
        if ($json.PSObject.Properties['service'] -and $null -ne $json.service) { $reply.Service = [string]$json.service }
        if ($json.PSObject.Properties['maxZipBytes']) { try { $reply.MaxZipBytes = [long]$json.maxZipBytes } catch { } }
        if (-not $reply.Ok) {
            $detail = ''
            if ($json.PSObject.Properties['message'] -and $null -ne $json.message) { $detail = [string]$json.message }
            $reply.Message = Get-M2ReportRefusalText -Code $reply.Error -Detail $detail
        }
        return $reply
    }
    $snippet = ([Regex]::Replace([string]$Http.Text, '<[^>]*>', ' ') -replace '\s+', ' ').Trim()
    if ($snippet.Length -gt 160) { $snippet = $snippet.Substring(0, 160) + '...' }
    $reply.Error = 'not_endpoint'
    $reply.Message = (UI-Text 'adres odpowiedział czymś, co nie jest aplikacją zgłoszeń (HTTP {0}: {1}) - aplikacja internetowa musi być wdrożona z dostępem "Każdy"' 'the address answered with something that is not the report web app (HTTP {0}: {1}) - the web app has to be deployed with access "Anyone"') -f $Http.Status, $snippet
    return $reply
}

function Get-M2ReportEndpointInfo {
    # The web app's GET: that it is there and answers as itself, and the ZIP
    # size it takes. Asked before the logs are collected, so a wrong address
    # costs a second and not an upload.
    param([Parameter(Mandatory = $true)][string]$Url, [int]$TimeoutSec = 30)
    $reply = ConvertFrom-M2ReportReply -Http (Invoke-M2ReportHttp -Method GET -Url $Url -TimeoutSec $TimeoutSec)
    if ($reply.Ok -and $reply.Service -ne 'metin2-report') {
        $reply.Ok = $false
        $reply.Error = 'not_endpoint'
        $reply.Message = (UI-Text 'adres odpowiada, ale nie jako aplikacja zgłoszeń ({0})' 'the address answers, but not as the report web app ({0})') -f $reply.Service
    }
    return $reply
}

function Send-M2Report {
    param([Parameter(Mandatory = $true)][string]$Url, [Parameter(Mandatory = $true)][byte[]]$Body, [int]$TimeoutSec = 600)
    if (-not (Test-M2ReportUrl -Url $Url)) {
        throw ((UI-Text 'To nie jest adres aplikacji zgłoszeń (https): {0}' 'This is not an address of the report web app (https): {0}') -f $Url)
    }
    return (ConvertFrom-M2ReportReply -Http (Invoke-M2ReportHttp -Method POST -Url $Url -Body $Body -TimeoutSec $TimeoutSec))
}

function Get-M2ReportDiscordText {
    # The message a Discord webhook posts beside the ZIP: what the report is,
    # at a glance. The whole description is report.txt in the ZIP.
    param([Parameter(Mandatory = $true)]$Request, [Parameter(Mandatory = $true)]$Package, [AllowEmptyString()][string]$InstallId = '')
    $id = ([string]$Request.id)
    $short = $id.Substring(0, [Math]::Min(8, $id.Length))
    $versions = $Package.Versions
    $pick = { param($k) if ($versions -and $versions.Contains($k)) { [string]$versions[$k] } else { '?' } }
    $head = New-Object 'System.Collections.Generic.List[string]'
    $head.Add(('**Zgłoszenie {0}** - {1}' -f $short, (Get-M2ReportCategoryText -Category ([string]$Request.category))))
    $head.Add(('Serwer {0}, klient {1}, silnik {2}, logi: {3}' -f (& $pick 'server'), (& $pick 'client'), (& $pick 'engine'), $(if ($Package.HasLogs) { 'tak' } else { 'nie' })))
    if ([string]$Package.Contact) { $head.Add(('Kontakt: {0}' -f [string]$Package.Contact)) }
    if ($InstallId) { $head.Add(('Instalacja: {0}' -f $InstallId.Substring(0, [Math]::Min(8, $InstallId.Length)))) }
    $text = ($head -join "`n") + "`n```````n"
    $room = $script:M2ReportDiscordMaxContent - $text.Length - 40
    $description = ([string]$Package.Description) -replace '```', "'''"
    if ($description.Length -gt $room) { $description = $description.Substring(0, [Math]::Max(0, $room)) + "`n[... reszta w report.txt]" }
    return ($text + $description + "`n``````")
}

function Send-M2ReportDiscord {
    # One webhook message: the text above and the ZIP as its attachment.
    # ?wait=true makes Discord answer with the message it made, so a sent
    # report has a number the owner can find in the channel.
    param([Parameter(Mandatory = $true)][string]$Url, [Parameter(Mandatory = $true)]$Request, [Parameter(Mandatory = $true)]$Package, [AllowEmptyString()][string]$InstallId = '', [int]$TimeoutSec = 600)
    $reply = [pscustomobject]@{ Ok = $false; Id = ''; Error = ''; Message = ''; MaxZipBytes = [long]0; Service = 'discord'; Status = 0 }
    if (-not (Test-M2ReportDiscordUrl -Url $Url)) {
        $reply.Error = 'bad_request'
        $reply.Message = (UI-Text 'to nie jest adres webhooka Discorda' 'this is not a Discord webhook address')
        return $reply
    }
    $target = $Url.Trim()
    if ($target -notmatch '[?&]wait=') { $target += $(if ($target.Contains('?')) { '&wait=true' } else { '?wait=true' }) }
    Add-Type -AssemblyName System.Net.Http
    try { [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12 } catch { }
    $client = [Net.Http.HttpClient]::new()
    $form = [Net.Http.MultipartFormDataContent]::new()
    $stream = $null
    try {
        $client.Timeout = [TimeSpan]::FromSeconds([Math]::Max(5, $TimeoutSec))
        [void]$client.DefaultRequestHeaders.UserAgent.TryParseAdd('mt2009-plus-launcher/report')
        $payload = [ordered]@{ content = (Get-M2ReportDiscordText -Request $Request -Package $Package -InstallId $InstallId); allowed_mentions = @{ parse = @() } }
        $form.Add([Net.Http.StringContent]::new((ConvertTo-Json -InputObject $payload -Depth 4 -Compress), [Text.Encoding]::UTF8, 'application/json'), 'payload_json')
        $stream = [IO.File]::OpenRead($Package.Path)
        $file = [Net.Http.StreamContent]::new($stream)
        $file.Headers.ContentType = [Net.Http.Headers.MediaTypeHeaderValue]::Parse('application/zip')
        $form.Add($file, 'files[0]', [IO.Path]::GetFileName($Package.Path))
        $response = $client.PostAsync($target, $form).GetAwaiter().GetResult()
        $text = $response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
        $reply.Status = [int]$response.StatusCode
        if ($response.IsSuccessStatusCode) {
            $reply.Ok = $true
            try { $json = $text | ConvertFrom-Json; if ($json -and $json.PSObject.Properties['id']) { $reply.Id = [string]$json.id } } catch { }
            return $reply
        }
        $reply.Error = $(switch ($reply.Status) { 413 { 'too_large' } 429 { 'rate_limited' } 401 { 'forbidden' } 403 { 'forbidden' } 404 { 'forbidden' } default { 'internal' } })
        $snippet = ([string]$text -replace '\s+', ' ').Trim()
        if ($snippet.Length -gt 160) { $snippet = $snippet.Substring(0, 160) + '...' }
        $reply.Message = Get-M2ReportRefusalText -Code $reply.Error -Detail ('HTTP {0} {1}' -f $reply.Status, $snippet)
        return $reply
    }
    catch {
        $inner = $_.Exception
        while ($inner.InnerException) { $inner = $inner.InnerException }
        $reply.Error = 'network'
        $reply.Message = (UI-Text 'brak połączenia z adresem zgłoszeń ({0})' 'no connection to the report address ({0})') -f [string]$inner.Message
        return $reply
    }
    finally {
        if ($stream) { $stream.Dispose() }
        $form.Dispose()
        $client.Dispose()
    }
}

# ---------------------------------------------------------------- the action

function Invoke-M2Report {
    # The whole report: where it goes, the endpoint's own size limit, the
    # logs when the form wants them, the package, and then the POST or - with
    # no address, or one that does not answer as the web app - the ZIP kept
    # for the player to send. Returns Outcome (sent, saved, failed), the
    # report's number, the ZIP's path and the message the player reads; with
    # -ResultPath the window's copy of it too.
    param(
        [Parameter(Mandatory = $true)][string]$ServerRoot,
        [Parameter(Mandatory = $true)]$Request,
        $Config = $null,
        $Manifest = $null,
        [switch]$NoRemote,
        [hashtable]$ExtraFiles = @{},
        [AllowEmptyString()][string]$BundlePath = '',
        [AllowEmptyString()][string]$ResultPath = '',
        [long]$MaxBytes = 0
    )
    $root = [IO.Path]::GetFullPath($ServerRoot).TrimEnd('\')
    $problems = @(Test-M2ReportRequest -Request $Request)
    if ($problems.Count -gt 0) { throw ($problems -join ' ') }
    if ($null -eq $Config) { $Config = Get-M2LauncherConfig -ServerRoot $root -ConfigPath (Join-Path $root '.m2launcher.json') }
    $state = Get-M2ReportState -ServerRoot $root
    $settings = Get-M2ReportSettings -Config $Config -Manifest $Manifest -NoRemote:$NoRemote
    if ($settings.Problem) { Write-Host $settings.Problem -ForegroundColor Yellow }
    $cap = $(if ($MaxBytes -gt 0) { $MaxBytes } else { $script:M2ReportMaxZipBytes })
    $probe = $null
    $discord = [bool]($settings.Url -and (Test-M2ReportDiscordUrl -Url $settings.Url))
    # A Discord webhook takes no probe (its GET would say nothing about the
    # upload) and a smaller ZIP than a web app.
    if ($discord) { $cap = [Math]::Min($cap, $script:M2ReportDiscordMaxZipBytes) }
    elseif ($settings.Url) {
        Write-Host (UI-Text 'Sprawdzam adres zgłoszeń...' 'Checking the report address...') -ForegroundColor Cyan
        $probe = Get-M2ReportEndpointInfo -Url $settings.Url
        if ($probe.Ok -and $probe.MaxZipBytes -gt 0) { $cap = [Math]::Max($script:M2ReportMinZipBytes, [Math]::Min($cap, $probe.MaxZipBytes)) }
    }

    $bundle = $BundlePath
    $madeBundle = $false
    if ($Request.attachLogs -and -not $bundle) {
        Write-Host (UI-Text 'Zbieram logi do zgłoszenia - to może potrwać minutę...' 'Collecting the logs for the report - this can take a minute...') -ForegroundColor Cyan
        try {
            $bundle = New-M2SupportBundle -ServerRoot $root -ExtraFiles $ExtraFiles
            $madeBundle = $true
        }
        catch {
            Write-Host ((UI-Text 'Nie udało się zebrać logów ({0}) - zgłoszenie pójdzie bez nich.' 'Could not collect the logs ({0}) - the report goes without them.') -f $_.Exception.Message) -ForegroundColor Yellow
            $bundle = ''
        }
    }
    try {
        $package = New-M2ReportPackage -ServerRoot $root -Request $Request -BundlePath $bundle -MaxBytes $cap -Config $Config -InstallId $state.installId
    }
    finally {
        # The bundle was this report's own step: its files are in the report.
        if ($madeBundle -and $bundle -and (Test-Path -LiteralPath $bundle -PathType Leaf)) { Remove-Item -LiteralPath $bundle -Force -ErrorAction SilentlyContinue }
    }
    Write-Host ((UI-Text 'Zgłoszenie: {0} ({1:N2} MB)' 'The report: {0} ({1:N2} MB)') -f $package.Path, ($package.Bytes / 1MB)) -ForegroundColor Gray
    if (@($package.Trimmed).Count -gt 0 -or @($package.Dropped).Count -gt 0) {
        Write-Host ((UI-Text 'Logi skrócone do {0:N1} MB (najstarsze linie): {1}' 'Logs trimmed to {0:N1} MB (the oldest lines): {1}') -f ($cap / 1MB), (@(@($package.Trimmed) + @($package.Dropped)) -join ', ')) -ForegroundColor Gray
    }

    $result = [pscustomobject]@{
        Outcome    = ''
        Ok         = $false
        Id         = ''
        Path       = $package.Path
        Bytes      = $package.Bytes
        Message    = ''
        ContactUrl = $settings.ContactUrl
        Source     = $settings.Source
    }
    if (-not $settings.Url) {
        $result.Outcome = 'saved'
        $result.Message = (UI-Text 'Wysyłanie zgłoszeń nie jest jeszcze włączone. Zgłoszenie zapisano jako {0} - wyślij ten plik autorowi na Discordzie: {1}' 'Sending reports is not switched on yet. The report is saved as {0} - send this file to the author on Discord: {1}') -f $package.Path, $settings.ContactUrl
    }
    elseif ($probe -and -not $probe.Ok) {
        $result.Outcome = 'failed'
        $result.Message = (UI-Text 'Nie udało się wysłać zgłoszenia: {0}. Zgłoszenie zapisano jako {1} - wyślij ten plik autorowi na Discordzie: {2}' 'The report could not be sent: {0}. It is saved as {1} - send this file to the author on Discord: {2}') -f $probe.Message, $package.Path, $settings.ContactUrl
    }
    else {
        Write-Host ((UI-Text 'Wysyłam zgłoszenie ({0:N2} MB)...' 'Sending the report ({0:N2} MB)...') -f ($package.Bytes / 1MB)) -ForegroundColor Cyan
        if ($discord) { $reply = Send-M2ReportDiscord -Url $settings.Url -Request $Request -Package $package -InstallId $state.installId }
        else {
            $body = ConvertTo-M2ReportBody -Request $Request -Package $package -InstallId $state.installId
            $reply = Send-M2Report -Url $settings.Url -Body $body
        }
        if ($reply.Ok) {
            $result.Outcome = 'sent'
            $result.Ok = $true
            $result.Id = $(if ($reply.Id) { $reply.Id } else { ([string]$Request.id).Substring(0, 8) })
            $result.Message = (UI-Text 'Zgłoszenie wysłane - dziękujemy! Numer zgłoszenia: {0}. Kopia: {1}' 'The report is sent - thank you! Report number: {0}. A copy: {1}') -f $result.Id, $package.Path
            $state.lastSent = (Get-Date).ToString('s')
        }
        else {
            $result.Outcome = 'failed'
            $result.Message = (UI-Text 'Nie udało się wysłać zgłoszenia: {0}. Zgłoszenie zapisano jako {1} - wyślij ten plik autorowi na Discordzie: {2}' 'The report could not be sent: {0}. It is saved as {1} - send this file to the author on Discord: {2}') -f $reply.Message, $package.Path, $settings.ContactUrl
        }
    }
    if ([string]$Request.contact) { $state.lastContact = [string]$Request.contact }
    Save-M2ReportState -ServerRoot $root -State $state
    if ($ResultPath) { Write-M2ReportResult -Path $ResultPath -Result $result }
    Write-Host $result.Message -ForegroundColor $(if ($result.Outcome -eq 'sent') { 'Green' } else { 'Yellow' })
    return $result
}

function Read-M2ReportFromConsole {
    # The text launcher's form. An empty description sends nothing.
    param([Parameter(Mandatory = $true)][string]$ServerRoot, [AllowEmptyString()][string]$Category = '', [AllowEmptyString()][string]$Contact = '', [switch]$NoLogs)
    Write-Host (UI-Text 'Zgłoszenie do autora projektu: błąd, propozycja albo coś innego.' 'A report to the project''s author: a bug, a suggestion or anything else.') -ForegroundColor Cyan
    if (-not $Category) { $Category = Read-Host (UI-Text 'Rodzaj: 1 = błąd, 2 = propozycja, 3 = inne [1]' 'Kind: 1 = bug, 2 = suggestion, 3 = other [1]') }
    $kind = ConvertTo-M2ReportCategory -Value $Category
    Write-Host (UI-Text 'Opisz, co się stało albo co proponujesz. Możesz pisać w kilku liniach; pusta linia kończy opis.' 'Describe what happened or what you suggest. You can write several lines; an empty line ends the description.')
    $lines = @()
    while ($true) {
        $line = Read-Host '>'
        if (-not $line) { break }
        $lines += $line
    }
    $description = $lines -join "`n"
    if (-not $description.Trim()) { return $null }
    if (-not $Contact) {
        $state = Get-M2ReportState -ServerRoot $ServerRoot
        $Contact = Read-Host (UI-Text 'Kontakt, jeśli autor ma odpisać (nick na Discordzie albo e-mail; Enter = bez kontaktu)' 'A contact, should the author answer (a Discord name or an e-mail; Enter = none)')
        if (-not $Contact -and $state.lastContact) {
            $again = Read-Host ((UI-Text 'Użyć poprzedniego kontaktu {0}? [t/N]' 'Use the previous contact {0}? [y/N]') -f $state.lastContact)
            if ($again -match '^(t|tak|y|yes)$') { $Contact = $state.lastContact }
        }
    }
    $attach = -not $NoLogs
    if ($attach) {
        $answer = Read-Host (UI-Text 'Dołączyć logi (launcher, serwer, baza; hasła są maskowane)? [T/n]' 'Attach the logs (the launcher, the server, the database; passwords are masked)? [Y/n]')
        if ($answer -match '^(n|nie|no)$') { $attach = $false }
    }
    return (New-M2ReportRequest -Category $kind -Description $description -Contact $Contact -AttachLogs $attach)
}

# ---------------------------------------------------------------- the window

function Get-M2ReportDestinationText {
    # Where the window's report goes, from what is known without a network:
    # the launcher's own reportUrl and the manifest the window already read.
    param($Config = $null, $Manifest = $null)
    $url = ''
    if ($null -ne $Config) {
        try { $url = (Get-M2ReportSettings -Config $Config -Manifest $Manifest -NoRemote).Url } catch { }
    }
    if ($url) { return (UI-Text 'Zgłoszenie trafi prosto do autora projektu.' 'The report goes straight to the project''s author.') }
    if ($null -ne $Manifest) {
        return (UI-Text 'Wysyłanie zgłoszeń nie jest jeszcze włączone: zapiszę zgłoszenie jako plik ZIP i otworzę jego folder - wyślesz go autorowi na Discordzie.' 'Sending reports is not switched on yet: I will save the report as a ZIP file and open its folder - you send it to the author on Discord.')
    }
    return (UI-Text 'Zgłoszenie trafi do autora projektu, a jeśli wysyłanie nie jest jeszcze włączone - zapiszę je jako plik ZIP do wysłania na Discordzie.' 'The report goes to the project''s author - or, if sending is not switched on yet, is saved as a ZIP file for you to send on Discord.')
}

function New-M2ReportForm {
    # The window's form, built and not shown, so a test can fill it in and
    # draw it. Its controls are in the returned table and in the form's Tag,
    # where the Send button's handler finds them.
    param($Draft = $null, [AllowEmptyString()][string]$Destination = '', [AllowEmptyString()][string]$Contact = '')
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    $gray = [Drawing.Color]::FromArgb(90, 90, 90)
    $form = [Windows.Forms.Form]::new()
    $form.Text = (UI-Text 'Zgłoś błąd lub pomysł' 'Report a bug or an idea')
    $form.ClientSize = [Drawing.Size]::new(620, 612)
    $form.StartPosition = 'CenterParent'
    $form.FormBorderStyle = 'FixedDialog'
    $form.MaximizeBox = $false
    $form.MinimizeBox = $false
    $form.ShowInTaskbar = $false
    $form.Font = [Drawing.Font]::new('Segoe UI', 9)

    $add = {
        param($Control, [int]$X, [int]$Y, [int]$Width, [int]$Height)
        $Control.Location = [Drawing.Point]::new($X, $Y)
        $Control.Size = [Drawing.Size]::new($Width, $Height)
        $form.Controls.Add($Control)
        return $Control
    }
    $intro = [Windows.Forms.Label]::new()
    $intro.Text = (UI-Text 'Opisz, co się stało albo co chcesz zaproponować. Autor projektu dostanie opis, wersje serwera i klienta oraz - jeśli zostawisz zaznaczone - logi.' 'Describe what happened or what you would like to suggest. The project''s author gets the description, the server''s and the client''s versions and - if you leave it ticked - the logs.')
    [void](& $add $intro 16 12 588 36)

    $kindLabel = [Windows.Forms.Label]::new()
    $kindLabel.Text = (UI-Text 'Rodzaj:' 'Kind:')
    [void](& $add $kindLabel 16 58 70 20)
    $bug = [Windows.Forms.RadioButton]::new()
    $bug.Text = (UI-Text 'Błąd' 'Bug')
    [void](& $add $bug 92 55 110 24)
    $suggestion = [Windows.Forms.RadioButton]::new()
    $suggestion.Text = (UI-Text 'Propozycja' 'Suggestion')
    [void](& $add $suggestion 210 55 130 24)
    $other = [Windows.Forms.RadioButton]::new()
    $other.Text = (UI-Text 'Inne' 'Other')
    [void](& $add $other 348 55 110 24)

    $descriptionLabel = [Windows.Forms.Label]::new()
    $descriptionLabel.Text = (UI-Text 'Opis (wymagany):' 'Description (required):')
    [void](& $add $descriptionLabel 16 88 588 18)
    $description = [Windows.Forms.TextBox]::new()
    $description.Multiline = $true
    $description.AcceptsReturn = $true
    $description.ScrollBars = 'Vertical'
    $description.WordWrap = $true
    $description.MaxLength = $script:M2ReportMaxDescription
    [void](& $add $description 16 108 588 190)
    $hint = [Windows.Forms.Label]::new()
    $hint.Text = (UI-Text 'Co robiłeś, co się stało i czego się spodziewałeś. Przy błędzie: o której godzinie i czy się powtarza.' 'What you did, what happened and what you expected. For a bug: at what time, and whether it happens again.')
    $hint.ForeColor = $gray
    [void](& $add $hint 16 300 588 34)

    $contactLabel = [Windows.Forms.Label]::new()
    $contactLabel.Text = (UI-Text 'Kontakt (opcjonalnie) - nick na Discordzie albo e-mail, jeśli autor ma odpisać:' 'Contact (optional) - a Discord name or an e-mail, should the author answer:')
    [void](& $add $contactLabel 16 346 588 18)
    $contactBox = [Windows.Forms.TextBox]::new()
    $contactBox.MaxLength = $script:M2ReportMaxContact
    [void](& $add $contactBox 16 366 588 24)

    $attach = [Windows.Forms.CheckBox]::new()
    $attach.Text = (UI-Text 'Dołącz logi (zalecane przy błędzie)' 'Attach the logs (recommended for a bug)')
    $attach.Checked = $true
    [void](& $add $attach 16 402 588 22)
    $logsNote = [Windows.Forms.Label]::new()
    $logsNote.Text = (UI-Text 'Logi to dzienniki launchera z ostatnich dni, logi serwera gry i bazy z Dockera, stan kontenerów, wersje, Windows i ustawienia serwera. Hasła, klucze i dane COOP są maskowane przed wysłaniem, a zapisane loginy klienta gry nie są dołączane. Zbieranie trwa do minuty.' 'The logs are the launcher''s logs of the last days, the game server''s and the database''s logs from Docker, the containers'' state, the versions, Windows and the server''s settings. Passwords, keys and COOP data are masked before sending, and the game client''s saved logins are never included. Collecting takes up to a minute.')
    $logsNote.ForeColor = $gray
    [void](& $add $logsNote 34 426 570 66)

    $destinationLabel = [Windows.Forms.Label]::new()
    $destinationLabel.Text = $Destination
    [void](& $add $destinationLabel 16 498 588 36)
    $errorLabel = [Windows.Forms.Label]::new()
    $errorLabel.ForeColor = [Drawing.Color]::Firebrick
    [void](& $add $errorLabel 16 536 588 20)

    $send = [Windows.Forms.Button]::new()
    $send.Text = (UI-Text 'Wyślij' 'Send')
    [void](& $add $send 384 566 108 32)
    $cancel = [Windows.Forms.Button]::new()
    $cancel.Text = (UI-Text 'Anuluj' 'Cancel')
    $cancel.DialogResult = [Windows.Forms.DialogResult]::Cancel
    [void](& $add $cancel 500 566 104 32)
    $form.CancelButton = $cancel

    $bug.Checked = $true
    $contactBox.Text = $Contact
    if ($null -ne $Draft) {
        $kind = 'bug'
        try { $kind = ConvertTo-M2ReportCategory -Value ([string]$Draft.category) } catch { }
        $suggestion.Checked = ($kind -eq 'suggestion')
        $other.Checked = ($kind -eq 'other')
        $bug.Checked = ($kind -eq 'bug')
        $description.Text = ([string]$Draft.description) -replace "(?<!`r)`n", "`r`n"
        if ([string]$Draft.contact) { $contactBox.Text = [string]$Draft.contact }
        $attach.Checked = [bool]$Draft.attachLogs
    }
    $ui = @{
        Form = $form; Bug = $bug; Suggestion = $suggestion; Other = $other; Description = $description
        Contact = $contactBox; AttachLogs = $attach; Error = $errorLabel; Destination = $destinationLabel
        Send = $send; Cancel = $cancel
    }
    $form.Tag = $ui
    # The handler finds the form's controls through the button itself: an
    # event runs long after this function's variables are gone.
    $send.Add_Click({ Complete-M2ReportForm -Ui $this.FindForm().Tag })
    $form.Add_Shown({ $this.Tag.Description.Focus() })
    return $ui
}

function Read-M2ReportForm {
    param([Parameter(Mandatory = $true)][hashtable]$Ui)
    $kind = 'bug'
    if ($Ui.Suggestion.Checked) { $kind = 'suggestion' }
    elseif ($Ui.Other.Checked) { $kind = 'other' }
    return [pscustomobject]@{
        category    = $kind
        description = ([string]$Ui.Description.Text -replace "`r`n", "`n").Trim()
        contact     = ([string]$Ui.Contact.Text).Trim()
        attachLogs  = [bool]$Ui.AttachLogs.Checked
    }
}

function Complete-M2ReportForm {
    # Send: a form that may go closes the window with OK; one that may not
    # says why under the fields and keeps everything typed.
    param([Parameter(Mandatory = $true)][hashtable]$Ui)
    $problems = @(Test-M2ReportRequest -Request (Read-M2ReportForm -Ui $Ui))
    if ($problems.Count -gt 0) {
        $Ui.Error.Text = $problems -join ' '
        [void]$Ui.Description.Focus()
        return $false
    }
    $Ui.Error.Text = ''
    $Ui.Form.DialogResult = [Windows.Forms.DialogResult]::OK
    return $true
}

function Show-M2ReportDialog {
    # The window's ZGLOS / REPORT: the form, filled in from an unsent draft
    # and the last contact. Returns the request, or nothing when cancelled -
    # and a cancelled form that holds a description is kept as the draft.
    param([Parameter(Mandatory = $true)][string]$ServerRoot, $Owner = $null, $Config = $null, $Manifest = $null)
    $state = Get-M2ReportState -ServerRoot $ServerRoot
    $ui = New-M2ReportForm -Draft (Get-M2ReportDraft -ServerRoot $ServerRoot) -Destination (Get-M2ReportDestinationText -Config $Config -Manifest $Manifest) -Contact $state.lastContact
    try {
        $answer = if ($Owner) { $ui.Form.ShowDialog($Owner) } else { $ui.Form.ShowDialog() }
        $typed = Read-M2ReportForm -Ui $ui
        if ($answer -ne [Windows.Forms.DialogResult]::OK) {
            if ($typed.description) { [void](Save-M2ReportRequest -ServerRoot $ServerRoot -Request $typed -Draft) }
            else { Clear-M2ReportDrafts -ServerRoot $ServerRoot }
            return $null
        }
        Clear-M2ReportDrafts -ServerRoot $ServerRoot
        return (New-M2ReportRequest -Category $typed.category -Description $typed.description -Contact $typed.contact -AttachLogs $typed.attachLogs)
    }
    finally { $ui.Form.Dispose() }
}

function Show-M2ReportOutcome {
    # What became of the report, from the action's answer: sent, or saved as
    # a ZIP whose folder opens with the file selected.
    param([Parameter(Mandatory = $true)]$Result, $Owner = $null)
    Add-Type -AssemblyName System.Windows.Forms
    $outcome = [string]$Result.outcome
    $path = [string]$Result.path
    $contact = [string]$Result.contactUrl
    if (-not $contact) { $contact = $script:M2ReportDefaultContact }
    $icon = [Windows.Forms.MessageBoxIcon]::Information
    if ($outcome -eq 'sent') {
        $title = (UI-Text 'Zgłoszenie wysłane' 'Report sent')
        $text = (UI-Text "Dziękujemy! Zgłoszenie dotarło do autora projektu.`r`n`r`nNumer zgłoszenia: {0}`r`nKopia na dysku: {1}" "Thank you! The report reached the project's author.`r`n`r`nReport number: {0}`r`nA copy on disk: {1}") -f [string]$Result.id, $path
    }
    elseif ($outcome -eq 'saved') {
        $title = (UI-Text 'Zgłoszenie zapisane' 'Report saved')
        $text = (UI-Text "Wysyłanie zgłoszeń nie jest jeszcze włączone, więc zgłoszenie zapisano jako plik:`r`n{0}`r`n`r`nOtwieram jego folder - wyślij ten plik autorowi na Discordzie:`r`n{1}" "Sending reports is not switched on yet, so the report is saved as a file:`r`n{0}`r`n`r`nIts folder opens now - send this file to the author on Discord:`r`n{1}") -f $path, $contact
    }
    else {
        $title = (UI-Text 'Zgłoszenie niewysłane' 'Report not sent')
        $icon = [Windows.Forms.MessageBoxIcon]::Warning
        $text = (UI-Text "Nie udało się wysłać zgłoszenia.`r`n`r`n{0}`r`n`r`nOtwieram folder zgłoszenia - wyślij ten plik autorowi na Discordzie:`r`n{1}" "The report could not be sent.`r`n`r`n{0}`r`n`r`nThe report's folder opens now - send this file to the author on Discord:`r`n{1}") -f [string]$Result.message, $contact
    }
    if ($Owner) { [void][Windows.Forms.MessageBox]::Show($Owner, $text, $title, [Windows.Forms.MessageBoxButtons]::OK, $icon) }
    else { [void][Windows.Forms.MessageBox]::Show($text, $title, [Windows.Forms.MessageBoxButtons]::OK, $icon) }
    if ($outcome -ne 'sent' -and $path -and (Test-Path -LiteralPath $path -PathType Leaf)) {
        Start-Process -FilePath 'explorer.exe' -ArgumentList ('/select,"{0}"' -f $path)
    }
}

Export-ModuleMember -Function @(
    'Get-M2ReportFolder',
    'Get-M2ReportState',
    'Save-M2ReportState',
    'ConvertTo-M2ReportCategory',
    'Get-M2ReportCategoryText',
    'Test-M2ReportRequest',
    'New-M2ReportRequest',
    'Save-M2ReportRequest',
    'Read-M2ReportRequest',
    'Get-M2ReportDraft',
    'Clear-M2ReportDrafts',
    'Get-M2ReportResultPath',
    'Write-M2ReportResult',
    'Read-M2ReportResult',
    'Test-M2ReportDiscordUrl',
    'Test-M2ReportUrl',
    'Get-M2ReportSettings',
    'Get-M2ReportVersionInfo',
    'Get-M2ReportSecretValues',
    'Get-M2ReportScrubRegex',
    'Protect-M2ReportText',
    'Protect-M2ReportFile',
    'Limit-M2ReportFile',
    'New-M2ReportPackage',
    'ConvertTo-M2ReportBody',
    'Get-M2ReportEndpointInfo',
    'Send-M2Report',
    'Get-M2ReportDiscordText',
    'Send-M2ReportDiscord',
    'Invoke-M2Report',
    'Read-M2ReportFromConsole',
    'Get-M2ReportDestinationText',
    'New-M2ReportForm',
    'Read-M2ReportForm',
    'Complete-M2ReportForm',
    'Show-M2ReportDialog',
    'Show-M2ReportOutcome'
)
