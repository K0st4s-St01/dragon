#ifndef DE_SYNTAX_H
#define DE_SYNTAX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    SYNTAX_NORMAL,
    SYNTAX_KEYWORD,
    SYNTAX_STRING,
    SYNTAX_NUMBER,
    SYNTAX_COMMENT,
    SYNTAX_FUNCTION,
    SYNTAX_TYPE,
    SYNTAX_ERROR,
    SYNTAX_WARNING,
} SyntaxType;

typedef struct {
    int start_row;
    int start_col;
    int end_row;
    int end_col;
    SyntaxType type;
} SyntaxToken;

typedef struct {
    SyntaxToken *tokens;
    int token_count;
    int token_capacity;
    char *language_id;
} SyntaxHighlighting;

/* Initialize and manage syntax highlighting */
void syntax_init(SyntaxHighlighting *sh, const char *language_id);
void syntax_free(SyntaxHighlighting *sh);
void syntax_clear(SyntaxHighlighting *sh);

/* Add tokens (from LSP semantic tokens) */
void syntax_add_token(SyntaxHighlighting *sh, int sr, int sc, int er, int ec, SyntaxType type);

/* Query syntax for a position */
SyntaxType syntax_get_type_at(SyntaxHighlighting *sh, int row, int col);

/* Update from LSP semantic tokens response */
void syntax_update_from_lsp(SyntaxHighlighting *sh, const char *response);

/* Basic syntax highlighting for files without LSP */
void syntax_highlight_basic(SyntaxHighlighting *sh, const char *text, size_t len);

#endif
