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
    /* Linear search with early termination */
    for (int i = 0; i < sh->token_count; i++) {
        SyntaxToken *token = &sh->tokens[i];
        
        /* Skip tokens that end before this position */
        if (token->end_row < row || (token->end_row == row && token->end_col <= col))
            continue;
        
        /* Skip tokens that start after this position */
        if (token->start_row > row || (token->start_row == row && token->start_col > col))
            continue;
        
        /* Token covers this position */
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
    }
    
    return SYNTAX_NORMAL;
}

void syntax_update_from_lsp(SyntaxHighlighting *sh, const char *response) {
    if (!response || !sh) return;
    
    /* Find the "data" array */
    const char *data_start = strstr(response, "\"data\"");
    if (!data_start) return;
    
    /* Find the opening bracket */
    const char *bracket = strchr(data_start, '[');
    if (!bracket) return;
    
    const char *p = bracket + 1;
    
    /* Check if data array is empty */
    while (*p && (*p == ' ' || *p == '\n' || *p == '\t')) p++;
    if (*p == ']') return;  /* Empty array - don't clear existing tokens */
    
    /* Clear existing tokens only when we have new LSP data */
    syntax_clear(sh);
    
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
