/* ============================================================================
 * codegen.c  --  Code generator for WOS (Way Of Success)
 * This file is #included by wospras.c after the AST is defined.
 * ========================================================================== */
#ifndef WOS_CODEGEN_C
#define WOS_CODEGEN_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------------------
 * Globals
 * -------------------------------------------------------------------------- */
static int g_has_gui = 0;
static int g_has_list = 0;

static char *g_list_types[64];
static int   g_list_type_count = 0;

/* Track list variables and their element types */
typedef struct {
    char *name;
    char *elem_type;
} ListVar;

static ListVar g_list_vars[256];
static int g_list_var_count = 0;

static void register_list_var(const char *name, const char *elem) {
    if (!name) return;
    if (g_list_var_count < 256) {
        g_list_vars[g_list_var_count].name = wos_strdup(name);
        g_list_vars[g_list_var_count].elem_type = wos_strdup(elem ? elem : "int");
        g_list_var_count++;
    }
}

static const char *lookup_list_elem(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_list_var_count; i++)
        if (strcmp(g_list_vars[i].name, name) == 0)
            return g_list_vars[i].elem_type;
    return NULL;
}

static void cg_register_list_type(const char *t) {
    if (!t) return;
    for (int i = 0; i < g_list_type_count; i++)
        if (strcmp(g_list_types[i], t) == 0) return;
    if (g_list_type_count < 64)
        g_list_types[g_list_type_count++] = wos_strdup(t);
}

/* ----------------------------------------------------------------------------
 * Utility
 * -------------------------------------------------------------------------- */
static void indent(FILE *f, int n) {
    for (int i = 0; i < n; i++) fputc(' ', f), fputc(' ', f), fputc(' ', f), fputc(' ', f);
}

static const char *c_type(const char *wos) {
    if (!wos || !*wos) return "int";
    if (strcmp(wos, "int")    == 0) return "int";
    if (strcmp(wos, "float")  == 0) return "float";
    if (strcmp(wos, "double") == 0) return "double";
    if (strcmp(wos, "bool")   == 0) return "int";
    if (strcmp(wos, "string") == 0) return "char*";
    if (strcmp(wos, "void")   == 0) return "void";
    if (strcmp(wos, "list")   == 0) return "WosList_int*";
    return wos;
}

static int is_list_type(const char *t) { return t && strcmp(t, "list") == 0; }

static const char *list_elem_c_type(const char *elem) {
    if (!elem || !*elem) return "int";
    if (strcmp(elem, "string") == 0) return "char*";
    return c_type(elem);
}

static void list_struct_name(const char *elem, char *buf, size_t n) {
    const char *e = (elem && *elem) ? elem : "int";
    if (strcmp(e, "string") == 0) e = "string";
    snprintf(buf, n, "WosList_%s", e);
}

/* ----------------------------------------------------------------------------
 * Forward decls
 * -------------------------------------------------------------------------- */
static void gen_stmt(FILE *f, ASTNode *n, int ind);
static void gen_expr(FILE *f, ASTNode *n);

/* ----------------------------------------------------------------------------
 * Expressions
 * -------------------------------------------------------------------------- */
