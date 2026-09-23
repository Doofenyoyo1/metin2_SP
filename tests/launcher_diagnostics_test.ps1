[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$modulePath = Join-Path $root 'launcher\Metin2Launcher.Diagnostics.psm1'

$parserErrors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile($modulePath, [ref]$null, [ref]$parserErrors)
if (@($parserErrors).Count -ne 0) {
    throw "Moduł diagnostyczny zawiera błędy składni: $($parserErrors | Out-String)"
}

Import-Module $modulePath -Force

$cases = @(
    [pscustomobject]@{
        Name = 'Docker port collision'
        Text = 'driver failed programming external connectivity: Bind for 127.0.0.1:7788 failed: port is already allocated'
        Expected = 'PORT_IN_USE'
    },
    [pscustomobject]@{
        Name = 'Virtualization disabled'
        Text = 'Virtualization support not detected'
        Expected = 'VIRTUALIZATION_DISABLED'
    },
    [pscustomobject]@{
        Name = 'Docker WSL disk read-only'
        Text = 'failed to solve: write /var/lib/desktop-containerd/daemon/io.containerd.metadata.v1.bolt/meta.db: read-only file system'
        Expected = 'DOCKER_DISK_BROKEN'
    },
    [pscustomobject]@{
        Name = 'Broken WSL'
        Text = 'There was a problem with WSL. wsl.exe exit status 1'
        Expected = 'WSL_BROKEN'
    },
    [pscustomobject]@{
        Name = 'Docker Engine timeout'
        Text = 'Docker Engine did not become ready within 180 seconds.'
        Expected = 'DOCKER_NOT_READY'
    },
    [pscustomobject]@{
        Name = 'Legacy installer destination'
        Text = 'docker.exe : cannot overwrite non-directory artifacts.json with directory C:\Users\tester\Metin2Server'
        Expected = 'LEGACY_INSTALLER_DESTINATION'
    },
    [pscustomobject]@{
        Name = 'Unpublished update channel'
        Text = 'Serwer zdalny zwrócił błąd: (404) Nie znaleziono.'
        Expected = 'UPDATE_CHANNEL_UNPUBLISHED'
    }
)

foreach ($case in $cases) {
    $actual = Get-M2LauncherErrorGuidance -Text $case.Text
    if ($actual.Code -ne $case.Expected) {
        throw "$($case.Name): oczekiwano $($case.Expected), otrzymano $($actual.Code)."
    }
    if (-not $actual.Title -or -not $actual.Message -or -not $actual.Remedy) {
        throw "$($case.Name): komunikat dla użytkownika jest niekompletny."
    }
}

$unknown = Get-M2LauncherErrorGuidance -Text 'unexpected test failure'
if ($unknown.Code -ne 'UNKNOWN') {
    throw "Nieznany błąd powinien używać kodu UNKNOWN, otrzymano $($unknown.Code)."
}

# The collision that actually stops an update is not the panel's. 7790 is the
# advanced panel, which a second copy of the server publishes too, and the
# remedy has to say why quitting Docker Desktop does not help.
$secondPanel = Get-M2LauncherErrorGuidance -Text (
    'driver failed programming external connectivity on endpoint m2zip-seban-panel: ' +
    'Bind for 127.0.0.1:7790 failed: port is already allocated')
if ($secondPanel.Code -ne 'PORT_IN_USE') {
    throw "Kolizja na porcie 7790 powinna dać PORT_IN_USE, otrzymano $($secondPanel.Code)."
}
if ($secondPanel.Title -notmatch '7790') {
    throw "Komunikat powinien nazywać port 7790, otrzymano: $($secondPanel.Title)"
}
if ($secondPanel.Remedy -notmatch 'unless-stopped') {
    throw 'Rada przy zajętym porcie musi tłumaczyć, dlaczego samo wyłączenie Dockera nie pomaga.'
}

# Every published port comes from the installation's own .env: compose gives up
# on the first one that is taken, so a preflight that knows only 7788 passes and
# the build dies afterwards.
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('m2ports-' + [Guid]::NewGuid().ToString('N'))
$expectedPorts = @(7788, 7790, 7791, 11000, 13000, 13001, 13002, 3306)
try {
    New-Item -ItemType Directory -Path (Join-Path $fixture 'linux-port\docker') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $fixture 'linux-port\docker\.env') -Encoding UTF8 -Value @(
        'M2_PANEL_PUBLIC_PORT=7788',
        'M2_SEBAN_PANEL_PORT=7790',
        'M2_ITEMSHOP_PUBLIC_PORT=7791',
        'M2_AUTH_PORT=11000',
        'M2_GAME_PORT_RANGE=13000-13002',
        'M2_DB_PUBLISH_PORT=3306')
    $stackPorts = @(Get-M2StackHostPorts -ServerRoot $fixture | ForEach-Object { [int]$_.Port })
    foreach ($expected in $expectedPorts) {
        if ($stackPorts -notcontains $expected) {
            throw "Preflight musi sprawdzać port $expected; otrzymano: $($stackPorts -join ', ')."
        }
    }
}
finally { Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue }

# Regresja z 2.0.31: Docker drukuje opublikowany zakres jako JEDEN wpis
# ("127.0.0.1:13000-13002->13000-13002/tcp"), a szukanie dosłownego "13001->"
# nie trafiało w żaden kanał gry. Preflight uznawał wtedy własny, działający
# serwer gracza za obcy program i odmawiał startu (sizowski).
$portsColumn = '127.0.0.1:11000->11000/tcp, 127.0.0.1:13000-13002->13000-13002/tcp'
$matchedPorts = @(Get-M2PublishedPortMatches -PortsText $portsColumn -Ports @(7788, 11000, 13000, 13001, 13002))
foreach ($expected in @(11000, 13000, 13001, 13002)) {
    if ($matchedPorts -notcontains $expected) {
        throw "Port $expected z zakresu musi zostać rozpoznany; otrzymano: $($matchedPorts -join ', ')."
    }
}
if ($matchedPorts -contains 7788) {
    throw 'Port spoza opublikowanej listy nie może zostać dopasowany.'
}

# Pojedynczy port, adres IPv6 i wpis bez adresu - wszystkie trzy postacie, w
# jakich Docker podaje stronę hosta.
$singleMatches = @(Get-M2PublishedPortMatches -PortsText '[::]:7788->7788/tcp, 7790->7789/tcp' -Ports @(7788, 7790, 7791))
foreach ($expected in @(7788, 7790)) {
    if ($singleMatches -notcontains $expected) {
        throw "Port $expected musi zostać rozpoznany; otrzymano: $($singleMatches -join ', ')."
    }
}
if ($singleMatches -contains 7791) {
    throw 'Nieopublikowany port nie może zostać dopasowany.'
}

# Port wystawiony tylko wewnątrz sieci (bez "->") nie jest publikowany na hoście.
if (@(Get-M2PublishedPortMatches -PortsText '7789/tcp' -Ports @(7789)).Count -ne 0) {
    throw 'Port bez publikacji na hoście nie może zostać dopasowany.'
}

[pscustomobject]@{
    Result = 'OK'
    ParserErrors = @($parserErrors).Count
    ClassifiedCases = $cases.Count
    UnknownFallback = $unknown.Code
} | ConvertTo-Json
