/* Print features specific for this build */

#include <config.h>
#include <stdio.h>

#include "global.h"

static const char * const features [] =  {
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
    N_("Using old curses library"),
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
