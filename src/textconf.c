/* Print features specific for this build

   Copyright (C) 2001-2002 Free Software Foundation

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
#include <stdio.h>

#include "global.h"

static const char * const features [] =  {
#ifdef USE_VFS
    N_("Virtual File System: tarfs, extfs"),
    N_(", cpiofs"),
#ifdef USE_NETCODE
    N_(", ftpfs"),
#   ifdef HSC_PROXY
    N_(" (proxies: hsc proxy)"),
#   endif
    N_(", fish"),
#   ifdef WITH_MCFS
    N_(", mcfs"),
#   endif
#   ifdef USE_TERMNET
    N_(" (with termnet support)"),
#   endif
#   ifdef WITH_SMBFS
    N_(", smbfs"),
#   endif
#endif /* USE_NETCODE */
#ifdef USE_EXT2FSLIB
    N_(", undelfs"),
#endif
    "\n",
#endif /* USE_VFS */
    
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

#ifdef SLANG_TERMINFO
    N_("with terminfo database"),
#elif defined(USE_TERMCAP)
    N_("with termcap database"),
#else
    N_("with an unknown terminal database"),
#endif

#elif defined(USE_NCURSES)
    N_("Using the ncurses library"),
#else
 #error "Cannot compile mc without S-Lang or ncurses"
#endif /* !HAVE_SLANG && !USE_NCURSES */

    "\n",

#ifdef HAVE_SUBSHELL_SUPPORT
#   ifdef SUBSHELL_OPTIONAL
    N_("With optional subshell support"),
#   else
    N_("With subshell support as default"),
#   endif
    "\n",
#endif /* !HAVE_SUBSHELL_SUPPORT */

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

    NULL }
;

void
version (int verbose)
{
    int i;

    fprintf (stdout, _("GNU Midnight Commander %s\n"), VERSION);
    if (!verbose)
	return;
    
    for (i = 0; features [i]; i++) 
	fputs (_(features [i]), stdout);
}
