#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>

#include "dragon_editor/buffer.h"
#include "dragon_editor/cursor.h"
#include "dragon_editor/history.h"
#include "dragon_editor/mode.h"
#include "dragon_editor/syntax.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { printf("  %-50s ", #name); tests_run++; } while(0)
#define PASS() \
    do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) \
    do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)
#define ASSERT(cond) \
    do { if (!(cond)) { FAIL(#cond); return; } } while(0)
#define ASSERT_EQ_INT(a, b) \
    do { if ((a) != (b)) { char _buf[128]; snprintf(_buf, sizeof(_buf), \
        "%s == %s (%d != %d)", #a, #b, (a), (b)); FAIL(_buf); return; } } while(0)
#define ASSERT_EQ_SIZE(a, b) \
    do { if ((a) != (b)) { char _buf[128]; snprintf(_buf, sizeof(_buf), \
        "%s == %s (%zu != %zu)", #a, #b, (a), (b)); FAIL(_buf); return; } } while(0)
#define ASSERT_EQ_STR(a, b) \
    do { if (strcmp((a), (b)) != 0) { char _buf[256]; snprintf(_buf, sizeof(_buf), \
        "%s == \"%s\" (got \"%s\")", #b, (b), (a)); FAIL(_buf); return; } } while(0)
#define ASSERT_TRUE(cond) \
    do { if (!(cond)) { FAIL(#cond " expected true"); return; } } while(0)
#define ASSERT_FALSE(cond) \
    do { if (cond) { FAIL(#cond " expected false"); return; } } while(0)

/* ================================================================
 * BUFFER TESTS
 * ================================================================ */

static void test_buffer_init(void) {
    TEST(buffer_init);
    Buffer b;
    buffer_init(&b);
    ASSERT(b.text != NULL);
    ASSERT_EQ_SIZE(b.len, 0);
    ASSERT(b.cap > 0);
    buffer_free(&b);
    PASS();
}

static void test_buffer_free(void) {
    TEST(buffer_free);
    Buffer b;
    buffer_init(&b);
    buffer_free(&b);
    ASSERT(b.text == NULL);
    ASSERT_EQ_SIZE(b.len, 0);
    ASSERT_EQ_SIZE(b.cap, 0);
    PASS();
}

static void test_buffer_clear(void) {
    TEST(buffer_clear);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "hello", 5);
    ASSERT_EQ_SIZE(b.len, 5);
    buffer_clear(&b);
    ASSERT_EQ_SIZE(b.len, 0);
    ASSERT(b.text[0] == '\0');
    buffer_free(&b);
    PASS();
}

static void test_buffer_append(void) {
    TEST(buffer_append);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "hello", 5);
    ASSERT_EQ_SIZE(b.len, 5);
    ASSERT_EQ_STR(b.text, "hello");
    buffer_append(&b, " world", 6);
    ASSERT_EQ_SIZE(b.len, 11);
    ASSERT_EQ_STR(b.text, "hello world");
    buffer_free(&b);
    PASS();
}

static void test_buffer_insert_middle(void) {
    TEST(buffer_insert_middle);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "hello", 5);
    buffer_insert(&b, 3, "XY", 2);
    ASSERT_EQ_SIZE(b.len, 7);
    ASSERT_EQ_STR(b.text, "helXYlo");
    buffer_free(&b);
    PASS();
}

static void test_buffer_insert_at_start(void) {
    TEST(buffer_insert_at_start);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "hello", 5);
    buffer_insert(&b, 0, ">>", 2);
    ASSERT_EQ_SIZE(b.len, 7);
    ASSERT_EQ_STR(b.text, ">>hello");
    buffer_free(&b);
    PASS();
}

static void test_buffer_insert_at_end(void) {
    TEST(buffer_insert_at_end);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "hello", 5);
    buffer_insert(&b, 5, "!!", 2);
    ASSERT_EQ_SIZE(b.len, 7);
    ASSERT_EQ_STR(b.text, "hello!!");
    buffer_free(&b);
    PASS();
}

static void test_buffer_delete_middle(void) {
    TEST(buffer_delete_middle);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "hello", 5);
    buffer_delete(&b, 1, 3);
    ASSERT_EQ_SIZE(b.len, 2);
    ASSERT_EQ_STR(b.text, "ho");
    buffer_free(&b);
    PASS();
}

static void test_buffer_delete_from_start(void) {
    TEST(buffer_delete_from_start);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "hello", 5);
    buffer_delete(&b, 0, 3);
    ASSERT_EQ_SIZE(b.len, 2);
    ASSERT_EQ_STR(b.text, "lo");
    buffer_free(&b);
    PASS();
}

static void test_buffer_delete_clamps(void) {
    TEST(buffer_delete_clamps);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "hello", 5);
    buffer_delete(&b, 3, 100);
    ASSERT_EQ_SIZE(b.len, 3);
    ASSERT_EQ_STR(b.text, "hel");
    buffer_free(&b);
    PASS();
}

static void test_buffer_line_count_empty(void) {
    TEST(buffer_line_count_empty);
    Buffer b;
    buffer_init(&b);
    ASSERT_EQ_SIZE(buffer_line_count(&b), 1);
    buffer_free(&b);
    PASS();
}

static void test_buffer_line_count_single(void) {
    TEST(buffer_line_count_single);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "hello", 5);
    ASSERT_EQ_SIZE(buffer_line_count(&b), 1);
    buffer_free(&b);
    PASS();
}

static void test_buffer_line_count_multi(void) {
    TEST(buffer_line_count_multi);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "line1\nline2\nline3\n", 18);
    ASSERT_EQ_SIZE(buffer_line_count(&b), 4);
    buffer_free(&b);
    PASS();
}

static void test_buffer_line_count_trailing_newline(void) {
    TEST(buffer_line_count_trailing_newline);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "a\nb\n", 4);
    ASSERT_EQ_SIZE(buffer_line_count(&b), 3);
    buffer_free(&b);
    PASS();
}

static void test_buffer_line_len(void) {
    TEST(buffer_line_len);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "line1\nline2\nline3", 17);
    ASSERT_EQ_SIZE(buffer_line_len(&b, 0), 5);
    ASSERT_EQ_SIZE(buffer_line_len(&b, 1), 5);
    ASSERT_EQ_SIZE(buffer_line_len(&b, 2), 5);
    buffer_free(&b);
    PASS();
}

static void test_buffer_line_ptr(void) {
    TEST(buffer_line_ptr);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "line1\nline2\nline3", 17);
    ASSERT_EQ_STR(buffer_line_ptr(&b, 0), "line1\nline2\nline3");
    ASSERT_EQ_STR(buffer_line_ptr(&b, 1), "line2\nline3");
    ASSERT_EQ_STR(buffer_line_ptr(&b, 2), "line3");
    buffer_free(&b);
    PASS();
}

static void test_buffer_pos_from_row_col(void) {
    TEST(buffer_pos_from_row_col);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "abc\ndef\nghi", 11);
    ASSERT_EQ_SIZE(buffer_pos_from_row_col(&b, 0, 0), 0);
    ASSERT_EQ_SIZE(buffer_pos_from_row_col(&b, 0, 2), 2);
    ASSERT_EQ_SIZE(buffer_pos_from_row_col(&b, 1, 0), 4);
    ASSERT_EQ_SIZE(buffer_pos_from_row_col(&b, 1, 3), 7);
    ASSERT_EQ_SIZE(buffer_pos_from_row_col(&b, 2, 0), 8);
    ASSERT_EQ_SIZE(buffer_pos_from_row_col(&b, 2, 2), 10);
    buffer_free(&b);
    PASS();
}

static void test_buffer_row_col_from_pos(void) {
    TEST(buffer_row_col_from_pos);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "abc\ndef\nghi", 11);
    int row, col;
    buffer_row_col_from_pos(&b, 0, &row, &col);
    ASSERT_EQ_INT(row, 0); ASSERT_EQ_INT(col, 0);
    buffer_row_col_from_pos(&b, 3, &row, &col);
    ASSERT_EQ_INT(row, 0); ASSERT_EQ_INT(col, 3);
    buffer_row_col_from_pos(&b, 4, &row, &col);
    ASSERT_EQ_INT(row, 1); ASSERT_EQ_INT(col, 0);
    buffer_row_col_from_pos(&b, 8, &row, &col);
    ASSERT_EQ_INT(row, 2); ASSERT_EQ_INT(col, 0);
    buffer_free(&b);
    PASS();
}

static void test_buffer_roundtrip_row_col(void) {
    TEST(buffer_roundtrip_row_col);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "abc\ndef\nghi", 11);
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            size_t pos = buffer_pos_from_row_col(&b, row, col);
            int r2, c2;
            buffer_row_col_from_pos(&b, pos, &r2, &c2);
            ASSERT_EQ_INT(r2, row);
            ASSERT_EQ_INT(c2, col);
        }
    }
    buffer_free(&b);
    PASS();
}

