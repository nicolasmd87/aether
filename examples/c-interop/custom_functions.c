// custom_functions.c - Custom C functions called from Aether
// Compile with: gcc -c custom_functions.c -o custom_functions.o
// Then link when building your Aether program

#include <stdio.h>

int my_add(int a, int b) {
    return a + b;
}

// Note: Aether's float type maps to C's double
double my_multiply(double x, double y) {
    return x * y;
}

void my_greet(const char* name) {
    printf("Hello, %s! Greetings from C!\n", name);
}

int get_magic_number(void) {
    return 42;
}
