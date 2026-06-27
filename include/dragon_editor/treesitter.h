#ifndef TREESITTER_H
#define TREESITTER_H

#include <tree_sitter/api.h>
#include <stdbool.h>
#include "syntax.h"

typedef struct {
    TSLanguage *language;
    TSParser *parser;
    TSTree *tree;
    TSQuery *highlight_query;
    const uint32_t *capture_names;
    uint32_t capture_count;
    char *source_text;
    uint32_t source_len;
} TreeSitterLanguage;

typedef struct {
    TreeSitterLanguage *languages[32];
    uint32_t language_count;
} TreeSitterManager;

/* Initialize tree-sitter manager */
TreeSitterManager* treesitter_manager_new(void);
void treesitter_manager_free(TreeSitterManager *mgr);

/* Load language by file extension */
bool treesitter_load_language(TreeSitterManager *mgr, const char *file_extension);
const char* treesitter_language_name_for_extension(const char *file_extension);

/* Get current language */
TreeSitterLanguage* treesitter_get_language(TreeSitterManager *mgr, const char *language_name);

/* Parse buffer */
void treesitter_parse(TreeSitterLanguage *lang, const char *text, uint32_t len);

/* Get syntax highlighting info at position */
typedef struct {
    uint32_t start_col;
    uint32_t end_col;
    uint32_t capture_id;
    const char *capture_name;
} TreeSitterHighlight;

TreeSitterHighlight treesitter_get_highlight_at(TreeSitterLanguage *lang, 
                                                  uint32_t row, uint32_t col);
bool treesitter_describe_node_at(TreeSitterLanguage *lang, uint32_t row, uint32_t col,
                                 char *buf, size_t buf_size);
bool treesitter_parent_range_at(TreeSitterLanguage *lang, uint32_t row, uint32_t col,
                                uint32_t *start_row, uint32_t *start_col,
                                uint32_t *end_row, uint32_t *end_col);

/* Query-based symbol extraction */
typedef struct {
    char *name;
    const char *kind;  /* "function", "class", "variable", etc */
    uint32_t start_row;
    uint32_t start_col;
    uint32_t end_row;
    uint32_t end_col;
} TreeSitterSymbol;

typedef struct {
    TreeSitterSymbol *symbols;
    uint32_t count;
} TreeSitterSymbols;

/* Extract symbols from tree using queries */
TreeSitterSymbols treesitter_extract_symbols(TreeSitterLanguage *lang);
void treesitter_symbols_free(TreeSitterSymbols *symbols);

/* Generate syntax tokens from tree-sitter parse tree */
bool treesitter_generate_syntax_tokens(TreeSitterLanguage *lang, SyntaxHighlighting *sh);

#endif /* TREESITTER_H */
