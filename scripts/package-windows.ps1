# CadView Windows Packaging Script
# Enforces the 60 MB package budget for zip / installer.

param(
    [string]$BuildDir = "$PSScriptRoot\..\build",
    [string]$DistDir = "$PSScriptRoot\..\dist",
    [string]$OcctDir = $env:CSF_OCCT_DIR
)

$ErrorActionPreference = "Stop"

Write-Host "=== Packaging CadView for Windows (x64) ==="

$RepoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$CadviewExe = Join-Path $BuildDir "cadview.exe"

if (-not (Test-Path $CadviewExe)) {
    Write-Error "Error: $CadviewExe not found. Please build the Release target first."
}

$PackageName = "cadview-windows-x64"
$PackageRoot = Join-Path $DistDir $PackageName
$ZipPath = Join-Path $DistDir "$PackageName.zip"

if (Test-Path $PackageRoot) { Remove-Item -Recurse -Force $PackageRoot }
if (Test-Path $ZipPath) { Remove-Item -Force $ZipPath }

New-Item -ItemType Directory -Path (Join-Path $PackageRoot "bin") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $PackageRoot "resources") -Force | Out-Null

# Copy binary & legal documents
Copy-Item $CadviewExe (Join-Path $PackageRoot "bin\cadview.exe")
Copy-Item "$RepoRoot\LICENSE" $PackageRoot
Copy-Item "$RepoRoot\THIRD_PARTY.md" $PackageRoot

# Run windeployqt
if (Get-Command "windeployqt" -ErrorAction SilentlyContinue) {
    Write-Host "Running windeployqt..."
    windeployqt --release --no-translations --no-system-d3d-compiler (Join-Path $PackageRoot "bin\cadview.exe")
} else {
    Write-Warning "windeployqt not found in PATH; skipping Qt DLL deployment."
}

# Copy OpenCASCADE DLLs & Shaders if OCCT dir is provided
if ($OcctDir -and (Test-Path $OcctDir)) {
    Write-Host "Copying OpenCASCADE DLLs from $OcctDir..."
    Get-ChildItem -Path "$OcctDir\bin\TK*.dll" | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $PackageRoot "bin\")
    }
    if (Test-Path "$OcctDir\resources\Shaders") {
        Copy-Item -Recurse "$OcctDir\resources\Shaders" (Join-Path $PackageRoot "resources\Shaders")
    }
}

# Compress into standalone ZIP
Write-Host "Compressing archive to $ZipPath..."
Compress-Archive -Path "$PackageRoot\*" -DestinationPath $ZipPath -CompressionLevel Optimal

# Assert budget: ≤ 60 MB (62914560 bytes)
$MaxBytes = 62914560
$ZipSize = (Get-Item $ZipPath).Length
$ZipMb = [math]::Round($ZipSize / 1MB, 2)

Write-Host "Package created: $ZipPath ($ZipMb MB)"

if ($ZipSize -gt $MaxBytes) {
    Write-Error "ERROR: Package size $ZipMb MB exceeds maximum allowable budget of 60.00 MB!"
}

Write-Host "✓ Package budget assertion passed ($ZipMb MB <= 60.00 MB)"
Write-Host "=== Packaging completed successfully ==="
