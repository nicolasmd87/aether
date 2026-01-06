# Build script for Aether REPL (Windows)

Write-Host "Building Aether REPL..." -ForegroundColor Cyan

# Compile the REPL
gcc -O2 -Wall tools\aether_repl.c -o aether-repl.exe

if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ REPL built successfully: aether-repl.exe" -ForegroundColor Green
    Write-Host ""
    Write-Host "Run with: .\aether-repl.exe" -ForegroundColor Yellow
} else {
    Write-Host "✗ Build failed" -ForegroundColor Red
    exit 1
}
