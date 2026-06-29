#include "dragon_editor/input.h"
#include "dragon_editor/app.h"
#include "dragon_editor/document.h"
#include "dragon_editor/mode.h"
#include "dragon_editor/command.h"
#include "panel_file_browser.h"
#include "panel_find_replace.h"
#include "panel_goto.h"
#include "panel_about.h"
#include "panel_buffer_picker.h"
#include "panel_jumplist_picker.h"
#include "panel_changed_files.h"
#include "panel_lsp_goto.h"
#include "panel_lsp_hover.h"
#include "panel_lsp_diagnostics.h"
#include "panel_space_menu.h"
#include "panel_symbols_picker.h"
#include "panel_rename.h"
#include "panel_code_actions.h"
#include "panel_palette.h"
#include "panel_settings.h"
#include "panel_plugins.h"
#include "panel_treesitter_inspector.h"
#include "panel_terminal.h"
#include "panel_workspace_symbols.h"
#include "panel_workspace_diagnostics.h"
#include "panel_completion.h"
#include "panel_notification.h"
#include "theme.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define CMD_BUF_MAX 1024
#define CMD_COMPLETION_MAX 12
#define CMD_COMPLETION_DETAIL_MAX 96

static char cmd_buf[CMD_BUF_MAX] = {0};
static int  cmd_len = 0;

typedef enum {
    CMD_COMPLETION_COMMAND = 0,
    CMD_COMPLETION_THEME,
    CMD_COMPLETION_PLUGIN,
    CMD_COMPLETION_PATH,
    CMD_COMPLETION_BUFFER,
} CmdCompletionKind;

typedef struct {
    const char *name;
    const char *detail;
    CmdCompletionKind kind;
    bool path_is_dir;
    int buffer_index;
} CmdCompletion;

typedef struct {
    const char *name;
    const char *detail;
} StaticCmdCompletion;

static CmdCompletion cmd_completions[CMD_COMPLETION_MAX];
static char cmd_completion_names[CMD_COMPLETION_MAX][CMD_BUF_MAX];
static char cmd_completion_details[CMD_COMPLETION_MAX][CMD_COMPLETION_DETAIL_MAX];
static int cmd_completion_count = 0;
static int cmd_completion_selected = 0;
static void command_completion_update(App *app);

static const StaticCmdCompletion static_cmds[] = {
    {"w", "Write current buffer"},
    {"write", "Write current buffer"},
    {"q", "Quit"},
    {"quit", "Quit"},
    {"wq", "Write and quit"},
    {"x", "Write and quit"},
    {"wqa", "Write all and quit"},
    {"qa", "Quit all"},
    {"bn", "Next buffer"},
    {"bnext", "Next buffer"},
    {"bp", "Previous buffer"},
    {"bprev", "Previous buffer"},
    {"b", "Switch buffer"},
    {"buffer", "Switch buffer"},
    {"bc", "Close buffer"},
    {"bclose", "Close buffer"},
    {"new", "New buffer"},
    {"n", "New buffer"},
    {"e", "Open file"},
    {"edit", "Open file"},
    {"o", "Open file"},
    {"open", "Open file"},
    {"r", "Read file into buffer"},
    {"read", "Read file into buffer"},
    {"mv", "Move/rename file"},
    {"move", "Move/rename file"},
    {"reload", "Reload current file"},
    {"rl", "Reload current file"},
    {"reload-all", "Reload current file"},
    {"rla", "Reload current file"},
    {"sort", "Sort selection"},
    {"fmt", "Format document"},
    {"format", "Format document"},
    {"theme", "Set theme"},
    {"colorscheme", "Set theme"},
    {"reflow", "Reflow text"},
    {"retab", "Convert indentation to tabs"},
    {"expandtab", "Convert indentation to spaces"},
    {"lsp-stop", "Stop LSP servers"},
    {"lsp-restart", "Restart LSP servers"},
    {"workspace-symbols", "Workspace symbols"},
    {"workspace-diagnostics", "Workspace diagnostics"},
    {"plugins", "Plugin manager"},
    {"plugin-enable", "Enable plugin"},
    {"plugin-disable", "Disable plugin"},
    {"plugin-toggle", "Toggle plugin"},
    {"config-reload", "Reload config"},
    {"tree-sitter-subtree", "Tree-sitter inspector"},
    {"ts-subtree", "Tree-sitter inspector"},
    {"tree-sitter-highlight-name", "Tree-sitter inspector"},
    {"tree-sitter-scopes", "Tree-sitter inspector"},
};

static void input_save_all_buffers(App *app) {
    int count = app_get_buffer_count(app);
    for (int i = 0; i < count; i++) {
        Document *doc = (Document *)app_get_doc_at(app, i);
        if (doc && doc->filepath)
            document_save(doc);
    }
}

static void input_format_document(App *app, Document *doc) {
    (void)doc;
    app_format_document(app);
}

static bool command_path_is_absolute(const char *path) {
    return path && path[0] == '/';
}

static void command_join_path(char *out, size_t out_size, const char *dir, const char *name) {
    if (!out || out_size == 0) return;
    if (!dir) dir = "";
    if (!name) name = "";
    size_t dir_len = strlen(dir);
    size_t name_len = strlen(name);
    bool need_slash = dir_len > 0 && dir[dir_len - 1] != '/';
    size_t pos = 0;
    if (dir_len >= out_size) dir_len = out_size - 1;
    memcpy(out, dir, dir_len);
    pos = dir_len;
    if (need_slash && pos + 1 < out_size)
        out[pos++] = '/';
    size_t remain = pos < out_size ? out_size - pos - 1 : 0;
    if (name_len > remain) name_len = remain;
    memcpy(out + pos, name, name_len);
    out[pos + name_len] = '\0';
}

static void command_resolve_workspace_path(App *app, const char *path,
                                           char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!path || !*path) return;
    if (command_path_is_absolute(path)) {
        snprintf(out, out_size, "%s", path);
        return;
    }
    if (path[0] == '~' && (path[1] == '\0' || path[1] == '/')) {
        const char *home = getenv("HOME");
        if (home && *home) {
            command_join_path(out, out_size, home, path[1] == '/' ? path + 2 : "");
            return;
        }
    }
    const char *root = app_get_workspace_root(app);
    command_join_path(out, out_size, root && *root ? root : ".", path);
}

static char *trim_command_arg(char *s) {
    if (!s) return s;
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
        *--end = '\0';
    return s;
}

static void input_theme_command(App *app, char *arg) {
    arg = trim_command_arg(arg);
    if (!arg || !*arg || strcmp(arg, "list") == 0) {
        const char *names[16];
        int count = theme_list_names(names, 16);
        char buf[256] = {0};
        for (int i = 0; i < count && i < 16; i++) {
            if (i > 0)
                strncat(buf, ", ", sizeof(buf) - strlen(buf) - 1);
            strncat(buf, names[i], sizeof(buf) - strlen(buf) - 1);
        }
        notification_push(NOTIF_INFO, "Themes: %s", buf);
        return;
    }

    if (!app_apply_theme(app, arg))
        notification_push(NOTIF_ERROR, "Unknown theme: %s", arg);
}

static bool plugin_matches_arg(const ConfigPlugin *plugin, const char *arg) {
    if (!plugin || !arg || !*arg) return false;
    if (strcmp(plugin->name, arg) == 0 || strcmp(plugin->path, arg) == 0)
        return true;
    if (plugin->path[0]) {
        const char *base = strrchr(plugin->path, '/');
        if (base && strcmp(base + 1, arg) == 0)
            return true;
    }
    return false;
}

static void input_plugin_command(App *app, char *arg, int action) {
    arg = trim_command_arg(arg);
    Config *cfg = app_get_config(app);
    if (!cfg || cfg->plugin_count <= 0) {
        notification_push(NOTIF_INFO, "No plugins configured");
        return;
    }
    if (!arg || !*arg) {
        panel_plugins_open(app);
        return;
    }

    for (int i = 0; i < cfg->plugin_count; i++) {
        if (!plugin_matches_arg(&cfg->plugins[i], arg))
            continue;
        bool enabled = action > 0 ? true : action < 0 ? false : !cfg->plugins[i].enabled;
        app_set_plugin_enabled(app, i, enabled);
        return;
    }

    notification_push(NOTIF_ERROR, "Unknown plugin: %s", arg);
}

void input_cmd_reset(void) {
    cmd_buf[0] = '\0';
    cmd_len = 0;
    cmd_completion_selected = 0;
    cmd_completion_count = 0;
}

const char *input_cmd_get(void) {
    return cmd_buf;
}

int input_cmd_completion_count(void) {
    return cmd_completion_count;
}

int input_cmd_completion_selected(void) {
    return cmd_completion_selected;
}

const char *input_cmd_completion_name(int index) {
    if (index < 0 || index >= cmd_completion_count) return NULL;
    return cmd_completions[index].name;
}

const char *input_cmd_completion_detail(int index) {
    if (index < 0 || index >= cmd_completion_count) return NULL;
    return cmd_completions[index].detail;
}

static bool cmd_has_arg_prefix(const char *cmd, const char *name, const char **arg) {
    size_t len = strlen(name);
    if (strncmp(cmd, name, len) != 0)
        return false;
    if (cmd[len] != ' ' && cmd[len] != '\0')
        return false;
    if (arg)
        *arg = cmd + len;
    return true;
}

static bool command_completion_matches(const char *name, const char *query) {
    if (!name || !query) return false;
    if (!*query) return true;
    size_t qlen = strlen(query);
    if (strncmp(name, query, qlen) == 0)
        return true;
    return strstr(name, query) != NULL;
}

static bool command_completion_exists(const char *name) {
    for (int i = 0; i < cmd_completion_count; i++) {
        if (strcmp(cmd_completions[i].name, name) == 0)
            return true;
    }
    return false;
}

static void command_completion_add(const char *name, const char *detail,
                                   CmdCompletionKind kind, bool path_is_dir,
                                   int buffer_index) {
    if (!name || !*name || cmd_completion_count >= CMD_COMPLETION_MAX)
        return;
    if (command_completion_exists(name))
        return;
    int index = cmd_completion_count++;
    snprintf(cmd_completion_names[index], sizeof(cmd_completion_names[index]), "%s", name);
    snprintf(cmd_completion_details[index], sizeof(cmd_completion_details[index]),
             "%s", detail ? detail : "");
    cmd_completions[index] = (CmdCompletion){
        .name = cmd_completion_names[index],
        .detail = cmd_completion_details[index],
        .kind = kind,
        .path_is_dir = path_is_dir,
        .buffer_index = buffer_index,
    };
}

static bool command_is_buffer_arg(const char *cmd, const char **arg) {
    return cmd_has_arg_prefix(cmd, "b", arg) ||
           cmd_has_arg_prefix(cmd, "buffer", arg) ||
           cmd_has_arg_prefix(cmd, "bc", arg) ||
           cmd_has_arg_prefix(cmd, "buffer-close", arg) ||
           cmd_has_arg_prefix(cmd, "bclose", arg);
}

static bool command_is_plugin_arg(const char *cmd, const char **arg) {
    return cmd_has_arg_prefix(cmd, "plugin-enable", arg) ||
           cmd_has_arg_prefix(cmd, "plugin-disable", arg) ||
           cmd_has_arg_prefix(cmd, "plugin-toggle", arg);
}

static const char *command_plugin_prefix(const char *cmd) {
    const char *arg = NULL;
    if (cmd_has_arg_prefix(cmd, "plugin-enable", &arg)) return "plugin-enable";
    if (cmd_has_arg_prefix(cmd, "plugin-disable", &arg)) return "plugin-disable";
    if (cmd_has_arg_prefix(cmd, "plugin-toggle", &arg)) return "plugin-toggle";
    return NULL;
}

static const char *command_buffer_prefix(const char *cmd) {
    const char *arg = NULL;
    if (cmd_has_arg_prefix(cmd, "buffer-close", &arg)) return "buffer-close";
    if (cmd_has_arg_prefix(cmd, "bclose", &arg)) return "bclose";
    if (cmd_has_arg_prefix(cmd, "bc", &arg)) return "bc";
    if (cmd_has_arg_prefix(cmd, "buffer", &arg)) return "buffer";
    if (cmd_has_arg_prefix(cmd, "b", &arg)) return "b";
    return NULL;
}

static bool command_is_path_arg(const char *cmd, const char **arg, bool *dirs_only) {
    struct PathCommand {
        const char *name;
        bool dirs_only;
    };
    static const struct PathCommand commands[] = {
        {"e", false}, {"edit", false}, {"o", false}, {"open", false},
        {"r", false}, {"read", false},
        {"w", false}, {"write", false}, {"w!", false}, {"write!", false},
        {"wq", false}, {"write-quit", false}, {"x", false},
        {"mv", false}, {"move", false},
        {"cwd", true}, {"open-workspace", true},
    };

    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (cmd_has_arg_prefix(cmd, commands[i].name, arg)) {
            if (dirs_only) *dirs_only = commands[i].dirs_only;
            return true;
        }
    }
    return false;
}

static const char *command_path_prefix(const char *cmd) {
    const char *arg = NULL;
    bool dirs_only = false;
    static const char *names[] = {
        "open-workspace", "write-quit", "write!", "write", "edit", "open",
        "read", "move", "cwd", "wq", "w!", "w", "x", "mv", "e", "o", "r",
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (cmd_has_arg_prefix(cmd, names[i], &arg)) {
            (void)dirs_only;
            return names[i];
        }
    }
    return NULL;
}

typedef struct {
    char name[256];
    bool is_dir;
} PathCompletionEntry;

