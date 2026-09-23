# A .env zero-filled by a crash (start-server.ps1): the durable write, the copy
# the last start left, and the values read back from the containers - with
# Docker replaced by canned answers, so nothing here touches a real engine or
# a real installation. Run with Windows PowerShell 5.1, the launcher's engine:
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests\start_server_env_repair_test.ps1
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# The script runs a whole start when it is invoked, so its functions are taken
# out of it one by one and nothing else.
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile((Join-Path $root 'start-server.ps1'), [ref]$null, [ref]$errors)
if ($errors.Count) { throw 'start-server.ps1 does not parse' }
foreach ($fn in $ast.EndBlock.Statements | Where-Object { $_ -is [System.Management.Automation.Language.FunctionDefinitionAst] }) {
    . ([scriptblock]::Create($fn.Extent.Text))
}

# Docker: what the test says it holds.
$script:containers = @()
$script:volumes = @()
function Invoke-DockerQuery {
    param([string[]]$Arguments)
    if ($Arguments[0] -eq 'ps') {
        $ids = @($script:containers | ForEach-Object { $_.Id })
        return [pscustomobject]@{ ExitCode = 0; Output = ($ids -join "`n") }
    }
    if ($Arguments[0] -eq 'inspect') {
        $c = @($script:containers | Where-Object { $_.Id -eq $Arguments[1] })
        if (-not $c.Count) { return [pscustomobject]@{ ExitCode = 1; Output = 'No such object' } }
        return [pscustomobject]@{ ExitCode = 0; Output = ('[' + $c[0].Json + ']') }
    }
    if ($Arguments[0] -eq 'volume') {
        $exists = $script:volumes -contains $Arguments[2]
        return [pscustomobject]@{ ExitCode = $(if ($exists) { 0 } else { 1 }); Output = '' }
    }
    return [pscustomobject]@{ ExitCode = 1; Output = 'unexpected' }
}
function Container {
    param([string]$Id, [string]$Service, [string[]]$Env, [string]$HostIp = '')
    $bindings = if ($HostIp) { '"11000/tcp":[{"HostIp":"' + $HostIp + '","HostPort":"11000"}]' } else { '' }
    $json = '{"Id":"' + $Id + '","Config":{"Env":[' + (($Env | ForEach-Object { '"' + $_ + '"' }) -join ',') +
        '],"Labels":{"com.docker.compose.project":"m2pb-test","com.docker.compose.service":"' + $Service + '"}},' +
        '"HostConfig":{"PortBindings":{' + $bindings + '}}}'
    return [pscustomobject]@{ Id = $Id; Json = $json }
}

$script:failed = 0
$script:passed = 0
function Check {
    param([string]$Name, [bool]$Ok, [string]$Detail = '')
    if ($Ok) { $script:passed++ }
    else { $script:failed++; Write-Host ("FAIL {0} {1}" -f $Name, $Detail) }
}
function Value { param([string]$Text, [string]$Name) return (Get-DotEnvValue -Content $Text -Name $Name) }

