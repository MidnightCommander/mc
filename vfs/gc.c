/* Virtual File System garbage collection code
   Copyright (C) 2003, 2004, 2005, 2007 Free Software Foundation, Inc.

   Written by: 1995 Miguel de Icaza
               1995 Jakub Jelinek
	       1998 Pavel Machek
	       2003 Pavel Roskin

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>		/* For atol() */
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>		/* is_digit() */

#include "../src/global.h"
#include "../src/tty.h"		/* enable/disable interrupt key */
#include "../src/wtools.h"	/* message() */
#include "../src/main.h"	/* print_vfs_message */
#include "utilvfs.h"
#include "vfs-impl.h"
#include "gc.h"
#include "vfs.h"

#include "../src/panel.h"	/* get_current_panel() */
#include "../src/layout.h"	/* get_current_type() */


int vfs_timeout = 60;		/* VFS timeout in seconds */

static struct vfs_stamping *stamps;


static void
vfs_addstamp (struct vfs_class *v, vfsid id)
{
    if (!(v->flags & VFSF_LOCAL) && id != NULL) {
	struct vfs_stamping *stamp;
	struct vfs_stamping *last_stamp = NULL;

	for (stamp = stamps; stamp != NULL; stamp = stamp->next) {
	    if (stamp->v == v && stamp->id == id) {
		gettimeofday (&(stamp->time), NULL);
		return;
	    }
	    last_stamp = stamp;
	}
	stamp = g_new (struct vfs_stamping, 1);
	stamp->v = v;
	stamp->id = id;

	gettimeofday (&(stamp->time), NULL);
	stamp->next = 0;

	if (stamps) {
	    /* Add to the end */
	    last_stamp->next = stamp;
	} else {
	    /* Add first element */
	    stamps = stamp;
	}
    }
}


void
vfs_stamp (struct vfs_class *v, vfsid id)
{
    struct vfs_stamping *stamp;

    for (stamp = stamps; stamp != NULL; stamp = stamp->next)
	if (stamp->v == v && stamp->id == id) {
	    gettimeofday (&(stamp->time), NULL);
	    return;
	}
}


void
vfs_rmstamp (struct vfs_class *v, vfsid id)
{
    struct vfs_stamping *stamp, *st1;

    for (stamp = stamps, st1 = NULL; stamp != NULL;
	 st1 = stamp, stamp = stamp->next)
	if (stamp->v == v && stamp->id == id) {
	    if (st1 == NULL) {
		stamps = stamp->next;
	    } else {
		st1->next = stamp->next;
	    }
	    g_free (stamp);

	    return;
	}
}


/* Find VFS id for given directory name */
vfsid
vfs_getid (struct vfs_class *vclass, const char *dir)
{
    char *dir1;
    vfsid id = NULL;

    /* append slash if needed */
    dir1 = concat_dir_and_file (dir, "");
    if (vclass->getid)
	id = (*vclass->getid) (vclass, dir1);

    g_free (dir1);
    return id;
}


static void
vfs_stamp_path (char *path)
{
    struct vfs_class *vfs;
    vfsid id;

    vfs = vfs_get_class (path);
    id = vfs_getid (vfs, path);
    vfs_addstamp (vfs, id);
}


/*
 * Create a new timestamp item by VFS class and VFS id.
 */
void
vfs_stamp_create (struct vfs_class *oldvfs, vfsid oldvfsid)
{
    struct vfs_class *nvfs, *n2vfs, *n3vfs;
    vfsid nvfsid, n2vfsid, n3vfsid;

    /* There are three directories we have to take care of: current_dir,
       current_panel->cwd and other_panel->cwd. Athough most of the time either
       current_dir and current_panel->cwd or current_dir and other_panel->cwd are the
       same, it's possible that all three are different -- Norbert */

    if (!current_panel)
	return;

    nvfs = vfs_get_class (vfs_get_current_dir ());
    nvfsid = vfs_getid (nvfs, vfs_get_current_dir ());
    vfs_rmstamp (nvfs, nvfsid);

    if ((nvfs == oldvfs && nvfsid == oldvfsid) || oldvfsid == NULL) {
	return;
    }

    if (get_current_type () == view_listing) {
	n2vfs = vfs_get_class (current_panel->cwd);
	n2vfsid = vfs_getid (n2vfs, current_panel->cwd);
	if (n2vfs == oldvfs && n2vfsid == oldvfsid)
	    return;
    } else {
	n2vfs = NULL;
	n2vfsid = NULL;
    }

    if (get_other_type () == view_listing) {
	n3vfs = vfs_get_class (other_panel->cwd);
	n3vfsid = vfs_getid (n3vfs, other_panel->cwd);
	if (n3vfs == oldvfs && n3vfsid == oldvfsid)
	    return;
    } else {
	n3vfs = NULL;
	n3vfsid = NULL;
    }

    if (!oldvfs->nothingisopen || !(*oldvfs->nothingisopen) (oldvfsid))
	return;

    vfs_addstamp (oldvfs, oldvfsid);
}


void
vfs_add_current_stamps (void)
{
    vfs_stamp_path (vfs_get_current_dir ());

    if (current_panel) {
	if (get_current_type () == view_listing)
	    vfs_stamp_path (current_panel->cwd);
    }

    if (other_panel) {
	if (get_other_type () == view_listing)
	    vfs_stamp_path (other_panel->cwd);
    }
}


/* Compare two timeval structures.  Return 0 is t1 is less than t2. */
static inline int
timeoutcmp (struct timeval *t1, struct timeval *t2)
{
    return ((t1->tv_sec < t2->tv_sec)
	    || ((t1->tv_sec == t2->tv_sec)
		&& (t1->tv_usec <= t2->tv_usec)));
}


/* This is called from timeout handler with now = 0, or can be called
   with now = 1 to force freeing all filesystems that are not in use */
void
vfs_expire (int now)
{
    static int locked = 0;
    struct timeval time;
    struct vfs_stamping *stamp, *st;

    /* Avoid recursive invocation, e.g. when one of the free functions
       calls message */
    if (locked)
	return;
    locked = 1;

    gettimeofday (&time, NULL);
    time.tv_sec -= vfs_timeout;

    for (stamp = stamps; stamp != NULL;) {
	if (now || (timeoutcmp (&stamp->time, &time))) {
	    st = stamp->next;
	    if (stamp->v->free)
		(*stamp->v->free) (stamp->id);
	    vfs_rmstamp (stamp->v, stamp->id);
	    stamp = st;
	} else
	    stamp = stamp->next;
    }
    locked = 0;
}


/*
 * Return the number of seconds remaining to the vfs timeout.
 * FIXME: The code should be improved to actually return the number of
 * seconds until the next item times out.
 */
int
vfs_timeouts ()
{
    return stamps ? 10 : 0;
}


void
vfs_timeout_handler (void)
{
    vfs_expire (0);
}


void
vfs_release_path (const char *dir)
{
    struct vfs_class *oldvfs;
    vfsid oldvfsid;

    oldvfs = vfs_get_class (dir);
    oldvfsid = vfs_getid (oldvfs, dir);
    vfs_stamp_create (oldvfs, oldvfsid);
}


/* Free all data */
void
vfs_gc_done (void)
{
    struct vfs_stamping *stamp, *st;

    for (stamp = stamps, stamps = 0; stamp != NULL;) {
	if (stamp->v->free)
	    (*stamp->v->free) (stamp->id);
	st = stamp->next;
	g_free (stamp);
	stamp = st;
    }

    if (stamps)
	vfs_rmstamp (stamps->v, stamps->id);
}
