/* Virtual File System garbage collection code
   Copyright (C) 1995-2003 The Free Software Foundation

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>		/* For atol() */
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>		/* is_digit() */

#include "utilvfs.h"
#include "gc.h"
#include "vfs.h"

#include "../src/panel.h"	/* get_current_panel() */
#include "../src/layout.h"	/* get_current_type() */


int vfs_timeout = 60;		/* VFS timeout in seconds */

static struct vfs_stamping *stamps;


static void
vfs_addstamp (struct vfs_class *v, vfsid id, struct vfs_stamping *parent)
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
	if (parent) {
	    struct vfs_stamping *st = stamp;
	    while (parent) {
		st->parent = g_new (struct vfs_stamping, 1);
		*st->parent = *parent;
		parent = parent->parent;
		st = st->parent;
	    }
	    st->parent = 0;
	} else
	    stamp->parent = 0;

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
	    if (stamp->parent != NULL)
		vfs_stamp (stamp->parent->v, stamp->parent->id);

	    return;
	}
}


static void
vfs_rm_parents (struct vfs_stamping *stamp)
{
    struct vfs_stamping *parent;

    while (stamp) {
	parent = stamp->parent;
	g_free (stamp);
	stamp = parent;
    }
}


void
vfs_rmstamp (struct vfs_class *v, vfsid id, int removeparents)
{
    struct vfs_stamping *stamp, *st1;

    for (stamp = stamps, st1 = NULL; stamp != NULL;
	 st1 = stamp, stamp = stamp->next)
	if (stamp->v == v && stamp->id == id) {
	    if (stamp->parent != NULL) {
		if (removeparents)
		    vfs_rmstamp (stamp->parent->v, stamp->parent->id, 1);
		vfs_rm_parents (stamp->parent);
		stamp->parent = NULL;
		continue;	/* rescan the tree */
	    }
	    if (st1 == NULL) {
		stamps = stamp->next;
	    } else {
		st1->next = stamp->next;
	    }
	    g_free (stamp);

	    return;
	}
}


/* Wrapper around getid methods */
vfsid
vfs_getid (struct vfs_class *vclass, const char *path,
	   struct vfs_stamping **parent)
{
    vfsid id = NULL;

    *parent = NULL;
    if (vclass->getid)
	id = (*vclass->getid) (vclass, path, parent);

    return id;
}


/* Same as vfs_getid, but append slash to the path if needed */
vfsid
vfs_ncs_getid (struct vfs_class *nvfs, const char *dir,
	       struct vfs_stamping **par)
{
    vfsid nvfsid;
    char *dir1;

    dir1 = concat_dir_and_file (dir, "");
    nvfsid = vfs_getid (nvfs, dir1, par);
    g_free (dir1);
    return nvfsid;
}


static int
is_parent (struct vfs_class *nvfs, vfsid nvfsid,
	   struct vfs_stamping *parent)
{
    struct vfs_stamping *stamp;

    for (stamp = parent; stamp; stamp = stamp->parent)
	if (stamp->v == nvfs && stamp->id == nvfsid)
	    break;

    return (stamp ? 1 : 0);
}


static void
vfs_stamp_path (char *path)
{
    struct vfs_class *vfs;
    vfsid id;
    struct vfs_stamping *par, *stamp;

    vfs = vfs_get_class (path);
    id = vfs_ncs_getid (vfs, path, &par);
    vfs_addstamp (vfs, id, par);

    for (stamp = par; stamp != NULL; stamp = stamp->parent)
	vfs_addstamp (stamp->v, stamp->id, stamp->parent);
    vfs_rm_parents (par);
}


static void
_vfs_add_noncurrent_stamps (struct vfs_class *oldvfs, vfsid oldvfsid,
			    struct vfs_stamping *parent)
{
    struct vfs_class *nvfs, *n2vfs, *n3vfs;
    vfsid nvfsid, n2vfsid, n3vfsid;
    struct vfs_stamping *par, *stamp;
    int f;

    /* FIXME: As soon as we convert to multiple panels, this stuff
       has to change. It works like this: We do not time out the
       vfs's which are current in any panel and on the other
       side we add the old directory with all its parents which
       are not in any panel (if we find such one, we stop adding
       parents to the time-outing structure. */

    /* There are three directories we have to take care of: current_dir,
       current_panel->cwd and other_panel->cwd. Athough most of the time either
       current_dir and current_panel->cwd or current_dir and other_panel->cwd are the
       same, it's possible that all three are different -- Norbert */

    if (!current_panel)
	return;

    nvfs = vfs_get_class (vfs_get_current_dir ());
    nvfsid = vfs_ncs_getid (nvfs, vfs_get_current_dir (), &par);
    vfs_rmstamp (nvfs, nvfsid, 1);

    f = is_parent (oldvfs, oldvfsid, par);
    vfs_rm_parents (par);
    if ((nvfs == oldvfs && nvfsid == oldvfsid) || oldvfsid == NULL
	|| f) {
	return;
    }

    if (get_current_type () == view_listing) {
	n2vfs = vfs_get_class (current_panel->cwd);
	n2vfsid = vfs_ncs_getid (n2vfs, current_panel->cwd, &par);
	f = is_parent (oldvfs, oldvfsid, par);
	vfs_rm_parents (par);
	if ((n2vfs == oldvfs && n2vfsid == oldvfsid) || f)
	    return;
    } else {
	n2vfs = NULL;
	n2vfsid = NULL;
    }

    if (get_other_type () == view_listing) {
	n3vfs = vfs_get_class (other_panel->cwd);
	n3vfsid = vfs_ncs_getid (n3vfs, other_panel->cwd, &par);
	f = is_parent (oldvfs, oldvfsid, par);
	vfs_rm_parents (par);
	if ((n3vfs == oldvfs && n3vfsid == oldvfsid) || f)
	    return;
    } else {
	n3vfs = NULL;
	n3vfsid = NULL;
    }

    if (!oldvfs->nothingisopen || !(*oldvfs->nothingisopen) (oldvfsid))
	return;

    vfs_addstamp (oldvfs, oldvfsid, parent);
    for (stamp = parent; stamp != NULL; stamp = stamp->parent) {
	if ((stamp->v == nvfs && stamp->id == nvfsid)
	    || (stamp->v == n2vfs && stamp->id == n2vfsid)
	    || (stamp->v == n3vfs && stamp->id == n3vfsid)
	    || !stamp->id || !stamp->v->nothingisopen
	    || !(*stamp->v->nothingisopen) (stamp->id))
	    break;
	vfs_addstamp (stamp->v, stamp->id, stamp->parent);
    }
}


void
vfs_add_noncurrent_stamps (struct vfs_class *oldvfs, vfsid oldvfsid,
			   struct vfs_stamping *parent)
{
    _vfs_add_noncurrent_stamps (oldvfs, oldvfsid, parent);
    vfs_rm_parents (parent);
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
	    vfs_rmstamp (stamp->v, stamp->id, 0);
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
    struct vfs_stamping *parent;

    oldvfs = vfs_get_class (dir);
    oldvfsid = vfs_ncs_getid (oldvfs, dir, &parent);
    vfs_add_noncurrent_stamps (oldvfs, oldvfsid, parent);
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
	vfs_rmstamp (stamps->v, stamps->id, 1);
}