static int path_completion_cmp(const void *a, const void *b) {
    const PathCompletionEntry *ea = (const PathCompletionEntry *)a;
    const PathCompletionEntry *eb = (const PathCompletionEntry *)b;
    if (ea->is_dir != eb->is_dir)
        return ea->is_dir ? -1 : 1;
    return strcmp(ea->name, eb->name);
}

static void command_expand_scan_path(App *app, const char *query,
                                     char *scan_dir, size_t scan_dir_size,
                                     char *display_prefix, size_t display_prefix_size,
                                     char *leaf_prefix, size_t leaf_prefix_size) {
    if (scan_dir && scan_dir_size > 0) scan_dir[0] = '\0';
    if (display_prefix && display_prefix_size > 0) display_prefix[0] = '\0';
    if (leaf_prefix && leaf_prefix_size > 0) leaf_prefix[0] = '\0';
    if (!query) query = "";

    const char *slash = strrchr(query, '/');
    char dir_part[CMD_BUF_MAX] = {0};
    if (slash) {
        size_t len = (size_t)(slash - query);
        if (len == 0 && query[0] == '/') len = 1;
        if (len >= sizeof(dir_part)) len = sizeof(dir_part) - 1;
        memcpy(dir_part, query, len);
        dir_part[len] = '\0';
        snprintf(display_prefix, display_prefix_size, "%.*s",
                 (int)((slash - query) + 1), query);
        snprintf(leaf_prefix, leaf_prefix_size, "%s", slash + 1);
    } else {
        snprintf(leaf_prefix, leaf_prefix_size, "%s", query);
    }

    if (query[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) home = "";
        if (slash) {
            char suffix[CMD_BUF_MAX] = {0};
            size_t len = (size_t)(slash - query);
            if (len > 1) {
                size_t suffix_len = len - 1;
                if (suffix_len >= sizeof(suffix)) suffix_len = sizeof(suffix) - 1;
                memcpy(suffix, query + 1, suffix_len);
                suffix[suffix_len] = '\0';
            }
            command_join_path(scan_dir, scan_dir_size, home, suffix);
        } else {
            snprintf(scan_dir, scan_dir_size, "%s", home);
        }
        return;
    }

    if (dir_part[0]) {
        if (command_path_is_absolute(dir_part)) {
            snprintf(scan_dir, scan_dir_size, "%s", dir_part);
        } else {
            command_resolve_workspace_path(app, dir_part, scan_dir, scan_dir_size);
        }
    } else {
        const char *root = app_get_workspace_root(app);
        snprintf(scan_dir, scan_dir_size, "%s", root && *root ? root : ".");
    }
}

static void command_complete_paths(App *app, const char *arg, bool dirs_only) {
    while (arg && *arg && isspace((unsigned char)*arg)) arg++;
    if (!arg) arg = "";

    char scan_dir[CMD_BUF_MAX];
    char display_prefix[CMD_BUF_MAX];
    char leaf_prefix[256];
    command_expand_scan_path(app, arg, scan_dir, sizeof(scan_dir),
                             display_prefix, sizeof(display_prefix),
                             leaf_prefix, sizeof(leaf_prefix));

    DIR *dir = opendir(scan_dir[0] ? scan_dir : ".");
    if (!dir) return;

    PathCompletionEntry entries[256];
    int count = 0;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL && count < 256) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        if (de->d_name[0] == '.' && leaf_prefix[0] != '.')
            continue;
        if (strncmp(de->d_name, leaf_prefix, strlen(leaf_prefix)) != 0)
            continue;

        char full[CMD_BUF_MAX];
        command_join_path(full, sizeof(full), scan_dir, de->d_name);
        struct stat st;
        bool is_dir = stat(full, &st) == 0 && S_ISDIR(st.st_mode);
        if (dirs_only && !is_dir)
            continue;

        snprintf(entries[count].name, sizeof(entries[count].name), "%s", de->d_name);
        entries[count].is_dir = is_dir;
        count++;
    }
    closedir(dir);

    qsort(entries, count, sizeof(entries[0]), path_completion_cmp);
    for (int i = 0; i < count && cmd_completion_count < CMD_COMPLETION_MAX; i++) {
        char text[CMD_BUF_MAX];
        command_join_path(text, sizeof(text), display_prefix, entries[i].name);
        if (entries[i].is_dir) {
            size_t len = strlen(text);
            if (len + 1 < sizeof(text) && (len == 0 || text[len - 1] != '/')) {
                text[len++] = '/';
                text[len] = '\0';
            }
        }
        command_completion_add(text, entries[i].is_dir ? "dir" : "file",
                               CMD_COMPLETION_PATH, entries[i].is_dir, -1);
    }
}

static const char *document_display_name(Document *doc, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return "";
    if (!doc || !doc->filepath) {
        snprintf(buf, buf_size, "[scratch]");
        return buf;
    }
    const char *slash = strrchr(doc->filepath, '/');
    snprintf(buf, buf_size, "%s", slash ? slash + 1 : doc->filepath);
    return buf;
}

static bool buffer_completion_matches(Document *doc, int index, const char *query,
                                      char *name, size_t name_size) {
    document_display_name(doc, name, name_size);
    if (!query || !*query) return true;
    char number[16];
    snprintf(number, sizeof(number), "%d", index + 1);
    if (command_completion_matches(number, query))
        return true;
    if (command_completion_matches(name, query))
        return true;
    return doc && doc->filepath && command_completion_matches(doc->filepath, query);
}

static void command_complete_buffers(App *app, const char *arg) {
    while (arg && *arg && isspace((unsigned char)*arg)) arg++;
    if (!arg) arg = "";
    int count = app_get_buffer_count(app);
    for (int i = 0; i < count && cmd_completion_count < CMD_COMPLETION_MAX; i++) {
        Document *doc = (Document *)app_get_doc_at(app, i);
        char name[CMD_BUF_MAX];
        if (!buffer_completion_matches(doc, i, arg, name, sizeof(name)))
            continue;
        char detail[CMD_COMPLETION_DETAIL_MAX];
        snprintf(detail, sizeof(detail), "buffer %d%s", i + 1,
                 doc && doc->dirty ? " modified" : "");
        command_completion_add(name, detail, CMD_COMPLETION_BUFFER, false, i);
    }
}

static int input_find_buffer(App *app, const char *arg) {
    char query[CMD_BUF_MAX];
    snprintf(query, sizeof(query), "%s", arg ? arg : "");
    char *trimmed = trim_command_arg(query);
    if (!trimmed || !*trimmed) return -1;

    char *end = NULL;
    long n = strtol(trimmed, &end, 10);
    if (end && *end == '\0' && n > 0 && n <= app_get_buffer_count(app))
        return (int)n - 1;

    int count = app_get_buffer_count(app);
    for (int i = 0; i < count; i++) {
        Document *doc = (Document *)app_get_doc_at(app, i);
        char name[CMD_BUF_MAX];
        document_display_name(doc, name, sizeof(name));
        if (strcmp(name, trimmed) == 0)
            return i;
        if (doc && doc->filepath && strcmp(doc->filepath, trimmed) == 0)
            return i;
    }
    return -1;
}

static void input_buffer_command(App *app, char *arg, bool close_buffer) {
    int index = input_find_buffer(app, arg);
    if (index < 0) {
        char query[CMD_BUF_MAX];
        snprintf(query, sizeof(query), "%s", arg ? arg : "");
        notification_push(NOTIF_ERROR, "Unknown buffer: %s", trim_command_arg(query));
        return;
    }
    if (close_buffer)
        app_close_buffer(app, index);
    else
        app_switch_to_buffer(app, index);
}

static bool plugin_completion_matches(const ConfigPlugin *plugin, const char *query) {
    if (!plugin) return false;
    if (command_completion_matches(plugin->name, query))
        return true;
    if (command_completion_matches(plugin->path, query))
        return true;
    if (plugin->path[0]) {
        const char *base = strrchr(plugin->path, '/');
        if (base && command_completion_matches(base + 1, query))
            return true;
    }
    return false;
}

static void command_completion_update(App *app) {
    cmd_completion_count = 0;
    if (cmd_completion_selected < 0)
        cmd_completion_selected = 0;

    const char *arg = NULL;
    bool completing_theme =
        cmd_has_arg_prefix(cmd_buf, "theme", &arg) ||
        cmd_has_arg_prefix(cmd_buf, "colorscheme", &arg);
    bool completing_plugin = command_is_plugin_arg(cmd_buf, &arg);
    bool dirs_only = false;
    bool completing_path = command_is_path_arg(cmd_buf, &arg, &dirs_only);
    bool completing_buffer = command_is_buffer_arg(cmd_buf, &arg);
    if (completing_theme && arg && *arg == ' ') {
        while (*arg && isspace((unsigned char)*arg)) arg++;
        const char *names[32];
        int count = theme_list_names(names, 32);
        for (int i = 0; i < count && i < 32; i++) {
            if (command_completion_matches(names[i], arg))
                command_completion_add(names[i], "theme", CMD_COMPLETION_THEME, false, -1);
        }
    } else if (completing_plugin && arg && *arg == ' ') {
        while (*arg && isspace((unsigned char)*arg)) arg++;
        Config *cfg = app_get_config(app);
        for (int i = 0; cfg && i < cfg->plugin_count; i++) {
            ConfigPlugin *plugin = &cfg->plugins[i];
            if (plugin_completion_matches(plugin, arg))
                command_completion_add(plugin->name,
                                       plugin->enabled ? "plugin enabled" : "plugin disabled",
                                       CMD_COMPLETION_PLUGIN, false, -1);
        }
    } else if (completing_path && arg && *arg == ' ') {
        command_complete_paths(app, arg, dirs_only);
    } else if (completing_buffer && arg && *arg == ' ') {
        command_complete_buffers(app, arg);
    } else {
        for (size_t i = 0; i < sizeof(static_cmds) / sizeof(static_cmds[0]); i++) {
            if (command_completion_matches(static_cmds[i].name, cmd_buf))
                command_completion_add(static_cmds[i].name, static_cmds[i].detail,
                                       CMD_COMPLETION_COMMAND, false, -1);
        }

        int count = 0;
        Command *commands = command_get_all(&count);
        for (int i = 0; commands && i < count; i++) {
            if (command_completion_matches(commands[i].name, cmd_buf))
                command_completion_add(commands[i].name, commands[i].label,
                                       CMD_COMPLETION_COMMAND, false, -1);
        }
    }

    if (cmd_completion_selected >= cmd_completion_count)
        cmd_completion_selected = cmd_completion_count > 0 ? cmd_completion_count - 1 : 0;
}

static void command_completion_accept(App *app) {
    if (cmd_completion_count <= 0)
        return;

    CmdCompletion *item = &cmd_completions[cmd_completion_selected];
    if (item->kind == CMD_COMPLETION_THEME) {
        const char *arg = NULL;
        if (cmd_has_arg_prefix(cmd_buf, "theme", &arg)) {
            snprintf(cmd_buf, sizeof(cmd_buf), "theme %s", item->name);
        } else if (cmd_has_arg_prefix(cmd_buf, "colorscheme", &arg)) {
            snprintf(cmd_buf, sizeof(cmd_buf), "colorscheme %s", item->name);
        }
    } else if (item->kind == CMD_COMPLETION_PLUGIN) {
        const char *prefix = command_plugin_prefix(cmd_buf);
        if (prefix)
            snprintf(cmd_buf, sizeof(cmd_buf), "%s %s", prefix, item->name);
    } else if (item->kind == CMD_COMPLETION_PATH) {
        const char *prefix = command_path_prefix(cmd_buf);
        if (prefix)
            snprintf(cmd_buf, sizeof(cmd_buf), "%s %s", prefix, item->name);
    } else if (item->kind == CMD_COMPLETION_BUFFER) {
        const char *prefix = command_buffer_prefix(cmd_buf);
        if (prefix)
            snprintf(cmd_buf, sizeof(cmd_buf), "%s %s", prefix, item->name);
    } else {
        snprintf(cmd_buf, sizeof(cmd_buf), "%s", item->name);
    }
    cmd_len = (int)strlen(cmd_buf);
    command_completion_update(app);
}

static char key_to_char(int key, int mods) {
    bool shift = (mods & GLFW_MOD_SHIFT) != 0;

    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
        return (char)((shift ? 'A' : 'a') + (key - GLFW_KEY_A));

    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
        static const char shifted_digits[] = ")!@#$%^&*(";
        int idx = key - GLFW_KEY_0;
        return shift ? shifted_digits[idx] : (char)('0' + idx);
    }

    switch (key) {
    case GLFW_KEY_SPACE: return ' ';
    case GLFW_KEY_APOSTROPHE: return shift ? '"' : '\'';
    case GLFW_KEY_GRAVE_ACCENT: return shift ? '~' : '`';
    case GLFW_KEY_COMMA: return shift ? '<' : ',';
    case GLFW_KEY_PERIOD: return shift ? '>' : '.';
    case GLFW_KEY_LEFT_BRACKET: return shift ? '{' : '[';
    case GLFW_KEY_RIGHT_BRACKET: return shift ? '}' : ']';
    case GLFW_KEY_BACKSLASH: return shift ? '|' : '\\';
    case GLFW_KEY_SLASH: return shift ? '?' : '/';
    case GLFW_KEY_SEMICOLON: return shift ? ':' : ';';
    case GLFW_KEY_MINUS: return shift ? '_' : '-';
    case GLFW_KEY_EQUAL: return shift ? '+' : '=';
    default: return 0;
    }
}

static char key_to_object_char(int key, int mods) {
    char c = key_to_char(key, mods);
    if (c >= 'A' && c <= 'Z')
        c = (char)(c - 'A' + 'a');
    return c;
}

static void record_last_insert_char(ModeState *mode, char c, int cursor_delta) {
    if (mode->last_insert_len < (int)sizeof(mode->last_insert_text) - 1) {
        mode->last_insert_text[mode->last_insert_len++] = c;
        mode->last_insert_text[mode->last_insert_len] = '\0';
        mode->last_insert_cursor_delta += cursor_delta;
    }
}

