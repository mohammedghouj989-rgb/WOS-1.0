// ============================================================
// WOS CODE GENERATOR
// ============================================================
#include <stdarg.h>

typedef struct {
    FILE *out;
    int indent;
    int has_gui;
    int has_library;
    char *library_name;
} CodeGen;

void cg_indent(CodeGen *cg) {
    for (int i = 0; i < cg->indent; i++) fprintf(cg->out, "    ");
}

int cg_detect_gui(ASTNode *node) {
    if (!node) return 0;
    if (node->type == AST_GUI_BUTTON ||
        node->type == AST_GUI_TEXT   ||
        node->type == AST_GUI_INPUT) return 1;
    for (int i = 0; i < node->child_count; i++) {
        if (cg_detect_gui(node->children[i])) return 1;
    }
    return 0;
}

ASTNode *cg_find_library(ASTNode *node) {
    if (!node) return NULL;
    if (node->type == AST_LIBRARY) return node;
    for (int i = 0; i < node->child_count; i++) {
        ASTNode *found = cg_find_library(node->children[i]);
        if (found) return found;
    }
    return NULL;
}

const char *cg_c_type(const char *wos_type) {
    if (!wos_type) return "int";
    if (strcmp(wos_type, "int")    == 0) return "int";
    if (strcmp(wos_type, "float")  == 0) return "float";
    if (strcmp(wos_type, "double") == 0) return "double";
    if (strcmp(wos_type, "bool")   == 0) return "int";
    if (strcmp(wos_type, "string") == 0) return "char*";
    if (strcmp(wos_type, "void")   == 0) return "void";
    return "int";
}

void cg_node(CodeGen *cg, ASTNode *node);
void cg_expression(CodeGen *cg, ASTNode *node);

// ============================================================
// EXPRESSION
// ============================================================
void cg_expression(CodeGen *cg, ASTNode *node) {
    if (!node) return;

    if (node->type == AST_STRING) {
        if (node->value) fprintf(cg->out, "\"%s\"", node->value);
        return;
    }

    if (node->type == AST_FUNCTION_CALL) {
        fprintf(cg->out, "%s(", node->value);
        for (int i = 0; i < node->child_count; i++) {
            cg_expression(cg, node->children[i]);
            if (i < node->child_count - 1) fprintf(cg->out, ", ");
        }
        fprintf(cg->out, ")");
        return;
    }

    if (node->type != AST_EXPRESSION) return;

    if (node->child_count == 0) {
        if (node->value) fprintf(cg->out, "%s", node->value);
        return;
    }

    if (node->child_count == 2) {
        cg_expression(cg, node->children[0]);
        fprintf(cg->out, " %s ", node->value ? node->value : "?");
        cg_expression(cg, node->children[1]);
    }
}

// ============================================================
// PRINT
// ============================================================
void cg_print(CodeGen *cg, ASTNode *node) {
    if (!node->child_count) return;
    ASTNode *arg = node->children[0];

    cg_indent(cg);

    if (arg->type == AST_STRING) {
        fprintf(cg->out, "printf(\"%%s\\n\", \"%s\");\n", arg->value);
        return;
    }

    if (arg->type == AST_EXPRESSION && arg->child_count == 0 && arg->value) {
        int is_num = 1;
        for (int i = 0; arg->value[i]; i++) {
            if (!isdigit(arg->value[i]) && arg->value[i] != '.' && arg->value[i] != '-') {
                is_num = 0;
                break;
            }
        }
        if (is_num) {
            fprintf(cg->out, "printf(\"%%d\\n\", %s);\n", arg->value);
            return;
        }
    }

    fprintf(cg->out, "printf(\"%%d\\n\", ");
    cg_expression(cg, arg);
    fprintf(cg->out, ");\n");
}

// ============================================================
// DECLARATION
// ============================================================
void cg_declaration(CodeGen *cg, ASTNode *node) {
    const char *c_type = cg_c_type(node->value);
    const char *name = node->children[0]->value;

    cg_indent(cg);
    fprintf(cg->out, "%s %s", c_type, name);

    if (node->child_count > 1) {
        fprintf(cg->out, " = ");
        cg_expression(cg, node->children[1]);
    }
    fprintf(cg->out, ";\n");
}

// ============================================================
// ASSIGNMENT
// ============================================================
void cg_assignment(CodeGen *cg, ASTNode *node) {
    cg_indent(cg);
    fprintf(cg->out, "%s = ", node->value);
    cg_expression(cg, node->children[0]);
    fprintf(cg->out, ";\n");
}

