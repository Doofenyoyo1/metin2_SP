# The client folder's executables, as the launcher puts them in order.
#
# An install made from an old full package kept that package's
# metin2client.exe - one that knows two inventory pages - under a root that
# knows four: pages III and IV drawn empty, the server's page IV in the
# equipment slots, nothing sold, what is put on lost (601210, kordianq1112,
# 27 September; upstream 2.2.27, our 2.2.30). Repair-M2ClientExecutables
# replaces such an exe from the manifest's "clientExe" and deletes the two
# stray executables the old full packages carried, pointing a launcher that
# started one back at metin2client.exe. Pinned here: only a known old build is
# replaced, only by a download whose SHA-256 is the manifest's, the old one is
# kept under the backups, a newer or somebody's own build is never touched, and
# the strays go only while metin2client.exe is there.
#
# Runs on Windows PowerShell 5.1 (the launcher's engine) and on PowerShell 7:
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests\launcher_client_exe_repair_test.ps1
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$root = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path (Join-Path $root "launcher") "Metin2Launcher.psm1") -Force

$script:pass = 0
$script:fail = 0
function Check {
    param([string]$What, $Expected, $Actual)
    if ([string]$Expected -eq [string]$Actual) {
        Write-Host ("  OK   " + $What) -ForegroundColor Green
        $script:pass++
    } else {
        Write-Host ("  FAIL " + $What + ": expected [" + $Expected + "], got [" + $Actual + "]") -ForegroundColor Red
        $script:fail++
    }
}

function Get-Sha {
    param([string]$Path)
    return ([string](Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash).ToUpperInvariant()
}

$work = Join-Path ([IO.Path]::GetTempPath()) ('m2-exe-repair-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
try {
    $client = Join-Path $work 'Klient'
    $backups = Join-Path $work 'backups'
    New-Item -ItemType Directory -Path $client | Out-Null
    $main = Join-Path $client 'metin2client.exe'
    $newExe = Join-Path $work 'new.exe'
    [IO.File]::WriteAllText($main, 'old two-page build')
    [IO.File]::WriteAllText($newExe, 'the current build')
    $oldHash = Get-Sha $main
    $newHash = Get-Sha $newExe

    Write-Host 'The manifest''s clientExe'
    Check 'no manifest, no component' $true ($null -eq (Get-M2ClientExeComponent -Manifest $null))
    $manifest = [pscustomobject]@{ server = [pscustomobject]@{ version = '2.2.30' } }
    Check 'an older manifest has none' $true ($null -eq (Get-M2ClientExeComponent -Manifest $manifest))
    $manifest | Add-Member -NotePropertyName clientExe -NotePropertyValue ([pscustomobject]@{ url = $newExe; sha256 = 'nope' })
    Check 'a malformed sum is none' $true ($null -eq (Get-M2ClientExeComponent -Manifest $manifest))
    $manifest.clientExe.sha256 = $newHash
    $component = Get-M2ClientExeComponent -Manifest $manifest
    Check 'a whole one is read' $newExe ([string]$component.url)

    Write-Host 'Old or not'
    Check 'a known old build is old' $true (Test-M2ClientExeOld -ClientFolder $client -OldHashes @($oldHash))
    Check 'anything else is left alone' $false (Test-M2ClientExeOld -ClientFolder $client -OldHashes @($newHash))
    Check 'the shipped list does not name the test build' $false (Test-M2ClientExeOld -ClientFolder $client)

    Write-Host 'A download that is not the manifest''s is refused'
    $bad = [pscustomobject]@{ url = $newExe; sha256 = ('0' * 64) }
    $notes = @(Repair-M2ClientExecutables -ClientFolder $client -ExeComponent $bad -BackupRoot $backups -OldHashes @($oldHash))
    Check 'the old exe stays' $oldHash (Get-Sha $main)
    Check 'and the note says why' $true ([bool](@($notes | Where-Object { $_ -match 'SHA-256' }).Count))

    Write-Host 'The old exe is replaced and kept'
    $notes = @(Repair-M2ClientExecutables -ClientFolder $client -ExeComponent $component -BackupRoot $backups -OldHashes @($oldHash))
    Check 'metin2client.exe is the current build' $newHash (Get-Sha $main)
    $kept = @(Get-ChildItem -LiteralPath $backups -Recurse -Filter 'metin2client.exe')
    Check 'one backup' 1 $kept.Count
    Check 'the backup is the old build' $oldHash (Get-Sha $kept[0].FullName)
    Check 'one note' 1 $notes.Count
    $notes = @(Repair-M2ClientExecutables -ClientFolder $client -ExeComponent $component -BackupRoot $backups -OldHashes @($oldHash))
    Check 'a current exe is not replaced again' 0 $notes.Count

    Write-Host 'The strays'
    $stray = Join-Path $client 'metin2client-2.0.13.exe'
    [IO.File]::WriteAllText($stray, 'stray')
    $configPath = Join-Path $work 'launcher.config.json'
    [IO.File]::WriteAllText($configPath, (@{ clientRoot = $client; clientExecutable = $stray } | ConvertTo-Json))
    $notes = @(Repair-M2ClientExecutables -ClientFolder $client -ServerRoot $work -ConfigPath $configPath)
    Check 'the stray is gone' $false (Test-Path -LiteralPath $stray)
    $config = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
    Check 'the launcher starts metin2client.exe' 'metin2client.exe' ([IO.Path]::GetFileName([string]$config.clientExecutable))
    Check 'two notes' 2 $notes.Count

    Remove-Item -LiteralPath $main
    [IO.File]::WriteAllText($stray, 'stray')
    $notes = @(Repair-M2ClientExecutables -ClientFolder $client -ServerRoot $work -ConfigPath $configPath)
    Check 'without metin2client.exe a stray stays' $true (Test-Path -LiteralPath $stray)
    Check 'and nothing is said' 0 $notes.Count
}
finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ''
Write-Host ("launcher_client_exe_repair_test: " + $script:pass + " ok, " + $script:fail + " failed")
if ($script:fail -gt 0) { exit 1 }
