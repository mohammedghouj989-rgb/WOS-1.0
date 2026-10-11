// ============================================================
// WOS PARSER + CODE GENERATOR
// Written by: CCODE
// ============================================================

#include "WOS.c"

// ============================================================
// AST NODE TYPES
// ============================================================
typedef enum {
    AST_PROGRAM,
    AST_LIBRARY,
    AST_IMPORT,
    AST_DECLARATION,
    AST_ASSIGNMENT,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_FUNCTION,
    AST_FUNCTION_CALL,
    AST_RETURN,
    AST_BLOCK,
    AST_EXPRESSION,
    AST_STRING,
    AST_GUI_BUTTON,
    AST_GUI_TEXT,
    AST_GUI_INPUT
} ASTNodeType;

// ============================================================
// AST NODE STRUCTURE
// ============================================================
typedef struct ASTNode {
    ASTNodeType type;
    char *value;
    int line;
    int column;
    struct ASTNode **children;
    int child_count;
    int child_capacity;
} ASTNode;

// ============================================================
// CREATE AST NODE
// ============================================================
ASTNode *ast_create(ASTNodeType type, char *value, int line, int column) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = type;
    node->value = value ? strdup(value) : NULL;
    node->line = line;
    node->column = column;
    node->child_count = 0;
    node->child_capacity = 4;
    node->children = malloc(sizeof(ASTNode*) * node->child_capacity);
    return node;
}

// ============================================================
// ADD CHILD
// ============================================================
void ast_add_child(ASTNode *parent, ASTNode *child) {
    if (!parent || !child) return;
    if (parent->child_count >= parent->child_capacity) {
        parent->child_capacity *= 2;
        parent->children = realloc(parent->children,
            sizeof(ASTNode*) * parent->child_capacity);
    }
    parent->children[parent->child_count++] = child;
}

// ============================================================
// FREE AST
// ============================================================
void ast_free(ASTNode *node) {
    if (!node) return;
    for (int i = 0; i < node->child_count; i++) {
        ast_free(node->children[i]);
    }
    free(node->children);
    free(node->value);
    free(node);
}

// ============================================================
// PARSER STRUCTURE
// ============================================================
typedef struct {
    Token *tokens;
    int token_count;
    int position;
    int has_error;
} Parser;

// ============================================================
// PARSER HELPERS
// ============================================================
Token parser_peek(Parser *parser) {
    return parser->tokens[parser->position];
}

Token parser_peek_next(Parser *parser) {
    if (parser->position + 1 < parser->token_count)
        return parser->tokens[parser->position + 1];
    return parser->tokens[parser->position];
}

Token parser_advance(Parser *parser) {
    Token t = parser->tokens[parser->position];
    if (parser->position < parser->token_count - 1) parser->position++;
    return t;
}

int parser_check(Parser *parser, TokenType type) {
    return parser_peek(parser).type == type;
}

int parser_check_keyword(Parser *parser, char *kw) {
    Token t = parser_peek(parser);
    return t.type == TOKEN_KEYWORD && t.value && strcmp(t.value, kw) == 0;
}

int parser_check_punct(Parser *parser, char c) {
    Token t = parser_peek(parser);
    return t.type == TOKEN_PUNCTUATION && t.value && t.value[0] == c;
}

Token parser_expect(Parser *parser, TokenType type, char *error) {
    if (parser_check(parser, type)) return parser_advance(parser);
    Token t = parser_peek(parser);
    printf("Parse Error at line %d, col %d: %s\n", t.line, t.column, error);
    parser->has_error = 1;
    Token err;
    err.type = TOKEN_EOF;
    err.value = NULL;
    err.line = t.line;
    err.column = t.column;
    return err;
}

Token parser_expect_punct(Parser *parser, char c, char *error) {
    if (parser_check_punct(parser, c)) return parser_advance(parser);
    Token t = parser_peek(parser);
    printf("Parse Error at line %d, col %d: %s\n", t.line, t.column, error);
    parser->has_error = 1;
    Token err;
    err.type = TOKEN_EOF;
    err.value = NULL;
    err.line = t.line;
    err.column = t.column;
    return err;
}

