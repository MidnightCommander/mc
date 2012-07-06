/*
   Execution routines for GNU Midnight Commander

   Copyright (C) 2003, 2004, 2005, 2007, 2011
   The Free Software Foundation, Inc.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file  execute.c
 *  \brief Source: execution routines
 */

#include <config.h>

#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/tty/win.h"
#include "lib/vfs/vfs.h"
#include "lib/mcconfig.h"
#include "lib/util.h"
#include "lib/widget.h"

#include "filemanager/midnight.h"
#include "filemanager/layout.h" /* use_dash() */
#include "consaver/cons.saver.h"
#include "subshell.h"
#include "setup.h"              /* clear_before_exec */

#include "execute.h"

/*** global variables ****************************************************************************/

int pause_after_run = pause_on_dumb_terminals;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
edition_post_exec (void)
{
    do_enter_ca_mode ();

    /* FIXME: Missing on slang endwin? */
    tty_reset_prog_mode ();
    tty_flush_input ();

    tty_keypad (TRUE);
    tty_raw_mode ();
    channels_up ();
    enable_mouse ();
    if (mc_global.tty.alternate_plus_minus)
        application_keypad_mode ();
}

/* --------------------------------------------------------------------------------------------- */

static void
edition_pre_exec (void)
{
    if (clear_before_exec)
        clr_scr ();
    else
    {
        if (!(mc_global.tty.console_flag != '\0' || mc_global.tty.xterm_flag))
            printf ("\n\n");
    }

    channels_down ();
    disable_mouse ();

    tty_reset_shell_mode ();
    tty_keypad (FALSE);
    tty_reset_screen ();

    numeric_keypad_mode ();

    /* on xterms: maybe endwin did not leave the terminal on the shell
     * screen page: do it now.
     *
     * Do not move this before endwin: in some systems rmcup includes
     * a call to clear screen, so it will end up clearing the shell screen.
     */
    do_exit_ca_mode ();
}

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_SUBSHELL_SUPPORT
static void
do_possible_cd (const vfs_path_t * new_dir_vpath)
{
    if (!do_cd (new_dir_vpath, cd_exact))
        message (D_ERROR, _("Warning"),
                 _("The Commander can't change to the directory that\n"
                   "the subshell claims you are in. Perhaps you have\n"
                   "deleted your working directory, or given yourself\n"
                   "extra access permissions with the \"su\" command?"));
}
#endif /* HAVE_SUBSHELL_SUPPORT */

/* --------------------------------------------------------------------------------------------- */

