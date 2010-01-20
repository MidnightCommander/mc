/*
   Handle command line arguments.

   Copyright (C) 2009 The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

#include <config.h>
#include <stdio.h>


#include "lib/global.h"
#include "lib/tty/tty.h"
#include "src/args.h"
#include "src/strutil.h"
#include "src/textconf.h"

/*** external variables **************************************************************************/

extern int reset_hp_softkeys;
extern int use_subshell;

extern char *mc_home;
extern char *mc_home_alt;

/* colors specified on the command line: they override any other setting */
extern char *command_line_colors;

extern const char *edit_one_file;
extern const char *view_one_file;
/*** global variables ****************************************************************************/

/* If true, show version info and exit */
gboolean mc_args__version = FALSE;

/* If true, assume we are running on an xterm terminal */
gboolean mc_args__force_xterm = FALSE;

gboolean mc_args__nomouse = FALSE;

/* For slow terminals */
gboolean mc_args__slow_terminal = FALSE;

/* If true use +, -, | for line drawing */
gboolean mc_args__ugly_line_drawing = FALSE;

/* Set to force black and white display at program startup */
gboolean mc_args__disable_colors = FALSE;

/* Force colors, only used by Slang */
gboolean mc_args__force_colors = FALSE;

/* Show in specified skin */
char *mc_args__skin = NULL;

char *mc_args__last_wd_file = NULL;

/* when enabled NETCODE, use folowing file as logfile */
char *mc_args__netfs_logfile = NULL;

/* keymap file */
char *mc_args__keymap_file = NULL;

/* Debug level*/
int mc_args__debug_level = 0;


/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static GOptionContext *context;


static gboolean mc_args__nouse_subshell = FALSE;
static gboolean mc_args__show_datadirs = FALSE;

