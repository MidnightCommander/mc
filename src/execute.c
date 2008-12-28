/* Execution routines for GNU Midnight Commander
   Copyright (C) 2003, 2004, 2005, 2007 Free Software Foundation, Inc.
   
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include <config.h>

#include <signal.h>

#include "global.h"
#include "tty.h"
#include "win.h"
#include "key.h"
#include "main.h"
#include "cons.saver.h"
#include "subshell.h"
#include "layout.h"
#include "dialog.h"
#include "wtools.h"
#include "execute.h"
#include "../vfs/vfs.h"


static void
edition_post_exec (void)
{
    do_enter_ca_mode ();

    /* FIXME: Missing on slang endwin? */
    reset_prog_mode ();
    flushinp ();

    keypad (stdscr, TRUE);
    mc_raw_mode ();
    channels_up ();
    enable_mouse ();
    if (alternate_plus_minus)
	application_keypad_mode ();
}


static void
edition_pre_exec (void)
{
    if (clear_before_exec)
	clr_scr ();
    else {
	if (!(console_flag || xterm_flag))
	    printf ("\n\n");
    }

    channels_down ();
    disable_mouse ();

    reset_shell_mode ();
    keypad (stdscr, FALSE);
    endwin ();

    numeric_keypad_mode ();

    /* on xterms: maybe endwin did not leave the terminal on the shell
     * screen page: do it now.
     *
     * Do not move this before endwin: in some systems rmcup includes
     * a call to clear screen, so it will end up clearing the shell screen.
     */
    do_exit_ca_mode ();
}


/* Set up the terminal before executing a program */
static void
pre_exec (void)
{
    use_dash (0);
    edition_pre_exec ();
}


static void
do_execute (const char *shell, const char *command, int flags)
{
#ifdef HAVE_SUBSHELL_SUPPORT
    char *new_dir = NULL;
#endif				/* HAVE_SUBSHELL_SUPPORT */

#ifdef USE_VFS
    char *old_vfs_dir = 0;

    if (!vfs_current_is_local ())
	old_vfs_dir = g_strdup (vfs_get_current_dir ());
#endif				/* USE_VFS */

    save_cwds_stat ();
    pre_exec ();
    if (console_flag)
	handle_console (CONSOLE_RESTORE);

    if (!use_subshell && command && !(flags & EXECUTE_INTERNAL)) {
	printf ("%s%s\n", prompt, command);
	fflush (stdout);
    }
#ifdef HAVE_SUBSHELL_SUPPORT
    if (use_subshell && !(flags & EXECUTE_INTERNAL)) {
	do_update_prompt ();

	/* We don't care if it died, higher level takes care of this */
#ifdef USE_VFS
	invoke_subshell (command, VISIBLY, old_vfs_dir ? 0 : &new_dir);
#else
	invoke_subshell (command, VISIBLY, &new_dir);
#endif				/* !USE_VFS */
    } else
#endif				/* HAVE_SUBSHELL_SUPPORT */
	my_system (flags, shell, command);

    if (!(flags & EXECUTE_INTERNAL)) {
	if ((pause_after_run == pause_always
	     || (pause_after_run == pause_on_dumb_terminals && !xterm_flag
		 && !console_flag)) && !quit
#ifdef HAVE_SUBSHELL_SUPPORT
	    && subshell_state != RUNNING_COMMAND
#endif				/* HAVE_SUBSHELL_SUPPORT */
	    ) {
	    printf (_("Press any key to continue..."));
	    fflush (stdout);
	    mc_raw_mode ();
	    get_key_code (0);
	    printf ("\r\n");
	    fflush (stdout);
	}
	if (console_flag) {
	    if (output_lines && keybar_visible) {
		putchar ('\n');
		fflush (stdout);
	    }
	}
    }

    if (console_flag)
	handle_console (CONSOLE_SAVE);
    edition_post_exec ();

#ifdef HAVE_SUBSHELL_SUPPORT
    if (new_dir)
	do_possible_cd (new_dir);

#endif				/* HAVE_SUBSHELL_SUPPORT */

#ifdef USE_VFS
    if (old_vfs_dir) {
	mc_chdir (old_vfs_dir);
	g_free (old_vfs_dir);
    }
#endif				/* USE_VFS */

    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    update_xterm_title_path ();

    do_refresh ();
    use_dash (TRUE);
}


/* Executes a command */
void
shell_execute (const char *command, int flags)
{
    char *cmd = NULL;

    if (flags & EXECUTE_HIDE) {
	cmd = g_strconcat (" ", command, (char *) NULL);
	flags ^= EXECUTE_HIDE;
    }

#ifdef HAVE_SUBSHELL_SUPPORT
    if (use_subshell)
	if (subshell_state == INACTIVE)
	    do_execute (shell, cmd ? cmd : command, flags | EXECUTE_AS_SHELL);
	else
	    message (D_ERROR, MSG_ERROR,
		     _(" The shell is already running a command "));
    else
#endif				/* HAVE_SUBSHELL_SUPPORT */
	do_execute (shell, cmd ? cmd : command, flags | EXECUTE_AS_SHELL);

    g_free (cmd);
}


