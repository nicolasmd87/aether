#include "aether_os.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int os_system(const char* cmd) {
    if (!cmd) return -1;
    return system(cmd);
}

char* os_exec(const char* cmd) {
    if (!cmd) return NULL;

#ifdef _WIN32
    FILE* pipe = _popen(cmd, "r");
#else
    FILE* pipe = popen(cmd, "r");
#endif
    if (!pipe) return NULL;

    size_t capacity = 1024;
    size_t len = 0;
    char* result = (char*)malloc(capacity);
    if (!result) {
#ifdef _WIN32
        _pclose(pipe);
#else
        pclose(pipe);
#endif
        return NULL;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        size_t chunk = strlen(buffer);
        if (len + chunk + 1 > capacity) {
            capacity *= 2;
            char* new_result = (char*)realloc(result, capacity);
            if (!new_result) {
                free(result);
#ifdef _WIN32
                _pclose(pipe);
#else
                pclose(pipe);
#endif
                return NULL;
            }
            result = new_result;
        }
        memcpy(result + len, buffer, chunk);
        len += chunk;
    }

    result[len] = '\0';

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

char* os_getenv(const char* name) {
    if (!name) return NULL;
    char* val = getenv(name);
    if (!val) return NULL;
    return strdup(val);
}
