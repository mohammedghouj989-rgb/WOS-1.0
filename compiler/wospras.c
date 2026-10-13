/* ============================================================================
 * wospras.c  --  Parser + Main driver for WOS (Way Of Success)
 *
 * Build:   clang wospras.c -o wos
 * Use:     ./wos program.wos            (compile to C, build, run)
 *          ./wos program.wos --emit     (only write output.c, don't build/run)
 *
 * This file #includes WOS.c (lexer) and codegen.c (code generator), so a
 * single clang invocation builds the whole compiler.
 * ========================================================================== */
#include "WOS.c"
#include <stdarg.h>

/* ----------------------------------------------------------------------------
 * AST
 * -------------------------------------------------------------------------- */
typedef enum {
    AST_PROGRAM, AST_LIBRARY, AST_IMPORT, AST_STRUCT_DEF, AST_DECLARATION,
    AST_ASSIGNMENT, AST_MEMBER_ASSIGN, AST_LIST_ASSIGN, AST_DEREF_ASSIGN,
    AST_IF, AST_WHILE, AST_FOR, AST_FUNCTION, AST_FUNCTION_CALL,
    AST_MEMBER_ACCESS, AST_LIST_ACCESS, AST_LIST_INIT, AST_STRUCT_INIT,
    AST_DEREF, AST_ADDRESS_OF, AST_ALLOC, AST_FREE, AST_RETURN, AST_BLOCK,
    AST_EXPRESSION, AST_STRING, AST_STRING_VAR, AST_GUI_BUTTON, AST_GUI_TEXT,
    AST_GUI_INPUT,
    /* extra node kinds needed by the implementation */
    AST_GUI_WINDOW, AST_NUMBER, AST_IDENTIFIER, AST_BOOL, AST_BREAK, AST_CONTINUE
} ASTType;

typedef struct ASTNode {
    ASTType type;
    char *value;          /* name / operator / literal text                  */
    char *type_name;      /* declared type ("int", "list", "Point", "void*") */
    char *element_type;   /* list<T>: the T ("int" by default)               */
    int flag;             /* member access: 1 if written with '->'           */
    int line, column;
    struct ASTNode **children;
    int child_count;
    int child_cap;
} ASTNode;

static ASTNode *node_new(ASTType type, const char *value, Token *at) {
    ASTNode *n = (ASTNode *)calloc(1, sizeof(ASTNode));
    n->type = type;
    n->value = wos_strdup(value ? value : "");
    if (at) { n->line = at->line; n->column = at->column; }
    return n;
}

static void node_add(ASTNode *parent, ASTNode *child) {
    if (parent->child_count >= parent->child_cap) {
        parent->child_cap = parent->child_cap ? parent->child_cap * 2 : 4;
        parent->children = (ASTNode **)realloc(parent->children,
                                               sizeof(ASTNode *) * parent->child_cap);
    }
    parent->children[parent->child_count++] = child;   /* child may be NULL (for-loop slots) */
}

/* ----------------------------------------------------------------------------
 * Parser state + helpers
 * -------------------------------------------------------------------------- */
static TokenList g_tokens;
static int g_pos = 0;
static int g_depth = 0;                 /* block nesting depth (0 = top level) */
static char *g_struct_names[256];
static int g_struct_count = 0;

static Token *cur(void) { return &g_tokens.items[g_pos]; }

static Token *peek_at(int n) {
    int i = g_pos + n;
    if (i >= g_tokens.count) i = g_tokens.count - 1;
    return &g_tokens.items[i];
}

static Token *advance(void) {
    Token *t = cur();
    if (t->type != TOK_EOF) g_pos++;
    return t;
}

static int is_tok(Token *t, const char *v) {
    return (t->type == TOK_KEYWORD || t->type == TOK_OPERATOR || t->type == TOK_PUNCTUATION)
           && strcmp(t->value, v) == 0;
}
static int check(const char *v) { return is_tok(cur(), v); }
static int match(const char *v) { if (check(v)) { advance(); return 1; } return 0; }