static void insert_backspace(Document *doc, ModeState *mode) {
    if (doc->cursor_count > 1)
        document_delete_char_multi(doc);
    else
        document_delete_char(doc);
    if (mode->last_insert_len > 0) {
        mode->last_insert_len--;
        mode->last_insert_text[mode->last_insert_len] = '\0';
        mode->last_insert_cursor_delta--;
    }
}

static void insert_newline(Document *doc, ModeState *mode) {
    if (doc->cursor_count > 1)
        document_newline_multi(doc);
    else
        document_newline(doc);
    record_last_insert_char(mode, '\n', -mode->last_insert_cursor_delta);
}

static void insert_tab(Document *doc, ModeState *mode) {
    if (doc->cursor_count > 1)
        document_insert_char_multi(doc, '\t');
    else
        document_insert_char(doc, '\t');
    record_last_insert_char(mode, '\t', 1);
}

static void handle_normal_key(App *app, int key, int action, int mods);
static void handle_insert_key(App *app, int key, int action, int mods);
static void handle_select_key(App *app, int key, int action, int mods);
static void handle_command_key(App *app, int key, int action, int mods);

static bool handle_window_key(App *app, int key, int mods) {
    (void)mods;
    switch (key) {
    case GLFW_KEY_W:
        app_next_window(app);
        return true;
    case GLFW_KEY_V:
        app_split_vertical(app);
        return true;
    case GLFW_KEY_S:
        app_split_horizontal(app);
        return true;
    case GLFW_KEY_Q:
        app_close_split(app);
        return true;
    case GLFW_KEY_O:
        app_maximize_window(app);
        return true;
    case GLFW_KEY_H:
        if (mods & GLFW_MOD_SHIFT) {
            app_swap_window_left(app);
            return true;
        }
        app_goto_window_left(app);
        return true;
    case GLFW_KEY_LEFT:
        app_goto_window_left(app);
        return true;
    case GLFW_KEY_L:
        if (mods & GLFW_MOD_SHIFT) {
            app_swap_window_right(app);
            return true;
        }
        app_goto_window_right(app);
        return true;
    case GLFW_KEY_RIGHT:
        app_goto_window_right(app);
        return true;
    case GLFW_KEY_K:
        if (mods & GLFW_MOD_SHIFT) {
            app_swap_window_up(app);
            return true;
        }
        app_goto_window_up(app);
        return true;
    case GLFW_KEY_UP:
        app_goto_window_up(app);
        return true;
    case GLFW_KEY_J:
        if (mods & GLFW_MOD_SHIFT) {
            app_swap_window_down(app);
            return true;
        }
        app_goto_window_down(app);
        return true;
    case GLFW_KEY_DOWN:
        app_goto_window_down(app);
        return true;
    case GLFW_KEY_EQUAL:
        app_equalize_windows(app);
        return true;
    default:
        return false;
    }
}

void input_handle_key(App *app, int key, int scancode, int action, int mods) {
    (void)scancode;
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    ModeState *mode = (ModeState *)app_get_mode(app);

    if (key == GLFW_KEY_GRAVE_ACCENT && (mods & GLFW_MOD_CONTROL)) {
        if (panel_terminal_is_open())
            panel_terminal_close(app);
        else
            panel_terminal_open(app);
        return;
    }

    /* Global escape */
    if (key == GLFW_KEY_ESCAPE) {
        if (panel_terminal_is_open()) { panel_terminal_close(app); return; }
        if (panel_file_browser_is_open()) { panel_file_browser_close(app); return; }
        if (panel_find_is_open()) { panel_find_close(app); return; }
        if (panel_goto_is_open()) { panel_goto_close(app); return; }
        if (panel_about_is_open()) { panel_about_close(app); return; }
        if (panel_buffer_picker_is_open()) { panel_buffer_picker_close(app); return; }
        if (panel_jumplist_picker_is_open()) { panel_jumplist_picker_close(app); return; }
        if (panel_changed_files_is_open()) { panel_changed_files_close(app); return; }
        if (panel_lsp_goto_is_open()) { panel_lsp_goto_close(app); return; }
        if (panel_lsp_diagnostics_is_open()) { panel_lsp_diagnostics_close(app); return; }
        if (panel_lsp_hover_is_open()) { panel_lsp_hover_close(app); return; }
        if (panel_symbols_picker_is_open()) { panel_symbols_picker_close(app); return; }
        if (panel_rename_is_open()) { panel_rename_close(app); return; }
        if (panel_code_actions_is_open()) { panel_code_actions_close(app); return; }
        if (panel_space_menu_is_open()) { panel_space_menu_close(app); return; }
        if (panel_palette_is_open()) { panel_palette_close(app); return; }
        if (panel_settings_is_open()) { panel_settings_close(app); return; }
        if (panel_plugins_is_open()) { panel_plugins_close(app); return; }
        if (panel_treesitter_inspector_is_open()) { panel_treesitter_inspector_close(app); return; }
        if (panel_workspace_symbols_is_open()) { panel_workspace_symbols_close(app); return; }
        if (panel_workspace_diagnostics_is_open()) { panel_workspace_diagnostics_close(app); return; }
        if (panel_completion_is_open()) { panel_completion_close(app); return; }
        if (!mode_is(mode, MODE_NORMAL)) {
            if (mode_is(mode, MODE_INSERT)) {
                Document *doc = (Document *)app_get_doc(app);
                Cursor *cur = &doc->cursors[0];
                document_jumplist_push(doc, cur->row, cur->col);
            }
            input_cmd_reset();
            mode_set(mode, MODE_NORMAL);
            return;
        }
        /* Exit sticky view mode on Escape */
        if (mode->view_mode_sticky) {
            mode->view_mode_sticky = false;
            mode->pending_key = 0;
            return;
        }
    }

    if (panel_terminal_is_open()) {
        panel_terminal_key(app, key, mods);
        return;
    }

    /* Route keys to find panel when open */
    if (panel_find_is_open()) {
        Document *doc = (Document *)app_get_doc(app);
        panel_find_key(app, doc, key);
        return;
    }

    /* Route keys to file browser when open */
    if (panel_file_browser_is_open()) {
        panel_file_browser_key(app, key);
        return;
    }

    if (panel_settings_is_open()) {
        panel_settings_key(app, key);
        return;
    }

    if (panel_plugins_is_open()) {
        panel_plugins_key(app, key);
        return;
    }

    if (panel_treesitter_inspector_is_open()) {
        panel_treesitter_inspector_key(app, key);
        return;
    }

    if (panel_workspace_symbols_is_open()) {
        panel_workspace_symbols_key(app, key);
        return;
    }

    if (panel_workspace_diagnostics_is_open()) {
        panel_workspace_diagnostics_key(app, key);
        return;
    }

    if (panel_changed_files_is_open()) {
        panel_changed_files_key(app, key);
        return;
    }

    if (panel_completion_is_open()) {
        if (panel_completion_key(app, key, mods))
            return;
    }

    /* Route keys to buffer picker when open */
    if (panel_buffer_picker_is_open()) {
        panel_buffer_picker_key(app, key);
        return;
    }

    /* Route keys to jumplist picker when open */
    if (panel_jumplist_picker_is_open()) {
        panel_jumplist_picker_key(app, key);
        return;
    }

    if (panel_lsp_hover_is_open()) {
        if (panel_lsp_hover_key(app, key, mods))
            return;
    }

    /* Route keys to LSP goto picker when open */
    if (panel_lsp_goto_is_open()) {
        panel_lsp_goto_key(app, key);
        return;
    }
    
    /* Route keys to LSP diagnostics picker when open */
    if (panel_lsp_diagnostics_is_open()) {
        panel_lsp_diagnostics_key(app, key);
        return;
    }
    
    /* Route keys to symbols picker when open */
    if (panel_symbols_picker_is_open()) {
        panel_symbols_picker_key(app, key);
        return;
    }
    
    /* Route keys to rename panel when open */
    if (panel_rename_is_open()) {
        panel_rename_key(app, key);
        return;
    }
    
    /* Route keys to code actions panel when open */
    if (panel_code_actions_is_open()) {
        panel_code_actions_key(app, key);
        return;
    }
    
    /* Route keys to space menu when open */
    if (panel_space_menu_is_open()) {
        panel_space_menu_key(app, key);
        return;
    }
    
    /* Route keys to palette when open */
    if (panel_palette_is_open()) {
        panel_palette_key(app, key);
        return;
    }

    switch (mode_get(mode)) {
    case MODE_NORMAL:          handle_normal_key(app, key, action, mods); break;
    case MODE_INSERT:          handle_insert_key(app, key, action, mods); break;
    case MODE_SELECT:          handle_select_key(app, key, action, mods); break;
    case MODE_COMMAND:         handle_command_key(app, key, action, mods); break;
    default: break;
    }
}

void input_handle_char(App *app, unsigned int c) {
    ModeState *mode = (ModeState *)app_get_mode(app);
    
    if (panel_terminal_is_open()) {
        if (mode->suppress_next_char) {
            mode->suppress_next_char = false;
            return;
        }
        panel_terminal_input(app, c);
        return;
    }

    if (panel_completion_is_open()) {
        panel_completion_close(app);
    }
    if (panel_find_is_open()) {
        Document *doc = (Document *)app_get_doc(app);
        panel_find_input(app, doc, c);
        return;
    }
    if (panel_file_browser_is_open()) {
        panel_file_browser_input(app, c);
        return;
    }
    if (panel_space_menu_is_open()) {
        panel_space_menu_input(app, c);
        return;
    }
    if (panel_palette_is_open()) {
        panel_palette_input(app, c);
        return;
    }
    
    /* Suppress character if mode was just changed (prevents 'i' from inserting 'i') */
    if (mode->suppress_next_char) {
        mode->suppress_next_char = false;
        return;
    }

    if ((mode_is(mode, MODE_NORMAL) || mode_is(mode, MODE_SELECT)) && c == '~') {
        Document *doc = (Document *)app_get_doc(app);
        document_toggle_case(doc);
        return;
    }
    
    if (mode_is(mode, MODE_INSERT)) {
        if (c >= 32 && c < 127) {
            Document *doc = (Document *)app_get_doc(app);
            if (doc->cursor_count > 1)
                document_insert_char_multi(doc, (char)c);
            else
                document_insert_char(doc, (char)c);
            if (mode->last_insert_len < 4095) {
                mode->last_insert_text[mode->last_insert_len++] = (char)c;
                mode->last_insert_text[mode->last_insert_len] = '\0';
                mode->last_insert_cursor_delta++;
            }
        }
    } else if (mode_is(mode, MODE_COMMAND)) {
        if (c >= 32 && c < 127 && cmd_len < CMD_BUF_MAX - 1 && c != ':') {
            cmd_buf[cmd_len++] = (char)c;
            cmd_buf[cmd_len] = '\0';
            command_completion_update(app);
        }
    }
}

