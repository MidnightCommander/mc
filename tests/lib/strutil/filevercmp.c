/*
   lib/strutil - tests for lib/strutil/fileverscmp function.

   Copyright (C) 2019-2024
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2019

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

#define TEST_SUITE_NAME "/lib/strutil"

#include "tests/mctest.h"

#include "lib/strutil.h"
#include "lib/util.h"           /* _GL_CMP() */

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

static int
sign (int n)
{
    return _GL_CMP (n, 0);
}

/* --------------------------------------------------------------------------------------------- */

/*
 * Return filevercmp (a, a), checking that a similar result is gotten after replacing all '\1's
 * with '\0's and calling filenvercmp with the embedded '\0's.
 */
static int
test_filevercmp (char const *a, char const *b)
{
    int result;
    char buffer[BUF_1K];
    size_t alen, blen;
    size_t i;
    int nresult;

    result = filevercmp (a, b);

    alen = strlen (a);
    blen = strlen (b);

    ck_assert_int_le (alen + blen, sizeof (buffer));
    memcpy (buffer, a, alen);
    memcpy (buffer + alen, b, blen);

    for (i = 0; i < alen + blen; i++)
        if (buffer[i] == '\1')
            buffer[i] = '\0';

    nresult = filenvercmp (buffer, alen, buffer + alen, blen);
    ck_assert_int_eq (sign (nresult), sign (result));

    return result;
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("filevercmp_test_ds1") */
/* Testcases are taken from Gnulib */
/* *INDENT-OFF* */
static const struct filevercmp_test_struct
{
    const char *s1;
    const char *s2;
    int expected_result;
} filevercmp_test_ds1[] =
{
    { "", "", 0 },
    { "a", "a", 0 },
    { "a", "b", -1 },
    { "b", "a", 1 },
    { "00", "01", -1 },
    { "01", "010", -1 },
    { "9", "10", -1 },
    { "0a", "0", 1 }
};
/* *INDENT-ON* */


/* @Test(dataSource = "filevercmp_test_ds1") */
/* *INDENT-OFF* */
START_TEST (filevercmp_test1)
/* *INDENT-ON* */
{
    /* given */
    int actual_result;
    const struct filevercmp_test_struct *data = &filevercmp_test_ds1[_i];

    /* when */
    actual_result = filevercmp (data->s1, data->s2);

    /* then */
    ck_assert_int_eq (sign (actual_result), sign (data->expected_result));
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("filevercmp_test_ds2") */
/* Testcases are taken from Gnulib */
static const char *filevercmp_test_ds2[] = {
    "",
    ".",
    "..",
    ".0",
    ".9",
    ".A",
    ".Z",
    ".a~",
    ".a",
    ".b~",
    ".b",
    ".z",
    ".zz~",
    ".zz",
    ".zz.~1~",
    ".zz.0",
    ".\1",
    ".\1.txt",
    ".\1x",
    ".\1x\1",
    ".\1.0",
    "0",
    "9",
    "A",
    "Z",
    "a~",
    "a",
    "a.b~",
    "a.b",
    "a.bc~",
    "a.bc",
    "a+",
    "a.",
    "a..a",
    "a.+",
    "b~",
    "b",
    "gcc-c++-10.fc9.tar.gz",
    "gcc-c++-10.fc9.tar.gz.~1~",
    "gcc-c++-10.fc9.tar.gz.~2~",
    "gcc-c++-10.8.12-0.7rc2.fc9.tar.bz2",
    "gcc-c++-10.8.12-0.7rc2.fc9.tar.bz2.~1~",
    "glibc-2-0.1.beta1.fc10.rpm",
    "glibc-common-5-0.2.beta2.fc9.ebuild",
    "glibc-common-5-0.2b.deb",
    "glibc-common-11b.ebuild",
    "glibc-common-11-0.6rc2.ebuild",
    "libstdc++-0.5.8.11-0.7rc2.fc10.tar.gz",
    "libstdc++-4a.fc8.tar.gz",
    "libstdc++-4.10.4.20040204svn.rpm",
    "libstdc++-devel-3.fc8.ebuild",
    "libstdc++-devel-3a.fc9.tar.gz",
    "libstdc++-devel-8.fc8.deb",
    "libstdc++-devel-8.6.2-0.4b.fc8",
    "nss_ldap-1-0.2b.fc9.tar.bz2",
    "nss_ldap-1-0.6rc2.fc8.tar.gz",
    "nss_ldap-1.0-0.1a.tar.gz",
    "nss_ldap-10beta1.fc8.tar.gz",
    "nss_ldap-10.11.8.6.20040204cvs.fc10.ebuild",
    "z",
    "zz~",
    "zz",
    "zz.~1~",
    "zz.0",
    "zz.0.txt",
    "\1",
    "\1.txt",
    "\1x",
    "\1x\1",
    "\1.0",
    "#\1.b#",
    "#.b#"
};

const size_t filevercmp_test_ds2_len = G_N_ELEMENTS (filevercmp_test_ds2);

/* @Test(dataSource = "filevercmp_test_ds2") */
/* *INDENT-OFF* */
START_TEST (filevercmp_test2)
/* *INDENT-ON* */
{
    const char *i = filevercmp_test_ds2[_i];
    size_t _j;

    for (_j = 0; _j < filevercmp_test_ds2_len; _j++)
    {
        const char *j = filevercmp_test_ds2[_j];
        int result;

        result = test_filevercmp (i, j);

        if (result < 0)
            ck_assert_int_lt ((size_t) _i, _j);
        else if (result > 0)
            ck_assert_int_gt ((size_t) _i, _j);
        else
            ck_assert_int_eq ((size_t) _i, _j);
    }
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */


/* @DataSource("filevercmp_test_ds3") */
/* Ticket #3959 */
static const char *filevercmp_test_ds3[] = {
    "application-1.10.tar.gz",
    "application-1.10.1.tar.gz"
};

const size_t filevercmp_test_ds3_len = G_N_ELEMENTS (filevercmp_test_ds3);

/* @Test(dataSource = "filevercmp_test_ds3") */
/* *INDENT-OFF* */
START_TEST (filevercmp_test3)
/* *INDENT-ON* */
{
    const char *i = filevercmp_test_ds3[_i];
    size_t _j;

    for (_j = 0; _j < filevercmp_test_ds3_len; _j++)
    {
        const char *j = filevercmp_test_ds3[_j];
        int result;

        result = filevercmp (i, j);

        if (result < 0)
            ck_assert_int_lt ((size_t) _i, _j);
        else if (result > 0)
            ck_assert_int_gt ((size_t) _i, _j);
        else
            ck_assert_int_eq ((size_t) _i, _j);
    }
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */


/* @DataSource("filevercmp_test_ds4") */
/* Ticket #3905 */
static const char *filevercmp_test_ds4[] = {
    "firefox-58.0.1+build1.tar.gz",
    "firefox-59.0~b14+build1.tar.gz",
    "firefox-59.0.1+build1.tar.gz"
};

const size_t filevercmp_test_ds4_len = G_N_ELEMENTS (filevercmp_test_ds4);

/* @Test(dataSource = "filevercmp_test_ds4") */
/* *INDENT-OFF* */
START_TEST (filevercmp_test4)
/* *INDENT-ON* */
{
    const char *i = filevercmp_test_ds4[_i];
    size_t _j;

    for (_j = 0; _j < filevercmp_test_ds4_len; _j++)
    {
        const char *j = filevercmp_test_ds4[_j];
        int result;

        result = filevercmp (i, j);

        if (result < 0)
            ck_assert_int_lt ((size_t) _i, _j);
        else if (result > 0)
            ck_assert_int_gt ((size_t) _i, _j);
        else
            ck_assert_int_eq ((size_t) _i, _j);
    }
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */


/* @DataSource("filevercmp_test_ds5") */
/* Testcases are taken from Gnulib */
static const char *filevercmp_test_ds5[] = {
    "a",
    "a0",
    "a0000",
    NULL,
    "a\1c-27.txt",
    "a\1c-027.txt",
    "a\1c-00000000000000000000000000000000000000000000000000000027.txt",
    NULL,
    ".a\1c-27.txt",
    ".a\1c-027.txt",
    ".a\1c-00000000000000000000000000000000000000000000000000000027.txt",
    NULL,
    "a\1c-",
    "a\1c-0",
    "a\1c-00",
    NULL,
    ".a\1c-",
    ".a\1c-0",
    ".a\1c-00",
    NULL,
    "a\1c-0.txt",
    "a\1c-00.txt",
    NULL,
    ".a\1c-1\1.txt",
    ".a\1c-001\1.txt",
    NULL
};

const size_t filevercmp_test_ds5_len = G_N_ELEMENTS (filevercmp_test_ds5);

/* @Test(dataSource = "filevercmp_test_ds5") */
/* *INDENT-OFF* */
START_TEST (filevercmp_test5)
/* *INDENT-ON* */
{
    size_t ii;

    for (ii = 0; ii < filevercmp_test_ds5_len; ii++)
        for (; filevercmp_test_ds5[ii] != NULL; ii++)
        {
            const char *i = filevercmp_test_ds5[ii];
            size_t jj;

            for (jj = ii; filevercmp_test_ds5[jj] != NULL; jj++)
            {
                const char *j = filevercmp_test_ds5[jj];

                ck_assert_int_eq (test_filevercmp (i, j), 0);
                ck_assert_int_eq (test_filevercmp (j, i), 0);
            }
        }
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
    mctest_add_parameterized_test (tc_core, filevercmp_test1, filevercmp_test_ds1);
    tcase_add_loop_test (tc_core, filevercmp_test2, 0, filevercmp_test_ds2_len);
    tcase_add_loop_test (tc_core, filevercmp_test3, 0, filevercmp_test_ds3_len);
    tcase_add_loop_test (tc_core, filevercmp_test4, 0, filevercmp_test_ds4_len);
    tcase_add_test (tc_core, filevercmp_test5);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
