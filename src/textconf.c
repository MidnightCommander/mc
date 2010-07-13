/* Print features specific for this build

   Copyright (C) 2000, 2001, 2002, 2004, 2005, 2007
   Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file textconf.c
 *  \brief Source: prints features specific for this build
 */

#include <config.h>

#include <limits.h>
#include <stdio.h>
#include <sys/types.h>

#include "lib/global.h"
#include "src/textconf.h"

#ifdef ENABLE_VFS
static const char *const vfs_supported[] = {
#ifdef ENABLE_VFS_CPIO
    "cpiofs",
#endif
#ifdef ENABLE_VFS_TAR
    "tarfs",
#endif
#ifdef ENABLE_VFS_SFS
    "sfs",
#endif
#ifdef ENABLE_VFS_EXTFS
    "extfs",
#endif
#ifdef ENABLE_VFS_UNDELFS
    "undelfs",
#endif
#ifdef ENABLE_VFS_FTP
    "ftpfs",
#endif
#ifdef ENABLE_VFS_FISH
    "fish",
#endif
#ifdef ENABLE_VFS_SMB
    "smbfs",
#endif /* ENABLE_VFS_SMB */
    NULL
};
#endif				/* ENABLE_VFS */


static const char *const features[] = {
#ifdef USE_INTERNAL_EDIT
    N_("With builtin Editor\n"),
#endif

#ifdef HAVE_SLANG

    N_("Using system-installed S-Lang library"),

    " ",

    N_("with terminfo database"),

#elif defined(USE_NCURSES)
    N_("Using the ncurses library"),
#elif defined(USE_NCURSESW)
    N_("Using the ncursesw library"),
#else
#error "Cannot compile mc without S-Lang or ncurses"
#endif				/* !HAVE_SLANG && !USE_NCURSES */

    "\n",

#ifdef HAVE_SUBSHELL_SUPPORT
#   ifdef SUBSHELL_OPTIONAL
    N_("With optional subshell support"),
#   else
    N_("With subshell support as default"),
#   endif
    "\n",
#endif				/* !HAVE_SUBSHELL_SUPPORT */

#ifdef WITH_BACKGROUND
    N_("With support for background operations\n"),
#endif

#ifdef HAVE_LIBGPM
    N_("With mouse support on xterm and Linux console\n"),
#else
    N_("With mouse support on xterm\n"),
#endif

#ifdef HAVE_TEXTMODE_X11_SUPPORT
    N_("With support for X11 events\n"),
#endif

#ifdef ENABLE_NLS
    N_("With internationalization support\n"),
#endif

#ifdef HAVE_CHARSET
    N_("With multiple codepages support\n"),
#endif

    NULL
};

void
show_version (void)
{
    size_t i;

    printf (_("GNU Midnight Commander %s\n"), VERSION);

#ifdef ENABLE_VFS
    printf (_("Virtual File Systems:"));
    for (i = 0; vfs_supported[i] != NULL; i++)
	printf ("%s %s", i == 0 ? "" : ",", _(vfs_supported[i]));

    printf ("\n");
#endif				/* ENABLE_VFS */

    for (i = 0; features[i] != NULL; i++)
	printf ("%s", _(features[i]));

    (void)printf(_("Data types:"));
#define TYPE_INFO(T) \
    (void)printf(" %s: %d;", #T, (int) (CHAR_BIT * sizeof(T)))
    TYPE_INFO(char);
    TYPE_INFO(int);
    TYPE_INFO(long);
    TYPE_INFO(void *);
    TYPE_INFO(size_t);
    TYPE_INFO(off_t);
#undef TYPE_INFO
    (void)printf("\n");
}
