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

static bool term_open = false;
static int term_fd = -1;
static pid_t term_pid = -1;
static char term_lines[TERM_MAX_LINES][TERM_MAX_COLS];
static int term_line_count = 1;
static int term_scroll = 0;
static int esc_state = 0;

static void terminal_reset_buffer(void) {
    memset(term_lines, 0, sizeof(term_lines));
    term_line_count = 1;
    term_scroll = 0;
    esc_state = 0;
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
}

static void terminal_append_char(char c) {
    char *line = term_lines[term_line_count - 1];
    size_t len = strlen(line);
    if (c == '\n') {
        terminal_append_newline();
        return;
    }
    if (c == '\r') {
        return;
    }
    if (c == '\b' || c == 127) {
        if (len > 0) line[len - 1] = '\0';
        return;
    }
    if (c == '\t') {
        int spaces = 4 - ((int)len % 4);
        for (int i = 0; i < spaces; i++) terminal_append_char(' ');
        return;
    }
    if ((unsigned char)c < 32) return;
    if (len + 1 >= TERM_MAX_COLS) {
        terminal_append_newline();
        line = term_lines[term_line_count - 1];
        len = 0;
    }
    line[len] = c;
    line[len + 1] = '\0';
}

static void terminal_consume_output(const char *buf, ssize_t len) {
    for (ssize_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)buf[i];
        if (esc_state) {
            if (esc_state == 1) {
                esc_state = (c == '[' || c == ']' || c == '(' || c == ')') ? 2 : 0;
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
        terminal_append_char((char)c);
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

static void terminal_resize(App *app) {
    if (term_fd < 0) return;
    int cols = app_get_width(app) / 8;
    int rows = app_get_height(app) / 28;
    if (cols < 20) cols = 20;
    if (rows < 6) rows = 6;
    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    ws.ws_col = (unsigned short)cols;
    ws.ws_row = (unsigned short)rows;
    ioctl(term_fd, TIOCSWINSZ, &ws);
}

static void terminal_spawn(App *app) {
    if (terminal_running()) return;
    terminal_reset_buffer();

    const char *shell = getenv("SHELL");
    if (!shell || !*shell) shell = "/bin/sh";

    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    ws.ws_col = (unsigned short)(app_get_width(app) / 8);
    ws.ws_row = (unsigned short)(app_get_height(app) / 28);
    if (ws.ws_col < 20) ws.ws_col = 20;
    if (ws.ws_row < 6) ws.ws_row = 6;

    term_pid = forkpty(&term_fd, NULL, NULL, &ws);
    if (term_pid == 0) {
        setenv("TERM", "xterm-256color", 1);
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
    terminal_resize(app);
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
    if (key == GLFW_KEY_PAGE_UP) {
        term_scroll += 8;
        int max_scroll = term_line_count > 1 ? term_line_count - 1 : 0;
        if (term_scroll > max_scroll) term_scroll = max_scroll;
        return;
    }
    if (key == GLFW_KEY_PAGE_DOWN) {
        term_scroll -= 8;
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
    default: break;
    }

    if ((mods & GLFW_MOD_CONTROL) && key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
        char c = (char)(key - GLFW_KEY_A + 1);
        terminal_write_bytes(&c, 1);
    }
}

void panel_terminal_input(App *app, unsigned int c) {
    (void)app;
    if (!term_open || c < 32 || c > 126) return;
    char out = (char)c;
    terminal_write_bytes(&out, 1);
}

void panel_terminal_render(Gui *g, App *app) {
    if (!term_open) return;
    terminal_poll();

    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);

    float ph = (float)h * 0.38f;
    if (ph < 160.0f) ph = 160.0f;
    if (ph > (float)h - 48.0f) ph = (float)h - 48.0f;
    float px = 0.0f;
    float py = (float)h - ph - 24.0f;
    float pw = (float)w;

    renderer_draw_rect(r, px, py, pw, ph, 0.02f, 0.02f, 0.025f, 0.98f);
    renderer_draw_rect(r, px, py, pw, 2.0f, t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, px, py + ph - 1.0f, pw, 1.0f, 0, 0, 0, 0.8f);

    char title[128];
    snprintf(title, sizeof(title), "Terminal  %s  PageUp/PageDown scroll  Esc hide",
             terminal_running() ? "running" : "stopped");
    font_draw(&g->font, r, title, px + 12.0f, py + 8.0f,
              t->accent[0], t->accent[1], t->accent[2], 1.0f);

    float line_h = g->font.glyph_h + 4.0f;
    float text_y = py + 34.0f;
    int visible = (int)((ph - 42.0f) / line_h);
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