static void test_buffer_load_save(void) {
    TEST(buffer_load_save);
    const char *tmppath = "/tmp/dragon_test_buffer.txt";
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "hello world\nline2\n", 18);
    ASSERT_TRUE(buffer_save(&b, tmppath));

    Buffer b2;
    buffer_init(&b2);
    ASSERT_TRUE(buffer_load(&b2, tmppath));
    ASSERT_EQ_SIZE(b2.len, b.len);
    ASSERT(memcmp(b2.text, b.text, b.len) == 0);
    buffer_free(&b);
    buffer_free(&b2);
    unlink(tmppath);
    PASS();
}

static void test_buffer_load_nonexistent(void) {
    TEST(buffer_load_nonexistent);
    Buffer b;
    buffer_init(&b);
    ASSERT_FALSE(buffer_load(&b, "/tmp/dragon_nonexistent_12345.txt"));
    buffer_free(&b);
    PASS();
}

static void test_buffer_insert_empty(void) {
    TEST(buffer_insert_empty);
    Buffer b;
    buffer_init(&b);
    buffer_insert(&b, 0, "test", 4);
    ASSERT_EQ_SIZE(b.len, 4);
    ASSERT_EQ_STR(b.text, "test");
    buffer_free(&b);
    PASS();
}

static void test_buffer_delete_all(void) {
    TEST(buffer_delete_all);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "hello", 5);
    buffer_delete(&b, 0, 5);
    ASSERT_EQ_SIZE(b.len, 0);
    ASSERT(b.text[0] == '\0');
    buffer_free(&b);
    PASS();
}

