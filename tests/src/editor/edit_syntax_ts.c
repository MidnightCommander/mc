/*
   src/editor - tests for tree-sitter syntax highlighting integration

   Copyright (C) 2026
   Free Software Foundation, Inc.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/src/editor"

#include "tests/mctest.h"

#include <string.h>
#include <tree_sitter/api.h>

#ifdef TREE_SITTER_STATIC
#include "src/editor/ts-grammars/ts-grammar-registry.h"
#else
#include "src/editor/ts-grammar-loader.h"
#endif

/* --------------------------------------------------------------------------------------------- */

/* Path to the query files directory (set via -DTEST_TS_QUERIES_DIR) */
#ifndef TEST_TS_QUERIES_DIR
#error "TEST_TS_QUERIES_DIR must be defined"
#endif

/* In shared mode, HAVE_GRAMMAR_* macros are not defined.
   Define them all to 1 so tests compile unconditionally — runtime lookup
   via ts_grammar_registry_lookup() handles missing grammars gracefully. */
#ifdef TREE_SITTER_SHARED
#define HAVE_GRAMMAR_C 1
#define HAVE_GRAMMAR_PYTHON 1
#define HAVE_GRAMMAR_BASH 1
#define HAVE_GRAMMAR_MARKDOWN 1
#define HAVE_GRAMMAR_MARKDOWN_INLINE 1
#define HAVE_GRAMMAR_HTML 1
#define HAVE_GRAMMAR_JAVASCRIPT 1
#define HAVE_GRAMMAR_CSS 1
#endif

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
}

/* --------------------------------------------------------------------------------------------- */
/* Test 1: Grammar registry lookup returns non-NULL for known grammars */

static const struct test_registry_lookup_found_ds
{
    const char *grammar_name;
} test_registry_lookup_found_ds[] = {
    { "c" },
    { "python" },
    { "bash" },
};

