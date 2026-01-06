# Comprehensive scaling test for work-stealing scheduler
# Tests multi-core scaling from 1 to 8 cores

Write-Host "=== Aether Work-Stealing Multi-Core Scaling Test ===" -ForegroundColor Cyan
Write-Host ""

# Configuration
$ACTORS = 10000
$MESSAGES = 100
$TOTAL_MESSAGES = $ACTORS * $MESSAGES

# Build first
Write-Host "Building benchmark..." -ForegroundColor Yellow
.\build.ps1
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "`n=== Running Scaling Tests ===" -ForegroundColor Cyan
Write-Host "Total messages: $TOTAL_MESSAGES`n" -ForegroundColor Gray

$results = @()

# Test 1, 2, 4, 8 cores
foreach ($cores in @(1, 2, 4, 8)) {
    Write-Host "--- Testing $cores worker(s) ---" -ForegroundColor Yellow
    
    # Run benchmark
    $output = .\work_stealing_bench.exe $cores $ACTORS $MESSAGES | Out-String
    Write-Host $output
    
    # Parse throughput
    if ($output -match "Throughput:\s+([\d.]+)\s+M msg/sec") {
        $throughput = [double]$matches[1]
        $results += [PSCustomObject]@{
            Workers = $cores
            Throughput_Mmsgs = $throughput
            Speedup = 0  # Will calculate later
        }
    }
    
    Write-Host ""
}

# Calculate speedups
$baseline = $results[0].Throughput_Mmsgs
foreach ($result in $results) {
    $result.Speedup = [math]::Round($result.Throughput_Mmsgs / $baseline, 2)
}

# Print summary
Write-Host "=== Scaling Summary ===" -ForegroundColor Cyan
Write-Host ""
$results | Format-Table -Property `
    @{Label="Workers"; Expression={$_.Workers}; Width=8}, `
    @{Label="Throughput (M msg/s)"; Expression={$_.Throughput_Mmsgs.ToString("F2")}; Width=22}, `
    @{Label="Speedup"; Expression={$_.Speedup.ToString("F2") + "x"}; Width=10}, `
    @{Label="Efficiency"; Expression={([int](($_.Speedup / $_.Workers) * 100)).ToString() + "%"}; Width=12}

# Calculate average efficiency
$avg_efficiency = ($results | ForEach-Object { $_.Speedup / $_.Workers } | Measure-Object -Average).Average * 100
Write-Host "Average Multi-Core Efficiency: $([int]$avg_efficiency)%" -ForegroundColor Cyan

# Performance verdict
Write-Host "`n=== Performance Verdict ===" -ForegroundColor Cyan
$max_throughput = ($results | Measure-Object -Property Throughput_Mmsgs -Maximum).Maximum
Write-Host "Peak Throughput: $($max_throughput.ToString('F2')) M msg/sec"

if ($max_throughput -ge 50) {
    Write-Host "Status: ✓ EXCELLENT (>50M msg/sec)" -ForegroundColor Green
} elseif ($max_throughput -ge 20) {
    Write-Host "Status: ✓ GOOD (>20M msg/sec)" -ForegroundColor Green
} elseif ($max_throughput -ge 10) {
    Write-Host "Status: ⚠ ACCEPTABLE (>10M msg/sec)" -ForegroundColor Yellow
} else {
    Write-Host "Status: ✗ NEEDS OPTIMIZATION (<10M msg/sec)" -ForegroundColor Red
}

if ($avg_efficiency -ge 80) {
    Write-Host "Scaling: ✓ EXCELLENT (>80% efficiency)" -ForegroundColor Green
} elseif ($avg_efficiency -ge 60) {
    Write-Host "Scaling: ✓ GOOD (>60% efficiency)" -ForegroundColor Green
} elseif ($avg_efficiency -ge 40) {
    Write-Host "Scaling: ⚠ ACCEPTABLE (>40% efficiency)" -ForegroundColor Yellow
} else {
    Write-Host "Scaling: ✗ POOR (<40% efficiency)" -ForegroundColor Red
}
