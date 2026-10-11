
# WOS 1.0 — Way Of Success

A small educational programming language that teaches beginners
low-level concepts (pointers, manual memory allocation) without
C's scary syntax.

WOS compiles to C, then to a binary.

## Features

- **Simple syntax** — easier than C, but keeps the low-level power
- **Compiles to C** — works on any platform that has a C compiler
- **Built-in GUI** — WosGUI for quick interface building
- **Library support** — create and import libraries
- **Vim integration** — syntax highlighting + autocomplete + diagnostics
- **Language Server** — LSP for editors

## Requirements

- **clang** — to compile the compiler
- **node.js** — for the language server (optional)
- **vim** — for the editor experience (optional)

Install on Termux:

```bash
pkg install clang nodejs vim
```

Installation

1. Clone the repository

```bash
git clone https://github.com/mohammedghouj989-rgb/WOS-1.0.git
cd WOS-1.0
```

2. Build the compiler

```bash
cd compiler
clang wospras.c -o wos
```

3. Use from the project folder

```bash
./wos ../examples/hello.wos
```

4. (Recommended) Install globally

```bash
cp wos $PREFIX/bin/wos
chmod +x $PREFIX/bin/wos
```

Now you can use WOS from anywhere:

```bash
cd ~
echo 'print("Hello, World!");' > hello.wos
wos hello.wos
```

Quick Start

Create a file hello.wos:

```wos
print("Hello, World!");
```

Run it:

```bash
wos hello.wos
```

Output:

```
Hello, World!
```

Examples

Check the examples/ folder for more:

File What it shows
hello.wos Print "Hello, World!"
variables.wos Variables and arithmetic
condition.wos if statements
loop.wos while loops
function.wos Functions with return values
math.wos Math operations
gui_button.wos WosGUI button
mylib.wos Library creation
test_import.wos Importing libraries

Documentation

Full docs are in the docs/ folder:

· Getting Started
· Syntax Reference
· Examples
· Language Server
· Vim Setup

Project Structure

```
WOS 1.0/
├── compiler/          # The WOS compiler (Lexer + Parser + Codegen)
├── docs/              # Documentation
├── examples/          # Example WOS programs
├── language-server/   # LSP server for editors
└── vim/               # Vim syntax highlighting + config
```

Language Overview

Variables

```wos
int x = 10;
string name = "WOS";
float pi = 3.14;
```

Conditions

```wos
if x > 5 {
    print(x);
} else {
    print(0);
}
```

Loops

```wos
while x < 10 {
    print(x);
    x = x + 1;
}
```

Functions

```wos
function add(int a, int b) {
    return a + b;
}

int result = add(5, 3);
print(result);  // 8
```

Libraries

```wos
library "mylib";

function add(int a, int b) {
    return a + b;
}
```

Compile with WOS, and you get:

· output.c — the C code (no main)
· mylib.h — header file

Imports

```wos
import "math.h";

int x = 10;
print(x);
```

WosGUI (Hidden Feature)

```wos
window "My App" 800 x 600;
text "Hello" top;
button rectangle "Click" center 100 x 40;
input "Enter name" bottom;
```

Vim Setup

For syntax highlighting + autocomplete:

```bash
mkdir -p ~/.vim/syntax ~/.vim/ftdetect
cp vim/syntax/wos.vim ~/.vim/syntax/
cp vim/ftdetect/wos.vim ~/.vim/ftdetect/
cat vim/vimrc >> ~/.vimrc
```

For the language server:

```bash
cd language-server
npm install
```

See docs/language-server/README.md for details.

Contributing

WOS is in early development. Contributions are welcome.

License

MIT License — see LICENSE for details.

Author

CCODE (Libya)

Built on a phone, in Termux, with Vim.

