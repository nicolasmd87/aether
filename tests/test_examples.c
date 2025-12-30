#include "test_harness.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif

static int file_exists(const char* filename) {
    return access(filename, F_OK) == 0;
}

static int compile_aether_file(const char* input_file, const char* output_file) {
    char command[512];
#ifdef _WIN32
    snprintf(command, sizeof(command), ".\\build\\aetherc.exe %s %s", input_file, output_file);
#else
    snprintf(command, sizeof(command), "./build/aetherc %s %s", input_file, output_file);
#endif
    return system(command);
}

static int compile_c_file(const char* input_file, const char* output_file) {
    char command[512];
#ifdef _WIN32
    snprintf(command, sizeof(command), "gcc %s runtime\\multicore_scheduler.c runtime\\memory.c -o %s -lpthread -Iruntime", input_file, output_file);
#else
    snprintf(command, sizeof(command), "gcc %s runtime/multicore_scheduler.c runtime/memory.c -o %s -lpthread -Iruntime", input_file, output_file);
#endif
    return system(command);
}

static int run_executable(const char* exe_path) {
    char command[512];
#ifdef _WIN32
    snprintf(command, sizeof(command), "%s > nul 2>&1", exe_path);
#else
    snprintf(command, sizeof(command), "%s > /dev/null 2>&1", exe_path);
#endif
    return system(command);
}

static void test_example_file(const char* example_name) {
    char input_path[256];
    char c_output_path[256];
    char exe_path[256];
    
    snprintf(input_path, sizeof(input_path), "examples/%s.ae", example_name);
    snprintf(c_output_path, sizeof(c_output_path), "build/test_%s.c", example_name);
    snprintf(exe_path, sizeof(exe_path), "build/test_%s.exe", example_name);
    
    if (!file_exists(input_path)) {
        printf("  SKIPPED (file not found: %s)\n", input_path);
        return;
    }
    
    int result = compile_aether_file(input_path, c_output_path);
    ASSERT_EQ(0, result);
    
    result = compile_c_file(c_output_path, exe_path);
    ASSERT_EQ(0, result);
    
    result = run_executable(exe_path);
    ASSERT_EQ(0, result);
}

TEST(simple_for_loop) {
    test_example_file("simple_for");
}

TEST(test_for_loop) {
    test_example_file("test_for_loop");
}

TEST(ultra_simple) {
    test_example_file("ultra_simple");
}

TEST(hello_world) {
    test_example_file("hello_world");
}

TEST(minimal_test) {
    test_example_file("minimal_test");
}

TEST(test_condition) {
    test_example_file("test_condition");
}

TEST(simple_demo) {
    test_example_file("simple_demo");
}

TEST(main_example) {
    test_example_file("main_example");
}

