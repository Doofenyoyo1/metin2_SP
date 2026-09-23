# The one-shot switch of an existing .env to the unified world layout
# (start-server.ps1, Assert-WorldLayoutDefault). The same shape as the three
# kingdoms and the clock's zone before it: the default is written once, the
# marker says it was, and a world too big for one core is left on split.
# Run with Windows PowerShell 5.1, the launcher's engine:
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests\start_server_world_layout_test.ps1
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$root = Split-Path -Parent $PSScriptRoot

# The script runs a whole start when it is invoked, so its functions are taken
# out of it one by one and nothing else.
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile((Join-Path $root 'start-server.ps1'), [ref]$null, [ref]$errors)
if ($errors.Count) { throw 'start-server.ps1 does not parse' }
foreach ($fn in $ast.EndBlock.Statements | Where-Object { $_ -is [System.Management.Automation.Language.FunctionDefinitionAst] }) {
    . ([scriptblock]::Create($fn.Extent.Text))
}

$script:pass = 0
$script:fail = 0
function Check {
    param([string]$What, [string]$Expected, [string]$Actual)
    if ($Expected -eq $Actual) {
        Write-Host ("  OK   " + $What) -ForegroundColor Green
        $script:pass++
    } else {
        Write-Host ("  FAIL " + $What + ": expected [" + $Expected + "], got [" + $Actual + "]") -ForegroundColor Red
        $script:fail++
    }
}

$work = Join-Path ([System.IO.Path]::GetTempPath()) ('m2layout-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
function NewEnv {
    # The function is gated on the ENGINE marker beside the .env - this is the
    # mt2009 line's default and the r40250 tree keeps its own - so every case
    # gets a directory of its own with that marker in it.
    param([string]$Body, [string]$Engine = 'mt2009')
    $dir = Join-Path $work ([guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $dir | Out-Null
    Set-Content -LiteralPath (Join-Path $dir 'ENGINE') -Value $Engine -Encoding ASCII
    $path = Join-Path $dir '.env'
    Set-Content -LiteralPath $path -Value $Body -Encoding ASCII
    return $path
}
function Value {
    param([string]$Content, [string]$Name)
    $v = Get-DotEnvValue -Content $Content -Name $Name
    if ($null -eq $v) { return '' }
    return [string]$v
}

try {
    Write-Host '== a small world is switched to unified, once =='
    $envPath = NewEnv "M2_PLAYERBOT_WORLD_LAYOUT=split`nPLAYERBOT_AUTOSPAWN_COUNT=800`n"
    $content = [System.IO.File]::ReadAllText($envPath)
    $content = Assert-WorldLayoutDefault -Content $content -EnvPath $envPath
    Check 'layout is unified' 'unified' (Value $content 'M2_PLAYERBOT_WORLD_LAYOUT')
    Check 'the marker is written' '1' (Value $content 'M2_PLAYERBOT_WORLD_LAYOUT_DEFAULTED')

    Write-Host '== and the operator''s own choice after it is kept =='
    $content = $content -replace 'M2_PLAYERBOT_WORLD_LAYOUT=unified', 'M2_PLAYERBOT_WORLD_LAYOUT=split'
    $content = Assert-WorldLayoutDefault -Content $content -EnvPath $envPath
    Check 'a second run leaves split alone' 'split' (Value $content 'M2_PLAYERBOT_WORLD_LAYOUT')
    Check 'the marker is not doubled' 1 ([regex]::Matches($content, '(?m)^M2_PLAYERBOT_WORLD_LAYOUT_DEFAULTED=').Count)

    Write-Host '== a world too big for one core stays split =='
    $envPath = NewEnv "M2_PLAYERBOT_WORLD_LAYOUT=split`nPLAYERBOT_AUTOSPAWN_COUNT=2200`n"
    $content = [System.IO.File]::ReadAllText($envPath)
    $content = Assert-WorldLayoutDefault -Content $content -EnvPath $envPath
    Check '2200 bots stay on split' 'split' (Value $content 'M2_PLAYERBOT_WORLD_LAYOUT')
    Check 'but the migration is marked done' '1' (Value $content 'M2_PLAYERBOT_WORLD_LAYOUT_DEFAULTED')

    Write-Host '== an .env that never named the layout =='
    $envPath = NewEnv "PLAYERBOT_AUTOSPAWN_COUNT=500`n"
    $content = [System.IO.File]::ReadAllText($envPath)
    $content = Assert-WorldLayoutDefault -Content $content -EnvPath $envPath
    Check 'the key is added as unified' 'unified' (Value $content 'M2_PLAYERBOT_WORLD_LAYOUT')

    Write-Host '== the r40250 line keeps its own default =='
    $envPath = NewEnv "M2_PLAYERBOT_WORLD_LAYOUT=split`nPLAYERBOT_AUTOSPAWN_COUNT=800`n" 'r40250'
    $content = [System.IO.File]::ReadAllText($envPath)
    $content = Assert-WorldLayoutDefault -Content $content -EnvPath $envPath
    Check 'the other engine is left alone' 'split' (Value $content 'M2_PLAYERBOT_WORLD_LAYOUT')
    Check 'and gets no marker' '' (Value $content 'M2_PLAYERBOT_WORLD_LAYOUT_DEFAULTED')

    Write-Host '== an install with no ENGINE marker is left alone =='
    $envPath = NewEnv "M2_PLAYERBOT_WORLD_LAYOUT=split`n"
    Remove-Item -LiteralPath (Join-Path (Split-Path -Parent $envPath) 'ENGINE') -Force
    $content = [System.IO.File]::ReadAllText($envPath)
    $content = Assert-WorldLayoutDefault -Content $content -EnvPath $envPath
    Check 'nothing is written without the marker' 'split' (Value $content 'M2_PLAYERBOT_WORLD_LAYOUT')

    Write-Host '== a per-kingdom count is the world''s size too =='
    $envPath = NewEnv ("M2_PLAYERBOT_WORLD_LAYOUT=split`nPLAYERBOT_AUTOSPAWN_PER_KINGDOM=1`n" +
        "PLAYERBOT_AUTOSPAWN_SHINSOO=700`nPLAYERBOT_AUTOSPAWN_CHUNJO=700`nPLAYERBOT_AUTOSPAWN_JINNO=700`n")
    $content = [System.IO.File]::ReadAllText($envPath)
    $content = Assert-WorldLayoutDefault -Content $content -EnvPath $envPath
    Check '2100 bots over three kingdoms stay split' 'split' (Value $content 'M2_PLAYERBOT_WORLD_LAYOUT')

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
