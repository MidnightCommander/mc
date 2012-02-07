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
#include "lib/charsets.h"       /* get_codepage_index */

#include "src/setup.h"          /* use_file_to_check_type */
#include "src/execute.h"
#include "src/history.h"
#include "src/main.h"           /* do_cd */

#include "src/consaver/cons.saver.h"
#include "src/viewer/mcviewer.h"

#ifdef HAVE_CHARSET
#include "src/selcodepage.h"    /* do_set_codepage */
#endif

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

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
exec_extension (const char *filename, const char *lc_data, int *move_dir, int start_line)
{
    vfs_path_t *file_name_vpath;
    int cmd_file_fd;
    FILE *cmd_file;
    char *cmd = NULL;
    int expand_prefix_found = 0;
    int parameter_found = 0;
    char lc_prompt[80];
    int run_view = 0;
    int def_hex_mode = mcview_default_hex_mode, changed_hex_mode = 0;
    int def_nroff_flag = mcview_default_nroff_flag, changed_nroff_flag = 0;
    int written_nonspace = 0;
    int is_cd = 0;
    char buffer[1024];
    char *p = 0;
    vfs_path_t *localcopy_vpath = NULL;
    int do_local_copy;
    time_t localmtime = 0;
    struct stat mystat;
    quote_func_t quote_func = name_quote;
    vfs_path_t *vpath;

    g_return_if_fail (filename != NULL);
    g_return_if_fail (lc_data != NULL);

    vpath = vfs_path_from_str (filename);

    /* Avoid making a local copy if we are doing a cd */
    do_local_copy = vfs_file_is_local (vpath) ? 0 : 1;

    /*
     * All commands should be run in /bin/sh regardless of user shell.
     * To do that, create temporary shell script and run it.
     * Sometimes it's not needed (e.g. for %cd and %view commands),
     * but it's easier to create it anyway.
     */
    cmd_file_fd = mc_mkstemps (&file_name_vpath, "mcext", SCRIPT_SUFFIX);

    if (cmd_file_fd == -1)
    {
        message (D_ERROR, MSG_ERROR,
                 _("Cannot create temporary command file\n%s"), unix_error_string (errno));
        goto ret;
    }

    cmd_file = fdopen (cmd_file_fd, "w");
    fputs ("#! /bin/sh\n", cmd_file);

    lc_prompt[0] = '\0';
    for (; *lc_data != '\0' && *lc_data != '\n'; lc_data++)
    {
        if (parameter_found)
        {
            if (*lc_data == '}')
            {
                char *parameter;

                parameter_found = 0;
                parameter = input_dialog (_("Parameter"), lc_prompt, MC_HISTORY_EXT_PARAMETER, "");
                if (parameter == NULL)
                {
                    /* User canceled */
                    fclose (cmd_file);
                    mc_unlink (file_name_vpath);
                    if (localcopy_vpath != NULL)
                    {
                        mc_ungetlocalcopy (vpath, localcopy_vpath, FALSE);
                        vfs_path_free (localcopy_vpath);
                    }
                    goto ret;
                }
                fputs (parameter, cmd_file);
                written_nonspace = 1;
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
            expand_prefix_found = 0;
            if (*lc_data == '{')
                parameter_found = 1;
            else
            {
                int i;
                char *v;

                i = check_format_view (lc_data);
                if (i != 0)
                {
                    lc_data += i - 1;
                    run_view = 1;
                }
                else
                {
                    i = check_format_cd (lc_data);
                    if (i > 0)
                    {
                        is_cd = 1;
                        quote_func = fake_name_quote;
                        do_local_copy = 0;
                        p = buffer;
                        lc_data += i - 1;
                    }
                    else
                    {
                        i = check_format_var (lc_data, &v);
                        if (i > 0 && v != NULL)
                        {
                            fputs (v, cmd_file);
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
                                if (do_local_copy)
                                {
                                    localcopy_vpath = mc_getlocalcopy (vpath);
                                    if (localcopy_vpath == NULL)
                                    {
                                        fclose (cmd_file);
                                        mc_unlink (file_name_vpath);
                                        goto ret;
                                    }
                                    mc_stat (localcopy_vpath, &mystat);
                                    localmtime = mystat.st_mtime;
                                    text =
                                        quote_func (vfs_path_get_last_path_str (localcopy_vpath),
                                                    0);
                                }
                                else
                                {
                                    const vfs_path_element_t *path_element;

                                    path_element = vfs_path_get_by_index (vpath, -1);
                                    text = quote_func (path_element->path, 0);
                                }
                            }

                            if (!is_cd)
                                fputs (text, cmd_file);
                            else
                            {
                                strcpy (p, text);
                                p = strchr (p, 0);
                            }

                            g_free (text);
                            written_nonspace = 1;
                        }
                    }
                }
            }
        }
        else if (*lc_data == '%')
            expand_prefix_found = 1;
        else
        {
            if (*lc_data != ' ' && *lc_data != '\t')
                written_nonspace = 1;
            if (is_cd)
                *(p++) = *lc_data;
            else
                fputc (*lc_data, cmd_file);
        }
    }                           /* for */

    /*
     * Make the script remove itself when it finishes.
     * Don't do it for the viewer - it may need to rerun the script,
     * so we clean up after calling view().
     */
    if (!run_view)
    {
        char *file_name;

        file_name = vfs_path_to_str (file_name_vpath);
        fprintf (cmd_file, "\n/bin/rm -f %s\n", file_name);
        g_free (file_name);
    }

    fclose (cmd_file);

    if ((run_view && !written_nonspace) || is_cd)
    {
        mc_unlink (file_name_vpath);
        vfs_path_free (file_name_vpath);
        file_name_vpath = NULL;
    }
    else
    {
        char *file_name;

        file_name = vfs_path_to_str (file_name_vpath);
        /* Set executable flag on the command file ... */
        mc_chmod (file_name_vpath, S_IRWXU);
        /* ... but don't rely on it - run /bin/sh explicitly */
        cmd = g_strconcat ("/bin/sh ", file_name, (char *) NULL);
        g_free (file_name);
    }

    if (run_view)
    {
        mcview_ret_t ret;

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
            ret = mcview_viewer (cmd, vpath, start_line);
            mc_unlink (file_name_vpath);
        }
        else
            ret = mcview_viewer (NULL, vpath, start_line);

        if (move_dir != NULL)
            switch (ret)
            {
            case MCVIEW_WANT_NEXT:
                *move_dir = 1;
                break;
            case MCVIEW_WANT_PREV:
                *move_dir = -1;
                break;
            default:
                *move_dir = 0;
            }

        if (changed_hex_mode && !mcview_altered_hex_mode)
            mcview_default_hex_mode = def_hex_mode;
        if (changed_nroff_flag && !mcview_altered_nroff_flag)
            mcview_default_nroff_flag = def_nroff_flag;

        dialog_switch_process_pending ();
    }
    else if (is_cd)
    {
        char *q;
        vfs_path_t *p_vpath;

        *p = 0;
        p = buffer;
        /*      while (*p == ' ' && *p == '\t')
         *          p++;
         */
        /* Search last non-space character. Start search at the end in order
           not to short filenames containing spaces. */
        q = p + strlen (p) - 1;
        while (q >= p && (*q == ' ' || *q == '\t'))
            q--;
        q[1] = 0;

        p_vpath = vfs_path_from_str_flags (p, VPF_NO_CANON);
        do_cd (p_vpath, cd_parse_command);
        vfs_path_free (p_vpath);
    }
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

    if (localcopy_vpath != NULL)
    {
        mc_stat (localcopy_vpath, &mystat);
        mc_ungetlocalcopy (vpath, localcopy_vpath, localmtime != mystat.st_mtime);
        vfs_path_free (localcopy_vpath);
    }
  ret:
    vfs_path_free (file_name_vpath);
    vfs_path_free (vpath);
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

