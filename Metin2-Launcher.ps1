[CmdletBinding()]
param(
    [ValidateSet('Menu', 'Start', 'Stop', 'StartDocker', 'StopAll', 'Check', 'UpdateServer', 'UpdateClient', 'UpdateAll', 'RepairClientExe', 'Diagnose', 'Logs', 'SendLogs', 'Configure', 'SetBots', 'SetDifficulty', 'ImportDb', 'BackupDb', 'RestoreDb', 'ResetWorld', 'RepairDb', 'DbAccess', 'PanelPassword', 'FreePorts', 'CoopCheck', 'CoopSecure', 'CoopAddFriend', 'CoopBlockFriend', 'CoopUnblockFriend', 'CoopInvite', 'CoopHost', 'CoopStop', 'CoopRenew', 'CoopJoin', 'VpsConnect', 'VpsCheck', 'VpsInstall', 'VpsUpdate', 'VpsStatus', 'VpsPanel', 'VpsPanelClose', 'VpsLogs', 'VpsPasswords', 'VpsClient', 'VpsInvite')]
    [string]$Action = 'Menu',
    [string]$Manifest = '',
    [int]$BotCount = -1,
    # The spawn plan beside the count (SetBots): -1 leaves .env as it is.
    [int]$SpawnMinutes = -1,
    [int]$LateJoiners = -1,
    [int]$LateHours = -1,
    # SetBots: the operator's own number per kingdom (1 = on, 0 = off, -1 =
    # leave it) and the three numbers, and the second channel with its share.
    [int]$PerKingdom = -1,
    [int]$ShinsooBots = -1,
    [int]$ChunjoBots = -1,
    [int]$JinnoBots = -1,
    [int]$Channel2 = -1,
    [int]$Channel2Share = -1,
    # SetBots: the game channels as one choice, "Kanaly gry 1-4" - 1 is one
    # channel, 2 the second channel for the world's bots, 3 and 4 that and the
    # fresh cohort's CH3, or CH3 and CH4 (the 2.x line only) - and how many
    # fresh bots play there. -1 leaves .env as it is; -Channel2 is the older
    # switch for the second channel alone.
    [int]$GameChannels = -1,
    [int]$FreshCount = -1,
    # SetDifficulty: easy | medium | hard | custom, and the hours custom reads.
    [string]$Difficulty = '',
    [string]$BiologistHours = '',
    [string]$HorseHours = '',
    # And the waits between two skill books, players' and bots' (custom).
    [string]$BookHours = '',
    [string]$BotBookHours = '',
    # And whether the world is played with Auto Lowy and with the companion
    # (Towarzysz): 1 = on, 0 = off, -1 leaves .env as it is.
    [int]$AutoHunt = -1,
    # SetDifficulty: Auto Lowy for everybody (0) or only with the ItemShop's
    # "Auto Lowy (8h)" (1); -1 keeps what .env says.
    [int]$AutoHuntItem = -1,
    [int]$Sidekick = -1,
    # SetDifficulty: the Dom Towarowy (Uxie [DSO]'s flea market at the
    # miscellaneous merchant in M1): 1 = on, 0 = off, -1 keeps what .env says.
    [int]$FleaMarket = -1,
    # The rates a fresh world starts on, asked for when one is about to be
    # made (ResetWorld, and the first start of an install that has no database
    # yet). -1 leaves .env as it is, which is what every other caller wants.
    [int]$RateExp = -1,
    [int]$RateDrop = -1,
    [int]$RateYang = -1,
    # And whether that world comes up with the bots held at the door: 1 = held
    # until the operator lets them in, 0 = they walk in with the world.
    [int]$HoldBots = -1,
    # And whether a player's new character there gets the apprentice chest:
    # 1 = yes, 0 = no, -1 leaves .env as it is.
    [int]$StarterChest = -1,
    [string]$ImportSource = '',
    [string]$RestoreSource = '',
    # COOP (experimental): the friend's name for CoopAddFriend, a friend's
    # login for CoopBlockFriend/CoopInvite, and the code CoopJoin reads.
    [string]$FriendName = '',
    [string]$FriendLogin = '',
    [string]$Invite = '',
    # CoopHost: how the world is offered - auto (the Internet where it can
    # reach this machine, a VPN found here where it cannot), internet, or one
    # VPN by name (vpn = the first one found).
    [ValidateSet('auto', 'internet', 'vpn', 'radmin', 'tailscale', 'zerotier', 'hamachi')]
    [string]$CoopVia = 'auto',
    # CoopHost from the window: it has asked Windows for the firewall rule
    # itself (in front, where the question is seen), so the action does not
    # ask again from a hidden process whose question only blinks on the
    # taskbar.
    [switch]$CoopFirewallAsked,
    # Vps*: the VPS to work on. What is given is saved in .m2vps.json and
    # what is not is taken from there (the window's VPS dialog saves it
    # before it starts an action).
    [string]$VpsHost = '',
    [string]$VpsUser = '',
    [int]$VpsPort = -1,
    [string]$VpsDir = '',
    # ResetWorld only: bring the server up on the fresh world right away, so
    # "wyzeruj swiat i zacznij od nowa" is one click and not a reset followed
    # by GRAJ.
    [switch]$ThenStart,
    [switch]$Yes
)

$ErrorActionPreference = 'Stop'

# The GUI runs this script hidden with its stdout redirected into a file and
# reads that file back as UTF-8. Without this the redirect gets the console's
# OEM code page instead, and every Polish letter this script prints reaches the
# log broken - "Serwer dzia?a w wersji", while the GUI's own lines beside them
# are fine. Both ends speak UTF-8 now.
try {
    [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
    $OutputEncoding = [Text.UTF8Encoding]::new($false)
}
catch { }

$serverRoot = [IO.Path]::GetFullPath($PSScriptRoot)
$modulePath = Join-Path $serverRoot 'launcher\Metin2Launcher.psm1'
$diagnosticsModulePath = Join-Path $serverRoot 'launcher\Metin2Launcher.Diagnostics.psm1'
$configPath = Join-Path $serverRoot '.m2launcher.json'
$statePath = Join-Path $serverRoot '.m2launcher-state.json'
# Written when new files are already on disk but Docker did not finish building
# them. Until it is gone the installation is not really on the version its
# VERSION file claims, and starting it would run the previous images.
$rebuildMarkerPath = Join-Path $serverRoot '.m2launcher-rebuild-pending'

# The launcher's language, .m2launcher.json's "language": English for 'en',
# Polish otherwise. Read here rather than through the module, because the
# first refusal below comes before the module is loaded. It goes into the
# process environment, where the modules' UI-Text reads it and where
# start-server.ps1, run from here, finds it; the window puts the same value
# there for the actions it starts, which is what a missing file leaves.
$launcherLanguage = [string]$env:M2_LAUNCHER_LANGUAGE
try {
    if (Test-Path -LiteralPath $configPath -PathType Leaf) {
        $storedConfig = Get-Content -LiteralPath $configPath -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($storedConfig -and $storedConfig.PSObject.Properties['language']) { $launcherLanguage = [string]$storedConfig.language }
    }
}
catch { }
$env:M2_LAUNCHER_LANGUAGE = $(if ($launcherLanguage -eq 'en') { 'en' } else { 'pl' })

# A text in that language: the English one for 'en', otherwise the Polish
# one, which is word for word what the launcher always said.
function UI-Text {
    param([AllowEmptyString()][string]$Pl, [AllowEmptyString()][string]$En)
    if ($env:M2_LAUNCHER_LANGUAGE -eq 'en' -and $En) { return $En }
    return $Pl
}

foreach ($requiredModule in @($modulePath, $diagnosticsModulePath)) {
    if (-not (Test-Path -LiteralPath $requiredModule -PathType Leaf)) {
        throw (UI-Text "Brakuje modułu launchera: $requiredModule" "A launcher module is missing: $requiredModule")
    }
}
Import-Module $modulePath -Force

# Where the time goes. Every action prints a "[faza]" line with the seconds
# since the action began at each point that can be slow - Docker checks, the
# download, the file swap, the image build, compose up - so a launcher log
# from a player says which of them took the ten minutes instead of "the
# update is slow". The GUI stamps every line with the clock as well; this is
# for the CLI, and for reading a log without doing the subtraction.
$script:phaseWatch = [Diagnostics.Stopwatch]::StartNew()
function Write-Phase {
    param([Parameter(Mandatory = $true)][string]$Name)
    Write-Host ((UI-Text "[faza] {0} (+{1} s od poczatku akcji)" "[phase] {0} (+{1} s since the action began)") -f $Name, [int]$script:phaseWatch.Elapsed.TotalSeconds) -ForegroundColor DarkCyan
}
Import-Module $diagnosticsModulePath -Force
# COOP (experimental, the local branch "coop"): an optional module; without
# it the Coop* actions say so and nothing else changes.
$coopModulePath = Join-Path $serverRoot 'launcher\Metin2Launcher.Coop.psm1'
if (Test-Path -LiteralPath $coopModulePath -PathType Leaf) { Import-Module $coopModulePath -Force }
# The VPS (the 2.x line): optional the same way - without it the Vps* actions
# say so and the menu does not offer them.
$vpsModulePath = Join-Path $serverRoot 'launcher\Metin2Launcher.Vps.psm1'
if (Test-Path -LiteralPath $vpsModulePath -PathType Leaf) { Import-Module $vpsModulePath -Force }

function Write-Header {
    Clear-Host
    Write-Host '========================================================' -ForegroundColor DarkYellow
    Write-Host (UI-Text '  Metin2 Singleplayer - Launcher i aktualizacje' '  Metin2 Singleplayer - launcher and updates') -ForegroundColor Yellow
    Write-Host '========================================================' -ForegroundColor DarkYellow
    Write-Host ''
}

function Get-Config {
    return Get-M2LauncherConfig -ServerRoot $serverRoot -ConfigPath $configPath
}

function Get-ManifestSource {
    param($Config)
    if ($Manifest) { return $Manifest }
    return [string]$Config.manifestUrl
}

function Test-RebuildPending {
    return (Test-Path -LiteralPath $rebuildMarkerPath -PathType Leaf)
}

function Read-RecordedState {
    # What the files on disk are, whatever the images are: the versions the
    # last updates recorded, else VERSION and the client the full package
    # shipped (New-M2DeployTree.ps1 puts CLIENT_VERSION beside VERSION). A
    # recorded "unknown" is no record - older launchers wrote one back (see
    # Read-State) - and a state file that does not parse, which a crash can
    # leave behind, must not stop every action.
    $versionFile = Join-Path $serverRoot 'VERSION'
    $onDisk = if (Test-Path -LiteralPath $versionFile -PathType Leaf) {
        (Get-Content -LiteralPath $versionFile -Raw).Trim()
    }
    else { 'unknown' }
    $clientMarker = Join-Path $serverRoot 'CLIENT_VERSION'
    $shippedClient = if (Test-Path -LiteralPath $clientMarker -PathType Leaf) {
        (Get-Content -LiteralPath $clientMarker -Raw).Trim()
    }
    else { 'unknown' }
    $server = ''
    $client = ''
    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        try {
            $saved = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
            if ($saved -and $saved.PSObject.Properties['server']) { $server = ([string]$saved.server).Trim() }
            if ($saved -and $saved.PSObject.Properties['client']) { $client = ([string]$saved.client).Trim() }
        }
        catch { }
    }
    if (-not $server -or $server -eq 'unknown') { $server = $onDisk }
    if (-not $client -or $client -eq 'unknown') { $client = $shippedClient }
    return [pscustomobject]@{ schema = 1; server = $server; client = $client }
}

function Read-State {
    # An interrupted update leaves the new VERSION file on disk while the running
    # containers are still the old ones. Reporting that version would make the
    # update check answer "already up to date" and never rebuild, which is the
    # state a player cannot get out of on their own. The server alone: nothing
    # of the client is built, and hiding its version as well had two costs -
    # the update offered the client again, and Save-State, which read through
    # here, wrote "unknown" over the version a client update had just recorded
    # (pattsito, 23 September, client 2.0.26 recorded and lost two minutes later).
    $state = Read-RecordedState
    if (Test-RebuildPending) { $state.server = 'unknown' }
    return $state
}

function Save-State {
    param([string]$ServerVersion, [string]$ClientVersion)
    $state = Read-RecordedState
    if ($ServerVersion) { $state.server = $ServerVersion }
    if ($ClientVersion) { $state.client = $ClientVersion }
    $state | Select-Object schema, server, client | ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding UTF8
}

function Get-ManifestComponent {
    param(
        [Parameter(Mandatory = $true)]$RemoteManifest,
        [Parameter(Mandatory = $true)][ValidateSet('server', 'client')][string]$Name
    )
    $property = $RemoteManifest.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value) { return $null }
    $component = $property.Value
    if (-not [string]$component.version -or -not [string]$component.url -or -not [string]$component.sha256) {
        return $null
    }
    return $component
}

function Test-InstalledVersion {
    param(
        [AllowEmptyString()][string]$Installed,
        [AllowEmptyString()][string]$Available
    )
    if (-not $Installed -or -not $Available -or $Installed -eq 'unknown') { return $false }
    return $Installed.Trim().Equals($Available.Trim(), [StringComparison]::OrdinalIgnoreCase)
}

function Confirm-Operation {
    param([Parameter(Mandatory = $true)][string]$Question)
    if ($Yes) { return $true }
    $answer = Read-Host ("$Question " + (UI-Text '[t/N]' '[y/N]'))
    return $answer -match '^(t|tak|y|yes)$'
}

function Show-DockerDiagnostics {
    param([switch]$CheckPanelPort)

    $report = Get-M2DockerPreflight -ServerRoot $serverRoot -CheckPanelPort:$CheckPanelPort
    $text = Format-M2DockerPreflightReport -Report $report
    Write-Host $text -ForegroundColor $(if ($report.CanStart) { 'Green' } else { 'Yellow' })
    return $report
}

function Assert-DockerPrerequisites {
    param([switch]$CheckPanelPort)

    $report = Show-DockerDiagnostics -CheckPanelPort:$CheckPanelPort
    if (-not $report.CanStart) {
        throw (@($report.BlockingIssues) -join [Environment]::NewLine)
    }
}

function Assert-DockerDiskWritable {
    # Before a build, and before an update swaps a single file: a Docker disk
    # gone read-only fails every build at its first write, and after the
    # first failure Docker only says "failed to solve: exit code: 255".
    # pattsito (23 September) downloaded and applied the same update five
    # times in forty minutes against such a disk. An engine that is not
    # running cannot be asked; Rebuild-Server starts it and asks again.
    # -KeepRebuildPending: the files are already the new ones, so a later
    # GRAJ must still finish the build.
    param([switch]$KeepRebuildPending, [string]$Before = (UI-Text 'budowanie serwera' 'the server build'))
    if (-not (Test-M2DockerRunning)) { return }
    $fault = Get-M2DockerDiskFault
    if (-not $fault) { return }
    if ($KeepRebuildPending) {
        Set-Content -LiteralPath $rebuildMarkerPath -Value ([DateTime]::UtcNow.ToString('o')) -Encoding UTF8
    }
    throw ((UI-Text "Przerywam $Before - dysk Dockera nie przyjmuje zapisu:" "Stopping $Before - the Docker disk takes no writes:") + [Environment]::NewLine +
           $fault + [Environment]::NewLine + [Environment]::NewLine + (Get-M2DockerDiskRemedy))
}

function Close-VpsTunnelOnServerPorts {
    # The VPS window's tunnel to the panels is this launcher's own ssh, and one
    # opened while this PC's server was stopped took 127.0.0.1:7788, 7790 and
    # 7791 - an update started meanwhile built its images and could not bind
    # 7790, and every retry stopped at "port 7788 zajmuje proces ssh" (Sudak,
    # 28 September). Since 2.2.36 the tunnel keeps off those ports; one that an
    # older launcher opened on them is closed here, before a start or an update
    # asks about the ports, because the server comes first and the VPS window
    # opens the tunnel again ten thousand ports higher.
    if (-not (Get-Command Get-M2VpsTunnelProcess -ErrorAction SilentlyContinue)) { return }
    try {
        $process = Get-M2VpsTunnelProcess -State (Get-M2VpsState -ServerRoot $serverRoot)
        if (-not $process) { return }
        $held = @(Get-M2ProgramPortConflicts -ServerRoot $serverRoot | Where-Object { [int]$_.Listener.Pid -eq [int]$process.Id })
        if ($held.Count -eq 0) { return }
        if (Close-M2VpsPanel -ServerRoot $serverRoot) {
            Write-Host ((UI-Text 'Zamknięto tunel do paneli VPS - trzymał porty tego serwera ({0}). Panele VPS otworzysz znowu przyciskiem OTWÓRZ PANEL w oknie SERWER NA VPS.' 'Closed the tunnel to the VPS panels - it held this server''s ports ({0}). Open the VPS panels again with OPEN PANEL in the SERVER ON A VPS window.') -f
                ((@($held) | ForEach-Object { [string]$_.Port }) -join ', ')) -ForegroundColor Yellow
        }
    }
    catch { Write-Host (UI-Text "Nie udało się sprawdzić tunelu do paneli VPS: $($_.Exception.Message)" "Could not check the tunnel to the VPS panels: $($_.Exception.Message)") -ForegroundColor Yellow }
}

function Assert-ServerPortsFree {
    # A program of Windows' own on one of the server's ports - a MySQL on 3306
    # (Producent Hip Hopu, 27 September) - is what the start's preflight names,
    # but an update never asked: it downloaded, swapped the files, built for
    # minutes, and only then did compose fail to bind the port. Asked before
    # the download now, and again before a build. Another installation's
    # containers are not this check's: Clear-PortConflicts stops them.
    # -KeepRebuildPending: the files are already the new ones, so a later GRAJ
    # must still finish the build.
    param([switch]$KeepRebuildPending, [string]$Before = (UI-Text 'budowanie serwera' 'the server build'))
    Close-VpsTunnelOnServerPorts
    $conflicts = @(Get-M2ProgramPortConflicts -ServerRoot $serverRoot)
    if ($conflicts.Count -eq 0) { return }
    if ($KeepRebuildPending) {
        Set-Content -LiteralPath $rebuildMarkerPath -Value ([DateTime]::UtcNow.ToString('o')) -Encoding UTF8
    }
    throw ((UI-Text "Przerywam $Before - port serwera zajmuje inny program:" "Stopping $Before - another program holds a server port:") + [Environment]::NewLine +
           ((@($conflicts) | ForEach-Object { [string]$_.Advice }) -join [Environment]::NewLine) +
           [Environment]::NewLine + (UI-Text 'Baza, postacie i ustawienia są w porządku.' 'The database, the characters and the settings are fine.'))
}

function Start-Server {
    # Before the preflight refuses the start: an old installation takes the
    # ports back on every engine start, so a check that only names it leaves the
    # player exactly where they were.
    Clear-PortConflicts -Quiet | Out-Null
    Close-VpsTunnelOnServerPorts
    Assert-DockerPrerequisites -CheckPanelPort
    Write-Phase (UI-Text 'Docker sprawdzony' 'Docker checked')
    # A second-channel wish left in the web panel, before .env is read.
    try { Sync-ChannelWishFromPanel }
    catch { Write-Host (UI-Text "Nie udalo sie odczytac ustawienia kanalow z panelu WWW: $($_.Exception.Message)" "Could not read the channel setting from the web panel: $($_.Exception.Message)") -ForegroundColor Yellow }
    # start-server.ps1 brings the stack up from the images that already exist.
    # After an interrupted update those are the old ones, so finish the build
    # first - otherwise the player keeps running the previous server and the
    # website keeps showing the previous panel.
    if (Test-RebuildPending) {
        Write-Host (UI-Text 'Poprzednia aktualizacja nie dokonczyla budowania. Dokancczam je teraz...' 'The previous update did not finish its build. Finishing it now...') -ForegroundColor Yellow
        Rebuild-Server
        Write-Host (UI-Text 'Budowanie zakonczone.' 'Build finished.') -ForegroundColor Green
    }
    $script = Join-Path $serverRoot 'start-server.ps1'
    if (-not (Test-Path -LiteralPath $script -PathType Leaf)) { throw (UI-Text 'Brakuje start-server.ps1.' 'start-server.ps1 is missing.') }
    & $script
    if ($LASTEXITCODE -ne 0) { throw (UI-Text "Uruchamianie serwera zakończyło się kodem $LASTEXITCODE." "Starting the server ended with code $LASTEXITCODE.") }
    Write-Phase (UI-Text 'Serwer uruchomiony' 'Server started')
    # COOP: a world hosted before this start is still hosted - .env keeps the
    # address - so the router's four-hour lease is renewed here.
    try { Update-CoopHostingLease }
    catch { Write-Host (UI-Text "COOP: nie udalo sie odnowic przekierowan w routerze: $($_.Exception.Message)" "COOP: could not renew the port forwarding in the router: $($_.Exception.Message)") -ForegroundColor Yellow }
}

function Stop-Server {
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'SilentlyContinue'
        docker info 1>$null 2>$null
        $dockerAvailable = $LASTEXITCODE -eq 0
    }
    finally { $ErrorActionPreference = $previousPreference }
    if (-not $dockerAvailable) {
        Write-Host (UI-Text 'Docker jest już zatrzymany.' 'Docker is already stopped.') -ForegroundColor Yellow
        return
    }
    $composeDir = Join-Path $serverRoot 'linux-port\docker'
    $composeFile = Join-Path $composeDir 'docker-compose.yml'
    # `docker compose' writes progress to stderr; under $ErrorActionPreference=
    # 'Stop' Windows PowerShell 5.1 turns that into a terminating error and the
    # stop reports failure even when it worked. Decide from the exit code.
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        docker compose --project-directory $composeDir -f $composeFile stop
        $stopExit = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previousPreference }
    if ($stopExit -ne 0) { throw (UI-Text "Zatrzymywanie serwera zakończyło się kodem $stopExit." "Stopping the server ended with code $stopExit.") }
}

# Another installation of this same server, sitting on the ports this one
# publishes. Every container ships `restart: unless-stopped`, so Docker Desktop
# starts the old project again on every engine start and it binds the ports
# before this installation can - which is why quitting Docker by hand never
# helped ("nawet jak recznie wylacze calkowicie docker"). `docker stop` is what
# holds, because its manual-stop flag survives an engine restart. Volumes are
# never touched: the collision is containers, and a removed volume is the world.
function Clear-PortConflicts {
    param([switch]$Quiet)

    if (-not (Test-M2DockerRunning)) { return 0 }
    $holders = @(Get-M2ForeignPortHolders -ServerRoot $serverRoot)
    if ($holders.Count -eq 0) {
        if (-not $Quiet) {
            Write-Host (UI-Text 'Zadna inna instalacja nie trzyma portow tego serwera.' 'No other installation holds this server''s ports.') -ForegroundColor Green
        }
        return 0
    }
    foreach ($holder in $holders) {
        $where = if ($holder.WorkingDir) { " (folder: $($holder.WorkingDir))" } else { '' }
        Write-Host ((UI-Text "Port {0}: trzyma go kontener {1} z instalacji '{2}'{3}." "Port {0}: held by the container {1} of the installation '{2}'{3}.") -f
            ((@($holder.Ports) | ForEach-Object { "$_" }) -join ', '), $holder.Container, $holder.Project, $where) -ForegroundColor Yellow
    }
    $stopped = @(Stop-M2ForeignPortHolders -ServerRoot $serverRoot)
    foreach ($entry in $stopped) {
        Write-Host ((UI-Text "Zatrzymano instalacje '{0}' ({1} kontenerow). Baza, wolumeny i postep sa nietkniete." "Stopped the installation '{0}' ({1} containers). The database, the volumes and the progress are untouched.") -f
            $entry.Project, $entry.Containers) -ForegroundColor Green
    }
    return $stopped.Count
}

function Clear-PortConflictsAction {
    $freed = Clear-PortConflicts
    if ($freed -gt 0) {
        Write-Host (UI-Text 'Porty zwolnione. Mozesz kliknac GRAJ albo ponowic aktualizacje.' 'The ports are free. You can click PLAY or try the update again.') -ForegroundColor Green
    }
}

function Start-Docker {
    Assert-DockerPrerequisites
    $script = Join-Path $serverRoot 'start-server.ps1'
    if (-not (Test-Path -LiteralPath $script -PathType Leaf)) { throw (UI-Text 'Brakuje start-server.ps1.' 'start-server.ps1 is missing.') }
    & $script -DockerOnly
    if ($LASTEXITCODE -ne 0) { throw (UI-Text "Uruchamianie Docker Desktop zakończyło się kodem $LASTEXITCODE." "Starting Docker Desktop ended with code $LASTEXITCODE.") }
}

function Stop-DockerAndServer {
    Stop-Server
    $dockerCli = Join-Path $env:ProgramFiles 'Docker\Docker\DockerCli.exe'
    if (Test-Path -LiteralPath $dockerCli -PathType Leaf) {
        $previousPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'SilentlyContinue'
            & $dockerCli -Shutdown 1>$null 2>$null
        }
        finally { $ErrorActionPreference = $previousPreference }
    }
    else {
        Get-Process -Name 'Docker Desktop', 'com.docker.backend' -ErrorAction SilentlyContinue |
            Stop-Process -ErrorAction SilentlyContinue
    }
    Write-Host (UI-Text 'Serwer i Docker Desktop zatrzymane. Dane pozostają zapisane w wolumenach.' 'The server and Docker Desktop are stopped. The data stays saved in the volumes.') -ForegroundColor Green
}