// ============================================================
// IF
// ============================================================
void cg_if(CodeGen *cg, ASTNode *node) {
    cg_indent(cg);
    fprintf(cg->out, "if (");
    cg_expression(cg, node->children[0]);
    fprintf(cg->out, ") {\n");

    cg->indent++;
    cg_node(cg, node->children[1]);
    cg->indent--;

    cg_indent(cg);
    fprintf(cg->out, "}\n");

    if (node->child_count > 2) {
        cg_indent(cg);
        fprintf(cg->out, "else {\n");
        cg->indent++;
        cg_node(cg, node->children[2]);
        cg->indent--;
        cg_indent(cg);
        fprintf(cg->out, "}\n");
    }
}

// ============================================================
// WHILE
// ============================================================
void cg_while(CodeGen *cg, ASTNode *node) {
    cg_indent(cg);
    fprintf(cg->out, "while (");
    cg_expression(cg, node->children[0]);
    fprintf(cg->out, ") {\n");

    cg->indent++;
    cg_node(cg, node->children[1]);
    cg->indent--;

    cg_indent(cg);
    fprintf(cg->out, "}\n");
}

// ============================================================
// FOR
// ============================================================
void cg_for(CodeGen *cg, ASTNode *node) {
    cg_indent(cg);
    fprintf(cg->out, "for (");

    const char *c_type = cg_c_type(node->children[0]->value);
    const char *name = node->children[0]->children[0]->value;
    fprintf(cg->out, "%s %s = ", c_type, name);
    if (node->children[0]->child_count > 1) {
        cg_expression(cg, node->children[0]->children[1]);
    } else {
        fprintf(cg->out, "0");
    }

    fprintf(cg->out, "; ");
    cg_expression(cg, node->children[1]);
    fprintf(cg->out, "; ");
    cg_expression(cg, node->children[2]);
    fprintf(cg->out, ") {\n");

    cg->indent++;
    cg_node(cg, node->children[3]);
    cg->indent--;

    cg_indent(cg);
    fprintf(cg->out, "}\n");
}

// ============================================================
// FUNCTION
// ============================================================
void cg_function(CodeGen *cg, ASTNode *node) {
    cg_indent(cg);
    fprintf(cg->out, "int %s(", node->value);

    ASTNode *params = node->children[0];
    if (params->child_count == 0) {
        fprintf(cg->out, "void");
    } else {
        for (int i = 0; i < params->child_count; i++) {
            ASTNode *p = params->children[i];
            const char *pname = p->value;
            const char *ptype = p->children[0]->value;
            fprintf(cg->out, "%s %s", cg_c_type(ptype), pname);
            if (i < params->child_count - 1) fprintf(cg->out, ", ");
        }
    }

    fprintf(cg->out, ") {\n");
    cg->indent++;
    cg_node(cg, node->children[1]);
    cg->indent--;
    cg_indent(cg);
    fprintf(cg->out, "}\n\n");
}

// ============================================================
// RETURN
// ============================================================
void cg_return(CodeGen *cg, ASTNode *node) {
    cg_indent(cg);
    fprintf(cg->out, "return");
    if (node->child_count > 0) {
        fprintf(cg->out, " ");
        cg_expression(cg, node->children[0]);
    } else {
        fprintf(cg->out, " 0");
    }
    fprintf(cg->out, ";\n");
}

// ============================================================
// GUI BUTTON
// ============================================================
void cg_gui_button(CodeGen *cg, ASTNode *node) {
    const char *text = node->value;
    const char *pos = node->children[1]->value;

    cg_indent(cg);
    fprintf(cg->out, "GuiButton((Rectangle){ ");
    if (strcmp(pos, "center") == 0)      fprintf(cg->out, "screenWidth/2 - 50, screenHeight/2 - 20, ");
    else if (strcmp(pos, "left") == 0)   fprintf(cg->out, "20, screenHeight/2 - 20, ");
    else if (strcmp(pos, "right") == 0)  fprintf(cg->out, "screenWidth - 120, screenHeight/2 - 20, ");
    else if (strcmp(pos, "top") == 0)    fprintf(cg->out, "screenWidth/2 - 50, 20, ");
    else if (strcmp(pos, "bottom") == 0) fprintf(cg->out, "screenWidth/2 - 50, screenHeight - 60, ");
    else                                  fprintf(cg->out, "screenWidth/2 - 50, screenHeight/2 - 20, ");

    if (node->child_count >= 4) {
        fprintf(cg->out, "%s, %s ", node->children[2]->value, node->children[3]->value);
    } else {
        fprintf(cg->out, "100, 40 ");
    }
    fprintf(cg->out, "}, \"%s\");\n", text);
}