/* ================================================================
 * CURSOR TESTS
 * ================================================================ */

static void test_cursor_init(void) {
    TEST(cursor_init);
    Cursor c;
    cursor_init(&c);
    ASSERT_EQ_INT(c.row, 0);
    ASSERT_EQ_INT(c.col, 0);
    ASSERT_FALSE(c.has_selection);
    PASS();
}

static void test_cursor_set(void) {
    TEST(cursor_set);
    Cursor c;
    cursor_init(&c);
    cursor_set(&c, 5, 10);
    ASSERT_EQ_INT(c.row, 5);
    ASSERT_EQ_INT(c.col, 10);
    ASSERT_EQ_INT(c.desired_col, 10);
    PASS();
}

static void test_cursor_move_horizontal(void) {
    TEST(cursor_move_horizontal);
    Cursor c;
    cursor_init(&c);
    cursor_move(&c, 0, 5);
    ASSERT_EQ_INT(c.col, 5);
    ASSERT_EQ_INT(c.row, 0);
    cursor_move(&c, 0, -2);
    ASSERT_EQ_INT(c.col, 3);
    PASS();
}

static void test_cursor_move_vertical(void) {
    TEST(cursor_move_vertical);
    Cursor c;
    cursor_init(&c);
    cursor_set(&c, 3, 5);
    cursor_move(&c, 2, 0);
    ASSERT_EQ_INT(c.row, 5);
    ASSERT_EQ_INT(c.col, 5);
    cursor_move(&c, -1, 0);
    ASSERT_EQ_INT(c.row, 4);
    ASSERT_EQ_INT(c.col, 5);
    PASS();
}

static void test_cursor_move_clamp_negative(void) {
    TEST(cursor_move_clamp_negative);
    Cursor c;
    cursor_init(&c);
    cursor_move(&c, -5, -5);
    ASSERT_EQ_INT(c.row, 0);
    ASSERT_EQ_INT(c.col, 0);
    PASS();
}

