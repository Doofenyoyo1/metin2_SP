# The full package of the mt2009 line - the one zip a player downloads from
# the hosting: the client and the whole server tree side by side, as
#
#     Metin2 Singleplayer\
#         CZYTAJ.txt
#         Klient\      the client, its packs pointing at 127.0.0.1
#         Serwer\      the deploy tree from New-M2DeployTree.ps1
#
# Nothing personal goes in: the server's .env, installation identity, logs,
# backups and support bundles stay out, and so do the client's saved
# credentials, settings, screenshots and syserr.txt. The launcher makes a
# fresh .env and identity on the player's first start.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Deploy,
    [Parameter(Mandatory = $true)][string]$Client,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [string]$Version = '',
    [string]$SevenZip = 'C:\Program Files\7-Zip\7z.exe'
)
$ErrorActionPreference = 'Stop'
foreach ($p in @($Deploy, $Client)) {
    if (-not (Test-Path -LiteralPath $p -PathType Container)) { throw "no such directory: $p" }
}
if (-not (Test-Path -LiteralPath $SevenZip -PathType Leaf)) { throw "7-Zip not found at $SevenZip" }
if (-not (Test-Path -LiteralPath (Join-Path $Client 'metin2client.exe') -PathType Leaf)) { throw "no metin2client.exe in $Client" }
if (-not $Version) { $Version = ([IO.File]::ReadAllText((Join-Path $Deploy 'VERSION'))).Trim() }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$staging = Join-Path ([IO.Path]::GetTempPath()) ('m2-full-' + [Guid]::NewGuid().ToString('N'))
$rootName = 'Metin2 Singleplayer'
$stageRoot = Join-Path $staging $rootName
New-Item -ItemType Directory -Path $stageRoot -Force | Out-Null

function Invoke-Mirror {
    param([string]$From, [string]$To, [string[]]$ExcludeFiles = @(), [string[]]$ExcludeDirs = @())
    $args = @($From, $To, '/E', '/NFL', '/NDL', '/NJH', '/NJS', '/NP')
    if ($ExcludeFiles.Count -gt 0) { $args += '/XF'; $args += $ExcludeFiles }
    if ($ExcludeDirs.Count -gt 0) { $args += '/XD'; $args += $ExcludeDirs }
    & robocopy @args | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE): $From -> $To" }
}

try {
    Write-Host "staging the server from $Deploy"
    Invoke-Mirror -From $Deploy -To (Join-Path $stageRoot 'Serwer') `
        -ExcludeFiles @('.env', '.m2install.json', '.m2launcher.json', '.m2launcher-state.json', '*.log') `
        -ExcludeDirs @('launcher-logs', 'backups', 'support-bundles')
    foreach ($must in @('VERSION', 'Metin2-Launcher-GUI.bat', 'linux-port\docker\ENGINE', 'linux-port\docker\.env.example',
                        'linux-port\docker\game\src\server\game\src\playerbot_manager.cpp',
                        'linux-port\docker\mariadb\initdb.d\dumps\world.sql')) {
        if (-not (Test-Path -LiteralPath (Join-Path $stageRoot "Serwer\$must") -PathType Leaf)) { throw "the deploy tree lacks $must" }
    }
    if (Test-Path -LiteralPath (Join-Path $stageRoot 'Serwer\linux-port\docker\.env')) { throw '.env leaked into the staging' }

    Write-Host "staging the client from $Client"
    Invoke-Mirror -From $Client -To (Join-Path $stageRoot 'Klient') `
        -ExcludeFiles @('syserr.txt', 'credentials.json', 'game_settings.json') `
        -ExcludeDirs @('screenshot', 'upload')

    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '..\CZYTAJ.txt') -Destination (Join-Path $stageRoot 'CZYTAJ.txt') -Force

    $zipPath = Join-Path $OutputDirectory ("Metin2-Singleplayer-$Version.zip")
    if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
    Write-Host "zipping to $zipPath"
    Push-Location $staging
    try {
        & $SevenZip a -tzip -mx=5 -mmt=on -bso0 -bsp0 $zipPath $rootName
        if ($LASTEXITCODE -ne 0) { throw "7z failed with $LASTEXITCODE" }
    }
    finally { Pop-Location }

    $hash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToUpperInvariant()
    [IO.File]::WriteAllText("$zipPath.sha256", "$hash  $(Split-Path -Leaf $zipPath)`n", [Text.UTF8Encoding]::new($false))
    $mb = [math]::Round((Get-Item -LiteralPath $zipPath).Length / 1MB)
    Write-Host "Created: $zipPath ($mb MB)"
    Write-Host "SHA-256: $hash"
}
finally {
    if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
}
