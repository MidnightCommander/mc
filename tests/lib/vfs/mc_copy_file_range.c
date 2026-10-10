/*
   lib/vfs - test mc_copy_file_range_*() functionality

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

#if defined(HAVE_FICLONERANGE)
#include <linux/fs.h>   // FICLONERANGE
#include <sys/ioctl.h>  // ioctl()
#endif

#if defined(HAVE_COPY_FILE_RANGE)
#include <unistd.h>  // copy_file_range(), COPY_FILE_RANGE_CLONE
#ifdef __linux__
#include <sys/utsname.h>  // uname()
#endif
#endif

#include "lib/strutil.h"
#include "lib/util.h"
#include "src/vfs/local/local.c"

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_FILE_CLONING_BY_RANGE

static int clone_syscall__call_count = 0;
static gboolean clone_syscall__call_arguments_are_proper = FALSE;

#ifdef HAVE_FICLONERANGE
/* @ThenReturnValue */
static int ioctl__ficlonerange__return_value = -1;
#endif

#ifdef HAVE_COPY_FILE_RANGE
/* @ThenReturnValue */
static ssize_t copy_file_range__return_value = -1;
#endif

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
#ifdef COPY_FILE_RANGE_CLONE
    clone_syscall__call_arguments_are_proper = (flags == COPY_FILE_RANGE_CLONE);
#else
    clone_syscall__call_arguments_are_proper = (flags == 0);
#endif

    return clone_syscall__call_arguments_are_proper ? copy_file_range__return_value : -1;
}

#ifdef __linux__
/* @Mock */
int
uname (struct utsname *buf)
{
    strcpy (buf->release, "5.19.0");
    return 0;
}
#endif
#endif  // HAVE_COPY_FILE_RANGE

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
    return request == FICLONERANGE ? ioctl__ficlonerange__return_value : -1;
}
#endif  // HAVE_FICLONERANGE

#endif  // HAVE_FILE_CLONING_BY_RANGE

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

#ifdef HAVE_FILE_CLONING_BY_RANGE

static void
prepare_files (vfs_path_t **vpath1, vfs_path_t **vpath2, int *src_vfs_fd, int *dst_vfs_fd,
               int *src_fd, int *dst_fd)
{
    void *src_fd_ptr = NULL;
    void *dst_fd_ptr = NULL;

    unlink (test_filename1);  // remove a possible leftover from a previous run
    g_file_set_contents (test_filename1, "test", sizeof ("test") - 1, NULL);
    unlink (test_filename2);  // remove a possible leftover from a previous run

    *vpath1 = vfs_path_from_str (test_filename1);
    *vpath2 = vfs_path_from_str (test_filename2);
    *src_vfs_fd = mc_open (*vpath1, O_RDONLY | O_BINARY);
    *dst_vfs_fd = mc_open (*vpath2, O_CREAT | O_WRONLY | O_TRUNC | O_BINARY, 0600);
    vfs_class_find_by_handle (*src_vfs_fd, &src_fd_ptr);
    vfs_class_find_by_handle (*dst_vfs_fd, &dst_fd_ptr);
    *src_fd = *(int *) src_fd_ptr;
    *dst_fd = *(int *) dst_fd_ptr;
}

