import * as vscode from 'vscode';
import * as path from 'path';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    Executable,
    TransportKind
} from 'vscode-languageclient/node';

let client: LanguageClient | undefined;

export function activate(context: vscode.ExtensionContext) {
    const config = vscode.workspace.getConfiguration('aether');
    let lspPath = config.get<string>('lsp.path', 'aether-lsp');
    
    if (!path.isAbsolute(lspPath)) {
        const possiblePaths = [
            lspPath,
            path.join(process.env.HOME || process.env.USERPROFILE || '', '.local', 'bin', lspPath),
            path.join('C:', 'Aether', 'bin', lspPath + '.exe'),
            '/usr/local/bin/' + lspPath,
            path.join(context.extensionPath, '..', '..', 'lsp', lspPath)
        ];
        
        for (const p of possiblePaths) {
            try {
                const stat = require('fs').statSync(p);
                if (stat.isFile()) {
                    lspPath = p;
                    break;
                }
            } catch (e) {
            }
        }
    }
    
    const serverExecutable: Executable = {
        command: lspPath,
        transport: TransportKind.stdio
    };
    
    const serverOptions: ServerOptions = {
        run: serverExecutable,
        debug: serverExecutable
    };
    
    const clientOptions: LanguageClientOptions = {
        documentSelector: [
            { scheme: 'file', language: 'aether' },
            { scheme: 'untitled', language: 'aether' }
        ],
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*.ae')
        },
        outputChannelName: 'Aether Language Server',
        traceOutputChannel: vscode.window.createOutputChannel('Aether Language Server Trace')
    };
    
    try {
        client = new LanguageClient(
            'aetherLSP',
            'Aether Language Server',
            serverOptions,
            clientOptions
        );
        
        client.start().then(() => {
            vscode.window.showInformationMessage('Aether Language Server started');
        }).catch((error) => {
            vscode.window.showErrorMessage(`Failed to start Aether LSP: ${error.message}`);
            console.error('LSP Error:', error);
        });
        
        context.subscriptions.push(client);
    } catch (error: any) {
        vscode.window.showErrorMessage(`Error initializing Aether LSP: ${error.message}`);
        console.error('LSP Init Error:', error);
    }
    
    const commandRunFile = vscode.commands.registerCommand('aether.runFile', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor || editor.document.languageId !== 'aether') {
            vscode.window.showErrorMessage('No active Aether file');
            return;
        }
        
        const filePath = editor.document.uri.fsPath;
        const terminal = vscode.window.createTerminal('Aether');
        terminal.show();
        terminal.sendText(`aetherc "${filePath}" && ./output`);
    });
    
    context.subscriptions.push(commandRunFile);
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}