static void test_cursor_move_to(void) {
    TEST(cursor_move_to);
    Cursor c;
    cursor_init(&c);
    cursor_move_to(&c, 10, 20);
    ASSERT_EQ_INT(c.row, 10);
    ASSERT_EQ_INT(c.col, 20);
    ASSERT_EQ_INT(c.desired_col, 20);
    cursor_move_to(&c, -3, -5);
    ASSERT_EQ_INT(c.row, 0);
    ASSERT_EQ_INT(c.col, 0);
    PASS();
}

static void test_cursor_select_start(void) {
    TEST(cursor_select_start);
    Cursor c;
    cursor_init(&c);
    cursor_set(&c, 3, 5);
    cursor_select_start(&c);
    ASSERT_TRUE(c.has_selection);
    ASSERT_EQ_INT(c.anchor_row, 3);
    ASSERT_EQ_INT(c.anchor_col, 5);
    PASS();
}

static void test_cursor_clear_selection(void) {
    TEST(cursor_clear_selection);
    Cursor c;
    cursor_init(&c);
    cursor_set(&c, 3, 5);
    cursor_select_start(&c);
    ASSERT_TRUE(c.has_selection);
    cursor_clear_selection(&c);
    ASSERT_FALSE(c.has_selection);
    ASSERT_EQ_INT(c.anchor_row, 3);
    ASSERT_EQ_INT(c.anchor_col, 5);
    PASS();
}

static void test_cursor_has_selection(void) {
    TEST(cursor_has_selection);
    Cursor c;
    cursor_init(&c);
    ASSERT_FALSE(cursor_has_selection(&c));
    cursor_select_start(&c);
    ASSERT_TRUE(cursor_has_selection(&c));
    PASS();
}

static void test_cursor_normalize_forward(void) {
    TEST(cursor_normalize_forward);
    Cursor c;
    cursor_init(&c);
    c.anchor_row = 1; c.anchor_col = 3;
    c.row = 2; c.col = 5;
    int sr, sc, er, ec;
    cursor_normalize(&c, &sr, &sc, &er, &ec);
    ASSERT_EQ_INT(sr, 1); ASSERT_EQ_INT(sc, 3);
    ASSERT_EQ_INT(er, 2); ASSERT_EQ_INT(ec, 5);
    PASS();
}

static void test_cursor_normalize_backward(void) {
    TEST(cursor_normalize_backward);
    Cursor c;
    cursor_init(&c);
    c.anchor_row = 2; c.anchor_col = 5;
    c.row = 1; c.col = 3;
    int sr, sc, er, ec;
    cursor_normalize(&c, &sr, &sc, &er, &ec);
    ASSERT_EQ_INT(sr, 1); ASSERT_EQ_INT(sc, 3);
    ASSERT_EQ_INT(er, 2); ASSERT_EQ_INT(ec, 5);
    PASS();
}

static void test_cursor_normalize_same_line(void) {
    TEST(cursor_normalize_same_line);
    Cursor c;
    cursor_init(&c);
    c.anchor_row = 2; c.anchor_col = 5;
    c.row = 2; c.col = 3;
    int sr, sc, er, ec;
    cursor_normalize(&c, &sr, &sc, &er, &ec);
    ASSERT_EQ_INT(sr, 2); ASSERT_EQ_INT(sc, 3);
    ASSERT_EQ_INT(er, 2); ASSERT_EQ_INT(ec, 5);
    PASS();
}

static void test_cursor_desired_col_vertical(void) {
    TEST(cursor_desired_col_vertical);
    Cursor c;
    cursor_init(&c);
    cursor_set(&c, 5, 10);
    cursor_move(&c, -3, 0);
    ASSERT_EQ_INT(c.col, 10);
    cursor_move(&c, 0, -3);
    ASSERT_EQ_INT(c.col, 7);
    cursor_move(&c, 1, 0);
    ASSERT_EQ_INT(c.col, 7);
    cursor_move(&c, -1, 0);
    ASSERT_EQ_INT(c.col, 7);
    PASS();
}