// ============================================================
// EXPRESSIONS
// ============================================================
ASTNode *parse_comparison(Parser *parser);
ASTNode *parse_expression(Parser *parser);
ASTNode *parse_term(Parser *parser);
ASTNode *parse_factor(Parser *parser);
ASTNode *parse_block(Parser *parser);
ASTNode *parse_statement(Parser *parser);

ASTNode *parse_factor(Parser *parser) {
    Token t = parser_peek(parser);

    if (t.type == TOKEN_NUMBER) {
        parser_advance(parser);
        return ast_create(AST_EXPRESSION, t.value, t.line, t.column);
    }

    if (t.type == TOKEN_IDENTIFIER) {
        parser_advance(parser);

        // Function call: identifier(args)
        if (parser_check_punct(parser, '(')) {
            parser_advance(parser);  // skip '('

            ASTNode *node = ast_create(AST_FUNCTION_CALL, t.value,
                                       t.line, t.column);

            while (!parser_check_punct(parser, ')') &&
                   !parser_check(parser, TOKEN_EOF)) {
                ASTNode *arg = parse_comparison(parser);
                if (arg) ast_add_child(node, arg);
                if (parser_check_punct(parser, ',')) parser_advance(parser);
                else break;
            }

            parser_expect_punct(parser, ')', "expected ')'");
            return node;
        }

        return ast_create(AST_EXPRESSION, t.value, t.line, t.column);
    }

    if (t.type == TOKEN_STRING) {
        parser_advance(parser);
        return ast_create(AST_STRING, t.value, t.line, t.column);
    }

    if (parser_check_punct(parser, '(')) {
        parser_advance(parser);
        ASTNode *expr = parse_comparison(parser);
        parser_expect_punct(parser, ')', "expected ')'");
        return expr;
    }

    printf("Parse Error at line %d, col %d: unexpected token in expression\n",
           t.line, t.column);
    parser->has_error = 1;
    return NULL;
}

ASTNode *parse_term(Parser *parser) {
    ASTNode *left = parse_factor(parser);
    if (!left) return NULL;

    while (parser_check(parser, TOKEN_OPERATOR)) {
        Token op = parser_peek(parser);
        if (!op.value) break;
        if (op.value[0] == '*' || op.value[0] == '/' || op.value[0] == '%') {
            parser_advance(parser);
            ASTNode *right = parse_factor(parser);
            if (!right) return left;
            ASTNode *node = ast_create(AST_EXPRESSION, op.value, op.line, op.column);
            ast_add_child(node, left);
            ast_add_child(node, right);
            left = node;
        } else break;
    }
    return left;
}

ASTNode *parse_expression(Parser *parser) {
    ASTNode *left = parse_term(parser);
    if (!left) return NULL;

    while (parser_check(parser, TOKEN_OPERATOR)) {
        Token op = parser_peek(parser);
        if (!op.value) break;
        if (op.value[0] == '+' || op.value[0] == '-') {
            parser_advance(parser);
            ASTNode *right = parse_term(parser);
            if (!right) return left;
            ASTNode *node = ast_create(AST_EXPRESSION, op.value, op.line, op.column);
            ast_add_child(node, left);
            ast_add_child(node, right);
            left = node;
        } else break;
    }
    return left;
}

ASTNode *parse_comparison(Parser *parser) {
    ASTNode *left = parse_expression(parser);
    if (!left) return NULL;

    while (parser_check(parser, TOKEN_OPERATOR)) {
        Token op = parser_peek(parser);
        if (!op.value) break;
        if (strcmp(op.value, ">")  == 0 || strcmp(op.value, "<")  == 0 ||
            strcmp(op.value, ">=") == 0 || strcmp(op.value, "<=") == 0 ||
            strcmp(op.value, "==") == 0 || strcmp(op.value, "!=") == 0) {
            parser_advance(parser);
            ASTNode *right = parse_expression(parser);
            if (!right) return left;
            ASTNode *node = ast_create(AST_EXPRESSION, op.value, op.line, op.column);
            ast_add_child(node, left);
            ast_add_child(node, right);
            left = node;
        } else break;
    }
    return left;
}

