#include "aether_lsp.h"

int main(int argc, char** argv) {
    LSPServer* server = lsp_server_create();
    lsp_server_run(server);
    lsp_server_free(server);
    return 0;
}