static void parse_error(const char *fmt, ...) {
    Token *t = cur();
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "WOS syntax error at line %d, column %d: ", t->line, t->column);
    vfprintf(stderr, fmt, ap);
    if (t->type == TOK_EOF) fprintf(stderr, " (at end of file)");
    else fprintf(stderr, " (near '%s')", t->value);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static Token *expect(const char *v) {
    if (!check(v)) parse_error("expected '%s'", v);
    return advance();
}

static Token *expect_ident(void) {
    if (cur()->type != TOK_IDENTIFIER) parse_error("expected a name");
    return advance();
}

static int is_struct_name(const char *s) {
    for (int i = 0; i < g_struct_count; i++)
        if (strcmp(g_struct_names[i], s) == 0) return 1;
    return 0;
}

static int is_type_keyword(Token *t) {
    static const char *names[] = { "int", "float", "double", "bool", "string", "void", "list", NULL };
    if (t->type != TOK_KEYWORD) return 0;
    for (int i = 0; names[i]; i++)
        if (strcmp(t->value, names[i]) == 0) return 1;
    return 0;
}

/* ----------------------------------------------------------------------------
 * Types:  int | float | ... | StructName | T* | list | list<T>
 * -------------------------------------------------------------------------- */
static void parse_type(char **tn, char **elem) {
    char buf[128];
    *elem = NULL;
    Token *t = cur();

    if (is_tok(t, "list")) {
        advance();
        *tn = "list";
        *elem = "int";                       /* default: list<int> */
        if (match("<")) {
            Token *e = cur();
            if (is_tok(e, "list")) parse_error("nested lists are not supported yet");
            if (!is_type_keyword(e) && e->type != TOK_IDENTIFIER)
                parse_error("expected an element type inside list<...>");
            advance();
            *elem = e->value;
            expect(">");
        }
        return;
    }

    if (!is_type_keyword(t) && t->type != TOK_IDENTIFIER) parse_error("expected a type");
    advance();
    snprintf(buf, sizeof buf, "%s", t->value);
    while (check("*")) {
        advance();
        strncat(buf, "*", sizeof(buf) - strlen(buf) - 1);
    }
    *tn = wos_strdup(buf);
}

/* Does the current position start a variable declaration? */
static int is_decl_start(void) {
    Token *t = cur();
    if (is_tok(t, "const")) return 1;
    if (is_type_keyword(t)) return 1;
    if (t->type == TOK_IDENTIFIER && is_struct_name(t->value)) {
        Token *n1 = peek_at(1);
        if (n1->type == TOK_IDENTIFIER) return 1;
        if (is_tok(n1, "*") && peek_at(2)->type == TOK_IDENTIFIER) return 1;
    }
    return 0;
}

/* ----------------------------------------------------------------------------
 * Expressions
 *   ||  <  &&  <  comparison  <  + -  <  * / %  <  unary (! - * &)  <  postfix
 * -------------------------------------------------------------------------- */
static ASTNode *parse_expression(void);

static void parse_call_args(ASTNode *call) {
    expect("(");
    if (!check(")")) {
        do { node_add(call, parse_expression()); } while (match(","));
    }
    expect(")");
}

static ASTNode *parse_primary(void) {
    Token *t = cur();

    if (t->type == TOK_NUMBER) { advance(); return node_new(AST_NUMBER, t->value, t); }
    if (t->type == TOK_STRING) { advance(); return node_new(AST_STRING, t->value, t); }
    if (is_tok(t, "true") || is_tok(t, "false")) { advance(); return node_new(AST_BOOL, t->value, t); }

    if (is_tok(t, "(")) {
        advance();
        ASTNode *e = parse_expression();
        expect(")");
        return e;
    }

    if (is_tok(t, "[")) {                               /* list literal */
        advance();
        ASTNode *l = node_new(AST_LIST_INIT, "list", t);
        if (!check("]")) {
            do { node_add(l, parse_expression()); } while (match(","));
        }
        expect("]");
        return l;
    }

    if (is_tok(t, "alloc")) {
        advance();
        ASTNode *a = node_new(AST_ALLOC, "alloc", t);
        expect("(");
        node_add(a, parse_expression());
        expect(")");
        return a;
    }
    if (is_tok(t, "free")) {
        advance();
        ASTNode *a = node_new(AST_FREE, "free", t);
        expect("(");
        node_add(a, parse_expression());
        expect(")");
        return a;
    }
    if (is_tok(t, "print")) {
        advance();
        ASTNode *c = node_new(AST_FUNCTION_CALL, "print", t);
        parse_call_args(c);
        return c;
    }

    if (t->type == TOK_IDENTIFIER) {
        advance();
        if (check("(")) {
            ASTNode *c = node_new(is_struct_name(t->value) ? AST_STRUCT_INIT : AST_FUNCTION_CALL,
                                  t->value, t);
            parse_call_args(c);
            return c;
        }
        return node_new(AST_IDENTIFIER, t->value, t);
    }

    parse_error("unexpected token in expression");
    return NULL;
}

