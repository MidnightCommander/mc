/*
   Main program for the Midnight Commander

   Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
   2003, 2004, 2005, 2006, 2007, 2009, 2011
   The Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1994, 1995, 1996, 1997
   Janne Kukonlehto, 1994, 1995
   Norbert Warmuth, 1997

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

/** \file main.c
 *  \brief Source: this is a main module
 */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <pwd.h>                /* for username in xterm title */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

#include "lib/global.h"

#include "lib/event.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"        /* For init_key() */
#include "lib/tty/mouse.h"      /* init_mouse() */
#include "lib/skin.h"
#include "lib/filehighlight.h"
#include "lib/fileloc.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/vfs/vfs.h"        /* vfs_init(), vfs_shut() */

#include "filemanager/midnight.h"       /* current_panel */
#include "filemanager/treestore.h"      /* tree_store_save */
#include "filemanager/layout.h" /* command_prompt */
#include "filemanager/ext.h"    /* flush_extension_file() */
#include "filemanager/command.h"        /* cmdline */
#include "filemanager/panel.h"  /* panalized_panel */

#include "vfs/plugins_init.h"

#include "events_init.h"
#include "args.h"
#ifdef ENABLE_SUBSHELL
#include "subshell.h"
#endif
#include "setup.h"              /* load_setup() */

#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#include "selcodepage.h"
#endif /* HAVE_CHARSET */

#include "consaver/cons.saver.h"        /* cons_saver_pid */

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
check_codeset (void)
{
    const char *current_system_codepage = NULL;

    current_system_codepage = str_detect_termencoding ();

#ifdef HAVE_CHARSET
    {
        const char *_display_codepage;

        _display_codepage = get_codepage_id (mc_global.display_codepage);

        if (strcmp (_display_codepage, current_system_codepage) != 0)
        {
            mc_global.display_codepage = get_codepage_index (current_system_codepage);
            if (mc_global.display_codepage == -1)
                mc_global.display_codepage = 0;

            mc_config_set_string (mc_main_config, CONFIG_MISC_SECTION, "display_codepage",
                                  cp_display);
        }
    }
#endif

    mc_global.utf8_display = str_isutf8 (current_system_codepage);
}

/* --------------------------------------------------------------------------------------------- */

/** POSIX version.  The only version we support.  */
static void
OS_Setup (void)
{
    const char *shell_env;
    const char *datadir_env;

    shell_env = getenv ("SHELL");
    if ((shell_env == NULL) || (shell_env[0] == '\0'))
    {
        struct passwd *pwd;

        pwd = getpwuid (geteuid ());
        if (pwd != NULL)
            mc_global.tty.shell = g_strdup (pwd->pw_shell);
    }
    else
        mc_global.tty.shell = g_strdup (shell_env);

    if ((mc_global.tty.shell == NULL) || (mc_global.tty.shell[0] == '\0'))
    {
        g_free (mc_global.tty.shell);
        mc_global.tty.shell = g_strdup ("/bin/sh");
    }

    /* This is the directory, where MC was installed, on Unix this is DATADIR */
    /* and can be overriden by the MC_DATADIR environment variable */
    datadir_env = g_getenv ("MC_DATADIR");
    if (datadir_env != NULL)
        mc_global.sysconfig_dir = g_strdup (datadir_env);
    else
        mc_global.sysconfig_dir = g_strdup (SYSCONFDIR);

    mc_global.share_data_dir = g_strdup (DATADIR);

    /* Set up temporary directory */
    mc_tmpdir ();
}

/* --------------------------------------------------------------------------------------------- */

static void
sigchld_handler_no_subshell (int sig)
{
#ifdef __linux__
    int pid, status;

    if (!mc_global.tty.console_flag != '\0')
        return;

    /* COMMENT: if it were true that after the call to handle_console(..INIT)
       the value of mc_global.tty.console_flag never changed, we could simply not install
       this handler at all if (!mc_global.tty.console_flag && !mc_global.tty.use_subshell). */

    /* That comment is no longer true.  We need to wait() on a sigchld
       handler (that's at least what the tarfs code expects currently). */

    pid = waitpid (cons_saver_pid, &status, WUNTRACED | WNOHANG);

    if (pid == cons_saver_pid)
    {

        if (WIFSTOPPED (status))
        {
            /* Someone has stopped cons.saver - restart it */
            kill (pid, SIGCONT);
        }
        else
        {
            /* cons.saver has died - disable console saving */
            handle_console (CONSOLE_DONE);
            mc_global.tty.console_flag = '\0';
        }
    }
    /* If we got here, some other child exited; ignore it */
#endif /* __linux__ */

    (void) sig;
}

