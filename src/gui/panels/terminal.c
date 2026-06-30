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
#define TERM_MIN_H 160.0f
#define TERM_HEADER_H 30.0f
#define TERM_FOOTER_H 22.0f
#define TERM_BODY_GAP 10.0f
#define TERM_PAD_X 12.0f

static bool term_open = false;
static int term_fd = -1;
static pid_t term_pid = -1;
static char term_lines[TERM_MAX_LINES][TERM_MAX_COLS];
static int term_line_count = 1;
static int term_scroll = 0;
static int term_col = 0;
static int term_cursor_line = 0;
static int term_saved_col = 0;
static int term_saved_cursor_line = 0;
static int esc_state = 0;
static char csi_buf[CSI_BUF_MAX];
static int csi_len = 0;
static int term_cols = 80;
static int term_rows = 12;
static int term_margin_top = 0;
static int term_margin_bottom = -1;
static bool term_alt_screen = false;
static bool term_cursor_visible = true;
static bool term_bracketed_paste = false;
static bool term_app_cursor_keys = false;
static char term_shell_name[64] = "shell";

static void terminal_reset_margins(void) {
    term_margin_top = 0;
    term_margin_bottom = -1;
}

static void terminal_reset_buffer(void) {
    memset(term_lines, 0, sizeof(term_lines));
    term_line_count = 1;
    term_scroll = 0;
    term_col = 0;
    term_cursor_line = 0;
    term_saved_col = 0;
    term_saved_cursor_line = 0;
    esc_state = 0;
    csi_len = 0;
    terminal_reset_margins();
    term_alt_screen = false;
    term_cursor_visible = true;
    term_bracketed_paste = false;
    term_app_cursor_keys = false;
}

static bool terminal_running(void) {
    if (term_pid <= 0) return false;
    int status = 0;
    pid_t result = waitpid(term_pid, &status, WNOHANG);
    if (result == 0) return true;
    if (result < 0 && errno == EINTR) return true;
    if (result == term_pid) {
        term_pid = -1;
        if (term_fd >= 0) {
            close(term_fd);
            term_fd = -1;
        }
    }
    return false;
}

static void terminal_live(void) {
    term_scroll = 0;
}

