/* Command line widget.
   Copyright (C) 1995 Miguel de Icaza

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

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
#include "complete.h"		/* completion constants */
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

/*Tries variable substitution, and if a variable CDPATH
of the form e.g. CDPATH=".:~:/usr" exists, we try then all the paths which
are mentioned there. Also, we do not support such extraordinary things as
${var:-value}, etc. Use the eval cd 'path' command instead.
Bugs: No quoting occurrs here, so ${VAR} and $VAR will be always
substituted. I think we can encourage users to use in such extreme
cases instead of >cd path< command a >eval cd 'path'< command, which works
as they might expect :) 
FIXME: Perhaps we should do wildcard matching as well? */
static int examine_cd (char *path)
{
    char *p;
    int result;
    char *q, *r, *s, *t, c;

    q = g_malloc (MC_MAXPATHLEN + 10);
    /* Variable expansion */
    for (p = path, r = q; *p && r < q + MC_MAXPATHLEN; ) {
        if (*p != '$' || (p [1] == '[' || p [1] == '('))
            *(r++)=*(p++);
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
        p = getenv ("CDPATH");
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
    }
    g_free (q);
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
	    char *dir = cpanel->cwd;
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
	    char *old = cpanel->cwd;
	    char *new;
	    new = concat_dir_and_file (old, cmd+3);
	    sync_tree (new);
	    g_free (new);
	}
    } else
	if (!examine_cd (&cmd [3])) {
	    message (1, MSG_ERROR, _(" Cannot chdir to \"%s\" \n %s "),
		     &cmd [3], unix_error_string (errno));
	    return;
	}
}

/* Returns 1 if the we could handle the enter, 0 if not */
static int
enter (WInput *cmdline)
{
    Dlg_head *old_dlg;

    if (command_prompt && strlen (cmdline->buffer)) {
	char *cmd;

	/* Any initial whitespace should be removed at this point */
	cmd = cmdline->buffer;
	while (*cmd == ' ' || *cmd == '\t' || *cmd == '\n')
	    cmd++;

	if (strncmp (cmd, "cd ", 3) == 0 || strcmp (cmd, "cd") == 0) {
	    do_cd_command (cmd);
	    new_input (cmdline);
	    return MSG_HANDLED;
	} else {
	    char *command, *s;
	    int i, j;

	    if (!vfs_current_is_local ()) {
		message (1, MSG_ERROR,
			 _
			 (" Cannot execute commands on non-local filesystems"));

		return MSG_NOT_HANDLED;
	    }
#ifdef HAVE_SUBSHELL_SUPPORT
	    /* Check this early before we clean command line
	     * (will be checked again by shell_execute) */
	    if (use_subshell && subshell_state != INACTIVE) {
		message (1, MSG_ERROR,
			 _(" The shell is already running a command "));
		return MSG_NOT_HANDLED;
	    }
#endif

	    command = g_malloc (strlen (cmd) + 1);
	    command[0] = 0;
	    for (i = j = 0; i < strlen (cmd); i++) {
		if (cmd[i] == '%') {
		    i++;
		    s = expand_format (NULL, cmd[i], 1);
		    command =
			g_realloc (command, strlen (command) + strlen (s)
				   + strlen (cmd) - i + 1);
		    strcat (command, s);
		    g_free (s);
		    j = strlen (command);
		} else {
		    command[j] = cmd[i];
		    j++;
		}
		command[j] = 0;
	    }
	    old_dlg = current_dlg;
	    current_dlg = 0;
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

	    current_dlg = old_dlg;
	}
    }
    return MSG_HANDLED;
}

static cb_ret_t
command_callback (WInput *cmd, widget_msg_t msg, int parm)
{
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
	return input_callback (cmd, msg, parm);
    }
}

WInput *
command_new (int y, int x, int cols)
{
    WInput *cmd;

    cmd = input_new (y, x, DEFAULT_COLOR, cols, "", "cmdline");

    /* Add our hooks */
    cmd->widget.callback = (callback_fn) command_callback;
    cmd->completion_flags |= INPUT_COMPLETE_COMMANDS;

    return cmd;
}

/*
 * Insert quoted text in input line.  The function is meant for the
 * command line, so the percent sign is quoted as well.
 */
void
command_insert (WInput * in, char *text, int insert_extra_space)
{
    char *quoted_text;

    quoted_text = name_quote (text, 1);
    stuff (in, quoted_text, insert_extra_space);
    g_free (quoted_text);
}

