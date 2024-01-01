/*
   lib/vfs - vfs_path_t serialize/deserialize functions

   Copyright (C) 2011-2024
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011, 2013

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

#define TEST_SUITE_NAME "/lib/vfs"

#include "tests/mctest.h"

#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/path.h"

#include "src/vfs/local/local.c"

static struct vfs_class vfs_test_ops1, vfs_test_ops2, vfs_test_ops3;

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    vfs_init_class (&vfs_test_ops1, "testfs1", VFSF_NOLINKS | VFSF_REMOTE, "test1");
    vfs_register_class (&vfs_test_ops1);

    vfs_init_class (&vfs_test_ops2, "testfs2", VFSF_UNKNOWN, "test2");
    vfs_register_class (&vfs_test_ops2);

    vfs_init_class (&vfs_test_ops3, "testfs3", VFSF_UNKNOWN, "test3");
    vfs_register_class (&vfs_test_ops3);

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
#ifdef HAVE_CHARSET
    load_codepages_list ();
#endif
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
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

#ifdef HAVE_CHARSET
#define ETALON_PATH_STR "/local/path/#test1:user:pass@some.host:12345/bla-bla/some/path/#test2/#enc:KOI8-R/bla-bla/some/path#test3/111/22/33"
#define ETALON_PATH_URL_STR "/local/path/test1://user:pass@some.host:12345/bla-bla/some/path/test2://#enc:KOI8-R/bla-bla/some/path/test3://111/22/33"
#define ETALON_SERIALIZED_PATH \
    "g14:path-element-0" \
        "p4:pathv12:/local/path/" \
        "p10:class-namev7:localfs" \
    "g14:path-element-1" \
        "p4:pathv18:bla-bla/some/path/" \
        "p10:class-namev7:testfs1" \
        "p10:vfs_prefixv5:test1" \
        "p4:userv4:user" \
        "p8:passwordv4:pass" \
        "p4:hostv9:some.host" \
        "p4:portv5:12345" \
    "g14:path-element-2" \
        "p4:pathv17:bla-bla/some/path" \
        "p10:class-namev7:testfs2" \
        "p8:encodingv6:KOI8-R" \
        "p10:vfs_prefixv5:test2" \
    "g14:path-element-3" \
        "p4:pathv9:111/22/33" \
        "p10:class-namev7:testfs3" \
        "p10:vfs_prefixv5:test3"
#else
#define ETALON_PATH_STR "/local/path/#test1:user:pass@some.host:12345/bla-bla/some/path/#test2/bla-bla/some/path#test3/111/22/33"
#define ETALON_PATH_URL_STR "/local/path/test1://user:pass@some.host:12345/bla-bla/some/path/test2://bla-bla/some/path/test3://111/22/33"
#define ETALON_SERIALIZED_PATH \
    "g14:path-element-0" \
        "p4:pathv12:/local/path/" \
        "p10:class-namev7:localfs" \
    "g14:path-element-1" \
        "p4:pathv18:bla-bla/some/path/" \
        "p10:class-namev7:testfs1" \
        "p10:vfs_prefixv5:test1" \
        "p4:userv4:user" \
        "p8:passwordv4:pass" \
        "p4:hostv9:some.host" \
        "p4:portv5:12345" \
    "g14:path-element-2" \
        "p4:pathv17:bla-bla/some/path" \
        "p10:class-namev7:testfs2" \
        "p10:vfs_prefixv5:test2" \
    "g14:path-element-3" \
        "p4:pathv9:111/22/33" \
        "p10:class-namev7:testfs3" \
        "p10:vfs_prefixv5:test3"
#endif

/* *INDENT-OFF* */
START_TEST (test_path_serialize)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath;
    char *serialized_vpath;
    GError *error = NULL;

    /* when */
    vpath = vfs_path_from_str_flags (ETALON_PATH_STR, VPF_USE_DEPRECATED_PARSER);
    serialized_vpath = vfs_path_serialize (vpath, &error);

    vfs_path_free (vpath, TRUE);

    if (error != NULL)
        g_error_free (error);

    /* then */
    mctest_assert_ptr_ne (serialized_vpath, NULL);
    mctest_assert_str_eq (serialized_vpath, ETALON_SERIALIZED_PATH);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_path_deserialize)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath;
    GError *error = NULL;

    /* when */
    vpath = vfs_path_deserialize (ETALON_SERIALIZED_PATH, &error);

    /* then */
    mctest_assert_ptr_ne (vpath, NULL);
    mctest_assert_str_eq (vfs_path_as_str (vpath), ETALON_PATH_URL_STR);

    vfs_path_free (vpath, TRUE);

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
    tcase_add_test (tc_core, test_path_serialize);
    tcase_add_test (tc_core, test_path_deserialize);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
