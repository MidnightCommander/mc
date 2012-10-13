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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <pwd.h>                /* for username in xterm title */
#include <signal.h>

#include "lib/global.h"

#include "lib/event.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"        /* For init_key() */
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
#include "subshell.h"
#include "setup.h"              /* load_setup() */

#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#include "selcodepage.h"
#endif /* HAVE_CHARSET */

#include "consaver/cons.saver.h"        /* cons_saver_pid */

#include "main.h"

/*** global variables ****************************************************************************/

mc_fhl_t *mc_filehighlight;

/* Set when main loop should be terminated */
int quit = 0;

#ifdef HAVE_CHARSET
/* Numbers of (file I/O) and (input/display) codepages. -1 if not selected */
int default_source_codepage = -1;
char *autodetect_codeset = NULL;
gboolean is_autodetect_codeset_enabled = FALSE;
#endif /* !HAVE_CHARSET */

/* If true use the internal viewer */
int use_internal_view = 1;
/* If set, use the builtin editor */
int use_internal_edit = 1;

char *mc_run_param0 = NULL;
char *mc_run_param1 = NULL;

/* The user's shell */
char *shell = NULL;

/* The prompt */
const char *mc_prompt = NULL;

/* Set to TRUE to suppress printing the last directory */
int print_last_revert = FALSE;

/* If set, then print to the given file the last directory we were at */
char *last_wd_string = NULL;

/* index to record_macro_buf[], -1 if not recording a macro */
int macro_index = -1;

/* macro stuff */
struct macro_action_t record_macro_buf[MAX_MACRO_LENGTH];

