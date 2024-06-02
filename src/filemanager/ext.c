/*
   Extension dependent execution.

   Copyright (C) 1994-2024
   Free Software Foundation, Inc.

   Written by:
   Jakub Jelinek, 1995
   Miguel de Icaza, 1994
   Slava Zanko <slavazanko@gmail.com>, 2013

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

/** \file  ext.c
 *  \brief Source: extension dependent execution
 */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/search.h"
#include "lib/fileloc.h"
#include "lib/mcconfig.h"
#include "lib/util.h"
#include "lib/vfs/vfs.h"
#include "lib/widget.h"
#ifdef HAVE_CHARSET
#include "lib/charsets.h"       /* get_codepage_index */
#endif

#ifdef USE_FILE_CMD
#include "src/setup.h"          /* use_file_to_check_type */
#endif
#include "src/execute.h"
#include "src/history.h"
#include "src/usermenu.h"

#include "src/consaver/cons.saver.h"
#include "src/viewer/mcviewer.h"

#ifdef HAVE_CHARSET
#include "src/selcodepage.h"    /* do_set_codepage */
#endif

#include "filemanager.h"        /* current_panel */
#include "panel.h"              /* panel_cd */

#include "ext.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#ifdef USE_FILE_CMD
#ifdef FILE_B
#define FILE_CMD "file -z " FILE_B FILE_S FILE_L
#else
#define FILE_CMD "file -z " FILE_S FILE_L
#endif
#endif

/*** file scope type declarations ****************************************************************/

typedef char *(*quote_func_t) (const char *name, gboolean quote_percent);

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* This variable points to a copy of the mc.ext file in memory
 * With this we avoid loading/parsing the file each time we
 * need it
 */
static mc_config_t *ext_ini = NULL;
static gchar **ext_ini_groups = NULL;
static vfs_path_t *localfilecopy_vpath = NULL;
static char buffer[BUF_1K];

static char *pbuffer = NULL;
static time_t localmtime = 0;
static quote_func_t quote_func = name_quote;
static gboolean run_view = FALSE;
static gboolean is_cd = FALSE;
static gboolean written_nonspace = FALSE;
static gboolean do_local_copy = FALSE;

