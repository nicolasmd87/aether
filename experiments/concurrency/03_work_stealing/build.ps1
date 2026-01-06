# Build script for Work-Stealing Benchmark (Windows/MinGW)

Write-Host "=== Building Work-Stealing Multi-Core Benchmark ===" -ForegroundColor Cyan

# Compile with maximum optimizations
gcc -O3 -march=native -Wall -Wextra `
    work_stealing_bench.c `
    -o work_stealing_bench.exe `
    -lpthread

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n✓ Build successful!" -ForegroundColor Green
    Write-Host "`nUsage:" -ForegroundColor Yellow
    Write-Host "  .\work_stealing_bench.exe [workers] [actors] [messages_per_actor]"
    Write-Host "`nExamples:" -ForegroundColor Yellow
    Write-Host "  .\work_stealing_bench.exe 1 10000 100     # Single-core baseline"
    Write-Host "  .\work_stealing_bench.exe 2 10000 100     # 2 cores"
    Write-Host "  .\work_stealing_bench.exe 4 10000 100     # 4 cores"
    Write-Host "  .\work_stealing_bench.exe 8 10000 100     # 8 cores"
    Write-Host "  .\work_stealing_bench.exe 4 100000 10     # Scalability test"
} else {
    Write-Host "`n✗ Build failed" -ForegroundColor Red
    exit 1
}
