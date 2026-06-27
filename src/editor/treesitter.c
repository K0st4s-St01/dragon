#include "dragon_editor/treesitter.h"
#include "dragon_editor/syntax.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>

/* Dynamic symbol loading for language parsers */
typedef TSLanguage *(*TSLanguageFn)(void);

/* Input reading callback for tree-sitter parser */
typedef struct {
    const char *text;
    uint32_t len;
} TSInputPayload;

static const char* ts_input_read(void *payload, uint32_t byte_index, TSPoint position, uint32_t *bytes_read) {
    (void)position;  /* unused */
    TSInputPayload *input = (TSInputPayload *)payload;
    
    if (byte_index >= input->len) {
        *bytes_read = 0;
        return "";
    }
    
    *bytes_read = input->len - byte_index;
    return input->text + byte_index;
}

static TSLanguage* treesitter_load_language_from_file(const char *language_name) {
    char lib_path[256];
    snprintf(lib_path, sizeof(lib_path), "libtree-sitter-%s.so", language_name);
    
    void *handle = dlopen(lib_path, RTLD_LAZY);
    if (!handle) {
        return NULL;
    }
    
    char fn_name[128];
    snprintf(fn_name, sizeof(fn_name), "tree_sitter_%s", language_name);
    
    TSLanguageFn lang_fn = (TSLanguageFn)dlsym(handle, fn_name);
    if (!lang_fn) {
        dlclose(handle);
        return NULL;
    }
    
    return lang_fn();
}

/* Map file extensions to language names */
const char* treesitter_language_name_for_extension(const char *file_extension) {
    if (!file_extension) return NULL;
    
    /* Remove leading dot if present */
    if (file_extension[0] == '.') file_extension++;
    
    /* C/C++ - map all variants to C grammar (TreeSitter C parser handles C++) */
    if (strcmp(file_extension, "c") == 0) return "c";
    if (strcmp(file_extension, "h") == 0) return "c";
    if (strcmp(file_extension, "cpp") == 0 || strcmp(file_extension, "cc") == 0 || 
        strcmp(file_extension, "cxx") == 0) return "c";
    if (strcmp(file_extension, "hpp") == 0 || strcmp(file_extension, "hh") == 0 || 
        strcmp(file_extension, "hxx") == 0) return "c";
    
    /* Objective-C/C++ - use C grammar for now */
    if (strcmp(file_extension, "m") == 0) return "c";
    if (strcmp(file_extension, "mm") == 0) return "c";
    
    /* CUDA - use C grammar (compatible syntax) */
    if (strcmp(file_extension, "cu") == 0) return "c";
    
    if (strcmp(file_extension, "py") == 0) return "python";
    if (strcmp(file_extension, "js") == 0 || strcmp(file_extension, "mjs") == 0) return "javascript";
    if (strcmp(file_extension, "ts") == 0) return "typescript";
    if (strcmp(file_extension, "sh") == 0) return "bash";
    if (strcmp(file_extension, "lua") == 0) return "lua";
    if (strcmp(file_extension, "md") == 0) return "markdown";
    
    return NULL;
}

TreeSitterManager* treesitter_manager_new(void) {
    TreeSitterManager *mgr = (TreeSitterManager *)malloc(sizeof(TreeSitterManager));
    if (!mgr) return NULL;
    
    mgr->language_count = 0;
    for (int i = 0; i < 32; i++) {
        mgr->languages[i] = NULL;
    }
    
    return mgr;
}

void treesitter_manager_free(TreeSitterManager *mgr) {
    if (!mgr) return;
    
    for (uint32_t i = 0; i < mgr->language_count; i++) {
        TreeSitterLanguage *lang = mgr->languages[i];
        if (!lang) continue;
        
        if (lang->parser) ts_parser_delete(lang->parser);
        if (lang->tree) ts_tree_delete(lang->tree);
        if (lang->highlight_query) ts_query_delete(lang->highlight_query);
        free(lang->source_text);
        free(lang);
    }
    free(mgr);
}