static ASTNode *parse_postfix(void) {
    ASTNode *n = parse_primary();
    for (;;) {
        if (check("[")) {
            Token *t = advance();
            ASTNode *idx = parse_expression();
            expect("]");
            ASTNode *a = node_new(AST_LIST_ACCESS, "[]", t);
            node_add(a, n);
            node_add(a, idx);
            n = a;
        } else if (check(".") || check("->")) {
            Token *t = advance();
            Token *f = cur();
            if (f->type != TOK_IDENTIFIER && f->type != TOK_KEYWORD)
                parse_error("expected a field name after '%s'", t->value);
            advance();
            ASTNode *m = node_new(AST_MEMBER_ACCESS, f->value, t);
            m->flag = (strcmp(t->value, "->") == 0);
            node_add(m, n);
            n = m;
        } else break;
    }
    return n;
}

static ASTNode *parse_unary(void) {
    Token *t = cur();
    if (is_tok(t, "!") || is_tok(t, "-")) {
        advance();
        ASTNode *n = node_new(AST_EXPRESSION, t->value, t);
        node_add(n, parse_unary());
        return n;
    }
    if (is_tok(t, "*")) {
        advance();
        ASTNode *n = node_new(AST_DEREF, "*", t);
        node_add(n, parse_unary());
        return n;
    }
    if (is_tok(t, "&")) {
        advance();
        ASTNode *n = node_new(AST_ADDRESS_OF, "&", t);
        node_add(n, parse_unary());
        return n;
    }
    return parse_postfix();
}

static ASTNode *binary(const char *op, Token *t, ASTNode *l, ASTNode *r) {
    ASTNode *n = node_new(AST_EXPRESSION, op, t);
    node_add(n, l);
    node_add(n, r);
    return n;
}

static ASTNode *parse_term(void) {
    ASTNode *l = parse_unary();
    while (check("*") || check("/") || check("%")) {
        Token *op = advance();
        l = binary(op->value, op, l, parse_unary());
    }
    return l;
}

static ASTNode *parse_additive(void) {
    ASTNode *l = parse_term();
    while (check("+") || check("-")) {
        Token *op = advance();
        l = binary(op->value, op, l, parse_term());
    }
    return l;
}

static ASTNode *parse_comparison(void) {
    ASTNode *l = parse_additive();
    while (check(">") || check("<") || check(">=") || check("<=") || check("==") || check("!=")) {
        Token *op = advance();
        l = binary(op->value, op, l, parse_additive());
    }
    return l;
}

static ASTNode *parse_and(void) {
    ASTNode *l = parse_comparison();
    while (check("&&")) {
        Token *op = advance();
        l = binary(op->value, op, l, parse_comparison());
    }
    return l;
}

static ASTNode *parse_or(void) {
    ASTNode *l = parse_and();
    while (check("||")) {
        Token *op = advance();
        l = binary(op->value, op, l, parse_and());
    }
    return l;
}

static ASTNode *parse_expression(void) { return parse_or(); }

/* ----------------------------------------------------------------------------
 * Statements
 * -------------------------------------------------------------------------- */
static ASTNode *parse_statement(void);

static ASTNode *parse_block(void) {
    Token *lb = expect("{");
    g_depth++;
    ASTNode *b = node_new(AST_BLOCK, "block", lb);
    while (!check("}")) {
        if (cur()->type == TOK_EOF) parse_error("missing '}' to close the block");
        node_add(b, parse_statement());
    }
    g_depth--;
    expect("}");
    return b;
}

