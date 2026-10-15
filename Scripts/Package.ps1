<#
.SYNOPSIS
    Builds IluUnrealTool binaries for one engine version (RunUAT BuildPlugin) and zips them for a GitHub release.
    Собирает бинарники IluUnrealTool под одну версию движка (RunUAT BuildPlugin) и пакует zip для GitHub Release.

.EXAMPLE
    .\Scripts\Package.ps1 -EngineDir "D:\Software\UE_5.8"
    -> Build\IluUnrealTool-0.2.0-UE5.8-Win64\IluUnrealTool\  +  Build\IluUnrealTool-0.2.0-UE5.8-Win64.zip
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$EngineDir,

    [string]$OutDir = (Join-Path $PSScriptRoot '..\Build'),

    [switch]$NoZip
)

$ErrorActionPreference = 'Stop'
$Utf8NoBom = New-Object System.Text.UTF8Encoding $false

$PluginRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Uplugin = Join-Path $PluginRoot 'IluUnrealTool.uplugin'
$RunUAT = Join-Path $EngineDir 'Engine\Build\BatchFiles\RunUAT.bat'
if (-not (Test-Path $RunUAT)) { throw "RunUAT not found: $RunUAT" }

# Engine and plugin versions name the package / Версии движка и плагина дают имя пакету.
$BuildVersion = [System.IO.File]::ReadAllText((Join-Path $EngineDir 'Engine\Build\Build.version')) | ConvertFrom-Json
$EngineShort = "$($BuildVersion.MajorVersion).$($BuildVersion.MinorVersion)"
$PluginVersion = ([System.IO.File]::ReadAllText($Uplugin) | ConvertFrom-Json).VersionName

New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$PackageName = "IluUnrealTool-$PluginVersion-UE$EngineShort-Win64"
$PackageDir = Join-Path $OutDir $PackageName
$PluginOut = Join-Path $PackageDir 'IluUnrealTool'
if (Test-Path $PackageDir) { Remove-Item $PackageDir -Recurse -Force }

Write-Host "Building $PackageName with $EngineDir ..."
# Out-Host: the build log goes to the screen, not into the script output / лог сборки — на экран, не в вывод скрипта.
& $RunUAT BuildPlugin "-Plugin=$Uplugin" "-Package=$PluginOut" -TargetPlatforms=Win64 | Out-Host
if ($LASTEXITCODE -ne 0) { throw "BuildPlugin failed with exit code $LASTEXITCODE" }

# BuildPlugin stamps "Installed" and "EngineVersion" but drops "EnabledByDefault", and an engine plugin without it is
# disabled in every project. Restore the flag from the source descriptor (text edit keeps the formatting).
# BuildPlugin проставляет «Installed» и «EngineVersion», но выкидывает «EnabledByDefault», а движковый плагин без него
# выключен во всех проектах. Возвращаем флаг из исходного дескриптора (правка текстом сохраняет формат).
$EnabledByDefault = ([System.IO.File]::ReadAllText($Uplugin) | ConvertFrom-Json).EnabledByDefault
$OutUplugin = Join-Path $PluginOut 'IluUnrealTool.uplugin'
$Text = [System.IO.File]::ReadAllText($OutUplugin)
if ($null -ne $EnabledByDefault -and $Text -notmatch '"EnabledByDefault"') {
    $Value = if ($EnabledByDefault) { 'true' } else { 'false' }
    $Text = $Text -replace '("Installed":\s*(true|false),)', "`$1`r`n`t`"EnabledByDefault`": $Value,"
    [System.IO.File]::WriteAllText($OutUplugin, $Text, $Utf8NoBom)
}

if (-not $NoZip) {
    $Zip = "$PackageDir.zip"
    if (Test-Path $Zip) { Remove-Item $Zip -Force }
    Compress-Archive -Path $PluginOut -DestinationPath $Zip
    Write-Host "Zip: $Zip"
}

Write-Host "Package: $PluginOut"
# Last line of output — the packaged plugin folder (Install.ps1 reads it).
# Последняя строка вывода — папка собранного плагина (её читает Install.ps1).
Write-Output $PluginOut
