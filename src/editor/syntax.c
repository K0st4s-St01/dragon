#include "dragon_editor/syntax.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

void syntax_init(SyntaxHighlighting *sh, const char *language_id) {
    sh->tokens = NULL;
    sh->token_count = 0;
    sh->token_capacity = 0;
    
    if (language_id) {
        sh->language_id = malloc(strlen(language_id) + 1);
        strcpy(sh->language_id, language_id);
    } else {
        sh->language_id = NULL;
    }
}

void syntax_free(SyntaxHighlighting *sh) {
    free(sh->tokens);
    free(sh->language_id);
    sh->tokens = NULL;
    sh->token_count = 0;
    sh->token_capacity = 0;
    sh->language_id = NULL;
}

void syntax_clear(SyntaxHighlighting *sh) {
    sh->token_count = 0;
}

void syntax_add_token(SyntaxHighlighting *sh, int sr, int sc, int er, int ec, SyntaxType type) {
    if (sh->token_count >= sh->token_capacity) {
        sh->token_capacity = (sh->token_capacity == 0) ? 256 : sh->token_capacity * 2;
        sh->tokens = realloc(sh->tokens, sh->token_capacity * sizeof(SyntaxToken));
    }
    
    SyntaxToken *token = &sh->tokens[sh->token_count++];
    token->start_row = sr;
    token->start_col = sc;
    token->end_row = er;
    token->end_col = ec;
    token->type = type;
}

SyntaxType syntax_get_type_at(SyntaxHighlighting *sh, int row, int col) {
    /* Binary search for token at position */
    int left = 0, right = sh->token_count - 1;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        SyntaxToken *token = &sh->tokens[mid];
        
        /* Check if position is within token bounds */
        if (token->start_row == row && token->end_row == row) {
            /* Single line token */
            if (col >= token->start_col && col < token->end_col) {
                return token->type;
            }
        } else if (token->start_row == row && col >= token->start_col) {
            /* Start of multi-line token */
            return token->type;
        } else if (token->end_row == row && col < token->end_col) {
            /* End of multi-line token */
            return token->type;
        } else if (token->start_row < row && token->end_row > row) {
            /* Middle of multi-line token */
            return token->type;
        }
        
        /* Move search bounds */
        if (token->end_row < row || (token->end_row == row && token->end_col <= col)) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    return SYNTAX_NORMAL;
}