function Rebuild-Server {
    $composeDir = Join-Path $serverRoot 'linux-port\docker'
    $composeFile = Join-Path $composeDir 'docker-compose.yml'
    # Stopping the server also stops Docker Desktop (see Stop-DockerAndServer),
    # so the sensible order - stop the server, then update it - always arrived
    # here with a dead engine and failed on a raw npipe error, after the files
    # had already been swapped. Start-Server has the same hole: it finishes a
    # pending build before start-server.ps1 gets a chance to bring the engine
    # up, so "click GRAJ" only ever worked when Docker happened to be running.
    # Both paths go through here, so the engine is ensured here as well.
    if (-not (Test-M2DockerRunning)) {
        Write-Host (UI-Text 'Silnik Dockera jest zatrzymany - uruchamiam go przed budowaniem.' 'The Docker engine is stopped - starting it before the build.') -ForegroundColor Yellow
        Start-Docker
    }
    Assert-DockerDiskWritable -KeepRebuildPending
    # Compose needs the .env before it can build anything - the database
    # passwords are required variables. A copy unpacked by hand has no .env
    # until start-server.ps1 writes one, and that used to run only after this
    # build, so the update failed and "click GRAJ" failed the same way.
    $identityScript = Join-Path $serverRoot 'start-server.ps1'
    if (Test-Path -LiteralPath $identityScript -PathType Leaf) {
        & $identityScript -IdentityOnly
        if ($LASTEXITCODE -ne 0) { throw (UI-Text "Przygotowanie pliku .env zakonczylo sie kodem $LASTEXITCODE." "Preparing the .env file ended with code $LASTEXITCODE.") }
    }
    # The overlay is the source of truth; the build context is only a copy of
    # it. Refresh the copy before Docker reads it, or an update that added a
    # source file compiles against the previous one - or, as in 1.23.2, against
    # a header that is not there at all.
    $synced = Sync-M2PlayerbotOverlay -ServerRoot $serverRoot
    if ($synced -gt 0) {
        Write-Host (UI-Text "Zsynchronizowano $synced plik(ow) zrodlowych bota do kontekstu budowania." "Copied $synced bot source file(s) into the build context.") -ForegroundColor DarkGray
    }
    Write-Phase (UI-Text 'Kontekst budowania przygotowany (.env, nakladka)' 'Build context ready (.env, overlay)')
    # The engine patches are part of the overlay too, and until now nothing on a
    # player's machine ever applied them.
    $patched = Invoke-M2EnginePatches -ServerRoot $serverRoot
    if ($patched -gt 0) {
        Write-Host (UI-Text "Nalozono $patched latek silnika." "Applied $patched engine patch(es).") -ForegroundColor DarkGray
    }
    # And the sources the image is actually built from.
    #
    # This check exists in start-server.ps1 too, and that was not enough: this
    # path calls start-server.ps1 with -IdentityOnly, which returns after
    # writing the .env and never reaches it, then builds here. So a player
    # clicking GRAJ went straight to `docker compose --build' with an
    # incomplete context and got fifteen "failed to calculate checksum ... not
    # found" lines. Reported from the Discord twice, the second time against a
    # version that was supposed to have fixed it - because the fix was in the
    # half of the code that click does not run.
    #
    # linux-port/docker/game/src holds the r40250 tree, put there once by
    # fetch-sources.sh during installation. It is the operator's own package and
    # never travels in an update; what an update does put there is
    # src/server/game, because that is where the bot sources belong - which is
    # why a broken install still shows a plausible src/server/game and a build
    # context of about 1.6 MB where a complete one is hundreds of megabytes.
    $gameContext = Join-Path $serverRoot 'linux-port\docker\game\src'
    [void](Restore-M2EmptyGameContextDirs -ServerRoot $serverRoot)
    $requiredContext = @(Get-M2RequiredGameContext -ServerRoot $serverRoot)
    $missingContext = @()
    foreach ($entry in $requiredContext) {
        if (-not (Test-Path -LiteralPath (Join-Path $gameContext $entry))) {
            $missingContext += $entry
        }
    }
    # The dumps, the same way (see start-server.ps1 for why an initialised
    # database is exempt): this is the half of the code that click runs.
    $missingDumps = @(Get-M2MissingSqlDumps -ServerRoot $serverRoot)
    if ($missingDumps.Count -gt 0) {
        $dbVolume = Get-CurrentInstallTargetVolume
        $dbReady = $false
        if ($dbVolume) { $dbReady = Test-M2VolumeInitialized -Volume $dbVolume }
        if (-not $dbReady) {
            throw ((UI-Text "Brakuje zrzutow bazy danych, wiec pierwsza baza powstalaby pusta.`n`n" "The database dumps are missing, so the first database would come up empty.`n`n") +
                   (UI-Text "Katalog: " "Folder: ") + (Join-Path $serverRoot 'linux-port\docker\mariadb\initdb.d\dumps') + "`n" +
                   (UI-Text "Brakuje: " "Missing: ") + ($missingDumps -join ', ') + "`n`n" +
                   (UI-Text "MariaDB wystartowalaby bez schematu gry (i zglosila 'healthy'), a playerbot-migrate " "MariaDB would start without the game's schema (and report 'healthy'), and playerbot-migrate ") +
                   (UI-Text "czekalby 30 minut na tabele, ktore nigdy nie powstana. Zrzuty pochodza z Twojej " "would wait 30 minutes for tables that never appear. The dumps come from your own ") +
                   (UI-Text "paczki serwera r40250 (Server\metin2_mysql_dump.zip) i wystawia je wylacznie " "r40250 server package (Server\metin2_mysql_dump.zip), and only the installer ") +
                   (UI-Text "instalator - zadna aktualizacja ich nie przywroci.`n`n" "puts them there - no update brings them back.`n`n") +
                   (UI-Text "Uruchom ponownie instalator (installer\install.ps1) ze wskazana paczka " "Run the installer again (installer\install.ps1) with the package named in ") +
                   (UI-Text "(`$env:M2_SRC_ARCHIVE), albo rozpakuj metin2_mysql_dump.zip do tego katalogu " "(`$env:M2_SRC_ARCHIVE), or unpack metin2_mysql_dump.zip into this folder ") +
                   (UI-Text "i kliknij GRAJ jeszcze raz." "and click PLAY again."))
        }
    }
    if ($missingContext.Count -gt 0) {
        throw ((UI-Text "Brakuje zrodel gry, wiec nie ma z czego zbudowac serwera.`n`n" "The game sources are missing, so there is nothing to build the server from.`n`n") +
               (UI-Text "Katalog: " "Folder: ") + $gameContext + "`n" +
               (UI-Text "Brakuje: " "Missing: ") + ($missingContext -join ', ') + "`n`n" +
               (UI-Text "To nie jest blad Dockera, WSL ani tej aktualizacji. Te pliki pochodza " "This is no fault of Docker, WSL or this update. These files come ") +
               (UI-Text "z Twojej wlasnej paczki serwera r40250 i sa rozpakowywane raz, podczas " "from your own r40250 server package and are unpacked once, during ") +
               (UI-Text "instalacji - zadna aktualizacja ich nie przywroci, bo nie wolno nam ich " "the installation - no update brings them back, because we may not ") +
               (UI-Text "rozpowszechniac.`n`n" "distribute them.`n`n") +
               (UI-Text "Uruchom ponownie instalator (installer\install.ps1). Pobierze zrodla i " "Run the installer again (installer\install.ps1). It fetches the sources and ") +
               (UI-Text "odtworzy kontekst budowania. Baza, postacie i ustawienia zostaja nietkniete." "rebuilds the build context. The database, the characters and the settings stay untouched."))
    }

    # The update is where a port collision hurts most: the images build for
    # minutes and compose then cannot bind a port another installation took back
    # while they were building ("Bind for 127.0.0.1:7790 failed"), so the whole
    # update is lost at its last step and the player is told to free a port they
    # cannot find.
    Clear-PortConflicts -Quiet | Out-Null
    # And a port a program of Windows' own holds, which nothing here can stop:
    # said before the minutes of building, not after them.
    Assert-ServerPortsFree -KeepRebuildPending

    # See Stop-Server: compose progress on stderr must not be treated as failure
    # under $ErrorActionPreference='Stop' in Windows PowerShell 5.1.
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        # `up --build` on a fresh engine has raced its own pull: the images
        # were built, then "No such image: mariadb:10.11" while creating the
        # database container, and the update was reported as failed although
        # the second click succeeded. Pull what is not built first; a failure
        # here is not final, `up` tries again.
        docker compose --project-directory $composeDir -f $composeFile pull --ignore-buildable 2>&1 | Out-Null
        Write-Phase (UI-Text 'Obrazy bazowe pobrane, zaczynam docker compose up --build' 'Base images pulled, starting docker compose up --build')
        Set-M2PlayerbotsVersionEnvironment -ServerRoot $serverRoot
        docker compose --project-directory $composeDir -f $composeFile up -d --build
        $buildExit = $LASTEXITCODE
        Write-Phase (UI-Text "docker compose up --build zakonczone (kod $buildExit)" "docker compose up --build finished (code $buildExit)")
    }
    finally { $ErrorActionPreference = $previousPreference }
    if ($buildExit -ne 0) {
        Set-Content -LiteralPath $rebuildMarkerPath -Value ([DateTime]::UtcNow.ToString('o')) -Encoding UTF8
        throw (UI-Text 'Nowa wersja plików została zapisana, ale Docker nie zbudował serwera. Kliknij GRAJ — launcher dokończy budowanie. Kopia plików jest w katalogu backups.' 'The new version''s files are saved, but Docker did not build the server. Click PLAY - the launcher finishes the build. A copy of the old files is in the backups folder.')
    }
    if (Test-RebuildPending) { Remove-Item -LiteralPath $rebuildMarkerPath -Force -ErrorAction SilentlyContinue }
}

function Show-UpdateStatus {
    param($RemoteManifest)
    $state = Read-State
    $serverComponent = Get-ManifestComponent -RemoteManifest $RemoteManifest -Name 'server'
    $clientComponent = Get-ManifestComponent -RemoteManifest $RemoteManifest -Name 'client'
    $messageProperty = $RemoteManifest.PSObject.Properties['statusMessage']
    if ($null -ne $messageProperty -and [string]$messageProperty.Value) {
        Write-Host ([string]$messageProperty.Value) -ForegroundColor Yellow
    }
    Write-Host (UI-Text "Zainstalowany serwer: $($state.server)" "Installed server: $($state.server)") -ForegroundColor Gray
    Write-Host (UI-Text "Dostępny serwer:     $(if ($serverComponent) { $serverComponent.version } else { 'brak w tym kanale' })" "Available server: $(if ($serverComponent) { $serverComponent.version } else { 'none in this channel' })") -ForegroundColor Cyan
    Write-Host (UI-Text "Zainstalowany klient: $($state.client)" "Installed client: $($state.client)") -ForegroundColor Gray
    Write-Host (UI-Text "Dostępny klient:      $(if ($clientComponent) { $clientComponent.version } else { 'brak w tym kanale' })" "Available client: $(if ($clientComponent) { $clientComponent.version } else { 'none in this channel' })") -ForegroundColor Cyan
}

function Update-Server {
    param($RemoteManifest)
    $component = Get-ManifestComponent -RemoteManifest $RemoteManifest -Name 'server'
    if (-not $component) {
        Write-Host (UI-Text 'Manifest nie zawiera aktualizacji serwera. Pomijam.' 'The manifest has no server update. Skipping it.') -ForegroundColor Yellow
        return
    }
    $state = Read-State
    if (Test-InstalledVersion -Installed ([string]$state.server) -Available ([string]$component.version)) {
        Write-Host (UI-Text "Serwer jest już aktualny (wersja $($component.version))." "The server is up to date (version $($component.version)).") -ForegroundColor Green
        return
    }
    # A build that failed after the files were swapped leaves them at the new
    # version with only the images missing, and applying the same package
    # again changes nothing but the backups folder: pattsito's five attempts
    # were five downloads of 47.8 MB and five copies of 7042 files, on the
    # drive whose room was the likeliest cause of the failure. Finish the build.
    if ((Test-RebuildPending) -and (Test-InstalledVersion -Installed ([string](Read-RecordedState).server) -Available ([string]$component.version))) {
        Write-Host (UI-Text "Pliki serwera w wersji $($component.version) są już na dysku - dokańczam budowanie bez ponownego pobierania." "The server files of version $($component.version) are already on the disk - finishing the build without downloading them again.") -ForegroundColor Yellow
        Rebuild-Server
        Write-Host (UI-Text "Serwer działa w wersji $($component.version)." "The server runs version $($component.version).") -ForegroundColor Green
        return
    }
    Assert-DockerDiskWritable -Before (UI-Text 'aktualizację (niczego nie pobrano ani nie podmieniono)' 'the update (nothing was downloaded or replaced)')
    Assert-ServerPortsFree -Before (UI-Text 'aktualizację (niczego nie pobrano ani nie podmieniono)' 'the update (nothing was downloaded or replaced)')
    if (-not (Confirm-Operation (UI-Text 'Zaktualizować pliki serwera i przebudować kontenery? Baza postaci pozostanie bez zmian.' 'Update the server files and rebuild the containers? The character database stays as it is.'))) {
        Write-Host (UI-Text 'Anulowano.' 'Cancelled.') -ForegroundColor Yellow
        return
    }
    $result = Invoke-M2PackageUpdate -Component $component -TargetRoot $serverRoot -BackupRoot (Join-Path $serverRoot 'backups')
    Write-Host (UI-Text "Podmieniono $($result.Files) plików. Kopia: $($result.Backup)" "Replaced $($result.Files) files. Backup: $($result.Backup)") -ForegroundColor Green
    Write-Phase (UI-Text 'Pliki aktualizacji pobrane i podmienione' 'Update files downloaded and replaced')
    # From here the files on disk are the new version whatever happens to the
    # build, and VERSION on disk already says so. Recording it only after a
    # successful rebuild meant a deferred build left the launcher reporting the
    # previous version for ever - it kept offering the same update and kept
    # re-downloading and re-applying it, one backup directory per attempt. What
    # tracks the build is the rebuild marker, not the version number.
    Save-State -ServerVersion $result.Version -ClientVersion ''
    Rebuild-Server
    Write-Host (UI-Text "Serwer działa w wersji $($result.Version)." "The server runs version $($result.Version).") -ForegroundColor Green
}

function Assert-ClientNotRunning {
    # The client's exe cannot be replaced while the game runs, and Windows
    # says so only when the file is copied - after the whole download.
    # Ratorex (18 September) tried five times in a quarter of an hour, each
    # time 65 MB and the same "used by another process". Asked first now,
    # and before the server as well, so an "update everything" does not
    # leave a new server beside a client that cannot log in to it.
    param($Config)
    $clientRoot = [string]$Config.clientRoot
    if (-not $clientRoot -or -not (Test-Path -LiteralPath $clientRoot -PathType Container)) { return }
    $running = @(Get-M2FolderProcesses -Root $clientRoot)
    if ($running.Count -gt 0) {
        throw ((UI-Text "Klient gry jest uruchomiony ({0}). Zamknij gre - sprawdz tez Menedzer zadan, czy metin2client.exe nie zostal w tle - i kliknij ZAINSTALUJ AKTUALIZACJE jeszcze raz." "The game client is running ({0}). Close the game - check Task Manager too, in case metin2client.exe stayed in the background - and click CHECK FOR UPDATES again.") -f ($running -join ', '))
    }
}

function Update-Client {
    param($RemoteManifest, $Config)
    $component = Get-ManifestComponent -RemoteManifest $RemoteManifest -Name 'client'
    if (-not $component) {
        Write-Host (UI-Text 'Manifest nie zawiera aktualizacji klienta. Pomijam.' 'The manifest has no client update. Skipping it.') -ForegroundColor Yellow
        return
    }
    $state = Read-State
    if (Test-InstalledVersion -Installed ([string]$state.client) -Available ([string]$component.version)) {
        Write-Host (UI-Text "Klient jest już aktualny (wersja $($component.version))." "The client is up to date (version $($component.version)).") -ForegroundColor Green
        Repair-ClientExe -RemoteManifest $RemoteManifest -Config $Config
        return
    }
    $clientRoot = [string]$Config.clientRoot
    if (-not $clientRoot) {
        throw (UI-Text 'Nie ustawiono folderu klienta. Uruchom launcher z akcją Configure.' 'No client folder is set. Run the launcher with the Configure action.')
    }
    if (-not (Test-Path -LiteralPath $clientRoot -PathType Container)) {
        throw (UI-Text "Nie znaleziono folderu klienta: $clientRoot" "Client folder not found: $clientRoot")
    }
    Assert-ClientNotRunning -Config $Config
    if (-not (Confirm-Operation (UI-Text "Zaktualizować klienta w $clientRoot?" "Update the client in $clientRoot?"))) {
        Write-Host (UI-Text 'Anulowano.' 'Cancelled.') -ForegroundColor Yellow
        return
    }
    $result = Invoke-M2PackageUpdate -Component $component -TargetRoot $clientRoot -BackupRoot (Join-Path $serverRoot 'backups\client')
    Save-State -ServerVersion '' -ClientVersion $result.Version
    Write-Host (UI-Text "Klient został zaktualizowany. Plików: $($result.Files), kopia: $($result.Backup)" "The client is updated. Files: $($result.Files), backup: $($result.Backup)") -ForegroundColor Green
    Repair-ClientExe -RemoteManifest $RemoteManifest -Config $Config
}

function Repair-ClientExe {
    # The client folder's executables after a client update, and whenever the
    # GUI finds an old metin2client.exe (Repair-M2ClientExecutables): the exe
    # of an old full package replaced by the manifest's "clientExe", the two
    # strays those packages carried deleted, and a launcher that started one of
    # them pointed back at metin2client.exe. A refusal is said, never thrown:
    # the client package has been applied by then and stays applied.
    param($RemoteManifest, $Config)
    $clientRoot = [string]$Config.clientRoot
    if (-not $clientRoot -and [string]$Config.clientExecutable) {
        $clientRoot = Split-Path -Parent ([string]$Config.clientExecutable)
    }
    if (-not $clientRoot -or -not (Test-Path -LiteralPath $clientRoot -PathType Container)) { return }
    $component = Get-M2ClientExeComponent -Manifest $RemoteManifest
    $notes = @(Repair-M2ClientExecutables -ClientFolder $clientRoot -ExeComponent $component `
        -BackupRoot (Join-Path $serverRoot 'backups\client') -ServerRoot $serverRoot -ConfigPath $configPath)
    foreach ($note in $notes) { Write-Host $note -ForegroundColor Yellow }
    if (Test-M2ClientExeOld -ClientFolder $clientRoot) {
        Write-Host (UI-Text 'metin2client.exe w folderze klienta jest nadal stary (sprzed czterech stron ekwipunku) - gra nie wpuści go do logowania, dopóki nie zostanie podmieniony.' 'metin2client.exe in the client folder is still the old one (from before the four inventory pages) - the game will not let it log in until it is replaced.') -ForegroundColor Yellow
    }
}

function Configure-Launcher {
    $config = Get-Config
    Write-Host (UI-Text 'Pozostaw puste pole, aby zachować dotychczasową wartość.' 'Leave a field empty to keep its current value.') -ForegroundColor Gray
    $manifestValue = Read-Host (UI-Text "Manifest aktualizacji [$($config.manifestUrl)]" "Update manifest [$($config.manifestUrl)]")
    if ($manifestValue) { $config.manifestUrl = $manifestValue }
    $clientValue = Read-Host (UI-Text "Folder klienta [$($config.clientRoot)]" "Client folder [$($config.clientRoot)]")
    if ($clientValue) { $config.clientRoot = [IO.Path]::GetFullPath($clientValue) }
    $clientExeValue = Read-Host (UI-Text "Plik EXE klienta [$($config.clientExecutable)]" "Client EXE file [$($config.clientExecutable)]")
    if ($clientExeValue -eq '-') { $config.clientExecutable = '' }
    elseif ($clientExeValue) { $config.clientExecutable = [IO.Path]::GetFullPath($clientExeValue) }
    $supportState = if ($config.supportUploadUrl) { (UI-Text 'ustawiony' 'set') } else { (UI-Text 'nieustawiony' 'not set') }
    $supportValue = Read-Host (UI-Text "Prywatny webhook Discord lub adres HTTPS pomocy [$supportState] (wpisz - aby usunąć)" "Private Discord webhook or HTTPS support address [$supportState] (type - to remove it)")
    if ($supportValue -eq '-') { $config.supportUploadUrl = '' }
    elseif ($supportValue) { $config.supportUploadUrl = $supportValue }
    # The language the window's switch sets, asked here in both languages
    # because whoever reads the menu may read only one of them. From the next
    # line on the launcher speaks it.
    $languageValue = Read-Host "Język / Language [$($config.language)] (pl / en)"
    if ("$languageValue".Trim() -match '^(pl|en)$') { $config.language = $Matches[1] }
    Save-M2LauncherConfig -Config $config -ConfigPath $configPath
    $env:M2_LAUNCHER_LANGUAGE = $(if ([string]$config.language -eq 'en') { 'en' } else { 'pl' })
    Write-Host (UI-Text "Zapisano konfigurację: $configPath" "Settings saved: $configPath") -ForegroundColor Green
}

function Get-PlayerbotEnvPath {
    return Join-Path $serverRoot 'linux-port\docker\.env'
}

function Get-PlayerbotCount {
    $envPath = Get-PlayerbotEnvPath
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) { return 350 }
    $match = [Regex]::Match([IO.File]::ReadAllText($envPath), '(?m)^PLAYERBOT_AUTOSPAWN_COUNT=(\d+)\s*$')
    if ($match.Success) { return [int]$match.Groups[1].Value }
    return 350
}

function Set-PlayerbotCount {
    # Writes PLAYERBOT_AUTOSPAWN_COUNT to .env. The core reads it once at startup
    # and spawns at most this many of the bots it will accept, which is a
    # different and usually smaller number: only characters the canonical seed
    # created are in the registry. A world carrying bots from an older bootstrap
    # keeps them, but they never spawn, so asking for more than the registry
    # holds simply gets the registry. The core says both numbers at startup:
    #   PLAYERBOT_AUTH: loaded <n> registered bot identities
    #   PLAYERBOT: autospawn requested=<x> registered_started=<n>
    #
    # The ceiling is the core's own (2500, input_db.cpp); the seed's canonical
    # cohort holds 1500 a kingdom since 2.2.1, so the number is split equally
    # and never runs short of identities. This clamp is the one that decides -
    # the slider in the GUI only proposes a number, and raising that alone
    # would have written 1500 into .env while showing the player 2500.
    param([Parameter(Mandatory = $true)][int]$Count)
    if ($Count -lt 0) { $Count = 0 }
    if ($Count -gt 2500) { $Count = 2500 }
    $envPath = Get-PlayerbotEnvPath
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) {
        throw (UI-Text "Brak pliku .env: $envPath. Uruchom najpierw serwer (GRAJ), aby go utworzyć." "There is no .env file: $envPath. Start the server (PLAY) first to create it.")
    }
    $content = [IO.File]::ReadAllText($envPath)
    $pattern = '(?m)^PLAYERBOT_AUTOSPAWN_COUNT=.*$'
    if ([Regex]::IsMatch($content, $pattern)) {
        $content = [Regex]::Replace($content, $pattern, "PLAYERBOT_AUTOSPAWN_COUNT=$Count")
    }
    else {
        if ($content -and -not $content.EndsWith("`n")) { $content += [Environment]::NewLine }
        $content += "PLAYERBOT_AUTOSPAWN_COUNT=$Count" + [Environment]::NewLine
    }
    [IO.File]::WriteAllText($envPath, $content, [Text.UTF8Encoding]::new($false))
    return $Count
}

function Get-SpawnPlanFromEnv {
    # PLAYERBOT_SPAWN_WINDOW_MINUTES / PLAYERBOT_LATE_JOINERS / PLAYERBOT_LATE_JOIN_HOURS
    # as .env has them; 1 / 0 / 24 when the keys are not there yet.
    return @{
        Minutes = Get-DotEnvValue -Key 'PLAYERBOT_SPAWN_WINDOW_MINUTES' -Default '1'
        Late    = Get-DotEnvValue -Key 'PLAYERBOT_LATE_JOINERS' -Default '0'
        Hours   = Get-DotEnvValue -Key 'PLAYERBOT_LATE_JOIN_HOURS' -Default '24'
    }
}