bool treesitter_load_language(TreeSitterManager *mgr, const char *file_extension) {
    if (!mgr || !file_extension) return false;
    if (mgr->language_count >= 32) return false;
    
    const char *lang_name = treesitter_language_name_for_extension(file_extension);
    if (!lang_name) return false;
    
    /* Check if already loaded */
    for (uint32_t i = 0; i < mgr->language_count; i++) {
        if (mgr->languages[i] && mgr->languages[i]->language) {
            if (strcmp(ts_language_name(mgr->languages[i]->language), lang_name) == 0) {
                return true;
            }
        }
    }
    
    TSLanguage *language = treesitter_load_language_from_file(lang_name);
    if (!language) return false;
    
    TreeSitterLanguage *lang = (TreeSitterLanguage *)malloc(sizeof(TreeSitterLanguage));
    if (!lang) return false;
    
    lang->language = language;
    lang->parser = ts_parser_new();
    ts_parser_set_language(lang->parser, language);
    lang->tree = NULL;
    lang->highlight_query = NULL;
    lang->capture_names = NULL;
    lang->capture_count = 0;
    lang->source_text = NULL;
    lang->source_len = 0;
    
    mgr->languages[mgr->language_count++] = lang;
    return true;
}

TreeSitterLanguage* treesitter_get_language(TreeSitterManager *mgr, const char *language_name) {
    if (!mgr || !language_name) return NULL;
    
    for (uint32_t i = 0; i < mgr->language_count; i++) {
        TreeSitterLanguage *lang = mgr->languages[i];
        if (!lang || !lang->language) continue;
        
        if (strcmp(ts_language_name(lang->language), language_name) == 0) {
            return lang;
        }
    }
    
    return NULL;
}

void treesitter_parse(TreeSitterLanguage *lang, const char *text, uint32_t len) {
    if (!lang || !text) return;
    
    if (lang->tree) {
        ts_tree_delete(lang->tree);
    }
    free(lang->source_text);
    lang->source_text = malloc(len + 1);
    if (lang->source_text) {
        memcpy(lang->source_text, text, len);
        lang->source_text[len] = '\0';
        lang->source_len = len;
    } else {
        lang->source_len = 0;
    }
    
    TSInputPayload payload = {
        .text = text,
        .len = len
    };
    
    TSInput input = {
        .payload = &payload,
        .read = ts_input_read,
        .encoding = TSInputEncodingUTF8
    };
    
    lang->tree = ts_parser_parse(lang->parser, NULL, input);
}

static char *treesitter_node_text(TreeSitterLanguage *lang, TSNode node) {
    if (!lang || !lang->source_text) return strdup("");
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    if (end < start || end > lang->source_len) return strdup("");
    size_t len = end - start;
    char *text = malloc(len + 1);
    if (!text) return strdup("");
    memcpy(text, lang->source_text + start, len);
    text[len] = '\0';
    return text;
}

TreeSitterHighlight treesitter_get_highlight_at(TreeSitterLanguage *lang, uint32_t row, uint32_t col) {
    TreeSitterHighlight result = {0};
    
    if (!lang || !lang->tree) return result;
    
    TSNode root = ts_tree_root_node(lang->tree);
    TSPoint point = {row, col};
    TSNode node = ts_node_named_descendant_for_point_range(root, point, point);
    
    if (!ts_node_is_null(node)) {
        result.start_col = ts_node_start_byte(node);
        result.end_col = ts_node_end_byte(node);
        /* Map node type to capture ID for syntax highlighting */
        const char *node_type = ts_node_type(node);
        if (strcmp(node_type, "keyword") == 0) result.capture_id = 1;
        else if (strcmp(node_type, "string") == 0) result.capture_id = 2;
        else if (strcmp(node_type, "number") == 0) result.capture_id = 3;
        else if (strcmp(node_type, "comment") == 0) result.capture_id = 4;
        else if (strcmp(node_type, "function") == 0) result.capture_id = 5;
        result.capture_name = node_type;
    }
    
    return result;
}

