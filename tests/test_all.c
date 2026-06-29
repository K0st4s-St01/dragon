#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

#include "dragon_editor/buffer.h"
#include "dragon_editor/cursor.h"
#include "dragon_editor/history.h"
#include "dragon_editor/mode.h"
#include "dragon_editor/syntax.h"
#include "dragon_editor/document.h"
#include "dragon_editor/lsp.h"
#include "dragon_editor/config.h"
#include "dragon_editor/theme.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { printf("  %-55s ", #name); tests_run++; } while(0)
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

static Document *make_doc(const char *text) {
    Document *doc = calloc(1, sizeof(Document));
    document_init(doc);
    if (text) {
        buffer_append(&doc->buffer, text, strlen(text));
    }
    return doc;
}

static void free_doc(Document *doc) {
    document_free(doc);
    free(doc);
}

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

static void test_buffer_append_empty(void) {
    TEST(buffer_append_empty);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "", 0);
    ASSERT_EQ_SIZE(b.len, 0);
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

static void test_buffer_insert_zero_len(void) {
    TEST(buffer_insert_zero_len);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "hello", 5);
    buffer_insert(&b, 2, "XY", 0);
    ASSERT_EQ_SIZE(b.len, 5);
    ASSERT_EQ_STR(b.text, "hello");
    buffer_free(&b);
    PASS();
}

static void test_buffer_insert_past_end(void) {
    TEST(buffer_insert_past_end);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "hello", 5);
    buffer_insert(&b, 100, "!", 1);
    ASSERT_EQ_SIZE(b.len, 6);
    ASSERT_EQ_STR(b.text, "hello!");
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

static void test_buffer_delete_zero_len(void) {
    TEST(buffer_delete_zero_len);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "hello", 5);
    buffer_delete(&b, 2, 0);
    ASSERT_EQ_SIZE(b.len, 5);
    ASSERT_EQ_STR(b.text, "hello");
    buffer_free(&b);
    PASS();
}

static void test_buffer_delete_from_empty(void) {
    TEST(buffer_delete_from_empty);
    Buffer b;
    buffer_init(&b);
    buffer_delete(&b, 0, 5);
    ASSERT_EQ_SIZE(b.len, 0);
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

static void test_buffer_line_count_newlines_only(void) {
    TEST(buffer_line_count_newlines_only);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "\n\n\n", 3);
    ASSERT_EQ_SIZE(buffer_line_count(&b), 4);
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

static void test_buffer_line_len_empty_line(void) {
    TEST(buffer_line_len_empty_line);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "a\n\nb", 4);
    ASSERT_EQ_SIZE(buffer_line_len(&b, 0), 1);
    ASSERT_EQ_SIZE(buffer_line_len(&b, 1), 0);
    ASSERT_EQ_SIZE(buffer_line_len(&b, 2), 1);
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

static void test_buffer_pos_from_row_col_negative(void) {
    TEST(buffer_pos_from_row_col_negative);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "abc", 3);
    ASSERT_EQ_SIZE(buffer_pos_from_row_col(&b, -1, 0), 0);
    ASSERT_EQ_SIZE(buffer_pos_from_row_col(&b, 0, -1), 0);
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

static void test_buffer_row_col_from_pos_past_end(void) {
    TEST(buffer_row_col_from_pos_past_end);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "abc", 3);
    int row, col;
    buffer_row_col_from_pos(&b, 100, &row, &col);
    ASSERT_EQ_INT(row, 0);
    ASSERT_EQ_INT(col, 3);
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

static void test_buffer_insert_delete_sequence(void) {
    TEST(buffer_insert_delete_sequence);
    Buffer b;
    buffer_init(&b);
    buffer_append(&b, "ac", 2);
    buffer_insert(&b, 1, "b", 1);
    ASSERT_EQ_STR(b.text, "abc");
    buffer_delete(&b, 1, 1);
    ASSERT_EQ_STR(b.text, "ac");
    buffer_free(&b);
    PASS();
}

static void test_buffer_large_insert(void) {
    TEST(buffer_large_insert);
    Buffer b;
    buffer_init(&b);
    char big[4096];
    memset(big, 'A', sizeof(big));
    buffer_append(&b, big, sizeof(big));
    ASSERT_EQ_SIZE(b.len, 4096);
    for (int i = 0; i < 4096; i++) ASSERT(b.text[i] == 'A');
    buffer_free(&b);
    PASS();
}

static void test_buffer_insert_triggers_realloc(void) {
    TEST(buffer_insert_triggers_realloc);
    Buffer b;
    buffer_init(&b);
    size_t old_cap = b.cap;
    char big[8192];
    memset(big, 'X', sizeof(big));
    buffer_append(&b, big, sizeof(big));
    ASSERT(b.cap > old_cap);
    ASSERT_EQ_SIZE(b.len, 8192);
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

static void test_cursor_select_end_noop(void) {
    TEST(cursor_select_end_noop);
    Cursor c;
    cursor_init(&c);
    cursor_set(&c, 3, 5);
    cursor_select_start(&c);
    cursor_move(&c, 0, 3);
    cursor_select_end(&c);
    ASSERT_TRUE(c.has_selection);
    ASSERT_EQ_INT(c.row, 3);
    ASSERT_EQ_INT(c.col, 8);
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

static void test_cursor_normalize_same_position(void) {
    TEST(cursor_normalize_same_position);
    Cursor c;
    cursor_init(&c);
    c.anchor_row = 2; c.anchor_col = 3;
    c.row = 2; c.col = 3;
    int sr, sc, er, ec;
    cursor_normalize(&c, &sr, &sc, &er, &ec);
    ASSERT_EQ_INT(sr, 2); ASSERT_EQ_INT(sc, 3);
    ASSERT_EQ_INT(er, 2); ASSERT_EQ_INT(ec, 3);
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

static void test_cursor_move_zero(void) {
    TEST(cursor_move_zero);
    Cursor c;
    cursor_init(&c);
    cursor_set(&c, 5, 5);
    cursor_move(&c, 0, 0);
    ASSERT_EQ_INT(c.row, 5);
    ASSERT_EQ_INT(c.col, 5);
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

static void test_history_text_independence(void) {
    TEST(history_text_independence);
    History h;
    history_init(&h);
    char buf[] = "abc";
    history_push_insert(&h, 0, buf, 3, 0, 0);
    buf[0] = 'X';
    HistoryEntry *e = history_peek_undo(&h);
    ASSERT(e->text[0] == 'a');
    history_free(&h);
    PASS();
}

static void test_history_multiple_undo_redo_cycles(void) {
    TEST(history_multiple_undo_redo_cycles);
    History h;
    history_init(&h);
    for (int i = 0; i < 10; i++) {
        char t[2] = {'a' + i, 0};
        history_push_insert(&h, i, t, 1, 0, i);
    }
    for (int i = 0; i < 5; i++) history_regress(&h);
    ASSERT_EQ_INT(h.current, 4);
    for (int i = 0; i < 3; i++) history_advance(&h);
    ASSERT_EQ_INT(h.current, 7);
    for (int i = 0; i < 10; i++) history_regress(&h);
    ASSERT_EQ_INT(h.current, -1);
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

static void test_mode_all_modes(void) {
    TEST(mode_all_modes);
    ModeState ms;
    mode_init(&ms);
    Mode modes[] = { MODE_NORMAL, MODE_INSERT, MODE_SELECT, MODE_VIEW,
                     MODE_COMMAND, MODE_GOTO, MODE_FIND, MODE_SEARCH,
                     MODE_EXTEND, MODE_UNDO, MODE_MACRO };
    for (int i = 0; i < 11; i++) {
        mode_set(&ms, modes[i]);
        ASSERT_TRUE(mode_is(&ms, modes[i]));
    }
    PASS();
}

static void test_mode_previous_tracking(void) {
    TEST(mode_previous_tracking);
    ModeState ms;
    mode_init(&ms);
    mode_set(&ms, MODE_INSERT);
    mode_set(&ms, MODE_SELECT);
    mode_set(&ms, MODE_COMMAND);
    ASSERT_EQ_INT(ms.previous, MODE_SELECT);
    ASSERT_EQ_INT(ms.current, MODE_COMMAND);
    PASS();
}

static void test_mode_transition_clears_pending_state(void) {
    TEST(mode_transition_clears_pending_state);
    ModeState ms;
    mode_init(&ms);
    ms.g_pending = true;
    ms.pending_key = 'i';
    ms.pending_keys[0] = 'x';
    ms.pending_len = 1;
    ms.count = 3;
    ms.pending_operator = 'd';
    ms.pending_text_obj = 'a';
    ms.view_mode_sticky = true;
    mode_set(&ms, MODE_INSERT);
    ASSERT_FALSE(ms.g_pending);
    ASSERT_EQ_INT(ms.pending_key, 0);
    ASSERT_EQ_INT(ms.pending_keys[0], 0);
    ASSERT_EQ_INT(ms.pending_len, 0);
    ASSERT_EQ_INT(ms.count, 0);
    ASSERT_EQ_INT(ms.pending_operator, 0);
    ASSERT_EQ_INT(ms.pending_text_obj, 0);
    ASSERT_FALSE(ms.view_mode_sticky);
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

static void test_syntax_multiline_tokens(void) {
    TEST(syntax_multiline_tokens);
    SyntaxHighlighting sh;
    syntax_init(&sh, NULL);
    syntax_add_token(&sh, 0, 5, 2, 3, SYNTAX_COMMENT);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, 6), SYNTAX_COMMENT);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 1, 0), SYNTAX_COMMENT);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 2, 2), SYNTAX_COMMENT);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 2, 3), SYNTAX_NORMAL);
    syntax_free(&sh);
    PASS();
}

static void test_syntax_all_types(void) {
    TEST(syntax_all_types);
    SyntaxHighlighting sh;
    syntax_init(&sh, NULL);
    SyntaxType types[] = {
        SYNTAX_KEYWORD, SYNTAX_STRING, SYNTAX_NUMBER, SYNTAX_COMMENT,
        SYNTAX_FUNCTION, SYNTAX_TYPE, SYNTAX_ERROR, SYNTAX_WARNING,
        SYNTAX_VARIABLE, SYNTAX_MACRO, SYNTAX_OPERATOR, SYNTAX_NAMESPACE
    };
    for (int i = 0; i < 12; i++) {
        syntax_add_token(&sh, i, 0, i, 5, types[i]);
    }
    for (int i = 0; i < 12; i++) {
        ASSERT_EQ_INT(syntax_get_type_at(&sh, i, 2), types[i]);
    }
    syntax_free(&sh);
    PASS();
}

static void test_syntax_boundary_positions(void) {
    TEST(syntax_boundary_positions);
    SyntaxHighlighting sh;
    syntax_init(&sh, NULL);
    syntax_add_token(&sh, 0, 3, 0, 7, SYNTAX_KEYWORD);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, 2), SYNTAX_NORMAL);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, 3), SYNTAX_KEYWORD);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, 6), SYNTAX_KEYWORD);
    ASSERT_EQ_INT(syntax_get_type_at(&sh, 0, 7), SYNTAX_NORMAL);
    syntax_free(&sh);
    PASS();
}

/* ================================================================
 * LSP TESTS
 * ================================================================ */

static void test_lsp_publish_diagnostics_parse(void) {
    TEST(lsp_publish_diagnostics_parse);
    const char *json =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\","
        "\"params\":{\"uri\":\"file:///tmp/a.c\",\"diagnostics\":["
        "{\"range\":{\"start\":{\"line\":2,\"character\":4},"
        "\"end\":{\"line\":2,\"character\":7}},\"severity\":1,"
        "\"message\":\"expected ] token\",\"code\":\"E1\"}]}}";
    LSPDiagnostics *diag = lsp_parse_publish_diagnostics_notification(json);
    ASSERT(diag != NULL);
    ASSERT_EQ_STR(diag->uri, "file:///tmp/a.c");
    ASSERT_EQ_INT(diag->count, 1);
    ASSERT_EQ_INT(diag->items[0].start_line, 2);
    ASSERT_EQ_INT(diag->items[0].start_col, 4);
    ASSERT_EQ_INT(diag->items[0].end_col, 7);
    ASSERT_EQ_INT(diag->items[0].severity, LSP_DIAG_ERROR);
    ASSERT_EQ_STR(diag->items[0].message, "expected ] token");
    lsp_free_diagnostics(diag);
    PASS();
}

static void test_lsp_publish_diagnostics_empty(void) {
    TEST(lsp_publish_diagnostics_empty);
    const char *json =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\","
        "\"params\":{\"uri\":\"file:///tmp/a.c\",\"diagnostics\":[]}}";
    LSPDiagnostics *diag = lsp_parse_publish_diagnostics_notification(json);
    ASSERT(diag != NULL);
    ASSERT_EQ_STR(diag->uri, "file:///tmp/a.c");
    ASSERT_EQ_INT(diag->count, 0);
    lsp_free_diagnostics(diag);
    PASS();
}

static void test_lsp_publish_diagnostics_clangd_shape(void) {
    TEST(lsp_publish_diagnostics_clangd_shape);
    const char *json =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\","
        "\"params\":{\"uri\":\"file:///tmp/main.c\",\"version\":3,\"diagnostics\":["
        "{\"range\":{\"start\":{\"line\":4,\"character\":9},"
        "\"end\":{\"line\":4,\"character\":17}},"
        "\"message\":\"use of undeclared identifier 'missing'\","
        "\"severity\":1,\"code\":{\"value\":\"undeclared_var\","
        "\"target\":\"https://clang.llvm.org/\"},\"source\":\"clangd\","
        "\"relatedInformation\":[{\"location\":{\"uri\":\"file:///tmp/main.c\","
        "\"range\":{\"start\":{\"line\":1,\"character\":0},"
        "\"end\":{\"line\":1,\"character\":1}}},\"message\":\"from here\"}]},"
        "{\"range\":{\"start\":{\"line\":8,\"character\":2},"
        "\"end\":{\"line\":8,\"character\":3}},\"severity\":2,"
        "\"code\":1234,\"message\":\"unused variable\"}]}}";
    LSPDiagnostics *diag = lsp_parse_publish_diagnostics_notification(json);
    ASSERT(diag != NULL);
    ASSERT_EQ_STR(diag->uri, "file:///tmp/main.c");
    ASSERT_EQ_INT(diag->count, 2);
    ASSERT_EQ_INT(diag->items[0].severity, LSP_DIAG_ERROR);
    ASSERT_EQ_STR(diag->items[0].code, "undeclared_var");
    ASSERT_EQ_STR(diag->items[0].message, "use of undeclared identifier 'missing'");
    ASSERT_EQ_INT(diag->items[1].severity, LSP_DIAG_WARNING);
    ASSERT_EQ_STR(diag->items[1].code, "1234");
    lsp_free_diagnostics(diag);
    PASS();
}

static void test_lsp_publish_diagnostics_quoted_key_text(void) {
    TEST(lsp_publish_diagnostics_quoted_key_text);
    const char *json =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\","
        "\"params\":{\"uri\":\"file:///tmp/a.c\",\"diagnostics\":["
        "{\"range\":{\"start\":{\"line\":0,\"character\":0},"
        "\"end\":{\"line\":0,\"character\":1}},"
        "\"message\":\"literal \\\"severity\\\" text should not be parsed as a key\","
        "\"severity\":2}]}}";
    LSPDiagnostics *diag = lsp_parse_publish_diagnostics_notification(json);
    ASSERT(diag != NULL);
    ASSERT_EQ_INT(diag->count, 1);
    ASSERT_EQ_INT(diag->items[0].severity, LSP_DIAG_WARNING);
    ASSERT_EQ_STR(diag->items[0].message, "literal \"severity\" text should not be parsed as a key");
    lsp_free_diagnostics(diag);
    PASS();
}

