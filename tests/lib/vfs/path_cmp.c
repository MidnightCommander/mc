/* lib/vfs - vfs_path_t compare functions

   Copyright (C) 2011 Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#define TEST_SUITE_NAME "/lib/vfs"

#include <check.h>

#include "lib/global.c"

#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/path.h"

#include "src/vfs/local/local.c"


static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    init_localfs ();
    vfs_setup_work_dir ();

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
#ifdef HAVE_CHARSET
    load_codepages_list ();
#endif
}

static void
teardown (void)
{
#ifdef HAVE_CHARSET
    free_codepages_list ();
#endif

    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */
#define path_cmp_one_check(input1, input2, etalon_condition) {\
    vpath1 = vfs_path_from_str (input1);\
    vpath2 = vfs_path_from_str (input2);\
    result = vfs_path_cmp (vpath1, vpath2);\
    vfs_path_free (vpath1); \
    vfs_path_free (vpath2); \
    fail_unless ( result etalon_condition, "\ninput1: %s\ninput2: %s\nexpected: %d\nactual: %d\n",\
        input1, input2, #etalon_condition, result); \
}

/* *INDENT-OFF* */
START_TEST (test_path_compare)
/* *INDENT-ON* */
{
    vfs_path_t *vpath1, *vpath2;
    int result;

    path_cmp_one_check ("/тестовый/путь", "/тестовый/путь", == 0);

#ifdef HAVE_CHARSET
    path_cmp_one_check ("/#enc:KOI8-R/тестовый/путь", "/тестовый/путь", <0);
    path_cmp_one_check ("/тестовый/путь", "/#enc:KOI8-R/тестовый/путь", >0);
#endif

    path_cmp_one_check (NULL, "/тестовый/путь", -1);
    path_cmp_one_check ("/тестовый/путь", NULL, -1);
    path_cmp_one_check (NULL, NULL, -1);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */
#undef path_cmp_one_check

#define path_cmp_one_check(input1, input2, len, etalon_condition) {\
    vpath1 = vfs_path_from_str (input1);\
    vpath2 = vfs_path_from_str (input2);\
    result = vfs_path_ncmp (vpath1, vpath2, len);\
    vfs_path_free (vpath1); \
    vfs_path_free (vpath2); \
    fail_unless ( result etalon_condition, "\ninput1: %s\ninput2: %s\nexpected: %d\nactual: %d\n",\
        input1, input2, #etalon_condition, result); \
}

/* *INDENT-OFF* */
START_TEST (test_path_compare_len)
/* *INDENT-ON* */
{
    vfs_path_t *vpath1, *vpath2;
    int result;

    path_cmp_one_check ("/тестовый/путь", "/тестовый/путь", 10, == 0);

    path_cmp_one_check ("/тест/овый/путь", "/тестовый/путь", 10, <0);

    path_cmp_one_check ("/тестовый/путь", "/тест/овый/путь", 10, >0);

    path_cmp_one_check ("/тест/овый/путь", "/тестовый/путь", 9, == 0);

    path_cmp_one_check (NULL, "/тестовый/путь", 0, <0);
    path_cmp_one_check ("/тестовый/путь", NULL, 0, <0);
    path_cmp_one_check (NULL, NULL, 0, <0);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

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
    tcase_add_test (tc_core, test_path_compare);
    tcase_add_test (tc_core, test_path_compare_len);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "path_cmp.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
