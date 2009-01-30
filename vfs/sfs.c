/*
 * Single File fileSystem
 *
 * Copyright 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
 * Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * This defines whole class of filesystems which contain single file
 * inside. It is somehow similar to extfs, except that extfs makes
 * whole virtual trees and we do only single virtual files. 
 *
 * If you want to gunzip something, you should open it with #ugz
 * suffix, DON'T try to gunzip it yourself.
 *
 * Namespace: exports vfs_sfs_ops */

#include <config.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <mhl/memory.h>

#include "../src/global.h"
#include "../src/tty.h"		/* enable/disable interrupt key */
#include "../src/wtools.h"	/* message() */
#include "../src/main.h"	/* print_vfs_message */
#include "utilvfs.h"
#include "vfs.h"
#include "vfs-impl.h"
#include "gc.h"		/* vfs_stamp_create */
#include "local.h"
#include "../src/execute.h"	/* EXECUTE_AS_SHELL */

struct cachedfile {
    char *name, *cache;
    struct cachedfile *next;
};

static struct cachedfile *head;
static struct vfs_class vfs_sfs_ops;

#define MAXFS 32
static int sfs_no = 0;
static char *sfs_prefix[ MAXFS ];
static char *sfs_command[ MAXFS ];
static int sfs_flags[ MAXFS ];
#define F_1 1
#define F_2 2
#define F_NOLOCALCOPY 4
#define F_FULLMATCH 8

static int
sfs_vfmake (struct vfs_class *me, const char *name, char *cache)
{
    char *inpath, *op;
    int w;
    char pad[10240];
    char *s, *t = pad;
    int was_percent = 0;
    char *pname;	/* name of parent archive */
    char *pqname;	/* name of parent archive, quoted */

    pname = g_strdup (name);
    vfs_split (pname, &inpath, &op);
    if ((w = (*me->which) (me, op)) == -1)
	vfs_die ("This cannot happen... Hopefully.\n");

    if (!(sfs_flags[w] & F_1) && strcmp (pname, "/")) {
	mhl_mem_free (pname);
	return -1;
    }

    /*    if ((sfs_flags[w] & F_2) || (!inpath) || (!*inpath)); else return -1; */
    if (!(sfs_flags[w] & F_NOLOCALCOPY)) {
	s = mc_getlocalcopy (pname);
	if (!s) {
	    mhl_mem_free (pname);
	    return -1;
	}
	pqname = name_quote (s, 0);
	mhl_mem_free (s);
    } else {
	pqname = name_quote (pname, 0);
    }
    mhl_mem_free (pname);

#define COPY_CHAR \
    if ((size_t) (t-pad) > sizeof(pad)) { \
	mhl_mem_free (pqname); \
	return -1; \
    } \
    else \
	*t++ = *s;

#define COPY_STRING(a) \
    if ((t-pad)+strlen(a)>sizeof(pad)) { \
	mhl_mem_free (pqname); \
	return -1; \
    } else { \
	strcpy (t, a); \
	t+= strlen(a); \
    }

    for (s = sfs_command[w]; *s; s++) {
	if (was_percent) {

	    const char *ptr = NULL;
	    was_percent = 0;

	    switch (*s) {
	    case '1':
		ptr = pqname;
		break;
	    case '2':
		ptr = op + strlen (sfs_prefix[w]);
		break;
	    case '3':
		ptr = cache;
		break;
	    case '%':
		COPY_CHAR;
		continue;
	    }
	    COPY_STRING (ptr);
	} else {
	    if (*s == '%')
		was_percent = 1;
	    else
		COPY_CHAR;
	}
    }

    mhl_mem_free (pqname);
    open_error_pipe ();
    if (my_system (EXECUTE_AS_SHELL, "/bin/sh", pad)) {
	close_error_pipe (D_ERROR, NULL);
	return -1;
    }

    close_error_pipe (D_NORMAL, NULL);
    return 0;			/* OK */
}