static void
do_execute (const char *lc_shell, const char *command, int flags)
{
#ifdef HAVE_SUBSHELL_SUPPORT
    vfs_path_t *new_dir_vpath = NULL;
#endif /* HAVE_SUBSHELL_SUPPORT */

    vfs_path_t *old_vfs_dir_vpath = NULL;

    if (!vfs_current_is_local ())
        old_vfs_dir_vpath = vfs_path_clone (vfs_get_raw_current_dir ());

    if (mc_global.mc_run_mode == MC_RUN_FULL)
        save_cwds_stat ();
    pre_exec ();
    if (mc_global.tty.console_flag != '\0')
        handle_console (CONSOLE_RESTORE);

    if (!mc_global.tty.use_subshell && command && !(flags & EXECUTE_INTERNAL))
    {
        printf ("%s%s\n", mc_prompt, command);
        fflush (stdout);
    }
#ifdef HAVE_SUBSHELL_SUPPORT
    if (mc_global.tty.use_subshell && !(flags & EXECUTE_INTERNAL))
    {
        do_update_prompt ();

        /* We don't care if it died, higher level takes care of this */
        invoke_subshell (command, VISIBLY, old_vfs_dir_vpath != NULL ? NULL : &new_dir_vpath);
    }
    else
#endif /* HAVE_SUBSHELL_SUPPORT */
        my_system (flags, lc_shell, command);

    if (!(flags & EXECUTE_INTERNAL))
    {
        if ((pause_after_run == pause_always
             || (pause_after_run == pause_on_dumb_terminals && !mc_global.tty.xterm_flag
                 && mc_global.tty.console_flag == '\0')) && quit == 0
#ifdef HAVE_SUBSHELL_SUPPORT
            && subshell_state != RUNNING_COMMAND
#endif /* HAVE_SUBSHELL_SUPPORT */
            )
        {
            printf (_("Press any key to continue..."));
            fflush (stdout);
            tty_raw_mode ();
            get_key_code (0);
            printf ("\r\n");
            fflush (stdout);
        }
        if (mc_global.tty.console_flag != '\0')
        {
            if (output_lines && mc_global.keybar_visible)
            {
                putchar ('\n');
                fflush (stdout);
            }
        }
    }

    if (mc_global.tty.console_flag != '\0')
        handle_console (CONSOLE_SAVE);
    edition_post_exec ();

#ifdef HAVE_SUBSHELL_SUPPORT
    if (new_dir_vpath != NULL)
    {
        do_possible_cd (new_dir_vpath);
        vfs_path_free (new_dir_vpath);
    }

#endif /* HAVE_SUBSHELL_SUPPORT */

    if (old_vfs_dir_vpath != NULL)
    {
        mc_chdir (old_vfs_dir_vpath);
        vfs_path_free (old_vfs_dir_vpath);
    }

    if (mc_global.mc_run_mode == MC_RUN_FULL)
    {
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
        update_xterm_title_path ();
    }

    do_refresh ();
    use_dash (TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void
do_suspend_cmd (void)
{
    pre_exec ();

    if (mc_global.tty.console_flag != '\0' && !mc_global.tty.use_subshell)
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
#endif /* SIGTSTP */

    if (mc_global.tty.console_flag != '\0' && !mc_global.tty.use_subshell)
        handle_console (CONSOLE_SAVE);

    edition_post_exec ();
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** Set up the terminal before executing a program */

void
pre_exec (void)
{
    use_dash (FALSE);
    edition_pre_exec ();
}

/* --------------------------------------------------------------------------------------------- */
/** Hide the terminal after executing a program */
void
post_exec (void)
{
    edition_post_exec ();
    use_dash (TRUE);
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */
/* Executes a command */

void
shell_execute (const char *command, int flags)
{
    char *cmd = NULL;

    if (flags & EXECUTE_HIDE)
    {
        cmd = g_strconcat (" ", command, (char *) NULL);
        flags ^= EXECUTE_HIDE;
    }

#ifdef HAVE_SUBSHELL_SUPPORT
    if (mc_global.tty.use_subshell)
        if (subshell_state == INACTIVE)
            do_execute (shell, cmd ? cmd : command, flags | EXECUTE_AS_SHELL);
        else
            message (D_ERROR, MSG_ERROR, _("The shell is already running a command"));
    else
#endif /* HAVE_SUBSHELL_SUPPORT */
        do_execute (shell, cmd ? cmd : command, flags | EXECUTE_AS_SHELL);

    g_free (cmd);
}

/* --------------------------------------------------------------------------------------------- */

void
exec_shell (void)
{
    do_execute (shell, 0, 0);
}

/* --------------------------------------------------------------------------------------------- */

void
toggle_panels (void)
{
#ifdef HAVE_SUBSHELL_SUPPORT
    vfs_path_t *new_dir_vpath = NULL;
    vfs_path_t **new_dir_p;
#endif /* HAVE_SUBSHELL_SUPPORT */

    channels_down ();
    disable_mouse ();
    if (clear_before_exec)
        clr_scr ();
    if (mc_global.tty.alternate_plus_minus)
        numeric_keypad_mode ();
#ifndef HAVE_SLANG
    /* With slang we don't want any of this, since there
     * is no raw_mode supported
     */
    tty_reset_shell_mode ();
#endif /* !HAVE_SLANG */
    tty_noecho ();
    tty_keypad (FALSE);
    tty_reset_screen ();
    do_exit_ca_mode ();
    tty_raw_mode ();
    if (mc_global.tty.console_flag != '\0')
        handle_console (CONSOLE_RESTORE);

#ifdef HAVE_SUBSHELL_SUPPORT
    if (mc_global.tty.use_subshell)
    {
        new_dir_p = vfs_current_is_local ()? &new_dir_vpath : NULL;
        invoke_subshell (NULL, VISIBLY, new_dir_p);
    }
    else
#endif /* HAVE_SUBSHELL_SUPPORT */
    {
        if (output_starts_shell)
        {
            fprintf (stderr, _("Type `exit' to return to the Midnight Commander"));
            fprintf (stderr, "\n\r\n\r");

            my_system (EXECUTE_INTERNAL, shell, NULL);
        }
        else
            get_key_code (0);
    }

    if (mc_global.tty.console_flag != '\0')
        handle_console (CONSOLE_SAVE);

    do_enter_ca_mode ();

    tty_reset_prog_mode ();
    tty_keypad (TRUE);

    /* Prevent screen flash when user did 'exit' or 'logout' within
       subshell */
    if ((quit & SUBSHELL_EXIT) != 0)
    {
        /* User did `exit' or `logout': quit MC */
        if (quiet_quit_cmd ())
            return;

        quit = 0;
#ifdef HAVE_SUBSHELL_SUPPORT
        /* restart subshell */
        if (mc_global.tty.use_subshell)
            init_subshell ();
#endif /* HAVE_SUBSHELL_SUPPORT */
    }

    enable_mouse ();
    channels_up ();
    if (mc_global.tty.alternate_plus_minus)
        application_keypad_mode ();

#ifdef HAVE_SUBSHELL_SUPPORT
    if (mc_global.tty.use_subshell)
    {
        do_load_prompt ();
        if (new_dir_vpath != NULL)
            do_possible_cd (new_dir_vpath);
        if (mc_global.tty.console_flag != '\0' && output_lines)
            show_console_contents (output_start_y,
                                   LINES - mc_global.keybar_visible - output_lines -
                                   1, LINES - mc_global.keybar_visible - 1);
    }

    vfs_path_free (new_dir_vpath);
#endif /* HAVE_SUBSHELL_SUPPORT */

    if (mc_global.mc_run_mode == MC_RUN_FULL)
    {
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
        update_xterm_title_path ();
    }
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

/* event callback */
gboolean
execute_suspend (const gchar * event_group_name, const gchar * event_name,
                 gpointer init_data, gpointer data)
{
    (void) event_group_name;
    (void) event_name;
    (void) init_data;
    (void) data;

    if (mc_global.mc_run_mode == MC_RUN_FULL)
        save_cwds_stat ();
    do_suspend_cmd ();
    if (mc_global.mc_run_mode == MC_RUN_FULL)
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    do_refresh ();

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Execute command on a filename that can be on VFS.
 * Errors are reported to the user.
 */

void
execute_with_vfs_arg (const char *command, const vfs_path_t * filename_vpath)
{
    struct stat st;
    time_t mtime;
    vfs_path_t *localcopy_vpath;

    /* Simplest case, this file is local */
    if (filename_vpath == NULL || vfs_file_is_local (filename_vpath))
    {
        do_execute (command, vfs_path_get_last_path_str (filename_vpath), EXECUTE_INTERNAL);
        return;
    }

    /* FIXME: Creation of new files on VFS is not supported */
    if (vfs_path_len (filename_vpath) == 0)
        return;

    localcopy_vpath = mc_getlocalcopy (filename_vpath);
    if (localcopy_vpath == NULL)
    {
        char *filename;

        filename = vfs_path_to_str (filename_vpath);
        message (D_ERROR, MSG_ERROR, _("Cannot fetch a local copy of %s"), filename);
        g_free (filename);
        return;
    }

    /*
     * filename can be an entry on panel, it can be changed by executing
     * the command, so make a copy.  Smarter VFS code would make the code
     * below unnecessary.
     */
    mc_stat (localcopy_vpath, &st);
    mtime = st.st_mtime;
    do_execute (command, vfs_path_get_last_path_str (localcopy_vpath), EXECUTE_INTERNAL);
    mc_stat (localcopy_vpath, &st);
    mc_ungetlocalcopy (filename_vpath, localcopy_vpath, mtime != st.st_mtime);
    vfs_path_free (localcopy_vpath);
}

/* --------------------------------------------------------------------------------------------- */