function Set-SpawnPlan {
    # The core reads the three at startup (input_db.cpp): the window the
    # cohort arrives over, the second cohort and its hours. Clamped to what
    # the core accepts, so .env never carries a number it would refuse.
    param([int]$Minutes, [int]$Late, [int]$Hours)
    if ($Minutes -lt 1) { $Minutes = 1 }
    if ($Minutes -gt 180) { $Minutes = 180 }
    if ($Late -lt 0) { $Late = 0 }
    if ($Late -gt 2500) { $Late = 2500 }
    if ($Hours -lt 1) { $Hours = 1 }
    if ($Hours -gt 168) { $Hours = 168 }
    Set-DotEnvValue -Key 'PLAYERBOT_SPAWN_WINDOW_MINUTES' -Value "$Minutes"
    Set-DotEnvValue -Key 'PLAYERBOT_LATE_JOINERS' -Value "$Late"
    Set-DotEnvValue -Key 'PLAYERBOT_LATE_JOIN_HOURS' -Value "$Hours"
    return @{ Minutes = $Minutes; Late = $Late; Hours = $Hours }
}

function Get-KingdomCountsFromEnv {
    # PLAYERBOT_AUTOSPAWN_PER_KINGDOM and the three numbers.
    #
    # A kingdom whose key is not in .env yet defaults to the equal share of
    # PLAYERBOT_AUTOSPAWN_COUNT, which is what the world runs on right now -
    # never to zero. Zero is a real setting that means "this kingdom starts
    # nobody", and offering it as the opening value of a dialog is how a world
    # ends up with bots in one kingdom: the three keys are absent on every
    # install made before 2.0.83, so the box showed 0 for all three, and
    # ticking "Indywidualne wartosci" with one of them filled left the other
    # two empty for good ("nowe postacie tworza sie tylko w Chunjo",
    # NerrVoVy, 19 September - his world had just turned the second channel on
    # and the two were read together).
    $total = 0
    [int]::TryParse((Get-DotEnvValue -Key 'PLAYERBOT_AUTOSPAWN_COUNT' -Default '0'), [ref]$total) | Out-Null
    $even = [int][Math]::Floor($total / 3)
    $enabled = (Get-DotEnvValue -Key 'PLAYERBOT_AUTOSPAWN_PER_KINGDOM' -Default '0') -eq '1'
    $read = {
        param($key)
        # Only numbers in use are offered back: .env.example ships the three at
        # 0 and start-server.ps1 adds them to every .env, so "not there yet" was
        # never true and the dialog opened on zeros after all.
        if (-not $enabled) { return $even }
        $raw = Get-DotEnvValue -Key $key -Default ''
        if ([string]::IsNullOrWhiteSpace([string]$raw)) { return $even }
        $n = 0
        if ([int]::TryParse($raw, [ref]$n)) { return $n }
        return $even
    }
    return @{
        Enabled = $enabled
        Shinsoo = & $read 'PLAYERBOT_AUTOSPAWN_SHINSOO'
        Chunjo  = & $read 'PLAYERBOT_AUTOSPAWN_CHUNJO'
        Jinno   = & $read 'PLAYERBOT_AUTOSPAWN_JINNO'
    }
}

function Set-KingdomCounts {
    # The operator's own number per kingdom (Greess): with it on, each kingdom
    # starts its own count instead of a share of PLAYERBOT_AUTOSPAWN_COUNT, cut
    # by the core to the identities the kingdom has - 1500 since 2.2.1, so a
    # number is clamped there rather than written and quietly cut (kavvaski's
    # 729 Shinsoo came out as the 500 the kingdom held). Read at the next start.
    param([bool]$Enabled, [int]$Shinsoo = 0, [int]$Chunjo = 0, [int]$Jinno = 0)
    $clamp = { param($n) if ($n -lt 0) { 0 } elseif ($n -gt 1500) { 1500 } else { $n } }
    $Shinsoo = & $clamp $Shinsoo
    $Chunjo = & $clamp $Chunjo
    $Jinno = & $clamp $Jinno
    Set-DotEnvValue -Key 'PLAYERBOT_AUTOSPAWN_PER_KINGDOM' -Value $(if ($Enabled) { '1' } else { '0' })
    Set-DotEnvValue -Key 'PLAYERBOT_AUTOSPAWN_SHINSOO' -Value "$Shinsoo"
    Set-DotEnvValue -Key 'PLAYERBOT_AUTOSPAWN_CHUNJO' -Value "$Chunjo"
    Set-DotEnvValue -Key 'PLAYERBOT_AUTOSPAWN_JINNO' -Value "$Jinno"
    return @{ Enabled = $Enabled; Shinsoo = $Shinsoo; Chunjo = $Chunjo; Jinno = $Jinno }
}

function Get-GameChannelsFromEnv {
    # "Kanaly gry 1-4" as .env has it: the second channel (M2_PLAYERBOT_CH2)
    # with its share, and the fresh cohort's channels 3 and 4
    # (M2_PLAYERBOT_FRESH_CHANNELS) with how many of it play. Channels is the
    # one number the choice shows: 1, 2, or 2 and the fresh channels.
    $share = 40
    [int]::TryParse((Get-DotEnvValue -Key 'PLAYERBOT_CH2_SHARE' -Default '40'), [ref]$share) | Out-Null
    $fresh = 0
    [int]::TryParse((Get-DotEnvValue -Key 'M2_PLAYERBOT_FRESH_CHANNELS' -Default '0'), [ref]$fresh) | Out-Null
    if ($fresh -lt 0 -or $fresh -gt 2) { $fresh = 0 }
    $freshCount = 200
    [int]::TryParse((Get-DotEnvValue -Key 'PLAYERBOT_FRESH_COUNT' -Default '200'), [ref]$freshCount) | Out-Null
    $enabled = (Get-DotEnvValue -Key 'M2_PLAYERBOT_CH2' -Default '0') -eq '1'
    $channels = if ($fresh -gt 0) { 2 + $fresh } elseif ($enabled) { 2 } else { 1 }
    return @{ Channels = $channels; Enabled = $enabled; Share = $share; Fresh = $fresh; FreshCount = $freshCount }
}

function Set-GameChannels {
    # One choice for the channels a world runs (the operator's "Kanaly gry
    # 1-4", 28 September): 1 is the first channel alone; 2 the second channel
    # for the world's bots and players (M2_PLAYERBOT_CH2, with the share of the
    # bots on it); 3 and 4 that and the fresh cohort - bots of their own from
    # level one - on CH3, or on CH3 and CH4 (M2_PLAYERBOT_FRESH_CHANNELS 1 or 2,
    # PLAYERBOT_FRESH_COUNT of them at once). Only the 2.x line has the fresh
    # cohort; r40250 stops at two. And the two port ranges compose publishes:
    # channel N listens on 13000+10*(N-1)..+2, so 13000-13002 for one channel
    # up to 13000-13032 for four - never fewer than an M2_CHANNELS the operator
    # set by hand runs, which the old two-way answer took back to one channel's.
    # The host side keeps the first port a player may have moved. SetAt is when
    # the choice was made: the game container compares it with the web panel's
    # wish, and the newer of the two wins.
    param([int]$Channels, [int]$Share = 40, [int]$FreshCount = 200, [long]$SetAt = 0)
    if ($Channels -lt 1) { $Channels = 1 }
    if ($Channels -gt 4) { $Channels = 4 }
    if ($Channels -gt 2 -and (Get-M2ServerEngine -ServerRoot $serverRoot) -eq 'r40250') { $Channels = 2 }
    if ($Share -lt 10) { $Share = 10 }
    if ($Share -gt 90) { $Share = 90 }
    if ($FreshCount -lt 0) { $FreshCount = 0 }
    if ($FreshCount -gt 1500) { $FreshCount = 1500 }
    if ($SetAt -le 0) { $SetAt = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() }
    $fresh = if ($Channels -ge 3) { $Channels - 2 } else { 0 }
    $running = $Channels
    $configured = 1
    if ([int]::TryParse((Get-DotEnvValue -Key 'M2_CHANNELS' -Default '1'), [ref]$configured) -and
        $configured -gt $running -and $configured -le 4) { $running = $configured }
    $span = 10 * ($running - 1) + 2
    $first = 13000
    $range = Get-DotEnvValue -Key 'M2_GAME_PORT_RANGE' -Default '13000-13002'
    if ($range -match '^\s*(\d+)') { $first = [int]$Matches[1] }
    Set-DotEnvValue -Key 'M2_PLAYERBOT_CH2' -Value $(if ($Channels -ge 2) { '1' } else { '0' })
    Set-DotEnvValue -Key 'PLAYERBOT_CH2_SHARE' -Value "$Share"
    Set-DotEnvValue -Key 'M2_PLAYERBOT_FRESH_CHANNELS' -Value "$fresh"
    Set-DotEnvValue -Key 'PLAYERBOT_FRESH_COUNT' -Value "$FreshCount"
    Set-DotEnvValue -Key 'M2_PLAYERBOT_CH2_SET_AT' -Value "$SetAt"
    Set-DotEnvValue -Key 'M2_GAME_PORT_RANGE' -Value ('{0}-{1}' -f $first, ($first + $span))
    Set-DotEnvValue -Key 'M2_GAME_CONTAINER_PORT_RANGE' -Value ('13000-{0}' -f (13000 + $span))
    return @{ Channels = $Channels; Enabled = ($Channels -ge 2); Share = $Share; Fresh = $fresh; FreshCount = $FreshCount }
}

function Format-GameChannelsChoice {
    # The choice in words, for the confirmation lines.
    param([Parameter(Mandatory = $true)][hashtable]$Choice)
    switch ([int]$Choice.Channels) {
        1 { return (UI-Text 'jeden kanał (CH1)' 'one channel (CH1)') }
        2 { return (UI-Text "dwa kanały, $($Choice.Share)% botów na CH2" "two channels, $($Choice.Share)% of the bots on CH2") }
        3 { return (UI-Text "trzy kanały, $($Choice.Share)% botów na CH2, $($Choice.FreshCount) świeżych botów na CH3" "three channels, $($Choice.Share)% of the bots on CH2, $($Choice.FreshCount) fresh bots on CH3") }
        default { return (UI-Text "cztery kanały, $($Choice.Share)% botów na CH2, $($Choice.FreshCount) świeżych botów na CH3 i CH4" "four channels, $($Choice.Share)% of the bots on CH2, $($Choice.FreshCount) fresh bots on CH3 and CH4") }
    }
}

function Sync-ChannelWishFromPanel {
    # The web panel cannot write .env; it leaves its channel wish - the second
    # channel with its share, and the fresh channels with their count - in the
    # spool the game container reads (channels.wanted, with SET_AT). The
    # container honours it for the bots at its next start whatever happens
    # here, but only .env can publish the channels' ports - so a wish newer
    # than .env's own is copied into .env before the stack comes up.
    # Only while the game container runs: its spool cannot be read otherwise.
    $envPath = Get-PlayerbotEnvPath
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) { return }
    $composeDir = Join-Path $serverRoot 'linux-port\docker'
    $composeFile = Join-Path $composeDir 'docker-compose.yml'
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $text = @(docker compose --project-directory $composeDir -f $composeFile exec -T game cat /opt/m2spool/channels.wanted 2>$null)
        $exit = $LASTEXITCODE
    }
    catch { return }
    finally { $ErrorActionPreference = $previousPreference }
    if ($exit -ne 0 -or $text.Count -eq 0) { return }
    $wish = @{}
    foreach ($line in $text) {
        if ("$line" -match '^\s*([A-Z0-9_]+)=(\d+)\s*$') { $wish[$Matches[1]] = [long]$Matches[2] }
    }
    if (-not $wish.ContainsKey('CH2') -or -not $wish.ContainsKey('SET_AT')) { return }
    $envAt = 0L
    [long]::TryParse((Get-DotEnvValue -Key 'M2_PLAYERBOT_CH2_SET_AT' -Default '0'), [ref]$envAt) | Out-Null
    if ($wish['SET_AT'] -le $envAt) { return }
    $current = Get-GameChannelsFromEnv
    $share = if ($wish.ContainsKey('SHARE')) { [int]$wish['SHARE'] } else { 40 }
    # A wish that does not name the fresh channels (a panel from before them)
    # leaves .env's, as the game container does.
    $fresh = if ($wish.ContainsKey('FRESH') -and $wish['FRESH'] -le 2) { [int]$wish['FRESH'] } else { [int]$current.Fresh }
    $freshCount = if ($wish.ContainsKey('FRESH_COUNT')) { [int]$wish['FRESH_COUNT'] } else { [int]$current.FreshCount }
    $channels = if ($fresh -gt 0) { 2 + $fresh } elseif ($wish['CH2'] -eq 1) { 2 } else { 1 }
    $applied = Set-GameChannels -Channels $channels -Share $share -FreshCount $freshCount -SetAt $wish['SET_AT']
    Write-Host (UI-Text "Kanaly gry ustawione w panelu WWW: $(Format-GameChannelsChoice -Choice $applied)." "Game channels set in the web panel: $(Format-GameChannelsChoice -Choice $applied).") -ForegroundColor Green
}

function Set-BotCountAction {
    $current = Get-PlayerbotCount
    $plan = Get-SpawnPlanFromEnv
    Write-Host (UI-Text "Aktualnie gra: $current botów (efektywny limit = liczba botów w Twoim świecie; kanoniczna paczka ma 350)." "Playing now: $current bots (the real limit is the number of bots in your world; the canonical package has 350).") -ForegroundColor Gray
    Write-Host (UI-Text "Wchodzą w ciągu $($plan.Minutes) min od startu; dodatkowych botów dołączających stopniowo: $($plan.Late) w ciągu $($plan.Hours) h." "They come in within $($plan.Minutes) min of the start; extra bots joining gradually: $($plan.Late) over $($plan.Hours) h.") -ForegroundColor Gray

    # -BotCount passed (from the GUI or scripting) is non-interactive: never call
    # Read-Host, because the GUI runs this in a hidden, non-interactive console.
    # Restart only when -Yes is also given. Without -BotCount we are in the text
    # menu and can prompt for the numbers and the restart.
    if ($BotCount -ge 0) {
        $applied = Set-PlayerbotCount -Count $BotCount
        Write-Host (UI-Text "Zapisano: $applied grających botów." "Saved: $applied playing bots.") -ForegroundColor Green
        if ($SpawnMinutes -ge 0 -or $LateJoiners -ge 0 -or $LateHours -ge 0) {
            $m = if ($SpawnMinutes -ge 0) { $SpawnMinutes } else { [int]$plan.Minutes }
            $l = if ($LateJoiners -ge 0) { $LateJoiners } else { [int]$plan.Late }
            $h = if ($LateHours -ge 0) { $LateHours } else { [int]$plan.Hours }
            $p = Set-SpawnPlan -Minutes $m -Late $l -Hours $h
            Write-Host (UI-Text "Zapisano: wejście w $($p.Minutes) min, $($p.Late) dodatkowych botów w ciągu $($p.Hours) h." "Saved: entry over $($p.Minutes) min, $($p.Late) extra bots over $($p.Hours) h.") -ForegroundColor Green
        }
        if ($PerKingdom -ge 0) {
            $k = Get-KingdomCountsFromEnv
            $s = if ($ShinsooBots -ge 0) { $ShinsooBots } else { $k.Shinsoo }
            $c = if ($ChunjoBots -ge 0) { $ChunjoBots } else { $k.Chunjo }
            $j = if ($JinnoBots -ge 0) { $JinnoBots } else { $k.Jinno }
            $kk = Set-KingdomCounts -Enabled ($PerKingdom -eq 1) -Shinsoo $s -Chunjo $c -Jinno $j
            if ($kk.Enabled) {
                Write-Host (UI-Text "Zapisano: osobno dla królestw - Shinsoo $($kk.Shinsoo), Chunjo $($kk.Chunjo), Jinno $($kk.Jinno)." "Saved: per kingdom - Shinsoo $($kk.Shinsoo), Chunjo $($kk.Chunjo), Jinno $($kk.Jinno).") -ForegroundColor Green
                # A kingdom at zero starts nobody, and nothing in the game says
                # so afterwards - the world simply has no bots there. It is a
                # legitimate setting, so it is said out loud rather than
                # refused.
                $empty = @()
                if ($kk.Shinsoo -le 0) { $empty += 'Shinsoo' }
                if ($kk.Chunjo -le 0) { $empty += 'Chunjo' }
                if ($kk.Jinno -le 0) { $empty += 'Jinno' }
                if (@($empty).Count -gt 0) {
                    Write-Host ((UI-Text "UWAGA: " "WARNING: ") + ($empty -join (UI-Text ' i ' ' and ')) + (UI-Text " nie wystartuje zadnego bota. Wpisz tam liczbe wieksza od zera albo wylacz indywidualne wartosci." " will start no bot. Enter a number above zero there or turn the separate numbers off.")) -ForegroundColor Yellow
                }
            }
            else { Write-Host (UI-Text 'Zapisano: jedna liczba botów dzielona po równo na królestwa.' 'Saved: one bot count, shared equally between the kingdoms.') -ForegroundColor Green }
        }
        if ($GameChannels -ge 0 -or $Channel2 -ge 0) {
            # Written only when it changes, so the moment of the choice stays the
            # one it was made at and a wish from the web panel made after it is
            # not overwritten by a dialog that only changed the bot count.
            # -Channel2 alone is the older switch: on keeps whatever channels
            # past the second there are, off is the first channel alone.
            $cur = Get-GameChannelsFromEnv
            $want = if ($GameChannels -ge 0) { $GameChannels } elseif ($Channel2 -eq 1) { [Math]::Max(2, [int]$cur.Channels) } else { 1 }
            $share = if ($Channel2Share -ge 0) { $Channel2Share } else { $cur.Share }
            $count = if ($FreshCount -ge 0) { $FreshCount } else { $cur.FreshCount }
            if ($want -ne $cur.Channels -or ($want -ge 2 -and $share -ne $cur.Share) -or
                ($want -ge 3 -and $count -ne $cur.FreshCount)) {
                $ch = Set-GameChannels -Channels $want -Share $share -FreshCount $count
                Write-Host (UI-Text "Zapisano: $(Format-GameChannelsChoice -Choice $ch)." "Saved: $(Format-GameChannelsChoice -Choice $ch).") -ForegroundColor Green
                if ($ch.Channels -lt $want) { Write-Host (UI-Text 'Kanały 3 i 4 ze świeżymi botami są tylko na linii 2.x - zostają dwa kanały.' 'Channels 3 and 4 with fresh bots are on the 2.x line only - two channels stay.') -ForegroundColor Yellow }
                $memory = Get-M2ChannelMemoryWarning -Channels $ch.Channels
                if ($memory) { Write-Host $memory -ForegroundColor Yellow }
            }
        }
        if ($Yes) {
            Start-Server
            Write-Host (UI-Text "Serwer zrestartowany z liczbą botów: $applied." "Server restarted with $applied bots.") -ForegroundColor Green
        }
        else {
            Write-Host (UI-Text 'Zmiana zostanie zastosowana przy następnym starcie serwera.' 'The change applies at the next server start.') -ForegroundColor Yellow
        }
        return
    }

    $answer = Read-Host (UI-Text 'Ilu botów ma grać (0-2500)' 'How many bots should play (0-2500)')
    if ($answer -notmatch '^\d+$') { Write-Host (UI-Text 'Anulowano: to nie jest liczba.' 'Cancelled: that is not a number.') -ForegroundColor Yellow; return }
    $applied = Set-PlayerbotCount -Count ([int]$answer)
    Write-Host (UI-Text "Zapisano: $applied grających botów." "Saved: $applied playing bots.") -ForegroundColor Green
    # The same words as the "?" of the window's bot dialog, shorter.
    Write-Host (UI-Text 'Wejście: w ile minut od startu serwera wchodzą boty podane wyżej (1 = prawie od razu, 15 = stopniowo przez kwadrans).' 'Entry: within how many minutes of the server start the bots above come in (1 = almost at once, 15 = gradually over a quarter of an hour).') -ForegroundColor Gray
    Write-Host (UI-Text 'Dodatkowe boty: dołączają później pojedynczo, ponad liczbę wyżej, równo rozłożone na podane godziny (0 = bez dodatkowych).' 'Extra bots: they join later one by one, on top of the number above, spread evenly over the hours given (0 = none).') -ForegroundColor Gray
    $m = Read-Host (UI-Text "W ciągu ilu minut od startu mają wejść (1-180, Enter = $($plan.Minutes))" "Within how many minutes of the start should they come in (1-180, Enter = $($plan.Minutes))")
    $l = Read-Host (UI-Text "Ilu dodatkowych botów ma dołączać stopniowo później (0-2500, Enter = $($plan.Late))" "How many extra bots should join gradually later (0-2500, Enter = $($plan.Late))")
    $h = Read-Host (UI-Text "W ciągu ilu godzin mają dołączać (1-168, Enter = $($plan.Hours))" "Over how many hours should they join (1-168, Enter = $($plan.Hours))")
    if (-not "$m".Trim()) { $m = $plan.Minutes }
    if (-not "$l".Trim()) { $l = $plan.Late }
    if (-not "$h".Trim()) { $h = $plan.Hours }
    if ("$m" -notmatch '^\d+$' -or "$l" -notmatch '^\d+$' -or "$h" -notmatch '^\d+$') {
        Write-Host (UI-Text 'Plan wejścia bez zmian: to nie są liczby.' 'Entry plan unchanged: those are not numbers.') -ForegroundColor Yellow
    }
    else {
        $p = Set-SpawnPlan -Minutes ([int]$m) -Late ([int]$l) -Hours ([int]$h)
        Write-Host (UI-Text "Zapisano: wejście w $($p.Minutes) min, $($p.Late) dodatkowych botów w ciągu $($p.Hours) h." "Saved: entry over $($p.Minutes) min, $($p.Late) extra bots over $($p.Hours) h.") -ForegroundColor Green
    }
    $k = Get-KingdomCountsFromEnv
    $kAnswer = Read-Host (UI-Text "Osobna liczba botów dla każdego królestwa? (t/n, Enter = $(if ($k.Enabled) { 't' } else { 'n' }))" "A separate bot count for each kingdom? (y/n, Enter = $(if ($k.Enabled) { 'y' } else { 'n' }))")
    if ("$kAnswer".Trim() -match '^[tTyY]') {
        $sAnswer = Read-Host (UI-Text "Shinsoo, czerwone (0-1500, Enter = $($k.Shinsoo))" "Shinsoo, red (0-1500, Enter = $($k.Shinsoo))")
        $cAnswer = Read-Host (UI-Text "Chunjo, żółte (0-1500, Enter = $($k.Chunjo))" "Chunjo, yellow (0-1500, Enter = $($k.Chunjo))")
        $jAnswer = Read-Host (UI-Text "Jinno, niebieskie (0-1500, Enter = $($k.Jinno))" "Jinno, blue (0-1500, Enter = $($k.Jinno))")
        $s = if ("$sAnswer".Trim() -match '^\d+$') { [int]$sAnswer } else { $k.Shinsoo }
        $c = if ("$cAnswer".Trim() -match '^\d+$') { [int]$cAnswer } else { $k.Chunjo }
        $j = if ("$jAnswer".Trim() -match '^\d+$') { [int]$jAnswer } else { $k.Jinno }
        $kk = Set-KingdomCounts -Enabled $true -Shinsoo $s -Chunjo $c -Jinno $j
        Write-Host (UI-Text "Zapisano: Shinsoo $($kk.Shinsoo), Chunjo $($kk.Chunjo), Jinno $($kk.Jinno)." "Saved: Shinsoo $($kk.Shinsoo), Chunjo $($kk.Chunjo), Jinno $($kk.Jinno).") -ForegroundColor Green
    }
    elseif ("$kAnswer".Trim() -match '^[nN]') {
        Set-KingdomCounts -Enabled $false -Shinsoo $k.Shinsoo -Chunjo $k.Chunjo -Jinno $k.Jinno | Out-Null
        Write-Host (UI-Text 'Zapisano: jedna liczba botów dzielona po równo na królestwa.' 'Saved: one bot count, shared equally between the kingdoms.') -ForegroundColor Green
    }
    $gc = Get-GameChannelsFromEnv
    # The same words as the "?" of the window's channel choice, shorter.
    Write-Host (UI-Text 'Kanały gry: 1 = jeden kanał; 2 = drugi kanał dla botów i graczy (sklepy tylko na CH1); 3 i 4 = do tego kanały ze świeżymi botami od 1 poziomu (CH3, CH4; tylko linia 2.x). Każdy kanał to ok. 2,5 GB RAM.' 'Game channels: 1 = one channel; 2 = a second channel for bots and players (shops on CH1 only); 3 and 4 = and channels with fresh bots from level 1 (CH3, CH4; the 2.x line only). Every channel is about 2.5 GB of RAM.') -ForegroundColor Gray
    $chAnswer = Read-Host (UI-Text "Ile kanałów gry (1-4, Enter = $($gc.Channels))" "How many game channels (1-4, Enter = $($gc.Channels))")
    if ("$chAnswer".Trim() -match '^[1-4]$') {
        $want = [int]"$chAnswer".Trim()
        $share = $gc.Share
        $count = $gc.FreshCount
        if ($want -ge 2) {
            $shAnswer = Read-Host (UI-Text "Ile procent botów na CH2 (10-90, Enter = $($gc.Share))" "What percentage of the bots on CH2 (10-90, Enter = $($gc.Share))")
            if ("$shAnswer".Trim() -match '^\d+$') { $share = [int]$shAnswer }
        }
        if ($want -ge 3) {
            $frAnswer = Read-Host (UI-Text "Ilu świeżych botów ma grać na CH3$(if ($want -ge 4) { ' i CH4' }) (0-1500, Enter = $($gc.FreshCount))" "How many fresh bots should play on CH3$(if ($want -ge 4) { ' and CH4' }) (0-1500, Enter = $($gc.FreshCount))")
            if ("$frAnswer".Trim() -match '^\d+$') { $count = [int]$frAnswer }
        }
        $applied2 = Set-GameChannels -Channels $want -Share $share -FreshCount $count
        Write-Host (UI-Text "Zapisano: $(Format-GameChannelsChoice -Choice $applied2)." "Saved: $(Format-GameChannelsChoice -Choice $applied2).") -ForegroundColor Green
        if ($applied2.Channels -lt $want) { Write-Host (UI-Text 'Kanały 3 i 4 ze świeżymi botami są tylko na linii 2.x - zostają dwa kanały.' 'Channels 3 and 4 with fresh bots are on the 2.x line only - two channels stay.') -ForegroundColor Yellow }
        $memory = Get-M2ChannelMemoryWarning -Channels $applied2.Channels
        if ($memory) { Write-Host $memory -ForegroundColor Yellow }
    }
    elseif ("$chAnswer".Trim()) {
        Write-Host (UI-Text 'Kanały bez zmian: to nie jest liczba od 1 do 4.' 'Channels unchanged: that is not a number from 1 to 4.') -ForegroundColor Yellow
    }
    if (Confirm-Operation (UI-Text 'Zrestartować serwer teraz, aby zastosować zmianę? Baza i postęp botów pozostają bez zmian' 'Restart the server now to apply the change? The database and the bots'' progress stay as they are')) {
        Start-Server
        Write-Host (UI-Text "Serwer zrestartowany z liczbą botów: $applied." "Server restarted with $applied bots.") -ForegroundColor Green
    }
    else {
        Write-Host (UI-Text 'Zmiana zostanie zastosowana przy następnym starcie serwera.' 'The change applies at the next server start.') -ForegroundColor Yellow
    }
}