void syntax_update_from_lsp(SyntaxHighlighting *sh, const char *response) {
    if (!response || !sh) return;
    
    /* Clear existing tokens to prevent accumulation */
    syntax_clear(sh);
    
    /* Find the "data" array */
    const char *data_start = strstr(response, "\"data\"");
    if (!data_start) return;
    
    /* Find the opening bracket */
    const char *bracket = strchr(data_start, '[');
    if (!bracket) return;
    
    const char *p = bracket + 1;
    
    /* Parse delta-encoded tokens */
    int cur_line = 0;
    int cur_col = 0;
    int token_idx = 0;
    
    while (*p && *p != ']' && token_idx < 10000) {
        /* Skip whitespace and commas */
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == ',')) p++;
        if (*p == ']') break;
        
        /* Parse 5 integers for each token: deltaLine, deltaStartChar, length, tokenType, tokenModifiers */
        int delta_line = 0, delta_start = 0, length = 0, token_type = 0, modifiers = 0;
        
        /* Parse deltaLine */
        if (sscanf(p, "%d", &delta_line) != 1) break;
        while (*p && (*p == '-' || isdigit((unsigned char)*p))) p++;
        while (*p && (*p == ' ' || *p == ',' || *p == '\n')) p++;
        
        /* Parse deltaStartChar */
        if (sscanf(p, "%d", &delta_start) != 1) break;
        while (*p && (*p == '-' || isdigit((unsigned char)*p))) p++;
        while (*p && (*p == ' ' || *p == ',' || *p == '\n')) p++;
        
        /* Parse length */
        if (sscanf(p, "%d", &length) != 1) break;
        while (*p && (*p == '-' || isdigit((unsigned char)*p))) p++;
        while (*p && (*p == ' ' || *p == ',' || *p == '\n')) p++;
        
        /* Parse tokenType */
        if (sscanf(p, "%d", &token_type) != 1) break;
        while (*p && (*p == '-' || isdigit((unsigned char)*p))) p++;
        while (*p && (*p == ' ' || *p == ',' || *p == '\n')) p++;
        
        /* Parse modifiers */
        if (sscanf(p, "%d", &modifiers) != 1) break;
        while (*p && (*p == '-' || isdigit((unsigned char)*p))) p++;
        
        /* Decode deltas */
        if (delta_line != 0) {
            cur_line += delta_line;
            cur_col = delta_start;
        } else {
            cur_col += delta_start;
        }
        
         /* Map token type to syntax type */
         SyntaxType type = SYNTAX_NORMAL;
         switch (token_type) {
         case 0: type = SYNTAX_NAMESPACE; break;  /* namespace */
         case 1: type = SYNTAX_TYPE; break;       /* type */
         case 2: type = SYNTAX_TYPE; break;       /* class */
         case 3: type = SYNTAX_TYPE; break;       /* enum */
         case 4: type = SYNTAX_TYPE; break;       /* interface */
         case 5: type = SYNTAX_TYPE; break;       /* struct */
         case 6: type = SYNTAX_TYPE; break;       /* typeParameter */
         case 7: type = SYNTAX_VARIABLE; break;   /* parameter */
         case 8: type = SYNTAX_VARIABLE; break;   /* variable */
         case 9: type = SYNTAX_VARIABLE; break;   /* property */
         case 10: type = SYNTAX_VARIABLE; break;  /* enumMember */
         case 11: type = SYNTAX_FUNCTION; break;  /* event */
         case 12: type = SYNTAX_FUNCTION; break;  /* function */
         case 13: type = SYNTAX_FUNCTION; break;  /* method */
         case 14: type = SYNTAX_MACRO; break;     /* macro */
         case 15: type = SYNTAX_KEYWORD; break;   /* keyword */
         case 16: type = SYNTAX_COMMENT; break;   /* comment */
         case 17: type = SYNTAX_STRING; break;    /* string */
         case 18: type = SYNTAX_NUMBER; break;    /* number */
         case 19: type = SYNTAX_NORMAL; break;    /* regexp */
         case 20: type = SYNTAX_OPERATOR; break;  /* operator */
         default: type = SYNTAX_NORMAL;
         }
        
        /* Add token */
        if (length > 0) {
            syntax_add_token(sh, cur_line, cur_col, cur_line, cur_col + length, type);
        }
        
        token_idx++;
        
        /* Skip to next comma or bracket */
        while (*p && *p != ',' && *p != ']') p++;
    }
}

