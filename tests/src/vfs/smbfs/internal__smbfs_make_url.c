/*
   src/vfs/smbfs - tests for check URL making function.

   Copyright (C) 2013
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013

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

#define TEST_SUITE_NAME "/src/vfs/smbfs"

#include "tests/mctest.h"

#include "lib/vfs/vfs.h"
#include "lib/strutil.h"

#include "src/vfs/local/local.h"
#include "src/vfs/smbfs/init.h"
#include "src/vfs/smbfs/internal.h"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    init_localfs ();
    init_smbfs ();

    vfs_setup_work_dir ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_make_url_ds") */
/* *INDENT-OFF* */
static const struct test_make_url_ds
{
    const char *input_path;
    gboolean input_with_path;
    const char *expected_url;
} test_make_url_ds[] =
{
    { /* 0. */
        "/smb://",
        TRUE,
        "smb://"
    },
    { /* 1. */
        "/smb://",
        FALSE,
        "smb://"
    },
    { /* 2. */
        "/smb://MCTESTSERVER",
        TRUE,
        "smb://MCTESTSERVER"
    },
    { /* 3. */
        "/smb://MCTESTSERVER",
        FALSE,
        "smb://MCTESTSERVER"
    },
    { /* 4. */
        "/smb://WORKGROUP",
        TRUE,
        "smb://WORKGROUP"
    },
    { /* 5. */
        "/smb://WORKGROUP",
        FALSE,
        "smb://WORKGROUP"
    },
    { /* 6. */
        "/smb://WORKGROUP;smbUser@",
        TRUE,
        "smb://WORKGROUP;smbUser@"
    },
    { /* 7. */
        "/smb://WORKGROUP;smbUser@",
        FALSE,
        "smb://WORKGROUP;smbUser@"
    },
    { /* 8. */
        "/smb://WORKGROUP;smbUser:smbPass@",
        TRUE,
        "smb://WORKGROUP;smbUser:smbPass@"
    },
    { /* 9. */
        "/smb://WORKGROUP;smbUser:smbPass@",
        FALSE,
        "smb://WORKGROUP;smbUser:smbPass@"
    },
    { /* 10. */
        "smb://MCTESTSERVER/SHARE",
        TRUE,
        "smb://MCTESTSERVER/SHARE"
    },
    { /* 11. */
        "smb://MCTESTSERVER/SHARE",
        FALSE,
        "smb://MCTESTSERVER/SHARE"
    },
    { /* 12. */
        "smb://WORKGROUP;MCTESTSERVER/SHARE",
        TRUE,
        "smb://WORKGROUP;MCTESTSERVER/SHARE"
    },
    { /* 13. */
        "smb://WORKGROUP;MCTESTSERVER/SHARE",
        FALSE,
        "smb://WORKGROUP;MCTESTSERVER/SHARE"
    },
    { /* 14. */
        "smb://smbUser@MCTESTSERVER/SHARE/RESTRICTED_SHARE",
        TRUE,
        "smb://smbUser@MCTESTSERVER/SHARE/RESTRICTED_SHARE",
    },
    { /* 15. */
        "smb://smbUser@MCTESTSERVER/SHARE/RESTRICTED_SHARE",
        FALSE,
        "smb://smbUser@MCTESTSERVER/SHARE",
    },
    { /* 16. */
        "smb://smbUser:smbPass@MCTESTSERVER/SHARE/RESTRICTED_SHARE",
        TRUE,
        "smb://smbUser:smbPass@MCTESTSERVER/SHARE/RESTRICTED_SHARE"
    },
    { /* 17. */
        "smb://smbUser:smbPass@MCTESTSERVER/SHARE/RESTRICTED_SHARE",
        FALSE,
        "smb://smbUser:smbPass@MCTESTSERVER/SHARE"
    },
    { /* 18. */
        "smb://WORKGROUP;smbUser@MCTESTSERVER/SHARE/RESTRICTED_SHARE",
        TRUE,
        "smb://WORKGROUP;smbUser@MCTESTSERVER/SHARE/RESTRICTED_SHARE"
    },
    { /* 19. */
        "smb://WORKGROUP;smbUser@MCTESTSERVER/SHARE/RESTRICTED_SHARE",
        FALSE,
        "smb://WORKGROUP;smbUser@MCTESTSERVER/SHARE"
    },
    { /* 20. */
        "smb://WORKGROUP;smbUser:smbPass@MCTESTSERVER/SHARE/RESTRICTED_SHARE",
        TRUE,
        "smb://WORKGROUP;smbUser:smbPass@MCTESTSERVER/SHARE/RESTRICTED_SHARE"
    },
    { /* 21. */
        "smb://WORKGROUP;smbUser:smbPass@MCTESTSERVER/SHARE/RESTRICTED_SHARE",
        FALSE,
        "smb://WORKGROUP;smbUser:smbPass@MCTESTSERVER/SHARE"
    },
    { /* 22. */
        "smb://WORKGROUP;smbUser:smbPass@MCTESTSERVER/SHARE/RESTRICTED_SHARE/path/to/file.ext",
        TRUE,
        "smb://WORKGROUP;smbUser:smbPass@MCTESTSERVER/SHARE/RESTRICTED_SHARE/path/to/file.ext",
    },
    { /* 23. */
        "smb://WORKGROUP;smbUser:smbPass@MCTESTSERVER/SHARE/RESTRICTED_SHARE/path/to/file.ext",
        FALSE,
        "smb://WORKGROUP;smbUser:smbPass@MCTESTSERVER/SHARE"
    },
    { /* 24. */
        "smb://MCTESTSERVER/RESTRICTED_SHARE/path/to/file.ext",
        TRUE,
        "smb://MCTESTSERVER/RESTRICTED_SHARE/path/to/file.ext",
    },
    { /* 25. */
        "smb://MCTESTSERVER/RESTRICTED_SHARE/path/to/file.ext",
        FALSE,
        "smb://MCTESTSERVER/RESTRICTED_SHARE"
    },
    { /* 26. */
        "smb://[::1]/SHARE/path/to/file.ext",
        TRUE,
        "smb://[::1]/SHARE/path/to/file.ext",
    },
    { /* 25. */
        "smb://[::1]/SHARE/path/to/file.ext",
        FALSE,
        "smb://[::1]/SHARE"
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_make_url_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_make_url, test_make_url_ds)
/* *INDENT-ON* */
{
    /* given */
    char *url;
    const vfs_path_element_t *element;
    vfs_path_t *vpath;

    vpath = vfs_path_from_str (data->input_path);
    element = vfs_path_get_by_index (vpath, -1);


    /* when */
    url = smbfs_make_url (element, data->input_with_path);

    /* then */
    vfs_path_free (vpath);
    mctest_assert_str_eq (url, data->expected_url);
    g_free (url);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
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
    mctest_add_parameterized_test (tc_core, test_make_url, test_make_url_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "internal__smbfs_make_url.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