# The world's difficulty: how long a player waits at the Biologist between two
# hand-ins and at the stable keeper (the pony, each Horse Book, the medal
# trainings). M2_DIFFICULTY in .env - easy, medium, hard or custom with the two
# hour counts - is turned into event flags by the migrate service at every
# start and read by the quests (linux-port-mt2009/docker/game/quest/
# m2_difficulty.lua), so a change needs a restart. The bots never waited.
# The skill books' waits are the package's twenty-one hours on hard and a
# third of them on medium, for the players and the bots alike; the migrator's
# presets (apply.sh) carry the same numbers in seconds.
$script:DifficultyPresets = @{
    easy   = @{ Biologist = '0';  Horse = '0';  Book = '0';  BotBook = '0' }
    medium = @{ Biologist = '8';  Horse = '4';  Book = '7';  BotBook = '7' }
    hard   = @{ Biologist = '24'; Horse = '12'; Book = '21'; BotBook = '21' }
}

function Get-DotEnvValue {
    param([Parameter(Mandatory = $true)][string]$Key, [string]$Default = '')
    $envPath = Get-PlayerbotEnvPath
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) { return $Default }
    $match = [Regex]::Match([IO.File]::ReadAllText($envPath), '(?m)^' + [Regex]::Escape($Key) + '=(.*?)\s*$')
    if ($match.Success) { return $match.Groups[1].Value.Trim() }
    return $Default
}

function Set-DotEnvValue {
    # One key of .env replaced in place or appended; nothing else in the file -
    # the player's own passwords included - is touched. Same shape as
    # Set-PlayerbotCount. The value is a literal: a $ in it must not become a
    # group reference for Regex.Replace.
    param([Parameter(Mandatory = $true)][string]$Key, [Parameter(Mandatory = $true)][string]$Value)
    $envPath = Get-PlayerbotEnvPath
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) {
        throw (UI-Text "Brak pliku .env: $envPath. Uruchom najpierw serwer (GRAJ), aby go utworzyć." "There is no .env file: $envPath. Start the server (PLAY) first to create it.")
    }
    $content = [IO.File]::ReadAllText($envPath)
    $pattern = '(?m)^' + [Regex]::Escape($Key) + '=.*$'
    $line = "$Key=$Value"
    if ([Regex]::IsMatch($content, $pattern)) {
        $content = [Regex]::Replace($content, $pattern, $line.Replace('$', '$$'))
    }
    else {
        if ($content -and -not $content.EndsWith("`n")) { $content += [Environment]::NewLine }
        $content += $line + [Environment]::NewLine
    }
    [IO.File]::WriteAllText($envPath, $content, [Text.UTF8Encoding]::new($false))
}

function Test-DifficultyHours {
    param([string]$Text)
    $n = 0.0
    $ok = [double]::TryParse("$Text".Trim().Replace(',', '.'), [Globalization.NumberStyles]::Float,
        [Globalization.CultureInfo]::InvariantCulture, [ref]$n)
    return ($ok -and $n -ge 0 -and $n -le 720)
}

function Set-DifficultyAction {
    $current = Get-DotEnvValue -Key 'M2_DIFFICULTY' -Default 'easy'
    $currentBio = Get-DotEnvValue -Key 'M2_BIOLOGIST_WAIT_HOURS' -Default '0'
    $currentHorse = Get-DotEnvValue -Key 'M2_HORSE_WAIT_HOURS' -Default '0'
    $currentBook = Get-DotEnvValue -Key 'M2_BOOK_WAIT_HOURS' -Default '0'
    $currentBotBook = Get-DotEnvValue -Key 'M2_BOT_BOOK_WAIT_HOURS' -Default '0'
    $currentAutoHunt = (Get-DotEnvValue -Key 'M2_AUTOHUNT' -Default '1') -ne '0'
    $currentAutoHuntItem = (Get-DotEnvValue -Key 'M2_AUTOHUNT_ITEM' -Default '0') -eq '1'
    $currentSidekick = (Get-DotEnvValue -Key 'M2_SIDEKICK' -Default '1') -ne '0'
    $currentStarter = (Get-DotEnvValue -Key 'M2_STARTER_CHEST' -Default '1') -ne '0'
    $currentFlea = (Get-DotEnvValue -Key 'M2_FLEA_MARKET' -Default '1') -ne '0'
    Write-Host (UI-Text "Aktualny poziom trudności: $current (przy 'custom': Biolog $currentBio h, Stajenny $currentHorse h, księgi: gracze $currentBook h, boty $currentBotBook h)." "Current difficulty: $current (with 'custom': Biologist $currentBio h, Stable Keeper $currentHorse h, books: players $currentBook h, bots $currentBotBook h).") -ForegroundColor Gray
    Write-Host (UI-Text "Auto Łowy: $(if ($currentAutoHunt) { 'włączone' } else { 'wyłączone' }) ($(if ($currentAutoHuntItem) { 'tylko po kupnie przedmiotu z ItemShop' } else { 'dla każdego' })); Towarzysz: $(if ($currentSidekick) { 'włączony' } else { 'wyłączony' }); Skrzynia Ucznia: $(if ($currentStarter) { 'tak' } else { 'nie' }); Dom Towarowy: $(if ($currentFlea) { 'włączony' } else { 'wyłączony' })." "Auto Hunt: $(if ($currentAutoHunt) { 'on' } else { 'off' }) ($(if ($currentAutoHuntItem) { 'only after buying the ItemShop item' } else { 'for everybody' })); Companion: $(if ($currentSidekick) { 'on' } else { 'off' }); Apprentice Chest: $(if ($currentStarter) { 'yes' } else { 'no' }); Flea Market: $(if ($currentFlea) { 'on' } else { 'off' }).") -ForegroundColor Gray

    # -Difficulty passed (from the GUI or scripting) is non-interactive, like
    # -BotCount: never Read-Host, restart only with -Yes.
    $level = "$Difficulty".Trim().ToLowerInvariant()
    $bio = "$BiologistHours"
    $horse = "$HorseHours"
    $book = "$BookHours"
    $botBook = "$BotBookHours"
    $interactive = (-not $level)
    if ($interactive) {
        Write-Host (UI-Text ' 1. easy   - bez czekania u Biologa, u Stajennego i na kolejną księgę (tak jak dotąd)' ' 1. easy   - no waiting at the Biologist, at the Stable Keeper or for the next book (as before)')
        Write-Host (UI-Text ' 2. medium - Biolog 8 h; kucyk i Księgi Konia 4 h; treningi konia 6 h (1-10) i 7 h (11-19); księgi 7 h' ' 2. medium - Biologist 8 h; pony and Horse Books 4 h; horse trainings 6 h (1-10) and 7 h (11-19); books 7 h')
        Write-Host (UI-Text ' 3. hard   - jak w oryginale: Biolog 24 h; kucyk i Księgi 12 h; treningi 18 h i 21 h; księgi 21 h' ' 3. hard   - as in the original: Biologist 24 h; pony and Horse Books 12 h; trainings 18 h and 21 h; books 21 h')
        Write-Host (UI-Text ' 4. custom - własne godziny (Biolog, każde czekanie u Stajennego, księgi graczy i księgi botów)' ' 4. custom - your own hours (Biologist, every wait at the Stable Keeper, players'' books and bots'' books)')
        $answer = Read-Host (UI-Text 'Wybierz poziom (1-4)' 'Choose a level (1-4)')
        $level = switch ($answer) { '1' { 'easy' } '2' { 'medium' } '3' { 'hard' } '4' { 'custom' } default { '' } }
        if (-not $level) { Write-Host (UI-Text 'Anulowano.' 'Cancelled.') -ForegroundColor Yellow; return }
        if ($level -eq 'custom') {
            $bio = Read-Host (UI-Text 'Ile godzin czeka się u Biologa między oddaniami (0 = bez czekania, ułamki dozwolone)' 'Hours of waiting at the Biologist between two hand-ins (0 = no waiting, fractions allowed)')
            $horse = Read-Host (UI-Text 'Ile godzin czeka się u Stajennego na kucyka, Księgę Konia i trening (0 = bez czekania)' 'Hours of waiting at the Stable Keeper for the pony, a Horse Book and a training (0 = no waiting)')
            $book = Read-Host (UI-Text 'Ile godzin gracz czeka między dwiema księgami tej samej umiejętności (0 = od razu)' 'Hours a player waits between two books of the same skill (0 = at once)')
            $botBook = Read-Host (UI-Text 'Ile godzin czekają na kolejną księgę boty (0 = od razu)' 'Hours the bots wait for their next book (0 = at once)')
        }
    }
    # Auto Lowy, the companion, the apprentice chest and the Dom Towarowy:
    # asked in the text menu after the level, and taken from -AutoHunt/
    # -Sidekick/-StarterChest/-FleaMarket otherwise; what .env says when neither. The chest was asked only where a
    # fresh world is made, so a world already standing had no way to it - and
    # "gdzie te skrzynie ucznia do wylaczenia ... w launcherze szukam, ni ma"
    # (Drip, 25 September) was answered with this very window.
    $autoHuntOn = $currentAutoHunt
    $autoHuntItemOn = $currentAutoHuntItem
    $sidekickOn = $currentSidekick
    $starterOn = $currentStarter
    $fleaOn = $currentFlea
    if ($interactive) {
        $answer = Read-Host (UI-Text "Auto Łowy (automatyczne polowanie w kliencie, klawisz K) włączone? (T/n, Enter = $(if ($currentAutoHunt) { 'tak' } else { 'nie' }))" "Auto Hunt (automatic hunting in the client, the K key) on? (Y/n, Enter = $(if ($currentAutoHunt) { 'yes' } else { 'no' }))")
        if ("$answer".Trim()) { $autoHuntOn = "$answer".Trim().ToLowerInvariant() -notin @('n', 'nie', 'no', '0') }
        if ($autoHuntOn) {
            # The operator, 27 September: the panel for everybody, or only for
            # a character that bought "Auto Lowy (8h)" in the ItemShop.
            $answer = Read-Host (UI-Text "Panel Autołowy: 1 = dostępny dla każdego, 2 = dostępny tylko po kupnie przedmiotu z ItemShop (Enter = $(if ($currentAutoHuntItem) { '2' } else { '1' }))" "Auto Hunt window: 1 = for everybody, 2 = only after buying the ItemShop item (Enter = $(if ($currentAutoHuntItem) { '2' } else { '1' }))")
            if ("$answer".Trim() -eq '1') { $autoHuntItemOn = $false }
            elseif ("$answer".Trim() -eq '2') { $autoHuntItemOn = $true }
        }
        $answer = Read-Host (UI-Text "Towarzysz (stały kompan gracza, list i okno P) włączony? (T/n, Enter = $(if ($currentSidekick) { 'tak' } else { 'nie' }))" "Companion (a player's own companion, the letter and the P window) on? (Y/n, Enter = $(if ($currentSidekick) { 'yes' } else { 'no' }))")
        if ("$answer".Trim()) { $sidekickOn = "$answer".Trim().ToLowerInvariant() -notin @('n', 'nie', 'no', '0') }
        $answer = Read-Host (UI-Text "Skrzynia Ucznia w grze - dla nowych postaci graczy i dla botów? (T/n, Enter = $(if ($currentStarter) { 'tak' } else { 'nie' }))" "Apprentice Chest in the game - for players' new characters and for the bots? (Y/n, Enter = $(if ($currentStarter) { 'yes' } else { 'no' }))")
        if ("$answer".Trim()) { $starterOn = "$answer".Trim().ToLowerInvariant() -notin @('n', 'nie', 'no', '0') }
        $answer = Read-Host (UI-Text "Dom Towarowy (wszystkie oferty sklepów offline u Handlarki Różności w M1) włączony? (T/n, Enter = $(if ($currentFlea) { 'tak' } else { 'nie' }))" "Flea Market (every offline shop offer at the General Store merchant in M1) on? (Y/n, Enter = $(if ($currentFlea) { 'yes' } else { 'no' }))")
        if ("$answer".Trim()) { $fleaOn = "$answer".Trim().ToLowerInvariant() -notin @('n', 'nie', 'no', '0') }
    }
    else {
        if ($AutoHunt -ge 0) { $autoHuntOn = ($AutoHunt -ne 0) }
        if ($AutoHuntItem -ge 0) { $autoHuntItemOn = ($AutoHuntItem -ne 0) }
        if ($Sidekick -ge 0) { $sidekickOn = ($Sidekick -ne 0) }
        if ($StarterChest -ge 0) { $starterOn = ($StarterChest -ne 0) }
        if ($FleaMarket -ge 0) { $fleaOn = ($FleaMarket -ne 0) }
    }
    if ($level -notin @('easy', 'medium', 'hard', 'custom')) {
        throw (UI-Text "Nieznany poziom trudności: '$level'. Dozwolone: easy, medium, hard, custom." "Unknown difficulty: '$level'. Allowed: easy, medium, hard, custom.")
    }
    if ($level -ne 'custom') {
        $bio = $script:DifficultyPresets[$level].Biologist
        $horse = $script:DifficultyPresets[$level].Horse
        $book = $script:DifficultyPresets[$level].Book
        $botBook = $script:DifficultyPresets[$level].BotBook
    }
    # An older GUI passes no book hours for custom: what .env already says.
    if ("$book".Trim() -eq '') { $book = $currentBook }
    if ("$botBook".Trim() -eq '') { $botBook = $currentBotBook }
    if (-not (Test-DifficultyHours $bio)) { throw (UI-Text "Godziny u Biologa: podaj liczbę od 0 do 720 (np. 12 albo 0.5), nie '$bio'." "Hours at the Biologist: give a number from 0 to 720 (e.g. 12 or 0.5), not '$bio'.") }
    if (-not (Test-DifficultyHours $horse)) { throw (UI-Text "Godziny u Stajennego: podaj liczbę od 0 do 720 (np. 12 albo 0.5), nie '$horse'." "Hours at the Stable Keeper: give a number from 0 to 720 (e.g. 12 or 0.5), not '$horse'.") }
    if (-not (Test-DifficultyHours $book)) { throw (UI-Text "Godziny między księgami graczy: podaj liczbę od 0 do 720 (np. 21 albo 0.5), nie '$book'." "Hours between players' books: give a number from 0 to 720 (e.g. 21 or 0.5), not '$book'.") }
    if (-not (Test-DifficultyHours $botBook)) { throw (UI-Text "Godziny między księgami botów: podaj liczbę od 0 do 720 (np. 21 albo 0.5), nie '$botBook'." "Hours between bots' books: give a number from 0 to 720 (e.g. 21 or 0.5), not '$botBook'.") }
    $bio = "$bio".Trim().Replace(',', '.')
    $horse = "$horse".Trim().Replace(',', '.')
    $book = "$book".Trim().Replace(',', '.')
    $botBook = "$botBook".Trim().Replace(',', '.')
    Set-DotEnvValue -Key 'M2_DIFFICULTY' -Value $level
    Set-DotEnvValue -Key 'M2_BIOLOGIST_WAIT_HOURS' -Value $bio
    Set-DotEnvValue -Key 'M2_HORSE_WAIT_HOURS' -Value $horse
    Set-DotEnvValue -Key 'M2_BOOK_WAIT_HOURS' -Value $book
    Set-DotEnvValue -Key 'M2_BOT_BOOK_WAIT_HOURS' -Value $botBook
    Set-DotEnvValue -Key 'M2_AUTOHUNT' -Value $(if ($autoHuntOn) { '1' } else { '0' })
    Set-DotEnvValue -Key 'M2_AUTOHUNT_ITEM' -Value $(if ($autoHuntItemOn) { '1' } else { '0' })
    Set-DotEnvValue -Key 'M2_SIDEKICK' -Value $(if ($sidekickOn) { '1' } else { '0' })
    Set-DotEnvValue -Key 'M2_STARTER_CHEST' -Value $(if ($starterOn) { '1' } else { '0' })
    Set-DotEnvValue -Key 'M2_FLEA_MARKET' -Value $(if ($fleaOn) { '1' } else { '0' })
    Write-Host (UI-Text "Zapisano: poziom trudności $level (Biolog $bio h, Stajenny $horse h, księgi: gracze $book h, boty $botBook h)." "Saved: difficulty $level (Biologist $bio h, Stable Keeper $horse h, books: players $book h, bots $botBook h).") -ForegroundColor Green
    Write-Host (UI-Text "Auto Łowy: $(if ($autoHuntOn) { 'włączone' } else { 'wyłączone' }) ($(if ($autoHuntItemOn) { 'tylko po kupnie przedmiotu z ItemShop' } else { 'dla każdego' })); Towarzysz: $(if ($sidekickOn) { 'włączony' } else { 'wyłączony' }); Skrzynia Ucznia: $(if ($starterOn) { 'tak' } else { 'nie' }); Dom Towarowy: $(if ($fleaOn) { 'włączony' } else { 'wyłączony' })." "Auto Hunt: $(if ($autoHuntOn) { 'on' } else { 'off' }) ($(if ($autoHuntItemOn) { 'only after buying the ItemShop item' } else { 'for everybody' })); Companion: $(if ($sidekickOn) { 'on' } else { 'off' }); Apprentice Chest: $(if ($starterOn) { 'yes' } else { 'no' }); Flea Market: $(if ($fleaOn) { 'on' } else { 'off' }).") -ForegroundColor Green
    if ($Yes) {
        Start-Server
        Write-Host (UI-Text "Serwer zrestartowany z poziomem trudności: $level." "Server restarted with difficulty: $level.") -ForegroundColor Green
        return
    }
    if ($interactive -and (Confirm-Operation (UI-Text 'Zrestartować serwer teraz, aby zastosować zmianę? Baza i postęp botów pozostają bez zmian' 'Restart the server now to apply the change? The database and the bots'' progress stay as they are'))) {
        Start-Server
        Write-Host (UI-Text "Serwer zrestartowany z poziomem trudności: $level." "Server restarted with difficulty: $level.") -ForegroundColor Green
        return
    }
    Write-Host (UI-Text 'Zmiana zostanie zastosowana przy następnym starcie serwera.' 'The change applies at the next server start.') -ForegroundColor Yellow
}

function Get-CurrentInstallTargetVolume {
    # The db-data volume of THIS installation (import target).
    $statePath = Join-Path $serverRoot '.m2install.json'
    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        try {
            $state = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
            $volProp = $state.PSObject.Properties['databaseVolume']
            if ($volProp -and [string]$volProp.Value) { return [string]$volProp.Value }
            if ([string]$state.projectName) { return "$([string]$state.projectName)_db-data" }
        }
        catch { }
    }
    $envPath = Join-Path $serverRoot 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $match = [Regex]::Match([IO.File]::ReadAllText($envPath), '(?m)^M2_COMPOSE_PROJECT_NAME=([a-z0-9][a-z0-9_-]+)\s*$')
        if ($match.Success) { return "$($match.Groups[1].Value)_db-data" }
    }
    return $null
}

function Import-DatabaseAction {
    if (-not (Test-M2DockerRunning)) {
        Write-Host (UI-Text 'Silnik Dockera jest zatrzymany, więc nie widać żadnych baz.' 'The Docker engine is stopped, so no database can be seen.') -ForegroundColor Yellow
        Write-Host (UI-Text 'Uruchom Docker (akcja StartDocker lub przycisk „URUCHOM DOCKER") i spróbuj ponownie.' 'Start Docker (the StartDocker action or the START DOCKER button) and try again.') -ForegroundColor Yellow
        Write-Host (UI-Text 'Żadne dane nie zginęły — bazy są na dysku, tylko Docker ich teraz nie pokazuje.' 'No data was lost - the databases are on the disk, Docker just does not show them now.') -ForegroundColor Gray
        return
    }
    $target = Get-CurrentInstallTargetVolume
    if (-not $target) {
        Write-Host (UI-Text 'Nie można ustalić bazy tej instalacji. Uruchom najpierw serwer (GRAJ) choć raz, aby utworzyć tożsamość i wolumen.' 'Cannot tell which database is this installation''s. Start the server (PLAY) once first to create its identity and volume.') -ForegroundColor Yellow
        return
    }
    Write-Host (UI-Text "Baza docelowa (ta instalacja): $target" "Target database (this installation): $target") -ForegroundColor Gray
    if (-not (Test-M2VolumeInitialized -Volume $target)) {
        Write-Host (UI-Text 'Ta instalacja nie ma jeszcze gotowej bazy danych.' 'This installation has no database ready yet.') -ForegroundColor Yellow
        Write-Host (UI-Text 'Najpierw kliknij GRAJ i pozwól serwerowi wystartować choć raz (utworzy bazę ze schematami gry),' 'First click PLAY and let the server start once (it creates the database with the game''s schemas),') -ForegroundColor Yellow
        Write-Host (UI-Text 'a dopiero potem importuj świat. Import na pustą bazę zostawiłby instalację bez schematów.' 'and only then import the world. An import onto an empty database would leave the installation without schemas.') -ForegroundColor Yellow
        return
    }
    $sources = @(Get-M2DbDataVolumes | Where-Object { $_.Name -ne $target })
    if ($sources.Count -eq 0) {
        Write-Host (UI-Text 'Nie znaleziono innej bazy Docker do importu na tym komputerze.' 'No other Docker database to import was found on this computer.') -ForegroundColor Yellow
        return
    }

    $chosen = $null
    if ($ImportSource) {
        $chosen = $sources | Where-Object { $_.Name -eq $ImportSource -or $_.Project -eq $ImportSource } | Select-Object -First 1
        if (-not $chosen) { Write-Host (UI-Text "Nie znaleziono źródła do importu: $ImportSource" "Import source not found: $ImportSource") -ForegroundColor Red; return }
    }
    else {
        Write-Host (UI-Text 'Dostępne bazy do importu:' 'Databases available to import:') -ForegroundColor Cyan
        for ($i = 0; $i -lt $sources.Count; $i++) {
            $label = $sources[$i].Project
            if ($sources[$i].CreatedAt) { $label = (UI-Text '{0}   (utworzona {1:yyyy-MM-dd HH:mm})' '{0}   (created {1:yyyy-MM-dd HH:mm})') -f $label, $sources[$i].CreatedAt }
            Write-Host ("  [{0}] {1}" -f ($i + 1), $label)
        }
        $pick = Read-Host (UI-Text 'Wybierz numer źródła (Enter = anuluj)' 'Choose the source''s number (Enter = cancel)')
        if ($pick -notmatch '^\d+$') { Write-Host (UI-Text 'Anulowano.' 'Cancelled.') -ForegroundColor Yellow; return }
        $idx = [int]$pick - 1
        if ($idx -lt 0 -or $idx -ge $sources.Count) { Write-Host (UI-Text 'Nieprawidłowy numer.' 'Wrong number.') -ForegroundColor Yellow; return }
        $chosen = $sources[$idx]
    }

    Write-Host (UI-Text "Sprawdzam świat źródłowy '$($chosen.Project)'..." "Checking the source world '$($chosen.Project)'...") -ForegroundColor Gray
    $stats = Get-M2VolumeWorldStats -Volume $chosen.Name
    if ($stats.Ok) {
        Write-Host ((UI-Text "Źródło: {0} postaci, najwyższy poziom {1}." "Source: {0} characters, highest level {1}.") -f $stats.Players, $stats.MaxLevel) -ForegroundColor Green
        if ($stats.Created) { Write-Host ((UI-Text "  Baza utworzona: {0}" "  Database created: {0}") -f $stats.Created) -ForegroundColor Gray }
        if ($stats.LastPlay -and $stats.LastPlay -ne '0') { Write-Host ((UI-Text "  Ostatnia gra: {0}" "  Last played: {0}") -f $stats.LastPlay) -ForegroundColor Gray }
    }
    else {
        Write-Host (UI-Text 'Nie udało się odczytać statystyk źródła (mimo to można spróbować importu).' 'Could not read the source''s statistics (the import can still be tried).') -ForegroundColor Yellow
    }

    Write-Host ''
    Write-Host (UI-Text "UWAGA: import ZASTĄPI obecny świat tej instalacji światem ze źródła '$($chosen.Project)'." "WARNING: the import REPLACES this installation's current world with the world from '$($chosen.Project)'.") -ForegroundColor Yellow
    Write-Host (UI-Text "Źródło pozostaje nietknięte. Obecny świat trafi do kopii w 'backups' przed nadpisaniem." "The source stays untouched. The current world goes into a backup in 'backups' before it is overwritten.") -ForegroundColor Yellow
    if (-not (Confirm-Operation (UI-Text "Kontynuować import z '$($chosen.Project)'?" "Go on with the import from '$($chosen.Project)'?"))) { Write-Host (UI-Text 'Anulowano.' 'Cancelled.') -ForegroundColor Yellow; return }

    # Read this install's game DB user/password so the import can re-apply the
    # user and grants afterwards (guards against the migrator failing to
    # authenticate after a swap).
    $dbUser = 'metin2'; $dbPass = ''
    $importEnvPath = Join-Path $serverRoot 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $importEnvPath -PathType Leaf) {
        $importEnvText = [IO.File]::ReadAllText($importEnvPath)
        $userMatch = [Regex]::Match($importEnvText, '(?m)^M2_DB_USER=(.+?)\s*$')
        if ($userMatch.Success) { $dbUser = $userMatch.Groups[1].Value }
        $passMatch = [Regex]::Match($importEnvText, '(?m)^M2_DB_PASSWORD=(.+?)\s*$')
        if ($passMatch.Success) { $dbPass = $passMatch.Groups[1].Value }
    }

    Write-Host (UI-Text 'Zatrzymuję serwer, aby zwolnić bazę docelową...' 'Stopping the server to free the target database...') -ForegroundColor Cyan
    Stop-Server

    Write-Host (UI-Text 'Importuję bazę (to może potrwać chwilę)...' 'Importing the database (this can take a while)...') -ForegroundColor Cyan
    $result = Invoke-M2DatabaseImport -SourceVolume $chosen.Name -TargetVolume $target -BackupRoot (Join-Path $serverRoot 'backups') -DbUser $dbUser -DbPassword $dbPass
    Write-Host ((UI-Text "Gotowe. Zaimportowany świat: {0} postaci, najwyższy poziom {1}." "Done. Imported world: {0} characters, highest level {1}.") -f $result.Players, $result.MaxLevel) -ForegroundColor Green
    Write-Host ((UI-Text "Kopia poprzedniego świata: {0}" "Backup of the previous world: {0}") -f $result.Backup) -ForegroundColor Gray
    Write-Host (UI-Text 'Kliknij GRAJ (lub akcja Start), aby uruchomić serwer z zaimportowanym światem.' 'Click PLAY (or the Start action) to start the server with the imported world.') -ForegroundColor Green
}

