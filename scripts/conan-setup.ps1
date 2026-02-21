# PowerShell script for Windows Conan setup
param(
    [string]$Platform = "win32",
    [string]$Arch = "x64",
    [string]$BuildDir = "C:\temp\pgnx-build"
)

Write-Host "=== Conan Setup for pgnx (Windows) ==="
Write-Host "Platform: $Platform"
Write-Host "Architecture: $Arch"
Write-Host "Build directory: $BuildDir"

# Check if Conan is installed
if (!(Get-Command conan -ErrorAction SilentlyContinue)) {
    Write-Host "Installing Conan..."
    pip install conan
}

# Initialize Conan profile
conan profile detect --force

# Create build directory
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
Set-Location $BuildDir

# Install dependencies via Conan
$Workspace = if ($env:GITHUB_WORKSPACE) { $env:GITHUB_WORKSPACE } else { (Get-Location).Path }
Write-Host "Installing dependencies via Conan..."
conan install $Workspace `
    --build=missing `
    -s build_type=Release `
    -s compiler.cppstd=17 `
    -o "*:shared=False"

Write-Host "=== Conan setup complete ==="