/* ================================================================
 * HISTORY TESTS
 * ================================================================ */

static void test_history_init(void) {
    TEST(history_init);
    History h;
    history_init(&h);
    ASSERT_EQ_INT(h.count, 0);
    ASSERT_EQ_INT(h.current, -1);
    ASSERT_FALSE(history_can_undo(&h));
    ASSERT_FALSE(history_can_redo(&h));
    history_free(&h);
    PASS();
}

static void test_history_push_insert(void) {
    TEST(history_push_insert);
    History h;
    history_init(&h);
    history_push_insert(&h, 0, "abc", 3, 0, 0);
    ASSERT_TRUE(history_can_undo(&h));
    ASSERT_FALSE(history_can_redo(&h));
    ASSERT_EQ_INT(h.count, 1);
    HistoryEntry *e = history_peek_undo(&h);
    ASSERT(e != NULL);
    ASSERT_EQ_INT(e->type, OP_INSERT);
    ASSERT_EQ_SIZE(e->pos, 0);
    ASSERT_EQ_SIZE(e->len, 3);
    ASSERT(memcmp(e->text, "abc", 3) == 0);
    history_free(&h);
    PASS();
}

static void test_history_push_delete(void) {
    TEST(history_push_delete);
    History h;
    history_init(&h);
    history_push_delete(&h, 5, "xyz", 3, 1, 2);
    ASSERT_TRUE(history_can_undo(&h));
    HistoryEntry *e = history_peek_undo(&h);
    ASSERT(e != NULL);
    ASSERT_EQ_INT(e->type, OP_DELETE);
    ASSERT_EQ_SIZE(e->pos, 5);
    ASSERT_EQ_SIZE(e->len, 3);
    ASSERT(memcmp(e->text, "xyz", 3) == 0);
    ASSERT_EQ_INT(e->cursor_row, 1);
    ASSERT_EQ_INT(e->cursor_col, 2);
    history_free(&h);
    PASS();
}

static void test_history_advance_regress(void) {
    TEST(history_advance_regress);
    History h;
    history_init(&h);
    history_push_insert(&h, 0, "a", 1, 0, 0);
    history_push_insert(&h, 1, "b", 1, 0, 1);
    history_push_insert(&h, 2, "c", 1, 0, 2);

    ASSERT_EQ_INT(h.current, 2);

    history_regress(&h);
    ASSERT_EQ_INT(h.current, 1);
    ASSERT_TRUE(history_can_redo(&h));

    history_regress(&h);
    ASSERT_EQ_INT(h.current, 0);

    history_advance(&h);
    ASSERT_EQ_INT(h.current, 1);

    history_advance(&h);
    ASSERT_EQ_INT(h.current, 2);
    ASSERT_FALSE(history_can_redo(&h));

    history_free(&h);
    PASS();
}

static void test_history_clear(void) {
    TEST(history_clear);
    History h;
    history_init(&h);
    history_push_insert(&h, 0, "a", 1, 0, 0);
    history_push_insert(&h, 1, "b", 1, 0, 1);
    history_clear(&h);
    ASSERT_EQ_INT(h.count, 0);
    ASSERT_EQ_INT(h.current, -1);
    ASSERT_FALSE(history_can_undo(&h));
    history_free(&h);
    PASS();
}

static void test_history_redo_discarded_on_new_push(void) {
    TEST(history_redo_discarded_on_new_push);
    History h;
    history_init(&h);
    history_push_insert(&h, 0, "a", 1, 0, 0);
    history_push_insert(&h, 1, "b", 1, 0, 1);
    history_regress(&h);
    ASSERT_TRUE(history_can_redo(&h));
    history_push_insert(&h, 1, "x", 1, 0, 1);
    ASSERT_FALSE(history_can_redo(&h));
    ASSERT_EQ_INT(h.count, 2);
    history_free(&h);
    PASS();
}

static void test_history_peek_undo_null(void) {
    TEST(history_peek_undo_null);
    History h;
    history_init(&h);
    ASSERT(history_peek_undo(&h) == NULL);
    history_free(&h);
    PASS();
}

