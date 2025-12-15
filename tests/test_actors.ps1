$ErrorActionPreference = "Stop"

Write-Host "Testing Actor Implementation"
Write-Host "=============================="
Write-Host ""

Write-Host "Test 1: Basic Actor Compilation"
& .\build\aetherc.exe examples\test_actor_working.ae build\test1.c 2>&1 | Out-Null
gcc -c build\test1.c -Iruntime -o build\test1.o 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) {
    Write-Host "PASS: Basic actor compiles"
} else {
    Write-Host "FAIL: Basic actor failed"
    exit 1
}
Write-Host ""

Write-Host "Test 2: Multiple Actors"
& .\build\aetherc.exe examples\test_multiple_actors.ae build\test2.c 2>&1 | Out-Null
gcc -c build\test2.c -Iruntime -o build\test2.o 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) {
    Write-Host "PASS: Multiple actors compile"
} else {
    Write-Host "FAIL: Multiple actors failed"
    exit 1
}
Write-Host ""

Write-Host "Test 3: Check Generated Code"
$content = Get-Content build\test1.c -Raw
if ($content -match "self->count" -and 
    $content -match "actor_state_machine.h" -and 
    $content -match "typedef struct Counter" -and 
    $content -match "void Counter_step") {
    Write-Host "PASS: Generated code correct"
} else {
    Write-Host "FAIL: Generated code missing expected content"
    exit 1
}
Write-Host ""

Write-Host "All Tests Passed"
