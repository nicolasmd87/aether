# Correctness test for work-stealing scheduler
# Ensures no messages are lost or duplicated

Write-Host "=== Work-Stealing Correctness Test ===" -ForegroundColor Cyan

# Build
Write-Host "`nBuilding..." -ForegroundColor Yellow
.\build.ps1
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "`n=== Running Correctness Tests ===" -ForegroundColor Cyan

$all_passed = $true

# Test 1: Small scale, single worker
Write-Host "`nTest 1: Single worker (baseline)" -ForegroundColor Yellow
$output = .\work_stealing_bench.exe 1 100 10 | Out-String
if ($output -match "Status:\s+(.*)") {
    $status = $matches[1].Trim()
    Write-Host "Result: $status"
    if ($status -ne "✓ PASS") { $all_passed = $false }
}

# Test 2: Multi-worker, small scale
Write-Host "`nTest 2: 4 workers, 1000 actors" -ForegroundColor Yellow
$output = .\work_stealing_bench.exe 4 1000 50 | Out-String
if ($output -match "Status:\s+(.*)") {
    $status = $matches[1].Trim()
    Write-Host "Result: $status"
    if ($status -ne "✓ PASS") { $all_passed = $false }
}

# Test 3: Multi-worker, medium scale
Write-Host "`nTest 3: 8 workers, 10000 actors" -ForegroundColor Yellow
$output = .\work_stealing_bench.exe 8 10000 100 | Out-String
if ($output -match "Status:\s+(.*)") {
    $status = $matches[1].Trim()
    Write-Host "Result: $status"
    if ($status -ne "✓ PASS") { $all_passed = $false }
}

# Test 4: High message load
Write-Host "`nTest 4: High message load (100000 actors)" -ForegroundColor Yellow
$output = .\work_stealing_bench.exe 4 100000 10 | Out-String
if ($output -match "Status:\s+(.*)") {
    $status = $matches[1].Trim()
    Write-Host "Result: $status"
    if ($status -ne "✓ PASS") { $all_passed = $false }
}

# Test 5: Stress test - many workers
Write-Host "`nTest 5: Maximum workers (16 workers)" -ForegroundColor Yellow
$output = .\work_stealing_bench.exe 16 5000 20 | Out-String
if ($output -match "Status:\s+(.*)") {
    $status = $matches[1].Trim()
    Write-Host "Result: $status"
    if ($status -ne "✓ PASS") { $all_passed = $false }
}

Write-Host "`n=== Final Result ===" -ForegroundColor Cyan
if ($all_passed) {
    Write-Host "✓ All correctness tests PASSED" -ForegroundColor Green
    exit 0
} else {
    Write-Host "✗ Some tests FAILED" -ForegroundColor Red
    exit 1
}
