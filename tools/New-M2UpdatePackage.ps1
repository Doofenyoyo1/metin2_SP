[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][ValidateSet('server', 'client')][string]$Type,
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$SourceRoot,
    [Parameter(Mandatory = $true)][string]$FileList,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [string]$DownloadUrl = '',
    # Source prefix => published prefix, e.g. @{ 'linux-port-mt2009/' = 'linux-port/' }:
    # the mt2009 tree lives beside the r40250 one in the repository and is
    # deployed under the r40250 name, so the launcher's paths stay one path.
    # Applied to every listed path on its way into the zip; the pairing rules
    # below judge the published names, the copies read the sources.
    [hashtable]$PathMap = @{}
)

$ErrorActionPreference = 'Stop'
$source = [IO.Path]::GetFullPath($SourceRoot).TrimEnd('\')
$listPath = [IO.Path]::GetFullPath($FileList)
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw "Source root does not exist: $source" }
if (-not (Test-Path -LiteralPath $listPath -PathType Leaf)) { throw "File list does not exist: $listPath" }
New-Item -ItemType Directory -Path $output -Force | Out-Null

$temp = Join-Path ([IO.Path]::GetTempPath()) ('m2-package-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temp -Force | Out-Null
try {
    $entries = @(Get-Content -LiteralPath $listPath | ForEach-Object { $_.Trim() } |
        Where-Object { $_ -and -not $_.StartsWith('#') })
    if ($entries.Count -eq 0) { throw 'The file list is empty.' }

    # A line may name a directory with a wildcard - "…/src/playerbot_*" - and it
    # expands to whatever is there. Splitting a source file used to mean editing
    # this list too, and forgetting to is what shipped a manager without its own
    # headers twice. A pattern that matches nothing is still an error: it means
    # the tree moved and the package would be silently short.
    # Python leaves bytecode beside every module it has run: a syntax check
    # (py_compile) or a test run of a panel drops __pycache__\*.pyc into the
    # tree, and a wildcard line takes whatever is on disk. Two server packages
    # carried it to players - three .pyc files in 2.0.46, and the seban panel's
    # app and collector bytecode in the first build of 2.0.48, caught by a zip
    # check and rebuilt by hand. Bytecode is never a source, so every wildcard
    # or directory expansion leaves it out; a file named on a line of its own is
    # still published exactly as named.
    function Test-M2PythonBytecode([string]$Relative) {
        $parts = @($Relative.Replace('/', '\').Split('\'))
        if ($parts -contains '__pycache__') { return $true }
        $extension = [IO.Path]::GetExtension($Relative)
        return ($extension -ieq '.pyc' -or $extension -ieq '.pyo')
    }
    $skippedBytecode = 0

    $expanded = @()
    foreach ($entry in $entries) {
        if ($entry -notmatch '[\*\?]') { $expanded += $entry; continue }
        $relativePattern = $entry.Replace('/', '\').TrimStart('\')
        if ([IO.Path]::IsPathRooted($relativePattern) -or
            $relativePattern.Split('\') -contains '..') {
            throw "Unsafe pattern: $entry"
        }
        $directory = Split-Path -Parent $relativePattern
        $leaf = Split-Path -Leaf $relativePattern
        $searchRoot = Join-Path $source $directory
        if (-not (Test-Path -LiteralPath $searchRoot -PathType Container)) {
            throw "Pattern directory does not exist: $directory"
        }
        # A bare "*" is taken to mean the whole tree under that directory, so a
        # line can name a folder of assets without naming the shape of it.
        # files/static holds only subdirectories, and listing
        # static/skill_icons/*.png instead would have worked exactly until the
        # second icon set - which is the trap the playerbot sources were pulled
        # out of.
        if ($leaf -eq '*') {
            $matched = @(Get-ChildItem -LiteralPath $searchRoot -File -Recurse |
                Sort-Object FullName | ForEach-Object {
                    Join-Path $directory $_.FullName.Substring($searchRoot.Length).TrimStart('\\')
                })
        } else {
            $matched = @(Get-ChildItem -LiteralPath $searchRoot -File -Filter $leaf |
                Sort-Object Name | ForEach-Object { (Join-Path $directory $_.Name) })
        }
        $kept = @($matched | Where-Object { -not (Test-M2PythonBytecode $_) })
        $skippedBytecode += $matched.Count - $kept.Count
        if ($kept.Count -eq 0) {
            if ($matched.Count -gt 0) { throw "Pattern matched only Python bytecode: $entry" }
            throw "Pattern matched no files: $entry"
        }
        $expanded += $kept
    }
    $entries = @($expanded | Select-Object -Unique)
    Write-Host "Skipped Python bytecode (__pycache__, .pyc, .pyo) under wildcard lines: $skippedBytecode file(s)"
    $explicitBytecode = @($entries | Where-Object { Test-M2PythonBytecode $_ })
    if ($explicitBytecode.Count -gt 0) {
        Write-Warning ("Python bytecode named on a line of its own is published as named: " + ($explicitBytecode -join ', '))
    }

    # Published name for a listed (source) path: the first matching prefix of
    # the map, forward slashes either way.
    function Get-PublishedPath([string]$Relative) {
        $normal = $Relative.Replace('\', '/').TrimStart('/')
        foreach ($prefix in @($PathMap.Keys | Sort-Object { $_.Length } -Descending)) {
            $from = ([string]$prefix).Replace('\', '/')
            if ($normal.StartsWith($from, [StringComparison]::OrdinalIgnoreCase)) {
                return ([string]$PathMap[$prefix]).Replace('\', '/') + $normal.Substring($from.Length)
            }
        }
        return $normal
    }
    # Source path (as listed) for a published name; the pairing checks hash
    # the sources by the names they will be published under.
    $sourceOf = @{}
    foreach ($e in $entries) { $sourceOf[(Get-PublishedPath $e)] = $e }
    $published = @($sourceOf.Keys)

    # A server package has to carry the installation's own VERSION, at the
    # root, and this is exactly where that gets lost. The mt2009 tree is
    # published under another name, so a PathMap holding only the directory
    # prefix sends VERSION to linux-port/VERSION - a path nothing reads.
    # tools/update.sh reads <root>/VERSION twice over: to report what is
    # installed, and to decide whether there is anything to install at all. So
    # the number never moved, the updater announced the previous version after
    # a successful update, and every later run downloaded and unpacked the same
    # release again ("drugi raz robie aktualizacje z 2.0.34 do 2.0.35 i drugi
    # raz komunikat ... version 2.0.34", Mkls, 13 September). The map that does
    # this right lives in linux-port-mt2009/README.md and in
    # New-M2DeployTree.ps1; this is what stops a release being built from a
    # half-remembered one.
    if ($Type -eq 'server' -and -not ($published -contains 'VERSION')) {
        throw ("This server package would carry no VERSION at its root, so the " +
               "installation would keep reporting the version it already had and " +
               "its updater would re-install this release on every run. Add the " +
               "file to the list, or - on the mt2009 line, which publishes under " +
               "another name - give it its own PathMap row: " +
               "'linux-port-mt2009/VERSION' = 'VERSION'.")
    }


    # The overlay sources and the staged build context are two copies of the
    # same files, and a package that carries one without the other is what took
    # every player's server down in 1.22.4 and again in 1.23.2: the compiler saw
    # a manager whose header was still the previous release's, or missing
    # outright. Refuse to build such a package at all.
    $overlayPrefix = 'linux-port\overlays\playerbot\src\game\src\'
    $stagedPrefix = 'linux-port\docker\game\src\server\game\src\'
    $overlayNames = @()
    $stagedNames = @()
    foreach ($relativeInput in $published) {
        $relative = $relativeInput.Replace('/', '\').TrimStart('\')
        if ($relative.StartsWith($overlayPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            $overlayNames += $relative.Substring($overlayPrefix.Length)
        }
        elseif ($relative.StartsWith($stagedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            $stagedNames += $relative.Substring($stagedPrefix.Length)
        }
    }

    # The seed has the same two-copy shape as the sources: the overlay's is the
    # source of truth and the migrate container mounts the other one.
    # Listed with forward slashes, the way the file list writes them, and only
    # turned into a path when one is needed.
    $seedOverlay = 'linux-port/overlays/playerbot/sql/playerbots_seed.sql'
    $seedMounted = 'linux-port/docker/mariadb/playerbot/playerbots_seed.sql'
    $shipsOverlaySeed = $published -contains $seedOverlay
    $shipsMountedSeed = $published -contains $seedMounted
    # A tree that names its engine (the mt2009 one publishes linux-port/docker/
    # ENGINE) renders its seed from the overlay's at port time, and the launcher's
    # Sync-M2PlayerbotOverlay never copies the overlay's over it - so there the
    # mounted copy alone is the whole story, and the overlay's would be wrong.
    $shipsEngineMarker = $published -contains 'linux-port/docker/ENGINE'
    if ($shipsOverlaySeed -or ($shipsMountedSeed -and -not $shipsEngineMarker)) {
        if (-not ($shipsOverlaySeed -and $shipsMountedSeed)) {
            throw "The seed ships in only one of its two locations. Add both $seedOverlay and $seedMounted to $listPath."
        }
        $seedOverlayPath = Join-Path $source ($sourceOf[$seedOverlay] -replace '/', [IO.Path]::DirectorySeparatorChar)
        $seedMountedPath = Join-Path $source ($sourceOf[$seedMounted] -replace '/', [IO.Path]::DirectorySeparatorChar)
        if ((Get-FileHash -LiteralPath $seedOverlayPath -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $seedMountedPath -Algorithm SHA256).Hash) {
            throw "The overlay seed and the seed the migrate container mounts differ. Copy it across before packaging."
        }
    }

    foreach ($name in $stagedNames) {
        # An engine file shipped as its patched copy - item_manager.cpp,
        # config.cpp - has no overlay source and the launcher's sync never
        # touches it: Sync-M2PlayerbotOverlay copies what sits in the overlay
        # directory, which is only ever playerbot_*. The pairing rule below is
        # for the overlay sources alone.
        if ($name -notlike 'playerbot_*') { continue }
        if ($overlayNames -notcontains $name) {
            # The launcher syncs overlay -> build context before every build, so
            # an overlay copy left behind by an older release would overwrite the
            # good staged one on the player's machine. Both halves ship together
            # or neither does.
            throw "Build-context copy shipped without its Playerbot source: $name. Add $overlayPrefix$name to $listPath."
        }
    }
    foreach ($name in $overlayNames) {
        if ($stagedNames -notcontains $name) {
            throw "Playerbot source shipped without its build-context copy: $name. Add $stagedPrefix$name to $listPath."
        }
        $a = Join-Path $source $sourceOf[($overlayPrefix + $name).Replace('\', '/')]
        $b = Join-Path $source $sourceOf[($stagedPrefix + $name).Replace('\', '/')]
        if ((Get-FileHash -LiteralPath $a -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $b -Algorithm SHA256).Hash) {
            throw "Playerbot source and its build-context copy differ: $name. Run prepare-context.sh or copy it across before packaging."
        }
    }

    foreach ($relativeInput in $entries) {
        $relative = $relativeInput.Replace('/', '\').TrimStart('\')
        if ([IO.Path]::IsPathRooted($relative) -or $relative.Split('\') -contains '..') {
            throw "Unsafe relative path: $relativeInput"
        }
        if ((Get-PublishedPath $relative) -ieq 'linux-port/docker/.env' -or $relative.StartsWith('.git\')) {
            throw "Protected file cannot be published in an update: $relative"
        }
        # PowerShell's automatic pipeline-enumerator variable used to be
        # reused here as an ordinary name. Handing it to a cmdlet made
        # packaging hang at random, with the process sitting fully idle.
        $sourceFile = [IO.Path]::GetFullPath((Join-Path $source $relative))
        if (-not $sourceFile.StartsWith($source + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw "Path leaves source root: $relative"
        }
        if (-not (Test-Path -LiteralPath $sourceFile -PathType Leaf)) {
            throw "Listed file does not exist: $relative"
        }
        $destination = Join-Path $temp ((Get-PublishedPath $relative).Replace('/', '\'))
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Copy-Item -LiteralPath $sourceFile -Destination $destination -Force
    }

    $safeVersion = $Version -replace '[^A-Za-z0-9._-]', '-'
    $zipName = "metin2-$Type-update-$safeVersion.zip"
    $zipPath = Join-Path $output $zipName
    Compress-Archive -Path (Join-Path $temp '*') -DestinationPath $zipPath -CompressionLevel Optimal -Force
    $hash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToUpperInvariant()
    $component = [ordered]@{
        version = $Version
        url = $DownloadUrl
        sha256 = $hash
        size = (Get-Item -LiteralPath $zipPath).Length
    }
    $fragmentPath = Join-Path $output ("$Type-manifest-fragment-$safeVersion.json")
    $component | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $fragmentPath -Encoding UTF8

    Write-Host "Created: $zipPath"
    Write-Host "SHA-256: $hash"
    Write-Host "Manifest fragment: $fragmentPath"
}
finally {
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
}