static const char *
sfs_redirect (struct vfs_class *me, const char *name)
{
    struct cachedfile *cur = head;
    char *cache;
    int handle;

    while (cur) {
	if (!strcmp (name, cur->name)) {
	    vfs_stamp (&vfs_sfs_ops, cur);
	    return cur->cache;
	}
	cur = cur->next;
    }

    handle = vfs_mkstemps (&cache, "sfs", name);

    if (handle == -1) {
	return "/SOMEONE_PLAYING_DIRTY_TMP_TRICKS_ON_US";
    }

    close (handle);

    if (!sfs_vfmake (me, name, cache)) {
	cur = g_new (struct cachedfile, 1);
	cur->name = g_strdup (name);
	cur->cache = cache;
	cur->next = head;
	head = cur;

	vfs_stamp_create (&vfs_sfs_ops, head);

	return cache;
    }

    unlink (cache);
    mhl_mem_free (cache);
    return "/I_MUST_NOT_EXIST";
}

static void *
sfs_open (struct vfs_class *me, const char *path, int flags, int mode)
{
    int *sfs_info;
    int fd;

    path = sfs_redirect (me, path);
    fd = open (path, NO_LINEAR (flags), mode);
    if (fd == -1)
	return 0;

    sfs_info = g_new (int, 1);
    *sfs_info = fd;

    return sfs_info;
}

static int sfs_stat (struct vfs_class *me, const char *path, struct stat *buf)
{
    path = sfs_redirect (me, path);
    return stat (path, buf);
}

static int sfs_lstat (struct vfs_class *me, const char *path, struct stat *buf)
{
    path = sfs_redirect (me, path);
#ifndef HAVE_STATLSTAT
    return lstat (path, buf);
#else
    return statlstat (path, buf);
#endif
}

static int sfs_chmod (struct vfs_class *me, const char *path, int mode)
{
    path = sfs_redirect (me, path);
    return chmod (path, mode);
}

static int sfs_chown (struct vfs_class *me, const char *path, int owner, int group)
{
    path = sfs_redirect (me, path);
    return chown (path, owner, group);
}

static int sfs_utime (struct vfs_class *me, const char *path, struct utimbuf *times)
{
    path = sfs_redirect (me, path);
    return utime (path, times);
}

static int
sfs_readlink (struct vfs_class *me, const char *path, char *buf, size_t size)
{
    path = sfs_redirect (me, path);
    return readlink (path, buf, size);
}

static vfsid
sfs_getid (struct vfs_class *me, const char *path)
{
    struct cachedfile *cur = head;

    (void) me;

    while (cur) {
	if (!strcmp (path, cur->name))
	    break;
	cur = cur->next;
    }

    return (vfsid) cur;
}

static void sfs_free (vfsid id)
{
    struct cachedfile *which = (struct cachedfile *) id;
    struct cachedfile *cur, *prev;

    for (cur = head, prev = 0; cur && cur != which; prev = cur, cur = cur->next)
	;
    if (!cur)
    	vfs_die( "Free of thing which is unknown to me\n" );
    unlink (cur->cache);

    if (prev)
	prev->next = cur->next;
    else
	head = cur->next;

    mhl_mem_free (cur->cache);
    mhl_mem_free (cur->name);
    mhl_mem_free (cur);
}

static void sfs_fill_names (struct vfs_class *me, fill_names_f func)
{
    struct cachedfile *cur = head;

    (void) me;

    while (cur){
	(*func)(cur->name);
	cur = cur->next;
    }
}

static int sfs_nothingisopen (vfsid id)
{
    /* FIXME: Investigate whether have to guard this like in
       the other VFSs (see fd_usage in extfs) -- Norbert */
    (void) id;
    return 1;
}

static char *
sfs_getlocalcopy (struct vfs_class *me, const char *path)
{
    path = sfs_redirect (me, path);
    return g_strdup (path);
}

static int
sfs_ungetlocalcopy (struct vfs_class *me, const char *path,
		    const char *local, int has_changed)
{
    (void) me;
    (void) path;
    (void) local;
    (void) has_changed;
    return 0;
}

