/*
   lib - Read string from mc_pipe_stream

   Copyright (C) 2021-2024
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2021

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/lib/util"

#include "tests/mctest.h"

#include "lib/util.h"

/* --------------------------------------------------------------------------------------------- */

#define MAX_CHUNKS 8

/* --------------------------------------------------------------------------------------------- */

static mc_pipe_stream_t stream;

static char etalon_long_file_list[BUF_1K];
static size_t etalon_long_file_list_pos;

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
}

/* @After */
static void
teardown (void)
{
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("data_source") */
/* *INDENT-OFF* */
static const struct data_source
{
    /* input */
    const char *buf;                /* string to read */

    /* output */
    int pos[MAX_CHUNKS];            /* ps.pos values */
    const char *str[MAX_CHUNKS];    /* chunks */
    size_t len[MAX_CHUNKS];         /* chunk lengths */
}
data_source[] =
{
    /* 0 */
    {
        .buf = "",
        .pos = { 0 },
        .str = { "" },
        .len = { 0 }
    },
    /* 1 */
    {
        .buf = "\n",
        .pos = { 0, 1 },
        .str = { "\n" },
        .len = { 1, 0 }
    },
    /* 2 */
    {
        .buf = "\\\n",
        .pos = { 0, 2 },
        .str = { "\\\n" },
        .len = { 2, 0 }
    },
    /* 3 */
    {
        .buf = "\\\\\n",
        .pos = { 0, 3 },
        .str = { "\\\\\n" },
        .len = { 3, 0 }
    },
    /* 4 */
    {
        .buf = "\\\\\\\n",
        .pos = { 0, 4 },
        .str = { "\\\\\\\n" },
        .len = { 4, 0 }
    },
    /* 5 */
    {
        .buf = "\\\\\\\\\n",
        .pos = { 0, 5 },
        .str = { "\\\\\\\\\n" },
        .len = { 5, 0 }
    },
    /* 6 */
    {
        .buf = "12345",
        .pos = { 0, 5 },
        .str = { "12345" },
        .len = { 5, 0 }
    },
    /* 7 */
    {
        .buf = "12345\n",
        .pos = { 0, 6 },
        .str = { "12345\n" },
        .len = { 6, 0 }
    },
    /* 8 */
    {
        .buf = "12345\\\n",
        .pos = { 0, 7 },
        .str = { "12345\\\n" },
        .len = { 7, 0 }
    },
    /* 9 */
    {
        .buf = "12345\\\\\n",
        .pos = { 0, 8 },
        .str = { "12345\\\\\n" },
        .len = { 8, 0 }
    },
    /* 10 */
    {
        .buf = "12345\nabcd",
        .pos = { 0, 6, 10 },
        .str = { "12345\n", "abcd" },
        .len = { 6, 4, 0 }
    },
    /* 11 */
    {
        .buf = "12345\\\nabcd",
        .pos = { 0, 11 },
        .str = { "12345\\\nabcd" },
        .len = { 11, 0 }
    },
    /* 12 */
    {
        .buf = "12345\\\\\nabcd",
        .pos = { 0, 8, 12 },
        .str = { "12345\\\\\n", "abcd" },
        .len = { 8, 4, 0 }
    },
    /* 13 */
    {
        .buf = "12345\\\\\\\nabcd",
        .pos = { 0, 13 },
        .str = { "12345\\\\\\\nabcd" },
        .len = { 13, 0 }
    },
    /* 14 */
    {
        .buf = "12345\\\\\\\\\nabcd",
        .pos = { 0, 10, 14 },
        .str = { "12345\\\\\\\\\n", "abcd" },
        .len = { 10, 4, 0 }
    },
    /* 15 */
    {
        .buf = "12345\nabcd\n",
        .pos = { 0, 6, 11 },
        .str = { "12345\n", "abcd\n" },
        .len = { 6, 5, 0 }
    },
    /* 16 */
    {
        .buf = "12345\nabcd\n~!@#$%^",
        .pos = { 0, 6, 11, 18  },
        .str = { "12345\n", "abcd\n", "~!@#$%^" },
        .len = { 6, 5, 7, 0 }
    },
    /* 17 */
    {
        .buf = "12345\nabcd\n~!@#$%^\n",
        .pos = { 0, 6, 11, 19 },
        .str = { "12345\n", "abcd\n", "~!@#$%^\n" },
        .len = { 6, 5, 8, 0 }
    }
};
/* *INDENT-ON* */

/* @Test(dataSource = "data_source") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (mc_pstream_get_string_test, data_source)
/* *INDENT-ON* */
{
    /* given */
    int j = 0;

    /* when */
    memset (&stream, 0, sizeof (stream));
    stream.len = strlen (data->buf);
    memmove (&stream.buf, data->buf, stream.len);

    /* then */
    do
    {
        GString *ret;

        ck_assert_int_eq (stream.pos, data->pos[j]);

        ret = mc_pstream_get_string (&stream);
        if (ret == NULL)
            break;

        ck_assert_int_eq (ret->len, data->len[j]);
        mctest_assert_str_eq (ret->str, data->str[j]);

        g_string_free (ret, TRUE);

        j++;
    }
    while (TRUE);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

static mc_pipe_t *
test_mc_popen (void)
{
    mc_pipe_t *p;

    p = g_try_new0 (mc_pipe_t, 1);
    /* make less than sizeof (etalon_long_file_list) */
    p->out.len = 128;

    etalon_long_file_list_pos = 0;

    return p;
}

static void
test_mc_pread (mc_pipe_t *p)
{
    size_t len;

    p->out.pos = 0;

    if (etalon_long_file_list_pos >= sizeof (etalon_long_file_list))
    {
        etalon_long_file_list_pos = sizeof (etalon_long_file_list);
        p->out.len = MC_PIPE_STREAM_EOF;
        return;
    }

    len = sizeof (etalon_long_file_list) - etalon_long_file_list_pos;
    len = MIN (len, (size_t) p->out.len);
    memmove (p->out.buf, etalon_long_file_list + etalon_long_file_list_pos, len);
    p->out.len = (ssize_t) len;

    etalon_long_file_list_pos += len;
}

/* *INDENT-OFF* */
START_TEST (mc_pstream_get_long_file_list_test)
/* *INDENT-ON* */

{
    /* given */
    GString *result_long_file_list = NULL;
    mc_pipe_t *pip;
    GString *remain_file_name = NULL;

    /* when */
    /* fill the list */
    memset (etalon_long_file_list, 'a', sizeof (etalon_long_file_list) - 1);
    /* create an \n-separated list */
    etalon_long_file_list[5] = '\n';
    etalon_long_file_list[25] = '\n';
    etalon_long_file_list[50] = '\n';
    etalon_long_file_list[75] = '\n';
    etalon_long_file_list[127] = '\n';
    etalon_long_file_list[200] = '\n';
    etalon_long_file_list[310] = '\n';
    etalon_long_file_list[325] = '\n';
    etalon_long_file_list[360] = '\n';
    etalon_long_file_list[512] = '\n';
    etalon_long_file_list[701] = '\n';
    etalon_long_file_list[725] = '\n';
    etalon_long_file_list[800] = '\n';
    etalon_long_file_list[sizeof (etalon_long_file_list) - 2] = '\n';
    etalon_long_file_list[sizeof (etalon_long_file_list) - 1] = '\0';

    /* then */
    /* read file list */
    pip = test_mc_popen ();

    while (TRUE)
    {
        GString *line;

        test_mc_pread (pip);

        if (pip->out.len == MC_PIPE_STREAM_EOF)
            break;

        while ((line = mc_pstream_get_string (&pip->out)) != NULL)
        {
            /* handle an \n-separated file list */

            if (line->str[line->len - 1] == '\n')
            {
                /* entire file name or last chunk */

                g_string_truncate (line, line->len - 1);

                /* join filename chunks */
                if (remain_file_name != NULL)
                {
                    g_string_append_len (remain_file_name, line->str, line->len);
                    g_string_free (line, TRUE);
                    line = remain_file_name;
                    remain_file_name = NULL;
                }
            }
            else
            {
                /* first or middle chunk of file name */
                if (remain_file_name == NULL)
                    remain_file_name = line;
                else
                {
                    g_string_append_len (remain_file_name, line->str, line->len);
                    g_string_free (line, TRUE);
                }

                line = NULL;
            }

            /* collect file names to assemble the result string */
            if (line == NULL)
                continue;

            if (result_long_file_list == NULL)
                result_long_file_list = line;
            else
            {
                g_string_append_len (result_long_file_list, line->str, line->len);
                g_string_free (line, TRUE);
            }

            g_string_append_c (result_long_file_list, '\n');
        }
    }

    mctest_assert_str_eq (etalon_long_file_list, result_long_file_list->str);
    g_string_free (result_long_file_list, TRUE);

}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, mc_pstream_get_string_test, data_source);
    tcase_add_test (tc_core, mc_pstream_get_long_file_list_test);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
