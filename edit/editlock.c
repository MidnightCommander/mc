/* editor file locking.

   Copyright (C) 2003 the Free Software Foundation

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

*/

#include <config.h>
#include "edit.h"
#include "editlock.h"

#include "src/wtools.h"		/* edit_query_dialog () */

#define BUF_SIZE 255
#define PID_BUF_SIZE 10

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
lock_build_name (char *fname)
{
    char host[BUF_SIZE], *user;

    if (!
	((user = getpwuid (getuid ())->pw_name) || (user = getenv ("USER"))
	 || (user = getenv ("USERNAME")) || (user = getenv ("LOGNAME"))))
	user = "";

    /* TODO: Use FQDN, no clean interface, so requires lot of code */
    if (gethostname (host, BUF_SIZE - 1) == -1)
	*host = '\0';

    return g_strdup_printf ("%s@%s.%d", user, host, getpid ());
}

/* Extract pid from user@host.domain.pid string */
static pid_t
lock_extract_pid (char *str)
{
    int i;
    char *p, pid[PID_BUF_SIZE];

    /* Treat text between '.' and ':' or '\0' as pid */
    for (p = str + strlen (str) - 1; p >= str; p--)
	if (*p == '.')
	    break;

    i = 0;
    for (p = p + 1;
	 p < str + strlen (str) && *p != ':' && i < PID_BUF_SIZE; p++)
	pid[i++] = *p;
    pid[i] = '\0';

    return (pid_t) atol (pid);
}

/* Extract user@host.domain.pid from lock file (static string)  */
static char *
lock_get_info (char *lockfname)
{
    int cnt;
    static char buf[BUF_SIZE];

    if ((cnt = readlink (lockfname, buf, BUF_SIZE - 1)) == -1 || !buf
	|| !*buf)
	return NULL;
    buf[cnt] = '\0';
    return buf;
}


/* Tries to raise file lock
   Returns 1 on success,  0 on failure, -1 if abort */
int
edit_lock_file (char *fname)
{
    char *lockfname, *newlock, *msg, *lock;
    struct stat statbuf;
    pid_t pid;

    /* Just to be sure (and don't lock new file) */
    if (!fname || !*fname)
	return 0;

    /* Locking on VFS is not supported */
    if (!vfs_file_is_local (fname))
	return 0;

    /* Check if already locked */
    lockfname = g_strconcat (".#", fname, NULL);
    if (lstat (lockfname, &statbuf) == 0) {
	lock = lock_get_info (lockfname);
	if (!lock) {
	    g_free (lockfname);
	    return 0;
	}
	pid = lock_extract_pid (lock);

	/* Check if locking process alive, ask user if required */
	if (!pid || !(kill (pid, 0) == -1 && errno == ESRCH)) {
	    msg =
		g_strdup_printf (_("File %s is locked by lock %s"), fname,
				 lock);
	    /* TODO: Implement "Abort" - needs to rewind undo stack */
	    switch (edit_query_dialog2
		    (_("File locked"), msg, _("&Grab lock"),
		     _("&Ignore lock"))) {
	    case 0:
		break;
	    case 1:
	    case -1:
		g_free (lockfname);
		g_free (msg);
		return 0;
	    }
	    g_free (msg);
	}
	unlink (lockfname);
    }

    /* Create lock symlink */
    newlock = lock_build_name (fname);
    if (symlink (newlock, lockfname) == -1) {
	g_free (lockfname);
	g_free (newlock);
	return 0;
    }

    g_free (lockfname);
    g_free (newlock);
    return 1;
}

/* Lowers file lock if possible 
   Always returns 0 to make 'lock = edit_unlock_file (f)' possible */
int
edit_unlock_file (char *fname)
{
    char *lockfname, *lock;
    struct stat statbuf;

    /* Just to be sure */
    if (!fname || !*fname)
	return 0;

    lockfname = g_strconcat (".#", fname, NULL);

    /* Check if lock exists */
    if (lstat (lockfname, &statbuf) == -1) {
	g_free (lockfname);
	return 0;
    }

    lock = lock_get_info (lockfname);
    if (lock) {
	/* Don't touch if lock is not ours */
	if (lock_extract_pid (lock) != getpid ()) {
	    g_free (lockfname);
	    return 0;
	}
    }

    /* Remove lock */
    unlink (lockfname);
    g_free (lockfname);
    return 0;
}
