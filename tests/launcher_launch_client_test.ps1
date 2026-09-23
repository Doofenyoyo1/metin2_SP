# The PLAY button's "start the client too" switch (launcher.config.json,
# launchClientOnPlay). It is a bool in a file whose every other field is a
# string, which is exactly where it can go wrong: the loading loop casts to
# [string], and "False" is a non-empty string.
# Run with Windows PowerShell 5.1, the launcher's engine:
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests\launcher_launch_client_test.ps1
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$root = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $root 'launcher\Metin2Launcher.psm1') -Force

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

$work = Join-Path ([System.IO.Path]::GetTempPath()) ('m2play-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
$configPath = Join-Path $work 'launcher.config.json'

try {
    Write-Host '== a fresh install starts the client, as the button always has =='
    $config = Get-M2LauncherConfig -ServerRoot $work -ConfigPath $configPath
    Check 'the default is on' $true $config.launchClientOnPlay

    Write-Host '== switching it off survives a save and a load =='
    $config.launchClientOnPlay = $false
    Save-M2LauncherConfig -Config $config -ConfigPath $configPath
    $again = Get-M2LauncherConfig -ServerRoot $work -ConfigPath $configPath
    Check 'still off after a reload' $false $again.launchClientOnPlay
    Check 'and it is written to the file' $true ((Get-Content -LiteralPath $configPath -Raw) -match 'launchClientOnPlay')

    Write-Host '== and switching it back on =='
    $again.launchClientOnPlay = $true
    Save-M2LauncherConfig -Config $again -ConfigPath $configPath
    Check 'on after a reload' $true (Get-M2LauncherConfig -ServerRoot $work -ConfigPath $configPath).launchClientOnPlay

    Write-Host '== a config written before this switch existed =='
    # The field simply is not there, and the default has to answer for it.
    Set-Content -LiteralPath $configPath -Encoding UTF8 -Value (
        '{"schema":1,"manifestUrl":"https://example/m.json","clientRoot":"","clientExecutable":"","supportUploadUrl":"","language":"pl"}')
    Check 'an older config starts the client' $true (Get-M2LauncherConfig -ServerRoot $work -ConfigPath $configPath).launchClientOnPlay

    Write-Host '== the string "False" is not a yes =='
    # The loading loop casts every other field to [string]; a bool that went
    # through it would come back as the non-empty string "False" and read as on.
    Set-Content -LiteralPath $configPath -Encoding UTF8 -Value (
        '{"schema":1,"manifestUrl":"https://example/m.json","clientRoot":"","clientExecutable":"","supportUploadUrl":"","language":"pl","launchClientOnPlay":false}')
    $loaded = Get-M2LauncherConfig -ServerRoot $work -ConfigPath $configPath
    Check 'false stays false' $false $loaded.launchClientOnPlay
    Check 'and is a real boolean' 'System.Boolean' $loaded.launchClientOnPlay.GetType().FullName

    Write-Host ''
    if ($script:fail -eq 0) {
        Write-Host ("PASS=" + $script:pass + " FAIL=0") -ForegroundColor Green
    } else {
        Write-Host ("PASS=" + $script:pass + " FAIL=" + $script:fail) -ForegroundColor Red
        exit 1
    }
} finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