static void handle_normal_key(App *app, int key, int action, int mods) {
    (void)action;
    Document *doc = (Document *)app_get_doc(app);
    ModeState *mode = (ModeState *)app_get_mode(app);

    if ((mods & GLFW_MOD_ALT) && (key == GLFW_KEY_O || key == GLFW_KEY_UP)) {
        document_select_treesitter_parent(doc, app_get_treesitter_manager(app));
        return;
    }
    if ((mods & GLFW_MOD_ALT) && (mods & GLFW_MOD_SHIFT) &&
        (key == GLFW_KEY_I || key == GLFW_KEY_DOWN)) {
        document_select_treesitter_all_children(doc, app_get_treesitter_manager(app));
        return;
    }
    if ((mods & GLFW_MOD_ALT) && (key == GLFW_KEY_I || key == GLFW_KEY_DOWN)) {
        document_select_treesitter_child(doc, app_get_treesitter_manager(app));
        return;
    }
    if ((mods & GLFW_MOD_ALT) && (key == GLFW_KEY_P || key == GLFW_KEY_LEFT)) {
        document_select_treesitter_sibling(doc, app_get_treesitter_manager(app), -1);
        return;
    }
    if ((mods & GLFW_MOD_ALT) && (key == GLFW_KEY_N || key == GLFW_KEY_RIGHT)) {
        document_select_treesitter_sibling(doc, app_get_treesitter_manager(app), 1);
        return;
    }
    if ((mods & GLFW_MOD_ALT) && key == GLFW_KEY_A) {
        document_select_treesitter_all_siblings(doc, app_get_treesitter_manager(app));
        return;
    }
    if ((mods & GLFW_MOD_ALT) && key == GLFW_KEY_E) {
        document_move_to_treesitter_parent_edge(doc, app_get_treesitter_manager(app), true);
        return;
    }
    if ((mods & GLFW_MOD_ALT) && key == GLFW_KEY_B) {
        document_move_to_treesitter_parent_edge(doc, app_get_treesitter_manager(app), false);
        return;
    }

    /* Count accumulation for digit prefix */
    if (!(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT) && !(mods & GLFW_MOD_SHIFT)) {
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
            mode->count = mode->count * 10 + (key - GLFW_KEY_0);
            return;
        }
        if (key == GLFW_KEY_0 && mode->count > 0) {
            mode->count = mode->count * 10;
            return;
        }
    }

    /* . (period) - Repeat last insert */
    if (key == GLFW_KEY_PERIOD && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        if (mode->has_last_insert && mode->last_insert_len > 0) {
            Cursor *cur = &doc->cursors[0];
            size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
            buffer_insert(&doc->buffer, pos, mode->last_insert_text, mode->last_insert_len);
            history_push_insert(&doc->history, pos, mode->last_insert_text, mode->last_insert_len,
                                cur->row, cur->col);
            cur->col += mode->last_insert_cursor_delta;
            document_mark_dirty(doc);
        }
        mode->count = 0;
        return;
    }

    /* Get repeat count (default 1) */
    bool had_count = mode->count > 0;
    int count = had_count ? mode->count : 1;
    mode->count = 0;

    /* Pending key handling */
    if (mode->pending_key) {
        char pk = mode->pending_key;
        mode->pending_key = 0;

        if (pk == 'w') {
            handle_window_key(app, key, mods);
            return;
        }

        if (pk == 'r') {
            char c = key_to_char(key, mods);
            if (c) document_replace_selection_char(doc, c);
            return;
        }
        if (pk == 'f') {
            char c = key_to_char(key, mods);
            if (c) {
                document_find_char_forward(doc, c);
                mode->last_motion_type = 'f';
                mode->last_motion_char = c;
            }
            return;
        }
        if (pk == 'F') {
            char c = key_to_char(key, mods);
            if (c) {
                document_find_char_backward(doc, c);
                mode->last_motion_type = 'F';
                mode->last_motion_char = c;
            }
            return;
        }
        if (pk == 't') {
            char c = key_to_char(key, mods);
            if (c) {
                document_till_char_forward(doc, c);
                mode->last_motion_type = 't';
                mode->last_motion_char = c;
            }
            return;
        }
        if (pk == 'T') {
            char c = key_to_char(key, mods);
            if (c) {
                document_till_char_backward(doc, c);
                mode->last_motion_type = 'T';
                mode->last_motion_char = c;
            }
            return;
        }
        if (pk == 'z') {
            bool handled = false;
            if (key == GLFW_KEY_Z) { document_scroll_center(doc); handled = true; }
            else if (key == GLFW_KEY_C) { document_scroll_center(doc); handled = true; }
            else if (key == GLFW_KEY_M) { 
                /* zm - horizontally center/middle align cursor */
                document_scroll_horizontal_center(doc); 
                handled = true; 
            }
            else if (key == GLFW_KEY_T) { document_scroll_top(doc, doc->viewport_lines); handled = true; }
            else if (key == GLFW_KEY_B && !(mods & GLFW_MOD_CONTROL)) { document_scroll_bottom(doc, doc->viewport_lines); handled = true; }
            else if (key == GLFW_KEY_J) { document_scroll_down(doc); handled = true; }
            else if (key == GLFW_KEY_K) { document_scroll_up(doc); handled = true; }
            else if (key == GLFW_KEY_F) { document_view_page_down(doc); handled = true; }
            else if (key == GLFW_KEY_B && (mods & GLFW_MOD_CONTROL)) { document_view_page_up(doc); handled = true; }
            else if (key == GLFW_KEY_D) { document_view_half_page_down(doc); handled = true; }
            else if (key == GLFW_KEY_U) { document_view_half_page_up(doc); handled = true; }
            
            /* If not sticky, clear pending_key after command */
            if (handled && !mode->view_mode_sticky) {
                mode->pending_key = 0;
            }
            return;
        }
        if (pk == 'm') {
            if (key == GLFW_KEY_S) {
                mode->pending_key = 's';
                mode->pending_keys[0] = 's';
                mode->pending_len = 1;
                return;
            }
            if (key == GLFW_KEY_D) {
                mode->pending_key = 'd';
                mode->pending_keys[0] = 'd';
                mode->pending_len = 1;
                return;
            }
            if (key == GLFW_KEY_R) {
                mode->pending_key = 'r';
                mode->pending_keys[0] = 'r';
                mode->pending_len = 1;
                return;
            }
            if (key == GLFW_KEY_I && !(mods & GLFW_MOD_SHIFT)) {
                mode->pending_text_obj = 'i';
                mode->pending_key = 'i';
                return;
            }
            if (key == GLFW_KEY_A && !(mods & GLFW_MOD_SHIFT)) {
                mode->pending_text_obj = 'a';
                mode->pending_key = 'i';
                return;
            }
            if (key == GLFW_KEY_M) {
                /* mm = go to matching bracket */
                document_match_bracket(doc);
                return;
            }
            return;
        }
        if (pk == 's' && mode->pending_len == 1 && mode->pending_keys[0] == 's') {
            /* ms<char> = surround with char */
            char c = key_to_char(key, mods);
            if (c) document_surround(doc, c);
            mode->pending_len = 0;
            return;
        }
        if (pk == 'd' && mode->pending_len == 1 && mode->pending_keys[0] == 'd') {
            /* md<char> = delete surrounding char */
            char c = key_to_char(key, mods);
            if (c) document_delete_surround(doc, c);
            mode->pending_len = 0;
            return;
        }
        if (pk == 'r' && mode->pending_len == 1 && mode->pending_keys[0] == 'r') {
            /* mr<from><to> = replace surrounding delimiter */
            char from = key_to_char(key, mods);
            if (from) {
                mode->pending_key = 'r';
                mode->pending_keys[0] = 'r';
                mode->pending_keys[1] = from;
                mode->pending_len = 2;
                return;
            }
            mode->pending_len = 0;
            return;
        }
        if (pk == 'r' && mode->pending_len == 2 && mode->pending_keys[0] == 'r') {
            /* mr<from><to> second char */
            char from = mode->pending_keys[1];
            char to = key_to_char(key, mods);
            if (to) document_replace_surround(doc, from, to);
            mode->pending_len = 0;
            return;
        }
        if (pk == 'i') {
            /* Text object: i<a/w/(/)/[/]/{/}/</>/< /"/'/`/p> */
            char obj = mode->pending_text_obj;  /* 'i' or 'a' */
            bool inner = (obj == 'i');
            char c = key_to_object_char(key, mods);

            if (c) {
                /* Ensure cursor has a selection anchor */
                Cursor *cur = &doc->cursors[0];
                if (!cur->has_selection) cursor_select_start(cur);

                switch (c) {
                case 'w': inner ? document_select_inside_word(doc) : document_select_around_word(doc); break;
                case '(': case ')': inner ? document_select_inside_paren(doc) : document_select_around_paren(doc); break;
                case '[': case ']': inner ? document_select_inside_bracket(doc) : document_select_around_bracket(doc); break;
                case '{': case '}': inner ? document_select_inside_curly(doc) : document_select_around_curly(doc); break;
                case '<': case '>': inner ? document_select_inside_angle(doc) : document_select_around_angle(doc); break;
                case '"': case '\'': inner ? document_select_inside_quote(doc) : document_select_around_quote(doc); break;
                case '`': inner ? document_select_inside_backtick(doc) : document_select_around_backtick(doc); break;
                case 'p': inner ? document_select_inside_paragraph(doc) : document_select_around_paragraph(doc); break;
                default: break;
                }

                /* Apply pending operator if any */
                if (mode->pending_operator) {
                    char op = mode->pending_operator;
                    mode->pending_operator = 0;
                    if (op == 'd') {
                        document_delete_selection(doc);
                    } else if (op == 'c') {
                        document_delete_selection(doc);
                        mode_set(mode, MODE_INSERT);
                    } else if (op == 'y') {
                        document_yank(doc);
                        document_collapse_selection(doc);
                    }
                } else {
                    /* No operator: enter select mode with the selection */
                    mode_set(mode, MODE_SELECT);
                }
            } else {
                mode->pending_operator = 0;
            }
            return;
        }
        if (pk == '[') {
            /* [ bracket mode - navigate to previous */
            if (key == GLFW_KEY_D && (mods & GLFW_MOD_SHIFT)) {
                /* [D - first diagnostic */
                extern void document_goto_first_diagnostic(Document *);
                document_goto_first_diagnostic(doc);
                return;
            }
            if (key == GLFW_KEY_D) {
                /* [d - previous diagnostic */
                extern void document_goto_prev_diagnostic(Document *);
                document_goto_prev_diagnostic(doc);
                return;
            }
            return;
        }
        if (pk == 'q') {
            /* q<letter> - start recording macro to register */
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
                int slot = key - GLFW_KEY_A;
                macro_start_record(&doc->macros, slot);
            }
            return;
        }
        if (pk == '@') {
            /* @<letter> - replay macro */
            if (mode->pending_len == 1 && mode->pending_keys[0] == '@') {
                /* @@ - replay last macro: find last recorded */
                macro_replay(&doc->macros, -1);
                mode->pending_len = 0;
                return;
            }
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
                int slot = key - GLFW_KEY_A;
                macro_replay(&doc->macros, slot);
            }
            return;
        }
        if (pk == ']') {
            /* ] bracket mode - navigate to next */
            if (key == GLFW_KEY_D && (mods & GLFW_MOD_SHIFT)) {
                /* ]D - last diagnostic */
                extern void document_goto_last_diagnostic(Document *);
                document_goto_last_diagnostic(doc);
                return;
            }
            if (key == GLFW_KEY_D) {
                /* ]d - next diagnostic */
                extern void document_goto_next_diagnostic(Document *);
                document_goto_next_diagnostic(doc);
                return;
            }
            return;
        }
        if (pk == ' ') {
            /* Standalone modifier presses keep the menu open */
            if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT ||
                key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL ||
                key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT ||
                key == GLFW_KEY_LEFT_SUPER || key == GLFW_KEY_RIGHT_SUPER) {
                mode->pending_key = ' ';
                return;
            }

            /* Space mode sub-commands - close menu after command */
            panel_space_menu_close(app);
            
            if (key == GLFW_KEY_F) {
                /* Space F - file browser at current directory */
                if (mods & GLFW_MOD_SHIFT) {
                    panel_file_browser_open(app);
                } else {
                    /* Space f - file browser at workspace root */
                    const char *workspace = app_get_workspace_root(app);
                    extern void panel_file_browser_open_at(App *, const char *);
                    panel_file_browser_open_at(app, workspace);
                }
                mode->pending_len = 0;
                return;
            }
            if (key == GLFW_KEY_B) {
                /* Space b - buffer picker */
                panel_buffer_picker_open(app);
                mode->pending_len = 0;
                return;
            }
            if (key == GLFW_KEY_J) {
                /* Space j - jumplist picker */
                panel_jumplist_picker_open(app);
                mode->pending_len = 0;
                return;
            }
            if (key == GLFW_KEY_G) {
                /* Space g - changed-file picker */
                panel_changed_files_open(app);
                mode->pending_len = 0;
                return;
            }
             if (key == GLFW_KEY_C) {
                 document_comment_toggle(doc);
                 mode->pending_len = 0;
                 return;
             }
             if (key == GLFW_KEY_K) {
                 /* Space k - hover documentation */
                 panel_lsp_hover_request(app);
                 mode->pending_len = 0;
                 return;
             }
              if (key == GLFW_KEY_D) {
                  if (mods & GLFW_MOD_SHIFT) {
                      panel_workspace_diagnostics_open(app);
                      mode->pending_len = 0;
                      return;
                  }
                  /* Space d / D - diagnostics picker */
                  panel_lsp_diagnostics_open(app);
                  mode->pending_len = 0;
                  return;
              }
              if (key == GLFW_KEY_S) {
                  if (mods & GLFW_MOD_SHIFT) {
                      panel_workspace_symbols_open(app);
                      mode->pending_len = 0;
                      return;
                  }
                  /* Space s / S - symbols picker */
                  panel_symbols_picker_open(app);
                  mode->pending_len = 0;
                  return;
              }
              if (key == GLFW_KEY_H) {
                  /* Space h - select references under cursor */
                  app_lsp_select_references(app);
                  mode->pending_len = 0;
                  return;
              }
              if (key == GLFW_KEY_T) {
                  if (mods & GLFW_MOD_SHIFT) {
                      /* Space T - terminal panel */
                      panel_terminal_open(app);
                      mode->suppress_next_char = true;
                  } else {
                      /* Space t - tree-sitter node inspector */
                      panel_treesitter_inspector_open(app);
                  }
                  mode->pending_len = 0;
                  return;
              }
              if (key == GLFW_KEY_A) {
                  /* Space a - code actions */
                  panel_code_actions_open(app);
                  mode->pending_len = 0;
                  return;
              }
              if (key == GLFW_KEY_Y) {
                  /* Space y - yank selection to system clipboard */
                  if (mods & GLFW_MOD_SHIFT) {
                      /* Space Y - yank main selection to system clipboard */
                      document_yank_main_to_system_clipboard(doc);
                  } else {
                      document_yank_to_system_clipboard(doc);
                  }
                  mode->pending_len = 0;
                  return;
              }
              if (key == GLFW_KEY_P) {
                  if (mods & GLFW_MOD_SHIFT) {
                      /* Space P - paste before from system clipboard */
                      document_paste_before_from_system_clipboard(doc);
                  } else {
                      /* Space p - paste from system clipboard after cursor */
                      document_paste_from_system_clipboard(doc);
                  }
                  mode->pending_len = 0;
                  return;
              }
              if (key == GLFW_KEY_O) {
                  /* Space o - file picker at $HOME */
                  extern void panel_file_browser_open_at_home(App *);
                  panel_file_browser_open_at_home(app);
                  mode->pending_len = 0;
                  return;
              }
              if (key == GLFW_KEY_R) {
                  if (mods & GLFW_MOD_SHIFT) {
                      /* Space R - replace selection with system clipboard */
                      document_replace_selection_from_system_clipboard(doc);
                  } else {
                      /* Space r - rename symbol */
                      panel_rename_open(app);
                  }
                  mode->pending_len = 0;
                  return;
              }
              mode->pending_len = 0;
              return;
        }
        return;
    }
    if (mode->g_pending) {
        mode->g_pending = false;
        if (key == GLFW_KEY_G)
            document_cursor_doc_start(doc);
        else if (key == GLFW_KEY_E)
            document_cursor_doc_end(doc);
        else if (key == GLFW_KEY_S)
            document_cursor_first_non_blank(doc);
        else if (key == GLFW_KEY_H)
            document_goto_line_start(doc);
        else if (key == GLFW_KEY_L)
            document_goto_line_end(doc);
        else if (key == GLFW_KEY_T)
            document_goto_view_top(doc);
        else if (key == GLFW_KEY_C)
            document_goto_view_center(doc);
        else if (key == GLFW_KEY_B)
            document_goto_view_bottom(doc);
        else if (key == GLFW_KEY_F)
            document_go_to_file(doc);
        else if (key == GLFW_KEY_J)
            document_move_cursor(doc, 1, 0);
        else if (key == GLFW_KEY_K)
            document_move_cursor(doc, -1, 0);
        else if (key == GLFW_KEY_N)
            app_next_buffer(app); /* gn - go to next buffer */
        else if (key == GLFW_KEY_P)
            app_prev_buffer(app); /* gp - go to previous buffer */
        else if (key == GLFW_KEY_D) {
            /* gd - go to definition (LSP) */
            app_lsp_request_goto(app, APP_LSP_GOTO_DEFINITION);
        }
        else if (key == GLFW_KEY_Y) {
            /* gy - go to type definition (LSP) */
            app_lsp_request_goto(app, APP_LSP_GOTO_TYPE_DEFINITION);
        }
        else if (key == GLFW_KEY_R) {
            /* gr - go to references (LSP) */
            app_lsp_request_goto(app, APP_LSP_GOTO_REFERENCES);
        }
        else if (key == GLFW_KEY_I) {
            /* gi - go to implementation (LSP) */
            app_lsp_request_goto(app, APP_LSP_GOTO_IMPLEMENTATION);
        }
        else if (key == GLFW_KEY_PERIOD)
            document_goto_last_modification(doc); /* g. - go to last modification */
        else if (key == GLFW_KEY_BACKSLASH) {
            /* g| - go to column number (count required) */
            if (had_count) {
                Cursor *cur = &doc->cursors[0];
                int target = count - 1;
                int len = (int)buffer_line_len(&doc->buffer, cur->row);
                cur->col = target < len ? target : len;
            }
        }
        return;
    }

    /* Alt combinations */
    if (mods & GLFW_MOD_ALT) {
        /* Alt-C - copy selection above / add cursor above */
        if (key == GLFW_KEY_C && (mods & GLFW_MOD_SHIFT)) {
            document_copy_selection_above(doc);
            return;
        }
        switch (key) {
        case GLFW_KEY_SEMICOLON:
            if (mods & GLFW_MOD_SHIFT)
                document_force_selection_forward(doc);
            else
                document_flip_cursor_anchor(doc);
            return;
        case GLFW_KEY_D:
            document_delete_selection(doc);
            return;
        case GLFW_KEY_C:
            document_delete_selection(doc);
            mode_set(mode, MODE_INSERT);
            return;
        case GLFW_KEY_S:
            document_split_selection_newlines(doc);
            return;
        case GLFW_KEY_MINUS:
            if (mods & GLFW_MOD_SHIFT)
                document_merge_consecutive_selections(doc);
            else
                document_merge_selections(doc);
            return;
        case GLFW_KEY_J:
            document_join_lines_with_space(doc);
            return;
        case GLFW_KEY_X:
            document_shrink_to_line_bounds(doc);
            return;
        case GLFW_KEY_GRAVE_ACCENT:
            document_uppercase(doc);
            return;
        case GLFW_KEY_PERIOD:
            /* Alt-. - Repeat last motion */
            if (mode->last_motion_type) {
                char c = mode->last_motion_char;
                switch (mode->last_motion_type) {
                case 'f': document_find_char_forward(doc, c); break;
                case 'F': document_find_char_backward(doc, c); break;
                case 't': document_till_char_forward(doc, c); break;
                case 'T': document_till_char_backward(doc, c); break;
                }
            }
            return;
        case GLFW_KEY_9:
            /* Alt-( - Rotate selection contents backward */
            if (mods & GLFW_MOD_SHIFT) {
                document_rotate_selection_contents_backward(doc);
            }
            return;
        case GLFW_KEY_0:
            /* Alt-) - Rotate selection contents forward */
            if (mods & GLFW_MOD_SHIFT) {
                document_rotate_selection_contents_forward(doc);
            }
            return;
        case GLFW_KEY_U:
            /* Alt-U - Move forward in history tree (redo) */
            if (mods & GLFW_MOD_SHIFT) {
                document_redo(doc);
            } else {
                /* Alt-u - Move backward in history tree (undo) */
                document_undo(doc);
            }
            return;
        default: break;
        }
    }

    /* C - copy selection below / add cursor below */
    if (key == GLFW_KEY_C && (mods & GLFW_MOD_SHIFT) &&
        !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        document_copy_selection_below(doc);
        return;
    }

    /* Ctrl combinations */
    if (mods & GLFW_MOD_CONTROL) {
        switch (key) {
        case GLFW_KEY_Z:
            if (mods & GLFW_MOD_SHIFT)
                document_redo(doc);
            else
                document_undo(doc);
            return;
        case GLFW_KEY_Y:
            document_redo(doc);
            return;
        case GLFW_KEY_D:
            document_half_page_down(doc, doc->viewport_lines);
            return;
        case GLFW_KEY_U:
            document_half_page_up(doc, doc->viewport_lines);
            return;
        case GLFW_KEY_F:
            document_cursor_page_down(doc);
            return;
        case GLFW_KEY_B:
            document_cursor_page_up(doc);
            return;
        case GLFW_KEY_C:
            document_comment_toggle(doc);
            return;
        case GLFW_KEY_A:
            for (int i = 0; i < count; i++) document_increment_number(doc);
            return;
        case GLFW_KEY_X:
            for (int i = 0; i < count; i++) document_decrement_number(doc);
            return;
        case GLFW_KEY_O:
            document_jumplist_backward(doc);
            return;
        case GLFW_KEY_I:
            document_jumplist_forward(doc);
            return;
        case GLFW_KEY_S: {
            Cursor *cur = &doc->cursors[0];
            document_jumplist_push(doc, cur->row, cur->col);
            return;
        }
        case GLFW_KEY_W:
            mode->pending_key = 'w';
            return;
        case GLFW_KEY_6:
            /* Ctrl-^ - go to alternate file */
            if (mods & GLFW_MOD_SHIFT) {
                document_goto_alternate(doc);
            }
            return;
        default: break;
        }
    }

    /* W/B/E (Shift) - WORD motions */
    if (key == GLFW_KEY_W && (mods & GLFW_MOD_SHIFT)) {
        for (int i = 0; i < count; i++) document_cursor_WORD_forward(doc);
        return;
    }
    if (key == GLFW_KEY_B && (mods & GLFW_MOD_SHIFT)) {
        for (int i = 0; i < count; i++) document_cursor_WORD_backward(doc);
        return;
    }
    if (key == GLFW_KEY_E && (mods & GLFW_MOD_SHIFT)) {
        for (int i = 0; i < count; i++) document_cursor_WORD_end(doc);
        return;
    }

    /* : (Shift+semicolon) - Command mode */
    if (key == GLFW_KEY_SEMICOLON && (mods & GLFW_MOD_SHIFT) &&
        !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        mode_set(mode, MODE_COMMAND);
        input_cmd_reset();
        command_completion_update(app);
        return;
    }

    /* ? (Shift+/) - Search backward */
    if (key == GLFW_KEY_SLASH && (mods & GLFW_MOD_SHIFT)) {
        Document *doc2 = (Document *)app_get_doc(app);
        panel_find_open_backward(app, doc2);
        return;
    }

    /* * (Shift+8) - Use selection/word as search pattern */
    if (key == GLFW_KEY_8 && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_ALT)) {
        document_search_word(doc, true);
        return;
    }

    /* Alt-* - Search without word boundaries */
    if (key == GLFW_KEY_8 && (mods & GLFW_MOD_SHIFT) && (mods & GLFW_MOD_ALT)) {
        document_search_word(doc, false);
        return;
    }


    /* ( ) - Rotate selections */
    if (key == GLFW_KEY_9 && !(mods & GLFW_MOD_SHIFT)) {
        document_rotate_selections_backward(doc);
        return;
    }
    if (key == GLFW_KEY_0 && (mods & GLFW_MOD_SHIFT)) {
        document_rotate_selections_forward(doc);
        return;
    }

    /* G - Go to line number or end of file */
    if (key == GLFW_KEY_G && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL)) {
        if (had_count)
            mode->count = count;
        mode->g_pending = true;
        return;
    }

    /* Shift+G - Go to end of file */
    if (key == GLFW_KEY_G && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL)) {
        if (had_count)
            document_cursor_to(doc, count - 1, 0);
        else
            document_cursor_doc_end(doc);
        return;
    }

    /* [ - Bracket mode (navigate to previous) */
    if (key == GLFW_KEY_LEFT_BRACKET && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        mode->pending_key = '[';
        return;
    }

    /* ] - Bracket mode (navigate to next) */
    if (key == GLFW_KEY_RIGHT_BRACKET && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        mode->pending_key = ']';
        return;
    }

    /* P - Paste before */
    if (key == GLFW_KEY_P && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL)) {
        document_paste_before(doc);
        return;
    }

    /* o / O - Open line below/above and enter insert */
    if (key == GLFW_KEY_O && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL)) {
        document_open_line_below(doc);
        mode_set(mode, MODE_INSERT);
        return;
    }
    if (key == GLFW_KEY_O && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL)) {
        document_open_line_above(doc);
        mode_set(mode, MODE_INSERT);
        return;
    }

    /* I - Insert at line start */
    if (key == GLFW_KEY_I && (mods & GLFW_MOD_SHIFT)) {
        document_cursor_home(doc);
        mode_set(mode, MODE_INSERT);
        return;
    }

    /* A - Insert at line end */
    if (key == GLFW_KEY_A && (mods & GLFW_MOD_SHIFT)) {
        Cursor *cur = &doc->cursors[0];
        cur->col = (int)buffer_line_len(&doc->buffer, cur->row);
        mode_set(mode, MODE_INSERT);
        return;
    }

    /* Alt-d - Delete selection without yanking */
    if (key == GLFW_KEY_D && (mods & GLFW_MOD_ALT)) {
        document_delete_selection(doc);
        return;
    }

    /* Alt-c - Change selection without yanking */
    if (key == GLFW_KEY_C && (mods & GLFW_MOD_ALT) && !(mods & GLFW_MOD_SHIFT)) {
        document_delete_selection(doc);
        mode_set(mode, MODE_INSERT);
        return;
    }

    /* > (Shift+.) - Indent selection */
    if (key == GLFW_KEY_PERIOD && (mods & GLFW_MOD_SHIFT)) {
        document_indent_selection(doc);
        return;
    }

    /* < (Shift+,) - Dedent selection */
    if (key == GLFW_KEY_COMMA && (mods & GLFW_MOD_SHIFT)) {
        document_dedent_selection(doc);
        return;
    }

    /* s - Select all matches of selection text within selection */
    if (key == GLFW_KEY_S && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        Cursor *cur = &doc->cursors[0];
        if (cur->has_selection) {
            panel_find_open_ex(app, doc, FR_ACTION_SELECT);
        }
        return;
    }

    /* S - Split selections on matches of selection text */
    if (key == GLFW_KEY_S && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        Cursor *cur = &doc->cursors[0];
        if (cur->has_selection) {
            panel_find_open_ex(app, doc, FR_ACTION_SPLIT);
        }
        return;
    }

    /* K - Keep selections matching */
    if (key == GLFW_KEY_K && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        panel_find_open_ex(app, doc, FR_ACTION_KEEP);
        return;
    }

    /* Alt-K - Remove selections matching */
    if (key == GLFW_KEY_K && (mods & GLFW_MOD_ALT)) {
        panel_find_open_ex(app, doc, FR_ACTION_REMOVE);
        return;
    }

    /* Shell commands */
    /* | - Pipe selection through shell command */
    if (key == GLFW_KEY_BACKSLASH && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_ALT)) {
        Cursor *cur = &doc->cursors[0];
        if (cur->has_selection) {
            panel_find_open_ex(app, doc, FR_ACTION_PIPE);
        }
        return;
    }

    /* Alt-| - Pipe selection to command (ignore output) */
    if (key == GLFW_KEY_BACKSLASH && (mods & GLFW_MOD_SHIFT) && (mods & GLFW_MOD_ALT)) {
        Cursor *cur = &doc->cursors[0];
        if (cur->has_selection) {
            panel_find_open_ex(app, doc, FR_ACTION_PIPE_TO);
        }
        return;
    }

    /* ! - Insert command output at cursor */
    if (key == GLFW_KEY_1 && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_ALT)) {
        panel_find_open_ex(app, doc, FR_ACTION_INSERT_OUTPUT);
        return;
    }

    /* Alt-! - Append command output to end of line */
    if (key == GLFW_KEY_1 && (mods & GLFW_MOD_SHIFT) && (mods & GLFW_MOD_ALT)) {
        panel_find_open_ex(app, doc, FR_ACTION_APPEND_OUTPUT);
        return;
    }

    /* $ - Filter selection through shell command (without alt) or end of line (without selection) */
    if (key == GLFW_KEY_4 && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_ALT)) {
        Cursor *cur = &doc->cursors[0];
        if (cur->has_selection) {
            panel_find_open_ex(app, doc, FR_ACTION_FILTER);
            return;
        }
        /* Fall through to normal $ behavior below */
    }

    /* Ctrl-Shift-C - block comment toggle. Check before plain Ctrl-C. */
    if ((mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT) && key == GLFW_KEY_C) {
        const LanguageSettings *ls = language_settings_get(doc->language_id);
        if (ls && ls->comment_open && ls->comment_open[0]) {
            document_comment_toggle_block(doc, ls->comment_open, ls->comment_close);
        } else {
            document_comment_toggle_block(doc, "/*", "*/");
        }
        goto finish_normal_key;
    }

    /* J - Join lines inside selection */
    if (key == GLFW_KEY_J && (mods & GLFW_MOD_SHIFT) &&
        !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        document_join_lines_selection(doc);
        goto finish_normal_key;
    }

    switch (key) {
    /* Mode switching */
    case GLFW_KEY_I: mode_set(mode, MODE_INSERT); goto finish_normal_key;
    case GLFW_KEY_A: {
        Cursor *cur = &doc->cursors[0];
        cur->col = (int)buffer_line_len(&doc->buffer, cur->row);
        mode_set(mode, MODE_INSERT);
        goto finish_normal_key;
    }
    case GLFW_KEY_V: mode_set(mode, MODE_SELECT);
                     cursor_select_start(&doc->cursors[0]); goto finish_normal_key;
    case GLFW_KEY_SPACE:
        mode->pending_key = ' ';
        panel_space_menu_open(app);
        goto finish_normal_key;
    case GLFW_KEY_SLASH: {
        Document *doc2 = (Document *)app_get_doc(app);
        if (mods & GLFW_MOD_SHIFT)
            panel_find_open_backward(app, doc2);
        else
            panel_find_open(app, doc2);
        goto finish_normal_key;
    }
    /* G - handled before switch */

    /* Navigation (with count) */
    case GLFW_KEY_H: case GLFW_KEY_LEFT:  for (int i = 0; i < count; i++) document_move_cursor(doc, 0, -1); goto finish_normal_key;
    case GLFW_KEY_J: case GLFW_KEY_DOWN:  for (int i = 0; i < count; i++) document_move_cursor(doc, 1, 0); goto finish_normal_key;
    case GLFW_KEY_K: case GLFW_KEY_UP:    for (int i = 0; i < count; i++) document_move_cursor(doc, -1, 0); goto finish_normal_key;
    case GLFW_KEY_L: case GLFW_KEY_RIGHT: for (int i = 0; i < count; i++) document_move_cursor(doc, 0, 1); goto finish_normal_key;
    case GLFW_KEY_HOME:  document_cursor_home(doc); goto finish_normal_key;
    case GLFW_KEY_END:   document_cursor_end(doc); goto finish_normal_key;
    case GLFW_KEY_PAGE_UP:   document_cursor_page_up(doc); goto finish_normal_key;
    case GLFW_KEY_PAGE_DOWN: document_cursor_page_down(doc); goto finish_normal_key;

    /* Word motions (with count) */
    case GLFW_KEY_W: for (int i = 0; i < count; i++) document_cursor_word_forward(doc); goto finish_normal_key;
    case GLFW_KEY_B: for (int i = 0; i < count; i++) document_cursor_word_backward(doc); goto finish_normal_key;
    case GLFW_KEY_E: for (int i = 0; i < count; i++) document_cursor_word_end(doc); goto finish_normal_key;

    /* WORD motions (Shift+W/B/E) */
    /* Handled before switch with shift check */

    /* Line start/end */
    case GLFW_KEY_0: document_cursor_home(doc); goto finish_normal_key;
    case GLFW_KEY_4:
        if (mods & GLFW_MOD_SHIFT) {
            document_cursor_end(doc);
            goto finish_normal_key;
        }
        break;

    /* n/N - Search next/prev */
    case GLFW_KEY_N:
        if (mods & GLFW_MOD_SHIFT)
            document_search_prev(doc);
        else
            document_search_next(doc);
        goto finish_normal_key;

    /* x/X - Select line / extend / shrink to line bounds */
    case GLFW_KEY_X:
        if (mods & GLFW_MOD_SHIFT)
            document_shrink_to_line_bounds(doc);
        else {
            Cursor *cur = &doc->cursors[0];
            if (!cur->has_selection) {
                document_select_line(doc);
            } else {
                int sr, sc, er, ec;
                cursor_normalize(cur, &sr, &sc, &er, &ec);
                int next_line = er + 1;
                size_t total_lines = buffer_line_count(&doc->buffer);
                if (next_line < (int)total_lines) {
                    cur->anchor_col = 0;
                    cur->has_selection = true;
                    cursor_move_to(cur, next_line, 0);
                    int next_len = (int)buffer_line_len(&doc->buffer, next_line);
                    cur->col = next_len;
                }
            }
        }
        goto finish_normal_key;

    /* % - Select entire file */
    case GLFW_KEY_5:
        document_select_all(doc);
        goto finish_normal_key;

    /* p - Paste after */
    case GLFW_KEY_P: document_paste(doc); goto finish_normal_key;

    /* u/U - Undo/Redo */
    case GLFW_KEY_U:
        if (mods & GLFW_MOD_SHIFT)
            document_redo(doc);
        else
            document_undo(doc);
        goto finish_normal_key;

    default: break;
    }

    /* Keys that conflict with switch cases handled here */
    switch (key) {
    case GLFW_KEY_D:
        if (doc->cursors[0].has_selection) {
            document_delete_selection(doc);
        } else {
            mode->pending_operator = 'd';
        }
        break;
    case GLFW_KEY_C:
        if (doc->cursors[0].has_selection) {
            document_delete_selection(doc);
            mode_set(mode, MODE_INSERT);
        } else {
            mode->pending_operator = 'c';
        }
        break;
    case GLFW_KEY_Y:
        if (doc->cursors[0].has_selection) {
            document_yank(doc);
        } else {
            mode->pending_operator = 'y';
        }
        break;
    case GLFW_KEY_R:
        if (mods & GLFW_MOD_SHIFT)
            document_replace_selection_yanked(doc);
        else
            mode->pending_key = 'r';
        break;
    case GLFW_KEY_SEMICOLON:
        document_collapse_selection(doc);
        break;
    case GLFW_KEY_GRAVE_ACCENT:
        if (mods & GLFW_MOD_SHIFT) {
            document_toggle_case(doc);
            mode->suppress_next_char = true;
        } else {
            document_lowercase(doc);
        }
        break;
    case GLFW_KEY_COMMA:
        document_keep_primary_selection(doc);
        break;
    case GLFW_KEY_F:
        mode->pending_key = (mods & GLFW_MOD_SHIFT) ? 'F' : 'f';
        break;
    case GLFW_KEY_T:
        mode->pending_key = (mods & GLFW_MOD_SHIFT) ? 'T' : 't';
        break;
    case GLFW_KEY_I:
        mode->pending_text_obj = 'i';
        mode->pending_key = 'i';
        break;
    case GLFW_KEY_A:
        mode->pending_text_obj = 'a';
        mode->pending_key = 'i';
        break;
    case GLFW_KEY_Z:
        if (mods & GLFW_MOD_SHIFT) {
            /* Z - sticky view mode */
            mode->pending_key = 'z';
            mode->view_mode_sticky = true;
        } else {
            /* z - normal view mode */
            mode->pending_key = 'z';
            mode->view_mode_sticky = false;
        }
        break;
    case GLFW_KEY_MINUS:
        document_trim_whitespace(doc);
        break;
    case GLFW_KEY_EQUAL:
        input_format_document(app, doc);
        break;
    case GLFW_KEY_M:
        mode->pending_key = 'm';
        mode->pending_len = 0;
        break;
    case GLFW_KEY_7:
        document_align_selections(doc);
        break;
    case GLFW_KEY_Q:
        /* q - toggle macro recording */
        if (macro_is_recording(&doc->macros)) {
            macro_stop_record(&doc->macros);
        } else {
            mode->pending_key = 'q';
        }
        break;
    case GLFW_KEY_2:
        /* @ - replay macro */
        if (mods & GLFW_MOD_SHIFT) {
            mode->pending_key = '@';
        } else {
            /* @@ - replay last macro (handled via pending) */
            mode->pending_key = '@';
            mode->pending_keys[0] = '@';
            mode->pending_len = 1;
        }
        break;
    default: break;
    }

