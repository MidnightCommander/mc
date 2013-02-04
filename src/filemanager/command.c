/*
   Command line widget.
   This widget is derived from the WInput widget, it's used to cope
   with all the magic of the command input line, we depend on some
   help from the program's callback.

   Copyright (C) 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007, 2011
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

/** \file command.c
 *  \brief Source: command line widget
 */

#include <config.h>

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "lib/global.h"         /* home_dir */
#include "lib/tty/tty.h"
#include "lib/vfs/vfs.h"
#include "lib/strescape.h"
#include "lib/skin.h"           /* DEFAULT_COLOR */
#include "lib/util.h"
#include "lib/widget.h"

#include "src/setup.h"          /* quit */
#ifdef ENABLE_SUBSHELL
#include "src/subshell.h"
#endif
#include "src/execute.h"        /* shell_execute */

#include "midnight.h"           /* current_panel */
#include "layout.h"             /* for command_prompt variable */
#include "usermenu.h"           /* expand_format */
#include "tree.h"               /* for tree_chdir */

#include "command.h"

/*** global variables ****************************************************************************/

/* This holds the command line */
WInput *cmdline;

/*** file scope macro definitions ****************************************************************/

#define CD_OPERAND_OFFSET 3

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Expand the argument to "cd" and change directory.  First try tilde
 * expansion, then variable substitution.  If the CDPATH variable is set
 * (e.g. CDPATH=".:~:/usr"), try all the paths contained there.
 * We do not support such rare substitutions as ${var:-value} etc.
 * No quoting is implemented here, so ${VAR} and $VAR will be always
 * substituted.  Wildcards are not supported either.
 * Advanced users should be encouraged to use "\cd" instead of "cd" if
 * they want the behavior they are used to in the shell.
 *
 * @param _path string to examine
 * @return newly allocated string
 */

static char *
examine_cd (const char *_path)
{
    typedef enum
    { copy_sym, subst_var } state_t;

    state_t state = copy_sym;
    char *q;
    size_t qlen;
    char *path_tilde, *path;
    char *p, *r;

    /* Tilde expansion */
    path = strutils_shell_unescape (_path);
    path_tilde = tilde_expand (path);
    g_free (path);

    /* Leave space for further expansion */
    qlen = strlen (path_tilde) + MC_MAXPATHLEN;
    q = g_malloc (qlen);

    /* Variable expansion */
    for (p = path_tilde, r = q; *p != '\0' && r < q + MC_MAXPATHLEN;)
    {

        switch (state)
        {
        case copy_sym:
            if (p[0] == '\\' && p[1] == '$')
            {
                /* skip backslash */
                p++;
                /* copy dollar */
                *(r++) = *(p++);
            }
            else if (p[0] != '$' || p[1] == '[' || p[1] == '(')
                *(r++) = *(p++);
            else
                state = subst_var;
            break;

        case subst_var:
            {
                char *s;
                char c;
                const char *t;

                /* skip dollar */
                p++;

                if (p[0] != '{')
                    s = NULL;
                else
                {
                    p++;
                    s = strchr (p, '}');
                }
                if (s == NULL)
                    s = strchr (p, PATH_SEP);
                if (s == NULL)
                    s = strchr (p, '\0');
                c = *s;
                *s = '\0';
                t = getenv (p);
                *s = c;
                if (t == NULL)
                {
                    *(r++) = '$';
                    if (p[-1] != '$')
                        *(r++) = '{';
                }
                else
                {
                    size_t tlen;

                    tlen = strlen (t);

                    if (r + tlen < q + MC_MAXPATHLEN)
                    {
                        strncpy (r, t, tlen + 1);
                        r += tlen;
                    }
                    p = s;
                    if (*s == '}')
                        p++;
                }

                state = copy_sym;
                break;
            }
        }
    }

    g_free (path_tilde);

    *r = '\0';

    return q;
}

/* --------------------------------------------------------------------------------------------- */

