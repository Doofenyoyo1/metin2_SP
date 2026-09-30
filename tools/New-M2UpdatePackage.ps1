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
    # The updaters compare the installed VERSION with the manifest's version
    # for equality: a zip whose VERSION says something else than -Version is
    # installed again on every run, or never.
    if ($Type -eq 'server') {
        $versionSource = Join-Path $source (($sourceOf['VERSION']) -replace '/', [IO.Path]::DirectorySeparatorChar)
        $packagedVersion = ([IO.File]::ReadAllText($versionSource)).Trim()
        if ($packagedVersion -ne $Version.Trim()) {
            throw "VERSION in the source says '$packagedVersion' but -Version is '$Version'. Put the new version into VERSION first."
        }
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

    # MT2009 Plus ships its engine changes the way Tieru ships his: patched
    # once, at release time (tools\port\Apply-MT2009PlusEngine.ps1), and
    # carried in the zip - a player's start-server.ps1 no longer patches
    # anything. An engine file listed here without its marks would take the
    # change away from every player who installs the package.
    if ($Type -eq 'server') {
        $engineMarks = [ordered]@{
            'linux-port/docker/game/src/server/game/src/item_manager.cpp' = @(
                'MT2009_PLUS_BOT_RARE_DROP_V1', 'MT2009_PLUS_BOT_RARE_DROP_V2', 'MT2009_PLUS_BOT_RARE_DROP_V3',
                'MT2009_PLUS_RARE_LEVEL_V1', 'MT2009_PLUS_DROP_PREVIEW_MIN_V1', 'MT2009_PLUS_RARE_TOGGLE_V1', 'MT2009_PLUS_BOT_SASH_DROP_V1', 'MT2009_PLUS_PREVIEW_RARE_V1', 'MT2009_PLUS_LOOT_EVENTS_V1', 'MT2009_PLUS_DUNGEON_DROP_V1 (boss)', 'MT2009_PLUS_DUNGEON_DROP_V1 (specials)', 'MT2009_PLUS_DUNGEON_DROP_V1 (specials end)', 'MT2009_PLUS_DUNGEON_DROP_V1 (preview book)', 'MT2009_PLUS_DUNGEON_DROP_V1 (preview)')
            'linux-port/docker/game/src/server/game/src/ikarus_shop_manager.cpp' = @('MT2009_PLUS_SHOP_SEARCH_ITEM_V1', 'MT2009_PLUS_SHOP_SEARCH_PLUS_V1 (bound)', 'MT2009_PLUS_SALE_ONCE_V1')
            'linux-port/docker/game/src/server/game/src/PetSystem.cpp' = @('MT2009_PLUS_PET_STAYS_ON_DEATH_V1')
            'linux-port/docker/game/src/server/game/src/input_auth.cpp' = @('MT2009_PLUS_LOGIN_UNDERSCORE_V1')
            'linux-port/docker/game/src/server/game/src/char_item.cpp' = @('IsStackableCorDraconisVnum', 'MT2009_PLUS_RARE_TOGGLE_V1', 'MT2009_PLUS_COR_AUTOSTACK_V1', 'MT2009_PLUS_DS_TRACE_PLAYERS_V1', 'MT2009_PLUS_BOT_SASH_DROP_V1', 'MT2009_PLUS_COR_PARTY_PICKUP_V1', 'MT2009_PLUS_PICKUP_FILTER_V1 (collect)', 'MT2009_PLUS_MOUNT_ON_EQUIP_V1 (ride)', 'MT2009_PLUS_BATTLE_PASS_V1 (use)', 'MT2009_PLUS_BATTLE_PASS_V1 (refine try)', 'MT2009_PLUS_BATTLE_PASS_V1 (refine try scroll)', 'MT2009_PLUS_NEW_PET_V1 (use)', 'MT2009_PLUS_GOBLIN_V1 (use)', 'MT2009_PLUS_GOBLIN_V1 (refine free)', 'MT2009_PLUS_PICKUP_FILTER_V1 (party)')
            'linux-port/docker/game/src/server/game/src/pvp.cpp' = @('MT2009_PLUS_GOBLIN_V1 (attack)')
            'linux-port/docker/game/src/server/game/src/input_main.cpp' = @('MT2009_PLUS_SPEEDHACK_CLOCK_V1', 'MT2009_PLUS_FLEA_FILL_V1', 'MT2009_PLUS_BATTLE_PASS_V1 (shout)')
            'linux-port/docker/game/src/server/game/src/dragon_soul_table.cpp' = @('MT2009_PLUS_DS_APPLYS_V1')
            'linux-port/docker/game/src/server/game/src/char_affect.cpp' = @('MT2009_PLUS_DS_QUALIFY_ON_LOGIN_V1', 'MT2009_PLUS_COSTUME_SET_V1 (no save)')
            'linux-port/docker/game/src/server/game/src/cmd_general.cpp' = @('ACMD(do_autohunt_target)', 'ACMD(do_autohunt_loot)', 'MT2009_PLUS_GARBAGE_BATCH_V1', 'MT2009_PLUS_PICKUP_FILTER_V1 (commands)', 'MT2009_PLUS_ARRANGE_MERGE_V1', 'MT2009_PLUS_COSTUME_HIDE_V1 (command)', 'MT2009_PLUS_EVENT_CALENDAR_V1 (command)', 'MT2009_PLUS_BATTLE_PASS_V1 (command)', 'MT2009_PLUS_WHEEL_V1 (command)', 'MT2009_PLUS_GUILD_DUTY_V1 (command)', 'MT2009_PLUS_GOBLIN_V1 (command)')
            'linux-port/docker/game/src/server/game/src/cmd_gm.cpp' = @('MT2009_PLUS_DS_PLAYER_CMD_V1', 'MT2009_PLUS_FLEA_SALES_V1')
            'linux-port/docker/game/src/server/game/src/char.cpp' = @('MT2009_PLUS_MAGIC_ATT_PER_V1', 'MT2009_PLUS_SADDLEBAG_MOUNT_V1', 'MT2009_PLUS_STONE_STILL_V1', 'MT2009_PLUS_BATTLE_PASS_V1 (stat)', 'MT2009_PLUS_NEW_PET_V1 (points)')
            'linux-port/docker/game/src/server/game/src/char_state.cpp' = @('MT2009_PLUS_STONE_STILL_V1')
            'linux-port/docker/game/src/server/game/src/char_skill.cpp' = @('MT2009_PLUS_DRAGON_ROAR_TARGET_V1', 'MT2009_PLUS_MOUNT_HORSE_SKILLS_V1', 'MT2009_PLUS_DRAGON_ROAR_EFFECT_V1', 'MT2009_PLUS_SKILL_DURATION_V1', 'MT2009_PLUS_SKILL_DURATION_V1 (at position)', 'MT2009_PLUS_SKILL_DURATION_V1 (compute)')
            'linux-port/docker/game/src/server/game/src/regen.cpp' = @('MT2009_PLUS_EASTER_METIN_CAP_V1', 'MT2009_PLUS_EASTER_METIN_CAP_V1 (spawn)')
            'linux-port/docker/game/src/server/game/src/char_player.cpp' = @('MT2009_PLUS_MOUNT_SPEED_V1')
            'linux-port/docker/game/src/server/game/src/packet.h' = @('MT2009_PLUS_DUNGEON_ONE_WARP_V1 (header)', 'MT2009_PLUS_DUNGEON_ONE_WARP_V1 (packet)')
            'linux-port/docker/game/src/server/game/src/packet_info.cpp' = @('MT2009_PLUS_DUNGEON_ONE_WARP_V1 (info)')
            'linux-port/docker/game/src/server/game/src/input_p2p.cpp' = @('MT2009_PLUS_DUNGEON_ONE_WARP_V1 (decl)', 'MT2009_PLUS_DUNGEON_ONE_WARP_V1 (case)', 'MT2009_PLUS_SHOUTERS_V1 (p2p)')
            'linux-port/docker/game/src/server/game/src/questlua_dungeon.cpp' = @('MT2009_PLUS_DUNGEON_ONE_WARP_V1 (lua)', 'MT2009_PLUS_DUNGEON_ONE_WARP_V1 (table)')
            'linux-port/docker/game/src/server/game/src/dungeon.cpp' = @('MT2009_PLUS_DUNGEON_RETURN_V1')
            'linux-port/docker/game/src/server/db/src/ClientManagerIkarusShop.cpp' = @('MT2009_PLUS_SHOP_LOCK_OWNER_V1 (map)', 'MT2009_PLUS_SHOP_LOCK_OWNER_V1 (lock)', 'MT2009_PLUS_SHOP_LOCK_OWNER_V1 (settle)')
            'linux-port/docker/game/src/server/common/CommonDefines.h' = @('MT2009_PLUS_MAP_ALLOW_48_V1')
            'linux-port/docker/game/src/server/game/src/char_battle.cpp' = @('MT2009_PLUS_BOT_RARE_SHARE_V1', 'MT2009_PLUS_MOUNT_DEATH_UNEQUIP_V1', 'MT2009_PLUS_BATTLE_PASS_V1 (kill)', 'MT2009_PLUS_NEW_PET_V1 (exp given)', 'MT2009_PLUS_BATTLE_PASS_V1 (kill share)')
            'linux-port/docker/game/src/server/game/src/MountSystem.cpp' = @('MT2009_PLUS_MOUNT_BONUS_ONCE_V1', 'MT2009_PLUS_MOUNT_PERMANENT_V1')
            'linux-port/docker/game/src/server/game/src/item.cpp' = @('MT2009_PLUS_COSTUME_HIDE_V1 (hook)', 'MT2009_PLUS_COSTUME_SET_V1 (hook)', 'MT2009_PLUS_COSTUME_SET_V1 (sets)')
            'linux-port/docker/game/src/server/game/src/cmd.cpp' = @('"autohunt_target"', '"autohunt_loot"', 'MT2009_PLUS_DS_PLAYER_CMD_V1', '"chest_preview"', 'MT2009_PLUS_PICKUP_FILTER_V1 (table)', 'MT2009_PLUS_COSTUME_HIDE_V1 (table)', 'MT2009_PLUS_EVENT_CALENDAR_V1 (table)', 'MT2009_PLUS_BATTLE_PASS_V1 (table)', 'MT2009_PLUS_WHEEL_V1 (table)', 'MT2009_PLUS_NEW_PET_V1 (table)', 'MT2009_PLUS_GUILD_DUTY_V1 (table)', 'MT2009_PLUS_GOBLIN_V1 (table)')
        }
        foreach ($enginePublished in $engineMarks.Keys) {
            if (-not ($published -contains $enginePublished)) { continue }
            $engineText = [IO.File]::ReadAllText((Join-Path $source ($sourceOf[$enginePublished] -replace '/', [IO.Path]::DirectorySeparatorChar)))
            foreach ($mark in $engineMarks[$enginePublished]) {
                if (-not $engineText.Contains($mark)) {
                    throw "$enginePublished has no $mark. Run tools\port\Apply-MT2009PlusEngine.ps1 -ServerRoot $source first."
                }
            }
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
        # Windows PowerShell 5.1 reads a script without a byte order mark as
        # the ANSI code page: "ł" turns into "Ĺ‚", whose second character is a
        # quotation mark to the parser, and the whole module fails to load -
        # the launcher then cannot even install the update that would fix it
        # (MT2009 Plus 2.2.3, launcher/Metin2Launcher.psm1).
        if ($relative -match '\.(ps1|psm1)$') {
            $scriptBytes = [IO.File]::ReadAllBytes($sourceFile)
            $hasBom = $scriptBytes.Length -ge 3 -and $scriptBytes[0] -eq 0xEF -and $scriptBytes[1] -eq 0xBB -and $scriptBytes[2] -eq 0xBF
            if (-not $hasBom -and @($scriptBytes | Where-Object { $_ -gt 127 }).Count -gt 0) {
                throw "PowerShell script has non-ASCII characters but no UTF-8 BOM: $relative. Save it as 'UTF-8 with BOM' before packaging."
            }
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