bool treesitter_describe_node_at(TreeSitterLanguage *lang, uint32_t row, uint32_t col,
                                 char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return false;
    buf[0] = '\0';
    if (!lang || !lang->tree) return false;

    TSNode root = ts_tree_root_node(lang->tree);
    TSPoint point = {row, col};
    TSNode node = ts_node_named_descendant_for_point_range(root, point, point);
    if (ts_node_is_null(node)) return false;

    TSPoint start = ts_node_start_point(node);
    TSPoint end = ts_node_end_point(node);
    TSNode parent = ts_node_parent(node);
    const char *parent_type = ts_node_is_null(parent) ? "<root>" : ts_node_type(parent);
    snprintf(buf, buf_size,
             "node: %s\nparent: %s\nrange: %u:%u-%u:%u\nnamed: %s\nchildren: %u",
             ts_node_type(node), parent_type,
             start.row + 1, start.column + 1, end.row + 1, end.column + 1,
             ts_node_is_named(node) ? "yes" : "no",
             ts_node_child_count(node));
    return true;
}

bool treesitter_parent_range_at(TreeSitterLanguage *lang, uint32_t row, uint32_t col,
                                uint32_t *start_row, uint32_t *start_col,
                                uint32_t *end_row, uint32_t *end_col) {
    if (!lang || !lang->tree) return false;
    TSNode root = ts_tree_root_node(lang->tree);
    TSPoint point = {row, col};
    TSNode node = ts_node_named_descendant_for_point_range(root, point, point);
    if (ts_node_is_null(node)) return false;

    TSNode parent = ts_node_parent(node);
    while (!ts_node_is_null(parent) && !ts_node_is_named(parent))
        parent = ts_node_parent(parent);
    if (ts_node_is_null(parent)) return false;

    TSPoint start = ts_node_start_point(parent);
    TSPoint end = ts_node_end_point(parent);
    if (start_row) *start_row = start.row;
    if (start_col) *start_col = start.column;
    if (end_row) *end_row = end.row;
    if (end_col) *end_col = end.column;
    return true;
}

static bool node_range(TSNode node, uint32_t *start_row, uint32_t *start_col,
                       uint32_t *end_row, uint32_t *end_col) {
    if (ts_node_is_null(node)) return false;
    TSPoint start = ts_node_start_point(node);
    TSPoint end = ts_node_end_point(node);
    if (start_row) *start_row = start.row;
    if (start_col) *start_col = start.column;
    if (end_row) *end_row = end.row;
    if (end_col) *end_col = end.column;
    return true;
}

static TSNode node_for_range(TreeSitterLanguage *lang,
                             uint32_t range_start_row, uint32_t range_start_col,
                             uint32_t range_end_row, uint32_t range_end_col) {
    if (!lang || !lang->tree) return (TSNode){0};
    TSNode root = ts_tree_root_node(lang->tree);
    TSPoint start = {range_start_row, range_start_col};
    TSPoint end = {range_end_row, range_end_col};
    if (range_end_row < range_start_row ||
        (range_end_row == range_start_row && range_end_col < range_start_col))
        end = start;
    return ts_node_named_descendant_for_point_range(root, start, end);
}

bool treesitter_child_range_for_range(TreeSitterLanguage *lang,
                                      uint32_t range_start_row, uint32_t range_start_col,
                                      uint32_t range_end_row, uint32_t range_end_col,
                                      uint32_t *start_row, uint32_t *start_col,
                                      uint32_t *end_row, uint32_t *end_col) {
    TSNode node = node_for_range(lang, range_start_row, range_start_col,
                                 range_end_row, range_end_col);
    if (ts_node_is_null(node)) return false;

    uint32_t child_count = ts_node_named_child_count(node);
    if (child_count == 0) return false;
    TSNode child = ts_node_named_child(node, 0);
    return node_range(child, start_row, start_col, end_row, end_col);
}

