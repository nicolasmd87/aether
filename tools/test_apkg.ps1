# Test: apkg package manager functionality
# This test verifies basic apkg commands work correctly

Write-Host "Testing apkg package manager..." -ForegroundColor Cyan

# Create test directory
$TestDir = "test_apkg_project"
if (Test-Path $TestDir) {
    Remove-Item -Recurse -Force $TestDir
}
New-Item -ItemType Directory -Path $TestDir | Out-Null

# Test 1: apkg init
Write-Host ""
Write-Host "[Test 1] apkg init..." -ForegroundColor Yellow
Push-Location $TestDir
..\apkg.exe init myproject
$InitResult = $LASTEXITCODE
Pop-Location
if ($InitResult -eq 0 -and (Test-Path "$TestDir\aether.toml")) {
    Write-Host "PASS: apkg init creates project" -ForegroundColor Green
} else {
    Write-Host "FAIL: apkg init failed" -ForegroundColor Red
    Remove-Item -Recurse -Force $TestDir
    exit 1
}

# Test 2: Check generated files
Write-Host ""
Write-Host "[Test 2] Generated files..." -ForegroundColor Yellow
$ExpectedFiles = @("aether.toml", "src\main.ae", "README.md")
$AllExist = $true
foreach ($File in $ExpectedFiles) {
    if (-not (Test-Path "$TestDir\$File")) {
        Write-Host "FAIL: Missing $File" -ForegroundColor Red
        $AllExist = $false
    }
}
if ($AllExist) {
    Write-Host "PASS: All expected files created" -ForegroundColor Green
} else {
    exit 1
}

# Test 3: apkg help
Write-Host ""
Write-Host "[Test 3] apkg help..." -ForegroundColor Yellow
.\apkg.exe help | Out-Null
if ($LASTEXITCODE -eq 0) {
    Write-Host "PASS: apkg help works" -ForegroundColor Green
} else {
    Write-Host "FAIL: apkg help failed" -ForegroundColor Red
    exit 1
}

# Test 4: apkg version
Write-Host ""
Write-Host "[Test 4] apkg version..." -ForegroundColor Yellow
.\apkg.exe version | Out-Null
if ($LASTEXITCODE -eq 0) {
    Write-Host "PASS: apkg version works" -ForegroundColor Green
} else {
    Write-Host "FAIL: apkg version failed" -ForegroundColor Red
    exit 1
}

# Test 5: apkg search
Write-Host ""
Write-Host "[Test 5] apkg search..." -ForegroundColor Yellow
.\apkg.exe search test | Out-Null
if ($LASTEXITCODE -eq 0) {
    Write-Host "PASS: apkg search works" -ForegroundColor Green
} else {
    Write-Host "FAIL: apkg search failed" -ForegroundColor Red
    exit 1
}

# Test 6: apkg update (in test project)
Write-Host ""
Write-Host "[Test 6] apkg update..." -ForegroundColor Yellow
Push-Location $TestDir
..\apkg.exe update | Out-Null
$UpdateResult = $LASTEXITCODE
Pop-Location
if ($UpdateResult -eq 0) {
    Write-Host "PASS: apkg update works" -ForegroundColor Green
} else {
    Write-Host "FAIL: apkg update failed" -ForegroundColor Red
    exit 1
}

# Test 7: apkg test (creates test directory)
Write-Host ""
Write-Host "[Test 7] apkg test..." -ForegroundColor Yellow
Push-Location $TestDir
..\apkg.exe test | Out-Null
$TestResult = $LASTEXITCODE
Pop-Location
$TestDirExists = Test-Path "$TestDir\tests"
if (($TestResult -eq 0) -and $TestDirExists) {
    Write-Host "PASS: apkg test creates test directory" -ForegroundColor Green
} else {
    Write-Host "FAIL: apkg test failed" -ForegroundColor Red
    exit 1
}

# Cleanup
Write-Host ""
Write-Host "Cleaning up..." -ForegroundColor Cyan
Remove-Item -Recurse -Force $TestDir

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "All apkg tests passed!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