// ============================================================
// PARSE LIBRARY
// ============================================================
ASTNode *parse_library(Parser *parser) {
    Token lib_tok = parser_advance(parser);
    Token name = parser_expect(parser, TOKEN_STRING, "expected library name");

    ASTNode *node = ast_create(AST_LIBRARY, name.value,
                               lib_tok.line, lib_tok.column);

    parser_expect_punct(parser, ';', "expected ';'");
    return node;
}

// ============================================================
// PARSE IMPORT — NEW!
// ============================================================
ASTNode *parse_import(Parser *parser) {
    Token import_tok = parser_advance(parser);
    Token name = parser_expect(parser, TOKEN_STRING, "expected module name");

    ASTNode *node = ast_create(AST_IMPORT, name.value,
                               import_tok.line, import_tok.column);

    parser_expect_punct(parser, ';', "expected ';'");
    return node;
}

// ============================================================
// STATEMENTS
// ============================================================
ASTNode *parse_declaration(Parser *parser) {
    Token type_token = parser_advance(parser);
    Token name = parser_expect(parser, TOKEN_IDENTIFIER, "expected name");

    ASTNode *node = ast_create(AST_DECLARATION, type_token.value,
                               type_token.line, type_token.column);
    ast_add_child(node, ast_create(AST_EXPRESSION, name.value, name.line, name.column));

    if (parser_check(parser, TOKEN_OPERATOR)) {
        Token op = parser_peek(parser);
        if (op.value && strcmp(op.value, "=") == 0) {
            parser_advance(parser);
            ASTNode *value = parse_comparison(parser);
            if (value) ast_add_child(node, value);
        }
    }

    parser_expect_punct(parser, ';', "expected ';'");
    return node;
}

ASTNode *parse_assignment(Parser *parser) {
    Token name = parser_advance(parser);
    parser_advance(parser);  // '='

    ASTNode *node = ast_create(AST_ASSIGNMENT, name.value,
                               name.line, name.column);
    ASTNode *value = parse_comparison(parser);
    if (value) ast_add_child(node, value);

    parser_expect_punct(parser, ';', "expected ';'");
    return node;
}

ASTNode *parse_print(Parser *parser) {
    Token print_tok = parser_advance(parser);
    parser_expect_punct(parser, '(', "expected '(' after print");

    ASTNode *node = ast_create(AST_EXPRESSION, "print",
                               print_tok.line, print_tok.column);
    ASTNode *arg = parse_comparison(parser);
    if (arg) ast_add_child(node, arg);

    parser_expect_punct(parser, ')', "expected ')'");
    parser_expect_punct(parser, ';', "expected ';'");
    return node;
}

ASTNode *parse_if(Parser *parser) {
    Token if_tok = parser_advance(parser);
    ASTNode *condition = parse_comparison(parser);
    ASTNode *then_block = parse_block(parser);

    ASTNode *node = ast_create(AST_IF, NULL, if_tok.line, if_tok.column);
    ast_add_child(node, condition);
    ast_add_child(node, then_block);

    if (parser_check_keyword(parser, "else")) {
        parser_advance(parser);
        ASTNode *else_block = parse_block(parser);
        ast_add_child(node, else_block);
    }
    return node;
}

ASTNode *parse_while(Parser *parser) {
    Token while_tok = parser_advance(parser);
    ASTNode *condition = parse_comparison(parser);
    ASTNode *block = parse_block(parser);

    ASTNode *node = ast_create(AST_WHILE, NULL, while_tok.line, while_tok.column);
    ast_add_child(node, condition);
    ast_add_child(node, block);
    return node;
}

ASTNode *parse_for(Parser *parser) {
    Token for_tok = parser_advance(parser);
    parser_expect_punct(parser, '(', "expected '(' after for");

    ASTNode *init = parse_declaration(parser);
    ASTNode *condition = parse_comparison(parser);
    parser_expect_punct(parser, ';', "expected ';'");
    ASTNode *increment = parse_expression(parser);
    parser_expect_punct(parser, ')', "expected ')'");
    ASTNode *block = parse_block(parser);

    ASTNode *node = ast_create(AST_FOR, NULL, for_tok.line, for_tok.column);
    ast_add_child(node, init);
    ast_add_child(node, condition);
    ast_add_child(node, increment);
    ast_add_child(node, block);
    return node;
}