bool treesitter_sibling_range_for_range(TreeSitterLanguage *lang,
                                        uint32_t range_start_row, uint32_t range_start_col,
                                        uint32_t range_end_row, uint32_t range_end_col,
                                        int direction,
                                        uint32_t *start_row, uint32_t *start_col,
                                        uint32_t *end_row, uint32_t *end_col) {
    TSNode node = node_for_range(lang, range_start_row, range_start_col,
                                 range_end_row, range_end_col);
    if (ts_node_is_null(node)) return false;

    TSNode sibling = direction < 0 ? ts_node_prev_named_sibling(node)
                                   : ts_node_next_named_sibling(node);
    return node_range(sibling, start_row, start_col, end_row, end_col);
}

static void fill_range(TreeSitterRange *range, TSNode node) {
    TSPoint start = ts_node_start_point(node);
    TSPoint end = ts_node_end_point(node);
    range->start_row = start.row;
    range->start_col = start.column;
    range->end_row = end.row;
    range->end_col = end.column;
}

int treesitter_child_ranges_for_range(TreeSitterLanguage *lang,
                                      uint32_t range_start_row, uint32_t range_start_col,
                                      uint32_t range_end_row, uint32_t range_end_col,
                                      TreeSitterRange *ranges, int max_ranges) {
    if (!ranges || max_ranges <= 0) return 0;
    TSNode node = node_for_range(lang, range_start_row, range_start_col,
                                 range_end_row, range_end_col);
    if (ts_node_is_null(node)) return 0;

    int count = 0;
    uint32_t child_count = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < child_count && count < max_ranges; i++) {
        TSNode child = ts_node_named_child(node, i);
        if (ts_node_is_null(child)) continue;
        fill_range(&ranges[count++], child);
    }
    return count;
}

int treesitter_sibling_ranges_for_range(TreeSitterLanguage *lang,
                                        uint32_t range_start_row, uint32_t range_start_col,
                                        uint32_t range_end_row, uint32_t range_end_col,
                                        TreeSitterRange *ranges, int max_ranges) {
    if (!ranges || max_ranges <= 0) return 0;
    TSNode node = node_for_range(lang, range_start_row, range_start_col,
                                 range_end_row, range_end_col);
    if (ts_node_is_null(node)) return 0;

    TSNode parent = ts_node_parent(node);
    if (ts_node_is_null(parent)) return 0;

    int count = 0;
    uint32_t child_count = ts_node_named_child_count(parent);
    for (uint32_t i = 0; i < child_count && count < max_ranges; i++) {
        TSNode child = ts_node_named_child(parent, i);
        if (ts_node_is_null(child)) continue;
        fill_range(&ranges[count++], child);
    }
    return count;
}

