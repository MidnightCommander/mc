/*
   Handle command line arguments.

   Copyright (C) 2009, 2011
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.

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

#include <config.h>
#include <stdlib.h>
#include <stdio.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/strutil.h"
#include "lib/vfs/vfs.h"
#include "lib/util.h"           /* x_basename() */

#ifdef ENABLE_VFS_SMB
#include "src/vfs/smbfs/smbfs.h"        /* smbfs_set_debugf()  */
#endif

#include "src/main.h"
#include "src/textconf.h"

#include "src/args.h"

/*** external variables **************************************************************************/

/*** global variables ****************************************************************************/

/* If true, show version info and exit */
gboolean mc_args__show_version = FALSE;

/* If true, assume we are running on an xterm terminal */
gboolean mc_args__force_xterm = FALSE;

gboolean mc_args__nomouse = FALSE;

/* Force colors, only used by Slang */
gboolean mc_args__force_colors = FALSE;

/* Don't load keymap form file and use default one */
gboolean mc_args__nokeymap = FALSE;

/* Line to start the editor on */
int mc_args__edit_start_line = 0;

char *mc_args__last_wd_file = NULL;

/* when enabled NETCODE, use folowing file as logfile */
char *mc_args__netfs_logfile = NULL;

/* keymap file */
char *mc_args__keymap_file = NULL;

/* Debug level */
int mc_args__debug_level = 0;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* forward declarations */
static gboolean parse_mc_e_argument (const gchar * option_name, const gchar * value,
                                     gpointer data, GError ** error);
static gboolean parse_mc_v_argument (const gchar * option_name, const gchar * value,
                                     gpointer data, GError ** error);

static GOptionContext *context;

static gboolean mc_args__nouse_subshell = FALSE;
static gboolean mc_args__show_datadirs = FALSE;
static gboolean mc_args__show_datadirs_extended = FALSE;
static gboolean mc_args__show_configure_opts = FALSE;

static GOptionGroup *main_group;

static const GOptionEntry argument_main_table[] = {
    /* *INDENT-OFF* */
    /* generic options */
    {
     "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
     &mc_args__show_version,
     N_("Displays the current version"),
     NULL
    },

    /* options for wrappers */
    {
     "datadir", 'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
     &mc_args__show_datadirs,
     N_("Print data directory"),
     NULL
    },

    /* show extended information about used data directories */
    {
     "datadir-info", 'F', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
     &mc_args__show_datadirs_extended,
     N_("Print extended info about used data directories"),
     NULL
    },

    /* show configure options */
    {
     "configure-options", '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
     &mc_args__show_configure_opts,
     N_("Print configure options"),
     NULL
    },

    {
     "printwd", 'P', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
     &mc_args__last_wd_file,
     N_("Print last working directory to specified file"),
     "<file>"
    },

#ifdef HAVE_SUBSHELL_SUPPORT
    {
     "subshell", 'U', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
     &mc_global.tty.use_subshell,
     N_("Enables subshell support (default)"),
     NULL
    },

    {
     "nosubshell", 'u', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
     &mc_args__nouse_subshell,
     N_("Disables subshell support"),
     NULL
    },
#endif

    /* debug options */
#ifdef ENABLE_VFS_FTP
    {
     "ftplog", 'l', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
     &mc_args__netfs_logfile,
     N_("Log ftp dialog to specified file"),
     "<file>"
    },
#endif /* ENABLE_VFS_FTP */
#ifdef ENABLE_VFS_SMB
    {
     "debuglevel", 'D', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,
     &mc_args__debug_level,
     N_("Set debug level"),
     "<integer>"
    },
#endif /* ENABLE_VFS_SMB */

    /* single file operations */
    {
     "view", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_CALLBACK,
     parse_mc_v_argument,
     N_("Launches the file viewer on a file"),
     "<file>"
    },

    {
     "edit", 'e', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_CALLBACK,
     parse_mc_e_argument,
     N_("Edits one file"),
     "<file>"},

    {
     NULL, '\0', 0, 0, NULL, NULL, NULL /* Complete struct initialization */
    }
    /* *INDENT-ON* */
};