static void test_history_peek_redo_null(void) {
    TEST(history_peek_redo_null);
    History h;
    history_init(&h);
    ASSERT(history_peek_redo(&h) == NULL);
    history_push_insert(&h, 0, "a", 1, 0, 0);
    ASSERT(history_peek_redo(&h) == NULL);
    history_free(&h);
    PASS();
}

static void test_history_advance_bounds(void) {
    TEST(history_advance_bounds);
    History h;
    history_init(&h);
    history_push_insert(&h, 0, "a", 1, 0, 0);
    history_advance(&h);
    ASSERT_EQ_INT(h.current, 0);
    history_free(&h);
    PASS();
}

static void test_history_regress_bounds(void) {
    TEST(history_regress_bounds);
    History h;
    history_init(&h);
    history_regress(&h);
    ASSERT_EQ_INT(h.current, -1);
    history_free(&h);
    PASS();
}

static void test_history_large(void) {
    TEST(history_large);
    History h;
    history_init(&h);
    char text[2] = {0, 0};
    for (int i = 0; i < 200; i++) {
        text[0] = 'a' + (i % 26);
        history_push_insert(&h, i, text, 1, 0, i);
    }
    ASSERT_EQ_INT(h.count, 200);
    ASSERT_EQ_INT(h.current, 199);
    for (int i = 0; i < 50; i++) history_regress(&h);
    ASSERT_EQ_INT(h.current, 149);
    history_free(&h);
    PASS();
}

/* ================================================================
 * MODE TESTS
 * ================================================================ */

static void test_mode_init(void) {
    TEST(mode_init);
    ModeState ms;
    mode_init(&ms);
    ASSERT_EQ_INT(ms.current, MODE_NORMAL);
    ASSERT_EQ_INT(ms.previous, MODE_NORMAL);
    PASS();
}

static void test_mode_set_get(void) {
    TEST(mode_set_get);
    ModeState ms;
    mode_init(&ms);
    mode_set(&ms, MODE_INSERT);
    ASSERT_EQ_INT(mode_get(&ms), MODE_INSERT);
    ASSERT_EQ_INT(ms.previous, MODE_NORMAL);
    mode_set(&ms, MODE_SELECT);
    ASSERT_EQ_INT(mode_get(&ms), MODE_SELECT);
    ASSERT_EQ_INT(ms.previous, MODE_INSERT);
    PASS();
}

static void test_mode_is(void) {
    TEST(mode_is);
    ModeState ms;
    mode_init(&ms);
    ASSERT_TRUE(mode_is(&ms, MODE_NORMAL));
    ASSERT_FALSE(mode_is(&ms, MODE_INSERT));
    mode_set(&ms, MODE_INSERT);
    ASSERT_FALSE(mode_is(&ms, MODE_NORMAL));
    ASSERT_TRUE(mode_is(&ms, MODE_INSERT));
    PASS();
}

static void test_mode_select_initialized_flag(void) {
    TEST(mode_select_initialized_flag);
    ModeState ms;
    mode_init(&ms);
    ms.select_mode_initialized = true;
    mode_set(&ms, MODE_NORMAL);
    ASSERT_FALSE(ms.select_mode_initialized);
    ms.select_mode_initialized = true;
    mode_set(&ms, MODE_SELECT);
    ASSERT_TRUE(ms.select_mode_initialized);
    mode_set(&ms, MODE_COMMAND);
    ASSERT_FALSE(ms.select_mode_initialized);
    PASS();
}

static void test_mode_suppress_next_char(void) {
    TEST(mode_suppress_next_char);
    ModeState ms;
    mode_init(&ms);
    ms.suppress_next_char = false;
    mode_set(&ms, MODE_INSERT);
    ASSERT_TRUE(ms.suppress_next_char);
    PASS();
}

static void test_mode_same_mode_no_suppress(void) {
    TEST(mode_same_mode_no_suppress);
    ModeState ms;
    mode_init(&ms);
    ms.suppress_next_char = false;
    mode_set(&ms, MODE_NORMAL);
    ASSERT_FALSE(ms.suppress_next_char);
    PASS();
}

/* ================================================================
 * SYNTAX TESTS
 * ================================================================ */