$work = Join-Path ([IO.Path]::GetTempPath()) ('m2envtest-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $work | Out-Null
try {
    Copy-Item -LiteralPath (Join-Path $root 'linux-port-mt2009\docker\.env.example') -Destination (Join-Path $work '.env.example')
    $envPath = Join-Path $work '.env'
    $utf8 = [Text.UTF8Encoding]::new($false)
    function Damage {
        # What Greess's file was: the old bytes turned to zeros, and after them
        # what an older launcher appended from the example to a file it could
        # no longer read - a fresh panel password among it.
        param([string]$Tail)
        $zeros = New-Object byte[] 4096
        [IO.File]::WriteAllBytes($envPath, $zeros + $utf8.GetBytes($Tail))
        Remove-Item -LiteralPath ($envPath + '.last-good') -ErrorAction SilentlyContinue
        Get-ChildItem -LiteralPath $work -Filter '.env.damaged-*' | Remove-Item
    }
    $tail = "M2_COMPOSE_PROJECT_NAME=m2pb-test`nPLAYERBOT_AUTOSPAWN_COUNT=350`nM2_HOST_BIND_ADDRESS=0.0.0.0`nM2_PANEL_PASSWORD=freshlymade`n"

    # ---- the durable write: exact bytes, no temporary left, over a file and anew
    Write-FileDurable -Path $envPath -Content "A=1`n"
    Write-FileDurable -Path $envPath -Content "A=2`nB=3`n"
    Check 'durable write replaces' ([IO.File]::ReadAllText($envPath) -eq "A=2`nB=3`n")
    Check 'no temporary left' (-not (Test-Path -LiteralPath ($envPath + '.tmp')))
    Check 'no byte order mark' ([IO.File]::ReadAllBytes($envPath)[0] -eq [byte][char]'A')

    # ---- a sound file is left alone
    Repair-DotEnvAfterCrash -EnvPath $envPath -Project 'm2pb-test'
    Check 'sound file untouched' ([IO.File]::ReadAllText($envPath) -eq "A=2`nB=3`n")
    Check 'sound file: no damaged copy' (@(Get-ChildItem -LiteralPath $work -Filter '.env.damaged-*').Count -eq 0)

    # ---- zero-filled, with the copy the last start left: the copy goes back
    Damage $tail
    Write-FileDurable -Path ($envPath + '.last-good') -Content "M2_DB_ROOT_PASSWORD=goodroot`nM2_DB_PASSWORD=gooduser`nPLAYERBOT_AUTOSPAWN_COUNT=900`n"
    Repair-DotEnvAfterCrash -EnvPath $envPath -Project 'm2pb-test'
    $text = [IO.File]::ReadAllText($envPath)
    Check 'last-good restored' ((Value $text 'M2_DB_ROOT_PASSWORD') -eq 'goodroot' -and (Value $text 'PLAYERBOT_AUTOSPAWN_COUNT') -eq '900')
    Check 'damaged copy kept' (@(Get-ChildItem -LiteralPath $work -Filter '.env.damaged-*').Count -eq 1)

    # ---- a last-good copy without the passwords is not trusted
    Damage $tail
    Write-FileDurable -Path ($envPath + '.last-good') -Content "PLAYERBOT_AUTOSPAWN_COUNT=900`n"
    $script:containers = @(
        (Container 'db1' 'mariadb' @('MARIADB_ROOT_PASSWORD=rootpw', 'M2_DB_PASSWORD=userpw', 'PATH=/usr/bin', 'TZ=Europe/Warsaw')),
        (Container 'game1' 'game' @('PLAYERBOT_AUTOSPAWN_COUNT=700', 'M2_DB_PASSWORD=userpw', 'M2_PANEL_PASSWORD=theoldone', 'NOT_IN_EXAMPLE=1') '127.0.0.1')
    )
    Repair-DotEnvAfterCrash -EnvPath $envPath -Project 'm2pb-test'
    $text = [IO.File]::ReadAllText($envPath)
    Check 'containers: root password under the image name' ((Value $text 'M2_DB_ROOT_PASSWORD') -eq 'rootpw')
    Check 'containers: user password' ((Value $text 'M2_DB_PASSWORD') -eq 'userpw')
    Check 'containers win over the appended defaults' ((Value $text 'PLAYERBOT_AUTOSPAWN_COUNT') -eq '700')
    Check 'the old panel password back' ((Value $text 'M2_PANEL_PASSWORD') -eq 'theoldone')
    Check 'bind address from the published ports' ((Value $text 'M2_HOST_BIND_ADDRESS') -eq '127.0.0.1')
    Check 'the zone under its container name' ((Value $text 'M2_TZ') -eq 'Europe/Warsaw')
    Check 'nothing outside the example' (-not ($text -match '(?m)^NOT_IN_EXAMPLE=') -and -not ($text -match '(?m)^PATH='))
    Check 'the project line survived' ((Value $text 'M2_COMPOSE_PROJECT_NAME') -eq 'm2pb-test')
    Check 'no zero left' (-not (Test-FileZeroFilled -Bytes ([IO.File]::ReadAllBytes($envPath))))

    # ---- the project read from the surviving line when the caller has none
    Damage $tail
    Repair-DotEnvAfterCrash -EnvPath $envPath -Project ''
    Check 'project from the file' ((Value ([IO.File]::ReadAllText($envPath)) 'M2_DB_PASSWORD') -eq 'userpw')

    # ---- no containers, a database that exists: stop, and touch nothing
    Damage $tail
    $script:containers = @()
    $script:volumes = @('m2pb-test_db-data')
    $threw = $false
    try { Repair-DotEnvAfterCrash -EnvPath $envPath -Project 'm2pb-test' } catch { $threw = $true }
    Check 'refuses to invent passwords for a database that exists' $threw
    Check 'damaged file left for the player' (Test-FileZeroFilled -Bytes ([IO.File]::ReadAllBytes($envPath)))

    # ---- no containers and no database: a fresh pair
    Damage $tail
    $script:volumes = @()
    Repair-DotEnvAfterCrash -EnvPath $envPath -Project 'm2pb-test'
    $text = [IO.File]::ReadAllText($envPath)
    Check 'fresh root password' ((Value $text 'M2_DB_ROOT_PASSWORD') -match '^[0-9a-f]{48}$')
    Check 'fresh user password' ((Value $text 'M2_DB_PASSWORD') -match '^[0-9a-f]{48}$')
}
finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ("start_server_env_repair_test: {0} passed, {1} failed" -f $script:passed, $script:failed)
if ($script:failed) { exit 1 }
