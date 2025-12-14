#include "test_harness.h"
#include <string.h>

int main(int argc, char** argv) {
    printf("Aether Language Test Suite\n");
    printf("==========================\n\n");
    
    // Note: All tests are automatically registered via constructor attributes
    // when the test object files are linked. The test harness will run all
    // registered tests regardless of arguments.
    // 
    // For filtering by category, you would need to implement test tagging
    // or separate executables for each category.
    
    if (argc > 1) {
        printf("Note: Test filtering not yet implemented. Running all tests.\n\n");
    }
    
    // Run all registered tests
    run_all_tests();
    
    return 0;
}