function Backup-DatabaseAction {
    # Everything the import path already did to protect a world, asked for on
    # purpose instead of as a side effect: five SQL dumps, a manifest and a zip.
    if (-not (Test-M2DockerRunning)) {
        Write-Host (UI-Text 'Silnik Dockera jest zatrzymany, wiec nie da sie odczytac bazy.' 'The Docker engine is stopped, so the database cannot be read.') -ForegroundColor Yellow
        Write-Host (UI-Text 'Uruchom Docker (akcja StartDocker) i sprobuj ponownie. Nic nie zginelo.' 'Start Docker (the StartDocker action) and try again. Nothing was lost.') -ForegroundColor Gray
        return
    }
    $target = Get-CurrentInstallTargetVolume
    if (-not $target) {
        Write-Host (UI-Text 'Nie mozna ustalic bazy tej instalacji. Uruchom najpierw serwer (GRAJ) choc raz.' 'Cannot tell which database is this installation''s. Start the server (PLAY) once first.') -ForegroundColor Yellow
        return
    }
    if (-not (Test-M2VolumeInitialized -Volume $target)) {
        Write-Host (UI-Text 'Ta instalacja nie ma jeszcze bazy danych - nie ma czego zapisac.' 'This installation has no database yet - there is nothing to save.') -ForegroundColor Yellow
        return
    }
    Write-Host (UI-Text 'Zatrzymuję serwer, aby baza była spójna w chwili zapisu...' 'Stopping the server so that the database is consistent when it is saved...') -ForegroundColor Cyan
    Stop-Server
    Write-Host (UI-Text 'Zapisuję kopię (to może potrwać chwilę)...' 'Saving the backup (this can take a while)...') -ForegroundColor Cyan
    $result = New-M2DatabaseBackup -Volume $target -BackupRoot (Join-Path $serverRoot 'backups')
    Write-Host ((UI-Text "Gotowe. Zapisany świat: {0} postaci, najwyższy poziom {1}." "Done. Saved world: {0} characters, highest level {1}.") -f $result.Players, $result.MaxLevel) -ForegroundColor Green
    Write-Host ("  Folder: {0}" -f $result.Folder) -ForegroundColor Gray
    Write-Host ((UI-Text "  Plik:   {0}  ({1:N0} MB)" "  File:   {0}  ({1:N0} MB)") -f $result.Zip, ($result.ZipBytes / 1MB)) -ForegroundColor Gray
    Write-Host (UI-Text 'Ten jeden plik zip wystarczy, aby odtworzyć świat na tym albo na innym komputerze.' 'This one zip file is enough to restore the world on this or another computer.') -ForegroundColor Gray
    Write-Host (UI-Text 'Kliknij GRAJ, aby uruchomić serwer z powrotem.' 'Click PLAY to start the server again.') -ForegroundColor Green
}

function Restore-DatabaseAction {
    if (-not (Test-M2DockerRunning)) {
        Write-Host (UI-Text 'Silnik Dockera jest zatrzymany. Uruchom Docker i spróbuj ponownie.' 'The Docker engine is stopped. Start Docker and try again.') -ForegroundColor Yellow
        return
    }
    $target = Get-CurrentInstallTargetVolume
    if (-not $target) {
        Write-Host (UI-Text 'Nie mozna ustalic bazy tej instalacji. Uruchom najpierw serwer (GRAJ) choc raz.' 'Cannot tell which database is this installation''s. Start the server (PLAY) once first.') -ForegroundColor Yellow
        return
    }
    $picked = $RestoreSource
    if (-not $picked) {
        $backupRoot = Join-Path $serverRoot 'backups'
        $found = @()
        if (Test-Path -LiteralPath $backupRoot -PathType Container) {
            $found = @(Get-ChildItem -LiteralPath $backupRoot -Filter 'db-backup-*.zip' -File |
                       Sort-Object LastWriteTime -Descending)
        }
        if ($found.Count -eq 0) {
            Write-Host (UI-Text "Nie znaleziono zadnej kopii w '$backupRoot'." "No backup found in '$backupRoot'.") -ForegroundColor Yellow
            Write-Host (UI-Text 'Zrob najpierw kopie (akcja BackupDb), albo podaj sciezke: -RestoreSource "C:\...\db-backup-....zip"' 'Make a backup first (the BackupDb action), or give a path: -RestoreSource "C:\...\db-backup-....zip"') -ForegroundColor Gray
            return
        }
        Write-Host (UI-Text 'Dostępne kopie:' 'Available backups:') -ForegroundColor Cyan
        for ($i = 0; $i -lt $found.Count; $i++) {
            Write-Host ("  [{0}] {1}   ({2:yyyy-MM-dd HH:mm}, {3:N0} MB)" -f ($i + 1),
                $found[$i].Name, $found[$i].LastWriteTime, ($found[$i].Length / 1MB))
        }
        $pick = Read-Host (UI-Text 'Wybierz numer kopii (Enter = anuluj)' 'Choose the backup''s number (Enter = cancel)')
        if ($pick -notmatch '^\d+$') { Write-Host (UI-Text 'Anulowano.' 'Cancelled.') -ForegroundColor Yellow; return }
        $idx = [int]$pick - 1
        if ($idx -lt 0 -or $idx -ge $found.Count) { Write-Host (UI-Text 'Nieprawidłowy numer.' 'Wrong number.') -ForegroundColor Yellow; return }
        $picked = $found[$idx].FullName
    }
    if (-not (Test-Path -LiteralPath $picked)) {
        Write-Host (UI-Text "Nie znaleziono kopii: $picked" "Backup not found: $picked") -ForegroundColor Red
        return
    }
    Write-Host ''
    Write-Host (UI-Text "UWAGA: przywrócenie ZASTĄPI obecny świat tej instalacji zawartością kopii." "WARNING: the restore REPLACES this installation's current world with the backup's contents.") -ForegroundColor Yellow
    Write-Host (UI-Text 'Obecny świat zostanie najpierw zapisany do własnej kopii w folderze backups.' 'The current world is saved to a backup of its own in the backups folder first.') -ForegroundColor Yellow
    if (-not (Confirm-Operation (UI-Text "Przywrócić świat z '$([IO.Path]::GetFileName($picked))'?" "Restore the world from '$([IO.Path]::GetFileName($picked))'?"))) {
        Write-Host (UI-Text 'Anulowano.' 'Cancelled.') -ForegroundColor Yellow; return
    }
    $creds = Get-InstallDbCredentials
    Write-Host (UI-Text 'Zatrzymuję serwer, aby zwolnić bazę...' 'Stopping the server to free the database...') -ForegroundColor Cyan
    Stop-Server
    Write-Host (UI-Text 'Przywracam kopię (to może potrwać chwilę)...' 'Restoring the backup (this can take a while)...') -ForegroundColor Cyan
    $result = Restore-M2DatabaseBackup -BackupPath $picked -TargetVolume $target `
        -BackupRoot (Join-Path $serverRoot 'backups') -DbUser $creds.User -DbPassword $creds.Password
    Write-Host ((UI-Text "Gotowe. Przywrócony świat: {0} postaci, najwyższy poziom {1}." "Done. Restored world: {0} characters, highest level {1}.") -f $result.Players, $result.MaxLevel) -ForegroundColor Green
    Write-Host ((UI-Text "  Kopia poprzedniego świata: {0}" "  Backup of the previous world: {0}") -f $result.Safety) -ForegroundColor Gray
    Write-Host (UI-Text 'Kliknij GRAJ, aby uruchomić serwer z przywróconym światem.' 'Click PLAY to start the server with the restored world.') -ForegroundColor Green
}

function Test-RatePercent {
    param([int]$Value)
    return ($Value -ge 1 -and $Value -le 10000)
}

function Set-FreshWorldSettings {
    <#
      .SYNOPSIS
        The rates a world about to be made starts on, and whether its bots wait.

      .DESCRIPTION
        Both are read by the migrator before the cores come up, and only for a
        world whose event flags do not exist yet - a world already set from the
        panel is never touched by .env, so this is asked where a fresh world is
        about to be made and nowhere else.

        The panel used to promise 650% experience on a world the game ran at
        100%, and the first press of its button - field untouched - was what
        made the promise real. That is the window NerrVoVy asked us to close
        (20 September): "zanim sie zmieni ustawienia to juz cos sie tam
        podzieje".

        Non-interactive when the numbers come in as parameters or -Yes is set,
        which is how the GUI calls every action; the console path asks.
    #>
    param([string]$Reason = (UI-Text 'nowego świata' 'of the new world'))

    $exp = $RateExp
    $drop = $RateDrop
    $yang = $RateYang
    $hold = $HoldBots
    $starter = $StarterChest
    $interactive = (-not $Yes) -and $exp -lt 0 -and $drop -lt 0 -and $yang -lt 0 -and $hold -lt 0 -and $starter -lt 0
    if ($interactive) {
        Write-Host ''
        Write-Host (UI-Text "Ustawienia $Reason - wchodzą w życie, zanim pojawi się pierwszy bot:" "Settings $Reason - they take effect before the first bot appears:") -ForegroundColor Cyan
        Write-Host (UI-Text ' 1. Normalnie      - 100% doświadczenia, 100% dropu, 100% yang (tak, jak gra została stworzona)' ' 1. Normal         - 100% experience, 100% drops, 100% yang (the way the game was made)')
        Write-Host (UI-Text ' 2. Spokojnie      - 300% / 200% / 200%' ' 2. Relaxed        - 300% / 200% / 200%')
        Write-Host (UI-Text ' 3. Szybko         - 1000% / 500% / 500%' ' 3. Fast           - 1000% / 500% / 500%')
        Write-Host (UI-Text ' 4. Własne liczby' ' 4. Your own numbers')
        Write-Host (UI-Text ' 5. Nie zmieniaj   - zostaw to, co jest w .env' ' 5. No change      - keep what .env says')
        $answer = Read-Host (UI-Text 'Wybierz (1-5)' 'Choose (1-5)')
        switch ($answer) {
            '1' { $exp = 100;  $drop = 100; $yang = 100 }
            '2' { $exp = 300;  $drop = 200; $yang = 200 }
            '3' { $exp = 1000; $drop = 500; $yang = 500 }
            '4' {
                $exp = [int](Read-Host (UI-Text 'Doświadczenie w procentach (100 = normalnie)' 'Experience in percent (100 = normal)'))
                $drop = [int](Read-Host (UI-Text 'Drop przedmiotów w procentach' 'Item drops in percent'))
                $yang = [int](Read-Host (UI-Text 'Yang w procentach' 'Yang in percent'))
            }
            default { $exp = -1; $drop = -1; $yang = -1 }
        }
        Write-Host ''
        Write-Host (UI-Text 'Boty mogą poczekać przy drzwiach, żeby dało się spokojnie ustawić resztę:' 'The bots can wait at the door, so that the rest can be set up in peace:') -ForegroundColor Cyan
        if (Confirm-Operation (UI-Text 'Wstrzymać boty po starcie (wpuścisz je przyciskiem w panelu)?' 'Hold the bots after the start (you let them in with a button in the panel)?')) {
            $hold = 1
        }
        else {
            $hold = 0
        }
        Write-Host ''
        Write-Host (UI-Text 'Skrzynia Ucznia to zestaw skrzyń, który prowadzi postać przez pierwsze wioski - jeden przełącznik dla graczy i botów:' 'The Apprentice Chest is a set of chests that takes a character through the first villages - one switch for players and bots:') -ForegroundColor Cyan
        # Enter keeps what the world had. A [t/N] question switched the chest off
        # for everybody who pressed Enter, and the window ticked it back on for
        # everybody who had it off - either way a wipe changed it unasked.
        $starterNow = (Get-DotEnvValue -Key 'M2_STARTER_CHEST' -Default '1') -ne '0'
        $answer = Read-Host (UI-Text "Skrzynia Ucznia w grze - dla nowych postaci graczy i dla botów? (T/n, Enter = $(if ($starterNow) { 'tak' } else { 'nie' }))" "Apprentice Chest in the game - for players' new characters and for the bots? (Y/n, Enter = $(if ($starterNow) { 'yes' } else { 'no' }))")
        if ("$answer".Trim()) { $starterNow = "$answer".Trim().ToLowerInvariant() -notin @('n', 'nie', 'no', '0') }
        $starter = $(if ($starterNow) { 1 } else { 0 })
    }

    $written = @()
    foreach ($pair in @(
            @{ Key = 'M2_RATE_EXP';  Value = $exp;  Label = (UI-Text 'doświadczenie' 'experience') },
            @{ Key = 'M2_RATE_DROP'; Value = $drop; Label = 'drop' },
            @{ Key = 'M2_RATE_YANG'; Value = $yang; Label = 'yang' })) {
        $v = [int]$pair.Value
        if ($v -lt 0) { continue }
        if (-not (Test-RatePercent -Value $v)) {
            throw ((UI-Text "{0}: podaj całe procenty od 1 do 10000, nie '{1}'." "{0}: give a whole percentage from 1 to 10000, not '{1}'.") -f $pair.Label, $v)
        }
        Set-DotEnvValue -Key $pair.Key -Value "$v"
        $written += ('{0} {1}%' -f $pair.Label, $v)
    }
    if ($hold -ge 0) {
        $heldValue = $(if ($hold -ge 1) { '1' } else { '0' })
        Set-DotEnvValue -Key 'M2_PLAYERBOT_START_HELD' -Value $heldValue
        $written += $(if ($heldValue -eq '1') { (UI-Text 'boty czekają na wpuszczenie' 'the bots wait to be let in') } else { (UI-Text 'boty wchodzą od razu' 'the bots come in at once') })
    }
    if ($starter -ge 0) {
        $starterValue = $(if ($starter -ge 1) { '1' } else { '0' })
        Set-DotEnvValue -Key 'M2_STARTER_CHEST' -Value $starterValue
        $written += $(if ($starterValue -eq '1') { (UI-Text 'Skrzynia Ucznia w grze' 'the Apprentice Chest in the game') } else { (UI-Text 'bez Skrzyni Ucznia (ani dla graczy, ani dla botów)' 'no Apprentice Chest (for neither players nor bots)') })
    }
    if ($written.Count -gt 0) {
        Write-Host ((UI-Text 'Zapisano: ' 'Saved: ') + ($written -join ', ') + '.') -ForegroundColor Green
    }
}

function Reset-WorldAction {
    # "Zacznij od zera": the world a fresh install starts with, with the old one
    # kept as a zip. The volume is deleted, because that is the only thing that
    # makes MariaDB import initdb.d again.
    if (-not (Test-M2DockerRunning)) {
        Write-Host (UI-Text 'Silnik Dockera jest zatrzymany. Uruchom Docker i spróbuj ponownie.' 'The Docker engine is stopped. Start Docker and try again.') -ForegroundColor Yellow
        return
    }
    $target = Get-CurrentInstallTargetVolume
    if (-not $target) {
        Write-Host (UI-Text 'Nie mozna ustalic bazy tej instalacji.' 'Cannot tell which database is this installation''s.') -ForegroundColor Yellow
        return
    }
    $missing = @(Get-M2MissingSqlDumps -ServerRoot $serverRoot)
    if ($missing.Count -gt 0) {
        Write-Host (UI-Text 'Nie mogę zresetować świata: brakuje zrzutów, z których powstaje nowa baza.' 'Cannot reset the world: the dumps a new database is made from are missing.') -ForegroundColor Red
        Write-Host ((UI-Text '  Brakuje: ' '  Missing: ') + ($missing -join ', ')) -ForegroundColor Red
        Write-Host (UI-Text '  Miejsce: linux-port\docker\mariadb\initdb.d\dumps' '  Place: linux-port\docker\mariadb\initdb.d\dumps') -ForegroundColor Gray
        Write-Host (UI-Text 'Bez nich skasowanie bazy zostawiłoby instalację bez świata i bez sposobu na nowy.' 'Without them, deleting the database would leave the installation with no world and no way to make a new one.') -ForegroundColor Gray
        return
    }
    Write-Host ''
    Write-Host (UI-Text 'UWAGA: to kasuje CAŁY obecny świat - postacie, poziomy, ekwipunek, boty, konta gry.' 'WARNING: this deletes the WHOLE current world - characters, levels, equipment, bots, game accounts.') -ForegroundColor Yellow
    Write-Host (UI-Text 'Przed skasowaniem świat zostanie zapisany do kopii zip w folderze backups,' 'Before it is deleted, the world is saved to a zip backup in the backups folder,') -ForegroundColor Yellow
    Write-Host (UI-Text 'więc da się do niego wrócić akcją "Przywróć kopię".' 'so you can go back to it with the "Restore from a backup" action.') -ForegroundColor Yellow
    Write-Host (UI-Text 'Po resecie pierwszy start potrwa dłużej: baza powstaje od nowa i boty są zasiewane.' 'After the reset the first start takes longer: the database is made anew and the bots are seeded.') -ForegroundColor Gray
    if (-not (Confirm-Operation (UI-Text 'Zresetować świat do stanu świeżej instalacji?' 'Reset the world to a fresh installation?'))) {
        Write-Host (UI-Text 'Anulowano.' 'Cancelled.') -ForegroundColor Yellow; return
    }
    Set-FreshWorldSettings -Reason (UI-Text 'nowego świata' 'of the new world')
    Write-Host (UI-Text 'Zatrzymuję serwer i Dockera po stronie stosu...' 'Stopping the server and the stack''s containers...') -ForegroundColor Cyan
    Stop-Server
    Write-Host (UI-Text 'Zapisuję kopię i kasuję bazę...' 'Saving the backup and deleting the database...') -ForegroundColor Cyan
    $result = Reset-M2WorldToFreshInstall -Volume $target -ServerRoot $serverRoot `
        -BackupRoot (Join-Path $serverRoot 'backups')
    if ($result.Backup) {
        Write-Host ((UI-Text "Kopia poprzedniego świata ({0} postaci): {1}" "Backup of the previous world ({0} characters): {1}") -f $result.Players, $result.Backup) -ForegroundColor Gray
    }
    if ($ThenStart) {
        Write-Host (UI-Text 'Świat skasowany. Uruchamiam serwer z nowym światem - baza powstaje od nowa i boty są zasiewane, to potrwa dłużej niż zwykły start.' 'World deleted. Starting the server with the new world - the database is made anew and the bots are seeded, which takes longer than a normal start.') -ForegroundColor Green
        Start-Server
        return
    }
    Write-Host (UI-Text 'Świat skasowany. Kliknij GRAJ - serwer zbuduje bazę od nowa i zasieje boty.' 'World deleted. Click PLAY - the server makes the database anew and seeds the bots.') -ForegroundColor Green
}

function Reset-PanelPasswordAction {
    # The panel keeps a PBKDF2 hash of its passphrase in m2panel.conf, on a
    # volume of its own, and its entrypoint never regenerates it - regenerating
    # would log every operator out and invalidate every session cookie. Right,
    # except when the passphrase it hashed is one nobody has: the container
    # invented it on a first run and printed it to a log nobody read.
    #
    # Deleting that one file is the whole reset. The entrypoint then rebuilds it
    # from M2_PANEL_PASSWORD, which the launcher now guarantees is in .env.
    $creds = Get-InstallDbCredentials
    $panelPw = ''
    if ($creds.EnvPath) {
        $text = [IO.File]::ReadAllText($creds.EnvPath)
        $match = [Regex]::Match($text, '(?m)^M2_PANEL_PASSWORD=(.+?)\s*$')
        if ($match.Success) { $panelPw = $match.Groups[1].Value }
    }
    if (-not $panelPw) {
        Write-Host (UI-Text 'W pliku .env nie ma hasla do panelu. Uruchom raz GRAJ - launcher je uzupelni i pokaze.' 'The .env file has no panel password. Click PLAY once - the launcher fills it in and shows it.') -ForegroundColor Yellow
        return
    }
    Write-Host (UI-Text 'Haslo do panelu WWW (z pliku linux-port\docker\.env):' 'Web panel password (from linux-port\docker\.env):') -ForegroundColor Cyan
    Write-Host "  $panelPw"
    Write-Host ''
    Write-Host (UI-Text 'Jesli panel go nie przyjmuje, znaczy to, ze zapamietal starsze haslo.' 'If the panel does not take it, the panel remembers an older password.') -ForegroundColor Gray
    Write-Host (UI-Text 'Reset kasuje jeden plik konfiguracyjny panelu; swiat, postacie i boty' 'The reset deletes one configuration file of the panel; the world, the characters and the bots') -ForegroundColor Gray
    Write-Host (UI-Text 'sa w bazie i nie sa tym ruszane. Wylogowuje otwarte sesje panelu.' 'are in the database and are not touched. It logs out the panel''s open sessions.') -ForegroundColor Gray
    if (-not (Confirm-Operation (UI-Text 'Zresetowac haslo panelu do tego z .env?' 'Reset the panel password to the one in .env?'))) {
        Write-Host (UI-Text 'Anulowano - haslo wyzej pozostaje aktualne.' 'Cancelled - the password above is still the valid one.') -ForegroundColor Yellow
        return
    }
    # The panel's config volume is named after the same project as the database
    # volume, which the launcher already knows how to find.
    $dbVolume = Get-CurrentInstallTargetVolume
    if (-not $dbVolume -or -not $dbVolume.EndsWith('_db-data')) {
        Write-Host (UI-Text 'Nie moge ustalic nazwy projektu tej instalacji. Uruchom raz GRAJ.' 'Cannot tell this installation''s project name. Click PLAY once.') -ForegroundColor Yellow
        return
    }
    $volume = $dbVolume.Substring(0, $dbVolume.Length - '_db-data'.Length) + '_panel-conf'

    $composeDir = Join-Path $serverRoot 'linux-port\docker'
    $composeFile = Join-Path $composeDir 'docker-compose.yml'
    $previousPreference = $ErrorActionPreference
    try {
        # docker compose writes progress to stderr; under 'Stop' that is a
        # terminating error even when the command worked. See Stop-Server.
        $ErrorActionPreference = 'Continue'
        Write-Host (UI-Text 'Zatrzymuje panel...' 'Stopping the panel...') -ForegroundColor Cyan
        docker compose --project-directory $composeDir -f $composeFile stop panel 2>&1 | Out-Null
        Write-Host (UI-Text 'Kasuje zapamietane haslo...' 'Deleting the remembered password...') -ForegroundColor Cyan
        docker run --rm -v "${volume}:/etc/m2panel" alpine:3.20 rm -f /etc/m2panel/m2panel.conf 2>&1 | Out-Null
        $removeExit = $LASTEXITCODE
        if ($removeExit -ne 0) {
            Write-Host (UI-Text "Nie udalo sie skasowac pliku (kod $removeExit). Panel zostaje bez zmian." "Could not delete the file (code $removeExit). The panel stays as it was.") -ForegroundColor Red
            docker compose --project-directory $composeDir -f $composeFile start panel 2>&1 | Out-Null
            return
        }
        Write-Host (UI-Text 'Uruchamiam panel...' 'Starting the panel...') -ForegroundColor Cyan
        docker compose --project-directory $composeDir -f $composeFile up -d --no-deps panel 2>&1 | Out-Null
    }
    finally { $ErrorActionPreference = $previousPreference }

    Write-Host ''
    Write-Host (UI-Text 'Gotowe. Zaloguj sie haslem:' 'Done. Log in with the web panel password:') -ForegroundColor Green
    Write-Host "  $panelPw"
}

