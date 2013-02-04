/*
   Extension dependent execution.

   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2007, 2011
   The Free Software Foundation, Inc.

   Written by:
   Jakub Jelinek, 1995
   Miguel de Icaza, 1994

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

#include "src/setup.h"          /* use_file_to_check_type */
#include "src/execute.h"
#include "src/history.h"

#include "src/consaver/cons.saver.h"
#include "src/viewer/mcviewer.h"

#ifdef HAVE_CHARSET
#include "src/selcodepage.h"    /* do_set_codepage */
#endif

#include "panel.h"              /* do_cd */
#include "usermenu.h"

#include "ext.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#ifdef FILE_L
#define FILE_CMD "file -L "
#else
#define FILE_CMD "file "
#endif

/*** file scope type declarations ****************************************************************/

typedef char *(*quote_func_t) (const char *name, int quote_percent);

/*** file scope variables ************************************************************************/

/* This variable points to a copy of the mc.ext file in memory
 * With this we avoid loading/parsing the file each time we
 * need it
 */
static char *data = NULL;
static vfs_path_t *localfilecopy_vpath = NULL;
static char buffer[BUF_1K];

static char *pbuffer = NULL;
static time_t localmtime = 0;
static quote_func_t quote_func = name_quote;
static gboolean run_view = FALSE;
static gboolean is_cd = FALSE;
static gboolean written_nonspace = FALSE;
static gboolean do_local_copy = FALSE;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
exec_cleanup_file_name (const vfs_path_t * filename_vpath, gboolean has_changed)
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
    vfs_path_free (localfilecopy_vpath);
    localfilecopy_vpath = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static char *
