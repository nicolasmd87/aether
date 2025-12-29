$env:PATH = "C:\cygwin64\bin;$env:PATH"

$examples = @(
    # Type inference tests
    "tests\test_type_inference_literals.ae",
    "tests\test_type_inference_functions.ae",
    "tests\test_type_inference_structs.ae",
    
    # Basic examples
    "examples\basic\hello_world.ae",
    "examples\basic\hello_actors.ae",
    
    # Syntax tests
    "examples\tests\test_condition.ae",
    "examples\tests\test_for_loop.ae",
    "examples\tests\test_arrays.ae",
    "examples\tests\test_struct.ae",
    "examples\tests\test_struct_complex.ae",
    "examples\tests\test_modern_syntax.ae",
    
    # Actor tests
    "examples\tests\test_actor_simple.ae",
    "examples\tests\test_multiple_actors.ae",
    "examples\tests\test_actor_spawn_send.ae",
    "examples\tests\test_actor_working.ae",
    "examples\tests\test_send.ae",
    
    # Advanced examples
    "examples\advanced\main_example.ae",
    "examples\advanced\supervisor.ae",
    
    # Benchmarks
    "examples\benchmarks\ring_benchmark.ae"
)

$passed = 0
$failed = 0

foreach ($example in $examples) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($example)
    $output_c = "build\$name.c"
    $output_exe = "build\$name.exe"
    
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "Testing: $example" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    
    # Compile Aether to C
    .\build\aetherc.exe $example $output_c 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "❌ FAILED: Aether compilation" -ForegroundColor Red
        $failed++
        continue
    }
    
    # Compile C to executable
    gcc $output_c -Iruntime runtime\multicore_scheduler.c runtime\memory.c runtime\aether_string.c -o $output_exe 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "❌ FAILED: C compilation" -ForegroundColor Red
        $failed++
        continue
    }
    
    # Run with timeout (5 seconds)
    $job = Start-Job -ScriptBlock { param($exe) & $exe 2>&1 } -ArgumentList $output_exe
    $completed = Wait-Job $job -Timeout 5
    
    if ($completed) {
        $result = Receive-Job $job
        Remove-Job $job
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✅ PASSED" -ForegroundColor Green
            $passed++
        } else {
            Write-Host "❌ FAILED: Runtime error" -ForegroundColor Red
            $failed++
        }
    } else {
        Stop-Job $job
        Remove-Job $job
        Write-Host "⏱️  TIMEOUT (5s) - Likely infinite loop" -ForegroundColor Yellow
        $failed++
    }
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Results: $passed passed, $failed failed" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