/* @Test(dataSource = "test_registry_lookup_found_ds") */
START_PARAMETRIZED_TEST (test_registry_lookup_found, test_registry_lookup_found_ds)
{
    // when
    const TSLanguage *lang = ts_grammar_registry_lookup (data->grammar_name);

    // then
    ck_assert_msg (lang != NULL, "Grammar '%s' should be found in registry", data->grammar_name);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test 2: Grammar registry lookup returns NULL for unknown grammars */

START_TEST (test_registry_lookup_not_found)
{
    // when
    const TSLanguage *lang = ts_grammar_registry_lookup ("nonexistent_language_xyz");

    // then
    mctest_assert_null (lang);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test 3: Every available grammar has a valid query file that compiles without errors.
   This is the most important test -- it catches the silent failure mode where an invalid
   node name in a .scm file causes ts_query_new() to reject the entire query.

   In static mode: iterates the compile-time registry.
   In shared mode: scans the query directory for *-highlights.scm files. */

/**
 * Try to compile a query for a grammar.  Returns: 0=success, 1=failure, -1=skip.
 */
static int
test_one_query (const char *name, const TSLanguage *lang)
{
    char path[1024];
    char *src = NULL;
    gsize len = 0;
    uint32_t eo = 0;
    TSQueryError et = TSQueryErrorNone;
    TSQuery *q;

    if (lang == NULL || ts_language_version (lang) < TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION)
        return -1;

    snprintf (path, sizeof (path), "%s/%s-highlights.scm", TEST_TS_QUERIES_DIR, name);
    if (!g_file_get_contents (path, &src, &len, NULL))
        return 1;

    q = ts_query_new (lang, src, (uint32_t) len, &eo, &et);
    g_free (src);

    if (q == NULL)
        return 1;

    ts_query_delete (q);
    return 0;
}

START_TEST (test_all_query_files_compile)
{
    int tested = 0;
    int failed = 0;
    char first_fail[128] = "";

#ifdef TREE_SITTER_STATIC
    {
        const ts_grammar_entry_static_t *entry;

        for (entry = ts_grammar_registry; entry->name != NULL; entry++)
        {
            int rc = test_one_query (entry->name, entry->func ());

            if (rc == -1)
                continue;
            tested++;
            if (rc == 1 && ++failed == 1)
                snprintf (first_fail, sizeof (first_fail), "%s", entry->name);
        }
    }
#else
    {
        GDir *dir = g_dir_open (TEST_TS_QUERIES_DIR, 0, NULL);

        ck_assert_msg (dir != NULL, "Cannot open query dir");

        const gchar *fname;

        while ((fname = g_dir_read_name (dir)) != NULL)
        {
            if (g_str_has_suffix (fname, "-highlights.scm"))
            {
                gchar *name = g_strndup (fname, strlen (fname) - strlen ("-highlights.scm"));
                const TSLanguage *lang = ts_grammar_registry_lookup (name);
                int rc = test_one_query (name, lang);

                if (rc >= 0)
                {
                    tested++;
                    if (rc == 1 && ++failed == 1)
                        snprintf (first_fail, sizeof (first_fail), "%s", name);
                }
                g_free (name);
            }
        }
        g_dir_close (dir);
    }
#endif

    ck_assert_msg (tested > 0, "No grammars found");
    ck_assert_msg (failed == 0, "Query failed for %d/%d (first: %s)", failed, tested, first_fail);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test 4: Verify parser creation and basic parse works for a few grammars */

#ifdef HAVE_GRAMMAR_C
START_TEST (test_parser_basic_parse)
{
    const char *test_code = "int main(void) { return 0; }\n";
    const TSLanguage *lang;
    TSParser *parser;
    TSTree *tree;

    lang = ts_grammar_registry_lookup ("c");
    ck_assert_msg (lang != NULL, "C grammar must exist");

    parser = ts_parser_new ();
    ck_assert_msg (parser != NULL, "ts_parser_new() must succeed");

    ck_assert_msg (ts_parser_set_language (parser, lang), "ts_parser_set_language() must succeed");

    tree = ts_parser_parse_string (parser, NULL, test_code, (uint32_t) strlen (test_code));
    ck_assert_msg (tree != NULL, "ts_parser_parse_string() must return a tree");

    // Root node should not be error
    TSNode root = ts_tree_root_node (tree);
    ck_assert_msg (!ts_node_is_null (root), "Root node must not be null");
    ck_assert_msg (ts_node_child_count (root) > 0, "Root node must have children");

    ts_tree_delete (tree);
    ts_parser_delete (parser);
}
END_TEST
#endif

/* --------------------------------------------------------------------------------------------- */
/* Test 5: Query cursor produces captures for C code */

#ifdef HAVE_GRAMMAR_C
START_TEST (test_query_captures_c)
{
    const char *test_code = "// comment\nint main(void) { return 0; }\n";
    const TSLanguage *lang;
    TSParser *parser;
    TSTree *tree;
    char query_path[1024];
    char *query_src = NULL;
    gsize query_len = 0;
    uint32_t error_offset = 0;
    TSQueryError error_type = TSQueryErrorNone;
    TSQuery *query;
    TSQueryCursor *cursor;
    TSQueryMatch match;
    int capture_count = 0;

    lang = ts_grammar_registry_lookup ("c");
    ck_assert_msg (lang != NULL, "C grammar must exist");

    // Parse test code
    parser = ts_parser_new ();
    ts_parser_set_language (parser, lang);
    tree = ts_parser_parse_string (parser, NULL, test_code, (uint32_t) strlen (test_code));
    ck_assert_msg (tree != NULL, "Parse must succeed");

    // Load and compile query
    snprintf (query_path, sizeof (query_path), "%s/c-highlights.scm", TEST_TS_QUERIES_DIR);
    ck_assert_msg (g_file_get_contents (query_path, &query_src, &query_len, NULL),
                   "c-highlights.scm must be readable");

    query = ts_query_new (lang, query_src, (uint32_t) query_len, &error_offset, &error_type);
    ck_assert_msg (query != NULL, "C query must compile (error type %d at offset %u)",
                   (int) error_type, error_offset);

    // Run query cursor
    cursor = ts_query_cursor_new ();
    ts_query_cursor_exec (cursor, query, ts_tree_root_node (tree));

    while (ts_query_cursor_next_match (cursor, &match))
    {
        for (uint16_t i = 0; i < match.capture_count; i++)
        {
            capture_count++;
        }
    }

    // We expect at least some captures (comment, keyword "int", "return", number "0", etc.)
    ck_assert_msg (capture_count >= 3,
                   "Expected at least 3 captures for C test code, got %d", capture_count);

    ts_query_cursor_delete (cursor);
    ts_query_delete (query);
    g_free (query_src);
    ts_tree_delete (tree);
    ts_parser_delete (parser);
}
END_TEST
#endif

/* --------------------------------------------------------------------------------------------- */
/* Test 6: Markdown inline injection - parse with included ranges and get captures */

#if defined(HAVE_GRAMMAR_MARKDOWN) && defined(HAVE_GRAMMAR_MARKDOWN_INLINE)
START_TEST (test_markdown_inline_injection)
{
    const char *test_md = "# Hello\n\nThis is **bold** and `code` text.\n";
    const TSLanguage *block_lang;
    const TSLanguage *inline_lang;
    TSParser *block_parser;
    TSParser *inline_parser;
    TSTree *block_tree;
    TSTree *inline_tree;
    TSNode root;
    uint32_t child_count, i;
    GArray *ranges;
    char query_path[1024];
    char *query_src = NULL;
    gsize query_len = 0;
    uint32_t error_offset = 0;
    TSQueryError error_type = TSQueryErrorNone;
    TSQuery *query;
    TSQueryCursor *cursor;
    TSQueryMatch match;
    int capture_count = 0;

    block_lang = ts_grammar_registry_lookup ("markdown");
    ck_assert_msg (block_lang != NULL, "markdown grammar must exist");

    inline_lang = ts_grammar_registry_lookup ("markdown_inline");
    ck_assert_msg (inline_lang != NULL, "markdown_inline grammar must exist");

    // Parse with block parser
    block_parser = ts_parser_new ();
    ts_parser_set_language (block_parser, block_lang);
    block_tree = ts_parser_parse_string (block_parser, NULL, test_md, (uint32_t) strlen (test_md));
    ck_assert_msg (block_tree != NULL, "Block parse must succeed");

    // Recursively collect "inline" node ranges from the block tree
    ranges = g_array_new (FALSE, FALSE, sizeof (TSRange));
    root = ts_tree_root_node (block_tree);
    {
        // Use a simple stack-based DFS to find all "inline" nodes
        GArray *stack = g_array_new (FALSE, FALSE, sizeof (TSNode));
        g_array_append_val (stack, root);

        while (stack->len > 0)
        {
            TSNode node = g_array_index (stack, TSNode, stack->len - 1);
            const char *type;

            g_array_set_size (stack, stack->len - 1);
            type = ts_node_type (node);

            if (strcmp (type, "inline") == 0)
            {
                TSRange r;

                r.start_point = ts_node_start_point (node);
                r.end_point = ts_node_end_point (node);
                r.start_byte = ts_node_start_byte (node);
                r.end_byte = ts_node_end_byte (node);
                g_array_append_val (ranges, r);
            }
            else
            {
                child_count = ts_node_child_count (node);
                for (i = 0; i < child_count; i++)
                {
                    TSNode child = ts_node_child (node, i);
                    g_array_append_val (stack, child);
                }
            }
        }

        g_array_free (stack, TRUE);
    }

    ck_assert_msg (ranges->len > 0, "Should find at least one 'inline' node in markdown block tree");

    // Set up inline parser with included ranges
    inline_parser = ts_parser_new ();
    ts_parser_set_language (inline_parser, inline_lang);
    ts_parser_set_included_ranges (inline_parser, &g_array_index (ranges, TSRange, 0), ranges->len);

    inline_tree = ts_parser_parse_string (inline_parser, NULL, test_md, (uint32_t) strlen (test_md));
    ck_assert_msg (inline_tree != NULL, "Inline parse must succeed");

    // Load and compile inline query
    snprintf (query_path, sizeof (query_path), "%s/markdown_inline-highlights.scm",
              TEST_TS_QUERIES_DIR);
    ck_assert_msg (g_file_get_contents (query_path, &query_src, &query_len, NULL),
                   "markdown_inline-highlights.scm must be readable");

    query = ts_query_new (inline_lang, query_src, (uint32_t) query_len, &error_offset, &error_type);
    ck_assert_msg (query != NULL,
                   "markdown_inline query must compile (error type %d at offset %u)",
                   (int) error_type, error_offset);

    // Run query cursor
    cursor = ts_query_cursor_new ();
    ts_query_cursor_exec (cursor, query, ts_tree_root_node (inline_tree));

    while (ts_query_cursor_next_match (cursor, &match))
    {
        for (uint16_t mi = 0; mi < match.capture_count; mi++)
        {
            capture_count++;
        }
    }

    // We expect at least 2 captures: **bold** (strong_emphasis) and `code` (code_span)
    ck_assert_msg (capture_count >= 2,
                   "Expected at least 2 captures for markdown inline test, got %d", capture_count);

    ts_query_cursor_delete (cursor);
    ts_query_delete (query);
    g_free (query_src);
    g_array_free (ranges, TRUE);
    ts_tree_delete (inline_tree);
    ts_tree_delete (block_tree);
    ts_parser_delete (inline_parser);
    ts_parser_delete (block_parser);
}
END_TEST
#endif

/* --------------------------------------------------------------------------------------------- */
/* Test 7: HTML multi-injection - JS in <script> and CSS in <style> */

#if defined(HAVE_GRAMMAR_HTML) && defined(HAVE_GRAMMAR_JAVASCRIPT) && defined(HAVE_GRAMMAR_CSS)
START_TEST (test_html_multi_injection)
{
    const char *test_html =
        "<html><head>\n"
        "<style>body { color: red; }</style>\n"
        "<script>var x = 1;</script>\n"
        "</head></html>\n";
    const TSLanguage *html_lang;
    TSParser *html_parser;
    TSTree *html_tree;
    TSNode root;
    uint32_t child_count, i;

    html_lang = ts_grammar_registry_lookup ("html");
    ck_assert_msg (html_lang != NULL, "html grammar must exist");

    html_parser = ts_parser_new ();
    ts_parser_set_language (html_parser, html_lang);
    html_tree =
        ts_parser_parse_string (html_parser, NULL, test_html, (uint32_t) strlen (test_html));
    ck_assert_msg (html_tree != NULL, "HTML parse must succeed");

    // Verify that script_element and style_element nodes exist in the tree
    {
        gboolean found_script = FALSE;
        gboolean found_style = FALSE;
        GArray *stack = g_array_new (FALSE, FALSE, sizeof (TSNode));

        root = ts_tree_root_node (html_tree);
        g_array_append_val (stack, root);

        while (stack->len > 0)
        {
            TSNode node = g_array_index (stack, TSNode, stack->len - 1);
            const char *type;

            g_array_set_size (stack, stack->len - 1);
            type = ts_node_type (node);

            if (strcmp (type, "script_element") == 0)
                found_script = TRUE;
            if (strcmp (type, "style_element") == 0)
                found_style = TRUE;

            child_count = ts_node_child_count (node);
            for (i = 0; i < child_count; i++)
            {
                TSNode child = ts_node_child (node, i);
                g_array_append_val (stack, child);
            }
        }

        g_array_free (stack, TRUE);

        ck_assert_msg (found_script, "HTML tree must contain script_element");
        ck_assert_msg (found_style, "HTML tree must contain style_element");
    }

    // Parse the raw_text inside script_element with the JavaScript grammar
    {
        const TSLanguage *js_lang;
        TSParser *js_parser;
        char query_path[1024];
        char *query_src = NULL;
        gsize query_len = 0;
        uint32_t error_offset = 0;
        TSQueryError error_type = TSQueryErrorNone;
        TSQuery *query;

        js_lang = ts_grammar_registry_lookup ("javascript");
        ck_assert_msg (js_lang != NULL, "javascript grammar must exist");

        // Find raw_text inside script_element
        {
            GArray *stack = g_array_new (FALSE, FALSE, sizeof (TSNode));

            root = ts_tree_root_node (html_tree);
            g_array_append_val (stack, root);

            while (stack->len > 0)
            {
                TSNode node = g_array_index (stack, TSNode, stack->len - 1);
                const char *type;

                g_array_set_size (stack, stack->len - 1);
                type = ts_node_type (node);

                if (strcmp (type, "script_element") == 0)
                {
                    // Find raw_text child and parse it with JS
                    uint32_t cc = ts_node_child_count (node);
                    uint32_t ci;

                    for (ci = 0; ci < cc; ci++)
                    {
                        TSNode child = ts_node_child (node, ci);

                        if (strcmp (ts_node_type (child), "raw_text") == 0)
                        {
                            TSRange r;
                            TSTree *js_tree;
                            TSQueryCursor *cursor;
                            TSQueryMatch match;
                            int js_captures = 0;

                            r.start_point = ts_node_start_point (child);
                            r.end_point = ts_node_end_point (child);
                            r.start_byte = ts_node_start_byte (child);
                            r.end_byte = ts_node_end_byte (child);

                            js_parser = ts_parser_new ();
                            ts_parser_set_language (js_parser, js_lang);
                            ts_parser_set_included_ranges (js_parser, &r, 1);

                            js_tree = ts_parser_parse_string (js_parser, NULL, test_html,
                                                              (uint32_t) strlen (test_html));
                            ck_assert_msg (js_tree != NULL, "JS injection parse must succeed");

                            snprintf (query_path, sizeof (query_path),
                                      "%s/javascript-highlights.scm", TEST_TS_QUERIES_DIR);
                            ck_assert_msg (
                                g_file_get_contents (query_path, &query_src, &query_len, NULL),
                                "javascript-highlights.scm must be readable");

                            query = ts_query_new (js_lang, query_src, (uint32_t) query_len,
                                                  &error_offset, &error_type);
                            ck_assert_msg (query != NULL, "JS query must compile");

                            cursor = ts_query_cursor_new ();
                            ts_query_cursor_exec (cursor, query, ts_tree_root_node (js_tree));

                            while (ts_query_cursor_next_match (cursor, &match))
                            {
                                uint16_t mi;

                                for (mi = 0; mi < match.capture_count; mi++)
                                    js_captures++;
                            }

                            ck_assert_msg (js_captures >= 1,
                                           "Expected JS captures from script content, got %d",
                                           js_captures);

                            ts_query_cursor_delete (cursor);
                            ts_query_delete (query);
                            g_free (query_src);
                            ts_tree_delete (js_tree);
                            ts_parser_delete (js_parser);
                        }
                    }
                    break;
                }

                child_count = ts_node_child_count (node);
                for (i = 0; i < child_count; i++)
                {
                    TSNode child = ts_node_child (node, i);
                    g_array_append_val (stack, child);
                }
            }

            g_array_free (stack, TRUE);
        }
    }

    ts_tree_delete (html_tree);
    ts_parser_delete (html_parser);
}
END_TEST
#endif

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    if (sizeof (test_registry_lookup_found_ds) > 0)
        mctest_add_parameterized_test (tc_core, test_registry_lookup_found,
                                       test_registry_lookup_found_ds);
    tcase_add_test (tc_core, test_registry_lookup_not_found);
    tcase_add_test (tc_core, test_all_query_files_compile);
#ifdef HAVE_GRAMMAR_C
    tcase_add_test (tc_core, test_parser_basic_parse);
    tcase_add_test (tc_core, test_query_captures_c);
#endif
#if defined(HAVE_GRAMMAR_MARKDOWN) && defined(HAVE_GRAMMAR_MARKDOWN_INLINE)
    tcase_add_test (tc_core, test_markdown_inline_injection);
#endif
#if defined(HAVE_GRAMMAR_HTML) && defined(HAVE_GRAMMAR_JAVASCRIPT) && defined(HAVE_GRAMMAR_CSS)
    tcase_add_test (tc_core, test_html_multi_injection);
#endif
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