function Get-InstallDbCredentials {
    # Everything a database client needs, straight from .env: the port the
    # compose file publishes on 127.0.0.1, the game account and root. The root
    # password is what MariaDB was initialised with, and what Repair-DatabaseAction
    # puts back on root@'%' when the two have drifted apart.
    $result = [pscustomobject]@{ User = 'metin2'; Password = ''; RootPassword = ''; Port = '3306'; EnvPath = '' }
    $envPath = Join-Path $serverRoot 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $result.EnvPath = $envPath
        $text = [IO.File]::ReadAllText($envPath)
        $userMatch = [Regex]::Match($text, '(?m)^M2_DB_USER=(.+?)\s*$')
        if ($userMatch.Success) { $result.User = $userMatch.Groups[1].Value }
        $passMatch = [Regex]::Match($text, '(?m)^M2_DB_PASSWORD=(.+?)\s*$')
        if ($passMatch.Success) { $result.Password = $passMatch.Groups[1].Value }
        $rootMatch = [Regex]::Match($text, '(?m)^M2_DB_ROOT_PASSWORD=(.+?)\s*$')
        if ($rootMatch.Success) { $result.RootPassword = $rootMatch.Groups[1].Value }
        $portMatch = [Regex]::Match($text, '(?m)^M2_DB_PUBLISH_PORT=(\d+)\s*$')
        if ($portMatch.Success) { $result.Port = $portMatch.Groups[1].Value }
    }
    return $result
}

function Show-DatabaseAccessAction {
    # Where a database client (Navicat, HeidiSQL, DBeaver) connects, and with
    # which accounts. The passwords are not printed: this output lands in the
    # launcher log, and the launcher log lands in support bundles that get
    # posted on the Discord. The GUI shows them in a dialog of its own; here
    # the .env is opened in Notepad instead.
    $creds = Get-InstallDbCredentials
    if (-not $creds.EnvPath) {
        Write-Host (UI-Text 'Brak pliku linux-port\docker\.env — uruchom najpierw serwer (GRAJ), launcher go utworzy.' 'No linux-port\docker\.env yet - start the server (PLAY) first, the launcher creates it.') -ForegroundColor Yellow
        return
    }
    Write-Host (UI-Text 'Dane do połączenia z bazą (Navicat, HeidiSQL, DBeaver — typ MySQL/MariaDB):' 'Database connection details (Navicat, HeidiSQL, DBeaver - type MySQL/MariaDB):') -ForegroundColor Cyan
    Write-Host '  Host:      127.0.0.1'
    Write-Host "  Port:      $($creds.Port)"
    Write-Host (UI-Text '  Konto 1:   root        — pełny dostęp; hasło: M2_DB_ROOT_PASSWORD w pliku .env' '  Account 1: root        - full access; its password is M2_DB_ROOT_PASSWORD in the .env file')
    Write-Host (UI-Text "  Konto 2:   $($creds.User)      — tylko bazy gry; hasło: M2_DB_PASSWORD w pliku .env" "  Account 2: $($creds.User)      - the game databases only; its password is M2_DB_PASSWORD in the .env file")
    Write-Host (UI-Text "  Plik .env: $($creds.EnvPath)" "  .env file: $($creds.EnvPath)")
    Write-Host ''
    Write-Host (UI-Text 'Baza słucha tylko na tym komputerze (127.0.0.1), więc klient musi działać na nim.' 'The database listens on this computer only (127.0.0.1), so the database client has to run on it.') -ForegroundColor Gray
    if ((Get-M2ServerEngine -ServerRoot $serverRoot) -ne 'r40250') {
        Write-Host (UI-Text 'Na plikach 2.x przedmioty i potwory (item_proto, mob_proto) są w bazie world; player.item_proto' 'On the 2.x files items and monsters (item_proto, mob_proto) live in the world database; player.item_proto') -ForegroundColor Gray
        Write-Host (UI-Text 'i player.mob_proto to tylko widoki. Zmiany w world zostają po restarcie serwera.' 'and player.mob_proto are only views. Changes in world survive a server restart.') -ForegroundColor Gray
    }
    Write-Host (UI-Text 'Jeśli baza odrzuca hasło z .env („Access denied"), użyj akcji RepairDb (przycisk' 'If the database rejects the password from .env ("Access denied"), use the RepairDb action (the') -ForegroundColor Gray
    Write-Host (UI-Text '„NAPRAW DOSTĘP DO BAZY"): ustawia konta root i metin2 na hasła z tego pliku.' 'REPAIR DATABASE ACCESS button): it sets the root and metin2 accounts to the passwords in that file.') -ForegroundColor Gray
    Write-Host (UI-Text 'Nie wklejaj haseł z .env publicznie ani do paczki z logami.' 'Never paste the passwords from .env in public or into a log bundle.') -ForegroundColor Yellow
    if (-not $Yes) {
        $answer = Read-Host (UI-Text 'Otworzyć plik .env w Notatniku, żeby skopiować hasła? [t/N]' 'Open the .env file in Notepad to copy the passwords? [y/N]')
        if ($answer -match '^[tTyY]') { Start-Process notepad.exe -ArgumentList ('"' + $creds.EnvPath + '"') }
    }
}

function Repair-DatabaseAction {
    if (-not (Test-M2DockerRunning)) {
        Write-Host (UI-Text 'Silnik Dockera jest zatrzymany, więc nie widać żadnych baz.' 'The Docker engine is stopped, so no database can be seen.') -ForegroundColor Yellow
        Write-Host (UI-Text 'Uruchom Docker (akcja StartDocker lub przycisk „URUCHOM DOCKER") i spróbuj ponownie.' 'Start Docker (the StartDocker action or the START DOCKER button) and try again.') -ForegroundColor Yellow
        Write-Host (UI-Text 'Żadne dane nie zginęły — bazy są na dysku, tylko Docker ich teraz nie pokazuje.' 'No data was lost - the databases are on the disk, Docker just does not show them now.') -ForegroundColor Gray
        return
    }
    $target = Get-CurrentInstallTargetVolume
    if (-not $target) {
        Write-Host (UI-Text 'Nie można ustalić bazy tej instalacji. Uruchom najpierw serwer (GRAJ) choć raz.' 'Cannot tell which database is this installation''s. Start the server (PLAY) once first.') -ForegroundColor Yellow
        return
    }
    $creds = Get-InstallDbCredentials
    if (-not $creds.Password) {
        Write-Host (UI-Text 'Brak M2_DB_PASSWORD w linux-port\docker\.env — nie mam czego przywrócić.' 'No M2_DB_PASSWORD in linux-port\docker\.env - there is nothing to restore.') -ForegroundColor Red
        return
    }
    Write-Host (UI-Text "Naprawiam konta bazy dla instalacji: $target" "Repairing the database accounts of the installation: $target") -ForegroundColor Cyan
    Write-Host (UI-Text 'To odtwarza wyłącznie użytkowników i uprawnienia bazy — konto gry i root — z hasłami z pliku .env. Postacie, przedmioty i boty pozostają bez zmian.' 'This restores only the database''s users and privileges - the game account and root - with the passwords from the .env file. Characters, items and bots stay as they are.') -ForegroundColor Gray
    if (-not $creds.RootPassword) {
        Write-Host (UI-Text 'Brak M2_DB_ROOT_PASSWORD w .env — konto root zostanie pominięte.' 'No M2_DB_ROOT_PASSWORD in .env - the root account is skipped.') -ForegroundColor Yellow
    }
    Write-Host (UI-Text 'Zatrzymuję serwer, aby zwolnić bazę...' 'Stopping the server to free the database...') -ForegroundColor Cyan
    Stop-Server
    if (Repair-M2GameDbUser -Volume $target -DbUser $creds.User -DbPassword $creds.Password -RootPassword $creds.RootPassword) {
        Write-Host (UI-Text 'Gotowe. Konta i uprawnienia bazy odtworzone. Kliknij GRAJ, aby uruchomić serwer.' 'Done. The database accounts and privileges are restored. Click PLAY to start the server.') -ForegroundColor Green
        Write-Host (UI-Text "Do Navicat: host 127.0.0.1, port $($creds.Port), root albo $($creds.User) — hasła z .env (akcja DbAccess pokaże szczegóły)." "For Navicat: host 127.0.0.1, port $($creds.Port), root or $($creds.User) - the passwords from .env (the DbAccess action shows the details).") -ForegroundColor Gray
    }
    else {
        Write-Host (UI-Text 'Naprawa nie powiodła się. Zbierz logi (ZIP) i zgłoś problem.' 'The repair failed. Collect the logs (ZIP) and report the problem.') -ForegroundColor Red
    }
}

function Create-Logs {
    $preflightLog = Join-Path $serverRoot 'launcher-logs\preflight-last.log'
    New-Item -ItemType Directory -Path (Split-Path -Parent $preflightLog) -Force | Out-Null
    $report = Get-M2DockerPreflight -ServerRoot $serverRoot -CheckPanelPort
    [IO.File]::WriteAllText(
        $preflightLog,
        (Format-M2DockerPreflightReport -Report $report),
        [Text.UTF8Encoding]::new($false))
    # Free space and the size of Docker's disk: whether the drive was full is
    # the first question a report of a read-only Docker disk raises, and the
    # bundle could not answer it (pattsito, 23 September).
    $extra = @{}
    try { $extra['disk-space.txt'] = Get-M2DiskSpaceReport -ServerRoot $serverRoot }
    catch { $extra['disk-space.txt'] = (UI-Text "Nie udalo sie odczytac miejsca na dyskach: $($_.Exception.Message)" "Could not read the free space on the drives: $($_.Exception.Message)") }
    $bundle = New-M2SupportBundle -ServerRoot $serverRoot -ExtraFiles $extra
    Write-Host (UI-Text "Gotowa paczka diagnostyczna: $bundle" "Diagnostic bundle ready: $bundle") -ForegroundColor Green
    return $bundle
}

function Send-Logs {
    $config = Get-Config
    $support = Get-M2SupportSettings -Config $config
    if (-not $support.UploadUrl) {
        throw (UI-Text "Kanał zgłoszeń jest teraz niedostępny. Utwórz ZIP akcją Logs i dołącz go ręcznie do zgłoszenia: $($support.ContactUrl)" "The report channel is not available now. Make the ZIP with the Logs action and attach it to a report yourself: $($support.ContactUrl)")
    }
    $bundle = Create-Logs
    Write-Host (UI-Text 'Paczka zawiera logi Dockera i konfigurację z usuniętymi hasłami.' 'The bundle holds Docker''s logs and the configuration with the passwords removed.') -ForegroundColor Yellow
    $target = if ($support.Source -eq 'manifest') { (UI-Text 'kanału zgłoszeń autora' 'the author''s report channel') } else { $support.UploadUrl }
    if (-not (Confirm-Operation (UI-Text "Wysłać $bundle do $target?" "Send $bundle to $target?"))) {
        Write-Host (UI-Text 'Nie wysłano. ZIP pozostał na dysku.' 'Not sent. The ZIP stays on the disk.') -ForegroundColor Yellow
        return
    }
    $response = Send-M2SupportBundle -BundlePath $bundle -UploadUrl $support.UploadUrl
    if ($response) { Write-Host (UI-Text "Wysłano. Odpowiedź serwera: $response" "Sent. The server answered: $response") -ForegroundColor Green }
    else { Write-Host (UI-Text 'Wysłano paczkę diagnostyczną.' 'The diagnostic bundle is sent.') -ForegroundColor Green }
}

# ---------------------------------------------------------------- co-op
# Playing the host's world with friends over the Internet (experimental;
# launcher\Metin2Launcher.Coop.psm1 does the work). The window's COOP dialog
# runs CoopHost, CoopStop and CoopCheck through here and does the rest itself:
# anything that prints a password is for the console only, because the
# window's action output is a file under launcher-logs, which the support
# bundle collects.

function Assert-CoopModule {
    if (-not (Get-Command Get-M2CoopNetworkReport -ErrorAction SilentlyContinue)) {
        throw (UI-Text 'Brak modułu launcher\Metin2Launcher.Coop.psm1 - ta paczka nie ma trybu COOP.' 'The launcher\Metin2Launcher.Coop.psm1 module is missing - this package has no COOP mode.')
    }
}

function Write-CoopNetworkReport {
    param($Report)
    Write-Host ((UI-Text "Karta sieciowa: {0} ({1}), brama {2}" "Network adapter: {0} ({1}), gateway {2}") -f $Report.LanAddress, $Report.Interface, $Report.Gateway)
    Write-Host ((UI-Text "Adres widziany z internetu: {0}" "Address the Internet sees: {0}") -f $(if ($Report.PublicAddress) { $Report.PublicAddress } else { (UI-Text 'nie odczytano' 'not read') }))
    if ($Report.Router) { Write-Host ((UI-Text "Router (UPnP): {0}, adres WAN {1}" "Router (UPnP): {0}, WAN address {1}") -f $Report.Router, $Report.RouterWan) }
    else { Write-Host (UI-Text 'Router: nie odpowiedział na UPnP' 'Router: did not answer UPnP') }
    $color = $(if ($Report.Verdict -eq 'public') { 'Green' } elseif (@('no-upnp', 'mismatch', 'no-wan') -contains $Report.Verdict) { 'Yellow' } else { 'Red' })
    Write-Host ((UI-Text "Wynik: {0}" "Result: {0}") -f $Report.Text) -ForegroundColor $color
    $vpns = @($Report.Vpns)
    foreach ($vpn in $vpns) { Write-Host ((UI-Text "Sieć VPN: {0}, adres {1} (karta {2})" "VPN: {0}, address {1} (adapter {2})") -f $vpn.Name, $vpn.Address, $vpn.Interface) }
    if ($vpns.Count -eq 0) { Write-Host (UI-Text 'Sieć VPN: nie wykryto (Radmin VPN, Tailscale, ZeroTier, Hamachi).' 'VPN: none found (Radmin VPN, Tailscale, ZeroTier, Hamachi).') }
}

function Get-CoopHostingField {
    # One field of the hosting record, '' when the record or the field is not
    # there (a state file written before a field existed). Under StrictMode a
    # missing property is an error, not an empty value.
    param($Hosting, [Parameter(Mandatory = $true)][string]$Name)
    if (-not $Hosting) { return '' }
    if (-not (@($Hosting.PSObject.Properties.Name) -contains $Name)) { return '' }
    return [string]$Hosting.$Name
}

function Show-CoopCheckAction {
    Assert-CoopModule
    Write-Phase (UI-Text 'sprawdzanie sieci' 'checking the network')
    $report = Get-M2CoopNetworkReport
    Write-CoopNetworkReport -Report $report
    if (@('cgnat', 'double-nat') -contains $report.Verdict) {
        $vpns = @($report.Vpns)
        if ($vpns.Count -gt 0) { Write-Host ((UI-Text "Rozwiązanie: hostuj przez {0} - HOSTUJ ŚWIAT wybierze go sam." "The way out: host through {0} - HOST THE WORLD picks it by itself.") -f $vpns[0].Name) -ForegroundColor Yellow }
        else {
            Write-Host ((UI-Text 'Rozwiązanie: zainstaluj Radmin VPN albo Tailscale, połącz się ze znajomymi w jednej sieci i hostuj ponownie - ' 'The way out: install Radmin VPN or Tailscale, join your friends in one network and host again - ') +
                (UI-Text 'launcher wykryje VPN i użyje go zamiast routera.' 'the launcher finds the VPN and uses it instead of the router.')) -ForegroundColor Yellow
        }
    }
    $ports = Get-M2CoopGamePorts -ServerRoot $serverRoot
    if ($report.Verdict -eq 'no-wan') {
        $vpns = @($report.Vpns)
        if ($vpns.Count -gt 0) { Write-Host ((UI-Text "Masz {0} - hostuj przez niego (HOSTUJ ŚWIAT wybierze go sam, gdy router nie otworzy portów)." "You have {0} - host through it (HOST THE WORLD picks it by itself when the router opens no ports).") -f $vpns[0].Name) -ForegroundColor Yellow }
        foreach ($line in @(Get-M2CoopRouterHelp -Router $report.Router -LanAddress $report.LanAddress -Ports $ports)) { Write-Host $line -ForegroundColor Yellow }
    }
    Write-Host ((UI-Text "Porty gry: {0}" "Game ports: {0}") -f ($ports -join ', '))
    $bindings = Get-M2CoopGameBindings -ServerRoot $serverRoot
    if (-not $bindings.Running) { Write-Host (UI-Text 'Serwer gry nie działa (brak opublikowanych portów).' 'The game server is not running (no published ports).') -ForegroundColor Yellow }
    elseif ($bindings.Public) { Write-Host (UI-Text 'Porty gry są otwarte na wszystkich kartach sieciowych - świat jest hostowany.' 'The game ports are open on every network adapter - the world is hosted.') -ForegroundColor Green }
    else { Write-Host (UI-Text 'Porty gry słuchają tylko lokalnie (127.0.0.1) - świat nie jest hostowany.' 'The game ports listen locally only (127.0.0.1) - the world is not hosted.') }
    $hostingState = (Read-M2CoopState -ServerRoot $serverRoot).hosting
    if ((Get-CoopHostingField $hostingState 'mode') -eq 'vpn') {
        Write-Host ((UI-Text "Ostatnie hostowanie: przez {0}, adres dla znajomych {1}." "Last hosting: through {0}, address for friends {1}.") -f (Get-CoopHostingField $hostingState 'vpnName'), (Get-CoopHostingField $hostingState 'friendAddress'))
    }
    if ($report.GatewayInfo) {
        foreach ($port in $ports) {
            $m = Get-M2CoopPortMapping -Gateway $report.GatewayInfo -Port $port
            if ($m) { Write-Host ("  router: port {0} -> {1}:{2} ({3})" -f $port, $m.InternalClient, $m.InternalPort, $m.Description) }
            else { Write-Host ((UI-Text "  router: port {0} bez przekierowania" "  router: port {0} not forwarded") -f $port) }
        }
    }
    Write-Host ((UI-Text "Reguła zapory Windows dla portów gry: {0}" "Windows Firewall rule for the game ports: {0}") -f $(if (Test-M2CoopFirewallRule) { (UI-Text 'jest' 'present') } else { (UI-Text 'brak (doda ją Hostuj)' 'missing (HOST THE WORLD adds it)') }))
    foreach ($block in @(Get-M2CoopFirewallBlocks)) {
        Write-Host ((UI-Text "  UWAGA: zapora blokuje program {0} (reguła '{1}', profil {2}) - taka reguła wygrywa z każdą regułą zezwalającą." "  WARNING: the firewall blocks the program {0} (rule '{1}', profile {2}) - such a rule beats every rule that allows.") -f $block.Program, $block.Name, $block.Profile) -ForegroundColor Yellow
    }
    try {
        $defaults = @(Get-M2CoopDefaultPasswordAccounts -ServerRoot $serverRoot)
        if ($defaults.Count -gt 0) { Write-Host ((UI-Text "Konta z hasłem z paczki: {0} - przed hostowaniem użyj 'Zabezpiecz konta'." "Accounts with the package's password: {0} - use 'Secure the accounts' before hosting.") -f ($defaults -join ', ')) -ForegroundColor Yellow }
        else { Write-Host (UI-Text 'Konta admin i test nie mają haseł z paczki.' 'The admin and test accounts do not have the package''s passwords.') -ForegroundColor Green }
    }
    catch { Write-Host (UI-Text "Baza nie odpowiada: $($_.Exception.Message)" "The database does not answer: $($_.Exception.Message)") -ForegroundColor Yellow }
    $state = Read-M2CoopState -ServerRoot $serverRoot
    Write-Host ((UI-Text "Znajomi: {0}" "Friends: {0}") -f @($state.friends).Count)
    foreach ($f in @($state.friends)) {
        Write-Host ("  {0}: login {1}{2}" -f $f.name, $f.login, $(if ($f.blocked) { (UI-Text ' (zablokowany)' ' (blocked)') } else { '' }))
    }
}

function Protect-CoopAccountsAction {
    Assert-CoopModule
    $changed = Protect-M2CoopAccounts -ServerRoot $serverRoot
    $names = @($changed.PSObject.Properties | ForEach-Object { $_.Name })
    if ($names.Count -eq 0) {
        Write-Host (UI-Text 'Konta admin i test nie mają haseł z paczki - nic do zmiany.' 'The admin and test accounts do not have the package''s passwords - nothing to change.') -ForegroundColor Green
        return
    }
    foreach ($name in $names) {
        Write-Host ((UI-Text "Nowe hasło konta {0}: {1}" "Account {0}, new password: {1}") -f $name, $changed.$name) -ForegroundColor Yellow
    }
    Write-Host (UI-Text 'Zapisz je - od teraz logujesz się nimi (okno COOP w launcherze też je pokazuje).' 'Write them down - you log in with them from now on (the launcher''s COOP window shows them too).')
}

function Add-CoopFriendAction {
    Assert-CoopModule
    $name = $FriendName
    if (-not $name) { $name = Read-Host (UI-Text 'Imię albo nick znajomego' 'Your friend''s name or nick') }
    if (-not $name) { throw (UI-Text 'Nie podano imienia znajomego.' 'No friend''s name was given.') }
    $friend = New-M2CoopFriend -ServerRoot $serverRoot -Name $name
    Write-Host ((UI-Text "Konto dla {0}: login {1}, hasło {2}, kod usuwania postaci {3}" "Account for {0}: login {1}, password: {2}, character deletion code {3}") -f $friend.name, $friend.login, $friend.password, $friend.socialId) -ForegroundColor Green
    $target = Get-M2CoopInviteTarget -ServerRoot $serverRoot
    if ($target.Address) {
        Write-Host (UI-Text 'Kod zaproszenia (skopiuj i wyślij znajomemu):' 'Invite code (copy it and send it to your friend):')
        Write-Host (Get-M2CoopFriendInvite -ServerRoot $serverRoot -Friend $friend -HostAddress $target.Address -Vpn $target.Vpn -Lan $target.Lan) -ForegroundColor Cyan
        if ($target.Vpn) { Write-Host ((UI-Text "Znajomy musi być w Twojej sieci {0} - kod prowadzi na adres {1}." "Your friend has to be in your {0} network - the code leads to the address {1}.") -f $target.VpnName, $target.Address) -ForegroundColor Yellow }
    }
}

function Set-CoopFriendBlockedAction {
    param([bool]$Blocked = $true)
    Assert-CoopModule
    $login = $FriendLogin
    if (-not $login) { $login = Read-Host (UI-Text 'Login znajomego' 'Your friend''s login') }
    if (-not $login) { throw (UI-Text 'Nie podano loginu.' 'No login was given.') }
    Set-M2CoopFriendBlocked -ServerRoot $serverRoot -Login $login -Blocked $Blocked
    if ($Blocked) { Write-Host (UI-Text "Konto $login zablokowane: nie zaloguje się, dopóki go nie odblokujesz." "Account $login blocked: it cannot log in until you unblock it.") -ForegroundColor Green }
    else { Write-Host (UI-Text "Konto $login odblokowane." "Account $login unblocked.") -ForegroundColor Green }
}

