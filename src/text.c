/*
 * Text edition support code
 *
 *
 */
#include <config.h>
#include <locale.h>

#ifdef _OS_NT
#    include <windows.h>
#endif

#ifdef __os2__
#    define INCL_DOS
#    define INCL_DOSFILEMGR
#    define INCL_DOSERRORS
#    include <os2.h>
#    include <io.h>
#    include <direct.h>
#endif

#include "tty.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>

#include <sys/stat.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

/* unistd.h defines _POSIX_VERSION on POSIX.1 systems. */
#if defined(HAVE_DIRENT_H) || defined(_POSIX_VERSION)
#   include <dirent.h>
#   define NLENGTH(dirent) (strlen ((dirent)->d_name))
#else
#   define dirent direct
#   define NLENGTH(dirent) ((dirent)->d_namlen)

#   ifdef HAVE_SYS_NDIR_H
#       include <sys/ndir.h>
#   endif /* HAVE_SYS_NDIR_H */

#   ifdef HAVE_SYS_DIR_H
#       include <sys/dir.h>
#   endif /* HAVE_SYS_DIR_H */

#   ifdef HAVE_NDIR_H
#       include <ndir.h>
#   endif /* HAVE_NDIR_H */
#endif /* not (HAVE_DIRENT_H or _POSIX_VERSION) */

#if HAVE_SYS_WAIT_H
#   include <sys/wait.h>	/* For waitpid() */
#endif

#include <errno.h>
#ifndef OS2_NT
#    include <pwd.h>
#endif
#include <ctype.h>
#include <fcntl.h>	/* For O_RDWR */
#include <signal.h>

/* Program include files */
#include "mad.h"
#include "dir.h"
#include "color.h"
#include "global.h"
#include "util.h"
#include "dialog.h"
#include "menu.h"
#include "file.h"
#include "panel.h"
#define WANT_WIDGETS
#include "main.h"
#include "win.h"
#include "user.h"
#include "mem.h"
#include "mouse.h"
#include "option.h"
#include "tree.h"
#include "cons.saver.h"
#include "subshell.h"
#include "key.h"	/* For init_key() and mi_getch() */
#include "setup.h"	/* save_setup() */
#include "profile.h"	/* free_profiles() */
#include "boxes.h"
#include "layout.h"
#include "cmd.h"		/* Normal commands */
#include "hotlist.h"
#include "panelize.h"
#ifndef __os2__
#    include "learn.h"
#endif
#include "listmode.h"
#include "background.h"
#include "dirhist.h"
#include "ext.h"	/* For flush_extension_file() */

/* Listbox for the command history feature */
#include "widget.h"
#include "command.h"
#include "wtools.h"
#include "complete.h"		/* For the free_completion */

#include "chmod.h"
#include "chown.h"

#ifdef OS2_NT
#    include <drive.h>
#endif

#include "../vfs/vfs.h"
#include "../vfs/extfs.h"

#ifdef HAVE_XVIEW
#   include "../xv/xvmain.h"
#endif

#ifdef HAVE_TK
#   include "tkmain.h"
#endif

#include "popt.h"

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

#ifndef USE_VFS
#ifdef USE_NETCODE
#undef USE_NETCODE
#endif
#endif


char *default_edition_colors =
"normal=lightgray,blue:"
"selected=black,cyan:"
"marked=yellow,blue:"
"markselect=yellow,cyan:"
"errors=white,red:"
"menu=white,cyan:"
"reverse=black,white:"
"dnormal=black,white:"
"dfocus=black,cyan:"
#if defined(__os2__)
"dhotnormal=magenta,white:"	/* .ado: yellow */
"dhotfocus=magenta,cyan:"
#else
"dhotnormal=yellow,white:"
"dhotfocus=yellow,cyan:"
#endif
"viewunderline=brightred,blue:"
"menuhot=yellow,cyan:"
"menusel=white,black:"
"menuhotsel=yellow,black:"
"helpnormal=black,white:"
"helpitalic=red,white:"
"helpbold=blue,white:"
"helplink=black,cyan:"
"helpslink=yellow,blue:"
"gauge=white,black:"
"input=black,cyan:"
"directory=white,blue:"
"execute=brightgreen,blue:"
"link=lightgray,blue:"
"device=brightmagenta,blue:"
"core=red,blue:"
"special=black,blue";

void
edition_post_exec (void)
{
    do_enter_ca_mode ();

    /* FIXME: Missing on slang endwin? */
    reset_prog_mode ();
    flushinp ();
    
    keypad (stdscr, TRUE);
    mc_raw_mode ();
    channels_up ();
    if (use_mouse_p)
	init_mouse ();
    if (alternate_plus_minus && (console_flag || xterm_flag)) {
        fprintf (stdout, "\033="); fflush (stdout);
    }
}

void
edition_pre_exec (void)
{
    if (clear_before_exec)
	clr_scr ();
    else {
	if (!(console_flag || xterm_flag))
	    printf ("\n\n");
    }

    channels_down ();
    if (use_mouse_p)
	shut_mouse ();
    
    reset_shell_mode ();
    keypad (stdscr, FALSE);
    endwin ();
    
    if (alternate_plus_minus && (console_flag || xterm_flag)) {
        fprintf (stdout, "\033>"); fflush (stdout);
    }
    
    /* on xterms: maybe endwin did not leave the terminal on the shell
     * screen page: do it now.
     *
     * Do not move this before endwin: in some systems rmcup includes
     * a call to clear screen, so it will end up clearing the sheel screen.
     */
    if (!status_using_ncurses){
	do_exit_ca_mode ();
    }
}

void
clr_scr (void)
{
    standend ();
    dlg_erase (midnight_dlg);
    mc_refresh ();
    doupdate ();
}

