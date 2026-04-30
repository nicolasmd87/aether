/*
 * Aether Language Support — VS Code / Cursor extension entry point.
 *
 * Job: when a `.ae` file is opened, locate the `aether-lsp` binary and
 * start a language-server-protocol client against it. The user shouldn't
 * have to wire anything up by hand — this resolver tries every place
 * an `aether-lsp` realistically lives, in order, and only falls back to
 * a status-bar warning if none are present.
 *
 * Resolution order:
 *   1. `aether.lsp.path` setting (explicit override always wins).
 *   2. The current workspace's `build/aether-lsp` — the common case for
 *      anyone working in the Aether repo itself; building the LSP is
 *      `make lsp` in that workspace, and the resulting binary belongs
 *      to that very tree.
 *   3. `aether-lsp` resolved through PATH (covers system installs).
 *   4. Hardcoded common install dirs (`~/.local/bin`, `~/.aether/bin`,
 *      `/usr/local/bin`, `/opt/homebrew/bin`) for shells that don't
 *      have those directories on PATH (notably non-interactive Cursor
 *      child processes on some macOS configurations).
 *
 * Disabling: set `aether.lsp.enable: false` for syntax-only mode.
 */

import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind,
} from 'vscode-languageclient/node';

let client: LanguageClient | undefined;

const LSP_BINARY_NAME = process.platform === 'win32' ? 'aether-lsp.exe' : 'aether-lsp';
const OUTPUT_CHANNEL_NAME = 'Aether Language Server';

// ---------------------------------------------------------------------------
// Resolver: find an executable aether-lsp the user did not have to configure.
// ---------------------------------------------------------------------------

function isExecutable(p: string): boolean {
    try {
        const st = fs.statSync(p);
        if (!st.isFile()) return false;
        // POSIX: any-x bit is enough; Windows: extension matters but we
        // already filtered to .exe in LSP_BINARY_NAME above.
        if (process.platform === 'win32') return true;
        return (st.mode & 0o111) !== 0;
    } catch {
        return false;
    }
}

function expandHome(p: string): string {
    if (p === '~' || p.startsWith('~/')) return path.join(os.homedir(), p.slice(2));
    return p;
}

function searchOnPath(): string | undefined {
    const PATH = process.env.PATH ?? '';
    const sep = process.platform === 'win32' ? ';' : ':';
    for (const dir of PATH.split(sep)) {
        if (!dir) continue;
        const candidate = path.join(dir, LSP_BINARY_NAME);
        if (isExecutable(candidate)) return candidate;
    }
    return undefined;
}

function searchWorkspaceBuild(): string | undefined {
    for (const folder of vscode.workspace.workspaceFolders ?? []) {
        const candidate = path.join(folder.uri.fsPath, 'build', LSP_BINARY_NAME);
        if (isExecutable(candidate)) return candidate;
    }
    return undefined;
}

function searchCommonInstallDirs(): string | undefined {
    const home = os.homedir();
    const dirs = [
        path.join(home, '.local', 'bin'),
        path.join(home, '.aether', 'bin'),
        '/usr/local/bin',
        '/opt/homebrew/bin',
    ];
    for (const dir of dirs) {
        const candidate = path.join(dir, LSP_BINARY_NAME);
        if (isExecutable(candidate)) return candidate;
    }
    return undefined;
}

interface ResolvedLsp {
    binary: string;
    source: string;
}

function resolveLspBinary(): ResolvedLsp | undefined {
    const config = vscode.workspace.getConfiguration('aether');
    const explicit = (config.get<string>('lsp.path') ?? '').trim();

    if (explicit) {
        const expanded = expandHome(explicit);
        const absolute = path.isAbsolute(expanded)
            ? expanded
            : (searchOnPath() && path.basename(expanded) === LSP_BINARY_NAME
                ? searchOnPath()
                : expanded);
        if (absolute && isExecutable(absolute)) {
            return { binary: absolute, source: 'aether.lsp.path setting' };
        }
        // Explicit override that doesn't resolve is a configuration error;
        // fall through but warn — we'd rather surface the misconfiguration
        // than silently substitute a different binary.
        vscode.window.showWarningMessage(
            `aether.lsp.path is set to "${explicit}" but no executable was found there. ` +
            `Falling back to auto-detection.`,
        );
    }

    const ws = searchWorkspaceBuild();
    if (ws) return { binary: ws, source: 'workspace build/' };

    const onPath = searchOnPath();
    if (onPath) return { binary: onPath, source: 'PATH' };

    const common = searchCommonInstallDirs();
    if (common) return { binary: common, source: 'common install dir' };

    return undefined;
}

// ---------------------------------------------------------------------------
// Activation.
// ---------------------------------------------------------------------------

function showLspMissingMessage(channel: vscode.OutputChannel): void {
    const msg =
        'Aether language server (aether-lsp) was not found. ' +
        'Build it with `make lsp` in the Aether repo, or set `aether.lsp.path` ' +
        'to point at the binary.';
    channel.appendLine(`[aether] ${msg}`);
    vscode.window.showWarningMessage(msg, 'Open Settings').then((choice) => {
        if (choice === 'Open Settings') {
            vscode.commands.executeCommand(
                'workbench.action.openSettings',
                'aether.lsp.path',
            );
        }
    });
}

export async function activate(context: vscode.ExtensionContext): Promise<void> {
    const channel = vscode.window.createOutputChannel(OUTPUT_CHANNEL_NAME);
    context.subscriptions.push(channel);

    const config = vscode.workspace.getConfiguration('aether');
    const enabled = config.get<boolean>('lsp.enable', true);
    if (!enabled) {
        channel.appendLine('[aether] LSP disabled via aether.lsp.enable; syntax-only mode.');
        return;
    }

    const resolved = resolveLspBinary();
    if (!resolved) {
        showLspMissingMessage(channel);
        return;
    }

    channel.appendLine(`[aether] Using LSP server at ${resolved.binary} (${resolved.source}).`);

    const serverOptions: ServerOptions = {
        run:   { command: resolved.binary, transport: TransportKind.stdio },
        debug: { command: resolved.binary, transport: TransportKind.stdio },
    };

    const clientOptions: LanguageClientOptions = {
        documentSelector: [
            { scheme: 'file',     language: 'aether' },
            { scheme: 'untitled', language: 'aether' },
        ],
        outputChannel: channel,
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*.ae'),
        },
        initializationOptions: {
            clientName: 'vscode-aether',
        },
    };

    client = new LanguageClient(
        'aether',
        'Aether Language Server',
        serverOptions,
        clientOptions,
    );

    try {
        await client.start();
        channel.appendLine('[aether] Language server connected.');
    } catch (err) {
        channel.appendLine(`[aether] Failed to start language server: ${err}`);
        vscode.window.showErrorMessage(
            `Aether language server failed to start: ${err instanceof Error ? err.message : String(err)}`,
        );
    }

    // Settings change → reactivate. Cheaper than restarting the editor
    // when the user updates aether.lsp.path or aether.lsp.enable.
    context.subscriptions.push(
        vscode.workspace.onDidChangeConfiguration(async (event) => {
            if (
                event.affectsConfiguration('aether.lsp.path') ||
                event.affectsConfiguration('aether.lsp.enable')
            ) {
                channel.appendLine('[aether] Configuration changed; restarting language server.');
                await deactivate();
                await activate(context);
            }
        }),
    );
}

export async function deactivate(): Promise<void> {
    if (!client) return;
    try {
        await client.stop();
    } catch {
        /* swallow — extension shutdown is best-effort */
    } finally {
        client = undefined;
    }
}
