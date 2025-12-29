#include "apkg.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        apkg_print_help();
        return 1;
    }
    
    const char* command = argv[1];
    
    if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
        apkg_print_help();
        return 0;
    }
    
    if (strcmp(command, "version") == 0 || strcmp(command, "--version") == 0 || strcmp(command, "-v") == 0) {
        apkg_print_version();
        return 0;
    }
    
    if (strcmp(command, "init") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: 'init' requires a package name\n");
            fprintf(stderr, "Usage: apkg init <name>\n");
            return 1;
        }
        return apkg_init(argv[2]);
    }
    
    if (strcmp(command, "install") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: 'install' requires a package name\n");
            fprintf(stderr, "Usage: apkg install <package>\n");
            return 1;
        }
        return apkg_install(argv[2]);
    }
    
    if (strcmp(command, "build") == 0) {
        return apkg_build();
    }
    
    if (strcmp(command, "run") == 0) {
        return apkg_run();
    }
    
    if (strcmp(command, "test") == 0) {
        return apkg_test();
    }
    
    if (strcmp(command, "publish") == 0) {
        return apkg_publish();
    }
    
    if (strcmp(command, "search") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: 'search' requires a query\n");
            fprintf(stderr, "Usage: apkg search <query>\n");
            return 1;
        }
        return apkg_search(argv[2]);
    }
    
    if (strcmp(command, "update") == 0) {
        return apkg_update();
    }
    
    fprintf(stderr, "Error: Unknown command '%s'\n", command);
    fprintf(stderr, "Run 'apkg help' for usage information\n");
    return 1;
}

