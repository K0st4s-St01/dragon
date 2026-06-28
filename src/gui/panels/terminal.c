#include "panel_terminal.h"
#include "renderer.h"
#include "theme.h"

#include <GLFW/glfw3.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define TERM_MAX_LINES 2048
#define TERM_MAX_COLS 512
#define CSI_BUF_MAX 32

static bool term_open = false;
static int term_fd = -1;
static pid_t term_pid = -1;
static char term_lines[TERM_MAX_LINES][TERM_MAX_COLS];
static int term_line_count = 1;
static int term_scroll = 0;
static int term_col = 0;
static int esc_state = 0;
static char csi_buf[CSI_BUF_MAX];
static int csi_len = 0;
static int term_cols = 80;
static int term_rows = 12;

static void terminal_reset_buffer(void) {
    memset(term_lines, 0, sizeof(term_lines));
    term_line_count = 1;
    term_scroll = 0;
    term_col = 0;
    esc_state = 0;
    csi_len = 0;
}

static bool terminal_running(void) {
    if (term_pid <= 0) return false;
    int status = 0;
    pid_t result = waitpid(term_pid, &status, WNOHANG);
    if (result == 0) return true;
    if (result == term_pid) {
        term_pid = -1;
        if (term_fd >= 0) {
            close(term_fd);
            term_fd = -1;
        }
    }
    return false;
}

static void terminal_append_newline(void) {
    if (term_line_count < TERM_MAX_LINES) {
        term_lines[term_line_count][0] = '\0';
        term_line_count++;
    } else {
        memmove(term_lines, term_lines + 1, sizeof(term_lines[0]) * (TERM_MAX_LINES - 1));
        term_lines[TERM_MAX_LINES - 1][0] = '\0';
    }
    if (term_scroll > 0) term_scroll++;
    term_col = 0;
}

static void terminal_clear_screen(void) {
    memset(term_lines, 0, sizeof(term_lines));
    term_line_count = 1;
    term_scroll = 0;
    term_col = 0;
}

static void terminal_clear_line_from_cursor(void) {
    char *line = term_lines[term_line_count - 1];
    size_t len = strlen(line);
    if (term_col < 0) term_col = 0;
    if (term_col < (int)len)
        line[term_col] = '\0';
}

static void terminal_append_byte(char c) {
    char *line = term_lines[term_line_count - 1];
    size_t len = strlen(line);
    if (c == '\n') {
        terminal_append_newline();
        return;
    }
    if (c == '\r') {
        term_col = 0;
        return;
    }
    if (c == '\b' || c == 127) {
        if (term_col > 0) {
            term_col--;
            if (term_col < (int)len)
                line[term_col] = '\0';
        }
        return;
    }
    if (c == '\t') {
        int spaces = 4 - (term_col % 4);
        for (int i = 0; i < spaces; i++) terminal_append_byte(' ');
        return;
    }
    if ((unsigned char)c < 32) return;
    if (term_col >= TERM_MAX_COLS - 1 || term_col >= term_cols) {
        terminal_append_newline();
        line = term_lines[term_line_count - 1];
        len = 0;
        term_col = 0;
    }
    if (term_col > (int)len) {
        int pad_to = term_col;
        if (pad_to > TERM_MAX_COLS - 2) pad_to = TERM_MAX_COLS - 2;
        while ((int)len < pad_to) {
            line[len++] = ' ';
            line[len] = '\0';
        }
    }
    line[term_col++] = c;
    if (term_col > (int)len)
        line[term_col] = '\0';
}

static int csi_param(int fallback) {
    int value = 0;
    bool any = false;
    for (int i = 0; i < csi_len; i++) {
        if (csi_buf[i] >= '0' && csi_buf[i] <= '9') {
            value = value * 10 + (csi_buf[i] - '0');
            any = true;
        } else if (csi_buf[i] == ';') {
            break;
        }
    }
    return any ? value : fallback;
}

static void terminal_handle_csi(char final) {
    int n = csi_param(1);
    if (n < 1) n = 1;

    switch (final) {
    case 'C':
        term_col += n;
        if (term_col >= TERM_MAX_COLS) term_col = TERM_MAX_COLS - 1;
        break;
    case 'D':
        term_col -= n;
        if (term_col < 0) term_col = 0;
        break;
    case 'G':
        term_col = n - 1;
        if (term_col < 0) term_col = 0;
        if (term_col >= TERM_MAX_COLS) term_col = TERM_MAX_COLS - 1;
        break;
    case 'H':
    case 'f':
        term_col = 0;
        break;
    case 'J':
        if (n == 2 || n == 3)
            terminal_clear_screen();
        break;
    case 'K':
        terminal_clear_line_from_cursor();
        break;
    default:
        break;
    }
}

