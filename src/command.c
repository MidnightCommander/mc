/* Command line widget.
   Copyright (C) 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

   This widget is derived from the WInput widget, it's used to cope
   with all the magic of the command input line, we depend on some
   help from the program's callback.

*/

#include <config.h>

#include <errno.h>
#include <string.h>

#include "global.h"		/* home_dir */
#include "tty.h"
#include "widget.h"		/* WInput */
#include "command.h"
#include "wtools.h"		/* message () */
#include "panel.h"		/* view_tree enum. Also, needed by main.h */
#include "main.h"		/* do_cd */
#include "layout.h"		/* for command_prompt variable */
#include "user.h"		/* expand_format */
#include "subshell.h"		/* SUBSHELL_EXIT */
#include "tree.h"		/* for tree_chdir */
#include "color.h"		/* DEFAULT_COLOR */
#include "execute.h"		/* shell_execute */

/* This holds the command line */
WInput *cmdline;

/*
 * Expand the argument to "cd" and change directory.  First try tilde
 * expansion, then variable substitution.  If the CDPATH variable is set
 * (e.g. CDPATH=".:~:/usr"), try all the paths contained there.
 * We do not support such rare substitutions as ${var:-value} etc.
 * No quoting is implemented here, so ${VAR} and $VAR will be always
 * substituted.  Wildcards are not supported either.
 * Advanced users should be encouraged to use "\cd" instead of "cd" if
 * they want the behavior they are used to in the shell.
 */
static int
examine_cd (char *path)
{
    int result, qlen;
    char *path_tilde;
    char *p, *q, *r, *s, c;
    const char *t;

    /* Tilde expansion */
    path = shell_unescape(path);
    path_tilde = tilde_expand (path);

    /* Leave space for further expansion */
    qlen = strlen (path_tilde) + MC_MAXPATHLEN;
    q = g_malloc (qlen);

    /* Variable expansion */
    for (p = path_tilde, r = q; *p && r < q + MC_MAXPATHLEN;) {
	if (*p != '$' || (p[1] == '[' || p[1] == '('))
	    *(r++) = *(p++);
	else {
	    p++;
	    if (*p == '{') {
		p++;
		s = strchr (p, '}');
	    } else
		s = NULL;
	    if (s == NULL)
		s = strchr (p, PATH_SEP);
	    if (s == NULL)
		s = strchr (p, 0);
	    c = *s;
	    *s = 0;
	    t = getenv (p);
	    *s = c;
	    if (t == NULL) {
		*(r++) = '$';
		if (*(p - 1) != '$')
		    *(r++) = '{';
	    } else {
		if (r + strlen (t) < q + MC_MAXPATHLEN) {
		    strcpy (r, t);
		    r = strchr (r, 0);
		}
		if (*s == '}')
		    p = s + 1;
		else
		    p = s;
	    }
	}
    }
    *r = 0;

    result = do_cd (q, cd_parse_command);

    /* CDPATH handling */
    if (*q != PATH_SEP && !result) {
	char * const cdpath = g_strdup (getenv ("CDPATH"));
	char *p = cdpath;
	if (p == NULL)
	    c = 0;
	else
	    c = ':';
	while (!result && c == ':') {
	    s = strchr (p, ':');
	    if (s == NULL)
		s = strchr (p, 0);
	    c = *s;
	    *s = 0;
	    if (*p) {
		r = concat_dir_and_file (p, q);
		result = do_cd (r, cd_parse_command);
		g_free (r);
	    }
	    *s = c;
	    p = s + 1;
	}
	g_free (cdpath);
    }
    g_free (q);
    g_free (path_tilde);
    return result;
}