exec_get_file_name (const vfs_path_t * filename_vpath)
{
    if (!do_local_copy)
        return quote_func (vfs_path_get_last_path_str (filename_vpath), 0);

    if (localfilecopy_vpath == NULL)
    {
        struct stat mystat;
        localfilecopy_vpath = mc_getlocalcopy (filename_vpath);
        if (localfilecopy_vpath == NULL)
            return NULL;

        mc_stat (localfilecopy_vpath, &mystat);
        localmtime = mystat.st_mtime;
    }

    return quote_func (vfs_path_get_last_path_str (localfilecopy_vpath), 0);
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

static char *
exec_get_export_variables (const vfs_path_t * filename_vpath)
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
    return g_string_free (export_vars_string, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static char *
exec_make_shell_string (const char *lc_data, const vfs_path_t * filename_vpath)
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
                char *v;

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
                        i = check_format_var (lc_data, &v);
                        if (i > 0 && v != NULL)
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

                            if (!is_cd)
                                g_string_append (shell_string, text);
                            else
                            {
                                strcpy (pbuffer, text);
                                pbuffer = strchr (pbuffer, 0);
                            }

                            g_free (text);
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
            if (*lc_data != ' ' && *lc_data != '\t')
                written_nonspace = TRUE;
            if (is_cd)
                *(pbuffer++) = *lc_data;
            else
                g_string_append_c (shell_string, *lc_data);
        }
    }                           /* for */
    return g_string_free (shell_string, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
exec_extension_view (char *cmd, const vfs_path_t * filename_vpath, int start_line,
                     vfs_path_t * temp_file_name_vpath)
{
    int def_hex_mode = mcview_default_hex_mode, changed_hex_mode = 0;
    int def_nroff_flag = mcview_default_nroff_flag, changed_nroff_flag = 0;

    mcview_altered_hex_mode = 0;
    mcview_altered_nroff_flag = 0;
    if (def_hex_mode != mcview_default_hex_mode)
        changed_hex_mode = 1;
    if (def_nroff_flag != mcview_default_nroff_flag)
        changed_nroff_flag = 1;

    /* If we've written whitespace only, then just load filename
     * into view
     */
    if (written_nonspace)
    {
        mcview_viewer (cmd, filename_vpath, start_line);
        mc_unlink (temp_file_name_vpath);
    }
    else
        mcview_viewer (NULL, filename_vpath, start_line);

    if (changed_hex_mode && !mcview_altered_hex_mode)
        mcview_default_hex_mode = def_hex_mode;
    if (changed_nroff_flag && !mcview_altered_nroff_flag)
        mcview_default_nroff_flag = def_nroff_flag;

    dialog_switch_process_pending ();
}

/* --------------------------------------------------------------------------------------------- */

static void
exec_extension_cd (void)
{
    char *q;
    vfs_path_t *p_vpath;

    *pbuffer = '\0';
    pbuffer = buffer;
    /*      while (*p == ' ' && *p == '\t')
     *          p++;
     */
    /* Search last non-space character. Start search at the end in order
       not to short filenames containing spaces. */
    q = pbuffer + strlen (pbuffer) - 1;
    while (q >= pbuffer && (*q == ' ' || *q == '\t'))
        q--;
    q[1] = 0;

    p_vpath = vfs_path_from_str_flags (pbuffer, VPF_NO_CANON);
    do_cd (p_vpath, cd_parse_command);
    vfs_path_free (p_vpath);
}


/* --------------------------------------------------------------------------------------------- */

static void
exec_extension (const vfs_path_t * filename_vpath, const char *lc_data, int start_line)
{
    char *shell_string, *export_variables;
    vfs_path_t *temp_file_name_vpath = NULL;
    int cmd_file_fd;
    FILE *cmd_file;
    char *cmd = NULL;

    g_return_if_fail (lc_data != NULL);

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
        exec_extension_cd ();
        g_free (shell_string);
        goto ret;
    }

    /*
     * All commands should be run in /bin/sh regardless of user shell.
     * To do that, create temporary shell script and run it.
     * Sometimes it's not needed (e.g. for %cd and %view commands),
     * but it's easier to create it anyway.
     */
    cmd_file_fd = mc_mkstemps (&temp_file_name_vpath, "mcext", SCRIPT_SUFFIX);

    if (cmd_file_fd == -1)
    {
        message (D_ERROR, MSG_ERROR,
                 _("Cannot create temporary command file\n%s"), unix_error_string (errno));
        goto ret;
    }

    cmd_file = fdopen (cmd_file_fd, "w");
    fputs ("#! /bin/sh\n\n", cmd_file);

    export_variables = exec_get_export_variables (filename_vpath);
    if (export_variables != NULL)
    {
        fprintf (cmd_file, "%s\n", export_variables);
        g_free (export_variables);
    }

    fputs (shell_string, cmd_file);
    g_free (shell_string);

    /*
     * Make the script remove itself when it finishes.
     * Don't do it for the viewer - it may need to rerun the script,
     * so we clean up after calling view().
     */
    if (!run_view)
    {
        char *file_name;

        file_name = vfs_path_to_str (temp_file_name_vpath);
        fprintf (cmd_file, "\n/bin/rm -f %s\n", file_name);
        g_free (file_name);
    }

    fclose (cmd_file);

    if ((run_view && !written_nonspace) || is_cd)
    {
        mc_unlink (temp_file_name_vpath);
        vfs_path_free (temp_file_name_vpath);
        temp_file_name_vpath = NULL;
    }
    else
    {
        char *file_name;

        file_name = vfs_path_to_str (temp_file_name_vpath);
        /* Set executable flag on the command file ... */
        mc_chmod (temp_file_name_vpath, S_IRWXU);
        /* ... but don't rely on it - run /bin/sh explicitly */
        cmd = g_strconcat ("/bin/sh ", file_name, (char *) NULL);
        g_free (file_name);
    }

    if (run_view)
        exec_extension_view (cmd, filename_vpath, start_line, temp_file_name_vpath);
    else
    {
        shell_execute (cmd, EXECUTE_INTERNAL);
        if (mc_global.tty.console_flag != '\0')
        {
            handle_console (CONSOLE_SAVE);
            if (output_lines && mc_global.keybar_visible)
                show_console_contents (output_start_y,
                                       LINES - mc_global.keybar_visible -
                                       output_lines - 1, LINES - mc_global.keybar_visible - 1);
        }
    }

    g_free (cmd);

    exec_cleanup_file_name (filename_vpath, TRUE);
  ret:
    vfs_path_free (temp_file_name_vpath);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Run cmd_file with args, put result into buf.
 * If error, put '\0' into buf[0]
 * Return 1 if the data is valid, 0 otherwise, -1 for fatal errors.
 *
 * NOTES: buf is null-terminated string.
 */

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
get_file_type_local (const vfs_path_t * filename_vpath, char *buf, int buflen)
{
    char *tmp;
    int ret;

    tmp = name_quote (vfs_path_get_last_path_str (filename_vpath), 0);
    ret = get_popen_information (FILE_CMD, tmp, buf, buflen);
    g_free (tmp);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Run the "enca" command on the local file.
 * Return 1 if the data is valid, 0 otherwise, -1 for fatal errors.
 */

#ifdef HAVE_CHARSET
static int
get_file_encoding_local (const vfs_path_t * filename_vpath, char *buf, int buflen)
{
    char *tmp, *lang, *args;
    int ret;

    tmp = name_quote (vfs_path_get_last_path_str (filename_vpath), 0);
    lang = name_quote (autodetect_codeset, 0);
    args = g_strconcat (" -L", lang, " -i ", tmp, (char *) NULL);

    ret = get_popen_information ("enca", args, buf, buflen);

    g_free (args);
    g_free (lang);
    g_free (tmp);

    return ret;
}
#endif /* HAVE_CHARSET */

/* --------------------------------------------------------------------------------------------- */
/**
 * Invoke the "file" command on the file and match its output against PTR.
 * have_type is a flag that is set if we already have tried to determine
 * the type of that file.
 * Return 1 for match, 0 for no match, -1 errors.
 */

static gboolean
regex_check_type (const vfs_path_t * filename_vpath, const char *ptr, int *have_type,
                  gboolean case_insense, GError ** error)
{
    gboolean found = FALSE;

    /* Following variables are valid if *have_type is 1 */
    static char content_string[2048];
#ifdef HAVE_CHARSET
    static char encoding_id[21];        /* CSISO51INISCYRILLIC -- 20 */
#endif
    static size_t content_shift = 0;
    static int got_data = 0;

    if (!use_file_to_check_type)
        return FALSE;

    if (*have_type == 0)
    {
        vfs_path_t *localfile_vpath;
        const char *realname;   /* name used with "file" */

#ifdef HAVE_CHARSET
        int got_encoding_data;
#endif /* HAVE_CHARSET */

        /* Don't repeate even unsuccessful checks */
        *have_type = 1;

        localfile_vpath = mc_getlocalcopy (filename_vpath);
        if (localfile_vpath == NULL)
        {
            char *filename;

            filename = vfs_path_to_str (filename_vpath);
            g_propagate_error (error,
                               g_error_new (MC_ERROR, -1,
                                            _("Cannot fetch a local copy of %s"), filename));
            g_free (filename);
            return FALSE;
        }

        realname = vfs_path_get_last_path_str (localfile_vpath);

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

        mc_ungetlocalcopy (filename_vpath, localfile_vpath, FALSE);

        got_data = get_file_type_local (localfile_vpath, content_string, sizeof (content_string));

        if (got_data > 0)
        {
            char *pp;
            size_t real_len;

            pp = strchr (content_string, '\n');
            if (pp != NULL)
                *pp = '\0';

            real_len = strlen (realname);

            if (strncmp (content_string, realname, real_len) == 0)
            {
                /* Skip "realname: " */
                content_shift = real_len;
                if (content_string[content_shift] == ':')
                {
                    /* Solaris' file prints tab(s) after ':' */
                    for (content_shift++;
                         content_string[content_shift] == ' '
                         || content_string[content_shift] == '\t'; content_shift++)
                        ;
                }
            }
        }
        else
        {
            /* No data */
            content_string[0] = '\0';
        }
        vfs_path_free (localfile_vpath);
    }

    if (got_data == -1)
    {
        g_propagate_error (error, g_error_new (MC_ERROR, -1, _("Pipe failed")));
        return FALSE;
    }

    if (content_string[0] != '\0')
    {
        mc_search_t *search;

        search = mc_search_new (ptr, -1);
        if (search != NULL)
        {
            search->search_type = MC_SEARCH_T_REGEX;
            search->is_case_sensitive = !case_insense;
            found = mc_search_run (search, content_string + content_shift, 0, -1, NULL);
            mc_search_free (search);
        }
        else
        {
            g_propagate_error (error, g_error_new (MC_ERROR, -1, _("Regular expression error")));
        }
    }

    return found;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
flush_extension_file (void)
{
    g_free (data);
    data = NULL;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * The second argument is action, i.e. Open, View or Edit
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
regex_command (const vfs_path_t * filename_vpath, const char *action)
{
    char *filename, *p, *q, *r, c;
    size_t file_len;
    gboolean found = FALSE;
    gboolean error_flag = FALSE;
    int ret = 0;
    struct stat mystat;
    int view_at_line_number;
    char *include_target;
    int include_target_len;
    int have_type = 0;          /* Flag used by regex_check_type() */

    if (filename_vpath == NULL)
        return 0;

    /* Check for the special View:%d parameter */
    if (strncmp (action, "View:", 5) == 0)
    {
        view_at_line_number = atoi (action + 5);
        action = "View";
    }
    else
    {
        view_at_line_number = 0;
    }

    if (data == NULL)
    {
        char *extension_file;
        gboolean mc_user_ext = TRUE;
        gboolean home_error = FALSE;

        extension_file = mc_config_get_full_path (MC_FILEBIND_FILE);
        if (!exist_file (extension_file))
        {
            g_free (extension_file);
          check_stock_mc_ext:
            extension_file = mc_build_filename (mc_global.sysconfig_dir, MC_LIB_EXT, NULL);
            if (!exist_file (extension_file))
            {
                g_free (extension_file);
                extension_file = mc_build_filename (mc_global.share_data_dir, MC_LIB_EXT, NULL);
            }
            mc_user_ext = FALSE;
        }

        g_file_get_contents (extension_file, &data, NULL, NULL);
        g_free (extension_file);
        if (data == NULL)
            return 0;

        if (strstr (data, "default/") == NULL)
        {
            if (strstr (data, "regex/") == NULL && strstr (data, "shell/") == NULL &&
                strstr (data, "type/") == NULL)
            {
                g_free (data);
                data = NULL;

                if (!mc_user_ext)
                {
                    char *title;

                    title = g_strdup_printf (_(" %s%s file error"),
                                             mc_global.sysconfig_dir, MC_LIB_EXT);
                    message (D_ERROR, title, _("The format of the %smc.ext "
                                               "file has changed with version 3.0. It seems that "
                                               "the installation failed. Please fetch a fresh "
                                               "copy from the Midnight Commander package."),
                             mc_global.sysconfig_dir);
                    g_free (title);
                    return 0;
                }

                home_error = TRUE;
                goto check_stock_mc_ext;
            }
        }

        if (home_error)
        {
            char *filebind_filename;
            char *title;

            filebind_filename = mc_config_get_full_path (MC_FILEBIND_FILE);
            title = g_strdup_printf (_("%s file error"), filebind_filename);
            message (D_ERROR, title,
                     _("The format of the %s file has "
                       "changed with version 3.0. You may either want to copy "
                       "it from %smc.ext or use that file as an example of how to write it."),
                     filebind_filename, mc_global.sysconfig_dir);
            g_free (filebind_filename);
            g_free (title);
        }
    }

    mc_stat (filename_vpath, &mystat);

    include_target = NULL;
    include_target_len = 0;
    filename = vfs_path_to_str (filename_vpath);
    file_len = vfs_path_len (filename_vpath);

    for (p = data; *p != '\0'; p++)
    {
        for (q = p; *q == ' ' || *q == '\t'; q++)
            ;
        if (*q == '\n' || *q == '\0')
            p = q;              /* empty line */
        if (*p == '#')          /* comment */
            while (*p != '\0' && *p != '\n')
                p++;
        if (*p == '\n')
            continue;
        if (*p == '\0')
            break;
        if (p == q)
        {                       /* i.e. starts in the first column, should be
                                 * keyword/descNL
                                 */
            gboolean case_insense;

            found = FALSE;
            q = strchr (p, '\n');
            if (q == NULL)
                q = strchr (p, '\0');
            c = *q;
            *q = '\0';
            if (include_target)
            {
                if ((strncmp (p, "include/", 8) == 0)
                    && (strncmp (p + 8, include_target, include_target_len) == 0))
                    found = TRUE;
            }
            else if (strncmp (p, "regex/", 6) == 0)
            {
                mc_search_t *search;

                p += 6;
                case_insense = (strncmp (p, "i/", 2) == 0);
                if (case_insense)
                    p += 2;

                search = mc_search_new (p, -1);
                if (search != NULL)
                {
                    search->search_type = MC_SEARCH_T_REGEX;
                    search->is_case_sensitive = !case_insense;
                    found = mc_search_run (search, filename, 0, file_len, NULL);
                    mc_search_free (search);
                }
            }
            else if (strncmp (p, "directory/", 10) == 0)
            {
                if (S_ISDIR (mystat.st_mode) && mc_search (p + 10, filename, MC_SEARCH_T_REGEX))
                    found = TRUE;
            }
            else if (strncmp (p, "shell/", 6) == 0)
            {
                int (*cmp_func) (const char *s1, const char *s2, size_t n) = strncmp;

                p += 6;
                case_insense = (strncmp (p, "i/", 2) == 0);
                if (case_insense)
                {
                    p += 2;
                    cmp_func = strncasecmp;
                }

                if (*p == '.' && file_len >= (size_t) (q - p))
                {
                    if (cmp_func (p, filename + file_len - (q - p), q - p) == 0)
                        found = TRUE;
                }
                else
                {
                    if ((size_t) (q - p) == file_len && cmp_func (p, filename, q - p) == 0)
                        found = TRUE;
                }
            }
            else if (strncmp (p, "type/", 5) == 0)
            {
                GError *error = NULL;

                p += 5;

                case_insense = (strncmp (p, "i/", 2) == 0);
                if (case_insense)
                    p += 2;

                found = regex_check_type (filename_vpath, p, &have_type, case_insense, &error);
                if (error != NULL)
                {
                    g_error_free (error);
                    error_flag = TRUE;  /* leave it if file cannot be opened */
                }
            }
            else if (strncmp (p, "default/", 8) == 0)
                found = TRUE;

            *q = c;
            p = q;
            if (*p == '\0')
                break;
        }
        else
        {                       /* List of actions */
            p = q;
            q = strchr (p, '\n');
            if (q == NULL)
                q = strchr (p, '\0');
            if (found && !error_flag)
            {
                r = strchr (p, '=');
                if (r != NULL)
                {
                    c = *r;
                    *r = '\0';
                    if (strcmp (p, "Include") == 0)
                    {
                        char *t;

                        include_target = p + 8;
                        t = strchr (include_target, '\n');
                        if (t != NULL)
                            *t = '\0';
                        include_target_len = strlen (include_target);
                        if (t != NULL)
                            *t = '\n';

                        *r = c;
                        p = q;
                        found = FALSE;

                        if (*p == '\0')
                            break;
                        continue;
                    }

                    if (strcmp (action, p) != 0)
                        *r = c;
                    else
                    {
                        *r = c;

                        for (p = r + 1; *p == ' ' || *p == '\t'; p++)
                            ;

                        /* Empty commands just stop searching
                         * through, they don't do anything
                         */
                        if (p < q)
                        {
                            exec_extension (filename_vpath, r + 1, view_at_line_number);
                            ret = 1;
                        }
                        break;
                    }
                }
            }
            p = q;
            if (*p == '\0')
                break;
        }
    }
    g_free (filename);
    if (error_flag)
        ret = -1;
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