static void terminal_consume_output(const char *buf, ssize_t len) {
    for (ssize_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)buf[i];
        if (esc_state) {
            if (esc_state == 1) {
                if (c == '[') {
                    esc_state = 2;
                    csi_len = 0;
                } else if (c == ']' || c == '(' || c == ')') {
                    esc_state = 3;
                } else {
                    esc_state = 0;
                }
                continue;
            }
            if (esc_state == 2) {
                if (c >= 0x40 && c <= 0x7e) {
                    terminal_handle_csi((char)c);
                    esc_state = 0;
                    csi_len = 0;
                } else if (csi_len < CSI_BUF_MAX - 1) {
                    csi_buf[csi_len++] = (char)c;
                    csi_buf[csi_len] = '\0';
                }
                continue;
            }
            if ((c >= '@' && c <= '~') || c == '\a')
                esc_state = 0;
            continue;
        }
        if (c == 0x1b) {
            esc_state = 1;
            continue;
        }
        terminal_append_byte((char)c);
    }
}

static void terminal_poll(void) {
    if (term_fd < 0) return;
    char buf[4096];
    for (;;) {
        ssize_t n = read(term_fd, buf, sizeof(buf));
        if (n > 0) {
            terminal_consume_output(buf, n);
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        if (n == 0 || (n < 0 && errno != EINTR)) {
            close(term_fd);
            term_fd = -1;
            term_pid = -1;
            terminal_append_newline();
            terminal_consume_output("[terminal exited]", 17);
            break;
        }
    }
}

static void terminal_write_bytes(const char *s, size_t len) {
    if (term_fd < 0 || !terminal_running()) return;
    while (len > 0) {
        ssize_t n = write(term_fd, s, len);
        if (n > 0) {
            s += n;
            len -= (size_t)n;
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
}

static void terminal_compute_layout(Gui *g, App *app,
                                    float *px, float *py, float *pw, float *ph,
                                    int *cols, int *rows) {
    int w = app_get_width(app);
    int h = app_get_height(app);
    float panel_h = (float)h * 0.38f;
    if (panel_h < 160.0f) panel_h = 160.0f;
    if (panel_h > (float)h - 48.0f) panel_h = (float)h - 48.0f;
    if (panel_h < 80.0f) panel_h = 80.0f;
    float line_h = g ? g->font.glyph_h + 4.0f : 18.0f;
    float glyph_w = g ? g->font.glyph_w : 8.0f;

    if (px) *px = 0.0f;
    if (py) *py = (float)h - panel_h - 24.0f;
    if (pw) *pw = (float)w;
    if (ph) *ph = panel_h;
    if (cols) {
        *cols = (int)(((float)w - 24.0f) / glyph_w);
        if (*cols < 20) *cols = 20;
        if (*cols > TERM_MAX_COLS - 1) *cols = TERM_MAX_COLS - 1;
    }
    if (rows) {
        *rows = (int)((panel_h - 42.0f) / line_h);
        if (*rows < 4) *rows = 4;
    }
}

static void terminal_resize(int cols, int rows) {
    if (term_fd < 0) return;
    if (cols < 20) cols = 20;
    if (rows < 4) rows = 4;
    if (cols == term_cols && rows == term_rows) return;
    term_cols = cols;
    term_rows = rows;
    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    ws.ws_col = (unsigned short)term_cols;
    ws.ws_row = (unsigned short)term_rows;
    ioctl(term_fd, TIOCSWINSZ, &ws);
    if (term_pid > 0) kill(term_pid, SIGWINCH);
}

static void terminal_spawn(App *app) {
    if (terminal_running()) return;
    terminal_reset_buffer();

    const char *shell = getenv("SHELL");
    if (!shell || !*shell) shell = "/bin/sh";

    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    term_cols = app_get_width(app) / 8;
    term_rows = app_get_height(app) / 28;
    if (term_cols < 20) term_cols = 20;
    if (term_rows < 6) term_rows = 6;
    ws.ws_col = (unsigned short)term_cols;
    ws.ws_row = (unsigned short)term_rows;

    term_pid = forkpty(&term_fd, NULL, NULL, &ws);
    if (term_pid == 0) {
        setenv("TERM", "xterm-256color", 1);
        const char *root = app_get_workspace_root(app);
        if (root && *root) chdir(root);
        execl(shell, shell, (char *)NULL);
        execl("/bin/sh", "sh", (char *)NULL);
        _exit(127);
    }

    if (term_pid < 0) {
        term_pid = -1;
        term_fd = -1;
        terminal_consume_output("failed to open terminal", 23);
        return;
    }

    int flags = fcntl(term_fd, F_GETFL, 0);
    if (flags >= 0) fcntl(term_fd, F_SETFL, flags | O_NONBLOCK);
}

void panel_terminal_open(App *app) {
    term_open = true;
    terminal_spawn(app);
    terminal_resize(term_cols, term_rows);
}

void panel_terminal_close(App *app) {
    (void)app;
    term_open = false;
}

bool panel_terminal_is_open(void) {
    return term_open;
}

void panel_terminal_key(App *app, int key, int mods) {
    if (!term_open) return;
    terminal_poll();

    if (key == GLFW_KEY_ESCAPE) {
        panel_terminal_close(app);
        return;
    }
    int page = term_rows > 1 ? term_rows - 1 : 8;
    if (key == GLFW_KEY_PAGE_UP) {
        term_scroll += page;
        int max_scroll = term_line_count > 1 ? term_line_count - 1 : 0;
        if (term_scroll > max_scroll) term_scroll = max_scroll;
        return;
    }
    if (key == GLFW_KEY_PAGE_DOWN) {
        term_scroll -= page;
        if (term_scroll < 0) term_scroll = 0;
        return;
    }
    if (key == GLFW_KEY_HOME && (mods & GLFW_MOD_CONTROL)) {
        term_scroll = term_line_count > 0 ? term_line_count - 1 : 0;
        return;
    }
    if (key == GLFW_KEY_END && (mods & GLFW_MOD_CONTROL)) {
        term_scroll = 0;
        return;
    }

    switch (key) {
    case GLFW_KEY_ENTER: terminal_write_bytes("\r", 1); return;
    case GLFW_KEY_BACKSPACE: terminal_write_bytes("\x7f", 1); return;
    case GLFW_KEY_TAB: terminal_write_bytes("\t", 1); return;
    case GLFW_KEY_UP: terminal_write_bytes("\x1b[A", 3); return;
    case GLFW_KEY_DOWN: terminal_write_bytes("\x1b[B", 3); return;
    case GLFW_KEY_RIGHT: terminal_write_bytes("\x1b[C", 3); return;
    case GLFW_KEY_LEFT: terminal_write_bytes("\x1b[D", 3); return;
    case GLFW_KEY_DELETE: terminal_write_bytes("\x1b[3~", 4); return;
    case GLFW_KEY_HOME: terminal_write_bytes("\x1b[H", 3); return;
    case GLFW_KEY_END: terminal_write_bytes("\x1b[F", 3); return;
    default: break;
    }

    if ((mods & GLFW_MOD_CONTROL) && key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
        char c = (char)(key - GLFW_KEY_A + 1);
        terminal_write_bytes(&c, 1);
    }
}

void panel_terminal_input(App *app, unsigned int c) {
    (void)app;
    if (!term_open || c < 32) return;
    char out[4];
    size_t len = 0;
    if (c < 0x80) {
        out[len++] = (char)c;
    } else if (c < 0x800) {
        out[len++] = (char)(0xc0 | (c >> 6));
        out[len++] = (char)(0x80 | (c & 0x3f));
    } else if (c < 0x10000) {
        out[len++] = (char)(0xe0 | (c >> 12));
        out[len++] = (char)(0x80 | ((c >> 6) & 0x3f));
        out[len++] = (char)(0x80 | (c & 0x3f));
    } else if (c <= 0x10ffff) {
        out[len++] = (char)(0xf0 | (c >> 18));
        out[len++] = (char)(0x80 | ((c >> 12) & 0x3f));
        out[len++] = (char)(0x80 | ((c >> 6) & 0x3f));
        out[len++] = (char)(0x80 | (c & 0x3f));
    }
    if (len > 0) terminal_write_bytes(out, len);
}

void panel_terminal_render(Gui *g, App *app) {
    if (!term_open) return;
    terminal_poll();

    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int h = app_get_height(app);

    float px, py, pw, ph;
    int cols, rows;
    terminal_compute_layout(g, app, &px, &py, &pw, &ph, &cols, &rows);
    terminal_resize(cols, rows);

    renderer_draw_rect(r, px, py, pw, ph, 0.02f, 0.02f, 0.025f, 0.98f);
    renderer_draw_rect(r, px, py, pw, 2.0f, t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, px, py + ph - 1.0f, pw, 1.0f, 0, 0, 0, 0.8f);

    char title[128];
    snprintf(title, sizeof(title), "Terminal  %s  %dx%d  PageUp/PageDown scroll  Esc hide",
             terminal_running() ? "running" : "stopped", term_cols, term_rows);
    font_draw(&g->font, r, title, px + 12.0f, py + 8.0f,
              t->accent[0], t->accent[1], t->accent[2], 1.0f);

    float line_h = g->font.glyph_h + 4.0f;
    float text_y = py + 34.0f;
    int visible = rows;
    if (visible < 1) visible = 1;

    int max_scroll = term_line_count > visible ? term_line_count - visible : 0;
    if (term_scroll > max_scroll) term_scroll = max_scroll;
    if (term_scroll < 0) term_scroll = 0;

    int start = term_line_count - visible - term_scroll;
    if (start < 0) start = 0;

    glEnable(GL_SCISSOR_TEST);
    glScissor((int)px, h - (int)(py + ph), (int)pw, (int)ph);
    for (int i = 0; i < visible && start + i < term_line_count; i++) {
        font_draw(&g->font, r, term_lines[start + i], px + 12.0f, text_y + (float)i * line_h,
                  t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], 1.0f);
    }
    glDisable(GL_SCISSOR_TEST);
}