static void gen_expr(FILE *f, ASTNode *n) {
    if (!n) { fprintf(f, "0"); return; }

    switch (n->type) {
        case AST_NUMBER:
        case AST_BOOL:
            fprintf(f, "%s", n->value);
            return;
        case AST_STRING:
            fprintf(f, "\"%s\"", n->value);
            return;
        case AST_IDENTIFIER:
            fprintf(f, "%s", n->value);
            return;
        case AST_DEREF:
            fprintf(f, "*(int*)");
            gen_expr(f, n->children[0]);
            return;
        case AST_ADDRESS_OF:
            fprintf(f, "&");
            gen_expr(f, n->children[0]);
            return;
        case AST_MEMBER_ACCESS:
            gen_expr(f, n->children[0]);
            fprintf(f, "%s", n->flag ? "->" : ".");
            fprintf(f, "%s", n->value);
            return;
        case AST_LIST_ACCESS:
            gen_expr(f, n->children[0]);
            fprintf(f, "->data[");
            gen_expr(f, n->children[1]);
            fprintf(f, "]");
            return;
        case AST_LIST_INIT: {
            const char *elem = n->element_type ? n->element_type : "int";
            cg_register_list_type(elem);
            fprintf(f, "wos_list_%s_from(%d", elem, n->child_count);
            for (int i = 0; i < n->child_count; i++) {
                fprintf(f, ", ");
                gen_expr(f, n->children[i]);
            }
            fprintf(f, ")");
            return;
        }
        case AST_STRUCT_INIT: {
            fprintf(f, "(%s){", n->value);
            for (int i = 0; i < n->child_count; i++) {
                gen_expr(f, n->children[i]);
                if (i + 1 < n->child_count) fprintf(f, ", ");
            }
            fprintf(f, "}");
            return;
        }
        case AST_ALLOC:
            fprintf(f, "malloc(");
            if (n->child_count > 0) gen_expr(f, n->children[0]);
            else fprintf(f, "0");
            fprintf(f, ")");
            return;
        case AST_FREE:
            fprintf(f, "free(");
            if (n->child_count > 0) gen_expr(f, n->children[0]);
            fprintf(f, ")");
            return;
        case AST_FUNCTION_CALL:
            fprintf(f, "%s(", n->value);
            for (int i = 0; i < n->child_count; i++) {
                gen_expr(f, n->children[i]);
                if (i + 1 < n->child_count) fprintf(f, ", ");
            }
            fprintf(f, ")");
            return;
        case AST_EXPRESSION:
            if (n->child_count == 1) {
                fprintf(f, "%s", n->value);
                gen_expr(f, n->children[0]);
            } else if (n->child_count == 2) {
                fprintf(f, "(");
                gen_expr(f, n->children[0]);
                fprintf(f, " %s ", n->value);
                gen_expr(f, n->children[1]);
                fprintf(f, ")");
            } else {
                fprintf(f, "0");
            }
            return;
        default:
            fprintf(f, "0");
            return;
    }
}

/* ----------------------------------------------------------------------------
 * Print statement
 * -------------------------------------------------------------------------- */
static void gen_print(FILE *f, ASTNode *n, int ind) {
    if (n->child_count == 0) { indent(f, ind); fprintf(f, "printf(\"\\n\");\n"); return; }
    ASTNode *a = n->children[0];
    indent(f, ind);

    /* String literal */
    if (a->type == AST_STRING) {
        fprintf(f, "printf(\"%%s\\n\", \"%s\");\n", a->value);
        return;
    }

    /* List access - check the element type */
    if (a->type == AST_LIST_ACCESS) {
        const char *var = a->children[0]->value;
        const char *elem = lookup_list_elem(var);
        if (elem && strcmp(elem, "string") == 0) {
            fprintf(f, "printf(\"%%s\\n\", ");
            gen_expr(f, a);
            fprintf(f, ");\n");
            return;
        }
        /* fall through to %d for other types */
    }

    /* String variable - check if it's a known string variable */
    /* (We don't have a symbol table here, so skip this) */

    /* Default: print as integer */
    fprintf(f, "printf(\"%%d\\n\", ");
    gen_expr(f, a);
    fprintf(f, ");\n");
}

/* ----------------------------------------------------------------------------
 * Statements
 * -------------------------------------------------------------------------- */
static void gen_block(FILE *f, ASTNode *n, int ind) {
    for (int i = 0; i < n->child_count; i++)
        gen_stmt(f, n->children[i], ind);
}

