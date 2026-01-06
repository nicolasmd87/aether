Write-Host "=== Building Message Batching Benchmark ===" -ForegroundColor Cyan

$output = "batch_bench.exe"
$source = "batch_bench.c"

gcc -O3 -march=native -o $output $source

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nBuild successful!" -ForegroundColor Green
    Write-Host "`nUsage:" -ForegroundColor Yellow
    Write-Host "  .\batch_bench.exe [actors] [messages_per_actor]"
    Write-Host "`nExample:" -ForegroundColor Yellow
    Write-Host "  .\batch_bench.exe 10000 100"
} else {
    Write-Host "`nBuild failed" -ForegroundColor Red
    exit 1
}
