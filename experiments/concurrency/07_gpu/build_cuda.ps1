Write-Host "=== Building GPU Acceleration Benchmark (CUDA) ===" -ForegroundColor Cyan

# Check if nvcc (CUDA compiler) is available
$nvcc = Get-Command nvcc -ErrorAction SilentlyContinue

if (-not $nvcc) {
    Write-Host "`nERROR: CUDA Toolkit not found!" -ForegroundColor Red
    Write-Host "Please install NVIDIA CUDA Toolkit from:" -ForegroundColor Yellow
    Write-Host "https://developer.nvidia.com/cuda-downloads"
    Write-Host "`nOr skip GPU experiment if you don't have NVIDIA GPU" -ForegroundColor Yellow
    exit 1
}

$output = "gpu_bench.exe"
$source = "gpu_bench.cu"

Write-Host "Compiling with CUDA..." -ForegroundColor Yellow
nvcc -O3 -arch=sm_75 -o $output $source

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nBuild successful!" -ForegroundColor Green
    Write-Host "`nUsage:" -ForegroundColor Yellow
    Write-Host "  .\gpu_bench.exe [actors] [messages_per_actor]"
    Write-Host "`nExamples:" -ForegroundColor Yellow
    Write-Host "  .\gpu_bench.exe 10000 100    # Small (CPU likely wins)"
    Write-Host "  .\gpu_bench.exe 100000 10    # Medium (GPU starts winning)"
    Write-Host "  .\gpu_bench.exe 1000000 10   # Large (GPU dominates)"
    Write-Host "`nNote: Requires NVIDIA GPU with CUDA support" -ForegroundColor Cyan
} else {
    Write-Host "`nBuild failed" -ForegroundColor Red
    Write-Host "Check that you have:"
    Write-Host "  1. NVIDIA GPU"
    Write-Host "  2. Latest NVIDIA drivers"
    Write-Host "  3. CUDA Toolkit installed"
    exit 1
}
