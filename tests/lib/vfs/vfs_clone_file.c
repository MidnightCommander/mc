/*
   lib/vfs - test vfs_clone_file() functionality

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Phil Krylov <phil@krylov.eu>, 2026

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

#include <stdarg.h>
#include <stdlib.h>

#ifdef HAVE_FICLONERANGE
#include <linux/fs.h>   // FICLONERANGE
#include <sys/ioctl.h>  // ioctl()
#elif defined(HAVE_COPY_FILE_RANGE)
#include <unistd.h>  // copy_file_range()
#elif defined(HAVE_REFLINK)
#include <unistd.h>  // reflink()
#elif defined(HAVE_SYS_CLONEFILE_H)
#include <sys/clonefile.h>  // clonefile()
#endif

#include "lib/strutil.h"
#include "lib/util.h"
#include "src/vfs/local/local.c"

/* --------------------------------------------------------------------------------------------- */

static int clone_syscall__call_count = 0;
static gboolean clone_syscall__call_arguments_are_proper = FALSE;

static const char test_filename1[] = "mctestclone1.tst";
static const char test_filename2[] = "mctestclone2.tst";

#ifdef HAVE_COPY_FILE_RANGE
/* @Mock */
ssize_t
copy_file_range (int infd, off_t *inoffp, int outfd, off_t *outoffp, size_t len, unsigned int flags)
{
    (void) infd;
    (void) inoffp;
    (void) outfd;
    (void) outoffp;
    (void) len;

    clone_syscall__call_count++;
    clone_syscall__call_arguments_are_proper = (flags == COPY_FILE_RANGE_CLONE);

    return -1;
}
#endif

#ifdef HAVE_FICLONERANGE
#ifdef __GLIBC__
/* @Mock */
int
ioctl (int fd, unsigned long request, ...)
#else  // POSIX, musl
/* @Mock */
int
ioctl (int fd, int request, ...)
#endif
{
    (void) fd;

    clone_syscall__call_count++;
    clone_syscall__call_arguments_are_proper = (request == FICLONERANGE);
    return -1;
}
#endif

#ifdef HAVE_SYS_CLONEFILE_H
/* @Mock */
int
my_clonefile (const char *src, const char *dst, uint32_t flags)
{
    (void) src;
    (void) dst;

    clone_syscall__call_count++;
    clone_syscall__call_arguments_are_proper = (flags == 0);
    return -1;
}
#endif

#ifdef HAVE_REFLINK
/* @Mock */
int
reflink (const char *src, const char *dst, int preserve)
{
    (void) src;
    (void) dst;

    clone_syscall__call_count++;
    clone_syscall__call_arguments_are_proper = (preserve != 0);
    return -1;
}
#endif

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
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
extern GPtrArray *vfs_openfiles;
static void
prepare_files (vfs_path_t **vpath1, vfs_path_t **vpath2)
{
    unlink (test_filename1);  // remove a possible leftover from a previous run
    g_file_set_contents (test_filename1, "test", sizeof ("test") - 1, NULL);
    unlink (test_filename2);  // remove a possible leftover from a previous run

    *vpath1 = vfs_path_from_str (test_filename1);
    *vpath2 = vfs_path_from_str (test_filename2);
}

static void
cleanup_files (vfs_path_t *vpath1, vfs_path_t *vpath2)
{
    vfs_path_free (vpath1, TRUE);
    vfs_path_free (vpath2, TRUE);
    unlink (test_filename1);
    unlink (test_filename2);
}

/* @Test */
START_TEST (test_vfs_clone_file)
{
    vfs_path_t *vpath1;
    vfs_path_t *vpath2;
    int fdin;
    int fdout;

    // given
    clone_syscall__call_count = 0;
    clone_syscall__call_arguments_are_proper = FALSE;
    prepare_files (&vpath1, &vpath2);
    fdin = mc_open (vpath1, O_RDONLY | O_BINARY);
    fdout = mc_open (vpath2, O_CREAT | O_WRONLY | O_TRUNC | O_BINARY, 0600);

    // when
    vfs_clone_file (fdout, fdin);

    // then
#ifdef HAVE_FILE_CLONING_BY_RANGE
    ck_assert (clone_syscall__call_count > 0);
    ck_assert (clone_syscall__call_arguments_are_proper);
#else
    ck_assert (errno == ENOTSUP);
#endif

    // cleanup
    mc_close (fdout);
    mc_close (fdin);
    cleanup_files (vpath1, vpath2);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_vfs_clone_file_by_path)
{
    vfs_path_t *vpath1;
    vfs_path_t *vpath2;

    // given
    clone_syscall__call_count = 0;
    clone_syscall__call_arguments_are_proper = FALSE;
    prepare_files (&vpath1, &vpath2);

    // when
    vfs_clone_file_by_path (vpath1, vpath2, TRUE);

    // then
#ifdef HAVE_FILE_CLONING_BY_PATH
    ck_assert (clone_syscall__call_count > 0);
    ck_assert (clone_syscall__call_arguments_are_proper);
#else
    ck_assert (errno == ENOTSUP);
#endif

    // cleanup
    cleanup_files (vpath1, vpath2);
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
    tcase_add_test (tc_core, test_vfs_clone_file);
    tcase_add_test (tc_core, test_vfs_clone_file_by_path);
    // ***********************************

    return mctest_run_all (tc_core);
}
