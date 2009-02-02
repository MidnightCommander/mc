/* editor file locking.

   Copyright (C) 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

   Authors: 2003 Adam Byrtek

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

*/

#include <config.h>

#include <signal.h>		/* kill() */
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>

#include <mhl/memory.h>
#include <mhl/string.h>

#include "../src/global.h"

#include "edit.h"
#include "editlock.h"

#include "../src/wtools.h"	/* edit_query_dialog () */

#define BUF_SIZE 255
#define PID_BUF_SIZE 10

struct lock_s {
    char *who;
    pid_t pid;
};

/* Locking scheme used in mcedit is based on a documentation found
   in JED editor sources. Abstract from lock.c file (by John E. Davis):

   The basic idea here is quite simple.  Whenever a buffer is attached to
   a file, and that buffer is modified, then attempt to lock the
   file. Moreover, before writing to a file for any reason, lock the
   file. The lock is really a protocol respected and not a real lock.
   The protocol is this: If in the directory of the file is a
   symbolic link with name ".#FILE", the FILE is considered to be locked
   by the process specified by the link.
*/

/* Build user@host.domain.pid string (need to be freed) */
static char *
lock_build_name (void)
{
    char host[BUF_SIZE];
    const char *user = NULL;
    struct passwd *pw;

    pw = getpwuid (getuid ());
    if (pw) user = pw->pw_name;
    if (!user) user = getenv ("USER");
    if (!user) user = getenv ("USERNAME");
    if (!user) user = getenv ("LOGNAME");
    if (!user) user = "";

    /* TODO: Use FQDN, no clean interface, so requires lot of code */
    if (gethostname (host, BUF_SIZE - 1) == -1)
	*host = '\0';

    return g_strdup_printf ("%s@%s.%d", user, host, (int) getpid ());
}

static char *
lock_build_symlink_name (const char *fname)
{
    char *fname_copy, *symlink_name;
    char absolute_fname[PATH_MAX];

    if (mc_realpath (fname, absolute_fname) == NULL)
	return NULL;

    fname = x_basename (absolute_fname);
    fname_copy = mhl_str_dup (fname);
    absolute_fname[fname - absolute_fname] = '\0';
    symlink_name = g_strconcat (absolute_fname, ".#", fname_copy, (char *) NULL);
    mhl_mem_free (fname_copy);

    return symlink_name;
}

/* Extract pid from user@host.domain.pid string */
static struct lock_s *
lock_extract_info (const char *str)
{
    int i;
    const char *p, *s;
    static char pid[PID_BUF_SIZE], who[BUF_SIZE];
    static struct lock_s lock;

    for (p = str + strlen (str) - 1; p >= str; p--)
	if (*p == '.')
	    break;

    /* Everything before last '.' is user@host */
    i = 0;
    for (s = str; s < p && i < BUF_SIZE; s++)
	who[i++] = *s;
    who[i] = '\0';

    /* Treat text between '.' and ':' or '\0' as pid */
    i = 0;
    for (p = p + 1;
	 p < str + strlen (str) && *p != ':' && i < PID_BUF_SIZE; p++)
	pid[i++] = *p;
    pid[i] = '\0';

    lock.pid = (pid_t) atol (pid);
    lock.who = who;
    return &lock;
}

/* Extract user@host.domain.pid from lock file (static string)  */
static char *
lock_get_info (const char *lockfname)
{
    int cnt;
    static char buf[BUF_SIZE];

    if ((cnt = readlink (lockfname, buf, BUF_SIZE - 1)) == -1 || !*buf)
	return NULL;
    buf[cnt] = '\0';
    return buf;
}


/* Tries to raise file lock
   Returns 1 on success,  0 on failure, -1 if abort
   Warning: Might do screen refresh and lose edit->force */
int
edit_lock_file (const char *fname)
{
    char *lockfname, *newlock, *msg, *lock;
    struct stat statbuf;
    struct lock_s *lockinfo;

    /* Just to be sure (and don't lock new file) */
    if (!fname || !*fname)
	return 0;

    /* Locking on VFS is not supported */
    if (!vfs_file_is_local (fname))
	return 0;

    /* Check if already locked */
    lockfname = lock_build_symlink_name (fname);
    if (lockfname == NULL)
	return 0;
    if (lstat (lockfname, &statbuf) == 0) {
	lock = lock_get_info (lockfname);
	if (!lock) {
	    mhl_mem_free (lockfname);
	    return 0;
	}
	lockinfo = lock_extract_info (lock);

	/* Check if locking process alive, ask user if required */
	if (!lockinfo->pid
	    || !(kill (lockinfo->pid, 0) == -1 && errno == ESRCH)) {
	    msg =
		g_strdup_printf (_
				 ("File \"%s\" is already being edited\n"
				  "User: %s\nProcess ID: %d"), x_basename (lockfname) + 2,
				 lockinfo->who, (int) lockinfo->pid);
	    /* TODO: Implement "Abort" - needs to rewind undo stack */
	    switch (edit_query_dialog2
		    (_("File locked"), msg, _("&Grab lock"),
		     _("&Ignore lock"))) {
	    case 0:
		break;
	    case 1:
	    case -1:
		mhl_mem_free (lockfname);
		mhl_mem_free (msg);
		return 0;
	    }
	    mhl_mem_free (msg);
	}
	unlink (lockfname);
    }

    /* Create lock symlink */
    newlock = lock_build_name ();
    if (symlink (newlock, lockfname) == -1) {
	mhl_mem_free (lockfname);
	mhl_mem_free (newlock);
	return 0;
    }

    mhl_mem_free (lockfname);
    mhl_mem_free (newlock);
    return 1;
}

/* Lowers file lock if possible
   Always returns 0 to make 'lock = edit_unlock_file (f)' possible */
int
edit_unlock_file (const char *fname)
{
    char *lockfname, *lock;
    struct stat statbuf;

    /* Just to be sure */
    if (!fname || !*fname)
	return 0;

    lockfname = lock_build_symlink_name (fname);
    if (lockfname == NULL)
	return 0;

    /* Check if lock exists */
    if (lstat (lockfname, &statbuf) == -1) {
	mhl_mem_free (lockfname);
	return 0;
    }

    lock = lock_get_info (lockfname);
    if (lock) {
	/* Don't touch if lock is not ours */
	if (lock_extract_info (lock)->pid != getpid ()) {
	    mhl_mem_free (lockfname);
	    return 0;
	}
    }

    /* Remove lock */
    unlink (lockfname);
    mhl_mem_free (lockfname);
    return 0;
}