static void gen_decl(FILE *f, ASTNode *n, int ind) {
    indent(f, ind);

    if (is_list_type(n->type_name)) {
        const char *elem = n->element_type ? n->element_type : "int";
        char sname[128];
        list_struct_name(elem, sname, sizeof sname);
        cg_register_list_type(elem);
        register_list_var(n->value, elem);
        fprintf(f, "%s* %s", sname, n->value);
        if (n->child_count > 0) {
    fprintf(f, " = ");
    if (n->children[0]->type == AST_LIST_INIT)
        n->children[0]->element_type = n->element_type;
    gen_expr(f, n->children[0]);
} else {
    fprintf(f, " = wos_list_%s_new(4)", elem);
}
        fprintf(f, ";\n");
        return;
    }

    fprintf(f, "%s %s", c_type(n->type_name), n->value);
    if (n->child_count > 0) {
        fprintf(f, " = ");
        gen_expr(f, n->children[0]);
    }
    fprintf(f, ";\n");
}

static void gen_assign(FILE *f, ASTNode *n, int ind) {
    indent(f, ind);
    if (n->type == AST_MEMBER_ASSIGN) {
        gen_expr(f, n->children[0]->children[0]);
        fprintf(f, "%s", n->children[0]->flag ? "->" : ".");
        fprintf(f, "%s = ", n->children[0]->value);
        gen_expr(f, n->children[1]);
    } else if (n->type == AST_LIST_ASSIGN) {
        gen_expr(f, n->children[0]->children[0]);
        fprintf(f, "->data[");
        gen_expr(f, n->children[0]->children[1]);
        fprintf(f, "] = ");
        gen_expr(f, n->children[1]);
    } else if (n->type == AST_DEREF_ASSIGN) {
        fprintf(f, "*(int*)");
        gen_expr(f, n->children[0]->children[0]);
        fprintf(f, " = ");
        gen_expr(f, n->children[1]);
    } else {
        gen_expr(f, n->children[0]);
        fprintf(f, " = ");
        gen_expr(f, n->children[1]);
    }
    fprintf(f, ";\n");
}

static void gen_function(FILE *f, ASTNode *n) {
    fprintf(f, "int %s(", n->value);
    ASTNode *params = n->children[0];
    if (params->child_count == 0) {
        fprintf(f, "void");
    } else {
        for (int i = 0; i < params->child_count; i++) {
            ASTNode *p = params->children[i];
            if (is_list_type(p->type_name)) {
                const char *elem = p->element_type ? p->element_type : "int";
                char sname[128];
                list_struct_name(elem, sname, sizeof sname);
                fprintf(f, "%s* %s", sname, p->value);
            } else {
                fprintf(f, "%s %s", c_type(p->type_name), p->value);
            }
            if (i + 1 < params->child_count) fprintf(f, ", ");
        }
    }
    fprintf(f, ") {\n");
    gen_block(f, n->children[1], 1);
    fprintf(f, "}\n\n");
}

static void gen_struct(FILE *f, ASTNode *n) {
    fprintf(f, "typedef struct {\n");
    for (int i = 0; i < n->child_count; i++) {
        ASTNode *field = n->children[i];
        fprintf(f, "    %s %s;\n", c_type(field->type_name), field->value);
    }
    fprintf(f, "} %s;\n\n", n->value);
}

static void gen_if(FILE *f, ASTNode *n, int ind) {
    indent(f, ind);
    fprintf(f, "if (");
    gen_expr(f, n->children[0]);
    fprintf(f, ") {\n");
    gen_block(f, n->children[1], ind + 1);
    indent(f, ind);
    fprintf(f, "}");
    if (n->child_count > 2) {
        fprintf(f, " else ");
        if (n->children[2]->type == AST_IF) {
            fprintf(f, "\n");
            gen_if(f, n->children[2], ind);
            return;
        }
        fprintf(f, "{\n");
        gen_block(f, n->children[2], ind + 1);
        indent(f, ind);
        fprintf(f, "}");
    }
    fprintf(f, "\n");
}

static void gen_while(FILE *f, ASTNode *n, int ind) {
    indent(f, ind);
    fprintf(f, "while (");
    gen_expr(f, n->children[0]);
    fprintf(f, ") {\n");
    gen_block(f, n->children[1], ind + 1);
    indent(f, ind);
    fprintf(f, "}\n");
}