/* --------------------------------------------------------------------------------------------- */

static void
init_sigchld (void)
{
    struct sigaction sigchld_action;

    sigchld_action.sa_handler =
#ifdef ENABLE_SUBSHELL
        mc_global.tty.use_subshell ? sigchld_handler :
#endif /* ENABLE_SUBSHELL */
        sigchld_handler_no_subshell;

    sigemptyset (&sigchld_action.sa_mask);

#ifdef SA_RESTART
    sigchld_action.sa_flags = SA_RESTART;
#else
    sigchld_action.sa_flags = 0;
#endif /* !SA_RESTART */

    if (sigaction (SIGCHLD, &sigchld_action, NULL) == -1)
    {
#ifdef ENABLE_SUBSHELL
        /*
         * This may happen on QNX Neutrino 6, where SA_RESTART
         * is defined but not implemented.  Fallback to no subshell.
         */
        mc_global.tty.use_subshell = FALSE;
#endif /* ENABLE_SUBSHELL */
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
main (int argc, char *argv[])
{
    GError *error = NULL;
    gboolean config_migrated = FALSE;
    char *config_migrate_msg;
    int exit_code = EXIT_FAILURE;

    /* We had LC_CTYPE before, LC_ALL includs LC_TYPE as well */
#ifdef HAVE_SETLOCALE
    (void) setlocale (LC_ALL, "");
#endif
    (void) bindtextdomain (PACKAGE, LOCALEDIR);
    (void) textdomain (PACKAGE);

    /* do this before args parsing */
    str_init_strings (NULL);

    if (!mc_args_parse (&argc, &argv, "mc", &error))
    {
      startup_exit_falure:
        fprintf (stderr, _("Failed to run:\n%s\n"), error->message);
        g_error_free (error);
        g_free (mc_global.tty.shell);
      startup_exit_ok:
        str_uninit_strings ();
        return exit_code;
    }

    /* do this before mc_args_show_info () to view paths in the --datadir-info output */
    OS_Setup ();

    if (!g_path_is_absolute (mc_config_get_home_dir ()))
    {
        error = g_error_new (MC_ERROR, 0, "%s: %s", _("Home directory path is not absolute"),
                             mc_config_get_home_dir ());
        mc_event_deinit (NULL);
        goto startup_exit_falure;
    }

    if (!mc_args_show_info ())
    {
        exit_code = EXIT_SUCCESS;
        goto startup_exit_ok;
    }

    if (!events_init (&error))
        goto startup_exit_falure;

    mc_config_init_config_paths (&error);
    if (error == NULL)
        config_migrated = mc_config_migrate_from_old_place (&error, &config_migrate_msg);
    if (error != NULL)
    {
        mc_event_deinit (NULL);
        goto startup_exit_falure;
    }

    vfs_init ();
    vfs_plugins_init ();
    vfs_setup_work_dir ();

    /* do this after vfs initialization due to mc_setctl() call in mc_setup_by_args() */
    if (!mc_setup_by_args (argc, argv, &error))
    {
        vfs_shut ();
        mc_event_deinit (NULL);
        goto startup_exit_falure;
    }

    /* check terminal type
     * $TEMR must be set and not empty
     * mc_global.tty.xterm_flag is used in init_key() and tty_init()
     * Do this after mc_args_handle() where mc_args__force_xterm is set up.
     */
    mc_global.tty.xterm_flag = tty_check_term (mc_args__force_xterm);

    /* NOTE: This has to be called before tty_init or whatever routine
       calls any define_sequence */
    init_key ();

    /* Must be done before installing the SIGCHLD handler [[FIXME]] */
    handle_console (CONSOLE_INIT);

#ifdef ENABLE_SUBSHELL
    /* Don't use subshell when invoked as viewer or editor */
    if (mc_global.mc_run_mode != MC_RUN_FULL)
        mc_global.tty.use_subshell = FALSE;

    if (mc_global.tty.use_subshell)
        subshell_get_console_attributes ();
#endif /* ENABLE_SUBSHELL */

    /* Install the SIGCHLD handler; must be done before init_subshell() */
    init_sigchld ();

    /* We need this, since ncurses endwin () doesn't restore the signals */
    save_stop_handler ();

    /* Must be done before init_subshell, to set up the terminal size: */
    /* FIXME: Should be removed and LINES and COLS computed on subshell */
    tty_init (!mc_args__nomouse, mc_global.tty.xterm_flag);

    load_setup ();

    /* start check mc_global.display_codepage and mc_global.source_codepage */
    check_codeset ();

    /* Removing this from the X code let's us type C-c */
    load_key_defs ();

    load_keymap_defs (!mc_args__nokeymap);

    macros_list = g_array_new (TRUE, FALSE, sizeof (macros_t));

    tty_init_colors (mc_global.tty.disable_colors, mc_args__force_colors);

    mc_skin_init (&error);
    if (error != NULL)
    {
        message (D_ERROR, _("Warning"), "%s", error->message);
        g_error_free (error);
        error = NULL;
    }

    dlg_set_default_colors ();

#ifdef ENABLE_SUBSHELL
    /* Done here to ensure that the subshell doesn't  */
    /* inherit the file descriptors opened below, etc */
    if (mc_global.tty.use_subshell)
        init_subshell ();
#endif /* ENABLE_SUBSHELL */

    /* Also done after init_subshell, to save any shell init file messages */
    if (mc_global.tty.console_flag != '\0')
        handle_console (CONSOLE_SAVE);

    if (mc_global.tty.alternate_plus_minus)
        application_keypad_mode ();

    /* Done after subshell initialization to allow select and paste text by mouse
       w/o Shift button in subshell in the native console */
    init_mouse ();

    /* subshell_prompt is NULL here */
    mc_prompt = (geteuid () == 0) ? "# " : "$ ";

    if (config_migrated)
    {
        message (D_ERROR, _("Warning"), "%s", config_migrate_msg);
        g_free (config_migrate_msg);
    }

    /* Program main loop */
    if (mc_global.midnight_shutdown)
        exit_code = EXIT_SUCCESS;
    else
        exit_code = do_nc ()? EXIT_SUCCESS : EXIT_FAILURE;

    /* Save the tree store */
    (void) tree_store_save ();

    free_keymap_defs ();

    /* Virtual File System shutdown */
    vfs_shut ();

    flush_extension_file ();    /* does only free memory */

    mc_skin_deinit ();
    tty_colors_done ();

    tty_shutdown ();

    done_setup ();

    if (mc_global.tty.console_flag != '\0' && (quit & SUBSHELL_EXIT) == 0)
        handle_console (CONSOLE_RESTORE);
    if (mc_global.tty.alternate_plus_minus)
        numeric_keypad_mode ();

    (void) signal (SIGCHLD, SIG_DFL);   /* Disable the SIGCHLD handler */

    if (mc_global.tty.console_flag != '\0')
        handle_console (CONSOLE_DONE);

    if (mc_global.mc_run_mode == MC_RUN_FULL && mc_args__last_wd_file != NULL
        && last_wd_string != NULL && !print_last_revert)
    {
        int last_wd_fd;

        last_wd_fd = open (mc_args__last_wd_file, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL,
                           S_IRUSR | S_IWUSR);
        if (last_wd_fd != -1)
        {
            ssize_t ret1;
            int ret2;
            ret1 = write (last_wd_fd, last_wd_string, strlen (last_wd_string));
            ret2 = close (last_wd_fd);
            (void) ret1;
            (void) ret2;
        }
    }
    g_free (last_wd_string);

    g_free (mc_global.tty.shell);

    done_key ();

    if (macros_list != NULL)
    {
        guint i;
        macros_t *macros;
        for (i = 0; i < macros_list->len; i++)
        {
            macros = &g_array_index (macros_list, struct macros_t, i);
            if (macros != NULL && macros->macro != NULL)
                (void) g_array_free (macros->macro, FALSE);
        }
        (void) g_array_free (macros_list, TRUE);
    }

    str_uninit_strings ();

    if (mc_global.mc_run_mode != MC_RUN_EDITOR)
        g_free (mc_run_param0);
    else
    {
        g_list_foreach ((GList *) mc_run_param0, (GFunc) mcedit_arg_free, NULL);
        g_list_free ((GList *) mc_run_param0);
    }
    g_free (mc_run_param1);

    mc_config_deinit_config_paths ();

    (void) mc_event_deinit (&error);
    if (error != NULL)
    {
        fprintf (stderr, _("\nFailed while close:\n%s\n"), error->message);
        g_error_free (error);
        exit_code = EXIT_FAILURE;
    }

    (void) putchar ('\n');      /* Hack to make shell's prompt start at left of screen */

    return exit_code;
}

/* --------------------------------------------------------------------------------------------- */