static void terminal_write_raw(const char *s, size_t len) {
    if (term_fd < 0) return;
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

static void terminal_append_newline(void) {
    if (term_cursor_line < term_line_count - 1) {
        term_cursor_line++;
    } else if (term_line_count < TERM_MAX_LINES) {
        term_lines[term_line_count][0] = '\0';
        term_line_count++;
        term_cursor_line = term_line_count - 1;
    } else {
        memmove(term_lines, term_lines + 1, sizeof(term_lines[0]) * (TERM_MAX_LINES - 1));
        term_lines[TERM_MAX_LINES - 1][0] = '\0';
        term_cursor_line = TERM_MAX_LINES - 1;
    }
    if (term_scroll > 0) term_scroll++;
    term_col = 0;
}

static void terminal_clear_screen(void) {
    memset(term_lines, 0, sizeof(term_lines));
    term_line_count = 1;
    term_scroll = 0;
    term_col = 0;
    term_cursor_line = 0;
    term_saved_col = 0;
    term_saved_cursor_line = 0;
    terminal_reset_margins();
}

static void terminal_clamp_cursor(void) {
    if (term_line_count < 1) {
        term_line_count = 1;
        term_lines[0][0] = '\0';
    }
    if (term_cursor_line < 0) term_cursor_line = 0;
    while (term_cursor_line >= term_line_count && term_line_count < TERM_MAX_LINES) {
        term_lines[term_line_count][0] = '\0';
        term_line_count++;
    }
    if (term_cursor_line >= term_line_count) term_cursor_line = term_line_count - 1;
    if (term_col < 0) term_col = 0;
    if (term_col >= TERM_MAX_COLS) term_col = TERM_MAX_COLS - 1;
}

static int terminal_screen_origin(void) {
    int origin = term_line_count - term_rows;
    return origin > 0 ? origin : 0;
}

static int terminal_screen_bottom(void) {
    int bottom = terminal_screen_origin() + term_rows;
    if (bottom > term_line_count) bottom = term_line_count;
    if (bottom < 1) bottom = 1;
    return bottom;
}

static int terminal_margin_bottom_row(void) {
    int bottom = term_margin_bottom >= 0 ? term_margin_bottom : term_rows - 1;
    if (bottom < 0) bottom = 0;
    if (bottom >= term_rows) bottom = term_rows - 1;
    return bottom;
}

static void terminal_region_bounds(int *start_out, int *end_out) {
    int origin = terminal_screen_origin();
    int top = term_margin_top;
    int bottom = terminal_margin_bottom_row();
    if (top < 0) top = 0;
    if (top >= term_rows) top = term_rows - 1;
    if (bottom < top) bottom = top;

    int start = origin + top;
    int end = origin + bottom + 1;
    int screen_end = terminal_screen_bottom();
    if (end > screen_end) end = screen_end;
    if (start >= end) start = end > 0 ? end - 1 : 0;

    if (start_out) *start_out = start;
    if (end_out) *end_out = end;
}

static void terminal_trim_blank_tail_after_cursor(void) {
    int keep = term_cursor_line + 1;
    if (keep < 1) keep = 1;
    while (term_line_count > keep && term_lines[term_line_count - 1][0] == '\0')
        term_line_count--;
    terminal_clamp_cursor();
}

static void terminal_clear_live_tail_for_input(void) {
    if (term_scroll != 0) return;
    if (term_cursor_line < 0 || term_cursor_line >= term_line_count) return;
    for (int i = term_cursor_line + 1; i < term_line_count; i++)
        term_lines[i][0] = '\0';
    terminal_trim_blank_tail_after_cursor();
}

static void terminal_set_cursor_position(int row, int col) {
    if (row < 1) row = 1;
    if (col < 1) col = 1;
    int target = terminal_screen_origin() + row - 1;
    if (target >= TERM_MAX_LINES) target = TERM_MAX_LINES - 1;
    term_cursor_line = target;
    term_col = col - 1;
    terminal_clamp_cursor();
}

static void terminal_set_margins(int top, int bottom) {
    if (top < 1) top = 1;
    if (bottom < 1 || bottom > term_rows) bottom = term_rows;
    if (top >= bottom) {
        terminal_reset_margins();
    } else {
        term_margin_top = top - 1;
        term_margin_bottom = bottom - 1;
    }
    terminal_set_cursor_position(1, 1);
}

static void terminal_save_cursor(void) {
    terminal_clamp_cursor();
    term_saved_cursor_line = term_cursor_line;
    term_saved_col = term_col;
}

static void terminal_restore_cursor(void) {
    term_cursor_line = term_saved_cursor_line;
    term_col = term_saved_col;
    terminal_clamp_cursor();
}

static void terminal_clear_line_from_cursor(void) {
    if (term_cursor_line < 0) term_cursor_line = 0;
    if (term_cursor_line >= term_line_count) term_cursor_line = term_line_count - 1;
    char *line = term_lines[term_cursor_line];
    size_t len = strlen(line);
    if (term_col < 0) term_col = 0;
    if (term_col < (int)len)
        line[term_col] = '\0';
}

static void terminal_clear_line_to_cursor(void) {
    if (term_cursor_line < 0) term_cursor_line = 0;
    if (term_cursor_line >= term_line_count) term_cursor_line = term_line_count - 1;
    char *line = term_lines[term_cursor_line];
    size_t len = strlen(line);
    int end = term_col;
    if (end < 0) end = 0;
    if (end >= TERM_MAX_COLS) end = TERM_MAX_COLS - 1;
    if (end >= (int)len)
        end = (int)len - 1;
    for (int i = 0; i <= end; i++)
        line[i] = ' ';
    while (len > 0 && line[len - 1] == ' ') {
        line[len - 1] = '\0';
        len--;
    }
}

static void terminal_clear_line(void) {
    if (term_cursor_line < 0) term_cursor_line = 0;
    if (term_cursor_line >= term_line_count) term_cursor_line = term_line_count - 1;
    term_lines[term_cursor_line][0] = '\0';
    term_col = 0;
}

static void terminal_clear_screen_from_cursor(void) {
    terminal_clear_line_from_cursor();
    int end = terminal_screen_bottom();
    for (int i = term_cursor_line + 1; i < end; i++)
        term_lines[i][0] = '\0';
    terminal_trim_blank_tail_after_cursor();
}

static void terminal_clear_screen_to_cursor(void) {
    int start = terminal_screen_origin();
    if (start < 0) start = 0;
    for (int i = start; i < term_cursor_line && i < term_line_count; i++)
        term_lines[i][0] = '\0';
    terminal_clear_line_to_cursor();
}

static void terminal_delete_lines(int n) {
    terminal_clamp_cursor();
    if (n < 1) n = 1;
    int origin, bottom;
    terminal_region_bounds(&origin, &bottom);
    if (term_cursor_line < origin || term_cursor_line >= bottom) return;
    if (n > bottom - term_cursor_line)
        n = bottom - term_cursor_line;
    for (int i = term_cursor_line; i + n < bottom; i++)
        memcpy(term_lines[i], term_lines[i + n], sizeof(term_lines[i]));
    for (int i = bottom - n; i < bottom; i++)
        term_lines[i][0] = '\0';
    terminal_trim_blank_tail_after_cursor();
}

static void terminal_insert_lines(int n) {
    terminal_clamp_cursor();
    if (n < 1) n = 1;
    int origin, bottom;
    terminal_region_bounds(&origin, &bottom);
    if (term_cursor_line < origin || term_cursor_line >= bottom) return;
    if (n > bottom - term_cursor_line)
        n = bottom - term_cursor_line;
    for (int i = bottom - 1; i - n >= term_cursor_line; i--)
        memcpy(term_lines[i], term_lines[i - n], sizeof(term_lines[i]));
    for (int i = term_cursor_line; i < term_cursor_line + n; i++)
        term_lines[i][0] = '\0';
}

static void terminal_scroll_up(int n) {
    if (n < 1) n = 1;
    int origin, bottom;
    terminal_region_bounds(&origin, &bottom);
    if (bottom <= origin) return;
    if (n > bottom - origin) n = bottom - origin;
    for (int i = origin; i + n < bottom; i++)
        memcpy(term_lines[i], term_lines[i + n], sizeof(term_lines[i]));
    for (int i = bottom - n; i < bottom; i++)
        term_lines[i][0] = '\0';
    terminal_trim_blank_tail_after_cursor();
}

static void terminal_scroll_down(int n) {
    if (n < 1) n = 1;
    int origin, bottom;
    terminal_region_bounds(&origin, &bottom);
    if (bottom <= origin) return;
    if (n > bottom - origin) n = bottom - origin;
    for (int i = bottom - 1; i - n >= origin; i--)
        memcpy(term_lines[i], term_lines[i - n], sizeof(term_lines[i]));
    for (int i = origin; i < origin + n; i++)
        term_lines[i][0] = '\0';
}

static void terminal_append_byte(char c) {
    terminal_clamp_cursor();
    char *line = term_lines[term_cursor_line];
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
        }
        return;
    }
    if (c == '\f') {
        terminal_clear_screen();
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
        line = term_lines[term_cursor_line];
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

static int csi_param_at(int index, int fallback) {
    int value = 0;
    bool any = false;
    int current = 0;
    for (int i = 0; i < csi_len; i++) {
        if (csi_buf[i] >= '0' && csi_buf[i] <= '9') {
            if (current == index) {
                value = value * 10 + (csi_buf[i] - '0');
                any = true;
            }
        } else if (csi_buf[i] == ';') {
            if (current == index)
                break;
            current++;
            value = 0;
            any = false;
        }
    }
    return any ? value : fallback;
}

static int csi_param(int fallback) {
    return csi_param_at(0, fallback);
}

static bool csi_private_mode(void) {
    return csi_len > 0 && csi_buf[0] == '?';
}

static int csi_first_digit_index(void) {
    int i = 0;
    while (i < csi_len && (csi_buf[i] < '0' || csi_buf[i] > '9') && csi_buf[i] != ';')
        i++;
    return i;
}

static int csi_parse_params(int *out, int max_params, int fallback) {
    if (!out || max_params <= 0) return 0;
    int count = 0;
    int value = 0;
    bool any = false;
    for (int i = csi_first_digit_index(); i <= csi_len; i++) {
        char ch = i < csi_len ? csi_buf[i] : ';';
        if (ch >= '0' && ch <= '9') {
            value = value * 10 + (ch - '0');
            any = true;
        } else if (ch == ';' || ch == ':') {
            if (count < max_params)
                out[count++] = any ? value : fallback;
            value = 0;
            any = false;
        }
    }
    if (count == 0) {
        out[count++] = fallback;
    }
    return count;
}

static void terminal_enter_alt_screen(void) {
    if (term_alt_screen) return;
    terminal_save_cursor();
    int saved_line = term_saved_cursor_line;
    int saved_col = term_saved_col;
    term_alt_screen = true;
    terminal_clear_screen();
    term_alt_screen = true;
    term_saved_cursor_line = saved_line;
    term_saved_col = saved_col;
}

static void terminal_leave_alt_screen(void) {
    if (!term_alt_screen) return;
    int saved_line = term_saved_cursor_line;
    int saved_col = term_saved_col;
    terminal_clear_screen();
    term_alt_screen = false;
    term_cursor_line = saved_line;
    term_col = saved_col;
    terminal_clamp_cursor();
}

static void terminal_handle_private_mode_value(int mode, bool enabled) {
    switch (mode) {
    case 1:
        term_app_cursor_keys = enabled;
        break;
    case 25:
        term_cursor_visible = enabled;
        break;
    case 47:
    case 1047:
    case 1049:
        if (enabled)
            terminal_enter_alt_screen();
        else
            terminal_leave_alt_screen();
        break;
    case 2004:
        term_bracketed_paste = enabled;
        break;
    default:
        break;
    }
}

static void terminal_handle_private_mode(bool enabled) {
    int modes[8];
    int count = csi_parse_params(modes, 8, 0);
    for (int i = 0; i < count; i++)
        terminal_handle_private_mode_value(modes[i], enabled);
}

static void terminal_delete_chars(int n) {
    terminal_clamp_cursor();
    if (n < 1) n = 1;
    char *line = term_lines[term_cursor_line];
    size_t len = strlen(line);
    int col = term_col;
    if (col < 0) col = 0;
    if (col >= TERM_MAX_COLS) col = TERM_MAX_COLS - 1;
    if (col >= (int)len) return;
    if (col + n >= (int)len) {
        line[col] = '\0';
        return;
    }
    memmove(line + col, line + col + n, len - (size_t)col - (size_t)n + 1);
}

static void terminal_insert_spaces(int n) {
    terminal_clamp_cursor();
    if (n < 1) n = 1;
    if (n > TERM_MAX_COLS - 1) n = TERM_MAX_COLS - 1;
    char *line = term_lines[term_cursor_line];
    size_t len = strlen(line);
    int col = term_col;
    if (col < 0) col = 0;
    if (col >= TERM_MAX_COLS) col = TERM_MAX_COLS - 1;
    if (col > (int)len) {
        int pad_to = col;
        if (pad_to > TERM_MAX_COLS - 2) pad_to = TERM_MAX_COLS - 2;
        while ((int)len < pad_to) {
            line[len++] = ' ';
            line[len] = '\0';
        }
    }
    int max_len = TERM_MAX_COLS - 1;
    if ((int)len + n > max_len)
        n = max_len - (int)len;
    if (n <= 0) return;
    memmove(line + col + n, line + col, len - (size_t)col + 1);
    memset(line + col, ' ', (size_t)n);
}

static void terminal_erase_chars(int n) {
    terminal_clamp_cursor();
    if (n < 1) n = 1;
    char *line = term_lines[term_cursor_line];
    size_t len = strlen(line);
    int col = term_col;
    if (col < 0) col = 0;
    if (col >= TERM_MAX_COLS - 1) return;
    int end = col + n;
    if (end > TERM_MAX_COLS - 1) end = TERM_MAX_COLS - 1;
    while ((int)len < end) {
        line[len++] = ' ';
        line[len] = '\0';
    }
    memset(line + col, ' ', (size_t)(end - col));
    while (len > 0 && line[len - 1] == ' ') {
        line[len - 1] = '\0';
        len--;
    }
}

static void terminal_move_cursor_line(int delta) {
    term_cursor_line += delta;
    if (term_cursor_line < 0) term_cursor_line = 0;
    while (term_cursor_line >= term_line_count && term_line_count < TERM_MAX_LINES) {
        term_lines[term_line_count][0] = '\0';
        term_line_count++;
    }
    if (term_cursor_line >= term_line_count) term_cursor_line = term_line_count - 1;
}

static void terminal_index(void) {
    int top, bottom;
    terminal_region_bounds(&top, &bottom);
    if (term_cursor_line >= bottom - 1) {
        terminal_scroll_up(1);
    } else {
        terminal_move_cursor_line(1);
    }
}

static void terminal_reverse_index(void) {
    int top, bottom;
    terminal_region_bounds(&top, &bottom);
    (void)bottom;
    if (term_cursor_line <= top) {
        terminal_scroll_down(1);
    } else {
        terminal_move_cursor_line(-1);
    }
}

static void terminal_handle_csi(char final) {
    int n = csi_param(1);
    if (n < 1) n = 1;

    switch (final) {
    case 'A':
        terminal_move_cursor_line(-n);
        break;
    case 'B':
        terminal_move_cursor_line(n);
        break;
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
    case '`':
        term_col = n - 1;
        if (term_col < 0) term_col = 0;
        if (term_col >= TERM_MAX_COLS) term_col = TERM_MAX_COLS - 1;
        break;
    case 'E':
        terminal_move_cursor_line(n);
        term_col = 0;
        break;
    case 'F':
        terminal_move_cursor_line(-n);
        term_col = 0;
        break;
    case 'H':
    case 'f':
        terminal_set_cursor_position(csi_param_at(0, 1), csi_param_at(1, 1));
        break;
    case 'h':
        if (csi_private_mode())
            terminal_handle_private_mode(true);
        break;
    case 'l':
        if (csi_private_mode())
            terminal_handle_private_mode(false);
        break;
    case 'd':
        terminal_set_cursor_position(csi_param_at(0, 1), term_col + 1);
        break;
    case 'r':
        terminal_set_margins(csi_param_at(0, 1), csi_param_at(1, term_rows));
        break;
    case '@':
        terminal_insert_spaces(n);
        break;
    case 'L':
        terminal_insert_lines(n);
        break;
    case 'M':
        terminal_delete_lines(n);
        break;
    case 'P':
        terminal_delete_chars(n);
        break;
    case 'X':
        terminal_erase_chars(n);
        break;
    case 'S':
        terminal_scroll_up(n);
        break;
    case 'T':
        terminal_scroll_down(n);
        break;
    case 'J':
        n = csi_param(0);
        if (n == 0)
            terminal_clear_screen_from_cursor();
        else if (n == 1)
            terminal_clear_screen_to_cursor();
        else if (n == 2 || n == 3)
            terminal_clear_screen();
        break;
    case 'K':
        n = csi_param(0);
        if (n == 0)
            terminal_clear_line_from_cursor();
        else if (n == 1)
            terminal_clear_line_to_cursor();
        else if (n == 2)
            terminal_clear_line();
        break;
    case 's':
        terminal_save_cursor();
        break;
    case 'u':
        terminal_restore_cursor();
        break;
    case 'n':
        n = csi_param(0);
        if (n == 5) {
            terminal_write_raw("\x1b[0n", 4);
        } else if (n == 6) {
            char reply[32];
            int row = term_cursor_line - terminal_screen_origin() + 1;
            int col = term_col + 1;
            if (row < 1) row = 1;
            if (row > term_rows) row = term_rows;
            if (col < 1) col = 1;
            snprintf(reply, sizeof(reply), "\x1b[%d;%dR", row, col);
            terminal_write_raw(reply, strlen(reply));
        }
        break;
    case 'm':
    case 'q':
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
                } else if (c == ']') {
                    esc_state = 3;
                } else if (c == '(' || c == ')') {
                    esc_state = 5;
                } else if (c == '7') {
                    terminal_save_cursor();
                    esc_state = 0;
                } else if (c == '8') {
                    terminal_restore_cursor();
                    esc_state = 0;
                } else if (c == 'c') {
                    terminal_clear_screen();
                    esc_state = 0;
                } else if (c == 'D') {
                    terminal_index();
                    esc_state = 0;
                } else if (c == 'E') {
                    terminal_index();
                    term_col = 0;
                    esc_state = 0;
                } else if (c == 'M') {
                    terminal_reverse_index();
                    esc_state = 0;
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
            if (esc_state == 3) {
                if (c == '\a') {
                    esc_state = 0;
                } else if (c == 0x1b) {
                    esc_state = 4;
                }
                continue;
            }
            if (esc_state == 4) {
                esc_state = c == '\\' ? 0 : 3;
                continue;
            }
            if (esc_state == 5 && (c >= '@' && c <= '~'))
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
    terminal_live();
    terminal_clear_live_tail_for_input();
    terminal_write_raw(s, len);
}

static void terminal_compute_layout(Gui *g, App *app,
                                    float *px, float *py, float *pw, float *ph,
                                    int *cols, int *rows) {
    int w = app_get_width(app);
    int h = app_get_height(app);
    float panel_h = (float)h * 0.38f;
    if (panel_h < TERM_MIN_H) panel_h = TERM_MIN_H;
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
        float body_h = panel_h - TERM_HEADER_H - TERM_FOOTER_H - TERM_BODY_GAP;
        *rows = (int)(body_h / line_h);
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
    terminal_reset_margins();
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
    const char *base = strrchr(shell, '/');
    snprintf(term_shell_name, sizeof(term_shell_name), "%s", base && base[1] ? base + 1 : shell);

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
        setenv("COLORTERM", "truecolor", 1);
        setenv("TERM_PROGRAM", "dragon", 1);
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

static void terminal_restart(App *app) {
    if (term_pid > 0) {
        kill(term_pid, SIGHUP);
        for (int i = 0; i < 8; i++) {
            pid_t r = waitpid(term_pid, NULL, WNOHANG);
            if (r == term_pid || (r < 0 && errno != EINTR))
                break;
            usleep(10000);
        }
    }
    if (term_fd >= 0) {
        close(term_fd);
        term_fd = -1;
    }
    term_pid = -1;
    terminal_spawn(app);
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

static int terminal_modifier_code(int mods) {
    bool shift = (mods & GLFW_MOD_SHIFT) != 0;
    bool alt = (mods & GLFW_MOD_ALT) != 0;
    bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    if (shift && alt && ctrl) return 8;
    if (alt && ctrl) return 7;
    if (shift && ctrl) return 6;
    if (ctrl) return 5;
    if (shift && alt) return 4;
    if (alt) return 3;
    if (shift) return 2;
    return 1;
}

static void terminal_write_csi_key(char final, int mods) {
    int code = terminal_modifier_code(mods);
    char seq[16];
    if (code <= 1) {
        snprintf(seq, sizeof(seq), "\x1b[%c", final);
    } else {
        snprintf(seq, sizeof(seq), "\x1b[1;%d%c", code, final);
    }
    terminal_write_bytes(seq, strlen(seq));
}

static void terminal_write_cursor_key(char normal_final, char app_final, int mods) {
    if ((mods & GLFW_MOD_ALT) && !(mods & (GLFW_MOD_CONTROL | GLFW_MOD_SHIFT))) {
        if (normal_final == 'C') {
            terminal_write_bytes("\033f", 2);
            return;
        }
        if (normal_final == 'D') {
            terminal_write_bytes("\033b", 2);
            return;
        }
    }
    if ((mods & (GLFW_MOD_SHIFT | GLFW_MOD_CONTROL | GLFW_MOD_ALT)) == 0 && term_app_cursor_keys) {
        char seq[3] = {'\x1b', 'O', app_final};
        terminal_write_bytes(seq, sizeof(seq));
        return;
    }
    terminal_write_csi_key(normal_final, mods);
}

static void terminal_paste_text(const char *text) {
    if (!text || !*text) return;
    if (term_bracketed_paste)
        terminal_write_bytes("\x1b[200~", 6);
    terminal_write_bytes(text, strlen(text));
    if (term_bracketed_paste)
        terminal_write_bytes("\x1b[201~", 6);
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
    if ((mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT) && key == GLFW_KEY_R) {
        terminal_restart(app);
        return;
    }
    if ((mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT) && key == GLFW_KEY_V) {
        terminal_paste_text(app_get_clipboard(app));
        return;
    }
    if (key == GLFW_KEY_ENTER && !terminal_running()) {
        terminal_spawn(app);
        return;
    }

    switch (key) {
    case GLFW_KEY_ENTER: terminal_write_bytes("\r", 1); return;
    case GLFW_KEY_BACKSPACE:
        if (mods & GLFW_MOD_ALT) terminal_write_bytes("\x1b\x7f", 2);
        else terminal_write_bytes("\x7f", 1);
        return;
    case GLFW_KEY_TAB:
        if (mods & GLFW_MOD_SHIFT) terminal_write_bytes("\x1b[Z", 3);
        else terminal_write_bytes("\t", 1);
        return;
    case GLFW_KEY_UP:
        terminal_write_cursor_key('A', 'A', mods);
        return;
    case GLFW_KEY_DOWN:
        terminal_write_cursor_key('B', 'B', mods);
        return;
    case GLFW_KEY_RIGHT:
        terminal_write_cursor_key('C', 'C', mods);
        return;
    case GLFW_KEY_LEFT:
        terminal_write_cursor_key('D', 'D', mods);
        return;
    case GLFW_KEY_DELETE: terminal_write_bytes("\x1b[3~", 4); return;
    case GLFW_KEY_INSERT: terminal_write_bytes("\x1b[2~", 4); return;
    case GLFW_KEY_HOME:
        if ((mods & (GLFW_MOD_SHIFT | GLFW_MOD_CONTROL | GLFW_MOD_ALT)) == 0 && term_app_cursor_keys)
            terminal_write_bytes("\x1bOH", 3);
        else
            terminal_write_csi_key('H', mods);
        return;
    case GLFW_KEY_END:
        if ((mods & (GLFW_MOD_SHIFT | GLFW_MOD_CONTROL | GLFW_MOD_ALT)) == 0 && term_app_cursor_keys)
            terminal_write_bytes("\x1bOF", 3);
        else
            terminal_write_csi_key('F', mods);
        return;
    default: break;
    }

    if (mods & GLFW_MOD_CONTROL) {
        char c = 0;
        switch (key) {
        case GLFW_KEY_SPACE:
        case GLFW_KEY_2:
            c = 0;
            terminal_write_bytes(&c, 1);
            return;
        case GLFW_KEY_LEFT_BRACKET:
        case GLFW_KEY_3:
            c = 0x1b;
            terminal_write_bytes(&c, 1);
            return;
        case GLFW_KEY_BACKSLASH:
        case GLFW_KEY_4:
            c = 0x1c;
            terminal_write_bytes(&c, 1);
            return;
        case GLFW_KEY_RIGHT_BRACKET:
        case GLFW_KEY_5:
            c = 0x1d;
            terminal_write_bytes(&c, 1);
            return;
        case GLFW_KEY_6:
            c = 0x1e;
            terminal_write_bytes(&c, 1);
            return;
        case GLFW_KEY_MINUS:
        case GLFW_KEY_SLASH:
        case GLFW_KEY_7:
            c = 0x1f;
            terminal_write_bytes(&c, 1);
            return;
        case GLFW_KEY_8:
            c = 0x7f;
            terminal_write_bytes(&c, 1);
            return;
        default:
            break;
        }
    }

    if ((mods & GLFW_MOD_CONTROL) && key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
        char c = (char)(key - GLFW_KEY_A + 1);
        terminal_write_bytes(&c, 1);
        return;
    }
}

void panel_terminal_input(App *app, unsigned int c) {
    (void)app;
    if (!term_open || c < 32) return;
    terminal_live();
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

    float header_h = TERM_HEADER_H;
    float footer_h = TERM_FOOTER_H;
    float pad_x = TERM_PAD_X;
    float line_h = g->font.glyph_h + 4.0f;
    float body_x = px + pad_x;
    float body_y = py + header_h + 6.0f;
    float body_w = pw - pad_x * 2.0f - 10.0f;
    float body_h = ph - header_h - footer_h - 10.0f;
    if (body_h < line_h) body_h = line_h;

    renderer_draw_rect(r, px, py - 6.0f, pw, 6.0f, 0.0f, 0.0f, 0.0f, 0.35f);
    renderer_draw_rect(r, px, py, pw, ph, 0.015f, 0.016f, 0.020f, 0.98f);
    renderer_draw_rect(r, px, py, pw, header_h, 0.045f, 0.045f, 0.060f, 0.98f);
    renderer_draw_rect(r, px, py + ph - footer_h, pw, footer_h, 0.035f, 0.035f, 0.045f, 0.98f);
    renderer_draw_rect(r, px, py, pw, 2.0f, t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, px, py + header_h, pw, 1.0f, t->accent[0], t->accent[1], t->accent[2], 0.28f);
    renderer_draw_rect(r, px, py + ph - footer_h, pw, 1.0f, t->accent[0], t->accent[1], t->accent[2], 0.18f);

    char title[128];
    bool running = terminal_running();
    snprintf(title, sizeof(title), "Terminal  %s", term_shell_name);
    font_draw(&g->font, r, title, px + pad_x, py + 7.0f,
              t->accent[0], t->accent[1], t->accent[2], 1.0f);

    char meta[128];
    snprintf(meta, sizeof(meta), "%s%s%s%s  %dx%d  %d lines",
             running ? "running" : "stopped",
             term_alt_screen ? "  alt" : "",
             term_bracketed_paste ? "  paste" : "",
             term_app_cursor_keys ? "  app-keys" : "",
             term_cols, term_rows, term_line_count);
    float meta_x = px + pw - font_text_width(&g->font, meta) - 14.0f;
    if (meta_x < px + 120.0f) meta_x = px + 120.0f;
    font_draw(&g->font, r, meta, meta_x, py + 8.0f,
              running ? 0.68f : t->warning[0],
              running ? 0.78f : t->warning[1],
              running ? 0.72f : t->warning[2],
              0.95f);

    int visible = rows;
    if (visible < 1) visible = 1;

    int max_scroll = term_line_count > visible ? term_line_count - visible : 0;
    if (term_scroll > max_scroll) term_scroll = max_scroll;
    if (term_scroll < 0) term_scroll = 0;

    int start = term_line_count - visible - term_scroll;
    if (start < 0) start = 0;

    renderer_draw_rect(r, body_x - 4.0f, body_y - 3.0f, body_w + 8.0f, body_h + 6.0f,
                       0.0f, 0.0f, 0.0f, 0.34f);

    glEnable(GL_SCISSOR_TEST);
    glScissor((int)body_x, h - (int)(body_y + body_h), (int)body_w, (int)body_h);
    for (int i = 0; i < visible && start + i < term_line_count; i++) {
        float y = body_y + (float)i * line_h;
        font_draw(&g->font, r, term_lines[start + i], body_x, y,
                  0.82f, 0.86f, 0.82f, 1.0f);
    }
    glDisable(GL_SCISSOR_TEST);

    if (term_cursor_visible && term_scroll == 0 && term_line_count > 0) {
        int cursor_line = term_cursor_line - start;
        if (cursor_line >= 0 && cursor_line < visible) {
            float cx = body_x + (float)term_col * g->font.glyph_w;
            float cy = body_y + (float)cursor_line * line_h;
            if (cx >= body_x && cx < body_x + body_w)
                renderer_draw_rect(r, cx, cy + 2.0f, 2.0f, line_h - 4.0f,
                                   t->accent[0], t->accent[1], t->accent[2], 0.85f);
        }
    }

    if (max_scroll > 0) {
        float sb_x = px + pw - 7.0f;
        float sb_y = body_y;
        float sb_h = body_h;
        float thumb_h = sb_h * (float)visible / (float)term_line_count;
        if (thumb_h < 18.0f) thumb_h = 18.0f;
        float travel = sb_h - thumb_h;
        float thumb_y = sb_y + travel * (float)(max_scroll - term_scroll) / (float)max_scroll;
        renderer_draw_rect(r, sb_x, sb_y, 3.0f, sb_h, 0.12f, 0.12f, 0.14f, 0.65f);
        renderer_draw_rect(r, sb_x, thumb_y, 3.0f, thumb_h,
                           t->accent[0], t->accent[1], t->accent[2], 0.72f);
    }

    char footer[160];
    if (term_scroll > 0)
        snprintf(footer, sizeof(footer), "scrolled +%d  PageDown: newer  Ctrl-End: live  typing returns live  Esc: hide", term_scroll);
    else if (!running)
        snprintf(footer, sizeof(footer), "stopped  Enter/Ctrl-Shift-r: restart  Ctrl-~: toggle  Esc: hide");
    else
        snprintf(footer, sizeof(footer), "live  PageUp/PageDown: scrollback  Ctrl-Shift-V: paste  Ctrl-Shift-r: restart  Esc: hide");
    font_draw(&g->font, r, footer, px + pad_x, py + ph - footer_h + 5.0f,
              0.62f, 0.66f, 0.68f, 0.95f);
}