static void test_syntax_init(void) {
    TEST(syntax_init);
    SyntaxHighlighting sh;
    syntax_init(&sh, "c");
    ASSERT(sh.tokens == NULL);
    ASSERT_EQ_INT(sh.token_count, 0);
    ASSERT(sh.language_id != NULL);
    ASSERT_EQ_STR(sh.language_id, "c");
    syntax_free(&sh);
    PASS();
}

static void test_syntax_init_no_lang(void) {
    TEST(syntax_init_no_lang);
    SyntaxHighlighting sh;
    syntax_init(&sh, NULL);
    ASSERT(sh.language_id == NULL);
    syntax_free(&sh);
    PASS();
}

static void test_syntax_add_token(void) {
    TEST(syntax_add_token);
    SyntaxHighlighting sh;
    syntax_init(&sh, NULL);
    syntax_add_token(&sh, 0, 0, 0, 5, SYNTAX_KEYWORD);
    ASSERT_EQ_INT(sh.token_count, 1);
    ASSERT_EQ_INT(sh.tokens[0].start_row, 0);
    ASSERT_EQ_INT(sh.tokens[0].start_col, 0);
    ASSERT_EQ_INT(sh.tokens[0].end_row, 0);
    ASSERT_EQ_INT(sh.tokens[0].end_col, 5);
    ASSERT_EQ_INT(sh.tokens[0].type, SYNTAX_KEYWORD);
    syntax_free(&sh);
    PASS();
}

static void test_syntax_add_multiple_tokens(void) {
    TEST(syntax_add_multiple_tokens);
    SyntaxHighlighting sh;
    syntax_init(&sh, NULL);
    syntax_add_token(&sh, 0, 0, 0, 3, SYNTAX_KEYWORD);
    syntax_add_token(&sh, 0, 5, 0, 10, SYNTAX_STRING);
    syntax_add_token(&sh, 1, 0, 1, 8, SYNTAX_COMMENT);
    ASSERT_EQ_INT(sh.token_count, 3);
    syntax_free(&sh);
    PASS();
}

static void test_syntax_get_type_at(void) {
    TEST(syntax_get_type_at);
    SyntaxHighlighting sh;
    syntax_init(&sh, NULL);
    syntax_add_token(&sh, 0, 0, 0, 5, SYNTAX_KEYWORD);
    syntax_add_token(&sh, 0, 6, 0, 12, SYNTAX_STRING);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, 0), SYNTAX_KEYWORD);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, 3), SYNTAX_KEYWORD);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, 4), SYNTAX_KEYWORD);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, 6), SYNTAX_STRING);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, 11), SYNTAX_STRING);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, 12), SYNTAX_NORMAL);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 1, 0), SYNTAX_NORMAL);
    syntax_free(&sh);
    PASS();
}

static void test_syntax_get_type_at_empty(void) {
    TEST(syntax_get_type_at_empty);
    SyntaxHighlighting sh;
    syntax_init(&sh, NULL);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, 0), SYNTAX_NORMAL);
    syntax_free(&sh);
    PASS();
}

static void test_syntax_clear(void) {
    TEST(syntax_clear);
    SyntaxHighlighting sh;
    syntax_init(&sh, NULL);
    syntax_add_token(&sh, 0, 0, 0, 5, SYNTAX_KEYWORD);
    syntax_add_token(&sh, 1, 0, 1, 3, SYNTAX_NUMBER);
    syntax_clear(&sh);
    ASSERT_EQ_INT(sh.token_count, 0);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, 0), SYNTAX_NORMAL);
    syntax_free(&sh);
    PASS();
}

static void test_syntax_add_token_rejects_normal(void) {
    TEST(syntax_add_token_rejects_normal);
    SyntaxHighlighting sh;
    syntax_init(&sh, NULL);
    syntax_add_token(&sh, 0, 0, 0, 5, SYNTAX_NORMAL);
    ASSERT_EQ_INT(sh.token_count, 0);
    syntax_free(&sh);
    PASS();
}

