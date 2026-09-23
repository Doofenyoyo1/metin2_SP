<#
.SYNOPSIS
  Builds metin2client.exe from the package's client source with our edits.

.DESCRIPTION
  The client's C++ is the package's (Mt2009.pl Metin2hub V2, "Source Client"),
  not this repository's; what is ours is port/clientify.py. This takes a fresh
  copy of the package source every time - the edits apply to the package's own
  text, never to a tree edited before - into a short path, because MSBuild
  cannot open "..\UserInterface\Locale_inc.h" once a path passes 260
  characters. Beside the copy it links the package's Extern (libraries,
  read-only) and our staged server tree as "Server": the client compiles
  against the server's common/*.h, and INVENTORY_PAGE_COUNT and the feature
  switches come from there, so the client and the server this repository
  builds cannot disagree about them.

  Links are made with New-Item -ItemType Junction and never removed with a
  recursive Remove-Item: Windows PowerShell 5.1 follows a junction and empties
  the directory it points to.

.PARAMETER PackageSource
  The package's Source folder, holding "Source Client" and "Extern".
.PARAMETER ServerTree
  The staged mt2009 server tree (linuxify.py + playerbotify.py applied).
.PARAMETER BuildDir
  The short working path.
#>
param(
	[string]$PackageSource = (Join-Path $env:USERPROFILE "Downloads\Metin2 Singleplayer\Source"),
	[string]$ServerTree = (Join-Path $PSScriptRoot "..\docker\game\src\server"),
	[string]$BuildDir = (Join-Path $env:TEMP "m2cb"),
	[string]$Configuration = "Release"
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

function Get-FullPath([string]$Path) { return [System.IO.Path]::GetFullPath($Path) }

$PackageSource = Get-FullPath $PackageSource
$ServerTree = Get-FullPath $ServerTree
$BuildDir = Get-FullPath $BuildDir
$clientSource = Join-Path $PackageSource "Source Client"
$extern = Join-Path $PackageSource "Extern"
foreach ($required in @((Join-Path $clientSource "Metin2Client.sln"), $extern, (Join-Path $ServerTree "common\length.h"))) {
	if (-not (Test-Path -LiteralPath $required)) { throw "Brak: $required" }
}

$root = Join-Path $BuildDir "Source"
New-Item -ItemType Directory -Force $root | Out-Null

function Set-Junction([string]$Link, [string]$Target) {
	if (Test-Path -LiteralPath $Link) {
		$item = Get-Item -LiteralPath $Link -Force
		if ($item.LinkType -ne 'Junction') { throw "$Link istnieje i nie jest laczem" }
		$current = @($item.Target)[0]
		if ((Get-FullPath $current).TrimEnd('\') -eq $Target.TrimEnd('\')) { return }
		# The link itself, never what it points to.
		[System.IO.Directory]::Delete($Link, $false)
	}
	New-Item -ItemType Junction -Path $Link -Target $Target | Out-Null
}
Set-Junction (Join-Path $root "Extern") $extern
Set-Junction (Join-Path $root "Server") $ServerTree

$copy = Join-Path $root "Source Client"
if (Test-Path -LiteralPath $copy) {
	$item = Get-Item -LiteralPath $copy -Force
	if ($item.LinkType) { throw "$copy jest laczem - nie usuwam" }
	Remove-Item -LiteralPath $copy -Recurse -Force
}
Copy-Item -LiteralPath $clientSource -Destination $copy -Recurse
Write-Host "kopia zrodla: $copy"

$clientify = Join-Path $PSScriptRoot "..\port\clientify.py"
& python $clientify $copy
if ($LASTEXITCODE -ne 0) { throw "clientify.py zakonczyl sie kodem $LASTEXITCODE" }

$msbuild = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path -LiteralPath $msbuild)) { throw "Brak MSBuild 2022: $msbuild" }
$log = Join-Path $BuildDir "build-client.log"
& $msbuild (Join-Path $copy "Metin2Client.sln") "/p:Configuration=$Configuration" "/p:Platform=Win32" "/m" "/v:minimal" "/nologo" 2>&1 | Out-File -Encoding utf8 $log
$code = $LASTEXITCODE
$errors = @(Select-String -Path $log -Pattern ': (fatal )?error' -SimpleMatch:$false)
Write-Host ("MSBuild: kod {0}, bledow {1}, ostrzezen {2}, log {3}" -f $code, $errors.Count,
	@(Select-String -Path $log -Pattern ': warning').Count, $log)
if ($code -ne 0) {
	$errors | Select-Object -First 20 | ForEach-Object { Write-Host $_.Line }
	throw "kompilacja klienta nieudana"
}

$exe = Join-Path $BuildDir "Client\$Configuration\compiled_binary\metin2client.exe"
if (-not (Test-Path -LiteralPath $exe)) { throw "Brak exe: $exe" }
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $exe).Hash
Write-Host ("exe: {0} ({1} B, sha256 {2})" -f $exe, (Get-Item -LiteralPath $exe).Length, $hash)