GOptionGroup *terminal_group;
#define ARGS_TERM_OPTIONS 0
static const GOptionEntry argument_terminal_table[] = {
    /* *INDENT-OFF* */
    /* terminal options */
    {
     "xterm", 'x', ARGS_TERM_OPTIONS, G_OPTION_ARG_NONE,
     &mc_args__force_xterm,
     N_("Forces xterm features"),
     NULL
    },

    {
     "oldmouse", 'g', ARGS_TERM_OPTIONS, G_OPTION_ARG_NONE,
     &mc_global.tty.old_mouse,
     N_("Tries to use an old highlight mouse tracking"),
     NULL
    },

    {
     "nomouse", 'd', ARGS_TERM_OPTIONS, G_OPTION_ARG_NONE,
     &mc_args__nomouse,
     N_("Disable mouse support in text version"),
     NULL
    },

#ifdef HAVE_SLANG
    {
     "termcap", 't', ARGS_TERM_OPTIONS, G_OPTION_ARG_NONE,
     &SLtt_Try_Termcap,
     N_("Tries to use termcap instead of terminfo"),
     NULL
    },
#endif

    {
     "slow", 's', ARGS_TERM_OPTIONS, G_OPTION_ARG_NONE,
     &mc_global.tty.slow_terminal,
     N_("To run on slow terminals"),
     NULL
    },

    {
     "stickchars", 'a', ARGS_TERM_OPTIONS, G_OPTION_ARG_NONE,
     &mc_global.tty.ugly_line_drawing,
     N_("Use stickchars to draw"),
     NULL
    },

    {
     "resetsoft", 'k', ARGS_TERM_OPTIONS, G_OPTION_ARG_NONE,
     &reset_hp_softkeys,
     N_("Resets soft keys on HP terminals"),
     NULL
    },

    {
     "keymap", 'K', ARGS_TERM_OPTIONS, G_OPTION_ARG_STRING,
     &mc_args__keymap_file,
     N_("Load definitions of key bindings from specified file"),
     "<file>"
    },

    {
     "nokeymap", '\0', ARGS_TERM_OPTIONS, G_OPTION_ARG_NONE,
     &mc_args__nokeymap,
     N_("Don't load definitions of key bindings from file, use defaults"),
     NULL
    },

    {
     NULL, '\0', 0, 0, NULL, NULL, NULL /* Complete struct initialization */
    }
    /* *INDENT-ON* */
};

#undef ARGS_TERM_OPTIONS

GOptionGroup *color_group;
#define ARGS_COLOR_OPTIONS 0
/* #define ARGS_COLOR_OPTIONS G_OPTION_FLAG_IN_MAIN */
static const GOptionEntry argument_color_table[] = {
    /* *INDENT-OFF* */
    /* color options */
    {
     "nocolor", 'b', ARGS_COLOR_OPTIONS, G_OPTION_ARG_NONE,
     &mc_global.tty.disable_colors,
     N_("Requests to run in black and white"),
     NULL
    },

    {
     "color", 'c', ARGS_COLOR_OPTIONS, G_OPTION_ARG_NONE,
     &mc_args__force_colors,
     N_("Request to run in color mode"),
     NULL
    },

    {
     "colors", 'C', ARGS_COLOR_OPTIONS, G_OPTION_ARG_STRING,
     &mc_global.tty.command_line_colors,
     N_("Specifies a color configuration"),
     "<string>"
    },

    {
     "skin", 'S', ARGS_COLOR_OPTIONS, G_OPTION_ARG_STRING,
     &mc_global.skin,
     N_("Show mc with specified skin"),
     "<string>"
    },

    {
     NULL, '\0', 0, 0, NULL, NULL, NULL /* Complete struct initialization */
    }
    /* *INDENT-ON* */
};

#undef ARGS_COLOR_OPTIONS