GOptionGroup *main_group;
static const GOptionEntry argument_main_table[] = {
    /* generic options */
    {
     "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
     &mc_args__version,
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

    {
     "printwd", 'P', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
     &mc_args__last_wd_file,
     N_("Print last working directory to specified file"),
     "<file>"
    },

#ifdef HAVE_SUBSHELL_SUPPORT
    {
     "subshell", 'U', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
     &use_subshell,
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
#ifdef USE_NETCODE
    {
     "ftplog", 'l', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
     &mc_args__netfs_logfile,
     N_("Log ftp dialog to specified file"),
     "<file>"
    },
#ifdef ENABLE_VFS_SMB
    {
     "debuglevel", 'D', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,
     &mc_args__debug_level,
     N_("Set debug level"),
     "<integer>"
    },
#endif /* ENABLE_VFS_SMB */
#endif

    /* single file operations */
    {
     "view", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
     &view_one_file,
     N_("Launches the file viewer on a file"),
     "<file>"
    },

    {
     "edit", 'e', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
     &edit_one_file,
     N_("Edits one file"),
     "<file>"
    },

    {NULL}
};

GOptionGroup *terminal_group;
#define ARGS_TERM_OPTIONS 0
static const GOptionEntry argument_terminal_table[] = {

    /* terminal options */
    {
     "xterm", 'x', ARGS_TERM_OPTIONS, G_OPTION_ARG_NONE,
     &mc_args__force_xterm,
     N_("Forces xterm features"),
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
     &mc_args__slow_terminal,
     N_("To run on slow terminals"),
     NULL
    },

    {
     "stickchars", 'a', ARGS_TERM_OPTIONS, G_OPTION_ARG_NONE,
     &mc_args__ugly_line_drawing,
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

    {NULL}
};
#undef ARGS_TERM_OPTIONS

GOptionGroup *color_group;
#define ARGS_COLOR_OPTIONS 0
// #define ARGS_COLOR_OPTIONS G_OPTION_FLAG_IN_MAIN
static const GOptionEntry argument_color_table[] = {
    /* color options */
    {
     "nocolor", 'b', ARGS_COLOR_OPTIONS, G_OPTION_ARG_NONE,
     &mc_args__disable_colors,
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
     &command_line_colors,
     N_("Specifies a color configuration"),
     "<string>"
    },

    {
     "skin", 'S', ARGS_COLOR_OPTIONS, G_OPTION_ARG_STRING,
     &mc_args__skin,
     N_("Show mc with specified skin"),
     "<string>"
    },

    {NULL}
};
#undef ARGS_COLOR_OPTIONS

static gchar *mc_args__loc__colors_string = NULL;
static gchar *mc_args__loc__footer_string = NULL;
static gchar *mc_args__loc__header_string = NULL;
static gchar *mc_args__loc__usage_string = NULL;

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
static void
mc_args_clean_temp_help_strings(void)
{
    g_free(mc_args__loc__colors_string);
    mc_args__loc__colors_string = NULL;

    g_free(mc_args__loc__footer_string);
    mc_args__loc__footer_string = NULL;

    g_free(mc_args__loc__header_string);
    mc_args__loc__header_string = NULL;

    g_free(mc_args__loc__usage_string);
    mc_args__loc__usage_string = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static GOptionGroup *
mc_args_new_color_group(void)
{
    /*
     * FIXME: undocumented keywords: viewunderline, editnormal, editbold,
     * and editmarked.  To preserve translations, lines should be split.
     */

    mc_args__loc__colors_string = g_strdup_printf("%s%s",
	/* TRANSLATORS: don't translate keywords and names of colors */
	_(  "--colors KEYWORD={FORE},{BACK}\n\n"
	    "{FORE} and {BACK} can be omitted, and the default will be used\n"
	    "\n" "Keywords:\n"
	    "   Global:       errors, reverse, gauge, input, viewunderline\n"
	    "   File display: normal, selected, marked, markselect\n"
	    "   Dialog boxes: dnormal, dfocus, dhotnormal, dhotfocus, errdhotnormal,\n"
	    "                 errdhotfocus\n"
	    "   Menus:        menu, menuhot, menusel, menuhotsel\n"
	    "   Editor:       editnormal, editbold, editmarked, editwhitespace,\n"
	    "                 editlinestate\n"),
	/* TRANSLATORS: don't translate keywords and names of colors */
	_(  "   Help:         helpnormal, helpitalic, helpbold, helplink, helpslink\n"
	    "\n" "Colors:\n"
	    "   black, gray, red, brightred, green, brightgreen, brown,\n"
	    "   yellow, blue, brightblue, magenta, brightmagenta, cyan,\n"
	    "   brightcyan, lightgray and white\n\n")
    );

    return g_option_group_new ("color", mc_args__loc__colors_string,
			_("Color options"),NULL, NULL);

}

/* --------------------------------------------------------------------------------------------- */

static gchar *
mc_args_add_usage_info(void)
{
    mc_args__loc__usage_string = g_strdup_printf("[%s] %s\n %s - %s\n",
	_("+number"),
	_("[this_dir] [other_panel_dir]"),
	_("+number"),
	_("Set initial line number for the internal editor")
    );
    return mc_args__loc__usage_string;
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_args_add_extended_info_to_help(void)
{
    mc_args__loc__footer_string = g_strdup_printf("%s",
	_
	   ("\n"
	    "Please send any bug reports (including the output of `mc -V')\n"
	    "to mc-devel@gnome.org\n")
    );
    mc_args__loc__header_string = g_strdup_printf (_("GNU Midnight Commander %s\n"), VERSION);

#if GLIB_CHECK_VERSION(2,12,0)
    g_option_context_set_description (context, mc_args__loc__footer_string);
    g_option_context_set_summary (context, mc_args__loc__header_string);
#endif

}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_args_process(void)
{
    if (mc_args__version){
	show_version ();
	return FALSE;
    }
    if (mc_args__show_datadirs){
	printf ("%s (%s)\n", mc_home, mc_home_alt);
	return FALSE;
    }

    if (mc_args__force_colors)
	mc_args__disable_colors = FALSE;

#ifdef HAVE_SUBSHELL_SUPPORT
    if (mc_args__nouse_subshell)
	use_subshell = 0;

    if (mc_args__nouse_subshell)
	use_subshell = 0;
#endif				/* HAVE_SUBSHELL_SUPPORT */

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gchar *
mc_args__convert_help_to_syscharset(const gchar *charset, const gchar *error_message, const gchar *help_str)
{
    GString *buffer = g_string_new("");
    GIConv conv = g_iconv_open ( charset, "UTF-8");
    gchar *full_help_str = g_strdup_printf("%s\n\n%s\n",error_message,help_str);

    str_convert (conv, full_help_str, buffer);

    g_free(full_help_str);
    g_iconv_close (conv);

    return g_string_free(buffer, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_args_handle(int *argc, char ***argv, const gchar *translation_domain)
{
    GError *error = NULL;
    const gchar *_system_codepage = str_detect_termencoding();

#ifdef ENABLE_NLS
    if (!str_isutf8 (_system_codepage))
	bind_textdomain_codeset ("mc", "UTF-8");
#endif

    context = g_option_context_new (mc_args_add_usage_info());

    g_option_context_set_ignore_unknown_options (context, FALSE);

    mc_args_add_extended_info_to_help();

    main_group = g_option_group_new ("main", _("Main options"),
			_("Main options"),NULL, NULL);

    g_option_group_add_entries (main_group, argument_main_table);
    g_option_context_set_main_group (context, main_group);
    g_option_group_set_translation_domain(main_group, translation_domain);


    terminal_group = g_option_group_new ("terminal", _("Terminal options"),
			_("Terminal options"),NULL, NULL);

    g_option_group_add_entries (terminal_group, argument_terminal_table);
    g_option_context_add_group (context, terminal_group);
    g_option_group_set_translation_domain(terminal_group, translation_domain);


    color_group = mc_args_new_color_group();

    g_option_group_add_entries (color_group, argument_color_table);
    g_option_context_add_group (context, color_group);
    g_option_group_set_translation_domain(color_group, translation_domain);

    if (! g_option_context_parse (context, argc, argv, &error)) {
	if (error != NULL)
	{
	    gchar *full_help_str;
	    gchar *help_str;

#if GLIB_CHECK_VERSION(2,14,0)
	    help_str = g_option_context_get_help  (context, TRUE, NULL);
#else
	    help_str = g_strdup("");
#endif
	    if ( !str_isutf8 (_system_codepage))
		full_help_str = mc_args__convert_help_to_syscharset(_system_codepage,error->message, help_str);
	    else 
		full_help_str = g_strdup_printf("%s\n\n%s\n",error->message,help_str);

	    fprintf(stderr, "%s",full_help_str);

	    g_free(help_str);
	    g_free(full_help_str);
	    g_error_free (error);
	}
	g_option_context_free (context);
	mc_args_clean_temp_help_strings();
	return FALSE;
    }

    g_option_context_free (context);
    mc_args_clean_temp_help_strings();

#ifdef ENABLE_NLS
    if (!str_isutf8 (_system_codepage))
	bind_textdomain_codeset ("mc", _system_codepage);
#endif

    return mc_args_process();
}

/* --------------------------------------------------------------------------------------------- */
