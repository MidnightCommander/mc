/* Print features specific for this build */

#include <config.h>
#include <sys/param.h>
#include <stdio.h>
#include <glib.h>
#include "i18n.h"
#include "cmd.h"
#include "textconf.h"

static const char * const features [] =  {
    N_("Edition: "),
    N_("text mode"),
#ifdef HAVE_TEXTMODE_X11_SUPPORT
    N_(" with X11 support to read modifiers"),
#endif
    "\n",

#ifdef USE_VFS
    N_("Virtual File System: tarfs, extfs"),
#ifdef USE_NETCODE
    N_(", ftpfs"),
#   ifdef HSC_PROXY
    N_(" (proxies: hsc proxy)"),
#   endif
    N_(", mcfs"),
#   ifdef USE_TERMNET
    N_(" (with termnet support)"),
#   endif
#   ifdef WITH_SMBFS
    N_(", smbfs"),
#   endif
#endif
#ifdef USE_EXT2FSLIB
    N_(", undelfs"),
#endif
    "\n",
#endif
    
#ifdef USE_INTERNAL_EDIT
    N_("With builtin Editor\n"),
#endif

    N_("Using "),
#ifdef HAVE_SLANG
#   ifdef HAVE_SYSTEM_SLANG
	N_("system-installed "),
#   endif
    N_("S-lang library with "),

#   ifdef SLANG_TERMINFO
        N_("terminfo"),
#   else
#       ifdef USE_TERMCAP
            N_("termcap"),
#       else
            N_("an unknown terminal"),
#       endif
#   endif
    N_(" database"),
#else
#   ifdef USE_NCURSES
        N_("the ncurses library"),
#   else
        N_("some unknown curses library"),
#   endif
#endif
    "\n",
#ifdef HAVE_SUBSHELL_SUPPORT
    N_("With subshell support: "),
#   ifdef SUBSHELL_OPTIONAL
        N_("optional"),
#   else
        N_("as default"),
#   endif
    "\n",
#endif

#ifdef WITH_BACKGROUND
    N_("With support for background operations\n"),
#endif

#ifdef HAVE_LIBGPM
    N_("with mouse support on xterm and the Linux console.\n"),
#else
    N_("with mouse support on xterm.\n"),
#endif
	NULL }
;

void
version (int verbose)
{
    char *str;
    int i;

    fprintf (stderr, "The Midnight Commander %s\n", VERSION);
    if (!verbose)
	return;
    
    for (i = 0; features [i]; i++) 
    	fprintf (stderr, _(features [i]));

    str = guess_message_value (1);
    fprintf (stderr, "%s\n", str);
    g_free (str);
}