// ============================================================
// GUI TEXT
// ============================================================
void cg_gui_text(CodeGen *cg, ASTNode *node) {
    const char *text = node->value;
    const char *pos = node->children[0]->value;

    cg_indent(cg);
    fprintf(cg->out, "DrawText(\"%s\", ", text);
    if (strcmp(pos, "center") == 0)      fprintf(cg->out, "screenWidth/2 - 50, screenHeight/2, 20, BLACK);\n");
    else if (strcmp(pos, "top") == 0)    fprintf(cg->out, "screenWidth/2 - 50, 20, 20, BLACK);\n");
    else if (strcmp(pos, "bottom") == 0) fprintf(cg->out, "screenWidth/2 - 50, screenHeight - 40, 20, BLACK);\n");
    else                                  fprintf(cg->out, "20, 20, 20, BLACK);\n");
}

// ============================================================
// GUI INPUT
// ============================================================
void cg_gui_input(CodeGen *cg, ASTNode *node) {
    const char *text = node->value;
    const char *pos = node->children[0]->value;

    cg_indent(cg);
    fprintf(cg->out, "GuiTextBox((Rectangle){ ");
    if (strcmp(pos, "center") == 0)      fprintf(cg->out, "screenWidth/2 - 100, screenHeight/2 - 15, ");
    else if (strcmp(pos, "top") == 0)    fprintf(cg->out, "screenWidth/2 - 100, 20, ");
    else if (strcmp(pos, "bottom") == 0) fprintf(cg->out, "screenWidth/2 - 100, screenHeight - 60, ");
    else                                  fprintf(cg->out, "20, 20, ");
    fprintf(cg->out, "200, 30 }, \"%s\", false);\n", text);
}

// ============================================================
// IMPORT — NEW!
// ============================================================
void cg_import(CodeGen *cg, ASTNode *node) {
    cg_indent(cg);
    fprintf(cg->out, "#include <%s>\n", node->value);
}

// ============================================================
// DISPATCHER
// ============================================================
void cg_node(CodeGen *cg, ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
            for (int i = 0; i < node->child_count; i++)
                cg_node(cg, node->children[i]);
            break;

        case AST_LIBRARY:
            // handled in cg_generate
            break;

        case AST_IMPORT:
            // handled in cg_generate
            break;

        case AST_DECLARATION:  cg_declaration(cg, node); break;
        case AST_ASSIGNMENT:   cg_assignment(cg, node); break;
        case AST_FUNCTION:     cg_function(cg, node); break;
        case AST_FUNCTION_CALL:
            cg_indent(cg);
            cg_expression(cg, node);
            fprintf(cg->out, ";\n");
            break;
        case AST_IF:           cg_if(cg, node); break;
        case AST_WHILE:        cg_while(cg, node); break;
        case AST_FOR:          cg_for(cg, node); break;
        case AST_RETURN:       cg_return(cg, node); break;
        case AST_BLOCK:
            for (int i = 0; i < node->child_count; i++)
                cg_node(cg, node->children[i]);
            break;

        case AST_GUI_BUTTON:   cg_gui_button(cg, node); break;
        case AST_GUI_TEXT:     cg_gui_text(cg, node); break;
        case AST_GUI_INPUT:    cg_gui_input(cg, node); break;

        case AST_STRING:
            cg_indent(cg);
            fprintf(cg->out, "\"%s\";\n", node->value);
            break;

        case AST_EXPRESSION:
            if (node->value && strcmp(node->value, "print") == 0) {
                cg_print(cg, node);
            } else {
                cg_indent(cg);
                cg_expression(cg, node);
                fprintf(cg->out, ";\n");
            }
            break;

        default: break;
    }
}

// ============================================================
// GENERATE HEADER
// ============================================================
void cg_generate_header(ASTNode *program, const char *lib_name) {
    char header_path[256];
    snprintf(header_path, sizeof(header_path), "%s.h", lib_name);

    FILE *h = fopen(header_path, "w");
    if (!h) {
        printf("Error: cannot create header %s\n", header_path);
        return;
    }

    char guard[128];
    snprintf(guard, sizeof(guard), "%s_H", lib_name);
    for (int i = 0; guard[i]; i++) guard[i] = toupper(guard[i]);

    fprintf(h, "#ifndef %s\n", guard);
    fprintf(h, "#define %s\n\n", guard);

    for (int i = 0; i < program->child_count; i++) {
        ASTNode *stmt = program->children[i];
        if (stmt->type == AST_FUNCTION) {
            fprintf(h, "int %s(", stmt->value);

            ASTNode *params = stmt->children[0];
            if (params->child_count == 0) {
                fprintf(h, "void");
            } else {
                for (int j = 0; j < params->child_count; j++) {
                    ASTNode *p = params->children[j];
                    const char *pname = p->value;
                    const char *ptype = p->children[0]->value;
                    fprintf(h, "%s %s", cg_c_type(ptype), pname);
                    if (j < params->child_count - 1) fprintf(h, ", ");
                }
            }
            fprintf(h, ");\n");
        }
    }

    fprintf(h, "\n#endif\n");
    fclose(h);

    printf("Generated header: %s\n", header_path);
}