static void test_lsp_completion_insert_text(void) {
    TEST(lsp_completion_insert_text);
    const char *json =
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"isIncomplete\":false,\"items\":["
        "{\"label\":\"printf\",\"insertText\":\"printf($1);\",\"detail\":\"function\"}]}}";
    LSPCompletionItems *items = lsp_parse_completion_response(json);
    ASSERT(items != NULL);
    ASSERT_EQ_INT(items->count, 1);
    ASSERT_EQ_STR(items->items[0].label, "printf");
    ASSERT_EQ_STR(items->items[0].insert_text, "printf($1);");
    ASSERT_EQ_STR(items->items[0].detail, "function");
    lsp_free_completion_items(items);
    PASS();
}

static void test_lsp_formatting_response_parse(void) {
    TEST(lsp_formatting_response_parse);
    const char *json =
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"result\":["
        "{\"range\":{\"start\":{\"line\":0,\"character\":0},"
        "\"end\":{\"line\":0,\"character\":3}},\"newText\":\"int\"}]}";
    LSPWorkspaceEdit *edit = lsp_parse_formatting_response(json);
    ASSERT(edit != NULL);
    ASSERT_EQ_INT(edit->count, 1);
    ASSERT_EQ_INT(edit->changes[0].range.start.line, 0);
    ASSERT_EQ_INT(edit->changes[0].range.end.character, 3);
    ASSERT_EQ_STR(edit->changes[0].new_text, "int");
    lsp_free_workspace_edit(edit);
    PASS();
}

static void test_lsp_code_action_edit_parse(void) {
    TEST(lsp_code_action_edit_parse);
    const char *json =
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"result\":[{\"title\":\"Fix include\","
        "\"kind\":\"quickfix\",\"edit\":{\"changes\":{\"file:///tmp/a.c\":["
        "{\"range\":{\"start\":{\"line\":1,\"character\":0},"
        "\"end\":{\"line\":1,\"character\":0}},\"newText\":\"#include <stdio.h>\\n\"}]}}}]}";
    LSPCodeActions *actions = lsp_parse_code_actions_response(json);
    ASSERT(actions != NULL);
    ASSERT_EQ_INT(actions->count, 1);
    ASSERT_EQ_STR(actions->actions[0].title, "Fix include");
    ASSERT(actions->actions[0].edit != NULL);
    ASSERT_EQ_INT(actions->actions[0].edit->count, 1);
    ASSERT_EQ_STR(actions->actions[0].edit->changes[0].new_text, "#include <stdio.h>\n");
    lsp_free_code_actions(actions);
    PASS();
}

/* ================================================================
 * DOCUMENT TESTS
 * ================================================================ */

static void test_document_init(void) {
    TEST(document_init);
    Document *doc = make_doc(NULL);
    ASSERT_EQ_INT(doc->cursor_count, 1);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    ASSERT_FALSE(doc->dirty);
    ASSERT_EQ_INT(doc->scroll_y, 0);
    ASSERT(doc->buffer.text != NULL);
    ASSERT_EQ_SIZE(doc->buffer.len, 0);
    free_doc(doc);
    PASS();
}

static void test_document_mark_dirty(void) {
    TEST(document_mark_dirty);
    Document *doc = make_doc(NULL);
    ASSERT_FALSE(doc->dirty);
    document_mark_dirty(doc);
    ASSERT_TRUE(doc->dirty);
    ASSERT_TRUE(doc->syntax_dirty);
    ASSERT_TRUE(doc->lsp_dirty);
    free_doc(doc);
    PASS();
}

static void test_document_insert_char(void) {
    TEST(document_insert_char);
    Document *doc = make_doc(NULL);
    document_insert_char(doc, 'h');
    document_insert_char(doc, 'i');
    ASSERT_EQ_SIZE(doc->buffer.len, 2);
    ASSERT_EQ_STR(doc->buffer.text, "hi");
    ASSERT_EQ_INT(doc->cursors[0].col, 2);
    ASSERT_TRUE(doc->dirty);
    free_doc(doc);
    PASS();
}

static void test_document_insert_char_multiline(void) {
    TEST(document_insert_char_multiline);
    Document *doc = make_doc("line1\nline2");
    document_cursor_to(doc, 1, 0);
    document_insert_char(doc, 'X');
    ASSERT_EQ_STR(doc->buffer.text, "line1\nXline2");
    ASSERT_EQ_INT(doc->cursors[0].row, 1);
    ASSERT_EQ_INT(doc->cursors[0].col, 1);
    free_doc(doc);
    PASS();
}

static void test_document_delete_char(void) {
    TEST(document_delete_char);
    Document *doc = make_doc("abc");
    document_cursor_to(doc, 0, 3);
    document_delete_char(doc);
    ASSERT_EQ_SIZE(doc->buffer.len, 2);
    ASSERT_EQ_STR(doc->buffer.text, "ab");
    ASSERT_EQ_INT(doc->cursors[0].col, 2);
    free_doc(doc);
    PASS();
}

