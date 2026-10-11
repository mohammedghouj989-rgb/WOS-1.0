# WOS 1.0 (Way Of Success)

A small educational programming language that teaches beginners
low-level concepts (pointers, manual memory allocation) without
C's scary syntax.

## What's inside

- **compiler/** — The WOS compiler (Lexer + Parser + Code Generator)
- **language-server/** — LSP server for autocomplete + diagnostics
- **vim/** — Vim syntax highlighting + config
- **examples/** — Example WOS programs

## Requirements

- clang
- node.js (for the language server)
- vim (with vim-lsp + asyncomplete)

## Build

```bash
cd compiler
clang wospras.c -o wos