finish_normal_key:
    /* Record key if macro is recording */
    if (macro_is_recording(&doc->macros)) {
        macro_record_key(&doc->macros, key);
    }
}

static void handle_insert_key(App *app, int key, int action, int mods) {
    (void)action;
    Document *doc = (Document *)app_get_doc(app);
    ModeState *mode = (ModeState *)app_get_mode(app);

    /* Ctrl-x - completion */
    if (key == GLFW_KEY_X && (mods & GLFW_MOD_CONTROL)) {
        panel_completion_open(app);
        return;
    }

    /* Ctrl-w / Alt-Backspace - delete previous word */
    if (key == GLFW_KEY_W && (mods & GLFW_MOD_CONTROL)) {
        Cursor *cur = &doc->cursors[0];
        const char *line = buffer_line_ptr(&doc->buffer, cur->row);
        int col = cur->col;
        if (col > 0) {
            int start = col - 1;
            while (start > 0 && (line[start] == ' ' || line[start] == '\t'))
                start--;
            while (start > 0 && line[start - 1] != ' ' && line[start - 1] != '\t'
                   && line[start - 1] != '\n')
                start--;
            int del = col - start;
            size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, start);
            buffer_delete(&doc->buffer, pos, del);
            cur->col = start;
            document_mark_dirty(doc);
        }
        return;
    }
    if (key == GLFW_KEY_BACKSPACE && (mods & GLFW_MOD_ALT)) {
        Cursor *cur = &doc->cursors[0];
        const char *line = buffer_line_ptr(&doc->buffer, cur->row);
        int col = cur->col;
        if (col > 0) {
            int start = col - 1;
            while (start > 0 && (line[start] == ' ' || line[start] == '\t'))
                start--;
            while (start > 0 && line[start - 1] != ' ' && line[start - 1] != '\t'
                   && line[start - 1] != '\n')
                start--;
            int del = col - start;
            size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, start);
            buffer_delete(&doc->buffer, pos, del);
            cur->col = start;
            document_mark_dirty(doc);
        }
        return;
    }

    /* Ctrl-h / Backspace - delete previous char */
    if (key == GLFW_KEY_H && (mods & GLFW_MOD_CONTROL)) {
        insert_backspace(doc, mode);
        return;
    }

    /* Ctrl-j / Enter - insert newline */
    if (key == GLFW_KEY_J && (mods & GLFW_MOD_CONTROL)) {
        insert_newline(doc, mode);
        return;
    }

    /* Ctrl-d / Delete - delete next char */
    if (key == GLFW_KEY_D && (mods & GLFW_MOD_CONTROL)) {
        document_delete_char_at_cursor(doc);
        return;
    }
    if (key == GLFW_KEY_DELETE) {
        document_delete_char_at_cursor(doc);
        return;
    }

    /* Alt-d / Alt-Delete - delete next word */
    if (key == GLFW_KEY_D && (mods & GLFW_MOD_ALT)) {
        document_delete_word_forward(doc);
        return;
    }
    if (key == GLFW_KEY_DELETE && (mods & GLFW_MOD_ALT)) {
        document_delete_word_forward(doc);
        return;
    }

    /* Ctrl-u - delete to line start */
    if (key == GLFW_KEY_U && (mods & GLFW_MOD_CONTROL)) {
        Cursor *cur = &doc->cursors[0];
        if (cur->col > 0) {
            size_t start = buffer_pos_from_row_col(&doc->buffer, cur->row, 0);
            size_t end = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
            buffer_delete(&doc->buffer, start, end - start);
            cur->col = 0;
            document_mark_dirty(doc);
        }
        return;
    }

    /* Ctrl-k - delete to line end */
    if (key == GLFW_KEY_K && (mods & GLFW_MOD_CONTROL)) {
        Cursor *cur = &doc->cursors[0];
        int len = (int)buffer_line_len(&doc->buffer, cur->row);
        if (cur->col < len) {
            size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
            size_t line_end = buffer_pos_from_row_col(&doc->buffer, cur->row, len);
            buffer_delete(&doc->buffer, pos, line_end - pos);
            document_mark_dirty(doc);
        }
        return;
    }

    /* Ctrl-z / Ctrl-Shift-z - undo/redo */
    if (key == GLFW_KEY_Z && (mods & GLFW_MOD_CONTROL)) {
        if (mods & GLFW_MOD_SHIFT)
            document_redo(doc);
        else
            document_undo(doc);
        return;
    }

    /* Ctrl-y - redo */
    if (key == GLFW_KEY_Y && (mods & GLFW_MOD_CONTROL)) {
        document_redo(doc);
        return;
    }

    /* Ctrl-r - insert register content */
    if (key == GLFW_KEY_R && (mods & GLFW_MOD_CONTROL)) {
        if (doc->clipboard && doc->clipboard_len > 0) {
            for (size_t i = 0; i < doc->clipboard_len; i++) {
                if (doc->clipboard[i] == '\n')
                    document_newline(doc);
                else
                    document_insert_char(doc, doc->clipboard[i]);
            }
        }
        return;
    }

    switch (key) {
    case GLFW_KEY_ESCAPE:
        document_clear_cursors(doc);
        mode_set(mode, MODE_NORMAL);
        break;
    case GLFW_KEY_BACKSPACE:
        insert_backspace(doc, mode);
        break;
    case GLFW_KEY_ENTER:
        insert_newline(doc, mode);
        break;
    case GLFW_KEY_TAB:
        insert_tab(doc, mode);
        break;
    case GLFW_KEY_HOME:
        document_cursor_home(doc);
        break;
    case GLFW_KEY_END:
        document_cursor_end(doc);
        break;
    case GLFW_KEY_UP:
        document_move_cursor(doc, -1, 0);
        break;
    case GLFW_KEY_DOWN:
        document_move_cursor(doc, 1, 0);
        break;
    case GLFW_KEY_LEFT:
        document_move_cursor(doc, 0, -1);
        break;
    case GLFW_KEY_RIGHT:
        document_move_cursor(doc, 0, 1);
        break;
    case GLFW_KEY_PAGE_UP:
        document_cursor_page_up(doc);
        break;
    case GLFW_KEY_PAGE_DOWN:
        document_cursor_page_down(doc);
        break;
    default:
        break;
    }
}

