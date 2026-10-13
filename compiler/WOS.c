/* ============================================================================
 * WOS.c  --  Lexer for WOS (Way Of Success)
 *
 * This file is #included by wospras.c, so the whole compiler builds with:
 *     clang wospras.c -o wos
 * ========================================================================== */
#ifndef WOS_LEXER_C
#define WOS_LEXER_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum {
    TOK_KEYWORD,
    TOK_IDENTIFIER,
    TOK_NUMBER,
    TOK_STRING,
    TOK_OPERATOR,
    TOK_PUNCTUATION,
    TOK_EOF
} TokenType;

typedef struct {
    TokenType type;
    char *value;
    int line;
    int column;
} Token;

typedef struct {
    Token *items;
    int count;
    int capacity;
} TokenList;

static const char *WOS_KEYWORDS[] = {
    "int", "float", "double", "bool", "string", "void", "list",
    "if", "else", "while", "for", "return", "function", "struct",
    "library", "import", "use", "alloc", "free", "print",
    "window", "button", "text", "input", "label",
    "rectangle", "circle", "triangle",
    "center", "left", "right", "top", "bottom",
    "const", "move", "borrow", "mut",
    "switch", "case", "break", "continue", "enum", "typedef",
    "true", "false",
    NULL
};

/* strdup is not part of strict C11, so we carry our own. */
static char *wos_strdup(const char *s) {
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) { fprintf(stderr, "WOS: out of memory\n"); exit(1); }
    memcpy(p, s, n + 1);
    return p;
}

static int lexer_is_keyword(const char *s) {
    for (int i = 0; WOS_KEYWORDS[i]; i++)
        if (strcmp(WOS_KEYWORDS[i], s) == 0) return 1;
    return 0;
}

static void lex_error(int line, int col, const char *msg) {
    fprintf(stderr, "WOS lexer error at line %d, column %d: %s\n", line, col, msg);
    exit(1);
}

static void token_list_push(TokenList *tl, TokenType type, const char *start,
                            int len, int line, int col) {
    if (tl->count >= tl->capacity) {
        tl->capacity = tl->capacity ? tl->capacity * 2 : 256;
        tl->items = (Token *)realloc(tl->items, sizeof(Token) * tl->capacity);
        if (!tl->items) { fprintf(stderr, "WOS: out of memory\n"); exit(1); }
    }
    Token *t = &tl->items[tl->count++];
    t->type = type;
    t->value = (char *)malloc(len + 1);
    memcpy(t->value, start, len);
    t->value[len] = '\0';
    t->line = line;
    t->column = col;
}

/*
 * NOTE on generics: '<' and '>' are always emitted as plain OPERATOR tokens.
 * The parser recognises  list<int>  by looking at the tokens after `list`.
 */
static TokenList tokenize(const char *src) {
    static const char *TWO_CHAR[] = { "==", "!=", ">=", "<=", "&&", "||", "->", "::", NULL };
    TokenList tl = {0};
    int i = 0, line = 1, col = 1;

    while (src[i]) {
        char c = src[i];

        /* whitespace */
        if (c == '\n') { line++; col = 1; i++; continue; }
        if (c == ' ' || c == '\t' || c == '\r') { i++; col++; continue; }

        /* comments */
        if (c == '/' && src[i + 1] == '/') {
            while (src[i] && src[i] != '\n') i++;
            continue;
        }
        if (c == '/' && src[i + 1] == '*') {
            int sl = line, sc = col;
            i += 2; col += 2;
            while (src[i] && !(src[i] == '*' && src[i + 1] == '/')) {
                if (src[i] == '\n') { line++; col = 1; } else col++;
                i++;
            }
            if (!src[i]) lex_error(sl, sc, "unterminated /* comment");
            i += 2; col += 2;
            continue;
        }

        int start = i, scol = col;

        /* identifiers / keywords */
        if (isalpha((unsigned char)c) || c == '_') {
            while (isalnum((unsigned char)src[i]) || src[i] == '_') { i++; col++; }
            token_list_push(&tl, TOK_IDENTIFIER, src + start, i - start, line, scol);
            Token *t = &tl.items[tl.count - 1];
            if (lexer_is_keyword(t->value)) t->type = TOK_KEYWORD;
            continue;
        }

        /* numbers (123 or 1.5) */
        if (isdigit((unsigned char)c)) {
            while (isdigit((unsigned char)src[i])) { i++; col++; }
            if (src[i] == '.' && isdigit((unsigned char)src[i + 1])) {
                i++; col++;
                while (isdigit((unsigned char)src[i])) { i++; col++; }
            }
            token_list_push(&tl, TOK_NUMBER, src + start, i - start, line, scol);
            continue;
        }

        /* strings: the token value is the raw text between the quotes
           (escape sequences are kept as-is and handed straight to C) */
        if (c == '"') {
            int sl = line;
            i++; col++;
            start = i;
            while (src[i] && src[i] != '"') {
                if (src[i] == '\\' && src[i + 1]) { i += 2; col += 2; }
                else {
                    if (src[i] == '\n') { line++; col = 1; } else col++;
                    i++;
                }
            }
            if (!src[i]) lex_error(sl, scol, "unterminated string");
            token_list_push(&tl, TOK_STRING, src + start, i - start, sl, scol);
            i++; col++;
            continue;
        }

        /* two-character operators */
        int matched = 0;
        for (int k = 0; TWO_CHAR[k]; k++) {
            if (src[i] == TWO_CHAR[k][0] && src[i + 1] == TWO_CHAR[k][1]) {
                token_list_push(&tl, TOK_OPERATOR, src + i, 2, line, scol);
                i += 2; col += 2; matched = 1;
                break;
            }
        }
        if (matched) continue;

        /* single-character operators */
        if (strchr("+-*/%=<>!&|", c)) {
            token_list_push(&tl, TOK_OPERATOR, src + i, 1, line, scol);
            i++; col++;
            continue;
        }

        /* punctuation */
        if (strchr(";,(){}[]:.", c)) {
            token_list_push(&tl, TOK_PUNCTUATION, src + i, 1, line, scol);
            i++; col++;
            continue;
        }

        char msg[64];
        snprintf(msg, sizeof msg, "unexpected character '%c'", c);
        lex_error(line, col, msg);
    }

    token_list_push(&tl, TOK_EOF, "", 0, line, col);
    return tl;
}

#endif /* WOS_LEXER_C */
