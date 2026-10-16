/*
   src/editor - tests for Sublime color scheme support in syntax highlighting

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

#include "src/editor/syntax_scheme.h"

/* lenient JSON: comments, trailing commas, variables */
static const char test_scheme[] =
    "// Sublime-style scheme\n"
    "{\n"
    "  \"name\": \"Test\",\n"
    "  \"variables\": { \"red\": \"#f92672\", \"alias\": \"var(red)\", },\n"
    "  \"globals\": { \"foreground\": \"#f8f8f2\", \"background\": \"#272822\" },\n"
    "  /* rules */\n"
    "  \"rules\": [\n"
    "    { \"scope\": \"comment\", \"foreground\": \"#75715e\", \"font_style\": \"italic\" },\n"
    "    { \"scope\": \"keyword, storage\", \"foreground\": \"var(alias)\" },\n"
    "    { \"scope\": \"keyword.control\", \"foreground\": \"#abc\", \"font_style\": \"bold\" },\n"
    "    { \"scope\": \"string\", \"foreground\": \"rgb(230, 219, 116)\" },\n"
    "    { \"scope\": \"string constant.character.escape\", \"foreground\": \"hsl(0, 100%, 50%)\" "
    "},\n"
    "    { \"scope\": \"constant - string\", \"foreground\": \"lime\" },\n"
    "    { \"scope\": \"invalid\", \"background\": \"color(var(red) alpha(0.5))\" },\n"
    "  ],\n"
    "}\n";

static syntax_scheme_t *scheme;

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    scheme = syntax_scheme_parse (test_scheme, NULL);
}

/* @After */
static void
teardown (void)
{
    syntax_scheme_free (scheme);
}

/* --------------------------------------------------------------------------------------------- */

static const struct test_resolve_ds
{
    const char *ctx_scope;
    const char *scope;
    syntax_scheme_depth_t depth;
    const char *fg;
    const char *bg;
    const char *attrs;
} test_resolve_ds[] = {
    // 0. variable chain
    { NULL, "storage.type", SYNTAX_SCHEME_DEPTH_TRUECOLOR, "#f92672", "", "" },
    // 1. longest prefix wins, #rgb
    { NULL, "keyword.control.flow", SYNTAX_SCHEME_DEPTH_TRUECOLOR, "#aabbcc", "", "bold" },
    // 2. shorter prefix
    { NULL, "keyword.operator", SYNTAX_SCHEME_DEPTH_TRUECOLOR, "#f92672", "", "" },
    // 3. unmatched scope gets globals foreground, never globals background
    { NULL, "variable.other", SYNTAX_SCHEME_DEPTH_TRUECOLOR, "#f8f8f2", "", "" },
    // 4. rgb()
    { NULL, "string.quoted.double", SYNTAX_SCHEME_DEPTH_TRUECOLOR, "#e6db74", "", "" },
    // 5. descendant selector, hsl()
    { "string.quoted.double", "constant.character.escape", SYNTAX_SCHEME_DEPTH_TRUECOLOR, "#ff0000",
      "", "" },
    // 6. exclusion: "constant - string" does not apply inside strings
    { "string.quoted.double", "constant.other.placeholder", SYNTAX_SCHEME_DEPTH_TRUECOLOR,
      "#e6db74", "", "" },
    // 7. ...but applies elsewhere
    { NULL, "constant.numeric", SYNTAX_SCHEME_DEPTH_TRUECOLOR, "#00ff00", "", "" },
    // 8. font style
    { NULL, "comment.line", SYNTAX_SCHEME_DEPTH_TRUECOLOR, "#75715e", "", "italic" },
    // 9. alpha is blended over globals background
    { NULL, "invalid.illegal", SYNTAX_SCHEME_DEPTH_TRUECOLOR, "#f8f8f2", "#90274a", "" },
    // 10. 256 colors
    { NULL, "keyword.control", SYNTAX_SCHEME_DEPTH_256, "color146", "", "bold" },
    // 11. 16 colors
    { NULL, "constant.numeric", SYNTAX_SCHEME_DEPTH_16, "brightgreen", "", "" },
};

/* @Test(dataSource = "test_resolve_ds") */
START_PARAMETRIZED_TEST (test_resolve, test_resolve_ds)
{
    syntax_scheme_style_t style;

    // given
    mctest_assert_not_null (scheme);

    // when
    syntax_scheme_resolve (scheme, data->ctx_scope, data->scope, data->depth, &style);

    // then
    mctest_assert_str_eq (style.fg, data->fg);
    mctest_assert_str_eq (style.bg, data->bg);
    mctest_assert_str_eq (style.attrs, data->attrs);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

static const struct test_builtin_ds
{
    const char *ctx_scope;
    const char *scope;
    const char *fg;
} test_builtin_ds[] = {
    { NULL, "keyword.control", "yellow" },
    { NULL, "storage.type.primitive", "yellow" },
    { NULL, "constant.language", "yellow" },
    { NULL, "support.function", "brightred" },
    { NULL, "comment.block", "brown" },
    { "comment.block", "keyword.other.todo", "brightred" },
    { NULL, "markup.inserted.diff", "brightgreen" },
    { NULL, "markup.deleted", "brightred" },
    { NULL, "markup.underline.link", "yellow" },
    { NULL, "meta.diff.header.from-file", "brightmagenta" },
    { NULL, "punctuation.terminator", "brightmagenta" },
    { "string.quoted.double", "constant.character.escape", "brightgreen" },
    { "meta.preprocessor", "string.quoted.other", "red" },
    { "meta.preprocessor", "punctuation.separator.continuation", "yellow" },
};

/* @Test(dataSource = "test_builtin_ds") */
START_PARAMETRIZED_TEST (test_builtin, test_builtin_ds)
{
    syntax_scheme_t *builtin;
    syntax_scheme_style_t style;

    // given
    builtin = syntax_scheme_builtin ();
    mctest_assert_not_null (builtin);

    // when: MC color names are kept even in truecolor mode
    syntax_scheme_resolve (builtin, data->ctx_scope, data->scope, SYNTAX_SCHEME_DEPTH_TRUECOLOR,
                           &style);

    // then
    mctest_assert_str_eq (style.fg, data->fg);
    syntax_scheme_free (builtin);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tokens)
{
    char *s;

    mctest_assert_true (syntax_scheme_is_scope_token ("KEYWORD_CONTROL"));
    mctest_assert_true (syntax_scheme_is_scope_token ("ENTITY_NAME_H1"));
    mctest_assert_false (syntax_scheme_is_scope_token ("yellow"));
    mctest_assert_false (syntax_scheme_is_scope_token ("Yellow"));
    mctest_assert_false (syntax_scheme_is_scope_token ("123"));
    mctest_assert_false (syntax_scheme_is_scope_token (""));
    mctest_assert_false (syntax_scheme_is_scope_token (NULL));

    s = syntax_scheme_token_to_scope ("KEYWORD_CONTROL_FLOW");
    mctest_assert_str_eq (s, "keyword.control.flow");
    g_free (s);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_parse_error)
{
    GError *error = NULL;
    syntax_scheme_t *bad;

    bad = syntax_scheme_parse ("{\n \"rules\": [ { \"scope\" \"x\" } ]\n}", &error);

    mctest_assert_null (bad);
    mctest_assert_not_null (error);
    mctest_assert_str_eq (error->message, "line 2: ':' expected");
    g_error_free (error);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, test_resolve, test_resolve_ds);
    mctest_add_parameterized_test (tc_core, test_builtin, test_builtin_ds);
    tcase_add_test (tc_core, test_tokens);
    tcase_add_test (tc_core, test_parse_error);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
