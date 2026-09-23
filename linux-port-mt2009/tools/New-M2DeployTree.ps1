# Assembles the mt2009 deploy tree - what a player's Serwer\ folder holds:
# the server update package unpacked (the repository's mt2009 tree published
# under the linux-port name, see port/listify.py), plus what no update ever
# carries: the staged engine, its externals, the runtime share, the SQL dumps
# and the docs. Re-runnable; an existing .env, .m2install.json and database
# volume are left alone.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Deploy,
    [string]$Repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [string]$Version = ''
)
$ErrorActionPreference = 'Stop'
$Repo = [IO.Path]::GetFullPath($Repo).TrimEnd('\')
$mt = Join-Path $Repo 'linux-port-mt2009'
if (-not $Version) { $Version = ([IO.File]::ReadAllText((Join-Path $mt 'VERSION'))).Trim() }

$temp = Join-Path ([IO.Path]::GetTempPath()) ('m2-deploy-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temp -Force | Out-Null
try {
    Write-Host "packaging $Version from $Repo"
    & (Join-Path $Repo 'tools\New-M2UpdatePackage.ps1') -Type server -Version $Version -SourceRoot $Repo `
        -FileList (Join-Path $Repo 'launcher\server-update-files.mt2009.txt') -OutputDirectory $temp `
        -PathMap @{
            'linux-port-mt2009/docker/docker-compose.deploy.yml' = 'linux-port/docker/docker-compose.yml'
            'linux-port-mt2009/VERSION' = 'VERSION'
            'linux-port-mt2009/PACZKA_INFO.txt' = 'PACZKA_INFO.txt'
            'linux-port-mt2009/' = 'linux-port/'
        }
    $zip = Get-ChildItem -LiteralPath $temp -Filter 'metin2-server-update-*.zip' | Select-Object -First 1
    if (-not $zip) { throw 'the packager produced no zip' }

    New-Item -ItemType Directory -Path $Deploy -Force | Out-Null
    Write-Host "unpacking $($zip.Name) into $Deploy"
    Expand-Archive -LiteralPath $zip.FullName -DestinationPath $Deploy -Force

    $dst = Join-Path $Deploy 'linux-port\docker'
    # What the update never carries (gitignored in the repository): the
    # engine and the runtime tree from the package, the SQL dumps.
    foreach ($tree in @('game\src\server', 'game\src\extern', 'game\src\extern-tarballs', 'game\src\serverfiles', 'mariadb\initdb.d\dumps')) {
        $from = Join-Path $mt "docker\$tree"
        $to = Join-Path $dst $tree
        if (-not (Test-Path -LiteralPath $from)) { throw "missing in the repository tree (stage it from the package first): $from" }
        Write-Host "mirroring $tree"
        New-Item -ItemType Directory -Path (Split-Path -Parent $to) -Force | Out-Null
        & robocopy $from $to /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
        if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE) for $tree" }
    }

    # Which client the full package ships beside this tree. Read by the
    # launcher as the installed client version until the first client update
    # records one in .m2launcher-state.json - without it a fresh install said
    # "unknown" and the startup check offered the client package it already had.
    # Deliberately NOT in the server update list: a server update must not
    # overwrite what a player's client actually is.
    Copy-Item -LiteralPath (Join-Path $mt 'CLIENT_VERSION') -Destination (Join-Path $Deploy 'CLIENT_VERSION') -Force
    # Docs and the installer travel with a full package, never with an update.
    foreach ($f in @('README.md', 'README_EN.md', 'TUTORIAL.md', 'UNINSTALL.md', 'UPDATING.md', 'LICENSE', 'NOTICE.md', 'CONTRIBUTING.md')) {
        $p = Join-Path $Repo $f
        if (Test-Path -LiteralPath $p) { Copy-Item -LiteralPath $p -Destination (Join-Path $Deploy $f) -Force }
    }
    foreach ($dir in @('docs', 'installer')) {
        & robocopy (Join-Path $Repo $dir) (Join-Path $Deploy $dir) /E /NFL /NDL /NJH /NJS /NP | Out-Null
        if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE) for $dir" }
    }

    # The launcher's overlay sync looks for these two beside the sources; they
    # are the same bytes as docker\game\*.txt, so the copy it makes is a no-op.
    $sf = Join-Path $Deploy 'linux-port\overlays\playerbot\serverfiles'
    New-Item -ItemType Directory -Path $sf -Force | Out-Null
    foreach ($f in @('mob_drop_item.m3.append.txt', 'special_item_group.moonlight.txt')) {
        Copy-Item -LiteralPath (Join-Path $mt "docker\game\$f") -Destination (Join-Path $sf $f) -Force
    }

    $size = [math]::Round((Get-ChildItem -LiteralPath $Deploy -Recurse -File | Measure-Object Length -Sum).Sum / 1MB)
    Write-Host "deploy tree ready: $Deploy ($size MB), version $Version"
}
finally {
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
}
