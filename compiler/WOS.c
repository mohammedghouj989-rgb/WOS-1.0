// ============================================================
// WOS LEXER
// Written by: CCODE
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

typedef enum {
    TOKEN_KEYWORD,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_OPERATOR,
    TOKEN_PUNCTUATION,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    char *value;
    int line;
    int column;
} Token;

typedef struct {
    char *source;
    int position;
    int line;
    int column;
    Token *tokens;
    int token_count;
    int token_capacity;
    int has_error;
} Lexer;

Lexer *lexer_create(char *source) {
    Lexer *lexer = malloc(sizeof(Lexer));
    lexer->source = source;
    lexer->position = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->token_count = 0;
    lexer->token_capacity = 16;
    lexer->tokens = malloc(sizeof(Token) * lexer->token_capacity);
    lexer->has_error = 0;
    return lexer;
}

void lexer_free(Lexer *lexer) {
    for (int i = 0; i < lexer->token_count; i++) {
        if (lexer->tokens[i].value) free(lexer->tokens[i].value);
    }
    free(lexer->tokens);
    free(lexer);
}

void lexer_add_token(Lexer *lexer, Token token) {
    if (lexer->token_count >= lexer->token_capacity) {
        lexer->token_capacity *= 2;
        lexer->tokens = realloc(lexer->tokens,
            sizeof(Token) * lexer->token_capacity);
    }
    lexer->tokens[lexer->token_count++] = token;
}

char lexer_peek(Lexer *lexer) {
    return lexer->source[lexer->position];
}

char lexer_advance(Lexer *lexer) {
    char c = lexer->source[lexer->position];
    lexer->position++;
    lexer->column++;
    return c;
}

void lexer_skip_whitespace(Lexer *lexer) {
    while (isspace(lexer_peek(lexer))) {
        if (lexer_peek(lexer) == '\n') {
            lexer->line++;
            lexer->column = 1;
        }
        lexer_advance(lexer);
    }
}

int is_keyword(char *word) {
    char *keyword[] = {
        // WOS Core Keywords
        "int", "float", "double", "bool", "string", "void",
        "if", "else", "while", "for", "switch", "case",
        "break", "continue", "return", "function",
        "struct", "enum", "typedef",
        "import", "const",
        "alloc", "free",
        "print",
        "library",                       // ← NEW

        // WosGUI Keywords
        "window", "button", "text", "input", "label",
        "rectangle", "circle", "triangle",
        "center", "left", "right", "top", "bottom",

        NULL
    };
    for (int i = 0; keyword[i] != NULL; i++) {
        if (strcmp(word, keyword[i]) == 0) return 1;
    }
    return 0;
}

Token lexer_read_identifier(Lexer *lexer) {
    int start = lexer->position;
    while (isalnum(lexer_peek(lexer)) || lexer_peek(lexer) == '_') {
        lexer_advance(lexer);
    }
    int length = lexer->position - start;
    char *value = malloc(length + 1);
    strncpy(value, lexer->source + start, length);
    value[length] = '\0';

    Token token;
    token.value = value;
    token.line = lexer->line;
    token.column = lexer->column - length;
    token.type = is_keyword(value) ? TOKEN_KEYWORD : TOKEN_IDENTIFIER;
    return token;
}

Token lexer_read_number(Lexer *lexer) {
    int start = lexer->position;
    while (isdigit(lexer_peek(lexer)) || lexer_peek(lexer) == '.') {
        lexer_advance(lexer);
    }
    int length = lexer->position - start;
    char *value = malloc(length + 1);
    strncpy(value, lexer->source + start, length);
    value[length] = '\0';

    Token token;
    token.type = TOKEN_NUMBER;
    token.value = value;
    token.line = lexer->line;
    token.column = lexer->column - length;
    return token;
}

Token lexer_read_string(Lexer *lexer) {
    lexer_advance(lexer);
    int start = lexer->position;
    while (lexer_peek(lexer) != '"' && lexer_peek(lexer) != '\0') {
        lexer_advance(lexer);
    }
    if (lexer_peek(lexer) == '\0') {
        printf("Error: unclosed string at line %d\n", lexer->line);
        lexer->has_error = 1;
        Token err;
        err.type = TOKEN_EOF;
        err.value = NULL;
        err.line = lexer->line;
        err.column = lexer->column;
        return err;
    }
    int length = lexer->position - start;
    char *value = malloc(length + 1);
    strncpy(value, lexer->source + start, length);
    value[length] = '\0';
    lexer_advance(lexer);

    Token token;
    token.type = TOKEN_STRING;
    token.value = value;
    token.line = lexer->line;
    token.column = lexer->column - length - 2;
    return token;
}

Token lexer_read_operator(Lexer *lexer) {
    char c = lexer_advance(lexer);
    char n = lexer_peek(lexer);
    char *value = malloc(3);
    value[0] = c;
    value[1] = '\0';
    value[2] = '\0';

    if ((c == '=' && n == '=') || (c == '!' && n == '=') ||
        (c == '>' && n == '=') || (c == '<' && n == '=') ||
        (c == '&' && n == '&') || (c == '|' && n == '|')) {
        value[1] = lexer_advance(lexer);
        value[2] = '\0';
    }

    Token token;
    token.type = TOKEN_OPERATOR;
    token.value = value;
    token.line = lexer->line;
    token.column = lexer->column - strlen(value);
    return token;
}

Token lexer_next_token(Lexer *lexer) {
    lexer_skip_whitespace(lexer);

    Token token;
    token.line = lexer->line;
    token.column = lexer->column;
    token.value = NULL;

    char c = lexer_peek(lexer);

    if (c == '\0') {
        token.type = TOKEN_EOF;
        token.value = NULL;
        return token;
    }

    if (isalpha(c) || c == '_') return lexer_read_identifier(lexer);
    if (isdigit(c))            return lexer_read_number(lexer);
    if (c == '"')              return lexer_read_string(lexer);
    if (strchr("+-*/%=!<>&|", c)) return lexer_read_operator(lexer);

    if (strchr(";,(){}[]", c)) {
        lexer_advance(lexer);
        token.type = TOKEN_PUNCTUATION;
        token.value = malloc(2);
        token.value[0] = c;
        token.value[1] = '\0';
        return token;
    }

    printf("Error: unknown character '%c' at line %d, column %d\n",
           c, lexer->line, lexer->column);
    lexer->has_error = 1;
    lexer_advance(lexer);
    token.type = TOKEN_PUNCTUATION;
    token.value = malloc(2);
    token.value[0] = c;
    token.value[1] = '\0';
    return token;
}

void lexer_tokenize_all(Lexer *lexer) {
    while (1) {
        Token token = lexer_next_token(lexer);
        lexer_add_token(lexer, token);
        if (token.type == TOKEN_EOF) break;
    }
}

void lexer_print_tokens(Lexer *lexer) {
    char *names[] = {
        "KEYWORD", "IDENTIFIER", "NUMBER",
        "STRING", "OPERATOR", "PUNCTUATION", "EOF"
    };
    for (int i = 0; i < lexer->token_count; i++) {
        Token t = lexer->tokens[i];
        printf("[%s] '%s' (line %d, col %d)\n",
               names[t.type], t.value ? t.value : "",
               t.line, t.column);
    }
}

char *read_file(char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("Error: cannot open file %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buffer = malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);
    return buffer;
}