function Show-CoopInviteAction {
    Assert-CoopModule
    $state = Read-M2CoopState -ServerRoot $serverRoot
    $target = Get-M2CoopInviteTarget -ServerRoot $serverRoot
    if (-not $target.Address) {
        if ($target.Vpn) { throw ((UI-Text "Nie udało się odczytać adresu {0} - uruchom go i spróbuj jeszcze raz." "Could not read the {0} address - start it and try again.") -f $target.VpnName) }
        throw (UI-Text 'Nie udało się odczytać adresu publicznego (brak internetu?).' 'Could not read the public address (no Internet?).')
    }
    $shown = 0
    foreach ($f in @($state.friends)) {
        if ($FriendLogin -and [string]$f.login -ne $FriendLogin) { continue }
        if ($f.blocked) { continue }
        Write-Host ((UI-Text "{0} (login {1}, hasło {2}):" "{0} (login {1}, password: {2}):") -f $f.name, $f.login, $f.password)
        Write-Host (Get-M2CoopFriendInvite -ServerRoot $serverRoot -Friend $f -HostAddress $target.Address -Vpn $target.Vpn -Lan $target.Lan) -ForegroundColor Cyan
        $shown++
    }
    if ($shown -eq 0) { Write-Host (UI-Text 'Brak znajomych - dodaj ich najpierw.' 'No friends yet - add them first.') -ForegroundColor Yellow }
    elseif ($target.Vpn) { Write-Host ((UI-Text "Kody prowadzą na adres {0} w sieci {1} - znajomi muszą być w tej sieci." "The codes lead to the address {0} in the {1} network - your friends have to be in that network.") -f $target.Address, $target.VpnName) -ForegroundColor Yellow }
}

function Invoke-CoopGameRecreate {
    # Docker cannot move a running container's published ports, so the game
    # container is recreated with the new address - every core restarts, about
    # a minute. The panels stay on M2_PANEL_BIND_ADDRESS, written out as
    # 127.0.0.1 first if it was empty, so they never follow the game outwards.
    param([Parameter(Mandatory = $true)][string]$BindAddress)
    if (-not (Get-DotEnvValue -Key 'M2_PANEL_BIND_ADDRESS')) { Set-DotEnvValue -Key 'M2_PANEL_BIND_ADDRESS' -Value '127.0.0.1' }
    Set-DotEnvValue -Key 'M2_HOST_BIND_ADDRESS' -Value $BindAddress
    $composeDir = Join-Path $serverRoot 'linux-port\docker'
    $composeFile = Join-Path $composeDir 'docker-compose.yml'
    # compose writes its progress to stderr, which 'Stop' would turn into a
    # failure; the exit code decides (the same shape as Stop-Server).
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        docker compose --project-directory $composeDir -f $composeFile up -d --no-deps game
        $exit = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previousPreference }
    if ($exit -ne 0) { throw (UI-Text "docker compose up game zakończył się kodem $exit." "docker compose up game ended with code $exit.") }
}

function Test-CoopCoreAnswers {
    # A core is up when it sends its handshake. A connection alone proves
    # nothing: Docker Desktop's port proxy accepts one before anything inside
    # the container listens and then closes it, so "connected" came back
    # eleven seconds into a boot the cores needed forty for.
    param([int]$Port)
    $client = New-Object Net.Sockets.TcpClient
    try {
        $wait = $client.BeginConnect('127.0.0.1', $Port, $null, $null)
        if (-not ($wait.AsyncWaitHandle.WaitOne(2000) -and $client.Connected)) { return $false }
        $client.EndConnect($wait)
        $stream = $client.GetStream()
        $stream.ReadTimeout = 3000
        $buffer = New-Object byte[] 16
        return ($stream.Read($buffer, 0, $buffer.Length) -gt 0)
    }
    catch { return $false }
    finally { $client.Close() }
}

function Wait-CoopGameReady {
    # Every core the client may be sent to has to answer, not only the auth:
    # a friend who logs in while game2 is still booting is dropped at the
    # first map that core hosts.
    param([int]$TimeoutSeconds = 240)
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $pending = New-Object System.Collections.Generic.List[int]
    foreach ($port in @(Get-M2CoopGamePorts -ServerRoot $serverRoot)) { $pending.Add([int]$port) }
    while ((Get-Date) -lt $deadline) {
        foreach ($port in @($pending)) { if (Test-CoopCoreAnswers -Port $port) { [void]$pending.Remove($port) } }
        if ($pending.Count -eq 0) { return $true }
        Start-Sleep -Seconds 3
    }
    Write-Host ((UI-Text "Nie odpowiadają jeszcze porty: {0}" "These ports do not answer yet: {0}") -f ($pending -join ', ')) -ForegroundColor Yellow
    return $false
}

function Start-CoopHostingAction {
    Assert-CoopModule
    Write-Phase (UI-Text 'sprawdzanie sieci' 'checking the network')
    $report = Get-M2CoopNetworkReport
    Write-CoopNetworkReport -Report $report
    if (@('no-lan', 'offline') -contains $report.Verdict) {
        throw (UI-Text 'Ten komputer nie ma połączenia z internetem - hostowanie przerwane, nic nie zmieniono.' 'This computer has no Internet connection - hosting stopped, nothing was changed.')
    }
    # The text menu asks when there is a real choice - a VPN here and an
    # Internet that could work too; the window's button passes its own answer.
    $requested = $CoopVia
    $vpns = @($report.Vpns)
    if ($Action -eq 'Menu' -and $requested -eq 'auto' -and $vpns.Count -gt 0 -and -not (@('cgnat', 'double-nat') -contains $report.Verdict)) {
        if (Confirm-Operation -Question ((UI-Text "Wykryto {0} (adres {1}). Hostować przez VPN zamiast przez internet?" "Found {0} (address {1}). Host through the VPN instead of the Internet?") -f $vpns[0].Name, $vpns[0].Address)) { $requested = $vpns[0].Kind }
        else { $requested = 'internet' }
    }
    $via = Resolve-M2CoopHostingVia -Report $report -Requested $requested
    if ($via.Mode -eq 'blocked') {
        throw ((UI-Text 'Z tej sieci znajomi nie połączą się bezpośrednio (operator albo drugi router nie daje publicznego adresu) - hostowanie przerwane, nic nie zmieniono. ' 'Friends cannot connect straight to this network (the provider or a second router gives it no public address) - hosting stopped, nothing was changed. ') +
            (UI-Text 'Zainstaluj Radmin VPN albo Tailscale, połącz się ze znajomymi w jednej sieci i hostuj ponownie: launcher wykryje VPN i użyje go zamiast routera.' 'Install Radmin VPN or Tailscale, join your friends in one network and host again: the launcher finds the VPN and uses it instead of the router.'))
    }
    if ($via.Mode -eq 'vpn') { Write-Host ((UI-Text "Hostowanie przez {0}, adres {1}." "Hosting through {0}, address {1}.") -f $via.Vpn.Name, $via.Vpn.Address) -ForegroundColor Green }
    $defaults = @(Get-M2CoopDefaultPasswordAccounts -ServerRoot $serverRoot)
    if ($defaults.Count -gt 0) {
        throw ((UI-Text "Konta {0} mają hasła z paczki - każdy w internecie mógłby się na nie zalogować. Najpierw 'Zabezpiecz konta'." "The accounts {0} have the package's passwords - anybody on the Internet could log in to them. First 'Secure the accounts'.") -f ($defaults -join ', '))
    }
    $ports = Get-M2CoopGamePorts -ServerRoot $serverRoot
    $bindings = Get-M2CoopGameBindings -ServerRoot $serverRoot
    if ($bindings.Public) { Write-Host (UI-Text 'Porty gry są już otwarte na wszystkich kartach sieciowych.' 'The game ports are already open on every network adapter.') -ForegroundColor Green }
    else {
        Write-Phase (UI-Text 'porty gry dla sieci (restart serwera gry, około minuty)' 'game ports for the network (the game server restarts, about a minute)')
        Invoke-CoopGameRecreate -BindAddress '0.0.0.0'
        if (Wait-CoopGameReady) { Write-Host (UI-Text 'Serwer gry wstał.' 'The game server is up.') -ForegroundColor Green }
        else { Write-Host (UI-Text 'Serwer gry jeszcze wstaje - znajomi zalogują się za chwilę.' 'The game server is still coming up - friends can log in in a moment.') -ForegroundColor Yellow }
    }
    Write-Host ((UI-Text "Opublikowane: {0}" "Published: {0}") -f ((Get-M2CoopGameBindings -ServerRoot $serverRoot).Lines -join '; '))
    Write-Phase (UI-Text 'zapora Windows' 'Windows Firewall')
    # The window asks for the rule itself before it starts this action
    # (-CoopFirewallAsked): from here, a hidden process, Windows only blinks
    # its question on the taskbar, and xXxDaronxXx's (24 September) went
    # unanswered twice - the second time for two minutes - so nothing outside
    # his PC could reach the world.
    $firewallOk = [bool](Test-M2CoopFirewallRule)
    if ($firewallOk) { Write-Host (UI-Text 'Reguła zapory dla portów gry już jest.' 'The firewall rule for the game ports is already there.') }
    elseif ($CoopFirewallAsked) { Write-Host (UI-Text 'Reguły zapory nie dodano - okno launchera zapytało o nią Windows i nie dostało zgody.' 'The firewall rule was not added - the launcher window asked Windows for it and got no consent.') -ForegroundColor Red }
    else {
        Write-Host (UI-Text 'Windows zapyta o zgodę administratora na regułę zapory dla portów gry - potwierdź (okienko Windows może tylko migać na pasku zadań).' 'Windows asks for an administrator''s consent to the firewall rule for the game ports - confirm it (the Windows prompt may only blink on the taskbar).') -ForegroundColor Yellow
        $firewallOk = [bool](Add-M2CoopFirewallRule -Ports $ports)
        if ($firewallOk) { Write-Host (UI-Text 'Reguła zapory dodana.' 'Firewall rule added.') -ForegroundColor Green }
        else { Write-Host (UI-Text 'Reguły zapory nie dodano (odmowa zgody albo brak odpowiedzi).' 'The firewall rule was not added (consent refused or no answer).') -ForegroundColor Red }
    }
    if (-not $firewallOk) {
        Write-Host (UI-Text 'UWAGA: bez tej reguły zapora Windows może nie wpuścić nikogo spoza tego komputera - ani znajomych z internetu, ani laptopa w tym samym domu. Kliknij HOSTUJ ŚWIAT jeszcze raz i w okienku Windows wybierz "Tak".' 'WARNING: without this rule Windows Firewall may let nobody in from outside this computer - neither friends from the Internet nor a laptop in the same home. Click HOST THE WORLD again and choose "Yes" in the Windows prompt.') -ForegroundColor Red
    }
    foreach ($block in @(Get-M2CoopFirewallBlocks)) {
        Write-Host ((UI-Text "UWAGA: zapora blokuje program {0} (reguła '{1}') - usuń tę regułę w Zaporze Windows, inaczej znajomi się nie połączą." "WARNING: the firewall blocks the program {0} (rule '{1}') - remove that rule in Windows Firewall, or your friends cannot connect.") -f $block.Program, $block.Name) -ForegroundColor Yellow
    }
    $mapped = @()
    $routerRefused = $false
    $state = Read-M2CoopState -ServerRoot $serverRoot
    if ($via.Mode -eq 'vpn') {
        # Nothing is opened in the router, and what hosting over the Internet
        # opened before is closed: through a VPN the world is for the VPN's
        # members and the LAN, not for everybody who scans the address.
        $wasMapped = @()
        if ($state.hosting -and (@($state.hosting.PSObject.Properties.Name) -contains 'mapped')) { $wasMapped = @($state.hosting.mapped) }
        if ($report.GatewayInfo -and $wasMapped.Count -gt 0) {
            Write-Phase (UI-Text 'router: zamykanie portów z hostowania przez internet' 'router: closing the ports of hosting over the Internet')
            foreach ($port in $wasMapped) {
                if (Remove-M2CoopPortMapping -Gateway $report.GatewayInfo -Port ([int]$port) -LanAddress $report.LanAddress) { Write-Host (UI-Text "  port $port zamknięty" "  port $port closed") }
            }
        }
        Write-Host ((UI-Text "W routerze nic nie otwieram - znajomi łączą się przez {0}." "Opening nothing in the router - friends connect through {0}.") -f $via.Vpn.Name)
    }
    elseif ($report.GatewayInfo) {
        Write-Phase (UI-Text 'przekierowania w routerze (UPnP)' 'port forwarding in the router (UPnP)')
        foreach ($port in $ports) {
            $r = Add-M2CoopPortMapping -Gateway $report.GatewayInfo -Port $port -LanAddress $report.LanAddress
            if ($r.Ok) {
                $mapped += $port
                $lease = $(if ($r.Lease -gt 0) { (UI-Text "na $([int]($r.Lease / 3600)) h" "for $([int]($r.Lease / 3600)) h") } else { (UI-Text 'bez terminu' 'with no end') })
                Write-Host ((UI-Text "  port {0}: otwarty ({1})" "  port {0}: open ({1})") -f $port, $lease) -ForegroundColor Green
            }
            else { Write-Host ("  port {0}: {1}" -f $port, $r.Reason) -ForegroundColor Red }
        }
        # Not one port opened: through a VPN on this machine when the way was
        # left to the launcher (Resolve-M2CoopRouterFallback), else said out
        # loud - "Hostowanie włączone" in green over seven refusals is what
        # sent Sudak's friend a code for a world that was offline.
        $fallback = Resolve-M2CoopRouterFallback -Via $via -Requested $requested -Vpns $vpns -Mapped $mapped.Count -Ports @($ports).Count
        if ($fallback.Mode -eq 'vpn' -and $via.Mode -ne 'vpn') {
            $via = $fallback
            Write-Host ((UI-Text "Router nie otworzył żadnego portu - hostuję przez {0}, adres {1}." "The router opened no port - hosting through {0}, address {1}.") -f $via.Vpn.Name, $via.Vpn.Address) -ForegroundColor Yellow
        }
        elseif ($mapped.Count -eq 0) {
            $routerRefused = $true
            Write-Host (UI-Text 'UWAGA: router nie otworzył żadnego portu - znajomi z internetu się nie połączą (serwer będzie dla nich offline), chyba że porty są już przekierowane w routerze ręcznie.' 'WARNING: the router opened no port - friends from the Internet cannot connect (the server is offline for them), unless the ports are already forwarded in the router by hand.') -ForegroundColor Red
            foreach ($line in @(Get-M2CoopRouterHelp -Router $report.Router -LanAddress $report.LanAddress -Ports $ports)) { Write-Host $line -ForegroundColor Yellow }
        }
    }
    else {
        # A router that does not answer the search opens nothing either: the
        # same way out as one that refused every port - a VPN here when the
        # way was left to the launcher, and otherwise said out loud, not
        # "Hostowanie włączone" in green (xXxDaronxXx, 24 September).
        $fallback = Resolve-M2CoopRouterFallback -Via $via -Requested $requested -Vpns $vpns -Mapped 0 -Ports @($ports).Count
        if ($fallback.Mode -eq 'vpn' -and $via.Mode -ne 'vpn') {
            $via = $fallback
            Write-Host ((UI-Text "Router nie odpowiada na UPnP - hostuję przez {0}, adres {1}." "The router does not answer UPnP - hosting through {0}, address {1}.") -f $via.Vpn.Name, $via.Vpn.Address) -ForegroundColor Yellow
        }
        else {
            $routerRefused = $true
            Write-Host (UI-Text 'UWAGA: router nie odpowiada na UPnP, więc launcher nie otworzył w nim żadnego portu - znajomi z internetu się nie połączą, dopóki nie przekierujesz portów ręcznie.' 'WARNING: the router does not answer UPnP, so the launcher opened no port in it - friends from the Internet cannot connect until you forward the ports by hand.') -ForegroundColor Red
            foreach ($line in @(Get-M2CoopRouterHelp -Router '' -LanAddress $report.LanAddress -Ports $ports)) { Write-Host $line -ForegroundColor Yellow }
            if ($vpns.Count -gt 0) {
                Write-Host ((UI-Text "Albo wybierz w oknie COOP połączenie {0} i hostuj jeszcze raz - wtedy router nie jest potrzebny." "Or choose the {0} connection in the COOP window and host again - then no router is needed.") -f $vpns[0].Name) -ForegroundColor Yellow
            }
        }
    }
    $friendAddress = $(if ($via.Mode -eq 'vpn') { $via.Vpn.Address } else { $report.PublicAddress })
    $state.hosting = [pscustomobject]@{
        active = $true; since = (Get-Date).ToString('s'); lanAddress = $report.LanAddress
        publicAddress = $report.PublicAddress; ports = @($ports); mapped = @($mapped)
        mode = $via.Mode; vpn = $(if ($via.Vpn) { $via.Vpn.Kind } else { '' }); vpnName = $(if ($via.Vpn) { $via.Vpn.Name } else { '' })
        friendAddress = $friendAddress
    }
    Save-M2CoopState -ServerRoot $serverRoot -State $state
    Write-Host ''
    if ($via.Mode -eq 'vpn') {
        Write-Host ((UI-Text "Hostowanie włączone przez {0}. Adres dla znajomych: {1}" "Hosting is on, through {0}. Address for friends: {1}") -f $via.Vpn.Name, $friendAddress) -ForegroundColor Green
        Write-Host ((UI-Text "Znajomi muszą dołączyć do Twojej sieci {0}, zanim wkleją kod zaproszenia." "Your friends have to join your {0} network before they paste the invite code.") -f $via.Vpn.Name) -ForegroundColor Yellow
    }
    elseif ($routerRefused) {
        Write-Host ((UI-Text "Hostowanie włączone, ale bez portów w routerze (UWAGA wyżej). Adres dla znajomych: {0}" "Hosting is on, but with no ports in the router (WARNING above). Address for friends: {0}") -f $friendAddress) -ForegroundColor Yellow
    }
    else { Write-Host ((UI-Text "Hostowanie włączone. Adres dla znajomych: {0}" "Hosting is on. Address for friends: {0}") -f $friendAddress) -ForegroundColor Green }
    if ($report.LanAddress) {
        Write-Host ((UI-Text "W tej samej sieci domowej (drugi komputer, laptop na tym samym Wi-Fi) gra łączy się przez {0}, bez routera: kod zaproszenia ma też ten adres, a Dolacz.bat i launcher same go wybiorą." "In the same home network (a second computer, a laptop on the same Wi-Fi) the game connects through {0}, without the router: the invite code carries that address too, and Dolacz.bat and the launcher pick it by themselves.") -f $report.LanAddress)
    }
    Write-Host (UI-Text 'Ty grasz dalej na serwerze 1 (Metin2 SinglePlayer). Kody zaproszeń dla znajomych są w oknie COOP.' 'You go on playing on server 1 (Metin2 SinglePlayer). The invite codes for friends are in the COOP window.')
    if (@($state.friends).Count -eq 0) { Write-Host (UI-Text 'Nie masz jeszcze znajomych - dodaj ich w oknie COOP.' 'You have no friends added yet - add them in the COOP window.') -ForegroundColor Yellow }
    if ($mapped.Count -gt 0 -and $mapped.Count -lt $ports.Count) {
        Write-Host (UI-Text 'Nie wszystkie porty udało się otworzyć - bez nich znajomy utknie przy zmianie mapy.' 'Not every port could be opened - without them a friend gets stuck at a map change.') -ForegroundColor Yellow
    }
}

function Stop-CoopHostingAction {
    Assert-CoopModule
    $state = Read-M2CoopState -ServerRoot $serverRoot
    $ports = Get-M2CoopGamePorts -ServerRoot $serverRoot
    $lan = Get-M2CoopLanAddress
    if ($lan) {
        $gateway = Find-M2CoopGateway -LanAddress $lan.Address
        if ($gateway) {
            Write-Phase (UI-Text 'router: zamykanie portów' 'router: closing the ports')
            foreach ($port in $ports) {
                if (Remove-M2CoopPortMapping -Gateway $gateway -Port $port -LanAddress $lan.Address) { Write-Host (UI-Text "  port $port zamknięty" "  port $port closed") }
                else { Write-Host (UI-Text "  port $port ma cudze przekierowanie - nie ruszam go" "  port $port is forwarded by somebody else - leaving it alone") -ForegroundColor Yellow }
            }
        }
    }
    $bindings = Get-M2CoopGameBindings -ServerRoot $serverRoot
    $previous = Get-DotEnvValue -Key 'M2_HOST_BIND_ADDRESS'
    if ($bindings.Public -or $previous -ne '127.0.0.1') {
        Write-Phase (UI-Text 'porty gry tylko dla tego komputera (restart serwera gry, około minuty)' 'game ports for this computer only (the game server restarts, about a minute)')
        Invoke-CoopGameRecreate -BindAddress '127.0.0.1'
        [void](Wait-CoopGameReady)
    }
    Write-Host ((UI-Text "Opublikowane: {0}" "Published: {0}") -f ((Get-M2CoopGameBindings -ServerRoot $serverRoot).Lines -join '; '))
    if ($state.hosting) { $state.hosting.active = $false }
    Save-M2CoopState -ServerRoot $serverRoot -State $state
    Write-Host (UI-Text 'Hostowanie wyłączone. Reguła zapory zostaje, ale porty słuchają już tylko na tym komputerze.' 'Hosting is off. The firewall rule stays, but the ports listen on this computer only now.') -ForegroundColor Green
}

function Update-CoopHostingLease {
    # A mapping leased for four hours has to be renewed by somebody: GRAJ and
    # the window's timer both come here. Only while hosting is on, and quietly.
    if (-not (Get-Command Read-M2CoopState -ErrorAction SilentlyContinue)) { return }
    $state = Read-M2CoopState -ServerRoot $serverRoot
    if (-not ($state.hosting -and $state.hosting.active)) { return }
    # Through a VPN nothing is leased in the router.
    if ((Get-CoopHostingField $state.hosting 'mode') -eq 'vpn') { return }
    $lan = Get-M2CoopLanAddress
    if (-not $lan) { return }
    $gateway = Find-M2CoopGateway -LanAddress $lan.Address
    if (-not $gateway) { return }
    $ok = 0
    foreach ($port in @(Get-M2CoopGamePorts -ServerRoot $serverRoot)) {
        if ((Add-M2CoopPortMapping -Gateway $gateway -Port $port -LanAddress $lan.Address).Ok) { $ok++ }
    }
    Write-Host ((UI-Text "COOP: przekierowania w routerze odnowione ({0})." "COOP: port forwarding in the router renewed ({0}).") -f $ok)
}

function Join-CoopAction {
    Assert-CoopModule
    $code = $Invite
    if (-not $code) { $code = Read-Host (UI-Text 'Wklej kod zaproszenia od znajomego' 'Paste the invite code from your friend') }
    $inv = Read-M2CoopInvite -Code $code
    $client = Get-M2CoopClientFolder -ServerRoot $serverRoot
    if (-not $client) { throw (UI-Text 'Nie znaleziono folderu klienta (wskaż go przyciskiem WYBIERZ KLIENTA).' 'The client folder was not found (point to it with the CHOOSE CLIENT button).') }
    # The host's home address when this machine is in that network and the
    # world answers there (Select-M2CoopJoinHost), the invite's own otherwise.
    $choice = Resolve-M2CoopJoinHost -Invite $inv
    $path = Write-M2CoopClientConfig -ClientFolder $client -Invite $inv -HostAddress $choice.Host
    Write-Host ((UI-Text "Zapisano {0} (adres {1})" "Saved {0} (address {1})") -f $path, $choice.Host) -ForegroundColor Green
    Write-Host ((UI-Text "W kliencie wybierz serwer 'Online: {0}' i zaloguj się: login {1}, hasło {2}" "In the client choose the server 'Online: {0}' and log in: login {1}, password: {2}") -f $inv.name, $inv.login, $inv.password) -ForegroundColor Cyan
    $advice = $(if ($choice.Lan) { '' } else { Get-M2CoopJoinAdvice -Invite $inv })
    if ($advice) { Write-Host $advice -ForegroundColor Yellow }
    foreach ($note in @(Get-M2CoopJoinNotes -Choice $choice)) {
        Write-Host $note -ForegroundColor $(if ($choice.Answers) { 'Green' } else { 'Yellow' })
    }
    # A client exe from before 2.0.17 cannot enter a friend's world at all
    # (Test-M2CoopClientExeOld).
    if (Test-M2CoopClientExeOld -ClientFolder $client) { Write-Host (Get-M2CoopOldClientNote) -ForegroundColor Red }
}

# ---------------------------------------------------------------- vps
# This world on a rented Linux VPS (launcher\Metin2Launcher.Vps.psm1 does the
# work, linux-port/tools/vps-install.sh does it on the VPS). The window's VPS
# dialog runs the long ones through here - the install, the update, the
# status, the logs - and does the key, the tunnel and anything that shows a
# password in-process: an action's output is a file under launcher-logs, which
# support bundles carry. Installing, updating, the panels and the invite
# codes are for everybody.

