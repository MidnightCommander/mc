/* Midnight Commander Tk Information display
   Copyright (C) 1995 Miguel de Icaza
   
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"
#include "dlg.h"
#include "widget.h"
#include "info.h"
#include "win.h"
#include "tkmain.h"

/* The following three include files are needed for cpanel */
#include "main.h"
#include "dir.h"
#include "panel.h"

/* Create the Tk widget */
void
x_create_info (Dlg_head *h, widget_data parent, WInfo *info)
{
    char *cmd;
    widget_data container = info->widget.wcontainer;

    cmd = tk_new_command  (container, info, 0, 'o');
    tk_evalf ("newinfo %s %s " VERSION, (char *)container,
	      wtk_win (info->widget));
}

/* Updates the information on the Tk widgets */
void
x_show_info (WInfo *info, struct my_statfs *s, struct stat *b)
{
    char bsize [17];
    char avail_buf [17], total_buf [17];
    char *mod, *acc, *cre;
    char *fname;
    int  have_space, have_nodes;
    int  space_percent;
    int  ispace_percent;

    fname = cpanel->dir.list [cpanel->selected].fname;
    sprint_bytesize (bsize, b->st_size, 0);
    mod = strdup (file_date (b->st_mtime));
    acc = strdup (file_date (b->st_atime));
    cre = strdup (file_date (b->st_ctime));

    /* Do we have information on space/inodes? */
    have_space = s->avail > 0 || s->total > 0;
    have_nodes = s->nfree > 0 || s->nodes > 0;

    /* Compute file system usage */
    sprint_bytesize (avail_buf, s->avail, 1);
    sprint_bytesize (total_buf, s->total, 1);
    space_percent = s->total
	? 100 * s->avail / s->total : 0;

    /* inode percentage use */
    ispace_percent = s->total ? 100 * s->nfree / s->nodes : 0;
    
    tk_evalf ("info_update %s.b {%s} %X %X " /* window fname dev ino */
	      "{%s} %o "	        /* mode mode_oct */
	      "%d %s %s "               /* links owner group */
#ifdef HAVE_ST_BLOCKS
#define BLOCKS b->st_blocks
	      "1 %d "	                /* have_blocks blocks */
#else
#define BLOCKS 0
	      "0 %d "                   /* have_blocks blocks */
#endif
              "{%s} "                     /* size */
#ifdef HAVE_RDEV
#define RDEV b->st_rdev
	      "1 %d %d "                /* have_rdev rdev rdev2 */
#else
#define RDEV 0
	      "0 %d %d "                /* have_rdev rdev rdev2 */
#endif
	      "{%s} {%s} {%s} "         /* create modify access */
	      "{%s} {%s} {%s} "         /* fsys dev type */
	      "%d {%s} %d {%s} "        /* have_space avail percent total */
	      "%d %d %d %d",            /* have_ino nfree inoperc inotot */

	      wtk_win (info->widget), fname,
	      b->st_dev, b->st_ino,
	      string_perm (b->st_mode), b->st_mode & 07777,
	      b->st_nlink, get_owner (b->st_uid), get_group (b->st_gid),
	      BLOCKS,
	      bsize,
	      RDEV >> 8, RDEV & 0xff,
	      cre, mod, acc,
	      s->mpoint, s->device, s->typename,
	      have_space, avail_buf, space_percent, total_buf,
	      have_nodes, s->nfree, ispace_percent, s->nodes);

    free (mod);
    free (acc);
    free (cre);
}