static void gen_for(FILE *f, ASTNode *n, int ind) {
    indent(f, ind);
    fprintf(f, "for (");
    if (n->children[0]) {
        if (n->children[0]->type == AST_DECLARATION) {
            ASTNode *d = n->children[0];
            fprintf(f, "%s %s", c_type(d->type_name), d->value);
            if (d->child_count > 0) {
                fprintf(f, " = ");
                gen_expr(f, d->children[0]);
            }
        } else {
            gen_expr(f, n->children[0]);
        }
    }
    fprintf(f, "; ");
    if (n->children[1]) gen_expr(f, n->children[1]);
    fprintf(f, "; ");
    if (n->children[2]) gen_expr(f, n->children[2]);
    fprintf(f, ") {\n");
    gen_block(f, n->children[3], ind + 1);
    indent(f, ind);
    fprintf(f, "}\n");
}

static void gen_return(FILE *f, ASTNode *n, int ind) {
    indent(f, ind);
    fprintf(f, "return");
    if (n->child_count > 0) {
        fprintf(f, " ");
        gen_expr(f, n->children[0]);
    } else {
        fprintf(f, " 0");
    }
    fprintf(f, ";\n");
}

/* ---- GUI ---------------------------------------------------------------- */
static void gen_gui_window(FILE *f, ASTNode *n) {
    int w = 800, h = 600;
    if (n->child_count >= 2) {
        w = atoi(n->children[0]->value);
        h = atoi(n->children[1]->value);
    }
    fprintf(f, "    InitWindow(%d, %d, \"%s\");\n", w, h, n->value);
    fprintf(f, "    SetTargetFPS(60);\n");
    fprintf(f, "    int screenWidth = %d;\n", w);
    fprintf(f, "    int screenHeight = %d;\n", h);
}

static void gen_gui_button(FILE *f, ASTNode *n) {
    int w = 100, h = 40;
    if (n->child_count >= 2) {
        w = atoi(n->children[0]->value);
        h = atoi(n->children[1]->value);
    }
    const char *pos = n->element_type ? n->element_type : "center";
    fprintf(f, "        GuiButton((Rectangle){ ");
    if (strcmp(pos, "center") == 0)      fprintf(f, "screenWidth/2 - %d/2, screenHeight/2 - %d/2, ", w, h);
    else if (strcmp(pos, "left") == 0)   fprintf(f, "20, screenHeight/2 - %d/2, ", h);
    else if (strcmp(pos, "right") == 0)  fprintf(f, "screenWidth - %d - 20, screenHeight/2 - %d/2, ", w, h);
    else if (strcmp(pos, "top") == 0)    fprintf(f, "screenWidth/2 - %d/2, 20, ", w);
    else if (strcmp(pos, "bottom") == 0) fprintf(f, "screenWidth/2 - %d/2, screenHeight - %d - 20, ", w, h);
    else                                  fprintf(f, "screenWidth/2 - %d/2, screenHeight/2 - %d/2, ", w, h);
    fprintf(f, "%d, %d }, \"%s\");\n", w, h, n->value);
}

static void gen_gui_text(FILE *f, ASTNode *n) {
    const char *pos = n->element_type ? n->element_type : "center";
    fprintf(f, "        DrawText(\"%s\", ", n->value);
    if (strcmp(pos, "center") == 0)      fprintf(f, "screenWidth/2 - 50, screenHeight/2, 20, BLACK);\n");
    else if (strcmp(pos, "top") == 0)    fprintf(f, "screenWidth/2 - 50, 20, 20, BLACK);\n");
    else if (strcmp(pos, "bottom") == 0) fprintf(f, "screenWidth/2 - 50, screenHeight - 40, 20, BLACK);\n");
    else                                  fprintf(f, "20, 20, 20, BLACK);\n");
}

