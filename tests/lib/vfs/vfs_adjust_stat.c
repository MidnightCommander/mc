/*
   lib/vfs - test vfs_adjust_stat() functionality

   Copyright (C) 2017-2024
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2017

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

#include <sys/stat.h>

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_test_vfs_adjust_stat_ds") */
/* *INDENT-OFF* */
static const struct test_vfs_adjust_stat_ds
{
    struct stat etalon_stat;
} test_vfs_adjust_stat_ds[] =
{
    /* 0 */
    {
        .etalon_stat =
        {
            .st_size = 0,
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
            .st_blksize = 512,
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
            .st_blocks = 0
#endif
        }
    },
    /* 1 */
    {
        .etalon_stat =
        {
            .st_size = 4096,
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
            .st_blksize = 512,
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
            .st_blocks = 8
#endif
        }
    },
    /* 2 */
    {
        .etalon_stat =
        {
            .st_size = 4096,
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
            .st_blksize = 1024,
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
            .st_blocks = 8
#endif
        }
    },
    /* 3 */
    {
        .etalon_stat =
        {
            .st_size = 4096,
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
            .st_blksize = 2048,
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
            .st_blocks = 8
#endif
        }
    },
    /* 4 */
    {
        .etalon_stat =
        {
            .st_size = 4096,
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
            .st_blksize = 4096,
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
            .st_blocks = 8
#endif
        }
    },
    /* 5 */
    {
        .etalon_stat =
        {
            .st_size = 5000,
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
            .st_blksize = 512,
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
            .st_blocks = 10
#endif
        }
    },
    /* 6 */
    {
        .etalon_stat =
        {
            .st_size = 5000,
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
            .st_blksize = 1024,
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
            .st_blocks = 10
#endif
        }
    },
    /* 7 */
    {
        .etalon_stat =
        {
            .st_size = 5000,
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
            .st_blksize = 2048,
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
            .st_blocks = 12
#endif
        }
    },
    /* 8 */
    {
        .etalon_stat =
        {
            .st_size = 5000,
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
            .st_blksize = 4096,
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
            .st_blocks = 16
#endif
        }
    }
};
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @Test(dataSource = "test_vfs_adjust_stat_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_vfs_adjust_stat, test_vfs_adjust_stat_ds)
/* *INDENT-ON* */
{
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
    /* given */
    struct stat expected_stat;

    expected_stat.st_size = data->etalon_stat.st_size;
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    expected_stat.st_blksize = data->etalon_stat.st_blksize;
#endif /* HAVE_STRUCT_STAT_ST_BLKSIZE */
    /* when */
    vfs_adjust_stat (&expected_stat);

    /* then */
    ck_assert_int_eq (data->etalon_stat.st_blocks, expected_stat.st_blocks);
#else
    ck_assert_int_eq (0, 0);
#endif /* HAVE_STRUCT_STAT_ST_BLOCKS */
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, test_vfs_adjust_stat, test_vfs_adjust_stat_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