static int sfs_init (struct vfs_class *me)
{
    char *mc_sfsini;
    FILE *cfg;
    char key[256];

    (void) me;

    mc_sfsini = concat_dir_and_file (mc_home, "extfs" PATH_SEP_STR "sfs.ini");
    cfg = fopen (mc_sfsini, "r");

    if (!cfg){
	fprintf (stderr, _("Warning: file %s not found\n"), mc_sfsini);
	mhl_mem_free (mc_sfsini);
	return 0;
    }
    mhl_mem_free (mc_sfsini);

    sfs_no = 0;
    while (sfs_no < MAXFS && fgets (key, sizeof (key), cfg)) {
	char *c, *semi = NULL, flags = 0;

	if (*key == '#' || *key == '\n')
	    continue;

	for (c = key; *c; c++)
	    if ((*c == ':') || (*c == '/')){
		semi = c;
		if (*c == '/'){
		    *c = 0;
		    flags |= F_FULLMATCH;
		}
		break;
	    }

	if (!semi){
	invalid_line:
	    fprintf (stderr, _("Warning: Invalid line in %s:\n%s\n"),
		     "sfs.ini", key);
	    continue;
	}

	c = semi + 1;
	while (*c && (*c != ' ') && (*c != '\t')) {
	    switch (*c) {
	    case '1': flags |= F_1; break;
	    case '2': flags |= F_2; break;
	    case 'R': flags |= F_NOLOCALCOPY; break;
	    default:
		fprintf (stderr, _("Warning: Invalid flag %c in %s:\n%s\n"),
			 *c, "sfs.ini", key);
	    }	    
	    c++;
	}
	if (!*c)
	    goto invalid_line;
	c++;
	*(semi+1) = 0;
	if ((semi = strchr (c, '\n')))
	    *semi = 0;

	sfs_prefix [sfs_no] = g_strdup (key);
	sfs_command [sfs_no] = g_strdup (c);
	sfs_flags [sfs_no] = flags;
	sfs_no++;
    }
    fclose (cfg);
    return 1;
}

static void
sfs_done (struct vfs_class *me)
{
    int i;

    (void) me;

    for (i = 0; i < sfs_no; i++){
        mhl_mem_free (sfs_prefix [i]);
	mhl_mem_free (sfs_command [i]);
	sfs_prefix [i] = sfs_command [i] = NULL;
    }
    sfs_no = 0;
}

static int
sfs_which (struct vfs_class *me, const char *path)
{
    int i;

    (void) me;

    for (i = 0; i < sfs_no; i++)
        if (sfs_flags [i] & F_FULLMATCH) {
	    if (!strcmp (path, sfs_prefix [i]))
	        return i;
	} else
	    if (!strncmp (path, sfs_prefix [i], strlen (sfs_prefix [i])))
	        return i;

    return -1;
}

void
init_sfs (void)
{
    vfs_sfs_ops.name = "sfs";
    vfs_sfs_ops.init = sfs_init;
    vfs_sfs_ops.done = sfs_done;
    vfs_sfs_ops.fill_names = sfs_fill_names;
    vfs_sfs_ops.which = sfs_which;
    vfs_sfs_ops.open = sfs_open;
    vfs_sfs_ops.close = local_close;
    vfs_sfs_ops.read = local_read;
    vfs_sfs_ops.stat = sfs_stat;
    vfs_sfs_ops.lstat = sfs_lstat;
    vfs_sfs_ops.fstat = local_fstat;
    vfs_sfs_ops.chmod = sfs_chmod;
    vfs_sfs_ops.chown = sfs_chown;
    vfs_sfs_ops.utime = sfs_utime;
    vfs_sfs_ops.readlink = sfs_readlink;
    vfs_sfs_ops.ferrno = local_errno;
    vfs_sfs_ops.lseek = local_lseek;
    vfs_sfs_ops.getid = sfs_getid;
    vfs_sfs_ops.nothingisopen = sfs_nothingisopen;
    vfs_sfs_ops.free = sfs_free;
    vfs_sfs_ops.getlocalcopy = sfs_getlocalcopy;
    vfs_sfs_ops.ungetlocalcopy = sfs_ungetlocalcopy;
    vfs_register_class (&vfs_sfs_ops);
}
