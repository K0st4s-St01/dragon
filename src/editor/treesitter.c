#include "dragon_editor/treesitter.h"
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
static const char* treesitter_get_language_name(const char *file_extension) {
    if (!file_extension) return NULL;
    
    /* Remove leading dot if present */
    if (file_extension[0] == '.') file_extension++;
    
    if (strcmp(file_extension, "c") == 0) return "c";
    if (strcmp(file_extension, "h") == 0) return "c";
    if (strcmp(file_extension, "cpp") == 0 || strcmp(file_extension, "cc") == 0 || 
        strcmp(file_extension, "cxx") == 0) return "c";
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
        free(lang);
    }
    free(mgr);
}

bool treesitter_load_language(TreeSitterManager *mgr, const char *file_extension) {
    if (!mgr || !file_extension) return false;
    if (mgr->language_count >= 32) return false;
    
    const char *lang_name = treesitter_get_language_name(file_extension);
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
                    
                    result.symbols[result.count].name = ts_node_string(node);
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
    free(symbols->symbols);
    symbols->count = 0;
}