static const char *descr_group = "mc.ext.ini";
static const char *default_group = "Default";

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
exec_cleanup_script (vfs_path_t *script_vpath)
{
    if (script_vpath != NULL)
    {
        (void) mc_unlink (script_vpath);
        vfs_path_free (script_vpath, TRUE);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
exec_cleanup_file_name (const vfs_path_t *filename_vpath, gboolean has_changed)
{
    if (localfilecopy_vpath == NULL)
        return;

    if (has_changed)
    {
        struct stat mystat;

        mc_stat (localfilecopy_vpath, &mystat);
        has_changed = localmtime != mystat.st_mtime;
    }
    mc_ungetlocalcopy (filename_vpath, localfilecopy_vpath, has_changed);
    vfs_path_free (localfilecopy_vpath, TRUE);
    localfilecopy_vpath = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static char *
exec_get_file_name (const vfs_path_t *filename_vpath)
{
    if (!do_local_copy)
        return quote_func (vfs_path_get_last_path_str (filename_vpath), FALSE);

    if (localfilecopy_vpath == NULL)
    {
        struct stat mystat;
        localfilecopy_vpath = mc_getlocalcopy (filename_vpath);
        if (localfilecopy_vpath == NULL)
            return NULL;

        mc_stat (localfilecopy_vpath, &mystat);
        localmtime = mystat.st_mtime;
    }

    return quote_func (vfs_path_get_last_path_str (localfilecopy_vpath), FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static char *
exec_expand_format (char symbol, gboolean is_result_quoted)
{
    char *text;

    text = expand_format (NULL, symbol, TRUE);
    if (is_result_quoted && text != NULL)
    {
        char *quoted_text;

        quoted_text = g_strdup_printf ("\"%s\"", text);
        g_free (text);
        text = quoted_text;
    }
    return text;
}

/* --------------------------------------------------------------------------------------------- */

static GString *
exec_get_export_variables (const vfs_path_t *filename_vpath)
{
    char *text;
    GString *export_vars_string;
    size_t i;

    /* *INDENT-OFF* */
    struct
    {
        const char symbol;
        const char *name;
        const gboolean is_result_quoted;
    } export_variables[] = {
        {'p', "MC_EXT_BASENAME", FALSE},
        {'d', "MC_EXT_CURRENTDIR", FALSE},
        {'s', "MC_EXT_SELECTED", TRUE},
        {'t', "MC_EXT_ONLYTAGGED", TRUE},
        {'\0', NULL, FALSE}
    };
    /* *INDENT-ON* */

    text = exec_get_file_name (filename_vpath);
    if (text == NULL)
        return NULL;

    export_vars_string = g_string_new ("MC_EXT_FILENAME=");
    g_string_append_printf (export_vars_string, "%s\nexport MC_EXT_FILENAME\n", text);
    g_free (text);

    for (i = 0; export_variables[i].name != NULL; i++)
    {
        text =
            exec_expand_format (export_variables[i].symbol, export_variables[i].is_result_quoted);
        if (text != NULL)
        {
            g_string_append_printf (export_vars_string,
                                    "%s=%s\nexport %s\n", export_variables[i].name, text,
                                    export_variables[i].name);
            g_free (text);
        }
    }

    return export_vars_string;
}

/* --------------------------------------------------------------------------------------------- */

static GString *
exec_make_shell_string (const char *lc_data, const vfs_path_t *filename_vpath)
{
    GString *shell_string;
    char lc_prompt[80] = "\0";
    gboolean parameter_found = FALSE;
    gboolean expand_prefix_found = FALSE;

    shell_string = g_string_new ("");

    for (; *lc_data != '\0' && *lc_data != '\n'; lc_data++)
    {
        if (parameter_found)
        {
            if (*lc_data == '}')
            {
                char *parameter;

                parameter_found = FALSE;
                parameter =
                    input_dialog (_("Parameter"), lc_prompt, MC_HISTORY_EXT_PARAMETER, "",
                                  INPUT_COMPLETE_NONE);
                if (parameter == NULL)
                {
                    /* User canceled */
                    g_string_free (shell_string, TRUE);
                    exec_cleanup_file_name (filename_vpath, FALSE);
                    return NULL;
                }
                g_string_append (shell_string, parameter);
                written_nonspace = TRUE;
                g_free (parameter);
            }
            else
            {
                size_t len = strlen (lc_prompt);

                if (len < sizeof (lc_prompt) - 1)
                {
                    lc_prompt[len] = *lc_data;
                    lc_prompt[len + 1] = '\0';
                }
            }
        }
        else if (expand_prefix_found)
        {
            expand_prefix_found = FALSE;
            if (*lc_data == '{')
                parameter_found = TRUE;
            else
            {
                int i;

                i = check_format_view (lc_data);
                if (i != 0)
                {
                    lc_data += i - 1;
                    run_view = TRUE;
                }
                else
                {
                    i = check_format_cd (lc_data);
                    if (i > 0)
                    {
                        is_cd = TRUE;
                        quote_func = fake_name_quote;
                        do_local_copy = FALSE;
                        pbuffer = buffer;
                        lc_data += i - 1;
                    }
                    else
                    {
                        char *v;

                        i = check_format_var (lc_data, &v);
                        if (i > 0)
                        {
                            g_string_append (shell_string, v);
                            g_free (v);
                            lc_data += i;
                        }
                        else
                        {
                            char *text;

                            if (*lc_data != 'f')
                                text = expand_format (NULL, *lc_data, !is_cd);
                            else
                            {
                                text = exec_get_file_name (filename_vpath);
                                if (text == NULL)
                                {
                                    g_string_free (shell_string, TRUE);
                                    return NULL;
                                }
                            }

                            if (text != NULL)
                            {
                                if (!is_cd)
                                    g_string_append (shell_string, text);
                                else
                                {
                                    strcpy (pbuffer, text);
                                    pbuffer = strchr (pbuffer, '\0');
                                }

                                g_free (text);
                            }

                            written_nonspace = TRUE;
                        }
                    }
                }
            }
        }
        else if (*lc_data == '%')
            expand_prefix_found = TRUE;
        else
        {
            if (!whitespace (*lc_data))
                written_nonspace = TRUE;
            if (is_cd)
                *(pbuffer++) = *lc_data;
            else
                g_string_append_c (shell_string, *lc_data);
        }
    }                           /* for */

    return shell_string;
}

/* --------------------------------------------------------------------------------------------- */

static void
exec_extension_view (void *target, char *cmd, const vfs_path_t *filename_vpath, int start_line)
{
    mcview_mode_flags_t def_flags = {
        /* *INDENT-OFF* */
        .wrap = FALSE,
        .hex = mcview_global_flags.hex,
        .magic = FALSE,
        .nroff = mcview_global_flags.nroff
        /* *INDENT-ON* */
    };

    mcview_mode_flags_t changed_flags;

    mcview_clear_mode_flags (&changed_flags);
    mcview_altered_flags.hex = FALSE;
    mcview_altered_flags.nroff = FALSE;
    if (def_flags.hex != mcview_global_flags.hex)
        changed_flags.hex = TRUE;
    if (def_flags.nroff != mcview_global_flags.nroff)
        changed_flags.nroff = TRUE;

    if (target == NULL)
        mcview_viewer (cmd, filename_vpath, start_line, 0, 0);
    else
        mcview_load ((WView *) target, cmd, vfs_path_as_str (filename_vpath), start_line, 0, 0);

    if (changed_flags.hex && !mcview_altered_flags.hex)
        mcview_global_flags.hex = def_flags.hex;
    if (changed_flags.nroff && !mcview_altered_flags.nroff)
        mcview_global_flags.nroff = def_flags.nroff;

    dialog_switch_process_pending ();
}

/* --------------------------------------------------------------------------------------------- */

static void
exec_extension_cd (WPanel *panel)
{
    char *q;
    vfs_path_t *p_vpath;

    *pbuffer = '\0';
    pbuffer = buffer;
    /* Search last non-space character. Start search at the end in order
       not to short filenames containing spaces. */
    q = pbuffer + strlen (pbuffer) - 1;
    while (q >= pbuffer && whitespace (*q))
        q--;
    q[1] = 0;

    p_vpath = vfs_path_from_str_flags (pbuffer, VPF_NO_CANON);
    panel_cd (panel, p_vpath, cd_parse_command);
    vfs_path_free (p_vpath, TRUE);
}


/* --------------------------------------------------------------------------------------------- */

static vfs_path_t *
exec_extension (WPanel *panel, void *target, const vfs_path_t *filename_vpath,
                const char *lc_data, int start_line)
{
    GString *shell_string, *export_variables;
    vfs_path_t *script_vpath = NULL;
    int cmd_file_fd;
    FILE *cmd_file;
    char *cmd = NULL;

    pbuffer = NULL;
    localmtime = 0;
    quote_func = name_quote;
    run_view = FALSE;
    is_cd = FALSE;
    written_nonspace = FALSE;

    /* Avoid making a local copy if we are doing a cd */
    do_local_copy = !vfs_file_is_local (filename_vpath);

    shell_string = exec_make_shell_string (lc_data, filename_vpath);
    if (shell_string == NULL)
        goto ret;

    if (is_cd)
    {
        exec_extension_cd (panel);
        g_string_free (shell_string, TRUE);
        goto ret;
    }

    /*
     * All commands should be run in /bin/sh regardless of user shell.
     * To do that, create temporary shell script and run it.
     * Sometimes it's not needed (e.g. for %cd and %view commands),
     * but it's easier to create it anyway.
     */
    cmd_file_fd = mc_mkstemps (&script_vpath, "mcext", SCRIPT_SUFFIX);

    if (cmd_file_fd == -1)
    {
        message (D_ERROR, MSG_ERROR,
                 _("Cannot create temporary command file\n%s"), unix_error_string (errno));
        g_string_free (shell_string, TRUE);
        goto ret;
    }

    cmd_file = fdopen (cmd_file_fd, "w");
    fputs ("#! /bin/sh\n\n", cmd_file);

    export_variables = exec_get_export_variables (filename_vpath);
    if (export_variables != NULL)
    {
        fputs (export_variables->str, cmd_file);
        g_string_free (export_variables, TRUE);
    }

    fputs (shell_string->str, cmd_file);
    g_string_free (shell_string, TRUE);

    /*
     * Make the script remove itself when it finishes.
     * Don't do it for the viewer - it may need to rerun the script,
     * so we clean up after calling view().
     */
    if (!run_view)
        fprintf (cmd_file, "\n/bin/rm -f %s\n", vfs_path_as_str (script_vpath));

    fclose (cmd_file);

    if ((run_view && !written_nonspace) || is_cd)
    {
        exec_cleanup_script (script_vpath);
        script_vpath = NULL;
    }
    else
    {
        /* Set executable flag on the command file ... */
        mc_chmod (script_vpath, S_IRWXU);
        /* ... but don't rely on it - run /bin/sh explicitly */
        cmd = g_strconcat ("/bin/sh ", vfs_path_as_str (script_vpath), (char *) NULL);
    }

    if (run_view)
    {
        /* If we've written whitespace only, then just load filename into view */
        if (!written_nonspace)
            exec_extension_view (target, NULL, filename_vpath, start_line);
        else
            exec_extension_view (target, cmd, filename_vpath, start_line);
    }
    else
    {
        shell_execute (cmd, EXECUTE_INTERNAL);
        if (mc_global.tty.console_flag != '\0')
        {
            handle_console (CONSOLE_SAVE);
            if (output_lines != 0 && mc_global.keybar_visible)
            {
                unsigned char end_line;

                end_line = LINES - (mc_global.keybar_visible ? 1 : 0) - 1;
                show_console_contents (output_start_y, end_line - output_lines, end_line);
            }
        }
    }

    g_free (cmd);

    exec_cleanup_file_name (filename_vpath, TRUE);
  ret:
    return script_vpath;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Run cmd_file with args, put result into buf.
 * If error, put '\0' into buf[0]
 * Return 1 if the data is valid, 0 otherwise, -1 for fatal errors.
 *
 * NOTES: buf is null-terminated string.
 */

#ifdef USE_FILE_CMD
static int
get_popen_information (const char *cmd_file, const char *args, char *buf, int buflen)
{
    gboolean read_bytes = FALSE;
    char *command;
    FILE *f;

    command = g_strconcat (cmd_file, args, " 2>/dev/null", (char *) NULL);
    f = popen (command, "r");
    g_free (command);

    if (f != NULL)
    {
#ifdef __QNXNTO__
        if (setvbuf (f, NULL, _IOFBF, 0) != 0)
        {
            (void) pclose (f);
            return -1;
        }
#endif
        read_bytes = (fgets (buf, buflen, f) != NULL);
        if (!read_bytes)
            buf[0] = '\0';      /* Paranoid termination */
        pclose (f);
    }
    else
    {
        buf[0] = '\0';          /* Paranoid termination */
        return -1;
    }

    buf[buflen - 1] = '\0';

    return read_bytes ? 1 : 0;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Run the "file" command on the local file.
 * Return 1 if the data is valid, 0 otherwise, -1 for fatal errors.
 */

static int
get_file_type_local (const vfs_path_t *filename_vpath, char *buf, int buflen)
{
    char *filename_quoted;
    int ret = 0;

    filename_quoted = name_quote (vfs_path_get_last_path_str (filename_vpath), FALSE);
    if (filename_quoted != NULL)
    {
        ret = get_popen_information (FILE_CMD, filename_quoted, buf, buflen);
        g_free (filename_quoted);
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Run the "enca" command on the local file.
 * Return 1 if the data is valid, 0 otherwise, -1 for fatal errors.
 */

#ifdef HAVE_CHARSET
static int
get_file_encoding_local (const vfs_path_t *filename_vpath, char *buf, int buflen)
{
    char *filename_quoted;
    int ret = 0;

    filename_quoted = name_quote (vfs_path_get_last_path_str (filename_vpath), FALSE);
    if (filename_quoted != NULL)
    {
        char *lang;

        lang = name_quote (autodetect_codeset, FALSE);
        if (lang != NULL)
        {
            char *args;

            args = g_strdup_printf (" -L %s -i %s", lang, filename_quoted);
            g_free (lang);

            ret = get_popen_information ("enca", args, buf, buflen);
            g_free (args);
        }

        g_free (filename_quoted);
    }

    return ret;
}
#endif /* HAVE_CHARSET */

/* --------------------------------------------------------------------------------------------- */
/**
 * Invoke the "file" command on the file and match its output against PTR.
 * have_type is a flag that is set if we already have tried to determine
 * the type of that file.
 * Return TRUE for match, FALSE otherwise.
 */

static gboolean
regex_check_type (const vfs_path_t *filename_vpath, const char *ptr, gboolean case_insense,
                  gboolean *have_type, GError **mcerror)
{
    gboolean found = FALSE;

    /* Following variables are valid if *have_type is TRUE */
    static char content_string[2048];
    static size_t content_shift = 0;
    static int got_data = 0;

    mc_return_val_if_error (mcerror, FALSE);

    if (!*have_type)
    {
        vfs_path_t *localfile_vpath;

#ifdef HAVE_CHARSET
        static char encoding_id[21];    /* CSISO51INISCYRILLIC -- 20 */
        int got_encoding_data;
#endif /* HAVE_CHARSET */

        /* Don't repeate even unsuccessful checks */
        *have_type = TRUE;

        localfile_vpath = mc_getlocalcopy (filename_vpath);
        if (localfile_vpath == NULL)
        {
            mc_propagate_error (mcerror, 0, _("Cannot fetch a local copy of %s"),
                                vfs_path_as_str (filename_vpath));
            return FALSE;
        }


#ifdef HAVE_CHARSET
        got_encoding_data = is_autodetect_codeset_enabled
            ? get_file_encoding_local (localfile_vpath, encoding_id, sizeof (encoding_id)) : 0;

        if (got_encoding_data > 0)
        {
            char *pp;
            int cp_id;

            pp = strchr (encoding_id, '\n');
            if (pp != NULL)
                *pp = '\0';

            cp_id = get_codepage_index (encoding_id);
            if (cp_id == -1)
                cp_id = default_source_codepage;

            do_set_codepage (cp_id);
        }
#endif /* HAVE_CHARSET */

        got_data = get_file_type_local (localfile_vpath, content_string, sizeof (content_string));

        mc_ungetlocalcopy (filename_vpath, localfile_vpath, FALSE);

        if (got_data > 0)
        {
            char *pp;

            pp = strchr (content_string, '\n');
            if (pp != NULL)
                *pp = '\0';

#ifndef FILE_B
            {
                const char *real_name;  /* name used with "file" */
                size_t real_len;

                real_name = vfs_path_get_last_path_str (localfile_vpath);
                real_len = strlen (real_name);

                if (strncmp (content_string, real_name, real_len) == 0)
                {
                    /* Skip "real_name: " */
                    content_shift = real_len;

                    /* Solaris' file prints tab(s) after ':' */
                    if (content_string[content_shift] == ':')
                        for (content_shift++; whitespace (content_string[content_shift]);
                             content_shift++)
                            ;
                }
            }
#endif /* FILE_B */
        }
        else
        {
            /* No data */
            content_string[0] = '\0';
        }
        vfs_path_free (localfile_vpath, TRUE);
    }

    if (got_data == -1)
    {
        mc_propagate_error (mcerror, 0, "%s", _("Pipe failed"));
        return FALSE;
    }

    if (content_string[0] != '\0')
    {
        mc_search_t *search;

        search = mc_search_new (ptr, DEFAULT_CHARSET);
        if (search != NULL)
        {
            search->search_type = MC_SEARCH_T_REGEX;
            search->is_case_sensitive = !case_insense;
            found = mc_search_run (search, content_string + content_shift, 0, -1, NULL);
            mc_search_free (search);
        }
        else
        {
            mc_propagate_error (mcerror, 0, "%s", _("Regular expression error"));
        }
    }

    return found;
}
#endif /* USE_FILE_CMD */

/* --------------------------------------------------------------------------------------------- */

static void
check_old_extension_file (void)
{
    char *extension_old_file;

    extension_old_file = mc_config_get_full_path (MC_EXT_OLD_FILE);
    if (exist_file (extension_old_file))
        message (D_ERROR, _("Warning"),
                 _("You have an outdated %s file.\nMidnight Commander now uses %s file.\n"
                   "Please copy your modifications of the old file to the new one."),
                 extension_old_file, MC_EXT_FILE);
    g_free (extension_old_file);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
load_extension_file (void)
{
    char *extension_file;
    gboolean mc_user_ext = TRUE;
    gboolean home_error = FALSE;

    extension_file = mc_config_get_full_path (MC_EXT_FILE);
    if (!exist_file (extension_file))
    {
        g_free (extension_file);

        check_old_extension_file ();

      check_stock_mc_ext:
        extension_file = mc_build_filename (mc_global.sysconfig_dir, MC_EXT_FILE, (char *) NULL);
        if (!exist_file (extension_file))
        {
            g_free (extension_file);
            extension_file =
                mc_build_filename (mc_global.share_data_dir, MC_EXT_FILE, (char *) NULL);
            if (!exist_file (extension_file))
                MC_PTR_FREE (extension_file);
        }
        mc_user_ext = FALSE;
    }

    if (extension_file != NULL)
    {
        ext_ini = mc_config_init (extension_file, TRUE);
        g_free (extension_file);
    }
    if (ext_ini == NULL)
        return FALSE;

    /* Check version */
    if (!mc_config_has_group (ext_ini, descr_group))
    {
        flush_extension_file ();

        if (!mc_user_ext)
        {
            message (D_ERROR, MSG_ERROR,
                     _("The format of the\n%s%s\nfile has changed with version 4.0.\n"
                       "It seems that the installation has failed.\nPlease fetch a fresh copy "
                       "from the Midnight Commander package."),
                     mc_global.sysconfig_dir, MC_EXT_FILE);
            return FALSE;
        }

        home_error = TRUE;
        goto check_stock_mc_ext;
    }

    if (home_error)
    {
        extension_file = mc_config_get_full_path (MC_EXT_FILE);
        message (D_ERROR, MSG_ERROR,
                 _("The format of the\n%s\nfile has changed with version 4.0.\nYou may either want "
                   "to copy it from\n%s%s\nor use that file as an example of how to write it."),
                 extension_file, mc_global.sysconfig_dir, MC_EXT_FILE);
        g_free (extension_file);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
flush_extension_file (void)
{
    g_strfreev (ext_ini_groups);
    ext_ini_groups = NULL;

    mc_config_deinit (ext_ini);
    ext_ini = NULL;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * The second argument is action, i.e. Open, View or Edit
 * Use target object to open file in.
 *
 * This function returns:
 *
 * -1 for a failure or user interrupt
 * 0 if no command was run
 * 1 if some command was run
 *
 * If action == "View" then a parameter is checked in the form of "View:%d",
 * if the value for %d exists, then the viewer is started up at that line number.
 */

int
regex_command_for (void *target, const vfs_path_t *filename_vpath, const char *action,
                   vfs_path_t **script_vpath)
{
    const char *filename;
    size_t filename_len;
    gboolean found = FALSE;
    gboolean error_flag = FALSE;
    int ret = 0;
    struct stat mystat;
    int view_at_line_number = 0;
#ifdef USE_FILE_CMD
    gboolean have_type = FALSE; /* Flag used by regex_check_type() */
#endif
    char **group_iter;
    char *include_group = NULL;
    const char *current_group;

    if (filename_vpath == NULL)
        return 0;

    if (script_vpath != NULL)
        *script_vpath = NULL;

    /* Check for the special View:%d parameter */
    if (strncmp (action, "View:", 5) == 0)
    {
        view_at_line_number = atoi (action + 5);
        action = "View";
    }

    if (ext_ini == NULL && !load_extension_file ())
        return 0;

    mc_stat (filename_vpath, &mystat);

    filename = vfs_path_get_last_path_str (filename_vpath);
    filename = x_basename (filename);
    filename_len = strlen (filename);

    if (ext_ini_groups == NULL)
        ext_ini_groups = mc_config_get_groups (ext_ini, NULL);

    /* find matched type, regex or shell pattern */
    for (group_iter = ext_ini_groups; *group_iter != NULL && !found; group_iter++)
    {
        enum
        {
            TYPE_UNUSED,
            TYPE_NOT_FOUND,
            TYPE_FOUND
        } type_state = TYPE_UNUSED;

        const gchar *g = *group_iter;
        gchar *pattern;
        gboolean ignore_case;

        if (strcmp (g, descr_group) == 0 || strncmp (g, "Include/", 8) == 0
            || strcmp (g, default_group) == 0)
            continue;

        /* The "Directory" parameter is a special case: if it's present then
           "Type", "Regex", and "Shell" parameters are ignored */
        pattern = mc_config_get_string_raw (ext_ini, g, "Directory", NULL);
        if (pattern != NULL)
        {
            found = S_ISDIR (mystat.st_mode)
                && mc_search (pattern, DEFAULT_CHARSET, vfs_path_as_str (filename_vpath),
                              MC_SEARCH_T_REGEX);
            g_free (pattern);

            continue;           /* stop if found */
        }

#ifdef USE_FILE_CMD
        if (use_file_to_check_type)
        {
            pattern = mc_config_get_string_raw (ext_ini, g, "Type", NULL);
            if (pattern != NULL)
            {
                GError *mcerror = NULL;

                ignore_case = mc_config_get_bool (ext_ini, g, "TypeIgnoreCase", FALSE);
                type_state =
                    regex_check_type (filename_vpath, pattern, ignore_case, &have_type, &mcerror)
                    ? TYPE_FOUND : TYPE_NOT_FOUND;
                g_free (pattern);

                if (mc_error_message (&mcerror, NULL))
                    error_flag = TRUE;  /* leave it if file cannot be opened */

                if (type_state == TYPE_NOT_FOUND)
                    continue;
            }
        }
#endif /* USE_FILE_CMD */

        pattern = mc_config_get_string_raw (ext_ini, g, "Regex", NULL);
        if (pattern != NULL)
        {
            mc_search_t *search;

            ignore_case = mc_config_get_bool (ext_ini, g, "RegexIgnoreCase", FALSE);
            search = mc_search_new (pattern, DEFAULT_CHARSET);
            g_free (pattern);

            if (search != NULL)
            {
                search->search_type = MC_SEARCH_T_REGEX;
                search->is_case_sensitive = !ignore_case;
                found = mc_search_run (search, filename, 0, filename_len, NULL);
                mc_search_free (search);
            }

            found = found && (type_state == TYPE_UNUSED || type_state == TYPE_FOUND);
        }
        else
        {
            pattern = mc_config_get_string_raw (ext_ini, g, "Shell", NULL);
            if (pattern != NULL)
            {
                int (*cmp_func) (const char *s1, const char *s2, size_t n);
                size_t pattern_len;

                ignore_case = mc_config_get_bool (ext_ini, g, "ShellIgnoreCase", FALSE);
                cmp_func = ignore_case ? strncasecmp : strncmp;
                pattern_len = strlen (pattern);

                if (*pattern == '.' && filename_len >= pattern_len)
                    found =
                        cmp_func (pattern, filename + filename_len - pattern_len, pattern_len) == 0;
                else
                    found = pattern_len == filename_len
                        && cmp_func (pattern, filename, filename_len) == 0;

                g_free (pattern);

                found = found && (type_state == TYPE_UNUSED || type_state == TYPE_FOUND);
            }
            else
                found = type_state == TYPE_FOUND;
        }
    }

    /* group is found, process actions */
    if (found)
    {
        char *include_value;

        group_iter--;

        /* "Include" parameter has the highest priority over any actions */
        include_value = mc_config_get_string_raw (ext_ini, *group_iter, "Include", NULL);
        if (include_value != NULL)
        {
            /* find "Include/include_value" group */
            include_group = g_strconcat ("Include/", include_value, (char *) NULL);
            g_free (include_value);
            found = mc_config_has_group (ext_ini, include_group);
        }
    }

    if (found)
        current_group = include_group != NULL ? include_group : *group_iter;
    else
    {
        current_group = default_group;
        found = mc_config_has_group (ext_ini, current_group);
    }

    if (found && !error_flag)
    {
        gchar *action_value;

        action_value = mc_config_get_string_raw (ext_ini, current_group, action, NULL);
        if (action_value == NULL)
        {
            /* Not found, try the action from default section */
            action_value = mc_config_get_string_raw (ext_ini, default_group, action, NULL);
            found = (action_value != NULL && *action_value != '\0');
        }
        else
        {
            /* If action's value is empty, ignore action from default section */
            found = (*action_value != '\0');
        }

        if (found)
        {
            vfs_path_t *sv;

            sv = exec_extension (current_panel, target, filename_vpath, action_value,
                                 view_at_line_number);
            if (script_vpath != NULL)
                *script_vpath = sv;
            else
                exec_cleanup_script (sv);

            ret = 1;
        }

        g_free (action_value);
    }

    g_free (include_group);

    return (error_flag ? -1 : ret);
}

/* --------------------------------------------------------------------------------------------- */
