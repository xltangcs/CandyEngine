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

# Regenerate CANDY_PROPERTY-derived code (Python bindings, scene serialization,
# inspector defaults, component inventory) so the build can never drift from
# the annotated headers.
$python = Get-Command python -ErrorAction SilentlyContinue
if ($python) {
	Write-Host "Regenerating metadata-driven sources (generate_bindings.py)..." -ForegroundColor Cyan
	& python (Join-Path $PSScriptRoot "generate_bindings.py") | Out-Host
	if ($LASTEXITCODE -ne 0) {
		Write-Error "generate_bindings.py failed (exit $LASTEXITCODE)"
		exit $LASTEXITCODE
	}
} else {
	Write-Warning "python not found on PATH - skipping metadata regeneration (generated files may be stale)"
}

Write-Host "Building $solution ($Config|$Platform) with $msbuild" -ForegroundColor Cyan
& $msbuild $solution /p:Configuration=$Config /p:Platform=$Platform /m /v:minimal /nr:false
exit $LASTEXITCODE