ASTNode *parse_function(Parser *parser) {
    Token fn_tok = parser_advance(parser);
    Token name = parser_expect(parser, TOKEN_IDENTIFIER, "expected name");
    parser_expect_punct(parser, '(', "expected '('");

    ASTNode *params = ast_create(AST_EXPRESSION, "params",
                                 fn_tok.line, fn_tok.column);
    while (!parser_check_punct(parser, ')') && !parser_check(parser, TOKEN_EOF)) {
        Token ptype = parser_expect(parser, TOKEN_KEYWORD, "expected type");
        Token pname = parser_expect(parser, TOKEN_IDENTIFIER, "expected param name");
        ASTNode *p = ast_create(AST_EXPRESSION, pname.value, ptype.line, ptype.column);
        ast_add_child(p, ast_create(AST_EXPRESSION, ptype.value, ptype.line, ptype.column));
        ast_add_child(params, p);
        if (parser_check_punct(parser, ',')) parser_advance(parser);
        else break;
    }
    parser_expect_punct(parser, ')', "expected ')'");
    ASTNode *block = parse_block(parser);

    ASTNode *node = ast_create(AST_FUNCTION, name.value, fn_tok.line, fn_tok.column);
    ast_add_child(node, params);
    ast_add_child(node, block);
    return node;
}

ASTNode *parse_return(Parser *parser) {
    Token ret_tok = parser_advance(parser);
    ASTNode *node = ast_create(AST_RETURN, NULL, ret_tok.line, ret_tok.column);
    if (!parser_check_punct(parser, ';')) {
        ASTNode *value = parse_comparison(parser);
        if (value) ast_add_child(node, value);
    }
    parser_expect_punct(parser, ';', "expected ';'");
    return node;
}

ASTNode *parse_block(Parser *parser) {
    Token open = parser_expect_punct(parser, '{', "expected '{'");
    ASTNode *block = ast_create(AST_BLOCK, NULL, open.line, open.column);

    while (1) {
        Token t = parser_peek(parser);
        if (t.type == TOKEN_EOF) break;
        if (t.type == TOKEN_PUNCTUATION && t.value && t.value[0] == '}') break;
        ASTNode *stmt = parse_statement(parser);
        if (stmt) ast_add_child(block, stmt);
        else break;
    }

    parser_expect_punct(parser, '}', "expected '}'");
    return block;
}

ASTNode *parse_gui(Parser *parser) {
    Token type = parser_advance(parser);

    if (strcmp(type.value, "button") == 0) {
        Token shape = parser_expect(parser, TOKEN_KEYWORD, "expected shape");
        Token text = parser_expect(parser, TOKEN_STRING, "expected text");
        Token position = parser_expect(parser, TOKEN_KEYWORD, "expected position");

        ASTNode *node = ast_create(AST_GUI_BUTTON, text.value, type.line, type.column);
        ast_add_child(node, ast_create(AST_EXPRESSION, shape.value, shape.line, shape.column));
        ast_add_child(node, ast_create(AST_EXPRESSION, position.value, position.line, position.column));

        if (parser_check(parser, TOKEN_NUMBER)) {
            Token w = parser_advance(parser);
            parser_expect(parser, TOKEN_IDENTIFIER, "expected 'x'");
            Token h = parser_expect(parser, TOKEN_NUMBER, "expected height");
            ast_add_child(node, ast_create(AST_EXPRESSION, w.value, w.line, w.column));
            ast_add_child(node, ast_create(AST_EXPRESSION, h.value, h.line, h.column));
        }
        return node;
    }

    if (strcmp(type.value, "text") == 0) {
        Token text = parser_expect(parser, TOKEN_STRING, "expected text");
        Token position = parser_expect(parser, TOKEN_KEYWORD, "expected position");
        ASTNode *node = ast_create(AST_GUI_TEXT, text.value, type.line, type.column);
        ast_add_child(node, ast_create(AST_EXPRESSION, position.value, position.line, position.column));
        return node;
    }

    if (strcmp(type.value, "input") == 0) {
        Token text = parser_expect(parser, TOKEN_STRING, "expected text");
        Token position = parser_expect(parser, TOKEN_KEYWORD, "expected position");
        ASTNode *node = ast_create(AST_GUI_INPUT, text.value, type.line, type.column);
        ast_add_child(node, ast_create(AST_EXPRESSION, position.value, position.line, position.column));
        return node;
    }

    printf("Parse Error at line %d: unknown GUI keyword '%s'\n", type.line, type.value);
    parser->has_error = 1;
    return NULL;
}

