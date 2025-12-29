$files = @(
    "examples\basic\hello_world.ae",
    "examples\basic\hello_actors.ae",
    "examples\minimal_syntax.ae",
    "examples\type_inference.ae",
    "examples\runtime_test.ae",
    "examples\tests\test_arrays.ae",
    "examples\tests\test_condition.ae",
    "examples\tests\test_for_loop.ae",
    "examples\tests\test_modern_syntax.ae",
    "examples\tests\test_struct.ae",
    "examples\tests\test_struct_complex.ae",
    "examples\tests\test_actor_simple.ae",
    "examples\tests\test_actor_spawn_send.ae",
    "examples\tests\test_multiple_actors.ae",
    "examples\advanced\main_example.ae",
    "examples\advanced\supervisor.ae",
    "examples\benchmarks\simple_demo.ae",
    "examples\benchmarks\performance_demo.ae",
    "examples\benchmarks\feature_showcase.ae",
    "examples\benchmarks\ring_benchmark.ae",
    "examples\benchmarks\working_demo.ae"
)

$passed = 0
$failed = 0
$failedFiles = @()

foreach($f in $files) {
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "Testing: $f" -ForegroundColor Yellow
    
    $basename = [System.IO.Path]::GetFileNameWithoutExtension($f)
    $outfile = "build\test_$basename.c"
    
    # Compile Aether to C
    .\build\aetherc.exe $f $outfile 2>&1 | Out-Null
    
    if($LASTEXITCODE -eq 0) {
        # Compile C to object
        gcc -Iruntime -c $outfile -o "build\test_$basename.o" 2>&1 | Out-Null
        
        if($LASTEXITCODE -eq 0) {
            Write-Host "PASSED" -ForegroundColor Green
            $passed++
        } else {
            Write-Host "FAILED: C compilation" -ForegroundColor Red
            $failed++
            $failedFiles += $f
        }
    } else {
        Write-Host "FAILED: Aether compilation" -ForegroundColor Red
        $failed++
        $failedFiles += $f
    }
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Results: $passed passed, $failed failed" -ForegroundColor $(if($failed -eq 0){"Green"}else{"Yellow"})

if($failedFiles.Count -gt 0) {
    Write-Host "`nFailed files:" -ForegroundColor Red
    $failedFiles | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
}

