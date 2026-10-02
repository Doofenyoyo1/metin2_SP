param(
    [Parameter(Position = 0)]
    [string]$Code
)

$ErrorActionPreference = 'Stop'

function Get-RequiredText {
    param($Object, [string]$Name)

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        throw "Kod nie zawiera pola '$Name'."
    }
    $value = [string]$property.Value
    if ([string]::IsNullOrWhiteSpace($value)) {
        throw "Pole '$Name' jest puste."
    }
    return $value
}

function Get-RequiredPort {
    param($Object, [string]$Name)

    $text = Get-RequiredText $Object $Name
    $value = 0
    if (-not [int]::TryParse($text, [ref]$value) -or $value -lt 1 -or $value -gt 65535) {
        throw "Pole '$Name' nie jest poprawnym portem (1-65535)."
    }
    return $value
}

function Test-CoopHost {
    param([string]$HostName)

    if ([string]::IsNullOrWhiteSpace($HostName) -or $HostName.Length -gt 253) {
        return $false
    }
    if ($HostName -notmatch '^[A-Za-z0-9.-]+$') {
        return $false
    }

    if ($HostName -match '^[0-9.]+$') {
        $parts = $HostName.Split('.')
        if ($parts.Count -ne 4) {
            return $false
        }
        foreach ($part in $parts) {
            $octet = 0
            if ($part -notmatch '^\d{1,3}$' -or -not [int]::TryParse($part, [ref]$octet) -or $octet -gt 255) {
                return $false
            }
        }
        return $true
    }

    foreach ($label in $HostName.Split('.')) {
        if ($label.Length -lt 1 -or $label.Length -gt 63 -or
            $label -notmatch '^[A-Za-z0-9](?:[A-Za-z0-9-]*[A-Za-z0-9])?$') {
            return $false
        }
    }
    return $true
}

function ConvertTo-AsciiName {
    param([string]$Text)

    $Text = $Text.Replace('ą', 'a').Replace('Ą', 'A')
    $Text = $Text.Replace('ć', 'c').Replace('Ć', 'C')
    $Text = $Text.Replace('ę', 'e').Replace('Ę', 'E')
    $Text = $Text.Replace('ł', 'l').Replace('Ł', 'L')
    $Text = $Text.Replace('ń', 'n').Replace('Ń', 'N')
    $Text = $Text.Replace('ó', 'o').Replace('Ó', 'O')
    $Text = $Text.Replace('ś', 's').Replace('Ś', 'S')
    $Text = $Text.Replace('ź', 'z').Replace('Ź', 'Z')
    $Text = $Text.Replace('ż', 'z').Replace('Ż', 'Z')

    $normalized = $Text.Normalize([Text.NormalizationForm]::FormD)
    $builder = New-Object Text.StringBuilder
    foreach ($character in $normalized.ToCharArray()) {
        if ([Globalization.CharUnicodeInfo]::GetUnicodeCategory($character) -eq
            [Globalization.UnicodeCategory]::NonSpacingMark) {
            continue
        }
        $number = [int][char]$character
        if ($number -ge 32 -and $number -le 126) {
            [void]$builder.Append($character)
        }
    }
    return $builder.ToString().Trim()
}

try {
    if ([string]::IsNullOrWhiteSpace($Code)) {
        $Code = Read-Host 'Wklej kod zaproszenia COOP'
    }
    $Code = $Code.Trim()
    $prefix = 'M2COOP1:'
    if (-not $Code.StartsWith($prefix, [StringComparison]::Ordinal)) {
        throw "Kod musi zaczynać się od M2COOP1:."
    }

    $encoded = $Code.Substring($prefix.Length)
    if ([string]::IsNullOrWhiteSpace($encoded) -or $encoded -notmatch '^[A-Za-z0-9_-]+$') {
        throw 'Kod zawiera nieprawidłowe znaki base64url.'
    }
    $base64 = $encoded.Replace('-', '+').Replace('_', '/')
    while (($base64.Length % 4) -ne 0) {
        $base64 += '='
    }

    $bytes = [Convert]::FromBase64String($base64)
    $utf8 = New-Object Text.UTF8Encoding($false, $true)
    $jsonText = $utf8.GetString($bytes)
    $invite = $jsonText | ConvertFrom-Json

    $version = 0
    if ($null -eq $invite.PSObject.Properties['v'] -or
        -not [int]::TryParse([string]$invite.v, [ref]$version) -or $version -ne 1) {
        throw 'Nieobsługiwana wersja kodu zaproszenia.'
    }

    $name = ConvertTo-AsciiName (Get-RequiredText $invite 'name')
    if ([string]::IsNullOrWhiteSpace($name)) {
        $name = 'Swiat znajomego'
    }
    $hostName = Get-RequiredText $invite 'host'
    if (-not (Test-CoopHost $hostName)) {
        throw 'Host nie jest poprawnym adresem IPv4 ani nazwą DNS.'
    }
    $authPort = Get-RequiredPort $invite 'auth'
    $channelPort = Get-RequiredPort $invite 'channel'

    $channelsText = Get-RequiredText $invite 'channels'
    $channels = 0
    if (-not [int]::TryParse($channelsText, [ref]$channels) -or $channels -lt 1 -or $channels -gt 2) {
        throw "Pole 'channels' musi mieć wartość 1 albo 2."
    }
    if (($channelPort + (($channels - 1) * 10)) -gt 65535) {
        throw 'Port ostatniego kanału przekracza 65535.'
    }

    $login = Get-RequiredText $invite 'login'
    $password = Get-RequiredText $invite 'password'

    $vpn = $null
    if ($null -ne $invite.PSObject.Properties['vpn'] -and
        -not [string]::IsNullOrWhiteSpace([string]$invite.vpn)) {
        $vpn = ([string]$invite.vpn).ToLowerInvariant()
        if ($vpn -notin @('radmin', 'tailscale', 'zerotier', 'hamachi')) {
            throw "Nieobsługiwany VPN: $vpn."
        }
    }

    $lines = @(
        '# Metin2 SinglePlayer - swiat znajomego (zapisal launcher, kod zaproszenia)',
        "name=$name",
        "host=$hostName",
        "auth=$authPort",
        "channel=$channelPort",
        "channels=$channels"
    )
    $configPath = Join-Path $PSScriptRoot 'coop.cfg'
    [IO.File]::WriteAllText($configPath, (($lines -join "`r`n") + "`r`n"), [Text.Encoding]::ASCII)

    Write-Host ''
    Write-Host "Zapisano: $configPath" -ForegroundColor Green
    Write-Host "W kliencie wybierz serwer 'Online: $name' i zaloguj się: login $login, hasło $password"
    if ($vpn) {
        Write-Host "Musisz mieć zainstalowany VPN $vpn i być w sieci hosta." -ForegroundColor Yellow
    }
}
catch {
    Write-Host ''
    Write-Host ("Nie udało się zapisać zaproszenia COOP: " + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