/* type name [= expr]   (no trailing ';') */
static ASTNode *parse_declaration(void) {
    match("const");
    char *tn, *elem;
    parse_type(&tn, &elem);
    Token *name = expect_ident();
    ASTNode *d = node_new(AST_DECLARATION, name->value, name);
    d->type_name = tn;
    d->element_type = elem;
    if (match("=")) node_add(d, parse_expression());
    return d;
}

/* assignment or bare expression   (no trailing ';') */
static ASTNode *parse_simple(void) {
    ASTNode *lhs = parse_expression();
    if (check("=")) {
        Token *eq = advance();
        ASTNode *rhs = parse_expression();
        ASTType at = AST_ASSIGNMENT;
        switch (lhs->type) {
            case AST_IDENTIFIER:    at = AST_ASSIGNMENT;    break;
            case AST_MEMBER_ACCESS: at = AST_MEMBER_ASSIGN; break;
            case AST_LIST_ACCESS:   at = AST_LIST_ASSIGN;   break;
            case AST_DEREF:         at = AST_DEREF_ASSIGN;  break;
            default: parse_error("you can't assign to that expression");
        }
        ASTNode *a = node_new(at, "=", eq);
        node_add(a, lhs);
        node_add(a, rhs);
        return a;
    }
    return lhs;
}

static ASTNode *parse_if(void) {
    Token *kw = advance();                       /* 'if' */
    ASTNode *n = node_new(AST_IF, "if", kw);
    node_add(n, parse_expression());
    node_add(n, parse_block());
    if (match("else")) {
        if (check("if")) node_add(n, parse_if());
        else node_add(n, parse_block());
    }
    return n;
}

static ASTNode *parse_while(void) {
    Token *kw = advance();
    ASTNode *n = node_new(AST_WHILE, "while", kw);
    node_add(n, parse_expression());
    node_add(n, parse_block());
    return n;
}

static ASTNode *parse_for(void) {
    Token *kw = advance();
    ASTNode *n = node_new(AST_FOR, "for", kw);
    int paren = match("(");
    ASTNode *init = NULL, *cond = NULL, *step = NULL;
    if (!check(";")) init = is_decl_start() ? parse_declaration() : parse_simple();
    expect(";");
    if (!check(";")) cond = parse_expression();
    expect(";");
    if (!check(")") && !check("{")) step = parse_simple();
    if (paren) expect(")");
    node_add(n, init);
    node_add(n, cond);
    node_add(n, step);
    node_add(n, parse_block());
    return n;
}

static ASTNode *parse_function(void) {
    if (g_depth != 0) parse_error("functions can only be defined at the top level");
    Token *kw = advance();
    Token *name = expect_ident();
    ASTNode *fn = node_new(AST_FUNCTION, name->value, name);
    ASTNode *params = node_new(AST_BLOCK, "params", kw);
    expect("(");
    if (!check(")")) {
        do {
            char *tn, *el;
            parse_type(&tn, &el);
            Token *pn = expect_ident();
            ASTNode *p = node_new(AST_DECLARATION, pn->value, pn);
            p->type_name = tn;
            p->element_type = el;
            node_add(params, p);
        } while (match(","));
    }
    expect(")");
    if (match("->")) parse_type(&fn->type_name, &fn->element_type);   /* optional explicit return type */
    node_add(fn, params);
    node_add(fn, parse_block());
    return fn;
}

static ASTNode *parse_struct(void) {
    if (g_depth != 0) parse_error("structs can only be defined at the top level");
    Token *kw = advance();
    Token *name = expect_ident();
    ASTNode *s = node_new(AST_STRUCT_DEF, name->value, kw);
    expect("{");
    while (!check("}")) {
        if (cur()->type == TOK_EOF) parse_error("missing '}' to close the struct");
        char *tn, *el;
        parse_type(&tn, &el);
        Token *fname = expect_ident();
        ASTNode *f = node_new(AST_DECLARATION, fname->value, fname);
        f->type_name = tn;
        f->element_type = el;
        node_add(s, f);
        expect(";");
    }
    expect("}");
    match(";");                                   /* optional trailing ';' */
    return s;
}