static void handle_command_key(App *app, int key, int action, int mods) {
    (void)action;
    Document *doc = (Document *)app_get_doc(app);
    ModeState *mode = (ModeState *)app_get_mode(app);

    if ((key == GLFW_KEY_BACKSPACE || (key == GLFW_KEY_H && (mods & GLFW_MOD_CONTROL))) && cmd_len > 0) {
        cmd_len--;
        cmd_buf[cmd_len] = '\0';
        command_completion_update(app);
        return;
    }

    if (key == GLFW_KEY_TAB) {
        if (cmd_completion_count > 0) {
            if (mods & GLFW_MOD_SHIFT) {
                cmd_completion_selected--;
                if (cmd_completion_selected < 0)
                    cmd_completion_selected = cmd_completion_count - 1;
            }
            command_completion_accept(app);
        }
        return;
    }

    if (key == GLFW_KEY_DOWN) {
        if (cmd_completion_count > 0)
            cmd_completion_selected = (cmd_completion_selected + 1) % cmd_completion_count;
        return;
    }

    if (key == GLFW_KEY_UP) {
        if (cmd_completion_count > 0) {
            cmd_completion_selected--;
            if (cmd_completion_selected < 0)
                cmd_completion_selected = cmd_completion_count - 1;
        }
        return;
    }

    if (key != GLFW_KEY_ENTER) return;

    const char *write_path = NULL;
    bool write_and_quit = false;
    if (strncmp(cmd_buf, "w ", 2) == 0) {
        write_path = cmd_buf + 2;
    } else if (strncmp(cmd_buf, "write ", 6) == 0) {
        write_path = cmd_buf + 6;
    } else if (strncmp(cmd_buf, "w! ", 3) == 0) {
        write_path = cmd_buf + 3;
    } else if (strncmp(cmd_buf, "write! ", 7) == 0) {
        write_path = cmd_buf + 7;
    } else if (strncmp(cmd_buf, "wq ", 3) == 0) {
        write_path = cmd_buf + 3;
        write_and_quit = true;
    } else if (strncmp(cmd_buf, "write-quit ", 11) == 0) {
        write_path = cmd_buf + 11;
        write_and_quit = true;
    } else if (strncmp(cmd_buf, "x ", 2) == 0) {
        write_path = cmd_buf + 2;
        write_and_quit = true;
    }

    /* Execute command */
    if (write_path && *write_path) {
        char resolved[CMD_BUF_MAX];
        command_resolve_workspace_path(app, write_path, resolved, sizeof(resolved));
        document_save_as(doc, resolved[0] ? resolved : write_path);
        if (write_and_quit)
            cmd_quit(app);
    } else if (strcmp(cmd_buf, "w") == 0 || strcmp(cmd_buf, "write") == 0) {
        document_save(doc);
    } else if (strcmp(cmd_buf, "q") == 0 || strcmp(cmd_buf, "quit") == 0) {
        cmd_quit(app);
    } else if (strcmp(cmd_buf, "wq") == 0 || strcmp(cmd_buf, "x") == 0) {
        document_save(doc);
        cmd_quit(app);
    } else if (strcmp(cmd_buf, "q!") == 0) {
        cmd_quit(app);
    } else if (strcmp(cmd_buf, "w!") == 0) {
        document_save(doc);
    } else if (strcmp(cmd_buf, "wq!") == 0 || strcmp(cmd_buf, "x!") == 0) {
        document_save(doc);
        cmd_quit(app);
    } else if (strcmp(cmd_buf, "wqa") == 0 || strcmp(cmd_buf, "wqa!") == 0 ||
               strcmp(cmd_buf, "write-quit-all") == 0 ||
               strcmp(cmd_buf, "write-quit-all!") == 0) {
        input_save_all_buffers(app);
        cmd_quit(app);
    } else if (strcmp(cmd_buf, "qa") == 0 || strcmp(cmd_buf, "quit-all") == 0 ||
               strcmp(cmd_buf, "qa!") == 0 || strcmp(cmd_buf, "quit-all!") == 0) {
        cmd_quit(app);
    } else if (strcmp(cmd_buf, "bn") == 0 || strcmp(cmd_buf, "bnext") == 0 ||
               strcmp(cmd_buf, "buffer-next") == 0) {
        app_next_buffer(app);
    } else if (strcmp(cmd_buf, "bp") == 0 || strcmp(cmd_buf, "bprev") == 0 ||
               strcmp(cmd_buf, "buffer-previous") == 0) {
        app_prev_buffer(app);
    } else if (strncmp(cmd_buf, "b ", 2) == 0) {
        input_buffer_command(app, cmd_buf + 2, false);
    } else if (strncmp(cmd_buf, "buffer ", 7) == 0) {
        input_buffer_command(app, cmd_buf + 7, false);
    } else if (strcmp(cmd_buf, "bc") == 0 || strcmp(cmd_buf, "bclose") == 0 ||
               strcmp(cmd_buf, "buffer-close") == 0 ||
               strcmp(cmd_buf, "bc!") == 0 || strcmp(cmd_buf, "bclose!") == 0 ||
               strcmp(cmd_buf, "buffer-close!") == 0) {
        app_close_buffer(app, app_get_current_buffer_index(app));
    } else if (strncmp(cmd_buf, "bc ", 3) == 0 || strncmp(cmd_buf, "bclose ", 7) == 0 ||
               strncmp(cmd_buf, "buffer-close ", 13) == 0) {
        const char *arg = cmd_buf[1] == 'c' && cmd_buf[2] == ' ' ? cmd_buf + 3 :
                          cmd_buf[0] == 'b' && cmd_buf[1] == 'c' ? cmd_buf + 7 :
                          cmd_buf + 13;
        input_buffer_command(app, (char *)arg, true);
    } else if (strcmp(cmd_buf, "new") == 0 || strcmp(cmd_buf, "n") == 0) {
        document_new(doc);
    } else if (strncmp(cmd_buf, "e ", 2) == 0 ||
               strncmp(cmd_buf, "o ", 2) == 0 ||
               strncmp(cmd_buf, "open ", 5) == 0 ||
               strncmp(cmd_buf, "edit ", 5) == 0) {
        const char *path = (cmd_buf[1] == ' ') ? cmd_buf + 2 : cmd_buf + 5;
        char resolved[CMD_BUF_MAX];
        command_resolve_workspace_path(app, path, resolved, sizeof(resolved));
        app_open_file(app, resolved[0] ? resolved : path);
    } else if (strncmp(cmd_buf, "r ", 2) == 0 || strncmp(cmd_buf, "read ", 5) == 0) {
        const char *path = cmd_buf[1] == ' ' ? cmd_buf + 2 : cmd_buf + 5;
        char resolved[CMD_BUF_MAX];
        command_resolve_workspace_path(app, path, resolved, sizeof(resolved));
        document_insert_file(doc, resolved[0] ? resolved : path);
    } else if (strncmp(cmd_buf, "mv ", 3) == 0 || strncmp(cmd_buf, "move ", 5) == 0) {
        const char *path = cmd_buf[2] == ' ' ? cmd_buf + 3 : cmd_buf + 5;
        char resolved[CMD_BUF_MAX];
        command_resolve_workspace_path(app, path, resolved, sizeof(resolved));
        document_move_file(doc, resolved[0] ? resolved : path);
    } else if (strncmp(cmd_buf, "cwd ", 4) == 0 ||
               strncmp(cmd_buf, "open-workspace ", 15) == 0) {
        bool change_dir = cmd_buf[0] == 'c';
        const char *path = change_dir ? cmd_buf + 4 : cmd_buf + 15;
        char resolved[CMD_BUF_MAX];
        command_resolve_workspace_path(app, path, resolved, sizeof(resolved));
        struct stat st;
        bool is_dir = resolved[0] && stat(resolved, &st) == 0 && S_ISDIR(st.st_mode);
        if (!is_dir) {
            notification_push(NOTIF_ERROR, "Not a directory: %s", resolved[0] ? resolved : path);
        } else if (change_dir && chdir(resolved) != 0) {
            notification_push(NOTIF_ERROR, "Could not change dir: %s", resolved);
        } else {
            app_set_workspace_root(app, resolved);
        }
    } else if (strcmp(cmd_buf, "reload") == 0 || strcmp(cmd_buf, "rl") == 0 ||
               strcmp(cmd_buf, "reload-all") == 0 || strcmp(cmd_buf, "rla") == 0) {
        if (doc->filepath) {
            document_open(doc, doc->filepath);
            document_notify_lsp_open(doc, app_get_lsp_manager(app));
        }
    } else if (strcmp(cmd_buf, "sort") == 0) {
        document_sort_selection(doc);
    } else if (strcmp(cmd_buf, "fmt") == 0 || strcmp(cmd_buf, "format") == 0) {
        input_format_document(app, doc);
    } else if (strncmp(cmd_buf, "theme", 5) == 0 &&
               (cmd_buf[5] == '\0' || isspace((unsigned char)cmd_buf[5]))) {
        input_theme_command(app, cmd_buf + 5);
    } else if (strncmp(cmd_buf, "colorscheme", 11) == 0 &&
               (cmd_buf[11] == '\0' || isspace((unsigned char)cmd_buf[11]))) {
        input_theme_command(app, cmd_buf + 11);
    } else if (strncmp(cmd_buf, "plugin-enable", 13) == 0 &&
               (cmd_buf[13] == '\0' || isspace((unsigned char)cmd_buf[13]))) {
        input_plugin_command(app, cmd_buf + 13, 1);
    } else if (strncmp(cmd_buf, "plugin-disable", 14) == 0 &&
               (cmd_buf[14] == '\0' || isspace((unsigned char)cmd_buf[14]))) {
        input_plugin_command(app, cmd_buf + 14, -1);
    } else if (strncmp(cmd_buf, "plugin-toggle", 13) == 0 &&
               (cmd_buf[13] == '\0' || isspace((unsigned char)cmd_buf[13]))) {
        input_plugin_command(app, cmd_buf + 13, 0);
    } else if (strncmp(cmd_buf, "reflow ", 7) == 0) {
        int width = atoi(cmd_buf + 7);
        if (width > 0) document_reflow(doc, width);
    } else if (strcmp(cmd_buf, "retab") == 0 || strcmp(cmd_buf, "retab!") == 0) {
        /* Convert all indentation to tabs using configured tab width */
        const LanguageSettings *ls = language_settings_get(doc->language_id);
        int tw = ls ? ls->tab_width : 4;
        document_indent_style_to_tabs(doc, tw);
    } else if (strcmp(cmd_buf, "expandtab") == 0 || strcmp(cmd_buf, "expandtab!") == 0) {
        const LanguageSettings *ls = language_settings_get(doc->language_id);
        int sw = ls ? ls->tab_width : 4;
        document_indent_style_to_spaces(doc, sw);
    } else if (strcmp(cmd_buf, "lsp-stop") == 0) {
        lsp_manager_stop_all((LSPManager *)app_get_lsp_manager(app));
    } else if (strcmp(cmd_buf, "lsp-restart") == 0) {
        lsp_manager_restart_all((LSPManager *)app_get_lsp_manager(app));
    } else if (strcmp(cmd_buf, "workspace-symbols") == 0) {
        panel_workspace_symbols_open(app);
    } else if (strcmp(cmd_buf, "workspace-diagnostics") == 0) {
        panel_workspace_diagnostics_open(app);
    } else if (strcmp(cmd_buf, "tree-sitter-subtree") == 0 ||
               strcmp(cmd_buf, "ts-subtree") == 0 ||
               strcmp(cmd_buf, "tree-sitter-highlight-name") == 0 ||
               strcmp(cmd_buf, "tree-sitter-scopes") == 0) {
        panel_treesitter_inspector_open(app);
    } else {
        Command *cmd = command_find(cmd_buf);
        if (cmd && cmd->fn) {
            cmd->fn(app);
        } else if (cmd_buf[0] >= '1' && cmd_buf[0] <= '9') {
            int line = atoi(cmd_buf);
            if (line > 0)
                document_cursor_to(doc, line - 1, 0);
        }
    }

    input_cmd_reset();
    mode_set(mode, MODE_NORMAL);
}