/* CDPATH handling */
static gboolean
handle_cdpath (const char *path)
{
    gboolean result = FALSE;

    /* CDPATH handling */
    if (*path != PATH_SEP)
    {
        char *cdpath, *p;
        char c;

        cdpath = g_strdup (getenv ("CDPATH"));
        p = cdpath;
        c = (p == NULL) ? '\0' : ':';

        while (!result && c == ':')
        {
            char *s;

            s = strchr (p, ':');
            if (s == NULL)
                s = strchr (p, '\0');
            c = *s;
            *s = '\0';
            if (*p != '\0')
            {
                vfs_path_t *r_vpath;

                r_vpath = vfs_path_build_filename (p, path, NULL);
                result = do_cd (r_vpath, cd_parse_command);
                vfs_path_free (r_vpath);
            }
            *s = c;
            p = s + 1;
        }
        g_free (cdpath);
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */

/** Handle Enter on the command line
 *
 * @param lc_cmdline string for handling
 * @return MSG_HANDLED on sucsess else MSG_NOT_HANDLED
 */

static cb_ret_t
enter (WInput * lc_cmdline)
{
    char *cmd = lc_cmdline->buffer;

    if (!command_prompt)
        return MSG_HANDLED;

    /* Any initial whitespace should be removed at this point */
    while (*cmd == ' ' || *cmd == '\t' || *cmd == '\n')
        cmd++;

    if (!*cmd)
        return MSG_HANDLED;

    if (strncmp (cmd, "cd ", 3) == 0 || strcmp (cmd, "cd") == 0)
    {
        do_cd_command (cmd);
        input_clean (lc_cmdline);
        return MSG_HANDLED;
    }
    else if (strcmp (cmd, "exit") == 0)
    {
        input_assign_text (lc_cmdline, "");
        if (!quiet_quit_cmd ())
            return MSG_NOT_HANDLED;
    }
    else
    {
        GString *command;
        size_t i;

        if (!vfs_current_is_local ())
        {
            message (D_ERROR, MSG_ERROR, _("Cannot execute commands on non-local filesystems"));
            return MSG_NOT_HANDLED;
        }
#ifdef ENABLE_SUBSHELL
        /* Check this early before we clean command line
         * (will be checked again by shell_execute) */
        if (mc_global.tty.use_subshell && subshell_state != INACTIVE)
        {
            message (D_ERROR, MSG_ERROR, _("The shell is already running a command"));
            return MSG_NOT_HANDLED;
        }
#endif
        command = g_string_sized_new (32);

        for (i = 0; cmd[i] != '\0'; i++)
        {
            if (cmd[i] != '%')
                g_string_append_c (command, cmd[i]);
            else
            {
                char *s;

                s = expand_format (NULL, cmd[++i], TRUE);
                g_string_append (command, s);
                g_free (s);
            }
        }

        input_clean (lc_cmdline);
        shell_execute (command->str, 0);
        g_string_free (command, TRUE);

#ifdef ENABLE_SUBSHELL
        if ((quit & SUBSHELL_EXIT) != 0)
        {
            if (quiet_quit_cmd ())
                return MSG_HANDLED;

            quit = 0;
            /* restart subshell */
            if (mc_global.tty.use_subshell)
                init_subshell ();
        }

        if (mc_global.tty.use_subshell)
            do_load_prompt ();
#endif
    }
    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Default command line callback
 *
 * @param w Widget object
 * @param msg message for handling
 * @param parm extra parameter such as key code
 *
 * @return MSG_NOT_HANDLED on fail else MSG_HANDLED
 */

static cb_ret_t
command_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_FOCUS:
        /* Never accept focus, otherwise panels will be unselected */
        return MSG_NOT_HANDLED;

    case MSG_KEY:
        /* Special case: we handle the enter key */
        if (parm == '\n')
            return enter (INPUT (w));
        /* fall through */

    default:
        return input_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** Execute the cd command on the command line
 *
 * @param orig_cmd command for execution
 */

void
do_cd_command (char *orig_cmd)
{
    int len;
    int operand_pos = CD_OPERAND_OFFSET;
    const char *cmd;

    /* Any final whitespace should be removed here
       (to see why, try "cd fred "). */
    /* NOTE: I think we should not remove the extra space,
       that way, we can cd into hidden directories */
    /* FIXME: what about interpreting quoted strings like the shell.
       so one could type "cd <tab> M-a <enter>" and it would work. */
    len = strlen (orig_cmd) - 1;
    while (len >= 0 && (orig_cmd[len] == ' ' || orig_cmd[len] == '\t' || orig_cmd[len] == '\n'))
    {
        orig_cmd[len] = 0;
        len--;
    }

    cmd = orig_cmd;
    if (cmd[CD_OPERAND_OFFSET - 1] == 0)
        cmd = "cd ";            /* 0..2 => given text, 3 => \0 */

    /* allow any amount of white space in front of the path operand */
    while (cmd[operand_pos] == ' ' || cmd[operand_pos] == '\t')
        operand_pos++;

    if (get_current_type () == view_tree)
    {
        if (cmd[0] == 0)
        {
            sync_tree (mc_config_get_home_dir ());
        }
        else if (strcmp (cmd + operand_pos, "..") == 0)
        {
            char *str_path;

            if (vfs_path_elements_count (current_panel->cwd_vpath) != 1 ||
                strlen (vfs_path_get_by_index (current_panel->cwd_vpath, 0)->path) > 1)
            {
                vfs_path_t *tmp_vpath = current_panel->cwd_vpath;

                current_panel->cwd_vpath =
                    vfs_path_vtokens_get (tmp_vpath, 0, vfs_path_tokens_count (tmp_vpath) - 1);
                vfs_path_free (tmp_vpath);
            }
            str_path = vfs_path_to_str (current_panel->cwd_vpath);
            sync_tree (str_path);
            g_free (str_path);
        }
        else if (cmd[operand_pos] == PATH_SEP)
        {
            sync_tree (cmd + operand_pos);
        }
        else
        {
            char *str_path;
            vfs_path_t *new_vpath;

            new_vpath = vfs_path_append_new (current_panel->cwd_vpath, cmd + operand_pos, NULL);
            str_path = vfs_path_to_str (new_vpath);
            vfs_path_free (new_vpath);
            sync_tree (str_path);
            g_free (str_path);
        }
    }
    else
    {
        char *path;
        vfs_path_t *q_vpath;
        gboolean ok;

        path = examine_cd (&cmd[operand_pos]);

        if (*path == '\0')
            q_vpath = vfs_path_from_str (mc_config_get_home_dir ());
        else
            q_vpath = vfs_path_from_str_flags (path, VPF_NO_CANON);

        ok = do_cd (q_vpath, cd_parse_command);
        if (!ok)
            ok = handle_cdpath (path);

        if (!ok)
        {
            char *d;

            d = vfs_path_to_str_flags (q_vpath, 0, VPF_STRIP_PASSWORD);
            message (D_ERROR, MSG_ERROR, _("Cannot chdir to \"%s\"\n%s"), d,
                     unix_error_string (errno));
            g_free (d);
        }

        vfs_path_free (q_vpath);
        g_free (path);
    }
}

/* --------------------------------------------------------------------------------------------- */

WInput *
command_new (int y, int x, int cols)
{
    WInput *cmd;
    const input_colors_t command_colors = {
        DEFAULT_COLOR,
        COMMAND_MARK_COLOR,
        DEFAULT_COLOR,
        COMMAND_HISTORY_COLOR
    };

    cmd = input_new (y, x, (int *) command_colors, cols, "", "cmdline",
                     INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_VARIABLES | INPUT_COMPLETE_USERNAMES
                     | INPUT_COMPLETE_HOSTNAMES | INPUT_COMPLETE_CD | INPUT_COMPLETE_COMMANDS |
                     INPUT_COMPLETE_SHELL_ESC);

    /* Add our hooks */
    WIDGET (cmd)->callback = command_callback;

    return cmd;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Insert quoted text in input line.  The function is meant for the
 * command line, so the percent sign is quoted as well.
 *
 * @param in WInput object
 * @param text string for insertion
 * @param insert_extra_space add extra space
 */

void
command_insert (WInput * in, const char *text, gboolean insert_extra_space)
{
    char *quoted_text;

    quoted_text = name_quote (text, 1);
    input_insert (in, quoted_text, insert_extra_space);
    g_free (quoted_text);
}

/* --------------------------------------------------------------------------------------------- */
