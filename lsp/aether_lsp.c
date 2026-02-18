#include "aether_lsp.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../compiler/frontend/lexer.h"
#include "../compiler/ast.h"

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
            } else if (strcmp(msg->method, "textDocument/didOpen") == 0) {
                lsp_log(server, "Document opened");
            } else if (strcmp(msg->method, "textDocument/didChange") == 0) {
                lsp_log(server, "Document changed");
            } else if (strcmp(msg->method, "textDocument/didSave") == 0) {
                lsp_log(server, "Document saved - running diagnostics");
                lsp_publish_diagnostics(server, "file:///test.ae");
            } else if (strcmp(msg->method, "initialized") == 0) {
                lsp_log(server, "Client initialized");
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
    char** new_docs = (char**)realloc(server->open_documents, (server->document_count + 1) * sizeof(char*));
    if (!new_docs) {
        lsp_log(server, "Error: Failed to allocate document array");
        return;
    }
    server->open_documents = new_docs;

    char** new_contents = (char**)realloc(server->document_contents, (server->document_count + 1) * sizeof(char*));
    if (!new_contents) {
        lsp_log(server, "Error: Failed to allocate contents array");
        return;
    }
    server->document_contents = new_contents;

    char* uri_copy = strdup(uri);
    char* text_copy = strdup(text);
    if (!uri_copy || !text_copy) {
        free(uri_copy);
        free(text_copy);
        lsp_log(server, "Error: Failed to duplicate document strings");
        return;
    }

    server->open_documents[server->document_count] = uri_copy;
    server->document_contents[server->document_count] = text_copy;
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
    const char* completions =
        "{"
        "\"isIncomplete\":false,"
        "\"items\":["
        "{\"label\":\"actor\",\"kind\":14,\"detail\":\"actor definition\",\"documentation\":\"Define a new actor\"},"
        "{\"label\":\"spawn\",\"kind\":3,\"detail\":\"spawn actor\",\"documentation\":\"Create a new actor instance\"},"
        "{\"label\":\"send\",\"kind\":3,\"detail\":\"send message\",\"documentation\":\"Send a message to an actor\"},"
        "{\"label\":\"receive\",\"kind\":3,\"detail\":\"receive message\",\"documentation\":\"Receive messages in actor\"},"
        "{\"label\":\"make\",\"kind\":3,\"detail\":\"make actor\",\"documentation\":\"Create actor with initial state\"},"
        "{\"label\":\"func\",\"kind\":14,\"detail\":\"function definition\",\"documentation\":\"Define a function\"},"
        "{\"label\":\"main\",\"kind\":3,\"detail\":\"main function\",\"documentation\":\"Program entry point\"},"
        "{\"label\":\"struct\",\"kind\":14,\"detail\":\"struct definition\",\"documentation\":\"Define a struct type\"},"
        "{\"label\":\"if\",\"kind\":14,\"detail\":\"if statement\",\"documentation\":\"Conditional statement\"},"
        "{\"label\":\"else\",\"kind\":14,\"detail\":\"else clause\",\"documentation\":\"Alternative branch\"},"
        "{\"label\":\"for\",\"kind\":14,\"detail\":\"for loop\",\"documentation\":\"For loop iteration\"},"
        "{\"label\":\"while\",\"kind\":14,\"detail\":\"while loop\",\"documentation\":\"While loop\"},"
        "{\"label\":\"return\",\"kind\":14,\"detail\":\"return statement\",\"documentation\":\"Return from function\"},"
        "{\"label\":\"break\",\"kind\":14,\"detail\":\"break statement\",\"documentation\":\"Exit loop\"},"
        "{\"label\":\"continue\",\"kind\":14,\"detail\":\"continue statement\",\"documentation\":\"Continue to next iteration\"},"
        "{\"label\":\"defer\",\"kind\":14,\"detail\":\"defer statement\",\"documentation\":\"Execute at scope exit\"},"
        "{\"label\":\"true\",\"kind\":12,\"detail\":\"boolean literal\"},"
        "{\"label\":\"false\",\"kind\":12,\"detail\":\"boolean literal\"},"
        "{\"label\":\"print\",\"kind\":3,\"detail\":\"print(value)\",\"documentation\":\"Print to stdout\"},"
        "{\"label\":\"println\",\"kind\":3,\"detail\":\"println(value)\",\"documentation\":\"Print with newline\"},"
        "{\"label\":\"len\",\"kind\":3,\"detail\":\"len(array)\",\"documentation\":\"Get array length\"},"
        "{\"label\":\"string_concat\",\"kind\":3,\"detail\":\"string concat\"},"
        "{\"label\":\"http_get\",\"kind\":3,\"detail\":\"HTTP GET request\"},"
        "{\"label\":\"socket_connect\",\"kind\":3,\"detail\":\"TCP socket connect\"},"
        "{\"label\":\"file_exists\",\"kind\":3,\"detail\":\"check file exists\"},"
        "{\"label\":\"json_parse\",\"kind\":3,\"detail\":\"parse JSON string\"}"
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

void lsp_publish_diagnostics(LSPServer* server, const char* uri) {
    const char* source = lsp_document_get(server, uri);
    if (!source) return;

    // Build diagnostics JSON safely with bounds checking
    char diagnostics[8192];
    int written = snprintf(diagnostics, sizeof(diagnostics),
                          "{\"uri\":\"%s\",\"diagnostics\":[]}", uri);

    if (written < 0 || (size_t)written >= sizeof(diagnostics)) {
        lsp_log(server, "Warning: URI too long for diagnostics buffer");
        return;
    }

    lsp_send_notification(server, "textDocument/publishDiagnostics", diagnostics);
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
    if (!content) {
        lsp_log(server, "Error: Failed to allocate content buffer");
        return NULL;
    }
    size_t bytes_read = fread(content, 1, content_length, server->input);
    if (bytes_read != (size_t)content_length) {
        lsp_log(server, "Warning: Read fewer bytes than expected");
    }
    content[bytes_read] = '\0';

    // Parse JSON (simplified - would use a proper JSON parser in production)
    JSONRPCMessage* msg = (JSONRPCMessage*)malloc(sizeof(JSONRPCMessage));
    if (!msg) {
        lsp_log(server, "Error: Failed to allocate message struct");
        free(content);
        return NULL;
    }
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