static gchar *mc_args__loc__colors_string = NULL;
static gchar *mc_args__loc__footer_string = NULL;
static gchar *mc_args__loc__header_string = NULL;
static gchar *mc_args__loc__usage_string = NULL;

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
static void
mc_args_clean_temp_help_strings (void)
{
    g_free (mc_args__loc__colors_string);
    mc_args__loc__colors_string = NULL;

    g_free (mc_args__loc__footer_string);
    mc_args__loc__footer_string = NULL;

    g_free (mc_args__loc__header_string);
    mc_args__loc__header_string = NULL;

    g_free (mc_args__loc__usage_string);
    mc_args__loc__usage_string = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static GOptionGroup *
mc_args_new_color_group (void)
{
/* *INDENT-OFF* */
    /* FIXME: to preserve translations, lines should be split. */
    mc_args__loc__colors_string = g_strdup_printf ("%s\n%s",
                                                   /* TRANSLATORS: don't translate keywords */
                                                   _("--colors KEYWORD={FORE},{BACK},{ATTR}:KEYWORD2=...\n\n"
                                                     "{FORE}, {BACK} and {ATTR} can be omitted, and the default will be used\n"
                                                     "\n Keywords:\n"
                                                     "   Global:       errors, disabled, reverse, gauge, header\n"
                                                     "                 input, inputmark, inputunchanged, commandlinemark\n"
                                                     "                 bbarhotkey, bbarbutton, statusbar\n"
                                                     "   File display: normal, selected, marked, markselect\n"
                                                     "   Dialog boxes: dnormal, dfocus, dhotnormal, dhotfocus, errdhotnormal,\n"
                                                     "                 errdhotfocus\n"
                                                     "   Menus:        menunormal, menuhot, menusel, menuhotsel, menuinactive\n"
                                                     "   Popup menus:  pmenunormal, pmenusel, pmenutitle\n"
                                                     "   Editor:       editnormal, editbold, editmarked, editwhitespace,\n"
                                                     "                 editlinestate\n"
                                                     "   Viewer:       viewbold, viewunderline, viewselected\n"
                                                     "   Help:         helpnormal, helpitalic, helpbold, helplink, helpslink\n"),
                                                   /* TRANSLATORS: don't translate color names and attributes */
                                                   _("Standard Colors:\n"
                                                    "   black, gray, red, brightred, green, brightgreen, brown,\n"
                                                    "   yellow, blue, brightblue, magenta, brightmagenta, cyan,\n"
                                                    "   brightcyan, lightgray and white\n\n"
                                                    "Extended colors, when 256 colors are available:\n"
                                                    "   color16 to color255, or rgb000 to rgb555 and gray0 to gray23\n\n"
                                                    "Attributes:\n"
                                                    "   bold, underline, reverse, blink; append more with '+'\n")
                                                    );
/* *INDENT-ON* */

    return g_option_group_new ("color", mc_args__loc__colors_string,
                               _("Color options"), NULL, NULL);

}

/* --------------------------------------------------------------------------------------------- */

static gchar *
mc_args_add_usage_info (void)
{
    mc_args__loc__usage_string = g_strdup_printf ("[%s] %s\n %s - %s\n",
                                                  _("+number"),
                                                  _("[this_dir] [other_panel_dir]"),
                                                  _("+number"),
                                                  _
                                                  ("Set initial line number for the internal editor"));
    return mc_args__loc__usage_string;
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_args_add_extended_info_to_help (void)
{
    mc_args__loc__footer_string = g_strdup_printf ("%s",
                                                   _
                                                   ("\n"
                                                    "Please send any bug reports (including the output of `mc -V')\n"
                                                    "as tickets at www.midnight-commander.org\n"));
    mc_args__loc__header_string = g_strdup_printf (_("GNU Midnight Commander %s\n"), VERSION);

#if GLIB_CHECK_VERSION(2,12,0)
    g_option_context_set_description (context, mc_args__loc__footer_string);
    g_option_context_set_summary (context, mc_args__loc__header_string);
#endif
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_setup_by_args (int argc, char *argv[])
{
    const char *base;
    char *tmp;

#ifdef ENABLE_VFS_SMB
    if (mc_args__debug_level != 0)
        smbfs_set_debug (mc_args__debug_level);
#endif /* ENABLE_VFS_SMB */

    if (mc_args__netfs_logfile != NULL)
    {
#ifdef ENABLE_VFS_FTP
        mc_setctl ("ftp://", VFS_SETCTL_LOGFILE, (void *) mc_args__netfs_logfile);
#endif /* ENABLE_VFS_FTP */
#ifdef ENABLE_VFS_SMB
        mc_setctl ("smb://", VFS_SETCTL_LOGFILE, (void *) mc_args__netfs_logfile);
#endif /* ENABLE_VFS_SMB */
    }

    base = x_basename (argv[0]);
    tmp = (argc > 0) ? argv[1] : NULL;

    if (strncmp (base, "mce", 3) == 0 || strcmp (base, "vi") == 0)
    {
        /* mce* or vi is link to mc */

        mc_run_param0 = g_strdup ("");
        if (tmp != NULL)
        {
            /*
             * Check for filename:lineno, followed by an optional colon.
             * This format is used by many programs (especially compilers)
             * in error messages and warnings. It is supported so that
             * users can quickly copy and paste file locations.
             */
            char *end, *p;

            end = tmp + strlen (tmp);
            p = end;

            if (p > tmp && p[-1] == ':')
                p--;
            while (p > tmp && g_ascii_isdigit ((gchar) p[-1]))
                p--;
            if (tmp < p && p < end && p[-1] == ':')
            {
                char *fname;
                struct stat st;

                fname = g_strndup (tmp, p - 1 - tmp);
                /*
                 * Check that the file before the colon actually exists.
                 * If it doesn't exist, revert to the old behavior.
                 */
                if (mc_stat (tmp, &st) == -1 && mc_stat (fname, &st) != -1)
                {
                    mc_run_param0 = fname;
                    mc_args__edit_start_line = atoi (p);
                }
                else
                {
                    g_free (fname);
                    goto try_plus_filename;
                }
            }
            else
            {
              try_plus_filename:
                if (*tmp == '+' && g_ascii_isdigit ((gchar) tmp[1]))
                {
                    int start_line;

                    start_line = atoi (tmp);

                    /*
                     * If start_line is zero, position the cursor at the
                     * beginning of the file as other editors (vi, nano)
                     */
                    if (start_line == 0)
                        start_line++;

                    if (start_line > 0)
                    {
                        char *file;

                        file = (argc > 1) ? argv[2] : NULL;
                        if (file != NULL)
                        {
                            tmp = file;
                            mc_args__edit_start_line = start_line;
                        }
                    }
                }
                mc_run_param0 = g_strdup (tmp);
            }
        }
        mc_global.mc_run_mode = MC_RUN_EDITOR;
    }
    else if (strncmp (base, "mcv", 3) == 0 || strcmp (base, "view") == 0)
    {
        /* mcv* or view is link to mc */

        if (tmp != NULL)
            mc_run_param0 = g_strdup (tmp);
        else
        {
            fprintf (stderr, "%s\n", _("No arguments given to the viewer."));
            exit (EXIT_FAILURE);
        }
        mc_global.mc_run_mode = MC_RUN_VIEWER;
    }
#ifdef USE_DIFF_VIEW
    else if (strncmp (base, "mcd", 3) == 0 || strcmp (base, "diff") == 0)
    {
        /* mcd* or diff is link to mc */

        if (argc < 3)
        {
            fprintf (stderr, "%s\n", _("Two files are required to evoke the diffviewer."));
            exit (EXIT_FAILURE);
        }

        if (tmp != NULL)
        {
            mc_run_param0 = g_strdup (tmp);
            tmp = (argc > 1) ? argv[2] : NULL;
            if (tmp != NULL)
                mc_run_param1 = g_strdup (tmp);
            mc_global.mc_run_mode = MC_RUN_DIFFVIEWER;
        }
    }
#endif /* USE_DIFF_VIEW */
    else
    {
        /* MC is run as mc */

        switch (mc_global.mc_run_mode)
        {
        case MC_RUN_EDITOR:
        case MC_RUN_VIEWER:
            /* mc_run_param0 is set up in parse_mc_e_argument() and parse_mc_v_argument() */
            break;

        case MC_RUN_DIFFVIEWER:
            /* not implemented yet */
            break;

        case MC_RUN_FULL:
        default:
            /* sets the current dir and the other dir */
            if (tmp != NULL)
            {
                mc_run_param0 = g_strdup (tmp);
                tmp = (argc > 1) ? argv[2] : NULL;
                if (tmp != NULL)
                    mc_run_param1 = g_strdup (tmp);
            }
            mc_global.mc_run_mode = MC_RUN_FULL;
            break;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_args_process (int argc, char *argv[])
{
    if (mc_args__show_version)
    {
        show_version ();
        return FALSE;
    }
    if (mc_args__show_datadirs)
    {
        printf ("%s (%s)\n", mc_global.sysconfig_dir, mc_global.share_data_dir);
        return FALSE;
    }

    if (mc_args__show_datadirs_extended)
    {
        show_datadirs_extended ();
        return FALSE;
    }

    if (mc_args__show_configure_opts)
    {
        show_configure_options ();
        return FALSE;
    }

    if (mc_args__force_colors)
        mc_global.tty.disable_colors = FALSE;

#ifdef HAVE_SUBSHELL_SUPPORT
    if (mc_args__nouse_subshell)
        mc_global.tty.use_subshell = FALSE;
#endif /* HAVE_SUBSHELL_SUPPORT */

    mc_setup_by_args (argc, argv);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gchar *
mc_args__convert_help_to_syscharset (const gchar * charset, const gchar * error_message,
                                     const gchar * help_str)
{
    GString *buffer = g_string_new ("");
    GIConv conv = g_iconv_open (charset, "UTF-8");
    gchar *full_help_str = g_strdup_printf ("%s\n\n%s\n", error_message, help_str);

    str_convert (conv, full_help_str, buffer);

    g_free (full_help_str);
    g_iconv_close (conv);

    return g_string_free (buffer, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
parse_mc_e_argument (const gchar * option_name, const gchar * value, gpointer data, GError ** error)
{
    (void) option_name;
    (void) data;
    (void) error;

    mc_global.mc_run_mode = MC_RUN_EDITOR;
    mc_run_param0 = g_strdup (value);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
parse_mc_v_argument (const gchar * option_name, const gchar * value, gpointer data, GError ** error)
{
    (void) option_name;
    (void) data;
    (void) error;

    mc_global.mc_run_mode = MC_RUN_VIEWER;
    mc_run_param0 = g_strdup (value);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_args_handle (int argc, char **argv, const char *translation_domain)
{
    GError *error = NULL;
    const gchar *_system_codepage = str_detect_termencoding ();

#ifdef ENABLE_NLS
    if (!str_isutf8 (_system_codepage))
        bind_textdomain_codeset ("mc", "UTF-8");
#endif

    context = g_option_context_new (mc_args_add_usage_info ());

    g_option_context_set_ignore_unknown_options (context, FALSE);

    mc_args_add_extended_info_to_help ();

    main_group = g_option_group_new ("main", _("Main options"), _("Main options"), NULL, NULL);

    g_option_group_add_entries (main_group, argument_main_table);
    g_option_context_set_main_group (context, main_group);
    g_option_group_set_translation_domain (main_group, translation_domain);

    terminal_group = g_option_group_new ("terminal", _("Terminal options"),
                                         _("Terminal options"), NULL, NULL);

    g_option_group_add_entries (terminal_group, argument_terminal_table);
    g_option_context_add_group (context, terminal_group);
    g_option_group_set_translation_domain (terminal_group, translation_domain);

    color_group = mc_args_new_color_group ();

    g_option_group_add_entries (color_group, argument_color_table);
    g_option_context_add_group (context, color_group);
    g_option_group_set_translation_domain (color_group, translation_domain);

    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        if (error != NULL)
        {
            gchar *full_help_str;
            gchar *help_str;

#if GLIB_CHECK_VERSION(2,14,0)
            help_str = g_option_context_get_help (context, TRUE, NULL);
#else
            help_str = g_strdup ("");
#endif
            if (!str_isutf8 (_system_codepage))
                full_help_str =
                    mc_args__convert_help_to_syscharset (_system_codepage, error->message,
                                                         help_str);
            else
                full_help_str = g_strdup_printf ("%s\n\n%s\n", error->message, help_str);

            fprintf (stderr, "%s", full_help_str);

            g_free (help_str);
            g_free (full_help_str);
            g_error_free (error);
        }
        g_option_context_free (context);
        mc_args_clean_temp_help_strings ();
        return FALSE;
    }

    g_option_context_free (context);
    mc_args_clean_temp_help_strings ();

#ifdef ENABLE_NLS
    if (!str_isutf8 (_system_codepage))
        bind_textdomain_codeset ("mc", _system_codepage);
#endif

    return mc_args_process (argc, argv);
}

/* --------------------------------------------------------------------------------------------- */
