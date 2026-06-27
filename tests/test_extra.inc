
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
    ASSERT_EQ_INT(doc->cursors[0].col, 11);
    document_cursor_WORD_backward(doc);
    ASSERT_EQ_INT(doc->cursors[0].col, 5);
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
    ASSERT_EQ_INT(doc->cursors[0].col, 10);
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
    ASSERT_EQ_STR(doc->buffer.text, "Xaaa Xbb Xccc");
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

/* ================================================================
 * ADDITIONAL DOCUMENT TESTS - Selection ops
 * ================================================================ */

static void test_document_replace_selection_char(void) {
    TEST(document_replace_selection_char);
    Document *doc = make_doc("hello world");
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_replace_selection_char(doc, 'X');
    ASSERT_EQ_STR(doc->buffer.text, "X world");
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
    cursor_select_start(&doc->cursors[0]);
    doc->cursors[0].col = 5;
    document_keep_primary_selection(doc);
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
    ASSERT_EQ_STR(doc->buffer.text, "hello ld");
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
    ASSERT_EQ_INT(doc->cursors[0].col, 3);
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
    int len0 = (int)buffer_line_len(&doc->buffer, 0);
    int len1 = (int)buffer_line_len(&doc->buffer, 1);
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
    ASSERT_EQ_INT(doc->cursors[0].col, 5);
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
    ASSERT_EQ_STR(doc->buffer.text, "[hello]");
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