ASTNode *parse_statement(Parser *parser) {
    Token t = parser_peek(parser);

    if (t.type == TOKEN_KEYWORD && t.value) {
        if (strcmp(t.value, "library") == 0) return parse_library(parser);
        if (strcmp(t.value, "import")  == 0) return parse_import(parser);

        if (strcmp(t.value, "int")    == 0 ||
            strcmp(t.value, "float")  == 0 ||
            strcmp(t.value, "double") == 0 ||
            strcmp(t.value, "bool")   == 0 ||
            strcmp(t.value, "string") == 0 ||
            strcmp(t.value, "void")   == 0) {
            return parse_declaration(parser);
        }

        if (strcmp(t.value, "print")    == 0) return parse_print(parser);
        if (strcmp(t.value, "if")       == 0) return parse_if(parser);
        if (strcmp(t.value, "while")    == 0) return parse_while(parser);
        if (strcmp(t.value, "for")      == 0) return parse_for(parser);
        if (strcmp(t.value, "function") == 0) return parse_function(parser);
        if (strcmp(t.value, "return")   == 0) return parse_return(parser);

        if (strcmp(t.value, "button")   == 0 ||
            strcmp(t.value, "text")     == 0 ||
            strcmp(t.value, "input")    == 0) return parse_gui(parser);
    }

    if (parser_check_punct(parser, '{')) return parse_block(parser);

    if (t.type == TOKEN_IDENTIFIER) {
        Token next = parser_peek_next(parser);
        if (next.type == TOKEN_OPERATOR && next.value && strcmp(next.value, "=") == 0) {
            return parse_assignment(parser);
        }
    }

    ASTNode *expr = parse_comparison(parser);
    if (expr) parser_expect_punct(parser, ';', "expected ';'");
    return expr;
}

ASTNode *parse_program(Parser *parser) {
    ASTNode *program = ast_create(AST_PROGRAM, NULL, 1, 1);

    while (!parser_check(parser, TOKEN_EOF)) {
        Token t = parser_peek(parser);
        if (t.type == TOKEN_KEYWORD && (!t.value || t.value[0] == '\0')) {
            parser_advance(parser);
            continue;
        }
        int before = parser->position;
        ASTNode *stmt = parse_statement(parser);
        if (stmt) ast_add_child(program, stmt);
        if (parser->position == before) break;
    }
    return program;
}

// ============================================================
// CODE GENERATOR
// ============================================================
#include "codegen.c"

// ============================================================
// MAIN
// ============================================================
int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <file.wos>\n", argv[0]);
        return 1;
    }

    char *source = read_file(argv[1]);
    if (!source) return 1;

    Lexer *lexer = lexer_create(source);
    lexer_tokenize_all(lexer);

    if (lexer->has_error) {
        printf("Lexer errors found.\n");
        lexer_free(lexer);
        free(source);
        return 1;
    }

    Parser parser;
    parser.tokens = lexer->tokens;
    parser.token_count = lexer->token_count;
    parser.position = 0;
    parser.has_error = 0;

    ASTNode *program = parse_program(&parser);

    if (parser.has_error) {
        printf("Errors found during parsing.\n");
        ast_free(program);
        lexer_free(lexer);
        free(source);
        return 1;
    }

    cg_generate(program, "output.c");

    int ret = system("clang output.c -o output 2>/dev/null");
    if (ret == 0) {
        system("./output");
    } else {
        printf("Note: Could not compile output.c\n");
    }

    ast_free(program);
    lexer_free(lexer);
    free(source);
    return 0;
}
