# Aether Programming Language Installer for Windows
# PowerShell installation script

$ErrorActionPreference = "Stop"

# Configuration
$InstallDir = if ($env:AETHER_HOME) { $env:AETHER_HOME } else { "$env:USERPROFILE\.aether" }
$BinDir = "$InstallDir\bin"
$RuntimeDir = "$InstallDir\runtime"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Aether Programming Language Installer" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check for GCC
$gccPath = Get-Command gcc -ErrorAction SilentlyContinue

if (-not $gccPath) {
    Write-Host "Error: GCC not found" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install one of the following:" -ForegroundColor Yellow
    Write-Host "  1. MSYS2: https://www.msys2.org/" -ForegroundColor Gray
    Write-Host "  2. MinGW-w64: https://www.mingw-w64.org/" -ForegroundColor Gray
    Write-Host "  3. WSL with Ubuntu: wsl --install" -ForegroundColor Gray
    Write-Host ""
    Write-Host "After installation, add GCC to your PATH and run this script again." -ForegroundColor Yellow
    exit 1
}

Write-Host "✓ GCC found:" -ForegroundColor Green
Write-Host "  $($gccPath.Source)" -ForegroundColor Gray

# Create directories
Write-Host "`nCreating directories..." -ForegroundColor Yellow
New-Item -ItemType Directory -Path $BinDir -Force | Out-Null
New-Item -ItemType Directory -Path $RuntimeDir -Force | Out-Null
Write-Host "✓ Directories created" -ForegroundColor Green

# Build compiler
Write-Host "`nBuilding Aether compiler..." -ForegroundColor Yellow
if (Test-Path "Makefile") {
    # Try Make first
    $makePath = Get-Command make -ErrorAction SilentlyContinue
    if ($makePath) {
        make
    } else {
        # Fall back to manual GCC build
        & .\build_compiler.ps1
    }
    
    if (Test-Path "build\aetherc.exe") {
        Copy-Item "build\aetherc.exe" "$BinDir\"
        Write-Host "✓ Compiler built successfully" -ForegroundColor Green
    } else {
        Write-Host "Error: Build failed" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "Error: Makefile not found" -ForegroundColor Red
    Write-Host "Please run this script from the Aether project root" -ForegroundColor Yellow
    exit 1
}

# Copy runtime files
Write-Host "`nInstalling runtime library..." -ForegroundColor Yellow
if (Test-Path "runtime") {
    Copy-Item "runtime\*.c" "$RuntimeDir\"
    Copy-Item "runtime\*.h" "$RuntimeDir\"
    Write-Host "✓ Runtime installed" -ForegroundColor Green
} else {
    Write-Host "Error: Runtime directory not found" -ForegroundColor Red
    exit 1
}

# Add to PATH
Write-Host "`nConfiguring PATH..." -ForegroundColor Yellow
$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")

if ($currentPath -notlike "*$BinDir*") {
    [Environment]::SetEnvironmentVariable(
        "Path",
        "$currentPath;$BinDir",
        "User"
    )
    Write-Host "✓ Added to PATH (User scope)" -ForegroundColor Green
    Write-Host "  Please restart your terminal for PATH changes to take effect" -ForegroundColor Yellow
} else {
    Write-Host "✓ Already in PATH" -ForegroundColor Green
}

# Set AETHER_RUNTIME environment variable
[Environment]::SetEnvironmentVariable("AETHER_RUNTIME", $RuntimeDir, "User")
Write-Host "✓ Set AETHER_RUNTIME environment variable" -ForegroundColor Green

# Create aether.bat wrapper
Write-Host "`nCreating aether CLI wrapper..." -ForegroundColor Yellow
$wrapperContent = @"
@echo off
setlocal

set "AETHER_RUNTIME=%AETHER_RUNTIME%"
if "%AETHER_RUNTIME%"=="" set "AETHER_RUNTIME=%USERPROFILE%\.aether\runtime"

if "%1"=="build" goto :build
if "%1"=="run" goto :run
if "%1"=="version" goto :version
if "%1"=="--version" goto :version
if "%1"=="-v" goto :version
if "%1"=="help" goto :help
if "%1"=="--help" goto :help
if "%1"=="-h" goto :help
goto :passthrough

:build
shift
if "%1"=="" (
    echo Input file required
    exit /b 1
)
set "INPUT=%1"
set "OUTPUT=%2"
if "%OUTPUT%"=="" set "OUTPUT=%~n1.c"
aetherc.exe "%INPUT%" "%OUTPUT%"
goto :end

:run
shift
if "%1"=="" (
    echo Input file required
    exit /b 1
)
set "INPUT=%1"
set "TEMP_C=%INPUT%.c"
set "TEMP_EXE=%~n1.exe"

aetherc.exe "%INPUT%" "%TEMP_C%" || exit /b 1
gcc "%TEMP_C%" -I"%AETHER_RUNTIME%" "%AETHER_RUNTIME%"\*.c -o "%TEMP_EXE%" -lpthread || exit /b 1
"%TEMP_EXE%"
del "%TEMP_C%" "%TEMP_EXE%"
goto :end

:version
aetherc.exe --version
goto :end

:help
echo Aether Programming Language
echo.
echo Usage:
echo   aether build ^<input.ae^> [output.c]  Compile Aether to C
echo   aether run ^<input.ae^>                Compile and run
echo   aether version                         Show version
echo   aether help                            Show this help
goto :end

:passthrough
aetherc.exe %*
goto :end

:end
endlocal
"@

$wrapperPath = "$BinDir\aether.bat"
Set-Content -Path $wrapperPath -Value $wrapperContent
Write-Host "✓ Created aether.bat CLI wrapper" -ForegroundColor Green

# Summary
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Installation complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Installed to: $InstallDir"
Write-Host ""
Write-Host "To use Aether:" -ForegroundColor Yellow
Write-Host "  1. Restart your terminal (or PowerShell)"
Write-Host "  2. Run: aether version"
Write-Host "  3. Try: aether run examples\basic\hello_world.ae"
Write-Host ""
Write-Host "Or use immediately in this session:" -ForegroundColor Yellow
Write-Host "  `$env:Path += `";$BinDir`""
Write-Host ""