TreeSitterSymbols treesitter_extract_symbols(TreeSitterLanguage *lang) {
    TreeSitterSymbols result = {0};
    
    if (!lang || !lang->tree) return result;
    
    result.symbols = (TreeSitterSymbol *)malloc(sizeof(TreeSitterSymbol) * 256);
    result.count = 0;
    
    if (!result.symbols) return result;
    
    TSNode root = ts_tree_root_node(lang->tree);
    
    /* Simple query for common patterns - can be extended */
    const char *query_str = NULL;
    const char *lang_name = ts_language_name(lang->language);
    
    /* Basic function/definition queries per language */
    if (strcmp(lang_name, "c") == 0) {
        query_str = "(function_definition name: (identifier) @function)";
    } else if (strcmp(lang_name, "python") == 0) {
        query_str = "(function_definition name: (identifier) @function)";
    }
    
    if (query_str) {
        uint32_t error_offset = 0;
        TSQueryError error_type = TSQueryErrorNone;
        TSQuery *query = ts_query_new(lang->language, query_str, strlen(query_str), &error_offset, &error_type);
        
        if (query && error_type == TSQueryErrorNone) {
            TSQueryCursor *cursor = ts_query_cursor_new();
            ts_query_cursor_exec(cursor, query, root);
            
            TSQueryMatch match;
            while (ts_query_cursor_next_match(cursor, &match)) {
                if (result.count >= 256) break;
                
                for (uint32_t i = 0; i < match.capture_count; i++) {
                    TSQueryCapture capture = match.captures[i];
                    TSNode node = capture.node;
                    
                    result.symbols[result.count].name = treesitter_node_text(lang, node);
                    result.symbols[result.count].kind = "function";
                    result.symbols[result.count].start_row = ts_node_start_point(node).row;
                    result.symbols[result.count].start_col = ts_node_start_point(node).column;
                    result.symbols[result.count].end_row = ts_node_end_point(node).row;
                    result.symbols[result.count].end_col = ts_node_end_point(node).column;
                    result.count++;
                }
            }
            
            ts_query_cursor_delete(cursor);
            ts_query_delete(query);
        }
    }
    
    return result;
}

void treesitter_symbols_free(TreeSitterSymbols *symbols) {
    if (!symbols) return;
    for (uint32_t i = 0; i < symbols->count; i++)
        free(symbols->symbols[i].name);
    free(symbols->symbols);
    symbols->count = 0;
}