static int
regex_check_type (const vfs_path_t * filename_vpath, const char *ptr, int *have_type)
{
    int found = 0;

    /* Following variables are valid if *have_type is 1 */
    static char content_string[2048];
    static char encoding_id[21];        /* CSISO51INISCYRILLIC -- 20 */
    static size_t content_shift = 0;
    static int got_data = 0;

    if (!use_file_to_check_type)
        return 0;

    if (*have_type == 0)
    {
        vfs_path_t *localfile_vpath;
        const char *realname;         /* name used with "file" */

#ifdef HAVE_CHARSET
        int got_encoding_data;
#endif /* HAVE_CHARSET */

        /* Don't repeate even unsuccessful checks */
        *have_type = 1;

        localfile_vpath = mc_getlocalcopy (filename_vpath);
        if (localfile_vpath == NULL)
            return -1;

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
        return -1;

    if (content_string[0] != '\0'
        && mc_search (ptr, content_string + content_shift, MC_SEARCH_T_REGEX))
    {
        found = 1;
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
regex_command (const vfs_path_t * filename_vpath, const char *action, int *move_dir)
{
    char *filename, *p, *q, *r, c;
    size_t file_len;
    int found = 0;
    int error_flag = 0;
    int ret = 0;
    struct stat mystat;
    int view_at_line_number;
    char *include_target;
    int include_target_len;
    int have_type = 0;          /* Flag used by regex_check_type() */

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
        int mc_user_ext = 1;
        int home_error = 0;

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
            mc_user_ext = 0;
        }

        g_file_get_contents (extension_file, &data, NULL, NULL);
        g_free (extension_file);
        if (data == NULL)
            return 0;

        if (!strstr (data, "default/"))
        {
            if (!strstr (data, "regex/") && !strstr (data, "shell/") && !strstr (data, "type/"))
            {
                g_free (data);
                data = NULL;
                if (mc_user_ext)
                {
                    home_error = 1;
                    goto check_stock_mc_ext;
                }
                else
                {
                    char *title = g_strdup_printf (_(" %s%s file error"),
                                                   mc_global.sysconfig_dir, MC_LIB_EXT);
                    message (D_ERROR, title, _("The format of the %smc.ext "
                                               "file has changed with version 3.0. It seems that "
                                               "the installation failed. Please fetch a fresh "
                                               "copy from the Midnight Commander package."),
                             mc_global.sysconfig_dir);
                    g_free (title);
                    return 0;
                }
            }
        }
        if (home_error)
        {
            char *filebind_filename = mc_config_get_full_path (MC_FILEBIND_FILE);
            char *title = g_strdup_printf (_("%s file error"), filebind_filename);
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

    for (p = data; *p; p++)
    {
        for (q = p; *q == ' ' || *q == '\t'; q++);
        if (*q == '\n' || !*q)
            p = q;              /* empty line */
        if (*p == '#')          /* comment */
            while (*p && *p != '\n')
                p++;
        if (*p == '\n')
            continue;
        if (!*p)
            break;
        if (p == q)
        {                       /* i.e. starts in the first column, should be
                                 * keyword/descNL
                                 */
            found = 0;
            q = strchr (p, '\n');
            if (q == NULL)
                q = strchr (p, 0);
            c = *q;
            *q = 0;
            if (include_target)
            {
                if ((strncmp (p, "include/", 8) == 0)
                    && (strncmp (p + 8, include_target, include_target_len) == 0))
                    found = 1;
            }
            else if (!strncmp (p, "regex/", 6))
            {
                p += 6;
                /* Do not transform shell patterns, you can use shell/ for
                 * that
                 */
                if (mc_search (p, filename, MC_SEARCH_T_REGEX))
                    found = 1;
            }
            else if (!strncmp (p, "directory/", 10))
            {
                if (S_ISDIR (mystat.st_mode) && mc_search (p + 10, filename, MC_SEARCH_T_REGEX))
                    found = 1;
            }
            else if (!strncmp (p, "shell/", 6))
            {
                p += 6;
                if (*p == '.' && file_len >= (size_t)(q - p))
                {
                    if (!strncmp (p, filename + file_len - (q - p), q - p))
                        found = 1;
                }
                else
                {
                    if ((size_t)(q - p) == file_len && !strncmp (p, filename, q - p))
                        found = 1;
                }
            }
            else if (!strncmp (p, "type/", 5))
            {
                int res;
                p += 5;
                res = regex_check_type (filename_vpath, p, &have_type);
                if (res == 1)
                    found = 1;
                if (res == -1)
                    error_flag = 1;     /* leave it if file cannot be opened */
            }
            else if (!strncmp (p, "default/", 8))
            {
                found = 1;
            }
            *q = c;
            p = q;
            if (!*p)
                break;
        }
        else
        {                       /* List of actions */
            p = q;
            q = strchr (p, '\n');
            if (q == NULL)
                q = strchr (p, 0);
            if (found && !error_flag)
            {
                r = strchr (p, '=');
                if (r != NULL)
                {
                    c = *r;
                    *r = 0;
                    if (strcmp (p, "Include") == 0)
                    {
                        char *t;

                        include_target = p + 8;
                        t = strchr (include_target, '\n');
                        if (t)
                            *t = 0;
                        include_target_len = strlen (include_target);
                        if (t)
                            *t = '\n';

                        *r = c;
                        p = q;
                        found = 0;

                        if (!*p)
                            break;
                        continue;
                    }
                    if (!strcmp (action, p))
                    {
                        *r = c;
                        for (p = r + 1; *p == ' ' || *p == '\t'; p++);

                        /* Empty commands just stop searching
                         * through, they don't do anything
                         *
                         * We need to copy the filename because exec_extension
                         * may end up invoking update_panels thus making the
                         * filename parameter invalid (ie, most of the time,
                         * we get filename as a pointer from current_panel->dir).
                         */
                        if (p < q)
                        {
                            char *filename_copy = g_strdup (filename);

                            exec_extension (filename_copy, r + 1, move_dir, view_at_line_number);
                            g_free (filename_copy);

                            ret = 1;
                        }
                        break;
                    }
                    else
                        *r = c;
                }
            }
            p = q;
            if (!*p)
                break;
        }
    }
    g_free (filename);
    if (error_flag)
        return -1;
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
