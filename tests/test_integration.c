#include "test_harness.h"
#include <stdlib.h>
#include <string.h>

// Test full compilation pipeline
TEST(compile_and_run_simple_loop) {
    // This test verifies that a simple for loop compiles and runs without hanging
    const char* test_code = 
        "main() {\n"
        "    print(\"Testing loop\\n\");\n"
        "    for (int i = 0; i < 3; i++) {\n"
        "        print(\"i = %d\\n\", i);\n"
        "    }\n"
        "    print(\"Done\\n\");\n"
        "}\n";
    
    // Write test code to temporary file
    FILE* f = fopen("build/test_loop.ae", "w");
    ASSERT_NOT_NULL(f);
    fprintf(f, "%s", test_code);
    fclose(f);
    
    // Compile
#ifdef _WIN32
    int result = system(".\\src\\aetherc.exe build\\test_loop.ae build\\test_loop.c");
#else
    int result = system("./src/aetherc build/test_loop.ae build/test_loop.c");
#endif
    ASSERT_EQ(0, result);
    
    // Compile C
#ifdef _WIN32
    result = system("gcc build\\test_loop.c runtime\\*.c -o build\\test_loop.exe -lpthread -Iruntime");
#else
    result = system("gcc build/test_loop.c runtime/*.c -o build/test_loop.exe -lpthread -Iruntime");
#endif
    ASSERT_EQ(0, result);
    
    // Run (should complete quickly)
#ifdef _WIN32
    result = system("build\\test_loop.exe");
#else
    result = system("./build/test_loop.exe");
#endif
    ASSERT_EQ(0, result);
}

TEST(compile_and_run_while_loop) {
    const char* test_code = 
        "main() {\n"
        "    print(\"Testing while loop\\n\");\n"
        "    int i = 0;\n"
        "    while (i < 3) {\n"
        "        print(\"i = %d\\n\", i);\n"
        "        i++;\n"
        "    }\n"
        "    print(\"Done\\n\");\n"
        "}\n";
    
    FILE* f = fopen("build/test_while.ae", "w");
    ASSERT_NOT_NULL(f);
    fprintf(f, "%s", test_code);
    fclose(f);
    
#ifdef _WIN32
    int result = system(".\\src\\aetherc.exe build\\test_while.ae build\\test_while.c");
#else
    int result = system("./src/aetherc build/test_while.ae build/test_while.c");
#endif
    ASSERT_EQ(0, result);
    
#ifdef _WIN32
    result = system("gcc build\\test_while.c runtime\\*.c -o build\\test_while.exe -lpthread -Iruntime");
#else
    result = system("gcc build/test_while.c runtime/*.c -o build/test_while.exe -lpthread -Iruntime");
#endif
    ASSERT_EQ(0, result);
    
#ifdef _WIN32
    result = system("build\\test_while.exe");
#else
    result = system("./build/test_while.exe");
#endif
    ASSERT_EQ(0, result);
}

TEST(runtime_shutdown_completes) {
    // Test that runtime shutdown doesn't hang
    const char* test_code = 
        "main() {\n"
        "    print(\"Test\\n\");\n"
        "}\n";
    
    FILE* f = fopen("build/test_shutdown.ae", "w");
    ASSERT_NOT_NULL(f);
    fprintf(f, "%s", test_code);
    fclose(f);
    
#ifdef _WIN32
    int result = system(".\\src\\aetherc.exe build\\test_shutdown.ae build\\test_shutdown.c");
#else
    int result = system("./src/aetherc build/test_shutdown.ae build/test_shutdown.c");
#endif
    ASSERT_EQ(0, result);
    
#ifdef _WIN32
    result = system("gcc build\\test_shutdown.c runtime\\*.c -o build\\test_shutdown.exe -lpthread -Iruntime");
#else
    result = system("gcc build/test_shutdown.c runtime/*.c -o build/test_shutdown.exe -lpthread -Iruntime");
#endif
    ASSERT_EQ(0, result);
    
    // This should complete quickly (not hang)
#ifdef _WIN32
    result = system("build\\test_shutdown.exe");
#else
    result = system("./build/test_shutdown.exe");
#endif
    ASSERT_EQ(0, result);
}