static void test_syntax_add_token_rejects_invalid(void) {
    TEST(syntax_add_token_rejects_invalid);
    SyntaxHighlighting sh;
    syntax_init(&sh, NULL);
    syntax_add_token(&sh, -1, 0, 0, 5, SYNTAX_KEYWORD);
    ASSERT_EQ_INT(sh.token_count, 0);
    syntax_add_token(&sh, 0, 5, 0, 3, SYNTAX_KEYWORD);
    ASSERT_EQ_INT(sh.token_count, 0);
    syntax_add_token(&sh, 2, 0, 1, 0, SYNTAX_KEYWORD);
    ASSERT_EQ_INT(sh.token_count, 0);
    syntax_free(&sh);
    PASS();
}

static void test_syntax_get_type_negative_coords(void) {
    TEST(syntax_get_type_negative_coords);
    SyntaxHighlighting sh;
    syntax_init(&sh, NULL);
    syntax_add_token(&sh, 0, 0, 0, 5, SYNTAX_KEYWORD);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, -1, 0), SYNTAX_NORMAL);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, -1), SYNTAX_NORMAL);
    syntax_free(&sh);
    PASS();
}

static void test_syntax_overlapping_tokens_smallest_wins(void) {
    TEST(syntax_overlapping_tokens_smallest_wins);
    SyntaxHighlighting sh;
    syntax_init(&sh, NULL);
    syntax_add_token(&sh, 0, 0, 0, 10, SYNTAX_KEYWORD);
    syntax_add_token(&sh, 0, 2, 0, 5, SYNTAX_STRING);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, 3), SYNTAX_STRING);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, 0), SYNTAX_KEYWORD);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, 6), SYNTAX_KEYWORD);
    syntax_free(&sh);
    PASS();
}

/* ================================================================
 * MAIN
 * ================================================================ */

int main(void) {
    printf("=== Dragon Editor CTests ===\n\n");

    printf("[Buffer Tests]\n");
    test_buffer_init();
    test_buffer_free();
    test_buffer_clear();
    test_buffer_append();
    test_buffer_insert_middle();
    test_buffer_insert_at_start();
    test_buffer_insert_at_end();
    test_buffer_insert_empty();
    test_buffer_delete_middle();
    test_buffer_delete_from_start();
    test_buffer_delete_clamps();
    test_buffer_delete_all();
    test_buffer_line_count_empty();
    test_buffer_line_count_single();
    test_buffer_line_count_multi();
    test_buffer_line_count_trailing_newline();
    test_buffer_line_len();
    test_buffer_line_ptr();
    test_buffer_pos_from_row_col();
    test_buffer_row_col_from_pos();
    test_buffer_roundtrip_row_col();
    test_buffer_load_save();
    test_buffer_load_nonexistent();

    printf("\n[Cursor Tests]\n");
    test_cursor_init();
    test_cursor_set();
    test_cursor_move_horizontal();
    test_cursor_move_vertical();
    test_cursor_move_clamp_negative();
    test_cursor_move_to();
    test_cursor_select_start();
    test_cursor_clear_selection();
    test_cursor_has_selection();
    test_cursor_normalize_forward();
    test_cursor_normalize_backward();
    test_cursor_normalize_same_line();
    test_cursor_desired_col_vertical();

    printf("\n[History Tests]\n");
    test_history_init();
    test_history_push_insert();
    test_history_push_delete();
    test_history_advance_regress();
    test_history_clear();
    test_history_redo_discarded_on_new_push();
    test_history_peek_undo_null();
    test_history_peek_redo_null();
    test_history_advance_bounds();
    test_history_regress_bounds();
    test_history_large();

    printf("\n[Mode Tests]\n");
    test_mode_init();
    test_mode_set_get();
    test_mode_is();
    test_mode_select_initialized_flag();
    test_mode_suppress_next_char();
    test_mode_same_mode_no_suppress();

    printf("\n[Syntax Tests]\n");
    test_syntax_init();
    test_syntax_init_no_lang();
    test_syntax_add_token();
    test_syntax_add_multiple_tokens();
    test_syntax_get_type_at();
    test_syntax_get_type_at_empty();
    test_syntax_clear();
    test_syntax_add_token_rejects_normal();
    test_syntax_add_token_rejects_invalid();
    test_syntax_get_type_negative_coords();
    test_syntax_overlapping_tokens_smallest_wins();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