static void test_document_delete_char_at_start(void) {
    TEST(document_delete_char_at_start);
    Document *doc = make_doc("abc");
    document_delete_char(doc);
    ASSERT_EQ_SIZE(doc->buffer.len, 3);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_delete_char_across_lines(void) {
    TEST(document_delete_char_across_lines);
    Document *doc = make_doc("ab\ncd");
    document_cursor_to(doc, 1, 0);
    document_delete_char(doc);
    ASSERT_EQ_SIZE(doc->buffer.len, 4);
    ASSERT_EQ_STR(doc->buffer.text, "abcd");
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    free_doc(doc);
    PASS();
}

static void test_document_newline(void) {
    TEST(document_newline);
    Document *doc = make_doc("hello");
    document_cursor_to(doc, 0, 3);
    document_newline(doc);
    ASSERT_EQ_STR(doc->buffer.text, "hel\nlo");
    ASSERT_EQ_INT(doc->cursors[0].row, 1);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_newline_auto_indent(void) {
    TEST(document_newline_auto_indent);
    Document *doc = make_doc("    hello");
    document_cursor_to(doc, 0, 8);
    document_newline(doc);
    ASSERT_EQ_INT(doc->cursors[0].row, 1);
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    const char *line1 = buffer_line_ptr(&doc->buffer, 1);
    ASSERT(strncmp(line1, "    ", 4) == 0);
    free_doc(doc);
    PASS();
}

static void test_document_newline_no_indent(void) {
    TEST(document_newline_no_indent);
    Document *doc = make_doc("hello");
    document_cursor_to(doc, 0, 2);
    document_newline(doc);
    ASSERT_EQ_INT(doc->cursors[0].row, 1);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_insert_text(void) {
    TEST(document_insert_text);
    Document *doc = make_doc(NULL);
    document_insert_text(doc, "hello");
    ASSERT_EQ_STR(doc->buffer.text, "hello");
    ASSERT_EQ_INT(doc->cursors[0].col, 5);
    free_doc(doc);
    PASS();
}

static void test_document_insert_text_multiline(void) {
    TEST(document_insert_text_multiline);
    Document *doc = make_doc(NULL);
    document_insert_text(doc, "ab\ncd");
    ASSERT_EQ_STR(doc->buffer.text, "ab\ncd");
    ASSERT_EQ_INT(doc->cursors[0].row, 1);
    ASSERT_EQ_INT(doc->cursors[0].col, 2);
    free_doc(doc);
    PASS();
}

static void test_document_move_cursor(void) {
    TEST(document_move_cursor);
    Document *doc = make_doc("line1\nline2\nline3");
    document_move_cursor(doc, 1, 2);
    ASSERT_EQ_INT(doc->cursors[0].row, 1);
    ASSERT_EQ_INT(doc->cursors[0].col, 2);
    free_doc(doc);
    PASS();
}

static void test_document_move_cursor_clamp(void) {
    TEST(document_move_cursor_clamp);
    Document *doc = make_doc("ab");
    document_move_cursor(doc, 100, 100);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 2);
    free_doc(doc);
    PASS();
}

static void test_document_move_cursor_negative(void) {
    TEST(document_move_cursor_negative);
    Document *doc = make_doc("abc");
    document_move_cursor(doc, -5, -5);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_move_cursor_line_wrap(void) {
    TEST(document_move_cursor_line_wrap);
    Document *doc = make_doc("ab\ncd");
    document_cursor_to(doc, 1, 0);
    document_move_cursor(doc, 0, -1);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 1);
    free_doc(doc);
    PASS();
}

static void test_document_cursor_to(void) {
    TEST(document_cursor_to);
    Document *doc = make_doc("abc\ndef\nghi");
    document_cursor_to(doc, 2, 1);
    ASSERT_EQ_INT(doc->cursors[0].row, 2);
    ASSERT_EQ_INT(doc->cursors[0].col, 1);
    free_doc(doc);
    PASS();
}

static void test_document_cursor_to_clamp(void) {
    TEST(document_cursor_to_clamp);
    Document *doc = make_doc("abc");
    document_cursor_to(doc, 100, 100);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 3);
    free_doc(doc);
    PASS();
}

static void test_document_cursor_to_negative(void) {
    TEST(document_cursor_to_negative);
    Document *doc = make_doc("abc");
    document_cursor_to(doc, -5, -5);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_cursor_home_end(void) {
    TEST(document_cursor_home_end);
    Document *doc = make_doc("hello world");
    document_cursor_to(doc, 0, 5);
    document_cursor_home(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    document_cursor_end(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 11);
    free_doc(doc);
    PASS();
}

static void test_document_cursor_doc_start_end(void) {
    TEST(document_cursor_doc_start_end);
    Document *doc = make_doc("line1\nline2\nline3");
    document_cursor_doc_end(doc);
    ASSERT_EQ_INT(doc->cursors[0].row, 2);
    document_cursor_doc_start(doc);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_undo_redo_insert(void) {
    TEST(document_undo_redo_insert);
    Document *doc = make_doc(NULL);
    document_insert_char(doc, 'A');
    document_insert_char(doc, 'B');
    ASSERT_EQ_STR(doc->buffer.text, "AB");
    document_undo(doc);
    ASSERT_EQ_STR(doc->buffer.text, "A");
    document_undo(doc);
    ASSERT_EQ_STR(doc->buffer.text, "");
    document_redo(doc);
    ASSERT_EQ_STR(doc->buffer.text, "A");
    document_redo(doc);
    ASSERT_EQ_STR(doc->buffer.text, "AB");
    free_doc(doc);
    PASS();
}

static void test_document_undo_redo_delete(void) {
    TEST(document_undo_redo_delete);
    Document *doc = make_doc("abc");
    document_cursor_to(doc, 0, 3);
    document_delete_char(doc);
    ASSERT_EQ_STR(doc->buffer.text, "ab");
    document_undo(doc);
    ASSERT_EQ_STR(doc->buffer.text, "abc");
    free_doc(doc);
    PASS();
}

static void test_document_undo_redo_newline(void) {
    TEST(document_undo_redo_newline);
    Document *doc = make_doc("ab");
    document_cursor_to(doc, 0, 1);
    document_newline(doc);
    ASSERT_EQ_STR(doc->buffer.text, "a\nb");
    document_undo(doc);
    ASSERT_EQ_STR(doc->buffer.text, "ab");
    free_doc(doc);
    PASS();
}

static void test_document_yank_paste(void) {
    TEST(document_yank_paste);
    Document *doc = make_doc("hello");
    document_yank(doc);
    ASSERT(doc->clipboard != NULL);
    document_cursor_to(doc, 0, 5);
    document_paste(doc);
    ASSERT_EQ_STR(doc->buffer.text, "hellohello\n");
    free_doc(doc);
    PASS();
}

static void test_document_yank_line(void) {
    TEST(document_yank_line);
    Document *doc = make_doc("hello\nworld");
    document_yank_line(doc);
    ASSERT(doc->clipboard != NULL);
    ASSERT(doc->clipboard_len > 0);
    ASSERT(doc->clipboard[doc->clipboard_len - 1] == '\n');
    free_doc(doc);
    PASS();
}

static void test_document_delete_line_at(void) {
    TEST(document_delete_line_at);
    Document *doc = make_doc("line1\nline2\nline3");
    document_cursor_to(doc, 1, 0);
    document_delete_line_at(doc);
    ASSERT_EQ_STR(doc->buffer.text, "line1\nline3");
    ASSERT_EQ_INT(doc->cursors[0].row, 1);
    free_doc(doc);
    PASS();
}

static void test_document_delete_line_at_first(void) {
    TEST(document_delete_line_at_first);
    Document *doc = make_doc("line1\nline2");
    document_delete_line_at(doc);
    ASSERT_EQ_STR(doc->buffer.text, "line2");
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    free_doc(doc);
    PASS();
}

static void test_document_select_word(void) {
    TEST(document_select_word);
    Document *doc = make_doc("hello world");
    document_select_word(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 5);
    free_doc(doc);
    PASS();
}

static void test_document_select_line(void) {
    TEST(document_select_line);
    Document *doc = make_doc("hello");
    document_select_line(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    free_doc(doc);
    PASS();
}

static void test_document_select_all(void) {
    TEST(document_select_all);
    Document *doc = make_doc("abc\ndef");
    document_select_all(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].anchor_row, 0);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 0);
    ASSERT_EQ_INT(doc->cursors[0].row, 1);
    ASSERT_EQ_INT(doc->cursors[0].col, 3);
    free_doc(doc);
    PASS();
}

static void test_document_delete_selection(void) {
    TEST(document_delete_selection);
    Document *doc = make_doc("hello world");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_delete_selection(doc);
    ASSERT_EQ_STR(doc->buffer.text, " world");
    ASSERT_FALSE(doc->cursors[0].has_selection);
    free_doc(doc);
    PASS();
}

static void test_document_replace_char(void) {
    TEST(document_replace_char);
    Document *doc = make_doc("abc");
    document_replace_char(doc, 'X');
    ASSERT_EQ_STR(doc->buffer.text, "Xbc");
    free_doc(doc);
    PASS();
}

static void test_document_delete_char_at_cursor(void) {
    TEST(document_delete_char_at_cursor);
    Document *doc = make_doc("abc");
    document_delete_char_at_cursor(doc);
    ASSERT_EQ_STR(doc->buffer.text, "bc");
    free_doc(doc);
    PASS();
}

static void test_document_indent_dedent_line(void) {
    TEST(document_indent_dedent_line);
    Document *doc = make_doc("hello");
    document_indent_line(doc);
    ASSERT_EQ_STR(doc->buffer.text, "\thello");
    ASSERT_EQ_INT(doc->cursors[0].col, 1);
    document_dedent_line(doc);
    ASSERT_EQ_STR(doc->buffer.text, "hello");
    free_doc(doc);
    PASS();
}

static void test_document_join_lines(void) {
    TEST(document_join_lines);
    Document *doc = make_doc("hello\nworld");
    document_join_lines(doc);
    ASSERT_EQ_STR(doc->buffer.text, "helloworld");
    free_doc(doc);
    PASS();
}

static void test_document_join_lines_strips_indent(void) {
    TEST(document_join_lines_strips_indent);
    Document *doc = make_doc("hello\n    world");
    document_join_lines(doc);
    ASSERT_EQ_STR(doc->buffer.text, "helloworld");
    free_doc(doc);
    PASS();
}

static void test_document_open_line_below(void) {
    TEST(document_open_line_below);
    Document *doc = make_doc("hello");
    document_open_line_below(doc);
    ASSERT_EQ_STR(doc->buffer.text, "hello\n");
    ASSERT_EQ_INT(doc->cursors[0].row, 1);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_open_line_above(void) {
    TEST(document_open_line_above);
    Document *doc = make_doc("hello");
    document_open_line_above(doc);
    ASSERT_EQ_SIZE(buffer_line_count(&doc->buffer), 2);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    free_doc(doc);
    PASS();
}

static void test_document_cursor_first_non_blank(void) {
    TEST(document_cursor_first_non_blank);
    Document *doc = make_doc("    hello");
    document_cursor_first_non_blank(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    free_doc(doc);
    PASS();
}

static void test_document_cursor_word_forward(void) {
    TEST(document_cursor_word_forward);
    Document *doc = make_doc("hello world");
    document_cursor_to(doc, 0, 0);
    document_cursor_word_forward(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 5);
    document_cursor_word_forward(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 11);
    free_doc(doc);
    PASS();
}

static void test_document_cursor_word_backward(void) {
    TEST(document_cursor_word_backward);
    Document *doc = make_doc("hello world");
    document_cursor_to(doc, 0, 11);
    document_cursor_word_backward(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 6);
    document_cursor_word_backward(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_toggle_case(void) {
    TEST(document_toggle_case);
    Document *doc = make_doc("hello");
    document_toggle_case(doc);
    ASSERT_EQ_STR(doc->buffer.text, "Hello");
    document_cursor_to(doc, 0, 1);
    document_toggle_case(doc);
    ASSERT_EQ_STR(doc->buffer.text, "HEllo");
    free_doc(doc);
    PASS();
}

static void test_document_toggle_case_selection(void) {
    TEST(document_toggle_case_selection);
    Document *doc = make_doc("hello world");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_toggle_case(doc);
    ASSERT_EQ_STR(doc->buffer.text, "HELLO world");
    free_doc(doc);
    PASS();
}

static void test_document_lowercase(void) {
    TEST(document_lowercase);
    Document *doc = make_doc("HELLO");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_lowercase(doc);
    ASSERT_EQ_STR(doc->buffer.text, "hello");
    free_doc(doc);
    PASS();
}

static void test_document_uppercase(void) {
    TEST(document_uppercase);
    Document *doc = make_doc("hello");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_uppercase(doc);
    ASSERT_EQ_STR(doc->buffer.text, "HELLO");
    free_doc(doc);
    PASS();
}

static void test_document_find_char_forward(void) {
    TEST(document_find_char_forward);
    Document *doc = make_doc("abcabc");
    document_find_char_forward(doc, 'c');
    ASSERT_EQ_INT(doc->cursors[0].col, 2);
    document_find_char_forward(doc, 'c');
    ASSERT_EQ_INT(doc->cursors[0].col, 5);
    free_doc(doc);
    PASS();
}

static void test_document_find_char_backward(void) {
    TEST(document_find_char_backward);
    Document *doc = make_doc("abcabc");
    document_cursor_to(doc, 0, 5);
    document_find_char_backward(doc, 'c');
    ASSERT_EQ_INT(doc->cursors[0].col, 2);
    document_find_char_backward(doc, 'a');
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_till_char_forward(void) {
    TEST(document_till_char_forward);
    Document *doc = make_doc("abcabc");
    document_till_char_forward(doc, 'c');
    ASSERT_EQ_INT(doc->cursors[0].col, 1);
    free_doc(doc);
    PASS();
}

static void test_document_till_char_backward(void) {
    TEST(document_till_char_backward);
    Document *doc = make_doc("abcabc");
    document_cursor_to(doc, 0, 5);
    document_till_char_backward(doc, 'a');
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    free_doc(doc);
    PASS();
}

static void test_document_goto_line_start_end(void) {
    TEST(document_goto_line_start_end);
    Document *doc = make_doc("hello world");
    document_cursor_to(doc, 0, 5);
    document_goto_line_start(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    document_goto_line_end(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 11);
    free_doc(doc);
    PASS();
}

static void test_document_extend_to_line_bounds(void) {
    TEST(document_extend_to_line_bounds);
    Document *doc = make_doc("hello");
    document_extend_to_line_bounds(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 5);
    free_doc(doc);
    PASS();
}

static void test_document_scroll_up_down(void) {
    TEST(document_scroll_up_down);
    Document *doc = make_doc("line1\nline2\nline3");
    doc->scroll_y = 1;
    document_scroll_up(doc);
    ASSERT_EQ_INT(doc->scroll_y, 0);
    document_scroll_down(doc);
    ASSERT_EQ_INT(doc->scroll_y, 1);
    free_doc(doc);
    PASS();
}

static void test_document_scroll_up_clamp(void) {
    TEST(document_scroll_up_clamp);
    Document *doc = make_doc("line1");
    doc->scroll_y = 0;
    document_scroll_up(doc);
    ASSERT_EQ_INT(doc->scroll_y, 0);
    free_doc(doc);
    PASS();
}

static void test_document_sync_viewport(void) {
    TEST(document_sync_viewport);
    Document *doc = make_doc("line1\nline2\nline3");
    doc->viewport_lines = 2;
    document_cursor_to(doc, 2, 0);
    document_sync_viewport_to_cursor(doc);
    ASSERT(doc->scroll_y >= 1);
    free_doc(doc);
    PASS();
}

static void test_document_sync_viewport_horizontal_right(void) {
    TEST(document_sync_viewport_horizontal_right);
    Document *doc = make_doc("0123456789abcdef");
    doc->viewport_cols = 5;
    document_cursor_to(doc, 0, 12);
    ASSERT_EQ_INT(doc->scroll_x, 8);
    free_doc(doc);
    PASS();
}

static void test_document_sync_viewport_horizontal_left(void) {
    TEST(document_sync_viewport_horizontal_left);
    Document *doc = make_doc("0123456789abcdef");
    doc->viewport_cols = 5;
    doc->scroll_x = 8;
    document_cursor_to(doc, 0, 2);
    ASSERT_EQ_INT(doc->scroll_x, 2);
    free_doc(doc);
    PASS();
}

static void test_document_move_cursor_updates_scroll_x(void) {
    TEST(document_move_cursor_updates_scroll_x);
    Document *doc = make_doc("0123456789abcdef");
    doc->viewport_cols = 4;
    document_move_cursor(doc, 0, 10);
    ASSERT_EQ_INT(doc->cursors[0].col, 10);
    ASSERT_EQ_INT(doc->scroll_x, 7);
    document_move_cursor(doc, 0, -10);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    ASSERT_EQ_INT(doc->scroll_x, 0);
    free_doc(doc);
    PASS();
}

static void test_document_set_search(void) {
    TEST(document_set_search);
    Document *doc = make_doc("hello world hello");
    document_set_search(doc, "hello", 5);
    ASSERT_EQ_STR(doc->search_query, "hello");
    ASSERT_EQ_SIZE(doc->search_len, 5);
    free_doc(doc);
    PASS();
}

static void test_document_search_next(void) {
    TEST(document_search_next);
    Document *doc = make_doc("hello world hello");
    document_set_search(doc, "hello", 5);
    document_search_next(doc);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 12);
    document_search_next(doc);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_search_prev(void) {
    TEST(document_search_prev);
    Document *doc = make_doc("hello world hello");
    document_set_search(doc, "hello", 5);
    document_cursor_to(doc, 1, 12);
    document_search_prev(doc);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_search_wraps(void) {
    TEST(document_search_wraps);
    Document *doc = make_doc("hello world hello");
    document_set_search(doc, "hello", 5);
    document_cursor_to(doc, 1, 14);
    document_search_next(doc);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_add_cursor(void) {
    TEST(document_add_cursor);
    Document *doc = make_doc("foo bar foo");
    document_add_cursor(doc);
    ASSERT_EQ_INT(doc->cursor_count, 2);
    ASSERT_EQ_INT(doc->cursors[1].row, 0);
    ASSERT_EQ_INT(doc->cursors[1].col, 8);
    free_doc(doc);
    PASS();
}

static void test_document_add_cursor_repeated(void) {
    TEST(document_add_cursor_repeated);
    Document *doc = make_doc("foo foo foo");
    document_add_cursor(doc);
    document_add_cursor(doc);
    ASSERT_EQ_INT(doc->cursor_count, 3);
    ASSERT_EQ_INT(doc->cursors[1].col, 4);
    ASSERT_EQ_INT(doc->cursors[2].col, 8);
    free_doc(doc);
    PASS();
}

static void test_document_remove_last_cursor(void) {
    TEST(document_remove_last_cursor);
    Document *doc = make_doc("foo bar foo");
    document_add_cursor(doc);
    ASSERT_EQ_INT(doc->cursor_count, 2);
    document_remove_last_cursor(doc);
    ASSERT_EQ_INT(doc->cursor_count, 1);
    free_doc(doc);
    PASS();
}

static void test_document_clear_cursors(void) {
    TEST(document_clear_cursors);
    Document *doc = make_doc("foo bar foo");
    document_add_cursor(doc);
    ASSERT_EQ_INT(doc->cursor_count, 2);
    document_clear_cursors(doc);
    ASSERT_EQ_INT(doc->cursor_count, 1);
    free_doc(doc);
    PASS();
}

static void test_document_collapse_selection(void) {
    TEST(document_collapse_selection);
    Document *doc = make_doc("hello");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_collapse_selection(doc);
    ASSERT_FALSE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 5);
    free_doc(doc);
    PASS();
}

static void test_document_flip_cursor_anchor(void) {
    TEST(document_flip_cursor_anchor);
    Document *doc = make_doc("hello");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_flip_cursor_anchor(doc);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    free_doc(doc);
    PASS();
}

static void test_document_surround(void) {
    TEST(document_surround);
    Document *doc = make_doc("hello");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_surround(doc, '"');
    ASSERT_EQ_STR(doc->buffer.text, "\"hello\"");
    free_doc(doc);
    PASS();
}

static void test_document_surround_parens(void) {
    TEST(document_surround_parens);
    Document *doc = make_doc("hello");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_surround(doc, '(');
    ASSERT_EQ_STR(doc->buffer.text, "(hello)");
    free_doc(doc);
    PASS();
}

static void test_document_delete_surround(void) {
    TEST(document_delete_surround);
    Document *doc = make_doc("\"hello\"");
    document_cursor_to(doc, 0, 1);
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 6;
    document_delete_surround(doc, '"');
    ASSERT_EQ_STR(doc->buffer.text, "hello");
    free_doc(doc);
    PASS();
}

static void test_document_move_line_up_down(void) {
    TEST(document_move_line_up_down);
    Document *doc = make_doc("line1\nline2\nline3");
    document_cursor_to(doc, 1, 0);
    document_move_line_up(doc);
    ASSERT_EQ_STR(doc->buffer.text, "line2\nline1\nline3");
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    document_move_line_down(doc);
    ASSERT_EQ_STR(doc->buffer.text, "line1\nline2\nline3");
    ASSERT_EQ_INT(doc->cursors[0].row, 1);
    free_doc(doc);
    PASS();
}

static void test_document_move_line_up_first_line(void) {
    TEST(document_move_line_up_first_line);
    Document *doc = make_doc("line1\nline2");
    document_move_line_up(doc);
    ASSERT_EQ_STR(doc->buffer.text, "line1\nline2");
    free_doc(doc);
    PASS();
}

static void test_document_move_line_down_last_line(void) {
    TEST(document_move_line_down_last_line);
    Document *doc = make_doc("line1\nline2");
    document_cursor_to(doc, 1, 0);
    document_move_line_down(doc);
    ASSERT_EQ_STR(doc->buffer.text, "line1\nline2");
    free_doc(doc);
    PASS();
}

static void test_document_change_selection(void) {
    TEST(document_change_selection);
    Document *doc = make_doc("hello");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 3;
    document_change_selection(doc);
    ASSERT_EQ_STR(doc->buffer.text, "lo");
    ASSERT_FALSE(doc->cursors[0].has_selection);
    free_doc(doc);
    PASS();
}

static void test_document_substitute_char(void) {
    TEST(document_substitute_char);
    Document *doc = make_doc("abc");
    document_substitute_char(doc);
    ASSERT_EQ_STR(doc->buffer.text, "bc");
    free_doc(doc);
    PASS();
}

static void test_document_indent_selection(void) {
    TEST(document_indent_selection);
    Document *doc = make_doc("line1\nline2\nline3");
    cursor_select_start(&doc->cursors[0]);
    document_cursor_to(doc, 2, 0);
    document_indent_selection(doc);
    const char *l0 = buffer_line_ptr(&doc->buffer, 0);
    const char *l1 = buffer_line_ptr(&doc->buffer, 1);
    const char *l2 = buffer_line_ptr(&doc->buffer, 2);
    ASSERT(l0[0] == '\t');
    ASSERT(l1[0] == '\t');
    ASSERT(l2[0] == '\t');
    free_doc(doc);
    PASS();
}

static void test_document_dedent_selection(void) {
    TEST(document_dedent_selection);
    Document *doc = make_doc("\tline1\n\tline2\n\tline3");
    cursor_select_start(&doc->cursors[0]);
    document_cursor_to(doc, 2, 0);
    document_dedent_selection(doc);
    ASSERT(doc->buffer.text[0] != '\t');
    free_doc(doc);
    PASS();
}

static void test_document_paste_before(void) {
    TEST(document_paste_before);
    Document *doc = make_doc("ac");
    doc->clipboard = strdup("b");
    doc->clipboard_len = 1;
    document_cursor_to(doc, 0, 1);
    document_paste_before(doc);
    ASSERT_EQ_STR(doc->buffer.text, "bac");
    free_doc(doc);
    PASS();
}

static void test_document_cursor_page_up_down(void) {
    TEST(document_cursor_page_up_down);
    Document *doc = make_doc("line1\nline2\nline3\nline4\nline5");
    doc->viewport_lines = 3;
    document_cursor_doc_end(doc);
    int start_row = doc->cursors[0].row;
    document_cursor_page_up(doc);
    ASSERT(doc->cursors[0].row < start_row);
    free_doc(doc);
    PASS();
}

static void test_document_half_page(void) {
    TEST(document_half_page);
    Document *doc = make_doc("line1\nline2\nline3\nline4\nline5\nline6\nline7\nline8");
    doc->viewport_lines = 8;
    document_cursor_to(doc, 4, 0);
    document_half_page_down(doc, 8);
    ASSERT(doc->cursors[0].row >= 4);
    document_half_page_up(doc, 8);
    ASSERT(doc->cursors[0].row >= 0);
    free_doc(doc);
    PASS();
}

static void test_document_jumplist(void) {
    TEST(document_jumplist);
    Document *doc = make_doc("hello");
    document_jumplist_push(doc, 0, 5);
    document_jumplist_push(doc, 1, 3);
    ASSERT_EQ_INT(doc->jumplist_len, 2);
    document_jumplist_backward(doc);
    ASSERT_EQ_INT(doc->cursors[0].row, 1);
    ASSERT_EQ_INT(doc->cursors[0].col, 3);
    document_jumplist_backward(doc);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 5);
    free_doc(doc);
    PASS();
}

static void test_document_scroll_center(void) {
    TEST(document_scroll_center);
    Document *doc = make_doc("line1\nline2\nline3\nline4\nline5");
    doc->viewport_lines = 3;
    document_cursor_to(doc, 4, 0);
    document_scroll_center(doc);
    ASSERT(doc->scroll_y >= 0);
    free_doc(doc);
    PASS();
}

static void test_document_empty_buffer_ops(void) {
    TEST(document_empty_buffer_ops);
    Document *doc = make_doc(NULL);
    document_delete_char(doc);
    ASSERT_EQ_SIZE(doc->buffer.len, 0);
    document_cursor_home(doc);
    document_cursor_end(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_new_empty(void) {
    TEST(document_new_empty);
    Document *doc = make_doc("hello");
    document_new(doc);
    ASSERT_EQ_SIZE(doc->buffer.len, 0);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_trim_whitespace(void) {
    TEST(document_trim_whitespace);
    Document *doc = make_doc("hello  \nworld  ");
    document_trim_whitespace(doc);
    ASSERT_EQ_STR(doc->buffer.text, "hello\nworld  ");
    free_doc(doc);
    PASS();
}

static void test_document_select_regex(void) {
    TEST(document_select_regex);
    Document *doc = make_doc("abc 123 def 456");
    document_select_regex(doc, "[0-9]+", 3);
    ASSERT_FALSE(doc->cursors[0].has_selection);
    free_doc(doc);
    PASS();
}

static void test_document_sort_selection(void) {
    TEST(document_sort_selection);
    Document *doc = make_doc("c\na\nb");
    cursor_select_start(&doc->cursors[0]);
    document_cursor_to(doc, 2, 1);
    document_sort_selection(doc);
    const char *l0 = buffer_line_ptr(&doc->buffer, 0);
    const char *l1 = buffer_line_ptr(&doc->buffer, 1);
    const char *l2 = buffer_line_ptr(&doc->buffer, 2);
    ASSERT(l0[0] == 'a');
    ASSERT(l1[0] == 'b');
    ASSERT(l2[0] == 'c');
    free_doc(doc);
    PASS();
}

/* ================================================================
 * ADDITIONAL DOCUMENT TESTS - Word/WORD Motion
 * ================================================================ */

static void test_document_cursor_word_end(void) {
    TEST(document_cursor_word_end);
    Document *doc = make_doc("hello world");
    document_cursor_to(doc, 0, 0);
    document_cursor_word_end(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    document_cursor_word_end(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 10);
    free_doc(doc);
    PASS();
}

static void test_document_cursor_WORD_forward(void) {
    TEST(document_cursor_WORD_forward);
    Document *doc = make_doc("hello world foo");
    document_cursor_to(doc, 0, 0);
    document_cursor_WORD_forward(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 5);
    document_cursor_WORD_forward(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 11);
    document_cursor_WORD_forward(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 15);
    free_doc(doc);
    PASS();
}

static void test_document_cursor_WORD_backward(void) {
    TEST(document_cursor_WORD_backward);
    Document *doc = make_doc("hello world foo");
    document_cursor_to(doc, 0, 15);
    document_cursor_WORD_backward(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 12);
    document_cursor_WORD_backward(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 6);
    document_cursor_WORD_backward(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_cursor_WORD_end(void) {
    TEST(document_cursor_WORD_end);
    Document *doc = make_doc("hello  world  foo");
    document_cursor_to(doc, 0, 0);
    document_cursor_WORD_end(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    document_cursor_WORD_end(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 11);
    free_doc(doc);
    PASS();
}

static void test_document_increment_number(void) {
    TEST(document_increment_number);
    Document *doc = make_doc("count: 42 items");
    document_cursor_to(doc, 0, 8);
    document_increment_number(doc);
    ASSERT_EQ_STR(doc->buffer.text, "count: 43 items");
    document_increment_number(doc);
    ASSERT_EQ_STR(doc->buffer.text, "count: 44 items");
    free_doc(doc);
    PASS();
}

static void test_document_decrement_number(void) {
    TEST(document_decrement_number);
    Document *doc = make_doc("count: 42 items");
    document_cursor_to(doc, 0, 8);
    document_decrement_number(doc);
    ASSERT_EQ_STR(doc->buffer.text, "count: 41 items");
    free_doc(doc);
    PASS();
}

static void test_document_increment_negative_number(void) {
    TEST(document_increment_negative_number);
    Document *doc = make_doc("val = -5");
    document_cursor_to(doc, 0, 6);
    document_increment_number(doc);
    ASSERT_EQ_STR(doc->buffer.text, "val = -4");
    free_doc(doc);
    PASS();
}

static void test_document_decrement_negative_number(void) {
    TEST(document_decrement_negative_number);
    Document *doc = make_doc("val = -5");
    document_cursor_to(doc, 0, 6);
    document_decrement_number(doc);
    ASSERT_EQ_STR(doc->buffer.text, "val = -6");
    free_doc(doc);
    PASS();
}

/* ================================================================
 * ADDITIONAL DOCUMENT TESTS - Multi-cursor
 * ================================================================ */

static void test_document_insert_char_multi(void) {
    TEST(document_insert_char_multi);
    Document *doc = make_doc("aaa bbb ccc");
    doc->cursor_count = 3;
    cursor_init(&doc->cursors[0]);
    cursor_move_to(&doc->cursors[0], 0, 0);
    cursor_init(&doc->cursors[1]);
    cursor_move_to(&doc->cursors[1], 0, 4);
    cursor_init(&doc->cursors[2]);
    cursor_move_to(&doc->cursors[2], 0, 8);
    document_insert_char_multi(doc, 'X');
    ASSERT_EQ_STR(doc->buffer.text, "Xaaa Xbbb Xccc");
    free_doc(doc);
    PASS();
}

static void test_document_insert_char_multi_undo_redo(void) {
    TEST(document_insert_char_multi_undo_redo);
    Document *doc = make_doc("aaa bbb ccc");
    doc->cursor_count = 3;
    cursor_init(&doc->cursors[0]);
    cursor_move_to(&doc->cursors[0], 0, 0);
    cursor_init(&doc->cursors[1]);
    cursor_move_to(&doc->cursors[1], 0, 4);
    cursor_init(&doc->cursors[2]);
    cursor_move_to(&doc->cursors[2], 0, 8);
    document_insert_char_multi(doc, 'X');
    ASSERT_EQ_STR(doc->buffer.text, "Xaaa Xbbb Xccc");
    document_undo(doc);
    ASSERT_EQ_STR(doc->buffer.text, "aaa bbb ccc");
    document_redo(doc);
    ASSERT_EQ_STR(doc->buffer.text, "Xaaa Xbbb Xccc");
    free_doc(doc);
    PASS();
}

static void test_document_move_cursor_multi(void) {
    TEST(document_move_cursor_multi);
    Document *doc = make_doc("aaa\nbbb\nccc");
    doc->cursor_count = 2;
    cursor_init(&doc->cursors[0]);
    cursor_move_to(&doc->cursors[0], 0, 1);
    cursor_init(&doc->cursors[1]);
    cursor_move_to(&doc->cursors[1], 1, 2);
    document_move_cursor(doc, 1, 0);
    ASSERT_EQ_INT(doc->cursors[0].row, 1);
    ASSERT_EQ_INT(doc->cursors[0].col, 1);
    ASSERT_EQ_INT(doc->cursors[1].row, 2);
    ASSERT_EQ_INT(doc->cursors[1].col, 2);
    document_cursor_end(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 3);
    ASSERT_EQ_INT(doc->cursors[1].col, 3);
    free_doc(doc);
    PASS();
}

static void test_document_delete_selection_multi(void) {
    TEST(document_delete_selection_multi);
    Document *doc = make_doc("abc def ghi");
    doc->cursor_count = 2;
    cursor_init(&doc->cursors[0]);
    cursor_move_to(&doc->cursors[0], 0, 0);
    doc->cursors[0].anchor_row = 0;
    doc->cursors[0].anchor_col = 3;
    doc->cursors[0].has_selection = true;
    cursor_init(&doc->cursors[1]);
    cursor_move_to(&doc->cursors[1], 0, 8);
    doc->cursors[1].anchor_row = 0;
    doc->cursors[1].anchor_col = 11;
    doc->cursors[1].has_selection = true;
    document_delete_selection(doc);
    ASSERT_EQ_STR(doc->buffer.text, " def ");
    ASSERT_FALSE(doc->cursors[0].has_selection);
    ASSERT_FALSE(doc->cursors[1].has_selection);
    free_doc(doc);
    PASS();
}

static void test_document_add_cursor_below_above(void) {
    TEST(document_add_cursor_below_above);
    Document *doc = make_doc("aaa\nbb\nccc");
    cursor_move_to(&doc->cursors[0], 1, 2);
    document_copy_selection_below(doc);
    ASSERT_EQ_INT(doc->cursor_count, 2);
    ASSERT_EQ_INT(doc->cursors[1].row, 2);
    ASSERT_EQ_INT(doc->cursors[1].col, 2);
    document_copy_selection_above(doc);
    ASSERT_EQ_INT(doc->cursor_count, 3);
    ASSERT_EQ_INT(doc->cursors[2].row, 0);
    ASSERT_EQ_INT(doc->cursors[2].col, 2);
    free_doc(doc);
    PASS();
}

static void test_document_delete_char_multi(void) {
    TEST(document_delete_char_multi);
    Document *doc = make_doc("aaa bbb ccc");
    doc->cursor_count = 3;
    cursor_init(&doc->cursors[0]);
    cursor_move_to(&doc->cursors[0], 0, 1);
    cursor_init(&doc->cursors[1]);
    cursor_move_to(&doc->cursors[1], 0, 5);
    cursor_init(&doc->cursors[2]);
    cursor_move_to(&doc->cursors[2], 0, 9);
    document_delete_char_multi(doc);
    ASSERT_EQ_STR(doc->buffer.text, "aa bb cc");
    free_doc(doc);
    PASS();
}

static void test_document_delete_char_multi_undo_redo(void) {
    TEST(document_delete_char_multi_undo_redo);
    Document *doc = make_doc("aaa bbb ccc");
    doc->cursor_count = 3;
    cursor_init(&doc->cursors[0]);
    cursor_move_to(&doc->cursors[0], 0, 1);
    cursor_init(&doc->cursors[1]);
    cursor_move_to(&doc->cursors[1], 0, 5);
    cursor_init(&doc->cursors[2]);
    cursor_move_to(&doc->cursors[2], 0, 9);
    document_delete_char_multi(doc);
    ASSERT_EQ_STR(doc->buffer.text, "aa bb cc");
    document_undo(doc);
    ASSERT_EQ_STR(doc->buffer.text, "aaa bbb ccc");
    document_redo(doc);
    ASSERT_EQ_STR(doc->buffer.text, "aa bb cc");
    free_doc(doc);
    PASS();
}

static void test_document_newline_multi(void) {
    TEST(document_newline_multi);
    Document *doc = make_doc("abc def ghi");
    doc->cursor_count = 3;
    cursor_init(&doc->cursors[0]);
    cursor_move_to(&doc->cursors[0], 0, 3);
    cursor_init(&doc->cursors[1]);
    cursor_move_to(&doc->cursors[1], 0, 7);
    cursor_init(&doc->cursors[2]);
    cursor_move_to(&doc->cursors[2], 0, 11);
    document_newline_multi(doc);
    ASSERT_EQ_SIZE(buffer_line_count(&doc->buffer), 4);
    free_doc(doc);
    PASS();
}

static void test_document_newline_multi_undo_redo(void) {
    TEST(document_newline_multi_undo_redo);
    Document *doc = make_doc("abc def ghi");
    doc->cursor_count = 3;
    cursor_init(&doc->cursors[0]);
    cursor_move_to(&doc->cursors[0], 0, 3);
    cursor_init(&doc->cursors[1]);
    cursor_move_to(&doc->cursors[1], 0, 7);
    cursor_init(&doc->cursors[2]);
    cursor_move_to(&doc->cursors[2], 0, 11);
    document_newline_multi(doc);
    ASSERT_EQ_SIZE(buffer_line_count(&doc->buffer), 4);
    document_undo(doc);
    ASSERT_EQ_STR(doc->buffer.text, "abc def ghi");
    document_redo(doc);
    ASSERT_EQ_SIZE(buffer_line_count(&doc->buffer), 4);
    free_doc(doc);
    PASS();
}

/* ================================================================
 * ADDITIONAL DOCUMENT TESTS - Selection ops
 * ================================================================ */

static void test_document_replace_selection_char(void) {
    TEST(document_replace_selection_char);
    Document *doc = make_doc("hello world");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_replace_selection_char(doc, 'X');
    ASSERT_EQ_STR(doc->buffer.text, "XXXXX world");
    free_doc(doc);
    PASS();
}

static void test_document_replace_selection_yanked(void) {
    TEST(document_replace_selection_yanked);
    Document *doc = make_doc("hello world");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    doc->clipboard = strdup("XX");
    doc->clipboard_len = 2;
    document_replace_selection_yanked(doc);
    ASSERT_EQ_STR(doc->buffer.text, "XX world");
    free_doc(doc);
    PASS();
}

static void test_document_keep_primary_selection(void) {
    TEST(document_keep_primary_selection);
    Document *doc = make_doc("hello");
    doc->cursor_count = 3;
    cursor_init(&doc->cursors[1]);
    cursor_move_to(&doc->cursors[1], 0, 1);
    cursor_init(&doc->cursors[2]);
    cursor_move_to(&doc->cursors[2], 0, 2);
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_keep_primary_selection(doc);
    ASSERT_EQ_INT(doc->cursor_count, 1);
    ASSERT_FALSE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 5);
    free_doc(doc);
    PASS();
}

static void test_document_copy_selection_below(void) {
    TEST(document_copy_selection_below);
    Document *doc = make_doc("aaa\nbbb");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 3;
    document_copy_selection_below(doc);
    const char *l1 = buffer_line_ptr(&doc->buffer, 1);
    ASSERT(l1[0] == 'a');
    free_doc(doc);
    PASS();
}

static void test_document_join_lines_selection(void) {
    TEST(document_join_lines_selection);
    Document *doc = make_doc("hello\nworld\nfoo");
    cursor_select_start(&doc->cursors[0]);
    document_cursor_to(doc, 1, 0);
    document_join_lines_selection(doc);
    ASSERT_EQ_STR(doc->buffer.text, "helloworld\nfoo");
    free_doc(doc);
    PASS();
}

static void test_document_force_selection_forward(void) {
    TEST(document_force_selection_forward);
    Document *doc = make_doc("hello");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].anchor_row = 0;
    doc->cursors[0].anchor_col = 5;
    doc->cursors[0].row = 0;
    doc->cursors[0].col = 0;
    document_force_selection_forward(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].anchor_row, 0);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 0);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 5);
    free_doc(doc);
    PASS();
}

/* ================================================================
 * ADDITIONAL DOCUMENT TESTS - Scroll/Viewport
 * ================================================================ */

static void test_document_cursor_page_down(void) {
    TEST(document_cursor_page_down);
    Document *doc = make_doc("l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10");
    doc->viewport_lines = 3;
    document_cursor_page_down(doc);
    ASSERT(doc->cursors[0].row >= 3);
    free_doc(doc);
    PASS();
}

static void test_document_scroll_horizontal_center(void) {
    TEST(document_scroll_horizontal_center);
    Document *doc = make_doc("hello");
    document_cursor_to(doc, 0, 50);
    document_scroll_horizontal_center(doc);
    ASSERT(doc->scroll_x >= 0);
    free_doc(doc);
    PASS();
}

static void test_document_scroll_top(void) {
    TEST(document_scroll_top);
    Document *doc = make_doc("l1\nl2\nl3\nl4\nl5");
    doc->scroll_y = 3;
    document_cursor_to(doc, 2, 0);
    document_scroll_top(doc, 10);
    ASSERT_EQ_INT(doc->scroll_y, 2);
    free_doc(doc);
    PASS();
}

static void test_document_scroll_bottom(void) {
    TEST(document_scroll_bottom);
    Document *doc = make_doc("l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10");
    doc->viewport_lines = 5;
    document_cursor_to(doc, 0, 0);
    document_scroll_bottom(doc, 5);
    ASSERT(doc->scroll_y >= 0);
    free_doc(doc);
    PASS();
}

static void test_document_shrink_to_line_bounds(void) {
    TEST(document_shrink_to_line_bounds);
    Document *doc = make_doc("hello world");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].anchor_row = 0;
    doc->cursors[0].anchor_col = 0;
    doc->cursors[0].row = 0;
    doc->cursors[0].col = 11;
    document_shrink_to_line_bounds(doc);
    ASSERT_FALSE(doc->cursors[0].has_selection);
    free_doc(doc);
    PASS();
}

static void test_document_remove_primary_selection(void) {
    TEST(document_remove_primary_selection);
    Document *doc = make_doc("hello");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_remove_primary_selection(doc);
    ASSERT_FALSE(doc->cursors[0].has_selection);
    free_doc(doc);
    PASS();
}

/* ================================================================
 * ADDITIONAL DOCUMENT TESTS - View navigation
 * ================================================================ */

static void test_document_goto_view_top(void) {
    TEST(document_goto_view_top);
    Document *doc = make_doc("l1\nl2\nl3\nl4\nl5");
    doc->scroll_y = 2;
    document_goto_view_top(doc);
    ASSERT_EQ_INT(doc->cursors[0].row, 2);
    free_doc(doc);
    PASS();
}

static void test_document_goto_view_center(void) {
    TEST(document_goto_view_center);
    Document *doc = make_doc("l1\nl2\nl3\nl4\nl5");
    doc->scroll_y = 0;
    doc->viewport_lines = 4;
    document_goto_view_center(doc);
    ASSERT_EQ_INT(doc->cursors[0].row, 2);
    free_doc(doc);
    PASS();
}

static void test_document_goto_view_bottom(void) {
    TEST(document_goto_view_bottom);
    Document *doc = make_doc("l1\nl2\nl3\nl4\nl5");
    doc->scroll_y = 0;
    doc->viewport_lines = 3;
    document_goto_view_bottom(doc);
    ASSERT_EQ_INT(doc->cursors[0].row, 2);
    free_doc(doc);
    PASS();
}

/* ================================================================
 * ADDITIONAL DOCUMENT TESTS - Comment, Join, Copy, Delete word
 * ================================================================ */

static void test_document_comment_toggle(void) {
    TEST(document_comment_toggle);
    Document *doc = make_doc("hello");
    document_comment_toggle(doc);
    ASSERT_EQ_STR(doc->buffer.text, "//hello");
    document_comment_toggle(doc);
    ASSERT_EQ_STR(doc->buffer.text, "hello");
    free_doc(doc);
    PASS();
}

static void test_document_comment_toggle_indented(void) {
    TEST(document_comment_toggle_indented);
    Document *doc = make_doc("    hello");
    document_comment_toggle(doc);
    ASSERT_EQ_STR(doc->buffer.text, "    //hello");
    free_doc(doc);
    PASS();
}

static void test_document_join_lines_with_space(void) {
    TEST(document_join_lines_with_space);
    Document *doc = make_doc("hello\nworld");
    document_join_lines_with_space(doc);
    ASSERT_EQ_STR(doc->buffer.text, "hello world");
    free_doc(doc);
    PASS();
}

static void test_document_join_lines_with_space_selection(void) {
    TEST(document_join_lines_with_space_selection);
    Document *doc = make_doc("a\nb\nc");
    cursor_select_start(&doc->cursors[0]);
    document_cursor_to(doc, 2, 1);
    document_join_lines_with_space(doc);
    ASSERT_EQ_STR(doc->buffer.text, "a b c");
    free_doc(doc);
    PASS();
}

static void test_document_delete_word_forward(void) {
    TEST(document_delete_word_forward);
    Document *doc = make_doc("hello world");
    document_cursor_to(doc, 0, 0);
    document_delete_word_forward(doc);
    ASSERT_EQ_STR(doc->buffer.text, " world");
    free_doc(doc);
    PASS();
}

static void test_document_delete_word_forward_mid(void) {
    TEST(document_delete_word_forward_mid);
    Document *doc = make_doc("hello world");
    document_cursor_to(doc, 0, 6);
    document_delete_word_forward(doc);
    ASSERT_EQ_STR(doc->buffer.text, "hello ");
    free_doc(doc);
    PASS();
}

/* ================================================================
 * ADDITIONAL DOCUMENT TESTS - Page/Half-page extend
 * ================================================================ */

static void test_document_page_down_extend(void) {
    TEST(document_page_down_extend);
    Document *doc = make_doc("l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10");
    doc->viewport_lines = 3;
    document_page_down_extend(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT(doc->cursors[0].row >= 3);
    free_doc(doc);
    PASS();
}

static void test_document_page_up_extend(void) {
    TEST(document_page_up_extend);
    Document *doc = make_doc("l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10");
    doc->viewport_lines = 3;
    document_cursor_to(doc, 5, 0);
    document_page_up_extend(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    free_doc(doc);
    PASS();
}

static void test_document_half_page_down_extend(void) {
    TEST(document_half_page_down_extend);
    Document *doc = make_doc("l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10");
    doc->viewport_lines = 8;
    document_half_page_down_extend(doc, 8);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    free_doc(doc);
    PASS();
}

static void test_document_half_page_up_extend(void) {
    TEST(document_half_page_up_extend);
    Document *doc = make_doc("l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10");
    doc->viewport_lines = 8;
    document_cursor_to(doc, 5, 0);
    document_half_page_up_extend(doc, 8);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    free_doc(doc);
    PASS();
}

/* ================================================================
 * ADDITIONAL DOCUMENT TESTS - Split/Merge selections
 * ================================================================ */

static void test_document_split_selection_newlines(void) {
    TEST(document_split_selection_newlines);
    Document *doc = make_doc("hello\nworld");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_cursor_to(doc, 1, 3);
    document_split_selection_newlines(doc);
    ASSERT_EQ_INT(doc->cursors[0].row, 1);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_merge_selections(void) {
    TEST(document_merge_selections);
    Document *doc = make_doc("hello");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_merge_selections(doc);
    ASSERT_FALSE(doc->cursors[0].has_selection);
    free_doc(doc);
    PASS();
}

static void test_document_merge_consecutive_selections(void) {
    TEST(document_merge_consecutive_selections);
    Document *doc = make_doc("hello");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 3;
    document_merge_consecutive_selections(doc);
    ASSERT_FALSE(doc->cursors[0].has_selection);
    free_doc(doc);
    PASS();
}

/* ================================================================
 * ADDITIONAL DOCUMENT TESTS - Align, Jumplist forward
 * ================================================================ */

static void test_document_align_selections(void) {
    TEST(document_align_selections);
    Document *doc = make_doc("aa\nbbbb\nccc");
    cursor_select_start(&doc->cursors[0]);
    document_cursor_to(doc, 2, 3);
    document_align_selections(doc);
    size_t len0 = buffer_line_len(&doc->buffer, 0);
    size_t len1 = buffer_line_len(&doc->buffer, 1);
    ASSERT(len0 == len1);
    free_doc(doc);
    PASS();
}

static void test_document_jumplist_forward(void) {
    TEST(document_jumplist_forward);
    Document *doc = make_doc("hello");
    document_jumplist_push(doc, 0, 0);
    document_jumplist_push(doc, 0, 3);
    document_jumplist_push(doc, 0, 5);
    document_jumplist_backward(doc);
    document_jumplist_backward(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 3);
    document_jumplist_forward(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 5);
    free_doc(doc);
    PASS();
}

static void test_document_jumplist_forward_at_end(void) {
    TEST(document_jumplist_forward_at_end);
    Document *doc = make_doc("hello");
    document_jumplist_push(doc, 0, 5);
    document_jumplist_forward(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

/* ================================================================
 * ADDITIONAL DOCUMENT TESTS - Surround variants
 * ================================================================ */

static void test_document_surround_curly(void) {
    TEST(document_surround_curly);
    Document *doc = make_doc("hello");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_surround(doc, '{');
    ASSERT_EQ_STR(doc->buffer.text, "{hello}");
    free_doc(doc);
    PASS();
}

static void test_document_surround_bracket(void) {
    TEST(document_surround_bracket);
    Document *doc = make_doc("hello");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_surround(doc, '[');
    ASSERT_EQ_STR(doc->buffer.text, "[hello]");
    free_doc(doc);
    PASS();
}

static void test_document_surround_angle(void) {
    TEST(document_surround_angle);
    Document *doc = make_doc("hello");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_surround(doc, '<');
    ASSERT_EQ_STR(doc->buffer.text, "<hello>");
    free_doc(doc);
    PASS();
}

static void test_document_delete_surround_curly(void) {
    TEST(document_delete_surround_curly);
    Document *doc = make_doc("{hello}");
    document_cursor_to(doc, 0, 1);
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 6;
    document_delete_surround(doc, '{');
    ASSERT_EQ_STR(doc->buffer.text, "hello");
    free_doc(doc);
    PASS();
}

static void test_document_replace_surround(void) {
    TEST(document_replace_surround);
    Document *doc = make_doc("{hello}");
    document_cursor_to(doc, 0, 1);
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 6;
    document_replace_surround(doc, '{', '[');
    ASSERT_EQ_STR(doc->buffer.text, "[hello}");
    free_doc(doc);
    PASS();
}

/* ================================================================
 * ADDITIONAL DOCUMENT TESTS - View scroll page
 * ================================================================ */

static void test_document_view_page_down(void) {
    TEST(document_view_page_down);
    Document *doc = make_doc("l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10");
    doc->viewport_lines = 3;
    doc->scroll_y = 0;
    document_view_page_down(doc);
    ASSERT(doc->scroll_y > 0);
    free_doc(doc);
    PASS();
}

static void test_document_view_page_up(void) {
    TEST(document_view_page_up);
    Document *doc = make_doc("l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10");
    doc->viewport_lines = 3;
    doc->scroll_y = 5;
    document_view_page_up(doc);
    ASSERT(doc->scroll_y < 5);
    free_doc(doc);
    PASS();
}

static void test_document_view_half_page_down(void) {
    TEST(document_view_half_page_down);
    Document *doc = make_doc("l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10");
    doc->viewport_lines = 6;
    doc->scroll_y = 0;
    document_view_half_page_down(doc);
    ASSERT(doc->scroll_y > 0);
    free_doc(doc);
    PASS();
}

static void test_document_view_half_page_up(void) {
    TEST(document_view_half_page_up);
    Document *doc = make_doc("l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10");
    doc->viewport_lines = 6;
    doc->scroll_y = 5;
    document_view_half_page_up(doc);
    ASSERT(doc->scroll_y < 5);
    free_doc(doc);
    PASS();
}

/* ================================================================
 * ADDITIONAL DOCUMENT TESTS - Match bracket, Goto last mod
 * ================================================================ */

static void test_document_match_bracket_forward(void) {
    TEST(document_match_bracket_forward);
    Document *doc = make_doc("a(b)c");
    document_cursor_to(doc, 0, 1);
    document_match_bracket(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 3);
    free_doc(doc);
    PASS();
}

static void test_document_match_bracket_backward(void) {
    TEST(document_match_bracket_backward);
    Document *doc = make_doc("a(b)c");
    document_cursor_to(doc, 0, 3);
    document_match_bracket(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 1);
    free_doc(doc);
    PASS();
}

static void test_document_match_bracket_nested(void) {
    TEST(document_match_bracket_nested);
    Document *doc = make_doc("((a))");
    document_cursor_to(doc, 0, 0);
    document_match_bracket(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    free_doc(doc);
    PASS();
}

static void test_document_match_bracket_square(void) {
    TEST(document_match_bracket_square);
    Document *doc = make_doc("[abc]");
    document_cursor_to(doc, 0, 0);
    document_match_bracket(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    free_doc(doc);
    PASS();
}

static void test_document_match_bracket_curly(void) {
    TEST(document_match_bracket_curly);
    Document *doc = make_doc("{abc}");
    document_cursor_to(doc, 0, 4);
    document_match_bracket(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    free_doc(doc);
    PASS();
}

static void test_document_goto_last_modification(void) {
    TEST(document_goto_last_modification);
    Document *doc = make_doc("hello");
    document_insert_char(doc, 'X');
    document_insert_char(doc, 'Y');
    document_goto_last_modification(doc);
    ASSERT(doc->cursors[0].col >= 0);
    free_doc(doc);
    PASS();
}

/* ================================================================
 * ADDITIONAL DOCUMENT TESTS - Select literal, all matches
 * ================================================================ */

static void test_document_select_literal(void) {
    TEST(document_select_literal);
    Document *doc = make_doc("foo bar baz");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 4;
    document_cursor_to(doc, 0, 11);
    document_select_literal(doc, "bar", 3);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    free_doc(doc);
    PASS();
}

static void test_document_select_all_matches(void) {
    TEST(document_select_all_matches);
    Document *doc = make_doc("abc abc abc");
    document_select_all_matches(doc, "abc", 3);
    ASSERT(doc->cursor_count >= 1);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    free_doc(doc);
    PASS();
}

static void test_document_split_literal(void) {
    TEST(document_split_literal);
    Document *doc = make_doc("foo bar baz");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 4;
    document_cursor_to(doc, 0, 11);
    document_split_literal(doc, "bar", 3);
    ASSERT_FALSE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 7);
    free_doc(doc);
    PASS();
}

static void test_document_split_all_matches(void) {
    TEST(document_split_all_matches);
    Document *doc = make_doc("aXbXc");
    cursor_select_start(&doc->cursors[0]);
    document_cursor_to(doc, 0, 5);
    document_split_all_matches(doc, "X", 1);
    ASSERT(doc->cursor_count >= 1);
    free_doc(doc);
    PASS();
}

static void test_document_keep_matching(void) {
    TEST(document_keep_matching);
    Document *doc = make_doc("abc abc abc");
    document_keep_matching(doc, "abc", 3);
    ASSERT(doc->cursor_count >= 1);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    free_doc(doc);
    PASS();
}

/* ================================================================
 * ADDITIONAL DOCUMENT TESTS - File I/O
 * ================================================================ */

static void test_document_open_save(void) {
    TEST(document_open_save);
    const char *tmppath = "/tmp/dragon_test_doc.txt";
    Document *doc = make_doc("hello world\n");
    doc->filepath = strdup(tmppath);
    document_save(doc);
    ASSERT_FALSE(doc->dirty);

    Document *doc2 = make_doc(NULL);
    document_open(doc2, tmppath);
    ASSERT_EQ_STR(doc2->buffer.text, "hello world\n");
    ASSERT_EQ_INT(doc2->cursors[0].row, 0);
    ASSERT_FALSE(doc2->dirty);
    free_doc(doc);
    free_doc(doc2);
    unlink(tmppath);
    PASS();
}

static void test_document_save_as(void) {
    TEST(document_save_as);
    const char *tmp1 = "/tmp/dragon_test_sa1.txt";
    const char *tmp2 = "/tmp/dragon_test_sa2.txt";
    Document *doc = make_doc("test data");
    document_save_as(doc, tmp1);
    ASSERT_EQ_STR(doc->filepath, tmp1);
    ASSERT_FALSE(doc->dirty);
    document_save_as(doc, tmp2);
    ASSERT_EQ_STR(doc->filepath, tmp2);

    Document *doc2 = make_doc(NULL);
    document_open(doc2, tmp2);
    ASSERT_EQ_STR(doc2->buffer.text, "test data");
    free_doc(doc);
    free_doc(doc2);
    unlink(tmp1);
    unlink(tmp2);
    PASS();
}

static void test_document_insert_file(void) {
    TEST(document_insert_file);
    const char *tmppath = "/tmp/dragon_test_if.txt";
    FILE *f = fopen(tmppath, "w");
    fprintf(f, "INSERTED");
    fclose(f);

    Document *doc = make_doc("BEFORE");
    document_cursor_to(doc, 0, 6);
    document_insert_file(doc, tmppath);
    ASSERT_EQ_STR(doc->buffer.text, "BEFOREINSERTED");
    free_doc(doc);
    unlink(tmppath);
    PASS();
}

static void test_document_detect_language(void) {
    TEST(document_detect_language);
    Document *doc = make_doc(NULL);
    struct { const char *ext; const char *lang; } cases[] = {
        {"test.c", "c"}, {"test.h", "c"}, {"test.cpp", "cpp"},
        {"test.rs", "rust"}, {"test.py", "python"}, {"test.go", "go"},
        {"test.js", "javascript"}, {"test.ts", "typescript"},
        {"test.java", "java"}, {"test.m", "objc"}, {"test.mm", "objcpp"},
        {"test.cu", "cuda"}, {"test.hh", "cpp"}, {"test.cc", "cpp"},
    };
    for (int i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++) {
        free(doc->filepath);
        doc->filepath = strdup(cases[i].ext);
        document_detect_language(doc);
        ASSERT(doc->language_id != NULL);
        ASSERT_EQ_STR(doc->language_id, cases[i].lang);
    }
    free(doc->filepath);
    doc->filepath = strdup("unknown.xyz");
    document_detect_language(doc);
    ASSERT(doc->language_id == NULL);
    free(doc->filepath);
    doc->filepath = NULL;
    document_detect_language(doc);
    ASSERT(doc->language_id == NULL);
    free_doc(doc);
    PASS();
}

/* ================================================================
 * TEXT OBJECT TESTS
 * ================================================================ */

static void test_text_obj_select_inside_word(void) {
    TEST(text_obj_select_inside_word);
    Document *doc = make_doc("hello world");
    document_cursor_to(doc, 0, 2);
    document_select_inside_word(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].row, 0);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    ASSERT_EQ_INT(doc->cursors[0].anchor_row, 0);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 5);
    free_doc(doc);
    PASS();
}

static void test_text_obj_select_around_word(void) {
    TEST(text_obj_select_around_word);
    Document *doc = make_doc("hello world");
    document_cursor_to(doc, 0, 2);
    document_select_around_word(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 0);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 6);
    free_doc(doc);
    PASS();
}

static void test_text_obj_select_inside_word_mid(void) {
    TEST(text_obj_select_inside_word_mid);
    Document *doc = make_doc("foo bar baz");
    document_cursor_to(doc, 0, 5);
    document_select_inside_word(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 7);
    free_doc(doc);
    PASS();
}

static void test_text_obj_select_inside_paren(void) {
    TEST(text_obj_select_inside_paren);
    Document *doc = make_doc("foo(bar) baz");
    document_cursor_to(doc, 0, 5);
    document_select_inside_paren(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 7);
    free_doc(doc);
    PASS();
}

static void test_text_obj_select_around_paren(void) {
    TEST(text_obj_select_around_paren);
    Document *doc = make_doc("foo(bar) baz");
    document_cursor_to(doc, 0, 5);
    document_select_around_paren(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 3);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 8);
    free_doc(doc);
    PASS();
}

static void test_text_obj_select_inside_bracket(void) {
    TEST(text_obj_select_inside_bracket);
    Document *doc = make_doc("foo[bar] baz");
    document_cursor_to(doc, 0, 5);
    document_select_inside_bracket(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 7);
    free_doc(doc);
    PASS();
}

static void test_text_obj_select_around_bracket(void) {
    TEST(text_obj_select_around_bracket);
    Document *doc = make_doc("foo[bar] baz");
    document_cursor_to(doc, 0, 5);
    document_select_around_bracket(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 3);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 8);
    free_doc(doc);
    PASS();
}

static void test_text_obj_select_inside_curly(void) {
    TEST(text_obj_select_inside_curly);
    Document *doc = make_doc("foo{bar} baz");
    document_cursor_to(doc, 0, 5);
    document_select_inside_curly(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 7);
    free_doc(doc);
    PASS();
}

static void test_text_obj_select_around_curly(void) {
    TEST(text_obj_select_around_curly);
    Document *doc = make_doc("foo{bar} baz");
    document_cursor_to(doc, 0, 5);
    document_select_around_curly(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 3);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 8);
    free_doc(doc);
    PASS();
}

static void test_text_obj_select_inside_angle(void) {
    TEST(text_obj_select_inside_angle);
    Document *doc = make_doc("foo<bar> baz");
    document_cursor_to(doc, 0, 5);
    document_select_inside_angle(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 7);
    free_doc(doc);
    PASS();
}

static void test_text_obj_select_around_angle(void) {
    TEST(text_obj_select_around_angle);
    Document *doc = make_doc("foo<bar> baz");
    document_cursor_to(doc, 0, 5);
    document_select_around_angle(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 3);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 8);
    free_doc(doc);
    PASS();
}

static void test_text_obj_select_inside_quote(void) {
    TEST(text_obj_select_inside_quote);
    Document *doc = make_doc("foo\"bar\" baz");
    document_cursor_to(doc, 0, 5);
    document_select_inside_quote(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 7);
    free_doc(doc);
    PASS();
}

static void test_text_obj_select_around_quote(void) {
    TEST(text_obj_select_around_quote);
    Document *doc = make_doc("foo\"bar\" baz");
    document_cursor_to(doc, 0, 5);
    document_select_around_quote(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 3);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 8);
    free_doc(doc);
    PASS();
}

static void test_text_obj_select_inside_backtick(void) {
    TEST(text_obj_select_inside_backtick);
    Document *doc = make_doc("foo`bar` baz");
    document_cursor_to(doc, 0, 5);
    document_select_inside_backtick(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 4);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 7);
    free_doc(doc);
    PASS();
}

static void test_text_obj_select_around_backtick(void) {
    TEST(text_obj_select_around_backtick);
    Document *doc = make_doc("foo`bar` baz");
    document_cursor_to(doc, 0, 5);
    document_select_around_backtick(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT_EQ_INT(doc->cursors[0].col, 3);
    ASSERT_EQ_INT(doc->cursors[0].anchor_col, 8);
    free_doc(doc);
    PASS();
}

static void test_text_obj_select_inside_paragraph(void) {
    TEST(text_obj_select_inside_paragraph);
    Document *doc = make_doc("hello world\n\nfoo bar\n");
    document_cursor_to(doc, 2, 2);
    document_select_inside_paragraph(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    /* Selection should be non-empty */
    int sr, sc, er, ec;
    cursor_normalize(&doc->cursors[0], &sr, &sc, &er, &ec);
    ASSERT((er > sr) || (er == sr && ec > sc));
    free_doc(doc);
    PASS();
}

static void test_text_obj_select_around_paragraph(void) {
    TEST(text_obj_select_around_paragraph);
    Document *doc = make_doc("hello world\n\nfoo bar\n");
    document_cursor_to(doc, 2, 2);
    document_select_around_paragraph(doc);
    ASSERT_TRUE(doc->cursors[0].has_selection);
    ASSERT(doc->cursors[0].row <= 2);
    free_doc(doc);
    PASS();
}

/* ================================================================
 * MACRO TESTS
 * ================================================================ */

static void test_macro_init(void) {
    TEST(macro_init);
    MacroState ms;
    macro_init(&ms);
    ASSERT_FALSE(macro_is_recording(&ms));
    ASSERT_EQ_INT(ms.active_slot, -1);
    ASSERT_EQ_INT(ms.last_replayed, -1);
    PASS();
}

static void test_macro_record_and_stop(void) {
    TEST(macro_record_and_stop);
    MacroState ms;
    macro_init(&ms);
    ASSERT_TRUE(macro_start_record(&ms, 0));
    ASSERT_TRUE(macro_is_recording(&ms));
    ASSERT_EQ_INT(ms.active_slot, 0);
    macro_stop_record(&ms);
    ASSERT_FALSE(macro_is_recording(&ms));
    ASSERT_EQ_INT(ms.active_slot, -1);
    PASS();
}

static void test_macro_record_keys(void) {
    TEST(macro_record_keys);
    MacroState ms;
    macro_init(&ms);
    macro_start_record(&ms, 5);
    macro_record_key(&ms, 'h');
    macro_record_key(&ms, 'j');
    macro_record_key(&ms, 'l');
    macro_stop_record(&ms);
    ASSERT_EQ_INT(ms.slots[5].len, 3);
    ASSERT_EQ_INT(ms.slots[5].keys[0], 'h');
    ASSERT_EQ_INT(ms.slots[5].keys[1], 'j');
    ASSERT_EQ_INT(ms.slots[5].keys[2], 'l');
    PASS();
}

static void test_macro_replay(void) {
    TEST(macro_replay);
    MacroState ms;
    macro_init(&ms);
    macro_start_record(&ms, 0);
    macro_record_key(&ms, 'x');
    macro_record_key(&ms, 'y');
    macro_stop_record(&ms);
    ASSERT_TRUE(macro_replay(&ms, 0));
    ASSERT_EQ_INT(ms.last_replayed, 0);
    PASS();
}

static void test_macro_cannot_record_while_recording(void) {
    TEST(macro_cannot_record_while_recording);
    MacroState ms;
    macro_init(&ms);
    ASSERT_TRUE(macro_start_record(&ms, 0));
    ASSERT_FALSE(macro_start_record(&ms, 1));
    macro_stop_record(&ms);
    PASS();
}

static void test_macro_replay_empty_is_false(void) {
    TEST(macro_replay_empty_is_false);
    MacroState ms;
    macro_init(&ms);
    ASSERT_FALSE(macro_replay(&ms, 0));
    ASSERT_FALSE(macro_replay(&ms, 25));
    PASS();
}

static void test_macro_stop_noop_when_not_recording(void) {
    TEST(macro_stop_noop_when_not_recording);
    MacroState ms;
    macro_init(&ms);
    macro_stop_record(&ms); /* should not crash */
    ASSERT_FALSE(macro_is_recording(&ms));
    PASS();
}

/* ================================================================
 * REFLOW TESTS
 * ================================================================ */

static void test_reflow_simple(void) {
    TEST(reflow_simple);
    Document *doc = make_doc("hello world foo bar baz qux");
    document_reflow(doc, 10);
    /* Should wrap at word boundaries */
    ASSERT(doc->buffer.len > 0);
    /* Check that no line exceeds width (approximate) */
    size_t line_start = 0;
    for (size_t i = 0; i <= doc->buffer.len; i++) {
        if (i == doc->buffer.len || doc->buffer.text[i] == '\n') {
            size_t line_len = i - line_start;
            ASSERT(line_len <= 11); /* allow 10 + possible last word */
            line_start = i + 1;
        }
    }
    free_doc(doc);
    PASS();
}

static void test_reflow_already_fits(void) {
    TEST(reflow_already_fits);
    Document *doc = make_doc("hello");
    document_reflow(doc, 80);
    /* Reflow may add trailing newline on single-line text */
    ASSERT(doc->buffer.len >= 5);
    ASSERT(memcmp(doc->buffer.text, "hello", 5) == 0);
    free_doc(doc);
    PASS();
}

static void test_reflow_preserves_newlines(void) {
    TEST(reflow_preserves_newlines);
    Document *doc = make_doc("hello\nworld");
    document_reflow(doc, 80);
    /* Should preserve the explicit newline */
    ASSERT(strchr(doc->buffer.text, '\n') != NULL);
    free_doc(doc);
    PASS();
}

/* ================================================================
 * INDENT STYLE TESTS
 * ================================================================ */

static void test_indent_tabs_to_spaces(void) {
    TEST(indent_tabs_to_spaces);
    Document *doc = make_doc("\thello");
    document_indent_style_to_spaces(doc, 4);
    ASSERT_EQ_STR(doc->buffer.text, "    hello");
    free_doc(doc);
    PASS();
}

static void test_indent_spaces_to_tabs(void) {
    TEST(indent_spaces_to_tabs);
    Document *doc = make_doc("    hello");
    document_indent_style_to_tabs(doc, 4);
    ASSERT_EQ_STR(doc->buffer.text, "\thello");
    free_doc(doc);
    PASS();
}

static void test_indent_tabs_to_spaces_mixed(void) {
    TEST(indent_tabs_to_spaces_mixed);
    Document *doc = make_doc("\t\thello");
    document_indent_style_to_spaces(doc, 4);
    ASSERT_EQ_STR(doc->buffer.text, "        hello");
    free_doc(doc);
    PASS();
}

/* ================================================================
 * ALTERNATE FILE TESTS
 * ================================================================ */

static void test_alternate_set_get(void) {
    TEST(alternate_set_get);
    Document *doc = make_doc(NULL);
    document_set_alternate(doc, "/tmp/foo.c");
    ASSERT_EQ_STR(document_get_alternate(doc), "/tmp/foo.c");
    free_doc(doc);
    PASS();
}

static void test_alternate_null(void) {
    TEST(alternate_null);
    Document *doc = make_doc(NULL);
    ASSERT(document_get_alternate(doc) == NULL);
    document_set_alternate(doc, NULL);
    ASSERT(document_get_alternate(doc) == NULL);
    free_doc(doc);
    PASS();
}

static void test_alternate_overwrite(void) {
    TEST(alternate_overwrite);
    Document *doc = make_doc(NULL);
    document_set_alternate(doc, "/tmp/a.c");
    document_set_alternate(doc, "/tmp/b.c");
    ASSERT_EQ_STR(document_get_alternate(doc), "/tmp/b.c");
    free_doc(doc);
    PASS();
}

/* ================================================================
 * BLOCK COMMENT TESTS
 * ================================================================ */

static void test_comment_block_add(void) {
    TEST(comment_block_add);
    Document *doc = make_doc("hello world");
    document_comment_toggle_block(doc, "/*", "*/");
    ASSERT_EQ_SIZE(doc->buffer.len, 15);
    ASSERT(doc->buffer.text[0] == '/');
    ASSERT(doc->buffer.text[1] == '*');
    ASSERT(doc->buffer.text[13] == '*');
    ASSERT(doc->buffer.text[14] == '/');
    free_doc(doc);
    PASS();
}

static void test_comment_block_remove(void) {
    TEST(comment_block_remove);
    Document *doc = make_doc("/*hello world*/");
    document_comment_toggle_block(doc, "/*", "*/");
    ASSERT_EQ_SIZE(doc->buffer.len, 11);
    ASSERT(doc->buffer.text[0] == 'h');
    free_doc(doc);
    PASS();
}

static void test_comment_block_toggle(void) {
    TEST(comment_block_toggle);
    Document *doc = make_doc("test");
    document_comment_toggle_block(doc, "/*", "*/");
    ASSERT_EQ_SIZE(doc->buffer.len, 8);  // /*test*/ = 8 chars
    ASSERT(doc->buffer.text[0] == '/');
    ASSERT(doc->buffer.text[1] == '*');
    document_comment_toggle_block(doc, "/*", "*/");
    ASSERT_EQ_SIZE(doc->buffer.len, 4);
    ASSERT(doc->buffer.text[0] == 't');
    free_doc(doc);
    PASS();
}

static void test_comment_line_add(void) {
    TEST(comment_line_add);
    Document *doc = make_doc("hello\nworld");
    /* Select both lines */
    cursor_select_start(&doc->cursors[0]);
    document_cursor_to(doc, 1, 5);
    document_comment_toggle_line(doc, "//");
    ASSERT_EQ_SIZE(doc->buffer.len, 15);
    ASSERT(doc->buffer.text[0] == '/');
    ASSERT(doc->buffer.text[1] == '/');
    ASSERT(doc->buffer.text[7] == '\n');
    ASSERT(doc->buffer.text[8] == '/');
    ASSERT(doc->buffer.text[9] == '/');
    free_doc(doc);
    PASS();
}

static void test_comment_line_remove(void) {
    TEST(comment_line_remove);
    Document *doc = make_doc("//hello\n//world");
    cursor_select_start(&doc->cursors[0]);
    document_cursor_to(doc, 1, 7);
    document_comment_toggle_line(doc, "//");
    ASSERT_EQ_SIZE(doc->buffer.len, 11);
    ASSERT(doc->buffer.text[0] == 'h');
    ASSERT(doc->buffer.text[5] == '\n');
    ASSERT(doc->buffer.text[6] == 'w');
    free_doc(doc);
    PASS();
}

static void test_comment_line_toggle(void) {
    TEST(comment_line_toggle);
    Document *doc = make_doc("foo\nbar");
    cursor_select_start(&doc->cursors[0]);
    document_cursor_to(doc, 1, 3);
    document_comment_toggle_line(doc, "#");
    ASSERT_EQ_SIZE(doc->buffer.len, 9);  // #foo\n#bar = 9 chars
    ASSERT(doc->buffer.text[0] == '#');
    /* After toggle, move cursor to start then select both lines */
    document_cursor_to(doc, 0, 0);
    cursor_select_start(&doc->cursors[0]);
    document_cursor_to(doc, 1, 4);
    document_comment_toggle_line(doc, "#");
    ASSERT_EQ_SIZE(doc->buffer.len, 7);
    ASSERT(doc->buffer.text[0] == 'f');
    free_doc(doc);
    PASS();
}

/* ================================================================
 * WINDOW MANAGER TESTS
 * ================================================================ */

static void test_window_init(void) {
    TEST(window_init);
    WindowManager wm;
    window_manager_init(&wm);
    ASSERT_EQ_INT(wm.count, 1);
    ASSERT_EQ_INT(wm.active, 0);
    ASSERT_TRUE(wm.windows[0].visible);
    ASSERT_EQ_INT(wm.windows[0].parent, -1);
    PASS();
}

static void test_window_split_vertical(void) {
    TEST(window_split_vertical);
    WindowManager wm;
    window_manager_init(&wm);
    int idx = window_split_vertical(&wm, 1);
    ASSERT(idx > 0);
    ASSERT_EQ_INT(wm.count, 2);
    ASSERT_EQ_INT(wm.active, idx);
    ASSERT_TRUE(wm.windows[idx].visible);
    ASSERT_EQ_INT(wm.windows[idx].doc_index, 1);
    ASSERT_EQ_INT(wm.windows[idx].parent, 0);
    PASS();
}

static void test_window_split_horizontal(void) {
    TEST(window_split_horizontal);
    WindowManager wm;
    window_manager_init(&wm);
    int idx = window_split_horizontal(&wm, 2);
    ASSERT(idx > 0);
    ASSERT_EQ_INT(wm.count, 2);
    ASSERT_TRUE(wm.windows[idx].visible);
    ASSERT_EQ_INT(wm.windows[idx].parent, 0);
    PASS();
}

static void test_window_close(void) {
    TEST(window_close);
    WindowManager wm;
    window_manager_init(&wm);
    window_split_vertical(&wm, 1);
    ASSERT_EQ_INT(wm.count, 2);
    window_close(&wm);
    ASSERT_EQ_INT(wm.count, 1);
    ASSERT_TRUE(wm.windows[0].visible);
    ASSERT_EQ_INT(wm.active, 0);
    PASS();
}

static void test_window_next_prev(void) {
    TEST(window_next_prev);
    WindowManager wm;
    window_manager_init(&wm);
    window_split_vertical(&wm, 1);
    window_next(&wm);
    ASSERT_EQ_INT(wm.active, 0);
    window_next(&wm);
    ASSERT_EQ_INT(wm.active, 1);
    window_prev(&wm);
    ASSERT_EQ_INT(wm.active, 0);
    PASS();
}

static void test_window_goto_left_right(void) {
    TEST(window_goto_left_right);
    WindowManager wm;
    window_manager_init(&wm);
    int idx = window_split_vertical(&wm, 1);
    wm.active = idx;
    window_goto_left(&wm);
    ASSERT_EQ_INT(wm.active, 0);
    window_goto_right(&wm);
    ASSERT_EQ_INT(wm.active, idx);
    PASS();
}

static void test_window_swap_left_right(void) {
    TEST(window_swap_left_right);
    WindowManager wm;
    window_manager_init(&wm);
    int idx = window_split_vertical(&wm, 1);
    wm.active = idx;
    int active_x = wm.windows[idx].x;
    int neighbor_x = wm.windows[0].x;
    window_swap_left(&wm);
    ASSERT_EQ_INT(wm.active, idx);
    ASSERT_EQ_INT(wm.windows[idx].x, neighbor_x);
    ASSERT_EQ_INT(wm.windows[0].x, active_x);
    window_swap_right(&wm);
    ASSERT_EQ_INT(wm.active, idx);
    ASSERT_EQ_INT(wm.windows[idx].x, active_x);
    ASSERT_EQ_INT(wm.windows[0].x, neighbor_x);
    PASS();
}

static void test_window_swap_up_down(void) {
    TEST(window_swap_up_down);
    WindowManager wm;
    window_manager_init(&wm);
    int idx = window_split_horizontal(&wm, 1);
    wm.active = idx;
    int active_y = wm.windows[idx].y;
    int neighbor_y = wm.windows[0].y;
    window_swap_up(&wm);
    ASSERT_EQ_INT(wm.active, idx);
    ASSERT_EQ_INT(wm.windows[idx].y, neighbor_y);
    ASSERT_EQ_INT(wm.windows[0].y, active_y);
    window_swap_down(&wm);
    ASSERT_EQ_INT(wm.active, idx);
    ASSERT_EQ_INT(wm.windows[idx].y, active_y);
    ASSERT_EQ_INT(wm.windows[0].y, neighbor_y);
    PASS();
}

static void test_window_maximize(void) {
    TEST(window_maximize);
    WindowManager wm;
    window_manager_init(&wm);
    window_maximize(&wm);
    ASSERT_EQ_INT(wm.windows[0].x, 0);
    ASSERT_EQ_INT(wm.windows[0].y, 0);
    ASSERT_EQ_INT(wm.windows[0].width, 100);
    ASSERT_EQ_INT(wm.windows[0].height, 40);
    PASS();
}

static void test_window_equalize(void) {
    TEST(window_equalize);
    WindowManager wm;
    window_manager_init(&wm);
    window_split_vertical(&wm, 1);
    window_equalize(&wm);
    /* Both visible children should have equal width */
    ASSERT(wm.windows[0].width > 0);
    ASSERT(wm.windows[1].width > 0);
    PASS();
}

/* ================================================================
 * LANGUAGE SETTINGS TESTS
 * ================================================================ */

static void test_lang_settings_c(void) {
    TEST(lang_settings_c);
    const LanguageSettings *ls = language_settings_get("c");
    ASSERT(ls != NULL);
    ASSERT_EQ_INT(ls->tab_width, 8);
    ASSERT_TRUE(ls->use_tabs);
    ASSERT(ls->line_comment != NULL);
    ASSERT_EQ_STR(ls->line_comment, "//");
    ASSERT(ls->comment_open != NULL);
    ASSERT_EQ_STR(ls->comment_open, "/*");
    ASSERT(ls->comment_close != NULL);
    ASSERT_EQ_STR(ls->comment_close, "*/");
    PASS();
}

static void test_lang_settings_rust(void) {
    TEST(lang_settings_rust);
    const LanguageSettings *ls = language_settings_get("rust");
    ASSERT(ls != NULL);
    ASSERT_EQ_INT(ls->tab_width, 4);
    ASSERT_FALSE(ls->use_tabs);
    ASSERT(ls->line_comment != NULL);
    ASSERT_EQ_STR(ls->line_comment, "//");
    ASSERT_TRUE(ls->auto_format);
    PASS();
}

static void test_lang_settings_python(void) {
    TEST(lang_settings_python);
    const LanguageSettings *ls = language_settings_get("python");
    ASSERT(ls != NULL);
    ASSERT_EQ_INT(ls->tab_width, 4);
    ASSERT_FALSE(ls->use_tabs);
    ASSERT(ls->line_comment != NULL);
    ASSERT_EQ_STR(ls->line_comment, "#");
    ASSERT(ls->comment_open == NULL);
    PASS();
}

static void test_lang_settings_unknown(void) {
    TEST(lang_settings_unknown);
    const LanguageSettings *ls = language_settings_get("xyz_unknown");
    ASSERT(ls == NULL);
    PASS();
}

/* ================================================================
 * CONFIG AND THEME TESTS
 * ================================================================ */

static void test_config_default_theme_name(void) {
    TEST(config_default_theme_name);
    Config *cfg = config_default();
    ASSERT(cfg != NULL);
    ASSERT_EQ_STR(cfg->theme_name, "dragon");
    ASSERT_TRUE(cfg->theme.bg[0] < 0.10f);
    config_free(cfg);
    PASS();
}

static void test_theme_builtin_black_plus(void) {
    TEST(theme_builtin_black_plus);
    Theme t;
    ASSERT_TRUE(theme_get_named("black+", &t));
    ASSERT_TRUE(t.bg[0] == 0.0f);
    ASSERT_TRUE(t.bg[1] == 0.0f);
    ASSERT_TRUE(t.bg[2] == 0.0f);
    ASSERT_TRUE(t.fg[0] == 1.0f);
    ASSERT_TRUE(t.accent[0] == 1.0f);
    ASSERT_TRUE(t.accent[1] == 0.0f);
    PASS();
}

static void test_theme_apply_named_case_insensitive(void) {
    TEST(theme_apply_named_case_insensitive);
    ASSERT_TRUE(theme_apply_named("GLACIER"));
    ASSERT_EQ_STR(theme_current_name(), "glacier");
    ASSERT_TRUE(theme_get()->accent[2] > 0.80f);
    ASSERT_TRUE(theme_apply_named("dragon"));
    PASS();
}

static void test_lang_settings_detect(void) {
    TEST(lang_settings_detect);
    Document *doc = make_doc(NULL);
    doc->language_id = strdup("rust");
    LanguageSettings out;
    language_settings_detect(doc, &out);
    ASSERT_EQ_INT(out.tab_width, 4);
    ASSERT_FALSE(out.use_tabs);
    ASSERT_EQ_STR(out.line_comment, "//");
    free(doc->language_id);
    doc->language_id = strdup("unknown");
    language_settings_detect(doc, &out);
    ASSERT_EQ_INT(out.tab_width, 4);
    ASSERT_FALSE(out.use_tabs);
    free_doc(doc);
    PASS();
}

static void test_config_language_entries(void) {
    TEST(config_language_entries);
    char oldcwd[1024];
    ASSERT(getcwd(oldcwd, sizeof(oldcwd)) != NULL);
    char dir[] = "/tmp/dragon-config-lang-XXXXXX";
    ASSERT(mkdtemp(dir) != NULL);
    ASSERT(chdir(dir) == 0);
    FILE *f = fopen("dragon.toml", "w");
    ASSERT(f != NULL);
    fputs("[[language]]\n"
          "id = \"nix\"\n"
          "extensions = [\"nix\"]\n"
          "tab_width = 2\n"
          "line_comment = \"#\"\n"
          "tree_sitter = \"nix\"\n"
          "tree_sitter_path = \"plugins/nix/libtree-sitter-nix.so\"\n"
          "format_command = \"nixfmt {file}\"\n"
          "lsp_command = \"nil\"\n"
          "lsp_args = [\"--stdio\"]\n"
          "keywords = [\"let\", \"in\"]\n"
          "type_keywords = [\"path\"]\n"
          "macro_keywords = [\"builtins\"]\n", f);
    fclose(f);

    Config *cfg = config_load();
    int ok = cfg && cfg->language_count == 1 &&
             strcmp(cfg->languages[0].id, "nix") == 0 &&
             strcmp(cfg->languages[0].extensions[0], "nix") == 0 &&
             cfg->languages[0].tab_width == 2 &&
             strcmp(cfg->languages[0].line_comment, "#") == 0 &&
             strcmp(cfg->languages[0].tree_sitter, "nix") == 0 &&
             strcmp(cfg->languages[0].tree_sitter_path, "plugins/nix/libtree-sitter-nix.so") == 0 &&
             strcmp(cfg->languages[0].format_command, "nixfmt {file}") == 0 &&
             strcmp(cfg->languages[0].lsp_command, "nil") == 0 &&
             cfg->languages[0].lsp_arg_count == 1 &&
             cfg->languages[0].keyword_count == 2 &&
             strcmp(cfg->languages[0].keywords[0], "let") == 0 &&
             cfg->languages[0].type_keyword_count == 1 &&
             strcmp(cfg->languages[0].type_keywords[0], "path") == 0 &&
             cfg->languages[0].macro_keyword_count == 1 &&
             strcmp(cfg->languages[0].macro_keywords[0], "builtins") == 0;
    language_registry_load_config(cfg);
    const LanguageSettings *ls = language_settings_get("nix");
    ok = ok && strcmp(language_id_for_extension("nix"), "nix") == 0 &&
         strcmp(language_treesitter_for_extension(".nix"), "nix") == 0 &&
         strcmp(language_treesitter_path_for_extension("nix"), "plugins/nix/libtree-sitter-nix.so") == 0 &&
         strcmp(language_treesitter_path_for_name("nix"), "plugins/nix/libtree-sitter-nix.so") == 0 &&
         strcmp(language_format_command("nix"), "nixfmt {file}") == 0 &&
         ls && ls->tab_width == 2 && ls->line_comment && strcmp(ls->line_comment, "#") == 0;
    language_registry_load_config(NULL);
    config_free(cfg);
    ASSERT(chdir(oldcwd) == 0);
    ASSERT_TRUE(ok);
    PASS();
}

static void test_config_language_fallback_keywords(void) {
    TEST(config_language_fallback_keywords);
    Config *cfg = config_default();
    ASSERT(cfg != NULL);
    ConfigLanguage *lang = &cfg->languages[cfg->language_count++];
    memset(lang, 0, sizeof(*lang));
    snprintf(lang->id, sizeof(lang->id), "toy");
    snprintf(lang->extensions[0], sizeof(lang->extensions[0]), "toy");
    lang->extension_count = 1;
    snprintf(lang->keywords[0], sizeof(lang->keywords[0]), "entity");
    lang->keyword_count = 1;
    snprintf(lang->type_keywords[0], sizeof(lang->type_keywords[0]), "Scalar");
    lang->type_keyword_count = 1;
    snprintf(lang->macro_keywords[0], sizeof(lang->macro_keywords[0]), "derive");
    lang->macro_keyword_count = 1;
    language_registry_load_config(cfg);

    Document *doc = make_doc("entity Scalar derive call()\n");
    doc->filepath = strdup("sample.toy");
    document_detect_language(doc);
    syntax_free(&doc->syntax);
    syntax_init(&doc->syntax, doc->language_id);
    ASSERT_TRUE(document_update_syntax_fallback(doc));
    ASSERT_EQ_INT(syntax_get_type_at(&doc->syntax, 0, 1), SYNTAX_KEYWORD);
    ASSERT_EQ_INT(syntax_get_type_at(&doc->syntax, 0, 8), SYNTAX_TYPE);
    ASSERT_EQ_INT(syntax_get_type_at(&doc->syntax, 0, 15), SYNTAX_MACRO);
    ASSERT_EQ_INT(syntax_get_type_at(&doc->syntax, 0, 22), SYNTAX_FUNCTION);

    free_doc(doc);
    language_registry_load_config(NULL);
    config_free(cfg);
    PASS();
}

static void test_document_format_command_filter(void) {
    TEST(document_format_command_filter);
    Document *doc = make_doc("abc\n");
    ASSERT_TRUE(document_format_with_command(doc, "tr a-z A-Z"));
    ASSERT_EQ_STR(doc->buffer.text, "ABC\n");
    ASSERT_TRUE(doc->dirty);
    free_doc(doc);
    PASS();
}

static void test_document_format_command_file_placeholder(void) {
    TEST(document_format_command_file_placeholder);
    Document *doc = make_doc("abc\n");
    ASSERT_TRUE(document_format_with_command(doc, "sed -i 's/abc/xyz/' {file}"));
    ASSERT_EQ_STR(doc->buffer.text, "xyz\n");
    ASSERT_TRUE(doc->dirty);
    free_doc(doc);
    PASS();
}

static void test_config_plugin_manifest(void) {
    TEST(config_plugin_manifest);
    char oldcwd[1024];
    ASSERT(getcwd(oldcwd, sizeof(oldcwd)) != NULL);
    char dir[] = "/tmp/dragon-plugin-config-XXXXXX";
    ASSERT(mkdtemp(dir) != NULL);
    ASSERT(chdir(dir) == 0);
    ASSERT(mkdir("plugins", 0777) == 0);
    ASSERT(mkdir("plugins/gleam", 0777) == 0);
    ASSERT(mkdir("plugins/grain", 0777) == 0);
    FILE *f = fopen("dragon.toml", "w");
    ASSERT(f != NULL);
    fputs("[[plugin]]\n"
          "path = \"plugins/gleam\"\n"
          "enabled = true\n"
          "\n"
          "[[plugin]]\n"
          "path = \"plugins/zig-tools.toml\"\n"
          "enabled = true\n"
          "\n"
          "[[plugin]]\n"
          "path = \"plugins/grain\"\n"
          "enabled = false\n", f);
    fclose(f);
    f = fopen("plugins/gleam/dragon-plugin.toml", "w");
    ASSERT(f != NULL);
    fputs("[plugin]\n"
          "name = \"gleam-tools\"\n"
          "version = \"0.1\"\n"
          "description = \"Gleam language support\"\n"
          "\n"
          "[[language]]\n"
          "id = \"gleam\"\n"
          "extensions = [\"gleam\"]\n"
          "tab_width = 2\n"
          "line_comment = \"//\"\n"
          "tree_sitter = \"gleam\"\n"
          "lsp_command = \"gleam\"\n"
          "lsp_args = [\"lsp\"]\n", f);
    fclose(f);
    f = fopen("plugins/zig-tools.toml", "w");
    ASSERT(f != NULL);
    fputs("[plugin]\n"
          "name = \"zig-tools\"\n"
          "\n"
          "[[language]]\n"
          "id = \"zig\"\n"
          "extensions = [\"zig\"]\n"
          "tree_sitter = \"zig\"\n", f);
    fclose(f);
    f = fopen("plugins/grain/dragon-plugin.toml", "w");
    ASSERT(f != NULL);
    fputs("[plugin]\n"
          "name = \"grain-tools\"\n"
          "\n"
          "[[language]]\n"
          "id = \"grain\"\n"
          "extensions = [\"gr\"]\n"
          "tree_sitter = \"grain\"\n", f);
    fclose(f);

    Config *cfg = config_load();
    int ok = cfg && cfg->plugin_count == 3 && cfg->language_count == 3 &&
             cfg->plugins[0].enabled == 1 && cfg->plugins[0].loaded == 1 &&
             cfg->plugins[0].language_count == 1 &&
             strcmp(cfg->plugins[0].name, "gleam-tools") == 0 &&
             strcmp(cfg->languages[0].id, "gleam") == 0 &&
             strcmp(cfg->languages[0].tree_sitter_path, "plugins/gleam/libtree-sitter-gleam.so") == 0 &&
             strcmp(cfg->languages[0].lsp_command, "gleam") == 0 &&
             cfg->languages[0].source_plugin == 0 &&
             strcmp(cfg->plugins[1].name, "zig-tools") == 0 &&
             strcmp(cfg->languages[1].tree_sitter_path, "plugins/libtree-sitter-zig.so") == 0 &&
             cfg->plugins[2].enabled == 0 && cfg->plugins[2].loaded == 1 &&
             strcmp(cfg->languages[2].id, "grain") == 0;
    language_registry_load_config(cfg);
    ok = ok && language_id_for_extension("gr") == NULL;
    cfg->plugins[2].enabled = 1;
    language_registry_load_config(cfg);
    ok = ok && strcmp(language_id_for_extension("gr"), "grain") == 0;
    language_registry_load_config(NULL);
    config_free(cfg);
    ASSERT(chdir(oldcwd) == 0);
    ASSERT_TRUE(ok);
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
    test_buffer_append_empty();
    test_buffer_insert_middle();
    test_buffer_insert_at_start();
    test_buffer_insert_at_end();
    test_buffer_insert_empty();
    test_buffer_insert_zero_len();
    test_buffer_insert_past_end();
    test_buffer_delete_middle();
    test_buffer_delete_from_start();
    test_buffer_delete_clamps();
    test_buffer_delete_all();
    test_buffer_delete_zero_len();
    test_buffer_delete_from_empty();
    test_buffer_line_count_empty();
    test_buffer_line_count_single();
    test_buffer_line_count_multi();
    test_buffer_line_count_trailing_newline();
    test_buffer_line_count_newlines_only();
    test_buffer_line_len();
    test_buffer_line_len_empty_line();
    test_buffer_line_ptr();
    test_buffer_pos_from_row_col();
    test_buffer_pos_from_row_col_negative();
    test_buffer_row_col_from_pos();
    test_buffer_row_col_from_pos_past_end();
    test_buffer_roundtrip_row_col();
    test_buffer_load_save();
    test_buffer_load_nonexistent();
    test_buffer_insert_delete_sequence();
    test_buffer_large_insert();
    test_buffer_insert_triggers_realloc();

    printf("\n[Cursor Tests]\n");
    test_cursor_init();
    test_cursor_set();
    test_cursor_move_horizontal();
    test_cursor_move_vertical();
    test_cursor_move_clamp_negative();
    test_cursor_move_to();
    test_cursor_select_start();
    test_cursor_select_end_noop();
    test_cursor_clear_selection();
    test_cursor_has_selection();
    test_cursor_normalize_forward();
    test_cursor_normalize_backward();
    test_cursor_normalize_same_line();
    test_cursor_normalize_same_position();
    test_cursor_desired_col_vertical();
    test_cursor_move_zero();

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
    test_history_text_independence();
    test_history_multiple_undo_redo_cycles();

    printf("\n[Mode Tests]\n");
    test_mode_init();
    test_mode_set_get();
    test_mode_is();
    test_mode_select_initialized_flag();
    test_mode_suppress_next_char();
    test_mode_same_mode_no_suppress();
    test_mode_all_modes();
    test_mode_previous_tracking();
    test_mode_transition_clears_pending_state();

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
    test_syntax_multiline_tokens();
    test_syntax_all_types();
    test_syntax_boundary_positions();

    printf("\n[LSP Tests]\n");
    test_lsp_publish_diagnostics_parse();
    test_lsp_publish_diagnostics_empty();
    test_lsp_publish_diagnostics_clangd_shape();
    test_lsp_publish_diagnostics_quoted_key_text();
    test_lsp_completion_insert_text();
    test_lsp_formatting_response_parse();
    test_lsp_code_action_edit_parse();

    printf("\n[Config and Theme Tests]\n");
    test_config_default_theme_name();
    test_theme_builtin_black_plus();
    test_theme_apply_named_case_insensitive();

    printf("\n[Document Tests]\n");
    test_document_init();
    test_document_mark_dirty();
    test_document_insert_char();
    test_document_insert_char_multiline();
    test_document_delete_char();
    test_document_delete_char_at_start();
    test_document_delete_char_across_lines();
    test_document_newline();
    test_document_newline_auto_indent();
    test_document_newline_no_indent();
    test_document_insert_text();
    test_document_insert_text_multiline();
    test_document_move_cursor();
    test_document_move_cursor_clamp();
    test_document_move_cursor_negative();
    test_document_move_cursor_line_wrap();
    test_document_cursor_to();
    test_document_cursor_to_clamp();
    test_document_cursor_to_negative();
    test_document_cursor_home_end();
    test_document_cursor_doc_start_end();
    test_document_undo_redo_insert();
    test_document_undo_redo_delete();
    test_document_undo_redo_newline();
    test_document_yank_paste();
    test_document_yank_line();
    test_document_delete_line_at();
    test_document_delete_line_at_first();
    test_document_select_word();
    test_document_select_line();
    test_document_select_all();
    test_document_delete_selection();
    test_document_replace_char();
    test_document_delete_char_at_cursor();
    test_document_indent_dedent_line();
    test_document_join_lines();
    test_document_join_lines_strips_indent();
    test_document_open_line_below();
    test_document_open_line_above();
    test_document_cursor_first_non_blank();
    test_document_cursor_word_forward();
    test_document_cursor_word_backward();
    test_document_toggle_case();
    test_document_toggle_case_selection();
    test_document_lowercase();
    test_document_uppercase();
    test_document_find_char_forward();
    test_document_find_char_backward();
    test_document_till_char_forward();
    test_document_till_char_backward();
    test_document_goto_line_start_end();
    test_document_extend_to_line_bounds();
    test_document_scroll_up_down();
    test_document_scroll_up_clamp();
    test_document_sync_viewport();
    test_document_sync_viewport_horizontal_right();
    test_document_sync_viewport_horizontal_left();
    test_document_move_cursor_updates_scroll_x();
    test_document_set_search();
    test_document_search_next();
    test_document_search_prev();
    test_document_search_wraps();
    test_document_add_cursor();
    test_document_add_cursor_repeated();
    test_document_remove_last_cursor();
    test_document_clear_cursors();
    test_document_collapse_selection();
    test_document_flip_cursor_anchor();
    test_document_surround();
    test_document_surround_parens();
    test_document_delete_surround();
    test_document_move_line_up_down();
    test_document_move_line_up_first_line();
    test_document_move_line_down_last_line();
    test_document_change_selection();
    test_document_substitute_char();
    test_document_indent_selection();
    test_document_dedent_selection();
    test_document_paste_before();
    test_document_cursor_page_up_down();
    test_document_half_page();
    test_document_jumplist();
    test_document_scroll_center();
    test_document_empty_buffer_ops();
    test_document_new_empty();
    test_document_trim_whitespace();
    test_document_select_regex();
    test_document_sort_selection();

    printf("\n[Additional Document Tests - Word/WORD Motion]\n");
    test_document_cursor_word_end();
    test_document_cursor_WORD_forward();
    test_document_cursor_WORD_backward();
    test_document_cursor_WORD_end();
    test_document_increment_number();
    test_document_decrement_number();
    test_document_increment_negative_number();
    test_document_decrement_negative_number();

    printf("\n[Additional Document Tests - Multi-cursor]\n");
    test_document_insert_char_multi();
    test_document_insert_char_multi_undo_redo();
    test_document_move_cursor_multi();
    test_document_delete_selection_multi();
    test_document_add_cursor_below_above();
    test_document_delete_char_multi();
    test_document_delete_char_multi_undo_redo();
    test_document_newline_multi();
    test_document_newline_multi_undo_redo();

    printf("\n[Additional Document Tests - Selection ops]\n");
    test_document_replace_selection_char();
    test_document_replace_selection_yanked();
    test_document_keep_primary_selection();
    test_document_copy_selection_below();
    test_document_join_lines_selection();
    test_document_force_selection_forward();

    printf("\n[Additional Document Tests - Scroll/Viewport]\n");
    test_document_cursor_page_down();
    test_document_scroll_horizontal_center();
    test_document_scroll_top();
    test_document_scroll_bottom();
    test_document_shrink_to_line_bounds();
    test_document_remove_primary_selection();

    printf("\n[Additional Document Tests - View navigation]\n");
    test_document_goto_view_top();
    test_document_goto_view_center();
    test_document_goto_view_bottom();

    printf("\n[Additional Document Tests - Comment, Join, Delete word]\n");
    test_document_comment_toggle();
    test_document_comment_toggle_indented();
    test_document_join_lines_with_space();
    test_document_join_lines_with_space_selection();
    test_document_delete_word_forward();
    test_document_delete_word_forward_mid();

    printf("\n[Additional Document Tests - Page/Half-page extend]\n");
    test_document_page_down_extend();
    test_document_page_up_extend();
    test_document_half_page_down_extend();
    test_document_half_page_up_extend();

    printf("\n[Additional Document Tests - Split/Merge selections]\n");
    test_document_split_selection_newlines();
    test_document_merge_selections();
    test_document_merge_consecutive_selections();

    printf("\n[Additional Document Tests - Align, Jumplist forward]\n");
    test_document_align_selections();
    test_document_jumplist_forward();
    test_document_jumplist_forward_at_end();

    printf("\n[Additional Document Tests - Surround variants]\n");
    test_document_surround_curly();
    test_document_surround_bracket();
    test_document_surround_angle();
    test_document_delete_surround_curly();
    test_document_replace_surround();

    printf("\n[Additional Document Tests - View scroll page]\n");
    test_document_view_page_down();
    test_document_view_page_up();
    test_document_view_half_page_down();
    test_document_view_half_page_up();

    printf("\n[Additional Document Tests - Match bracket, Goto last mod]\n");
    test_document_match_bracket_forward();
    test_document_match_bracket_backward();
    test_document_match_bracket_nested();
    test_document_match_bracket_square();
    test_document_match_bracket_curly();
    test_document_goto_last_modification();

    printf("\n[Additional Document Tests - Select literal, All matches]\n");
    test_document_select_literal();
    test_document_select_all_matches();
    test_document_split_literal();
    test_document_split_all_matches();
    test_document_keep_matching();

    printf("\n[Additional Document Tests - File I/O]\n");
    test_document_open_save();
    test_document_save_as();
    test_document_insert_file();
    test_document_detect_language();

    printf("\n[Text Object Tests]\n");
    test_text_obj_select_inside_word();
    test_text_obj_select_around_word();
    test_text_obj_select_inside_word_mid();
    test_text_obj_select_inside_paren();
    test_text_obj_select_around_paren();
    test_text_obj_select_inside_bracket();
    test_text_obj_select_around_bracket();
    test_text_obj_select_inside_curly();
    test_text_obj_select_around_curly();
    test_text_obj_select_inside_angle();
    test_text_obj_select_around_angle();
    test_text_obj_select_inside_quote();
    test_text_obj_select_around_quote();
    test_text_obj_select_inside_backtick();
    test_text_obj_select_around_backtick();
    test_text_obj_select_inside_paragraph();
    test_text_obj_select_around_paragraph();

    printf("\n[Macro Tests]\n");
    test_macro_init();
    test_macro_record_and_stop();
    test_macro_record_keys();
    test_macro_replay();
    test_macro_cannot_record_while_recording();
    test_macro_replay_empty_is_false();
    test_macro_stop_noop_when_not_recording();

    printf("\n[Reflow Tests]\n");
    test_reflow_simple();
    test_reflow_already_fits();
    test_reflow_preserves_newlines();

    printf("\n[Indent Style Tests]\n");
    test_indent_tabs_to_spaces();
    test_indent_spaces_to_tabs();
    test_indent_tabs_to_spaces_mixed();

    printf("\n[Alternate File Tests]\n");
    test_alternate_set_get();
    test_alternate_null();
    test_alternate_overwrite();

    printf("\n[Block Comment Tests]\n");
    test_comment_block_add();
    test_comment_block_remove();
    test_comment_block_toggle();
    test_comment_line_add();
    test_comment_line_remove();
    test_comment_line_toggle();

    printf("\n[Window Manager Tests]\n");
    test_window_init();
    test_window_split_vertical();
    test_window_split_horizontal();
    test_window_close();
    test_window_next_prev();
    test_window_goto_left_right();
    test_window_swap_left_right();
    test_window_swap_up_down();
    test_window_maximize();
    test_window_equalize();

    printf("\n[Language Settings Tests]\n");
    test_lang_settings_c();
    test_lang_settings_rust();
    test_lang_settings_python();
    test_lang_settings_unknown();
    test_lang_settings_detect();
    test_config_language_entries();
    test_config_language_fallback_keywords();
    test_document_format_command_filter();
    test_document_format_command_file_placeholder();
    test_config_plugin_manifest();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
