#!/usr/bin/env node

const {
    createConnection,
    TextDocuments,
    ProposedFeatures,
    CompletionItemKind,
    TextDocumentSyncKind,
    Diagnostic,
    DiagnosticSeverity
} = require('vscode-languageserver/node');

const { TextDocument } = require('vscode-languageserver-textdocument');

const connection = createConnection(ProposedFeatures.all);
const documents = new TextDocuments(TextDocument);

// ============================================================
// WOS KEYWORDS
// ============================================================
const KEYWORDS = [
    { label: 'int',    kind: CompletionItemKind.Keyword, detail: 'Type' },
    { label: 'float',  kind: CompletionItemKind.Keyword, detail: 'Type' },
    { label: 'double', kind: CompletionItemKind.Keyword, detail: 'Type' },
    { label: 'bool',   kind: CompletionItemKind.Keyword, detail: 'Type' },
    { label: 'string', kind: CompletionItemKind.Keyword, detail: 'Type' },
    { label: 'void',   kind: CompletionItemKind.Keyword, detail: 'Type' },
    { label: 'if',       kind: CompletionItemKind.Keyword, detail: 'Control' },
    { label: 'else',     kind: CompletionItemKind.Keyword, detail: 'Control' },
    { label: 'while',    kind: CompletionItemKind.Keyword, detail: 'Control' },
    { label: 'for',      kind: CompletionItemKind.Keyword, detail: 'Control' },
    { label: 'switch',   kind: CompletionItemKind.Keyword, detail: 'Control' },
    { label: 'case',     kind: CompletionItemKind.Keyword, detail: 'Control' },
    { label: 'break',    kind: CompletionItemKind.Keyword, detail: 'Control' },
    { label: 'continue', kind: CompletionItemKind.Keyword, detail: 'Control' },
    { label: 'return',   kind: CompletionItemKind.Keyword, detail: 'Control' },
    { label: 'struct',  kind: CompletionItemKind.Keyword, detail: 'Structure' },
    { label: 'enum',    kind: CompletionItemKind.Keyword, detail: 'Structure' },
    { label: 'typedef', kind: CompletionItemKind.Keyword, detail: 'Structure' },
    { label: 'import', kind: CompletionItemKind.Keyword, detail: 'Import' },
    { label: 'const',  kind: CompletionItemKind.Keyword, detail: 'Constant' },
    { label: 'alloc',  kind: CompletionItemKind.Function, detail: 'Memory' },
    { label: 'free',   kind: CompletionItemKind.Function, detail: 'Memory' },
    { label: 'print',  kind: CompletionItemKind.Function, detail: 'Built-in' },
    { label: 'window',    kind: CompletionItemKind.Keyword, detail: 'GUI' },
    { label: 'button',    kind: CompletionItemKind.Keyword, detail: 'GUI' },
    { label: 'text',      kind: CompletionItemKind.Keyword, detail: 'GUI' },
    { label: 'input',     kind: CompletionItemKind.Keyword, detail: 'GUI' },
    { label: 'label',     kind: CompletionItemKind.Keyword, detail: 'GUI' },
    { label: 'rectangle', kind: CompletionItemKind.Keyword, detail: 'GUI Shape' },
    { label: 'circle',    kind: CompletionItemKind.Keyword, detail: 'GUI Shape' },
    { label: 'triangle',  kind: CompletionItemKind.Keyword, detail: 'GUI Shape' },
    { label: 'center',    kind: CompletionItemKind.Keyword, detail: 'GUI Position' },
    { label: 'left',      kind: CompletionItemKind.Keyword, detail: 'GUI Position' },
    { label: 'right',     kind: CompletionItemKind.Keyword, detail: 'GUI Position' },
    { label: 'top',       kind: CompletionItemKind.Keyword, detail: 'GUI Position' },
    { label: 'bottom',    kind: CompletionItemKind.Keyword, detail: 'GUI Position' }
];

// ============================================================
// INITIALIZE
// ============================================================
connection.onInitialize((params) => {
    return {
        capabilities: {
            textDocumentSync: TextDocumentSyncKind.Incremental,
            completionProvider: {
                resolveProvider: false,
                triggerCharacters: ['.', ' ', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
                                    'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
                                    'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z']
            }
        }
    };
});

// ============================================================
// COMPLETION
// ============================================================
connection.onCompletion((params) => {
    return KEYWORDS;
});

// ============================================================
// DIAGNOSTICS
// ============================================================
function validateDocument(doc) {
    const diagnostics = [];
    const text = doc.getText();
    const lines = text.split('\n');

    for (let i = 0; i < lines.length; i++) {
        const line = lines[i];
        const trimmed = line.trim();
        if (trimmed.length === 0) continue;

        if ((trimmed.startsWith('int ') || trimmed.startsWith('float ') ||
             trimmed.startsWith('string ') || trimmed.startsWith('bool ') ||
             trimmed.startsWith('double ')) &&
            !trimmed.endsWith(';') && !trimmed.endsWith('{') && !trimmed.endsWith('}')) {
            diagnostics.push({
                severity: DiagnosticSeverity.Error,
                range: {
                    start: { line: i, character: 0 },
                    end: { line: i, character: line.length }
                },
                message: "Missing semicolon ';'",
                source: 'wos'
            });
        }

        if (trimmed.startsWith('print(') && !trimmed.endsWith(';')) {
            diagnostics.push({
                severity: DiagnosticSeverity.Error,
                range: {
                    start: { line: i, character: 0 },
                    end: { line: i, character: line.length }
                },
                message: "Missing semicolon ';' after print()",
                source: 'wos'
            });
        }
    }

    return diagnostics;
}

documents.onDidChangeContent((change) => {
    const diagnostics = validateDocument(change.document);
    connection.sendDiagnostics({
        uri: change.document.uri,
        diagnostics
    });
});

documents.listen(connection);
connection.listen();
