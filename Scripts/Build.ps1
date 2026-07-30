param(
	[string]$Config = "Debug",
	[string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
	Write-Error "vswhere not found: $vswhere"
	exit 1
}

$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) {
	Write-Error "MSBuild not found via vswhere."
	exit 1
}

$solution = Join-Path $PSScriptRoot "..\CandyEngine.sln"
if (-not (Test-Path $solution)) {
	Write-Error "Solution not found: $solution (run .\Scripts\GenerateProjects.bat first)"
	exit 1
}

Write-Host "Building $solution ($Config|$Platform) with $msbuild" -ForegroundColor Cyan
& $msbuild $solution /p:Configuration=$Config /p:Platform=$Platform /m /v:minimal /nr:false
exit $LASTEXITCODE