// ============================================================
// GENERATE
// ============================================================
void cg_generate(ASTNode *program, const char *output_path) {
    CodeGen cg;
    cg.out = fopen(output_path, "w");
    cg.indent = 0;
    cg.has_gui = cg_detect_gui(program);

    ASTNode *lib = cg_find_library(program);
    cg.has_library = (lib != NULL);
    cg.library_name = lib ? lib->value : NULL;

    if (!cg.out) {
        printf("Error: cannot open output file %s\n", output_path);
        return;
    }

    fprintf(cg.out, "#include <stdio.h>\n");
    fprintf(cg.out, "#include <stdlib.h>\n");

    if (cg.has_gui) {
        fprintf(cg.out, "#include \"raygui.h\"\n");
        fprintf(cg.out, "#include \"raylib.h\"\n");
    }

    // ========================================================
    // WRITE IMPORTS
    // ========================================================
    for (int i = 0; i < program->child_count; i++) {
        ASTNode *stmt = program->children[i];
        if (stmt->type == AST_IMPORT) {
            cg.indent = 0;
            cg_import(&cg, stmt);
        }
    }

    fprintf(cg.out, "\n");

    // ========================================================
    // LIBRARY MODE
    // ========================================================
    if (cg.has_library) {
        for (int i = 0; i < program->child_count; i++) {
            ASTNode *stmt = program->children[i];
            if (stmt->type == AST_FUNCTION) {
                cg.indent = 0;
                cg_node(&cg, stmt);
            }
        }

        fclose(cg.out);
        cg_generate_header(program, cg.library_name);
        printf("Generated library: %s (no main)\n", output_path);
        return;
    }

    // ========================================================
    // NORMAL MODE
    // ========================================================
    // Functions before main
    for (int i = 0; i < program->child_count; i++) {
        ASTNode *stmt = program->children[i];
        if (stmt->type == AST_FUNCTION) {
            cg.indent = 0;
            cg_node(&cg, stmt);
        }
    }

    // Main
    if (cg.has_gui) {
        fprintf(cg.out, "int main() {\n");
        fprintf(cg.out, "    InitWindow(800, 600, \"WOS App\");\n");
        fprintf(cg.out, "    SetTargetFPS(60);\n");
        fprintf(cg.out, "    int screenWidth = 800;\n");
        fprintf(cg.out, "    int screenHeight = 600;\n\n");

        for (int i = 0; i < program->child_count; i++) {
            ASTNode *stmt = program->children[i];
            if (stmt->type == AST_DECLARATION) {
                cg.indent = 1;
                cg_node(&cg, stmt);
            }
        }

        fprintf(cg.out, "\n    while (!WindowShouldClose()) {\n");
        fprintf(cg.out, "        BeginDrawing();\n");
        fprintf(cg.out, "        ClearBackground(RAYWHITE);\n\n");

        for (int i = 0; i < program->child_count; i++) {
            ASTNode *stmt = program->children[i];
            if (stmt->type != AST_DECLARATION &&
                stmt->type != AST_FUNCTION &&
                stmt->type != AST_IMPORT) {
                cg.indent = 2;
                cg_node(&cg, stmt);
            }
        }

        fprintf(cg.out, "\n        EndDrawing();\n");
        fprintf(cg.out, "    }\n");
        fprintf(cg.out, "    CloseWindow();\n");
        fprintf(cg.out, "    return 0;\n");
        fprintf(cg.out, "}\n");
    } else {
        fprintf(cg.out, "int main() {\n");
        cg.indent = 1;
        for (int i = 0; i < program->child_count; i++) {
            ASTNode *stmt = program->children[i];
            if (stmt->type == AST_FUNCTION) continue;
            if (stmt->type == AST_IMPORT) continue;
            if (stmt->type == AST_LIBRARY) continue;
            cg_node(&cg, stmt);
        }
        fprintf(cg.out, "    return 0;\n");
        fprintf(cg.out, "}\n");
    }

    fclose(cg.out);
    printf("Generated C code: %s\n", output_path);
}
