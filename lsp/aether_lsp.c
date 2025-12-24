#include "aether_lsp.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// LSP Server lifecycle
LSPServer* lsp_server_create() {
    LSPServer* server = (LSPServer*)malloc(sizeof(LSPServer));
    server->input = stdin;
    server->output = stdout;
    server->log_file = fopen("aether-lsp.log", "w");
    server->running = 1;
    server->open_documents = NULL;
    server->document_contents = NULL;
    server->document_count = 0;
    return server;
}

void lsp_server_free(LSPServer* server) {
    if (!server) return;
    
    for (int i = 0; i < server->document_count; i++) {
        free(server->open_documents[i]);
        free(server->document_contents[i]);
    }
    free(server->open_documents);
    free(server->document_contents);
    
    if (server->log_file) {
        fclose(server->log_file);
    }
    
    free(server);
}

void lsp_server_run(LSPServer* server) {
    lsp_log(server, "Aether LSP Server starting...");
    
    while (server->running) {
        JSONRPCMessage* msg = lsp_read_message(server);
        if (!msg) break;
        
        lsp_log(server, "Received: %s (id: %s)", msg->method ? msg->method : "null", msg->id ? msg->id : "null");
        
        if (msg->method) {
            if (strcmp(msg->method, "initialize") == 0) {
                lsp_handle_initialize(server, msg->id);
            } else if (strcmp(msg->method, "textDocument/completion") == 0) {
                // Parse params to extract URI, line, character
                lsp_handle_completion(server, msg->id, "file:///test.ae", 0, 0);
            } else if (strcmp(msg->method, "textDocument/hover") == 0) {
                lsp_handle_hover(server, msg->id, "file:///test.ae", 0, 0);
            } else if (strcmp(msg->method, "textDocument/definition") == 0) {
                lsp_handle_definition(server, msg->id, "file:///test.ae", 0, 0);
            } else if (strcmp(msg->method, "shutdown") == 0) {
                server->running = 0;
                lsp_send_response(server, msg->id, "null");
            } else if (strcmp(msg->method, "exit") == 0) {
                server->running = 0;
            }
        }
        
        lsp_free_message(msg);
    }
    
    lsp_log(server, "Aether LSP Server shutting down...");
}

// Document management
void lsp_document_open(LSPServer* server, const char* uri, const char* text) {
    server->open_documents = (char**)realloc(server->open_documents, (server->document_count + 1) * sizeof(char*));
    server->document_contents = (char**)realloc(server->document_contents, (server->document_count + 1) * sizeof(char*));
    server->open_documents[server->document_count] = strdup(uri);
    server->document_contents[server->document_count] = strdup(text);
    server->document_count++;
}

const char* lsp_document_get(LSPServer* server, const char* uri) {
    for (int i = 0; i < server->document_count; i++) {
        if (strcmp(server->open_documents[i], uri) == 0) {
            return server->document_contents[i];
        }
    }
    return NULL;
}

// LSP features
void lsp_handle_initialize(LSPServer* server, const char* id) {
    const char* capabilities = 
        "{"
        "\"capabilities\":{"
        "\"textDocumentSync\":1,"
        "\"completionProvider\":{\"triggerCharacters\":[\".\",\":\"]},"
        "\"hoverProvider\":true,"
        "\"definitionProvider\":true,"
        "\"documentSymbolProvider\":true"
        "}"
        "}";
    lsp_send_response(server, id, capabilities);
}

void lsp_handle_completion(LSPServer* server, const char* id, const char* uri, int line, int character) {
    // Return completion items for Aether keywords and stdlib
    const char* completions =
        "{"
        "\"isIncomplete\":false,"
        "\"items\":["
        "{\"label\":\"actor\",\"kind\":14},"
        "{\"label\":\"spawn\",\"kind\":3},"
        "{\"label\":\"make\",\"kind\":3},"
        "{\"label\":\"let\",\"kind\":14},"
        "{\"label\":\"struct\",\"kind\":14},"
        "{\"label\":\"import\",\"kind\":14},"
        "{\"label\":\"export\",\"kind\":14}"
        "]"
        "}";
    lsp_send_response(server, id, completions);
}

void lsp_handle_hover(LSPServer* server, const char* id, const char* uri, int line, int character) {
    // Return hover information
    const char* hover =
        "{"
        "\"contents\":{"
        "\"kind\":\"markdown\","
        "\"value\":\"**Aether Actor**\\n\\nLightweight concurrent actor\""
        "}"
        "}";
    lsp_send_response(server, id, hover);
}

void lsp_handle_definition(LSPServer* server, const char* id, const char* uri, int line, int character) {
    // Return definition location
    lsp_send_response(server, id, "null");
}

void lsp_handle_document_symbol(LSPServer* server, const char* id, const char* uri) {
    // Return document symbols (functions, actors, etc.)
    lsp_send_response(server, id, "[]");
}

// JSON-RPC (simplified implementation)
JSONRPCMessage* lsp_read_message(LSPServer* server) {
    char header[1024];
    int content_length = 0;
    
    // Read headers
    while (fgets(header, sizeof(header), server->input)) {
        if (strstr(header, "Content-Length:")) {
            sscanf(header, "Content-Length: %d", &content_length);
        }
        if (strcmp(header, "\r\n") == 0 || strcmp(header, "\n") == 0) {
            break;
        }
    }
    
    if (content_length == 0) return NULL;
    
    // Read content
    char* content = (char*)malloc(content_length + 1);
    fread(content, 1, content_length, server->input);
    content[content_length] = '\0';
    
    // Parse JSON (simplified - would use a proper JSON parser in production)
    JSONRPCMessage* msg = (JSONRPCMessage*)malloc(sizeof(JSONRPCMessage));
    msg->method = NULL;
    msg->id = NULL;
    msg->params = NULL;
    
    // Extract method
    char* method_start = strstr(content, "\"method\":");
    if (method_start) {
        method_start = strchr(method_start, '"');
        method_start = strchr(method_start + 1, '"') + 1;
        char* method_end = strchr(method_start, '"');
        msg->method = strndup(method_start, method_end - method_start);
    }
    
    free(content);
    return msg;
}

void lsp_free_message(JSONRPCMessage* msg) {
    if (!msg) return;
    free(msg->method);
    free(msg->id);
    free(msg->params);
    free(msg);
}

void lsp_send_response(LSPServer* server, const char* id, const char* result) {
    char response[4096];
    snprintf(response, sizeof(response),
             "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":%s}",
             id ? id : "null", result);
    
    fprintf(server->output, "Content-Length: %zu\r\n\r\n%s", strlen(response), response);
    fflush(server->output);
}

void lsp_send_notification(LSPServer* server, const char* method, const char* params) {
    char notification[4096];
    snprintf(notification, sizeof(notification),
             "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}",
             method, params);
    
    fprintf(server->output, "Content-Length: %zu\r\n\r\n%s", strlen(notification), notification);
    fflush(server->output);
}

void lsp_log(LSPServer* server, const char* format, ...) {
    if (!server->log_file) return;
    
    va_list args;
    va_start(args, format);
    vfprintf(server->log_file, format, args);
    fprintf(server->log_file, "\n");
    fflush(server->log_file);
    va_end(args);
}