/* ---- WosGUI -------------------------------------------------------------- */
static int is_position(Token *t) {
    return is_tok(t, "center") || is_tok(t, "left") || is_tok(t, "right") ||
           is_tok(t, "top") || is_tok(t, "bottom");
}

static char *opt_position(void) {
    if (is_position(cur())) return advance()->value;
    return "center";
}

/* optional "W x H" */
static void opt_size(ASTNode *n) {
    if (cur()->type != TOK_NUMBER) return;
    Token *w = advance();
    Token *x = cur();
    if (x->type != TOK_IDENTIFIER || (strcmp(x->value, "x") != 0 && strcmp(x->value, "X") != 0))
        parse_error("expected 'x' between width and height (e.g. 100 x 40)");
    advance();
    if (cur()->type != TOK_NUMBER) parse_error("expected a height");
    Token *h = advance();
    node_add(n, node_new(AST_NUMBER, w->value, w));
    node_add(n, node_new(AST_NUMBER, h->value, h));
}

static ASTNode *parse_gui(void) {
    if (g_depth != 0) parse_error("GUI elements can only be used at the top level");
    Token *kw = advance();
    ASTNode *n;
    if (is_tok(kw, "window")) {
        if (cur()->type != TOK_STRING) parse_error("expected a window title string");
        Token *title = advance();
        n = node_new(AST_GUI_WINDOW, title->value, kw);
        opt_size(n);
    } else if (is_tok(kw, "button")) {
        char *shape = "rectangle";
        if (is_tok(cur(), "rectangle") || is_tok(cur(), "circle") || is_tok(cur(), "triangle"))
            shape = advance()->value;
        if (cur()->type != TOK_STRING) parse_error("expected the button label string");
        Token *label = advance();
        n = node_new(AST_GUI_BUTTON, label->value, kw);
        n->type_name = shape;
        n->element_type = opt_position();
        opt_size(n);
    } else if (is_tok(kw, "input")) {
        if (cur()->type != TOK_STRING) parse_error("expected the input placeholder string");
        Token *ph = advance();
        n = node_new(AST_GUI_INPUT, ph->value, kw);
        n->element_type = opt_position();
        opt_size(n);
    } else {                                      /* text / label */
        if (cur()->type != TOK_STRING) parse_error("expected a text string");
        Token *s = advance();
        n = node_new(AST_GUI_TEXT, s->value, kw);
        n->element_type = opt_position();
        opt_size(n);
    }
    expect(";");
    return n;
}

static ASTNode *parse_statement(void) {
    Token *t = cur();

    if (t->type == TOK_KEYWORD) {
        const char *k = t->value;

        if (!strcmp(k, "library")) {
            if (g_depth != 0) parse_error("'library' must be at the top level");
            advance();
            if (cur()->type != TOK_STRING) parse_error("expected a library name, e.g. library \"mymath\";");
            Token *s = advance();
            ASTNode *n = node_new(AST_LIBRARY, s->value, t);
            expect(";");
            return n;
        }
        if (!strcmp(k, "import")) {
            if (g_depth != 0) parse_error("'import' must be at the top level");
            advance();
            if (cur()->type != TOK_STRING) parse_error("expected a header name, e.g. import \"stdio.h\";");
            Token *s = advance();
            ASTNode *n = node_new(AST_IMPORT, s->value, t);
            expect(";");
            return n;
        }
        if (!strcmp(k, "struct"))   return parse_struct();
        if (!strcmp(k, "function")) return parse_function();
        if (!strcmp(k, "if"))       return parse_if();
        if (!strcmp(k, "while"))    return parse_while();
        if (!strcmp(k, "for"))      return parse_for();

        if (!strcmp(k, "return")) {
            advance();
            ASTNode *n = node_new(AST_RETURN, "return", t);
            if (!check(";")) node_add(n, parse_expression());
            expect(";");
            return n;
        }
        if (!strcmp(k, "break") || !strcmp(k, "continue")) {
            advance();
            ASTNode *n = node_new(!strcmp(k, "break") ? AST_BREAK : AST_CONTINUE, k, t);
            expect(";");
            return n;
        }
        if (!strcmp(k, "window") || !strcmp(k, "button") || !strcmp(k, "text") ||
            !strcmp(k, "input") || !strcmp(k, "label"))
            return parse_gui();

        if (!strcmp(k, "use") || !strcmp(k, "move") || !strcmp(k, "borrow") || !strcmp(k, "mut") ||
            !strcmp(k, "switch") || !strcmp(k, "case") || !strcmp(k, "enum") || !strcmp(k, "typedef"))
            parse_error("'%s' is reserved for a future version of WOS", k);
    }

    if (is_tok(t, "{")) return parse_block();

    if (is_decl_start()) {
        ASTNode *d = parse_declaration();
        expect(";");
        return d;
    }

    ASTNode *s = parse_simple();
    expect(";");
    return s;
}

