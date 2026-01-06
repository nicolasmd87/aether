Write-Host "=== Building SIMD Vectorization Benchmark ===" -ForegroundColor Cyan

$output = "simd_bench.exe"
$source = "simd_bench.c"

# Try AVX2 first (most CPUs support this)
Write-Host "Compiling with AVX2 support..." -ForegroundColor Yellow
gcc -O3 -mavx2 -march=native -o $output $source

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nBuild successful!" -ForegroundColor Green
    Write-Host "`nUsage:" -ForegroundColor Yellow
    Write-Host "  .\simd_bench.exe [actors] [messages_per_actor]"
    Write-Host "`nExample:" -ForegroundColor Yellow
    Write-Host "  .\simd_bench.exe 10000 100"
    Write-Host "`nNote: AVX2 requires Intel Haswell (2013+) or AMD Excavator (2015+)" -ForegroundColor Cyan
} else {
    Write-Host "`nBuild failed" -ForegroundColor Red
    Write-Host "Your CPU may not support AVX2" -ForegroundColor Yellow
    exit 1
}
