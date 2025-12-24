# Install Aether Language Support for VS Code/Cursor (Windows)
# Run this script from the project root: .\editor\vscode\install.ps1

$ErrorActionPreference = "Stop"

function Install-Extension {
    param($targetPath)
    
    Write-Host "Installing Aether language support..." -ForegroundColor Green
    Write-Host "Target: $targetPath" -ForegroundColor Gray
    
    # Create extension directory
    New-Item -ItemType Directory -Path $targetPath -Force | Out-Null
    
    # Copy extension files
    $scriptDir = Split-Path -Parent $MyInvocation.PSCommandPath
    Copy-Item "$scriptDir\package.json" "$targetPath\" -Force
    Copy-Item "$scriptDir\aether.tmLanguage.json" "$targetPath\" -Force
    Copy-Item "$scriptDir\language-configuration.json" "$targetPath\" -Force
    
    Write-Host "✓ Extension installed successfully!" -ForegroundColor Green
    Write-Host "Please restart VS Code/Cursor for changes to take effect." -ForegroundColor Yellow
}

# Detect VS Code or Cursor
$vscodePath = "$env:USERPROFILE\.vscode\extensions\aether-language-0.0.1"
$cursorPath = "$env:USERPROFILE\.cursor\extensions\aether-language-0.0.1"

if (Test-Path "$env:USERPROFILE\.cursor") {
    Write-Host "Detected Cursor installation" -ForegroundColor Cyan
    Install-Extension $cursorPath
} else {
    Write-Host "Detected VS Code installation" -ForegroundColor Cyan
    Install-Extension $vscodePath
}

