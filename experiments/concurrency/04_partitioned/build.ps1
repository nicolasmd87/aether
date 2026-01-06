Write-Host "=== Building Partitioned State Machine Benchmark ===" -ForegroundColor Cyan

$output = "partitioned_bench.exe"
$source = "partitioned_bench.c"

gcc -O3 -march=native -o $output $source -lpthread

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n✓ Build successful!" -ForegroundColor Green
    Write-Host "`nUsage:" -ForegroundColor Yellow
    Write-Host "  .\partitioned_bench.exe [cores] [actors] [messages_per_actor]"
    Write-Host "`nExamples:" -ForegroundColor Yellow
    Write-Host "  .\partitioned_bench.exe 1 10000 100   # Baseline (compare to Exp 02)"
    Write-Host "  .\partitioned_bench.exe 2 10000 100   # 2 cores"
    Write-Host "  .\partitioned_bench.exe 4 10000 100   # 4 cores"
    Write-Host "  .\partitioned_bench.exe 8 10000 100   # 8 cores"
} else {
    Write-Host "`n✗ Build failed" -ForegroundColor Red
    exit 1
}