void
exec_shell (void)
{
    do_execute (shell, 0, 0);
}


void
toggle_panels (void)
{
#ifdef HAVE_SUBSHELL_SUPPORT
    char *new_dir = NULL;
    char **new_dir_p;
#endif				/* HAVE_SUBSHELL_SUPPORT */

    channels_down ();
    disable_mouse ();
    if (clear_before_exec)
	clr_scr ();
    if (alternate_plus_minus)
	numeric_keypad_mode ();
#ifndef HAVE_SLANG
    /* With slang we don't want any of this, since there
     * is no mc_raw_mode supported
     */
    reset_shell_mode ();
    noecho ();
#endif				/* !HAVE_SLANG */
    keypad (stdscr, FALSE);
    endwin ();
    do_exit_ca_mode ();
    mc_raw_mode ();
    if (console_flag)
	handle_console (CONSOLE_RESTORE);

#ifdef HAVE_SUBSHELL_SUPPORT
    if (use_subshell) {
	new_dir_p = vfs_current_is_local ()? &new_dir : NULL;
	if (invoke_subshell (NULL, VISIBLY, new_dir_p))
	    quiet_quit_cmd ();	/* User did `exit' or `logout': quit MC quietly */
    } else
#endif				/* HAVE_SUBSHELL_SUPPORT */
    {
	if (output_starts_shell) {
	    fprintf (stderr,
		     _("Type `exit' to return to the Midnight Commander"));
	    fprintf (stderr, "\n\r\n\r");

	    my_system (EXECUTE_INTERNAL, shell, NULL);
	} else
	    get_key_code (0);
    }
    if (console_flag)
	handle_console (CONSOLE_SAVE);

    do_enter_ca_mode ();

    reset_prog_mode ();
    keypad (stdscr, TRUE);

    /* Prevent screen flash when user did 'exit' or 'logout' within
       subshell */
    if (quit)
	return;

    enable_mouse ();
    channels_up ();
    if (alternate_plus_minus)
	application_keypad_mode ();

#ifdef HAVE_SUBSHELL_SUPPORT
    if (use_subshell) {
	load_prompt (0, 0);
	if (new_dir)
	    do_possible_cd (new_dir);
	if (console_flag && output_lines)
	    show_console_contents (output_start_y,
				   LINES - keybar_visible - output_lines -
				   1, LINES - keybar_visible - 1);
    }
#endif				/* HAVE_SUBSHELL_SUPPORT */

    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    update_xterm_title_path ();
    do_refresh ();
}


static void
do_suspend_cmd (void)
{
    pre_exec ();

    if (console_flag && !use_subshell)
	handle_console (CONSOLE_RESTORE);

#ifdef SIGTSTP
    {
	struct sigaction sigtstp_action;

	/* Make sure that the SIGTSTP below will suspend us directly,
	   without calling ncurses' SIGTSTP handler; we *don't* want
	   ncurses to redraw the screen immediately after the SIGCONT */
	sigaction (SIGTSTP, &startup_handler, &sigtstp_action);

	kill (getpid (), SIGTSTP);

	/* Restore previous SIGTSTP action */
	sigaction (SIGTSTP, &sigtstp_action, NULL);
    }
#endif				/* SIGTSTP */

    if (console_flag && !use_subshell)
	handle_console (CONSOLE_SAVE);

    edition_post_exec ();
}


void
suspend_cmd (void)
{
    save_cwds_stat ();
    do_suspend_cmd ();
    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    do_refresh ();
}


/*
 * Execute command on a filename that can be on VFS.
 * Errors are reported to the user.
 */
void
execute_with_vfs_arg (const char *command, const char *filename)
{
    char *localcopy;
    char *fn;
    struct stat st;
    time_t mtime;

    /* Simplest case, this file is local */
    if (!filename || vfs_file_is_local (filename)) {
        fn = vfs_canon_and_translate (filename);
	do_execute (command, fn, EXECUTE_INTERNAL);
        g_free (fn);
	return;
    }

    /* FIXME: Creation of new files on VFS is not supported */
    if (!*filename)
	return;

    localcopy = mc_getlocalcopy (filename);
    if (localcopy == NULL) {
	message (D_ERROR, MSG_ERROR, _(" Cannot fetch a local copy of %s "),
		 filename);
	return;
    }

    /*
     * filename can be an entry on panel, it can be changed by executing
     * the command, so make a copy.  Smarter VFS code would make the code
     * below unnecessary.
     */
    fn = g_strdup (filename);
    mc_stat (localcopy, &st);
    mtime = st.st_mtime;
    do_execute (command, localcopy, EXECUTE_INTERNAL);
    mc_stat (localcopy, &st);
    mc_ungetlocalcopy (fn, localcopy, mtime != st.st_mtime);
    g_free (localcopy);
    g_free (fn);
}