/* Map tree-sitter node type to SyntaxType */
static SyntaxType treesitter_node_type_to_syntax(const char *node_type, bool is_named) {
    /* For anonymous/literal nodes, the type IS the keyword text */
    if (!is_named) {
        if (strcmp(node_type, "if") == 0 || strcmp(node_type, "else") == 0 ||
            strcmp(node_type, "for") == 0 || strcmp(node_type, "while") == 0 ||
            strcmp(node_type, "do") == 0 || strcmp(node_type, "switch") == 0 ||
            strcmp(node_type, "case") == 0 || strcmp(node_type, "default") == 0 ||
            strcmp(node_type, "break") == 0 || strcmp(node_type, "continue") == 0 ||
            strcmp(node_type, "return") == 0 || strcmp(node_type, "goto") == 0 ||
            strcmp(node_type, "struct") == 0 || strcmp(node_type, "union") == 0 ||
            strcmp(node_type, "enum") == 0 || strcmp(node_type, "class") == 0 ||
            strcmp(node_type, "typedef") == 0 || strcmp(node_type, "sizeof") == 0 ||
            strcmp(node_type, "extern") == 0 || strcmp(node_type, "static") == 0 ||
            strcmp(node_type, "const") == 0 || strcmp(node_type, "volatile") == 0 ||
            strcmp(node_type, "auto") == 0 || strcmp(node_type, "register") == 0 ||
            strcmp(node_type, "void") == 0 || strcmp(node_type, "int") == 0 ||
            strcmp(node_type, "float") == 0 || strcmp(node_type, "double") == 0 ||
            strcmp(node_type, "char") == 0 || strcmp(node_type, "short") == 0 ||
            strcmp(node_type, "long") == 0 || strcmp(node_type, "unsigned") == 0 ||
            strcmp(node_type, "signed") == 0 || strcmp(node_type, "bool") == 0 ||
            strcmp(node_type, "true") == 0 || strcmp(node_type, "false") == 0 ||
            strcmp(node_type, "null") == 0 || strcmp(node_type, "nil") == 0 ||
            strcmp(node_type, "def") == 0 || strcmp(node_type, "fn") == 0 ||
            strcmp(node_type, "func") == 0 || strcmp(node_type, "var") == 0 ||
            strcmp(node_type, "let") == 0 || strcmp(node_type, "new") == 0 ||
            strcmp(node_type, "delete") == 0 || strcmp(node_type, "try") == 0 ||
            strcmp(node_type, "catch") == 0 || strcmp(node_type, "finally") == 0 ||
            strcmp(node_type, "throw") == 0 || strcmp(node_type, "throws") == 0 ||
            strcmp(node_type, "import") == 0 || strcmp(node_type, "include") == 0 ||
            strcmp(node_type, "require") == 0 || strcmp(node_type, "module") == 0 ||
            strcmp(node_type, "package") == 0 || strcmp(node_type, "namespace") == 0 ||
            strcmp(node_type, "public") == 0 || strcmp(node_type, "private") == 0 ||
            strcmp(node_type, "protected") == 0 || strcmp(node_type, "async") == 0 ||
            strcmp(node_type, "await") == 0 || strcmp(node_type, "yield") == 0 ||
            strcmp(node_type, "self") == 0 || strcmp(node_type, "Self") == 0 ||
            strcmp(node_type, "interface") == 0 || strcmp(node_type, "trait") == 0 ||
            strcmp(node_type, "impl") == 0 || strcmp(node_type, "in") == 0 ||
            strcmp(node_type, "not") == 0 || strcmp(node_type, "and") == 0 ||
            strcmp(node_type, "or") == 0 || strcmp(node_type, "is") == 0 ||
            strcmp(node_type, "as") == 0 || strcmp(node_type, "with") == 0 ||
            strcmp(node_type, "from") == 0 || strcmp(node_type, "lambda") == 0 ||
            strcmp(node_type, "print") == 0 || strcmp(node_type, "pass") == 0 ||
            strcmp(node_type, "elif") == 0 || strcmp(node_type, "except") == 0 ||
            strcmp(node_type, "assert") == 0 || strcmp(node_type, "raise") == 0 ||
            strcmp(node_type, "local") == 0 || strcmp(node_type, "global") == 0 ||
            strcmp(node_type, "type") == 0 || strcmp(node_type, "ref") == 0 ||
            strcmp(node_type, "defer") == 0 || strcmp(node_type, "go") == 0 ||
            strcmp(node_type, "chan") == 0 || strcmp(node_type, "select") == 0 ||
            strcmp(node_type, "struct") == 0 || strcmp(node_type, "make") == 0 ||
            strcmp(node_type, "len") == 0 || strcmp(node_type, "append") == 0 ||
            strcmp(node_type, "panic") == 0 || strcmp(node_type, "recover") == 0) {
            return SYNTAX_KEYWORD;
        }
        /* Operators */
        if (strcmp(node_type, "+") == 0 || strcmp(node_type, "-") == 0 ||
            strcmp(node_type, "*") == 0 || strcmp(node_type, "/") == 0 ||
            strcmp(node_type, "%") == 0 || strcmp(node_type, "=") == 0 ||
            strcmp(node_type, "==") == 0 || strcmp(node_type, "!=") == 0 ||
            strcmp(node_type, "<") == 0 || strcmp(node_type, ">") == 0 ||
            strcmp(node_type, "<=") == 0 || strcmp(node_type, ">=") == 0 ||
            strcmp(node_type, "+=") == 0 || strcmp(node_type, "-=") == 0 ||
            strcmp(node_type, "*=") == 0 || strcmp(node_type, "/=") == 0 ||
            strcmp(node_type, "!") == 0 || strcmp(node_type, "&") == 0 ||
            strcmp(node_type, "|") == 0 || strcmp(node_type, "^") == 0 ||
            strcmp(node_type, "~") == 0 || strcmp(node_type, "++") == 0 ||
            strcmp(node_type, "--") == 0 || strcmp(node_type, "&&") == 0 ||
            strcmp(node_type, "||") == 0 || strcmp(node_type, "->") == 0 ||
            strcmp(node_type, ".") == 0 || strcmp(node_type, "::") == 0 ||
            strcmp(node_type, "<<") == 0 || strcmp(node_type, ">>") == 0) {
            return SYNTAX_OPERATOR;
        }
        return SYNTAX_NORMAL;
    }

    /* Named nodes - language-specific types */
    /* C/C++ types */
    if (strcmp(node_type, "primitive_type") == 0) return SYNTAX_TYPE;
    if (strcmp(node_type, "type_identifier") == 0) return SYNTAX_TYPE;
    if (strcmp(node_type, "sized_type_specifier") == 0) return SYNTAX_TYPE;
    
    /* Strings */
    if (strcmp(node_type, "string_literal") == 0) return SYNTAX_STRING;
    if (strcmp(node_type, "string") == 0) return SYNTAX_STRING;
    if (strcmp(node_type, "concatenated_string") == 0) return SYNTAX_STRING;
    if (strcmp(node_type, "raw_string_literal") == 0) return SYNTAX_STRING;
    if (strcmp(node_type, "character_literal") == 0) return SYNTAX_STRING;
    if (strcmp(node_type, "escape_sequence") == 0) return SYNTAX_STRING;
    if (strcmp(node_type, "system_lib_string") == 0) return SYNTAX_STRING;
    if (strcmp(node_type, "interpreted_string_literal") == 0) return SYNTAX_STRING;
    if (strcmp(node_type, "raw_string_literal") == 0) return SYNTAX_STRING;
    if (strcmp(node_type, "template_string") == 0) return SYNTAX_STRING;
    if (strcmp(node_type, "string_content") == 0) return SYNTAX_STRING;
    
    /* Numbers */
    if (strcmp(node_type, "number_literal") == 0) return SYNTAX_NUMBER;
    if (strcmp(node_type, "integer_literal") == 0) return SYNTAX_NUMBER;
    if (strcmp(node_type, "float_literal") == 0) return SYNTAX_NUMBER;
    if (strcmp(node_type, "hex_literal") == 0) return SYNTAX_NUMBER;
    if (strcmp(node_type, "octal_literal") == 0) return SYNTAX_NUMBER;
    if (strcmp(node_type, "binary_literal") == 0) return SYNTAX_NUMBER;
    if (strcmp(node_type, "oct_number_literal") == 0) return SYNTAX_NUMBER;
    if (strcmp(node_type, "hex_number_literal") == 0) return SYNTAX_NUMBER;
    if (strcmp(node_type, "decimal_literal") == 0) return SYNTAX_NUMBER;
    if (strcmp(node_type, "float") == 0) return SYNTAX_NUMBER;
    if (strcmp(node_type, "imaginary_literal") == 0) return SYNTAX_NUMBER;
    
    /* Comments */
    if (strcmp(node_type, "comment") == 0) return SYNTAX_COMMENT;
    if (strcmp(node_type, "line_comment") == 0) return SYNTAX_COMMENT;
    if (strcmp(node_type, "block_comment") == 0) return SYNTAX_COMMENT;
    
    /* Functions */
    if (strcmp(node_type, "function_definition") == 0) return SYNTAX_FUNCTION;
    if (strcmp(node_type, "call_expression") == 0) return SYNTAX_FUNCTION;
    if (strcmp(node_type, "method_definition") == 0) return SYNTAX_FUNCTION;
    if (strcmp(node_type, "function_declarator") == 0) return SYNTAX_FUNCTION;
    if (strcmp(node_type, "function_item") == 0) return SYNTAX_FUNCTION;
    if (strcmp(node_type, "function_declaration") == 0) return SYNTAX_FUNCTION;
    if (strcmp(node_type, "arrow_function") == 0) return SYNTAX_FUNCTION;
    if (strcmp(node_type, "method_call_expression") == 0) return SYNTAX_FUNCTION;
    if (strcmp(node_type, "call") == 0) return SYNTAX_FUNCTION;
    if (strcmp(node_type, "subscript") == 0) return SYNTAX_FUNCTION;
    
    /* Variables */
    if (strcmp(node_type, "identifier") == 0) return SYNTAX_VARIABLE;
    if (strcmp(node_type, "field_identifier") == 0) return SYNTAX_VARIABLE;
    if (strcmp(node_type, "parameter_identifier") == 0) return SYNTAX_VARIABLE;
    if (strcmp(node_type, "variable_identifier") == 0) return SYNTAX_VARIABLE;
    
    /* Macros */
    if (strcmp(node_type, "macro_definition") == 0) return SYNTAX_MACRO;
    if (strcmp(node_type, "preproc_def") == 0) return SYNTAX_MACRO;
    if (strcmp(node_type, "preproc_function_def") == 0) return SYNTAX_MACRO;
    if (strcmp(node_type, "preproc_include") == 0) return SYNTAX_MACRO;
    if (strcmp(node_type, "preproc_call") == 0) return SYNTAX_MACRO;
    if (strcmp(node_type, "preproc_if") == 0) return SYNTAX_MACRO;
    if (strcmp(node_type, "preproc_else") == 0) return SYNTAX_MACRO;
    if (strcmp(node_type, "preproc_elif") == 0) return SYNTAX_MACRO;
    if (strcmp(node_type, "preproc_directive") == 0) return SYNTAX_MACRO;
    if (strcmp(node_type, "system_lib_string") == 0) return SYNTAX_MACRO;
    if (strcmp(node_type, "string_literal") == 0) return SYNTAX_STRING;
    
    /* Namespace */
    if (strcmp(node_type, "namespace_definition") == 0) return SYNTAX_NAMESPACE;
    if (strcmp(node_type, "namespace") == 0) return SYNTAX_NAMESPACE;
    if (strcmp(node_type, "module") == 0) return SYNTAX_NAMESPACE;
    if (strcmp(node_type, "package_clause") == 0) return SYNTAX_NAMESPACE;
    if (strcmp(node_type, "import_spec") == 0) return SYNTAX_NAMESPACE;
    if (strcmp(node_type, "import_declaration") == 0) return SYNTAX_NAMESPACE;
    
    return SYNTAX_NORMAL;
}