static void gen_gui_input(FILE *f, ASTNode *n) {
    const char *pos = n->element_type ? n->element_type : "center";
    fprintf(f, "        GuiTextBox((Rectangle){ ");
    if (strcmp(pos, "center") == 0)      fprintf(f, "screenWidth/2 - 100, screenHeight/2 - 15, ");
    else if (strcmp(pos, "top") == 0)    fprintf(f, "screenWidth/2 - 100, 20, ");
    else if (strcmp(pos, "bottom") == 0) fprintf(f, "screenWidth/2 - 100, screenHeight - 60, ");
    else                                  fprintf(f, "20, 20, ");
    fprintf(f, "200, 30 }, \"%s\", false);\n", n->value);
}

/* ----------------------------------------------------------------------------
 * Dispatcher
 * -------------------------------------------------------------------------- */
static void gen_stmt(FILE *f, ASTNode *n, int ind) {
    if (!n) return;

    switch (n->type) {
        case AST_DECLARATION:   gen_decl(f, n, ind); return;
        case AST_ASSIGNMENT:
        case AST_MEMBER_ASSIGN:
        case AST_LIST_ASSIGN:
        case AST_DEREF_ASSIGN:  gen_assign(f, n, ind); return;
        case AST_IF:            gen_if(f, n, ind); return;
        case AST_WHILE:         gen_while(f, n, ind); return;
        case AST_FOR:           gen_for(f, n, ind); return;
        case AST_RETURN:        gen_return(f, n, ind); return;
        case AST_BLOCK:         gen_block(f, n, ind); return;
        case AST_BREAK:         indent(f, ind); fprintf(f, "break;\n"); return;
        case AST_CONTINUE:      indent(f, ind); fprintf(f, "continue;\n"); return;
        case AST_FUNCTION:      gen_function(f, n); return;
        case AST_STRUCT_DEF:    gen_struct(f, n); return;
        case AST_LIBRARY:
        case AST_IMPORT:
            return;
        case AST_GUI_WINDOW:
        case AST_GUI_BUTTON:
        case AST_GUI_TEXT:
        case AST_GUI_INPUT:
            return;
        case AST_FUNCTION_CALL:
            if (strcmp(n->value, "print") == 0) { gen_print(f, n, ind); return; }
            indent(f, ind);
            gen_expr(f, n);
            fprintf(f, ";\n");
            return;
        default:
            indent(f, ind);
            gen_expr(f, n);
            fprintf(f, ";\n");
            return;
    }
}

/* ----------------------------------------------------------------------------
 * Detection
 * -------------------------------------------------------------------------- */
static int uses_gui(ASTNode *n) {
    if (!n) return 0;
    if (n->type == AST_GUI_WINDOW || n->type == AST_GUI_BUTTON ||
        n->type == AST_GUI_TEXT   || n->type == AST_GUI_INPUT) return 1;
    for (int i = 0; i < n->child_count; i++)
        if (uses_gui(n->children[i])) return 1;
    return 0;
}

static int uses_list(ASTNode *n) {
    if (!n) return 0;
    if (n->type == AST_LIST_INIT || n->type == AST_LIST_ACCESS ||
        n->type == AST_LIST_ASSIGN) return 1;
    if (n->type == AST_DECLARATION && n->type_name &&
        strcmp(n->type_name, "list") == 0) return 1;
    for (int i = 0; i < n->child_count; i++)
        if (uses_list(n->children[i])) return 1;
    return 0;
}

static const char *cg_find_library(ASTNode *n) {
    if (!n) return NULL;
    if (n->type == AST_LIBRARY) return n->value;
    for (int i = 0; i < n->child_count; i++) {
        const char *r = cg_find_library(n->children[i]);
        if (r) return r;
    }
    return NULL;
}

/* ----------------------------------------------------------------------------
 * List runtime
 * -------------------------------------------------------------------------- */
