<#
.SYNOPSIS
    Builds IluUnrealTool for an engine and installs it into that engine (Engine\Plugins\Marketplace\IluUnrealTool).
    Every project of this engine gets the plugin (EnabledByDefault). Close the editors of this engine first.
    Собирает IluUnrealTool под движок и ставит его в этот движок (Engine\Plugins\Marketplace\IluUnrealTool).
    Плагин появляется во всех проектах этого движка (EnabledByDefault). Редакторы этого движка закройте заранее.

.EXAMPLE
    .\Scripts\Install.ps1 -EngineDir "D:\Software\UE_5.8"
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$EngineDir,

    # Already built package folder (skip the build) / Уже собранная папка пакета (без сборки).
    [string]$PackagePath
)

$ErrorActionPreference = 'Stop'

if (-not $PackagePath) {
    $PackagePath = & (Join-Path $PSScriptRoot 'Package.ps1') -EngineDir $EngineDir -NoZip | Select-Object -Last 1
}
if (-not (Test-Path (Join-Path $PackagePath 'IluUnrealTool.uplugin'))) {
    throw "Not a packaged plugin folder: $PackagePath"
}

$Dest = Join-Path $EngineDir 'Engine\Plugins\Marketplace\IluUnrealTool'
if (Test-Path $Dest) {
    Write-Host "Removing previous install: $Dest"
    Remove-Item $Dest -Recurse -Force
}
New-Item -ItemType Directory -Force (Split-Path $Dest) | Out-Null
Copy-Item $PackagePath $Dest -Recurse
Write-Host "Installed: $Dest"
