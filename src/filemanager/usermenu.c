/*
   User Menu implementation

   Copyright (C) 1994-2016
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2013

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

/** \file usermenu.c
 *  \brief Source: user menu implementation
 */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/tty/tty.h"
#include "lib/skin.h"
#include "lib/search.h"
#include "lib/vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/widget.h"

#include "src/editor/edit.h"    /* WEdit, BLOCK_FILE */
#include "src/viewer/mcviewer.h"        /* for default_* externs */

#include "src/execute.h"
#include "src/setup.h"
#include "src/history.h"

#include "dir.h"
#include "midnight.h"
#include "layout.h"

#include "usermenu.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define MAX_ENTRIES 16
#define MAX_ENTRY_LEN 60
#define END_OF_STRING '\0'
/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static gboolean debug_flag = FALSE;
static gboolean debug_error = FALSE;
static char *menu = NULL;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** strip file's extension */
static char *
strip_ext (char *ss)
{
    char *s = ss;
    char *e = NULL;

    while (*s != END_OF_STRING)
    {
        if (*s == '.')
            e = s;
        if (IS_PATH_SEP (*s) && e != NULL)
            e = NULL;           /* '.' in *directory* name */
        s++;
    }
    if (e != NULL)
        *e = END_OF_STRING;
    return ss;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Check for the "shell_patterns" directive.  If it's found and valid,
 * interpret it and move the pointer past the directive.  Return the
 * current pointer.
 */

static char *
check_patterns (char *p)
{
    static const char def_name[] = "shell_patterns=";
    char *p0 = p;

    if (strncmp (p, def_name, sizeof (def_name) - 1) != 0)
        return p0;

    p += sizeof (def_name) - 1;
    if (*p == '1')
        easy_patterns = 1;
    else if (*p == '0')
        easy_patterns = 0;
    else
        return p0;

    /* Skip spaces */
    p++;
    while (*p == '\n' || *p == '\t' || *p == ' ')
        p++;
    return p;
}

/* --------------------------------------------------------------------------------------------- */
/** Copies a whitespace separated argument from p to arg. Returns the
   point after argument. */

static char *
extract_arg (char *p, char *arg, int size)
{
    while (*p != END_OF_STRING && (*p == ' ' || *p == '\t' || *p == '\n'))
        p++;

    /* support quote space .mnu */
    while (*p != END_OF_STRING && (*p != ' ' || *(p - 1) == '\\') && *p != '\t' && *p != '\n')
    {
        char *np;

        np = str_get_next_char (p);
        if (np - p >= size)
            break;
        memcpy (arg, p, np - p);
        arg += np - p;
        size -= np - p;
        p = np;
    }
    *arg = END_OF_STRING;
    if (*p == END_OF_STRING || *p == '\n')
        str_prev_char (&p);
    return p;
}

/* --------------------------------------------------------------------------------------------- */
/* Tests whether the selected file in the panel is of any of the types
   specified in argument. */

static gboolean
test_type (WPanel * panel, char *arg)
{
    int result = 0;             /* False by default */
    mode_t st_mode = panel->dir.list[panel->selected].st.st_mode;

    for (; *arg != END_OF_STRING; arg++)
    {
        switch (*arg)
        {
        case 'n':              /* Not a directory */
            result |= !S_ISDIR (st_mode);
            break;
        case 'r':              /* Regular file */
            result |= S_ISREG (st_mode);
            break;
        case 'd':              /* Directory */
            result |= S_ISDIR (st_mode);
            break;
        case 'l':              /* Link */
            result |= S_ISLNK (st_mode);
            break;
        case 'c':              /* Character special */
            result |= S_ISCHR (st_mode);
            break;
        case 'b':              /* Block special */
            result |= S_ISBLK (st_mode);
            break;
        case 'f':              /* Fifo (named pipe) */
            result |= S_ISFIFO (st_mode);
            break;
        case 's':              /* Socket */
            result |= S_ISSOCK (st_mode);
            break;
        case 'x':              /* Executable */
            result |= (st_mode & 0111) != 0 ? 1 : 0;
            break;
        case 't':
            result |= panel->marked != 0 ? 1 : 0;
            break;
        default:
            debug_error = TRUE;
            break;
        }
    }

    return (result != 0);
}

/* --------------------------------------------------------------------------------------------- */
/** Calculates the truth value of the next condition starting from
   p. Returns the point after condition. */

static char *
test_condition (const WEdit * edit_widget, char *p, gboolean * condition)
{
    char arg[256];
    const mc_search_type_t search_type = easy_patterns ? MC_SEARCH_T_GLOB : MC_SEARCH_T_REGEX;

    /* Handle one condition */
    for (; *p != '\n' && *p != '&' && *p != '|'; p++)
    {
        WPanel *panel = NULL;

        /* support quote space .mnu */
        if ((*p == ' ' && *(p - 1) != '\\') || *p == '\t')
            continue;
        if (*p >= 'a')
            panel = current_panel;
        else if (get_other_type () == view_listing)
            panel = other_panel;

        *p |= 0x20;

        switch (*p++)
        {
        case '!':
            p = test_condition (edit_widget, p, condition);
            *condition = !*condition;
            str_prev_char (&p);
            break;
        case 'f':              /* file name pattern */
            p = extract_arg (p, arg, sizeof (arg));
#ifdef USE_INTERNAL_EDIT
            if (edit_widget != NULL)
            {
                const char *edit_filename;

                edit_filename = edit_get_file_name (edit_widget);
                *condition = mc_search (arg, DEFAULT_CHARSET, edit_filename, search_type);
            }
            else
#endif
                *condition = panel != NULL &&
                    mc_search (arg, DEFAULT_CHARSET, panel->dir.list[panel->selected].fname,
                               search_type);
            break;
        case 'y':              /* syntax pattern */
#ifdef USE_INTERNAL_EDIT
            if (edit_widget != NULL)
            {
                const char *syntax_type;

                syntax_type = edit_get_syntax_type (edit_widget);
                if (syntax_type != NULL)
                {
                    p = extract_arg (p, arg, sizeof (arg));
                    *condition = mc_search (arg, DEFAULT_CHARSET, syntax_type, MC_SEARCH_T_NORMAL);
                }
            }
#endif
            break;
        case 'd':
            p = extract_arg (p, arg, sizeof (arg));
            *condition = panel != NULL
                && mc_search (arg, DEFAULT_CHARSET, vfs_path_as_str (panel->cwd_vpath),
                              search_type);
            break;
        case 't':
            p = extract_arg (p, arg, sizeof (arg));
            *condition = panel != NULL && test_type (panel, arg);
            break;
        case 'x':              /* executable */
            {
                struct stat status;

                p = extract_arg (p, arg, sizeof (arg));
                *condition = stat (arg, &status) == 0 && is_exe (status.st_mode);
                break;
            }
        default:
            debug_error = TRUE;
            break;
        }                       /* switch */
    }                           /* while */
    return p;
}

/* --------------------------------------------------------------------------------------------- */
/** General purpose condition debug output handler */

static void
debug_out (char *start, char *end, gboolean condition)
{
    static char *msg = NULL;

    if (start == NULL && end == NULL)
    {
        /* Show output */
        if (debug_flag && msg != NULL)
        {
            size_t len;

            len = strlen (msg);
            if (len != 0)
                msg[len - 1] = END_OF_STRING;
            message (D_NORMAL, _("Debug"), "%s", msg);

        }
        debug_flag = FALSE;
        MC_PTR_FREE (msg);
    }
    else
    {
        const char *type;
        char *p;

        /* Save debug info for later output */
        if (!debug_flag)
            return;
        /* Save the result of the condition */
        if (debug_error)
        {
            type = _("ERROR:");
            debug_error = FALSE;
        }
        else if (condition)
            type = _("True:");
        else
            type = _("False:");
        /* This is for debugging, don't need to be super efficient.  */
        if (end == NULL)
            p = g_strdup_printf ("%s %s %c \n", msg ? msg : "", type, *start);
        else
            p = g_strdup_printf ("%s %s %.*s \n", msg ? msg : "", type, (int) (end - start), start);
        g_free (msg);
        msg = p;
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Calculates the truth value of one lineful of conditions. Returns
   the point just before the end of line. */

static char *
test_line (const WEdit * edit_widget, char *p, gboolean * result)
{
    char operator;

    /* Repeat till end of line */
    while (*p != END_OF_STRING && *p != '\n')
    {
        char *debug_start, *debug_end;
        gboolean condition = TRUE;

        /* support quote space .mnu */
        while ((*p == ' ' && *(p - 1) != '\\') || *p == '\t')
            p++;
        if (*p == END_OF_STRING || *p == '\n')
            break;
        operator = *p++;
        if (*p == '?')
        {
            debug_flag = TRUE;
            p++;
        }
        /* support quote space .mnu */
        while ((*p == ' ' && *(p - 1) != '\\') || *p == '\t')
            p++;
        if (*p == END_OF_STRING || *p == '\n')
            break;

        debug_start = p;
        p = test_condition (edit_widget, p, &condition);
        debug_end = p;
        /* Add one debug statement */
        debug_out (debug_start, debug_end, condition);

        switch (operator)
        {
        case '+':
        case '=':
            /* Assignment */
            *result = condition;
            break;
        case '&':              /* Logical and */
            *result = *result && condition;
            break;
        case '|':              /* Logical or */
            *result = *result || condition;
            break;
        default:
            debug_error = TRUE;
            break;
        }                       /* switch */
        /* Add one debug statement */
        debug_out (&operator, NULL, *result);

    }                           /* while (*p != '\n') */
    /* Report debug message */
    debug_out (NULL, NULL, TRUE);

    if (*p == END_OF_STRING || *p == '\n')
        str_prev_char (&p);
    return p;
}

/*
 * execute_menu_command:
 * 
 *   DESCRIPTION:
 *     Parses a menu command byte-by-byte, performs all macro
 *     substitutions, and attempts to execute the result as a
 *     shell script (/bin/sh, not /bin/bash).
 *
 *   PARAMETERS:
 *     edit_widget:
 *     unparsed_cmd: the text of the menu entry, from its title
 *       line until its termination.
 *     show_prompt:
 *
 *   This function begins with several (temporary) embedded
 *   sub-functions:
 *     prepare_output_parsed_cmd_file ()
 *     found_end_of_input ()
 *     macro_substitution()
 *     macro_substition_completion()
 *     process_input_box()
 *     execute_parsed_cmd_file()
 *     main_execute_menu_command()
 *   The processing begins with the call to sub-function
 *   main_execute_menu_command.
 */
static void
execute_menu_command (const WEdit * edit_widget, const char *unparsed_cmd, gboolean show_prompt)
{
    /*
     * These next three variables support the user menu macro
     * %{some text}, which indicates to mc to create an ncurses-
     * style input dialog, using "some text" as the prompt.
     *
     * input_box_prompt: an incrementing pointer within the static
     *        array "lc_prompt" (defined below) to place there the
     *        input prompt "some_text".
     *
     * input_box_reply: pointer to a dynamically allocated buffer
     *        holding the user's ersponse to the input dialog. The
     *        function will add the response to the parsed command
     *        which will be executed at the successful conclusion of
     *        this function.
     *
     * user_abort: respect a user's response to the input_box.
     */
    char *input_box_prompt = NULL;
    char *input_box_reply = NULL;
    gboolean user_abort = FALSE;
    /*
     * do_quote: "%0" indicate not to surround the following macro
     *     substition in quotes. When the macro prefix character is
     *     followed by any other digit, the indication is to quote
     *     the substitution, ie. "%0f" will potentially substitute
     *     unquoted a file name with embedded spaces.
     */
    gboolean do_quote = FALSE;
    /*
     * lc_prompt: This static array will hold a prompt "some_text"
     *     to be displayed to a user in an ncurses-style input
     *     dialog box for any occurrence of the macro "%{some_text}"
     *
     * WISHLIST: convert it to a resizable and dynamically allocated
     *     buffer. I want this because I would like to be able to
     *     present the user with longer and multi-line prompt
     *     strings. This would also separately require modifications
     *     to the widget function wrapped by function "input_dialog".
     */
    char lc_prompt[80];
    /* line_begin: menu command lines must begin with whitespace
     *     (ie. space or tab).
     */
    gboolean line_begin = TRUE;

    vfs_path_t *file_name_vpath;
    gboolean run_view = FALSE;

    FILE *parsed_cmd_file;
    int parsed_cmd_file_fd;

    /* BEGIN SUB-FUNCTIONS */
    /*
     *  I suggest that this function be moved to its own source
     *  code file for clarity / readability.
     */
    gboolean prepare_output_parsed_cmd_file (void)
    {
        parsed_cmd_file_fd = mc_mkstemps (&file_name_vpath, "mcusr", SCRIPT_SUFFIX);
        if (parsed_cmd_file_fd == -1)
        {
            message (D_ERROR, MSG_ERROR, _("Cannot create temporary command file\n%s"),
                     unix_error_string (errno));
            vfs_path_free (file_name_vpath);
            return FALSE;
        }
        parsed_cmd_file = fdopen (parsed_cmd_file_fd, "w");
        fputs ("#! /bin/sh\n", parsed_cmd_file);
        unparsed_cmd++;
        return TRUE;
    }

    gboolean found_end_of_input (void)
    {
        /*
         *  return TRUE if we find any indication of an end to
         *  parsing, ie. an empty line or a line not beginning
         *  with whitespace. The other EOF condition, END_OF_STRING,
         *  was checked in the 'for' loop statement wrapping this
         *  function.
         */
        if (line_begin)
        {
            if (*unparsed_cmd != ' ' && *unparsed_cmd != '\t')
                return TRUE;
            while (*unparsed_cmd == ' ' || *unparsed_cmd == '\t')
                unparsed_cmd++;
            if (*unparsed_cmd == '\n')
                return TRUE;
        }
        if (*unparsed_cmd == END_OF_STRING)
            return TRUE;
        line_begin = FALSE;
        return FALSE;
    }

    void process_input_box (void)
    {
        input_box_prompt = lc_prompt;
        while TRUE
        {
            unparsed_cmd++;
            if (found_end_of_input ())
            {
                /* unexpected EOF. rewind and let main function handle it */
                unparsed_cmd--;
                break;
            }
            /* if reached end of input prompt string */
            if (*unparsed_cmd == '}')
            {
                input_box_reply =
                    input_dialog (_("Parameter"), lc_prompt, MC_HISTORY_FM_MENU_EXEC_PARAM, "",
                                  INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD |
                                  INPUT_COMPLETE_HOSTNAMES | INPUT_COMPLETE_VARIABLES |
                                  INPUT_COMPLETE_USERNAMES);
                if (input_box_reply == NULL || *input_box_reply == END_OF_STRING)
                {
                    /* User canceled */
                    fclose (parsed_cmd_file);
                    mc_unlink (file_name_vpath);
                    vfs_path_free (file_name_vpath);
                    user_abort = TRUE;
                }
                else if (do_quote)
                {
                    char *tmp;
                    tmp = name_quote (input_box_reply, FALSE);
                    fputs (tmp, parsed_cmd_file);
                    g_free (tmp);
                }
                else
                    fputs (input_box_reply, parsed_cmd_file);
                MC_PTR_FREE (input_box_reply);
                break;
            }
            /*
             * add another character of macro %{some_text} to the
             * static 80 character buffer that began at location
             * lc_prompt if there is space remaining there. Variable
             * "unparsed_cmd" points to the next character to parse in
             * the menu entry.
             */
            else if (input_box_prompt < lc_prompt + sizeof (lc_prompt) - 1)
                *input_box_prompt++ = *unparsed_cmd;
        }
    }

    void macro_substitution_completion (void)
    {
        /* Quote expanded macro unless next char is zero */
        do_quote = TRUE;
        unparsed_cmd++;
        if (found_end_of_input ())
        {
            /* unexpected EOF. rewind and let main function handle it */
            unparsed_cmd--;
        }
        else
        {
            if (g_ascii_isdigit ((gchar) * unparsed_cmd))
            {
                do_quote = (atoi (unparsed_cmd) != 0);
                while (g_ascii_isdigit ((gchar) * unparsed_cmd))
                    unparsed_cmd++;
            }
            if (*unparsed_cmd == '{')
                process_input_box ();
            else
            {
                char *text;
                text = expand_format (edit_widget, *unparsed_cmd, do_quote);
                fputs (text, parsed_cmd_file);
                g_free (text);
            }
        }
    }

    gboolean macro_substitution (void)
    {
        int len;
        if (*unparsed_cmd != '%')
            return FALSE;
        len = check_format_view (unparsed_cmd + 1);
        if (len != 0)
        {
            unparsed_cmd += len;
            run_view = TRUE;
        }
        else
            macro_substitution_completion ();
        return TRUE;
    }

    void execute_parsed_cmd_file (void)
    {
        /* execute the command indirectly to allow execution even
         * on no-exec filesystems. */
        char *cmd;
        cmd = g_strconcat ("/bin/sh ", vfs_path_as_str (file_name_vpath), (char *) NULL);
        if (!show_prompt)
        {
            if (system (cmd) == -1)
                message (D_ERROR, MSG_ERROR, "%s", _("Error calling program"));
        }
        else
            shell_execute (cmd, EXECUTE_HIDE);
        g_free (cmd);
    }

    void main_execute_menu_command (void)
    {
        /* Skip menu entry title line */
        unparsed_cmd = strchr (unparsed_cmd, '\n');
        if (unparsed_cmd == NULL)
            /* menu entry was a title with no executable code! */
            return;
        if (!prepare_output_parsed_cmd_file ())
            return;
        for (; !found_end_of_input (); unparsed_cmd++)
        {
            if (!macro_substitution ())
                fputc (*unparsed_cmd, parsed_cmd_file);
            else if (user_abort)
                /* user aborted at an input dialog box, eg. C-c, ESC */
                return;
        }
        fclose (parsed_cmd_file);
        mc_chmod (file_name_vpath, S_IRWXU);
        if (run_view)
        {
            mcview_viewer (vfs_path_as_str (file_name_vpath), NULL, 0, 0, 0);
            dialog_switch_process_pending ();
        }
        else
            execute_parsed_cmd_file ();
        mc_unlink (file_name_vpath);
        vfs_path_free (file_name_vpath);
    }
    /* END SUB-FUNCTIONS */

    main_execute_menu_command ();

}

/* --------------------------------------------------------------------------------------------- */
/**
 **     Check owner of the menu file. Using menu file is allowed, if
 **     owner of the menu is root or the actual user. In either case
 **     file should not be group and word-writable.
 **
 **     Q. Should we apply this routine to system and home menu (and .ext files)?
 */

static gboolean
menu_file_own (char *path)
{
    struct stat st;

    if (stat (path, &st) == 0 && (st.st_uid == 0 || (st.st_uid == geteuid ()) != 0)
        && ((st.st_mode & (S_IWGRP | S_IWOTH)) == 0))
        return TRUE;

    if (verbose)
        message (D_NORMAL, _("Warning -- ignoring file"),
                 _("File %s is not owned by root or you or is world writable.\n"
                   "Using it may compromise your security"), path);

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* Formats defined:
   %%  The % character
   %f  The current file (if non-local vfs, file will be copied locally and
   %f will be full path to it).
   %p  The current file
   %d  The current working directory
   %s  "Selected files"; the tagged files if any, otherwise the current file
   %t  Tagged files
   %u  Tagged files (and they are untagged on return from expand_format)
   %view Runs the commands and pipes standard output to the view command.
   If %view is immediately followed by '{', recognize keywords
   ascii, hex, nroff and unform

   If the format letter is in uppercase, it refers to the other panel.

   With a number followed the % character you can turn quoting on (default)
   and off. For example:
   %f    quote expanded macro
   %1f   ditto
   %0f   don't quote expanded macro

   expand_format returns a memory block that must be free()d.
 */

/* Returns how many characters we should advance if %view was found */
int
check_format_view (const char *p)
{
    const char *q = p;

    if (strncmp (p, "view", 4) == 0)
    {
        q += 4;
        if (*q == '{')
        {
            for (q++; *q != END_OF_STRING && *q != '}'; q++)
            {
                if (strncmp (q, DEFAULT_CHARSET, 5) == 0)
                {
                    mcview_default_hex_mode = 0;
                    q += 4;
                }
                else if (strncmp (q, "hex", 3) == 0)
                {
                    mcview_default_hex_mode = 1;
                    q += 2;
                }
                else if (strncmp (q, "nroff", 5) == 0)
                {
                    mcview_default_nroff_flag = 1;
                    q += 4;
                }
                else if (strncmp (q, "unform", 6) == 0)
                {
                    mcview_default_nroff_flag = 0;
                    q += 5;
                }
            }
            if (*q == '}')
                q++;
        }
        return q - p;
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

int
check_format_cd (const char *p)
{
    return (strncmp (p, "cd", 2)) != 0 ? 0 : 3;
}

/* --------------------------------------------------------------------------------------------- */
/* Check if p has a "^var\{var-name\}" */
/* Returns the number of skipped characters (zero on not found) */
/* V will be set to the expanded variable name */

int
check_format_var (const char *p, char **v)
{
    const char *q = p;
    char *var_name;

    *v = NULL;
    if (strncmp (p, "var{", 4) == 0)
    {
        const char *dots = NULL;
        const char *value;

        for (q += 4; *q != END_OF_STRING && *q != '}'; q++)
        {
            if (*q == ':')
                dots = q + 1;
        }
        if (*q == END_OF_STRING)
            return 0;

        if (dots == NULL || dots == q + 5)
        {
            message (D_ERROR,
                     _("Format error on file Extensions File"),
                     !dots ? _("The %%var macro has no default")
                     : _("The %%var macro has no variable"));
            return 0;
        }

        /* Copy the variable name */
        var_name = g_strndup (p + 4, dots - 2 - (p + 3));

        value = getenv (var_name);
        g_free (var_name);
        if (value != NULL)
        {
            *v = g_strdup (value);
            return q - p;
        }
        var_name = g_strndup (dots, q - dots);
        *v = var_name;
        return q - p;
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

char *
expand_format (const WEdit * edit_widget, char c, gboolean do_quote)
{
    WPanel *panel = NULL;
    char *(*quote_func) (const char *, gboolean);
    const char *fname = NULL;
    char *result;
    char c_lc;

#ifndef USE_INTERNAL_EDIT
    (void) edit_widget;
#endif

    if (c == '%')
        return g_strdup ("%");

    switch (mc_global.mc_run_mode)
    {
    case MC_RUN_FULL:
        if (g_ascii_islower ((gchar) c))
            panel = current_panel;
        else
        {
            if (get_other_type () != view_listing)
                return g_strdup ("");
            panel = other_panel;
        }
        fname = panel->dir.list[panel->selected].fname;
        break;

#ifdef USE_INTERNAL_EDIT
    case MC_RUN_EDITOR:
        fname = edit_get_file_name (edit_widget);
        break;
#endif

    default:
        /* other modes don't use formats */
        return g_strdup ("");
    }

    if (do_quote)
        quote_func = name_quote;
    else
        quote_func = fake_name_quote;

    c_lc = g_ascii_tolower ((gchar) c);

    switch (c_lc)
    {
    case 'f':
    case 'p':
        result = quote_func (fname, FALSE);
        goto ret;
    case 'x':
        result = quote_func (extension (fname), FALSE);
        goto ret;
    case 'd':
        {
            const char *cwd;
            char *qstr;

            if (panel != NULL)
                cwd = vfs_path_as_str (panel->cwd_vpath);
            else
                cwd = vfs_get_current_dir ();

            qstr = quote_func (cwd, FALSE);

            result = qstr;
            goto ret;
        }
    case 'i':                  /* indent equal number cursor position in line */
#ifdef USE_INTERNAL_EDIT
        if (edit_widget != NULL)
        {
            result = g_strnfill (edit_get_curs_col (edit_widget), ' ');
            goto ret;
        }
#endif
        break;
    case 'y':                  /* syntax type */
#ifdef USE_INTERNAL_EDIT
        if (edit_widget != NULL)
        {
            const char *syntax_type;

            syntax_type = edit_get_syntax_type (edit_widget);
            if (syntax_type != NULL)
            {
                result = g_strdup (syntax_type);
                goto ret;
            }
        }
#endif
        break;
    case 'k':                  /* block file name */
    case 'b':                  /* block file name / strip extension */
#ifdef USE_INTERNAL_EDIT
        if (edit_widget != NULL)
        {
            char *file;

            file = mc_config_get_full_path (EDIT_BLOCK_FILE);
            result = quote_func (file, FALSE);
            g_free (file);
            goto ret;
        }
#endif
        if (c_lc == 'b')
        {
            result = strip_ext (quote_func (fname, FALSE));
            goto ret;
        }
        break;
    case 'n':                  /* strip extension in editor */
#ifdef USE_INTERNAL_EDIT
        if (edit_widget != NULL)
        {
            result = strip_ext (quote_func (fname, FALSE));
            goto ret;
        }
#endif
        break;
    case 'm':                  /* menu file name */
        if (menu != NULL)
        {
            result = quote_func (menu, FALSE);
            goto ret;
        }
        break;
    case 's':
        if (panel == NULL || panel->marked == 0)
        {
            result = quote_func (fname, FALSE);
            goto ret;
        }

        /* Fall through */

    case 't':
    case 'u':
        {
            GString *block;
            int i;

            if (panel == NULL)
            {
                result = g_strdup ("");
                goto ret;
            }

            block = g_string_sized_new (16);

            for (i = 0; i < panel->dir.len; i++)
                if (panel->dir.list[i].f.marked)
                {
                    char *tmp;

                    tmp = quote_func (panel->dir.list[i].fname, FALSE);
                    g_string_append (block, tmp);
                    g_string_append_c (block, ' ');
                    g_free (tmp);

                    if (c_lc == 'u')
                        do_file_mark (panel, i, 0);
                }
            result = g_string_free (block, FALSE);
            goto ret;
        }                       /* sub case block */
    default:
        break;
    }                           /* switch */

    result = g_strdup ("% ");
    result[1] = c;
  ret:
    return result;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * If edit_widget is NULL then we are called from the mc menu,
 * otherwise we are called from the mcedit menu.
 */

gboolean
user_menu_cmd (const WEdit * edit_widget, const char *menu_file, int selected_entry)
{
    char *p;
    char *data, **entries;
    int max_cols, menu_lines, menu_limit;
    int col, i;
    gboolean accept_entry = TRUE;
    int selected, old_patterns;
    gboolean res = FALSE;
    gboolean interactive = TRUE;

    if (!vfs_current_is_local ())
    {
        message (D_ERROR, MSG_ERROR, "%s", _("Cannot execute commands on non-local filesystems"));
        return FALSE;
    }
    if (menu_file != NULL)
        menu = g_strdup (menu_file);
    else
        menu = g_strdup (edit_widget != NULL ? EDIT_LOCAL_MENU : MC_LOCAL_MENU);
    if (!exist_file (menu) || !menu_file_own (menu))
    {
        if (menu_file != NULL)
        {
            message (D_ERROR, MSG_ERROR, _("Cannot open file %s\n%s"), menu,
                     unix_error_string (errno));
            MC_PTR_FREE (menu);
            return FALSE;
        }

        g_free (menu);
        if (edit_widget != NULL)
            menu = mc_config_get_full_path (EDIT_HOME_MENU);
        else
            menu = mc_config_get_full_path (MC_USERMENU_FILE);


        if (!exist_file (menu))
        {
            g_free (menu);
            menu =
                mc_build_filename (mc_config_get_home_dir (),
                                   edit_widget != NULL ? EDIT_GLOBAL_MENU : MC_GLOBAL_MENU,
                                   (char *) NULL);
            if (!exist_file (menu))
            {
                g_free (menu);
                menu =
                    mc_build_filename (mc_global.sysconfig_dir,
                                       edit_widget != NULL ? EDIT_GLOBAL_MENU : MC_GLOBAL_MENU,
                                       (char *) NULL);
                if (!exist_file (menu))
                {
                    g_free (menu);
                    menu =
                        mc_build_filename (mc_global.share_data_dir,
                                           edit_widget != NULL ? EDIT_GLOBAL_MENU : MC_GLOBAL_MENU,
                                           (char *) NULL);
                }
            }
        }
    }

    if (!g_file_get_contents (menu, &data, NULL, NULL))
    {
        message (D_ERROR, MSG_ERROR, _("Cannot open file %s\n%s"), menu, unix_error_string (errno));
        MC_PTR_FREE (menu);
        return FALSE;
    }

    max_cols = 0;
    selected = 0;
    menu_limit = 0;
    entries = NULL;

    /* Parse the menu file */
    old_patterns = easy_patterns;
    p = check_patterns (data);
    for (menu_lines = col = 0; *p != END_OF_STRING; str_next_char (&p))
    {
        if (menu_lines >= menu_limit)
        {
            char **new_entries;

            menu_limit += MAX_ENTRIES;
            new_entries = g_try_realloc (entries, sizeof (new_entries[0]) * menu_limit);

            if (new_entries == NULL)
                break;

            entries = new_entries;
            new_entries += menu_limit;
            while (--new_entries >= &entries[menu_lines])
                *new_entries = NULL;
        }
        if (col == 0 && entries[menu_lines] == NULL)
        {
            if (*p == '#')
            {
                /* show prompt if first line of external script is #interactive */
                if (selected_entry >= 0 && strncmp (p, "#silent", 7) == 0)
                    interactive = FALSE;
                /* A commented menu entry */
                accept_entry = TRUE;
            }
            else if (*p == '+')
            {
                if (*(p + 1) == '=')
                {
                    /* Combined adding and default */
                    p = test_line (edit_widget, p + 1, &accept_entry);
                    if (selected == 0 && accept_entry)
                        selected = menu_lines;
                }
                else
                {
                    /* A condition for adding the entry */
                    p = test_line (edit_widget, p, &accept_entry);
                }
            }
            else if (*p == '=')
            {
                if (*(p + 1) == '+')
                {
                    /* Combined adding and default */
                    p = test_line (edit_widget, p + 1, &accept_entry);
                    if (selected == 0 && accept_entry)
                        selected = menu_lines;
                }
                else
                {
                    /* A condition for making the entry default */
                    i = 1;
                    p = test_line (edit_widget, p, &i);
                    if (selected == 0 && i != 0)
                        selected = menu_lines;
                }
            }
            else if (*p != ' ' && *p != '\t' && str_isprint (p))
            {
                /* A menu entry title line */
                if (accept_entry)
                    entries[menu_lines] = p;
                else
                    accept_entry = TRUE;
            }
        }
        if (*p == '\n')
        {
            if (entries[menu_lines] != NULL)
            {
                menu_lines++;
                accept_entry = TRUE;
            }
            max_cols = MAX (max_cols, col);
            col = 0;
        }
        else
        {
            if (*p == '\t')
                *p = ' ';
            col++;
        }
    }

    if (menu_lines == 0)
    {
        message (D_ERROR, MSG_ERROR, _("No suitable entries found in %s"), menu);
        res = FALSE;
    }
    else
    {
        if (selected_entry >= 0)
            selected = selected_entry;
        else
        {
            Listbox *listbox;

            max_cols = MIN (MAX (max_cols, col), MAX_ENTRY_LEN);

            /* Create listbox */
            listbox = create_listbox_window (menu_lines, max_cols + 2, _("User menu"),
                                             "[Edit Menu File]");
            /* insert all the items found */
            for (i = 0; i < menu_lines; i++)
            {
                p = entries[i];
                LISTBOX_APPEND_TEXT (listbox, (unsigned char) p[0],
                                     extract_line (p, p + MAX_ENTRY_LEN), p, FALSE);
            }
            /* Select the default entry */
            listbox_select_entry (listbox->list, selected);

            selected = run_listbox (listbox);
        }
        if (selected >= 0)
        {
            execute_menu_command (edit_widget, entries[selected], interactive);
            res = TRUE;
        }

        do_refresh ();
    }

    easy_patterns = old_patterns;
    MC_PTR_FREE (menu);
    g_free (entries);
    g_free (data);
    return res;
}

/* --------------------------------------------------------------------------------------------- */