static void emit_list_runtime(FILE *f) {
    for (int t = 0; t < g_list_type_count; t++) {
        const char *elem = g_list_types[t];
        const char *ctype = list_elem_c_type(elem);
        char sname[128];
        list_struct_name(elem, sname, sizeof sname);

        fprintf(f, "typedef struct {\n");
        fprintf(f, "    %s *data;\n", ctype);
        fprintf(f, "    int size;\n");
        fprintf(f, "    int capacity;\n");
        fprintf(f, "} %s;\n\n", sname);

        fprintf(f, "static %s* wos_list_%s_new(int capacity) {\n", sname, elem);
        fprintf(f, "    %s *l = (%s*)malloc(sizeof(%s));\n", sname, sname, sname);
        fprintf(f, "    l->data = (%s*)malloc(sizeof(%s) * capacity);\n", ctype, ctype);
        fprintf(f, "    l->size = 0;\n");
        fprintf(f, "    l->capacity = capacity;\n");
        fprintf(f, "    return l;\n");
        fprintf(f, "}\n\n");

        fprintf(f, "static %s* wos_list_%s_from(int count, ...) {\n", sname, elem);
        fprintf(f, "    %s *l = wos_list_%s_new(count > 0 ? count : 4);\n", sname, elem);
        fprintf(f, "    va_list args;\n");
        fprintf(f, "    va_start(args, count);\n");
        fprintf(f, "    for (int i = 0; i < count; i++) {\n");
        fprintf(f, "        l->data[i] = va_arg(args, %s);\n", ctype);
        fprintf(f, "    }\n");
        fprintf(f, "    va_end(args);\n");
        fprintf(f, "    l->size = count;\n");
        fprintf(f, "    return l;\n");
        fprintf(f, "}\n\n");
    }
}

/* ----------------------------------------------------------------------------
 * Header (library mode)
 * -------------------------------------------------------------------------- */
static void emit_header(ASTNode *prog, FILE *h, const char *libname) {
    char guard[128];
    snprintf(guard, sizeof guard, "%s_H", libname);
    for (int i = 0; guard[i]; i++) guard[i] = (char)toupper((unsigned char)guard[i]);

    fprintf(h, "#ifndef %s\n", guard);
    fprintf(h, "#define %s\n\n", guard);

    for (int i = 0; i < prog->child_count; i++) {
        ASTNode *s = prog->children[i];
        if (s->type != AST_STRUCT_DEF) continue;
        fprintf(h, "typedef struct {\n");
        for (int j = 0; j < s->child_count; j++) {
            ASTNode *f = s->children[j];
            fprintf(h, "    %s %s;\n", c_type(f->type_name), f->value);
        }
        fprintf(h, "} %s;\n\n", s->value);
    }

    for (int i = 0; i < prog->child_count; i++) {
        ASTNode *fn = prog->children[i];
        if (fn->type != AST_FUNCTION) continue;
        fprintf(h, "int %s(", fn->value);
        ASTNode *params = fn->children[0];
        if (params->child_count == 0) fprintf(h, "void");
        else {
            for (int j = 0; j < params->child_count; j++) {
                ASTNode *p = params->children[j];
                fprintf(h, "%s %s", c_type(p->type_name), p->value);
                if (j + 1 < params->child_count) fprintf(h, ", ");
            }
        }
        fprintf(h, ");\n");
    }

    fprintf(h, "\n#endif\n");
}

/* ----------------------------------------------------------------------------
 * Main generator
 * -------------------------------------------------------------------------- */