GArray *macros_list;

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
            shell = g_strdup (pwd->pw_shell);
    }
    else
        shell = g_strdup (shell_env);

    if ((shell == NULL) || (shell[0] == '\0'))
    {
        g_free (shell);
        shell = g_strdup ("/bin/sh");
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
#ifdef HAVE_SUBSHELL_SUPPORT
        mc_global.tty.use_subshell ? sigchld_handler :
#endif /* HAVE_SUBSHELL_SUPPORT */
        sigchld_handler_no_subshell;

    sigemptyset (&sigchld_action.sa_mask);

#ifdef SA_RESTART
    sigchld_action.sa_flags = SA_RESTART;
#else
    sigchld_action.sa_flags = 0;
#endif /* !SA_RESTART */

    if (sigaction (SIGCHLD, &sigchld_action, NULL) == -1)
    {
#ifdef HAVE_SUBSHELL_SUPPORT
        /*
         * This may happen on QNX Neutrino 6, where SA_RESTART
         * is defined but not implemented.  Fallback to no subshell.
         */
        mc_global.tty.use_subshell = FALSE;
#endif /* HAVE_SUBSHELL_SUPPORT */
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
do_cd (const vfs_path_t * new_dir_vpath, enum cd_enum exact)
{
    gboolean res;
    const vfs_path_t *_new_dir_vpath = new_dir_vpath;

    if (current_panel->is_panelized)
    {
        size_t new_vpath_len;

        new_vpath_len = vfs_path_len (new_dir_vpath);
        if (vfs_path_ncmp (new_dir_vpath, panelized_panel.root_vpath, new_vpath_len) == 0)
            _new_dir_vpath = panelized_panel.root_vpath;
    }

    res = do_panel_cd (current_panel, _new_dir_vpath, exact);

#ifdef HAVE_CHARSET
    if (res)
    {
        const vfs_path_element_t *path_element;

        path_element = vfs_path_get_by_index (current_panel->cwd_vpath, -1);
        if (path_element->encoding != NULL)
            current_panel->codepage = get_codepage_index (path_element->encoding);
        else
            current_panel->codepage = SELECT_CHARSET_NO_TRANSLATE;
    }
#endif /* HAVE_CHARSET */

    return res;
}

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_SUBSHELL_SUPPORT
gboolean
do_load_prompt (void)
{
    gboolean ret = FALSE;

    if (!read_subshell_prompt ())
        return ret;

    /* Don't actually change the prompt if it's invisible */
    if (top_dlg != NULL && ((Dlg_head *) top_dlg->data == midnight_dlg) && command_prompt)
    {
        setup_cmdline ();

        /* since the prompt has changed, and we are called from one of the
         * tty_get_event channels, the prompt updating does not take place
         * automatically: force a cursor update and a screen refresh
         */
        update_cursor (midnight_dlg);
        mc_refresh ();
        ret = TRUE;
    }
    update_subshell_prompt = TRUE;
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

int
load_prompt (int fd, void *unused)
{
    (void) fd;
    (void) unused;

    do_load_prompt ();
    return 0;
}
#endif /* HAVE_SUBSHELL_SUPPORT */

/* --------------------------------------------------------------------------------------------- */

void
title_path_prepare (char **path, char **login)
{

    char host[BUF_TINY];
    struct passwd *pw = NULL;
    int res = 0;

    *login = NULL;


    *path =
        vfs_path_to_str_flags (current_panel->cwd_vpath, 0, VPF_STRIP_HOME | VPF_STRIP_PASSWORD);
    res = gethostname (host, sizeof (host));
    if (res)
    {                           /* On success, res = 0 */
        host[0] = '\0';
    }
    else
    {
        host[sizeof (host) - 1] = '\0';
    }
    pw = getpwuid (getuid ());
    if (pw)
    {
        *login = g_strdup_printf ("%s@%s", pw->pw_name, host);
    }
    else
    {
        *login = g_strdup (host);
    }
}

/* --------------------------------------------------------------------------------------------- */

/** Show current directory in the xterm title */
void
update_xterm_title_path (void)
{
    char *p;
    char *path;
    char *login;

    if (!(mc_global.tty.xterm_flag && xterm_title))
        return;

    title_path_prepare (&path, &login);

    p = g_strdup_printf ("mc [%s]:%s", login, path);
    fprintf (stdout, "\33]0;%s\7", str_term_form (p));
    g_free (login);
    g_free (p);
    if (!mc_global.tty.alternate_plus_minus)
        numeric_keypad_mode ();
    (void) fflush (stdout);
    g_free (path);
}

/* --------------------------------------------------------------------------------------------- */

int
main (int argc, char *argv[])
{
    GError *error = NULL;
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
        g_free (shell);
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
    if (error == NULL && mc_config_deprecated_dir_present ())
        mc_config_migrate_from_old_place (&error);
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

#ifdef HAVE_SUBSHELL_SUPPORT
    /* Don't use subshell when invoked as viewer or editor */
    if (mc_global.mc_run_mode != MC_RUN_FULL)
        mc_global.tty.use_subshell = FALSE;

    if (mc_global.tty.use_subshell)
        subshell_get_console_attributes ();
#endif /* HAVE_SUBSHELL_SUPPORT */

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

    mc_filehighlight = mc_fhl_new (TRUE);
    dlg_set_default_colors ();

#ifdef HAVE_SUBSHELL_SUPPORT
    /* Done here to ensure that the subshell doesn't  */
    /* inherit the file descriptors opened below, etc */
    if (mc_global.tty.use_subshell)
        init_subshell ();

#endif /* HAVE_SUBSHELL_SUPPORT */

    /* Also done after init_subshell, to save any shell init file messages */
    if (mc_global.tty.console_flag != '\0')
        handle_console (CONSOLE_SAVE);

    if (mc_global.tty.alternate_plus_minus)
        application_keypad_mode ();

#ifdef HAVE_SUBSHELL_SUPPORT
    if (mc_global.tty.use_subshell)
    {
        mc_prompt = strip_ctrl_codes (subshell_prompt);
        if (mc_prompt == NULL)
            mc_prompt = (geteuid () == 0) ? "# " : "$ ";
    }
    else
#endif /* HAVE_SUBSHELL_SUPPORT */
        mc_prompt = (geteuid () == 0) ? "# " : "$ ";

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

    mc_fhl_free (&mc_filehighlight);
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
        }
    }
    g_free (last_wd_string);

    g_free (shell);

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

    g_free (mc_run_param0);
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
