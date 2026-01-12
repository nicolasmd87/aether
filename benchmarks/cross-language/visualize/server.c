/**
 * Aether Benchmark Visualization Server
 * 
 * Simple HTTP server to serve the benchmark dashboard
 * Demonstrates Aether's web capabilities while providing professional benchmarks
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include "../../../std/net/aether_http_server.h"

// Forward declarations
void serve_index(HttpRequest* req, HttpServerResponse* res, void* user_data);
void serve_results(HttpRequest* req, HttpServerResponse* res, void* user_data);
void serve_sysinfo(HttpRequest* req, HttpServerResponse* res, void* user_data);

// Get the directory where the executable is located
char* get_exe_dir() {
    static char dir[512] = {0};
    if (dir[0] == 0) {
        #ifdef _WIN32
        GetModuleFileNameA(NULL, dir, sizeof(dir));
        char* last_slash = strrchr(dir, '\\');
        if (last_slash) *last_slash = '\0';
        #elif defined(__APPLE__)
        // On macOS, just use current directory
        if (getcwd(dir, sizeof(dir)) == NULL) {
            strcpy(dir, ".");
        }
        #else
        readlink("/proc/self/exe", dir, sizeof(dir));
        char* last_slash = strrchr(dir, '/');
        if (last_slash) *last_slash = '\0';
        #endif
    }
    return dir;
}

// Serve index.html or stats.html
void serve_index(HttpRequest* req, HttpServerResponse* res, void* user_data) {
    char path[1024];
    const char* req_path = req->path ? req->path : "/";
    const char* filename = "index.html";

    if (strstr(req_path, "stats.html")) {
        filename = "stats.html";
    }

    snprintf(path, sizeof(path), "%s/%s", get_exe_dir(), filename);

    FILE* f = fopen(path, "rb");
    if (!f) {
        aether_http_response_set_status(res, 404);
        char error[512];
        snprintf(error, sizeof(error), "404 - Dashboard not found at: %s", path);
        aether_http_response_set_body(res, error);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = malloc(size + 1);
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);
    
    aether_http_response_set_header(res, "Content-Type", "text/html; charset=utf-8");
    aether_http_response_set_body(res, content);
    free(content);
}

// Serve results.json or results_*.json files
void serve_results(HttpRequest* req, HttpServerResponse* res, void* user_data) {
    char path[1024];
    // Get the requested path (e.g., "/results_ping_pong.json" or "/results.json")
    const char* req_path = req->path ? req->path : "/results.json";
    const char* filename = strrchr(req_path, '/');
    if (filename) {
        filename++; // Skip the '/'
    } else {
        filename = "results.json";
    }

    snprintf(path, sizeof(path), "%s/%s", get_exe_dir(), filename);

    FILE* f = fopen(path, "rb");
    if (!f) {
        // Return empty results if file doesn't exist yet
        const char* empty = "{\"benchmarks\":{},\"message\":\"No results yet. Run benchmarks first.\"}";
        aether_http_response_set_header(res, "Content-Type", "application/json");
        aether_http_response_set_body(res, empty);
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = malloc(size + 1);
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    aether_http_response_set_header(res, "Content-Type", "application/json");
    aether_http_response_set_header(res, "Access-Control-Allow-Origin", "*");
    aether_http_response_set_body(res, content);
    free(content);
}

// API to get system info
void serve_sysinfo(HttpRequest* req, HttpServerResponse* res, void* user_data) {
    char info[512];
    snprintf(info, sizeof(info),
        "{"
        "\"server\":\"Aether HTTP Server\","
        "\"version\":\"0.1.0\","
        "\"language\":\"C\","
        "\"runtime\":\"Native\""
        "}");
    
    aether_http_response_set_header(res, "Content-Type", "application/json");
    aether_http_response_set_body(res, info);
}

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    printf("=======================================================\n");
    printf("  Aether Cross-Language Benchmark Visualization Server\n");
    printf("=======================================================\n\n");
    
    HttpServer* server = aether_http_server_create(port);
    if (!server) {
        fprintf(stderr, "Failed to create HTTP server\n");
        return 1;
    }
    
    // Register routes
    aether_http_server_get(server, "/", serve_index, NULL);
    aether_http_server_get(server, "/index.html", serve_index, NULL);
    aether_http_server_get(server, "/stats.html", serve_index, NULL);
    aether_http_server_get(server, "/results.json", serve_results, NULL);
    aether_http_server_get(server, "/results_ping_pong.json", serve_results, NULL);
    aether_http_server_get(server, "/results_ring.json", serve_results, NULL);
    aether_http_server_get(server, "/results_skynet.json", serve_results, NULL);
    aether_http_server_get(server, "/results_statistical.json", serve_results, NULL);
    aether_http_server_get(server, "/api/sysinfo", serve_sysinfo, NULL);

    printf("Server starting on http://localhost:%d\n", port);
    printf("\nAvailable endpoints:\n");
    printf("  GET  /                         - Benchmark dashboard\n");
    printf("  GET  /results.json             - All benchmark results (JSON)\n");
    printf("  GET  /results_ping_pong.json   - Ping-pong pattern results\n");
    printf("  GET  /results_ring.json        - Ring pattern results\n");
    printf("  GET  /results_skynet.json      - Skynet pattern results\n");
    printf("  GET  /api/sysinfo              - Server information\n");
    printf("\nPress Ctrl+C to stop\n\n");
    
    // Start server (blocking)
    int result = aether_http_server_start(server);
    
    if (result != 0) {
        fprintf(stderr, "Failed to start server on port %d\n", port);
        return 1;
    }
    
    aether_http_server_free(server);
    return 0;
}