static void
cleanup_files (vfs_path_t *vpath1, vfs_path_t *vpath2, int src_vfs_fd, int dst_vfs_fd)
{
    mc_close (src_vfs_fd);
    mc_close (dst_vfs_fd);
    vfs_path_free (vpath1, TRUE);
    vfs_path_free (vpath2, TRUE);
    unlink (test_filename1);
    unlink (test_filename2);
}

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_FICLONERANGE
/* @Test */
START_TEST (test_mc_copy_file_range_ficlonerange)
{
    vfs_path_t *vpath1;
    vfs_path_t *vpath2;
    int src_vfs_fd;
    int dst_vfs_fd;
    int src_fd;
    int dst_fd;
    off_t in_offset = 0;
    off_t out_offset = 0;

    // given
    clone_syscall__call_count = 0;
    clone_syscall__call_arguments_are_proper = FALSE;
    prepare_files (&vpath1, &vpath2, &src_vfs_fd, &dst_vfs_fd, &src_fd, &dst_fd);

    // when
    mc_copy_file_range_ficlonerange (src_fd, &in_offset, dst_fd, &out_offset, SSIZE_MAX);

    // then
    ck_assert (clone_syscall__call_count > 0);
    ck_assert (clone_syscall__call_arguments_are_proper);

    // cleanup
    cleanup_files (vpath1, vpath2, src_vfs_fd, dst_vfs_fd);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_mc_copy_file_range_ficlonerange_success)
{
    vfs_path_t *vpath1;
    vfs_path_t *vpath2;
    int src_vfs_fd;
    int dst_vfs_fd;
    int src_fd;
    int dst_fd;
    off_t in_offset = 0;
    off_t out_offset = 0;
    ssize_t result;

    // given
    clone_syscall__call_count = 0;
    ioctl__ficlonerange__return_value = 0;
    prepare_files (&vpath1, &vpath2, &src_vfs_fd, &dst_vfs_fd, &src_fd, &dst_fd);

    // when
    result = mc_copy_file_range_ficlonerange (src_fd, &in_offset, dst_fd, &out_offset, SSIZE_MAX);

    // then: a successful FICLONERANGE is a successful clone, whatever copy_file_range() would do
    ck_assert_int_eq (result, sizeof ("test") - 1);

    // cleanup
    ioctl__ficlonerange__return_value = -1;
    cleanup_files (vpath1, vpath2, src_vfs_fd, dst_vfs_fd);
}
END_TEST
#endif

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_COPY_FILE_RANGE
/* @Test */
START_TEST (test_mc_copy_file_range_native)
{
    vfs_path_t *vpath1;
    vfs_path_t *vpath2;
    int src_vfs_fd;
    int dst_vfs_fd;
    int src_fd;
    int dst_fd;
    off_t in_offset = 0;
    off_t out_offset = 0;

    // given
    clone_syscall__call_count = 0;
    clone_syscall__call_arguments_are_proper = FALSE;
    prepare_files (&vpath1, &vpath2, &src_vfs_fd, &dst_vfs_fd, &src_fd, &dst_fd);

    // when
    mc_copy_file_range_native (src_fd, &in_offset, dst_fd, &out_offset, SSIZE_MAX);

    // then
    ck_assert (clone_syscall__call_count > 0);
    ck_assert (clone_syscall__call_arguments_are_proper);

    // cleanup
    cleanup_files (vpath1, vpath2, src_vfs_fd, dst_vfs_fd);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_mc_copy_file_range_native_success)
{
    vfs_path_t *vpath1;
    vfs_path_t *vpath2;
    int src_vfs_fd;
    int dst_vfs_fd;
    int src_fd;
    int dst_fd;
    off_t in_offset = 0;
    off_t out_offset = 0;
    ssize_t result;

    // given
    clone_syscall__call_count = 0;
    copy_file_range__return_value = sizeof ("test") - 1;
    prepare_files (&vpath1, &vpath2, &src_vfs_fd, &dst_vfs_fd, &src_fd, &dst_fd);

    // when
    result = mc_copy_file_range_native (src_fd, &in_offset, dst_fd, &out_offset, SSIZE_MAX);

    // then: a successful mc_copy_file_range_native() is a successful clone
    ck_assert_int_eq (result, sizeof ("test") - 1);

    // cleanup
    copy_file_range__return_value = -1;
    cleanup_files (vpath1, vpath2, src_vfs_fd, dst_vfs_fd);
}
END_TEST
#endif

#endif  // HAVE_FILE_CLONING_BY_RANGE

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
#ifdef HAVE_FILE_CLONING_BY_RANGE
#ifdef HAVE_FICLONERANGE
    tcase_add_test (tc_core, test_mc_copy_file_range_ficlonerange);
    tcase_add_test (tc_core, test_mc_copy_file_range_ficlonerange_success);
#endif
#if defined(HAVE_COPY_FILE_RANGE)
    tcase_add_test (tc_core, test_mc_copy_file_range_native);
    tcase_add_test (tc_core, test_mc_copy_file_range_native_success);
#endif
#endif
    // ***********************************

    return mctest_run_all (tc_core);
}
