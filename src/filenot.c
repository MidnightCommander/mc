/*
   filenot.c:  wrapper for routines to notify the
   tree about the changes made to the directory
   structure.

   Author:
      Janne Kukonlehto
      Miguel de Icaza

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <string.h>
#include <errno.h>
#include "global.h"

static char *
get_absolute_name (const char *file)
{
    char dir[MC_MAXPATHLEN];

    if (file[0] == PATH_SEP)
	return g_strdup (file);
    mc_get_current_wd (dir, MC_MAXPATHLEN);
    return concat_dir_and_file (dir, file);
}

static int
my_mkdir_rec (char *s, mode_t mode)
{
    char *p, *q;
    int result;

    if (!mc_mkdir (s, mode))
	return 0;
    else if (errno != ENOENT)
	return -1;

    /* FIXME: should check instead if s is at the root of that filesystem */
    if (!vfs_file_is_local (s))
	return -1;

    if (!strcmp (s, PATH_SEP_STR)) {
	errno = ENOTDIR;
	return -1;
    }

    p = concat_dir_and_file (s, "..");
    q = vfs_canon (p);
    g_free (p);

    if (!(result = my_mkdir_rec (q, mode)))
	result = mc_mkdir (s, mode);

    g_free (q);
    return result;
}

int
my_mkdir (const char *s, mode_t mode)
{
    int result;
    char *my_s;

    result = mc_mkdir (s, mode);
    if (result) {
	char *p = vfs_canon (s);

	result = my_mkdir_rec (p, mode);
	g_free (p);
    }
    if (result == 0) {
	my_s = get_absolute_name (s);

#ifdef FIXME
	tree_add_entry (tree, my_s);
#endif

	g_free (my_s);
    }
    return result;
}

int
my_rmdir (const char *s)
{
    int result;
    char *my_s;
#ifdef FIXME
    WTree *tree = 0;
#endif

    /* FIXME: Should receive a Wtree! */
    result = mc_rmdir (s);
    if (result == 0) {
	my_s = get_absolute_name (s);

#ifdef FIXME
	tree_remove_entry (tree, my_s);
#endif

	g_free (my_s);
    }
    return result;
}