static void generate_c(ASTNode *prog, FILE *out, FILE *hdr) {
    const char *lib = cg_find_library(prog);
    g_has_gui = uses_gui(prog);
    g_has_list = uses_list(prog);

    /* Pre-register all list types */
    for (int i = 0; i < prog->child_count; i++) {
        ASTNode *n = prog->children[i];
        if (n->type == AST_DECLARATION && is_list_type(n->type_name))
            cg_register_list_type(n->element_type ? n->element_type : "int");
        if (n->type == AST_FUNCTION) {
            ASTNode *params = n->children[0];
            for (int j = 0; j < params->child_count; j++) {
                ASTNode *p = params->children[j];
                if (is_list_type(p->type_name))
                    cg_register_list_type(p->element_type ? p->element_type : "int");
            }
        }
    }

    /* Headers */
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <stdlib.h>\n");
    fprintf(out, "#include <string.h>\n");
    fprintf(out, "#include <stdarg.h>\n");
    if (g_has_gui) {
        fprintf(out, "#include \"raygui.h\"\n");
        fprintf(out, "#include \"raylib.h\"\n");
    }

    for (int i = 0; i < prog->child_count; i++) {
        ASTNode *n = prog->children[i];
        if (n->type == AST_IMPORT)
            fprintf(out, "#include \"%s\"\n", n->value);
    }
    fprintf(out, "\n");

    if (g_has_list) emit_list_runtime(out);

    for (int i = 0; i < prog->child_count; i++) {
        ASTNode *n = prog->children[i];
        if (n->type == AST_STRUCT_DEF) gen_struct(out, n);
    }

    for (int i = 0; i < prog->child_count; i++) {
        ASTNode *n = prog->children[i];
        if (n->type == AST_FUNCTION) gen_function(out, n);
    }

    if (lib) {
        if (hdr) emit_header(prog, hdr, lib);
        return;
    }

    if (g_has_gui) {
        int found_window = 0;
        for (int i = 0; i < prog->child_count; i++) {
            if (prog->children[i]->type == AST_GUI_WINDOW) { found_window = 1; break; }
        }
        fprintf(out, "int main() {\n");
        if (!found_window) {
            fprintf(out, "    InitWindow(800, 600, \"WOS App\");\n");
            fprintf(out, "    SetTargetFPS(60);\n");
            fprintf(out, "    int screenWidth = 800;\n");
            fprintf(out, "    int screenHeight = 600;\n");
        } else {
            for (int i = 0; i < prog->child_count; i++) {
                ASTNode *n = prog->children[i];
                if (n->type == AST_GUI_WINDOW) { gen_gui_window(out, n); break; }
            }
        }

        for (int i = 0; i < prog->child_count; i++) {
            ASTNode *n = prog->children[i];
            if (n->type == AST_DECLARATION) gen_stmt(out, n, 1);
        }
        fprintf(out, "\n    while (!WindowShouldClose()) {\n");
        fprintf(out, "        BeginDrawing();\n");
        fprintf(out, "        ClearBackground(RAYWHITE);\n\n");

        for (int i = 0; i < prog->child_count; i++) {
            ASTNode *n = prog->children[i];
            if (n->type == AST_GUI_BUTTON) gen_gui_button(out, n);
            else if (n->type == AST_GUI_TEXT)   gen_gui_text(out, n);
            else if (n->type == AST_GUI_INPUT)  gen_gui_input(out, n);
            else if (n->type == AST_GUI_WINDOW) continue;
            else if (n->type == AST_DECLARATION) continue;
            else if (n->type == AST_FUNCTION) continue;
            else if (n->type == AST_STRUCT_DEF) continue;
            else if (n->type == AST_IMPORT) continue;
            else if (n->type == AST_LIBRARY) continue;
            else gen_stmt(out, n, 2);
        }

        fprintf(out, "\n        EndDrawing();\n");
        fprintf(out, "    }\n");
        fprintf(out, "    CloseWindow();\n");
        fprintf(out, "    return 0;\n");
        fprintf(out, "}\n");
    } else {
        fprintf(out, "int main(void) {\n");
        for (int i = 0; i < prog->child_count; i++) {
            ASTNode *n = prog->children[i];
            if (n->type == AST_FUNCTION) continue;
            if (n->type == AST_STRUCT_DEF) continue;
            if (n->type == AST_IMPORT) continue;
            if (n->type == AST_LIBRARY) continue;
            gen_stmt(out, n, 1);
        }
        fprintf(out, "    return 0;\n");
        fprintf(out, "}\n");
    }
}

#endif /* WOS_CODEGEN_C */