/* Recursively walk tree and generate syntax tokens */
static void treesitter_walk_node(TSNode node, SyntaxHighlighting *sh) {
    if (ts_node_is_null(node)) return;
    
    /* Get node type and map to syntax type */
    const char *node_type = ts_node_type(node);
    bool is_named = ts_node_is_named(node);
    SyntaxType syntax_type = treesitter_node_type_to_syntax(node_type, is_named);
    
    /* Only add token if it's not NORMAL (to avoid flooding with useless tokens) */
    if (syntax_type != SYNTAX_NORMAL) {
        TSPoint start = ts_node_start_point(node);
        TSPoint end = ts_node_end_point(node);
        
        syntax_add_token(sh, 
                        (int)start.row, (int)start.column,
                        (int)end.row, (int)end.column,
                        syntax_type);
    }
    
    /* Recurse into children */
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        treesitter_walk_node(child, sh);
    }
}

/* Generate syntax tokens from tree-sitter parse tree */
bool treesitter_generate_syntax_tokens(TreeSitterLanguage *lang, SyntaxHighlighting *sh) {
    if (!lang || !lang->tree || !sh) return false;
    
    /* Save existing tokens in case tree-sitter produces nothing */
    int saved_count = sh->token_count;
    
    /* Walk the tree and generate tokens into a temporary area past the existing count */
    TSNode root = ts_tree_root_node(lang->tree);
    treesitter_walk_node(root, sh);
    
    /* Only replace if tree-sitter actually produced new tokens */
    if (sh->token_count > saved_count) {
        /* Move new tokens to the beginning, discarding old ones */
        memmove(sh->tokens, sh->tokens + saved_count,
                (sh->token_count - saved_count) * sizeof(SyntaxToken));
        sh->token_count = sh->token_count - saved_count;
        return true;
    } else {
        /* Tree-sitter produced nothing — keep existing tokens */
        sh->token_count = saved_count;
        return false;
    }
}