/* Basic syntax highlighting for when LSP is not available */
void syntax_highlight_basic(SyntaxHighlighting *sh, const char *text, size_t len) {
    if (!text || len == 0) return;
    
    syntax_clear(sh);
    
    /* C/Rust/Go keywords */
    const char *keywords[] = {
        "if", "else", "for", "while", "do", "switch", "case", "default",
        "break", "continue", "return", "goto", "struct", "union", "enum",
        "class", "interface", "trait", "impl", "fn", "func", "def",
        "var", "let", "const", "static", "extern", "typedef", "void",
        "int", "float", "double", "char", "bool", "long", "short",
        "unsigned", "signed", "auto", "register", "volatile", "const",
        "public", "private", "protected", "async", "await", "yield",
        "import", "include", "require", "module", "package", "namespace",
        "try", "catch", "finally", "throw", "throws", "new", "delete",
        "true", "false", "null", "None", "undefined", "self", "Self",
        NULL
    };
    
    int row = 0, col = 0;
    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        
        /* Newline */
        if (c == '\n') {
            row++;
            col = 0;
            continue;
        }
        
        /* Preprocessor directives (#include, #define, etc) */
        if (c == '#' && col == 0) {
            int start_col = col;
            while (i < len && text[i] != '\n') {
                i++;
                col++;
            }
            syntax_add_token(sh, row, start_col, row, col, SYNTAX_KEYWORD);
            continue;
        }
        
        /* Comments - // style */
        if (c == '/' && i + 1 < len && text[i+1] == '/') {
            int start_col = col;
            while (i < len && text[i] != '\n') {
                i++;
                col++;
            }
            syntax_add_token(sh, row, start_col, row, col, SYNTAX_COMMENT);
            continue;
        }
        
        /* Comments - C style (slash-asterisk) */
        if (c == '/' && i + 1 < len && text[i+1] == '*') {
            int start_row = row, start_col = col;
            i += 2;
            col += 2;
            while (i + 1 < len && !(text[i] == '*' && text[i+1] == '/')) {
                if (text[i] == '\n') {
                    row++;
                    col = 0;
                } else {
                    col++;
                }
                i++;
            }
            if (i + 1 < len) {
                i += 2;
                col += 2;
            }
            syntax_add_token(sh, start_row, start_col, row, col, SYNTAX_COMMENT);
            continue;
        }
        
        /* Strings - double quotes */
        if (c == '"') {
            int start_col = col;
            i++;
            col++;
            while (i < len && text[i] != '"') {
                if (text[i] == '\\' && i + 1 < len) {
                    i += 2;
                    col += 2;
                } else {
                    i++;
                    col++;
                }
            }
            if (i < len) {
                i++;
                col++;
            }
            syntax_add_token(sh, row, start_col, row, col, SYNTAX_STRING);
            continue;
        }
        
        /* Strings - single quotes */
        if (c == '\'') {
            int start_col = col;
            i++;
            col++;
            while (i < len && text[i] != '\'') {
                if (text[i] == '\\' && i + 1 < len) {
                    i += 2;
                    col += 2;
                } else {
                    i++;
                    col++;
                }
            }
            if (i < len) {
                i++;
                col++;
            }
            syntax_add_token(sh, row, start_col, row, col, SYNTAX_STRING);
            continue;
        }
        
        /* Backtick strings (template literals in JS/TS) */
        if (c == '`') {
            int start_col = col;
            i++;
            col++;
            while (i < len && text[i] != '`') {
                if (text[i] == '\\' && i + 1 < len) {
                    i += 2;
                    col += 2;
                } else if (text[i] == '\n') {
                    row++;
                    col = 0;
                    i++;
                } else {
                    i++;
                    col++;
                }
            }
            if (i < len) {
                i++;
                col++;
            }
            syntax_add_token(sh, row, start_col, row, col, SYNTAX_STRING);
            continue;
        }
        
        /* Numbers - more robust detection (hex, binary, scientific) */
        if (isdigit((unsigned char)c)) {
            int start_col = col;
            
            /* Check for hex (0x), binary (0b), or octal (0o) */
            if (c == '0' && i + 1 < len) {
                char next = text[i+1];
                if (next == 'x' || next == 'X' || next == 'b' || next == 'B' || next == 'o' || next == 'O') {
                    i += 2;
                    col += 2;
                    while (i < len && (isxdigit((unsigned char)text[i]) || text[i] == '_')) {
                        i++;
                        col++;
                    }
                    i--;
                    col--;
                    syntax_add_token(sh, row, start_col, row, col + 1, SYNTAX_NUMBER);
                    col++;
                    continue;
                }
            }
            
            /* Decimal or floating point */
            while (i < len && (isdigit((unsigned char)text[i]) || text[i] == '.' || text[i] == '_')) {
                i++;
                col++;
            }
            
            /* Scientific notation (1e5, 1.5e-3) */
            if (i < len && (text[i] == 'e' || text[i] == 'E')) {
                i++;
                col++;
                if (i < len && (text[i] == '+' || text[i] == '-')) {
                    i++;
                    col++;
                }
                while (i < len && isdigit((unsigned char)text[i])) {
                    i++;
                    col++;
                }
            }
            
            i--;
            col--;
            syntax_add_token(sh, row, start_col, row, col + 1, SYNTAX_NUMBER);
            col++;
            continue;
        }
        
        /* Identifiers and keywords */
        if (isalpha((unsigned char)c) || c == '_') {
            int start_col = col;
            size_t start_i = i;
            while (i < len && (isalnum((unsigned char)text[i]) || text[i] == '_')) {
                i++;
                col++;
            }
            
            /* Check if it's a keyword */
            size_t word_len = i - start_i;
            char word[64];
            if (word_len < sizeof(word)) {
                strncpy(word, &text[start_i], word_len);
                word[word_len] = '\0';
                
                int is_keyword = 0;
                for (int k = 0; keywords[k] != NULL; k++) {
                    if (strcmp(word, keywords[k]) == 0) {
                        is_keyword = 1;
                        break;
                    }
                }
                
                if (is_keyword) {
                    syntax_add_token(sh, row, start_col, row, col, SYNTAX_KEYWORD);
                }
            }
            
            i--;
            col--;
            continue;
        }
        
        col++;
    }
}
