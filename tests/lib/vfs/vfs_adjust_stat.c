/*
   lib/vfs - test vfs_adjust_stat() functionality

   Copyright (C) 2017-2025
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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/lib/vfs"

#include "tests/mctest.h"

#include <sys/stat.h>

/* --------------------------------------------------------------------------------------------- */

#if defined(HAVE_STRUCT_STAT_ST_BLKSIZE) && defined(HAVE_STRUCT_STAT_ST_BLOCKS)
#define STRUCT_STAT(size, blksize, blocks)                                                         \
    {                                                                                              \
        .st_size = size,                                                                           \
        .st_blksize = blksize,                                                                     \
        .st_blocks = blocks,                                                                       \
    }
#elif defined(HAVE_STRUCT_STAT_ST_BLKSIZE)
#define STRUCT_STAT(st_blksize, st_blocks)                                                         \
    {                                                                                              \
        .st_size = size,                                                                           \
        .st_blksize = st_blksize,                                                                  \
    }
#elif defined(HAVE_STRUCT_STAT_ST_BLOCKS)
#define STRUCT_STAT(st_blksize, st_blocks)                                                         \
    {                                                                                              \
        .st_size = size,                                                                           \
        .st_blocks = st_blocks,                                                                    \
    }
#else
#define STRUCT_STAT(st_blksize, st_blocks)                                                         \
    {                                                                                              \
        .st_size = size,                                                                           \
    }
#endif

/* @DataSource("test_test_vfs_adjust_stat_ds") */
static const struct test_vfs_adjust_stat_ds
{
    struct stat etalon_stat;
} test_vfs_adjust_stat_ds[] = {
    { .etalon_stat = STRUCT_STAT (0, 512, 0) },       // 0
    { .etalon_stat = STRUCT_STAT (4096, 512, 8) },    // 1
    { .etalon_stat = STRUCT_STAT (4096, 1024, 8) },   // 2
    { .etalon_stat = STRUCT_STAT (4096, 2048, 8) },   // 3
    { .etalon_stat = STRUCT_STAT (4096, 4096, 8) },   // 4
    { .etalon_stat = STRUCT_STAT (5000, 512, 10) },   // 5
    { .etalon_stat = STRUCT_STAT (5000, 1024, 10) },  // 6
    { .etalon_stat = STRUCT_STAT (5000, 2048, 12) },  // 7
    { .etalon_stat = STRUCT_STAT (5000, 4096, 16) },  // 8
};

#undef STRUCT_STAT

/* --------------------------------------------------------------------------------------------- */

/* @Test(dataSource = "test_vfs_adjust_stat_ds") */
START_PARAMETRIZED_TEST (test_vfs_adjust_stat, test_vfs_adjust_stat_ds)
{
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
    // given
    struct stat expected_stat;

    expected_stat.st_size = data->etalon_stat.st_size;
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    expected_stat.st_blksize = data->etalon_stat.st_blksize;
#endif
    // when
    vfs_adjust_stat (&expected_stat);

    // then
    ck_assert_int_eq (data->etalon_stat.st_blocks, expected_stat.st_blocks);
#else
    ck_assert_int_eq (0, 0);
#endif
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, test_vfs_adjust_stat, test_vfs_adjust_stat_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
