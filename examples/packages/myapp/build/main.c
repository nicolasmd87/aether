#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <unistd.h>

void aether_args_init(int argc, char** argv);


// Import: utils
// Extern C function: utils_double_value
int utils_double_value(int);

// Extern C function: utils_triple_value
int utils_triple_value(int);


int main(int argc, char** argv) {
    aether_args_init(argc, argv);
    
    {
printf("=== Local Package Import Demo ===\n\n");
int x = 5;
printf("Original value: ");
printf("%d", x);
printf("\n");
int doubled = utils_double_value(x);
printf("Doubled (utils.double_value): ");
printf("%d", doubled);
printf("\n");
int tripled = utils_triple_value(x);
printf("Tripled (utils.triple_value): ");
printf("%d", tripled);
printf("\n");
printf("\nLocal package import works!\n");
    }
    return 0;
}
