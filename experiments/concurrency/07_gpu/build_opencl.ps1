Write-Host "=== Building GPU Benchmark (OpenCL - Cross-Platform) ===" -ForegroundColor Cyan

# OpenCL works with GCC, no Visual Studio needed!

$output = "gpu_bench_opencl.exe"
$source = "gpu_bench_opencl.c"

# Try to compile with OpenCL
Write-Host "Checking for OpenCL..." -ForegroundColor Yellow

# Common OpenCL SDK locations
$openclPaths = @(
    "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.1\include",
    "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\include",
    "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4\include",
    "C:\Program Files (x86)\Intel\OpenCL SDK\include",
    "C:\Program Files\AMD\AMD GPU Computing Toolkit\include"
)

$openclLibPaths = @(
    "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.1\lib\x64",
    "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\lib\x64",
    "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4\lib\x64",
    "C:\Program Files (x86)\Intel\OpenCL SDK\lib\x64",
    "C:\Program Files\AMD\AMD GPU Computing Toolkit\lib\x64"
)

$foundInclude = $null
$foundLib = $null

foreach ($path in $openclPaths) {
    if (Test-Path "$path\CL\cl.h") {
        $foundInclude = $path
        Write-Host "Found OpenCL headers: $path" -ForegroundColor Green
        break
    }
}

foreach ($path in $openclLibPaths) {
    if (Test-Path "$path\OpenCL.lib") {
        $foundLib = $path
        Write-Host "Found OpenCL library: $path" -ForegroundColor Green
        break
    }
}

if (-not $foundInclude -or -not $foundLib) {
    Write-Host "`nWARNING: OpenCL SDK not found!" -ForegroundColor Yellow
    Write-Host "`nOpenCL is usually included with GPU drivers:" -ForegroundColor Cyan
    Write-Host "  - NVIDIA: GeForce drivers include OpenCL"
    Write-Host "  - AMD: Radeon drivers include OpenCL"
    Write-Host "  - Intel: Intel GPU drivers include OpenCL"
    Write-Host "`nTrying to compile anyway (might work if drivers are installed)..."
}

Write-Host "`nCompiling with GCC..." -ForegroundColor Yellow

if ($foundInclude -and $foundLib) {
    gcc -O3 -I"$foundInclude" -L"$foundLib" -o $output $source -lOpenCL
} else {
    # Try without explicit paths (might work if in system PATH)
    gcc -O3 -o $output $source -lOpenCL
}

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nBuild successful!" -ForegroundColor Green
    Write-Host "`nUsage:" -ForegroundColor Yellow
    Write-Host "  .\gpu_bench_opencl.exe [actors] [messages_per_actor]"
    Write-Host "`nExamples:" -ForegroundColor Yellow
    Write-Host "  .\gpu_bench_opencl.exe 10000 100    # Small"
    Write-Host "  .\gpu_bench_opencl.exe 100000 10    # Medium"
    Write-Host "  .\gpu_bench_opencl.exe 1000000 10   # Large"
    Write-Host "`nNote: Works with NVIDIA, AMD, or Intel GPUs" -ForegroundColor Cyan
} else {
    Write-Host "`nBuild failed" -ForegroundColor Red
    Write-Host "`nPossible solutions:" -ForegroundColor Yellow
    Write-Host "  1. Update GPU drivers (includes OpenCL runtime)"
    Write-Host "  2. Install GPU vendor SDK (optional, drivers should be enough)"
    Write-Host "  3. This experiment is OPTIONAL - CPU+SIMD is already very fast"
}
