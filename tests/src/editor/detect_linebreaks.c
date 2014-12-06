/*
   src/editor - check 'detect linebreaks' functionality

   Copyright (C) 2011
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011

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

#define TEST_SUITE_NAME "src/editor/detect_linebreaks"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <check.h>

#include <config.h>

#include "common_editor_includes.c" /* for testing static functions*/

#include "src/vfs/local/local.h"

const char *filename = "detect_linebreaks.in.txt";
struct macro_action_t record_macro_buf[MAX_MACRO_LENGTH];

static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    init_localfs ();
    vfs_setup_work_dir ();
}

static void
teardown (void)
{
    vfs_shut ();
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_detect_lb_type)
{
    LineBreaks result;
    char buf[] = "Test for detect line break\r\n";

    result = detect_lb_type_buf ((unsigned char *) buf, strlen(buf));
    fail_unless(result == LB_WIN, "Incorrect lineBreak: result(%d) != LB_WIN(%d)",result, LB_WIN);

    unlink(filename);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_detect_lb_type_very_long_string)
{
    LineBreaks result;
    char buf[1024] = "";
    int i;
    for (i = 0; i<20 ; i++)
    {
        strcat (buf, "Very long string. ");
    }
    strcat (buf, "\r\n");

    result = detect_lb_type_buf ((unsigned char *) buf, strlen(buf));
    fail_unless(result == LB_WIN, "Incorrect lineBreak: result(%d) != LB_WIN(%d)",result, LB_WIN);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_detect_lb_type_rrrrrn)
{
    LineBreaks result;
    char buf[1024] = "";
    strcat (buf, "test\r\r\r\r\r\n");

    result = detect_lb_type_buf ((unsigned char *) buf, strlen(buf));
    fail_unless(result == LB_ASIS, "Incorrect lineBreak: result(%d) != LB_ASIS(%d)", result, LB_ASIS);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_detect_lb_type_nnnnnr)
{
    LineBreaks result;
    char buf[] = "test\n\n\n\n\n\r  ";

    result = detect_lb_type_buf ((unsigned char *) buf, strlen(buf));
    fail_unless(result == LB_ASIS, "Incorrect lineBreak: result(%d) != LB_ASIS(%d)",result, LB_ASIS);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_detect_lb_type_nnnrnrnnnn)
{
    LineBreaks result;
    char buf[] = "test\n\n\n\r\n\r\n\r\n\n\n\n";

    result = detect_lb_type_buf ((unsigned char *) buf, strlen(buf));
    fail_unless(result == LB_ASIS, "Incorrect lineBreak: result(%d) != LB_ASIS(%d)",result, LB_ASIS);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_detect_lb_type_rrrrrr)
{
    LineBreaks result;
    char buf[1024] = "test\r\r\r\r\r\r";

    result = detect_lb_type_buf ((unsigned char *) buf, strlen(buf));
    fail_unless(result == LB_MAC, "Incorrect lineBreak: result(%d) != LB_MAC(%d)", result, LB_MAC);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_detect_lb_type_nnnnnn)
{
    LineBreaks result;
    char buf[1024] = "test\n\n\n\n\n\n";

    result = detect_lb_type_buf ((unsigned char *) buf, strlen(buf));
    fail_unless(result == LB_ASIS, "Incorrect lineBreak: result(%d) != LB_ASIS(%d)", result, LB_ASIS);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_detect_lb_type_buffer_border)
{
    LineBreaks result;
    char buf[DETECT_LB_TYPE_BUFLEN + 100];
    memset(buf, ' ', DETECT_LB_TYPE_BUFLEN);
    buf[DETECT_LB_TYPE_BUFLEN - 101] = '\r';
    buf[DETECT_LB_TYPE_BUFLEN - 100] = '\n';
    buf[DETECT_LB_TYPE_BUFLEN - 1] = '\r';
    buf[DETECT_LB_TYPE_BUFLEN] = '\n';

    result = detect_lb_type_buf ((unsigned char *) buf, DETECT_LB_TYPE_BUFLEN);
    fail_unless(result == LB_WIN, "Incorrect lineBreak: result(%d) != LB_WIN(%d)", result, LB_WIN);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_detect_lb_type_buffer_border_overflow)
{
    LineBreaks result;
    char buf[DETECT_LB_TYPE_BUFLEN + 100];
    memset(buf, ' ', DETECT_LB_TYPE_BUFLEN);
    buf[DETECT_LB_TYPE_BUFLEN - 100] = '\r';
    buf[DETECT_LB_TYPE_BUFLEN - 1] = '\r';

    strcat (buf, "bla-bla\r\n");

    result = detect_lb_type_buf ((unsigned char *) buf, DETECT_LB_TYPE_BUFLEN);
    fail_unless(result == LB_MAC, "Incorrect lineBreak: result(%d) != LB_MAC(%d)",result, LB_MAC);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_detect_lb_type_buffer_border_more)
{
    LineBreaks result;
    char buf[DETECT_LB_TYPE_BUFLEN + 100];
    memset(buf, ' ', DETECT_LB_TYPE_BUFLEN);

    strcat (buf, "bla-bla\n");

    result = detect_lb_type_buf ((unsigned char *) buf, strlen(buf));
    fail_unless(result == LB_ASIS, "Incorrect lineBreak: result(%d) != LB_ASIS(%d)",result, LB_ASIS);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    int number_failed;

    Suite *s = suite_create (TEST_SUITE_NAME);
    TCase *tc_core = tcase_create ("Core");
    SRunner *sr;

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    tcase_add_test (tc_core, test_detect_lb_type);
    tcase_add_test (tc_core, test_detect_lb_type_very_long_string);
    tcase_add_test (tc_core, test_detect_lb_type_rrrrrn);
    tcase_add_test (tc_core, test_detect_lb_type_nnnnnr);
    tcase_add_test (tc_core, test_detect_lb_type_nnnrnrnnnn);
    tcase_add_test (tc_core, test_detect_lb_type_rrrrrr);
    tcase_add_test (tc_core, test_detect_lb_type_nnnnnn);
    tcase_add_test (tc_core, test_detect_lb_type_buffer_border);
    tcase_add_test (tc_core, test_detect_lb_type_buffer_border_overflow);
    tcase_add_test (tc_core, test_detect_lb_type_buffer_border_more);
   /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "detect_linebreaks.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