static ASTNode *parse_program(void) {
    /* pre-scan: register struct names so Point(1,2) and "Point p;" parse correctly
       even if the struct is declared further down the file */
    for (int i = 0; i + 1 < g_tokens.count; i++) {
        if (is_tok(&g_tokens.items[i], "struct") && g_tokens.items[i + 1].type == TOK_IDENTIFIER) {
            if (g_struct_count < 256) g_struct_names[g_struct_count++] = g_tokens.items[i + 1].value;
        }
    }
    ASTNode *prog = node_new(AST_PROGRAM, "program", &g_tokens.items[0]);
    while (cur()->type != TOK_EOF) node_add(prog, parse_statement());
    return prog;
}

/* ----------------------------------------------------------------------------
 * Code generator
 * -------------------------------------------------------------------------- */
#include "codegen.c"

/* ----------------------------------------------------------------------------
 * Main driver
 * -------------------------------------------------------------------------- */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "WOS: cannot open '%s'\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(n + 1);
    size_t got = fread(buf, 1, n, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "WOS - Way Of Success\nusage: %s program.wos [--emit]\n", argv[0]);
        return 1;
    }
    int emit_only = (argc > 2 && strcmp(argv[2], "--emit") == 0);

    char *src = read_file(argv[1]);
    g_tokens = tokenize(src);
    ASTNode *prog = parse_program();

    const char *lib = cg_find_library(prog);
    FILE *out = fopen("output.c", "w");
    if (!out) { fprintf(stderr, "WOS: cannot write output.c\n"); return 1; }
    FILE *hdr = NULL;
    char hdr_path[160] = "";
    if (lib) {
        snprintf(hdr_path, sizeof hdr_path, "%s.h", lib);
        hdr = fopen(hdr_path, "w");
        if (!hdr) { fprintf(stderr, "WOS: cannot write %s\n", hdr_path); return 1; }
    }

    generate_c(prog, out, hdr);
    fclose(out);
    if (hdr) fclose(hdr);

    if (lib) fprintf(stderr, "WOS: wrote output.c and %s\n", hdr_path);
    else     fprintf(stderr, "WOS: wrote output.c\n");
    if (emit_only) return 0;

    /* pick a C compiler: $WOS_CC, else clang, else cc */
    const char *cc = getenv("WOS_CC");
    if (!cc || !*cc) cc = (system("command -v clang >/dev/null 2>&1") == 0) ? "clang" : "cc";

    char cmd[512];
    if (lib) {
        snprintf(cmd, sizeof cmd, "%s -std=c11 -w -c output.c -o %s.o", cc, lib);
        if (system(cmd) != 0) { fprintf(stderr, "WOS: C compilation failed\n"); return 1; }
        fprintf(stderr, "WOS: library built: %s.o + %s\n", lib, hdr_path);
        return 0;
    }

    const char *extra = "";
    if (g_has_gui) {
#ifdef __APPLE__
        extra = " -lraylib -framework IOKit -framework Cocoa -framework OpenGL";
#else
        extra = " -lraylib -lGL -lm -lpthread -ldl -lrt -lX11";
#endif
    }
    snprintf(cmd, sizeof cmd, "%s -std=c11 -w output.c -o output%s", cc, extra);
    if (system(cmd) != 0) { fprintf(stderr, "WOS: C compilation failed\n"); return 1; }
    return system("./output") == 0 ? 0 : 1;
}
