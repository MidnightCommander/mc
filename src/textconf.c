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

#include <config.h>

#include <limits.h>
#include <stdio.h>

#include <sys/types.h>

#include "global.h"
#include "ecs.h"

#ifdef USE_VFS
static const char *const vfs_supported[] = {
    "tarfs",
    "extfs",
    "cpiofs",
#ifdef USE_NETCODE
    "ftpfs",
    "fish",
#   ifdef WITH_MCFS
    "mcfs",
#   endif
#   ifdef WITH_SMBFS
    "smbfs",
#   endif
#endif				/* USE_NETCODE */
#ifdef USE_EXT2FSLIB
    "undelfs",
#endif
    NULL
};
#endif				/* USE_VFS */


static const char *const features[] = {
#ifdef USE_INTERNAL_EDIT
    N_("With builtin Editor\n"),
#endif

#ifdef HAVE_SLANG

#   ifdef HAVE_SYSTEM_SLANG
    N_("Using system-installed S-Lang library"),
#   else
    N_("Using included S-Lang library"),
#   endif

    " ",

#ifdef USE_TERMCAP
    N_("with termcap database"),
#else
    N_("with terminfo database"),
#endif

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
show_version (int verbose)
{
    int i;

    printf (_("GNU Midnight Commander %s\n"), VERSION);
    if (!verbose)
	return;

#ifdef USE_VFS
    printf (_("Virtual File System:"));
    for (i = 0; vfs_supported[i]; i++) {
	if (i == 0)
	    printf (" ");
	else
	    printf (", ");

	printf ("%s", _(vfs_supported[i]));
    }
    printf ("\n");
#endif				/* USE_VFS */

    for (i = 0; features[i]; i++)
	printf ("%s", _(features[i]));

    (void)printf("Data types:");
#define TYPE_INFO(T) \
    (void)printf(" %s %d", #T, (int) (CHAR_BIT * sizeof(T)))
    TYPE_INFO(char);
    TYPE_INFO(int);
    TYPE_INFO(long);
    TYPE_INFO(void *);
    TYPE_INFO(off_t);
    TYPE_INFO(ecs_char);
#undef TYPE_INFO
    (void)printf("\n");
}