static void handle_select_key(App *app, int key, int action, int mods) {
    (void)action;
    Document *doc = (Document *)app_get_doc(app);
    ModeState *mode = (ModeState *)app_get_mode(app);

    if (mods & GLFW_MOD_SHIFT) {
        Cursor *cur = &doc->cursors[0];
        if (!cur->has_selection) cursor_select_start(cur);
    }

    /* Initialize selection only once when entering select mode */
    if (!mode->select_mode_initialized) {
        Cursor *cur = &doc->cursors[0];
        if (!cur->has_selection) cursor_select_start(cur);
        mode->select_mode_initialized = true;
    }

    /* g pending in select mode - goto motions extend the selection */
    if (mode->g_pending) {
        mode->g_pending = false;
        if (key == GLFW_KEY_G)
            document_cursor_doc_start(doc);
        else if (key == GLFW_KEY_E)
            document_cursor_doc_end(doc);
        else if (key == GLFW_KEY_S)
            document_cursor_first_non_blank(doc);
        else if (key == GLFW_KEY_H)
            document_goto_line_start(doc);
        else if (key == GLFW_KEY_L)
            document_goto_line_end(doc);
        else if (key == GLFW_KEY_T)
            document_goto_view_top(doc);
        else if (key == GLFW_KEY_C)
            document_goto_view_center(doc);
        else if (key == GLFW_KEY_B)
            document_goto_view_bottom(doc);
        else if (key == GLFW_KEY_J)
            document_move_cursor(doc, 1, 0);
        else if (key == GLFW_KEY_K)
            document_move_cursor(doc, -1, 0);
        else if (key == GLFW_KEY_PERIOD)
            document_goto_last_modification(doc);
        return;
    }

    /* Ctrl combinations in select mode */
    if (mods & GLFW_MOD_CONTROL) {
        switch (key) {
        case GLFW_KEY_W:
            mode->pending_key = 'w';
            return;
        case GLFW_KEY_F:
            document_page_down_extend(doc);
            return;
        case GLFW_KEY_B:
            document_page_up_extend(doc);
            return;
        case GLFW_KEY_D:
            document_half_page_down_extend(doc, doc->viewport_lines);
            return;
        case GLFW_KEY_U:
            document_half_page_up_extend(doc, doc->viewport_lines);
            return;
        default: break;
        }
    }

    /* Alt combinations in select mode */
    if (mods & GLFW_MOD_ALT) {
        if (key == GLFW_KEY_C && (mods & GLFW_MOD_SHIFT)) {
            document_copy_selection_above(doc);
            return;
        }
        if (key == GLFW_KEY_SEMICOLON) {
            if (mods & GLFW_MOD_SHIFT)
                document_force_selection_forward(doc);
            else
                document_flip_cursor_anchor(doc);
            return;
        }
        if (key == GLFW_KEY_MINUS) {
            if (mods & GLFW_MOD_SHIFT)
                document_merge_consecutive_selections(doc);
            else
                document_merge_selections(doc);
            return;
        }
        if (key == GLFW_KEY_S) {
            document_split_selection_newlines(doc);
            return;
        }
        if (key == GLFW_KEY_J) {
            document_join_lines_with_space(doc);
            return;
        }
        if (key == GLFW_KEY_K) {
            panel_find_open_ex(app, doc, FR_ACTION_REMOVE);
            return;
        }
        if (key == GLFW_KEY_X) {
            document_shrink_to_line_bounds(doc);
            return;
        }
        if (key == GLFW_KEY_GRAVE_ACCENT) {
            document_uppercase(doc);
            return;
        }
        if (key == GLFW_KEY_9 && (mods & GLFW_MOD_SHIFT)) {
            document_rotate_selection_contents_backward(doc);
            return;
        }
        if (key == GLFW_KEY_0 && (mods & GLFW_MOD_SHIFT)) {
            document_rotate_selection_contents_forward(doc);
            return;
        }
        if (key == GLFW_KEY_COMMA) {
            document_remove_primary_selection(doc);
            return;
        }
        if (key == GLFW_KEY_D) {
            document_delete_selection(doc);
            mode_set(mode, MODE_NORMAL);
            return;
        }
        if (key == GLFW_KEY_C) {
            document_delete_selection(doc);
            mode_set(mode, MODE_INSERT);
            return;
        }
        if (key == GLFW_KEY_PERIOD) {
            /* Alt-. - Repeat last motion in select mode */
            if (mode->last_motion_type) {
                char c = mode->last_motion_char;
                switch (mode->last_motion_type) {
                case 'f': document_find_char_forward(doc, c); break;
                case 'F': document_find_char_backward(doc, c); break;
                case 't': document_till_char_forward(doc, c); break;
                case 'T': document_till_char_backward(doc, c); break;
                }
            }
            return;
        }
    }

    if (key == GLFW_KEY_C && (mods & GLFW_MOD_SHIFT) &&
        !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        document_copy_selection_below(doc);
        return;
    }

    /* > (Shift+.) - Indent selection */
    if (key == GLFW_KEY_PERIOD && (mods & GLFW_MOD_SHIFT)) {
        document_indent_selection(doc);
        return;
    }

    /* < (Shift+,) - Dedent selection */
    if (key == GLFW_KEY_COMMA && (mods & GLFW_MOD_SHIFT)) {
        document_dedent_selection(doc);
        return;
    }

    if (key == GLFW_KEY_S && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        panel_find_open_ex(app, doc, FR_ACTION_SELECT);
        return;
    }

    if (key == GLFW_KEY_S && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        panel_find_open_ex(app, doc, FR_ACTION_SPLIT);
        return;
    }

    if (key == GLFW_KEY_K && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        panel_find_open_ex(app, doc, FR_ACTION_KEEP);
        return;
    }

    if (key == GLFW_KEY_J && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        document_join_lines_selection(doc);
        return;
    }

    /* Pending keys in select mode */
    if (mode->pending_key) {
        char pk = mode->pending_key;
        mode->pending_key = 0;

        if (pk == 'w') {
            handle_window_key(app, key, mods);
            return;
        }

        if (pk == 'f') {
            char c = key_to_char(key, mods);
            if (c) {
                document_find_char_forward(doc, c);
                mode->last_motion_type = 'f';
                mode->last_motion_char = c;
            }
            return;
        }
        if (pk == 'F') {
            char c = key_to_char(key, mods);
            if (c) {
                document_find_char_backward(doc, c);
                mode->last_motion_type = 'F';
                mode->last_motion_char = c;
            }
            return;
        }
        if (pk == 't') {
            char c = key_to_char(key, mods);
            if (c) {
                document_till_char_forward(doc, c);
                mode->last_motion_type = 't';
                mode->last_motion_char = c;
            }
            return;
        }
        if (pk == 'T') {
            char c = key_to_char(key, mods);
            if (c) {
                document_till_char_backward(doc, c);
                mode->last_motion_type = 'T';
                mode->last_motion_char = c;
            }
            return;
        }
        if (pk == 'm') {
            if (key == GLFW_KEY_S) {
                mode->pending_key = 's';
                mode->pending_keys[0] = 's';
                mode->pending_len = 1;
                return;
            }
            if (key == GLFW_KEY_D) {
                mode->pending_key = 'd';
                mode->pending_keys[0] = 'd';
                mode->pending_len = 1;
                return;
            }
            if (key == GLFW_KEY_R) {
                mode->pending_key = 'r';
                mode->pending_keys[0] = 'r';
                mode->pending_len = 1;
                return;
            }
            if (key == GLFW_KEY_I && !(mods & GLFW_MOD_SHIFT)) {
                mode->pending_text_obj = 'i';
                mode->pending_key = 'i';
                return;
            }
            if (key == GLFW_KEY_A && !(mods & GLFW_MOD_SHIFT)) {
                mode->pending_text_obj = 'a';
                mode->pending_key = 'i';
                return;
            }
            if (key == GLFW_KEY_M) {
                document_match_bracket(doc);
                return;
            }
            return;
        }
        if (pk == 's' && mode->pending_len == 1 && mode->pending_keys[0] == 's') {
            char c = key_to_char(key, mods);
            if (c) document_surround(doc, c);
            mode->pending_len = 0;
            return;
        }
        if (pk == 'd' && mode->pending_len == 1 && mode->pending_keys[0] == 'd') {
            char c = key_to_char(key, mods);
            if (c) document_delete_surround(doc, c);
            mode->pending_len = 0;
            return;
        }
        if (pk == 'r' && mode->pending_len == 1 && mode->pending_keys[0] == 'r') {
            char from = key_to_char(key, mods);
            if (from) {
                mode->pending_key = 'r';
                mode->pending_keys[0] = 'r';
                mode->pending_keys[1] = from;
                mode->pending_len = 2;
                return;
            }
            mode->pending_len = 0;
            return;
        }
        if (pk == 'r' && mode->pending_len == 2 && mode->pending_keys[0] == 'r') {
            char from = mode->pending_keys[1];
            char to = key_to_char(key, mods);
            if (to) document_replace_surround(doc, from, to);
            mode->pending_len = 0;
            return;
        }
        if (pk == 'i') {
            /* Text object in select mode: extends selection */
            char obj = mode->pending_text_obj;
            bool inner = (obj == 'i');
            char c = key_to_object_char(key, mods);

            if (c) {
                Cursor *cur = &doc->cursors[0];
                if (!cur->has_selection) cursor_select_start(cur);
                switch (c) {
                case 'w': inner ? document_select_inside_word(doc) : document_select_around_word(doc); break;
                case '(': case ')': inner ? document_select_inside_paren(doc) : document_select_around_paren(doc); break;
                case '[': case ']': inner ? document_select_inside_bracket(doc) : document_select_around_bracket(doc); break;
                case '{': case '}': inner ? document_select_inside_curly(doc) : document_select_around_curly(doc); break;
                case '<': case '>': inner ? document_select_inside_angle(doc) : document_select_around_angle(doc); break;
                case '"': case '\'': inner ? document_select_inside_quote(doc) : document_select_around_quote(doc); break;
                case '`': inner ? document_select_inside_backtick(doc) : document_select_around_backtick(doc); break;
                case 'p': inner ? document_select_inside_paragraph(doc) : document_select_around_paragraph(doc); break;
                default: break;
                }
            }
            return;
        }
        return;
    }

    /* i/a - text objects in select mode */
    if (key == GLFW_KEY_I && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        mode->pending_text_obj = 'i';
        mode->pending_key = 'i';
        return;
    }
    if (key == GLFW_KEY_A && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        mode->pending_text_obj = 'a';
        mode->pending_key = 'i';
        return;
    }
    if (key == GLFW_KEY_M && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        mode->pending_key = 'm';
        mode->pending_len = 0;
        return;
    }

    switch (key) {
    case GLFW_KEY_ESCAPE:
        cursor_clear_selection(&doc->cursors[0]);
        mode_set(mode, MODE_NORMAL);
        break;
    /* Navigation - extends selection */
    case GLFW_KEY_H: case GLFW_KEY_LEFT:  document_move_cursor(doc, 0, -1); break;
    case GLFW_KEY_J: case GLFW_KEY_DOWN:  document_move_cursor(doc, 1, 0); break;
    case GLFW_KEY_K: case GLFW_KEY_UP:    document_move_cursor(doc, -1, 0); break;
    case GLFW_KEY_L: case GLFW_KEY_RIGHT: document_move_cursor(doc, 0, 1); break;
    case GLFW_KEY_HOME:  document_cursor_home(doc); break;
    case GLFW_KEY_END:   document_cursor_end(doc); break;
    case GLFW_KEY_PAGE_UP:   document_cursor_page_up(doc); break;
    case GLFW_KEY_PAGE_DOWN: document_cursor_page_down(doc); break;

    /* Word motions */
    case GLFW_KEY_W: document_cursor_word_forward(doc); break;
    case GLFW_KEY_B: document_cursor_word_backward(doc); break;
    case GLFW_KEY_E: document_cursor_word_end(doc); break;

    /* g - enter goto mode (extends selection) */
    case GLFW_KEY_G:
        if (mods & GLFW_MOD_SHIFT)
            document_cursor_doc_end(doc);
        else
            mode->g_pending = true;
        break;

    /* x - Extend selection to next line */
    case GLFW_KEY_X: {
        Cursor *cur = &doc->cursors[0];
        if (!cur->has_selection) {
            document_select_line(doc);
        } else {
            int sr, sc, er, ec;
            cursor_normalize(cur, &sr, &sc, &er, &ec);
            int next_line = er + 1;
            size_t total_lines = buffer_line_count(&doc->buffer);
            if (next_line < (int)total_lines) {
                cur->anchor_col = 0;
                cur->has_selection = true;
                cursor_move_to(cur, next_line, 0);
                int next_len = (int)buffer_line_len(&doc->buffer, next_line);
                cur->col = next_len;
            }
        }
        break;
    }

    /* d - Delete selection */
    case GLFW_KEY_D:
        document_delete_selection(doc);
        mode_set(mode, MODE_NORMAL);
        break;

    /* c - Change selection */
    case GLFW_KEY_C:
        document_delete_selection(doc);
        mode_set(mode, MODE_INSERT);
        break;

    /* y - Yank selection */
    case GLFW_KEY_Y: {
        Cursor *cur = &doc->cursors[0];
        document_yank(doc);
        cursor_clear_selection(cur);
        mode_set(mode, MODE_NORMAL);
        break;
    }

    /* p / P - Paste after/before */
    case GLFW_KEY_P:
        if (mods & GLFW_MOD_SHIFT)
            document_paste_before(doc);
        else
            document_paste(doc);
        break;

    /* n / N - Search next/prev (extend selection) */
    case GLFW_KEY_N:
        if (mods & GLFW_MOD_SHIFT)
            document_search_prev(doc);
        else
            document_search_next(doc);
        break;

    /* f/F/t/T - find/till extend selection */
    case GLFW_KEY_F:
        mode->pending_key = (mods & GLFW_MOD_SHIFT) ? 'F' : 'f';
        break;
    case GLFW_KEY_T:
        mode->pending_key = (mods & GLFW_MOD_SHIFT) ? 'T' : 't';
        break;

    /* ~ - Toggle case */
    case GLFW_KEY_GRAVE_ACCENT:
        if (mods & GLFW_MOD_SHIFT) {
            document_toggle_case(doc);
            mode->suppress_next_char = true;
        } else {
            document_lowercase(doc);
        }
        break;

    /* ; - Collapse selection */
    case GLFW_KEY_SEMICOLON:
        document_collapse_selection(doc);
        break;

    /* , - Keep primary selection */
    case GLFW_KEY_COMMA:
        document_keep_primary_selection(doc);
        break;

    /* % - Select entire file */
    case GLFW_KEY_5:
        document_select_all(doc);
        break;

    case GLFW_KEY_EQUAL:
        input_format_document(app, doc);
        break;

    default: break;
    }
}