function Assert-VpsModule {
    if (-not (Get-Command Install-M2Vps -ErrorAction SilentlyContinue)) {
        throw (UI-Text 'Brak modułu launcher\Metin2Launcher.Vps.psm1 - ta paczka nie ma opcji VPS.' 'The launcher\Metin2Launcher.Vps.psm1 module is missing - this package has no VPS option.')
    }
}

function Get-VpsStateForAction {
    # The saved VPS with whatever -VpsHost/-VpsUser/-VpsPort/-VpsDir said,
    # asked for in the text menu when there is none yet (or -Ask), and saved.
    param([switch]$Ask)
    Assert-VpsModule
    $state = Get-M2VpsState -ServerRoot $serverRoot
    if ($VpsHost) { $state.host = $VpsHost.Trim() }
    if ($VpsUser) { $state.user = $VpsUser.Trim() }
    if ($VpsPort -gt 0) { $state.port = $VpsPort }
    if ($VpsDir) { $state.remoteDir = $VpsDir.Trim() }
    if (($Ask -or -not $state.host) -and $Action -eq 'Menu') {
        $answer = Read-Host ((UI-Text 'Adres VPS (IPv4 albo domena){0}' 'VPS address (IPv4 or domain){0}') -f $(if ($state.host) { ' [' + $state.host + ']' } else { '' }))
        if ($answer) { $state.host = $answer.Trim() }
        $answer = Read-Host ((UI-Text 'Użytkownik na VPS [{0}]' 'User on the VPS [{0}]') -f $state.user)
        if ($answer) { $state.user = $answer.Trim() }
        $answer = Read-Host ((UI-Text 'Port SSH [{0}]' 'SSH port [{0}]') -f $state.port)
        $number = 0
        if ($answer -and [int]::TryParse($answer, [ref]$number)) { $state.port = $number }
    }
    Assert-M2VpsState -State $state
    Save-M2VpsState -ServerRoot $serverRoot -State $state
    return $state
}

function Assert-VpsConsole {
    # Passwords go to a person and never into a file: the window runs actions
    # with their output redirected into launcher-logs.
    if ([Console]::IsOutputRedirected) {
        throw (UI-Text 'To polecenie pokazuje hasła, więc działa w menu tekstowym albo w oknie VPS launchera, nie jako akcja w tle.' 'This command shows passwords, so it runs in the text menu or in the launcher''s VPS window, not as a background action.')
    }
}

function Connect-VpsAction {
    $state = Get-VpsStateForAction -Ask
    Write-Host (UI-Text 'Otworzy się okno ssh - wpisz w nim hasło do VPS (tylko ten jeden raz; launcher go nie widzi).' 'An ssh window opens - type the VPS password in it (this one time only; the launcher never sees it).') -ForegroundColor Yellow
    if (Install-M2VpsKey -State $state) { Write-Host ((UI-Text 'Klucz działa: launcher łączy się z {0} bez hasła.' 'The key works: the launcher connects to {0} without a password.') -f $state.host) -ForegroundColor Green }
    else { throw (UI-Text 'Klucz nie działa - sprawdź adres, użytkownika i hasło, i spróbuj jeszcze raz.' 'The key does not work - check the address, the user and the password, and try again.') }
}

function Show-VpsCheckAction {
    $state = Get-VpsStateForAction
    Write-Phase (UI-Text 'sprawdzanie VPS' 'checking the VPS')
    $machine = Test-M2VpsMachine -State $state
    foreach ($line in (Format-M2VpsMachineReport -Machine $machine)) { Write-Host $line }
    if (-not $machine.Verdict.Ok) { throw (UI-Text 'VPS nie spełnia wymagań - szczegóły wyżej.' 'The VPS does not meet the requirements - the details are above.') }
}

function Write-VpsOutcome {
    param([Parameter(Mandatory = $true)]$Status, [Parameter(Mandatory = $true)]$State, [string]$What = (UI-Text 'Instalacja' 'Installation'))
    if ($Status.State -ne 'done') {
        throw ((UI-Text '{0} na VPS: {1} (etap {2}) - {3}. Szczegóły: LOGI VPS.' '{0} on the VPS: {1} (stage {2}) - {3}. Details: VPS LOGS.') -f $What, $Status.State, $Status.Phase, $Status.Message)
    }
    Write-Host ((UI-Text 'Serwer działa na VPS, wersja {0}.' 'The server runs on the VPS, version {0}.') -f $Status.Version) -ForegroundColor Green
    Write-Host ((UI-Text 'Gracze łączą się z {0} (porty TCP {1} i {2}; jeśli dostawca VPS ma własną zaporę, otwórz je tam).' 'Players connect to {0} (TCP ports {1} and {2}; if the VPS provider has a firewall of its own, open them there too).') -f (Get-M2VpsWorldAddress -State $State -Status $Status), $Status.AuthPort, $Status.GamePortRange)
    Write-Host (UI-Text 'Panele słuchają tylko na VPS: otwiera je OTWÓRZ PANEL (tunel SSH). Hasła kont admin i test pokazuje HASŁA KONT - w tym logu ich nie ma.' 'The panels listen on the VPS only: OPEN PANEL opens them (SSH tunnel). ACCOUNT PASSWORDS shows the admin and test passwords - they are not in this log.')
}

function Install-VpsAction {
    $state = Get-VpsStateForAction
    if (-not (Confirm-Operation ((UI-Text 'Zainstalować ten świat na VPS {0}? Folder serwera pójdzie na VPS (około 100 MB), a pierwsza budowa trwa tam 15-40 minut.' 'Install this world on the VPS {0}? The server folder goes to the VPS (about 100 MB), and the first build takes 15-40 minutes there.') -f $state.host))) { return }
    Write-Phase (UI-Text 'instalacja na VPS' 'installing on the VPS')
    $final = Install-M2Vps -State $state -ServerRoot $serverRoot
    Write-VpsOutcome -Status $final -State $state
}

function Update-VpsAction {
    $state = Get-VpsStateForAction
    if (-not (Confirm-Operation ((UI-Text 'Zaktualizować serwer na VPS {0} do najnowszej wersji z GitHuba?' 'Update the server on the VPS {0} to the newest version from GitHub?') -f $state.host))) { return }
    Write-Phase (UI-Text 'aktualizacja VPS' 'updating the VPS')
    $final = Update-M2Vps -State $state -ServerRoot $serverRoot
    Write-VpsOutcome -Status $final -State $state -What (UI-Text 'Aktualizacja' 'Update')
}

function Show-VpsStatusAction {
    $state = Get-VpsStateForAction
    $status = Get-M2VpsStatus -State $state
    $words = @{ running = (UI-Text 'trwa' 'running'); done = (UI-Text 'gotowe' 'done'); failed = (UI-Text 'NIE UDAŁO SIĘ' 'FAILED'); interrupted = (UI-Text 'PRZERWANE (VPS zrestartowany w trakcie? uruchom instalację jeszcze raz)' 'INTERRUPTED (was the VPS restarted meanwhile? run the installation again)'); none = (UI-Text 'nic jeszcze nie uruchomiono' 'nothing started yet') }
    $said = $(if ($words.ContainsKey($status.State)) { $words[$status.State] } else { $status.State })
    Write-Host ((UI-Text 'VPS {0}: wersja {1}, zadanie "{2}": {3}' 'VPS {0}: version {1}, job "{2}": {3}') -f $state.host, $status.Version, $status.Kind, $said)
    if ($status.Message) { Write-Host ((UI-Text '  {0} (etap {1}, od {2}{3})' '  {0} (stage {1}, since {2}{3})') -f $status.Message, $status.Phase, $status.Started, $(if ($status.Finished) { (UI-Text ' do ' ' until ') + $status.Finished } else { '' })) }
    Write-Host ((UI-Text 'Adres dla graczy: {0}, porty {1} i {2}; panele na {3}, gra na {4}; botów: {5}' 'Address for players: {0}, ports {1} and {2}; panels on {3}, the game on {4}; bots: {5}') -f $status.PublicAddress, $status.AuthPort, $status.GamePortRange, $status.PanelBind, $status.HostBind, $status.Bots)
    if ($status.LogText) { Write-Host (UI-Text '--- koniec logu instalacji ---' '--- end of the installation log ---'); Write-Host $status.LogText }
    if ($status.PsText) { Write-Host (UI-Text '--- kontenery ---' '--- containers ---'); Write-Host $status.PsText }
}

function Open-VpsPanelAction {
    $state = Get-VpsStateForAction
    $addresses = Open-M2VpsPanel -State $state -ServerRoot $serverRoot
    Write-Host ((UI-Text 'Tunel do paneli VPS działa: panel {0}, panel zaawansowany {1}, ItemShop {2}.' 'The tunnel to the VPS panels works: panel {0}, advanced panel {1}, ItemShop {2}.') -f $addresses.ClassicUrl, $addresses.SebanUrl, $addresses.ItemShopUrl) -ForegroundColor Green
    Write-Host (UI-Text 'Działa po zamknięciu launchera; kończy go ZAMKNIJ TUNEL albo zerwane połączenie.' 'It keeps running after the launcher is closed; CLOSE TUNNEL or a broken connection ends it.')
    Start-Process $addresses.ClassicUrl
}

function Close-VpsPanelAction {
    Assert-VpsModule
    if (Close-M2VpsPanel -ServerRoot $serverRoot) { Write-Host (UI-Text 'Tunel do paneli VPS zamknięty.' 'The tunnel to the VPS panels is closed.') -ForegroundColor Green }
    else { Write-Host (UI-Text 'Tunel do paneli VPS nie był otwarty.' 'The tunnel to the VPS panels was not open.') }
}

function Show-VpsLogsAction {
    $state = Get-VpsStateForAction
    Write-Host (Get-M2VpsLogs -State $state -Lines 300)
}

function Show-VpsPasswordsAction {
    Assert-VpsConsole
    $state = Get-VpsStateForAction
    $accounts = @(Get-M2VpsAccounts -State $state)
    if ($accounts.Count -eq 0) { Write-Host (UI-Text 'Na VPS nie ma jeszcze pliku z hasłami - powstaje, gdy baza wstanie po instalacji.' 'The VPS has no password file yet - it is made when the database comes up after the installation.') -ForegroundColor Yellow; return }
    foreach ($account in $accounts) {
        Write-Host ((UI-Text '  login {0,-14} hasło {1}{2}' '  login {0,-14} password: {1}{2}') -f $account.Login, $account.Password, $(if ($account.Note) { '   (' + $account.Note + ')' } else { '' })) -ForegroundColor Cyan
    }
    Write-Host ((UI-Text 'Te same hasła leżą na VPS w /root/metin2-accounts.txt (tylko dla roota).' 'The same passwords are on the VPS in /root/metin2-accounts.txt (for root only).'))
}

function Write-VpsClientAction {
    $state = Get-VpsStateForAction
    $result = Write-M2VpsClientEntry -State $state -ServerRoot $serverRoot
    Write-Host ((UI-Text 'Zapisano {0}: w kliencie wybierz serwer "Online: {1}" ({2}).' 'Saved {0}: in the client choose the server "Online: {1}" ({2}).') -f $result.Path, $result.Name, $result.Host) -ForegroundColor Green
    if ($result.Replaced) { Write-Host ((UI-Text 'Zastąpił świat znajomego "{0}" - klient ma jedno takie miejsce; kod zaproszenia wpisze go z powrotem.' 'It replaced your friend''s world "{0}" - the client has one such place; the invite code puts it back.') -f $result.Replaced) -ForegroundColor Yellow }
}

function Show-VpsInviteAction {
    Assert-VpsConsole
    Assert-CoopModule
    $state = Get-VpsStateForAction
    $name = $FriendName
    if (-not $name) { $name = Read-Host (UI-Text 'Imię albo nick znajomego (z niego powstanie login na VPS)' 'Your friend''s name or nick (the login on the VPS is made from it)') }
    if (-not $name) { throw (UI-Text 'Nie podano imienia znajomego.' 'No friend''s name was given.') }
    $status = Get-M2VpsStatus -State $state
    $friend = New-M2VpsFriend -State $state -ServerRoot $serverRoot -Name $name
    Write-Host ((UI-Text 'Konto na VPS dla {0}: login {1}, hasło {2}' 'Account on the VPS for {0}: login {1}, password: {2}') -f $friend.name, $friend.login, $friend.password) -ForegroundColor Green
    Write-Host (UI-Text 'Kod zaproszenia (skopiuj i wyślij znajomemu w prywatnej wiadomości - zawiera hasło):' 'Invite code (copy it and send it to your friend in a private message - it holds the password):')
    Write-Host (Get-M2VpsFriendInvite -State $state -ServerRoot $serverRoot -Account $friend -Status $status) -ForegroundColor Cyan
}

function Invoke-Action {
    param([Parameter(Mandatory = $true)][string]$SelectedAction)
    $config = Get-Config
    switch ($SelectedAction) {
        'Start' { Start-Server }
        'Stop' {
            Stop-Server
            if (Test-M2DockerRunning) {
                Write-Host (UI-Text 'Serwer zatrzymany, Docker Desktop działa dalej. Dane pozostają zapisane w wolumenach.' 'Server stopped, Docker Desktop keeps running. The data stays saved in the volumes.') -ForegroundColor Green
            }
        }
        'StartDocker' { Start-Docker }
        'StopAll' { Stop-DockerAndServer }
        'FreePorts' { Clear-PortConflictsAction }
        'Check' {
            $remote = Get-M2UpdateManifest -Source (Get-ManifestSource $config)
            Show-UpdateStatus -RemoteManifest $remote
        }
        'UpdateServer' {
            $remote = Get-M2UpdateManifest -Source (Get-ManifestSource $config)
            Show-UpdateStatus -RemoteManifest $remote
            Update-Server -RemoteManifest $remote
        }
        'UpdateClient' {
            $remote = Get-M2UpdateManifest -Source (Get-ManifestSource $config)
            Show-UpdateStatus -RemoteManifest $remote
            Update-Client -RemoteManifest $remote -Config $config
        }
        'RepairClientExe' {
            $remote = Get-M2UpdateManifest -Source (Get-ManifestSource $config)
            Repair-ClientExe -RemoteManifest $remote -Config $config
        }
        'UpdateAll' {
            $remote = Get-M2UpdateManifest -Source (Get-ManifestSource $config)
            Show-UpdateStatus -RemoteManifest $remote
            $clientComponent = Get-ManifestComponent -RemoteManifest $remote -Name 'client'
            if ($clientComponent -and -not (Test-InstalledVersion -Installed ([string](Read-State).client) -Available ([string]$clientComponent.version))) {
                Assert-ClientNotRunning -Config $config
            }
            Update-Server -RemoteManifest $remote
            Update-Client -RemoteManifest $remote -Config $config
        }
        'Diagnose' { Show-DockerDiagnostics -CheckPanelPort | Out-Null }
        'Logs' { Create-Logs | Out-Null }
        'SendLogs' { Send-Logs }
        'Configure' { Configure-Launcher }
        'SetBots' { Set-BotCountAction }
        'SetDifficulty' { Set-DifficultyAction }
        'ImportDb' { Import-DatabaseAction }
        'BackupDb' { Backup-DatabaseAction }
        'RestoreDb' { Restore-DatabaseAction }
        'ResetWorld' { Reset-WorldAction }
        'RepairDb' { Repair-DatabaseAction }
        'DbAccess' { Show-DatabaseAccessAction }
        'PanelPassword' { Reset-PanelPasswordAction }
        'CoopCheck' { Show-CoopCheckAction }
        'CoopSecure' { Protect-CoopAccountsAction }
        'CoopAddFriend' { Add-CoopFriendAction }
        'CoopBlockFriend' { Set-CoopFriendBlockedAction -Blocked $true }
        'CoopUnblockFriend' { Set-CoopFriendBlockedAction -Blocked $false }
        'CoopInvite' { Show-CoopInviteAction }
        'CoopHost' { Start-CoopHostingAction }
        'CoopStop' { Stop-CoopHostingAction }
        'CoopRenew' { Assert-CoopModule; Update-CoopHostingLease }
        'CoopJoin' { Join-CoopAction }
        'VpsConnect' { Connect-VpsAction }
        'VpsCheck' { Show-VpsCheckAction }
        'VpsInstall' { Install-VpsAction }
        'VpsUpdate' { Update-VpsAction }
        'VpsStatus' { Show-VpsStatusAction }
        'VpsPanel' { Open-VpsPanelAction }
        'VpsPanelClose' { Close-VpsPanelAction }
        'VpsLogs' { Show-VpsLogsAction }
        'VpsPasswords' { Show-VpsPasswordsAction }
        'VpsClient' { Write-VpsClientAction }
        'VpsInvite' { Show-VpsInviteAction }
        default { throw (UI-Text "Nieznana akcja: $SelectedAction" "Unknown action: $SelectedAction") }
    }
}

function Show-Menu {
    while ($true) {
        Write-Header
        Write-Host (UI-Text '  1. Uruchom serwer' '  1. Start the server')
        Write-Host (UI-Text '  2. Zatrzymaj serwer' '  2. Stop the server')
        Write-Host (UI-Text '  3. Uruchom tylko Docker Desktop' '  3. Start Docker Desktop only')
        Write-Host (UI-Text '  4. Zatrzymaj serwer i Docker (postęp zostaje)' '  4. Stop the server and Docker (the progress is kept)')
        Write-Host (UI-Text '  5. Sprawdź aktualizacje' '  5. Check for updates')
        Write-Host (UI-Text '  6. Aktualizuj serwer' '  6. Update the server')
        Write-Host (UI-Text '  7. Aktualizuj klienta' '  7. Update the client')
        Write-Host (UI-Text '  8. Aktualizuj wszystko' '  8. Update everything')
        Write-Host (UI-Text '  9. Sprawdź Docker, WSL, wirtualizację i porty' '  9. Check Docker, WSL, virtualization and the ports')
        Write-Host (UI-Text ' 10. Utwórz paczkę diagnostyczną ZIP' ' 10. Make a diagnostic ZIP bundle')
        Write-Host (UI-Text ' 11. Utwórz i wyślij logi (po potwierdzeniu)' ' 11. Make and send the logs (after you confirm)')
        Write-Host (UI-Text ' 12. Konfiguracja launchera' ' 12. Launcher settings')
        Write-Host (UI-Text ' 13. Ustaw liczbę grających botów (0-2500)' ' 13. Set the number of playing bots (0-2500)')
        Write-Host (UI-Text ' 14. Importuj bazę z innej instalacji (wyższe postacie)' ' 14. Import the database of another installation (higher characters)')
        Write-Host (UI-Text ' 15. Zapisz kopię świata (backup do pliku zip)' ' 15. Save a backup of the world (to a zip file)')
        Write-Host (UI-Text ' 16. Przywróć świat z kopii' ' 16. Restore the world from a backup')
        Write-Host (UI-Text ' 17. Wyzeruj świat i zacznij od nowa (świeża instalacja; kopia zapisywana automatycznie)' ' 17. Wipe the world and start over (fresh install; a backup is saved automatically)')
        Write-Host (UI-Text ' 18. Napraw dostęp do bazy (gdy migrate/serwer nie startuje albo Navicat odrzuca hasło)' ' 18. Repair database access (when migrate/the server does not start or Navicat rejects the password)')
        Write-Host (UI-Text ' 19. Dane do połączenia z bazą (Navicat, HeidiSQL)' ' 19. Database connection details (Navicat, HeidiSQL)')
        Write-Host (UI-Text ' 20. Hasło do panelu WWW (pokaż / zresetuj)' ' 20. Web panel password (show / reset)')
        Write-Host (UI-Text ' 21. Zwolnij porty (gdy „port jest już zajęty” blokuje start lub aktualizację)' ' 21. Free the ports (when "port is already in use" blocks a start or an update)')
        Write-Host (UI-Text ' 22. Poziom trudności (czekanie u Biologa i Stajennego: easy / medium / hard / własne godziny)' ' 22. Difficulty (the waits at the Biologist and the Stable Keeper: easy / medium / hard / your own hours)')
        if (Get-Command Get-M2CoopNetworkReport -ErrorAction SilentlyContinue) {
            Write-Host (UI-Text ' 23. COOP: sprawdź sieć i stan hostowania (eksperymentalne)' ' 23. COOP: check the network and the hosting (experimental)')
            Write-Host (UI-Text ' 24. COOP: zabezpiecz konta admin i test (nowe hasła)' ' 24. COOP: secure the admin and test accounts (new passwords)')
            Write-Host (UI-Text ' 25. COOP: dodaj znajomego (konto i kod zaproszenia)' ' 25. COOP: add a friend (an account and an invite code)')
            Write-Host (UI-Text ' 26. COOP: pokaż kody zaproszeń' ' 26. COOP: show the invite codes')
            Write-Host (UI-Text ' 27. COOP: hostuj świat dla znajomych' ' 27. COOP: host the world for friends')
            Write-Host (UI-Text ' 28. COOP: zakończ hostowanie' ' 28. COOP: stop hosting')
            Write-Host (UI-Text ' 29. COOP: dołącz do świata znajomego (wklej kod)' ' 29. COOP: join a friend''s world (paste the code)')
        }
        if (Get-Command Install-M2Vps -ErrorAction SilentlyContinue) {
            Write-Host (UI-Text ' 30. VPS: adres i połączenie (klucz SSH; hasło do VPS wpisujesz raz)' ' 30. VPS: address and connection (SSH key; you type the VPS password once)')
            Write-Host (UI-Text ' 31. VPS: sprawdź serwer (procesor, pamięć, dysk, uprawnienia)' ' 31. VPS: check the server (CPU, memory, disk, permissions)')
            Write-Host (UI-Text ' 32. VPS: zainstaluj ten świat na VPS' ' 32. VPS: install this world on the VPS')
            Write-Host (UI-Text ' 33. VPS: aktualizuj serwer na VPS' ' 33. VPS: update the server on the VPS')
            Write-Host (UI-Text ' 34. VPS: stan instalacji' ' 34. VPS: installation status')
            Write-Host (UI-Text ' 35. VPS: otwórz panel WWW (tunel SSH)' ' 35. VPS: open the web panel (SSH tunnel)')
            Write-Host (UI-Text ' 36. VPS: zamknij tunel do paneli' ' 36. VPS: close the tunnel to the panels')
            Write-Host (UI-Text ' 37. VPS: hasła kont gry (admin, test, znajomi)' ' 37. VPS: game account passwords (admin, test, friends)')
            Write-Host (UI-Text ' 38. VPS: dopisz serwer VPS do klienta gry' ' 38. VPS: add the VPS server to the game client')
            Write-Host (UI-Text ' 39. VPS: logi serwera' ' 39. VPS: server logs')
            Write-Host (UI-Text ' 40. VPS: konto i kod zaproszenia dla znajomego (COOP)' ' 40. VPS: an account and an invite code for a friend (COOP)')
        }
        Write-Host (UI-Text '  0. Wyjście' '  0. Exit')
        Write-Host ''
        $choice = Read-Host (UI-Text 'Wybierz opcję' 'Choose an option')
        $selected = switch ($choice) {
            '1' { 'Start' } '2' { 'Stop' } '3' { 'StartDocker' } '4' { 'StopAll' }
            '5' { 'Check' } '6' { 'UpdateServer' } '7' { 'UpdateClient' }
            '8' { 'UpdateAll' } '9' { 'Diagnose' } '10' { 'Logs' } '11' { 'SendLogs' } '12' { 'Configure' }
            '13' { 'SetBots' }
            '14' { 'ImportDb' }
            '15' { 'BackupDb' }
            '16' { 'RestoreDb' }
            '17' { 'ResetWorld' }
            '18' { 'RepairDb' }
            '19' { 'DbAccess' }
            '20' { 'PanelPassword' }
            '21' { 'FreePorts' }
            '22' { 'SetDifficulty' }
            '23' { 'CoopCheck' }
            '24' { 'CoopSecure' }
            '25' { 'CoopAddFriend' }
            '26' { 'CoopInvite' }
            '27' { 'CoopHost' }
            '28' { 'CoopStop' }
            '29' { 'CoopJoin' }
            '30' { 'VpsConnect' }
            '31' { 'VpsCheck' }
            '32' { 'VpsInstall' }
            '33' { 'VpsUpdate' }
            '34' { 'VpsStatus' }
            '35' { 'VpsPanel' }
            '36' { 'VpsPanelClose' }
            '37' { 'VpsPasswords' }
            '38' { 'VpsClient' }
            '39' { 'VpsLogs' }
            '40' { 'VpsInvite' }
            '0' { return }
            default { '' }
        }
        if (-not $selected) { continue }
        try { Invoke-Action -SelectedAction $selected }
        catch { Write-Host (UI-Text "BŁĄD: $($_.Exception.Message)" "ERROR: $($_.Exception.Message)") -ForegroundColor Red }
        Write-Host ''
        Read-Host (UI-Text 'Naciśnij Enter, aby wrócić do menu' 'Press Enter to go back to the menu') | Out-Null
    }
}

try {
    if ($Action -eq 'Menu') { Show-Menu }
    else { Invoke-Action -SelectedAction $Action }
}
catch {
    Write-Host (UI-Text "BŁĄD: $($_.Exception.Message)" "ERROR: $($_.Exception.Message)") -ForegroundColor Red
    exit 1
}
