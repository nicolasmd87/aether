# Install Aether Language Support for VS Code/Cursor (Windows)
# Run this script from the project root: .\editor\vscode\install.ps1

$ErrorActionPreference = "Stop"

function Install-Extension {
    param($targetPath)
    
    Write-Host "Installing Aether language support..." -ForegroundColor Green
    Write-Host "Target: $targetPath" -ForegroundColor Gray
    
    $scriptDir = Split-Path -Parent $MyInvocation.PSCommandPath
    Set-Location $scriptDir
    
    if (-not (Get-Command npm -ErrorAction SilentlyContinue)) {
        Write-Host "Warning: npm not found. Installing syntax highlighting only." -ForegroundColor Yellow
        New-Item -ItemType Directory -Path $targetPath -Force | Out-Null
        Copy-Item "$scriptDir\package.json" "$targetPath\" -Force
        Copy-Item "$scriptDir\aether.tmLanguage.json" "$targetPath\" -Force
        Copy-Item "$scriptDir\language-configuration.json" "$targetPath\" -Force
    } else {
        Write-Host "Building extension with LSP support..." -ForegroundColor Cyan
        npm install
        npm run compile
        
        New-Item -ItemType Directory -Path $targetPath -Force | Out-Null
        Copy-Item "$scriptDir\package.json" "$targetPath\" -Force
        Copy-Item "$scriptDir\aether.tmLanguage.json" "$targetPath\" -Force
        Copy-Item "$scriptDir\language-configuration.json" "$targetPath\" -Force
        if (Test-Path "$scriptDir\out") {
            Copy-Item -Recurse "$scriptDir\out" "$targetPath\" -Force
        }
        if (Test-Path "$scriptDir\node_modules\vscode-languageclient") {
            New-Item -ItemType Directory -Path "$targetPath\node_modules" -Force | Out-Null
            Copy-Item -Recurse "$scriptDir\node_modules\vscode-languageclient" "$targetPath\node_modules\" -Force
        }
    }
    
    Write-Host "✓ Extension installed successfully!" -ForegroundColor Green
    Write-Host "Please restart VS Code/Cursor for changes to take effect." -ForegroundColor Yellow
    Write-Host "Note: Make sure aether-lsp.exe is in your PATH for LSP features." -ForegroundColor Cyan
}

# Detect VS Code or Cursor
$vscodePath = "$env:USERPROFILE\.vscode\extensions\aether-language-0.1.0"
$cursorPath = "$env:USERPROFILE\.cursor\extensions\aether-language-0.1.0"

if (Test-Path "$env:USERPROFILE\.cursor") {
    Write-Host "Detected Cursor installation" -ForegroundColor Cyan
    Install-Extension $cursorPath
} else {
    Write-Host "Detected VS Code installation" -ForegroundColor Cyan
    Install-Extension $vscodePath
}

