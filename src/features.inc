/* This just computes a nice value for the features variable */

#ifndef VERSION
#    define VERSION "undefined"
#endif

static const char * const features [] =  {
    N_("Edition: "),
#ifdef HAVE_X
#    ifdef HAVE_XVIEW
         N_("XView"),
#    else
         N_("Tk"),
#    endif
#else
    N_("text mode"),
#ifdef HAVE_TEXTMODE_X11_SUPPORT
    N_(" with X11 support to read modifiers"),
#endif
#endif
    ".\n",

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
    ".\n",
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
	NULL }
;

static const int status_mouse_support = 
#ifdef HAVE_LIBGPM
    1;
#else
    0;
#endif

const int status_using_ncurses =
#ifdef HAVE_SLANG
    0;
#else
#ifdef USE_NCURSES
    1;
#else
    0;
#endif
#endif