/* Execute the cd command on the command line */
void do_cd_command (char *cmd)
{
    int len;

    /* Any final whitespace should be removed here
       (to see why, try "cd fred "). */
    /* NOTE: I think we should not remove the extra space,
       that way, we can cd into hidden directories */
    /* FIXME: what about interpreting quoted strings like the shell.
       so one could type "cd <tab> M-a <enter>" and it would work. */
    len = strlen (cmd) - 1;
    while (len >= 0 &&
	   (cmd [len] == ' ' || cmd [len] == '\t' || cmd [len] == '\n')){
	cmd [len] = 0;
	len --;
    }
    
    if (cmd [2] == 0)
	cmd = "cd ";

    if (get_current_type () == view_tree){
	if (cmd [0] == 0){
	    sync_tree (home_dir);
	} else if (strcmp (cmd+3, "..") == 0){
	    char *dir = current_panel->cwd;
	    int len = strlen (dir);
	    while (len && dir [--len] != PATH_SEP);
	    dir [len] = 0;
	    if (len)
		sync_tree (dir);
	    else
		sync_tree (PATH_SEP_STR);
	} else if (cmd [3] == PATH_SEP){
	    sync_tree (cmd+3);
	} else {
	    char *old = current_panel->cwd;
	    char *new;
	    new = concat_dir_and_file (old, cmd+3);
	    sync_tree (new);
	    g_free (new);
	}
    } else
	if (!examine_cd (&cmd [3])) {
	    char *d = strip_password (g_strdup (&cmd [3]), 1);
	    message (D_ERROR, MSG_ERROR, _(" Cannot chdir to \"%s\" \n %s "),
		     d, unix_error_string (errno));
	    g_free (d);
	    return;
	}
}

/* Handle Enter on the command line */
static cb_ret_t
enter (WInput *cmdline)
{
    char *cmd = cmdline->buffer;

    if (!command_prompt)
	return MSG_HANDLED;

    /* Any initial whitespace should be removed at this point */
    while (*cmd == ' ' || *cmd == '\t' || *cmd == '\n')
	cmd++;

    if (!*cmd)
	return MSG_HANDLED;

    if (strncmp (cmd, "cd ", 3) == 0 || strcmp (cmd, "cd") == 0) {
	do_cd_command (cmd);
	new_input (cmdline);
	return MSG_HANDLED;
    } else {
	char *command, *s;
	size_t i, j, cmd_len;

	if (!vfs_current_is_local ()) {
	    message (D_ERROR, MSG_ERROR,
		     _
		     (" Cannot execute commands on non-local filesystems"));

	    return MSG_NOT_HANDLED;
	}
#ifdef HAVE_SUBSHELL_SUPPORT
	/* Check this early before we clean command line
	 * (will be checked again by shell_execute) */
	if (use_subshell && subshell_state != INACTIVE) {
	    message (D_ERROR, MSG_ERROR,
		     _(" The shell is already running a command "));
	    return MSG_NOT_HANDLED;
	}
#endif
	cmd_len = strlen (cmd);
	command = g_malloc (cmd_len + 1);
	command[0] = 0;
	for (i = j = 0; i < cmd_len; i++) {
	    if (cmd[i] == '%') {
		i++;
		s = expand_format (NULL, cmd[i], 1);
		command = g_realloc (command, j + strlen (s) + cmd_len - i + 1);
		strcpy (command + j, s);
		g_free (s);
		j = strlen (command);
	    } else {
		command[j] = cmd[i];
		j++;
	    }
	    command[j] = 0;
	}
	new_input (cmdline);
	shell_execute (command, 0);
	g_free (command);

#ifdef HAVE_SUBSHELL_SUPPORT
	if (quit & SUBSHELL_EXIT) {
	    quiet_quit_cmd ();
	    return MSG_HANDLED;
	}
	if (use_subshell)
	    load_prompt (0, 0);
#endif
    }
    return MSG_HANDLED;
}

static cb_ret_t
command_callback (Widget *w, widget_msg_t msg, int parm)
{
    WInput *cmd = (WInput *) w;

    switch (msg) {
    case WIDGET_FOCUS:
	/* Never accept focus, otherwise panels will be unselected */
	return MSG_NOT_HANDLED;

    case WIDGET_KEY:
	/* Special case: we handle the enter key */
	if (parm == '\n') {
	    return enter (cmd);
	}
	/* fall through */

    default:
	return input_callback (w, msg, parm);
    }
}

WInput *
command_new (int y, int x, int cols)
{
    WInput *cmd;

    cmd = input_new (y, x, DEFAULT_COLOR, cols, "", "cmdline",
	INPUT_COMPLETE_DEFAULT | INPUT_COMPLETE_CD | INPUT_COMPLETE_COMMANDS | INPUT_COMPLETE_SHELL_ESC);

    /* Add our hooks */
    cmd->widget.callback = command_callback;
    cmd->completion_flags |= INPUT_COMPLETE_COMMANDS;

    return cmd;
}

/*
 * Insert quoted text in input line.  The function is meant for the
 * command line, so the percent sign is quoted as well.
 */
void
command_insert (WInput * in, const char *text, int insert_extra_space)
{
    char *quoted_text;

    quoted_text = name_quote (text, 1);
    stuff (in, quoted_text, insert_extra_space);
    g_free (quoted_text);
}

