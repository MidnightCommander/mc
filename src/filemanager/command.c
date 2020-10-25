/*
   Command line widget.
   This widget is derived from the WInput widget, it's used to cope
   with all the magic of the command input line, we depend on some
   help from the program's callback.

   Copyright (C) 1995-2020
   Free Software Foundation, Inc.

   Written by:
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
#include "src/subshell/subshell.h"
#endif
#include "src/execute.h"        /* shell_execute */
#include "src/usermenu.h"       /* expand_format */

#include "filemanager.h"        /* current_panel */
#include "layout.h"             /* command_prompt */
#include "tree.h"               /* sync_tree() */

#include "command.h"

/*** global variables ****************************************************************************/

/* This holds the command line */
WInput *cmdline;

/*** file scope macro definitions ****************************************************************/

#define CD_OPERAND_OFFSET 3

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* Color styles command line */
static input_colors_t command_colors;

/* --------------------------------------------------------------------------------------------- */
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
    /* *INDENT-OFF* */
    typedef enum
    {
        copy_sym,
        subst_var
    } state_t;
    /* *INDENT-ON* */

    state_t state = copy_sym;
    GString *q;
    char *path_tilde, *path;
    char *p;

    /* Tilde expansion */
    path = strutils_shell_unescape (_path);
    path_tilde = tilde_expand (path);
    g_free (path);

    q = g_string_sized_new (32);

    /* Variable expansion */
    for (p = path_tilde; *p != '\0';)
    {
        switch (state)
        {
        case copy_sym:
            if (p[0] == '\\' && p[1] == '$')
            {
                g_string_append_c (q, '$');
                p += 2;
            }
            else if (p[0] != '$' || p[1] == '[' || p[1] == '(')
            {
                g_string_append_c (q, *p);
                p++;
            }
            else
                state = subst_var;
            break;

        case subst_var:
            {
                char *s = NULL;
                char c;
                const char *t = NULL;

                /* skip dollar */
                p++;

                if (p[0] == '{')
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
                    g_string_append_c (q, '$');
                    if (p[-1] != '$')
                        g_string_append_c (q, '{');
                }
                else
                {
                    g_string_append (q, t);
                    p = s;
                    if (*s == '}')
                        p++;
                }

                state = copy_sym;
                break;
            }

        default:
            break;
        }
    }

    g_free (path_tilde);

    return g_string_free (q, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* CDPATH handling */
static gboolean
handle_cdpath (const char *path)
{
    gboolean result = FALSE;

    /* CDPATH handling */
    if (!IS_PATH_SEP (*path))
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

                r_vpath = vfs_path_build_filename (p, path, (char *) NULL);
                result = do_cd (current_panel, r_vpath, cd_parse_command);
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
    while (whiteness (*cmd))
        cmd++;

    if (*cmd == '\0')
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
    case MSG_KEY:
        /* Special case: we handle the enter key */
        if (parm == '\n')
            return enter (INPUT (w));
        MC_FALLTHROUGH;

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
    while (len >= 0 && whiteness (orig_cmd[len]))
    {
        orig_cmd[len] = '\0';
        len--;
    }

    cmd = orig_cmd;
    if (cmd[CD_OPERAND_OFFSET - 1] == '\0')
        cmd = "cd ";            /* 0..2 => given text, 3 => \0 */

    /* allow any amount of white space in front of the path operand */
    while (whitespace (cmd[operand_pos]))
        operand_pos++;

    if (get_current_type () == view_tree)
    {
        vfs_path_t *new_vpath = NULL;

        if (cmd[0] == '\0')
        {
            new_vpath = vfs_path_from_str (mc_config_get_home_dir ());
            sync_tree (new_vpath);
        }
        else if (DIR_IS_DOTDOT (cmd + operand_pos))
        {
            if (vfs_path_elements_count (current_panel->cwd_vpath) != 1 ||
                strlen (vfs_path_get_by_index (current_panel->cwd_vpath, 0)->path) > 1)
            {
                vfs_path_t *tmp_vpath = current_panel->cwd_vpath;

                current_panel->cwd_vpath =
                    vfs_path_vtokens_get (tmp_vpath, 0, vfs_path_tokens_count (tmp_vpath) - 1);
                vfs_path_free (tmp_vpath);
            }
            sync_tree (current_panel->cwd_vpath);
        }
        else
        {
            if (IS_PATH_SEP (cmd[operand_pos]))
                new_vpath = vfs_path_from_str (cmd + operand_pos);
            else
                new_vpath =
                    vfs_path_append_new (current_panel->cwd_vpath, cmd + operand_pos,
                                         (char *) NULL);

            sync_tree (new_vpath);
        }

        vfs_path_free (new_vpath);
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

        ok = do_cd (current_panel, q_vpath, cd_parse_command);
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
    Widget *w;

    cmd = input_new (y, x, command_colors, cols, "", "cmdline",
                     INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_VARIABLES | INPUT_COMPLETE_USERNAMES
                     | INPUT_COMPLETE_HOSTNAMES | INPUT_COMPLETE_CD | INPUT_COMPLETE_COMMANDS |
                     INPUT_COMPLETE_SHELL_ESC);
    w = WIDGET (cmd);
    /* Don't set WOP_SELECTABLE up, otherwise panels will be unselected */
    widget_set_options (w, WOP_SELECTABLE, FALSE);
    /* Add our hooks */
    w->callback = command_callback;

    return cmd;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Set colors for the command line.
 */

void
command_set_default_colors (void)
{
    command_colors[WINPUTC_MAIN] = DEFAULT_COLOR;
    command_colors[WINPUTC_MARK] = COMMAND_MARK_COLOR;
    command_colors[WINPUTC_UNCHANGED] = DEFAULT_COLOR;
    command_colors[WINPUTC_HISTORY] = COMMAND_HISTORY_COLOR;
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

    quoted_text = name_quote (text, TRUE);
    input_insert (in, quoted_text, insert_extra_space);
    g_free (quoted_text);
}

/* --------------------------------------------------------------------------------------------- */
